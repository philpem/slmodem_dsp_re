/*
 * fpm_atan.c -- Fixed Point Modem: four-quadrant arctangent.
 *
 * Reconstructed from dsplibs.o fpm_atan.c, .text 0x0a6a50 (409 bytes), with
 * the table at .rodata 0x0c9a0 (257 signed 16-bit entries, 514 bytes).  The
 * translation unit holds this one function and nothing else -- see
 * docs/attribution.md, which brackets `fpm_atan.c` as 0x0a6a50 - 0x0a6a50.
 *
 * THE RESULT IS DELIVERED THROUGH A POINTER, and the function is `void`.  A
 * hand-over described it as `short FPM_atan(...)`; it is not.  Every path
 * ends at the same three instructions -- load the third argument, store a
 * 16-bit value through it, return -- and `%eax` holds a different leftover on
 * each of them (the sign mask in the x == 0 case, `x` itself in the y == 0
 * case, the 0x145f product in the general case).  GCC 3.4.2 at -O2 sets
 * `%eax` on every path of a function that returns one.
 *
 * ---------------------------------------------------------------------------
 * Units
 *
 * The angle is in the same phase units as fpm_phasor.c: a full turn is
 * 0x8000, so 0x2000 is 90 degrees and 0x1000 is 45.  The whole circle
 * therefore fits a signed short with one unit to spare, which is why the
 * fourth-octant fold below is 0x7fff and not 0x8000.
 *
 * ---------------------------------------------------------------------------
 * Method
 *
 * Octant reduction.  Divide the smaller magnitude by the larger, so the ratio
 * is always in [0, 1], take the arctangent of that from a table, then fold the
 * eighth of a circle it names into the right one using the two signs and which
 * magnitude was larger.  The division is FPM_div's reciprocal lookup, not a
 * divide:
 *
 *     FPM_div(max, &recip, &shift);
 *     ratio = (unsigned short)(((recip * min) >> 15) << shift);
 *
 * `recip` is 2^30 / (max << shift), so the whole expression is 32768 * min /
 * max -- the ratio in Q15.
 *
 * ---------------------------------------------------------------------------
 * Table derivation, recovered and checked against every entry:
 *
 *     FPM_atan_table[i] = round(32768 * atan(i / 256))      i in [0, 257)
 *
 * ROUNDED, not truncated -- and that is worth stating, because it is the
 * opposite of every other table in this layer.  fpm_phasor.c's sine and cosine
 * truncate, and so does fpm_div.c's reciprocal; truncation here misses 147 of
 * the 257 entries, rounding misses none.
 *
 * So the table holds atan in Q15 RADIANS, and 0x145f converts that to the
 * phase unit: 32768 / (2 * pi) = 5215.19, truncated to 5215 = 0x145f.  The
 * +0x4000 before the >> 15 rounds the product.
 *
 * The index is `ratio >> 7`, so entry i covers ratios 128*i .. 128*i+127 and
 * the last entry is reached only by a ratio of 32768 or more.  257 entries is
 * exactly enough and not one more: the largest ratio the expression above can
 * produce is 32894 (at max == min == 16447, where FPM_div's bucketing rounds
 * the mantissa down and the reciprocal comes out a shade large), and
 * 32894 >> 7 is 256.  Verified by sweeping all 32768 values of `max` with
 * `min` equal to it, which is where the ratio is maximal because it is
 * monotonic in `min`.  There is no out-of-range read here -- unlike FPM_sqrt
 * (D1) and FPM_div (D4), whose tables are both one entry short.
 *
 * ---------------------------------------------------------------------------
 * Two faithful oddities.  Both are reproduced, neither is repaired.
 *
 * 1. THE LINEAR SHORTCUT IS OFF BY ONE.  For a ratio of 0x7e or less the code
 *    uses the ratio itself as the Q15 radian value, which is right because
 *    atan(r) ~= r there.  But entry 0 of the table covers ratios 0..127 and
 *    holds 0, so a ratio of exactly 127 -- one past the shortcut -- takes the
 *    table path and yields an angle of 0 where 126 yields 20 and 128 yields
 *    20 again.  A single-point notch to zero.  Had the compare been 0x7f the
 *    shortcut would have covered the whole of entry 0's range and there would
 *    be no discontinuity, so this reads as an off-by-one rather than a design.
 *
 * 2. THE WRAP-THROUGH-ZERO OCTANT IS ONE UNIT LOW.  x > 0, y < 0, |x| > |y|
 *    is the only octant whose angle would need 0x8000 as its base, and 0x8000
 *    does not fit a signed short.  The original uses 0x7fff, so every angle in
 *    that eighth of the circle is one unit less than the other seven octants'
 *    convention would give.  The other three subtracting folds (0x2000 -,
 *    0x4000 -, 0x6000 -) are exact.
 */

#include "dsplib/fpm.h"

/*
 * .rodata 0x0c9a0.  Global in the original (`nm` shows `R FPM_atan_table`,
 * not `r`), and the relocation at 0x0a6b1e names the symbol rather than the
 * section, so it is not a file-static.  Kept global here for the same reason,
 * which also lets the unit test compare it word for word against the blob's.
 */
