/*
 * t_v32fprecr.c -- differential test of `V32FP_recreate`,
 *                  src/pump/v32/v32fprecr.c.
 *
 * 3733 bytes of constructor, and what makes reconstructing it safe is not
 * reading it carefully -- it is comparing the WHOLE object tree afterwards.
 * The instance is 0x6c bytes, the half-duplex context 0xb0 and the datapump
 * block 0x50dc, and every field of all three is compared, named or not, on
 * every trial.  A field left unwritten, written with the wrong value, taken
 * from the wrong template or written in a branch that should not have been
 * taken all show up as a word that differs.
 *
 * That works because the harness's `sysdep_malloc` fills every allocation
 * with HARNESS_MALLOC_FILL before handing it over, so a field the constructor
 * never touches holds the same byte on both sides and is compared like any
 * other.  A fresh page of zeroes would have made every unwritten field look
 * deliberate; see the note in test/harness/runtime.c.
 *
 * ---------------------------------------------------------------------------
 * THE FIXTURE HAZARD, AND WHY THIS TEST DOES NOT HAVE IT
 *
 * Deviation D955 / finding F8587: `v32_common_rate` subscripts a seven-entry
 * table with fp + 0x2a and does not bound it, so a fixture that fills a
 * datapump block with pseudorandom bytes reads far out of range, and a
 * blob-against-blob run cannot see it because both sides read the same wild
 * index out of the same array.
 *
 * Nothing here fills a block with garbage: every object is built by the
 * function under test.  What DOES need care is the other direction -- the
 * PARAMETER BLOCK carries three fields that are used as subscripts, and a
 * sweep is free to put anything in them:
 *
 *     +0x16  r16          indexes V32_SYMBOL_LEN[2], V32_SAMPLE_LEN[2] and
 *                         V32_TURNAROUND_DLY[2]      -- swept over 0 and 1
 *     +0x18  symlen_sel   indexes V32_SYMBOL_LEN[2]  -- swept over 0 and 1
 *     +0x00  protocol     reaches SDMv32_GPC[4] and SDMv32_GPA[4] through the
 *                         half-duplex mode, which the function clamps to
 *                         0, 1 or 2 itself
 *
 * so the two selectors are swept over their two legal values and no further.
 * The rate fields are NOT subscripts here -- this function is what turns them
 * into an index -- so they are swept over all four recognised rates plus one
 * that matches none.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS COMPARED AS AN IDENTITY RATHER THAN AS A VALUE
 *
 * Two objects necessarily hold different addresses, and `symmap.py` gives the
 * blob's copy of every table and every function a `ref_` name of its own, so
 * a pointer into `.rodata` differs between the two sides exactly as a heap
 * pointer does.  Those slots are skipped by offset and checked another way:
 *
 *   - the two half-duplex state pointers and the three symbol encoders and
 *     the equaliser's slicer, through an identity table (ours against ours,
 *     the blob's against the blob's);
 *   - the constellation pair at fp + 0xa8 / +0xac the same way, because a
 *     transposition of the two is otherwise invisible;
 *   - the equaliser's `owner`, the symbol ring's buffer and the instance's
 *     whole diagnostic window as OFFSETS from a base each side has, which is
 *     what those nine pointers actually encode;
 *   - the heap buffers by CONTENT.
 *
 * ---------------------------------------------------------------------------
 * THE ONE FIELD THAT CANNOT BE COMPARED, AND WHY IT IS NAMED HERE
 *
 * `mtd->cfg.f0a` is stack garbage in both objects.  The function fills four
 * of `struct fpm_mtd_cfg`'s five fields on the stack and `FPM_MTD_create`
 * copies all five, so the fifth is whatever the frame held -- and the two
 * frames are not the same frame.  Deviation D960.  It is excluded by name
 * rather than by widening a tolerance, and the four fields around it are
 * compared.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v32data.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32fp.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32fpstat.h"
#include "dsplib/v32hdx.h"
#include "dsplib/v32hdxst.h"
#include "dsplib/v32scram.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32smc.h"
#include "dsplib/vtb.h"

extern void *ref_V32FP_recreate(void *modem, const struct v32fp_params *param,
				void *arg2);
extern const struct v32fp_params ref_V32_CFG;

/*
 * The blob's copies of everything this constructor stores the ADDRESS of.
 * Declared `short` and taken by address, which is the idiom t_b103create uses:
 * a renamed function has no prototype to borrow and only its address matters.
 */
