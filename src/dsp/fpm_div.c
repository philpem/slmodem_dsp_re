/*
 * fpm_div.c -- Fixed Point Modem: reciprocal lookup for division.
 *
 * Reconstructed from dsplibs.o fpm_div.c:
 *
 *   FPM_div     .text 0x0a6bf0   150 bytes
 *   FPM_div_32  .text 0x0a6c90   147 bytes
 *
 * with the table both of them read at .rodata 0x0c6a0.
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

const unsigned short FPM_div_table[FPM_DIV_TABLE_REAL + 1] = {
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
	 * Index 128 -- one past the original's 128 entries.  See D4.
	 *
	 * In the BUG build this is 0, because the original reads
	 * `FPM_xor_table[0]` here, which is 0: every denominator that
	 * normalises to a mantissa of 0xff80 or above gets a reciprocal of
	 * ZERO, which silences an AGC block and drops a Bell 103 connection
	 * (finding F40).  In the FIXED build it is 16384, the value the
	 * table's own generator produces:
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

/*
 * The 32-bit denominator.  Same contract, same table, same D4 overrun: the
 * mantissa is the top 16 bits of the normalised word, so it lies in
 * [0x8000, 0xffff] exactly as FPM_div's does and the index runs 0..128.
 *
 * The two differences from FPM_div are both in the normalisation: the shift
 * count can reach 31, and the loop tests the whole 32-bit word rather than a
 * 16-bit one, so a denominator whose top bit is already set is returned with
 * a shift of zero without the loop running at all.
 */
int
FPM_div_32(unsigned int denom, unsigned short *recip, unsigned short *shift)
{
	unsigned short count = 0;
	unsigned short mantissa;
	int index;

	if (denom == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Fatal error: Division by zero!\n");
		return 1;
	}

	/* Left-normalise until the top bit is set, counting the shifts. */
	while ((int)denom >= 0) {
		denom += denom;
		count++;
	}

	mantissa = (unsigned short)(denom >> 16);
	index = (int)((mantissa + 0x80) >> 8) - 0x80;

	*recip = FPM_div_table[index];
	*shift = count;
	return 0;
}
