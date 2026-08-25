/*
 * t_v32fset.c -- differential test of V.32bis' four TRELLIS slicers.
 *
 * Separate from `t_v32fse.c` because these four are a different problem.  The
 * other five decide and return; these four also drive `VTB_decoder` over the
 * Viterbi state the datapump object carries at +0x18, so what has to agree is
 * not only the decision but sixteen symbols of survivor history.
 *
 * WHAT IS COMPARED, per symbol, on both sides:
 *
 *   - the returned decision, `*angle` and `*mag`;
 *   - the datapump object, WITH THE VITERBI WINDOW BLANKED.  `struct vtb`'s
 *     `paths`, `imap`, `qmap`, `bound` and `region` are ADDRESSES -- ours into
 *     our tables and our heap, the blob's into its own -- so bytes 0x18..0x50
 *     can never agree and are zeroed in a copy of each object before
 *     `diff_eq_obj` sees them.  Everything the slicers themselves write is
 *     outside that window;
 *   - the Viterbi state field by field instead: the eight path metrics, the
 *     ring index, the differential state, the four scalars `VTBv32_init` set,
 *     and all 128 survivor nodes BY CONTENT.  That is what pins the pointers,
 *     which is finding F1614's rule and `t_v32vtb`'s;
 *   - that neither side installed a successor.  All four slicers are
 *     terminal; a `cfg.decision` that moved would be a wrong reading of the
 *     one branch that could have written it.
 *
 * BOTH SIDES RUN THEIR OWN `VTBv32_init` FIRST, ours through `VTBv32_init`
 * and the blob's through `ref_VTBv32_init`, because `VTB_decoder`
 * dereferences `paths` and a zeroed object would fault.  The rate code is
 * chosen from the CONSTELLATION each slicer decides on, not from the name of
 * its magnitude table: `_16Tpt` reads `DECv32_MAG9600` but is the sixteen-
 * point trellis constellation, which is mode 3 (7200).  Getting that wrong
 * would leave both sides agreeing about a decoder whose `imap` did not match
 * the slicer's constellation, and the test would quietly measure less.
 *
 * THE REGION TREES ARE THE POINT, AND RANDOM INPUT CANNOT REACH THEM.
 * `_64pt` has sixteen cells, `_128pt` twelve leaves of which two skip the
 * search entirely, and `_32pt` three bands with a two-way tie-break on top.
 * Several of those are thin slivers in the ROTATED frame, which no hand-
 * picked (I, Q) reaches by inspection.  So this file scans a coarse grid of
 * the plane once per slicer, keeps one input per leaf, and then ASSERTS THAT
 * EVERY LEAF COUNT IS NON-ZERO.  A region tree tested only near the origin is
 * a test that cannot see a wrong threshold -- finding F134's rule applied to
 * the input set.
 *
 * And the exact-value inputs are constructed, not sampled.  `_64pt`'s two
 * axes split at different places -- `i > 8192` against `q > 8191` -- and the
 * rotation's `sel` turns on `|i| > |q|`; both differences are one value wide
 * and a uniform sample essentially never lands on them.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32fse.h"
#include "dsplib/vtb.h"

extern unsigned short ref_FSE_decision_16Tpt(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_32pt(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_64pt(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_128pt(void *s, short *a, short *m);
extern void ref_VTBv32_init(void *st, short mode, int alloc);

extern short ref_DECv32_ANA_QMAP[8];
extern short ref_DECv32_COS_ROT_ANGLE[4];
extern short ref_DECv32_SIN_ROT_ANGLE[4];
extern short ref_DECv32_MAG9600T[32];
extern short ref_DECv32_ANGL9600T[32];
extern const short ref_DECv32_IMAP64[64];
extern const short ref_DECv32_QMAP64[64];
extern const short ref_DECv32_MAG12000[64];
extern const short ref_DECv32_ANGL12000[64];
extern const short ref_DECv32_ANA_IMAP128[32];
extern const short ref_DECv32_ANA_QMAP128[32];
extern const short ref_DECv32_MAG14400[128];
extern const short ref_DECv32_ANGL14400[128];

#define NSYM	64

typedef unsigned short (*ref_decision)(void *, short *, short *);

static struct fpm_fse st_a, st_b;
static struct v32_dec dec_a, dec_b;
static short i_a[NSYM], q_a[NSYM], i_b[NSYM], q_b[NSYM];
static fpm_fse_decision installed_a, installed_b;

static unsigned seed;

static int
rnd(int range)
{
	seed = seed * 1103515245u + 12345u;
	return (int)((seed >> 13) % (unsigned)range);
}

/* ------------------------------------------------------------------ */
/* The harness                                                        */
/* ------------------------------------------------------------------ */

