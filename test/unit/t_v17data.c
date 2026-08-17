/*
 * t_v17data.c -- differential test of V.17's transmit data-path leaves.
 *
 * Both functions are wrappers: they pick a sub-object out of an unmodelled
 * instance and hand it to the module that owns it.  A wrapper has almost no
 * arithmetic of its own, so "it agrees with the blob over a thousand random
 * inputs" is nearly worthless -- the thousand inputs are carried by the
 * callee, which has its own suite.  What can be wrong is WHICH sub-object,
 * and a wrong offset agrees with the right one over every input unless the
 * fixture makes the two objects behave differently.
 *
 * So every check is built around a NAMED WRONG READING and the count of
 * trials that SEPARATE it is asserted non-zero at the end (finding 3052's
 * rule).  The wrong readings, in order:
 *
 *   ModDataV17
 *       - the encoder table at the wrong base (fp + 0x84 rather than 0x80).
 *         Evaluated for real: the three slots hold three DIFFERENT stubs
 *         that write three different patterns into the ring, so the wrong
 *         base changes the OUTPUT SAMPLES and not only a log.
 *       - the selector at the wrong offset (fp + 0x8e rather than 0x8c),
 *         same construction.
 *       - the selector read UNSIGNED.  Separated by construction rather
 *         than counted: with a selector of -1 the signed reading calls
 *         fp + 0x7c, which this fixture fills with a fourth stub, and the
 *         unsigned reading indexes 262,140 bytes past the block.  The blob
 *         is RUN at -1 and must land on the fourth stub, which is what
 *         makes `short` a measurement here rather than a codegen note.
 *       - the coder and the ring passed the other way round.  Evaluated for
 *         real; the stub writes into whichever it is given, so a transposed
 *         pair leaves the ring stale and the samples differ.
 *       - the ring not shared between the encoder and the shaper.  Evaluated
 *         for real: encode into one ring, shape from another.
 *       - the instance's FP pointer not re-read after the encoder returns.
 *         NOT separable and not claimed: no callee this function can make
 *         writes obj + 0x28.  The re-read is recorded in v17data.h as
 *         forced-by-codegen and nothing here pretends to measure it.
 *
 *   TxNoCarrierV17
 *       - the symbol taken from another offset of the parameter block.  The
 *         whole block is pseudorandom, so +0x1c, +0x1e and +0x20 all differ.
 *       - the symbol written to the ring's `i`/`q` rails rather than `sym`,
 *         which is V.29's form of the same function.  With `cfg.mapped` set
 *         the shaper reads `sym`, so the rails form leaves it stale.
 *       - the write cursor wrapped at `len - 1`, or not wrapped, or not
 *         written back.
 *       - `(short)(widx + 1)` read as `widx + 1`.  Reached by SEEDING the
 *         cursor at 32767 over a ring whose buffer has 32,768 shorts of
 *         headroom below it -- a stimulus sweep never gets there.
 *       - the second argument used rather than ignored: it is filled with a
 *         pattern that is never a legal symbol index for this fixture.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v17data.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

extern unsigned short ref_ModDataV17(void *modem, const unsigned short *data,
				     short *out, unsigned short count);
extern unsigned short ref_TxNoCarrierV17(void *modem,
					 const unsigned short *data,
					 short *out, unsigned short count);
extern void ref_FPM_PPS_init(void *state, const void *cfg, int fresh);
extern unsigned short ref_FPM_PPS_filter(void *state, void *src, short *out,
					 unsigned short count);

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x40
#define FP_SIZE		0x100
#define PRM_SIZE	0x40
#define RING_LEN	50
#define MAP_LEN		256
#define PPS_PHASES	10
#define PPS_COEFFS	120
#define PPS_TAPS	(PPS_COEFFS / PPS_PHASES)

#define NOUT		4096
#define OMARK		0x5ead
#define NDATA		256

/* Where the shaper's two history pointers land inside the FP block. */
#define PPS_HIST_LO	(V17FP_PPS + 0x30)
#define PPS_HIST_HI	(V17FP_PPS + 0x38)
/* Where the ring's three array pointers land. */
#define RING_PTR_LO	(V17FP_SMC_RING + 0x00)
#define RING_PTR_HI	(V17FP_SMC_RING + 0x0c)

