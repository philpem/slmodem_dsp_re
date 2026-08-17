/*
 * t_v90p4ddec.cpp -- differential test of the three V90Phase4Demodulator
 * decision members:
 *
 *     V90Phase4Demodulator::getDecision(short)        52 bytes
 *     V90Phase4Demodulator::getV90Decision(short)  3,095
 *     V90Phase4Demodulator::getV92Decision(short)  3,252
 *
 * ---------------------------------------------------------------------------
 * WHAT IS COMPARED
 *
 * One sample in, one soft decision out, and an eighteen-way switch on +0x20 in
 * between that can reach `V90Demapper`, `V90CP`, `V90MP`, `Descrambler`, both
 * embedded `V90RDetector`s and `V90ConnectionEvaluator`.  So the comparison is
 * four-way and all four halves matter:
 *
 *   - the 13,596-byte demodulator, whole, guard included;
 *   - every peer object either side touched, whole;
 *   - the return value, on the arms where there IS one (see below);
 *   - the diagnostic transcript, as text, at levels 0, 2 and 3.
 *
 * THE OBJECTS ARE NEVER ZEROED -- finding 230.  Every slot gets varied
 * pseudorandom bytes before every trial, so a store that fails to happen is
 * visible and a store of zero into memory that was already zero is not
 * mistaken for one.  The fields each arm reads are then planted on top,
 * identically on both sides.
 *
 * ---------------------------------------------------------------------------
 * FIVE ARMS HAVE NO RETURN VALUE TO COMPARE, AND THAT IS THE OBJECT'S
 *
 * Both functions build the answer in %edi and never write it on the paths that
 * reach the epilogue from V.90's states 4 and 0x10, V.92's states 5 and 6, or
 * from the out-of-range `ja` at the top.  What comes back there is the
 * CALLER'S %edi.  Deviation D600 records it; here it means the return value is
 * asserted on the fifteen arms that define one and never on those five -- an
 * assertion there would be comparing two pieces of stack litter and would pass
 * or fail for reasons that have nothing to do with this reconstruction.  The
 * OBJECT and the transcript are still compared on all of them, and for four of
 * the five that is the whole of the arm's work anyway.
 *
 * ---------------------------------------------------------------------------
 * WHICH PEERS ARE SHARED AND WHICH ARE SPLIT
 *
 * Shared, because nothing under test writes through them, so an identical
 * pointer keeps the field IN the comparison rather than blanked out of it
 * (finding 1105): `params`, the two `V90MappingParams` blocks, the connection
 * evaluator and the auto-digital-impairment detector.
 *
 * Split, one per side, because they are written: the demapper, the CP record,
 * the MP record and the descrambler.  Their four pointer words in the
 * demodulator -- +0x14, +0x18, +0x3054 and +0x3058 -- are blanked in a scratch
 * copy before the object comparison and the four objects are then compared
 * themselves, which is where those arms' work actually lands.
 *
 * THE DESCRAMBLER IS COMPARED THROUGH ITS BUFFER AND ITS CURSORS AS OFFSETS,
 * not byte for byte: seven of its eight words are addresses into a per-side
 * array and would differ for ever.  The 123-byte array and the three cursor
 * displacements are the whole of its state.
 *
 * ---------------------------------------------------------------------------
 * ANTI-VACUITY, per finding 3509 rather than per path
 *
 * Every counter below names an OBSERVABLE difference -- a returned decision, a
 * byte of some object, a line of transcript -- and never "a branch believed to
 * have been taken".  A run in which the detectors never fired, the bit loops
 * never ran or nothing was ever printed FAILS.  The mutations in
 * `test/mutations/v90p4ddec.json` are what adjudicate; the counters only stop
 * a green run that measured nothing.
 *
 * TWO OF THE CHECKS ARE BLOB AGAINST BLOB, so they are properties of the
 * object rather than of this reconstruction, and they are the ones that hold
 * claims no comparison of two identically-seeded runs can:
 *
 *   - the same state, the same sample, two different `linearMappStudyStart`
 *     values, and the two transcripts must DIFFER.  That is what says the
 *     study gate reads +0x3518 and not a constant.
 *   - `getDecision` with `sessionFlag` zero and non-zero, in a state where the
 *     two arms do different things, and the two OBJECTS must differ.  That is
 *     what says +0x00 selects, and it cannot be faked by a `getDecision` that
 *     always calls the same arm.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

int ref_p4d_getdecision(void *, short)
	asm("ref__ZN20V90Phase4Demodulator11getDecisionEs");
short ref_p4d_getv90decision(void *, short)
	asm("ref__ZN20V90Phase4Demodulator14getV90DecisionEs");
short ref_p4d_getv92decision(void *, short)
	asm("ref__ZN20V90Phase4Demodulator14getV92DecisionEs");

int our_p4d_getdecision(void *, short)
	asm("_ZN20V90Phase4Demodulator11getDecisionEs");
short our_p4d_getv90decision(void *, short)
	asm("_ZN20V90Phase4Demodulator14getV90DecisionEs");
short our_p4d_getv92decision(void *, short)
	asm("_ZN20V90Phase4Demodulator14getV92DecisionEs");
}

/* ----------------------------------------------------------- the storage */

#define P4D_SLOT	((unsigned)sizeof(V90Phase4Demodulator) + 64u)
#define DEM_SLOT	((unsigned)sizeof(V90Demapper) + 64u)
#define CP_SLOT		((unsigned)sizeof(V90CP) + 64u)
#define MP_SLOT		((unsigned)sizeof(V90MP) + 64u)

