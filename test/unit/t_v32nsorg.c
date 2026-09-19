/*
 * t_v32nsorg.c -- differential test of `V32OrgNextState`, the ORIGINATE
 *                 handshake's next-state function.
 *
 * The function is a 34-way switch on hdx + 0x74 that rewrites the half-duplex
 * context, installs the next transmit and receive states, and reconfigures the
 * datapump through fifteen already-reconstructed callees.  The whole of that
 * is compared here: the instance, the context, the datapump block, the
 * decoder, the three tone objects, the multi-tone detector and every buffer
 * they own, WORD BY WORD, plus the diagnostic transcript.
 *
 * ---------------------------------------------------------------------------
 * WHY THE COMPARISON NORMALISES POINTERS, AND WHAT IT DOES NOT SKIP
 *
 * Both sides run the SAME fixture layout at DIFFERENT addresses, and both
 * sides store SYMBOL addresses into it -- ours the reconstruction's, the
 * blob's the `ref_`-prefixed copies `symmap.py` made.  So a word that differs
 * is not automatically a defect, and a comparison that ignored the difference
 * would not be a comparison at all.  The rule used here is:
 *
 *   1. words that are equal are equal, and cost nothing;
 *   2. a word that DIFFERS is normalised, and only then compared:
 *        - an address inside this side's own fixture becomes its OFFSET, so
 *          "both point at their own `mtd`" passes and "one points at the
 *          `mtd` and the other at the `dec`" fails;
 *        - an address in the KNOWN SYMBOL PAIRS table becomes that pair's
 *          index, so "ours installed TxHdxScrSequence and the blob installed
 *          ref_TxHdxScrSequence" passes and any other pairing fails;
 *        - anything else is left alone, so it still fails.
 *   3. an unknown differing word is a FAILURE that names the byte offset.
 *
 * That last point is the important one: the table is not an allow-list of
 * regions, it is a list of seventeen specific symbols, and a store of any
 * OTHER symbol -- or of the wrong one -- still fails.  `norm_hits` counts the
 * words rule 2 rescued and is asserted non-zero, so a run in which nothing
 * ever installed a pointer cannot pass vacuously.
 *
 * FOUR WINDOWS ARE SKIPPED OUTRIGHT, and each is a sub-object CONFIGURATION
 * whose pointers belong to a callee this test is not the differential test of:
 *
 *   fp + 0x70 .. 0x78    fpm_pps cfg.imap / cfg.qmap, written by SetTxModeV32
 *   fp + 0x158 .. 0x170  fpm_sre cfg's six table pointers, copied wholesale
 *                        out of SREv32_CFG by FPM_SRE_init
 *   fp + 0x228 .. 0x238  fpm_fse cfg.pll_k1/k2/owner/decision, written by
 *                        SetRxModeV32
 *   dec + 0x18 .. 0x50   the `struct vtb`, whose four table pointers
 *                        VTBv32_init writes
 *
 * Nothing V32OrgNextState itself writes is inside any of them, and the
 * ARGUMENTS it passes to those callees remain observable outside them --
 * `V32FP_SHORT_2C`, `V32FP_SHORT_2E`, the scrambler's group and taps, the
 * coder's mode and shift, `vtb`'s nsub/grid/mask/shift, `dec`'s short_04.  So
 * a wrong mode argument is still caught; only the pointer words are given up.
 *
 * ---------------------------------------------------------------------------
 * WHY NOTHING ALLOCATES
 *
 * Four of the callees allocate when handed an empty object, and two heap
 * pointers are two different addresses for ever.  The fixture is built so the
 * reuse path is taken every time instead:
 *
 *   FPM_SRE_init   `sre->cfg.coeffs` is preset to SREv32_CFG's own 180, so the
 *                  `cfg.coeffs < cfg->coeffs` realloc test is false and the
 *                  four buffers the fixture supplies are kept.
 *   FPM_MTD_create  a non-NULL state supplies its own accumulator array.
 *   FPM_TONE_create the same, and `cfg.len` is zero so no kernel is touched.
 *   VTBv32_init     SetRxModeV32 passes `alloc` zero; `vtb->paths` is the
 *                  fixture's 128-entry array.
 *
 * `SetAdaptEcV32`'s RESET arm -- the one that calls `FPM_ECC_init`, which does
 * allocate -- is not reachable from this function: the three arms it uses are
 * OFF, ON and SLOW.
 *
 * ---------------------------------------------------------------------------
 * THE SEPARATING TRIALS
 *
 * Every one of these is a NAMED WRONG READING that the sweep is shaped to
 * reach, and each has a counter asserted non-zero at the end:
 *
 *   - all 35 handshake states, so no arm is untested and the nine that must
 *     fall to the default (W..DONE) are shown to do nothing;
 *   - a state of -1 and of 0x7fff, because the object's bound is UNSIGNED
 *     against 0x21 on a SIGN-EXTENDED load: a `movzwl` reading would send
 *     0xffff somewhere else entirely;
 *   - hdx + 0x76 swept over all six modes, which this function must ignore;
 *   - hdx + 0x7c both sides of 0x95f AND at 0x80000000, which separates the
 *     object's `jbe` from a signed `jle`;
 *   - hdx + 0xa8 both sides of 0x48 AND negative, which separates the
 *     object's SIGNED `jle` from an unsigned one;
 *   - a rate signal with no rate in common, so V32_STATE_S posts its fault,
 *     and one with a common rate, so it does not;
 *   - a negotiated rate above 2 and one at or below it, which are the two
 *     arms of V32_STATE_T's `smc->trellis_state` store;
 *   - both AGC coefficient pairs, [0] from V32_STATE_N and [1] from F and P,
 *     which a single shared pointer would confuse;
 *   - `dsplibs_debug_level` at 0 and at 2, so the trace is compared as text
 *     on both sides -- the format string, `V32StateName`'s answer and the
 *     state number all at once.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/v32hdxst.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v32cfg.h"
#include "dsplib/v32data.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32state.h"
#include "dsplib/vtb.h"

extern void ref_V32OrgNextState(void *modem);
extern unsigned int ref_dsplibs_debug_level;

/*
 * The blob's copies of every symbol this function or its callees can STORE
 * into the fixture.  Declared as byte arrays: only their addresses are used,
 * and that spelling works for a function symbol as well as for a table.
 */
