/*
 * t_v32rxhdx.c -- differential test of V.32's twelve half-duplex RECEIVE
 *                 states, `src/pump/v32/V32rxhdx.c`.
 *
 * Every one of the twelve is a plumber: it charges a counter, asks one or two
 * already-reconstructed detectors a question, maybe dispatches through
 * `V32NextState`, maybe posts a fault, and always clamps the block on the way
 * out.  So what can be wrong is WHICH sub-object, WHICH buffer, WHICH count,
 * WHICH branch and WHICH successor -- and every one of those agrees with its
 * wrong reading over most inputs.  The fixture exists to separate them.
 *
 * ---------------------------------------------------------------------------
 * EVERY CHECK IS BUILT AROUND A NAMED WRONG READING
 *
 *   - the AGC taken from the DATAPUMP block rather than from the CONTEXT.
 *     `struct fpm_agc` sits at hdx + 0x00 and there is a second one at
 *     fp + 0x1d8; both are real objects here, both are initialised from the
 *     same config, and only one of them may move.
 *   - the wrong tone object.  hdx + 0x2c, +0x30 and +0x34 hold three, and
 *     `RxHdxPhsReversal` uses all three -- +0x30 and +0x34 for the two kills
 *     and +0x2c for the detector and the reversal search.  The three are
 *     seeded with DIFFERENT running energies, so a swap is visible in the
 *     verdict as well as in the object.
 *   - the counter at hdx + 0x7c compared SIGNED.  Every comparison in the
 *     object is `jb`/`jbe`; trials with the counter above 0x7fffffff separate
 *     the two readings and nothing else does.
 *   - `>=` written as `>` at the bound.  A trial lands the counter exactly on
 *     the bound.
 *   - the fault posted with the wrong reason code.  The six codes 0x10..0x15
 *     are compared byte for byte, and 0x10 and 0x14 are each shared by two
 *     states, so a table indexed by state would get four of them wrong.
 *   - the successor pointers transposed.  hdx + 0x6c must take
 *     `TxHdxNoCarrier` and hdx + 0x70 `RxHdxError`; both are compared as an
 *     IDENTITY (ours against ours, the blob's against the blob's), because
 *     the two sides hold different addresses and always will.
 *   - `RxHdxTone`'s three-term gate read as a conjunction.  All eight
 *     combinations of the three terms are driven and the AGC's own state says
 *     whether the block was processed.
 *   - `RxHdxNoSignal`'s `> 60` read as `>=` or as signed.
 *   - the run counter at hdx + 0xac read the obvious way round.  It counts the
 *     blocks the tone is PRESENT in, because `FPM_TONE_detect` reports
 *     presence as ZERO; every trial that seeds it drives both senses.
 *   - `RxHdxPhsReversal`'s second test read as an `else`.  A next-state stub
 *     CLEARS hdx + 0x90 from inside the dispatch, which is the only way the
 *     tone-detect arm can run in the same call as the reversal arm.
 *   - the context cached across the dispatch.  Another stub swaps obj + 0x64
 *     for a second, differently-seeded context, and everything after the
 *     dispatch must come from the new one.
 *   - `RxHdxRateSequence` calling `GetSequence` once.  The object calls it
 *     twice and compares the match word's halves; a trial makes the halves
 *     differ, which is the only shape that separates the comparison from a
 *     bare `DetSequence >= 0`.
 *   - `RxHdxSequenceE`'s three arms collapsed.  The counter arm, the decode
 *     arm and the timeout arm are mutually exclusive in the object, so a
 *     trial that arms the counter must NOT post a fault however far past the
 *     bound it is.
 *   - `RxHdxSTone` running the detector over the wrong buffer.  The two arms
 *     hand `FPM_MTD_detect` different buffers and different lengths, and the
 *     input buffer and the datapump's working buffer are seeded differently.
 *   - `RxHdxToneData` demodulating before the timeout check rather than
 *     after.  A trial times out and the demodulated block is compared.
 *   - `RxHdxError` reading the context at all.  It is the one state that does
 *     not, and its context is left as a canary.
 *
 * ---------------------------------------------------------------------------
 * HOW THE DETECTORS ARE STEERED
 *
 * `FPM_TONE_detect` and `FPM_MTD_detect` both skip their sample loop entirely
 * when the count is zero and then answer from the energies already in the
 * object.  So a count of zero turns each into a function of the fixture, and
 * every verdict is reachable exactly: the thresholds are read back out of the
 * created object rather than assumed, so this does not depend on which
 * configuration the built-in is.
 *
 * `FPM_TONE_find_rev` needs a non-zero count -- its report comes from inside
 * the loop -- so the reversal trials feed eight samples of silence with
 * `rev_age` past its 160-sample debounce and the correlation seeded at the
 * bottom of its range, which makes the very first sample qualify.
 *
 * The trials with a real signal and a real count are what cover the plumbing
 * the forced ones cannot: which buffer, which length, and the equaliser's
 * output landing in the caller's `out`.
 *
 * ---------------------------------------------------------------------------
 * THE MASKS ARE BUILT, NOT DECLARED
 *
 * Five blocks hold heap addresses that differ between the two fixtures for
 * ever -- the datapump block, the decoder, and the three tone objects and the
 * multi-tone detector, all built by the blob's own `ref_FPM_*` constructors.
 * For each, the bytes that differ IMMEDIATELY AFTER CONSTRUCTION are the mask,
 * and `main` asserts every mask is small and non-empty, so a mask that
 * swallowed its block fails rather than passing silently.  What the pointers
 * point at is compared separately and unmasked.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/v32hdxst.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v32data.h"
#include "dsplib/v32demod.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32state.h"

/* --------------------------------------------------------------------- */
/* The blob's side of everything this file touches.                       */

extern void ref_RxHdxTone(struct v32_modem *m, short *in, unsigned short *o,
			  unsigned short *c);
extern void ref_RxHdxNoSignal(struct v32_modem *m, short *in, unsigned short *o,
			      unsigned short *c);
extern void ref_RxHdxPhsReversal(struct v32_modem *m, short *in,
				 unsigned short *o, unsigned short *c);
extern void ref_RxHdxRateSequence(struct v32_modem *m, short *in,
				  unsigned short *o, unsigned short *c);
extern void ref_RxHdxSequence(struct v32_modem *m, short *in, unsigned short *o,
			      unsigned short *c);
extern void ref_RxHdxSequenceE(struct v32_modem *m, short *in,
			       unsigned short *o, unsigned short *c);
extern void ref_RxHdxData(struct v32_modem *m, short *in, unsigned short *o,
			  unsigned short *c);
extern void ref_RxHdxToneData(struct v32_modem *m, short *in, unsigned short *o,
			      unsigned short *c);
extern void ref_RxHdxSTone(struct v32_modem *m, short *in, unsigned short *o,
			   unsigned short *c);
extern void ref_RxHdxEpoch(struct v32_modem *m, short *in, unsigned short *o,
			   unsigned short *c);
extern void ref_RxHdxError(struct v32_modem *m, short *in, unsigned short *o,
			   unsigned short *c);
extern void ref_RxHdxNull(struct v32_modem *m, short *in, unsigned short *o,
			  unsigned short *c);

extern short ref_TxHdxNoCarrier(struct v32_modem *m, short *d, short *o,
				unsigned short *left);

extern v32_nextstate_fn ref_V32NextState[V32_NEXTSTATE_COUNT];
extern const short ref_V32_S_DATA_COEF[15];

extern unsigned int ref_dsplibs_debug_level;

extern struct fpm_tone *ref_FPM_TONE_create(struct fpm_tone *s,
					    const struct fpm_tone_cfg *c);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *s,
					  const struct fpm_mtd_cfg *c);

extern void ref_FPM_MRF_init(struct fpm_mrf *s, const struct fpm_mrf_cfg *c,
			     int fresh);
extern void ref_FPM_ECC_init(struct fpm_ecc *s, const struct fpm_ecc_cfg *c,
			     int fresh);
extern void ref_FPM_SRE_init(struct fpm_sre *s, const struct fpm_sre_cfg *c,
			     int fresh);
extern void ref_FPM_AGC_init(struct fpm_agc *s, const struct fpm_agc_cfg *c,
			     int reset);
extern void ref_FPM_FSE_init(struct fpm_fse *s, const struct fpm_fse_cfg *c,
			     int fresh);

extern const struct fpm_mrf_cfg ref_MRFv32_CFG;
extern const struct fpm_ecc_cfg ref_ECCv32_CFG;
extern const struct fpm_sre_cfg ref_SREv32_CFG;
extern struct fpm_agc_cfg ref_AGCv32_CFG;
extern struct fpm_fse_cfg ref_FSEv32_CFG;

/* --------------------------------------------------------------------- */
/*
 * THE OFFSETS `src/pump/v32/V32rxhdx.c` DEFINES PRIVATELY.
 *
 * They are deliberately not in a shared header -- the file that owns them
 * says why -- so this test spells them again.  Nothing checks the two copies
 * against each other except this test failing, which is exactly what a wrong
 * offset on either side would cause.
 */
#define T_OBJ_PROTOCOL		0x00	/* short                             */
#define T_OBJ_OPTIONS		0x10	/* int                               */
#define T_OPT_0400		0x0400
#define T_FLAG_04		0x04

#define T_HDX_AGC		0x00	/* struct fpm_agc                    */
#define T_HDX_SHORT_48		0x48
#define T_HDX_INT_78		0x78
#define T_HDX_TIMER		0x7c	/* unsigned int                      */
#define T_HDX_LIMIT		0x80	/* unsigned int                      */
#define T_HDX_INT_90		0x90
#define T_HDX_RTD		0x96	/* short                             */
#define T_HDX_SHORT_A8		0xa8
#define T_HDX_SHORT_AA		0xaa
#define T_HDX_SHORT_AC		0xac

/* v32dec.h's `rate_change`, which `EpochDetectV32` returns straight through. */
#define T_DEC_RATE_CHANGE	0x64
/* t_v32fpctl.c's, and for the same reason: `struct vtb`'s survivor ring. */
#define T_DEC_VTB		0x18

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x100
#define FP_SIZE		0x50e0		/* past V32FP_CLEANLEN + 2           */
#define DEC_SIZE	0x80
#define PATHS_SIZE	4096

#define BUFN		256		/* V32FP_RXBUF / V32FP_CLEAN         */
#define BUFSLACK	64
#define ION		512

