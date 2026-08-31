/*
 * t_v22fpcreate.c -- V.22: the datapump constructor, whole object.
 *
 * `V22FP_create` builds a graph, not a value, so the test has to be about the
 * graph.  Three things are checked and they answer different questions:
 *
 *   1. EVERY BYTE of the object, its two blocks and each of the four heap
 *      sub-objects, with the pointer fields blanked because two heaps hold
 *      two different addresses and always will.
 *   2. EVERY BUFFER either side allocated, entry by entry, plus each block's
 *      REQUESTED size and its position in the allocation sequence.  A buffer
 *      that is too small is invisible until something reads off the end of
 *      it, and two blocks of the same size swapped is invisible to sizes
 *      alone -- there are two such pairs here (0xf0 twice, 0x62 twice), so
 *      the ordinals are not a formality.
 *   3. WHICH ARGUMENTS ACTUALLY CHANGE THE OBJECT.  A constructor is exactly
 *      the shape finding F3052 warns about: a template copied into the wrong
 *      sub-object leaves the object the right length with the right
 *      allocation count, and a test driven by one configuration cannot see
 *      it.  So the sweep below counts, per axis, how many of its trials moved
 *      the object at all, and asserts that count -- measured on the BLOB's
 *      object, so it is a statement about the original and not about us.
 *
 * WHAT NO TEST HERE CAN SEPARATE, said out loud rather than left to look like
 * coverage:
 *
 *   - `params.bps2` is unconditionally a copy of `params.bps`, so `dsp->r28`
 *     and `dsp->r2a` are always equal and swapping either pair is invisible.
 *   - `TONEv22_CFG` and `TONEv22INIT_CFG` are byte-identical, so which feeds
 *     `hdx->tone` and which feeds the throwaway generator cannot be told.
 *   - The mode-2 arm's write of 1 to `params.r14` is invisible: the only
 *     template there is already holds 1.
 *   - The mask widths in the flags patch (`& 0xf3`, `& 0xfd`) are invisible:
 *     the bits they clear beyond the three being set are already zero.
 *   - `dsp->scrambler_on`, `r1c` and `r20` read bits 0, 1 and 2 of `params.flags`, and
 *     create patches only bits 9, 10 and 11 -- so those three always carry the
 *     template's 1, 1, 0 and NO configuration can vary them.  With the
 *     template at 0x65b, bits 0, 1, 3, 4 and 6 all read 1 and bits 2, 5, 7 and
 *     8 all read 0, so several wrong shift amounts produce a byte-identical
 *     object.  The reading is forced by the disassembly and is not testable,
 *     which is worth saying because a wrong shift is exactly the transcription
 *     slip that would survive here.
 *
 * The two coefficient pairs ARE separable, and only by content: I is the
 * cosine arm and Q the sine, so the test asserts that each pair's two halves
 * differ from EACH OTHER as well as agreeing across the two sides.  Without
 * that the content check would pass vacuously if the carrier degenerated.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22tab.h"
#include "dsplib/v22txtab.h"

extern void *ref_V22FP_create(void *fp, const void *cfg);
extern void ref_V22FP_delete(void *fp);

/*
 * The blob's own copies of everything create installs a POINTER to.  A
 * pointer field cannot be compared across the two sides -- they hold two
 * different addresses -- so each side is checked against ITS OWN symbol
 * instead.  Without this, blanking the field to compare the rest of the
 * struct would leave it checked by nothing at all, which is exactly what a
 * "template copied into the wrong sub-object" defect hides behind.
 */
extern unsigned short ref_FSEv22_decision12(struct v22_fse *state,
					    short *angle, short *mag);
extern const short ref_SMCv22_IMAP_1200BPS[16];
extern const short ref_SMCv22_QMAP_1200BPS[16];
extern const short ref_MTDv22_COEF[];
extern const short ref_MTDv22_COEF2[];
extern const short ref_V22_S1_HC_COEF[];

/* ------------------------------------------------------------------------ */

#define MAX_REGIONS 40

struct region {
	const char *name;
	const void *p;
	unsigned long n;
};

/*
 * Every heap region the graph owns, in a fixed order, so two graphs can be
 * walked in step.  Nothing here is a pointer field -- these are the bytes
 * behind the pointers, which is what has to agree.
 */