static void
setup(fpm_fse_decision ours, fpm_fse_decision theirs, int mode)
{
	memset(&st_a, 0, sizeof(st_a));
	memset(&st_b, 0, sizeof(st_b));
	memset(&dec_a, 0, sizeof(dec_a));
	memset(&dec_b, 0, sizeof(dec_b));
	memset(i_a, 0, sizeof(i_a));
	memset(q_a, 0, sizeof(q_a));
	memset(i_b, 0, sizeof(i_b));
	memset(q_b, 0, sizeof(q_b));

	st_a.cfg = FSEv32_CFG;
	st_b.cfg = FSEv32_CFG;
	st_a.cfg.owner = &dec_a;
	st_b.cfg.owner = &dec_b;
	st_a.cfg.decision = ours;
	st_b.cfg.decision = theirs;
	st_a.out_i = i_a;
	st_a.out_q = q_a;
	st_b.out_i = i_b;
	st_b.out_q = q_b;
	installed_a = ours;
	installed_b = theirs;

	VTBv32_init((struct vtb *)dec_a.vtb, (short)mode, 1);
	ref_VTBv32_init((struct vtb *)dec_b.vtb, (short)mode, 1);
}

static void
compare(long n)
{
	struct v32_dec ca, cb;
	struct vtb *va = (struct vtb *)dec_a.vtb;
	struct vtb *vb = (struct vtb *)dec_b.vtb;
	int j;

	/*
	 * `memcpy` and not struct assignment: assignment is not required to
	 * copy PADDING, and `diff_eq_obj` compares every byte.  Under a
	 * member-wise copy the pad bytes of the two copies would be
	 * independent stack garbage and this would fail for a reason that has
	 * nothing to do with the slicers.
	 */
	memcpy(&ca, &dec_a, sizeof(ca));
	memcpy(&cb, &dec_b, sizeof(cb));
	memset(ca.vtb, 0, sizeof(ca.vtb));
	memset(cb.vtb, 0, sizeof(cb.vtb));
	diff_eq_obj("decoder object, Viterbi window blanked", struct v32_dec,
		    &ca, &cb, n);

	diff_eq_int("vtb ring (%ld)", va->ring, vb->ring, n);
	diff_eq_int("vtb prev (%ld)", va->prev, vb->prev, n);
	diff_eq_int("vtb nsub (%ld)", va->nsub, vb->nsub, n);
	diff_eq_int("vtb grid (%ld)", va->grid, vb->grid, n);
	diff_eq_int("vtb mask (%ld)", va->mask, vb->mask, n);
	diff_eq_int("vtb shift (%ld)", va->shift, vb->shift, n);
	diff_eq_int("vtb depth (%ld)", va->depth, vb->depth, n);
	for (j = 0; j < 8; j++)
		diff_eq_int("vtb metric (%ld)", va->metric[j], vb->metric[j],
			    n * 8 + j);
	for (j = 0; j < 128; j++) {
		diff_eq_int("vtb survivor sym (%ld)", va->paths[j].sym,
			    vb->paths[j].sym, n * 128 + j);
		diff_eq_int("vtb survivor surv (%ld)", va->paths[j].surv,
			    vb->paths[j].surv, n * 128 + j);
	}

	diff_eq_int("mu_sel (%ld)", st_a.mu_sel, st_b.mu_sel, n);
	diff_eq_int("lms_on (%ld)", st_a.lms_on, st_b.lms_on, n);
	diff_eq_int("ours installed no successor (%ld)",
		    st_a.cfg.decision == installed_a, 1, n);
	diff_eq_int("theirs installed no successor (%ld)",
		    st_b.cfg.decision == installed_b, 1, n);
}

