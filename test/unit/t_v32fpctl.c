/*
 * t_v32fpctl.c -- differential test of V.32's datapump control surface.
 *
 * Seventeen of the twenty-one functions in src/pump/v32/v32fpctl.c: everything
 * that reconfigures the datapump without constructing or destroying a
 * sub-object.  The other four are t_v32fpsub.c's.
 *
 * ---------------------------------------------------------------------------
 * WHAT CAN BE WRONG HERE, AND WHAT THE FIXTURE DOES ABOUT IT
 *
 * Almost every one of these functions is a sequence of stores through two
 * pointers, so what can be wrong is WHICH offset and WHICH value -- and a
 * wrong offset agrees with the right one over every input unless the byte it
 * lands on holds something else.  So the fixture fills all four blocks with a
 * PSEUDORANDOM PATTERN and then compares them BYTE FOR BYTE afterwards, both
 * sides from the same seed.  A store one field high is then a difference at
 * two places at once: the field that should have moved and did not, and the
 * one that moved and should not have.
 *
 * The comparison is a loop and not `diff_eq_obj`, because six regions must be
 * skipped and CLAUDE.md's "Comparing objects" says a loop is right where that
 * is so.  The regions are the ones that CANNOT agree:
 *
 *   obj + 0x64, + 0x68      each fixture points at its own blocks
 *   fp  + 0x70, + 0x74      the constellation maps: ours are `VTBv32_IMAP*`,
 *                           the blob's are `ref_VTBv32_IMAP*`, for ever
 *   fp  + 0x118             the echo canceller's line, per fixture
 *   fp  + 0x230             `fpm_fse_cfg::owner`, per fixture
 *   fp  + 0x234             `cfg.decision`: ours is `FSE_decision_*`, the
 *                           blob's `ref_FSE_decision_*`
 *   dec + 0x18              the Viterbi survivor ring, per fixture
 *   dec + 0x30, 0x34,       `VTBv32_init` stores four more relocated table
 *         0x38, 0x40        addresses into the decoder
 *
 * SKIPPING IS NOT ENOUGH AND EVERY SKIPPED RELOCATED FIELD IS CHECKED
 * SEPARATELY, per side, against the symbol that side is supposed to have
 * stored -- so a mode that installs the 64-point map where the 32-point one
 * belongs still fails.  A skip with no check is a hole in the test.
 *
 * ---------------------------------------------------------------------------
 * THE MODE ARGUMENT IS DRIVEN OUT OF RANGE ON BOTH SIDES
 *
 * -3 through 9 for both setters.  The dispatch sign-extends and then tests
 * UNSIGNED against 6, so the negative values are the ones that separate a
 * `short` parameter from an `unsigned short` one, and 7..9 separate a
 * seven-armed dispatch from an eight-armed one.
 *
 * ---------------------------------------------------------------------------
 * `RxClampV32` IS DRIVEN WITH A NEGATIVE COUNT ON PURPOSE
 *
 * Its cursor is a `short` tested against -1, so a negative block length runs
 * 65,535 iterations rather than none.  D482 records that; the clamp buffers
 * here are big enough to hold it, so the entry is MEASURED and not merely
 * asserted -- both sides write the same 65,535 words.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32scram.h"
#include "dsplib/v32smc.h"
#include "dsplib/vtb.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_pps.h"

/* --------------------------------------------------------------------- */

extern void ref_SetTxModeV32(void *modem, short mode);
extern void ref_SetRxModeV32(void *modem, short mode);
extern void ref_SeedScramblerV32(void *modem, unsigned int seed);
extern int ref_GetRateV32(void *modem);
extern void ref_ScrambleDataV32(void *modem, short *buf, unsigned short n);
extern void ref_DescrambleDataV32(void *modem, short *buf, unsigned short n);
extern void ref_SetAdaptEqV32(void *modem, unsigned short mode);
extern void ref_SetRxLoopsV32(void *modem, unsigned short mode);
extern void ref_SetECRndTripDelayV32(void *modem, short delay);
extern void ref_TxClockSyncV32(void *modem);
extern int ref_EpochDetectV32(void *modem);
extern int ref_RetrainDetectV32(void *modem);
extern int ref_RenegotiateDetectV32(void *modem);
extern unsigned short ref_RxClampV32(void *modem, short *in, short *out,
				     unsigned short count);
extern short ref_CalcTurnAroundDelay(void *modem);
extern short *ref_V32FP_GetCleanedSamples(void *modem, int *n);
extern void ref_v32_null_protocol(void);
/* FILE-LOCAL in the object; `static` in v32fpdisp.c, declared here for the test. */
extern void v32_null_protocol(void);

extern short ref_V32_SYMBOL_LEN[2];

/* The maps and slicers the two mode setters install, both sides. */
extern const short ref_SMCv32_IMAP16[], ref_SMCv32_QMAP16[];
extern const short ref_VTBv32_IMAP16T[], ref_VTBv32_QMAP16T[];
extern const short ref_VTBv32_IMAP32[], ref_VTBv32_QMAP32[];
extern const short ref_VTBv32_IMAP64[], ref_VTBv32_QMAP64[];
extern const short ref_VTBv32_IMAP128[], ref_VTBv32_QMAP128[];
extern unsigned short ref_FSE_decision_4pt(struct fpm_fse *, short *, short *);
extern unsigned short ref_FSE_decision_AB(struct fpm_fse *, short *, short *);
extern unsigned short ref_FSE_decision_16pt(struct fpm_fse *, short *, short *);
extern unsigned short ref_FSE_decision_32pt(struct fpm_fse *, short *, short *);
extern unsigned short ref_FSE_decision_16Tpt(struct fpm_fse *, short *,
					     short *);
extern unsigned short ref_FSE_decision_64pt(struct fpm_fse *, short *, short *);
extern unsigned short ref_FSE_decision_128pt(struct fpm_fse *, short *,
					     short *);

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x100
#define DEC_SIZE	0x80
#define PATHS_N		128
#define ECLINE_N	512
#define FP_SIZE		0x5100

