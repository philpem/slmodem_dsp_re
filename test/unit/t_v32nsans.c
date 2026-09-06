/*
 * t_v32nsans.c -- differential test of `V32AnsNextState`.
 *
 * The answering station's handshake step: a 29-armed switch on hdx + 0x74
 * that installs the next transmit and receive states, sets the two budgets
 * they count against, and reconfigures the datapump through fourteen already
 * reconstructed functions.  So the fixture has to be a WORKING instance and
 * not a scratch block -- `SetToneDetect` rebuilds a `struct fpm_tone` through
 * `FPM_TONE_create`, `SetAdaptEcV32` re-inits an echo canceller,
 * `FPM_MTD_create` copies a config back over itself, and every one of those
 * dereferences a pointer the fixture must have planted.
 *
 * ---------------------------------------------------------------------------
 * THE INSTALLED STATE POINTERS ARE COMPARED BY IDENTITY, NOT BY VALUE
 *
 * This function's whole product is two function pointers, and the two sides
 * can NEVER hold the same bits in them: ours stores `TxHdxScrSequence` and the
 * blob stores `ref_TxHdxScrSequence`, which are different addresses for the
 * same logical state and always will be.  So hdx + 0x6c and hdx + 0x70 are
 * SKIPPED by the byte comparison -- CLAUDE.md's exception for a region that
 * must be skipped -- and checked separately, through a table of
 * (ours, the blob's) PAIRS:
 *
 *   - our side's pointer is looked up in the pair table; a value that is in
 *     neither the table nor the untouched sentinel is a failure on its own,
 *     because it means we installed something no arm should install;
 *   - the blob's pointer must then be that entry's `ref_` half.
 *
 * A swapped install -- `RxHdxSequence` where the blob puts `ref_RxHdxData` --
 * fails on the second check, so the pairing is as strong as a byte comparison
 * would be if the addresses could agree.  The sentinel is planted identically
 * on both sides, so an arm that installs NOTHING is distinguishable from every
 * arm that installs something.
 *
 * ---------------------------------------------------------------------------
 * THE OTHER RELOCATED FIELDS ARE SKIPPED AND THE ARGUMENT FOR THAT IS NOT
 * "THEY ARE COVERED ELSEWHERE"
 *
 * `SetTxModeV32` and `SetRxModeV32` install constellation maps and a slicer,
 * and those are relocated addresses too (`VTBv32_IMAP32` against
 * `ref_VTBv32_IMAP32`).  They are skipped here, and the reason they can be is
 * that they carry NO INFORMATION THIS TEST NEEDS: each of the two setters
 * writes a `struct v32_sdm` beside them -- seven plain integer fields at
 * fp + 0x30 and fp + 0x50b0 -- whose contents differ for every one of the
 * seven modes.  So passing the WRONG mode is caught by the byte comparison of
 * those fields, and an out-of-range mode is caught by the status byte and the
 * fault flag, which are also plain bytes.  The pointers are a function of the
 * mode and nothing else.  Which map belongs to which mode is `t_v32fpctl.c`'s
 * question and is answered there.
 *
 * `mtd->cfg.coeff` is NOT in that category -- storing `V32_S_DATA_COEF` there
 * is this function's own doing and nothing else in the block moves with it --
 * so it is checked as a pair like the state pointers are.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE SWEEP IS BUILT TO SEPARATE
 *
 * Every check is aimed at a named wrong reading:
 *
 *   - THE STATE IS SWEPT 0..34 AND OUT OF RANGE.  The object's dispatch is
 *     `cmp $0x21` / `ja`, so 34 (DONT_CARE) is past the table and negative
 *     states take the default through the UNSIGNED branch.  -1 and -32768 are
 *     driven for exactly that: a signed bound would run them into the table.
 *     Five in-range slots (D2, R, S, CLEARDOWN, DONE) point at the default and
 *     must change nothing.
 *   - THE MODE AT hdx + 0x76 IS SWEPT TOO, although this function never reads
 *     it.  A reconstruction that dispatched on the mode instead of the state
 *     would pass a fixture that left it at zero.
 *   - THE SIX CONDITIONAL ARMS ARE DRIVEN BOTH WAYS AND ON THE BOUNDARY.
 *     +0x78 at -1, 0 and 1 separates `<= 0` from `< 0`; +0x7c at 0x8a, 0x8b
 *     and 0x8c separates `> 0x8b` from `>= 0x8b`; +0xa8 at 0x48 and 0x49 and
 *     +0xaa at 0x5f and 0x60 do the same for their arms.
 *   - THE SIGNEDNESS OF EACH TEST IS SEPARATED.  +0x78 and the two 16-bit
 *     counters are compared SIGNED in the object and +0x7c UNSIGNED, so
 *     negative values are driven into all four: a negative +0x7c must NOT
 *     satisfy `> 0x8b` if the field is unsigned, and it does if it is signed.
 *     0xffffffff is the separating input and it is in the sweep.
 *   - THE B ARM'S OPTION BIT.  obj + 0x11 bit 0x02 picks 0xa60 over 0x100; the
 *     byte is driven with the bit set, clear, and with its NEIGHBOURING bits
 *     set instead, so a mask one bit wide in the wrong place is visible.
 *   - THE X2 ARM'S ARITHMETIC.  8 - ((limit - budget) & 7) is driven over all
 *     eight residues and with a limit BELOW the budget, which makes the
 *     subtraction wrap -- the case a `%` would get wrong.
 *   - THE RATE LADDER, BOTH HALVES.  The ladder ANDs the far end's sequence
 *     against the local station's own `V32_RATE_SEQ[fp->rx_rate_index]`, so
 *     the sequence is swept beside the INDEX and not on its own: a fixed
 *     local station can only ever reach the rungs its own entry carries.
 *     Between them they make `DecodeRateSeq` return each rate AND
 *     `V32_RATE_NONE`, which is the T arm's fault branch and the END arm's
 *     last `V32_CONNECT` entry.
 *   - THE F2 ARM READS hdx + 0x9e SIGNED (`movswl`), so a negative symbol
 *     length must reach +0x78 as a negative int and not as 0xffff.
 *
 * ---------------------------------------------------------------------------
 * fp + 0x2a IS A SUBSCRIPT AND MUST BE PLANTED -- THE FIXTURE'S OWN LESSON
 *
 * The first version of this file filled the datapump block with pseudorandom
 * bytes and planted only the pointers a callee dereferences.  That is not
 * enough, and the reason is a class of field a pointer audit does not find:
 * `v32_common_rate` -- inlined into `DecodeRateSeq`, `CodeRateSeq`,
 * `CodeFinalRateSeq`, `CodeESeq` and `SeqToRate` -- opens with
 *
 *     short local = V32_RATE_SEQ[fp->rx_rate_index];
 *
 * against a SEVEN-entry table, and the object has no bound check.  A random
 * sixteen-bit index therefore reads up to 64 KB either side of that array,
 * which did two things at once and neither of them looked like its cause.  It
 * SEGFAULTED, in a later trial than the one that set it up; and before that it
 * made the two sides disagree, because ours subscripts `V32_RATE_SEQ` and the
 * blob's subscripts `ref_V32_RATE_SEQ` -- two arrays at different addresses,
 * so out of bounds they pick up different neighbours and the ladder returns a
 * different rate.  What surfaced was one byte at hdx + 0x3e, the register the
 * L arm's `StoreReg` writes, which is four calls downstream of the fault.
 *
 * A BLOB-AGAINST-BLOB DRY RUN CANNOT FIND THIS and one was done: with both
 * sides reading `ref_V32_RATE_SEQ` the same wild index gives the same wrong
 * answer, so every comparison agrees and only the segfault is left to chance.
 * The general rule this fixture now follows is that a field the callee uses as
 * a SUBSCRIPT is as load-bearing as a field it uses as a POINTER, and both
 * must be planted in range.  Finding F8587.
 *
 * ---------------------------------------------------------------------------
 * THE DIAGNOSTIC PATH IS DRIVEN, BECAUSE IT IS THE AUTHOR'S OWN STRING
 *
 * `dsplibs_debug_level` ships at zero, so the tail print is dead in normal
 * running and a wrong format string there would never fail.  The last section
 * raises BOTH levels, turns the harness's capture on, and compares the two
 * transcripts: that covers "state %s(%d)\n", the two arguments, the fact that
 * the print happens AFTER the transition, and `FPM_AGC_Freeze`'s own line on
 * the Y arm.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v32hdxst.h"
#include "dsplib/v32state.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32data.h"
#include "dsplib/v32demod.h"
#include "dsplib/v32dec.h"
#include "dsplib/vtb.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/debug.h"

/* --------------------------------------------------------------------- */

