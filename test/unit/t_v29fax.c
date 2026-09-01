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

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/v29fax.h"
#include "dsplib/v29cfg.h"
#include "dsplib/v29data.h"
#include "dsplib/faxcfg.h"
#include "dsplib/debug.h"

#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sgd.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_sdm.h"
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
extern int   ref_V29RX_status(void *modem, void *status);
extern void  ref_ScrambleDataV29(void *modem, unsigned short *data,
				 unsigned short count);
extern void  ref_DescrambleDataV29(void *modem, unsigned short *data,
				   unsigned short count);
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
extern void  ref_FPM_SDM_init(struct fpm_sdm *sdm,
			      const struct fpm_sdm_cfg *cfg);
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
/* V29RX_status                                                          */

/*
 * The receiver's half of the report.  It is not a mirror of the transmit one
 * and the checks below are built around the four ways it differs: which
 * handle it reads, which slots take the bit rate, that it writes +0x0e and
 * not +0x0c, and that its flags byte is rewritten four times.
 *
 * THE STATUS BLOCK IS OVERLAID ON THE INSTANCE IN HALF THE TRIALS, which is
 * the only arrangement that can observe the intermediate stores at all: the
 * object flushes +0x14 before each load through `modem`, and a disjoint
 * report cannot tell that from one store at the end.  The overlay is placed
 * so that the report's +0x14 lands on the instance's +0x14 -- a byte
 * `V29RX_status` neither reads nor writes through `modem`, so both sides see
 * the same sequence and any difference is the code's.
 */
static long rxst_bps_sep, rxst_q0, rxst_q1, rxst_b1_0, rxst_b1_1;
static long rxst_b3_0, rxst_b3_1, rxst_b5_0, rxst_b5_1;
static long rxst_gap_sep, rxst_alias_sep, rxst_null_trials;