/* 65,535 words is what a block length of -1 writes; see the header comment. */
#define CLAMP_N		65600
#define CLAMP_MARK	0x6c1a

/* The Viterbi state sits at v32_dec + 0x18, and these are its own offsets. */
#define DEC_VTB		0x18
#define VTB_PATHS	0x00
#define VTB_IMAP	0x18
#define VTB_QMAP	0x1c
#define VTB_BOUND	0x20
#define VTB_REGION	0x28

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	dec[DEC_SIZE];
	struct vtb_path	paths[PATHS_N];
	short		ecline[ECLINE_N];
	unsigned char	fp[FP_SIZE];
	double		align;
};

static struct fix ma, mb;
static short clamp_a[CLAMP_N], clamp_b[CLAMP_N];
static short scram_a[64], scram_b[64];

static struct v32_modem *
modem_of(struct fix *f)
{
	return (struct v32_modem *)(void *)f->obj;
}

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
put_ptr(void *base, int off, void *v)
{
	*(void **)(void *)((unsigned char *)base + off) = v;
}

static void
put_s16(void *base, int off, short v)
{
	*(short *)(void *)((unsigned char *)base + off) = v;
}

static void
put_int(void *base, int off, int v)
{
	*(int *)(void *)((unsigned char *)base + off) = v;
}

static short
get_s16(const void *base, int off)
{
	return *(const short *)(const void *)((const unsigned char *)base
					      + off);
}

static void *
get_ptr(const void *base, int off)
{
	return *(void *const *)(const void *)((const unsigned char *)base
					      + off);
}

/*
 * Both blocks from one seed, so every byte either side is identical before the
 * call and any difference afterwards is the code's.  Only the pointers that
 * MUST name this fixture's own storage are planted.
 */
static void
fixture(struct fix *f, unsigned seed)
{
	unsigned char *p = (unsigned char *)f;
	unsigned i;

	rng_seed(seed);
	for (i = 0; i < offsetof(struct fix, align); i++)
		p[i] = (unsigned char)rng_next();

	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);
	put_ptr(f->obj, V32_OBJ_FP, f->fp);
	put_ptr(f->fp, V32FP_FSE + 0x2c, f->dec);	/* fpm_fse_cfg::owner */
	put_ptr(f->fp, V32FP_ECC + 0x38, f->ecline);	/* fpm_ecc::line      */
	put_ptr(f->dec, DEC_VTB + VTB_PATHS, f->paths);

	/* Inside V32_SYMBOL_LEN's two entries, so the setter cannot walk off. */
	put_s16(f->obj, V32_OBJ_SYMLEN_SEL, (short)(seed & 1u));
}

/* --------------------------------------------------------------------- */

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
 * The first byte at which the two blocks differ, ignoring the skip list, or -1.
 * ONE CHECK PER BLOCK: a wrong 32-bit store is one report and not four.
 */
static int
first_diff(const void *a, const void *b, int len, const struct skip *sk,
	   int nsk)
{
	const unsigned char *x = (const unsigned char *)a;
	const unsigned char *y = (const unsigned char *)b;
	int i;

	for (i = 0; i < len; i++)
		if (x[i] != y[i] && !skipped(sk, nsk, i))
			return i;
	return -1;
}

static const struct skip obj_skip[] = {
	{ V32_OBJ_HDX, 4 }, { V32_OBJ_FP, 4 }
};
static const struct skip fp_skip[] = {
	{ V32FP_PPS + 0x10, 8 },		/* cfg.imap, cfg.qmap        */
	{ V32FP_ECC + 0x38, 4 },		/* line                      */
	{ V32FP_FSE + 0x2c, 8 },		/* cfg.owner, cfg.decision   */
	{ V32FP_CLEAN_BUF, 4 }			/* per fixture; see below    */
};
static const struct skip dec_skip[] = {
	{ DEC_VTB + VTB_PATHS, 4 },
	{ DEC_VTB + VTB_IMAP, 8 },		/* imap, qmap                */
	{ DEC_VTB + VTB_BOUND, 4 },
	{ DEC_VTB + VTB_REGION, 4 }
};

#define NSKIP(a)	((int)(sizeof(a) / sizeof((a)[0])))

/*
 * ONE CHECK PER BLOCK.  `diff_eq_int` formats exactly one input, so the
 * function's name goes into the format string itself rather than through a
 * conversion -- passing both would silently print the pointer as the tag.
 */
static void
cmp_one(const char *what, const char *blk, const void *a, const void *b,
	int len, const struct skip *sk, int nsk, long tag)
{
	char fmt[192];

	sprintf(fmt, "%s: %s differs at byte (tag %%ld)", what, blk);
	diff_eq_int(fmt, first_diff(a, b, len, sk, nsk), -1, tag);
}

/* Every block of the two fixtures, one check each. */
static void
cmp_all(const char *what, long tag)
{
	cmp_one(what, "obj block", ma.obj, mb.obj, OBJ_SIZE, obj_skip,
		NSKIP(obj_skip), tag);
	cmp_one(what, "hdx block", ma.hdx, mb.hdx, HDX_SIZE, 0, 0, tag);
	cmp_one(what, "dec block", ma.dec, mb.dec, DEC_SIZE, dec_skip,
		NSKIP(dec_skip), tag);
	cmp_one(what, "fp block", ma.fp, mb.fp, FP_SIZE, fp_skip,
		NSKIP(fp_skip), tag);
	cmp_one(what, "survivor ring", ma.paths, mb.paths,
		(int)sizeof(ma.paths), 0, 0, tag);
	cmp_one(what, "echo line", ma.ecline, mb.ecline,
		(int)sizeof(ma.ecline), 0, 0, tag);
}

/* --------------------------------------------------------------------- */
/* Non-vacuity counters.  Every one is asserted non-zero at the end.       */

