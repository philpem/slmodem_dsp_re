/*
 * t_v22rate.c -- differential test of V.22's rate setters, receiver reset and
 * receive chain.
 *
 * THE FIXTURE IS A REAL OBJECT, not a poked byte buffer, and it has to be:
 * `DemodDataV22` runs six sub-objects that each need their own heap buffers
 * and their own initialised state, and `ResetRx` re-runs two of those inits
 * with `fresh` = 0, which is only meaningful on a graph that was allocated
 * once already.  So every check here builds TWO graphs with `V22FP_create`
 * -- ours on both sides, because t_v22fpcreate has already established that
 * ours and the blob's agree byte for byte -- drives the reference through one
 * and the reconstruction through the other, and then compares the two graphs
 * entire: the three structs with their pointer fields blanked, plus the
 * twenty-eight heap regions behind those pointers.
 *
 * WHAT BLANKING A POINTER COSTS, AND HOW IT IS PAID BACK.  Two graphs hold
 * two different addresses for the same buffer, so a pointer field cannot be
 * compared across the sides and has to be blanked before `diff_eq_obj` sees
 * it.  Three of the fields these functions WRITE are pointers -- `pps.imap`,
 * `pps.qmap` and `fse.decision` -- and blanking them would leave the whole
 * point of `SetTxRate` and `SetRxRate` checked by nothing.  Each is therefore
 * checked separately against ITS OWN side's symbol: the reference must land
 * on `ref_SMCv22_IMAP_1200BPS` exactly when ours lands on
 * `SMCv22_IMAP_1200BPS`.  That separates a swapped I/Q pair and a swapped
 * rate, neither of which any content comparison could see, because the four
 * maps are permutations of one another.
 *
 * WHY THE RECEIVE CHAIN IS DRIVEN IN A LOOP RATHER THAN ONCE.  Every arm of
 * `DemodDataV22`'s flag logic is gated on state the CALLEES move: the clock
 * loop's `acquiring` starts at 1 and clears when it reaches mode 2, and its
 * `mode` starts at 0.  A single block therefore exercises exactly one arm of
 * each.  The sweep runs 60 blocks per configuration and the guards at the
 * bottom assert that both arms of all four gates were taken.
 *
 * The four configurations are chosen to cover the two gates the OBJECT
 * carries rather than the callees: `dsp->r2e == 2` selects the IIR front end
 * and `hdx->protocol == 0` enables the level check, and `V22FP_create`'s mode and
 * its flags bit 11 set those two independently.
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
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22rate.h"
#include "dsplib/v22txtab.h"

extern void ref_SetTxRate(struct v22fp *fp, short rate);
extern void ref_SetRxRate(struct v22fp *fp, short rate);
extern void ref_ResetRx(struct v22fp *fp);
extern unsigned short ref_DemodDataV22(struct v22fp *fp, short *in,
				       unsigned short *sym,
				       unsigned short count);

/* The blob's own copies of everything these four install a POINTER to. */
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

/*
 * Every heap region the graph owns, in a fixed order, so two graphs can be
 * walked in step.  Same list as t_v22fpcreate.c's, for the same reason.
 */
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

/* Copies with every pointer blanked, so the rest can be compared entire. */
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
	c.pps.imap = NULL;		/* SetTxRate writes it; checked below */
	c.pps.qmap = NULL;		/* likewise                           */
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
	c.fse.decision = NULL;		/* SetRxRate writes it; checked below */
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

/*
 * One check per buffer, naming the first differing byte.  A per-byte
 * diff_eq_int would put tens of thousands of checks in the ledger.
 */
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
 * `mine` is the graph the reconstruction ran on, `theirs` the one the blob
 * ran on.  Everything that is not a pointer is compared directly; the bytes
 * behind every pointer are compared through the region list.
 */
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
 * Which of the four constellation maps a pointer is.  BOTH sides' symbols map
 * onto the same id, which is what lets one function serve both graphs: the
 * two copies of a table are two addresses for the same thing, and what has to
 * agree is which table was chosen, not where it lives.
 *
 * The id has to cover our symbols as well as the blob's even on the reference
 * graph, because a selector the object IGNORES leaves the field holding what
 * `V22FP_create` -- ours, on both graphs -- put there.  Naming only the blob's
 * copies made every ignored selector read as a difference; the ids are shared
 * so that "nothing was written" compares equal and "the wrong one was written"
 * still does not.  Zero means neither side's table, i.e. a field nobody set.
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

