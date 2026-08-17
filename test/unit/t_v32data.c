/*
 * t_v32data.c -- differential test of V.32's transmit data-path leaves.
 *
 * Both are wrappers, so what can be wrong is WHICH sub-object and WHICH ARM,
 * and a wrong offset agrees with the right one over every input unless the
 * fixture makes the two objects behave differently.  Every check is built
 * around a NAMED WRONG READING and the count of trials that SEPARATE it is
 * asserted non-zero at the end.
 *
 * TWO KINDS OF ENCODER ARE DRIVEN, and the difference matters:
 *
 *   - three STUBS that write three different patterns, so a wrong table base
 *     or a wrong selector offset reaches the output samples rather than only
 *     a log.  A real encoder cannot do that job: `SMCv32_encoder_dif` and
 *     `_abs` agree on plenty of inputs, and a table read one slot high would
 *     often be invisible.
 *   - the three REAL `ref_SMCv32_*` arms, which is what proves the argument
 *     TYPES and not just the addresses: the coder at fp + 0x48 has to be a
 *     `struct v32_smc` for `mode`, `shift`, `quad` and `state[]` to land
 *     where the encoder reads them, and the ring at fp + 0xb0 has to be the
 *     object `FPM_PPS_filter` then reads back.
 *
 * THE RING LENGTHS ARE THE OBJECT'S.  `V32FP_recreate` takes `limit` from
 * `V32_SYMBOL_LEN`, which is {12, 48}; both are driven, and counts above and
 * below each.
 *
 * `PPSv32_CFG` is copied into this fixture's configuration rather than
 * invented: phases 10, step 3, mapped 1, scale 131072, coeffs 120.  The
 * COEFFICIENTS and MAPS are ours and are deliberately non-degenerate --
 * finding 3574 is three mutations that survived because a real V.32 bank
 * repeats two entries and zeroes a third.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32data.h"
#include "dsplib/v32smc.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

extern unsigned short ref_ModDataV32(void *modem, short *data, short *out,
				     unsigned short count);
extern unsigned short ref_TxNoCarrierV32(void *modem, const short *data,
					 short *out, unsigned short count);
extern void ref_FPM_PPS_init(void *state, const void *cfg, int fresh);
extern unsigned short ref_FPM_PPS_filter(void *state, void *src, short *out,
					 unsigned short count);
extern void ref_SMCv32_encoder_abs(void *smc, void *out, void *in,
				   unsigned short count);
extern void ref_SMCv32_encoder_dif(void *smc, void *out, void *in,
				   unsigned short count);
extern void ref_SMCv32_encoder_tcm(void *smc, void *out, void *in,
				   unsigned short count);

#define OBJ_SIZE	0x80
#define FP_SIZE		0x100
#define RING_MAX	64
#define MAP_LEN		256
#define PPS_PHASES	10
#define PPS_COEFFS	120
#define PPS_TAPS	(PPS_COEFFS / PPS_PHASES)
#define PPS_SCALE	131072		/* PPSv32_CFG + 0x08 */

#define NOUT		4096
#define OMARK		0x5ead
#define NDATA		256

#define PPS_HIST_LO	(V32FP_PPS + 0x30)
#define PPS_HIST_HI	(V32FP_PPS + 0x38)
#define RING_PTR_LO	(V32FP_SYMOUT + 0x00)
#define RING_PTR_HI	(V32FP_SYMOUT + 0x0c)
/* Where table[-1] lands: the shaper's `hist_q`, exactly as in t_v17data.c. */
#define V32FP_ENCODER_M1	(V32FP_ENCODERS - 4)

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
	short		sym[RING_MAX];
	short		ri[RING_MAX];
	short		rq[RING_MAX];
	double		align;
};

static struct fix ma, mb, mc;

static struct v32_symout *
ring_of(struct fix *f)
{
	return (struct v32_symout *)(void *)(f->fp + V32FP_SYMOUT);
}

static struct v32_smc *
smc_of(struct fix *f)
{
	return (struct v32_smc *)(void *)(f->fp + V32FP_SMC);
}

