/*
 * t_v22conn.c -- differential test of connect_1200 and connect_2400.
 *
 * THE FIXTURE IS THE SAME ONE t_v22rate.c BUILDS, and for the same reason:
 * these two handlers run the whole receive chain, the scrambler, the symbol
 * coder and the pulse shaper on every call, so the only fixture that means
 * anything is a real `V22FP_create` graph.  Two are built per scenario, the
 * blob drives one and the reconstruction the other, and afterwards both are
 * compared entire -- the three structs with their pointers blanked, the tone
 * detector, all twenty-eight heap regions, and the four caller-owned buffers.
 *
 * A STATE HANDLER IS A STATE MACHINE AND ONE CALL PROVES NOTHING.  Every
 * scenario below drives sixty to a hundred blocks so that the shared
 * millisecond clock actually crosses each node's deadline, and the guards at
 * the bottom assert that every arm this test believes exists was taken on the
 * REFERENCE graph.  A stub would satisfy a single call; it would not satisfy
 * "NODE_2400A advanced to B, B to C, C latched a trained verdict, D timed
 * out, and the carrier came back inside the grace window".
 *
 * ---------------------------------------------------------------------------
 * THE THREE LEVERS THAT MAKE THE ARMS REACHABLE, AND WHY EACH IS FAIR
 *
 * All three poke BOTH graphs identically, which is the same move t_v22rate.c
 * makes with `dsp->r04`; the comparison is unaffected because the two sides
 * start each block from the same state.
 *
 *   1. `hdx->r0c` is set directly, so each node can be entered without
 *      walking the whole ladder, and a value with no case (0, 7, 14) drops
 *      straight through to the carrier-loss tail with no callee having run.
 *      That is what makes the tail deterministic: nothing has touched
 *      `sre.active` between the poke and `CarrierDetect`.
 *
 *   2. `dsp->sre.active` is `CarrierDetect`'s field (v22prc.h's
 *      V22FP_CARRIER), and `hdx->r3c` is the carrier-loss counter.  Setting
 *      the pair chooses which of the tail's four arms runs.
 *
 *   3. `dsp->fse.decision` IS A FUNCTION POINTER, so the symbols the
 *      equaliser reports can be chosen.  `RxTrained1200` wants every symbol
 *      to be 3 and `RxTrained2400` wants more than seven trailing 15s;
 *      neither is reachable from the noise this test can generate, and the
 *      trained arms are half of both functions.  The stand-in calls the real
 *      slicer for its `angle` and `mag` outputs -- which feed the phase loop
 *      and must stay live -- and replaces only the symbol it returns.  It is
 *      installed on both graphs, so both call the same code.
 *
 * Scenarios with `force = -1` leave the real slicer in place, so the untrained
 * arms are also exercised with the object's own symbols and not only with a
 * forced 0.
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
#include "dsplib/v22rate.h"

extern void ref_connect_1200(struct v22fp *fp, unsigned short *txsym,
			     short *txout, short *rxin, unsigned short *rxsym,
			     unsigned short *txcount,
			     unsigned short *rxcount);
extern void ref_connect_2400(struct v22fp *fp, unsigned short *txsym,
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

/* Every heap region the graph owns, in a fixed order.  t_v22rate.c's list. */
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
	int n, m, i;

	diff_eq_obj("object", struct v22fp, &oa, &ob, tag);
	diff_eq_obj("hdx", struct v22fp_hdx, &ha, &hb, tag);
	diff_eq_obj("dsp", struct v22fp_dsp, &da, &db, tag);
	diff_eq_obj("tone", struct fpm_tone, &ta, &tb, tag);

	n = regions_of(mine, ra);
	m = regions_of(theirs, rb);
	diff_eq_int("same region count", n, m, tag);
	for (i = 0; i < n && i < m; i++)
		cmp_raw(ra[i].name, ra[i].p, rb[i].p, ra[i].n, tag);
}

/* ------------------------------------------------------------------------ */

