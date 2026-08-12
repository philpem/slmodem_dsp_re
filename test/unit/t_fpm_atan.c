/*
 * t_fpm_atan.c -- differential test of the four-quadrant arctangent.
 *
 * FPM_atan is a pure function of two shorts, so this is one of the few places
 * in the tree where near-exhaustive coverage is affordable.  What it has to
 * catch:
 *
 *   - the argument order.  (y, x) and (x, y) agree on the diagonal and on
 *     both axes and disagree everywhere else, so a sweep that only walked the
 *     axes would pass with them swapped.
 *   - the eight reflections.  Four quadrants times "nearer the x axis" or
 *     "nearer the y axis", each with its own constant; getting one wrong is a
 *     45-degree wedge of wrong answers and 7/8 of the plane still right.
 *   - the 0x7fff, which differs from the 0x8000 a correct implementation
 *     would use by exactly one count and only in one wedge.
 *   - the table/no-table split at a ratio of 127, and the 16-bit mask on the
 *     ratio before the index is taken.
 *
 * The guards at the end require every one of those regions to have been
 * visited, on the input side only -- they are computed from x and y and not
 * from anything the function under test decides.
 */

#include "harness.h"
#include "dsplib/fpm.h"

extern void ref_FPM_atan(short y, short x, short *angle);
extern const short ref_FPM_atan_table[];

/* One counter per region the implementation branches on. */
static long seen_x_zero;
static long seen_y_zero;
static long seen_octant[8];
static long seen_table;
static long seen_small;

static int
iabs(int v)
{
	return v < 0 ? -v : v;
}

static void
note(short y, short x)
{
	int ay = iabs((int)y);
	int ax = iabs((int)x);
	int oct;

	if (x == 0) {
		seen_x_zero++;
		return;
	}
	if (y == 0) {
		seen_y_zero++;
		return;
	}

	oct = (ax > ay ? 4 : 0) | (x < 0 ? 2 : 0) | (y < 0 ? 1 : 0);
	seen_octant[oct]++;

	/*
	 * The table is consulted when the Q15 ratio exceeds 126, which to
	 * within the reciprocal's rounding is min * 256 > max.  Both sides of
	 * that split have to be exercised; the exact boundary is the
	 * function's business, not the guard's.
	 */
	if ((ay < ax ? ay : ax) * 256 > (ay < ax ? ax : ay) * 2)
		seen_table++;
	else
		seen_small++;
}

static void
one(short y, short x, long tag)
{
	short a = (short)0x5ead, b = (short)0x5ead;

	ref_FPM_atan(y, x, &a);
	FPM_atan(y, x, &b);
	diff_eq_int("atan(y,x) at %ld", b, a, tag);
	note(y, x);
}

int
main(void)
{
	static const short edge[] = {
		-32768, -32767, -32766, -30000, -16384, -16383, -1000, -257,
		-256, -255, -129, -128, -127, -126, -2, -1, 0, 1, 2, 126,
		127, 128, 129, 255, 256, 257, 1000, 16383, 16384, 30000,
		32766, 32767
	};
	const int nedge = (int)(sizeof edge / sizeof edge[0]);
	unsigned lfsr = 0x2A17u;
	int rc = 0;
	int i, j;
	long tag = 0;

	/* --- the table ------------------------------------------------- */

	diff_begin("fpm atan table");
	for (i = 0; i < FPM_ATAN_TABLE_LEN; i++)
		diff_eq_int("FPM_atan_table[%ld]", FPM_atan_table[i],
			    ref_FPM_atan_table[i], i);
	rc |= diff_end();

	/* --- every pair of interesting magnitudes ---------------------- */

	diff_begin("fpm atan edge grid");
	for (i = 0; i < nedge; i++)
		for (j = 0; j < nedge; j++)
			one(edge[i], edge[j], tag++);
	rc |= diff_end();

	/*
	 * Exhaustive near the origin, where the reciprocal is coarsest and
	 * where the ratio's 16-bit mask is most likely to bite.
	 */
	diff_begin("fpm atan near origin, exhaustive");
	for (i = -48; i <= 48; i++)
		for (j = -48; j <= 48; j++)
			one((short)i, (short)j, tag++);
	rc |= diff_end();

	/*
	 * A coarse sweep of the whole plane.  The stride is prime to 65536 so
	 * the grid does not sit on any power-of-two boundary.
	 */
	diff_begin("fpm atan full plane, strided");
	for (i = -32768; i <= 32767 - 419; i += 419)
		for (j = -32768; j <= 32767 - 419; j += 419)
			one((short)i, (short)j, tag++);
	rc |= diff_end();

	/*
	 * And a long pseudorandom run, because a regular grid can miss a
	 * single table entry for ever.
	 */
	diff_begin("fpm atan random pairs");
	for (i = 0; i < 200000; i++) {
		short y, x;

		lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xB400u);
		y = (short)(lfsr & 0xffffu);
		lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xB400u);
		x = (short)(lfsr & 0xffffu);
		one(y, x, tag++);
	}
	rc |= diff_end();

	/* --- the guards ------------------------------------------------ */

	diff_begin("fpm atan coverage");
	diff_eq_int("x == 0 reached (%ld)", seen_x_zero > 0, 1, 0);
	diff_eq_int("y == 0 reached (%ld)", seen_y_zero > 0, 1, 0);
	for (i = 0; i < 8; i++)
		diff_eq_int("octant %ld reached", seen_octant[i] > 0, 1, i);
	diff_eq_int("table path reached (%ld)", seen_table > 0, 1, 0);
	diff_eq_int("small-angle path reached (%ld)", seen_small > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
