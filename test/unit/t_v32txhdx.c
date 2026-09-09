/*
 * t_v32txhdx.c -- differential test of V.32's eight half-duplex TRANSMIT
 *                 states.
 *
 * All eight are THIN: each clamps a count, charges a countdown, calls one to
 * three already-reconstructed leaves, maybe transitions, and reports a sample
 * count.  So what can be wrong is WHICH field, WHICH quantity and WHICH
 * ORDER, and none of those shows up unless the fixture is built to make them
 * different from one another.  Every check below is built around a NAMED
 * WRONG READING and the number of trials that SEPARATE each one is asserted
 * non-zero at the end.
 *
 * THE FIVE FIELDS ARE ALL GIVEN DIFFERENT VALUES, ALWAYS.  A fixture that
 * sets any two of them equal cannot tell the two readings apart:
 *
 *   hdx + 0x78  the handshake state's countdown, an `int`
 *   hdx + 0x84  the per-block charge against it, a `short`
 *   hdx + 0x76  the mode, which indexes V32NextState
 *   hdx + 0x9e  symbols per block          -- never read by these eight
 *   hdx + 0xa0  samples per block          -- TxHdxTone and TxHdxNull's return
 *
 * and `*left` is a sixth quantity again, distinct from all of them.
 *
 * THE NAMED WRONG READINGS
 *
 *   - the countdown charged `count` instead of `*left`.  Six of the states
 *     subtract the symbols they were OFFERED, not the clamped count they
 *     actually asked for, so a trial with `hdx->left < *left` leaves -7 in
 *     the object and 0 in the wrong reading.  Both transition, so only the
 *     countdown's VALUE separates them: the whole context is compared.
 *   - the clamp read as a `min` over `int`, or clamped at zero.  The object
 *     compares SIGNED and truncates the losing arm to sixteen bits UNSIGNED,
 *     so a countdown of -65530 yields a count of 6 rather than 0 or -65530.
 *   - the same clamp in TxHdxNoCarrier, which ALONE forces the count to zero
 *     once the countdown has expired.  The -65530 trial therefore has to
 *     produce 6 for five states and 0 for that one, and the trace records
 *     which.
 *   - the transition taken on `< 0` rather than `<= 0`.  Trials land the
 *     countdown on exactly zero.
 *   - the sample count read BEFORE the transition.  The scripted dispatcher
 *     swaps obj + 0x64 to an alternate context whose +0xa0 differs, so
 *     TxHdxTone and TxHdxNull must report the NEW context's block length.
 *   - the context cached across the MODULATOR.  The encoder stub swaps
 *     obj + 0x64 mid-call, so the four states that reach `ModDataV32` must
 *     test the NEW context's countdown afterwards; a cached one transitions
 *     where the object does not.
 *   - `*left` cached across the transition.  The dispatcher rewrites it, and
 *     the six subtracting states must subtract `count` from the REWRITTEN
 *     value.
 *   - `*left` reduced by itself instead of by `count`, and TxHdxFinishFrame
 *     zeroing it where the others reduce it.
 *   - the wrong leaf, or the right leaves in the wrong order.  `GenSequence`
 *     OVERWRITES `data` and the scrambler is stateful, so generate-then-
 *     scramble and scramble-then-generate leave different words in `data`
 *     AND a different shift register in the fp block.  TxHdxTRN's fold sits
 *     between the scrambler and the modulator and maps {0,1,2,3} to
 *     {0,0,3,3}; applying it on the other side of either call is visible in
 *     `data`.
 *   - the tone taken from hdx + 0x30 or + 0x34 instead of + 0x2c.  Three
 *     DIFFERENT tone objects are installed and only the first is right.
 *   - TxHdxNoCarrier / TxHdxFinishFrame calling `ModDataV32` rather than
 *     `TxNoCarrierV32`.  The two write different symbols into the ring.
 *
 * WHAT THE FIXTURE IS.  `ModDataV32` and `TxNoCarrierV32` reach a whole
 * datapump block, so this file borrows `t_v32data.c`'s recipe wholesale: a
 * `struct v32_smc`, a symbol ring, an `FPM_PPS_init`ed shaper over
 * `PPSv32_CFG`'s own shape, and STUB encoders in the table at fp + 0x98 --
 * stubs rather than the real arms because a stub can write a slot mark, so a
 * wrong table base reaches the samples instead of only a log.  The scrambler
 * at fp + 0x30 and the sequence generator at hdx + 0x4a are the objects their
 * own tests use.
 *
 * WHY THE DISPATCHERS ARE SCRIPTED.  `V32NextState`'s six real entries are
 * the five `V32*NextState` machines, each hundreds of instructions over state
 * this fixture does not model.  Both tables -- ours and the blob's
 * `ref_V32NextState` -- are overwritten with six recording stubs, one per
 * slot, so the trace says WHICH slot ran and a wrong `hdx->mode` read is a
 * failure rather than a coincidence.  They are restored afterwards.
 *
 * WHAT IS NOT TESTED, AND WHY.  A state's return is `short` while
 * `ModDataV32` is `unsigned short`; the two readings differ only above 32,767
 * samples in one call and reach the caller as the same sixteen bits either
 * way, so no test at this boundary can separate them -- the object's `movswl`
 * is the only evidence and it is tier 3.  TxHdxNull's zero-fill loop is
 * `while (i-- != 0)`, which for a NEGATIVE hdx + 0xa0 runs 2**32 - |n| times
 * in the object exactly as it does here; that input is deliberately not
 * driven, and it is a property of the original rather than of this harness.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/v32hdxst.h"
#include "dsplib/v32data.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32smc.h"
#include "dsplib/v32scram.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_tone.h"

/* ------------------------------------------------------------------ blob */

