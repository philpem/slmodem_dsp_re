/*
 * t_fp_math.c -- differential test of the Q14 helpers.
 *
 * FP_Pow is swept across its whole meaningful range, including both sides of
 * the |x| > 0x3fff boundary where it switches scaling -- that branch is the
 * only reason the function is not a single loop, so it must be crossed.
 *
 * GetFP_Value divides by repeated subtraction in the original, so the test
 * avoids operand pairs that would make it run for hundreds of millions of
 * iterations: b is kept large enough that (|a| << 14) / |b| stays bounded.
 * That is a property of the reference, not of our arithmetic form.
 */

#include "harness.h"
#include "dsplib/fp_math.h"

extern int ref_FP_Pow(int x);
extern short ref_GetFP_Value(short a, short b);

int
main(void)
{
	int rc = 0;
	int i, a, b;

	/*
	 * The coefficients are 16384/(i+1)! but rounded inconsistently -- two
	 * truncate, two round up -- so no single rule reproduces them.  Assert
	 * each is within 1 of the ideal, which catches a transcription slip
	 * without claiming a derivation we do not have.
	 */
	diff_begin("FP_Pow coefficients");
	for (i = 0; i < 7; i++) {
		int got = FP_Pow_coefficient(i);
		int ideal = FP_Pow_coefficient_generate(i);
		int err = got - ideal;

		diff_eq_int("coef[%ld] within 1 of 16384/(i+1)!",
			    (err >= 0 && err <= 1), 1, i);
	}
	rc |= diff_end();

	/* Small |x|: the precise scaling path. */
	diff_begin("FP_Pow small");
	for (i = -0x3fff; i <= 0x3fff; i++)
		diff_eq_int("FP_Pow(%ld)", FP_Pow(i), ref_FP_Pow(i), i);
	rc |= diff_end();

	/* Large |x|: the overflow-avoiding path, both signs. */
	diff_begin("FP_Pow large");
	for (i = 0x4000; i <= 0x20000; i += 3) {
		diff_eq_int("FP_Pow(%ld)", FP_Pow(i), ref_FP_Pow(i), i);
		diff_eq_int("FP_Pow(%ld)", FP_Pow(-i), ref_FP_Pow(-i), -i);
	}
	rc |= diff_end();

	/* The boundary itself. */
	diff_begin("FP_Pow boundary");
	for (i = 0x3ff0; i <= 0x4010; i++) {
		diff_eq_int("FP_Pow(%ld)", FP_Pow(i), ref_FP_Pow(i), i);
		diff_eq_int("FP_Pow(%ld)", FP_Pow(-i), ref_FP_Pow(-i), -i);
	}
	rc |= diff_end();

	/*
	 * GetFP_Value.  Keep |b| well above |a| << 14 / 200000 so the
	 * reference's subtraction loop terminates promptly.
	 */
	diff_begin("GetFP_Value");
	for (a = -2000; a <= 2000; a += 7) {
		for (b = -32767; b <= 32767; b += 811) {
			if (b == 0)
				continue;
			diff_eq_int("GetFP_Value(%ld, b)",
				    GetFP_Value((short)a, (short)b),
				    ref_GetFP_Value((short)a, (short)b), a);
		}
	}
	rc |= diff_end();

	/* Zero numerator, and both sign combinations at the extremes. */
	diff_begin("GetFP_Value edges");
	for (b = -32767; b <= 32767; b += 4093) {
		if (b == 0)
			continue;
		diff_eq_int("GetFP_Value(0, %ld)", GetFP_Value(0, (short)b),
			    ref_GetFP_Value(0, (short)b), b);
		diff_eq_int("GetFP_Value(1, %ld)", GetFP_Value(1, (short)b),
			    ref_GetFP_Value(1, (short)b), b);
		diff_eq_int("GetFP_Value(-1, %ld)", GetFP_Value(-1, (short)b),
			    ref_GetFP_Value(-1, (short)b), b);
	}
	rc |= diff_end();

	return rc;
}
