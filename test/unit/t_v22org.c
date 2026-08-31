/*
 * t_v22org.c -- differential test of v22_answer and v22_originate.
 *
 * THE FIXTURE IS t_v22conn.c's, and for the same reason: these handlers run
 * the whole receive chain, the scrambler, the symbol coder, the pulse shaper,
 * the tone generator and three detectors on every call, so the only fixture
 * that means anything is a real `V22FP_create` graph.  Two are built per
 * scenario, the blob drives one and the reconstruction the other, and
 * afterwards both are compared entire -- the three structs with their pointers
 * blanked, the tone object, all three MTD detectors, all twenty-eight heap
 * regions and the four caller-owned buffers.
 *
 * A STATE HANDLER IS A STATE MACHINE AND ONE CALL PROVES NOTHING.  Every
 * scenario drives several blocks, and the guards at the bottom assert that
 * every arm this test believes exists was taken on the REFERENCE graph.
 *
 * ---------------------------------------------------------------------------
 * THE LEVERS, AND WHY EACH IS FAIR
 *
 * All of them poke BOTH graphs identically, which is the move t_v22rate.c and
 * t_v22conn.c already make; the comparison is unaffected because the two sides
 * start each block from the same state.
 *
 *   1. `hdx->connect_substate` selects the node, so each arm can be entered without walking
 *      the whole ladder.  Values with no case fall through to the epilogue
 *      with nothing but the status byte written.
 *
 *   2. `hdx->gtimer`, `hdx->node_deadline`, `hdx->r08`, `hdx->r0a`, `hdx->r2c` and
 *      `hdx->r32` are the counters the arms compare against.  Poking them per
 *      block -- the `repoke` column -- makes each comparison land exactly
 *      where the probe wants it.  NOTE that `hdx->gtimer` only ever presents
 *      `gtimer + 20` to `ReadGTimer`, so a deadline constant needs TWO probes:
 *      a start of C-20 presents exactly C and must not fire, and C-19 presents
 *      C+1 and must.  `v22_answer`'s NODE_3 tone gate is the exception -- it
 *      compares `hdx->gtimer` itself, so C and C+1 are direct.
 *
 *   3. NEGATIVE values in those counters are what separate the object's
 *      unsigned tests from the signed ones the same C source would produce
 *      with the declared types.  `-100` on the clock is above every deadline
 *      unsigned and below every one signed, and `-1` in `r08` or `r0a` is
 *      65535 unsigned.  Nothing in the datapump can produce either; the
 *      differential test can, and does.
 *
 *   4. `mtd_s1->cfg.ratio` chooses `FPM_MTD_detect`'s verdict for the S1
 *      detector: 0x7fff makes the threshold the whole wideband energy so the
 *      tone is always PRESENT, and 0 makes it zero so anything with energy
 *      outside the band is ABSENT.  `mtd->cfg.ratio` and `mtd2->cfg.ratio`
 *      -- the `det_ratio` column -- do the same for `Detect_v22`, which is
 *      what `v22_originate`'s NODE_1 branches on: a 0 there makes both
 *      detectors report ABSENT on every sub-block, which is exactly what that
 *      function counts.
 *
 *   5. `dsp->fse.decision` IS A FUNCTION POINTER, so the symbols the equaliser
 *      reports can be chosen, which is what drives `Detect_1s`.  With the
 *      1200 ideal of 3 and the object's 0.85 threshold, a constant 3 makes it
 *      fire and a constant 0 does not -- so both of its call sites can be
 *      steered.  The stand-in calls the real slicer first, so `angle` and
 *      `mag` still feed the phase loop; only the reported symbol is replaced,
 *      and `fse.decision` is blanked before the comparison.
 *
 *   6. THE DESCRAMBLER HAS TO BE STOOD DOWN for the forced symbol to survive
 *      to the SECOND `Detect_1s`, and left alone for the second call to be
 *      given anything different from the first.  `FPM_SDM_descrambler`
 *      computes `(reg >> shift1) ^ in ^ (reg >> shift2)`, so equal shifts
 *      cancel both tap terms; `SetRxRate` recomputes the pair from the taps,
 *      so it is re-applied every block.  The `descram` column picks.
 *
 *   7. `hdx->protocol` IS WHAT MAKES `DemodDataV22` RETURN ZERO.  Its disconnect
 *      check runs only while `r0e` is 0, and then a block whose `FPM_rms` is
 *      below `params.disconnect_thresh` returns no symbols at all.  That is
 *      the only lever on the `*rxcount != 0` test, whose operand is the
 *      demodulator's RESULT and not the count handed in.
 *
 *      A SHORT INPUT BLOCK IS NOT AN ALTERNATIVE and was measured: with
 *      `dsp->r2e` at 2 -- every mode-0 graph -- `DemodDataV22` fills a
 *      `V22_IIR_BLOCK` stack array with only `count` samples and then runs the
 *      whole thing through the mixer, so anything shorter than a full block
 *      makes the result depend on stack garbage.  Handing in 16, 32, 48, 80
 *      and 120 samples each made the two sides differ from byte `2 * count`
 *      onwards, exactly at the boundary of what was filled.
 *
 *   8. `params.bps2` is the third operand of `v22_answer`'s V22bis verdict and
 *      picks `v22_originate`'s NODE_5 transmit pattern.  v22fp.h records that
 *      it is unconditionally a copy of `params.bps`, so poking it is the only
 *      way to exercise both without changing the rest of the graph.
 *
 * Scenarios with `force = -1` leave the real slicer in place, so the arms are
 * also exercised with the object's own symbols and not only with a constant.
 *
 * ---------------------------------------------------------------------------
 * ONE INPUT THIS TEST MUST NOT CONSTRUCT
 *
 * `v22_originate`'s NODE_3 divides `hdx->r2c` by `hdx->r32` once `r08` passes
 * 135.  `V22FP_create` leaves `r32` at zero and only a block with symbols in
 * it increments it, so entering that arm with no block counted is a divide by
 * zero -- in the blob as much as here.  Every scenario that pokes `r08` near
 * the threshold pokes `r32` as well.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_iir.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22conn.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22org.h"
#include "dsplib/v22rate.h"
#include "dsplib/v22status.h"

extern void ref_v22_answer(struct v22fp *fp, unsigned short *txsym,
			   short *txout, short *rxin, unsigned short *rxsym,
			   unsigned short *txcount, unsigned short *rxcount);
extern void ref_v22_originate(struct v22fp *fp, unsigned short *txsym,
			      short *txout, short *rxin, unsigned short *rxsym,
			      unsigned short *txcount,
			      unsigned short *rxcount);

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

/* Every heap region the graph owns, in a fixed order.  t_v22conn.c's list. */
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
	struct fpm_mtd s1a = blank_mtd(mine->hdx->mtd_s1);
	struct fpm_mtd s1b = blank_mtd(theirs->hdx->mtd_s1);
	struct fpm_mtd m1a = blank_mtd(mine->hdx->mtd);
	struct fpm_mtd m1b = blank_mtd(theirs->hdx->mtd);
	struct fpm_mtd m2a = blank_mtd(mine->hdx->mtd2);
	struct fpm_mtd m2b = blank_mtd(theirs->hdx->mtd2);
	int n, m, i;

	diff_eq_obj("object", struct v22fp, &oa, &ob, tag);
	diff_eq_obj("hdx", struct v22fp_hdx, &ha, &hb, tag);
	diff_eq_obj("dsp", struct v22fp_dsp, &da, &db, tag);
	diff_eq_obj("tone", struct fpm_tone, &ta, &tb, tag);
	diff_eq_obj("mtd_s1", struct fpm_mtd, &s1a, &s1b, tag);
	diff_eq_obj("mtd", struct fpm_mtd, &m1a, &m1b, tag);
	diff_eq_obj("mtd2", struct fpm_mtd, &m2a, &m2b, tag);

	n = regions_of(mine, ra);
	m = regions_of(theirs, rb);
	diff_eq_int("same region count", n, m, tag);
	for (i = 0; i < n && i < m; i++)
		cmp_raw(ra[i].name, ra[i].p, rb[i].p, ra[i].n, tag);
}