extern short ref_TxHdxTone(void *modem, short *data, short *out,
			   unsigned short *left);
extern short ref_TxHdxCarrierState(void *modem, short *data, short *out,
				   unsigned short *left);
extern short ref_TxHdxScrSequence(void *modem, short *data, short *out,
				  unsigned short *left);
extern short ref_TxHdxTRN(void *modem, short *data, short *out,
			  unsigned short *left);
extern short ref_TxHdxData(void *modem, short *data, short *out,
			   unsigned short *left);
extern short ref_TxHdxNoCarrier(void *modem, short *data, short *out,
				unsigned short *left);
extern short ref_TxHdxFinishFrame(void *modem, short *data, short *out,
				  unsigned short *left);
extern short ref_TxHdxNull(void *modem, short *data, short *out,
			   unsigned short *left);

extern v32_nextstate_fn ref_V32NextState[V32_NEXTSTATE_COUNT];

extern void ref_FPM_PPS_init(struct fpm_pps *state,
			     const struct fpm_pps_cfg *cfg, int fresh);

/* --------------------------------------------------------------- offsets */

/*
 * The two fields `src/pump/v32/V32TXHDX.c` derives; spelled again here rather
 * than exported, and guarded in case a sibling header grows them.
 */
#ifndef V32HDX_STATE_LEFT
#define V32HDX_STATE_LEFT	0x78	/* int, <= 0 transitions              */
#endif
#ifndef V32HDX_BLOCK_CHARGE
#define V32HDX_BLOCK_CHARGE	0x84	/* short, the whole-block charge      */
#endif

/* --------------------------------------------------------------- fixture */

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x120
#define FP_SIZE		0x100

#define RING_MAX	64
#define NDATA		256
#define GUARD		64
#define NOUT		(GUARD + 2048)
#define OMARK		((short)0x5ead)

#define MAP_LEN		256
#define PPS_PHASES	10
#define PPS_COEFFS	120
#define PPS_TAPS	(PPS_COEFFS / PPS_PHASES)
#define PPS_SCALE	131072		/* PPSv32_CFG + 0x08 */

/* The two regions that hold a POINTER and so differ between the two sides. */
#define RING_PTR_LO	(V32FP_SYMOUT + 0x00)
#define RING_PTR_HI	(V32FP_SYMOUT + 0x0c)
#define PPS_HIST_LO	(V32FP_PPS + 0x30)
#define PPS_HIST_HI	(V32FP_PPS + 0x38)

#define NSMAX		4

enum {
	S_TONE, S_CARRIER, S_SCRSEQ, S_TRN,
	S_DATA, S_NOCARRIER, S_FINISH, S_NULL,
	S_COUNT
};

static const char *const state_name[S_COUNT] = {
	"TxHdxTone", "TxHdxCarrierState", "TxHdxScrSequence", "TxHdxTRN",
	"TxHdxData", "TxHdxNoCarrier", "TxHdxFinishFrame", "TxHdxNull"
};

typedef short (*txfn)(void *modem, short *data, short *out,
		      unsigned short *left);

static const txfn ours[S_COUNT] = {
	TxHdxTone, TxHdxCarrierState, TxHdxScrSequence, TxHdxTRN,
	TxHdxData, TxHdxNoCarrier, TxHdxFinishFrame, TxHdxNull
};

static const txfn refs[S_COUNT] = {
	ref_TxHdxTone, ref_TxHdxCarrierState, ref_TxHdxScrSequence,
	ref_TxHdxTRN, ref_TxHdxData, ref_TxHdxNoCarrier,
	ref_TxHdxFinishFrame, ref_TxHdxNull
};

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	alt[HDX_SIZE];
	unsigned char	fp[FP_SIZE];
	struct fpm_tone	tone[3];
	short		sym[RING_MAX];
	short		ri[RING_MAX];
	short		rq[RING_MAX];
	short		dbuf[NDATA];
	short		obuf[NOUT];
	double		align;
};

