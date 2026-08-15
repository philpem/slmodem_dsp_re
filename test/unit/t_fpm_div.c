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
extern int ref_FPM_div_32(unsigned int denom, unsigned short *recip,
			  unsigned short *shift);

/*
 * The blob's own table, under the name the blob exports.  Ours is `static`,
 * so the comparison goes through the accessor -- but the reference copy is a
 * plain global and can be named directly.
 */
extern const unsigned short ref_FPM_div_table[];

int
main(void)
{
	int rc = 0;
	int i, overrun = 0, overrun32 = 0;

	diff_begin("div table generator");
	for (i = 0; i < 128; i++)
		diff_eq_int("table[%ld]", FPM_div_table_entry(i),
			    FPM_div_table_generate(i), i);
	rc |= diff_end();

	/*
	 * The bytes themselves, against the blob's.  The generator check above
	 * proves the table is the one the design describes; this proves it is
	 * the one the object holds, which is the claim the link closure makes
	 * when our definition takes the blob's name.
	 */
	diff_begin("FPM_div_table against the blob");
	for (i = 0; i < 128; i++)
		diff_eq_int("FPM_div_table[%ld]", FPM_div_table_entry(i),
			    ref_FPM_div_table[i], i);
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
	 * FPM_div_32 over every distinct MANTISSA and every distinct SHIFT.
	 *
	 * 2^32 denominators is infeasible and would prove nothing extra: the
	 * result depends only on the top 16 bits after normalisation and on
	 * the number of shifts it took.  `m` covers every mantissa with a
	 * shift of 16..31, `m << 16` covers the same mantissas with a shift
	 * of 0..15, and between them every (mantissa, shift) pair the routine
	 * can produce is reached.  The mantissas at or above 0xff80 -- the
	 * ones that drive the index off the end of the table, D4 -- are in
	 * both halves.
	 */
	diff_begin("FPM_div_32 over every mantissa and shift");
	for (i = 0; i < 0x10000; i++) {
		unsigned int d[2];
		int k;

		d[0] = (unsigned int)i;
		d[1] = (unsigned int)i << 16;

		for (k = 0; k < 2; k++) {
			unsigned short ra = 0xdead, sa = 0xbeef;
			unsigned short rb = 0xdead, sb = 0xbeef;
			int va, vb;

			va = ref_FPM_div_32(d[k], &ra, &sa);
			vb = FPM_div_32(d[k], &rb, &sb);

			diff_eq_int("denom %ld: return", vb, va, (long)d[k]);
			diff_eq_int("denom %ld: reciprocal", rb, ra,
				    (long)d[k]);
			diff_eq_int("denom %ld: shift", sb, sa, (long)d[k]);

			if (d[k] != 0 && ra == 0)
				overrun32++;
		}
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
	/*
	 * 510 and not 255: the 32-bit sweep visits each of the 255 offending
	 * mantissas twice, once reached by a shift of 16..31 and once by a
	 * shift of 0..15.  The overrun is a property of the mantissa alone,
	 * so both halves must show it.
	 */
	diff_eq_int("32-bit denominators yielding a zero reciprocal (%ld)",
		    overrun32, 510, 0);
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
			/*
			 * FPM_div_32 has its OWN copy of the string -- the
			 * two live at .rodata.str1.4 0x12de0 and 0x12e00 and
			 * were not merged -- so it has to be driven here too
			 * or its copy is never compared with anything.
			 */
			ra = sa = rb = sb = 0x5a5a;
			diff_eq_int("32-bit returns the same",
				    FPM_div_32(0, &rb, &sb),
				    ref_FPM_div_32(0, &ra, &sa), lvl);
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