static int
regions_of(struct v22fp *fp, struct region *r)
{
	struct v22fp_dsp *d = fp->dsp;
	struct v22fp_hdx *h = fp->hdx;
	int n = 0;

#define REGION(nm, ptr, bytes) \
	do { r[n].name = (nm); r[n].p = (ptr); r[n].n = (bytes); n++; } while (0)

	REGION("dsp->ra8", d->ra8, 0x18);
	REGION("dsp->rx_scratch", d->rx_scratch, 0x154);
	REGION("hdx->iir", h->iir, 0x20);
	REGION("dsp->pps_coff_i", d->pps_coff_i, V22_PPS_COEFFS * 2);
	REGION("dsp->pps_coff_q", d->pps_coff_q, V22_PPS_COEFFS * 2);
	REGION("dsp->mrf_coeff", d->mrf_coeff, V22_MRF_COEFFS * 2);
	REGION("dsp->fse_coff_i", d->fse_coff_i, V22_FSE_TAPS * 2);
	REGION("dsp->fse_coff_q", d->fse_coff_q, V22_FSE_TAPS * 2);

	REGION("pps.hist_i", d->pps.hist_i, V22_PPS_HISTORY * 2);
	REGION("pps.hist_q", d->pps.hist_q, V22_PPS_HISTORY * 2);
	REGION("mrf.history", d->mrf.history, V22_MRF_HISTORY * 2);
	REGION("sre.coeff", d->sre.coeff, V22_SRE_COEFFS * 2);
	REGION("sre.hist", d->sre.hist, V22_SRE_HIST * 2);
	REGION("sre.clk", d->sre.clk, V22_SRE_CLOCK * 2);

	REGION("fse.out_i", d->fse.out_i, V22_FSE_OUT * 2);
	REGION("fse.out_q", d->fse.out_q, V22_FSE_OUT * 2);
	REGION("fse.icoeff", d->fse.icoeff, V22_FSE_TAPS * 2);
	REGION("fse.qcoeff", d->fse.qcoeff, V22_FSE_TAPS * 2);
	REGION("fse.hist", d->fse.hist, V22_FSE_HIST * 2);
	REGION("fse.r44", d->fse.r44, V22_FSE_AUX * 2);
	REGION("fse.r48", d->fse.r48, V22_FSE_AUX * 2);

	REGION("tone.kernel", h->tone->kernel, (unsigned long)h->tone->cfg.len * 2);
	REGION("tone.history", h->tone->history,
	       (unsigned long)(h->tone->cfg.len + h->tone->cfg.extra) * 2);
	REGION("tone.rev_block", h->tone->rev_block, 10);
	REGION("tone.rev_acc", h->tone->rev_acc, 8);

	REGION("mtd.acc", h->mtd->acc, (unsigned long)h->mtd->cfg.tones * 4);
	REGION("mtd_s1.acc", h->mtd_s1->acc,
	       (unsigned long)h->mtd_s1->cfg.tones * 4);
	REGION("mtd2.acc", h->mtd2->acc, (unsigned long)h->mtd2->cfg.tones * 4);
#undef REGION
	return n;
}

/*
 * The eleven allocations `V22FP_create` makes directly, in the order it makes
 * them.  The sizes are the literals in its own `sysdep_malloc` calls; the
 * ordinals are what says a same-sized pair did not get swapped.
 */
struct direct {
	const char *name;
	const void *p;
	unsigned want;
};

static int
directs_of(struct v22fp *fp, struct direct *d)
{
	int n = 0;

#define DIRECT(nm, ptr, sz) \
	do { d[n].name = (nm); d[n].p = (ptr); d[n].want = (sz); n++; } while (0)

	DIRECT("object", fp, 0x5c);
	DIRECT("dsp", fp->dsp, 0x1f8);
	DIRECT("hdx", fp->hdx, 0x40);
	DIRECT("hdx->iir", fp->hdx->iir, 0x20);
	DIRECT("dsp->rx_scratch", fp->dsp->rx_scratch, 0x154);
	DIRECT("dsp->ra8", fp->dsp->ra8, 0x18);
	DIRECT("dsp->pps_coff_i", fp->dsp->pps_coff_i, 0xf0);
	DIRECT("dsp->pps_coff_q", fp->dsp->pps_coff_q, 0xf0);
	DIRECT("dsp->mrf_coeff", fp->dsp->mrf_coeff, 0x21c);
	DIRECT("dsp->fse_coff_i", fp->dsp->fse_coff_i, 0x62);
	DIRECT("dsp->fse_coff_q", fp->dsp->fse_coff_q, 0x62);
#undef DIRECT
	return n;
}

/* ------------------------------------------------------------------------ */

/* Copies with every pointer blanked, so the rest can be compared entire. */
static struct v22fp
blank_obj(const struct v22fp *fp)
{
	struct v22fp c = *fp;

	c.out_i = NULL;
	c.out_q = NULL;
	c.n_out = NULL;
	c.icoeff = NULL;
	c.qcoeff = NULL;
	c.hdx = NULL;
	c.dsp = NULL;
	return c;
}

