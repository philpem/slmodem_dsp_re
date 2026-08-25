/*
 * cid_fsd.c -- reconstructed from dsplibs.o `Cidfsd.c`.
 *
 *   CID_FSD_demodulate  .text 0x092280  1049 bytes
 *
 * The whole translation unit: one function and the three coefficient tables
 * it reads.  All three are GLOBAL in the object, and finding F6 records that
 * the AUTOCOR_COEF_* pair is referenced from here and nowhere else -- so the
 * 7200/9600 choice is this function's own, not a shared configuration.
 *
 * THE DEMODULATOR, in four stages.
 *
 * 1. A DELAY-LINE DISCRIMINATOR.  Five samples of history in a circular
 *    buffer, correlated against AUTOCOR_COEF_7200 or _9600, and the result
 *    multiplied by the newest sample.  Both tables are one non-zero tap, so
 *    the correlation is just a delayed copy of the input and the multiply is
 *    the classic x[n] * x[n-D] frequency discriminator:
 *
 *        AUTOCOR_COEF_7200   { 0, 32767, 0, 0, 0 }   D = 1, gain +1.0
 *        AUTOCOR_COEF_9600   { 0, 0, 0, 0, -29491 }  D = 4, gain -0.9
 *
 *    The five-tap machinery is general and the tables make it a delay; the
 *    two spellings agree on sign, which is what matters.  At 7200 Hz a
 *    1200 Hz mark turns one sample in 1/6 of a cycle, so cos(60 deg) = +0.5,
 *    and a 2200 Hz space gives cos(110 deg) = -0.34.  At 9600 Hz over four
 *    samples the mark turns half a cycle, cos(180 deg) = -1, and the -0.9
 *    tap puts it back positive at +0.9 against the space's -0.78.  Either
 *    way MARK COMES OUT POSITIVE.
 *
 * 2. A 17-TAP SYMMETRIC LOW PASS, `fix_LPF`, over a second circular buffer:
 *    the discriminator's output is at twice the carrier plus the baseband
 *    difference, and this keeps the difference.
 *
 * 3. AN ADAPTIVE SLICING LEVEL.  The mean of the first 128 filtered values
 *    above the current threshold becomes `high_level`; the running mean of
 *    the negative ones becomes the other end; the threshold is the midpoint.
 *    It is seeded after 16 negatives and re-derived every 128 after that.
 *
 * 4. A SLICER WITH A DEAD ZONE AND A TIMER.  +-22 either side of
 *    threshold/8 is "no decision"; three bit times of that resets the timing.
 *    Outside it, a decision that AGREES with the last one is emitted once it
 *    has held for a whole bit, and one that DISAGREES is emitted after half a
 *    bit -- which is what re-times the receiver on every transition.
 *
 * SIX SAMPLES PER BIT, OR EIGHT.  `cid->rate` says 8000 or 9600, but the bit
 * length here is 6 or 8 and the baud rate is 1200 either way, so the "8000"
 * path is really 7200 -- exactly what its coefficient table is called.  The
 * caller resamples; this function never sees 8000 Hz.  Finding F1509.
 *
 * The three tables live in `.data`, not `.rodata`, so they were not `const`
 * in the original; `const` here is intent, as it is in dtmf_mtd_coeffs.c.
 */

#include "dsplib/cid.h"

/*
 * Declared in the object's own .data order, 0x7820 upwards.
 *
 * fix_LPF is symmetric about its centre tap and sums to 32768 -- unity gain
 * in Q15, which is why the correlation below is a plain >> 15 with no other
 * scaling.
 */
const short fix_LPF[17] = {
	-883, -1023, -713, 159, 1547, 3226, 4842, 6010, 6436,
	6010, 4842, 3226, 1547, 159, -713, -1023, -883
};

const short AUTOCOR_COEF_9600[5] = { 0, 0, 0, 0, -29491 };
const short AUTOCOR_COEF_7200[5] = { 0, 32767, 0, 0, 0 };

/* Half a bit either side of the slicing level is no decision at all. */
#define CID_FSD_DEADZONE	22

short
CID_FSD_demodulate(const short *samples, short *bits, short count,
		   struct cid *cid)
{
	unsigned short nbits = 0;
	int thresh = cid->thresh;

