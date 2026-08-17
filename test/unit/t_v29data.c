/*
 * t_v29data.c -- differential test of V.29's transmit no-carrier leaf.
 *
 * `TxNoCarrierV29` is a wrapper: it zeroes both rails of the shared ring,
 * `count` slots' worth, and runs the shaper.  A wrapper has almost no
 * arithmetic of its own, so agreement over random inputs proves little; what
 * can be wrong is WHICH sub-object and WHICH FORM, and a wrong offset agrees
 * with the right one over every input unless the fixture makes the two
 * objects behave differently.
 *
 * So every check is built around a NAMED WRONG READING and the count of
 * trials that SEPARATE it is asserted non-zero at the end.  The readings:
 *
 *   - the SYMBOL FORM rather than the rails, which is V.17's version of this
 *     function.  This is the reading the two functions' near-identical shape
 *     invites, and it is the one this file exists to exclude: the fixture's
 *     `sym` array is filled with indices that map to loud constellation
 *     points, so a shaper reading `sym` and a shaper reading zeroed rails
 *     produce visibly different samples.  Both are evaluated for real, with
 *     the blob's own `FPM_PPS_filter` on each side.
 *   - only one rail zeroed.  Under `cfg.mapped` clear the shaper differences
 *     the two rails, so leaving `q` stale is not symmetric and shows up in
 *     the samples.
 *   - the cursor wrapped at `len - 1`, or not written back.
 *   - `(short)(widx + 1)` read as `widx + 1`.  Reached by SEEDING the cursor
 *     at 32767 over rails with 32,768 shorts of headroom below the pointer
 *     the ring holds; a stimulus sweep never gets there.
 *   - the second argument used rather than ignored.
 *
 * `cfg.mapped` IS CLEAR HERE and set in t_v17data.c, which is the fixture
 * half of finding 3641: the two functions are tested against the two
 * configurations their own constructors build, not against one shared one.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v29data.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

extern unsigned short ref_TxNoCarrierV29(void *modem,
					 const unsigned short *data,
					 short *out, unsigned short count);
extern void ref_FPM_PPS_init(void *state, const void *cfg, int fresh);
extern unsigned short ref_FPM_PPS_filter(void *state, void *src, short *out,
					 unsigned short count);

#define OBJ_SIZE	0x40
#define FP_SIZE		0xc0
#define RING_LEN	50
#define MAP_LEN		256
#define PPS_PHASES	10
#define PPS_COEFFS	120
#define PPS_TAPS	(PPS_COEFFS / PPS_PHASES)

#define NOUT		4096
#define OMARK		0x5ead
#define NDATA		256

#define PPS_HIST_LO	(V29FP_PPS + 0x30)
#define PPS_HIST_HI	(V29FP_PPS + 0x38)
#define RING_PTR_LO	(V29FP_SMC_RING + 0x00)
#define RING_PTR_HI	(V29FP_SMC_RING + 0x0c)

static short coeff_i[PPS_COEFFS], coeff_q[PPS_COEFFS];
static short imap[MAP_LEN], qmap[MAP_LEN];

static unsigned rng_state;

static void
rng_seed(unsigned s)
{
	rng_state = s ? s : 1u;
}

static unsigned
rng_next(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

/*
 * Distinct everywhere, and no two banks agreeing anywhere: finding 3574 is
 * three mutations that survived a degenerate table.
 */
static void
build_tables(void)
{
	int i;

	for (i = 0; i < PPS_COEFFS; i++) {
		coeff_i[i] = (short)(((i * 613) % 4001) - 2000);
		coeff_q[i] = (short)(((i * 947) % 4001) - 2100);
	}
	for (i = 0; i < MAP_LEN; i++) {
		imap[i] = (short)(((i * 271) % 6007) - 3000);
		qmap[i] = (short)(((i * 419) % 6007) - 3100);
	}
}

