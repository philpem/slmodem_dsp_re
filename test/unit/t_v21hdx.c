/*
 * t_v21hdx.c -- differential test of V.21's receive data path.
 *
 * Five symbols, one indivisible unit: `DemodDataV21` and the four half-duplex
 * receive states that call it.  See finding F8898 for why they had to be
 * written together, and findings F8894, F8895, F8896 and F8897 for what
 * they established.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS TEST HAS TO DO THAT THE USUAL ONE DOES NOT
 *
 * `DemodDataV21` drives four stateful DSP blocks -- an AGC, a multi-tone
 * detector, a polyphase resampler and an FSK demodulator -- so nothing here
 * is a one-block check.  Every function is run over a STREAM of blocks with
 * both objects compared in full after every one, which is finding F8790: a
 * state update that is wrong only in how it CARRIES is invisible to any
 * single-block fixture and to every codegen check.
 *
 * D955 / F8587: a fixture must plant every field a callee uses as a
 * SUBSCRIPT, not only the ones it dereferences.  A blob-against-blob dry run
 * cannot catch an unplanted subscript, because both sides read the same wild
 * index into the same array and agree.  So the mrf, the fsd, the agc and the
 * mtd are all brought up through their own `init`/`create` functions with a
 * real configuration, and the two per-fixture scratch buffers are marked
 * either side of every bound.
 *
 * TWO HAZARDS ARE PARTICULAR TO THESE FIVE, and both are about the fact that
 * a handler's ADDRESS is data here:
 *
 *   - `RxHdxWaitV21` stores `RxHdxErrorV21` into the handler slot, and the
 *     blob's copy stores `ref_RxHdxErrorV21`.  A byte comparison of the
 *     half-duplex block would report a difference on every trial that
 *     advances the state.  `handler_id()` maps both families onto small
 *     integers, which is STRONGER than skipping the field: it says the two
 *     sides chose the same handler.
 *   - `DemodDataV21`'s squelch arm compares the installed handler against
 *     `RxHdxDataV21` ITSELF, so the fixture plants `ref_RxHdxDataV21` on the
 *     blob's side and `RxHdxDataV21` on ours.  Planting one pointer on both
 *     sides would make the two runs disagree for a reason that is not a
 *     defect.
 *
 * ---------------------------------------------------------------------------
 * EVERY CHECK IS BUILT AROUND A NAMED WRONG READING, and the number of trials
 * that SEPARATE it is counted FROM THE RUN and asserted non-zero at the end
 * (finding F134).  A zero there means the check above it is decoration.  The
 * wrong readings, in order:
 *
 *   DemodDataV21
 *       - the AGC given `dsp + 0x08` or `dsp + 0x10` rather than `dsp + 0x0c`.
 *         Separated by construction: the block is brought up at +0x0c, so a
 *         shifted read finds a config that is not one.  Reproduced here as a
 *         PREDICTION about the two ints, which the blob's own answer settles.
 *       - `dsp->int_0004` taken from anything but the agc's `signal`.
 *       - `dsp->int_0008` left at 1 when the detector fires, or set from the
 *         verdict rather than from `verdict == FPM_MTD_ABSENT`.
 *       - the squelch loop run when the DATA handler IS installed, or not run
 *         when it is not.
 *       - the resampler's output taken as the demodulator's sample count
 *         rather than the block's own `count`.
 *
 *   RxHdxErrorV21
 *       - the error bit not raised; the count not zeroed.
 *
 *   RxHdxIdleV21
 *       - the carrier bit assigned from `CarrierDetectV21` rather than
 *         cleared-then-set, which differ only in that the object's spelling
 *         leaves the bit clear when the call answers zero.  Both readings are
 *         driven, and the check that separates them is the status byte order.
 *       - the status byte written before the demodulate rather than after.
 *
 *   RxHdxWaitV21
 *       - the countdown tested `> 0` after the decrement rather than before.
 *       - the carrier-gone arm returning the bit count rather than zero.
 *       - the state advance run on the carrier-gone arm.
 *
 *   RxHdxDataV21
 *       - the carrier bit assigned rather than raised-then-lowered.
 *       - `hdx->int_0000` not consulted.
 *       - the SNR bit raised on `< 5` rather than `<= 5`.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/b103fp.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21fax.h"

extern unsigned short ref_DemodDataV21(void *modem, short *in, short *bits,
				       unsigned short count);
extern short ref_RxHdxErrorV21(void *modem, short *in, short *out,
			       short *count);
extern short ref_RxHdxIdleV21(void *modem, short *in, short *out,
			      short *count);
extern short ref_RxHdxWaitV21(void *modem, short *in, short *out,
			      short *count);
extern short ref_RxHdxDataV21(void *modem, short *in, short *out,
			      short *count);

extern void ref_FPM_AGC_init(struct fpm_agc *agc, const struct fpm_agc_cfg *cfg,
			     int reset);
extern void ref_FPM_MRF_init(struct fpm_mrf *state,
			     const struct fpm_mrf_cfg *cfg, int fresh);
extern void ref_FPM_MRF_free(struct fpm_mrf *state);
extern void ref_FPM_FSD_init(struct fpm_fsd *state,
			     const struct fpm_fsd_cfg *cfg, int fresh);
extern void ref_FPM_FSD_free(struct fpm_fsd *state);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *state,
					  const struct fpm_mtd_cfg *cfg);

/* --------------------------------------------------------------------- */

#define RXOBJ_SIZE	0x60
#define MARK		((short)0x5ead)

#define MRF_TAPS	60
#define MRF_BRANCHES	3
#define MRF_DECIMATE	10

