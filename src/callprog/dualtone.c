/*
 * dualtone.c -- Dual Tone Detector: answer-tone detection.
 *
 * Reconstructed from dsplibs.o DualTone_Detector.c:
 *
 *   TONE_read                      .text 0x07e240
 *   Get_Detection_Threshold_Table  .text 0x07e2c0
 *   Dual_TONE_create               .text 0x07e2e0
 *   Dual_TONE_delete               .text 0x07e320
 *   Dual_TONE_detect               .text 0x07e330
 *
 * See dualtone.h for what it decides and why the measurement is a notch
 * difference.  This file is about how the arithmetic is spelled, which has
 * several details that a straightforward rewrite gets wrong.
 *
 * SCALING
 *
 * Q14 here, unlike toneiir.c's Q13.  Every filter section computes its sum in
 * 32 bits, shifts right by 14, and truncates to 16 -- no rounding, no
 * saturation, wrap on overflow.
 *
 * The input bandpass additionally *doubles* each section's output before
 * storing it, so its coefficients are effectively Q15 held in Q14 words: b0
 * reads 8192 where the design says 1.0.  The doubling happens after the
 * truncation to 16 bits, not before, so it is not the same as one extra bit
 * of shift.
 *
 * THE SHIFT TABLE HAS A UNUSED FIRST ENTRY
 *
 * The table is { 3, 3, 3, 0 }: the input shift and one per section.  But the
 * input shift is written into the code as a literal 3 and the table's first
 * entry is never read.  The two agree, so nothing is wrong; it just means
 * changing the table alone would not change the input scaling.
 */

#include "dsplib/dualtone.h"
#include "dsplib/sysdep.h"

/*
 * ---------------------------------------------------------------------------
 * Tables (.rodata; all file statics in the original, so none of them can be
 * compared against the blob directly -- they are checked through behaviour).
 */

/*
 * Quarter-wave cosine, Q14, 513 entries covering 0 to pi/2 of a 2048-point
 * cycle.  Entry 0 is 16384 (cos 0) and entry 512 is 0 (cos pi/2).
 * .rodata+0x6920.
 */