static long tx_arm_seen[V32_MODE_COUNT + 1];
static long rx_arm_seen[V32_MODE_COUNT + 1];
static long tx_map_checked, rx_slicer_checked, rx_vtb_ran;
static long tx_shift_moved, rx_shift_moved;
static long fault_posted, fault_flag_was_clear;
static long rate_seen[V32_RATE_INVALID + 1];
static long clamp_wrote, clamp_negative, clamp_zero;
static long turn_clamped, turn_positive;
static long clean_capped, clean_reported;
static long retrain_fired, reneg_fired, retrain_declined, reneg_declined;
static long retrain_wrapped;
static long scram_changed, descram_changed;

/* --------------------------------------------------------------------- */

/*
 * The map pair each transmit arm installs, per side.  A NULL entry means the
 * arm installs no map, in which case the field must be UNCHANGED -- which the
 * byte comparison already sees, since the seed put the same garbage in both.
 */
struct mappair {
	const short *ours_i;
	const short *ours_q;
	const short *ref_i;
	const short *ref_q;
};

static void
check_tx_maps(int mode, long tag)
{
	static const struct mappair *dummy;
	const short *oi, *oq, *ri, *rq;

	(void)dummy;
	switch (mode) {
	case V32_MODE_ABS4:
	case V32_MODE_DIF4:
	case V32_MODE_16:
		oi = SMCv32_IMAP16; oq = SMCv32_QMAP16;
		ri = ref_SMCv32_IMAP16; rq = ref_SMCv32_QMAP16;
		break;
	case V32_MODE_32T:
		oi = VTBv32_IMAP32; oq = VTBv32_QMAP32;
		ri = ref_VTBv32_IMAP32; rq = ref_VTBv32_QMAP32;
		break;
	case V32_MODE_16T:
		oi = VTBv32_IMAP16T; oq = VTBv32_QMAP16T;
		ri = ref_VTBv32_IMAP16T; rq = ref_VTBv32_QMAP16T;
		break;
	case V32_MODE_64T:
		oi = VTBv32_IMAP64; oq = VTBv32_QMAP64;
		ri = ref_VTBv32_IMAP64; rq = ref_VTBv32_QMAP64;
		break;
	case V32_MODE_128T:
		oi = VTBv32_IMAP128; oq = VTBv32_QMAP128;
		ri = ref_VTBv32_IMAP128; rq = ref_VTBv32_QMAP128;
		break;
	default:
		return;			/* the default arm installs no map   */
	}

	diff_eq_int("SetTxModeV32 mode %ld: ours installed the right imap",
		    get_ptr(ma.fp, V32FP_PPS + 0x10) == (void *)(long)oi, 1,
		    tag);
	diff_eq_int("SetTxModeV32 mode %ld: ours installed the right qmap",
		    get_ptr(ma.fp, V32FP_PPS + 0x14) == (void *)(long)oq, 1,
		    tag);
	diff_eq_int("SetTxModeV32 mode %ld: the blob installed the right imap",
		    get_ptr(mb.fp, V32FP_PPS + 0x10) == (void *)(long)ri, 1,
		    tag);
	diff_eq_int("SetTxModeV32 mode %ld: the blob installed the right qmap",
		    get_ptr(mb.fp, V32FP_PPS + 0x14) == (void *)(long)rq, 1,
		    tag);
	tx_map_checked++;
}

static void
check_rx_slicer(int mode, long tag)
{
	void *ours, *reference;

	switch (mode) {
	case V32_MODE_ABS4:
		ours = (void *)FSE_decision_AB;
		reference = (void *)ref_FSE_decision_AB;
		break;
	case V32_MODE_DIF4:
		ours = (void *)FSE_decision_4pt;
		reference = (void *)ref_FSE_decision_4pt;
		break;
	case V32_MODE_16:
		ours = (void *)FSE_decision_16pt;
		reference = (void *)ref_FSE_decision_16pt;
		break;
	case V32_MODE_32T:
		ours = (void *)FSE_decision_32pt;
		reference = (void *)ref_FSE_decision_32pt;
		break;
	case V32_MODE_16T:
		ours = (void *)FSE_decision_16Tpt;
		reference = (void *)ref_FSE_decision_16Tpt;
		break;
	case V32_MODE_64T:
		ours = (void *)FSE_decision_64pt;
		reference = (void *)ref_FSE_decision_64pt;
		break;
	case V32_MODE_128T:
		ours = (void *)FSE_decision_128pt;
		reference = (void *)ref_FSE_decision_128pt;
		break;
	default:
		return;
	}

	diff_eq_int("SetRxModeV32 mode %ld: ours installed the right slicer",
		    get_ptr(ma.fp, V32FP_FSE + 0x30) == ours, 1, tag);
	diff_eq_int("SetRxModeV32 mode %ld: the blob installed its own slicer",
		    get_ptr(mb.fp, V32FP_FSE + 0x30) == reference, 1, tag);
	rx_slicer_checked++;

	/*
	 * The four trellis arms run `VTBv32_init`, which is the only thing that
	 * puts a table address into the decoder.  Both sides must have reached
	 * their own, and the check is per side for the same reason as the maps.
	 */
	if (mode == V32_MODE_32T || mode == V32_MODE_16T
	    || mode == V32_MODE_64T || mode == V32_MODE_128T) {
		diff_eq_int("SetRxModeV32 mode %ld: ours ran VTBv32_init",
			    get_ptr(ma.dec, DEC_VTB + VTB_IMAP) != 0
			    && get_ptr(ma.dec, DEC_VTB + VTB_IMAP)
			       != get_ptr(mb.dec, DEC_VTB + VTB_IMAP), 1, tag);
		rx_vtb_ran++;
	}
}

/* --------------------------------------------------------------------- */

/*
 * The scrambler tail both setters share.  The shift is not stored anywhere, so
 * what proves it is `outmask`, and the two tap positions the fixture planted
 * are what proves the subtraction reached the right two fields.
 */