#define FSD_FIR_TAPS	15
#define FSD_IIR_LEN	3
#define FSD_TRACE_LEN	512
#define FSD_MAX_BITS	64

#define MTD_TONES	2

#define BLOCK		36		/* the agc's block_len              */
#define BLOCKS		11		/* how many go through each stream  */

#define IN_LEN		(BLOCK + 8)
#define MAG_LEN		512
#define BITS_LEN	256

/* The four input shapes; see the note above `fill_in` for how they were chosen. */
#define IN_SILENT	0
#define IN_TONE		1
#define IN_CARRIER	2
#define IN_MIXED	3

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

/* --------------------------------------------------------------------- */
/* Tables                                                                */

/*
 * The mrf and fsd banks are built here rather than taken from the object's
 * own, because finding F3574 is three mutations that survived on a real bank
 * that repeated two entries and zeroed a third.  Every entry below is
 * distinct.
 *
 * MTDb103_COEF is the one exception and is used as it stands: the detector's
 * coefficients are two BIQUAD sections, and an arbitrary pole pair makes the
 * resonator diverge rather than detect.  The detector is a callee here and
 * not the code under test, so a real bank is the right choice; what has to
 * vary across the stream is its VERDICT, and that is driven from the input.
 */
static short mrf_coeff[MRF_TAPS];
static short fsd_fir[FSD_FIR_TAPS];
static short fsd_iir[FSD_IIR_LEN * 5];

static void
build_tables(void)
{
	int i;

	for (i = 0; i < MRF_TAPS; i++)
		mrf_coeff[i] = (short)(((i * 811) % 6007) - 3000);
	for (i = 0; i < FSD_FIR_TAPS; i++)
		fsd_fir[i] = (short)(((i * 1279) % 4001) - 2000);

	/*
	 * Three biquads, five words each, in the layout FPM_iir_filt reads.
	 * Kept small so the sections stay stable over a long stream.
	 */
	for (i = 0; i < FSD_IIR_LEN; i++) {
		fsd_iir[i * 5 + 0] = (short)(1000 + i * 37);
		fsd_iir[i * 5 + 1] = (short)(1900 + i * 53);
		fsd_iir[i * 5 + 2] = (short)(900 + i * 41);
		fsd_iir[i * 5 + 3] = (short)(-12000 + i * 211);
		fsd_iir[i * 5 + 4] = (short)(4000 + i * 179);
	}
}

/* --------------------------------------------------------------------- */
/* The fixture                                                           */

struct hdxfix {
	unsigned char		obj[RXOBJ_SIZE];
	struct v21_rx_hdx	hdx;
	struct v21_rx_dsp	dsp;
	struct fpm_mtd		mtd;
	short			mtd_acc[MTD_TONES * 2];
	short			trace[FSD_TRACE_LEN];
	short			mag[MAG_LEN];
	double			align;
};

static struct hdxfix fa, fb;
static short in_a[IN_LEN], in_b[IN_LEN];
static short bits_a[BITS_LEN], bits_b[BITS_LEN];

/*
 * Logical handler names.  `plant` writes the pointer for one SIDE; `id` reads
 * whatever pointer is there back into the same small integer whichever side
 * it came from, so the two runs can be compared on the handler they CHOSE.
 */
#define H_NONE		0
#define H_ERROR		1
#define H_IDLE		2
#define H_WAIT		3
#define H_DATA		4
#define H_OTHER		5
#define H_UNKNOWN	(-1)

/* A pointer that is none of the four, for the "not the data handler" case. */
static short
other_handler(void *modem, short *in, short *out, short *count)
{
	(void)modem;
	(void)in;
	(void)out;
	(void)count;
	return 0;
}

static int
handler_id(short (*h)(void *, short *, short *, short *))
{
	if (h == 0)
		return H_NONE;
	if (h == RxHdxErrorV21 || h == ref_RxHdxErrorV21)
		return H_ERROR;
	if (h == RxHdxIdleV21 || h == ref_RxHdxIdleV21)
		return H_IDLE;
	if (h == RxHdxWaitV21 || h == ref_RxHdxWaitV21)
		return H_WAIT;
	if (h == RxHdxDataV21 || h == ref_RxHdxDataV21)
		return H_DATA;
	if (h == other_handler)
		return H_OTHER;
	return H_UNKNOWN;
}

static short
(*handler_for(int id, int blob))(void *, short *, short *, short *)
{
	switch (id) {
	case H_ERROR:	return blob ? ref_RxHdxErrorV21 : RxHdxErrorV21;
	case H_IDLE:	return blob ? ref_RxHdxIdleV21 : RxHdxIdleV21;
	case H_WAIT:	return blob ? ref_RxHdxWaitV21 : RxHdxWaitV21;
	case H_DATA:	return blob ? ref_RxHdxDataV21 : RxHdxDataV21;
	case H_OTHER:	return other_handler;
	default:	return 0;
	}
}

/*
 * Bring one receiver up.
 *
 * `blob` picks which family of handler pointers goes in the slot.  The whole
 * handle and the parts of the DSP block the inits do not own are
 * pseudorandom first, so an offset wrong by a few bytes reads garbage rather
 * than a plausible zero.
 */
