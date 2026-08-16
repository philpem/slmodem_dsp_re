/*
 * t_v32vtb.c -- differential test of the Viterbi trellis decoder.
 *
 * FOUR THINGS ARE CHECKED, and the third is the one that matters.
 *
 * 1. The sixteen tables, entry by entry against the blob's own copies.  This
 *    proves the BYTES and nothing else: `const short[1856]` and
 *    `const int[928]` hold the same bytes and a byte comparison cannot tell
 *    them apart.
 *
 * 2. `VTBv32_init`, over every rate code including two that fall through to
 *    the 128-point branch.  The two sides cannot be compared as objects --
 *    each points at its own tables and its own survivor ring -- so the
 *    pointers are compared by the CONTENT of what they point at, which is
 *    finding 1614's rule and is what pins the SELECTION rather than the
 *    address.
 *
 * 3. `VTB_decoder` driven one symbol at a time from a state our `VTBv32_init`
 *    built against one the blob's built, comparing the emitted symbol, all
 *    eight metrics, the ring index, the differential state and all 128
 *    survivor nodes after every call.  This is where a wrong element width
 *    dies: our decoder indexes OUR tables with OUR declared stride, so a
 *    four-byte reading of `bound[]` reads different values from the same
 *    bytes and the first symbol disagrees.
 *
 * 4. THE READING THAT AGREES WITH THE TRUE ONE ALMOST EVERYWHERE.  For the
 *    two rotated constellations the decoder looks the region up in the
 *    ROTATED frame and computes the branch metric in the frame as received.
 *    Rotating both -- the obvious tidy reading -- gives the same answer over
 *    most of the plane.  So the test counts the trials whose region index
 *    actually DIFFERS between the two frames and asserts that count is not
 *    zero; a pass with a zero count would be a test that cannot see the
 *    difference (finding 3052's rule, and finding 134's).
 */

#include <string.h>

#include "harness.h"
#include "dsplib/vtb.h"

extern void ref_VTB_decoder(void *st, short i, short q, short *out);
extern void ref_VTBv32_init(void *st, short mode, int alloc);

extern const short ref_VTBv32_IMAP16T[16];
extern const short ref_VTBv32_QMAP16T[16];
extern const short ref_VTBv32_IMAP32[32];
extern const short ref_VTBv32_QMAP32[32];
extern const short ref_VTBv32_IMAP64[64];
extern const short ref_VTBv32_QMAP64[64];
extern const short ref_VTBv32_IMAP128[128];
extern const short ref_VTBv32_QMAP128[128];
extern const short ref_VTB_REGION_7200[8];
extern const short ref_VTB_REGION_9600[32];
extern const short ref_VTB_REGION_12000[72];
extern const short ref_VTB_REGION_14400[128];
extern const short ref_VTB_BOUND_7200[128];
extern const short ref_VTB_BOUND_9600[416];
extern const short ref_VTB_BOUND_12000[960];
extern const short ref_VTB_BOUND_14400[1856];

static unsigned seed;

static int
rnd(int range)
{
	seed = seed * 1103515245u + 12345u;
	return (int)((seed >> 13) % (unsigned)range);
}

static void
cmp_table(const char *what, const short *ours, const short *theirs, int n)
{
	int k;

	for (k = 0; k < n; k++)
		diff_eq_int_(__FILE__, __LINE__, what, (long)ours[k],
			     (long)theirs[k], (long)k);
}

#define CMP_TAB(name, n) cmp_table(#name "[%ld]", name, ref_##name, (n))

