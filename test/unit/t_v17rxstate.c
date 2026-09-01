/*
 * t_v17rxstate.c -- differential test of V.17's half-duplex RECEIVE machine:
 *                   `RxNextStateV17` and the five state handlers around it.
 *
 * `t_v17fax.c` already covers the two handlers that were written first,
 * `RxHdxDataV17` and `RxHdxErrorV17`, and every primitive all seven of them
 * call.  This file covers the rest, and it is built round three problems that
 * a "run it and compare the memory" test does NOT solve on its own.
 *
 * ---------------------------------------------------------------------------
 * THE THREE HANDLERS THAT ARE ALMOST THE SAME FUNCTION
 *
 * `RxHdxBridgeV17` and `RxHdxPrtcolV17` are byte-for-byte identical, and
 * `RxHdxScramV17` is the same 210 bytes plus one thing: on the expiry path it
 * reads `V17RXC_RATE_CODE` and writes the matching `V17RX_STATUS_RATE_*` into
 * `V17RX_OBJ_RESULT`, where the other two leave `V17RX_STATUS_CARRIER` there.
 *
 * A reconstruction that copied one of the three into the other two passes
 * EVERY check that does not drive an expiry with a rate code set.  So
 * `run_train` drives each of the three to expiry at all four rate codes, and
 * asserts by name that Bridge and Prtcol leave the status byte at
 * `V17RX_STATUS_CARRIER` on a block where Scram would have changed it.  The
 * separating counter `train_ladder_sep` is what says that check has teeth.
 *
 * ---------------------------------------------------------------------------
 * THE FIELDS THAT ARE SUBSCRIPTS AND NOT DEREFERENCES (D955, F8587)
 *
 * `RxNextStateV17`'s EPOCH_DET arm advances `struct fpm_agc::cfg.alpha` and
 * `::cfg.beta` by one `short` each.  Those are POINTERS INTO A TABLE, and a
 * blob-against-blob comparison cannot catch an unplanted one: both sides walk
 * off the same end of the same array and agree about the garbage.
 *
 * So both fixtures point at `AGCv17_CFG`'s OWN two-entry tables -- the
 * object's `.rodata`, reached through `ref_AGCv17_CFG` -- and the check is on
 * the POINTER VALUES after the call, not on anything downstream of them:
 * `alpha` must land one `short` along and hold 0x7333, `beta` one `short`
 * along and hold 0x0ccd.  That is the acquisition-to-tracking step the arm
 * exists for, and it is asserted as two addresses and two values rather than
 * as an AGC output that would be the same either way.
 *
 * NO FIXTURE DRIVES THE EPOCH_DET ARM TWICE, because the tables have exactly
 * two entries and the object does not bound the increment.  That is D1216 and
 * it is reproduced, not fixed.
 *
 * `StoreCoefV17` copies 49 shorts through each of two receiver-state pointers,
 * so both are planted at full size; `FPM_AGC_Freeze` writes the AGC's own
 * `freeze`, which the whole-block compare covers.
 *
 * ---------------------------------------------------------------------------
 * THE DENOMINATORS (F134)
 *
 * Every named wrong reading below is a variant of a hand-written model of the
 * object's own sequence, and every variant's separating count is asserted
 * non-zero at the end.  Beside them are RUN counters -- how many trials
 * actually reached each switch arm, each carrier verdict and each countdown
 * outcome -- because a check that never ran is indistinguishable from a check
 * that passed.
 *
 * ---------------------------------------------------------------------------
 * THE WRONG READINGS, AND THE ONE THAT IS NOT SEPARABLE
 *
 *   RxNextStateV17
 *       - the state read from ctl + 0x1a (the countdown) rather than + 0x18.
 *       - the state read as a byte, so 0x0100 reads as state 0.
 *       - no default arm: state 7 and above handled as state 6.
 *       - `V17RXC_INT_0010` read 16-bit, separated by 0x00010000.
 *       - that gate inverted, on both arms that read it.
 *       - `Restore_rateV17` not called.
 *       - the two countdown seeds 1 and 62 transposed.
 *       - the AGC step omitted, applied to `alpha` only, and applied twice.
 *       - the PROTOCOL/SCRAM arm clearing `V17RX_OBJ_RESULT_B2` as its BRIDGE
 *         sibling does.  This is the one asymmetry of the seven arms and the
 *         one a careless reading smooths over.
 *       - `StoreCoefV17` not called on the BRIDGE arm.
 *       - `V17RX_FLAG_DATA` written into +0x2a rather than +0x29.
 *       - the IDLE arm seeding the countdown as the other six do.
 *       - the rate ladder shifted by one, and its default arm made a fourth
 *         case so that an unknown code writes nothing.
 *       - the DATA arm not clearing `V17RXC_INT_0008`.
 *       - the default arm installing a handler, and leaving CARRIER set.
 *
 *   the handlers
 *       - the countdown decremented but not stored, and stored but tested
 *         UNSIGNED -- seeded at 0x8000, where the signed test the object emits
 *         expires immediately and an unsigned one runs on.
 *       - the countdown tested `>= 0` rather than `> 0`.
 *       - the return value `n` on every path rather than only on the
 *         transition (D1214).
 *       - `RxHdxScramV17`'s rate ladder given to Bridge and Prtcol, and taken
 *         away from Scram.
 *       - `V17RX_FLAG_LOW_SNR` cleared before the test, as `RxHdxDataV17`
 *         does and these three do not.
 *       - the error arm leaving the state alone, and installing the wrong
 *         handler.
 *       - `RxHdxEpochDetV17`'s `||` evaluated in full, so an expired countdown
 *         still consults `EpochDetectV17`.  Driven with the epoch flag CLEAR
 *         and the countdown expired, which is the only shape that separates
 *         them.
 *       - `RxHdxIdleV17`'s 0x1fff compared `<` and compared UNSIGNED, driven
 *         at 0x1ffe, 0x1fff, 0x2000 and at a negative error.
 *       - `RxHdxIdleV17` testing `CarrierDetectV17`'s verdict rather than
 *         re-reading the flag byte.  NOT SEPARABLE and NOT CLAIMED: the byte
 *         was just assigned from that verdict, so the two agree for every
 *         input.  The re-read is in the source because the object emits it.
 *       - `RxHdxStartV17` given an error arm.
 *
 * BOTH SIDES' DIAGNOSTICS ARE COMPARED AS OUTPUT.  `RxNextStateV17` prints on
 * all eight arms and `RxHdxIdleV17` prints on its transition, all gated on
 * `dsplibs_debug_level > 1`, and each print sits in an out-of-line block that
 * RELOADS the control block afterwards -- so a missed reload shows up only
 * with the level raised.  Every sweep runs at level 0 and at level 2.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v17fax.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sdm.h"
#include "dsplib/sysdep.h"

/* --------------------------------------------------------------------- */
/* The blob's side                                                        */

extern void ref_RxNextStateV17(void *modem);
extern short ref_RxHdxIdleV17(void *modem, short *in, short *out,
			      unsigned short *count);
extern short ref_RxHdxScramV17(void *modem, short *in, short *out,
			       unsigned short *count);
extern short ref_RxHdxBridgeV17(void *modem, short *in, short *out,
				unsigned short *count);
extern short ref_RxHdxPrtcolV17(void *modem, short *in, short *out,
				unsigned short *count);
extern short ref_RxHdxEpochDetV17(void *modem, short *in, short *out,
				  unsigned short *count);
extern short ref_RxHdxStartV17(void *modem, short *in, short *out,
			       unsigned short *count);
extern short ref_RxHdxDataV17(void *modem, short *in, short *out,
			      unsigned short *count);
extern short ref_RxHdxErrorV17(void *modem, short *in, short *out,
			       unsigned short *count);

extern void ref_StoreCoefV17(void *modem);
extern void ref_Restore_rateV17(void *modem);
extern int ref_CarrierDetectV17(void *modem);
extern int ref_EpochDetectV17(void *modem);
extern short ref_GetSNRV17(void *modem);
extern unsigned short ref_DemodDataV17(void *modem, short *in,
				       unsigned short *bits,
				       unsigned short count);
extern void ref_DescrambleDataV17(void *modem, unsigned short *data,
				  unsigned short count);

extern void ref_FPM_AGC_Freeze(struct fpm_agc *agc);
extern void ref_FPM_AGC_init(struct fpm_agc *agc, const struct fpm_agc_cfg *cfg,
			     int reset);
extern void ref_FPM_MRF_init(struct fpm_mrf *state,
			     const struct fpm_mrf_cfg *cfg, int reset);
extern void ref_FPM_MRF_free(struct fpm_mrf *state);
extern void ref_FPM_SRE_init(struct fpm_sre *sre, const struct fpm_sre_cfg *cfg,
			     int reset);
extern void ref_FPM_SRE_free(struct fpm_sre *sre);
extern void ref_FPM_FSE_init(struct fpm_fse *state,
			     const struct fpm_fse_cfg *cfg, int reset);
extern void ref_FPM_FSE_free(struct fpm_fse *state);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *state,
					  const struct fpm_mtd_cfg *cfg);
extern void ref_FPM_MTD_delete(struct fpm_mtd *state);
extern void *ref_FPM_TONE_create(void *state, const void *cfg);
extern void ref_FPM_TONE_delete(void *state);
extern void ref_SDM_init(struct fpm_sdm *sdm, const struct fpm_sdm_cfg *cfg);

extern const struct fpm_agc_cfg ref_AGCv17_CFG;
extern const struct fpm_mtd_cfg ref_MTDv22_CFG;
extern const struct fpm_mrf_cfg MRFv32_CFG;
extern const struct fpm_sre_cfg SREv32_CFG;

extern unsigned int ref_dsplibs_debug_level;

/* --------------------------------------------------------------------- */
/* The fixture                                                            */

#define OBJ_SIZE	0x80
#define CTL_SIZE	0x80		/* past V17RXC_AGC + sizeof(fpm_agc) */
#define RXS_SIZE	0x5000		/* the object reaches +0x4fb8        */
#define COEF_SLOTS	64		/* V17_COEF_N is 49; the rest catch
					 * a loop that runs one too far      */
#define DEM_BUF		2048
/*
 * SIXTY-FOUR AND NOT SIXTEEN, and this is D955 rather than a preference.
 * `FPM_FSE_init` replaces the planted `V17RXS_COEF0` / `_COEF1` pointers with
 * its own `icoeff` and `qcoeff` allocations, which are `taps` entries long --
 * and `StoreCoefV17` copies `V17_COEF_N` = 49 of them.  With sixteen taps the
 * copy walks 33 entries past the end of each array, into heap the two sides
 * allocated separately, and the saved arrays then differ for ever with nothing
 * in the reconstruction to blame.  The equaliser is synthetic here, so the tap
 * count is ours to choose; it is chosen to make the subscript legal.
 */
#define DEM_TAPS	64
#define DEM_CLK		8
#define MTD_ACC		16

struct fix {
	unsigned char	robj[OBJ_SIZE];
	unsigned char	ctl[CTL_SIZE];
	unsigned char	rxs[RXS_SIZE];
	short		coef0[COEF_SLOTS];
	short		coef1[COEF_SLOTS];
	short		save0[COEF_SLOTS];
	short		save1[COEF_SLOTS];
	short		ratesave[4];
	struct fpm_mtd	mtd;
	short		mtd_acc[MTD_ACC];
	short		buf2[DEM_BUF];
	short		mrfbuf[DEM_BUF];
	short		srebuf[DEM_BUF];
	short		detbuf[DEM_BUF];
	double		align;
};

static struct fix ma, mb, mc;

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

/* xorshift32 is linear over GF(2); see t_v17fax.c and finding F9105. */
static unsigned
spread(unsigned base, long where)
{
	return base ^ ((unsigned)where * 0x9e3779b9u);
}

