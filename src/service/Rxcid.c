/*
 * Rxcid.c -- Caller ID, FSK: the receiver object and the async framer over
 * the demodulated bit stream.
 *
 * Reconstructed from dsplibs.o Rxcid.c, and written in the object's own
 * emission order:
 *
 *   create_cid      .text 0x091ac0   183
 *   pack_next_bit   .text 0x091b80   301
 *   cid_modem       .text 0x091cb0  1487
 *
 * `reset_cid` and its file-static V23_MRF_FILT table live in Dtmf_Rx.c:
 * the object relocates create_cid's call to reset_cid, and reset_cid follows
 * dtmf_modem in the object's emission order (see Dtmf_Rx.c).
 *
 * HOW THE FOUR FIT TOGETHER.  `create_cid` allocates and seeds the three
 * caller-visible settings; `reset_cid` clears the rest and configures the 9:10
 * resampler.  `cid_modem` is the driver: it DC-blocks a block of line samples,
 * asks CID_MTD_detect whether the 1200 Hz mark tone is there, and only once it
 * has seen enough of it does it resample, apply an adaptive gain, demodulate
 * and frame.  `pack_next_bit` is the framer, called once per demodulated bit
 * and inlined into cid_modem by the original's own compiler.
 *
 * THE TWO THRESHOLDS ARE TIME, NOT LEVEL.  cid_modem derives
 * m = (rate == 9600 ? 49152 : 40960) / count and compares `mark_conf` against
 * m*18/256 and m*12/256.  Since a detected block adds `mark_conf_step` (9)
 * to `mark_conf`, those work out at 40 ms and 26.7 ms of tone at both
 * rates, independent of the block length.  Finding F8712.
 */

#include "dsplib/cid.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/sysdep.h"

/*
 * Construct one.  A NULL argument allocates; anything else is the caller's
 * storage and is used in place.  The object is returned either way, so
 * `cid_create` can write the result back over the pointer it passed.
 *
 * `sysdep_malloc`'s result is used without being checked -- the house style of
 * this object, and D303 for the DTMF half of the same pair.
 *
 * NULLING `mrf.history` FIRST IS LOAD-BEARING: it is what tells the reset
 * below to allocate the resampler buffer.  The three settings are written
 * AFTER the reset, because the reset does not touch them and the trace that
 * follows reads `rate` back out of the object.
 */