/*
 * Where table[-1] lands, for the signed-selector trial.
 *
 * IT IS THE SHAPER'S `hist_q` POINTER: `struct fpm_pps` is 0x38 bytes and
 * V17FP_PPS is 0x48, so the state ends exactly where the table begins and
 * fp + 0x7c is its last field.  A stub put there destroys the shaper, so the
 * negative-selector trials run with a count of ZERO -- `FPM_PPS_filter`'s
 * loop is `while (count != 0)` and neither history is dereferenced on that
 * path, while the encoder is still dispatched and still leaves its mark.
 */
#define V17FP_ENCODER_M1	(V17FP_ENCODERS - 4)

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
 * The coefficient banks are built here rather than taken from the object's,
 * and deliberately: finding 3574 is three mutations that survived because a
 * real V.32 bank repeats two entries and zeroes a third, so transposing them
 * was invisible.  Every entry below is distinct and no two of the four banks
 * agree anywhere.
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
/* The fixture                                                           */

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	fp[FP_SIZE];
	unsigned char	prm[PRM_SIZE];
	short		sym[RING_LEN];
	short		ri[RING_LEN];
	short		rq[RING_LEN];
	double		align;
};

static struct fix ma, mb, mc;

static struct fpm_smc_ring *
ring_of(struct fix *f)
{
	return (struct fpm_smc_ring *)(void *)(f->fp + V17FP_SMC_RING);
}

static struct fpm_pps *
pps_of(struct fix *f)
{
	return (struct fpm_pps *)(void *)(f->fp + V17FP_PPS);
}

/*
 * What each stub was handed, and by which slot.  The stubs write a slot-
 * specific pattern into the ring they are given, so a wrong slot, a wrong
 * ring or a transposed argument pair all reach the OUTPUT SAMPLES and are
 * not merely logged.
 */
struct enc_log {
	int	calls;
	int	slot;
	void	*smc;
	void	*ring;
	const unsigned short *data;
	unsigned short count;
};

static struct enc_log elog;

static void
enc_common(int slot, void *smc, struct fpm_smc_ring *ring,
	   const unsigned short *data, unsigned short count)
{
	unsigned short k;

	elog.calls++;
	elog.slot = slot;
	elog.smc = smc;
	elog.ring = (void *)ring;
	elog.data = data;
	elog.count = count;

	/*
	 * A slot-specific mark, written whatever the count.  It puts WHICH
	 * SLOT WAS CALLED into a compared byte of the ring rather than only
	 * into this log, which is what finding 3509 asks of a separating
	 * trial -- and it is the only observable the zero-count trials have.
	 */
	ring->sym[0] = (short)(0x0700 + (slot & 0xf));

	/*
	 * Write `count` symbols, advancing and wrapping the ring's own write
	 * cursor exactly as a real encoder does.  The pattern depends on the
	 * slot, on the data and on the position, so no two slots can produce
	 * the same ring.
	 */
	for (k = 0; k < count; k++) {
		short next;

		ring->sym[ring->widx] = (short)(((data[k] + slot * 37u
						  + k * 11u) & 0x7fu)
						| (unsigned)(slot << 8));
		next = (short)(ring->widx + 1);
		ring->widx = next < ring->len ? next : 0;
	}
}

static void
enc0(void *smc, struct fpm_smc_ring *r, const unsigned short *d,
     unsigned short n)
{
	enc_common(0, smc, r, d, n);
}

static void
enc1(void *smc, struct fpm_smc_ring *r, const unsigned short *d,
     unsigned short n)
{
	enc_common(1, smc, r, d, n);
}

static void
enc2(void *smc, struct fpm_smc_ring *r, const unsigned short *d,
     unsigned short n)
{
	enc_common(2, smc, r, d, n);
}