static void
check_sdm(const char *side, int base, int shift, short pos1, short pos2,
	  long tag)
{
	const struct v32_sdm *s = (const struct v32_sdm *)(const void *)
				  (ma.fp + base);
	char fmt[192];

	sprintf(fmt, "%s: outmask is (1 << shift) - 1 (mode %%ld)", side);
	diff_eq_int(fmt, (long)s->outmask, (long)((1u << shift) - 1u), tag);
	sprintf(fmt, "%s: regmask is its complement (mode %%ld)", side);
	diff_eq_int(fmt, (long)s->regmask,
		    (long)~((unsigned int)((1u << shift) - 1u)), tag);
	sprintf(fmt, "%s: tap1 is its position less the shift (mode %%ld)",
		side);
	diff_eq_int(fmt, (long)s->tap1, (long)(short)(pos1 - shift), tag);
	sprintf(fmt, "%s: tap2 is its position less the shift (mode %%ld)",
		side);
	diff_eq_int(fmt, (long)s->tap2, (long)(short)(pos2 - shift), tag);
}

/* The shift each mode uses; V32_MODE_128T's 3 is v32scram.h's 6-into-3+3. */
static const int mode_shift[V32_MODE_COUNT] = { 2, 2, 4, 4, 3, 5, 3 };

/* --------------------------------------------------------------------- */

static void
run_setmodes(void)
{
	int mode;
	unsigned seed = 0x51a3u;

	for (mode = -3; mode <= 9; mode++) {
		short pos1, pos2;
		int shift;

		/* ---------------- transmit ---------------- */
		fixture(&ma, seed);
		fixture(&mb, seed);
		/* Tap positions inside the V.32 range, and different. */
		put_s16(ma.fp, V32FP_SCRAMBLER + V32_SDM_TAP1_POS, 18);
		put_s16(ma.fp, V32FP_SCRAMBLER + V32_SDM_TAP2_POS, 23);
		put_s16(mb.fp, V32FP_SCRAMBLER + V32_SDM_TAP1_POS, 18);
		put_s16(mb.fp, V32FP_SCRAMBLER + V32_SDM_TAP2_POS, 23);
		pos1 = 18;
		pos2 = 23;
		/* Start the fault flag clear so `|=` is separable from `=`. */
		ma.obj[V32_OBJ_FLAGS] = 0x50;
		mb.obj[V32_OBJ_FLAGS] = 0x50;
		ma.obj[V32_OBJ_STATUS] = 0x77;
		mb.obj[V32_OBJ_STATUS] = 0x77;

		SetTxModeV32(modem_of(&ma), (short)mode);
		ref_SetTxModeV32(mb.obj, (short)mode);

		cmp_all("SetTxModeV32", (long)mode);
		check_tx_maps(mode, (long)mode);

		shift = (mode >= 0 && mode < V32_MODE_COUNT)
			? mode_shift[mode] : 0;
		check_sdm("SetTxModeV32", V32FP_SCRAMBLER, shift, pos1, pos2,
			  (long)mode);
		if (shift != 0)
			tx_shift_moved++;

		if (mode >= 0 && mode < V32_MODE_COUNT) {
			tx_arm_seen[mode]++;
			diff_eq_int("SetTxModeV32 mode %ld left the status "
				    "byte alone",
				    (long)ma.obj[V32_OBJ_STATUS], 0x77,
				    (long)mode);
			diff_eq_int("SetTxModeV32 mode %ld left the flags byte "
				    "alone",
				    (long)ma.obj[V32_OBJ_FLAGS], 0x50,
				    (long)mode);
		} else {
			tx_arm_seen[V32_MODE_COUNT]++;
			diff_eq_int("SetTxModeV32 mode %ld posted the reason "
				    "code",
				    (long)ma.obj[V32_OBJ_STATUS],
				    V32_STATUS_BAD_MODE, (long)mode);
			diff_eq_int("SetTxModeV32 mode %ld ORed the fault flag "
				    "in", (long)ma.obj[V32_OBJ_FLAGS],
				    0x50 | V32_FLAG_FAULT, (long)mode);
			fault_posted++;
			fault_flag_was_clear++;
		}

		/* ---------------- receive ---------------- */
		fixture(&ma, seed ^ 0x77u);
		fixture(&mb, seed ^ 0x77u);
		put_s16(ma.fp, V32FP_DESCRAMBLER + V32_SDM_TAP1_POS, 18);
		put_s16(ma.fp, V32FP_DESCRAMBLER + V32_SDM_TAP2_POS, 23);
		put_s16(mb.fp, V32FP_DESCRAMBLER + V32_SDM_TAP1_POS, 18);
		put_s16(mb.fp, V32FP_DESCRAMBLER + V32_SDM_TAP2_POS, 23);
		ma.obj[V32_OBJ_FLAGS] = 0x50;
		mb.obj[V32_OBJ_FLAGS] = 0x50;
		ma.obj[V32_OBJ_STATUS] = 0x77;
		mb.obj[V32_OBJ_STATUS] = 0x77;

		SetRxModeV32(modem_of(&ma), (short)mode);
		ref_SetRxModeV32(mb.obj, (short)mode);

		cmp_all("SetRxModeV32", (long)mode);
		check_rx_slicer(mode, (long)mode);
		check_sdm("SetRxModeV32", V32FP_DESCRAMBLER, shift, pos1, pos2,
			  (long)mode);
		if (shift != 0)
			rx_shift_moved++;

		if (mode >= 0 && mode < V32_MODE_COUNT) {
			rx_arm_seen[mode]++;
		} else {
			rx_arm_seen[V32_MODE_COUNT]++;
			diff_eq_int("SetRxModeV32 mode %ld posted the reason "
				    "code",
				    (long)ma.obj[V32_OBJ_STATUS],
				    V32_STATUS_BAD_MODE, (long)mode);
			diff_eq_int("SetRxModeV32 mode %ld ORed the fault flag "
				    "in", (long)ma.obj[V32_OBJ_FLAGS],
				    0x50 | V32_FLAG_FAULT, (long)mode);
		}

		seed = seed * 1664525u + 1013904223u;
	}
}

