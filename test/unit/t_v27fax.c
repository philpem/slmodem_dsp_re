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

	return diff_end();
}

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
	unsigned short	skip_tone;	/* non-zero skips the tone test    */
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
	*(unsigned short *)(void *)(f->sh + V27SH_SKIP_TONE) = u->skip_tone;

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

	if (*(unsigned short *)(void *)(sh + V27SH_SKIP_TONE) == 0
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

		if (u->skip_tone != 0)
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

		u.skip_tone = (unsigned short)(gate ? 0x1234 : 0);
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

		u.skip_tone = 0x1234;
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
	rc |= sep_report();

	return rc;
}