extern short ref_TxHdxNull, ref_TxHdxTone, ref_TxHdxCarrierState;
extern short ref_RxHdxTone, ref_RxHdxPhsReversal;
extern short ref_SMCv32_encoder_dif, ref_SMCv32_encoder_abs;
extern short ref_SMCv32_encoder_tcm;
extern short ref_FSE_decision_AB, ref_FSE_decision_4pt;
extern short ref_SMCv32_IMAP16, ref_SMCv32_QMAP16;

#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_S16(obj, off)	(*(short *)(void *)FIELD((obj), (off)))

#define HDX(m)			FIELD_PTR((m), V32_OBJ_HDX)
#define FP(m)			FIELD_PTR((m), V32_OBJ_FP)

#define V32_OBJ_SIZE		0x6c
#define V32_HDX_SIZE		0xb0
#define V32FP_SIZE		0x50dc
#define V32FP_DEC		0x5020

#define ECC(fp)		((struct fpm_ecc *)(void *)FIELD((fp), V32FP_ECC))
#define FSE(fp)		((struct fpm_fse *)(void *)FIELD((fp), V32FP_FSE))
#define RING(fp)	((struct v32_symout *)(void *)FIELD((fp), V32FP_SYMOUT))
#define DEC(fp)		((struct v32_dec *)(void *)FIELD((fp), V32FP_DEC))

/* ---------------------------------------------------------------------- */

/*
 * Offsets in the datapump block holding an address.  Listed rather than
 * derived, because the block is not modelled as a struct -- v32data.h's
 * ruling -- so every entry carries the sub-object and member it belongs to.
 *
 * THE `aux` SLOTS ARE DELIBERATELY ABSENT.  fp + 0x84, +0xd0, +0xf4, +0x17c
 * and +0x238 all take the parameter block's +0x24 and therefore hold the SAME
 * value on both sides; leaving them in the comparison is what checks that all
 * five were filled.
 */
static int
fp_is_pointer(int off)
{
	static const int p[] = {
		0x70, 0x74, 0x78, 0x7c,		/* pps cfg imap/qmap/coeffs   */
		0x90, 0x94,			/* pps hist_i, hist_q         */
		0x98, 0x9c, 0xa0,		/* the three encoders         */
		0xa8, 0xac,			/* the constellation pair     */
		0xb8,				/* symout.buf                 */
		0xc8, 0xdc,			/* mrf cfg.coeff, history     */
		0xe8, 0xec,			/* ecc cfg imap/qmap          */
		0x118,				/* ecc line                   */
		0x120, 0x124, 0x12c, 0x130,	/* ecc near/far histories     */
		0x134, 0x138, 0x13c,		/* ecc coef[3]                */
		0x158, 0x15c, 0x160, 0x164,	/* sre cfg proto/disc/clocks  */
		0x168, 0x16c,			/* sre cfg pll gains          */
		0x198, 0x19c, 0x1a0, 0x1bc,	/* sre coeff/hist/clk/rms_buf */
		0x1e4, 0x1e8,			/* agc cfg alpha, beta        */
		0x208, 0x20c, 0x218,		/* fse cfg icoff/qcoff/clk    */
		0x228, 0x22c,			/* fse cfg pll gains          */
		0x230, 0x234,			/* fse cfg owner, decision    */
		0x258, 0x25c,			/* fse out_i, out_q           */
		0x264, 0x268, 0x26c,		/* fse icoeff/qcoeff/hist     */
		0x5038,				/* dec vtb.paths              */
		0x5050, 0x5054, 0x5058, 0x5060,	/* vtb imap/qmap/bound/region */
		0x50cc, 0x50d0			/* the two 0x154 buffers      */
	};
	unsigned i;

	for (i = 0; i < sizeof(p) / sizeof(p[0]); i++)
		if (off == p[i])
			return 1;
	return 0;
}

