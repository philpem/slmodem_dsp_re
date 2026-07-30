/*
 * t_fpm_sqrt.c -- Q15 square root: differential test plus table self-check.
 *
 * Two independent things are verified here:
 *
 *   1. The generator reproduces the original coefficient table exactly.  This
 *      is what makes the table maintainable rather than a magic blob -- if the
 *      derivation were wrong, regenerating at another scale would silently
 *      produce a subtly different filter.
 *
 *   2. FPM_sqrt agrees with the original across its entire defined domain,
 *      0x0000..0x7fff, exhaustively.
 *
 * Inputs above 0x7fff are deliberately excluded: the original indexes past the
 * end of its table there, and that is documented rather than reproduced.
 */

#include "harness.h"
#include "dsplib/fpm.h"

extern unsigned short ref_FPM_sqrt(unsigned short x);
extern unsigned short ref_FPM_sqrt_dp(unsigned int x);

int
main(void)
{
	int rc = 0;
	int i;

	diff_begin("sqrt_table/generator");
	for (i = 0; i < FPM_sqrt_table_size(); i++)
		diff_eq_int("table[%ld]", FPM_sqrt_table_generate(i),
			    FPM_sqrt_table_entry(i), i);
	rc |= diff_end();

	/*
	 * 32-bit sibling.  The input space is too large to sweep exhaustively,
	 * so cover it structurally: every power of two and its neighbours (the
	 * normalisation boundaries), the 0x1fffffff threshold, the 0x80000000
	 * point where the mantissa truncation bites, and a deterministic
	 * pseudorandom spread.
	 */
	diff_begin("FPM_sqrt_dp structural");
	{
		unsigned lfsr = 0x13579BDFu;
		int b;

		for (b = 0; b < 32; b++) {
			unsigned base = 1u << b;
			int d;

			for (d = -2; d <= 2; d++) {
				unsigned v = base + (unsigned)d;

				diff_eq_int("sqrt_dp(0x%08lx)", FPM_sqrt_dp(v),
					    ref_FPM_sqrt_dp(v), v);
			}
		}
		for (i = 0; i < 200000; i++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xD0000001u);
			diff_eq_int("sqrt_dp(0x%08lx)", FPM_sqrt_dp(lfsr),
				    ref_FPM_sqrt_dp(lfsr), lfsr);
		}
	}
	rc |= diff_end();

	/* The region FPM_rms can actually reach, swept densely. */
	diff_begin("FPM_sqrt_dp rms range");
	for (i = 0; i < 0x40000; i++) {
		unsigned v = (unsigned)i * 8192u;

		diff_eq_int("sqrt_dp(0x%08lx)", FPM_sqrt_dp(v),
			    ref_FPM_sqrt_dp(v), v);
	}
	rc |= diff_end();

	diff_begin("FPM_sqrt/Q15");
	for (i = 0; i <= 0x7fff; i++)
		diff_eq_int("FPM_sqrt(0x%04lx)", FPM_sqrt((unsigned short)i),
			    ref_FPM_sqrt((unsigned short)i), i);
	rc |= diff_end();

	return rc;
}
