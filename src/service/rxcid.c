/*
 * rxcid.c -- Caller ID, FSK: the receiver object and the async framer over
 * the demodulated bit stream.
 *
 * Reconstructed from dsplibs.o Rxcid.c (finding F1410 pins the TU as
 * reset_cid, create_cid, pack_next_bit, cid_modem), and written in the
 * object's own emission order:
 *
 *   reset_cid       .text 0x0918b0   517
 *   create_cid      .text 0x091ac0   183
 *   pack_next_bit   .text 0x091b80   301
 *   cid_modem       .text 0x091cb0  1487
 *
 * plus the file-static V23_MRF_FILT (.rodata 0x009240, 180 bytes), which is
 * the resampler prototype reset_cid installs.
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
 * m*18/256 and m*12/256.  Since a detected block adds f02c (9) to mark_conf,
 * those work out at 40 ms and 26.7 ms of tone at both rates, independent of
 * the block length.  Finding F8712.
 */

#include "dsplib/cid.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/sysdep.h"

/*
 * The 9:10 interpolating prototype that takes the 8000 Hz line to the 7200 Hz
 * CID_FSD_demodulate wants -- ninety taps read as nine polyphase phases of ten
 * by FPM_MRF_filter's stride, so the array itself is a plain linear-phase FIR
 * and is symmetric about its centre.
 *
 * FILE-STATIC IN THE OBJECT (`r V23_MRF_FILT`, .rodata 0x9240) and static
 * here, so a test cannot name it; t_rxcid compares it through the `coeff`
 * pointer reset_cid leaves in the object, which reaches the blob's copy on one
 * side and this one on the other.
 *
 * NOT the same table as the GLOBAL `_V23_MRF_FILT` (48 shorts, .rodata
 * 0x81c0), which is src/pump/v23/v23filt.c's -- see the name-collision note in
 * include/dsplib/v23fp.h.
 */
static const short V23_MRF_FILT[90] = {
	  -633,   -811,   -938,  -1005,  -1002,   -927,
	  -781,   -572,   -309,    -10,    308,    622,
	   911,   1152,   1325,   1414,   1406,   1296,
	  1083,    774,    383,    -69,   -556,  -1047,
	 -1510,  -1908,  -2208,  -2379,  -2394,  -2231,
	 -1879,  -1332,   -597,    311,   1368,   2542,
	  3792,   5074,   6340,   7540,   8627,   9556,
	 10289,  10795,  11054,  11054,  10795,  10289,
	  9556,   8627,   7540,   6340,   5074,   3792,
	  2542,   1368,    311,   -597,  -1332,  -1879,
	 -2231,  -2394,  -2379,  -2208,  -1908,  -1510,
	 -1047,   -556,    -69,    383,    774,   1083,
	  1296,   1406,   1414,   1325,   1152,    911,
	   622,    308,    -10,   -309,   -572,   -781,
	  -927,  -1002,  -1005,   -938,   -811,   -633
};

/*
 * Put the receiver back to the state a new one is in.
 *
 * Everything the DSP accumulates is cleared and the resampler is
 * (re-)configured; the three settings `create_cid` and the CID service write
 * -- `rate`, `f028` and `f02c` -- survive, which is what makes this usable as
 * `cid_reset`'s reset as well as part of construction.
 *
 * THE `fresh` ARGUMENT IS THE HISTORY POINTER ITSELF.  FPM_MRF_init allocates
 * unconditionally when `fresh` is set and leaks whatever was there, so the
 * object asks for that only when `mrf.history` is still NULL -- which is the
 * state `create_cid` leaves it in and nothing else ever restores.  Reading
 * +0x24 as a flag of `struct cid` rather than as `mrf.history` is the trap
 * here, and the two are the same four bytes.
 */