/*
 * The context's own pointers.  +0x0c and +0x10 are the SECOND `struct fpm_agc`
 * -- the one AGCv32Prc_CFG configures, which sits at hdx + 0x00 -- and they
 * are its `cfg.alpha` and `cfg.beta`.  They are here because the constructor's
 * two AGCs are the trap t_v32rxhdx names: one at hdx + 0x00 and one at
 * fp + 0x1d8, and a reader that forgets the first is reading a `short` where
 * an address is.
 */
static int
hdx_is_pointer(int off)
{
	return off == V32_HDX_TONE0 || off == V32_HDX_TONE1
	       || off == V32_HDX_TONE2 || off == V32_HDX_MTD
	       || off == V32HDX_TXSTATE || off == V32HDX_RXSTATE
	       || off == V32_HDX_BUF_A4
	       || off == (int)__builtin_offsetof(struct fpm_agc_cfg, alpha)
	       || off == (int)__builtin_offsetof(struct fpm_agc_cfg, beta);
}

static int
obj_is_pointer(int off)
{
	return off == 0x34 || off == 0x38 || off == 0x3c || off == 0x40
	       || off == 0x44 || off == 0x4c || off == 0x50 || off == 0x58
	       || off == 0x5c || off == V32_OBJ_HDX || off == V32_OBJ_FP;
}

/*
 * The tone object's own pointer slots, plus +0xf2: FPM_TONE_create stops its
 * clear at +0xf0, so the last word keeps allocator fill on both sides and
 * carries no information.  Same list t_b103create derives.
 */
static int
tone_is_pointer(int off)
{
	return off == (int)__builtin_offsetof(struct fpm_tone, rev_idx)
	       || off == (int)__builtin_offsetof(struct fpm_tone, cfg)
			 + (int)__builtin_offsetof(struct fpm_tone_cfg, src)
	       || off == (int)__builtin_offsetof(struct fpm_tone, kernel)
	       || off == (int)__builtin_offsetof(struct fpm_tone, history)
	       || off == (int)__builtin_offsetof(struct fpm_tone, rev_block)
	       || off == (int)__builtin_offsetof(struct fpm_tone, rev_acc)
	       || off == (int)__builtin_offsetof(struct fpm_tone, iir_self);
}

/*
 * The detector's config pointer, its accumulator, and `cfg.f0a` -- which is
 * stack garbage in both objects (D960) and is the one field in this whole
 * test that is excluded rather than compared.
 */
static int
mtd_is_pointer(int off)
{
	return off == (int)__builtin_offsetof(struct fpm_mtd, cfg)
			 + (int)__builtin_offsetof(struct fpm_mtd_cfg, coeff)
	       || off == (int)__builtin_offsetof(struct fpm_mtd, cfg)
			 + (int)__builtin_offsetof(struct fpm_mtd_cfg, f0a)
	       || off == (int)__builtin_offsetof(struct fpm_mtd, acc);
}

static void
compare_block(const char *what, const unsigned char *ours,
	      const unsigned char *ref, int size, int (*is_ptr)(int), long tag)
{
	char buf[160];
	int i;

	for (i = 0; i < size; i += 2) {
		if (is_ptr(i) || is_ptr(i - 2))
			continue;
		snprintf(buf, sizeof(buf), "%.90s word 0x%04x (%%ld)", what, i);
		diff_eq_int(buf, *(const short *)(const void *)(ours + i),
			    *(const short *)(const void *)(ref + i), tag);
	}
}

static void
compare_shorts(const char *what, const short *ours, const short *ref, int n,
	       long tag)
{
	char buf[160];
	int i;

	if (ours == 0 || ref == 0) {
		snprintf(buf, sizeof(buf), "%.90s: both present (%%ld)", what);
		diff_eq_int(buf, ours != 0, ref != 0, tag);
		return;
	}
	snprintf(buf, sizeof(buf), "%.90s[%%ld]", what);
	for (i = 0; i < n; i++)
		diff_eq_int(buf, ours[i], ref[i], i);
}

/*
 * Map a stored address to an identity both sides agree on.  -1 is NULL and
 * -2 is "an address neither side was supposed to store", which is itself a
 * failure the moment the two sides disagree about it.
 */