#define ECC_DELAY	40		/* fpm_ecc's two INPUT fields        */
#define CANARY_RX	((short)0x3c3c)
#define CANARY_CL	((short)0x6d6d)
#define CANARY_OUT	((unsigned short)0x9e9e)

#define MTD_TONES	3
#define MTD_RATIO	16384
#define MTD_MIN		100

#define MAXNS		16
#define NTONE		3

/* The twelve, in the object's address order. */
#define FN_TONE		0
#define FN_NOSIGNAL	1
#define FN_PHSREV	2
#define FN_RATESEQ	3
#define FN_SEQ		4
#define FN_SEQE		5
#define FN_DATA		6
#define FN_TONEDATA	7
#define FN_STONE	8
#define FN_EPOCH	9
#define FN_ERROR	10
#define FN_NULL		11
#define FN_COUNT	12

static const char *const fn_name[FN_COUNT] = {
	"RxHdxTone", "RxHdxNoSignal", "RxHdxPhsReversal", "RxHdxRateSequence",
	"RxHdxSequence", "RxHdxSequenceE", "RxHdxData", "RxHdxToneData",
	"RxHdxSTone", "RxHdxEpoch", "RxHdxError", "RxHdxNull"
};

/* What the next-state stub does to the context it was dispatched from. */
#define NS_NOTHING	0
#define NS_CLEAR_90	1
#define NS_SET_90	2
#define NS_SWAP_CTX	3

/* How the tone detector's verdict is forced; -1 leaves it to the signal. */
#define TONE_FREE	(-1)
#define TONE_PRESENT	0
#define TONE_OTHER	1
#define TONE_NOSIG	2

/* The same for the multi-tone detector. */
#define MTD_FREE	(-1)
#define MTD_ABSENT	0
#define MTD_PRESENT	1
#define MTD_NOSIG	2

/* How the sequence detector is armed. */
#define DET_MISS	0	/* width 0: the inner loop never runs      */
#define DET_HIT_EQ	1	/* hits, and the match word's halves agree */
#define DET_HIT_NE	2	/* hits, and they do not                   */

struct fix {
	double		align;
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	alt[HDX_SIZE];
	unsigned char	fp[FP_SIZE];
	unsigned char	dec[DEC_SIZE];
	unsigned char	paths[PATHS_SIZE];
	short		rxbuf[BUFN + BUFSLACK];
	short		clean[BUFN + BUFSLACK];
	short		io[ION];
	unsigned short	out[ION];
};

static struct fix fa, fb;

static struct v32_modem *
modem_of(struct fix *f)
{
	return (struct v32_modem *)(void *)f->obj;
}

/* The heap objects, built once per side and restored before every trial. */
static struct fpm_tone *tone_a[NTONE], *tone_b[NTONE];
static struct fpm_mtd *mtd_a, *mtd_b;

static unsigned char tone_img_a[NTONE][FPM_TONE_STATE_SIZE];
static unsigned char tone_img_b[NTONE][FPM_TONE_STATE_SIZE];
static unsigned char mtd_img_a[sizeof(struct fpm_mtd)];
static unsigned char mtd_img_b[sizeof(struct fpm_mtd)];

static int hist_len;			/* cfg.len + cfg.extra, in shorts */
static short *hist_img_a[NTONE], *hist_img_b[NTONE];
static short rev_acc_img_a[NTONE][4], rev_acc_img_b[NTONE][4];
static short *mtd_acc_img_a, *mtd_acc_img_b;
static int mtd_acc_n;

/* The bank that is NOT V32_S_DATA_COEF; shared, so both sides see one array. */
static const short other_coef[15] = {
	 3000, -3000,  1200,  -900,  2400,
	-1500,  2100,  -600,  1800,  -300,
	  900,  1500, -2100,   600, -1200
};

static unsigned char fpmask[FP_SIZE];
static unsigned char decmask[DEC_SIZE];
static unsigned char tonemask[FPM_TONE_STATE_SIZE];
static unsigned char mtdmask[sizeof(struct fpm_mtd)];
static long fpmask_bytes, decmask_bytes, tonemask_bytes, mtdmask_bytes;

/*
 * `diff_begin` ZEROES the failure count, so a section whose `diff_end` is
 * discarded reports FAIL and exits 0.  Every section accumulates here.
 */
static int rc_total;

/* --------------------------------------------------------------------- */

struct nscall {
	long	slot;		/* which V32NextState entry ran            */
	long	obj_ok;		/* the instance pointer it was handed      */
	long	ctx_alt;	/* obj + 0x64 named the alternate context  */
};

static struct nscall nslog[2][MAXNS];
static int nsn[2];

static int cur_side;			/* 0 = ours, 1 = the blob          */
static struct fix *cur_fix;
static int ns_action;

/* Non-vacuity counters; every one is asserted in main(). */
static long sep_ns_called;
static long sep_ns_not_called;
static long sep_fault;
static long sep_no_fault;
static long sep_ctx_swapped;
static long sep_90_cleared;
static long sep_90_armed;
static long sep_rev_found;
static long sep_rev_none;
static long sep_tone_present;
static long sep_tone_absent;
static long sep_mtd_absent;
static long sep_mtd_other;
static long sep_det_hit;
static long sep_det_miss;
static long sep_halves_differ;
static long sep_seqe_count_arm;
static long sep_seqe_decode_arm;
static long sep_seqe_timeout_arm;
static long sep_flag04;
static long sep_rxstate_data;
static long sep_debug_lines;
static long sep_count_changed;
static long sep_out_written;
static long sep_timer_wrapped;
static long sep_timer_exact;
static long sep_big_count;

/* --------------------------------------------------------------------- */

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

static int
get_int(const void *base, int off)
{
	return *(const int *)(const void *)((const unsigned char *)base + off);
}