static const short TONE_quarter_cosine[513] = {
	 16384,  16384,  16384,  16383,  16383,  16382,  16381,  16380,
	 16379,  16378,  16376,  16375,  16373,  16371,  16369,  16367,
	 16364,  16362,  16359,  16356,  16353,  16350,  16347,  16343,
	 16340,  16336,  16332,  16328,  16324,  16319,  16315,  16310,
	 16305,  16300,  16295,  16290,  16284,  16279,  16273,  16267,
	 16261,  16255,  16248,  16242,  16235,  16228,  16221,  16214,
	 16207,  16199,  16192,  16184,  16176,  16168,  16160,  16151,
	 16143,  16134,  16125,  16116,  16107,  16098,  16088,  16079,
	 16069,  16059,  16049,  16039,  16029,  16018,  16008,  15997,
	 15986,  15975,  15964,  15952,  15941,  15929,  15917,  15905,
	 15893,  15881,  15868,  15856,  15843,  15830,  15817,  15804,
	 15791,  15777,  15763,  15750,  15736,  15722,  15707,  15693,
	 15679,  15664,  15649,  15634,  15619,  15604,  15588,  15573,
	 15557,  15541,  15525,  15509,  15493,  15476,  15460,  15443,
	 15426,  15409,  15392,  15375,  15357,  15340,  15322,  15304,
	 15286,  15268,  15250,  15231,  15213,  15194,  15175,  15156,
	 15137,  15118,  15098,  15078,  15059,  15039,  15019,  14999,
	 14978,  14958,  14937,  14917,  14896,  14875,  14854,  14832,
	 14811,  14789,  14768,  14746,  14724,  14702,  14680,  14657,
	 14635,  14612,  14589,  14566,  14543,  14520,  14497,  14473,
	 14449,  14426,  14402,  14378,  14354,  14329,  14305,  14280,
	 14256,  14231,  14206,  14181,  14155,  14130,  14104,  14079,
	 14053,  14027,  14001,  13975,  13949,  13922,  13896,  13869,
	 13842,  13815,  13788,  13761,  13733,  13706,  13678,  13651,
	 13623,  13595,  13567,  13538,  13510,  13482,  13453,  13424,
	 13395,  13366,  13337,  13308,  13279,  13249,  13219,  13190,
	 13160,  13130,  13100,  13069,  13039,  13008,  12978,  12947,
	 12916,  12885,  12854,  12823,  12792,  12760,  12729,  12697,
	 12665,  12633,  12601,  12569,  12537,  12504,  12472,  12439,
	 12406,  12373,  12340,  12307,  12274,  12240,  12207,  12173,
	 12140,  12106,  12072,  12038,  12004,  11970,  11935,  11901,
	 11866,  11831,  11797,  11762,  11727,  11691,  11656,  11621,
	 11585,  11550,  11514,  11478,  11442,  11406,  11370,  11334,
	 11297,  11261,  11224,  11188,  11151,  11114,  11077,  11040,
	 11003,  10966,  10928,  10891,  10853,  10815,  10778,  10740,
	 10702,  10663,  10625,  10587,  10549,  10510,  10471,  10433,
	 10394,  10355,  10316,  10277,  10238,  10198,  10159,  10120,
	 10080,  10040,  10001,   9961,   9921,   9881,   9841,   9800,
	  9760,   9720,   9679,   9638,   9598,   9557,   9516,   9475,
	  9434,   9393,   9352,   9310,   9269,   9227,   9186,   9144,
	  9102,   9061,   9019,   8977,   8935,   8892,   8850,   8808,
	  8765,   8723,   8680,   8638,   8595,   8552,   8509,   8466,
	  8423,   8380,   8337,   8293,   8250,   8207,   8163,   8119,
	  8076,   8032,   7988,   7944,   7900,   7856,   7812,   7768,
	  7723,   7679,   7635,   7590,   7545,   7501,   7456,   7411,
	  7366,   7321,   7276,   7231,   7186,   7141,   7096,   7050,
	  7005,   6960,   6914,   6868,   6823,   6777,   6731,   6685,
	  6639,   6593,   6547,   6501,   6455,   6409,   6363,   6316,
	  6270,   6223,   6177,   6130,   6084,   6037,   5990,   5943,
	  5897,   5850,   5803,   5756,   5708,   5661,   5614,   5567,
	  5520,   5472,   5425,   5377,   5330,   5282,   5235,   5187,
	  5139,   5092,   5044,   4996,   4948,   4900,   4852,   4804,
	  4756,   4708,   4660,   4612,   4563,   4515,   4467,   4418,
	  4370,   4321,   4273,   4224,   4176,   4127,   4078,   4030,
	  3981,   3932,   3883,   3835,   3786,   3737,   3688,   3639,
	  3590,   3541,   3492,   3442,   3393,   3344,   3295,   3246,
	  3196,   3147,   3098,   3048,   2999,   2949,   2900,   2851,
	  2801,   2752,   2702,   2652,   2603,   2553,   2503,   2454,
	  2404,   2354,   2305,   2255,   2205,   2155,   2105,   2055,
	  2006,   1956,   1906,   1856,   1806,   1756,   1706,   1656,
	  1606,   1556,   1506,   1456,   1406,   1356,   1306,   1255,
	  1205,   1155,   1105,   1055,   1005,    955,    904,    854,
	   804,    754,    704,    653,    603,    553,    503,    452,
	   402,    352,    302,    251,    201,    151,    101,     50,
	     0
};

/*
 * Detection thresholds, .rodata+0x6900.  Indexed as 45 - level, so the table
 * reads backwards: a lower level number gives a larger threshold.
 */
static const short Detection_Threshold[16] = {
	 90,  92,  96,  97,  99, 102, 185, 188,
	190, 250, 280, 285, 370, 390, 470, 560
};

/*
 * Input bandpass, .rodata+0x6d36.  Three sections, Q14, coefficient order
 * { a2, b2, a1, b1, b0 } -- the reverse of the usual, and the reverse of what
 * fpm_iir.h uses.  The zeros of sections 1 and 2 sit on the unit circle at DC
 * and Nyquist; the passband is centred at 0.26 of the sample rate, which is
 * 2100 Hz at the 8000 Hz this module runs at.
 */
static const short DualTone_bp_coeff[5 * DUAL_TONE_BP_SECTIONS] = {
	6862, -8192,  1495,      0, 8192,
	7508,  8192, -1140, -16384, 8192,
	7531,  8192,  4185,  16384, 8192
};

/*
 * .rodata+0x6d2e.  Entry 0 duplicates the literal input shift and is never
 * read; entries 1 to 3 rescale each section's output.
 */
static const short DualTone_bp_shift[4] = { 3, 3, 3, 0 };