static void
step(fpm_fse_decision ours, ref_decision theirs, int slot, short ai, short aq,
     short ang, short mag, long n)
{
	short ang_a = ang, ang_b = ang;
	short mag_a = mag, mag_b = mag;
	unsigned short ra, rb;

	st_a.n_out = (unsigned short)slot;
	st_b.n_out = (unsigned short)slot;
	i_a[slot] = i_b[slot] = ai;
	q_a[slot] = q_b[slot] = aq;

	rb = theirs(&st_b, &ang_b, &mag_b);
	ra = ours(&st_a, &ang_a, &mag_a);

	diff_eq_int("decision (%ld)", ra, rb, n);
	diff_eq_int("angle out (%ld)", ang_a, ang_b, n);
	diff_eq_int("magnitude out (%ld)", mag_a, mag_b, n);
	compare(n);
}

/* ------------------------------------------------------------------ */
/* The leaf oracles.  An independent copy of each region tree, used only     */
/* to COUNT which leaf a trial enters; correctness is the differential       */
/* comparison above and never these.                                        */
/* ------------------------------------------------------------------ */

static int
t_rotate(int i, int q, short *ri, short *rq)
{
	short ai = (short)(i < 0 ? -i : i);
	short aq = (short)(q < 0 ? -q : q);
	int sel = (ai > aq) + (i > q ? 2 : 0);
	int c = DECv32_COS_ROT_ANGLE[sel];
	int s = DECv32_SIN_ROT_ANGLE[sel];

	*ri = (short)(((i * c) >> 15) - ((q * s) >> 15));
	*rq = (short)(((q * c) >> 15) + ((i * s) >> 15));
	return sel;
}

/* Which of the sixteen points `_16Tpt` decides.  Sixteen leaves. */
static int
leaf_16T(int i, int q)
{
	int k, best = 0;
	short min = 0x7fff;

	for (k = 0; k <= 15; k++) {
		int di = (short)(i - DECv32_IMAP16[k]);
		int dq = (short)(q - DECv32_QMAP16[k]);
		short d = (short)(((di * di) >> 16) + ((dq * dq) >> 16));

		if (d < min) {
			best = k;
			min = d;
		}
	}
	return best;
}

/* `_64pt`'s sixteen cells, numbered by base >> 2. */
static int
leaf_64(int i, int q)
{
	int base;

	if (i > 0) {
		if (q > 0)
			base = (i > 0x2000) ? (q > 0x1fff ? 0x28 : 0x2c)
					    : (q > 0x1fff ? 0x20 : 0x24);
		else
			base = (i > 0x2000) ? (q >= -0x2000 ? 0x38 : 0x3c)
					    : (q >= -0x2000 ? 0x30 : 0x34);
	} else {
		if (q > 0)
			base = (i > -0x2000) ? (q > 0x1fff ? 0x08 : 0x0c)
					     : (q > 0x1fff ? 0x00 : 0x04);
		else
			base = (i > -0x2000) ? (q >= -0x2000 ? 0x18 : 0x1c)
					     : (q >= -0x2000 ? 0x10 : 0x14);
	}
	return base >> 2;
}

/*
 * `_32pt`: the region is 0, 1 or 2 by which three (or two) candidates the
 * band selects; the return value is 0 when the tie-break did not run, 1 when
 * it chose point 3 and 2 when it chose point 6.
 */
static int
leaf_32(int i, int q, int *region)
{
	short ri, rq;
	int first = 0, count = 3, k, best = 0;
	short min = 0x7fff;

	t_rotate(i, q, &ri, &rq);
	if (ri > 0x16a0) {
		first = 3;
		if (ri > 0x2d41 && rq <= 0x2d40) {
			first = 6;
			count = 2;
		}
	}
	*region = first / 3;

	for (k = first; k < first + count; k++) {
		short d = (short)(rq >= DECv32_ANA_QMAP[k]
				  ? rq - DECv32_ANA_QMAP[k]
				  : DECv32_ANA_QMAP[k] - rq);

		if (d < min) {
			best = k;
			min = d;
		}
	}

	if (best == 3 || best == 6) {
		short d3i = (short)(0x21f1 - ri);
		short d3q = (short)(DECv32_ANA_QMAP[3] - rq);
		short d6i = (short)(0x3891 - ri);
		short d6q = (short)(DECv32_ANA_QMAP[6] - rq);
		short e3 = (short)(((d3q * d3q) >> 15) + ((d3i * d3i) >> 15));
		short e6 = (short)(((d6q * d6q) >> 15) + ((d6i * d6i) >> 15));

		return (e6 < e3) ? 2 : 1;
	}
	return 0;
}