static int
fn_id(const void *p)
{
	if (p == 0)
		return -1;
	if (p == (const void *)TxHdxNull || p == (const void *)&ref_TxHdxNull)
		return 0;
	if (p == (const void *)TxHdxTone || p == (const void *)&ref_TxHdxTone)
		return 1;
	if (p == (const void *)TxHdxCarrierState
	    || p == (const void *)&ref_TxHdxCarrierState)
		return 2;
	if (p == (const void *)RxHdxTone || p == (const void *)&ref_RxHdxTone)
		return 3;
	if (p == (const void *)RxHdxPhsReversal
	    || p == (const void *)&ref_RxHdxPhsReversal)
		return 4;
	if (p == (const void *)SMCv32_encoder_dif
	    || p == (const void *)&ref_SMCv32_encoder_dif)
		return 5;
	if (p == (const void *)SMCv32_encoder_abs
	    || p == (const void *)&ref_SMCv32_encoder_abs)
		return 6;
	if (p == (const void *)SMCv32_encoder_tcm
	    || p == (const void *)&ref_SMCv32_encoder_tcm)
		return 7;
	if (p == (const void *)FSE_decision_AB
	    || p == (const void *)&ref_FSE_decision_AB)
		return 8;
	if (p == (const void *)FSE_decision_4pt
	    || p == (const void *)&ref_FSE_decision_4pt)
		return 9;
	if (p == (const void *)SMCv32_IMAP16
	    || p == (const void *)&ref_SMCv32_IMAP16)
		return 10;
	if (p == (const void *)SMCv32_QMAP16
	    || p == (const void *)&ref_SMCv32_QMAP16)
		return 11;
	return -2;
}

static void
same_id(const char *what, const void *a, const void *b, long tag)
{
	char buf[160];

	snprintf(buf, sizeof(buf), "%.90s (%%ld)", what);
	diff_eq_int(buf, fn_id(a), fn_id(b), tag);
	snprintf(buf, sizeof(buf), "%.90s: known symbol (%%ld)", what);
	diff_eq_int(buf, fn_id(a) != -2, 1, tag);
}

/* A pointer compared as its OFFSET from a base each side has of its own. */
static void
same_offset(const char *what, const void *pa, const void *ba,
	    const void *pb, const void *bb, long tag)
{
	char buf[160];

	snprintf(buf, sizeof(buf), "%.90s: offset (%%ld)", what);
	diff_eq_int(buf, (long)((const char *)pa - (const char *)ba),
		    (long)((const char *)pb - (const char *)bb), tag);
}

/* ---------------------------------------------------------------------- */