static void
put_ptr(unsigned char *p, int off, const void *v)
{
	*(const void **)(void *)(p + off) = v;
}

static void *
get_ptr(const unsigned char *p, int off)
{
	return *(void *const *)(const void *)(p + off);
}

static void
put_s(unsigned char *p, int off, short v)
{
	*(short *)(void *)(p + off) = v;
}

static short
get_s(const unsigned char *p, int off)
{
	return *(const short *)(const void *)(p + off);
}

static unsigned short
get_us(const unsigned char *p, int off)
{
	return *(const unsigned short *)(const void *)(p + off);
}

static void
put_i(unsigned char *p, int off, int v)
{
	*(int *)(void *)(p + off) = v;
}

static int
get_i(const unsigned char *p, int off)
{
	return *(const int *)(const void *)(p + off);
}

static struct fpm_agc *
fix_agc(struct fix *f)
{
	return (struct fpm_agc *)(void *)(f->rxs + V17RXS_AGC);
}

static void
rehome(struct fix *f)
{
	put_ptr(f->robj, V17RX_OBJ_COEFSAVE0, f->save0);
	put_ptr(f->robj, V17RX_OBJ_COEFSAVE1, f->save1);
	put_ptr(f->robj, V17RX_OBJ_RATESAVE, f->ratesave);
	put_ptr(f->robj, V17RX_OBJ_CTL, f->ctl);
	put_ptr(f->robj, V17RX_OBJ_STATE, f->rxs);
	put_ptr(f->rxs, V17RXS_COEF0, f->coef0);
	put_ptr(f->rxs, V17RXS_COEF1, f->coef1);
}

/*
 * Every byte pseudorandom before any pointer goes in, so an offset wrong by
 * two reads noise rather than a plausible zero.
 */
static void
fixture(struct fix *f, unsigned seed)
{
	unsigned char *p = (unsigned char *)f;
	int n = (int)((const char *)&f->align - (const char *)f);
	int i;

	rng_seed(seed);
	for (i = 0; i < n; i++)
		p[i] = (unsigned char)rng_next();
	f->align = 0.0;

	rehome(f);

	if (get_s(f->rxs, V17RXS_AGC_SIGNAL + 2) == 0)
		put_s(f->rxs, V17RXS_AGC_SIGNAL + 2, 0x1234);
}

static long
first_diff(const unsigned char *a, const unsigned char *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return i;
	return -1;
}

static long
robj_diff(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < OBJ_SIZE; i++) {
		if (i >= V17RX_OBJ_COEFSAVE0
		    && i < V17RX_OBJ_RATESAVE + (int)sizeof(void *))
			continue;
		if (i >= V17RX_OBJ_CTL
		    && i < V17RX_OBJ_STATE + (int)sizeof(void *))
			continue;
		if (a->robj[i] != b->robj[i])
			return i;
	}
	return -1;
}

/*
 * The dispatch slot is compared by IDENTITY, not by bytes: our side stores our
 * handler's address and the blob's side stores `ref_`'s, so they differ for
 * ever and the byte compare must skip them.  `slot_index` is what puts the
 * check back.
 */
static long
ctl_diff(const struct fix *a, const struct fix *b, int demod)
{
	int i;

	for (i = 0; i < CTL_SIZE; i++) {
		if (i >= V17RXC_PROCESS
		    && i < V17RXC_PROCESS + (int)sizeof(void *))
			continue;
		if (demod) {
			if (i < V17RXC_TONE + (int)sizeof(void *))
				continue;	/* the MTD and the notch   */
			if (i >= V17RXC_SCRATCH
			    && i < V17RXC_SCRATCH + (int)sizeof(void *))
				continue;
			if (i >= V17RXC_MTD2
			    && i < V17RXC_BUF2 + (int)sizeof(void *))
				continue;
		}
		if (a->ctl[i] != b->ctl[i])
			return i;
	}
	return -1;
}

static long
rxs_diff(const struct fix *a, const struct fix *b, int demod)
{
	int i;

	for (i = 0; i < RXS_SIZE; i++) {
		if (i >= V17RXS_COEF0
		    && i < V17RXS_COEF1 + (int)sizeof(void *))
			continue;
		if (demod) {
			if (i >= V17RXS_MRF + 0x18 && i < V17RXS_MRF + 0x1c)
				continue;
			if (i >= V17RXS_SRE + 0x50 && i < V17RXS_SRE + 0x5c)
				continue;
			if (i >= V17RXS_SRE + 0x74 && i < V17RXS_SRE + 0x78)
				continue;
			if (i >= V17RXS_FSE + 0x54 && i < V17RXS_FSE + 0x5c)
				continue;
			if (i >= V17RXS_FSE + 0x60 && i < V17RXS_FSE + 0x6c)
				continue;
			if (i >= V17RXS_BUF_MRF
			    && i < V17RXS_BUF_SRE + (int)sizeof(void *))
				continue;
		}
		if (a->rxs[i] != b->rxs[i])
			return i;
	}
	return -1;
}

static void
compare_all(struct fix *a, struct fix *b, long where, int demod)
{
	diff_eq_int("at %ld: first differing receive-instance byte",
		    robj_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing control-block byte",
		    ctl_diff(b, a, demod), -1, where);
	diff_eq_int("at %ld: first differing receiver-state byte",
		    rxs_diff(b, a, demod), -1, where);
	diff_eq_int("at %ld: first differing saved-coefficient byte (0)",
		    first_diff((const unsigned char *)b->save0,
			       (const unsigned char *)a->save0,
			       (int)sizeof(a->save0)), -1, where);
	diff_eq_int("at %ld: first differing saved-coefficient byte (1)",
		    first_diff((const unsigned char *)b->save1,
			       (const unsigned char *)a->save1,
			       (int)sizeof(a->save1)), -1, where);
	diff_eq_int("at %ld: first differing saved-rate byte",
		    first_diff((const unsigned char *)b->ratesave,
			       (const unsigned char *)a->ratesave,
			       (int)sizeof(a->ratesave)), -1, where);
	diff_eq_int("at %ld: first differing source-coefficient byte (0)",
		    first_diff((const unsigned char *)b->coef0,
			       (const unsigned char *)a->coef0,
			       (int)sizeof(a->coef0)), -1, where);
	diff_eq_int("at %ld: first differing source-coefficient byte (1)",
		    first_diff((const unsigned char *)b->coef1,
			       (const unsigned char *)a->coef1,
			       (int)sizeof(a->coef1)), -1, where);
}

/* --------------------------------------------------------------------- */
/* The dispatch slot, by identity                                         */

/*
 * Nine handlers on each side.  A slot the object left alone holds the
 * fixture's own pseudorandom bytes, which are the SAME on both sides because
 * both fixtures are laid down from one seed, so an untouched slot scores -1 on
 * both and still compares equal.
 */
static const void *ours_fn[] = {
	(const void *)(size_t)0,
	(const void *)RxHdxStartV17,
	(const void *)RxHdxEpochDetV17,
	(const void *)RxHdxPrtcolV17,
	(const void *)RxHdxBridgeV17,
	(const void *)RxHdxScramV17,
	(const void *)RxHdxDataV17,
	(const void *)RxHdxIdleV17,
	(const void *)RxHdxErrorV17
};

static const void *ref_fn[] = {
	(const void *)(size_t)0,
	(const void *)ref_RxHdxStartV17,
	(const void *)ref_RxHdxEpochDetV17,
	(const void *)ref_RxHdxPrtcolV17,
	(const void *)ref_RxHdxBridgeV17,
	(const void *)ref_RxHdxScramV17,
	(const void *)ref_RxHdxDataV17,
	(const void *)ref_RxHdxIdleV17,
	(const void *)ref_RxHdxErrorV17
};

#define NHANDLER	((int)(sizeof(ours_fn) / sizeof(ours_fn[0])))

static int
slot_index(const void *p, int blob)
{
	int i;

	for (i = 1; i < NHANDLER; i++)
		if (p == (blob ? ref_fn[i] : ours_fn[i]))
			return i;
	return -1;
}

/* Install handler `h` (an index into the tables above) on side `blob`. */
static void
put_slot(struct fix *f, int h, int blob)
{
	put_ptr(f->ctl, V17RXC_PROCESS, blob ? ref_fn[h] : ours_fn[h]);
}

static void
compare_slot(struct fix *a, struct fix *b, long where)
{
	diff_eq_int("at %ld: the handler installed",
		    slot_index(get_ptr(b->ctl, V17RXC_PROCESS), 0),
		    slot_index(get_ptr(a->ctl, V17RXC_PROCESS), 1), where);
}

/* --------------------------------------------------------------------- */
/* Diagnostics                                                            */

static long dbg_lines_seen;

/*
 * ONE LINE IS DROPPED FROM BOTH TRANSCRIPTS BEFORE THEY ARE COMPARED, AND IT
 * IS NOT ONE OF THIS BATCH'S.
 *
 * `FPM_FSE_receive` prints "Decoder Error = %d" every `FPM_FSE_SHOW_SAMPLES`
 * INPUT SAMPLES, gated on `avg_err_show.0` -- a function-scope static in
 * `.bss`, one per side, shared by every equaliser instance and never reset.
 * The variant loops below drive the BLOB's `ref_DemodDataV17` a dozen times
 * for every one call of ours, so the two counters walk thousands of samples
 * apart and one side emits the line where the other does not.  The static is
 * the object's and `src/dsp/fpm_fse.c` reproduces it; what is not composable
 * is "compare the whole transcript" across sections that drive the two sides
 * unequally.  Findings F9120 and F9238 are the same problem answered the same
 * way from V.27ter and from `t_v17fax.c`.
 *
 * So the filter drops exactly that line and NOTHING else -- every line this
 * batch's own functions emit is compared, text and count.
 */
#define DBG_TEXT	16384

static char dbg_ours[DBG_TEXT], dbg_blob[DBG_TEXT];

static unsigned
transcript_filter(char *dst, const char *src)
{
	static const char skip[] = "Decoder Error";
	unsigned lines = 0;
	size_t o = 0;

	while (*src != '\0') {
		const char *e = strchr(src, '\n');
		size_t n = e != 0 ? (size_t)(e - src) + 1 : strlen(src);

		if (strncmp(src, skip, sizeof(skip) - 1) != 0) {
			if (o + n + 1 < (size_t)DBG_TEXT) {
				memcpy(dst + o, src, n);
				o += n;
			}
			lines++;
		}
		src += n;
	}
	dst[o] = '\0';
	return lines;
}

static void
debug_begin(unsigned level)
{
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();
	dsplibs_debug_level = level;
	ref_dsplibs_debug_level = level;
}

static void
debug_compare(long where)
{
	unsigned no = transcript_filter(dbg_ours,
					dsplib_debug_capture_text(0));
	unsigned nb = transcript_filter(dbg_blob,
					dsplib_debug_capture_text(1));

	if (strcmp(dbg_ours, dbg_blob) != 0)
		fprintf(stderr, "at %ld: transcripts differ\n"
				"--- ours ---\n%s--- blob ---\n%s---\n",
			where, dbg_ours, dbg_blob);
	diff_eq_int("at %ld: the same diagnostic text",
		    strcmp(dbg_ours, dbg_blob), 0, where);
	diff_eq_int("at %ld: the same number of diagnostic lines", (long)no,
		    (long)nb, where);
	dbg_lines_seen += (long)nb;
}

static void
debug_end(void)
{
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;
}

/* --------------------------------------------------------------------- */
/* Separating-trial counters                                              */