/* ------------------------------------------------------------------------ */

static unsigned short forced_symbol;

static unsigned short
decision_forced(struct v22_fse *state, short *angle, short *mag)
{
	(void)FSEv22_decision24(state, angle, mag);
	return forced_symbol;
}

/* ------------------------------------------------------------------------ */

static struct v22fp_cfg
base_cfg(int mode, int rate, int f0c)
{
	struct v22fp_cfg c;

	/* What `v22_create` passes, with mode, rate and flags bit 10 open. */
	c.mode = mode;
	c.rate = rate;
	c.f08 = 60000;
	c.f0c = f0c;
	c.f10 = 700;
	c.f14 = 0;
	c.f18 = 1;
	return c;
}

/* ------------------------------------------------------------------------ */

#define BLOCK	160		/* received samples handed in per call      */
#define TXSYMS	12		/* symbols asked for per call               */
#define INBUF	1024		/* DemodDataV22 destroys and overruns `in`  */
#define SYMBUF	256
#define TXOUT	4096

#define KEEP	(-32768)	/* "leave this counter alone"               */

struct scenario {
	const char *name;
	int mode;		/* V22FP_create mode                        */
	int rate;		/* V22FP_create rate: 0 -> 2400, 1 -> 1200  */
	int f0c;		/* -> params.flags bit 10 -> st.flags2 bit 0*/
	short node;		/* hdx->connect_substate poked before the first block    */
	int gtimer;		/* hdx->gtimer, or KEEP                     */
	int node_deadline;	/* hdx->node_deadline, the give-up one     */
	int r08;		/* hdx->r08, or KEEP                        */
	int r0a;		/* hdx->r0a, or KEEP                        */
	int r2c;		/* hdx->r2c, or KEEP                        */
	int r32;		/* hdx->r32, or KEEP                        */
	int protocol;		/* hdx->protocol, or KEEP                  */
	int bps2;		/* params.bps2, or 0 to leave it            */
	int repoke;		/* re-apply the counters every block        */
	int force;		/* forced symbol, or -1 for the real slicer */
	int descram;		/* 1: leave the descrambler live anyway     */
	int mtd_ratio;		/* mtd_s1->cfg.ratio, or -1 to leave it     */
	int det_ratio;		/* mtd and mtd2 ratio, or -1 to leave them  */
	int amp;		/* input amplitude                          */
	int blocks;
};

/* Coverage, all read off the REFERENCE graph. */
static long saw_node[2][16];
static long saw_node_other[2];
static long saw_default[2];
static long saw_conn2400[2], saw_conn1200[2];

static long saw_n0_txnop, saw_n0_data;
static long saw_n1_wait, saw_n1_done;
static long saw_n3_tone, saw_n3_notone;
static long saw_n3_r08_reset, saw_n3_r08_keep, saw_n3_r08_inc;
static long saw_n3_r0a_reset, saw_n3_r0a_keep, saw_n3_r0a_inc;
static long saw_n3_quiet;
static long saw_n3_v22bis, saw_n3_v22, saw_n3_error5, saw_n3_nothing;
static long saw_n4_wait, saw_n4_done;
static long saw_silence_tick, saw_silence_done;

static long saw_o_n0;
static long saw_o_n1_wait, saw_o_n1_det, saw_o_n1_error1;
static long saw_o_n3_sym, saw_o_n3_nosym;
static long saw_o_n3_wait, saw_o_n3_done;
static long saw_o_n3_mean_hi, saw_o_n3_mean_lo, saw_o_n3_error3;
static long saw_o_n4_wait, saw_o_n4_done;
static long saw_o_n5_wait, saw_o_n5_done;
static long saw_o_n5_r08_reset, saw_o_n5_r08_inc, saw_o_n5_r0a_inc;
static long saw_o_n5_quiet;
static long saw_o_n6_r08_reset, saw_o_n6_r08_keep, saw_o_n6_r08_inc;
static long saw_o_n6_r0a_inc;
static long saw_o_n6_v22bis, saw_o_n6_v22, saw_o_n6_error4, saw_o_n6_nothing;

/* ------------------------------------------------------------------------ */