/*
 * The stand-in slicer.  The real one runs first, so `angle` and `mag` -- the
 * phase loop's inputs -- are exactly what they would have been; only the
 * symbol it reports is replaced.  Both graphs get this same function, so the
 * two sides remain in step; `fse.decision` is blanked before comparison,
 * which is why installing it costs no coverage.
 */
static unsigned short forced_symbol;

static unsigned short
decision_forced(struct v22_fse *state, short *angle, short *mag)
{
	(void)FSEv22_decision24(state, angle, mag);
	return forced_symbol;
}

/* ------------------------------------------------------------------------ */

static struct v22fp_cfg
base_cfg(int mode, int f14)
{
	struct v22fp_cfg c;

	/* What `v22_create` passes, with mode and flags bit 11 left open. */
	c.mode = mode;
	c.rate = 0;
	c.f08 = 60000;
	c.f0c = 0;
	c.f10 = 700;
	c.f14 = f14;
	c.f18 = 1;
	return c;
}

/* ------------------------------------------------------------------------ */

#define BLOCK	160		/* received samples handed in per call      */
#define TXSYMS	12		/* symbols asked for per call               */
#define INBUF	1024		/* DemodDataV22 destroys and overruns `in`  */
#define SYMBUF	256
#define TXOUT	4096

/* Coverage, all read off the REFERENCE graph. */
static long saw_node[16];		/* by the r0c the call started in   */
static long saw_node_other;
static long saw_advance[16];		/* r0c changed while in that node   */
static long saw_deadline[16];		/* the node's own deadline fired    */
static long saw_no_deadline[16];
static long saw_latch_call, saw_latch_skip;
static long saw_latched_1, saw_latched_other;
static long saw_trained_yes, saw_trained_no;
static long saw_connect_2400, saw_connect_1200;
static long saw_error7, saw_error1200;
static long saw_carrier, saw_no_carrier;
static long saw_retrain, saw_carrier_clean;
static long saw_nocarrier_report, saw_nocarrier_hold;

struct scenario {
	const char *name;
	int mode;		/* V22FP_create mode                        */
	int f14;		/* V22FP_create flags bit 11                */
	short node;		/* hdx->r0c poked before the first block    */
	int r04;		/* hdx->r04, the last node's deadline       */
	int force;		/* forced symbol, or -1 for the real slicer */
	int carrier;		/* poked into sre.active, or -1 to leave it */
	int r10;		/* hdx->r10 poked before the first block    */
	int r3c;		/* hdx->r3c poked before the first block    */
	/*
	 * `params.r18` is the carrier-loss grace time, and the tail rewrites
	 * the status byte once it expires.  A node scenario that wants its
	 * own status to survive to the end of the call raises it; the tail
	 * scenarios keep the 700 ms `v22_create` configures.  0 leaves it.
	 */
	int r18;
	int amp;		/* input amplitude                          */
	int blocks;
	/*
	 * `hdx->gtimer` poked before the first block.  `ReadGTimer` adds 20
	 * and returns the new value, so a clock left at 0 only ever presents
	 * MULTIPLES OF 20 to a deadline and every constant between two of
	 * them reads the same -- 755 and 754 are indistinguishable, which an
	 * injected off-by-one confirmed.  Starting the clock 20 short of a
	 * deadline puts the comparison exactly on its boundary and pins the
	 * constant; starting it NEGATIVE is what distinguishes the object's
	 * `jbe` from a `jle`, because -80 is above 449 unsigned and below it
	 * signed.
	 */
	int gtimer;
};

#define LONG_GRACE	30000	/* park the tail so a node's status survives */