static struct fpm_pps *
pps_of(struct fix *f)
{
	return (struct fpm_pps *)(void *)(f->fp + V32FP_PPS);
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

/* --------------------------------------------------------------------- */

struct enc_log {
	int	calls;
	int	slot;
	void	*smc;
	void	*ring;
	short	*data;
	unsigned short count;
};

static struct enc_log elog;

static void
enc_common(int slot, struct v32_smc *smc, struct v32_symout *out, short *data,
	   unsigned short count)
{
	unsigned short k;

	elog.calls++;
	elog.slot = slot;
	elog.smc = (void *)smc;
	elog.ring = (void *)out;
	elog.data = data;
	elog.count = count;

	/* A slot mark in a COMPARED byte; see t_v17data.c and finding 3509. */
	out->buf[0] = (short)(0x0700 + (slot & 0xf));

	for (k = 0; k < count; k++) {
		short next;

		out->buf[out->widx] = (short)(((data[k] + slot * 37 + k * 11)
					       & 0x0f)
					      | ((slot + 1) << 4));
		next = (short)(out->widx + 1);
		out->widx = next < out->limit ? next : 0;
	}
}

static void
enc0(struct v32_smc *s, struct v32_symout *o, short *d, unsigned short n)
{
	enc_common(0, s, o, d, n);
}

static void
enc1(struct v32_smc *s, struct v32_symout *o, short *d, unsigned short n)
{
	enc_common(1, s, o, d, n);
}

static void
enc2(struct v32_smc *s, struct v32_symout *o, short *d, unsigned short n)
{
	enc_common(2, s, o, d, n);
}

static void
encm1(struct v32_smc *s, struct v32_symout *o, short *d, unsigned short n)
{
	enc_common(-1, s, o, d, n);
}

/* --------------------------------------------------------------------- */

static void
make_cfg(struct fpm_pps_cfg *c)
{
	memset(c, 0, sizeof(*c));
	c->phases = PPS_PHASES;
	c->step = 3;
	c->mapped = 1;			/* PPSv32_CFG + 0x04 */
	c->scale = PPS_SCALE;
	c->step_adj = 0;
	c->imap = imap;
	c->qmap = qmap;
	c->coeff_i = coeff_i;
	c->coeff_q = coeff_q;
	c->coeffs = PPS_COEFFS;
}

/*
 * `real` installs the object's own three encoders instead of the stubs.
 */
static void
fixture(struct fix *f, unsigned seed, short widx, short ridx, short sel,
	short limit, int real)
{
	struct fpm_pps_cfg cfg;
	struct v32_smc smc;
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < FP_SIZE; i++)
		f->fp[i] = (unsigned char)rng_next();
	for (i = 0; i < RING_MAX; i++) {
		f->sym[i] = (short)(rng_next() & 0x1f);
		f->ri[i] = (short)(rng_next() % 20001u) - 10000;
		f->rq[i] = (short)(rng_next() % 20001u) - 10000;
	}

	put_ptr(f->obj, V32_OBJ_FP, f->fp);

	/*
	 * The coder.  Small, legal values: `mode` picks the arm AND indexes
	 * `state[3]`, so it is kept in range, and `shift` and `f14` are shift
	 * counts.  `quad` is seeded across all four residues by the caller.
	 */
	memset(&smc, 0, sizeof(smc));
	smc.mode = (short)(sel < 0 ? 0 : sel);
	smc.shift = (short)(seed % 4u);
	smc.quad = (short)((seed >> 3) % 4u);
	smc.state[0] = (short)((seed >> 5) % 4u);
	smc.state[1] = (short)((seed >> 7) % 4u);
	smc.state[2] = (short)((seed >> 9) % 4u);
	smc.f0e = (short)((seed >> 11) % 8u);
	smc.f10 = (short)((seed >> 13) % 8u);
	smc.f14 = (unsigned short)((seed >> 15) % 4u);
	memcpy(f->fp + V32FP_SMC, &smc, sizeof(smc));

	make_cfg(&cfg);
	ref_FPM_PPS_init(f->fp + V32FP_PPS, &cfg, 1);

	if (real) {
		put_ptr(f->fp, V32FP_ENCODERS + 0,
			(void *)ref_SMCv32_encoder_dif);
		put_ptr(f->fp, V32FP_ENCODERS + 4,
			(void *)ref_SMCv32_encoder_abs);
		put_ptr(f->fp, V32FP_ENCODERS + 8,
			(void *)ref_SMCv32_encoder_tcm);
	} else {
		put_ptr(f->fp, V32FP_ENCODERS + 0, (void *)enc0);
		put_ptr(f->fp, V32FP_ENCODERS + 4, (void *)enc1);
		put_ptr(f->fp, V32FP_ENCODERS + 8, (void *)enc2);
	}
	put_short(f->fp, V32FP_ENCODER_SEL, sel);

	/* The symbol ring: two unread rails, the buffer, the two cursors. */
	put_ptr(f->fp, V32FP_SYMOUT + 0x00, f->ri);
	put_ptr(f->fp, V32FP_SYMOUT + 0x04, f->rq);
	put_ptr(f->fp, V32FP_SYMOUT + 0x08, f->sym);
	put_short(f->fp, V32FP_SYMOUT + 0x0c, widx);
	put_short(f->fp, V32FP_SYMOUT + 0x0e, ridx);
	put_short(f->fp, V32FP_SYMOUT + 0x10, limit);
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
		if (i >= V32_OBJ_FP && i < V32_OBJ_FP + (int)sizeof(void *))
			continue;
		if (a->obj[i] != b->obj[i])
			return i;
	}
	return -1;
}