	/*
	 * `count` is a short and the test is against the value BEFORE the
	 * decrement, so a negative count runs it 65535 times over.  D304.
	 */
	while (count-- != 0) {
		const short *coef;
		int acc;
		int j, k;
		int d, baud, half;
		short rate, idx, lidx;
		short corr, y, last, bit;

		rate = cid->rate;

		/* ---- 1. the discriminator ------------------------------ */

		if (++cid->ac_idx > 4)
			cid->ac_idx = 0;
		idx = cid->ac_idx;
		cid->ac_hist[idx] = *samples++;

		coef = rate == CID_RATE_9600 ? AUTOCOR_COEF_9600
					     : AUTOCOR_COEF_7200;

		/*
		 * Circular correlation, newest sample against coef[0]: down
		 * from the write index to the bottom of the buffer, then
		 * round from the top back to it.
		 */
		acc = 0;
		k = 0;
		for (j = idx; j >= 0; j--)
			acc += cid->ac_hist[j] * coef[k++];
		for (j = 4; j > idx; j--)
			acc += cid->ac_hist[j] * coef[k++];
		corr = (short)(acc >> 15);

		/*
		 * The newest sample again, times the correlation.  The wrap
		 * below cannot fire -- `ac_idx` is 0..4 by construction --
		 * and is reproduced because the object has it (finding F1508).
		 */
		j = idx;
		if (j < 0)
			j += 5;

		/* ---- 2. the low pass ----------------------------------- */

		if (++cid->lpf_idx > 16)
			cid->lpf_idx = 0;
		lidx = cid->lpf_idx;
		cid->lpf_hist[lidx] = (short)((cid->ac_hist[j] * corr) >> 15);

		acc = 0;
		k = 0;
		for (j = lidx; j >= 0; j--)
			acc += cid->lpf_hist[j] * fix_LPF[k++];
		for (j = 16; j > lidx; j--)
			acc += cid->lpf_hist[j] * fix_LPF[k++];
		y = (short)(acc >> 15);

		/* ---- 3. the slicing level ------------------------------ */

		/*
		 * The high end: the mean of the first 128 samples above the
		 * threshold, computed once.  `high_count` keeps counting
		 * afterwards and is a short, so it eventually wraps negative
		 * and this arms itself again -- D305.
		 */
		if (thresh < y) {
			if (cid->high_count++ <= 127) {
				cid->high_sum += y;
				if (cid->high_count == 128)
					cid->high_level =
					    (cid->high_sum + 64) >> 7;
			}
		}

		/*
		 * The low end, and it does not start until the high end has
		 * produced a number.  Seeded from the first 16 negatives,
		 * then re-derived from the running total every 128 -- the
		 * count is tested a BYTE at a time, so 0x80 and 0x00 are the
		 * two points in each 256 where it happens.
		 */
		if (cid->high_level != 0 && y < 0) {
			int sum = cid->low_sum + y;

			cid->low_count++;
			if (cid->low_count == 16) {
				thresh = (cid->high_level + (sum >> 4)) >> 1;
				cid->thresh = thresh;
			}
			if ((unsigned char)cid->low_count == 0x80 ||
			    (unsigned char)cid->low_count == 0) {
				cid->low_sum = sum >> 7;
				thresh = (cid->high_level + cid->low_sum) >> 1;
				cid->thresh = thresh;
			} else {
				cid->low_sum = sum;
			}
		}

		/* ---- 4. the slicer ------------------------------------- */

		baud = rate == CID_RATE_9600 ? 8 : 6;
		half = baud >> 1;
		last = cid->last_bit;
		d = y - (thresh >> 3);

		if (d >= -CID_FSD_DEADZONE && d <= CID_FSD_DEADZONE) {
			/*
			 * No decision.  Three bit times of it and the timing
			 * is given up: -baud in both counters costs the next
			 * two bit times as well, which is a squelch.
			 */
			if (++cid->dead > 3 * baud) {
				cid->opp = (short)-baud;
				cid->run = (short)-baud;
			}
			continue;
		}

		if (d > CID_FSD_DEADZONE)
			bit = 1;
		else if (d > -(CID_FSD_DEADZONE + 1))
			bit = last;	/* dead: the test above caught it */
		else
			bit = 0;
		bit &= 1;

		cid->run++;
		if (last == bit) {
			/* A full bit of agreement: emit and re-arm. */
			if (cid->run == baud) {
				bits[nbits++] = last;
				cid->opp = 0;
				cid->run = 0;
			}
		} else {
			/*
			 * Half a bit of disagreement is a transition, and
			 * emitting it here is what aligns the receiver to the
			 * incoming baud.
			 */
			cid->opp++;
			if (cid->opp == half) {
				cid->last_bit = bit;
				bits[nbits++] = bit;
				cid->opp = 0;
				cid->run = 0;
			}
		}

		cid->dead = 0;
	}

	return (short)nbits;
}
