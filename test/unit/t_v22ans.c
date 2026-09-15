/*
 * t_v22ans.c -- differential test of v22_data and v22_ans_rmloop2.
 *
 * TWO GRAPHS, six buffers and a transcript.  Both handlers are state machines
 * over a real `struct v22fp`, so the fixture is t_v22rate.c's: two graphs
 * built by `V22FP_create` -- ours on both sides, because t_v22fpcreate has
 * established that ours and the blob's agree byte for byte -- the reference
 * driven through one and the reconstruction through the other, and then the
 * two graphs compared entire, structs with pointers blanked plus the
 * twenty-eight heap regions behind them.  On top of that the six caller
 * buffers are compared, because these two functions exist to fill them, and
 * so is the debug transcript.
 *
 * A SINGLE CALL EXERCISES ONE ARM, so nothing here is driven once.  Between
 * them the two functions have thirteen conditionals and four of the arms are
 * only reachable from state the CALLEES move.  Three fixtures open those up:
 *
 *   - SILENCE PLUS A POKED `fse.mse`.  A quiet block produces no symbols, so
 *     `V22_FSE_receive`'s loop body never runs and `fse.mse` survives the
 *     call; `GetSignalQuality` inside `v22_data` then reports exactly the
 *     value the test poked, and both arms of the retrain-level comparison
 *     become reachable by assignment.  `hdx->r08`, `hdx->carrier_loss_blocks` and
 *     `hdx->gtimer` are the handlers' own state and are poked the same way.
 *     Three of those pokes carry values a working modem never reaches, and
 *     they are here on purpose: every comparison against `hdx->r08`,
 *     `hdx->carrier_loss_blocks * 20` and `ReadGTimer`'s return is UNSIGNED in the object,
 *     and a negative input is the only thing that tells that reading apart
 *     from a signed one.  Without them four otherwise-correct sign mutants
 *     survive.
 *
 *   - `sre.pll_acc` MADE NON-ZERO, because `TxClockSync` writes three times
 *     it into `pps.cfg.step` and that field starts at zero.  With the baud
 *     field left alone the call writes 0 over 0, is invisible in every buffer
 *     and every struct, and BOTH of the gates on it -- one per function --
 *     accept any bit of the status byte.
 *
 *   - A TEST `fse.decision`.  `V22_FSE_receive` writes its symbol buffer
 *     straight from `state->decision(...)`, and the field is a function
 *     pointer in a graph THIS TEST OWNS, so installing one of our own on both
 *     sides makes the demodulated symbol stream an input rather than an
 *     output.  That is what reaches `Detect_Retrain`'s firing arm -- the
 *     quadrant sequence it wants is 3,0,3,0,... with a period-2 repeat, which
 *     no amount of noise into a cold equaliser will produce -- and both arms
 *     of `Detect_1s` in the three `v22_ans_rmloop2` sub-states that call it.
 *     Both graphs get the same function, so the comparison is unaffected;
 *     `blank_dsp` already blanks the field, and the SEPARATE `dec_id` check
 *     below is what keeps a wrong `SetRxRate` from hiding in that blanking.
 *
 *   - `dsplibs_debug_level` AT TWO, with the harness's capture on.  Five of
 *     the object's own format strings live in `v22_data`, one of them with
 *     two `%d` arguments, and at the shipping level of zero every one of them
 *     is dead in both the blob and the reconstruction.  Each sweep is run at
 *     level 0 and level 2 and the two transcripts are compared as text.
 *
 * WHAT THE GUARDS AT THE BOTTOM ARE FOR.  A handler whose other arm was never
 * taken agrees with a stub, and two of the arms here are exactly the shape
 * finding F8528 describes -- an arm that jumps into the middle of another one
 * and shares its tail.  `v22_ans_rmloop2` sub-states 1 and 2 differ only in
 * the ReadGTimer limit they compare against, 1300 ms and 5000 ms, and the
 * timer is swept across both boundaries in both sub-states for that reason.
 * `v22_data`'s retrain-request path is the other: it leaves WITHOUT the
 * common tail, so a reconstruction that let it fall through would set
 * `fp->flags` and clear `fp->status` and pass every check that never reached
 * it.
 *
 * WHY `run_rmloop2` BUILDS A GRAPH PER CALL AND THE OTHER TWO DO NOT.  It was
 * written the other way round first -- one graph per configuration, forty-two
 * consecutive blocks through it -- and two of those blocks disagreed in the
 * TRANSMIT SAMPLES while the two graphs, the received symbols and both counts
 * all still matched.  Driving the BLOB'S OWN function on both sides reproduced
 * the same disagreement at the same two blocks, so it is not the
 * reconstruction: something under `ModDataV22` depends on memory this fixture
 * does not control identically, and forty blocks of the test slicer in
 * sub-state 3 is enough to reach it.  A fresh graph per call is the honest
 * fixture for a sweep whose every call is a single arm anyway; it is recorded
 * here so that nobody re-derives it, and so that nobody reads the per-call
 * create as a stylistic choice.
 *
 * THREE MUTANTS ARE KNOWN-EQUIVALENT and are not worth chasing.  Dropping the
 * discarded `GetSignalQuality` call in `v22_data` changes nothing, because the
 * accessor is pure and the object calls it twice.  Dropping the FIRST of
 * sub-state 2's two `RxClampV22` calls changes nothing, because the second
 * writes the same twelve values and the same count.  And forcing
 * `MakeTxData`'s pattern to V22_TXDATA_SYMBOL_10 at 1200 changes nothing,
 * because the 1200 scrambler keeps two bits and 2 and 10 agree in both of
 * them -- the 2400 direction of that same ternary IS caught.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22ans.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22rate.h"
#include "dsplib/v22status.h"
#include "dsplib/v22txtab.h"

extern void ref_v22_data(struct v22fp *fp, unsigned short *txsym, short *txout,
			 short *rxin, unsigned short *rxsym,
			 unsigned short *txcount, unsigned short *rxcount);
extern void ref_v22_ans_rmloop2(struct v22fp *fp, unsigned short *txsym,
				short *txout, short *rxin,
				unsigned short *rxsym,
				unsigned short *txcount,
				unsigned short *rxcount);

extern unsigned int ref_dsplibs_debug_level;

/* The blob's copies of everything SetTxRate / SetRxRate can install. */
extern const short ref_SMCv22_IMAP_1200BPS[16];
extern const short ref_SMCv22_QMAP_1200BPS[16];
extern const short ref_SMCv22_IMAP_2400BPS[16];
extern const short ref_SMCv22_QMAP_2400BPS[16];
extern unsigned short ref_FSEv22_decision12(struct v22_fse *state,
					    short *angle, short *mag);
