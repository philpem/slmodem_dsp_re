/*
 * t_v22hdx.c -- differential test of the two V22_PROTOCOL state handlers
 * `v22_retrain` and `v22_org_rmloop2`.
 *
 * THE FIXTURE IS t_v22rate.c's, and for the same reason: these handlers run
 * `ModDataV22`, `DemodDataV22`, `ScrambleDataV22`, `SetTxRate`, `SetRxRate`
 * and `ResetRx` over a live datapump, so the only thing worth comparing is two
 * whole object graphs.  Each check builds TWO with `V22FP_create` -- ours on
 * both sides, which t_v22fpcreate established is byte-identical to the blob's
 * -- drives the blob's handler through one and ours through the other, and
 * then compares the three structs with their pointers blanked plus the
 * twenty-eight heap regions behind those pointers, AND all four buffers the
 * handler was given.
 *
 * A SINGLE CALL EXERCISES ONE ARM OF ONE SUBSTATE, which is the whole problem
 * with testing a state machine one block at a time.  So the sweep is a table
 * of scenarios; each pins `hdx->r0c` to one substate, optionally pre-loads the
 * three state words the substate branches on, and runs a run of blocks.  The
 * guards at the bottom assert that every arm this test claims to cover was
 * actually taken, counted on the REFERENCE graph so the numbers describe the
 * blob and not us.
 *
 * WHERE THE RECEIVE SIGNAL COMES FROM, AND WHY IT IS NOT NOISE.  Three of the
 * arms here cannot be reached with noise at any amplitude: `Detect_1s` and
 * `Detect_Rmloop2_ACK` want a constant symbol for 231 ms, and `RxTrained2400`
 * wants more than seven consecutive 15s at the end of a block.  Random symbols
 * reach none of them -- eight 4-bit symbols agreeing is one chance in 2^32.
 * So the test carries a PEER: a second `struct v22fp`, built the same way,
 * whose `MakeTxData` / `ScrambleDataV22` / `ModDataV22` chain generates real
 * modulated V.22 and whose output is fed into the graph under test through a
 * FIFO, because the modulator does not produce exactly one 160-sample block
 * per twelve symbols and splicing at a block boundary would break the carrier.
 * Both sides get the same samples, so the peer is apparatus and not a second
 * implementation being compared.
 *
 * The peer is built with its own `cfg.mode`, because that is what picks the
 * transmit carrier: `V22FP_create` uses phase increment 0x666 when
 * `dsp->r2c == 1` (mode 0) and 0xccc otherwise (mode 1), while the RECEIVE
 * kernel is 0x222 whatever the mode.  So which of the two peer modes a
 * receiver can hear is a property of the object, not a choice, and the table
 * below sweeps both rather than assuming one.
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
#include "dsplib/v22data.h"
#include "dsplib/v22det.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22hdx.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22rate.h"

/*
 * The signature is `V22_PROTOCOL`'s, not this pair's: all seven handlers sit
 * in one table and `V22FP_modem` calls them through it, so the two counts are
 * `unsigned short *` here as they are for the other five.  The object cannot
 * tell the two signednesses apart at this boundary -- it passes the address
 * of a 16-bit stack slot -- and what each handler's body READS is a separate
 * question, settled per use inside it.  Finding F8534.
 */
typedef void (*hdx_fn)(struct v22fp *fp, unsigned short *txdata, short *txout,
		       short *rxin, unsigned short *rxsym,
		       unsigned short *txcount, unsigned short *rxcount);

extern void ref_v22_retrain(struct v22fp *fp, unsigned short *txdata,
			    short *txout, short *rxin, unsigned short *rxsym,
			    unsigned short *txcount, unsigned short *rxcount);