/* Room for far more samples and bits than any trial below asks for. */
#define NSAMPLE		72
#define NCPBUF		16

/*
 * The V.90 descrambler's geometry, from `V90ModemCtor`: taps 18 and 23 and a
 * 99-element run-up, so the block is 1 + 23 + 99 elements.  Scrambler.h
 * records the same three numbers.
 */
#define DSC_A		18u
#define DSC_B		23u
#define DSC_C		99u
#define DSC_N		(1u + DSC_B + DSC_C)

static unsigned char p4d_s[2][P4D_SLOT] __attribute__((aligned(8)));
static unsigned char p4d_seed[P4D_SLOT];
static unsigned char p4d_cmp[2][P4D_SLOT];

static unsigned char dem_s[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char dem_cmp[2][DEM_SLOT];
static unsigned char cp_s[2][CP_SLOT] __attribute__((aligned(8)));
static unsigned char cp_cmp[2][CP_SLOT];
static unsigned char mp_s[2][MP_SLOT] __attribute__((aligned(8)));

static unsigned int code_s[2][NSAMPLE];
static unsigned char sign_s[2][NSAMPLE];
static unsigned char sbstate_s[2][V90SBE_DECODER_SIZE];
static int cpbuf_s[2][V90CP_BUFS][NCPBUF];

typedef Descrambler<unsigned char, int> V90Descrambler;

/*
 * RAW STORAGE, not a constructed object: `Descrambler` has no default
 * constructor and the real one calls `sysdep_malloc`, which would put a
 * fresh block behind each side every trial and hide the seed.  Every
 * member is public and the eight words are planted directly, in exactly
 * the arrangement the constructor produces.
 */
static unsigned char dsc_s[2][sizeof(V90Descrambler)]
	__attribute__((aligned(8)));
static unsigned char dscbuf_s[2][DSC_N];

/* Shared, because nothing under test writes through them. */
static unsigned char parm_s[sizeof(V90Parameters) + 64]
	__attribute__((aligned(8)));
static unsigned char mapp1_s[sizeof(V90MappingParams) + 64]
	__attribute__((aligned(8)));
static unsigned char mapp2_s[sizeof(V90MappingParams) + 64]
	__attribute__((aligned(8)));
static unsigned char ce_s[sizeof(V90ConnectionEvaluator) + 64]
	__attribute__((aligned(8)));
static unsigned char adi_s[sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));

#define DSC(s)		(*(V90Descrambler *)dsc_s[s])
#define P4D(s)		(*(V90Phase4Demodulator *)p4d_s[s])
#define DEM(s)		(*(V90Demapper *)dem_s[s])
#define CPR(s)		(*(V90CP *)cp_s[s])
#define MPR(s)		(*(V90MP *)mp_s[s])
#define PARAMS		((V90Parameters *)parm_s)
#define MAPP1		((V90MappingParams *)mapp1_s)
#define MAPP2		((V90MappingParams *)mapp2_s)
#define CEV		((V90ConnectionEvaluator *)ce_s)
#define ADI		((V90AutoDigitalImpDetector *)adi_s)