/* --------------------------------------------------------------------- */

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	fp[FP_SIZE];
	short		sym[RING_LEN];
	short		ri[RING_LEN];
	short		rq[RING_LEN];
	double		align;
};

static struct fix ma, mb, mc;

static struct fpm_smc_ring *
ring_of(struct fix *f)
{
	return (struct fpm_smc_ring *)(void *)(f->fp + V29FP_SMC_RING);
}

static struct fpm_pps *
pps_of(struct fix *f)
{
	return (struct fpm_pps *)(void *)(f->fp + V29FP_PPS);
}

static void
put_ptr(unsigned char *p, int off, void *v)
{
	*(void **)(void *)(p + off) = v;
}

static void
put_short(unsigned char *p, int off, short v)
{
	*(short *)(void *)(p + off) = v;
}

/*
 * The shaper's phase step, which every trial but one leaves at V29TX_create's
 * 3.  The exception is the count-above-32767 trial: at a step of 3 the shaper
 * makes ten samples per three symbols, so 40,000 symbols would be 133,000
 * samples and the `unsigned short` return would wrap.  At a step equal to the
 * phase count every output consumes a symbol, so the sample count equals the
 * symbol count and the corner is affordable.  The function under test never
 * reads this field; it only decides how much output the trial has to hold.
 */
static short cfg_step = 3;

static void
make_cfg(struct fpm_pps_cfg *c, int mapped)
{
	memset(c, 0, sizeof(*c));
	c->phases = PPS_PHASES;
	c->step = cfg_step;
	c->mapped = mapped;	/* V29TX_create clears this; see v29data.h */
	c->scale = 32767;
	c->step_adj = 0;
	c->imap = imap;
	c->qmap = qmap;
	c->coeff_i = coeff_i;
	c->coeff_q = coeff_q;
	c->coeffs = PPS_COEFFS;
}

/*
 * `mapped` is a parameter only so that the "V.17's form" wrong reading can be
 * evaluated against the configuration that would make it meaningful.  Every
 * trial of the function itself uses 0, which is what V29TX_create builds.
 */
static void
fixture(struct fix *f, unsigned seed, short widx, short ridx, int mapped,
	short *rail_i, short *rail_q, short len)
{
	struct fpm_pps_cfg cfg;
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < FP_SIZE; i++)
		f->fp[i] = (unsigned char)rng_next();
	for (i = 0; i < RING_LEN; i++) {
		f->sym[i] = (short)((rng_next() % 200u) + 20u);
		f->ri[i] = (short)(rng_next() % 20001u) - 10000;
		f->rq[i] = (short)(rng_next() % 20001u) - 10000;
	}

	put_ptr(f->obj, V29TX_OBJ_FP, f->fp);

	put_ptr(f->fp, V29FP_SMC_RING + 0x00, rail_i ? rail_i : f->ri);
	put_ptr(f->fp, V29FP_SMC_RING + 0x04, rail_q ? rail_q : f->rq);
	put_ptr(f->fp, V29FP_SMC_RING + 0x08, f->sym);
	put_short(f->fp, V29FP_SMC_RING + 0x0c, widx);
	put_short(f->fp, V29FP_SMC_RING + 0x0e, ridx);
	put_short(f->fp, V29FP_SMC_RING + 0x10, len ? len : (short)RING_LEN);

	make_cfg(&cfg, mapped);
	ref_FPM_PPS_init(f->fp + V29FP_PPS, &cfg, 1);
}

static int
skip_fp(int off)
{
	if (off >= RING_PTR_LO && off < RING_PTR_HI)
		return 1;
	if (off >= PPS_HIST_LO && off < PPS_HIST_HI)
		return 1;
	return 0;
}

static long
fp_first_diff(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < FP_SIZE; i++) {
		if (skip_fp(i))
			continue;
		if (a->fp[i] != b->fp[i])
			return i;
	}
	return -1;
}

