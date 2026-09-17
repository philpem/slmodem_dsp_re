/*
 * t_v17rxcreate.c -- differential test of `V17RX_create` (.text 0x096eb0,
 *                    3,201 bytes) against the blob's own.
 *
 * The constructor builds a tree of eleven allocations and about 20.5 KB of
 * state: a 0x64-byte instance, a 0x5c-byte control block with two scratch
 * buffers, a 0x4fbc-byte demodulator state with two chained buffers, a
 * survivor ring, an `SGD`, two `fpm_mtd`, an `fpm_tone`, and the five buffers
 * `FPM_MRF_init`, `FPM_SRE_init` and `FPM_FSE_init` hang off their own
 * configurations.  All three blocks are compared IN FULL, and every pointer
 * they hold is followed to what it reaches.
 *
 * ---------------------------------------------------------------------------
 * WHY A BYTE COMPARE OF THREE BLOCKS IS ENOUGH, AND WHERE IT IS NOT
 *
 * The harness fills every fresh `sysdep_malloc` with `HARNESS_MALLOC_FILL`,
 * so the spans this constructor never writes -- and there are thousands of
 * bytes of them, because it does NOT clear the 20 KB state block -- carry the
 * same pattern on both sides and compare equal.  That is what makes "compare
 * everything" meaningful rather than a comparison of two different heaps.
 *
 * POINTER SLOTS ARE SKIPPED AND THEIR TARGETS COMPARED BY CONTENT, which is
 * `t_v21create.c`'s and `t_b103create.c`'s method: the two sides allocate from
 * one allocator at different times, so an address can never be compared, and
 * `cmp_target` follows each slot instead.  It asserts the ALLOCATION SIZE
 * first, from `harness_alloc_reqsize`, because CLAUDE.md's `V90Parameters`
 * example is exactly this shape -- an under-allocation passes every content
 * comparison it is large enough to survive.
 *
 * ---------------------------------------------------------------------------
 * D955 / F8587: THE FIELDS USED AS SUBSCRIPTS, NOT ONLY THOSE DEREFERENCED
 *
 * Three things here are read as an INDEX or a SELECTOR rather than followed,
 * and a blob-against-blob run cannot see any of them go wrong:
 *
 *   - `V17RXC_RATE_CODE`.  It selects the trellis constellation, the quality
 *     threshold and the descrambler's word width, and it indexes nothing --
 *     so `check_shape` asserts the four constellations, the four thresholds
 *     and `nbits` by NAME against each rate, and `rate_arm[]` counts how many
 *     trials reached each arm.
 *   - `struct vtb`'s four table pointers.  `VTB_decoder` subscripts them and
 *     `V17RX_create` only stores them, so they are compared against the
 *     tables they should BE (each side against its own copy) and by content.
 *   - the instance's +0x24, which reaches three configurations' tail slots.
 *     It is given a value with unequal halves so a 32-bit slot written as two
 *     transposed shorts is caught, and it is checked in all three.
 *
 * ---------------------------------------------------------------------------
 * THE CASE THAT CANNOT BE RUN, AND WHY THAT IS THE OBJECT'S FAULT
 *
 * A CALLER-SUPPLIED INSTANCE WITH A NULL STATE POINTER SEGFAULTS ON BOTH
 * SIDES.  `owned` is set only when this call allocated the INSTANCE, and it is
 * what gates the survivor ring's `sysdep_malloc(0x200)` at 0x097568 -- while
 * the state block's own allocation is gated on `V17RX_OBJ_STATE == NULL` at
 * 0x097162.  So a caller-supplied instance whose state is NULL gets a freshly
 * malloc'd state, `owned == 0`, no ring, and then a 512-byte zeroing loop
 * through whatever the allocator left at +0x30.  That is deviation D1222 and
 * it is reproduced; it is also why the re-initialisation case below hands back
 * an instance this test built a moment earlier, which is what the object
 * actually supports.
 *
 * ---------------------------------------------------------------------------
 * `FPM_TONE_CFG` IS COMPARED BEFORE THE CONSTRUCTOR COPIES IT
 *
 * Both copies are the 36-byte configuration structure.  `V17RX_create`
 * copies all nine dwords, so `check_shape` compares `FPM_TONE_CFG` against
 * `ref_FPM_TONE_CFG` byte for byte and follows the embedded prototype pointer
 * separately.  A disagreement is then reported at the table itself before
 * it can surface as an unexplained field in the tone object.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17cfg.h"
#include "dsplib/v17dec.h"
#include "dsplib/v17fax.h"
#include "dsplib/vtb.h"

/* ------------------------------------------------------------------------- */
/* The blob's side                                                           */

extern void *ref_V17RX_create(void *modem, const struct v17rx_cfg *params);
extern void ref_V17RX_delete(void *modem);
extern int ref_V17RX_control(void *modem, const struct v17rx_ctl *arg);
extern int ref_V17RX_modem(void *modem, short *in, short *out,
			   unsigned short *count);
extern short ref_RxHdxStartV17(void *modem, short *in, short *out,
			       unsigned short *count);
extern unsigned short ref_FAX_FSE_decision_AB(struct fpm_fse *state,
					      short *angle, short *mag);
extern struct v17rx_cfg ref_V17RX_CFG;
extern const struct fpm_agc_cfg ref_AGCv17_CFG;
extern struct sgd_cfg ref_SGD_CFG;
extern struct fpm_sdm_cfg ref_SDM_CFG;
extern const struct fpm_tone_cfg ref_FPM_TONE_CFG;
extern unsigned int ref_dsplibs_debug_level;

#define OFF(t, f)	((int)offsetof(t, f))

#define OBJ_SIZE	0x64
#define CTL_SIZE	0x5c
#define RXS_SIZE	0x4fbc

/* The five allocations whose size the constructor states as a literal. */
#define SCRATCH_BYTES	0x140
#define BUF_MRF_BYTES	0x140
#define BUF_SRE_BYTES	0x148
#define PATHS_BYTES	0x200

/* ------------------------------------------------------------------------- */
/* Coverage denominators (F134): counted FROM THE RUN, never from intent      */

static long rate_arm[5];	/* 7200, 9600, 12000, 14400, unrecognised    */
static long vtb_arm[4];		/* nsub 1..4                                 */
static long retrain_arm[2];	/* V17RXC_INT_0010 clear / set               */
static long params_arm[2];	/* explicit table / NULL                     */
static long debug_arm[2];	/* level 0 / level 2                         */
static long path_arm[2];	/* self-allocating / re-initialised          */
static long modem_blocks;
static long cmp_words;		/* words actually compared across all blocks */
static long run_state_moved;	/* state bytes the demodulation actually moved */

/* ------------------------------------------------------------------------- */

static const void *
ptr_at(const void *base, int off)
{
	const void *p;

	memcpy(&p, (const char *)base + off, sizeof p);
	return p;
}

/*
 * Compare a span word by word, skipping both halves of every pointer, and
 * report the FIRST differing word with its offset in the message.  One check
 * per span, and the message carries the word count so a span that compared
 * nothing is visible as such -- a detector must report its denominator.
 */
static void
cmp_span(const char *what, const void *ours, const void *ref, int size,
	 int (*is_ptr)(int), long tag)
{
	const unsigned char *a = (const unsigned char *)ours;
	const unsigned char *b = (const unsigned char *)ref;
	char buf[160];
	int i, n = 0, bad = -1;

	for (i = 0; i + 1 < size; i += 2) {
		if (is_ptr != 0 && (is_ptr(i) || is_ptr(i - 2)))
			continue;
		n++;
		if (*(const short *)(const void *)(a + i)
		    != *(const short *)(const void *)(b + i)) {
			bad = i;
			break;
		}
	}
	cmp_words += n;

	if (bad >= 0) {
		snprintf(buf, sizeof(buf),
			 "%.80s: word 0x%04x of %d compared (%%ld)",
			 what, bad, n);
		diff_eq_int(buf, *(const short *)(const void *)(a + bad),
			    *(const short *)(const void *)(b + bad), tag);
		return;
	}
	snprintf(buf, sizeof(buf), "%.80s: %d words identical (%%ld)", what, n);
	diff_eq_int(buf, 0, 0, tag);
}

/*
 * Follow a pointer slot to what it reaches.
 *
 * `fixed` is the length in BYTES to use when the target is NOT one of the
 * allocator's -- a static table, where `harness_alloc_reqsize` reports 0.  For
 * a heap target the size comes from the allocator on each side and the two are
 * compared before the bytes are, which is the `V90Parameters` check: an
 * under-allocation passes a content comparison it is big enough to survive.
 */