/*
 * The three notches.  All share pole radius 0.9, so all share a2; only b1 and
 * a1 place the frequency.  Written out rather than tabulated because the
 * original inlines them as immediates -- there is no table in the object.
 */
#define NOTCH_A2	13271		/* 0.81 = 0.9^2, Q14              */
#define NOTCH_A_B1	 2571		/* zero at 0.26250 fs -- 2100 Hz  */
#define NOTCH_A_A1	 2314
#define NOTCH_B_B1	-5126		/* zero at 0.22500 fs -- 1800 Hz  */
#define NOTCH_B_A1	-4613
#define NOTCH_C_B1	 6393		/* zero at 0.28125 fs -- 2250 Hz  */
#define NOTCH_C_A1	 5753

/* Energy smoother, Q8: y = (205 y + 51 x) / 256, a 0.8/0.2 one-pole. */
#define ENERGY_ALPHA	205
#define ENERGY_BETA	51

/*
 * ---------------------------------------------------------------------------
 * TONE_read
 *
 * Reconstructs a full 2048-point cosine from the quarter stored above, by
 * folding the phase into one of four quadrants.  Two of the four negate the
 * table value; that is not an error, it is what makes the second and third
 * quadrants of a cosine.
 */
short
TONE_read(short phase)
{
	int p = phase & 0x7ff;

	if (p >= 0x201 && p <= 0x400)
		return (short)-TONE_quarter_cosine[0x400 - p];
	if (p >= 0x401 && p <= 0x600)
		return (short)-TONE_quarter_cosine[p - 0x400];
	if (p >= 0x601)
		return TONE_quarter_cosine[0x800 - p];
	return TONE_quarter_cosine[p];
}

/*
 * ---------------------------------------------------------------------------
 * Get_Detection_Threshold_Table
 *
 * Unchecked, as in the original: a level outside 30..45 indexes outside the
 * table.  Levels above 45 read backwards into the .rodata padding that
 * precedes it (zeros), levels below 30 read forwards into the cosine table.
 */
short
Get_Detection_Threshold_Table(short level)
{
	return Detection_Threshold[(short)(45 - level)];
}

/*
 * ---------------------------------------------------------------------------
 * Dual_TONE_create / delete
 */
struct dual_tone *
Dual_TONE_create(void)
{
	struct dual_tone *st;

	/*
	 * sizeof, not the 0x44 literal the original uses.  They agree on the
	 * 32-bit differential target; on a 64-bit host they would not, and
	 * the literal would under-allocate.
	 */
	st = (struct dual_tone *)sysdep_malloc(sizeof(*st));
	if (st == 0)
		return 0;

	sysdep_memset(st, 0, sizeof(*st));
	st->ratio = 226;
	st->min_energy = 1;

	return st;
}

void
Dual_TONE_delete(struct dual_tone *st)
{
	sysdep_free(st);
}

/*
 * ---------------------------------------------------------------------------
 * Dual_TONE_detect
 */

/*
 * One biquad of the input bandpass.  Coefficients { a2, b2, a1, b1, b0 };
 * history { x[n-1], x[n-2], y[n-1], y[n-2] }.
 *
 * The accumulator is allowed to wrap, which is what the original's 32-bit
 * add and sub do; spelled through unsigned so it is defined rather than left
 * to the compiler.  Five products of two 16-bit values do not fit in 32 bits
 * in general, and nothing here bounds the history.
 */
static short
bp_section(short *h, const short *c, short x)
{
	int acc = 0;
	short y;

	acc = (int)((unsigned)acc + (unsigned)(c[4] * (int)x));
	acc = (int)((unsigned)acc + (unsigned)(c[3] * (int)h[0]));
	acc = (int)((unsigned)acc + (unsigned)(c[1] * (int)h[1]));
	acc = (int)((unsigned)acc - (unsigned)(c[2] * (int)h[2]));
	acc = (int)((unsigned)acc - (unsigned)(c[0] * (int)h[3]));

	/*
	 * Truncate to 16 bits, THEN double.  Doing it the other way round --
	 * shifting by 13 instead of 14 -- differs whenever the truncation
	 * discards a bit, which is most of the time.
	 */
	y = (short)(2 * (short)(acc >> 14));

	h[1] = h[0];
	h[0] = x;
	h[3] = h[2];
	h[2] = y;

	return y;
}

/*
 * One notch.  b0 and b2 are exactly 1.0, hence the shifts rather than
 * multiplies; a2 is shared by all three.
 */
