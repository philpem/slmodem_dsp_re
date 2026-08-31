/*
 * t_v29fax.c -- differential test of V.29's receive entry points and the two
 *               transmitter accessors beside them.
 *
 * Ten of the eleven functions in `src/fax/v29.c` are covered here.  Most of
 * them are ACCESSORS: they pick one field out of an instance nothing models
 * and hand it back.  An accessor has no arithmetic to get wrong, so "it agreed
 * with the blob over a thousand random inputs" is worth almost nothing on its
 * own -- what can be wrong is WHICH FIELD, and a wrong offset agrees with the
 * right one over every input unless the fixture makes the two differ.
 *
 * So every check below is built around a NAMED WRONG READING, and the number
 * of trials that SEPARATED it is asserted non-zero at the end (finding F3052's
 * rule).  A zero there means the check above it is decoration.
 *
 * ---------------------------------------------------------------------------
 * THE WRONG READINGS, BY FUNCTION
 *
 *   CarrierDetectV29
 *       - the gate at rx + 0xd4 rather than 0xd0, and the AGC's signal at
 *         rx + 0x84 rather than 0x80.  The whole receiver block is
 *         pseudorandom, so a neighbouring int is a different number.
 *       - `&&` rather than `&`.  Separated by a trial where the two ints are
 *         2 and 1: the bitwise AND is 0 and the logical one is 1.
 *
 *   EpochDetectV29
 *       - the int at rx + 0x164.
 *       - the raw value returned rather than 0/1.  Separated by any value
 *         outside {0, 1}.
 *       - a 16-BIT test rather than a 32-bit one.  Separated by 0x10000,
 *         whose low half is zero; this is the reading that no random sweep
 *         ever reaches, because a random int has a zero low half once in
 *         65,536 trials.
 *
 *   GetSNRV29
 *       - the constant 13, which is V.17's.  Separated by every trial.
 *       - the error at rx + 0x170 or rx + 0x174.
 *       - NOT CLAIMED: the movzwl/movswl difference on rx + 0x172.  It is
 *         provably unobservable -- the result is truncated to 16 bits before
 *         it leaves, and the two readings differ only above that -- so this
 *         test asserts nothing about it and `v29.c` records it as free.
 *
 *   SeedScramblerV29 / SetEncoderV29
 *       - the store two, four and eight bytes away, and a 16-bit store
 *         instead of a 32-bit one.  The whole transmitter block is compared,
 *         so a store anywhere it should not be is caught by the same check.
 *       - SetEncoderV29's argument read as a 32-bit int.  THE OBJECT LOADS IT
 *         `movswl` AND USES THE 32-BIT RESULT, so `short` is forced -- and
 *         this test makes that a MEASUREMENT rather than a codegen note, by
 *         calling both sides through an `int`-taking pointer with 0x10000.
 *         The narrowing reading writes 0; the wide one writes nothing.
 *
 *   V29TX_status
 *       - +0x10 taken from the source's +0x00 rather than its +0x02.
 *       - the `& 0x04` mask dropped, and taken from the source's +0x11.
 *       - +0x0e zeroed, which the object does NOT do -- the one gap in an
 *         otherwise solid run of stores, and the easiest thing in the function
 *         to get wrong by tidying.
 *       - the null check.  Separated by construction: a null destination must
 *         return 0 and write nothing.
 *       - NOT CLAIMED: the dead store to +0x14.  It is observable only if the
 *         caller passes overlapping pointers, and what it would then measure
 *         is STATEMENT ORDER, which is a codegen-tier question this test is
 *         not equipped to settle.  See the note in v29.c.
 *
 *   V29RX_modem
 *       - a `while` rather than a `do`.  Separated by construction: entered
 *         with `*count` already zero, the object still dispatches the slot
 *         once and the loop-first reading dispatches it none, which the call
 *         log records.
 *       - `in` and `out` advanced by each other's quantity.
 *       - the running total not truncated to 16 bits.  Reached by a script
 *         that produces 40,000 samples in two calls; a stimulus sweep over
 *         plausible block sizes never gets there.
 *       - bit 0x200 of the status word left alone, or a different bit
 *         cleared, or the status read from +0x1c.
 *
 *   V29RX_delete
 *       - every one of the ten sub-objects taken from a neighbouring offset.
 *         WHAT IS CHECKED IS WHICH POINTERS WERE RELEASED, not that nothing
 *         crashed: the four buffers and the three created objects come from
 *         `sysdep_malloc`, so `harness_alloc_ordinal` reports 0 for each one
 *         that was freed and its ordinal for each one that was not, and the
 *         two sides' seven-bit vectors are compared.  The three BLOCKS are
 *         static arrays, so the object's frees of them land in `bad_free`
 *         and are counted rather than destroying the fixture.
 *
 *   DataCarrierDetectV29
 *       - the eleven readings enumerated in `dcd_defect` below, each one a
 *         single changed constant, offset or comparison in a local copy of
 *         the function that is otherwise identical and calls the same blob
 *         primitives.
 *
 *   QualityDetectV29
 *       - the eight readings in `q_defect`, INCLUDING THE SWAPPED SMOOTHING
 *         WEIGHTS.  Finding F8790: a swapped weight is invisible to every
 *         codegen check and to any one-block fixture, so this one is driven
 *         over sixty consecutive blocks with the state carried across, which
 *         is the only shape that can see it.
 *
 *   DemodDataV29
 *       - the eleven readings in `dem_defect`, driven over EIGHT consecutive
 *         blocks per trial because the resampler, the recoverer and the
 *         equaliser all carry history and a one-block fixture cannot see a
 *         stage fed from the wrong buffer (F8790's rule).
 *       - THE THREE ENABLE WORDS ARE SEEDED SO THAT ONE OF THEM HAS BIT 0
 *         CLEAR.  `agc.signal` is 0 or 1, so `signal & word` can only be 0 or
 *         `word & 1`; with all three words odd, transposing two of them is
 *         invisible.  F8885.
 *       - one of the three stimulus levels is SILENCE, which is the only one
 *         that makes `agc.signal` itself observable.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/v29fax.h"
#include "dsplib/v29data.h"

#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/sysdep.h"

/* --------------------------------------------------------------------- */
/* The blob's side.                                                      */

extern int   ref_CarrierDetectV29(void *modem);
extern int   ref_EpochDetectV29(void *modem);
extern short ref_GetSNRV29(void *modem);
extern void  ref_SeedScramblerV29(void *modem, int seed);
extern void  ref_SetEncoderV29(void *modem, short which);
extern int   ref_V29TX_status(void *tx, void *status);
extern void  ref_V29RX_delete(void *modem);
extern int   ref_V29RX_modem(void *modem, short *in, short *out,
			     unsigned short *count);
extern int   ref_DataCarrierDetectV29(void *modem, short *in,
				      unsigned short count);
extern int   ref_QualityDetectV29(void *modem);

extern void  ref_FPM_AGC_init(struct fpm_agc *agc, const struct fpm_agc_cfg *cfg,
			      int reset);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *state,
					  const struct fpm_mtd_cfg *cfg);
extern void *ref_FPM_TONE_create(void *state, const void *cfg);
extern void  ref_FPM_MTD_delete(struct fpm_mtd *state);
extern short ref_FPM_rms(const short *samples, unsigned short count);
extern short ref_FPM_MTD_detect(struct fpm_mtd *state, const short *samples,
				short count);
/*
 * THE AGC'S `%eax` IS `agc->signal`, AND THIS DECLARATION IS HOW THAT IS
 * MEASURED RATHER THAN BELIEVED.
 *
 * `FPM_AGC_agc` is `void` and takes three arguments; `DemodDataV29` in the
 * object nonetheless uses the register the definition happens to leave the
 * store to `signal` in.  `src/fax/v29.c` reads the field instead, which is
 * only correct while the identity holds, so the identity is asserted here on
 * every trial that runs an AGC.  Deviation D1035.
 */
extern void  ref_FPM_AGC_agc(struct fpm_agc *agc, short *samples,
			     unsigned short count);
typedef int (*agc_int_fn)(struct fpm_agc *agc, short *samples,
			  unsigned short count);

extern const struct fpm_agc_cfg AGCv22_CFG;

extern unsigned short ref_DemodDataV29(void *modem, short *in,
				       unsigned short *out, unsigned short count);
extern void  ref_FPM_MRF_init(struct fpm_mrf *state,
			      const struct fpm_mrf_cfg *cfg, int fresh);
extern void  ref_FPM_MRF_free(struct fpm_mrf *state);
extern short ref_FPM_MRF_filter(struct fpm_mrf *state, const short *in,
				short *out, short count);
extern void  ref_FPM_SRE_init(struct fpm_sre *sre,
			      const struct fpm_sre_cfg *cfg, int fresh);
extern void  ref_FPM_SRE_free(struct fpm_sre *sre);
extern unsigned short ref_FPM_SRE_recover(struct fpm_sre *sre, const short *in,
					  short *out, short count);
extern void  ref_FPM_FSE_init(struct fpm_fse *state,
			      const struct fpm_fse_cfg *cfg, int fresh);
extern void  ref_FPM_FSE_free(struct fpm_fse *state);
extern unsigned short ref_FPM_FSE_receive(struct fpm_fse *state,
					  const short *in, unsigned short *out,
					  unsigned short count);
extern void  ref_FPM_TONE_kill(void *state, short *samples, short count);
extern void  ref_FPM_TONE_delete(void *state);

/*
 * THE TWO "DEFAULT" CONFIGURATIONS ARE NOT USABLE AND THAT IS THE LIBRARY'S
 * OWN DOING.  `FPM_MRF_CFG` is 9:10 with a NULL coefficient pointer -- a
 * template, not a filter -- and `FPM_SRE_CFG`'s six table pointers are all
 * zero in the object.  The V.32 instances are the real ones and are what the
 * demodulator fixture uses.
 */
extern const struct fpm_mrf_cfg MRFv32_CFG;
extern const struct fpm_sre_cfg SREv32_CFG;

/* --------------------------------------------------------------------- */
/* The fixture                                                           */

#define RX_SIZE		0x5000
#define DET_SIZE	0x80
#define TX_SIZE		0xa0
#define OBJ_SIZE	0x60
#define STAT_SIZE	0x20
#define NBUF		512

#define NOUT		40016
#define OMARK		0x5ead

struct fix {
	unsigned char	rx[RX_SIZE];
	unsigned char	det[DET_SIZE];
	unsigned char	tx[TX_SIZE];
	unsigned char	obj[OBJ_SIZE];
	short		buf_det[NBUF];
	short		buf_v21[NBUF];
	short		buf_mrf[NBUF];
	short		buf_sre[NBUF];
	double		align;
};

static struct fix fa, fb, fc;

#define RX_AGC_OF(f)	((struct fpm_agc *)(void *)((f)->rx + V29RX_AGC))
#define RX_MRF_OF(f)	((struct fpm_mrf *)(void *)((f)->rx + V29RX_MRF))
#define RX_SRE_OF(f)	((struct fpm_sre *)(void *)((f)->rx + V29RX_SRE))
#define RX_FSE_OF(f)	((struct fpm_fse *)(void *)((f)->rx + V29RX_FSE))

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
put_ptr(unsigned char *p, int off, void *v)
{
	*(void **)(void *)(p + off) = v;
}