extern void ref_V32AnsNextState(void *modem);

extern short ref_TxHdxCarrierState(void *m, short *d, short *o,
				   unsigned short *l);
extern short ref_TxHdxScrSequence(void *m, short *d, short *o,
				  unsigned short *l);
extern short ref_TxHdxTRN(void *m, short *d, short *o, unsigned short *l);
extern short ref_TxHdxNoCarrier(void *m, short *d, short *o,
				unsigned short *l);
extern short ref_TxHdxFinishFrame(void *m, short *d, short *o,
				  unsigned short *l);

extern void ref_RxHdxNoSignal(void *m, short *i, unsigned short *o,
			      unsigned short *c);
extern void ref_RxHdxPhsReversal(void *m, short *i, unsigned short *o,
				 unsigned short *c);
extern void ref_RxHdxRateSequence(void *m, short *i, unsigned short *o,
				  unsigned short *c);
extern void ref_RxHdxSequence(void *m, short *i, unsigned short *o,
			      unsigned short *c);
extern void ref_RxHdxData(void *m, short *i, unsigned short *o,
			  unsigned short *c);
extern void ref_RxHdxSTone(void *m, short *i, unsigned short *o,
			   unsigned short *c);
extern void ref_RxHdxEpoch(void *m, short *i, unsigned short *o,
			   unsigned short *c);
extern void ref_RxHdxError(void *m, short *i, unsigned short *o,
			   unsigned short *c);

extern const short ref_V32_S_DATA_COEF[15];

extern unsigned int ref_dsplibs_debug_level;

/* --------------------------------------------------------------------- */
/* The context offsets this function drives.  src/pump/v32/v32nsans.c     */
/* carries the derivation of every one of them.                          */

#define HDX_INT_78	0x78
#define HDX_U32_7C	0x7c
#define HDX_U32_80	0x80
#define HDX_INT_90	0x90
#define HDX_SHORT_96	0x96
#define HDX_SHORT_A8	0xa8
#define HDX_SHORT_AA	0xaa

#define OBJ_U8_11	0x11
#define OBJ_OPT11_02	0x02
#define OBJ_U8_32	0x32

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x100
#define DEC_SIZE	0x80
#define FP_SIZE		0x5100
#define PATHS_N		128

#define ECC_NEAR	8
#define ECC_FAR		4
#define ECC_LAG		16
#define ECC_FILL	0x10
#define ECLINE_N	512
#define ECC_COEF	64

#define TONE_LEN	16
#define TONE_EXTRA	4
#define MTD_TONES	3

/* The Viterbi state sits at v32_dec + 0x18; these are its own offsets. */
#define DEC_VTB		0x18
#define VTB_PATHS	0x00
#define VTB_IMAP	0x18
#define VTB_BOUND	0x20
#define VTB_REGION	0x28

/* `struct fpm_tone`'s six pointer fields, which are per fixture. */
#define TONE_CFG_SRC	0x10
#define TONE_KERNEL	0x2c
#define TONE_HISTORY	0x30
#define TONE_REV_BLOCK	0xf4
#define TONE_REV_ACC	0xf8
#define TONE_IIR_SELF	0xfc

/* `struct fpm_mtd`'s two. */
#define MTD_CFG_COEFF	0x00
#define MTD_ACC		0x0c

/*
 * The untouched marker for the two state slots.  Identical on both sides, so
 * "this arm installed nothing" compares equal; never dereferenced, because
 * nothing here runs a state.
 */
#define TX_SENTINEL	((void *)(size_t)0x51000001u)
#define RX_SENTINEL	((void *)(size_t)0x51000002u)