static const struct scenario scenarios[] = {
	/* The 2400 ladder, walked end to end from NODE_2400A. */
	{ "2400A ladder, trained",   0, 0,  8, 1400, 15, -1, 0, 0,
	  LONG_GRACE, 12000, 90, 0 },
	{ "2400A ladder, untrained", 0, 0,  8, 1400,  0, -1, 0, 0,
	  LONG_GRACE, 12000, 90, 0 },
	{ "2400A ladder, real",      1, 0,  8, 1400, -1, -1, 0, 0,
	  LONG_GRACE, 12000, 90, 0 },
	{ "2400A ladder, mode 1",    1, 1,  8, 1400, 15, -1, 0, 0,
	  LONG_GRACE,  8000, 90, 0 },

	/* NODE_2400C entered directly, with the latch already set. */
	{ "2400C latched 1",         0, 0, 10, 1400, -1, -1, 1, 0,
	  LONG_GRACE, 12000, 60, 0 },
	{ "2400C latched 2",         0, 0, 10, 1400, -1, -1, 2, 0,
	  LONG_GRACE, 12000, 60, 0 },
	{ "2400C trained",           0, 0, 10, 1400, 15, -1, 0, 0,
	  LONG_GRACE, 12000, 60, 0 },

	/* NODE_2400D: the deadline arm and the trained arm. */
	{ "2400D timeout",           0, 0, 11,  300,  0, -1, 0, 0,
	  LONG_GRACE, 12000, 40, 0 },
	{ "2400D trained",           0, 0, 11, 1400, 15, -1, 0, 0,
	  LONG_GRACE, 12000, 40, 0 },
	{ "2400D real slicer",       1, 0, 11, 1400, -1, -1, 0, 0,
	  LONG_GRACE, 12000, 40, 0 },

	/* The 1200 pair. */
	{ "1200_12 trained",         0, 0, 12, 1400,  3, -1, 0, 0,
	  LONG_GRACE, 12000, 90, 0 },
	{ "1200_12 untrained",       0, 0, 12,  900,  0, -1, 0, 0,
	  LONG_GRACE, 12000, 90, 0 },
	{ "1200_12 latched 2",       0, 0, 12, 1400, -1, -1, 2, 0,
	  LONG_GRACE, 12000, 60, 0 },
	{ "1200_13 trained",         0, 0, 13, 1400,  3, -1, 0, 0,
	  LONG_GRACE, 12000, 40, 0 },
	{ "1200_13 timeout",         0, 0, 13,  300,  0, -1, 0, 0,
	  LONG_GRACE, 12000, 40, 0 },
	{ "1200_13 real slicer",     1, 1, 13, 1400, -1, -1, 0, 0,
	  LONG_GRACE, 12000, 40, 0 },

	/*
	 * The tail on its own.  `node` matches no case, so nothing runs
	 * before `CarrierDetect` and the poked carrier and counter decide
	 * which arm is taken.  These keep the configured 700 ms grace.
	 */
	{ "tail: carrier, clean",    0, 0,  0, 1400, -1,  1, 0, 0,
	  0,     0, 10, 0 },
	{ "tail: carrier back",      0, 0,  7, 1400, -1,  1, 0, 5,
	  0,     0, 10, 0 },
	{ "tail: loss inside grace", 0, 0, 14, 1400, -1,  0, 0, 0,
	  0,     0, 30, 0 },
	{ "tail: loss reported",     0, 0,  0, 1400, -1,  0, 0, 0,
	  0,     0, 60, 0 },
	{ "tail: loss, r3c primed",  1, 1, 14, 1400, -1,  0, 0, 40,
	  0,     0, 10, 0 },

	/*
	 * THE DEADLINE BOUNDARIES, AND IT TAKES TWO PROBES PER CONSTANT.
	 * The clock only ever presents `gtimer + 20k`, so one probe pins a
	 * constant from one side only: a start of C-20 presents exactly C,
	 * which must NOT fire and therefore separates C from C-1, and a start
	 * of C-19 presents C+1, which MUST fire and separates C from C+1.
	 * Measured rather than reasoned -- with only the first of each pair,
	 * an injected 790 -> 791 went unnoticed while 449 -> 448 did not.
	 */
	{ "2400A boundary -",        0, 0,  8, 1400, -1, -1, 0, 0,
	  LONG_GRACE, 12000,  4, 429 },
	{ "2400A boundary +",        0, 0,  8, 1400, -1, -1, 0, 0,
	  LONG_GRACE, 12000,  4, 430 },
	{ "2400B boundary -",        0, 0,  9, 1400, -1, -1, 0, 0,
	  LONG_GRACE, 12000,  4, 580 },
	{ "2400B boundary +",        0, 0,  9, 1400, -1, -1, 0, 0,
	  LONG_GRACE, 12000,  4, 581 },
	{ "2400C boundary -",        0, 0, 10, 1400, -1, -1, 2, 0,
	  LONG_GRACE, 12000,  4, 770 },
	{ "2400C boundary +",        0, 0, 10, 1400, -1, -1, 2, 0,
	  LONG_GRACE, 12000,  4, 771 },
	{ "2400D boundary -",        0, 0, 11,  300,  0, -1, 0, 0,
	  LONG_GRACE, 12000,  4, 280 },
	{ "2400D boundary +",        0, 0, 11,  300,  0, -1, 0, 0,
	  LONG_GRACE, 12000,  4, 281 },
	{ "1200_12 boundary -",      0, 0, 12, 1400, -1, -1, 2, 0,
	  LONG_GRACE, 12000,  4, 735 },
	{ "1200_12 boundary +",      0, 0, 12, 1400, -1, -1, 2, 0,
	  LONG_GRACE, 12000,  4, 736 },
	{ "1200_13 boundary -",      0, 0, 13,  300,  0, -1, 0, 0,
	  LONG_GRACE, 12000,  4, 280 },
	{ "1200_13 boundary +",      0, 0, 13,  300,  0, -1, 0, 0,
	  LONG_GRACE, 12000,  4, 281 },

	/*
	 * A NEGATIVE CLOCK, which is the only input that separates the
	 * object's unsigned `jbe` from the signed test the same C source
	 * would produce with an `int` comparison.  Nothing in the datapump
	 * can produce one; the differential test can, and does.
	 */
	{ "2400A negative clock",    0, 0,  8, 1400, -1, -1, 0, 0,
	  LONG_GRACE, 12000,  3, -100 },
	{ "2400D negative clock",    0, 0, 11,  300,  0, -1, 0, 0,
	  LONG_GRACE, 12000,  3, -100 },
	{ "1200_12 negative clock",  0, 0, 12, 1400, -1, -1, 2, 0,
	  LONG_GRACE, 12000,  3, -100 },
	{ "1200_13 negative clock",  0, 0, 13,  300,  0, -1, 0, 0,
	  LONG_GRACE, 12000,  3, -100 }
};