/* Varied, never zero, never the same twice: findings 223, 224, 230. */
static unsigned
fill(unsigned char *p, unsigned n, unsigned lfsr)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		p[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
	return lfsr;
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ------------------------------------------------------------- the fixture */

/*
 * THE FOUR SPLIT PEERS' POINTER WORDS.  Blanked in a scratch copy before the
 * demodulator is compared, and the four objects are compared themselves.
 */
static const unsigned p4d_skip[] = { 0x14u, 0x18u, 0x3054u, 0x3058u, ~0u };

/* And the demapper's own, which point into per-side arrays. */
static const unsigned dem_skip[] = { 0x1cu, 0x20u, 0x684u, ~0u };

static void
scrub(unsigned char *dst, const unsigned char *src, unsigned n,
      const unsigned *skip)
{
	int i;

	memcpy(dst, src, n);
	for (i = 0; skip[i] != ~0u; i++)
		memset(dst + skip[i], 0, 4);
}

/*
 * Plant one demodulator and its four private peers per side.  Nothing here
 * calls a constructor: `V90Phase4Demodulator`'s is t_v90p4dctor's job and
 * running it would allocate and hide the seed the comparison rests on.
 */
static void
setup(int trial, int mode)
{
	unsigned lf = 0x37c1u + 0x9e37u * (unsigned)trial + 0x51edu *
		      (unsigned)mode;
	int s;
	int k;

	fill(parm_s, (unsigned)sizeof parm_s, lf ^ 0x4321u);
	fill(mapp1_s, (unsigned)sizeof mapp1_s, lf ^ 0x8ac1u);
	fill(mapp2_s, (unsigned)sizeof mapp2_s, lf ^ 0x1f77u);
	fill(ce_s, (unsigned)sizeof ce_s, lf ^ 0x2b19u);
	fill(adi_s, (unsigned)sizeof adi_s, lf ^ 0x6d05u);

	/*
	 * The six parameters the silence chain reads, planted rather than
	 * left to the seed: three of them are compared against `countInState`
	 * and a pseudorandom 32-bit value would put every threshold out of
	 * reach.  `RRN_TRN2D_DD_LENGTH` is what the RdNot arm copies into
	 * `trn2dDDLength`, so it has to be distinguishable from
	 * `TRN2D_QC_DD_LENGTH`, which `reset` uses.
	 */
	PARAMS->RRN_R_DETECTION_LENGTH = 0x60;
	PARAMS->RRN_TRN2D_DD_LENGTH = 0x2a0;
	PARAMS->RRN_SILENCE_SCR_LENGTH = 0x120;
	PARAMS->RRN_SILENCE_WAIT_BEFORE_ECHO_CALC = 0x30;
	PARAMS->RRN_SILENCE_ECHO_CALC_PERIOD = 0x24;
	PARAMS->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE =
		(mode & 1) ? 6.0f : -80.0f;

	/*
	 * `3 * word_0 + 0x17` is the B1d delay, and it has to be small enough
	 * that the zero counter can pass it inside one trial.
	 */
	MAPP1->word_0 = 3u;
	MAPP2->word_0 = 5u;

	/*
	 * AND THE TWO MAPPING BLOCKS HAVE TO BE SANE, because four arms reach
	 * `V90Demapper::resetNoSpectral`, which copies `constellationSize`
	 * straight out of the block and then walks it.  A pseudorandom word
	 * there is a four-billion-iteration loop writing off the end of the
	 * demapper -- which is what the first run of this fixture did, and it
	 * segfaulted rather than failing a check.  Each code is also an index
	 * into the detector's 128-entry rows, so they are masked.
	 */
	PARAMS->DEBUG_DEMAPPER_ERROR_HISTOGRAM = 0;
	PARAMS->DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM = 0;
	for (k = 0; k < V90_CONSTELLATIONS; k++) {
		int j;

		MAPP1->constellationSize[k] = 32u;
		MAPP2->constellationSize[k] = 24u;
		for (j = 0; j < V90_CONSTELLATION_MAX; j++) {
			MAPP1->constellation[k][j] =
				(unsigned char)((V90_CONSTELLATION_MAX - 1 - j)
						& 0x7f);
			MAPP2->constellation[k][j] =
				(unsigned char)((V90_CONSTELLATION_MAX - 1 - j)
						& 0x7f);
		}
	}
	/* Both arms of `resetNoSpectral`'s per-phase test. */
	for (k = 0; k < V90ADID_PHASES; k++) {
		int j;

		ADI->short_2800[k] = (short)(k & 1);
		for (j = 0; j < V90ADID_CODES; j++) {
			ADI->linMapp[k][j] = (short)(4000 - 20 * j);
			ADI->linMappAlt[k][j] = (short)(3990 - 20 * j);
		}
	}

	/* Both settings of the gate the WaitForEd arm reads. */
	CEV->word_90 = (mode & 2) ? 1u : 0u;

	fill(p4d_s[0], P4D_SLOT, lf);
	memcpy(p4d_s[1], p4d_s[0], P4D_SLOT);
	memcpy(p4d_seed, p4d_s[0], P4D_SLOT);

	for (s = 0; s < 2; s++) {
		V90Phase4Demodulator *d = &P4D(s);
		V90Demapper *m = &DEM(s);
		V90Descrambler *x = &DSC(s);

		fill(dem_s[s], DEM_SLOT, lf ^ 0x77u);
		fill(cp_s[s], CP_SLOT, lf ^ 0x31u);
		fill(mp_s[s], MP_SLOT, lf ^ 0x9bu);
		fill((unsigned char *)code_s[s], (unsigned)sizeof code_s[s],
		     lf ^ 0xa5u);
		fill(sign_s[s], NSAMPLE, lf ^ 0xc3u);
		fill(sbstate_s[s], V90SBE_DECODER_SIZE, lf ^ 0x5eu);
		fill((unsigned char *)cpbuf_s[s], (unsigned)sizeof cpbuf_s[s],
		     lf ^ 0x2du);
		/*
		 * TRANSPARENT ON HALF THE TRIALS.  `Descrambler::process`
		 * answers `in ^ *pTap1 ^ *pTap2`, so a varied buffer turns
		 * every zero bit the demapper produces into a non-zero one and
		 * NEITHER message decoder ever answers -- which is fourteen
		 * mutations reading NOT CAUGHT.  A constant buffer makes the
		 * two taps cancel and the demapper's own bits reach `V90MP`
		 * and `V90CP`; the varied half keeps the descrambler's own
		 * state in the comparison.  Finding 4811.
		 */
		if (mode & 1)
			fill(dscbuf_s[s], DSC_N, lf ^ 0x13u);
		else
			memset(dscbuf_s[s], 0x55, DSC_N);

		d->params = PARAMS;
		d->mappingParams1 = MAPP1;
		d->mappingParams2 = MAPP2;
		d->connectionEvaluator = CEV;
		d->demapper = m;
		d->descrambler = x;
		d->cp = &CPR(s);
		d->mp = &MPR(s);

		/*
		 * The demapper, armed so that `hardDecision` has room to
		 * append and `process` yields exactly one frame of bits.  The
		 * moduli and the shape are t_v90demap's; what matters here is
		 * that `bitsPerFrame` is non-zero, so the bit loops run.
		 */
		m->params = PARAMS;
		m->adiDetector = ADI;
		m->codes = code_s[s];
		m->signs = sign_s[s];
		m->sampleCapacity = NSAMPLE;
		m->sampleCount = 5u;
		m->frameStart = 0u;
		m->bitsPerFrame = 8u;
		m->signBitsPerFrame = 2u;
		m->signBitGroups = 0u;
		m->signBitGroupSize = 1u;
		m->rbsFramePosition = 2u;
		/*
		 * NOT 3.  The RtNot arm calls `incrementRBSFramePosition`
		 * before it prints, which takes the cursor from 2 to 3, so a
		 * `word_08` of 3 makes "prints the cursor" and "prints the
		 * phase count" produce the same line and the mutation that
		 * swaps them survives.  `resetNoSpectral` also computes
		 * `word_08` as `word_0 - signBitsPerFrame`, which is 1 or 3,
		 * so 5 is clear of both.  Finding 4811.
		 */
		m->word_08 = 5u;
		m->linearMappStudyEnabled = (short)((mode & 1) ? 1 : 0);
		m->modulusDecoder.field_00 = 7u;
		m->modulusDecoder.field_04 = 11u;
		m->modulusDecoder.field_08 = 5u;
		m->modulusDecoder.field_0c = 13u;
		m->modulusDecoder.field_10 = 3u;
		m->modulusDecoder.field_14 = 0u;
		m->modulusDecoder.field_18 = 6u;
		m->signBits.spacing = 1u;
		m->signBits.width = 1u;
		m->signBits.state = 0u;
		m->signBits.decoder.state_ = sbstate_s[s];
		m->signBits.decoder.capacity_ = V90SBE_DECODER_SIZE;
		m->signBits.decoder.size_ = 0u;
		m->signBits.oddDecoder.prev_ = 0;
		m->signDecoder.prev_ = 0;
		m->errorHistogramCount = 0u;
		m->histogramDelay = 0;
		m->histogramIntegration = 0;

		/*
		 * A DESCENDING CONSTELLATION PER ROW, which is the order
		 * `hardDecision`'s search assumes.
		 */
		for (k = 0; k < V90DEMAPPER_CONSTELLATIONS; k++) {
			int j;

			m->constellationSize[k] = V90DEMAPPER_LEVELS;
			for (j = 0; j < V90DEMAPPER_LEVELS; j++)
				m->constellation[k][j] =
					(short)(8000 - j * 60 - k * 7);
		}

		/*
		 * THE TWO DECODERS ARE PLANTED ONE BIT FROM AN ANSWER, and
		 * without this neither bit loop ever produces one.  Both
		 * answer on a run of zeros while the cursor is still at its
		 * home 18 -- `byte_1a == 2 * word_114` for the MP record and
		 * `byte_caa == 2 * word_3ba8` for the CP one -- so a group
		 * size of 1 and a run already at 1 turns the first zero bit
		 * `Descrambler` hands over into `Ed detected` (3) for V.90 and
		 * into answer 5 for V.92.  State 0 is the preamble and leaves
		 * the cursor alone, so the condition survives the other seven
		 * bits of the frame.
		 */
		MPR(s).word_14 = 0u;
		MPR(s).byte_19 = 0;
		MPR(s).byte_1a = 1;
		MPR(s).byte_1b = 18;
		MPR(s).word_114 = 1u;
		CPR(s).word_ca4 = 0u;
		CPR(s).byte_ca9 = 0;
		CPR(s).byte_caa = 1;
		CPR(s).word_cac = 18u;
		CPR(s).word_cb0 = 0u;
		CPR(s).word_3ba8 = 1u;

		/* The CP record's six buffers, one array per side. */
		for (k = 0; k < V90CP_BUFS; k++) {
			CPR(s).buf[k] = cpbuf_s[s][k];
			CPR(s).nof_buf[k] = 1u;
		}

		/* The descrambler, laid out exactly as its constructor does. */
		x->pLimit = dscbuf_s[s];
		x->pInitOut = dscbuf_s[s] + DSC_C;
		x->pInitTap1 = dscbuf_s[s] + DSC_C + DSC_A;
		x->pInitTap2 = dscbuf_s[s] + DSC_C + DSC_B;
		x->tailLength = DSC_B;
		x->pOut = x->pInitOut;
		x->pTap1 = x->pInitTap1;
		x->pTap2 = x->pInitTap2;
	}
}

/*
 * The four-way comparison.  `n` bounds the demodulator itself; the 64 bytes
 * past it are the guard.
 */
static void
compare_all(const char *what, long tag)
{
	unsigned n = (unsigned)sizeof(V90Phase4Demodulator);
	int s;

	scrub(p4d_cmp[0], p4d_s[0], P4D_SLOT, p4d_skip);
	scrub(p4d_cmp[1], p4d_s[1], P4D_SLOT, p4d_skip);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase4Demodulator",
		     p4d_cmp[0], p4d_cmp[1], n, tag);
	diff_eq_int("nothing stored past the demodulator (%ld)",
		    memcmp(p4d_cmp[0] + n, p4d_cmp[1] + n, P4D_SLOT - n) == 0,
		    1, tag);
	for (s = 0; s < 2; s++)
		diff_eq_int("the guard past the demodulator held (%ld)",
			    memcmp(p4d_s[s] + n, p4d_seed + n,
				   P4D_SLOT - n) == 0, 1, tag);

	scrub(dem_cmp[0], dem_s[0], DEM_SLOT, dem_skip);
	scrub(dem_cmp[1], dem_s[1], DEM_SLOT, dem_skip);
	diff_eq_obj_(__FILE__, __LINE__, "the demapper", "V90Demapper",
		     dem_cmp[0], dem_cmp[1], (unsigned)sizeof(V90Demapper),
		     tag);
	diff_eq_int("the demapper's codes (%ld)",
		    memcmp(code_s[0], code_s[1], sizeof code_s[0]) == 0, 1,
		    tag);
	diff_eq_int("the demapper's signs (%ld)",
		    memcmp(sign_s[0], sign_s[1], sizeof sign_s[0]) == 0, 1,
		    tag);
	diff_eq_int("the sign decoder's state (%ld)",
		    memcmp(sbstate_s[0], sbstate_s[1],
			   sizeof sbstate_s[0]) == 0, 1, tag);

	scrub(cp_cmp[0], cp_s[0], CP_SLOT, p4d_skip);	/* unused offsets */
	diff_eq_int("the CP record (%ld)",
		    memcmp((unsigned char *)&CPR(0) + 0,
			   (unsigned char *)&CPR(1) + 0,
			   __builtin_offsetof(V90CP, buf)) == 0, 1, tag);
	diff_eq_int("the CP record past its buffers (%ld)",
		    memcmp((unsigned char *)&CPR(0) +
			   __builtin_offsetof(V90CP, word_ca0),
			   (unsigned char *)&CPR(1) +
			   __builtin_offsetof(V90CP, word_ca0),
			   CP_SLOT - __builtin_offsetof(V90CP, word_ca0)) == 0,
		    1, tag);
	diff_eq_int("the CP record's buffers (%ld)",
		    memcmp(cpbuf_s[0], cpbuf_s[1], sizeof cpbuf_s[0]) == 0, 1,
		    tag);

	diff_eq_int("the MP record (%ld)",
		    memcmp(mp_s[0], mp_s[1], MP_SLOT) == 0, 1, tag);

	diff_eq_int("the descrambler's buffer (%ld)",
		    memcmp(dscbuf_s[0], dscbuf_s[1], DSC_N) == 0, 1, tag);
	diff_eq_int("the descrambler's output cursor (%ld)",
		    (long)(DSC(0).pOut - dscbuf_s[0]),
		    (long)(DSC(1).pOut - dscbuf_s[1]), tag);
	diff_eq_int("the descrambler's near tap (%ld)",
		    (long)(DSC(0).pTap1 - dscbuf_s[0]),
		    (long)(DSC(1).pTap1 - dscbuf_s[1]), tag);
	diff_eq_int("the descrambler's far tap (%ld)",
		    (long)(DSC(0).pTap2 - dscbuf_s[0]),
		    (long)(DSC(1).pTap2 - dscbuf_s[1]), tag);
}