static void
compare_tree(const char *what, void *a, void *b, long tag)
{
	char buf[160];
	unsigned char *fa = (unsigned char *)FP(a);
	unsigned char *fb = (unsigned char *)FP(b);
	unsigned char *ha = (unsigned char *)HDX(a);
	unsigned char *hb = (unsigned char *)HDX(b);
	int i;

	snprintf(buf, sizeof(buf), "%.60s obj", what);
	compare_block(buf, (unsigned char *)a, (unsigned char *)b,
		      V32_OBJ_SIZE, obj_is_pointer, tag);
	snprintf(buf, sizeof(buf), "%.60s hdx", what);
	compare_block(buf, ha, hb, V32_HDX_SIZE, hdx_is_pointer, tag);
	snprintf(buf, sizeof(buf), "%.60s fp", what);
	compare_block(buf, fa, fb, V32FP_SIZE, fp_is_pointer, tag);

	/* The two half-duplex state pointers. */
	snprintf(buf, sizeof(buf), "%.60s hdx txstate", what);
	same_id(buf, FIELD_PTR(ha, V32HDX_TXSTATE),
		FIELD_PTR(hb, V32HDX_TXSTATE), tag);
	snprintf(buf, sizeof(buf), "%.60s hdx rxstate", what);
	same_id(buf, FIELD_PTR(ha, V32HDX_RXSTATE),
		FIELD_PTR(hb, V32HDX_RXSTATE), tag);

	/* The three encoders, in order, and the constellation pair. */
	for (i = 0; i < V32FP_ENCODERS_N; i++) {
		snprintf(buf, sizeof(buf), "%.50s fp encoder %d", what, i);
		same_id(buf, FIELD_PTR(fa, V32FP_ENCODERS + 4 * i),
			FIELD_PTR(fb, V32FP_ENCODERS + 4 * i), tag);
	}
	snprintf(buf, sizeof(buf), "%.60s fp imap", what);
	same_id(buf, FIELD_PTR(fa, 0xa8), FIELD_PTR(fb, 0xa8), tag);
	snprintf(buf, sizeof(buf), "%.60s fp qmap", what);
	same_id(buf, FIELD_PTR(fa, 0xac), FIELD_PTR(fb, 0xac), tag);
	snprintf(buf, sizeof(buf), "%.60s fse decision", what);
	same_id(buf, (const void *)FSE(fa)->cfg.decision,
		(const void *)FSE(fb)->cfg.decision, tag);

	/* The equaliser's owner is the decoder, which lives in the block. */
	snprintf(buf, sizeof(buf), "%.60s fse owner", what);
	same_offset(buf, FSE(fa)->cfg.owner, fa, FSE(fb)->cfg.owner, fb, tag);

	/* The ring's buffer IS the echo canceller's delay line. */
	snprintf(buf, sizeof(buf), "%.60s ring buf == ecc line", what);
	diff_eq_int(buf, RING(fa)->buf == ECC(fa)->line
		    && RING(fb)->buf == ECC(fb)->line, 1, tag);

	/* The instance's diagnostic window, all nine pointers. */
	snprintf(buf, sizeof(buf), "%.50s diag out_i", what);
	diff_eq_int(buf, FIELD_PTR(a, 0x34) == (void *)FSE(fa)->out_i
		    && FIELD_PTR(b, 0x34) == (void *)FSE(fb)->out_i, 1, tag);
	snprintf(buf, sizeof(buf), "%.50s diag out_q", what);
	diff_eq_int(buf, FIELD_PTR(a, 0x38) == (void *)FSE(fa)->out_q
		    && FIELD_PTR(b, 0x38) == (void *)FSE(fb)->out_q, 1, tag);
	snprintf(buf, sizeof(buf), "%.50s diag n_out", what);
	diff_eq_int(buf, FIELD_PTR(a, 0x3c) == (void *)&FSE(fa)->n_out
		    && FIELD_PTR(b, 0x3c) == (void *)&FSE(fb)->n_out, 1, tag);
	snprintf(buf, sizeof(buf), "%.50s diag icoeff", what);
	diff_eq_int(buf, FIELD_PTR(a, 0x40) == (void *)FSE(fa)->icoeff
		    && FIELD_PTR(b, 0x40) == (void *)FSE(fb)->icoeff, 1, tag);
	snprintf(buf, sizeof(buf), "%.50s diag qcoeff", what);
	diff_eq_int(buf, FIELD_PTR(a, 0x44) == (void *)FSE(fa)->qcoeff
		    && FIELD_PTR(b, 0x44) == (void *)FSE(fb)->qcoeff, 1, tag);

	/*
	 * The four echo-canceller coefficient banks, as offsets in SHORTS from
	 * `coef[0]`.  This is the check that separates near-I, near-Q, far-I
	 * and far-Q from any other ordering of the same four addresses.
	 */
	snprintf(buf, sizeof(buf), "%.50s diag near_i", what);
	diff_eq_int(buf, FIELD_PTR(a, 0x4c) == (void *)ECC(fa)->coef[0]
		    && FIELD_PTR(b, 0x4c) == (void *)ECC(fb)->coef[0], 1, tag);
	snprintf(buf, sizeof(buf), "%.40s diag near_q offset", what);
	same_offset(buf, FIELD_PTR(a, 0x50), ECC(fa)->coef[0],
		    FIELD_PTR(b, 0x50), ECC(fb)->coef[0], tag);
	snprintf(buf, sizeof(buf), "%.40s diag far_i offset", what);
	same_offset(buf, FIELD_PTR(a, 0x58), ECC(fa)->coef[0],
		    FIELD_PTR(b, 0x58), ECC(fb)->coef[0], tag);
	snprintf(buf, sizeof(buf), "%.40s diag far_q offset", what);
	same_offset(buf, FIELD_PTR(a, 0x5c), ECC(fa)->coef[0],
		    FIELD_PTR(b, 0x5c), ECC(fb)->coef[0], tag);

	/*
	 * The buffers this function itself fills, by content.  The survivor
	 * ring is 128 nodes of two words and is cleared here; the delay line
	 * is `line_len` packed symbols and is the symbol ring's storage.
	 */
	snprintf(buf, sizeof(buf), "%.50s vtb paths", what);
	compare_shorts(buf,
		       (const short *)(const void *)
		       (&DEC(fa)->vtb)->paths,
		       (const short *)(const void *)
		       (&DEC(fb)->vtb)->paths,
		       128 * 2, tag);
	snprintf(buf, sizeof(buf), "%.50s ecc line", what);
	compare_shorts(buf, ECC(fa)->line, ECC(fb)->line,
		       ECC(fa)->line_len == ECC(fb)->line_len
		       ? ECC(fa)->line_len : 0, tag);

	/* The three tone objects and the multi-tone detector. */
	snprintf(buf, sizeof(buf), "%.50s tone0", what);
	compare_block(buf, (unsigned char *)FIELD_PTR(ha, V32_HDX_TONE0),
		      (unsigned char *)FIELD_PTR(hb, V32_HDX_TONE0),
		      FPM_TONE_STATE_SIZE, tone_is_pointer, tag);
	snprintf(buf, sizeof(buf), "%.50s tone1", what);
	compare_block(buf, (unsigned char *)FIELD_PTR(ha, V32_HDX_TONE1),
		      (unsigned char *)FIELD_PTR(hb, V32_HDX_TONE1),
		      FPM_TONE_STATE_SIZE, tone_is_pointer, tag);
	snprintf(buf, sizeof(buf), "%.50s tone2", what);
	compare_block(buf, (unsigned char *)FIELD_PTR(ha, V32_HDX_TONE2),
		      (unsigned char *)FIELD_PTR(hb, V32_HDX_TONE2),
		      FPM_TONE_STATE_SIZE, tone_is_pointer, tag);
	snprintf(buf, sizeof(buf), "%.50s mtd", what);
	compare_block(buf, (unsigned char *)FIELD_PTR(ha, V32_HDX_MTD),
		      (unsigned char *)FIELD_PTR(hb, V32_HDX_MTD),
		      (int)sizeof(struct fpm_mtd), mtd_is_pointer, tag);
	snprintf(buf, sizeof(buf), "%.50s mtd coeff", what);
	diff_eq_int(buf,
		    ((struct fpm_mtd *)FIELD_PTR(ha, V32_HDX_MTD))->cfg.coeff
		    == V32_S_DATA_COEF, 1, tag);
	snprintf(buf, sizeof(buf), "%.50s mtd acc", what);
	compare_shorts(buf,
		       ((struct fpm_mtd *)FIELD_PTR(ha, V32_HDX_MTD))->acc,
		       ((struct fpm_mtd *)FIELD_PTR(hb, V32_HDX_MTD))->acc,
		       6, tag);
}