/* --------------------------------------------------------------------- */

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	dec[DEC_SIZE];
	struct vtb_path	paths[PATHS_N];
	struct fpm_tone	tone[3];
	short		kern[3][TONE_LEN];
	short		hist[3][TONE_LEN + TONE_EXTRA];
	short		revb[3][8];
	short		reva[3][8];
	struct fpm_mtd	mtd;
	short		mtdacc[MTD_TONES * 2];
	short		ecline[ECLINE_N];
	short		near_i[ECC_NEAR];
	short		near_q[ECC_NEAR];
	short		far_i[ECC_FAR];
	short		far_q[ECC_FAR];
	short		coef[3][ECC_COEF];
	unsigned char	fp[FP_SIZE];
	double		align;
};

static struct fix fa, fb;

/* Named images, so diff_eq_obj can stringify a type for whichfield.py. */
struct ecline_image { short s[ECLINE_N]; };
struct kern_image { short s[3][TONE_LEN]; };
struct hist_image { short s[3][TONE_LEN + TONE_EXTRA]; };
struct revb_image { short s[3][8]; };
struct mtdacc_image { short s[MTD_TONES * 2]; };
struct coef_image { short s[3][ECC_COEF]; };
struct paths_image { struct vtb_path p[PATHS_N]; };

/* --------------------------------------------------------------------- */

