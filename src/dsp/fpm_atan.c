/*
 * fpm_atan.c -- Fixed Point Modem: four-quadrant arctangent.
 *
 * Reconstructed from dsplibs.o fpm_atan.c:
 *   FPM_atan        .text   0x0a6a50, 409 bytes
 *   FPM_atan_table  .rodata 0x00c9a0, 514 bytes
 *
 * ANGLE UNITS.  A full turn is 0x8000, so the result is a signed 16-bit
 * "binary radian": 0x2000 is +90 degrees, 0x4000 is 180, 0x6000 is 270.  The
 * result is written through a pointer; the function returns nothing.
 *
 * ARGUMENT ORDER is (y, x), like atan2 and unlike the axis order of most of
 * this library.  It is settled by the two degenerate cases, which need no
 * table at all:
 *
 *   x == 0  ->  0x2000 or 0x6000, chosen by the sign of y   (+-90 degrees)
 *   y == 0  ->  0      or 0x4000, chosen by the sign of x   (0 or 180)
 *
 * and by V22_SRE_recover's call, which passes its cosine-correlated
 * accumulator second.
 *
 * HOW IT WORKS.  Both arguments are reduced to magnitudes, the smaller is
 * divided by the larger through FPM_div's reciprocal lookup, and the
 * resulting Q15 ratio -- always in [0, 1] -- indexes a 257-entry table of
 * atan in radians at Q15.  One multiply by 0x145f converts radians to this
 * function's units: 0x8000 / (2 * pi) = 5215.19, and 0x145f is 5215.
 *
 * Below a ratio of 127 the table is skipped and the ratio is used as the
 * angle directly, which is the small-angle approximation atan(r) ~ r -- and
 * is also exactly what the table's first two entries would give.
 *
 * THREE DETAILS THAT ARE NOT COSMETIC:
 *
 *   - the ratio is masked to 16 bits before it is compared and before it is
 *     shifted down to a table index, so a shift that overflows wraps rather
 *     than saturating.
 *   - the reflection for the fourth quadrant is `0x7fff - t`, NOT
 *     `0x8000 - t`.  A y just below zero with a large positive x therefore
 *     reports 0x7fff rather than 0.  That is the original's arithmetic and it
 *     is reproduced; see docs/deviations.md.
 *   - the three other reflections are exact: 0x4000 - t, 0x2000 - t and
 *     0x6000 - t.
 */

#include "dsplib/fpm.h"

/*
 * atan(i / 256) in radians at Q15, rounded.  257 entries, so a ratio of
 * exactly 1.0 (index 256) is in range: 25736 is round(pi/4 * 32768).
 *
 * Reference bytes, extracted by tools/tabdump.py.  NOT static: the original
 * places it in .rodata as a global.
 */
const short FPM_atan_table[FPM_ATAN_TABLE_LEN] = {
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

/* Radians at Q15 to a 0x8000-per-turn angle: round(0x8000 / (2 * pi)). */
#define FPM_ATAN_SCALE 0x145f

void
FPM_atan(short y, short x, short *angle)
{
	unsigned short recip, shift;
	unsigned short ay, ax;
	int ratio;
	int t;

	if (x == 0) {
		*angle = (short)(0x2000 + (y < 0 ? 0x4000 : 0));
		return;
	}

	if (y == 0) {
		*angle = (short)(x < 0 ? 0x4000 : 0);
		return;
	}

	/*
	 * Magnitudes, truncated to 16 bits.  -32768 becomes 32768, which is
	 * why these are unsigned.
	 */
	ay = (unsigned short)(y < 0 ? -(int)y : (int)y);
	ax = (unsigned short)(x < 0 ? -(int)x : (int)x);

	/* Always the smaller over the larger, so the ratio stays inside Q15. */
	if (ay >= ax) {
		FPM_div(ay, &recip, &shift);
		ratio = (int)recip * (int)ax;
	} else {
		FPM_div(ax, &recip, &shift);
		ratio = (int)recip * (int)ay;
	}

	ratio = (ratio >> 15) << shift;
	ratio &= 0xffff;

	/* Below 127, atan(r) == r to the last bit this representation has. */
	if (ratio > 0x7e)
		t = FPM_atan_table[ratio >> 7];
	else
		t = (short)ratio;

	t = (t * FPM_ATAN_SCALE + 0x4000) >> 15;

	if (ax > ay) {
		/* Nearer the x axis: t is measured from it. */
		if (x < 0)
			*angle = (short)(y < 0 ? t + 0x4000 : 0x4000 - t);
		else
			*angle = (short)(y < 0 ? 0x7fff - t : t);
	} else {
		/* Nearer the y axis: t is measured from +-90 degrees. */
		if (y < 0)
			*angle = (short)(x < 0 ? 0x6000 - t : t + 0x6000);
		else
			*angle = (short)(x < 0 ? t + 0x2000 : 0x2000 - t);
	}
}