/* Named images, because diff_eq_obj stringifies its type for whichfield.py. */
struct out_image { short s[NOUT]; };
struct data_image { short s[NDATA]; };
struct ring_image { short s[RING_MAX]; };
struct tone_image { struct fpm_tone t[3]; };

static struct fix fa, fb;

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
put_int(unsigned char *p, int off, int v)
{
	*(int *)(void *)(p + off) = v;
}

static void *
get_ptr(const unsigned char *p, int off)
{
	return *(void *const *)(const void *)(p + off);
}

static int
get_int(const unsigned char *p, int off)
{
	return *(const int *)(const void *)(p + off);
}

/* ------------------------------------------------------------ the stubs */

/*
 * The encoder stubs, taken from t_v32data.c: a slot mark in a compared byte,
 * so a wrong table base reaches the output samples and not only a log.
 */
struct enc_log {
	int		calls;
	int		slot;
	unsigned short	count;
};

static struct enc_log elog[2];
static int cur_side;			/* 0 = ours, 1 = the blob */
static struct fix *cur_fix;

/*
 * The encoder stub doubles as the only hook INSIDE a callee, and it is used
 * for one further named wrong reading: the context cached across the
 * modulator.  The four states that reach `ModDataV32` re-read obj + 0x64
 * afterwards, so a stub that swaps the context there changes which countdown
 * the transition test reads.  `TxHdxNoCarrier` and `TxHdxFinishFrame` call
 * `TxNoCarrierV32`, which has no such hook, and `TxHdxTone` and `TxHdxNull`
 * call neither -- their reload is separated by the dispatcher's swap instead.
 */
static int ns_encswap;
static int enc_swapped;

static void
enc_common(int slot, struct v32_symout *out, short *data, unsigned short count)
{
	unsigned short k;

	if (ns_encswap && !enc_swapped) {
		enc_swapped = 1;
		put_ptr(cur_fix->obj, V32_OBJ_HDX, (void *)cur_fix->alt);
	}

	elog[cur_side].calls++;
	elog[cur_side].slot = slot;
	elog[cur_side].count = count;

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
	(void)s;
	enc_common(0, o, d, n);
}

static void
enc1(struct v32_smc *s, struct v32_symout *o, short *d, unsigned short n)
{
	(void)s;
	enc_common(1, o, d, n);
}

static void
enc2(struct v32_smc *s, struct v32_symout *o, short *d, unsigned short n)
{
	(void)s;
	enc_common(2, o, d, n);
}

/* ---------------------------------------------------- the dispatchers */

struct nscall {
	long	slot;		/* which V32NextState entry ran           */
	long	obj_ok;		/* the argument was this side's instance  */
	long	ctx_alt;	/* obj + 0x64 named the alternate context */
	long	budget_in;	/* hdx + 0x78 as the dispatcher saw it    */
	long	left_in;	/* *left as the dispatcher saw it         */
};

static struct nscall nslog[2][NSMAX];
static int nsn[2];

static unsigned short *cur_left;	/* the `left` this side was handed */
static int ns_swap;			/* swap obj + 0x64 to `alt`        */
static long ns_setleft;			/* -1 none, else write into *left  */

static v32_nextstate_fn saved_ours[V32_NEXTSTATE_COUNT];
static v32_nextstate_fn saved_ref[V32_NEXTSTATE_COUNT];

static void
ns_body(int slot, void *modem)
{
	unsigned char *hdx = get_ptr(cur_fix->obj, V32_OBJ_HDX);

	if (nsn[cur_side] < NSMAX) {
		struct nscall *c = &nslog[cur_side][nsn[cur_side]];

		c->slot = slot;
		c->obj_ok = (modem == (void *)cur_fix->obj);
		c->ctx_alt = (hdx == cur_fix->alt);
		c->budget_in = get_int(hdx, V32HDX_STATE_LEFT);
		c->left_in = (long)*cur_left;
	}
	nsn[cur_side]++;

	if (ns_swap)
		put_ptr(cur_fix->obj, V32_OBJ_HDX, (void *)cur_fix->alt);
	if (ns_setleft >= 0)
		*cur_left = (unsigned short)ns_setleft;
}

static void ns0(void *m) { ns_body(0, m); }
static void ns1(void *m) { ns_body(1, m); }
static void ns2(void *m) { ns_body(2, m); }
static void ns3(void *m) { ns_body(3, m); }
static void ns4(void *m) { ns_body(4, m); }
static void ns5(void *m) { ns_body(5, m); }

static void
install_dispatchers(void)
{
	static const v32_nextstate_fn stub[V32_NEXTSTATE_COUNT] = {
		ns0, ns1, ns2, ns3, ns4, ns5
	};
	int i;

	for (i = 0; i < V32_NEXTSTATE_COUNT; i++) {
		saved_ours[i] = V32NextState[i];
		saved_ref[i] = ref_V32NextState[i];
		V32NextState[i] = stub[i];
		ref_V32NextState[i] = stub[i];
	}
}

