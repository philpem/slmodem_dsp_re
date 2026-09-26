/*
 * Dtmf_Detector.c -- reconstructed from dsplibs.o Dtmf_Detector.c.
 *
 *   DTMF_MTD_detect  .text 0x0927b0  947 bytes
 *
 * The whole translation unit.
 *
 * WHAT IT DOES, and why the group pre-notches are the interesting part.
 *
 * Each of the eight tones gets its own notch section (dtmf_mtd_coeffs.c), so
 * a tone that IS present comes out of its own section small -- the search at
 * the bottom is for a minimum.  Ahead of the eight sits one more notch per
 * GROUP, and both are reused out of the same bank: the low group's four
 * sections are fed through MTD5 (1209 Hz) so the high tone is taken out
 * first, and the high group's four through MTD4 (941 Hz) so the low tone is.
 * That is why the code indexes coef[4] and coef[3] before either inner loop
 * and why no ninth or tenth table exists.
 *
 * The per-tone bias, +i for i = 0..3 and +(7-i) for i = 4..7 in units of
 * wideband/4096, is measured; the reading that it compensates the pre-notch's
 * tilt -- the pre-notch is only 90% radius, so the tone nearest the other
 * group is the one it attenuates least -- is a reading and not a derivation,
 * and it is not what makes this bank get things wrong.  What does is the
 * 1477 Hz table at 9600 Hz: see finding F1416 and D250, where the bias was
 * first blamed and then measured out of it.
 */

#include <string.h>

#include "dsplib/dtmf_rx.h"
#include "dsplib/fpm_iir.h"

/* 0.9 in Q15: everything is scaled down by a tenth before it is filtered. */
#define DTMF_MTD_INSCALE	0x7332

/* The wideband multiplier of the two search thresholds, 1.2 and 1.5. */
#define DTMF_MTD_TIGHT		0x6666

int
DTMF_MTD_detect(const short *samples, short count, struct dtmf_rx *rx)
{
	/*
	 * (low, high) -> keypad code.  Rows are 697 770 852 941 and columns
	 * 1209 1336 1477 1633, so the last row is * 0 # D.
	 */
	short digits[16] = {
		1, 2, 3, 10,
		4, 5, 6, 11,
		7, 8, 9, 12,
		14, 0, 15, 13,
	};
	const short *coef[8];
	unsigned energy[8];
	unsigned wide = 0;
	unsigned scale, tight;
	unsigned best;
	short lo = 9, hi = 9;
	short i;
	short j;

	memset(energy, 0, sizeof(energy));

	if (rx->rate == DTMF_RX_RATE_9600) {
		coef[0] = MTD1_COEF_9600;
		coef[1] = MTD2_COEF_9600;
		coef[2] = MTD3_COEF_9600;
		coef[3] = MTD4_COEF_9600;
		coef[4] = MTD5_COEF_9600;
		coef[5] = MTD6_COEF_9600;
		coef[6] = MTD7_COEF_9600;
		coef[7] = MTD8_COEF_9600;
	} else {
		coef[0] = MTD1_COEF_8000;
		coef[1] = MTD2_COEF_8000;
		coef[2] = MTD3_COEF_8000;
		coef[3] = MTD4_COEF_8000;
		coef[4] = MTD5_COEF_8000;
		coef[5] = MTD6_COEF_8000;
		coef[6] = MTD7_COEF_8000;
		coef[7] = MTD8_COEF_8000;
	}

	for (i = 0; i < count; i++) {
		short v;
		short y;

		v = (short)(((int)samples[i] * DTMF_MTD_INSCALE + 0x4000) >> 15);

		/*
		 * The wideband reference runs ahead of the tone energies, so
		 * the bias each tone gets this sample already includes this
		 * sample.  Preserved: moving the accumulation after the loops
		 * changes every energy.
		 */
		wide += ((int)v * v + 4) >> 3;

		y = FPM_iir_filt(v, coef[4], rx->pre_low, 1);
		for (j = 0; j <= 3; j++) {
			short z = FPM_iir_filt(y, coef[j], rx->tone_state[j],
					       1);

			energy[j] += (wide >> 12) * (unsigned)j
			    + (unsigned)(((int)z * z + 4) >> 3);
		}

		y = FPM_iir_filt(v, coef[3], rx->pre_high, 1);
		for (j = 4; j <= 7; j++) {
			short z = FPM_iir_filt(y, coef[j], rx->tone_state[j],
					       1);

			energy[j] += (wide >> 12) * (unsigned)(7 - j)
			    + (unsigned)(((int)z * z + 4) >> 3);
		}
	}

	scale = (wide + 0x4000) >> 15;
	for (j = 0; j <= 7; j++)
		energy[j] = (energy[j] + 0x4000) >> 15;

	/* 1.5 * wideband, and 1.2 * wideband, both in one integer step. */
	scale = (3 * scale + 1) >> 1;
	tight = (scale * DTMF_MTD_TIGHT + 0x4000) >> 15;

	/*
	 * The low group has to beat the tighter of the two thresholds.  Not
	 * symmetric, and not obviously deliberate, but it is what the object
	 * does: the low search compares against `tight` and the high search
	 * against `scale`.
	 */
	best = scale;
	for (j = 0; j <= 3; j++)
		if (energy[j] < tight && energy[j] < best) {
			best = energy[j];
			lo = j;
		}

	best = scale;
	for (j = 0; j <= 3; j++)
		if (energy[j + 4] < scale && energy[j + 4] < best) {
			best = energy[j + 4];
			hi = j;
		}

	if (lo <= 3 && hi <= 3)
		return digits[hi + lo * 4];
	return -9;
}