static const struct scenario ans_scenarios[] = {
	/* ---- NODE_0: both exits of the V22_status bit ------------------ */
	{ "N0 data path",       0, 0, 0,  0, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "N0 TxNOP path",      0, 0, 1,  0, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "N0 mode 1",          1, 1, 0,  0, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1,  8000, 1 },

	/* ---- NODE_1: the 2100 Hz answer tone and its deadline ---------- */
	{ "N1 tone, running",   0, 0, 1,  1, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 8 },
	{ "N1 boundary -",      0, 0, 1,  1, 3280, 60000, KEEP, KEEP, KEEP,
	  KEEP, KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "N1 boundary +",      0, 0, 1,  1, 3281, 60000, KEEP, KEEP, KEEP,
	  KEEP, KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "N1 negative clock",  0, 0, 1,  1, -100, 60000, KEEP, KEEP, KEEP,
	  KEEP, KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },

	/* ---- NODE_SILENCE_AFTER_2100 ----------------------------------- */
	{ "N14 silence",        0, 0, 1, 14, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 6 },
	{ "N14 again",          0, 0, 1, 14, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 3 },

	/* ---- NODE_3: the tone gate on hdx->gtimer ---------------------- */
	{ "N3 tone gate -",     0, 0, 0,  3, 1470, 60000, 0, 0, KEEP, KEEP,
	  KEEP, 0, 1, -1, 0, -1, -1, 12000, 3 },
	{ "N3 tone gate +",     0, 0, 0,  3, 1471, 60000, 0, 0, KEEP, KEEP,
	  KEEP, 0, 1, -1, 0, -1, -1, 12000, 3 },
	{ "N3 tone gate neg",   0, 0, 0,  3, -100, 60000, 0, 0, KEEP, KEEP,
	  KEEP, 0, 1, -1, 0, -1, -1, 12000, 3 },

	/* ---- NODE_3: the S1 detector's two arms ------------------------ */
	{ "N3 detected, short", 0, 0, 0,  3, 0, 60000, 20, 30, KEEP, KEEP,
	  KEEP, 0, 1, 0, 0, 0x7fff, -1, 12000, 3 },
	{ "N3 detected, long",  0, 0, 0,  3, 0, 60000, 100, 30, KEEP, KEEP,
	  KEEP, 1200, 1, 0, 0, 0x7fff, -1, 12000, 3 },
	{ "N3 undetected",      0, 0, 0,  3, 0, 60000, 20, 30, KEEP, KEEP,
	  KEEP, 0, 1, 0, 0, 0, -1, 12000, 12 },
	{ "N3 r08 boundary -",  0, 0, 0,  3, 0, 60000, 59, 0, KEEP, KEEP,
	  KEEP, 1200, 1, 0, 0, 0x7fff, -1, 12000, 2 },
	{ "N3 r08 boundary +",  0, 0, 0,  3, 0, 60000, 60, 0, KEEP, KEEP,
	  KEEP, 1200, 1, 0, 0, 0x7fff, -1, 12000, 2 },

	/* ---- NODE_3: no signal, and no symbols ------------------------- */
	{ "N3 silent",          0, 0, 0,  3, 0, 60000, 20, 30, KEEP, KEEP,
	  KEEP, 0, 1, -1, 0, -1, -1, 0, 4 },
	{ "N3 no symbols",      0, 0, 0,  3, 0, 60000, 20, 30, KEEP, KEEP,
	  0, 0, 1, -1, 0, -1, -1, 0, 4 },

	/* ---- NODE_3: Detect_1s, both call sites ------------------------ */
	{ "N3 ones on 1st",     0, 0, 0,  3, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "N3 ones on 2nd",     0, 0, 0,  3, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 0, 1, 0, 1, 0x7fff, -1, 12000, 8 },
	{ "N3 no ones at all",  0, 0, 0,  3, 0, 60000, 20, 40, KEEP, KEEP,
	  KEEP, 0, 1, 0, 0, 0x7fff, -1, 12000, 3 },
	{ "N3 r0a boundary -",  0, 0, 0,  3, 0, 60000, 0, 59, KEEP, KEEP,
	  KEEP, 0, 1, 0, 0, 0x7fff, -1, 12000, 2 },
	{ "N3 r0a boundary +",  0, 0, 0,  3, 0, 60000, 0, 60, KEEP, KEEP,
	  KEEP, 0, 1, 0, 0, 0x7fff, -1, 12000, 2 },

	/* ---- NODE_3: the three verdicts -------------------------------- */
	{ "N3 V22bis carrier",  0, 0, 0,  3, 0, 60000, 100, 0, KEEP, KEEP,
	  KEEP, 2400, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "N3 bis, wrong bps",  0, 0, 0,  3, 0, 60000, 100, 0, KEEP, KEEP,
	  KEEP, 1200, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "N3 bis, gap open",   0, 0, 0,  3, 0, 60000, 100, 0, KEEP, KEEP,
	  KEEP, 2400, 1, 3, 0, 0, -1, 12000, 3 },
	{ "N3 bis, r08 = -1",   0, 0, 0,  3, 0, 60000, -1, 0, KEEP, KEEP,
	  KEEP, 2400, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "N3 V22 carrier",     0, 0, 0,  3, 0, 60000, 0, 231, KEEP, KEEP,
	  KEEP, 1200, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "N3 r0a carrier -",   0, 0, 0,  3, 0, 60000, 0, 230, KEEP, KEEP,
	  KEEP, 1200, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "N3 r0a = -1",        0, 0, 0,  3, 0, 60000, 0, -1, KEEP, KEEP,
	  KEEP, 1200, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "N3 ERROR5",          0, 0, 0,  3, 81, 100, 0, 0, KEEP, KEEP,
	  KEEP, 1200, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "N3 ERROR5 bound -",  0, 0, 0,  3, 80, 100, 0, 0, KEEP, KEEP,
	  KEEP, 1200, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "N3 ERROR5 neg clk",  0, 0, 0,  3, -100, 60000, 0, 0, KEEP, KEEP,
	  KEEP, 1200, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "N3 real slicer",     1, 0, 0,  3, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 40 },
	{ "N3 real, 1200",      0, 1, 0,  3, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1,  6000, 40 },

	/* ---- NODE_4 ---------------------------------------------------- */
	{ "N4 running",         0, 0, 0,  4, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 3 },
	{ "N4 boundary -",      0, 0, 0,  4, 77, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "N4 boundary +",      0, 0, 0,  4, 78, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "N4 negative clock",  0, 0, 0,  4, -100, 60000, KEEP, KEEP, KEEP,
	  KEEP, KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "N4 forced symbols",  0, 0, 0,  4, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 15, 0, -1, -1, 12000, 3 },

	/* ---- the connect handoffs -------------------------------------- */
	{ "N8 -> connect_2400", 0, 0, 0,  8, 0, 1400, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 15, 0, -1, -1, 12000, 30 },
	{ "N10 -> connect_2400",0, 0, 0, 10, 0, 1400, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 15, 0, -1, -1, 12000, 20 },
	{ "N12 -> connect_1200",0, 1, 0, 12, 0, 1400, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 3, 0, -1, -1, 12000, 30 },
	{ "N13 -> connect_1200",0, 1, 0, 13, 0,  300, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 0, 0, -1, -1, 12000, 20 },

	/* ---- the arms with no case ------------------------------------- */
	{ "no case 2",          0, 0, 0,  2, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 },
	{ "no case 5",          0, 0, 0,  5, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 },
	{ "no case 7",          0, 0, 0,  7, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 },
	{ "no case 15",         0, 0, 0, 15, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 },
	{ "no case 300",        0, 0, 0,300, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 },
	{ "no case -3",         0, 0, 0, -3, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 }
};