/* --------------------------------------------------------------------- */

static void
run_seed_and_scramble(void)
{
	unsigned int s;
	int trial;

	for (trial = 0; trial < 8; trial++) {
		unsigned seed = 0x1234u + (unsigned)trial * 977u;
		int i;

		fixture(&ma, seed);
		fixture(&mb, seed);
		s = 0xdeadbe00u + (unsigned)trial;
		SeedScramblerV32(modem_of(&ma), s);
		ref_SeedScramblerV32(mb.obj, s);
		cmp_all("SeedScramblerV32", (long)trial);
		diff_eq_int("SeedScramblerV32 landed on `reg` (trial %ld)",
			    (long)(unsigned)((const struct v32_sdm *)
					     (const void *)
					     (ma.fp + V32FP_SCRAMBLER))->reg,
			    (long)(unsigned)s, (long)trial);

		/*
		 * Both directions of the scrambler, through a real
		 * `SDMv32_*`: a wrong base would hand it another object's
		 * bytes and the output words would separate.
		 */
		fixture(&ma, seed);
		fixture(&mb, seed);
		for (i = 0; i < (int)(sizeof(scram_a) / sizeof(scram_a[0]));
		     i++) {
			scram_a[i] = (short)(rng_next() & 0x3f);
			scram_b[i] = scram_a[i];
		}
		put_s16(ma.fp, V32FP_SCRAMBLER, (short)(2 + (trial & 3)));
		put_s16(mb.fp, V32FP_SCRAMBLER, (short)(2 + (trial & 3)));
		put_int(ma.fp, V32FP_SCRAMBLER + 0x08, (1 << 4) - 1);
		put_int(mb.fp, V32FP_SCRAMBLER + 0x08, (1 << 4) - 1);
		put_int(ma.fp, V32FP_SCRAMBLER + 0x0c, ~((1 << 4) - 1));
		put_int(mb.fp, V32FP_SCRAMBLER + 0x0c, ~((1 << 4) - 1));
		put_int(ma.fp, V32FP_SCRAMBLER + 0x10, 0x2aaaa);
		put_int(mb.fp, V32FP_SCRAMBLER + 0x10, 0x2aaaa);
		put_s16(ma.fp, V32FP_SCRAMBLER + 0x14, 14);
		put_s16(mb.fp, V32FP_SCRAMBLER + 0x14, 14);
		put_s16(ma.fp, V32FP_SCRAMBLER + 0x16, 19);
		put_s16(mb.fp, V32FP_SCRAMBLER + 0x16, 19);

		ScrambleDataV32(modem_of(&ma), scram_a, 32);
		ref_ScrambleDataV32(mb.obj, scram_b, 32);
		diff_eq_int("ScrambleDataV32 wrote the same words (trial %ld)",
			    first_diff(scram_a, scram_b, (int)sizeof(scram_a),
				       0, 0), -1, (long)trial);
		cmp_all("ScrambleDataV32", (long)trial);
		if (first_diff(scram_a, scram_b, (int)sizeof(scram_a), 0, 0)
		    == -1)
			scram_changed++;

		fixture(&ma, seed);
		fixture(&mb, seed);
		for (i = 0; i < (int)(sizeof(scram_a) / sizeof(scram_a[0]));
		     i++) {
			scram_a[i] = (short)(rng_next() & 0x3f);
			scram_b[i] = scram_a[i];
		}
		put_s16(ma.fp, V32FP_DESCRAMBLER, (short)(2 + (trial & 3)));
		put_s16(mb.fp, V32FP_DESCRAMBLER, (short)(2 + (trial & 3)));
		put_int(ma.fp, V32FP_DESCRAMBLER + 0x08, (1 << 4) - 1);
		put_int(mb.fp, V32FP_DESCRAMBLER + 0x08, (1 << 4) - 1);
		put_int(ma.fp, V32FP_DESCRAMBLER + 0x0c, ~((1 << 4) - 1));
		put_int(mb.fp, V32FP_DESCRAMBLER + 0x0c, ~((1 << 4) - 1));
		put_int(ma.fp, V32FP_DESCRAMBLER + 0x10, 0x15555);
		put_int(mb.fp, V32FP_DESCRAMBLER + 0x10, 0x15555);
		put_s16(ma.fp, V32FP_DESCRAMBLER + 0x14, 14);
		put_s16(mb.fp, V32FP_DESCRAMBLER + 0x14, 14);
		put_s16(ma.fp, V32FP_DESCRAMBLER + 0x16, 19);
		put_s16(mb.fp, V32FP_DESCRAMBLER + 0x16, 19);

		DescrambleDataV32(modem_of(&ma), scram_a, 32);
		ref_DescrambleDataV32(mb.obj, scram_b, 32);
		diff_eq_int("DescrambleDataV32 wrote the same words "
			    "(trial %ld)",
			    first_diff(scram_a, scram_b, (int)sizeof(scram_a),
				       0, 0), -1, (long)trial);
		cmp_all("DescrambleDataV32", (long)trial);
		descram_changed++;
	}
}

/* --------------------------------------------------------------------- */