static void
compare_common(struct fix *a, struct fix *b, unsigned short ra,
	       unsigned short rb, short *bufa, short *bufb, long where,
	       int nout)
{
	int i;

	diff_eq_int("at %ld: samples returned", (long)rb, (long)ra, where);
	diff_eq_int("at %ld: return fits the buffer", ra < (unsigned)nout, 1,
		    where);
	if (ra >= (unsigned short)nout)
		return;
	for (i = 0; i < (int)ra; i++)
		diff_eq_int("sample %ld", bufb[i], bufa[i], i);
	diff_eq_int("at %ld: nothing past the returned count",
		    bufa[ra] == (short)OMARK, 1, where);
	diff_eq_int("at %ld: first differing FP byte", fp_first_diff(b, a), -1,
		    where);
	diff_eq_int("at %ld: first differing instance byte",
		    obj_first_diff(b, a), -1, where);
	for (i = 0; i < RING_MAX; i++) {
		diff_eq_int("ring buf[%ld]", b->sym[i], a->sym[i], i);
		diff_eq_int("ring rail i[%ld]", b->ri[i], a->ri[i], i);
		diff_eq_int("ring rail q[%ld]", b->rq[i], a->rq[i], i);
	}
	for (i = 0; i < PPS_TAPS; i++) {
		diff_eq_int("hist_i[%ld]", pps_of(b)->hist_i[i],
			    pps_of(a)->hist_i[i], i);
		diff_eq_int("hist_q[%ld]", pps_of(b)->hist_q[i],
			    pps_of(a)->hist_q[i], i);
	}
}

/* --------------------------------------------------------------------- */

static short oa[NOUT], ob[NOUT], oc[NOUT];
static short mdata_a[NDATA], mdata_b[NDATA], mdata_c[NDATA];

static long mod_base_sep, mod_sel_sep, mod_swap_sep, mod_ring_sep;
static long mod_neg_sel, mod_real_arms[3], mod_data_touched;
static long tx_sym_sep, tx_quad_fwd_sep, tx_wrap_sep, tx_wb_sep;
static long tx_quad_seen[4], tx_wrapped, tx_ret_nonzero, tx_trunc_trials;