struct cid *
create_cid(struct cid *cid)
{
	if (cid == NULL)
		cid = sysdep_malloc(sizeof(*cid));

	cid->mrf.history = NULL;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("FSK CID Creating \n");

	reset_cid(cid);

	cid->rate = CID_RATE_8000;
	cid->threshold = 2;
	cid->mark_conf_step = 9;

	/*
	 * The object's own words, and what NAMED `threshold`: this line calls
	 * it the "Threshold", which is evidence rule 1 and is why the field is
	 * not an `fNNNN` any more.  Finding F8710.
	 *
	 * `rate` is re-loaded with `movzwl`, so it is read unsigned here as it
	 * is everywhere else.  The second `%d` is a materialised constant 2 in
	 * the object, so which of `cid->threshold`, a local or a literal the
	 * author wrote is not recoverable -- written the plain way, exactly as
	 * create_cid_dtmf's 0 is.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("FSK CID  Setting  Fs = %d   " "Threshold = %d !\n",
				     (unsigned short)cid->rate, 2);

	return cid;
}

/*
 * Feed the framer one demodulated bit.
 *
 * CID_FSD_demodulate emits one short per bit; cid_modem feeds them here one at
 * a time, and bytes accumulate in cid->data.  Four states:
 *
 *   0  hunting carrier.  A mark (bit == 1 EXACTLY -- any other value
 *      counts as space, including values above 1) steps mark_bal up, a
 *      space steps it down; sixteen net marks arm state 3 and clear the
 *      accumulator.
 *   3  channel seizure.  A run of threshold + 1 consecutive spaces enters the
 *      byte collector -- but the object BACKDATES pack_pos to threshold rather
 *      than clearing it, so the first byte's bits land at positions
 *      threshold..7 and the low positions keep whatever state 0 left in the
 *      accumulator (state 0 cleared it, so zeros).  Kept exactly; see the
 *      deviations note D954.
 *   1  between bytes: wait for a start bit (space), then clear and collect.
 *   2  collect: shift the bit in LSB-first at pack_pos; at position 8 store
 *      the byte and return to state 1.  There is NO stop-bit check and NO
 *      bound on pack_len -- both the object's own shape.
 *
 * The widths matter and are kept: pack_acc accumulates as a short while the
 * bit is shifted as an int, so a bit value other than 0/1 smears into high
 * positions exactly as it does in the object.  cid_modem's own call site masks
 * with 1 before calling, so states 0 and 3 cannot disagree there -- but this
 * function is exported and does not.
 */
void
pack_next_bit(short bit, struct cid *cid)
{
	unsigned short limit = 0;

	if (cid->threshold)
		limit = (unsigned short)cid->threshold;

	if (cid->pack_state == 0) {
		if (bit == 1)
			cid->mark_bal = (short)(cid->mark_bal + 1);
		else
			cid->mark_bal = (short)(cid->mark_bal - 1);
		if (cid->mark_bal > 15) {
			cid->pack_state = 3;
			cid->pack_acc = 0;
			cid->pack_pos = 0;
		}
	} else if (cid->pack_state == 3) {
		if (bit != 0)
			cid->pack_pos = 0;
		else
			cid->pack_pos = (short)(cid->pack_pos + 1);
		if (cid->pack_pos == (int)limit + 1) {
			cid->pack_pos = (short)limit;
			cid->pack_state = 2;
		}
	} else if (cid->pack_state == 1) {
		if (bit == 0) {
			cid->pack_state = 2;
			cid->pack_acc = 0;
			cid->pack_pos = 0;
		}
	} else {
		short pos = cid->pack_pos;
		unsigned short acc = (unsigned short)cid->pack_acc;

		acc = (unsigned short)(acc | ((int)bit << pos));
		cid->pack_pos = (short)(pos + 1);
		cid->pack_acc = (short)acc;
		if ((short)(pos + 1) == 8) {
			short idx = cid->pack_len;

			cid->pack_state = 1;
			cid->pack_len = (short)(idx + 1);
			cid->data[idx] = (unsigned char)acc;
		}
	}
}

/*
 * One block of line samples through the FSK receiver.
 *
 * Four stages, and each is gated on how much mark tone has been seen, so a
 * quiet line costs stage 1 and the detector and nothing else:
 *
 *   1. copy to a local buffer, track the block mean into `dc` at 0.8 of this
 *      block plus 0.2 of the last, and subtract it.
 *   2. below the 40 ms threshold, run CID_MTD_detect.  IT ANSWERS BACKWARDS:
 *      0 means the 1200 Hz tone IS there and adds `mark_conf_step` to
 *      `mark_conf`; anything else clears it.
 *   3. above the 26.7 ms threshold, resample 8000 -> 7200 in place (9600 is
 *      already the demodulator's rate) and apply `gain`.  While `mark_conf`
 *      is still BETWEEN the two thresholds, `gain` is first re-adapted from
 *      the block's mean absolute value through FPM_div_32, halved against its
 *      previous value, and floored at 1.
 *   4. above the 40 ms threshold, demodulate into `cid->bits` and push each
 *      bit -- masked to its low bit -- through pack_next_bit.  Once
 *      `data[1] + 3` bytes have been framed, checksum them.
 *
 * THE CHECKSUM IS THE MESSAGE'S OWN: data[1] is the body length, the sum of
 * every byte up to and including the body must be zero modulo 256, and the
 * byte after it is what makes it so.  Agreement returns 3, disagreement -1.
 *
 * TWO BUFFERS ARE UNBOUNDED, both the object's own shape (deviation D973):
 * `buf` is 206 shorts and the copy loop is bounded only by `count`, and
 * CID_FSD_demodulate writes into `cid->bits` -- 36 shorts before `data`
 * starts -- with no limit of its own.
 */
int
cid_modem(const short *samples, unsigned short count, struct cid *cid)
{
	/*
	 * INITIALISED AND NEVER READ, exactly as band_pass's pair is (D253).
	 * The object builds both of Dtmf_Rx.c's high-pass coefficient sets on
	 * this frame and uses neither; a modern compiler deletes them again,
	 * which changes nothing observable.
	 */
	short hp_8000[5] = { -12971, 12917, 28620, -25834, 12917 };
	short hp_9600[5] = { -13484, 13194, 29347, -26388, 13194 };
	short buf[206];
	unsigned short recip, shift;
	short ncount = (short)count;
	short nsamp = 0;		/* samples in `buf` after resampling */
	short nbits;
	short msglen;
	short result;
	short i;
	int sum;
	int m;
	int gain;

	(void)hp_8000;
	(void)hp_9600;

	for (i = 0; i < (int)count; i++)
		buf[i] = samples[i];

	/*
	 * 3278/4096 is 0.8003 and 3278/16384 is 0.2001, so `dc` tracks this
	 * block's mean four parts to one -- a fast tracker, not a leak.
	 * A zero `count` divides by zero here, in the object as here.
	 */
	sum = 0;
	for (i = 0; i < ncount; i++)
		sum += buf[i];
	cid->dc = (short)((sum / ncount * 3278 >> 12) + (cid->dc * 3278 >> 14));

	/*
	 * Descending, and the entry guard is `ncount != 0` rather than
	 * `ncount >= 1`, which is what `while (i--)` compiles to and what
	 * `for (i = ncount - 1; i >= 0; i--)` does not.
	 */
	i = ncount;
	while (i--)
		buf[i] = (short)((unsigned short)buf[i] - cid->dc);

	/*
	 * The confidence scale.  m is the number of blocks in 5.12 s, so
	 * m*18/256 and m*12/256 are 40 ms and 26.7 ms once `mark_conf_step`'s
	 * step of 9 per block is divided out -- at both rates and every block
	 * length.
	 */
	m = (cid->rate == CID_RATE_9600 ? 49152 : 40960) / (int)count;
	if (m <= 0)
		m = 1;

	if (cid->mark_conf < (m * 18 >> 8)) {
		if (CID_MTD_detect(buf, ncount, cid) != 0) {
			cid->mark_conf = 0;
		} else {
			cid->mark_conf = (short)((unsigned short)cid->mark_conf
						 + (unsigned short)cid->mark_conf_step);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Tone 1200 Detected\n");
		}
	}

	if (cid->mark_conf > (m * 12 >> 8)) {
		nsamp = ncount;
		if (cid->rate != CID_RATE_9600)
			nsamp = FPM_MRF_filter(&cid->mrf, buf, buf, count);

		/*
		 * Re-tested, and re-read from the object, because the two
		 * calls above could have moved it.  The gain adapts only while
		 * the tone is still being confirmed; once it is, the gain that
		 * was found is held for the rest of the message.
		 */
		if (cid->mark_conf <= (m * 18 >> 8) &&
		    cid->mark_conf > (m * 12 >> 8)) {
			sum = 0;
			for (i = 0; i < nsamp; i++)
				sum += buf[i] < 0 ? -(int)buf[i] : (int)buf[i];
			if (sum == 0)
				sum = 1;
			FPM_div_32((unsigned int)sum, &recip, &shift);
			gain = (int)(((unsigned int)(recip * 25))
				     >> (31 - shift)) + cid->gain;
			gain >>= 1;
			if (gain == 0)
				gain = 1;
			cid->gain = gain;
		}

		for (i = 0; i < nsamp; i++)
			buf[i] = (short)(buf[i] * cid->gain);
	}

	if (cid->mark_conf < (m * 18 >> 8))
		return 1;

	cid->mark_conf = (short)(cid->mark_conf + 1);

	/*
	 * How long the message is.  115 until two bytes have arrived, then the
	 * body length the message declares plus its three overhead bytes --
	 * clamped at 115 above, and a declared length of zero (msglen 3) is
	 * rejected outright rather than waited for.
	 */
	msglen = 115;
	if (cid->pack_len > 2) {
		msglen = (short)(cid->data[1] + 3);
		if (msglen > 115)
			msglen = 115;
		else if (msglen <= 3)
			return -1;
	}

	if (cid->pack_len < msglen) {
		nbits = CID_FSD_demodulate(buf, cid->bits, nsamp, cid);
		if (nbits <= 0)
			return 2;
		for (i = 0; i < nbits; i++)
			pack_next_bit((short)(cid->bits[i] & 1), cid);
		return 2;
	}

	{
		unsigned char csum = 0;
		int body = cid->data[1] + 2;

		for (i = 0; i < body; i++)
			csum = (unsigned char)(csum + cid->data[i]);
		result = (short)(cid->data[body] == (unsigned char)-csum
				 ? 3 : -1);
	}
	return result;
}