extern char ref_TxHdxCarrierState[];
extern char ref_TxHdxNoCarrier[];
extern char ref_TxHdxScrSequence[];
extern char ref_TxHdxTRN[];
extern char ref_RxHdxData[];
extern char ref_RxHdxEpoch[];
extern char ref_RxHdxNoSignal[];
extern char ref_RxHdxNull[];
extern char ref_RxHdxPhsReversal[];
extern char ref_RxHdxRateSequence[];
extern char ref_RxHdxSTone[];
extern char ref_RxHdxSequence[];
extern char ref_V32_S_DATA_COEF[];
extern char ref_AGC_DEF_ALPHA[];
extern char ref_AGC_DEF_BETA[];

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x100
#define HDX_SIZE	0x100
#define FP_SIZE		0x5200		/* past fp + 0x50d4, the last field   */

#define VTB_PATHS	128		/* VTBv32_init clears 0..0x7f         */

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	fp[FP_SIZE];
	struct v32_dec	dec;
	struct fpm_tone	tone[3];
	struct fpm_mtd	mtd;
	short		mtd_acc[8];
	short		tone_blk[3][8];
	short		tone_acc[3][8];
	short		ecc_line[512];
	short		sre_coeff[256];
	short		sre_hist[64];
	short		sre_clk[16];
	short		sre_rms[32];
	struct vtb_path	vtb_paths[VTB_PATHS];
	short		hdxbuf[64];
	double		align;
};

static struct fix fa, fb;

static struct v32_modem *
modem_of(struct fix *f)
{
	return (struct v32_modem *)(void *)f->obj;
}

/* --------------------------------------------------------------------- */

struct pair {
	const void	*ours;
	const void	*theirs;
};

#define NPAIR	17
static struct pair pairs[NPAIR];