static void *
get_ptr(unsigned char *p, int off)
{
	return *(void **)(void *)(p + off);
}

static void
put_int(unsigned char *p, int off, int v)
{
	*(int *)(void *)(p + off) = v;
}

static int
get_int(const unsigned char *p, int off)
{
	return *(const int *)(const void *)(p + off);
}

static void
put_short(unsigned char *p, int off, short v)
{
	*(short *)(void *)(p + off) = v;
}

static short
get_short(const unsigned char *p, int off)
{
	return *(const short *)(const void *)(p + off);
}

/*
 * Lay one instance down.  Every block is filled pseudorandomly first, so an
 * offset wrong by two bytes reads garbage rather than a plausible zero, and
 * the four pointers are planted afterwards.
 */
static void
fixture(struct fix *f, unsigned seed)
{
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < RX_SIZE; i++)
		f->rx[i] = (unsigned char)rng_next();
	for (i = 0; i < DET_SIZE; i++)
		f->det[i] = (unsigned char)rng_next();
	for (i = 0; i < TX_SIZE; i++)
		f->tx[i] = (unsigned char)rng_next();
	for (i = 0; i < OBJ_SIZE; i++)
		f->obj[i] = (unsigned char)rng_next();
	for (i = 0; i < NBUF; i++) {
		f->buf_det[i] = (short)(rng_next() & 0x7fff);
		f->buf_v21[i] = (short)(rng_next() & 0x7fff);
		f->buf_mrf[i] = (short)(rng_next() & 0x7fff);
		f->buf_sre[i] = (short)(rng_next() & 0x7fff);
	}

	put_ptr(f->obj, V29_OBJ_TX, f->tx);
	put_ptr(f->obj, V29_OBJ_DET, f->det);
	put_ptr(f->obj, V29_OBJ_RX, f->rx);
	put_ptr(f->det, V29DET_BUF, f->buf_det);
	put_ptr(f->det, V29DET_V21_BUF, f->buf_v21);
	put_ptr(f->rx, V29RX_BUF_MRF, f->buf_mrf);
	put_ptr(f->rx, V29RX_BUF_SRE, f->buf_sre);
}

/*
 * The bytes a comparison must skip: the pointers, which are per-fixture
 * addresses and always differ, and nothing else.
 */
static int
skip_obj(int off)
{
	if (off >= V29_OBJ_TX && off < V29_OBJ_TX + 4)
		return 1;
	if (off >= V29_OBJ_DET && off < V29_OBJ_RX + 4)
		return 1;
	return 0;
}

static int
skip_det(int off)
{
	if (off < 8)
		return 1;			/* the MTD and TONE pointers */
	if (off >= V29DET_BUF && off < V29DET_BUF + 4)
		return 1;
	if (off >= V29DET_V21_MTD && off < V29DET_V21_BUF + 4)
		return 1;
	return 0;
}

static int
skip_rx(int off)
{
	if (off >= V29RX_BUF_MRF && off < V29RX_BUF_SRE + 4)
		return 1;
	/* fpm_agc_cfg's two coefficient pointers, planted by FPM_AGC_init. */
	if (off >= V29RX_AGC + 0x0c && off < V29RX_AGC + 0x14)
		return 1;
	return 0;
}

static long
blk_first_diff(const unsigned char *a, const unsigned char *b, int n,
	       int (*skip)(int))
{
	int i;

	for (i = 0; i < n; i++) {
		if (skip != 0 && skip(i))
			continue;
		if (a[i] != b[i])
			return i;
	}
	return -1;
}

static void
compare_state(struct fix *a, struct fix *b, long where)
{
	diff_eq_int("at %ld: first differing receiver byte",
		    blk_first_diff(b->rx, a->rx, RX_SIZE, skip_rx), -1, where);
	diff_eq_int("at %ld: first differing detector byte",
		    blk_first_diff(b->det, a->det, DET_SIZE, skip_det), -1,
		    where);
	diff_eq_int("at %ld: first differing transmitter byte",
		    blk_first_diff(b->tx, a->tx, TX_SIZE, 0), -1, where);
	diff_eq_int("at %ld: first differing instance byte",
		    blk_first_diff(b->obj, a->obj, OBJ_SIZE, skip_obj), -1,
		    where);
	diff_eq_int("at %ld: first differing det buffer byte",
		    blk_first_diff((unsigned char *)b->buf_det,
				   (unsigned char *)a->buf_det,
				   (int)sizeof(a->buf_det), 0), -1, where);
	diff_eq_int("at %ld: first differing v21 buffer byte",
		    blk_first_diff((unsigned char *)b->buf_v21,
				   (unsigned char *)a->buf_v21,
				   (int)sizeof(a->buf_v21), 0), -1, where);
}

/* --------------------------------------------------------------------- */
/* Separating-trial counters                                             */

static long acc_gate_sep, acc_signal_sep, acc_bitand_sep;
static long ep_off_sep, ep_bool_sep, ep_wide_sep;
static long snr_const_sep, snr_off_sep;
static long seed_off_sep, seed_width_sep;
static long enc_off_sep, enc_short_sep, enc_ignored_sep;
static long st_src_sep, st_mask_sep, st_gap_sep, st_assign_sep;
static long st_null_trials;
static long mdm_dowhile_sep, mdm_swap_sep, mdm_wrapped, mdm_status_sep;
static long mdm_ret_nonzero;
static long del_vector_trials, del_badfree_trials;
static long dcd_sep[16], dcd_paths[8];
static long q_sep[16], q_paths[6];

/* --------------------------------------------------------------------- */
/* The accessors                                                         */

static const int wide_values[] = {
	0, 1, 2, 3, -1, 0x10000, 0x7fffffff, (int)0x80000000, 0x00ff0000, 5
};
#define N_WIDE	((int)(sizeof(wide_values) / sizeof(wide_values[0])))

static const short snr_values[] = {
	0, 1, -1, 13, 14, 15, 100, (short)0x7fff, (short)0x8000, (short)0x4000
};
#define N_SNR	((int)(sizeof(snr_values) / sizeof(snr_values[0])))