/*
 * `_128pt`: ten base cells in the order 0x00, 04, 08, 0c, 10, 14, 18, 1c, 0e,
 * 1a, then 10 for the outer ambiguous cell and 11 for the inner one.  `tie` is
 * -1 unless the search was skipped, and then 0..3 for the four outcomes the
 * two two-way decisions can have.
 */
static int
leaf_128(int i, int q, int *tie)
{
	short ri, rq;
	int base = 0;
	unsigned int amb = 0;
	static const int slot[8] = { 0x00, 0x04, 0x08, 0x0c,
				     0x10, 0x14, 0x18, 0x1c };
	int k;

	*tie = -1;
	t_rotate(i, q, &ri, &rq);

	if (ri > 0x16a0) {
		base = 0x0c;
		if (ri > 0x2d41) {
			if (rq <= 0x2d41)
				base = (rq <= 0x16a0) ? 0x1c : 0x18;
			else if (ri <= 0x3891) {
				if (rq > 0x3891)
					base = 0x0e;
				else
					amb = 2;
			} else {
				if (rq < 0x3891)
					base = 0x1a;
				else
					amb = 1;
			}
		} else if (rq <= 0x2d40) {
			base = (rq <= 0x169f) ? 0x14 : 0x10;
		}
	} else if (rq <= 0x2d40) {
		base = (rq <= 0x169f) ? 0x08 : 0x04;
	}

	if (amb & 1) {
		short d14i = (short)(0x2799 - ri);
		short d14q = (short)(DECv32_ANA_QMAP128[14] - rq);
		short d26i = (short)(0x3e3a - ri);
		short d26q = (short)(DECv32_ANA_QMAP128[26] - rq);
		short e14 = (short)(((d14q * d14q) >> 13)
				    + ((d14i * d14i) >> 13));
		short e26 = (short)(((d26q * d26q) >> 13)
				    + ((d26i * d26i) >> 13));

		*tie = (e26 >= e14) ? 1 : 0;
		return 10;
	}
	if (amb >> 1) {
		short d15i = (short)(0x2799 - ri);
		short d15q = (short)(DECv32_ANA_QMAP128[15] - rq);
		short d24i = (short)(0x32e9 - ri);
		short d24q = (short)(DECv32_ANA_QMAP128[24] - rq);
		short e15 = (short)(((d15q * d15q) >> 13)
				    + ((d15i * d15i) >> 13));
		short e24 = (short)(((d24q * d24q) >> 13)
				    + ((d24i * d24i) >> 13));

		*tie = (e24 <= e15) ? 3 : 2;
		return 11;
	}

	if (base == 0x0e)
		return 8;
	if (base == 0x1a)
		return 9;
	for (k = 0; k < 8; k++)
		if (slot[k] == base)
			return k;
	return -1;
}

/* ------------------------------------------------------------------ */
/* The input set                                                      */
/* ------------------------------------------------------------------ */

#define MAXTRIAL	1024

static short in_i[MAXTRIAL], in_q[MAXTRIAL];
static int ntrial;

static void
add(int i, int q)
{
	if (ntrial < MAXTRIAL) {
		in_i[ntrial] = (short)i;
		in_q[ntrial] = (short)q;
		ntrial++;
	}
}

/*
 * The values that have to be hit exactly.  +-8191/8192/8193 are `_64pt`'s two
 * axes disagreeing by one; the symmetric list makes every |i| == |q| pair --
 * where the rotation's `sel` turns over -- appear as a product term.
 */
static const int edge_v[] = {
	0, 1, -1, 2896, -2896, 4096, -4096, 8191, 8192, 8193,
	-8191, -8192, -8193, 12288, -12288, 16384, -16384, 23170, -23170, 30000
};
#define NEDGE	((int)(sizeof(edge_v) / sizeof(edge_v[0])))