static void
build_pairs(void)
{
	int i = 0;

#define P(o, t)	do { pairs[i].ours = (const void *)(o); \
		     pairs[i].theirs = (const void *)(t); i++; } while (0)
	P(TxHdxCarrierState, ref_TxHdxCarrierState);
	P(TxHdxNoCarrier, ref_TxHdxNoCarrier);
	P(TxHdxScrSequence, ref_TxHdxScrSequence);
	P(TxHdxTRN, ref_TxHdxTRN);
	P(RxHdxData, ref_RxHdxData);
	P(RxHdxEpoch, ref_RxHdxEpoch);
	P(RxHdxNoSignal, ref_RxHdxNoSignal);
	P(RxHdxNull, ref_RxHdxNull);
	P(RxHdxPhsReversal, ref_RxHdxPhsReversal);
	P(RxHdxRateSequence, ref_RxHdxRateSequence);
	P(RxHdxSTone, ref_RxHdxSTone);
	P(RxHdxSequence, ref_RxHdxSequence);
	P(V32_S_DATA_COEF, ref_V32_S_DATA_COEF);
	P(&AGC_DEF_ALPHA[0], ref_AGC_DEF_ALPHA);
	P(&AGC_DEF_ALPHA[1], ref_AGC_DEF_ALPHA + sizeof(short));
	P(&AGC_DEF_BETA[0], ref_AGC_DEF_BETA);
	P(&AGC_DEF_BETA[1], ref_AGC_DEF_BETA + sizeof(short));
#undef P
}

/*
 * A differing word, mapped into a side-independent value.  Returns 1 if it
 * mapped the word to something, so the caller can count non-vacuity.
 */
static int
norm(int side, unsigned long w, unsigned long *out)
{
	const struct fix *f = side ? &fb : &fa;
	unsigned long base = (unsigned long)(const void *)f;
	int i;

	for (i = 0; i < NPAIR; i++) {
		const void *p = side ? pairs[i].theirs : pairs[i].ours;

		if (w == (unsigned long)p) {
			*out = 0x40000000ul + (unsigned long)i;
			return 1;
		}
	}
	if (w >= base && w < base + sizeof(*f)) {
		*out = 0x20000000ul + (w - base);
		return 1;
	}
	*out = w;
	return 0;
}

/* --------------------------------------------------------------------- */

struct window { size_t lo, hi; };

#define NWINDOW	4
static struct window skip[NWINDOW];

static void
build_windows(void)
{
	size_t fp = offsetof(struct fix, fp);
	size_t dec = offsetof(struct fix, dec);

	skip[0].lo = fp + 0x70;   skip[0].hi = fp + 0x78;	/* pps maps   */
	skip[1].lo = fp + 0x158;  skip[1].hi = fp + 0x170;	/* sre tables */
	skip[2].lo = fp + 0x228;  skip[2].hi = fp + 0x238;	/* fse cfg    */
	skip[3].lo = dec + 0x18;  skip[3].hi = dec + 0x50;	/* struct vtb */
}

static int
skipped(size_t off)
{
	int i;

	for (i = 0; i < NWINDOW; i++)
		if (off >= skip[i].lo && off < skip[i].hi)
			return 1;
	return 0;
}

/* --------------------------------------------------------------------- */

static long norm_hits;
static long words_compared;

static void
compare_fixtures(long trial)
{
	const unsigned char *a = (const unsigned char *)&fa;
	const unsigned char *b = (const unsigned char *)&fb;
	size_t off;
	int reported = 0;
	long bad = 0;

	for (off = 0; off + sizeof(unsigned long) <= sizeof(fa);
	     off += sizeof(unsigned long)) {
		unsigned long wa, wb, na, nb;

		if (skipped(off))
			continue;
		memcpy(&wa, a + off, sizeof(wa));
		memcpy(&wb, b + off, sizeof(wb));
		words_compared++;
		if (wa == wb)
			continue;
		if (norm(0, wa, &na) | norm(1, wb, &nb))
			norm_hits++;
		if (na == nb)
			continue;
		bad++;
		if (reported < 6) {
			reported++;
			diff_eq_int("differing word at fixture+%ld",
				    (long)na, (long)nb, (long)off);
		}
	}
	diff_eq_int("unexplained differing words, trial %ld", bad, 0, trial);
}