void
reset_cid(struct cid *cid)
{
	struct fpm_mrf_cfg cfg;
	short *p;
	short i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("FSK CID Reset !\n");

	cid->short_004 = 0;
	cid->short_008 = 0;
	cid->dc = 0;
	cid->gain = 0;

	/*
	 * The library default patched in four places.  `branches` and
	 * `decimate` are re-set to the values FPM_MRF_CFG already carries and
	 * `coeff` is the only field of the copy the object does not emit,
	 * because the store below kills it -- both of which fall out of a
	 * struct assignment followed by field stores, and neither of which
	 * falls out of designated initialisers.
	 */
	cfg = FPM_MRF_CFG;
	cfg.branches = 9;
	cfg.decimate = 10;
	cfg.coeff = V23_MRF_FILT;
	cfg.taps = 90;
	FPM_MRF_init(&cid->mrf, &cfg, cid->mrf.history == NULL);

	cid->ac_idx = 0;
	cid->last_bit = 0;
	cid->lpf_idx = 0;

	/*
	 * FIFTY shorts from +0x030, which is `lpf_hist` and everything after
	 * it as far as +0x092 -- two shorts INTO the bit buffer at +0x090.
	 * Over-broad, and the two loops after it then clear parts of the same
	 * span again.  Reproduced as found; deviation D974.  Written through a
	 * pointer because no field of `struct cid` spans it, and off
	 * `lpf_hist` rather than a numeric offset so it stays ABI-independent.
	 */
	p = cid->lpf_hist;
	for (i = 0; i <= 49; i++)
		p[i] = 0;

	for (i = 0; i <= 4; i++)
		cid->ac_hist[i] = 0;

	for (i = 0; i <= 16; i++)
		cid->lpf_hist[i] = 0;

	cid->high_count = 0;

	/*
	 * The rest of the state, ascending by offset.  The object's emission
	 * order interleaves these -- 0x80, 0x84, 0x82, 0x88 ... and 0x76
	 * before 0x74 -- which is the i686 scheduler pairing independent
	 * stores, so the source order is NOT recovered here the way
	 * reset_dtmf's was.  Nothing was enumerated; see finding F8711.
	 */
	cid->low_count = 0;
	cid->thresh = 0;
	cid->high_level = 0;
	cid->low_sum = 0;
	cid->high_sum = 0;
	cid->short_074 = 0;
	cid->short_076 = 0;
	cid->run = 0;
	cid->opp = 0;
	cid->dead = 0;
	cid->mtd1_state[0] = 0;
	cid->mtd1_state[1] = 0;
	cid->mtd2_state[0] = 0;
	cid->mtd2_state[1] = 0;
	cid->short_086 = 0;
	cid->short_088 = 0;
	cid->short_08a = 0;
	cid->short_08c = 0;
	cid->mark_conf = 0;
	cid->short_150 = 0;
	cid->short_152 = 0;
	cid->mark_bal = 0;
	cid->pack_state = 0;
	cid->pack_acc = 0;
	cid->pack_pos = 0;
	cid->pack_len = 0;

	/* 120 bytes, which is what pins `data`'s length. */
	for (i = 0; i <= 119; i++)
		cid->data[i] = 0;
}

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
		cid = (struct cid *)sysdep_malloc(sizeof(*cid));

	cid->mrf.history = NULL;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("FSK CID Creating \n");

	reset_cid(cid);

	cid->rate = CID_RATE_8000;
	cid->f028 = 2;
	cid->f02c = 9;

	/*
	 * The object's own words, and what settles `f028`: it is the
	 * "Threshold".  The name is not changed here because src/service/cid.c
	 * and t_cidsvc name the field; finding F8710 records the evidence.
	 *
	 * `rate` is re-loaded with `movzwl`, so it is read unsigned here as it
	 * is everywhere else.  The second `%d` is a materialised constant 2 in
	 * the object, so which of `cid->f028`, a local or a literal the author
	 * wrote is not recoverable -- written the plain way, exactly as
	 * create_cid_dtmf's 0 is.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("FSK CID  Setting  Fs = %d   "
				     "Threshold = %d !\n",
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
 *   3  channel seizure.  A run of f028 + 1 consecutive spaces enters the
 *      byte collector -- but the object BACKDATES pack_pos to f028 rather
 *      than clearing it, so the first byte's bits land at positions
 *      f028..7 and the low positions keep whatever state 0 left in the
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

	if (cid->f028)
		limit = (unsigned short)cid->f028;

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
 *      0 means the 1200 Hz tone IS there and adds `f02c` to `mark_conf`;
 *      anything else clears it.
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
	 * m*18/256 and m*12/256 are 40 ms and 26.7 ms once `f02c`'s step of 9
	 * per block is divided out -- at both rates and every block length.
	 */
	m = (cid->rate == CID_RATE_9600 ? 49152 : 40960) / (int)count;
	if (m <= 0)
		m = 1;

	if (cid->mark_conf < (m * 18 >> 8)) {
		if (CID_MTD_detect(buf, ncount, cid) != 0) {
			cid->mark_conf = 0;
		} else {
			cid->mark_conf = (short)((unsigned short)cid->mark_conf
						 + (unsigned short)cid->f02c);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Tone 1200 Detected\n");
		}
	}

	if (cid->mark_conf > (m * 12 >> 8)) {
		nsamp = ncount;
		if (cid->rate != CID_RATE_9600)
			nsamp = FPM_MRF_filter(&cid->mrf, buf, buf, ncount);

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