static void
cmp_target(const char *what, const void *pa, const void *pb, int fixed,
	   long tag)
{
	char buf[160];
	unsigned sa, sb;
	int n, i;
	const unsigned char *a = (const unsigned char *)pa;
	const unsigned char *b = (const unsigned char *)pb;

	if (pa == 0 || pb == 0) {
		snprintf(buf, sizeof(buf), "%.90s: both present (%%ld)", what);
		diff_eq_int(buf, pa != 0, pb != 0, tag);
		return;
	}
	sa = harness_alloc_reqsize(pa);
	sb = harness_alloc_reqsize(pb);
	snprintf(buf, sizeof(buf), "%.90s: allocation size (%%ld)", what);
	diff_eq_int(buf, (long)sa, (long)sb, tag);

	n = sa != 0 ? (int)sa : fixed;
	for (i = 0; i < n; i++) {
		if (a[i] != b[i]) {
			snprintf(buf, sizeof(buf),
				 "%.80s: byte %d of %d (%%ld)", what, i, n);
			diff_eq_int(buf, a[i], b[i], tag);
			return;
		}
	}
	snprintf(buf, sizeof(buf), "%.80s: %d bytes identical (%%ld)", what, n);
	diff_eq_int(buf, 0, 0, tag);
}

/* An allocation's requested size, asserted on both sides at once. */
static void
cmp_size(const char *what, const void *pa, const void *pb, long want, long tag)
{
	char buf[160];

	snprintf(buf, sizeof(buf), "%.80s: sysdep_malloc size, ours (%%ld)",
		 what);
	diff_eq_int(buf, (long)harness_alloc_reqsize(pa), want, tag);
	snprintf(buf, sizeof(buf), "%.80s: sysdep_malloc size, blob (%%ld)",
		 what);
	diff_eq_int(buf, (long)harness_alloc_reqsize(pb), want, tag);
}

/* Map a function pointer to an identity the two sides can agree on. */
static int
fn_id(const void *p)
{
	if (p == 0)
		return -1;
	if (p == (const void *)RxHdxStartV17
	    || p == (const void *)&ref_RxHdxStartV17)
		return 1;
	if (p == (const void *)FAX_FSE_decision_AB
	    || p == (const void *)&ref_FAX_FSE_decision_AB)
		return 2;
	return -2;
}

/* ------------------------------------------------------------------------- */
/* The pointer maps                                                          */

static int
obj_is_ptr(int off)
{
	return off == V17RX_OBJ_OUT_I
	       || off == V17RX_OBJ_OUT_Q
	       || off == V17RX_OBJ_N_OUT
	       || off == V17RX_OBJ_ICOEFF
	       || off == V17RX_OBJ_QCOEFF
	       || off == V17RX_OBJ_CTL
	       || off == V17RX_OBJ_STATE;
}

static int
ctl_is_ptr(int off)
{
	return off == V17RXC_MTD
	       || off == V17RXC_TONE
	       || off == V17RXC_PROCESS
	       || off == V17RXC_SCRATCH
	       || off == V17RXC_MTD2
	       || off == V17RXC_BUF2
	       || off == V17RXC_AGC + OFF(struct fpm_agc, cfg)
			 + OFF(struct fpm_agc_cfg, alpha)
	       || off == V17RXC_AGC + OFF(struct fpm_agc, cfg)
			 + OFF(struct fpm_agc_cfg, beta);
}

/* The head, `struct v17_dec` and the `struct vtb` inside it: 0x00..0x97. */
static int
head_is_ptr(int off)
{
	return off == V17RXS_SGD
	       || off == V17RXS_PTR_0030 + OFF(struct vtb, paths)
	       || off == V17RXS_PTR_0030 + OFF(struct vtb, imap)
	       || off == V17RXS_PTR_0030 + OFF(struct vtb, qmap)
	       || off == V17RXS_PTR_0030 + OFF(struct vtb, bound)
	       || off == V17RXS_PTR_0030 + OFF(struct vtb, region);
}

static int
mrf_is_ptr(int off)
{
	return off == OFF(struct fpm_mrf, cfg) + OFF(struct fpm_mrf_cfg, coeff)
	       || off == OFF(struct fpm_mrf, history);
}

static int
agc_is_ptr(int off)
{
	return off == OFF(struct fpm_agc, cfg) + OFF(struct fpm_agc_cfg, alpha)
	       || off == OFF(struct fpm_agc, cfg)
			 + OFF(struct fpm_agc_cfg, beta);
}

static int
sre_is_ptr(int off)
{
	return off == OFF(struct fpm_sre, cfg) + OFF(struct fpm_sre_cfg, proto)
	       || off == OFF(struct fpm_sre, cfg) + OFF(struct fpm_sre_cfg, disc)
	       || off == OFF(struct fpm_sre, cfg)
			 + OFF(struct fpm_sre_cfg, xclock)
	       || off == OFF(struct fpm_sre, cfg)
			 + OFF(struct fpm_sre_cfg, yclock)
	       || off == OFF(struct fpm_sre, cfg)
			 + OFF(struct fpm_sre_cfg, pll_k1)
	       || off == OFF(struct fpm_sre, cfg)
			 + OFF(struct fpm_sre_cfg, pll_k2)
	       || off == OFF(struct fpm_sre, coeff)
	       || off == OFF(struct fpm_sre, hist)
	       || off == OFF(struct fpm_sre, clk)
	       || off == OFF(struct fpm_sre, rms_buf);
}

static int
fse_is_ptr(int off)
{
	return off == OFF(struct fpm_fse, cfg) + OFF(struct fpm_fse_cfg, icoff)
	       || off == OFF(struct fpm_fse, cfg)
			 + OFF(struct fpm_fse_cfg, qcoff)
	       || off == OFF(struct fpm_fse, cfg) + OFF(struct fpm_fse_cfg, clk)
	       || off == OFF(struct fpm_fse, cfg)
			 + OFF(struct fpm_fse_cfg, pll_k1)
	       || off == OFF(struct fpm_fse, cfg)
			 + OFF(struct fpm_fse_cfg, pll_k2)
	       || off == OFF(struct fpm_fse, cfg)
			 + OFF(struct fpm_fse_cfg, owner)
	       || off == OFF(struct fpm_fse, cfg)
			 + OFF(struct fpm_fse_cfg, decision)
	       || off == OFF(struct fpm_fse, out_i)
	       || off == OFF(struct fpm_fse, out_q)
	       || off == OFF(struct fpm_fse, icoeff)
	       || off == OFF(struct fpm_fse, qcoeff)
	       || off == OFF(struct fpm_fse, hist);
}

static int
tail_is_ptr(int off)
{
	/* Offsets here are relative to V17RXS_SDM. */
	return off == V17RXS_BUF_MRF - V17RXS_SDM
	       || off == V17RXS_BUF_SRE - V17RXS_SDM;
}

static int
mtd_is_ptr(int off)
{
	return off == OFF(struct fpm_mtd, cfg) + OFF(struct fpm_mtd_cfg, coeff)
	       || off == OFF(struct fpm_mtd, acc);
}

static int
tone_is_ptr(int off)
{
	return off == OFF(struct fpm_tone, cfg) + OFF(struct fpm_tone_cfg, src)
	       || off == OFF(struct fpm_tone, kernel)
	       || off == OFF(struct fpm_tone, history)
	       || off == OFF(struct fpm_tone, rev_block)
	       || off == OFF(struct fpm_tone, rev_acc)
	       || off == OFF(struct fpm_tone, iir_self);
}

static int
sgd_is_ptr(int off)
{
	return off == OFF(struct sgd, cfg) + OFF(struct sgd_cfg, gen)
			+ OFF(struct sgd_gen_cfg, seq)
	       || off == OFF(struct sgd, cfg) + OFF(struct sgd_cfg, det)
			+ OFF(struct sgd_det_cfg, ref)
	       || off == OFF(struct sgd, status)
			+ OFF(struct sgd_status, det_at)
	       || off == OFF(struct sgd, hist);
}

/* ------------------------------------------------------------------------- */
/* The whole tree                                                            */

static const struct {
	int nsub;
	int n_imap;		/* map entries, INCLUDING the spare short   */
	int n_bound;
	int n_region;
	int grid;
	int mask;
	int threshold;
} trellis[4] = {
	{ 1,  17,  128,   8, 2, 0x07, 0xa28 },
	{ 2,  33,  416,  32, 4, 0x0f, 0x514 },
	{ 3,  65,  960,  72, 6, 0x1f, 0x341 },
	{ 4, 129, 1856, 128, 8, 0x3f, 0x1c2 }
};

