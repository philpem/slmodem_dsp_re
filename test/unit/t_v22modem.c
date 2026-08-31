/*
 * t_v22modem.c -- V22FP_modem driven directly, over every protocol state.
 *
 *   V22FP_modem   .text 0x0887b0  346 bytes
 *   V22_PROTOCOL  .rodata 0x008544  28 bytes, LOCAL
 *
 * t_v22dp.c drives this function the way service does -- through
 * `v22_process`, over a cross-connected link -- and a live link only ever
 * visits the states the handshake reaches, in the order it reaches them.
 * This file is the other half: it PUTS the machine into each of the seven
 * `V22_PROTOCOL` states and each of the eight sub-states `hdx->connect_substate` takes,
 * and drives one block from each.  Fifty-six starting points, none of which
 * a link would produce on demand.
 *
 * ---------------------------------------------------------------------------
 * THIS IS THE EXPERIMENT FINDING F8538 DECLINED ON, RE-RUN
 *
 * The previous attempt reported a divergence here -- ours holding
 * V22_CLAMP_VALUE in the receive-word output where the reference held 0, with
 * the return, both counts, the transmit samples, all three structs and all
 * twenty-eight heap regions agreeing over 18,018 checks -- and correctly
 * declined the whole wave rather than commit an unexplained difference or
 * scope the sweep away from the state that failed.
 *
 * It is reproduced here rather than replaced, because a test written to avoid
 * a failure is worth nothing.  What it compares is a superset of what that
 * one did: the return, both counts, the transmit block, the receive words
 * over the count the object reports AND over the whole array beyond it, the
 * three structs, and -- the channel F8538 named as the prime suspect and
 * could not see -- the blob's own `rx_in_internal` staging buffer, which is
 * the one of the three `.bss` statics whose name is unique in the object and
 * therefore the only one `tools/symmap.py` can alias.
 *
 * THE BUFFERS PERSIST ACROSS THE SWEEP, deliberately.  They are file-static
 * on both sides, so state (a) written by one iteration and read by the next
 * is exactly the sharing the object has, and a comparison that reset them
 * between cases would be a weaker test, not a cleaner one.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22txtab.h"

/* The blob's copies of everything SetTxRate / SetRxRate can install. */
extern const short ref_SMCv22_IMAP_1200BPS[16];
extern const short ref_SMCv22_QMAP_1200BPS[16];
extern const short ref_SMCv22_IMAP_2400BPS[16];
extern const short ref_SMCv22_QMAP_2400BPS[16];
extern unsigned short ref_FSEv22_decision12(struct v22_fse *state,
					    short *angle, short *mag);
extern unsigned short ref_FSEv22_decision24(struct v22_fse *state,
					    short *angle, short *mag);

extern struct v22fp *ref_V22FP_create(struct v22fp *fp,
				      const struct v22fp_cfg *cfg);
extern void ref_V22FP_delete(struct v22fp *fp);
extern int ref_V22FP_modem(struct v22fp *fp, const int *tx_bits, short *tx_out,
			   const short *rx_in, int *rx_bits, int *n_tx,
			   int *n_rx);

/*
 * The blob's receive staging buffer, 160 shorts at `.bss` + 0x560.  Unique in
 * the object, so it has a `ref_` alias; `tx_in_internal` and
 * `rx_out_internal` each appear three times and cannot be aliased at all.
 */
extern short ref_rx_in_internal[160];

/*
 * ---------------------------------------------------------------------------
 * Graph comparison, taken unchanged from t_v22ans.c: the three structs with
 * every pointer field blanked, the four map/slicer pointers compared by
 * IDENTITY rather than by value, and all twenty-eight heap regions compared
 * byte for byte.  Two graphs live at two addresses and always will, so a raw
 * `diff_eq_obj` over the structs reports its own artefact -- which is what
 * the first draft of this file did, 168 checks of pure pointer difference.
 */
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

static int
dec_id(v22_fse_decision d)
{
	if (d == FSEv22_decision12 || d == ref_FSEv22_decision12)
		return 1;
	if (d == FSEv22_decision24 || d == ref_FSEv22_decision24)
		return 2;
	return 0;
}

/*
 * `what` names the CASE.  Without it a struct or region difference reports
 * only which field moved, over fifty-six starting states that share one
 * `diff_begin` group and a ten-line cap between them.
 */