static struct v22fp_hdx
blank_hdx(const struct v22fp_hdx *h)
{
	struct v22fp_hdx c = *h;

	c.tone = NULL;
	c.mtd = NULL;
	c.mtd_s1 = NULL;
	c.mtd2 = NULL;
	c.iir = NULL;
	return c;
}

static struct v22fp_dsp
blank_dsp(const struct v22fp_dsp *d)
{
	struct v22fp_dsp c = *d;

	c.ra8 = NULL;
	c.rx_scratch = NULL;
	c.pps_coff_i = NULL;
	c.pps_coff_q = NULL;
	c.mrf_coeff = NULL;
	c.fse_coff_i = NULL;
	c.fse_coff_q = NULL;
	c.pps.cfg.coeff_i = NULL;
	c.pps.cfg.coeff_q = NULL;
	c.pps.hist_i = NULL;
	c.pps.hist_q = NULL;
	c.pps.imap = NULL;
	c.pps.qmap = NULL;
	c.smc.cfg.pmap = NULL;
	c.smc.cfg.imap = NULL;
	c.smc.cfg.qmap = NULL;
	c.agc.cfg.alpha = NULL;
	c.agc.cfg.beta = NULL;
	c.agc2.cfg.alpha = NULL;
	c.agc2.cfg.beta = NULL;
	c.mrf.cfg.coeff = NULL;
	c.mrf.history = NULL;
	c.sre.coeff = NULL;
	c.sre.hist = NULL;
	c.sre.clk = NULL;
	c.fse.icoff = NULL;
	c.fse.qcoff = NULL;
	c.fse.out_i = NULL;
	c.fse.out_q = NULL;
	c.fse.icoeff = NULL;
	c.fse.qcoeff = NULL;
	c.fse.hist = NULL;
	c.fse.r44 = NULL;
	c.fse.r48 = NULL;
	c.fse.prev_quad = NULL;
	c.fse.decision = NULL;
	return c;
}

static struct fpm_tone
blank_tone(const struct fpm_tone *t)
{
	struct fpm_tone c = *t;

	c.cfg.src = NULL;
	c.kernel = NULL;
	c.history = NULL;
	c.rev_block = NULL;
	c.rev_acc = NULL;
	c.iir_self = NULL;
	return c;
}

static struct fpm_mtd
blank_mtd(const struct fpm_mtd *m)
{
	struct fpm_mtd c = *m;

	c.cfg.coeff = NULL;
	c.acc = NULL;
	return c;
}

/* ------------------------------------------------------------------------ */

/*
 * One check per buffer, naming the first differing byte.  A per-byte
 * `diff_eq_int` would put twelve thousand checks in the ledger and drown the
 * ten-line report.
 */
static void
cmp_raw(const char *what, const void *a, const void *b, unsigned long n,
	long input)
{
	const unsigned char *pa = (const unsigned char *)a;
	const unsigned char *pb = (const unsigned char *)b;
	unsigned long i;

	for (i = 0; i < n; i++)
		if (pa[i] != pb[i])
			break;
	/* -1 means "identical to the end"; anything else is an offset. */
	diff_eq_int(what, i == n ? -1L : (long)i, -1L, input);
}

/* FNV-1a, so a whole graph reduces to one number the sweep can compare. */
static void
mix(unsigned long *h, const void *p, unsigned long n)
{
	const unsigned char *b = (const unsigned char *)p;
	unsigned long i;

	for (i = 0; i < n; i++) {
		*h ^= b[i];
		*h *= 16777619UL;
	}
}

static unsigned long
fingerprint(struct v22fp *fp)
{
	struct region r[MAX_REGIONS];
	struct v22fp o = blank_obj(fp);
	struct v22fp_hdx h = blank_hdx(fp->hdx);
	struct v22fp_dsp d = blank_dsp(fp->dsp);
	struct fpm_tone t = blank_tone(fp->hdx->tone);
	struct fpm_mtd m0 = blank_mtd(fp->hdx->mtd);
	struct fpm_mtd m1 = blank_mtd(fp->hdx->mtd_s1);
	struct fpm_mtd m2 = blank_mtd(fp->hdx->mtd2);
	unsigned long hash = 2166136261UL;
	int n, i;

	mix(&hash, &o, sizeof(o));
	mix(&hash, &h, sizeof(h));
	mix(&hash, &d, sizeof(d));
	mix(&hash, &t, sizeof(t));
	mix(&hash, &m0, sizeof(m0));
	mix(&hash, &m1, sizeof(m1));
	mix(&hash, &m2, sizeof(m2));

	n = regions_of(fp, r);
	for (i = 0; i < n; i++)
		mix(&hash, r[i].p, r[i].n);
	return hash;
}

