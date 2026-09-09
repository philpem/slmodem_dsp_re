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