static void
compare_tree(const char *what, void *a, void *b, int fresh, long tag)
{
	unsigned char *ca, *cb, *sa, *sb;
	struct fpm_mrf *mra, *mrb;
	struct fpm_agc *aga, *agb;
	struct fpm_sre *sra, *srb;
	struct fpm_fse *fsa, *fsb;
	struct fpm_mtd *mta, *mtb;
	struct fpm_tone *toa, *tob;
	struct sgd *ga, *gb;
	struct vtb *va, *vb;
	int code;
	char buf[160];

	ca = (unsigned char *)ptr_at(a, V17RX_OBJ_CTL);
	cb = (unsigned char *)ptr_at(b, V17RX_OBJ_CTL);
	sa = (unsigned char *)ptr_at(a, V17RX_OBJ_STATE);
	sb = (unsigned char *)ptr_at(b, V17RX_OBJ_STATE);

	diff_eq_int("control block built on both sides (%ld)",
		    ca != 0 && cb != 0, 1, tag);
	diff_eq_int("state block built on both sides (%ld)",
		    sa != 0 && sb != 0, 1, tag);
	if (ca == 0 || cb == 0 || sa == 0 || sb == 0)
		return;

	/* ---- the three blocks, in full ------------------------------- */

	snprintf(buf, sizeof(buf), "%.60s instance", what);
	cmp_span(buf, a, b, OBJ_SIZE, obj_is_ptr, tag);
	snprintf(buf, sizeof(buf), "%.60s control block", what);
	cmp_span(buf, ca, cb, CTL_SIZE, ctl_is_ptr, tag);

	snprintf(buf, sizeof(buf), "%.60s state head + dec + vtb", what);
	cmp_span(buf, sa, sb, V17RXS_MRF, head_is_ptr, tag);

	mra = (struct fpm_mrf *)(void *)(sa + V17RXS_MRF);
	mrb = (struct fpm_mrf *)(void *)(sb + V17RXS_MRF);
	snprintf(buf, sizeof(buf), "%.60s state fpm_mrf", what);
	cmp_span(buf, mra, mrb, (int)sizeof(*mra), mrf_is_ptr, tag);

	aga = (struct fpm_agc *)(void *)(sa + V17RXS_AGC);
	agb = (struct fpm_agc *)(void *)(sb + V17RXS_AGC);
	snprintf(buf, sizeof(buf), "%.60s state fpm_agc", what);
	cmp_span(buf, aga, agb, (int)sizeof(*aga), agc_is_ptr, tag);

	sra = (struct fpm_sre *)(void *)(sa + V17RXS_SRE);
	srb = (struct fpm_sre *)(void *)(sb + V17RXS_SRE);
	snprintf(buf, sizeof(buf), "%.60s state fpm_sre", what);
	cmp_span(buf, sra, srb, (int)sizeof(*sra), sre_is_ptr, tag);

	fsa = (struct fpm_fse *)(void *)(sa + V17RXS_FSE);
	fsb = (struct fpm_fse *)(void *)(sb + V17RXS_FSE);
	snprintf(buf, sizeof(buf), "%.60s state fpm_fse", what);
	cmp_span(buf, fsa, fsb, (int)sizeof(*fsa), fse_is_ptr, tag);

	snprintf(buf, sizeof(buf), "%.60s state tail (sdm..0x4fbc)", what);
	cmp_span(buf, sa + V17RXS_SDM, sb + V17RXS_SDM,
		 RXS_SIZE - V17RXS_SDM, tail_is_ptr, tag);

	/* ---- everything the skipped slots reach ---------------------- */

	cmp_size("instance", a, b, OBJ_SIZE, tag);
	cmp_size("control block", ca, cb, CTL_SIZE, tag);
	cmp_size("state block", sa, sb, RXS_SIZE, tag);
	cmp_size("ctl scratch", ptr_at(ca, V17RXC_SCRATCH),
		 ptr_at(cb, V17RXC_SCRATCH), SCRATCH_BYTES, tag);
	cmp_size("ctl buf2", ptr_at(ca, V17RXC_BUF2),
		 ptr_at(cb, V17RXC_BUF2), SCRATCH_BYTES, tag);
	cmp_size("state buf_mrf", ptr_at(sa, V17RXS_BUF_MRF),
		 ptr_at(sb, V17RXS_BUF_MRF), BUF_MRF_BYTES, tag);
	cmp_size("state buf_sre", ptr_at(sa, V17RXS_BUF_SRE),
		 ptr_at(sb, V17RXS_BUF_SRE), BUF_SRE_BYTES, tag);

	cmp_target("ctl scratch", ptr_at(ca, V17RXC_SCRATCH),
		   ptr_at(cb, V17RXC_SCRATCH), SCRATCH_BYTES, tag);
	cmp_target("ctl buf2", ptr_at(ca, V17RXC_BUF2),
		   ptr_at(cb, V17RXC_BUF2), SCRATCH_BYTES, tag);
	cmp_target("state buf_mrf", ptr_at(sa, V17RXS_BUF_MRF),
		   ptr_at(sb, V17RXS_BUF_MRF), BUF_MRF_BYTES, tag);
	cmp_target("state buf_sre", ptr_at(sa, V17RXS_BUF_SRE),
		   ptr_at(sb, V17RXS_BUF_SRE), BUF_SRE_BYTES, tag);

	/*
	 * The two AGCs' smoother tables, and each is two entries.
	 *
	 * ONCE THE MACHINE HAS RUN THEY ARE COMPARED AS DISPLACEMENTS AND NOT
	 * AS CONTENTS, and that is D1216 rather than a convenience:
	 * `RxNextStateV17`'s EPOCH_DET arm advances both by one `short` with no
	 * bound, so after a transition the pointers are one entry into a
	 * two-entry table and reading four bytes through them reads whatever
	 * `.rodata` put next -- which is a different symbol on each side.  The
	 * displacement from each side's OWN `AGCv17_CFG` is the comparable
	 * quantity, and it is the one the arm actually changes.
	 */
	if (fresh) {
		cmp_target("ctl agc.cfg.alpha",
			   ((struct fpm_agc *)(void *)
			    (ca + V17RXC_AGC))->cfg.alpha,
			   ((struct fpm_agc *)(void *)
			    (cb + V17RXC_AGC))->cfg.alpha, 4, tag);
		cmp_target("ctl agc.cfg.beta",
			   ((struct fpm_agc *)(void *)
			    (ca + V17RXC_AGC))->cfg.beta,
			   ((struct fpm_agc *)(void *)
			    (cb + V17RXC_AGC))->cfg.beta, 4, tag);
		cmp_target("state agc.cfg.alpha", aga->cfg.alpha,
			   agb->cfg.alpha, 4, tag);
		cmp_target("state agc.cfg.beta", aga->cfg.beta, agb->cfg.beta,
			   4, tag);
	} else {
		diff_eq_int("state agc.cfg.alpha displacement (%ld)",
			    aga->cfg.alpha - AGCv17_CFG.alpha,
			    agb->cfg.alpha - ref_AGCv17_CFG.alpha, tag);
		diff_eq_int("state agc.cfg.beta displacement (%ld)",
			    aga->cfg.beta - AGCv17_CFG.beta,
			    agb->cfg.beta - ref_AGCv17_CFG.beta, tag);
		diff_eq_int("ctl agc.cfg.alpha displacement (%ld)",
			    ((struct fpm_agc *)(void *)
			     (ca + V17RXC_AGC))->cfg.alpha - AGCv17_CFG.alpha,
			    ((struct fpm_agc *)(void *)
			     (cb + V17RXC_AGC))->cfg.alpha
			    - ref_AGCv17_CFG.alpha, tag);
		diff_eq_int("ctl agc.cfg.beta displacement (%ld)",
			    ((struct fpm_agc *)(void *)
			     (ca + V17RXC_AGC))->cfg.beta - AGCv17_CFG.beta,
			    ((struct fpm_agc *)(void *)
			     (cb + V17RXC_AGC))->cfg.beta
			    - ref_AGCv17_CFG.beta, tag);
	}

	cmp_target("mrf.cfg.coeff", mra->cfg.coeff, mrb->cfg.coeff, 360 * 2,
		   tag);
	cmp_target("mrf.history", mra->history, mrb->history, 40 * 2, tag);
	diff_eq_int("mrf.cfg.aux is the instance's +0x24 (%ld)",
		    mra->cfg.aux == mrb->cfg.aux
		    && mra->cfg.aux == (void *)ptr_at(a, 0x24), 1, tag);

	cmp_target("sre.cfg.proto", sra->cfg.proto, srb->cfg.proto, 181 * 2,
		   tag);
	cmp_target("sre.cfg.disc", sra->cfg.disc, srb->cfg.disc,
		   FPM_SRE_DISC * 2, tag);
	cmp_target("sre.cfg.xclock", sra->cfg.xclock, srb->cfg.xclock, 3 * 2,
		   tag);
	cmp_target("sre.cfg.yclock", sra->cfg.yclock, srb->cfg.yclock, 3 * 2,
		   tag);
	cmp_target("sre.cfg.pll_k1", sra->cfg.pll_k1, srb->cfg.pll_k1,
		   FPM_SRE_MODES * 2, tag);
	cmp_target("sre.cfg.pll_k2", sra->cfg.pll_k2, srb->cfg.pll_k2,
		   FPM_SRE_MODES * 2, tag);
	cmp_target("sre.coeff", sra->coeff, srb->coeff, 0, tag);
	cmp_target("sre.hist", sra->hist, srb->hist, 0, tag);
	cmp_target("sre.clk", sra->clk, srb->clk, 0, tag);
	cmp_target("sre.rms_buf", sra->rms_buf, srb->rms_buf, 0, tag);

	cmp_target("fse.cfg.icoff", fsa->cfg.icoff, fsb->cfg.icoff,
		   V17_COEF_N * 2, tag);
	cmp_target("fse.cfg.qcoff", fsa->cfg.qcoff, fsb->cfg.qcoff,
		   V17_COEF_N * 2, tag);
	cmp_target("fse.cfg.clk", fsa->cfg.clk, fsb->cfg.clk, 4 * 2, tag);
	cmp_target("fse.cfg.pll_k1", fsa->cfg.pll_k1, fsb->cfg.pll_k1, 3 * 2,
		   tag);
	cmp_target("fse.cfg.pll_k2", fsa->cfg.pll_k2, fsb->cfg.pll_k2, 3 * 2,
		   tag);
	cmp_target("fse.out_i", fsa->out_i, fsb->out_i, 0, tag);
	cmp_target("fse.out_q", fsa->out_q, fsb->out_q, 0, tag);
	cmp_target("fse.icoeff", fsa->icoeff, fsb->icoeff, 0, tag);
	cmp_target("fse.qcoeff", fsa->qcoeff, fsb->qcoeff, 0, tag);
	cmp_target("fse.hist", fsa->hist, fsb->hist, 0, tag);

	/*
	 * The two slots that are a RELATION on each side rather than a
	 * comparison across them: `owner` is this side's own `struct v17_dec`
	 * and `decision` is this side's own slicer.
	 */
	diff_eq_int("fse.cfg.owner is state+0x2c, ours (%ld)",
		    fsa->cfg.owner == (void *)(sa + V17RXS_SGD), 1, tag);
	diff_eq_int("fse.cfg.owner is state+0x2c, blob (%ld)",
		    fsb->cfg.owner == (void *)(sb + V17RXS_SGD), 1, tag);
	diff_eq_int("fse.cfg.decision is FAX_FSE_decision_AB (%ld)",
		    fn_id((const void *)fsa->cfg.decision),
		    fn_id((const void *)fsb->cfg.decision), tag);
	diff_eq_int("and it is that slicer and not another (%ld)",
		    fn_id((const void *)fsa->cfg.decision), 2, tag);
	diff_eq_int("fse.cfg.reserved34 is the instance's +0x24 (%ld)",
		    fsa->cfg.reserved34 == fsb->cfg.reserved34
		    && fsa->cfg.reserved34 == (void *)ptr_at(a, 0x24), 1, tag);

	diff_eq_int("ctl process slot agrees (%ld)",
		    fn_id(ptr_at(ca, V17RXC_PROCESS)),
		    fn_id(ptr_at(cb, V17RXC_PROCESS)), tag);
	if (fresh)
		diff_eq_int("and it is RxHdxStartV17 (%ld)",
			    fn_id(ptr_at(ca, V17RXC_PROCESS)), 1, tag);

	/* ---- the trellis, whose tables are subscripts elsewhere ------ */

	va = (struct vtb *)(void *)(sa + V17RXS_PTR_0030);
	vb = (struct vtb *)(void *)(sb + V17RXS_PTR_0030);
	code = (int)va->nsub - 1;
	diff_eq_int("vtb.nsub agrees (%ld)", va->nsub, vb->nsub, tag);
	if (code >= 0 && code < 4) {
		vtb_arm[code]++;
		cmp_target("vtb.imap", va->imap, vb->imap,
			   trellis[code].n_imap * 2, tag);
		cmp_target("vtb.qmap", va->qmap, vb->qmap,
			   trellis[code].n_imap * 2, tag);
		cmp_target("vtb.bound", va->bound, vb->bound,
			   trellis[code].n_bound * 2, tag);
		cmp_target("vtb.region", va->region, vb->region,
			   trellis[code].n_region * 2, tag);
	}
	cmp_target("vtb.paths", va->paths, vb->paths, PATHS_BYTES, tag);
	cmp_size("vtb.paths", va->paths, vb->paths, PATHS_BYTES, tag);

	/* ---- the two detectors, the notch and the sequence engine ---- */

	mta = (struct fpm_mtd *)ptr_at(ca, V17RXC_MTD);
	mtb = (struct fpm_mtd *)ptr_at(cb, V17RXC_MTD);
	diff_eq_int("mtd built on both sides (%ld)", mta != 0 && mtb != 0, 1,
		    tag);
	if (mta != 0 && mtb != 0) {
		snprintf(buf, sizeof(buf), "%.60s ctl fpm_mtd", what);
		cmp_span(buf, mta, mtb, (int)sizeof(*mta), mtd_is_ptr, tag);
		cmp_target("mtd.cfg.coeff", mta->cfg.coeff, mtb->cfg.coeff,
			   10 * 2, tag);
		cmp_target("mtd.acc", mta->acc, mtb->acc, 0, tag);
	}

	mta = (struct fpm_mtd *)ptr_at(ca, V17RXC_MTD2);
	mtb = (struct fpm_mtd *)ptr_at(cb, V17RXC_MTD2);
	diff_eq_int("mtd2 built on both sides (%ld)", mta != 0 && mtb != 0, 1,
		    tag);
	if (mta != 0 && mtb != 0) {
		snprintf(buf, sizeof(buf), "%.60s ctl fpm_mtd2", what);
		cmp_span(buf, mta, mtb, (int)sizeof(*mta), mtd_is_ptr, tag);
		cmp_target("mtd2.cfg.coeff", mta->cfg.coeff, mtb->cfg.coeff,
			   10 * 2, tag);
		cmp_target("mtd2.acc", mta->acc, mtb->acc, 0, tag);
	}

	toa = (struct fpm_tone *)ptr_at(ca, V17RXC_TONE);
	tob = (struct fpm_tone *)ptr_at(cb, V17RXC_TONE);
	diff_eq_int("tone built on both sides (%ld)", toa != 0 && tob != 0, 1,
		    tag);
	if (toa != 0 && tob != 0) {
		snprintf(buf, sizeof(buf), "%.60s ctl fpm_tone", what);
		cmp_span(buf, toa, tob, FPM_TONE_STATE_SIZE, tone_is_ptr, tag);
		cmp_size("tone object", toa, tob, FPM_TONE_STATE_SIZE, tag);
		cmp_target("tone.cfg.src", toa->cfg.src, tob->cfg.src, 53 * 2,
			   tag);
		cmp_target("tone.kernel", toa->kernel, tob->kernel, 0, tag);
		cmp_target("tone.history", toa->history, tob->history, 0, tag);
		cmp_target("tone.rev_block", toa->rev_block, tob->rev_block, 0,
			   tag);
		cmp_target("tone.rev_acc", toa->rev_acc, tob->rev_acc, 0, tag);
		diff_eq_int("tone.iir_self points at its own coeffs (%ld)",
			    toa->iir_self == toa->iir_coeff
			    && tob->iir_self == tob->iir_coeff, 1, tag);
	}

	ga = (struct sgd *)ptr_at(sa, V17RXS_SGD);
	gb = (struct sgd *)ptr_at(sb, V17RXS_SGD);
	diff_eq_int("sgd built on both sides (%ld)", ga != 0 && gb != 0, 1,
		    tag);
	if (ga != 0 && gb != 0) {
		snprintf(buf, sizeof(buf), "%.60s state sgd", what);
		cmp_span(buf, ga, gb, (int)sizeof(*ga), sgd_is_ptr, tag);
		cmp_size("sgd object", ga, gb, (long)sizeof(struct sgd), tag);
		cmp_target("sgd.hist", ga->hist, gb->hist, 0, tag);
		/*
		 * The two config pointers `SGD_CFG` itself carries.  They are
		 * compared against the table each side copied them FROM, which
		 * is the only claim that can be made about an address the
		 * object holds in `.data`.
		 */
		diff_eq_int("sgd.cfg.gen.seq is SGD_CFG's, ours (%ld)",
			    ga->cfg.gen.seq == SGD_CFG.gen.seq, 1, tag);
		diff_eq_int("sgd.cfg.gen.seq is SGD_CFG's, blob (%ld)",
			    gb->cfg.gen.seq == ref_SGD_CFG.gen.seq, 1, tag);
		diff_eq_int("sgd.cfg.det.ref is SGD_CFG's, ours (%ld)",
			    ga->cfg.det.ref == SGD_CFG.det.ref, 1, tag);
		diff_eq_int("sgd.cfg.det.ref is SGD_CFG's, blob (%ld)",
			    gb->cfg.det.ref == ref_SGD_CFG.det.ref, 1, tag);
	}
}

