/*
 * t_v32nsrng.c -- differential test of V.32's two RING next-state dispatchers.
 *
 *   V32RngInitNextState   .text 0x084c80
 *   V32RngRespNextState   .text 0x0852d0
 *
 * Both are switches on the handshake state at hdx + 0x74, so the test is a
 * SWEEP: every state 0..34 -- one past the object's own `cmp $0x21` bound, so
 * the default arm is reached deliberately and not by accident -- crossed with
 * a small set of PROFILES that move the eight things the arms branch on.
 *
 * ---------------------------------------------------------------------------
 * THE INSTALLED FUNCTION POINTERS ARE COMPARED BY IDENTITY, NOT BY VALUE
 *
 * These functions' whole job is to store a `TxHdx*` at hdx + 0x6c and an
 * `RxHdx*` at hdx + 0x70, and OUR copy of `TxHdxScrSequence` and the blob's
 * `ref_TxHdxScrSequence` are two different addresses for the same logical
 * state.  Comparing those eight bytes as memory would fail on every trial that
 * worked.  So hdx + 0x6c..0x73 is SKIPPED in the object comparison and checked
 * separately: each side's stored pointer is looked up in a table of
 * (ours, blob's) pairs and the two INDICES are compared.  An unrecognised
 * pointer maps to a distinguished value, so installing something outside the
 * table is a failure rather than a silent pass.
 *
 * The same treatment covers four fields the mode setters reach through:
 * `fpm_pps_cfg::imap` and `::qmap` (fp + 0x70, + 0x74), which take one of five
 * constellation tables per side, and `fpm_fse_cfg::decision` (fp + 0x234),
 * which takes one of seven slicers.  `fpm_fse_cfg::owner` (fp + 0x230) is a
 * FIXTURE address and is checked as "this side's own `v32_dec`".
 *
 * ---------------------------------------------------------------------------
 * WHAT THE PROFILES MOVE, AND WHY EACH ONE IS THERE
 *
 *   protocol   obj + 0x00.  Four arms across the two functions branch on it
 *              and always as `== 0`, so 0, 1, 2 and 3 all appear.
 *   bps/trellis  obj + 0x04 and + 0x1c, which is all `GetRateV32` reads.
 *   left       hdx + 0x78.  RngInit's B and C arms are gated on `> 0`, so the
 *              set carries positive, zero and negative values.
 *   limit      hdx + 0x80.  RngInit's D arm computes `limit - left` and takes
 *              a different arm above 0x3f, so both sides of that are present.
 *   flag 0x04  obj + 0x31.  RngInit's C and D arms both branch on it.
 *   reg[4]     hdx + 0x44.  Zero and non-zero decide whether RngInit's F and
 *              RngResp's G go on to the next state or jump to DONE.
 *   reg[1]     hdx + 0x3e, and fp + 0x2a beside it: together they decide what
 *              `DecodeRateSeq` returns, and one profile is chosen to return
 *              V32_RATE_NONE so the cleardown arms fire.
 *   hdx + 0x48 charged against 0x18 by the two early-DONE arms.
 *
 * ---------------------------------------------------------------------------
 * THE TRACE IS EXERCISED, AT BOTH LEVELS
 *
 * Both functions end with a `dsplibs_debug_printf("state %s(%d)\n", ...)`
 * gated on `dsplibs_debug_level > 1`.  Half the trials run at level 0 and half
 * at level 2 with the harness's capture on, and the two transcripts are
 * compared as text -- which is also the only check on the two `V32StateName`
 * calls, since ours and the blob's return different pointers to the same
 * string.  `dsplib_debug_capture_lines` is asserted non-zero so a dead capture
 * cannot read as agreement.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/v32hdxst.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32state.h"
#include "dsplib/v32smc.h"
#include "dsplib/v32dec.h"
#include "dsplib/vtb.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/debug.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_V32RngInitNextState(void *modem);
extern void ref_V32RngRespNextState(void *modem);

extern short ref_TxHdxCarrierState(void *m, short *d, short *o,
				   unsigned short *l);
extern short ref_TxHdxScrSequence(void *m, short *d, short *o,
				  unsigned short *l);
extern short ref_TxHdxData(void *m, short *d, short *o, unsigned short *l);

extern void ref_RxHdxToneData(void *m, short *i, unsigned short *o,
			      unsigned short *c);
extern void ref_RxHdxSequence(void *m, short *i, unsigned short *o,
			      unsigned short *c);
extern void ref_RxHdxSequenceE(void *m, short *i, unsigned short *o,
			       unsigned short *c);
extern void ref_RxHdxData(void *m, short *i, unsigned short *o,
			  unsigned short *c);

extern const short ref_SMCv32_IMAP16[];
extern const short ref_SMCv32_QMAP16[];
extern const short ref_VTBv32_IMAP32[];
extern const short ref_VTBv32_QMAP32[];
extern const short ref_VTBv32_IMAP16T[];
extern const short ref_VTBv32_QMAP16T[];
extern const short ref_VTBv32_IMAP64[];
extern const short ref_VTBv32_QMAP64[];
extern const short ref_VTBv32_IMAP128[];
extern const short ref_VTBv32_QMAP128[];

extern unsigned short ref_FSE_decision_4pt(struct fpm_fse *, short *, short *,
					   short *);
extern unsigned short ref_FSE_decision_AB(struct fpm_fse *, short *, short *,
					  short *);
extern unsigned short ref_FSE_decision_16pt(struct fpm_fse *, short *, short *,
					    short *);
extern unsigned short ref_FSE_decision_32pt(struct fpm_fse *, short *, short *,
					    short *);
extern unsigned short ref_FSE_decision_16Tpt(struct fpm_fse *, short *,
					     short *, short *);
extern unsigned short ref_FSE_decision_64pt(struct fpm_fse *, short *, short *,
					    short *);
extern unsigned short ref_FSE_decision_128pt(struct fpm_fse *, short *,
					     short *, short *);

extern const short ref_VTB_REGION_7200[];
extern const short ref_VTB_REGION_9600[];
extern const short ref_VTB_REGION_12000[];
extern const short ref_VTB_REGION_14400[];
extern const short ref_VTB_BOUND_7200[];
extern const short ref_VTB_BOUND_9600[];
extern const short ref_VTB_BOUND_12000[];
extern const short ref_VTB_BOUND_14400[];

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x100
#define FP_SIZE		0x5100

#define TONE_LEN	16
#define TONE_EXTRA	4

/* The offsets this file needs and no header owns; see src/pump/v32/v32nsrng.c. */
#define T_OBJ_PROTOCOL		0x00
#define T_HDX_STATE_LEFT	0x78
#define T_HDX_INT_7C		0x7c
#define T_HDX_INT_80		0x80
#define T_HDX_SHORT_48		0x48
#define T_FLAG_04		0x04
#define T_HDX_BLOCK_CHARGE	0x84

