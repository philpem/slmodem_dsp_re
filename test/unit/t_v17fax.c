/*
 * t_v17fax.c -- differential test of V.17's receive primitives and the
 *               transmit-side setters beside them.
 *
 * Every one of these functions is small, and most do nothing but move a value
 * between two offsets of an instance nothing models.  "It agrees with the blob
 * over a thousand random inputs" is therefore nearly worthless on its own:
 * what can be WRONG is which offset, how wide the access is, and which way a
 * comparison faces -- and a wrong offset agrees with the right one over every
 * input unless the fixture makes the two differ.
 *
 * So every block of both instances is pseudorandom before any pointer goes
 * into it, every call compares ALL of them afterwards, and every check is
 * built around a NAMED WRONG READING whose separating-trial count is asserted
 * non-zero at the end (finding F3052's rule).
 *
 * THE WRONG READINGS, AND THE THREE THAT ARE NOT SEPARABLE
 *
 *   SeedScramblerV17
 *       - the seed at fp + 0x28 or fp + 0x30 rather than fp + 0x2c, which is
 *         `struct fpm_sdm::reg`; and stored 16-bit rather than 32-bit.
 *       - the block taken from obj + 0x24 rather than obj + 0x28.
 *
 *   SetEncoderV17
 *       - mode 1 writing V17FP_SMC_SHORT_06 as modes 0 and 2 do.  This is the
 *         asymmetry the object encodes and the one thing a careless reading
 *         would smooth over.
 *       - a `default:` arm that writes.  Driven at -32768, -2, -1, 3, 4 and
 *         32767, where the object does nothing at all.
 *       - the selector at fp + 0x8e rather than fp + 0x8c.
 *       - `which` READ UNSIGNED IS NOT SEPARABLE AND IS NOT CLAIMED.  The
 *         object's chain is `cmp $1 / je / jle / cmp $2 / je`, and no `short`
 *         has an unsigned reading that lands on 0, 1 or 2 when its signed one
 *         does not: -1 is 65535 either way and both fall out.  The `movswl`
 *         is recorded in v17fax.h as forced by the object; nothing here
 *         pretends to measure it.  What IS asserted is that every negative
 *         and every out-of-range `which` left the block untouched.
 *
 *   SetTxModeV17
 *       - the per-mode literal at `V17FP_SMC` off by the pattern that would
 *         make it track `mode` cleanly (`V17FP_SMC_SHORT_12`'s 1, 2, 3, 4
 *         does; `V17FP_SMC`'s 3, 2, 4, 5 does not, so the two are not
 *         interchangeable).
 *       - the constellation maps swapped (I for Q) or drawn from the wrong
 *         mode's pair.
 *       - `V17TXP_NOCARRIER_SYM` one shift off (a doubling error).
 *       - the shift register left as `SDM_init` sets it, rather than
 *         restored (D1271).
 *       - a `default:` arm that does nothing, or that does not clear bit 0
 *         of `V17TX_OBJ_RESULT_B1` alongside setting bit 1.
 *       - THE OUT-OF-RANGE TABLE READ (D1270) IS NOT DRIVEN AT THE EXTREMES.
 *         `V17TX_SYM_SIZE[mode]` runs unconditionally before the mode is
 *         checked, and what an out-of-bounds read returns is the linked
 *         object's own layout, which the reference build and this one do not
 *         share -- so the `default` arm is exercised at a `mode` clear of the
 *         table, and only its two result bytes are compared, not the rest of
 *         `fp`.
 *
 *   V17TX_status
 *       - the NULL guard absent, so the block is filled anyway.
 *       - status + 0x10 taken from params + 0x10 rather than params + 0x02.
 *       - the flag byte masked with 0x02 rather than 0x04.
 *       - status + 0x0e written.  The object skips it and writes +0x10; a
 *         reading that walked the block in even steps would not.
 *       - THE DEAD STORE AT status + 0x14 IS NOT SEPARABLE AND IS NOT
 *         CLAIMED.  The second store covers it for every input.  It is in the
 *         source because the object has it and because the intervening load
 *         may alias, not because a test can see it.
 *
 *   V17RX_modem
 *       - the flag byte at obj + 0x28 rather than obj + 0x29, and the bit
 *         0x01 rather than 0x02.  Observable because that byte lives INSIDE
 *         the int the function returns.
 *       - `in` advanced by what was produced and `out` by what was consumed,
 *         which is the transposition the object's two `lea`s make easy to get
 *         backwards.  Evaluated for real: the dispatch stub logs both.
 *       - `*count` never written back.
 *       - the result read from obj + 0x2c rather than obj + 0x28.
 *       - the dispatch slot at ctl + 0x10 rather than ctl + 0x14, separated
 *         by construction: the fixture leaves +0x10 pseudorandom, so a
 *         reading that used it would have jumped into noise.
 *       - THE RUNNING TOTAL'S `short` TRUNCATION IS NOT SEPARABLE AND IS NOT
 *         CLAIMED.  The object narrows it with `cwtl` every iteration, but
 *         the only place it escapes is a 16-bit store through `count`, and
 *         repeated truncation modulo 65536 is the same value as one at the
 *         end.  The 2,000-sample run drives the total past 32767 anyway, so
 *         the corner is REACHED and counted; it is just not separating.
 *
 *   CarrierDetectV17 / QualityDetectV17
 *       - receiver state + 0xd0 read 16-bit where the object reads 32.  THE
 *         FIXTURE PUTS A NON-ZERO SHORT AT +0xd2, which is the only thing
 *         that makes the two readings differ (finding F8853).
 *         IT IS SEPARABLE IN `CarrierDetectV17` ONLY.  `QualityDetectV17`
 *         narrows the AND back to a `short`, and (short)(x & m) depends on
 *         nothing above bit 15, so its own `movswl` cannot be measured -- it
 *         is followed because the object was not free to emit it, and that is
 *         said in v17fax.h rather than asserted here.
 *       - the 999 gate read `>=`, driven at 998, 999 and 1000.
 *       - the 0x3fff gate read `>=`, driven at 0x3ffe, 0x3fff, 0x4000 and at
 *         negative errors.
 *       - the `r = 0` and `r &= 1` arms swapped.
 *       - QualityDetectV17's block counter read UNSIGNED.  Reached by seeding
 *         it at 0x8000, where a signed reading SMOOTHS and an unsigned one
 *         returns untouched: the two leave different memory behind, so this
 *         one is a measurement and not a codegen note.
 *       - the two smoothing weights swapped.  Finding F8790: invisible to
 *         every codegen check AND to any one-block fixture, so the counter is
 *         driven over 60 consecutive blocks with the error moving on each.
 *       - the round-to-nearest term dropped.
 *       - the judge comparison faced the other way at block 0x32.
 *
 *   EpochDetectV17 / GetSNRV17 / StoreCoefV17 / Restore_rateV17
 *       - GetSNRV17's UNSIGNED load, asserted to be FREE over the whole
 *         domain rather than claimed to be measured: the two readings differ
 *         by 65536 and the result is truncated to a short.
 *       - StoreCoefV17's count read as 48 or 50 rather than 49, and its two
 *         arrays transposed.
 *       - Restore_rateV17's saved rate read unsigned rather than signed.
 *
 * BOTH SIDES' DIAGNOSTICS ARE COMPARED AS OUTPUT.  Four of these functions
 * print, all four are gated on `dsplibs_debug_level > 1`, and a branch taken
 * on one side and not the other shows up in the transcript and nowhere else.
 */

#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v17fax.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17cfg.h"

extern int ref_V17RX_modem(void *modem, short *in, short *out,
			   unsigned short *count);
extern void ref_V17RX_delete(void *modem);
extern void ref_V17TX_delete(void *modem);
extern int ref_V17TX_modem(void *modem, unsigned short *in, short *out,
			   unsigned short *count);
extern int ref_FIFO_write(struct fax_fifo *f, unsigned short *src,
			  unsigned short count);
extern int ref_V17RX_status(void *modem, struct v17_status *status);
extern void ref_ScrambleDataV17(void *modem, unsigned short *data,
				unsigned short count);
extern void ref_DescrambleDataV17(void *modem, unsigned short *data,
				  unsigned short count);
extern unsigned short ref_DemodDataV17(void *modem, short *in,
				       unsigned short *bits,
				       unsigned short count);

extern void ref_SDM_init(struct fpm_sdm *sdm, const struct fpm_sdm_cfg *cfg);
extern void ref_SDM_scrambler(struct fpm_sdm *sdm, unsigned short *data,
			      unsigned short count);
extern void ref_SDM_descrambler(struct fpm_sdm *sdm, unsigned short *data,
				unsigned short count);

extern struct sgd *ref_SGD_create(struct sgd *s, const struct sgd_cfg *cfg);

extern void *ref_FPM_TONE_create(void *state, const void *cfg);
extern void ref_FPM_TONE_kill(void *state, short *samples, short count);
extern void ref_FPM_TONE_delete(void *state);
extern void ref_FPM_MTD_delete(struct fpm_mtd *state);

extern void ref_FPM_MRF_init(struct fpm_mrf *state,
			     const struct fpm_mrf_cfg *cfg, int fresh);
extern void ref_FPM_MRF_free(struct fpm_mrf *state);
extern short ref_FPM_MRF_filter(struct fpm_mrf *state, const short *in,
				short *out, short count);
extern void ref_FPM_SRE_init(struct fpm_sre *sre, const struct fpm_sre_cfg *cfg,
			     int fresh);
extern void ref_FPM_SRE_free(struct fpm_sre *sre);
extern unsigned short ref_FPM_SRE_recover(struct fpm_sre *sre, const short *in,
					  short *out, short count);
extern void ref_FPM_FSE_init(struct fpm_fse *state,
			     const struct fpm_fse_cfg *cfg, int fresh);
extern void ref_FPM_FSE_free(struct fpm_fse *state);
extern unsigned short ref_FPM_FSE_receive(struct fpm_fse *state,
					  const short *in, unsigned short *out,
					  unsigned short count);

/*
 * The two "default" configurations are templates and not filters -- see the
 * note in `t_v29fax.c`.  The V.32 instances are the real ones and are what the
 * demodulator fixture uses.
 */
extern const struct fpm_mrf_cfg MRFv32_CFG;
extern const struct fpm_sre_cfg SREv32_CFG;
extern void ref_SeedScramblerV17(void *modem, unsigned int seed);
extern void ref_SetEncoderV17(void *modem, short which, short arg);
extern void ref_SetTxModeV17(void *modem, short mode);
extern const short ref_V17TX_SYM_SIZE[4];
extern const short ref_SMCv17_CFG[2];
extern const short ref_VTBv17_QMAP128[129];
extern const short ref_VTBv17_IMAP128[129];
extern const short ref_VTBv17_QMAP64[65];
extern const short ref_VTBv17_IMAP64[65];
extern const short ref_VTBv17_QMAP32[33];
extern const short ref_VTBv17_IMAP32[33];
extern const short ref_VTBv17_QMAP16T[17];
extern const short ref_VTBv17_IMAP16T[17];
extern int ref_V17TX_status(void *params, struct v17_status *status);
extern int ref_CarrierDetectV17(void *modem);
extern short ref_QualityDetectV17(void *modem);
extern int ref_EpochDetectV17(void *modem);
extern short ref_GetSNRV17(void *modem);
extern void ref_StoreCoefV17(void *modem);
extern void ref_Restore_rateV17(void *modem);

extern short ref_DataCarrierDetectV17(void *modem, const short *in,
				      unsigned short count);

extern void ref_FPM_AGC_init(struct fpm_agc *agc, const struct fpm_agc_cfg *cfg,
			     int reset);
extern void ref_FPM_AGC_agc(struct fpm_agc *agc, short *samples,
			    unsigned short count);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *state,
					  const struct fpm_mtd_cfg *cfg);
extern short ref_FPM_MTD_detect(struct fpm_mtd *state, const short *samples,
				short count);
extern short ref_FPM_rms(const short *samples, unsigned short count);

/*
 * `AGCv17_CFG` is the object's OWN V.17 gain-control configuration, .rodata
 * 0x9e10, and is used here for that reason.  The tone detector's is not: no
 * `MTDv17_CFG` exists in the object, so `MTDv22_CFG` stands in.  That costs
 * nothing a differential test cares about -- both sides run the same detector
 * over the same samples, and what is being compared is the V.17 code around
 * it -- but it does mean the FREQUENCIES below are chosen to move THAT bank's
 * verdict, and are not a claim about what V.17 listens for.
 */
extern const struct fpm_agc_cfg ref_AGCv17_CFG;
extern const struct fpm_mtd_cfg ref_MTDv22_CFG;

extern unsigned int ref_dsplibs_debug_level;

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x80
/* Past V17RXC_AGC (0x30) plus a whole struct fpm_agc. */
#define CTL_SIZE	0x80
#define RXS_SIZE	0x5000		/* the object reaches +0x4fb8       */
#define FP_SIZE		0x100
#define PRM_SIZE	0x40
#define STA_SIZE	0x40
#define COEF_SLOTS	64		/* V17_COEF_N is 49; the rest catch
					 * a loop that runs one too far     */

#define NBIG_IN		4096
#define NBIG_OUT	48000
#define OMARK		0x5ead

#define MTD_ACC		16		/* two words per tone section       */
#define NBLK		512		/* the largest block driven below   */

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
 * A SEED THAT ACTUALLY MOVES EVERY BIT.
 *
 * `rng_next` is xorshift32, which is LINEAR over GF(2): every bit of the byte
 * `fixture()` writes at a fixed offset is a fixed XOR of the seed's bits.
 * Seeding a sweep with `base + where` for a small `where` therefore varies
 * only the seed's low five bits, and any bit of any byte whose linear form
 * does not involve one of those five is CONSTANT over the whole sweep.
 *
 * That is not a theoretical worry: it is how `run_rxstatus`'s "0x80 not forced
 * clear" reading reported a separating count of zero over 24 trials that
 * looked pseudorandom.  Bit 7 of the status byte the fixture laid down was 0
 * on all 24.  Finding F9105.
 */
static unsigned
spread(unsigned base, long where)
{
	return base ^ ((unsigned)where * 0x9e3779b9u);
}

struct fix {
	unsigned char	robj[OBJ_SIZE];		/* the receive instance      */
	unsigned char	tobj[OBJ_SIZE];		/* the transmit instance     */
	unsigned char	ctl[CTL_SIZE];		/* V17RX_OBJ_CTL             */
	unsigned char	rxs[RXS_SIZE];		/* V17RX_OBJ_STATE           */
	unsigned char	fp[FP_SIZE];		/* V17TX_OBJ_FP              */
	unsigned char	prm[PRM_SIZE];		/* V17TX_OBJ_PARAMS          */
	unsigned char	sta[STA_SIZE];		/* V17TX_status's second arg */
	short		coef0[COEF_SLOTS];
	short		coef1[COEF_SLOTS];
	short		save0[COEF_SLOTS];
	short		save1[COEF_SLOTS];
	short		ratesave[4];
	/* DataCarrierDetectV17's private detector chain. */
	struct fpm_mtd	mtd;
	short		mtd_acc[MTD_ACC];
	short		buf2[NBLK];
	double		align;
};

static struct fix ma, mb, mc, msnap;

static void
put_ptr(unsigned char *p, int off, void *v)
{
	*(void **)(void *)(p + off) = v;
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

/*
 * Point one fixture's instances at that fixture's own blocks.  Split out so a
 * whole-struct copy can be re-homed: `memcpy` duplicates the pointers too,
 * and a copy still pointing at the original measures nothing.
 */
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
	put_ptr(f->tobj, V17TX_OBJ_PARAMS, f->prm);
	put_ptr(f->tobj, V17TX_OBJ_FP, f->fp);
}

/*
 * Lay one pair of instances down.  EVERY byte of every block is pseudorandom
 * before the pointers go in, so an offset wrong by two bytes reads garbage
 * rather than a plausible zero -- and in particular receiver state + 0xd2 is
 * never zero, which is what separates the 32-bit read of +0xd0 from the
 * 16-bit one (finding F8853).
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
		/* Five per-fixture pointers, and they differ for ever. */
		if ((i >= V17RX_OBJ_COEFSAVE0
		     && i < V17RX_OBJ_RATESAVE + (int)sizeof(void *))
		    || (i >= V17RX_OBJ_CTL
			&& i < V17RX_OBJ_STATE + (int)sizeof(void *)))
			continue;
		if (a->robj[i] != b->robj[i])
			return i;
	}
	return -1;
}

static long
tobj_diff(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < OBJ_SIZE; i++) {
		if (i >= V17TX_OBJ_PARAMS
		    && i < V17TX_OBJ_FP + (int)sizeof(void *))
			continue;
		if (a->tobj[i] != b->tobj[i])
			return i;
	}
	return -1;
}

static long
rxs_diff(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < RXS_SIZE; i++) {
		if (i >= V17RXS_COEF0
		    && i < V17RXS_COEF1 + (int)sizeof(void *))
			continue;
		if (a->rxs[i] != b->rxs[i])
			return i;
	}
	return -1;
}

static long
ctl_diff(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < CTL_SIZE; i++) {
		/* Three per-fixture pointers, and they differ for ever. */
		if (i >= V17RXC_PROCESS
		    && i < V17RXC_PROCESS + (int)sizeof(void *))
			continue;
		if (i >= V17RXC_MTD2
		    && i < V17RXC_BUF2 + (int)sizeof(void *))
			continue;
		if (a->ctl[i] != b->ctl[i])
			return i;
	}
	return -1;
}

/*
 * The detector's own state, field by field, because its `acc` pointer is
 * per-fixture and a whole-struct compare would report that for ever.  The
 * config's `coeff` is NOT skipped: both sides copy it from the same
 * `MTDv22_CFG`, so it must come out equal.
 */
static long
mtd_diff(const struct fix *a, const struct fix *b)
{
	if (a->mtd.cfg.coeff != b->mtd.cfg.coeff)
		return 0;
	if (a->mtd.cfg.tones != b->mtd.cfg.tones)
		return 4;
	if (a->mtd.cfg.ratio != b->mtd.cfg.ratio)
		return 6;
	if (a->mtd.cfg.min_level != b->mtd.cfg.min_level)
		return 8;
	if (a->mtd.cfg.f0a != b->mtd.cfg.f0a)
		return 0x0a;
	if (a->mtd.dc_state[0] != b->mtd.dc_state[0]
	    || a->mtd.dc_state[1] != b->mtd.dc_state[1])
		return 0x10;
	if (a->mtd.out_of_band != b->mtd.out_of_band)
		return 0x14;
	if (a->mtd.wideband != b->mtd.wideband)
		return 0x16;
	return first_diff((const unsigned char *)b->mtd_acc,
			  (const unsigned char *)a->mtd_acc,
			  (int)sizeof(a->mtd_acc)) < 0 ? -1 : 0x100;
}

/*
 * Everything a call must leave untouched, in one place.  A function that
 * scribbled anywhere in twenty kilobytes of receiver state would otherwise be
 * caught only if the scribble happened to land where this test looks.
 */
static void
compare_all(struct fix *a, struct fix *b, long where)
{
	diff_eq_int("at %ld: first differing receive-instance byte",
		    robj_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing transmit-instance byte",
		    tobj_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing control-block byte",
		    ctl_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing receiver-state byte",
		    rxs_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing private-block byte",
		    first_diff(b->fp, a->fp, FP_SIZE), -1, where);
	diff_eq_int("at %ld: first differing parameter byte",
		    first_diff(b->prm, a->prm, PRM_SIZE), -1, where);
	diff_eq_int("at %ld: first differing status byte",
		    first_diff(b->sta, a->sta, STA_SIZE), -1, where);
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
	diff_eq_int("at %ld: first differing detector field", mtd_diff(b, a),
		    -1, where);
	diff_eq_int("at %ld: first differing detector-buffer byte",
		    first_diff((const unsigned char *)b->buf2,
			       (const unsigned char *)a->buf2,
			       (int)sizeof(a->buf2)), -1, where);
}

/* --------------------------------------------------------------------- */

static long dbg_lines_seen;

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
	const char *ours = dsplib_debug_capture_text(0);
	const char *blob = dsplib_debug_capture_text(1);

	diff_eq_int("at %ld: the same diagnostic text", strcmp(ours, blob), 0,
		    where);
	diff_eq_int("at %ld: the same number of diagnostic lines",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), where);
	dbg_lines_seen += (long)dsplib_debug_capture_lines(1);
}

static void
debug_end(void)
{
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;
}

/* --------------------------------------------------------------------- */
/* Separating-trial counters                                             */

static long seed_off_sep, seed_width_sep, seed_base_sep;
static long enc_mode1_sep, enc_default_sep, enc_sel_sep;
static long enc_wrote, enc_declined;
static long sta_null_sep, sta_10_sep, sta_mask_sep, sta_0e_sep;
static long sta_assign_sep, sta_15_sep;
static long rxm_bit_sep, rxm_byte_sep, rxm_swap_sep;
static long rxm_wb_sep, rxm_slot_sep, rxm_result_sep;
static long rxm_wrapped, rxm_calls_seen;
static long cd_width_sep, cd_gate999_sep, cd_gate3fff_sep, cd_arm_sep;
static long cd_true, cd_false, cd_printed;
static long dcd_sep[8];
static long dcd_true, dcd_false, dcd_counted, dcd_refreshed, dcd_printed;
static long dcd_edge, dcd_edge_unsolved;
static long qd_signed_sep, qd_weight_sep, qd_round_sep;
static long qd_judge_sep, qd_smoothed, qd_judged, qd_unreliable;
static long snr_free_checked, coef_n_sep, coef_swap_sep, restore_sep;

/* --------------------------------------------------------------------- */
/* SeedScramblerV17                                                      */

static int
run_seed(void)
{
	static const unsigned seeds[] = {
		0u, 1u, 0xffffffffu, 0x80000000u, 0x0000ffffu, 0xffff0000u,
		0x5a5a5a5au, 0x12345678u
	};
	unsigned s;

	diff_begin("SeedScramblerV17");
	for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
		unsigned seed = seeds[s];
		long where = (long)s;

		fixture(&ma, 0x11110000u + s);
		fixture(&mb, 0x11110000u + s);

		ref_SeedScramblerV17(ma.tobj, seed);
		SeedScramblerV17(mb.tobj, seed);
		compare_all(&ma, &mb, where);
		diff_eq_int("at %ld: the register took the seed",
			    (long)(int)((struct fpm_sdm *)(void *)
					(ma.fp + V17FP_SDM))->reg,
			    (long)(int)seed, where);

		/* WRONG READING: the seed at fp + 0x28 or fp + 0x30. */
		fixture(&mc, 0x11110000u + s);
		put_i(mc.fp, V17FP_SDM + 0x0c, (int)seed);
		if (memcmp(mc.fp, ma.fp, FP_SIZE) != 0)
			seed_off_sep++;
		fixture(&mc, 0x11110000u + s);
		put_i(mc.fp, V17FP_SDM + 0x14, (int)seed);
		if (memcmp(mc.fp, ma.fp, FP_SIZE) != 0)
			seed_off_sep++;

		/* WRONG READING: stored 16-bit. */
		fixture(&mc, 0x11110000u + s);
		put_s(mc.fp, V17FP_SDM + 0x10, (short)seed);
		if (memcmp(mc.fp, ma.fp, FP_SIZE) != 0)
			seed_width_sep++;

		/* WRONG READING: the block taken from obj + 0x24. */
		fixture(&mc, 0x11110000u + s);
		put_i(mc.prm, V17FP_SDM + 0x10, (int)seed);
		if (memcmp(mc.prm, ma.prm, PRM_SIZE) != 0
		    || memcmp(mc.fp, ma.fp, FP_SIZE) != 0)
			seed_base_sep++;
	}
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* SetEncoderV17                                                         */