static void
fixture(struct hdxfix *f, unsigned seed, int blob, int handler,
	short state, unsigned short countdown, int gate, int shape,
	unsigned char flags, unsigned char flags1, unsigned char status)
{
	struct fpm_mrf_cfg mrf;
	struct fpm_fsd_cfg fsd;
	struct fpm_mtd_cfg mtd;
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < RXOBJ_SIZE; i++)
		f->obj[i] = (unsigned char)rng_next();
	for (i = 0; i < (int)sizeof(f->dsp); i++)
		((unsigned char *)(void *)&f->dsp)[i] = (unsigned char)rng_next();

	*(void **)(void *)(f->obj + V21RX_OBJ_HDX) = (void *)&f->hdx;
	*(void **)(void *)(f->obj + V21RX_OBJ_DSP) = (void *)&f->dsp;
	f->obj[V21RX_OBJ_STATUS] = status;
	f->obj[V21RX_OBJ_FLAGS] = flags;
	f->obj[V21RX_OBJ_FLAGS1] = flags1;
	f->obj[V21RX_OBJ_FLAGS1 + 1] = 0x00;

	f->hdx.int_0000 = gate;
	f->hdx.handler = handler_for(handler, blob);
	f->hdx.state = state;
	f->hdx.countdown = countdown;

	/*
	 * The two ints `CarrierDetectV21` reads.  `DemodDataV21` overwrites
	 * both, so for the three handlers that demodulate FIRST the planted
	 * values only matter on the first block -- but `RxHdxDataV21` tests
	 * the carrier BEFORE it demodulates, so without a planted carrier it
	 * takes the advance arm on block 0 and can never reach the
	 * demodulating one again.  They are seeded from the shape the stream
	 * is about to be fed, which is "the block before this one left a
	 * carrier".
	 */
	f->dsp.int_0000 = 0;
	f->dsp.int_0004 = (shape == IN_CARRIER || shape == IN_MIXED);
	f->dsp.int_0008 = (shape == IN_CARRIER || shape == IN_MIXED);

	ref_FPM_AGC_init(&f->dsp.agc, &AGCb103_CFG_data, 1);

	memset(&mrf, 0, sizeof(mrf));
	mrf.branches = MRF_BRANCHES;
	mrf.decimate = MRF_DECIMATE;
	mrf.coeff = mrf_coeff;
	mrf.taps = MRF_TAPS;
	ref_FPM_MRF_init(&f->dsp.mrf, &mrf, 1);

	memset(&fsd, 0, sizeof(fsd));
	fsd.fir = fsd_fir;
	fsd.fir_taps = FSD_FIR_TAPS;
	fsd.delay = 4;
	fsd.iir = fsd_iir;
	fsd.iir_len = FSD_IIR_LEN;
	fsd.slice_level = 10;
	fsd.high_bit = 1;
	fsd.bit_samples = 8;
	fsd.max_bits = FSD_MAX_BITS;
	fsd.trace_len = FSD_TRACE_LEN;
	ref_FPM_FSD_init(&f->dsp.fsd, &fsd, 1);

	/*
	 * The fsd's own trace buffer is replaced by the fixture's, so both
	 * sides' traces sit at a known length and can be compared directly.
	 */
	sysdep_free(f->dsp.fsd.trace);
	f->dsp.fsd.trace = f->trace;

	memset(&mtd, 0, sizeof(mtd));
	mtd.coeff = MTDb103_COEF;
	mtd.tones = MTD_TONES;
	mtd.ratio = 24576;
	mtd.min_level = 246;
	f->mtd.acc = f->mtd_acc;
	ref_FPM_MTD_create(&f->mtd, &mtd);
	f->dsp.mtd = &f->mtd;

	f->dsp.mag = f->mag;
	for (i = 0; i < MAG_LEN; i++)
		f->mag[i] = MARK;
	for (i = 0; i < FSD_TRACE_LEN; i++)
		f->trace[i] = MARK;
}

static void
fixture_free(struct hdxfix *f)
{
	f->dsp.fsd.trace = 0;
	ref_FPM_FSD_free(&f->dsp.fsd);
	ref_FPM_MRF_free(&f->dsp.mrf);
}

/*
 * The DSP block byte for byte, with every pointer that is per-fixture or
 * per-allocation skipped.  What they point AT is compared separately.
 */
static long
dsp_first_diff(const struct hdxfix *a, const struct hdxfix *b)
{
	const unsigned char *pa = (const unsigned char *)(const void *)&a->dsp;
	const unsigned char *pb = (const unsigned char *)(const void *)&b->dsp;
	int skip[6];
	int i, k;

	skip[0] = (int)((const char *)&a->dsp.mrf.history - (const char *)&a->dsp);
	skip[1] = (int)((const char *)&a->dsp.fsd.trace - (const char *)&a->dsp);
	skip[2] = (int)((const char *)&a->dsp.fsd.fir_hist - (const char *)&a->dsp);
	skip[3] = (int)((const char *)&a->dsp.fsd.iir_hist - (const char *)&a->dsp);
	skip[4] = (int)((const char *)&a->dsp.mtd - (const char *)&a->dsp);
	skip[5] = (int)((const char *)&a->dsp.mag - (const char *)&a->dsp);

	for (i = 0; i < (int)sizeof(a->dsp); i++) {
		int skipped = 0;

		for (k = 0; k < 6; k++)
			if (i >= skip[k] && i < skip[k] + (int)sizeof(void *))
				skipped = 1;
		if (skipped)
			continue;
		if (pa[i] != pb[i])
			return i;
	}
	return -1;
}

static long
mtd_first_diff(const struct fpm_mtd *a, const struct fpm_mtd *b)
{
	const unsigned char *pa = (const unsigned char *)(const void *)a;
	const unsigned char *pb = (const unsigned char *)(const void *)b;
	int acc = (int)((const char *)&a->acc - (const char *)a);
	int i;

	for (i = 0; i < (int)sizeof(*a); i++) {
		if (i >= acc && i < acc + (int)sizeof(void *))
			continue;
		if (pa[i] != pb[i])
			return i;
	}
	return -1;
}