#define T_PPS_IMAP	(V32FP_PPS + (int)offsetof(struct fpm_pps_cfg, imap))
#define T_PPS_QMAP	(V32FP_PPS + (int)offsetof(struct fpm_pps_cfg, qmap))
#define T_FSE_OWNER	(V32FP_FSE + (int)offsetof(struct fpm_fse_cfg, owner))
#define T_FSE_DECISION	(V32FP_FSE + (int)offsetof(struct fpm_fse_cfg, decision))

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	fp[FP_SIZE];
	struct v32_dec	dec;
	/*
	 * The survivor ring `struct vtb::paths` names.  `SetRxModeV32` reaches
	 * `VTBv32_init` with `alloc` ZERO for the four trellis modes, so it
	 * CLEARS 128 nodes through whatever pointer is already there -- the
	 * fixture has to supply one or the trial dereferences the random fill.
	 */
	struct vtb_path	vpath[16 * 8];
	struct fpm_tone	tone[3];
	short		kern[3][TONE_LEN];
	short		hist[3][TONE_LEN + TONE_EXTRA];
	short		revb[3][8];
	short		reva[3][8];
	double		align;
};

static struct fix fa, fb;
static struct fix pre;

static int rc_total;

/* --------------------------------------------------------------------- */

static unsigned rng_state;