const short FPM_atan_table[FPM_ATAN_TABLE] = {
	0, 128, 256, 384, 512, 640, 768, 896,
	1024, 1152, 1279, 1407, 1535, 1663, 1790, 1918,
	2045, 2173, 2300, 2428, 2555, 2682, 2809, 2936,
	3063, 3190, 3317, 3443, 3570, 3696, 3823, 3949,
	4075, 4201, 4327, 4452, 4578, 4703, 4829, 4954,
	5079, 5204, 5329, 5453, 5578, 5702, 5826, 5950,
	6073, 6197, 6320, 6444, 6567, 6689, 6812, 6935,
	7057, 7179, 7301, 7422, 7544, 7665, 7786, 7907,
	8027, 8148, 8268, 8388, 8508, 8627, 8746, 8865,
	8984, 9102, 9221, 9339, 9456, 9574, 9691, 9808,
	9925, 10041, 10158, 10274, 10389, 10505, 10620, 10735,
	10849, 10964, 11078, 11192, 11305, 11418, 11531, 11644,
	11756, 11868, 11980, 12092, 12203, 12314, 12424, 12535,
	12645, 12754, 12864, 12973, 13082, 13190, 13298, 13406,
	13514, 13621, 13728, 13835, 13941, 14047, 14153, 14258,
	14363, 14468, 14573, 14677, 14781, 14884, 14987, 15090,
	15193, 15295, 15397, 15499, 15600, 15701, 15801, 15902,
	16002, 16101, 16201, 16300, 16398, 16497, 16595, 16693,
	16790, 16887, 16984, 17080, 17176, 17272, 17368, 17463,
	17557, 17652, 17746, 17840, 17933, 18027, 18119, 18212,
	18304, 18396, 18488, 18579, 18670, 18760, 18851, 18941,
	19030, 19120, 19209, 19297, 19386, 19474, 19561, 19649,
	19736, 19823, 19909, 19995, 20081, 20166, 20252, 20336,
	20421, 20505, 20589, 20673, 20756, 20839, 20922, 21004,
	21086, 21168, 21249, 21331, 21411, 21492, 21572, 21652,
	21732, 21811, 21890, 21969, 22047, 22126, 22203, 22281,
	22358, 22435, 22512, 22588, 22664, 22740, 22815, 22891,
	22966, 23040, 23115, 23189, 23262, 23336, 23409, 23482,
	23555, 23627, 23699, 23771, 23842, 23914, 23985, 24055,
	24126, 24196, 24266, 24335, 24405, 24474, 24542, 24611,
	24679, 24747, 24815, 24882, 24950, 25017, 25083, 25150,
	25216, 25282, 25347, 25413, 25478, 25543, 25607, 25672,
	25736,
};

void
FPM_atan(short y, short x, short *angle)
{
	unsigned short recip, shift;
	unsigned short ax, ay, ratio;
	int a;

	/*
	 * The two degenerate axes, each returning before the division that
	 * would divide by zero.  Straight up or straight down when x is zero;
	 * along the positive or negative x axis when y is zero.
	 */
	if (x == 0) {
		*angle = (short)(0x2000 + (y < 0 ? 0x4000 : 0));
		return;
	}
	if (y == 0) {
		*angle = (short)(x < 0 ? 0x4000 : 0);
		return;
	}

	/*
	 * Magnitudes, as UNSIGNED 16-bit.  The original takes the absolute
	 * value in 32 bits and then truncates, so |-32768| is 32768 and not
	 * the -32768 a 16-bit negate would give.  Every comparison on them
	 * below is unsigned for the same reason.
	 */
	ay = (unsigned short)(y < 0 ? -y : y);
	ax = (unsigned short)(x < 0 ? -x : x);

	/* min / max in Q15, so the table is only ever asked about [0, 1]. */
	if (ay < ax) {
		FPM_div(ax, &recip, &shift);
		ratio = (unsigned short)((((int)recip * ay) >> 15) << shift);
	} else {
		FPM_div(ay, &recip, &shift);
		ratio = (unsigned short)((((int)recip * ax) >> 15) << shift);
	}

	/* Q15 radians: the small-angle shortcut, else the table.  See note 1. */
	a = (ratio <= 0x7e) ? (int)ratio : FPM_atan_table[ratio >> 7];

	/* Q15 radians -> phase units, rounded. */
	a = (a * 0x145f + 0x4000) >> 15;

	/*
	 * Fold into the octant.  `ax > ay` means the vector lies within 45
	 * degrees of the x axis, so `a` counts up from that axis; otherwise it
	 * counts up from the y axis and the fold subtracts.
	 *
	 * The 0x7fff in the x > 0, y < 0 case is the original's; see note 2.
	 */
	if (ax > ay) {
		if (x < 0)
			*angle = (short)(y < 0 ? 0x4000 + a : 0x4000 - a);
		else
			*angle = (short)(y < 0 ? 0x7fff - a : a);
	} else {
		if (y < 0)
			*angle = (short)(x < 0 ? 0x6000 - a : 0x6000 + a);
		else
			*angle = (short)(x < 0 ? 0x2000 + a : 0x2000 - a);
	}
}