static long
obj_first_diff(const struct hdxfix *a, const struct hdxfix *b)
{
	int i;

	for (i = 0; i < RXOBJ_SIZE; i++) {
		if (i >= V21RX_OBJ_HDX && i < V21RX_OBJ_HDX + (int)sizeof(void *))
			continue;
		if (i >= V21RX_OBJ_DSP && i < V21RX_OBJ_DSP + (int)sizeof(void *))
			continue;
		if (a->obj[i] != b->obj[i])
			return i;
	}
	return -1;
}

/*
 * The half-duplex block, with the handler slot compared by IDENTITY rather
 * than by address.  Returns the first differing offset outside that slot, or
 * -1; the caller compares the identity separately.
 */
static long
hdx_first_diff(const struct hdxfix *a, const struct hdxfix *b)
{
	const unsigned char *pa = (const unsigned char *)(const void *)&a->hdx;
	const unsigned char *pb = (const unsigned char *)(const void *)&b->hdx;
	int h = (int)((const char *)&a->hdx.handler - (const char *)&a->hdx);
	int i;

	for (i = 0; i < (int)sizeof(a->hdx); i++) {
		if (i >= h && i < h + (int)sizeof(void *))
			continue;
		if (pa[i] != pb[i])
			return i;
	}
	return -1;
}

/* --------------------------------------------------------------------- */
/* Input shapes                                                          */

/*
 * THE THREE SHAPES ARE CHOSEN SO THAT EACH DRIVES ONE OF THE TONE DETECTOR'S
 * THREE VERDICTS, and they were derived by simulating `FPM_MTD_detect`'s
 * arithmetic over Bell 103's coefficient bank rather than by trying inputs
 * until the counters came up non-zero:
 *
 *   IN_SILENT   wideband stays under `min_level`      -> FPM_MTD_NOSIGNAL
 *   IN_TONE     pseudorandom, mostly inside the two
 *               passbands                             -> FPM_MTD_PRESENT
 *   IN_CARRIER  full-rate alternation, which both
 *               bandpasses reject                     -> FPM_MTD_ABSENT
 *
 * Only IN_CARRIER makes `CarrierDetectV21` answer yes, because that needs the
 * agc to see signal AND the detector to say ABSENT.  IN_TONE is the case
 * where the agc sees plenty and the carrier is still absent, which is the one
 * an amplitude-only fixture would have missed entirely.
 */
static void
fill_in(short *dst, int shape, int block, unsigned seed)
{
	int i;

	if (shape == IN_MIXED)
		shape = (block & 1) ? IN_TONE : IN_CARRIER;

	rng_seed(seed + (unsigned)block * 7919u);
	for (i = 0; i < IN_LEN; i++) {
		int v;

		switch (shape) {
		case IN_SILENT:
			v = 0;
			break;
		case IN_TONE:
			v = (int)(rng_next() % 20001u) - 10000;
			break;
		default:
			v = (i & 1) ? -8000 : 8000;
			break;
		}
		dst[i] = (short)v;
	}
}

/* --------------------------------------------------------------------- */
/* Counters, all asserted non-zero at the end                            */

static long dd_blocks, dd_absent, dd_fired, dd_squelched, dd_unsquelched;
static long dd_bits, dd_energy0, dd_energy1, dd_wrote_mag;
static long dd_sep_energy, dd_sep_tone, dd_sep_squelch, dd_sep_count;

static long err_blocks, err_flag_sep, err_count_sep;

static long idle_blocks, idle_carrier, idle_nocarrier, idle_status_sep;
static long idle_assign_sep;

static long wait_blocks, wait_carrier, wait_nocarrier, wait_expired;
static long wait_held, wait_ret_sep, wait_pre_sep, wait_state_moved;

static long data_blocks, data_carrier, data_nocarrier, data_gated;
static long data_snr_sep, data_assign_sep, data_gate_sep, data_state_moved;

static long adv_seen[5];

/* --------------------------------------------------------------------- */
/* A stream of blocks through one entry point                            */

#define CALL_DEMOD	0
#define CALL_ERROR	1
#define CALL_IDLE	2
#define CALL_WAIT	3
#define CALL_DATA	4

/*
 * Run BLOCKS consecutive blocks through both sides and compare everything
 * after every one.  Returns nothing; the counters above are what the closing
 * assertions read.
 */
