/*
 * t_fpm_atan.c -- differential test of the four-quadrant arctangent.
 *
 * FPM_atan is pure -- two shorts in, one short out through a pointer -- so
 * the only question is coverage.  The full domain is 2^32 pairs, which is too
 * many, so the sweeps below are chosen to hit every part of the machine
 * rather than to sample uniformly:
 *
 *   - both degenerate axes, exhaustively (x == 0 for all y, y == 0 for all x);
 *   - every pair inside a 401x401 box around the origin, which is where the
 *     linear shortcut lives and where the ratio is coarsest;
 *   - all 65536 values of one argument against a fixed set of the other,
 *     chosen to include the denominators FPM_div reads past its table for
 *     (511, 1023, 2047, 32767 -- see D4) and both signed extremes;
 *   - the 45 degree diagonals and their immediate neighbours, where the
 *     octant fold switches and where the 0x7fff asymmetry shows;
 *   - a ratio sweep that walks the table index across all 257 entries;
 *   - a million pseudorandom pairs from a fixed LCG.
 *
 * And the table itself is compared against the blob's copy word for word --
 * FPM_atan_table is a global symbol, so the harness aliases it as
 * ref_FPM_atan_table -- and against its recovered closed form, so a future
 * regeneration at another scale cannot silently drift.
 *
 * THIS BINARY MUST BE BUILT WITH -DDSPLIB_REPRODUCE_BUGS.  FPM_atan divides
 * through FPM_div, whose D4 out-of-range table read is the one defect this
 * tree fixes, and about one denominator in 256 reaches it: 32767 does, and so
 * do 511, 1023 and 2047.  Without the define our FPM_div returns 16384 where
 * the blob returns 0, and this test would fail on inputs that are not wrong.
 */

#include <math.h>

#include "harness.h"
#include "dsplib/fpm.h"

#ifndef DSPLIB_REPRODUCE_BUGS
#error "t_fpm_atan must be built with -DDSPLIB_REPRODUCE_BUGS -- see the header"
#endif

extern void ref_FPM_atan(short y, short x, short *angle);
extern const short ref_FPM_atan_table[];

/*
 * One comparison.  Both output slots are pre-loaded with a sentinel, so a
 * reconstruction that failed to write at all would be caught rather than
 * reading as agreement.
 */
static void
check(int y, int x)
{
	short mine = (short)0x5a5a;
	short theirs = (short)0x5a5a;
	long tag = ((long)(y & 0xffff) << 16) | (long)(x & 0xffff);

	ref_FPM_atan((short)y, (short)x, &theirs);
	FPM_atan((short)y, (short)x, &mine);

	diff_eq_int("FPM_atan(y,x) with yx=0x%08lx", mine, theirs, tag);
}

/*
 * Values worth holding one argument at.  The four in the middle are the
 * denominators whose normalised mantissa lands at or above 0xff80, which is
 * FPM_div's out-of-range index; the rest are the extremes, the powers of two
 * that make the shift exact, and a couple of ordinary numbers.
 */
static const int fixed[] = {
	-32768, -32767, -16384, -2047, -1023, -511, -256, -255,
	-3, -1, 1, 3, 255, 256, 511, 1023,
	2047, 4096, 16384, 12345, 32766, 32767
};