static int
run_setenc(void)
{
	static const short whichs[] = {
		-32768, -2, -1, 0, 1, 2, 3, 4, 32767
	};
	static const short args[] = { 0, 1, -1, 0x1234, -0x1234, 32767, -32768 };
	unsigned w, a;

	diff_begin("SetEncoderV17");
	for (w = 0; w < sizeof(whichs) / sizeof(whichs[0]); w++) {
		for (a = 0; a < sizeof(args) / sizeof(args[0]); a++) {
			short which = whichs[w];
			short arg = args[a];
			long where = (long)which * 100 + (long)a;
			unsigned seed = 0x22220000u + w * 16 + a;

			fixture(&ma, seed);
			fixture(&mb, seed);
			ref_SetEncoderV17(ma.tobj, which, arg);
			SetEncoderV17(mb.tobj, which, arg);
			compare_all(&ma, &mb, where);

			fixture(&mc, seed);
			if (memcmp(ma.fp, mc.fp, FP_SIZE) == 0) {
				enc_declined++;
				diff_eq_int("at %ld: out of range writes"
					    " nothing",
					    which < 0
					    || which > V17_ENCODER_TCM, 1,
					    where);
			} else {
				enc_wrote++;
				diff_eq_int("at %ld: in range writes",
					    which >= 0
					    && which <= V17_ENCODER_TCM, 1,
					    where);
			}

			/* WRONG READING: mode 1 writing the second value. */
			if (which == V17_ENCODER_ABS) {
				fixture(&mc, seed);
				put_s(mc.fp, V17FP_ENCODER_SEL,
				      V17_ENCODER_ABS);
				put_s(mc.fp, V17FP_SMC_SHORT_06, arg);
				if (memcmp(mc.fp, ma.fp, FP_SIZE) != 0)
					enc_mode1_sep++;
			}

			/* WRONG READING: a default arm that writes. */
			if (which < 0 || which > V17_ENCODER_TCM) {
				fixture(&mc, seed);
				put_s(mc.fp, V17FP_ENCODER_SEL, which);
				if (memcmp(mc.fp, ma.fp, FP_SIZE) != 0)
					enc_default_sep++;
			}

			/* WRONG READING: the selector two bytes high. */
			if (which >= 0 && which <= V17_ENCODER_TCM) {
				fixture(&mc, seed);
				put_s(mc.fp, V17FP_ENCODER_SEL + 2, which);
				if (which != V17_ENCODER_ABS)
					put_s(mc.fp, V17FP_SMC_SHORT_06, arg);
				if (memcmp(mc.fp, ma.fp, FP_SIZE) != 0)
					enc_sel_sep++;
			}
		}
	}
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* SetTxModeV17                                                          */

static long stm_case_sep, stm_pattern_sep, stm_map_sep, stm_nc_sep;
static long stm_reg_sep, stm_default_sep;
static long stm_valid, stm_default_hit;

/*
 * Two fresh `struct sgd` instances, one per fixture, so `V17TXP_SGD` never
 * points at a pseudorandom address `SGD_create` would then write through
 * (finding the shape of `txdelete_build`'s setup, above).
 */
static void
stm_arm_sgd(struct fix *f)
{
	struct sgd *s = ref_SGD_create(0, 0);
	put_ptr(f->prm, V17TXP_SGD, s);
}

/*
 * `fp`'s and `prm`'s content compared WITHOUT the two pointer pairs that are
 * per-fixture or per-binary by construction and never claimed to be equal as
 * bytes: `V17TXP_SGD` (each fixture's own `struct sgd *`, both freshly
 * malloc'd) and `V17FP_SMC_IMAP`/`_QMAP` (the reference binary's tables and
 * this tree's are two distinct symbols holding the same sixteen bytes, so
 * their ADDRESSES differ on principle). Those four are checked separately,
 * by content and by identity against the correctly-named table.
 */
static long
stm_fp_diff(const unsigned char *a, const unsigned char *b)
{
	int i;

	for (i = 0; i < FP_SIZE; i++) {
		if (i >= V17FP_SMC_IMAP
		    && i < V17FP_SMC_QMAP + (int)sizeof(void *))
			continue;
		if (a[i] != b[i])
			return i;
	}
	return -1;
}

static long
stm_prm_diff(const unsigned char *a, const unsigned char *b)
{
	int i;

	for (i = 0; i < PRM_SIZE; i++) {
		if (i >= V17TXP_SGD && i < V17TXP_SGD + (int)sizeof(void *))
			continue;
		if (a[i] != b[i])
			return i;
	}
	return -1;
}

static int
stm_maps_ok(const unsigned char *fp, short mode, int ours)
{
	const short *want_i, *want_q;
	void *got_i, *got_q;

	switch (mode) {
	case 0:
		want_i = ours ? VTBv17_IMAP16T : ref_VTBv17_IMAP16T;
		want_q = ours ? VTBv17_QMAP16T : ref_VTBv17_QMAP16T;
		break;
	case 1:
		want_i = ours ? VTBv17_IMAP32 : ref_VTBv17_IMAP32;
		want_q = ours ? VTBv17_QMAP32 : ref_VTBv17_QMAP32;
		break;
	case 2:
		want_i = ours ? VTBv17_IMAP64 : ref_VTBv17_IMAP64;
		want_q = ours ? VTBv17_QMAP64 : ref_VTBv17_QMAP64;
		break;
	default:
		want_i = ours ? VTBv17_IMAP128 : ref_VTBv17_IMAP128;
		want_q = ours ? VTBv17_QMAP128 : ref_VTBv17_QMAP128;
		break;
	}
	got_i = get_ptr(fp, V17FP_SMC_IMAP);
	got_q = get_ptr(fp, V17FP_SMC_QMAP);
	return got_i == (const void *)want_i && got_q == (const void *)want_q;
}

static int
run_settxmode(void)
{
	static const short modes[] = { 0, 1, 2, 3 };
	unsigned m;

	diff_begin("SetTxModeV17");
	for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
		short mode = modes[m];
		long where = (long)mode;
		unsigned seed = 0x55550000u + m;
		unsigned int reg_before;

		fixture(&ma, seed);
		fixture(&mb, seed);
		stm_arm_sgd(&ma);
		stm_arm_sgd(&mb);

		reg_before = ((struct fpm_sdm *)(void *)
			      (ma.fp + V17FP_SDM))->reg;
		if (reg_before == 0)
			reg_before = 0x13579bdfu;
		put_i(ma.fp, V17FP_SDM + 0x10, (int)reg_before);
		put_i(mb.fp, V17FP_SDM + 0x10, (int)reg_before);

		ref_SetTxModeV17(ma.tobj, mode);
		SetTxModeV17(mb.tobj, mode);

		diff_eq_int("at %ld: first differing receive-instance byte",
			    robj_diff(&mb, &ma), -1, where);
		diff_eq_int("at %ld: first differing transmit-instance byte",
			    tobj_diff(&mb, &ma), -1, where);
		diff_eq_int("at %ld: first differing control-block byte",
			    ctl_diff(&mb, &ma), -1, where);
		diff_eq_int("at %ld: first differing receiver-state byte",
			    rxs_diff(&mb, &ma), -1, where);
		diff_eq_int("at %ld: first differing private-block byte"
			    " (maps excepted)",
			    stm_fp_diff(mb.fp, ma.fp), -1, where);
		diff_eq_int("at %ld: first differing parameter byte"
			    " (SGD excepted)",
			    stm_prm_diff(mb.prm, ma.prm), -1, where);
		diff_eq_int("at %ld: our maps follow the mode",
			    stm_maps_ok(mb.fp, mode, 1), 1, where);
		diff_eq_int("at %ld: the blob's own maps follow the mode",
			    stm_maps_ok(ma.fp, mode, 0), 1, where);
		stm_valid++;

		diff_eq_int("at %ld: the shift register survived re-init",
			    (long)(int)((struct fpm_sdm *)(void *)
					(mb.fp + V17FP_SDM))->reg,
			    (long)(int)reg_before, where);

		/* WRONG READING: SDM_init's own clear left standing. */
		fixture(&mc, seed);
		stm_arm_sgd(&mc);
		put_i(mc.fp, V17FP_SDM + 0x10, (int)reg_before);
		ref_SetTxModeV17(mc.tobj, mode);
		put_i(mc.fp, V17FP_SDM + 0x10, 0);
		if (stm_fp_diff(mc.fp, ma.fp) != -1)
			stm_reg_sep++;

		/*
		 * WRONG READING: the per-mode literal at V17FP_SMC following
		 * the mode+1 pattern V17FP_SMC_SHORT_12 actually uses, rather
		 * than the object's 3, 2, 4, 5.
		 */
		fixture(&mc, seed);
		stm_arm_sgd(&mc);
		ref_SetTxModeV17(mc.tobj, mode);
		put_s(mc.fp, V17FP_SMC, (short)(mode + 1));
		if ((short)(mode + 1) != get_s(ma.fp, V17FP_SMC)
		    && stm_fp_diff(mc.fp, ma.fp) != -1)
			stm_case_sep++;

		/* WRONG READING: V17FP_SMC_SHORT_12 not tracking mode. */
		fixture(&mc, seed);
		stm_arm_sgd(&mc);
		ref_SetTxModeV17(mc.tobj, mode);
		put_s(mc.fp, V17FP_SMC_SHORT_12, (short)(mode + 2));
		if (stm_fp_diff(mc.fp, ma.fp) != -1)
			stm_pattern_sep++;

		/*
		 * WRONG READING: the maps swapped for this mode's pair.
		 * `stm_maps_ok` is the check the main assertion above relies
		 * on; this proves it would actually catch the swap rather
		 * than trusting that a pointer identity test always would.
		 */
		{
			unsigned char scratch[FP_SIZE];

			memcpy(scratch, mb.fp, FP_SIZE);
			put_ptr(scratch, V17FP_SMC_IMAP,
				get_ptr(mb.fp, V17FP_SMC_QMAP));
			put_ptr(scratch, V17FP_SMC_QMAP,
				get_ptr(mb.fp, V17FP_SMC_IMAP));
			if (stm_maps_ok(mb.fp, mode, 1)
			    && !stm_maps_ok(scratch, mode, 1))
				stm_map_sep++;
		}

		/* WRONG READING: V17TXP_NOCARRIER_SYM one shift off. */
		fixture(&mc, seed);
		stm_arm_sgd(&mc);
		ref_SetTxModeV17(mc.tobj, mode);
		put_s(mc.prm, V17TXP_NOCARRIER_SYM,
		      (short)(get_us(ma.prm, V17TXP_NOCARRIER_SYM) << 1));
		if (get_us(ma.prm, V17TXP_NOCARRIER_SYM) != 0
		    && stm_prm_diff(mc.prm, ma.prm) != -1)
			stm_nc_sep++;
	}

	/*
	 * The `default` arm.  `mode` is comfortably clear of `V17TX_SYM_SIZE`
	 * (D1270), so only the two result bytes in `tobj` -- untouched by
	 * that out-of-bounds read -- are compared.
	 */
	{
		static const short bad_modes[] = { -1, 4, 1000, -1000 };
		unsigned b;

		for (b = 0; b < sizeof(bad_modes) / sizeof(bad_modes[0]);
		     b++) {
			short mode = bad_modes[b];
			long where = 10000 + (long)b;
			unsigned seed = 0x55560000u + b;
			unsigned char ra20, ra21, rb20, rb21;

			fixture(&ma, seed);
			fixture(&mb, seed);
			stm_arm_sgd(&ma);
			stm_arm_sgd(&mb);

			ref_SetTxModeV17(ma.tobj, mode);
			SetTxModeV17(mb.tobj, mode);

			ra20 = ma.tobj[V17TX_OBJ_RESULT];
			ra21 = ma.tobj[V17TX_OBJ_RESULT_B1];
			rb20 = mb.tobj[V17TX_OBJ_RESULT];
			rb21 = mb.tobj[V17TX_OBJ_RESULT_B1];
			diff_eq_int("at %ld: default arm's result byte",
				    (long)rb20, (long)ra20, where);
			diff_eq_int("at %ld: default arm's result flag byte",
				    (long)rb21, (long)ra21, where);
			diff_eq_int("at %ld: default arm wrote 7",
				    (long)ra20, (long)V17TX_RESULT_BYTE_07,
				    where);
			stm_default_hit++;

			/* WRONG READING: an arm that leaves both untouched. */
			fixture(&mc, seed);
			if (mc.tobj[V17TX_OBJ_RESULT] != ra20
			    || mc.tobj[V17TX_OBJ_RESULT_B1] != ra21)
				stm_default_sep++;
		}
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V17TX_status                                                          */

static int
run_txstatus(void)
{
	unsigned s;

	diff_begin("V17TX_status");
	for (s = 0; s < 8; s++) {
		unsigned seed = 0x33330000u + s;
		int ra, rb;
		long where = (long)s;

		fixture(&ma, seed);
		fixture(&mb, seed);
		/*
		 * params + 0x00 and + 0x02 must differ, or "+0x10 from +0x02"
		 * and "+0x10 from +0x00" cannot be told apart; and the status
		 * block's +0x0e must be non-zero, or a reading that cleared it
		 * would look identical.
		 */
		put_s(ma.prm, 0x00, 0x0101);
		put_s(mb.prm, 0x00, 0x0101);
		put_s(ma.prm, 0x02, 0x0202);
		put_s(mb.prm, 0x02, 0x0202);
		put_s(ma.prm, 0x10, (short)(0x0400 | (s * 3)));
		put_s(mb.prm, 0x10, (short)(0x0400 | (s * 3)));
		put_s(ma.sta, 0x0e, 0x3c3c);
		put_s(mb.sta, 0x0e, 0x3c3c);
		/*
		 * The two flag bytes go in ALL ONES, which is what makes
		 * "assigned" and "merged" different answers: the object
		 * ASSIGNS +0x14 outright, so every bit the caller had is lost,
		 * and MASKS +0x15, so every bit but 0 survives.  With a
		 * pseudorandom byte the two readings agree whenever the bits
		 * happen to be clear.
		 */
		ma.sta[0x14] = mb.sta[0x14] = 0xff;
		ma.sta[0x15] = mb.sta[0x15] = 0xff;
		/*
		 * The block is `struct v17_status` and `sta` is the byte array
		 * it lives in, so the layout is asserted rather than assumed:
		 * a struct that had grown a member or lost its packing would
		 * put every offset below somewhere else.
		 */
		diff_eq_int("at %ld: the status struct is 0x1c bytes",
			    (long)sizeof(struct v17_status), 0x1c, where);
		diff_eq_int("at %ld: flags lands at +0x14",
			    (long)((char *)&((struct v17_status *)0)->flags
				   - (char *)0), 0x14, where);
		diff_eq_int("at %ld: int_18 lands at +0x18",
			    (long)((char *)&((struct v17_status *)0)->int_18
				   - (char *)0), 0x18, where);

		ra = ref_V17TX_status(ma.prm, (struct v17_status *)(void *)ma.sta);
		rb = V17TX_status(mb.prm, (struct v17_status *)(void *)mb.sta);
		diff_eq_int("at %ld: status returned", (long)rb, (long)ra,
			    where);
		diff_eq_int("at %ld: status reported filled", ra, 1, where);
		compare_all(&ma, &mb, where);

		/* WRONG READING: +0x10 taken from params + 0x10. */
		memcpy(&mc, &ma, sizeof(mc));
		rehome(&mc);
		put_s(mc.sta, 0x10, get_s(ma.prm, 0x10));
		if (memcmp(mc.sta, ma.sta, STA_SIZE) != 0)
			sta_10_sep++;

		/* WRONG READING: the flag byte masked with 0x02 not 0x04. */
		memcpy(&mc, &ma, sizeof(mc));
		rehome(&mc);
		mc.sta[0x14] = (unsigned char)(ma.prm[0x10] & 0x02);
		if (memcmp(mc.sta, ma.sta, STA_SIZE) != 0)
			sta_mask_sep++;

		/*
		 * WRONG READING: the flag byte MERGED rather than assigned.
		 * The object's final store to +0x14 is a plain `mov %al`, so
		 * the `and $0xfc` four instructions earlier is dead and the
		 * caller's bits do not survive; an `|=` keeps them.
		 */
		memcpy(&mc, &ma, sizeof(mc));
		rehome(&mc);
		mc.sta[0x14] = (unsigned char)(0xfc
					       | (ma.prm[0x10] & 0x04));
		if (memcmp(mc.sta, ma.sta, STA_SIZE) != 0)
			sta_assign_sep++;

		/*
		 * WRONG READING: +0x15 ASSIGNED zero rather than masked.  It
		 * is the other way round from +0x14 -- `andb $0xfe` is the
		 * only write to it and it is live -- so bits 1..7 must come
		 * through unchanged.
		 */
		memcpy(&mc, &ma, sizeof(mc));
		rehome(&mc);
		mc.sta[0x15] = 0;
		if (memcmp(mc.sta, ma.sta, STA_SIZE) != 0)
			sta_15_sep++;

		/* WRONG READING: status + 0x0e written too. */
		memcpy(&mc, &ma, sizeof(mc));
		rehome(&mc);
		put_s(mc.sta, 0x0e, 0);
		if (memcmp(mc.sta, ma.sta, STA_SIZE) != 0)
			sta_0e_sep++;

		/* The NULL case: it must do nothing and report nothing. */
		fixture(&ma, seed);
		fixture(&mb, seed);
		ra = ref_V17TX_status(ma.prm, (struct v17_status *)0);
		rb = V17TX_status(mb.prm, (struct v17_status *)0);
		diff_eq_int("at %ld: NULL status returned", (long)rb, (long)ra,
			    where);
		diff_eq_int("at %ld: NULL status reported empty", ra, 0, where);
		compare_all(&ma, &mb, where);

		/*
		 * WRONG READING: no guard at all.  A fresh fixture is what the
		 * block looked like before the call, and it is still that --
		 * so a version that filled it would differ here.
		 */
		fixture(&mc, seed);
		if (memcmp(mc.sta, ma.sta, STA_SIZE) == 0)
			sta_null_sep++;
	}
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V17RX_modem                                                           */

/*
 * The dispatch slot's occupant, and the log of what it was handed.
 *
 * It is a PURE FUNCTION of `*count` and of the samples it is shown, so both
 * sides drive it identically without any per-side bookkeeping: it takes five
 * samples at a time and produces twenty outputs for every one it consumes.
 * Producing four times what it consumes is what lets a 2,000-sample run push
 * the running total past 32767.
 */
struct rxm_log {
	int	calls;
	long	in_off;			/* of the LAST call                 */
	long	out_off;
	long	first_in_off;
	long	first_out_off;
	long	raw_total;
};

static struct rxm_log log_a, log_b;
static short big_in_a[NBIG_IN], big_in_b[NBIG_IN];
static short big_out_a[NBIG_OUT], big_out_b[NBIG_OUT];

static short
rxm_step(void *modem, short *in, short *out, unsigned short *count)
{
	struct rxm_log *lg;
	short *ibase;
	short *obase;
	unsigned short take;
	short give;
	int k;

	if (modem == (void *)ma.robj) {
		lg = &log_a;
		ibase = big_in_a;
		obase = big_out_a;
	} else {
		lg = &log_b;
		ibase = big_in_b;
		obase = big_out_b;
	}

	if (lg->calls == 0) {
		lg->first_in_off = in - ibase;
		lg->first_out_off = out - obase;
	}
	lg->calls++;
	lg->in_off = in - ibase;
	lg->out_off = out - obase;

	take = (unsigned short)(*count >= 5 ? 5 : *count);
	give = (short)(take * 20 + (*count & 3));

	for (k = 0; k < give; k++)
		out[k] = (short)(0x2000
				 + ((in[take ? k % take : 0] + k) & 0x7ff));

	*count = (unsigned short)(*count - take);
	lg->raw_total += give;
	return give;
}

/*
 * V17RX_modem with one reading changed.  Both halves are ours -- the dispatch
 * slot is a stub either way -- so what this measures is whether the change is
 * OBSERVABLE, which is what a separating count has to mean here.
 */
static int
drive_rxm(struct fix *f, short *in, short *out, unsigned short *count,
	  int result_off, int flag_off, unsigned char flag_bit, int swap,
	  int wb)
{
	short total = 0;
	short before;
	unsigned short left;
	void *ctl;
	short got;

	f->robj[flag_off] &= (unsigned char)~flag_bit;

	do {
		before = (short)*count;
		ctl = get_ptr(f->robj, V17RX_OBJ_CTL);
		got = (*(v17rx_process_fn *)(void *)((unsigned char *)ctl
						     + V17RXC_PROCESS))
				(f->robj, in, out, count);
		left = *count;
		if (swap) {
			in += got;
			out += before - left;
		} else {
			in += before - left;
			out += got;
		}
		total = (short)(total + got);
	} while (left != 0);

	if (wb)
		*count = (unsigned short)total;

	return get_i(f->robj, result_off);
}

static void
run_rxm_one(int n, unsigned seed, int alt)
{
	int ra, rb, rc;
	unsigned short ca, cb, cc;
	long where = (long)n;
	int i;

	fixture(&ma, seed);
	fixture(&mb, seed);
	put_ptr(ma.ctl, V17RXC_PROCESS, (void *)rxm_step);
	put_ptr(mb.ctl, V17RXC_PROCESS, (void *)rxm_step);

	rng_seed(seed ^ 0x77771111u);
	for (i = 0; i < NBIG_IN; i++)
		big_in_a[i] = big_in_b[i] = (short)(rng_next() & 0x3fff);
	for (i = 0; i < NBIG_OUT; i++)
		big_out_a[i] = big_out_b[i] = (short)OMARK;

	ca = (unsigned short)n;
	cb = (unsigned short)n;
	memset(&log_a, 0, sizeof(log_a));
	memset(&log_b, 0, sizeof(log_b));

	ra = ref_V17RX_modem(ma.robj, big_in_a, big_out_a, &ca);
	rb = V17RX_modem(mb.robj, big_in_b, big_out_b, &cb);

	diff_eq_int("at %ld: V17RX_modem returned", (long)rb, (long)ra, where);
	diff_eq_int("at %ld: the total written back", (long)cb, (long)ca,
		    where);
	diff_eq_int("at %ld: the same number of inner calls", log_b.calls,
		    log_a.calls, where);
	diff_eq_int("at %ld: the same inner calls fired", log_a.calls > 0, 1,
		    where);
	diff_eq_int("at %ld: the last input pointer", log_b.in_off,
		    log_a.in_off, where);
	diff_eq_int("at %ld: the last output pointer", log_b.out_off,
		    log_a.out_off, where);
	diff_eq_int("at %ld: the first input pointer", log_b.first_in_off,
		    log_a.first_in_off, where);
	diff_eq_int("at %ld: the first output pointer", log_b.first_out_off,
		    log_a.first_out_off, where);
	diff_eq_int("at %ld: first differing output sample",
		    first_diff((const unsigned char *)big_out_b,
			       (const unsigned char *)big_out_a,
			       (int)sizeof(big_out_a)), -1, where);
	diff_eq_int("at %ld: first differing input sample",
		    first_diff((const unsigned char *)big_in_b,
			       (const unsigned char *)big_in_a,
			       (int)sizeof(big_in_a)), -1, where);
	compare_all(&ma, &mb, where);

	rxm_calls_seen += log_a.calls;
	if (log_a.raw_total > 32767)
		rxm_wrapped++;

	if (!alt)
		return;

	/* WRONG READING: the flag bit 0x01 rather than 0x02. */
	fixture(&mc, seed);
	put_ptr(mc.ctl, V17RXC_PROCESS, (void *)rxm_step);
	cc = (unsigned short)n;
	rc = drive_rxm(&mc, big_in_b, big_out_b, &cc, V17RX_OBJ_RESULT,
		       V17RX_OBJ_RESULT_B1, 0x01, 0, 1);
	if (rc != ra)
		rxm_bit_sep++;

	/* WRONG READING: the flag byte at obj + 0x28. */
	fixture(&mc, seed);
	put_ptr(mc.ctl, V17RXC_PROCESS, (void *)rxm_step);
	cc = (unsigned short)n;
	rc = drive_rxm(&mc, big_in_b, big_out_b, &cc, V17RX_OBJ_RESULT,
		       V17RX_OBJ_RESULT, V17RX_FLAG_ERROR, 0, 1);
	if (rc != ra)
		rxm_byte_sep++;

	/* WRONG READING: `in` and `out` advanced by each other's count. */
	memset(&log_b, 0, sizeof(log_b));
	fixture(&mc, seed);
	put_ptr(mc.ctl, V17RXC_PROCESS, (void *)rxm_step);
	cc = (unsigned short)n;
	(void)drive_rxm(&mc, big_in_b, big_out_b, &cc, V17RX_OBJ_RESULT,
			V17RX_OBJ_RESULT_B1, V17RX_FLAG_ERROR, 1, 1);
	if (log_b.calls != log_a.calls || log_b.in_off != log_a.in_off
	    || log_b.out_off != log_a.out_off)
		rxm_swap_sep++;

	/* Restore what the transposed run scribbled over. */
	rng_seed(seed ^ 0x77771111u);
	for (i = 0; i < NBIG_IN; i++)
		big_in_b[i] = (short)(rng_next() & 0x3fff);

	/* WRONG READING: `*count` never written back. */
	fixture(&mc, seed);
	put_ptr(mc.ctl, V17RXC_PROCESS, (void *)rxm_step);
	cc = (unsigned short)n;
	(void)drive_rxm(&mc, big_in_b, big_out_b, &cc, V17RX_OBJ_RESULT,
			V17RX_OBJ_RESULT_B1, V17RX_FLAG_ERROR, 0, 0);
	if (cc != ca)
		rxm_wb_sep++;

	/*
	 * WRONG READING: the dispatch slot at ctl + 0x10.  Separated by
	 * construction: the fixture leaves that slot pseudorandom, so the
	 * object could not have been reading it and returned.
	 */
	if (get_ptr(ma.ctl, V17RXC_INT_0010)
	    != get_ptr(ma.ctl, V17RXC_PROCESS))
		rxm_slot_sep++;

	/* WRONG READING: the result read from obj + 0x2c. */
	if (get_i(ma.robj, V17RX_OBJ_RESULT + 4) != ra)
		rxm_result_sep++;
}

static int
run_rxm(void)
{
	static const int counts[] = { 0, 1, 2, 4, 5, 6, 9, 10, 37, 200 };
	unsigned c;

	diff_begin("V17RX_modem");
	for (c = 0; c < sizeof(counts) / sizeof(counts[0]); c++)
		run_rxm_one(counts[c], 0x44440000u + c, 1);

	/*
	 * The far corner: 2,000 samples at five per call and a hundred outputs
	 * per call is 40,000 outputs, so the total passes 32767 and the
	 * object's `cwtl` wraps it.  Driven with the alternates OFF, because
	 * the transposed arm would run the output pointer off the input
	 * buffer at this length.
	 */
	run_rxm_one(2000, 0x4444beefu, 0);
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* CarrierDetectV17                                                      */

static void
cd_setup(struct fix *f, int lo, int hi, int gate, int epoch, short g94,
	 short err)
{
	put_s(f->rxs, V17RXS_AGC_SIGNAL, (short)lo);
	put_s(f->rxs, V17RXS_AGC_SIGNAL + 2, (short)hi);
	put_i(f->rxs, V17RXS_INT_0120, -1);
	put_i(f->ctl, V17RXC_INT_0010, gate);
	put_i(f->rxs, V17RXS_EPOCH, epoch);
	put_s(f->rxs, V17RXS_SHORT_0094, g94);
	put_s(f->rxs, V17RXS_DEC_ERROR, err);
}

static int
drive_cd(struct fix *f, int narrow, int ge999, int ge3fff, int swap_arms)
{
	const unsigned char *rx = f->rxs;
	const unsigned char *ctl = f->ctl;
	int r;

	if (narrow)
		r = get_s(rx, V17RXS_AGC_SIGNAL) & get_i(rx, V17RXS_INT_0120);
	else
		r = get_i(rx, V17RXS_AGC_SIGNAL) & get_i(rx, V17RXS_INT_0120);

	if (get_i(ctl, V17RXC_INT_0010) != 0
	    && get_i(rx, V17RXS_EPOCH) != 0
	    && (ge999 ? get_s(rx, V17RXS_SHORT_0094) >= V17RXS_0094_MIN
		      : get_s(rx, V17RXS_SHORT_0094) > V17RXS_0094_MIN)) {
		int big = ge3fff
			? get_s(rx, V17RXS_DEC_ERROR) >= V17RXS_DEC_ERROR_MAX
			: get_s(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX;

		if (swap_arms)
			r = big ? (r & 1) : 0;
		else
			r = big ? 0 : (r & 1);
	}
	return r;
}

static int
run_cd(void)
{
	static const short g94s[] = { -1, 0, 998, 999, 1000, 32767 };
	static const short errs[] = {
		-32768, -1, 0, 0x3ffe, 0x3fff, 0x4000, 32767
	};
	static const int los[] = { 0, 1, 2, 3, -1 };
	static const short his[] = { 0, 1, -1 };
	unsigned g, e, l, h, gate, ep;

	diff_begin("CarrierDetectV17");
	debug_begin(2u);
	for (g = 0; g < sizeof(g94s) / sizeof(g94s[0]); g++)
	for (e = 0; e < sizeof(errs) / sizeof(errs[0]); e++)
	for (l = 0; l < sizeof(los) / sizeof(los[0]); l++)
	for (h = 0; h < sizeof(his) / sizeof(his[0]); h++)
	for (gate = 0; gate < 2; gate++)
	for (ep = 0; ep < 2; ep++) {
		unsigned seed = 0x55550000u + g * 977 + e * 131 + l * 17
				+ h * 5 + gate * 3 + ep;
		long where = (long)g * 100000 + (long)e * 1000 + (long)l * 100
			     + (long)h * 10 + (long)gate * 2 + (long)ep;
		int ra, rb;

		fixture(&ma, seed);
		fixture(&mb, seed);
		cd_setup(&ma, los[l], his[h], (int)gate, (int)ep, g94s[g],
			 errs[e]);
		cd_setup(&mb, los[l], his[h], (int)gate, (int)ep, g94s[g],
			 errs[e]);

		dsplib_debug_capture_reset();
		ra = ref_CarrierDetectV17(ma.robj);
		rb = CarrierDetectV17(mb.robj);
		diff_eq_int("at %ld: carrier verdict", (long)rb, (long)ra,
			    where);
		compare_all(&ma, &mb, where);
		debug_compare(where);

		if (ra != 0)
			cd_true++;
		else
			cd_false++;
		if (dsplib_debug_capture_lines(1) != 0)
			cd_printed++;

		/* WRONG READING: +0xd0 read 16-bit. */
		if (drive_cd(&ma, 1, 0, 0, 0) != ra)
			cd_width_sep++;
		/* WRONG READING: the 999 gate inclusive. */
		if (drive_cd(&ma, 0, 1, 0, 0) != ra)
			cd_gate999_sep++;
		/* WRONG READING: the 0x3fff gate inclusive. */
		if (drive_cd(&ma, 0, 0, 1, 0) != ra)
			cd_gate3fff_sep++;
		/* WRONG READING: the two arms swapped. */
		if (drive_cd(&ma, 0, 0, 0, 1) != ra)
			cd_arm_sep++;
	}
	debug_end();
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* DataCarrierDetectV17                                                  */

/*
 * The private detector chain lives inside the fixture, so a whole-struct copy
 * has to be re-homed the same way `rehome` re-homes the instance pointers.
 */
static void
rehome_dcd(struct fix *f)
{
	rehome(f);
	f->mtd.acc = f->mtd_acc;
	put_ptr(f->ctl, V17RXC_MTD2, &f->mtd);
	put_ptr(f->ctl, V17RXC_BUF2, f->buf2);
}

static void
dcd_setup(struct fix *f, int use_ref)
{
	struct fpm_agc *agc;

	memset(&f->mtd, 0, sizeof(f->mtd));
	memset(f->mtd_acc, 0, sizeof(f->mtd_acc));
	memset(f->buf2, 0, sizeof(f->buf2));
	f->mtd.acc = f->mtd_acc;
	if (use_ref)
		ref_FPM_MTD_create(&f->mtd, &ref_MTDv22_CFG);
	else
		FPM_MTD_create(&f->mtd, &ref_MTDv22_CFG);

	put_ptr(f->ctl, V17RXC_MTD2, &f->mtd);
	put_ptr(f->ctl, V17RXC_BUF2, f->buf2);

	agc = (struct fpm_agc *)(void *)(f->ctl + V17RXC_AGC);
	memset(agc, 0, sizeof(*agc));
	if (use_ref)
		ref_FPM_AGC_init(agc, &ref_AGCv17_CFG, 1);
	else
		FPM_AGC_init(agc, &ref_AGCv17_CFG, 1);
}

/*
 * `DataCarrierDetectV17` with one reading changed, driven through the BLOB's
 * own callees so the only difference is the reading itself.
 */
#define DCD_FAITHFUL	0
#define DCD_INVERT	1	/* accumulate on a detection, clear on none */
#define DCD_GE_MAX	2	/* the counter's threshold inclusive        */
#define DCD_LE_DROP	3	/* the energy test inclusive                */
#define DCD_ROUND	4	/* a round-to-nearest the object has not    */
#define DCD_PERIOD3	5	/* the reference refreshed every third block */
#define DCD_MODE22	6	/* the mode selector two bytes high         */

static short
drive_dcd(struct fix *f, const short *in, unsigned short count, int variant)
{
	unsigned char *rx = f->rxs;
	unsigned char *ctl = f->ctl;
	short r;

	r = (short)(get_s(rx, V17RXS_AGC_SIGNAL)
		    & get_i(rx, V17RXS_INT_0120));

	if (get_s(ctl, variant == DCD_MODE22 ? V17RXC_SHORT_0020 + 2
					     : V17RXC_SHORT_0020) == 0) {
		if (get_i(ctl, V17RXC_INT_0010) != 0
		    && get_i(rx, V17RXS_EPOCH) != 0
		    && get_s(rx, V17RXS_SHORT_0094) > V17RXS_0094_MIN) {
			if (get_s(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX)
				r = 0;
			else
				r &= 1;
		}
	} else {
		if (get_s(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX
		    || (r & 1) == 0)
			put_s(ctl, V17RXC_SHORT_002E, 1);

		r = 1;
		if (get_s(ctl, V17RXC_SHORT_002E) != 0) {
			short *buf = (short *)get_ptr(ctl, V17RXC_BUF2);
			int hit;
			short i;

			for (i = 0; i < (int)count; i++)
				buf[i] = in[i];

			ref_FPM_AGC_agc((struct fpm_agc *)(void *)
						(ctl + V17RXC_AGC), buf, count);
			hit = ref_FPM_MTD_detect((struct fpm_mtd *)
						 get_ptr(ctl, V17RXC_MTD2),
						 buf, (short)count) != 0;
			if (variant == DCD_INVERT)
				hit = !hit;
			if (hit)
				put_s(ctl, V17RXC_OFFBAND, 0);
			else
				put_s(ctl, V17RXC_OFFBAND, (short)
				      (get_us(ctl, V17RXC_OFFBAND) + count));

			if (variant == DCD_GE_MAX
			    ? get_s(ctl, V17RXC_OFFBAND) >= V17RXC_OFFBAND_MAX
			    : get_s(ctl, V17RXC_OFFBAND) > V17RXC_OFFBAND_MAX)
				r = 0;
		}
	}

	if (get_s(rx, V17RXS_SHORT_4FB4) != 0) {
		short rms = ref_FPM_rms(in, count);
		unsigned short phase;
		int thr = (int)get_s(rx, V17RXS_RMS_REF) * V17RXS_RMS_DROP_Q15;

		thr = (variant == DCD_ROUND ? thr + 0x4000 : thr) >> 15;
		if (variant == DCD_LE_DROP ? (int)rms <= thr : (int)rms < thr)
			r = 0;

		phase = (unsigned short)(get_us(rx, V17RXS_RMS_PHASE) + 1);
		if ((short)phase == (variant == DCD_PERIOD3
				     ? 3 : V17RXS_RMS_PERIOD)) {
			put_s(rx, V17RXS_RMS_REF, rms);
			put_s(rx, V17RXS_RMS_PHASE, 0);
		} else {
			put_s(rx, V17RXS_RMS_PHASE, (short)phase);
		}
	}
	return r;
}

/* Everything a DCD call can leave behind, in one comparison against `ma`. */
static int
dcd_differs(const struct fix *w, short rw, short ra)
{
	return rw != ra
	    || get_s(w->ctl, V17RXC_OFFBAND) != get_s(ma.ctl, V17RXC_OFFBAND)
	    || get_s(w->ctl, V17RXC_SHORT_002E)
	       != get_s(ma.ctl, V17RXC_SHORT_002E)
	    || get_s(w->rxs, V17RXS_RMS_REF) != get_s(ma.rxs, V17RXS_RMS_REF)
	    || get_s(w->rxs, V17RXS_RMS_PHASE)
	       != get_s(ma.rxs, V17RXS_RMS_PHASE);
}

static short tone_in[NBLK];
static short tone_ref[NBLK];
static double tone_phase;

static void
fill_tone(int n, int hz, int amp)
{
	static const double twopi = 6.283185307179586476925286766559;
	int i;

	for (i = 0; i < n; i++) {
		tone_in[i] = (short)(amp * sin(tone_phase));
		tone_phase += twopi * (double)hz / 8000.0;
		if (tone_phase > twopi)
			tone_phase -= twopi;
	}
	memcpy(tone_ref, tone_in, (size_t)n * sizeof(tone_in[0]));
}

/*
 * The frequencies, and why these.  `MTDv22_CFG`'s bank answers ABSENT at
 * 2200 Hz and PRESENT elsewhere in band (`t_v22data.c` measured that sweep and
 * finding F3574's lesson is why it is not guessed here), and NOSIGNAL below
 * its gate.  ABSENT is ZERO, which is what ADVANCES the counter -- so a table
 * without 2200 Hz in it would leave the counter pinned and three separating
 * counts dead.
 */
static const struct { int hz; int amp; } dtones[] = {
	{ 2200, 12000 },	/* ABSENT: the counter advances             */
	{ 1200, 12000 },	/* PRESENT: the counter clears              */
	{ 2200,  9000 },
	{    0,     0 },	/* silence: NOSIGNAL, which also clears     */
	{ 2200, 14000 },
	{  400,  6000 }
};
#define NDTONE	(int)(sizeof(dtones) / sizeof(dtones[0]))

static void
dcd_state(struct fix *f, short mode, short latch, short offband, int gate,
	  int epoch, short g94, short err, short lo, short hi, short watchdog,
	  short ref)
{
	put_s(f->rxs, V17RXS_AGC_SIGNAL, lo);
	put_s(f->rxs, V17RXS_AGC_SIGNAL + 2, hi);
	put_i(f->rxs, V17RXS_INT_0120, -1);
	put_i(f->ctl, V17RXC_INT_0010, gate);
	put_i(f->rxs, V17RXS_EPOCH, epoch);
	put_s(f->rxs, V17RXS_SHORT_0094, g94);
	put_s(f->rxs, V17RXS_DEC_ERROR, err);
	put_s(f->ctl, V17RXC_SHORT_0020, mode);
	put_s(f->ctl, V17RXC_SHORT_0020 + 2, (short)~mode);
	put_s(f->ctl, V17RXC_SHORT_002E, latch);
	put_s(f->ctl, V17RXC_OFFBAND, offband);
	put_s(f->rxs, V17RXS_SHORT_4FB4, watchdog);
	put_s(f->rxs, V17RXS_RMS_REF, ref);
	put_s(f->rxs, V17RXS_RMS_PHASE, 0);
}

/*
 * A multi-block run.  Everything this function keeps -- the counter, the
 * latch, the energy reference and its phase, the AGC's gain and the
 * detector's resonators -- carries across calls, so a one-block fixture would
 * measure almost none of it (finding F8790).
 */
static void
run_dcd_seq(unsigned seed, short mode, short latch, short offband, int gate,
	    int epoch, short g94, short err, short lo, short watchdog,
	    short ref, int blocks, int n, int chase)
{
	long where = (long)mode * 1000000 + (long)offband * 100 + n;
	int i;

	fixture(&ma, seed);
	fixture(&mb, seed);
	dcd_setup(&ma, 1);
	dcd_setup(&mb, 0);
	dcd_state(&ma, mode, latch, offband, gate, epoch, g94, err, lo, 1,
		  watchdog, ref);
	dcd_state(&mb, mode, latch, offband, gate, epoch, g94, err, lo, 1,
		  watchdog, ref);

	tone_phase = 0.0;
	for (i = 0; i < blocks; i++) {
		short ra, rb, rc;
		int v;

		fill_tone(n, dtones[i % NDTONE].hz, dtones[i % NDTONE].amp);

		/*
		 * CHASING THE EXACT BOUNDARY, because a sweep never reaches it.
		 * The counter advances by `count` from wherever it is, so the
		 * values it can take are an arithmetic progression that steps
		 * straight over 0x4ff -- and `> 0x4ff` and `>= 0x4ff` differ on
		 * that one value and nowhere else.  Seeding it at 0x4ff minus
		 * the block length, on a block whose tone is the one the
		 * detector reports ABSENT, is what puts the post-update value
		 * exactly on it.  The separating count below is what says the
		 * chase worked; a silent miss would fail the run.
		 */
		if (chase && dtones[i % NDTONE].hz == 2200) {
			put_s(ma.ctl, V17RXC_OFFBAND,
			      (short)(V17RXC_OFFBAND_MAX - n));
			put_s(mb.ctl, V17RXC_OFFBAND,
			      (short)(V17RXC_OFFBAND_MAX - n));
			offband = (short)(V17RXC_OFFBAND_MAX - n);
		}

		memcpy(&msnap, &mb, sizeof(msnap));
		rehome_dcd(&msnap);

		dsplib_debug_capture_reset();
		ra = ref_DataCarrierDetectV17(ma.robj, tone_in, (unsigned short)n);
		rb = DataCarrierDetectV17(mb.robj, tone_in, (unsigned short)n);

		diff_eq_int("at %ld: data carrier verdict", (long)rb, (long)ra,
			    where + i);
		diff_eq_int("at %ld: the input block was not modified",
			    memcmp(tone_in, tone_ref,
				   (size_t)n * sizeof(tone_in[0])), 0,
			    where + i);
		compare_all(&ma, &mb, where + i);
		debug_compare(where + i);

		if (ra != 0)
			dcd_true++;
		else
			dcd_false++;
		if (get_s(ma.ctl, V17RXC_OFFBAND) != offband)
			dcd_counted++;
		if (get_s(ma.rxs, V17RXS_RMS_PHASE) == 0 && watchdog != 0)
			dcd_refreshed++;
		if (dsplib_debug_capture_lines(1) != 0)
			dcd_printed++;
		offband = get_s(ma.ctl, V17RXC_OFFBAND);

		/* The faithful local model first: it must agree exactly. */
		memcpy(&mc, &msnap, sizeof(mc));
		rehome_dcd(&mc);
		rc = drive_dcd(&mc, tone_in, (unsigned short)n, DCD_FAITHFUL);
		diff_eq_int("at %ld: the local model agrees",
			    dcd_differs(&mc, rc, ra), 0, where + i);

		for (v = DCD_INVERT; v <= DCD_MODE22; v++) {
			memcpy(&mc, &msnap, sizeof(mc));
			rehome_dcd(&mc);
			rc = drive_dcd(&mc, tone_in, (unsigned short)n, v);
			if (dcd_differs(&mc, rc, ra))
				dcd_sep[v]++;
		}
	}
}

/*
 * The energy watchdog's two remaining readings need the RMS to land EXACTLY
 * on the threshold, and no sweep of tones will do that either.
 *
 *   `rms < thr` against `rms <= thr`   differ only at rms == thr
 *   `(ref * 0x32fe) >> 15` against `(ref * 0x32fe + 0x4000) >> 15`
 *                                     differ only when the low 15 bits carry,
 *                                     and then only observably when rms sits
 *                                     between the two
 *
 * So the block is generated first, its RMS is MEASURED with the blob's own
 * `FPM_rms`, and the reference is then solved for: the `ref` whose threshold
 * is exactly that RMS, and whose rounded threshold is one higher.  Both
 * readings then flip the verdict and the faithful one does not.
 */
static int
solve_ref(short rms, int want_round_carry)
{
	int ref;

	if (rms < 0)
		return -1;
	for (ref = 0; ref <= 32767; ref++) {
		int t = (ref * V17RXS_RMS_DROP_Q15) >> 15;
		int tr = (ref * V17RXS_RMS_DROP_Q15 + 0x4000) >> 15;

		if (t == (int)rms && (!want_round_carry || tr == (int)rms + 1))
			return ref;
	}
	return -1;
}

static void
run_dcd_edge(unsigned seed, int hz, int amp, int n, int want_round_carry)
{
	long where = (long)hz * 100 + n + (want_round_carry ? 1 : 0);
	short rms, ra, rb, rc;
	int ref, v;

	fixture(&ma, seed);
	dcd_setup(&ma, 1);
	tone_phase = 0.25;
	fill_tone(n, hz, amp);
	rms = ref_FPM_rms(tone_in, (unsigned short)n);

	/*
	 * NOT EVERY BLOCK HAS A SOLUTION, and that is arithmetic rather than a
	 * fault: the threshold tops out at (32767 * 0x32fe) >> 15 == 13053, so
	 * a loud block has no reference that puts its RMS on the edge, and the
	 * rounded form needs a carry as well.  An unsolved attempt is COUNTED
	 * and reported rather than asserted -- the assertion that matters is
	 * `dcd_edge > 0` at the end, which says the edge was reached at all.
	 */
	ref = solve_ref(rms, want_round_carry);
	if (ref < 0) {
		dcd_edge_unsolved++;
		return;
	}

	fixture(&ma, seed);
	fixture(&mb, seed);
	dcd_setup(&ma, 1);
	dcd_setup(&mb, 0);
	/*
	 * The head is switched off -- mode 0 with the gate clear -- so the
	 * verdict reaching the watchdog is the AGC signal alone, and a flip
	 * there is unambiguously the watchdog's.
	 */
	dcd_state(&ma, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, (short)ref);
	dcd_state(&mb, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, (short)ref);

	memcpy(&msnap, &mb, sizeof(msnap));
	rehome_dcd(&msnap);

	dsplib_debug_capture_reset();
	ra = ref_DataCarrierDetectV17(ma.robj, tone_in, (unsigned short)n);
	rb = DataCarrierDetectV17(mb.robj, tone_in, (unsigned short)n);

	diff_eq_int("at %ld: edge verdict", (long)rb, (long)ra, where);
	diff_eq_int("at %ld: the edge block was not modified",
		    memcmp(tone_in, tone_ref, (size_t)n * sizeof(tone_in[0])),
		    0, where);
	compare_all(&ma, &mb, where);
	debug_compare(where);
	diff_eq_int("at %ld: the RMS sits exactly on the threshold",
		    (long)rms,
		    (long)(((int)ref * V17RXS_RMS_DROP_Q15) >> 15), where);
	dcd_edge++;

	memcpy(&mc, &msnap, sizeof(mc));
	rehome_dcd(&mc);
	rc = drive_dcd(&mc, tone_in, (unsigned short)n, DCD_FAITHFUL);
	diff_eq_int("at %ld: the local model agrees on the edge",
		    dcd_differs(&mc, rc, ra), 0, where);

	for (v = DCD_INVERT; v <= DCD_MODE22; v++) {
		memcpy(&mc, &msnap, sizeof(mc));
		rehome_dcd(&mc);
		rc = drive_dcd(&mc, tone_in, (unsigned short)n, v);
		if (dcd_differs(&mc, rc, ra))
			dcd_sep[v]++;
	}
}

static int
run_dcd(void)
{
	unsigned s;

	diff_begin("DataCarrierDetectV17");
	debug_begin(2u);

	/*
	 * The V.21 watch, long enough for the counter to cross 0x4ff and print.
	 * 40 blocks of 160 samples is 6,400, and the ABSENT tone is one block
	 * in six, so the counter both advances and is cleared repeatedly.
	 */
	run_dcd_seq(0xaaaa1111u, 1, 1, 0, 1, 1, 1000, 0x0100, 1, 1, 8000, 40,
		    160, 0);
	/* The same, chasing the exact 0x4ff boundary; see the note above. */
	run_dcd_seq(0xaaaa1112u, 1, 1, 0, 1, 1, 1000, 0x0100, 1, 1, 8000, 40,
		    160, 1);
	run_dcd_seq(0xaaaa1113u, 1, 1, 0, 1, 1, 1000, 0x0100, 1, 1, 8000, 24,
		    77, 1);
	/* The same, with the counter seeded just below the threshold. */
	run_dcd_seq(0xaaaa2222u, 1, 1, 0x4f0, 1, 1, 1000, 0x0100, 1, 1, 8000,
		    12, 160, 0);
	/* The latch clear, so the whole tone-watch body is skipped. */
	run_dcd_seq(0xaaaa3333u, 1, 0, 0, 1, 1, 1000, 0x0100, 1, 1, 8000, 6,
		    160, 0);
	/* The latch clear AND a verdict with bit 0 clear, which sets it. */
	run_dcd_seq(0xaaaa4444u, 1, 0, 0, 1, 1, 1000, 0x0100, 2, 1, 8000, 6,
		    160, 0);
	/* The latch clear and a decoder error over the gate, which also sets it. */
	run_dcd_seq(0xaaaa5555u, 1, 0, 0, 1, 1, 1000, 0x4000, 1, 1, 8000, 6,
		    160, 0);

	/* The CarrierDetectV17-shaped path, with and without the gates. */
	run_dcd_seq(0xaaaa6666u, 0, 0, 0, 1, 1, 1000, 0x4000, 1, 1, 8000, 8,
		    160, 0);
	run_dcd_seq(0xaaaa7777u, 0, 0, 0, 1, 1, 1000, 0x0100, 3, 1, 8000, 8,
		    160, 0);
	run_dcd_seq(0xaaaa8888u, 0, 0, 0, 0, 1, 1000, 0x4000, 1, 1, 8000, 4,
		    160, 0);
	run_dcd_seq(0xaaaa9999u, 0, 0, 0, 1, 0, 1000, 0x4000, 1, 1, 8000, 4,
		    160, 0);
	run_dcd_seq(0xaaaaaaaau, 0, 0, 0, 1, 1, 999, 0x4000, 1, 1, 8000, 4,
		    160, 0);

	/* The energy watchdog off, so the second half of the function is dead. */
	run_dcd_seq(0xaaaabbbbu, 1, 1, 0, 1, 1, 1000, 0x0100, 1, 0, 8000, 6,
		    160, 0);
	/* A high reference, so the drop fires and the message prints. */
	run_dcd_seq(0xaaaaccccu, 0, 0, 0, 0, 0, 0, 0, 1, 1, 32767, 10, 160, 0);
	/* A zero reference, so the drop can never fire. */
	run_dcd_seq(0xaaaaddddu, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 6, 160, 0);

	/* Block lengths: a short one, an odd one and a long one. */
	run_dcd_seq(0xaaaaeeeeu, 1, 1, 0, 1, 1, 1000, 0x0100, 1, 1, 8000, 8,
		    40, 0);
	run_dcd_seq(0xaaaaffffu, 1, 1, 0, 1, 1, 1000, 0x0100, 1, 1, 8000, 8,
		    77, 0);
	run_dcd_seq(0xaaab0000u, 1, 1, 0, 1, 1, 1000, 0x0100, 1, 1, 8000, 6,
		    512, 0);

	/*
	 * And the energy watchdog's exact edge, both readings, over a spread
	 * of block lengths and levels so the solved reference is not one
	 * lucky number.
	 */
	for (s = 0; s < 6; s++) {
		static const int hzs[] = { 1000, 1800, 2200, 600, 3000, 1400 };
		static const int amps[] = { 12000, 9000, 6000, 3000, 1500, 800 };
		static const int ns[] = { 160, 80, 40, 200, 120, 64 };

		run_dcd_edge(0xabcd0000u + s, hzs[s], amps[s], ns[s], 0);
		run_dcd_edge(0xabce0000u + s, hzs[s], amps[s], ns[s], 1);
	}

	debug_end();
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* QualityDetectV17                                                      */

static short
drive_qd(struct fix *f, int unsigned_n, int swap_w, int no_round,
	 int flip_judge)
{
	unsigned char *rx = f->rxs;
	short r, err, n;
	long un;

	r = (short)(get_s(rx, V17RXS_AGC_SIGNAL) & get_i(rx, V17RXS_INT_0120));
	err = get_s(rx, V17RXS_DEC_ERROR);
	if (r == 0)
		r = V17_QUALITY_UNRELIABLE;

	n = (short)get_us(rx, V17RXS_QCOUNT);
	un = unsigned_n ? (long)get_us(rx, V17RXS_QCOUNT) : (long)n;
	if (un == 0) {
		put_s(rx, V17RXS_QAVG, err);
		put_s(rx, V17RXS_QCOUNT, 1);
		return r;
	}
	if (un > V17RXS_QCOUNT_SETTLE) {
		if (un != V17RXS_QCOUNT_JUDGE)
			return r;
		if (flip_judge
		    ? get_s(rx, V17RXS_QAVG)
			  >= (short)get_us(rx, V17RXS_SHORT_4FB0)
		    : get_s(rx, V17RXS_QAVG)
			  <= (short)get_us(rx, V17RXS_SHORT_4FB0))
			put_s(rx, V17RXS_SHORT_4FB2, 1);
	} else {
		int wa = swap_w ? V17RXS_QWEIGHT_OLD : V17RXS_QWEIGHT_NEW;
		int wb = swap_w ? V17RXS_QWEIGHT_NEW : V17RXS_QWEIGHT_OLD;
		int rnd = no_round ? 0 : V17RXS_QROUND;

		put_s(rx, V17RXS_QAVG,
		      (short)(((err * wa + rnd) >> 15)
			      + ((get_s(rx, V17RXS_QAVG) * wb + rnd) >> 15)));
	}
	put_s(rx, V17RXS_QCOUNT, (short)(un + 1));
	return r;
}

/* Did the wrong reading leave different state behind than the blob did? */
static int
qd_differs(const struct fix *w)
{
	return get_s(w->rxs, V17RXS_QAVG) != get_s(ma.rxs, V17RXS_QAVG)
	    || get_s(w->rxs, V17RXS_QCOUNT) != get_s(ma.rxs, V17RXS_QCOUNT)
	    || get_s(w->rxs, V17RXS_SHORT_4FB2)
	       != get_s(ma.rxs, V17RXS_SHORT_4FB2);
}

/*
 * A multi-block run.  Finding F8790: a swapped smoothing weight is invisible
 * to any single-block fixture, so the counter is driven from `start` through
 * `blocks` consecutive calls with the error moving on every one.
 */
static void
run_qd_seq(short start, int blocks, unsigned seed, short lo, short hi)
{
	int i;
	long where = (long)start * 1000 + blocks;

	fixture(&ma, seed);
	fixture(&mb, seed);
	put_s(ma.rxs, V17RXS_AGC_SIGNAL, lo);
	put_s(mb.rxs, V17RXS_AGC_SIGNAL, lo);
	put_s(ma.rxs, V17RXS_AGC_SIGNAL + 2, hi);
	put_s(mb.rxs, V17RXS_AGC_SIGNAL + 2, hi);
	put_i(ma.rxs, V17RXS_INT_0120, -1);
	put_i(mb.rxs, V17RXS_INT_0120, -1);
	put_s(ma.rxs, V17RXS_QCOUNT, start);
	put_s(mb.rxs, V17RXS_QCOUNT, start);
	put_s(ma.rxs, V17RXS_QAVG, 0x0123);
	put_s(mb.rxs, V17RXS_QAVG, 0x0123);
	put_s(ma.rxs, V17RXS_SHORT_4FB0, 0x0400);
	put_s(mb.rxs, V17RXS_SHORT_4FB0, 0x0400);
	put_s(ma.rxs, V17RXS_SHORT_4FB2, 0);
	put_s(mb.rxs, V17RXS_SHORT_4FB2, 0);

	rng_seed(seed ^ 0x66660000u);
	for (i = 0; i < blocks; i++) {
		short err = (short)(rng_next() & 0x7fff);
		short ra, rb;
		short before_avg, before_cnt;

		put_s(ma.rxs, V17RXS_DEC_ERROR, err);
		put_s(mb.rxs, V17RXS_DEC_ERROR, err);
		before_avg = get_s(ma.rxs, V17RXS_QAVG);
		before_cnt = get_s(ma.rxs, V17RXS_QCOUNT);

		/* The state as it stands BEFORE the call, for the alternates. */
		memcpy(&msnap, &mb, sizeof(msnap));
		rehome(&msnap);

		dsplib_debug_capture_reset();
		ra = ref_QualityDetectV17(ma.robj);
		rb = QualityDetectV17(mb.robj);

		diff_eq_int("at %ld: quality verdict", (long)rb, (long)ra,
			    where + i);
		compare_all(&ma, &mb, where + i);
		debug_compare(where + i);

		if (ra == V17_QUALITY_UNRELIABLE && lo == 0)
			qd_unreliable++;
		if (get_s(ma.rxs, V17RXS_QAVG) != before_avg && before_cnt != 0)
			qd_smoothed++;
		if (before_cnt == V17RXS_QCOUNT_JUDGE)
			qd_judged++;

		/* WRONG READING: the block counter unsigned. */
		memcpy(&mc, &msnap, sizeof(mc));
		rehome(&mc);
		(void)drive_qd(&mc, 1, 0, 0, 0);
		if (qd_differs(&mc))
			qd_signed_sep++;

		/* WRONG READING: the two smoothing weights swapped. */
		memcpy(&mc, &msnap, sizeof(mc));
		rehome(&mc);
		(void)drive_qd(&mc, 0, 1, 0, 0);
		if (qd_differs(&mc))
			qd_weight_sep++;

		/* WRONG READING: no round-to-nearest. */
		memcpy(&mc, &msnap, sizeof(mc));
		rehome(&mc);
		(void)drive_qd(&mc, 0, 0, 1, 0);
		if (qd_differs(&mc))
			qd_round_sep++;

		/* WRONG READING: the judge faced the other way. */
		memcpy(&mc, &msnap, sizeof(mc));
		rehome(&mc);
		(void)drive_qd(&mc, 0, 0, 0, 1);
		if (qd_differs(&mc))
			qd_judge_sep++;

		/* And the faithful one, which must agree with the blob. */
		memcpy(&mc, &msnap, sizeof(mc));
		rehome(&mc);
		(void)drive_qd(&mc, 0, 0, 0, 0);
		diff_eq_int("at %ld: the local model agrees", qd_differs(&mc),
			    0, where + i);
	}
}

static int
run_qd(void)
{
	diff_begin("QualityDetectV17");
	debug_begin(2u);

	/* From block 0, all the way through the judge and out the far side. */
	run_qd_seq(0, 60, 0x66661111u, 1, 1);
	/* Starting mid-run, so the seeding arm is not the only one exercised. */
	run_qd_seq(1, 8, 0x66662222u, 3, -1);
	run_qd_seq(0x30, 6, 0x66663333u, 1, 0);
	run_qd_seq(0x32, 3, 0x66664444u, 1, 0x4001);
	run_qd_seq(0x33, 3, 0x66665555u, 1, 1);
	/* The zero verdict, which is what prints and what returns 2. */
	run_qd_seq(0, 5, 0x66666666u, 0, 0);
	run_qd_seq(2, 5, 0x66667777u, 0, 1);
	/*
	 * The signedness corner: a counter with bit 15 set.  A `short` reading
	 * SMOOTHS here (0x8000 is negative, so not above 0x31); an
	 * `unsigned short` reading is 32768, above 0x31 and not 0x32, and
	 * returns without touching anything.
	 */
	run_qd_seq((short)0x8000, 3, 0x66668888u, 1, 1);
	run_qd_seq((short)0xffff, 3, 0x66669999u, 1, 1);

	debug_end();
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* The four accessors                                                    */

static int
run_accessors(void)
{
	static const int epochs[] = { 0, 1, -1, 0x10000, 0x7fffffff };
	static const short errs[] = {
		-32768, -1, 0, 12, 13, 14, 0x3fff, 32767
	};
	static const int rates[] = { 0, 1, -1, 0x12345678, 0x00007fff };
	static const short saved[] = { 0, 1, -1, 32767, -32768 };
	static const unsigned short f18c[] = { 0, 1, 0xfffb, 0xfffe, 0xffff };
	unsigned i, j;
	int rc = 0;

	diff_begin("EpochDetectV17 / GetSNRV17");
	for (i = 0; i < sizeof(epochs) / sizeof(epochs[0]); i++) {
		for (j = 0; j < sizeof(errs) / sizeof(errs[0]); j++) {
			unsigned seed = 0x77770000u + i * 32 + j;
			long where = (long)i * 100 + j;
			int ea, eb;
			short sa, sb;

			fixture(&ma, seed);
			fixture(&mb, seed);
			put_i(ma.rxs, V17RXS_EPOCH, epochs[i]);
			put_i(mb.rxs, V17RXS_EPOCH, epochs[i]);
			put_s(ma.rxs, V17RXS_DEC_ERROR, errs[j]);
			put_s(mb.rxs, V17RXS_DEC_ERROR, errs[j]);

			ea = ref_EpochDetectV17(ma.robj);
			eb = EpochDetectV17(mb.robj);
			diff_eq_int("at %ld: epoch", (long)eb, (long)ea, where);
			diff_eq_int("at %ld: epoch is 0 or 1",
				    ea == 0 || ea == 1, 1, where);

			sa = ref_GetSNRV17(ma.robj);
			sb = GetSNRV17(mb.robj);
			diff_eq_int("at %ld: SNR", (long)sb, (long)sa, where);
			/*
			 * The unsigned load is FREE, and this is the assertion
			 * of that rather than a claim to have measured it: the
			 * two readings differ by 65536 and the result is
			 * truncated to a short, so they agree everywhere.
			 */
			diff_eq_int("at %ld: signed and unsigned agree",
				    (long)(short)(13 - (int)errs[j]),
				    (long)sa, where);
			snr_free_checked++;
			compare_all(&ma, &mb, where);
		}
	}
	rc |= diff_end();

	diff_begin("StoreCoefV17");
	for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		unsigned seed = 0x88880000u + i;
		long where = (long)i;
		int k;

		fixture(&ma, seed);
		fixture(&mb, seed);
		put_i(ma.rxs, V17RXS_RATE, rates[i]);
		put_i(mb.rxs, V17RXS_RATE, rates[i]);
		/*
		 * Four distinct patterns, no value shared between any two
		 * arrays or any two indices: a transposition, a short copy and
		 * a long copy all become visible.
		 */
		for (k = 0; k < COEF_SLOTS; k++) {
			ma.coef0[k] = mb.coef0[k] = (short)(0x1000 + k * 3);
			ma.coef1[k] = mb.coef1[k] = (short)(0x4000 + k * 7);
			ma.save0[k] = mb.save0[k] = (short)(0x6000 + k * 11);
			ma.save1[k] = mb.save1[k] = (short)(0x7000 + k * 13);
		}

		ref_StoreCoefV17(ma.robj);
		StoreCoefV17(mb.robj);
		compare_all(&ma, &mb, where);
		diff_eq_int("at %ld: the rate was saved",
			    (long)ma.ratesave[0], (long)(short)rates[i],
			    where);

		/* WRONG READING: 48 entries -- the last would be stale. */
		if (ma.save0[V17_COEF_N - 1] == ma.coef0[V17_COEF_N - 1]
		    && ma.save1[V17_COEF_N - 1] == ma.coef1[V17_COEF_N - 1])
			coef_n_sep++;
		/* WRONG READING: 50 entries -- one past would be copied. */
		if (ma.save0[V17_COEF_N] == (short)(0x6000 + V17_COEF_N * 11)
		    && ma.save1[V17_COEF_N] == (short)(0x7000 + V17_COEF_N * 13))
			coef_n_sep++;
		/* WRONG READING: the two arrays transposed. */
		if (ma.save0[0] == ma.coef0[0] && ma.save1[0] == ma.coef1[0]
		    && ma.coef0[0] != ma.coef1[0])
			coef_swap_sep++;
	}
	rc |= diff_end();

	diff_begin("Restore_rateV17");
	for (i = 0; i < sizeof(saved) / sizeof(saved[0]); i++) {
		for (j = 0; j < sizeof(f18c) / sizeof(f18c[0]); j++) {
			unsigned seed = 0x99990000u + i * 8 + j;
			long where = (long)i * 100 + j;

			fixture(&ma, seed);
			fixture(&mb, seed);
			ma.ratesave[0] = mb.ratesave[0] = saved[i];
			put_s(ma.rxs, V17RXS_USHORT_018C, (short)f18c[j]);
			put_s(mb.rxs, V17RXS_USHORT_018C, (short)f18c[j]);

			ref_Restore_rateV17(ma.robj);
			Restore_rateV17(mb.robj);
			compare_all(&ma, &mb, where);
			diff_eq_int("at %ld: the rate came back signed",
				    (long)get_i(ma.rxs, V17RXS_RATE),
				    (long)saved[i], where);
			diff_eq_int("at %ld: +0x1f8 is +0x18c plus five",
				    (long)get_s(ma.rxs, V17RXS_SHORT_01F8),
				    (long)(short)(f18c[j] + 5), where);

			/* WRONG READING: the saved rate read unsigned. */
			if (get_i(ma.rxs, V17RXS_RATE)
			    != (int)(unsigned short)saved[i])
				restore_sep++;
		}
	}
	rc |= diff_end();
	return rc;
}

/* --------------------------------------------------------------------- */
/* V17RX_status                                                          */

/*
 * A REPORTER WITH NO ARITHMETIC, so what can be wrong is WHICH FIELD and WHICH
 * WAY ROUND, and every check below is built around a named wrong reading whose
 * separating count is asserted non-zero at the end.
 *
 * TWO OF THE READINGS NEEDED THE FIXTURE CHANGED TO SEPARATE AT ALL.  The
 * flags byte's bits 3 and 5 are `sete` on two state ints, and a pseudorandom
 * int is non-zero every time -- so with the blocks left as `fixture()` fills
 * them, taking bit 5 from the wrong offset produced the same 0 as taking it
 * from the right one and the count read zero.  The sweep therefore drives all
 * four combinations of (`V17RXS_INT_0000`, `V17RXS_INT_0010`) being zero, and
 * plants the OPPOSITE value at the neighbouring offset a wrong reading would
 * pick up.  Finding F9104.
 */
enum rxs_defect {
	S_NONE = 0,
	S_SNR_ZERO,		/* +0x08 zeroed, which is V17TX_status's    */
	S_06_NOT_INV,		/* +0x06 not inverted                       */
	S_06_BIT0,		/* +0x06 from bit 0 of the result byte      */
	S_RXBPS_02,		/* +0x04 taken from the instance's +0x02    */
	S_12_ZERO,		/* +0x12 zeroed rather than copied          */
	S_0C_WRITTEN,		/* +0x0c zeroed, which the object skips     */
	S_18_WRITTEN,		/* +0x18 written, which the object skips    */
	S_1C_BIT1,		/* the state byte's bit 1, not its bit 0    */
	S_0000_NOT_INV,		/* flags bit 3 not inverted                 */
	S_0010_OFF,		/* flags bit 5 from the state's +0x0c       */
	S_NO_10,		/* 0x10 not forced set                      */
	S_NO_40,		/* 0x40 not forced set                      */
	S_NO_80,		/* 0x80 not forced clear                    */
	S_15_ALL,		/* +0x15 cleared outright, not bit 0        */
	S_MAX
};

static long rxs_sep[S_MAX];
static long rxs_null_sep;
static long rxs_paths[4];

/* The object's own sequence, with one reading changed. */
static int
drive_rxstatus(struct fix *f, enum rxs_defect d)
{
	struct v17_status *status = (struct v17_status *)(void *)f->sta;
	unsigned char *rx = f->robj;
	unsigned char *rxs;
	int bit;

	status->protocol = (short)get_us(rx, V17RX_OBJ_PROTOCOL);
	status->tx_bps = 0;
	status->rx_bps = (short)get_us(rx, (d == S_RXBPS_02)
					   ? 0x02 : V17RX_OBJ_RX_BPS);
	bit = (d == S_06_BIT0) ? 0x01 : V17RX_FLAG_LOW_SNR;
	status->short_06 = (short)((d == S_06_NOT_INV)
				   ? ((rx[V17RX_OBJ_RESULT_B1] & bit) != 0)
				   : ((rx[V17RX_OBJ_RESULT_B1] & bit) == 0));
	status->snr = (d == S_SNR_ZERO) ? 0 : ref_GetSNRV17(f->robj);
	status->short_0a = 0;
	if (d == S_0C_WRITTEN)
		status->short_0c = 0;
	if (d == S_18_WRITTEN)
		status->int_18 = get_i(rx, 0x18);
	status->short_0e = 0;
	status->short_10 = 0;
	status->short_12 = (d == S_12_ZERO)
			   ? 0 : (short)get_us(rx, V17RX_OBJ_RX_BPS);

	rxs = (unsigned char *)get_ptr(rx, V17RX_OBJ_STATE);
	status->flags &= (unsigned char)~V17_STATUS_FLAG_01;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_02)
		 | ((rxs[V17RXS_BYTE_001C] & ((d == S_1C_BIT1) ? 2 : 1))
		    ? 2 : 0));
	status->flags &= (unsigned char)~V17_STATUS_FLAG_04;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_08)
		 | (((d == S_0000_NOT_INV)
		     ? (get_i(rxs, V17RXS_INT_0000) != 0)
		     : (get_i(rxs, V17RXS_INT_0000) == 0)) << 3));
	if (d != S_NO_10)
		status->flags |= V17_STATUS_FLAG_10;
	status->flags1 &= (unsigned char)
		(d == S_15_ALL ? 0x00 : ~V17_STATUS_FLAGS1_CLEAR);
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_20)
		 | ((get_i(rxs, (d == S_0010_OFF) ? 0x0c : V17RXS_INT_0010)
		     == 0) << 5));
	if (d != S_NO_40)
		status->flags |= V17_STATUS_FLAG_40;
	if (d != S_NO_80)
		status->flags &= (unsigned char)~V17_STATUS_FLAG_80;

	return 1;
}

static unsigned long
rxs_mark(const struct fix *f, int ret)
{
	unsigned long m = (unsigned long)ret;
	int i;

	for (i = 0; i < STA_SIZE; i++)
		m = m * 131u + f->sta[i];
	return m;
}

/*
 * `zero_sel` names which of the two state ints the flags byte tests are zero
 * on this trial; the neighbour a wrong reading would pick up is planted with
 * the complement, so an offset that is wrong by four bytes reports a different
 * bit.
 */
static void
rxstatus_state(struct fix *f, int zero_sel)
{
	put_i(f->rxs, V17RXS_INT_0000, (zero_sel & 1) ? 0 : 0x51ee7);
	put_i(f->rxs, V17RXS_INT_0010, (zero_sel & 2) ? 0 : 0x0d15c);
	put_i(f->rxs, 0x0c, (zero_sel & 2) ? 0x0d15c : 0);
}

static int
run_rxstatus(void)
{
	long where = 0;
	int zero_sel, trial, d;

	diff_begin("V17RX_status");

	for (zero_sel = 0; zero_sel < 4; zero_sel++)
	for (trial = 0; trial < 6; trial++) {
		unsigned seed = spread(0x0517a700u, where);
		unsigned long marka, markc;
		int ra, rb;

		fixture(&ma, seed);
		fixture(&mb, seed);
		rxstatus_state(&ma, zero_sel);
		rxstatus_state(&mb, zero_sel);

		ra = ref_V17RX_status(ma.robj, (struct v17_status *)
					       (void *)ma.sta);
		rb = V17RX_status(mb.robj, (struct v17_status *)
					   (void *)mb.sta);

		diff_eq_int("at %ld: V17RX_status returned", (long)rb,
			    (long)ra, where);
		compare_all(&ma, &mb, where);

		if (ma.sta[0x14] & V17_STATUS_FLAG_02)
			rxs_paths[0]++;
		else
			rxs_paths[1]++;
		if (ma.sta[0x14] & V17_STATUS_FLAG_08)
			rxs_paths[2]++;
		if (ma.sta[0x14] & V17_STATUS_FLAG_20)
			rxs_paths[3]++;

		marka = rxs_mark(&ma, ra);
		for (d = 1; d < (int)S_MAX; d++) {
			fixture(&mc, seed);
			rxstatus_state(&mc, zero_sel);
			markc = rxs_mark(&mc, drive_rxstatus(&mc,
						(enum rxs_defect)d));
			if (markc != marka)
				rxs_sep[d]++;
		}

		/* The NULL guard, separated by construction. */
		fixture(&ma, seed);
		fixture(&mb, seed);
		ra = ref_V17RX_status(ma.robj, 0);
		rb = V17RX_status(mb.robj, 0);
		diff_eq_int("at %ld: the NULL guard returned", (long)rb,
			    (long)ra, where);
		diff_eq_int("at %ld: the NULL guard returned 0", (long)ra, 0,
			    where);
		compare_all(&ma, &mb, where);
		if (ra == 0)
			rxs_null_sep++;

		where++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* ScrambleDataV17 and DescrambleDataV17                                 */

/*
 * TWO ADAPTERS ONTO ONE PAIR OF PRIMITIVES, and what a wrong reading gets
 * wrong is the OBJECT they reach: the scrambler is at `V17FP_SDM` in the
 * transmitter's private block and the descrambler is at `V17RXS_SDM`, 0x4f8c
 * into the receiver's demodulator state.  So the fixture builds FOUR live
 * `struct fpm_sdm`, each with its own shift register -- the two right ones and
 * one decoy beside each -- and every named wrong reading picks up one of the
 * other three.
 *
 * THE DECOYS ARE A WHOLE `sizeof(struct fpm_sdm)` AWAY, NOT FOUR BYTES, AND
 * THAT IS DELIBERATE.  0x18 is the struct's size, so a decoy at +/-4 would
 * OVERLAP the real object and the second `SDM_init` would overwrite the first
 * one's `tap2` with the second one's `nbits` -- corrupting the object under
 * test rather than providing an alternative to it.  The check this buys is
 * "the right one of two valid objects in the right block", which is what an
 * offset error would get wrong; it is not a claim that +/-4 was tried.
 *
 * F8790's rule applies even here: the register is the whole state, so a run
 * that scrambled one block would agree with a run that scrambled the wrong
 * object's first block.  Each trial drives SIX consecutive blocks.
 */
#define SCR_WORDS	64
#define SCR_BLOCKS	6
#define SCR_FP_DECOY	0x34		/* V17FP_SDM + sizeof(struct fpm_sdm) */
#define SCR_RXS_DECOY	0x4f74		/* V17RXS_SDM - sizeof(struct fpm_sdm) */

static unsigned short scr_a[SCR_WORDS], scr_b[SCR_WORDS], scr_c[SCR_WORDS];
static unsigned short scr_in[SCR_WORDS];

enum scr_defect {
	C_NONE = 0,
	C_TX_DECOY,		/* the scrambler one struct further on      */
	C_TX_ON_STATE,		/* the scrambler read from the RX state     */
	C_TX_DESCRAMBLES,	/* the scrambler calls SDM_descrambler      */
	C_RX_DECOY,		/* the descrambler one struct earlier       */
	C_RX_ON_FP,		/* the descrambler read from the TX block   */
	C_RX_SCRAMBLES,		/* the descrambler calls SDM_scrambler      */
	C_MAX
};

static long scr_sep[C_MAX];
static long scr_words_moved;

static void
scr_one(unsigned char *block, int off, const struct fpm_sdm_cfg *cfg,
	unsigned int reg)
{
	struct fpm_sdm *s = (struct fpm_sdm *)(void *)(block + off);

	ref_SDM_init(s, cfg);
	s->reg = reg;			/* no two alike, so none can agree */
}

static void
scr_build(struct fix *f, unsigned seed)
{
	struct fpm_sdm_cfg cfg;

	fixture(f, seed);

	/*
	 * V.17's own polynomial, from `sdm.h`: every caller patches SDM_CFG's
	 * nbits and sets the taps to 0x12 and 0x17.
	 */
	cfg.nbits = 8;
	cfg.tap1 = 0x12;
	cfg.tap2 = 0x17;

	scr_one(f->fp, V17FP_SDM, &cfg, 0x1111u);
	scr_one(f->fp, SCR_FP_DECOY, &cfg, 0x2222u);
	scr_one(f->rxs, V17RXS_SDM, &cfg, 0x3333u);
	scr_one(f->rxs, SCR_RXS_DECOY, &cfg, 0x4444u);
}

static void
drive_scr(struct fix *f, unsigned short *data, unsigned short n, int descr,
	  enum scr_defect d)
{
	/*
	 * TWO INSTANCES, AND THIS IS WHERE F8850 BITES.  The scrambler hangs
	 * off the TRANSMIT instance's `V17TX_OBJ_FP` and the descrambler off
	 * the RECEIVE instance's `V17RX_OBJ_STATE` -- which are offsets 0x28
	 * and 0x60 of two different structs.  Reading both from one instance
	 * takes 0x28 of the receive one, which is `V17RX_OBJ_RESULT`, an int
	 * and not a pointer.
	 */
	unsigned char *fp = (unsigned char *)get_ptr(f->tobj, V17TX_OBJ_FP);
	unsigned char *rxs = (unsigned char *)get_ptr(f->robj,
						      V17RX_OBJ_STATE);
	struct fpm_sdm *s;

	if (!descr) {
		s = (d == C_TX_DECOY)
			? (struct fpm_sdm *)(void *)(fp + SCR_FP_DECOY)
			: (d == C_TX_ON_STATE)
			? (struct fpm_sdm *)(void *)(rxs + V17RXS_SDM)
			: (struct fpm_sdm *)(void *)(fp + V17FP_SDM);
		if (d == C_TX_DESCRAMBLES)
			ref_SDM_descrambler(s, data, n);
		else
			ref_SDM_scrambler(s, data, n);
	} else {
		s = (d == C_RX_DECOY)
			? (struct fpm_sdm *)(void *)(rxs + SCR_RXS_DECOY)
			: (d == C_RX_ON_FP)
			? (struct fpm_sdm *)(void *)(fp + V17FP_SDM)
			: (struct fpm_sdm *)(void *)(rxs + V17RXS_SDM);
		if (d == C_RX_SCRAMBLES)
			ref_SDM_scrambler(s, data, n);
		else
			ref_SDM_descrambler(s, data, n);
	}
}

static int
run_scramble(void)
{
	static const unsigned short counts[] = { 1, 5, 32, SCR_WORDS };
	int descr, c, blk, d, i;
	long where = 0;

	diff_begin("ScrambleDataV17 / DescrambleDataV17");

	for (descr = 0; descr < 2; descr++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		unsigned seed = spread(0x05c12b00u, where);
		unsigned short n = counts[c];
		unsigned long marka = 0, markc;

		scr_build(&ma, seed);
		scr_build(&mb, seed);

		rng_seed(seed ^ 0xd1eu);
		for (i = 0; i < SCR_WORDS; i++)
			scr_in[i] = (unsigned short)rng_next();

		for (blk = 0; blk < SCR_BLOCKS; blk++) {
			long id = where * 100 + blk;

			for (i = 0; i < SCR_WORDS; i++)
				scr_a[i] = scr_b[i] = (unsigned short)
					(scr_in[i] + (unsigned)blk);

			if (descr) {
				ref_DescrambleDataV17(ma.robj, scr_a, n);
				DescrambleDataV17(mb.robj, scr_b, n);
			} else {
				ref_ScrambleDataV17(ma.tobj, scr_a, n);
				ScrambleDataV17(mb.tobj, scr_b, n);
			}

			for (i = 0; i < SCR_WORDS; i++) {
				diff_eq_int("at %ld: word", (long)scr_b[i],
					    (long)scr_a[i], id);
				if (scr_a[i] != (unsigned short)
						(scr_in[i] + (unsigned)blk))
					scr_words_moved++;
			}
			compare_all(&ma, &mb, id);

			marka = marka * 1000003u + n;
			for (i = 0; i < SCR_WORDS; i++)
				marka = marka * 31u + scr_a[i];
		}

		for (d = 1; d < (int)C_MAX; d++) {
			scr_build(&mc, seed);
			markc = 0;
			for (blk = 0; blk < SCR_BLOCKS; blk++) {
				for (i = 0; i < SCR_WORDS; i++)
					scr_c[i] = (unsigned short)
						(scr_in[i] + (unsigned)blk);
				drive_scr(&mc, scr_c, n, descr,
					  (enum scr_defect)d);
				markc = markc * 1000003u + n;
				for (i = 0; i < SCR_WORDS; i++)
					markc = markc * 31u + scr_c[i];
			}
			if (markc != marka)
				scr_sep[d]++;
		}

		where++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V17RX_delete                                                          */

/*
 * WHAT A DELETE TEST CAN ACTUALLY CHECK is which pointers were released, and
 * `t_v29fax.c`'s `V29RX_delete` block is the shape this follows.
 *
 * The three BLOCKS -- the instance, the control block and the demodulator
 * state -- are members of a static fixture, so the object's frees of them
 * reach `sysdep_free` as pointers the allocator never handed out: they land in
 * `bad_free` and are counted rather than destroying the fixture.  The TEN
 * things that ARE allocated are probed one at a time with
 * `harness_alloc_ordinal`, which reports 0 for a pointer no longer live.
 *
 * The three embedded FPM states are zeroed so their `_free` functions release
 * null pointers and land in `free_null` rather than chasing the block's
 * pseudorandom filler.  That the object calls them AT ALL is what `free_null`
 * counts.
 */
#define N_PROBE		10
#define DEL_BUF		256

static void
delete_build(struct fix *f, unsigned seed, void *probe[N_PROBE])
{
	struct sgd *s;

	fixture(f, seed);

	s = ref_SGD_create(0, 0);
	put_ptr(f->rxs, V17RXS_SGD, s);
	put_ptr(f->rxs, V17RXS_PTR_0030, sysdep_malloc(DEL_BUF));
	put_ptr(f->rxs, V17RXS_BUF_MRF, sysdep_malloc(DEL_BUF));
	put_ptr(f->rxs, V17RXS_BUF_SRE, sysdep_malloc(DEL_BUF));

	memset(f->rxs + V17RXS_MRF, 0, 0x1c);
	memset(f->rxs + V17RXS_SRE, 0, 0x90);
	memset(f->rxs + V17RXS_FSE, 0, 0x4e18);

	put_ptr(f->ctl, V17RXC_MTD, ref_FPM_MTD_create(0, 0));
	put_ptr(f->ctl, V17RXC_TONE, ref_FPM_TONE_create(0, 0));
	put_ptr(f->ctl, V17RXC_SCRATCH, sysdep_malloc(DEL_BUF));
	put_ptr(f->ctl, V17RXC_BUF2, sysdep_malloc(DEL_BUF));
	put_ptr(f->ctl, V17RXC_MTD2, ref_FPM_MTD_create(0, 0));

	probe[0] = s;
	probe[1] = s->hist;
	probe[2] = get_ptr(f->rxs, V17RXS_PTR_0030);
	probe[3] = get_ptr(f->rxs, V17RXS_BUF_MRF);
	probe[4] = get_ptr(f->rxs, V17RXS_BUF_SRE);
	probe[5] = get_ptr(f->ctl, V17RXC_MTD);
	probe[6] = get_ptr(f->ctl, V17RXC_TONE);
	probe[7] = get_ptr(f->ctl, V17RXC_SCRATCH);
	probe[8] = get_ptr(f->ctl, V17RXC_BUF2);
	probe[9] = get_ptr(f->ctl, V17RXC_MTD2);
}

static long del_released, del_probes;

static int
run_rxdelete(void)
{
	void *pa[N_PROBE], *pb[N_PROBE];
	int liva[N_PROBE], livb[N_PROBE];
	struct alloc_log before, mid, after;
	int i, s, sum;
	static const unsigned seeds[] = { 0x0de1e7a0u, 0x0de1e7a1u };

	diff_begin("V17RX_delete");

	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++) {
		delete_build(&ma, seeds[s], pa);
		before = harness_alloc;
		ref_V17RX_delete(ma.robj);
		mid = harness_alloc;
		for (i = 0; i < N_PROBE; i++)
			liva[i] = harness_alloc_ordinal(pa[i]) != 0;

		delete_build(&mb, seeds[s], pb);
		V17RX_delete(mb.robj);
		after = harness_alloc;
		for (i = 0; i < N_PROBE; i++)
			livb[i] = harness_alloc_ordinal(pb[i]) != 0;

		sum = 0;
		for (i = 0; i < N_PROBE; i++) {
			diff_eq_int("probe %ld still live", livb[i], liva[i],
				    i);
			sum += liva[i];
			del_probes++;
			if (liva[i] == 0)
				del_released++;
		}
		diff_eq_int("seed %ld: everything the delete owns was released",
			    sum, 0, s);

		diff_eq_int("seed %ld: frees", after.frees - mid.frees,
			    mid.frees - before.frees, s);
		diff_eq_int("seed %ld: null frees",
			    after.free_null - mid.free_null,
			    mid.free_null - before.free_null, s);
		diff_eq_int("seed %ld: unknown frees",
			    after.bad_free - mid.bad_free,
			    mid.bad_free - before.bad_free, s);
		diff_eq_int("seed %ld: the three FPM states were freed",
			    (mid.free_null - before.free_null) > 0, 1, s);
		diff_eq_int("seed %ld: the three static blocks were freed",
			    (mid.bad_free - before.bad_free) > 0, 1, s);
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* DemodDataV17                                                          */

/*
 * THE ONLY SYMBOL HERE THAT NEEDS A LIVE DSP CHAIN, and the fixture is most of
 * the work.  `DemodDataV17` calls, in order, `FPM_AGC_agc`, `FPM_TONE_kill`,
 * `FPM_MTD_detect`, `FPM_MRF_filter`, `FPM_SRE_recover` and `FPM_FSE_receive`,
 * so a trial needs six constructed objects at their real offsets inside the
 * demodulator state and three scratch buffers big enough for what they
 * produce.
 *
 * D955's rule is why none of them may be faked: this is table-lookup code, and
 * a field left unplanted that is used as a SUBSCRIPT cannot be caught by a
 * blob-against-blob dry run -- both sides read the same wild index and agree.
 * Every one of the six is constructed by the BLOB's own `_init` or `_create`,
 * on both sides, so the states are identical bytes and any difference the test
 * sees belongs to `src/fax/v17.c`.
 *
 * F8790's rule is why each trial is EIGHT consecutive blocks with the state
 * carried across: the MRF, the SRE and the FSE all hold history, and a
 * one-block fixture cannot see a swapped weight or a buffer used for the wrong
 * stage.
 *
 * THE THREE ENABLE WORDS ARE SEEDED SO THAT ONE OF THEM HAS BIT 0 CLEAR.
 * `agc.signal` is 0 or 1, so `signal & word` can only be 0 or `word & 1`; with
 * all three words odd, transposing two of them is invisible.  F8885, borrowed
 * whole from `t_v29fax.c`, which measured it.
 */
#define DEM_BUF		4096
#define DEM_BLOCKS	8
#define DEM_TAPS	16
#define DEM_CLK		8

static short dem_icoff[DEM_TAPS], dem_qcoff[DEM_TAPS];
static short dem_clk[DEM_CLK], dem_k1[3], dem_k2[3];
static struct fpm_fse_cfg dem_fse_cfg;

static short dem_mrf_a[DEM_BUF], dem_mrf_b[DEM_BUF], dem_mrf_c[DEM_BUF];
static short dem_sre_a[DEM_BUF], dem_sre_b[DEM_BUF], dem_sre_c[DEM_BUF];
static short dem_det_a[DEM_BUF], dem_det_b[DEM_BUF], dem_det_c[DEM_BUF];
static unsigned short dem_out_a[DEM_BUF], dem_out_b[DEM_BUF];
static short dem_in[DEM_BUF], dem_work[DEM_BUF];

static short dem_slice_perr, dem_slice_mag;

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
 * The pointer fields the two sides cannot agree on, because each side's own
 * `_init` allocated them.  Everything else in twenty kilobytes IS compared,
 * including the equaliser's two scatter logs.
 */
static long
rxs_diff_demod(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < RXS_SIZE; i++) {
		if (i >= V17RXS_COEF0 && i < V17RXS_COEF1 + 4)
			continue;
		if (i >= V17RXS_MRF + 0x18 && i < V17RXS_MRF + 0x1c)
			continue;		/* fpm_mrf::history        */
		if (i >= V17RXS_SRE + 0x50 && i < V17RXS_SRE + 0x5c)
			continue;		/* coeff, hist, clk        */
		if (i >= V17RXS_SRE + 0x74 && i < V17RXS_SRE + 0x78)
			continue;		/* rms_buf                 */
		if (i >= V17RXS_FSE + 0x54 && i < V17RXS_FSE + 0x5c)
			continue;		/* out_i, out_q            */
		if (i >= V17RXS_FSE + 0x60 && i < V17RXS_FSE + 0x6c)
			continue;		/* icoeff, qcoeff, hist    */
		if (i >= V17RXS_BUF_MRF && i < V17RXS_BUF_SRE + 4)
			continue;		/* the two chained buffers */
		if (a->rxs[i] != b->rxs[i])
			return i;
	}
	return -1;
}

static long
ctl_diff_demod(const struct fix *a, const struct fix *b)
{
	int i;

	for (i = 0; i < CTL_SIZE; i++) {
		if (i < V17RXC_TONE + 4)
			continue;		/* the MTD and the notch   */
		if (i >= V17RXC_PROCESS && i < V17RXC_PROCESS + 4)
			continue;
		if (i >= V17RXC_SCRATCH && i < V17RXC_SCRATCH + 4)
			continue;
		if (i >= V17RXC_MTD2 && i < V17RXC_BUF2 + 4)
			continue;
		if (a->ctl[i] != b->ctl[i])
			return i;
	}
	return -1;
}

struct dem_setup {
	short	gate_18;	/* non-zero skips the tone pre-pass        */
	int	tone;		/* index into dtones[], the pre-pass input */
	int	enables;	/* which shape of the three enable words   */
};

static const int dem_enable[2][3] = {
	{ 0x0e, 0x33, 0x54 },		/* adapt off, pll on,  lms off */
	{ 0x33, 0x54, 0x0f }		/* adapt on,  pll off, lms on  */
};

static void
dem_build(struct fix *f, unsigned seed, const struct dem_setup *u,
	  short *mrfbuf, short *srebuf, short *detbuf)
{
	fixture(f, seed);

	put_ptr(f->rxs, V17RXS_BUF_MRF, mrfbuf);
	put_ptr(f->rxs, V17RXS_BUF_SRE, srebuf);
	put_ptr(f->ctl, V17RXC_SCRATCH, detbuf);
	memset(mrfbuf, 0, DEM_BUF * sizeof(short));
	memset(srebuf, 0, DEM_BUF * sizeof(short));
	memset(detbuf, 0, DEM_BUF * sizeof(short));

	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->rxs + V17RXS_AGC),
			 &ref_AGCv17_CFG, 1);
	ref_FPM_MRF_init((struct fpm_mrf *)(void *)(f->rxs + V17RXS_MRF),
			 &MRFv32_CFG, 1);
	ref_FPM_SRE_init((struct fpm_sre *)(void *)(f->rxs + V17RXS_SRE),
			 &SREv32_CFG, 1);
	ref_FPM_FSE_init((struct fpm_fse *)(void *)(f->rxs + V17RXS_FSE),
			 &dem_fse_cfg, 1);

	put_ptr(f->ctl, V17RXC_MTD, ref_FPM_MTD_create(0, &ref_MTDv22_CFG));
	put_ptr(f->ctl, V17RXC_TONE, ref_FPM_TONE_create(0, 0));

	put_s(f->ctl, V17RXC_SHORT_0018, u->gate_18);
	put_i(f->rxs, V17RXS_INT_0004, dem_enable[u->enables][0]);
	put_i(f->rxs, V17RXS_INT_0008, dem_enable[u->enables][1]);
	put_i(f->rxs, V17RXS_INT_0010, dem_enable[u->enables][2]);
}

static void
dem_free(struct fix *f)
{
	ref_FPM_MRF_free((struct fpm_mrf *)(void *)(f->rxs + V17RXS_MRF));
	ref_FPM_SRE_free((struct fpm_sre *)(void *)(f->rxs + V17RXS_SRE));
	ref_FPM_FSE_free((struct fpm_fse *)(void *)(f->rxs + V17RXS_FSE));
	ref_FPM_MTD_delete((struct fpm_mtd *)get_ptr(f->ctl, V17RXC_MTD));
	ref_FPM_TONE_delete(get_ptr(f->ctl, V17RXC_TONE));
}

/* The named wrong readings, one changed thing each. */
enum dem_defect {
	M_NONE = 0,
	M_HALVE,		/* the pre-pass copies with a >> 1.  F9103  */
	M_KILL_INPUT,		/* the notch applied to `in`, not the copy  */
	M_DETECT_INPUT,		/* the detector fed `in`, not the copy      */
	M_NO_ABANDON,		/* a tone detection does not abandon        */
	M_GATE_INVERTED,	/* the pre-pass runs when the gate is set   */
	M_SRE_FROM_INPUT,	/* the recoverer fed `in`                   */
	M_FSE_FROM_MRF,		/* the equaliser fed the resampler's buffer */
	M_FSE_COUNT,		/* the equaliser given the resampler's count */
	M_ADAPT_SOURCE,		/* sre.adapt taken from the wrong enable    */
	M_TILT_NOT_CLEARED,	/* fse.tilt_on left alone                   */
	M_LMS_SOURCE,		/* fse.lms_on and fse.pll_on transposed     */
	M_SIGNAL_ONE,		/* the carrier bit forced to 1              */
	M_MAX
};

/* The object's own sequence, with one reading changed. */
static unsigned short
drive_demod(struct fix *f, short *in, unsigned short *out, unsigned short count,
	    enum dem_defect d)
{
	unsigned char *rxs = f->rxs;
	unsigned char *ctl = f->ctl;
	int signal;
	unsigned short n;
	int gate = (get_s(ctl, V17RXC_SHORT_0018) == 0);

	ref_FPM_AGC_agc((struct fpm_agc *)(void *)(rxs + V17RXS_AGC), in,
			count);
	signal = ((struct fpm_agc *)(void *)(rxs + V17RXS_AGC))->signal;
	if (d == M_SIGNAL_ONE)
		signal = 1;

	if (d == M_GATE_INVERTED)
		gate = !gate;

	if (gate) {
		short *buf = (short *)get_ptr(ctl, V17RXC_SCRATCH);
		unsigned short i;

		for (i = 0; i < count; i++) {
			if (d == M_HALVE)
				buf[i] = (short)(in[i] >> 1);
			else
				buf[i] = in[i];
		}

		ref_FPM_TONE_kill(get_ptr(ctl, V17RXC_TONE),
				  (d == M_KILL_INPUT) ? in : buf,
				  (short)count);

		if (ref_FPM_MTD_detect((struct fpm_mtd *)
					get_ptr(ctl, V17RXC_MTD),
				       (d == M_DETECT_INPUT) ? in : buf,
				       (short)count) != 0
		    && d != M_NO_ABANDON)
			return 0;
	}

	n = (unsigned short)ref_FPM_MRF_filter(
			(struct fpm_mrf *)(void *)(rxs + V17RXS_MRF), in,
			(short *)get_ptr(rxs, V17RXS_BUF_MRF), (short)count);

	put_i(rxs, V17RXS_SRE_ADAPT,
	      signal & get_i(rxs, (d == M_ADAPT_SOURCE) ? V17RXS_INT_0008
							: V17RXS_INT_0004));

	n = ref_FPM_SRE_recover((struct fpm_sre *)(void *)(rxs + V17RXS_SRE),
				(d == M_SRE_FROM_INPUT)
					? (const short *)in
					: (const short *)get_ptr(rxs,
							V17RXS_BUF_MRF),
				(short *)get_ptr(rxs, V17RXS_BUF_SRE),
				(short)n);

	if (d != M_TILT_NOT_CLEARED)
		put_i(rxs, V17RXS_FSE_TILT_ON, 0);
	put_i(rxs, V17RXS_FSE_PLL_ON,
	      signal & get_i(rxs, (d == M_LMS_SOURCE) ? V17RXS_INT_0010
						      : V17RXS_INT_0008));
	put_i(rxs, V17RXS_FSE_LMS_ON,
	      signal & get_i(rxs, (d == M_LMS_SOURCE) ? V17RXS_INT_0008
						      : V17RXS_INT_0010));

	return ref_FPM_FSE_receive(
			(struct fpm_fse *)(void *)(rxs + V17RXS_FSE),
			(const short *)get_ptr(rxs,
				(d == M_FSE_FROM_MRF) ? V17RXS_BUF_MRF
						      : V17RXS_BUF_SRE),
			out, (d == M_FSE_COUNT) ? count : n);
}

static long dem_sep[M_MAX], dem_paths[6], dem_agc_checked;

/*
 * A trial: DEM_BLOCKS consecutive blocks through both sides, compared after
 * every one.  The defect replays the WHOLE sequence from a fresh fixture, so a
 * reading that only diverges once the state has built up is still caught.
 */
static void
run_demod_one(unsigned seed, const struct dem_setup *u, unsigned short count,
	      long where)
{
	unsigned long marka = 0, markc;
	int blk, d, i;

	dem_build(&ma, seed, u, dem_mrf_a, dem_sre_a, dem_det_a);
	dem_build(&mb, seed, u, dem_mrf_b, dem_sre_b, dem_det_b);

	tone_phase = 0.0;
	for (blk = 0; blk < DEM_BLOCKS; blk++) {
		unsigned short ra, rb;
		long id = where * 100 + blk;

		fill_tone((int)count, dtones[u->tone].hz, dtones[u->tone].amp);
		for (i = 0; i < DEM_BUF; i++)
			dem_in[i] = (i < (int)count) ? tone_in[i] : 0;

		for (i = 0; i < DEM_BUF; i++) {
			dem_out_a[i] = dem_out_b[i] = 0xbeef;
			dem_work[i] = dem_in[i];
		}
		ra = ref_DemodDataV17(ma.robj, dem_work, dem_out_a, count);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		rb = DemodDataV17(mb.robj, dem_work, dem_out_b, count);

		diff_eq_int("at %ld: DemodDataV17 returned", (long)rb,
			    (long)ra, id);
		diff_eq_int("at %ld: the return fits the buffer",
			    ra < DEM_BUF, 1, id);
		if (ra >= DEM_BUF)
			return;
		for (i = 0; i < (int)ra; i++)
			diff_eq_int("word %ld", (long)dem_out_b[i],
				    (long)dem_out_a[i], i);
		diff_eq_int("at %ld: nothing past the returned count",
			    dem_out_a[ra] == 0xbeef, 1, id);
		diff_eq_int("at %ld: first differing receive-instance byte",
			    robj_diff(&mb, &ma), -1, id);
		diff_eq_int("at %ld: first differing control-block byte",
			    ctl_diff_demod(&mb, &ma), -1, id);
		diff_eq_int("at %ld: first differing demodulator-state byte",
			    rxs_diff_demod(&mb, &ma), -1, id);
		diff_eq_int("at %ld: the resampler's buffer",
			    first_diff((const unsigned char *)dem_mrf_b,
				       (const unsigned char *)dem_mrf_a,
				       DEM_BUF * 2), -1, id);
		diff_eq_int("at %ld: the recoverer's buffer",
			    first_diff((const unsigned char *)dem_sre_b,
				       (const unsigned char *)dem_sre_a,
				       DEM_BUF * 2), -1, id);
		diff_eq_int("at %ld: the pre-pass buffer",
			    first_diff((const unsigned char *)dem_det_b,
				       (const unsigned char *)dem_det_a,
				       DEM_BUF * 2), -1, id);

		/*
		 * THE MARK CARRIES THE FLAG WORDS AS WELL AS THE OUTPUT.  On a
		 * single block `sre.adapt`, `fse.pll_on`, `fse.lms_on` and
		 * `fse.tilt_on` do not reach the samples at all, so four named
		 * wrong readings about them separate nothing unless the mark
		 * folds them in -- which is what `t_v29fax.c` measured.
		 */
		marka = marka * 1000003u + ra;
		for (i = 0; i < (int)ra; i++)
			marka = marka * 31u + dem_out_a[i];
		marka = marka * 131u
			+ (unsigned long)get_i(ma.rxs, V17RXS_SRE_ADAPT);
		marka = marka * 131u
			+ (unsigned long)get_i(ma.rxs, V17RXS_FSE_PLL_ON);
		marka = marka * 131u
			+ (unsigned long)get_i(ma.rxs, V17RXS_FSE_LMS_ON);
		marka = marka * 131u
			+ (unsigned long)get_i(ma.rxs, V17RXS_FSE_TILT_ON);

		if (ra == 0)
			dem_paths[0]++;
		else
			dem_paths[1]++;
		if (u->gate_18 == 0)
			dem_paths[2]++;
		else
			dem_paths[3]++;
		if (get_i(ma.rxs, V17RXS_FSE_LMS_ON) != 0)
			dem_paths[4]++;
		if (get_i(ma.rxs, V17RXS_SRE_ADAPT) != 0)
			dem_paths[5]++;
	}

	dem_free(&ma);
	dem_free(&mb);

	for (d = 1; d < (int)M_MAX; d++) {
		dem_build(&mc, seed, u, dem_mrf_c, dem_sre_c, dem_det_c);
		markc = 0;
		tone_phase = 0.0;
		for (blk = 0; blk < DEM_BLOCKS; blk++) {
			unsigned short rc;

			fill_tone((int)count, dtones[u->tone].hz,
				  dtones[u->tone].amp);
			for (i = 0; i < DEM_BUF; i++) {
				dem_out_b[i] = 0xbeef;
				dem_work[i] = (i < (int)count) ? tone_in[i] : 0;
			}
			rc = drive_demod(&mc, dem_work, dem_out_b, count,
					 (enum dem_defect)d);
			markc = markc * 1000003u + rc;
			if (rc < DEM_BUF)
				for (i = 0; i < (int)rc; i++)
					markc = markc * 31u + dem_out_b[i];
			markc = markc * 131u
				+ (unsigned long)get_i(mc.rxs,
						V17RXS_SRE_ADAPT);
			markc = markc * 131u
				+ (unsigned long)get_i(mc.rxs,
						V17RXS_FSE_PLL_ON);
			markc = markc * 131u
				+ (unsigned long)get_i(mc.rxs,
						V17RXS_FSE_LMS_ON);
			markc = markc * 131u
				+ (unsigned long)get_i(mc.rxs,
						V17RXS_FSE_TILT_ON);
		}
		if (markc != marka)
			dem_sep[d]++;
		dem_free(&mc);
	}
}

/*
 * THE AGC'S `%eax` IS `agc->signal`, MEASURED AND NOT BELIEVED.
 *
 * `src/fax/v17.c` reads the field where the object uses the register
 * `FPM_AGC_agc` happens to leave that store in.  That is deviation D1091, and
 * it is only correct while the identity holds -- so it is asserted here, over
 * a live AGC driven with the same stimuli the demodulator sees, by calling the
 * blob's `void` function through a pointer that returns `int`.
 *
 * This is NOT `t_v29fax.c`'s withdrawn `run_agc_identity` (F9001): that one
 * built its own AGC in a local and segfaulted under the period compiler for
 * reasons nobody established.  This runs inside the demodulator fixture, on
 * the AGC the demodulator itself uses, and takes the reading the demodulator
 * takes.
 */
typedef int (*agc_int_fn)(struct fpm_agc *agc, short *samples,
			  unsigned short count);

static void
dem_agc_identity(unsigned seed, const struct dem_setup *u, unsigned short count)
{
	agc_int_fn f = (agc_int_fn)ref_FPM_AGC_agc;
	struct fpm_agc *agc;
	int blk, i, r;

	dem_build(&mc, seed, u, dem_mrf_c, dem_sre_c, dem_det_c);
	agc = (struct fpm_agc *)(void *)(mc.rxs + V17RXS_AGC);

	tone_phase = 0.0;
	for (blk = 0; blk < DEM_BLOCKS; blk++) {
		fill_tone((int)count, dtones[u->tone].hz, dtones[u->tone].amp);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = (i < (int)count) ? tone_in[i] : 0;
		r = f(agc, dem_work, count);
		diff_eq_int("block %ld: FPM_AGC_agc's %%eax is agc->signal",
			    (long)r, (long)agc->signal, blk);
		dem_agc_checked++;
	}
	dem_free(&mc);
}

static int
run_demod(void)
{
	static const unsigned short counts[] = { 32, 160, 512 };
	int gate, tone, enables, c;
	long where = 0;

	dem_tables();
	diff_begin("DemodDataV17");

	for (gate = 0; gate < 2; gate++)
	for (tone = 0; tone < NDTONE; tone++)
	for (enables = 0; enables < 2; enables++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		struct dem_setup u;

		u.gate_18 = (short)gate;
		u.tone = tone;
		u.enables = enables;
		run_demod_one(spread(0x0de70000u, where), &u, counts[c],
			      where);
		if (where < 4)
			dem_agc_identity(spread(0x0a9c0000u, where), &u,
					 counts[c]);
		where++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V17TX_delete                                                          */

/*
 * The transmit side's counterpart, and the same argument as `V17RX_delete`'s:
 * what is checkable is which pointers were released.  Three of the seven
 * releases are the fixture's own static blocks and land in `bad_free`; the
 * other four allocations, plus the two the `SGD` and the `fax_fifo` own
 * internally, are probed with `harness_alloc_ordinal`.
 *
 * The shaper at `V17FP_PPS` is zeroed so `FPM_PPS_free` releases two null
 * pointers and lands in `free_null` rather than chasing the block's
 * pseudorandom filler.  That the object calls it AT ALL is what `free_null`
 * counts.
 */
#define N_TXPROBE	6
#define TXFIFO_ELEMS	32

static void
txdelete_build(struct fix *f, unsigned seed, void *probe[N_TXPROBE])
{
	struct sgd *s;
	struct fax_fifo *ff;

	fixture(f, seed);

	memset(f->fp + V17FP_PPS, 0, sizeof(struct fpm_pps));
	put_ptr(f->fp, V17FP_PTR_0010, sysdep_malloc(DEL_BUF));

	s = ref_SGD_create(0, 0);
	put_ptr(f->prm, V17TXP_SGD, s);

	ff = (struct fax_fifo *)sysdep_malloc(sizeof(struct fax_fifo));
	memset(ff, 0, sizeof(*ff));
	ff->size = TXFIFO_ELEMS;
	ff->buf = (unsigned short *)sysdep_malloc(TXFIFO_ELEMS
						  * sizeof(unsigned short));
	put_ptr(f->prm, V17TXP_FIFO, ff);

	probe[0] = get_ptr(f->fp, V17FP_PTR_0010);
	probe[1] = s;
	probe[2] = s->hist;
	probe[3] = ff;
	probe[4] = ff->buf;
	/*
	 * The sixth probe is the ONE THING THIS FUNCTION MUST NOT FREE that is
	 * also allocated.  `V17TXP_INT_0008 + 4` is in the same parameter block
	 * as the queue and the sequence detector and the delete path does not
	 * touch it; without it, "everything the delete owns was released" would
	 * pass equally well for a function that walked the block freeing every
	 * word in it.
	 */
	probe[5] = sysdep_malloc(DEL_BUF);
	put_ptr(f->prm, V17TXP_INT_0008 + 4, probe[5]);
}

static long txdel_released, txdel_probes, txdel_kept;

static int
run_txdelete(void)
{
	void *pa[N_TXPROBE], *pb[N_TXPROBE];
	int liva[N_TXPROBE], livb[N_TXPROBE];
	struct alloc_log before, mid, after;
	int i, s, sum;
	static const unsigned seeds[] = { 0x0de1e7b0u, 0x0de1e7b1u };

	diff_begin("V17TX_delete");

	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++) {
		unsigned seed = spread(seeds[s], s);

		txdelete_build(&ma, seed, pa);
		before = harness_alloc;
		ref_V17TX_delete(ma.tobj);
		mid = harness_alloc;
		for (i = 0; i < N_TXPROBE; i++)
			liva[i] = harness_alloc_ordinal(pa[i]) != 0;

		txdelete_build(&mb, seed, pb);
		V17TX_delete(mb.tobj);
		after = harness_alloc;
		for (i = 0; i < N_TXPROBE; i++)
			livb[i] = harness_alloc_ordinal(pb[i]) != 0;

		sum = 0;
		for (i = 0; i < N_TXPROBE; i++) {
			diff_eq_int("probe %ld still live", livb[i], liva[i],
				    i);
			txdel_probes++;
			if (i < N_TXPROBE - 1) {
				sum += liva[i];
				if (liva[i] == 0)
					txdel_released++;
			} else if (liva[i] != 0) {
				txdel_kept++;
			}
		}
		diff_eq_int("seed %ld: everything the delete owns was released",
			    sum, 0, s);
		diff_eq_int("seed %ld: the scrambler was NOT released",
			    liva[N_TXPROBE - 1], 1, s);

		diff_eq_int("seed %ld: frees", after.frees - mid.frees,
			    mid.frees - before.frees, s);
		diff_eq_int("seed %ld: null frees",
			    after.free_null - mid.free_null,
			    mid.free_null - before.free_null, s);
		diff_eq_int("seed %ld: unknown frees",
			    after.bad_free - mid.bad_free,
			    mid.bad_free - before.bad_free, s);
		diff_eq_int("seed %ld: the shaper was freed",
			    (mid.free_null - before.free_null) > 0, 1, s);
		diff_eq_int("seed %ld: the three static blocks were freed",
			    (mid.bad_free - before.bad_free) > 0, 1, s);
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V17TX_modem                                                           */

/*
 * The dispatch slot is a stub on BOTH sides -- it has to be, since nothing
 * establishes which function the object plants there -- so what this measures
 * is the loop around it: how many times it fires, with which pointers, what
 * budget it is handed, and what the instance's result word ends up as.
 *
 * THE SLOT'S LOG IS WHAT MAKES THE POINTER ARITHMETIC OBSERVABLE AT ALL.  `in`
 * does not advance and `out` does, and neither fact reaches the return value
 * or the samples unless the calls are recorded.
 *
 * TWO STEP MODES, AND THE SECOND ONE EXISTS FOR TWO NAMED READINGS.  Mode 0
 * takes 9 off the budget and writes a handful of samples, which is the
 * ordinary shape.  Mode 1 takes 24, so the budget lands EXACTLY on zero -- the
 * only way "the loop runs while the budget is >= 0" is distinguishable from
 * "> 0" -- and writes 20,000 samples a call, so the running total passes
 * 32767.
 *
 * TWO READINGS ARE NOT CLAIMED, AND WHY IS PART OF THE RECORD.
 *
 *   - "the budget reloaded every iteration rather than once before the loop".
 *     The object sets it once (`movw $0x30,0x1a(%esp)` sits at the join of
 *     both arms, above the loop), and a per-iteration reading DOES NOT
 *     TERMINATE for any slot that decrements by less than the reload -- which
 *     is a fact about the reading, not something a differential run can be
 *     asked to measure.  Settled from the disassembly.
 *   - "the running total not narrowed to a short".  The object narrows with
 *     `cwtl` on every iteration, and the narrowing is UNOBSERVABLE: the total
 *     leaves through `*count`, which is 16 bits wide, and a 32-bit sum and its
 *     `short` truncation have the same low sixteen.  Followed because the
 *     object encodes it, not because anything here can see it.  Finding F9107.
 */
#define TXM_IN		256
#define TXM_OUT		65536
#define TXM_BIG		20000

struct txm_log {
	int	calls;
	long	in_off;			/* of the LAST call            */
	long	out_off;
	long	first_out_off;
	long	budget_first;		/* what the slot was handed    */
	long	raw_total;
};

static struct txm_log tlog_a, tlog_b;
static unsigned short txm_in_a[TXM_IN], txm_in_b[TXM_IN];
static short txm_out_a[TXM_OUT], txm_out_b[TXM_OUT];

/* One FIFO per fixture, so the queue's own state is compared as well. */
static struct fax_fifo txm_fifo[3];
static unsigned short txm_fbuf[3][TXFIFO_ELEMS];

static int txm_mode;

static short
txm_step(void *modem, unsigned short *in, short *out, short *budget)
{
	struct txm_log *lg;
	unsigned short *ibase;
	short *obase;
	short give;
	int k;

	if (modem == (void *)ma.tobj) {
		lg = &tlog_a;
		ibase = txm_in_a;
		obase = txm_out_a;
	} else {
		lg = &tlog_b;
		ibase = txm_in_b;
		obase = txm_out_b;
	}

	if (lg->calls == 0) {
		lg->first_out_off = out - obase;
		lg->budget_first = *budget;
	}
	lg->calls++;
	lg->in_off = in - ibase;
	lg->out_off = out - obase;

	if (txm_mode == 0) {
		give = (short)(11 + (*budget & 7));
		*budget = (short)(*budget - 9);
	} else {
		give = (short)(TXM_BIG + (*budget & 7));
		*budget = (short)(*budget - 24);
	}

	/*
	 * The source words come from the BASE, never from `in`.  One named
	 * wrong reading advances `in` by what the slot returned, which under
	 * mode 1 is 20,000 -- reading through it would be out of bounds and
	 * would measure the operating system rather than the code.
	 */
	for (k = 0; k < give; k++)
		out[k] = (short)(0x1000 + ((ibase[k % TXM_IN] + k) & 0x7ff));

	lg->raw_total += give;
	return give;
}

/* The named wrong readings, one changed thing each. */
enum txm_defect {
	T_NONE = 0,
	T_NO_CLEAR,		/* the entry bit is not cleared             */
	T_WRONG_BIT,		/* bit 0 cleared instead of bit 1           */
	T_BUDGET_31,		/* the budget starts at 0x31               */
	T_GE_ZERO,		/* the loop runs while the budget is >= 0   */
	T_IN_ADVANCES,		/* `in` advanced by what the slot returned  */
	T_OUT_STILL,		/* `out` not advanced                       */
	T_FIFO_ALWAYS,		/* the queue used whatever +0x08 says       */
	T_FIFO_NEVER,		/* the queue never used                     */
	T_SAVED_IS_COUNT,	/* the saved value always `*count`          */
	T_NO_BUSY_BIT,		/* the flag bit not set on a short write    */
	T_NO_BUSY_BYTE,		/* the literal 9 not written                */
	T_RESULT_OFF,		/* the result word read from +0x1c          */
	T_MAX
};

static long txm_sep[T_MAX];
static long txm_paths[6];

static int
drive_txm(struct fix *f, unsigned short *in, short *out, unsigned short *count,
	  enum txm_defect d)
{
	unsigned char *prm;
	unsigned short taken;
	short budget;
	short total;

	prm = (unsigned char *)get_ptr(f->tobj, V17TX_OBJ_PARAMS);

	if (d != T_NO_CLEAR)
		f->tobj[V17TX_OBJ_RESULT_B1] &= (unsigned char)
			~((d == T_WRONG_BIT) ? 0x01 : V17TX_RESULT_B1_BIT1);

	if (d == T_FIFO_NEVER
	    || (d != T_FIFO_ALWAYS && get_i(prm, V17TXP_INT_0008) != 0))
		taken = *count;
	else
		taken = (unsigned short)ref_FIFO_write(
				(struct fax_fifo *)get_ptr(prm, V17TXP_FIFO),
				in, *count);
	if (d == T_SAVED_IS_COUNT)
		taken = *count;

	budget = (short)((d == T_BUDGET_31) ? 0x31 : V17TX_MODEM_BUDGET);
	total = 0;
	do {
		short got;

		prm = (unsigned char *)get_ptr(f->tobj, V17TX_OBJ_PARAMS);
		got = (*(v17tx_process_fn *)(void *)(prm + V17TXP_PROCESS))
				(f->tobj, in, out, &budget);

		if (d == T_IN_ADVANCES)
			in += got;
		if (d != T_OUT_STILL)
			out += got;
		total = (short)(total + got);
	} while ((d == T_GE_ZERO) ? (budget >= 0) : (budget > 0));

	if (*count != taken) {
		if (d != T_NO_BUSY_BIT)
			f->tobj[V17TX_OBJ_RESULT_B1] |= V17TX_RESULT_B1_BIT1;
		if (d != T_NO_BUSY_BYTE)
			f->tobj[V17TX_OBJ_RESULT] = V17TX_RESULT_BYTE_09;
	}

	*count = (unsigned short)total;

	return get_i(f->tobj, (d == T_RESULT_OFF) ? 0x1c : V17TX_OBJ_RESULT);
}

static unsigned long
txm_mark(const struct fix *f, int ret, unsigned short cnt,
	 const struct txm_log *lg, const short *out, int slot)
{
	unsigned long m = (unsigned long)ret;
	int i;

	m = m * 131u + cnt;
	m = m * 131u + (unsigned long)lg->calls;
	m = m * 131u + (unsigned long)lg->in_off;
	m = m * 131u + (unsigned long)lg->out_off;
	m = m * 131u + (unsigned long)lg->first_out_off;
	m = m * 131u + (unsigned long)lg->budget_first;
	m = m * 131u + (unsigned long)lg->raw_total;
	m = m * 131u + f->tobj[V17TX_OBJ_RESULT];
	m = m * 131u + f->tobj[V17TX_OBJ_RESULT_B1];
	/* The queue's own state, so a wrong arm is caught even when the
	 * result word happens to agree. */
	m = m * 131u + (unsigned long)txm_fifo[slot].count;
	m = m * 131u + (unsigned long)txm_fifo[slot].wr;
	m = m * 131u + (unsigned long)txm_fifo[slot].rd;
	for (i = 0; i < TXFIFO_ELEMS; i++)
		m = m * 31u + txm_fbuf[slot][i];
	for (i = 0; i < TXM_OUT; i++)
		m = m * 31u + (unsigned short)out[i];
	return m;
}

/*
 * `gate` chooses the arm and `room` chooses whether the queue can take the
 * whole block.  Both matter: with the FIFO always able to take everything, the
 * mismatch that sets the result byte never fires and four named readings
 * separate nothing.
 */
static void
txm_build(struct fix *f, unsigned seed, int gate, int room, int slot)
{
	fixture(f, seed);
	put_ptr(f->prm, V17TXP_PROCESS, (void *)txm_step);
	put_i(f->prm, V17TXP_INT_0008, gate);

	memset(&txm_fifo[slot], 0, sizeof(txm_fifo[slot]));
	memset(txm_fbuf[slot], 0, sizeof(txm_fbuf[slot]));
	txm_fifo[slot].size = (short)(room ? TXFIFO_ELEMS : 2);
	txm_fifo[slot].buf = txm_fbuf[slot];
	put_ptr(f->prm, V17TXP_FIFO, &txm_fifo[slot]);
}

/*
 * `compare_all` minus the parameter block, which cannot come out equal here:
 * `V17TXP_FIFO` holds each fixture's own queue and the two addresses differ for
 * ever.  Every other byte of the block IS compared, including the mode int and
 * the dispatch slot.
 */
static void
txm_compare_all(struct fix *a, struct fix *b, long where)
{
	int i;

	for (i = 0; i < PRM_SIZE; i++) {
		if (i >= V17TXP_FIFO && i < V17TXP_FIFO + (int)sizeof(void *))
			continue;
		if (i >= V17TXP_PROCESS
		    && i < V17TXP_PROCESS + (int)sizeof(void *))
			continue;
		if (a->prm[i] != b->prm[i]) {
			diff_eq_int("at %ld: first differing parameter byte",
				    (long)i, -1, where);
			break;
		}
	}
	if (i == PRM_SIZE)
		diff_eq_int("at %ld: first differing parameter byte", -1L, -1,
			    where);

	diff_eq_int("at %ld: first differing receive-instance byte",
		    robj_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing transmit-instance byte",
		    tobj_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing control-block byte",
		    ctl_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing receiver-state byte",
		    rxs_diff(b, a), -1, where);
	diff_eq_int("at %ld: first differing private-block byte",
		    first_diff(b->fp, a->fp, FP_SIZE), -1, where);
	diff_eq_int("at %ld: first differing status byte",
		    first_diff(b->sta, a->sta, STA_SIZE), -1, where);
}

static void
run_txm_one(unsigned seed, int gate, int room, int mode, unsigned short n,
	    long where)
{
	unsigned long marka, markc;
	int ra, rb, rc, d, i;
	unsigned short ca, cb, cc;

	txm_mode = mode;
	txm_build(&ma, seed, gate, room, 0);
	txm_build(&mb, seed, gate, room, 1);

	rng_seed(seed ^ 0x7c1a5500u);
	for (i = 0; i < TXM_IN; i++)
		txm_in_a[i] = txm_in_b[i] = (unsigned short)rng_next();
	for (i = 0; i < TXM_OUT; i++)
		txm_out_a[i] = txm_out_b[i] = (short)OMARK;

	memset(&tlog_a, 0, sizeof(tlog_a));
	memset(&tlog_b, 0, sizeof(tlog_b));
	ca = n;
	cb = n;

	ra = ref_V17TX_modem(ma.tobj, txm_in_a, txm_out_a, &ca);
	rb = V17TX_modem(mb.tobj, txm_in_b, txm_out_b, &cb);

	diff_eq_int("at %ld: V17TX_modem returned", (long)rb, (long)ra, where);
	diff_eq_int("at %ld: the total written back", (long)cb, (long)ca,
		    where);
	diff_eq_int("at %ld: the same number of inner calls", tlog_b.calls,
		    tlog_a.calls, where);
	diff_eq_int("at %ld: the inner call fired", tlog_a.calls > 0, 1, where);
	diff_eq_int("at %ld: the budget the slot was handed",
		    tlog_b.budget_first, tlog_a.budget_first, where);
	diff_eq_int("at %ld: the budget started at 0x30", tlog_a.budget_first,
		    V17TX_MODEM_BUDGET, where);
	diff_eq_int("at %ld: the last input pointer", tlog_b.in_off,
		    tlog_a.in_off, where);
	diff_eq_int("at %ld: the input pointer did not move", tlog_a.in_off, 0,
		    where);
	diff_eq_int("at %ld: the last output pointer", tlog_b.out_off,
		    tlog_a.out_off, where);
	diff_eq_int("at %ld: the first output pointer", tlog_b.first_out_off,
		    tlog_a.first_out_off, where);
	diff_eq_int("at %ld: first differing output sample",
		    first_diff((const unsigned char *)txm_out_b,
			       (const unsigned char *)txm_out_a,
			       (int)sizeof(txm_out_a)), -1, where);
	diff_eq_int("at %ld: first differing input word",
		    first_diff((const unsigned char *)txm_in_b,
			       (const unsigned char *)txm_in_a,
			       (int)sizeof(txm_in_a)), -1, where);
	diff_eq_int("at %ld: first differing queue byte",
		    first_diff((const unsigned char *)&txm_fifo[1],
			       (const unsigned char *)&txm_fifo[0],
			       (int)((const char *)&txm_fifo[0].buf
				     - (const char *)&txm_fifo[0])), -1, where);
	diff_eq_int("at %ld: first differing queued word",
		    first_diff((const unsigned char *)txm_fbuf[1],
			       (const unsigned char *)txm_fbuf[0],
			       (int)sizeof(txm_fbuf[0])), -1, where);
	diff_eq_int("at %ld: the queue's occupancy",
		    (long)txm_fifo[1].count, (long)txm_fifo[0].count, where);
	txm_compare_all(&ma, &mb, where);

	if (gate == 0)
		txm_paths[0]++;
	else
		txm_paths[1]++;
	if (ma.tobj[V17TX_OBJ_RESULT_B1] & V17TX_RESULT_B1_BIT1)
		txm_paths[2]++;
	else
		txm_paths[3]++;
	if (tlog_a.calls > 1)
		txm_paths[4]++;
	if (tlog_a.raw_total > 32767)
		txm_paths[5]++;		/* the running total wrapped */

	marka = txm_mark(&ma, ra, ca, &tlog_a, txm_out_a, 0);

	for (d = 1; d < (int)T_MAX; d++) {
		txm_build(&mc, seed, gate, room, 2);
		for (i = 0; i < TXM_IN; i++)
			txm_in_b[i] = txm_in_a[i];
		for (i = 0; i < TXM_OUT; i++)
			txm_out_b[i] = (short)OMARK;
		memset(&tlog_b, 0, sizeof(tlog_b));
		cc = n;
		rc = drive_txm(&mc, txm_in_b, txm_out_b, &cc,
			       (enum txm_defect)d);
		markc = txm_mark(&mc, rc, cc, &tlog_b, txm_out_b, 2);
		if (markc != marka)
			txm_sep[d]++;
	}
}

static int
run_txm(void)
{
	static const unsigned short counts[] = { 1, 4, 20, 64 };
	int gate, room, mode, c;
	long where = 0;

	diff_begin("V17TX_modem");

	for (gate = 0; gate < 2; gate++)
	for (room = 0; room < 2; room++)
	for (mode = 0; mode < 2; mode++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		run_txm_one(spread(0x07c17a00u, where), gate, room, mode,
			    counts[c], where);
		where++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* RxHdxDataV17 and RxHdxErrorV17                                        */

/*
 * The two receive-machine states this batch writes, driven THROUGH THEIR REAL
 * CALLEES -- a live demodulator chain, a live V.21 detector, a live
 * descrambler and the two graders -- because neither function does anything
 * except order five calls and eight bit operations around them.  A fixture
 * that stubbed the callees would measure the stubs.
 *
 * F8790 APPLIES TWICE OVER.  Both functions are stateful in six different
 * places at once (the resampler, the recoverer, the equaliser, both AGCs, the
 * descrambler's shift register and `QualityDetectV17`'s block counter), and
 * the SNR arm depends on the equaliser's smoothed error, which needs blocks to
 * move.  So a trial is HDX_BLOCKS consecutive blocks and every named wrong
 * reading replays the whole sequence from a fresh fixture.
 *
 * THE WRONG READINGS
 *
 *   the second gate at ctl + 0x0c rather than ctl + 0x08, and read 16-bit
 *     rather than 32.  The 16-bit reading is separated ON PURPOSE by setups
 *     that plant 0x00010000 there: the int is non-zero and the short is zero,
 *     so the two readings take different arms.  Without that plant the
 *     variant would report zero and the check would be decoration.
 *   the second gate not tested at all, and tested the other way round.
 *   the carrier bit 0x01 rather than 0x20; not raised on entry; not lowered
 *     on the deny arm.
 *   the status byte written at 0x29 (the flags) rather than 0x28, and written
 *     32 bits wide rather than as a byte -- the second is what a reading that
 *     took `V17RX_OBJ_RESULT` for the whole word would do, and it destroys
 *     the flags byte that lives inside it.
 *   `*count` left alone, on each arm separately.
 *   the quality code compared against 1 rather than 2, and the two arms of
 *     that conditional transposed.
 *   the SNR test `<` rather than `<=`, and the flag not cleared before it.
 *   the descrambler given `*count` rather than the demodulator's return, and
 *     not called at all.
 *
 * THE DIAGNOSTIC TRANSCRIPTS ARE NOT COMPARED HERE, AND THAT IS A PROPERTY OF
 * THE HARNESS RATHER THAN A GAP IN THE CHECK.  Neither function prints; every
 * line either could produce comes from a callee whose own section already
 * compares it at level 2.  What made a transcript comparison here FAIL is
 * `FPM_FSE_receive`'s "Decoder Error" line, which is gated on
 * `avg_err_show.0` -- a FUNCTION-SCOPE STATIC in `.bss`, one per side, shared
 * by every equaliser instance and never reset.  `run_demod`'s variant loop
 * drives the BLOB's `ref_FPM_FSE_receive` seventeen times for every one call
 * of ours, so by the time this section runs the two counters are thousands of
 * samples apart and one side prints where the other does not.  The static is
 * the object's and is correctly reproduced (`src/dsp/fpm_fse.c`); what is not
 * composable is "compare the transcripts" across sections that drive the two
 * sides unequally.  That is finding F9120, found from the V.27ter side and
 * answered there by narrowing the comparison to one function's own line;
 * finding F9238 records that it binds in `t_v17fax` too and that here there is
 * no line to narrow to, because neither handler prints.
 *
 * WHAT IS NOT CLAIMED: the object's `movswl %si` on the way out.  The result
 * is `n & -(q != 2)` and `n` is the equaliser's symbol count, which
 * `V17RXS_SRE_MAX` bounds at 0xa4 -- so the value never reaches 0x8000 and no
 * input separates a `short` return from an `unsigned short` one.  The
 * narrowing is in `src/fax/v17.c` because the object encodes it, and it is
 * counted here as REACHED-BUT-NOT-SEPARATING rather than asserted.
 */
extern short ref_RxHdxDataV17(void *modem, short *in, short *out,
			      unsigned short *count);
extern short ref_RxHdxErrorV17(void *modem, short *in, short *out,
			       unsigned short *count);

#define HDX_BLOCKS	10
#define HDX_GATE_HIGH	0x00010000	/* non-zero as an int, zero as a short */

struct hdx_setup {
	short	gate_18;	/* ctl V17RXC_SHORT_0018, the tone pre-pass  */
	int	tone;
	int	enables;
	int	gate_08;	/* ctl V17RXC_INT_0008, the second gate      */
	short	mode_20;	/* ctl V17RXC_SHORT_0020, DCD's arm          */
	short	rms_on;		/* rxs V17RXS_SHORT_4FB4                     */
	int	carrier;	/* rxs V17RXS_INT_0120                       */
	short	dec_error;	/* rxs V17RXS_DEC_ERROR, so GetSNRV17 moves  */
};

enum hdx_defect {
	H_NONE = 0,
	H_GATE_AT_0C,
	H_GATE_16BIT,
	H_GATE_OFF,
	H_GATE_INVERTED,
	H_CARRIER_BIT_01,
	H_CARRIER_NOT_SET,
	H_CARRIER_NOT_CLEARED,
	H_STATUS_AT_29,
	H_STATUS_WIDE,
	H_COUNT_KEPT,
	H_COUNT_KEPT_DENY,
	H_QUALITY_EQ_1,
	H_QUALITY_INVERTED,
	H_SNR_STRICT,
	H_SNR_NOT_CLEARED,
	H_DESCR_COUNT,
	H_NO_DESCRAMBLE,
	H_MAX
};

enum herr_defect {
	E_NONE = 0,
	E_BIT_01,
	E_AT_28,
	E_NO_DEMOD,
	E_COUNT_KEPT,
	E_DEMOD_ZERO,
	E_RET_N,
	E_MAX
};

static long hdx_sep[H_MAX], herr_sep[E_MAX];
static long hdx_accepted, hdx_denied;
static long hdx_snr_low, hdx_snr_high;
static long hdx_unreliable, hdx_graded;
static long hdx_wide_return;	/* a return that would separate the narrowing */
static long herr_blocks;

static short hdx_out_a[DEM_BUF], hdx_out_b[DEM_BUF];
static unsigned short hdx_scr_a[DEM_BUF];

static void
hdx_build(struct fix *f, unsigned seed, const struct hdx_setup *u,
	  short *mrfbuf, short *srebuf, short *detbuf)
{
	struct dem_setup d;
	struct fpm_sdm_cfg cfg;
	struct fpm_agc *agc;

	d.gate_18 = u->gate_18;
	d.tone = u->tone;
	d.enables = u->enables;
	dem_build(f, seed, &d, mrfbuf, srebuf, detbuf);

	/* `DataCarrierDetectV17`'s own chain, exactly as `dcd_setup` lays it. */
	memset(&f->mtd, 0, sizeof(f->mtd));
	memset(f->mtd_acc, 0, sizeof(f->mtd_acc));
	memset(f->buf2, 0, sizeof(f->buf2));
	f->mtd.acc = f->mtd_acc;
	ref_FPM_MTD_create(&f->mtd, &ref_MTDv22_CFG);
	put_ptr(f->ctl, V17RXC_MTD2, &f->mtd);
	put_ptr(f->ctl, V17RXC_BUF2, f->buf2);

	agc = (struct fpm_agc *)(void *)(f->ctl + V17RXC_AGC);
	memset(agc, 0, sizeof(*agc));
	ref_FPM_AGC_init(agc, &ref_AGCv17_CFG, 1);

	/* V.17's own polynomial; see `scr_build`. */
	cfg.nbits = 8;
	cfg.tap1 = 0x12;
	cfg.tap2 = 0x17;
	ref_SDM_init((struct fpm_sdm *)(void *)(f->rxs + V17RXS_SDM), &cfg);

	put_i(f->ctl, V17RXC_INT_0008, u->gate_08);
	put_i(f->ctl, V17RXC_INT_0010, 1);
	put_s(f->ctl, V17RXC_SHORT_0020, u->mode_20);
	put_s(f->ctl, V17RXC_SHORT_002E, 0);
	put_s(f->ctl, V17RXC_OFFBAND, 0);

	put_i(f->rxs, V17RXS_EPOCH, 1);
	put_s(f->rxs, V17RXS_SHORT_0094, 1000);
	put_i(f->rxs, V17RXS_INT_0120, u->carrier);
	put_s(f->rxs, V17RXS_AGC_SIGNAL, 1);
	put_s(f->rxs, V17RXS_AGC_SIGNAL + 2, 0x1234);
	put_s(f->rxs, V17RXS_SHORT_4FB4, u->rms_on);
	put_s(f->rxs, V17RXS_RMS_REF, 0x2000);
	put_s(f->rxs, V17RXS_RMS_PHASE, 0);
	put_s(f->rxs, V17RXS_DEC_ERROR, u->dec_error);
	put_s(f->rxs, V17RXS_QAVG, 0);
	put_s(f->rxs, V17RXS_QCOUNT, 0);
	put_s(f->rxs, V17RXS_SHORT_4FB0, 0x0100);
	put_s(f->rxs, V17RXS_SHORT_4FB2, 0);
}

/* The object's own sequence, with one reading changed. */
static short
drive_hdx(struct fix *f, short *in, short *out, unsigned short *count, int v)
{
	unsigned char *ro = f->robj;
	unsigned char carrier = (v == H_CARRIER_BIT_01)
				? (unsigned char)0x01
				: (unsigned char)V17RX_FLAG_CARRIER;
	unsigned short n;
	short q, s, r;
	int gate;

	if (v != H_CARRIER_NOT_SET)
		ro[V17RX_OBJ_RESULT_B1] |= carrier;
	if (v == H_STATUS_AT_29)
		ro[V17RX_OBJ_RESULT_B1] = V17RX_STATUS_DATA;
	else if (v == H_STATUS_WIDE)
		put_i(ro, V17RX_OBJ_RESULT, V17RX_STATUS_DATA);
	else
		ro[V17RX_OBJ_RESULT] = V17RX_STATUS_DATA;

	if (v == H_GATE_AT_0C)
		gate = get_i(f->ctl, 0x0c);
	else if (v == H_GATE_16BIT)
		gate = get_s(f->ctl, V17RXC_INT_0008);
	else
		gate = get_i(f->ctl, V17RXC_INT_0008);
	if (v == H_GATE_OFF)
		gate = 0;
	else if (v == H_GATE_INVERTED)
		gate = !gate;

	if (ref_DataCarrierDetectV17(ro, in, *count) == 0 || gate != 0) {
		if (v != H_CARRIER_NOT_CLEARED)
			ro[V17RX_OBJ_RESULT_B1] &= (unsigned char)~carrier;
		if (v != H_COUNT_KEPT_DENY)
			*count = 0;
		return 0;
	}

	n = ref_DemodDataV17(ro, in, (unsigned short *)(void *)out, *count);
	if (v != H_NO_DESCRAMBLE)
		ref_DescrambleDataV17(ro, (unsigned short *)(void *)out,
				      (v == H_DESCR_COUNT)
					? (unsigned short)*count : n);
	if (v != H_COUNT_KEPT)
		*count = 0;

	q = ref_QualityDetectV17(ro);
	if (v == H_QUALITY_EQ_1)
		r = (short)(q != 1 ? n : 0);
	else if (v == H_QUALITY_INVERTED)
		r = (short)(q == V17_QUALITY_UNRELIABLE ? n : 0);
	else
		r = (short)(q != V17_QUALITY_UNRELIABLE ? n : 0);

	if (v != H_SNR_NOT_CLEARED)
		ro[V17RX_OBJ_RESULT_B1] &=
			(unsigned char)~(unsigned char)V17RX_FLAG_LOW_SNR;
	s = ref_GetSNRV17(ro);
	if (v == H_SNR_STRICT ? s < V17RX_SNR_THRESHOLD
			      : s <= V17RX_SNR_THRESHOLD)
		ro[V17RX_OBJ_RESULT_B1] |= (unsigned char)V17RX_FLAG_LOW_SNR;

	return r;
}

/* `RxHdxErrorV17`'s eleven instructions, with one reading changed. */
static short
drive_herr(struct fix *f, short *in, short *out, unsigned short *count, int v)
{
	unsigned char *ro = f->robj;
	unsigned short n;

	ro[(v == E_AT_28) ? V17RX_OBJ_RESULT : V17RX_OBJ_RESULT_B1] |=
		(v == E_BIT_01) ? (unsigned char)0x01
				: (unsigned char)V17RX_FLAG_ERROR;

	n = 0;
	if (v != E_NO_DEMOD)
		n = ref_DemodDataV17(ro, in, (unsigned short *)(void *)out,
				     (v == E_DEMOD_ZERO) ? 0 : *count);
	if (v != E_COUNT_KEPT)
		*count = 0;

	return (v == E_RET_N) ? (short)n : 0;
}

/* Everything one call made observable, folded into a word. */
static unsigned long
hdx_mark(unsigned long m, struct fix *f, short r, unsigned short count,
	 const short *out)
{
	int i;

	m = m * 1000003u + (unsigned short)r;
	m = m * 31u + count;
	m = m * 31u + f->robj[V17RX_OBJ_RESULT];
	m = m * 31u + f->robj[V17RX_OBJ_RESULT_B1];
	/*
	 * A `short`, and reading it as an int would fold in the low half of
	 * `fpm_fse::out_i` -- a per-fixture pointer, which differs for ever
	 * and would make every variant "separate" whatever it did.
	 */
	m = m * 131u + (unsigned short)get_s(f->rxs, V17RXS_DEC_ERROR);
	m = m * 131u + (unsigned long)(unsigned short)
			get_s(f->rxs, V17RXS_QAVG);
	m = m * 131u + get_us(f->rxs, V17RXS_QCOUNT);
	for (i = 0; i < DEM_BUF; i++)
		m = m * 31u + (unsigned short)out[i];
	return m;
}

static void
hdx_compare(struct fix *a, struct fix *b, long id)
{
	diff_eq_int("at %ld: first differing receive-instance byte",
		    robj_diff(b, a), -1, id);
	diff_eq_int("at %ld: first differing control-block byte",
		    ctl_diff_demod(b, a), -1, id);
	diff_eq_int("at %ld: first differing demodulator-state byte",
		    rxs_diff_demod(b, a), -1, id);
	diff_eq_int("at %ld: first differing V.21 detector field",
		    mtd_diff(b, a), -1, id);
	diff_eq_int("at %ld: first differing detector-buffer byte",
		    first_diff((const unsigned char *)b->buf2,
			       (const unsigned char *)a->buf2,
			       (int)sizeof(a->buf2)), -1, id);
}

static void
run_hdx_one(unsigned seed, const struct hdx_setup *u, unsigned short count,
	    long tag)
{
	unsigned long marka = 0, markc;
	int blk, d, i;

	hdx_build(&ma, seed, u, dem_mrf_a, dem_sre_a, dem_det_a);
	hdx_build(&mb, seed, u, dem_mrf_b, dem_sre_b, dem_det_b);

	tone_phase = 0.0;
	for (blk = 0; blk < HDX_BLOCKS; blk++) {
		unsigned short ca = count, cb = count;
		short ra, rb;
		long id = tag * 100 + blk;

		fill_tone((int)count, dtones[u->tone].hz, dtones[u->tone].amp);
		for (i = 0; i < DEM_BUF; i++) {
			dem_in[i] = (i < (int)count) ? tone_in[i] : 0;
			hdx_out_a[i] = hdx_out_b[i] = (short)0xbeef;
		}

		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		ra = ref_RxHdxDataV17(ma.robj, dem_work, hdx_out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		rb = RxHdxDataV17(mb.robj, dem_work, hdx_out_b, &cb);

		diff_eq_int("at %ld: RxHdxDataV17 returned", (long)rb, (long)ra,
			    id);
		diff_eq_int("at %ld: RxHdxDataV17 left *count", (long)cb,
			    (long)ca, id);
		diff_eq_int("at %ld: RxHdxDataV17's output buffer",
			    first_diff((const unsigned char *)hdx_out_b,
				       (const unsigned char *)hdx_out_a,
				       DEM_BUF * 2), -1, id);
		hdx_compare(&ma, &mb, id);

		if ((ma.robj[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_CARRIER) != 0)
			hdx_accepted++;
		else
			hdx_denied++;
		if ((ma.robj[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_LOW_SNR) != 0)
			hdx_snr_low++;
		else
			hdx_snr_high++;
		if (ra == 0)
			hdx_unreliable++;
		else
			hdx_graded++;
		if (ra < 0)
			hdx_wide_return++;

		marka = hdx_mark(marka, &ma, ra, ca, hdx_out_a);
	}

	dem_free(&ma);
	dem_free(&mb);

	for (d = 1; d < (int)H_MAX; d++) {
		hdx_build(&mc, seed, u, dem_mrf_c, dem_sre_c, dem_det_c);
		markc = 0;
		tone_phase = 0.0;
		for (blk = 0; blk < HDX_BLOCKS; blk++) {
			unsigned short cc = count;
			short rc;

			fill_tone((int)count, dtones[u->tone].hz,
				  dtones[u->tone].amp);
			for (i = 0; i < DEM_BUF; i++) {
				dem_work[i] = (i < (int)count) ? tone_in[i] : 0;
				hdx_out_b[i] = (short)0xbeef;
			}
			rc = drive_hdx(&mc, dem_work, hdx_out_b, &cc, d);
			markc = hdx_mark(markc, &mc, rc, cc, hdx_out_b);
		}
		if (markc != marka)
			hdx_sep[d]++;
		dem_free(&mc);
	}

	/* The faithful model, which must agree with the blob exactly. */
	hdx_build(&mc, seed, u, dem_mrf_c, dem_sre_c, dem_det_c);
	markc = 0;
	tone_phase = 0.0;
	for (blk = 0; blk < HDX_BLOCKS; blk++) {
		unsigned short cc = count;
		short rc;

		fill_tone((int)count, dtones[u->tone].hz, dtones[u->tone].amp);
		for (i = 0; i < DEM_BUF; i++) {
			dem_work[i] = (i < (int)count) ? tone_in[i] : 0;
			hdx_out_b[i] = (short)0xbeef;
		}
		rc = drive_hdx(&mc, dem_work, hdx_out_b, &cc, H_NONE);
		markc = hdx_mark(markc, &mc, rc, cc, hdx_out_b);
	}
	diff_eq_int("RxHdxDataV17 model (%ld)", markc == marka, 1, tag);
	dem_free(&mc);
}

static void
run_herr_one(unsigned seed, const struct hdx_setup *u, unsigned short count,
	     long tag)
{
	unsigned long marka = 0, markc;
	int blk, d, i;

	hdx_build(&ma, seed, u, dem_mrf_a, dem_sre_a, dem_det_a);
	hdx_build(&mb, seed, u, dem_mrf_b, dem_sre_b, dem_det_b);

	tone_phase = 0.0;
	for (blk = 0; blk < HDX_BLOCKS; blk++) {
		unsigned short ca = count, cb = count;
		short ra, rb;
		long id = 100000 + tag * 100 + blk;

		fill_tone((int)count, dtones[u->tone].hz, dtones[u->tone].amp);
		for (i = 0; i < DEM_BUF; i++) {
			dem_in[i] = (i < (int)count) ? tone_in[i] : 0;
			hdx_out_a[i] = hdx_out_b[i] = (short)0xbeef;
		}

		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		ra = ref_RxHdxErrorV17(ma.robj, dem_work, hdx_out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		rb = RxHdxErrorV17(mb.robj, dem_work, hdx_out_b, &cb);

		diff_eq_int("at %ld: RxHdxErrorV17 returned", (long)rb,
			    (long)ra, id);
		diff_eq_int("at %ld: RxHdxErrorV17 left *count", (long)cb,
			    (long)ca, id);
		diff_eq_int("at %ld: RxHdxErrorV17's output buffer",
			    first_diff((const unsigned char *)hdx_out_b,
				       (const unsigned char *)hdx_out_a,
				       DEM_BUF * 2), -1, id);
		hdx_compare(&ma, &mb, id);
		herr_blocks++;

		marka = hdx_mark(marka, &ma, ra, ca, hdx_out_a);
	}

	dem_free(&ma);
	dem_free(&mb);

	for (d = 1; d < (int)E_MAX; d++) {
		hdx_build(&mc, seed, u, dem_mrf_c, dem_sre_c, dem_det_c);
		markc = 0;
		tone_phase = 0.0;
		for (blk = 0; blk < HDX_BLOCKS; blk++) {
			unsigned short cc = count;
			short rc;

			fill_tone((int)count, dtones[u->tone].hz,
				  dtones[u->tone].amp);
			for (i = 0; i < DEM_BUF; i++) {
				dem_work[i] = (i < (int)count) ? tone_in[i] : 0;
				hdx_out_b[i] = (short)0xbeef;
			}
			rc = drive_herr(&mc, dem_work, hdx_out_b, &cc, d);
			markc = hdx_mark(markc, &mc, rc, cc, hdx_out_b);
		}
		if (markc != marka)
			herr_sep[d]++;
		dem_free(&mc);
	}
}

/*
 * The descrambler runs over the handler's OWN OUTPUT, which is the one thing
 * `hdx_mark` cannot separate from the demodulator's: a reconstruction that
 * left `DescrambleDataV17` out entirely writes a different buffer, but so does
 * one that demodulated differently.  This runs the reference chain by hand --
 * demodulate, then descramble the copy -- and asserts the handler's buffer is
 * the descrambled one and NOT the raw one, on a block where the two differ.
 */
static long hdx_scr_checked, hdx_scr_moved;

static void
hdx_scramble_check(unsigned seed, const struct hdx_setup *u,
		   unsigned short count, long tag)
{
	unsigned short ca = count;
	unsigned short raw[8];
	int i, differs = 0;
	short n;

	hdx_build(&ma, seed, u, dem_mrf_a, dem_sre_a, dem_det_a);
	hdx_build(&mc, seed, u, dem_mrf_c, dem_sre_c, dem_det_c);

	tone_phase = 0.0;
	fill_tone((int)count, dtones[u->tone].hz, dtones[u->tone].amp);
	for (i = 0; i < DEM_BUF; i++) {
		dem_in[i] = (i < (int)count) ? tone_in[i] : 0;
		hdx_out_a[i] = (short)0xbeef;
		hdx_scr_a[i] = 0xbeef;
	}

	for (i = 0; i < DEM_BUF; i++)
		dem_work[i] = dem_in[i];
	n = ref_RxHdxDataV17(ma.robj, dem_work, hdx_out_a, &ca);

	/* The same block, demodulated and left UNDESCRAMBLED. */
	for (i = 0; i < DEM_BUF; i++)
		dem_work[i] = dem_in[i];
	(void)ref_DemodDataV17(mc.robj, dem_work, hdx_scr_a, count);

	for (i = 0; i < 8; i++)
		raw[i] = hdx_scr_a[i];
	for (i = 0; i < 8; i++)
		if (raw[i] != (unsigned short)hdx_out_a[i])
			differs = 1;
	if (differs)
		hdx_scr_moved++;
	diff_eq_int("at %ld: the handler descrambled its output",
		    (n != 0 && differs) || n == 0, 1, tag);
	hdx_scr_checked++;

	dem_free(&ma);
	dem_free(&mc);
}

static int
run_hdx(void)
{
	static const struct hdx_setup setups[] = {
	    /* g18 tone en gate_08         m20 rms carrier dec_error */
	    {  0,  0, 0, 0,                0,  0,  1,      4 },
	    {  0,  0, 0, 0,                0,  1,  1,      5 },
	    {  1,  1, 1, 0,                0,  0,  1,      6 },
	    {  1,  2, 0, 0,                1,  1,  1,      0 },
	    {  0,  1, 1, 1,                0,  0,  1,      9 },
	    {  0,  2, 0, HDX_GATE_HIGH,    0,  1,  1,     13 },
	    {  1,  0, 1, HDX_GATE_HIGH,    1,  0,  1,     14 },
	    {  0,  0, 0, 0,                0,  0,  0,      4 },
	    {  1,  1, 0, 0,                1,  1,  0,      8 },
	    {  0,  2, 1, 0,                0,  0,  1,      8 }
	};
	static const unsigned short counts[] = { 32, 160 };
	int s, c;
	long tag = 0;

	dem_tables();
	diff_begin("RxHdxDataV17 / RxHdxErrorV17");

	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		run_hdx_one(spread(0x0d47a000u, tag), &setups[s], counts[c],
			    tag);
		run_herr_one(spread(0x0e770000u, tag), &setups[s], counts[c],
			     tag);
		hdx_scramble_check(spread(0x05c70000u, tag), &setups[s],
				   counts[c], tag);
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

	rc |= run_seed();
	rc |= run_setenc();
	rc |= run_settxmode();
	rc |= run_txstatus();
	rc |= run_rxm();
	rc |= run_rxstatus();
	rc |= run_scramble();
	rc |= run_rxdelete();
	rc |= run_txdelete();
	rc |= run_txm();
	rc |= run_demod();
	rc |= run_cd();
	rc |= run_dcd();
	rc |= run_qd();
	rc |= run_hdx();
	rc |= run_accessors();

	/*
	 * The separating counts.  Each is the number of trials on which a
	 * NAMED wrong reading produced a different OBSERVABLE answer -- a
	 * return value, a compared byte of an instance, or a pointer the
	 * dispatch slot was handed -- from the blob's.  A zero here means the
	 * corresponding check above is decoration.
	 */
	diff_begin("v17fax separating trials");

	diff_eq_int("the seed's offset separates (%ld)", seed_off_sep > 0, 1,
		    seed_off_sep);
	diff_eq_int("the seed's width separates (%ld)", seed_width_sep > 0, 1,
		    seed_width_sep);
	diff_eq_int("the seed's base block separates (%ld)", seed_base_sep > 0,
		    1, seed_base_sep);

	diff_eq_int("mode 1 not writing the second value separates (%ld)",
		    enc_mode1_sep > 0, 1, enc_mode1_sep);
	diff_eq_int("a writing default arm separates (%ld)",
		    enc_default_sep > 0, 1, enc_default_sep);
	diff_eq_int("the selector's offset separates (%ld)", enc_sel_sep > 0,
		    1, enc_sel_sep);
	diff_eq_int("SetEncoderV17 wrote on some trials (%ld)", enc_wrote > 0,
		    1, enc_wrote);
	diff_eq_int("SetEncoderV17 declined on some trials (%ld)",
		    enc_declined > 0, 1, enc_declined);

	diff_eq_int("SetTxModeV17's shift-register preservation separates"
		    " (%ld)", stm_reg_sep > 0, 1, stm_reg_sep);
	diff_eq_int("SetTxModeV17's per-mode literal separates (%ld)",
		    stm_case_sep > 0, 1, stm_case_sep);
	diff_eq_int("SetTxModeV17's mode+1 field separates (%ld)",
		    stm_pattern_sep > 0, 1, stm_pattern_sep);
	diff_eq_int("SetTxModeV17's map assignment separates (%ld)",
		    stm_map_sep > 0, 1, stm_map_sep);
	diff_eq_int("SetTxModeV17's no-carrier symbol separates (%ld)",
		    stm_nc_sep > 0, 1, stm_nc_sep);
	diff_eq_int("SetTxModeV17's default arm separates (%ld)",
		    stm_default_sep > 0, 1, stm_default_sep);
	diff_eq_int("SetTxModeV17 ran the valid-mode path (%ld)",
		    stm_valid > 0, 1, stm_valid);
	diff_eq_int("SetTxModeV17 ran the default arm (%ld)",
		    stm_default_hit > 0, 1, stm_default_hit);

	diff_eq_int("the NULL guard separates (%ld)", sta_null_sep > 0, 1,
		    sta_null_sep);
	diff_eq_int("status +0x10's source separates (%ld)", sta_10_sep > 0, 1,
		    sta_10_sep);
	diff_eq_int("the status flag's mask separates (%ld)", sta_mask_sep > 0,
		    1, sta_mask_sep);
	diff_eq_int("writing status +0x0e separates (%ld)", sta_0e_sep > 0, 1,
		    sta_0e_sep);
	diff_eq_int("merging the status flag byte separates (%ld)",
		    sta_assign_sep > 0, 1, sta_assign_sep);
	diff_eq_int("clearing status +0x15 outright separates (%ld)",
		    sta_15_sep > 0, 1, sta_15_sep);

	diff_eq_int("the cleared bit separates (%ld)", rxm_bit_sep > 0, 1,
		    rxm_bit_sep);
	diff_eq_int("the cleared byte separates (%ld)", rxm_byte_sep > 0, 1,
		    rxm_byte_sep);
	diff_eq_int("transposing in and out separates (%ld)", rxm_swap_sep > 0,
		    1, rxm_swap_sep);
	diff_eq_int("the write-back is observable (%ld)", rxm_wb_sep > 0, 1,
		    rxm_wb_sep);
	diff_eq_int("the two dispatch slots are distinguishable (%ld)",
		    rxm_slot_sep > 0, 1, rxm_slot_sep);
	diff_eq_int("the result's offset separates (%ld)", rxm_result_sep > 0,
		    1, rxm_result_sep);
	diff_eq_int("the dispatch slot fired (%ld)", rxm_calls_seen > 0, 1,
		    rxm_calls_seen);
	diff_eq_int("the running total passed 32767 (%ld)", rxm_wrapped > 0, 1,
		    rxm_wrapped);

	diff_eq_int("the width of +0xd0 separates in CarrierDetect (%ld)",
		    cd_width_sep > 0, 1, cd_width_sep);
	diff_eq_int("the 999 gate's strictness separates (%ld)",
		    cd_gate999_sep > 0, 1, cd_gate999_sep);
	diff_eq_int("the 0x3fff gate's strictness separates (%ld)",
		    cd_gate3fff_sep > 0, 1, cd_gate3fff_sep);
	diff_eq_int("swapping the two arms separates (%ld)", cd_arm_sep > 0, 1,
		    cd_arm_sep);
	diff_eq_int("CarrierDetectV17 said yes (%ld)", cd_true > 0, 1, cd_true);
	diff_eq_int("CarrierDetectV17 said no (%ld)", cd_false > 0, 1,
		    cd_false);
	diff_eq_int("CarrierDetectV17 printed (%ld)", cd_printed > 0, 1,
		    cd_printed);

	diff_eq_int("inverting the off-band counter separates (%ld)",
		    dcd_sep[DCD_INVERT] > 0, 1, dcd_sep[DCD_INVERT]);
	diff_eq_int("the off-band threshold's strictness separates (%ld)",
		    dcd_sep[DCD_GE_MAX] > 0, 1, dcd_sep[DCD_GE_MAX]);
	diff_eq_int("the energy test's strictness separates (%ld)",
		    dcd_sep[DCD_LE_DROP] > 0, 1, dcd_sep[DCD_LE_DROP]);
	diff_eq_int("adding a rounding term to the -8 dB scale separates (%ld)",
		    dcd_sep[DCD_ROUND] > 0, 1, dcd_sep[DCD_ROUND]);
	diff_eq_int("a three-block reference period separates (%ld)",
		    dcd_sep[DCD_PERIOD3] > 0, 1, dcd_sep[DCD_PERIOD3]);
	diff_eq_int("the mode selector's offset separates (%ld)",
		    dcd_sep[DCD_MODE22] > 0, 1, dcd_sep[DCD_MODE22]);
	diff_eq_int("DataCarrierDetectV17 said yes (%ld)", dcd_true > 0, 1,
		    dcd_true);
	diff_eq_int("DataCarrierDetectV17 said no (%ld)", dcd_false > 0, 1,
		    dcd_false);
	diff_eq_int("the off-band counter moved (%ld)", dcd_counted > 0, 1,
		    dcd_counted);
	diff_eq_int("the energy reference was refreshed (%ld)",
		    dcd_refreshed > 0, 1, dcd_refreshed);
	diff_eq_int("DataCarrierDetectV17 printed (%ld)", dcd_printed > 0, 1,
		    dcd_printed);
	diff_eq_int("the energy threshold's exact edge was reached (%ld)",
		    dcd_edge > 0, 1, dcd_edge);
	/*
	 * Reported, not asserted: see run_dcd_edge.  It is here so a future
	 * change that quietly stopped SOLVING any of them is visible beside
	 * the count that did.
	 */
	diff_eq_int("edge attempts with no solvable reference (%ld)",
		    dcd_edge_unsolved < dcd_edge, 1, dcd_edge_unsolved);

	diff_eq_int("an unsigned block counter separates (%ld)",
		    qd_signed_sep > 0, 1, qd_signed_sep);
	diff_eq_int("swapping the smoothing weights separates (%ld)",
		    qd_weight_sep > 0, 1, qd_weight_sep);
	diff_eq_int("dropping the rounding term separates (%ld)",
		    qd_round_sep > 0, 1, qd_round_sep);
	diff_eq_int("flipping the judge separates (%ld)", qd_judge_sep > 0, 1,
		    qd_judge_sep);
	diff_eq_int("the average was smoothed (%ld)", qd_smoothed > 0, 1,
		    qd_smoothed);
	diff_eq_int("the judge block was reached (%ld)", qd_judged > 0, 1,
		    qd_judged);
	diff_eq_int("the unreliable verdict was reached (%ld)",
		    qd_unreliable > 0, 1, qd_unreliable);

	diff_eq_int("GetSNRV17's free extension was checked (%ld)",
		    snr_free_checked > 0, 1, snr_free_checked);
	diff_eq_int("StoreCoefV17's count separates (%ld)", coef_n_sep > 0, 1,
		    coef_n_sep);
	diff_eq_int("transposing the coefficient arrays separates (%ld)",
		    coef_swap_sep > 0, 1, coef_swap_sep);
	diff_eq_int("the saved rate's signedness separates (%ld)",
		    restore_sep > 0, 1, restore_sep);

	diff_eq_int("some diagnostics were printed (%ld)", dbg_lines_seen > 0,
		    1, dbg_lines_seen);

	/*
	 * The four new blocks, each reading its own counters.  A named wrong
	 * reading with a zero count is a check that measured nothing, so each
	 * is asserted rather than reported.
	 */
	for (i = 1; i < (int)S_MAX; i++)
		diff_eq_int("V17RX_status wrong reading %ld separates",
			    rxs_sep[i] > 0, 1, i);
	diff_eq_int("V17RX_status's NULL guard was exercised (%ld)",
		    rxs_null_sep > 0, 1, rxs_null_sep);
	for (i = 0; i < 4; i++)
		diff_eq_int("V17RX_status flag path %ld was reached",
			    rxs_paths[i] > 0, 1, i);

	for (i = 1; i < (int)C_MAX; i++)
		diff_eq_int("the scrambler's wrong reading %ld separates",
			    scr_sep[i] > 0, 1, i);
	diff_eq_int("the scramblers moved some words (%ld)",
		    scr_words_moved > 0, 1, scr_words_moved);

	diff_eq_int("V17RX_delete probes taken (%ld)", del_probes > 0, 1,
		    del_probes);
	diff_eq_int("V17RX_delete released every probe (%ld of %ld)",
		    del_released, del_probes, del_probes);

	for (i = 1; i < (int)M_MAX; i++)
		diff_eq_int("DemodDataV17 wrong reading %ld separates",
			    dem_sep[i] > 0, 1, i);
	for (i = 0; i < 6; i++)
		diff_eq_int("DemodDataV17 path %ld was reached",
			    dem_paths[i] > 0, 1, i);
	diff_eq_int("the AGC identity was measured (%ld)",
		    dem_agc_checked > 0, 1, dem_agc_checked);

	diff_eq_int("V17TX_delete probes taken (%ld)", txdel_probes > 0, 1,
		    txdel_probes);
	diff_eq_int("V17TX_delete released every probe (%ld)",
		    txdel_released, (txdel_probes / N_TXPROBE)
				    * (N_TXPROBE - 1), txdel_probes);
	diff_eq_int("V17TX_delete kept the one it does not own (%ld)",
		    txdel_kept, txdel_probes / N_TXPROBE, txdel_kept);

	for (i = 1; i < (int)T_MAX; i++)
		diff_eq_int("V17TX_modem wrong reading %ld separates",
			    txm_sep[i] > 0, 1, i);
	for (i = 0; i < 6; i++)
		diff_eq_int("V17TX_modem path %ld was reached",
			    txm_paths[i] > 0, 1, i);

	for (i = 1; i < (int)H_MAX; i++)
		diff_eq_int("RxHdxDataV17 wrong reading %ld separates",
			    hdx_sep[i] > 0, 1, i);
	for (i = 1; i < (int)E_MAX; i++)
		diff_eq_int("RxHdxErrorV17 wrong reading %ld separates",
			    herr_sep[i] > 0, 1, i);
	diff_eq_int("RxHdxDataV17 took the demodulating arm (%ld)",
		    hdx_accepted > 0, 1, hdx_accepted);
	diff_eq_int("RxHdxDataV17 took the deny arm (%ld)", hdx_denied > 0, 1,
		    hdx_denied);
	diff_eq_int("RxHdxDataV17 raised LOW_SNR (%ld)", hdx_snr_low > 0, 1,
		    hdx_snr_low);
	diff_eq_int("RxHdxDataV17 left LOW_SNR clear (%ld)", hdx_snr_high > 0,
		    1, hdx_snr_high);
	diff_eq_int("RxHdxDataV17 returned a graded count (%ld)",
		    hdx_graded > 0, 1, hdx_graded);
	diff_eq_int("RxHdxDataV17 returned zero (%ld)", hdx_unreliable > 0, 1,
		    hdx_unreliable);
	diff_eq_int("RxHdxErrorV17 drove blocks (%ld)", herr_blocks > 0, 1,
		    herr_blocks);
	diff_eq_int("the descrambler check ran (%ld)", hdx_scr_checked > 0, 1,
		    hdx_scr_checked);
	diff_eq_int("the descrambler moved the handler's output (%ld)",
		    hdx_scr_moved > 0, 1, hdx_scr_moved);
	/*
	 * NOT an assertion that it separates.  `n` is bounded by
	 * `V17RXS_SRE_MAX`, so a `short` return and an `unsigned short` one
	 * agree over every reachable value; this reports the count so a future
	 * fixture that DID reach 0x8000 would show up as a non-zero here
	 * rather than silently.
	 */
	diff_eq_int("RxHdxDataV17 returns that would separate the narrowing"
		    " (%ld, expected 0)", hdx_wide_return, 0, hdx_wide_return);

	rc |= diff_end();
	return rc;
}