static void
build_common(int nrandom)
{
	int a, b, k;

	ntrial = 0;
	for (a = 0; a < NEDGE; a++)
		for (b = 0; b < NEDGE; b++)
			add(edge_v[a], edge_v[b]);
	for (k = 0; k < nrandom; k++)
		add(rnd(60000) - 30000, rnd(60000) - 30000);
}

/*
 * One representative input per leaf, found by sweeping the plane.  The leaves
 * of `_32pt` and `_128pt` live in the rotated frame, so they cannot be
 * constructed by inspection; this is how they get entered at all.
 */
#define SCAN_LO		(-30000)
#define SCAN_HI		30000
#define SCAN_STEP	53

/*
 * AND THE BOUNDARIES OF THE ROTATED TREES, ONE INPUT EACH.
 *
 * `_32pt` and `_128pt` cut on `ri` and `rq`, which no (I, Q) reaches by
 * inspection, and their thresholds are encoded inconsistently: the same
 * nominal boundary is `> 0x2d40` in one arm and `<= 0x2d41` in another
 * (D372).  A test that never puts a symbol EXACTLY on one of those lines
 * cannot tell the object's tree from a tidied one.  `ri` moves by about 0.7
 * per unit of Q, so sweeping Q at a few fixed I hits nearly every integer
 * value of both coordinates and the exact hits fall out.
 */
static const int thresh[] = {
	0x169f, 0x16a0, 0x16a1, 0x2d40, 0x2d41, 0x2d42, 0x3890, 0x3891, 0x3892
};
#define NTHRESH	((int)(sizeof(thresh) / sizeof(thresh[0])))

/*
 * AND `rq` ON A LINE IS NOT ENOUGH: the arm has to be right too.  D372 is
 * that the third arm of `_128pt`'s tree -- the one `ri > 0x2d41` reaches --
 * claims 5792 and 11585 for the band BELOW where the first two claim them for
 * the band above.  An input with `rq` exactly 5792 and a small `ri` enters the
 * first arm and cannot see that at all.  So the thresholds are collected
 * twice: once anywhere, and once restricted to `ri > 0x2d41`.
 */
#define ARM3_RI	0x2d41

static void
collect_boundaries(void)
{
	static const int base_i[7] = { -32768, -20000, -8000, 0, 8000, 20000,
				       32767 };
	int seen_ri[NTHRESH], seen_rq[NTHRESH], seen_arm3[NTHRESH];
	int a, k, q;
	short ri, rq;

	memset(seen_ri, 0, sizeof(seen_ri));
	memset(seen_rq, 0, sizeof(seen_rq));
	memset(seen_arm3, 0, sizeof(seen_arm3));
	for (a = 0; a < 7; a++)
		for (q = -32768; q <= 32767; q++) {
			t_rotate(base_i[a], q, &ri, &rq);
			for (k = 0; k < NTHRESH; k++) {
				if (!seen_ri[k] && ri == thresh[k]) {
					seen_ri[k] = 1;
					add(base_i[a], q);
				}
				if (!seen_rq[k] && rq == thresh[k]) {
					seen_rq[k] = 1;
					add(base_i[a], q);
				}
				if (!seen_arm3[k] && rq == thresh[k]
				    && ri > ARM3_RI) {
					seen_arm3[k] = 1;
					add(base_i[a], q);
				}
			}
		}

	/*
	 * The two D372 witnesses, named as well as swept, in case the sweep
	 * above ever stops reaching them.  Both put `rq` exactly on a boundary
	 * with `ri` in the third arm, which is the only place the two
	 * conventions give different cells: (-32768, -24576) rotates to
	 * (24989, 5792) and (-32768, -16384) to (30781, 11585).
	 */
	add(-32768, -24576);
	add(-32768, -16384);
}

static void
collect_16T(void)
{
	int i, q, seen[16], id;

	memset(seen, 0, sizeof(seen));
	for (i = SCAN_LO; i <= SCAN_HI; i += SCAN_STEP)
		for (q = SCAN_LO; q <= SCAN_HI; q += SCAN_STEP) {
			id = leaf_16T(i, q);
			if (!seen[id]) {
				seen[id] = 1;
				add(i, q);
			}
		}
}