/* ------------------------------------------------------------------------- */
/* The configuration matrix                                                  */

static const struct {
	const char *name;
	int use_default;	/* pass params == NULL                       */
	short bit_rate;
	int retrain;		/* struct v17rx_cfg::short_train             */
	int arm;		/* which rate_arm[] this reaches             */
	int code;		/* the V17RX_RATE_* it must produce          */
} cases[] = {
	{ "params NULL (V17RX_CFG)",       1, 14400, 0, 3, V17RX_RATE_14400 },
	{ "7200, cold",                    0,  7200, 0, 0, V17RX_RATE_7200 },
	{ "7200, short retrain",           0,  7200, 1, 0, V17RX_RATE_7200 },
	{ "9600, cold",                    0,  9600, 0, 1, V17RX_RATE_9600 },
	{ "9600, short retrain",           0,  9600, 1, 1, V17RX_RATE_9600 },
	{ "12000, cold",                   0, 12000, 0, 2, V17RX_RATE_12000 },
	{ "12000, short retrain",          0, 12000, 1, 2, V17RX_RATE_12000 },
	{ "14400, cold",                   0, 14400, 0, 3, V17RX_RATE_14400 },
	{ "14400, short retrain",          0, 14400, 1, 3, V17RX_RATE_14400 },
	{ "4800, unrecognised",            0,  4800, 0, 4, V17RX_RATE_14400 },
	{ "0, unrecognised",               0,     0, 0, 4, V17RX_RATE_14400 },
	{ "2400, unrecognised",            0,  2400, 1, 4, V17RX_RATE_14400 },
	{ "32000, unrecognised and high",  0, 32000, 0, 4, V17RX_RATE_14400 }
};