static long tx_selected, tx_ignored;
static long rx_selected, rx_ignored;

static int
run_settxrate(void)
{
	static const short rates[] = { 0, 1, 2, 3, -1, -32768, 32767 };
	long tag = 0;
	int mode, f14;
	unsigned ri;

	diff_begin("SetTxRate");

	for (mode = 0; mode <= 1; mode++)
		for (f14 = 0; f14 <= 1; f14++)
			for (ri = 0; ri < sizeof(rates) / sizeof(rates[0]);
			     ri++) {
				struct v22fp_cfg cfg = base_cfg(mode, f14);
				short r = rates[ri];
				struct v22fp *a = V22FP_create(0, &cfg);
				struct v22fp *b = V22FP_create(0, &cfg);

				ref_SetTxRate(a, r);
				SetTxRate(b, r);

				compare_graphs(b, a, tag);
				diff_eq_int("pps.imap identity, case %ld",
					    map_id(b->dsp->pps.imap),
					    map_id(a->dsp->pps.imap), tag);
				diff_eq_int("pps.qmap identity, case %ld",
					    map_id(b->dsp->pps.qmap),
					    map_id(a->dsp->pps.qmap), tag);

				/*
				 * `V22FP_create` leaves the 1200 pair in
				 * place, so "selected" means the 2400 pair
				 * arrived and "ignored" means the 1200 pair
				 * survived a selector that is neither rate.
				 */
				if (map_id(a->dsp->pps.imap) == 3)
					tx_selected++;
				else if (r != V22_RATE_1200)
					tx_ignored++;

				V22FP_delete(a);
				V22FP_delete(b);
				tag++;
			}

	return diff_end();
}

static int
run_setrxrate(void)
{
	static const short rates[] = { 0, 1, 2, 3, -1, -32768, 32767 };
	long tag = 0;
	int mode, f14;
	unsigned ri;

	diff_begin("SetRxRate");

	for (mode = 0; mode <= 1; mode++)
		for (f14 = 0; f14 <= 1; f14++)
			for (ri = 0; ri < sizeof(rates) / sizeof(rates[0]);
			     ri++) {
				struct v22fp_cfg cfg = base_cfg(mode, f14);
				short r = rates[ri];
				struct v22fp *a = V22FP_create(0, &cfg);
				struct v22fp *b = V22FP_create(0, &cfg);

				ref_SetRxRate(a, r);
				SetRxRate(b, r);

				compare_graphs(b, a, tag);
				diff_eq_int("fse.decision identity, case %ld",
					    dec_id(b->dsp->fse.decision),
					    dec_id(a->dsp->fse.decision),
					    tag);

				/*
				 * `V22FP_create` installs decision12, so an
				 * "ignored" selector is only visible as the
				 * ABSENCE of a change to 24 -- which is why
				 * both rate arms are swept and guarded below.
				 */
				if (r == V22_RATE_2400)
					rx_selected++;
				else if (r != V22_RATE_1200)
					rx_ignored++;

				V22FP_delete(a);
				V22FP_delete(b);
				tag++;
			}

	return diff_end();
}

/* ------------------------------------------------------------------------ */

#define BLOCK	160
#define INBUF	512
#define SYMBUF	256

/*
 * Gate coverage, read off the REFERENCE graph so the counts describe the
 * blob's behaviour and not ours.
 */
static long saw_iir, saw_no_iir;
static long saw_level_check, saw_level_skipped;
static long saw_disconnect, saw_no_disconnect;
static long saw_acquiring, saw_acquired;
static long saw_sre_mode0, saw_sre_moden;
static long saw_symbols;

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