enum next_defect {
	N_NONE = 0,
	N_STATE_AT_1A,		/* the state read from the countdown        */
	N_STATE_BYTE,		/* the state read one byte wide             */
	N_NO_DEFAULT,		/* 7 and above handled as IDLE              */
	N_GATE_16BIT,		/* V17RXC_INT_0010 read as a short          */
	N_GATE_INVERTED,
	N_NO_RESTORE,		/* Restore_rateV17 not called               */
	N_HOLD_SWAPPED,		/* the 1 and the 62 transposed              */
	N_NO_AGC_STEP,
	N_AGC_ALPHA_ONLY,
	N_AGC_STEP_TWO,		/* two shorts along, not one                */
	N_SCRAM_CLEARS_B2,	/* the one asymmetry, smoothed over         */
	N_NO_STORECOEF,
	N_DATA_BIT_AT_2A,	/* V17RX_FLAG_DATA written to +0x2a         */
	N_IDLE_SEEDS,		/* the IDLE arm seeding the countdown       */
	N_LADDER_SHIFTED,	/* 8/7/6/5 rather than 9/8/7/6              */
	N_LADDER_NO_DEFAULT,	/* an unknown rate code writing nothing     */
	N_DATA_KEEPS_0008,
	N_DEFAULT_INSTALLS,
	N_DEFAULT_KEEPS_CARRIER,
	N_DEFAULT_KEEPS_DATA,	/* `and $0xdf` where the object has `$0xde` */
	N_MAX
};

enum hdx_defect {
	H_NONE = 0,
	H_COUNT_NOT_STORED,
	H_COUNT_UNSIGNED,	/* `> 0` read unsigned; seeded at 0x8000    */
	H_COUNT_GE,		/* `>= 0` rather than `> 0`                 */
	H_RETURN_N_ALWAYS,	/* D1214 smoothed over                      */
	H_LADDER_EVERYWHERE,	/* Scram's rate ladder given to all three   */
	H_LADDER_NOWHERE,	/* and taken away from Scram                */
	H_SNR_CLEARED,		/* LOW_SNR cleared before the test          */
	H_SNR_STRICT,		/* `<` rather than `<=` at the threshold    */
	H_ERR_NO_STATE,		/* the error arm not writing the state      */
	H_ERR_WRONG_HANDLER,
	H_ERR_KEEPS_CARRIER,
	H_MAX
};

/*
 * THE SHORT CIRCUIT ITSELF IS NOT A RUNTIME FACT AND IS NOT CLAIMED AS ONE.
 * `EpochDetectV17` is pure, so an `||` that evaluated its right-hand side
 * anyway would leave exactly the same memory behind, and no differential test
 * can see the difference.  What CAN be measured is the CONDITION -- that an
 * expired countdown advances the machine with the epoch flag clear -- which is
 * `P_BOTH_REQUIRED`.  That the object really does skip the call is read off
 * `jle 0xa07d3` at 0xa07c4 jumping over 0xa07c9, and is recorded in v17fax.h.
 */
enum epoch_defect {
	P_NONE = 0,
	P_BOTH_REQUIRED,	/* `&&` where the object has `||`           */
	P_EPOCH_INVERTED,
	P_DESCRAMBLES,		/* a descramble this handler does not do    */
	P_MAX
};

enum idle_defect {
	I_NONE = 0,
	I_SMALL_STRICT,		/* `<` rather than `<=` at 0x1fff           */
	I_SMALL_UNSIGNED,	/* the compare read unsigned                */
	I_NO_CARRIER_CLEAR,	/* the flag not cleared before the re-test  */
	I_STATUS_MISSING,
	I_MAX
};

enum start_defect {
	S_NONE = 0,
	S_HAS_ERROR_ARM,	/* an error arm this handler does not have  */
	S_COUNT_KEPT,
	S_NO_CARRIER_CLEAR,
	S_MAX
};

static long next_sep[N_MAX], hdx_sep[H_MAX];
static long epoch_sep[P_MAX], idle_sep[I_MAX], start_sep[S_MAX];

/* Run counters: what the sweeps actually reached. */
static long next_arm[8];		/* 0..6, and 7 for the default arm  */
static long next_restored, next_notrestored;
static long next_scram_arm, next_bridge_arm;
static long agc_checked;
static long train_carrier, train_lost, train_expired, train_held;
static long train_rate_seen[4];
static long train_ladder_sep;		/* Scram's ladder vs its two twins  */
static long epoch_expired, epoch_found, epoch_missed, epoch_lost;
static long idle_carrier, idle_nocarrier, idle_small, idle_big, idle_negative;
static long start_carrier, start_nocarrier;
static long walk_blocks, walk_states[8];
static long snr_low_seen, snr_high_seen;

/* --------------------------------------------------------------------- */
/* Layer A -- RxNextStateV17 on its own                                   */

/*
 * No demodulator is needed here: the function calls only `Restore_rateV17`,
 * `StoreCoefV17`, `FPM_AGC_Freeze` and the diagnostic printer, and installs
 * handler addresses it never calls.
 */
struct next_setup {
	short	state;
	int	gate;		/* V17RXC_INT_0010                          */
	short	rate;		/* V17RXC_RATE_CODE                         */
};

static void
next_build(struct fix *f, unsigned seed, const struct next_setup *u, int blob)
{
	fixture(f, seed);

	put_s(f->ctl, V17RXC_STATE, u->state);
	put_i(f->ctl, V17RXC_INT_0010, u->gate);
	put_s(f->ctl, V17RXC_RATE_CODE, u->rate);
	put_s(f->ctl, V17RXC_COUNTDOWN, 0x0123);
	put_i(f->ctl, V17RXC_INT_0008, 0x5a5a5a5a);
	put_slot(f, 0, blob);

	/*
	 * The object's OWN gain-control configuration, so `cfg.alpha` and
	 * `cfg.beta` point into real two-entry tables and the arm that
	 * advances them can be asserted on the pointer VALUES.  D955.
	 */
	memset(fix_agc(f), 0, sizeof(struct fpm_agc));
	ref_FPM_AGC_init(fix_agc(f), &ref_AGCv17_CFG, 1);

	put_i(f->rxs, V17RXS_RATE, 0x0777);
	put_s(f->rxs, V17RXS_USHORT_018C, 0x0040);
	f->ratesave[0] = (short)0x9abc;
}

