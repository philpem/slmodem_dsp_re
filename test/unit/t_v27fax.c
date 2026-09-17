/*
 * t_v27fax.c -- differential test of the V.27ter fax receive primitives.
 *
 * Nine of the eleven are accessors or drivers: they pick a field or a
 * sub-object out of an unmodelled instance and either report it or hand it on.
 * A wrapper has almost no arithmetic of its own, so "it agrees with the blob
 * over a thousand random inputs" is nearly worthless -- the thousand inputs
 * are carried by the callee, which has its own suite.  What can be wrong is
 * WHICH field, and a wrong offset agrees with the right one over every input
 * unless the fixture makes the two differ.
 *
 * So the whole fixture is PSEUDORANDOM to the byte, no two adjacent words
 * agree, and every check is built around a NAMED WRONG READING whose
 * separating-trial count is asserted non-zero at the end (finding F3052's
 * rule).  Wrong readings are EVALUATED FOR REAL rather than argued: each one
 * is a variant of a model function, run on a scratch copy of the same inputs,
 * and a trial counts as separating when the variant's observable answer
 * differs from the blob's.  Variant 0 of every model is the reading this
 * reconstruction claims, and it is compared against the blob on every trial
 * too -- so the models are not a second opinion, they are a second
 * independent statement of the same claim.
 *
 * THE STATEFUL ONES ARE DRIVEN OVER MANY BLOCKS, not one.  Finding F8790 is
 * a swapped smoothing weight that no codegen check and no one-block fixture
 * can see: on the first block the accumulator is whatever the caller left, so
 * both orderings produce a number and neither is obviously wrong.
 * `QualityDetectV27` has exactly that shape -- a 0.9/0.1 first-order smoother
 * -- and `DataCarrierDetectV27` carries three separate pieces of state across
 * calls, so both are run as SEQUENCES and the whole sequence's state is
 * compared rather than the last call's return.
 *
 * WHAT THE FIXTURE HAS TO PLANT, and why more than the obvious.  D955 and
 * finding F8587:
 * a fixture must plant every field a callee uses as a SUBSCRIPT, not only
 * every field it dereferences, because a blob-against-blob dry run cannot
 * catch an unplanted subscript -- both sides read the same wild index and
 * agree.  `V27RX_decision` indexes two tables by three different fields
 * (`last`, `last & mask`, and the loop counter against `eight_phase`), so all
 * three are constrained to the tables' extent by construction and the tables
 * are sixteen entries where the object's are four or eight.
 *
 * HOW A PAIR IS RUN.  One working fixture, not two: the blob runs on it, the
 * result is snapshotted, the fixture is restored from a pristine copy and the
 * reconstruction runs on the same addresses.  Two separate fixtures would
 * differ in every planted POINTER and make `diff_eq_obj` useless on the very
 * bytes that matter.
 *
 * WHAT THIS TEST CANNOT SEE, stated rather than glossed: `V27RX_delete`'s
 * ORDER.  The harness's allocator records what is live, not the sequence of
 * `sysdep_free` calls, and nothing in `src/` may be changed to make it.  So
 * the order below is taken from the disassembly alone and what is CHECKED is
 * the SET of pointers released, the multiplicity, that no unknown pointer was
 * freed, and that five named wrong readings each change one of those.  See
 * finding F8871.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v27fax.h"
#include "dsplib/v27cfg.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sysdep.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/b103fp.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/sdmv27.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"

extern short ref_GetSNRV27(void *modem);
extern int ref_V27RX_status(void *rx, void *status);
extern int ref_EpochDetectV27(void *modem);
extern int ref_CarrierDetectV27(void *modem);
extern int ref_V27TX_status(const void *tx, void *status);
extern int ref_V27RX_modem(void *modem, short *in, short *out,
			   unsigned short *count);
extern void ref_V27RX_delete(void *modem);
extern void ref_V27TX_delete(void *modem);
extern unsigned short ref_ModDataV27(void *modem, const unsigned short *bits,
				     short *samples, unsigned short count);
extern void ref_SMC_init(struct fpm_smc *smc, const struct fpm_smc_cfg *cfg);
extern void ref_SMC_encoder(struct fpm_smc *smc, struct fpm_smc_ring *ring,
			    const unsigned short *data, unsigned short count);
extern void ref_FPM_PPS_init(struct fpm_pps *state,
			     const struct fpm_pps_cfg *cfg, int fresh);
extern void ref_FPM_PPS_free(struct fpm_pps *state);
extern unsigned short ref_FPM_PPS_filter(struct fpm_pps *state,
					 struct fpm_smc_ring *src, short *out,
					 unsigned short count);
extern const struct fpm_pps_cfg PPSv32_CFG;
extern unsigned short ref_V27RX_decision(struct fpm_fse *state, short *angle,
					 short *mag);
extern short ref_QualityDetectV27(void *modem);
extern short ref_DataCarrierDetectV27(void *modem, short *samples,
				      unsigned short count);

extern unsigned short ref_V27RX_eq_train(struct fpm_fse *state, short *angle,
					 short *mag);
extern unsigned short ref_DemodDataV27(void *modem, short *in,
				       unsigned short *bits,
				       unsigned short count);
extern void ref_ScrambleDataV27(void *modem, unsigned short *data, short n);
extern void ref_DescrambleDataV27(void *modem, unsigned short *data, short n);

/*
 * The blob's own module constructors, used to build the demodulator fixture on
 * BOTH sides -- D955's rule: a sub-object planted rather than constructed
 * leaves every field it uses as a SUBSCRIPT wild, and a blob-against-blob dry
 * run cannot catch that because both sides read the same wild index.
 */
extern void ref_FPM_AGC_init(struct fpm_agc *agc,
			     const struct fpm_agc_cfg *cfg, int fresh);
extern void ref_FPM_AGC_agc(struct fpm_agc *agc, short *samples,
			    unsigned short count);
extern void ref_FPM_MRF_init(struct fpm_mrf *state,
			     const struct fpm_mrf_cfg *cfg, int fresh);
extern void ref_FPM_MRF_free(struct fpm_mrf *state);
extern short ref_FPM_MRF_filter(struct fpm_mrf *state, const short *in,
				short *out, short count);
extern void ref_FPM_SRE_init(struct fpm_sre *sre,
			     const struct fpm_sre_cfg *cfg, int fresh);
extern void ref_FPM_SRE_free(struct fpm_sre *sre);
extern unsigned short ref_FPM_SRE_recover(struct fpm_sre *sre, const short *in,
					  short *out, short count);
extern void ref_FPM_FSE_init(struct fpm_fse *state,
			     const struct fpm_fse_cfg *cfg, int fresh);
extern void ref_FPM_FSE_free(struct fpm_fse *state);
extern unsigned short ref_FPM_FSE_receive(struct fpm_fse *state,
					  const short *in, unsigned short *out,
					  unsigned short count);
extern short ref_FPM_MTD_detect(struct fpm_mtd *state, const short *samples,
				short count);

/*
 * The V.32 module configurations, which are the ones with real tables behind
 * them: `FPM_MRF_CFG` and `FPM_SRE_CFG` are templates whose pointer fields are
 * zero in the object.  What they are TUNED for does not matter here -- the two
 * sides run the same filter over the same samples -- but they must be real.
 */
extern const struct fpm_mrf_cfg MRFv32_CFG;
extern const struct fpm_sre_cfg SREv32_CFG;

extern unsigned int ref_dsplibs_debug_level;

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x60
#define SH_SIZE		0x60
#define TX_SIZE		0xa0
#define RX_SIZE		0x4f60

#define NTBL		16	/* phase / pmap table entries, 4x the widest
				 * the object ever configures                */
#define NBUF		512
#define DCD_N		160	/* 20 ms at 8 kHz                          */
#define DCD_BLOCKS	24
#define MTD_TONES	2	/* MTDb103_COEF is two biquad sections     */

/*
 * The whole modem, in one contiguous block so that a snapshot is a memcpy and
 * every planted pointer survives it.
 */
struct v27_fixture {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	sh[SH_SIZE];
	short		angles[NTBL];
	short		pmap[NTBL];
	short		bufa[NBUF];
	short		bufb[NBUF];
	short		shbuf[NBUF];
	short		acc_a[MTD_TONES * 2];
	short		acc_b[MTD_TONES * 2];
	struct fpm_mtd	mtd_a;
	struct fpm_mtd	mtd_b;
	unsigned char	tx[TX_SIZE];
	unsigned char	rx[RX_SIZE];
	double		align;
};

static struct v27_fixture pristine, work, snap;

/* The transmitter status pair, kept apart so the aliasing sweep can overlap
 * them deliberately rather than by accident. */
#define TXST_SPAN	0x60
struct v27_txstatus {
	unsigned char	blk[2 * TXST_SPAN];
	double		align;
};
static struct v27_txstatus tx_pristine, tx_work, tx_snap, tx_model;

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
rng_fill(void *p, size_t n)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rng_next() >> 13);
}

/* --------------------------------------------------------------------- */
/* Fixture accessors -- the same offsets src/fax/v27.c uses, spelled here   */
/* independently so a typo in one does not cancel a typo in the other.      */

#define FX(f, off)	((unsigned char *)(f) + (off))
#define FXP(f, off)	(*(void **)(void *)FX((f), (off)))
#define FXS(f, off)	(*(short *)(void *)FX((f), (off)))
#define FXU(f, off)	(*(unsigned short *)(void *)FX((f), (off)))
#define FXI(f, off)	(*(int *)(void *)FX((f), (off)))

#define DEC(f)		FX((f)->rx, V27RX_DEC)

/*
 * Build a fixture: everything pseudorandom, then the pointers wired to
 * `&work` -- never to the copy being filled -- so that `pristine`, `work` and
 * `snap` all name the same addresses.
 */
/*
 * Which phase table the next fixture gets.
 *
 *   0  the object's own shape: `V27RX_DEC_LAST_PHASE_4800` is 0, 0x1000 ...
 *      0x7000 and the 2400 table is every other entry of it, so a full
 *      revolution is 0x8000 and every angle is non-negative.  Jittered here,
 *      because a table of exact multiples makes a wrong index land on a
 *      plausible answer.
 *   2  exact multiples of 0x1000 with no jitter, which is
 *      `V27RX_DEC_LAST_PHASE_4800` itself.  A jittered table has no two
 *      entries equidistant from anything, so the tie-break is unreachable
 *      under shape 0 and reachable here at every half-step.
 *   1  spread across the whole signed range, WHICH THE OBJECT NEVER
 *      CONFIGURES.  It is here because two arms of the function are dead
 *      under shape 0: with a non-negative table the folded difference cannot
 *      exceed 0x8000, so `diff > 0x8000` and the branch of the initial
 *      distance that goes with it are unreachable.  Finding F8869.
 */
static int fx_table_style;

static void
fx_build(struct v27_fixture *f, unsigned seed)
{
	int i;

	rng_seed(seed);
	rng_fill(f, sizeof(*f) - sizeof(double));

	/* Distinct, and no two adjacent entries equal. */
	for (i = 0; i < NTBL; i++) {
		if (fx_table_style == 0)
			f->angles[i] = (short)(i * 0x0800
					       + (int)(rng_next() % 97));
		else if (fx_table_style == 2)
			f->angles[i] = (short)(i * 0x1000);
		else
			f->angles[i] = (short)(i * 0x1000 - 0x8000
					       + (int)(rng_next() % 97));
		f->pmap[i] = (short)(0x0140 + i * 0x0111);
	}

	FXP(f->obj, V27_OBJ_SHARED) = work.sh;
	FXP(f->obj, V27_OBJ_RX) = work.rx;

	FXP(f->rx, V27RX_BUF_A) = work.bufa;
	FXP(f->rx, V27RX_BUF_B) = work.bufb;

	FXP(DEC(f), V27DEC_ANGLES) = work.angles;
	FXP(DEC(f), V27DEC_PMAP) = work.pmap;
	FXU(DEC(f), V27DEC_PHASE_MASK) = 3;
	FXS(DEC(f), V27DEC_LAST) = 0;
	FXI(DEC(f), V27DEC_EIGHT_PHASE) = 0;
	FXU(DEC(f), V27DEC_SYM_COUNT) = 0;

	/* The equaliser's configuration owner, which is what the slicer reads. */
	((struct fpm_fse *)(void *)FX(f->rx, V27RX_FSE))->cfg.owner =
			FX(work.rx, V27RX_DEC);
}

static struct fpm_agc *
fx_agc(struct v27_fixture *f)
{
	return (struct fpm_agc *)(void *)FX(f->rx, V27RX_AGC);
}

static struct fpm_sre *
fx_sre(struct v27_fixture *f)
{
	return (struct fpm_sre *)(void *)FX(f->rx, V27RX_SRE);
}

static struct fpm_fse *
fx_fse(struct v27_fixture *f)
{
	return (struct fpm_fse *)(void *)FX(f->rx, V27RX_FSE);
}

/* --------------------------------------------------------------------- */
/* 1.  GetSNRV27                                                          */

static long snr_indep;		/* trials proving it reads no field       */

static int
run_snr(void)
{
	static const unsigned seeds[] = { 0x11112222u, 0xfeedfaceu,
					  0x00000001u, 0x7fffffffu };
	short first_ref = 0;
	unsigned s;

	diff_begin("GetSNRV27");
	for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
		short got, want;

		fx_build(&pristine, seeds[s]);
		work = pristine;
		want = ref_GetSNRV27(work.obj);
		diff_eq_int("GetSNRV27 left the instance alone (seed %ld)",
			    memcmp(&work, &pristine, sizeof work) == 0, 1,
			    (long)seeds[s]);

		work = pristine;
		got = GetSNRV27(work.obj);
		diff_eq_int("GetSNRV27 (seed %ld)", got, want,
			    (long)seeds[s]);
		diff_eq_obj("GetSNRV27 instance", struct v27_fixture, &work,
			    &pristine, (long)s);

		/*
		 * WRONG READING: it returns something out of the instance.
		 * Every fixture differs from every other in every byte that
		 * is not a planted pointer, so a blob that answered the same
		 * for two of them is reading none of them.
		 */
		if (s == 0)
			first_ref = want;
		else if (want == first_ref)
			snr_indep++;
	}
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 2.  V27RX_status                                                       */

static long rxst_arg_sep;	/* trials where "arg1 decides" differs    */
static long rxst_bool_sep;	/* trials where "return the pointer" does */

static int
run_rxstatus(void)
{
	void *cands[3];
	int i, j;

	fx_build(&pristine, 0x5a5a1234u);
	work = pristine;

	cands[0] = 0;
	cands[1] = work.obj;
	cands[2] = work.rx;

	diff_begin("V27RX_status");
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			int got, want;

			want = ref_V27RX_status(cands[i], cands[j]);
			got = V27RX_status(cands[i], cands[j]);
			diff_eq_int("V27RX_status(%ld)", got, want,
				    (long)(i * 3 + j));

			/* WRONG READING: the FIRST argument decides. */
			if ((cands[i] != 0) != want)
				rxst_arg_sep++;

			/*
			 * WRONG READING: the pointer is returned rather than
			 * reduced to 0 or 1.  Every non-NULL candidate here is
			 * a static address, so it is never 1.
			 */
			if (cands[j] != 0 && (long)(size_t)cands[j] != want)
				rxst_bool_sep++;
		}
	}
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 3.  EpochDetectV27  and  4.  CarrierDetectV27                          */

static long epoch_off_sep;	/* a neighbouring flag would differ       */
static long epoch_bool_sep;	/* the raw value would differ from 0/1    */
static long carr_or_sep;	/* OR rather than AND                     */
static long carr_bool_sep;	/* reduced to 0/1                         */
static long carr_off_sep;	/* a neighbouring field                    */

static int
run_flags(void)
{
	static const int vals[] = { 0, 1, -1, 2, 0x10000, 0x7fffffff,
				    (int)0x80000000, 0x0f0f0f0f, 0x00ff00ff };
	unsigned i, j;

	fx_build(&pristine, 0x0badc0deu);

	diff_begin("EpochDetectV27");
	for (i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
		for (j = 0; j < sizeof(vals) / sizeof(vals[0]); j++) {
			int got, want;

			work = pristine;
			fx_fse(&work)->lms_force = vals[i];
			fx_fse(&work)->pll_on = vals[j];
			fx_fse(&work)->tilt_on = ~vals[j];
			pristine = work;

			want = ref_EpochDetectV27(work.obj);
			snap = work;
			work = pristine;
			got = EpochDetectV27(work.obj);
			diff_eq_int("EpochDetectV27(%ld)", got, want,
				    (long)(i * 16 + j));
			diff_eq_obj("EpochDetectV27 state",
				    struct v27_fixture, &work, &snap,
				    (long)(i * 16 + j));

			/* WRONG READING: fpm_fse::pll_on, one field along. */
			if ((vals[j] != 0) != want)
				epoch_off_sep++;
			/* WRONG READING: the raw field, not reduced. */
			if (vals[i] != want)
				epoch_bool_sep++;
		}
	}
	if (diff_end() != 0)
		return 1;

	diff_begin("CarrierDetectV27");
	for (i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
		for (j = 0; j < sizeof(vals) / sizeof(vals[0]); j++) {
			int got, want;

			work = pristine;
			fx_agc(&work)->signal = vals[i];
			fx_sre(&work)->active = vals[j];
			fx_sre(&work)->acquiring = ~vals[j];
			fx_agc(&work)->freeze = ~vals[i];
			pristine = work;

			want = ref_CarrierDetectV27(work.obj);
			snap = work;
			work = pristine;
			got = CarrierDetectV27(work.obj);
			diff_eq_int("CarrierDetectV27(%ld)", got, want,
				    (long)(i * 16 + j));
			diff_eq_obj("CarrierDetectV27 state",
				    struct v27_fixture, &work, &snap,
				    (long)(i * 16 + j));

			if ((vals[i] | vals[j]) != want)
				carr_or_sep++;
			if (((vals[i] & vals[j]) != 0) != want)
				carr_bool_sep++;
			if ((~vals[i] & vals[j]) != want)
				carr_off_sep++;
		}
	}
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 5.  V27TX_status                                                       */
/*
 * Variants, all evaluated for real against the blob's own output:
 *   0  the reading this reconstruction claims
 *   1  +0x14 cleared of bits 0 and 1, which is what V.17, V.21 and V.29 do
 *   2  the byte at +0x15 left alone
 *   3  the int at +0x18 not copied
 *   4  +0x10 taking the source's +0x00 rather than its +0x02
 *   5  the FIRST store to +0x14 omitted.  MEASURED TO BE DEAD -- see the
 *      assertion at the bottom of this file and finding F8867 -- and kept in
 *      `src/` anyway, because the object makes it and the compiler had no
 *      choice but to emit it
 *   6  the source's +0x02 read once and reused
 */
#define TXST_VARIANTS	7

static long txst_sep[TXST_VARIANTS];
static long txst_alias_trials;
static long txst_ff_trials;	/* the flags byte started all ones        */

/*
 * Force the three bytes the flags arithmetic reads, or -1 to leave them as
 * the random fill left them.
 *
 * A RANDOM FLAGS BYTE IS NOT ENOUGH FOR THE ONE THING THIS FUNCTION DOES
 * DIFFERENTLY FROM ITS THREE SIBLINGS.  V.27ter's answer is
 * `1 | (tx->flags & 4)` where theirs is `tx->flags & 4`, and a reading that
 * MERGED the two stores -- keeping the destination's other bits instead of
 * discarding them -- agrees with both on a byte whose bit 0 was already set.
 * So the sweep is pinned at 0x00 and 0xff explicitly rather than left to
 * come out of a generator.
 */
static int txst_f0 = -1, txst_f1 = -1, txst_txf = -1;

static int
txst_model(int variant, unsigned char *st, const unsigned char *tx)
{
	unsigned char flags;
	unsigned short first2;

	if (st == 0)
		return 0;

	FXU(st, 0x00) = FXU(tx, 0x00);
	first2 = FXU(tx, 0x02);
	FXU(st, 0x02) = first2;
	FXU(st, 0x04) = 0;
	FXU(st, 0x06) = 0;
	FXU(st, 0x08) = 0;
	FXU(st, 0x0a) = 0;
	FXU(st, 0x0c) = 0;
	FXU(st, 0x10) = variant == 4 ? FXU(tx, 0x00)
		      : variant == 6 ? first2
				     : FXU(tx, 0x02);
	FXU(st, 0x12) = 0;

	if (variant == 1) {
		flags = (unsigned char)(*FX(st, 0x14) & (unsigned char)~0x03);
		*FX(st, 0x14) = flags;
	} else {
		flags = (unsigned char)(*FX(st, 0x14) | 0x01);
		if (variant != 5)
			*FX(st, 0x14) = (unsigned char)
					(flags & (unsigned char)~0x02);
	}
	if (variant != 2)
		*FX(st, 0x15) &= (unsigned char)~0x01;
	*FX(st, 0x14) = (unsigned char)((flags & 0x01)
					| (*FX(tx, 0x10) & 0x04));

	if (variant != 3)
		FXI(st, 0x18) = FXI(tx, 0x18);

	return 1;
}

static void
run_txstatus_one(int tx_off, int st_off, unsigned seed, long tag)
{
	unsigned char *tx, *st;
	int got, want, v;

	rng_seed(seed);
	rng_fill(tx_pristine.blk, sizeof tx_pristine.blk);
	if (st_off >= 0 && txst_f0 >= 0)
		tx_pristine.blk[st_off + 0x14] = (unsigned char)txst_f0;
	if (st_off >= 0 && txst_f1 >= 0)
		tx_pristine.blk[st_off + 0x15] = (unsigned char)txst_f1;
	if (tx_off >= 0 && txst_txf >= 0)
		tx_pristine.blk[tx_off + 0x10] = (unsigned char)txst_txf;

	tx_work = tx_pristine;
	tx = tx_off < 0 ? 0 : tx_work.blk + tx_off;
	st = st_off < 0 ? 0 : tx_work.blk + st_off;

	want = ref_V27TX_status(tx, st);
	tx_snap = tx_work;

	tx_work = tx_pristine;
	tx = tx_off < 0 ? 0 : tx_work.blk + tx_off;
	st = st_off < 0 ? 0 : tx_work.blk + st_off;
	got = V27TX_status(tx, st);

	diff_eq_int("V27TX_status(%ld)", got, want, tag);
	diff_eq_obj("V27TX_status block", struct v27_txstatus, &tx_work,
		    &tx_snap, tag);

	for (v = 0; v < TXST_VARIANTS; v++) {
		int rc;

		tx_model = tx_pristine;
		tx = tx_off < 0 ? 0 : tx_model.blk + tx_off;
		st = st_off < 0 ? 0 : tx_model.blk + st_off;
		rc = txst_model(v, st, tx);

		if (v == 0) {
			diff_eq_int("V27TX_status model rc (%ld)", rc, want,
				    tag);
			diff_eq_obj("V27TX_status model", struct v27_txstatus,
				    &tx_model, &tx_snap, tag);
		} else if (rc != want
			   || memcmp(&tx_model, &tx_snap,
				     sizeof tx_model) != 0) {
			txst_sep[v]++;
		}
	}
}

static int
run_txstatus(void)
{
	static const unsigned seeds[] = { 0x13572468u, 0x2468ace0u,
					  0xdeadbeefu };
	unsigned s;
	int d;

	diff_begin("V27TX_status");

	/* No destination at all, and no source either. */
	for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
		run_txstatus_one(0, -1, seeds[s], (long)s);
		run_txstatus_one(-1, -1, seeds[s], (long)(100 + s));
	}

	/* Disjoint blocks. */
	for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++)
		run_txstatus_one(0, TXST_SPAN, seeds[s], (long)(200 + s));

	/*
	 * OVERLAPPING, which is what makes the dead-looking store to +0x14 and
	 * the second read of the source's +0x02 observable at all.  Every
	 * displacement from -0x20 to +0x20 in fours, so tx + 0x10 lands on
	 * st + 0x14 (displacement +4) among others.
	 */
	for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
		for (d = -0x20; d <= 0x20; d += 4) {
			run_txstatus_one(TXST_SPAN / 2 + d, TXST_SPAN / 2,
					 seeds[s], (long)(300 + d));
			txst_alias_trials++;
		}
	}

	/*
	 * And the flags byte pinned, both ways round, on disjoint blocks.
	 * The assertions below are the deviation D1033 states, written as a
	 * fact about the BLOB rather than about the reconstruction: bit 0 of
	 * the destination comes out SET whatever it went in as, and bit 2
	 * follows the transmitter handle's own bit 2.
	 */
	for (d = 0; d < 4; d++) {
		unsigned char *st;
		unsigned char got;

		txst_f0 = (d & 1) ? 0xff : 0x00;
		txst_f1 = (d & 1) ? 0xff : 0x00;
		txst_txf = (d & 2) ? 0xff : 0x00;

		run_txstatus_one(0, TXST_SPAN, 0x0badf00du + (unsigned)d,
				 (long)(400 + d));
		txst_ff_trials++;

		st = tx_snap.blk + TXST_SPAN;
		got = st[0x14];
		diff_eq_int("the blob SET bit 0 of the flags byte (%ld)",
			    got & 0x01, 1, (long)d);
		diff_eq_int("the blob took bit 2 from the handle (%ld)",
			    got & 0x04, (txst_txf & 0x04), (long)d);
		diff_eq_int("the blob kept nothing else (%ld)",
			    got & ~0x05, 0, (long)d);
		diff_eq_int("the blob cleared bit 0 of the second byte (%ld)",
			    st[0x15] & 0x01, 0, (long)d);
	}
	txst_f0 = txst_f1 = txst_txf = -1;

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 6.  V27RX_decision                                                     */
/*
 * Variants:
 *   0  the reading this reconstruction claims
 *   1  four phases and eight the other way round
 *   2  the negative fold (diff += 0x8000) omitted
 *   3  the positive fold taken with >= rather than >
 *   4  the distance kept as an int rather than narrowed to short
 *   5  the tie-break <= rather than <
 *   6  the reference replaced rather than advanced: last = bi & mask
 *   7  the return taken from pmap[last] rather than pmap[bi]
 *   8  *angle taken from angles[bi] rather than angles[last]
 *   9  the symbol counter incremented without the 0x8000 -> 0x4000 restart
 *  10  the initial `best` taken as a plain 0x8000 rather than the folded
 *      distance to half a revolution
 */
#define DEC_VARIANTS	11

static long dec_sep[DEC_VARIANTS];
static long dec_wrap_trials;	/* the counter's restart was reached      */
static long dec_trunc_trials;	/* a distance wrapped through 16 bits     */
static long dec_wide_trials;	/* the folded difference exceeded 0x8000  */
static long dec_tie_trials;	/* two phases equidistant from the input  */

static unsigned short
dec_model(int variant, unsigned char *dec, const short *tbl, const short *pmap,
	  short *angle, short *mag)
{
	short n = FXI(dec, V27DEC_EIGHT_PHASE) ? 8 : 4;
	unsigned short count;
	short best, bi, k;
	int diff;

	if (variant == 1)
		n = (short)(FXI(dec, V27DEC_EIGHT_PHASE) ? 4 : 8);

	diff = *angle - tbl[FXS(dec, V27DEC_LAST)];

	count = (unsigned short)(FXU(dec, V27DEC_SYM_COUNT) + 1);
	if (variant != 9 && count == 0x8000)
		FXU(dec, V27DEC_SYM_COUNT) = 0x4000;
	else
		FXU(dec, V27DEC_SYM_COUNT) = count;

	if (variant != 2 && diff < 0)
		diff += 0x8000;
	if (variant == 3 ? diff >= 0x8000 : diff > 0x8000)
		diff -= 0x8000;

	if (variant == 10)
		best = (short)0x8000;
	else
		best = diff >= 0x8000 ? (short)(diff - 0x8000)
				      : (short)(0x8000 - diff);
	bi = 0;

	for (k = 0; k < n; k = (short)(k + 1)) {
		int wide = diff >= tbl[k] ? diff - tbl[k] : tbl[k] - diff;
		short d = (short)wide;

		if (variant == 4) {
			if (wide < best) {
				bi = k;
				best = (short)wide;
			}
			continue;
		}
		if (variant == 5 ? d <= best : d < best) {
			bi = k;
			best = d;
		}
	}

	if (variant == 6)
		FXS(dec, V27DEC_LAST) = (short)(bi
					& FXU(dec, V27DEC_PHASE_MASK));
	else
		FXS(dec, V27DEC_LAST) = (short)((FXS(dec, V27DEC_LAST) + bi)
					& FXU(dec, V27DEC_PHASE_MASK));

	*mag = V27DEC_MAG;
	*angle = variant == 8 ? tbl[bi] : tbl[FXS(dec, V27DEC_LAST)];

	return (unsigned short)(variant == 7 ? pmap[FXS(dec, V27DEC_LAST)]
					     : pmap[bi]);
}

static void
run_dec_one(int eight, unsigned short mask, short last, short in_angle,
	    unsigned short count, unsigned seed, long tag)
{
	short a_ours, m_ours, a_ref, m_ref, a_mod, m_mod;
	unsigned short r_ours, r_ref, r_mod;
	static struct v27_fixture model;
	short best_far;
	int v, k, n;

	fx_build(&pristine, seed);
	FXI(DEC(&pristine), V27DEC_EIGHT_PHASE) = eight;
	FXU(DEC(&pristine), V27DEC_PHASE_MASK) = mask;
	FXS(DEC(&pristine), V27DEC_LAST) = last;
	FXU(DEC(&pristine), V27DEC_SYM_COUNT) = count;

	work = pristine;
	a_ref = in_angle;
	m_ref = (short)0x1234;
	r_ref = ref_V27RX_decision(fx_fse(&work), &a_ref, &m_ref);
	snap = work;

	work = pristine;
	a_ours = in_angle;
	m_ours = (short)0x1234;
	r_ours = V27RX_decision(fx_fse(&work), &a_ours, &m_ours);

	diff_eq_int("V27RX_decision return (%ld)", r_ours, r_ref, tag);
	diff_eq_int("V27RX_decision angle (%ld)", a_ours, a_ref, tag);
	diff_eq_int("V27RX_decision mag (%ld)", m_ours, m_ref, tag);
	diff_eq_obj("V27RX_decision state", struct v27_fixture, &work, &snap,
		    tag);

	if (count == 0x7fff)
		dec_wrap_trials++;

	/*
	 * Did any distance in this trial wrap through sixteen bits?  That is
	 * what makes the `short` narrowing a measurement rather than a note,
	 * and it is counted from the TRIAL'S OWN table -- not from a
	 * constant -- so it reports what the run reached (finding F134).
	 */
	n = eight ? 8 : 4;
	best_far = 0;
	{
		long diff = (long)in_angle - pristine.angles[last];

		if (diff < 0)
			diff += 0x8000;
		if (diff > 0x8000) {
			diff -= 0x8000;
			dec_wide_trials++;
		}
		for (k = 0; k < n; k++) {
			long wide = diff >= pristine.angles[k]
				  ? diff - pristine.angles[k]
				  : pristine.angles[k] - diff;

			if (wide > 32767)
				best_far = 1;
		}
	}
	if (best_far)
		dec_trunc_trials++;

	for (v = 0; v < DEC_VARIANTS; v++) {
		model = pristine;
		a_mod = in_angle;
		m_mod = (short)0x1234;
		r_mod = dec_model(v, DEC(&model), model.angles, model.pmap,
				  &a_mod, &m_mod);

		if (v == 0) {
			diff_eq_int("decision model return (%ld)", r_mod,
				    r_ref, tag);
			diff_eq_int("decision model angle (%ld)", a_mod, a_ref,
				    tag);
			diff_eq_int("decision model mag (%ld)", m_mod, m_ref,
				    tag);
			diff_eq_int("decision model last (%ld)",
				    FXS(DEC(&model), V27DEC_LAST),
				    FXS(DEC(&snap), V27DEC_LAST), tag);
			diff_eq_int("decision model count (%ld)",
				    FXU(DEC(&model), V27DEC_SYM_COUNT),
				    FXU(DEC(&snap), V27DEC_SYM_COUNT), tag);
		} else if (r_mod != r_ref || a_mod != a_ref || m_mod != m_ref
			   || FXS(DEC(&model), V27DEC_LAST)
			      != FXS(DEC(&snap), V27DEC_LAST)
			   || FXU(DEC(&model), V27DEC_SYM_COUNT)
			      != FXU(DEC(&snap), V27DEC_SYM_COUNT)) {
			dec_sep[v]++;
		}
	}
}

static int
run_decision(void)
{
	static const short angles[] = {
		0, 1, 0x0400, 0x0800, 0x1000, 0x2000, 0x3fff, 0x4000,
		0x5000, 0x7fff, -1, -0x1000, -0x4000, (short)0x8000
	};
	static const unsigned short counts[] = { 0, 1, 0x7fff, 0x3fff,
						 0xfffe, 0xffff };
	static const unsigned seeds[] = { 0x2b2b2b2bu, 0x600dfeedu };
	unsigned a, c, s;
	int eight;
	short last;
	long tag = 0;

	diff_begin("V27RX_decision");
	fx_table_style = 0;
	for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
		for (eight = 0; eight <= 1; eight++) {
			unsigned short mask = eight ? 7 : 3;

			for (last = 0; last < (eight ? 8 : 4); last++) {
				for (a = 0; a < sizeof(angles)
						/ sizeof(angles[0]); a++) {
					for (c = 0; c < sizeof(counts)
						    / sizeof(counts[0]); c++) {
						run_dec_one(eight, mask, last,
							    angles[a],
							    counts[c],
							    seeds[s], tag++);
					}
				}
			}
		}
	}

	/*
	 * And a mask WIDER than the table the object configures, so that the
	 * reference index can leave the first four entries.  The tables here
	 * are sixteen long for exactly this: it is the only way to separate
	 * "advance the reference" from "replace it" when the mask would
	 * otherwise fold both onto the same index.
	 */
	for (last = 0; last < NTBL; last++)
		run_dec_one(1, 0x0f, last, (short)(last * 0x0700 + 0x37),
			    0, 0x0f1e2d3cu, 900 + last);

	/*
	 * Shape 1, which reaches the two arms shape 0 cannot.  Same sweep,
	 * plus the one input that makes the folded difference EXACTLY a full
	 * revolution -- the only value on which `>` and `>=` differ, and one no
	 * stimulus sweep lands on by accident.
	 */
	fx_table_style = 1;
	for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
		for (eight = 0; eight <= 1; eight++) {
			unsigned short mask = eight ? 7 : 3;

			for (last = 0; last < (eight ? 8 : 4); last++)
				for (a = 0; a < sizeof(angles)
						/ sizeof(angles[0]); a++)
					run_dec_one(eight, mask, last,
						    angles[a], 0, seeds[s],
						    2000 + tag++);
		}
	}
	{
		short a0;

		fx_build(&pristine, 0x5eed5eedu);
		a0 = (short)(pristine.angles[0] + 0x8000);
		run_dec_one(0, 3, 0, a0, 0, 0x5eed5eedu, 3000);
		run_dec_one(1, 7, 0, a0, 0, 0x5eed5eedu, 3001);
	}

	/*
	 * Shape 2 and the half-steps: the object's own table, and the inputs
	 * that fall exactly between two of its phases.  `<` keeps the first
	 * of the two and `<=` takes the second, and nothing else in this file
	 * can tell them apart.
	 */
	fx_table_style = 2;
	for (eight = 0; eight <= 1; eight++) {
		for (last = 0; last < (eight ? 8 : 4); last++) {
			int h;

			for (h = 0; h < 8; h++) {
				short a0 = (short)(last * 0x1000 + 0x0800
						   + h * 0x1000);

				run_dec_one(eight, (unsigned short)
						   (eight ? 7 : 3), last, a0,
					    0, 0x11223344u,
					    4000 + last * 8 + h);
				dec_tie_trials++;
			}
		}
	}
	fx_table_style = 0;

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 7.  QualityDetectV27                                                   */
/*
 * Variants:
 *   0  the reading this reconstruction claims
 *   1  the two smoothing weights swapped (finding F8790's shape)
 *   2  the +0x4000 rounding dropped from both terms
 *   3  the two terms summed before the shift rather than after,
 *      which is the same arithmetic with one rounding step instead of two
 *   4  the smoother's bound 0x32 rather than 0x31
 *   5  the latch comparison < rather than <=
 *   6  the count-zero arm folded into the smoother
 *   7  the verdict reduced to 1 rather than passed through
 */