static void
encm1(void *smc, struct fpm_smc_ring *r, const unsigned short *d,
      unsigned short n)
{
	enc_common(-1, smc, r, d, n);
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

static void
make_cfg(struct fpm_pps_cfg *c)
{
	memset(c, 0, sizeof(*c));
	c->phases = PPS_PHASES;
	c->step = 3;
	c->mapped = 1;			/* V17TX_create sets this; see the header */
	c->scale = 32767;
	c->step_adj = 0;
	c->imap = imap;
	c->qmap = qmap;
	c->coeff_i = coeff_i;
	c->coeff_q = coeff_q;
	c->coeffs = PPS_COEFFS;
}

/*
 * Lay one instance down.  `seed` drives the pseudorandom fill of the whole FP
 * block and the parameter block, so an offset wrong by a few bytes reads
 * garbage rather than a plausible zero.
 */
static void
fixture(struct fix *f, unsigned seed, short widx, short ridx, short sel,
	short *sym, short len)
{
	struct fpm_pps_cfg cfg;
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < FP_SIZE; i++)
		f->fp[i] = (unsigned char)rng_next();
	for (i = 0; i < PRM_SIZE; i++)
		f->prm[i] = (unsigned char)rng_next();
	for (i = 0; i < RING_LEN; i++) {
		f->sym[i] = (short)(rng_next() & 0x7f);
		f->ri[i] = (short)(rng_next() % 20001u) - 10000;
		f->rq[i] = (short)(rng_next() % 20001u) - 10000;
	}

	put_ptr(f->obj, V17TX_OBJ_FP, f->fp);
	put_ptr(f->obj, V17TX_OBJ_PARAMS, f->prm);

	/* The ring, field for field as V17TX_create builds it. */
	put_ptr(f->fp, V17FP_SMC_RING + 0x00, f->ri);
	put_ptr(f->fp, V17FP_SMC_RING + 0x04, f->rq);
	put_ptr(f->fp, V17FP_SMC_RING + 0x08, sym ? sym : f->sym);
	put_short(f->fp, V17FP_SMC_RING + 0x0c, widx);
	put_short(f->fp, V17FP_SMC_RING + 0x0e, ridx);
	put_short(f->fp, V17FP_SMC_RING + 0x10, len ? len : (short)RING_LEN);

	make_cfg(&cfg);
	ref_FPM_PPS_init(f->fp + V17FP_PPS, &cfg, 1);

	put_ptr(f->fp, V17FP_ENCODERS + 0, (void *)enc0);
	put_ptr(f->fp, V17FP_ENCODERS + 4, (void *)enc1);
	put_ptr(f->fp, V17FP_ENCODERS + 8, (void *)enc2);
	put_short(f->fp, V17FP_ENCODER_SEL, sel);
	/*
	 * The two bytes after the selector are left pseudorandom on purpose:
	 * a selector read from +0x8e picks a different slot.
	 */
}

static int
skip_fp(int off)
{
	if (off >= RING_PTR_LO && off < RING_PTR_HI)
		return 1;		/* three per-fixture array pointers */
	if (off >= PPS_HIST_LO && off < PPS_HIST_HI)
		return 1;		/* two per-fixture history buffers  */
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
		if (i >= V17TX_OBJ_PARAMS
		    && i < V17TX_OBJ_FP + (int)sizeof(void *))
			continue;		/* the two instance pointers */
		if (a->obj[i] != b->obj[i])
			return i;
	}
	return -1;
}

static long
prm_first_diff(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < PRM_SIZE; i++)
		if (a->prm[i] != b->prm[i])
			return i;
	return -1;
}

static void
compare_hist(struct fix *a, struct fix *b, long where)
{
	int i;

	for (i = 0; i < PPS_TAPS; i++) {
		diff_eq_int("hist_i[%ld]", pps_of(b)->hist_i[i],
			    pps_of(a)->hist_i[i], i);
		diff_eq_int("hist_q[%ld]", pps_of(b)->hist_q[i],
			    pps_of(a)->hist_q[i], i);
	}
	(void)where;
}

/* --------------------------------------------------------------------- */

static short oa[NOUT], ob[NOUT], oc[NOUT];
static unsigned short mdata[NDATA];

static long mod_base_sep, mod_sel_sep, mod_swap_sep, mod_ring_sep;
static long mod_neg_sel, mod_ret_nonzero, mod_ring_wrapped;
static long tx_off_sep, tx_rail_sep, tx_wrap_sep, tx_wb_sep, tx_arg2_sep;
static long tx_trunc_trials, tx_wrapped, tx_ret_nonzero;

/*
 * A local driver: everything ModDataV17 does, with one reading changed.  Both
 * halves come from the blob, so the only difference is the reading itself.
 */