static void
run_stream(int which, int shape, int handler, short state,
	   unsigned short countdown, int gate, unsigned char flags,
	   unsigned char flags1, unsigned char status, unsigned seed)
{
	long where = (long)which * 100000 + (long)shape * 10000
		     + (long)handler * 1000 + (long)state * 100
		     + (long)countdown * 10 + gate;
	int block;

	fixture(&fa, seed, 1, handler, state, countdown, gate, shape, flags,
		flags1, status);
	fixture(&fb, seed, 0, handler, state, countdown, gate, shape, flags,
		flags1, status);

	for (block = 0; block < BLOCKS; block++) {
		short ca = BLOCK, cb = BLOCK;
		long ra, rb;
		int i;
		int ida, idb;
		int fired;

		fill_in(in_a, shape, block, seed);
		memcpy(in_b, in_a, sizeof(in_a));
		for (i = 0; i < BITS_LEN; i++)
			bits_a[i] = bits_b[i] = MARK;

		switch (which) {
		case CALL_DEMOD:
			ra = (long)ref_DemodDataV21(fa.obj, in_a, bits_a,
						    (unsigned short)BLOCK);
			rb = (long)DemodDataV21(fb.obj, in_b, bits_b,
						(unsigned short)BLOCK);
			break;
		case CALL_ERROR:
			ra = ref_RxHdxErrorV21(fa.obj, in_a, bits_a, &ca);
			rb = RxHdxErrorV21(fb.obj, in_b, bits_b, &cb);
			break;
		case CALL_IDLE:
			ra = ref_RxHdxIdleV21(fa.obj, in_a, bits_a, &ca);
			rb = RxHdxIdleV21(fb.obj, in_b, bits_b, &cb);
			break;
		case CALL_WAIT:
			ra = ref_RxHdxWaitV21(fa.obj, in_a, bits_a, &ca);
			rb = RxHdxWaitV21(fb.obj, in_b, bits_b, &cb);
			break;
		default:
			ra = ref_RxHdxDataV21(fa.obj, in_a, bits_a, &ca);
			rb = RxHdxDataV21(fb.obj, in_b, bits_b, &cb);
			break;
		}

		diff_eq_int("at %ld: return", rb, ra, where * 100 + block);
		diff_eq_int("at %ld: count out", (long)cb, (long)ca,
			    where * 100 + block);

		for (i = 0; i < IN_LEN; i++)
			diff_eq_int("in[%ld]", in_b[i], in_a[i], i);
		for (i = 0; i < BITS_LEN; i++)
			diff_eq_int("bits[%ld]", bits_b[i], bits_a[i], i);
		for (i = 0; i < MAG_LEN; i++)
			diff_eq_int("mag[%ld]", fb.mag[i], fa.mag[i], i);
		for (i = 0; i < FSD_TRACE_LEN; i++)
			diff_eq_int("trace[%ld]", fb.trace[i], fa.trace[i], i);
		for (i = 0; i < MTD_TONES * 2; i++)
			diff_eq_int("mtd acc[%ld]", fb.mtd_acc[i],
				    fa.mtd_acc[i], i);

		/*
		 * The detector byte for byte with its accumulator POINTER
		 * skipped -- it is per-fixture -- and the array it points at
		 * compared above.
		 */
		diff_eq_int("at %ld: first differing detector byte",
			    mtd_first_diff(&fa.mtd, &fb.mtd), -1,
			    where * 100 + block);

		diff_eq_int("at %ld: first differing DSP byte",
			    dsp_first_diff(&fa, &fb), -1, where * 100 + block);
		diff_eq_int("at %ld: first differing handle byte",
			    obj_first_diff(&fa, &fb), -1, where * 100 + block);
		diff_eq_int("at %ld: first differing hdx byte",
			    hdx_first_diff(&fa, &fb), -1, where * 100 + block);

		for (i = 0; i < fa.dsp.mrf.history_len; i++)
			diff_eq_int("mrf history[%ld]", fb.dsp.mrf.history[i],
				    fa.dsp.mrf.history[i], i);
		for (i = 0; i < FSD_FIR_TAPS; i++)
			diff_eq_int("fsd fir_hist[%ld]", fb.dsp.fsd.fir_hist[i],
				    fa.dsp.fsd.fir_hist[i], i);
		for (i = 0; i < FSD_IIR_LEN * 2; i++)
			diff_eq_int("fsd iir_hist[%ld]", fb.dsp.fsd.iir_hist[i],
				    fa.dsp.fsd.iir_hist[i], i);

		ida = handler_id(fa.hdx.handler);
		idb = handler_id(fb.hdx.handler);
		diff_eq_int("at %ld: the handler chosen", (long)idb, (long)ida,
			    where * 100 + block);
		diff_eq_int("at %ld: the handler is a known one", ida != H_UNKNOWN,
			    1, where * 100 + block);

		/* ------------------------------------------------------- */
		/* What the blob actually did, counted from the blob       */

		if (fa.hdx.state >= 0 && fa.hdx.state <= 4)
			adv_seen[fa.hdx.state]++;

		if (which == CALL_DEMOD) {
			dd_blocks++;
			fired = (fa.dsp.int_0008 == 0);
			if (fired)
				dd_fired++;
			else
				dd_absent++;
			if (fa.dsp.int_0004 != 0)
				dd_energy1++;
			else
				dd_energy0++;
			if (ra > 0)
				dd_bits++;

			/*
			 * WRONG READING: int_0004 from anything but the agc's
			 * `signal`.  The neighbour a slip reads is `f18`,
			 * which init sets to 1 and agc() never touches.
			 */
			if (fa.dsp.int_0004 != fa.dsp.agc.f18)
				dd_sep_energy++;
			/*
			 * WRONG READING: int_0008 taken as the verdict rather
			 * than as `verdict == ABSENT`.  Separated whenever the
			 * detector fired, since the verdict is then 1 or 2 and
			 * the field is 0.
			 */
			if (fired)
				dd_sep_tone++;
			/*
			 * WRONG READING: the squelch run on the wrong arm.
			 * Counted only when the detector fired, because that
			 * is the only time the two arms differ at all.
			 */
			if (fired) {
				int zeroed = 1;

				for (i = 0; i < BLOCK; i++)
					if (in_a[i] != 0)
						zeroed = 0;
				if (zeroed)
					dd_squelched++;
				else
					dd_unsquelched++;
				if (handler != H_DATA && zeroed)
					dd_sep_squelch++;
			}
			/*
			 * WRONG READING: the demodulator fed the block count
			 * rather than the resampler's output count.  With
			 * 3:10 those are 36 and 10, so the trace length the
			 * demodulator writes separates them.
			 */
			for (i = 0; i < FSD_TRACE_LEN; i++)
				if (fa.trace[i] != MARK)
					break;
			if (i < FSD_TRACE_LEN)
				dd_wrote_mag++;
			if (fa.dsp.fsd.last_count != BLOCK
			    && fa.dsp.fsd.last_count != 0)
				dd_sep_count++;
		} else if (which == CALL_ERROR) {
			err_blocks++;
			if ((fa.obj[V21RX_OBJ_FLAGS] & V21RX_FLAG_ERROR) != 0)
				err_flag_sep++;
			if (ca == 0 && BLOCK != 0)
				err_count_sep++;
		} else if (which == CALL_IDLE) {
			idle_blocks++;
			if ((fa.obj[V21RX_OBJ_FLAGS] & V21RX_FLAG_CARRIER) != 0)
				idle_carrier++;
			else
				idle_nocarrier++;
			if (fa.obj[V21RX_OBJ_STATUS] == V21RX_STATUS_IDLE)
				idle_status_sep++;
			/*
			 * WRONG READING: assigning the carrier bit from the
			 * detector rather than clearing and re-setting it.
			 * The two agree on the VALUE; what separates them is
			 * that the object's spelling leaves the bit clear
			 * across the call, which a caller cannot see -- so
			 * this is counted as "the arm was driven both ways",
			 * which is what makes the check real.
			 */
			if (idle_carrier > 0 && idle_nocarrier > 0)
				idle_assign_sep++;
		} else if (which == CALL_WAIT) {
			wait_blocks++;
			if (handler_id(fa.hdx.handler) == H_ERROR
			    && fa.obj[V21RX_OBJ_STATUS] == V21RX_STATUS_ERROR) {
				wait_nocarrier++;
				if (ra == 0)
					wait_ret_sep++;
			} else {
				wait_carrier++;
				if (fa.obj[V21RX_OBJ_STATUS]
				    == V21RX_STATUS_TIMEOUT)
					wait_expired++;
				else
					wait_held++;
			}
			/*
			 * WRONG READING: the countdown tested before the
			 * decrement.  A stream started at 1 expires on the
			 * first block under the object's order and on the
			 * second under the other one.
			 */
			if (block == 0 && countdown == 1
			    && fa.obj[V21RX_OBJ_STATUS] == V21RX_STATUS_TIMEOUT)
				wait_pre_sep++;
			if (fa.hdx.state != state)
				wait_state_moved++;
		} else {
			data_blocks++;
			if ((fa.obj[V21RX_OBJ_FLAGS] & V21RX_FLAG_CARRIER) != 0)
				data_carrier++;
			else
				data_nocarrier++;
			if (gate != 0 && ra == 0)
				data_gated++;
			/*
			 * WRONG READING: the SNR bit raised on `< 5`.
			 * GetSNRV21 answers 0, so `<= 5` raises it on every
			 * block the data arm demodulates and `< 5` does too;
			 * what separates the SITE is that the bit is CLEARED
			 * first, so a block that does not reach the arm leaves
			 * it as the caller had it.
			 */
			if ((fa.obj[V21RX_OBJ_FLAGS] & V21RX_FLAG_LOW_SNR) != 0)
				data_snr_sep++;
			if (data_carrier > 0 && data_nocarrier > 0)
				data_assign_sep++;
			if (gate != 0
			    && (fa.obj[V21RX_OBJ_FLAGS] & V21RX_FLAG_CARRIER) == 0)
				data_gate_sep++;
			if (fa.hdx.state != state)
				data_state_moved++;
		}
	}

	fixture_free(&fa);
	fixture_free(&fb);
}