static long
obj_first_diff(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < OBJ_SIZE; i++) {
		if (i >= V29TX_OBJ_FP
		    && i < V29TX_OBJ_FP + (int)sizeof(void *))
			continue;
		if (a->obj[i] != b->obj[i])
			return i;
	}
	return -1;
}

/* --------------------------------------------------------------------- */

static short oa[NOUT], ob[NOUT], oc[NOUT];
static unsigned short mdata[NDATA];

static long tx_sym_sep, tx_rail_sep, tx_wrap_sep, tx_wb_sep, tx_arg2_sep;
static long tx_trunc_trials, tx_wrapped, tx_ret_nonzero, tx_rails_moved;

/* The far corner; see t_v17data.c for the same construction. */
static short big_ia[65536 + 16], big_qa[65536 + 16];
static short big_ib[65536 + 16], big_qb[65536 + 16];

static void
run_one(int n, unsigned seed, short widx, short len, short *ia, short *qa,
	short *ib, short *qb, int alt)
{
	unsigned short ra, rb;
	long where = (long)n * 1000 + (long)(widx & 0x3ff);
	int i;

	fixture(&ma, seed, widx, (short)(seed % 7), 0, ia, qa, len);
	fixture(&mb, seed, widx, (short)(seed % 7), 0, ib, qb, len);

	rng_seed(seed ^ 0x13572468u);
	for (i = 0; i < NDATA; i++)
		mdata[i] = (unsigned short)(rng_next() | 0x8000u);

	for (i = 0; i < NOUT; i++)
		oa[i] = ob[i] = (short)OMARK;

	ra = ref_TxNoCarrierV29(ma.obj, mdata, oa, (unsigned short)n);
	rb = TxNoCarrierV29(mb.obj, mdata, ob, (unsigned short)n);

	diff_eq_int("at %ld: samples returned", (long)rb, (long)ra, where);
	diff_eq_int("at %ld: return fits the buffer", ra < NOUT, 1, where);
	if (ra >= (unsigned short)NOUT)
		return;
	for (i = 0; i < (int)ra; i++)
		diff_eq_int("sample %ld", ob[i], oa[i], i);
	diff_eq_int("at %ld: nothing past the returned count",
		    oa[ra] == (short)OMARK, 1, where);

	diff_eq_int("at %ld: first differing FP byte", fp_first_diff(&mb, &ma),
		    -1, where);
	diff_eq_int("at %ld: first differing instance byte",
		    obj_first_diff(&mb, &ma), -1, where);
	for (i = 0; i < PPS_TAPS; i++) {
		diff_eq_int("hist_i[%ld]", pps_of(&mb)->hist_i[i],
			    pps_of(&ma)->hist_i[i], i);
		diff_eq_int("hist_q[%ld]", pps_of(&mb)->hist_q[i],
			    pps_of(&ma)->hist_q[i], i);
	}

	if (ia == 0) {
		for (i = 0; i < RING_LEN; i++) {
			diff_eq_int("ring i[%ld]", mb.ri[i], ma.ri[i], i);
			diff_eq_int("ring q[%ld]", mb.rq[i], ma.rq[i], i);
			diff_eq_int("ring sym[%ld]", mb.sym[i], ma.sym[i], i);
			if (n > 0 && ma.ri[i] == 0 && ma.rq[i] == 0)
				tx_rails_moved++;
		}
	} else {
		for (i = 0; i < 65536 + 16; i++)
			if (ia[i - 32768] != ib[i - 32768]
			    || qa[i - 32768] != qb[i - 32768]) {
				diff_eq_int("far rail i[%ld]", ib[i - 32768],
					    ia[i - 32768], i - 32768);
				diff_eq_int("far rail q[%ld]", qb[i - 32768],
					    qa[i - 32768], i - 32768);
				break;
			}
		tx_trunc_trials++;
	}

	if (ra != 0)
		tx_ret_nonzero++;
	if (n > 0 && ring_of(&ma)->widx < widx)
		tx_wrapped++;

	if (!alt || n == 0)
		return;

	{
		struct fpm_smc_ring *r;
		unsigned short rc_alt;
		short w;
		int k;

		/*
		 * WRONG READING: V.17's form -- a symbol index written into
		 * `sym`, with the shaper configured mapped to read it.  Both
		 * sides of this arm come from the blob.
		 */
		fixture(&mc, seed, widx, (short)(seed % 7), 1, 0, 0, len);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->sym[w] = 0;
			next = (short)(w + 1);
			w = next < r->len ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V29FP_PPS, r, oc,
					    (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			tx_sym_sep++;

		/* WRONG READING: only the I rail zeroed. */
		fixture(&mc, seed, widx, (short)(seed % 7), 0, 0, 0, len);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->i[w] = 0;
			next = (short)(w + 1);
			w = next < r->len ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V29FP_PPS, r, oc,
					    (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			tx_rail_sep++;

		/* WRONG READING: the cursor wrapped at len - 1. */
		fixture(&mc, seed, widx, (short)(seed % 7), 0, 0, 0, len);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->i[w] = 0;
			r->q[w] = 0;
			next = (short)(w + 1);
			w = next < (short)(r->len - 1) ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V29FP_PPS, r, oc,
					    (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0
		    || w != ring_of(&ma)->widx)
			tx_wrap_sep++;

		/* WRONG READING: the cursor never written back. */
		if (ring_of(&ma)->widx != widx)
			tx_wb_sep++;

		/*
		 * WRONG READING: the second argument used as a rail source.
		 * Every word of `mdata` has bit 15 set, so none of them is a
		 * value the zeroing path can produce.
		 */
		fixture(&mc, seed, widx, (short)(seed % 7), 0, 0, 0, len);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->i[w] = (short)mdata[k % NDATA];
			r->q[w] = (short)mdata[k % NDATA];
			next = (short)(w + 1);
			w = next < r->len ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V29FP_PPS, r, oc,
					    (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			tx_arg2_sep++;
	}
}

/*
 * A count with bit 15 set.
 *
 * The object loads it `movzwl` and compares it `jb`, so the counter is
 * UNSIGNED and that is forced -- but a signed reading agrees over every count
 * a 50-slot ring will ever be handed, and only a count above 32767 tells them
 * apart.  Seeded directly, because no stimulus sweep produces one.
 */
#define BIGN		40000
static short bigout_a[BIGN + 32], bigout_b[BIGN + 32];

static long tx_bigcount;

static void
run_bigcount(void)
{
	unsigned short ra, rb;
	int i;

	cfg_step = PPS_PHASES;		/* one sample per symbol; see above */
	fixture(&ma, 0x77aa33ccu, 3, 1, 0, 0, 0, 0);
	fixture(&mb, 0x77aa33ccu, 3, 1, 0, 0, 0, 0);
	cfg_step = 3;

	for (i = 0; i < BIGN + 32; i++)
		bigout_a[i] = bigout_b[i] = (short)OMARK;

	ra = ref_TxNoCarrierV29(ma.obj, mdata, bigout_a, (unsigned short)BIGN);
	rb = TxNoCarrierV29(mb.obj, mdata, bigout_b, (unsigned short)BIGN);

	diff_eq_int("bigcount: samples returned", (long)rb, (long)ra, BIGN);
	/*
	 * A step equal to the phase count gives one sample per symbol, plus
	 * however many the shaper owes from its seeded phase -- `FPM_PPS_init`
	 * seeds `phase` from `step`, so the first output can be produced
	 * before any symbol is consumed.  The bound is the symbol count with
	 * room for that debt, not equality.
	 */
	diff_eq_int("bigcount: return fits the buffer", ra <= BIGN + 16, 1,
		    BIGN);
	if (ra > (unsigned short)(BIGN + 16))
		return;
	for (i = 0; i < (int)ra; i++)
		diff_eq_int("bigcount sample %ld", bigout_b[i], bigout_a[i], i);
	diff_eq_int("bigcount: nothing past the returned count",
		    bigout_a[ra] == (short)OMARK, 1, BIGN);
	for (i = 0; i < RING_LEN; i++) {
		diff_eq_int("bigcount ring i[%ld]", mb.ri[i], ma.ri[i], i);
		diff_eq_int("bigcount ring q[%ld]", mb.rq[i], ma.rq[i], i);
	}
	diff_eq_int("bigcount: first differing FP byte",
		    fp_first_diff(&mb, &ma), -1, BIGN);
	/*
	 * The whole ring must have been zeroed: 40,000 passes over 50 slots.
	 * A signed counter would have made none of them.
	 */
	for (i = 0; i < RING_LEN; i++)
		if (ma.ri[i] != 0 || ma.rq[i] != 0)
			break;
	if (i == RING_LEN && ra != 0)
		tx_bigcount++;
}

int
main(void)
{
	static const int counts[] = { 0, 1, 2, 3, 7, 20, 49, 50, 51, 120 };
	static const unsigned seeds[] = { 0x0f0f1e1eu, 0x76543210u };
	static const short widxs[] = { 0, 1, 25, 48, 49 };
	int rc, c, s, w, i;

	build_tables();

	diff_begin("TxNoCarrierV29");
	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++)
		for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++)
			for (w = 0; w < (int)(sizeof(widxs)
					     / sizeof(widxs[0])); w++)
				run_one(counts[c], seeds[s], widxs[w], 0, 0, 0,
					0, 0, 1);

	/*
	 * The far corner: `len` 32767 and a cursor at 32767, so the wrap goes
	 * to -32768 -- which the object reaches by truncating `widx + 1` to a
	 * short and nothing else does.
	 */
	for (i = 0; i < 65536 + 16; i++) {
		big_ia[i] = big_ib[i] = (short)(((i * 29) % 4001) - 2000);
		big_qa[i] = big_qb[i] = (short)(((i * 71) % 4001) - 2100);
	}
	run_one(4, 0x2b2b2b2bu, 32767, 32767, big_ia + 32768, big_qa + 32768,
		big_ib + 32768, big_qb + 32768, 0);
	run_bigcount();
	rc = diff_end();

	diff_begin("v29data separating trials");
	diff_eq_int("V.17's symbol form separates (%ld)", tx_sym_sep > 0, 1,
		    tx_sym_sep);
	diff_eq_int("zeroing only the I rail separates (%ld)",
		    tx_rail_sep > 0, 1, tx_rail_sep);
	diff_eq_int("wrapping at len - 1 separates (%ld)", tx_wrap_sep > 0, 1,
		    tx_wrap_sep);
	diff_eq_int("the cursor write-back is observable (%ld)",
		    tx_wb_sep > 0, 1, tx_wb_sep);
	diff_eq_int("using the second argument separates (%ld)",
		    tx_arg2_sep > 0, 1, tx_arg2_sep);
	diff_eq_int("the cursor wrapped (%ld)", tx_wrapped > 0, 1, tx_wrapped);
	diff_eq_int("both rails were zeroed somewhere (%ld)",
		    tx_rails_moved > 0, 1, tx_rails_moved);
	diff_eq_int("TxNoCarrierV29 returned samples (%ld)",
		    tx_ret_nonzero > 0, 1, tx_ret_nonzero);
	diff_eq_int("the short truncation corner was reached (%ld)",
		    tx_trunc_trials > 0, 1, tx_trunc_trials);
	diff_eq_int("a count with bit 15 set zeroed the whole ring (%ld)",
		    tx_bigcount > 0, 1, tx_bigcount);
	rc |= diff_end();

	return rc;
}