static unsigned short
drive_mod(struct fix *f, const unsigned short *data, short *out,
	  unsigned short count, int tbl_off, int sel_off, int swap,
	  struct fpm_smc_ring *shape_from)
{
	v17_encoder_fn fn;
	short sel;

	sel = *(short *)(void *)(f->fp + sel_off);
	fn = *(v17_encoder_fn *)(void *)(f->fp + tbl_off + sel * 4);
	if (swap)
		fn((void *)ring_of(f),
		   (struct fpm_smc_ring *)(void *)(f->fp + V17FP_SMC),
		   data, count);
	else
		fn(f->fp + V17FP_SMC, ring_of(f), data, count);

	return ref_FPM_PPS_filter(f->fp + V17FP_PPS,
				  shape_from ? (void *)shape_from
					     : (void *)ring_of(f),
				  out, count);
}

static void
run_mod_one(short sel, int n, unsigned seed, short widx, int alt)
{
	unsigned short ra, rb;
	struct enc_log la, lb;
	long where = (long)sel * 100000 + (long)n * 100 + (long)(widx & 0xff);
	int i;

	fixture(&ma, seed, widx, (short)(seed % RING_LEN), sel, 0, 0);
	fixture(&mb, seed, widx, (short)(seed % RING_LEN), sel, 0, 0);
	if (sel < 0) {
		put_ptr(ma.fp, V17FP_ENCODER_M1, (void *)encm1);
		put_ptr(mb.fp, V17FP_ENCODER_M1, (void *)encm1);
	}

	rng_seed(seed ^ 0x5a5a5a5au);
	for (i = 0; i < NDATA; i++)
		mdata[i] = (unsigned short)rng_next();

	for (i = 0; i < NOUT; i++)
		oa[i] = ob[i] = (short)OMARK;

	memset(&elog, 0, sizeof(elog));
	ra = ref_ModDataV17(ma.obj, mdata, oa, (unsigned short)n);
	la = elog;
	memset(&elog, 0, sizeof(elog));
	rb = ModDataV17(mb.obj, mdata, ob, (unsigned short)n);
	lb = elog;

	diff_eq_int("at %ld: samples returned", (long)rb, (long)ra, where);
	diff_eq_int("at %ld: return fits the buffer", ra < NOUT, 1, where);
	if (ra >= (unsigned short)NOUT)
		return;
	for (i = 0; i < (int)ra; i++)
		diff_eq_int("sample %ld", ob[i], oa[i], i);
	diff_eq_int("at %ld: nothing past the returned count",
		    oa[ra] == (short)OMARK, 1, where);

	diff_eq_int("at %ld: the encoder was called once", lb.calls, la.calls,
		    where);
	diff_eq_int("at %ld: through the same slot", lb.slot, la.slot, where);
	diff_eq_int("at %ld: with the coder at V17FP_SMC",
		    lb.smc == (void *)(mb.fp + V17FP_SMC), 1, where);
	diff_eq_int("at %ld: and the blob agreed",
		    la.smc == (void *)(ma.fp + V17FP_SMC), 1, where);
	diff_eq_int("at %ld: with the ring at V17FP_SMC_RING",
		    lb.ring == (void *)ring_of(&mb), 1, where);
	diff_eq_int("at %ld: and the blob agreed",
		    la.ring == (void *)ring_of(&ma), 1, where);
	diff_eq_int("at %ld: the data pointer straight through",
		    lb.data == mdata && la.data == mdata, 1, where);
	diff_eq_int("at %ld: the count straight through",
		    lb.count == (unsigned short)n
		    && la.count == (unsigned short)n, 1, where);

	diff_eq_int("at %ld: first differing FP byte", fp_first_diff(&mb, &ma),
		    -1, where);
	diff_eq_int("at %ld: first differing instance byte",
		    obj_first_diff(&mb, &ma), -1, where);
	diff_eq_int("at %ld: first differing parameter byte",
		    prm_first_diff(&mb, &ma), -1, where);
	for (i = 0; i < RING_LEN; i++) {
		diff_eq_int("ring sym[%ld]", mb.sym[i], ma.sym[i], i);
		diff_eq_int("ring i[%ld]", mb.ri[i], ma.ri[i], i);
		diff_eq_int("ring q[%ld]", mb.rq[i], ma.rq[i], i);
	}
	compare_hist(&ma, &mb, where);

	if (ra != 0)
		mod_ret_nonzero++;
	if (sel < 0 && la.slot == -1)
		mod_neg_sel++;
	if (ring_of(&ma)->widx < widx)
		mod_ring_wrapped++;

	if (!alt || n == 0)
		return;

	/*
	 * WRONG READING: the table base one slot high.  Only for selectors 0
	 * and 1: at 2 the shifted base would read fp + 0x8c, which is the
	 * SELECTOR and not a function pointer, so that arm would be measuring
	 * a wild jump rather than a wrong table.
	 */
	if (sel >= 0 && sel <= 1) {
		unsigned short rc_alt;

		fixture(&mc, seed, widx, (short)(seed % RING_LEN), sel, 0, 0);
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = drive_mod(&mc, mdata, oc, (unsigned short)n,
				   V17FP_ENCODERS + 4, V17FP_ENCODER_SEL, 0,
				   0);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			mod_base_sep++;
	}

	/* WRONG READING: the selector two bytes high. */
	fixture(&mc, seed, widx, (short)(seed % RING_LEN), sel, 0, 0);
	put_short(mc.fp, V17FP_ENCODER_SEL + 2,
		  (short)((sel + 1) % V17FP_ENCODERS_N));
	for (i = 0; i < NOUT; i++)
		oc[i] = (short)OMARK;
	{
		unsigned short rc_alt = drive_mod(&mc, mdata, oc,
						  (unsigned short)n,
						  V17FP_ENCODERS,
						  V17FP_ENCODER_SEL + 2, 0, 0);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			mod_sel_sep++;
	}

	/*
	 * WRONG READING: the coder and the ring transposed.  The coder block
	 * is given a VALID ring header first, so the transposed call writes
	 * real memory somewhere else instead of dereferencing the block's
	 * pseudorandom filler -- the reading is evaluated, not merely made to
	 * crash.
	 */
	if (sel >= 0) {
		static short swap_sym[RING_LEN];
		unsigned short rc_alt;

		fixture(&mc, seed, widx, (short)(seed % RING_LEN), sel, 0, 0);
		for (i = 0; i < RING_LEN; i++)
			swap_sym[i] = (short)((i * 7 + 3) & 0x7f);
		put_ptr(mc.fp, V17FP_SMC + 0x08, swap_sym);
		put_short(mc.fp, V17FP_SMC + 0x0c, widx);
		put_short(mc.fp, V17FP_SMC + 0x10, (short)RING_LEN);
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = drive_mod(&mc, mdata, oc, (unsigned short)n,
				   V17FP_ENCODERS, V17FP_ENCODER_SEL, 1, 0);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			mod_swap_sep++;
	}

	/* WRONG READING: the ring not shared with the shaper. */
	if (sel >= 0) {
		static struct fpm_smc_ring alt_ring;
		static short alt_sym[RING_LEN];

		fixture(&mc, seed, widx, (short)(seed % RING_LEN), sel, 0, 0);
		for (i = 0; i < RING_LEN; i++)
			alt_sym[i] = (short)((i * 13 + 5) & 0x7f);
		alt_ring = *ring_of(&mc);
		alt_ring.sym = alt_sym;
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		{
			unsigned short rc_alt = drive_mod(&mc, mdata, oc,
							  (unsigned short)n,
							  V17FP_ENCODERS,
							  V17FP_ENCODER_SEL,
							  0, &alt_ring);
			if (rc_alt != ra
			    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
				mod_ring_sep++;
		}
	}
}

static int
run_mod(void)
{
	static const int counts[] = { 0, 1, 2, 3, 7, 20, 49, 50, 51, 120 };
	static const unsigned seeds[] = { 0x2468ace0u, 0x11223344u };
	static const short widxs[] = { 0, 1, 48, 49 };
	int c, s, w;
	short sel;

	diff_begin("ModDataV17");
	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++)
		for (sel = 0; sel < V17FP_ENCODERS_N; sel++)
			for (c = 0; c < (int)(sizeof(counts)
					      / sizeof(counts[0])); c++)
				for (w = 0; w < (int)(sizeof(widxs)
						     / sizeof(widxs[0])); w++)
					run_mod_one(sel, counts[c], seeds[s],
						    widxs[w], w == 0);
	/*
	 * The selector is SIGNED.  Seeded at -1, the object must reach the
	 * slot below the table; an unsigned reading would index 262,140 bytes
	 * past the block, so this is separated by construction and what is
	 * counted is that the negative arm was reached at all.
	 */
	/*
	 * The count is ZERO because table[-1] is the shaper's `hist_q`; see
	 * V17FP_ENCODER_M1 above.  The encoder is still dispatched and still
	 * writes its slot mark into `sym[0]`, which is compared.
	 */
	run_mod_one(-1, 0, 0x0badf00du, 0, 0);
	run_mod_one(-1, 0, 0x0badf00du, 47, 0);
	return diff_end();
}

