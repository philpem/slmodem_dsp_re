/*
 * t_v17slicer.c -- differential test of the V.17 fax receiver's seven slicers
 *                  and of the table that holds four of them.
 *
 * Every one of the seven is a state machine over the receiver object as well
 * as a decision, so each case drives OUR copy and the BLOB's from the same
 * inputs one symbol at a time and compares, after every call:
 *
 *   - the returned decision, `*angle` and `*mag`;
 *   - the whole `struct v17_dec`, WITH THE VITERBI WINDOW BLANKED.  `struct
 *     vtb`'s `paths`, `imap`, `qmap`, `bound` and `region` are ADDRESSES --
 *     ours into our tables and our heap, the blob's into its own -- so bytes
 *     0x04..0x3c can never agree and are zeroed in a copy of each object
 *     before `diff_eq_obj` sees them.  Everything the slicers write themselves
 *     is outside that window;
 *   - the Viterbi state field by field instead, and all 128 survivor nodes BY
 *     CONTENT.  That is what pins the pointers the blanking dropped;
 *   - the whole `struct fpm_fse`, with its thirteen pointers zeroed the same
 *     way.  Comparing the block rather than a list of scalars is what catches
 *     a write this reading did not predict at all;
 *   - `cfg.decision` as an IDENTITY, through `slicer_id`, because each side
 *     installs its own build's successor.  That is the check that the
 *     handshake chain is wired the same way on both sides.
 *
 * ===========================================================================
 * THE LAYERS, AND WHAT A WRONG READING OF THE LAYER BELOW WOULD PASS
 * ===========================================================================
 *
 * 1. THE OBJECT'S SHAPE.  `sizeof(struct v17_dec)` and every named offset.
 *    Layer 2 cannot pass this on its own: a struct that is the right size with
 *    two fields transposed would still be compared byte for byte and would
 *    still agree, because both sides would be reading the same bytes -- one
 *    through the blob's hard-coded offsets and one through ours.  What would
 *    NOT agree is the reader of this header, so the offsets are asserted.
 *
 * 2. `FSEv17_decision` IS FOUR FUNCTION POINTERS IN RATE ORDER.  Read as data
 *    those sixteen bytes are eight plausible small integers, and every value
 *    comparison in this file would pass with the table declared `short[8]` and
 *    never indexed.  The check is an IDENTITY: ours holds our four functions,
 *    the blob's holds the blob's four, and the two agree slot for slot under
 *    `slicer_id`.  Then layer 5 drives the handshake THROUGH the table, so a
 *    table that is right and unused would still fail there.
 *
 * 3. THE FOUR RATE SLICERS, over a scan of the plane binned by an independent
 *    copy of each region tree, with EVERY LEAF COUNT ASSERTED NON-ZERO.  A
 *    region tree tested only near the origin is a test that cannot see a wrong
 *    threshold -- finding F134's rule applied to the input set.  Random input
 *    alone would pass a slicer with any one of `_128pt`'s seven V.17-specific
 *    thresholds replaced by V.32bis', because each difference is ONE VALUE
 *    WIDE.
 *
 * 4. THE SEVEN THRESHOLDS THAT DIFFER FROM V.32bis', ONE LINE AT A TIME, by
 *    construction and not by sampling.  `threshold_line` says which of the
 *    seven an input sits EXACTLY on, and EACH OF THE SEVEN REPORTS ITS OWN
 *    DENOMINATOR.  This is the layer that would catch reading `v32fse.c`
 *    across instead of the disassembly, which is the single likeliest mistake
 *    here -- and it is per line because an aggregate version of it ran green
 *    while covering six of the seven (finding F9417).
 *
 * 5. AB's EXIT THRESHOLD, swept across its exact crossing.  Its constant is a
 *    Q14 multiply where V.32's is a real divide, and the two agree over most
 *    of the range; nothing that samples can separate them.
 *
 * 6. THE HANDSHAKE AS A WALK.  AB -> eqtrn -> (Bridge_det) -> rate slicer,
 *    following `cfg.decision` on each side rather than calling a function this
 *    file chose.  Layers 3 to 5 would pass with every handover missing.  Both
 *    the short-training and the long-training arms are walked, and the arm
 *    counts are asserted.
 *
 * 7. THE SYMBOL COUNTER'S WRAP, planted, because 32768 symbols is further than
 *    any run here goes; and THE DEBUG ARM of `_128pt`, at `dsplibs_debug_level`
 *    0 and 2.
 *
 * ===========================================================================
 * WHAT IS PLANTED, AND WHY ZEROING IS NOT ENOUGH (D955, finding F8587)
 * ===========================================================================
 *
 * A blob-against-blob dry run cannot catch an unplanted SUBSCRIPT: both sides
 * read the same out-of-bounds neighbour of the same array and agree, so every
 * comparison passes and only a segfault is left to chance.  The fixture is
 * zeroed and then planted deliberately, and these are the fields used as a
 * subscript rather than dereferenced:
 *
 *   `fpm_fse::n_out`     indexes `out_i[]` and `out_q[]`.  Planted per step,
 *                        and driven across the whole 64-slot buffer so a
 *                        reading that ignored it would be caught.
 *   `v17_dec::rate`      indexes `FSEv17_decision[4]`.  A zeroed fixture gives
 *                        0, which is IN RANGE and would hide a wrong width or
 *                        a wrong element type entirely.  Every handshake case
 *                        runs all four values.
 *   `v17_dec::scram`     drives `FSE_decision_eqtrn`'s two-bit output, which
 *                        indexes `DECv17_MAP_TRN[4]`.  Zero makes the
 *                        generator produce a constant, so it is seeded.
 *   `v17_dec::count`     is compared, not indexed, but a zeroed fixture never
 *                        reaches any segment's exit inside a short run, so it
 *                        is planted near each boundary as well as walked to it.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/v17cfg.h"
#include "dsplib/v17dec.h"
#include "dsplib/vtb.h"

typedef unsigned short (*ref_decision)(void *, short *, short *);

extern unsigned short ref_FAX_FSE_decision_16pt(void *s, short *a, short *m);
extern unsigned short ref_FAX_FSE_decision_32pt(void *s, short *a, short *m);
extern unsigned short ref_FAX_FSE_decision_64pt(void *s, short *a, short *m);
extern unsigned short ref_FAX_FSE_decision_128pt(void *s, short *a, short *m);
extern unsigned short ref_FAX_FSE_decision_AB(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_eqtrn(void *s, short *a, short *m);
extern unsigned short ref_FSE_Bridge_det(void *s, short *a, short *m);

extern const fpm_fse_decision ref_FSEv17_decision[4];

extern const short ref_DECv17_ANA_IMAP128[32];
extern const short ref_DECv17_ANA_QMAP128[32];
extern const short ref_DECv17_ANA_QMAP[8];
extern const short ref_DECv17_MAP_TRN[4];
extern const short ref_DECv17_ANGL4800[4];
extern const short ref_DECv17_MAG7200[3];

extern const short ref_VTBv17_IMAP16T[17];
extern const short ref_VTBv17_QMAP16T[17];
extern const short ref_VTBv17_IMAP32[33];
extern const short ref_VTBv17_QMAP32[33];
extern const short ref_VTBv17_IMAP64[65];
extern const short ref_VTBv17_QMAP64[65];
extern const short ref_VTBv17_IMAP128[129];
extern const short ref_VTBv17_QMAP128[129];

extern const short ref_VTB_REGION_7200[8];
extern const short ref_VTB_REGION_9600[32];
extern const short ref_VTB_REGION_12000[72];
extern const short ref_VTB_REGION_14400[128];
extern const short ref_VTB_BOUND_7200[128];
extern const short ref_VTB_BOUND_9600[416];
extern const short ref_VTB_BOUND_12000[960];
extern const short ref_VTB_BOUND_14400[1856];

extern unsigned int ref_dsplibs_debug_level;

#define NSYM	64

static struct fpm_fse st_a, st_b;
static struct v17_dec dec_a, dec_b;
static short i_a[NSYM], q_a[NSYM], i_b[NSYM], q_b[NSYM];
static struct vtb_path paths_a[128], paths_b[128];

/* Copies for the whole-object comparisons, with every pointer field zeroed. */
static struct fpm_fse fse_ca, fse_cb;