static void
compare_graphs_named(const char *what, struct v22fp *mine,
		     struct v22fp *theirs, long tag)
{
	char lbl[96];
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

	snprintf(lbl, sizeof(lbl), "%s object", what);
	diff_eq_obj(lbl, struct v22fp, &oa, &ob, tag);
	snprintf(lbl, sizeof(lbl), "%s hdx", what);
	diff_eq_obj(lbl, struct v22fp_hdx, &ha, &hb, tag);
	snprintf(lbl, sizeof(lbl), "%s dsp", what);
	diff_eq_obj(lbl, struct v22fp_dsp, &da, &db, tag);
	snprintf(lbl, sizeof(lbl), "%s tone", what);
	diff_eq_obj(lbl, struct fpm_tone, &ta, &tb, tag);

	snprintf(lbl, sizeof(lbl), "%s pps.imap identity (%%ld)", what);
	diff_eq_int(lbl, map_id(mine->dsp->pps.imap),
		    map_id(theirs->dsp->pps.imap), tag);
	snprintf(lbl, sizeof(lbl), "%s pps.qmap identity (%%ld)", what);
	diff_eq_int(lbl, map_id(mine->dsp->pps.qmap),
		    map_id(theirs->dsp->pps.qmap), tag);
	snprintf(lbl, sizeof(lbl), "%s fse.decision identity (%%ld)", what);
	diff_eq_int(lbl, dec_id(mine->dsp->fse.decision),
		    dec_id(theirs->dsp->fse.decision), tag);

	n = regions_of(mine, ra);
	m = regions_of(theirs, rb);
	snprintf(lbl, sizeof(lbl), "%s same region count (%%ld)", what);
	diff_eq_int(lbl, n, m, tag);
	for (i = 0; i < n && i < m; i++) {
		snprintf(lbl, sizeof(lbl), "%s %s", what, ra[i].name);
		cmp_raw(lbl, ra[i].p, rb[i].p, ra[i].n, tag);
	}
}

#define FRAG	160
#define WORDS	12

/*
 * GUARD ELEMENTS PAST EVERY BUFFER THE CALL WRITES.  `V22FP_modem` scales
 * exactly V22_TX_BLOCK transmit samples and copies exactly `*n_rx` receive
 * words, so nothing may touch the tail; the two `.bss` staging buffers are
 * laid out differently in our object and in the blob's, so an overrun lands
 * somewhere different on each side and would read as a divergence with no
 * cause visible in the source.  Poisoned with the rest and counted after
 * every call, on both sides.
 */
#define GUARD	16

/*
 * ---------------------------------------------------------------------------
 * THE RECEIVE DESTINATION IS FOUR TIMES THE INPUT BLOCK, AND THAT IS NOT
 * SLACK -- IT IS WHAT THE OBJECT'S OWN COPY-OUT CAN DEMAND (finding F8609).
 *
 * `V22FP_modem`'s copy-out is `for (i = 0; i < *n_rx; i++) rx_bits[i] = ...`
 * with NO clamp -- the object's loop at 0x888a7 re-reads `*prxcount` and
 * compares, and there is no `cmp $0x64` anywhere in it.  `*n_rx` goes IN as
 * the input SAMPLE count and is supposed to come back as a SYMBOL count, so
 * a handler arm that returns without touching it leaves 160 there and the
 * copy-out writes 160 ints.
 *
 * Nineteen of the fifty-six cases below do exactly that, on both sides, and
 * with `int rx[100]` the two overruns landed 240 bytes outside the array --
 * in whatever the compiler happened to put next.  That is how a compiler
 * came to decide this test's verdict.
 */
#define RXBUF	(4 * FRAG)

/*
 * How much of the destination is backed by real storage in the object.
 * `rx_out_internal` is 100 entries (`nm`: 0xc8 bytes at .bss 0x480), so
 * anything the copy-out writes at index 100 or above came from a read PAST
 * that array, and its value is whatever the `.bss` neighbour holds.  Two
 * different objects have two different neighbours, necessarily and for ever,
 * so those entries are compared by nothing -- the same rule CLAUDE.md gives
 * for two heap pointers that hold two different addresses.
 */
#define RXOUT_ENTRIES	100

