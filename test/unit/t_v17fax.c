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

#include <string.h>

#include "harness.h"
#include "dsplib/v17fax.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_sdm.h"

extern int ref_V17RX_modem(void *modem, short *in, short *out,
			   unsigned short *count);
extern void ref_SeedScramblerV17(void *modem, unsigned int seed);
extern void ref_SetEncoderV17(void *modem, short which, short arg);
extern int ref_V17TX_status(void *params, void *status);
extern int ref_CarrierDetectV17(void *modem);
extern short ref_QualityDetectV17(void *modem);
extern int ref_EpochDetectV17(void *modem);
extern short ref_GetSNRV17(void *modem);
extern void ref_StoreCoefV17(void *modem);
extern void ref_Restore_rateV17(void *modem);

extern unsigned int ref_dsplibs_debug_level;

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x80
#define CTL_SIZE	0x40
#define RXS_SIZE	0x5000		/* the object reaches +0x4fb8       */
#define FP_SIZE		0x100
#define PRM_SIZE	0x40
#define STA_SIZE	0x40
#define COEF_SLOTS	64		/* V17_COEF_N is 49; the rest catch
					 * a loop that runs one too far     */

#define NBIG_IN		4096
#define NBIG_OUT	48000
#define OMARK		0x5ead

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
		if (i >= V17RXC_PROCESS
		    && i < V17RXC_PROCESS + (int)sizeof(void *))
			continue;
		if (a->ctl[i] != b->ctl[i])
			return i;
	}
	return -1;
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
static long rxm_bit_sep, rxm_byte_sep, rxm_swap_sep;
static long rxm_wb_sep, rxm_slot_sep, rxm_result_sep;
static long rxm_wrapped, rxm_calls_seen;
static long cd_width_sep, cd_gate999_sep, cd_gate3fff_sep, cd_arm_sep;
static long cd_true, cd_false, cd_printed;
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

		ra = ref_V17TX_status(ma.prm, ma.sta);
		rb = V17TX_status(mb.prm, mb.sta);
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

		/* WRONG READING: status + 0x0e written too. */
		memcpy(&mc, &ma, sizeof(mc));
		rehome(&mc);
		put_s(mc.sta, 0x0e, 0);
		if (memcmp(mc.sta, ma.sta, STA_SIZE) != 0)
			sta_0e_sep++;

		/* The NULL case: it must do nothing and report nothing. */
		fixture(&ma, seed);
		fixture(&mb, seed);
		ra = ref_V17TX_status(ma.prm, (void *)0);
		rb = V17TX_status(mb.prm, (void *)0);
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
		       V17RX_OBJ_RESULT, V17RX_RESULT_B1_BIT1, 0, 1);
	if (rc != ra)
		rxm_byte_sep++;

	/* WRONG READING: `in` and `out` advanced by each other's count. */
	memset(&log_b, 0, sizeof(log_b));
	fixture(&mc, seed);
	put_ptr(mc.ctl, V17RXC_PROCESS, (void *)rxm_step);
	cc = (unsigned short)n;
	(void)drive_rxm(&mc, big_in_b, big_out_b, &cc, V17RX_OBJ_RESULT,
			V17RX_OBJ_RESULT_B1, V17RX_RESULT_B1_BIT1, 1, 1);
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
			V17RX_OBJ_RESULT_B1, V17RX_RESULT_B1_BIT1, 0, 0);
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

int
main(void)
{
	int rc = 0;

	rc |= run_seed();
	rc |= run_setenc();
	rc |= run_txstatus();
	rc |= run_rxm();
	rc |= run_cd();
	rc |= run_qd();
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

	diff_eq_int("the NULL guard separates (%ld)", sta_null_sep > 0, 1,
		    sta_null_sep);
	diff_eq_int("status +0x10's source separates (%ld)", sta_10_sep > 0, 1,
		    sta_10_sep);
	diff_eq_int("the status flag's mask separates (%ld)", sta_mask_sep > 0,
		    1, sta_mask_sep);
	diff_eq_int("writing status +0x0e separates (%ld)", sta_0e_sep > 0, 1,
		    sta_0e_sep);

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

	rc |= diff_end();
	return rc;
}