#define NSCEN ((int)(sizeof(scenarios) / sizeof(scenarios[0])))

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
note_deadline(short node, int fired)
{
	if (node < 0 || node >= 16)
		return;
	if (fired)
		saw_deadline[node]++;
	else
		saw_no_deadline[node]++;
}

static void
note_node(short node)
{
	if (node >= 0 && node < 16)
		saw_node[node]++;
	else
		saw_node_other++;
}

/*
 * One scenario, run against one of the two handlers.  `which` picks it:
 * 0 is connect_1200 and 1 is connect_2400.
 */
static void
run_scenario(const struct scenario *s, int which, long *tagp)
{
	static short txout_a[TXOUT], txout_b[TXOUT];
	static short in_a[INBUF], in_b[INBUF];
	static unsigned short tsym_a[SYMBUF], tsym_b[SYMBUF];
	static unsigned short rsym_a[SYMBUF], rsym_b[SYMBUF];
	static short src[BLOCK];
	struct v22fp_cfg cfg = base_cfg(s->mode, s->f14);
	struct v22fp *a = V22FP_create(0, &cfg);
	struct v22fp *b = V22FP_create(0, &cfg);
	unsigned short tc_a, tc_b, rc_a, rc_b;
	int blk;

	a->hdx->r0c = s->node;
	b->hdx->r0c = s->node;
	a->hdx->r04 = s->r04;
	b->hdx->r04 = s->r04;
	a->hdx->r10 = s->r10;
	b->hdx->r10 = s->r10;
	a->hdx->r3c = s->r3c;
	b->hdx->r3c = s->r3c;
	a->hdx->gtimer = s->gtimer;
	b->hdx->gtimer = s->gtimer;
	if (s->r18 != 0) {
		a->params.r18 = (short)s->r18;
		b->params.r18 = (short)s->r18;
	}

	memset(txout_a, 0, sizeof(txout_a));
	memset(txout_b, 0, sizeof(txout_b));
	memset(tsym_a, 0, sizeof(tsym_a));
	memset(tsym_b, 0, sizeof(tsym_b));
	memset(rsym_a, 0, sizeof(rsym_a));
	memset(rsym_b, 0, sizeof(rsym_b));

	for (blk = 0; blk < s->blocks; blk++) {
		short node_before;
		int r10_before;
		unsigned char flags_before;
		int connected, timed_out, fired;
		long tag = *tagp;

		if (s->force >= 0) {
			forced_symbol = (unsigned short)s->force;
			a->dsp->fse.decision = decision_forced;
			b->dsp->fse.decision = decision_forced;
			/*
			 * AND THE DESCRAMBLER HAS TO BE STOOD DOWN TOO, or
			 * the lever does not reach.  Both handlers descramble
			 * IN PLACE before they ask `RxTrained*` anything, so
			 * a forced constant off the slicer arrives at the
			 * predicate as scrambled rubbish and the trained arm
			 * is unreachable.  `FPM_SDM_descrambler` computes
			 * `(reg >> shift1) ^ in ^ (reg >> shift2)`, so making
			 * the two shifts equal cancels both tap terms and
			 * leaves `in & mask` -- an identity for any word
			 * already inside the mask, which 3 and 15 both are.
			 * `SetRxRate` recomputes the pair from the taps, so
			 * it is re-applied every block.
			 */
			a->dsp->sdm2.shift2 = a->dsp->sdm2.shift1;
			b->dsp->sdm2.shift2 = b->dsp->sdm2.shift1;
		}
		if (s->carrier >= 0) {
			a->dsp->sre.active = s->carrier;
			b->dsp->sre.active = s->carrier;
		}

		fill_block(src, BLOCK, s->amp);
		memset(in_a, 0, sizeof(in_a));
		memset(in_b, 0, sizeof(in_b));
		memcpy(in_a, src, sizeof(src));
		memcpy(in_b, src, sizeof(src));

		tc_a = tc_b = TXSYMS;
		rc_a = rc_b = BLOCK;

		node_before = a->hdx->r0c;
		r10_before = a->hdx->r10;
		flags_before = a->flags;
		note_node(node_before);

		if (which == 0) {
			ref_connect_1200(a, tsym_a, txout_a, in_a, rsym_a,
					 &tc_a, &rc_a);
			connect_1200(b, tsym_b, txout_b, in_b, rsym_b,
				     &tc_b, &rc_b);
		} else {
			ref_connect_2400(a, tsym_a, txout_a, in_a, rsym_a,
					 &tc_a, &rc_a);
			connect_2400(b, tsym_b, txout_b, in_b, rsym_b,
				     &tc_b, &rc_b);
		}

		diff_eq_int("txcount, case %ld", tc_b, tc_a, tag);
		diff_eq_int("rxcount, case %ld", rc_b, rc_a, tag);
		cmp_raw("tx symbols", tsym_b, tsym_a, sizeof(tsym_a), tag);
		cmp_raw("tx samples", txout_b, txout_a, sizeof(txout_a), tag);
		cmp_raw("rx samples", in_b, in_a, sizeof(in_a), tag);
		cmp_raw("rx symbols", rsym_b, rsym_a, sizeof(rsym_a), tag);
		compare_graphs(b, a, tag);

		/* ---- coverage, from the reference graph only ---- */
		if (node_before >= 0 && node_before < 16) {
			if (a->hdx->r0c != node_before)
				saw_advance[node_before]++;
		}

		if (node_before == V22_NODE_2400C ||
		    node_before == V22_NODE_1200_12) {
			if (r10_before == 0)
				saw_latch_call++;
			else
				saw_latch_skip++;
		}

		if (a->dsp->sre.active)
			saw_carrier++;
		else
			saw_no_carrier++;

		/*
		 * THE STATUS BYTE IS NOT A WITNESS FOR THE NODE ARMS.  Every
		 * node falls into the tail, and the tail rewrites `status`
		 * whenever the carrier-loss grace expires -- so a block that
		 * did reach CONNECT can end with NO_CARRIER in that byte.  The
		 * FLAGS byte is the witness: both masks are OR-ed in and
		 * nothing ever clears a bit, so a mask appearing is a
		 * one-way transition that the tail cannot undo.  `status` is
		 * still the witness for the tail's own arms, which are the
		 * last thing to write it.
		 */
		connected = (a->flags & V22FP_FLAGS_CONNECT)
			     == V22FP_FLAGS_CONNECT &&
			    (flags_before & V22FP_FLAGS_CONNECT)
			     != V22FP_FLAGS_CONNECT;
		timed_out = (a->flags & V22FP_FLAGS_TIMEOUT) != 0 &&
			    (flags_before & V22FP_FLAGS_TIMEOUT) == 0;

		if (connected) {
			if (node_before == V22_NODE_2400C ||
			    node_before == V22_NODE_2400D)
				saw_connect_2400++;
			else
				saw_connect_1200++;
		}
		if (timed_out) {
			if (node_before == V22_NODE_2400D)
				saw_error7++;
			else
				saw_error1200++;
		}

		/* Did this node's own deadline fire on this block? */
		fired = 0;
		switch (node_before) {
		case V22_NODE_2400A:
			fired = a->hdx->r0c == V22_NODE_2400B;
			break;
		case V22_NODE_2400B:
			fired = a->hdx->r0c == V22_NODE_2400C;
			break;
		case V22_NODE_2400C:
			fired = a->hdx->r0c == V22_NODE_2400D || connected;
			break;
		case V22_NODE_2400D:
			fired = timed_out;
			break;
		case V22_NODE_1200_12:
			fired = a->hdx->r0c == V22_NODE_1200_13 || connected;
			break;
		case V22_NODE_1200_13:
			fired = timed_out;
			break;
		default:
			node_before = -1;
			break;
		}
		if (node_before >= 0)
			note_deadline(node_before, fired);

		if (a->status == V22_MSG_NO_CARRIER)
			saw_nocarrier_report++;
		if (a->status == V22_STATUS_0B)
			saw_retrain++;

		if (!a->dsp->sre.active &&
		    a->status != V22_MSG_NO_CARRIER)
			saw_nocarrier_hold++;
		if (a->dsp->sre.active && a->status != V22_STATUS_0B)
			saw_carrier_clean++;

		*tagp = tag + 1;
	}

	V22FP_delete(a);
	V22FP_delete(b);
}