/* ------------------------------------------------------------------------ */

static struct v22fp_cfg
base_cfg(void)
{
	struct v22fp_cfg c;

	/* What `v22_create` passes, with mode and rate left to the caller. */
	c.mode = 0;
	c.rate = 0;
	c.f08 = 60000;
	c.f0c = 0;
	c.f10 = 700;
	c.f14 = 0;
	c.f18 = 1;
	return c;
}

/*
 * Build the same configuration on both sides in one allocator epoch and
 * compare everything.  Ours runs second, so its ordinals are the reference's
 * plus the number the reference took; `base` carries that offset.
 */
static void
compare_one(const struct v22fp_cfg *cfg, const char *what, long idx)
{
	struct region ra[MAX_REGIONS];
	struct region rb[MAX_REGIONS];
	struct direct da[16];
	struct direct db[16];
	struct v22fp *r;
	struct v22fp *o;
	struct v22fp ba, bb;
	struct v22fp_hdx ha, hb;
	struct v22fp_dsp dda, ddb;
	struct fpm_tone ta, tb;
	struct fpm_mtd ma, mb;
	int base, n, m, i;

	harness_alloc_reset();
	r = (struct v22fp *)ref_V22FP_create(0, cfg);
	base = harness_alloc.allocs;
	o = V22FP_create(0, cfg);

	diff_eq_int("same number of allocations", harness_alloc.allocs - base,
		    base, idx);

	/* --- the three structs, entire, pointers blanked --------------- */
	ba = blank_obj(o);
	bb = blank_obj(r);
	diff_eq_obj(what, struct v22fp, &ba, &bb, idx);

	ha = blank_hdx(o->hdx);
	hb = blank_hdx(r->hdx);
	diff_eq_obj(what, struct v22fp_hdx, &ha, &hb, idx);

	dda = blank_dsp(o->dsp);
	ddb = blank_dsp(r->dsp);
	diff_eq_obj(what, struct v22fp_dsp, &dda, &ddb, idx);

	ta = blank_tone(o->hdx->tone);
	tb = blank_tone(r->hdx->tone);
	diff_eq_obj(what, struct fpm_tone, &ta, &tb, idx);

	ma = blank_mtd(o->hdx->mtd);
	mb = blank_mtd(r->hdx->mtd);
	diff_eq_obj(what, struct fpm_mtd, &ma, &mb, idx);
	ma = blank_mtd(o->hdx->mtd_s1);
	mb = blank_mtd(r->hdx->mtd_s1);
	diff_eq_obj(what, struct fpm_mtd, &ma, &mb, idx);
	ma = blank_mtd(o->hdx->mtd2);
	mb = blank_mtd(r->hdx->mtd2);
	diff_eq_obj(what, struct fpm_mtd, &ma, &mb, idx);

	/* --- every buffer, entry by entry ------------------------------ */
	n = regions_of(o, ra);
	m = regions_of(r, rb);
	diff_eq_int("same number of heap regions", n, m, idx);
	for (i = 0; i < n && i < m; i++) {
		diff_eq_int(ra[i].name, (long)ra[i].n, (long)rb[i].n, idx);
		cmp_raw(ra[i].name, ra[i].p, rb[i].p, ra[i].n, idx);
	}

	/* --- sizes AND positions of the eleven direct allocations ------ */
	n = directs_of(o, da);
	m = directs_of(r, db);
	for (i = 0; i < n && i < m; i++) {
		diff_eq_int(da[i].name, (long)harness_alloc_reqsize(da[i].p),
			    (long)da[i].want, idx);
		diff_eq_int(db[i].name, (long)harness_alloc_reqsize(db[i].p),
			    (long)da[i].want, idx);
		/*
		 * Position in the sequence, which is what a swap of the two
		 * 0xf0 buffers or the two 0x62 buffers shows up in and
		 * nothing else does.
		 */
		diff_eq_int(da[i].name,
			    (long)(harness_alloc_ordinal(da[i].p) - base),
			    (long)harness_alloc_ordinal(db[i].p), idx);
	}

	/*
	 * --- the relationships inside one side -------------------------
	 *
	 * Asserted on the REFERENCE, so they are statements about the object.
	 * These are what a template copied into the wrong sub-object breaks:
	 * the addresses still exist and the counts still balance, but the
	 * equalities do not hold.
	 */
	diff_eq_int("ref: obj.out_i is fse.out_i",
		    r->out_i == r->dsp->fse.out_i, 1, idx);
	diff_eq_int("ref: obj.out_q is fse.out_q",
		    r->out_q == r->dsp->fse.out_q, 1, idx);
	diff_eq_int("ref: obj.n_out is &fse.n_out",
		    r->n_out == &r->dsp->fse.n_out, 1, idx);
	diff_eq_int("ref: obj.icoeff is fse.icoeff",
		    r->icoeff == r->dsp->fse.icoeff, 1, idx);
	diff_eq_int("ref: obj.qcoeff is fse.qcoeff",
		    r->qcoeff == r->dsp->fse.qcoeff, 1, idx);
	diff_eq_int("ref: fse.prev_quad is &dsp.prev_quad",
		    r->dsp->fse.prev_quad == &r->dsp->prev_quad, 1, idx);
	diff_eq_int("ref: pps.cfg.coeff_i is dsp.pps_coff_i",
		    r->dsp->pps.cfg.coeff_i == r->dsp->pps_coff_i, 1, idx);
	diff_eq_int("ref: pps.cfg.coeff_q is dsp.pps_coff_q",
		    r->dsp->pps.cfg.coeff_q == r->dsp->pps_coff_q, 1, idx);
	diff_eq_int("ref: mrf.cfg.coeff is dsp.mrf_coeff",
		    r->dsp->mrf.cfg.coeff == r->dsp->mrf_coeff, 1, idx);
	diff_eq_int("ref: fse.icoff is dsp.fse_coff_i",
		    r->dsp->fse.icoff == r->dsp->fse_coff_i, 1, idx);
	diff_eq_int("ref: fse.qcoff is dsp.fse_coff_q",
		    r->dsp->fse.qcoff == r->dsp->fse_coff_q, 1, idx);
	/*
	 * The pointers that are BLANKED above, each against its own side's
	 * symbol.  `fse.decision` is the sharp one: create installs the
	 * 1200 bit/s slicer whatever the rate, and `SetRxRate` is what moves
	 * it to `FSEv22_decision24` -- so an implementation that installed
	 * the 2400 one here would agree on every other byte of the object.
	 */
	diff_eq_int("ref: fse.decision is FSEv22_decision12",
		    r->dsp->fse.decision == ref_FSEv22_decision12, 1, idx);
	diff_eq_int("ours: fse.decision is FSEv22_decision12",
		    o->dsp->fse.decision == FSEv22_decision12, 1, idx);
	/*
	 * Both maps are [16], so no size or count check separates them; the
	 * pulse shaper never reads them either, it carries them for its
	 * consumer.  Nothing but this would notice a swap.
	 */
	diff_eq_int("ref: pps.imap is SMCv22_IMAP_1200BPS",
		    r->dsp->pps.imap == ref_SMCv22_IMAP_1200BPS, 1, idx);
	diff_eq_int("ref: pps.qmap is SMCv22_QMAP_1200BPS",
		    r->dsp->pps.qmap == ref_SMCv22_QMAP_1200BPS, 1, idx);
	diff_eq_int("ours: pps.imap is SMCv22_IMAP_1200BPS",
		    o->dsp->pps.imap == SMCv22_IMAP_1200BPS, 1, idx);
	diff_eq_int("ours: pps.qmap is SMCv22_QMAP_1200BPS",
		    o->dsp->pps.qmap == SMCv22_QMAP_1200BPS, 1, idx);
	/*
	 * Which detector configuration reached which slot.  `MTDv22_CFG` and
	 * `MTDv22_CFG2` differ in `ratio` (29820 against 27980), so a swap of
	 * those two is already caught by the struct comparison -- but a
	 * BANK-only slip, both slots pointed at one coefficient array, is
	 * not, and this is what sees it.
	 */
	diff_eq_int("ref: mtd is MTDv22_CFG's bank",
		    r->hdx->mtd->cfg.coeff == ref_MTDv22_COEF, 1, idx);
	diff_eq_int("ref: mtd_s1 is MTDs1_CFG's bank",
		    r->hdx->mtd_s1->cfg.coeff == ref_V22_S1_HC_COEF, 1, idx);
	diff_eq_int("ref: mtd2 is MTDv22_CFG2's bank",
		    r->hdx->mtd2->cfg.coeff == ref_MTDv22_COEF2, 1, idx);
	diff_eq_int("ours: mtd is MTDv22_CFG's bank",
		    o->hdx->mtd->cfg.coeff == MTDv22_COEF, 1, idx);
	diff_eq_int("ours: mtd_s1 is MTDs1_CFG's bank",
		    o->hdx->mtd_s1->cfg.coeff == V22_S1_HC_COEF, 1, idx);
	diff_eq_int("ours: mtd2 is MTDv22_CFG2's bank",
		    o->hdx->mtd2->cfg.coeff == MTDv22_COEF2, 1, idx);

	/* And ours holds the same relationships, not merely the same bytes. */
	diff_eq_int("ours: obj.n_out is &fse.n_out",
		    o->n_out == &o->dsp->fse.n_out, 1, idx);
	diff_eq_int("ours: fse.prev_quad is &dsp.prev_quad",
		    o->dsp->fse.prev_quad == &o->dsp->prev_quad, 1, idx);
	diff_eq_int("ours: mrf.cfg.coeff is dsp.mrf_coeff",
		    o->dsp->mrf.cfg.coeff == o->dsp->mrf_coeff, 1, idx);

	/*
	 * The two same-size pairs are separable ONLY by content -- I is the
	 * cosine arm, Q the sine.  Assert they differ from each other, or the
	 * content comparisons above would pass vacuously on a degenerate
	 * carrier and the swap would be invisible after all.
	 */
	diff_eq_int("ref: pps I and Q differ",
		    memcmp(r->dsp->pps_coff_i, r->dsp->pps_coff_q,
			   V22_PPS_COEFFS * 2) != 0, 1, idx);
	diff_eq_int("ref: fse I and Q differ",
		    memcmp(r->dsp->fse_coff_i, r->dsp->fse_coff_q,
			   V22_FSE_TAPS * 2) != 0, 1, idx);

	V22FP_delete(o);
	ref_V22FP_delete(r);
	diff_eq_int("both sides balance", harness_alloc.live, 0, idx);
	diff_eq_int("no bad frees", harness_alloc.bad_free, 0, idx);
}

