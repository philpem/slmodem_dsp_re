/*
 * FP_math.c -- Fixed Point maths helpers.
 *
 * Reconstructed from dsplibs.o FP_math.c:
 *   GetFP_Value  .text 0x07e150
 *   FP_Pow       .text 0x07e1b0
 *
 * Used by the V32FP_* diagnostics path.
 */

#include "dsplib/fp_math.h"
#include "dsplib/dualtone.h"

/*
 * Maclaurin coefficients for e^x in Q14: entry i is 16384 / (i+1)!.
 *
 *   16384/1!  = 16384      16384/5!  = 136.5  -> 136
 *   16384/2!  =  8192      16384/6!  =  22.8  -> 23
 *   16384/3!  =  2730.7 -> 2730      16384/7! =  3.25 -> 3
 *   16384/4!  =   682.7 -> 683
 *
 * The rounding is inconsistent: 2730 and 136 are truncated (2730.67, 136.53)
 * while 683 and 23 are rounded up (682.67, 22.76).  No single rule reproduces
 * all seven, so these are kept as extracted rather than generated -- the
 * design is recovered, the exact arithmetic that produced the last bit is not.
 * The self-check below asserts each entry is within 1 of 16384/(i+1)!, which
 * is enough to catch a transcription error without pretending to a precision
 * we do not have.
 *
 * What the values do establish is the identity: FP_Pow is not a general power
 * function, it is exp().
 */
#define FP_POW_TERMS 7

static const short fp_pow_coef[FP_POW_TERMS] = {
	16384, 8192, 2730, 683, 136, 23, 3,
};

short
FP_Pow_coefficient(int i)
{
	return (short)((i >= 0 && i < FP_POW_TERMS) ? fp_pow_coef[i] : 0);
}

short
FP_Pow_coefficient_generate(int i)
{
	double fact = 1.0;
	int k;

	for (k = 2; k <= i + 1; k++)
		fact *= k;

	/* Ideal value; see the note above on why this is a tolerance, not a match. */
	return (short)(16384.0 / fact);
}

short
GetFP_Value(short a, short b)
{
	int va = a < 0 ? -a : a;
	int vb = b < 0 ? -b : b;
	int remaining = va << 14;
	int count = 0;

	/*
	 * The original divides by repeated subtraction, counting iterations in
	 * a 16-bit register that is allowed to wrap.  Computed directly here;
	 * the ceiling form matches because the loop stops only once the
	 * remainder has gone non-positive.
	 */
	if (remaining > 0 && vb != 0)
		count = (short)((remaining + vb - 1) / vb);

	return (short)(((int)a * (int)b) > 0 ? (short)count : (short)(-count));
}

int
FP_Pow(int x)
{
	int result = 0x4000;		/* 1.0 in Q14 */
	int term = x;
	int i;

	/*
	 * Two paths, chosen by magnitude, and they are NOT the same expression
	 * with a different shift order -- the scaling differs.
	 *
	 * The original branches on (unsigned)(x + 0x3fff) > 0x7ffe, which is
	 * exactly |x| > 0x3fff, i.e. |x| > 1.0 in Q14.
	 */
	if ((unsigned)(x + 0x3fff) > 0x7ffeu) {
		/*
		 * Large |x|: shift the term down to an integer first, so the
		 * product cannot overflow.  The contribution is then already
		 * at Q14 scale and is added unshifted.
		 */
		for (i = 0; i < FP_POW_TERMS; i++) {
			int scaled = term >> 14;
			int contribution = scaled * fp_pow_coef[i];

			term = scaled * x;
			result += contribution;

			if ((contribution < 0 ? -contribution : contribution) <= 0)
				break;
		}
	} else {
		/*
		 * Small |x|: multiply at full precision and shift afterwards,
		 * which keeps the low bits that the other path discards.
		 */
		for (i = 0; i < FP_POW_TERMS; i++) {
			int product = term * fp_pow_coef[i];
			int contribution = product >> 14;

			term = (term * x) >> 14;
			result += contribution;

			if ((contribution < 0 ? -contribution : contribution) <= 0)
				break;
		}
	}

	return result;
}

/* FPTONE and ThresholdsTable are surviving locals owned by FP_math.c. */
/*
 * Quarter-wave cosine, Q14, 513 entries covering 0 to pi/2 of a 2048-point
 * cycle.  Entry 0 is 16384 (cos 0) and entry 512 is 0 (cos pi/2).
 * .rodata+0x6920.
 */
static const short FPTONE[513] = {
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
static const short ThresholdsTable[16] = {
	 90,  92,  96,  97,  99, 102, 185, 188,
	190, 250, 280, 285, 370, 390, 470, 560
};

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
		return (short)-FPTONE[0x400 - p];
	if (p >= 0x401 && p <= 0x600)
		return (short)-FPTONE[p - 0x400];
	if (p >= 0x601)
		return FPTONE[0x800 - p];
	return FPTONE[p];
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
	return ThresholdsTable[(short)(45 - level)];
}