static void
collect_64(void)
{
	int i, q, seen[16], id;

	memset(seen, 0, sizeof(seen));
	for (i = SCAN_LO; i <= SCAN_HI; i += SCAN_STEP)
		for (q = SCAN_LO; q <= SCAN_HI; q += SCAN_STEP) {
			id = leaf_64(i, q);
			if (!seen[id]) {
				seen[id] = 1;
				add(i, q);
			}
		}
}

static void
collect_32(void)
{
	int i, q, region, tie;
	int seen_r[3], seen_t[3];

	memset(seen_r, 0, sizeof(seen_r));
	memset(seen_t, 0, sizeof(seen_t));
	for (i = SCAN_LO; i <= SCAN_HI; i += SCAN_STEP)
		for (q = SCAN_LO; q <= SCAN_HI; q += SCAN_STEP) {
			tie = leaf_32(i, q, &region);
			if (!seen_r[region] || !seen_t[tie]) {
				seen_r[region] = 1;
				seen_t[tie] = 1;
				add(i, q);
			}
		}
}

static void
collect_128(void)
{
	int i, q, tie, id;
	int seen[12], seen_t[4];

	memset(seen, 0, sizeof(seen));
	memset(seen_t, 0, sizeof(seen_t));
	for (i = SCAN_LO; i <= SCAN_HI; i += SCAN_STEP)
		for (q = SCAN_LO; q <= SCAN_HI; q += SCAN_STEP) {
			id = leaf_128(i, q, &tie);
			if (id < 0)
				continue;
			if (!seen[id] || (tie >= 0 && !seen_t[tie])) {
				seen[id] = 1;
				if (tie >= 0)
					seen_t[tie] = 1;
				add(i, q);
			}
		}
}

/* ------------------------------------------------------------------ */
/* The inlined preamble's own state machine                           */
/* ------------------------------------------------------------------ */

/*
 * `fse_quality` is inlined into all four, and none of the decision cases
 * above reaches its two retrain windows: scattered symbols keep `eqm` over
 * 0x4ff and the count never runs.  These two passes are `t_v32fse`'s, which
 * is where the same preamble was first pinned.
 */
static void
run_quality(fpm_fse_decision ours, ref_decision theirs, int mode, int *saw1,
	    int *saw2)
{
	int k;

	setup(ours, (fpm_fse_decision)theirs, mode);
	for (k = 0; k < 90; k++) {
		step(ours, theirs, k % NSYM, 4096, 12288,
		     (short)rnd(30000), (short)rnd(30000), 100000 + k);
		if (dec_b.retrain == 1)
			*saw1 = 1;
	}

	setup(ours, (fpm_fse_decision)theirs, mode);
	for (k = 0; k < 200; k++) {
		short ii = (short)((k % 30 == 29) ? -12288 : 4096);
		short qq = (short)((k % 30 == 29) ? -4096 : 12288);

		step(ours, theirs, k % NSYM, ii, qq, (short)rnd(30000),
		     (short)rnd(30000), 200000 + k);
		if (dec_b.retrain == 2)
			*saw2 = 1;
	}
}

/* ------------------------------------------------------------------ */
/* The four suites                                                    */
/* ------------------------------------------------------------------ */

static int
run_16T(void)
{
	int k, leaf[16], saw1 = 0, saw2 = 0;

	diff_begin("FSE_decision_16Tpt: 7200 bit/s, all sixteen points searched");
	memset(leaf, 0, sizeof(leaf));

	build_common(300);
	collect_16T();

	setup(FSE_decision_16Tpt, (fpm_fse_decision)ref_FSE_decision_16Tpt, 3);
	for (k = 0; k < ntrial; k++) {
		leaf[leaf_16T(in_i[k], in_q[k])]++;
		step(FSE_decision_16Tpt, ref_FSE_decision_16Tpt, k % NSYM,
		     in_i[k], in_q[k], (short)rnd(30000), (short)rnd(30000), k);
	}
	for (k = 0; k < 16; k++)
		diff_eq_int("point %ld was decided at least once",
			    leaf[k] > 0, 1, k);

	run_quality(FSE_decision_16Tpt, ref_FSE_decision_16Tpt, 3, &saw1, &saw2);
	diff_eq_int("the count reached sixty (%ld)", saw1, 1, 0);
	diff_eq_int("the quiet-then-jump retrain fired (%ld)", saw2, 1, 0);
	return diff_end();
}