static void
restore_dispatchers(void)
{
	int i;

	for (i = 0; i < V32_NEXTSTATE_COUNT; i++) {
		V32NextState[i] = saved_ours[i];
		ref_V32NextState[i] = saved_ref[i];
	}
}

/* --------------------------------------------------------------- fixture */

struct trial {
	const char	*what;
	unsigned short	left;		/* *left on entry                    */
	int		budget;		/* hdx + 0x78                        */
	short		charge;		/* hdx + 0x84                        */
	short		symlen;		/* hdx + 0x9e -- never read by these */
	short		samplen;	/* hdx + 0xa0                        */
	short		altsamplen;	/* the alternate context's + 0xa0    */
	short		mode;		/* hdx + 0x76                        */
	short		limit;		/* the symbol ring's length          */
	short		sel;		/* the encoder selector, fp + 0xa4   */
	short		group;		/* the scrambler's bits per symbol   */
	int		swap;		/* the dispatcher swaps the context  */
	long		setleft;	/* ... and rewrites *left            */
	unsigned	seed;
	int		encswap;	/* the MODULATOR swaps the context   */
};

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
 * A tone object built by hand rather than by `FPM_TONE_create`, because
 * `FPM_TONE_generate` reads exactly four fields and create would put two heap
 * pointers into a structure this test compares byte for byte.  `kernel`,
 * `history` and `cfg.src` stay NULL on both sides and are never dereferenced.
 */
static void
make_tone(struct fpm_tone *t, int which, unsigned seed)
{
	memset(t, 0, sizeof(*t));
	t->cfg.scale = (short)(4096 + which * 1500);
	t->cfg.rev_period = (short)(which == 1 ? 0 : 3 + which);
	t->inc = (unsigned short)(1200 + which * 700 + (seed & 0x3f));
	t->phase = (unsigned short)((seed >> 6) & 0x7fff);
	t->rev_count = (unsigned short)((seed >> 3) & 0x0f);
}

static void
fixture(struct fix *f, const struct trial *t)
{
	struct fpm_pps_cfg cfg;
	struct v32_smc smc;
	struct v32_sdm sdm;
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(t->seed);
	for (i = 0; i < RING_MAX; i++) {
		f->sym[i] = (short)(rng_next() & 0x1f);
		f->ri[i] = (short)(rng_next() % 20001u) - 10000;
		f->rq[i] = (short)(rng_next() % 20001u) - 10000;
	}
	for (i = 0; i < NDATA; i++)
		f->dbuf[i] = (short)(rng_next() & 0x3fff);
	for (i = 0; i < NOUT; i++)
		f->obuf[i] = OMARK;

	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);
	put_ptr(f->obj, V32_OBJ_FP, f->fp);

	/* --- the handshake context ------------------------------------ */

	for (i = 0; i < 3; i++)
		make_tone(&f->tone[i], i, t->seed + (unsigned)i * 0x9e3779b9u);
	put_ptr(f->hdx, V32_HDX_TONE0, &f->tone[0]);
	put_ptr(f->hdx, V32_HDX_TONE1, &f->tone[1]);
	put_ptr(f->hdx, V32_HDX_TONE2, &f->tone[2]);

	/* The sequence generator, as `InitGenSequence` would leave it. */
	put_short(f->hdx, V32HDX_GEN_INDEX, (short)((t->seed >> 2) & 3));
	put_short(f->hdx, V32HDX_GEN_INDEX_MASK, 3);
	put_short(f->hdx, V32HDX_GEN_WIDTH, 4);
	put_short(f->hdx, V32HDX_GEN_MASK, 0x0f);
	put_short(f->hdx, V32HDX_GEN_PATTERN, (short)(t->seed | 0x1249));

	put_short(f->hdx, V32HDX_MODE, t->mode);
	put_short(f->hdx, V32HDX_STATE, 7);
	put_int(f->hdx, V32HDX_STATE_LEFT, t->budget);
	put_short(f->hdx, V32HDX_BLOCK_CHARGE, t->charge);
	put_short(f->hdx, V32HDX_SYMBOL_LEN, t->symlen);
	put_short(f->hdx, V32HDX_SAMPLE_LEN, t->samplen);

	/*
	 * The alternate context the dispatcher swaps in.  Only + 0xa0 is ever
	 * read from it -- by TxHdxTone and TxHdxNull, after the transition --
	 * but every field is given a DIFFERENT value so that a state reading
	 * any other one from the wrong context is caught too.
	 */
	put_ptr(f->alt, V32_HDX_TONE0, &f->tone[2]);
	put_ptr(f->alt, V32_HDX_TONE1, &f->tone[0]);
	put_ptr(f->alt, V32_HDX_TONE2, &f->tone[1]);
	put_short(f->alt, V32HDX_MODE, (short)((t->mode + 3) % 6));
	put_short(f->alt, V32HDX_STATE, 11);
	put_int(f->alt, V32HDX_STATE_LEFT, t->budget + 0x4321);
	put_short(f->alt, V32HDX_BLOCK_CHARGE, (short)(t->charge ^ 0x2a));
	put_short(f->alt, V32HDX_SYMBOL_LEN, (short)(t->symlen ^ 0x17));
	put_short(f->alt, V32HDX_SAMPLE_LEN, t->altsamplen);

	/* --- the datapump block --------------------------------------- */

	memset(&smc, 0, sizeof(smc));
	smc.mode = (short)(t->sel < 0 ? 0 : t->sel);
	smc.shift = (short)(t->seed % 4u);
	smc.quad = (short)((t->seed >> 3) % 4u);
	smc.state[0] = (short)((t->seed >> 5) % 4u);
	smc.state[1] = (short)((t->seed >> 7) % 4u);
	smc.state[2] = (short)((t->seed >> 9) % 4u);
	smc.f0e = (short)((t->seed >> 11) % 8u);
	smc.f10 = (short)((t->seed >> 13) % 8u);
	smc.f14 = (unsigned short)((t->seed >> 15) % 4u);
	memcpy(f->fp + V32FP_SMC, &smc, sizeof(smc));

	/* The transmit scrambler at fp + 0x30, as t_v32scram.c drives it. */
	memset(&sdm, 0, sizeof(sdm));
	sdm.group = t->group;
	sdm.outmask = (unsigned int)((1u << t->group) - 1u);
	sdm.regmask = 0x7fffffu;
	sdm.reg = t->seed | 0x00010001u;
	sdm.tap1 = 17;
	sdm.tap2 = 22;
	memcpy(f->fp + V32FP_SCRAMBLER, &sdm, sizeof(sdm));

	make_cfg(&cfg);
	ref_FPM_PPS_init((struct fpm_pps *)(void *)(f->fp + V32FP_PPS), &cfg,
			 1);

	put_ptr(f->fp, V32FP_ENCODERS + 0, (void *)enc0);
	put_ptr(f->fp, V32FP_ENCODERS + 4, (void *)enc1);
	put_ptr(f->fp, V32FP_ENCODERS + 8, (void *)enc2);
	put_short(f->fp, V32FP_ENCODER_SEL, t->sel);

	put_ptr(f->fp, V32FP_SYMOUT + 0x00, f->ri);
	put_ptr(f->fp, V32FP_SYMOUT + 0x04, f->rq);
	put_ptr(f->fp, V32FP_SYMOUT + 0x08, f->sym);
	put_short(f->fp, V32FP_SYMOUT + 0x0c, (short)(t->seed % 5u));
	put_short(f->fp, V32FP_SYMOUT + 0x0e, (short)(t->seed % 3u));
	put_short(f->fp, V32FP_SYMOUT + 0x10, t->limit);
}