static int rc_total;

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
fill(void *p, int n, unsigned seed)
{
	unsigned char *b = (unsigned char *)p;
	int i;

	rng_seed(seed);
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

static void
put_u32(void *base, int off, unsigned int v)
{
	*(unsigned int *)(void *)((unsigned char *)base + off) = v;
}

static void
put_u8(void *base, int off, unsigned char v)
{
	*((unsigned char *)base + off) = v;
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

/* ONE CHECK PER BLOCK: a wrong 32-bit store is one report and not four. */
static void
cmp_one(const char *what, const char *blk, const void *a, const void *b,
	int len, const struct skip *sk, int nsk, long tag)
{
	char fmt[192];

	sprintf(fmt, "%s: %s differs at byte (tag %%ld)", what, blk);
	diff_eq_int(fmt, first_diff(a, b, len, sk, nsk), -1, tag);
}

#define NSKIP(a)	((int)(sizeof(a) / sizeof((a)[0])))

static const struct skip obj_skip[] = {
	{ V32_OBJ_HDX, 4 }, { V32_OBJ_FP, 4 }
};

static const struct skip hdx_skip[] = {
	{ V32_HDX_TONE0, 12 },		/* the three tone objects, per fixture */
	{ V32_HDX_MTD, 4 },		/* the MTD, per fixture                */
	{ V32HDX_TXSTATE, 8 }		/* + RXSTATE; paired, see the header   */
};

static const struct skip fp_skip[] = {
	{ V32FP_PPS + 0x10, 8 },	/* cfg.imap, cfg.qmap                  */
	{ V32FP_ECC + 0x38, 4 },	/* line                                */
	{ V32FP_ECC + 0x40, 8 },	/* near_i, near_q                      */
	{ V32FP_ECC + 0x4c, 8 },	/* far_i, far_q                        */
	{ V32FP_ECC + 0x54, 12 },	/* the three coefficient banks         */
	{ V32FP_FSE + 0x2c, 8 }		/* cfg.owner, cfg.decision             */
};

static const struct skip dec_skip[] = {
	{ DEC_VTB + VTB_PATHS, 4 },
	{ DEC_VTB + VTB_IMAP, 8 },	/* imap, qmap                          */
	{ DEC_VTB + VTB_BOUND, 4 },
	{ DEC_VTB + VTB_REGION, 4 }
};

static const struct skip tone_skip[] = {
	{ TONE_CFG_SRC, 4 }, { TONE_KERNEL, 8 }, { TONE_REV_BLOCK, 12 }
};

static const struct skip mtd_skip[] = {
	{ MTD_CFG_COEFF, 4 }, { MTD_ACC, 4 }
};

/* --------------------------------------------------------------------- */
/* The pair tables.                                                       */

struct fnpair {
	const char	*name;
	void		*ours;
	void		*reference;
};

static const struct fnpair tx_pairs[] = {
	{ "(unchanged)",	TX_SENTINEL,			TX_SENTINEL },
	{ "TxHdxCarrierState",	(void *)TxHdxCarrierState,
				(void *)ref_TxHdxCarrierState },
	{ "TxHdxScrSequence",	(void *)TxHdxScrSequence,
				(void *)ref_TxHdxScrSequence },
	{ "TxHdxTRN",		(void *)TxHdxTRN,  (void *)ref_TxHdxTRN },
	{ "TxHdxNoCarrier",	(void *)TxHdxNoCarrier,
				(void *)ref_TxHdxNoCarrier },
	{ "TxHdxFinishFrame",	(void *)TxHdxFinishFrame,
				(void *)ref_TxHdxFinishFrame }
};

static const struct fnpair rx_pairs[] = {
	{ "(unchanged)",	RX_SENTINEL,			RX_SENTINEL },
	{ "RxHdxNoSignal",	(void *)RxHdxNoSignal,
				(void *)ref_RxHdxNoSignal },
	{ "RxHdxPhsReversal",	(void *)RxHdxPhsReversal,
				(void *)ref_RxHdxPhsReversal },
	{ "RxHdxRateSequence",	(void *)RxHdxRateSequence,
				(void *)ref_RxHdxRateSequence },
	{ "RxHdxSequence",	(void *)RxHdxSequence,
				(void *)ref_RxHdxSequence },
	{ "RxHdxData",		(void *)RxHdxData, (void *)ref_RxHdxData },
	{ "RxHdxSTone",		(void *)RxHdxSTone, (void *)ref_RxHdxSTone },
	{ "RxHdxEpoch",		(void *)RxHdxEpoch, (void *)ref_RxHdxEpoch },
	{ "RxHdxError",		(void *)RxHdxError, (void *)ref_RxHdxError }
};

#define NTX	((int)(sizeof(tx_pairs) / sizeof(tx_pairs[0])))
#define NRX	((int)(sizeof(rx_pairs) / sizeof(rx_pairs[0])))

static long tx_seen[NTX];
static long rx_seen[NRX];

/*
 * Ours must be one of the pairs; the blob's must then be that pair's `ref_`
 * half.  Returns the index, or -1 if ours is a pointer no arm may install.
 */
static int
check_pair(const char *what, const struct fnpair *tab, int n, void *ours,
	   void *reference, long tag)
{
	int i;

	for (i = 0; i < n; i++)
		if (tab[i].ours == ours)
			break;

	if (i == n) {
		char fmt[160];

		sprintf(fmt, "%s: we installed a pointer no arm may install "
			     "(state %%ld)", what);
		diff_eq_int(fmt, 0, 1, tag);
		return -1;
	}

	{
		char fmt[192];

		sprintf(fmt, "%s: we installed %s, the blob installed "
			     "something else (state %%ld)", what, tab[i].name);
		diff_eq_int(fmt, reference == tab[i].reference, 1, tag);
	}
	return i;
}

/* --------------------------------------------------------------------- */
/* The fixture.                                                           */

static const short tone_src[TONE_LEN] = {
	1000, -900, 800, -700, 600, -500, 400, -300,
	200, -100, 50, -25, 12, -6, 3, -1
};

static const short mtd_coeff_other[15] = {
	-15000, 15700, 27000, -27000, 15700,
	-15000, 15700,     0,      0, 15700,
	-15000, 15700, -27000, 27000, 15700
};

struct trial {
	unsigned	seed;
	short		state;
	short		mode;
	int		i78;
	unsigned int	u7c;
	unsigned int	u80;
	short		a8;
	short		aa;
	short		s96;
	short		symlen;		/* hdx + 0x9e                    */
	unsigned char	opt11;		/* obj + 0x11                    */
	short		bps;		/* obj + 0x04, GetRateV32        */
	int		trellis;	/* obj + 0x1c                    */
	short		reg[V32HDX_NREGS];
	int		detmatch;	/* hdx + 0x68, GetSequence       */
	short		symlen_sel;	/* obj + 0x18, 0 or 1            */
	/*
	 * fp + 0x2a, THE RATE LADDER'S OWN INDEX, and it MUST be in 0..6 --
	 * see the header comment.  It is what `v32_common_rate` subscripts
	 * `V32_RATE_SEQ` with, so leaving it at whatever the seed produced is
	 * an out-of-bounds read on every arm that consults the ladder.
	 */
	short		rx_rate_index;	/* fp + 0x2a, 0..6               */
};

static void
fixture(struct fix *f, const struct trial *t)
{
	unsigned char *e;
	int i;

	fill(f, (int)offsetof(struct fix, align), t->seed);

	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);
	put_ptr(f->obj, V32_OBJ_FP, f->fp);
	put_s16(f->obj, V32_OBJ_BPS, t->bps);
	put_int(f->obj, V32_OBJ_TRELLIS, t->trellis);
	put_s16(f->obj, V32_OBJ_SYMLEN_SEL, t->symlen_sel);
	put_s16(f->obj, V32_OBJ_EC_NEAR_DELAY, 8);
	put_u8(f->obj, OBJ_U8_11, t->opt11);

	/* The handshake context. */
	put_s16(f->hdx, V32HDX_STATE, t->state);
	put_s16(f->hdx, V32HDX_MODE, t->mode);
	put_ptr(f->hdx, V32HDX_TXSTATE, TX_SENTINEL);
	put_ptr(f->hdx, V32HDX_RXSTATE, RX_SENTINEL);
	put_int(f->hdx, HDX_INT_78, t->i78);
	put_u32(f->hdx, HDX_U32_7C, t->u7c);
	put_u32(f->hdx, HDX_U32_80, t->u80);
	put_int(f->hdx, HDX_INT_90, 0x5a5a5a5a);
	put_s16(f->hdx, HDX_SHORT_96, t->s96);
	put_s16(f->hdx, HDX_SHORT_A8, t->a8);
	put_s16(f->hdx, HDX_SHORT_AA, t->aa);
	put_s16(f->hdx, V32HDX_SYMBOL_LEN, t->symlen);
	put_s16(f->hdx, V32_HDX_SHORT_94, 40);
	put_s16(f->hdx, V32_HDX_SHORT_9C, 12);
	for (i = 0; i < V32HDX_NREGS; i++)
		put_s16(f->hdx, V32HDX_REGS + 2 * i, t->reg[i]);
	put_int(f->hdx, V32HDX_DET_MATCH, t->detmatch);

	/* The three tone detectors and the multi-tone detector. */
	for (i = 0; i < 3; i++) {
		f->tone[i].cfg = FPM_TONE_CFG;
		f->tone[i].cfg.freq = (short)(1100 + i * 500);
		f->tone[i].cfg.damp = (short)(31000 - i * 2500);
		f->tone[i].cfg.scale = (short)(8000 + i * 700);
		f->tone[i].cfg.ratio = (short)(20000 + i * 1234);
		f->tone[i].cfg.src = tone_src;
		f->tone[i].cfg.len = TONE_LEN;
		f->tone[i].cfg.extra = TONE_EXTRA;
		f->tone[i].kernel = f->kern[i];
		f->tone[i].history = f->hist[i];
		f->tone[i].rev_block = f->revb[i];
		f->tone[i].rev_acc = f->reva[i];
	}
	put_ptr(f->hdx, V32_HDX_TONE0, &f->tone[0]);
	put_ptr(f->hdx, V32_HDX_TONE1, &f->tone[1]);
	put_ptr(f->hdx, V32_HDX_TONE2, &f->tone[2]);

	/*
	 * The MTD starts on the OTHER coefficient bank, so the L arm's store
	 * of V32_S_DATA_COEF is a change and not a no-op.
	 */
	f->mtd.cfg.coeff = mtd_coeff_other;
	f->mtd.cfg.tones = MTD_TONES;
	f->mtd.cfg.ratio = 12000;
	f->mtd.cfg.min_level = 50;
	f->mtd.cfg.f0a = 0;
	f->mtd.acc = f->mtdacc;
	put_ptr(f->hdx, V32_HDX_MTD, &f->mtd);

	/* The datapump block. */
	/*
	 * IN RANGE, AND THAT IS NOT TIDINESS.  `v32_common_rate` -- inlined
	 * into `DecodeRateSeq`, `CodeRateSeq`, `CodeFinalRateSeq`, `CodeESeq`
	 * and `SeqToRate` -- opens with `V32_RATE_SEQ[fp->rx_rate_index]` and
	 * the object has no bound check, so a random sixteen-bit index reads
	 * up to 64 KB either side of a seven-entry table.
	 */
	put_s16(f->fp, V32FP_RX_RATE_INDEX, t->rx_rate_index);
	put_s16(f->fp, V32FP_TX_RATE_INDEX, 3);
	put_ptr(f->fp, V32FP_FSE + 0x2c, f->dec);
	put_ptr(f->dec, DEC_VTB + VTB_PATHS, f->paths);

	e = f->fp + V32FP_ECC;
	put_s16(e, 0x00, ECC_LAG);		/* cfg.far_lag              */
	put_s16(e, 0x02, ECC_NEAR);		/* cfg.near_taps            */
	put_s16(e, 0x04, ECC_FAR);		/* cfg.far_taps             */
	put_s16(e, 0x06, 0);
	put_ptr(e, 0x08, 0);			/* cfg.imap, unread by init */
	put_ptr(e, 0x0c, 0);			/* cfg.qmap                 */
	put_s16(e, 0x10, ECC_FILL);		/* cfg.fill                 */
	put_s16(e, 0x12, 0);
	put_ptr(e, 0x14, 0);			/* cfg.aux                  */
	put_s16(e, 0x34, 120);			/* line_len                 */
	put_ptr(e, 0x38, f->ecline);
	put_ptr(e, 0x40, f->near_i);
	put_ptr(e, 0x44, f->near_q);
	put_ptr(e, 0x4c, f->far_i);
	put_ptr(e, 0x50, f->far_q);
	put_ptr(e, 0x54, f->coef[0]);
	put_ptr(e, 0x58, f->coef[1]);
	put_ptr(e, 0x5c, f->coef[2]);
	put_s16(e, 0x60, 320);			/* the update gain          */
	put_s16(e, 0x66, 0);			/* far_delay                */
}

/* --------------------------------------------------------------------- */
/* Non-vacuity counters.  Every one is asserted at the end.                */

static long arm_taken[V32_STATE_COUNT + 2];
static long arm_default;
/*
 * The five arms that can decline to do anything: A, C, D, E and F.  B2 is NOT
 * one of them -- its test picks between two things it does, and never between
 * doing something and doing nothing -- so it is counted separately below.
 */
static const char *const cond_name[5] = { "A", "C", "D", "E", "F" };
static long cond_fired[5];
static long cond_declined[5];
static long b2_to_c, b2_to_error;
static long sep_out_of_range, sep_negative_state;
static long sep_u7c_negative, sep_i78_negative;
static long sep_a8_negative, sep_aa_negative;
static long sep_opt11_set, sep_opt11_clear, sep_opt11_neighbour;
static long sep_x2_residue[8], sep_x2_wrapped;
static long sep_rate_seen[V32_RATE_COUNT];
static long sep_local_index[V32_RATE_COUNT];
static long sep_fault_posted;
static long sep_symlen_negative;
static long sep_mtd_retuned;
static long sep_mode_swept[V32_NEXTSTATE_COUNT];
static long sep_dbg_lines;
static long sep_dbg_trials;

/* --------------------------------------------------------------------- */

static void
compare_all(const char *what, long tag)
{
	cmp_one(what, "instance", fa.obj, fb.obj, OBJ_SIZE, obj_skip,
		NSKIP(obj_skip), tag);
	cmp_one(what, "context", fa.hdx, fb.hdx, HDX_SIZE, hdx_skip,
		NSKIP(hdx_skip), tag);
	cmp_one(what, "datapump", fa.fp, fb.fp, FP_SIZE, fp_skip,
		NSKIP(fp_skip), tag);
	cmp_one(what, "decoder", fa.dec, fb.dec, DEC_SIZE, dec_skip,
		NSKIP(dec_skip), tag);
	cmp_one(what, "tone 0", &fa.tone[0], &fb.tone[0],
		(int)sizeof(fa.tone[0]), tone_skip, NSKIP(tone_skip), tag);
	cmp_one(what, "tone 1", &fa.tone[1], &fb.tone[1],
		(int)sizeof(fa.tone[1]), tone_skip, NSKIP(tone_skip), tag);
	cmp_one(what, "tone 2", &fa.tone[2], &fb.tone[2],
		(int)sizeof(fa.tone[2]), tone_skip, NSKIP(tone_skip), tag);
	cmp_one(what, "multi-tone detector", &fa.mtd, &fb.mtd,
		(int)sizeof(fa.mtd), mtd_skip, NSKIP(mtd_skip), tag);

	diff_eq_obj("the echo line", struct ecline_image, fa.ecline,
		    fb.ecline, tag);
	diff_eq_obj("the tone kernels", struct kern_image, fa.kern, fb.kern,
		    tag);
	diff_eq_obj("the tone histories", struct hist_image, fa.hist,
		    fb.hist, tag);
	diff_eq_obj("the reversal blocks", struct revb_image, fa.revb,
		    fb.revb, tag);
	diff_eq_obj("the reversal accumulators", struct revb_image, fa.reva,
		    fb.reva, tag);
	diff_eq_obj("the MTD accumulator", struct mtdacc_image, fa.mtdacc,
		    fb.mtdacc, tag);
	diff_eq_obj("the canceller coefficients", struct coef_image, fa.coef,
		    fb.coef, tag);
	diff_eq_obj("the survivor ring", struct paths_image, fa.paths,
		    fb.paths, tag);
}

/*
 * The two state slots and the MTD's coefficient bank, by identity.  Each of
 * the three holds a different address on each side by construction.
 */
static void
compare_pointers(long tag)
{
	int i;

	i = check_pair("the transmit state", tx_pairs, NTX,
		       get_ptr(fa.hdx, V32HDX_TXSTATE),
		       get_ptr(fb.hdx, V32HDX_TXSTATE), tag);
	if (i >= 0)
		tx_seen[i]++;

	i = check_pair("the receive state", rx_pairs, NRX,
		       get_ptr(fa.hdx, V32HDX_RXSTATE),
		       get_ptr(fb.hdx, V32HDX_RXSTATE), tag);
	if (i >= 0)
		rx_seen[i]++;

	if (fa.mtd.cfg.coeff == V32_S_DATA_COEF) {
		diff_eq_int("the MTD was retuned: the blob stored its own "
			    "V32_S_DATA_COEF (state %ld)",
			    fb.mtd.cfg.coeff == ref_V32_S_DATA_COEF, 1, tag);
		sep_mtd_retuned++;
	} else {
		diff_eq_int("the MTD was left alone: so was the blob's "
			    "(state %ld)",
			    fb.mtd.cfg.coeff == mtd_coeff_other, 1, tag);
		diff_eq_int("the MTD was left alone: ours too (state %ld)",
			    fa.mtd.cfg.coeff == mtd_coeff_other, 1, tag);
	}
}

/* --------------------------------------------------------------------- */

static void
note_coverage(const struct trial *t)
{
	int st = t->state;

	if (st >= 0 && st < V32_STATE_COUNT)
		arm_taken[st]++;
	if (st < 0 || st > 0x21)
		arm_default++;
	if (st > 0x21)
		sep_out_of_range++;
	if (st < 0)
		sep_negative_state++;
	if ((unsigned int)t->u7c > 0x7fffffffu)
		sep_u7c_negative++;
	if (t->i78 < 0)
		sep_i78_negative++;
	if (t->a8 < 0)
		sep_a8_negative++;
	if (t->aa < 0)
		sep_aa_negative++;
	if (t->symlen < 0)
		sep_symlen_negative++;
	if (t->mode >= 0 && t->mode < V32_NEXTSTATE_COUNT)
		sep_mode_swept[t->mode]++;

	/* Which way each conditional arm went, read off the new state. */
	switch (st) {
	case V32_STATE_A:
		if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_B)
			cond_fired[0]++;
		else
			cond_declined[0]++;
		break;
	case V32_STATE_B2:
		if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_ERROR)
			b2_to_error++;
		else if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_C)
			b2_to_c++;
		break;
	case V32_STATE_C:
		if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_D)
			cond_fired[1]++;
		else
			cond_declined[1]++;
		break;
	case V32_STATE_D:
		if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_E)
			cond_fired[2]++;
		else
			cond_declined[2]++;
		break;
	case V32_STATE_E:
		if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_F)
			cond_fired[3]++;
		else
			cond_declined[3]++;
		break;
	case V32_STATE_F:
		if (get_s16(fa.hdx, V32HDX_STATE) == V32_STATE_F2)
			cond_fired[4]++;
		else
			cond_declined[4]++;
		break;
	case V32_STATE_B:
		if (t->opt11 & OBJ_OPT11_02)
			sep_opt11_set++;
		else if (t->opt11 & (unsigned char)~OBJ_OPT11_02)
			sep_opt11_neighbour++;
		else
			sep_opt11_clear++;
		break;
	case V32_STATE_X2:
		sep_x2_residue[(t->u80 - (unsigned int)t->i78) & 7u]++;
		if (t->u80 < (unsigned int)t->i78)
			sep_x2_wrapped++;
		break;
	default:
		break;
	}

	/*
	 * Which rate the ladder produced, for the arms that consult it.
	 * `DecodeRateSeq` reads the instance and writes nothing, so calling it
	 * here cannot disturb the comparison that has already happened.
	 */
	if (st == V32_STATE_T || st == V32_STATE_Y || st == V32_STATE_Z
	    || st == V32_STATE_END || st == V32_STATE_L) {
		short reg = st == V32_STATE_T ? t->reg[2] : t->reg[4];
		short rate = DecodeRateSeq(fa.obj, (unsigned short)reg);

		if (rate >= 0 && rate < V32_RATE_COUNT)
			sep_rate_seen[rate]++;
	}

	if (get_u8(fa.obj, V32_OBJ_FLAGS) & V32_FLAG_FAULT)
		sep_fault_posted++;
}