/* --------------------------------------------------------------------- */

static void
put_ptr(void *p, int off, const void *v)
{
	*(const void **)(void *)((unsigned char *)p + off) = v;
}

static void
put_short(void *p, int off, short v)
{
	*(short *)(void *)((unsigned char *)p + off) = v;
}

static void
put_int(void *p, int off, int v)
{
	*(int *)(void *)((unsigned char *)p + off) = v;
}

static short
get_short(const void *p, int off)
{
	return *(const short *)(const void *)((const unsigned char *)p + off);
}

/* The trial's knobs, so one description drives both sides identically. */
struct trial {
	short		state;		/* hdx + 0x74                         */
	short		mode;		/* hdx + 0x76, which must be ignored  */
	int		left;		/* hdx + 0x78                         */
	int		spent;		/* hdx + 0x7c                         */
	int		total;		/* hdx + 0x80                         */
	short		a8;		/* hdx + 0xa8                         */
	short		rate_index;	/* fp + 0x2a, the local station's     */
	unsigned short	reg[V32HDX_NREGS];
	unsigned	seed;
};

static void
fixture(struct fix *f, const struct trial *t)
{
	struct fpm_ecc *ecc;
	struct fpm_sre *sre;
	struct fpm_fse *fse;
	struct vtb *vtb;
	unsigned s = t->seed ? t->seed : 1u;
	int i;

	memset(f, 0, sizeof(*f));

	/* A deterministic wash over the two blocks the callees write into. */
	for (i = 0; i < (int)sizeof(f->ecc_line) / 2; i++) {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		f->ecc_line[i] = (short)(s & 0x3fff);
	}
	for (i = 0; i < (int)sizeof(f->sre_hist) / 2; i++) {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		f->sre_hist[i] = (short)(s & 0x3fff);
	}

	/* ------------------------------------------------ the instance */
	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);
	put_ptr(f->obj, V32_OBJ_FP, f->fp);
	f->obj[V32_OBJ_STATUS] = 0x77;		/* a value no arm writes  */
	f->obj[V32_OBJ_FLAGS] = 0x81;		/* bits 0x01 and 0x80 set */
	f->obj[0x32] = 0x11;
	put_short(f->obj, V32_OBJ_SYMLEN_SEL, 0);
	*(unsigned short *)(void *)(f->obj + V32_OBJ_EC_NEAR_DELAY) = 0x10;

	/* -------------------------------------- the half-duplex context */
	put_short(f->hdx, V32HDX_STATE, t->state);
	put_short(f->hdx, V32HDX_MODE, t->mode);
	put_int(f->hdx, 0x78, t->left);
	put_int(f->hdx, 0x7c, t->spent);
	put_int(f->hdx, 0x80, t->total);
	put_short(f->hdx, 0x46, 0x1234);
	put_short(f->hdx, 0x84, 0x5678);
	put_int(f->hdx, 0x90, 0x0badf00d);
	put_short(f->hdx, 0x94, 0x0040);
	put_short(f->hdx, 0x96, 0x0021);
	put_short(f->hdx, 0x98, 8);
	put_short(f->hdx, 0x9a, 8);
	put_short(f->hdx, 0x9c, 8);
	put_short(f->hdx, V32HDX_SYMBOL_LEN, 12);
	put_short(f->hdx, V32HDX_SAMPLE_LEN, 36);
	put_short(f->hdx, 0xa8, t->a8);

	/*
	 * A sentinel neither side can produce, so "installed nothing" and
	 * "installed something" are distinguishable and both sides agree on
	 * the sentinel by construction.
	 */
	put_ptr(f->hdx, V32HDX_TXSTATE, (const void *)0);
	put_ptr(f->hdx, V32HDX_RXSTATE, (const void *)0);

	put_ptr(f->hdx, V32_HDX_TONE0, &f->tone[0]);
	put_ptr(f->hdx, V32_HDX_TONE1, &f->tone[1]);
	put_ptr(f->hdx, V32_HDX_TONE2, &f->tone[2]);
	put_ptr(f->hdx, V32_HDX_MTD, &f->mtd);
	put_ptr(f->hdx, V32_HDX_BUF_A4, f->hdxbuf);

	for (i = 0; i < V32HDX_NREGS; i++)
		put_short(f->hdx, V32HDX_REGS + 2 * i, (short)t->reg[i]);

	/* The generator and detector state, so a wrong Init* is visible. */
	put_short(f->hdx, V32HDX_GEN_INDEX, 0x0101);
	put_short(f->hdx, V32HDX_GEN_INDEX_MASK, 0x0202);
	put_short(f->hdx, V32HDX_GEN_WIDTH, 0x0303);
	put_short(f->hdx, V32HDX_GEN_MASK, 0x0404);
	put_short(f->hdx, V32HDX_GEN_PATTERN, 0x0505);
	put_short(f->hdx, V32HDX_DET_WIDTH, 0x0606);
	put_int(f->hdx, V32HDX_DET_OUT_MASK, 0x07070707);
	put_int(f->hdx, V32HDX_DET_TARGET, 0x08080808);
	put_int(f->hdx, V32HDX_DET_MASK, 0x09090909);
	put_int(f->hdx, V32HDX_DET_REG, 0x0a0a0a0a);
	put_int(f->hdx, V32HDX_DET_MATCH, 0x0b0b0b0b);

	/* -------------------------------------------- the datapump block */
	put_short(f->fp, V32FP_TX_RATE_INDEX, t->rate_index);
	put_short(f->fp, V32FP_RX_RATE_INDEX, t->rate_index);

	/* The two scramblers' absolute tap positions, which the setters read. */
	put_short(f->fp, V32FP_SCRAMBLER + V32_SDM_TAP1_POS, 18);
	put_short(f->fp, V32FP_SCRAMBLER + V32_SDM_TAP2_POS, 23);
	put_short(f->fp, V32FP_DESCRAMBLER + V32_SDM_TAP1_POS, 18);
	put_short(f->fp, V32FP_DESCRAMBLER + V32_SDM_TAP2_POS, 23);

	ecc = (struct fpm_ecc *)(void *)(f->fp + V32FP_ECC);
	ecc->cfg.far_lag = 4;
	ecc->cfg.near_taps = 6;
	ecc->cfg.far_taps = 2;
	ecc->line = f->ecc_line;
	ecc->line_len = 24;
	ecc->mu = 0x29;

	sre = (struct fpm_sre *)(void *)(f->fp + V32FP_SRE);
	sre->cfg.coeffs = 180;			/* == SREv32_CFG's: no realloc */
	sre->coeff = f->sre_coeff;
	sre->hist = f->sre_hist;
	sre->clk = f->sre_clk;
	sre->rms_buf = f->sre_rms;

	fse = (struct fpm_fse *)(void *)(f->fp + V32FP_FSE);
	fse->cfg.owner = &f->dec;
	fse->mu_sel = 0x55;
	fse->sym_count = 0x66;

	vtb = &f->dec.vtb;
	vtb->paths = f->vtb_paths;

	/* --------------------------------------------- the tone objects */
	for (i = 0; i < 3; i++) {
		f->tone[i].cfg.freq = (short)(1800 + 100 * i);
		f->tone[i].cfg.scale = 16384;
		f->tone[i].cfg.ratio = 0x2000;
		f->tone[i].cfg.f08 = 0x1000;
		f->tone[i].cfg.min_level = (short)(0x0400 << i);
		f->tone[i].cfg.damp = 30000;
		f->tone[i].cfg.len = 0;		/* no kernel: nothing allocates */
		f->tone[i].cfg.extra = 0;
		/*
		 * `FPM_TONE_create` writes the phase-reversal biquad and its
		 * accumulator UNCONDITIONALLY, through pointers a caller-owned
		 * state has to supply; `kernel` and `history` stay NULL
		 * because `cfg.len` is zero and only that loop reads them.
		 */
		f->tone[i].rev_block = f->tone_blk[i];
		f->tone[i].rev_acc = f->tone_acc[i];
	}

	/* ----------------------------------------- the S-tone detector */
	f->mtd.cfg.coeff = (const short *)0;
	f->mtd.cfg.tones = 1;
	f->mtd.cfg.ratio = 0x4000;
	f->mtd.cfg.min_level = 100;
	f->mtd.acc = f->mtd_acc;
}