static int tx_a[100], tx_b[100];
static int rx_a[RXBUF + GUARD], rx_b[RXBUF + GUARD];
static short out_a[FRAG + GUARD], out_b[FRAG + GUARD];
static short in_samples[FRAG + GUARD];

/* How many of the GUARD elements past `n` are no longer the poison. */
static int
guard_short(const short *p, int n, short poison)
{
	int i, bad = 0;

	for (i = n; i < n + GUARD; i++)
		if (p[i] != poison)
			bad++;
	return bad;
}

static int
guard_int(const int *p, int n, int poison)
{
	int i, bad = 0;

	for (i = n; i < n + GUARD; i++)
		if (p[i] != poison)
			bad++;
	return bad;
}

static void
build_input(int seed)
{
	unsigned lfsr = (unsigned)(0x1ACE + seed * 7919);
	int i;

	for (i = 0; i < FRAG; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		in_samples[i] = (short)((int)(lfsr & 0x1fffu) - 4096);
	}
	/* The input's own guard, repoisoned with the samples it follows. */
	for (i = FRAG; i < FRAG + GUARD; i++)
		in_samples[i] = 0x2d2d;
	for (i = 0; i < 100; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		tx_a[i] = tx_b[i] = (int)(lfsr & 0x0fu);
	}
}