static int
run_accessors(void)
{
	int i, j, s;
	static const unsigned seeds[] = { 0x1234abcdu, 0x0f1e2d3cu };

	diff_begin("V.29 accessors");

	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++) {
		for (i = 0; i < N_WIDE; i++) {
			for (j = 0; j < N_WIDE; j++) {
				int ra, rb;
				long where = (long)i * 100 + j;

				fixture(&fa, seeds[s]);
				fixture(&fb, seeds[s]);
				put_int(fa.rx, V29RX_SRE_ACTIVE, wide_values[i]);
				put_int(fb.rx, V29RX_SRE_ACTIVE, wide_values[i]);
				put_int(fa.rx, V29RX_AGC_SIGNAL, wide_values[j]);
				put_int(fb.rx, V29RX_AGC_SIGNAL, wide_values[j]);

				ra = ref_CarrierDetectV29(fa.obj);
				rb = CarrierDetectV29(fb.obj);
				diff_eq_int("at %ld: CarrierDetectV29", rb, ra,
					    where);
				compare_state(&fa, &fb, where);

				/* WRONG READING: the gate one int high. */
				if ((get_int(fa.rx, V29RX_SRE_ACTIVE + 4)
				     & wide_values[j]) != ra)
					acc_gate_sep++;
				/* WRONG READING: the signal one int high. */
				if ((wide_values[i]
				     & get_int(fa.rx, V29RX_AGC_SIGNAL + 4))
				    != ra)
					acc_signal_sep++;
				/* WRONG READING: `&&` rather than `&`. */
				if ((wide_values[i] && wide_values[j]) != ra)
					acc_bitand_sep++;
			}

			/* EpochDetectV29 over the same value set. */
			{
				int ra, rb;
				long where = 1000 + i;

				fixture(&fa, seeds[s]);
				fixture(&fb, seeds[s]);
				put_int(fa.rx, V29RX_FSE_LMS_FORCE, wide_values[i]);
				put_int(fb.rx, V29RX_FSE_LMS_FORCE, wide_values[i]);

				ra = ref_EpochDetectV29(fa.obj);
				rb = EpochDetectV29(fb.obj);
				diff_eq_int("at %ld: EpochDetectV29", rb, ra,
					    where);
				compare_state(&fa, &fb, where);

				/* WRONG READING: the int one word high. */
				if ((get_int(fa.rx, V29RX_FSE_LMS_FORCE + 4) != 0)
				    != ra)
					ep_off_sep++;
				/* WRONG READING: the raw value returned. */
				if (wide_values[i] != ra)
					ep_bool_sep++;
				/* WRONG READING: a 16-bit test. */
				if (((wide_values[i] & 0xffff) != 0) != ra)
					ep_wide_sep++;
			}
		}

		for (i = 0; i < N_SNR; i++) {
			short ra, rb;
			long where = 2000 + i;

			fixture(&fa, seeds[s]);
			fixture(&fb, seeds[s]);
			put_short(fa.rx, V29RX_FSE_MSE, snr_values[i]);
			put_short(fb.rx, V29RX_FSE_MSE, snr_values[i]);

			ra = ref_GetSNRV29(fa.obj);
			rb = GetSNRV29(fb.obj);
			diff_eq_int("at %ld: GetSNRV29", rb, ra, where);
			compare_state(&fa, &fb, where);

			/* WRONG READING: V.17's constant. */
			if ((short)(13 - snr_values[i]) != ra)
				snr_const_sep++;
			/* WRONG READING: the field two bytes either way. */
			if ((short)(14 - get_short(fa.rx, V29RX_FSE_MSE - 2))
			    != ra
			    || (short)(14 - get_short(fa.rx,
						      V29RX_FSE_MSE + 2))
			       != ra)
				snr_off_sep++;
		}
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* The transmitter accessors                                             */

static const int seed_values[] = {
	0, 1, -1, 0x12345678, (int)0xdeadbeef, 0x00010000, 0x0000ffff, 0x7fffffff
};
#define N_SEED	((int)(sizeof(seed_values) / sizeof(seed_values[0])))

static const short enc_values[] = {
	0, 1, 2, 3, -1, (short)0x7fff, (short)0x8000, 100
};
#define N_ENC	((int)(sizeof(enc_values) / sizeof(enc_values[0])))

static int
run_tx_accessors(void)
{
	int i;
	unsigned seed = 0x5a5a1234u;

	diff_begin("SeedScramblerV29 / SetEncoderV29");

	for (i = 0; i < N_SEED; i++) {
		long where = i;
		int before;

		fixture(&fa, seed + (unsigned)i);
		fixture(&fb, seed + (unsigned)i);
		before = get_int(fa.tx, V29TXFP_SCRAMBLER_SEED);

		ref_SeedScramblerV29(fa.obj, seed_values[i]);
		SeedScramblerV29(fb.obj, seed_values[i]);

		diff_eq_int("at %ld: the seed landed",
			    get_int(fb.tx, V29TXFP_SCRAMBLER_SEED),
			    get_int(fa.tx, V29TXFP_SCRAMBLER_SEED), where);
		compare_state(&fa, &fb, where);

		/*
		 * WRONG READING: the store four bytes either way, or only two
		 * bytes wide.  All three are caught by the whole-block
		 * comparison above; what is counted here is that this trial
		 * would have SEEN them.
		 */
		if (get_int(fa.tx, V29TXFP_SCRAMBLER_SEED - 4) != seed_values[i]
		    && get_int(fa.tx, V29TXFP_SCRAMBLER_SEED + 4)
		       != seed_values[i]
		    && seed_values[i] != before)
			seed_off_sep++;
		if ((seed_values[i] & ~0xffff) != (before & ~0xffff))
			seed_width_sep++;
	}

	for (i = 0; i < N_ENC; i++) {
		long where = 100 + i;
		int before;

		fixture(&fa, seed + 0x100u + (unsigned)i);
		fixture(&fb, seed + 0x100u + (unsigned)i);
		before = get_int(fa.tx, V29FP_SMC + 4);

		ref_SetEncoderV29(fa.obj, enc_values[i]);
		SetEncoderV29(fb.obj, enc_values[i]);

		diff_eq_int("at %ld: SetEncoderV29 wrote the same",
			    get_int(fb.tx, V29FP_SMC + 4),
			    get_int(fa.tx, V29FP_SMC + 4), where);
		compare_state(&fa, &fb, where);

		if (enc_values[i] == 0 || enc_values[i] == 1) {
			diff_eq_int("at %ld: 0 and 1 write themselves",
				    get_int(fa.tx, V29FP_SMC + 4),
				    enc_values[i], where);
			if (get_int(fa.tx, V29FP_SMC) != enc_values[i]
			    && get_int(fa.tx, V29FP_SMC + 8) != enc_values[i]
			    && before != enc_values[i])
				enc_off_sep++;
		} else {
			/*
			 * WRONG READING: any other value clamped, or written
			 * through.  The object writes NOTHING.
			 */
			diff_eq_int("at %ld: anything else writes nothing",
				    get_int(fa.tx, V29FP_SMC + 4), before,
				    where);
			enc_ignored_sep++;
		}
	}

	/*
	 * WRONG READING: the argument read as a 32-bit int.  Both sides are
	 * called through an `int`-taking pointer with 0x10000, whose low 16
	 * bits are zero: the object's `movswl` sees 0 and writes 0, a 32-bit
	 * read sees 0x10000 and writes nothing.  This is the trial that turns
	 * "short is forced by the codegen" into a measurement.
	 */
	{
		void (*ref_i)(void *, int);
		void (*our_i)(void *, int);
		static const int wides[] = { 0x10000, 0x10001, -0x10000,
					     (int)0xffff0001 };
		int k;

		ref_i = (void (*)(void *, int))ref_SetEncoderV29;
		our_i = (void (*)(void *, int))SetEncoderV29;

		for (k = 0; k < (int)(sizeof(wides) / sizeof(wides[0])); k++) {
			int before;
			long where = 200 + k;

			fixture(&fa, seed + 0x200u + (unsigned)k);
			fixture(&fb, seed + 0x200u + (unsigned)k);
			before = get_int(fa.tx, V29FP_SMC + 4);

			ref_i(fa.obj, wides[k]);
			our_i(fb.obj, wides[k]);

			diff_eq_int("at %ld: a wide argument agrees",
				    get_int(fb.tx, V29FP_SMC + 4),
				    get_int(fa.tx, V29FP_SMC + 4), where);
			compare_state(&fa, &fb, where);

			/*
			 * The narrowing reading and the wide one disagree
			 * whenever the low half is 0 or 1 and the whole is
			 * not.  Count only those.
			 */
			if ((wides[k] & 0xffff) <= 1
			    && get_int(fa.tx, V29FP_SMC + 4) != before)
				enc_short_sep++;
		}
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V29TX_status                                                          */

static unsigned char sta[STAT_SIZE], stb[STAT_SIZE];

static int
run_status(void)
{
	int i;

	diff_begin("V29TX_status");

	for (i = 0; i < 24; i++) {
		int ra, rb, k;
		unsigned char pre14;
		long where = i;

		fixture(&fa, 0x77000000u + (unsigned)i);
		fixture(&fb, 0x77000000u + (unsigned)i);

		rng_seed(0x31415926u + (unsigned)i);
		for (k = 0; k < STAT_SIZE; k++)
			sta[k] = stb[k] = (unsigned char)rng_next();
		/*
		 * EVERY BIT SET IN THE FLAGS BYTE, so the merging reading
		 * cannot agree with the assigning one by accident.
		 */
		if ((i & 1) == 0)
			sta[V29STAT_FLAGS] = stb[V29STAT_FLAGS] = 0xff;
		pre14 = sta[V29STAT_FLAGS];

		ra = ref_V29TX_status(fa.tx, sta);
		rb = V29TX_status(fb.tx, stb);

		diff_eq_int("at %ld: V29TX_status returned", rb, ra, where);
		diff_eq_int("at %ld: first differing status byte",
			    blk_first_diff(stb, sta, STAT_SIZE, 0), -1, where);
		compare_state(&fa, &fb, where);

		/* WRONG READING: +0x10 from the source's +0x00. */
		if (get_short(fa.tx, V29TXS_PROTOCOL)
		    != get_short(fa.tx, V29TXS_BITRATE))
			st_src_sep++;
		/* WRONG READING: the `& 0x04` mask dropped or shifted. */
		if (fa.tx[V29TXS_FLAGS_10] != (fa.tx[V29TXS_FLAGS_10] & 0x04)
		    || (fa.tx[V29TXS_FLAGS_10 + 1] & 0x04)
		       != (fa.tx[V29TXS_FLAGS_10] & 0x04))
			st_mask_sep++;
		/* WRONG READING: +0x0e zeroed.  The object leaves it alone. */
		if (get_short(sta, 0x0e) != 0)
			st_gap_sep++;
		/*
		 * WRONG READING: the flags byte MERGED rather than assigned,
		 * which is what the `&= 0xfc` a statement earlier makes a
		 * reader expect.  Deviation D1035.
		 */
		if ((unsigned char)((pre14 & (unsigned char)~V29STAT_FLAGS_LOW2)
				    | (fa.tx[V29TXS_FLAGS_10] & V29TXS_10_BIT2))
		    != sta[V29STAT_FLAGS])
			st_assign_sep++;
	}

	/* The null destination, separated by construction. */
	{
		int ra, rb;

		fixture(&fa, 0x99887766u);
		fixture(&fb, 0x99887766u);
		ra = ref_V29TX_status(fa.tx, 0);
		rb = V29TX_status(fb.tx, 0);
		diff_eq_int("a null status returns %ld", rb, ra, (long)ra);
		diff_eq_int("a null status returns zero", ra, 0, 0);
		compare_state(&fa, &fb, 0);
		st_null_trials++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V29RX_modem                                                           */

/*
 * The demodulator slot, shared by both sides and scripted.  It logs what it
 * was handed, marks its output, consumes and produces what the script says,
 * and stops the loop by driving `*count` to zero.
 */
struct slot_step {
	unsigned short consume;
	short produce;
};

struct slot_log {
	int	calls;
	void	*modem;
	short	*in[8];
	short	*out[8];
	unsigned short count_in[8];
	long	total_produced;
};

static const struct slot_step *slot_script;
static int slot_script_len;
static int slot_pos;
static struct slot_log slog;

static short
slot_fn(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short take;
	short give;

	if (slog.calls < 8) {
		slog.in[slog.calls] = in;
		slog.out[slog.calls] = out;
		slog.count_in[slog.calls] = *count;
	}
	slog.modem = modem;
	slog.calls++;

	if (slot_pos < slot_script_len) {
		take = slot_script[slot_pos].consume;
		give = slot_script[slot_pos].produce;
		slot_pos++;
	} else {
		take = *count;
		give = 0;
	}
	if (take > *count)
		take = *count;
	*count = (unsigned short)(*count - take);
	slog.total_produced += give;

	/* A mark in the output, so a wrong `out` advance reaches memory. */
	out[0] = (short)(0x2900 + (slog.calls & 0xff));

	return give;
}

static short obuf_a[NOUT], obuf_b[NOUT];
static short ibuf[NBUF];

static void
run_modem_one(const struct slot_step *script, int len, unsigned short count,
	      unsigned seed, long where)
{
	int ra, rb, k;
	unsigned short ca, cb;
	struct slot_log la, lb;

	fixture(&fa, seed);
	fixture(&fb, seed);
	put_ptr(fa.det, V29DET_DEMOD, (void *)slot_fn);
	put_ptr(fb.det, V29DET_DEMOD, (void *)slot_fn);

	rng_seed(seed ^ 0x2f2f2f2fu);
	for (k = 0; k < NBUF; k++)
		ibuf[k] = (short)(rng_next() & 0x3fff);
	for (k = 0; k < NOUT; k++)
		obuf_a[k] = obuf_b[k] = (short)OMARK;

	slot_script = script;
	slot_script_len = len;

	slot_pos = 0;
	memset(&slog, 0, sizeof(slog));
	ca = count;
	ra = ref_V29RX_modem(fa.obj, ibuf, obuf_a, &ca);
	la = slog;

	slot_pos = 0;
	memset(&slog, 0, sizeof(slog));
	cb = count;
	rb = V29RX_modem(fb.obj, ibuf, obuf_b, &cb);
	lb = slog;

	diff_eq_int("at %ld: V29RX_modem returned", rb, ra, where);
	diff_eq_int("at %ld: the count came back", (long)cb, (long)ca, where);
	diff_eq_int("at %ld: the slot was called the same number of times",
		    lb.calls, la.calls, where);
	diff_eq_int("at %ld: with the same instance",
		    lb.modem == (void *)fb.obj && la.modem == (void *)fa.obj, 1,
		    where);
	for (k = 0; k < la.calls && k < 8; k++) {
		diff_eq_int("call %ld: the input offset",
			    (long)(lb.in[k] - ibuf), (long)(la.in[k] - ibuf),
			    k);
		diff_eq_int("call %ld: the output offset",
			    (long)(lb.out[k] - obuf_b),
			    (long)(la.out[k] - obuf_a), k);
		diff_eq_int("call %ld: the count in",
			    (long)lb.count_in[k], (long)la.count_in[k], k);
	}
	diff_eq_int("at %ld: first differing output byte",
		    blk_first_diff((unsigned char *)obuf_b,
				   (unsigned char *)obuf_a,
				   (int)sizeof(obuf_a), 0), -1, where);
	compare_state(&fa, &fb, where);

	/* The status word: bit 0x200 clear, everything else untouched. */
	diff_eq_int("at %ld: bit 0x200 was cleared",
		    get_int(fa.obj, V29_OBJ_STATUS) & V29_STATUS_0200, 0,
		    where);
	if (ra != 0)
		mdm_ret_nonzero++;

	/* WRONG READING: `in` and `out` advanced by each other's quantity. */
	if (la.calls > 1
	    && (la.in[1] - ibuf) != (la.out[1] - obuf_a))
		mdm_swap_sep++;

	/*
	 * WRONG READING: a `while` rather than a `do`.  With a zero count on
	 * entry the object dispatches the slot exactly once.
	 */
	if (count == 0 && la.calls == 1)
		mdm_dowhile_sep++;

	/*
	 * NOT A WRONG READING, AND SAID SO RATHER THAN COUNTED.  The running
	 * total is a `short` in the object and re-narrowed with `cwtl` every
	 * iteration, but the only place it is ever read is a 16-bit store into
	 * `*count` -- so an `int` accumulator would leave the same sixteen bits
	 * and NOTHING can separate the two.  What is counted here is that the
	 * corner was REACHED, which is path coverage and not a verdict.
	 */
	if (la.total_produced > 0x7fff)
		mdm_wrapped++;
}

static int
run_modem(void)
{
	static const struct slot_step s1[] = { { 10, 3 } };
	static const struct slot_step s2[] = { { 4, 2 }, { 4, 2 }, { 2, 1 } };
	static const struct slot_step s3[] = { { 0, 5 }, { 0, 5 }, { 40, 7 } };
	static const struct slot_step s4[] = { { 0, 20000 }, { 0, 20000 },
					       { 1, 3 } };
	static const struct slot_step s0[] = { { 0, 0 } };

	diff_begin("V29RX_modem");

	run_modem_one(s1, 1, 10, 0x0c0ffee0u, 1);
	run_modem_one(s2, 3, 10, 0x0c0ffee1u, 2);
	run_modem_one(s3, 3, 40, 0x0c0ffee2u, 3);
	run_modem_one(s2, 3, 1, 0x0c0ffee3u, 4);
	/* The far corner: 40,000 samples produced, so the total wraps. */
	run_modem_one(s4, 3, 1, 0x0c0ffee4u, 5);
	/* Entered with nothing to do: the slot still runs once. */
	run_modem_one(s0, 1, 0, 0x0c0ffee5u, 6);

	/*
	 * WRONG READING: the status word left alone, a different bit cleared,
	 * or read from +0x1c.  Evaluated for real: the instance is seeded with
	 * every bit set, so a wrong bit or a wrong offset is a different
	 * return value.
	 */
	{
		int ra, rb;
		unsigned short ca, cb;
		static const struct slot_step s[] = { { 8, 2 } };
		int k;

		fixture(&fa, 0x0c0ffee6u);
		fixture(&fb, 0x0c0ffee6u);
		put_ptr(fa.det, V29DET_DEMOD, (void *)slot_fn);
		put_ptr(fb.det, V29DET_DEMOD, (void *)slot_fn);
		put_int(fa.obj, V29_OBJ_STATUS, -1);
		put_int(fb.obj, V29_OBJ_STATUS, -1);
		put_int(fa.obj, V29_OBJ_STATUS + 4, -1);
		put_int(fb.obj, V29_OBJ_STATUS + 4, -1);
		for (k = 0; k < NOUT; k++)
			obuf_a[k] = obuf_b[k] = (short)OMARK;

		slot_script = s;
		slot_script_len = 1;
		slot_pos = 0;
		memset(&slog, 0, sizeof(slog));
		ca = 8;
		ra = ref_V29RX_modem(fa.obj, ibuf, obuf_a, &ca);
		slot_pos = 0;
		memset(&slog, 0, sizeof(slog));
		cb = 8;
		rb = V29RX_modem(fb.obj, ibuf, obuf_b, &cb);

		diff_eq_int("the seeded status came back as %ld", rb, ra,
			    (long)ra);
		diff_eq_int("exactly bit 0x200 was cleared", ra, ~V29_STATUS_0200,
			    0);
		diff_eq_int("the next word is untouched",
			    get_int(fa.obj, V29_OBJ_STATUS + 4), -1, 0);
		if (ra != -1)
			mdm_status_sep++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V29RX_delete                                                          */

/*
 * WHAT A DELETE TEST CAN ACTUALLY CHECK is which pointers were released.
 *
 * The three BLOCKS are static arrays, so the object's frees of them reach
 * `sysdep_free` as pointers the allocator never handed out: they are counted
 * in `bad_free` and swallowed, which is what keeps the fixture readable
 * afterwards.  The seven things that ARE allocated -- four buffers and three
 * created sub-objects -- are probed one at a time with
 * `harness_alloc_ordinal`, which reports 0 for a pointer no longer live.  A
 * wrong offset reads a pseudorandom pointer instead, which lands in
 * `bad_free` and leaves the right one live, so both halves of the vector
 * move.
 */
#define N_PROBE	7

static void
delete_build(struct fix *f, unsigned seed, void *probe[N_PROBE])
{
	void *p;

	fixture(f, seed);

	/* The three sub-objects with real destructors. */
	put_ptr(f->det, V29DET_MTD, ref_FPM_MTD_create(0, 0));
	put_ptr(f->det, V29DET_TONE, ref_FPM_TONE_create(0, 0));
	put_ptr(f->det, V29DET_V21_MTD, ref_FPM_MTD_create(0, 0));

	/* The four plain buffers, from the tracked allocator. */
	p = sysdep_malloc(NBUF * sizeof(short));
	put_ptr(f->rx, V29RX_BUF_MRF, p);
	p = sysdep_malloc(NBUF * sizeof(short));
	put_ptr(f->rx, V29RX_BUF_SRE, p);
	p = sysdep_malloc(NBUF * sizeof(short));
	put_ptr(f->det, V29DET_BUF, p);
	p = sysdep_malloc(NBUF * sizeof(short));
	put_ptr(f->det, V29DET_V21_BUF, p);

	/*
	 * The three embedded FPM states are zeroed, so their `_free`
	 * functions release null pointers and land in `free_null` rather than
	 * chasing the block's pseudorandom filler.  That the object calls
	 * them AT ALL is what `free_null` counts.
	 */
	memset(f->rx + V29RX_MRF, 0, 0x1c);
	memset(f->rx + V29RX_SRE, 0, 0x90);
	memset(f->rx + V29RX_FSE, 0, 0x4e18);

	probe[0] = get_ptr(f->det, V29DET_MTD);
	probe[1] = get_ptr(f->det, V29DET_TONE);
	probe[2] = get_ptr(f->det, V29DET_V21_MTD);
	probe[3] = get_ptr(f->rx, V29RX_BUF_MRF);
	probe[4] = get_ptr(f->rx, V29RX_BUF_SRE);
	probe[5] = get_ptr(f->det, V29DET_BUF);
	probe[6] = get_ptr(f->det, V29DET_V21_BUF);
}

static int
run_delete(void)
{
	void *pa[N_PROBE], *pb[N_PROBE];
	int liva[N_PROBE], livb[N_PROBE];
	struct alloc_log before, mid, after;
	int i, s;
	static const unsigned seeds[] = { 0x0de1e7e0u, 0x0de1e7e1u };

	diff_begin("V29RX_delete");

	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++) {
		delete_build(&fa, seeds[s], pa);
		before = harness_alloc;
		ref_V29RX_delete(fa.obj);
		mid = harness_alloc;
		for (i = 0; i < N_PROBE; i++)
			liva[i] = harness_alloc_ordinal(pa[i]) != 0;

		delete_build(&fb, seeds[s], pb);
		V29RX_delete(fb.obj);
		after = harness_alloc;
		for (i = 0; i < N_PROBE; i++)
			livb[i] = harness_alloc_ordinal(pb[i]) != 0;

		for (i = 0; i < N_PROBE; i++)
			diff_eq_int("probe %ld still live", livb[i], liva[i],
				    i);
		diff_eq_int("seed %ld: everything the delete owns was released",
			    liva[0] + liva[1] + liva[2] + liva[3] + liva[4]
			    + liva[5] + liva[6], 0, s);

		diff_eq_int("seed %ld: frees", after.frees - mid.frees,
			    mid.frees - before.frees, s);
		diff_eq_int("seed %ld: null frees",
			    after.free_null - mid.free_null,
			    mid.free_null - before.free_null, s);
		diff_eq_int("seed %ld: unknown frees",
			    after.bad_free - mid.bad_free,
			    mid.bad_free - before.bad_free, s);
		/*
		 * The three static blocks: rx, det and the instance.  The
		 * object frees all three unconditionally, so the count is
		 * exactly three and a missing one is visible here.
		 */
		diff_eq_int("seed %ld: the three static blocks were freed",
			    mid.bad_free - before.bad_free, 3, s);
		if (mid.bad_free - before.bad_free == 3)
			del_badfree_trials++;
		if (mid.frees - before.frees > 0)
			del_vector_trials++;

		/* The blocks survived, because the allocator swallowed them. */
		diff_eq_int("seed %ld: first differing detector byte",
			    blk_first_diff(fb.det, fa.det, DET_SIZE, skip_det),
			    -1, s);
		diff_eq_int("seed %ld: first differing instance byte",
			    blk_first_diff(fb.obj, fa.obj, OBJ_SIZE, skip_obj),
			    -1, s);
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* QualityDetectV29                                                      */

/*
 * The wrong readings, one per identifier.  Each changes exactly one thing in
 * a local copy that is otherwise the object's own arithmetic.
 */
enum q_defect {
	Q_NONE = 0,
	Q_WEIGHTS_SWAPPED,	/* 0x7333 and 0xccd exchanged             */
	Q_NO_ROUND,		/* the +0x4000 dropped                    */
	Q_SHIFT_16,		/* >> 16 rather than >> 15                */
	Q_SEED_ZERO,		/* block 0 seeds the average with 0       */
	Q_LAST_BLOCK,		/* the fold runs to 0x32, not 0x31        */
	Q_VERDICT_INVERT,	/* the flag set when the average IS above */
	Q_NO_SATURATE,		/* the counter keeps running past 0x33    */
	Q_CARRIER_ONE,		/* a clear carrier reports 1, not 2       */
	Q_MAX
};

/* The observable: the return, and the four shorts the function can write. */
struct q_obs {
	int	ret;
	short	avg, n, limit, verdict;
};

static void
q_read(struct fix *f, struct q_obs *o, int ret)
{
	o->ret = ret;
	o->avg = get_short(f->rx, V29RX_DEC_ERROR_AVG);
	o->n = get_short(f->rx, V29RX_DEC_ERROR_N);
	o->limit = get_short(f->rx, V29RX_DEC_ERROR_LIMIT);
	o->verdict = get_short(f->rx, V29RX_SHORT_4F62);
}

static int
q_same(const struct q_obs *a, const struct q_obs *b)
{
	return a->ret == b->ret && a->avg == b->avg && a->n == b->n
	       && a->limit == b->limit && a->verdict == b->verdict;
}

/* The object's own arithmetic, with one reading changed. */
static int
drive_quality(struct fix *f, enum q_defect d)
{
	unsigned char *rx = f->rx;
	int err = get_short(rx, V29RX_FSE_MSE);
	int keep = (d == Q_WEIGHTS_SWAPPED) ? V29Q_AVG_NEW : V29Q_AVG_KEEP;
	int fresh = (d == Q_WEIGHTS_SWAPPED) ? V29Q_AVG_KEEP : V29Q_AVG_NEW;
	int round = (d == Q_NO_ROUND) ? 0 : V29Q_AVG_ROUND;
	int shift = (d == Q_SHIFT_16) ? 16 : V29Q_AVG_SHIFT;
	int last = (d == Q_LAST_BLOCK) ? V29Q_VERDICT_BLOCK : V29Q_AVG_BLOCKS;
	int verdict;
	short n;

	verdict = (short)(get_int(rx, V29RX_SRE_ACTIVE)
			  & get_int(rx, V29RX_AGC_SIGNAL));
	if (verdict == 0)
		verdict = (d == Q_CARRIER_ONE) ? 1 : V29Q_NO_CARRIER;

	n = get_short(rx, V29RX_DEC_ERROR_N);
	if (n == 0) {
		put_short(rx, V29RX_DEC_ERROR_AVG,
			  (d == Q_SEED_ZERO) ? 0 : (short)err);
		put_short(rx, V29RX_DEC_ERROR_N, 1);
		return verdict;
	}
	if (n > last) {
		if (n != V29Q_VERDICT_BLOCK) {
			if (d != Q_NO_SATURATE)
				return verdict;
		} else {
			int gt = get_short(rx, V29RX_DEC_ERROR_AVG)
				 > get_short(rx, V29RX_DEC_ERROR_LIMIT);

			if ((d == Q_VERDICT_INVERT) ? gt : !gt)
				put_short(rx, V29RX_SHORT_4F62, 1);
		}
	} else {
		put_short(rx, V29RX_DEC_ERROR_AVG, (short)
			  (((get_short(rx, V29RX_DEC_ERROR_AVG) * keep + round)
			    >> shift)
			   + ((err * fresh + round) >> shift)));
	}
	put_short(rx, V29RX_DEC_ERROR_N,
		  (short)(get_short(rx, V29RX_DEC_ERROR_N) + 1));
	return verdict;
}

/* The pre-call snapshot every wrong reading is driven from. */
static struct fix fd;

#define Q_BLOCKS	60

static int
run_quality(void)
{
	int s, blk, d, lim, carrier;
	static const unsigned seeds[] = { 0x0da7a001u, 0x0da7a002u };
	static const short limits[] = { 900, 30000 };

	diff_begin("QualityDetectV29");

	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++)
	for (lim = 0; lim < (int)(sizeof(limits) / sizeof(limits[0])); lim++)
	for (carrier = 0; carrier < 2; carrier++) {
		struct q_obs oa, ob, oc;

		fixture(&fa, seeds[s]);
		fixture(&fb, seeds[s]);

		put_int(fa.rx, V29RX_SRE_ACTIVE, carrier ? 3 : 0);
		put_int(fb.rx, V29RX_SRE_ACTIVE, carrier ? 3 : 0);
		put_int(fa.rx, V29RX_AGC_SIGNAL, carrier ? 1 : 0);
		put_int(fb.rx, V29RX_AGC_SIGNAL, carrier ? 1 : 0);
		put_short(fa.rx, V29RX_DEC_ERROR_N, 0);
		put_short(fb.rx, V29RX_DEC_ERROR_N, 0);
		put_short(fa.rx, V29RX_DEC_ERROR_AVG, 0);
		put_short(fb.rx, V29RX_DEC_ERROR_AVG, 0);
		put_short(fa.rx, V29RX_DEC_ERROR_LIMIT, limits[lim]);
		put_short(fb.rx, V29RX_DEC_ERROR_LIMIT, limits[lim]);
		put_short(fa.rx, V29RX_SHORT_4F62, 0);
		put_short(fb.rx, V29RX_SHORT_4F62, 0);

		/*
		 * SIXTY BLOCKS, state carried across.  The counter runs
		 * 0, 1 ... 0x33 within this, so the seed, the fold, the verdict
		 * at 0x32 and the saturation past it are all reached in one
		 * sweep -- and the smoothing weights, which no single block can
		 * separate, are separated by the trajectory.  Finding F8790 is
		 * why this is not a one-block test.
		 */
		for (blk = 0; blk < Q_BLOCKS; blk++) {
			long where = (long)s * 100000 + (long)lim * 10000
				     + (long)carrier * 1000 + blk;
			short err = (short)(100 + blk * 37);

			put_short(fa.rx, V29RX_FSE_MSE, err);
			put_short(fb.rx, V29RX_FSE_MSE, err);

			fd = fa;		/* the pre-call state */

			q_read(&fa, &oa, ref_QualityDetectV29(fa.obj));
			q_read(&fb, &ob, QualityDetectV29(fb.obj));

			diff_eq_int("at %ld: returned", ob.ret, oa.ret, where);
			diff_eq_int("at %ld: the running average", ob.avg,
				    oa.avg, where);
			diff_eq_int("at %ld: the block counter", ob.n, oa.n,
				    where);
			diff_eq_int("at %ld: the limit is untouched", ob.limit,
				    oa.limit, where);
			diff_eq_int("at %ld: the verdict flag", ob.verdict,
				    oa.verdict, where);
			compare_state(&fa, &fb, where);

			if (oa.n == 1)
				q_paths[0]++;
			if (oa.n > 1 && oa.n <= V29Q_AVG_BLOCKS)
				q_paths[1]++;
			if (blk == V29Q_VERDICT_BLOCK)
				q_paths[2]++;
			if (blk > V29Q_VERDICT_BLOCK)
				q_paths[3]++;
			if (oa.ret == V29Q_NO_CARRIER)
				q_paths[4]++;
			if (oa.ret == 1)
				q_paths[5]++;

			for (d = 1; d < (int)Q_MAX; d++) {
				int r;

				fc = fd;
				r = drive_quality(&fc, (enum q_defect)d);
				q_read(&fc, &oc, r);
				if (!q_same(&oc, &oa))
					q_sep[d]++;
			}
		}
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* DataCarrierDetectV29                                                  */

enum dcd_defect {
	D_NONE = 0,
	D_AGE_GE,		/* >= 999 rather than > 999                */
	D_ERR_LT,		/* < rather than <= at the error limit     */
	D_AND_LOGICAL,		/* `&&` rather than `&` on the mask        */
	D_NO_MASK,		/* the `& (mse <= max)` dropped entirely   */
	D_V21_THRESH,		/* 0x500 rather than 0x4ff                 */
	D_V21_NO_RESET,		/* the sample count not reset on a hit     */
	D_V21_DETECT_ONE,	/* the V.21 hit reports 1 rather than 0    */
	D_RMS_HALF,		/* 0x4000 (-6 dB) rather than 0x32fe       */
	D_RMS_SHIFT,		/* >> 14 rather than >> 15                 */
	D_RMS_PERIOD,		/* the reference replaced every block      */
	D_RMS_NO_ZERO,		/* the drop not clearing the verdict       */
	D_FORCE_ONE,		/* the armed path not forcing 1            */
	D_MAX
};

struct dcd_obs {
	int	ret;
	short	v21_samples, v21_enable, rms_ref, rms_n;
};

static void
dcd_read(struct fix *f, struct dcd_obs *o, int ret)
{
	o->ret = ret;
	o->v21_samples = get_short(f->det, V29DET_V21_SAMPLES);
	o->v21_enable = get_short(f->det, V29DET_V21_ENABLE);
	o->rms_ref = get_short(f->rx, V29RX_RMS_REF);
	o->rms_n = get_short(f->rx, V29RX_RMS_N);
}

static int
dcd_same(const struct dcd_obs *a, const struct dcd_obs *b)
{
	return a->ret == b->ret && a->v21_samples == b->v21_samples
	       && a->v21_enable == b->v21_enable && a->rms_ref == b->rms_ref
	       && a->rms_n == b->rms_n;
}

static int
drive_dcd(struct fix *f, short *in, unsigned short count, enum dcd_defect d)
{
	unsigned char *rx = f->rx;
	unsigned char *det = f->det;
	int detected = (short)(get_int(rx, V29RX_SRE_ACTIVE)
			       & get_int(rx, V29RX_AGC_SIGNAL));
	int err_ok;
	short rms;

	err_ok = (d == D_ERR_LT)
		 ? (get_short(rx, V29RX_FSE_MSE) < V29RX_DEC_ERROR_MAX)
		 : (get_short(rx, V29RX_FSE_MSE) <= V29RX_DEC_ERROR_MAX);

	if (get_short(det, V29DET_GATE_1C) == 0) {
		int aged = (d == D_AGE_GE)
			   ? (get_short(rx, V29RX_SHORT_0046) >= 999)
			   : (get_short(rx, V29RX_SHORT_0046) > 999);

		if (aged) {
			if (d == D_NO_MASK)
				;
			else if (d == D_AND_LOGICAL)
				detected = detected && err_ok;
			else
				detected &= err_ok;
		}
	} else {
		if ((detected & err_ok) == 0)
			put_short(det, V29DET_V21_ENABLE, 1);

		if (d != D_FORCE_ONE)
			detected = 1;

		if (get_short(det, V29DET_V21_ENABLE) != 0) {
			short *buf = (short *)get_ptr(det, V29DET_V21_BUF);
			int i;
			int thresh = (d == D_V21_THRESH)
				     ? 0x500 : V29DET_V21_THRESHOLD;

			for (i = 0; i < (int)count; i++)
				buf[i] = in[i];

			ref_FPM_AGC_agc((struct fpm_agc *)(void *)
					(det + V29DET_V21_AGC), buf, count);

			if (ref_FPM_MTD_detect((struct fpm_mtd *)
						get_ptr(det, V29DET_V21_MTD),
					       buf, (short)count) != 0) {
				if (d != D_V21_NO_RESET)
					put_short(det, V29DET_V21_SAMPLES, 0);
			} else {
				put_short(det, V29DET_V21_SAMPLES, (short)
					  (get_short(det, V29DET_V21_SAMPLES)
					   + count));
			}

			if (get_short(det, V29DET_V21_SAMPLES) > thresh)
				detected = (d == D_V21_DETECT_ONE) ? 1 : 0;
		}
	}

	if (get_short(rx, V29RX_SHORT_4F64) == 0)
		return detected;

	rms = ref_FPM_rms(in, count);
	{
		int c = (d == D_RMS_HALF) ? 0x4000 : V29RX_RMS_DROP_Q15;
		int sh = (d == D_RMS_SHIFT) ? 14 : V29RX_RMS_SHIFT;
		int gate = (get_short(rx, V29RX_RMS_REF) * c) >> sh;

		if (rms < gate && d != D_RMS_NO_ZERO)
			detected = 0;
	}

	if ((short)(get_short(rx, V29RX_RMS_N) + 1)
	    == ((d == D_RMS_PERIOD) ? 1 : 2)) {
		put_short(rx, V29RX_RMS_REF, rms);
		put_short(rx, V29RX_RMS_N, 0);
	} else {
		put_short(rx, V29RX_RMS_N,
			  (short)(get_short(rx, V29RX_RMS_N) + 1));
	}

	return detected;
}

/*
 * Build the two sub-objects `DataCarrierDetectV29` really uses.  Both are
 * constructed by the BLOB's own code on every side, so the states are
 * identical bytes and any difference the test sees is this file's.
 *
 * The MTD is created per fixture and DELETED again after every trial: the
 * harness's live set is finite and a leak per trial would overflow it, which
 * would silently make `harness_alloc`'s counts unreliable for the delete test
 * running before this one.
 */
struct dcd_setup {
	short	gate_1c, enable, samples, age, mse, rms_gate, rms_ref, rms_n;
	int	carrier;
	int	mtd_absent;	/* 1: the detector must report ABSENT      */
	int	auto_ref;	/* 1: put the drop gate around this block  */
};

/*
 * THE TONE DETECTOR'S VERDICT IS CONFIGURED, NOT HOPED FOR.
 *
 * The first two versions of this test drove the V.21 scan with noise and with
 * a 3 kHz tone and asserted that three wrong readings separated.  All three
 * reported zero, and the reason was the same for all three: `FPM_MTD_detect`
 * never once returned ABSENT, so the branch that ACCUMULATES samples -- the
 * only branch that can reach the 0x4ff threshold -- was never entered.  A
 * stimulus sweep cannot fix that, because which verdict a bank produces is a
 * property of its coefficients and not of the signal alone.
 *
 * `fpm_mtd.h` makes the verdict a function of two config words instead:
 * NOSIGNAL when the wideband energy is below `min_level`, PRESENT when the
 * out-of-band share is at or below `ratio` of it, ABSENT otherwise.  A
 * NEGATIVE `ratio` puts the threshold below zero, and `out_of_band` is clamped
 * at or above zero, so ABSENT is forced; a `min_level` of 0x7fff forces
 * NOSIGNAL, since the energy is a `short`.  Both sides get the same
 * configuration, so this steers the FIXTURE and not the answer.
 */
static struct fpm_mtd_cfg mtd_absent_cfg, mtd_nosignal_cfg;

static void
dcd_cfgs(void)
{
	struct fpm_mtd *m = ref_FPM_MTD_create(0, 0);

	/*
	 * The coefficients come from the library's OWN default, taken out of a
	 * detector it built itself rather than named from a table -- there are
	 * two candidate config symbols and only the constructor knows which of
	 * them `NULL` selects.
	 */
	mtd_absent_cfg = m->cfg;
	mtd_absent_cfg.ratio = -1;
	mtd_absent_cfg.min_level = 0;

	mtd_nosignal_cfg = m->cfg;
	mtd_nosignal_cfg.min_level = (short)0x7fff;

	ref_FPM_MTD_delete(m);
}

static void
dcd_build(struct fix *f, unsigned seed, const struct dcd_setup *u)
{
	int act, sig;

	fixture(f, seed);

	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->det + V29DET_V21_AGC),
			 &AGCv22_CFG, 1);
	put_ptr(f->det, V29DET_V21_MTD,
		ref_FPM_MTD_create(0, u->mtd_absent ? &mtd_absent_cfg
						    : &mtd_nosignal_cfg));

	put_short(f->det, V29DET_GATE_1C, u->gate_1c);
	put_short(f->det, V29DET_V21_ENABLE, u->enable);
	put_short(f->det, V29DET_V21_SAMPLES, u->samples);
	put_short(f->rx, V29RX_SHORT_0046, u->age);
	put_short(f->rx, V29RX_FSE_MSE, u->mse);
	put_short(f->rx, V29RX_SHORT_4F64, u->rms_gate);
	put_short(f->rx, V29RX_RMS_REF, u->rms_ref);
	put_short(f->rx, V29RX_RMS_N, u->rms_n);

	/*
	 * Three carrier shapes, and the third is not decoration: with
	 * `sre.active` = 2 and `agc.signal` = 3 the carrier bit is TWO, which
	 * is the only way `x & 1` and `x && 1` disagree.  A fixture that only
	 * ever produces 0 and 1 cannot separate the object's bitwise AND from
	 * a logical one.
	 */
	act = (u->carrier == 0) ? 0 : (u->carrier == 1) ? 3 : 2;
	sig = (u->carrier == 0) ? 0 : (u->carrier == 1) ? 1 : 3;
	put_int(f->rx, V29RX_SRE_ACTIVE, act);
	put_int(f->rx, V29RX_AGC_SIGNAL, sig);
}

static void
dcd_free(struct fix *f)
{
	ref_FPM_MTD_delete((struct fpm_mtd *)get_ptr(f->det, V29DET_V21_MTD));
	put_ptr(f->det, V29DET_V21_MTD, 0);
}

static short dcd_in[NBUF];

/*
 * FOUR STIMULI, AND THE LAST TWO EXIST BECAUSE THREE WRONG READINGS COULD NOT
 * BE SEPARATED WITHOUT THEM.
 *
 *   0  quiet noise      the tone detector reports NOSIGNAL and the V.21
 *                       sample count is RESET
 *   1  loud noise       a large RMS, far above any drop gate
 *   2  mid noise        RMS near 1730, which is INSIDE the window between
 *                       0x32fe/32768 and 0x4000/32768 of the 4000 reference
 *                       -- the only place a -8 dB gate and a -6 dB one give
 *                       different answers
 *   3  a 3 kHz tone     out of the detector's band, so it reports ABSENT and
 *                       the V.21 sample count ACCUMULATES, which is the only
 *                       way the 0x4ff threshold is ever crossed
 *
 * Without 3 the two threshold readings are never evaluated at all, and without
 * 2 the drop constant is not; the first run of this test asserted all three and
 * reported three zeros, which is exactly what a separating count is for.
 */
static const short dcd_sine3k[8] = {
	0, 6928, 6928, 0, -6928, -6928, 0, 6928
};

static void
dcd_signal(unsigned seed, int kind)
{
	int i;

	rng_seed(seed);
	for (i = 0; i < NBUF; i++) {
		int v = (int)(rng_next() % 2001u) - 1000;

		switch (kind) {
		case 0:
			dcd_in[i] = (short)(v / 40);
			break;
		case 1:
			dcd_in[i] = (short)(v * 20);
			break;
		case 2:
			dcd_in[i] = (short)(v * 3);
			break;
		default:
			/* 8000 Hz / 3000 Hz is 8 samples per 3 cycles. */
			dcd_in[i] = (short)(dcd_sine3k[i % 8] + v / 8);
			break;
		}
	}
}

static void
run_dcd_one(unsigned seed, const struct dcd_setup *u, int kind,
	    unsigned short count, long where)
{
	struct dcd_obs oa, ob, oc;
	struct dcd_setup v = *u;
	int d;

	dcd_signal(seed ^ 0x0d0d0d0du, kind);

	/*
	 * THE DROP GATE IS PLACED AROUND THIS BLOCK'S OWN RMS when the setup
	 * asks for it.  The object's gate is 0x32fe/32768 of the reference and
	 * the two wrong readings move it to 0x4000/32768 and to twice itself,
	 * so a reference of 2.2 x rms puts the true gate just BELOW the block
	 * and both wrong ones just ABOVE -- the only arrangement in which those
	 * two constants are separable at all.  A fixed reference cannot do it:
	 * the RMS of any stimulus this test can generate is either far above
	 * every candidate gate or far below every one.
	 */
	if (v.auto_ref) {
		int r = ref_FPM_rms(dcd_in, count);

		v.rms_ref = (short)(r * 11 / 5);
	}
	u = &v;

	dcd_build(&fa, seed, u);
	dcd_build(&fb, seed, u);

	dcd_read(&fa, &oa, ref_DataCarrierDetectV29(fa.obj, dcd_in, count));
	dcd_read(&fb, &ob, DataCarrierDetectV29(fb.obj, dcd_in, count));

	diff_eq_int("at %ld: DataCarrierDetectV29 returned", ob.ret, oa.ret,
		    where);
	diff_eq_int("at %ld: the V.21 sample count", ob.v21_samples,
		    oa.v21_samples, where);
	diff_eq_int("at %ld: the V.21 enable", ob.v21_enable, oa.v21_enable,
		    where);
	diff_eq_int("at %ld: the RMS reference", ob.rms_ref, oa.rms_ref, where);
	diff_eq_int("at %ld: the RMS block counter", ob.rms_n, oa.rms_n, where);
	compare_state(&fa, &fb, where);

	if (u->gate_1c == 0)
		dcd_paths[0]++;
	else if (oa.v21_enable == 0)
		dcd_paths[1]++;
	else
		dcd_paths[2]++;
	if (u->rms_gate != 0)
		dcd_paths[3]++;
	if (oa.ret == 0)
		dcd_paths[4]++;
	if (oa.ret == 1)
		dcd_paths[5]++;
	if (oa.rms_ref != u->rms_ref)
		dcd_paths[6]++;
	/*
	 * STRICTLY GREATER, not merely different: a reset to zero is also a
	 * change, and reading it as coverage of the accumulate path is how the
	 * first version of this test believed it had exercised a branch it
	 * never entered.
	 */
	if (oa.v21_samples > u->samples)
		dcd_paths[7]++;

	dcd_free(&fa);
	dcd_free(&fb);

	for (d = 1; d < (int)D_MAX; d++) {
		int r;

		dcd_build(&fc, seed, u);
		r = drive_dcd(&fc, dcd_in, count, (enum dcd_defect)d);
		dcd_read(&fc, &oc, r);
		if (!dcd_same(&oc, &oa))
			dcd_sep[d]++;
		dcd_free(&fc);
	}
}

static int
run_dcd(void)
{
	static const short ages[] = { 10, 999, 1500 };
	static const short mses[] = { 0x100, 0x3fff, 0x7000 };
	int a, e, carrier, kind, rmsg, shape, absent;
	unsigned seed = 0x0abcd000u;
	long where = 0;

	dcd_cfgs();
	diff_begin("DataCarrierDetectV29");

	for (carrier = 0; carrier < 3; carrier++)
	for (kind = 0; kind < 4; kind++)
	for (absent = 0; absent < 2; absent++)
	for (a = 0; a < 3; a++)
	for (e = 0; e < 3; e++)
	for (rmsg = 0; rmsg < 2; rmsg++)
	for (shape = 0; shape < 7; shape++) {
		struct dcd_setup u;

		u.age = ages[a];
		u.mse = mses[e];
		u.rms_gate = (short)rmsg;
		u.rms_ref = 4000;
		u.rms_n = (short)((a + e) % 3);
		u.carrier = carrier;
		u.gate_1c = 0;
		u.enable = 0;
		u.samples = 0;
		u.mtd_absent = absent;
		u.auto_ref = 0;

		switch (shape) {
		case 0:				/* the quiet path         */
			break;
		case 1:				/* armed, latch clear     */
			u.gate_1c = 1;
			break;
		case 2:				/* armed and latched      */
			u.gate_1c = 1;
			u.enable = 1;
			u.samples = 300;
			break;
		case 3:				/* one sample under 0x4ff */
			u.gate_1c = 1;
			u.enable = 1;
			u.samples = (short)(V29DET_V21_THRESHOLD - 160);
			break;
		case 4:				/* exactly 0x500 after    */
			u.gate_1c = 1;
			u.enable = 1;
			u.samples = (short)(V29DET_V21_THRESHOLD + 1 - 160);
			break;
		case 5:				/* a reference that drops */
			u.rms_gate = 1;
			u.rms_ref = (short)0x7fff;
			break;
		default:			/* the gate ON this block */
			u.rms_gate = 1;
			u.auto_ref = 1;
			break;
		}

		run_dcd_one(seed + (unsigned)where, &u, kind, 160, where);
		where++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* The AGC's leaked register, measured rather than believed.             */

static int
run_agc_identity(void)
{
	static struct fpm_agc agc;
	static short samples[NBUF];
	int i, k;

	diff_begin("FPM_AGC_agc leaves agc->signal in %eax");

	for (k = 0; k < 12; k++) {
		int r;

		ref_FPM_AGC_init(&agc, &AGCv22_CFG, 1);
		rng_seed(0x0a9c0000u + (unsigned)k);
		for (i = 0; i < NBUF; i++)
			samples[i] = (short)((int)(rng_next() % 20001u) - 10000)
				     * (short)(k % 3 ? 1 : 0);

		r = ((agc_int_fn)ref_FPM_AGC_agc)(&agc, samples,
						  (unsigned short)(k * 30));
		diff_eq_int("trial %ld: the register is agc->signal", r,
			    agc.signal, k);
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* DemodDataV29                                                          */

/*
 * THE ONLY SYMBOL HERE THAT NEEDS A LIVE DSP CHAIN, and the fixture is most of
 * the work.  `DemodDataV29` calls, in order, `FPM_AGC_agc`, `FPM_TONE_kill`,
 * `FPM_MTD_detect`, `FPM_MRF_filter`, `FPM_SRE_recover` and `FPM_FSE_receive`,
 * so a trial needs six constructed objects at their real offsets inside the
 * receiver's block and three scratch buffers big enough for what they produce.
 *
 * D955's rule is the reason none of them may be faked: this is table-lookup
 * code, and a field left unplanted that is used as a SUBSCRIPT cannot be caught
 * by a blob-against-blob dry run -- both sides read the same wild index and
 * agree, and only a segfault is left to chance.  Every one of the six is
 * constructed by the BLOB's own `_init`, on both sides, so the states are
 * identical bytes and any difference the test sees belongs to this file.
 *
 * F8790's rule is why each trial is EIGHT consecutive blocks with the state
 * carried across: the MRF, the SRE and the FSE all hold history, and a
 * one-block fixture cannot see a swapped weight or a buffer used for the wrong
 * stage.
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
 * The pointer fields the two sides cannot agree on, because each side's `_init`
 * allocated its own.  Everything else in the block IS compared, including the
 * FSE's two scatter logs.
 */
static int
skip_rx_demod(int off)
{
	if (skip_rx(off))
		return 1;
	if (off >= V29RX_MRF + 0x18 && off < V29RX_MRF + 0x1c)
		return 1;			/* fpm_mrf::history          */
	if (off >= V29RX_SRE + 0x50 && off < V29RX_SRE + 0x5c)
		return 1;			/* coeff, hist, clk          */
	if (off >= V29RX_SRE + 0x74 && off < V29RX_SRE + 0x78)
		return 1;			/* rms_buf                   */
	if (off >= V29RX_FSE + 0x54 && off < V29RX_FSE + 0x5c)
		return 1;			/* out_i, out_q              */
	if (off >= V29RX_FSE + 0x60 && off < V29RX_FSE + 0x6c)
		return 1;			/* icoeff, qcoeff, hist      */
	return 0;
}

struct dem_setup {
	short	gate_14;	/* non-zero skips the tone pre-pass        */
	int	mtd_absent;	/* the pre-pass detector's forced verdict  */
	int	enables;	/* which shape of the three enable words   */
};

/*
 * THE THREE ENABLE WORDS MUST DIFFER IN BIT 0 AND NOWHERE ELSE MATTERS.
 *
 * `agc.signal` is a `setg` result, so it is 0 or 1 and never anything else --
 * which means `signal & word` can only ever be 0 or `word & 1`.  The first
 * version of this fixture seeded the three words 0x0f, 0x33 and 0x55, all of
 * which have bit 0 set, so all three products were the SAME value and
 * transposing two of them changed nothing at all.  Two named wrong readings
 * reported a separating count of zero and that is what said so.
 *
 * Two shapes, each with one word bit-0-clear and the others set, so every
 * pairing of source and destination is distinguishable in at least one.
 */
static const int dem_enable[2][3] = {
	{ 0x0e, 0x33, 0x54 },		/* adapt off, pll on,  lms off */
	{ 0x33, 0x54, 0x0f }		/* adapt on,  pll off, lms on  */
};

static void
dem_build(struct fix *f, unsigned seed, const struct dem_setup *u,
	  short *mrfbuf, short *srebuf, short *detbuf)
{
	fixture(f, seed);

	put_ptr(f->rx, V29RX_BUF_MRF, mrfbuf);
	put_ptr(f->rx, V29RX_BUF_SRE, srebuf);
	put_ptr(f->det, V29DET_BUF, detbuf);
	memset(mrfbuf, 0, DEM_BUF * sizeof(short));
	memset(srebuf, 0, DEM_BUF * sizeof(short));
	memset(detbuf, 0, DEM_BUF * sizeof(short));

	ref_FPM_AGC_init(RX_AGC_OF(f), &AGCv22_CFG, 1);
	ref_FPM_MRF_init(RX_MRF_OF(f), &MRFv32_CFG, 1);
	ref_FPM_SRE_init(RX_SRE_OF(f), &SREv32_CFG, 1);
	ref_FPM_FSE_init(RX_FSE_OF(f), &dem_fse_cfg, 1);

	put_ptr(f->det, V29DET_MTD,
		ref_FPM_MTD_create(0, u->mtd_absent ? &mtd_absent_cfg
						    : &mtd_nosignal_cfg));
	put_ptr(f->det, V29DET_TONE, ref_FPM_TONE_create(0, 0));

	put_short(f->det, V29DET_GATE_14, u->gate_14);
	put_int(f->rx, V29RX_INT_0004, dem_enable[u->enables][0]);
	put_int(f->rx, V29RX_INT_0008, dem_enable[u->enables][1]);
	put_int(f->rx, V29RX_INT_0020, dem_enable[u->enables][2]);
}

static void
dem_free(struct fix *f)
{
	ref_FPM_MRF_free(RX_MRF_OF(f));
	ref_FPM_SRE_free(RX_SRE_OF(f));
	ref_FPM_FSE_free(RX_FSE_OF(f));
	ref_FPM_MTD_delete((struct fpm_mtd *)get_ptr(f->det, V29DET_MTD));
	ref_FPM_TONE_delete(get_ptr(f->det, V29DET_TONE));
}

/* The named wrong readings, one changed thing each. */
enum dem_defect {
	M_NONE = 0,
	M_NO_HALVE,		/* the pre-pass copies without the >> 1     */
	M_HALVE_UNSIGNED,	/* the >> 1 taken on an unsigned value      */
	M_KILL_INPUT,		/* the notch applied to `in`, not the copy  */
	M_NO_ABANDON,		/* a tone detection does not abandon        */
	M_SRE_FROM_INPUT,	/* the recoverer fed `in` instead of the
				 * resampler's output                       */
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
	unsigned char *rx = f->rx;
	unsigned char *det = f->det;
	int signal;
	unsigned short n;

	ref_FPM_AGC_agc(RX_AGC_OF(f), in, count);
	signal = RX_AGC_OF(f)->signal;
	if (d == M_SIGNAL_ONE)
		signal = 1;

	if (get_short(det, V29DET_GATE_14) == 0) {
		short *buf = (short *)get_ptr(det, V29DET_BUF);
		unsigned short i;

		for (i = 0; i < count; i++) {
			if (d == M_NO_HALVE)
				buf[i] = in[i];
			else if (d == M_HALVE_UNSIGNED)
				buf[i] = (short)((unsigned short)in[i] >> 1);
			else
				buf[i] = (short)(in[i] >> 1);
		}

		ref_FPM_TONE_kill(get_ptr(det, V29DET_TONE),
				  (d == M_KILL_INPUT) ? in : buf,
				  (short)count);

		if (ref_FPM_MTD_detect((struct fpm_mtd *)
					get_ptr(det, V29DET_MTD),
				       buf, (short)count) != 0
		    && d != M_NO_ABANDON)
			return 0;
	}

	n = (unsigned short)ref_FPM_MRF_filter(RX_MRF_OF(f), in,
					       (short *)get_ptr(rx,
							V29RX_BUF_MRF),
					       (short)count);

	*(int *)(void *)(rx + V29RX_SRE_ADAPT) = signal
		& get_int(rx, (d == M_ADAPT_SOURCE) ? V29RX_INT_0008
						    : V29RX_INT_0004);

	{
		const short *src = (d == M_SRE_FROM_INPUT)
				   ? (const short *)in
				   : (const short *)get_ptr(rx, V29RX_BUF_MRF);

		n = ref_FPM_SRE_recover(RX_SRE_OF(f), src,
					(short *)get_ptr(rx, V29RX_BUF_SRE),
					(short)n);
	}

	if (d != M_TILT_NOT_CLEARED)
		*(int *)(void *)(rx + V29RX_FSE_TILT_ON) = 0;
	*(int *)(void *)(rx + V29RX_FSE_PLL_ON) = signal
		& get_int(rx, (d == M_LMS_SOURCE) ? V29RX_INT_0020
						  : V29RX_INT_0008);
	*(int *)(void *)(rx + V29RX_FSE_LMS_ON) = signal
		& get_int(rx, (d == M_LMS_SOURCE) ? V29RX_INT_0008
						  : V29RX_INT_0020);

	return ref_FPM_FSE_receive(RX_FSE_OF(f),
				   (const short *)get_ptr(rx,
					(d == M_FSE_FROM_MRF) ? V29RX_BUF_MRF
							      : V29RX_BUF_SRE),
				   out,
				   (d == M_FSE_COUNT)
					? (unsigned short)count : n);
}

static long dem_sep[16], dem_paths[6];

/*
 * THREE LEVELS, AND THE SILENT ONE IS NOT DECORATION.  `agc.signal` is the
 * gain control's own "more than half the blocks were above the gate", and on
 * both noise levels this fixture first used it came back 1 every time -- so
 * `signal & word` and `1 & word` were the same number and the wrong reading
 * that forces the carrier bit to 1 separated nothing.  A silent block is the
 * only stimulus that makes the bit itself observable.
 */
static void
dem_signal(unsigned seed, int level)
{
	int i;

	rng_seed(seed);
	for (i = 0; i < DEM_BUF; i++) {
		int v = (int)(rng_next() % 4001u) - 2000;

		dem_in[i] = (short)(level == 0 ? 0
				    : level == 1 ? v / 8
						 : v * 8);
	}
}

/*
 * A trial: DEM_BLOCKS consecutive blocks through both sides, compared after
 * every one.  The defect drives replay the WHOLE sequence from a fresh fixture,
 * so a reading that only diverges after the state has built up is still caught.
 */
static void
run_demod_one(unsigned seed, const struct dem_setup *u, int level,
	      unsigned short count, long where)
{
	unsigned long marka = 0, markc;
	int blk, d, i;

	dem_signal(seed ^ 0x0b10cced, level);

	dem_build(&fa, seed, u, dem_mrf_a, dem_sre_a, dem_det_a);
	dem_build(&fb, seed, u, dem_mrf_b, dem_sre_b, dem_det_b);

	for (blk = 0; blk < DEM_BLOCKS; blk++) {
		unsigned short ra, rb;
		long id = where * 100 + blk;

		for (i = 0; i < DEM_BUF; i++) {
			dem_out_a[i] = dem_out_b[i] = 0xbeef;
			dem_work[i] = dem_in[i];
		}
		ra = ref_DemodDataV29(fa.obj, dem_work, dem_out_a, count);
		for (i = 0; i < DEM_BUF; i++)
			dem_work[i] = dem_in[i];
		rb = DemodDataV29(fb.obj, dem_work, dem_out_b, count);

		diff_eq_int("at %ld: DemodDataV29 returned", (long)rb, (long)ra,
			    id);
		diff_eq_int("at %ld: the return fits the buffer", ra < DEM_BUF,
			    1, id);
		if (ra >= DEM_BUF)
			return;
		for (i = 0; i < (int)ra; i++)
			diff_eq_int("word %ld", (long)dem_out_b[i],
				    (long)dem_out_a[i], i);
		diff_eq_int("at %ld: nothing past the returned count",
			    dem_out_a[ra] == 0xbeef, 1, id);
		diff_eq_int("at %ld: first differing receiver byte",
			    blk_first_diff(fb.rx, fa.rx, RX_SIZE,
					   skip_rx_demod), -1, id);
		diff_eq_int("at %ld: first differing detector byte",
			    blk_first_diff(fb.det, fa.det, DET_SIZE, skip_det),
			    -1, id);
		diff_eq_int("at %ld: the resampler's buffer",
			    blk_first_diff((unsigned char *)dem_mrf_b,
					   (unsigned char *)dem_mrf_a,
					   DEM_BUF * 2, 0), -1, id);
		diff_eq_int("at %ld: the recoverer's buffer",
			    blk_first_diff((unsigned char *)dem_sre_b,
					   (unsigned char *)dem_sre_a,
					   DEM_BUF * 2, 0), -1, id);
		diff_eq_int("at %ld: the pre-pass buffer",
			    blk_first_diff((unsigned char *)dem_det_b,
					   (unsigned char *)dem_det_a,
					   DEM_BUF * 2, 0), -1, id);

		/*
		 * THE MARK CARRIES THE FLAG WORDS AS WELL AS THE OUTPUT, and
		 * that is not tidiness: `sre.adapt`, `fse.pll_on`,
		 * `fse.lms_on` and `fse.tilt_on` do not reach the samples at
		 * all on a single block, so four named wrong readings about
		 * them separated nothing until they were folded in here.
		 */
		marka = marka * 1000003u + ra;
		for (i = 0; i < (int)ra; i++)
			marka = marka * 31u + dem_out_a[i];
		marka = marka * 131u + (unsigned long)
				get_int(fa.rx, V29RX_SRE_ADAPT);
		marka = marka * 131u + (unsigned long)
				get_int(fa.rx, V29RX_FSE_PLL_ON);
		marka = marka * 131u + (unsigned long)
				get_int(fa.rx, V29RX_FSE_LMS_ON);
		marka = marka * 131u + (unsigned long)
				get_int(fa.rx, V29RX_FSE_TILT_ON);

		if (ra == 0)
			dem_paths[0]++;
		else
			dem_paths[1]++;
		if (u->gate_14 == 0)
			dem_paths[2]++;
		else
			dem_paths[3]++;
		if (get_int(fa.rx, V29RX_FSE_LMS_ON) != 0)
			dem_paths[4]++;
		if (get_int(fa.rx, V29RX_SRE_ADAPT) != 0)
			dem_paths[5]++;
	}

	dem_free(&fa);
	dem_free(&fb);

	for (d = 1; d < (int)M_MAX; d++) {
		dem_build(&fc, seed, u, dem_mrf_c, dem_sre_c, dem_det_c);
		markc = 0;
		for (blk = 0; blk < DEM_BLOCKS; blk++) {
			unsigned short rc;

			for (i = 0; i < DEM_BUF; i++) {
				dem_out_b[i] = 0xbeef;
				dem_work[i] = dem_in[i];
			}
			rc = drive_demod(&fc, dem_work, dem_out_b, count,
					 (enum dem_defect)d);
			markc = markc * 1000003u + rc;
			if (rc < DEM_BUF)
				for (i = 0; i < (int)rc; i++)
					markc = markc * 31u + dem_out_b[i];
			markc = markc * 131u + (unsigned long)
					get_int(fc.rx, V29RX_SRE_ADAPT);
			markc = markc * 131u + (unsigned long)
					get_int(fc.rx, V29RX_FSE_PLL_ON);
			markc = markc * 131u + (unsigned long)
					get_int(fc.rx, V29RX_FSE_LMS_ON);
			markc = markc * 131u + (unsigned long)
					get_int(fc.rx, V29RX_FSE_TILT_ON);
		}
		if (markc != marka)
			dem_sep[d]++;
		dem_free(&fc);
	}
}

static int
run_demod(void)
{
	static const unsigned short counts[] = { 32, 160, 700 };
	int gate, absent, enables, level, c;
	long where = 0;

	dem_tables();
	diff_begin("DemodDataV29");

	for (gate = 0; gate < 2; gate++)
	for (absent = 0; absent < 2; absent++)
	for (enables = 0; enables < 2; enables++)
	for (level = 0; level < 3; level++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		struct dem_setup u;

		u.gate_14 = (short)gate;
		u.mtd_absent = absent;
		u.enables = enables;
		run_demod_one(0x0de70000u + (unsigned)where, &u, level,
			      counts[c], where);
		where++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;
	int d;

	harness_alloc_reset();

	rc |= run_agc_identity();
	rc |= run_accessors();
	rc |= run_tx_accessors();
	rc |= run_status();
	rc |= run_modem();
	rc |= run_delete();
	rc |= run_quality();
	rc |= run_dcd();
	rc |= run_demod();

	/*
	 * The separating counts.  Each is the number of trials on which a
	 * NAMED wrong reading produced a different OBSERVABLE answer from the
	 * blob's.  A zero here means the corresponding check above is
	 * decoration, so each one is a failure in its own right.
	 */
	diff_begin("v29fax separating trials");
	diff_eq_int("the carrier gate's offset separates (%ld)",
		    acc_gate_sep > 0, 1, acc_gate_sep);
	diff_eq_int("the AGC signal's offset separates (%ld)",
		    acc_signal_sep > 0, 1, acc_signal_sep);
	diff_eq_int("`&` rather than `&&` separates (%ld)", acc_bitand_sep > 0,
		    1, acc_bitand_sep);
	diff_eq_int("the epoch flag's offset separates (%ld)", ep_off_sep > 0,
		    1, ep_off_sep);
	diff_eq_int("reporting 0/1 rather than the value separates (%ld)",
		    ep_bool_sep > 0, 1, ep_bool_sep);
	diff_eq_int("the epoch test being 32-bit separates (%ld)",
		    ep_wide_sep > 0, 1, ep_wide_sep);
	diff_eq_int("14 rather than V.17's 13 separates (%ld)",
		    snr_const_sep > 0, 1, snr_const_sep);
	diff_eq_int("the decoder error's offset separates (%ld)",
		    snr_off_sep > 0, 1, snr_off_sep);
	diff_eq_int("the scrambler seed's offset separates (%ld)",
		    seed_off_sep > 0, 1, seed_off_sep);
	diff_eq_int("the seed being 32 bits wide separates (%ld)",
		    seed_width_sep > 0, 1, seed_width_sep);
	diff_eq_int("SetEncoderV29's target offset separates (%ld)",
		    enc_off_sep > 0, 1, enc_off_sep);
	diff_eq_int("the argument being a SHORT separates (%ld)",
		    enc_short_sep > 0, 1, enc_short_sep);
	diff_eq_int("values other than 0 and 1 writing nothing (%ld)",
		    enc_ignored_sep > 0, 1, enc_ignored_sep);
	diff_eq_int("the status source field separates (%ld)", st_src_sep > 0,
		    1, st_src_sep);
	diff_eq_int("the status flag mask separates (%ld)", st_mask_sep > 0, 1,
		    st_mask_sep);
	diff_eq_int("+0x0e being left alone separates (%ld)", st_gap_sep > 0, 1,
		    st_gap_sep);
	diff_eq_int("the flags byte being ASSIGNED separates (%ld)",
		    st_assign_sep > 0, 1, st_assign_sep);
	diff_eq_int("the null destination was exercised (%ld)",
		    st_null_trials > 0, 1, st_null_trials);
	diff_eq_int("the do-while was exercised (%ld)", mdm_dowhile_sep > 0, 1,
		    mdm_dowhile_sep);
	diff_eq_int("in and out advancing differently separates (%ld)",
		    mdm_swap_sep > 0, 1, mdm_swap_sep);
	diff_eq_int("the total passed 32767 at least once (%ld)",
		    mdm_wrapped > 0, 1, mdm_wrapped);
	diff_eq_int("clearing exactly bit 0x200 separates (%ld)",
		    mdm_status_sep > 0, 1, mdm_status_sep);
	diff_eq_int("V29RX_modem returned a status (%ld)", mdm_ret_nonzero > 0,
		    1, mdm_ret_nonzero);
	diff_eq_int("the delete released tracked memory (%ld)",
		    del_vector_trials > 0, 1, del_vector_trials);
	diff_eq_int("the delete freed the three blocks (%ld)",
		    del_badfree_trials > 0, 1, del_badfree_trials);

	for (d = 1; d < (int)Q_MAX; d++)
		diff_eq_int("quality wrong reading %ld separates", q_sep[d] > 0,
			    1, d);
	for (d = 0; d < 6; d++)
		diff_eq_int("quality path %ld was reached", q_paths[d] > 0, 1,
			    d);
	for (d = 1; d < (int)D_MAX; d++)
		diff_eq_int("carrier-detect wrong reading %ld separates",
			    dcd_sep[d] > 0, 1, d);
	for (d = 0; d < 8; d++)
		diff_eq_int("carrier-detect path %ld was reached",
			    dcd_paths[d] > 0, 1, d);
	for (d = 1; d < (int)M_MAX; d++)
		diff_eq_int("demodulator wrong reading %ld separates",
			    dem_sep[d] > 0, 1, d);
	for (d = 0; d < 6; d++)
		diff_eq_int("demodulator path %ld was reached",
			    dem_paths[d] > 0, 1, d);

	rc |= diff_end();

	return rc;
}