/* --------------------------------------------------------------------- */

static int rc_total;

/* Non-vacuity counters, every one asserted at the end. */
static long sep_state_changed;
static long sep_state_kept;
static long sep_tx_installed;
static long sep_rx_installed;
static long sep_b_taken, sep_b_skipped;
static long sep_d_taken, sep_d_skipped;
static long sep_fault_posted, sep_fault_not;
static long sep_rate_high, sep_rate_low;
static long sep_default_arm;
static long sep_trace_on, sep_trace_off;
static long sep_modes_swept;

static void
run_one(const char *what, const struct trial *t, int trace, long trial)
{
	short before;

	fixture(&fa, t);
	fixture(&fb, t);
	before = get_short(fa.hdx, V32HDX_STATE);

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = trace;
	dsplibs_debug_level = trace ? 2u : 0u;
	ref_dsplibs_debug_level = dsplibs_debug_level;

	V32OrgNextState(modem_of(&fa));
	ref_V32OrgNextState(fb.obj);

	dsplib_debug_capture_on = 0;

	diff_begin(what);

	compare_fixtures(trial);

	diff_eq_int("trace lines (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), trial);
	diff_eq_int("trace text matches, trial %ld",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, trial);

	/* Non-vacuity. */
	if (get_short(fa.hdx, V32HDX_STATE) != before)
		sep_state_changed++;
	else
		sep_state_kept++;
	if (*(void **)(void *)(fa.hdx + V32HDX_TXSTATE) != (void *)0)
		sep_tx_installed++;
	if (*(void **)(void *)(fa.hdx + V32HDX_RXSTATE) != (void *)0)
		sep_rx_installed++;
	if (trace)
		sep_trace_on += (dsplib_debug_capture_lines(0) > 0);
	else
		sep_trace_off++;

	rc_total |= diff_end();
}

/* --------------------------------------------------------------------- */

/*
 * The rate signals.  0 has none of the bits `v32seq.h` tabulates, so the
 * common-rate ladder reports V32_RATE_NONE; 0x0ff9 has all of them, so it
 * reports whatever the local station's own index is.
 */
#define SEQ_NONE	0x0000
#define SEQ_ALL		0x0ff9

static void
base_trial(struct trial *t, short state)
{
	int i;

	memset(t, 0, sizeof(*t));
	t->state = state;
	t->mode = V32_MODE_ORIGINATE;
	t->left = 0x40;
	t->spent = 0x30;
	t->total = 0x180;
	t->a8 = 0x10;
	t->rate_index = 5;			/* 14400, so rate > 2      */
	for (i = 0; i < V32HDX_NREGS; i++)
		t->reg[i] = SEQ_ALL;
	t->seed = 0x1234567u + (unsigned)state;
}

static void
sweep_states(void)
{
	static char name[64];
	struct trial t;
	int st;

	for (st = 0; st < V32_STATE_COUNT; st++) {
		base_trial(&t, (short)st);
		sprintf(name, "V32OrgNextState: state %d, plain", st);
		run_one(name, &t, st & 1, (long)st);
		if (st >= V32_STATE_W && st <= V32_STATE_DONE)
			sep_default_arm++;
		if (st == V32_STATE_DONT_CARE)
			sep_default_arm++;
	}
}

static void
sweep_modes(void)
{
	static char name[64];
	struct trial t;
	int m;

	/*
	 * The mode selects WHICH next-state function runs and is not read by
	 * any of them; swept at a state with plenty of side effects so a read
	 * of hdx + 0x76 could not hide.
	 */
	for (m = 0; m < V32_NEXTSTATE_COUNT; m++) {
		base_trial(&t, V32_STATE_N);
		t.mode = (short)m;
		sprintf(name, "V32OrgNextState: state N, mode %d", m);
		run_one(name, &t, 1, 100L + m);
		sep_modes_swept++;
	}
}

static void
sweep_bounds(void)
{
	struct trial t;

	/* V32_STATE_B: the UNSIGNED test against 0x95f. */
	base_trial(&t, V32_STATE_B);
	t.spent = 0x95f;
	run_one("V32OrgNextState: state B, spent == 0x95f", &t, 1, 200L);
	sep_b_skipped++;

	base_trial(&t, V32_STATE_B);
	t.spent = 0x960;
	run_one("V32OrgNextState: state B, spent == 0x960", &t, 1, 201L);
	sep_b_taken++;

	base_trial(&t, V32_STATE_B);
	t.spent = (int)0x80000000;
	run_one("V32OrgNextState: state B, spent negative as a signed int",
		&t, 1, 202L);
	sep_b_taken++;

	/* V32_STATE_D: the SIGNED 16-bit test against 0x48. */
	base_trial(&t, V32_STATE_D);
	t.a8 = 0x48;
	run_one("V32OrgNextState: state D, a8 == 0x48", &t, 1, 210L);
	sep_d_skipped++;

	base_trial(&t, V32_STATE_D);
	t.a8 = 0x49;
	run_one("V32OrgNextState: state D, a8 == 0x49", &t, 1, 211L);
	sep_d_taken++;

	base_trial(&t, V32_STATE_D);
	t.a8 = (short)0x8000;
	run_one("V32OrgNextState: state D, a8 most negative", &t, 1, 212L);
	sep_d_skipped++;

	/* The switch bound, which is unsigned over a sign-extended load. */
	base_trial(&t, (short)-1);
	run_one("V32OrgNextState: state -1", &t, 1, 220L);

	base_trial(&t, (short)0x7fff);
	run_one("V32OrgNextState: state 0x7fff", &t, 1, 221L);

	base_trial(&t, (short)0x8000);
	run_one("V32OrgNextState: state 0x8000", &t, 1, 222L);
}

static void
sweep_rates(void)
{
	static char name[64];
	struct trial t;
	int r, i;

	/* V32_STATE_S: the fault, and the arm that does not post it. */
	base_trial(&t, V32_STATE_S);
	for (i = 0; i < V32HDX_NREGS; i++)
		t.reg[i] = SEQ_NONE;
	run_one("V32OrgNextState: state S, no rate in common", &t, 1, 300L);
	sep_fault_posted++;

	base_trial(&t, V32_STATE_S);
	run_one("V32OrgNextState: state S, a rate in common", &t, 1, 301L);
	sep_fault_not++;

	/*
	 * V32_STATE_T's `rate > 2` arm, and V32_STATE_V's V32_CONNECT index.
	 * The local index drives what the ladder settles on, so sweeping it
	 * over its whole range walks both.
	 */
	for (r = 0; r < V32_RATE_COUNT; r++) {
		base_trial(&t, V32_STATE_T);
		t.rate_index = (short)r;
		sprintf(name, "V32OrgNextState: state T, rate index %d", r);
		run_one(name, &t, 1, 310L + r);
		if (r > 2)
			sep_rate_high++;
		else
			sep_rate_low++;

		base_trial(&t, V32_STATE_U);
		t.rate_index = (short)r;
		sprintf(name, "V32OrgNextState: state U, rate index %d", r);
		run_one(name, &t, 1, 320L + r);

		base_trial(&t, V32_STATE_V);
		t.rate_index = (short)r;
		sprintf(name, "V32OrgNextState: state V, rate index %d", r);
		run_one(name, &t, 1, 330L + r);
	}
}

static void
sweep_agc(void)
{
	struct trial t;

	/*
	 * The three steps that install an AGC coefficient pair.  N takes
	 * AGC_DEF_*[0] and F and P take [1]; a reconstruction that shared one
	 * pointer would pass every other trial in this file.
	 */
	base_trial(&t, V32_STATE_N);
	run_one("V32OrgNextState: state N, the acquisition AGC pair", &t, 1,
		400L);
	base_trial(&t, V32_STATE_F);
	run_one("V32OrgNextState: state F, the tracking AGC pair", &t, 1, 401L);
	base_trial(&t, V32_STATE_P);
	run_one("V32OrgNextState: state P, the tracking AGC pair", &t, 1, 402L);
}

static void
sweep_countdowns(void)
{
	static char name[64];
	struct trial t;
	int k;

	/*
	 * V32_STATE_P snapshots +0x80 minus +0x78 into +0x46 and V32_STATE_R
	 * folds it back modulo eight, so the two are only separated by
	 * differences that are not multiples of eight.
	 */
	for (k = 0; k < 8; k++) {
		base_trial(&t, V32_STATE_P);
		t.left = 0x40 + k;
		t.total = 0x180;
		sprintf(name, "V32OrgNextState: state P, left 0x%x", t.left);
		run_one(name, &t, 0, 500L + k);

		base_trial(&t, V32_STATE_R);
		t.left = 0x40 + k;
		t.total = 0x180;
		sprintf(name, "V32OrgNextState: state R, left 0x%x", t.left);
		run_one(name, &t, 0, 510L + k);
	}

	/* V32_STATE_I rounds its countdown up to even, from both parities. */
	for (k = 0; k < 2; k++) {
		int i;

		base_trial(&t, V32_STATE_I);
		for (i = 0; i < V32HDX_NREGS; i++)
			t.reg[i] = (unsigned short)k;
		sprintf(name, "V32OrgNextState: state I, reg0 %d", k);
		run_one(name, &t, 0, 520L + k);
	}
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	build_pairs();
	build_windows();

	sweep_states();
	sweep_modes();
	sweep_bounds();
	sweep_rates();
	sweep_agc();
	sweep_countdowns();

	diff_begin("v32nsorg separating trials");
	diff_eq_int("words compared (%ld)", words_compared > 0, 1,
		    words_compared);
	diff_eq_int("pointer words normalised (%ld)", norm_hits > 0, 1,
		    norm_hits);
	diff_eq_int("a step advanced the state (%ld)", sep_state_changed > 0, 1,
		    sep_state_changed);
	diff_eq_int("a step left the state alone (%ld)", sep_state_kept > 0, 1,
		    sep_state_kept);
	diff_eq_int("a transmit state was installed (%ld)",
		    sep_tx_installed > 0, 1, sep_tx_installed);
	diff_eq_int("a receive state was installed (%ld)", sep_rx_installed > 0,
		    1, sep_rx_installed);
	diff_eq_int("state B's test passed (%ld)", sep_b_taken > 0, 1,
		    sep_b_taken);
	diff_eq_int("state B's test failed (%ld)", sep_b_skipped > 0, 1,
		    sep_b_skipped);
	diff_eq_int("state D's test passed (%ld)", sep_d_taken > 0, 1,
		    sep_d_taken);
	diff_eq_int("state D's test failed (%ld)", sep_d_skipped > 0, 1,
		    sep_d_skipped);
	diff_eq_int("state S posted its fault (%ld)", sep_fault_posted > 0, 1,
		    sep_fault_posted);
	diff_eq_int("state S did not post it (%ld)", sep_fault_not > 0, 1,
		    sep_fault_not);
	diff_eq_int("state T saw a rate above 2 (%ld)", sep_rate_high > 0, 1,
		    sep_rate_high);
	diff_eq_int("state T saw a rate at or below 2 (%ld)", sep_rate_low > 0,
		    1, sep_rate_low);
	diff_eq_int("the default arm was reached (%ld)", sep_default_arm > 0, 1,
		    sep_default_arm);
	diff_eq_int("the trace printed (%ld)", sep_trace_on > 0, 1,
		    sep_trace_on);
	diff_eq_int("the trace was silent (%ld)", sep_trace_off > 0, 1,
		    sep_trace_off);
	diff_eq_int("every mode was swept (%ld)",
		    sep_modes_swept == V32_NEXTSTATE_COUNT, 1, sep_modes_swept);
	rc |= diff_end();

	return rc | rc_total;
}