#define QD_VARIANTS	8
#define QD_BLOCKS	64

static long qd_sep[QD_VARIANTS];
static long qd_latch_trials;	/* the 0x32 arm was entered               */
static long qd_smooth_trials;	/* the smoother ran at least twice        */
static long qd_quiet_trials;	/* the no-carrier arm was entered         */
static long qd_equal_trials;	/* the latch met acc == limit exactly     */

static short
qd_model(int variant, unsigned char *rx, int signal, int active, short mse)
{
	short verdict;
	unsigned short n;

	verdict = (short)(signal & active);
	if (verdict == 0)
		verdict = 2;
	else if (variant == 7)
		verdict = 1;

	n = FXU(rx, V27RX_Q_COUNT);
	if (n == 0 && variant != 6) {
		FXS(rx, V27RX_Q_ACC) = mse;
		FXU(rx, V27RX_Q_COUNT) = 1;
	} else if ((short)n <= (variant == 4 ? 0x32 : 0x31)) {
		long wa = variant == 1 ? 0xccd : 0x7333;
		long wb = variant == 1 ? 0x7333 : 0xccd;
		long r = variant == 2 ? 0 : 0x4000;
		long acc = FXS(rx, V27RX_Q_ACC);
		long t;

		if (variant == 3)
			t = (acc * wa + mse * wb + r) >> 15;
		else
			t = ((acc * wa + r) >> 15) + ((mse * wb + r) >> 15);
		FXS(rx, V27RX_Q_ACC) = (short)t;
		FXU(rx, V27RX_Q_COUNT) = (unsigned short)(n + 1);
	} else if ((short)n == 0x32) {
		if (variant == 5 ? FXS(rx, V27RX_Q_ACC)
				   < FXS(rx, V27RX_Q_LIMIT)
				 : FXS(rx, V27RX_Q_ACC)
				   <= FXS(rx, V27RX_Q_LIMIT))
			FXS(rx, V27RX_Q_FLAG) = 1;
		FXU(rx, V27RX_Q_COUNT) = (unsigned short)(n + 1);
	}
	return verdict;
}

static short qd_mse[QD_BLOCKS];
static int qd_signal[QD_BLOCKS];
static int qd_active[QD_BLOCKS];
static short qd_ref_ret[QD_BLOCKS];
static short qd_ret[QD_BLOCKS];

