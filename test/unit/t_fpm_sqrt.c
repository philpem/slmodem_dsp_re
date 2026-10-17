/* Differential sqrt: generator over 192 original entries, DP boundaries,
 * Q15 values, and all 65536 unsigned-word inputs. The latter is a component
 * API probe; it does not establish modem reachability above Q15. */

#include <math.h>

#include "harness.h"
#include "dsplib/fpm.h"

extern unsigned short ref_FPM_sqrt(unsigned short x);
extern unsigned short ref_FPM_sqrt_dp(unsigned int x);

/* Offline derivation belongs to the fixture, not the reconstructed object. */
static unsigned short
FPM_sqrt_table_generate(int index)
{
	/* Compute the ideal table formula in double precision. */
	double frac = (index + 64) / 256.0;
	return (unsigned short)(32768.0 * sqrt(frac));
}

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

	/* Full unsigned-short component boundary; no modem caller claim.
	 * Above Q15 the original reads its following div/xor table words. */
	diff_begin("FPM_sqrt/full unsigned-word original fidelity");
	for (i = 0; i <= 0xffff; i++)
		diff_eq_int("sqrt full word %ld", FPM_sqrt((unsigned short)i),
			    ref_FPM_sqrt((unsigned short)i), i);
	rc |= diff_end();

	return rc;
}