extern unsigned short ref_FSEv22_decision24(struct v22_fse *state,
					    short *angle, short *mag);

/* ------------------------------------------------------------------------ */

static unsigned long rng_state;

static void
rng_seed(unsigned long s)
{
	rng_state = s | 1UL;
}

static unsigned
rng_next(void)
{
	rng_state = rng_state * 1103515245UL + 12345UL;
	return (unsigned)((rng_state >> 16) & 0xffffU);
}

/* ------------------------------------------------------------------------ */

#define MAX_REGIONS 40

struct region {
	const char *name;
	const void *p;
	unsigned long n;
};

/* Every heap region the graph owns, in a fixed order.  t_v22rate.c's list. */
static int
regions_of(struct v22fp *fp, struct region *r)
{
	struct v22fp_dsp *d = fp->dsp;
	struct v22fp_hdx *h = fp->hdx;
	int n = 0;

#define REGION(nm, ptr, bytes) \
	do { r[n].name = (nm); r[n].p = (ptr); r[n].n = (bytes); n++; } while (0)

	REGION("dsp->smc_ring.sym", d->smc_ring.sym, 0x18);
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

	REGION("tone.kernel", h->tone->kernel,
	       (unsigned long)h->tone->cfg.len * 2);
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

	c.smc_ring.sym = NULL;
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
	c.pps.imap = NULL;	/* the retrain path's SetTxRate; see map_id */
	c.pps.qmap = NULL;	/* likewise                                 */
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
	c.fse.decision = NULL;	/* the retrain path's SetRxRate; see dec_id */
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

/* One check per buffer, naming the first differing byte. */
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
	diff_eq_int(what, i == n ? -1L : (long)i, -1L, input);
}

/*
 * Which of the four constellation maps a pointer is, and which slicer a
 * function pointer is.  t_v22rate.c's argument applies unchanged: both sides'
 * symbols map onto the same id, so "nothing was written" compares equal and
 * "the wrong one was written" still does not.  Both matter here because
 * `v22_data`'s retrain-request path calls `SetTxRate` and `SetRxRate`, and
 * `blank_dsp` blanks the three fields they write.
 */
static int
map_id(const short *p)
{
	if (p == SMCv22_IMAP_1200BPS || p == ref_SMCv22_IMAP_1200BPS)
		return 1;
	if (p == SMCv22_QMAP_1200BPS || p == ref_SMCv22_QMAP_1200BPS)
		return 2;
	if (p == SMCv22_IMAP_2400BPS || p == ref_SMCv22_IMAP_2400BPS)
		return 3;
	if (p == SMCv22_QMAP_2400BPS || p == ref_SMCv22_QMAP_2400BPS)
		return 4;
	return 0;
}

static unsigned short test_decision(struct v22_fse *state, short *angle,
				    short *mag);

static int
dec_id(v22_fse_decision d)
{
	if (d == FSEv22_decision12 || d == ref_FSEv22_decision12)
		return 1;
	if (d == FSEv22_decision24 || d == ref_FSEv22_decision24)
		return 2;
	if (d == test_decision)
		return 3;
	return 0;
}