/* Build on the blob's side only, fingerprint it, tear it down. */
static unsigned long
ref_print(const struct v22fp_cfg *cfg)
{
	struct v22fp *r;
	unsigned long h;

	harness_alloc_reset();
	r = (struct v22fp *)ref_V22FP_create(0, cfg);
	h = fingerprint(r);
	ref_V22FP_delete(r);
	return h;
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;
	long k;

	/*
	 * 1. Six configurations, every byte and every buffer.  The first is
	 *    what `v22_create` actually passes; the rest walk the two
	 *    selectors and the three flag bits, including the out-of-range
	 *    values that select nothing.
	 */
	diff_begin("V22FP_create: the whole object, six configurations");
	{
		static const struct {
			int mode, rate, f0c, f14, f18;
			const char *name;
		} arms[] = {
			{ 0, 0, 0, 0, 1, "mode 0, 2400, as v22_create" },
			{ 1, 1, 0, 0, 1, "mode 1, 1200" },
			{ 2, 2, 0, 0, 1, "mode 2, 1200" },
			{ 0, 0, 1, 1, 0, "mode 0, 2400, flags inverted" },
			{ 5, 7, 0, 0, 1, "both selectors out of range" },
			{ 1, 0, 2, 2, 2, "flag words with bit 0 clear" }
		};

		for (k = 0; k < (long)(sizeof(arms) / sizeof(arms[0])); k++) {
			struct v22fp_cfg c = base_cfg();

			c.mode = arms[k].mode;
			c.rate = arms[k].rate;
			c.f0c = arms[k].f0c;
			c.f14 = arms[k].f14;
			c.f18 = arms[k].f18;
			compare_one(&c, arms[k].name, k);
		}
	}
	rc |= diff_end();

	/*
	 * 2. `cfg.f08` and `cfg.f10` reach the object as a whole int and as a
	 *    truncated short respectively.  The second is the sharp one: an
	 *    int at +0x18 rather than a short would leave 0x0001 where the
	 *    object leaves 0.
	 */
	diff_begin("V22FP_create: the two value-carrying fields");
	{
		struct v22fp_cfg c = base_cfg();

		c.f08 = 0x12345678;
		c.f10 = 0x00012345;
		compare_one(&c, "f08 and f10 swept", 0);

		{
			struct v22fp *r;

			harness_alloc_reset();
			r = (struct v22fp *)ref_V22FP_create(0, &c);
			diff_eq_int("f08 reaches params.r08 whole",
				    r->params.r08, 0x12345678, 0);
			diff_eq_int("f08 reaches hdx.node_deadline too",
				    r->hdx->node_deadline, 0x12345678, 0);
			diff_eq_int("f10 is TRUNCATED into params.carrier_loss_ms",
				    (unsigned short)r->params.carrier_loss_ms, 0x2345, 0);
			diff_eq_int("and nothing lands in params.r1a",
				    r->params.r1a, 0, 0);
			ref_V22FP_delete(r);
		}
	}
	rc |= diff_end();

	/*
	 * 3. THE SEPARATION SWEEP.  For each argument, how many of its values
	 *    actually move the object?  Measured on the blob's object, so a
	 *    zero here would be a statement that the argument is inert and
	 *    that this file's other checks are not testing what they look
	 *    like they are testing.
	 */
	diff_begin("V22FP_create: which arguments change the object");
	{
		struct v22fp_cfg c = base_cfg();
		unsigned long h0, h1, h2, hx;
		int moved;

		/* mode: 0, 1 and 2 are three distinct objects. */
		c = base_cfg();
		c.mode = 0; h0 = ref_print(&c);
		c.mode = 1; h1 = ref_print(&c);
		c.mode = 2; h2 = ref_print(&c);
		moved = (h1 != h0) + (h2 != h0) + (h2 != h1);
		printf("  mode:  %lu %lu %lu\n", h0, h1, h2);
		diff_eq_int("mode gives three distinct objects", moved, 3, 0);
		/* and out of range selects NOTHING, so it is the template's 1 */
		c.mode = 5; hx = ref_print(&c);
		diff_eq_int("mode 5 is mode 1, not mode 0", hx == h1, 1, 0);
		diff_eq_int("mode 5 is not mode 0", hx == h0, 0, 0);

		/* rate: 0 differs from 1, and 1 and 2 are the same 1200. */
		c = base_cfg();
		c.rate = 0; h0 = ref_print(&c);
		c.rate = 1; h1 = ref_print(&c);
		c.rate = 2; h2 = ref_print(&c);
		printf("  rate:  %lu %lu %lu\n", h0, h1, h2);
		diff_eq_int("rate 0 and rate 1 differ", h0 != h1, 1, 0);
		diff_eq_int("rate 1 and rate 2 are the same object",
			    h1 == h2, 1, 0);
		c.rate = 7; hx = ref_print(&c);
		diff_eq_int("rate 7 is rate 0, the template's 2400",
			    hx == h0, 1, 0);

		/* The three flag bits, each on its own. */
		c = base_cfg();
		c.f0c = 0; c.f14 = 0; c.f18 = 0; h0 = ref_print(&c);
		c.f0c = 1; h1 = ref_print(&c); c.f0c = 0;
		diff_eq_int("f0c bit 0 moves the object", h1 != h0, 1, 0);
		c.f0c = 2; hx = ref_print(&c); c.f0c = 0;
		diff_eq_int("f0c 2 selects nothing", hx == h0, 1, 0);

		c.f14 = 1; h1 = ref_print(&c); c.f14 = 0;
		diff_eq_int("f14 bit 0 moves the object", h1 != h0, 1, 0);
		c.f14 = 2; hx = ref_print(&c); c.f14 = 0;
		diff_eq_int("f14 2 selects nothing", hx == h0, 1, 0);

		c.f18 = 1; h1 = ref_print(&c); c.f18 = 0;
		diff_eq_int("f18 bit 0 moves the object", h1 != h0, 1, 0);
		c.f18 = 2; hx = ref_print(&c); c.f18 = 0;
		diff_eq_int("f18 2 selects nothing", hx == h0, 1, 0);

		/* f08 and f10 both reach the object. */
		c = base_cfg();
		h0 = ref_print(&c);
		c.f08 = 1; h1 = ref_print(&c); c.f08 = 60000;
		diff_eq_int("f08 moves the object", h1 != h0, 1, 0);
		c.f10 = 1; h1 = ref_print(&c); c.f10 = 700;
		diff_eq_int("f10 moves the object", h1 != h0, 1, 0);
		c.f0c = 3; h1 = ref_print(&c);
		diff_eq_int("only bit 0 of f0c is read",
			    h1 == ref_print(&c), 1, 0);
	}
	rc |= diff_end();

	/*
	 * 4. f14 bit 0 has a SECOND consequence, and it is the one that pins
	 *    the bit to position 11 rather than a neighbour: it forces
	 *    hdx.protocol to zero whatever the mode selected.  Checked directly
	 *    rather than through a hash, because the mechanism is the point.
	 */
	diff_begin("V22FP_create: flags bit 11 overrides every mode");
	{
		static const int modes[] = { 0, 1, 2 };
		static const int want[] = { 1, 2, 3 };

		for (k = 0; k < 3; k++) {
			struct v22fp_cfg c = base_cfg();
			struct v22fp *r;

			c.mode = modes[k];
			harness_alloc_reset();
			r = (struct v22fp *)ref_V22FP_create(0, &c);
			diff_eq_int("hdx.protocol with the bit clear", r->hdx->protocol,
				    want[k], k);
			diff_eq_int("params.flags bit 11 is clear",
				    (r->params.flags >> 11) & 1, 0, k);
			ref_V22FP_delete(r);

			c.f14 = 1;
			harness_alloc_reset();
			r = (struct v22fp *)ref_V22FP_create(0, &c);
			diff_eq_int("hdx.protocol with the bit set", r->hdx->protocol, 0,
				    k);
			diff_eq_int("params.flags bit 11 is set",
				    (r->params.flags >> 11) & 1, 1, k);
			ref_V22FP_delete(r);
		}
	}
	rc |= diff_end();

	/*
	 * 5. RE-INITIALISATION.  `V22FP_create` tests only `fp == NULL`, so a
	 *    non-NULL argument means an object that already has its tree --
	 *    the one path a caller-supplied buffer cannot take.  Building
	 *    with one configuration and rebuilding with another exercises it,
	 *    and the allocation ledger says whether anything is leaked in the
	 *    process.
	 */
	diff_begin("V22FP_create: re-initialising an existing object");
	{
		struct v22fp_cfg a = base_cfg();
		struct v22fp_cfg b = base_cfg();
		struct v22fp *r;
		struct v22fp *o;
		struct v22fp ba, bb;
		struct v22fp_hdx ha, hb;
		struct v22fp_dsp dda, ddb;
		struct region ra[MAX_REGIONS], rb[MAX_REGIONS];
		int first, n, i;

		b.mode = 1;
		b.rate = 1;
		b.f18 = 0;

		harness_alloc_reset();
		r = (struct v22fp *)ref_V22FP_create(0, &a);
		first = harness_alloc.live;
		printf("  ref: %d allocs to build, %d live\n",
		       harness_alloc.allocs, first);
		r = (struct v22fp *)ref_V22FP_create(r, &b);
		printf("  ref: %d allocs after rebuilding, %d live\n",
		       harness_alloc.allocs, harness_alloc.live);
		/*
		 * The LIVE set is what matters: rebuilding allocates the
		 * throwaway oscillator and its four buffers again and deletes
		 * them again, so `allocs` rises by five while nothing is
		 * retained.  Every other sub-object is reconfigured in place.
		 */
		diff_eq_int("rebuilding retains nothing further",
			    harness_alloc.live, first, 0);
		diff_eq_int("rebuilding costs one throwaway oscillator",
			    harness_alloc.allocs, 45, 0);

		o = V22FP_create(0, &a);
		o = V22FP_create(o, &b);

		ba = blank_obj(o); bb = blank_obj(r);
		diff_eq_obj("re-init", struct v22fp, &ba, &bb, 0);
		ha = blank_hdx(o->hdx); hb = blank_hdx(r->hdx);
		diff_eq_obj("re-init", struct v22fp_hdx, &ha, &hb, 0);
		dda = blank_dsp(o->dsp); ddb = blank_dsp(r->dsp);
		diff_eq_obj("re-init", struct v22fp_dsp, &dda, &ddb, 0);

		n = regions_of(o, ra);
		(void)regions_of(r, rb);
		for (i = 0; i < n; i++)
			cmp_raw(ra[i].name, ra[i].p, rb[i].p, ra[i].n, 0);

		/*
		 * A THIRD build, on the same configuration, still compared
		 * across the two sides.  `V22_MRF_init` permutes its
		 * coefficient array in place, so an implementation that
		 * failed to regenerate the array before each init would come
		 * out permuted twice and diverge here.  It is not a check
		 * that the third build equals the second -- neither side is
		 * asserted to be a fixed point -- only that ours tracks the
		 * object's through the repeat.
		 */
		o = V22FP_create(o, &b);
		r = (struct v22fp *)ref_V22FP_create(r, &b);
		n = regions_of(o, ra);
		(void)regions_of(r, rb);
		for (i = 0; i < n; i++)
			cmp_raw(ra[i].name, ra[i].p, rb[i].p, ra[i].n, 1);

		V22FP_delete(o);
		ref_V22FP_delete(r);
		diff_eq_int("re-init leaks nothing", harness_alloc.live, 0, 0);
		diff_eq_int("re-init frees nothing twice",
			    harness_alloc.bad_free, 0, 0);
	}
	rc |= diff_end();

	return rc;
}