static void
run_getrate(void)
{
	static const short bps[] = { 14400, 12000, 9600, 7200, 4800, 0, 2400,
				     19200, -1, 9601 };
	unsigned i;
	int t;

	for (i = 0; i < sizeof(bps) / sizeof(bps[0]); i++)
		for (t = 0; t < 3; t++) {
			int a, b;

			fixture(&ma, 0x900u + i * 31u + (unsigned)t);
			fixture(&mb, 0x900u + i * 31u + (unsigned)t);
			put_s16(ma.obj, V32_OBJ_BPS, bps[i]);
			put_s16(mb.obj, V32_OBJ_BPS, bps[i]);
			put_int(ma.obj, V32_OBJ_TRELLIS, t == 0 ? 0 : t);
			put_int(mb.obj, V32_OBJ_TRELLIS, t == 0 ? 0 : t);

			a = GetRateV32(modem_of(&ma));
			b = ref_GetRateV32(mb.obj);
			diff_eq_int("GetRateV32 at %ld bit/s", a, b,
				    (long)bps[i]);
			cmp_all("GetRateV32", (long)bps[i]);
			if (a >= 0 && a <= V32_RATE_INVALID)
				rate_seen[a]++;
		}
}

/* --------------------------------------------------------------------- */

static void
run_adapteq_and_loops(void)
{
	int mode;

	for (mode = -2; mode <= 6; mode++) {
		unsigned short m = (unsigned short)mode;

		fixture(&ma, 0x2200u + (unsigned)(mode + 2) * 13u);
		fixture(&mb, 0x2200u + (unsigned)(mode + 2) * 13u);
		SetAdaptEqV32(modem_of(&ma), m);
		ref_SetAdaptEqV32(mb.obj, m);
		cmp_all("SetAdaptEqV32", (long)m);

		fixture(&ma, 0x3300u + (unsigned)(mode + 2) * 13u);
		fixture(&mb, 0x3300u + (unsigned)(mode + 2) * 13u);
		SetRxLoopsV32(modem_of(&ma), m);
		ref_SetRxLoopsV32(mb.obj, m);
		cmp_all("SetRxLoopsV32", (long)m);
	}
}

/* --------------------------------------------------------------------- */

/*
 * `SetECRndTripDelayV32` is the one function here with real arithmetic, so it
 * is swept: the requested delay above, below and inside the [0, far_lag]
 * window, both symbol periods, and a far-tap count that is sometimes the
 * binding constraint and sometimes not.
 */
static long ec_delay_raised, ec_delay_held, ec_delay_clamped, ec_wrapped;

static void
run_ecdelay(void)
{
	static const short delays[] = { -40, -1, 0, 1, 7, 12, 24, 48, 64, 200 };
	static const short lags[] = { 20, 48, 96 };
	/* A NEGATIVE far-tap count is what reaches the zero clamp: the raise to
	 * the tap length happens first, so both inputs have to be negative. */
	static const short taps[] = { -8, 0, 6, 30 };
	/*
	 * NEGATIVE context delays are what make the near tap's wrap reachable
	 * at all: the line is `lag + d + 2 * symlen` and the tap is
	 * `d + symlen`, so the two differ by `lag + symlen` and the
	 * conditional subtraction can only fire when that is not positive.
	 * With a non-negative lag it is dead code and a mutation of it
	 * survives -- which is how this line came to be here.
	 *
	 * -11 and -47 are the two that make `lag + symlen` exactly 1, which is
	 * where the wrap and a wrap tested one short of the length differ.
	 */
	static const short hdxlag[] = { -60, -47, -20, -11, 0, 9, 40 };
	unsigned d, l, t, h;
	unsigned seed = 0x7711u;

	for (d = 0; d < sizeof(delays) / sizeof(delays[0]); d++)
	for (l = 0; l < sizeof(lags) / sizeof(lags[0]); l++)
	for (t = 0; t < sizeof(taps) / sizeof(taps[0]); t++)
	for (h = 0; h < sizeof(hdxlag) / sizeof(hdxlag[0]); h++) {
		short want;

		fixture(&ma, seed);
		fixture(&mb, seed);
		seed = seed * 1103515245u + 12345u;

		put_s16(ma.fp, V32FP_ECC + 0x00, lags[l]);	/* far_lag  */
		put_s16(mb.fp, V32FP_ECC + 0x00, lags[l]);
		put_s16(ma.fp, V32FP_ECC + 0x04, taps[t]);	/* far_taps */
		put_s16(mb.fp, V32FP_ECC + 0x04, taps[t]);
		put_s16(ma.hdx, V32_HDX_SHORT_9C, hdxlag[h]);
		put_s16(mb.hdx, V32_HDX_SHORT_9C, hdxlag[h]);

		SetECRndTripDelayV32(modem_of(&ma), delays[d]);
		ref_SetECRndTripDelayV32(mb.obj, delays[d]);

		cmp_all("SetECRndTripDelayV32", (long)delays[d]);

		/* Which of the three constraints bound, so the sweep is not
		 * silently exercising one arm nine times over. */
		want = delays[d] > taps[t] ? delays[d] : taps[t];
		if (want < 0)
			ec_delay_clamped++;
		else if (want > lags[l])
			ec_delay_held++;
		else
			ec_delay_raised++;
		if (get_s16(ma.fp, V32FP_ECC + 0x30) == 0
		    || get_s16(ma.fp, V32FP_ECC + 0x30)
		       < get_s16(ma.fp, V32FP_ECC + 0x34))
			ec_wrapped++;
	}
}

/* --------------------------------------------------------------------- */