static short
notch(short *h, short x, int b1, int a1)
{
	int acc = 0;
	short y;

	acc = (int)((unsigned)acc + ((unsigned)(int)x << 14));
	acc = (int)((unsigned)acc + (unsigned)(b1 * (int)h[0]));
	acc = (int)((unsigned)acc + ((unsigned)(int)h[1] << 14));
	acc = (int)((unsigned)acc - (unsigned)(a1 * (int)h[2]));
	acc = (int)((unsigned)acc - (unsigned)(NOTCH_A2 * (int)h[3]));

	y = (short)(acc >> 14);

	h[1] = h[0];
	h[0] = x;
	h[3] = h[2];
	h[2] = y;

	return y;
}

/*
 * Energy removed by a notch: the total, less what survived it, floored at
 * zero.  Both squares are scaled down by 256 first, which is what stops the
 * smoother's 205x multiply from overflowing.
 *
 * The comparison is unsigned in the original.  It cannot matter here -- both
 * operands are squares shifted right logically, so both are non-negative --
 * but it is reproduced rather than reasoned away.
 */
static unsigned
notch_energy(unsigned total, short filtered)
{
	unsigned f = (unsigned)((int)filtered * (int)filtered) >> 8;

	return f > total ? 0u : total - f;
}

static unsigned
smooth(unsigned acc, unsigned x)
{
	return (ENERGY_ALPHA * acc + ENERGY_BETA * x) >> 8;
}

int
Dual_TONE_detect(struct dual_tone *st, const short *samples, int count)
{
	unsigned energy_a = (unsigned)st->energy_a;
	unsigned energy_b = (unsigned)st->energy_b;
	unsigned energy = (unsigned)st->energy;
	unsigned threshold;
	short i;

	/*
	 * Both hold timers advance by the whole block before a single sample
	 * is looked at, and both are 16 bits, so they wrap silently after
	 * about eight seconds of a tone that never wins.  Nothing depends on
	 * that: whichever tone does not win has its timer cleared below.
	 */
	st->hold_a = (short)(st->hold_a + count);
	st->hold_b = (short)(st->hold_b + count);

	/*
	 * `i` is truncated to 16 bits each time round in the original, so a
	 * block of more than 32767 samples would never terminate.  Reproduced
	 * with a short rather than papered over -- see docs/deviations.md.
	 */
	for (i = 0; i < count; i++) {
		short x = (short)(samples[i] >> 3);
		unsigned total;
		short ya, yb, yc;
		int s;

		for (s = 0; s < DUAL_TONE_BP_SECTIONS; s++) {
			x = bp_section(&st->bp[4 * s], &DualTone_bp_coeff[5 * s],
				       x);
			x = (short)((int)x >> DualTone_bp_shift[s + 1]);
		}

		ya = notch(st->notch_a, x, NOTCH_A_B1, NOTCH_A_A1);
		yb = notch(st->notch_b, x, NOTCH_B_B1, NOTCH_B_A1);
		yc = notch(st->notch_c, yb, NOTCH_C_B1, NOTCH_C_A1);

		total = (unsigned)((int)x * (int)x) >> 8;

		energy_a = smooth(energy_a, notch_energy(total, ya));
		energy_b = smooth(energy_b, notch_energy(total, yc));
		energy = smooth(energy, total);
	}

	st->energy_a = (int)energy_a;
	st->energy_b = (int)energy_b;
	st->energy = (int)energy;

	if (energy < (unsigned)st->min_energy) {
		st->hold_a = 0;
		st->hold_b = 0;
		return DUAL_TONE_NOSIGNAL;
	}

	threshold = ((unsigned)st->ratio * energy) >> 8;

	if (energy_a > threshold) {
		/*
		 * A wins, so B's timer restarts.  On the confirmed branch A's
		 * restarts too, which re-arms the detector rather than
		 * latching -- a tone held for a second reports confirmed once
		 * every 160 ms, not continuously.
		 */
		if (st->hold_a < DUAL_TONE_HOLD) {
			st->hold_b = 0;
			return DUAL_TONE_A;
		}
		st->hold_a = 0;
		st->hold_b = 0;
		return DUAL_TONE_A_CONFIRMED;
	}

	if (energy_b > threshold) {
		if (st->hold_b < DUAL_TONE_HOLD) {
			st->hold_a = 0;
			return DUAL_TONE_B;
		}
		st->hold_b = 0;
		st->hold_a = 0;
		return DUAL_TONE_B_CONFIRMED;
	}

	st->hold_a = 0;
	st->hold_b = 0;
	return DUAL_TONE_OTHER;
}