#define NCASES	((long)(sizeof(cases) / sizeof(cases[0])))

/*
 * The caller's two coefficient arrays and the rate save.  ONE COPY, shared by
 * both sides on purpose: the short-retrain arm hands these very pointers to
 * `fpm_fse_cfg::icoff`/`qcoff`, so passing the same block makes those two
 * slots comparable as ADDRESSES and not only as contents, and makes the
 * equaliser start from identical coefficients.
 */
static short save0[V17_COEF_N];
static short save1[V17_COEF_N];
static short ratesave[1];

/* The value the three tail slots get.  Halves deliberately unequal. */
#define AUX_VALUE	0x1234abcdUL

static void
build_cfg(struct v17rx_cfg *c, long k)
{
	int i;

	*c = V17RX_CFG;
	if (!cases[k].use_default) {
		c->bit_rate = cases[k].bit_rate;
		c->short_train = cases[k].retrain;
		c->coefsave0 = save0;
		c->coefsave1 = save1;
		c->ratesave = ratesave;
		c->ptr_0024 = (void *)AUX_VALUE;
	}
	for (i = 0; i < V17_COEF_N; i++) {
		save0[i] = (short)(0x0100 + 3 * i);
		save1[i] = (short)(-0x0200 - 5 * i);
	}
	ratesave[0] = 0x55aa;
}

static void
count_case(long k)
{
	rate_arm[cases[k].arm]++;
	retrain_arm[cases[k].use_default ? 0 : (cases[k].retrain ? 1 : 0)]++;
	params_arm[cases[k].use_default ? 1 : 0]++;
}

/* ------------------------------------------------------------------------- */
/* Layer 1 -- what the constructor builds, without the blob                   */