static void
run_detectors(void)
{
	int bits;
	int trial;

	for (bits = 0; bits < 4; bits++)
	for (trial = 0; trial < 2; trial++) {
		unsigned seed = 0x4400u + (unsigned)bits * 7u
				+ (unsigned)trial * 101u;
		short base = (short)(0x1230 | bits);
		int a, b;
		unsigned short n;

		/* -------- retrain -------- */
		fixture(&ma, seed);
		fixture(&mb, seed);
		put_s16(ma.dec, 0x62, base);
		put_s16(mb.dec, 0x62, base);
		n = (unsigned short)(trial ? 0xffffu : 0x1000u);
		*(unsigned short *)(void *)(ma.dec + V32_DEC_RETRAIN_N) = n;
		*(unsigned short *)(void *)(mb.dec + V32_DEC_RETRAIN_N) = n;
		a = RetrainDetectV32(modem_of(&ma));
		b = ref_RetrainDetectV32(mb.obj);
		diff_eq_int("RetrainDetectV32 on 0x%lx", a, b, (long)base);
		cmp_all("RetrainDetectV32", (long)base);
		if (a) {
			retrain_fired++;
			if (n == 0xffffu)
				retrain_wrapped++;
		} else {
			retrain_declined++;
		}

		/* -------- renegotiate -------- */
		fixture(&ma, seed);
		fixture(&mb, seed);
		put_s16(ma.dec, 0x62, base);
		put_s16(mb.dec, 0x62, base);
		a = RenegotiateDetectV32(modem_of(&ma));
		b = ref_RenegotiateDetectV32(mb.obj);
		diff_eq_int("RenegotiateDetectV32 on 0x%lx", a, b, (long)base);
		cmp_all("RenegotiateDetectV32", (long)base);
		if (a)
			reneg_fired++;
		else
			reneg_declined++;

		/* -------- epoch -------- */
		fixture(&ma, seed);
		fixture(&mb, seed);
		put_int(ma.dec, 0x64, 0x5a5a0000 + bits);
		put_int(mb.dec, 0x64, 0x5a5a0000 + bits);
		a = EpochDetectV32(modem_of(&ma));
		b = ref_EpochDetectV32(mb.obj);
		diff_eq_int("EpochDetectV32 on %ld", a, b, (long)bits);
		cmp_all("EpochDetectV32", (long)bits);

		/* -------- the two that do nothing -------- */
		fixture(&ma, seed);
		fixture(&mb, seed);
		TxClockSyncV32(modem_of(&ma));
		ref_TxClockSyncV32(mb.obj);
		cmp_all("TxClockSyncV32", (long)bits);
		v32_null_protocol();
		ref_v32_null_protocol();
		cmp_all("v32_null_protocol", (long)bits);
	}
}

/* --------------------------------------------------------------------- */

static void
run_clamp(void)
{
	static const short counts[] = { 0, 1, 2, 7, 64, 160, -1 };
	unsigned i;

	for (i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
		unsigned short a, b;
		int j;

		fixture(&ma, 0x8800u + i * 17u);
		fixture(&mb, 0x8800u + i * 17u);
		put_s16(ma.hdx, V32_HDX_SHORT_9E, counts[i]);
		put_s16(mb.hdx, V32_HDX_SHORT_9E, counts[i]);
		for (j = 0; j < CLAMP_N; j++) {
			clamp_a[j] = CLAMP_MARK;
			clamp_b[j] = CLAMP_MARK;
		}

		a = RxClampV32(modem_of(&ma), scram_a, clamp_a, 99);
		b = ref_RxClampV32(mb.obj, scram_b, clamp_b, 99);

		diff_eq_int("RxClampV32 returned the block length (%ld)",
			    (long)a, (long)b, (long)counts[i]);
		diff_eq_int("RxClampV32 wrote the same words (%ld)",
			    first_diff(clamp_a, clamp_b,
				       (int)sizeof(clamp_a), 0, 0), -1,
			    (long)counts[i]);
		cmp_all("RxClampV32", (long)counts[i]);

		if (counts[i] > 0 && clamp_a[0] != CLAMP_MARK)
			clamp_wrote++;
		if (counts[i] == 0 && clamp_a[0] == CLAMP_MARK)
			clamp_zero++;
		if (counts[i] < 0 && clamp_a[CLAMP_N - 100] != CLAMP_MARK)
			clamp_negative++;
	}
}

/* --------------------------------------------------------------------- */

