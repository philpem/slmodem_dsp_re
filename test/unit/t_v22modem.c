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

#define FRAG	160
#define WORDS	12

static int tx_a[100], tx_b[100];
static int rx_a[100], rx_b[100];
static short out_a[FRAG], out_b[FRAG];
static short in_samples[FRAG];

static void
build_input(int seed)
{
	unsigned lfsr = (unsigned)(0x1ACE + seed * 7919);
	int i;

	for (i = 0; i < FRAG; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		in_samples[i] = (short)((int)(lfsr & 0x1fffu) - 4096);
	}
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

			diff_eq_int("state %ld: return", ra, rb, tag);
			diff_eq_int("state %ld: n_tx", na_tx, nb_tx, tag);
			diff_eq_int("state %ld: n_rx", na_rx, nb_rx, tag);

			for (i = 0; i < FRAG; i++)
				diff_eq_int("tx sample[%ld]", out_a[i],
					    out_b[i], i);
			/*
			 * The WHOLE receive array, not the first `*n_rx`
			 * entries: the count is forced to zero for any status
			 * but 0, so comparing only what it reports would hide
			 * everything the copy-out actually wrote.  That is the
			 * channel F8538 called "a channel the test cannot
			 * see".
			 */
			for (i = 0; i < 100; i++)
				diff_eq_int("rx word[%ld]", rx_a[i], rx_b[i],
					    i);

			compare_graphs(fa, fb, tag);
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