static int
run_tables(void)
{
	diff_begin("VTB tables: byte for byte against the blob");

	CMP_TAB(VTBv32_IMAP16T, 16);
	CMP_TAB(VTBv32_QMAP16T, 16);
	CMP_TAB(VTBv32_IMAP32, 32);
	CMP_TAB(VTBv32_QMAP32, 32);
	CMP_TAB(VTBv32_IMAP64, 64);
	CMP_TAB(VTBv32_QMAP64, 64);
	CMP_TAB(VTBv32_IMAP128, 128);
	CMP_TAB(VTBv32_QMAP128, 128);

	CMP_TAB(VTB_REGION_7200, 8);
	CMP_TAB(VTB_REGION_9600, 32);
	CMP_TAB(VTB_REGION_12000, 72);
	CMP_TAB(VTB_REGION_14400, 128);

	CMP_TAB(VTB_BOUND_7200, 128);
	CMP_TAB(VTB_BOUND_9600, 416);
	CMP_TAB(VTB_BOUND_12000, 960);
	CMP_TAB(VTB_BOUND_14400, 1856);

	return diff_end();
}

/*
 * THE LENGTHS ARE THE WIDTH ARGUMENT, so they are asserted rather than left
 * in a comment.  `region[]` is indexed `ci + grid*(cq + (cq>ci ? grid : 0))`
 * with both coordinates clamped to `grid - 1`, so the largest index is
 * 2*grid*grid - 1; the decoder then reads `bound[region[i] + off + 0..7]`
 * with `off` at most 24.  Under a two-byte element both bounds are exactly
 * met, at every one of the four rates.  Under a four-byte element each table
 * would hold half as many entries and every one of these would overrun.
 */
static int
run_lengths(void)
{
	static const int grid[4] = { 2, 4, 6, 8 };
	static const int nregion[4] = { 8, 32, 72, 128 };
	static const int nbound[4] = { 128, 416, 960, 1856 };
	const short *region[4];
	int r, k, max;

	region[0] = VTB_REGION_7200;
	region[1] = VTB_REGION_9600;
	region[2] = VTB_REGION_12000;
	region[3] = VTB_REGION_14400;

	diff_begin("VTB tables: the lengths the index arithmetic needs");
	for (r = 0; r < 4; r++) {
		diff_eq_int("region entries at grid %ld", nregion[r],
			    2 * grid[r] * grid[r], grid[r]);
		max = 0;
		for (k = 0; k < nregion[r]; k++) {
			diff_eq_int("region entry is a multiple of 32 (%ld)",
				    region[r][k] & 31, 0, k);
			if (region[r][k] > max)
				max = region[r][k];
		}
		diff_eq_int("bound entries at grid %ld", nbound[r],
			    max + 24 + 8, grid[r]);
	}
	return diff_end();
}

/* Layout, which the datapump object depends on: 0x38 between +0x18 and +0x50. */
static int
run_layout(void)
{
	struct vtb v;

	diff_begin("struct vtb: the layout VTB_decoder reads");
	diff_eq_int("metric (%ld)", (long)((char *)&v.metric - (char *)&v),
		    0x04, 0);
	diff_eq_int("ring (%ld)", (long)((char *)&v.ring - (char *)&v),
		    0x14, 0);
	diff_eq_int("imap (%ld)", (long)((char *)&v.imap - (char *)&v),
		    0x18, 0);
	diff_eq_int("qmap (%ld)", (long)((char *)&v.qmap - (char *)&v),
		    0x1c, 0);
	diff_eq_int("bound (%ld)", (long)((char *)&v.bound - (char *)&v),
		    0x20, 0);
	diff_eq_int("nsub (%ld)", (long)((char *)&v.nsub - (char *)&v),
		    0x24, 0);
	diff_eq_int("region (%ld)", (long)((char *)&v.region - (char *)&v),
		    0x28, 0);
	diff_eq_int("grid (%ld)", (long)((char *)&v.grid - (char *)&v),
		    0x2c, 0);
	diff_eq_int("depth (%ld)", (long)((char *)&v.depth - (char *)&v),
		    0x2e, 0);
	diff_eq_int("prev (%ld)", (long)((char *)&v.prev - (char *)&v),
		    0x30, 0);
	diff_eq_int("mask (%ld)", (long)((char *)&v.mask - (char *)&v),
		    0x32, 0);
	diff_eq_int("shift (%ld)", (long)((char *)&v.shift - (char *)&v),
		    0x34, 0);
	diff_eq_int("sizeof (%ld)", (long)sizeof(struct vtb), 0x38, 0);
	diff_eq_int("sizeof path (%ld)", (long)sizeof(struct vtb_path), 4, 0);
	return diff_end();
}