/* The object's own sequence, with one reading changed. */
static void
drive_next(struct fix *f, int v)
{
	unsigned char *ro = f->robj;
	unsigned char *ct = f->ctl;
	short state;
	int gate;

	if (v == N_STATE_AT_1A)
		state = get_s(ct, V17RXC_COUNTDOWN);
	else if (v == N_STATE_BYTE)
		state = (short)ct[V17RXC_STATE];
	else
		state = get_s(ct, V17RXC_STATE);

	if (v == N_NO_DEFAULT && (state < 0 || state > V17RX_STATE_IDLE))
		state = V17RX_STATE_IDLE;

	switch (state) {
	case V17RX_STATE_START:
		put_s(ct, V17RXC_COUNTDOWN, 5);
		put_slot(f, 2 /* EpochDet */, 1);
		put_s(ct, V17RXC_STATE, V17RX_STATE_EPOCH_DET);
		ro[V17RX_OBJ_RESULT_B2] &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		ro[V17RX_OBJ_RESULT_B1] &= (unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_EPOCH_DET:
		gate = (v == N_GATE_16BIT) ? get_s(ct, V17RXC_INT_0010)
					   : get_i(ct, V17RXC_INT_0010);
		if (v == N_GATE_INVERTED)
			gate = !gate;
		if (v != N_NO_RESTORE && gate != 0)
			ref_Restore_rateV17(f->robj);
		if (v == N_HOLD_SWAPPED)
			put_s(ct, V17RXC_COUNTDOWN, (short)(gate ? 62 : 1));
		else
			put_s(ct, V17RXC_COUNTDOWN, (short)(gate ? 1 : 62));
		put_slot(f, 3 /* Prtcol */, 1);
		put_s(ct, V17RXC_STATE, V17RX_STATE_PROTOCOL);
		ro[V17RX_OBJ_RESULT_B2] &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		ro[V17RX_OBJ_RESULT_B1] &= (unsigned char)~V17RX_FLAG_DATA;
		if (v != N_NO_AGC_STEP) {
			int step = (v == N_AGC_STEP_TWO) ? 2 : 1;

			fix_agc(f)->cfg.alpha += step;
			if (v != N_AGC_ALPHA_ONLY)
				fix_agc(f)->cfg.beta += step;
		}
		break;

	case V17RX_STATE_PROTOCOL:
		gate = (v == N_GATE_16BIT) ? get_s(ct, V17RXC_INT_0010)
					   : get_i(ct, V17RXC_INT_0010);
		if (v == N_GATE_INVERTED)
			gate = !gate;
		put_s(ct, V17RXC_COUNTDOWN, 1);
		if (gate != 0) {
			put_slot(f, 5 /* Scram */, 1);
			put_s(ct, V17RXC_STATE, V17RX_STATE_SCRAM);
			ro[V17RX_OBJ_RESULT_B1] &=
				(unsigned char)~V17RX_FLAG_DATA;
			if (v == N_SCRAM_CLEARS_B2)
				ro[V17RX_OBJ_RESULT_B2] &=
					(unsigned char)~V17RX_RESULT_B2_BIT0;
		} else {
			put_slot(f, 4 /* Bridge */, 1);
			put_s(ct, V17RXC_STATE, V17RX_STATE_BRIDGE);
			ro[V17RX_OBJ_RESULT_B1] &=
				(unsigned char)~V17RX_FLAG_DATA;
			if (v != N_NO_STORECOEF)
				ref_StoreCoefV17(f->robj);
			ro[V17RX_OBJ_RESULT_B2] &=
				(unsigned char)~V17RX_RESULT_B2_BIT0;
		}
		break;

	case V17RX_STATE_BRIDGE:
		put_s(ct, V17RXC_COUNTDOWN, 1);
		put_slot(f, 5 /* Scram */, 1);
		put_s(ct, V17RXC_STATE, V17RX_STATE_SCRAM);
		ro[V17RX_OBJ_RESULT_B2] &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		ro[V17RX_OBJ_RESULT_B1] &= (unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_SCRAM:
		ref_FPM_AGC_Freeze(fix_agc(f));
		put_s(ct, V17RXC_COUNTDOWN, 0);
		put_slot(f, 6 /* Data */, 1);
		put_s(ct, V17RXC_STATE, V17RX_STATE_DATA);
		ro[V17RX_OBJ_RESULT_B2] &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		if (v == N_DATA_BIT_AT_2A)
			ro[V17RX_OBJ_RESULT_B2] |= V17RX_FLAG_DATA;
		else
			ro[V17RX_OBJ_RESULT_B1] |= V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_DATA:
		put_slot(f, 7 /* Idle */, 1);
		put_s(ct, V17RXC_STATE, V17RX_STATE_IDLE);
		put_s(ct, V17RXC_COUNTDOWN, 0);
		if (v != N_DATA_KEEPS_0008)
			put_i(ct, V17RXC_INT_0008, 0);
		ro[V17RX_OBJ_RESULT_B2] |= V17RX_RESULT_B2_BIT0;
		ro[V17RX_OBJ_RESULT_B1] &= (unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_IDLE:
		put_slot(f, 6 /* Data */, 1);
		put_s(ct, V17RXC_STATE, V17RX_STATE_DATA);
		ro[V17RX_OBJ_RESULT_B2] &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		ro[V17RX_OBJ_RESULT_B1] |= V17RX_FLAG_DATA;
		if (v == N_IDLE_SEEDS)
			put_s(ct, V17RXC_COUNTDOWN, 1);
		switch (get_us(ct, V17RXC_RATE_CODE)) {
		case V17RX_RATE_7200:
			ro[V17RX_OBJ_RESULT] = (v == N_LADDER_SHIFTED)
				? 8 : V17RX_STATUS_RATE_7200;
			break;
		case V17RX_RATE_9600:
			ro[V17RX_OBJ_RESULT] = (v == N_LADDER_SHIFTED)
				? 7 : V17RX_STATUS_RATE_9600;
			break;
		case V17RX_RATE_12000:
			ro[V17RX_OBJ_RESULT] = (v == N_LADDER_SHIFTED)
				? 6 : V17RX_STATUS_RATE_12000;
			break;
		case V17RX_RATE_14400:
			ro[V17RX_OBJ_RESULT] = (v == N_LADDER_SHIFTED)
				? 5 : V17RX_STATUS_RATE_14400;
			break;
		default:
			if (v != N_LADDER_NO_DEFAULT)
				ro[V17RX_OBJ_RESULT] = (v == N_LADDER_SHIFTED)
					? 5 : V17RX_STATUS_RATE_14400;
			break;
		}
		break;

	default:
		ro[V17RX_OBJ_RESULT_B2] &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		ro[V17RX_OBJ_RESULT] = V17RX_STATUS_DEFAULT;
		if (v == N_DEFAULT_KEEPS_CARRIER)
			ro[V17RX_OBJ_RESULT_B1] |= V17RX_FLAG_ERROR;
		else if (v == N_DEFAULT_KEEPS_DATA)
			ro[V17RX_OBJ_RESULT_B1] = (unsigned char)
				((ro[V17RX_OBJ_RESULT_B1] | V17RX_FLAG_ERROR)
				 & ~V17RX_FLAG_CARRIER);
		else
			ro[V17RX_OBJ_RESULT_B1] = (unsigned char)
				((ro[V17RX_OBJ_RESULT_B1] | V17RX_FLAG_ERROR)
				 & ~(V17RX_FLAG_CARRIER | V17RX_FLAG_DATA));
		if (v == N_DEFAULT_INSTALLS)
			put_slot(f, 8 /* Error */, 1);
		break;
	}
}

/* Everything one call made observable, folded into a word. */
static unsigned long
next_mark(struct fix *f)
{
	unsigned long m = 1469598103u;
	int i;

	m = m * 31u + f->robj[V17RX_OBJ_RESULT];
	m = m * 31u + f->robj[V17RX_OBJ_RESULT_B1];
	m = m * 31u + f->robj[V17RX_OBJ_RESULT_B2];
	m = m * 131u + (unsigned short)get_s(f->ctl, V17RXC_STATE);
	m = m * 131u + get_us(f->ctl, V17RXC_COUNTDOWN);
	m = m * 131u + (unsigned long)(unsigned int)
			get_i(f->ctl, V17RXC_INT_0008);
	m = m * 131u + (unsigned long)(unsigned int)
			get_i(f->rxs, V17RXS_RATE);
	m = m * 131u + (unsigned short)get_s(f->rxs, V17RXS_SHORT_01F8);
	m = m * 131u + (unsigned long)(unsigned int)
			(size_t)(const void *)fix_agc(f)->cfg.alpha;
	m = m * 131u + (unsigned long)(unsigned int)
			(size_t)(const void *)fix_agc(f)->cfg.beta;
	m = m * 131u + (unsigned long)(unsigned int)fix_agc(f)->freeze;
	m = m * 31u + (unsigned long)(slot_index(
			get_ptr(f->ctl, V17RXC_PROCESS), 1) + 2);
	for (i = 0; i < V17_COEF_N; i++) {
		m = m * 31u + (unsigned short)f->save0[i];
		m = m * 31u + (unsigned short)f->save1[i];
	}
	m = m * 31u + (unsigned short)f->ratesave[0];
	return m;
}

static void
run_next_one(unsigned seed, const struct next_setup *u, unsigned level,
	     long tag)
{
	unsigned long marka, markc;
	int d;

	next_build(&ma, seed, u, 1);
	next_build(&mb, seed, u, 0);

	debug_begin(level);
	ref_RxNextStateV17(ma.robj);
	RxNextStateV17(mb.robj);
	debug_compare(tag);
	debug_end();

	compare_all(&ma, &mb, tag, 0);
	compare_slot(&ma, &mb, tag);

	/* Which arm actually ran, from the blob and not from the sweep. */
	if (u->state < 0 || u->state > V17RX_STATE_IDLE)
		next_arm[7]++;
	else
		next_arm[u->state]++;

	if (u->state == V17RX_STATE_EPOCH_DET) {
		if (u->gate != 0)
			next_restored++;
		else
			next_notrestored++;

		/*
		 * D955: the two coefficient pointers are SUBSCRIPTS, so they
		 * are checked as addresses and as the values they now name.
		 */
		diff_eq_int("at %ld: alpha stepped one short",
			    (long)(fix_agc(&ma)->cfg.alpha
				   - ref_AGCv17_CFG.alpha), 1, tag);
		diff_eq_int("at %ld: beta stepped one short",
			    (long)(fix_agc(&ma)->cfg.beta
				   - ref_AGCv17_CFG.beta), 1, tag);
		diff_eq_int("at %ld: alpha now names the tracking weight",
			    (long)fix_agc(&ma)->cfg.alpha[0], 0x7333, tag);
		diff_eq_int("at %ld: beta now names the tracking weight",
			    (long)fix_agc(&ma)->cfg.beta[0], 0x0ccd, tag);
		diff_eq_int("at %ld: ours stepped alpha the same way",
			    (long)(fix_agc(&mb)->cfg.alpha
				   - ref_AGCv17_CFG.alpha), 1, tag);
		diff_eq_int("at %ld: ours stepped beta the same way",
			    (long)(fix_agc(&mb)->cfg.beta
				   - ref_AGCv17_CFG.beta), 1, tag);
		agc_checked++;
	}
	if (u->state == V17RX_STATE_PROTOCOL) {
		if (u->gate != 0)
			next_scram_arm++;
		else
			next_bridge_arm++;
	}

	marka = next_mark(&ma);

	for (d = 1; d < (int)N_MAX; d++) {
		next_build(&mc, seed, u, 1);
		drive_next(&mc, d);
		markc = next_mark(&mc);
		if (markc != marka)
			next_sep[d]++;
	}

	/* The faithful model, which must agree with the blob exactly. */
	next_build(&mc, seed, u, 1);
	drive_next(&mc, N_NONE);
	diff_eq_int("RxNextStateV17 model (%ld)",
		    next_mark(&mc) == marka, 1, tag);
}

static int
run_next(void)
{
	static const short states[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 100, -1, -2, (short)0x8000,
		(short)0x0100
	};
	static const int gates[] = { 0, 1, 0x00010000, -5 };
	static const short rates[] = { 0, 1, 2, 3, 7 };
	static const unsigned levels[] = { 0, 2 };
	struct next_setup u;
	long tag = 0;
	int s, g, r, l;

	diff_begin("RxNextStateV17");

	for (s = 0; s < (int)(sizeof(states) / sizeof(states[0])); s++)
	for (g = 0; g < (int)(sizeof(gates) / sizeof(gates[0])); g++)
	for (r = 0; r < (int)(sizeof(rates) / sizeof(rates[0])); r++)
	for (l = 0; l < (int)(sizeof(levels) / sizeof(levels[0])); l++) {
		u.state = states[s];
		u.gate = gates[g];
		u.rate = rates[r];
		run_next_one(spread(0x17c50000u, tag), &u, levels[l], tag);
		tag++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* The demodulator fixture the five handlers need                         */

static short dem_icoff[DEM_TAPS], dem_qcoff[DEM_TAPS];
static short dem_clk[DEM_CLK], dem_k1[3], dem_k2[3];
static struct fpm_fse_cfg dem_fse_cfg;
static short dem_slice_perr, dem_slice_mag;

static short dem_in[DEM_BUF], dem_work[DEM_BUF];
static short out_a[DEM_BUF], out_b[DEM_BUF];

static unsigned short
dem_slicer(struct fpm_fse *state, short *angle, short *mag)
{
	short a = *angle;

	(void)state;
	*angle = (short)(a - dem_slice_perr);
	*mag = dem_slice_mag;
	return (unsigned short)a;
}

static void
dem_tables(void)
{
	int i;

	for (i = 0; i < DEM_TAPS; i++) {
		int v = ((i * 7919 + 1301) & 0x3fff) - 8192;

		dem_icoff[i] = (short)(v | 1);
		dem_qcoff[i] = (short)(-3 * v + 5 * i + 7);
	}
	for (i = 0; i < DEM_CLK; i++)
		dem_clk[i] = (short)(i * 4096 + 137);
	dem_k1[0] = 602; dem_k1[1] = 3050; dem_k1[2] = 766;
	dem_k2[0] = 0;   dem_k2[1] = 18;   dem_k2[2] = 1;
	dem_slice_perr = 311;
	dem_slice_mag = 1777;

	memset(&dem_fse_cfg, 0, sizeof(dem_fse_cfg));
	dem_fse_cfg.block = 2048;
	dem_fse_cfg.interp = 3;
	dem_fse_cfg.icoff = dem_icoff;
	dem_fse_cfg.qcoff = dem_qcoff;
	dem_fse_cfg.taps = DEM_TAPS;
	dem_fse_cfg.mu[0] = 2620;
	dem_fse_cfg.mu[1] = 393;
	dem_fse_cfg.mu[2] = 97;
	dem_fse_cfg.clk = dem_clk;
	dem_fse_cfg.clk_mod = DEM_CLK;
	dem_fse_cfg.clk_inc = 4096;
	dem_fse_cfg.train_sym = 4;
	dem_fse_cfg.err_hi = 6536;
	dem_fse_cfg.err_lo = 1638;
	dem_fse_cfg.pll_k1 = dem_k1;
	dem_fse_cfg.pll_k2 = dem_k2;
	dem_fse_cfg.decision = dem_slicer;
}

/*
 * A DETERMINISTIC INPUT BLOCK.  What these handlers do with the samples is
 * hand them to `DemodDataV17`, which `t_v17fax.c` already proves identical;
 * what matters here is that both sides see the same bytes and that the AGC
 * sees enough energy for `signal` to come up.
 *
 * THE SINE ARM EXISTS FOR ONE REASON AND IT IS NOT REALISM.  With
 * `V17RXC_STATE` at `V17RX_STATE_START`, `DemodDataV17` runs its tone pre-pass
 * and ABANDONS the call -- returning before the resampler, the recoverer and
 * the equaliser -- if `FPM_MTD_detect` answers anything but ABSENT.  That is
 * the only path on which `struct fpm_fse::mse` survives a call, and
 * `V17RXS_DEC_ERROR` *is* `mse`, so it is the only way to drive
 * `RxHdxIdleV17`'s 0x1fff threshold and `GetSNRV17`'s 8 at a planted value
 * with the carrier still up: the gain control has already run by then and sets
 * `signal` from the block's own energy.  1200 Hz at 12000 is PRESENT to
 * `MTDv22_CFG`'s bank, which `t_v17fax.c` measured (2200 is ABSENT, silence is
 * NOSIGNAL).  Findings F134 and D955 are why this is spelled out rather than
 * left as a magic frequency.
 */
static int fill_hz;

static void
fill_block(int n, int amp, unsigned phase)
{
	int i;

	if (fill_hz > 0) {
		/*
		 * A pure function of (n, amp, phase), so every replay of a
		 * fixture -- the blob's, ours, and each of the model variants
		 * -- sees the identical samples with no generator state to
		 * re-arm and no way for a missed reset to look like a defect.
		 */
		double w = 2.0 * 3.14159265358979323846
			   * (double)fill_hz / 8000.0;

		for (i = 0; i < n; i++)
			dem_in[i] = (short)((double)amp
					    * sin(w * (double)((unsigned)i
							       + phase)));
		for (; i < DEM_BUF; i++)
			dem_in[i] = 0;
		return;
	}

	for (i = 0; i < n; i++) {
		unsigned t = (unsigned)i + phase;
		int s = (int)((t / 3u) & 3u);
		int v = (s == 0 || s == 1) ? amp : -amp;

		dem_in[i] = (short)(v + (int)((t * 37u) & 0x1f) - 16);
	}
	for (; i < DEM_BUF; i++)
		dem_in[i] = 0;
}


struct hdx_setup {
	short	gate_18;	/* V17RXC_STATE; non-zero skips the pre-pass */
	int	carrier;	/* V17RXS_INT_0120                           */
	short	dec_error;	/* V17RXS_DEC_ERROR                          */
	short	countdown;
	int	epoch;
	int	amp;
	int	gate10;		/* V17RXC_INT_0010; 0 takes the BRIDGE arm   */
	int	hz;		/* >0 feeds a sine; see fill_block           */
};

static const int dem_enable[2][3] = {
	{ 0x0e, 0x33, 0x54 },
	{ 0x33, 0x54, 0x0f }
};

static void
hdx_build(struct fix *f, unsigned seed, const struct hdx_setup *u, int blob,
	  int enables)
{
	struct fpm_sdm_cfg cfg;

	fixture(f, seed);

	put_ptr(f->rxs, V17RXS_BUF_MRF, f->mrfbuf);
	put_ptr(f->rxs, V17RXS_BUF_SRE, f->srebuf);
	put_ptr(f->ctl, V17RXC_SCRATCH, f->detbuf);
	memset(f->mrfbuf, 0, sizeof(f->mrfbuf));
	memset(f->srebuf, 0, sizeof(f->srebuf));
	memset(f->detbuf, 0, sizeof(f->detbuf));

	ref_FPM_AGC_init(fix_agc(f), &ref_AGCv17_CFG, 1);
	ref_FPM_MRF_init((struct fpm_mrf *)(void *)(f->rxs + V17RXS_MRF),
			 &MRFv32_CFG, 1);
	ref_FPM_SRE_init((struct fpm_sre *)(void *)(f->rxs + V17RXS_SRE),
			 &SREv32_CFG, 1);
	ref_FPM_FSE_init((struct fpm_fse *)(void *)(f->rxs + V17RXS_FSE),
			 &dem_fse_cfg, 1);

	put_ptr(f->ctl, V17RXC_MTD, ref_FPM_MTD_create(0, &ref_MTDv22_CFG));
	put_ptr(f->ctl, V17RXC_TONE, ref_FPM_TONE_create(0, 0));

	memset(&f->mtd, 0, sizeof(f->mtd));
	memset(f->mtd_acc, 0, sizeof(f->mtd_acc));
	memset(f->buf2, 0, sizeof(f->buf2));
	f->mtd.acc = f->mtd_acc;
	ref_FPM_MTD_create(&f->mtd, &ref_MTDv22_CFG);
	put_ptr(f->ctl, V17RXC_MTD2, &f->mtd);
	put_ptr(f->ctl, V17RXC_BUF2, f->buf2);

	memset((void *)(f->ctl + V17RXC_AGC), 0, sizeof(struct fpm_agc));
	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->ctl + V17RXC_AGC),
			 &ref_AGCv17_CFG, 1);

	/* V.17's own polynomial; see `t_v17fax.c`. */
	cfg.nbits = 8;
	cfg.tap1 = 0x12;
	cfg.tap2 = 0x17;
	ref_SDM_init((struct fpm_sdm *)(void *)(f->rxs + V17RXS_SDM), &cfg);

	put_s(f->ctl, V17RXC_STATE, u->gate_18);
	put_s(f->ctl, V17RXC_COUNTDOWN, u->countdown);
	put_i(f->ctl, V17RXC_INT_0008, 0);
	put_i(f->ctl, V17RXC_INT_0010, u->gate10);
	put_s(f->ctl, V17RXC_SHORT_0020, 0);
	put_s(f->ctl, V17RXC_SHORT_002E, 0);
	put_s(f->ctl, V17RXC_OFFBAND, 0);
	put_slot(f, 0, blob);

	put_i(f->rxs, V17RXS_INT_0004, dem_enable[enables][0]);
	put_i(f->rxs, V17RXS_INT_0008, dem_enable[enables][1]);
	put_i(f->rxs, V17RXS_INT_0010, dem_enable[enables][2]);
	put_i(f->rxs, V17RXS_EPOCH, u->epoch);
	put_s(f->rxs, V17RXS_SHORT_0094, 1000);
	put_i(f->rxs, V17RXS_INT_0120, u->carrier);
	put_s(f->rxs, V17RXS_AGC_SIGNAL, 1);
	put_s(f->rxs, V17RXS_AGC_SIGNAL + 2, 0x1234);
	put_s(f->rxs, V17RXS_SHORT_4FB4, 0);
	put_s(f->rxs, V17RXS_DEC_ERROR, u->dec_error);
	put_s(f->rxs, V17RXS_QAVG, 0);
	put_s(f->rxs, V17RXS_QCOUNT, 0);
	put_s(f->rxs, V17RXS_SHORT_4FB0, 0x0100);
	put_s(f->rxs, V17RXS_SHORT_4FB2, 0);
	put_i(f->rxs, V17RXS_RATE, 0x0777);
	put_s(f->rxs, V17RXS_USHORT_018C, 0x0040);
}

static void
hdx_free(struct fix *f)
{
	ref_FPM_MRF_free((struct fpm_mrf *)(void *)(f->rxs + V17RXS_MRF));
	ref_FPM_SRE_free((struct fpm_sre *)(void *)(f->rxs + V17RXS_SRE));
	ref_FPM_FSE_free((struct fpm_fse *)(void *)(f->rxs + V17RXS_FSE));
	ref_FPM_MTD_delete((struct fpm_mtd *)get_ptr(f->ctl, V17RXC_MTD));
	ref_FPM_TONE_delete(get_ptr(f->ctl, V17RXC_TONE));
}

static unsigned long
hdx_mark(unsigned long m, struct fix *f, short r, unsigned short count,
	 const short *out)
{
	int i;

	m = m * 1000003u + (unsigned short)r;
	m = m * 31u + count;
	m = m * 31u + f->robj[V17RX_OBJ_RESULT];
	m = m * 31u + f->robj[V17RX_OBJ_RESULT_B1];
	m = m * 31u + f->robj[V17RX_OBJ_RESULT_B2];
	m = m * 131u + (unsigned short)get_s(f->ctl, V17RXC_STATE);
	m = m * 131u + get_us(f->ctl, V17RXC_COUNTDOWN);
	m = m * 31u + (unsigned long)(slot_index(
			get_ptr(f->ctl, V17RXC_PROCESS), 1) + 2);
	m = m * 131u + (unsigned short)get_s(f->rxs, V17RXS_DEC_ERROR);
	for (i = 0; i < 64; i++)
		m = m * 31u + (unsigned short)out[i];
	return m;
}

static void
hdx_compare(struct fix *a, struct fix *b, long id, short ra, short rb,
	    unsigned short ca, unsigned short cb, const char *who)
{
	(void)who;
	diff_eq_int("at %ld: the handler returned", (long)rb, (long)ra, id);
	diff_eq_int("at %ld: the handler left *count", (long)cb, (long)ca, id);
	diff_eq_int("at %ld: the handler's output buffer",
		    first_diff((const unsigned char *)out_b,
			       (const unsigned char *)out_a, DEM_BUF * 2),
		    -1, id);
	compare_all(a, b, id, 1);
	compare_slot(a, b, id);
}

/* --------------------------------------------------------------------- */
/* Layer B -- the three training handlers                                 */

/*
 * `which` is 5 for Scram, 4 for Bridge and 3 for Prtcol -- the same indices
 * `ours_fn` and `ref_fn` use, so one driver covers all three.
 */
static short
call_train(struct fix *f, int which, int blob, short *in, short *out,
	   unsigned short *count)
{
	if (blob)
		return which == 5 ? ref_RxHdxScramV17(f->robj, in, out, count)
		     : which == 4 ? ref_RxHdxBridgeV17(f->robj, in, out, count)
				  : ref_RxHdxPrtcolV17(f->robj, in, out, count);
	return which == 5 ? RxHdxScramV17(f->robj, in, out, count)
	     : which == 4 ? RxHdxBridgeV17(f->robj, in, out, count)
			  : RxHdxPrtcolV17(f->robj, in, out, count);
}

/* The object's own sequence, with one reading changed. */
static short
drive_train(struct fix *f, int which, short *in, short *out,
	    unsigned short *count, int v)
{
	unsigned char *ro = f->robj;
	unsigned char *ct = f->ctl;
	unsigned short n;
	short left;

	n = ref_DemodDataV17(ro, in, (unsigned short *)(void *)out, *count);
	ref_DescrambleDataV17(ro, (unsigned short *)(void *)out, n);
	*count = 0;

	if (ref_CarrierDetectV17(ro) == 0) {
		put_slot(f, (v == H_ERR_WRONG_HANDLER) ? 7 : 8, 1);
		if (v != H_ERR_NO_STATE)
			put_s(ct, V17RXC_STATE, V17RX_STATE_ERROR);
		ro[V17RX_OBJ_RESULT] = V17RX_STATUS_ERROR;
		if (v == H_ERR_KEEPS_CARRIER)
			ro[V17RX_OBJ_RESULT_B1] |= V17RX_FLAG_ERROR;
		else
			ro[V17RX_OBJ_RESULT_B1] = (unsigned char)
				((ro[V17RX_OBJ_RESULT_B1] | V17RX_FLAG_ERROR)
				 & ~V17RX_FLAG_CARRIER);
		return (v == H_RETURN_N_ALWAYS) ? (short)n : 0;
	}

	ro[V17RX_OBJ_RESULT_B1] |= V17RX_FLAG_CARRIER;
	ro[V17RX_OBJ_RESULT] = V17RX_STATUS_CARRIER;

	left = (short)(get_us(ct, V17RXC_COUNTDOWN) - 1);
	if (v != H_COUNT_NOT_STORED)
		put_s(ct, V17RXC_COUNTDOWN, left);
	if (v == H_COUNT_UNSIGNED ? (unsigned short)left > 0u
	  : v == H_COUNT_GE       ? left >= 0
				  : left > 0)
		return (v == H_RETURN_N_ALWAYS) ? (short)n : 0;

	if (which == 5 ? v != H_LADDER_NOWHERE : v == H_LADDER_EVERYWHERE) {
		switch (get_us(ct, V17RXC_RATE_CODE)) {
		case V17RX_RATE_7200:
			ro[V17RX_OBJ_RESULT] = V17RX_STATUS_RATE_7200;
			break;
		case V17RX_RATE_9600:
			ro[V17RX_OBJ_RESULT] = V17RX_STATUS_RATE_9600;
			break;
		case V17RX_RATE_12000:
			ro[V17RX_OBJ_RESULT] = V17RX_STATUS_RATE_12000;
			break;
		default:
			ro[V17RX_OBJ_RESULT] = V17RX_STATUS_RATE_14400;
			break;
		}
	}

	if (v == H_SNR_CLEARED)
		ro[V17RX_OBJ_RESULT_B1] &=
			(unsigned char)~(unsigned char)V17RX_FLAG_LOW_SNR;
	if (v == H_SNR_STRICT ? ref_GetSNRV17(ro) < V17RX_SNR_THRESHOLD
			      : ref_GetSNRV17(ro) <= V17RX_SNR_THRESHOLD)
		ro[V17RX_OBJ_RESULT_B1] |= (unsigned char)V17RX_FLAG_LOW_SNR;

	ref_RxNextStateV17(ro);

	return (short)n;
}

static void
run_train_one(unsigned seed, const struct hdx_setup *u, int which, short rate,
	      unsigned short count, unsigned level, long tag)
{
	unsigned long marka = 0, markc;
	unsigned char status_first = 0;
	int moved_first = 0;
	int blk, d, i;

	fill_hz = u->hz;
	hdx_build(&ma, seed, u, 1, (int)(tag & 1));
	hdx_build(&mb, seed, u, 0, (int)(tag & 1));
	put_s(ma.ctl, V17RXC_RATE_CODE, rate);
	put_s(mb.ctl, V17RXC_RATE_CODE, rate);

	debug_begin(level);
	for (blk = 0; blk < 4; blk++) {
		unsigned short ca = count, cb = count;
		short ra, rb;
		short st_before;
		long id = tag * 10 + blk;

		fill_block((int)count, u->amp, (unsigned)blk * 7u);
		for (i = 0; i < DEM_BUF; i++)
			out_a[i] = out_b[i] = (short)0xbeef;

		st_before = get_s(ma.ctl, V17RXC_STATE);

		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		ra = call_train(&ma, which, 1, dem_work, out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		rb = call_train(&mb, which, 0, dem_work, out_b, &cb);

		hdx_compare(&ma, &mb, id, ra, rb, ca, cb, "train");

		if (ma.robj[V17RX_OBJ_RESULT] == V17RX_STATUS_ERROR) {
			train_lost++;
		} else {
			train_carrier++;
			if (get_s(ma.ctl, V17RXC_STATE) != st_before)
				train_expired++;
			else
				train_held++;
		}
		if ((ma.robj[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_LOW_SNR) != 0)
			snr_low_seen++;
		else
			snr_high_seen++;
		/*
		 * ONLY BLOCK 0, and that is not laziness.  Once the machine
		 * has moved on, a later block can reach `RxNextStateV17`'s
		 * IDLE arm, which writes the SAME rate ladder -- so the status
		 * byte after block 3 says nothing about which handler wrote it.
		 */
		if (blk == 0) {
			status_first = ma.robj[V17RX_OBJ_RESULT];
			moved_first = get_s(ma.ctl, V17RXC_STATE) != st_before
				      && ma.robj[V17RX_OBJ_RESULT]
					 != V17RX_STATUS_ERROR;
		}

		marka = hdx_mark(marka, &ma, ra, ca, out_a);
	}
	debug_compare(tag);
	debug_end();

	/*
	 * THE CHECK THAT TELLS THE THREE APART.  On a run where the countdown
	 * expired with the carrier up, Scram must have written the rate's own
	 * status and its two twins must have left `V17RX_STATUS_CARRIER`
	 * there.  `train_ladder_sep` counts the runs where the two answers
	 * actually differed, which is what says this is a measurement.
	 */
	if (moved_first) {
		if (which == 5) {
			diff_eq_int("at %ld: Scram reported the rate",
				    (long)status_first,
				    rate == V17RX_RATE_7200
					? V17RX_STATUS_RATE_7200
				  : rate == V17RX_RATE_9600
					? V17RX_STATUS_RATE_9600
				  : rate == V17RX_RATE_12000
					? V17RX_STATUS_RATE_12000
					: V17RX_STATUS_RATE_14400, tag);
			train_rate_seen[rate & 3]++;
			train_ladder_sep++;
		} else {
			diff_eq_int("at %ld: a twin left the carrier status",
				    (long)status_first, V17RX_STATUS_CARRIER,
				    tag);
		}
	}

	hdx_free(&ma);
	hdx_free(&mb);

	for (d = 1; d < (int)H_MAX; d++) {
		hdx_build(&mc, seed, u, 1, (int)(tag & 1));
		put_s(mc.ctl, V17RXC_RATE_CODE, rate);
		markc = 0;
		for (blk = 0; blk < 4; blk++) {
			unsigned short cc = count;
			short rc;

			fill_block((int)count, u->amp, (unsigned)blk * 7u);
			for (i = 0; i < DEM_BUF; i++) {
				dem_work[i] = dem_in[i];
				out_b[i] = (short)0xbeef;
			}
			rc = drive_train(&mc, which, dem_work, out_b, &cc, d);
			markc = hdx_mark(markc, &mc, rc, cc, out_b);
		}
		if (markc != marka)
			hdx_sep[d]++;
		hdx_free(&mc);
	}

	/* The faithful model, which must agree with the blob exactly. */
	hdx_build(&mc, seed, u, 1, (int)(tag & 1));
	put_s(mc.ctl, V17RXC_RATE_CODE, rate);
	markc = 0;
	for (blk = 0; blk < 4; blk++) {
		unsigned short cc = count;
		short rc;

		fill_block((int)count, u->amp, (unsigned)blk * 7u);
		for (i = 0; i < DEM_BUF; i++) {
			dem_work[i] = dem_in[i];
			out_b[i] = (short)0xbeef;
		}
		rc = drive_train(&mc, which, dem_work, out_b, &cc, H_NONE);
		markc = hdx_mark(markc, &mc, rc, cc, out_b);
	}
	diff_eq_int("training-handler model (%ld)", markc == marka, 1, tag);
	hdx_free(&mc);
}

static int
run_train(void)
{
	static const struct hdx_setup setups[] = {
	    /* gate_18         carrier dec_err countdown ep   amp gate10  hz */
	    { V17RX_STATE_SCRAM,     1,      4,        1,  1, 9000,   1,   0 },
	    { V17RX_STATE_SCRAM,     1,      4,        3,  1, 9000,   1,   0 },
	    { V17RX_STATE_SCRAM,     1,     14,        1,  1, 9000,   1,   0 },
	    { V17RX_STATE_SCRAM,     0,      4,        1,  1, 9000,   1,   0 },
	    { V17RX_STATE_SCRAM,     1,      4, (short)0x8000, 1, 9000, 1,  0 },
	    { V17RX_STATE_BRIDGE,    1,      4,        1,  1,  600,   1,   0 },
	    { V17RX_STATE_PROTOCOL,  1, 0x4000,        1,  1, 9000,   1,   0 },
	    /*
	     * THE THREE ROWS THAT DRIVE THE SNR THRESHOLD, and they are the
	     * tone-abandon fixture: `V17RXC_STATE` at START plus a tone
	     * `MTDv22_CFG` answers PRESENT to, so `DemodDataV17` returns before
	     * the equaliser and leaves `V17RXS_DEC_ERROR` -- which IS
	     * `struct fpm_fse::mse` -- holding what was planted.  The gain
	     * control has already run by then, so the carrier is still up.
	     * `GetSNRV17` is 13 - error, so 4 gives 9, 5 gives exactly the
	     * threshold and 14 gives -1.  See `fill_block`.
	     */
	    { V17RX_STATE_START,     1,      4,        1,  1,12000,   1,1200 },
	    { V17RX_STATE_START,     1,      5,        1,  1,12000,   1,1200 },
	    { V17RX_STATE_START,     1,     14,        1,  1,12000,   1,1200 }
	};
	static const short rates[] = { 0, 1, 2, 3 };
	/*
	 * COUNT 2 IS THERE ON PURPOSE.  `V17RXS_DEC_ERROR` is 0x1c2, which is
	 * `struct fpm_fse::mse` -- the equaliser owns it and rewrites it once
	 * per symbol it produces.  A block of two samples produces none, so
	 * the planted error survives to `GetSNRV17` and the threshold's exact
	 * edge is actually driven; the larger counts are the realistic case
	 * and are classified from what the flag byte HOLDS afterwards.
	 */
	static const unsigned short counts[] = { 0, 40, 160 };
	static const unsigned levels[] = { 0, 2 };
	int s, w, r, c, l;
	long tag = 0;

	diff_begin("RxHdxScram / RxHdxBridge / RxHdxPrtcolV17");

	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++)
	for (w = 3; w <= 5; w++)
	for (r = 0; r < (int)(sizeof(rates) / sizeof(rates[0])); r++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++)
	for (l = 0; l < (int)(sizeof(levels) / sizeof(levels[0])); l++) {
		run_train_one(spread(0x17d10000u, tag), &setups[s], w,
			      rates[r], counts[c], levels[l], tag);
		tag++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* Layer B -- RxHdxEpochDetV17                                            */

static short
drive_epoch(struct fix *f, short *in, short *out, unsigned short *count, int v)
{
	unsigned char *ro = f->robj;
	unsigned char *ct = f->ctl;
	unsigned short n;
	short left;
	int go;

	n = ref_DemodDataV17(ro, in, (unsigned short *)(void *)out, *count);
	if (v == P_DESCRAMBLES)
		ref_DescrambleDataV17(ro, (unsigned short *)(void *)out, n);
	*count = 0;

	if (ref_CarrierDetectV17(ro) == 0) {
		put_slot(f, 8, 1);
		put_s(ct, V17RXC_STATE, V17RX_STATE_ERROR);
		ro[V17RX_OBJ_RESULT] = V17RX_STATUS_ERROR;
		ro[V17RX_OBJ_RESULT_B1] = (unsigned char)
			((ro[V17RX_OBJ_RESULT_B1] | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
		return 0;
	}

	ro[V17RX_OBJ_RESULT_B1] |= V17RX_FLAG_CARRIER;
	ro[V17RX_OBJ_RESULT] = V17RX_STATUS_CARRIER;

	left = (short)(get_us(ct, V17RXC_COUNTDOWN) - 1);
	put_s(ct, V17RXC_COUNTDOWN, left);

	if (v == P_BOTH_REQUIRED) {
		go = left <= 0 && ref_EpochDetectV17(ro) != 0;
	} else if (v == P_EPOCH_INVERTED) {
		go = left <= 0 || ref_EpochDetectV17(ro) == 0;
	} else {
		go = left <= 0 || ref_EpochDetectV17(ro) != 0;
	}
	if (go)
		ref_RxNextStateV17(ro);

	return 0;
}

static void
run_epoch_one(unsigned seed, const struct hdx_setup *u, unsigned short count,
	      unsigned level, long tag)
{
	unsigned long marka = 0, markc;
	int blk, d, i;

	fill_hz = u->hz;
	hdx_build(&ma, seed, u, 1, (int)(tag & 1));
	hdx_build(&mb, seed, u, 0, (int)(tag & 1));

	debug_begin(level);
	for (blk = 0; blk < 3; blk++) {
		unsigned short ca = count, cb = count;
		unsigned short cd_before;
		short ra, rb;
		short st_before;
		long id = 200000 + tag * 10 + blk;

		fill_block((int)count, u->amp, (unsigned)blk * 11u);
		for (i = 0; i < DEM_BUF; i++)
			out_a[i] = out_b[i] = (short)0xbeef;

		st_before = get_s(ma.ctl, V17RXC_STATE);
		cd_before = get_us(ma.ctl, V17RXC_COUNTDOWN);

		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		ra = ref_RxHdxEpochDetV17(ma.robj, dem_work, out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		rb = RxHdxEpochDetV17(mb.robj, dem_work, out_b, &cb);

		hdx_compare(&ma, &mb, id, ra, rb, ca, cb, "epoch");

		/*
		 * Classified from the countdown the blob actually held, so
		 * "expired" means the short-circuit arm ran and "found" means
		 * `EpochDetectV17` was consulted and said yes.
		 */
		if (ma.robj[V17RX_OBJ_RESULT] == V17RX_STATUS_ERROR)
			epoch_lost++;
		else if (get_s(ma.ctl, V17RXC_STATE) != st_before) {
			if ((short)(cd_before - 1) <= 0)
				epoch_expired++;
			else
				epoch_found++;
		} else
			epoch_missed++;

		marka = hdx_mark(marka, &ma, ra, ca, out_a);
	}
	debug_compare(200000 + tag);
	debug_end();

	hdx_free(&ma);
	hdx_free(&mb);

	for (d = 1; d < (int)P_MAX; d++) {
		hdx_build(&mc, seed, u, 1, (int)(tag & 1));
		markc = 0;
		for (blk = 0; blk < 3; blk++) {
			unsigned short cc = count;
			short rc;

			fill_block((int)count, u->amp, (unsigned)blk * 11u);
			for (i = 0; i < DEM_BUF; i++) {
				dem_work[i] = dem_in[i];
				out_b[i] = (short)0xbeef;
			}
			rc = drive_epoch(&mc, dem_work, out_b, &cc, d);
			markc = hdx_mark(markc, &mc, rc, cc, out_b);
		}
		if (markc != marka)
			epoch_sep[d]++;
		hdx_free(&mc);
	}

	hdx_build(&mc, seed, u, 1, (int)(tag & 1));
	markc = 0;
	for (blk = 0; blk < 3; blk++) {
		unsigned short cc = count;
		short rc;

		fill_block((int)count, u->amp, (unsigned)blk * 11u);
		for (i = 0; i < DEM_BUF; i++) {
			dem_work[i] = dem_in[i];
			out_b[i] = (short)0xbeef;
		}
		rc = drive_epoch(&mc, dem_work, out_b, &cc, P_NONE);
		markc = hdx_mark(markc, &mc, rc, cc, out_b);
	}
	diff_eq_int("RxHdxEpochDetV17 model (%ld)", markc == marka, 1, tag);
	hdx_free(&mc);
}

static int
run_epoch(void)
{
	/*
	 * THE THIRD ROW IS THE ONE THAT MATTERS: the countdown expires on the
	 * first block with the epoch flag CLEAR, so an `||` that evaluated its
	 * right-hand side would find no epoch and stay put where the object
	 * advances.  It is also D1216's reason for one visit per fixture --
	 * the arm the machine lands in steps the AGC tables.
	 */
	static const struct hdx_setup setups[] = {
	    /* gate_18          carrier dec_err countdown ep   amp gate10 hz */
	    { V17RX_STATE_EPOCH_DET, 1,      4,        5,  1, 9000,   1,  0 },
	    { V17RX_STATE_EPOCH_DET, 1,      4,        5,  0, 9000,   1,  0 },
	    { V17RX_STATE_EPOCH_DET, 1,      4,        1,  0, 9000,   1,  0 },
	    { V17RX_STATE_EPOCH_DET, 1,      4,        2,  0, 9000,   1,  0 },
	    { V17RX_STATE_EPOCH_DET, 0,      4,        5,  1, 9000,   1,  0 },
	    { V17RX_STATE_EPOCH_DET, 1, 0x4000,        5,  1,  600,   1,  0 }
	};
	static const unsigned short counts[] = { 0, 40, 160 };
	static const unsigned levels[] = { 0, 2 };
	int s, c, l;
	long tag = 0;

	diff_begin("RxHdxEpochDetV17");

	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++)
	for (l = 0; l < (int)(sizeof(levels) / sizeof(levels[0])); l++) {
		run_epoch_one(spread(0x17e20000u, tag), &setups[s], counts[c],
			      levels[l], tag);
		tag++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* Layer B -- RxHdxIdleV17 and RxHdxStartV17                              */

static short
drive_idle(struct fix *f, short *in, short *out, unsigned short *count, int v)
{
	unsigned char *ro = f->robj;
	short err;
	int small;

	ref_DemodDataV17(ro, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	if (v != I_NO_CARRIER_CLEAR)
		ro[V17RX_OBJ_RESULT_B1] &=
			(unsigned char)~(unsigned char)V17RX_FLAG_CARRIER;
	if (v != I_STATUS_MISSING)
		ro[V17RX_OBJ_RESULT] = V17RX_STATUS_IDLE;

	if (ref_CarrierDetectV17(ro) != 0)
		ro[V17RX_OBJ_RESULT_B1] |= (unsigned char)V17RX_FLAG_CARRIER;

	err = get_s((const unsigned char *)get_ptr(ro, V17RX_OBJ_STATE),
		    V17RXS_DEC_ERROR);
	small = (v == I_SMALL_STRICT)
			? err < V17RXS_DEC_ERROR_SMALL
	      : (v == I_SMALL_UNSIGNED)
			? (unsigned short)err <= (unsigned short)
						 V17RXS_DEC_ERROR_SMALL
			: err <= V17RXS_DEC_ERROR_SMALL;

	if ((ro[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_CARRIER) != 0 && small)
		ref_RxNextStateV17(ro);

	return 0;
}

static void
run_idle_one(unsigned seed, const struct hdx_setup *u, unsigned short count,
	     unsigned level, long tag)
{
	unsigned long marka = 0, markc;
	int blk, d, i;

	fill_hz = u->hz;
	hdx_build(&ma, seed, u, 1, (int)(tag & 1));
	hdx_build(&mb, seed, u, 0, (int)(tag & 1));

	debug_begin(level);
	for (blk = 0; blk < 2; blk++) {
		unsigned short ca = count, cb = count;
		short ra, rb;
		long id = 300000 + tag * 10 + blk;

		fill_block((int)count, u->amp, (unsigned)blk * 13u);
		for (i = 0; i < DEM_BUF; i++)
			out_a[i] = out_b[i] = (short)0xbeef;

		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		ra = ref_RxHdxIdleV17(ma.robj, dem_work, out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		rb = RxHdxIdleV17(mb.robj, dem_work, out_b, &cb);

		hdx_compare(&ma, &mb, id, ra, rb, ca, cb, "idle");

		if ((ma.robj[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_CARRIER) != 0)
			idle_carrier++;
		else
			idle_nocarrier++;
		/*
		 * From the field as the BLOB left it, not from what was
		 * planted: the equaliser owns +0x1c2 and overwrites it
		 * whenever the block was long enough to produce symbols.
		 */
		{
			short err = get_s(ma.rxs, V17RXS_DEC_ERROR);

			if (err < 0)
				idle_negative++;
			else if (err <= V17RXS_DEC_ERROR_SMALL)
				idle_small++;
			else
				idle_big++;
		}

		marka = hdx_mark(marka, &ma, ra, ca, out_a);
	}
	debug_compare(300000 + tag);
	debug_end();

	hdx_free(&ma);
	hdx_free(&mb);

	for (d = 1; d < (int)I_MAX; d++) {
		hdx_build(&mc, seed, u, 1, (int)(tag & 1));
		markc = 0;
		for (blk = 0; blk < 2; blk++) {
			unsigned short cc = count;
			short rc;

			fill_block((int)count, u->amp, (unsigned)blk * 13u);
			for (i = 0; i < DEM_BUF; i++) {
				dem_work[i] = dem_in[i];
				out_b[i] = (short)0xbeef;
			}
			rc = drive_idle(&mc, dem_work, out_b, &cc, d);
			markc = hdx_mark(markc, &mc, rc, cc, out_b);
		}
		if (markc != marka)
			idle_sep[d]++;
		hdx_free(&mc);
	}

	hdx_build(&mc, seed, u, 1, (int)(tag & 1));
	markc = 0;
	for (blk = 0; blk < 2; blk++) {
		unsigned short cc = count;
		short rc;

		fill_block((int)count, u->amp, (unsigned)blk * 13u);
		for (i = 0; i < DEM_BUF; i++) {
			dem_work[i] = dem_in[i];
			out_b[i] = (short)0xbeef;
		}
		rc = drive_idle(&mc, dem_work, out_b, &cc, I_NONE);
		markc = hdx_mark(markc, &mc, rc, cc, out_b);
	}
	diff_eq_int("RxHdxIdleV17 model (%ld)", markc == marka, 1, tag);
	hdx_free(&mc);
}

static short
drive_start(struct fix *f, short *in, short *out, unsigned short *count, int v)
{
	unsigned char *ro = f->robj;

	if (v != S_NO_CARRIER_CLEAR)
		ro[V17RX_OBJ_RESULT_B1] &=
			(unsigned char)~(unsigned char)V17RX_FLAG_CARRIER;
	ro[V17RX_OBJ_RESULT] = V17RX_STATUS_START;

	ref_DemodDataV17(ro, in, (unsigned short *)(void *)out, *count);

	if (ref_CarrierDetectV17(ro) != 0) {
		ro[V17RX_OBJ_RESULT_B1] |= (unsigned char)V17RX_FLAG_CARRIER;
		ref_RxNextStateV17(ro);
	} else if (v == S_HAS_ERROR_ARM) {
		put_slot(f, 8, 1);
		put_s(f->ctl, V17RXC_STATE, V17RX_STATE_ERROR);
		ro[V17RX_OBJ_RESULT] = V17RX_STATUS_ERROR;
		ro[V17RX_OBJ_RESULT_B1] |= (unsigned char)V17RX_FLAG_ERROR;
	}

	if (v != S_COUNT_KEPT)
		*count = 0;

	return 0;
}

static void
run_start_one(unsigned seed, const struct hdx_setup *u, unsigned short count,
	      unsigned level, long tag)
{
	unsigned long marka = 0, markc;
	int d, i;
	unsigned short ca = count, cb = count;
	short ra, rb;

	fill_hz = u->hz;
	hdx_build(&ma, seed, u, 1, (int)(tag & 1));
	hdx_build(&mb, seed, u, 0, (int)(tag & 1));

	fill_block((int)count, u->amp, 3u);
	for (i = 0; i < DEM_BUF; i++)
		out_a[i] = out_b[i] = (short)0xbeef;

	debug_begin(level);
	for (i = 0; i < DEM_BUF; i++)
		dem_work[i] = dem_in[i];
	ra = ref_RxHdxStartV17(ma.robj, dem_work, out_a, &ca);
	for (i = 0; i < DEM_BUF; i++)
		dem_work[i] = dem_in[i];
	rb = RxHdxStartV17(mb.robj, dem_work, out_b, &cb);
	debug_compare(400000 + tag);
	debug_end();

	hdx_compare(&ma, &mb, 400000 + tag, ra, rb, ca, cb, "start");

	if ((ma.robj[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_CARRIER) != 0)
		start_carrier++;
	else
		start_nocarrier++;

	marka = hdx_mark(0, &ma, ra, ca, out_a);
	hdx_free(&ma);
	hdx_free(&mb);

	for (d = 1; d < (int)S_MAX; d++) {
		unsigned short cc = count;
		short rc;

		hdx_build(&mc, seed, u, 1, (int)(tag & 1));
		for (i = 0; i < DEM_BUF; i++) {
			dem_work[i] = dem_in[i];
			out_b[i] = (short)0xbeef;
		}
		rc = drive_start(&mc, dem_work, out_b, &cc, d);
		markc = hdx_mark(0, &mc, rc, cc, out_b);
		if (markc != marka)
			start_sep[d]++;
		hdx_free(&mc);
	}

	{
		unsigned short cc = count;
		short rc;

		hdx_build(&mc, seed, u, 1, (int)(tag & 1));
		for (i = 0; i < DEM_BUF; i++) {
			dem_work[i] = dem_in[i];
			out_b[i] = (short)0xbeef;
		}
		rc = drive_start(&mc, dem_work, out_b, &cc, S_NONE);
		markc = hdx_mark(0, &mc, rc, cc, out_b);
		diff_eq_int("RxHdxStartV17 model (%ld)", markc == marka, 1,
			    tag);
		hdx_free(&mc);
	}
}

static int
run_idle_start(void)
{
	/*
	 * COUNT 2 IS THERE ON PURPOSE.  `V17RXS_DEC_ERROR` is 0x1c2, which is
	 * `struct fpm_fse::mse` -- the equaliser OWNS it and rewrites it once
	 * per symbol it produces.  A block of two samples produces none, so
	 * the planted 0x1ffe / 0x1fff / 0x2000 / negative values survive to
	 * the compare and the threshold's exact edges are actually driven.
	 * The larger counts are the realistic case and are classified from
	 * what the field HOLDS after the call, not from what was planted.
	 */
	/*
	 * THE FIRST FIVE ROWS ARE THE TONE-ABANDON FIXTURE, and they are the
	 * only ones on which the threshold's edge is actually driven.
	 * `V17RXS_DEC_ERROR` is `struct fpm_fse::mse`, so any call that
	 * reaches the equaliser rewrites it and the planted 0x1ffe / 0x1fff /
	 * 0x2000 / negative values never arrive at the compare.  A block the
	 * tone detector answers PRESENT to makes `DemodDataV17` return before
	 * the equaliser, with the gain control already run -- so the value
	 * survives AND the carrier is up.  Without this the sweep was five
	 * trials of "no carrier, so no transition either way" and three
	 * separating counts were zero.  See `fill_block`.
	 */
	static const struct hdx_setup idles[] = {
	    /* gate_18      carrier dec_err   countdown ep   amp gate10  hz */
	    { V17RX_STATE_START, 1, 0x1ffe,           0,  1,12000,   1,1200 },
	    { V17RX_STATE_START, 1, 0x1fff,           0,  1,12000,   1,1200 },
	    { V17RX_STATE_START, 1, 0x2000,           0,  1,12000,   1,1200 },
	    { V17RX_STATE_START, 1, (short)0xffff,    0,  1,12000,   1,1200 },
	    { V17RX_STATE_START, 1, (short)0x8000,    0,  1,12000,   1,1200 },
	    { V17RX_STATE_IDLE,  0, 0x1000,           0,  1, 9000,   1,   0 },
	    { V17RX_STATE_IDLE,  1, 0x1000,           0,  1,  600,   1,   0 }
	};
	static const struct hdx_setup starts[] = {
	    { V17RX_STATE_START, 1,      4,           0,  1, 9000,   1,   0 },
	    { V17RX_STATE_START, 0,      4,           0,  1, 9000,   1,   0 },
	    { V17RX_STATE_START, 1,      4,           0,  1,  600,   1,   0 },
	    { V17RX_STATE_START, 1, 0x4000,           0,  1, 9000,   1,   0 }
	};
	static const unsigned short counts[] = { 0, 40, 160 };
	static const unsigned levels[] = { 0, 2 };
	int s, c, l;
	long tag = 0;

	diff_begin("RxHdxIdleV17 / RxHdxStartV17");

	for (s = 0; s < (int)(sizeof(idles) / sizeof(idles[0])); s++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++)
	for (l = 0; l < (int)(sizeof(levels) / sizeof(levels[0])); l++) {
		run_idle_one(spread(0x17f30000u, tag), &idles[s], counts[c],
			     levels[l], tag);
		tag++;
	}

	tag = 0;
	for (s = 0; s < (int)(sizeof(starts) / sizeof(starts[0])); s++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++)
	for (l = 0; l < (int)(sizeof(levels) / sizeof(levels[0])); l++) {
		run_start_one(spread(0x18040000u, tag), &starts[s], counts[c],
			      levels[l], tag);
		tag++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* Layer C -- the machine, walked                                         */

/*
 * F8790: a one-call fixture cannot see a state machine.  This dispatches
 * through `V17RXC_PROCESS` exactly as `V17RX_modem`'s inner loop does, one
 * call per block, and lets the machine choose its own next handler.  Each side
 * dispatches through ITS OWN slot, because the two hold different addresses;
 * `compare_slot` is what checks they agree about which handler that is.
 *
 * Each walk visits `V17RX_STATE_EPOCH_DET` at most once, which is D1216's
 * bound and is why the AGC tables stay inside themselves.
 */
static void
run_walk_one(unsigned seed, const struct hdx_setup *u, int first,
	     unsigned short count, unsigned level, long tag)
{
	int blk, i;

	fill_hz = u->hz;
	hdx_build(&ma, seed, u, 1, (int)(tag & 1));
	hdx_build(&mb, seed, u, 0, (int)(tag & 1));
	put_slot(&ma, first, 1);
	put_slot(&mb, first, 0);

	debug_begin(level);
	for (blk = 0; blk < 12; blk++) {
		unsigned short ca = count, cb = count;
		v17rx_process_fn fa, fb;
		short ra, rb;
		short st;
		long id = 500000 + tag * 100 + blk;

		fill_block((int)count, u->amp, (unsigned)blk * 17u);
		for (i = 0; i < DEM_BUF; i++)
			out_a[i] = out_b[i] = (short)0xbeef;

		st = get_s(ma.ctl, V17RXC_STATE);
		if (st >= 0 && st < 8)
			walk_states[st]++;

		fa = *(v17rx_process_fn *)(void *)(ma.ctl + V17RXC_PROCESS);
		fb = *(v17rx_process_fn *)(void *)(mb.ctl + V17RXC_PROCESS);

		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		ra = fa(ma.robj, dem_work, out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		rb = fb(mb.robj, dem_work, out_b, &cb);

		hdx_compare(&ma, &mb, id, ra, rb, ca, cb, "walk");
		walk_blocks++;
	}
	debug_compare(500000 + tag);
	debug_end();

	hdx_free(&ma);
	hdx_free(&mb);
}

static int
run_walk(void)
{
	/*
	 * THE LAST TWO ROWS ARE THE ONLY WAY TO TWO OF THE EIGHT STATES.
	 * BRIDGE is reachable only when `V17RXC_INT_0010` is clear, which is
	 * what the PROTOCOL arm branches on, and the walk cannot get there
	 * from START in twelve blocks because the EPOCH_DET arm then seeds 62
	 * blocks rather than 1.  So that row starts the machine AT PROTOCOL.
	 * ERROR is reachable only from a handler that HAS an error arm, and
	 * `RxHdxStartV17` has none, so that row starts at EPOCH_DET with no
	 * carrier.
	 */
	static const struct hdx_setup setups[] = {
	    /* gate_18         carrier dec_err countdown ep   amp gate10 hz */
	    { V17RX_STATE_START,     1,      4,      1,  1, 9000,   1,   0 },
	    { V17RX_STATE_START,     1,      4,      2,  1, 9000,   1,   0 },
	    { V17RX_STATE_START,     1,     14,      1,  1, 9000,   1,   0 },
	    { V17RX_STATE_START,     0,      4,      1,  1, 9000,   1,   0 },
	    { V17RX_STATE_START,     1,      4,      1,  0, 9000,   1,   0 },
	    { V17RX_STATE_IDLE,      1,      4,      1,  1, 9000,   1,   0 },
	    { V17RX_STATE_IDLE,      1, 0x2000,      1,  1, 9000,   1,   0 },
	    { V17RX_STATE_PROTOCOL,  1,      4,      1,  1, 9000,   0,   0 },
	    { V17RX_STATE_EPOCH_DET, 0,      4,      1,  1, 9000,   1,   0 }
	};
	/* Start, Idle, Prtcol and EpochDet, by the `ours_fn` index. */
	static const int firsts[] = { 1, 1, 1, 1, 1, 7, 7, 3, 2 };
	static const unsigned short counts[] = { 40, 160 };
	static const unsigned levels[] = { 0, 2 };
	int s, c, l;
	long tag = 0;

	diff_begin("the V.17 receive machine, walked");

	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++)
	for (l = 0; l < (int)(sizeof(levels) / sizeof(levels[0])); l++) {
		run_walk_one(spread(0x18150000u, tag), &setups[s], firsts[s],
			     counts[c], levels[l], tag);
		tag++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;
	int i;

	harness_alloc_reset();
	dem_tables();

	rc |= run_next();
	rc |= run_train();
	rc |= run_epoch();
	rc |= run_idle_start();
	rc |= run_walk();

	/*
	 * The separating counts and the run counters.  A zero in the first
	 * group means the check above it is decoration; a zero in the second
	 * means the sweep never reached the case it was written for.  Finding
	 * F134.
	 */
	diff_begin("v17rxstate separating trials and denominators");

	for (i = 1; i < (int)N_MAX; i++)
		diff_eq_int("RxNextStateV17 reading %ld separates",
			    next_sep[i] > 0, 1, i);
	for (i = 1; i < (int)H_MAX; i++)
		diff_eq_int("training-handler reading %ld separates",
			    hdx_sep[i] > 0, 1, i);
	for (i = 1; i < (int)P_MAX; i++)
		diff_eq_int("RxHdxEpochDetV17 reading %ld separates",
			    epoch_sep[i] > 0, 1, i);
	for (i = 1; i < (int)I_MAX; i++)
		diff_eq_int("RxHdxIdleV17 reading %ld separates",
			    idle_sep[i] > 0, 1, i);
	for (i = 1; i < (int)S_MAX; i++)
		diff_eq_int("RxHdxStartV17 reading %ld separates",
			    start_sep[i] > 0, 1, i);

	for (i = 0; i < 8; i++)
		diff_eq_int("RxNextStateV17 arm %ld ran", next_arm[i] > 0, 1,
			    i);
	diff_eq_int("the rate was restored (%ld)", next_restored > 0, 1,
		    next_restored);
	diff_eq_int("the rate was not restored (%ld)", next_notrestored > 0, 1,
		    next_notrestored);
	diff_eq_int("PROTOCOL took the SCRAM arm (%ld)", next_scram_arm > 0, 1,
		    next_scram_arm);
	diff_eq_int("PROTOCOL took the BRIDGE arm (%ld)", next_bridge_arm > 0,
		    1, next_bridge_arm);
	diff_eq_int("the AGC step was checked as a pointer (%ld)",
		    agc_checked > 0, 1, agc_checked);

	diff_eq_int("a training handler saw the carrier (%ld)",
		    train_carrier > 0, 1, train_carrier);
	diff_eq_int("a training handler lost the carrier (%ld)",
		    train_lost > 0, 1, train_lost);
	diff_eq_int("a training countdown expired (%ld)", train_expired > 0, 1,
		    train_expired);
	diff_eq_int("a training countdown was held (%ld)", train_held > 0, 1,
		    train_held);
	diff_eq_int("the rate ladder discriminated the three (%ld)",
		    train_ladder_sep > 0, 1, train_ladder_sep);
	for (i = 0; i < 4; i++)
		diff_eq_int("Scram reported rate code %ld",
			    train_rate_seen[i] > 0, 1, i);
	diff_eq_int("LOW_SNR was raised (%ld)", snr_low_seen > 0, 1,
		    snr_low_seen);
	diff_eq_int("LOW_SNR was left alone (%ld)", snr_high_seen > 0, 1,
		    snr_high_seen);

	diff_eq_int("an epoch countdown expired (%ld)", epoch_expired > 0, 1,
		    epoch_expired);
	diff_eq_int("the epoch was found (%ld)", epoch_found > 0, 1,
		    epoch_found);
	diff_eq_int("the epoch was not found (%ld)", epoch_missed > 0, 1,
		    epoch_missed);
	diff_eq_int("RxHdxEpochDetV17 lost the carrier (%ld)", epoch_lost > 0,
		    1, epoch_lost);

	diff_eq_int("RxHdxIdleV17 saw the carrier (%ld)", idle_carrier > 0, 1,
		    idle_carrier);
	diff_eq_int("RxHdxIdleV17 saw no carrier (%ld)", idle_nocarrier > 0, 1,
		    idle_nocarrier);
	diff_eq_int("a small decision error (%ld)", idle_small > 0, 1,
		    idle_small);
	diff_eq_int("a large decision error (%ld)", idle_big > 0, 1, idle_big);
	diff_eq_int("a negative decision error (%ld)", idle_negative > 0, 1,
		    idle_negative);

	diff_eq_int("RxHdxStartV17 saw the carrier (%ld)", start_carrier > 0,
		    1, start_carrier);
	diff_eq_int("RxHdxStartV17 saw no carrier (%ld)", start_nocarrier > 0,
		    1, start_nocarrier);

	diff_eq_int("the machine was walked (%ld blocks)", walk_blocks > 0, 1,
		    walk_blocks);
	for (i = 0; i < 8; i++)
		diff_eq_int("the walk visited state %ld", walk_states[i] > 0,
			    1, i);

	diff_eq_int("diagnostics were compared (%ld lines)", dbg_lines_seen > 0,
		    1, dbg_lines_seen);

	rc |= diff_end();

	return rc;
}