static int
run_rxstatus(void)
{
	int i;

	diff_begin("V29RX_status");

	for (i = 0; i < 40; i++) {
		int ra, rb, k;
		int overlay = (i & 1);
		unsigned char *da, *db;
		long where = 1000 + i;

		fixture(&fa, 0x5a000000u + (unsigned)i);
		fixture(&fb, 0x5a000000u + (unsigned)i);

		/*
		 * The three fields the flags byte is assigned from, driven
		 * BOTH ways rather than left to the pseudorandom fill: two of
		 * the three are `!= 0` tests over a whole 32-bit word, which
		 * a random fill answers the same way every time.
		 */
		fa.rx[V29RX_FLAGS_0018] = fb.rx[V29RX_FLAGS_0018] =
			(unsigned char)((i & 2) ? 0xff : 0xfe);
		put_int(fa.rx, V29RX_INT_0000, (i & 4) ? 0 : 0x00c0ffee);
		put_int(fb.rx, V29RX_INT_0000, (i & 4) ? 0 : 0x00c0ffee);
		put_int(fa.rx, V29RX_INT_0020, (i & 8) ? 0 : 0x0badf00d);
		put_int(fb.rx, V29RX_INT_0020, (i & 8) ? 0 : 0x0badf00d);

		/* Bit 15 of the status word, both ways. */
		put_int(fa.obj, V29_OBJ_STATUS,
			(i & 16) ? 0x00008000 : 0x00004000);
		put_int(fb.obj, V29_OBJ_STATUS,
			(i & 16) ? 0x00008000 : 0x00004000);

		/* The protocol and the bit rate, made to differ. */
		put_short(fa.obj, V29_OBJ_PROTOCOL, (short)(0x0100 + i));
		put_short(fb.obj, V29_OBJ_PROTOCOL, (short)(0x0100 + i));
		put_short(fa.obj, V29_OBJ_BITRATE, (short)(9600 - i));
		put_short(fb.obj, V29_OBJ_BITRATE, (short)(9600 - i));

		if (overlay) {
			/*
			 * The report IS the instance's own bytes.  OBJ_SIZE is
			 * 0x60 and STAT_SIZE is 0x20, so +0x00..+0x1f of the
			 * report sits over +0x00..+0x1f of the instance --
			 * which includes the protocol, the bit rate and the
			 * status word this function reads.  That is the point:
			 * a wrong store order changes what the LATER loads
			 * see.
			 */
			da = fa.obj;
			db = fb.obj;
		} else {
			rng_seed(0x2718281u + (unsigned)i);
			for (k = 0; k < STAT_SIZE; k++)
				sta[k] = stb[k] = (unsigned char)rng_next();
			if ((i & 32) == 0)
				sta[V29STAT_FLAGS] = stb[V29STAT_FLAGS] = 0xff;
			da = sta;
			db = stb;
		}

		ra = ref_V29RX_status(fa.obj, da);
		rb = V29RX_status(fb.obj, db);

		diff_eq_int("at %ld: V29RX_status returned", rb, ra, where);
		if (!overlay)
			diff_eq_int("at %ld: first differing report byte",
				    blk_first_diff(stb, sta, STAT_SIZE, 0), -1,
				    where);
		compare_state(&fa, &fb, where);

		/* ARM COVERAGE, taken from what the REFERENCE left behind. */
		if (get_short(da, 0x06) != 0)
			rxst_q1++;
		else
			rxst_q0++;
		if ((da[V29STAT_FLAGS] & V29STAT_BIT1) != 0)
			rxst_b1_1++;
		else
			rxst_b1_0++;
		if ((da[V29STAT_FLAGS] & V29STAT_BIT3) != 0)
			rxst_b3_1++;
		else
			rxst_b3_0++;
		if ((da[V29STAT_FLAGS] & V29STAT_BIT5) != 0)
			rxst_b5_1++;
		else
			rxst_b5_0++;
		if (overlay)
			rxst_alias_sep++;

		/*
		 * WRONG READING: the bit rate taken from the instance's +0x00
		 * rather than its +0x04.  Separated whenever the two differ,
		 * which the seeding above guarantees.
		 */
		if (!overlay
		    && get_short(da, V29STAT_ZERO_LO)
		       != get_short(fa.obj, V29_OBJ_PROTOCOL))
			rxst_bps_sep++;
		/*
		 * WRONG READING: +0x0c zeroed instead of +0x0e.  The object
		 * leaves +0x0c exactly as the caller had it.
		 */
		if (!overlay && get_short(da, 0x0c) != 0)
			rxst_gap_sep++;
	}

	/* The null report. */
	{
		int ra, rb;

		fixture(&fa, 0x13572468u);
		fixture(&fb, 0x13572468u);
		ra = ref_V29RX_status(fa.obj, 0);
		rb = V29RX_status(fb.obj, 0);
		diff_eq_int("a null report returns %ld", rb, ra, (long)ra);
		diff_eq_int("a null report returns zero", ra, 0, 0);
		compare_state(&fa, &fb, 0);
		rxst_null_trials++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* ScrambleDataV29 and DescrambleDataV29                                 */

/*
 * Two wrappers over `SDM_scrambler` / `SDM_descrambler`.  What they establish
 * is WHICH sub-object each picks, so the test's whole job is to prove the
 * offset: the two `fpm_sdm` states are seeded DIFFERENTLY, and a wrapper that
 * reached the other one -- or the other block -- would produce a different
 * bit stream and leave a different state behind.
 */
static long scr_bits_sep, scr_state_sep, scr_wrong_off_sep, scr_zero_trials;

#define SCR_N	48

static unsigned short scr_a[SCR_N], scr_b[SCR_N], scr_in[SCR_N];

/*
 * Bring both `fpm_sdm` states up with the BLOB's own init, with DIFFERENT
 * configurations and different registers.
 *
 * The pseudorandom fill leaves both states with a nonsense configuration -- a
 * negative `nbits`, or a tap below it -- under which the module's shifts have
 * no defined meaning and the two sides could agree on nothing useful.  The
 * configurations DIFFER because that is what makes reaching the wrong state
 * observable at all: with the same config and the same register a crossed
 * offset would produce the same bit stream and the check would be decoration
 * (finding F134).
 */
static void
scr_seed_sdm(struct fix *f)
{
	struct fpm_sdm_cfg cfg;
	struct fpm_sdm *tx = (struct fpm_sdm *)(void *)(f->tx + V29TX_SDM);
	struct fpm_sdm *rx = (struct fpm_sdm *)(void *)(f->rx + V29RX_SDM);

	cfg.nbits = 4;
	cfg.tap1 = 14;
	cfg.tap2 = 17;
	ref_FPM_SDM_init(tx, &cfg);
	tx->reg = 0x0002468au;

	cfg.nbits = 2;
	cfg.tap1 = 9;
	cfg.tap2 = 11;
	ref_FPM_SDM_init(rx, &cfg);
	rx->reg = 0x0001357bu;
}

static unsigned
scr_reg(unsigned char *blk, int off)
{
	return ((struct fpm_sdm *)(void *)(blk + off))->reg;
}

static int
run_scramblers(void)
{
	int i;

	diff_begin("ScrambleDataV29 / DescrambleDataV29");

	for (i = 0; i < 24; i++) {
		int k, tx = (i & 1);
		unsigned short n = (unsigned short)((i * 7) % SCR_N);
		unsigned tx_reg, rx_reg;
		long where = 2000 + i;

		fixture(&fa, 0x6c000000u + (unsigned)i);
		fixture(&fb, 0x6c000000u + (unsigned)i);
		scr_seed_sdm(&fa);
		scr_seed_sdm(&fb);
		tx_reg = scr_reg(fa.tx, V29TX_SDM);
		rx_reg = scr_reg(fa.rx, V29RX_SDM);

		rng_seed(0x0defacedu + (unsigned)i);
		for (k = 0; k < SCR_N; k++) {
			scr_in[k] = (unsigned short)(rng_next() & 0x0f);
			scr_a[k] = scr_b[k] = scr_in[k];
		}

		if (tx) {
			ref_ScrambleDataV29(fa.obj, scr_a, n);
			ScrambleDataV29(fb.obj, scr_b, n);
		} else {
			ref_DescrambleDataV29(fa.obj, scr_a, n);
			DescrambleDataV29(fb.obj, scr_b, n);
		}

		for (k = 0; k < SCR_N; k++)
			diff_eq_int("bits[%ld]", (long)scr_b[k], (long)scr_a[k],
				    k);
		compare_state(&fa, &fb, where);

		/*
		 * ARM COVERAGE and the named wrong reading, read out of what
		 * the REFERENCE did.
		 */
		if (n > 0) {
			int moved = 0;
			int own_moved, other_same;

			for (k = 0; k < n; k++)
				if (scr_a[k] != scr_in[k])
					moved = 1;
			if (moved)
				scr_bits_sep++;

			own_moved = tx
				? (scr_reg(fa.tx, V29TX_SDM) != tx_reg)
				: (scr_reg(fa.rx, V29RX_SDM) != rx_reg);
			other_same = tx
				? (scr_reg(fa.rx, V29RX_SDM) == rx_reg)
				: (scr_reg(fa.tx, V29TX_SDM) == tx_reg);
			if (own_moved)
				scr_state_sep++;
			/*
			 * WRONG READING: the OTHER block's scrambler.  The two
			 * states carry different configurations and different
			 * registers, so a crossed offset moves the wrong one
			 * and produces a different stream.
			 */
			if (own_moved && other_same)
				scr_wrong_off_sep++;
		}
	}

	/* A zero count, on both entry points. */
	{
		int k;

		fixture(&fa, 0x0a0b0c0du);
		fixture(&fb, 0x0a0b0c0du);
		scr_seed_sdm(&fa);
		scr_seed_sdm(&fb);
		for (k = 0; k < SCR_N; k++)
			scr_a[k] = scr_b[k] = (unsigned short)k;
		ref_ScrambleDataV29(fa.obj, scr_a, 0);
		ScrambleDataV29(fb.obj, scr_b, 0);
		ref_DescrambleDataV29(fa.obj, scr_a, 0);
		DescrambleDataV29(fb.obj, scr_b, 0);
		for (k = 0; k < SCR_N; k++)
			diff_eq_int("zero-count bits[%ld]", (long)scr_b[k],
				    (long)scr_a[k], k);
		compare_state(&fa, &fb, 2999);
		scr_zero_trials++;
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
	put_ptr(fa.det, V29DET_HANDLER, (void *)slot_fn);
	put_ptr(fb.det, V29DET_HANDLER, (void *)slot_fn);

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
		    get_int(fa.obj, V29_OBJ_STATUS) & V29_STATUS_ERROR, 0,
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
		put_ptr(fa.det, V29DET_HANDLER, (void *)slot_fn);
		put_ptr(fb.det, V29DET_HANDLER, (void *)slot_fn);
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
		diff_eq_int("exactly bit 0x200 was cleared", ra, ~V29_STATUS_ERROR,
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

#if 0	/* WITHDRAWN, F9001: segfaults under GCC 3.4.2; see the note in main() */
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
#endif


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

	put_short(f->det, V29DET_STATE, u->gate_14);
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

	if (get_short(det, V29DET_STATE) == 0) {
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
/* V29TX_delete                                                          */

extern void ref_V29TX_delete(void *modem);

/*
 * TWELVE SEPARATE ALLOCATIONS, one per thing the twelve `sysdep_free` calls
 * the object reaches actually release -- nine of them directly and three
 * through `FPM_PPS_free`, `SGD_delete` and `FIFO_delete`.  `run_delete`
 * above says what a liveness vector can and cannot see; the same bound
 * applies here, and the ORDER is stated in `v29fax.h` from the disassembly
 * and is not tested.
 *
 * The handle, the parameter block and the private block are filled
 * PSEUDORANDOMLY before the pointers go in, so an offset wrong by four bytes
 * frees a pointer the allocator never handed out (counted in `bad_free`) and
 * leaves the right block live -- both halves of the vector move.
 */
#define TXDEL_BLOCKS	12
#define TXDEL_OBJ	0x40
#define TXDEL_PRM	0x20

struct txdelfix {
	void	*p[TXDEL_BLOCKS];
	int	live[TXDEL_BLOCKS];
	int	allocs, frees, bad_free, live_total;
};

static void *
txdel_build(struct txdelfix *d, unsigned seed)
{
	unsigned char *obj, *prm, *tx;
	struct fax_fifo *fifo;
	struct sgd *sg;
	struct fpm_pps *pps;
	int i;

	for (i = 0; i < TXDEL_BLOCKS; i++)
		d->p[i] = 0;

	obj = (unsigned char *)sysdep_malloc(TXDEL_OBJ);
	prm = (unsigned char *)sysdep_malloc(TXDEL_PRM);
	tx = (unsigned char *)sysdep_malloc(TX_SIZE);
	fifo = (struct fax_fifo *)sysdep_malloc(sizeof(*fifo));
	sg = (struct sgd *)sysdep_malloc(sizeof(*sg));

	rng_seed(seed);
	for (i = 0; i < TXDEL_OBJ; i++)
		obj[i] = (unsigned char)rng_next();
	for (i = 0; i < TXDEL_PRM; i++)
		prm[i] = (unsigned char)rng_next();
	for (i = 0; i < TX_SIZE; i++)
		tx[i] = (unsigned char)rng_next();
	memset(fifo, 0, sizeof(*fifo));
	memset(sg, 0, sizeof(*sg));

	d->p[0] = obj;
	d->p[1] = prm;
	d->p[2] = tx;
	d->p[3] = sysdep_malloc(32 * sizeof(short));	/* pps.hist_i     */
	d->p[4] = sysdep_malloc(32 * sizeof(short));	/* pps.hist_q     */
	d->p[5] = sysdep_malloc(32 * sizeof(short));	/* ring.i  +0x08  */
	d->p[6] = sysdep_malloc(32 * sizeof(short));	/* ring.q  +0x0c  */
	d->p[7] = sysdep_malloc(32 * sizeof(short));	/* ring.sym +0x10 */
	d->p[8] = fifo;					/* params + 0x00  */
	d->p[9] = sysdep_malloc(32 * sizeof(unsigned short));	/* fifo->buf */
	d->p[10] = sg;					/* params + 0x04  */
	d->p[11] = sysdep_malloc(32);			/* sgd->hist      */

	put_ptr(obj, V29_OBJ_TX, tx);
	put_ptr(obj, V29TX_OBJ_PARAMS, prm);
	put_ptr(prm, V29TXP_FIFO, fifo);
	put_ptr(prm, V29TXP_SGD, sg);

	pps = (struct fpm_pps *)(void *)(tx + V29FP_PPS);
	memset(pps, 0, sizeof(*pps));
	pps->hist_i = (short *)d->p[3];
	pps->hist_q = (short *)d->p[4];

	put_ptr(tx, V29FP_SMC_RING + 0x00, d->p[5]);
	put_ptr(tx, V29FP_SMC_RING + 0x04, d->p[6]);
	put_ptr(tx, V29FP_SMC_RING + 0x08, d->p[7]);

	fifo->buf = (unsigned short *)d->p[9];
	sg->hist = (unsigned short *)d->p[11];

	return (void *)obj;
}

static void
txdel_record(struct txdelfix *d)
{
	int i;

	for (i = 0; i < TXDEL_BLOCKS; i++)
		d->live[i] = harness_alloc_ordinal(d->p[i]) != 0;
	d->allocs = harness_alloc.allocs;
	d->frees = harness_alloc.frees;
	d->bad_free = harness_alloc.bad_free;
	d->live_total = harness_alloc.live;
}

static long txdel_trials, txdel_freed_all;

static int
run_txdelete(void)
{
	static const unsigned seeds[] = { 0x2de17a00u, 0x2de17a01u };
	int s, i;

	diff_begin("V29TX_delete");

	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++) {
		struct txdelfix da_, db_;
		void *m;

		harness_alloc_reset();
		m = txdel_build(&da_, seeds[s]);
		ref_V29TX_delete(m);
		txdel_record(&da_);

		harness_alloc_reset();
		m = txdel_build(&db_, seeds[s]);
		V29TX_delete(m);
		txdel_record(&db_);

		for (i = 0; i < TXDEL_BLOCKS; i++)
			diff_eq_int("block %ld still live", (long)db_.live[i],
				    (long)da_.live[i], i);
		diff_eq_int("seed %ld: frees", (long)db_.frees,
			    (long)da_.frees, s);
		diff_eq_int("seed %ld: bad frees", (long)db_.bad_free,
			    (long)da_.bad_free, s);
		diff_eq_int("seed %ld: outstanding", (long)db_.live_total,
			    (long)da_.live_total, s);

		diff_eq_int("seed %ld: the blob left nothing live",
			    (long)da_.live_total, 0, s);
		diff_eq_int("seed %ld: the blob made no bad free",
			    (long)da_.bad_free, 0, s);
		diff_eq_int("seed %ld: the blob made exactly twelve frees",
			    (long)da_.frees, TXDEL_BLOCKS, s);
		for (i = 0; i < TXDEL_BLOCKS; i++)
			diff_eq_int("the blob freed block %ld",
				    (long)da_.live[i], 0, i);

		txdel_trials++;
		if (da_.live_total == 0 && da_.frees == TXDEL_BLOCKS)
			txdel_freed_all++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V29TX_modem                                                           */

extern int ref_V29TX_modem(void *modem, unsigned short *in, short *out,
			   unsigned short *count);

/*
 * THE OUTPUT BUFFER IS SIZED FROM WHAT THE SLOT CAN PRODUCE, NOT FROM THE
 * INPUT COUNT -- deviation D956's lesson.  `V29TX_modem` advances `out` by
 * whatever the slot returns and clamps nothing, and `*count` goes in as an
 * input word count and comes back as an output unit count.  The wrap script
 * produces 40,000 units from an input count of one, so a buffer sized from
 * `count` would be overrun on the BLOB's side as much as ours.  The slot
 * bounds its own writes to the window, so a wrong `out` advance is a mark in
 * the wrong place and not a crash.
 */
#define TXM_OUT_LEN	40064
#define TXM_IN_LEN	256
#define TXM_FIFO_LEN	64
#define TXM_OBJ_SIZE	0x40
#define TXM_PRM_SIZE	0x20
#define TXM_MARK	((short)0x71ce)
#define TXM_MAX		40

struct txm_step {
	short	spend;			/* what the slot takes off `budget` */
	short	produce;		/* what it returns                  */
};

struct txm_call {
	long	in_off;
	long	out_off;
	long	budget_in;
	long	budget_out;
	long	ret;
};

static struct txm_call txm_log[TXM_MAX];
static int txm_calls;
static unsigned short *txm_in_base;
static short *txm_out_base;
static const struct txm_step *txm_script;
static int txm_script_len;

static short
txm_slot(void *modem, unsigned short *in, short *out, short *budget)
{
	int idx = txm_calls;
	short produce, spend;
	int k;

	if (idx < txm_script_len) {
		spend = txm_script[idx].spend;
		produce = txm_script[idx].produce;
	} else {
		spend = 0x7fff;
		produce = 0;
	}

	if (idx < TXM_MAX) {
		txm_log[idx].in_off = (long)(in - txm_in_base);
		txm_log[idx].out_off = (long)(out - txm_out_base);
		txm_log[idx].budget_in = *budget;
	}

	/* Stop a runaway script: the last entry always drains the budget. */
	if (idx + 1 >= txm_script_len)
		spend = 0x7fff;
	*budget = (short)(*budget - spend);

	for (k = 0; k < produce; k++)
		if ((long)(out - txm_out_base) + k < TXM_OUT_LEN)
			out[k] = (short)(0x2200 + ((idx << 4) & 0xff)
					 + (k & 0x0f));

	if (idx < TXM_MAX) {
		txm_log[idx].budget_out = *budget;
		txm_log[idx].ret = produce;
	}
	txm_calls++;
	(void)modem;
	return produce;
}

struct txmfix {
	unsigned char	obj[TXM_OBJ_SIZE];
	unsigned char	prm[TXM_PRM_SIZE];
	struct fax_fifo	fifo;
	unsigned short	fifobuf[TXM_FIFO_LEN];
	double		align;
};

static struct txmfix xa, xb;
static unsigned short txm_in[TXM_IN_LEN];
static short txm_out_a[TXM_OUT_LEN], txm_out_b[TXM_OUT_LEN];

/*
 * Lay one transmitter down.  Every field the function READS is planted --
 * including `V29TXP_PROCESS`, which it uses as a SUBSCRIPT into the parameter
 * block and then CALLS.  D955 / finding F8587: a blob-against-blob dry run
 * cannot catch an unplanted subscript, and here an unplanted slot is a jump
 * into pseudorandom bytes.
 */
static void
txm_fixture(struct txmfix *f, unsigned seed, int gate, short fifo_size,
	    unsigned short fifo_count, int word)
{
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < TXM_OBJ_SIZE; i++)
		f->obj[i] = (unsigned char)rng_next();
	for (i = 0; i < TXM_PRM_SIZE; i++)
		f->prm[i] = (unsigned char)rng_next();
	for (i = 0; i < TXM_FIFO_LEN; i++)
		f->fifobuf[i] = (unsigned short)rng_next();

	put_ptr(f->obj, V29TX_OBJ_PARAMS, f->prm);
	memcpy(f->obj + V29TX_OBJ_RESULT, &word, sizeof word);

	put_ptr(f->prm, V29TXP_FIFO, &f->fifo);
	put_int(f->prm, V29TXP_INT_0008, gate);
	put_ptr(f->prm, V29TXP_PROCESS, (void *)txm_slot);

	f->fifo.short_000 = 0;
	f->fifo.size = fifo_size;
	f->fifo.fill = 0;
	f->fifo.buf = f->fifobuf;
	f->fifo.count = fifo_count;
	f->fifo.rd = 0;
	f->fifo.wr = 0;
}

static long txm_fifo_arm, txm_direct_arm, txm_flag_sep, txm_noflag;
static long txm_multi_call, txm_wrapped, txm_out_moved, txm_in_static;
static long txm_budget_neg, txm_slot_called;

static long
txm_blk_diff(const unsigned char *a, const unsigned char *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return i;
	return -1;
}

/*
 * The handle, the parameter block and the FIFO object byte for byte, with
 * only the three per-fixture pointers skipped.
 */
static long
txm_first_diff(const struct txmfix *a, const struct txmfix *b)
{
	int i;
	int lo = (int)((const char *)&a->fifo.buf - (const char *)&a->fifo);

	for (i = 0; i < TXM_OBJ_SIZE; i++) {
		if (i >= V29TX_OBJ_PARAMS
		    && i < V29TX_OBJ_PARAMS + (int)sizeof(void *))
			continue;
		if (a->obj[i] != b->obj[i])
			return i;
	}
	for (i = 0; i < TXM_PRM_SIZE; i++) {
		if (i >= V29TXP_FIFO && i < V29TXP_FIFO + (int)sizeof(void *))
			continue;
		if (i >= V29TXP_PROCESS
		    && i < V29TXP_PROCESS + (int)sizeof(void *))
			continue;
		if (a->prm[i] != b->prm[i])
			return TXM_OBJ_SIZE + i;
	}
	for (i = 0; i < (int)sizeof(a->fifo); i++) {
		if (i >= lo && i < lo + (int)sizeof(void *))
			continue;
		if (((const unsigned char *)(const void *)&a->fifo)[i]
		    != ((const unsigned char *)(const void *)&b->fifo)[i])
			return TXM_OBJ_SIZE + TXM_PRM_SIZE + i;
	}
	return -1;
}

static void
run_txm_one(const struct txm_step *script, int len, unsigned short count,
	    int gate, short fifo_size, unsigned short fifo_count, int word,
	    unsigned seed, long where)
{
	struct txm_call la[TXM_MAX], lb[TXM_MAX];
	int na, nb, k;
	int ra, rb;
	unsigned short ca, cb;
	unsigned char ra_b1, rb_b1;

	txm_fixture(&xa, seed, gate, fifo_size, fifo_count, word);
	txm_fixture(&xb, seed, gate, fifo_size, fifo_count, word);

	rng_seed(seed ^ 0x1f1f1f1fu);
	for (k = 0; k < TXM_IN_LEN; k++)
		txm_in[k] = (unsigned short)rng_next();
	for (k = 0; k < TXM_OUT_LEN; k++)
		txm_out_a[k] = txm_out_b[k] = TXM_MARK;

	txm_script = script;
	txm_script_len = len;
	txm_in_base = txm_in;

	txm_calls = 0;
	memset(txm_log, 0, sizeof(txm_log));
	txm_out_base = txm_out_a;
	ca = count;
	ra = ref_V29TX_modem(xa.obj, txm_in, txm_out_a, &ca);
	na = txm_calls;
	memcpy(la, txm_log, sizeof(la));
	ra_b1 = xa.obj[V29TX_OBJ_RESULT_B1];

	txm_calls = 0;
	memset(txm_log, 0, sizeof(txm_log));
	txm_out_base = txm_out_b;
	cb = count;
	rb = V29TX_modem(xb.obj, txm_in, txm_out_b, &cb);
	nb = txm_calls;
	memcpy(lb, txm_log, sizeof(lb));
	rb_b1 = xb.obj[V29TX_OBJ_RESULT_B1];

	diff_eq_int("at %ld: V29TX_modem returned", (long)rb, (long)ra, where);
	diff_eq_int("at %ld: the count came back", (long)cb, (long)ca, where);
	diff_eq_int("at %ld: the slot was called the same number of times",
		    (long)nb, (long)na, where);
	diff_eq_int("at %ld: the result flag byte", (long)rb_b1, (long)ra_b1,
		    where);
	for (k = 0; k < na && k < TXM_MAX; k++) {
		diff_eq_int("call %ld: the input offset", lb[k].in_off,
			    la[k].in_off, k);
		diff_eq_int("call %ld: the output offset", lb[k].out_off,
			    la[k].out_off, k);
		diff_eq_int("call %ld: the budget in", lb[k].budget_in,
			    la[k].budget_in, k);
	}

	diff_eq_int("at %ld: first differing handle byte",
		    txm_first_diff(&xa, &xb), -1, where);
	diff_eq_int("at %ld: first differing output byte",
		    txm_blk_diff((const unsigned char *)txm_out_b,
				 (const unsigned char *)txm_out_a,
				 (int)sizeof(txm_out_a)), -1, where);
	diff_eq_int("at %ld: first differing FIFO buffer byte",
		    txm_blk_diff((const unsigned char *)xb.fifobuf,
				 (const unsigned char *)xa.fifobuf,
				 (int)sizeof(xa.fifobuf)), -1, where);

	/* The separating counts, taken FROM THE BLOB'S RUN. */
	if (na > 0)
		txm_slot_called++;
	if (na > 1)
		txm_multi_call++;
	if (gate == 0)
		txm_fifo_arm++;
	else
		txm_direct_arm++;
	if ((ra_b1 & V29TX_RESULT_B1_BIT1) != 0
	    && xa.obj[V29TX_OBJ_RESULT] == V29TX_RESULT_BYTE_07)
		txm_flag_sep++;
	if ((ra_b1 & V29TX_RESULT_B1_BIT1) == 0)
		txm_noflag++;
	if (na > 1 && la[1].out_off != 0)
		txm_out_moved++;
	if (na > 1 && la[1].in_off == 0)
		txm_in_static++;
	if (na > 0 && la[na - 1].budget_out < 0)
		txm_budget_neg++;
	{
		long tot = 0;

		for (k = 0; k < na && k < TXM_MAX; k++)
			tot += la[k].ret;
		if (tot > 0x7fff)
			txm_wrapped++;
	}
}

static int
run_txmodem(void)
{
	/* One call that drains the whole 0x30 budget in one go. */
	static const struct txm_step s1[] = { { 0x30, 4 } };
	/* Three calls, part of the budget each, then a drain. */
	static const struct txm_step s2[] = { { 0x10, 3 }, { 0x10, 5 },
					      { 0x10, 2 } };
	/* A slot that spends NOTHING twice: the loop must keep going. */
	static const struct txm_step s3[] = { { 0, 1 }, { 0, 2 }, { 0x30, 3 } };
	/* Overshoot: the budget goes negative and the loop still stops. */
	static const struct txm_step s4[] = { { 0x100, 7 } };
	/* The wrap: 40,000 units out of an input count of one. */
	static const struct txm_step s5[] = { { 0x10, 20000 },
					      { 0x10, 20000 },
					      { 0x10, 3 } };
	/* A slot that produces nothing at all. */
	static const struct txm_step s6[] = { { 0x30, 0 } };

	diff_begin("V29TX_modem");

	/* The FIFO arm with room for everything: the flag is NOT written. */
	run_txm_one(s1, 1, 8, 0, 64, 0, 0x11223344, 0x2a290000u, 1);
	/* The FIFO arm with the queue nearly full: flag AND status byte. */
	run_txm_one(s2, 3, 20, 0, 64, 60, 0x11223344, 0x2a290001u, 2);
	/* Completely full: nothing taken at all. */
	run_txm_one(s1, 1, 12, 0, 64, 64, 0x00000000, 0x2a290002u, 3);
	/* The DIRECT arm: the FIFO is untouched and so is the flag. */
	run_txm_one(s2, 3, 20, 1, 64, 60, 0x11223344, 0x2a290003u, 4);
	run_txm_one(s3, 3, 5, 1, 64, 0, 0x7fffffff, 0x2a290004u, 5);
	/* The budget driven negative. */
	run_txm_one(s4, 1, 3, 1, 64, 0, 0x11223344, 0x2a290005u, 6);
	/* The far corner: the running short total wraps. */
	run_txm_one(s5, 3, 1, 1, 64, 0, 0x11223344, 0x2a290006u, 7);
	/* A zero input count, on both arms. */
	run_txm_one(s6, 1, 0, 0, 64, 0, 0x11223344, 0x2a290007u, 8);
	run_txm_one(s6, 1, 0, 1, 64, 0, 0x11223344, 0x2a290008u, 9);
	/*
	 * The result word seeded with every bit set, so a wrong bit cleared on
	 * entry or a wrong offset read back is a different return value.
	 */
	run_txm_one(s1, 1, 8, 1, 64, 0, -1, 0x2a290009u, 10);

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* ModDataV29                                                            */

extern unsigned short ref_ModDataV29(void *modem, const unsigned short *bits,
				     short *samples, unsigned short count);
extern void ref_SMC_init(void *smc, const void *cfg);
extern void ref_FPM_PPS_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_PPS_free(void *state);

#define MOD_RING	64
#define MOD_PHASES	10
#define MOD_COEFFS	120
#define MOD_OUT		4096
#define MOD_NDATA	256
#define MOD_BLOCKS	12
#define MOD_MARK	((short)0x5ead)

struct modfix {
	unsigned char	obj[0x40];
	unsigned char	fp[TX_SIZE];
	short		ri[MOD_RING], rq[MOD_RING], sym[MOD_RING];
	double		align;
};

static struct modfix moda, modb;
static short mod_imap[16], mod_qmap[16];
static short mod_cos[32], mod_sin[32];
static unsigned short mod_pmap[16];
static short mod_ci[MOD_COEFFS], mod_cq[MOD_COEFFS];
static unsigned short mod_data[MOD_NDATA];
static short mod_out_a[MOD_OUT], mod_out_b[MOD_OUT];

/*
 * Built here rather than taken from the object's own banks, for finding
 * F3574's reason: a real bank repeats entries, and a transposition inside a
 * repeated run is invisible.  Every entry below is distinct.
 */
static void
mod_tables(void)
{
	int i;

	for (i = 0; i < 16; i++) {
		mod_imap[i] = (short)(1000 + i * 37);
		mod_qmap[i] = (short)(-900 - i * 41);
		mod_pmap[i] = (unsigned short)((i * 5 + 1) & 0x0f);
	}
	for (i = 0; i < 32; i++) {
		mod_cos[i] = (short)(3000 + i * 611);
		mod_sin[i] = (short)(-2500 + i * 577);
	}
	for (i = 0; i < MOD_COEFFS; i++) {
		mod_ci[i] = (short)(((i * 811) % 6007) - 3000);
		mod_cq[i] = (short)(((i * 907) % 5501) - 2700);
	}
	rng_seed(0x0d0da7a0u);
	for (i = 0; i < MOD_NDATA; i++)
		mod_data[i] = (unsigned short)(rng_next() & 0x0f);
}

static void
mod_fixture(struct modfix *f, unsigned seed)
{
	struct fpm_smc_cfg smc;
	struct fpm_pps_cfg pps;
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < (int)sizeof(f->obj); i++)
		f->obj[i] = (unsigned char)rng_next();
	for (i = 0; i < TX_SIZE; i++)
		f->fp[i] = (unsigned char)rng_next();
	for (i = 0; i < MOD_RING; i++) {
		f->ri[i] = (short)(rng_next() % 20001u) - 10000;
		f->rq[i] = (short)(rng_next() % 20001u) - 10000;
		f->sym[i] = (short)((rng_next() % 200u) + 20u);
	}

	put_ptr(f->obj, V29_OBJ_TX, f->fp);

	put_ptr(f->fp, V29FP_SMC_RING + 0x00, f->ri);
	put_ptr(f->fp, V29FP_SMC_RING + 0x04, f->rq);
	put_ptr(f->fp, V29FP_SMC_RING + 0x08, f->sym);
	put_short(f->fp, V29FP_SMC_RING + 0x0c, 0);	/* widx */
	put_short(f->fp, V29FP_SMC_RING + 0x0e, 0);	/* ridx */
	put_short(f->fp, V29FP_SMC_RING + 0x10, MOD_RING);

	/* V.29 9600's shape, from t_faxsmc.c's own enumeration. */
	memset(&smc, 0, sizeof(smc));
	smc.f00 = 0;			/* the COMPLEX output form  */
	smc.direct = 1;
	smc.rot_step = 17;
	smc.rot_mod = 24;
	smc.qshift = 0;
	smc.qmask = 7;
	smc.amask = 8;
	smc.pmask = 7;
	smc.pmap = mod_pmap;
	smc.imap = mod_imap;
	smc.qmap = mod_qmap;
	smc.cosine = mod_cos;
	smc.sine = mod_sin;
	ref_SMC_init(f->fp + V29FP_SMC, &smc);

	memset(&pps, 0, sizeof(pps));
	pps.phases = MOD_PHASES;
	pps.step = MOD_PHASES;		/* one output per symbol; see below */
	pps.mapped = 0;			/* V29TX_create clears it           */
	pps.scale = 32767;
	pps.step_adj = 0;
	pps.imap = mod_imap;
	pps.qmap = mod_qmap;
	pps.coeff_i = mod_ci;
	pps.coeff_q = mod_cq;
	pps.coeffs = MOD_COEFFS;
	ref_FPM_PPS_init(f->fp + V29FP_PPS, &pps, 1);
}

static void
mod_free(struct modfix *f)
{
	ref_FPM_PPS_free(f->fp + V29FP_PPS);
}

/*
 * The private block byte for byte, with the three ring pointers and the
 * shaper's two per-instance histories skipped.
 */
static long
mod_fp_diff(const struct modfix *a, const struct modfix *b)
{
	int i;
	int hlo = V29FP_PPS + (int)offsetof(struct fpm_pps, hist_i);

	for (i = 0; i < TX_SIZE; i++) {
		if (i >= V29FP_SMC_RING && i < V29FP_SMC_RING + 0x0c)
			continue;
		if (i >= hlo && i < hlo + 2 * (int)sizeof(void *))
			continue;
		if (a->fp[i] != b->fp[i])
			return i;
	}
	return -1;
}

static long mod_ret_nonzero, mod_state_carried, mod_ring_moved, mod_zero_count;

static int
run_moddata(void)
{
	static const unsigned short counts[] = { 1, 4, 11, 0, 20 };
	int c, blk, i;

	mod_tables();
	diff_begin("ModDataV29");

	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		unsigned seed = 0x30d0000u + (unsigned)c;
		short prev_widx = -1;

		mod_fixture(&moda, seed);
		mod_fixture(&modb, seed);

		/*
		 * MANY CONSECUTIVE BLOCKS, because the ring cursor, the
		 * encoder's quadrant accumulator and the shaper's phase and
		 * history all carry across -- finding F8790's rule.  A
		 * one-block fixture cannot see a stage fed from the wrong
		 * offset once the state has built up.
		 */
		for (blk = 0; blk < MOD_BLOCKS; blk++) {
			const unsigned short *bits =
				mod_data + (blk * 17) % (MOD_NDATA - 32);
			unsigned short count = counts[c];
			unsigned short ka, kb;
			long where = (long)c * 100 + blk;
			short widx;

			for (i = 0; i < MOD_OUT; i++)
				mod_out_a[i] = mod_out_b[i] = MOD_MARK;

			ka = ref_ModDataV29(moda.obj, bits, mod_out_a, count);
			kb = ModDataV29(modb.obj, bits, mod_out_b, count);

			diff_eq_int("at %ld: ModDataV29 returned", (long)kb,
				    (long)ka, where);
			diff_eq_int("at %ld: the return fits the buffer",
				    ka < MOD_OUT, 1, where);
			if (ka >= MOD_OUT)
				break;
			for (i = 0; i < (int)ka; i++)
				diff_eq_int("sample %ld", (long)mod_out_b[i],
					    (long)mod_out_a[i], i);
			diff_eq_int("at %ld: nothing past the returned count",
				    mod_out_a[ka] == MOD_MARK, 1, where);
			diff_eq_int("at %ld: first differing block byte",
				    mod_fp_diff(&moda, &modb), -1, where);
			for (i = 0; i < MOD_RING; i++) {
				diff_eq_int("ring i[%ld]", (long)modb.ri[i],
					    (long)moda.ri[i], i);
				diff_eq_int("ring q[%ld]", (long)modb.rq[i],
					    (long)moda.rq[i], i);
				diff_eq_int("ring sym[%ld]", (long)modb.sym[i],
					    (long)moda.sym[i], i);
			}

			widx = get_short(moda.fp, V29FP_SMC_RING + 0x0c);
			if (ka > 0)
				mod_ret_nonzero++;
			if (count == 0)
				mod_zero_count++;
			if (prev_widx >= 0 && widx != prev_widx)
				mod_ring_moved++;
			if (blk > 0 && ka > 0)
				mod_state_carried++;
			prev_widx = widx;
		}

		mod_free(&moda);
		mod_free(&modb);
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* RxHdxDataV29 and RxHdxErrorV29                                        */

extern short ref_RxHdxDataV29(void *modem, short *in, short *out,
			      unsigned short *count);
extern short ref_RxHdxErrorV29(void *modem, short *in, short *out,
			       unsigned short *count);

#define HDX_BLOCKS	60
#define HDX_COUNT	160
#define HDX_OUT		DEM_BUF
#define HDX_MARK	((short)0x6bad)

static short hdx_mrf_a[DEM_BUF], hdx_sre_a[DEM_BUF], hdx_det_a[DEM_BUF];
static short hdx_mrf_b[DEM_BUF], hdx_sre_b[DEM_BUF], hdx_det_b[DEM_BUF];
static short hdx_in[DEM_BUF], hdx_work[DEM_BUF];
static short hdx_out_a[HDX_OUT], hdx_out_b[HDX_OUT];

struct hdx_setup {
	short	gate_14;	/* DemodDataV29's tone pre-pass gate       */
	short	gate_1c;	/* DataCarrierDetectV29's V.21 scan gate   */
	int	mtd_absent;	/* the pre-pass detector's forced verdict  */
	int	det_gate;	/* V29DET_INT_0008: the second gate        */
	int	carrier;	/* the seed for sre.active / agc.signal    */
	int	status;		/* the seed for the whole status word      */
	/*
	 * THE EQUALISER'S ERROR, AND IT IS ONLY LIVE ON ONE PATH.
	 *
	 * `GetSNRV29` is `14 - mse` and `RxHdxDataV29` compares it against 8,
	 * so the ONE input that separates the threshold from its neighbour is
	 * `mse == 6`.  On every path where `FPM_FSE_receive` runs, `mse` is
	 * whatever the equaliser leaves and cannot be steered -- and the first
	 * version of this test asserted the threshold anyway and was not
	 * caught when the source was changed to `<= 7`.
	 *
	 * The pre-pass abandon is the path that makes it reachable:
	 * `DemodDataV29` returns before touching the equaliser, so `mse` keeps
	 * the value planted here for the whole trial.  `mtd_absent` = 0 forces
	 * the pre-pass detector to NOSIGNAL, which is non-zero and abandons.
	 */
	short	mse;
};

/*
 * The whole receiver, and every field either handler READS is planted.
 *
 * D955 / finding F8587 again: `fixture()` fills the receive and detection
 * blocks pseudorandomly, and `DataCarrierDetectV29` uses `V29DET_V21_MTD` and
 * `V29DET_V21_BUF` as pointers and `V29RX_SDM` as the descrambler's state --
 * an unplanted one of those is a wild pointer that both sides follow equally
 * and agree about, right up to the segfault.
 */
static void
hdx_build(struct fix *f, unsigned seed, const struct hdx_setup *u,
	  short *mrfbuf, short *srebuf, short *detbuf)
{
	struct dem_setup d;
	struct fpm_sdm_cfg sdm;
	int act, sig;

	d.gate_14 = u->gate_14;
	d.mtd_absent = u->mtd_absent;
	d.enables = 0;
	dem_build(f, seed, &d, mrfbuf, srebuf, detbuf);

	/* The V.21 scan's own detector and gain control. */
	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->det + V29DET_V21_AGC),
			 &AGCv22_CFG, 1);
	put_ptr(f->det, V29DET_V21_MTD,
		ref_FPM_MTD_create(0, u->mtd_absent ? &mtd_absent_cfg
						    : &mtd_nosignal_cfg));
	put_short(f->det, V29DET_GATE_1C, u->gate_1c);
	put_short(f->det, V29DET_V21_ENABLE, 0);
	put_short(f->det, V29DET_V21_SAMPLES, 0);

	/* The carrier-detect gates, all off but the ones under test. */
	put_short(f->rx, V29RX_SHORT_0046, 0);
	put_short(f->rx, V29RX_SHORT_4F64, 0);
	put_short(f->rx, V29RX_RMS_REF, 4000);
	put_short(f->rx, V29RX_RMS_N, 0);

	/*
	 * The quality average STARTS AT BLOCK ZERO, so HDX_BLOCKS blocks walk
	 * the seed, the whole 1..0x31 smoothing run and the 0x32 verdict.
	 * `QualityDetectV29` is what finding F8790 is about, and a one-block
	 * fixture never reaches any of it.
	 */
	put_short(f->rx, V29RX_DEC_ERROR_AVG, 0);
	put_short(f->rx, V29RX_DEC_ERROR_N, 0);
	put_short(f->rx, V29RX_DEC_ERROR_LIMIT, 400);
	put_short(f->rx, V29RX_SHORT_4F62, 0);

	/* The descrambler the DATA state runs over its own output. */
	sdm.nbits = 4;
	sdm.tap1 = 9;
	sdm.tap2 = 11;
	ref_FPM_SDM_init((struct fpm_sdm *)(void *)(f->rx + V29RX_SDM), &sdm);
	((struct fpm_sdm *)(void *)(f->rx + V29RX_SDM))->reg = 0x0001357bu;

	act = (u->carrier == 0) ? 0 : 1;
	sig = (u->carrier == 0) ? 0 : 1;
	put_int(f->rx, V29RX_SRE_ACTIVE, act);
	put_int(f->rx, V29RX_AGC_SIGNAL, sig);

	put_short(f->rx, V29RX_FSE_MSE, u->mse);

	put_int(f->det, V29DET_INT_0008, u->det_gate);
	put_int(f->obj, V29_OBJ_STATUS, u->status);
}

static void
hdx_free(struct fix *f)
{
	dem_free(f);
	ref_FPM_MTD_delete((struct fpm_mtd *)get_ptr(f->det, V29DET_V21_MTD));
	put_ptr(f->det, V29DET_V21_MTD, 0);
}

static long hdx_bail, hdx_proceed, hdx_units, hdx_zero_units, hdx_quality_zero;
static long hdx_snr_set, hdx_snr_clear, hdx_carrier_set, hdx_carrier_clear;
static long hdx_status_byte, hdx_err_flag, hdx_err_moved, hdx_count_zeroed;
static long hdx_snr_at, hdx_snr_over;

/*
 * `vary` ALTERNATES LOUD BLOCKS WITH SILENT ONES, and it is not a stimulus
 * sweep: it is the only shape that reaches the arm where the DATA state
 * demodulates and then reports ZERO units.
 *
 * `DataCarrierDetectV29` reads `sre.active & agc.signal` BEFORE the block is
 * demodulated, so it sees the PREVIOUS block's verdict, and
 * `QualityDetectV29` reads the same pair AFTER, so it sees this one's.  On a
 * steady stimulus the two always agree and the `setne`/`neg`/`and` that
 * forces the return to zero is never exercised.  One loud-to-quiet transition
 * per pair of blocks is what separates them; without it that named reading
 * reported a separating count of zero, which is exactly what the count is
 * for (finding F134).
 */
static void
run_hdx_one(unsigned seed, const struct hdx_setup *u, int error_state,
	    int vary, long where)
{
	int blk, i;

	dem_signal(seed ^ 0x0b10cced, 2);
	for (i = 0; i < DEM_BUF; i++)
		hdx_in[i] = dem_in[i];

	hdx_build(&fa, seed, u, hdx_mrf_a, hdx_sre_a, hdx_det_a);
	hdx_build(&fb, seed, u, hdx_mrf_b, hdx_sre_b, hdx_det_b);

	for (blk = 0; blk < HDX_BLOCKS; blk++) {
		unsigned short ca = HDX_COUNT, cb = HDX_COUNT;
		short ra, rb;
		int sa, sb;
		long id = where * 1000 + blk;
		int moved;

		if (vary) {
			dem_signal(seed ^ (0x0b10cced + (unsigned)blk),
				   (blk & 1) ? 0 : 2);
			for (i = 0; i < DEM_BUF; i++)
				hdx_in[i] = dem_in[i];
		}

		for (i = 0; i < HDX_OUT; i++)
			hdx_out_a[i] = hdx_out_b[i] = HDX_MARK;
		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];

		if (error_state)
			ra = ref_RxHdxErrorV29(fa.obj, hdx_work, hdx_out_a,
					       &ca);
		else
			ra = ref_RxHdxDataV29(fa.obj, hdx_work, hdx_out_a, &ca);
		sa = get_int(fa.obj, V29_OBJ_STATUS);

		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];
		if (error_state)
			rb = RxHdxErrorV29(fb.obj, hdx_work, hdx_out_b, &cb);
		else
			rb = RxHdxDataV29(fb.obj, hdx_work, hdx_out_b, &cb);
		sb = get_int(fb.obj, V29_OBJ_STATUS);

		diff_eq_int("at %ld: the handler returned", (long)rb, (long)ra,
			    id);
		diff_eq_int("at %ld: the count came back", (long)cb, (long)ca,
			    id);
		diff_eq_int("at %ld: the status word", (long)sb, (long)sa, id);
		diff_eq_int("at %ld: first differing output byte",
			    blk_first_diff((unsigned char *)hdx_out_b,
					   (unsigned char *)hdx_out_a,
					   HDX_OUT * 2, 0), -1, id);
		diff_eq_int("at %ld: first differing receiver byte",
			    blk_first_diff(fb.rx, fa.rx, RX_SIZE,
					   skip_rx_demod), -1, id);
		diff_eq_int("at %ld: first differing detector byte",
			    blk_first_diff(fb.det, fa.det, DET_SIZE, skip_det),
			    -1, id);
		diff_eq_int("at %ld: the resampler's buffer",
			    blk_first_diff((unsigned char *)hdx_mrf_b,
					   (unsigned char *)hdx_mrf_a,
					   DEM_BUF * 2, 0), -1, id);
		diff_eq_int("at %ld: the recoverer's buffer",
			    blk_first_diff((unsigned char *)hdx_sre_b,
					   (unsigned char *)hdx_sre_a,
					   DEM_BUF * 2, 0), -1, id);

		/*
		 * The named readings, all counted FROM THE BLOB'S RUN, so a
		 * zero at the end says the check is decoration.
		 */
		if (ca == 0)
			hdx_count_zeroed++;

		if (error_state) {
			if ((sa & V29_STATUS_ERROR) != 0)
				hdx_err_flag++;
			/*
			 * The ERROR state DEMODULATES ANYWAY, and that is what
			 * separates it from a handler that merely consumes the
			 * block: the resampler's output buffer moves.
			 */
			moved = 0;
			for (i = 0; i < HDX_COUNT; i++)
				if (hdx_mrf_a[i] != 0)
					moved = 1;
			if (moved)
				hdx_err_moved++;
			continue;
		}

		if ((sa & V29_STATUS_CARRIER) != 0)
			hdx_carrier_set++;
		else
			hdx_carrier_clear++;
		if ((sa & V29_STATUS_LOW_SNR) != 0)
			hdx_snr_set++;
		else
			hdx_snr_clear++;
		if ((sa & 0xff) == V29RX_STATUS_DATA)
			hdx_status_byte++;

		/*
		 * THE THRESHOLD WAS EVALUATED AT ITS BOUNDARY, and this is the
		 * count that says so.  `GetSNRV29` is re-read from the blob
		 * after the handler, which is the same value the handler saw
		 * on the pre-pass-abandon path because nothing between the two
		 * touches `mse`.
		 */
		{
			short snr = ref_GetSNRV29(fa.obj);

			if (snr == V29RX_SNR_THRESHOLD)
				hdx_snr_at++;
			else if (snr == V29RX_SNR_THRESHOLD + 1)
				hdx_snr_over++;
		}

		if (ra > 0) {
			hdx_units++;
			hdx_proceed++;
		} else {
			/*
			 * Zero can mean either arm.  The output buffer says
			 * which: the bail arm never writes it.
			 */
			if (hdx_out_a[0] != HDX_MARK) {
				hdx_proceed++;
				hdx_quality_zero++;
			} else {
				hdx_bail++;
			}
			hdx_zero_units++;
		}
	}

	hdx_free(&fa);
	hdx_free(&fb);
}

static int
run_hdx(void)
{
	static const struct hdx_setup cases[] = {
		/* gate14 gate1c absent detgate carrier status      mse */
		{ 0, 0, 1, 0, 1, 0, 0 },	/* the ordinary DATA path   */
		{ 1, 0, 0, 0, 1, -1, 0 },	/* pre-pass off, all bits set */
		{ 0, 0, 0, 0, 0, 0, 0 },	/* no carrier: the bail arm */
		{ 1, 0, 1, 1, 1, 0, 0 },	/* the second gate closed   */
		{ 1, 1, 1, 0, 1, (int)0xffff0000, 0 }, /* the V.21 scan armed */
		/*
		 * The threshold pair.  The pre-pass abandons, so `mse` stays
		 * planted and `GetSNRV29` answers exactly 8 and then 9 -- the
		 * only two inputs that tell `<= 8` from `<= 7` and from `< 8`.
		 */
		{ 0, 0, 0, 0, 1, 0, 6 },
		{ 0, 0, 0, 0, 1, 0, 5 }
	};
	int i;

	dem_tables();
	dcd_cfgs();
	diff_begin("RxHdxDataV29 / RxHdxErrorV29");

	for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++)
		run_hdx_one(0x8d000000u + (unsigned)i, &cases[i], 0, 0, i);

	/* The loud/quiet alternation; see the note above `run_hdx_one`. */
	run_hdx_one(0x8d100000u, &cases[0], 0, 1, 50);
	run_hdx_one(0x8d100001u, &cases[1], 0, 1, 51);

	/* The ERROR state, over the same two shapes. */
	run_hdx_one(0x8e000000u, &cases[0], 1, 0, 100);
	run_hdx_one(0x8e000001u, &cases[1], 1, 0, 101);

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 15.  The slicer chain: V29RX_epoch_det, V29RX_eq_train, V29RX_decision  */
/*
 * All three are PURE -- they call no FPM module and no other V.29 function --
 * so the fixture is an `fpm_fse` with its two output arrays, its symbol index
 * and its `cfg.owner` planted, and nothing else has to be wired.
 *
 * F8587 IS WHY THE INDEX IS PLANTED AND SWEPT.  `n_out` is a SUBSCRIPT and the
 * object reads it `movswl` where `fpm_fse.h` models it `unsigned short`, so a
 * negative index reads BEFORE both arrays.  A blob-against-blob dry run cannot
 * catch a wrong subscript -- both sides read the same neighbour and agree --
 * so `out_i` and `out_q` are aimed at the MIDDLE of two real arrays and the
 * index is swept from SL_INDEX_LO to SL_INDEX_HI including negatives, with
 * planted data at both ends.
 *
 * F8790 IS WHY EVERY TRIAL IS A SEQUENCE.  Two leaky averages, a three-point
 * history, an LFSR and three counters carry between calls, and each handover
 * fires on exactly ONE call -- so a single-call fixture sees none of it.  Every
 * trial is SL_BLOCKS consecutive calls from a fresh fixture.
 *
 * THE FUNCTION POINTER CANNOT BE COMPARED AS A BYTE PATTERN, because the blob
 * installs `ref_V29RX_eq_train` where we install `V29RX_eq_train`.  Each side's
 * `cfg.decision` is mapped through its OWN table into a small integer and the
 * two integers are compared; the slot is then restored to the sentinel before
 * the fixtures are compared byte for byte.
 *
 * THE SAMPLES ARE BOUNDED AT +/-16000 for `V27RX_epoch_det`'s reason: the two
 * squared differences are summed as `int`, and a pair at exactly +/-32768 would
 * overflow that sum -- which the object does in hardware and which is undefined
 * in C, making the answer a property of the compiler rather than of the code.
 * The `short` narrowing of `d` and of the distances still wraps and is still
 * exercised.
 */
extern unsigned short ref_V29RX_epoch_det(struct fpm_fse *state, short *angle,
					  short *mag);
extern unsigned short ref_V29RX_eq_train(struct fpm_fse *state, short *angle,
					 short *mag);
extern unsigned short ref_V29RX_decision(struct fpm_fse *state, short *angle,
					 short *mag);

#define SL_NBUF		256
#define SL_ORIGIN	(SL_NBUF / 2)
#define SL_INDEX_LO	(-48)
#define SL_INDEX_HI	48
#define SL_BLOCKS	40
#define SL_DEC		0x40		/* 0x20 of block, 0x20 of guard      */

/*
 * The equaliser is stored as a REAL `struct fpm_fse` rather than a byte block,
 * even though the three slicers touch nothing above +0x5e: a short block cast
 * to the struct is an out-of-bounds access the modern compiler is entitled to
 * warn about and to act on.  Only the first SL_FSE_LIVE bytes are filled or
 * compared, which is the region the object reaches.
 */
#define SL_FSE_LIVE	0x54

struct sl_fix {
	struct fpm_fse	fse;
	unsigned char	dec[SL_DEC];
	short		out_i[SL_NBUF];
	short		out_q[SL_NBUF];
	double		align;
};

static struct sl_fix sla, slb;

#define SL_FSE(f)	(&(f)->fse)
#define SL_FSEB(f)	((unsigned char *)(void *)&(f)->fse)

/*
 * The bytes a comparison of two equalisers must skip: `cfg.owner` points into
 * its own fixture, so the two addresses differ and always will, and
 * `cfg.decision` is normalised separately.  Nothing else in the first 0x54
 * bytes is a pointer.
 */
static int
skip_fse(int off)
{
	return off >= 0x2c && off < 0x34;
}

/* The sentinel `cfg.decision` holds when nothing has installed anything. */
static unsigned short
sl_sentinel(struct fpm_fse *state, short *angle, short *mag)
{
	(void)state;
	(void)angle;
	(void)mag;
	return 0;
}

struct sl_setup {
	int		sixteen_point;
	unsigned short	train_count;
	unsigned short	sym_count;
	short		lfsr;
	short		avg_far;
	short		avg_near;
	int		amp;		/* the steady constellation radius  */
	int		jump;		/* how far the occasional jump goes */
	int		period;		/* how often it jumps               */
	short		angle0;		/* the angle handed in each block   */
	short		astep;		/* ... and how far it moves         */
	short		mag0;		/* the magnitude handed in          */
};

static void
sl_build(struct sl_fix *f, unsigned seed, const struct sl_setup *u)
{
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < 0x60; i++)
		SL_FSEB(f)[i] = (unsigned char)rng_next();
	for (i = 0; i < SL_DEC; i++)
		f->dec[i] = (unsigned char)rng_next();

	rng_seed(seed ^ 0x9e3779b9u);
	for (i = 0; i < SL_NBUF; i++) {
		int k = i - SL_ORIGIN;
		int big = (u->period != 0 && (k % u->period) == 0);
		int v = (int)(rng_next() % 2001u) - 1000;
		int a = big ? u->jump : u->amp;

		/*
		 * A SETUP WITH NO SIGNAL AT ALL, which is the only shape that
		 * puts the sample exactly at the origin -- and that is the only
		 * place four constellation points TIE, which is what separates
		 * the slicer's `d < best` from `d <= best`.
		 */
		if (u->amp == 0 && u->jump == 0)
			v = 0;
		if (a > 16000)
			a = 16000;
		f->out_i[i] = (short)(a + v);
		f->out_q[i] = (short)(-a - v);
	}

	SL_FSE(f)->cfg.owner = f->dec;
	SL_FSE(f)->cfg.decision = sl_sentinel;
	SL_FSE(f)->cfg.taps = 16;
	SL_FSE(f)->cfg.mu[0] = 0x1111;
	SL_FSE(f)->cfg.mu[1] = 0x2222;
	SL_FSE(f)->cfg.mu[2] = 0x3333;
	SL_FSE(f)->out_i = &f->out_i[SL_ORIGIN];
	SL_FSE(f)->out_q = &f->out_q[SL_ORIGIN];
	SL_FSE(f)->n_out = 0;
	SL_FSE(f)->lms_on = 0x5a5a5a5a;
	SL_FSE(f)->lms_force = 0x3c3c3c3c;
	SL_FSE(f)->mu_sel = 0x1234;
	SL_FSE(f)->mse = 0x0123;

	put_int(f->dec, V29DEC_SIXTEEN_POINT, u->sixteen_point);
	put_short(f->dec, V29DEC_MAG_AVG_FAR, u->avg_far);
	put_short(f->dec, V29DEC_MAG_AVG_NEAR, u->avg_near);
	put_short(f->dec, V29DEC_I0, 0);
	put_short(f->dec, V29DEC_Q0, 0);
	put_short(f->dec, V29DEC_I1, 0);
	put_short(f->dec, V29DEC_Q1, 0);
	put_short(f->dec, V29DEC_I2, 0);
	put_short(f->dec, V29DEC_Q2, 0);
	put_short(f->dec, V29DEC_LAST, 0);
	put_short(f->dec, V29DEC_TRAIN_LFSR, u->lfsr);
	put_short(f->dec, V29DEC_TRAIN_COUNT, (short)u->train_count);
	put_short(f->dec, V29DEC_ANGLE_PREV, 0);
	put_short(f->dec, V29DEC_SYM_COUNT, (short)u->sym_count);
}

/*
 * Which slicer a `cfg.decision` slot holds, as a small integer, so the two
 * sides' different addresses for the same function compare equal.
 */
static long
sl_which(fpm_fse_decision fn, int is_ref)
{
	if (fn == sl_sentinel)
		return 0;
	if (is_ref) {
		if (fn == (fpm_fse_decision)ref_V29RX_epoch_det)
			return 1;
		if (fn == (fpm_fse_decision)ref_V29RX_eq_train)
			return 2;
		if (fn == (fpm_fse_decision)ref_V29RX_decision)
			return 3;
	} else {
		if (fn == V29RX_epoch_det)
			return 1;
		if (fn == V29RX_eq_train)
			return 2;
		if (fn == V29RX_decision)
			return 3;
	}
	return -1;
}

/* Which stage of the chain a trial drives. */
#define SL_EPOCH	0
#define SL_TRAIN	1
#define SL_DECIDE	2

static long sl_ret[3];			/* a non-0xffff return was seen     */
static long sl_handover[3];		/* the stage installed the next one */
static long sl_neg_index, sl_pos_index;
static long sl_far_arm, sl_near_arm;	/* both halves of the phase circle  */
static long sl_lfsr_odd, sl_lfsr_even;	/* both training points             */
static long sl_wrap;			/* the symbol counter restarted     */
static long sl_amp_bit;			/* the fourth bit reached the output */
static long sl_dec_ring1;		/* the outer ring won a decision    */

static void
run_sl_one(unsigned seed, const struct sl_setup *u, int stage, long where)
{
	int blk;

	sl_build(&sla, seed, u);
	sl_build(&slb, seed, u);

	switch (stage) {
	case SL_EPOCH:
		SL_FSE(&sla)->cfg.decision =
			(fpm_fse_decision)ref_V29RX_epoch_det;
		SL_FSE(&slb)->cfg.decision = V29RX_epoch_det;
		break;
	case SL_TRAIN:
		SL_FSE(&sla)->cfg.decision =
			(fpm_fse_decision)ref_V29RX_eq_train;
		SL_FSE(&slb)->cfg.decision = V29RX_eq_train;
		break;
	default:
		SL_FSE(&sla)->cfg.decision =
			(fpm_fse_decision)ref_V29RX_decision;
		SL_FSE(&slb)->cfg.decision = V29RX_decision;
		break;
	}

	for (blk = 0; blk < SL_BLOCKS; blk++) {
		short aa, ma, ab, mb;
		unsigned short ra, rb;
		long wa, wb, now;
		long id = where * 1000 + blk;
		short idx;

		/*
		 * The subscript sweep, INCLUDING NEGATIVES.  Both ends land on
		 * planted data because the arrays are aimed at their middle.
		 */
		idx = (short)(SL_INDEX_LO
			      + (blk * (SL_INDEX_HI - SL_INDEX_LO))
				/ (SL_BLOCKS - 1));
		if (idx < 0)
			sl_neg_index++;
		else
			sl_pos_index++;
		SL_FSE(&sla)->n_out = (unsigned short)idx;
		SL_FSE(&slb)->n_out = (unsigned short)idx;

		aa = ab = (short)(u->angle0 + (short)(blk * u->astep));
		ma = mb = u->mag0;

		/*
		 * DISPATCH THROUGH THE SLOT, so that once a stage hands over
		 * the trial goes on through the NEXT one -- which is what the
		 * equaliser does and is the only way a single trial reaches
		 * more than one stage.  `now` is which stage this block
		 * actually ran, and every counter below is indexed by it
		 * rather than by the stage the trial STARTED in.
		 */
		now = sl_which(SL_FSE(&sla)->cfg.decision, 1);

		ra = (*SL_FSE(&sla)->cfg.decision)(SL_FSE(&sla), &aa, &ma);
		rb = (*SL_FSE(&slb)->cfg.decision)(SL_FSE(&slb), &ab, &mb);

		diff_eq_int("at %ld: the slicer returned", (long)rb, (long)ra,
			    id);
		diff_eq_int("at %ld: the slicer left angle", (long)ab, (long)aa,
			    id);
		diff_eq_int("at %ld: the slicer left mag", (long)mb, (long)ma,
			    id);

		wa = sl_which(SL_FSE(&sla)->cfg.decision, 1);
		wb = sl_which(SL_FSE(&slb)->cfg.decision, 0);
		diff_eq_int("at %ld: the slot holds a known slicer", wb >= 0, 1,
			    id);
		diff_eq_int("at %ld: the installed slicer", wb, wa, id);

		if (now >= 1 && now <= 3) {
			if (ra != 0xffff)
				sl_ret[now - 1]++;
			if (wa != now)
				sl_handover[now - 1]++;
		}

		/*
		 * The slot is the ONLY field whose bytes legitimately differ,
		 * so it is normalised before the blocks are compared and put
		 * back afterwards.
		 */
		SL_FSE(&sla)->cfg.decision = sl_sentinel;
		SL_FSE(&slb)->cfg.decision = sl_sentinel;

		diff_eq_int("at %ld: first differing decoder byte",
			    blk_first_diff(slb.dec, sla.dec, SL_DEC, 0), -1,
			    id);
		diff_eq_int("at %ld: first differing equaliser byte",
			    blk_first_diff(SL_FSEB(&slb), SL_FSEB(&sla),
					   SL_FSE_LIVE, skip_fse), -1, id);
		diff_eq_int("at %ld: first differing sample byte",
			    blk_first_diff((unsigned char *)slb.out_i,
					   (unsigned char *)sla.out_i,
					   sizeof(sla.out_i), 0), -1, id);

		SL_FSE(&sla)->cfg.decision = (fpm_fse_decision)
			(wa == 1 ? (fpm_fse_decision)ref_V29RX_epoch_det
			 : wa == 2 ? (fpm_fse_decision)ref_V29RX_eq_train
			 : wa == 3 ? (fpm_fse_decision)ref_V29RX_decision
			 : sl_sentinel);
		SL_FSE(&slb)->cfg.decision =
			wb == 1 ? V29RX_epoch_det
			: wb == 2 ? V29RX_eq_train
			: wb == 3 ? V29RX_decision : sl_sentinel;

		/* Which arms and outcomes this block actually reached. */
		if (now == SL_EPOCH + 1) {
			if (aa == V29RX_DEC_ANGLE[V29_EPOCH_POINT_FAR])
				sl_far_arm++;
			else if (aa == V29RX_DEC_ANGLE[V29_EPOCH_POINT_NEAR])
				sl_near_arm++;
		} else if (now == SL_TRAIN + 1) {
			if (ma == V29RX_DEC_MAG[0])
				sl_lfsr_even++;
			else
				sl_lfsr_odd++;
		} else if (now == SL_DECIDE + 1) {
			if ((ra & 8) != 0)
				sl_amp_bit++;
			if (ma == V29RX_DEC_MAG[8] || ma == V29RX_DEC_MAG[9])
				sl_dec_ring1++;
		}
		if (get_short(sla.dec, V29DEC_SYM_COUNT)
		    == (short)V29DEC_SYM_COUNT_RESTART)
			sl_wrap++;
	}
}

/*
 * THE NAMED WRONG READINGS, and each one is a sentence somebody could have
 * believed.  `sl_model` recomputes the ONE observable each defect can move --
 * the chosen constellation index, which is what `angle`, `mag` and the return
 * are all derived from -- and the counter says how many trials separated it.
 */
enum sl_defect {
	S_NONE = 0,
	S_Q_SHIFT_15,		/* the Q term by >>15, i.e. symmetric  D1171 */
	S_I_SHIFT_16,		/* ... or the I term by >>16 instead         */
	S_MAPS_SWAPPED,		/* V29RX_DEC_IMAP and _QMAP transposed       */
	S_TIE_LOW,		/* `d <= best`, so a tie keeps the HIGHER    */
	S_POINTS_SWAPPED,	/* 8 and 16 the other way round              */
	S_IDX_UNSIGNED,		/* n_out read unsigned                       */
	S_MAX
};

static long sl_sep[S_MAX];

static short
sl_model(const struct sl_fix *f, short idx, enum sl_defect d)
{
	short best = 0x7fff;
	short bi = 0;
	short k;
	short points;
	int n = idx;
	short i, q;

	points = get_int(f->dec, V29DEC_SIXTEEN_POINT) ? 16 : 8;
	if (d == S_POINTS_SWAPPED)
		points = (short)(24 - points);
	if (d == S_IDX_UNSIGNED)
		n = (int)(unsigned short)idx;

	i = f->out_i[SL_ORIGIN + n];
	q = f->out_q[SL_ORIGIN + n];

	for (k = 0; k < points; k = (short)(k + 1)) {
		short im = V29RX_DEC_IMAP[k];
		short qm = V29RX_DEC_QMAP[k];
		short di, dq, dd;

		if (d == S_MAPS_SWAPPED) {
			short t = im;

			im = qm;
			qm = t;
		}
		di = (short)(i - im);
		dq = (short)(q - qm);
		if (d == S_Q_SHIFT_15)
			dd = (short)(((di * di) >> 15) + ((dq * dq) >> 15));
		else if (d == S_I_SHIFT_16)
			dd = (short)(((di * di) >> 16) + ((dq * dq) >> 16));
		else
			dd = (short)(((di * di) >> 15) + ((dq * dq) >> 16));

		if (d == S_TIE_LOW ? dd <= best : dd < best) {
			best = dd;
			bi = k;
		}
	}
	return bi;
}

static void
run_sl_model(unsigned seed, const struct sl_setup *u, long where)
{
	int blk;

	sl_build(&sla, seed, u);

	for (blk = 0; blk < SL_BLOCKS; blk++) {
		short aa = (short)(u->angle0 + (short)(blk * u->astep));
		short ma = u->mag0;
		short idx = (short)(SL_INDEX_LO
				    + (blk * (SL_INDEX_HI - SL_INDEX_LO))
				      / (SL_BLOCKS - 1));
		short good;
		int d;

		SL_FSE(&sla)->n_out = (unsigned short)idx;
		SL_FSE(&sla)->cfg.decision =
			(fpm_fse_decision)ref_V29RX_decision;
		ref_V29RX_decision(SL_FSE(&sla), &aa, &ma);

		/* The blob's own answer, recovered from what it reported. */
		good = sl_model(&sla, idx, S_NONE);
		diff_eq_int("at %ld: the model tracks the blob's magnitude",
			    (long)ma, (long)V29RX_DEC_MAG[good],
			    where * 1000 + blk);
		diff_eq_int("at %ld: the model tracks the blob's angle",
			    (long)aa, (long)V29RX_DEC_ANGLE[good],
			    where * 1000 + blk);

		for (d = 1; d < (int)S_MAX; d++)
			if (sl_model(&sla, idx, (enum sl_defect)d) != good)
				sl_sep[d]++;
	}
}

static int
run_slicers(void)
{
	static const struct sl_setup cases[] = {
	  /* 16pt count sym   lfsr  far   near  amp   jump  per angle step mag */
	  {  1,  0,    0,     0x55, 0,    0,    6000, 0,     0, 0,    977,  6144 },
	  {  0,  0,    0,     0x55, 0,    0,    6000, 0,     0, 0,    977,  2896 },
	  {  1,  0x7e, 0x7ffe, 0x2a, 300,  300, 4000, 15000, 5, 900,  4099, 10240 },
	  {  0,  0x7f, 0x7fff, 0x01, 10,   10,  2000, 12000, 3, 0x4000, 771, 8689 },
	  {  1,  0x17c, 0x100, 0x7f, 0,    0,   9000, 0,     0, 0x7000, 61,  6144 },
	  {  1,  0x81, 0,      0x00, 8000, 8000, 100, 32000, 2, 12000, 2731, 3000 },
	  {  0,  0,    0,      0x55, 0,    0,   16000, 0,    0, 0x3fff, 8192, 16000 },
	  /*
	   * NO SIGNAL AT ALL, which puts every sample exactly at the origin --
	   * where constellation points 1, 3, 5 and 7 are all the same distance
	   * away.  It is the only shape that separates `d < best` from
	   * `d <= best`, and without it that named wrong reading reported a
	   * separating count of zero.  F134's argument, met.
	   */
	  {  1,  0,    0,      0x55, 0,    0,   0,     0,    0, 0,      1013, 6144 },
	  {  0,  0,    0,      0x2a, 0,    0,   0,     0,    0, 0x2000, 511,  2896 }
	};
	int i;

	diff_begin("the V.29 slicer chain");

	for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
		run_sl_one(0xc9000000u + (unsigned)i, &cases[i], SL_EPOCH, i);
		run_sl_one(0xc9100000u + (unsigned)i, &cases[i], SL_TRAIN,
			   100 + i);
		run_sl_one(0xc9200000u + (unsigned)i, &cases[i], SL_DECIDE,
			   200 + i);
		run_sl_model(0xc9300000u + (unsigned)i, &cases[i], 300 + i);
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 16.  The half-duplex receive machine                                   */
/*
 * `RxNextStateV29` is testable on its own -- it calls nothing but
 * `FPM_AGC_Freeze` and `dsplibs_debug_printf` -- so its six arms are driven
 * directly, every state value from 0 to 7, at both debug levels.  The four
 * handlers need the whole receiver, so they reuse `hdx_build`: the same wired
 * fixture `RxHdxDataV29` is tested through.
 *
 * THE HANDLER SLOT IS THE ONE FIELD THAT CANNOT BE COMPARED AS BYTES, exactly
 * as `cfg.decision` is above, and it is normalised the same way.
 */
extern void  ref_RxNextStateV29(void *modem);
extern short ref_RxHdxStartV29(void *modem, short *in, short *out,
			       unsigned short *count);
extern short ref_RxHdxIdleV29(void *modem, short *in, short *out,
			      unsigned short *count);
extern short ref_RxHdxPrtcolV29(void *modem, short *in, short *out,
				unsigned short *count);
extern short ref_RxHdxEpochDetV29(void *modem, short *in, short *out,
				  unsigned short *count);
extern short ref_RxHdxDataV29(void *modem, short *in, short *out,
			      unsigned short *count);

/*
 * `skip_det` plus the handler slot, which holds two different addresses for
 * the same function and is compared as a small integer instead.
 */
static int
skip_det_state(int off)
{
	if (off >= V29DET_HANDLER && off < V29DET_HANDLER + 4)
		return 1;
	return skip_det(off);
}

static long
st_which(const void *fn, int is_ref)
{
	if (fn == 0)
		return 0;
	if (is_ref) {
		if (fn == (const void *)ref_RxHdxStartV29)	return 1;
		if (fn == (const void *)ref_RxHdxIdleV29)	return 2;
		if (fn == (const void *)ref_RxHdxPrtcolV29)	return 3;
		if (fn == (const void *)ref_RxHdxEpochDetV29)	return 4;
		if (fn == (const void *)ref_RxHdxDataV29)	return 5;
		if (fn == (const void *)ref_RxHdxErrorV29)	return 6;
	} else {
		if (fn == (const void *)RxHdxStartV29)		return 1;
		if (fn == (const void *)RxHdxIdleV29)		return 2;
		if (fn == (const void *)RxHdxPrtcolV29)		return 3;
		if (fn == (const void *)RxHdxEpochDetV29)	return 4;
		if (fn == (const void *)RxHdxDataV29)		return 5;
		if (fn == (const void *)RxHdxErrorV29)		return 6;
	}
	return -1;
}

static long st_arm[8];			/* how often each state was entered */
static long st_installed[7];		/* ... and each handler installed   */
static long st_freeze;			/* the AGC-freeze arm               */
static long st_coeff_step;		/* the coefficient-step arm         */
static long st_debug;			/* a trial that printed             */
static long st_done_a, st_done_b;	/* both sides of the 6/7 choice     */

/*
 * `RxNextStateV29` on its own: no demodulation, no carrier, just the
 * transition.  `V29DET_RATE` is swept because it is what chooses between
 * status bytes 6 and 7 on the IDLE arm, and `dsplibs_debug_level` because the
 * printing arms are separate code and F150 is about exactly that.
 */
static void
run_next_one(unsigned seed, int state, unsigned short c000c, unsigned level,
	     long where)
{
	const short *alpha_a, *alpha_b;
	long wa, wb;
	int i;

	fixture(&fa, seed);
	fixture(&fb, seed);

	for (i = 0; i < 2; i++) {
		struct fix *f = i ? &fb : &fa;

		ref_FPM_AGC_init(RX_AGC_OF(f), &AGCv29_CFG, 1);
		put_short(f->det, V29DET_STATE, (short)state);
		put_short(f->det, V29DET_STATE_COUNT, 0x1234);
		put_short(f->det, V29DET_RATE, (short)c000c);
		put_ptr(f->det, V29DET_HANDLER, 0);
		put_int(f->det, V29DET_INT_0008, (int)0xa5a5a5a5);
		put_int(f->obj, V29_OBJ_STATUS, (int)0x00ffff00);
	}

	alpha_a = RX_AGC_OF(&fa)->cfg.alpha;
	alpha_b = RX_AGC_OF(&fb)->cfg.alpha;

	dsplibs_debug_level = level;
	ref_RxNextStateV29(fa.obj);
	RxNextStateV29(fb.obj);
	dsplibs_debug_level = 0;

	if (level > 1)
		st_debug++;
	if (state >= 0 && state < 8)
		st_arm[state]++;

	wa = st_which(get_ptr(fa.det, V29DET_HANDLER), 1);
	wb = st_which(get_ptr(fb.det, V29DET_HANDLER), 0);
	diff_eq_int("at %ld: the slot holds a known handler", wb >= 0, 1,
		    where);
	diff_eq_int("at %ld: the installed handler", wb, wa, where);
	if (wa >= 0 && wa < 7)
		st_installed[wa]++;

	diff_eq_int("at %ld: the next state", (long)get_short(fb.det,
							      V29DET_STATE),
		    (long)get_short(fa.det, V29DET_STATE), where);
	diff_eq_int("at %ld: the block budget",
		    (long)get_short(fb.det, V29DET_STATE_COUNT),
		    (long)get_short(fa.det, V29DET_STATE_COUNT), where);
	diff_eq_int("at %ld: the status word",
		    (long)get_int(fb.obj, V29_OBJ_STATUS),
		    (long)get_int(fa.obj, V29_OBJ_STATUS), where);
	diff_eq_int("at %ld: the AGC coefficient pointer moved by",
		    (long)(RX_AGC_OF(&fb)->cfg.alpha - alpha_b),
		    (long)(RX_AGC_OF(&fa)->cfg.alpha - alpha_a), where);

	if (RX_AGC_OF(&fa)->cfg.alpha != alpha_a)
		st_coeff_step++;
	if (state == V29RX_STATE_PROTOCOL)
		st_freeze++;
	if (state == V29RX_STATE_IDLE) {
		if ((get_int(fa.obj, V29_OBJ_STATUS) & 0xff)
		    == V29RX_STATUS_DATA_9600)
			st_done_a++;
		if ((get_int(fa.obj, V29_OBJ_STATUS) & 0xff)
		    == V29RX_STATUS_DATA_7200)
			st_done_b++;
	}

	put_ptr(fa.det, V29DET_HANDLER, 0);
	put_ptr(fb.det, V29DET_HANDLER, 0);
	compare_state(&fa, &fb, where);
}

#define ST_HANDLERS	4

static long st_ret_nonzero, st_advanced, st_error_arm, st_lowsnr;

static void
run_state_one(unsigned seed, const struct hdx_setup *u, int which, int vary,
	      long where)
{
	int blk, i;

	dem_signal(seed ^ 0x0b10cced, 2);
	for (i = 0; i < DEM_BUF; i++)
		hdx_in[i] = dem_in[i];

	hdx_build(&fa, seed, u, hdx_mrf_a, hdx_sre_a, hdx_det_a);
	hdx_build(&fb, seed, u, hdx_mrf_b, hdx_sre_b, hdx_det_b);

	for (i = 0; i < 2; i++) {
		struct fix *f = i ? &fb : &fa;

		put_short(f->det, V29DET_STATE_COUNT, 3);
		put_short(f->det, V29DET_RATE, (short)(seed & 1));
		put_ptr(f->det, V29DET_HANDLER, 0);
	}

	for (blk = 0; blk < HDX_BLOCKS; blk++) {
		unsigned short ca = HDX_COUNT, cb = HDX_COUNT;
		short ra, rb;
		long wa, wb;
		long id = where * 1000 + blk;

		if (vary) {
			dem_signal(seed ^ (0x0b10cced + (unsigned)blk),
				   (blk & 1) ? 0 : 2);
			for (i = 0; i < DEM_BUF; i++)
				hdx_in[i] = dem_in[i];
		}

		for (i = 0; i < HDX_OUT; i++)
			hdx_out_a[i] = hdx_out_b[i] = HDX_MARK;
		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];

		switch (which) {
		case 0:
			ra = ref_RxHdxStartV29(fa.obj, hdx_work, hdx_out_a, &ca);
			break;
		case 1:
			ra = ref_RxHdxIdleV29(fa.obj, hdx_work, hdx_out_a, &ca);
			break;
		case 2:
			ra = ref_RxHdxPrtcolV29(fa.obj, hdx_work, hdx_out_a,
						&ca);
			break;
		default:
			ra = ref_RxHdxEpochDetV29(fa.obj, hdx_work, hdx_out_a,
						  &ca);
			break;
		}

		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];

		switch (which) {
		case 0:
			rb = RxHdxStartV29(fb.obj, hdx_work, hdx_out_b, &cb);
			break;
		case 1:
			rb = RxHdxIdleV29(fb.obj, hdx_work, hdx_out_b, &cb);
			break;
		case 2:
			rb = RxHdxPrtcolV29(fb.obj, hdx_work, hdx_out_b, &cb);
			break;
		default:
			rb = RxHdxEpochDetV29(fb.obj, hdx_work, hdx_out_b, &cb);
			break;
		}

		diff_eq_int("at %ld: the handler returned", (long)rb, (long)ra,
			    id);
		diff_eq_int("at %ld: the count came back", (long)cb, (long)ca,
			    id);
		diff_eq_int("at %ld: the status word",
			    (long)get_int(fb.obj, V29_OBJ_STATUS),
			    (long)get_int(fa.obj, V29_OBJ_STATUS), id);
		diff_eq_int("at %ld: the state number",
			    (long)get_short(fb.det, V29DET_STATE),
			    (long)get_short(fa.det, V29DET_STATE), id);
		diff_eq_int("at %ld: the block budget",
			    (long)get_short(fb.det, V29DET_STATE_COUNT),
			    (long)get_short(fa.det, V29DET_STATE_COUNT), id);
		diff_eq_int("at %ld: first differing output byte",
			    blk_first_diff((unsigned char *)hdx_out_b,
					   (unsigned char *)hdx_out_a,
					   HDX_OUT * (int)sizeof(short), 0),
			    -1, id);

		wa = st_which(get_ptr(fa.det, V29DET_HANDLER), 1);
		wb = st_which(get_ptr(fb.det, V29DET_HANDLER), 0);
		diff_eq_int("at %ld: the slot holds a known handler", wb >= 0,
			    1, id);
		diff_eq_int("at %ld: the installed handler", wb, wa, id);
		if (wa >= 0 && wa < 7)
			st_installed[wa]++;

		diff_eq_int("at %ld: first differing detector byte",
			    blk_first_diff(fb.det, fa.det, DET_SIZE,
					   skip_det_state), -1, id);
		diff_eq_int("at %ld: first differing instance byte",
			    blk_first_diff(fb.obj, fa.obj, OBJ_SIZE, skip_obj),
			    -1, id);
		diff_eq_int("at %ld: first differing MRF byte",
			    blk_first_diff((unsigned char *)hdx_mrf_b,
					   (unsigned char *)hdx_mrf_a,
					   DEM_BUF * (int)sizeof(short), 0),
			    -1, id);
		diff_eq_int("at %ld: first differing SRE byte",
			    blk_first_diff((unsigned char *)hdx_sre_b,
					   (unsigned char *)hdx_sre_a,
					   DEM_BUF * (int)sizeof(short), 0),
			    -1, id);

		if (ra != 0)
			st_ret_nonzero++;
		if (get_short(fa.det, V29DET_STATE) != (short)V29RX_STATE_ERROR
		    && wa != 0)
			st_advanced++;
		if (get_short(fa.det, V29DET_STATE) == (short)V29RX_STATE_ERROR)
			st_error_arm++;
		if ((get_int(fa.obj, V29_OBJ_STATUS) & V29_STATUS_LOW_SNR) != 0)
			st_lowsnr++;

		put_ptr(fa.det, V29DET_HANDLER, 0);
		put_ptr(fb.det, V29DET_HANDLER, 0);
	}

	hdx_free(&fa);
	hdx_free(&fb);
}

static int
run_states(void)
{
	static const struct hdx_setup cases[] = {
		/* gate14 gate1c absent detgate carrier status mse */
		{ 0, 0, 1, 0, 1, 0, 0 },
		{ 1, 0, 0, 0, 1, -1, 0 },
		{ 0, 0, 0, 0, 0, 0, 0 },	/* no carrier: the error arm */
		{ 1, 0, 1, 0, 1, 0, 0x1000 },	/* IDLE's mse test passes    */
		{ 1, 0, 1, 0, 1, 0, 0x7000 }	/* ... and fails             */
	};
	int i, s;
	unsigned lv;

	dem_tables();
	dcd_cfgs();
	diff_begin("the V.29 receive state machine");

	/* Every state value the switch can see, both debug levels. */
	for (lv = 0; lv <= 2; lv += 2)
		for (s = 0; s < 8; s++)
			for (i = 0; i < 2; i++)
				run_next_one(0xca000000u + (unsigned)(s * 4 + i)
						+ lv,
					     s, (unsigned short)i, lv,
					     (long)(lv * 100 + s * 2 + i));

	for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++)
		for (s = 0; s < ST_HANDLERS; s++)
			run_state_one(0xcb000000u
				      + (unsigned)(i * ST_HANDLERS + s),
				      &cases[i], s, 0,
				      (long)(1000 + i * ST_HANDLERS + s));

	/* The loud/quiet alternation, for the arms a steady stimulus misses. */
	for (s = 0; s < ST_HANDLERS; s++)
		run_state_one(0xcc000000u + (unsigned)s, &cases[0], s, 1,
			      (long)(2000 + s));

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 17.  V29RX_create                                                      */
/*
 * THE COMPARISON IS "ZAP, THEN COMPARE EVERYTHING", NOT "SKIP WHAT I THOUGHT
 * OF", and the difference is which way it fails.  Every field the two sides
 * cannot agree about is a POINTER -- each side allocates its own blocks and
 * names its own copy of every table -- so both sides' pointer words are
 * overwritten with zero and then all 0x54 + 0x58 + 0x4f6c bytes are compared.
 *
 * A pointer this list forgets therefore shows up as a LOUD failure naming its
 * offset, where a skip list that forgets one silently stops checking a region.
 * F134's argument applied to the shape of the check rather than to its count.
 *
 * The zap list is derived, not guessed: `fpm_agc.h`, `fpm_mrf.h`, `fpm_sre.h`
 * and `fpm_fse.h` each say which of their members are pointers, and the
 * offsets below are those plus the block base each module sits at.
 *
 * FOUR ALLOCATION SHAPES ARE DRIVEN, because `V29RX_create` has two
 * independent `reset` flags and a third block with none of its own:
 *
 *   A  modem NULL                       both flags set
 *   B  modem, det and rx all present    both clear
 *   C  modem and rx present, det NULL   det flag set, handle flag clear
 *   D  modem and det present, rx NULL   D1174: rx allocated, flag CLEAR
 *
 * and each over four configurations: NULL, 7200, 9600 and a rate that is
 * neither -- which is the only input that reaches D1173's default-less arm.
 */
extern void *ref_V29RX_create(void *modem, const struct v29rx_cfg *params);

static const int cr_zap_obj[] = {
	V29_OBJ_EQ_OUT_I, V29_OBJ_EQ_OUT_Q, V29_OBJ_EQ_NOUT,
	V29_OBJ_EQ_ICOEFF, V29_OBJ_EQ_QCOEFF,
	V29_OBJ_DET, V29_OBJ_RX,
	-1
};

static const int cr_zap_det[] = {
	V29DET_MTD, V29DET_TONE, V29DET_HANDLER, V29DET_BUF,
	V29DET_V21_MTD, V29DET_V21_BUF,
	V29DET_V21_AGC + 0x0c, V29DET_V21_AGC + 0x10,	/* cfg.alpha, .beta  */
	-1
};

static const int cr_zap_rx[] = {
	V29RX_MRF + 0x04, V29RX_MRF + 0x0c,		/* cfg.coeff, .aux   */
	V29RX_MRF + 0x18,				/* history           */
	V29RX_AGC + 0x0c, V29RX_AGC + 0x10,		/* cfg.alpha, .beta  */
	V29RX_SRE + 0x10, V29RX_SRE + 0x14, V29RX_SRE + 0x18,
	V29RX_SRE + 0x1c, V29RX_SRE + 0x20, V29RX_SRE + 0x24,
	V29RX_SRE + 0x34,				/* the aux pointer   */
	V29RX_SRE + 0x50, V29RX_SRE + 0x54, V29RX_SRE + 0x58,
	V29RX_SRE + 0x74,
	V29RX_FSE + 0x04, V29RX_FSE + 0x08, V29RX_FSE + 0x14,
	V29RX_FSE + 0x24, V29RX_FSE + 0x28, V29RX_FSE + 0x2c,
	V29RX_FSE + 0x30, V29RX_FSE + 0x34,
	V29RX_FSE + 0x54, V29RX_FSE + 0x58, V29RX_FSE + 0x60,
	V29RX_FSE + 0x64, V29RX_FSE + 0x68,
	V29RX_BUF_MRF, V29RX_BUF_SRE,
	-1
};

static long cr_bytes;			/* bytes actually compared          */
static long cr_shape[4];		/* how often each allocation shape  */
static long cr_rate[3];			/* 7200, 9600, neither              */
static long cr_limit_unwritten;		/* D1173's arm was reached          */
static long cr_reset_split;		/* D1174's arm was reached          */

static void
cr_zap(unsigned char *p, const int *offs)
{
	int i;

	for (i = 0; offs[i] >= 0; i++)
		*(void **)(void *)(p + offs[i]) = 0;
}

/*
 * The MRF's and SRE's scratch buffers are compared through their own
 * pointers, before those are zapped -- so the zeroing loop is measured and
 * not merely assumed to have run.
 */
static long
cr_cmp_buf(void *a, void *b, int nshorts, long where, const char *what)
{
	int i;
	short *sa = (short *)a;
	short *sb = (short *)b;

	for (i = 0; i < nshorts; i++)
		if (sa[i] != sb[i])
			break;
	diff_eq_int(what, i == nshorts ? -1 : i, -1, where);
	return nshorts;
}

static void
run_create_one(unsigned seed, int shape, const struct v29rx_cfg *params,
	       long where)
{
	void *ma, *mb;
	void *da, *db, *ra, *rb;
	unsigned char *pa, *pb;
	int i;

	(void)seed;

	/*
	 * SHAPES 1..3 RE-ENTER AN INSTANCE THIS FUNCTION BUILT, not a
	 * pseudorandom block.  A `struct fix` filled by `fixture()` cannot be
	 * used here: `V29RX_create` hands the detection block's existing MTD
	 * pointer straight to `FPM_MTD_create` and re-initialises four FPM
	 * modules in place, so garbage in any of those is a wild pointer the
	 * constructor writes through.  Building a real instance first is the
	 * only way to reach the re-entrant path at all.
	 */
	if (shape == 0) {
		ma = ref_V29RX_create(0, params);
		mb = V29RX_create(0, params);
	} else {
		ma = ref_V29RX_create(0, 0);
		mb = V29RX_create(0, 0);

		if (shape == 2) {
			put_ptr((unsigned char *)ma, V29_OBJ_DET, 0);
			put_ptr((unsigned char *)mb, V29_OBJ_DET, 0);
		} else if (shape == 3) {
			put_ptr((unsigned char *)ma, V29_OBJ_RX, 0);
			put_ptr((unsigned char *)mb, V29_OBJ_RX, 0);
			cr_reset_split++;
		}

		ma = ref_V29RX_create(ma, params);
		mb = V29RX_create(mb, params);
	}
	cr_shape[shape]++;

	diff_eq_int("at %ld: the constructor returned non-null", mb != 0, 1,
		    where);
	diff_eq_int("at %ld: it returned a handle", mb != 0 && ma != 0, 1,
		    where);
	if (mb == 0 || ma == 0)
		return;

	da = get_ptr((unsigned char *)ma, V29_OBJ_DET);
	db = get_ptr((unsigned char *)mb, V29_OBJ_DET);
	ra = get_ptr((unsigned char *)ma, V29_OBJ_RX);
	rb = get_ptr((unsigned char *)mb, V29_OBJ_RX);
	diff_eq_int("at %ld: both blocks were laid down",
		    (db != 0) + (rb != 0), (da != 0) + (ra != 0), where);
	if (da == 0 || db == 0 || ra == 0 || rb == 0)
		return;

	/* The rate, and which of D1173's three arms this trial reached. */
	i = get_short((unsigned char *)da, V29DET_RATE);
	diff_eq_int("at %ld: the rate index",
		    (long)get_short((unsigned char *)db, V29DET_RATE), (long)i,
		    where);
	if (params == 0)
		cr_rate[1]++;
	else if (params->bit_rate == V29_BPS_7200)
		cr_rate[0]++;
	else if (params->bit_rate == V29_BPS_9600)
		cr_rate[1]++;
	else
		cr_rate[2]++;
	if (i != V29_RATE_7200 && i != V29_RATE_9600)
		cr_limit_unwritten++;

	/* The scratch buffers, through the pointers, before they are zapped. */
	cr_bytes += 2 * cr_cmp_buf(get_ptr((unsigned char *)ra, V29RX_BUF_MRF),
				   get_ptr((unsigned char *)rb, V29RX_BUF_MRF),
				   V29RX_BUF_ZEROED, where,
				   "at %ld: first differing MRF buffer entry");
	cr_bytes += 2 * cr_cmp_buf(get_ptr((unsigned char *)ra, V29RX_BUF_SRE),
				   get_ptr((unsigned char *)rb, V29RX_BUF_SRE),
				   V29RX_BUF_ZEROED, where,
				   "at %ld: first differing SRE buffer entry");

	pa = (unsigned char *)ma;
	pb = (unsigned char *)mb;
	cr_zap(pa, cr_zap_obj);
	cr_zap(pb, cr_zap_obj);
	cr_zap((unsigned char *)da, cr_zap_det);
	cr_zap((unsigned char *)db, cr_zap_det);
	cr_zap((unsigned char *)ra, cr_zap_rx);
	cr_zap((unsigned char *)rb, cr_zap_rx);

	diff_eq_int("at %ld: first differing handle byte",
		    blk_first_diff(pb, pa, V29_OBJ_SIZE, 0), -1, where);
	diff_eq_int("at %ld: first differing detection byte",
		    blk_first_diff((unsigned char *)db, (unsigned char *)da,
				   V29DET_SIZE, 0), -1, where);
	diff_eq_int("at %ld: first differing receiver byte",
		    blk_first_diff((unsigned char *)rb, (unsigned char *)ra,
				   V29RX_SIZE, 0), -1, where);
	cr_bytes += V29_OBJ_SIZE + V29DET_SIZE + V29RX_SIZE;
}

static int
run_create(void)
{
	static struct v29rx_cfg cfgs[3];
	int shape, c;

	diff_begin("V29RX_create");

	cfgs[0] = V29RX_CFG;
	cfgs[0].bit_rate = V29_BPS_7200;
	cfgs[1] = V29RX_CFG;
	cfgs[1].bit_rate = V29_BPS_9600;
	cfgs[2] = V29RX_CFG;
	cfgs[2].bit_rate = 4800;	/* neither: D1173's default arm */

	for (shape = 0; shape < 4; shape++)
		for (c = 0; c < 4; c++) {
			harness_alloc_reset();
			run_create_one(0xd0000000u
				       + (unsigned)(shape * 4 + c),
				       shape, c == 3 ? 0 : &cfgs[c],
				       (long)(shape * 10 + c));
		}

	harness_alloc_reset();
	return diff_end();
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;
	int d;

	harness_alloc_reset();

	/*
	 * run_agc_identity() is WITHDRAWN -- it SEGFAULTS under the period
	 * compiler (F9001).  Preserved intact on branch
	 * `withdrawn/v29-agc-identity` at 62cb0d84.
	 */
	rc |= run_accessors();
	rc |= run_tx_accessors();
	rc |= run_status();
	rc |= run_rxstatus();
	rc |= run_scramblers();
	rc |= run_modem();
	rc |= run_delete();
	rc |= run_quality();
	rc |= run_dcd();
	rc |= run_demod();
	rc |= run_txdelete();
	rc |= run_txmodem();
	rc |= run_moddata();
	rc |= run_hdx();
	rc |= run_slicers();
	rc |= run_states();
	rc |= run_create();

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

	diff_eq_int("RX quality was reported one (%ld)", rxst_q1 > 0, 1,
		    rxst_q1);
	diff_eq_int("RX quality was reported zero (%ld)", rxst_q0 > 0, 1,
		    rxst_q0);
	diff_eq_int("report bit 1 was set (%ld)", rxst_b1_1 > 0, 1, rxst_b1_1);
	diff_eq_int("report bit 1 was clear (%ld)", rxst_b1_0 > 0, 1,
		    rxst_b1_0);
	diff_eq_int("report bit 3 was set (%ld)", rxst_b3_1 > 0, 1, rxst_b3_1);
	diff_eq_int("report bit 3 was clear (%ld)", rxst_b3_0 > 0, 1,
		    rxst_b3_0);
	diff_eq_int("report bit 5 was set (%ld)", rxst_b5_1 > 0, 1, rxst_b5_1);
	diff_eq_int("report bit 5 was clear (%ld)", rxst_b5_0 > 0, 1,
		    rxst_b5_0);
	diff_eq_int("the RX bit rate separates from the protocol (%ld)",
		    rxst_bps_sep > 0, 1, rxst_bps_sep);
	diff_eq_int("+0x0c was left alone where +0x0e was zeroed (%ld)",
		    rxst_gap_sep > 0, 1, rxst_gap_sep);
	diff_eq_int("the report was overlaid on the instance (%ld)",
		    rxst_alias_sep > 0, 1, rxst_alias_sep);
	diff_eq_int("the RX null report was exercised (%ld)",
		    rxst_null_trials > 0, 1, rxst_null_trials);

	diff_eq_int("a scrambler moved bits or state (%ld)", scr_bits_sep > 0,
		    1, scr_bits_sep);
	diff_eq_int("a scrambler advanced its own state (%ld)",
		    scr_state_sep > 0, 1, scr_state_sep);
	diff_eq_int("each scrambler left the other's state alone (%ld)",
		    scr_wrong_off_sep > 0, 1, scr_wrong_off_sep);
	diff_eq_int("a zero count was exercised (%ld)", scr_zero_trials > 0, 1,
		    scr_zero_trials);
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

	diff_eq_int("V29TX_delete was driven (%ld)", txdel_trials > 0, 1,
		    txdel_trials);
	diff_eq_int("the blob freed every transmit block (%ld)",
		    txdel_freed_all > 0, 1, txdel_freed_all);

	diff_eq_int("V29TX_modem dispatched its slot (%ld)",
		    txm_slot_called > 0, 1, txm_slot_called);
	diff_eq_int("the transmit slot ran more than once (%ld)",
		    txm_multi_call > 0, 1, txm_multi_call);
	diff_eq_int("the FIFO arm was taken (%ld)", txm_fifo_arm > 0, 1,
		    txm_fifo_arm);
	diff_eq_int("the direct arm was taken (%ld)", txm_direct_arm > 0, 1,
		    txm_direct_arm);
	diff_eq_int("a short FIFO write set the flag and the byte (%ld)",
		    txm_flag_sep > 0, 1, txm_flag_sep);
	diff_eq_int("a full FIFO write left both alone (%ld)", txm_noflag > 0,
		    1, txm_noflag);
	diff_eq_int("`out` advanced between transmit calls (%ld)",
		    txm_out_moved > 0, 1, txm_out_moved);
	diff_eq_int("`in` did NOT advance between transmit calls (%ld)",
		    txm_in_static > 0, 1, txm_in_static);
	diff_eq_int("the transmit budget was driven negative (%ld)",
		    txm_budget_neg > 0, 1, txm_budget_neg);
	diff_eq_int("the transmit total passed 32767 (%ld)", txm_wrapped > 0,
		    1, txm_wrapped);

	diff_eq_int("ModDataV29 returned samples (%ld)", mod_ret_nonzero > 0,
		    1, mod_ret_nonzero);
	diff_eq_int("the shaper carried state between blocks (%ld)",
		    mod_state_carried > 0, 1, mod_state_carried);
	diff_eq_int("the ring cursor advanced (%ld)", mod_ring_moved > 0, 1,
		    mod_ring_moved);
	diff_eq_int("a zero count was driven through ModDataV29 (%ld)",
		    mod_zero_count > 0, 1, mod_zero_count);

	diff_eq_int("the DATA state took its bail arm (%ld)", hdx_bail > 0, 1,
		    hdx_bail);
	diff_eq_int("the DATA state demodulated (%ld)", hdx_proceed > 0, 1,
		    hdx_proceed);
	diff_eq_int("the DATA state reported units (%ld)", hdx_units > 0, 1,
		    hdx_units);
	diff_eq_int("the DATA state reported none (%ld)", hdx_zero_units > 0,
		    1, hdx_zero_units);
	diff_eq_int("the quality verdict forced zero units (%ld)",
		    hdx_quality_zero > 0, 1, hdx_quality_zero);
	diff_eq_int("the low-SNR bit was set (%ld)", hdx_snr_set > 0, 1,
		    hdx_snr_set);
	diff_eq_int("the low-SNR bit was left clear (%ld)", hdx_snr_clear > 0,
		    1, hdx_snr_clear);
	diff_eq_int("the SNR landed exactly ON the threshold (%ld)",
		    hdx_snr_at > 0, 1, hdx_snr_at);
	diff_eq_int("the SNR landed exactly one ABOVE it (%ld)",
		    hdx_snr_over > 0, 1, hdx_snr_over);
	diff_eq_int("the carrier bit came out set (%ld)", hdx_carrier_set > 0,
		    1, hdx_carrier_set);
	diff_eq_int("the carrier bit came out clear (%ld)",
		    hdx_carrier_clear > 0, 1, hdx_carrier_clear);
	diff_eq_int("the status byte was written (%ld)", hdx_status_byte > 0,
		    1, hdx_status_byte);
	diff_eq_int("both handlers zeroed the count (%ld)",
		    hdx_count_zeroed > 0, 1, hdx_count_zeroed);
	diff_eq_int("the ERROR state raised its flag (%ld)", hdx_err_flag > 0,
		    1, hdx_err_flag);
	diff_eq_int("the ERROR state demodulated anyway (%ld)",
		    hdx_err_moved > 0, 1, hdx_err_moved);

	/*
	 * The slicer chain's own denominators.  Each says a NAMED arm or a
	 * NAMED wrong reading was actually reached; a zero makes the
	 * corresponding check above decoration.  F134's argument.
	 */
	for (d = 1; d < (int)S_MAX; d++)
		diff_eq_int("slicer wrong reading %ld separates", sl_sep[d] > 0,
			    1, d);

	diff_eq_int("the epoch detector handed over (%ld)",
		    sl_handover[SL_EPOCH] > 0, 1, sl_handover[SL_EPOCH]);
	diff_eq_int("the training slicer handed over (%ld)",
		    sl_handover[SL_TRAIN] > 0, 1, sl_handover[SL_TRAIN]);
	diff_eq_int("the running slicer installed nothing (%ld)",
		    sl_handover[SL_DECIDE], 0, sl_handover[SL_DECIDE]);
	diff_eq_int("the epoch detector always returned 0xffff (%ld)",
		    sl_ret[SL_EPOCH], 0, sl_ret[SL_EPOCH]);
	diff_eq_int("the training slicer always returned 0xffff (%ld)",
		    sl_ret[SL_TRAIN], 0, sl_ret[SL_TRAIN]);
	diff_eq_int("the running slicer returned a symbol (%ld)",
		    sl_ret[SL_DECIDE] > 0, 1, sl_ret[SL_DECIDE]);
	diff_eq_int("a NEGATIVE subscript was used (%ld)", sl_neg_index > 0, 1,
		    sl_neg_index);
	diff_eq_int("a non-negative subscript was used (%ld)",
		    sl_pos_index > 0, 1, sl_pos_index);
	diff_eq_int("the epoch detector took its FAR arm (%ld)",
		    sl_far_arm > 0, 1, sl_far_arm);
	diff_eq_int("the epoch detector took its NEAR arm (%ld)",
		    sl_near_arm > 0, 1, sl_near_arm);
	diff_eq_int("the training LFSR gave an even symbol (%ld)",
		    sl_lfsr_even > 0, 1, sl_lfsr_even);
	diff_eq_int("the training LFSR gave an odd symbol (%ld)",
		    sl_lfsr_odd > 0, 1, sl_lfsr_odd);
	diff_eq_int("the symbol counter restarted at half scale (%ld)",
		    sl_wrap > 0, 1, sl_wrap);
	diff_eq_int("the amplitude bit reached the output (%ld)",
		    sl_amp_bit > 0, 1, sl_amp_bit);
	diff_eq_int("the outer ring won a decision (%ld)", sl_dec_ring1 > 0, 1,
		    sl_dec_ring1);

	/* The state machine's. */
	for (d = 0; d < 8; d++)
		diff_eq_int("state %ld was dispatched", st_arm[d] > 0, 1, d);
	/*
	 * `RxHdxStartV29` is index 1 and is NOT here: nothing in the object
	 * installs it but `V29RX_create`, which this binary does not drive.
	 * Saying so is the point of the loop's lower bound.
	 */
	for (d = 2; d < 7; d++)
		diff_eq_int("handler %ld was installed", st_installed[d] > 0, 1,
			    d);
	diff_eq_int("the AGC-freeze arm ran (%ld)", st_freeze > 0, 1,
		    st_freeze);
	diff_eq_int("the coefficient-step arm ran (%ld)", st_coeff_step > 0, 1,
		    st_coeff_step);
	diff_eq_int("a transition printed (%ld)", st_debug > 0, 1, st_debug);
	diff_eq_int("the IDLE arm reported status byte 6 (%ld)", st_done_a > 0,
		    1, st_done_a);
	diff_eq_int("the IDLE arm reported status byte 7 (%ld)", st_done_b > 0,
		    1, st_done_b);
	diff_eq_int("a handler reported a non-zero count (%ld)",
		    st_ret_nonzero > 0, 1, st_ret_nonzero);
	diff_eq_int("a handler advanced the machine (%ld)", st_advanced > 0, 1,
		    st_advanced);
	diff_eq_int("a handler dropped to ERROR (%ld)", st_error_arm > 0, 1,
		    st_error_arm);
	diff_eq_int("a handler raised the low-SNR bit (%ld)", st_lowsnr > 0, 1,
		    st_lowsnr);

	/*
	 * The constructor's.  `cr_bytes` is the DENOMINATOR of the byte
	 * comparison and is printed rather than merely tested, because a zap
	 * list that grew a bug could otherwise leave the whole check reading
	 * green over nothing.
	 */
	diff_eq_int("V29RX_create compared bytes (%ld)", cr_bytes > 100000, 1,
		    cr_bytes);
	for (d = 0; d < 4; d++)
		diff_eq_int("allocation shape %ld was driven", cr_shape[d] > 0,
			    1, d);
	diff_eq_int("the 7200 arm was taken (%ld)", cr_rate[0] > 0, 1,
		    cr_rate[0]);
	diff_eq_int("the 9600 arm was taken (%ld)", cr_rate[1] > 0, 1,
		    cr_rate[1]);
	diff_eq_int("a rate that is neither was driven (%ld)", cr_rate[2] > 0,
		    1, cr_rate[2]);
	diff_eq_int("D1174's split reset flags were driven (%ld)",
		    cr_reset_split > 0, 1, cr_reset_split);

	rc |= diff_end();

	return rc;
}