static unsigned seed;

static int
rnd(int range)
{
	seed = seed * 1103515245u + 12345u;
	return (int)((seed >> 13) % (unsigned)range);
}

/* ------------------------------------------------------------------ */
/* Which slicer a `cfg.decision` names, on whichever side it came from */
/* ------------------------------------------------------------------ */

static int
slicer_id(fpm_fse_decision f)
{
	if (f == 0)
		return 0;
	if (f == FAX_FSE_decision_16pt
	    || f == (fpm_fse_decision)ref_FAX_FSE_decision_16pt)
		return 1;
	if (f == FAX_FSE_decision_32pt
	    || f == (fpm_fse_decision)ref_FAX_FSE_decision_32pt)
		return 2;
	if (f == FAX_FSE_decision_64pt
	    || f == (fpm_fse_decision)ref_FAX_FSE_decision_64pt)
		return 3;
	if (f == FAX_FSE_decision_128pt
	    || f == (fpm_fse_decision)ref_FAX_FSE_decision_128pt)
		return 4;
	if (f == FAX_FSE_decision_AB
	    || f == (fpm_fse_decision)ref_FAX_FSE_decision_AB)
		return 5;
	if (f == FSE_decision_eqtrn
	    || f == (fpm_fse_decision)ref_FSE_decision_eqtrn)
		return 6;
	if (f == FSE_Bridge_det
	    || f == (fpm_fse_decision)ref_FSE_Bridge_det)
		return 7;
	return -1;
}

/* ------------------------------------------------------------------ */
/* The fixture                                                        */
/* ------------------------------------------------------------------ */

/*
 * `V17RX_create` is not reconstructed, so the Viterbi state is set up here the
 * way that function sets it up: the tables `v17cfg.c`'s banner names, `nsub`
 * 1..4, `grid = 2 * nsub`, `mask = (1 << (nsub + 2)) - 1`, `shift = nsub`,
 * `depth = 16`.  That is `VTBv32_init`'s own arithmetic over V.17's copies of
 * the maps and V.32's own `bound` and `region`, which `t_v17cfg.c` establishes
 * independently of anything here.
 *
 * `side` picks whose tables the decoder reads: ours for the reconstruction and
 * the blob's for `ref_VTB_decoder`.  Both sets hold the same numbers -- that
 * is `t_v17cfg`'s claim, not this file's -- so the two decoders see the same
 * constellation, and the survivor comparison below is what proves it.
 */
static void
vtb_setup(struct vtb *v, struct vtb_path *paths, int nsub, int side)
{
	memset(v, 0, sizeof(*v));
	memset(paths, 0, 128 * sizeof(*paths));

	v->paths = paths;
	v->ring = 0;
	v->prev = 0;
	v->depth = 0x10;
	v->nsub = (unsigned short)nsub;
	v->grid = (short)(2 * nsub);
	v->mask = (unsigned short)((1 << (nsub + 2)) - 1);
	v->shift = (short)nsub;

	switch (nsub) {
	case 1:
		v->imap = side ? ref_VTBv17_IMAP16T : VTBv17_IMAP16T;
		v->qmap = side ? ref_VTBv17_QMAP16T : VTBv17_QMAP16T;
		v->bound = side ? ref_VTB_BOUND_7200 : VTB_BOUND_7200;
		v->region = side ? ref_VTB_REGION_7200 : VTB_REGION_7200;
		break;
	case 2:
		v->imap = side ? ref_VTBv17_IMAP32 : VTBv17_IMAP32;
		v->qmap = side ? ref_VTBv17_QMAP32 : VTBv17_QMAP32;
		v->bound = side ? ref_VTB_BOUND_9600 : VTB_BOUND_9600;
		v->region = side ? ref_VTB_REGION_9600 : VTB_REGION_9600;
		break;
	case 3:
		v->imap = side ? ref_VTBv17_IMAP64 : VTBv17_IMAP64;
		v->qmap = side ? ref_VTBv17_QMAP64 : VTBv17_QMAP64;
		v->bound = side ? ref_VTB_BOUND_12000 : VTB_BOUND_12000;
		v->region = side ? ref_VTB_REGION_12000 : VTB_REGION_12000;
		break;
	default:
		v->imap = side ? ref_VTBv17_IMAP128 : VTBv17_IMAP128;
		v->qmap = side ? ref_VTBv17_QMAP128 : VTBv17_QMAP128;
		v->bound = side ? ref_VTB_BOUND_14400 : VTB_BOUND_14400;
		v->region = side ? ref_VTB_REGION_14400 : VTB_REGION_14400;
		break;
	}
}

/*
 * Zero the fixture completely, then plant.  `nsub` 0 means "no trellis": the
 * three handshake slicers never call `VTB_decoder`, and leaving `paths` NULL
 * would turn a wrong reading that DID call it into a segfault rather than a
 * silent pass -- which is the outcome we want, so it is deliberate.
 */
static void
setup(int nsub, short rate, int short_train)
{
	memset(&st_a, 0, sizeof(st_a));
	memset(&st_b, 0, sizeof(st_b));
	memset(&dec_a, 0, sizeof(dec_a));
	memset(&dec_b, 0, sizeof(dec_b));
	memset(i_a, 0, sizeof(i_a));
	memset(q_a, 0, sizeof(q_a));
	memset(i_b, 0, sizeof(i_b));
	memset(q_b, 0, sizeof(q_b));

	st_a.cfg.owner = &dec_a;
	st_b.cfg.owner = &dec_b;
	st_a.out_i = i_a;
	st_a.out_q = q_a;
	st_b.out_i = i_b;
	st_b.out_q = q_b;

	dec_a.rate = rate;
	dec_b.rate = rate;
	dec_a.short_train = short_train;
	dec_b.short_train = short_train;

	if (nsub) {
		vtb_setup((struct vtb *)(void *)dec_a.vtb, paths_a, nsub, 0);
		vtb_setup((struct vtb *)(void *)dec_b.vtb, paths_b, nsub, 1);
	}
}

/* ------------------------------------------------------------------ */
/* The comparison                                                     */
/* ------------------------------------------------------------------ */