static void
put_u16(void *base, int off, unsigned short v)
{
	*(unsigned short *)(void *)((unsigned char *)base + off) = v;
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

/*
 * The two successor pointers, reported as a CODE rather than as an address:
 * the two sides hold different addresses for the same function and always
 * will.  0 nothing, 1 the expected successor, 2 something else.
 */
static long
tx_installed(const unsigned char *hdx, v32_txhdx_fn want)
{
	v32_txhdx_fn f = *(v32_txhdx_fn *)(void *)(hdx + V32HDX_TXSTATE);

	if (f == 0)
		return 0;
	return f == want ? 1 : 2;
}

static long
rx_installed(const unsigned char *hdx, v32_rxhdx_fn err, v32_rxhdx_fn data)
{
	v32_rxhdx_fn f = *(v32_rxhdx_fn *)(void *)(hdx + V32HDX_RXSTATE);

	if (f == 0)
		return 0;
	if (f == err)
		return 1;
	if (f == data)
		return 2;
	return 3;
}

/* --------------------------------------------------------------------- */

static void
ns_body(int slot, struct v32_modem *modem)
{
	void *hdx;

	if (nsn[cur_side] < MAXNS) {
		struct nscall *c = &nslog[cur_side][nsn[cur_side]];

		c->slot = slot;
		c->obj_ok = (modem == (void *)cur_fix->obj);
		c->ctx_alt = (get_ptr(cur_fix->obj, V32_OBJ_HDX)
			      == (void *)cur_fix->alt);
	}
	nsn[cur_side]++;

	hdx = get_ptr(cur_fix->obj, V32_OBJ_HDX);

	switch (ns_action) {
	case NS_CLEAR_90:
		put_int(hdx, T_HDX_INT_90, 0);
		break;
	case NS_SET_90:
		put_int(hdx, T_HDX_INT_90, 1);
		break;
	case NS_SWAP_CTX:
		put_ptr(cur_fix->obj, V32_OBJ_HDX, cur_fix->alt);
		break;
	default:
		break;
	}
}

static void ns0(struct v32_modem *m) { ns_body(0, m); }
static void ns1(struct v32_modem *m) { ns_body(1, m); }
static void ns2(struct v32_modem *m) { ns_body(2, m); }
static void ns3(struct v32_modem *m) { ns_body(3, m); }
static void ns4(struct v32_modem *m) { ns_body(4, m); }
static void ns5(struct v32_modem *m) { ns_body(5, m); }

static void
install_stubs(void)
{
	V32NextState[0] = ns0;
	V32NextState[1] = ns1;
	V32NextState[2] = ns2;
	V32NextState[3] = ns3;
	V32NextState[4] = ns4;
	V32NextState[5] = ns5;

	ref_V32NextState[0] = ns0;
	ref_V32NextState[1] = ns1;
	ref_V32NextState[2] = ns2;
	ref_V32NextState[3] = ns3;
	ref_V32NextState[4] = ns4;
	ref_V32NextState[5] = ns5;
}

/* --------------------------------------------------------------------- */

/*
 * The slicer.  Stateless and deliberately NOT a real V.32 decision: it must
 * be identical on both sides and must not agree with itself by accident.
 * `t_v32demod.c` and `t_fpm_fse_recv.c` make the same choice.
 */
static unsigned short
stub_slicer(struct fpm_fse *state, short *angle, short *mag)
{
	short a = *angle;

	(void)state;
	*angle = (short)(a - 0x0140);
	*mag = (short)(0x2000 + (a >> 4));
	return (unsigned short)((unsigned short)a & 0x0f);
}

/* --------------------------------------------------------------------- */

struct trial {
	const char	*what;
	int		fn;
	unsigned	seed;
	unsigned short	count;		/* *count on entry                 */
	int		amp;		/* input amplitude; 0 is silence   */
	short		symlen;		/* hdx + 0x9e                      */
	short		samplen;	/* hdx + 0xa0                      */
	unsigned int	timer;		/* hdx + 0x7c                      */
	unsigned int	limit;		/* hdx + 0x80                      */
	short		protocol;	/* obj + 0x00                      */
	int		options;	/* obj + 0x10                      */
	int		int78;		/* hdx + 0x78                      */
	short		mode;		/* hdx + 0x76, and the stub slot   */
	int		int90;		/* hdx + 0x90                      */
	short		ac;		/* hdx + 0xac                      */
	unsigned short	a8, aa;		/* hdx + 0xa8, + 0xaa              */
	unsigned short	s48;		/* hdx + 0x48                      */
	short		s94, s98, s9a, s9c;
	int		tone_force;	/* TONE_*                          */
	int		rev_force;	/* seed the reversal search        */
	int		mtd_force;	/* MTD_*                           */
	int		data_coef;	/* the MTD carries V32_S_DATA_COEF */
	int		det_arm;	/* DET_*                           */
	int		rate_change;	/* dec + 0x64                      */
	int		ns;		/* NS_*                            */
	unsigned	dbg;		/* dsplibs_debug_level             */
};

static unsigned
rng(unsigned *s)
{
	*s ^= *s << 13;
	*s ^= *s >> 17;
	*s ^= *s << 5;
	return *s;
}

/*
 * The tone object's thresholds are READ BACK rather than assumed, so forcing
 * a verdict does not depend on which configuration `FPM_TONE_create(NULL, 0)`
 * built.  `bias` separates the three tone objects, so using the wrong one is
 * visible in the verdict and not only in the bytes.
 */
static void
force_tone(struct fpm_tone *t, int verdict, int bias)
{
	short min_level = t->cfg.min_level;
	short ratio = t->cfg.ratio;
	int total;

	switch (verdict) {
	case TONE_PRESENT:
		total = (int)min_level + 1000 + bias;
		t->e_total = (short)total;
		/* Above (ratio * total) >> 15 for any ratio below unity. */
		t->e_tone = (short)total;
		break;
	case TONE_OTHER:
		total = (int)min_level + 1000 + bias;
		t->e_total = (short)total;
		t->e_tone = 0;
		(void)ratio;
		break;
	case TONE_NOSIG:
		t->e_total = (short)(min_level - 1 - bias);
		t->e_tone = 0;
		break;
	default:
		break;
	}
}

static void
force_mtd(struct fpm_mtd *m, int verdict)
{
	switch (verdict) {
	case MTD_ABSENT:
		m->wideband = 1000;
		m->out_of_band = 900;		/* above (ratio * 1000) >> 15 */
		break;
	case MTD_PRESENT:
		m->wideband = 1000;
		m->out_of_band = 100;
		break;
	case MTD_NOSIG:
		m->wideband = (short)(MTD_MIN - 1);
		m->out_of_band = 0;
		break;
	default:
		break;
	}
}

static void
restore_side(struct fpm_tone **tone, unsigned char (*img)[FPM_TONE_STATE_SIZE],
	     short *const *hist_img, short (*acc_img)[4],
	     struct fpm_mtd *mtd, const unsigned char *mimg,
	     const short *macc)
{
	int i;

	for (i = 0; i < NTONE; i++) {
		memcpy(tone[i], img[i], FPM_TONE_STATE_SIZE);
		memcpy(tone[i]->history, hist_img[i],
		       (size_t)hist_len * sizeof(short));
		memcpy(tone[i]->rev_acc, acc_img[i], 4 * sizeof(short));
	}
	memcpy(mtd, mimg, sizeof(struct fpm_mtd));
	memcpy(mtd->acc, macc, (size_t)mtd_acc_n * sizeof(short));
}

static void
build(struct fix *f, const struct trial *t, int side)
{
	struct fpm_fse_cfg fsecfg;
	struct fpm_ecc *ecc;
	struct fpm_tone **tone = side ? tone_b : tone_a;
	struct fpm_mtd *mtd = side ? mtd_b : mtd_a;
	unsigned s = t->seed ? t->seed : 1u;
	int i;

	memset(f, 0, sizeof(*f));

	/*
	 * A 1800 Hz square-ish carrier plus a little noise, so the echo
	 * canceller, the AGC and the timing recovery see something with
	 * structure.  Amplitude zero is silence, which is what the reversal
	 * trials need.
	 */
	for (i = 0; i < ION; i++) {
		int v = 0;

		if (t->amp != 0) {
			v = (i % 40) < 20 ? t->amp : -t->amp;
			v += (int)(rng(&s) % 129u) - 64;
			if (v > 32767)
				v = 32767;
			if (v < -32768)
				v = -32768;
		}
		f->io[i] = (short)v;
	}

	/*
	 * TWO DIFFERENT canaries: with one, a copy running a single element
	 * past `n` lands the value the slot already held and nothing fires.
	 */
	for (i = 0; i < BUFN + BUFSLACK; i++) {
		f->rxbuf[i] = CANARY_RX;
		f->clean[i] = CANARY_CL;
	}
	for (i = 0; i < ION; i++)
		f->out[i] = CANARY_OUT;

	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);
	put_ptr(f->obj, V32_OBJ_FP, f->fp);
	put_s16(f->obj, T_OBJ_PROTOCOL, t->protocol);
	put_int(f->obj, T_OBJ_OPTIONS, t->options);
	put_s16(f->obj, V32_OBJ_RMS_MIN, 0);
	f->obj[V32_OBJ_STATUS] = 0x77;
	f->obj[V32_OBJ_FLAGS] = 0x00;

	/* The context's own AGC, which is what hdx + 0x00 IS. */
	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->hdx + T_HDX_AGC),
			 &ref_AGCv32_CFG, 1);

	put_ptr(f->hdx, V32_HDX_TONE0, tone[0]);
	put_ptr(f->hdx, V32_HDX_TONE1, tone[1]);
	put_ptr(f->hdx, V32_HDX_TONE2, tone[2]);
	put_ptr(f->hdx, V32_HDX_MTD, mtd);

	put_s16(f->hdx, V32HDX_SYMBOL_LEN, t->symlen);
	put_s16(f->hdx, V32HDX_SAMPLE_LEN, t->samplen);
	put_s16(f->hdx, V32HDX_MODE, t->mode);
	put_s16(f->hdx, V32HDX_STATE, V32_STATE_C);
	put_u32(f->hdx, T_HDX_TIMER, t->timer);
	put_u32(f->hdx, T_HDX_LIMIT, t->limit);
	put_int(f->hdx, T_HDX_INT_78, t->int78);
	put_int(f->hdx, T_HDX_INT_90, t->int90);
	put_s16(f->hdx, T_HDX_SHORT_AC, t->ac);
	put_u16(f->hdx, T_HDX_SHORT_48, t->s48);
	put_u16(f->hdx, T_HDX_SHORT_A8, t->a8);
	put_u16(f->hdx, T_HDX_SHORT_AA, t->aa);
	put_s16(f->hdx, T_HDX_RTD, (short)0x5555);
	put_s16(f->hdx, V32_HDX_SHORT_94, t->s94);
	put_s16(f->hdx, V32_HDX_SHORT_98, t->s98);
	put_s16(f->hdx, V32_HDX_SHORT_9A, t->s9a);
	put_s16(f->hdx, V32_HDX_SHORT_9C, t->s9c);

	/* The sequence detector, armed so its verdict is the fixture's. */
	switch (t->det_arm) {
	case DET_HIT_EQ:
		put_u16(f->hdx, V32HDX_DET_WIDTH, 1);
		put_int(f->hdx, V32HDX_DET_MASK, 0);
		put_int(f->hdx, V32HDX_DET_TARGET, 0);
		put_int(f->hdx, V32HDX_DET_OUT_MASK, 0);
		put_int(f->hdx, V32HDX_DET_REG, 0);
		break;
	case DET_HIT_NE:
		put_u16(f->hdx, V32HDX_DET_WIDTH, 1);
		put_int(f->hdx, V32HDX_DET_MASK, 0);
		put_int(f->hdx, V32HDX_DET_TARGET, 0);
		put_int(f->hdx, V32HDX_DET_OUT_MASK, (int)0xffff0000u);
		put_int(f->hdx, V32HDX_DET_REG, 0x00040000);
		break;
	default:
		put_u16(f->hdx, V32HDX_DET_WIDTH, 0);
		put_int(f->hdx, V32HDX_DET_MASK, -1);
		put_int(f->hdx, V32HDX_DET_TARGET, 0x5a5a5a5a);
		put_int(f->hdx, V32HDX_DET_OUT_MASK, -1);
		put_int(f->hdx, V32HDX_DET_REG, 0x13572468);
		break;
	}
	put_int(f->hdx, V32HDX_DET_MATCH, 0x0badf00d);

	/*
	 * The ALTERNATE context: the same shape, differently seeded, and it
	 * is what a next-state stub swaps in.  Everything the state does
	 * after the dispatch must come from here.
	 */
	memcpy(f->alt, f->hdx, HDX_SIZE);
	put_s16(f->alt, V32HDX_SYMBOL_LEN, (short)(t->symlen + 7));
	put_s16(f->alt, V32HDX_MODE, (short)((t->mode + 1) % 6));
	put_u32(f->alt, T_HDX_TIMER, t->timer ^ 0x55u);
	put_u32(f->alt, T_HDX_LIMIT, t->limit);
	put_int(f->alt, T_HDX_INT_90, 0);
	put_s16(f->alt, T_HDX_SHORT_AC, (short)(t->ac + 1));
	put_u16(f->alt, T_HDX_SHORT_A8, 0x3333);
	put_u16(f->alt, T_HDX_SHORT_AA, 0x4444);

	/* The datapump block. */
	ecc = (struct fpm_ecc *)(void *)(f->fp + V32FP_ECC);
	ecc->near_delay = ECC_DELAY;
	ecc->far_delay = ECC_DELAY;

	ref_FPM_MRF_init((struct fpm_mrf *)(void *)(f->fp + V32FP_MRF),
			 &ref_MRFv32_CFG, 1);
	ref_FPM_ECC_init(ecc, &ref_ECCv32_CFG, 1);
	ref_FPM_SRE_init((struct fpm_sre *)(void *)(f->fp + V32FP_SRE),
			 &ref_SREv32_CFG, 1);
	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->fp + V32FP_AGC),
			 &ref_AGCv32_CFG, 1);

	fsecfg = ref_FSEv32_CFG;
	fsecfg.decision = stub_slicer;
	fsecfg.owner = f->dec;
	ref_FPM_FSE_init((struct fpm_fse *)(void *)(f->fp + V32FP_FSE),
			 &fsecfg, 1);
	/* init copies the config, so plant the owner where the code reads it. */
	put_ptr(f->fp, V32FP_FSE + 0x2c, f->dec);

	/*
	 * THE DEMODULATOR HAS TO PRODUCE SYMBOLS, or three of the twelve are
	 * untestable.  `RxHdxRateSequence`, `RxHdxSequence` and
	 * `RxHdxSequenceE` pass `DemodDataV32`'s return to `DetSequence` as
	 * its count, so a demodulator that returns zero makes the sequence
	 * detector's loop empty and EVERY arming of it miss -- which is what
	 * a first pass here did, behind green sections and a `sep_det_hit`
	 * counter that read the trial row instead of the run.  These are
	 * `t_v32demod.c`'s "loud line, carrier seeded" settings.
	 */
	((struct fpm_sre *)(void *)(f->fp + V32FP_SRE))->active = 1;
	((struct fpm_sre *)(void *)(f->fp + V32FP_SRE))->mode = 0;
	((struct fpm_agc *)(void *)(f->fp + V32FP_AGC))->f18 = 1;
	put_int(f->fp, V32FP_SRE_ADAPT_EN, 1);
	put_int(f->fp, V32FP_FSE_PLL_EN, 1);
	put_int(f->fp, V32FP_FSE_TILT_EN, 1);
	put_int(f->fp, V32FP_FSE_LMS_EN, 1);

	put_ptr(f->fp, V32FP_RXBUF, f->rxbuf);
	put_ptr(f->fp, V32FP_CLEAN, f->clean);
	put_s16(f->fp, V32FP_RXLEN, (short)0x5a5a);
	put_s16(f->fp, V32FP_CLEANLEN, (short)0x5a5a);
	put_s16(f->fp, V32FP_RX_RATE_INDEX, 2);

	/* The receive descrambler: no pointers, so plain values will do. */
	put_s16(f->fp, V32FP_DESCRAMBLER + 0x00, 4);		/* group    */
	put_int(f->fp, V32FP_DESCRAMBLER + 0x08, 0x0f);		/* outmask  */
	put_int(f->fp, V32FP_DESCRAMBLER + 0x0c, 0x0007ffff);	/* regmask  */
	put_int(f->fp, V32FP_DESCRAMBLER + 0x10, 0x00012345);	/* reg      */
	put_s16(f->fp, V32FP_DESCRAMBLER + 0x14, 13);		/* tap1     */
	put_s16(f->fp, V32FP_DESCRAMBLER + 0x16, 17);		/* tap2     */

	/* The decoder, and the survivor ring `struct vtb` holds a pointer to. */
	put_int(f->dec, T_DEC_RATE_CHANGE, t->rate_change);
	put_ptr(f->dec, T_DEC_VTB, f->paths);

	/* The detectors' running state, restored and then forced. */
	restore_side(tone, side ? tone_img_b : tone_img_a,
		     side ? hist_img_b : hist_img_a,
		     side ? rev_acc_img_b : rev_acc_img_a,
		     mtd, side ? mtd_img_b : mtd_img_a,
		     side ? mtd_acc_img_b : mtd_acc_img_a);

	for (i = 0; i < NTONE; i++)
		force_tone(tone[i], t->tone_force, 3 * i);

	if (t->rev_force) {
		tone[0]->rev_age = 400;
		tone[0]->rev_corr = (short)-32768;
		tone[0]->rev_energy = 0;
	}

	force_mtd(mtd, t->mtd_force);
	mtd->cfg.coeff = t->data_coef
		? (side ? ref_V32_S_DATA_COEF : V32_S_DATA_COEF)
		: other_coef;
}