static int
run_demod(void)
{
	static short in_a[INBUF], in_b[INBUF];
	static unsigned short sym_a[SYMBUF], sym_b[SYMBUF];
	static short src[BLOCK];
	long tag = 0;
	int mode, f14, frozen, blk;

	diff_begin("DemodDataV22");
	rng_seed(0x0d3e0dabUL);

	for (mode = 0; mode <= 1; mode++)
	    for (f14 = 0; f14 <= 1; f14++)
		for (frozen = 0; frozen <= 1; frozen++) {
			struct v22fp_cfg cfg = base_cfg(mode, f14);
			struct v22fp *a = V22FP_create(0, &cfg);
			struct v22fp *b = V22FP_create(0, &cfg);

			if (a->dsp->r2e == V22_FRONTEND_IIR)
				saw_iir++;
			else
				saw_no_iir++;
			if (a->hdx->protocol == 0)
				saw_level_check++;
			else
				saw_level_skipped++;

			/*
			 * THE FROZEN PASS, AND WHY IT IS NEEDED.  Two of
			 * DemodDataV22's four gates read `sre.acquiring` and
			 * `sre.mode`, and the clock loop only clears
			 * `acquiring` when it reaches mode 2 -- which needs a
			 * real phase-locked V.22 carrier, not the noise this
			 * test can generate.  Left alone, the sweep would
			 * exercise one arm of each and the guards below would
			 * say so.
			 *
			 * `dsp->r04` is what DemodDataV22 ANDs into
			 * `sre.adapt`, and `v22_sre.c`'s update returns
			 * before touching either field when `adapt` is zero.
			 * So clearing r04 freezes the two state words, and
			 * the pass below then drives them through all six
			 * combinations directly.  Both graphs are poked
			 * identically, so the comparison is unaffected.
			 */
			if (frozen) {
				a->dsp->r04 = 0;
				b->dsp->r04 = 0;
			}

			for (blk = 0; blk < 60; blk++) {
				unsigned short ra, rb;
				int amp;

				/*
				 * Silence for the first four blocks and the
				 * last four, so the level check gets to fail
				 * as well as pass; a loud carrier in between.
				 */
				amp = (blk < 4 || blk >= 56) ? 0 : 12000;
				fill_block(src, BLOCK, amp);

				if (frozen) {
					short m = (short)(blk % 3);
					int acq = (blk / 3) & 1;
					int eq = (blk / 6) & 1;

					a->dsp->sre.mode = m;
					b->dsp->sre.mode = m;
					a->dsp->sre.acquiring = acq;
					b->dsp->sre.acquiring = acq;
					a->dsp->eq_adapt = eq;
					b->dsp->eq_adapt = eq;
					a->dsp->r0c = 1 - eq;
					b->dsp->r0c = 1 - eq;
				}

				memset(in_a, 0, sizeof(in_a));
				memset(in_b, 0, sizeof(in_b));
				memcpy(in_a, src, sizeof(src));
				memcpy(in_b, src, sizeof(src));
				memset(sym_a, 0x5a, sizeof(sym_a));
				memset(sym_b, 0x5a, sizeof(sym_b));

				ra = ref_DemodDataV22(a, in_a, sym_a, BLOCK);
				rb = DemodDataV22(b, in_b, sym_b, BLOCK);

				diff_eq_int("return, case %ld", rb, ra, tag);
				cmp_raw("in buffer", in_b, in_a, sizeof(in_a),
					tag);
				cmp_raw("symbol buffer", sym_b, sym_a,
					sizeof(sym_a), tag);
				compare_graphs(b, a, tag);

				/*
				 * Read AFTER the call: the flag logic runs on
				 * what the clock recovery left behind, and
				 * nothing between there and the return moves
				 * either field.
				 */
				if (a->dsp->sre.acquiring)
					saw_acquiring++;
				else
					saw_acquired++;
				if (a->dsp->sre.mode == 0)
					saw_sre_mode0++;
				else
					saw_sre_moden++;

				if (ra == 0 && a->dsp->sre.active == 0)
					saw_disconnect++;
				else
					saw_no_disconnect++;
				if (ra != 0)
					saw_symbols++;
				tag++;
			}

			V22FP_delete(a);
			V22FP_delete(b);
		}

	return diff_end();
}