extern void ref_v22_org_rmloop2(struct v22fp *fp, unsigned short *txdata,
				short *txout, short *rxin,
				unsigned short *rxsym,
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

/* `mine` ran the reconstruction, `theirs` the blob. */
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

static struct v22fp_cfg
base_cfg(int mode, int f14, int rate)
{
	struct v22fp_cfg c;

	/* What `v22_create` passes, with mode, rate and flags bit 11 open. */
	c.mode = mode;
	c.rate = rate;
	c.f08 = 60000;
	c.f0c = 0;
	c.f10 = 700;
	c.f14 = f14;
	c.f18 = 1;
	return c;
}

/* ------------------------------------------------------------------------ */

#define BLOCK	160		/* V22_TX_BLOCK: samples per handler call    */
#define TXSYM	12		/* symbols per block                         */
#define BUFN	512
#define SYMN	256
#define FIFON	4096

/*
 * The peer: a second datapump used only as a signal generator.  `pattern` is a
 * MakeTxData selector, or PEER_SILENCE / PEER_NOISE for the two signals that
 * need no modulator.
 */
#define PEER_SILENCE	(-1)
#define PEER_NOISE	(-2)

struct peer {
	struct v22fp *fp;
	unsigned short data[SYMN];
	short out[BUFN];
	short fifo[FIFON];
	int fifo_len;
};

static void
peer_reset(struct peer *p)
{
	p->fifo_len = 0;
}

/*
 * One 160-sample block of whatever the peer is sending.  The FIFO exists
 * because `ModDataV22` does not return exactly BLOCK samples for TXSYM
 * symbols, and dropping or repeating the remainder would put a step in the
 * carrier every block and stop the receiver ever locking.
 */
static void
peer_block(struct peer *p, int pattern, int scramble, short *dst)
{
	int guard = 0;

	if (pattern == PEER_SILENCE) {
		memset(dst, 0, (size_t)BLOCK * sizeof(short));
		return;
	}
	if (pattern == PEER_NOISE) {
		int i;

		for (i = 0; i < BLOCK; i++)
			dst[i] = (short)((int)(rng_next() & 0x7fff) % 24000
					 - 12000);
		return;
	}

	while (p->fifo_len < BLOCK && guard++ < 64) {
		short n = TXSYM;
		int k;

		MakeTxData((short *)p->data, &n, (short)pattern);
		if (scramble)
			ScrambleDataV22(p->fp, p->data, (unsigned short)n);
		k = (int)ModDataV22(p->fp, p->data, p->out, (unsigned short)n);
		if (k <= 0)
			break;
		if (k > BUFN)
			k = BUFN;
		if (p->fifo_len + k > FIFON)
			k = FIFON - p->fifo_len;
		memcpy(p->fifo + p->fifo_len, p->out, (size_t)k * sizeof(short));
		p->fifo_len += k;
	}

	if (p->fifo_len >= BLOCK) {
		memcpy(dst, p->fifo, (size_t)BLOCK * sizeof(short));
		memmove(p->fifo, p->fifo + BLOCK,
			(size_t)(p->fifo_len - BLOCK) * sizeof(short));
		p->fifo_len -= BLOCK;
	} else {
		memset(dst, 0, (size_t)BLOCK * sizeof(short));
		p->fifo_len = 0;
	}
}

/* ------------------------------------------------------------------------ */

/*
 * A scenario: one substate, one signal, one set of pre-loaded state words, and
 * a run of blocks.  `-1` in any of the four presets means "leave whatever the
 * handler left there", which is what makes a run a run rather than a set of
 * independent calls.
 */
struct scen {
	int rx_mode;		/* cfg.mode for the graph under test        */
	int f14;		/* cfg.f14, i.e. params.flags bit 11        */
	int peer_mode;		/* cfg.mode for the peer                    */
	int pattern;		/* peer MakeTxData selector, or PEER_*      */
	int scramble;		/* peer scrambles before modulating         */
	int state;		/* forced into hdx->r0c, -1 to let it run   */
	int gtimer;		/* forced into hdx->gtimer, -1 to leave     */
	int r08;		/* forced into hdx->r08, -1 to leave        */
	int r10;		/* forced into hdx->r10, -1 to leave        */
	int r38;		/* forced into hdx+0x38, -1 to leave        */
	int blocks;
	/*
	 * Put all three graphs at 2400 bit/s before the run, the way substates
	 * 4, 5 and 6 leave the modem before it enters 7.  Without it the
	 * receiver's slicer is `FSEv22_decision12` and its symbols are two bits
	 * wide, so the fifteens `RxTrained2400` wants cannot occur at all.
	 * Last in the struct so that every row that does not need it can leave
	 * it out.
	 */
	int rate2400;
	/*
	 * `cfg.rate`, which is what sets `params.bps` and hence `params.bps2`:
	 * 0 gives 2400 and 1 gives 1200.  Both handlers pick their transmit
	 * pattern with `V22_TXDATA_ONES_1200 + (bps2 != 1200)`, so without a
	 * 1200 row that addition only ever produces one of its two values.
	 */
	int cfg_rate;
};

/*
 * What the reference graph did, per block, read from the two state words the
 * handler leaves behind.  `hdx->gtimer` after the call IS the value
 * `ReadGTimer` returned and compared, because nothing writes it afterwards.
 */
struct obs {
	short r0c_before, r0c_after;
	short r08_before, r08_after;
	int r10_before, r10_after;
	int gtimer_after;
	unsigned char status_after, flags_before, flags_after, r1e_after;
};

static long saw_case[8];
static long saw_r08_grew, saw_r08_flat;
static long saw_advance[8], saw_stay[8];
static long saw_timeout[8], saw_no_timeout[8];
static long saw_case2_to3, saw_case2_to4;
static long saw_trained2400;
static long saw_default_state;
static long saw_pattern_1200, saw_pattern_2400;

static long rm_case[3];
static long rm_r08_grew, rm_r08_flat;
static long rm_thresh[3], rm_no_thresh[3];
static long rm_ack_seen, rm_ack_missing;
static long rm_r10_preset, rm_r10_clear;
static long rm_timeout, rm_no_timeout;
static long rm_status6, rm_status7;
static long rm_default_state;
static long rm_pattern_1200, rm_pattern_2400;

/* ------------------------------------------------------------------------ */

/*
 * The halfword at hdx + 0x38 that `v22_retrain` substate 2 branches on.  It is
 * inside v22fp.h's unmodelled `r36[6]`, so it is written through `memcpy`
 * rather than through a field.
 */
static void
r38_set(struct v22fp_hdx *h, short v)
{
	memcpy(&h->r36[2], &v, sizeof(v));
}

/*
 * Drive one scenario through both sides.  Returns the number of blocks run, so
 * a scenario that produced nothing cannot be mistaken for one that passed.
 */
static long
run_scen(hdx_fn theirs, hdx_fn mine, const struct scen *s, long tag,
	 void (*record)(const struct obs *, const struct scen *))
{
	static short in_a[BUFN], in_b[BUFN];
	static short out_a[BUFN], out_b[BUFN];
	static unsigned short td_a[SYMN], td_b[SYMN];
	static unsigned short sym_a[SYMN], sym_b[SYMN];
	static short src[BUFN];
	static struct peer peer;

	struct v22fp_cfg cfg = base_cfg(s->rx_mode, s->f14, s->cfg_rate);
	struct v22fp_cfg pcfg = base_cfg(s->peer_mode, 0, s->cfg_rate);
	struct v22fp *a = V22FP_create(0, &cfg);
	struct v22fp *b = V22FP_create(0, &cfg);
	unsigned short txc_a, txc_b, rxc_a, rxc_b;
	long blocks = 0;
	int blk;

	peer.fp = V22FP_create(0, &pcfg);
	peer_reset(&peer);

	if (s->rate2400) {
		SetTxRate(peer.fp, V22_RATE_2400);
		SetRxRate(peer.fp, V22_RATE_2400);
		SetTxRate(a, V22_RATE_2400);
		SetRxRate(a, V22_RATE_2400);
		SetAdaptEqV22(a, 3);
		SetTxRate(b, V22_RATE_2400);
		SetRxRate(b, V22_RATE_2400);
		SetAdaptEqV22(b, 3);
	}

	for (blk = 0; blk < s->blocks; blk++) {
		struct obs o;

		if (s->state >= 0) {
			a->hdx->r0c = (short)s->state;
			b->hdx->r0c = (short)s->state;
		}
		if (s->gtimer >= 0) {
			a->hdx->gtimer = s->gtimer;
			b->hdx->gtimer = s->gtimer;
		}
		if (s->r08 >= 0) {
			a->hdx->r08 = (short)s->r08;
			b->hdx->r08 = (short)s->r08;
		}
		if (s->r10 >= 0) {
			a->hdx->r10 = s->r10;
			b->hdx->r10 = s->r10;
		}
		if (s->r38 >= 0) {
			r38_set(a->hdx, (short)s->r38);
			r38_set(b->hdx, (short)s->r38);
		}

		peer_block(&peer, s->pattern, s->scramble, src);

		memset(in_a, 0, sizeof(in_a));
		memset(in_b, 0, sizeof(in_b));
		memcpy(in_a, src, (size_t)BLOCK * sizeof(short));
		memcpy(in_b, src, (size_t)BLOCK * sizeof(short));
		memset(out_a, 0x33, sizeof(out_a));
		memset(out_b, 0x33, sizeof(out_b));
		memset(td_a, 0x5a, sizeof(td_a));
		memset(td_b, 0x5a, sizeof(td_b));
		memset(sym_a, 0x27, sizeof(sym_a));
		memset(sym_b, 0x27, sizeof(sym_b));
		txc_a = txc_b = TXSYM;
		rxc_a = rxc_b = BLOCK;

		o.r0c_before = a->hdx->r0c;
		o.r08_before = a->hdx->r08;
		o.r10_before = a->hdx->r10;
		o.flags_before = a->flags;

		theirs(a, td_a, out_a, in_a, sym_a, &txc_a, &rxc_a);
		mine(b, td_b, out_b, in_b, sym_b, &txc_b, &rxc_b);

		o.r0c_after = a->hdx->r0c;
		o.r08_after = a->hdx->r08;
		o.r10_after = a->hdx->r10;
		o.gtimer_after = a->hdx->gtimer;
		o.status_after = a->status;
		o.flags_after = a->flags;
		o.r1e_after = a->r1e[0];
		record(&o, s);

		diff_eq_int("txcount, case %ld", txc_b, txc_a, tag);
		diff_eq_int("rxcount, case %ld", rxc_b, rxc_a, tag);
		cmp_raw("txdata buffer", td_b, td_a, sizeof(td_a), tag);
		cmp_raw("txout buffer", out_b, out_a, sizeof(out_a), tag);
		cmp_raw("rxin buffer", in_b, in_a, sizeof(in_a), tag);
		cmp_raw("rxsym buffer", sym_b, sym_a, sizeof(sym_a), tag);
		compare_graphs(b, a, tag);

		blocks++;
	}

	V22FP_delete(a);
	V22FP_delete(b);
	V22FP_delete(peer.fp);
	return blocks;
}

/* ------------------------------------------------------------------------ */

static void
record_retrain(const struct obs *o, const struct scen *s)
{
	int st = o->r0c_before;

	if (st < 0 || st > 7) {
		saw_default_state++;
		return;
	}
	saw_case[st]++;
	if (st == 0) {
		if (s->cfg_rate == 0)
			saw_pattern_2400++;
		else
			saw_pattern_1200++;
	}

	if (o->r08_after == (short)(o->r08_before + V22_HDX_TICK_MS))
		saw_r08_grew++;
	else if (o->r08_after == o->r08_before)
		saw_r08_flat++;

	if (o->r0c_after != o->r0c_before)
		saw_advance[st]++;
	else
		saw_stay[st]++;

	switch (st) {
	case 0:
	case 3:
		if ((unsigned int)o->gtimer_after > V22_RETRAIN_T_SILENCE)
			saw_timeout[st]++;
		else
			saw_no_timeout[st]++;
		break;
	case 2:
		if ((unsigned int)o->gtimer_after > V22_RETRAIN_T_S1) {
			saw_timeout[st]++;
			if (o->r0c_after == 3)
				saw_case2_to3++;
			else if (o->r0c_after == 4)
				saw_case2_to4++;
		} else {
			saw_no_timeout[st]++;
		}
		break;
	case 4:
		if ((unsigned int)o->gtimer_after > V22_RETRAIN_T_ONES1200)
			saw_timeout[st]++;
		else
			saw_no_timeout[st]++;
		break;
	case 5:
		if ((unsigned int)o->gtimer_after > V22_RETRAIN_T_DESCR1200)
			saw_timeout[st]++;
		else
			saw_no_timeout[st]++;
		break;
	case 6:
		if ((unsigned int)o->gtimer_after > V22_RETRAIN_T_ONES2400)
			saw_timeout[st]++;
		else
			saw_no_timeout[st]++;
		break;
	case 7:
		if ((unsigned int)o->gtimer_after > V22_RETRAIN_T_TRAIN2400)
			saw_timeout[st]++;
		else
			saw_no_timeout[st]++;
		/* 0x28 is set by the trained arm and by nothing else. */
		if ((o->flags_after & 0x28) && !(o->flags_before & 0x28))
			saw_trained2400++;
		break;
	default:
		break;
	}
}

static void
record_rmloop2(const struct obs *o, const struct scen *s)
{
	int st = o->r0c_before;

	if (st < 0 || st > 2) {
		rm_default_state++;
		return;
	}
	rm_case[st]++;
	if (s->cfg_rate == 0)
		rm_pattern_2400++;
	else
		rm_pattern_1200++;

	if (o->r08_after > o->r08_before)
		rm_r08_grew++;
	else if (o->r08_after == o->r08_before)
		rm_r08_flat++;

	if (st == 1) {
		if (o->r0c_after == 2 && o->r08_after == 0) {
			/*
			 * The threshold arm sets `r10` when and only when the
			 * acknowledgement was NOT seen in this block, so the
			 * flag is what separates the two arms.  Entered with
			 * `r10` already set the arm is invisible, so those
			 * blocks are counted as neither.
			 */
			rm_thresh[1]++;
			if (o->r10_before == 0) {
				if (o->r10_after != 0)
					rm_ack_missing++;
				else
					rm_ack_seen++;
			}
		} else {
			rm_no_thresh[1]++;
		}
	}
	if (st == 2) {
		if (o->r10_before != 0)
			rm_r10_preset++;
		else
			rm_r10_clear++;
		if (o->status_after == 6)
			rm_status6++;
		if (o->status_after == 7)
			rm_status7++;
		if (o->r08_after == 0 && o->r08_before != 0)
			rm_thresh[2]++;
		else
			rm_no_thresh[2]++;
	}
	if (st == 1 || st == 2) {
		if ((unsigned int)o->gtimer_after > V22_RMLOOP2_TIMEOUT_MS)
			rm_timeout++;
		else
			rm_no_timeout++;
	}
}

/* ------------------------------------------------------------------------ */

/*
 * The retrain sweep.  Every substate is pinned in turn under four signals; the
 * timeouts are reached by pre-loading `hdx->gtimer` one tick short of each
 * substate's threshold rather than by waiting out two real seconds.
 */
static int
run_retrain(void)
{
	static const struct scen tab[] = {
	/* rx f14 pr pattern               scr st  gtim  r08 r10 r38 blk 24 rt*/
	/* -- substate 0: silence, noise, and a peer that is really there --  */
	 { 0, 0, 0, PEER_SILENCE,          0,  0,   -1,  -1, -1, -1, 12, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  0,   -1,  -1, -1, -1, 12, 0, 0 },
	 { 1, 0, 0, V22_TXDATA_ONES_2400,  1,  0,   -1,  -1, -1, -1, 24, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_2400,  1,  0,   -1,  -1, -1, -1, 24, 0, 0 },
	 { 0, 1, 1, V22_TXDATA_S1,         0,  0,   -1,  -1, -1, -1, 24, 0, 0 },
	/* the 1200 arm of the transmit-pattern selector                      */
	 { 0, 0, 0, PEER_NOISE,            0,  0,   -1,  -1, -1, -1, 12, 0, 1 },
	 { 1, 0, 0, V22_TXDATA_ONES_1200,  1,  0,   -1,  -1, -1, -1, 24, 0, 1 },
	/* r08 pre-loaded so the "it stopped" arm fires without a real run    */
	 { 0, 0, 0, PEER_SILENCE,          0,  0,   -1,  60, -1, -1,  6, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_2400,  1,  0,   -1,  60, -1, -1,  6, 0, 0 },
	/* and the 1,300 ms give-up                                          */
	 { 0, 0, 0, PEER_NOISE,            0,  0, 1300,  -1, -1, -1,  6, 0, 0 },
	 { 1, 1, 1, V22_TXDATA_ONES_2400,  1,  0, 1300,   0, -1, -1,  6, 0, 0 },

	/* -- substate 1: the reset.  Nothing branches; run it everywhere --  */
	 { 0, 0, 0, PEER_SILENCE,          0,  1,   -1,  -1, -1, -1,  4, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_2400,  1,  1,   -1,  -1, -1, -1,  4, 0, 0 },
	 { 0, 1, 0, PEER_NOISE,            0,  1, 1300,  -1, -1, -1,  4, 0, 0 },

	/* -- substate 2: both arms of the +0x38 branch --                    */
	 { 0, 0, 0, PEER_NOISE,            0,  2,   -1,  -1, -1, -1,  8, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  2,   99,  -1, -1,  1,  6, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  2,   99,  -1, -1,  0,  6, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_S1,         0,  2,   99,  -1, -1,  7,  6, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_S1,         0,  2,   -1,  -1, -1,  1,  8, 0, 0 },

	/* -- substate 3: same shape as 0, different exit --                  */
	 { 0, 0, 0, PEER_SILENCE,          0,  3,   -1,  -1, -1, -1, 12, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  3,   -1,  -1, -1, -1, 12, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_1200,  1,  3,   -1,  -1, -1, -1, 24, 0, 0 },
	 { 0, 0, 0, PEER_SILENCE,          0,  3,   -1,  60, -1, -1,  6, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_1200,  1,  3,   -1,  60, -1, -1,  6, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  3, 1300,   0, -1, -1,  6, 0, 0 },

	/* -- substates 4, 5, 6: wait, then step the rate --                  */
	 { 0, 0, 0, PEER_NOISE,            0,  4,   -1,  -1, -1, -1,  8, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_1200,  1,  4,  449,  -1, -1, -1,  6, 0, 0 },
	 { 0, 1, 0, PEER_SILENCE,          0,  4,  449,  -1, -1, -1,  6, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  5,   -1,  -1, -1, -1,  8, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_1200,  1,  5,  599,  -1, -1, -1,  6, 0, 0 },
	 { 0, 1, 0, PEER_SILENCE,          0,  5,  599,  -1, -1, -1,  6, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  6,   -1,  -1, -1, -1,  8, 0, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_2400,  1,  6,  799,  -1, -1, -1,  6, 0, 0 },
	 { 0, 1, 0, PEER_SILENCE,          0,  6,  799,  -1, -1, -1,  6, 0, 0 },

	/* -- substate 7: the trained exit, and the two-second give-up --     */
	 { 0, 0, 0, PEER_NOISE,            0,  7,   -1,  -1, -1, -1,  8, 0, 0 },
	 { 0, 0, 0, PEER_SILENCE,          0,  7, 1999,  -1, -1, -1,  6, 0, 0 },
	 { 1, 0, 0, V22_TXDATA_ONES_2400,  1,  7,    0,  -1, -1, -1,400, 1, 0 },
	 { 1, 0, 1, V22_TXDATA_ONES_2400,  1,  7,    0,  -1, -1, -1,400, 1, 0 },
	 { 0, 0, 0, V22_TXDATA_ONES_2400,  1,  7,    0,  -1, -1, -1,400, 1, 0 },
	 { 0, 0, 1, V22_TXDATA_ONES_2400,  1,  7,    0,  -1, -1, -1,400, 1, 0 },

	/*
	 * -- the timeouts ON the boundary.  `ReadGTimer` adds a tick before
	 * the comparison, so a `gtimer` of T - 20 puts the value tested
	 * exactly ON T, where `> T` and `>= T` disagree and nothing else
	 * does.  Without these rows the six comparisons are only ever
	 * evaluated 20 ms clear of their threshold.
	 */
	 { 0, 0, 0, PEER_NOISE,            0,  0, 1280,  -1, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  2,   79,  -1, -1,  1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  3, 1280,   0, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  4,  429,  -1, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  5,  579,  -1, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  6,  779,  -1, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  7, 1979,  -1, -1, -1,  4, 0, 0 },

	/* -- and a selector the table does not cover, which must do nothing  */
	 { 0, 0, 0, PEER_NOISE,            0,  8,   -1,  -1, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0, 31,   -1,  -1, -1, -1,  4, 0, 0 },
	};
	unsigned i;
	long tag = 0;
	long blocks = 0;

	diff_begin("v22_retrain");
	rng_seed(0x51a7c33dUL);

	for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
		blocks += run_scen(ref_v22_retrain, v22_retrain, &tab[i], tag,
				   record_retrain);
		tag++;
	}
	diff_eq_int("blocks driven (%ld)", blocks > 0, 1, blocks);

	return diff_end();
}