static void
blank_fse(struct fpm_fse *dst, const struct fpm_fse *src)
{
	/*
	 * `memcpy` and not struct assignment: assignment is not required to
	 * copy PADDING, and `diff_eq_obj` compares every byte.  Under a
	 * member-wise copy the pad bytes of the two copies would be
	 * independent stack garbage and this would fail for a reason that has
	 * nothing to do with the slicers.
	 */
	memcpy(dst, src, sizeof(*dst));

	dst->cfg.icoff = 0;
	dst->cfg.qcoff = 0;
	dst->cfg.clk = 0;
	dst->cfg.pll_k1 = 0;
	dst->cfg.pll_k2 = 0;
	dst->cfg.owner = 0;
	dst->cfg.decision = 0;
	dst->cfg.reserved34 = 0;
	dst->out_i = 0;
	dst->out_q = 0;
	dst->icoeff = 0;
	dst->qcoeff = 0;
	dst->hist = 0;
}

static void
compare(int nsub, long n)
{
	struct v17_dec ca, cb;
	struct vtb *va = (struct vtb *)(void *)dec_a.vtb;
	struct vtb *vb = (struct vtb *)(void *)dec_b.vtb;
	int j;

	memcpy(&ca, &dec_a, sizeof(ca));
	memcpy(&cb, &dec_b, sizeof(cb));
	memset(ca.vtb, 0, sizeof(ca.vtb));
	memset(cb.vtb, 0, sizeof(cb.vtb));
	diff_eq_obj("receiver object, Viterbi window blanked", struct v17_dec,
		    &ca, &cb, n);

	blank_fse(&fse_ca, &st_a);
	blank_fse(&fse_cb, &st_b);
	diff_eq_obj("equaliser block, pointers blanked", struct fpm_fse,
		    &fse_ca, &fse_cb, n);

	diff_eq_int("cfg.decision identity (%ld)", slicer_id(st_a.cfg.decision),
		    slicer_id(st_b.cfg.decision), n);
	diff_eq_int("cfg.decision is a slicer we know (%ld)",
		    slicer_id(st_a.cfg.decision) >= 0, 1, n);

	if (!nsub)
		return;

	diff_eq_int("vtb ring (%ld)", va->ring, vb->ring, n);
	diff_eq_int("vtb prev (%ld)", va->prev, vb->prev, n);
	for (j = 0; j < 8; j++)
		diff_eq_int("vtb metric (%ld)", va->metric[j], vb->metric[j],
			    n * 8 + j);
	for (j = 0; j < 128; j++) {
		diff_eq_int("vtb survivor sym (%ld)", va->paths[j].sym,
			    vb->paths[j].sym, n * 128 + j);
		diff_eq_int("vtb survivor surv (%ld)", va->paths[j].surv,
			    vb->paths[j].surv, n * 128 + j);
	}
}

/*
 * One symbol through both sides.  `ang` and `mag` are IN/OUT, so both sides
 * get their own copy of the same input value.
 */
static unsigned short
step(fpm_fse_decision ours, ref_decision theirs, int nsub, int slot, short ai,
     short aq, short ang, short mag, long n)
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
	compare(nsub, n);
	return rb;
}

/* ------------------------------------------------------------------ */
/* Independent copies of the region trees, for BINNING ONLY           */
/* ------------------------------------------------------------------ */

/*
 * These decide which LEAF an input reaches; they never decide a symbol, and
 * nothing they return is compared against either side.  They exist so that the
 * scan can report which leaves it covered and so that the V.32-threshold
 * variant below can be differenced against the V.17 one.
 */
static int
oracle_rotate(int i, int q, short *ri, short *rq)
{
	int ti = i < 0 ? -i : i;
	int tq = q < 0 ? -q : q;
	short ai = (short)ti;
	short aq = (short)tq;
	int sel = (ai > aq) + (i > q ? 2 : 0);
	int c = DECv17_COS_ROT_ANGLE[sel];
	int s = DECv17_SIN_ROT_ANGLE[sel];

	*ri = (short)(((i * c) >> 15) - ((q * s) >> 15));
	*rq = (short)(((q * c) >> 15) + ((i * s) >> 15));
	return sel;
}

/* 64pt: sixteen cells, reported as base/4 so the leaf index is 0..15. */
static int
oracle_64(int i, int q)
{
	int base;

	if (i > 0) {
		if (q > 0)
			base = i > 0x2000 ? (q > 0x1fff ? 0x28 : 0x2c)
					  : (q > 0x1fff ? 0x20 : 0x24);
		else
			base = i > 0x2000 ? (q >= -0x2000 ? 0x38 : 0x3c)
					  : (q >= -0x2000 ? 0x30 : 0x34);
	} else {
		if (q > 0)
			base = i > -0x2000 ? (q > 0x1fff ? 0x08 : 0x0c)
					   : (q > 0x1fff ? 0x00 : 0x04);
		else
			base = i > -0x2000 ? (q >= -0x2000 ? 0x18 : 0x1c)
					   : (q >= -0x2000 ? 0x10 : 0x14);
	}
	return base / 4;
}

/* 32pt: three bands, reported 0..2. */
static int
oracle_32(int i, int q)
{
	short ri, rq;

	(void)oracle_rotate(i, q, &ri, &rq);
	if (ri <= 0x16a0)
		return 0;
	if (ri > 0x2d41 && rq <= 0x2d40)
		return 2;
	return 1;
}

/*
 * 128pt: ten search leaves plus the two that skip the search.  `v32` selects
 * V.32bis' thresholds instead of V.17's, which is what layer 4 differences
 * against.  Leaves are numbered:
 *
 *   0..9   the ten search cells, in the order 0x00 0x04 0x08 0x0c 0x0e 0x10
 *          0x14 0x18 0x1a 0x1c
 *   10     amb = OUTER
 *   11     amb = INNER
 */
static const short leaf128_base[10] = {
	0x00, 0x04, 0x08, 0x0c, 0x0e, 0x10, 0x14, 0x18, 0x1a, 0x1c
};

static int
oracle_128(int i, int q, int v32)
{
	short ri, rq;
	int base = 0, j;

	(void)oracle_rotate(i, q, &ri, &rq);

	if (v32 ? (ri > 0x16a0) : (ri > 0x16a1)) {
		base = 0x0c;
		if (ri > 0x2d41) {
			if (rq <= 0x2d41)
				base = v32 ? (rq <= 0x16a0 ? 0x1c : 0x18)
					   : (rq < 0x16a2 ? 0x1c : 0x18);
			else if (v32 ? (ri <= 0x3891) : (ri <= 0x3892)) {
				if (v32 ? (rq > 0x3891) : (rq > 0x3892))
					base = 0x0e;
				else
					return 11;
			} else {
				if (v32 ? (rq < 0x3891) : (rq <= 0x3891))
					base = 0x1a;
				else
					return 10;
			}
		} else if (rq <= 0x2d40) {
			base = v32 ? (rq <= 0x169f ? 0x14 : 0x10)
				   : (rq > 0x16a0 ? 0x10 : 0x14);
		}
	} else if (rq <= 0x2d40) {
		base = v32 ? (rq <= 0x169f ? 0x08 : 0x04)
			   : (rq <= 0x16a0 ? 0x08 : 0x04);
	}

	for (j = 0; j < 10; j++)
		if (leaf128_base[j] == base)
			return j;
	return -1;
}