/* --------------------------------------------------------------------- */

static void
build_mask(unsigned char *mask, const void *a, const void *b, int n,
	   long *bytes)
{
	const unsigned char *x = (const unsigned char *)a;
	const unsigned char *y = (const unsigned char *)b;
	int i;

	*bytes = 0;
	for (i = 0; i < n; i++) {
		mask[i] = (unsigned char)(x[i] != y[i]);
		if (mask[i])
			(*bytes)++;
	}
}

struct skip {
	int off;
	int len;
};

static int
skipped(const struct skip *sk, int nsk, int off)
{
	int i;

	for (i = 0; i < nsk; i++)
		if (off >= sk[i].off && off < sk[i].off + sk[i].len)
			return 1;
	return 0;
}

/*
 * The first byte at which two blocks differ, masked bytes skipped, or -1.
 * ONE CHECK PER BLOCK: a wrong 32-bit store is one report and not four.
 */
static int
first_diff(const void *a, const void *b, int n, const unsigned char *mask,
	   const struct skip *sk, int nsk)
{
	const unsigned char *x = (const unsigned char *)a;
	const unsigned char *y = (const unsigned char *)b;
	int i;

	for (i = 0; i < n; i++) {
		if (mask != 0 && mask[i])
			continue;
		if (sk != 0 && skipped(sk, nsk, i))
			continue;
		if (x[i] != y[i])
			return i;
	}
	return -1;
}

/*
 * The two pointer fields of the instance, and the four sub-object pointers
 * and two successor slots of a context.  Every one holds a different address
 * on the two sides for ever; the successors are compared as identities below
 * and the rest are the fixture's own storage.
 */
static const struct skip obj_skip[] = {
	{ V32_OBJ_HDX, 4 }, { V32_OBJ_FP, 4 }
};
static const struct skip hdx_skip[] = {
	{ 0x0c, 8 },			/* fpm_agc_cfg::alpha, ::beta       */
	{ V32_HDX_TONE0, 16 },		/* tone0, tone1, tone2, mtd         */
	{ V32HDX_TXSTATE, 8 }		/* the two successors               */
};

#define NSKIP(a)	((int)(sizeof(a) / sizeof((a)[0])))

static void
cmp_block(const char *what, const char *blk, const void *a, const void *b,
	  int n, const unsigned char *mask, const struct skip *sk, int nsk,
	  long tag)
{
	char fmt[224];

	sprintf(fmt, "%s: %s differs at byte (trial %%ld)", what, blk);
	diff_eq_int(fmt, first_diff(a, b, n, mask, sk, nsk), -1, tag);
}

/* --------------------------------------------------------------------- */

static void
call_side(int fn, struct fix *f, unsigned short *count, int blob)
{
	struct v32_modem *modem = modem_of(f);

	if (!blob) {
		switch (fn) {
		case FN_TONE:
			RxHdxTone(modem, f->io, f->out, count); break;
		case FN_NOSIGNAL:
			RxHdxNoSignal(modem, f->io, f->out, count); break;
		case FN_PHSREV:
			RxHdxPhsReversal(modem, f->io, f->out, count); break;
		case FN_RATESEQ:
			RxHdxRateSequence(modem, f->io, f->out, count); break;
		case FN_SEQ:
			RxHdxSequence(modem, f->io, f->out, count); break;
		case FN_SEQE:
			RxHdxSequenceE(modem, f->io, f->out, count); break;
		case FN_DATA:
			RxHdxData(modem, f->io, f->out, count); break;
		case FN_TONEDATA:
			RxHdxToneData(modem, f->io, f->out, count); break;
		case FN_STONE:
			RxHdxSTone(modem, f->io, f->out, count); break;
		case FN_EPOCH:
			RxHdxEpoch(modem, f->io, f->out, count); break;
		case FN_ERROR:
			RxHdxError(modem, f->io, f->out, count); break;
		default:
			RxHdxNull(modem, f->io, f->out, count); break;
		}
		return;
	}

	switch (fn) {
	case FN_TONE:
		ref_RxHdxTone(modem, f->io, f->out, count); break;
	case FN_NOSIGNAL:
		ref_RxHdxNoSignal(modem, f->io, f->out, count); break;
	case FN_PHSREV:
		ref_RxHdxPhsReversal(modem, f->io, f->out, count); break;
	case FN_RATESEQ:
		ref_RxHdxRateSequence(modem, f->io, f->out, count); break;
	case FN_SEQ:
		ref_RxHdxSequence(modem, f->io, f->out, count); break;
	case FN_SEQE:
		ref_RxHdxSequenceE(modem, f->io, f->out, count); break;
	case FN_DATA:
		ref_RxHdxData(modem, f->io, f->out, count); break;
	case FN_TONEDATA:
		ref_RxHdxToneData(modem, f->io, f->out, count); break;
	case FN_STONE:
		ref_RxHdxSTone(modem, f->io, f->out, count); break;
	case FN_EPOCH:
		ref_RxHdxEpoch(modem, f->io, f->out, count); break;
	case FN_ERROR:
		ref_RxHdxError(modem, f->io, f->out, count); break;
	default:
		ref_RxHdxNull(modem, f->io, f->out, count); break;
	}
}

/* --------------------------------------------------------------------- */