/* ------------------------------------------------------------- compares */

static struct fpm_pps *
pps_of(struct fix *f)
{
	return (struct fpm_pps *)(void *)(f->fp + V32FP_PPS);
}

/*
 * The three blocks that hold a POINTER are compared with a skipping loop --
 * CLAUDE.md's exception, since the two sides hold two different addresses and
 * always will.  Everything else goes through diff_eq_obj.
 */
static long
hdx_first_diff(const unsigned char *a, const unsigned char *b)
{
	int i;

	for (i = 0; i < HDX_SIZE; i++) {
		if (i >= V32_HDX_TONE0 && i < V32_HDX_MTD + 4)
			continue;
		if (a[i] != b[i])
			return i;
	}
	return -1;
}

static long
fp_first_diff(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < FP_SIZE; i++) {
		if (i >= RING_PTR_LO && i < RING_PTR_HI)
			continue;
		if (i >= PPS_HIST_LO && i < PPS_HIST_HI)
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
		if (i >= V32_OBJ_HDX && i < V32_OBJ_FP + (int)sizeof(void *))
			continue;
		if (a->obj[i] != b->obj[i])
			return i;
	}
	return -1;
}

/* ------------------------------------------------------- non-vacuity */

static int rc_total;

static long sep_transition, sep_no_transition, sep_exact_zero;
static long sep_clamped, sep_truncating, sep_nocarrier_guard;
static long sep_ctx_swap, sep_ctx_swap_seen, sep_left_rewritten;
static long sep_left_nonzero_after, sep_left_zeroed;
static long sep_charged_offered, sep_samples, sep_data_changed;
static long sep_enc_ctx_swap;
static long sep_mode_seen[V32_NEXTSTATE_COUNT];
static long sep_fields_distinct, sep_encoder_ran, sep_zero_count;

/* ---------------------------------------------------------------- driver */