/*
 * Put `rDetector1` one sample from an R decision, and the same for
 * `rDetector2` and `detectRf`.  Both are t_v90p4dleaf's, unchanged: reaching
 * these arms through the front door costs six or twelve calls and would land
 * on whatever the run counters happened to hold.
 */
static short
arm_r(V90RDetector *d, int want, int limit)
{
	d->int_04 = limit;
	d->int_08 = limit;
	d->int_1c = 0;
	d->int_14 = 0;
	d->int_18 = 0;
	d->int_24 = 0x33;

	switch (want) {
	/*
	 * `detectRNot` IS A DIFFERENT ARMING and this is what the first run of
	 * this suite got wrong: it counts down +0x08 and matches 0x07 rather
	 * than counting +0x04 and matching 0x38, so a fixture armed for
	 * `detectR` leaves every RNot arm in both functions unreached and
	 * eleven mutations reading NOT CAUGHT.  Finding 4811.
	 */
	case 2:
		d->int_00 = 5;
		d->ushort_20 = 0x03;		/* 0x03 * 2 | 1 = 0x07 */
		d->int_08 = limit;
		d->int_1c = limit - 6;
		return 300;
	case 1:
		d->int_00 = 5;
		d->ushort_20 = 0x1c;
		d->int_14 = limit - 6;
		return -300;
	case -1:
		d->int_00 = 5;
		d->ushort_20 = 0x03;
		d->int_18 = limit - 6;
		return 300;
	default:
		d->int_00 = 2;
		d->ushort_20 = 0x1c;
		return 300;
	}
}