static int
check_shape(void)
{
	long k;
	int i;

	diff_begin("V17RX_create: the configuration it derives");

	/*
	 * The tree's `FPM_TONE_CFG` against the object's own copy, which is
	 * what this constructor copies.  The prototype is followed separately.
	 */
	{
		const unsigned char *p = (const unsigned char *)
					 &FPM_TONE_CFG;
		const unsigned char *q = (const unsigned char *)
					 &ref_FPM_TONE_CFG;
		int src_off = OFF(struct fpm_tone_cfg, src);
		int bad = -1;

		for (i = 0; i < (int)sizeof(struct fpm_tone_cfg); i++) {
			/*
			 * `src` is the ONE pointer in the table and cannot be
			 * compared as an address: ours reaches our `ToneLPF`
			 * and the blob's reaches its own.  Both are followed
			 * below.
			 */
			if (i >= src_off && i < src_off + (int)sizeof(void *))
				continue;
			if (p[i] != q[i]) {
				bad = i;
				break;
			}
		}
		diff_eq_int("FPM_TONE_CFG == ref_FPM_TONE_CFG, "
			    "first differing byte (%ld)", bad, -1,
			    (long)sizeof(struct fpm_tone_cfg));
		cmp_target("FPM_TONE_CFG.src (ToneLPF)",
			   (const short *)FPM_TONE_CFG.src,
			   ref_FPM_TONE_CFG.src, 53 * 2, 0);
		/*
		 * The symbol itself is file-local and ambiguous (there are two
		 * `ToneLPF` in the object), so the identity check is the
		 * pointer's non-nullness and the tap comparison above.
		 */
		diff_eq_int("our FPM_TONE_CFG.src is planted (%ld)",
			    FPM_TONE_CFG.src != 0, 1, 0);
	}

	diff_eq_int("SDM_CFG matches the blob's (%ld)",
		    SDM_CFG.nbits == ref_SDM_CFG.nbits
		    && SDM_CFG.tap1 == ref_SDM_CFG.tap1
		    && SDM_CFG.tap2 == ref_SDM_CFG.tap2, 1, 0);
	diff_eq_int("V17RX_CFG matches the blob's bit rate (%ld)",
		    V17RX_CFG.bit_rate, ref_V17RX_CFG.bit_rate, 0);
	diff_eq_int("V17RX_CFG's rate is 14400 (%ld)", V17RX_CFG.bit_rate,
		    14400, 0);

	for (k = 0; k < NCASES; k++) {
		struct v17rx_cfg c;
		unsigned char *ctl, *rxs;
		struct fpm_sre *sre;
		struct fpm_fse *fse;
		struct fpm_mrf *mrf;
		struct fpm_sdm *sdm;
		struct vtb *v;
		void *a;
		int code = cases[k].code;

		build_cfg(&c, k);
		a = V17RX_create(0, cases[k].use_default ? 0 : &c);
		ctl = (unsigned char *)ptr_at(a, V17RX_OBJ_CTL);
		rxs = (unsigned char *)ptr_at(a, V17RX_OBJ_STATE);

		diff_eq_int("the rate code (%ld)",
			    *(unsigned short *)(void *)(ctl + V17RXC_RATE_CODE),
			    code, k);
		diff_eq_int("the state is START (%ld)",
			    *(short *)(void *)(ctl + V17RXC_STATE),
			    V17RX_STATE_START, k);
		diff_eq_int("the countdown is 0 (%ld)",
			    *(short *)(void *)(ctl + V17RXC_COUNTDOWN), 0, k);
		diff_eq_int("V17RXC_INT_0010 is the instance's +0x14 (%ld)",
			    *(int *)(void *)(ctl + V17RXC_INT_0010),
			    cases[k].use_default ? V17RX_CFG.short_train
						 : cases[k].retrain, k);
		diff_eq_int("the status byte is START (%ld)",
			    ((const unsigned char *)a)[V17RX_OBJ_RESULT],
			    V17RX_STATUS_START, k);
		diff_eq_int("the flag byte is bits 4 and 6 (%ld)",
			    ((const unsigned char *)a)[V17RX_OBJ_RESULT_B1],
			    V17RX_FLAG_BIT4 | V17RX_FLAG_BIT6, k);
		diff_eq_int("byte 2 of the result word is clear (%ld)",
			    ((const unsigned char *)a)[V17RX_OBJ_RESULT_B2],
			    0, k);

		v = (struct vtb *)(void *)(rxs + V17RXS_PTR_0030);
		diff_eq_int("vtb.nsub follows the rate (%ld)", v->nsub,
			    trellis[code].nsub, k);
		diff_eq_int("vtb.grid is 2*nsub (%ld)", v->grid,
			    trellis[code].grid, k);
		diff_eq_int("vtb.mask is (1 << (nsub+2)) - 1 (%ld)", v->mask,
			    trellis[code].mask, k);
		diff_eq_int("vtb.shift is nsub (%ld)", v->shift, v->nsub, k);
		diff_eq_int("vtb.depth is 16 (%ld)", v->depth, 0x10, k);
		diff_eq_int("vtb.imap follows the rate (%ld)",
			    v->imap == (code == 0 ? VTBv17_IMAP16T
					: code == 1 ? VTBv17_IMAP32
					: code == 2 ? VTBv17_IMAP64
						    : VTBv17_IMAP128), 1, k);
		diff_eq_int("vtb.qmap follows the rate (%ld)",
			    v->qmap == (code == 0 ? VTBv17_QMAP16T
					: code == 1 ? VTBv17_QMAP32
					: code == 2 ? VTBv17_QMAP64
						    : VTBv17_QMAP128), 1, k);
		diff_eq_int("vtb.bound follows the rate (%ld)",
			    v->bound == (code == 0 ? VTB_BOUND_7200
					 : code == 1 ? VTB_BOUND_9600
					 : code == 2 ? VTB_BOUND_12000
						     : VTB_BOUND_14400), 1, k);
		diff_eq_int("vtb.region follows the rate (%ld)",
			    v->region == (code == 0 ? VTB_REGION_7200
					  : code == 1 ? VTB_REGION_9600
					  : code == 2 ? VTB_REGION_12000
						      : VTB_REGION_14400),
			    1, k);
		diff_eq_int("the quality threshold follows the rate (%ld)",
			    *(short *)(void *)(rxs + V17RXS_SHORT_4FB0),
			    trellis[code].threshold, k);

		sdm = (struct fpm_sdm *)(void *)(rxs + V17RXS_SDM);
		diff_eq_int("sdm.nbits is the rate code plus 3 (%ld)",
			    sdm->cfg.nbits, code + 3, k);
		diff_eq_int("sdm taps are V.17's 18 and 23 (%ld)",
			    sdm->cfg.tap1 == 0x12 && sdm->cfg.tap2 == 0x17, 1,
			    k);

		mrf = (struct fpm_mrf *)(void *)(rxs + V17RXS_MRF);
		diff_eq_int("mrf resamples 8000 to 7200 (%ld)",
			    8000 * mrf->cfg.branches / mrf->cfg.decimate, 7200,
			    k);
		diff_eq_int("mrf.taps is 360 (%ld)", mrf->cfg.taps, 0x168, k);
		diff_eq_int("mrf.history_len is taps/branches (%ld)",
			    mrf->history_len, 40, k);

		sre = (struct fpm_sre *)(void *)(rxs + V17RXS_SRE);
		diff_eq_int("sre.cfg.clock_len is 3 (%ld)", sre->cfg.clock_len,
			    3, k);
		diff_eq_int("sre.cfg.coeffs is 180 = 10 branches x 18 (%ld)",
			    sre->cfg.coeffs, FPM_SRE_BRANCHES * 18, k);
		diff_eq_int("sre.taps is coeffs/branches (%ld)", sre->taps, 18,
			    k);
		/*
		 * The level gate is a SIXTH of the gain control's reference,
		 * and the reference is the one the AGC beside it was just
		 * configured with -- both halves, so a change to either shows.
		 */
		diff_eq_int("sre.cfg.rms_min is agc.cfg.ref_level/6 (%ld)",
			    sre->cfg.rms_min,
			    ((struct fpm_agc *)(void *)
			     (rxs + V17RXS_AGC))->cfg.ref_level / 6, k);
		diff_eq_int("and AGCv17_CFG's reference is 9011 (%ld)",
			    AGCv17_CFG.ref_level, 9011, k);
		/*
		 * ppm per slipped sample: `clock_len` points a symbol at 9600
		 * symbols a second is 28800 units a second, and a slip is one
		 * of them in 10^6.
		 */
		diff_eq_int("sre.ppm_scale is 1e6/(clock_len*9600) (%ld)",
			    sre->ppm_scale,
			    1000000 / (sre->cfg.clock_len * 9600), k);
		diff_eq_int("sre.ppm_step is 0x30 (%ld)", sre->ppm_step, 0x30,
			    k);
		diff_eq_int("sre.ppm_period is 9600 (%ld)", sre->ppm_period,
			    0x2580, k);
		diff_eq_int("sre.ppm_n_max is 0x68 (%ld)", sre->ppm_n_max,
			    0x68, k);

		fse = (struct fpm_fse *)(void *)(rxs + V17RXS_FSE);
		diff_eq_int("fse.taps is V17_COEF_N (%ld)", fse->cfg.taps,
			    V17_COEF_N, k);
		diff_eq_int("fse.interp is 3 samples a symbol (%ld)",
			    fse->cfg.interp, 3, k);
		diff_eq_int("fse.clk_mod is 4 and clk_inc is 1 (%ld)",
			    fse->cfg.clk_mod == 4 && fse->cfg.clk_inc == 1, 1,
			    k);
		/* 7200 Hz / 4 * 1 is 1800 Hz, V.17's carrier. */
		diff_eq_int("the carrier reference is 1800 Hz (%ld)",
			    7200 * fse->cfg.clk_inc / fse->cfg.clk_mod, 1800,
			    k);

		/*
		 * The five things `V17RXC_INT_0010` governs at once.  A test
		 * that ran only one arm would cover none of them.
		 */
		if (*(int *)(void *)(ctl + V17RXC_INT_0010) != 0) {
			diff_eq_int("retrain: sre.settle is 48 (%ld)",
				    sre->cfg.settle, 0x30, k);
			diff_eq_int("retrain: sre.pll_k1 is the _S table (%ld)",
				    sre->cfg.pll_k1 == SREv17_PLL_K1_S, 1, k);
			diff_eq_int("retrain: fse.train_sym is 256 (%ld)",
				    fse->cfg.train_sym, 0x100, k);
			diff_eq_int("retrain: fse.pll_k1 is the _S table (%ld)",
				    fse->cfg.pll_k1 == CRRv17_PLL_K1_S, 1, k);
			diff_eq_int("retrain: fse.icoff is the caller's (%ld)",
				    fse->cfg.icoff == save0
				    && fse->cfg.qcoff == save1, 1, k);
		} else {
			diff_eq_int("cold: sre.settle is 85 (%ld)",
				    sre->cfg.settle, 0x55, k);
			diff_eq_int("cold: sre.pll_k1 is the plain table (%ld)",
				    sre->cfg.pll_k1 == SREv17_PLL_K1, 1, k);
			diff_eq_int("cold: fse.train_sym is 1500 (%ld)",
				    fse->cfg.train_sym, 0x5dc, k);
			diff_eq_int("cold: fse.pll_k1 is the plain table (%ld)",
				    fse->cfg.pll_k1 == CRRv17_PLL_K1, 1, k);
			diff_eq_int("cold: fse.icoff is FSEv17_ICOFF (%ld)",
				    fse->cfg.icoff == FSEv17_ICOFF
				    && fse->cfg.qcoff == FSEv17_QCOFF, 1, k);
		}
		/* The integral gains are shared between the two arms. */
		diff_eq_int("sre.pll_k2 is shared (%ld)",
			    sre->cfg.pll_k2 == SREv17_PLL_K2, 1, k);
		diff_eq_int("fse.pll_k2 is shared (%ld)",
			    fse->cfg.pll_k2 == CRRv17_PLL_K2, 1, k);

		/* The two detectors' bands, which is F9471's claim. */
		{
			struct fpm_mtd *m1 = (struct fpm_mtd *)
					     ptr_at(ctl, V17RXC_MTD);
			struct fpm_mtd *m2 = (struct fpm_mtd *)
					     ptr_at(ctl, V17RXC_MTD2);
			struct fpm_tone *t = (struct fpm_tone *)
					     ptr_at(ctl, V17RXC_TONE);

			diff_eq_int("mtd is the V.17 bank at level 100 (%ld)",
				    m1->cfg.coeff == V17_MTD_COEFF
				    && m1->cfg.min_level == 100
				    && m1->cfg.tones == 2, 1, k);
			diff_eq_int("mtd2 is V.21 channel 2 at level 300 (%ld)",
				    m2->cfg.coeff == V21_CHAN2_MTD_COEFF
				    && m2->cfg.min_level == 300
				    && m2->cfg.tones == 2, 1, k);
			diff_eq_int("the notch is retuned to 1800 Hz (%ld)",
				    t->cfg.freq, 1800, k);
			diff_eq_int("and the built-in is still 2100 (%ld)",
				    FPM_TONE_CFG.freq, 2100, k);
		}

		/* The state head, whose eleven constants are this function's. */
		diff_eq_int("state +0x00 is 1 (%ld)",
			    *(int *)(void *)(rxs + V17RXS_INT_0000), 1, k);
		diff_eq_int("state +0x1c is 1 as a full int (%ld)",
			    *(int *)(void *)(rxs + V17RXS_INT_001C), 1, k);
		diff_eq_int("state +0x24 is the rate code (%ld)",
			    *(unsigned short *)(void *)
			    (rxs + V17RXS_RATE_CODE), code, k);
		diff_eq_int("state +0x4fb4 is 1 (%ld)",
			    *(short *)(void *)(rxs + V17RXS_SHORT_4FB4), 1, k);

		/* The slicers' own state, which `v17dec.h` models. */
		{
			struct v17_dec *d = (struct v17_dec *)(void *)
					    (rxs + V17RXS_SGD);

			diff_eq_int("dec.rate is the rate code (%ld)", d->rate,
				    code, k);
			diff_eq_int("dec.short_0066 is 3 (%ld)", d->short_0066,
				    3, k);
			diff_eq_int("dec.short_train is V17RXC_INT_0010 (%ld)",
				    d->short_train,
				    *(int *)(void *)(ctl + V17RXC_INT_0010),
				    k);
			diff_eq_int("dec.count and dec.scram are clear (%ld)",
				    d->count == 0 && d->scram == 0, 1, k);
			diff_eq_int("dec.sym_count is clear (%ld)",
				    d->sym_count, 0, k);
		}

		/*
		 * D1225: 160 of the 164 entries of `V17RXS_BUF_SRE` are
		 * cleared and the top four keep the allocator's fill.  The
		 * claim is measured, not asserted.
		 */
		{
			const unsigned short *sb = (const unsigned short *)
						   ptr_at(rxs,
							  V17RXS_BUF_SRE);
			int zeros = 0, filled = 0;

			for (i = 0; i < V17RXS_SRE_MAX; i++) {
				if (sb[i] == 0)
					zeros++;
				else
					filled++;
			}
			diff_eq_int("buf_sre: 160 cleared entries (%ld)",
				    zeros, 160, k);
			diff_eq_int("buf_sre: 4 left as allocated (%ld)",
				    filled, V17RXS_SRE_MAX - 160, k);
		}

		V17RX_delete(a);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */
/* Layer 2 -- the self-allocating path, against the blob                      */

static int
test_self_allocating(void)
{
	long k, lvl;

	diff_begin("V17RX_create, self-allocating");

	for (lvl = 0; lvl < 2; lvl++) {
		dsplibs_debug_level = lvl ? 2u : 0u;
		ref_dsplibs_debug_level = dsplibs_debug_level;
		dsplib_debug_capture_on = 1;

		for (k = 0; k < NCASES; k++) {
			struct v17rx_cfg ca, cb;
			void *a, *b;
			long tag = k + 100 * lvl;

			build_cfg(&ca, k);
			build_cfg(&cb, k);
			dsplib_debug_capture_reset();

			b = ref_V17RX_create(0, cases[k].use_default ? 0 : &cb);
			a = V17RX_create(0, cases[k].use_default ? 0 : &ca);

			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1,
				    tag);
			if (a == 0 || b == 0)
				continue;

			count_case(k);
			debug_arm[lvl]++;
			path_arm[0]++;

			/*
			 * The diagnostics are output like any other: three
			 * call sites, all gated on the level, and the
			 * "New allocation" one is reached only here.
			 */
			diff_eq_int("debug transcripts agree (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)),
				    0, tag);
			diff_eq_int("debug lines agree (%ld)",
				    (long)dsplib_debug_capture_lines(0),
				    (long)dsplib_debug_capture_lines(1), tag);
			diff_eq_int("the level gates the print (%ld)",
				    dsplib_debug_capture_lines(0) > 0,
				    lvl ? 1 : 0, tag);

			compare_tree(cases[k].name, a, b, 1, tag);

			V17RX_delete(a);
			ref_V17RX_delete(b);
		}
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	return diff_end();
}

/* ------------------------------------------------------------------------- */
/* Layer 3 -- re-initialisation in place                                      */

/*
 * The caller-supplied path.  See the head of this file: an instance whose
 * state pointer is NULL cannot be handed in without both sides walking an
 * uninitialised pointer (D1222), so what is handed back is an instance this
 * function built a moment earlier -- which is `owned == 0` with every
 * sub-block already valid, and is what a retrain does.
 *
 * D955's planting goes here: the whole instance is overwritten with a
 * non-zero pattern EXCEPT the two sub-block pointers, so a field the
 * constructor forgets to rewrite reads as the pattern rather than as a zero a
 * fresh allocation would have supplied anyway.
 */
static int
test_reinit(void)
{
	long k;

	diff_begin("V17RX_create, re-initialised in place");

	for (k = 0; k < NCASES; k++) {
		struct v17rx_cfg ca, cb;
		void *a, *b, *ctla, *ctlb, *rxsa, *rxsb;
		void *patha, *pathb, *sgda, *sgdb;
		long tag = k;

		build_cfg(&ca, k);
		build_cfg(&cb, k);

		b = ref_V17RX_create(0, &cb);
		a = V17RX_create(0, &ca);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1,
				    tag);
			continue;
		}

		ctla = (void *)ptr_at(a, V17RX_OBJ_CTL);
		ctlb = (void *)ptr_at(b, V17RX_OBJ_CTL);
		rxsa = (void *)ptr_at(a, V17RX_OBJ_STATE);
		rxsb = (void *)ptr_at(b, V17RX_OBJ_STATE);
		patha = (void *)((struct vtb *)(void *)
				 ((char *)rxsa + V17RXS_PTR_0030))->paths;
		pathb = (void *)((struct vtb *)(void *)
				 ((char *)rxsb + V17RXS_PTR_0030))->paths;
		sgda = (void *)ptr_at(rxsa, V17RXS_SGD);
		sgdb = (void *)ptr_at(rxsb, V17RXS_SGD);

		memset(a, 0x5a, OBJ_SIZE);
		memset(b, 0x5a, OBJ_SIZE);
		memcpy((char *)a + V17RX_OBJ_CTL, &ctla, sizeof ctla);
		memcpy((char *)a + V17RX_OBJ_STATE, &rxsa, sizeof rxsa);
		memcpy((char *)b + V17RX_OBJ_CTL, &ctlb, sizeof ctlb);
		memcpy((char *)b + V17RX_OBJ_STATE, &rxsb, sizeof rxsb);

		b = ref_V17RX_create(b, &cb);
		a = V17RX_create(a, &ca);
		path_arm[1]++;
		count_case(k);

		/* Nothing may be replaced: `owned` is 0 on this path. */
		diff_eq_int("ctl reused, ours (%ld)",
			    ptr_at(a, V17RX_OBJ_CTL) == ctla, 1, tag);
		diff_eq_int("ctl reused, blob (%ld)",
			    ptr_at(b, V17RX_OBJ_CTL) == ctlb, 1, tag);
		diff_eq_int("state reused, ours (%ld)",
			    ptr_at(a, V17RX_OBJ_STATE) == rxsa, 1, tag);
		diff_eq_int("state reused, blob (%ld)",
			    ptr_at(b, V17RX_OBJ_STATE) == rxsb, 1, tag);
		/*
		 * The survivor ring and the SGD are the two things `owned`
		 * gates, and on this path it is CLEAR -- so both must be the
		 * ones the first call made.  That is D1221's shape observed
		 * from the outside.
		 */
		diff_eq_int("vtb.paths reused, ours (%ld)",
			    (void *)((struct vtb *)(void *)
				     ((char *)rxsa
				      + V17RXS_PTR_0030))->paths == patha,
			    1, tag);
		diff_eq_int("vtb.paths reused, blob (%ld)",
			    (void *)((struct vtb *)(void *)
				     ((char *)rxsb
				      + V17RXS_PTR_0030))->paths == pathb,
			    1, tag);
		diff_eq_int("sgd reused, ours (%ld)",
			    ptr_at(rxsa, V17RXS_SGD) == sgda, 1, tag);
		diff_eq_int("sgd reused, blob (%ld)",
			    ptr_at(rxsb, V17RXS_SGD) == sgdb, 1, tag);

		/* Nothing of the planted pattern may survive where it is written. */
		diff_eq_int("the status byte is not the pattern (%ld)",
			    ((const unsigned char *)a)[V17RX_OBJ_RESULT]
			    != 0x5a, 1, tag);
		diff_eq_int("the rate is not the pattern (%ld)",
			    *(const short *)(const void *)
			    ((const char *)a + V17RX_OBJ_RX_BPS) != 0x5a5a,
			    1, tag);

		compare_tree(cases[k].name, a, b, 1, tag);

		V17RX_delete(a);
		ref_V17RX_delete(b);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */
/* Layer 3.5 -- V17RX_control, reconfiguring a built instance in place        */

static long control_null_arm[2];	/* arg == NULL / arg present         */
static long control_0c_arm[2];		/* CLEAR_STATE0 / CLEAR_STATE10 seen */
static long control_0d_arm[2];		/* SET_CTL_INT_0008 / REINIT seen    */

static const struct {
	const char *name;
	int null_arg;
	int int_0004;
	unsigned char flags_0c;
	unsigned char flags_0d;
	int short_train;
} ctl_cases[] = {
	/*
	 * `int_0004` lands in `cfg->int_0008`, which every sibling table in
	 * faxcfg.h holds as the literal 60000 and nothing traced re-reads
	 * during construction; kept realistic anyway.  `short_train` lands in
	 * `cfg->short_train`, `V17RX_create`'s own retrain flag, and EVERY CASE
	 * HERE KEEPS IT AT 0 (cold) DELIBERATELY: the base instance below is
	 * always built cold (case 0), and driving REINIT with `short_train == 1`
	 * against an object that was BUILT cold -- a 0 -> 1 transition under
	 * reuse (`owned == 0`) -- crashes `ref_V17RX_control` itself.
	 * `t_v17rxcreate.c`'s own `test_reinit` only ever reinits a `retrain`
	 * value onto ITSELF (built and reinited with the same `cases[k]`), so
	 * that transition is untested territory in the object, not a defect
	 * in this reconstruction; nothing reconstructed drives V17RX_control
	 * this way either (its header note: no relocation reaches it at all).
	 * Retrain=1 stays covered by `test_reinit`.
	 */
	{ "NULL arg",                      1,     0, 0, 0, 0 },
	{ "all flags clear",               0, 60000, 0, 0, 0 },
	{ "SET_CTL_INT_0008 only",         0, 60000, 0,
	  V17RXCTL_SET_CTL_INT_0008, 0 },
	{ "REINIT only",                   0, 60000, 0,
	  V17RXCTL_REINIT, 0 },
	{ "SET_CTL_INT_0008 + REINIT",     0, 60000, 0,
	  (unsigned char)(V17RXCTL_SET_CTL_INT_0008 | V17RXCTL_REINIT), 0 },
	{ "CLEAR_STATE0 only",             0,     0, V17RXCTL_CLEAR_STATE0,
	  0, 0 },
	{ "CLEAR_STATE10 only",            0,     0, V17RXCTL_CLEAR_STATE10,
	  0, 0 },
	{ "CLEAR_STATE0 + CLEAR_STATE10",  0,     0,
	  (unsigned char)(V17RXCTL_CLEAR_STATE0 | V17RXCTL_CLEAR_STATE10),
	  0, 0 },
	{ "everything at once",            0, 60000,
	  (unsigned char)(V17RXCTL_CLEAR_STATE0 | V17RXCTL_CLEAR_STATE10),
	  (unsigned char)(V17RXCTL_SET_CTL_INT_0008 | V17RXCTL_REINIT), 0 },
};

#define NCTL_CASES	((long)(sizeof(ctl_cases) / sizeof(ctl_cases[0])))

static int
test_control(void)
{
	long k;

	diff_begin("V17RX_control: reconfigure a built instance in place");

	for (k = 0; k < NCTL_CASES; k++) {
		struct v17rx_cfg ca, cb;
		struct v17rx_ctl arga, argb;
		void *a, *b;
		int ra, rb;
		long tag = k;

		build_cfg(&ca, 0);
		build_cfg(&cb, 0);

		b = ref_V17RX_create(0, &cb);
		a = V17RX_create(0, &ca);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1,
				    tag);
			continue;
		}

		memset(&arga, 0, sizeof arga);
		arga.int_0004 = ctl_cases[k].int_0004;
		arga.flags_0c = ctl_cases[k].flags_0c;
		arga.flags_0d = ctl_cases[k].flags_0d;
		arga.short_train = ctl_cases[k].short_train;
		argb = arga;

		control_null_arm[ctl_cases[k].null_arg]++;
		if (ctl_cases[k].flags_0c & V17RXCTL_CLEAR_STATE0)
			control_0c_arm[0]++;
		if (ctl_cases[k].flags_0c & V17RXCTL_CLEAR_STATE10)
			control_0c_arm[1]++;
		if (ctl_cases[k].flags_0d & V17RXCTL_SET_CTL_INT_0008)
			control_0d_arm[0]++;
		if (ctl_cases[k].flags_0d & V17RXCTL_REINIT)
			control_0d_arm[1]++;

		rb = ref_V17RX_control(b, ctl_cases[k].null_arg ? NULL : &argb);
		ra = V17RX_control(a, ctl_cases[k].null_arg ? NULL : &arga);

		diff_eq_int("V17RX_control return (%ld)", ra, rb, tag);

		compare_tree(ctl_cases[k].name, a, b, 1, tag);

		V17RX_delete(a);
		ref_V17RX_delete(b);
	}

	diff_eq_int("NULL-arg arm was reached (%ld)", control_null_arm[1] > 0,
		    1, 0);
	diff_eq_int("present-arg arm was reached (%ld)",
		    control_null_arm[0] > 0, 1, 0);
	diff_eq_int("CLEAR_STATE0 arm was reached (%ld)",
		    control_0c_arm[0] > 0, 1, 0);
	diff_eq_int("CLEAR_STATE10 arm was reached (%ld)",
		    control_0c_arm[1] > 0, 1, 0);
	diff_eq_int("SET_CTL_INT_0008 arm was reached (%ld)",
		    control_0d_arm[0] > 0, 1, 0);
	diff_eq_int("REINIT arm was reached (%ld)", control_0d_arm[1] > 0, 1,
		    0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */
/* Layer 4 -- run the receiver on what was built                              */

/*
 * A constructor whose configuration is subtly wrong -- one coefficient table
 * swapped, one gain index off, one buffer short -- can still produce a block
 * comparison that passes, because most of what it writes is a copy of a table
 * both sides read from an identical source.  What it cannot do is make the
 * demodulator produce the same samples.  So the last layer drives
 * `V17RX_modem` over the constructed object and compares the OUTPUT and all
 * three blocks after every block.
 */
#define RUN_BLOCKS	12
#define RUN_COUNT	80

static unsigned char before[RXS_SIZE];
static unsigned rng_state;

static unsigned
rng_next(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

static int
test_run(void)
{
	long k;

	diff_begin("V17RX_create: the receiver it built, run for real");

	for (k = 0; k < NCASES; k++) {
		struct v17rx_cfg ca, cb;
		void *a, *b;
		int blk, i;
		const unsigned char *rxsa;

		build_cfg(&ca, k);
		build_cfg(&cb, k);
		b = ref_V17RX_create(0, cases[k].use_default ? 0 : &cb);
		a = V17RX_create(0, cases[k].use_default ? 0 : &ca);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
			continue;
		}

		/*
		 * ANTI-VACUITY (F134).  A run that demodulated nothing would
		 * compare equal on both sides and prove nothing, so the state
		 * block is snapshotted before the blocks and the bytes the
		 * demodulation MOVED are counted.
		 */
		rxsa = (const unsigned char *)ptr_at(a, V17RX_OBJ_STATE);
		memcpy(before, rxsa, RXS_SIZE);

		rng_state = 0x1234567u + (unsigned)k * 0x9e3779b9u;

		for (blk = 0; blk < RUN_BLOCKS; blk++) {
			/*
			 * HALF THE BLOCKS RUN THE FULL CHAIN AND HALF DO NOT,
			 * AND THAT IS MEASURED RATHER THAN HOPED FOR.
			 * `DemodDataV17` abandons the call while
			 * `V17RXC_STATE` is START and the V.17 tone detector
			 * answers anything but ABSENT -- which noise does --
			 * so the first run leaves the resampler, the recoverer
			 * and the equaliser untouched and exercises only the
			 * gain control and the notch.  The state is therefore
			 * advanced on BOTH sides halfway through, which is
			 * what the machine itself does once a carrier appears,
			 * and the second half drives everything the
			 * constructor configured.  `run_state_moved` is the
			 * denominator that says which of the two happened.
			 */
			if (blk == RUN_BLOCKS / 2) {
				*(short *)(void *)
					((char *)ptr_at(a, V17RX_OBJ_CTL)
					 + V17RXC_STATE) = V17RX_STATE_EPOCH_DET;
				*(short *)(void *)
					((char *)ptr_at(b, V17RX_OBJ_CTL)
					 + V17RXC_STATE) = V17RX_STATE_EPOCH_DET;
			}
			short ina[RUN_COUNT], inb[RUN_COUNT];
			short outa[4 * RUN_COUNT], outb[4 * RUN_COUNT];
			unsigned short na = RUN_COUNT, nb = RUN_COUNT;
			int ra, rb, i;
			long tag = k * 1000 + blk;

			for (i = 0; i < RUN_COUNT; i++) {
				ina[i] = (short)((int)(rng_next() >> 18)
						 - 8192);
				inb[i] = ina[i];
			}
			memset(outa, 0x33, sizeof outa);
			memset(outb, 0x33, sizeof outb);

			rb = ref_V17RX_modem(b, inb, outb, &nb);
			ra = V17RX_modem(a, ina, outa, &na);
			modem_blocks++;

			diff_eq_int("V17RX_modem result (%ld)", ra, rb, tag);
			diff_eq_int("V17RX_modem count (%ld)", na, nb, tag);
			for (i = 0; i < 4 * RUN_COUNT; i++)
				if (outa[i] != outb[i]) {
					diff_eq_int("V17RX_modem out[%ld]",
						    outa[i], outb[i], i);
					break;
				}
			if (i == 4 * RUN_COUNT)
				diff_eq_int("V17RX_modem output identical "
					    "over %ld words",
					    0, 0, 4 * RUN_COUNT);
		}

		for (i = 0; i < RXS_SIZE; i++)
			if (before[i] != rxsa[i])
				run_state_moved++;

		compare_tree(cases[k].name, a, b, 0, 900 + k);

		V17RX_delete(a);
		ref_V17RX_delete(b);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */
/* Layer 5 -- the denominators                                                */

static int
test_coverage(void)
{
	int i;

	diff_begin("V17RX_create: what the sweep actually reached");

	for (i = 0; i < 5; i++)
		diff_eq_int("rate arm %ld was reached", rate_arm[i] > 0, 1, i);
	for (i = 0; i < 4; i++)
		diff_eq_int("trellis arm nsub=%ld was reached",
			    vtb_arm[i] > 0, 1, i + 1);
	for (i = 0; i < 2; i++)
		diff_eq_int("retrain arm %ld was reached", retrain_arm[i] > 0,
			    1, i);
	for (i = 0; i < 2; i++)
		diff_eq_int("params arm %ld was reached", params_arm[i] > 0, 1,
			    i);
	for (i = 0; i < 2; i++)
		diff_eq_int("debug level arm %ld was reached", debug_arm[i] > 0,
			    1, i);
	for (i = 0; i < 2; i++)
		diff_eq_int("construction path %ld was reached",
			    path_arm[i] > 0, 1, i);
	diff_eq_int("blocks driven through V17RX_modem (%ld)",
		    modem_blocks >= NCASES * RUN_BLOCKS, 1, modem_blocks);
	diff_eq_int("words compared across all blocks (%ld)",
		    cmp_words > 400000L, 1, cmp_words);
	diff_eq_int("state bytes the run changed (%ld)",
		    run_state_moved > 4000, 1, run_state_moved);

	printf("t_v17rxcreate: %ld words compared, %ld modem blocks, "
	       "rate arms %ld/%ld/%ld/%ld/%ld, trellis arms %ld/%ld/%ld/%ld\n",
	       cmp_words, modem_blocks, rate_arm[0], rate_arm[1], rate_arm[2],
	       rate_arm[3], rate_arm[4], vtb_arm[0], vtb_arm[1], vtb_arm[2],
	       vtb_arm[3]);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= check_shape();
	rc |= test_self_allocating();
	rc |= test_reinit();
	rc |= test_control();
	rc |= test_run();
	rc |= test_coverage();

	return rc;
}