static void
run_one(const struct trial *t, int which)
{
	unsigned short la = t->left, lb = t->left;
	short na, nb;
	char label[160];
	short pristine[NDATA];
	long budget_a, budget_b;
	int i;

	ns_swap = t->swap;
	ns_setleft = t->setleft;
	ns_encswap = t->encswap;
	nsn[0] = nsn[1] = 0;
	memset(nslog, 0, sizeof(nslog));
	memset(elog, 0, sizeof(elog));

	fixture(&fa, t);
	memcpy(pristine, fa.dbuf, sizeof(pristine));
	cur_side = 0;
	cur_fix = &fa;
	cur_left = &la;
	enc_swapped = 0;
	na = ours[which](fa.obj, fa.dbuf, fa.obuf + GUARD, &la);

	fixture(&fb, t);
	cur_side = 1;
	cur_fix = &fb;
	cur_left = &lb;
	enc_swapped = 0;
	nb = refs[which](fb.obj, fb.dbuf, fb.obuf + GUARD, &lb);

	sprintf(label, "%s: %s", state_name[which], t->what);
	diff_begin(label);

	diff_eq_int("samples returned (%ld)", na, nb, na);
	diff_eq_int("*left afterwards (%ld)", la, lb, la);

	diff_eq_int("transitions (%ld)", nsn[0], nsn[1], nsn[0]);
	for (i = 0; i < nsn[0] && i < NSMAX; i++) {
		diff_eq_int("transition %ld: which V32NextState slot",
			    nslog[0][i].slot, nslog[1][i].slot, i);
		diff_eq_int("transition %ld: the instance pointer",
			    nslog[0][i].obj_ok, 1, i);
		diff_eq_int("transition %ld: the instance pointer (blob)",
			    nslog[1][i].obj_ok, 1, i);
		diff_eq_int("transition %ld: which context",
			    nslog[0][i].ctx_alt, nslog[1][i].ctx_alt, i);
		diff_eq_int("transition %ld: the countdown on entry",
			    nslog[0][i].budget_in, nslog[1][i].budget_in, i);
		diff_eq_int("transition %ld: *left on entry",
			    nslog[0][i].left_in, nslog[1][i].left_in, i);
	}

	diff_eq_int("encoder calls (%ld)", elog[0].calls, elog[1].calls,
		    elog[0].calls);
	diff_eq_int("encoder arm (%ld)", elog[0].slot, elog[1].slot,
		    elog[0].slot);
	diff_eq_int("encoder count (%ld)", elog[0].count, elog[1].count,
		    elog[0].count);

	diff_eq_int("first differing context byte (%ld)",
		    hdx_first_diff(fa.hdx, fb.hdx), -1, 0);
	diff_eq_int("first differing alternate-context byte (%ld)",
		    hdx_first_diff(fa.alt, fb.alt), -1, 0);
	diff_eq_int("first differing datapump byte (%ld)",
		    fp_first_diff(&fa, &fb), -1, 0);
	diff_eq_int("first differing instance byte (%ld)",
		    obj_first_diff(&fa, &fb), -1, 0);
	diff_eq_int("the context pointer names the same context (%ld)",
		    get_ptr(fa.obj, V32_OBJ_HDX) == (void *)fa.alt,
		    get_ptr(fb.obj, V32_OBJ_HDX) == (void *)fb.alt, 0);

	diff_eq_obj("the output buffer", struct out_image, fa.obuf, fb.obuf, 0);
	diff_eq_obj("the data buffer", struct data_image, fa.dbuf, fb.dbuf, 0);
	diff_eq_obj("the symbol ring", struct ring_image, fa.sym, fb.sym, 0);
	diff_eq_obj("the ring's i rail", struct ring_image, fa.ri, fb.ri, 0);
	diff_eq_obj("the ring's q rail", struct ring_image, fa.rq, fb.rq, 0);
	diff_eq_obj("the tone objects", struct tone_image, fa.tone, fb.tone, 0);

	for (i = 0; i < PPS_TAPS; i++) {
		diff_eq_int("shaper hist_i[%ld]", pps_of(&fa)->hist_i[i],
			    pps_of(&fb)->hist_i[i], i);
		diff_eq_int("shaper hist_q[%ld]", pps_of(&fa)->hist_q[i],
			    pps_of(&fb)->hist_q[i], i);
	}

	/* ------------------------------------------- what this trial did */

	budget_a = get_int(fa.hdx, V32HDX_STATE_LEFT);
	budget_b = get_int(fb.hdx, V32HDX_STATE_LEFT);
	diff_eq_int("the countdown afterwards (%ld)", budget_a, budget_b,
		    budget_a);

	if (nsn[0] > 0) {
		sep_transition++;
		if (nslog[0][0].slot >= 0
		    && nslog[0][0].slot < V32_NEXTSTATE_COUNT)
			sep_mode_seen[nslog[0][0].slot]++;
		if (nslog[0][0].budget_in == 0)
			sep_exact_zero++;
		if (t->swap) {
			sep_ctx_swap++;
			if ((which == S_TONE || which == S_NULL)
			    && t->altsamplen != t->samplen
			    && na == t->altsamplen)
				sep_ctx_swap_seen++;
		}
		if (t->setleft >= 0 && la != (unsigned short)t->setleft)
			sep_left_rewritten++;
	} else {
		sep_no_transition++;
	}

	if (t->charge != t->symlen && t->symlen != t->samplen
	    && t->charge != t->samplen)
		sep_fields_distinct++;
	if (elog[0].calls > 0) {
		sep_encoder_ran++;
		if (elog[0].count == 0)
			sep_zero_count++;
	}
	if (na != 0)
		sep_samples++;
	if (la != 0)
		sep_left_nonzero_after++;
	if (which == S_FINISH && la == 0 && t->left != 0)
		sep_left_zeroed++;
	if (memcmp(fa.dbuf, pristine, sizeof(pristine)) != 0)
		sep_data_changed++;

	/*
	 * The clamp's two interesting shapes, judged from the trial rather
	 * than from the result, so a broken state cannot make them read as
	 * exercised.
	 */
	if (which != S_TONE && which != S_NULL && which != S_FINISH) {
		if (t->budget >= 0 && t->budget < (int)t->left)
			sep_clamped++;
		if (t->budget < 0
		    && (t->budget & 0xffff) != 0
		    && (unsigned)(t->budget & 0xffff) <= t->left) {
			if (which == S_NOCARRIER)
				sep_nocarrier_guard++;
			else
				sep_truncating++;
		}
	}
	/*
	 * Charging `*left` and charging `count` differ only where the clamp
	 * actually bit, and TxHdxFinishFrame's count IS `*left`, so neither it
	 * nor the two block-wise states can separate the two readings.
	 */
	if (which != S_TONE && which != S_NULL && which != S_FINISH
	    && t->budget < (int)t->left)
		sep_charged_offered++;

	/*
	 * The context cached across the modulator.  The trial's countdown
	 * would have expired had the state read the OLD context, so a
	 * transition that did not happen is the evidence that it re-read.
	 */
	if (t->encswap && which >= S_CARRIER && which <= S_DATA
	    && t->budget - (int)t->left <= 0 && nsn[0] == 0)
		sep_enc_ctx_swap++;

	rc_total |= diff_end();
}