/* --------------------------------------------------------------------- */

/*
 * The far corner: a cursor at 32767, over a buffer with 32,768 shorts of
 * headroom BELOW the pointer the ring holds, so `(short)(widx + 1)` wrapping
 * to -32768 addresses real memory and can be compared instead of crashing.
 */
static short big_a[65536 + 16], big_b[65536 + 16];

static void
run_tx_one(int n, unsigned seed, short widx, short len, short *sym_a,
	   short *sym_b, int alt)
{
	unsigned short ra, rb;
	long where = (long)n * 1000 + (long)(widx & 0x3ff);
	int i;

	fixture(&ma, seed, widx, (short)(seed % 7), 0, sym_a, len);
	fixture(&mb, seed, widx, (short)(seed % 7), 0, sym_b, len);

	rng_seed(seed ^ 0x13572468u);
	for (i = 0; i < NDATA; i++)
		mdata[i] = (unsigned short)(rng_next() | 0x8000u);

	for (i = 0; i < NOUT; i++)
		oa[i] = ob[i] = (short)OMARK;

	ra = ref_TxNoCarrierV17(ma.obj, mdata, oa, (unsigned short)n);
	rb = TxNoCarrierV17(mb.obj, mdata, ob, (unsigned short)n);

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
	diff_eq_int("at %ld: first differing parameter byte",
		    prm_first_diff(&mb, &ma), -1, where);
	compare_hist(&ma, &mb, where);

	if (sym_a == 0) {
		for (i = 0; i < RING_LEN; i++) {
			diff_eq_int("ring sym[%ld]", mb.sym[i], ma.sym[i], i);
			diff_eq_int("ring i[%ld]", mb.ri[i], ma.ri[i], i);
			diff_eq_int("ring q[%ld]", mb.rq[i], ma.rq[i], i);
		}
	} else {
		for (i = 0; i < 65536 + 16; i++)
			if (sym_a[i - 32768] != sym_b[i - 32768]) {
				diff_eq_int("far ring sym[%ld]",
					    sym_b[i - 32768],
					    sym_a[i - 32768], i - 32768);
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

	/*
	 * The wrong readings, each evaluated by filling the ring the wrong way
	 * and shaping it with the blob's own filter.
	 */
	{
		struct fpm_smc_ring *r;
		unsigned short rc_alt;
		short w;
		int k;

		/* WRONG READING: the symbol from prm + 0x1c. */
		fixture(&mc, seed, widx, (short)(seed % 7), 0, 0, len);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->sym[w] = (short)*(unsigned short *)(void *)
					(mc.prm + V17TXP_NOCARRIER_SYM - 2);
			next = (short)(w + 1);
			w = next < r->len ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V17FP_PPS, r, oc,
					    (unsigned short)n);
		r->widx = w;
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			tx_off_sep++;

		/* WRONG READING: the rails written rather than `sym`. */
		fixture(&mc, seed, widx, (short)(seed % 7), 0, 0, len);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;
			short v = (short)*(unsigned short *)(void *)
					(mc.prm + V17TXP_NOCARRIER_SYM);

			r->i[w] = v;
			r->q[w] = v;
			next = (short)(w + 1);
			w = next < r->len ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V17FP_PPS, r, oc,
					    (unsigned short)n);
		r->widx = w;
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			tx_rail_sep++;

		/* WRONG READING: the cursor wrapped at len - 1. */
		fixture(&mc, seed, widx, (short)(seed % 7), 0, 0, len);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->sym[w] = (short)*(unsigned short *)(void *)
					(mc.prm + V17TXP_NOCARRIER_SYM);
			next = (short)(w + 1);
			w = next < (short)(r->len - 1) ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V17FP_PPS, r, oc,
					    (unsigned short)n);
		r->widx = w;
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0
		    || r->widx != ring_of(&ma)->widx)
			tx_wrap_sep++;

		/* WRONG READING: the cursor never written back. */
		if (ring_of(&ma)->widx != widx)
			tx_wb_sep++;

		/*
		 * WRONG READING: the second argument used as the symbol.
		 * Every word of `mdata` has bit 15 set, so it is never one of
		 * the values the parameter block can hold here.
		 */
		fixture(&mc, seed, widx, (short)(seed % 7), 0, 0, len);
		r = ring_of(&mc);
		w = r->widx;
		for (k = 0; k < n; k++) {
			short next;

			r->sym[w] = (short)mdata[k % NDATA];
			next = (short)(w + 1);
			w = next < r->len ? next : 0;
		}
		for (i = 0; i < NOUT; i++)
			oc[i] = (short)OMARK;
		rc_alt = ref_FPM_PPS_filter(mc.fp + V17FP_PPS, r, oc,
					    (unsigned short)n);
		if (rc_alt != ra
		    || memcmp(oc, oa, (size_t)ra * sizeof(oc[0])) != 0)
			tx_arg2_sep++;
	}
}