static int
run_64(void)
{
	int k, leaf[16], saw1 = 0, saw2 = 0;

	diff_begin("FSE_decision_64pt: 12000 bit/s, a four-by-four region tree");
	memset(leaf, 0, sizeof(leaf));

	build_common(300);
	collect_64();

	setup(FSE_decision_64pt, (fpm_fse_decision)ref_FSE_decision_64pt, 4);
	for (k = 0; k < ntrial; k++) {
		leaf[leaf_64(in_i[k], in_q[k])]++;
		step(FSE_decision_64pt, ref_FSE_decision_64pt, k % NSYM,
		     in_i[k], in_q[k], (short)rnd(30000), (short)rnd(30000), k);
	}
	for (k = 0; k < 16; k++)
		diff_eq_int("cell %ld was entered at least once",
			    leaf[k] > 0, 1, k);

	run_quality(FSE_decision_64pt, ref_FSE_decision_64pt, 4, &saw1, &saw2);
	diff_eq_int("the count reached sixty (%ld)", saw1, 1, 0);
	diff_eq_int("the quiet-then-jump retrain fired (%ld)", saw2, 1, 0);
	return diff_end();
}

static int
run_32(void)
{
	int k, region, tie, leaf[3], tied[3], saw1 = 0, saw2 = 0;

	diff_begin("FSE_decision_32pt: 9600 bit/s, three bands and a tie-break");
	memset(leaf, 0, sizeof(leaf));
	memset(tied, 0, sizeof(tied));

	build_common(300);
	collect_32();
	collect_boundaries();

	setup(FSE_decision_32pt, (fpm_fse_decision)ref_FSE_decision_32pt, 2);
	for (k = 0; k < ntrial; k++) {
		tie = leaf_32(in_i[k], in_q[k], &region);
		leaf[region]++;
		tied[tie]++;
		step(FSE_decision_32pt, ref_FSE_decision_32pt, k % NSYM,
		     in_i[k], in_q[k], (short)rnd(30000), (short)rnd(30000), k);
	}
	for (k = 0; k < 3; k++)
		diff_eq_int("band %ld was entered at least once",
			    leaf[k] > 0, 1, k);
	diff_eq_int("the tie-break was skipped at least once (%ld)",
		    tied[0] > 0, 1, 0);
	diff_eq_int("the tie-break chose point 3 at least once (%ld)",
		    tied[1] > 0, 1, 0);
	diff_eq_int("the tie-break chose point 6 at least once (%ld)",
		    tied[2] > 0, 1, 0);

	run_quality(FSE_decision_32pt, ref_FSE_decision_32pt, 2, &saw1, &saw2);
	diff_eq_int("the count reached sixty (%ld)", saw1, 1, 0);
	diff_eq_int("the quiet-then-jump retrain fired (%ld)", saw2, 1, 0);
	return diff_end();
}

static int
run_128(void)
{
	int k, tie, id, leaf[12], tied[4], saw1 = 0, saw2 = 0;

	diff_begin("FSE_decision_128pt: 14400 bit/s, ten leaves and two skips");
	memset(leaf, 0, sizeof(leaf));
	memset(tied, 0, sizeof(tied));

	build_common(300);
	collect_128();
	collect_boundaries();

	setup(FSE_decision_128pt, (fpm_fse_decision)ref_FSE_decision_128pt, 5);
	for (k = 0; k < ntrial; k++) {
		id = leaf_128(in_i[k], in_q[k], &tie);
		diff_eq_int("the leaf oracle classified trial %ld", id >= 0,
			    1, k);
		if (id >= 0)
			leaf[id]++;
		if (tie >= 0)
			tied[tie]++;
		step(FSE_decision_128pt, ref_FSE_decision_128pt, k % NSYM,
		     in_i[k], in_q[k], (short)rnd(30000), (short)rnd(30000), k);
	}
	for (k = 0; k < 12; k++)
		diff_eq_int("leaf %ld was entered at least once",
			    leaf[k] > 0, 1, k);
	for (k = 0; k < 4; k++)
		diff_eq_int("two-way outcome %ld was taken at least once",
			    tied[k] > 0, 1, k);

	run_quality(FSE_decision_128pt, ref_FSE_decision_128pt, 5, &saw1, &saw2);
	diff_eq_int("the count reached sixty (%ld)", saw1, 1, 0);
	diff_eq_int("the quiet-then-jump retrain fired (%ld)", saw2, 1, 0);
	return diff_end();
}