/*
 * The remote-loopback sweep.  Substates 1 and 2 both branch on `hdx->r08`
 * clearing 231 ms, which only a peer sending the acknowledgement pattern can
 * reach; substate 2 additionally branches on `hdx->r10`, which is pre-loaded.
 */
static int
run_rmloop2(void)
{
	static const struct scen tab[] = {
	/* rx f14 pr pattern               scr st  gtim  r08 r10 r38 blk 24 rt*/
	/* -- substate 0: one block, then it hands on to 1 --                 */
	 { 0, 0, 0, PEER_SILENCE,          0,  0,   -1,  -1, -1, -1,  6, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  0,   -1,  -1, -1, -1,  6, 0, 0 },
	 { 1, 0, 0, V22_TXDATA_SYMBOL_10,  0,  0,   -1,  -1, -1, -1, 12, 1, 0 },
	 { 1, 0, 1, V22_TXDATA_SYMBOL_10,  0,  0,   -1,  -1, -1, -1, 12, 1, 0 },
	 { 0, 0, 0, V22_TXDATA_SYMBOL_2,   0,  0,   -1,  -1, -1, -1, 12, 0, 1 },

	/* -- substate 1: count the acknowledgement --                        */
	 { 0, 0, 0, PEER_SILENCE,          0,  1,   -1,  -1, -1, -1, 12, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  1,   -1,  -1, -1, -1, 12, 0, 0 },
	 { 1, 0, 0, V22_TXDATA_SYMBOL_10,  0,  1,    0,  -1, -1, -1,300, 1, 0 },
	 { 1, 0, 1, V22_TXDATA_SYMBOL_10,  0,  1,    0,  -1, -1, -1,300, 1, 0 },
	 { 0, 0, 0, V22_TXDATA_SYMBOL_10,  0,  1,    0,  -1, -1, -1,300, 1, 0 },
	 { 0, 0, 0, V22_TXDATA_SYMBOL_2,   0,  1,    0,  -1, -1, -1,300, 0, 1 },
	 { 1, 0, 1, V22_TXDATA_SYMBOL_2,   0,  1,    0,  -1, -1, -1,300, 0, 1 },
	/* the threshold arm without a real pattern, and the give-up          */
	 { 0, 0, 0, PEER_NOISE,            0,  1,    0, 240, -1, -1,  6, 0, 0 },
	 { 0, 0, 0, PEER_SILENCE,          0,  1, 1300,  -1, -1, -1,  6, 0, 0 },
	 { 1, 1, 0, PEER_NOISE,            0,  1, 1300, 240, -1, -1,  6, 0, 0 },

	/* -- substate 2: scrambled ones, both arms of hdx->r10 --            */
	 { 0, 0, 0, PEER_SILENCE,          0,  2,   -1,  -1,  0, -1, 12, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  2,   -1,  -1,  0, -1, 12, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  2,   -1,  -1,  1, -1, 12, 0, 0 },
	/* an acknowledgement still arriving, so the r10 arm is NOT taken     */
	 { 1, 0, 0, V22_TXDATA_SYMBOL_10,  0,  2,    0,  -1,  0, -1,300, 1, 0 },
	 { 1, 0, 1, V22_TXDATA_SYMBOL_10,  0,  2,    0,  -1,  0, -1,300, 1, 0 },
	/* scrambled ones, which is what `Detect_1s` counts, at both rates    */
	 { 1, 0, 1, V22_TXDATA_ONES_2400,  1,  2,    0,  -1,  1, -1,300, 1, 0 },
	 { 0, 0, 0, V22_TXDATA_ONES_2400,  1,  2,    0,  -1,  1, -1,300, 1, 0 },
	 { 0, 0, 0, V22_TXDATA_ONES_1200,  1,  2,    0,  -1,  1, -1,300, 0, 1 },
	 { 0, 0, 1, V22_TXDATA_ONES_1200,  1,  2,    0,  -1,  1, -1,300, 0, 1 },
	/* the two exits, reached by pre-loading                              */
	 { 0, 0, 0, PEER_NOISE,            0,  2,    0, 240,  1, -1,  6, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  2, 1300,   0,  1, -1,  6, 0, 0 },

	/*
	 * -- both thresholds ON their boundary.  `r08` of exactly 231 is
	 * where `> 231` and `>= 231` disagree; `r08` of 40000, which is
	 * -25536 read as a signed short, is where the object's UNSIGNED
	 * `cmpw`/`ja` disagrees with a signed compare.  Neither difference is
	 * reachable by accumulation, so both are pre-loaded.
	 */
	 { 0, 0, 0, PEER_SILENCE,          0,  1,    0, 231, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_SILENCE,          0,  2,    0, 231,  1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_SILENCE,          0,  1,    0,40000, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_SILENCE,          0,  2,    0,40000,  1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  1, 1280,   0, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  2, 1280,   0,  1, -1,  4, 0, 0 },

	/* -- a selector outside 0..2, which must only stamp fp->status --    */
	 { 0, 0, 0, PEER_NOISE,            0,  3,   -1,  -1, -1, -1,  4, 0, 0 },
	 { 0, 0, 0, PEER_NOISE,            0,  9,   -1,  -1, -1, -1,  4, 0, 0 },
	};
	unsigned i;
	long tag = 0;
	long blocks = 0;

	diff_begin("v22_org_rmloop2");
	rng_seed(0x2c9f1e05UL);

	for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
		blocks += run_scen(ref_v22_org_rmloop2, v22_org_rmloop2,
				   &tab[i], tag, record_rmloop2);
		tag++;
	}
	diff_eq_int("blocks driven (%ld)", blocks > 0, 1, blocks);

	return diff_end();
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;
	int i;

	rc |= run_retrain();
	rc |= run_rmloop2();

	diff_begin("v22hdx coverage guards");

	for (i = 0; i < 8; i++)
		diff_eq_int("retrain substate entered (%ld)",
			    saw_case[i] > 0, 1, (long)i);
	/*
	 * Substate 1 writes `r0c` unconditionally and substate 7 never writes
	 * it at all, so those two have only one arm each and a guard on the
	 * other would be a guard that can never pass.
	 */
	for (i = 0; i < 8; i++) {
		if (i != 7)
			diff_eq_int("retrain substate advanced (%ld)",
				    saw_advance[i] > 0, 1, (long)i);
		if (i != 1)
			diff_eq_int("retrain substate stayed (%ld)",
				    saw_stay[i] > 0, 1, (long)i);
	}
	for (i = 0; i < 8; i++) {
		if (i == 1)
			continue;	/* substate 1 has no timeout test */
		diff_eq_int("retrain timeout taken (%ld)",
			    saw_timeout[i] > 0, 1, (long)i);
		diff_eq_int("retrain timeout not taken (%ld)",
			    saw_no_timeout[i] > 0, 1, (long)i);
	}
	diff_eq_int("retrain r08 advanced by a tick (%ld)",
		    saw_r08_grew > 0, 1, saw_r08_grew);
	diff_eq_int("retrain r08 stood still (%ld)",
		    saw_r08_flat > 0, 1, saw_r08_flat);
	diff_eq_int("retrain substate 2 chose 3 (%ld)",
		    saw_case2_to3 > 0, 1, saw_case2_to3);
	diff_eq_int("retrain substate 2 chose 4 (%ld)",
		    saw_case2_to4 > 0, 1, saw_case2_to4);
	diff_eq_int("retrain saw RxTrained2400 (%ld)",
		    saw_trained2400 > 0, 1, saw_trained2400);
	diff_eq_int("retrain selector outside 0..7 (%ld)",
		    saw_default_state > 0, 1, saw_default_state);
	diff_eq_int("retrain sent 1200 ones (%ld)", saw_pattern_1200 > 0, 1,
		    saw_pattern_1200);
	diff_eq_int("retrain sent 2400 ones (%ld)", saw_pattern_2400 > 0, 1,
		    saw_pattern_2400);

	for (i = 0; i < 3; i++)
		diff_eq_int("rmloop2 substate entered (%ld)",
			    rm_case[i] > 0, 1, (long)i);
	diff_eq_int("rmloop2 r08 advanced (%ld)", rm_r08_grew > 0, 1,
		    rm_r08_grew);
	diff_eq_int("rmloop2 r08 stood still (%ld)", rm_r08_flat > 0, 1,
		    rm_r08_flat);
	diff_eq_int("rmloop2 substate 1 cleared 231 ms (%ld)",
		    rm_thresh[1] > 0, 1, rm_thresh[1]);
	diff_eq_int("rmloop2 substate 1 did not (%ld)",
		    rm_no_thresh[1] > 0, 1, rm_no_thresh[1]);
	diff_eq_int("rmloop2 acknowledgement in the same block (%ld)",
		    rm_ack_seen > 0, 1, rm_ack_seen);
	diff_eq_int("rmloop2 no acknowledgement (%ld)",
		    rm_ack_missing > 0, 1, rm_ack_missing);
	diff_eq_int("rmloop2 substate 2 with r10 set (%ld)",
		    rm_r10_preset > 0, 1, rm_r10_preset);
	diff_eq_int("rmloop2 substate 2 with r10 clear (%ld)",
		    rm_r10_clear > 0, 1, rm_r10_clear);
	diff_eq_int("rmloop2 substate 2 cleared 231 ms (%ld)",
		    rm_thresh[2] > 0, 1, rm_thresh[2]);
	diff_eq_int("rmloop2 substate 2 did not (%ld)",
		    rm_no_thresh[2] > 0, 1, rm_no_thresh[2]);
	diff_eq_int("rmloop2 status 6 (%ld)", rm_status6 > 0, 1, rm_status6);
	diff_eq_int("rmloop2 status 7 (%ld)", rm_status7 > 0, 1, rm_status7);
	diff_eq_int("rmloop2 timeout taken (%ld)", rm_timeout > 0, 1,
		    rm_timeout);
	diff_eq_int("rmloop2 timeout not taken (%ld)", rm_no_timeout > 0, 1,
		    rm_no_timeout);
	diff_eq_int("rmloop2 selector outside 0..2 (%ld)",
		    rm_default_state > 0, 1, rm_default_state);
	diff_eq_int("rmloop2 sent 1200 ones (%ld)", rm_pattern_1200 > 0, 1,
		    rm_pattern_1200);
	diff_eq_int("rmloop2 sent 2400 ones (%ld)", rm_pattern_2400 > 0, 1,
		    rm_pattern_2400);

	rc |= diff_end();

	return rc;
}