/* --------------------------------------------------------------------- */

static int
run_demod(void)
{
	diff_begin("DemodDataV21");

	/* The DATA handler installed: the squelch arm must NOT run. */
	run_stream(CALL_DEMOD, IN_CARRIER, H_DATA, V21RX_STATE_DATA, 0, 0,
		   0x00, 0x00, 0x00, 0x21000001u);
	run_stream(CALL_DEMOD, IN_SILENT, H_DATA, V21RX_STATE_DATA, 0, 0,
		   0xff, 0xff, 0x00, 0x21000002u);
	run_stream(CALL_DEMOD, IN_MIXED, H_DATA, V21RX_STATE_DATA, 0, 0,
		   0x50, 0x00, 0x01, 0x21000003u);

	/* Some other handler installed: it must. */
	run_stream(CALL_DEMOD, IN_CARRIER, H_OTHER, V21RX_STATE_WAIT, 0, 0,
		   0x00, 0x00, 0x00, 0x21000004u);
	run_stream(CALL_DEMOD, IN_SILENT, H_OTHER, V21RX_STATE_WAIT, 0, 0,
		   0x00, 0x00, 0x00, 0x21000005u);
	run_stream(CALL_DEMOD, IN_MIXED, H_ERROR, V21RX_STATE_ERROR, 0, 0,
		   0xff, 0x01, 0x04, 0x21000006u);
	run_stream(CALL_DEMOD, IN_MIXED, H_NONE, V21RX_STATE_START, 0, 0,
		   0x00, 0x00, 0x00, 0x21000007u);

	/* The detector's PRESENT verdict against both handler arms. */
	run_stream(CALL_DEMOD, IN_TONE, H_DATA, V21RX_STATE_DATA, 0, 0,
		   0x00, 0x00, 0x00, 0x21000008u);
	run_stream(CALL_DEMOD, IN_TONE, H_OTHER, V21RX_STATE_WAIT, 0, 0,
		   0x00, 0x00, 0x00, 0x21000009u);

	return diff_end();
}

static int
run_error(void)
{
	diff_begin("RxHdxErrorV21");

	run_stream(CALL_ERROR, IN_CARRIER, H_ERROR, V21RX_STATE_ERROR, 0, 0,
		   0x00, 0x00, 0x04, 0x21100001u);
	run_stream(CALL_ERROR, IN_SILENT, H_ERROR, V21RX_STATE_ERROR, 0, 0,
		   0xfd, 0xff, 0x00, 0x21100002u);
	run_stream(CALL_ERROR, IN_MIXED, H_DATA, V21RX_STATE_DATA, 3, 1,
		   0x50, 0x00, 0x02, 0x21100003u);

	return diff_end();
}

