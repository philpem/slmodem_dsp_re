/* Fixed-point square root, reconstructed from FPM_sqrt (0xa9d70).
 * The original table has 192 words. Index192 reads FPM_div_table[0]; larger
 * unsigned-short inputs continue into the following division and XOR tables.
 * No clamp precedes the original load at 0xa9de2. The period link and full-word
 * fixture check this direct lookup and its original out-of-symbol reads.
 * Entry i is trunc(32768 * sqrt((i + 64) / 256.0)).
 */

#include <math.h>

#include "dsplib/fpm.h"

/*
 * Original192-word symbol extent; neighbour reads do not enlarge it.
 */
#define FPM_SQRT_TABLE_SIZE 192

/*
 * The table itself is GLOBAL in the object and now lives in `fpm_tables.c`,
 * declared in `dsplib/fpm.h`; this file keeps only the accessors over it.
 */

unsigned short
FPM_sqrt(unsigned short x)
{
	unsigned exponent = 0;
	unsigned mantissa;
	int index;

	if (x == 0)
		return 0;

	/*
	 * Left-normalise so the mantissa's top bit lands in bit 14 or 15.
	 * Inputs already at or above 0x4000 need no shifting.
	 */
	mantissa = x;
	while (mantissa <= 0x3fff) {
		mantissa = (mantissa << 1) & 0xffff;
		exponent++;
	}

	/*
	 * Halving an odd exponent would leave a stray factor of sqrt(2).
	 * Shift the mantissa down instead and let the table cover the wider
	 * input range -- which is why the table spans a factor of four in its
	 * argument but only two in its result.
	 */
	if (exponent & 1)
		mantissa >>= 1;

	index = (int)((mantissa + 64) >> 7) - 64;


	return (unsigned short)(FPM_sqrt_table[index] >> (exponent >> 1));
}

/*
 * 32-bit square root, sharing the same table.
 *
 * Structurally identical to FPM_sqrt: left-normalise, halve the exponent,
 * index the quarter-scale table, shift the result down.  It normalises until
 * the value exceeds 0x1fffffff and takes bits 15 upward as the mantissa.
 *
 * The interesting difference is that **this one bounds-checks**:
 *
 *     if ((unsigned short)index > 0xbf) index = 0xbf;
 *
 * clamping to entry 191, the last of the original's 192.  FPM_sqrt, indexing
 * the same table from the same author, does not -- it reads entry 192 and
 * lands in FPM_div_table (deviation D1).  So the bound was known; the 16-bit
 * version simply missed it.
 *
 * Note the clamp is *unsigned*, so a negative index wraps to a large unsigned
 * value and also clamps to 191 rather than to 0.  That is reachable: an input
 * at or above 0x80000000 gives `x >> 15 >= 0x10000`, which truncates to a
 * small or zero mantissa and drives the index negative.
 *
 * That truncation is reproduced rather than corrected. FPM_rms passes an
 * unsigned accumulator that can cross 0x80000000; retain the original clamp.
 */
unsigned short
FPM_sqrt_dp(unsigned int x)
{
	unsigned exponent = 0;
	unsigned mantissa;
	int index;

	if (x == 0)
		return 0;

	while (x <= 0x1fffffffu) {
		x += x;
		exponent++;
	}

	/* Truncated to 16 bits, which is where large inputs lose their top bit. */
	mantissa = (unsigned short)(x >> 15);

	if (exponent & 1)
		mantissa >>= 1;

	index = (int)((mantissa + 64) >> 7) - 64;
	if ((unsigned short)index > 0xbf)
		index = 0xbf;

	return (unsigned short)(FPM_sqrt_table[index] >> (exponent >> 1));
}

/*
 * Regenerate one table entry from the design parameters.  Kept alongside the
 * table so the derivation stays checkable: the unit test regenerates all 192
 * entries and compares against the extracted originals.
 */
unsigned short
FPM_sqrt_table_generate(int index)
{
	/* Deliberately double precision: the original was generated offline. */
	double frac = (index + 64) / 256.0;
	return (unsigned short)(32768.0 * sqrt(frac));
}

unsigned short
FPM_sqrt_table_entry(int index)
{
	return (index >= 0 && index < FPM_SQRT_TABLE_SIZE)
		? FPM_sqrt_table[index] : 0;
}

int
FPM_sqrt_table_size(void)
{
	return FPM_SQRT_TABLE_SIZE;
}