static unsigned
rng_next(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

static void
fill(void *p, int n, unsigned seed)
{
	unsigned char *b = (unsigned char *)p;
	int i;

	rng_state = seed ? seed : 1u;
	for (i = 0; i < n; i++)
		b[i] = (unsigned char)rng_next();
}

static void
put_ptr(void *base, int off, void *v)
{
	*(void **)(void *)((unsigned char *)base + off) = v;
}

static void *
get_ptr(const void *base, int off)
{
	return *(void *const *)(const void *)((const unsigned char *)base
					      + off);
}

static void
put_s16(void *base, int off, short v)
{
	*(short *)(void *)((unsigned char *)base + off) = v;
}

static short
get_s16(const void *base, int off)
{
	return *(const short *)(const void *)((const unsigned char *)base
					      + off);
}

static void
put_int(void *base, int off, int v)
{
	*(int *)(void *)((unsigned char *)base + off) = v;
}

static unsigned char
get_u8(const void *base, int off)
{
	return *((const unsigned char *)base + off);
}

/* --------------------------------------------------------------------- */

struct skip {
	int off;
	int len;
};

static int
first_diff(const void *a, const void *b, int len, const struct skip *sk,
	   int nsk)
{
	const unsigned char *x = (const unsigned char *)a;
	const unsigned char *y = (const unsigned char *)b;
	int i, j;

	for (i = 0; i < len; i++) {
		if (x[i] == y[i])
			continue;
		for (j = 0; j < nsk; j++)
			if (i >= sk[j].off && i < sk[j].off + sk[j].len)
				break;
		if (j == nsk)
			return i;
	}
	return -1;
}

static void
cmp_one(const char *blk, const void *a, const void *b, int len,
	const struct skip *sk, int nsk, long tag)
{
	char fmt[128];

	sprintf(fmt, "%s differs at byte (trial %%ld)", blk);
	diff_eq_int(fmt, first_diff(a, b, len, sk, nsk), -1, tag);
}

/* --------------------------------------------------------------------- */
/*
 * The (ours, blob's) pointer pairs.  Index 0 is the null pointer, so a field
 * nothing wrote compares equal without a special case; -1 is "some pointer
 * neither side's table names", which must never appear.
 */

struct pairent {
	const void *ours;
	const void *theirs;
};

static const struct pairent pairs[] = {
	{ (const void *)0,			(const void *)0 },

	{ (const void *)TxHdxCarrierState,	(const void *)ref_TxHdxCarrierState },
	{ (const void *)TxHdxScrSequence,	(const void *)ref_TxHdxScrSequence },
	{ (const void *)TxHdxData,		(const void *)ref_TxHdxData },

	{ (const void *)RxHdxToneData,		(const void *)ref_RxHdxToneData },
	{ (const void *)RxHdxSequence,		(const void *)ref_RxHdxSequence },
	{ (const void *)RxHdxSequenceE,		(const void *)ref_RxHdxSequenceE },
	{ (const void *)RxHdxData,		(const void *)ref_RxHdxData },

	{ (const void *)SMCv32_IMAP16,		(const void *)ref_SMCv32_IMAP16 },
	{ (const void *)SMCv32_QMAP16,		(const void *)ref_SMCv32_QMAP16 },
	{ (const void *)VTBv32_IMAP32,		(const void *)ref_VTBv32_IMAP32 },
	{ (const void *)VTBv32_QMAP32,		(const void *)ref_VTBv32_QMAP32 },
	{ (const void *)VTBv32_IMAP16T,		(const void *)ref_VTBv32_IMAP16T },
	{ (const void *)VTBv32_QMAP16T,		(const void *)ref_VTBv32_QMAP16T },
	{ (const void *)VTBv32_IMAP64,		(const void *)ref_VTBv32_IMAP64 },
	{ (const void *)VTBv32_QMAP64,		(const void *)ref_VTBv32_QMAP64 },
	{ (const void *)VTBv32_IMAP128,		(const void *)ref_VTBv32_IMAP128 },
	{ (const void *)VTBv32_QMAP128,		(const void *)ref_VTBv32_QMAP128 },

	{ (const void *)FSE_decision_4pt,	(const void *)ref_FSE_decision_4pt },
	{ (const void *)FSE_decision_AB,	(const void *)ref_FSE_decision_AB },
	{ (const void *)FSE_decision_16pt,	(const void *)ref_FSE_decision_16pt },
	{ (const void *)FSE_decision_32pt,	(const void *)ref_FSE_decision_32pt },
	{ (const void *)FSE_decision_16Tpt,	(const void *)ref_FSE_decision_16Tpt },
	{ (const void *)FSE_decision_64pt,	(const void *)ref_FSE_decision_64pt },
	{ (const void *)FSE_decision_128pt,	(const void *)ref_FSE_decision_128pt },

	/*
	 * APPENDED, AND THAT MATTERS: main() slices `pair_seen` by index, so
	 * the trellis tables go at the END rather than beside the maps.
	 */
	{ (const void *)VTB_REGION_7200,	(const void *)ref_VTB_REGION_7200 },
	{ (const void *)VTB_REGION_9600,	(const void *)ref_VTB_REGION_9600 },
	{ (const void *)VTB_REGION_12000,	(const void *)ref_VTB_REGION_12000 },
	{ (const void *)VTB_REGION_14400,	(const void *)ref_VTB_REGION_14400 },
	{ (const void *)VTB_BOUND_7200,		(const void *)ref_VTB_BOUND_7200 },
	{ (const void *)VTB_BOUND_9600,		(const void *)ref_VTB_BOUND_9600 },
	{ (const void *)VTB_BOUND_12000,	(const void *)ref_VTB_BOUND_12000 },
	{ (const void *)VTB_BOUND_14400,	(const void *)ref_VTB_BOUND_14400 }
};

#define NPAIR_SLICED	25	/* the first entry of the appended block */

#define NPAIR	((int)(sizeof(pairs) / sizeof(pairs[0])))

static long
pid(const void *p, int side)
{
	int i;

	for (i = 0; i < NPAIR; i++)
		if (p == (side ? pairs[i].theirs : pairs[i].ours))
			return (long)i;
	return -1;
}

/* Which pair indices were actually installed, for the non-vacuity report. */
static long pair_seen[NPAIR];

/* --------------------------------------------------------------------- */

static const short tone_src[TONE_LEN] = {
	1000, -900, 800, -700, 600, -500, 400, -300,
	200, -100, 50, -25, 12, -6, 3, -1
};

struct prof {
	unsigned short	protocol;	/* obj + 0x00                        */
	short		bps;		/* obj + 0x04                        */
	int		trellis;	/* obj + 0x1c                        */
	int		left;		/* hdx + 0x78                        */
	int		limit;		/* hdx + 0x80                        */
	int		flag04;		/* obj + 0x31 bit 0x04               */
	unsigned short	reg0;		/* hdx + 0x3c                        */
	unsigned short	reg1;		/* hdx + 0x3e                        */
	unsigned short	reg4;		/* hdx + 0x44                        */
	short		f48;		/* hdx + 0x48                        */
	short		rxidx;		/* fp  + 0x2a, the ladder's local    */
	int		detmatch;	/* hdx + 0x68, what GetSequence gives */
};

static const struct prof profs[] = {
	{ 0, 14400, 1,    40, 0x0060, 0, 0x0ff9, 0x0ff9, 0x0ff9,  3, 5, 0x0ff9 },
	{ 1,  9600, 0,     0, 0x0080, 1, 0x0b11, 0x0b11, 0x0000,  0, 1, 0x0b11 },
	{ 2,  7200, 1,    -5, 0x0020, 0, 0x09d1, 0x09d1, 0x09d1,  9, 3, 0x09d1 },
	{ 0,  4800, 0,   100, 0x0100, 1, 0x0d11, 0x0d11, 0x0000, 24, 0, 0x0d11 },
	{ 1, 12000, 1,     1, 0x0040, 0, 0x09b1, 0x09b1, 0x09b1,  1, 4, 0x09b1 },
	{ 3,     0, 0,    -1, 0x0000, 1, 0x0000, 0x0000, 0x0000,  0, 6, 0x0000 },
	{ 0, 14400, 0, 0x080, 0x0081, 0, 0x0997, 0x0997, 0x0997, 30, 5, 0x0997 },
	{ 2,  9600, 1, 0x07f, 0x007f, 1, 0x0111, 0x0111, 0x0002, 24, 2, 0x0111 },
	{ 1,  4800, 1,     7, 0x0d00, 0, 0x0b91, 0x0b91, 0x0001, 12, 2, 0x0b91 }
};

#define NPROF	((int)(sizeof(profs) / sizeof(profs[0])))

static void
build(struct fix *f, unsigned seed, const struct prof *pr, int state)
{
	unsigned char *hdx;
	unsigned char *fp;
	int t;

	fill(f, (int)offsetof(struct fix, align), seed);

	hdx = f->hdx;
	fp = f->fp;

	put_ptr(f->obj, V32_OBJ_HDX, hdx);
	put_ptr(f->obj, V32_OBJ_FP, fp);

	*(unsigned short *)(void *)(f->obj + T_OBJ_PROTOCOL) = pr->protocol;
	put_s16(f->obj, V32_OBJ_BPS, pr->bps);
	put_int(f->obj, V32_OBJ_TRELLIS, pr->trellis);
	f->obj[V32_OBJ_STATUS] = 0x5a;
	f->obj[V32_OBJ_FLAGS] = (unsigned char)(pr->flag04 ? T_FLAG_04 : 0);

	put_s16(hdx, V32HDX_STATE, (short)state);
	put_s16(hdx, V32HDX_MODE, 4);
	put_int(hdx, T_HDX_STATE_LEFT, pr->left);
	put_int(hdx, T_HDX_INT_7C, 0x1234);
	put_int(hdx, T_HDX_INT_80, pr->limit);
	put_s16(hdx, T_HDX_BLOCK_CHARGE, 0x11);
	put_s16(hdx, T_HDX_SHORT_48, pr->f48);
	put_int(hdx, V32HDX_DET_MATCH, pr->detmatch);

	*(unsigned short *)(void *)(hdx + V32HDX_REGS + 0) = pr->reg0;
	*(unsigned short *)(void *)(hdx + V32HDX_REGS + 2) = pr->reg1;
	*(unsigned short *)(void *)(hdx + V32HDX_REGS + 4) = 0x1111;
	*(unsigned short *)(void *)(hdx + V32HDX_REGS + 6) = 0x2222;
	*(unsigned short *)(void *)(hdx + V32HDX_REGS + 8) = pr->reg4;

	put_ptr(hdx, V32HDX_TXSTATE, (void *)0);
	put_ptr(hdx, V32HDX_RXSTATE, (void *)0);

	put_s16(fp, V32FP_TX_RATE_INDEX, pr->rxidx);
	put_s16(fp, V32FP_RX_RATE_INDEX, pr->rxidx);
	put_ptr(fp, T_PPS_IMAP, (void *)0);
	put_ptr(fp, T_PPS_QMAP, (void *)0);
	put_ptr(fp, T_FSE_OWNER, &f->dec);
	put_ptr(fp, T_FSE_DECISION, (void *)0);

	for (t = 0; t < 3; t++) {
		f->tone[t].cfg = FPM_TONE_CFG_data;
		f->tone[t].cfg.freq = (short)(1100 + t * 500);
		f->tone[t].cfg.damp = (short)(31000 - t * 2500);
		f->tone[t].cfg.scale = (short)(8000 + t * 700);
		f->tone[t].cfg.ratio = (short)(20000 + t * 1234);
		f->tone[t].cfg.src = tone_src;
		f->tone[t].cfg.len = TONE_LEN;
		f->tone[t].cfg.extra = TONE_EXTRA;
		f->tone[t].kernel = f->kern[t];
		f->tone[t].history = f->hist[t];
		f->tone[t].rev_block = f->revb[t];
		f->tone[t].rev_acc = f->reva[t];
	}
	((struct vtb *)(void *)f->dec.vtb)->paths = f->vpath;

	put_ptr(hdx, V32_HDX_TONE0, &f->tone[0]);
	put_ptr(hdx, V32_HDX_TONE1, &f->tone[1]);
	put_ptr(hdx, V32_HDX_TONE2, &f->tone[2]);
}

/* --------------------------------------------------------------------- */
/* Non-vacuity.                                                          */

static long arm_changed[V32_STATE_COUNT + 1];
static long n_default_quiet;
static long n_fault_posted;
static long n_cleardown;
static long n_connect_posted;
static long n_early_done;
static long n_tone_retuned;
static long n_state_advanced;
static long n_trace_lines;
static long n_proto_zero, n_proto_other;

/* --------------------------------------------------------------------- */

static void
run_one(const char *what, void (*ours)(void *), void (*theirs)(void *),
	int state, int prof_i, int level, long tag)
{
	static const struct skip objskip[] = {
		{ V32_OBJ_HDX, 4 }, { V32_OBJ_FP, 4 }
	};
	static const struct skip hdxskip[] = {
		{ V32_HDX_TONE0, 12 }, { V32HDX_TXSTATE, 8 }
	};
	static const struct skip fpskip[] = {
		{ T_PPS_IMAP, 4 }, { T_PPS_QMAP, 4 },
		{ T_FSE_OWNER, 4 }, { T_FSE_DECISION, 4 }
	};
	/*
	 * Five pointers inside the decoder's `struct vtb`: the survivor ring,
	 * which names this side's own fixture storage, and the four
	 * constellation and trellis tables, which name this side's own copies.
	 * All five are checked by identity below.
	 */
	static const struct skip decskip[] = {
		{ (int)offsetof(struct v32_dec, vtb) + 0x00, 4 },
		{ (int)offsetof(struct v32_dec, vtb) + 0x18, 4 },
		{ (int)offsetof(struct v32_dec, vtb) + 0x1c, 4 },
		{ (int)offsetof(struct v32_dec, vtb) + 0x20, 4 },
		{ (int)offsetof(struct v32_dec, vtb) + 0x28, 4 }
	};
	static const struct skip toneskip[] = {
		{ (int)offsetof(struct fpm_tone, kernel), 4 },
		{ (int)offsetof(struct fpm_tone, history), 4 },
		{ (int)offsetof(struct fpm_tone, rev_block), 4 },
		{ (int)offsetof(struct fpm_tone, rev_acc), 4 },
		{ (int)offsetof(struct fpm_tone, iir_self), 4 }
	};
	const struct prof *pr = &profs[prof_i];
	unsigned seed = 0x9e3779b9u ^ ((unsigned)tag * 2654435761u);
	int t;

	build(&fa, seed, pr, state);
	build(&fb, seed, pr, state);
	memcpy(&pre, &fa, sizeof(pre));

	dsplibs_debug_level = ref_dsplibs_debug_level = (unsigned)level;
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	(*ours)(fa.obj);
	(*theirs)(fb.obj);

	diff_begin(what);

	cmp_one("the instance", fa.obj, fb.obj, OBJ_SIZE, objskip, 2, tag);
	cmp_one("the context", fa.hdx, fb.hdx, HDX_SIZE, hdxskip, 2, tag);
	cmp_one("the datapump block", fa.fp, fb.fp, FP_SIZE, fpskip, 4, tag);
	cmp_one("the decoder", &fa.dec, &fb.dec, (int)sizeof(fa.dec), decskip,
		5, tag);
	cmp_one("the survivor ring", fa.vpath, fb.vpath, (int)sizeof(fa.vpath),
		0, 0, tag);
	for (t = 0; t < 3; t++)
		cmp_one("a tone object", &fa.tone[t], &fb.tone[t],
			(int)sizeof(fa.tone[t]), toneskip, 5, tag);
	cmp_one("the tone kernels", fa.kern, fb.kern, (int)sizeof(fa.kern), 0,
		0, tag);
	cmp_one("the tone histories", fa.hist, fb.hist, (int)sizeof(fa.hist),
		0, 0, tag);
	cmp_one("the reversal blocks", fa.revb, fb.revb, (int)sizeof(fa.revb),
		0, 0, tag);
	cmp_one("the reversal accumulators", fa.reva, fb.reva,
		(int)sizeof(fa.reva), 0, 0, tag);

	/* The five pointer fields, by identity rather than by value. */
	{
		long a, b;

		a = pid(get_ptr(fa.hdx, V32HDX_TXSTATE), 0);
		b = pid(get_ptr(fb.hdx, V32HDX_TXSTATE), 1);
		diff_eq_int("the installed transmit state (trial %ld)", a, b,
			    tag);
		diff_eq_int("the transmit state is a known one (trial %ld)",
			    a >= 0, 1, tag);
		if (a >= 0)
			pair_seen[a]++;

		a = pid(get_ptr(fa.hdx, V32HDX_RXSTATE), 0);
		b = pid(get_ptr(fb.hdx, V32HDX_RXSTATE), 1);
		diff_eq_int("the installed receive state (trial %ld)", a, b,
			    tag);
		diff_eq_int("the receive state is a known one (trial %ld)",
			    a >= 0, 1, tag);
		if (a >= 0)
			pair_seen[a]++;

		a = pid(get_ptr(fa.fp, T_PPS_IMAP), 0);
		b = pid(get_ptr(fb.fp, T_PPS_IMAP), 1);
		diff_eq_int("the transmit I map (trial %ld)", a, b, tag);
		diff_eq_int("the I map is a known one (trial %ld)", a >= 0, 1,
			    tag);
		if (a >= 0)
			pair_seen[a]++;

		a = pid(get_ptr(fa.fp, T_PPS_QMAP), 0);
		b = pid(get_ptr(fb.fp, T_PPS_QMAP), 1);
		diff_eq_int("the transmit Q map (trial %ld)", a, b, tag);

		a = pid(get_ptr(fa.fp, T_FSE_DECISION), 0);
		b = pid(get_ptr(fb.fp, T_FSE_DECISION), 1);
		diff_eq_int("the installed slicer (trial %ld)", a, b, tag);
		diff_eq_int("the slicer is a known one (trial %ld)", a >= 0, 1,
			    tag);
		if (a >= 0)
			pair_seen[a]++;

		diff_eq_int("ours left `owner` at its own decoder (trial %ld)",
			    get_ptr(fa.fp, T_FSE_OWNER) == (void *)&fa.dec, 1,
			    tag);
		diff_eq_int("the blob left `owner` at its own (trial %ld)",
			    get_ptr(fb.fp, T_FSE_OWNER) == (void *)&fb.dec, 1,
			    tag);

		/* The four tables VTBv32_init aims the trellis decoder at. */
		diff_eq_int("the trellis I map (trial %ld)",
			    pid(get_ptr(fa.dec.vtb, 0x18), 0),
			    pid(get_ptr(fb.dec.vtb, 0x18), 1), tag);
		diff_eq_int("the trellis Q map (trial %ld)",
			    pid(get_ptr(fa.dec.vtb, 0x1c), 0),
			    pid(get_ptr(fb.dec.vtb, 0x1c), 1), tag);
		diff_eq_int("the trellis boundary table (trial %ld)",
			    pid(get_ptr(fa.dec.vtb, 0x20), 0),
			    pid(get_ptr(fb.dec.vtb, 0x20), 1), tag);
		diff_eq_int("the trellis region table (trial %ld)",
			    pid(get_ptr(fa.dec.vtb, 0x28), 0),
			    pid(get_ptr(fb.dec.vtb, 0x28), 1), tag);

		diff_eq_int("ours left the survivor ring at its own (%ld)",
			    get_ptr(fa.dec.vtb, 0) == (void *)fa.vpath, 1,
			    tag);
		diff_eq_int("the blob left its own survivor ring (%ld)",
			    get_ptr(fb.dec.vtb, 0) == (void *)fb.vpath, 1,
			    tag);
	}

	/* The trace, as text. */
	diff_eq_int("the debug transcript (trial %ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("the debug line count (trial %ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);

	rc_total |= diff_end();

	/* ---- what did this trial actually exercise? ---- */
	if (state <= V32_STATE_COUNT)
		arm_changed[state] +=
			(memcmp(&pre.obj, &fa.obj, OBJ_SIZE) != 0
			 || memcmp(&pre.hdx, &fa.hdx, HDX_SIZE) != 0
			 || memcmp(&pre.fp, &fa.fp, FP_SIZE) != 0);
	if (state >= V32_STATE_COUNT - 1
	    && memcmp(&pre.hdx, &fa.hdx, HDX_SIZE) == 0
	    && memcmp(&pre.obj, &fa.obj, OBJ_SIZE) == 0)
		n_default_quiet++;
	if ((get_u8(fa.obj, V32_OBJ_FLAGS) & V32_FLAG_FAULT) != 0
	    && (get_u8(pre.obj, V32_OBJ_FLAGS) & V32_FLAG_FAULT) == 0)
		n_fault_posted++;
	if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_CLEARDOWN
	    && state != V32_STATE_CLEARDOWN)
		n_cleardown++;
	if (get_u8(fa.obj, V32_OBJ_STATUS) != get_u8(pre.obj, V32_OBJ_STATUS))
		n_connect_posted++;
	if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_DONE
	    && (state == V32_STATE_F || state == V32_STATE_G))
		n_early_done++;
	if (memcmp(&pre.tone[0], &fa.tone[0], sizeof(fa.tone[0])) != 0)
		n_tone_retuned++;
	if (get_s16(fa.hdx, V32HDX_STATE) != (short)state)
		n_state_advanced++;
	n_trace_lines += (long)dsplib_debug_capture_lines(0);
	if (pr->protocol == 0)
		n_proto_zero++;
	else
		n_proto_other++;
}