static const struct scenario org_scenarios[] = {
	/* ---- NODE_0 ---------------------------------------------------- */
	{ "O N0",               0, 0, 0,  0, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "O N0 mode 1",        1, 1, 1,  0, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1,  8000, 1 },

	/* ---- NODE_1: Detect_v22 and the give-up deadline ---------------- */
	{ "O N1 no detect",     0, 0, 0,  1, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, 0x7fff, 12000, 4 },
	{ "O N1 detect",        0, 0, 0,  1, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, 0, 12000, 4 },
	{ "O N1 ERROR1",        0, 0, 0,  1, 81, 100, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 1, -1, 0, -1, 0x7fff, 12000, 3 },
	{ "O N1 ERROR1 bnd -",  0, 0, 0,  1, 80, 100, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 1, -1, 0, -1, 0x7fff, 12000, 2 },
	{ "O N1 negative clk",  0, 0, 0,  1, -100, 60000, KEEP, KEEP, KEEP,
	  KEEP, KEEP, 0, 1, -1, 0, -1, 0x7fff, 12000, 2 },

	/* ---- NODE_3: the rms mean, the run counter and the deadline ----- */
	{ "O N3 counting",      0, 0, 0,  3, 0, 60000, 0, KEEP, 0, 0,
	  KEEP, 0, 0, 3, 0, -1, -1, 12000, 12 },
	{ "O N3 no symbols",    0, 0, 0,  3, 0, 60000, 0, KEEP, 0, 0,
	  0, 0, 0, -1, 0, -1, -1, 0, 4 },
	{ "O N3 r08 bound -",   0, 0, 0,  3, 0, 60000, 135, KEEP, 1000, 5,
	  KEEP, 0, 1, 0, 0, -1, -1, 12000, 2 },
	{ "O N3 r08 bound +",   0, 0, 0,  3, 0, 60000, 136, KEEP, 1000, 5,
	  KEEP, 0, 1, 0, 0, -1, -1, 12000, 2 },
	{ "O N3 mean high",     0, 0, 0,  3, 0, 60000, 136, KEEP, 100000, 5,
	  KEEP, 0, 1, 0, 0, -1, -1, 12000, 2 },
	{ "O N3 r08 = -1",      0, 0, 0,  3, 0, 60000, -1, KEEP, 1000, 5,
	  KEEP, 0, 1, 0, 0, -1, -1, 12000, 2 },
	{ "O N3 ERROR3",        0, 0, 0,  3, 81, 100, 0, KEEP, 0, 1,
	  KEEP, 0, 1, 0, 0, -1, -1, 12000, 3 },
	{ "O N3 ERROR3 bnd -",  0, 0, 0,  3, 80, 100, 0, KEEP, 0, 1,
	  KEEP, 0, 1, 0, 0, -1, -1, 12000, 2 },
	{ "O N3 negative clk",  0, 0, 0,  3, -100, 60000, 0, KEEP, 0, 1,
	  KEEP, 0, 1, 0, 0, -1, -1, 12000, 2 },

	/* ---- NODE_4 ---------------------------------------------------- */
	{ "O N4 running",       0, 0, 0,  4, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 4 },
	{ "O N4 boundary -",    0, 0, 0,  4, 396, 60000, KEEP, KEEP, KEEP,
	  KEEP, KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "O N4 boundary +",    0, 0, 0,  4, 397, 60000, KEEP, KEEP, KEEP,
	  KEEP, KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },
	{ "O N4 negative clk",  0, 0, 0,  4, -100, 60000, KEEP, KEEP, KEEP,
	  KEEP, KEEP, 0, 0, -1, 0, -1, -1, 12000, 1 },

	/* ---- NODE_5: both transmit patterns and the two counters -------- */
	{ "O N5 1200 pattern",  0, 1, 0,  5, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 1200, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "O N5 2400 pattern",  0, 0, 0,  5, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 2400, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "O N5 undetected",    0, 0, 0,  5, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 2400, 1, 0, 0, 0, -1, 12000, 12 },
	{ "O N5 ones on 2nd",   0, 0, 0,  5, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 2400, 1, 0, 1, 0x7fff, -1, 12000, 8 },
	{ "O N5 silent",        0, 0, 0,  5, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 2400, 1, -1, 0, -1, -1, 0, 4 },
	{ "O N5 boundary -",    0, 0, 0,  5, 516, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 2400, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N5 boundary +",    0, 0, 0,  5, 517, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 2400, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N5 negative clk",  0, 0, 0,  5, -100, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 2400, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N5 real slicer",   1, 0, 0,  5, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 30 },

	/* ---- NODE_6: the two verdicts and the deadline ------------------ */
	{ "O N6 detected",      0, 0, 0,  6, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "O N6 undetected",    0, 0, 0,  6, 0, 60000, 20, 30, KEEP, KEEP,
	  KEEP, 0, 1, 0, 0, 0, -1, 12000, 12 },
	{ "O N6 ones on 2nd",   0, 0, 0,  6, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 0, 1, 0, 1, 0x7fff, -1, 12000, 8 },
	{ "O N6 V22bis",        0, 0, 0,  6, 0, 60000, 100, 0, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "O N6 bis, gap open", 0, 0, 0,  6, 0, 60000, 100, 0, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0, -1, 12000, 12 },
	{ "O N6 r08 bound -",   0, 0, 0,  6, 0, 60000, 59, 0, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N6 r08 bound +",   0, 0, 0,  6, 0, 60000, 60, 0, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N6 r08 = -1",      0, 0, 0,  6, 0, 60000, -1, 0, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N6 V22",           0, 0, 0,  6, 0, 60000, 0, 231, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "O N6 r0a bound -",   0, 0, 0,  6, 0, 60000, 0, 230, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N6 r0a = -1",      0, 0, 0,  6, 0, 60000, 0, -1, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N6 ERROR4",        0, 0, 0,  6, 1907, 60000, 0, 0, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 3 },
	{ "O N6 ERROR4 bnd -",  0, 0, 0,  6, 1906, 60000, 0, 0, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N6 negative clk",  0, 0, 0,  6, -100, 60000, 0, 0, KEEP, KEEP,
	  KEEP, 0, 1, 3, 0, 0x7fff, -1, 12000, 2 },
	{ "O N6 silent",        0, 0, 0,  6, 0, 60000, 20, 20, KEEP, KEEP,
	  KEEP, 0, 1, -1, 0, -1, -1, 0, 4 },
	{ "O N6 real slicer",   1, 0, 0,  6, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 30 },
	{ "O N6 real, 1200",    0, 1, 0,  6, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1,  6000, 40 },

	/* ---- the connect handoffs -------------------------------------- */
	{ "O N8 -> conn_2400",  0, 0, 0,  8, 0, 1400, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 15, 0, -1, -1, 12000, 30 },
	{ "O N11 -> conn_2400", 0, 0, 0, 11, 0,  300, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 0, 0, -1, -1, 12000, 20 },
	{ "O N12 -> conn_1200", 0, 1, 0, 12, 0, 1400, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 3, 0, -1, -1, 12000, 30 },
	{ "O N13 -> conn_1200", 0, 1, 0, 13, 0,  300, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, 0, 0, -1, -1, 12000, 20 },

	/* ---- the arms with no case ------------------------------------- */
	{ "O no case 2",        0, 0, 0,  2, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 },
	{ "O no case 7",        0, 0, 0,  7, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 },
	{ "O no case 14",       0, 0, 0, 14, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 },
	{ "O no case 300",      0, 0, 0,300, 0, 60000, KEEP, KEEP, KEEP, KEEP,
	  KEEP, 0, 0, -1, 0, -1, -1, 12000, 2 }
};

#define NANS ((int)(sizeof(ans_scenarios) / sizeof(ans_scenarios[0])))
#define NORG ((int)(sizeof(org_scenarios) / sizeof(org_scenarios[0])))

/* ------------------------------------------------------------------------ */

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

static void
poke_counters(struct v22fp *fp, const struct scenario *s)
{
	if (s->gtimer != KEEP)
		fp->hdx->gtimer = s->gtimer;
	if (s->r08 != KEEP)
		fp->hdx->r08 = (short)s->r08;
	if (s->r0a != KEEP)
		fp->hdx->r0a = (short)s->r0a;
	if (s->r2c != KEEP)
		fp->hdx->r2c = s->r2c;
	if (s->r32 != KEEP)
		fp->hdx->r32 = (short)s->r32;
	if (s->protocol != KEEP)
		fp->hdx->protocol = (short)s->protocol;
}

static void
note_node(int which, short node)
{
	if (node >= 0 && node < 16)
		saw_node[which][node]++;
	else
		saw_node_other[which]++;
}

/* ------------------------------------------------------------------------ */

static void
cover_answer(struct v22fp *a, short node_before, short r08_before,
	     short r0a_before, int gtimer_before, long tag)
{
	unsigned char status_after = a->status;

	switch (node_before) {
	case V22_ANS_NODE_0:
		if (a->hdx->connect_substate == V22_ANS_NODE_1)
			saw_n0_txnop++;
		else if (a->hdx->connect_substate == V22_ANS_NODE_3)
			saw_n0_data++;
		break;

	case V22_ANS_NODE_1:
		if (a->hdx->connect_substate == V22_ANS_NODE_SILENCE_AFTER_2100)
			saw_n1_done++;
		else
			saw_n1_wait++;
		break;

	case V22_ANS_NODE_3:
		if ((unsigned int)gtimer_before > V22_ANS_NODE_3_TONE_MS)
			saw_n3_tone++;
		else
			saw_n3_notone++;

		if (a->hdx->connect_substate == V22_ANS_NODE_4)
			saw_n3_v22bis++;
		else if (a->hdx->connect_substate == V22_NODE_1200_12)
			saw_n3_v22++;
		else if (status_after == V22_MSG_ERROR5)
			saw_n3_error5++;
		else
			saw_n3_nothing++;

		/*
		 * The counters are only readable when no verdict fired -- both
		 * verdicts zero the pair on the way out.
		 */
		if (a->hdx->connect_substate == node_before
		    && status_after != V22_MSG_ERROR5) {
			short r08 = a->hdx->r08;
			short r0a = a->hdx->r0a;

			if (r08 == (short)(r08_before + V22_BLOCK_MS)
			    && r08_before != 0)
				saw_n3_r08_inc++;
			else if (r08 == 0 && r08_before != 0)
				saw_n3_r08_reset++;
			else if (r08 == r08_before && r08_before != 0)
				saw_n3_r08_keep++;

			if (r0a > r0a_before)
				saw_n3_r0a_inc++;
			else if (r0a == 0 && r0a_before != 0)
				saw_n3_r0a_reset++;
			else if (r0a == r0a_before && r0a_before != 0)
				saw_n3_r0a_keep++;

			if (r08 == r08_before && r0a == r0a_before
			    && r08_before != 0 && r0a_before != 0)
				saw_n3_quiet++;
		}
		break;

	case V22_ANS_NODE_4:
		if (a->hdx->connect_substate == V22_NODE_2400A)
			saw_n4_done++;
		else
			saw_n4_wait++;
		break;

	case V22_NODE_2400A:
	case V22_NODE_2400B:
	case V22_NODE_2400C:
	case V22_NODE_2400D:
		saw_conn2400[0]++;
		break;

	case V22_NODE_1200_12:
	case V22_NODE_1200_13:
		saw_conn1200[0]++;
		break;

	case V22_ANS_NODE_SILENCE_AFTER_2100:
		if (a->hdx->connect_substate == V22_ANS_NODE_3)
			saw_silence_done++;
		else
			saw_silence_tick++;
		break;

	default:
		saw_default[0]++;
		diff_eq_int("no-case arm left connect_substate alone", a->hdx->connect_substate,
			    node_before, tag);
		diff_eq_int("no-case arm set status", a->status,
			    V22_STATUS_01, tag);
		break;
	}
}

static void
cover_originate(struct v22fp *a, short node_before, short r08_before,
		short r0a_before, short r32_before, short r34_before, long tag)
{
	unsigned char status_after = a->status;

	switch (node_before) {
	case V22_ORG_NODE_0:
		if (a->hdx->connect_substate == V22_ORG_NODE_1)
			saw_o_n0++;
		break;

	case V22_ORG_NODE_1:
		if (a->hdx->connect_substate == V22_ORG_NODE_3)
			saw_o_n1_det++;
		else
			saw_o_n1_wait++;
		if (status_after == V22_MSG_ERROR1)
			saw_o_n1_error1++;
		break;

	case V22_ORG_NODE_3:
		if (a->hdx->r32 != r32_before)
			saw_o_n3_sym++;
		else
			saw_o_n3_nosym++;

		if (a->hdx->connect_substate == V22_ORG_NODE_4) {
			saw_o_n3_done++;
			if (a->hdx->rx_shift != r34_before)
				saw_o_n3_mean_hi++;
			else
				saw_o_n3_mean_lo++;
		} else {
			saw_o_n3_wait++;
		}
		if (status_after == V22_MSG_ERROR3)
			saw_o_n3_error3++;
		break;

	case V22_ORG_NODE_4:
		if (a->hdx->connect_substate == V22_ORG_NODE_5)
			saw_o_n4_done++;
		else
			saw_o_n4_wait++;
		break;

	case V22_ORG_NODE_5:
		if (a->hdx->connect_substate == V22_ORG_NODE_6)
			saw_o_n5_done++;
		else
			saw_o_n5_wait++;
		{
			short r08 = a->hdx->r08;
			short r0a = a->hdx->r0a;

			if (r08 == (short)(r08_before + V22_BLOCK_MS)
			    && r08_before != 0)
				saw_o_n5_r08_inc++;
			else if (r08 == 0 && r08_before != 0)
				saw_o_n5_r08_reset++;

			if (r0a > r0a_before)
				saw_o_n5_r0a_inc++;
			if (r08 == r08_before && r0a == r0a_before
			    && r08_before != 0 && r0a_before != 0)
				saw_o_n5_quiet++;
		}
		break;

	case V22_ORG_NODE_6:
		if (a->hdx->connect_substate == V22_NODE_2400A)
			saw_o_n6_v22bis++;
		else if (a->hdx->connect_substate == V22_NODE_1200_12)
			saw_o_n6_v22++;
		else if (status_after == V22_MSG_ERROR4)
			saw_o_n6_error4++;
		else
			saw_o_n6_nothing++;

		if (a->hdx->connect_substate == node_before
		    && status_after != V22_MSG_ERROR4) {
			short r08 = a->hdx->r08;
			short r0a = a->hdx->r0a;

			if (r08 == (short)(r08_before + V22_BLOCK_MS)
			    && r08_before != 0)
				saw_o_n6_r08_inc++;
			else if (r08 == 0 && r08_before != 0)
				saw_o_n6_r08_reset++;
			else if (r08 == r08_before && r08_before != 0)
				saw_o_n6_r08_keep++;

			if (r0a > r0a_before)
				saw_o_n6_r0a_inc++;
		}
		break;

	case V22_NODE_2400A:
	case V22_NODE_2400B:
	case V22_NODE_2400C:
	case V22_NODE_2400D:
		saw_conn2400[1]++;
		diff_eq_int("the 2400 handoff set the r1e bit",
			    (a->r1e[0] & V22FP_R1E_BIT3) != 0, 1, tag);
		break;

	case V22_NODE_1200_12:
	case V22_NODE_1200_13:
		saw_conn1200[1]++;
		break;

	default:
		saw_default[1]++;
		diff_eq_int("no-case arm left connect_substate alone", a->hdx->connect_substate,
			    node_before, tag);
		diff_eq_int("no-case arm set status", a->status,
			    V22_STATUS_01, tag);
		break;
	}
}

/* ------------------------------------------------------------------------ */

static void
run_scenario(const struct scenario *s, int which, long *tagp)
{
	static short txout_a[TXOUT], txout_b[TXOUT];
	static short in_a[INBUF], in_b[INBUF];
	static unsigned short tsym_a[SYMBUF], tsym_b[SYMBUF];
	static unsigned short rsym_a[SYMBUF], rsym_b[SYMBUF];
	static short src[BLOCK];
	struct v22fp_cfg cfg = base_cfg(s->mode, s->rate, s->f0c);
	struct v22fp *a = V22FP_create(0, &cfg);
	struct v22fp *b = V22FP_create(0, &cfg);
	unsigned short tc_a, tc_b, rc_a, rc_b;
	int blk;

	a->hdx->connect_substate = s->node;
	b->hdx->connect_substate = s->node;
	a->hdx->node_deadline = s->node_deadline;
	b->hdx->node_deadline = s->node_deadline;
	poke_counters(a, s);
	poke_counters(b, s);
	if (s->bps2 != 0) {
		a->params.bps2 = (short)s->bps2;
		b->params.bps2 = (short)s->bps2;
	}
	if (s->mtd_ratio >= 0) {
		a->hdx->mtd_s1->cfg.ratio = (short)s->mtd_ratio;
		b->hdx->mtd_s1->cfg.ratio = (short)s->mtd_ratio;
	}
	if (s->det_ratio >= 0) {
		a->hdx->mtd->cfg.ratio = (short)s->det_ratio;
		b->hdx->mtd->cfg.ratio = (short)s->det_ratio;
		a->hdx->mtd2->cfg.ratio = (short)s->det_ratio;
		b->hdx->mtd2->cfg.ratio = (short)s->det_ratio;
	}

	memset(txout_a, 0, sizeof(txout_a));
	memset(txout_b, 0, sizeof(txout_b));
	memset(tsym_a, 0, sizeof(tsym_a));
	memset(tsym_b, 0, sizeof(tsym_b));
	memset(rsym_a, 0, sizeof(rsym_a));
	memset(rsym_b, 0, sizeof(rsym_b));

	for (blk = 0; blk < s->blocks; blk++) {
		short node_before, r08_before, r0a_before;
		short r32_before, r34_before;
		int gtimer_before;
		long tag = *tagp;

		if (blk > 0 && s->repoke) {
			poke_counters(a, s);
			poke_counters(b, s);
		}
		if (s->force >= 0) {
			forced_symbol = (unsigned short)s->force;
			a->dsp->fse.decision = decision_forced;
			b->dsp->fse.decision = decision_forced;
			if (!s->descram) {
				a->dsp->sdm2.shift2 = a->dsp->sdm2.shift1;
				b->dsp->sdm2.shift2 = b->dsp->sdm2.shift1;
			}
		}

		fill_block(src, BLOCK, s->amp);
		memset(in_a, 0, sizeof(in_a));
		memset(in_b, 0, sizeof(in_b));
		memcpy(in_a, src, sizeof(src));
		memcpy(in_b, src, sizeof(src));

		tc_a = tc_b = TXSYMS;
		rc_a = rc_b = BLOCK;

		node_before = a->hdx->connect_substate;
		r08_before = a->hdx->r08;
		r0a_before = a->hdx->r0a;
		r32_before = a->hdx->r32;
		r34_before = a->hdx->rx_shift;
		gtimer_before = a->hdx->gtimer;
		note_node(which, node_before);

		if (which == 0) {
			ref_v22_answer(a, tsym_a, txout_a, in_a, rsym_a,
				       &tc_a, &rc_a);
			v22_answer(b, tsym_b, txout_b, in_b, rsym_b,
				   &tc_b, &rc_b);
		} else {
			ref_v22_originate(a, tsym_a, txout_a, in_a, rsym_a,
					  &tc_a, &rc_a);
			v22_originate(b, tsym_b, txout_b, in_b, rsym_b,
				      &tc_b, &rc_b);
		}

		diff_eq_int("txcount, case %ld", tc_b, tc_a, tag);
		diff_eq_int("rxcount, case %ld", rc_b, rc_a, tag);
		cmp_raw("tx symbols", tsym_b, tsym_a, sizeof(tsym_a), tag);
		cmp_raw("tx samples", txout_b, txout_a, sizeof(txout_a), tag);
		cmp_raw("rx samples", in_b, in_a, sizeof(in_a), tag);
		cmp_raw("rx symbols", rsym_b, rsym_a, sizeof(rsym_a), tag);
		compare_graphs(b, a, tag);

		if (which == 0)
			cover_answer(a, node_before, r08_before, r0a_before,
				     gtimer_before, tag);
		else
			cover_originate(a, node_before, r08_before, r0a_before,
					r32_before, r34_before, tag);

		*tagp = tag + 1;
	}

	V22FP_delete(a);
	V22FP_delete(b);
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	long tag;
	int i;
	int rc = 0;

	diff_begin("v22_answer");
	rng_seed(0x6d31a90bUL);
	tag = 0;
	for (i = 0; i < NANS; i++)
		run_scenario(&ans_scenarios[i], 0, &tag);
	rc |= diff_end();

	diff_begin("v22_originate");
	rng_seed(0x14c05e3fUL);
	tag = 0;
	for (i = 0; i < NORG; i++)
		run_scenario(&org_scenarios[i], 1, &tag);
	rc |= diff_end();

	diff_begin("v22org coverage guards");

	/* ---- v22_answer ------------------------------------------------ */
	diff_eq_int("A NODE_0 entered (%ld)",
		    saw_node[0][V22_ANS_NODE_0] > 0, 1, 0);
	diff_eq_int("A NODE_1 entered (%ld)",
		    saw_node[0][V22_ANS_NODE_1] > 0, 1, 0);
	diff_eq_int("A NODE_3 entered (%ld)",
		    saw_node[0][V22_ANS_NODE_3] > 0, 1, 0);
	diff_eq_int("A NODE_4 entered (%ld)",
		    saw_node[0][V22_ANS_NODE_4] > 0, 1, 0);
	diff_eq_int("A NODE_SILENCE entered (%ld)",
		    saw_node[0][V22_ANS_NODE_SILENCE_AFTER_2100] > 0, 1, 0);
	diff_eq_int("A 2400 connect node entered (%ld)",
		    saw_conn2400[0] > 0, 1, 0);
	diff_eq_int("A 1200 connect node entered (%ld)",
		    saw_conn1200[0] > 0, 1, 0);
	diff_eq_int("A no-case node entered (%ld)", saw_default[0] > 0, 1, 0);
	diff_eq_int("A node above the table entered (%ld)",
		    saw_node_other[0] > 0, 1, 0);

	diff_eq_int("A NODE_0 took the data path (%ld)", saw_n0_data > 0, 1, 0);
	diff_eq_int("A NODE_0 took the TxNOP path (%ld)", saw_n0_txnop > 0, 1,
		    0);
	diff_eq_int("A NODE_1 still generating (%ld)", saw_n1_wait > 0, 1, 0);
	diff_eq_int("A NODE_1 deadline fired (%ld)", saw_n1_done > 0, 1, 0);
	diff_eq_int("A NODE_3 sent the tone (%ld)", saw_n3_tone > 0, 1, 0);
	diff_eq_int("A NODE_3 sent data (%ld)", saw_n3_notone > 0, 1, 0);
	diff_eq_int("A NODE_3 reset r08 (%ld)", saw_n3_r08_reset > 0, 1, 0);
	diff_eq_int("A NODE_3 kept a long r08 (%ld)", saw_n3_r08_keep > 0, 1,
		    0);
	diff_eq_int("A NODE_3 advanced r08 (%ld)", saw_n3_r08_inc > 0, 1, 0);
	diff_eq_int("A NODE_3 reset r0a (%ld)", saw_n3_r0a_reset > 0, 1, 0);
	diff_eq_int("A NODE_3 kept r0a (%ld)", saw_n3_r0a_keep > 0, 1, 0);
	diff_eq_int("A NODE_3 advanced r0a (%ld)", saw_n3_r0a_inc > 0, 1, 0);
	diff_eq_int("A NODE_3 touched neither counter (%ld)", saw_n3_quiet > 0,
		    1, 0);
	diff_eq_int("A NODE_3 declared V22bis (%ld)", saw_n3_v22bis > 0, 1, 0);
	diff_eq_int("A NODE_3 declared V22 (%ld)", saw_n3_v22 > 0, 1, 0);
	diff_eq_int("A NODE_3 reported ERROR5 (%ld)", saw_n3_error5 > 0, 1, 0);
	diff_eq_int("A NODE_3 declared nothing (%ld)", saw_n3_nothing > 0, 1,
		    0);
	diff_eq_int("A NODE_4 still training (%ld)", saw_n4_wait > 0, 1, 0);
	diff_eq_int("A NODE_4 deadline fired (%ld)", saw_n4_done > 0, 1, 0);
	diff_eq_int("A SILENCE ticked (%ld)", saw_silence_tick > 0, 1, 0);
	diff_eq_int("A SILENCE reached four (%ld)", saw_silence_done > 0, 1, 0);

	/* ---- v22_originate --------------------------------------------- */
	diff_eq_int("O NODE_0 entered (%ld)",
		    saw_node[1][V22_ORG_NODE_0] > 0, 1, 0);
	diff_eq_int("O NODE_1 entered (%ld)",
		    saw_node[1][V22_ORG_NODE_1] > 0, 1, 0);
	diff_eq_int("O NODE_3 entered (%ld)",
		    saw_node[1][V22_ORG_NODE_3] > 0, 1, 0);
	diff_eq_int("O NODE_4 entered (%ld)",
		    saw_node[1][V22_ORG_NODE_4] > 0, 1, 0);
	diff_eq_int("O NODE_5 entered (%ld)",
		    saw_node[1][V22_ORG_NODE_5] > 0, 1, 0);
	diff_eq_int("O NODE_6 entered (%ld)",
		    saw_node[1][V22_ORG_NODE_6] > 0, 1, 0);
	diff_eq_int("O 2400 connect node entered (%ld)",
		    saw_conn2400[1] > 0, 1, 0);
	diff_eq_int("O 1200 connect node entered (%ld)",
		    saw_conn1200[1] > 0, 1, 0);
	diff_eq_int("O no-case node entered (%ld)", saw_default[1] > 0, 1, 0);
	diff_eq_int("O node above the table entered (%ld)",
		    saw_node_other[1] > 0, 1, 0);

	diff_eq_int("O NODE_0 advanced to NODE_1 (%ld)", saw_o_n0 > 0, 1, 0);
	diff_eq_int("O NODE_1 still waiting (%ld)", saw_o_n1_wait > 0, 1, 0);
	diff_eq_int("O NODE_1 detected V.22 (%ld)", saw_o_n1_det > 0, 1, 0);
	diff_eq_int("O NODE_1 reported ERROR1 (%ld)", saw_o_n1_error1 > 0, 1,
		    0);
	diff_eq_int("O NODE_3 saw symbols (%ld)", saw_o_n3_sym > 0, 1, 0);
	diff_eq_int("O NODE_3 saw none (%ld)", saw_o_n3_nosym > 0, 1, 0);
	diff_eq_int("O NODE_3 still counting (%ld)", saw_o_n3_wait > 0, 1, 0);
	diff_eq_int("O NODE_3 advanced to NODE_4 (%ld)", saw_o_n3_done > 0, 1,
		    0);
	diff_eq_int("O NODE_3 mean above r30 (%ld)", saw_o_n3_mean_hi > 0, 1,
		    0);
	diff_eq_int("O NODE_3 mean below r30 (%ld)", saw_o_n3_mean_lo > 0, 1,
		    0);
	diff_eq_int("O NODE_3 reported ERROR3 (%ld)", saw_o_n3_error3 > 0, 1,
		    0);
	diff_eq_int("O NODE_4 still receiving (%ld)", saw_o_n4_wait > 0, 1, 0);
	diff_eq_int("O NODE_4 advanced to NODE_5 (%ld)", saw_o_n4_done > 0, 1,
		    0);
	diff_eq_int("O NODE_5 still running (%ld)", saw_o_n5_wait > 0, 1, 0);
	diff_eq_int("O NODE_5 advanced to NODE_6 (%ld)", saw_o_n5_done > 0, 1,
		    0);
	diff_eq_int("O NODE_5 reset r08 (%ld)", saw_o_n5_r08_reset > 0, 1, 0);
	diff_eq_int("O NODE_5 advanced r08 (%ld)", saw_o_n5_r08_inc > 0, 1, 0);
	diff_eq_int("O NODE_5 advanced r0a (%ld)", saw_o_n5_r0a_inc > 0, 1, 0);
	diff_eq_int("O NODE_5 touched neither counter (%ld)",
		    saw_o_n5_quiet > 0, 1, 0);
	diff_eq_int("O NODE_6 reset r08 (%ld)", saw_o_n6_r08_reset > 0, 1, 0);
	diff_eq_int("O NODE_6 kept a long r08 (%ld)", saw_o_n6_r08_keep > 0, 1,
		    0);
	diff_eq_int("O NODE_6 advanced r08 (%ld)", saw_o_n6_r08_inc > 0, 1, 0);
	diff_eq_int("O NODE_6 advanced r0a (%ld)", saw_o_n6_r0a_inc > 0, 1, 0);
	diff_eq_int("O NODE_6 declared V22bis (%ld)", saw_o_n6_v22bis > 0, 1,
		    0);
	diff_eq_int("O NODE_6 declared V22 (%ld)", saw_o_n6_v22 > 0, 1, 0);
	diff_eq_int("O NODE_6 reported ERROR4 (%ld)", saw_o_n6_error4 > 0, 1,
		    0);
	diff_eq_int("O NODE_6 declared nothing (%ld)", saw_o_n6_nothing > 0, 1,
		    0);

	rc |= diff_end();

	return rc;
}
