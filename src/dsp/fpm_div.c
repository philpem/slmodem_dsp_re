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
 * The bug build preserves that direct neighbour read with the original
 * 128-word table. The fixed build retains its generated 129th entry (16384).
 * Period object and executable layouts are checked against the original.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm.h"

/* Declared reciprocal coefficients, excluding the neighbouring XOR word. */
#define FPM_DIV_TABLE_REAL 128


unsigned short
FPM_div_table_entry(int i)
{
	return (i >= 0 && i <= FPM_DIV_TABLE_REAL) ? FPM_div_table[i] : 0;
}

/*
 * A pointed count word reproduces the original normalizer's loop.  GCC3's
 * loop pass promotes that word and publishes it once after the backedge,
 * retaining the separate next-value pseudo (F11815).  A private int counter
 * with one terminal output assignment does not emit the same code.
 * Both output words belong to the caller; the zero guard precedes this helper.
 */
static inline void
normalize16(unsigned short denom, unsigned short *mantissa, unsigned short *count)
{
	*count = 0;
	if ((short)denom >= 0) {
		do {
			denom += denom;
			(*count)++;
		} while ((short)denom >= 0);
	}
	*mantissa = denom;
}

int
FPM_div(unsigned short denom, unsigned short *recip, unsigned short *shift)
{
	unsigned short mantissa;
	unsigned short count;
	unsigned short index;

	if (denom == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Fatal error: Division by zero!\n");
		return 1;
	}

	normalize16(denom, &mantissa, &count);

	index = (int)((mantissa + 0x80) >> 8) - 0x80;

	*recip = FPM_div_table[index];
	*shift = (unsigned short)count;
	return 0;
}