/* ------------------------------------------------------------------------ */

/*
 * ResetRx re-runs two inits with `fresh` = 0, so it is only worth anything on
 * a graph whose clock loop and equaliser have already moved.  Both graphs are
 * driven through the SAME reference chain first, which leaves them identical
 * and dirty, and only then does one side get ref_ResetRx and the other ours.
 */
static int
run_resetrx(void)
{
	static short in_a[INBUF], in_b[INBUF];
	static unsigned short sym_a[SYMBUF], sym_b[SYMBUF];
	static short src[BLOCK];
	long tag = 0;
	int mode, f14, blk, warm;

	diff_begin("ResetRx");
	rng_seed(0x2e5e7abcUL);

	for (mode = 0; mode <= 1; mode++)
		for (f14 = 0; f14 <= 1; f14++)
			for (warm = 0; warm <= 40; warm += 10) {
				struct v22fp_cfg cfg = base_cfg(mode, f14);
				struct v22fp *a = V22FP_create(0, &cfg);
				struct v22fp *b = V22FP_create(0, &cfg);

				for (blk = 0; blk < warm; blk++) {
					fill_block(src, BLOCK, 12000);
					memset(in_a, 0, sizeof(in_a));
					memset(in_b, 0, sizeof(in_b));
					memcpy(in_a, src, sizeof(src));
					memcpy(in_b, src, sizeof(src));
					ref_DemodDataV22(a, in_a, sym_a, BLOCK);
					ref_DemodDataV22(b, in_b, sym_b, BLOCK);
				}

				/* Identical and dirty before the split. */
				compare_graphs(b, a, tag);

				ref_ResetRx(a);
				ResetRx(b);
				compare_graphs(b, a, tag);

				V22FP_delete(a);
				V22FP_delete(b);
				tag++;
			}

	return diff_end();
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= run_settxrate();
	rc |= run_setrxrate();
	rc |= run_demod();
	rc |= run_resetrx();

	diff_begin("v22rate coverage guards");
	diff_eq_int("SetTxRate selected a map (%ld)", tx_selected > 0, 1, 0);
	diff_eq_int("SetTxRate ignored a selector (%ld)", tx_ignored > 0, 1, 0);
	diff_eq_int("SetRxRate selected 2400 (%ld)", rx_selected > 0, 1, 0);
	diff_eq_int("SetRxRate ignored a selector (%ld)", rx_ignored > 0, 1, 0);
	diff_eq_int("IIR front end configured (%ld)", saw_iir > 0, 1, 0);
	diff_eq_int("IIR front end bypassed (%ld)", saw_no_iir > 0, 1, 0);
	diff_eq_int("level check enabled (%ld)", saw_level_check > 0, 1, 0);
	diff_eq_int("level check skipped (%ld)", saw_level_skipped > 0, 1, 0);
	diff_eq_int("disconnect return taken (%ld)", saw_disconnect > 0, 1, 0);
	diff_eq_int("disconnect return not taken (%ld)", saw_no_disconnect > 0,
		    1, 0);
	diff_eq_int("clock loop still acquiring (%ld)", saw_acquiring > 0, 1, 0);
	diff_eq_int("clock loop acquired (%ld)", saw_acquired > 0, 1, 0);
	diff_eq_int("sre.mode zero (%ld)", saw_sre_mode0 > 0, 1, 0);
	diff_eq_int("sre.mode non-zero (%ld)", saw_sre_moden > 0, 1, 0);
	diff_eq_int("symbols were produced (%ld)", saw_symbols > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