static void
compare_graphs(struct v22fp *mine, struct v22fp *theirs, long tag)
{
	struct region ra[MAX_REGIONS];
	struct region rb[MAX_REGIONS];
	struct v22fp oa = blank_obj(mine);
	struct v22fp ob = blank_obj(theirs);
	struct v22fp_hdx ha = blank_hdx(mine->hdx);
	struct v22fp_hdx hb = blank_hdx(theirs->hdx);
	struct v22fp_dsp da = blank_dsp(mine->dsp);
	struct v22fp_dsp db = blank_dsp(theirs->dsp);
	struct fpm_tone ta = blank_tone(mine->hdx->tone);
	struct fpm_tone tb = blank_tone(theirs->hdx->tone);
	int n, m, i;

	diff_eq_obj("object", struct v22fp, &oa, &ob, tag);
	diff_eq_obj("hdx", struct v22fp_hdx, &ha, &hb, tag);
	diff_eq_obj("dsp", struct v22fp_dsp, &da, &db, tag);
	diff_eq_obj("tone", struct fpm_tone, &ta, &tb, tag);

	diff_eq_int("pps.imap identity", map_id(mine->dsp->pps.imap),
		    map_id(theirs->dsp->pps.imap), tag);
	diff_eq_int("pps.qmap identity", map_id(mine->dsp->pps.qmap),
		    map_id(theirs->dsp->pps.qmap), tag);
	diff_eq_int("fse.decision identity", dec_id(mine->dsp->fse.decision),
		    dec_id(theirs->dsp->fse.decision), tag);

	n = regions_of(mine, ra);
	m = regions_of(theirs, rb);
	diff_eq_int("same region count", n, m, tag);
	for (i = 0; i < n && i < m; i++)
		cmp_raw(ra[i].name, ra[i].p, rb[i].p, ra[i].n, tag);
}

/* ------------------------------------------------------------------------ */

/*
 * The test slicer.  It writes the two outputs `V22_FSE_receive` reads back --
 * the re-rotation angle and the decision magnitude -- with a constant each,
 * so the equaliser update downstream is deterministic, and returns the symbol
 * the test wants at that position.  Installed on BOTH graphs; the index is
 * reset before each call so that position 0 of the buffer always gets
 * `dec_pat[0]`.
 */
static unsigned short dec_pat[4];
static int dec_len = 1;
static int dec_idx;

static unsigned short
test_decision(struct v22_fse *state, short *angle, short *mag)
{
	(void)state;
	*angle = 0;
	*mag = 0;
	return dec_pat[dec_idx++ % dec_len];
}

static void
set_pattern(const unsigned short *p, int n)
{
	int i;

	for (i = 0; i < n; i++)
		dec_pat[i] = p[i];
	dec_len = n;
}

/* ------------------------------------------------------------------------ */

static struct v22fp_cfg
base_cfg(int mode, int rate, int f14, int f18)
{
	struct v22fp_cfg c;

	/* What `v22_create` passes, with the four dimensions left open. */
	c.mode = mode;
	c.rate = rate;
	c.f08 = 60000;
	c.f0c = 0;
	c.f10 = 700;
	c.f14 = f14;
	c.f18 = f18;
	return c;
}

/* ------------------------------------------------------------------------ */

#define BLOCK	160		/* V22_IIR_BLOCK; a short block leaves the
				 * mixer's tail uninitialised, see v22rate.c */
#define INBUF	512
#define SYMBUF	256
#define OUTBUF	2048

static short in_a[INBUF], in_b[INBUF];
static unsigned short rsym_a[SYMBUF], rsym_b[SYMBUF];
static unsigned short tsym_a[SYMBUF], tsym_b[SYMBUF];
static short out_a[OUTBUF], out_b[OUTBUF];
static short src[BLOCK];

static void
fill_block(short *dst, unsigned n, int amplitude)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		if (amplitude == 0)
			dst[i] = 0;
		else
			dst[i] = (short)((int)(rng_next() & 0x7fff)
					 % (2 * amplitude) - amplitude);
	}
}

/*
 * Set both sides' buffers to the same contents, poisoned outside the part the
 * call is expected to fill so that a short write shows up.
 */
static void
load_buffers(int amp, unsigned short txn)
{
	unsigned i;

	fill_block(src, BLOCK, amp);
	memset(in_a, 0, sizeof(in_a));
	memset(in_b, 0, sizeof(in_b));
	memcpy(in_a, src, sizeof(src));
	memcpy(in_b, src, sizeof(src));
	memset(rsym_a, 0x5a, sizeof(rsym_a));
	memset(rsym_b, 0x5a, sizeof(rsym_b));
	memset(out_a, 0x33, sizeof(out_a));
	memset(out_b, 0x33, sizeof(out_b));
	for (i = 0; i < SYMBUF; i++) {
		tsym_a[i] = (unsigned short)((i * 7 + 1) & 0x0f);
		tsym_b[i] = tsym_a[i];
	}
	(void)txn;
}