/* How many entries the constellation selected for a rate has. */
static int
points_for(int nsub)
{
	switch (nsub) {
	case 1:
		return 16;
	case 2:
		return 32;
	case 3:
		return 64;
	default:
		return 128;
	}
}

static void
cmp_init(struct vtb *a, struct vtb *b, int mode)
{
	int k, n;

	diff_eq_int("nsub after init(%ld)", a->nsub, b->nsub, mode);
	diff_eq_int("grid after init(%ld)", a->grid, b->grid, mode);
	diff_eq_int("mask after init(%ld)", a->mask, b->mask, mode);
	diff_eq_int("shift after init(%ld)", a->shift, b->shift, mode);
	diff_eq_int("depth after init(%ld)", a->depth, b->depth, mode);
	diff_eq_int("ring after init(%ld)", a->ring, b->ring, mode);
	diff_eq_int("prev after init(%ld)", a->prev, b->prev, mode);
	for (k = 0; k < 8; k++)
		diff_eq_int("metric after init(%ld)", a->metric[k],
			    b->metric[k], mode * 8 + k);
	for (k = 0; k < 128; k++) {
		diff_eq_int("ring sym after init(%ld)", a->paths[k].sym,
			    b->paths[k].sym, mode * 128 + k);
		diff_eq_int("ring surv after init(%ld)", a->paths[k].surv,
			    b->paths[k].surv, mode * 128 + k);
	}

	/*
	 * The four table pointers, compared by content and not by address:
	 * ours point into our .rodata and the blob's into its own, so the
	 * addresses can never agree and the contents must.
	 */
	n = points_for(a->nsub);
	for (k = 0; k < n; k++) {
		diff_eq_int("imap selected(%ld)", a->imap[k], b->imap[k],
			    mode * 128 + k);
		diff_eq_int("qmap selected(%ld)", a->qmap[k], b->qmap[k],
			    mode * 128 + k);
	}
	n = 2 * a->grid * a->grid;
	for (k = 0; k < n; k++)
		diff_eq_int("region selected(%ld)", a->region[k],
			    b->region[k], mode * 128 + k);
	n = a->region[0];
	for (k = 0; k < n + 32; k++)
		diff_eq_int("bound selected(%ld)", a->bound[k], b->bound[k],
			    mode * 2048 + k);
}

static struct vtb ours, theirs;

static int
run_init(void)
{
	int mode;

	diff_begin("VTBv32_init: every rate code, both allocation paths");
	for (mode = 0; mode <= 5; mode++) {
		memset(&ours, 0, sizeof(ours));
		memset(&theirs, 0, sizeof(theirs));
		VTBv32_init(&ours, (short)mode, 1);
		ref_VTBv32_init(&theirs, (short)mode, 1);
		cmp_init(&ours, &theirs, mode);

		/*
		 * And again without allocating, over a ring both sides have
		 * dirtied: the clear has to run either way.
		 */
		ours.paths[3].sym = 0x1234;
		ours.paths[3].surv = 5;
		theirs.paths[3].sym = 0x1234;
		theirs.paths[3].surv = 5;
		VTBv32_init(&ours, (short)mode, 0);
		ref_VTBv32_init(&theirs, (short)mode, 0);
		cmp_init(&ours, &theirs, mode + 16);
	}
	return diff_end();
}

/*
 * The region index, computed the way `VTB_decoder` computes it, for the
 * separation count below.  `rot` chooses the frame.
 */