static void
run_one(const struct trial *t)
{
	char what[96];

	sprintf(what, "V32AnsNextState: state %d mode %d", (int)t->state,
		(int)t->mode);

	fixture(&fa, t);
	fixture(&fb, t);

	V32AnsNextState(fa.obj);
	ref_V32AnsNextState(fb.obj);

	diff_begin(what);
	compare_all(what, (long)t->state);
	compare_pointers((long)t->state);
	rc_total |= diff_end();

	note_coverage(t);
}

/* --------------------------------------------------------------------- */
/*
 * The sweeps.  `base` is a context in which no conditional arm fires and no
 * counter is on a boundary; each sweep perturbs the fields its own arms read.
 */

static const struct trial base = {
	0x5a5a0001u,			/* seed                              */
	0,				/* state, overwritten                */
	0,				/* mode, overwritten                 */
	0x400,				/* +0x78, well above zero            */
	0x20u,				/* +0x7c, well below 0x8b            */
	0x180u,				/* +0x80                             */
	4,				/* +0xa8, below 0x48                 */
	7,				/* +0xaa, below 0x5f                 */
	0x2345,				/* +0x96                             */
	48,				/* +0x9e                             */
	0,				/* obj + 0x11                        */
	9600,				/* bps                               */
	1,				/* trellis                           */
	{ 0x0111, 0x0333, 0x1111, 0x0f0f, 0x0555 },
	0x1234abcd,			/* the detector's last match         */
	1,				/* symlen_sel                        */
	/*
	 * Index 5 is `V32_RATE_SEQ[5]` == 0x0ff9, the only entry with every
	 * one of the ladder's five test bits set, so the local station can
	 * meet whatever the far end offers and `sweep_rates` can reach all
	 * seven outcomes by varying the SEQUENCE alone.
	 */
	5				/* fp + 0x2a                         */
};