static int
run_tx(void)
{
	static const int counts[] = { 0, 1, 2, 3, 7, 20, 49, 50, 51, 120 };
	static const unsigned seeds[] = { 0x0f0f1e1eu, 0x76543210u };
	static const short widxs[] = { 0, 1, 25, 48, 49 };
	int c, s, w, i;

	diff_begin("TxNoCarrierV17");
	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++)
		for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++)
			for (w = 0; w < (int)(sizeof(widxs)
					     / sizeof(widxs[0])); w++)
				run_tx_one(counts[c], seeds[s], widxs[w], 0, 0,
					   0, 1);

	/*
	 * And the far corner.  `len` is 32767 and the cursor starts at 32767,
	 * so the first store is in range and the wrap goes to -32768 -- which
	 * the object reaches by truncating `widx + 1` to a short and nothing
	 * else does.
	 */
	for (i = 0; i < 65536 + 16; i++)
		big_a[i] = big_b[i] = (short)((i * 29) & 0x7f);
	run_tx_one(4, 0x2b2b2b2bu, 32767, 32767, big_a + 32768, big_b + 32768,
		   0);
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

	/*
	 * The separating counts.  Each is the number of trials on which a
	 * NAMED wrong reading produced a different OBSERVABLE answer -- a
	 * returned count, a compared sample, or a cursor that is compared --
	 * from the blob's.  A zero here means the corresponding check above
	 * is decoration.
	 */
	diff_begin("v17data separating trials");
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
	diff_eq_int("ModDataV17 returned samples (%ld)", mod_ret_nonzero > 0,
		    1, mod_ret_nonzero);
	diff_eq_int("the ring wrapped under the encoder (%ld)",
		    mod_ring_wrapped > 0, 1, mod_ring_wrapped);

	diff_eq_int("the no-carrier symbol's offset separates (%ld)",
		    tx_off_sep > 0, 1, tx_off_sep);
	diff_eq_int("writing the rails instead of sym separates (%ld)",
		    tx_rail_sep > 0, 1, tx_rail_sep);
	diff_eq_int("wrapping at len - 1 separates (%ld)", tx_wrap_sep > 0, 1,
		    tx_wrap_sep);
	diff_eq_int("the cursor write-back is observable (%ld)",
		    tx_wb_sep > 0, 1, tx_wb_sep);
	diff_eq_int("using the second argument separates (%ld)",
		    tx_arg2_sep > 0, 1, tx_arg2_sep);
	diff_eq_int("the cursor wrapped (%ld)", tx_wrapped > 0, 1, tx_wrapped);
	diff_eq_int("TxNoCarrierV17 returned samples (%ld)",
		    tx_ret_nonzero > 0, 1, tx_ret_nonzero);
	diff_eq_int("the short truncation corner was reached (%ld)",
		    tx_trunc_trials > 0, 1, tx_trunc_trials);
	rc |= diff_end();

	return rc;
}
