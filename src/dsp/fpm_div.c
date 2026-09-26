/*
 * fpm_div.c -- Fixed Point Modem: reciprocal lookup for division.
 *
 * Reconstructed from dsplibs.o fpm_div.c:
 *
 *   FPM_div     .text 0x0a6bf0   150 bytes
 *
 * with the table it reads at .rodata 0x0c6a0.  `FPM_div_32` and
 * `FPM_circ_dotp2` were once grouped here; the FILE order splits them into
 * `fpm_div32.c` (see that file's banner).
 *
 * Rather than divide, callers normalise the denominator and look up its
 * reciprocal, then multiply.  FPM_div does the normalisation and the lookup:
 *
 *     FPM_div(denom, &recip, &shift)
 *
 * leaving `recip` as an approximate 1/denom and `shift` as the number of left
 * shifts normalisation needed, so the caller can correct the exponent.
 * Returns 0 on success, 1 if `denom` is zero.
 *
 * FPM_div_32 is the same routine over a 32-bit denominator: it normalises the
 * whole word, takes the top 16 bits as the mantissa, and indexes the SAME
 * table -- so `shift` counts up to 31 rather than 15 and everything else,
 * including the out-of-range read below, is identical.
 *
 * Table derivation, exact for all 128 entries:
 *
 *     table[i] = trunc(2^30 / ((i + 0x80) * 0x100))
 *
 * Truncated, like the sine tables -- rounding differs on 58 of the 128.
 *
 * ---------------------------------------------------------------------------
 * A reachable out-of-range read, reproduced deliberately.  See D4.
 *
 * The index is ((mantissa + 0x80) >> 8) - 0x80 and the mantissa after
 * normalisation lies in [0x8000, 0xffff], so the index runs 0..**128** -- one
 * past the original's 128-entry table.  It is reached whenever the normalised
 * denominator is 0xff80 or above, which is 255 of the 65535 possible
 * denominators (0.39%), including values as ordinary as 511, 1023 and 2047.
 *
 * This is the same defect as FPM_sqrt's (D1), but without the luck: there, the
 * adjacent word happened to hold exactly the right value. Here the read lands
 * on FPM_xor_table[0], which is **0**, where the correct entry would be 16384.
 * So for those denominators the original returns a reciprocal of zero, and any
 * division built on it collapses to zero.
 *
 * The reconstruction reproduces that, because the project's rule is to guard
 * only where the contract makes an input impossible, and nothing rules these
 * denominators out.  The 129th entry below is 0 for exactly that reason -- it
 * is not a real coefficient, it is the neighbouring table's first word.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm.h"

/*
 * 129 entries: the object's 128 real ones plus a 129th that carries the value
 * the D4 over-read finds.  In the bug-reproducing build it is 0 -- what the
 * object's neighbouring `FPM_xor_table[0]` holds -- so the differential sees
 * the object's own behaviour; the fixed build puts the generated 16384 there
 * so the read is corrected.  The entry cannot simply be dropped in the bug
 * build: our `.rodata` does not reproduce the object's FPM_xor_table
 * adjacency, and 255 denominators then read a nonzero word instead of 0.
 */
#define FPM_DIV_TABLE_REAL 128


unsigned short
FPM_div_table_entry(int i)
{
	return (i >= 0 && i <= FPM_DIV_TABLE_REAL) ? FPM_div_table[i] : 0;
}

unsigned short
FPM_div_table_generate(int i)
{
	return (unsigned short)(0x40000000 / ((unsigned)(i + 0x80) * 0x100));
}

int
FPM_div(unsigned short denom, unsigned short *recip, unsigned short *shift)
{
	unsigned mantissa = denom;
	unsigned count = 0;
	int index;

	if (denom == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Fatal error: Division by zero!\n");
		return 1;
	}

	/* Left-normalise until the top bit is set, counting the shifts. */
	while ((short)mantissa >= 0) {
		mantissa = (mantissa + mantissa) & 0xffff;
		count++;
	}

	index = (int)((mantissa + 0x80) >> 8) - 0x80;

	*recip = FPM_div_table[index];
	*shift = (unsigned short)count;
	return 0;
}