static void
sweep_states(void)
{
	int st;
	int m;
	unsigned n = 0;

	/*
	 * Every state, in and out of range, against every mode slot.  The mode
	 * is never read by this function; driving it is what says so.
	 */
	for (st = -1; st <= 36; st++)
	for (m = 0; m < V32_NEXTSTATE_COUNT; m++) {
		struct trial t = base;

		t.seed = 0x5a5a0001u + (unsigned)(st + 8) * 977u
			 + (unsigned)m * 31u;
		t.state = (short)st;
		t.mode = (short)m;
		t.reg[4] = (short)(0x0111 + (int)n * 0x111);
		n++;
		run_one(&t);
	}

	/* The two states a signed bound would let into the table. */
	{
		struct trial t = base;

		t.seed = 0x7c0de001u;
		t.state = (short)-32768;
		t.mode = V32_MODE_ANSWER;
		run_one(&t);

		t.seed = 0x7c0de002u;
		t.state = (short)-256;
		run_one(&t);
	}
}

static void
sweep_conditionals(void)
{
	static const int i78v[] = { -0x10000, -1, 0, 1, 0x400 };
	static const unsigned int u7cv[] = { 0, 0x8au, 0x8bu, 0x8cu,
					     0x80000000u, 0xffffffffu };
	static const short a8v[] = { -1, (short)-32768, 0, 0x47, 0x48, 0x49,
				     0x7fff };
	static const short aav[] = { -1, (short)-32768, 0, 0x5e, 0x5f, 0x60,
				     0x7fff };
	unsigned i;

	/* A, B2 and E all test +0x78. */
	for (i = 0; i < sizeof(i78v) / sizeof(i78v[0]); i++) {
		struct trial t = base;
		int st;

		t.i78 = i78v[i];
		for (st = 0; st < 3; st++) {
			static const short which[3] = {
				V32_STATE_A, V32_STATE_B2, V32_STATE_E
			};

			t.seed = 0x11220000u + i * 101u + (unsigned)st;
			t.state = which[st];
			t.mode = V32_MODE_ANSWER;
			run_one(&t);
		}
	}

	/* C tests +0x7c, and it tests it UNSIGNED. */
	for (i = 0; i < sizeof(u7cv) / sizeof(u7cv[0]); i++) {
		struct trial t = base;

		t.seed = 0x22330000u + i * 103u;
		t.state = V32_STATE_C;
		t.mode = V32_MODE_ANSWER;
		t.u7c = u7cv[i];
		run_one(&t);

		/* D reads the same field on the way past. */
		t.seed = 0x22440000u + i * 103u;
		t.state = V32_STATE_D;
		t.a8 = 0x60;
		run_one(&t);
	}

	/* D tests +0xa8, signed. */
	for (i = 0; i < sizeof(a8v) / sizeof(a8v[0]); i++) {
		struct trial t = base;

		t.seed = 0x33440000u + i * 107u;
		t.state = V32_STATE_D;
		t.mode = V32_MODE_ANSWER;
		t.a8 = a8v[i];
		t.u7c = 0x33u + i;
		run_one(&t);
	}

	/* F tests +0xaa, signed, and posts three bytes when it fires. */
	for (i = 0; i < sizeof(aav) / sizeof(aav[0]); i++) {
		struct trial t = base;

		t.seed = 0x44550000u + i * 109u;
		t.state = V32_STATE_F;
		t.mode = V32_MODE_ANSWER;
		t.aa = aav[i];
		t.s96 = (short)(0x1000 + (int)i * 0x321);
		run_one(&t);
	}
}