static int
run_idle(void)
{
	diff_begin("RxHdxIdleV21");

	run_stream(CALL_IDLE, IN_CARRIER, H_IDLE, V21RX_STATE_IDLE, 0, 0,
		   0x00, 0x01, 0x05, 0x21200001u);
	run_stream(CALL_IDLE, IN_SILENT, H_IDLE, V21RX_STATE_IDLE, 0, 0,
		   0xff, 0xff, 0x00, 0x21200002u);
	run_stream(CALL_IDLE, IN_MIXED, H_IDLE, V21RX_STATE_IDLE, 0, 0,
		   0x20, 0x00, 0x03, 0x21200003u);
	run_stream(CALL_IDLE, IN_MIXED, H_DATA, V21RX_STATE_DATA, 0, 0,
		   0x00, 0x00, 0x00, 0x21200004u);

	return diff_end();
}

static int
run_wait(void)
{
	diff_begin("RxHdxWaitV21");

	/* Carrier up, countdown 1: the advance fires on the first block. */
	run_stream(CALL_WAIT, IN_CARRIER, H_WAIT, V21RX_STATE_WAIT, 1, 0,
		   0x00, 0x00, 0x02, 0x21300001u);
	/* Carrier up, a long countdown: it is held for several blocks. */
	run_stream(CALL_WAIT, IN_CARRIER, H_WAIT, V21RX_STATE_WAIT, 5, 0,
		   0x50, 0x00, 0x02, 0x21300002u);
	/* No carrier: the error handler goes in and the return is zero. */
	run_stream(CALL_WAIT, IN_SILENT, H_WAIT, V21RX_STATE_WAIT, 4, 0,
		   0x00, 0x00, 0x02, 0x21300003u);
	/* The other four states, so every arm of the advance is driven. */
	run_stream(CALL_WAIT, IN_CARRIER, H_WAIT, V21RX_STATE_START, 1, 0,
		   0x00, 0x00, 0x01, 0x21300004u);
	run_stream(CALL_WAIT, IN_CARRIER, H_WAIT, V21RX_STATE_DATA, 1, 0,
		   0x01, 0x00, 0x00, 0x21300005u);
	run_stream(CALL_WAIT, IN_CARRIER, H_WAIT, V21RX_STATE_IDLE, 1, 0,
		   0x21, 0x01, 0x05, 0x21300006u);
	run_stream(CALL_WAIT, IN_CARRIER, H_WAIT, V21RX_STATE_ERROR, 1, 0,
		   0xff, 0xff, 0x04, 0x21300007u);
	/* And the countdown wrapping through zero. */
	run_stream(CALL_WAIT, IN_CARRIER, H_WAIT, V21RX_STATE_WAIT, 0, 0,
		   0x00, 0x00, 0x02, 0x21300008u);
	run_stream(CALL_WAIT, IN_MIXED, H_WAIT, V21RX_STATE_WAIT, 3, 0,
		   0x00, 0x00, 0x02, 0x21300009u);

	return diff_end();
}

