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
#include "dsplib/debug.h"
#include "dsplib/fpm.h"

extern unsigned int ref_dsplibs_debug_level;
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
	/*
	 * This binary is built with -DDSPLIB_REPRODUCE_BUGS, because its job
	 * is to prove equivalence with the blob and it cannot do that against
	 * a fixed table.  Assert BOTH the count and the switch, so a build
	 * that lost the define fails here rather than quietly comparing a
	 * fixed implementation against a buggy blob and reporting a mismatch
	 * somewhere far away.
	 */
#ifndef DSPLIB_REPRODUCE_BUGS
#error "t_fpm_div must be built with -DDSPLIB_REPRODUCE_BUGS"
#endif
	diff_eq_int("denominators yielding a zero reciprocal (%ld)",
		    overrun, 255, 0);
	diff_eq_int("the fixed table would return 16384 there (%ld)",
		    FPM_div_table_generate(128), 16384, 0);
	rc |= diff_end();

	/*
	 * The divide-by-zero complaint, which is the one path in this file
	 * that announces anything and had never executed: every caller here
	 * passes a real denominator, and at level 0 the announcement is a
	 * branch nobody takes.  Level 1 must be silent -- the gate is `> 1`.
	 */
	diff_begin("FPM_div: division by zero says so");
	{
		unsigned short ra, sa, rb, sb;
		unsigned lines = 0;
		int lvl;

		for (lvl = 1; lvl <= 3; lvl++) {
			dsplibs_debug_level = ref_dsplibs_debug_level =
				(unsigned)lvl;
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			ra = sa = rb = sb = 0x5a5a;
			diff_eq_int("returns the same", FPM_div(0, &rb, &sb),
				    ref_FPM_div(0, &ra, &sa), lvl);
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			dsplib_debug_capture_on = 0;
			diff_eq_int("transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, lvl);
			if (lvl == 1)
				diff_eq_int("level 1 silent",
					    (int)dsplib_debug_capture_lines(1),
					    0, lvl);
			else
				lines += dsplib_debug_capture_lines(1);
		}
		diff_eq_int("it said something (%ld)", lines > 0, 1,
			    (long)lines);
	}
	rc |= diff_end();

	return rc;
}