static void
run_trial(const struct trial *t, long tag)
{
	unsigned short ca = t->count, cb = t->count;
	unsigned char *ha, *hb;
	long ia, ib;
	int i;

	nsn[0] = nsn[1] = 0;
	memset(nslog, 0, sizeof(nslog));
	ns_action = t->ns;

	/*
	 * THE FIXTURE MUST BE BUILT WITH THE DIAGNOSTICS OFF, and this cost a
	 * pass to learn.  `build` calls `ref_FPM_AGC_init` four times -- the
	 * context's AGC and the datapump's, on each of the two sides -- and
	 * that function prints two lines apiece above `dsplibs_debug_level`
	 * 1: `FPM_AGC_Release`'s "AGC_Release" (a6723, gated `ja` on 1) and
	 * its own banner.  Only the BLOB's constructors are used, so all eight
	 * lines land in side 1's transcript and none in side 0's, and the
	 * comparison reports a nine-against-one shortfall that has nothing to
	 * do with the code under test.  The level goes up, and capture starts,
	 * only for the two calls being compared.
	 */
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	build(&fa, t, 0);
	build(&fb, t, 1);

	build_mask(fpmask, fa.fp, fb.fp, FP_SIZE, &fpmask_bytes);
	build_mask(decmask, fa.dec, fb.dec, DEC_SIZE, &decmask_bytes);
	build_mask(tonemask, tone_a[0], tone_b[0], FPM_TONE_STATE_SIZE,
		   &tonemask_bytes);
	build_mask(mtdmask, mtd_a, mtd_b, (int)sizeof(struct fpm_mtd),
		   &mtdmask_bytes);

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = t->dbg;
	ref_dsplibs_debug_level = t->dbg;

	cur_side = 0;
	cur_fix = &fa;
	call_side(t->fn, &fa, &ca, 0);

	cur_side = 1;
	cur_fix = &fb;
	call_side(t->fn, &fb, &cb, 1);

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	diff_begin(t->what);

	/*
	 * A row that names one state and runs another would compare two
	 * agreeing sides and report nothing, so the row is checked first.
	 */
	diff_eq_int("the trial names the function it runs (%ld)",
		    strncmp(t->what, fn_name[t->fn],
			    strlen(fn_name[t->fn])) == 0, 1, tag);

	/* The reported count, and the caller's output buffer. */
	diff_eq_int("*count on exit (%ld)", ca, cb, ca);
	cmp_block(t->what, "the output buffer", fa.out, fb.out,
		  (int)sizeof(fa.out), 0, 0, 0, tag);

	/* The instance: the status byte and the flags byte live here. */
	cmp_block(t->what, "the instance", fa.obj, fb.obj, OBJ_SIZE, 0,
		  obj_skip, NSKIP(obj_skip), tag);
	diff_eq_int("the status byte (%ld)", fa.obj[V32_OBJ_STATUS],
		    fb.obj[V32_OBJ_STATUS], fa.obj[V32_OBJ_STATUS]);
	diff_eq_int("the flags byte (%ld)", fa.obj[V32_OBJ_FLAGS],
		    fb.obj[V32_OBJ_FLAGS], fa.obj[V32_OBJ_FLAGS]);

	/* Which context the instance names, as a flag and not an address. */
	diff_eq_int("the context in use (%ld)",
		    get_ptr(fa.obj, V32_OBJ_HDX) == (void *)fa.alt,
		    get_ptr(fb.obj, V32_OBJ_HDX) == (void *)fb.alt, tag);

	/* Both contexts, whichever is in use. */
	cmp_block(t->what, "the context", fa.hdx, fb.hdx, HDX_SIZE, 0,
		  hdx_skip, NSKIP(hdx_skip), tag);
	cmp_block(t->what, "the alternate context", fa.alt, fb.alt, HDX_SIZE,
		  0, hdx_skip, NSKIP(hdx_skip), tag);

	/* The two successor slots of both contexts, as identities. */
	diff_eq_int("hdx txstate (%ld)", tx_installed(fa.hdx, TxHdxNoCarrier),
		    tx_installed(fb.hdx, ref_TxHdxNoCarrier), tag);
	diff_eq_int("hdx rxstate (%ld)",
		    rx_installed(fa.hdx, RxHdxError, RxHdxData),
		    rx_installed(fb.hdx, ref_RxHdxError, ref_RxHdxData), tag);
	diff_eq_int("alt txstate (%ld)", tx_installed(fa.alt, TxHdxNoCarrier),
		    tx_installed(fb.alt, ref_TxHdxNoCarrier), tag);
	diff_eq_int("alt rxstate (%ld)",
		    rx_installed(fa.alt, RxHdxError, RxHdxData),
		    rx_installed(fb.alt, ref_RxHdxError, ref_RxHdxData), tag);

	/* The datapump block, the decoder, and the buffers they own. */
	cmp_block(t->what, "the datapump block", fa.fp, fb.fp, FP_SIZE,
		  fpmask, 0, 0, tag);
	cmp_block(t->what, "the decoder", fa.dec, fb.dec, DEC_SIZE, decmask,
		  0, 0, tag);
	cmp_block(t->what, "the survivor ring", fa.paths, fb.paths,
		  PATHS_SIZE, 0, 0, 0, tag);
	cmp_block(t->what, "the working buffer", fa.rxbuf, fb.rxbuf,
		  (int)sizeof(fa.rxbuf), 0, 0, 0, tag);
	cmp_block(t->what, "the cleaned copy", fa.clean, fb.clean,
		  (int)sizeof(fa.clean), 0, 0, 0, tag);
	cmp_block(t->what, "the input buffer", fa.io, fb.io,
		  (int)sizeof(fa.io), 0, 0, 0, tag);

	/* The three tone objects and the multi-tone detector. */
	for (i = 0; i < NTONE; i++) {
		char blk[64];

		sprintf(blk, "tone object %d", i);
		cmp_block(t->what, blk, tone_a[i], tone_b[i],
			  FPM_TONE_STATE_SIZE, tonemask, 0, 0, tag);
		sprintf(blk, "tone object %d history", i);
		cmp_block(t->what, blk, tone_a[i]->history,
			  tone_b[i]->history, hist_len * (int)sizeof(short),
			  0, 0, 0, tag);
		sprintf(blk, "tone object %d reversal filter", i);
		cmp_block(t->what, blk, tone_a[i]->rev_acc,
			  tone_b[i]->rev_acc, 4 * (int)sizeof(short), 0, 0, 0,
			  tag);
	}
	cmp_block(t->what, "the multi-tone detector", mtd_a, mtd_b,
		  (int)sizeof(struct fpm_mtd), mtdmask, 0, 0, tag);
	cmp_block(t->what, "the multi-tone accumulators", mtd_a->acc,
		  mtd_b->acc, mtd_acc_n * (int)sizeof(short), 0, 0, 0, tag);

	/* The dispatch trace. */
	diff_eq_int("next-state calls (%ld)", nsn[0], nsn[1], nsn[0]);
	for (i = 0; i < nsn[0] && i < MAXNS; i++) {
		diff_eq_int("dispatch %ld: which slot", nslog[0][i].slot,
			    nslog[1][i].slot, i);
		diff_eq_int("dispatch %ld: the instance", nslog[0][i].obj_ok,
			    1, i);
		diff_eq_int("dispatch %ld: the instance (blob)",
			    nslog[1][i].obj_ok, 1, i);
		diff_eq_int("dispatch %ld: context in use",
			    nslog[0][i].ctx_alt, nslog[1][i].ctx_alt, i);
	}

	/* The diagnostics, which are two of `RxHdxPhsReversal`'s statements. */
	diff_eq_int("diagnostic lines (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1),
		    (long)dsplib_debug_capture_lines(0));
	diff_eq_int("the diagnostic transcript (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)), 0, tag);
	/*
	 * A transcript mismatch reported as "1 against 0" says nothing about
	 * WHICH line went wrong, and the two texts are the only evidence
	 * there is.  Printed only when they actually differ.
	 */
	if (strcmp(dsplib_debug_capture_text(0),
		   dsplib_debug_capture_text(1)) != 0)
		printf("    ours:\n%s    the blob:\n%s",
		       dsplib_debug_capture_text(0),
		       dsplib_debug_capture_text(1));

	/* ----------------------------------------------- non-vacuity ---- */

	ha = (unsigned char *)get_ptr(fa.obj, V32_OBJ_HDX);
	hb = (unsigned char *)get_ptr(fb.obj, V32_OBJ_HDX);
	(void)hb;

	if (nsn[0] > 0)
		sep_ns_called++;
	else
		sep_ns_not_called++;
	if ((fa.obj[V32_OBJ_FLAGS] & V32_FLAG_FAULT) != 0)
		sep_fault++;
	else
		sep_no_fault++;
	if ((fa.obj[V32_OBJ_FLAGS] & T_FLAG_04) != 0)
		sep_flag04++;
	if (get_ptr(fa.obj, V32_OBJ_HDX) == (void *)fa.alt)
		sep_ctx_swapped++;
	if (t->ns == NS_CLEAR_90 && nsn[0] > 0)
		sep_90_cleared++;
	if (t->int90 == 0 && *(const int *)(const void *)(ha + T_HDX_INT_90)
			     != 0)
		sep_90_armed++;
	if (t->rev_force && t->int90 != 0 && t->count != 0
	    && get_s16(ha, T_HDX_RTD) != (short)0x5555)
		sep_rev_found++;
	if (t->int90 != 0 && !t->rev_force)
		sep_rev_none++;
	/*
	 * A FORCED VERDICT IS ONLY FORCED WHEN THE COUNT IS ZERO: with a real
	 * block the detector's loop runs and overwrites the energies the
	 * fixture seeded, so a trial with samples in it proves the plumbing
	 * and not the branch.  These counters only credit the forced ones.
	 */
	if (t->count == 0 && t->tone_force == TONE_PRESENT)
		sep_tone_present++;
	if (t->count == 0
	    && (t->tone_force == TONE_OTHER || t->tone_force == TONE_NOSIG))
		sep_tone_absent++;
	if (t->fn == FN_STONE && nsn[0] > 0)
		sep_mtd_absent++;
	if (t->fn == FN_STONE && nsn[0] == 0)
		sep_mtd_other++;
	/*
	 * OBSERVED, NOT ASSUMED.  `DetSequence` writes V32HDX_DET_MATCH only
	 * on a hit, so the seed left there by `build` says whether it really
	 * found anything -- reading `t->det_arm` instead only says what the
	 * trial ASKED for, and a demodulator returning zero symbols makes
	 * every one of them miss while the counter keeps reading one.
	 */
	if (t->fn == FN_RATESEQ || t->fn == FN_SEQ || t->fn == FN_SEQE) {
		if (get_int(fa.hdx, V32HDX_DET_MATCH) != 0x0badf00d)
			sep_det_hit++;
		else
			sep_det_miss++;
		if (t->det_arm == DET_HIT_NE
		    && get_int(fa.hdx, V32HDX_DET_MATCH) != 0x0badf00d
		    && t->fn == FN_RATESEQ && nsn[0] == 0)
			sep_halves_differ++;
	}
	if (t->fn == FN_SEQE && t->s48 != 0)
		sep_seqe_count_arm++;
	if (t->fn == FN_SEQE
	    && rx_installed(fa.hdx, RxHdxError, RxHdxData) == 2)
		sep_seqe_decode_arm++;
	if (t->fn == FN_SEQE && t->s48 == 0
	    && (fa.obj[V32_OBJ_FLAGS] & V32_FLAG_FAULT) != 0)
		sep_seqe_timeout_arm++;
	if (rx_installed(fa.hdx, RxHdxError, RxHdxData) == 2
	    || rx_installed(fa.alt, RxHdxError, RxHdxData) == 2)
		sep_rxstate_data++;
	if (dsplib_debug_capture_lines(0) > 0)
		sep_debug_lines++;
	if (ca != t->count)
		sep_count_changed++;
	if (t->count > 32)
		sep_big_count++;
	if (t->limit != 0 && t->timer + (unsigned int)t->symlen == t->limit)
		sep_timer_exact++;
	if (t->timer > 0x7fffffffu)
		sep_timer_wrapped++;
	for (i = 0; i < ION; i++)
		if (fa.out[i] != CANARY_OUT) {
			sep_out_written++;
			break;
		}

	ia = (long)fpmask_bytes;
	ib = (long)tonemask_bytes;
	diff_eq_int("the datapump mask is not empty (%ld)", ia > 0, 1, ia);
	diff_eq_int("the datapump mask is small (%ld)", ia < FP_SIZE / 8, 1,
		    ia);
	diff_eq_int("the tone mask is not empty (%ld)", ib > 0, 1, ib);
	diff_eq_int("the tone mask is small (%ld)",
		    ib < FPM_TONE_STATE_SIZE / 4, 1, ib);

	rc_total |= diff_end();
}

/* --------------------------------------------------------------------- */

/*
 * The defaults every trial starts from.  Only the fields a trial is ABOUT are
 * written at the call site, so what a trial exercises is visible in its row.
 */
static struct trial
base(const char *what, int fn, unsigned seed)
{
	struct trial t;

	memset(&t, 0, sizeof(t));
	t.what = what;
	t.fn = fn;
	t.seed = seed;
	t.count = 0;
	t.amp = 0;
	t.symlen = 12;
	t.samplen = 40;
	t.timer = 100;
	t.limit = 100000;
	t.protocol = 3;
	t.options = 0;
	t.int78 = 0;
	t.mode = V32_MODE_ORIGINATE;
	t.int90 = 0;
	t.ac = 0;
	t.a8 = 0x1111;
	t.aa = 0x2222;
	t.s48 = 0;
	t.s94 = 500;
	t.s98 = 40;
	t.s9a = 30;
	t.s9c = 20;
	t.tone_force = TONE_OTHER;
	t.rev_force = 0;
	t.mtd_force = MTD_PRESENT;
	t.data_coef = 0;
	t.det_arm = DET_MISS;
	t.rate_change = 0;
	t.ns = NS_NOTHING;
	t.dbg = 0;
	return t;
}

/* --------------------------------------------------------------------- */

static long trial_no;

static void
run(struct trial t)
{
	run_trial(&t, trial_no);
	trial_no++;
}

static void
run_tone(void)
{
	struct trial t;

	/* The gate: protocol, the option bit, and the counter at +0x78. */
	t = base("RxHdxTone: the gate opens on protocol", FN_TONE, 0x11110001u);
	t.protocol = 3;
	t.tone_force = TONE_PRESENT;
	run(t);

	t = base("RxHdxTone: the gate is shut", FN_TONE, 0x11110002u);
	t.protocol = 1;
	t.options = 0;
	t.int78 = 180;
	t.tone_force = TONE_PRESENT;
	run(t);

	t = base("RxHdxTone: the gate opens on the option bit", FN_TONE,
		 0x11110003u);
	t.protocol = 1;
	t.options = T_OPT_0400 | 0x11;
	t.int78 = 180;
	t.tone_force = TONE_PRESENT;
	run(t);

	t = base("RxHdxTone: the gate opens on +0x78 past 180", FN_TONE,
		 0x11110004u);
	t.protocol = 1;
	t.options = 0;
	t.int78 = 181;
	t.tone_force = TONE_PRESENT;
	run(t);

	t = base("RxHdxTone: +0x78 is signed and far negative", FN_TONE,
		 0x11110005u);
	t.protocol = 1;
	t.int78 = -1000000;
	t.tone_force = TONE_PRESENT;
	run(t);

	t = base("RxHdxTone: the option bit's neighbours are ignored", FN_TONE,
		 0x11110006u);
	t.protocol = 1;
	t.options = (int)0xfffffbffu;		/* every bit but 0x400       */
	t.int78 = 0;
	t.tone_force = TONE_PRESENT;
	run(t);

	t = base("RxHdxTone: the tone is absent", FN_TONE, 0x11110007u);
	t.tone_force = TONE_OTHER;
	run(t);

	t = base("RxHdxTone: no signal at all", FN_TONE, 0x11110008u);
	t.tone_force = TONE_NOSIG;
	run(t);

	t = base("RxHdxTone: the counter lands exactly on the bound", FN_TONE,
		 0x11110009u);
	t.timer = 988;
	t.limit = 1000;
	run(t);

	t = base("RxHdxTone: one short of the bound", FN_TONE, 0x1111000au);
	t.timer = 987;
	t.limit = 1000;
	run(t);

	t = base("RxHdxTone: the counter is above 0x7fffffff", FN_TONE,
		 0x1111000bu);
	t.timer = 0x80000000u;
	t.limit = 0x90000000u;
	run(t);

	t = base("RxHdxTone: an unsigned bound a signed read would pass",
		 FN_TONE, 0x1111000cu);
	t.timer = 0x90000000u;
	t.limit = 0x80000000u;
	run(t);

	/*
	 * NO TRIAL USES A NEGATIVE OR HUGE SYMBOL LENGTH, and that is a limit
	 * of the fixture rather than of the code: `RxClampV32` writes
	 * `symbol_len` words into the caller's buffer and counts down to -1,
	 * so a negative one writes about 65,500 of them.  The object does
	 * exactly that -- it is `v32fpctl.c`'s own reading and its own test's
	 * -- and a buffer big enough to survive it would swamp every block
	 * comparison here.  Symbol lengths stay inside `ION`.
	 */
	t = base("RxHdxTone: the largest symbol length the buffer allows",
		 FN_TONE, 0x1111000du);
	t.symlen = 400;
	t.timer = 10;
	t.limit = 1000;
	run(t);

	t = base("RxHdxTone: a real block through the AGC and the detector",
		 FN_TONE, 0x1111000eu);
	t.count = 40;
	t.amp = 6000;
	t.tone_force = TONE_FREE;
	t.mode = V32_MODE_ANSWER;
	t.ns = NS_SWAP_CTX;
	run(t);

	t = base("RxHdxTone: a real block with the gate shut", FN_TONE,
		 0x1111000fu);
	t.count = 40;
	t.amp = 6000;
	t.protocol = 1;
	t.int78 = 0;
	t.tone_force = TONE_FREE;
	run(t);

	t = base("RxHdxTone: the last dispatch slot", FN_TONE, 0x11110010u);
	t.tone_force = TONE_PRESENT;
	t.mode = V32_MODE_RING_RESP;
	run(t);

	t = base("RxHdxTone: the dispatch swaps the whole context", FN_TONE,
		 0x11110011u);
	t.tone_force = TONE_PRESENT;
	t.mode = V32_MODE_ANSWER;
	t.ns = NS_SWAP_CTX;
	run(t);

	t = base("RxHdxTone: the dispatch swaps the context, and the new one"
		 " times out", FN_TONE, 0x11110012u);
	t.tone_force = TONE_PRESENT;
	t.timer = 900;
	t.limit = 901;
	t.ns = NS_SWAP_CTX;
	run(t);
}

static void
run_nosignal(void)
{
	struct trial t;

	t = base("RxHdxNoSignal: the tone is present, so nothing dispatches",
		 FN_NOSIGNAL, 0x22220001u);
	t.tone_force = TONE_PRESENT;
	t.timer = 1000;
	run(t);

	t = base("RxHdxNoSignal: absent, and the counter is past 60",
		 FN_NOSIGNAL, 0x22220002u);
	t.tone_force = TONE_OTHER;
	t.timer = 1000;
	run(t);

	t = base("RxHdxNoSignal: absent, and the counter is exactly 60",
		 FN_NOSIGNAL, 0x22220003u);
	t.tone_force = TONE_OTHER;
	t.symlen = 10;
	t.timer = 50;
	run(t);

	t = base("RxHdxNoSignal: absent, and the counter is 61", FN_NOSIGNAL,
		 0x22220004u);
	t.tone_force = TONE_OTHER;
	t.symlen = 11;
	t.timer = 50;
	run(t);

	t = base("RxHdxNoSignal: the counter is above 0x7fffffff",
		 FN_NOSIGNAL, 0x22220005u);
	t.tone_force = TONE_NOSIG;
	t.timer = 0x80000000u;
	t.limit = 0xf0000000u;
	run(t);

	t = base("RxHdxNoSignal: it times out", FN_NOSIGNAL, 0x22220006u);
	t.tone_force = TONE_NOSIG;
	t.timer = 990;
	t.limit = 1000;
	t.symlen = 12;
	run(t);

	t = base("RxHdxNoSignal: a real block", FN_NOSIGNAL, 0x22220007u);
	t.count = 40;
	t.amp = 4000;
	t.tone_force = TONE_FREE;
	t.timer = 5000;
	t.mode = V32_MODE_LOCLOOP_2;
	run(t);

	t = base("RxHdxNoSignal: the dispatch swaps the whole context",
		 FN_NOSIGNAL, 0x22220009u);
	t.tone_force = TONE_OTHER;
	t.timer = 5000;
	t.mode = V32_MODE_LOCLOOP_2;
	t.ns = NS_SWAP_CTX;
	run(t);

	/*
	 * +0xaa is SIXTEEN bits and +0x7c is thirty-two, and both take the
	 * same addend.  A seed near the top of the sixteen makes the two part
	 * company, which is the only shape that separates them.
	 */
	t = base("RxHdxNoSignal: +0xaa wraps through sixteen bits",
		 FN_NOSIGNAL, 0x22220008u);
	t.symlen = 32;
	t.aa = 0xfff0;
	t.timer = 0;
	t.limit = 0x7fffffffu;
	run(t);
}

static void
run_phsrev(void)
{
	struct trial t;

	/*
	 * THE RUN COUNTER AT +0xac COUNTS THE TONE'S PRESENCE, NOT ITS
	 * ABSENCE.  `FPM_TONE_detect` returns zero when the tone IS there, and
	 * 83bc5's `jne` sends the non-zero verdict to the store of zero, so it
	 * is the PRESENT blocks that accumulate.  An earlier draft of these
	 * rows had it the other way round; every one of them then reset the
	 * counter, +0x90 was never armed by the counter at all, and the whole
	 * arming path went untested behind four green sections.
	 */
	t = base("RxHdxPhsReversal: the tone's run counter climbs", FN_PHSREV,
		 0x33330001u);
	t.tone_force = TONE_PRESENT;
	t.ac = 0;
	run(t);

	t = base("RxHdxPhsReversal: the fourth block of tone arms the search",
		 FN_PHSREV, 0x33330002u);
	t.tone_force = TONE_PRESENT;
	t.ac = 3;
	run(t);

	t = base("RxHdxPhsReversal: the third does not", FN_PHSREV,
		 0x33330003u);
	t.tone_force = TONE_PRESENT;
	t.ac = 2;
	run(t);

	t = base("RxHdxPhsReversal: the tone's absence resets the counter",
		 FN_PHSREV, 0x33330004u);
	t.tone_force = TONE_OTHER;
	t.ac = 3;
	run(t);

	t = base("RxHdxPhsReversal: no signal resets it too", FN_PHSREV,
		 0x33330014u);
	t.tone_force = TONE_NOSIG;
	t.ac = 3;
	run(t);

	t = base("RxHdxPhsReversal: armed, and no reversal is found",
		 FN_PHSREV, 0x33330005u);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 0;
	run(t);

	t = base("RxHdxPhsReversal: a reversal, originating", FN_PHSREV,
		 0x33330006u);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.mode = V32_MODE_ORIGINATE;
	run(t);

	t = base("RxHdxPhsReversal: a reversal, originating, RTD clamped",
		 FN_PHSREV, 0x33330007u);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.mode = V32_MODE_ORIGINATE;
	t.s94 = 4000;			/* 2 * 4000 swamps the scaled period */
	run(t);

	t = base("RxHdxPhsReversal: a reversal, answering", FN_PHSREV,
		 0x33330008u);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.mode = V32_MODE_ANSWER;
	run(t);

	t = base("RxHdxPhsReversal: a reversal, answering, RTD clamped",
		 FN_PHSREV, 0x33330009u);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.mode = V32_MODE_ANSWER;
	t.s94 = 4000;
	run(t);

	t = base("RxHdxPhsReversal: a reversal with the diagnostics on",
		 FN_PHSREV, 0x3333000au);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.mode = V32_MODE_ANSWER;
	t.dbg = 3;
	run(t);

	t = base("RxHdxPhsReversal: originating, diagnostics on -- one line",
		 FN_PHSREV, 0x3333000bu);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.mode = V32_MODE_ORIGINATE;
	t.dbg = 3;
	run(t);

	t = base("RxHdxPhsReversal: the turnaround budget is exhausted",
		 FN_PHSREV, 0x3333000cu);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.s94 = 10;
	t.s98 = 40;
	t.s9a = 30;
	t.s9c = 20;
	t.dbg = 3;
	run(t);

	/*
	 * THE SECOND TEST IS NOT AN `else`.  The dispatch clears +0x90, so
	 * the tone-detect arm has to run in the same call.
	 */
	t = base("RxHdxPhsReversal: the dispatch clears +0x90 and the detector"
		 " runs in the same call", FN_PHSREV, 0x3333000du);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.tone_force = TONE_OTHER;
	t.ac = 3;
	t.ns = NS_CLEAR_90;
	run(t);

	t = base("RxHdxPhsReversal: the dispatch swaps the whole context",
		 FN_PHSREV, 0x3333000eu);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.tone_force = TONE_OTHER;
	t.ns = NS_SWAP_CTX;
	run(t);

	t = base("RxHdxPhsReversal: the dispatch leaves +0x90 set", FN_PHSREV,
		 0x3333000fu);
	t.int90 = 1;
	t.count = 8;
	t.rev_force = 1;
	t.ns = NS_SET_90;
	run(t);

	t = base("RxHdxPhsReversal: it times out", FN_PHSREV, 0x33330010u);
	t.timer = 990;
	t.limit = 1000;
	run(t);

	t = base("RxHdxPhsReversal: +0xa8 wraps through sixteen bits",
		 FN_PHSREV, 0x33330013u);
	t.symlen = 32;
	t.a8 = 0xfff0;
	t.timer = 0;
	t.limit = 0x7fffffffu;
	run(t);

	t = base("RxHdxPhsReversal: a real block through all three tones",
		 FN_PHSREV, 0x33330011u);
	t.count = 48;
	t.amp = 9000;
	t.tone_force = TONE_FREE;
	run(t);

	t = base("RxHdxPhsReversal: a real block, armed", FN_PHSREV,
		 0x33330012u);
	t.count = 48;
	t.amp = 9000;
	t.int90 = 1;
	t.tone_force = TONE_FREE;
	run(t);
}

static void
run_sequences(void)
{
	struct trial t;

	t = base("RxHdxRateSequence: the detector misses", FN_RATESEQ,
		 0x44440001u);
	t.count = 120;
	t.amp = 9000;
	t.det_arm = DET_MISS;
	run(t);

	t = base("RxHdxRateSequence: it hits and the halves agree",
		 FN_RATESEQ, 0x44440002u);
	t.count = 120;
	t.amp = 9000;
	t.det_arm = DET_HIT_EQ;
	run(t);

	t = base("RxHdxRateSequence: it hits and the halves differ",
		 FN_RATESEQ, 0x44440003u);
	t.count = 120;
	t.amp = 9000;
	t.det_arm = DET_HIT_NE;
	run(t);

	t = base("RxHdxRateSequence: a hit, a dispatch, and a timeout",
		 FN_RATESEQ, 0x44440004u);
	t.count = 120;
	t.amp = 9000;
	t.det_arm = DET_HIT_EQ;
	t.timer = 990;
	t.limit = 1000;
	t.mode = V32_MODE_RING_INIT;
	t.ns = NS_SWAP_CTX;
	run(t);

	t = base("RxHdxRateSequence: a zero-length block", FN_RATESEQ,
		 0x44440005u);
	t.count = 0;
	t.det_arm = DET_HIT_EQ;
	run(t);

	t = base("RxHdxSequence: the detector misses", FN_SEQ, 0x55550001u);
	t.count = 120;
	t.amp = 9000;
	t.det_arm = DET_MISS;
	run(t);

	t = base("RxHdxSequence: it hits", FN_SEQ, 0x55550002u);
	t.count = 120;
	t.amp = 9000;
	t.det_arm = DET_HIT_EQ;
	run(t);

	t = base("RxHdxSequence: it hits with the halves differing, which it"
		 " must not care about", FN_SEQ, 0x55550003u);
	t.count = 120;
	t.amp = 9000;
	t.det_arm = DET_HIT_NE;
	run(t);

	t = base("RxHdxSequence: a hit, a dispatch, and a timeout", FN_SEQ,
		 0x55550004u);
	t.count = 120;
	t.amp = 9000;
	t.det_arm = DET_HIT_EQ;
	t.timer = 990;
	t.limit = 1000;
	t.ns = NS_SWAP_CTX;
	run(t);
}

static void
run_seqe(void)
{
	struct trial t;

	t = base("RxHdxSequenceE: the counter arm, well inside 0x17", FN_SEQE,
		 0x66660001u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 1;
	t.symlen = 4;
	run(t);

	t = base("RxHdxSequenceE: the counter arm, landing on 0x17", FN_SEQE,
		 0x66660002u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 0x13;
	t.symlen = 4;
	run(t);

	t = base("RxHdxSequenceE: the counter arm, one past 0x17", FN_SEQE,
		 0x66660003u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 0x14;
	t.symlen = 4;
	run(t);

	t = base("RxHdxSequenceE: the counter arm does not time out however"
		 " far past the bound it is", FN_SEQE, 0x66660004u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 1;
	t.timer = 0xfffffff0u;
	t.limit = 1000;
	run(t);

	t = base("RxHdxSequenceE: the counter arm, and the compare is signed",
		 FN_SEQE, 0x66660005u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 0xfff0;
	t.symlen = 4;
	run(t);

	t = base("RxHdxSequenceE: the decode arm", FN_SEQE, 0x66660006u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 0;
	t.det_arm = DET_HIT_EQ;
	run(t);

	t = base("RxHdxSequenceE: the decode arm with the other match word",
		 FN_SEQE, 0x66660007u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 0;
	t.det_arm = DET_HIT_NE;
	run(t);

	t = base("RxHdxSequenceE: the timeout arm", FN_SEQE, 0x66660008u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 0;
	t.det_arm = DET_MISS;
	t.timer = 990;
	t.limit = 1000;
	run(t);

	t = base("RxHdxSequenceE: the detector misses and there is time left",
		 FN_SEQE, 0x66660009u);
	t.count = 120;
	t.amp = 9000;
	t.s48 = 0;
	t.det_arm = DET_MISS;
	run(t);
}

static void
run_rest(void)
{
	struct trial t;

	t = base("RxHdxData: a real block", FN_DATA, 0x77770001u);
	t.count = 40;
	t.amp = 7000;
	run(t);

	t = base("RxHdxData: a zero-length block", FN_DATA, 0x77770002u);
	run(t);

	t = base("RxHdxData: it never posts a fault", FN_DATA, 0x77770003u);
	t.count = 40;
	t.amp = 7000;
	t.timer = 0xfffffff0u;
	t.limit = 1000;
	run(t);

	t = base("RxHdxToneData: the tone is present", FN_TONEDATA,
		 0x88880001u);
	t.tone_force = TONE_PRESENT;
	run(t);

	t = base("RxHdxToneData: the tone is absent", FN_TONEDATA,
		 0x88880002u);
	t.tone_force = TONE_OTHER;
	run(t);

	t = base("RxHdxToneData: it times out and demodulates anyway",
		 FN_TONEDATA, 0x88880003u);
	t.tone_force = TONE_OTHER;
	t.timer = 990;
	t.limit = 1000;
	run(t);

	t = base("RxHdxToneData: a dispatch that swaps the context",
		 FN_TONEDATA, 0x88880004u);
	t.tone_force = TONE_PRESENT;
	t.ns = NS_SWAP_CTX;
	run(t);

	t = base("RxHdxToneData: a real block", FN_TONEDATA, 0x88880005u);
	t.count = 40;
	t.amp = 7000;
	t.tone_force = TONE_FREE;
	run(t);

	t = base("RxHdxToneData: a real block that also times out",
		 FN_TONEDATA, 0x88880006u);
	t.count = 40;
	t.amp = 7000;
	t.tone_force = TONE_FREE;
	t.timer = 990;
	t.limit = 1000;
	run(t);

	t = base("RxHdxSTone: the raw-input arm, tone absent", FN_STONE,
		 0x99990001u);
	t.data_coef = 0;
	t.mtd_force = MTD_ABSENT;
	run(t);

	t = base("RxHdxSTone: the raw-input arm, tone present", FN_STONE,
		 0x99990002u);
	t.data_coef = 0;
	t.mtd_force = MTD_PRESENT;
	run(t);

	t = base("RxHdxSTone: the raw-input arm, no signal", FN_STONE,
		 0x99990003u);
	t.data_coef = 0;
	t.mtd_force = MTD_NOSIG;
	run(t);

	t = base("RxHdxSTone: the data-coefficient arm demodulates first",
		 FN_STONE, 0x99990004u);
	t.data_coef = 1;
	t.mtd_force = MTD_ABSENT;
	run(t);

	t = base("RxHdxSTone: the data-coefficient arm, tone present",
		 FN_STONE, 0x99990005u);
	t.data_coef = 1;
	t.mtd_force = MTD_PRESENT;
	run(t);

	t = base("RxHdxSTone: the data-coefficient arm and a timeout",
		 FN_STONE, 0x99990006u);
	t.data_coef = 1;
	t.mtd_force = MTD_ABSENT;
	t.timer = 990;
	t.limit = 1000;
	t.ns = NS_SWAP_CTX;
	run(t);

	/*
	 * The two arms with a REAL block, which is what separates the buffers
	 * they hand the detector: the raw arm sees `in` and the other sees the
	 * datapump's own working buffer, and the two hold different samples.
	 */
	t = base("RxHdxSTone: the raw-input arm over a real block", FN_STONE,
		 0x99990007u);
	t.count = 40;
	t.amp = 7000;
	t.data_coef = 0;
	t.mtd_force = MTD_FREE;
	run(t);

	t = base("RxHdxSTone: the data-coefficient arm over a real block",
		 FN_STONE, 0x99990008u);
	t.count = 40;
	t.amp = 7000;
	t.data_coef = 1;
	t.mtd_force = MTD_FREE;
	run(t);

	t = base("RxHdxEpoch: no rate change", FN_EPOCH, 0xaaaa0001u);
	t.count = 40;
	t.amp = 7000;
	t.rate_change = 0;
	run(t);

	t = base("RxHdxEpoch: a rate change", FN_EPOCH, 0xaaaa0002u);
	t.count = 40;
	t.amp = 7000;
	t.rate_change = 1;
	run(t);

	t = base("RxHdxEpoch: a rate change reported as a negative",
		 FN_EPOCH, 0xaaaa0003u);
	t.count = 40;
	t.amp = 7000;
	t.rate_change = -7;
	t.mode = V32_MODE_LOCLOOP_3;
	run(t);

	t = base("RxHdxEpoch: a rate change, a swap, and a timeout", FN_EPOCH,
		 0xaaaa0004u);
	t.count = 40;
	t.amp = 7000;
	t.rate_change = 1;
	t.timer = 990;
	t.limit = 1000;
	t.ns = NS_SWAP_CTX;
	run(t);

	t = base("RxHdxError: a real block", FN_ERROR, 0xbbbb0001u);
	t.count = 40;
	t.amp = 7000;
	run(t);

	t = base("RxHdxError: it reads no counter at all", FN_ERROR,
		 0xbbbb0002u);
	t.count = 40;
	t.amp = 7000;
	t.timer = 0xfffffff0u;
	t.limit = 1;
	run(t);

	t = base("RxHdxError: a zero-length block", FN_ERROR, 0xbbbb0003u);
	run(t);

	t = base("RxHdxNull: inside the bound", FN_NULL, 0xcccc0001u);
	t.timer = 100;
	t.limit = 1000;
	run(t);

	t = base("RxHdxNull: exactly on the bound", FN_NULL, 0xcccc0002u);
	t.timer = 988;
	t.limit = 1000;
	run(t);

	t = base("RxHdxNull: one short of it", FN_NULL, 0xcccc0003u);
	t.timer = 987;
	t.limit = 1000;
	run(t);

	t = base("RxHdxNull: above 0x7fffffff", FN_NULL, 0xcccc0004u);
	t.timer = 0x80000000u;
	t.limit = 0x90000000u;
	run(t);

	t = base("RxHdxNull: an unsigned bound a signed read would pass",
		 FN_NULL, 0xcccc0005u);
	t.timer = 0x90000000u;
	t.limit = 0x80000000u;
	run(t);

	t = base("RxHdxNull: a long clamp", FN_NULL, 0xcccc0006u);
	t.symlen = 200;
	t.count = 40;
	t.amp = 7000;
	run(t);
}

/* --------------------------------------------------------------------- */

static int
setup(void)
{
	struct fpm_mtd_cfg mcfg;
	int i;

	for (i = 0; i < NTONE; i++) {
		tone_a[i] = ref_FPM_TONE_create(0, 0);
		tone_b[i] = ref_FPM_TONE_create(0, 0);
		if (tone_a[i] == 0 || tone_b[i] == 0)
			return 1;
	}

	hist_len = (int)tone_a[0]->cfg.len + (int)tone_a[0]->cfg.extra;
	if (hist_len <= 0)
		return 1;

	mcfg.coeff = other_coef;
	mcfg.tones = MTD_TONES;
	mcfg.ratio = MTD_RATIO;
	mcfg.min_level = MTD_MIN;
	mcfg.f0a = 0;

	mtd_a = ref_FPM_MTD_create(0, &mcfg);
	mtd_b = ref_FPM_MTD_create(0, &mcfg);
	if (mtd_a == 0 || mtd_b == 0)
		return 1;
	mtd_acc_n = 2 * MTD_TONES;

	for (i = 0; i < NTONE; i++) {
		memcpy(tone_img_a[i], tone_a[i], FPM_TONE_STATE_SIZE);
		memcpy(tone_img_b[i], tone_b[i], FPM_TONE_STATE_SIZE);
		hist_img_a[i] = (short *)malloc((size_t)hist_len
						* sizeof(short));
		hist_img_b[i] = (short *)malloc((size_t)hist_len
						* sizeof(short));
		if (hist_img_a[i] == 0 || hist_img_b[i] == 0)
			return 1;
		memcpy(hist_img_a[i], tone_a[i]->history,
		       (size_t)hist_len * sizeof(short));
		memcpy(hist_img_b[i], tone_b[i]->history,
		       (size_t)hist_len * sizeof(short));
		memcpy(rev_acc_img_a[i], tone_a[i]->rev_acc,
		       4 * sizeof(short));
		memcpy(rev_acc_img_b[i], tone_b[i]->rev_acc,
		       4 * sizeof(short));
	}

	memcpy(mtd_img_a, mtd_a, sizeof(struct fpm_mtd));
	memcpy(mtd_img_b, mtd_b, sizeof(struct fpm_mtd));
	mtd_acc_img_a = (short *)malloc((size_t)mtd_acc_n * sizeof(short));
	mtd_acc_img_b = (short *)malloc((size_t)mtd_acc_n * sizeof(short));
	if (mtd_acc_img_a == 0 || mtd_acc_img_b == 0)
		return 1;
	memcpy(mtd_acc_img_a, mtd_a->acc, (size_t)mtd_acc_n * sizeof(short));
	memcpy(mtd_acc_img_b, mtd_b->acc, (size_t)mtd_acc_n * sizeof(short));

	install_stubs();
	return 0;
}

int
main(void)
{
	int rc = 0;

	if (setup() != 0) {
		printf("t_v32rxhdx: fixture construction failed\n");
		return 1;
	}

	run_tone();
	run_nosignal();
	run_phsrev();
	run_sequences();
	run_seqe();
	run_rest();

	diff_begin("v32rxhdx separating trials");
	diff_eq_int("a next-state dispatch happened (%ld)", sep_ns_called > 0,
		    1, sep_ns_called);
	diff_eq_int("a trial dispatched nothing (%ld)", sep_ns_not_called > 0,
		    1, sep_ns_not_called);
	diff_eq_int("a fault was posted (%ld)", sep_fault > 0, 1, sep_fault);
	diff_eq_int("a trial posted none (%ld)", sep_no_fault > 0, 1,
		    sep_no_fault);
	diff_eq_int("the context was swapped mid-call (%ld)",
		    sep_ctx_swapped > 0, 1, sep_ctx_swapped);
	diff_eq_int("a dispatch cleared +0x90 (%ld)", sep_90_cleared > 0, 1,
		    sep_90_cleared);
	diff_eq_int("the tone's run counter armed the search (%ld)",
		    sep_90_armed > 0, 1, sep_90_armed);
	diff_eq_int("a phase reversal was found (%ld)", sep_rev_found > 0, 1,
		    sep_rev_found);
	diff_eq_int("the search ran and found none (%ld)", sep_rev_none > 0, 1,
		    sep_rev_none);
	diff_eq_int("the tone detector said present (%ld)",
		    sep_tone_present > 0, 1, sep_tone_present);
	diff_eq_int("the tone detector said otherwise (%ld)",
		    sep_tone_absent > 0, 1, sep_tone_absent);
	diff_eq_int("the multi-tone detector said absent (%ld)",
		    sep_mtd_absent > 0, 1, sep_mtd_absent);
	diff_eq_int("the multi-tone detector said otherwise (%ld)",
		    sep_mtd_other > 0, 1, sep_mtd_other);
	diff_eq_int("the sequence detector hit (%ld)", sep_det_hit > 0, 1,
		    sep_det_hit);
	diff_eq_int("the sequence detector missed (%ld)", sep_det_miss > 0, 1,
		    sep_det_miss);
	diff_eq_int("the match word's halves differed (%ld)",
		    sep_halves_differ > 0, 1, sep_halves_differ);
	diff_eq_int("RxHdxSequenceE took its counter arm (%ld)",
		    sep_seqe_count_arm > 0, 1, sep_seqe_count_arm);
	diff_eq_int("RxHdxSequenceE took its decode arm (%ld)",
		    sep_seqe_decode_arm > 0, 1, sep_seqe_decode_arm);
	diff_eq_int("RxHdxSequenceE took its timeout arm (%ld)",
		    sep_seqe_timeout_arm > 0, 1, sep_seqe_timeout_arm);
	diff_eq_int("the 0x04 flag was raised (%ld)", sep_flag04 > 0, 1,
		    sep_flag04);
	diff_eq_int("RxHdxData was installed as the successor (%ld)",
		    sep_rxstate_data > 0, 1, sep_rxstate_data);
	diff_eq_int("a diagnostic line was printed (%ld)", sep_debug_lines > 0,
		    1, sep_debug_lines);
	diff_eq_int("*count came back changed (%ld)", sep_count_changed > 0, 1,
		    sep_count_changed);
	diff_eq_int("the output buffer was written (%ld)", sep_out_written > 0,
		    1, sep_out_written);
	diff_eq_int("a counter above 0x7fffffff was compared (%ld)",
		    sep_timer_wrapped > 0, 1, sep_timer_wrapped);
	diff_eq_int("a counter landed exactly on the bound (%ld)",
		    sep_timer_exact > 0, 1, sep_timer_exact);
	diff_eq_int("a block of real length was demodulated (%ld)",
		    sep_big_count > 0, 1, sep_big_count);
	rc |= diff_end();

	return rc | rc_total;
}