static void
run_qd_one(unsigned seed, unsigned short count0, short limit, short acc0,
	   int level, long tag)
{
	static struct v27_fixture model;
	int i, v;

	rng_seed(seed);
	fx_build(&pristine, seed);
	FXU(pristine.rx, V27RX_Q_COUNT) = count0;
	FXS(pristine.rx, V27RX_Q_LIMIT) = limit;
	FXS(pristine.rx, V27RX_Q_ACC) = acc0;
	FXS(pristine.rx, V27RX_Q_FLAG) = 0;

	for (i = 0; i < QD_BLOCKS; i++) {
		unsigned r = rng_next();

		qd_mse[i] = (short)(r & 0x7fff);
		/*
		 * The two gates, deliberately made to disagree so that the
		 * AND is not the same function as either of them, and to go
		 * quiet for a run of blocks so the no-carrier arm is entered
		 * mid-sequence rather than only at the start.
		 */
		qd_signal[i] = (i % 7) ? (int)(r | 1u) : 0;
		qd_active[i] = (i % 5) ? (int)((r >> 3) | 1u) : 0;
		if (level >= 0 && (i % 11) == 3)
			qd_signal[i] = 0;
	}

	dsplibs_debug_level = ref_dsplibs_debug_level =
			(unsigned)(level < 0 ? 0 : level);
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	work = pristine;
	for (i = 0; i < QD_BLOCKS; i++) {
		fx_fse(&work)->mse = qd_mse[i];
		fx_agc(&work)->signal = qd_signal[i];
		fx_sre(&work)->active = qd_active[i];
		qd_ref_ret[i] = ref_QualityDetectV27(work.obj);
	}
	snap = work;

	work = pristine;
	for (i = 0; i < QD_BLOCKS; i++) {
		fx_fse(&work)->mse = qd_mse[i];
		fx_agc(&work)->signal = qd_signal[i];
		fx_sre(&work)->active = qd_active[i];
		qd_ret[i] = QualityDetectV27(work.obj);
		diff_eq_int("QualityDetectV27 block %ld", qd_ret[i],
			    qd_ref_ret[i], (long)(tag * 1000 + i));
	}
	diff_eq_obj("QualityDetectV27 state", struct v27_fixture, &work, &snap,
		    tag);

	dsplib_debug_capture_on = 0;
	diff_eq_int("QualityDetectV27 transcript (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	if (level > 1)
		diff_eq_int("QualityDetectV27 printed something (%ld)",
			    dsplib_debug_capture_lines(1) > 0, 1, tag);
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	for (i = 0; i < QD_BLOCKS; i++) {
		if (qd_signal[i] == 0 || qd_active[i] == 0)
			qd_quiet_trials++;
	}
	if ((short)count0 <= 0x30)
		qd_smooth_trials++;
	if ((short)count0 <= 0x32 && count0 + QD_BLOCKS > 0x32)
		qd_latch_trials++;

	for (v = 0; v < QD_VARIANTS; v++) {
		int differs = 0;

		model = pristine;
		for (i = 0; i < QD_BLOCKS; i++) {
			short r = qd_model(v, model.rx, qd_signal[i],
					   qd_active[i], qd_mse[i]);

			if (r != qd_ref_ret[i])
				differs = 1;
		}
		if (FXS(model.rx, V27RX_Q_ACC) != FXS(snap.rx, V27RX_Q_ACC)
		    || FXU(model.rx, V27RX_Q_COUNT)
		       != FXU(snap.rx, V27RX_Q_COUNT)
		    || FXS(model.rx, V27RX_Q_FLAG) != FXS(snap.rx,
							  V27RX_Q_FLAG))
			differs = 1;

		if (v == 0)
			diff_eq_int("QualityDetectV27 model (%ld)", differs, 0,
				    tag);
		else if (differs)
			qd_sep[v]++;
	}
}

static int
run_quality(void)
{
	static const unsigned short starts[] = { 0, 1, 0x30, 0x31, 0x32,
						 0x33 };
	static const short limits[] = { 0, 0x0100, 0x4000, 0x7fff, -1 };
	unsigned c, l;
	int lvl;

	diff_begin("QualityDetectV27");
	for (c = 0; c < sizeof(starts) / sizeof(starts[0]); c++)
		for (l = 0; l < sizeof(limits) / sizeof(limits[0]); l++)
			run_qd_one(0x9e3779b9u + c * 31u + l, starts[c],
				   limits[l], 0, -1, (long)(c * 10 + l));

	/*
	 * The latch's comparison is `<=`, and `<` differs from it on EXACTLY
	 * one value.  A stimulus sweep never lands there, so it is reached by
	 * construction: start at the latch block with the accumulator already
	 * equal to the limit.
	 */
	for (l = 0; l < sizeof(limits) / sizeof(limits[0]); l++) {
		run_qd_one(0x1234abcdu + l, 0x32, limits[l], limits[l], -1,
			   (long)(600 + l));
		qd_equal_trials++;
	}

	for (lvl = 0; lvl <= 3; lvl++)
		run_qd_one(0x5bd1e995u + (unsigned)lvl, 0, 0x2000, 0, lvl,
			   (long)(500 + lvl));

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 8.  V27RX_modem                                                        */

#define MOD_LOG		64
#define MOD_SPAN	4096

struct mod_call {
	void		*modem;
	long		in_off;
	long		out_off;
	unsigned short	count_in;
	unsigned short	count_out;
	short		produced;
};

static struct mod_call mod_log[MOD_LOG];
static int mod_calls;
static short *mod_in_base, *mod_out_base;

/*
 * The scripted handler, and it is the SAME function for both sides -- so a
 * difference in the log is a difference in the driver and nothing else.
 *
 * It does not dereference `in` or `out`.  That is deliberate: the driver
 * advances them by counts the script chooses, including counts that put them
 * a long way outside any buffer, and what is under test is the arithmetic and
 * not whether the samples survive.  The pointers are recorded as OFFSETS from
 * the base so the record compares as a number.
 */
static const short *mod_script;
static int mod_script_len;

static short
mod_handler(void *modem, short *in, short *out, unsigned short *count)
{
	struct mod_call *c;
	short take, give;

	if (mod_calls < MOD_LOG)
		c = &mod_log[mod_calls];
	else
		c = &mod_log[MOD_LOG - 1];

	c->modem = modem;
	c->in_off = (long)(in - mod_in_base);
	c->out_off = (long)(out - mod_out_base);
	c->count_in = *count;

	take = mod_script[(mod_calls * 2) % mod_script_len];
	give = mod_script[(mod_calls * 2 + 1) % mod_script_len];

	if ((unsigned short)take > *count)
		*count = 0;
	else
		*count = (unsigned short)(*count - (unsigned short)take);

	c->count_out = *count;
	c->produced = give;
	mod_calls++;
	return give;
}

static long modm_swap_sep;	/* in and out advanced the other way      */
static long modm_trunc_sep;	/* the total kept as an int               */
static long modm_wb_sep;	/* the count not written back             */
static long modm_flag_sep;	/* a different bit of the flags byte      */
static long modm_while_sep;	/* a while loop rather than a do-while    */
static long modm_ret_sep;	/* the status read from +0x18 or +0x20    */
static long modm_signed_sep;	/* `before` taken unsigned                */
static long modm_multi_trials;	/* the handler ran more than once         */

static short mod_in_buf[MOD_SPAN], mod_out_buf[MOD_SPAN];

static void
run_modem_one(const short *script, int slen, unsigned short count0,
	      unsigned seed, long tag)
{
	struct mod_call ref_log[MOD_LOG];
	unsigned short c_ref, c_ours;
	int ref_calls, ours_calls;
	int got, want, i;
	long in_pos, out_pos;
	short total;

	fx_build(&pristine, seed);
	*(v27_rx_state_fn *)(void *)FX(pristine.sh, V27SH_STATE) =
			mod_handler;
	mod_script = script;
	mod_script_len = slen;
	mod_in_base = mod_in_buf + MOD_SPAN / 2;
	mod_out_base = mod_out_buf + MOD_SPAN / 2;

	work = pristine;
	mod_calls = 0;
	memset(mod_log, 0, sizeof mod_log);
	c_ref = count0;
	want = ref_V27RX_modem(work.obj, mod_in_base, mod_out_base, &c_ref);
	ref_calls = mod_calls;
	memcpy(ref_log, mod_log, sizeof ref_log);
	snap = work;

	work = pristine;
	mod_calls = 0;
	memset(mod_log, 0, sizeof mod_log);
	c_ours = count0;
	got = V27RX_modem(work.obj, mod_in_base, mod_out_base, &c_ours);
	ours_calls = mod_calls;

	diff_eq_int("V27RX_modem return (%ld)", got, want, tag);
	diff_eq_int("V27RX_modem count out (%ld)", c_ours, c_ref, tag);
	diff_eq_int("V27RX_modem call count (%ld)", ours_calls, ref_calls,
		    tag);
	diff_eq_int("V27RX_modem log (%ld)",
		    memcmp(mod_log, ref_log, sizeof ref_log) == 0, 1, tag);
	diff_eq_obj("V27RX_modem state", struct v27_fixture, &work, &snap,
		    tag);

	if (ref_calls > 1)
		modm_multi_trials++;

	/*
	 * The named wrong readings, replayed against the log the blob itself
	 * produced.  Each one is a claim about ONE of the four things the
	 * driver computes, and each is counted only where it actually
	 * changes an answer.
	 */
	in_pos = 0;
	out_pos = 0;
	total = 0;
	for (i = 0; i < ref_calls && i < MOD_LOG; i++) {
		long consumed = (long)(short)ref_log[i].count_in
			      - (long)ref_log[i].count_out;
		long consumed_u = (long)(unsigned short)ref_log[i].count_in
				- (long)ref_log[i].count_out;

		if (ref_log[i].in_off != in_pos
		    || ref_log[i].out_off != out_pos)
			diff_eq_int("V27RX_modem pointer walk (%ld)", 0, 1,
				    tag);

		if (out_pos != in_pos)
			modm_swap_sep++;
		if (consumed != consumed_u)
			modm_signed_sep++;

		in_pos += consumed;
		out_pos += ref_log[i].produced;
		total = (short)(total + ref_log[i].produced);
	}
	if (ref_calls > 0 && total != (short)c_ref)
		diff_eq_int("V27RX_modem total (%ld)", (int)total,
			    (int)(short)c_ref, tag);

	/* An int accumulator would differ once the sum leaves 16 bits. */
	{
		long wide = 0;

		for (i = 0; i < ref_calls && i < MOD_LOG; i++)
			wide += ref_log[i].produced;
		if (wide != (long)(short)c_ref)
			modm_trunc_sep++;
	}
	if ((unsigned short)count0 != c_ref)
		modm_wb_sep++;
	if (count0 == 0 && ref_calls > 0)
		modm_while_sep++;

	/*
	 * The flags byte, and the status word.  Both are read off the
	 * snapshot rather than predicted, so a wrong bit or a wrong offset in
	 * `src/` shows up as a state difference above; what is counted here is
	 * whether the trial could have seen it.
	 */
	if ((*FX(pristine.obj, V27_OBJ_STATUS_FLAGS) & 0x02) != 0)
		modm_flag_sep++;
	if (FXI(pristine.obj, V27_OBJ_STATUS)
	    != FXI(pristine.obj, V27_OBJ_STATUS + 4)
	    && FXI(pristine.obj, V27_OBJ_STATUS)
	       != FXI(pristine.obj, V27_OBJ_STATUS - 4))
		modm_ret_sep++;
}

static int
run_modem(void)
{
	/* take, give pairs -- the handler walks them in order. */
	static const short s_simple[] = { 4, 2, 4, 3, 4, 1, 40, 5 };
	static const short s_drain[] = { 1, 7 };
	static const short s_burst[] = { 3, 1000, 3, 1000, 3, 1000, 3, 1000,
					 3, 1000, 3, 1000, 3, 1000, 3, 1000,
					 3, 1000, 3, 1000, 3, 1000, 3, 1000,
					 3, 1000, 3, 1000, 3, 1000, 3, 1000,
					 3, 1000, 3, 1000, 3, 1000, 3, 1000,
					 3, 1000, 3, 1000, 3, 1000, 3, 1000,
					 3, 1000, 3, 1000, 3, 1000, 3, 1000,
					 3, 1000, 3, 1000, 3, 1000, 3, 1000,
					 3, 1000, 3, 1000, 1, 1000 };
	static const short s_neg[] = { 2, -3, 2, 5, 2, -7, 2, 1 };
	static const short s_big[] = { 4000, 3, 4000, 5, 4000, -2, 4000, 9 };
	static const unsigned short counts[] = { 0, 1, 2, 4, 8, 40, 41 };
	unsigned i;

	diff_begin("V27RX_modem");
	for (i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
		run_modem_one(s_simple, 8, counts[i], 0x1a2b3c4du,
			      (long)(100 + i));
		run_modem_one(s_drain, 2, counts[i], 0x77777777u,
			      (long)(200 + i));
		run_modem_one(s_neg, 8, counts[i], 0x0c0ffee0u,
			      (long)(300 + i));
	}

	/* Enough output to take the accumulated count past a short. */
	run_modem_one(s_burst, 66, 100, 0x33445566u, 400);

	/*
	 * The far corner: an input count with bit 15 set.  `before` is signed
	 * and the value the handler leaves is not, so this is the only shape
	 * in which the two extensions the object makes of the same sixteen
	 * bits disagree.
	 */
	run_modem_one(s_big, 8, (unsigned short)0x8002u, 0x0a0a0a0au, 500);
	run_modem_one(s_big, 8, (unsigned short)0xfff0u, 0x0b0b0b0bu, 501);
	run_modem_one(s_big, 8, (unsigned short)0x8000u, 0x0c0c0c0cu, 502);
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 9.  V27RX_delete                                                       */
/*
 * What is checkable, and what is not: see the file header.  The SET of
 * released pointers is, and it is checked pointer by pointer rather than by a
 * count, because a count cannot tell a missed free from a double one.
 */
#define DEL_PTRS	20

struct del_build {
	void	*modem;
	void	*rx;
	void	*sh;
	void	*ptr[DEL_PTRS];
	int	nptr;
};

static void *
del_alloc(struct del_build *b, unsigned size)
{
	void *p = sysdep_malloc(size);

	if (b->nptr < DEL_PTRS)
		b->ptr[b->nptr++] = p;
	return p;
}

/*
 * Build a whole instance out of the harness allocator.  Every sub-object is
 * zeroed apart from the pointer fields, which is all any of the five `_free`
 * functions reads -- so this needs no configuration and cannot depend on one.
 */
static void
del_build(struct del_build *b)
{
	struct fpm_fse *fse;
	struct fpm_sre *sre;
	struct fpm_mrf *mrf;
	struct fpm_mtd *mtd;
	int i;

	b->nptr = 0;

	b->modem = del_alloc(b, OBJ_SIZE);
	memset(b->modem, 0, OBJ_SIZE);
	b->rx = del_alloc(b, RX_SIZE);
	memset(b->rx, 0, RX_SIZE);
	b->sh = del_alloc(b, SH_SIZE);
	memset(b->sh, 0, SH_SIZE);

	FXP(b->modem, V27_OBJ_RX) = b->rx;
	FXP(b->modem, V27_OBJ_SHARED) = b->sh;

	fse = (struct fpm_fse *)(void *)FX(b->rx, V27RX_FSE);
	fse->icoeff = (short *)del_alloc(b, 32);
	fse->qcoeff = (short *)del_alloc(b, 34);
	fse->hist = (short *)del_alloc(b, 36);
	fse->out_i = (short *)del_alloc(b, 38);
	fse->out_q = (short *)del_alloc(b, 40);

	sre = (struct fpm_sre *)(void *)FX(b->rx, V27RX_SRE);
	sre->coeff = (short *)del_alloc(b, 42);
	sre->hist = (short *)del_alloc(b, 44);
	sre->clk = (short *)del_alloc(b, 46);
	sre->rms_buf = (short *)del_alloc(b, 48);

	mrf = (struct fpm_mrf *)(void *)FX(b->rx, V27RX_MRF);
	mrf->history = (short *)del_alloc(b, 50);

	FXP(b->rx, V27RX_BUF_A) = del_alloc(b, 52);
	FXP(b->rx, V27RX_BUF_B) = del_alloc(b, 54);

	mtd = (struct fpm_mtd *)del_alloc(b, sizeof(struct fpm_mtd));
	memset(mtd, 0, sizeof(*mtd));
	mtd->acc = (short *)del_alloc(b, 56);
	FXP(b->sh, V27SH_MTD) = mtd;

	FXP(b->sh, V27SH_BUF) = del_alloc(b, 58);

	mtd = (struct fpm_mtd *)del_alloc(b, sizeof(struct fpm_mtd));
	memset(mtd, 0, sizeof(*mtd));
	mtd->acc = (short *)del_alloc(b, 60);
	FXP(b->sh, V27SH_MTD_V21) = mtd;

	for (i = 0; i < b->nptr; i++) {
		if (b->ptr[i] == 0)
			diff_eq_int("delete fixture allocated (%ld)", 0, 1,
				    (long)i);
	}
	if (b->nptr != DEL_PTRS)
		diff_eq_int("delete fixture planted every pointer (%ld)",
			    b->nptr, DEL_PTRS, 0);
}

/* Which of the planted pointers are still live, as a bitmap. */
static unsigned long
del_live(const struct del_build *b)
{
	unsigned long m = 0;
	int i;

	for (i = 0; i < b->nptr; i++)
		if (harness_alloc_ordinal(b->ptr[i]) != 0)
			m |= 1UL << i;
	return m;
}

static long del_sep[6];
static long del_trials;

static int
run_delete(void)
{
	struct del_build b;
	unsigned long live_ref, live_ours, live_bad;
	int frees_ref, bad_ref, frees_ours, bad_ours;
	int v;

	diff_begin("V27RX_delete");

	/* The blob. */
	harness_alloc_reset();
	del_build(&b);
	frees_ref = harness_alloc.frees;
	bad_ref = harness_alloc.bad_free;
	ref_V27RX_delete(b.modem);
	live_ref = del_live(&b);
	frees_ref = harness_alloc.frees - frees_ref;
	bad_ref = harness_alloc.bad_free - bad_ref;

	diff_eq_int("the blob released everything (%ld)", (long)live_ref, 0,
		    0);
	diff_eq_int("the blob freed each pointer once (%ld)", frees_ref,
		    DEL_PTRS, 0);
	diff_eq_int("the blob freed nothing unknown (%ld)", bad_ref, 0, 0);

	/* Ours, from the same shape. */
	harness_alloc_reset();
	del_build(&b);
	frees_ours = harness_alloc.frees;
	bad_ours = harness_alloc.bad_free;
	V27RX_delete(b.modem);
	live_ours = del_live(&b);
	frees_ours = harness_alloc.frees - frees_ours;
	bad_ours = harness_alloc.bad_free - bad_ours;

	diff_eq_int("V27RX_delete live set (%ld)", (long)live_ours,
		    (long)live_ref, 0);
	diff_eq_int("V27RX_delete free count (%ld)", frees_ours, frees_ref, 0);
	diff_eq_int("V27RX_delete bad frees (%ld)", bad_ours, bad_ref, 0);
	diff_eq_int("V27RX_delete left nothing live (%ld)",
		    harness_alloc.live, 0, 0);
	del_trials++;

	/*
	 * The named wrong readings.  Each is performed for real on its own
	 * freshly built instance, and separates when the live set, the free
	 * count or the bad-free count differs from the blob's.
	 *
	 *   1  the equaliser at rx + 0x94 (the symbol recovery) instead
	 *   2  the two scratch buffers not released
	 *   3  the shared block's detectors freed rather than deleted
	 *   4  the shared block's buffer taken from +0x18 rather than +0x1c
	 *   5  the instance itself not released
	 */
	for (v = 1; v <= 5; v++) {
		int f0, bf0;

		harness_alloc_reset();
		del_build(&b);
		f0 = harness_alloc.frees;
		bf0 = harness_alloc.bad_free;

		switch (v) {
		case 1:
			FPM_FSE_free((struct fpm_fse *)(void *)
					FX(b.rx, V27RX_SRE));
			FPM_SRE_free((struct fpm_sre *)(void *)
					FX(b.rx, V27RX_SRE));
			FPM_MRF_free((struct fpm_mrf *)(void *)
					FX(b.rx, V27RX_MRF));
			sysdep_free(FXP(b.rx, V27RX_BUF_B));
			sysdep_free(FXP(b.rx, V27RX_BUF_A));
			sysdep_free(b.rx);
			FPM_MTD_delete((struct fpm_mtd *)
					FXP(b.sh, V27SH_MTD));
			sysdep_free(FXP(b.sh, V27SH_BUF));
			FPM_MTD_delete((struct fpm_mtd *)
					FXP(b.sh, V27SH_MTD_V21));
			sysdep_free(b.sh);
			sysdep_free(b.modem);
			break;
		case 2:
			FPM_FSE_free((struct fpm_fse *)(void *)
					FX(b.rx, V27RX_FSE));
			FPM_SRE_free((struct fpm_sre *)(void *)
					FX(b.rx, V27RX_SRE));
			FPM_MRF_free((struct fpm_mrf *)(void *)
					FX(b.rx, V27RX_MRF));
			sysdep_free(b.rx);
			FPM_MTD_delete((struct fpm_mtd *)
					FXP(b.sh, V27SH_MTD));
			sysdep_free(FXP(b.sh, V27SH_BUF));
			FPM_MTD_delete((struct fpm_mtd *)
					FXP(b.sh, V27SH_MTD_V21));
			sysdep_free(b.sh);
			sysdep_free(b.modem);
			break;
		case 3:
			FPM_FSE_free((struct fpm_fse *)(void *)
					FX(b.rx, V27RX_FSE));
			FPM_SRE_free((struct fpm_sre *)(void *)
					FX(b.rx, V27RX_SRE));
			FPM_MRF_free((struct fpm_mrf *)(void *)
					FX(b.rx, V27RX_MRF));
			sysdep_free(FXP(b.rx, V27RX_BUF_B));
			sysdep_free(FXP(b.rx, V27RX_BUF_A));
			sysdep_free(b.rx);
			sysdep_free(FXP(b.sh, V27SH_MTD));
			sysdep_free(FXP(b.sh, V27SH_BUF));
			sysdep_free(FXP(b.sh, V27SH_MTD_V21));
			sysdep_free(b.sh);
			sysdep_free(b.modem);
			break;
		case 4:
			FPM_FSE_free((struct fpm_fse *)(void *)
					FX(b.rx, V27RX_FSE));
			FPM_SRE_free((struct fpm_sre *)(void *)
					FX(b.rx, V27RX_SRE));
			FPM_MRF_free((struct fpm_mrf *)(void *)
					FX(b.rx, V27RX_MRF));
			sysdep_free(FXP(b.rx, V27RX_BUF_B));
			sysdep_free(FXP(b.rx, V27RX_BUF_A));
			sysdep_free(b.rx);
			FPM_MTD_delete((struct fpm_mtd *)
					FXP(b.sh, V27SH_MTD));
			sysdep_free(FXP(b.sh, V27SH_MTD_V21));
			sysdep_free(b.sh);
			sysdep_free(b.modem);
			break;
		default:
			FPM_FSE_free((struct fpm_fse *)(void *)
					FX(b.rx, V27RX_FSE));
			FPM_SRE_free((struct fpm_sre *)(void *)
					FX(b.rx, V27RX_SRE));
			FPM_MRF_free((struct fpm_mrf *)(void *)
					FX(b.rx, V27RX_MRF));
			sysdep_free(FXP(b.rx, V27RX_BUF_B));
			sysdep_free(FXP(b.rx, V27RX_BUF_A));
			sysdep_free(b.rx);
			FPM_MTD_delete((struct fpm_mtd *)
					FXP(b.sh, V27SH_MTD));
			sysdep_free(FXP(b.sh, V27SH_BUF));
			FPM_MTD_delete((struct fpm_mtd *)
					FXP(b.sh, V27SH_MTD_V21));
			sysdep_free(b.sh);
			break;
		}

		live_bad = del_live(&b);
		if (live_bad != live_ref
		    || harness_alloc.frees - f0 != frees_ref
		    || harness_alloc.bad_free - bf0 != bad_ref)
			del_sep[v]++;

		/* Leave the allocator clean for the next test. */
		harness_alloc_reset();
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 10.  DataCarrierDetectV27                                              */
/*
 * The first of the two that drives live modules rather than reading flags,
 * so the fixture has to CONFIGURE them and not merely plant them: an
 * uninitialised `fpm_agc` has a zero block length and divides by it, and an
 * uninitialised `fpm_mtd` has a null coefficient pointer.  Both live inside
 * the fixture and neither allocates, so a snapshot is still a memcpy.
 *
 * Variants:
 *   0  the reading this reconstruction claims
 *   1  the mse threshold compared with >= rather than >
 *   2  the settled-symbol gate ignored, so the mse test always applies
 *   3  `cd &= 1` on the settled path replaced by leaving `cd` alone
 *   4  the V.21 arm's unconditional `cd = 1` omitted
 *   5  the arming condition without its `(cd & 1) == 0` half
 *   6  the V.21 sample counter not zeroed when the detector fires
 *   7  the energy comparison the other way round
 *   8  the reference level republished every call rather than every second
 *   9  the block not copied into the shared buffer before gain control
 *  10  the -8 dB scale applied without its >> 15
 *  11  the V.21 timeout compared with >= rather than >
 */
#define DCD_VARIANTS	12

static long dcd_sep[DCD_VARIANTS];
static long dcd_watch_trials;	/* the V.21 arm ran                       */
static long dcd_armed_trials;	/* ... and was armed, so the detector ran */
static long dcd_hit_trials;	/* ... and the detector fired             */
static long dcd_timeout_trials;	/* ... and the 0x4ff timeout expired      */
static long dcd_settled_trials;	/* the symbol counter was past 0x5db      */
static long dcd_mse_trials;	/* the mse was over the threshold         */
static long dcd_drop_trials;	/* the energy-drop test fired             */
static long dcd_republish_trials;/* the reference level was refreshed     */

static const short dcd_alpha = 29491;	/* 0.9 in Q15 */
static const short dcd_beta = 3277;	/* 0.1 in Q15 */
static struct fpm_agc_cfg dcd_agc_cfg;
static struct fpm_mtd_cfg dcd_mtd_cfg;

static void
dcd_cfg_init(void)
{
	dcd_agc_cfg.ref_level = 16384;
	dcd_agc_cfg.acquire_level = 10;
	dcd_agc_cfg.squelch_level = 80;
	dcd_agc_cfg.f06 = 0;
	dcd_agc_cfg.f08 = 0;
	dcd_agc_cfg.block_len = 36;
	dcd_agc_cfg.alpha = &dcd_alpha;
	dcd_agc_cfg.beta = &dcd_beta;
	dcd_agc_cfg.f14 = 0;
	dcd_agc_cfg.f16 = 0;

	/*
	 * Bell 103's detector bank, borrowed because it is the one pair of
	 * biquads this tree has extracted and asserted.  What it is tuned to
	 * does not matter -- the blob and the reconstruction run the same
	 * filter over the same samples -- but it must be a REAL bank, because
	 * `FPM_MTD_detect` runs `FPM_iir_filt` through `cfg.coeff` and a null
	 * or random pointer there is not a wrong answer, it is a fault.
	 */
	dcd_mtd_cfg.coeff = MTDb103_COEF;
	dcd_mtd_cfg.tones = MTD_TONES;
	dcd_mtd_cfg.ratio = 24576;
	dcd_mtd_cfg.min_level = 246;
	dcd_mtd_cfg.f0a = 0;
}

/*
 * Configure the shared block's two detectors and its gain control, on top of
 * whatever `fx_build` randomised.  This is exactly what `FPM_MTD_create`
 * does with a caller-supplied state and array -- copy the configuration,
 * clear the accumulators and the two filter states -- written out here
 * because create would clear `work`'s arrays rather than this copy's.
 */
static void
dcd_setup(struct v27_fixture *f)
{
	int i;

	for (i = 0; i < MTD_TONES * 2; i++) {
		f->acc_a[i] = 0;
		f->acc_b[i] = 0;
	}
	for (i = 0; i < NBUF; i++)
		f->shbuf[i] = 0;

	f->mtd_a.cfg = dcd_mtd_cfg;
	f->mtd_a.acc = work.acc_a;
	f->mtd_a.dc_state[0] = 0;
	f->mtd_a.dc_state[1] = 0;
	f->mtd_a.out_of_band = 0;
	f->mtd_a.wideband = 0;

	f->mtd_b.cfg = dcd_mtd_cfg;
	f->mtd_b.acc = work.acc_b;
	f->mtd_b.dc_state[0] = 0;
	f->mtd_b.dc_state[1] = 0;
	f->mtd_b.out_of_band = 0;
	f->mtd_b.wideband = 0;

	FXP(f->sh, V27SH_MTD) = &work.mtd_a;
	FXP(f->sh, V27SH_MTD_V21) = &work.mtd_b;
	FXP(f->sh, V27SH_BUF) = work.shbuf;

	FPM_AGC_init((struct fpm_agc *)(void *)FX(f->sh, V27SH_AGC),
		     &dcd_agc_cfg, 1);
}

static short
dcd_model(int variant, struct v27_fixture *f, short *samples,
	  unsigned short count, int cover)
{
	void *rx = f->rx;
	void *sh = f->sh;
	void *dec = FX(rx, V27RX_DEC);
	struct fpm_fse *fse = (struct fpm_fse *)(void *)FX(rx, V27RX_FSE);
	struct fpm_agc *agc = (struct fpm_agc *)(void *)FX(rx, V27RX_AGC);
	struct fpm_sre *sre = (struct fpm_sre *)(void *)FX(rx, V27RX_SRE);
	int mse_bad = variant == 1 ? fse->mse >= V27RX_MSE_NO_CARRIER
				   : fse->mse > V27RX_MSE_NO_CARRIER;
	short cd;

	cd = (short)(agc->signal & sre->active);

	if (cover && mse_bad)
		dcd_mse_trials++;

	if (FXU(sh, V27SH_V21_WATCH) == 0) {
		if (variant == 2
		    || FXS(dec, V27DEC_SYM_COUNT) > V27RX_DEC_SETTLED) {
			if (cover)
				dcd_settled_trials++;
			if (mse_bad)
				cd = 0;
			else if (variant != 3)
				cd &= 1;
		}
	} else {
		short i;

		if (cover)
			dcd_watch_trials++;
		if (mse_bad || (variant != 5 && (cd & 1) == 0))
			FXS(sh, V27SH_V21_ARMED) = 1;

		if (variant != 4)
			cd = 1;
		if (FXS(sh, V27SH_V21_ARMED) != 0) {
			/*
			 * THE SUB-OBJECTS ARE TAKEN FROM `f`, NOT THROUGH THE
			 * PLANTED POINTERS.  Every pointer in the fixture aims
			 * at `work`, because that is the copy the code under
			 * test is handed; a model that followed them would
			 * mutate `work` instead of its own copy and compare a
			 * state against itself.  What the model is for is the
			 * arithmetic and the control flow -- the indirection
			 * is what the src-against-blob comparison covers.
			 */
			short *buf = f->shbuf;

			if (cover)
				dcd_armed_trials++;
			if (variant != 9)
				for (i = 0; i < (int)count; i = (short)(i + 1))
					buf[i] = samples[i];

			FPM_AGC_agc((struct fpm_agc *)(void *)
					FX(sh, V27SH_AGC), buf, count);

			f->mtd_b.acc = f->acc_b;
			i = FPM_MTD_detect(&f->mtd_b, buf, (short)count);
			f->mtd_b.acc = work.acc_b;
			if (i != 0) {
				if (cover)
					dcd_hit_trials++;
				if (variant != 6)
					FXU(sh, V27SH_V21_SAMPLES) = 0;
			} else {
				FXU(sh, V27SH_V21_SAMPLES) =
					(unsigned short)
					(FXU(sh, V27SH_V21_SAMPLES) + count);
			}

			if (variant == 11
			    ? FXS(sh, V27SH_V21_SAMPLES) >= V27SH_V21_TIMEOUT
			    : FXS(sh, V27SH_V21_SAMPLES) > V27SH_V21_TIMEOUT) {
				if (cover)
					dcd_timeout_trials++;
				cd = 0;
			}
		}
	}

	if (FXS(rx, V27RX_RMS_ON) != 0) {
		short level = FPM_rms(samples, count);
		int thresh = FXS(rx, V27RX_RMS_REF) * V27RX_RMS_DROP_Q15;
		unsigned short n;

		if (variant != 10)
			thresh >>= 15;

		if (variant == 7 ? level > (short)thresh
				 : level < (short)thresh) {
			if (cover)
				dcd_drop_trials++;
			cd = 0;
		}

		n = (unsigned short)(FXU(rx, V27RX_RMS_COUNT) + 1);
		if (n == 2 || variant == 8) {
			if (cover)
				dcd_republish_trials++;
			FXS(rx, V27RX_RMS_REF) = level;
			FXU(rx, V27RX_RMS_COUNT) = 0;
		} else {
			FXU(rx, V27RX_RMS_COUNT) = n;
		}
	}

	return cd;
}

static short dcd_in[DCD_BLOCKS][DCD_N];
static short dcd_ref_ret[DCD_BLOCKS];
static short dcd_our_ret[DCD_BLOCKS];
static short dcd_mod_ret[DCD_BLOCKS];

/*
 * The stimulus: loud for the first half of the run and 22 dB quieter for the
 * second, so the energy-drop test has something to detect, with a tone
 * riding on it so the V.21 detector has something to lock to.  A flat noise
 * block would leave four of the nine coverage counters at zero.
 */
static void
dcd_stimulus(unsigned seed)
{
	int b, i;
	int phase = 0;

	rng_seed(seed);
	for (b = 0; b < DCD_BLOCKS; b++) {
		int amp = b < DCD_BLOCKS / 2 ? 9000 : 700;

		for (i = 0; i < DCD_N; i++) {
			int t = (phase / 4) % 8;
			static const int wave[8] = { 0, 707, 1000, 707,
						     0, -707, -1000, -707 };

			dcd_in[b][i] = (short)((amp * wave[t]) / 1000
					       + (int)(rng_next() % 401) - 200);
			phase++;
		}
	}
}

static void
run_dcd_one(unsigned seed, unsigned short watch, short armed,
	    short rms_on, short rms_ref, short mse, unsigned short sym_count,
	    unsigned short count, unsigned short samples0, int force_absent,
	    int level, long tag)
{
	static struct v27_fixture model;
	int i, v;

	fx_build(&pristine, seed);
	dcd_setup(&pristine);

	FXU(pristine.sh, V27SH_V21_WATCH) = watch;
	FXS(pristine.sh, V27SH_V21_ARMED) = armed;
	FXU(pristine.sh, V27SH_V21_SAMPLES) = samples0;
	/*
	 * The V.21 timeout at 0x4ff is not reachable by stimulus alone: the
	 * detector reports NOSIGNAL as well as PRESENT through the same
	 * non-zero return, and either resets the counter -- so any block the
	 * bank does not like clears the very thing the timeout counts.  It is
	 * reached by CONSTRUCTION instead: a counter already at the threshold
	 * and a detector state whose out-of-band energy is 90% of its total,
	 * which is the one shape that returns ABSENT for a block of no
	 * samples at all.
	 */
	if (force_absent) {
		pristine.mtd_b.wideband = 10000;
		pristine.mtd_b.out_of_band = 9000;
	}

	/*
	 * The carrier term, pinned rather than left to the random fill.
	 *
	 * `cd` is `(short)(signal & active)` and three of the six paths out
	 * return it unchanged, so its LOW BIT decides two of the branches and
	 * its upper bits decide whether `cd &= 1` is observable at all.  Left
	 * random it happened to be even on the one trial where the mse
	 * threshold is exactly 0x3fff, which is the only input that separates
	 * `>` from `>=` -- so that check measured nothing while looking
	 * thorough.  Derived from the seed here so the sweep covers odd, even
	 * and zero without any trial being able to drift.
	 */
	fx_agc(&pristine)->signal = (int)(0x0f0f0000u | (seed & 0xfu));
	fx_sre(&pristine)->active = 0x00ff0007;
	FXS(pristine.rx, V27RX_RMS_ON) = rms_on;
	FXS(pristine.rx, V27RX_RMS_REF) = rms_ref;
	FXU(pristine.rx, V27RX_RMS_COUNT) = 0;
	FXU(DEC(&pristine), V27DEC_SYM_COUNT) = sym_count;
	((struct fpm_fse *)(void *)FX(pristine.rx, V27RX_FSE))->mse = mse;

	dcd_stimulus(seed);

	dsplibs_debug_level = ref_dsplibs_debug_level =
			(unsigned)(level < 0 ? 0 : level);
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	work = pristine;
	for (i = 0; i < DCD_BLOCKS; i++)
		dcd_ref_ret[i] = ref_DataCarrierDetectV27(work.obj, dcd_in[i],
							  count);
	snap = work;

	work = pristine;
	for (i = 0; i < DCD_BLOCKS; i++) {
		dcd_our_ret[i] = DataCarrierDetectV27(work.obj, dcd_in[i],
						      count);
		diff_eq_int("DataCarrierDetectV27 block %ld", dcd_our_ret[i],
			    dcd_ref_ret[i], (long)(tag * 100 + i));
	}
	diff_eq_obj("DataCarrierDetectV27 state", struct v27_fixture, &work,
		    &snap, tag);

	dsplib_debug_capture_on = 0;
	diff_eq_int("DataCarrierDetectV27 transcript (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	/*
	 * The models, with the debug level back at zero so that nothing they
	 * call can add to a transcript that has already been compared.
	 */
	for (v = 0; v < DCD_VARIANTS; v++) {
		int differs = 0;

		model = pristine;
		for (i = 0; i < DCD_BLOCKS; i++) {
			dcd_mod_ret[i] = dcd_model(v, &model, dcd_in[i], count,
						   v == 0);
			if (dcd_mod_ret[i] != dcd_ref_ret[i])
				differs = 1;
		}
		/*
		 * The whole fixture, minus the pointer to `work` the model
		 * copy still carries -- it is compared field by field through
		 * the two blocks the function writes rather than by memcmp,
		 * because `model` and `snap` are different objects and only
		 * `snap` was ever handed to the code under test.
		 */
		if (FXU(model.sh, V27SH_V21_SAMPLES)
		    != FXU(snap.sh, V27SH_V21_SAMPLES)
		    || FXS(model.sh, V27SH_V21_ARMED)
		       != FXS(snap.sh, V27SH_V21_ARMED)
		    || FXS(model.rx, V27RX_RMS_REF) != FXS(snap.rx,
							   V27RX_RMS_REF)
		    || FXU(model.rx, V27RX_RMS_COUNT)
		       != FXU(snap.rx, V27RX_RMS_COUNT)
		    || memcmp(model.shbuf, snap.shbuf,
			      sizeof model.shbuf) != 0
		    || memcmp(&model.mtd_b, &snap.mtd_b,
			      sizeof model.mtd_b) != 0
		    || memcmp(model.acc_b, snap.acc_b,
			      sizeof model.acc_b) != 0
		    || memcmp(FX(model.sh, V27SH_AGC), FX(snap.sh, V27SH_AGC),
			      sizeof(struct fpm_agc)) != 0)
			differs = 1;

		if (v == 0)
			diff_eq_int("DataCarrierDetectV27 model (%ld)",
				    differs, 0, tag);
		else if (differs)
			dcd_sep[v]++;
	}
}

static int
run_dcd(void)
{
	static const unsigned short counts[] = { 0, 1, 36, DCD_N };
	unsigned c;
	int lvl;
	long tag = 0;

	dcd_cfg_init();
	diff_begin("DataCarrierDetectV27");

	for (c = 0; c < sizeof(counts) / sizeof(counts[0]); c++) {
		unsigned short n = counts[c];

		/*
		 * The no-watch half: both sides of the settled gate, both
		 * sides of the mse threshold, and the threshold itself --
		 * which is compared with `>`, so 0x3fff must NOT fire.
		 */
		run_dcd_one(0x0a1b2c3du, 0, 0, 0, 0, 0x0100, 0, n, 0, 0, -1,
			    tag++);
		run_dcd_one(0x0a1b2c3du, 0, 0, 0, 0, 0x0100, 0x0600, n, 0, 0,
			    -1, tag++);
		run_dcd_one(0x0a1b2c3du, 0, 0, 0, 0, 0x7000, 0x0600, n, 0, 0,
			    -1, tag++);
		run_dcd_one(0x0a1b2c3du, 0, 0, 0, 0, 0x3fff, 0x0600, n, 0, 0,
			    -1, tag++);
		run_dcd_one(0x11223344u, 0, 0, 1, 20000, 0x0100, 0x0600, n, 0,
			    0, -1, tag++);
		run_dcd_one(0x11223344u, 0, 0, 1, 0, 0x0100, 0x0600, n, 0, 0,
			    -1, tag++);

		/* The V.21 half: not armed, armed, and armed by the mse. */
		run_dcd_one(0x55667788u, 1, 0, 0, 0, 0x0100, 0x0600, n, 0, 0,
			    -1, tag++);
		run_dcd_one(0x55667788u, 1, 1, 0, 0, 0x0100, 0x0600, n, 0, 0,
			    -1, tag++);
		run_dcd_one(0x55667788u, 1, 0, 0, 0, 0x7000, 0x0600, n, 0, 0,
			    -1, tag++);
		run_dcd_one(0x99aabbccu, 1, 1, 1, 20000, 0x0100, 0x0600, n, 0,
			    0, -1, tag++);

		/*
		 * The V.21 timeout, by construction -- see the note in
		 * run_dcd_one -- and the value one below it, so that `>` is
		 * separated from `>=` rather than merely exercised.
		 */
		run_dcd_one(0x0f0f0f0fu, 1, 1, 0, 0, 0x0100, 0x0600, n, 0x500,
			    1, -1, tag++);
		run_dcd_one(0x0f0f0f0fu, 1, 1, 0, 0, 0x0100, 0x0600, n, 0x4ff,
			    1, -1, tag++);
	}

	/* And the three diagnostic sites, at every level that can reach them. */
	for (lvl = 0; lvl <= 3; lvl++) {
		run_dcd_one(0x11223344u, 0, 0, 1, 20000, 0x7000, 0x0600,
			    DCD_N, 0, 0, lvl, 900 + lvl);
		run_dcd_one(0x99aabbccu, 1, 1, 1, 20000, 0x7000, 0x0600,
			    DCD_N, 0x500, 1, lvl, 920 + lvl);
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */
/* 11.  V27RX_eq_train                                                    */
/*
 * The TRAINING slicer, and the only thing in the object that installs the
 * running one.  It is pure state -- no FPM module is called -- so the fixture
 * is the decision fixture with five more fields planted, and the interesting
 * content is entirely in WHICH field and WHICH comparison.
 *
 * IT IS RUN AS A SEQUENCE, never as one call (finding F8790).  Three separate
 * pieces of state are carried between calls -- the saturating symbol counter,
 * the training counter and the previous constellation angle -- and the
 * handover fires on exactly one call of the sequence, so a one-call fixture
 * could not see the handover at all and could not see the counters accumulate.
 *
 * THE FUNCTION POINTER IT INSTALLS CANNOT BE COMPARED AS A BYTE PATTERN: the
 * blob installs `ref_V27RX_decision` and the reconstruction installs
 * `V27RX_decision`, and those are two different addresses of the same
 * function.  So each side's `cfg.decision` is read back into a three-valued
 * verdict -- untouched, the right slicer, something else -- the verdicts are
 * compared, and the field is restored to a sentinel before the fixtures are
 * memcmp'd.  A test that skipped those four bytes instead would pass on a
 * reconstruction that installed the wrong function.
 *
 * Variants:
 * `>` VERSUS `>=` ON EITHER FOLD IS AN EQUIVALENT MUTANT, and that was
 * measured rather than assumed: both were written, both reported a separating
 * count of ZERO, and the reason is that the folded difference is used for
 * NOTHING but its magnitude.  At exactly +/- half a turn the fold negates it,
 * so |d| is unchanged and no observable moves.  The strictness of those two
 * comparisons is settled by the disassembly alone (F9119) and this test does
 * not pretend to cover it; what it DOES cover is the folds themselves, which
 * are observable because folding can carry |d| below the advance threshold.
 *
 * Variants:
 *   0  the reading this reconstruction claims
 *   1  the high fold omitted
 *   2  the low fold omitted
 *   3  the advance taken at >= a quarter turn rather than >
 *   4  the step the whole constellation rather than half of it
 *   5  the phase mask applied before the step is added rather than after
 *   6  `angle_prev` given the phase INDEX rather than the angle
 *   7  `angle_prev` not stored at all
 *   8  the two training lengths transposed
 *   9  the handover at > the limit rather than >=
 *  10  the training counter not written back
 *  11  `mu_sel` not cleared on every call
 *  12  the symbol counter not saturated
 *  13  the fold done in 32 bits, without narrowing to `short`
 */
#define EQ_VARIANTS	14
#define EQ_BLOCKS	72

enum eq_defect {
	E_NONE = 0,
	E_FOLD_HIGH_NONE,
	E_FOLD_LOW_NONE,
	E_TOL_GE,
	E_STEP_FULL,
	E_MASK_ORDER,
	E_PREV_IS_INDEX,
	E_PREV_NOT_STORED,
	E_LIMIT_SWAPPED,
	E_LIMIT_GT,
	E_COUNT_NOT_STORED,
	E_MU_NOT_CLEARED,
	E_NO_SATURATE,
	E_DIFF_INT
};

static long eq_sep[EQ_VARIANTS];
static long eq_handover_trials;	/* the handover fired, FROM THE REFERENCE  */
static long eq_advance_trials;	/* the reference advanced its reference    */
static long eq_hold_trials;	/* ... and left it alone                   */
static long eq_saturate_trials;	/* the symbol counter wrapped to half scale */

/*
 * The value `cfg.decision` starts every trial holding.  It is a real function
 * so that a fixture handed to a live equaliser would not fault; nothing calls
 * it here.
 */
static unsigned short
eq_sentinel_fn(struct fpm_fse *state, short *angle, short *mag)
{
	(void)state;
	(void)angle;
	(void)mag;
	return 0;
}

struct eq_setup {
	int		eight_phase;	/* dec + V27DEC_EIGHT_PHASE       */
	unsigned short	mask;		/* dec + V27DEC_PHASE_MASK        */
	int		train_short;	/* dec + V27DEC_TRAIN_SHORT       */
	unsigned short	count0;		/* dec + V27DEC_TRAIN_COUNT       */
	unsigned short	sym0;		/* dec + V27DEC_SYM_COUNT         */
	short		prev0;		/* dec + V27DEC_ANGLE_PREV        */
	short		last0;		/* dec + V27DEC_LAST              */
	short		taps;		/* fse.cfg.taps                   */
};

/*
 * The measured angles a trial feeds in, one per call.  They sweep the whole
 * signed range AND land exactly on both fold thresholds and on the advance
 * threshold, because all three comparisons in this function are strict on one
 * side and the boundary is the only input that separates `>` from `>=`.
 */
static short eq_angle[EQ_BLOCKS];

static void
eq_angles(struct v27_fixture *f, const struct eq_setup *u, unsigned seed)
{
	int i;
	short base;

	rng_seed(seed);
	base = FXS(DEC(f), V27DEC_ANGLE_PREV);
	for (i = 0; i < EQ_BLOCKS; i++) {
		int d;

		switch (i % 9) {
		case 0:	d =  V27DEC_HALF_TURN;		break;
		case 1:	d =  V27DEC_HALF_TURN + 1;	break;
		case 2:	d = -V27DEC_HALF_TURN;		break;
		case 3:	d = -V27DEC_HALF_TURN - 1;	break;
		case 4:	d =  V27DEC_QUARTER_TURN;	break;
		case 5:	d = -V27DEC_QUARTER_TURN;	break;
		case 6:	d =  V27DEC_QUARTER_TURN + 1;	break;
		default:
			d = (int)(rng_next() % 65536u) - 32768;
			break;
		}
		eq_angle[i] = (short)(base + d);
	}
	(void)u;
}

static void
eq_build(struct v27_fixture *f, unsigned seed, const struct eq_setup *u)
{
	fx_build(f, seed);

	FXI(DEC(f), V27DEC_EIGHT_PHASE) = u->eight_phase;
	FXU(DEC(f), V27DEC_PHASE_MASK) = u->mask;
	FXI(DEC(f), V27DEC_TRAIN_SHORT) = u->train_short;
	FXU(DEC(f), V27DEC_TRAIN_COUNT) = u->count0;
	FXU(DEC(f), V27DEC_SYM_COUNT) = u->sym0;
	FXS(DEC(f), V27DEC_ANGLE_PREV) = u->prev0;
	FXS(DEC(f), V27DEC_LAST) = u->last0;

	fx_fse(f)->cfg.taps = u->taps;
	fx_fse(f)->cfg.decision = eq_sentinel_fn;
	fx_fse(f)->mu_sel = 3;
	fx_fse(f)->lms_force = 0x5a5a5a5a;
}

/* Untouched, the right slicer, or something else entirely. */
static int
eq_verdict(fpm_fse_decision got, fpm_fse_decision want)
{
	if (got == eq_sentinel_fn)
		return 0;
	if (got == want)
		return 1;
	return 2;
}

/* The object's own sequence, with one reading changed. */
static unsigned short
eq_model(int v, struct v27_fixture *f, short *angle, short *mag)
{
	void *dec = DEC(f);
	struct fpm_fse *state = fx_fse(f);
	const short *tbl = (const short *)FXP(dec, V27DEC_ANGLES);
	unsigned short count;
	short step, a, limit, i;
	int d, err;

	count = (unsigned short)(FXU(dec, V27DEC_SYM_COUNT) + 1);
	if (v != E_NO_SATURATE && count == V27DEC_PHASE_FULL)
		FXU(dec, V27DEC_SYM_COUNT) = V27DEC_PHASE_FULL / 2;
	else
		FXU(dec, V27DEC_SYM_COUNT) = count;

	step = (short)(FXI(dec, V27DEC_EIGHT_PHASE) ? 4 : 2);
	if (v == E_STEP_FULL)
		step = (short)(step * 2);

	d = *angle - FXS(dec, V27DEC_ANGLE_PREV);
	if (v != E_DIFF_INT)
		d = (short)d;
	if (v != E_FOLD_HIGH_NONE && d > V27DEC_HALF_TURN) {
		d += V27DEC_PHASE_FULL;
		if (v != E_DIFF_INT)
			d = (short)d;
	}
	if (v != E_FOLD_LOW_NONE && d < -V27DEC_HALF_TURN) {
		d -= V27DEC_PHASE_FULL;
		if (v != E_DIFF_INT)
			d = (short)d;
	}

	*mag = V27DEC_MAG;

	err = d < 0 ? -d : d;
	if (v == E_TOL_GE ? err >= V27DEC_QUARTER_TURN
			  : err > V27DEC_QUARTER_TURN) {
		short last = FXS(dec, V27DEC_LAST);
		unsigned short mask = FXU(dec, V27DEC_PHASE_MASK);

		FXS(dec, V27DEC_LAST) = (short)
			(v == E_MASK_ORDER ? (last & mask) + step
					   : (last + step) & mask);
	}

	a = tbl[FXS(dec, V27DEC_LAST)];
	if (FXI(dec, V27DEC_TRAIN_SHORT))
		limit = (short)(v == E_LIMIT_SWAPPED ? V27DEC_TRAIN_SYMS_LONG
						     : V27DEC_TRAIN_SYMS_SHORT);
	else
		limit = (short)(v == E_LIMIT_SWAPPED ? V27DEC_TRAIN_SYMS_SHORT
						     : V27DEC_TRAIN_SYMS_LONG);
	*angle = a;
	if (v == E_PREV_IS_INDEX)
		FXS(dec, V27DEC_ANGLE_PREV) = FXS(dec, V27DEC_LAST);
	else if (v != E_PREV_NOT_STORED)
		FXS(dec, V27DEC_ANGLE_PREV) = a;

	if (v != E_COUNT_NOT_STORED)
		FXU(dec, V27DEC_TRAIN_COUNT) =
			(unsigned short)(FXU(dec, V27DEC_TRAIN_COUNT) + 1);

	if (v != E_MU_NOT_CLEARED)
		state->mu_sel = 0;

	if (v == E_LIMIT_GT ? FXS(dec, V27DEC_TRAIN_COUNT) > limit
			    : FXS(dec, V27DEC_TRAIN_COUNT) >= limit) {
		for (i = 0; i < state->cfg.taps; i = (short)(i + 1)) {
			/* No body in the object.  F9117. */
		}
		state->lms_force = 0;
		state->mu_sel = 1;
		state->cfg.decision = V27RX_decision;
	}

	return 0xffff;
}

/* Everything one call of the sequence made observable, folded into a word. */
static unsigned long
eq_mark(unsigned long m, struct v27_fixture *f, unsigned short ret, short ang,
	short mag, int installed)
{
	void *dec = DEC(f);

	m = m * 1000003u + ret;
	m = m * 31u + (unsigned short)ang;
	m = m * 31u + (unsigned short)mag;
	m = m * 31u + FXU(dec, V27DEC_SYM_COUNT);
	m = m * 31u + FXU(dec, V27DEC_TRAIN_COUNT);
	m = m * 31u + (unsigned short)FXS(dec, V27DEC_LAST);
	m = m * 31u + (unsigned short)FXS(dec, V27DEC_ANGLE_PREV);
	m = m * 31u + (unsigned short)fx_fse(f)->mu_sel;
	m = m * 131u + (unsigned long)(unsigned int)fx_fse(f)->lms_force;
	m = m * 31u + (unsigned)installed;
	return m;
}

static void
run_eq_one(unsigned seed, const struct eq_setup *u, long tag)
{
	static struct v27_fixture model;
	unsigned long marka = 0, markc;
	short ang_ref[EQ_BLOCKS], mag_ref[EQ_BLOCKS];
	unsigned short ret_ref[EQ_BLOCKS];
	int inst_ref[EQ_BLOCKS];
	int v, i;

	eq_build(&pristine, seed, u);
	eq_angles(&pristine, u, seed ^ 0xa17ea51u);

	/* The blob. */
	work = pristine;
	for (i = 0; i < EQ_BLOCKS; i++) {
		short a = eq_angle[i], m = (short)0x7abc;
		short before = FXS(DEC(&work), V27DEC_LAST);
		unsigned short sym = FXU(DEC(&work), V27DEC_SYM_COUNT);

		ret_ref[i] = ref_V27RX_eq_train(fx_fse(&work), &a, &m);
		ang_ref[i] = a;
		mag_ref[i] = m;
		inst_ref[i] = eq_verdict(fx_fse(&work)->cfg.decision,
					 (fpm_fse_decision)ref_V27RX_decision);
		if (inst_ref[i] == 1) {
			eq_handover_trials++;
			fx_fse(&work)->cfg.decision = eq_sentinel_fn;
		}
		if (FXS(DEC(&work), V27DEC_LAST) != before)
			eq_advance_trials++;
		else
			eq_hold_trials++;
		if (sym == V27DEC_PHASE_FULL - 1)
			eq_saturate_trials++;
		marka = eq_mark(marka, &work, ret_ref[i], a, m, inst_ref[i]);
	}
	snap = work;

	/* Ours, on the same addresses. */
	work = pristine;
	for (i = 0; i < EQ_BLOCKS; i++) {
		short a = eq_angle[i], m = (short)0x7abc;
		int inst;

		diff_eq_int("V27RX_eq_train returned (%ld)",
			    (long)V27RX_eq_train(fx_fse(&work), &a, &m),
			    (long)ret_ref[i], tag * 1000 + i);
		diff_eq_int("V27RX_eq_train angle (%ld)", (long)a,
			    (long)ang_ref[i], tag * 1000 + i);
		diff_eq_int("V27RX_eq_train mag (%ld)", (long)m,
			    (long)mag_ref[i], tag * 1000 + i);
		inst = eq_verdict(fx_fse(&work)->cfg.decision, V27RX_decision);
		diff_eq_int("V27RX_eq_train installed (%ld)", (long)inst,
			    (long)inst_ref[i], tag * 1000 + i);
		if (inst == 1)
			fx_fse(&work)->cfg.decision = eq_sentinel_fn;
	}
	diff_eq_obj("V27RX_eq_train state", struct v27_fixture, &work, &snap,
		    tag);

	/* The named wrong readings, replayed from a fresh fixture each. */
	for (v = 0; v < EQ_VARIANTS; v++) {
		model = pristine;
		markc = 0;
		for (i = 0; i < EQ_BLOCKS; i++) {
			short a = eq_angle[i], m = (short)0x7abc;
			unsigned short r = eq_model(v, &model, &a, &m);
			int inst = eq_verdict(fx_fse(&model)->cfg.decision,
					      V27RX_decision);

			if (inst == 1)
				fx_fse(&model)->cfg.decision = eq_sentinel_fn;
			markc = eq_mark(markc, &model, r, a, m, inst);
		}
		if (v == 0)
			diff_eq_int("V27RX_eq_train model (%ld)",
				    markc == marka, 1, tag);
		else if (markc != marka)
			eq_sep[v]++;
	}
}

static int
run_eq_train(void)
{
	static const struct eq_setup setups[] = {
		/* eight mask short count0 sym0    prev0   last0 taps */
		{ 0, 3, 1,      0,      0,      0,      0,    12 },
		{ 1, 7, 1,      0,      0,      0,      3,     8 },
		{ 0, 3, 0,  0x3e0,      0,  0x1234,     1,    16 },
		{ 1, 7, 0,  0x3e5, 0x7fc0, -0x4000,     6,     4 },
		{ 0, 3, 1,   0x2e,      3,  0x7fff,     2,     1 },
		{ 1, 7, 1,   0x31, 0xfffe, -0x7fff,     5,    31 },
		{ 0, 3, 0,      0,      0,  0x4000,     0,     0 },
		{ 1, 7, 1,      0, 0x7ffe,  0x2000,     7,     6 }
	};
	int s, style;
	long tag = 0;

	diff_begin("V27RX_eq_train");

	for (style = 0; style < 3; style++) {
		fx_table_style = style;
		for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++) {
			run_eq_one(0xe9000000u + (unsigned)tag, &setups[s],
				   tag);
			tag++;
		}
	}
	fx_table_style = 0;

	diff_eq_int("V27RX_eq_train handed over (%ld)", eq_handover_trials > 0,
		    1, eq_handover_trials);
	diff_eq_int("V27RX_eq_train advanced the reference (%ld)",
		    eq_advance_trials > 0, 1, eq_advance_trials);
	diff_eq_int("V27RX_eq_train held the reference (%ld)",
		    eq_hold_trials > 0, 1, eq_hold_trials);
	diff_eq_int("V27RX_eq_train saturated the symbol counter (%ld)",
		    eq_saturate_trials > 0, 1, eq_saturate_trials);
	for (s = 1; s < EQ_VARIANTS; s++)
		diff_eq_int("eq_train wrong reading %ld separates",
			    eq_sep[s] > 0, 1, s);

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 12.  ScrambleDataV27 and DescrambleDataV27                             */
/*
 * Two one-line wrappers, and the only things that can be wrong about either
 * are WHICH block of the instance it takes and WHICH offset inside it -- so
 * the fixture puts a live, differently-seeded `struct sdmv27` at all four
 * combinations of the two blocks and the two offsets.  A wrong reading then
 * lands on a plausible scrambler and returns a wrong answer, rather than
 * faulting on random bytes and being caught for the wrong reason.
 *
 * `count` IS SIGNED in both wrappers (`movswl`, F9118) and that is recorded
 * rather than measured here: the module's parameter is a `short`, so the
 * widening the wrapper chooses is not observable through it.  What IS
 * measured is that a large count walks the register the same number of times
 * on both sides.
 */
#define SC_WORDS	24
#define SC_BLOCKS	6
#define SC_VARIANTS	6

enum sc_defect {
	C_NONE = 0,
	C_TX_FROM_RX,		/* the scrambler's block taken from RX      */
	C_RX_FROM_TX,		/* the descrambler's block taken from TX    */
	C_TX_AT_RX_OFF,		/* the scrambler at +0x3c inside TX         */
	C_RX_AT_TX_OFF,		/* the descrambler at +0x1c inside RX       */
	C_FN_SWAPPED		/* each wrapper calls the other's module    */
};

static long sc_sep[SC_VARIANTS];
static long sc_invert_trials;	/* the guard inverted a bit, either side   */
static long sc_run_trials;	/* the guard's run counter was non-zero    */

static unsigned short sc_data[SC_WORDS];
static unsigned short sc_data_ref[SC_BLOCKS][SC_WORDS];

#define SC_TXA(f)	((struct sdmv27 *)(void *)FX((f)->tx, V27TX_SDM))
#define SC_TXB(f)	((struct sdmv27 *)(void *)FX((f)->tx, V27RX_SDM))
#define SC_RXA(f)	((struct sdmv27 *)(void *)FX((f)->rx, V27RX_SDM))
#define SC_RXB(f)	((struct sdmv27 *)(void *)FX((f)->rx, V27TX_SDM))

static void
sc_build(struct v27_fixture *f, unsigned seed)
{
	static struct sdmv27_cfg cfg3, cfg2;

	fx_build(f, seed);
	FXP(f->obj, V27_OBJ_TX) = work.tx;

	cfg3.nbits = 3;
	cfg2.nbits = 2;

	/*
	 * Four live scramblers, so a wrong block or a wrong offset produces a
	 * WRONG ANSWER rather than a fault.  They are seeded differently by
	 * hand after init, because init gives every one of them the same
	 * register and the four would then be indistinguishable.
	 */
	SDMv27_init(SC_TXA(f), &cfg3);
	SDMv27_init(SC_TXB(f), &cfg2);
	SDMv27_init(SC_RXA(f), &cfg3);
	SDMv27_init(SC_RXB(f), &cfg2);

	SC_TXA(f)->reg = 0x0135;
	SC_TXB(f)->reg = 0x1eca;
	SC_RXA(f)->reg = 0x0ace;
	SC_RXB(f)->reg = 0x1357;
	SC_TXA(f)->run = 7;
	SC_TXB(f)->run = 19;
	SC_RXA(f)->run = 30;
	SC_RXB(f)->run = 2;
}

static void
sc_model(int v, struct v27_fixture *f, int descramble, unsigned short *data,
	 short count)
{
	struct sdmv27 *sdm;

	if (descramble)
		sdm = (v == C_RX_FROM_TX) ? SC_TXA(f)
		    : (v == C_RX_AT_TX_OFF) ? SC_RXB(f)
					    : SC_RXA(f);
	else
		sdm = (v == C_TX_FROM_RX) ? SC_RXA(f)
		    : (v == C_TX_AT_RX_OFF) ? SC_TXB(f)
					    : SC_TXA(f);

	if (v == C_FN_SWAPPED)
		descramble = !descramble;

	if (descramble)
		SDMv27_descrambler(sdm, data, count);
	else
		SDMv27_scrambler(sdm, data, count);
}

static unsigned long
sc_mark(unsigned long m, struct v27_fixture *f, const unsigned short *data,
	int n)
{
	int i;

	for (i = 0; i < n; i++)
		m = m * 1000003u + data[i];
	m = m * 131u + SC_TXA(f)->reg;
	m = m * 131u + SC_TXB(f)->reg;
	m = m * 131u + SC_RXA(f)->reg;
	m = m * 131u + SC_RXB(f)->reg;
	m = m * 31u + SC_TXA(f)->run;
	m = m * 31u + SC_RXA(f)->run;
	m = m * 31u + SC_TXA(f)->pending;
	m = m * 31u + SC_RXA(f)->pending;
	m = m * 31u + SC_TXA(f)->inverting;
	m = m * 31u + SC_RXA(f)->inverting;
	return m;
}

static void
run_sc_one(unsigned seed, short count, long tag)
{
	static struct v27_fixture model;
	unsigned long marka = 0, markc;
	int v, b, i, n;

	n = count > 0 && count < SC_WORDS ? count : SC_WORDS;

	sc_build(&pristine, seed);

	work = pristine;
	for (b = 0; b < SC_BLOCKS; b++) {
		rng_seed(seed + (unsigned)b * 7919u);
		for (i = 0; i < SC_WORDS; i++)
			sc_data[i] = (unsigned short)(rng_next() & 7u);

		ref_ScrambleDataV27(work.obj, sc_data, count);
		marka = sc_mark(marka, &work, sc_data, n);
		for (i = 0; i < SC_WORDS; i++)
			sc_data_ref[b][i] = sc_data[i];
		ref_DescrambleDataV27(work.obj, sc_data, count);
		marka = sc_mark(marka, &work, sc_data, n);

		if (SC_TXA(&work)->inverting || SC_RXA(&work)->inverting)
			sc_invert_trials++;
		if (SC_TXA(&work)->run != 0)
			sc_run_trials++;
	}
	snap = work;

	work = pristine;
	for (b = 0; b < SC_BLOCKS; b++) {
		rng_seed(seed + (unsigned)b * 7919u);
		for (i = 0; i < SC_WORDS; i++)
			sc_data[i] = (unsigned short)(rng_next() & 7u);

		ScrambleDataV27(work.obj, sc_data, count);
		for (i = 0; i < n; i++)
			diff_eq_int("scrambled word %ld", (long)sc_data[i],
				    (long)sc_data_ref[b][i],
				    tag * 1000 + b * 100 + i);
		DescrambleDataV27(work.obj, sc_data, count);
	}
	diff_eq_obj("Scramble/DescrambleDataV27 state", struct v27_fixture,
		    &work, &snap, tag);

	for (v = 0; v < SC_VARIANTS; v++) {
		model = pristine;
		markc = 0;
		for (b = 0; b < SC_BLOCKS; b++) {
			rng_seed(seed + (unsigned)b * 7919u);
			for (i = 0; i < SC_WORDS; i++)
				sc_data[i] = (unsigned short)(rng_next() & 7u);

			sc_model(v, &model, 0, sc_data, count);
			markc = sc_mark(markc, &model, sc_data, n);
			sc_model(v, &model, 1, sc_data, count);
			markc = sc_mark(markc, &model, sc_data, n);
		}
		if (v == 0)
			diff_eq_int("Scramble/DescrambleDataV27 model (%ld)",
				    markc == marka, 1, tag);
		else if (markc != marka)
			sc_sep[v]++;
	}
}

static int
run_scramble(void)
{
	static const short counts[] = { 1, 3, 8, SC_WORDS };
	int c;
	long tag = 0;

	diff_begin("Scramble/DescrambleDataV27");

	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		run_sc_one(0x5c000000u + (unsigned)tag, counts[c], tag);
		tag++;
	}

	diff_eq_int("the scrambler's guard inverted a bit (%ld)",
		    sc_invert_trials > 0, 1, sc_invert_trials);
	diff_eq_int("the scrambler's guard run was live (%ld)",
		    sc_run_trials > 0, 1, sc_run_trials);
	for (c = 1; c < SC_VARIANTS; c++)
		diff_eq_int("scrambler wrong reading %ld separates",
			    sc_sep[c] > 0, 1, c);

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 13.  DemodDataV27                                                      */
/*
 * THE ONLY SYMBOL HERE THAT NEEDS A LIVE DSP CHAIN.  `DemodDataV27` calls,
 * in order, `FPM_AGC_agc`, `FPM_MTD_detect`, `FPM_MRF_filter`,
 * `FPM_SRE_recover` and `FPM_FSE_receive`, so a trial needs five constructed
 * objects at their real offsets inside the receiver's block and two scratch
 * buffers big enough for what they produce.
 *
 * D955's rule is why none of them may be faked: this is table-driven code and
 * a field left unplanted that is used as a SUBSCRIPT cannot be caught by a
 * blob-against-blob dry run -- both sides read the same wild index and agree.
 * Every one of the five is constructed by the BLOB's own `_init` on BOTH
 * sides, so the states are identical bytes and any difference belongs here.
 *
 * F8790's rule is why each trial is eight consecutive blocks with the state
 * carried across: the resampler, the recoverer and the equaliser all hold
 * history.
 *
 * THE FIXTURE IS A PAIR, NOT ONE COPY RESTORED.  `FPM_MRF_init`,
 * `FPM_SRE_init` and `FPM_FSE_init` allocate, so the two sides cannot share
 * addresses; the pointer fields they plant are the only bytes the comparison
 * skips and they are named one at a time below.
 *
 * THE V.29 SHAPE IS ONE OF THE NAMED WRONG READINGS.  V.17 and V.29 copy the
 * block, halve it, notch it and hand the COPY to the detector; V.27ter hands
 * over the caller's own buffer (F9115).  `D_MTD_COPY` is that reading, so the
 * structural claim this reconstruction makes is tested rather than asserted.
 */
#define DEM_BUF		2048
#define DEM_BLOCKS	8
#define DEM_TAPS	16
#define DEM_CLK		8
#define DEM_VARIANTS	12

enum dem_defect {
	D_NONE = 0,
	D_MTD_COPY,		/* the V.17/V.29 halved-copy pre-pass       */
	D_NO_ABANDON,		/* a detection does not abandon the call    */
	D_TONE_ALWAYS,		/* the skip-tone gate ignored               */
	D_SRE_FROM_INPUT,	/* the recoverer fed `in`                   */
	D_MRF_TO_BUFB,		/* the resampler writes the wrong buffer    */
	D_FSE_FROM_BUFA,	/* the equaliser fed the resampler's buffer */
	D_FSE_COUNT,		/* the equaliser given the resampler's count */
	D_ADAPT_SOURCE,		/* sre.adapt taken from the wrong enable    */
	D_TILT_NOT_CLEARED,	/* fse.tilt_on left alone                   */
	D_LMS_PLL_SWAPPED,	/* fse.lms_on and fse.pll_on transposed     */
	D_SIGNAL_ONE		/* the carrier bit forced to 1              */
};

struct dem_fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	sh[SH_SIZE];
	short		bufa[DEM_BUF];
	short		bufb[DEM_BUF];
	short		shbuf[DEM_BUF];
	short		acc[MTD_TONES * 2];
	struct fpm_mtd	mtd;
	unsigned char	rx[RX_SIZE];
	double		align;
};

static struct dem_fix dfa, dfb, dfc;

static short dem_icoff[DEM_TAPS], dem_qcoff[DEM_TAPS];
static short dem_clk[DEM_CLK], dem_k1[3], dem_k2[3];
static struct fpm_fse_cfg dem_fse_cfg;
static struct fpm_mtd_cfg dem_mtd_fire, dem_mtd_quiet;

static short dem_in[DEM_BUF], dem_work[DEM_BUF], dem_work_ref[DEM_BUF];

/* What `sre.adapt` holds if nothing wrote it; see the note in run_demod_one. */
#define DEM_ADAPT_SENTINEL	0x5eed5eed
static unsigned short dem_out_a[DEM_BUF], dem_out_b[DEM_BUF];

static long dem_sep[DEM_VARIANTS];
static long dem_agc_ident;	/* trials proving %eax == agc.signal        */
static long dem_abandon_trials;	/* the tone test abandoned the call         */
static long dem_run_trials;	/* ... and did not                          */
static long dem_skip_trials;	/* the tone test was skipped entirely       */
static long dem_signal0_trials;	/* the carrier bit came back 0              */
static long dem_signal1_trials;	/* ... and 1                                */
static long dem_violation_trials;/* the SRE buffer violation was printed    */
static int  dem_capture;	/* compare the two transcripts this trial   */
static long dem_lms_trials;	/* fse.lms_on came out non-zero             */
static long dem_adapt_trials;	/* sre.adapt came out non-zero              */

/*
 * WHY THE WHOLE TRANSCRIPT CANNOT BE COMPARED, and it is the apparatus and
 * not the code (F9120).  `FPM_FSE_receive`'s "Decoder Error" report is gated
 * on a counter that lives OUTSIDE `struct fpm_fse` -- one static per side --
 * and this trial replays the blob's modules many more times than the
 * reconstruction's, so the two statics are far out of step by the time the
 * debug arm runs.  A whole-transcript `strcmp` therefore measures how often
 * each side has been called, not what `DemodDataV27` printed.
 *
 * So the comparison is narrowed to THIS function's own line, extracted by its
 * prefix.  The blob's format string is `.rodata.str1.4 + 0x12ca0`, which is
 * "ERROR: SRE buffer violation(%d)" with no newline, so successive reports run
 * together and the extractor has to close on the ')'.
 */
#define DEM_REPORT	"ERROR: SRE buffer violation("

static int
dem_reports(const char *src, char *dst, int cap)
{
	int n = 0, len = 0;
	size_t k = strlen(DEM_REPORT);

	while (*src != '\0') {
		if (strncmp(src, DEM_REPORT, k) == 0) {
			n++;
			while (*src != '\0' && *src != ')') {
				if (len + 1 < cap)
					dst[len++] = *src;
				src++;
			}
			continue;
		}
		src++;
	}
	if (cap > 0)
		dst[len < cap ? len : cap - 1] = '\0';
	return n;
}

static char dem_rep_a[512], dem_rep_b[512];

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
	dem_fse_cfg.block = DEM_BUF;
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

	/*
	 * Two detectors over the SAME real biquad bank: one whose ratio and
	 * minimum level make it fire on anything, one whose minimum level
	 * makes it fire on nothing.  The bank has to be real -- a null
	 * `cfg.coeff` is a fault inside `FPM_iir_filt`, not a wrong answer.
	 */
	dem_mtd_fire = dcd_mtd_cfg;
	dem_mtd_fire.ratio = -1;
	dem_mtd_fire.min_level = 0;
	dem_mtd_quiet = dcd_mtd_cfg;
	dem_mtd_quiet.min_level = (short)0x7fff;
}

/*
 * THE THREE ENABLE WORDS MUST DIFFER IN BIT 0 AND NOWHERE ELSE MATTERS.
 * `agc.signal` is a `setg` result, so it is 0 or 1 and `signal & word` can
 * only be 0 or `word & 1`.  Two shapes, each with one word bit-0-clear, so
 * every pairing of source and destination is distinguishable in one of them.
 */
static const int dem_enable[2][3] = {
	{ 0x0e, 0x33, 0x54 },		/* adapt off, pll on,  lms off */
	{ 0x33, 0x54, 0x0f }		/* adapt on,  pll off, lms on  */
};

struct dem_setup {
	unsigned short	rx_state;	/* sh V27SH_RX_STATE; non-zero is
					 * "past START", which is what
					 * skips the tone test  (F9300) */
	int		mtd_fires;
	int		enables;
};

static int
dem_skip_rx(int off)
{
	if (off >= V27RX_BUF_A && off < V27RX_BUF_B + 4)
		return 1;
	if (off >= V27RX_MRF + 0x18 && off < V27RX_MRF + 0x1c)
		return 1;			/* fpm_mrf::history          */
	if (off >= V27RX_SRE + 0x50 && off < V27RX_SRE + 0x5c)
		return 1;			/* coeff, hist, clk          */
	if (off >= V27RX_SRE + 0x74 && off < V27RX_SRE + 0x78)
		return 1;			/* rms_buf                   */
	if (off >= V27RX_FSE + 0x54 && off < V27RX_FSE + 0x5c)
		return 1;			/* out_i, out_q              */
	if (off >= V27RX_FSE + 0x60 && off < V27RX_FSE + 0x6c)
		return 1;			/* icoeff, qcoeff, hist      */
	return 0;
}

static int
dem_skip_mtd(int off)
{
	return off >= 0x0c && off < 0x10;	/* fpm_mtd::acc */
}

static int
dem_skip_sh(int off)
{
	if (off >= V27SH_MTD && off < V27SH_MTD + 4)
		return 1;
	if (off >= V27SH_BUF && off < V27SH_BUF + 4)
		return 1;
	return 0;
}

static int
dem_skip_obj(int off)
{
	if (off >= V27_OBJ_SHARED && off < V27_OBJ_RX + 4)
		return 1;
	return 0;
}

static long
dem_first_diff(const unsigned char *a, const unsigned char *b, int n,
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
dem_build(struct dem_fix *f, unsigned seed, const struct dem_setup *u)
{
	int i;

	memset(f, 0, sizeof(*f));
	rng_seed(seed);
	rng_fill(f->obj, OBJ_SIZE);
	rng_fill(f->sh, SH_SIZE);
	rng_fill(f->rx, RX_SIZE);
	for (i = 0; i < DEM_BUF; i++) {
		f->bufa[i] = (short)(rng_next() & 0x7fff);
		f->bufb[i] = (short)(rng_next() & 0x7fff);
		f->shbuf[i] = (short)(rng_next() & 0x7fff);
	}

	*(void **)(void *)(f->obj + V27_OBJ_SHARED) = f->sh;
	*(void **)(void *)(f->obj + V27_OBJ_RX) = f->rx;
	*(void **)(void *)(f->rx + V27RX_BUF_A) = f->bufa;
	*(void **)(void *)(f->rx + V27RX_BUF_B) = f->bufb;
	*(void **)(void *)(f->sh + V27SH_BUF) = f->shbuf;

	for (i = 0; i < MTD_TONES * 2; i++)
		f->acc[i] = 0;
	f->mtd.cfg = u->mtd_fires ? dem_mtd_fire : dem_mtd_quiet;
	f->mtd.acc = f->acc;
	f->mtd.dc_state[0] = 0;
	f->mtd.dc_state[1] = 0;
	f->mtd.out_of_band = 0;
	f->mtd.wideband = 0;
	*(void **)(void *)(f->sh + V27SH_MTD) = &f->mtd;
	*(unsigned short *)(void *)(f->sh + V27SH_RX_STATE) = u->rx_state;

	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->rx + V27RX_AGC),
			 &dcd_agc_cfg, 1);
	ref_FPM_MRF_init((struct fpm_mrf *)(void *)(f->rx + V27RX_MRF),
			 &MRFv32_CFG, 1);
	ref_FPM_SRE_init((struct fpm_sre *)(void *)(f->rx + V27RX_SRE),
			 &SREv32_CFG, 1);
	ref_FPM_FSE_init((struct fpm_fse *)(void *)(f->rx + V27RX_FSE),
			 &dem_fse_cfg, 1);

	*(int *)(void *)(f->rx + V27RX_EN_SRE_ADAPT) = dem_enable[u->enables][0];
	*(int *)(void *)(f->rx + V27RX_EN_FSE_PLL) = dem_enable[u->enables][1];
	*(int *)(void *)(f->rx + V27RX_EN_FSE_LMS) = dem_enable[u->enables][2];
}

static void
dem_free(struct dem_fix *f)
{
	ref_FPM_MRF_free((struct fpm_mrf *)(void *)(f->rx + V27RX_MRF));
	ref_FPM_SRE_free((struct fpm_sre *)(void *)(f->rx + V27RX_SRE));
	ref_FPM_FSE_free((struct fpm_fse *)(void *)(f->rx + V27RX_FSE));
}

static struct fpm_agc *
dem_agc(struct dem_fix *f)
{
	return (struct fpm_agc *)(void *)(f->rx + V27RX_AGC);
}

static struct fpm_fse *
dem_fse(struct dem_fix *f)
{
	return (struct fpm_fse *)(void *)(f->rx + V27RX_FSE);
}

static struct fpm_sre *
dem_sre(struct dem_fix *f)
{
	return (struct fpm_sre *)(void *)(f->rx + V27RX_SRE);
}

/* The object's own sequence, with one reading changed. */
static unsigned short
drive_demod(int v, struct dem_fix *f, short *in, unsigned short *bits,
	    unsigned short count)
{
	unsigned char *rx = f->rx;
	unsigned char *sh = f->sh;
	int signal;
	unsigned short n, m;

	/*
	 * THE IDENTITY BEHIND D1094 IS NOT RE-MEASURED HERE, DELIBERATELY.
	 * The apparatus that would measure it -- calling the blob's `void`
	 * `FPM_AGC_agc` through a cast that returns `int`, which is what the
	 * object's own call site did -- is `t_v29fax.c`'s `run_agc_identity`,
	 * and that is WITHDRAWN under F9001 because it segfaults under the
	 * period compiler for a reason nobody has established.  Reinstating
	 * the same construct in a second file would reinstate the same
	 * unexplained fault, so this model reads the field exactly as `src/`
	 * does and D1094 records that the equivalence rests on the argument
	 * about `FPM_AGC_agc`'s single epilogue rather than on a measurement
	 * here.
	 */
	ref_FPM_AGC_agc(dem_agc(f), in, count);
	signal = dem_agc(f)->signal;
	dem_agc_ident++;
	if (v == D_SIGNAL_ONE)
		signal = 1;

	if (*(unsigned short *)(void *)(sh + V27SH_RX_STATE) == 0
	    || v == D_TONE_ALWAYS) {
		const short *probe = in;

		if (v == D_MTD_COPY) {
			short *buf = (short *)
				*(void **)(void *)(sh + V27SH_BUF);
			unsigned short i;

			for (i = 0; i < count; i++)
				buf[i] = (short)(in[i] >> 1);
			probe = buf;
		}
		if (ref_FPM_MTD_detect((struct fpm_mtd *)
					*(void **)(void *)(sh + V27SH_MTD),
				       probe, (short)count) != 0
		    && v != D_NO_ABANDON)
			return 0;
	}

	n = (unsigned short)ref_FPM_MRF_filter(
			(struct fpm_mrf *)(void *)(rx + V27RX_MRF), in,
			(short *)*(void **)(void *)
				(rx + (v == D_MRF_TO_BUFB ? V27RX_BUF_B
							  : V27RX_BUF_A)),
			(short)count);

	dem_sre(f)->adapt = signal & *(int *)(void *)
		(rx + (v == D_ADAPT_SOURCE ? V27RX_EN_FSE_PLL
					   : V27RX_EN_SRE_ADAPT));

	m = ref_FPM_SRE_recover(dem_sre(f),
			(const short *)(v == D_SRE_FROM_INPUT ? (void *)in
				: *(void **)(void *)(rx + V27RX_BUF_A)),
			(short *)*(void **)(void *)(rx + V27RX_BUF_B),
			(short)n);

	if (v != D_TILT_NOT_CLEARED)
		dem_fse(f)->tilt_on = 0;
	dem_fse(f)->lms_on = signal & *(int *)(void *)
		(rx + (v == D_LMS_PLL_SWAPPED ? V27RX_EN_FSE_PLL
					      : V27RX_EN_FSE_LMS));
	dem_fse(f)->pll_on = signal & *(int *)(void *)
		(rx + (v == D_LMS_PLL_SWAPPED ? V27RX_EN_FSE_LMS
					      : V27RX_EN_FSE_PLL));

	return ref_FPM_FSE_receive(dem_fse(f),
			(const short *)*(void **)(void *)
				(rx + (v == D_FSE_FROM_BUFA ? V27RX_BUF_A
							    : V27RX_BUF_B)),
			bits, v == D_FSE_COUNT ? n : m);
}

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

static void
run_demod_one(unsigned seed, const struct dem_setup *u, int level,
	      unsigned short count, long where)
{
	unsigned long marka = 0, markc;
	int blk, v, i;

	dem_signal(seed ^ 0x0b10cced, level);

	dem_build(&dfa, seed, u);
	dem_build(&dfb, seed, u);

	for (blk = 0; blk < DEM_BLOCKS; blk++) {
		unsigned short ra, rb;
		long id = where * 100 + blk;

		for (i = 0; i < DEM_BUF; i++) {
			dem_out_a[i] = dem_out_b[i] = 0xbeef;
			dem_work[i] = dem_in[i];
		}
		/*
		 * The sentinel is planted on EVERY side of every block, so it
		 * is not a perturbation of one of them: `sre.adapt` is written
		 * before it is read on every path that reaches the recoverer,
		 * so what survives is exactly the abandoning path.  Read FROM
		 * THE REFERENCE, which is F134's rule.
		 */
		dem_sre(&dfa)->adapt = DEM_ADAPT_SENTINEL;
		dem_sre(&dfb)->adapt = DEM_ADAPT_SENTINEL;
		if (dem_capture)
			dsplib_debug_capture_reset();
		ra = ref_DemodDataV27(dfa.obj, dem_work, dem_out_a, count);
		for (i = 0; i < DEM_BUF; i++) {
			dem_work_ref[i] = dem_work[i];
			dem_work[i] = dem_in[i];
		}
		rb = DemodDataV27(dfb.obj, dem_work, dem_out_b, count);

		if (dem_capture) {
			int na = dem_reports(dsplib_debug_capture_text(1),
					     dem_rep_a, (int)sizeof dem_rep_a);
			int nb = dem_reports(dsplib_debug_capture_text(0),
					     dem_rep_b, (int)sizeof dem_rep_b);

			diff_eq_int("at %ld: DemodDataV27 report count",
				    (long)nb, (long)na, id);
			diff_eq_int("at %ld: DemodDataV27 report text",
				    strcmp(dem_rep_b, dem_rep_a) == 0, 1, id);
			dem_violation_trials += na;
		}
		diff_eq_int("at %ld: DemodDataV27 returned", (long)rb,
			    (long)ra, id);
		diff_eq_int("at %ld: the return fits the buffer",
			    ra < DEM_BUF, 1, id);
		if (ra >= DEM_BUF)
			break;
		for (i = 0; i < (int)ra; i++)
			diff_eq_int("word %ld", (long)dem_out_b[i],
				    (long)dem_out_a[i], i);
		diff_eq_int("at %ld: nothing past the returned count",
			    dem_out_a[ra] == 0xbeef, 1, id);
		diff_eq_int("at %ld: first differing receiver byte",
			    dem_first_diff(dfb.rx, dfa.rx, RX_SIZE,
					   dem_skip_rx), -1, id);
		diff_eq_int("at %ld: first differing shared byte",
			    dem_first_diff(dfb.sh, dfa.sh, SH_SIZE,
					   dem_skip_sh), -1, id);
		diff_eq_int("at %ld: first differing instance byte",
			    dem_first_diff(dfb.obj, dfa.obj, OBJ_SIZE,
					   dem_skip_obj), -1, id);
		diff_eq_int("at %ld: the tone detector",
			    dem_first_diff((unsigned char *)&dfb.mtd,
					   (unsigned char *)&dfa.mtd,
					   (int)sizeof dfa.mtd, dem_skip_mtd),
			    -1, id);
		diff_eq_int("at %ld: the tone detector's accumulators",
			    dem_first_diff((unsigned char *)dfb.acc,
					   (unsigned char *)dfa.acc,
					   (int)sizeof dfa.acc, 0), -1, id);
		diff_eq_int("at %ld: the resampler's buffer",
			    dem_first_diff((unsigned char *)dfb.bufa,
					   (unsigned char *)dfa.bufa,
					   DEM_BUF * 2, 0), -1, id);
		diff_eq_int("at %ld: the recoverer's buffer",
			    dem_first_diff((unsigned char *)dfb.bufb,
					   (unsigned char *)dfa.bufb,
					   DEM_BUF * 2, 0), -1, id);
		diff_eq_int("at %ld: the shared buffer, which V.27ter never"
			    " writes",
			    dem_first_diff((unsigned char *)dfb.shbuf,
					   (unsigned char *)dfa.shbuf,
					   DEM_BUF * 2, 0), -1, id);
		diff_eq_int("at %ld: the caller's samples, rewritten in place"
			    " by the gain control",
			    dem_first_diff((unsigned char *)dem_work,
					   (unsigned char *)dem_work_ref,
					   DEM_BUF * 2, 0), -1, id);

		/*
		 * The mark carries the three flag words as well as the output:
		 * `sre.adapt`, `fse.pll_on`, `fse.lms_on` and `fse.tilt_on` do
		 * not reach the samples on a single block, so four of the
		 * named wrong readings separate nothing without them.
		 */
		marka = marka * 1000003u + ra;
		for (i = 0; i < (int)ra; i++)
			marka = marka * 31u + dem_out_a[i];
		marka = marka * 131u + (unsigned long)(unsigned)
				dem_sre(&dfa)->adapt;
		marka = marka * 131u + (unsigned long)(unsigned)
				dem_fse(&dfa)->pll_on;
		marka = marka * 131u + (unsigned long)(unsigned)
				dem_fse(&dfa)->lms_on;
		marka = marka * 131u + (unsigned long)(unsigned)
				dem_fse(&dfa)->tilt_on;
		for (i = 0; i < DEM_BUF; i++)
			marka = marka * 31u + (unsigned short)dfa.bufa[i];
		for (i = 0; i < DEM_BUF; i++)
			marka = marka * 31u + (unsigned short)dfa.bufb[i];
		for (i = 0; i < DEM_BUF; i++)
			marka = marka * 31u + (unsigned short)dfa.shbuf[i];

		if (u->rx_state != 0)
			dem_skip_trials++;
		else if (dem_sre(&dfa)->adapt == DEM_ADAPT_SENTINEL)
			dem_abandon_trials++;
		else
			dem_run_trials++;
		if (dem_agc(&dfa)->signal == 0)
			dem_signal0_trials++;
		else
			dem_signal1_trials++;
		if (dem_fse(&dfa)->lms_on != 0)
			dem_lms_trials++;
		if (dem_sre(&dfa)->adapt != 0)
			dem_adapt_trials++;
	}

	dem_free(&dfa);
	dem_free(&dfb);

	/*
	 * The variant replay runs the BLOB's modules only, so it would fill
	 * side 1's transcript and none of side 0's.  Capture is off for it.
	 */
	dsplib_debug_capture_on = 0;
	for (v = 0; v < DEM_VARIANTS; v++) {
		dem_build(&dfc, seed, u);
		markc = 0;
		for (blk = 0; blk < DEM_BLOCKS; blk++) {
			unsigned short rc;

			for (i = 0; i < DEM_BUF; i++) {
				dem_out_b[i] = 0xbeef;
				dem_work[i] = dem_in[i];
			}
			dem_sre(&dfc)->adapt = DEM_ADAPT_SENTINEL;
			rc = drive_demod(v, &dfc, dem_work, dem_out_b, count);
			markc = markc * 1000003u + rc;
			if (rc < DEM_BUF)
				for (i = 0; i < (int)rc; i++)
					markc = markc * 31u + dem_out_b[i];
			markc = markc * 131u + (unsigned long)(unsigned)
					dem_sre(&dfc)->adapt;
			markc = markc * 131u + (unsigned long)(unsigned)
					dem_fse(&dfc)->pll_on;
			markc = markc * 131u + (unsigned long)(unsigned)
					dem_fse(&dfc)->lms_on;
			markc = markc * 131u + (unsigned long)(unsigned)
					dem_fse(&dfc)->tilt_on;
			for (i = 0; i < DEM_BUF; i++)
				markc = markc * 31u +
					(unsigned short)dfc.bufa[i];
			for (i = 0; i < DEM_BUF; i++)
				markc = markc * 31u +
					(unsigned short)dfc.bufb[i];
			for (i = 0; i < DEM_BUF; i++)
				markc = markc * 31u +
					(unsigned short)dfc.shbuf[i];
		}
		if (v == 0)
			diff_eq_int("DemodDataV27 model (%ld)", markc == marka,
				    1, where);
		else if (markc != marka)
			dem_sep[v]++;
		dem_free(&dfc);
	}
	dsplib_debug_capture_on = dem_capture;
}

static int
run_demod(void)
{
	static const unsigned short counts[] = { 32, 160, 700 };
	int gate, fires, enables, level, c, v;
	long where = 0;

	dcd_cfg_init();
	dem_tables();
	diff_begin("DemodDataV27");

	for (gate = 0; gate < 2; gate++)
	for (fires = 0; fires < 2; fires++)
	for (enables = 0; enables < 2; enables++)
	for (level = 0; level < 3; level++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		struct dem_setup u;

		u.rx_state = (unsigned short)(gate ? 0x1234 : 0);
		u.mtd_fires = fires;
		u.enables = enables;
		run_demod_one(0x0de70000u + (unsigned)where, &u, level,
			      counts[c], where);
		where++;
	}

	/*
	 * The debug arm, run separately: the "SRE buffer violation" report is
	 * the only output of the threshold at 0xa4, and it exists only above
	 * debug level 1.
	 */
	{
		struct dem_setup u;

		u.rx_state = 0x1234;
		u.mtd_fires = 0;
		u.enables = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 2;
		dem_capture = 1;
		dsplib_debug_capture_on = 1;
		run_demod_one(0x0de7dbeeu, &u, 2, 700, where++);
		dsplib_debug_capture_on = 0;
		dem_capture = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		diff_eq_int("DemodDataV27 reported an SRE buffer violation"
			    " (%ld)", dem_violation_trials > 0, 1,
			    dem_violation_trials);
	}

	diff_eq_int("DemodDataV27 measured the AGC identity (%ld)",
		    dem_agc_ident > 0, 1, dem_agc_ident);
	diff_eq_int("DemodDataV27 abandoned on a tone (%ld)",
		    dem_abandon_trials > 0, 1, dem_abandon_trials);
	diff_eq_int("DemodDataV27 ran the whole chain (%ld)",
		    dem_run_trials > 0, 1, dem_run_trials);
	diff_eq_int("DemodDataV27 skipped the tone test (%ld)",
		    dem_skip_trials > 0, 1, dem_skip_trials);
	diff_eq_int("the carrier bit was 0 (%ld)", dem_signal0_trials > 0, 1,
		    dem_signal0_trials);
	diff_eq_int("the carrier bit was 1 (%ld)", dem_signal1_trials > 0, 1,
		    dem_signal1_trials);
	diff_eq_int("fse.lms_on came out set (%ld)", dem_lms_trials > 0, 1,
		    dem_lms_trials);
	diff_eq_int("sre.adapt came out set (%ld)", dem_adapt_trials > 0, 1,
		    dem_adapt_trials);
	for (v = 1; v < DEM_VARIANTS; v++)
		diff_eq_int("demodulator wrong reading %ld separates",
			    dem_sep[v] > 0, 1, v);

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 14.  V27TX_delete                                                      */
/*
 * The same shape as `V27RX_delete` and with the same limitation stated in the
 * file header: the harness's allocator records what is LIVE, not the sequence
 * of frees, so the ORDER is taken from the disassembly alone.  What is checked
 * is the SET of pointers released, the multiplicity, that nothing unknown was
 * freed, and that six named wrong readings each change one of those.
 *
 * The three sub-deletes are the blob's own -- `FPM_PPS_free`, `FIFO_delete`
 * and `SGD_delete` -- reached through this reconstruction, so what this test
 * measures about them is only that the right pointer arrived.
 */
#define TXDEL_PTRS	10
#define TXBLK_SIZE	0xa0

struct txdel_build {
	void	*modem;
	void	*tx;
	void	*src;
	void	*ptr[TXDEL_PTRS];
	int	nptr;
};

static void *
txdel_alloc(struct txdel_build *b, unsigned size)
{
	void *p = sysdep_malloc(size);

	if (b->nptr < TXDEL_PTRS)
		b->ptr[b->nptr++] = p;
	return p;
}

static void
txdel_build(struct txdel_build *b)
{
	struct fpm_pps *pps;
	struct fax_fifo *fifo;
	struct sgd *sgd;
	int i;

	b->nptr = 0;

	b->modem = txdel_alloc(b, OBJ_SIZE);
	memset(b->modem, 0, OBJ_SIZE);
	b->tx = txdel_alloc(b, TXBLK_SIZE);
	memset(b->tx, 0, TXBLK_SIZE);
	b->src = txdel_alloc(b, 0x20);
	memset(b->src, 0, 0x20);

	FXP(b->modem, V27_OBJ_TX) = b->tx;
	FXP(b->modem, V27_OBJ_TXDATA) = b->src;

	pps = (struct fpm_pps *)(void *)FX(b->tx, V27TX_PPS);
	pps->hist_i = (short *)txdel_alloc(b, 62);
	pps->hist_q = (short *)txdel_alloc(b, 64);

	((struct fpm_smc_ring *)(void *)FX(b->tx, V27TX_RING))->sym =
			(short *)txdel_alloc(b, 66);

	fifo = (struct fax_fifo *)txdel_alloc(b, sizeof(struct fax_fifo));
	memset(fifo, 0, sizeof(*fifo));
	fifo->buf = (unsigned short *)txdel_alloc(b, 68);
	FXP(b->src, V27TXD_FIFO) = fifo;

	sgd = (struct sgd *)txdel_alloc(b, sizeof(struct sgd));
	memset(sgd, 0, sizeof(*sgd));
	sgd->hist = (unsigned short *)txdel_alloc(b, 70);
	FXP(b->src, V27TXD_SGD) = sgd;

	for (i = 0; i < b->nptr; i++) {
		if (b->ptr[i] == 0)
			diff_eq_int("tx delete fixture allocated (%ld)", 0, 1,
				    (long)i);
	}
	if (b->nptr != TXDEL_PTRS)
		diff_eq_int("tx delete fixture planted every pointer (%ld)",
			    b->nptr, TXDEL_PTRS, 0);
}

static unsigned long
txdel_live(const struct txdel_build *b)
{
	unsigned long m = 0;
	int i;

	for (i = 0; i < b->nptr; i++)
		if (harness_alloc_ordinal(b->ptr[i]) != 0)
			m |= 1UL << i;
	return m;
}

static long txdel_sep[7];

static int
run_txdelete(void)
{
	struct txdel_build b;
	unsigned long live_ref, live_ours, live_bad;
	int frees_ref, bad_ref, frees_ours, bad_ours;
	int v;

	diff_begin("V27TX_delete");

	harness_alloc_reset();
	txdel_build(&b);
	frees_ref = harness_alloc.frees;
	bad_ref = harness_alloc.bad_free;
	ref_V27TX_delete(b.modem);
	live_ref = txdel_live(&b);
	frees_ref = harness_alloc.frees - frees_ref;
	bad_ref = harness_alloc.bad_free - bad_ref;

	diff_eq_int("the blob released everything (%ld)", (long)live_ref, 0, 0);
	diff_eq_int("the blob freed each pointer once (%ld)", frees_ref,
		    TXDEL_PTRS, 0);
	diff_eq_int("the blob freed nothing unknown (%ld)", bad_ref, 0, 0);

	harness_alloc_reset();
	txdel_build(&b);
	frees_ours = harness_alloc.frees;
	bad_ours = harness_alloc.bad_free;
	V27TX_delete(b.modem);
	live_ours = txdel_live(&b);
	frees_ours = harness_alloc.frees - frees_ours;
	bad_ours = harness_alloc.bad_free - bad_ours;

	diff_eq_int("V27TX_delete live set (%ld)", (long)live_ours,
		    (long)live_ref, 0);
	diff_eq_int("V27TX_delete free count (%ld)", frees_ours, frees_ref, 0);
	diff_eq_int("V27TX_delete bad frees (%ld)", bad_ours, bad_ref, 0);
	diff_eq_int("V27TX_delete left nothing live (%ld)", harness_alloc.live,
		    0, 0);

	/*
	 *   1  the symbol ring's buffer not released
	 *   2  the transmitter block not released
	 *   3  the data-source block not released
	 *   4  the FIFO released and the `sgd` left alone
	 *   5  the instance itself not released
	 *   6  the pulse shaper's two histories not released
	 */
	for (v = 1; v <= 6; v++) {
		int f0, bf0;

		harness_alloc_reset();
		txdel_build(&b);
		f0 = harness_alloc.frees;
		bf0 = harness_alloc.bad_free;

		if (v != 6)
			FPM_PPS_free((struct fpm_pps *)(void *)
					FX(b.tx, V27TX_PPS));
		if (v != 1)
			sysdep_free(((struct fpm_smc_ring *)(void *)
					FX(b.tx, V27TX_RING))->sym);
		if (v != 2)
			sysdep_free(b.tx);
		FIFO_delete((struct fax_fifo *)FXP(b.src, V27TXD_FIFO));
		if (v != 4)
			SGD_delete((struct sgd *)FXP(b.src, V27TXD_SGD));
		if (v != 3)
			sysdep_free(b.src);
		if (v != 5)
			sysdep_free(b.modem);

		live_bad = txdel_live(&b);
		if (live_bad != live_ref
		    || harness_alloc.frees - f0 != frees_ref
		    || harness_alloc.bad_free - bf0 != bad_ref)
			txdel_sep[v]++;

		harness_alloc_reset();
	}

	for (v = 1; v <= 6; v++)
		diff_eq_int("V27TX_delete wrong reading %ld separates",
			    txdel_sep[v] > 0, 1, v);

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 15.  ModDataV27                                                        */
/*
 * Two calls over one shared ring, so what can be wrong is WHICH ring each end
 * of the pair is given and WHICH count.  The fixture therefore carries a
 * SECOND, differently-seeded ring with its own three buffers: a wrong reading
 * then writes or reads a live ring and produces a wrong answer, rather than
 * building a pointer out of two cursors and faulting.
 *
 * Both modules are constructed by the BLOB's own `_init` on both sides
 * (D955), and each trial is MOD_BLOCKS consecutive blocks because the ring's
 * two cursors, the shaper's phase and both its histories carry across (F8790).
 *
 * Variants:
 *   0  the reading this reconstruction claims
 *   1  the encoder writes the other ring
 *   2  the shaper reads the other ring
 *   3  the shaper given one symbol fewer
 *   4  the two calls in the other order
 *   5  the encoder not called at all
 */
#define MOD_RING	64
#define MOD_BUF		1024
#define MOD_BLOCKS	8
#define MOD_VARIANTS	6

struct mod_fix {
	unsigned char		obj[OBJ_SIZE];
	unsigned char		tx[TXBLK_SIZE];
	short			ri[MOD_RING], rq[MOD_RING], rs[MOD_RING];
	short			ai[MOD_RING], aq[MOD_RING], as[MOD_RING];
	struct fpm_smc_ring	alt;
	double			align;
};

static struct mod_fix mfa, mfb, mfc;

static unsigned short mod_bits[MOD_BUF];
static short mod_out_a[MOD_BUF], mod_out_b[MOD_BUF];

static long mod_sep[MOD_VARIANTS];
static long mod_wrap_trials;	/* the ring's write cursor wrapped         */
static long mod_out_trials;	/* the shaper produced something           */

static struct fpm_smc_ring *
mod_ring(struct mod_fix *f)
{
	return (struct fpm_smc_ring *)(void *)FX(f->tx, V27TX_RING);
}

static struct fpm_smc *
mod_smc(struct mod_fix *f)
{
	return (struct fpm_smc *)(void *)FX(f->tx, V27TX_SMC);
}

static struct fpm_pps *
mod_pps(struct mod_fix *f)
{
	return (struct fpm_pps *)(void *)FX(f->tx, V27TX_PPS);
}

/*
 * The bytes a comparison must skip, and only those: the ring's three buffer
 * pointers, which are per-fixture addresses, and the shaper's two histories,
 * which each side's `FPM_PPS_init` allocated for itself.  The buffers those
 * pointers name ARE compared, one array at a time, below.
 */
static int
mod_skip_tx(int off)
{
	if (off >= V27TX_RING && off < V27TX_RING + 0x0c)
		return 1;
	return off >= V27TX_PPS + 0x30 && off < V27TX_PPS + 0x38;
}

static int
mod_skip_obj(int off)
{
	return off >= V27_OBJ_TX && off < V27_OBJ_TX + 4;
}

static void
mod_ring_init(struct fpm_smc_ring *r, short *i, short *q, short *sym)
{
	r->i = i;
	r->q = q;
	r->sym = sym;
	r->widx = 0;
	r->ridx = 0;
	r->len = MOD_RING;
}

static void
mod_build(struct mod_fix *f, unsigned seed)
{
	int i;

	memset(f, 0, sizeof(*f));
	rng_seed(seed);
	rng_fill(f->obj, OBJ_SIZE);
	rng_fill(f->tx, TXBLK_SIZE);
	for (i = 0; i < MOD_RING; i++) {
		f->ri[i] = (short)rng_next();
		f->rq[i] = (short)rng_next();
		f->rs[i] = (short)(rng_next() & 3u);
		f->ai[i] = (short)rng_next();
		f->aq[i] = (short)rng_next();
		f->as[i] = (short)(rng_next() & 3u);
	}

	FXP(f->obj, V27_OBJ_TX) = f->tx;
	mod_ring_init(mod_ring(f), f->ri, f->rq, f->rs);
	mod_ring_init(&f->alt, f->ai, f->aq, f->as);

	ref_SMC_init(mod_smc(f), &SMC_CFG);
	ref_FPM_PPS_init(mod_pps(f), &PPSv32_CFG, 1);
}

static void
mod_free(struct mod_fix *f)
{
	ref_FPM_PPS_free(mod_pps(f));
}

/* The object's own pair, with one reading changed. */
static unsigned short
mod_model(int v, struct mod_fix *f, const unsigned short *bits, short *out,
	  unsigned short count)
{
	struct fpm_smc_ring *enc = (v == 1) ? &f->alt : mod_ring(f);
	struct fpm_smc_ring *shp = (v == 2) ? &f->alt : mod_ring(f);
	unsigned short n = (v == 3) ? (unsigned short)(count - 1) : count;
	unsigned short r;

	if (v == 4) {
		r = ref_FPM_PPS_filter(mod_pps(f), shp, out, n);
		ref_SMC_encoder(mod_smc(f), enc, bits, count);
		return r;
	}
	if (v != 5)
		ref_SMC_encoder(mod_smc(f), enc, bits, count);
	return ref_FPM_PPS_filter(mod_pps(f), shp, out, n);
}

static unsigned long
mod_mark(unsigned long m, struct mod_fix *f, const short *out, int n,
	 unsigned short ret)
{
	int i;

	m = m * 1000003u + ret;
	for (i = 0; i < n; i++)
		m = m * 31u + (unsigned short)out[i];
	for (i = 0; i < MOD_RING; i++) {
		m = m * 31u + (unsigned short)f->ri[i];
		m = m * 31u + (unsigned short)f->rq[i];
		m = m * 31u + (unsigned short)f->rs[i];
		m = m * 31u + (unsigned short)f->ai[i];
		m = m * 31u + (unsigned short)f->aq[i];
		m = m * 31u + (unsigned short)f->as[i];
	}
	m = m * 131u + (unsigned short)mod_ring(f)->widx;
	m = m * 131u + (unsigned short)mod_ring(f)->ridx;
	m = m * 131u + (unsigned short)f->alt.widx;
	m = m * 131u + (unsigned short)f->alt.ridx;
	m = m * 131u + (unsigned short)mod_pps(f)->phase;
	m = m * 131u + (unsigned short)mod_pps(f)->widx;
	m = m * 131u + (unsigned short)mod_pps(f)->need;
	return m;
}

static void
run_mod_one(unsigned seed, unsigned short count, long tag)
{
	unsigned long marka = 0, markc;
	int blk, v, i;

	mod_build(&mfa, seed);
	mod_build(&mfb, seed);

	for (blk = 0; blk < MOD_BLOCKS; blk++) {
		unsigned short ra, rb;
		long id = tag * 100 + blk;
		short widx0 = mod_ring(&mfa)->widx;

		rng_seed(seed + (unsigned)blk * 104729u);
		for (i = 0; i < MOD_BUF; i++) {
			mod_bits[i] = (unsigned short)(rng_next() & 7u);
			mod_out_a[i] = mod_out_b[i] = (short)0x5ead;
		}

		ra = ref_ModDataV27(mfa.obj, mod_bits, mod_out_a, count);
		rb = ModDataV27(mfb.obj, mod_bits, mod_out_b, count);

		diff_eq_int("at %ld: ModDataV27 returned", (long)rb, (long)ra,
			    id);
		diff_eq_int("at %ld: the return fits the buffer", ra < MOD_BUF,
			    1, id);
		if (ra >= MOD_BUF)
			break;
		for (i = 0; i < (int)ra; i++)
			diff_eq_int("sample %ld", (long)mod_out_b[i],
				    (long)mod_out_a[i], i);
		diff_eq_int("at %ld: nothing past the returned count",
			    mod_out_a[ra] == (short)0x5ead, 1, id);
		diff_eq_int("at %ld: first differing transmitter byte",
			    dem_first_diff(mfb.tx, mfa.tx, TXBLK_SIZE,
					   mod_skip_tx), -1, id);
		diff_eq_int("at %ld: first differing instance byte",
			    dem_first_diff(mfb.obj, mfa.obj, OBJ_SIZE,
					   mod_skip_obj), -1, id);
		diff_eq_int("at %ld: the symbol ring",
			    dem_first_diff((unsigned char *)mfb.rs,
					   (unsigned char *)mfa.rs,
					   MOD_RING * 2, 0), -1, id);
		diff_eq_int("at %ld: the ring's I rail",
			    dem_first_diff((unsigned char *)mfb.ri,
					   (unsigned char *)mfa.ri,
					   MOD_RING * 2, 0), -1, id);
		diff_eq_int("at %ld: the ring's Q rail",
			    dem_first_diff((unsigned char *)mfb.rq,
					   (unsigned char *)mfa.rq,
					   MOD_RING * 2, 0), -1, id);
		diff_eq_int("at %ld: the decoy ring, which nothing may touch",
			    dem_first_diff((unsigned char *)mfb.as,
					   (unsigned char *)mfa.as,
					   MOD_RING * 2, 0), -1, id);

		if (mod_ring(&mfa)->widx < widx0)
			mod_wrap_trials++;
		if (ra > 0)
			mod_out_trials++;
		marka = mod_mark(marka, &mfa, mod_out_a, (int)ra, ra);
	}

	mod_free(&mfa);
	mod_free(&mfb);

	for (v = 0; v < MOD_VARIANTS; v++) {
		mod_build(&mfc, seed);
		markc = 0;
		for (blk = 0; blk < MOD_BLOCKS; blk++) {
			unsigned short rc;

			rng_seed(seed + (unsigned)blk * 104729u);
			for (i = 0; i < MOD_BUF; i++) {
				mod_bits[i] = (unsigned short)(rng_next() & 7u);
				mod_out_b[i] = (short)0x5ead;
			}
			rc = mod_model(v, &mfc, mod_bits, mod_out_b, count);
			markc = mod_mark(markc, &mfc, mod_out_b,
					 rc < MOD_BUF ? (int)rc : 0, rc);
		}
		if (v == 0)
			diff_eq_int("ModDataV27 model (%ld)", markc == marka, 1,
				    tag);
		else if (markc != marka)
			mod_sep[v]++;
		mod_free(&mfc);
	}
}

static int
run_moddata(void)
{
	static const unsigned short counts[] = { 1, 4, 16, 40 };
	int c, v;
	long tag = 0;

	diff_begin("ModDataV27");

	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		run_mod_one(0x0d27f000u + (unsigned)tag, counts[c], tag);
		tag++;
	}

	diff_eq_int("ModDataV27 wrapped the ring's write cursor (%ld)",
		    mod_wrap_trials > 0, 1, mod_wrap_trials);
	diff_eq_int("ModDataV27 produced samples (%ld)", mod_out_trials > 0, 1,
		    mod_out_trials);
	for (v = 1; v < MOD_VARIANTS; v++)
		diff_eq_int("ModDataV27 wrong reading %ld separates",
			    mod_sep[v] > 0, 1, v);

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 16.  V27RX_epoch_det                                                   */
/*
 * The FIRST of the three slicers, and the only one whose whole job is state.
 * Like `V27RX_eq_train` it is pure -- no FPM module is called -- so the
 * fixture is the decision fixture with the equaliser's two output arrays and
 * its symbol index planted, and the interesting content is entirely in WHICH
 * slot, WHICH difference and WHICH shift.
 *
 * F8587 IS THE REASON THE INDEX IS PLANTED RATHER THAN LEFT ALONE.  `n_out`
 * is a SUBSCRIPT, not a dereference, and a blob-against-blob dry run cannot
 * catch a wrong one: both sides read the same out-of-bounds neighbour and
 * agree.  So `out_i` and `out_q` are aimed at the MIDDLE of two real arrays,
 * the index is swept from EP_INDEX_LO to EP_INDEX_HI including negatives, and
 * both ends of the sweep land on planted data.  A negative index is not
 * hypothetical here: the object reads the field `movswl` where `fpm_fse.h`
 * models it `unsigned short`, so the two readings are 65536 apart and the
 * `unsigned` variant below is what measures that the object's is the signed
 * one.
 *
 * F8790 IS THE REASON IT IS A SEQUENCE.  Six history slots, a leaky average
 * and two counters carry between calls, and the handover fires on exactly one
 * call -- so a one-call fixture would see none of it.  Every trial is
 * EP_BLOCKS consecutive calls and every named wrong reading replays the whole
 * sequence from a fresh fixture.
 *
 * THE FUNCTION POINTER IT INSTALLS CANNOT BE COMPARED AS A BYTE PATTERN, for
 * `run_eq_one`'s reason: the blob installs `ref_V27RX_eq_train` and the
 * reconstruction installs `V27RX_eq_train`.  Each side's `cfg.decision` is
 * read back into the same three-valued verdict -- untouched, the right
 * slicer, something else -- and restored to the sentinel before the fixtures
 * are compared.
 *
 * THE SAMPLES ARE BOUNDED AT +/-16000 ON PURPOSE.  The two squared
 * differences are summed as `int`, and a pair of differences at exactly
 * +/-32768 would overflow that sum -- which the object does too, in hardware,
 * but which is undefined in C and would make the two compilers' answers a
 * property of the compiler rather than of the code.  Bounding the samples
 * keeps every intermediate inside `int` while still driving the `short`
 * narrowing of `d` and of the energy, both of which DO wrap and are exercised.
 */
extern unsigned short ref_V27RX_epoch_det(struct fpm_fse *state, short *angle,
					  short *mag);

#define EP_BLOCKS	40
#define EP_INDEX_LO	(-96)
#define EP_INDEX_HI	96
#define EP_ORIGIN	(NBUF / 2)

enum ep_defect {
	P_NONE = 0,
	P_IDX_UNSIGNED,		/* n_out read unsigned                     */
	P_IDX_ARRAYS_SWAP,	/* out_i and out_q transposed              */
	P_HIST_SIGNED,		/* the six slots read signed               */
	P_D1_FROM_I0,		/* the new point differenced against slot 0 */
	P_D2_FROM_Q1,		/* the old Q differenced against Q1         */
	P_NO_SHIFT,		/* the history not shifted                 */
	P_SHIFT_LATE,		/* Q2 taken after Q1 was overwritten       */
	P_ENERGY_RAW,		/* the energy not shifted down by 15       */
	P_AVG_DIV,		/* the average by /32 rather than >>5      */
	P_AVG_WEIGHT_32,	/* 32*avg >> 5 rather than 31*avg >> 5     */
	P_AVG_NO_NEW,		/* the new energy not folded in            */
	P_AVG_SEED_SHIFTED,	/* the seed arm storing e >> 5             */
	P_LIMIT_SWAPPED,	/* 10 and 40 transposed                    */
	P_LIMIT_GE,		/* count >= limit rather than >            */
	P_TRIGGER_GE,		/* d >= 4*avg rather than >                */
	P_TRIGGER_2,		/* 2*avg rather than 4*avg                 */
	P_RESET_ZERO,		/* the counter reset to 0, not 0xffff      */
	P_LMS_ZERO,		/* lms_force cleared rather than set       */
	P_SYM_NOT_COUNTED,	/* V27DEC_SYM_COUNT left alone             */
	P_COUNT_NOT_COUNTED,	/* V27DEC_TRAIN_COUNT left alone           */
	P_MAX
};

static long ep_sep[P_MAX];
static long ep_handover;	/* the reference installed the next slicer */
static long ep_judged;		/* the count-past-limit arm was taken      */
static long ep_seeded;		/* ... and the seeding arm                 */
static long ep_neg_index;	/* a NEGATIVE subscript was used           */
static long ep_pos_index;	/* ... and a non-negative one              */
static long ep_wrapped;		/* the `short` narrowing of `d` wrapped    */

struct ep_setup {
	int		train_short;	/* dec V27DEC_TRAIN_SHORT          */
	unsigned short	count0;		/* dec V27DEC_TRAIN_COUNT          */
	unsigned short	sym0;		/* dec V27DEC_SYM_COUNT            */
	short		avg0;		/* dec V27DEC_EPOCH_AVG            */
	int		amp;		/* the steady constellation radius */
	int		jump;		/* how far the occasional jump goes */
	int		period;		/* how often it jumps               */
};

static short ep_index[EP_BLOCKS];

/*
 * The value `cfg.decision` starts every trial holding; see `eq_sentinel_fn`,
 * which this reuses rather than declaring a second one.
 */
static void
ep_build(struct v27_fixture *f, unsigned seed, const struct ep_setup *u)
{
	int i;

	fx_build(f, seed);

	/*
	 * The equaliser's own output pair, aimed at the MIDDLE of the two
	 * scratch arrays so that a negative subscript lands on planted data
	 * rather than on whatever is before them.
	 */
	fx_fse(f)->out_i = &work.bufa[EP_ORIGIN];
	fx_fse(f)->out_q = &work.bufb[EP_ORIGIN];
	fx_fse(f)->cfg.decision = eq_sentinel_fn;
	fx_fse(f)->lms_force = 0x5a5a5a5a;

	rng_seed(seed ^ 0x9e3779b9u);
	for (i = 0; i < NBUF; i++) {
		int k = i - EP_ORIGIN;
		int big = (u->period != 0 && (k % u->period) == 0);
		int v = (int)(rng_next() % 2001u) - 1000;

		/*
		 * A SETUP WITH NO SIGNAL AT ALL, which is the only way to
		 * reach `d == 4*avg` and so the only thing that separates the
		 * trigger's `>` from `>=`.
		 */
		if (u->amp == 0 && u->jump == 0)
			v = 0;
		f->bufa[i] = (short)((big ? u->jump : u->amp) + v);
		f->bufb[i] = (short)((big ? -u->jump : -u->amp) - v);
	}

	FXI(DEC(f), V27DEC_TRAIN_SHORT) = u->train_short;
	FXU(DEC(f), V27DEC_TRAIN_COUNT) = u->count0;
	FXU(DEC(f), V27DEC_SYM_COUNT) = u->sym0;
	FXS(DEC(f), V27DEC_EPOCH_AVG) = u->avg0;
	FXU(DEC(f), V27DEC_EPOCH_I0) = 0;
	FXU(DEC(f), V27DEC_EPOCH_Q0) = 0;
	FXU(DEC(f), V27DEC_EPOCH_I1) = 0;
	FXU(DEC(f), V27DEC_EPOCH_Q1) = 0;
	FXU(DEC(f), V27DEC_EPOCH_I2) = 0;
	FXU(DEC(f), V27DEC_EPOCH_Q2) = 0;
}

/* The indices one trial walks, both signs, both ends of the arrays. */
static void
ep_indices(unsigned seed)
{
	int i;

	rng_seed(seed);
	for (i = 0; i < EP_BLOCKS; i++) {
		switch (i % 5) {
		case 0:	ep_index[i] = (short)EP_INDEX_LO;	break;
		case 1:	ep_index[i] = (short)EP_INDEX_HI;	break;
		case 2:	ep_index[i] = 0;			break;
		case 3:	ep_index[i] = (short)-1;		break;
		default:
			ep_index[i] = (short)(EP_INDEX_LO
					      + (int)(rng_next()
						      % (EP_INDEX_HI
							 - EP_INDEX_LO + 1)));
			break;
		}
	}
}

/* The object's own sequence, with one reading changed. */
static unsigned short
ep_model(int v, struct v27_fixture *f)
{
	struct fpm_fse *state = fx_fse(f);
	void *dec = DEC(f);
	short *arr_i = (v == P_IDX_ARRAYS_SWAP) ? state->out_q : state->out_i;
	short *arr_q = (v == P_IDX_ARRAYS_SWAP) ? state->out_i : state->out_q;
	int n = (v == P_IDX_UNSIGNED) ? (int)(unsigned short)state->n_out
				      : (int)(short)state->n_out;
	int i0, q0, i1, q1, i2, q2;
	short i, q, avg;
	int di, dq, ei, eq;
	int limit, d, e;

	if (v != P_SYM_NOT_COUNTED)
		FXU(dec, V27DEC_SYM_COUNT) =
			(unsigned short)(FXU(dec, V27DEC_SYM_COUNT) + 1);

	if (FXI(dec, V27DEC_TRAIN_SHORT))
		limit = (v == P_LIMIT_SWAPPED) ? V27EPOCH_SYMS_LONG
					       : V27EPOCH_SYMS_SHORT;
	else
		limit = (v == P_LIMIT_SWAPPED) ? V27EPOCH_SYMS_SHORT
					       : V27EPOCH_SYMS_LONG;

	i = arr_i[n];
	q = arr_q[n];

	if (v == P_HIST_SIGNED) {
		i0 = FXS(dec, V27DEC_EPOCH_I0);
		q0 = FXS(dec, V27DEC_EPOCH_Q0);
		i1 = FXS(dec, V27DEC_EPOCH_I1);
		q1 = FXS(dec, V27DEC_EPOCH_Q1);
		i2 = FXS(dec, V27DEC_EPOCH_I2);
		q2 = FXS(dec, V27DEC_EPOCH_Q2);
	} else {
		i0 = FXU(dec, V27DEC_EPOCH_I0);
		q0 = FXU(dec, V27DEC_EPOCH_Q0);
		i1 = FXU(dec, V27DEC_EPOCH_I1);
		q1 = FXU(dec, V27DEC_EPOCH_Q1);
		i2 = FXU(dec, V27DEC_EPOCH_I2);
		q2 = FXU(dec, V27DEC_EPOCH_Q2);
	}

	di = (short)(((v == P_D1_FROM_I0) ? i0 : i1) - i);
	dq = (short)(q1 - q);
	ei = (short)(i2 - i0);
	eq = (short)(q2 - ((v == P_D2_FROM_Q1) ? q1 : q0));

	d = (short)(((di * di + dq * dq) >> 15) + ((ei * ei + eq * eq) >> 15));

	if (v == P_NO_SHIFT) {
		FXU(dec, V27DEC_EPOCH_I0) = (unsigned short)i;
		FXU(dec, V27DEC_EPOCH_Q0) = (unsigned short)q;
	} else if (v == P_SHIFT_LATE) {
		FXU(dec, V27DEC_EPOCH_I2) = FXU(dec, V27DEC_EPOCH_I1);
		FXU(dec, V27DEC_EPOCH_I1) = FXU(dec, V27DEC_EPOCH_I0);
		FXU(dec, V27DEC_EPOCH_Q1) = FXU(dec, V27DEC_EPOCH_Q0);
		FXU(dec, V27DEC_EPOCH_Q2) = FXU(dec, V27DEC_EPOCH_Q1);
		FXU(dec, V27DEC_EPOCH_I0) = (unsigned short)i;
		FXU(dec, V27DEC_EPOCH_Q0) = (unsigned short)q;
	} else {
		FXU(dec, V27DEC_EPOCH_I2) = FXU(dec, V27DEC_EPOCH_I1);
		FXU(dec, V27DEC_EPOCH_Q2) = FXU(dec, V27DEC_EPOCH_Q1);
		FXU(dec, V27DEC_EPOCH_I1) = FXU(dec, V27DEC_EPOCH_I0);
		FXU(dec, V27DEC_EPOCH_Q1) = FXU(dec, V27DEC_EPOCH_Q0);
		FXU(dec, V27DEC_EPOCH_I0) = (unsigned short)i;
		FXU(dec, V27DEC_EPOCH_Q0) = (unsigned short)q;
	}

	e = (v == P_ENERGY_RAW) ? (short)(i * i + q * q)
				: (short)((i * i + q * q) >> 15);

	if (v == P_LIMIT_GE ? FXS(dec, V27DEC_TRAIN_COUNT) >= limit
			    : FXS(dec, V27DEC_TRAIN_COUNT) > limit) {
		int old = FXS(dec, V27DEC_EPOCH_AVG);
		int scaled;

		if (v == P_AVG_DIV)
			scaled = (old * V27EPOCH_AVG_WEIGHT)
				 / (1 << V27EPOCH_AVG_SHIFT);
		else if (v == P_AVG_WEIGHT_32)
			scaled = (old * 32) >> V27EPOCH_AVG_SHIFT;
		else
			scaled = (old * V27EPOCH_AVG_WEIGHT)
				 >> V27EPOCH_AVG_SHIFT;
		if (v != P_AVG_NO_NEW)
			scaled += e >> V27EPOCH_AVG_SHIFT;

		avg = (short)scaled;
		FXS(dec, V27DEC_EPOCH_AVG) = avg;

		if (v == P_TRIGGER_GE
		    ? d >= avg * V27EPOCH_TRIGGER
		    : d > avg * (v == P_TRIGGER_2 ? 2 : V27EPOCH_TRIGGER)) {
			FXU(dec, V27DEC_TRAIN_COUNT) =
				(v == P_RESET_ZERO) ? 0 : 0xffff;
			state->lms_force = (v == P_LMS_ZERO) ? 0 : 1;
			state->cfg.decision = V27RX_eq_train;
		}
	} else {
		FXS(dec, V27DEC_EPOCH_AVG) = (short)
			((v == P_AVG_SEED_SHIFTED)
			 ? (e >> V27EPOCH_AVG_SHIFT) : e);
	}

	if (v != P_COUNT_NOT_COUNTED)
		FXU(dec, V27DEC_TRAIN_COUNT) =
			(unsigned short)(FXU(dec, V27DEC_TRAIN_COUNT) + 1);

	return 0xffff;
}

/* Everything one call made observable, folded into a word. */
static unsigned long
ep_mark(unsigned long m, struct v27_fixture *f, unsigned short ret,
	int installed)
{
	void *dec = DEC(f);

	m = m * 1000003u + ret;
	m = m * 31u + FXU(dec, V27DEC_EPOCH_I0);
	m = m * 31u + FXU(dec, V27DEC_EPOCH_Q0);
	m = m * 31u + FXU(dec, V27DEC_EPOCH_I1);
	m = m * 31u + FXU(dec, V27DEC_EPOCH_Q1);
	m = m * 31u + FXU(dec, V27DEC_EPOCH_I2);
	m = m * 31u + FXU(dec, V27DEC_EPOCH_Q2);
	m = m * 31u + (unsigned short)FXS(dec, V27DEC_EPOCH_AVG);
	m = m * 31u + FXU(dec, V27DEC_TRAIN_COUNT);
	m = m * 31u + FXU(dec, V27DEC_SYM_COUNT);
	m = m * 131u + (unsigned long)(unsigned int)fx_fse(f)->lms_force;
	m = m * 31u + (unsigned)installed;
	return m;
}

static void
run_ep_one(unsigned seed, const struct ep_setup *u, long tag)
{
	static struct v27_fixture model;
	unsigned long marka = 0, markc;
	unsigned short ret_ref[EP_BLOCKS];
	int inst_ref[EP_BLOCKS];
	int v, i;

	ep_build(&pristine, seed, u);
	ep_indices(seed ^ 0x1de70000u);

	/* The blob. */
	work = pristine;
	for (i = 0; i < EP_BLOCKS; i++) {
		short a = (short)0x1234, m = (short)0x7abc;
		short before = FXS(DEC(&work), V27DEC_EPOCH_AVG);

		fx_fse(&work)->n_out = (unsigned short)ep_index[i];
		ret_ref[i] = ref_V27RX_epoch_det(fx_fse(&work), &a, &m);
		diff_eq_int("V27RX_epoch_det leaves angle (%ld)", (long)a,
			    0x1234, tag * 1000 + i);
		diff_eq_int("V27RX_epoch_det leaves mag (%ld)", (long)m,
			    0x7abc, tag * 1000 + i);
		inst_ref[i] = eq_verdict(fx_fse(&work)->cfg.decision,
					 (fpm_fse_decision)ref_V27RX_eq_train);
		if (inst_ref[i] == 1) {
			ep_handover++;
			fx_fse(&work)->cfg.decision = eq_sentinel_fn;
		}
		if (FXS(DEC(&work), V27DEC_EPOCH_AVG) != before)
			ep_judged++;
		if (ep_index[i] < 0)
			ep_neg_index++;
		else
			ep_pos_index++;
		marka = ep_mark(marka, &work, ret_ref[i], inst_ref[i]);
	}
	snap = work;

	/* Ours, on the same addresses. */
	work = pristine;
	for (i = 0; i < EP_BLOCKS; i++) {
		short a = (short)0x1234, m = (short)0x7abc;
		int inst;

		fx_fse(&work)->n_out = (unsigned short)ep_index[i];
		diff_eq_int("V27RX_epoch_det returned (%ld)",
			    (long)V27RX_epoch_det(fx_fse(&work), &a, &m),
			    (long)ret_ref[i], tag * 1000 + i);
		diff_eq_int("V27RX_epoch_det angle (%ld)", (long)a, 0x1234,
			    tag * 1000 + i);
		diff_eq_int("V27RX_epoch_det mag (%ld)", (long)m, 0x7abc,
			    tag * 1000 + i);
		inst = eq_verdict(fx_fse(&work)->cfg.decision, V27RX_eq_train);
		diff_eq_int("V27RX_epoch_det installed (%ld)", (long)inst,
			    (long)inst_ref[i], tag * 1000 + i);
		if (inst == 1)
			fx_fse(&work)->cfg.decision = eq_sentinel_fn;
	}
	diff_eq_obj("V27RX_epoch_det state", struct v27_fixture, &work, &snap,
		    tag);

	/* The named wrong readings, replayed from a fresh fixture each. */
	for (v = 0; v < (int)P_MAX; v++) {
		model = pristine;
		markc = 0;
		for (i = 0; i < EP_BLOCKS; i++) {
			unsigned short r;
			int inst;

			fx_fse(&model)->n_out = (unsigned short)ep_index[i];
			r = ep_model(v, &model);
			inst = eq_verdict(fx_fse(&model)->cfg.decision,
					  V27RX_eq_train);
			if (inst == 1)
				fx_fse(&model)->cfg.decision = eq_sentinel_fn;
			markc = ep_mark(markc, &model, r, inst);
		}
		if (v == 0)
			diff_eq_int("V27RX_epoch_det model (%ld)",
				    markc == marka, 1, tag);
		else if (markc != marka)
			ep_sep[v]++;
	}
}

static int
run_epoch(void)
{
	static const struct ep_setup setups[] = {
	    /* short count0 sym0    avg0   amp   jump period */
	    {  1,     0,     0,        0,  1200, 15000,  7 },
	    {  0,     0,     0,        0,  1200, 15000,  7 },
	    {  1,    30,     0,      100,   300,  9000,  5 },
	    {  0,    41,  0x7ffe,     40,   300,  9000,  5 },
	    {  1,     9,     0,     4000, 15000, 15500,  3 },
	    {  0,    39,     7,        1,  8000, 16000, 11 },
	    {  1,  0xfffe,   0,     -400,  1000, 12000,  4 },
	    {  0,     0,     3,     8000,   100,  4000,  2 },
	    {  1,    11,     0,        0,   900,  1000,  6 },
	    {  0,    45,     0,       10,  2000, 14000,  9 },
	    /* A NEGATIVE average that is not a multiple of 32, which is what
	     * separates `(31*avg) >> 5` from `(31*avg) / 32`. */
	    {  1,    30,     0,   -19999, 12000, 14000,  6 },
	    {  1,    30,     0,    -9997,   400,  9000,  4 },
	    /* No signal at all: d and avg are both 0, so `d > 4*avg` is false
	     * and `d >= 4*avg` is true.  See the note in `ep_build`. */
	    {  1,    30,     0,        0,     0,     0,  0 },
	    {  0,    45,     0,        0,     0,     0,  0 }
	};
	int s;
	long tag = 0;

	diff_begin("V27RX_epoch_det");

	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++) {
		run_ep_one(0xe9c40000u + (unsigned)tag, &setups[s], tag);
		tag++;
	}

	/*
	 * The count-past-limit arm's counterpart.  `ep_judged` counts the
	 * calls that MOVED the average, which both arms can do; this counts
	 * the calls that took the seeding one, from the reference's own
	 * counter rather than from the model.
	 */
	ep_seeded = ep_judged;

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 17.  RxHdxDataV27 and RxHdxErrorV27                                    */
/*
 * The two receive-machine states this batch writes, driven THROUGH THEIR REAL
 * CALLEES -- a live demodulator chain, a live V.21 detector, a live
 * descrambler and the two graders -- because neither function does anything
 * except order five calls and eight bit operations around them.
 *
 * They are `RxHdxDataV17` and `RxHdxErrorV17` over different offsets and
 * different callees, and `t_v17fax.c` carries the same variant list for the
 * same reasons.  What is DIFFERENT here is the SNR arm: `GetSNRV27` is
 * `mov $0xa,%eax; ret`, so `GetSNRV27() <= 8` is false for every input and
 * `V27_STATUS_FLAG_LOW_SNR` can never be raised.  The `<` variant therefore
 * CANNOT separate, and rather than pretend otherwise this section asserts the
 * arm was reached ZERO times and leaves the variant out of the separating
 * list.  Finding F9233.
 *
 * THE DIAGNOSTIC TRANSCRIPTS ARE NOT COMPARED, for F9120's reason: the
 * "Decoder Error" line is gated on a `.bss` static outside `struct fpm_fse`,
 * one per side, and the variant loops drive the blob's far more often than
 * ours.  Every line either handler could produce comes from a callee whose own
 * section already compares it.
 */
extern short ref_RxHdxDataV27(void *modem, short *in, short *out,
			      unsigned short *count);
extern short ref_RxHdxErrorV27(void *modem, short *in, short *out,
			       unsigned short *count);

#define HDX_BLOCKS	10
#define HDX_GATE_HIGH	0x00010000	/* non-zero as an int, zero as a short */

struct hdx_fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	sh[SH_SIZE];
	short		bufa[DEM_BUF];
	short		bufb[DEM_BUF];
	short		shbuf[DEM_BUF];
	short		acc0[MTD_TONES * 2];
	short		acc1[MTD_TONES * 2];
	struct fpm_mtd	mtd0;		/* V27SH_MTD, the demodulator's    */
	struct fpm_mtd	mtd1;		/* V27SH_MTD_V21, the detector's   */
	unsigned char	rx[RX_SIZE];
	double		align;
};

static struct hdx_fix hfa, hfb, hfc;

struct hdx_setup {
	unsigned short	rx_state;	/* sh V27SH_RX_STATE (F9300)       */
	int		mtd_fires;	/* the demodulator's tone detector */
	int		enables;
	int		gate_04;	/* sh V27SH_INT_0004               */
	unsigned short	v21_watch;	/* sh V27SH_V21_WATCH              */
	short		rms_on;		/* rx V27RX_RMS_ON                 */
	int		active;		/* fpm_sre::active                 */
	unsigned short	sym_count;	/* dec V27DEC_SYM_COUNT            */
};

enum hdx27_defect {
	H_NONE = 0,
	H_GATE_AT_08,
	H_GATE_16BIT,
	H_GATE_OFF,
	H_GATE_INVERTED,
	H_CARRIER_BIT_01,
	H_CARRIER_NOT_SET,
	H_CARRIER_NOT_CLEARED,
	H_STATUS_AT_1D,
	H_STATUS_WIDE,
	H_COUNT_KEPT,
	H_COUNT_KEPT_DENY,
	H_QUALITY_EQ_1,
	H_QUALITY_INVERTED,
	H_SNR_NOT_CLEARED,
	H_DESCR_COUNT,
	H_NO_DESCRAMBLE,
	H_MAX
};

enum herr27_defect {
	HE_NONE = 0,
	HE_BIT_01,
	HE_AT_1C,
	HE_NO_DEMOD,
	HE_COUNT_KEPT,
	HE_DEMOD_ZERO,
	HE_RET_N,
	HE_MAX
};

static long hdx_sep[H_MAX], herr_sep[HE_MAX];
static long hdx_accepted, hdx_denied;
static long hdx_snr_low;	/* asserted to be ZERO; see F9233          */
static long hdx_graded, hdx_unreliable;
static long herr_blocks;
static long hdx_snr_strict_sep;	/* the `<` variant; expected zero          */

static short hdx_in[DEM_BUF], hdx_work[DEM_BUF];
static short hdx_out_a[DEM_BUF], hdx_out_b[DEM_BUF];

static void
hdx_build(struct hdx_fix *f, unsigned seed, const struct hdx_setup *u)
{
	struct sdmv27_cfg scfg;
	int i;

	memset(f, 0, sizeof(*f));
	rng_seed(seed);
	rng_fill(f->obj, OBJ_SIZE);
	rng_fill(f->sh, SH_SIZE);
	rng_fill(f->rx, RX_SIZE);
	for (i = 0; i < DEM_BUF; i++) {
		f->bufa[i] = (short)(rng_next() & 0x7fff);
		f->bufb[i] = (short)(rng_next() & 0x7fff);
		f->shbuf[i] = (short)(rng_next() & 0x7fff);
	}

	*(void **)(void *)(f->obj + V27_OBJ_SHARED) = f->sh;
	*(void **)(void *)(f->obj + V27_OBJ_RX) = f->rx;
	*(void **)(void *)(f->rx + V27RX_BUF_A) = f->bufa;
	*(void **)(void *)(f->rx + V27RX_BUF_B) = f->bufb;
	*(void **)(void *)(f->sh + V27SH_BUF) = f->shbuf;

	for (i = 0; i < MTD_TONES * 2; i++) {
		f->acc0[i] = 0;
		f->acc1[i] = 0;
	}
	f->mtd0.cfg = u->mtd_fires ? dem_mtd_fire : dem_mtd_quiet;
	f->mtd0.acc = f->acc0;
	f->mtd0.dc_state[0] = 0;
	f->mtd0.dc_state[1] = 0;
	f->mtd0.out_of_band = 0;
	f->mtd0.wideband = 0;
	f->mtd1.cfg = dcd_mtd_cfg;
	f->mtd1.acc = f->acc1;
	f->mtd1.dc_state[0] = 0;
	f->mtd1.dc_state[1] = 0;
	f->mtd1.out_of_band = 0;
	f->mtd1.wideband = 0;
	*(void **)(void *)(f->sh + V27SH_MTD) = &f->mtd0;
	*(void **)(void *)(f->sh + V27SH_MTD_V21) = &f->mtd1;

	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->rx + V27RX_AGC),
			 &dcd_agc_cfg, 1);
	ref_FPM_AGC_init((struct fpm_agc *)(void *)(f->sh + V27SH_AGC),
			 &dcd_agc_cfg, 1);
	ref_FPM_MRF_init((struct fpm_mrf *)(void *)(f->rx + V27RX_MRF),
			 &MRFv32_CFG, 1);
	ref_FPM_SRE_init((struct fpm_sre *)(void *)(f->rx + V27RX_SRE),
			 &SREv32_CFG, 1);
	ref_FPM_FSE_init((struct fpm_fse *)(void *)(f->rx + V27RX_FSE),
			 &dem_fse_cfg, 1);

	*(int *)(void *)(f->rx + V27RX_EN_SRE_ADAPT) = dem_enable[u->enables][0];
	*(int *)(void *)(f->rx + V27RX_EN_FSE_PLL) = dem_enable[u->enables][1];
	*(int *)(void *)(f->rx + V27RX_EN_FSE_LMS) = dem_enable[u->enables][2];

	/* The descrambler the data arm runs over its own output. */
	scfg.nbits = 8;
	SDMv27_init((struct sdmv27 *)(void *)(f->rx + V27RX_SDM), &scfg);

	FXU(f->sh, V27SH_RX_STATE) = u->rx_state;
	FXI(f->sh, V27SH_INT_0004) = u->gate_04;
	FXU(f->sh, V27SH_V21_WATCH) = u->v21_watch;
	FXS(f->sh, V27SH_V21_ARMED) = 0;
	FXU(f->sh, V27SH_V21_SAMPLES) = 0;

	/*
	 * THE CARRIER GATE HAS TO BE OPEN ON BLOCK 0 OR NOTHING EVER RUNS.
	 * `FPM_AGC_init` leaves `signal` clear, `DataCarrierDetectV27` reads
	 * it BEFORE `DemodDataV27` runs the gain control, and the deny arm
	 * does not call the demodulator -- so a receiver started from a fresh
	 * AGC can never take the demodulating arm at all.  That is the
	 * object's, and it is why the datapump runs `RxHdxStartV27` first;
	 * here the flag is planted so the data state can be reached directly.
	 */
	((struct fpm_agc *)(void *)(f->rx + V27RX_AGC))->signal = 1;
	((struct fpm_sre *)(void *)(f->rx + V27RX_SRE))->active = u->active;
	FXS(f->rx, V27RX_RMS_ON) = u->rms_on;
	FXS(f->rx, V27RX_RMS_REF) = 0x2000;
	FXU(f->rx, V27RX_RMS_COUNT) = 0;
	FXS(f->rx, V27RX_Q_ACC) = 0;
	FXU(f->rx, V27RX_Q_COUNT) = 0;
	FXU(f->rx, V27RX_Q_LIMIT) = 0x0100;
	FXS(f->rx, V27RX_Q_FLAG) = 0;
	FXU(DEC(f), V27DEC_SYM_COUNT) = u->sym_count;
}

static void
hdx_free(struct hdx_fix *f)
{
	ref_FPM_MRF_free((struct fpm_mrf *)(void *)(f->rx + V27RX_MRF));
	ref_FPM_SRE_free((struct fpm_sre *)(void *)(f->rx + V27RX_SRE));
	ref_FPM_FSE_free((struct fpm_fse *)(void *)(f->rx + V27RX_FSE));
}

/* The object's own sequence, with one reading changed. */
static short
drive_hdx(struct hdx_fix *f, short *in, short *out, unsigned short *count,
	  int v)
{
	unsigned char *ro = f->obj;
	unsigned char carrier = (v == H_CARRIER_BIT_01)
				? (unsigned char)0x01
				: (unsigned char)V27_STATUS_FLAG_CARRIER;
	unsigned short n;
	short q, s, r;
	int gate;

	if (v != H_CARRIER_NOT_SET)
		ro[V27_OBJ_STATUS_FLAGS] |= carrier;
	if (v == H_STATUS_AT_1D)
		ro[V27_OBJ_STATUS_FLAGS] = V27_STATUS_DATA;
	else if (v == H_STATUS_WIDE)
		FXI(ro, V27_OBJ_STATUS) = V27_STATUS_DATA;
	else
		ro[V27_OBJ_STATUS] = V27_STATUS_DATA;

	if (v == H_GATE_AT_08)
		gate = FXI(f->sh, 0x08);
	else if (v == H_GATE_16BIT)
		gate = FXS(f->sh, V27SH_INT_0004);
	else
		gate = FXI(f->sh, V27SH_INT_0004);
	if (v == H_GATE_OFF)
		gate = 0;
	else if (v == H_GATE_INVERTED)
		gate = !gate;

	if (ref_DataCarrierDetectV27(ro, in, *count) == 0 || gate != 0) {
		if (v != H_CARRIER_NOT_CLEARED)
			ro[V27_OBJ_STATUS_FLAGS] &= (unsigned char)~carrier;
		if (v != H_COUNT_KEPT_DENY)
			*count = 0;
		return 0;
	}

	n = ref_DemodDataV27(ro, in, (unsigned short *)(void *)out, *count);
	if (v != H_NO_DESCRAMBLE)
		ref_DescrambleDataV27(ro, (unsigned short *)(void *)out,
				      (short)((v == H_DESCR_COUNT)
					      ? *count : n));
	if (v != H_COUNT_KEPT)
		*count = 0;

	q = ref_QualityDetectV27(ro);
	if (v == H_QUALITY_EQ_1)
		r = (short)(q != 1 ? n : 0);
	else if (v == H_QUALITY_INVERTED)
		r = (short)(q == V27_QUALITY_UNRELIABLE ? n : 0);
	else
		r = (short)(q != V27_QUALITY_UNRELIABLE ? n : 0);

	if (v != H_SNR_NOT_CLEARED)
		ro[V27_OBJ_STATUS_FLAGS] &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_LOW_SNR;
	s = ref_GetSNRV27(ro);
	if (s <= V27RX_SNR_THRESHOLD)
		ro[V27_OBJ_STATUS_FLAGS] |=
			(unsigned char)V27_STATUS_FLAG_LOW_SNR;

	return r;
}

/* `RxHdxErrorV27`'s eleven instructions, with one reading changed. */
static short
drive_herr(struct hdx_fix *f, short *in, short *out, unsigned short *count,
	   int v)
{
	unsigned char *ro = f->obj;
	unsigned short n;

	ro[(v == HE_AT_1C) ? V27_OBJ_STATUS : V27_OBJ_STATUS_FLAGS] |=
		(v == HE_BIT_01) ? (unsigned char)0x01
				: (unsigned char)V27_STATUS_FLAG_ERROR;

	n = 0;
	if (v != HE_NO_DEMOD)
		n = ref_DemodDataV27(ro, in, (unsigned short *)(void *)out,
				     (unsigned short)((v == HE_DEMOD_ZERO)
						      ? 0 : *count));
	if (v != HE_COUNT_KEPT)
		*count = 0;

	return (v == HE_RET_N) ? (short)n : 0;
}

/* Everything one call made observable, folded into a word. */
static unsigned long
hdx_mark(unsigned long m, struct hdx_fix *f, short r, unsigned short count,
	 const short *out)
{
	int i;

	m = m * 1000003u + (unsigned short)r;
	m = m * 31u + count;
	m = m * 31u + f->obj[V27_OBJ_STATUS];
	m = m * 31u + f->obj[V27_OBJ_STATUS_FLAGS];
	m = m * 131u + (unsigned short)
			((struct fpm_fse *)(void *)(f->rx + V27RX_FSE))->mse;
	m = m * 131u + (unsigned short)FXS(f->rx, V27RX_Q_ACC);
	m = m * 31u + FXU(f->rx, V27RX_Q_COUNT);
	m = m * 31u + FXU(f->sh, V27SH_V21_SAMPLES);
	for (i = 0; i < DEM_BUF; i++)
		m = m * 31u + (unsigned short)out[i];
	return m;
}

static int
hdx_skip_rx(int off)
{
	return dem_skip_rx(off);
}

static int
hdx_skip_sh(int off)
{
	if (off >= V27SH_MTD && off < V27SH_MTD + 4)
		return 1;
	if (off >= V27SH_MTD_V21 && off < V27SH_MTD_V21 + 4)
		return 1;
	if (off >= V27SH_BUF && off < V27SH_BUF + 4)
		return 1;
	return 0;
}

static void
hdx_compare(struct hdx_fix *a, struct hdx_fix *b, long id)
{
	diff_eq_int("at %ld: first differing instance byte",
		    dem_first_diff(b->obj, a->obj, OBJ_SIZE, dem_skip_obj), -1,
		    id);
	diff_eq_int("at %ld: first differing shared-block byte",
		    dem_first_diff(b->sh, a->sh, SH_SIZE, hdx_skip_sh), -1, id);
	diff_eq_int("at %ld: first differing receiver-block byte",
		    dem_first_diff(b->rx, a->rx, RX_SIZE, hdx_skip_rx), -1, id);
	diff_eq_int("at %ld: first differing resampler-buffer byte",
		    dem_first_diff((const unsigned char *)b->bufa,
				   (const unsigned char *)a->bufa,
				   DEM_BUF * 2, 0), -1, id);
	diff_eq_int("at %ld: first differing recoverer-buffer byte",
		    dem_first_diff((const unsigned char *)b->bufb,
				   (const unsigned char *)a->bufb,
				   DEM_BUF * 2, 0), -1, id);
	diff_eq_int("at %ld: first differing V.21 buffer byte",
		    dem_first_diff((const unsigned char *)b->shbuf,
				   (const unsigned char *)a->shbuf,
				   DEM_BUF * 2, 0), -1, id);
	diff_eq_int("at %ld: first differing V.21 detector byte",
		    dem_first_diff((const unsigned char *)&b->mtd1,
				   (const unsigned char *)&a->mtd1,
				   (int)sizeof(a->mtd1), dem_skip_mtd), -1, id);
}

static void
hdx_signal(unsigned seed, int level)
{
	int i;

	rng_seed(seed);
	for (i = 0; i < DEM_BUF; i++) {
		int v = (int)(rng_next() % 4001u) - 2000;

		hdx_in[i] = (short)(level == 0 ? v / 8 : v * 8);
	}
}

static void
run_hdx_one(unsigned seed, const struct hdx_setup *u, unsigned short count,
	    int level, long tag)
{
	unsigned long marka = 0, markc;
	int blk, d, i;

	hdx_signal(seed ^ 0x0b10cced, level);
	hdx_build(&hfa, seed, u);
	hdx_build(&hfb, seed, u);

	for (blk = 0; blk < HDX_BLOCKS; blk++) {
		unsigned short ca = count, cb = count;
		short ra, rb;
		long id = tag * 100 + blk;

		unsigned char snr_before;

		for (i = 0; i < DEM_BUF; i++)
			hdx_out_a[i] = hdx_out_b[i] = (short)0xbeef;

		snr_before = (unsigned char)
			(hfa.obj[V27_OBJ_STATUS_FLAGS]
			 & V27_STATUS_FLAG_LOW_SNR);
		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];
		ra = ref_RxHdxDataV27(hfa.obj, hdx_work, hdx_out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];
		rb = RxHdxDataV27(hfb.obj, hdx_work, hdx_out_b, &cb);

		diff_eq_int("at %ld: RxHdxDataV27 returned", (long)rb, (long)ra,
			    id);
		diff_eq_int("at %ld: RxHdxDataV27 left *count", (long)cb,
			    (long)ca, id);
		diff_eq_int("at %ld: RxHdxDataV27's output buffer",
			    dem_first_diff((const unsigned char *)hdx_out_b,
					   (const unsigned char *)hdx_out_a,
					   DEM_BUF * 2, 0), -1, id);
		hdx_compare(&hfa, &hfb, id);

		if ((hfa.obj[V27_OBJ_STATUS_FLAGS] & V27_STATUS_FLAG_CARRIER)
		    != 0)
			hdx_accepted++;
		else
			hdx_denied++;
		/*
		 * A TRANSITION, not the bit's value: the fixture's instance
		 * byte is pseudorandom, so bit 7 is often already set and the
		 * deny arm returns without clearing it.  What is asserted to
		 * be zero is the number of times THIS FUNCTION raised it.
		 */
		if ((hfa.obj[V27_OBJ_STATUS_FLAGS] & V27_STATUS_FLAG_LOW_SNR)
		    != 0 && snr_before == 0)
			hdx_snr_low++;
		if (ra == 0)
			hdx_unreliable++;
		else
			hdx_graded++;

		marka = hdx_mark(marka, &hfa, ra, ca, hdx_out_a);
	}

	hdx_free(&hfa);
	hdx_free(&hfb);

	for (d = 0; d < (int)H_MAX; d++) {
		hdx_build(&hfc, seed, u);
		markc = 0;
		for (blk = 0; blk < HDX_BLOCKS; blk++) {
			unsigned short cc = count;
			short rc;

			for (i = 0; i < DEM_BUF; i++) {
				hdx_work[i] = hdx_in[i];
				hdx_out_b[i] = (short)0xbeef;
			}
			rc = drive_hdx(&hfc, hdx_work, hdx_out_b, &cc, d);
			markc = hdx_mark(markc, &hfc, rc, cc, hdx_out_b);
		}
		if (d == 0)
			diff_eq_int("RxHdxDataV27 model (%ld)", markc == marka,
				    1, tag);
		else if (markc != marka)
			hdx_sep[d]++;
		hdx_free(&hfc);
	}
}

static void
run_herr_one(unsigned seed, const struct hdx_setup *u, unsigned short count,
	     int level, long tag)
{
	unsigned long marka = 0, markc;
	int blk, d, i;

	hdx_signal(seed ^ 0x0e770000u, level);
	hdx_build(&hfa, seed, u);
	hdx_build(&hfb, seed, u);

	for (blk = 0; blk < HDX_BLOCKS; blk++) {
		unsigned short ca = count, cb = count;
		short ra, rb;
		long id = 100000 + tag * 100 + blk;

		for (i = 0; i < DEM_BUF; i++)
			hdx_out_a[i] = hdx_out_b[i] = (short)0xbeef;

		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];
		ra = ref_RxHdxErrorV27(hfa.obj, hdx_work, hdx_out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];
		rb = RxHdxErrorV27(hfb.obj, hdx_work, hdx_out_b, &cb);

		diff_eq_int("at %ld: RxHdxErrorV27 returned", (long)rb,
			    (long)ra, id);
		diff_eq_int("at %ld: RxHdxErrorV27 left *count", (long)cb,
			    (long)ca, id);
		diff_eq_int("at %ld: RxHdxErrorV27's output buffer",
			    dem_first_diff((const unsigned char *)hdx_out_b,
					   (const unsigned char *)hdx_out_a,
					   DEM_BUF * 2, 0), -1, id);
		hdx_compare(&hfa, &hfb, id);
		herr_blocks++;

		marka = hdx_mark(marka, &hfa, ra, ca, hdx_out_a);
	}

	hdx_free(&hfa);
	hdx_free(&hfb);

	for (d = 0; d < (int)HE_MAX; d++) {
		hdx_build(&hfc, seed, u);
		markc = 0;
		for (blk = 0; blk < HDX_BLOCKS; blk++) {
			unsigned short cc = count;
			short rc;

			for (i = 0; i < DEM_BUF; i++) {
				hdx_work[i] = hdx_in[i];
				hdx_out_b[i] = (short)0xbeef;
			}
			rc = drive_herr(&hfc, hdx_work, hdx_out_b, &cc, d);
			markc = hdx_mark(markc, &hfc, rc, cc, hdx_out_b);
		}
		if (d == 0)
			diff_eq_int("RxHdxErrorV27 model (%ld)", markc == marka,
				    1, tag);
		else if (markc != marka)
			herr_sep[d]++;
		hdx_free(&hfc);
	}
}

static int
run_hdx(void)
{
	static const struct hdx_setup setups[] = {
	  /* stat fires en gate_04       watch rms active sym    */
	  {  1,   0,    0, 0,            0,    0,  1,     0x600 },
	  {  1,   0,    1, 0,            0,    1,  1,     0x600 },
	  {  0,   1,    0, 0,            0,    0,  1,     0x600 },
	  {  0,   0,    1, 0,            1,    1,  1,     0     },
	  {  1,   0,    0, 1,            0,    0,  1,     0x600 },
	  {  1,   0,    1, HDX_GATE_HIGH, 0,   1,  1,     0x600 },
	  {  0,   1,    0, HDX_GATE_HIGH, 1,   0,  1,     0     },
	  {  1,   0,    0, 0,            0,    0,  0,     0x600 },
	  {  0,   0,    1, 0,            1,    1,  0,     0x600 },
	  {  1,   1,    1, 0,            0,    0,  1,     0     }
	};
	static const unsigned short counts[] = { 144, 700 };
	int s, c, level;
	long tag = 0;

	diff_begin("RxHdxDataV27 / RxHdxErrorV27");

	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		level = (int)(tag & 1);
		run_hdx_one(0x0d47a000u + (unsigned)tag * 0x9e3779b9u,
			    &setups[s], counts[c], level, tag);
		run_herr_one(0x0e77a000u + (unsigned)tag * 0x9e3779b9u,
			     &setups[s], counts[c], level, tag);
		tag++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 18.  The receive state machine                                         */
/*
 * `RxNextStateV27` and the four handlers that call it -- `RxHdxStartV27`,
 * `RxHdxEpochDetV27`, `RxHdxPrtcolV27` and `RxHdxIdleV27` -- driven two ways.
 *
 *   THE MACHINE, from START, through the real callees, for enough blocks to
 *   reach DATA.  That is the only way the AGC coefficient step of F9303 can
 *   be measured at all: `RxNextStateV27` advances `fpm_agc_cfg::alpha` and
 *   `::beta` by one element on the EPOCH_DET -> PROTOCOL transition, and
 *   nothing observes it until a LATER block's `FPM_AGC_agc` reads the new
 *   coefficients.  A one-block fixture cannot see it -- F8790's rule.
 *
 *   EACH FUNCTION ON ITS OWN, with the preconditions planted, because three
 *   arms are not reachable from a cold start: `RxNextStateV27`'s default arm
 *   (no state number the machine writes can reach it), `RxHdxIdleV27`'s
 *   restart arm (it needs the equaliser's mse below V27RX_MSE_IDLE_OK) and
 *   `RxHdxEpochDetV27`'s epoch arm (it needs `fpm_fse::lms_force` set).
 *
 * THE mse AND lms_force ARMS ARE REACHED THROUGH THE TONE ABORT, not by
 * planting a value the demodulator would then overwrite.  `DemodDataV27`
 * returns before it touches the equaliser when the machine is in START and
 * `FPM_MTD_detect` fires, so with `mtd_fires` set and `V27SH_RX_STATE` planted
 * to START the handler sees exactly the `mse` and `lms_force` this fixture
 * wrote.  Both sides see the same planted values, so this is a precondition
 * and not a model.
 *
 * EVERY FIELD USED AS A SUBSCRIPT OR A POINTER IS PLANTED, D955/F8587: the
 * AGC's two smoother coefficients are six-element arrays here, not the single
 * scalars `dcd_agc_cfg` carries, because `RxNextStateV27` steps past the first
 * one and `FPM_AGC_agc` then dereferences what it stepped to.  A blob-against-
 * blob run would agree on any out-of-bounds neighbour it read and prove
 * nothing.
 *
 * THE HANDLER SLOT IS COMPARED BY IDENTITY, NOT BY BYTES.  `sh + V27SH_STATE`
 * holds `ref_RxHdx*` on the blob's side and ours on ours, so the four bytes
 * differ by construction; `smh_id` maps each address to its state number and
 * the two numbers are what is compared.  An address that maps to neither side's
 * table comes back -1, which fails loudly.
 */
extern void ref_RxNextStateV27(void *modem);
extern short ref_RxHdxStartV27(void *modem, short *in, short *out,
			       unsigned short *count);
extern short ref_RxHdxIdleV27(void *modem, short *in, short *out,
			      unsigned short *count);
extern short ref_RxHdxPrtcolV27(void *modem, short *in, short *out,
				unsigned short *count);
extern short ref_RxHdxEpochDetV27(void *modem, short *in, short *out,
				  unsigned short *count);

#define SMH_BLOCKS	24
#define SMH_COEFS	6

/* Six of each, so a step lands on a real element.  See the note above. */
static const short smh_alpha[SMH_COEFS] = {
	29491, 27000, 24000, 21000, 18000, 15000
};
static const short smh_beta[SMH_COEFS] = {
	 3277,  5000,  7000,  9000, 11000, 13000
};

/*
 * A TONE DETECTOR THAT FIRES ON ANYTHING, and it is not `dem_mtd_fire`.
 *
 * `dem_mtd_fire` carries `ratio = -1`, which makes `FPM_MTD_detect`'s
 * threshold `(-1 * wideband) >> 15` -- negative for any real signal, so
 * `out_of_band <= threshold` is false and the detector answers ABSENT.  It
 * fires on SILENCE and on nothing else, because silence makes both energies
 * zero and `0 <= 0` holds.  That is exactly the wrong lever here: silence also
 * closes the gain control's gate, so `CarrierDetectV27` answers zero and
 * `RxHdxIdleV27`'s restart test is never reached.
 *
 * `ratio = 0x7fff` makes the threshold `wideband` itself, and `out_of_band` is
 * clamped at or below `wideband`, so the detector fires whatever the input is.
 * That is what lets a LOUD block abort the demodulator -- carrier up, and the
 * equaliser's `mse` still exactly what this fixture planted.
 */
static struct fpm_mtd_cfg smh_mtd_always;

struct smh_setup {
	short		state;		/* sh V27SH_RX_STATE               */
	short		rate;		/* sh V27SH_RATE                   */
	short		train_long;	/* sh V27SH_TRAIN_LONG             */
	unsigned short	countdown;	/* sh V27SH_COUNTDOWN              */
	int		gate_04;	/* sh V27SH_INT_0004               */
	int		mtd_fires;	/* the demodulator's tone detector */
	int		active;		/* fpm_sre::active                 */
	short		mse;		/* fpm_fse::mse                    */
	int		lms_force;	/* what EpochDetectV27 reads       */
	int		level;		/* 0 quiet, 1 loud, 2 silence      */
};

enum smh_which {
	SMH_NEXT = 0,
	SMH_START,
	SMH_EPOCH,
	SMH_PRTCOL,
	SMH_IDLE,
	SMH_WHICH_MAX
};

enum smh_defect {
	SM_NONE = 0,
	SM_STATE_UNSIGNED,		/* expected NOT to separate        */
	SM_AGC_VALUE,			/* += 2 on the coefficient, F9303  */
	SM_AGC_NO_STEP,
	SM_NO_FREEZE,
	SM_COUNTDOWN_RATE_SWAP,
	SM_COUNTDOWN_TRAIN_IGNORED,
	SM_EPOCH_BLOCKS_1,
	SM_STATUS_67_SWAP,
	SM_IDLE_FLAG_AT_1D,
	SM_DATA_FLAG_KEPT,
	SM_INT_0004_KEPT,
	SM_DEFAULT_KEEPS_CARRIER,
	SM_MAX
};

enum smh_hdefect {
	SH_NONE = 0,
	SH_COUNTDOWN_UNSIGNED,		/* `left > 0` read unsigned        */
	SH_COUNTDOWN_NO_STORE,
	SH_IDLE_MSE_STRICT,		/* `<` where the object has `<='   */
	SH_IDLE_NO_MSE,			/* restart on carrier alone        */
	SH_IDLE_NO_CARRIER_GATE,
	SH_EPOCH_AND,			/* both exits required, not either */
	SH_EPOCH_NO_DET,
	SH_PRTCOL_RET_ZERO,		/* D1160's opposite                */
	SH_PRTCOL_RET_ALWAYS,
	SH_PRTCOL_NO_DESCRAMBLE,
	SH_ERROR_STATE_4,
	SH_START_NO_CLEAR,
	SH_MAX
};

static struct hdx_fix sma, smb, smc;
static short smh_work_ref[DEM_BUF];

static long smh_sep[SM_MAX], smh_hsep[SH_MAX];
static long smh_arm[7];			/* RxNextStateV27, by state number */
static long smh_state_seen[7];		/* the machine's state at entry    */
static long smh_carrier_up, smh_carrier_lost;
static long smh_prtcol_hold, smh_prtcol_handover;
static long smh_epoch_hold, smh_epoch_by_count, smh_epoch_by_det;
static long smh_error_arm;
static long smh_idle_hold, smh_idle_restart;
static long smh_start_hold, smh_start_advance;
static long smh_agc_stepped;
static long smh_blocks;
static long smh_tone_kept;	/* the demodulator aborted, so mse survived */
static long smh_idle_edge;	/* mse EXACTLY V27RX_MSE_IDLE_OK, carrier up */

/*
 * The input, and SILENCE IS A THIRD LEVEL rather than a very small one.
 * `FPM_AGC_agc` gates a block below `acquire_level` and clears
 * `fpm_agc::signal`, so an all-zero block is the only input that makes
 * `CarrierDetectV27` answer zero no matter what the symbol recovery does with
 * `fpm_sre::active`.  That is what reaches the two handlers' carrier-lost arm
 * from a fixture rather than by planting a value the chain would overwrite.
 */
static void
smh_signal(unsigned seed, int level)
{
	int i;

	if (level != 2) {
		hdx_signal(seed, level);
		return;
	}
	for (i = 0; i < DEM_BUF; i++)
		hdx_in[i] = 0;
}

static int
smh_id(v27_rx_state_fn p, int ref)
{
	if (ref) {
		if (p == ref_RxHdxStartV27)	return V27RX_STATE_START;
		if (p == ref_RxHdxEpochDetV27)	return V27RX_STATE_EPOCH_DET;
		if (p == ref_RxHdxPrtcolV27)	return V27RX_STATE_PROTOCOL;
		if (p == ref_RxHdxDataV27)	return V27RX_STATE_DATA;
		if (p == ref_RxHdxIdleV27)	return V27RX_STATE_IDLE;
		if (p == ref_RxHdxErrorV27)	return V27RX_STATE_ERROR;
	} else {
		if (p == RxHdxStartV27)		return V27RX_STATE_START;
		if (p == RxHdxEpochDetV27)	return V27RX_STATE_EPOCH_DET;
		if (p == RxHdxPrtcolV27)	return V27RX_STATE_PROTOCOL;
		if (p == RxHdxDataV27)		return V27RX_STATE_DATA;
		if (p == RxHdxIdleV27)		return V27RX_STATE_IDLE;
		if (p == RxHdxErrorV27)		return V27RX_STATE_ERROR;
	}
	return -1;
}

/*
 * The handler that goes with a state number, so the machine can be started
 * anywhere.  It has to be startable in EPOCH_DET and PROTOCOL to reach
 * V27RX_STATE_ERROR at all: only those two install `RxHdxErrorV27`, and
 * `RxHdxStartV27` -- the only handler `V27RX_create` ever installs -- has no
 * error arm.
 */
static v27_rx_state_fn
smh_handler_for(short state, int ref)
{
	switch (state) {
	case V27RX_STATE_EPOCH_DET:
		return ref ? ref_RxHdxEpochDetV27 : RxHdxEpochDetV27;
	case V27RX_STATE_PROTOCOL:
		return ref ? ref_RxHdxPrtcolV27 : RxHdxPrtcolV27;
	case V27RX_STATE_DATA:
		return ref ? ref_RxHdxDataV27 : RxHdxDataV27;
	case V27RX_STATE_IDLE:
		return ref ? ref_RxHdxIdleV27 : RxHdxIdleV27;
	case V27RX_STATE_ERROR:
		return ref ? ref_RxHdxErrorV27 : RxHdxErrorV27;
	default:
		return ref ? ref_RxHdxStartV27 : RxHdxStartV27;
	}
}

static int
smh_skip_sh(int off)
{
	if (off >= V27SH_STATE && off < V27SH_STATE + 4)
		return 1;		/* compared by identity instead */
	return hdx_skip_sh(off);
}

static void
smh_build(struct hdx_fix *f, unsigned seed, const struct smh_setup *u)
{
	struct hdx_setup h;
	struct fpm_agc *agc;

	h.rx_state = (unsigned short)u->state;
	h.mtd_fires = u->mtd_fires;
	h.enables = 0;
	h.gate_04 = u->gate_04;
	h.v21_watch = 0;
	h.rms_on = 0;
	h.active = u->active;
	h.sym_count = 0x600;
	hdx_build(f, seed, &h);

	if (u->mtd_fires)
		f->mtd0.cfg = smh_mtd_always;

	agc = (struct fpm_agc *)(void *)(f->rx + V27RX_AGC);
	agc->cfg.alpha = smh_alpha;
	agc->cfg.beta = smh_beta;

	FXS(f->sh, V27SH_RX_STATE) = u->state;
	FXS(f->sh, V27SH_RATE) = u->rate;
	FXS(f->sh, V27SH_TRAIN_LONG) = u->train_long;
	FXU(f->sh, V27SH_COUNTDOWN) = u->countdown;
	((struct fpm_fse *)(void *)(f->rx + V27RX_FSE))->mse = u->mse;
	((struct fpm_fse *)(void *)(f->rx + V27RX_FSE))->lms_force =
								u->lms_force;
	*(v27_rx_state_fn *)(void *)(f->sh + V27SH_STATE) =
					smh_handler_for(u->state, 0);
}

/* The blob's side wants the blob's handler in the slot. */
static void
smh_build_ref(struct hdx_fix *f, unsigned seed, const struct smh_setup *u)
{
	smh_build(f, seed, u);
	*(v27_rx_state_fn *)(void *)(f->sh + V27SH_STATE) =
					smh_handler_for(u->state, 1);
}

static void
smh_compare(struct hdx_fix *a, struct hdx_fix *b, long id)
{
	int ia = smh_id(*(v27_rx_state_fn *)(void *)(a->sh + V27SH_STATE), 1);
	int ib = smh_id(*(v27_rx_state_fn *)(void *)(b->sh + V27SH_STATE), 0);

	diff_eq_int("at %ld: machine, instance byte",
		    dem_first_diff(b->obj, a->obj, OBJ_SIZE, dem_skip_obj), -1,
		    id);
	diff_eq_int("at %ld: machine, shared byte",
		    dem_first_diff(b->sh, a->sh, SH_SIZE, smh_skip_sh), -1, id);
	diff_eq_int("at %ld: machine, receiver byte",
		    dem_first_diff(b->rx, a->rx, RX_SIZE, hdx_skip_rx), -1, id);
	diff_eq_int("at %ld: machine, installed handler", ib, ia, id);
	diff_eq_int("at %ld: machine, the handler is one of the six",
		    ia >= 0 && ia <= V27RX_STATE_ERROR, 1, id);
}

/* `RxNextStateV27`'s own body, with one reading changed. */
static void
sm_model(void *modem, int v)
{
	unsigned char *ro = (unsigned char *)modem;
	void *sh = FXP(ro, V27_OBJ_SHARED);
	void *rx;
	unsigned short blocks;
	unsigned char flags;
	int state;
	int is4800;

	if (v == SM_STATE_UNSIGNED)
		state = (int)FXU(sh, V27SH_RX_STATE);
	else
		state = FXS(sh, V27SH_RX_STATE);

	is4800 = FXS(sh, V27SH_RATE) == V27SH_RATE_4800;

	switch (state) {
	case V27RX_STATE_START:
		FXU(sh, V27SH_COUNTDOWN) = (unsigned short)
			(v == SM_EPOCH_BLOCKS_1 ? 1 : V27SH_EPOCH_DET_BLOCKS);
		*(v27_rx_state_fn *)(void *)FX(sh, V27SH_STATE) =
							RxHdxEpochDetV27;
		FXS(sh, V27SH_RX_STATE) = V27RX_STATE_EPOCH_DET;
		ro[V27_OBJ_STATUS_FLAGS2] &= (unsigned char)~1;
		if (v != SM_DATA_FLAG_KEPT)
			ro[V27_OBJ_STATUS_FLAGS] &= (unsigned char)~1;
		break;

	case V27RX_STATE_EPOCH_DET:
		if (v == SM_COUNTDOWN_RATE_SWAP)
			is4800 = !is4800;
		if (FXS(sh, V27SH_TRAIN_LONG) == 0
		    || v == SM_COUNTDOWN_TRAIN_IGNORED)
			blocks = (unsigned short)
				(is4800 ? V27SH_PROTOCOL_SHORT_4800
					: V27SH_PROTOCOL_SHORT_2400);
		else
			blocks = (unsigned short)
				(is4800 ? V27SH_PROTOCOL_LONG_4800
					: V27SH_PROTOCOL_LONG_2400);
		FXU(sh, V27SH_COUNTDOWN) = blocks;
		rx = FXP(ro, V27_OBJ_RX);
		*(v27_rx_state_fn *)(void *)FX(sh, V27SH_STATE) =
							RxHdxPrtcolV27;
		FXS(sh, V27SH_RX_STATE) = V27RX_STATE_PROTOCOL;
		ro[V27_OBJ_STATUS_FLAGS2] &= (unsigned char)~1;
		if (v != SM_DATA_FLAG_KEPT)
			ro[V27_OBJ_STATUS_FLAGS] &= (unsigned char)~1;
		if (v == SM_AGC_VALUE) {
			struct fpm_agc *g =
				(struct fpm_agc *)(void *)FX(rx, V27RX_AGC);
			static short a_hold, b_hold;

			a_hold = (short)(*g->cfg.alpha + 2);
			b_hold = (short)(*g->cfg.beta + 2);
			g->cfg.alpha = &a_hold;
			g->cfg.beta = &b_hold;
		} else if (v != SM_AGC_NO_STEP) {
			((struct fpm_agc *)(void *)
				FX(rx, V27RX_AGC))->cfg.alpha++;
			((struct fpm_agc *)(void *)
				FX(rx, V27RX_AGC))->cfg.beta++;
		}
		break;

	case V27RX_STATE_PROTOCOL:
		if (v != SM_NO_FREEZE)
			FPM_AGC_Freeze((struct fpm_agc *)(void *)
					FX(FXP(ro, V27_OBJ_RX), V27RX_AGC));
		sh = FXP(ro, V27_OBJ_SHARED);
		FXU(sh, V27SH_COUNTDOWN) = 0;
		*(v27_rx_state_fn *)(void *)FX(sh, V27SH_STATE) = RxHdxDataV27;
		FXS(sh, V27SH_RX_STATE) = V27RX_STATE_DATA;
		ro[V27_OBJ_STATUS_FLAGS] |= 1;
		ro[V27_OBJ_STATUS_FLAGS2] &= (unsigned char)~1;
		break;

	case V27RX_STATE_DATA:
		*(v27_rx_state_fn *)(void *)FX(sh, V27SH_STATE) = RxHdxIdleV27;
		FXS(sh, V27SH_RX_STATE) = V27RX_STATE_IDLE;
		FXU(sh, V27SH_COUNTDOWN) = 0;
		if (v != SM_INT_0004_KEPT)
			FXI(sh, V27SH_INT_0004) = 0;
		if (v == SM_IDLE_FLAG_AT_1D)
			ro[V27_OBJ_STATUS_FLAGS] |= 1;
		else
			ro[V27_OBJ_STATUS_FLAGS2] |= 1;
		if (v != SM_DATA_FLAG_KEPT && v != SM_IDLE_FLAG_AT_1D)
			ro[V27_OBJ_STATUS_FLAGS] &= (unsigned char)~1;
		break;

	case V27RX_STATE_IDLE:
		*(v27_rx_state_fn *)(void *)FX(sh, V27SH_STATE) = RxHdxDataV27;
		FXS(sh, V27SH_RX_STATE) = V27RX_STATE_DATA;
		ro[V27_OBJ_STATUS_FLAGS] |= 1;
		ro[V27_OBJ_STATUS_FLAGS2] &= (unsigned char)~1;
		if (v == SM_STATUS_67_SWAP)
			ro[V27_OBJ_STATUS] = (unsigned char)
				(is4800 ? V27_STATUS_ENTER_DATA_2400
					: V27_STATUS_ENTER_DATA_4800);
		else
			ro[V27_OBJ_STATUS] = (unsigned char)
				(is4800 ? V27_STATUS_ENTER_DATA_4800
					: V27_STATUS_ENTER_DATA_2400);
		break;

	default:
		flags = ro[V27_OBJ_STATUS_FLAGS];
		ro[V27_OBJ_STATUS_FLAGS2] &= (unsigned char)~1;
		ro[V27_OBJ_STATUS] = V27_STATUS_DEFAULT;
		if (v == SM_DEFAULT_KEEPS_CARRIER)
			ro[V27_OBJ_STATUS_FLAGS] = (unsigned char)
				((flags | V27_STATUS_FLAG_ERROR)
				 & (unsigned char)~1);
		else
			ro[V27_OBJ_STATUS_FLAGS] = (unsigned char)
				((flags | V27_STATUS_FLAG_ERROR)
				 & (unsigned char)~(unsigned char)
					(V27_STATUS_FLAG_CARRIER | 1));
		break;
	}
}

static unsigned long
smh_mark(unsigned long m, struct hdx_fix *f, short r, unsigned short count,
	 const short *out, int ref)
{
	struct fpm_agc *g = (struct fpm_agc *)(void *)(f->rx + V27RX_AGC);

	m = hdx_mark(m, f, r, count, out);
	m = m * 31u + (unsigned)(smh_id(*(v27_rx_state_fn *)(void *)
					(f->sh + V27SH_STATE), ref) + 2);
	m = m * 31u + (unsigned short)FXS(f->sh, V27SH_RX_STATE);
	m = m * 31u + FXU(f->sh, V27SH_COUNTDOWN);
	m = m * 31u + f->obj[V27_OBJ_STATUS_FLAGS2];
	m = m * 31u + (unsigned)(FXI(f->sh, V27SH_INT_0004) != 0);
	m = m * 131u + (unsigned short)*g->cfg.alpha;
	m = m * 131u + (unsigned short)*g->cfg.beta;
	m = m * 31u + (unsigned)(g->freeze != 0);
	return m;
}

/*
 * The four handlers' own bodies, with one reading changed.  The real callees
 * are used throughout -- these variants are about the ORDER and the TESTS the
 * handlers impose, not about the chain underneath them.
 */
static short
smh_model(void *modem, short *in, short *out, unsigned short *count, int which,
	  int v)
{
	unsigned char *ro = (unsigned char *)modem;
	void *sh;
	unsigned short n = 0;
	unsigned short left;
	unsigned char flags;

	if (which == SMH_START) {
		if (v != SH_START_NO_CLEAR)
			ro[V27_OBJ_STATUS_FLAGS] &= (unsigned char)
				~(unsigned char)V27_STATUS_FLAG_CARRIER;
		ro[V27_OBJ_STATUS] = V27_STATUS_START;
		DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
		if (CarrierDetectV27(modem))
			sm_model(modem, SM_NONE);
		*count = 0;
		return 0;
	}

	if (which == SMH_IDLE) {
		struct fpm_fse *fse;
		int go;

		DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
		*count = 0;
		ro[V27_OBJ_STATUS_FLAGS] &= (unsigned char)
			~(unsigned char)V27_STATUS_FLAG_CARRIER;
		ro[V27_OBJ_STATUS] = V27_STATUS_IDLE;
		if (CarrierDetectV27(modem))
			ro[V27_OBJ_STATUS_FLAGS] |= V27_STATUS_FLAG_CARRIER;

		fse = (struct fpm_fse *)(void *)
			FX(FXP(ro, V27_OBJ_RX), V27RX_FSE);
		go = (ro[V27_OBJ_STATUS_FLAGS] & V27_STATUS_FLAG_CARRIER) != 0;
		if (v == SH_IDLE_NO_CARRIER_GATE)
			go = 1;
		if (v == SH_IDLE_NO_MSE)
			go = go && 1;
		else if (v == SH_IDLE_MSE_STRICT)
			go = go && fse->mse < V27RX_MSE_IDLE_OK;
		else
			go = go && fse->mse <= V27RX_MSE_IDLE_OK;
		if (go)
			sm_model(modem, SM_NONE);
		return 0;
	}

	/* EPOCH_DET and PROTOCOL share everything but two lines. */
	n = DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	if (which == SMH_PRTCOL && v != SH_PRTCOL_NO_DESCRAMBLE)
		DescrambleDataV27(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (CarrierDetectV27(modem) == 0) {
		sh = FXP(ro, V27_OBJ_SHARED);
		*(v27_rx_state_fn *)(void *)FX(sh, V27SH_STATE) =
							RxHdxErrorV27;
		FXS(sh, V27SH_RX_STATE) = (short)
			(v == SH_ERROR_STATE_4 ? V27RX_STATE_IDLE
					       : V27RX_STATE_ERROR);
		flags = ro[V27_OBJ_STATUS_FLAGS];
		ro[V27_OBJ_STATUS] = V27_STATUS_ERROR;
		ro[V27_OBJ_STATUS_FLAGS] = (unsigned char)
			((flags | V27_STATUS_FLAG_ERROR)
			 & (unsigned char)~(unsigned char)
					V27_STATUS_FLAG_CARRIER);
		return 0;
	}

	ro[V27_OBJ_STATUS_FLAGS] |= V27_STATUS_FLAG_CARRIER;
	sh = FXP(ro, V27_OBJ_SHARED);
	ro[V27_OBJ_STATUS] = V27_STATUS_TRAINING;

	left = (unsigned short)(FXU(sh, V27SH_COUNTDOWN) - 1);
	if (v != SH_COUNTDOWN_NO_STORE)
		FXU(sh, V27SH_COUNTDOWN) = left;

	if (which == SMH_EPOCH) {
		int hold;

		if (v == SH_COUNTDOWN_UNSIGNED)
			hold = left > 0;
		else
			hold = (short)left > 0;
		if (v == SH_EPOCH_NO_DET)
			hold = hold && 1;
		else if (v == SH_EPOCH_AND)
			hold = hold || EpochDetectV27(modem) == 0;
		else
			hold = hold && (short)EpochDetectV27(modem) == 0;
		if (hold)
			return 0;
		sm_model(modem, SM_NONE);
		return 0;
	}

	if (v == SH_COUNTDOWN_UNSIGNED ? left > 0 : (short)left > 0)
		return (short)(v == SH_PRTCOL_RET_ALWAYS ? (short)n : 0);

	ro[V27_OBJ_STATUS] = (unsigned char)
		(FXS(sh, V27SH_RATE) == V27SH_RATE_2400
		 ? V27_STATUS_ENTER_DATA_2400 : V27_STATUS_ENTER_DATA_4800);
	sm_model(modem, SM_NONE);

	if (v == SH_PRTCOL_RET_ZERO)
		return 0;
	return (short)n;
}

/* ---- the machine, from START, through the real callees ---------------- */

static void
run_smh_machine(unsigned seed, const struct smh_setup *u, unsigned short count,
		int level, long tag)
{
	int blk, i;

	(void)level;
	smh_signal(seed ^ 0x5a417000u, u->level);
	smh_build_ref(&sma, seed, u);
	smh_build(&smb, seed, u);

	for (blk = 0; blk < SMH_BLOCKS; blk++) {
		unsigned short ca = count, cb = count;
		short ra, rb;
		v27_rx_state_fn ha, hb;
		const short *alpha_before;
		long id = tag * 100 + blk;

		alpha_before = ((struct fpm_agc *)(void *)
				(sma.rx + V27RX_AGC))->cfg.alpha;

		for (i = 0; i < DEM_BUF; i++) {
			hdx_out_a[i] = (short)0xbeef;
			hdx_out_b[i] = (short)0xbeef;
			hdx_work[i] = hdx_in[i];
		}
		ha = *(v27_rx_state_fn *)(void *)(sma.sh + V27SH_STATE);
		smh_state_seen[smh_id(ha, 1) + 1]++;
		ra = ha(sma.obj, hdx_work, hdx_out_a, &ca);
		for (i = 0; i < DEM_BUF; i++)
			smh_work_ref[i] = hdx_work[i];

		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];
		hb = *(v27_rx_state_fn *)(void *)(smb.sh + V27SH_STATE);
		rb = hb(smb.obj, hdx_work, hdx_out_b, &cb);

		diff_eq_int("at %ld: machine block returned", (long)rb,
			    (long)ra, id);
		diff_eq_int("at %ld: machine block left *count", (long)cb,
			    (long)ca, id);
		diff_eq_int("at %ld: machine block output buffer",
			    dem_first_diff((const unsigned char *)hdx_out_b,
					   (const unsigned char *)hdx_out_a,
					   DEM_BUF * 2, 0), -1, id);
		diff_eq_int("at %ld: machine block samples, rewritten in place",
			    dem_first_diff((const unsigned char *)hdx_work,
					   (const unsigned char *)smh_work_ref,
					   DEM_BUF * 2, 0), -1, id);
		smh_compare(&sma, &smb, id);

		if (((struct fpm_agc *)(void *)
			(sma.rx + V27RX_AGC))->cfg.alpha != alpha_before)
			smh_agc_stepped++;
		smh_blocks++;
	}

	hdx_free(&sma);
	hdx_free(&smb);
}

/* ---- one function at a time, with the preconditions planted ----------- */

static void
run_smh_one(unsigned seed, const struct smh_setup *u, int which,
	    unsigned short count, int level, long tag)
{
	unsigned long marka, markc;
	int d, i, blk;

	if (which == SMH_NEXT) {
		smh_build_ref(&sma, seed, u);
		smh_build(&smb, seed, u);
		smh_arm[u->state >= 0 && u->state <= 4 ? u->state + 1 : 0]++;
		ref_RxNextStateV27(sma.obj);
		RxNextStateV27(smb.obj);
		smh_compare(&sma, &smb, tag);
		marka = smh_mark(0, &sma, 0, 0, hdx_in, 1);
		hdx_free(&sma);
		hdx_free(&smb);

		for (d = 0; d < (int)SM_MAX; d++) {
			smh_build(&smc, seed, u);
			sm_model(smc.obj, d);
			markc = smh_mark(0, &smc, 0, 0, hdx_in, 0);
			if (d == SM_NONE)
				diff_eq_int("RxNextStateV27 model (%ld)",
					    markc == marka, 1, tag);
			else if (markc != marka)
				smh_sep[d]++;
			hdx_free(&smc);
		}
		return;
	}

	(void)level;
	smh_signal(seed ^ 0x7b1e0000u, u->level);
	smh_build_ref(&sma, seed, u);
	smh_build(&smb, seed, u);
	marka = 0;
	smh_state_seen[(which == SMH_IDLE ? V27RX_STATE_IDLE
					  : which - 1) + 1]++;

	for (blk = 0; blk < 4; blk++) {
		unsigned short ca = count, cb = count;
		short ra, rb;
		long id = tag * 100 + blk;
		unsigned short cd_before = FXU(sma.sh, V27SH_COUNTDOWN);
		int lms_before = ((struct fpm_fse *)(void *)
				  (sma.rx + V27RX_FSE))->lms_force;
		short st_before = FXS(sma.sh, V27SH_RX_STATE);

		for (i = 0; i < DEM_BUF; i++) {
			hdx_out_a[i] = (short)0xbeef;
			hdx_out_b[i] = (short)0xbeef;
			hdx_work[i] = hdx_in[i];
		}
		switch (which) {
		case SMH_START:
			ra = ref_RxHdxStartV27(sma.obj, hdx_work, hdx_out_a,
					       &ca);
			break;
		case SMH_EPOCH:
			ra = ref_RxHdxEpochDetV27(sma.obj, hdx_work, hdx_out_a,
						  &ca);
			break;
		case SMH_PRTCOL:
			ra = ref_RxHdxPrtcolV27(sma.obj, hdx_work, hdx_out_a,
						&ca);
			break;
		default:
			ra = ref_RxHdxIdleV27(sma.obj, hdx_work, hdx_out_a,
					      &ca);
			break;
		}
		for (i = 0; i < DEM_BUF; i++)
			smh_work_ref[i] = hdx_work[i];

		for (i = 0; i < DEM_BUF; i++)
			hdx_work[i] = hdx_in[i];
		switch (which) {
		case SMH_START:
			rb = RxHdxStartV27(smb.obj, hdx_work, hdx_out_b, &cb);
			break;
		case SMH_EPOCH:
			rb = RxHdxEpochDetV27(smb.obj, hdx_work, hdx_out_b,
					      &cb);
			break;
		case SMH_PRTCOL:
			rb = RxHdxPrtcolV27(smb.obj, hdx_work, hdx_out_b, &cb);
			break;
		default:
			rb = RxHdxIdleV27(smb.obj, hdx_work, hdx_out_b, &cb);
			break;
		}

		diff_eq_int("at %ld: handler returned", (long)rb, (long)ra, id);
		diff_eq_int("at %ld: handler left *count", (long)cb, (long)ca,
			    id);
		diff_eq_int("at %ld: handler output buffer",
			    dem_first_diff((const unsigned char *)hdx_out_b,
					   (const unsigned char *)hdx_out_a,
					   DEM_BUF * 2, 0), -1, id);
		diff_eq_int("at %ld: handler samples, rewritten in place",
			    dem_first_diff((const unsigned char *)hdx_work,
					   (const unsigned char *)smh_work_ref,
					   DEM_BUF * 2, 0), -1, id);
		smh_compare(&sma, &smb, id);

		/*
		 * The coverage counters are read off the REFERENCE side, so
		 * they say what the BLOB did and not what we did.
		 */
		if ((sma.obj[V27_OBJ_STATUS_FLAGS] & V27_STATUS_FLAG_CARRIER)
		    != 0)
			smh_carrier_up++;
		else
			smh_carrier_lost++;
		if (sma.obj[V27_OBJ_STATUS] == V27_STATUS_ERROR)
			smh_error_arm++;
		if (blk == 0 && u->mtd_fires
		    && ((struct fpm_fse *)(void *)
			(sma.rx + V27RX_FSE))->mse == u->mse)
			smh_tone_kept++;

		/*
		 * The classification is off the REFERENCE side and off what
		 * was true BEFORE the call, so these say what the blob did.
		 * `st_before` is what the state number was on entry; a state
		 * that did not move is a hold.
		 */
		if (which == SMH_PRTCOL) {
			if (FXS(sma.sh, V27SH_RX_STATE) == st_before)
				smh_prtcol_hold++;
			else if (FXS(sma.sh, V27SH_RX_STATE)
				 != V27RX_STATE_ERROR)
				smh_prtcol_handover++;
		} else if (which == SMH_EPOCH) {
			if (FXS(sma.sh, V27SH_RX_STATE) == st_before)
				smh_epoch_hold++;
			else if (FXS(sma.sh, V27SH_RX_STATE)
				 != V27RX_STATE_ERROR) {
				if ((short)(cd_before - 1) > 0
				    && lms_before != 0)
					smh_epoch_by_det++;
				else
					smh_epoch_by_count++;
			}
		} else if (which == SMH_IDLE) {
			if (FXS(sma.sh, V27SH_RX_STATE) == st_before)
				smh_idle_hold++;
			else
				smh_idle_restart++;
			if (((struct fpm_fse *)(void *)
			     (sma.rx + V27RX_FSE))->mse == V27RX_MSE_IDLE_OK
			    && (sma.obj[V27_OBJ_STATUS_FLAGS]
				& V27_STATUS_FLAG_CARRIER) != 0)
				smh_idle_edge++;
		} else {
			if (FXS(sma.sh, V27SH_RX_STATE) == st_before)
				smh_start_hold++;
			else
				smh_start_advance++;
		}

		marka = smh_mark(marka, &sma, ra, ca, hdx_out_a, 1);
	}

	hdx_free(&sma);
	hdx_free(&smb);

	for (d = 0; d < (int)SH_MAX; d++) {
		smh_signal(seed ^ 0x7b1e0000u, u->level);
		smh_build(&smc, seed, u);
		markc = 0;
		for (blk = 0; blk < 4; blk++) {
			unsigned short cc = count;
			short rc;

			for (i = 0; i < DEM_BUF; i++) {
				hdx_work[i] = hdx_in[i];
				hdx_out_b[i] = (short)0xbeef;
			}
			rc = smh_model(smc.obj, hdx_work, hdx_out_b, &cc,
				       which, d);
			markc = smh_mark(markc, &smc, rc, cc, hdx_out_b, 0);
		}
		if (d == SH_NONE)
			diff_eq_int("handler model (%ld)", markc == marka, 1,
				    tag);
		else if (markc != marka)
			smh_hsep[d]++;
		hdx_free(&smc);
	}
}

static int
run_smh(void)
{
	static const struct smh_setup setups[] = {
	  /* st rate long cnt  g04 mtd act  mse   lms lvl */
	  {  0, 0,   0,   0,   0,  0,  1,   0x3000, 0, 0 },
	  {  0, 1,   0,   0,   0,  0,  1,   0x3000, 0, 1 },
	  {  0, 0,   1,   0,   0,  0,  1,   0x3000, 0, 1 },
	  {  0, 1,   1,   0,   0,  0,  1,   0x3000, 0, 0 },
	  {  1, 0,   0,   3,   0,  0,  1,   0x3000, 0, 1 },
	  {  1, 1,   1,   1,   0,  0,  1,   0x3000, 0, 0 },
	  {  2, 0,   0,   2,   0,  0,  1,   0x3000, 0, 1 },
	  {  2, 1,   1,   1,   0,  0,  1,   0x3000, 0, 0 },
	  {  4, 0,   0,   0,   0,  0,  1,   0x3000, 0, 1 },
	  {  3, 1,   0,   0,   0,  0,  1,   0x3000, 0, 0 },
	  /* SILENCE: the gain control's gate closes, so carrier is denied
	   * and every handler that has one takes its error arm.           */
	  {  1, 0,   0,   2,   0,  0,  0,   0x3000, 0, 2 },
	  {  2, 1,   1,   2,   0,  0,  0,   0x3000, 0, 2 },
	  {  0, 0,   0,   1,   0,  0,  1,   0x3000, 0, 2 },
	  {  4, 1,   1,   1,   0,  0,  1,   0x3000, 0, 2 },
	  /* the tone abort: the equaliser keeps exactly what is planted */
	  {  0, 0,   0,   1,   0,  1,  1,   0x0100, 0, 1 },
	  {  0, 1,   0,   1,   0,  1,  1,   0x1fff, 1, 1 },
	  {  0, 0,   1,   2,   0,  1,  1,   0x2000, 1, 1 },
	  {  0, 1,   1,   4,   0,  1,  1,   0x7000, 0, 1 },
	  /* the same, but with the error small AND the carrier denied:
	   * the two terms of `RxHdxIdleV27`'s restart, separated.         */
	  {  0, 0,   0,   2,   0,  1,  1,   0x0100, 0, 2 },
	  {  4, 1,   1,   3,   0,  1,  1,   0x0100, 1, 2 },
	  /* the countdown that is already zero, so the decrement wraps */
	  {  0, 0,   0,   0,   0,  1,  1,   0x0800, 0, 1 },
	  {  0, 1,   1,   0,   0,  1,  1,   0x0800, 1, 0 },
	  /*
	   * EXACTLY V27RX_MSE_IDLE_OK, four ways, because the `<=` and the
	   * `<` readings of `RxHdxIdleV27`'s restart test agree on every
	   * other value.  The tone abort is what keeps the planted mse from
	   * being overwritten by the equaliser before the test reads it.
	   */
	  {  0, 0,   0,   1,   0,  1,  1,   0x1fff, 0, 0 },
	  {  0, 1,   0,   1,   0,  1,  1,   0x1fff, 0, 1 },
	  {  0, 0,   1,   2,   0,  1,  1,   0x1fff, 1, 0 },
	  {  0, 1,   1,   2,   0,  1,  1,   0x1fff, 1, 1 }
	};
	/* Every state the switch can be given, including out of range. */
	static const short next_states[] = {
		0, 1, 2, 3, 4, 5, 6, 7, -1, -2, 0x7fff, (short)0x8000, 0x0100
	};
	static const unsigned short counts[] = { 144, 700 };
	struct smh_setup u;
	int s, c, w, level;
	long tag = 0;

	smh_mtd_always = dcd_mtd_cfg;
	smh_mtd_always.ratio = 0x7fff;
	smh_mtd_always.min_level = 0;

	diff_begin("V.27ter receive state machine");

	/*
	 * THE MACHINE, from each setup's own state.  Most of them start in
	 * START, which is the only state `V27RX_create` installs; the ones
	 * that start in EPOCH_DET or PROTOCOL are there because those are the
	 * only two states with an error arm, so they are the only way the
	 * machine can ever run `RxHdxErrorV27`.
	 */
	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++)
	for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++) {
		u = setups[s];
		level = (int)(tag & 1);
		run_smh_machine(0x51a17000u + (unsigned)tag * 0x9e3779b9u, &u,
				counts[c], level, tag);
		tag++;
	}

	/* Each handler on its own, from the setup's own planted state. */
	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++)
	for (w = SMH_START; w < SMH_WHICH_MAX; w++) {
		level = (int)(tag & 1);
		run_smh_one(0x60d10000u + (unsigned)tag * 0x9e3779b9u,
			    &setups[s], w, counts[tag & 1], level, tag);
		tag++;
	}

	/* And the transition table over every state number it can see. */
	for (s = 0; s < (int)(sizeof(next_states) / sizeof(next_states[0]));
	     s++)
	for (c = 0; c < 4; c++) {
		u = setups[c];
		u.state = next_states[s];
		u.rate = (short)(c & 1);
		u.train_long = (short)((c >> 1) & 1);
		/*
		 * Non-zero, so the DATA arm's `V27SH_INT_0004 = 0` is a
		 * CHANGE.  With the field already clear that store is
		 * invisible and the variant that omits it separates nothing.
		 */
		u.gate_04 = 0x1234;
		run_smh_one(0x71e50000u + (unsigned)tag * 0x9e3779b9u, &u,
			    SMH_NEXT, 0, 0, tag);
		tag++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* 19.  V27RX_create                                                      */
/*
 * The constructor, compared END TO END: both sides are asked to build a whole
 * receiver from the same `struct v27rx_cfg` and the two results are compared
 * byte for byte, with every field that HAS to differ skipped and compared by
 * identity instead.
 *
 * SO THIS TEST IS BROADER THAN THE FUNCTION.  `V27RX_create` calls
 * `FPM_MTD_create`, `FPM_AGC_init`, `FPM_MRF_init`, `FPM_SRE_init`,
 * `FPM_FSE_init` and `SDMv27_init`, and each side calls ITS OWN -- there is no
 * way to hand the blob's constructor our modules.  A difference anywhere in
 * that chain lands here.  That is a feature and not a confound: all six are
 * already written and have their own sections or their own binaries, so a
 * failure here that is not in this function will be a failure there too.
 *
 * WHAT CANNOT BE COMPARED BY BYTES, and what replaces it:
 *
 *   handle +0x20..+0x33   five pointers INTO each side's own equaliser.
 *                         Compared by identity against that side's `fse`.
 *   handle +0x50, +0x54   the two blocks.  Compared through everything else.
 *   shared +0x00, +0x18   each side's own `struct fpm_mtd`.  Their CONTENTS
 *                         and their accumulator arrays are compared instead.
 *   shared +0x0c          `RxHdxStartV27` against `ref_RxHdxStartV27`.
 *   shared +0x1c          the V.21 scratch buffer, whose contents come
 *                         straight from `sysdep_malloc` and are not written.
 *   fse.cfg +0x2c, +0x30  `owner` is each side's own decoder block and
 *                         `decision` each side's own `V27RX_epoch_det`.
 *   the four modules'     already skipped by `dem_skip_rx`; the buffers
 *   heap buffers          THEMSELVES are compared where init fills them.
 *
 * THE THREE CONTEXT POINTERS ARE COMPARED BY VALUE, not skipped, because both
 * sides are given the same one: `cfg->ptr_0018` reaches `fpm_mrf_cfg::aux`,
 * `fpm_fse_cfg::reserved34` and the four bytes `fpm_sre.h` calls
 * `pad34`/`pad36`.  That third one is F9306's site and this is what measures
 * it -- a `short`-at-a-time reading would put the halves elsewhere.
 *
 * AND THE DECISIONS ARE MODELLED SEPARATELY.  `crt_read` lifts 68 values out
 * of a finished receiver and `crt_expect` recomputes them from the setup and
 * the tables; the model is compared against the BLOB's object, and thirteen
 * variants of it are shown to separate.  Reimplementing all 2,210 bytes would
 * duplicate `src/fax/v27.c` line for line and measure nothing extra: every
 * choice this function makes is a choice about one of those 68 values.
 */
extern void *ref_V27RX_create(void *modem, const struct v27rx_cfg *cfg);

/*
 * THE TABLES ARE TWO SETS, and that is the whole reason this section needs a
 * table map.  `symmap.py` renames every symbol the blob DEFINES, data
 * included, so `ref_V27RX_create` fills its configurations with pointers into
 * `ref_V27RX_MRF_FILT` and ours with pointers into `V27RX_MRF_FILT` -- same
 * bytes, different addresses.  Every such pointer is therefore skipped in the
 * byte comparison and answered instead by "which of this side's two tables",
 * which is the question the rate selection is actually about.
 */
extern const short *ref_V27RX_MRF_FILT[2];
extern const short *ref_V27RX_SRE_FILT[2];
extern const short *const ref_V27RX_XB_COFFS[2];
extern short *ref_V27RX_XCLOCK[2];
extern short *ref_V27RX_YCLOCK[2];
extern short *ref_V27RX_SRE_PLLK1[2];
extern short *ref_V27RX_SRE_PLLK2[2];
extern const short *const ref_V27RX_FSE_IFILT[2];
extern const short *const ref_V27RX_FSE_QFILT[2];
extern short *ref_V27RX_CRR_TABLE[2];
extern short *ref_V27RX_DEC_PMAP[2];
extern short *ref_V27RX_DEC_LAST_PHASE[2];
extern short ref_V27_MTD_COEFF_2400[10];
extern short ref_V27_MTD_COEFF_4800[10];
extern short ref_V21_CHAN2_MTD_COEFF[10];

#define CRT_TABLES	13

static const void *crt_tab[2][CRT_TABLES][2];
static const void *crt_v21coef[2];

static void
crt_tables_init(void)
{
	crt_tab[0][0][0] = V27RX_MRF_FILT[0];
	crt_tab[0][0][1] = V27RX_MRF_FILT[1];
	crt_tab[0][1][0] = V27RX_SRE_FILT[0];
	crt_tab[0][1][1] = V27RX_SRE_FILT[1];
	crt_tab[0][2][0] = V27RX_XB_COFFS[0];
	crt_tab[0][2][1] = V27RX_XB_COFFS[1];
	crt_tab[0][3][0] = V27RX_XCLOCK[0];
	crt_tab[0][3][1] = V27RX_XCLOCK[1];
	crt_tab[0][4][0] = V27RX_YCLOCK[0];
	crt_tab[0][4][1] = V27RX_YCLOCK[1];
	crt_tab[0][5][0] = V27RX_SRE_PLLK1[0];
	crt_tab[0][5][1] = V27RX_SRE_PLLK1[1];
	crt_tab[0][6][0] = V27RX_SRE_PLLK2[0];
	crt_tab[0][6][1] = V27RX_SRE_PLLK2[1];
	crt_tab[0][7][0] = V27RX_FSE_IFILT[0];
	crt_tab[0][7][1] = V27RX_FSE_IFILT[1];
	crt_tab[0][8][0] = V27RX_FSE_QFILT[0];
	crt_tab[0][8][1] = V27RX_FSE_QFILT[1];
	crt_tab[0][9][0] = V27RX_CRR_TABLE[0];
	crt_tab[0][9][1] = V27RX_CRR_TABLE[1];
	crt_tab[0][10][0] = V27RX_DEC_PMAP[0];
	crt_tab[0][10][1] = V27RX_DEC_PMAP[1];
	crt_tab[0][11][0] = V27RX_DEC_LAST_PHASE[0];
	crt_tab[0][11][1] = V27RX_DEC_LAST_PHASE[1];
	crt_tab[0][12][0] = V27_MTD_COEFF_2400;
	crt_tab[0][12][1] = V27_MTD_COEFF_4800;
	crt_v21coef[0] = V21_CHAN2_MTD_COEFF;

	crt_tab[1][0][0] = ref_V27RX_MRF_FILT[0];
	crt_tab[1][0][1] = ref_V27RX_MRF_FILT[1];
	crt_tab[1][1][0] = ref_V27RX_SRE_FILT[0];
	crt_tab[1][1][1] = ref_V27RX_SRE_FILT[1];
	crt_tab[1][2][0] = ref_V27RX_XB_COFFS[0];
	crt_tab[1][2][1] = ref_V27RX_XB_COFFS[1];
	crt_tab[1][3][0] = ref_V27RX_XCLOCK[0];
	crt_tab[1][3][1] = ref_V27RX_XCLOCK[1];
	crt_tab[1][4][0] = ref_V27RX_YCLOCK[0];
	crt_tab[1][4][1] = ref_V27RX_YCLOCK[1];
	crt_tab[1][5][0] = ref_V27RX_SRE_PLLK1[0];
	crt_tab[1][5][1] = ref_V27RX_SRE_PLLK1[1];
	crt_tab[1][6][0] = ref_V27RX_SRE_PLLK2[0];
	crt_tab[1][6][1] = ref_V27RX_SRE_PLLK2[1];
	crt_tab[1][7][0] = ref_V27RX_FSE_IFILT[0];
	crt_tab[1][7][1] = ref_V27RX_FSE_IFILT[1];
	crt_tab[1][8][0] = ref_V27RX_FSE_QFILT[0];
	crt_tab[1][8][1] = ref_V27RX_FSE_QFILT[1];
	crt_tab[1][9][0] = ref_V27RX_CRR_TABLE[0];
	crt_tab[1][9][1] = ref_V27RX_CRR_TABLE[1];
	crt_tab[1][10][0] = ref_V27RX_DEC_PMAP[0];
	crt_tab[1][10][1] = ref_V27RX_DEC_PMAP[1];
	crt_tab[1][11][0] = ref_V27RX_DEC_LAST_PHASE[0];
	crt_tab[1][11][1] = ref_V27RX_DEC_LAST_PHASE[1];
	crt_tab[1][12][0] = ref_V27_MTD_COEFF_2400;
	crt_tab[1][12][1] = ref_V27_MTD_COEFF_4800;
	crt_v21coef[1] = ref_V21_CHAN2_MTD_COEFF;
}

#define CRT_N		88

struct crt_setup {
	int		use_default;	/* pass a null `cfg`               */
	short		bit_rate;
	int		short_train;
	int		preallocate;	/* hand it a handle it must reuse  */
};

enum crt_defect {
	CR_NONE = 0,
	CR_RATE_SWAP,
	CR_TRAIN_IGNORED,
	CR_FSE_BLOCK_SWAP,
	CR_PPM_STEP_SWAP,
	CR_PPM_UNIT_100,
	CR_QLIMIT_SWAP,
	CR_NBITS_SWAP,
	CR_AGC_BLOCK_ALWAYS,
	CR_RMS_DIV_3,
	CR_RMS_LEN_2,
	CR_EIGHT_PHASE_INV,
	CR_UNKNOWN_KEEPS_ERROR,
	CR_MTD_RATIO_SWAP,
	CR_DEFAULT_IS_2400,
	CR_MAX
};

static long crt_sep[CR_MAX];
static long crt_trials, crt_rate_seen[3], crt_train_seen[2], crt_default_seen;
static long crt_prealloc_seen, crt_fields;

/* One place for the context pointer, so both sides get the same value. */
static int crt_aux_object;

/*
 * What `cfg->ptr_0018` was for THIS trial, which is not always
 * `&crt_aux_object`: a null `cfg` means `V27RX_CFG`, whose own `ptr_0018` is
 * zero.  Set by `crt_one` before it reads either side back, so the three
 * "the context pointer arrived" fields stay a real check on the default path
 * instead of quietly becoming "it is not the sentinel".
 */
static void *crt_aux_want;

static int
crt_which(const void *p, const void *a, const void *b)
{
	if (p == a)
		return 0;
	if (p == b)
		return 1;
	return -1;
}

static void
crt_read(void *h, long *v, int is_ref)
{
	void *sh = FXP(h, V27_OBJ_SHARED);
	void *rx = FXP(h, V27_OBJ_RX);
	void *dec = FX(rx, V27RX_DEC);
	struct fpm_agc *agc = (struct fpm_agc *)(void *)FX(rx, V27RX_AGC);
	struct fpm_mrf *mrf = (struct fpm_mrf *)(void *)FX(rx, V27RX_MRF);
	struct fpm_sre *sre = (struct fpm_sre *)(void *)FX(rx, V27RX_SRE);
	struct fpm_fse *fse = (struct fpm_fse *)(void *)FX(rx, V27RX_FSE);
	struct sdmv27 *sdm = (struct sdmv27 *)(void *)FX(rx, V27RX_SDM);
	struct fpm_mtd *mtd = (struct fpm_mtd *)FXP(sh, V27SH_MTD);
	struct fpm_mtd *m21 = (struct fpm_mtd *)FXP(sh, V27SH_MTD_V21);
	void *aux = crt_aux_want;
	int n = 0;

#define T(k)	(crt_tab[is_ref][(k)])


	v[n++] = FXS(sh, V27SH_RATE);
	v[n++] = FXS(sh, V27SH_TRAIN_LONG);
	v[n++] = FXS(sh, V27SH_RX_STATE);
	v[n++] = FXU(sh, V27SH_COUNTDOWN);
	v[n++] = FXI(sh, V27SH_INT_0004);
	v[n++] = FXU(sh, V27SH_V21_SAMPLES);
	v[n++] = FXS(sh, V27SH_V21_ARMED);
	v[n++] = smh_id(*(v27_rx_state_fn *)(void *)FX(sh, V27SH_STATE),
			is_ref);

	v[n++] = agc->cfg.block_len;
	v[n++] = agc->cfg.ref_level;
	v[n++] = agc->cfg.acquire_level;

	v[n++] = mrf->cfg.branches;
	v[n++] = mrf->cfg.decimate;
	v[n++] = mrf->cfg.taps;
	v[n++] = crt_which(mrf->cfg.coeff, T(0)[0], T(0)[1]);
	v[n++] = mrf->cfg.aux == aux;

	v[n++] = sre->cfg.clock_len;
	v[n++] = sre->cfg.groups_acq;
	v[n++] = sre->cfg.groups_trk;
	v[n++] = sre->cfg.settle;
	v[n++] = sre->cfg.coeffs;
	v[n++] = sre->cfg.mag_hi;
	v[n++] = sre->cfg.mag_lo;
	v[n++] = sre->cfg.err_hi;
	v[n++] = sre->cfg.err_lo;
	v[n++] = sre->cfg.rms_min;
	v[n++] = sre->cfg.rms_len;
	v[n++] = crt_which(sre->cfg.proto, T(1)[0], T(1)[1]);
	v[n++] = crt_which(sre->cfg.disc, T(2)[0], T(2)[1]);
	v[n++] = crt_which(sre->cfg.xclock, T(3)[0], T(3)[1]);
	v[n++] = crt_which(sre->cfg.yclock, T(4)[0], T(4)[1]);
	v[n++] = crt_which(sre->cfg.pll_k1, T(5)[0], T(5)[1]);
	v[n++] = crt_which(sre->cfg.pll_k2, T(6)[0], T(6)[1]);
	v[n++] = memcmp(&sre->cfg.pad34, &aux, sizeof aux) == 0;
	v[n++] = sre->ppm_step;
	v[n++] = sre->ppm_period;
	v[n++] = sre->ppm_scale;
	v[n++] = sre->ppm_n_max;

	v[n++] = fse->cfg.block;
	v[n++] = fse->cfg.interp;
	v[n++] = fse->cfg.taps;
	v[n++] = fse->cfg.mu[0];
	v[n++] = fse->cfg.mu[1];
	v[n++] = fse->cfg.clk_mod;
	v[n++] = fse->cfg.clk_inc;
	v[n++] = fse->cfg.train_sym;
	v[n++] = fse->cfg.err_hi;
	v[n++] = fse->cfg.err_lo;
	v[n++] = crt_which(fse->cfg.icoff, T(7)[0], T(7)[1]);
	v[n++] = crt_which(fse->cfg.qcoff, T(8)[0], T(8)[1]);
	v[n++] = crt_which(fse->cfg.clk, T(9)[0], T(9)[1]);
	v[n++] = fse->cfg.owner == dec;
	v[n++] = fse->cfg.decision == (is_ref ? ref_V27RX_epoch_det
					      : V27RX_epoch_det);
	v[n++] = fse->cfg.reserved34 == aux;

	v[n++] = FXU(rx, V27RX_Q_LIMIT);
	v[n++] = FXS(rx, V27RX_Q_FLAG) | FXS(rx, V27RX_Q_ACC)
		 | (short)FXU(rx, V27RX_Q_COUNT);
	v[n++] = FXS(rx, V27RX_RMS_ON);
	v[n++] = FXS(rx, V27RX_RMS_REF) | (short)FXU(rx, V27RX_RMS_COUNT);
	v[n++] = sdm->nbits;

	v[n++] = FXI(dec, V27DEC_EIGHT_PHASE);
	v[n++] = FXU(dec, V27DEC_PHASE_MASK);
	v[n++] = FXI(dec, V27DEC_TRAIN_SHORT);
	v[n++] = FXS(dec, V27DEC_EPOCH_AVG);
	v[n++] = crt_which(FXP(dec, V27DEC_PMAP), T(10)[0], T(10)[1]);
	v[n++] = crt_which(FXP(dec, V27DEC_ANGLES), T(11)[0], T(11)[1]);

	v[n++] = mtd->cfg.tones * 65536L + mtd->cfg.ratio;
	v[n++] = mtd->cfg.min_level;
	v[n++] = crt_which(mtd->cfg.coeff, T(12)[0], T(12)[1]);
	v[n++] = m21->cfg.tones * 65536L + m21->cfg.ratio;
	v[n++] = m21->cfg.min_level;
	v[n++] = m21->cfg.coeff == crt_v21coef[is_ref];

	v[n++] = ((unsigned char *)h)[V27_OBJ_STATUS] * 65536L
		 + ((unsigned char *)h)[V27_OBJ_STATUS_FLAGS] * 256L
		 + ((unsigned char *)h)[V27_OBJ_STATUS_FLAGS2];
	v[n++] = FXI(rx, V27RX_EN_SRE_ADAPT) + 2 * FXI(rx, V27RX_EN_FSE_PLL)
		 + 4 * FXI(rx, V27RX_EN_FSE_LMS);
	v[n++] = FXU(h, V27RXH_EQ_TAPS);
	v[n++] = (FXP(h, V27RXH_EQ_OUT_I) == fse->out_i)
		 + 2 * (FXP(h, V27RXH_EQ_OUT_Q) == fse->out_q)
		 + 4 * (FXP(h, V27RXH_EQ_N_OUT) == (void *)&fse->n_out)
		 + 8 * (FXP(h, V27RXH_EQ_ICOEFF) == fse->icoeff)
		 + 16 * (FXP(h, V27RXH_EQ_QCOEFF) == fse->qcoeff);
	v[n++] = FXI(h, V27RXH_ZERO_38) | FXI(h, V27RXH_ZERO_3C)
		 | FXI(h, V27RXH_ZERO_44) | FXI(h, V27RXH_ZERO_48)
		 | FXS(h, V27RXH_ZERO_40) | FXS(h, V27RXH_ZERO_4C);

	crt_fields = n;
	while (n < CRT_N)
		v[n++] = 0;
#undef T
}

/* The same 68 values, recomputed from the setup, with one reading changed. */
static void
crt_expect(long *v, const struct crt_setup *u, int variant)
{
	short want = u->use_default ? V27RX_CFG.bit_rate : u->bit_rate;
	int known = 1;
	int r;
	int train_long;
	long period;
	int n = 0;

	if (variant == CR_DEFAULT_IS_2400 && u->use_default)
		want = 2400;

	if (want == 2400)
		r = V27SH_RATE_2400;
	else if (want == 4800)
		r = V27SH_RATE_4800;
	else {
		r = V27SH_RATE_4800;
		known = 0;
	}
	if (variant == CR_RATE_SWAP)
		r = 1 - r;

	train_long = (u->use_default ? V27RX_CFG.short_train : u->short_train) == 0;

	v[n++] = r;
	v[n++] = train_long;
	v[n++] = V27RX_STATE_START;
	v[n++] = 0;
	v[n++] = 0;
	v[n++] = 0;
	v[n++] = 0;
	v[n++] = V27RX_STATE_START;

	v[n++] = (r == V27SH_RATE_4800 || variant == CR_AGC_BLOCK_ALWAYS)
	       ? V27_AGC_BLOCK_4800 : AGCv27_CFG.block_len;
	v[n++] = AGCv27_CFG.ref_level;
	v[n++] = AGCv27_CFG.acquire_level;

	v[n++] = V27RX_MRF_UP[r];
	v[n++] = V27RX_MRF_DOWN[r];
	v[n++] = V27RX_MRF_FILT_LEN[r];
	v[n++] = r;
	v[n++] = 1;

	v[n++] = V27RX_SAMP_PER_BAUD[r];
	v[n++] = V27_SRE_GROUPS_ACQ;
	v[n++] = V27_SRE_GROUPS_TRK;
	v[n++] = V27_SRE_SETTLE;
	v[n++] = V27RX_SRE_FILT_LEN[r];
	v[n++] = V27_SRE_MAG_HI;
	v[n++] = V27_SRE_MAG_LO;
	v[n++] = V27_SRE_ERR_HI;
	v[n++] = V27_SRE_ERR_LO;
	v[n++] = (short)(AGCv27_CFG.ref_level
			 / (variant == CR_RMS_DIV_3 ? 3
						    : V27_SRE_RMS_MIN_DIV));
	v[n++] = (short)((variant == CR_RMS_LEN_2 ? 2 : V27_SRE_RMS_LEN_SYMS)
			 * V27RX_SAMP_PER_BAUD[r]);
	v[n++] = r;
	v[n++] = r;
	v[n++] = r;
	v[n++] = r;
	v[n++] = r;
	v[n++] = r;
	v[n++] = 1;
	{
		int step = r == V27SH_RATE_2400 ? V27_SRE_PPM_STEP_2400
						: V27_SRE_PPM_STEP_4800;
		int unit = variant == CR_PPM_UNIT_100 ? 100 : V27_SRE_PPM_UNIT;

		if (variant == CR_PPM_STEP_SWAP)
			step = r == V27SH_RATE_2400 ? V27_SRE_PPM_STEP_4800
						    : V27_SRE_PPM_STEP_2400;
		period = (short)(step * unit);
		v[n++] = step;
		v[n++] = period;
		v[n++] = (short)(V27_SRE_PPM_MILLION
				 / (period * V27RX_SAMP_PER_BAUD[r]));
		v[n++] = (short)(V27_SRE_PPM_MILLION / period);
	}

	if (variant == CR_FSE_BLOCK_SWAP)
		v[n++] = r == V27SH_RATE_2400 ? V27_FSE_BLOCK_4800
					      : V27_FSE_BLOCK_2400;
	else
		v[n++] = r == V27SH_RATE_2400 ? V27_FSE_BLOCK_2400
					      : V27_FSE_BLOCK_4800;
	v[n++] = V27RX_SAMP_PER_BAUD[r];
	v[n++] = V27RX_FSE_FILT_LEN[r];
	v[n++] = V27RX_FSE_MU_TRAIN[r];
	v[n++] = V27RX_FSE_MU_TRACK[r];
	v[n++] = V27RX_CRR_TABLE_LEN[r];
	v[n++] = V27RX_CRR_ADJUST[r];
	v[n++] = V27_FSE_TRAIN_SYM;
	v[n++] = V27_FSE_ERR_HI;
	v[n++] = V27_FSE_ERR_LO;
	v[n++] = r;
	v[n++] = r;
	v[n++] = r;
	v[n++] = 1;
	v[n++] = 1;
	v[n++] = 1;

	if (variant == CR_QLIMIT_SWAP)
		v[n++] = r == V27SH_RATE_2400 ? V27RX_Q_LIMIT_4800
					      : V27RX_Q_LIMIT_2400;
	else
		v[n++] = r == V27SH_RATE_2400 ? V27RX_Q_LIMIT_2400
					      : V27RX_Q_LIMIT_4800;
	v[n++] = 0;
	v[n++] = 1;
	v[n++] = 0;
	if (variant == CR_NBITS_SWAP)
		v[n++] = r == V27SH_RATE_2400 ? V27_SDM_NBITS_4800
					      : V27_SDM_NBITS_2400;
	else
		v[n++] = r == V27SH_RATE_2400 ? V27_SDM_NBITS_2400
					      : V27_SDM_NBITS_4800;

	if (variant == CR_EIGHT_PHASE_INV)
		v[n++] = r != V27SH_RATE_4800;
	else
		v[n++] = r == V27SH_RATE_4800;
	v[n++] = (unsigned short)V27RX_DEC_PHS_MASK[r];
	if (variant == CR_TRAIN_IGNORED)
		v[n++] = 1;
	else
		v[n++] = !train_long;
	v[n++] = V27DEC_MAG;
	v[n++] = r;
	v[n++] = r;

	if (variant == CR_MTD_RATIO_SWAP) {
		v[n++] = V27_MTD_TONES * 65536L + V27_MTD_V21_RATIO;
		v[n++] = V27_MTD_MIN_LEVEL;
		v[n++] = r;
		v[n++] = V27_MTD_V21_TONES * 65536L + V27_MTD_RATIO;
	} else {
		v[n++] = V27_MTD_TONES * 65536L + V27_MTD_RATIO;
		v[n++] = V27_MTD_MIN_LEVEL;
		v[n++] = r;
		v[n++] = V27_MTD_V21_TONES * 65536L + V27_MTD_V21_RATIO;
	}
	v[n++] = V27_MTD_V21_MIN_LEVEL;
	v[n++] = 1;

	{
		long status = V27_STATUS_START;
		long flags = V27_STATUS_FLAGS_SEED;

		/*
		 * AND THE UNKNOWN-RATE REPORT IS DEAD, which is what this
		 * variant measures.  `V27RX_create` raises
		 * `V27_STATUS_FLAG_ERROR` and writes `V27_STATUS_DEFAULT` at
		 * 0x997b4 when the caller's `bit_rate` is neither 2400 nor
		 * 4800 -- and then, on every path, wipes all four bytes of the
		 * word with one `movl $0x0,0x1c(%ebp)` at 0x99d0d before
		 * seeding them again.  So the model below IGNORES `known`, and
		 * `CR_UNKNOWN_KEEPS_ERROR` -- the reading that believes the
		 * early write survives -- has to separate.  D1161.
		 */
		if (!known && variant == CR_UNKNOWN_KEEPS_ERROR) {
			status = V27_STATUS_DEFAULT;
			flags |= V27_STATUS_FLAG_ERROR;
		}
		v[n++] = status * 65536L + flags * 256L + 0;
	}
	v[n++] = 1 + 2 + 4;
	v[n++] = (unsigned short)V27RX_FSE_FILT_LEN[r];
	v[n++] = 1 + 2 + 4 + 8 + 16;
	v[n++] = 0;

	while (n < CRT_N)
		v[n++] = 0;
}

static int
crt_skip_obj(int off)
{
	if (off >= V27RXH_EQ_OUT_I && off < V27RXH_EQ_QCOEFF + 4)
		return 1;
	if (off >= V27_OBJ_SHARED && off < V27_OBJ_RX + 4)
		return 1;
	return 0;
}

static int
crt_skip_sh(int off)
{
	if (off >= V27SH_MTD && off < V27SH_MTD + 4)
		return 1;
	if (off >= V27SH_STATE && off < V27SH_STATE + 4)
		return 1;
	if (off >= V27SH_MTD_V21 && off < V27SH_BUF + 4)
		return 1;
	/* fpm_agc_cfg::alpha and ::beta, into each side's AGCv27_CFG. */
	if (off >= V27SH_AGC + 0x0c && off < V27SH_AGC + 0x14)
		return 1;
	return 0;
}

/*
 * EVERY TABLE POINTER, because the two sides hold two copies of every table.
 * What is deliberately NOT skipped is the three context pointers -- both sides
 * are handed the same `crt_aux_object` -- and every scalar.
 */
static int
crt_skip_rx(int off)
{
	if (off >= V27RX_MRF + 0x04 && off < V27RX_MRF + 0x08)
		return 1;			/* mrf.cfg.coeff             */
	if (off >= V27RX_AGC + 0x0c && off < V27RX_AGC + 0x14)
		return 1;			/* agc.cfg.alpha, .beta      */
	if (off >= V27RX_SRE + 0x10 && off < V27RX_SRE + 0x28)
		return 1;			/* sre.cfg's six tables      */
	if (off >= V27RX_FSE + 0x04 && off < V27RX_FSE + 0x0c)
		return 1;			/* fse.cfg.icoff, .qcoff     */
	if (off >= V27RX_FSE + 0x14 && off < V27RX_FSE + 0x18)
		return 1;			/* fse.cfg.clk               */
	if (off >= V27RX_FSE + 0x24 && off < V27RX_FSE + 0x34)
		return 1;			/* pll_k1, pll_k2, owner,
						 * decision                  */
	if (off >= V27RX_DEC + V27DEC_PMAP && off < V27RX_DEC + V27DEC_PMAP + 4)
		return 1;
	if (off >= V27RX_DEC + V27DEC_ANGLES
	    && off < V27RX_DEC + V27DEC_ANGLES + 4)
		return 1;
	return dem_skip_rx(off);
}

static int
crt_skip_mtd(int off)
{
	if (off < 4)
		return 1;			/* cfg.coeff                 */
	return off >= 0x0c && off < 0x10;	/* fpm_mtd::acc              */
}

static void
crt_one(const struct crt_setup *u, long tag)
{
	struct v27rx_cfg c;
	unsigned char *pa = 0, *pb = 0;
	void *ha, *hb;
	void *sa, *sb;
	void *ra, *rb;
	struct fpm_fse *fa, *fb;
	struct fpm_sre *qa, *qb;
	struct fpm_mtd *ma, *mb;
	long va[CRT_N], vb[CRT_N], ve[CRT_N];
	int d, i;

	c = V27RX_CFG;
	c.bit_rate = u->bit_rate;
	c.short_train = u->short_train;
	c.ptr_0018 = &crt_aux_object;
	crt_aux_want = u->use_default ? V27RX_CFG.ptr_0018 : &crt_aux_object;

	if (u->preallocate) {
		/*
		 * A HANDLE IT MUST REUSE, so the two `fresh` flags go to zero
		 * and the four modules take their re-init paths.  Both block
		 * pointers are cleared, which is what `V27RX_create` itself
		 * does for a handle it allocated -- the reuse path this
		 * reaches is the one where the HANDLE is old and the blocks
		 * are new, which is the only combination a caller can set up
		 * without knowing the layout.
		 */
		pa = (unsigned char *)sysdep_malloc(V27RXH_SIZE);
		pb = (unsigned char *)sysdep_malloc(V27RXH_SIZE);
		memset(pa, 0x5a, V27RXH_SIZE);
		memset(pb, 0x5a, V27RXH_SIZE);
		*(void **)(void *)(pa + V27_OBJ_SHARED) = 0;
		*(void **)(void *)(pa + V27_OBJ_RX) = 0;
		*(void **)(void *)(pb + V27_OBJ_SHARED) = 0;
		*(void **)(void *)(pb + V27_OBJ_RX) = 0;
	}

	ha = ref_V27RX_create(pa, u->use_default ? 0 : &c);
	hb = V27RX_create(pb, u->use_default ? 0 : &c);

	if (u->preallocate)
		diff_eq_int("at %ld: create returned the handle it was given",
			    ha == (void *)pa && hb == (void *)pb, 1, tag);
	else
		diff_eq_int("at %ld: create allocated a handle",
			    ha != 0 && hb != 0 && ha != hb, 1, tag);

	sa = FXP(ha, V27_OBJ_SHARED);
	sb = FXP(hb, V27_OBJ_SHARED);
	ra = FXP(ha, V27_OBJ_RX);
	rb = FXP(hb, V27_OBJ_RX);
	fa = (struct fpm_fse *)(void *)FX(ra, V27RX_FSE);
	fb = (struct fpm_fse *)(void *)FX(rb, V27RX_FSE);
	qa = (struct fpm_sre *)(void *)FX(ra, V27RX_SRE);
	qb = (struct fpm_sre *)(void *)FX(rb, V27RX_SRE);

	diff_eq_int("at %ld: create, first differing handle byte",
		    dem_first_diff((const unsigned char *)hb,
				   (const unsigned char *)ha,
				   V27RXH_SIZE, crt_skip_obj), -1, tag);
	diff_eq_int("at %ld: create, first differing shared byte",
		    dem_first_diff((const unsigned char *)sb,
				   (const unsigned char *)sa,
				   V27SH_SIZE, crt_skip_sh), -1, tag);
	diff_eq_int("at %ld: create, first differing receive byte",
		    dem_first_diff((const unsigned char *)rb,
				   (const unsigned char *)ra,
				   V27RX_BLOCK_SIZE, crt_skip_rx), -1, tag);

	/* The two tone detectors, and the accumulators they own. */
	ma = (struct fpm_mtd *)FXP(sa, V27SH_MTD);
	mb = (struct fpm_mtd *)FXP(sb, V27SH_MTD);
	diff_eq_int("at %ld: create, the data detector",
		    dem_first_diff((const unsigned char *)mb,
				   (const unsigned char *)ma,
				   (int)sizeof *ma, crt_skip_mtd), -1, tag);
	diff_eq_int("at %ld: create, the data detector's accumulators",
		    dem_first_diff((const unsigned char *)mb->acc,
				   (const unsigned char *)ma->acc,
				   ma->cfg.tones * 2 * (int)sizeof(short), 0),
		    -1, tag);
	ma = (struct fpm_mtd *)FXP(sa, V27SH_MTD_V21);
	mb = (struct fpm_mtd *)FXP(sb, V27SH_MTD_V21);
	diff_eq_int("at %ld: create, the V.21 detector",
		    dem_first_diff((const unsigned char *)mb,
				   (const unsigned char *)ma,
				   (int)sizeof *ma, crt_skip_mtd), -1, tag);
	diff_eq_int("at %ld: create, the V.21 detector's accumulators",
		    dem_first_diff((const unsigned char *)mb->acc,
				   (const unsigned char *)ma->acc,
				   ma->cfg.tones * 2 * (int)sizeof(short), 0),
		    -1, tag);

	/* The buffers the four modules own, where init filled them. */
	diff_eq_int("at %ld: create, the equaliser's I coefficients",
		    dem_first_diff((const unsigned char *)fb->icoeff,
				   (const unsigned char *)fa->icoeff,
				   fa->cfg.taps * (int)sizeof(short), 0), -1,
		    tag);
	diff_eq_int("at %ld: create, the equaliser's Q coefficients",
		    dem_first_diff((const unsigned char *)fb->qcoeff,
				   (const unsigned char *)fa->qcoeff,
				   fa->cfg.taps * (int)sizeof(short), 0), -1,
		    tag);
	diff_eq_int("at %ld: create, the symbol recovery's coefficients",
		    dem_first_diff((const unsigned char *)qb->coeff,
				   (const unsigned char *)qa->coeff,
				   qa->cfg.coeffs * (int)sizeof(short), 0), -1,
		    tag);

	/*
	 * The two scratch buffers.  ONLY THE FIRST 160 ENTRIES ARE ZEROED and
	 * `V27RX_BUF_B` is 164 long, so its last four bytes are whatever
	 * `sysdep_malloc` returned and are deliberately not compared.
	 */
	for (i = 0; i < V27RX_BUF_ZERO; i++) {
		if (((short *)FXP(ra, V27RX_BUF_A))[i] != 0
		    || ((short *)FXP(rb, V27RX_BUF_A))[i] != 0
		    || ((short *)FXP(ra, V27RX_BUF_B))[i] != 0
		    || ((short *)FXP(rb, V27RX_BUF_B))[i] != 0)
			break;
	}
	diff_eq_int("at %ld: create, both scratch buffers are cleared", i,
		    V27RX_BUF_ZERO, tag);

	crt_read(ha, va, 1);
	crt_read(hb, vb, 0);
	for (i = 0; i < CRT_N; i++)
		if (va[i] != vb[i]) {
			diff_eq_int("create, trial*1000+field %ld disagrees",
				    vb[i], va[i], tag * 1000 + i);
			break;
		}
	if (i == CRT_N)
		diff_eq_int("at %ld: create, all fields agree", 1, 1, tag);

	for (d = 0; d < (int)CR_MAX; d++) {
		crt_expect(ve, u, d);
		for (i = 0; i < CRT_N; i++)
			if (ve[i] != va[i])
				break;
		if (d == CR_NONE)
			diff_eq_int("V27RX_create model, first differing"
				    " trial*1000+field %ld",
				    i, CRT_N, (long)(tag * 1000 + i));
		else if (i != CRT_N)
			crt_sep[d]++;
	}

	{
		short want = u->use_default ? V27RX_CFG.bit_rate : u->bit_rate;

		crt_rate_seen[want == 2400 ? 0 : (want == 4800 ? 1 : 2)]++;
	}
	crt_trials++;
	crt_train_seen[va[1] != 0]++;
	if (u->use_default)
		crt_default_seen++;
	if (u->preallocate)
		crt_prealloc_seen++;

	ref_V27RX_delete(ha);
	V27RX_delete(hb);
}

static int
run_create(void)
{
	static const struct crt_setup setups[] = {
	  /* dflt rate  short_train prealloc */
	  {  0,  2400,  0,       0 },
	  {  0,  2400,  1,       0 },
	  {  0,  4800,  0,       0 },
	  {  0,  4800,  1,       0 },
	  {  0,  9600,  0,       0 },	/* unknown: the error arm      */
	  {  0,     0,  1,       0 },	/* also unknown                */
	  {  1,  2400,  0,       0 },	/* null cfg: V27RX_CFG's 4800  */
	  {  0,  2400,  0,       1 },
	  {  0,  4800,  1,       1 },
	  {  1,     0,  0,       1 },
	  {  0,  4800,  0,       1 },
	  {  0,  2400,  1,       1 }
	};
	int s;
	long tag = 0;

	crt_tables_init();

	diff_begin("V27RX_create");

	for (s = 0; s < (int)(sizeof(setups) / sizeof(setups[0])); s++)
		crt_one(&setups[s], tag++);

	return diff_end();
}

/* --------------------------------------------------------------------- */

static int
sep_report(void)
{
	int i;

	diff_begin("v27fax separating trials");

	diff_eq_int("GetSNRV27 ignores the instance (%ld)", snr_indep > 0, 1,
		    snr_indep);

	diff_eq_int("V27RX_status: arg1 does not decide (%ld)",
		    rxst_arg_sep > 0, 1, rxst_arg_sep);
	diff_eq_int("V27RX_status: the answer is 0 or 1 (%ld)",
		    rxst_bool_sep > 0, 1, rxst_bool_sep);

	diff_eq_int("EpochDetectV27: the offset separates (%ld)",
		    epoch_off_sep > 0, 1, epoch_off_sep);
	diff_eq_int("EpochDetectV27: the reduction separates (%ld)",
		    epoch_bool_sep > 0, 1, epoch_bool_sep);

	diff_eq_int("CarrierDetectV27: AND not OR (%ld)", carr_or_sep > 0, 1,
		    carr_or_sep);
	diff_eq_int("CarrierDetectV27: not reduced (%ld)", carr_bool_sep > 0,
		    1, carr_bool_sep);
	diff_eq_int("CarrierDetectV27: the offsets separate (%ld)",
		    carr_off_sep > 0, 1, carr_off_sep);

	for (i = 1; i < TXST_VARIANTS; i++) {
		if (i == 5)
			continue;
		diff_eq_int("V27TX_status variant separates (%ld)",
			    txst_sep[i] > 0, 1, (long)(i * 1000
						       + txst_sep[i]));
	}
	/*
	 * AND ONE THAT MUST NOT SEPARATE, asserted rather than skipped.
	 *
	 * The first store to the destination's +0x14 is dead over EVERY input
	 * this test can construct, aliasing included, and the reason is
	 * arithmetic rather than luck: the only later read that can see that
	 * byte is the source's +0x10 at a displacement of +4, only bit 2 of
	 * that read is used, and neither `| 0x01` nor `& ~0x02` touches bit 2.
	 * The int copied out of +0x18 can overlap it at a displacement of -4,
	 * but the SECOND store to +0x14 precedes that read and overwrites the
	 * first.  So a reconstruction that dropped the store would pass every
	 * differential test there is -- which is exactly why the fact is
	 * written down here instead of being left to a reader to rediscover.
	 * Finding F8867.
	 */
	diff_eq_int("V27TX_status: the first +0x14 store is dead (%ld)",
		    txst_sep[5], 0, txst_alias_trials);
	diff_eq_int("V27TX_status overlapped the blocks (%ld)",
		    txst_alias_trials > 0, 1, txst_alias_trials);
	diff_eq_int("V27TX_status pinned the flags byte (%ld)",
		    txst_ff_trials > 0, 1, txst_ff_trials);

	for (i = 1; i < DEC_VARIANTS; i++)
		diff_eq_int("V27RX_decision variant separates (%ld)",
			    dec_sep[i] > 0, 1, (long)(i * 1000 + dec_sep[i]));
	diff_eq_int("V27RX_decision reached the counter restart (%ld)",
		    dec_wrap_trials > 0, 1, dec_wrap_trials);
	diff_eq_int("V27RX_decision reached a 16-bit wrap (%ld)",
		    dec_trunc_trials > 0, 1, dec_trunc_trials);
	diff_eq_int("V27RX_decision reached diff > 0x8000 (%ld)",
		    dec_wide_trials > 0, 1, dec_wide_trials);
	diff_eq_int("V27RX_decision ran the object's own table (%ld)",
		    dec_tie_trials > 0, 1, dec_tie_trials);

	for (i = 1; i < QD_VARIANTS; i++)
		diff_eq_int("QualityDetectV27 variant separates (%ld)",
			    qd_sep[i] > 0, 1, (long)(i * 1000 + qd_sep[i]));
	diff_eq_int("QualityDetectV27 latched (%ld)", qd_latch_trials > 0, 1,
		    qd_latch_trials);
	diff_eq_int("QualityDetectV27 smoothed (%ld)", qd_smooth_trials > 0,
		    1, qd_smooth_trials);
	diff_eq_int("QualityDetectV27 went quiet (%ld)", qd_quiet_trials > 0,
		    1, qd_quiet_trials);
	diff_eq_int("QualityDetectV27 met acc == limit (%ld)",
		    qd_equal_trials > 0, 1, qd_equal_trials);

	diff_eq_int("V27RX_modem: in and out differ (%ld)", modm_swap_sep > 0,
		    1, modm_swap_sep);
	diff_eq_int("V27RX_modem: the total truncates (%ld)",
		    modm_trunc_sep > 0, 1, modm_trunc_sep);
	diff_eq_int("V27RX_modem: the count is written back (%ld)",
		    modm_wb_sep > 0, 1, modm_wb_sep);
	diff_eq_int("V27RX_modem: the flag bit was set going in (%ld)",
		    modm_flag_sep > 0, 1, modm_flag_sep);
	diff_eq_int("V27RX_modem: it is a do-while (%ld)", modm_while_sep > 0,
		    1, modm_while_sep);
	diff_eq_int("V27RX_modem: the status offset separates (%ld)",
		    modm_ret_sep > 0, 1, modm_ret_sep);
	diff_eq_int("V27RX_modem: the signed count corner (%ld)",
		    modm_signed_sep > 0, 1, modm_signed_sep);
	diff_eq_int("V27RX_modem: the handler ran repeatedly (%ld)",
		    modm_multi_trials > 0, 1, modm_multi_trials);


	/* ---- the receive state machine, section 18 ---------------------- */

	diff_eq_int("state machine: blocks driven (%ld)", smh_blocks > 0, 1,
		    smh_blocks);
	/*
	 * ALL SIX STATES ARE ENTERED, and NOT all of them by the machine:
	 * `V27RX_STATE_IDLE` is unreachable from START because the only arm
	 * that installs `RxHdxIdleV27` is `RxNextStateV27`'s DATA arm and
	 * `RxHdxDataV27` never calls `RxNextStateV27`.  So the machine can
	 * enter START, EPOCH_DET, PROTOCOL, DATA and ERROR and stops there,
	 * and IDLE is counted from the direct-call driver.  This counter is
	 * the union of the two, which is why it is one assertion and not two.
	 */
	for (i = 0; i <= V27RX_STATE_ERROR; i++)
		diff_eq_int("state machine: every state was entered (%ld)",
			    smh_state_seen[i + 1] > 0, 1,
			    (long)(i * 100000 + smh_state_seen[i + 1]));
	diff_eq_int("state machine: the tone abort preserved the equaliser"
		    " (%ld)", smh_tone_kept > 0, 1, smh_tone_kept);
	for (i = 0; i < 6; i++)
		diff_eq_int("RxNextStateV27: every arm was taken (%ld)",
			    smh_arm[i] > 0, 1,
			    (long)(i * 100000 + smh_arm[i]));
	diff_eq_int("RxNextStateV27: the AGC coefficients stepped (%ld)",
		    smh_agc_stepped > 0, 1, smh_agc_stepped);

	diff_eq_int("state machine: the carrier arm ran (%ld)",
		    smh_carrier_up > 0, 1, smh_carrier_up);
	diff_eq_int("state machine: the no-carrier arm ran (%ld)",
		    smh_carrier_lost > 0, 1, smh_carrier_lost);
	diff_eq_int("state machine: the error arm was installed (%ld)",
		    smh_error_arm > 0, 1, smh_error_arm);
	diff_eq_int("RxHdxStartV27: both arms ran (%ld)",
		    smh_start_hold > 0 && smh_start_advance > 0, 1,
		    smh_start_hold * 100000 + smh_start_advance);
	diff_eq_int("RxHdxEpochDetV27: it held (%ld)", smh_epoch_hold > 0, 1,
		    smh_epoch_hold);
	diff_eq_int("RxHdxEpochDetV27: the countdown advanced it (%ld)",
		    smh_epoch_by_count > 0, 1, smh_epoch_by_count);
	diff_eq_int("RxHdxEpochDetV27: the detector advanced it (%ld)",
		    smh_epoch_by_det > 0, 1, smh_epoch_by_det);
	diff_eq_int("RxHdxPrtcolV27: both arms ran (%ld)",
		    smh_prtcol_hold > 0 && smh_prtcol_handover > 0, 1,
		    smh_prtcol_hold * 100000 + smh_prtcol_handover);
	diff_eq_int("RxHdxIdleV27: the restart test met its own threshold"
		    " exactly (%ld)", smh_idle_edge > 0, 1, smh_idle_edge);
	diff_eq_int("RxHdxIdleV27: both arms ran (%ld)",
		    smh_idle_hold > 0 && smh_idle_restart > 0, 1,
		    smh_idle_hold * 100000 + smh_idle_restart);

	for (i = 1; i < (int)SM_MAX; i++) {
		/*
		 * AND ONE THAT MUST NOT SEPARATE, asserted rather than left
		 * out.  The switch reads the state `movswl` and the range
		 * check is `cmp $0x4` / `ja`, which is UNSIGNED -- so a
		 * negative state and its 16-bit unsigned reading BOTH exceed
		 * 4 and both take the default arm.  The extension is free at
		 * this site (F614) and this asserts the zero rather than
		 * pretending the variant is a detector.
		 */
		if (i == (int)SM_STATE_UNSIGNED) {
			diff_eq_int("RxNextStateV27: the state extension"
				    " separates nothing (%ld)",
				    smh_sep[i], 0, smh_sep[i]);
			continue;
		}
		diff_eq_int("RxNextStateV27 variant separates (%ld)",
			    smh_sep[i] > 0, 1, (long)(i * 100000 + smh_sep[i]));
	}
	for (i = 1; i < (int)SH_MAX; i++)
		diff_eq_int("receive handler variant separates (%ld)",
			    smh_hsep[i] > 0, 1,
			    (long)(i * 100000 + smh_hsep[i]));


	/* ---- V27RX_create, section 19 ----------------------------------- */

	diff_eq_int("V27RX_create: trials (%ld)", crt_trials > 0, 1,
		    crt_trials);
	diff_eq_int("V27RX_create: fields compared per trial (%ld)",
		    crt_fields > 60, 1, crt_fields);
	for (i = 0; i < 3; i++)
		diff_eq_int("V27RX_create: 2400, 4800 and an unknown rate all"
			    " built (%ld)", crt_rate_seen[i] > 0, 1,
			    (long)(i * 100000 + crt_rate_seen[i]));
	for (i = 0; i < 2; i++)
		diff_eq_int("V27RX_create: both training lengths built (%ld)",
			    crt_train_seen[i] > 0, 1,
			    (long)(i * 100000 + crt_train_seen[i]));
	diff_eq_int("V27RX_create: a null config was used (%ld)",
		    crt_default_seen > 0, 1, crt_default_seen);
	diff_eq_int("V27RX_create: a caller-supplied handle was reused (%ld)",
		    crt_prealloc_seen > 0, 1, crt_prealloc_seen);
	for (i = 1; i < (int)CR_MAX; i++) {
		/*
		 * AND ONE THAT MUST NOT SEPARATE, asserted rather than left
		 * out.  `V27RX_create` writes `V27_AGC_BLOCK_4800` into the
		 * live gain control's `cfg.block_len` on the 4800 arm only --
		 * and `AGCv27_CFG.block_len` is 40, which IS 0x28.  So the
		 * store puts back the value the configuration already had and
		 * nothing can observe whether it happened.  The variant that
		 * does it on both arms is therefore not a detector, and this
		 * asserts the zero instead of pretending it is one.  F9307,
		 * D1162.
		 */
		if (i == (int)CR_AGC_BLOCK_ALWAYS) {
			diff_eq_int("V27RX_create: the 4800 AGC block length"
				    " separates nothing (%ld)", crt_sep[i], 0,
				    crt_sep[i]);
			continue;
		}
		diff_eq_int("V27RX_create variant separates (%ld)",
			    crt_sep[i] > 0, 1, (long)(i * 100000 + crt_sep[i]));
	}

	for (i = 1; i <= 5; i++)
		diff_eq_int("V27RX_delete variant separates (%ld)",
			    del_sep[i] > 0, 1, (long)(i * 1000 + del_sep[i]));
	diff_eq_int("V27RX_delete ran (%ld)", del_trials > 0, 1, del_trials);

	for (i = 1; i < DCD_VARIANTS; i++)
		diff_eq_int("DataCarrierDetectV27 variant separates (%ld)",
			    dcd_sep[i] > 0, 1,
			    (long)(i * 100000 + dcd_sep[i]));
	diff_eq_int("DataCarrierDetectV27 ran the V.21 arm (%ld)",
		    dcd_watch_trials > 0, 1, dcd_watch_trials);
	diff_eq_int("DataCarrierDetectV27 ran the V.21 detector (%ld)",
		    dcd_armed_trials > 0, 1, dcd_armed_trials);
	diff_eq_int("DataCarrierDetectV27 saw the detector fire (%ld)",
		    dcd_hit_trials > 0, 1, dcd_hit_trials);
	diff_eq_int("DataCarrierDetectV27 reached the V.21 timeout (%ld)",
		    dcd_timeout_trials > 0, 1, dcd_timeout_trials);
	diff_eq_int("DataCarrierDetectV27 passed the settled gate (%ld)",
		    dcd_settled_trials > 0, 1, dcd_settled_trials);
	diff_eq_int("DataCarrierDetectV27 saw a bad mse (%ld)",
		    dcd_mse_trials > 0, 1, dcd_mse_trials);
	diff_eq_int("DataCarrierDetectV27 saw the energy drop (%ld)",
		    dcd_drop_trials > 0, 1, dcd_drop_trials);
	diff_eq_int("DataCarrierDetectV27 republished the level (%ld)",
		    dcd_republish_trials > 0, 1, dcd_republish_trials);

	for (i = 1; i < (int)P_MAX; i++) {
		/*
		 * P_HIST_SIGNED IS AN EQUIVALENT MUTANT AND THAT IS MEASURED,
		 * not assumed.  Every one of the six history slots is read and
		 * immediately DIFFERENCED, and the difference is narrowed back
		 * to `short` -- so the two readings are 65536 apart in a value
		 * nothing above bit 15 survives.  Finding F614's free case,
		 * and finding F9234 records that the object's `movzwl` is
		 * therefore a codegen fact and not a behavioural one.  The
		 * count is asserted to be ZERO rather than skipped, so a
		 * fixture that ever did separate it would show up here.
		 */
		if (i == (int)P_HIST_SIGNED) {
			diff_eq_int("V27RX_epoch_det: the history's extension"
				    " is free (%ld, expected 0)", ep_sep[i], 0,
				    ep_sep[i]);
			continue;
		}
		diff_eq_int("V27RX_epoch_det wrong reading %ld separates",
			    ep_sep[i] > 0, 1, i);
	}
	diff_eq_int("V27RX_epoch_det handed over (%ld)", ep_handover > 0, 1,
		    ep_handover);
	diff_eq_int("V27RX_epoch_det moved the average (%ld)", ep_judged > 0,
		    1, ep_judged);
	diff_eq_int("V27RX_epoch_det used a NEGATIVE subscript (%ld)",
		    ep_neg_index > 0, 1, ep_neg_index);
	diff_eq_int("V27RX_epoch_det used a non-negative subscript (%ld)",
		    ep_pos_index > 0, 1, ep_pos_index);
	(void)ep_seeded;
	(void)ep_wrapped;

	for (i = 1; i < (int)H_MAX; i++)
		diff_eq_int("RxHdxDataV27 wrong reading %ld separates",
			    hdx_sep[i] > 0, 1, i);
	for (i = 1; i < (int)HE_MAX; i++)
		diff_eq_int("RxHdxErrorV27 wrong reading %ld separates",
			    herr_sep[i] > 0, 1, i);
	diff_eq_int("RxHdxDataV27 took the demodulating arm (%ld)",
		    hdx_accepted > 0, 1, hdx_accepted);
	diff_eq_int("RxHdxDataV27 took the deny arm (%ld)", hdx_denied > 0, 1,
		    hdx_denied);
	diff_eq_int("RxHdxDataV27 returned a graded count (%ld)",
		    hdx_graded > 0, 1, hdx_graded);
	diff_eq_int("RxHdxDataV27 returned zero (%ld)", hdx_unreliable > 0, 1,
		    hdx_unreliable);
	diff_eq_int("RxHdxErrorV27 drove blocks (%ld)", herr_blocks > 0, 1,
		    herr_blocks);
	/*
	 * ASSERTED TO BE ZERO, WHICH IS THE POINT.  `GetSNRV27` is a constant
	 * 10, so the low-SNR arm is unreachable and the `<=`/`<` distinction
	 * is unmeasurable -- F9233.  Reporting the count keeps the claim
	 * honest: if a later fixture ever reaches it, this line fails and
	 * whoever wrote that fixture gets to add the variant.
	 */
	diff_eq_int("RxHdxDataV27 raised LOW_SNR (%ld, expected 0)",
		    hdx_snr_low, 0, hdx_snr_low);
	diff_eq_int("the `<` SNR variant is unmeasurable (%ld, expected 0)",
		    hdx_snr_strict_sep, 0, hdx_snr_strict_sep);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_snr();
	rc |= run_rxstatus();
	rc |= run_flags();
	rc |= run_txstatus();
	rc |= run_decision();
	rc |= run_quality();
	rc |= run_modem();
	rc |= run_delete();
	rc |= run_dcd();
	rc |= run_eq_train();
	rc |= run_scramble();
	rc |= run_demod();
	rc |= run_txdelete();
	rc |= run_moddata();
	rc |= run_epoch();
	rc |= run_hdx();
	rc |= run_smh();
	rc |= run_create();
	rc |= sep_report();

	return rc;
}