static int
run_all(int which, const char *what)
{
	long tag = 0;
	int i;

	diff_begin(what);
	rng_seed(which == 0 ? 0x51a70c17UL : 0x2b9e4d05UL);
	for (i = 0; i < NSCEN; i++)
		run_scenario(&scenarios[i], which, &tag);
	return diff_end();
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= run_all(0, "connect_1200");
	rc |= run_all(1, "connect_2400");

	saw_latched_1 = saw_connect_2400 + saw_connect_1200;
	saw_latched_other = saw_advance[V22_NODE_2400C] +
			    saw_advance[V22_NODE_1200_12];
	saw_trained_yes = saw_connect_2400 + saw_connect_1200;
	saw_trained_no = saw_error7 + saw_error1200;

	diff_begin("v22conn coverage guards");

	diff_eq_int("NODE_2400A entered (%ld)",
		    saw_node[V22_NODE_2400A] > 0, 1, 0);
	diff_eq_int("NODE_2400B entered (%ld)",
		    saw_node[V22_NODE_2400B] > 0, 1, 0);
	diff_eq_int("NODE_2400C entered (%ld)",
		    saw_node[V22_NODE_2400C] > 0, 1, 0);
	diff_eq_int("NODE_2400D entered (%ld)",
		    saw_node[V22_NODE_2400D] > 0, 1, 0);
	diff_eq_int("NODE_1200 12 entered (%ld)",
		    saw_node[V22_NODE_1200_12] > 0, 1, 0);
	diff_eq_int("NODE_1200 13 entered (%ld)",
		    saw_node[V22_NODE_1200_13] > 0, 1, 0);
	diff_eq_int("a node with no case entered (%ld)",
		    saw_node[0] + saw_node[7] + saw_node[14] > 0, 1, 0);

	diff_eq_int("NODE_2400A deadline fired (%ld)",
		    saw_deadline[V22_NODE_2400A] > 0, 1, 0);
	diff_eq_int("NODE_2400A deadline not yet (%ld)",
		    saw_no_deadline[V22_NODE_2400A] > 0, 1, 0);
	diff_eq_int("NODE_2400B deadline fired (%ld)",
		    saw_deadline[V22_NODE_2400B] > 0, 1, 0);
	diff_eq_int("NODE_2400B deadline not yet (%ld)",
		    saw_no_deadline[V22_NODE_2400B] > 0, 1, 0);
	diff_eq_int("NODE_2400C deadline fired (%ld)",
		    saw_deadline[V22_NODE_2400C] > 0, 1, 0);
	diff_eq_int("NODE_2400C deadline not yet (%ld)",
		    saw_no_deadline[V22_NODE_2400C] > 0, 1, 0);
	diff_eq_int("NODE_1200 12 deadline fired (%ld)",
		    saw_deadline[V22_NODE_1200_12] > 0, 1, 0);
	diff_eq_int("NODE_1200 12 deadline not yet (%ld)",
		    saw_no_deadline[V22_NODE_1200_12] > 0, 1, 0);
	diff_eq_int("NODE_2400D deadline fired (%ld)",
		    saw_deadline[V22_NODE_2400D] > 0, 1, 0);
	diff_eq_int("NODE_2400D deadline not yet (%ld)",
		    saw_no_deadline[V22_NODE_2400D] > 0, 1, 0);
	diff_eq_int("NODE_1200 13 deadline fired (%ld)",
		    saw_deadline[V22_NODE_1200_13] > 0, 1, 0);
	diff_eq_int("NODE_1200 13 deadline not yet (%ld)",
		    saw_no_deadline[V22_NODE_1200_13] > 0, 1, 0);

	diff_eq_int("the latch was consulted (%ld)", saw_latch_call > 0, 1, 0);
	diff_eq_int("the latch was already set (%ld)", saw_latch_skip > 0, 1,
		    0);
	diff_eq_int("a latched 1 connected (%ld)", saw_latched_1 > 0, 1, 0);
	diff_eq_int("a latch other than 1 advanced (%ld)",
		    saw_latched_other > 0, 1, 0);

	diff_eq_int("CONNECT_2400 reached (%ld)", saw_connect_2400 > 0, 1, 0);
	diff_eq_int("the 1200 connect reached (%ld)", saw_connect_1200 > 0, 1,
		    0);
	diff_eq_int("ERROR7 reached (%ld)", saw_error7 > 0, 1, 0);
	diff_eq_int("the 1200 timeout reached (%ld)", saw_error1200 > 0, 1, 0);
	diff_eq_int("a trained verdict was taken (%ld)", saw_trained_yes > 0,
		    1, 0);
	diff_eq_int("an untrained verdict was taken (%ld)", saw_trained_no > 0,
		    1, 0);

	diff_eq_int("carrier present at the tail (%ld)", saw_carrier > 0, 1,
		    0);
	diff_eq_int("carrier absent at the tail (%ld)", saw_no_carrier > 0, 1,
		    0);
	diff_eq_int("carrier back inside the window (%ld)", saw_retrain > 0, 1,
		    0);
	diff_eq_int("carrier back with no window (%ld)", saw_carrier_clean > 0,
		    1, 0);
	diff_eq_int("NO_CARRIER reported (%ld)", saw_nocarrier_report > 0, 1,
		    0);
	diff_eq_int("NO_CARRIER withheld (%ld)", saw_nocarrier_hold > 0, 1, 0);

	rc |= diff_end();

	return rc;
}