static short
arm_rf(V90RDetector *d, int want, int limit)
{
	d->int_0c = limit;
	d->int_10 = limit;
	d->int_1c = 0;
	d->int_14 = 0;
	d->int_18 = 0;
	d->int_24 = 0x44;

	switch (want) {
	/* And `detectRfNot`: twelve to the group, +0x10, and 0x333. */
	case 2:
		d->int_00 = 11;
		d->ushort_20 = 0x199;		/* 0x199 * 2 | 1 = 0x333 */
		d->int_10 = limit;
		d->int_1c = limit - 12;
		return 300;
	case 1:
		d->int_00 = 11;
		d->ushort_20 = 0x666;
		d->int_14 = limit - 12;
		return -300;
	case -1:
		d->int_00 = 11;
		d->ushort_20 = 0x199;
		d->int_18 = limit - 12;
		return 300;
	default:
		d->int_00 = 4;
		d->ushort_20 = 0x666;
		return 300;
	}
}

/*
 * The fifteen states whose arm assigns the returned decision, per function.
 * Index by state; V.90 leaves 4 and 0x10 out and V.92 leaves 5 and 6 out.
 * See the file comment.
 */
static const char v90_returns[0x12] = {
	1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1
};
static const char v92_returns[0x12] = {
	1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/* ------------------------------------------------------------- one trial */

/*
 * Drive one state on both sides and compare everything.  `arm` picks which
 * outcome the detectors are set up for and `phase` moves `countInState` on to
 * the interesting side of whichever threshold this state tests.
 */
struct outcome {
	short ret[2];
	int spoke;		/* the blob printed at least one line   */
	int moved;		/* the blob's object left its seed      */
	int changed;		/* the blob's state changed             */
	unsigned int poststate;
};

static void
run_one(int v92, int state, int arm, int phase, int trial, int mode,
	long tag, struct outcome *o)
{
	short sample = 300;
	int s;

	setup(trial, mode);

	for (s = 0; s < 2; s++) {
		V90Phase4Demodulator *d = &P4D(s);
		short v;

		d->state = (Phase4DemodulatorState)state;
		d->sessionFlag = v92 ? 1u : 0u;
		d->uint_0034 = (unsigned)(phase & 1);
		d->int_0038 = (phase & 1) ? 1 : 0;
		d->int_003c = (phase & 2) ? 1 : 0;
		d->int_0040 = (phase & 4) ? 1 : 0;
		d->int_0044 = (phase & 1) ? 1 : 0;
		d->int_0048 = (phase & 4) ? 1 : 0;
		d->uchar_0030 = (unsigned char)((phase & 2) ? 1 : 0);
		d->uint_004c = (unsigned)(phase & 1);
		d->trn2dDDLength = 0x40;
		d->linearMappStudyStart = 0x20;
		/*
		 * BOTH ENERGIES STAY POSITIVE HERE, on purpose: a negative one
		 * makes the dB ratio negative, `fyl2x` answers a NaN, and the
		 * keep-rate flag then depends on an UNORDERED compare that the
		 * modern build cannot reproduce.  That case has its own group
		 * below and its own entry in `tools/gccdiverge.json`, so that
		 * the 32,000 checks in this sweep stay green under both
		 * compilers and only the one check that provably cannot be is
		 * excused.  Finding 4812.
		 */
		d->errorEnergyBeforeEC = 4.0f;
		d->errorEnergyAfterEC = 1.0f;
		d->b1dBits = 0x30;
		d->b1dZeros = 7;
		d->int_3510 = 0;
		d->nbits = 0;

		/*
		 * `countInState` is incremented before the switch, so every
		 * threshold below is one short of the value the arm sees.
		 */
		/*
		 * EVERY THRESHOLD IS APPROACHED FROM BOTH SIDES AND FROM
		 * BETWEEN.  A silence state that is only ever driven well past
		 * its threshold cannot tell `>= WAIT_BEFORE_ECHO_CALC` from
		 * `>= ECHO_CALC_PERIOD`, and a B1d delay only ever driven well
		 * past it cannot tell `+ 0x17` from `+ 0x18`.  So each list
		 * below straddles: one value below the real threshold, one
		 * between it and the nearest wrong one, and one above both.
		 * Finding 4811.
		 */
		switch (state) {
		case 2:
		case 3:
			d->countInState = (phase & 1) ? 0x1fu :
					  (phase & 2) ? 0x3fu :
					  (phase & 4) ? 0x3eu : 0x0fu;
			break;
		case 7:
			d->countInState = (phase & 1) ? 0x11fu : 0x40u;
			d->b1dBits = (phase & 2) ? 36u :
				     (phase & 4) ? 38u : 30u;
			break;
		case 0xa:
			/* 36 <= 42 < 48, so only the wrong threshold fires. */
			d->countInState = (phase & 1) ? 0x35u :
					  (phase & 2) ? 41u : 0x10u;
			break;
		case 0xb:
		case 0xd:
			d->countInState = (phase & 1) ? 0x29u :
					  (phase & 2) ? 35u : 0x10u;
			break;
		case 0xc:
			/* 180 <= 186 < 216, the same trick one period along. */
			d->countInState = (phase & 1) ? 0xddu :
					  (phase & 2) ? 185u : 0x10u;
			break;
		default:
			d->countInState = 0x10u;
			break;
		}

		v = (short)arm_r(&d->rDetector1, arm, 0x60);
		(void)arm_rf(&d->rDetector2, arm, 0x60);
		if (state == 0x10 && v92)
			sample = (short)arm_rf(&d->rDetector2, arm, 0x60);
		else
			sample = v;
	}

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;
	if (v92) {
		o->ret[0] = our_p4d_getv92decision(p4d_s[0], sample);
		o->ret[1] = ref_p4d_getv92decision(p4d_s[1], sample);
	} else {
		o->ret[0] = our_p4d_getv90decision(p4d_s[0], sample);
		o->ret[1] = ref_p4d_getv90decision(p4d_s[1], sample);
	}
	dsplib_debug_capture_on = 0;

	compare_all(v92 ? "getV92Decision" : "getV90Decision", tag);

	diff_eq_int("the transcripts agreed (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("the same number of lines (%ld)",
		    (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), tag);

	if ((v92 ? v92_returns : v90_returns)[state])
		diff_eq_int("the decision (%ld)", (long)o->ret[0],
			    (long)o->ret[1], tag);

	o->spoke = dsplib_debug_capture_lines(1) > 0;
	o->moved = memcmp(p4d_s[1], p4d_seed, sizeof(V90Phase4Demodulator))
		   != 0;
	o->poststate = (unsigned)P4D(1).state;
	o->changed = (int)o->poststate != state;
}

/* ==================================================== the two state sweeps */

static int
run_sweep(int v92)
{
	long trial = v92 ? 600000L : 500000L;
	int state, arm, phase, mode, lvl;
	int spoke = 0, moved = 0, changed = 0;
	unsigned seen = 0;
	int nseen = 0;
	int printed_at_2 = 0, printed_at_3 = 0;
	static const unsigned levels[3] = { 0u, 2u, 3u };

	diff_begin(v92 ? "V90Phase4Demodulator::getV92Decision"
			: "V90Phase4Demodulator::getV90Decision");

	for (lvl = 0; lvl < 3; lvl++) {
		set_level(levels[lvl]);
		for (state = 0; state < 0x12; state++)
			for (arm = -1; arm <= 2; arm++)
				for (phase = 0; phase < 8; phase++) {
					struct outcome o;

					/*
					 * NOT `trial % 4`: the phase loop is
					 * eight long and four divides eight, so
					 * that made `mode` constant per phase
					 * across every state, arm and level --
					 * half the seeds never met half the
					 * thresholds.  Finding 4811.
					 */
					mode = (phase + arm + 1 + state) & 3;
					/*
					 * THE SEED IS LEVEL-INDEPENDENT, so
					 * the three levels drive the SAME
					 * inputs and the two level counters
					 * below compare like with like.  With
					 * `trial` in the seed they did not,
					 * and "level 3 printed at least as
					 * much as level 2" was measuring the
					 * seed rather than the gate.
					 */
					run_one(v92, state, arm, phase,
						state * 100 + (arm + 1) * 10 +
						phase, mode, trial, &o);
					trial++;

					spoke += o.spoke;
					moved += o.moved;
					changed += o.changed;
					if (o.poststate < 32u &&
					    !(seen >> o.poststate & 1u)) {
						seen |= 1u << o.poststate;
						nseen++;
					}
					if (o.spoke && levels[lvl] == 2u)
						printed_at_2++;
					if (o.spoke && levels[lvl] == 3u)
						printed_at_3++;
				}
	}

	/*
	 * ONE OUT-OF-RANGE STATE, which is the `ja` at the top.  The object
	 * must be left exactly as it was apart from the two words the preamble
	 * always writes, so this is checked against the SEED and not only
	 * against the other side.
	 */
	{
		struct outcome o;
		int s;

		set_level(2);
		setup((int)trial, 0);
		for (s = 0; s < 2; s++) {
			P4D(s).state = (Phase4DemodulatorState)0x40;
			P4D(s).sessionFlag = v92 ? 1u : 0u;
			P4D(s).countInState = 9u;
			P4D(s).int_0028 = 0x55;
		}
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		if (v92) {
			o.ret[0] = our_p4d_getv92decision(p4d_s[0], 111);
			o.ret[1] = ref_p4d_getv92decision(p4d_s[1], 111);
		} else {
			o.ret[0] = our_p4d_getv90decision(p4d_s[0], 111);
			o.ret[1] = ref_p4d_getv90decision(p4d_s[1], 111);
		}
		dsplib_debug_capture_on = 0;
		compare_all("out of range", trial);
		diff_eq_int("out of range printed nothing (%ld)",
			    (int)dsplib_debug_capture_lines(1), 0, trial);
		diff_eq_int("out of range counted the sample (%ld)",
			    (long)P4D(1).countInState, 10, trial);
		diff_eq_int("out of range cleared +0x28 (%ld)",
			    (long)P4D(1).int_0028, 0, trial);
		diff_eq_int("out of range left the state (%ld)",
			    (long)P4D(1).state, 0x40, trial);
		trial++;
	}

	/*
	 * BLOB AGAINST BLOB: the linear mapping study gate reads +0x3518.
	 * Two runs of state `TRN2D_DD` that differ in nothing but
	 * `linearMappStudyStart`, one of which matches `countInState` and one
	 * of which does not, must produce DIFFERENT transcripts.  Nothing in
	 * the run above can fail on a gate that ignored the field.
	 */
	{
		char first[512];
		unsigned n;

		set_level(2);
		setup((int)trial, 0);
		P4D(1).state = P4D_STATE_TRN2D_DD;
		P4D(1).countInState = 0x1f;
		P4D(1).linearMappStudyStart = 0x20;
		P4D(1).trn2dDDLength = 0x400;
		DEM(1).linearMappStudyEnabled = 0;
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		if (v92)
			(void)ref_p4d_getv92decision(p4d_s[1], 300);
		else
			(void)ref_p4d_getv90decision(p4d_s[1], 300);
		dsplib_debug_capture_on = 0;
		n = (unsigned)strlen(dsplib_debug_capture_text(1));
		if (n >= sizeof first)
			n = (unsigned)sizeof first - 1u;
		memcpy(first, dsplib_debug_capture_text(1), n);
		first[n] = '\0';
		diff_eq_int("the study was enabled (%ld)",
			    (long)DEM(1).linearMappStudyEnabled, 1, trial);

		setup((int)trial, 0);
		P4D(1).state = P4D_STATE_TRN2D_DD;
		P4D(1).countInState = 0x1f;
		P4D(1).linearMappStudyStart = 0x900;
		P4D(1).trn2dDDLength = 0x400;
		DEM(1).linearMappStudyEnabled = 0;
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		if (v92)
			(void)ref_p4d_getv92decision(p4d_s[1], 300);
		else
			(void)ref_p4d_getv90decision(p4d_s[1], 300);
		dsplib_debug_capture_on = 0;
		diff_eq_int("the study stayed off (%ld)",
			    (long)DEM(1).linearMappStudyEnabled, 0, trial);
		diff_eq_int("the study gate reads +0x3518 (%ld)",
			    strcmp(first, dsplib_debug_capture_text(1)) != 0,
			    1, trial);
		trial++;
	}

	set_level(0);

	/*
	 * The anti-vacuity floor.  Every one of these is an OBSERVABLE thing
	 * the blob did, counted over the sweep above (finding 3509).
	 */
	diff_eq_int("some trial printed", spoke > 0, 1, 0);
	diff_eq_int("some trial changed the object", moved > 0, 1, 0);
	diff_eq_int("some trial changed the state", changed > 0, 1, 0);
	diff_eq_int("at least eight distinct states came out", nseen >= 8, 1,
		    0);
	diff_eq_int("level 2 printed", printed_at_2 > 0, 1, 0);
	diff_eq_int("level 3 printed at least as much as level 2",
		    printed_at_3 >= printed_at_2, 1, 0);

	return diff_end();
}

/* ================================================== getDecision, the switch */

/*
 * `getDecision` is 52 bytes and one test.  What has to be shown is not that it
 * returns the right number -- both arms return the same thing on most states
 * -- but that +0x00 SELECTS, so the test drives a state where the two arms
 * differ and requires the two OBJECTS to differ with it.
 *
 * V.90's state 5 is the MP arm and V.92's does nothing at all, so one call
 * each with the same seed and the same sample separates them.  That
 * comparison is blob against blob.
 */
static int
run_getdecision(void)
{
	long trial = 700000L;
	int state, flag;
	int agreed = 0;

	diff_begin("V90Phase4Demodulator::getDecision");
	set_level(2);

	for (state = 0; state < 0x12; state++)
		for (flag = 0; flag < 2; flag++) {
			int rc[2];
			int s;

			setup((int)trial, state & 3);
			for (s = 0; s < 2; s++) {
				P4D(s).state = (Phase4DemodulatorState)state;
				P4D(s).sessionFlag = flag ? 0x5au : 0u;
				P4D(s).countInState = 0x10u;
				P4D(s).trn2dDDLength = 0x40;
				P4D(s).linearMappStudyStart = 0x20;
				P4D(s).errorEnergyBeforeEC = 4.0f;
				P4D(s).errorEnergyAfterEC = 1.0f;
				P4D(s).b1dBits = 0x30;
				P4D(s).b1dZeros = 7;
				P4D(s).int_003c = 1;
				(void)arm_r(&P4D(s).rDetector1, 1, 0x60);
				(void)arm_rf(&P4D(s).rDetector2, 1, 0x60);
			}

			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			rc[0] = our_p4d_getdecision(p4d_s[0], -300);
			rc[1] = ref_p4d_getdecision(p4d_s[1], -300);
			dsplib_debug_capture_on = 0;

			compare_all("getDecision", trial);
			diff_eq_int("the transcripts agreed (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, trial);
			/*
			 * `getDecision` widens whichever arm ran, and both
			 * arms define the decision on every state reached
			 * here except V.90's 4 and 0x10 and V.92's 5 and 6.
			 */
			if (flag ? v92_returns[state] : v90_returns[state])
				diff_eq_int("the decision (%ld)", (long)rc[0],
					    (long)rc[1], trial);
			agreed++;
			trial++;
		}

	/*
	 * BLOB AGAINST BLOB: +0x00 selects.  State 5 is V.90's WaitForMP arm,
	 * which runs the demapper and the MP decoder, and V.92's exit, which
	 * touches nothing but the two preamble words.
	 */
	{
		unsigned char after_v90[P4D_SLOT];
		int s;

		setup((int)trial, 0);
		for (s = 0; s < 2; s++) {
			P4D(s).state = P4D_STATE_WAIT_FOR_MP;
			P4D(s).countInState = 0x10u;
		}
		P4D(1).sessionFlag = 0u;
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		(void)ref_p4d_getdecision(p4d_s[1], -300);
		dsplib_debug_capture_on = 0;
		memcpy(after_v90, p4d_s[1], P4D_SLOT);

		setup((int)trial, 0);
		P4D(1).state = P4D_STATE_WAIT_FOR_MP;
		P4D(1).countInState = 0x10u;
		P4D(1).sessionFlag = 1u;
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		(void)ref_p4d_getdecision(p4d_s[1], -300);
		dsplib_debug_capture_on = 0;

		/*
		 * `sessionFlag` itself differs, so the comparison starts past
		 * it.  What must move is the demapper's cursor, which only the
		 * V.90 arm advances.
		 */
		diff_eq_int("sessionFlag selects the arm (%ld)",
			    memcmp(after_v90 + 4, p4d_s[1] + 4,
				   sizeof(V90Phase4Demodulator) - 4) != 0, 1,
			    trial);
		trial++;
	}

	set_level(0);
	diff_eq_int("both settings were driven over every state",
		    agreed, 0x12 * 2, 0);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_sweep(0);
	rc |= run_sweep(1);
	rc |= run_getdecision();

	return rc;
}