static void
sweep_option_bit(void)
{
	static const unsigned char optv[] = { 0x00, 0x01, 0x02, 0x03, 0x04,
					      0x06, 0xfd, 0xff };
	unsigned i;

	for (i = 0; i < sizeof(optv) / sizeof(optv[0]); i++) {
		struct trial t = base;

		t.seed = 0x55660000u + i * 113u;
		t.state = V32_STATE_B;
		t.mode = V32_MODE_ANSWER;
		t.opt11 = optv[i];
		run_one(&t);
	}
}

static void
sweep_x2(void)
{
	unsigned i;

	/* Every residue of (limit - budget) mod 8, plus a wrapping pair. */
	for (i = 0; i < 8; i++) {
		struct trial t = base;

		t.seed = 0x66770000u + i * 127u;
		t.state = V32_STATE_X2;
		t.mode = V32_MODE_ANSWER;
		t.i78 = 0x100;
		t.u80 = 0x100u + i;
		t.detmatch = (int)(0xabcd0000u + i);
		run_one(&t);

		t.seed = 0x66880000u + i * 127u;
		t.u80 = 0x40u + i;	/* below the budget: the sub wraps  */
		run_one(&t);
	}
}

static void
sweep_rates(void)
{
	static const short bpsv[] = { 4800, 7200, 9600, 12000, 14400, 2400,
				      9600 };
	/*
	 * The first five isolate one rung each against `V32_RATE_SEQ[5]`:
	 * bit 10 alone is rate 0, bit 9 alone rate 1, bits 9+7 rate 2, bit 6
	 * rate 3, bit 5 rate 4, bit 3 rate 5, and nothing in common is
	 * V32_RATE_NONE.  The rest are the wide patterns, which land on the
	 * ladder's first match.
	 */
	static const short seqv[] = { 0x0000, 0x0400, 0x0200, 0x0280, 0x0040,
				      0x0020, 0x0008, 0x0111, 0x0333, 0x0f0f,
				      (short)0xffff };
	static const short statev[] = {
		V32_STATE_L, V32_STATE_T, V32_STATE_Y, V32_STATE_Z,
		V32_STATE_END
	};
	unsigned idx, s, st;

	/*
	 * The LOCAL index is swept beside the sequence, because the ladder is
	 * a bitwise AND of the two and a fixed local station can only ever
	 * reach the rungs its own entry carries.
	 */
	for (idx = 0; idx < V32_RATE_COUNT; idx++)
	for (s = 0; s < sizeof(seqv) / sizeof(seqv[0]); s++)
	for (st = 0; st < sizeof(statev) / sizeof(statev[0]); st++) {
		struct trial t = base;
		int i;

		t.seed = 0x77880000u + idx * 1009u + s * 37u + st;
		t.state = statev[st];
		t.mode = V32_MODE_ANSWER;
		t.rx_rate_index = (short)idx;
		t.bps = bpsv[idx];
		t.trellis = (int)(idx & 1u);
		for (i = 0; i < V32HDX_NREGS; i++)
			t.reg[i] = seqv[s];
		run_one(&t);
		sep_local_index[idx]++;
	}
}

static void
sweep_symbol_length(void)
{
	static const short lenv[] = { -1, (short)-32768, 0, 1, 12, 48, 0x7fff };
	unsigned i;

	/* F2 copies hdx + 0x9e into +0x78 through a SIGNED load. */
	for (i = 0; i < sizeof(lenv) / sizeof(lenv[0]); i++) {
		struct trial t = base;

		t.seed = 0x88990000u + i * 131u;
		t.state = V32_STATE_F2;
		t.mode = V32_MODE_ANSWER;
		t.symlen = lenv[i];
		run_one(&t);
	}
}

/* --------------------------------------------------------------------- */
/* The diagnostic path.                                                   */