/* ---------------------------------------------------------------------- */

struct trial {
	short protocol;
	short tx_rate;
	short rx_rate;
	int trellis;
	short r16;
	short symlen_sel;
	unsigned int options;
	const char *name;
};

static const struct trial trials[] = {
	{ 0, 14400, 14400, 0, 0, 0, 0x68b, "protocol 0, 14400" },
	{ 1, 14400, 14400, 0, 0, 0, 0x68b, "protocol 1, 14400" },
	{ 2, 14400, 14400, 0, 0, 0, 0x68b, "protocol 2, 14400" },
	{ 0, 12000, 12000, 0, 0, 0, 0x68b, "12000" },
	{ 0,  9600,  9600, 0, 0, 0, 0x68b, "9600, no trellis" },
	{ 0,  9600,  9600, 1, 0, 0, 0x68b, "9600, trellis" },
	{ 1,  7200,  7200, 1, 0, 0, 0x68b, "7200" },
	{ 0,  4800,  4800, 0, 0, 0, 0x68b, "4800 -- in neither ladder" },
	{ 0, 14400,  7200, 1, 0, 0, 0x68b, "asymmetric rates" },
	{ 1,  7200, 12000, 0, 0, 0, 0x68b, "asymmetric rates, other way" },
	{ 0,  9600,  9600, 1, 1, 0, 0x68b, "r16 = 1" },
	{ 0,  9600,  9600, 1, 0, 1, 0x68b, "symlen_sel = 1" },
	{ 0,  9600,  9600, 1, 1, 1, 0x68b, "both selectors 1" },
	{ 0, 14400, 14400, 0, 0, 0, 0xa8b, "protocol 0, options bit 10" },
	{ 1, 14400, 14400, 0, 0, 0, 0xa8b, "protocol 1, options bit 10" },
	{ 0, 14400, 14400, 0, 0, 0, 0x000, "options zero" },
	{ 1, 14400, 14400, 0, 0, 0, 0x007, "options low three bits" }
};