/* ---------------------------------------------------------------- trials */

/*
 * Every row keeps `charge`, `symlen`, `samplen` and `left` mutually distinct.
 * `samplen` is never negative -- see the file header for why.
 */
static const struct trial trials[] = {
	/* what, left, budget, charge, symlen, samplen, altsamplen,
	   mode, limit, sel, group, swap, setleft, seed */
	{ "the countdown outlives the block",
	  12, 200, 48, 24, 40, 88, 0, 12, 0, 4, 0, -1, 0x11111111u, 0 },
	{ "the countdown lands on exactly zero",
	  12, 12, 12, 24, 40, 88, 1, 12, 1, 4, 0, -1, 0x22222222u, 0 },
	{ "the countdown is spent and the block is not",
	  12, 5, 5, 24, 40, 88, 2, 12, 2, 4, 0, -1, 0x33333333u, 0 },
	{ "the countdown is already zero",
	  12, 0, 7, 24, 40, 88, 3, 12, 0, 4, 0, -1, 0x44444444u, 0 },
	{ "the countdown has gone negative, low half small",
	  20, -65530, 9, 24, 40, 88, 4, 16, 1, 4, 0, -1, 0x55555555u, 0 },
	{ "the countdown has gone negative, other low half",
	  48, -131064, 9, 24, 40, 88, 5, 32, 2, 4, 0, -1, 0x66666666u, 0 },
	{ "no symbols are offered at all",
	  0, 60, 11, 24, 40, 88, 0, 12, 0, 4, 0, -1, 0x77777777u, 0 },
	{ "no symbols are offered and the countdown is spent",
	  0, 0, 11, 24, 40, 88, 1, 12, 1, 4, 0, -1, 0x78787878u, 0 },
	{ "the transition swaps the whole context",
	  12, 6, 6, 24, 40, 132, 2, 12, 2, 4, 1, -1, 0x88888888u, 0 },
	{ "the transition swaps the context and rewrites *left",
	  24, 8, 8, 40, 56, 132, 3, 16, 0, 4, 1, 19, 0x99999999u, 0 },
	{ "the transition rewrites *left",
	  24, 8, 8, 40, 56, 132, 4, 16, 1, 4, 0, 19, 0xaaaaaaaau, 0 },
	{ "a whole block is offered, 48 symbols",
	  48, 400, 48, 12, 160, 88, 5, 48, 2, 4, 0, -1, 0xbbbbbbbbu, 0 },
	{ "a whole block is offered and the countdown expires on it",
	  48, 48, 12, 24, 160, 88, 0, 48, 0, 4, 0, -1, 0xccccccccu, 0 },
	{ "the block charge is zero, as V32RngRespNextState leaves it",
	  12, 40, 0, 24, 40, 88, 1, 12, 1, 4, 0, -1, 0xddddddddu, 0 },
	{ "the block charge exceeds the countdown",
	  12, 20, 48, 24, 40, 88, 2, 12, 2, 4, 0, -1, 0xeeeeeeeeu, 0 },
	{ "a six-bit scrambler group, which the object splits in two",
	  12, 200, 48, 24, 40, 88, 3, 12, 0, 6, 0, -1, 0x0f0f0f0fu, 0 },
	{ "a two-bit scrambler group",
	  16, 9, 9, 24, 40, 88, 4, 16, 1, 2, 0, -1, 0x12345678u, 0 },
	{ "the ring is shorter than the count",
	  32, 300, 48, 24, 40, 88, 5, 12, 2, 4, 0, -1, 0x24681357u, 0 },
	{ "the sample length is zero",
	  12, 200, 48, 24, 0, 88, 0, 12, 0, 4, 0, -1, 0x13572468u, 0 },
	{ "the sample length is zero and the countdown expires",
	  12, 4, 4, 24, 0, 132, 1, 12, 1, 4, 1, -1, 0x1a2b3c4du, 0 },
	/* The last field is `encswap`: the MODULATOR swaps obj + 0x64. */
	{ "the modulator swaps the context under the state",
	  12, 12, 12, 24, 40, 132, 2, 12, 2, 4, 0, -1, 0x2b3c4d5eu, 1 },
	{ "the modulator swaps the context and the count was clamped",
	  24, 6, 6, 40, 56, 132, 3, 16, 0, 4, 0, -1, 0x3c4d5e6fu, 1 }
};

