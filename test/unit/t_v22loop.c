/*
 * t_v22loop.c -- differential test of v22_local_loop.
 *
 * TWO GRAPHS, six buffers and a transcript, which is t_v22ans.c's fixture and
 * t_v22rate.c's before that: two `struct v22fp` graphs built by `V22FP_create`
 * -- ours on both sides, because t_v22fpcreate has established that ours and
 * the blob's agree byte for byte -- the reference driven through one and the
 * reconstruction through the other, then the two graphs compared entire.  That
 * is the three structs with their pointers blanked, the twenty-eight heap
 * regions behind them, the four caller buffers, both counts, and the debug
 * transcript.
 *
 * THIS FILE ADDS ONE REGION t_v22ans.c DOES NOT COMPARE: `hdx->mtd_s1` ITSELF.
 * Sub-state 3 runs that detector on every block and its verdict decides which
 * of the two accumulators moves, so its internal state is an output of this
 * function in a way it is not an output of `v22_data`.  `blank_hdx` nulls the
 * pointer, so without the extra `diff_eq_obj` below the running energy
 * estimates would be compared by nobody.
 *
 * A SINGLE CALL EXERCISES ONE ARM.  `v22_local_loop` dispatches fourteen ways
 * and its sub-state 3 alone has eight conditionals, four of which are only
 * reachable from state the CALLEES move.  Five fixtures open those up, and
 * three of the five are this file's rather than inherited:
 *
 *   - A TEST `fse.decision`, from t_v22ans.c.  `V22_FSE_receive` writes its
 *     symbol buffer straight from `state->decision(...)`, and the field is a
 *     function pointer in a graph THIS TEST OWNS, so installing one of our own
 *     on both sides makes the demodulated symbol stream an input.  That is
 *     what reaches both arms of `Detect_1s` -- and, through it, both the
 *     descrambler and the `hdx->ones_detect_ms` accumulator, which no amount of noise
 *     into a cold equaliser would drive to 230 ms.
 *
 *   - `mtd_s1->cfg.min_level` RAISED TO 0x7fff.  `hdx->r08` moves one of three
 *     ways per block depending on `FPM_MTD_detect`'s verdict, and a broadband
 *     noise block always yields FPM_MTD_ABSENT: the "detected" arm and its
 *     hysteresis are simply unreachable while the detector is left as
 *     `MTDs1_CFG` built it.  Raising the level gate forces FPM_MTD_NOSIGNAL,
 *     which the object treats as "not ABSENT" exactly as it treats a real
 *     detection -- the branch is `test %esi,%esi`, not a compare against 1.
 *     Both graphs get the same poke, so the comparison is unaffected.
 *
 *   - `agc.cfg.acquire_level` AND `squelch_level` RAISED THE SAME WAY.
 *     `SignalDetect` returns `dsp->agc.signal`, which `FPM_AGC_agc` sets from
 *     how many measurement blocks cleared the gate, and a loud block always
 *     clears it.  Without this the `SignalDetect(fp) == 1` gate is taken on
 *     every call that produced symbols at all and a reconstruction that
 *     dropped it entirely would pass.  Note it has to be BOTH levels: the
 *     first block of a fresh graph faces `acquire_level` and every later one
 *     faces `squelch_level`.
 *
 *   - `hdx->r08`, `hdx->ones_detect_ms`, `hdx->gtimer` and `hdx->node_deadline` POKED DIRECTLY.
 *     They are the handler's own state and every comparison against them is
 *     UNSIGNED in the object, so the sweeps carry values a working modem never
 *     reaches -- a negative `r08` whose value after the +20 step has bit 15
 *     set, and a negative `gtimer`.  Without those a signed reading of any of
 *     the five comparisons passes.
 *
 *   - `dsplibs_debug_level` AT TWO, with the harness's capture on.
 *     `v22_local_loop` itself prints NOTHING -- twenty-six relocations and the
 *     only non-call is its own jump table -- but sub-states 8..13 hand the
 *     whole call to `connect_2400` and `connect_1200`, which between them
 *     carry six format strings.  So the transcript check is here to police the
 *     delegation, not the handler.
 *
 * EVERY NODE DEADLINE IS PINNED FROM BOTH SIDES.  `ReadGTimer` adds 20 before
 * it returns, so a single probe cannot tell C from C-1: each timer sweep here
 * starts the clock at C-20 and at C-19, which makes the two returns exactly C
 * and C+1 and separates `>` from `>=`.
 *
 * A FRESH GRAPH PER CALL.  t_v22ans.c's header records why and it is not
 * re-derived here: driving many consecutive blocks through one graph makes the
 * TRANSMIT SAMPLES disagree between two runs of the BLOB'S OWN handler,
 * because something under `ModDataV22` reads memory `V22FP_create` never
 * writes.  Every call in these sweeps is a single arm anyway.
 *
 * WHAT THE GUARDS AT THE BOTTOM ARE FOR.  A handler whose other arm was never
 * taken agrees with a stub, and this function has two pairs of arms that share
 * a tail in the object -- sub-state 0's second successor jumps backwards into
 * the middle of sub-state 1's timeout arm, and sub-state 3's two threshold
 * arms differ in one immediate.  Reading either fall-through alone gives a
 * complete-looking arm that writes the wrong sub-state, so both members of
 * both pairs are counted.
 *
 * THIRTY-THREE DELIBERATE DEFECTS WERE INJECTED and thirty-two were caught.
 * The one that is not is KNOWN-EQUIVALENT and not worth chasing: spelling the
 * two accumulator steps `(short)hdx->rNN + ...` instead of
 * `(unsigned short)hdx->rNN + ...`.  Both sums are truncated back to sixteen
 * bits by the store, and the two extensions agree over every bit that
 * survives that -- which is finding F614's dead extension exactly, and is why
 * the source keeps the object's `movzwl` spelling on the ground that it is the
 * object's rather than on the ground that anything can see it.
 *
 * TWO of the thirty-two needed a probe the first sweep did not have, and both
 * are recorded beside the array that gained it: a deadline WIDER than sixteen
 * bits, and a NEGATIVE `hdx->ones_detect_ms`.  Neither gap was visible from the coverage
 * guards -- every guard was green while both mutants lived.
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
#include "dsplib/v22conn.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22loop.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22rate.h"
#include "dsplib/v22status.h"
#include "dsplib/v22txtab.h"

extern void ref_v22_local_loop(struct v22fp *fp, unsigned short *txsym,
			       short *txout, short *rxin,
			       unsigned short *rxsym, unsigned short *txcount,
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
	c.pps.imap = NULL;	/* SetTxRate installs it; see map_id  */
	c.pps.qmap = NULL;	/* likewise                           */
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
	c.fse.decision = NULL;	/* SetRxRate installs it; see dec_id  */
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
 * sub-states 0 and 1 call `SetTxRate` and `SetRxRate`, and `blank_dsp` blanks
 * the three fields they write.
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
	struct fpm_mtd ma = blank_mtd(mine->hdx->mtd_s1);
	struct fpm_mtd mb = blank_mtd(theirs->hdx->mtd_s1);
	int n, m, i;

	diff_eq_obj("object", struct v22fp, &oa, &ob, tag);
	diff_eq_obj("hdx", struct v22fp_hdx, &ha, &hb, tag);
	diff_eq_obj("dsp", struct v22fp_dsp, &da, &db, tag);
	diff_eq_obj("tone", struct fpm_tone, &ta, &tb, tag);
	diff_eq_obj("mtd_s1", struct fpm_mtd, &ma, &mb, tag);

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
 * The test slicer, t_v22ans.c's.  It writes the two outputs
 * `V22_FSE_receive` reads back with a constant each, so the equaliser update
 * downstream is deterministic, and returns the symbol the test wants at that
 * position.  Installed on BOTH graphs; the index is reset before each call.
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
base_cfg(int mode, int rate, int f0c, int f18)
{
	struct v22fp_cfg c;

	/* What `v22_create` passes, with the four dimensions left open. */
	c.mode = mode;
	c.rate = rate;
	c.f08 = 60000;
	c.f0c = f0c;
	c.f10 = 700;
	c.f14 = 0;
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
load_buffers(int amp)
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

/* The level gate both fixtures raise; 0x7fff is above any block this test
 * feeds, so the gate rejects everything and the verdict is forced. */
#define GATE_HIGH	0x7fff

/* ------------------------------------------------------------------------ */

/* Coverage, read off the REFERENCE side so the counts describe the blob. */
static long node_seen[16];
static long saw_node0_to_1, saw_node0_to_3;
static long saw_node1_expired, saw_node1_running;
static long saw_node2_quiet, saw_node2_signal;
static long saw_rx_symbols, saw_rx_empty;
static long saw_signal, saw_no_signal;
static long saw_r08_grew, saw_r08_reset, saw_r08_held;
static long saw_ones, saw_no_ones;
static long saw_r0a_grew;
static long saw_scramble_1200, saw_scramble_2400;
static long saw_pattern_forced;
static long saw_to_2400, saw_to_1200;
static long saw_node3_timeout, saw_node3_running;
static long saw_delegate_2400, saw_delegate_1200;
static long saw_default_arm;
static long saw_debug_lines;

/*
 * One call: build two graphs, apply the pokes to both, drive the reference
 * through one and the reconstruction through the other, compare everything.
 * Returns the reference graph's post-call state through `a` for the caller's
 * coverage counting -- which is why the graphs are deleted by the caller.
 */
struct poke {
	int rate;		/* struct v22fp_cfg::rate               */
	int f0c;		/* -> params.flags bit 10               */
	int amp;		/* input block amplitude                */
	short connect_substate;	/* the sub-state to drive       */
	short protocol;		/* non-zero skips the disconnect return */
	int gtimer;
	int node_deadline;
	short r08;
	short ones_detect_ms;
	int mtd;		/* 0 as built, 1 force ABSENT, 2 NOSIGNAL */
	int agc_gate;		/* force SignalDetect() == 0            */
	int desc_fix;		/* make the descrambler emit all 3s     */
	int dbg;
};

static void
apply(struct v22fp *p, const struct poke *k)
{
	p->hdx->connect_substate = k->connect_substate;
	p->hdx->protocol = k->protocol;
	p->hdx->gtimer = k->gtimer;
	p->hdx->node_deadline = k->node_deadline;
	p->hdx->r08 = k->r08;
	p->hdx->ones_detect_ms = k->ones_detect_ms;
	p->dsp->fse.decision = test_decision;
	/*
	 * `TxClockSync` inside `connect_1200` writes `3 * sre.pll_acc` into
	 * `pps.cfg.step`, which is zero on a fresh graph -- so with the baud
	 * field left alone the call is INVISIBLE.  A non-zero value here makes
	 * the write show up.
	 */
	p->dsp->sre.pll_acc = 3;
	/*
	 * The detector's verdict, forced.  `MTDs1_CFG` as built never returns
	 * FPM_MTD_ABSENT on any block this test can feed -- measured, not
	 * assumed: with the config left alone the "r08 stepped by a block"
	 * guard below is zero over the whole sweep.  A zero `ratio` puts the
	 * threshold at zero, which no non-silent block's out-of-band energy
	 * can be at or below, so the verdict is ABSENT; a high `min_level`
	 * takes the earlier return and gives NOSIGNAL, which the object treats
	 * exactly as it treats a real detection (the branch is
	 * `test %esi,%esi`, not a compare against FPM_MTD_PRESENT).
	 */
	if (k->mtd == 1)
		p->hdx->mtd_s1->cfg.ratio = 0;
	else if (k->mtd == 2)
		p->hdx->mtd_s1->cfg.min_level = GATE_HIGH;
	if (k->agc_gate) {
		p->dsp->agc.cfg.acquire_level = GATE_HIGH;
		p->dsp->agc.cfg.squelch_level = GATE_HIGH;
	}
	/*
	 * THE DESCRAMBLED-ONES FIXTURE, and it is the only way this test can
	 * reach the `hdx->ones_detect_ms` accumulator at all.
	 *
	 * The object runs `Detect_1s` TWICE on the same buffer -- once on the
	 * raw symbols, and, only if that found nothing, once more after
	 * `DescrambleDataV22` has rewritten them in place.  So the two calls
	 * have to DISAGREE, and no symbol pattern alone can make them: with
	 * the descrambler as `V22FP_create` built it, the register starts at
	 * zero and both feedback terms are zero for the first seven symbols,
	 * which makes the descrambled block equal to the raw one over most of
	 * a twelve-symbol call.
	 *
	 * `FPM_SDM_descrambler` writes `(reg >> shift1) ^ in ^ (reg >> shift2)`
	 * and then updates `reg = ((reg << nbits) & notmask) | in`.  With
	 * `nbits` zero, `notmask` all ones and the input word zero, that update
	 * is `reg = reg`: the register is FROZEN.  Freezing it at 3 with
	 * `shift1` 0 and `shift2` past the top of the word makes every output
	 * word 3 -- and 3 is exactly the symbol `Detect_1s` correlates against
	 * at V22_LOOP_DET_BPS.  So a block of zero symbols reads as "not ones"
	 * raw and as "ones" descrambled, which is the disagreement the arm
	 * needs.
	 *
	 * It is a poke of real fields, applied identically to both graphs, and
	 * the descrambler it drives is each side's own.
	 */
	if (k->desc_fix) {
		p->dsp->sdm2.cfg.nbits = 0;
		p->dsp->sdm2.notmask = -1;
		p->dsp->sdm2.shift1 = 0;
		p->dsp->sdm2.shift2 = 16;
		p->dsp->sdm2.reg = 3;
	}
}

static unsigned short g_tcount_a, g_tcount_b, g_rcount_a, g_rcount_b;
/*
 * What sub-state 3's two gates cannot be read off the arguments after the
 * call: `RxClampV22` overwrites `*rxcount` on the way out, so the demodulated
 * symbol count is taken from the equaliser's own per-call counter instead, and
 * "did the descrambler run" is taken from the descrambler's register.
 */
static short g_n_out;
static struct fpm_sdm g_pre_sdm2, g_post_sdm2;

static struct v22fp *
drive(const struct poke *k, struct v22fp **other, long tag)
{
	struct v22fp_cfg cfg = base_cfg(0, k->rate, k->f0c, 1);
	struct v22fp *a = V22FP_create(0, &cfg);
	struct v22fp *b = V22FP_create(0, &cfg);

	apply(a, k);
	apply(b, k);
	set_debug(k->dbg ? 2u : 0u);

	load_buffers(k->amp);
	g_tcount_a = g_tcount_b = 12;
	g_rcount_a = g_rcount_b = BLOCK;

	g_pre_sdm2 = a->dsp->sdm2;

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;
	dec_idx = 0;
	ref_v22_local_loop(a, tsym_a, out_a, in_a, rsym_a,
			   &g_tcount_a, &g_rcount_a);
	dec_idx = 0;
	v22_local_loop(b, tsym_b, out_b, in_b, rsym_b,
		       &g_tcount_b, &g_rcount_b);
	dsplib_debug_capture_on = 0;

	g_post_sdm2 = a->dsp->sdm2;
	g_n_out = a->dsp->fse.n_out;

	diff_eq_int("tx count", g_tcount_b, g_tcount_a, tag);
	diff_eq_int("rx count", g_rcount_b, g_rcount_a, tag);
	compare_buffers(tag);
	compare_graphs(b, a, tag);
	if (k->dbg)
		compare_transcript(tag);
	saw_debug_lines += dsplib_debug_capture_lines(1);

	set_debug(0u);
	*other = b;
	return a;
}

static struct poke
base_poke(void)
{
	struct poke k;

	memset(&k, 0, sizeof(k));
	k.rate = 0;
	k.f0c = 0;
	k.amp = 0;
	k.connect_substate = V22_LOOP_NODE_0;
	k.protocol = 0;
	k.gtimer = 0;
	k.node_deadline = 60000;
	k.r08 = 0;
	k.ones_detect_ms = 0;
	return k;
}

/* ------------------------------------------------------------------------ */

/*
 * Sub-state 0.  The successor is chosen by ONE bit of the status report --
 * `st.flags2` bit 0, i.e. `params.flags` bit 10, i.e. `cfg.f0c` bit 0 -- and
 * the two arms share a tail with sub-state 1's timeout, so both are driven.
 * The three counters are poked non-zero so that the zeroing at the top is
 * visible rather than a write of 0 over 0.
 */
static int
run_node0(void)
{
	long tag = 0;
	int f0c, rate, amp;

	diff_begin("v22_local_loop, sub-state 0");
	rng_seed(0x10c0100dUL);

	for (f0c = 0; f0c <= 1; f0c++)
	 for (rate = 0; rate <= 1; rate++)
	  for (amp = 0; amp <= 1; amp++) {
		struct v22fp *a, *b;
		struct poke k = base_poke();

		k.connect_substate = V22_LOOP_NODE_0;
		k.f0c = f0c;
		k.rate = rate;
		k.amp = amp ? 12000 : 0;
		/* Non-zero so the zeroing at the top of the arm is visible. */
		k.gtimer = 1234;
		k.r08 = 111;
		k.ones_detect_ms = 222;

		a = drive(&k, &b, tag);

		node_seen[V22_LOOP_NODE_0]++;
		if (a->hdx->connect_substate == V22_LOOP_NODE_1)
			saw_node0_to_1++;
		else if (a->hdx->connect_substate == V22_LOOP_NODE_3)
			saw_node0_to_3++;
		diff_eq_int("f0c bit 0 selects the successor",
			    a->hdx->connect_substate == V22_LOOP_NODE_1,
			    (a->params.flags & V22_PARAMS_BIT10) != 0, tag);

		tag++;
		V22FP_delete(a);
		V22FP_delete(b);
	  }

	return diff_end();
}

/*
 * Sub-state 1.  The tone runs until `ReadGTimer` exceeds V22_LOOP_TONE_MS, and
 * the clock advances 20 ms per call -- so the sweep starts it at C-20 and C-19
 * as well as far either side, which makes the two returns exactly C and C+1.
 * A negative start is here because the comparison is `jbe`.
 */
static int
run_node1(void)
{
	static const int gtimers[] = { -1000, 0, 1000, 3280, 3281, 5000 };
	long tag = 0;
	int gi, rate, amp;

	diff_begin("v22_local_loop, sub-state 1");
	rng_seed(0x10c01001UL);

	for (gi = 0; gi < (int)(sizeof(gtimers) / sizeof(gtimers[0])); gi++)
	 for (rate = 0; rate <= 1; rate++)
	  for (amp = 0; amp <= 1; amp++) {
		struct v22fp *a, *b;
		struct poke k = base_poke();

		k.connect_substate = V22_LOOP_NODE_1;
		k.rate = rate;
		k.amp = amp ? 12000 : 0;
		k.gtimer = gtimers[gi];

		a = drive(&k, &b, tag);

		node_seen[V22_LOOP_NODE_1]++;
		if (a->hdx->connect_substate == V22_LOOP_NODE_2)
			saw_node1_expired++;
		else
			saw_node1_running++;
		/* The whole point of the arm: 160 tone samples. */
		diff_eq_int("tone block length", g_tcount_a, V22_TX_BLOCK,
			    tag);

		tag++;
		V22FP_delete(a);
		V22FP_delete(b);
	  }

	return diff_end();
}

/*
 * Sub-state 2.  It waits for FPM_TONE_NOSIGNAL, which fpm_tone.h records as
 * the verdict for a block below the detector's minimum level -- so silence
 * advances the machine and a loud block does not.  Both are driven.
 */
static int
run_node2(void)
{
	long tag = 0;
	int rate, amp, gi;

	diff_begin("v22_local_loop, sub-state 2");
	rng_seed(0x10c01002UL);

	for (rate = 0; rate <= 1; rate++)
	 for (amp = 0; amp <= 1; amp++)
	  for (gi = 0; gi <= 1; gi++) {
		struct v22fp *a, *b;
		struct poke k = base_poke();

		k.connect_substate = V22_LOOP_NODE_2;
		k.rate = rate;
		k.amp = amp ? 12000 : 0;
		k.gtimer = gi ? 5000 : 0;

		a = drive(&k, &b, tag);

		node_seen[V22_LOOP_NODE_2]++;
		if (a->hdx->connect_substate == V22_LOOP_NODE_3)
			saw_node2_quiet++;
		else
			saw_node2_signal++;

		tag++;
		V22FP_delete(a);
		V22FP_delete(b);
	  }

	return diff_end();
}

/*
 * Sub-state 3, the loop itself and eight conditionals.  Every dimension here
 * exists to open one of them; see the file header for the three fixtures that
 * are not just a poked field.
 */
static int
run_node3(void)
{
	/*
	 * -40 is the entry whose value after the +20 step has bit 15 set --
	 * the only input that separates the object's unsigned `ja` from a
	 * signed reading.  0x3b/0x3c straddle the hysteresis and 0x61/0x62 the
	 * transition into V22_NODE_2400A.
	 */
	static const short r08s[] = { -40, 0, 0x3b, 0x3c, 0x61, 0x62 };
	/*
	 * Straddling the transition into V22_NODE_1200_12.  The negative is
	 * the unsigned probe for THIS comparison, and it is not redundant with
	 * `r08`'s: a sweep whose `ones_detect_ms` values were all non-negative let a
	 * signed reading of the `hdx->ones_detect_ms` test survive while catching every
	 * other one.
	 */
	static const short r0as[] = { -40, 0, 0xe6, 0xe7 };
	/*
	 * The node deadline, as (clock start, `hdx->node_deadline`) pairs.
	 *
	 * It is pinned FROM BOTH SIDES: the clock advances 20 ms per call, so
	 * with the deadline at 100 the two starts 80 and 81 make `ReadGTimer`
	 * return exactly 100 and 101 and separate `>` from `>=`.
	 *
	 * The negative start is the unsigned probe -- `ReadGTimer` returns
	 * -980, which is above any deadline read unsigned and below every one
	 * read signed.
	 *
	 * The last pair is the WIDTH probe, and it is here because a mutant
	 * that narrowed `hdx->node_deadline` to sixteen bits survived a sweep that held
	 * the deadline at 100: 0x10064 and 100 agree in their low half, so a
	 * clock of 220 is under the real deadline and over the truncated one.
	 */
	static const int gtimers[] = { -1000, 80, 81, 200 };
	static const int r04s[] = { 100, 100, 100, 0x10064 };
	static const unsigned short p_ones12[1] = { 3 };
	static const unsigned short p_ones24[1] = { 15 };
	static const unsigned short p_zero[1] = { 0 };
	long tag = 0;
	int rate, amp, mi, ag, df, pi, ri, ai, gi;

	diff_begin("v22_local_loop, sub-state 3");
	rng_seed(0x10c01003UL);

	for (rate = 0; rate <= 1; rate++)
	 for (amp = 0; amp <= 1; amp++)
	  for (mi = 0; mi < 3; mi++)
	   for (ag = 0; ag <= 1; ag++)
	    for (df = 0; df <= 1; df++)
	     for (pi = 0; pi < 3; pi++)
	      for (ri = 0; ri < (int)(sizeof(r08s) / sizeof(r08s[0])); ri++)
	       for (ai = 0; ai < (int)(sizeof(r0as) / sizeof(r0as[0])); ai++)
	        for (gi = 0; gi < (int)(sizeof(gtimers) / sizeof(gtimers[0]));
		     gi++) {
		struct v22fp *a, *b;
		struct poke k = base_poke();
		short pre_r08 = r08s[ri];
		short pre_r0a = r0as[ai];
		int signalled, descrambled;

		if (pi == 0)
			set_pattern(p_ones12, 1);
		else if (pi == 1)
			set_pattern(p_ones24, 1);
		else
			set_pattern(p_zero, 1);

		k.connect_substate = V22_LOOP_NODE_3;
		k.rate = rate;
		k.amp = amp ? 12000 : 0;
		/* Non-zero skips DemodDataV22's disconnect return. */
		k.protocol = amp ? 1 : 0;
		k.mtd = mi;
		k.agc_gate = ag;
		k.desc_fix = df;
		k.r08 = pre_r08;
		k.ones_detect_ms = pre_r0a;
		k.node_deadline = r04s[gi];
		k.gtimer = gtimers[gi];

		a = drive(&k, &b, tag);

		node_seen[V22_LOOP_NODE_3]++;

		if (a->params.bps2 == V22_LOOP_BPS_1200)
			saw_scramble_1200++;
		else
			saw_scramble_2400++;
		if ((unsigned short)pre_r08 > V22_LOOP_S1_MS)
			saw_pattern_forced++;

		/*
		 * `SignalDetect` reads dsp->agc.signal, which the AGC inside
		 * DemodDataV22 has just written -- so this reads back the very
		 * value the branch used.
		 */
		signalled = g_n_out != 0 && SignalDetect(a) == 1;
		if (g_n_out == 0)
			saw_rx_empty++;
		else {
			saw_rx_symbols++;
			if (SignalDetect(a) == 1)
				saw_signal++;
			else
				saw_no_signal++;
		}

		/*
		 * The three ways `hdx->r08` can move, distinguished by the
		 * value it holds afterwards.  Counted only where the tail did
		 * NOT transition, because the transition zeroes it.
		 */
		if (a->hdx->connect_substate == V22_LOOP_NODE_3 && signalled) {
			if (a->hdx->r08
			    == (short)((unsigned short)pre_r08
				       + V22_LOOP_BLOCK_MS))
				saw_r08_grew++;
			else if (a->hdx->r08 == 0 && pre_r08 != 0)
				saw_r08_reset++;
			else if (a->hdx->r08 == pre_r08
				 && (unsigned short)pre_r08
				    > V22_LOOP_S1_HOLD_MS)
				saw_r08_held++;
		}

		/*
		 * The ones detector.  "The descrambler ran" is read off the
		 * descrambler's own register, except under the fixture that
		 * deliberately freezes it -- there the accumulator moving is
		 * the evidence instead.
		 */
		descrambled = memcmp(&g_pre_sdm2, &g_post_sdm2,
				     sizeof(g_pre_sdm2)) != 0
			      || a->hdx->ones_detect_ms != pre_r0a;
		if (a->hdx->connect_substate == V22_LOOP_NODE_3 && signalled) {
			if (descrambled)
				saw_no_ones++;
			else
				saw_ones++;
		}
		if (a->hdx->connect_substate == V22_LOOP_NODE_3
		    && a->hdx->ones_detect_ms != pre_r0a)
			saw_r0a_grew++;

		if (a->hdx->connect_substate == V22_NODE_2400A)
			saw_to_2400++;
		else if (a->hdx->connect_substate == V22_NODE_1200_12)
			saw_to_1200++;
		else if (a->status == V22_STATUS_16)
			saw_node3_timeout++;
		else
			saw_node3_running++;

		tag++;
		V22FP_delete(a);
		V22FP_delete(b);
	        }

	return diff_end();
}

/*
 * The six delegating sub-states and the five that do nothing.  The
 * out-of-range values are here because the default arm still performs the two
 * stores at the top, which a mis-transcribed `switch` might not; -1 in
 * particular is the one that separates the object's UNSIGNED range check from
 * a signed one, because a signed check would send it to case 0.
 */
static int
run_dispatch(void)
{
	static const short states[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
					-1, 14, 100, -32768 };
	static const unsigned short p_ones12[1] = { 3 };
	long tag = 0;
	int si, rate, amp, dbg;

	diff_begin("v22_local_loop, dispatch");
	rng_seed(0x10c01004UL);
	set_pattern(p_ones12, 1);

	for (si = 0; si < (int)(sizeof(states) / sizeof(states[0])); si++)
	 for (rate = 0; rate <= 1; rate++)
	  for (amp = 0; amp <= 1; amp++)
	   for (dbg = 0; dbg <= 1; dbg++) {
		struct v22fp *a, *b;
		struct poke k = base_poke();
		short pre = states[si];

		k.connect_substate = pre;
		k.rate = rate;
		k.amp = amp ? 12000 : 0;
		k.protocol = amp ? 1 : 0;
		k.dbg = dbg;
		/* connect_* has its own deadlines; give them room to fire. */
		k.gtimer = (si & 1) ? 1000 : 0;
		k.node_deadline = 60000;

		a = drive(&k, &b, tag);

		if (pre >= 0 && pre < 16)
			node_seen[pre]++;
		if (pre >= V22_NODE_2400A && pre <= V22_NODE_2400D)
			saw_delegate_2400++;
		else if (pre == V22_NODE_1200_12 || pre == V22_NODE_1200_13)
			saw_delegate_1200++;
		else {
			saw_default_arm++;
			/*
			 * The two stores at the top stand, and nothing else
			 * happened: the default arm is a bare `break`.
			 */
			diff_eq_int("default arm left the sub-state alone",
				    a->hdx->connect_substate, pre, tag);
			diff_eq_int("default arm status", a->status,
				    V22_STATUS_01, tag);
			diff_eq_int("default arm tx count", g_tcount_a, 12,
				    tag);
			diff_eq_int("default arm rx count", g_rcount_a, BLOCK,
				    tag);
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

	rc |= run_node0();
	rc |= run_node1();
	rc |= run_node2();
	rc |= run_node3();
	rc |= run_dispatch();

	diff_begin("v22loop coverage guards");

	/* Every arm of the fourteen-entry table. */
	diff_eq_int("sub-state 0 driven (%ld)", node_seen[0] > 0, 1, 0);
	diff_eq_int("sub-state 1 driven (%ld)", node_seen[1] > 0, 1, 0);
	diff_eq_int("sub-state 2 driven (%ld)", node_seen[2] > 0, 1, 0);
	diff_eq_int("sub-state 3 driven (%ld)", node_seen[3] > 0, 1, 0);
	diff_eq_int("sub-state 8 driven (%ld)", node_seen[8] > 0, 1, 0);
	diff_eq_int("sub-state 11 driven (%ld)", node_seen[11] > 0, 1, 0);
	diff_eq_int("sub-state 12 driven (%ld)", node_seen[12] > 0, 1, 0);
	diff_eq_int("sub-state 13 driven (%ld)", node_seen[13] > 0, 1, 0);
	diff_eq_int("connect_2400 delegated to (%ld)", saw_delegate_2400 > 0,
		    1, 0);
	diff_eq_int("connect_1200 delegated to (%ld)", saw_delegate_1200 > 0,
		    1, 0);
	diff_eq_int("default arm taken (%ld)", saw_default_arm > 0, 1, 0);

	/*
	 * Sub-state 0's two successors.  They share a tail in the object --
	 * the 3 branch jumps backwards into the middle of sub-state 1's
	 * timeout arm -- so a reconstruction that let one fall into the other
	 * writes the wrong sub-state and both have to be seen.
	 */
	diff_eq_int("sub-state 0 -> 1 (%ld)", saw_node0_to_1 > 0, 1, 0);
	diff_eq_int("sub-state 0 -> 3 (%ld)", saw_node0_to_3 > 0, 1, 0);

	/* Sub-state 1's deadline, both sides. */
	diff_eq_int("tone timer expired (%ld)", saw_node1_expired > 0, 1, 0);
	diff_eq_int("tone timer running (%ld)", saw_node1_running > 0, 1, 0);

	/* Sub-state 2's detector, both verdicts. */
	diff_eq_int("tone gone, advanced (%ld)", saw_node2_quiet > 0, 1, 0);
	diff_eq_int("tone still there (%ld)", saw_node2_signal > 0, 1, 0);

	/* Sub-state 3's two gates on running the detectors at all. */
	diff_eq_int("rx symbols produced (%ld)", saw_rx_symbols > 0, 1, 0);
	diff_eq_int("rx block empty (%ld)", saw_rx_empty > 0, 1, 0);
	diff_eq_int("SignalDetect returned 1 (%ld)", saw_signal > 0, 1, 0);
	diff_eq_int("SignalDetect returned other (%ld)", saw_no_signal > 0, 1,
		    0);

	/*
	 * The three ways the S1 accumulator can move.  "held" is the
	 * hysteresis -- a detection above V22_LOOP_S1_HOLD_MS that does NOT
	 * reset the counter -- and it is the one arm a reconstruction reading
	 * the guard the other way round would still pass without.
	 */
	diff_eq_int("r08 stepped by a block (%ld)", saw_r08_grew > 0, 1, 0);
	diff_eq_int("r08 reset by a detection (%ld)", saw_r08_reset > 0, 1, 0);
	diff_eq_int("r08 held past the hysteresis (%ld)", saw_r08_held > 0, 1,
		    0);

	/* Both arms of the raw-symbol ones detector. */
	diff_eq_int("raw symbols read as ones (%ld)", saw_ones > 0, 1, 0);
	diff_eq_int("raw symbols did not (%ld)", saw_no_ones > 0, 1, 0);
	diff_eq_int("r0a accumulated (%ld)", saw_r0a_grew > 0, 1, 0);

	/* Both scrambler gates, which is the rate and not the status byte. */
	diff_eq_int("scrambled at 1200 (%ld)", saw_scramble_1200 > 0, 1, 0);
	diff_eq_int("not scrambled at 2400 (%ld)", saw_scramble_2400 > 0, 1,
		    0);
	diff_eq_int("tx pattern forced to ones (%ld)", saw_pattern_forced > 0,
		    1, 0);

	/*
	 * The three exits of sub-state 3.  The first two share everything but
	 * one immediate in the object.
	 */
	diff_eq_int("handed over to connect_2400 (%ld)", saw_to_2400 > 0, 1, 0);
	diff_eq_int("handed over to connect_1200 (%ld)", saw_to_1200 > 0, 1, 0);
	diff_eq_int("node deadline expired (%ld)", saw_node3_timeout > 0, 1, 0);
	diff_eq_int("node deadline running (%ld)", saw_node3_running > 0, 1, 0);

	/*
	 * The format strings.  None is `v22_local_loop`'s -- it has none --
	 * so this counts what the two connect subroutines printed through the
	 * delegation, which is the only thing that can catch a wrong argument
	 * being handed on.
	 */
	diff_eq_int("debug transcript non-empty (%ld)", saw_debug_lines > 0, 1,
		    0);

	rc |= diff_end();
	return rc;
}