static void
fill(struct v32fp_params *p, const struct trial *t)
{
	*p = V32_CFG;
	p->protocol = t->protocol;
	p->tx_rate = t->tx_rate;
	p->rx_rate = t->rx_rate;
	p->trellis = t->trellis;
	p->r16 = t->r16;
	p->symlen_sel = t->symlen_sel;
	p->options = t->options;
}

int
main(void)
{
	unsigned k;
	int rc = 0;

	/*
	 * The template itself, first: every trial below is a patched copy of
	 * it, so a difference here would corrupt every case downstream.
	 */
	diff_begin("V32FP_recreate: the template it falls back on");
	{
		const unsigned char *x = (const unsigned char *)&V32_CFG;
		const unsigned char *y = (const unsigned char *)&ref_V32_CFG;
		unsigned i;

		diff_eq_int("sizeof V32_CFG (%ld)",
			    (long)sizeof(struct v32fp_params), 48, 0);
		for (i = 0; i < sizeof(struct v32fp_params); i += 2)
			diff_eq_int("V32_CFG word 0x%02lx",
				    *(const short *)(const void *)(x + i),
				    *(const short *)(const void *)(y + i),
				    (long)i);
	}
	rc |= diff_end();

	/*
	 * The self-allocating path, over the sweep.  Both sides are built from
	 * the SAME parameter block, so a difference is the constructor's.
	 */
	diff_begin("V32FP_recreate, self-allocating");
	for (k = 0; k < sizeof(trials) / sizeof(trials[0]); k++) {
		struct v32fp_params pa, pb;
		void *a, *b;

		fill(&pa, &trials[k]);
		fill(&pb, &trials[k]);
		b = ref_V32FP_recreate(0, &pb, 0);
		a = V32FP_recreate(0, &pa, 0);
		diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, (long)k);
		if (a == 0 || b == 0)
			continue;
		diff_eq_int("returned the instance (%ld)",
			    a == a && b == b, 1, (long)k);
		compare_tree(trials[k].name, a, b, (long)k);

		/*
		 * The caller's block must come back UNTOUCHED -- the function
		 * copies out of it and writes into the instance, never back.
		 */
		diff_eq_int("caller's block untouched (%ld)",
			    memcmp(&pa, &pb, sizeof pa) == 0, 1, (long)k);
	}
	rc |= diff_end();

	/*
	 * A NULL parameter block means "use V32_CFG", which is the arm at
	 * 0x7f5fd.  Compared against an explicit V32_CFG on the other side as
	 * well as against the reference, so the fallback is checked for being
	 * the RIGHT template and not merely for being the same one twice.
	 */
	diff_begin("V32FP_recreate, NULL parameter block");
	{
		struct v32fp_params pc = V32_CFG;
		void *a = V32FP_recreate(0, 0, 0);
		void *b = ref_V32FP_recreate(0, 0, 0);
		void *c = V32FP_recreate(0, &pc, 0);

		diff_eq_int("built (%ld)", a != 0 && b != 0 && c != 0, 1, 0);
		if (a != 0 && b != 0)
			compare_tree("null params", a, b, 0);
		if (a != 0 && c != 0)
			compare_tree("null params == V32_CFG", a, c, 1);
	}
	rc |= diff_end();

	/*
	 * The RE-create path: `fresh` is zero, so every sub-object re-uses its
	 * buffers and the three tone objects are handed their existing state
	 * instead of a NULL.  This is the arm `V32FP_control` takes, and it is
	 * the only one where a missed `fresh` shows up.
	 */
	diff_begin("V32FP_recreate, re-creating in place");
	for (k = 0; k < sizeof(trials) / sizeof(trials[0]); k++) {
		struct v32fp_params pa, pb;
		void *a, *b;

		fill(&pa, &trials[0]);
		fill(&pb, &trials[0]);
		a = V32FP_recreate(0, &pa, 0);
		b = ref_V32FP_recreate(0, &pb, 0);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", 0, 1, (long)k);
			continue;
		}
		fill(&pa, &trials[k]);
		fill(&pb, &trials[k]);
		diff_eq_int("recreate returns the same instance (%ld)",
			    V32FP_recreate(a, &pa, 0) == a
			    && ref_V32FP_recreate(b, &pb, 0) == b, 1, (long)k);
		compare_tree(trials[k].name, a, b, (long)k);
	}
	rc |= diff_end();

	/*
	 * `V32FP_control`'s own call shape: the instance passed as BOTH
	 * arguments, which works only because its first 48 bytes are the
	 * parameter block.  A `rep movsl` of a block onto itself.
	 */
	diff_begin("V32FP_recreate(obj, obj, 0)");
	for (k = 0; k < sizeof(trials) / sizeof(trials[0]); k++) {
		struct v32fp_params pa, pb;
		void *a, *b;

		fill(&pa, &trials[k]);
		fill(&pb, &trials[k]);
		a = V32FP_recreate(0, &pa, 0);
		b = ref_V32FP_recreate(0, &pb, 0);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", 0, 1, (long)k);
			continue;
		}
		diff_eq_int("self-recreate returns the instance (%ld)",
			    V32FP_recreate(a, (struct v32fp_params *)a, 0) == a
			    && ref_V32FP_recreate(
				    b, (struct v32fp_params *)b, 0) == b,
			    1, (long)k);
		compare_tree(trials[k].name, a, b, (long)k);
	}
	rc |= diff_end();

	/*
	 * Allocation counts, which is the other half of `fresh`.
	 *
	 * THE RE-CREATE PATH IS NOT ALLOCATION-FREE, and that is measured
	 * rather than assumed: it allocates five times on a second call with
	 * the same configuration.  An earlier version of this section asserted
	 * zero, on the reasoning that `fresh` exists to stop the sub-objects
	 * allocating again -- and the reference allocates the same five, so the
	 * assertion was a claim about the DESIGN and not a difference.  Some of
	 * the `FPM_*_init` reuse paths free and reallocate; `fpm_fse.h` records
	 * that its own init has no reuse path at all.  What is compared is
	 * therefore both counts against the reference's, plus that neither is
	 * zero -- a counter that has stopped counting reads as agreement.
	 */
	diff_begin("V32FP_recreate allocation counts");
	{
		struct v32fp_params pa, pb;
		void *a, *b;
		int na, nb, ra, rb;

		fill(&pa, &trials[0]);
		fill(&pb, &trials[0]);

		harness_alloc_reset();
		a = V32FP_recreate(0, &pa, 0);
		na = harness_alloc.allocs;
		harness_alloc_reset();
		(void)V32FP_recreate(a, &pa, 0);
		ra = harness_alloc.allocs;

		harness_alloc_reset();
		b = ref_V32FP_recreate(0, &pb, 0);
		nb = harness_alloc.allocs;
		harness_alloc_reset();
		(void)ref_V32FP_recreate(b, &pb, 0);
		rb = harness_alloc.allocs;

		diff_eq_int("allocations on create (%ld)", na, nb, 0);
		diff_eq_int("allocations on recreate (%ld)", ra, rb, 0);
		diff_eq_int("the counter counted, create (%ld)", na > 0, 1, 0);
		diff_eq_int("the counter counted, recreate (%ld)", ra > 0, 1, 0);
		diff_eq_int("create allocates more than recreate (%ld)",
			    na > ra, 1, 0);
	}
	rc |= diff_end();

	return rc;
}