int
main(void)
{
	unsigned lcg = 0x13579bdfu;
	int rc = 0;
	int i, j, k;

	/*
	 * The table, twice over: against the blob and against the closed form.
	 * Rounded, not truncated -- the opposite of the sine, cosine and
	 * reciprocal tables in this layer, so it is asserted rather than
	 * assumed.
	 */
	diff_begin("FPM_atan_table");
	for (i = 0; i < FPM_ATAN_TABLE; i++) {
		diff_eq_int("entry %ld matches the blob",
			    FPM_atan_table[i], ref_FPM_atan_table[i], i);
		diff_eq_int("entry %ld matches round(32768*atan(i/256))",
			    FPM_atan_table[i],
			    (short)(32768.0 * atan(i / 256.0) + 0.5), i);
	}
	rc |= diff_end();

	/*
	 * The units, pinned as absolute values rather than only against the
	 * blob, so the header's claim about them is testable.  A full turn is
	 * 0x8000 and the angle runs anticlockwise from the positive x axis.
	 */
	diff_begin("FPM_atan units and documented oddities");
	{
		short v;

		ref_FPM_atan(0, 1, &v);
		diff_eq_int("atan2(0, +x) is 0 (%ld)", v, 0x0000, 0);
		ref_FPM_atan(1, 0, &v);
		diff_eq_int("atan2(+y, 0) is 90 deg (%ld)", v, 0x2000, 0);
		ref_FPM_atan(0, -1, &v);
		diff_eq_int("atan2(0, -x) is 180 deg (%ld)", v, 0x4000, 0);
		ref_FPM_atan(-1, 0, &v);
		diff_eq_int("atan2(-y, 0) is 270 deg (%ld)", v, 0x6000, 0);
		ref_FPM_atan(0, 0, &v);
		diff_eq_int("atan2(0, 0) falls out as 90 deg (%ld)",
			    v, 0x2000, 0);
		ref_FPM_atan(1, 1, &v);
		diff_eq_int("atan2(1, 1) is 45 deg (%ld)", v, 0x1000, 0);

		/*
		 * The wrap-through-zero octant is one unit low: the base is
		 * 0x7fff, not 0x8000, because 0x8000 is not a positive short.
		 * A vector just below the positive x axis therefore reads
		 * 0x7fff and not 0.
		 */
		ref_FPM_atan(-1, 32767, &v);
		diff_eq_int("x>0, y<0, |x|>|y| bases on 0x7fff (%ld)",
			    v, 0x7fff, 0);
	}
	rc |= diff_end();

	/* Both degenerate axes, exhaustively -- these never reach FPM_div. */
	diff_begin("FPM_atan degenerate axes");
	for (i = -32768; i < 32768; i++) {
		check(i, 0);
		check(0, i);
	}
	rc |= diff_end();

	/*
	 * Every pair in a box around the origin.  Small magnitudes are where
	 * the ratio is coarsest and where the linear shortcut for ratios of
	 * 0x7e or less is reached, including its off-by-one at 127.
	 */
	diff_begin("FPM_atan near the origin");
	for (i = -200; i <= 200; i++)
		for (j = -200; j <= 200; j++)
			check(i, j);
	rc |= diff_end();

	/* All of one argument against a fixed set of the other, both ways. */
	diff_begin("FPM_atan full sweep of y");
	for (k = 0; k < (int)(sizeof(fixed) / sizeof(fixed[0])); k++)
		for (i = -32768; i < 32768; i++)
			check(i, fixed[k]);
	rc |= diff_end();

	diff_begin("FPM_atan full sweep of x");
	for (k = 0; k < (int)(sizeof(fixed) / sizeof(fixed[0])); k++)
		for (i = -32768; i < 32768; i++)
			check(fixed[k], i);
	rc |= diff_end();

	/*
	 * The diagonals and their neighbours.  |x| == |y| is the boundary the
	 * octant fold turns on, and it is taken as "y wins" -- so the pair
	 * either side of it lands in different octants with different bases,
	 * which is where a fold written the other way round would show.
	 */
	diff_begin("FPM_atan diagonals");
	for (i = 1; i < 32768; i++) {
		check(i, i);
		check(i, -i);
		check(-i, i);
		check(-i, -i);
		check(i, i - 1);
		check(i - 1, i);
	}
	rc |= diff_end();

	/*
	 * Walk the ratio across the whole table.
	 *
	 * THE OBVIOUS WAY TO DO THIS DOES NOT WORK, and the first version of
	 * this section was dead for exactly that reason.  Pinning the larger
	 * magnitude at 32767 looks like the widest sweep available and is in
	 * fact the narrowest: 32767 normalises to 0xfffe, whose FPM_div index
	 * is 128 -- the D4 slot, zero under -DDSPLIB_REPRODUCE_BUGS -- so
	 * `recip` is zero, the ratio is zero, and every pair takes the linear
	 * shortcut without ever reading the table.  98304 checks that compared
	 * nothing while reporting PASS.  511, 1023, 2047, 32766 and 32767 in
	 * `fixed` above are all this same denominator class; they are useful
	 * for covering D4 itself and are not table coverage.  Finding 1502.
	 *
	 * 16384 is the value that works.  It normalises to 0x8000 with a shift
	 * of one, so `recip` is exactly 32768 and the ratio is exactly twice
	 * the smaller magnitude: sweeping that over 0..16384 walks the index
	 * over 0..256 -- all 257 entries, the last one included.
	 *
	 * Even ratios are all that gives, though.  An ODD ratio needs a shift
	 * of zero, which needs the larger magnitude to be normalised already,
	 * and magnitudes cap at 32768 -- so it needs an argument of exactly
	 * -32768, where the ratio is the smaller magnitude itself.  That is
	 * the second loop, and it is the only way to reach the ratio of 127
	 * that finding 1501 is about.
	 */
	diff_begin("FPM_atan table index sweep");
	for (i = 0; i <= 16384; i++) {
		check(i, 16384);
		check(16384, i);
		check(-i, 16384);
		check(16384, -i);
	}
	for (i = -32768; i < 32768; i++) {
		check(-32768, i);
		check(i, -32768);
	}
	rc |= diff_end();

	/* And a million pairs from a fixed LCG, for everything not thought of. */
	diff_begin("FPM_atan pseudorandom");
	for (i = 0; i < 1000000; i++) {
		int yy, xx;

		lcg = lcg * 1103515245u + 12345u;
		yy = (int)((lcg >> 16) & 0xffffu) - 32768;
		lcg = lcg * 1103515245u + 12345u;
		xx = (int)((lcg >> 16) & 0xffffu) - 32768;
		check(yy, xx);
	}
	rc |= diff_end();

	return rc;
}