/* ------------------------------------------------------------------ */

/*
 * THE THIRTEEN TABLES, ENTRY BY ENTRY, AND NOT ONLY THROUGH THE SLICERS.
 *
 * The suites above reach a table entry only if some input makes a slicer
 * index it, and nothing asserts that all 640 entries are reached --
 * `MAG14400` needs all four rotations of all thirty-two points before it is
 * fully read.  So each table is also compared straight against the blob's
 * copy, which is `t_v32vtb`'s and `t_v32ecc`'s convention.  Like theirs, this
 * proves the BYTES and not the width: 128 shorts and 64 ints hold the same
 * bytes.  The widths come from the `(%reg,%reg,1)` addressing of every load
 * in all four functions, and from `nm -S`, not from here.
 */
static void
cmp_table(const char *what, const short *ours, const short *theirs, int n)
{
	int k;

	for (k = 0; k < n; k++)
		diff_eq_int_(__FILE__, __LINE__, what, (long)ours[k],
			     (long)theirs[k], (long)k);
}

#define CMP_TAB(name, n)	cmp_table(#name "[%ld]", name, ref_##name, (n))

static int
run_tables(void)
{
	diff_begin("DECv32 trellis tables: byte for byte against the blob");

	CMP_TAB(DECv32_ANA_QMAP, 8);
	CMP_TAB(DECv32_COS_ROT_ANGLE, 4);
	CMP_TAB(DECv32_SIN_ROT_ANGLE, 4);
	CMP_TAB(DECv32_MAG9600T, 32);
	CMP_TAB(DECv32_ANGL9600T, 32);
	CMP_TAB(DECv32_IMAP64, 64);
	CMP_TAB(DECv32_QMAP64, 64);
	CMP_TAB(DECv32_MAG12000, 64);
	CMP_TAB(DECv32_ANGL12000, 64);
	CMP_TAB(DECv32_ANA_IMAP128, 32);
	CMP_TAB(DECv32_ANA_QMAP128, 32);
	CMP_TAB(DECv32_MAG14400, 128);
	CMP_TAB(DECv32_ANGL14400, 128);

	/*
	 * And D371, asserted rather than only written down: the literal
	 * `_128pt` uses for point 26's I coordinate is one MORE than the table
	 * entry for the same point, while the other three literals match
	 * theirs exactly.  If that ever stops being true the deviation is
	 * wrong, and this is the check that says so.
	 */
	diff_eq_int("ANA_IMAP128[14] equals its literal (%ld)",
		    DECv32_ANA_IMAP128[14], 0x2799, 14);
	diff_eq_int("ANA_IMAP128[15] equals its literal (%ld)",
		    DECv32_ANA_IMAP128[15], 0x2799, 15);
	diff_eq_int("ANA_IMAP128[24] equals its literal (%ld)",
		    DECv32_ANA_IMAP128[24], 0x32e9, 24);
	diff_eq_int("ANA_IMAP128[26] is one BELOW its literal (%ld)",
		    DECv32_ANA_IMAP128[26] + 1, 0x3e3a, 26);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	diff_begin("v32_dec: where the Viterbi state sits");
	diff_eq_int("vtb (%ld)",
		    (long)((char *)dec_a.vtb - (char *)&dec_a), 0x18, 0);
	diff_eq_int("vtb size (%ld)", (long)sizeof(dec_a.vtb), 0x38, 0);
	diff_eq_int("it is exactly struct vtb (%ld)",
		    (long)sizeof(struct vtb), 0x38, 0);
	diff_eq_int("ang_prev follows it (%ld)",
		    (long)((char *)&dec_a.ang_prev - (char *)&dec_a), 0x50, 0);
	rc |= diff_end();

	rc |= run_tables();

	seed = 20260816u;
	rc |= run_16T();
	rc |= run_64();
	rc |= run_32();
	rc |= run_128();

	return rc;
}