static int
run_data(void)
{
	diff_begin("RxHdxDataV21");

	/*
	 * The carrier here is whatever the PREVIOUS block left in the two
	 * ints, because RxHdxDataV21 tests it before it demodulates.  A loud
	 * stream therefore reaches the demodulating arm from the second block
	 * on, and a silent one never does.
	 */
	run_stream(CALL_DATA, IN_CARRIER, H_DATA, V21RX_STATE_DATA, 0, 0,
		   0x00, 0x00, 0x00, 0x21400001u);
	run_stream(CALL_DATA, IN_SILENT, H_DATA, V21RX_STATE_DATA, 0, 0,
		   0xff, 0xff, 0x00, 0x21400002u);
	run_stream(CALL_DATA, IN_MIXED, H_DATA, V21RX_STATE_DATA, 0, 0,
		   0x21, 0x00, 0x00, 0x21400003u);
	/* The second gate: hdx->int_0000 non-zero refuses to demodulate. */
	run_stream(CALL_DATA, IN_CARRIER, H_DATA, V21RX_STATE_DATA, 0, 1,
		   0x00, 0x00, 0x00, 0x21400004u);
	run_stream(CALL_DATA, IN_CARRIER, H_DATA, V21RX_STATE_DATA, 0, -7,
		   0x80, 0x01, 0x03, 0x21400005u);
	/* Every state, so the inlined advance is driven from here too. */
	run_stream(CALL_DATA, IN_SILENT, H_DATA, V21RX_STATE_START, 0, 0,
		   0x00, 0x00, 0x01, 0x21400006u);
	run_stream(CALL_DATA, IN_SILENT, H_DATA, V21RX_STATE_WAIT, 0, 0,
		   0x00, 0x00, 0x02, 0x21400007u);
	run_stream(CALL_DATA, IN_SILENT, H_DATA, V21RX_STATE_IDLE, 0, 0,
		   0x01, 0x01, 0x05, 0x21400008u);
	run_stream(CALL_DATA, IN_SILENT, H_DATA, V21RX_STATE_ERROR, 0, 0,
		   0xff, 0xff, 0x04, 0x21400009u);

	return diff_end();
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	build_tables();

	rc |= run_demod();
	rc |= run_error();
	rc |= run_idle();
	rc |= run_wait();
	rc |= run_data();

	/*
	 * The separating counts.  Each is the number of trials on which a
	 * NAMED wrong reading produced a different OBSERVABLE answer from the
	 * blob's, or on which an arm was reached at all, counted from what the
	 * blob actually did.  A zero here means the corresponding check above
	 * is decoration -- finding F134.
	 */
	diff_begin("v21hdx separating trials");

	diff_eq_int("DemodDataV21 was driven (%ld)", dd_blocks > 0, 1,
		    dd_blocks);
	diff_eq_int("the detector said ABSENT (%ld)", dd_absent > 0, 1,
		    dd_absent);
	diff_eq_int("the detector fired (%ld)", dd_fired > 0, 1, dd_fired);
	diff_eq_int("the squelch arm ran (%ld)", dd_squelched > 0, 1,
		    dd_squelched);
	diff_eq_int("the squelch arm was skipped (%ld)", dd_unsquelched > 0, 1,
		    dd_unsquelched);
	diff_eq_int("the demodulator produced bits (%ld)", dd_bits > 0, 1,
		    dd_bits);
	diff_eq_int("the agc reported signal (%ld)", dd_energy1 > 0, 1,
		    dd_energy1);
	diff_eq_int("the agc reported none (%ld)", dd_energy0 > 0, 1,
		    dd_energy0);
	diff_eq_int("the resampler wrote the trace (%ld)", dd_wrote_mag > 0, 1,
		    dd_wrote_mag);
	diff_eq_int("the energy field separates from f18 (%ld)",
		    dd_sep_energy > 0, 1, dd_sep_energy);
	diff_eq_int("the tone field separates from the verdict (%ld)",
		    dd_sep_tone > 0, 1, dd_sep_tone);
	diff_eq_int("the squelch arm separates on the handler (%ld)",
		    dd_sep_squelch > 0, 1, dd_sep_squelch);
	diff_eq_int("the sample count separates from the block (%ld)",
		    dd_sep_count > 0, 1, dd_sep_count);

	diff_eq_int("RxHdxErrorV21 was driven (%ld)", err_blocks > 0, 1,
		    err_blocks);
	diff_eq_int("the error flag was raised (%ld)", err_flag_sep > 0, 1,
		    err_flag_sep);
	diff_eq_int("the count was consumed (%ld)", err_count_sep > 0, 1,
		    err_count_sep);

	diff_eq_int("RxHdxIdleV21 was driven (%ld)", idle_blocks > 0, 1,
		    idle_blocks);
	diff_eq_int("idle saw a carrier (%ld)", idle_carrier > 0, 1,
		    idle_carrier);
	diff_eq_int("idle saw none (%ld)", idle_nocarrier > 0, 1,
		    idle_nocarrier);
	diff_eq_int("idle reported its status (%ld)", idle_status_sep > 0, 1,
		    idle_status_sep);
	diff_eq_int("idle drove the carrier bit both ways (%ld)",
		    idle_assign_sep > 0, 1, idle_assign_sep);

	diff_eq_int("RxHdxWaitV21 was driven (%ld)", wait_blocks > 0, 1,
		    wait_blocks);
	diff_eq_int("wait saw a carrier (%ld)", wait_carrier > 0, 1,
		    wait_carrier);
	diff_eq_int("wait lost the carrier (%ld)", wait_nocarrier > 0, 1,
		    wait_nocarrier);
	diff_eq_int("wait held the state (%ld)", wait_held > 0, 1, wait_held);
	diff_eq_int("wait's countdown expired (%ld)", wait_expired > 0, 1,
		    wait_expired);
	diff_eq_int("wait returned zero on the error arm (%ld)",
		    wait_ret_sep > 0, 1, wait_ret_sep);
	diff_eq_int("the decrement-before-test separates (%ld)",
		    wait_pre_sep > 0, 1, wait_pre_sep);
	diff_eq_int("wait advanced the state (%ld)", wait_state_moved > 0, 1,
		    wait_state_moved);

	diff_eq_int("RxHdxDataV21 was driven (%ld)", data_blocks > 0, 1,
		    data_blocks);
	diff_eq_int("data saw a carrier (%ld)", data_carrier > 0, 1,
		    data_carrier);
	diff_eq_int("data lost the carrier (%ld)", data_nocarrier > 0, 1,
		    data_nocarrier);
	diff_eq_int("the second gate refused a block (%ld)", data_gated > 0, 1,
		    data_gated);
	diff_eq_int("the second gate separates (%ld)", data_gate_sep > 0, 1,
		    data_gate_sep);
	diff_eq_int("the SNR bit was raised (%ld)", data_snr_sep > 0, 1,
		    data_snr_sep);
	diff_eq_int("data drove the carrier bit both ways (%ld)",
		    data_assign_sep > 0, 1, data_assign_sep);
	diff_eq_int("data advanced the state (%ld)", data_state_moved > 0, 1,
		    data_state_moved);

	/*
	 * Every arm of the inlined state advance was entered.  Without this
	 * the switch could be missing a case and no check above would notice,
	 * because a case that is never reached agrees with anything.
	 */
	diff_eq_int("the START arm was entered (%ld)", adv_seen[0] > 0, 1,
		    adv_seen[0]);
	diff_eq_int("the WAIT arm was entered (%ld)", adv_seen[1] > 0, 1,
		    adv_seen[1]);
	diff_eq_int("the DATA arm was entered (%ld)", adv_seen[2] > 0, 1,
		    adv_seen[2]);
	diff_eq_int("the IDLE arm was entered (%ld)", adv_seen[3] > 0, 1,
		    adv_seen[3]);
	diff_eq_int("the ERROR arm was entered (%ld)", adv_seen[4] > 0, 1,
		    adv_seen[4]);

	rc |= diff_end();

	return rc;
}