static void
sweep_debug(void)
{
	int st;

	dsplib_debug_capture_on = 1;

	for (st = -1; st <= 35; st++) {
		struct trial t = base;
		char what[96];

		t.seed = 0x99aa0000u + (unsigned)(st + 8) * 149u;
		t.state = (short)st;
		t.mode = V32_MODE_ANSWER;

		fixture(&fa, &t);
		fixture(&fb, &t);

		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2u;
		ref_dsplibs_debug_level = 2u;

		V32AnsNextState(fa.obj);
		ref_V32AnsNextState(fb.obj);

		dsplibs_debug_level = 0u;
		ref_dsplibs_debug_level = 0u;

		sprintf(what, "V32AnsNextState: transcript, state %d", st);
		diff_begin(what);
		diff_eq_int("the transcripts differ (state %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)st);
		diff_eq_int("the blob printed something (state %ld)",
			    dsplib_debug_capture_lines(1) > 0, 1, (long)st);
		compare_all(what, (long)st);
		compare_pointers((long)st);
		rc_total |= diff_end();

		if (dsplib_debug_capture_lines(0) > 0)
			sep_dbg_lines++;
		sep_dbg_trials++;
	}

	dsplib_debug_capture_on = 0;
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;
	int i;
	long arms = 0;
	long tx_used = 0;
	long rx_used = 0;
	long residues = 0;
	long rates = 0;
	long locals = 0;
	long modes = 0;

	sweep_states();
	sweep_conditionals();
	sweep_option_bit();
	sweep_x2();
	sweep_rates();
	sweep_symbol_length();
	sweep_debug();

	for (i = 0; i < V32_STATE_COUNT; i++)
		if (arm_taken[i] > 0)
			arms++;
	for (i = 0; i < NTX; i++)
		if (tx_seen[i] > 0)
			tx_used++;
	for (i = 0; i < NRX; i++)
		if (rx_seen[i] > 0)
			rx_used++;
	for (i = 0; i < 8; i++)
		if (sep_x2_residue[i] > 0)
			residues++;
	for (i = 0; i < V32_RATE_COUNT; i++)
		if (sep_rate_seen[i] > 0)
			rates++;
	for (i = 0; i < V32_RATE_COUNT; i++)
		if (sep_local_index[i] > 0)
			locals++;
	for (i = 0; i < V32_NEXTSTATE_COUNT; i++)
		if (sep_mode_swept[i] > 0)
			modes++;

	diff_begin("v32nsans separating trials");

	diff_eq_int("every one of the 35 handshake states was driven (%ld)",
		    arms, V32_STATE_COUNT, arms);
	diff_eq_int("the default arm was reached (%ld)", arm_default > 0, 1,
		    arm_default);
	diff_eq_int("a state past the jump table was driven (%ld)",
		    sep_out_of_range > 0, 1, sep_out_of_range);
	diff_eq_int("a NEGATIVE state was driven (%ld)",
		    sep_negative_state > 0, 1, sep_negative_state);

	for (i = 0; i < 5; i++) {
		char fmt[96];

		sprintf(fmt, "conditional arm %s fired (%%ld)", cond_name[i]);
		diff_eq_int(fmt, cond_fired[i] > 0, 1, cond_fired[i]);
		sprintf(fmt, "conditional arm %s declined (%%ld)",
			cond_name[i]);
		diff_eq_int(fmt, cond_declined[i] > 0, 1, cond_declined[i]);
	}
	diff_eq_int("the B2 arm reached C (%ld)", b2_to_c > 0, 1, b2_to_c);
	diff_eq_int("the B2 arm reached ERROR (%ld)", b2_to_error > 0, 1,
		    b2_to_error);

	diff_eq_int("every transmit state in the pair table was installed "
		    "(%ld)", tx_used, NTX, tx_used);
	diff_eq_int("every receive state in the pair table was installed "
		    "(%ld)", rx_used, NRX, rx_used);

	diff_eq_int("+0x78 was driven negative (%ld)", sep_i78_negative > 0, 1,
		    sep_i78_negative);
	diff_eq_int("+0x7c was driven above 0x7fffffff (%ld)",
		    sep_u7c_negative > 0, 1, sep_u7c_negative);
	diff_eq_int("+0xa8 was driven negative (%ld)", sep_a8_negative > 0, 1,
		    sep_a8_negative);
	diff_eq_int("+0xaa was driven negative (%ld)", sep_aa_negative > 0, 1,
		    sep_aa_negative);
	diff_eq_int("hdx + 0x9e was driven negative (%ld)",
		    sep_symlen_negative > 0, 1, sep_symlen_negative);

	diff_eq_int("the B arm saw its option bit set (%ld)",
		    sep_opt11_set > 0, 1, sep_opt11_set);
	diff_eq_int("the B arm saw it clear (%ld)", sep_opt11_clear > 0, 1,
		    sep_opt11_clear);
	diff_eq_int("the B arm saw a NEIGHBOURING bit set instead (%ld)",
		    sep_opt11_neighbour > 0, 1, sep_opt11_neighbour);

	diff_eq_int("all eight X2 residues were driven (%ld)", residues, 8,
		    residues);
	diff_eq_int("the X2 subtraction was made to wrap (%ld)",
		    sep_x2_wrapped > 0, 1, sep_x2_wrapped);

	diff_eq_int("every rate including NONE came out of the ladder (%ld)",
		    rates, V32_RATE_COUNT, rates);
	diff_eq_int("every local rate index was driven (%ld)", locals,
		    V32_RATE_COUNT, locals);
	diff_eq_int("a fault was posted (%ld)", sep_fault_posted > 0, 1,
		    sep_fault_posted);
	diff_eq_int("the MTD was retuned to V32_S_DATA_COEF (%ld)",
		    sep_mtd_retuned > 0, 1, sep_mtd_retuned);

	diff_eq_int("every mode slot was driven (%ld)", modes,
		    V32_NEXTSTATE_COUNT, modes);

	diff_eq_int("the diagnostic path printed on our side (%ld)",
		    sep_dbg_lines > 0, 1, sep_dbg_lines);
	diff_eq_int("every state was driven through the diagnostic path (%ld)",
		    sep_dbg_trials, 37, sep_dbg_trials);

	rc |= diff_end();

	return rc | rc_total;
}