static int
region_index(int i, int q, int grid, int rot)
{
	int ri = i, rq = q, ci, cq, lim;

	if (rot == 1) {
		ri = (i + q) >> 2;
		rq = (q - i) >> 2;
	} else if (rot == 3) {
		ri = (short)(i + q) >> 1;
		rq = (short)(q - i) >> 1;
	}
	ci = ((ri < 0 ? -ri : ri) + 0x400) >> 11;
	cq = ((rq < 0 ? -rq : rq) + 0x400) >> 11;
	lim = grid - 1;
	if (ci > lim)
		ci = lim;
	if (cq > lim)
		cq = lim;
	return (short)(ci + grid * (short)(cq + (cq > ci ? grid : 0)));
}

/*
 * One rate, `n` symbols, both sides in lockstep.  Returns the number of
 * trials at which the rotated and unrotated region indices differ -- the
 * trials that can tell the two readings of the frame apart.
 */
static int
run_rate(int mode, int n)
{
	short got, want;
	int k, j, sep = 0;
	int i, q;

	memset(&ours, 0, sizeof(ours));
	memset(&theirs, 0, sizeof(theirs));
	VTBv32_init(&ours, (short)mode, 1);
	ref_VTBv32_init(&theirs, (short)mode, 1);

	for (k = 0; k < n; k++) {
		/*
		 * Points over the whole plane the equaliser can produce, not
		 * only near constellation points: the region clamp and the
		 * quadrant split both need the corners.
		 */
		i = rnd(60000) - 30000;
		q = rnd(60000) - 30000;
		if ((k & 7) == 0) {
			/* and, deliberately, exactly on a decision boundary */
			i = (rnd(9) - 4) * 4096;
			q = (rnd(9) - 4) * 4096;
		}

		if (ours.nsub == 1 || ours.nsub == 3) {
			if (region_index(i, q, ours.grid, ours.nsub)
			    != region_index(i, q, ours.grid, 0))
				sep++;
		}

		got = want = -1;
		VTB_decoder(&ours, (short)i, (short)q, &got);
		ref_VTB_decoder(&theirs, (short)i, (short)q, &want);

		diff_eq_int("decoded symbol (trial %ld)", got, want,
			    mode * 10000 + k);
		diff_eq_int("ring (trial %ld)", ours.ring, theirs.ring,
			    mode * 10000 + k);
		diff_eq_int("prev (trial %ld)", ours.prev, theirs.prev,
			    mode * 10000 + k);
		for (j = 0; j < 8; j++)
			diff_eq_int("metric (trial.state %ld)", ours.metric[j],
				    theirs.metric[j],
				    (mode * 10000 + k) * 8 + j);
		for (j = 0; j < 128; j++) {
			diff_eq_int("ring sym (trial.node %ld)",
				    ours.paths[j].sym, theirs.paths[j].sym,
				    (mode * 10000 + k) * 128 + j);
			diff_eq_int("ring surv (trial.node %ld)",
				    ours.paths[j].surv, theirs.paths[j].surv,
				    (mode * 10000 + k) * 128 + j);
		}
	}
	return sep;
}

int
main(void)
{
	int rc = 0;
	int sep = 0;

	seed = 20260816u;

	rc |= run_layout();
	rc |= run_tables();
	rc |= run_lengths();
	rc |= run_init();

	diff_begin("VTB_decoder: 14400, the 128-point constellation");
	sep += run_rate(5, 96);
	rc |= diff_end();

	diff_begin("VTB_decoder: 12000, the rotated 64-point constellation");
	sep += run_rate(4, 96);
	rc |= diff_end();

	diff_begin("VTB_decoder: 9600, the 32-point constellation");
	sep += run_rate(2, 96);
	rc |= diff_end();

	diff_begin("VTB_decoder: 7200, the rotated 16-point constellation");
	sep += run_rate(3, 96);
	rc |= diff_end();

	/*
	 * The check that the run above could have failed.  If no trial put
	 * the rotated and unrotated region lookups in different cells then
	 * every trial agreed with the wrong reading too, and a pass would
	 * mean nothing.
	 */
	diff_begin("VTB_decoder: the trials that separate the two frames");
	diff_eq_int("trials where the frames disagree (%ld)", sep > 0, 1, sep);
	rc |= diff_end();

	return rc;
}
