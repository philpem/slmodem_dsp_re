/*
 * fpm_div.c -- Fixed Point Modem: reciprocal lookup for division.
 *
 * Reconstructed from dsplibs.o fpm_div.c, .text 0x0a6bf0, table at
 * .rodata 0x0c6a0.
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
 * 129 entries: 128 real ones plus the value the original reads past the end.
 * See the note above before "correcting" the last element.
 */
#define FPM_DIV_TABLE_REAL 128

static const unsigned short fpm_div_table[FPM_DIV_TABLE_REAL + 1] = {
	32768, 32513, 32263, 32017, 31775, 31536, 31300, 31068,
	30840, 30615, 30393, 30174, 29959, 29746, 29537, 29330,
	29127, 28926, 28728, 28532, 28339, 28149, 27962, 27776,
	27594, 27413, 27235, 27060, 26886, 26715, 26546, 26379,
	26214, 26051, 25890, 25731, 25575, 25420, 25266, 25115,
	24966, 24818, 24672, 24528, 24385, 24244, 24105, 23967,
	23831, 23696, 23563, 23431, 23301, 23172, 23045, 22919,
	22795, 22671, 22550, 22429, 22310, 22192, 22075, 21959,
	21845, 21732, 21620, 21509, 21399, 21290, 21183, 21076,
	20971, 20867, 20763, 20661, 20560, 20460, 20360, 20262,
	20164, 20068, 19972, 19878, 19784, 19691, 19599, 19508,
	19418, 19328, 19239, 19152, 19065, 18978, 18893, 18808,
	18724, 18641, 18558, 18477, 18396, 18315, 18236, 18157,
	18078, 18001, 17924, 17848, 17772, 17697, 17623, 17549,
	17476, 17403, 17331, 17260, 17189, 17119, 17050, 16980,
	16912, 16844, 16777, 16710, 16644, 16578, 16513, 16448,

	/*
	 * Index 128 -- one past the original's table.  See D4.
	 *
	 * The original reads FPM_xor_table[0] here, which is 0, so every
	 * denominator that normalises to a mantissa of 0xff80 or above gets a
	 * reciprocal of ZERO.  That is not theoretical: it silences an AGC
	 * block and drops a Bell 103 connection (finding 40).
	 *
	 * 16384 is the value the table's own generator produces:
	 *     trunc(2^30 / ((128 + 0x80) * 0x100)) = 2^30 / 65536 = 16384
	 */
#ifdef DSPLIB_REPRODUCE_BUGS
	0
#else
	16384
#endif
};

unsigned short
FPM_div_table_entry(int i)
{
	return (i >= 0 && i <= FPM_DIV_TABLE_REAL) ? fpm_div_table[i] : 0;
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

	*recip = fpm_div_table[index];
	*shift = (unsigned short)count;
	return 0;
}