/*
 * The sixteen fixed-point MTD coefficient tables, moved here VERBATIM from
 * the deleted ours-only dtmf_mtd_coeffs.c: the blob defines them in
 * Dtmf_Detector.c (F11412).  See the deleted file and D250 for MTD7_COEF_9600.
 */
const short MTD1_COEF_8000[5] = { -13271, 16384, 25182, -27979, 16384 };
const short MTD2_COEF_8000[5] = { -13271, 16384, 24261, -26957, 16384 };
const short MTD3_COEF_8000[5] = { -13271, 16384, 23131, -25702, 16384 };
const short MTD4_COEF_8000[5] = { -13271, 16384, 21797, -24219, 16384 };
const short MTD5_COEF_8000[5] = { -13271, 16384, 17166, -19073, 16384 };
const short MTD6_COEF_8000[5] = { -13271, 16384, 14692, -16325, 16384 };
const short MTD7_COEF_8000[5] = { -13271, 16384, 11777, -13084, 16384 };
const short MTD8_COEF_8000[5] = { -13271, 16384, 8384, -9314, 16384 };

const short MTD1_COEF_9600[5] = { -13271, 16384, 26475, -29417, 16384 };
const short MTD2_COEF_9600[5] = { -13271, 16384, 25824, -28695, 16384 };
const short MTD3_COEF_9600[5] = { -13271, 16384, 25023, -27804, 16384 };
const short MTD4_COEF_9600[5] = { -13271, 16384, 24073, -26747, 16384 };
const short MTD5_COEF_9600[5] = { -13271, 16384, 20731, -23034, 16384 };
const short MTD6_COEF_9600[5] = { -13271, 16384, 18917, -21019, 16384 };
/*
 * D250, and the ONE deviation in this file with a fix behind the define.
 *
 * `b1 = -21143` puts this notch's ZEROS at 1328.45 Hz while its `a1 = 16751`
 * keeps its POLES at 1477.04, and a biquad whose zeros and poles are 150 Hz
 * apart is not a notch at either frequency.  Every other table in the bank
 * matches its own `a1` to within 0.1 Hz (finding F1413).
 *
 * IT FIRES, AND THE CONSEQUENCE IS MEASURED, not argued: at 9600 Hz eleven of
 * the sixteen DTMF pairs come back with the wrong high-group tone, and all
 * eleven are 1477 Hz chosen when absent or missed when present (finding
 * F1416).  At 8000 Hz all sixteen decode.
 *
 * -18613 IS DERIVED, NOT GUESSED, which is the test this fix had to pass
 * before it was worth making.  The bank's own design rule is
 * `b1 = -round(2 cos(w0) * 2^14)`, and at 9600 Hz with w0 = 2*pi*1477/9600
 * that is -18613.  Four independent routes agree on it: the design rule, the
 * fifteen sibling tables that follow it, this table's own `a1` read back
 * through `a1 = round(1.8 cos(w0) * 2^14)`, and the 8000 Hz twin scaled to
 * this rate.  The object's -21143 is not a bit flip of it, not a digit
 * transposition, and not the right coefficient for 1477 Hz at 7200, 8000 or
 * 9600 (-9114, -13085, -18613).  No mechanism is proposed and none is needed:
 * what the right value is does not depend on knowing how the wrong one
 * arrived.
 */
#ifdef DSPLIB_REPRODUCE_BUGS
const short MTD7_COEF_9600[5] = { -13271, 16384, 16751, -21143, 16384 };
#else
const short MTD7_COEF_9600[5] = { -13271, 16384, 16751, -18613, 16384 };
#endif
const short MTD8_COEF_9600[5] = { -13271, 16384, 14190, -15768, 16384 };