int
main(void)
{
	struct v22fp_cfg cfg;
	struct v22fp *fa, *fb;
	int rc = 0;
	int state, sub, i;
	int cases = 0;
	int txdiff, rxdiff, first_tx, over_a, over_b;
	char lbl[80];

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = 0;
	cfg.rate = 0;			/* V.22bis, 2400 */
	cfg.f08 = 60000;
	cfg.f0c = 0;
	cfg.f10 = 700;
	cfg.f14 = 0;
	cfg.f18 = 1;

	diff_begin("V22FP_modem: two graphs");
	fb = ref_V22FP_create(0, &cfg);
	fa = V22FP_create(0, &cfg);
	diff_eq_int("both built (%ld)", (long)(fa != 0 && fb != 0), 1, 0);
	if (fa == 0 || fb == 0)
		return diff_end();
	rc |= diff_end();

	diff_begin("V22FP_modem over every V22_PROTOCOL state");
	for (state = 0; state < 7; state++) {
		for (sub = 0; sub < 8; sub++) {
			int na_tx = WORDS, nb_tx = WORDS;
			int na_rx = FRAG, nb_rx = FRAG;
			int ra, rb;
			long tag = (long)(state * 8 + sub);

			build_input(state * 8 + sub);

			fa->hdx->protocol = (short)state;
			fb->hdx->protocol = (short)state;
			fa->hdx->connect_substate = (short)sub;
			fb->hdx->connect_substate = (short)sub;

			memset(out_a, 0x33, sizeof(out_a));
			memset(out_b, 0x33, sizeof(out_b));
			memset(rx_a, 0x5a, sizeof(rx_a));
			memset(rx_b, 0x5a, sizeof(rx_b));

			rb = ref_V22FP_modem(fb, tx_b, out_b, in_samples, rx_b,
					     &nb_tx, &nb_rx);
			ra = V22FP_modem(fa, tx_a, out_a, in_samples, rx_a,
					 &na_tx, &na_rx);

			/*
			 * A PER-CASE SUMMARY LINE, PRINTED WHATEVER HAPPENS.
			 * `diff_eq_int` caps its report at ten lines per
			 * group, so on a wide failure the log names ten
			 * sample indices and NOT ONE of the fifty-six cases
			 * they came from -- which is exactly what the first
			 * period run produced (finding F8607).  This costs 56
			 * lines and turns "3,192 checks failed" into a map.
			 */
			txdiff = 0;
			first_tx = -1;
			for (i = 0; i < FRAG; i++)
				if (out_a[i] != out_b[i]) {
					if (first_tx < 0)
						first_tx = i;
					txdiff++;
				}
			rxdiff = 0;
			for (i = 0; i < RXOUT_ENTRIES; i++)
				if (rx_a[i] != rx_b[i])
					rxdiff++;
			/*
			 * How far past `rx_out_internal`'s hundred entries the
			 * object's own copy-out ran.  This is the measurement
			 * behind F8609: an untouched `*rxcount` is still the
			 * INPUT SAMPLE COUNT, so the expected reading is
			 * 160 - 100 = 60, on both sides.
			 */
			over_a = over_b = 0;
			for (i = 0; i < RXBUF; i++) {
				if (rx_a[i] != 0x5a5a5a5a)
					over_a++;
				if (rx_b[i] != 0x5a5a5a5a)
					over_b++;
			}
			/*
			 * `tx[0]` is printed on BOTH sides whether or not
			 * they differ, and it is the measurement that says
			 * WHICH side moved when a compiler changes: 5205 is
			 * the untouched poison scaled by the gain
			 * (`(0x3333 * 13014) >> 15`), so a case reading
			 * 5205/5205 is one where NEITHER handler wrote the
			 * transmit block.  The blob is fixed code, so if the
			 * REFERENCE column ever moves between two builds of
			 * ours, what changed is the input we hand it.
			 */
			printf("  st %d sub %d: ret %d/%d n_tx %d/%d "
			       "n_rx %d/%d fpst %u/%u tx[0] %d/%d "
			       "guard tx %d/%d rx %d/%d in %d wrote %d/%d "
			       "alloc live %d bad %d ovf %d  txdiff %d "
			       "(first %d: %d/%d) rxdiff %d\n",
			       state, sub, ra, rb, na_tx, nb_tx, na_rx, nb_rx,
			       (unsigned)fa->status, (unsigned)fb->status,
			       out_a[0], out_b[0],
			       guard_short(out_a, FRAG, 0x3333),
			       guard_short(out_b, FRAG, 0x3333),
			       guard_int(rx_a, RXBUF, 0x5a5a5a5a),
			       guard_int(rx_b, RXBUF, 0x5a5a5a5a),
			       guard_short(in_samples, FRAG, 0x2d2d),
			       over_a, over_b,
			       harness_alloc.live,
			       harness_alloc.bad_free, harness_alloc.overflow,
			       txdiff, first_tx,
			       first_tx < 0 ? 0 : out_a[first_tx],
			       first_tx < 0 ? 0 : out_b[first_tx], rxdiff);

			diff_eq_int("st %ld: nothing wrote past the tx block",
				    guard_short(out_a, FRAG, 0x3333)
				    + guard_short(out_b, FRAG, 0x3333), 0, tag);
			diff_eq_int("st %ld: nothing wrote past the rx array",
				    guard_int(rx_a, RXBUF, 0x5a5a5a5a)
				    + guard_int(rx_b, RXBUF, 0x5a5a5a5a), 0, tag);
			diff_eq_int("st %ld: nothing wrote past the input",
				    guard_short(in_samples, FRAG, 0x2d2d), 0,
				    tag);

			snprintf(lbl, sizeof(lbl),
				 "st %d sub %d: return (%%ld)", state, sub);
			diff_eq_int(lbl, ra, rb, tag);
			snprintf(lbl, sizeof(lbl),
				 "st %d sub %d: n_tx (%%ld)", state, sub);
			diff_eq_int(lbl, na_tx, nb_tx, tag);
			snprintf(lbl, sizeof(lbl),
				 "st %d sub %d: n_rx (%%ld)", state, sub);
			diff_eq_int(lbl, na_rx, nb_rx, tag);

			snprintf(lbl, sizeof(lbl),
				 "st %d sub %d: tx sample[%%ld]", state, sub);
			for (i = 0; i < FRAG; i++)
				diff_eq_int(lbl, out_a[i], out_b[i], i);
			/*
			 * The WHOLE receive array, not the first `*n_rx`
			 * entries: the count is forced to zero for any status
			 * but 0, so comparing only what it reports would hide
			 * everything the copy-out actually wrote.  That is the
			 * channel F8538 called "a channel the test cannot
			 * see".
			 */
			snprintf(lbl, sizeof(lbl),
				 "st %d sub %d: rx word[%%ld]", state, sub);
			for (i = 0; i < RXOUT_ENTRIES; i++)
				diff_eq_int(lbl, rx_a[i], rx_b[i], i);

			snprintf(lbl, sizeof(lbl), "st %d sub %d", state, sub);
			compare_graphs_named(lbl, fa, fb, tag);
			cases++;
		}
	}
	printf("  %d starting states driven\n", cases);
	diff_eq_int("all fifty-six ran (%ld)", cases, 56, 0);
	rc |= diff_end();

	V22FP_delete(fa);
	ref_V22FP_delete(fb);
	return rc;
}