/* --------------------------------------------------------------------- */

static void
sweep(const char *what, void (*ours)(void *), void (*theirs)(void *))
{
	int state, p;
	long tag = 0;

	for (state = 0; state <= V32_STATE_COUNT; state++)
		for (p = 0; p < NPROF; p++) {
			run_one(what, ours, theirs, state, p,
				((state + p) & 1) ? 2 : 0, tag);
			tag++;
		}
}

int
main(void)
{
	int rc = 0;
	int i;
	long tx_kinds = 0, rx_kinds = 0, map_kinds = 0, slicer_kinds = 0;

	sweep("V32RngInitNextState: the state sweep", V32RngInitNextState,
	      ref_V32RngInitNextState);
	sweep("V32RngRespNextState: the state sweep", V32RngRespNextState,
	      ref_V32RngRespNextState);

	for (i = 1; i <= 3; i++)
		tx_kinds += pair_seen[i] > 0;
	for (i = 4; i <= 7; i++)
		rx_kinds += pair_seen[i] > 0;
	for (i = 8; i <= 17; i++)
		map_kinds += pair_seen[i] > 0;
	for (i = 18; i < NPAIR_SLICED; i++)
		slicer_kinds += pair_seen[i] > 0;

	diff_begin("v32nsrng separating trials");
	for (i = 0; i < V32_STATE_COUNT; i++) {
		static const int live[] = {
			V32_STATE_A, V32_STATE_B, V32_STATE_C, V32_STATE_D,
			V32_STATE_D2, V32_STATE_E, V32_STATE_F, V32_STATE_G,
			V32_STATE_H, V32_STATE_DONE, V32_STATE_ERROR
		};
		int j, want = 0;

		for (j = 0; j < (int)(sizeof(live) / sizeof(live[0])); j++)
			if (live[j] == i)
				want = 1;
		if (want)
			diff_eq_int("state %ld reached a live arm",
				    arm_changed[i] > 0, 1, (long)i);
	}
	diff_eq_int("the default arm changed nothing (%ld)",
		    n_default_quiet > 0, 1, n_default_quiet);
	diff_eq_int("a fault was posted (%ld)", n_fault_posted > 0, 1,
		    n_fault_posted);
	diff_eq_int("cleardown was reached (%ld)", n_cleardown > 0, 1,
		    n_cleardown);
	diff_eq_int("a connect status was posted (%ld)", n_connect_posted > 0,
		    1, n_connect_posted);
	diff_eq_int("F or G went straight to DONE (%ld)", n_early_done > 0, 1,
		    n_early_done);
	diff_eq_int("the tone was retuned (%ld)", n_tone_retuned > 0, 1,
		    n_tone_retuned);
	diff_eq_int("the handshake advanced (%ld)", n_state_advanced > 0, 1,
		    n_state_advanced);
	diff_eq_int("the trace actually printed (%ld)", n_trace_lines > 0, 1,
		    n_trace_lines);
	diff_eq_int("protocol 0 was used (%ld)", n_proto_zero > 0, 1,
		    n_proto_zero);
	diff_eq_int("another protocol was used (%ld)", n_proto_other > 0, 1,
		    n_proto_other);
	diff_eq_int("distinct transmit states installed (%ld)", tx_kinds >= 2,
		    1, tx_kinds);
	diff_eq_int("distinct receive states installed (%ld)", rx_kinds >= 3,
		    1, rx_kinds);
	diff_eq_int("distinct constellation maps installed (%ld)",
		    map_kinds >= 2, 1, map_kinds);
	diff_eq_int("distinct slicers installed (%ld)", slicer_kinds >= 2, 1,
		    slicer_kinds);
	rc |= diff_end();

	return rc | rc_total;
}