static void
compare_buffers(long tag)
{
	cmp_raw("rx sample buffer", in_b, in_a, sizeof(in_a), tag);
	cmp_raw("rx symbol buffer", rsym_b, rsym_a, sizeof(rsym_a), tag);
	cmp_raw("tx symbol buffer", tsym_b, tsym_a, sizeof(tsym_a), tag);
	cmp_raw("tx sample buffer", out_b, out_a, sizeof(out_a), tag);
}

static void
compare_transcript(long tag)
{
	const char *ours = dsplib_debug_capture_text(0);
	const char *theirs = dsplib_debug_capture_text(1);

	diff_eq_int("debug transcript", strcmp(ours, theirs) == 0, 1, tag);
	diff_eq_int("debug line count", dsplib_debug_capture_lines(0),
		    dsplib_debug_capture_lines(1), tag);
}

static void
set_debug(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ------------------------------------------------------------------------ */

/* Coverage, read off the REFERENCE side so the counts describe the blob. */
static long saw_bit11, saw_no_bit11;
static long saw_bit11_timeout, saw_bit11_running;
static long saw_scramble, saw_no_scramble;
static long saw_descramble, saw_no_descramble;
static long saw_txclock, saw_no_txclock;
static long saw_rx_symbols, saw_rx_empty;
static long saw_retrain_req, saw_quality_retrain, saw_carrier_back;
static long saw_no_carrier, saw_quiet_block;
static long saw_quality_high, saw_quality_low;
static long saw_carrier, saw_carrier_lost;
static long saw_loss_reported, saw_loss_pending;
static long saw_debug_lines;

/*
 * The quiet sweep.  `hdx->protocol` is left at what `V22FP_create` chose and the
 * input is silence, so `DemodDataV22` takes its disconnect return; that is
 * what makes `fse.mse`, `hdx->r08`, `hdx->carrier_loss_blocks` and `hdx->gtimer` pokeable
 * inputs rather than callee outputs.
 */
static int
run_data_quiet(void)
{
	static const short mses[] = { 0, 0x100, 0x7f37, 0x7f38, -1 };
	/* -40 is the entry whose value after the step has bit 15 set. */
	static const short r08s[] = { -40, 0, 100, 2980, 3000 };
	/*
	 * The negative is not a modem state -- `hdx->carrier_loss_blocks` only ever counts
	 * up -- but the bit 11 arm's comparison against `params.r08` is
	 * UNSIGNED in the object (`ja`), and a negative product is the only
	 * input that tells that reading apart from a signed one.
	 */
	static const int r3cs[] = { -100, 0, 15, 34, 35, 36, 2000, 3000 };
	long tag = 0;
	int rate, f14, f18, dbg, mi, ri, ci, en;

	diff_begin("v22_data, quiet");
	rng_seed(0x51c0ffeeUL);

	for (rate = 0; rate <= 1; rate++)
	 for (f14 = 0; f14 <= 1; f14++)
	  for (f18 = 0; f18 <= 1; f18++)
	   for (dbg = 0; dbg <= 1; dbg++) {
		struct v22fp_cfg cfg = base_cfg(0, rate, f14, f18);
		struct v22fp *a = V22FP_create(0, &cfg);
		struct v22fp *b = V22FP_create(0, &cfg);

		set_debug(dbg ? 2u : 0u);

		for (mi = 0; mi < (int)(sizeof(mses) / sizeof(mses[0])); mi++)
		 for (ri = 0; ri < (int)(sizeof(r08s) / sizeof(r08s[0])); ri++)
		  for (ci = 0; ci < (int)(sizeof(r3cs) / sizeof(r3cs[0]));
		       ci++) {
			unsigned short tcount_a, tcount_b;
			unsigned short rcount_a, rcount_b;

			en = (mi + ri + ci) & 7;

			a->dsp->fse.mse = mses[mi];
			b->dsp->fse.mse = mses[mi];
			a->hdx->r08 = r08s[ri];
			b->hdx->r08 = r08s[ri];
			a->hdx->carrier_loss_blocks = r3cs[ci];
			b->hdx->carrier_loss_blocks = r3cs[ci];
			a->hdx->gtimer = 0;
			b->hdx->gtimer = 0;
			/* The three enable words behind st.flags 0..2. */
			a->dsp->scrambler_on = (en >> 0) & 1;
			b->dsp->scrambler_on = (en >> 0) & 1;
			a->dsp->descrambler_on = (en >> 1) & 1;
			b->dsp->descrambler_on = (en >> 1) & 1;
			a->dsp->r20 = (en >> 2) & 1;
			b->dsp->r20 = (en >> 2) & 1;
			/*
			 * `TxClockSync` writes `3 * sre.pll_acc` into
			 * `pps.cfg.step`, which is zero on a fresh graph -- so
			 * with the baud field left alone the call is INVISIBLE
			 * and a wrong gate on it cannot be caught.  A non-zero
			 * value here makes the write show up in the transmit
			 * samples of the block after it.
			 */
			a->dsp->sre.pll_acc = 3;
			b->dsp->sre.pll_acc = 3;

			load_buffers(0, 12);
			tcount_a = tcount_b = 12;
			rcount_a = rcount_b = BLOCK;

			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			ref_v22_data(a, tsym_a, out_a, in_a, rsym_a,
				     &tcount_a, &rcount_a);
			v22_data(b, tsym_b, out_b, in_b, rsym_b,
				 &tcount_b, &rcount_b);
			dsplib_debug_capture_on = 0;

			diff_eq_int("tx count", tcount_b, tcount_a, tag);
			diff_eq_int("rx count", rcount_b, rcount_a, tag);
			compare_buffers(tag);
			compare_graphs(b, a, tag);
			if (dbg)
				compare_transcript(tag);
			saw_debug_lines += dsplib_debug_capture_lines(1);

			if (a->params.flags & V22_PARAMS_BIT11) {
				saw_bit11++;
				if (a->status == V22_ST_NO_CARRIER)
					saw_bit11_timeout++;
				else
					saw_bit11_running++;
			} else {
				saw_no_bit11++;
			}
			if (a->dsp->scrambler_on & 1)
				saw_scramble++;
			else
				saw_no_scramble++;
			if (a->dsp->r20 & 1)
				saw_txclock++;
			else
				saw_no_txclock++;
			if (rcount_a == 0)
				saw_rx_empty++;
			else
				saw_rx_symbols++;
			if (a->dsp->sre.active)
				saw_carrier++;
			else
				saw_carrier_lost++;
			if ((unsigned short)mses[mi] > V22_DATA_RETRAIN_LEVEL)
				saw_quality_high++;
			else
				saw_quality_low++;
			if (a->status == V22_ST_RETRAIN && r3cs[ci] <= 14)
				saw_quality_retrain++;
			if (a->status == V22_ST_NO_CARRIER
			    && (a->params.flags & V22_PARAMS_BIT11) == 0)
				saw_no_carrier++;
			if (a->status == 0)
				saw_quiet_block++;
			if (a->status == V22_ST_NO_CARRIER)
				saw_loss_reported++;
			/*
			 * The "won't be reported" arm: the loss timer has
			 * started but has not reached `params.carrier_loss_ms` yet, so
			 * the block ends quiet with a non-zero count.
			 */
			if (a->hdx->carrier_loss_blocks > 0 && a->status == 0
			    && (a->params.flags & V22_PARAMS_BIT11) == 0)
				saw_loss_pending++;
			tag++;
		  }

		V22FP_delete(a);
		V22FP_delete(b);
	   }

	set_debug(0u);
	return diff_end();
}

/*
 * The loud sweep.  `hdx->protocol` is forced non-zero so `DemodDataV22` skips its
 * disconnect return and runs the whole chain, and the test slicer supplies
 * the symbol stream.  This is the only pass that reaches `Detect_Retrain`'s
 * firing arm, the descrambler, and the carrier-back retrain.
 */
static int
run_data_loud(void)
{
	static const unsigned short p_retrain[2] = { 12, 0 };
	static const unsigned short p_flat[1] = { 3 };
	static const unsigned short p_zero[1] = { 0 };
	static const int r3cs[] = { 0, 14, 15 };
	long tag = 0;
	int rate, f18, dbg, pi, ci, blk, en;

	diff_begin("v22_data, loud");
	rng_seed(0x7ea51de5UL);

	for (rate = 0; rate <= 1; rate++)
	 for (f18 = 0; f18 <= 1; f18++)
	  for (dbg = 0; dbg <= 1; dbg++)
	   for (pi = 0; pi < 3; pi++)
	    for (ci = 0; ci < 3; ci++) {
		struct v22fp_cfg cfg = base_cfg(0, rate, 0, f18);
		struct v22fp *a = V22FP_create(0, &cfg);
		struct v22fp *b = V22FP_create(0, &cfg);

		set_debug(dbg ? 2u : 0u);

		for (blk = 0; blk < 8; blk++) {
			unsigned short tcount_a, tcount_b;
			unsigned short rcount_a, rcount_b;

			en = (blk + pi + ci) & 7;

			if (pi == 0)
				set_pattern(p_retrain, 2);
			else if (pi == 1)
				set_pattern(p_flat, 1);
			else
				set_pattern(p_zero, 1);

			/* Non-zero: skip DemodDataV22's disconnect return. */
			a->hdx->protocol = 1;
			b->hdx->protocol = 1;
			a->hdx->carrier_loss_blocks = r3cs[ci];
			b->hdx->carrier_loss_blocks = r3cs[ci];
			a->dsp->fse.decision = test_decision;
			b->dsp->fse.decision = test_decision;
			a->dsp->scrambler_on = (en >> 0) & 1;
			b->dsp->scrambler_on = (en >> 0) & 1;
			a->dsp->descrambler_on = (en >> 1) & 1;
			b->dsp->descrambler_on = (en >> 1) & 1;
			a->dsp->r20 = (en >> 2) & 1;
			b->dsp->r20 = (en >> 2) & 1;
			/*
			 * `TxClockSync` writes `3 * sre.pll_acc` into
			 * `pps.cfg.step`, which is zero on a fresh graph -- so
			 * with the baud field left alone the call is INVISIBLE
			 * and a wrong gate on it cannot be caught.  A non-zero
			 * value here makes the write show up in the transmit
			 * samples of the block after it.
			 */
			a->dsp->sre.pll_acc = 3;
			b->dsp->sre.pll_acc = 3;

			load_buffers(12000, 12);
			tcount_a = tcount_b = 12;
			rcount_a = rcount_b = BLOCK;

			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			dec_idx = 0;
			ref_v22_data(a, tsym_a, out_a, in_a, rsym_a,
				     &tcount_a, &rcount_a);
			dec_idx = 0;
			v22_data(b, tsym_b, out_b, in_b, rsym_b,
				 &tcount_b, &rcount_b);
			dsplib_debug_capture_on = 0;

			diff_eq_int("tx count", tcount_b, tcount_a, tag);
			diff_eq_int("rx count", rcount_b, rcount_a, tag);
			compare_buffers(tag);
			compare_graphs(b, a, tag);
			if (dbg)
				compare_transcript(tag);
			saw_debug_lines += dsplib_debug_capture_lines(1);

			if (rcount_a == 0)
				saw_rx_empty++;
			else
				saw_rx_symbols++;
			if (a->dsp->sre.active)
				saw_carrier++;
			else
				saw_carrier_lost++;
			if (a->status == V22_ST_RETRAIN_REQ)
				saw_retrain_req++;
			if (a->status == V22_ST_RETRAIN && r3cs[ci] > 14)
				saw_carrier_back++;
			if (a->dsp->descrambler_on & 1)
				saw_descramble++;
			else
				saw_no_descramble++;
			if (a->hdx->carrier_loss_blocks > 0 && a->status == 0)
				saw_loss_pending++;
			if (a->status == V22_ST_NO_CARRIER)
				saw_loss_reported++;
			tag++;
		}

		V22FP_delete(a);
		V22FP_delete(b);
	   }

	set_debug(0u);
	return diff_end();
}

/* ------------------------------------------------------------------------ */

static long rmloop_state_seen[6];
static long saw_rm_timer_expired, saw_rm_timer_running;
static long saw_rm_detect_hit, saw_rm_detect_miss;
static long saw_rm_transition, saw_rm_no_transition;
static long saw_rm_loss_expired, saw_rm_loss_running;
static long saw_rm_carrier, saw_rm_no_carrier;
static long saw_rm_txclock;
static long saw_rm_status[16];

static int
run_rmloop2(void)
{
	/*
	 * Four live sub-states and two that select nothing.  The out-of-range
	 * pair is here because the default arm still performs the two stores
	 * at the top of the function, which a `switch` with no default would
	 * also do and a mis-transcribed one might not.
	 */
	static const short states[6] = { 0, 1, 2, 3, 4, -1 };
	/*
	 * ReadGTimer adds 20 before it returns, so these straddle both
	 * boundaries: 1300 ms for sub-state 1 and 5000 ms for sub-state 2.
	 */
	/*
	 * The negative is the same probe as the quiet sweep's: both
	 * `ReadGTimer` comparisons are `jbe`, and only a negative return
	 * separates that from a signed one.
	 */
	static const int gtimers[] = { -1000, 0, 1260, 1280, 1281, 4960,
				       4980, 4981 };
	/* Both the 154-count boundary and the 39 ms one. */
	/*
	 * The negative is the sixteen-bit version of the same probe: every
	 * read of `hdx->r08` in this function is `movzwl` feeding an
	 * unsigned compare, and -40 is the only entry here whose value
	 * after the step has bit 15 set.
	 */
	static const short r08s[] = { -40, 0, 19, 20, 39, 154, 155, 200 };
	static const unsigned short p_ones12[1] = { 3 };
	static const unsigned short p_ones24[1] = { 15 };
	static const unsigned short p_zero[1] = { 0 };
	long tag = 0;
	int si, rate, gi, ri, pi, loud, en;

	diff_begin("v22_ans_rmloop2");
	rng_seed(0x22ba5e01UL);

	for (si = 0; si < 6; si++)
	 for (rate = 0; rate <= 1; rate++)
	  for (loud = 0; loud <= 1; loud++)
	   for (pi = 0; pi < 3; pi++)
	    for (gi = 0; gi < (int)(sizeof(gtimers) / sizeof(gtimers[0]));
		 gi++)
	     for (ri = 0; ri < (int)(sizeof(r08s) / sizeof(r08s[0])); ri++) {
			struct v22fp_cfg cfg = base_cfg(0, rate, 0, 1);
			struct v22fp *a = V22FP_create(0, &cfg);
			struct v22fp *b = V22FP_create(0, &cfg);
			unsigned short tcount_a, tcount_b;
			unsigned short rcount_a, rcount_b;
			short pre_r0c = states[si];

			en = (gi + ri + pi) & 7;

			if (pi == 0)
				set_pattern(p_ones12, 1);
			else if (pi == 1)
				set_pattern(p_ones24, 1);
			else
				set_pattern(p_zero, 1);

			a->hdx->connect_substate = pre_r0c;
			b->hdx->connect_substate = pre_r0c;
			a->hdx->gtimer = gtimers[gi];
			b->hdx->gtimer = gtimers[gi];
			a->hdx->r08 = r08s[ri];
			b->hdx->r08 = r08s[ri];
			a->hdx->trained = 0;
			b->hdx->trained = 0;
			/*
			 * Non-zero so the loud pass reaches the equaliser;
			 * the quiet pass takes the disconnect return either
			 * way, because silence fails the level check when the
			 * check runs and produces no symbols when it does not.
			 */
			a->hdx->protocol = loud ? 1 : 0;
			b->hdx->protocol = loud ? 1 : 0;
			a->dsp->fse.decision = test_decision;
			b->dsp->fse.decision = test_decision;
			a->dsp->r20 = (en >> 2) & 1;
			b->dsp->r20 = (en >> 2) & 1;
			/*
			 * `TxClockSync` writes `3 * sre.pll_acc` into
			 * `pps.cfg.step`, which is zero on a fresh graph -- so
			 * with the baud field left alone the call is INVISIBLE
			 * and a wrong gate on it cannot be caught.  A non-zero
			 * value here makes the write show up in the transmit
			 * samples of the block after it.
			 */
			a->dsp->sre.pll_acc = 3;
			b->dsp->sre.pll_acc = 3;
			a->status = 0;
			b->status = 0;

			load_buffers(loud ? 12000 : 0, 12);
			tcount_a = tcount_b = 12;
			rcount_a = rcount_b = BLOCK;

			dec_idx = 0;
			ref_v22_ans_rmloop2(a, tsym_a, out_a, in_a, rsym_a,
					    &tcount_a, &rcount_a);
			dec_idx = 0;
			v22_ans_rmloop2(b, tsym_b, out_b, in_b, rsym_b,
					&tcount_b, &rcount_b);

			diff_eq_int("tx count", tcount_b, tcount_a, tag);
			diff_eq_int("rx count", rcount_b, rcount_a, tag);
			compare_buffers(tag);
			compare_graphs(b, a, tag);

			rmloop_state_seen[si]++;
			saw_rm_status[a->status & 0x0f]++;
			if (a->status == V22_ST_07)
				saw_rm_timer_expired++;
			else
				saw_rm_timer_running++;
			if (a->status == V22_ST_05)
				saw_rm_detect_miss++;
			if (a->status == V22_ST_08)
				saw_rm_loss_expired++;
			if (pre_r0c == V22_RMLOOP2_DETECT
			    && a->hdx->connect_substate == V22_RMLOOP2_ANSWER)
				saw_rm_transition++;
			if (pre_r0c == V22_RMLOOP2_DETECT
			    && a->hdx->connect_substate == V22_RMLOOP2_DETECT)
				saw_rm_no_transition++;
			if (pre_r0c == V22_RMLOOP2_DETECT && a->hdx->trained)
				saw_rm_detect_hit++;
			if (pre_r0c == V22_RMLOOP2_LOOP) {
				if (a->dsp->sre.active)
					saw_rm_carrier++;
				else
					saw_rm_no_carrier++;
				if (a->hdx->r08 <= V22_RMLOOP2_LOSS_MAX)
					saw_rm_loss_running++;
				if ((a->dsp->r20 & 1)
				    && a->status != V22_ST_08)
					saw_rm_txclock++;
			}
			tag++;
			V22FP_delete(a);
			V22FP_delete(b);
	     }

	return diff_end();
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= run_data_quiet();
	rc |= run_data_loud();
	rc |= run_rmloop2();

	diff_begin("v22ans coverage guards");

	/* v22_data's outer split. */
	diff_eq_int("params bit 11 arm taken (%ld)", saw_bit11 > 0, 1, 0);
	diff_eq_int("params bit 11 arm not taken (%ld)", saw_no_bit11 > 0, 1,
		    0);
	diff_eq_int("bit 11 timer expired (%ld)", saw_bit11_timeout > 0, 1, 0);
	diff_eq_int("bit 11 timer still running (%ld)", saw_bit11_running > 0,
		    1, 0);

	/* The three status-flag gates. */
	diff_eq_int("scrambler run (%ld)", saw_scramble > 0, 1, 0);
	diff_eq_int("scrambler skipped (%ld)", saw_no_scramble > 0, 1, 0);
	diff_eq_int("descrambler run (%ld)", saw_descramble > 0, 1, 0);
	diff_eq_int("descrambler skipped (%ld)", saw_no_descramble > 0, 1, 0);
	diff_eq_int("TxClockSync run (%ld)", saw_txclock > 0, 1, 0);
	diff_eq_int("TxClockSync skipped (%ld)", saw_no_txclock > 0, 1, 0);

	/* The demodulator produced symbols, and did not. */
	diff_eq_int("rx symbols produced (%ld)", saw_rx_symbols > 0, 1, 0);
	diff_eq_int("rx block empty (%ld)", saw_rx_empty > 0, 1, 0);

	/* The four things v22_data can report. */
	diff_eq_int("quiet block, status 0 (%ld)", saw_quiet_block > 0, 1, 0);
	diff_eq_int("retrain request detected (%ld)", saw_retrain_req > 0, 1,
		    0);
	diff_eq_int("retrain on signal quality (%ld)", saw_quality_retrain > 0,
		    1, 0);
	diff_eq_int("retrain on carrier back (%ld)", saw_carrier_back > 0, 1,
		    0);
	diff_eq_int("NO_CARRIER reported (%ld)", saw_no_carrier > 0, 1, 0);

	/* Both sides of the two comparisons that decide those. */
	diff_eq_int("quality above retrain level (%ld)", saw_quality_high > 0,
		    1, 0);
	diff_eq_int("quality at or below it (%ld)", saw_quality_low > 0, 1, 0);
	diff_eq_int("carrier present (%ld)", saw_carrier > 0, 1, 0);
	diff_eq_int("carrier absent (%ld)", saw_carrier_lost > 0, 1, 0);
	diff_eq_int("carrier loss reported (%ld)", saw_loss_reported > 0, 1, 0);
	diff_eq_int("carrier loss still pending (%ld)", saw_loss_pending > 0, 1,
		    0);

	/*
	 * The format strings.  At the shipping level of zero every one of them
	 * is dead on both sides, so a wrong argument list would never be seen
	 * -- this counts what the BLOB actually printed.
	 */
	diff_eq_int("debug transcript non-empty (%ld)", saw_debug_lines > 0, 1,
		    0);

	/* v22_ans_rmloop2: every sub-state, including the two that do nothing. */
	diff_eq_int("rmloop2 sub-state 0 driven (%ld)", rmloop_state_seen[0] > 0,
		    1, 0);
	diff_eq_int("rmloop2 sub-state 1 driven (%ld)", rmloop_state_seen[1] > 0,
		    1, 0);
	diff_eq_int("rmloop2 sub-state 2 driven (%ld)", rmloop_state_seen[2] > 0,
		    1, 0);
	diff_eq_int("rmloop2 sub-state 3 driven (%ld)", rmloop_state_seen[3] > 0,
		    1, 0);
	diff_eq_int("rmloop2 sub-state 4 driven (%ld)", rmloop_state_seen[4] > 0,
		    1, 0);
	diff_eq_int("rmloop2 sub-state -1 driven (%ld)",
		    rmloop_state_seen[5] > 0, 1, 0);

	/*
	 * F8528's shape: sub-states 1 and 2 share a tail and differ only in
	 * the limit, so both the expired and the running side of that timer
	 * have to be seen or a single-limit reconstruction would agree.
	 */
	diff_eq_int("rmloop2 timer expired (%ld)", saw_rm_timer_expired > 0, 1,
		    0);
	diff_eq_int("rmloop2 timer running (%ld)", saw_rm_timer_running > 0, 1,
		    0);
	diff_eq_int("rmloop2 detect count crossed (%ld)", saw_rm_transition > 0,
		    1, 0);
	diff_eq_int("rmloop2 detect count below (%ld)",
		    saw_rm_no_transition > 0, 1, 0);
	diff_eq_int("rmloop2 froze the equaliser (%ld)", saw_rm_detect_hit > 0,
		    1, 0);
	diff_eq_int("rmloop2 ones detector stopped (%ld)",
		    saw_rm_detect_miss > 0, 1, 0);
	diff_eq_int("rmloop2 carrier present in loop (%ld)", saw_rm_carrier > 0,
		    1, 0);
	diff_eq_int("rmloop2 carrier absent in loop (%ld)",
		    saw_rm_no_carrier > 0, 1, 0);
	diff_eq_int("rmloop2 loss timer expired (%ld)", saw_rm_loss_expired > 0,
		    1, 0);
	diff_eq_int("rmloop2 loss timer running (%ld)", saw_rm_loss_running > 0,
		    1, 0);
	diff_eq_int("rmloop2 TxClockSync run (%ld)", saw_rm_txclock > 0, 1, 0);
	diff_eq_int("rmloop2 default status stood (%ld)",
		    saw_rm_status[V22_ST_ANS_RMLOOP2] > 0, 1, 0);

	rc |= diff_end();
	return rc;
}
