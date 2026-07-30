/*
 * t_fpm_div.c -- exhaustive differential test of the reciprocal lookup.
 *
 * The input is a single 16-bit value, so every one of the 65536 denominators
 * is checked -- including the 255 that drive the table index out of range
 * (deviation D4).  Those are the whole reason this test is exhaustive rather
 * than sampled: at 0.39% of the space, a random sweep would likely miss them,
 * and they are where the original and any "tidied" reconstruction diverge.
 */

#include "harness.h"
#include "dsplib/fpm.h"

extern int ref_FPM_div(unsigned short denom, unsigned short *recip,
		       unsigned short *shift);

int
main(void)
{
	int rc = 0;
	int i, overrun = 0;

	diff_begin("div table generator");
	for (i = 0; i < 128; i++)
		diff_eq_int("table[%ld]", FPM_div_table_entry(i),
			    FPM_div_table_generate(i), i);
	rc |= diff_end();

	diff_begin("FPM_div exhaustive");
	for (i = 0; i < 0x10000; i++) {
		unsigned short ra = 0xdead, sa = 0xbeef;
		unsigned short rb = 0xdead, sb = 0xbeef;
		int va, vb;

		va = ref_FPM_div((unsigned short)i, &ra, &sa);
		vb = FPM_div((unsigned short)i, &rb, &sb);

		diff_eq_int("denom %ld: return", vb, va, i);
		diff_eq_int("denom %ld: reciprocal", rb, ra, i);
		diff_eq_int("denom %ld: shift", sb, sa, i);

		/* Count the cases that land on the out-of-range entry. */
		if (i != 0 && ra == 0)
			overrun++;
	}
	rc |= diff_end();

	/*
	 * Assert D4 is real and reproduced, so this cannot quietly regress
	 * into a "tidied" version that returns 16384 there.  255 denominators
	 * yield a zero reciprocal in the original.
	 */
	diff_begin("D4 overrun reproduced");
	diff_eq_int("denominators yielding a zero reciprocal (%ld)",
		    overrun, 255, 0);
	rc |= diff_end();

	return rc;
}