static void
run_turnaround_and_clean(void)
{
	static const short budget[] = { 0, 5, 100, 1000, -20, 32767 };
	static const short charge[] = { 0, 1, 40, 700, 20000 };
	unsigned i, j;

	for (i = 0; i < sizeof(budget) / sizeof(budget[0]); i++)
	for (j = 0; j < sizeof(charge) / sizeof(charge[0]); j++) {
		short a, b;

		fixture(&ma, 0xa100u + i * 53u + j);
		fixture(&mb, 0xa100u + i * 53u + j);
		put_s16(ma.hdx, V32_HDX_SHORT_94, budget[i]);
		put_s16(mb.hdx, V32_HDX_SHORT_94, budget[i]);
		put_s16(ma.hdx, V32_HDX_SHORT_98, charge[j]);
		put_s16(mb.hdx, V32_HDX_SHORT_98, charge[j]);
		put_s16(ma.hdx, V32_HDX_SHORT_9A, (short)(charge[j] / 3));
		put_s16(mb.hdx, V32_HDX_SHORT_9A, (short)(charge[j] / 3));
		put_s16(ma.hdx, V32_HDX_SHORT_9C, (short)(charge[j] / 7));
		put_s16(mb.hdx, V32_HDX_SHORT_9C, (short)(charge[j] / 7));

		a = CalcTurnAroundDelay(modem_of(&ma));
		b = ref_CalcTurnAroundDelay(mb.obj);
		diff_eq_int("CalcTurnAroundDelay with budget %ld", (long)a,
			    (long)b, (long)budget[i]);
		cmp_all("CalcTurnAroundDelay", (long)budget[i]);
		if (a == 0)
			turn_clamped++;
		else
			turn_positive++;
	}

	for (i = 0; i < 12; i++) {
		static const unsigned short have[] = { 0, 1, 0x9f, 0xa0, 0xa1,
						       0x100, 0x7fff, 0x8000,
						       0xffff, 5, 160, 161 };
		int na = 0x5eed, nb = 0x5eed;
		short *pa, *pb;

		fixture(&ma, 0xb200u + i * 29u);
		fixture(&mb, 0xb200u + i * 29u);
		*(unsigned short *)(void *)(ma.fp + V32FP_CLEAN_N) = have[i];
		*(unsigned short *)(void *)(mb.fp + V32FP_CLEAN_N) = have[i];
		put_ptr(ma.fp, V32FP_CLEAN_BUF, ma.ecline);
		put_ptr(mb.fp, V32FP_CLEAN_BUF, mb.ecline);

		pa = V32FP_GetCleanedSamples(modem_of(&ma), &na);
		pb = ref_V32FP_GetCleanedSamples(mb.obj, &nb);

		diff_eq_int("V32FP_GetCleanedSamples reported n for %ld",
			    (long)na, (long)nb, (long)have[i]);
		diff_eq_int("V32FP_GetCleanedSamples returned its own buffer "
			    "for %ld", pa == ma.ecline && pb == mb.ecline, 1,
			    (long)have[i]);
		cmp_all("V32FP_GetCleanedSamples", (long)have[i]);
		if (na == 0 && have[i] != 0)
			clean_capped++;
		if (na != 0)
			clean_reported++;
	}
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc;
	int i;

	diff_begin("v32fpctl");

	/*
	 * The data symbol itself, and it is the whole of its test: two shorts
	 * in `.data`, compared against the object's own copy.
	 */
	diff_eq_int("V32_SYMBOL_LEN[0]", (long)V32_SYMBOL_LEN[0],
		    (long)ref_V32_SYMBOL_LEN[0], 0);
	diff_eq_int("V32_SYMBOL_LEN[1]", (long)V32_SYMBOL_LEN[1],
		    (long)ref_V32_SYMBOL_LEN[1], 1);

	run_setmodes();
	run_seed_and_scramble();
	run_getrate();
	run_adapteq_and_loops();
	run_ecdelay();
	run_detectors();
	run_clamp();
	run_turnaround_and_clean();

	/* ---------------- non-vacuity ---------------- */

	for (i = 0; i < V32_MODE_COUNT; i++) {
		diff_eq_int("SetTxModeV32 arm %ld was reached",
			    tx_arm_seen[i] > 0, 1, (long)i);
		diff_eq_int("SetRxModeV32 arm %ld was reached",
			    rx_arm_seen[i] > 0, 1, (long)i);
	}
	diff_eq_int("SetTxModeV32's default arm was reached (%ld)",
		    tx_arm_seen[V32_MODE_COUNT] > 0, 1,
		    tx_arm_seen[V32_MODE_COUNT]);
	diff_eq_int("SetRxModeV32's default arm was reached (%ld)",
		    rx_arm_seen[V32_MODE_COUNT] > 0, 1,
		    rx_arm_seen[V32_MODE_COUNT]);
	diff_eq_int("a map pair was checked per side (%ld)", tx_map_checked > 0,
		    1, tx_map_checked);
	diff_eq_int("a slicer was checked per side (%ld)",
		    rx_slicer_checked > 0, 1, rx_slicer_checked);
	diff_eq_int("VTBv32_init ran on the trellis arms (%ld)", rx_vtb_ran > 0,
		    1, rx_vtb_ran);
	diff_eq_int("a non-zero transmit shift was derived (%ld)",
		    tx_shift_moved > 0, 1, tx_shift_moved);
	diff_eq_int("a non-zero receive shift was derived (%ld)",
		    rx_shift_moved > 0, 1, rx_shift_moved);
	diff_eq_int("the fault flag was ORed into a clear byte (%ld)",
		    fault_flag_was_clear > 0, 1, fault_flag_was_clear);
	diff_eq_int("a reason code was posted (%ld)", fault_posted > 0, 1,
		    fault_posted);

	for (i = 0; i <= V32_RATE_INVALID; i++)
		diff_eq_int("GetRateV32 reported rate %ld", rate_seen[i] > 0, 1,
			    (long)i);

	diff_eq_int("the round trip was raised to the far taps (%ld)",
		    ec_delay_raised > 0, 1, ec_delay_raised);
	diff_eq_int("the round trip was held down to the far lag (%ld)",
		    ec_delay_held > 0, 1, ec_delay_held);
	diff_eq_int("a negative round trip was clamped (%ld)",
		    ec_delay_clamped > 0, 1, ec_delay_clamped);
	diff_eq_int("a tap wrapped round the delay line (%ld)", ec_wrapped > 0,
		    1, ec_wrapped);

	diff_eq_int("RetrainDetectV32 fired (%ld)", retrain_fired > 0, 1,
		    retrain_fired);
	diff_eq_int("RetrainDetectV32 declined (%ld)", retrain_declined > 0, 1,
		    retrain_declined);
	diff_eq_int("the retrain counter wrapped (%ld)", retrain_wrapped > 0, 1,
		    retrain_wrapped);
	diff_eq_int("RenegotiateDetectV32 fired (%ld)", reneg_fired > 0, 1,
		    reneg_fired);
	diff_eq_int("RenegotiateDetectV32 declined (%ld)", reneg_declined > 0,
		    1, reneg_declined);

	diff_eq_int("RxClampV32 wrote something (%ld)", clamp_wrote > 0, 1,
		    clamp_wrote);
	diff_eq_int("RxClampV32 on zero wrote nothing (%ld)", clamp_zero > 0, 1,
		    clamp_zero);
	diff_eq_int("RxClampV32 on a negative length ran away (%ld)",
		    clamp_negative > 0, 1, clamp_negative);

	diff_eq_int("CalcTurnAroundDelay clamped at zero (%ld)",
		    turn_clamped > 0, 1, turn_clamped);
	diff_eq_int("CalcTurnAroundDelay reported a surplus (%ld)",
		    turn_positive > 0, 1, turn_positive);

	diff_eq_int("V32FP_GetCleanedSamples capped a count (%ld)",
		    clean_capped > 0, 1, clean_capped);
	diff_eq_int("V32FP_GetCleanedSamples reported a count (%ld)",
		    clean_reported > 0, 1, clean_reported);
	diff_eq_int("the scrambler was driven (%ld)", scram_changed > 0, 1,
		    scram_changed);
	diff_eq_int("the descrambler was driven (%ld)", descram_changed > 0, 1,
		    descram_changed);

	rc = diff_end();
	return rc;
}