/* ------------------------------------------------------------------ */
/* Layer 1: the shape of the object the slicers see                   */
/* ------------------------------------------------------------------ */

static int
run_shape(void)
{
	diff_begin("v17_dec: the region of the receiver state the slicers see");

#define OFF(f) ((long)((char *)&dec_a.f - (char *)&dec_a))
	diff_eq_int("sizeof (%ld)", (long)sizeof(struct v17_dec), 0x6c, 0);
	diff_eq_int("vtb (%ld)", OFF(vtb), 0x04, 0);
	diff_eq_int("vtb is exactly struct vtb (%ld)",
		    (long)sizeof(struct vtb), 0x38, 0);
	diff_eq_int("vtb array matches it (%ld)", (long)sizeof(dec_a.vtb),
		    0x38, 0);
	diff_eq_int("ang_prev (%ld)", OFF(ang_prev), 0x3c, 0);
	diff_eq_int("sym_i (%ld)", OFF(sym_i), 0x3e, 0);
	diff_eq_int("sym_q (%ld)", OFF(sym_q), 0x40, 0);
	diff_eq_int("sym_i1 (%ld)", OFF(sym_i1), 0x42, 0);
	diff_eq_int("sym_q1 (%ld)", OFF(sym_q1), 0x44, 0);
	diff_eq_int("sym_i2 (%ld)", OFF(sym_i2), 0x46, 0);
	diff_eq_int("sym_q2 (%ld)", OFF(sym_q2), 0x48, 0);
	diff_eq_int("eqm_a (%ld)", OFF(eqm_a), 0x4a, 0);
	diff_eq_int("eqm_b (%ld)", OFF(eqm_b), 0x4c, 0);
	diff_eq_int("int_0050 (%ld)", OFF(int_0050), 0x50, 0);
	diff_eq_int("count (%ld)", OFF(count), 0x5a, 0);
	diff_eq_int("scram (%ld)", OFF(scram), 0x5c, 0);
	diff_eq_int("short_train (%ld)", OFF(short_train), 0x60, 0);
	diff_eq_int("rate (%ld)", OFF(rate), 0x64, 0);
	diff_eq_int("short_0066 (%ld)", OFF(short_0066), 0x66, 0);
	diff_eq_int("sym_count (%ld)", OFF(sym_count), 0x68, 0);
#undef OFF

	/*
	 * The region ends where `V17RXS_MRF` begins.  `owner` is the receiver
	 * state + 0x2c and the MRF block is at + 0x98, so the object leaves
	 * exactly 0x6c bytes here and a struct of any other size is a wrong
	 * reading of `V17RX_create` whatever else agrees.
	 */
	diff_eq_int("0x2c + sizeof reaches V17RXS_MRF (%ld)",
		    0x2c + (long)sizeof(struct v17_dec), 0x98, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* Layer 2: FSEv17_decision is four function pointers, in rate order  */
/* ------------------------------------------------------------------ */

static int
run_table(void)
{
	static const int want[4] = { 1, 2, 3, 4 };	/* 16, 32, 64, 128 */
	int j;

	diff_begin("FSEv17_decision: four function pointers, in rate order");

	/*
	 * If this table were `short[8]` -- which its sixteen bytes read as
	 * data would suggest -- `sizeof` would still be 16 and this check
	 * would still pass.  What could not pass is the identity below.
	 */
	diff_eq_int("sixteen bytes (%ld)", (long)sizeof(FSEv17_decision), 16,
		    0);
	diff_eq_int("four elements (%ld)",
		    (long)(sizeof(FSEv17_decision) / sizeof(FSEv17_decision[0])),
		    4, 0);

	for (j = 0; j < 4; j++) {
		diff_eq_int("ours names the right slicer (%ld)",
			    slicer_id(FSEv17_decision[j]), want[j], j);
		diff_eq_int("the blob's names the same one (%ld)",
			    slicer_id(ref_FSEv17_decision[j]), want[j], j);
		/*
		 * And the two are not the SAME pointer, which is what says
		 * each side is running its own build.  If they were equal the
		 * whole differential tier would be comparing the blob with
		 * itself.
		 */
		diff_eq_int("the two sides are distinct copies (%ld)",
			    FSEv17_decision[j]
			    == ref_FSEv17_decision[j], 0, j);
	}

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* Layer 3: the four rate slicers, with leaf coverage asserted        */
/* ------------------------------------------------------------------ */

/*
 * The scan.  A coarse lattice over the plane the equaliser's output can
 * occupy, plus every exact constellation coordinate and every threshold value
 * on both axes, because the interesting boundaries are one value wide.
 */
#define SCAN_N	4096

static short scan_i[SCAN_N], scan_q[SCAN_N];
static int scan_n;

static void
scan_add(int i, int q)
{
	if (scan_n < SCAN_N) {
		scan_i[scan_n] = (short)i;
		scan_q[scan_n] = (short)q;
		scan_n++;
	}
}

static void
build_scan(void)
{
	static const int edge[] = {
		-32768, -30000, -24576, -20481, -20480, -16384, -14482,
		-14481, -12288, -11586, -11585, -8193, -8192, -8191, -5793,
		-5792, -4096, -2048, -1, 0, 1, 2048, 4096, 5792, 5793, 5794,
		8191, 8192, 8193, 11584, 11585, 11586, 12288, 14481, 14482,
		14483, 16384, 20480, 20481, 24576, 30000, 32767
	};
	int ne = (int)(sizeof(edge) / sizeof(edge[0]));
	int a, b;

	scan_n = 0;
	for (a = 0; a < ne; a++)
		for (b = 0; b < ne; b++)
			scan_add(edge[a], edge[b]);
}

/*
 * One rate slicer over the scan, one leaf at a time.  `leaf_of` bins the
 * input, `nleaf` is the denominator this run must report, and at most `per`
 * inputs per leaf are driven -- enough that the Viterbi history is deep and
 * bounded so the run stays short.
 *
 * MULTI-BLOCK BY CONSTRUCTION (finding F8790): the trials for a slicer share
 * one fixture and one `struct vtb`, so trial k is decided against sixteen
 * symbols of history that trials k-16..k-1 built.  A single call cannot see
 * state that only diverges after several symbols, and the survivor ring is
 * exactly that state.
 */
static int
run_rate(const char *name, fpm_fse_decision ours, ref_decision theirs,
	 int nsub, int (*leaf_of)(int, int), int nleaf, int per)
{
	static int counts[16];
	int j, k, slot = 0;
	long n = 0;

	diff_begin(name);
	setup(nsub, 0, 0);

	for (j = 0; j < nleaf; j++)
		counts[j] = 0;

	for (k = 0; k < scan_n; k++) {
		int leaf = leaf_of(scan_i[k], scan_q[k]);

		if (leaf < 0 || leaf >= nleaf)
			continue;
		if (counts[leaf] >= per)
			continue;
		counts[leaf]++;

		step(ours, theirs, nsub, slot, scan_i[k], scan_q[k],
		     (short)rnd(65536), (short)rnd(32768), n);
		n++;
		slot = (slot + 1) % NSYM;
	}

	/*
	 * The denominator, and the anti-vacuity check.  A run that reached
	 * eight of sixteen cells would otherwise report a clean pass over half
	 * the region tree -- finding F134.
	 */
	diff_eq_int("trials driven (%ld)", n > 0, 1, 0);
	for (j = 0; j < nleaf; j++)
		diff_eq_int("leaf reached (%ld)", counts[j] > 0, 1, j);

	return diff_end();
}

/* `_16pt` has no region tree; bin by the magnitude ring instead. */
static int
oracle_16_ring(int i, int q)
{
	int best = 0, k, ib, qb, sum;
	short min = 0x7fff;

	for (k = 0; k <= 15; k++) {
		int di = (short)(i - DECv17_IMAP16[k]);
		int dq = (short)(q - DECv17_QMAP16[k]);
		short d = (short)(((di * di) >> 16) + ((dq * dq) >> 16));

		if (d < min) {
			best = k;
			min = d;
		}
	}
	ib = DECv17_IMAP16[best];
	qb = DECv17_QMAP16[best];
	sum = (ib < 0 ? -ib : ib) + (qb < 0 ? -qb : qb);
	return (sum >> 13) - 1;
}

/*
 * `_16pt` again, binned by the DECIDED POINT rather than by the ring, so that
 * all sixteen angles are driven.  Two passes over one slicer, because one
 * binning cannot assert both denominators.
 */
static int
oracle_16_point(int i, int q)
{
	int best = 0, k;
	short min = 0x7fff;

	for (k = 0; k <= 15; k++) {
		int di = (short)(i - DECv17_IMAP16[k]);
		int dq = (short)(q - DECv17_QMAP16[k]);
		short d = (short)(((di * di) >> 16) + ((dq * dq) >> 16));

		if (d < min) {
			best = k;
			min = d;
		}
	}
	return best;
}

static int
oracle_128_v17(int i, int q)
{
	return oracle_128(i, q, 0);
}

/* ------------------------------------------------------------------ */
/* Layer 4: the seven thresholds that are NOT V.32bis'                */
/* ------------------------------------------------------------------ */

/*
 * The sharpest check in the file, and the one that would catch reading
 * `src/pump/v32/v32fse.c` across instead of the disassembly.
 *
 * WHICH of the seven V.17-specific cuts an input sits EXACTLY on.  Every one of
 * them is a place where V.17's threshold is one higher than V.32bis', so every
 * one is ONE VALUE WIDE in the ROTATED frame and no sampled input finds it.
 * The comment beside each names the two spellings.
 */
static int
threshold_line(int i, int q)
{
	short ri, rq;

	(void)oracle_rotate(i, q, &ri, &rq);

	if (ri == 0x16a1)
		return 0;			/* > 0x16a1  vs  > 0x16a0   */
	if (ri > 0x16a1) {
		if (ri > 0x2d41) {
			if (rq <= 0x2d41) {
				if (rq == 0x16a1)
					return 1;  /* < 0x16a2 vs <= 0x16a0 */
			} else if (ri == 0x3892) {
				return 2;	   /* <= 0x3892 vs <= 0x3891 */
			} else if (ri < 0x3892) {
				if (rq == 0x3892)
					return 3;  /* > 0x3892 vs > 0x3891   */
			} else {
				if (rq == 0x3891)
					return 4;  /* <= 0x3891 vs < 0x3891  */
			}
		} else if (rq <= 0x2d40) {
			if (rq == 0x16a0)
				return 5;	   /* > 0x16a0 vs <= 0x169f  */
		}
	} else if (rq <= 0x2d40) {
		if (rq == 0x16a0)
			return 6;		   /* <= 0x16a0 vs <= 0x169f */
	}
	return -1;
}

/*
 * THE COVERAGE IS PER LINE AND NOT AN AGGREGATE, AND THAT IS NOT A STYLE
 * CHOICE.  This layer was first written as "scan for inputs the V.17 and V.32
 * oracles disagree about, drive them, assert the count is at least seven".
 * It ran green and the injection that put V.32bis' `rq < 0x3891` back was NOT
 * CAUGHT: six of the seven lines were being reached and the seventh, which
 * needs `rq == 0x3891` under `ri > 0x3892`, first occurs at (-12219, -32700) --
 * a radius of 34900 in the received frame, outside the +-18000 window that
 * version scanned.  An aggregate count of 240 hid a zero.  Finding F134 in its
 * purest form, and finding F9417.
 *
 * So the lattice now runs to +-32700 and every line reports its own
 * denominator.  `i` steps by one because the rotated coordinates move by about
 * 0.707 per unit of `i`; `q` steps by a coarse stride because `ri - rq` is
 * fixed for a given `q` and it is `i` that sweeps along a line.
 */
static int
run_128_thresholds(void)
{
	int counts[7];
	int i, q, j, slot = 0;
	long n = 0;

	diff_begin("_128pt: the seven cuts that are V.17's and not V.32bis'");
	setup(4, 0, 0);

	for (j = 0; j < 7; j++)
		counts[j] = 0;

	for (q = -32700; q <= 32700; q += 97) {
		for (i = -32700; i <= 32700; i++) {
			int line = threshold_line(i, q);

			if (line < 0 || counts[line] >= 20)
				continue;
			counts[line]++;
			(void)step(FAX_FSE_decision_128pt,
				   ref_FAX_FSE_decision_128pt, 4, slot,
				   (short)i, (short)q, (short)rnd(65536),
				   (short)rnd(32768), n);
			n++;
			slot = (slot + 1) % NSYM;
		}
	}

	for (j = 0; j < 7; j++)
		diff_eq_int("cut driven on its exact value (%ld)",
			    counts[j] > 0, 1, j);
	diff_eq_int("trials driven (%ld)", n > 0, 1, 0);

	/*
	 * And the aggregate the first version relied on, kept as a second
	 * reading of the same thing rather than as the whole of it.
	 */
	diff_eq_int("the two oracles disagree somewhere (%ld)",
		    oracle_128(-12219, -32700, 0)
		    != oracle_128(-12219, -32700, 1), 1, 0);

	return diff_end();
}

/*
 * `_64pt`'s two axes split at different places, one value apart, and the
 * rotation's `sel` turns on `|i| > |q|`.  Those are constructed rather than
 * sampled: a uniform draw essentially never lands on them.
 */
static int
run_64_axes(void)
{
	static const short pt[][2] = {
		{  0x2000,  0x2000 }, {  0x2000,  0x1fff }, {  0x2001,  0x2000 },
		{ -0x2000, -0x2000 }, { -0x2001, -0x2000 }, { -0x2000, -0x2001 },
		{  0x2000, -0x2000 }, { -0x2000,  0x2000 }, {       0,       0 },
		{       1,       0 }, {       0,       1 }, {      -1,      -1 },
		{  0x2001,  0x2001 }, { -0x1fff, -0x1fff }, {  0x1fff,  0x1fff }
	};
	int k, np = (int)(sizeof(pt) / sizeof(pt[0]));
	long n = 0;

	diff_begin("_64pt: the two axes on their exact split values");
	setup(3, 0, 0);

	for (k = 0; k < np; k++) {
		step(FAX_FSE_decision_64pt, ref_FAX_FSE_decision_64pt, 3,
		     k % NSYM, pt[k][0], pt[k][1], (short)rnd(65536),
		     (short)rnd(32768), n);
		n++;
	}
	diff_eq_int("exact-value trials driven (%ld)", n, np, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* Layer 5: the handshake, walked through cfg.decision                */
/* ------------------------------------------------------------------ */

/*
 * Follow whatever each side installed rather than calling a function this file
 * chose.  `slicer_id` has already been asserted equal on both sides by
 * `compare`, so following ours and theirs independently walks the same chain.
 */
static void
walk(int nsub, int nstep, short ang0, long *n, int *ids, int nids)
{
	int k, slot = 0;
	short ang = ang0;

	for (k = 0; k < nstep; k++) {
		fpm_fse_decision fa = st_a.cfg.decision;
		fpm_fse_decision fb = st_b.cfg.decision;
		int id = slicer_id(fa);

		if (id >= 0 && id < nids)
			ids[id]++;

		/*
		 * The AB segment's decision is the SIGN OF THE PHASE STEP, so
		 * the angle has to alternate by more and by less than half a
		 * cycle for both arms to fire.  A constant angle exercises one
		 * arm and reports full coverage.
		 */
		ang = (short)(ang + ((k & 1) ? 0x5000 : 0x2000));

		/*
		 * The inputs are bounded at +-15000 rather than the full short
		 * range, because `FAX_FSE_decision_AB` sums two squared
		 * differences in an `int` and the object's `add` is allowed to
		 * wrap where C's is not.  15000 keeps every partial sum inside
		 * 2**31 without narrowing what the decision can see.
		 */
		(void)step(fa, (ref_decision)fb, nsub, slot,
			   (short)(rnd(30000) - 15000),
			   (short)(rnd(30000) - 15000), ang,
			   (short)(rnd(20000) + 1000), *n);
		(*n)++;
		slot = (slot + 1) % NSYM;
	}
}

static int
run_handshake(int short_train, short rate)
{
	int ids[8];
	int j;
	long n = 0;

	diff_begin(short_train ? "handshake walk, short training"
				: "handshake walk, long training");

	for (j = 0; j < 8; j++)
		ids[j] = 0;

	/*
	 * The trellis is set up for the rate under test even though only the
	 * handshake slicers run here: the walk ENDS by installing
	 * `FSEv17_decision[rate]`, and if a wrong reading let it be called the
	 * decoder must be valid rather than NULL.
	 */
	setup((int)rate + 1, rate, short_train);

	/*
	 * AB's exit needs `count > 0xc8` as well as an open constellation, and
	 * `eqtrn`'s long arm needs 0xb9f symbols after that.  Both are walked
	 * rather than planted here; the planted-count cases are below.
	 */
	st_a.cfg.decision = FAX_FSE_decision_AB;
	st_b.cfg.decision = (fpm_fse_decision)ref_FAX_FSE_decision_AB;

	walk((int)rate + 1, short_train ? 700 : 3600, 0, &n, ids, 8);

	diff_eq_int("AB ran (%ld)", ids[5] > 0, 1, rate);
	diff_eq_int("eqtrn ran (%ld)", ids[6] > 0, 1, rate);
	diff_eq_int("the bridge ran iff training was long (%ld)",
		    ids[7] > 0, !short_train, rate);
	diff_eq_int("the rate slicer was reached (%ld)",
		    ids[1 + rate] > 0, 1, rate);
	diff_eq_int("nothing unrecognised was installed (%ld)",
		    slicer_id(st_a.cfg.decision) >= 0, 1, rate);
	diff_eq_int("trials driven (%ld)", n > 0, 1, rate);

	return diff_end();
}

/*
 * The three handshake slicers again with their counters PLANTED at each
 * boundary, so that the exact symbol a handover happens on is compared rather
 * than only the fact that it eventually did.  A reading with any of the three
 * limits off by one passes the walk above and fails here.
 */
static int
run_handshake_edges(void)
{
	static const short bridge_at[] = { 0x3c, 0x3d, 0x3e, 0x3f, 0x40 };
	static const short eqtrn_short[] = { 0x22, 0x23, 0x24, 0x25, 0x26 };
	static const short eqtrn_long[] = { 0xb9c, 0xb9d, 0xb9e, 0xb9f, 0xba0 };
	static const short ab_at[] = { 0xc6, 0xc7, 0xc8, 0xc9, 0xca };
	int k, r;
	long n = 0;

	diff_begin("handshake: the three segment limits, planted");

	for (r = 0; r < 4; r++) {
		for (k = 0; k < 5; k++) {
			setup(r + 1, (short)r, 0);
			dec_a.count = dec_b.count = bridge_at[k];
			dec_a.scram = dec_b.scram = 0x00bb3754;
			step(FSE_Bridge_det, ref_FSE_Bridge_det, r + 1, 0,
			     (short)(rnd(20000) - 10000),
			     (short)(rnd(20000) - 10000), (short)rnd(65536),
			     (short)rnd(32768), n);
			n++;

			setup(r + 1, (short)r, 1);
			dec_a.count = dec_b.count = eqtrn_short[k];
			dec_a.scram = dec_b.scram = 0x00bb3754;
			step(FSE_decision_eqtrn, ref_FSE_decision_eqtrn,
			     r + 1, 0, 0, 0, (short)rnd(65536),
			     (short)rnd(32768), n);
			n++;

			setup(r + 1, (short)r, 0);
			dec_a.count = dec_b.count = eqtrn_long[k];
			dec_a.scram = dec_b.scram = 0x00bb3754;
			step(FSE_decision_eqtrn, ref_FSE_decision_eqtrn,
			     r + 1, 0, 0, 0, (short)rnd(65536),
			     (short)rnd(32768), n);
			n++;

			setup(r + 1, (short)r, r & 1);
			dec_a.count = dec_b.count = ab_at[k];
			dec_a.sym_i1 = dec_b.sym_i1 = 9000;
			dec_a.sym_q1 = dec_b.sym_q1 = -9000;
			dec_a.sym_i2 = dec_b.sym_i2 = -9000;
			dec_a.sym_q2 = dec_b.sym_q2 = 9000;
			dec_a.eqm_a = dec_b.eqm_a = 40;
			dec_a.eqm_b = dec_b.eqm_b = 40;
			step(FAX_FSE_decision_AB, ref_FAX_FSE_decision_AB,
			     r + 1, 0, 12000, -12000, (short)rnd(65536),
			     (short)rnd(32768), n);
			n++;
		}
	}

	diff_eq_int("planted-boundary trials driven (%ld)", n, 80, 0);
	return diff_end();
}

/*
 * `FSE_decision_eqtrn` runs its own generator and ignores the received symbol
 * entirely, so it needs a seeded register and a long run rather than a wide
 * input set.  This is also the only place `DECv17_MAP_TRN` is indexed, so the
 * four values it can produce are counted.
 */
static int
run_eqtrn_generator(void)
{
	int seen[4];
	int k, j;
	long n = 0;

	diff_begin("eqtrn: the TRN generator and DECv17_MAP_TRN's four values");

	for (j = 0; j < 4; j++)
		seen[j] = 0;

	setup(4, 3, 0);
	dec_a.scram = dec_b.scram = 0x00bb3754;

	for (k = 0; k < 200; k++) {
		unsigned short r;

		/*
		 * Held short of the 0xb9f handover so the GENERATOR, and not
		 * the handover, is what this measures.  `count` is compared by
		 * `compare` on every step like everything else; forcing it
		 * back afterwards on BOTH sides keeps the comparison honest.
		 */
		r = step(FSE_decision_eqtrn, ref_FSE_decision_eqtrn, 0, 0, 0,
			 0, 0, 0, n);
		if (r < 4)
			seen[r]++;
		dec_a.count = dec_b.count = 0;
		n++;
	}

	for (j = 0; j < 4; j++)
		diff_eq_int("MAP_TRN value produced (%ld)", seen[j] > 0, 1, j);
	diff_eq_int("generator trials driven (%ld)", n, 200, 0);

	return diff_end();
}

/*
 * AB's EXIT THRESHOLD, driven across its exact crossing.
 *
 * `thr` is `((eqm_a^2 + eqm_b^2) >> 15) * 21845 >> 14` -- four thirds in Q14 --
 * where V.32bis' copy of the same test shifts left by two and does a real
 * signed divide by three (`imul $0x55555556`).  The two agree over most of the
 * range, so the walks above cannot tell 21845 from 21846: THAT INJECTION WAS
 * NOT CAUGHT until this layer existed.  Finding F9417.
 *
 * The crossing is found with a binary search and then swept.  With the
 * two-back terms zeroed, `sum` is `((sym_i1 - i)^2 + (sym_q1 - q)^2) >> 15`,
 * and it changes by AT MOST ONE per unit of the QUADRATURE difference, so
 * sweeping that walks `sum` through consecutive integers and cannot step over
 * the threshold.  Sweeping the in-phase one instead moves `sum` by two at a
 * time at these magnitudes and would step over it -- which is why the sweep is
 * on `dq` and the search on `di`.
 *
 * THE BLOB IS THE ORACLE FOR WHERE TO LOOK AND NOT FOR WHAT IS RIGHT.  The
 * binary search only chooses the sample window; every sample in it is then
 * compared against the blob like everything else.  The assertion that BOTH
 * outcomes occurred is what says the window bracketed the crossing -- without
 * it a window that missed would report a clean pass over nothing.
 */
static void
ab_plant(short e, int di, int dq)
{
	setup(0, 1, 0);
	dec_a.eqm_a = dec_b.eqm_a = e;
	dec_a.eqm_b = dec_b.eqm_b = e;
	dec_a.count = dec_b.count = 0xc9;	/* past FSE_AB_EXIT_SYMBOLS */
	i_a[0] = i_b[0] = (short)-di;
	q_a[0] = q_b[0] = (short)-dq;
}

/* Did the BLOB take the exit?  It zeroes `count` there and nowhere else. */
static int
ab_exits(short e, int di, int dq)
{
	short ang = 0x5000, mag = 0;

	ab_plant(e, di, dq);
	st_b.n_out = 0;
	(void)ref_FAX_FSE_decision_AB(&st_b, &ang, &mag);
	return dec_b.count == 0;
}

static int
run_ab_threshold(void)
{
	static const short E[] = { 2000, 8000, 14000, 19000, 20000 };
	int ne = (int)(sizeof(E) / sizeof(E[0]));
	int k, dq, lo, hi, di0;
	long n = 0;

	diff_begin("AB: the exit threshold, swept across its exact crossing");

	for (k = 0; k < ne; k++) {
		int taken = 0, missed = 0;

		lo = 0;
		hi = 32700;
		while (lo < hi) {
			int mid = (lo + hi) / 2;

			if (ab_exits(E[k], mid, 0))
				hi = mid;
			else
				lo = mid + 1;
		}
		di0 = lo;
		diff_eq_int("a crossing exists inside the range (%ld)",
			    di0 > 0 && di0 < 32700, 1, k);
		if (di0 < 8 || di0 >= 32700)
			continue;

		for (dq = 0; dq <= 1400; dq++) {
			short ang = 0x5000;

			ab_plant(E[k], di0 - 4, dq);
			(void)step(FAX_FSE_decision_AB,
				   ref_FAX_FSE_decision_AB, 0, 0,
				   (short)-(di0 - 4), (short)-dq, ang, 0, n);
			n++;
			if (dec_b.count == 0)
				taken++;
			else
				missed++;
		}
		diff_eq_int("the sweep saw the exit taken (%ld)", taken > 0, 1,
			    k);
		diff_eq_int("and saw it not taken (%ld)", missed > 0, 1, k);
	}

	diff_eq_int("threshold trials driven (%ld)", n > 0, 1, 0);
	return diff_end();
}

/*
 * `sym_count`'s WRAP, which nothing above can reach.
 *
 * Five of the six slicers replace 0x8000 with 0x4000 on the way past and
 * `FAX_FSE_decision_AB` does not, and 32768 symbols is further than any run in
 * this file goes -- so a reconstruction that dropped the test, or that wrapped
 * to zero, or that gave AB the test as well, would pass every layer above.
 * The counter is therefore PLANTED at the boundary and one symbol driven
 * through each of the seven functions.
 *
 * 0xffff is driven too, because a reading with the test written as `>= 0x8000`
 * rather than `== 0x8000` agrees at 0x7fff and disagrees here.
 */
static int
run_sym_count(void)
{
	static const unsigned short at[] = {
		0x0000, 0x3fff, 0x4000, 0x7ffe, 0x7fff, 0x8000, 0xbfff, 0xfffe,
		0xffff
	};
	int nat = (int)(sizeof(at) / sizeof(at[0]));
	int k;
	long n = 0;

	diff_begin("sym_count: the 0x8000 -> 0x4000 wrap, planted");

	for (k = 0; k < nat; k++) {
#define AT(fn, ref, nsub, ai, aq)					\
		do {							\
			setup((nsub), 1, 0);				\
			dec_a.sym_count = dec_b.sym_count = at[k];	\
			dec_a.scram = dec_b.scram = 0x00bb3754;		\
			(void)step((fn), (ref), (nsub), 0, (ai), (aq),	\
				   (short)rnd(65536), (short)rnd(32768),\
				   n);					\
			n++;						\
		} while (0)

		AT(FAX_FSE_decision_16pt, ref_FAX_FSE_decision_16pt, 1,
		   5000, -5000);
		AT(FAX_FSE_decision_32pt, ref_FAX_FSE_decision_32pt, 2,
		   9000, 3000);
		AT(FAX_FSE_decision_64pt, ref_FAX_FSE_decision_64pt, 3,
		   -7000, 11000);
		AT(FAX_FSE_decision_128pt, ref_FAX_FSE_decision_128pt, 4,
		   13000, -2000);
		AT(FSE_Bridge_det, ref_FSE_Bridge_det, 1, 4096, 4096);
		AT(FSE_decision_eqtrn, ref_FSE_decision_eqtrn, 1, 0, 0);
		AT(FAX_FSE_decision_AB, ref_FAX_FSE_decision_AB, 1, 1000,
		   -1000);
#undef AT
	}

	diff_eq_int("wrap trials driven (%ld)", n, (long)nat * 7, 0);

	/*
	 * AND THE ASYMMETRY IS ASSERTED DIRECTLY, because "both sides agree"
	 * would be satisfied if both wrapped.  One step of `_16pt` and one of
	 * AB from 0x7fff, read off the BLOB's object.
	 */
	setup(1, 1, 0);
	dec_a.sym_count = dec_b.sym_count = 0x7fff;
	(void)step(FAX_FSE_decision_16pt, ref_FAX_FSE_decision_16pt, 1, 0,
		   5000, -5000, 0, 0, n);
	diff_eq_int("a rate slicer pulls 0x8000 back to 0x4000 (%ld)",
		    dec_b.sym_count, 0x4000, 0);
	n++;

	setup(1, 1, 0);
	dec_a.sym_count = dec_b.sym_count = 0x7fff;
	(void)step(FAX_FSE_decision_AB, ref_FAX_FSE_decision_AB, 1, 0, 1000,
		   -1000, 0, 0, n);
	diff_eq_int("AB does not (%ld)", dec_b.sym_count, 0x8000, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* Layer 6: the debug arm, and the clamp that guards it               */
/* ------------------------------------------------------------------ */

/*
 * `_128pt` clamps its table index and reports the clamp through
 * `dsplibs_debug_printf`.  ON EVERY REACHABLE INPUT THE CLAMP CANNOT FIRE:
 * the region tree bounds `best` at 0x1f and `rot` at 3, so `n` is at most 127.
 * That is asserted here over the whole scan with the denominator reported,
 * because "the debug arm never fired" is only meaningful beside how many
 * inputs were asked.
 *
 * Both debug levels are driven anyway, because the GATE is reachable even when
 * the arm behind it is not -- a reading that put the clamp test on the wrong
 * side of the level test would change behaviour at level 2 and not at 0.
 */
static int
run_debug(void)
{
	int k, slot = 0, checked = 0;
	long n = 0;

	diff_begin("_128pt: the index clamp and its debug report");

	setup(4, 3, 0);

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	for (k = 0; k < scan_n; k += 7) {
		step(FAX_FSE_decision_128pt, ref_FAX_FSE_decision_128pt, 4,
		     slot, scan_i[k], scan_q[k], (short)rnd(65536),
		     (short)rnd(32768), n);
		n++;
		slot = (slot + 1) % NSYM;
	}

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = 2;
	ref_dsplibs_debug_level = 2;
	for (k = 3; k < scan_n; k += 7) {
		step(FAX_FSE_decision_128pt, ref_FAX_FSE_decision_128pt, 4,
		     slot, scan_i[k], scan_q[k], (short)rnd(65536),
		     (short)rnd(32768), n);
		n++;
		slot = (slot + 1) % NSYM;
	}
	diff_eq_int("both sides printed the same number of lines (%ld)",
		    dsplib_debug_capture_lines(0),
		    dsplib_debug_capture_lines(1), 0);
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	/*
	 * And the bound itself, over the whole scan and independently of
	 * either implementation: `best + 32*rot` over the region tree's own
	 * leaves.  The denominator is what makes "it never fired" a result.
	 */
	for (k = 0; k < scan_n; k++) {
		short ri, rq;
		int rot = oracle_rotate(scan_i[k], scan_q[k], &ri, &rq);
		int leaf = oracle_128(scan_i[k], scan_q[k], 0);
		int worst;

		if (leaf < 0)
			continue;
		worst = leaf < 10 ? leaf128_base[leaf] + 3
				  : (leaf == 10 ? 26 : 24);
		checked++;
		diff_eq_int("index stays inside the table (%ld)",
			    worst + 32 * rot <= 127, 1, k);
	}
	diff_eq_int("inputs whose index was bounded (%ld)", checked > 0, 1, 0);
	diff_eq_int("and it was the whole scan (%ld)", checked, scan_n, 0);
	diff_eq_int("debug trials driven (%ld)", n > 0, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* The tables the slicers reach for as literals                       */
/* ------------------------------------------------------------------ */

/*
 * D1200: `_128pt` names four I coordinates as literals and one of them is one
 * greater than the table entry it otherwise equals.  The deviation is what we
 * REPRODUCE, so the check has to be that our table still disagrees with the
 * literal by exactly one -- if the table were ever corrected the deviation
 * would be wrong and this is what says so.
 */
static int
run_literals(void)
{
	diff_begin("_128pt: the four literal I coordinates, and D1200");

	diff_eq_int("ANA_IMAP128[14] equals its literal (%ld)",
		    DECv17_ANA_IMAP128[14], 0x2799, 14);
	diff_eq_int("ANA_IMAP128[15] equals its literal (%ld)",
		    DECv17_ANA_IMAP128[15], 0x2799, 15);
	diff_eq_int("ANA_IMAP128[24] equals its literal (%ld)",
		    DECv17_ANA_IMAP128[24], 0x32e9, 24);
	diff_eq_int("ANA_IMAP128[26] is one BELOW its literal (%ld)",
		    DECv17_ANA_IMAP128[26] + 1, 0x3e3a, 26);

	/* `_32pt`'s two are exact, and are the mirror pair in the octant. */
	diff_eq_int("ANA_QMAP[3] is _32pt's point-6 literal (%ld)",
		    DECv17_ANA_QMAP[3], 0x3891, 3);
	diff_eq_int("ANA_QMAP[6] is _32pt's point-3 literal (%ld)",
		    DECv17_ANA_QMAP[6], 0x21f1, 6);

	/*
	 * The handshake magnitude is written as the immediate 0x3299 in all
	 * three of AB, the bridge and eqtrn, and it is `DECv17_MAG7200[1]`.
	 * That is the corroboration for both the constant and the table.
	 */
	diff_eq_int("the handshake magnitude is MAG7200[1] (%ld)",
		    DECv17_MAG7200[1], 0x3299, 1);

	return diff_end();
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	seed = 20260901u;
	build_scan();

	rc |= run_shape();
	rc |= run_table();
	rc |= run_literals();

	rc |= run_rate("_16pt: all three magnitude rings",
		       FAX_FSE_decision_16pt, ref_FAX_FSE_decision_16pt, 1,
		       oracle_16_ring, 3, 24);
	rc |= run_rate("_16pt: all sixteen decided points",
		       FAX_FSE_decision_16pt, ref_FAX_FSE_decision_16pt, 1,
		       oracle_16_point, 16, 8);
	rc |= run_rate("_32pt: the three bands", FAX_FSE_decision_32pt,
		       ref_FAX_FSE_decision_32pt, 2, oracle_32, 3, 40);
	rc |= run_rate("_64pt: all sixteen cells", FAX_FSE_decision_64pt,
		       ref_FAX_FSE_decision_64pt, 3, oracle_64, 16, 12);
	rc |= run_rate("_128pt: all twelve leaves", FAX_FSE_decision_128pt,
		       ref_FAX_FSE_decision_128pt, 4, oracle_128_v17, 12, 12);

	rc |= run_64_axes();
	rc |= run_128_thresholds();

	rc |= run_handshake(1, 0);
	rc |= run_handshake(1, 2);
	rc |= run_handshake(0, 1);
	rc |= run_handshake(0, 3);
	rc |= run_handshake_edges();
	rc |= run_eqtrn_generator();
	rc |= run_ab_threshold();
	rc |= run_sym_count();

	rc |= run_debug();

	return rc;
}