#define NTRIAL	((int)(sizeof(trials) / sizeof(trials[0])))

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;
	int t, s;

	build_tables();
	install_dispatchers();

	for (t = 0; t < NTRIAL; t++)
		for (s = 0; s < S_COUNT; s++)
			run_one(&trials[t], s);

	restore_dispatchers();

	diff_begin("v32txhdx separating trials");
	diff_eq_int("a transition fired (%ld)", sep_transition > 0, 1,
		    sep_transition);
	diff_eq_int("a call returned without transitioning (%ld)",
		    sep_no_transition > 0, 1, sep_no_transition);
	diff_eq_int("the countdown landed on exactly zero (%ld)",
		    sep_exact_zero > 0, 1, sep_exact_zero);
	diff_eq_int("the count was clamped below *left (%ld)",
		    sep_clamped > 0, 1, sep_clamped);
	diff_eq_int("a negative countdown truncated to a small count (%ld)",
		    sep_truncating > 0, 1, sep_truncating);
	diff_eq_int("TxHdxNoCarrier's own guard was reached (%ld)",
		    sep_nocarrier_guard > 0, 1, sep_nocarrier_guard);
	diff_eq_int("a transition swapped the context (%ld)", sep_ctx_swap > 0,
		    1, sep_ctx_swap);
	diff_eq_int("a state reported the NEW context's block length (%ld)",
		    sep_ctx_swap_seen > 0, 1, sep_ctx_swap_seen);
	diff_eq_int("a transition rewrote *left and the state saw it (%ld)",
		    sep_left_rewritten > 0, 1, sep_left_rewritten);
	diff_eq_int("*left was left non-zero (%ld)",
		    sep_left_nonzero_after > 0, 1, sep_left_nonzero_after);
	diff_eq_int("TxHdxFinishFrame zeroed a non-zero *left (%ld)",
		    sep_left_zeroed > 0, 1, sep_left_zeroed);
	diff_eq_int("the countdown was charged the symbols OFFERED (%ld)",
		    sep_charged_offered > 0, 1, sep_charged_offered);
	diff_eq_int("samples were produced (%ld)", sep_samples > 0, 1,
		    sep_samples);
	diff_eq_int("an encoder arm ran (%ld)", sep_encoder_ran > 0, 1,
		    sep_encoder_ran);
	diff_eq_int("a leaf was called with a count of zero (%ld)",
		    sep_zero_count > 0, 1, sep_zero_count);
	diff_eq_int("charge, symbol and sample lengths were distinct (%ld)",
		    sep_fields_distinct > 0, 1, sep_fields_distinct);
	diff_eq_int("the context was re-read after the modulator (%ld)",
		    sep_enc_ctx_swap > 0, 1, sep_enc_ctx_swap);
	diff_eq_int("a state rewrote the data buffer (%ld)",
		    sep_data_changed > 0, 1, sep_data_changed);
	for (s = 0; s < V32_NEXTSTATE_COUNT; s++)
		diff_eq_int("V32NextState slot %ld was dispatched",
			    sep_mode_seen[s] > 0, 1, s);
	rc |= diff_end();

	return rc | rc_total;
}
