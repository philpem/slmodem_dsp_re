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

	diff_begin("FPM_sqrt/Q15");
	for (i = 0; i <= 0x7fff; i++)
		diff_eq_int("FPM_sqrt(0x%04lx)", FPM_sqrt((unsigned short)i),
			    ref_FPM_sqrt((unsigned short)i), i);
	rc |= diff_end();

	return rc;
}