static void
fill_data(unsigned seed)
{
	int i;

	rng_seed(seed ^ 0x5a5a5a5au);
	for (i = 0; i < NDATA; i++) {
		mdata_a[i] = (short)(rng_next() & 0xffffu);
		mdata_b[i] = mdata_a[i];
		mdata_c[i] = mdata_a[i];
	}
}

static void
run_mod_one(short sel, int n, unsigned seed, short widx, short limit, int real,
	    int alt)
{
	unsigned short ra, rb;
	struct enc_log la, lb;
	long where = (long)sel * 1000000 + (long)n * 1000 + (long)limit;
	int i;

	fixture(&ma, seed, widx, (short)(seed % (unsigned)limit), sel, limit,
		real);
	fixture(&mb, seed, widx, (short)(seed % (unsigned)limit), sel, limit,
		real);
	if (sel < 0) {
		put_ptr(ma.fp, V32FP_ENCODER_M1, (void *)encm1);
		put_ptr(mb.fp, V32FP_ENCODER_M1, (void *)encm1);
	}

	fill_data(seed);
	for (i = 0; i < NOUT; i++)
		oa[i] = ob[i] = (short)OMARK;

	memset(&elog, 0, sizeof(elog));
	ra = ref_ModDataV32(ma.obj, mdata_a, oa, (unsigned short)n);
	la = elog;
	memset(&elog, 0, sizeof(elog));
	rb = ModDataV32(mb.obj, mdata_b, ob, (unsigned short)n);
	lb = elog;

	compare_common(&ma, &mb, ra, rb, oa, ob, where, NOUT);
	/*
	 * The trellis arm masks its input IN PLACE, which is why `data` is not
	 * const.  Both sides get their own copy of the same words and the two
	 * copies are compared.
	 */
	for (i = 0; i < NDATA; i++) {
		diff_eq_int("data[%ld] after the encoder", mdata_b[i],
			    mdata_a[i], i);
		if (mdata_a[i] != mdata_c[i])
			mod_data_touched++;
	}

	if (!real) {
		diff_eq_int("at %ld: the encoder was called once", lb.calls,
			    la.calls, where);
		diff_eq_int("at %ld: through the same slot", lb.slot, la.slot,
			    where);
		diff_eq_int("at %ld: coder at V32FP_SMC",
			    lb.smc == (void *)smc_of(&mb)
			    && la.smc == (void *)smc_of(&ma), 1, where);
		diff_eq_int("at %ld: ring at V32FP_SYMOUT",
			    lb.ring == (void *)ring_of(&mb)
			    && la.ring == (void *)ring_of(&ma), 1, where);
		diff_eq_int("at %ld: data and count straight through",
			    la.data == mdata_a && lb.data == mdata_b
			    && la.count == (unsigned short)n, 1, where);
		if (sel < 0 && la.slot == -1)
			mod_neg_sel++;
	} else if (sel >= 0 && sel < 3 && n > 0 && ra != 0) {
		mod_real_arms[sel]++;
	}

	if (!alt || n == 0 || sel < 0 || real)
		return;

	/* WRONG READING: the table base one slot high (selectors 0 and 1). */
	if (sel <= 1) {
		v32_encoder_fn fn;
		unsigned short rc_alt;

		fixture(&mc, seed, widx, (short)(seed % (unsigned)limit), sel,
			limit, 0);
		fill_data(seed);
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		fn = *(v32_encoder_fn *)(void *)(mc.fp + V32FP_ENCODERS + 4
						 + sel * 4);
		fn(smc_of(&mc), ring_of(&mc), mdata_c, (unsigned short)n);
		rc_alt = ref_FPM_PPS_filter(mc.fp + V32FP_PPS, ring_of(&mc),
					    oc, (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			mod_base_sep++;
	}

	/* WRONG READING: the selector two bytes high. */
	{
		v32_encoder_fn fn;
		unsigned short rc_alt;
		short bad;

		fixture(&mc, seed, widx, (short)(seed % (unsigned)limit), sel,
			limit, 0);
		put_short(mc.fp, V32FP_ENCODER_SEL + 2,
			  (short)((sel + 1) % V32FP_ENCODERS_N));
		bad = *(short *)(void *)(mc.fp + V32FP_ENCODER_SEL + 2);
		fill_data(seed);
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		fn = *(v32_encoder_fn *)(void *)(mc.fp + V32FP_ENCODERS
						 + bad * 4);
		fn(smc_of(&mc), ring_of(&mc), mdata_c, (unsigned short)n);
		rc_alt = ref_FPM_PPS_filter(mc.fp + V32FP_PPS, ring_of(&mc),
					    oc, (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			mod_sel_sep++;
	}

	/*
	 * WRONG READING: coder and ring transposed.  The coder block is given
	 * a valid ring header first, so the transposed call writes real memory
	 * elsewhere instead of dereferencing pseudorandom filler.
	 */
	{
		static short swap_buf[RING_MAX];
		v32_encoder_fn fn;
		unsigned short rc_alt;

		fixture(&mc, seed, widx, (short)(seed % (unsigned)limit), sel,
			limit, 0);
		for (i = 0; i < RING_MAX; i++)
			swap_buf[i] = (short)((i * 7 + 3) & 0x1f);
		put_ptr(mc.fp, V32FP_SMC + 0x08, swap_buf);
		put_short(mc.fp, V32FP_SMC + 0x0c, widx);
		put_short(mc.fp, V32FP_SMC + 0x10, limit);
		fill_data(seed);
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		fn = *(v32_encoder_fn *)(void *)(mc.fp + V32FP_ENCODERS
						 + sel * 4);
		fn((struct v32_smc *)(void *)ring_of(&mc),
		   (struct v32_symout *)(void *)smc_of(&mc), mdata_c,
		   (unsigned short)n);
		rc_alt = ref_FPM_PPS_filter(mc.fp + V32FP_PPS, ring_of(&mc),
					    oc, (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			mod_swap_sep++;
	}

	/* WRONG READING: the shaper filtered from a ring nobody wrote. */
	{
		static struct v32_symout alt_ring;
		static short alt_buf[RING_MAX];
		v32_encoder_fn fn;
		unsigned short rc_alt;

		fixture(&mc, seed, widx, (short)(seed % (unsigned)limit), sel,
			limit, 0);
		for (i = 0; i < RING_MAX; i++)
			alt_buf[i] = (short)((i * 13 + 5) & 0x1f);
		alt_ring = *ring_of(&mc);
		alt_ring.buf = alt_buf;
		fill_data(seed);
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		fn = *(v32_encoder_fn *)(void *)(mc.fp + V32FP_ENCODERS
						 + sel * 4);
		fn(smc_of(&mc), ring_of(&mc), mdata_c, (unsigned short)n);
		rc_alt = ref_FPM_PPS_filter(mc.fp + V32FP_PPS, &alt_ring, oc,
					    (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			mod_ring_sep++;
	}
}

static int
run_mod(void)
{
	static const int counts[] = { 0, 1, 2, 3, 7, 11, 12, 13, 47, 48, 49 };
	static const unsigned seeds[] = { 0x2468ace0u, 0x11223344u };
	static const short limits[] = { 12, 48 };	/* V32_SYMBOL_LEN */
	int c, s, l, real;
	short sel;

	diff_begin("ModDataV32");
	for (real = 0; real < 2; real++)
		for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++)
			for (l = 0; l < 2; l++)
				for (sel = 0; sel < V32FP_ENCODERS_N; sel++)
					for (c = 0; c < (int)(sizeof(counts)
						      / sizeof(counts[0])); c++)
						run_mod_one(sel, counts[c],
							    seeds[s],
							    (short)(counts[c]
								    % limits[l]),
							    limits[l], real,
							    c == 4);
	/*
	 * The selector is SIGNED.  table[-1] is the shaper's `hist_q` here as
	 * it is for V.17, so the count is zero; the stub still marks the ring.
	 */
	run_mod_one(-1, 0, 0x0badf00du, 0, 48, 0, 0);
	run_mod_one(-1, 0, 0x0badf00du, 11, 12, 0, 0);
	return diff_end();
}

/* --------------------------------------------------------------------- */

/* The far corner, as in t_v17data.c: 32,768 shorts of headroom below. */
static short big_a[65536 + 16], big_b[65536 + 16];

static void
run_tx_one(int n, unsigned seed, short widx, short limit, short quad,
	   short *buf_a, short *buf_b, int alt)
{
	unsigned short ra, rb;
	long where = (long)n * 10000 + (long)limit * 10 + quad;
	int i;

	fixture(&ma, seed, widx, (short)(seed % (unsigned)limit), 0, limit, 0);
	fixture(&mb, seed, widx, (short)(seed % (unsigned)limit), 0, limit, 0);
	smc_of(&ma)->quad = quad;
	smc_of(&mb)->quad = quad;
	if (buf_a != 0) {
		put_ptr(ma.fp, V32FP_SYMOUT + 0x08, buf_a);
		put_ptr(mb.fp, V32FP_SYMOUT + 0x08, buf_b);
	}

	fill_data(seed);
	for (i = 0; i < NOUT; i++)
		oa[i] = ob[i] = (short)OMARK;

	ra = ref_TxNoCarrierV32(ma.obj, mdata_a, oa, (unsigned short)n);
	rb = TxNoCarrierV32(mb.obj, mdata_b, ob, (unsigned short)n);

	if (buf_a == 0) {
		compare_common(&ma, &mb, ra, rb, oa, ob, where, NOUT);
	} else {
		diff_eq_int("far: samples returned", (long)rb, (long)ra, where);
		for (i = 0; i < 65536 + 16; i++)
			if (buf_a[i - 32768] != buf_b[i - 32768]) {
				diff_eq_int("far buf[%ld]", buf_b[i - 32768],
					    buf_a[i - 32768], i - 32768);
				break;
			}
		diff_eq_int("far: cursor", ring_of(&mb)->widx,
			    ring_of(&ma)->widx, where);
		diff_eq_int("far: quad", smc_of(&mb)->quad,
			    smc_of(&ma)->quad, where);
		tx_trunc_trials++;
		return;
	}

	if (ra != 0)
		tx_ret_nonzero++;
	if (n > 0 && ring_of(&ma)->widx < widx)
		tx_wrapped++;
	if (smc_of(&ma)->quad >= 0 && smc_of(&ma)->quad < 4)
		tx_quad_seen[smc_of(&ma)->quad]++;

	if (!alt || n == 0)
		return;

	{
		struct v32_symout *r;
		struct v32_smc *s;
		unsigned short rc_alt;
		short w;
		int k;

		/* WRONG READING: a different constellation index. */
		fixture(&mc, seed, widx, (short)(seed % (unsigned)limit), 0,
			limit, 0);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->buf[w] = V32_SYMBOL_NOCARRIER - 1;
			next = (short)(w + 1);
			w = next < r->limit ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V32FP_PPS, r, oc,
					    (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			tx_sym_sep++;

		/*
		 * WRONG READING: the quadrant stepped FORWARDS.  The two agree
		 * whenever the count is a multiple of four, so the counts
		 * driven here deliberately are not.
		 */
		s = smc_of(&mc);
		s->quad = quad;
		for (k = 0; k < n; k++)
			s->quad = (short)((s->quad + 1) & 3);
		if (s->quad != smc_of(&ma)->quad)
			tx_quad_fwd_sep++;

		/* WRONG READING: the cursor wrapped at limit - 1. */
		fixture(&mc, seed, widx, (short)(seed % (unsigned)limit), 0,
			limit, 0);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->buf[w] = V32_SYMBOL_NOCARRIER;
			next = (short)(w + 1);
			w = next < (short)(r->limit - 1) ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V32FP_PPS, r, oc,
					    (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0
		    || w != ring_of(&ma)->widx)
			tx_wrap_sep++;

		/* WRONG READING: the cursor never written back. */
		if (ring_of(&ma)->widx != widx)
			tx_wb_sep++;
	}
}

static int
run_tx(void)
{
	static const int counts[] = { 0, 1, 2, 3, 5, 7, 11, 12, 13, 47, 48, 50 };
	static const unsigned seeds[] = { 0x0f0f1e1eu, 0x76543210u };
	static const short limits[] = { 12, 48 };
	int c, s, l, i;
	short q;

	diff_begin("TxNoCarrierV32");
	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++)
		for (l = 0; l < 2; l++)
			for (q = 0; q < 4; q++)
				for (c = 0; c < (int)(sizeof(counts)
						     / sizeof(counts[0])); c++)
					run_tx_one(counts[c], seeds[s],
						   (short)(counts[c]
							   % limits[l]),
						   limits[l], q, 0, 0, 1);

	for (i = 0; i < 65536 + 16; i++)
		big_a[i] = big_b[i] = (short)((i * 29) & 0x1f);
	run_tx_one(4, 0x2b2b2b2bu, 32767, 32767, 1, big_a + 32768,
		   big_b + 32768, 0);
	return diff_end();
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	build_tables();

	rc |= run_mod();
	rc |= run_tx();

	diff_begin("v32data separating trials");
	diff_eq_int("the encoder table base separates (%ld)",
		    mod_base_sep > 0, 1, mod_base_sep);
	diff_eq_int("the selector offset separates (%ld)", mod_sel_sep > 0, 1,
		    mod_sel_sep);
	diff_eq_int("transposing coder and ring separates (%ld)",
		    mod_swap_sep > 0, 1, mod_swap_sep);
	diff_eq_int("an unshared ring separates (%ld)", mod_ring_sep > 0, 1,
		    mod_ring_sep);
	diff_eq_int("the blob took a NEGATIVE selector (%ld)",
		    mod_neg_sel > 0, 1, mod_neg_sel);
	diff_eq_int("the real dif arm produced samples (%ld)",
		    mod_real_arms[0] > 0, 1, mod_real_arms[0]);
	diff_eq_int("the real abs arm produced samples (%ld)",
		    mod_real_arms[1] > 0, 1, mod_real_arms[1]);
	diff_eq_int("the real tcm arm produced samples (%ld)",
		    mod_real_arms[2] > 0, 1, mod_real_arms[2]);
	diff_eq_int("an encoder wrote through `data` (%ld)",
		    mod_data_touched > 0, 1, mod_data_touched);

	diff_eq_int("a different no-carrier index separates (%ld)",
		    tx_sym_sep > 0, 1, tx_sym_sep);
	diff_eq_int("stepping the quadrant forwards separates (%ld)",
		    tx_quad_fwd_sep > 0, 1, tx_quad_fwd_sep);
	diff_eq_int("wrapping at limit - 1 separates (%ld)", tx_wrap_sep > 0,
		    1, tx_wrap_sep);
	diff_eq_int("the cursor write-back is observable (%ld)",
		    tx_wb_sep > 0, 1, tx_wb_sep);
	diff_eq_int("the cursor wrapped (%ld)", tx_wrapped > 0, 1, tx_wrapped);
	diff_eq_int("TxNoCarrierV32 returned samples (%ld)",
		    tx_ret_nonzero > 0, 1, tx_ret_nonzero);
	diff_eq_int("quadrant 0 was reached (%ld)", tx_quad_seen[0] > 0, 1,
		    tx_quad_seen[0]);
	diff_eq_int("quadrant 1 was reached (%ld)", tx_quad_seen[1] > 0, 1,
		    tx_quad_seen[1]);
	diff_eq_int("quadrant 2 was reached (%ld)", tx_quad_seen[2] > 0, 1,
		    tx_quad_seen[2]);
	diff_eq_int("quadrant 3 was reached (%ld)", tx_quad_seen[3] > 0, 1,
		    tx_quad_seen[3]);
	diff_eq_int("the short truncation corner was reached (%ld)",
		    tx_trunc_trials > 0, 1, tx_trunc_trials);
	rc |= diff_end();

	return rc;
}
