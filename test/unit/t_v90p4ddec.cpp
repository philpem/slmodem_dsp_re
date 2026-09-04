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
 * THE OBJECTS ARE NEVER ZEROED -- finding F230.  Every slot gets varied
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
 * (finding F1105): `params`, the two `V90MappingParams` blocks, the connection
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
 * ANTI-VACUITY, per finding F3509 rather than per path
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
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"
/*
 * `V90Demodulator::exitPhase3` is the last section of this file, and these
 * three are what it needs beyond what the phase 4 receiver already brings.
 * V90Demodulator.h supplies V90Equalizer, V90Phase3Demodulator, V90Resampler,
 * V90SpectralVerifier, V90Phase2Info, V90Jd and V92Jd with it; the paragraph
 * that header carries about CLAIMING `DSPLIB_V90PARAMETERS_H` is history and
 * not a live restriction -- V90PreFilter.h INCLUDES the named map since task
 * #116, this file uses `PARAMS->TRN2D_DD_LENGTH` and friends throughout, and
 * the two coexist.
 */
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90TRN2Designer.h"
#include "dsplib/tagV90AdditionalCPinfo.h"

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

/* Varied, never zero, never the same twice: findings F223, F224, F230. */
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
		 * state in the comparison.  Finding F4811.
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
		 * so 5 is clear of both.  Finding F4811.
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
static void compare_peers(long tag);

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

	compare_peers(tag);
}

/*
 * The four split peers and the two per-side arrays under the demapper, split
 * out of `compare_all` so that `run_p4d_reset` can make the same claims about
 * them under a DIFFERENT comparison of the demodulator itself -- that one has
 * to canonicalise the embedded modulator's two heap words, which no other
 * test in this file constructs.
 */
static void
compare_peers(long tag)
{
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
	 * eleven mutations reading NOT CAUGHT.  Finding F4811.
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
		d->quickConnect = (unsigned)(phase & 1);
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
		 * excused.  Finding F4812.
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
		 * Finding F4811.
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
					 * thresholds.  Finding F4811.
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
	 * the blob did, counted over the sweep above (finding F3509).
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

/* ------------------------------------- V90Phase4Demodulator::reset -------
 *
 * 504 bytes at .text+0x277c0, and it is this class's entry point: eleven
 * scalars, both `V90RDetector`s, one of the CP and the MP, the demapper, the
 * embedded modulator's own `reset` and `setMappingParams`, two diagnostics
 * behind two different gates, and then a loop that runs the decision member
 * `sessionFlag` selects `nofSamples` times.
 *
 * IT LIVES HERE BECAUSE THE LOOP DOES.  Every other home for it -- a fixture
 * of its own, or t_v90rxctor, which already constructs this class -- would
 * have had to leave `nofSamples` at zero, because driving the loop means
 * driving `getV90Decision` and `getV92Decision` for real over a demapper, a
 * descrambler, a CP and an MP that are all in a usable state.  This file is
 * where all four of those are planted, and the two decision members are the
 * two things `reset` selects between, so a `nofSamples` of zero would leave
 * "the two arms are swapped" with no witness at all.
 *
 * WHAT THIS FILE'S `setup` DOES NOT PROVIDE, AND WHY IT IS CONSTRUCTED AND
 * NOT PLANTED.  `reset` reaches two things the decision members never do:
 * `Scrambler<h,h>::reset(0)` on the modulator embedded at +0x50, whose
 * history is a HEAP allocation the modulator's constructor makes, and
 * `V90BitsToSymbol::reset` through `setMappingParams`, which runs a whole
 * `V90Mapper` underneath it.  Planting that chain by hand is a longer and
 * more fragile thing than running the two constructors that build it, both of
 * which are already differentially green in t_v90modchain -- so the converter
 * and the embedded modulator are CONSTRUCTED, per side, over the seeded
 * block, with the arguments V90Phase4Demodulator's own constructor uses and
 * the two mapping blocks in the order it crosses them (finding F1301).  The
 * one deliberate difference is that the converter is SUPPLIED rather than
 * left for the modulator to allocate: the ownership flag is not this
 * function's claim and a supplied converter is one allocation fewer to mask.
 *
 * THE HISTORY IS DIRTIED AFTER CONSTRUCTION AND BEFORE THE CALL, which is
 * finding F7457: the allocator hands back zeroed memory and `reset(0)` writes
 * zeros, so without a non-zero pattern the call moves nothing any comparison
 * can see, and both "drop the call" and "seed with one" survive.
 *
 * FOUR AXES, INDEPENDENT BY CONSTRUCTION, per finding F7458:
 *
 *   - `sessionFlag`, which picks the CP arm over the MP arm AND the decision
 *     member the loop runs;
 *   - `quickConnect`, which picks `TRN2D_QC_DD_LENGTH` over `TRN2D_DD_LENGTH`
 *     -- two parameters planted to two different values, without which that
 *     choice has no observable at all;
 *   - `nofSamples`, 0, 1 or 2;
 *   - the debug level, 0, 1 and 2, because the `quickConnect` line is behind
 *     `> 1` and `edprintf` is behind `> 0`, and those differ at exactly one
 *     value.
 *
 * They are four nested loops and share no bit with one another or with the
 * state, which is the sweep's outermost axis.
 *
 * `PHASE4_R_DETECTION_LENGTH` IS PLANTED AWAY FROM 0x18 and its neighbours
 * are planted to different values again, so "the two detector arguments are
 * swapped" and "it reads +0x298" both have somewhere to land.
 */

extern "C" {
void ref_p4d_reset(void *, unsigned char, Phase4DemodulatorState,
		   unsigned int, unsigned int)
	asm("ref__ZN20V90Phase4Demodulator5resetEh22Phase4DemodulatorStatejj");

void our_bts_c1(void *, unsigned int, V90Parameters *)
	asm("_ZN15V90BitsToSymbolC1EjP13V90Parameters");
void ref_bts_c1(void *, unsigned int, V90Parameters *)
	asm("ref__ZN15V90BitsToSymbolC1EjP13V90Parameters");
void our_bts_d1(void *) asm("_ZN15V90BitsToSymbolD1Ev");
void ref_bts_d1(void *) asm("ref__ZN15V90BitsToSymbolD1Ev");

#define RD_P4M_MANGLE(v) \
	"_ZN18V90Phase4ModulatorC" #v "EP13V90ParametersjP15V90BitsToSymbol" \
	"P5V90MPP16V90MappingParamsS7_P5V90CPj"

void our_p4m_c1(void *, V90Parameters *, unsigned int, V90BitsToSymbol *,
		V90MP *, V90MappingParams *, V90MappingParams *, V90CP *,
		unsigned int) asm(RD_P4M_MANGLE(1));
void ref_p4m_c1(void *, V90Parameters *, unsigned int, V90BitsToSymbol *,
		V90MP *, V90MappingParams *, V90MappingParams *, V90CP *,
		unsigned int) asm("ref_" RD_P4M_MANGLE(1));
void our_p4m_d1(void *) asm("_ZN18V90Phase4ModulatorD1Ev");
void ref_p4m_d1(void *) asm("ref__ZN18V90Phase4ModulatorD1Ev");
}

#define RD_BTS_SIZE	0x24u
#define RD_MAP_SIZE	0x704u
#define RD_BTS_N	0x40u			/* symbols the converter owns */

/* The embedded modulator, and the two words of it that hold allocations. */
#define RD_P4M_BASE	0x0050u
#define RD_P4M_BTS	(RD_P4M_BASE + 0x0044u)		/* the converter   */
#define RD_P4M_SCRAM	(RD_P4M_BASE + 0x0058u)		/* the scrambler   */

/* The V90SpectralShaper embedded in V90Mapper: +0x68c, 0x6c bytes. */
#define RD_SHAPER_LO	0x68cu
#define RD_SHAPER_HI	0x6f8u

static unsigned char bts_s[2][RD_BTS_SIZE] __attribute__((aligned(8)));
static unsigned char rd_cmp[2][P4D_SLOT];
static unsigned char rd_map[2][RD_MAP_SIZE];

/* The live allocation set, for finding a heap pointer by value. */
#define RD_MAXLIVE	64
static void *rd_live[RD_MAXLIVE];
static int rd_nlive;

static int
rd_word_is_live(const unsigned char *w)
{
	void *p;
	int i;

	memcpy(&p, w, sizeof(p));
	if (p == 0)
		return 0;
	for (i = 0; i < rd_nlive; i++)
		if (rd_live[i] == p)
			return 1;
	return 0;
}

static void
rd_mask_word(unsigned char *a, unsigned char *b, unsigned off)
{
	memset(a + off, 0x5a, sizeof(void *));
	memset(b + off, 0x5a, sizeof(void *));
}

static void
rd_mask_live(unsigned char *a, unsigned char *b, unsigned lo, unsigned hi)
{
	unsigned o;

	for (o = lo; o + sizeof(void *) <= hi; o += sizeof(void *))
		if (rd_word_is_live(a + o) && rd_word_is_live(b + o))
			rd_mask_word(a, b, o);
}

/*
 * An embedded Scrambler, WITHOUT losing what is in it: the six derived
 * pointers become distances from `pLimit` and only the base is masked, so a
 * tap at the wrong distance still differs.  t_v90modchain's `canon_scrambler`
 * with the same argument and for the same reason.
 */
static void
rd_canon_scrambler(unsigned char *o, unsigned base)
{
	unsigned int lim, v, i;

	memcpy(&lim, o + base, sizeof(lim));
	for (i = 1; i < 7; i++) {
		memcpy(&v, o + base + 4 * i, sizeof(v));
		v -= lim;
		memcpy(o + base + 4 * i, &v, sizeof(v));
	}
	memset(o + base, 0x5a, sizeof(void *));
}

static void *
rd_slot_ptr(const unsigned char *o, unsigned off)
{
	void *p;

	memcpy(&p, o + off, sizeof(p));
	return p;
}

/*
 * Build the converter and the embedded modulator, per side, over the block
 * `setup` has just seeded, and then dirty the scrambler's history.
 */
static void
rd_construct(int flag)
{
	int s;

	harness_alloc_reset();
	our_bts_c1(bts_s[0], RD_BTS_N, PARAMS);
	ref_bts_c1(bts_s[1], RD_BTS_N, PARAMS);

	/*
	 * The demodulator's own constructor's arguments for the embedded
	 * modulator, mapping blocks crossed (1301), with the converter
	 * supplied rather than owned.
	 */
	our_p4m_c1(p4d_s[0] + RD_P4M_BASE, PARAMS, (unsigned int)flag,
		   (V90BitsToSymbol *)(void *)bts_s[0], (V90MP *)0, MAPP2,
		   MAPP1, (V90CP *)0, 0xcu);
	ref_p4m_c1(p4d_s[1] + RD_P4M_BASE, PARAMS, (unsigned int)flag,
		   (V90BitsToSymbol *)(void *)bts_s[1], (V90MP *)0, MAPP2,
		   MAPP1, (V90CP *)0, 0xcu);

	for (s = 0; s < 2; s++) {
		unsigned char *o = p4d_s[s] + RD_P4M_SCRAM;
		unsigned char *lim = (unsigned char *)rd_slot_ptr(o, 0);
		unsigned char *t2 = (unsigned char *)rd_slot_ptr(o, 0x0c);

		memset(lim, 0xa5, (size_t)(t2 - lim) + 1);
	}
}

static void
rd_destruct(void)
{
	our_p4m_d1(p4d_s[0] + RD_P4M_BASE);
	ref_p4m_d1(p4d_s[1] + RD_P4M_BASE);
	our_bts_d1(bts_s[0]);
	ref_bts_d1(bts_s[1]);
}

/*
 * The demodulator, its four split peers (through this file's own
 * `compare_all`), the converter and the mapper under it.
 */
static void
rd_compare(const char *what, long tag)
{
	unsigned n = (unsigned)sizeof(V90Phase4Demodulator);
	int s;

	rd_nlive = harness_alloc_live_set(rd_live, RD_MAXLIVE);
	if (rd_nlive > RD_MAXLIVE)
		rd_nlive = RD_MAXLIVE;

	scrub(rd_cmp[0], p4d_s[0], P4D_SLOT, p4d_skip);
	scrub(rd_cmp[1], p4d_s[1], P4D_SLOT, p4d_skip);
	rd_mask_word(rd_cmp[0], rd_cmp[1], RD_P4M_BTS);
	rd_canon_scrambler(rd_cmp[0], RD_P4M_SCRAM);
	rd_canon_scrambler(rd_cmp[1], RD_P4M_SCRAM);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase4Demodulator",
		     rd_cmp[0], rd_cmp[1], n, tag);
	for (s = 0; s < 2; s++)
		diff_eq_int("the guard past the demodulator held (%ld)",
			    memcmp(p4d_s[s] + n, p4d_seed + n,
				   P4D_SLOT - n) == 0, 1, tag);

	/*
	 * THE CONVERTER IS WHERE `setMappingParams` LANDS, and it is outside
	 * the object: `bitsPerFrame` is `mappingParams1->word_0`, so passing
	 * the other block -- which this file plants at a different value --
	 * fails here and nowhere else.
	 */
	memcpy(rd_cmp[0], bts_s[0], RD_BTS_SIZE);
	memcpy(rd_cmp[1], bts_s[1], RD_BTS_SIZE);
	rd_mask_word(rd_cmp[0], rd_cmp[1], 0x00);
	rd_mask_word(rd_cmp[0], rd_cmp[1], 0x08);
	diff_eq_obj_(__FILE__, __LINE__, "the converter", "V90BitsToSymbol",
		     rd_cmp[0], rd_cmp[1], (size_t)RD_BTS_SIZE, tag);

	memcpy(rd_map[0], rd_slot_ptr(bts_s[0], 0x00), RD_MAP_SIZE);
	memcpy(rd_map[1], rd_slot_ptr(bts_s[1], 0x00), RD_MAP_SIZE);
	rd_mask_word(rd_map[0], rd_map[1], 0x018);
	rd_mask_live(rd_map[0], rd_map[1], RD_SHAPER_LO, RD_SHAPER_HI);
	diff_eq_obj_(__FILE__, __LINE__, "the mapper under the converter",
		     "V90Mapper", rd_map[0], rd_map[1], (size_t)RD_MAP_SIZE,
		     tag);

	/* The scrambler's history, which lives outside the object (7457). */
	{
		const unsigned char *sa = p4d_s[0] + RD_P4M_SCRAM;
		const unsigned char *sb = p4d_s[1] + RD_P4M_SCRAM;
		const unsigned char *la = (const unsigned char *)
					  rd_slot_ptr(sa, 0);
		const unsigned char *lb = (const unsigned char *)
					  rd_slot_ptr(sb, 0);
		const unsigned char *ta = (const unsigned char *)
					  rd_slot_ptr(sa, 0x0c);

		diff_eq_int("the scrambler's history (%ld)",
			    memcmp(la, lb, (size_t)(ta - la) + 1) == 0, 1, tag);
	}
}

#define RD_NSTATE	0x12

static int
run_p4d_reset(void)
{
	long trial = 40000;
	int st, flag, qc, nof, lvl;
	int printed = 0, gated = 0, pumped = 0, qcsplit = 0, dirty = 0;
	int cparm = 0, mparm = 0, flagdiff = 0, rlen = 0;
	unsigned int qclen = 0, ddlen = 0;
	static unsigned char cp_pre[CP_SLOT], mp_pre[MP_SLOT];
	static unsigned char dem_pre[DEM_SLOT];
	static unsigned char flag0[P4D_SLOT];
	static char flag0_text[8192];

	diff_begin("V90Phase4Demodulator::reset");

	for (st = 0; st < RD_NSTATE; st++)
	 for (flag = 0; flag < 2; flag++)
	  for (qc = 0; qc < 2; qc++)
	   for (nof = 0; nof < 3; nof++)
	    for (lvl = 0; lvl < 3; lvl++) {
		int mode = (int)(trial % 4);
		unsigned char code = (unsigned char)(0x37u * (unsigned)trial);
		unsigned int qcarg = qc ? (0x1000u + (unsigned)(trial & 0xff))
					: 0u;
		Phase4DemodulatorState want = (Phase4DemodulatorState)st;
		int s;

		setup((int)trial, mode);

		/*
		 * THE TWO LENGTHS `quickConnect` CHOOSES BETWEEN, and the
		 * third that neither arm may take.  `setup` already plants
		 * RRN_TRN2D_DD_LENGTH, which is what the RdNot arms use.
		 */
		qclen = 0x310u + (unsigned int)(trial & 0x1f);
		ddlen = 0x480u + (unsigned int)(trial & 0x1f);
		PARAMS->TRN2D_QC_DD_LENGTH = (int)qclen;
		PARAMS->TRN2D_DD_LENGTH = (int)ddlen;

		/*
		 * The detector length, away from 0x18 and from its
		 * neighbours, so neither a swapped argument pair nor a
		 * misread displacement can agree by accident.
		 */
		PARAMS->PHASE4_R_DETECTION_LENGTH = 0x2a0 + (int)(trial & 0x3f);
		PARAMS->SD_DETECTOR_DETECTION_COUNTER_THRESHOLD = 0x1b30;

		/*
		 * The mapping blocks have to be usable by `V90Mapper::reset`,
		 * which `setMappingParams` reaches: a pseudorandom `shaperId`
		 * or `shaperSR` is a spectral shaper built out of rubbish and
		 * a divisor that may be zero.  The coefficients are exact
		 * binary fractions so the two builds' arithmetic cannot
		 * differ in a last bit.
		 */
		MAPP1->shaperSR = 2;
		MAPP1->shaperId = 1;
		MAPP2->shaperSR = 3;
		MAPP2->shaperId = 2;
		MAPP1->shaperA1 = 0.5f;
		MAPP1->shaperA2 = -0.25f;
		MAPP1->shaperB1 = 0.125f;
		MAPP1->shaperB2 = -0.0625f;
		MAPP2->shaperA1 = -0.75f;
		MAPP2->shaperA2 = 0.375f;
		MAPP2->shaperB1 = -0.1875f;
		MAPP2->shaperB2 = 0.03125f;

		/* Both companding laws, on an axis of their own. */
		ADI->pcmType = (PcmType)((trial & 4) ? 1 : 0);

		rd_construct(flag);

		for (s = 0; s < 2; s++) {
			V90Phase4Demodulator *d = &P4D(s);

			d->sessionFlag = (unsigned int)flag;
			d->autoDigitalImpDetector = ADI;
			/*
			 * SENTINELS OVER WHAT `reset` STORES A ZERO OR A ONE
			 * INTO.  `setup` seeds with varied bytes, so most of
			 * these already differ; these four are the ones the
			 * modulator's construction or this file's planting
			 * would otherwise have left holding the value `reset`
			 * is about to write.
			 */
			d->uint_34fc = 0xc1c1c100u + (unsigned int)trial;
			d->linearMappStudyStart = 0xc2c2c200u +
						  (unsigned int)trial;
			d->quickConnect = 0xc3c3c300u + (unsigned int)trial;
			d->trn2dDDLength = 0xc4c4c400u + (unsigned int)trial;
		}

		memcpy(cp_pre, cp_s[1], CP_SLOT);
		memcpy(mp_pre, mp_s[1], MP_SLOT);
		memcpy(dem_pre, dem_s[1], DEM_SLOT);

		set_level((unsigned)lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		P4D(0).reset(code, want, (unsigned int)nof, qcarg);
		ref_p4d_reset(p4d_s[1], code, want, (unsigned int)nof, qcarg);
		dsplib_debug_capture_on = 0;
		set_level(0);

		rd_compare("after reset", trial);
		compare_peers(trial);

		diff_eq_int("the transcripts agreed (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);

		/*
		 * BY VALUE ON THE BLOB'S SIDE.  Two runs agreeing cannot tell
		 * "stored correctly" from "both sides equally wrong".
		 */
		diff_eq_int("ucode took argument one (%ld)",
			    (long)P4D(1).ucode, (long)code, trial);
		diff_eq_int("quickConnect took argument four (%ld)",
			    (long)P4D(1).quickConnect, (long)qcarg, trial);
		diff_eq_int("trn2dDDLength followed quickConnect (%ld)",
			    (long)P4D(1).trn2dDDLength,
			    (long)(qc ? qclen : ddlen), trial);
		diff_eq_int("+0x38 was set to one (%ld)", (long)P4D(1).int_0038,
			    1L, trial);
		diff_eq_int("linearMappStudyStart was cleared (%ld)",
			    (long)P4D(1).linearMappStudyStart, 0L, trial);
		/*
		 * BOTH DETECTORS TAKE THE SAME PAIR, and the pair is
		 * (parameter, literal) and not the other way round.  The
		 * rounding `V90RDetector::reset` applies is that function's
		 * and is not restated here; what is asserted is that the
		 * literal survives it -- 0x18 is a multiple of both 6 and 12
		 * -- and that the other slot moves with the parameter, which
		 * the counter below is the denominator for.
		 */
		diff_eq_int("the two detectors took the same first argument "
			    "(%ld)", (long)P4D(1).rDetector1.int_04,
			    (long)P4D(1).rDetector2.int_04, trial);
		diff_eq_int("the first detector took the literal 0x18 (%ld)",
			    (long)P4D(1).rDetector1.int_08, 0x18L, trial);
		diff_eq_int("the second detector took the literal 0x18 (%ld)",
			    (long)P4D(1).rDetector2.int_08, 0x18L, trial);
		if (P4D(1).rDetector1.int_04 != 0x18)
			rlen++;

		/*
		 * THE ONE ASYMMETRY BETWEEN THE TWO SESSION ARMS: +0x34fc is
		 * written under V.92 and left alone under V.90.
		 */
		if (flag) {
			diff_eq_int("+0x34fc took the group size (%ld)",
				    (long)P4D(1).uint_34fc,
				    (long)MAPP1->word_0, trial);
			diff_eq_int("the CP took the group size (%ld)",
				    (long)CPR(1).word_3ba8,
				    (long)MAPP1->word_0, trial);
			if (memcmp(cp_pre, cp_s[1], CP_SLOT) != 0)
				cparm++;
		} else {
			diff_eq_int("+0x34fc was left alone (%ld)",
				    (long)P4D(1).uint_34fc,
				    (long)(0xc1c1c100u + (unsigned int)trial),
				    trial);
			diff_eq_int("the MP took the group size (%ld)",
				    (long)MPR(1).word_114,
				    (long)MAPP1->word_0, trial);
			if (memcmp(mp_pre, mp_s[1], MP_SLOT) != 0)
				mparm++;
		}

		if (memcmp(dem_pre, dem_s[1], DEM_SLOT) != 0)
			qcsplit++;

		if (nof > 0 && P4D(1).countInState != 0u)
			pumped++;

		/* The history is no longer all 0xa5, so `reset(0)` ran. */
		{
			const unsigned char *sb = p4d_s[1] + RD_P4M_SCRAM;
			const unsigned char *lb = (const unsigned char *)
						  rd_slot_ptr(sb, 0);
			const unsigned char *tb = (const unsigned char *)
						  rd_slot_ptr(sb, 0x0c);
			unsigned int n = (unsigned int)(tb - lb) + 1u;
			unsigned int i;
			int moved = 0;

			for (i = 0; i < n; i++)
				if (lb[i] != 0xa5)
					moved = 1;
			diff_eq_int("the modulator's history was reseeded "
				    "(%ld)", moved, 1, trial);
			if (moved)
				dirty++;
		}

		if (dsplib_debug_capture_text(1)[0] != '\0') {
			printed++;
			if (lvl > 1 &&
			    dsplib_debug_capture_lines(1) > 1)
				gated++;
		}

		/*
		 * BLOB AGAINST BLOB, AND IT IS AN ANTI-VACUITY CHECK RATHER
		 * THAN A COMPARISON.  The same trial with `sessionFlag` clear
		 * and set must leave two different objects or two different
		 * transcripts, which says the `sessionFlag` AXIS is real --
		 * that the object's two arms are distinguishable at all, so
		 * that sweeping it measures something.  3509's argument.
		 *
		 * IT IS NOT WHAT CATCHES "THE TWO DECISION MEMBERS ARE
		 * SWAPPED", and an earlier version of this comment said it
		 * was.  `mutate.py` patches `src/` and the blob is fixed, so a
		 * swap makes OUR side call the other member while the blob
		 * calls the right one, and the ordinary differential
		 * comparison fails; both swap mutations are caught there.
		 * Nor does this counter isolate the LOOP: `sessionFlag` also
		 * picks the CP arm over the MP arm, and that arm writes
		 * +0x34fc, so the two settings differ whether the loop ran or
		 * not.  `pumped` is the counter that speaks for the loop.
		 */
		if (flag == 0) {
			memcpy(flag0, p4d_s[1], P4D_SLOT);
			strncpy(flag0_text, dsplib_debug_capture_text(1),
				sizeof flag0_text - 1);
			flag0_text[sizeof flag0_text - 1] = '\0';
		} else if (memcmp(flag0, p4d_s[1], P4D_SLOT) != 0 ||
			   strcmp(flag0_text,
				  dsplib_debug_capture_text(1)) != 0) {
			flagdiff++;
		}

		rd_destruct();
		diff_eq_int("nothing left allocated (%ld)", harness_alloc.live,
			    0, trial);
		trial++;
	    }

	diff_eq_int("the detector length came from the parameter", rlen > 0,
		    1, 0);
	diff_eq_int("something was printed", printed > 0, 1, 0);
	diff_eq_int("and the raised level added a line", gated > 0, 1, 0);
	diff_eq_int("the decision loop ran", pumped > 0, 1, 0);
	diff_eq_int("the CP arm moved the CP record", cparm > 0, 1, 0);
	diff_eq_int("the MP arm moved the MP record", mparm > 0, 1, 0);
	diff_eq_int("the demapper was reset", qcsplit > 0, 1, 0);
	diff_eq_int("the modulator's history was reseeded somewhere",
		    dirty > 0, 1, 0);
	diff_eq_int("the two session flags took different arms",
		    flagdiff > 0, 1, 0);

	set_level(0);
	return diff_end();
}

/* ==================================== V90Demodulator::exitPhase3 ========== */

/*
 * exitPhase3 lives HERE and not in `t_v90demod.cpp` or `t_v90dataph.cpp`, and
 * the reason is its LAST statement rather than its first.  The member's own
 * fields hang off `V90Demodulator`, which both of those files build; what it
 * ends with is `phase4Demodulator->reset(...)`, and that reset reseeds two
 * `V90RDetector`s, resets the CP or the MP, hands the mapping block to the
 * demapper AND to an embedded `V90Phase4Modulator` whose converter owns a
 * `V90Mapper` and a `Scrambler` on the heap.  A seeded 0x351c block with three
 * planted pointers is a segfault, not a trial.  All of that apparatus is
 * already here -- `setup`, `rd_construct`, `rd_compare` and `rd_destruct` were
 * built for `V90Phase4Demodulator::reset` in the batch immediately before this
 * one -- so this file grows the SHALLOW half (a demodulator, a phase 3
 * demodulator, an equaliser, a designer, an additional-CP record and the two
 * Jd messages) instead of the deep one.
 *
 * WHAT IS PER SIDE AND WHAT IS SHARED, and the rule behind the split.
 * Anything `exitPhase3` WRITES has to exist twice or be restored between the
 * two calls, or the blob's run reads what our run left behind.  Four of the
 * objects it writes are peers this file already shares -- the parameter block
 * (`setNofUcodesInTrn2` stores into it), the mapping block (the designer
 * rewrites it), the detector (`V90TRN2Design` walks `maxUcode[]` DOWN through
 * the pointer `getMaxUcode` hands it) and the connection evaluator (the
 * delayed retrain request) -- and all four are snapshotted after our call,
 * restored, and compared against what the blob's call leaves.  That is
 * `t_v90equ`'s arena and it is why those four need no second copy.  The
 * demodulator, the phase 3 demodulator, the equaliser with its two
 * coefficient arrays, the designer and the additional-CP record are per side.
 *
 * THE EQUALISER IS WIRED THE WAY THE CONSTRUCTOR WIRES IT, which matters for
 * one claim: `resampler` is `&D(side)->resampler` and `spectralVerifier` is
 * `&D(side)->spectralVerifier`, the demodulator's OWN embedded subobjects, so
 * the `V90SpecialSpectralConditions` this function reads at +0x238 and the one
 * `V90Equalizer::enterPhase4` reads for its DFE-clear arm are the same word
 * rather than two planted copies.  `mmxMode` is held at 0: the fixed-point
 * arrays are `t_v90equ`'s to drive and a NULL one here would be a crash.
 *
 * THE SIX AXES, and every one of them has a counter that must fire:
 *
 *   - `inPhase3`, swept over 0, 1, 2, 5 and 0xffffffff.  ONE of those five
 *     does the work.  Testing it as `!= 0` instead of `== 1` is a live
 *     mutation because `enterChannelVerification` leaves the object at 5.
 *   - `phase3Demodulator->eventCode`, 0x14 or not, which is the phase 4 entry.
 *     `exitDIL` is what sets it to 0x14 in the field, so the sweep drives the
 *     modulator's state into and away from the terminating arm as well as
 *     planting the word directly.
 *   - the equaliser's `state`, 2 or not: 2 is `V90Equalizer::enterPhase4`'s
 *     own early return, so both the deep body and the cheap one are seen.
 *   - `sessionFlag`, which picks `jdV92->getMaxLookahead()` over `jd`'s.  The
 *     two message blocks hold DIFFERENT lookahead bits, so the choice has an
 *     observable; without that the two calls are interchangeable.
 *   - `trn1dRmsRatio`, over positive, negative, EXACTLY ZERO, a magnitude
 *     under one and one with eight significant fractional digits.  Its three
 *     printed numbers never reach memory, so the transcript is their only
 *     witness -- and zero is the only value that separates `> 0` from `>= 0`
 *     in the branchless sign character.
 *   - the design outcome.  `V90TRN2Design` must return both 0 and non-zero or
 *     the delayed-retrain arm, one of the four strings and the store into the
 *     connection evaluator are all dead.  The table shape and
 *     `nofUcodesInTrn2` move it across the boundary and `sawOk`/`sawFail`
 *     count both.
 *
 * plus `dsplibs_debug_level` at 0, 1 and 2, because "delayedRetrainRequest"
 * is behind `> 1` and the two `edprintf` lines are behind `> 0`.
 *
 * THE DETECTOR'S TABLES ARE RE-PLANTED AND THAT IS NOT DECORATION.  `setup`
 * gives `linMapp` a DESCENDING run for the demapper's benefit;
 * `V90TRN2Design` walks `topUcode[k]` down while the companded level is over
 * its ceiling and has no floor, so a descending table never terminates and
 * runs off the row.  The ascending shape with a zero at entry 0 is
 * `t_v90trn2design`'s and is what bounds the walk.
 *
 * WHAT THIS FIXTURE CANNOT PRESENT, said here rather than written as a
 * mutation that cannot fail: the inlined `enterPhase4()`'s own `inPhase3 == 2`
 * early return.  The only way into this member is `inPhase3 == 1`, so that
 * test never takes its taken edge here; it is exercised where it belongs, by
 * `t_v90dataph.cpp`'s latch sweep over the out-of-line member.
 */

extern "C" {
void ref_dem_exitphase3(void *)
	asm("ref__ZN14V90Demodulator10exitPhase3Ev");
}

#define X3_DEM_SLOT	((unsigned)sizeof(V90Demodulator) + 64u)
#define X3_P3D_SLOT	((unsigned)sizeof(V90Phase3Demodulator) + 64u)
#define X3_ACP_SLOT	((unsigned)sizeof(tagV90AdditionalCPinfo) + 64u)
#define X3_DES_SLOT	((unsigned)sizeof(V90TRN2Designer) + 64u)
#define X3_EQU_SLOT	((unsigned)sizeof(V90Equalizer) + 64u)
#define X3_PH2_SLOT	((unsigned)sizeof(V90Phase2Info) + 32u)
#define X3_COEFS	24u

static unsigned char x3_dem[2][X3_DEM_SLOT] __attribute__((aligned(8)));
static unsigned char x3_p3d[2][X3_P3D_SLOT] __attribute__((aligned(8)));
static unsigned char x3_acp[2][X3_ACP_SLOT] __attribute__((aligned(8)));
static unsigned char x3_des[2][X3_DES_SLOT] __attribute__((aligned(8)));
static unsigned char x3_equ[2][X3_EQU_SLOT] __attribute__((aligned(8)));
static float x3_le[2][X3_COEFS];
static float x3_dfe[2][X3_COEFS];
static unsigned char x3_cmp[2][X3_DEM_SLOT];
/*
 * The additional-CP record as it stood BEFORE the call, on the blob's side.
 * This is what turns "the latch returned early" into an observation: two runs
 * agreeing cannot tell an early return from the whole method, and comparing
 * one trial's object against ANOTHER trial's cannot either -- the seed moves
 * with the trial, so a memcmp across trials differs whatever the function did.
 * Five stores always land on the late path and none on the early one, so the
 * before/after of THIS object is the discriminator and the demodulator's own
 * bytes are not (with `eventCode != 0x14` and the design failing, `exitPhase3`
 * can leave every field of `V90Demodulator` exactly as it found it).
 */
static unsigned char x3_acp_pre[X3_ACP_SLOT];

/* Read-only, so one copy: both demodulators hold the same pointer. */
static unsigned char x3_ph2[X3_PH2_SLOT] __attribute__((aligned(8)));
static unsigned char x3_jd[sizeof(V90Jd)] __attribute__((aligned(8)));
static unsigned char x3_jd92[sizeof(V92Jd)] __attribute__((aligned(8)));

/* The arena: shared, written, snapshotted and restored between the calls. */
static unsigned char x3_parm_pre[sizeof parm_s];
static unsigned char x3_mapp_pre[sizeof mapp1_s];
static unsigned char x3_adi_pre[sizeof adi_s];
static unsigned char x3_ce_pre[sizeof ce_s];
static unsigned char x3_parm_post[sizeof parm_s];
static unsigned char x3_mapp_post[sizeof mapp1_s];
static unsigned char x3_adi_post[sizeof adi_s];
static unsigned char x3_ce_post[sizeof ce_s];

#define X3_D(s)		((V90Demodulator *)x3_dem[s])
#define X3_P3D(s)	((V90Phase3Demodulator *)x3_p3d[s])
#define X3_ACP(s)	((tagV90AdditionalCPinfo *)x3_acp[s])
#define X3_DES(s)	((V90TRN2Designer *)x3_des[s])
#define X3_EQU(s)	((V90Equalizer *)x3_equ[s])
#define X3_PH2		((V90Phase2Info *)x3_ph2)

/*
 * The demodulator's five per-side pointer words, and the equaliser's four.
 * Everything else in both objects points at a shared peer and therefore holds
 * the SAME value on both sides, which is a comparison rather than a hole.
 */
static const unsigned x3_dem_skip[] = {
	0x01cu, 0x020u, 0x1d8u, 0x1dcu, 0x1e0u, ~0u
};
static const unsigned x3_equ_skip[] = { 0x00u, 0x14u, 0x40u, 0x58u, ~0u };
static const unsigned x3_des_skip[] = { 0x04u, ~0u };

static void
x3_arena_save(void)
{
	memcpy(x3_parm_pre, parm_s, sizeof parm_s);
	memcpy(x3_mapp_pre, mapp1_s, sizeof mapp1_s);
	memcpy(x3_adi_pre, adi_s, sizeof adi_s);
	memcpy(x3_ce_pre, ce_s, sizeof ce_s);
}

static void
x3_arena_switch(void)
{
	memcpy(x3_parm_post, parm_s, sizeof parm_s);
	memcpy(x3_mapp_post, mapp1_s, sizeof mapp1_s);
	memcpy(x3_adi_post, adi_s, sizeof adi_s);
	memcpy(x3_ce_post, ce_s, sizeof ce_s);
	memcpy(parm_s, x3_parm_pre, sizeof parm_s);
	memcpy(mapp1_s, x3_mapp_pre, sizeof mapp1_s);
	memcpy(adi_s, x3_adi_pre, sizeof adi_s);
	memcpy(ce_s, x3_ce_pre, sizeof ce_s);
}

static void
x3_arena_compare(long tag)
{
	diff_eq_obj_(__FILE__, __LINE__, "the parameter block after exitPhase3",
		     "V90Parameters", x3_parm_post, parm_s, sizeof parm_s, tag);
	diff_eq_obj_(__FILE__, __LINE__, "the mapping block after exitPhase3",
		     "V90MappingParams", x3_mapp_post, mapp1_s,
		     sizeof mapp1_s, tag);
	diff_eq_obj_(__FILE__, __LINE__, "the detector after exitPhase3",
		     "V90AutoDigitalImpDetector", x3_adi_post, adi_s,
		     sizeof adi_s, tag);
	diff_eq_obj_(__FILE__, __LINE__, "the evaluator after exitPhase3",
		     "V90ConnectionEvaluator", x3_ce_post, ce_s, sizeof ce_s,
		     tag);
}

/*
 * The ascending per-code tables `V90TRN2Design` needs, with a zero at entry 0
 * of every row so the unbounded `topUcode` walk terminates.
 */
static void
x3_tables(int shape, unsigned lf)
{
	int k, i;

	for (k = 0; k < V90ADID_PHASES; k++) {
		int step = 4 + shape * 13 + k;

		for (i = 0; i < V90ADID_CODES; i++) {
			int v = i * step + (i * i) / (2 + (shape & 3));

			ADI->linMapp[k][i] = (short)v;
			ADI->linMappAlt[k][i] = (short)(v - (i & 7));
			ADI->byte_0d00[k][i] =
				(unsigned char)(((i * 7 + k) % 5) != 0);
		}
		ADI->linMapp[k][0] = 0;
		ADI->linMappAlt[k][0] = 0;
		ADI->short_2800[k] = (short)((lf >> k) & 1u);
		ADI->maxUcode[k] = (unsigned char)(96u + ((lf >> k) & 31u));
	}
}

static int
run_exit_phase3(void)
{
	static const unsigned int latch_v[] = { 0u, 1u, 2u, 5u, 0xffffffffu };
	static const float ratio_v[] = {
		0.0f, 2.5f, -2.5f, 0.75f, -0.75f, 12345.6789f, -0.00390625f,
		137.03125f
	};
	static const int nof_v[] = { 1, 2, 3, 4, 7, 8, 10, 13, 21 };
	long trial = 60000;
	int li, w30, eqs, sf, ri;
	int sawEarly = 0, sawLate = 0;
	int sawEnter = 0, sawSkip = 0;
	int sawEquDeep = 0, sawEquEarly = 0;
	int sawJd = 0, sawJd92 = 0;
	int sawOk = 0, sawFail = 0;
	int sawPlus = 0, sawMinus = 0, sawZero = 0;
	int sawLevel[3];
	int printed = 0, gated = 0, latchDiff = 0;

	sawLevel[0] = sawLevel[1] = sawLevel[2] = 0;

	diff_begin("V90Demodulator::exitPhase3");

	for (li = 0; li < 5; li++)
	 for (w30 = 0; w30 < 2; w30++)
	  for (eqs = 0; eqs < 2; eqs++)
	   for (sf = 0; sf < 2; sf++)
	    for (ri = 0; ri < 8; ri++) {
		int mode = (int)(trial % 4);
		int lvl = (int)(trial % 3);
		int shape = (int)(trial % 6);
		int nof = nof_v[trial % 9];
		int cond = (int)(trial % 4);
		unsigned lf = 0x1234u + 0x9e37u * (unsigned)trial;
		unsigned int qc = (trial & 8) ? (0x2000u + (unsigned)(trial & 0xff))
					      : 0u;
		short got;
		int s;

		setup((int)trial, mode);
		x3_tables(shape, lf);

		/* What the phase 4 reset at the end of the member needs. */
		PARAMS->TRN2D_QC_DD_LENGTH = 0x310 + (int)(trial & 0x1f);
		PARAMS->TRN2D_DD_LENGTH = 0x480 + (int)(trial & 0x1f);
		PARAMS->PHASE4_R_DETECTION_LENGTH = 0x2a0 + (int)(trial & 0x3f);
		PARAMS->SD_DETECTOR_DETECTION_COUNTER_THRESHOLD = 0x1b30;

		/*
		 * What the designer needs.  `unnamed_080` is what
		 * `setNofUcodesInTrn2` copies over `nofUcodesInTrn2` when its
		 * argument is non-zero, so the two are planted to DIFFERENT
		 * values -- otherwise "the copy was made" has no witness.
		 */
		PARAMS->nofUcodesInTrn2 = nof;
		PARAMS->unnamed_080 = nof + 1 + (int)(trial % 5);
		PARAMS->maxUcode = 60 + (int)(trial % 40);
		PARAMS->unnamed_360 = (trial % 7) == 0 ? 0x100
				    : (trial % 7) == 1 ? 0 : (int)(trial % 7);
		PARAMS->SPECTRAL_SHAPER_A1 = 1.5f;
		PARAMS->SPECTRAL_SHAPER_A2 = -0.25f;
		PARAMS->SPECTRAL_SHAPER_B1 = 0.75f;
		PARAMS->SPECTRAL_SHAPER_B2 = 0.125f;
		PARAMS->SPECTRAL_SHAPER_SR = 3 + (int)(trial % 4);
		PARAMS->SPECTRAL_SHAPER_ID = 5 + (int)(trial % 9);
		PARAMS->GERMAN_PBX_SPECTRAL_SHAPER_A1 = -1.5f;
		PARAMS->GERMAN_PBX_SPECTRAL_SHAPER_A2 = 0.25f;
		PARAMS->GERMAN_PBX_SPECTRAL_SHAPER_B1 = -0.75f;
		PARAMS->GERMAN_PBX_SPECTRAL_SHAPER_B2 = -0.125f;
		PARAMS->GERMAN_PBX_SPECTRAL_SHAPER_SR = 2 + (int)(trial % 5);
		PARAMS->GERMAN_PBX_SPECTRAL_SHAPER_ID = 3 + (int)(trial % 11);

		/*
		 * `additionalCPinfo`'s sixteen-bit slot comes from here, and
		 * it MOVES, so a dropped store cannot pass by holding the
		 * value it was going to be given.
		 */
		PARAMS->ANALOG_RATE_MASK = (int)(0x51a30000u +
						 (unsigned)(trial & 0x7fff));

		ADI->pcmType = (PcmType)((trial & 4) ? 1 : 0);
		ADI->int_a960 = (int)(0x0a960000u + (unsigned)(trial & 0xff));
		ADI->unSuspectedPhase = (short)(trial * 977);

		fill(x3_ph2, X3_PH2_SLOT, lf ^ 0x2f1u);
		fill(x3_jd, (unsigned)sizeof x3_jd, lf ^ 0x915u);
		fill(x3_jd92, (unsigned)sizeof x3_jd92, lf ^ 0x66du);
		X3_PH2->rtd = 0x1234 + (int)(trial & 0x3ff);
		X3_PH2->Uinfo = (unsigned char)(trial * 11u + 3u);
		X3_PH2->maxTxPower = (unsigned char)(trial % 21);
		/*
		 * DIFFERENT LOOKAHEADS.  `getMaxLookahead` is bits 30 and 31
		 * of each message, so the two blocks are planted to answer 1
		 * and 2 -- if they agreed, `sessionFlag` choosing between
		 * them would have no observable at all.
		 */
		((V90Jd *)x3_jd)->bits[30] = 1;
		((V90Jd *)x3_jd)->bits[31] = 0;
		((V92Jd *)x3_jd92)->bits[30] = 0;
		((V92Jd *)x3_jd92)->bits[31] = 1;

		rd_construct((int)(trial & 1));

		for (s = 0; s < 2; s++) {
			V90Demodulator *d = X3_D(s);
			V90Equalizer *e = X3_EQU(s);
			unsigned int k;

			fill(x3_dem[s], X3_DEM_SLOT, lf ^ 0x3b7u);
			fill(x3_p3d[s], X3_P3D_SLOT, lf ^ 0x4c1u);
			fill(x3_acp[s], X3_ACP_SLOT, lf ^ 0x5d9u);
			fill(x3_des[s], X3_DES_SLOT, lf ^ 0x6e3u);
			fill(x3_equ[s], X3_EQU_SLOT, lf ^ 0x7f5u);
			for (k = 0; k < X3_COEFS; k++) {
				x3_le[s][k] = (float)((int)k - 9) * 0.0625f;
				x3_dfe[s][k] =
				    (float)((int)((k * 13u) % 41u) - 17)
				    * 0.03125f;
			}

			d->phase2Info = X3_PH2;
			d->jd = (V90Jd *)x3_jd;
			d->jdV92 = (V92Jd *)x3_jd92;
			d->mappingParams = MAPP1;
			d->mappingParamsAlt = MAPP2;
			d->trn2Designer = X3_DES(s);
			d->additionalCPinfo = X3_ACP(s);
			d->params = PARAMS;
			d->sessionFlag = sf ? (0x8000u + (unsigned)trial) : 0u;
			d->inPhase3 = latch_v[li];
			d->equalizer = e;
			d->phase3Demodulator = X3_P3D(s);
			d->phase4Demodulator = &P4D(s);
			d->connectionEvaluator = CEV;
			d->autoDigitalImpDetector = ADI;
			d->trn1dRmsRatio = ratio_v[ri];
			d->quickConnect = qc;
			d->spectralVerifier.word_28 = (unsigned int)cond;
			d->resampler.params = PARAMS;
			d->resampler.ppmScale = 4.0f;
			d->resampler.timingOffset = 1.25e-5f *
			    (float)(int)((trial % 7) - 3);
			d->resampler.bllState = (V90BllState)
			    ((trial & 2) ? V90_BLL_FROZEN
					 : V90_BLL_STEADY_STATE);
			d->resampler.stateSamples = 0x11223344u;
			d->resampler.countStateSamples = 0x55667788u;

			X3_DES(s)->params = PARAMS;
			X3_DES(s)->power = &d->constellationPower;

			X3_P3D(s)->autoDigitalImpDetector = ADI;
			X3_P3D(s)->params = PARAMS;
			X3_P3D(s)->eventCode = w30 ? 0x14u
						 : (0x30u + (unsigned)trial);
			/*
			 * `exitDIL` runs the modulator's own exit only from
			 * seven states, and the terminating arm is what puts
			 * 0x14 into `eventCode` in the field.  Both are driven:
			 * the state moves with the trial and the word is
			 * planted on top of whatever `exitDIL` leaves.
			 */
			X3_P3D(s)->state = (Phase3DemodulatorState)
			    (int)(trial % 18);
			X3_P3D(s)->word_2c = 0x2c000000u + (unsigned)trial;
			X3_P3D(s)->phase3Modulator.state =
			    (Phase3ModulatorState)(int)((trial / 3) % 12);
			X3_P3D(s)->phase3Modulator.symbolCount =
			    (unsigned int)(trial & 3);
			X3_P3D(s)->phase3Modulator.segmentPos =
			    (unsigned int)((trial >> 2) & 1);
			X3_P3D(s)->phase3Modulator.eventCode = 0;

			e->resampler = &d->resampler;
			e->spectralVerifier = &d->spectralVerifier;
			e->params = PARAMS;
			e->linearEquCoefs = x3_le[s];
			e->dfeCoefs = x3_dfe[s];
			e->linearEquLength = X3_COEFS;
			e->dfeLength = X3_COEFS;
			e->linearEquHistoryLength = X3_COEFS;
			e->state = eqs ? V90EQU_STATE_PHASE4
				       : (int)(1 + trial % 5);
			e->stateCount = 0x5c5c0000 + (int)(trial & 0xff);
			e->mmxMode = 0;
			e->mmxArraysPresent = 0;
			e->linearEquBeta = 0.25f;
			e->dfeBeta = -0.125f;
			e->meanErrorCount = 17u;
			e->meanErrorFull = 1u;
			e->maxLeCoefValue = 1.0f;
			e->minLeCoefValue = 0.0f;
			e->maxDfeCoefValue = 1.0f;
			e->minDfeCoefValue = 0.0f;
			e->linearEquMmxConversionFactor = 1024.0f;
			e->dfeMmxConversionFactor = 256.0f;

			P4D(s).sessionFlag = (unsigned int)(trial & 1);
			P4D(s).autoDigitalImpDetector = ADI;
			/* 7105: sentinels over what `reset` stores 0 or 1 in. */
			P4D(s).uint_34fc = 0xc1c1c100u + (unsigned int)trial;
			P4D(s).linearMappStudyStart = 0xc2c2c200u +
						      (unsigned int)trial;
			P4D(s).quickConnect = 0xc3c3c300u +
					      (unsigned int)trial;
			P4D(s).trn2dDDLength = 0xc4c4c400u +
					       (unsigned int)trial;
		}

		/*
		 * The delayed retrain request is a store of ONE, so the slot
		 * is planted away from one -- the fill never produces a zero
		 * byte, but 1 is a value it could hold.
		 */
		CEV->delayedRetrainRequest = 0xd7d70000u + (unsigned int)trial;

		memcpy(x3_acp_pre, x3_acp[1], X3_ACP_SLOT);
		x3_arena_save();
		set_level((unsigned)lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		X3_D(0)->exitPhase3();
		x3_arena_switch();
		ref_dem_exitphase3(x3_dem[1]);
		dsplib_debug_capture_on = 0;
		sawLevel[lvl]++;

		scrub(x3_cmp[0], x3_dem[0], X3_DEM_SLOT, x3_dem_skip);
		scrub(x3_cmp[1], x3_dem[1], X3_DEM_SLOT, x3_dem_skip);
		diff_eq_obj_(__FILE__, __LINE__, "after exitPhase3",
			     "V90Demodulator", x3_cmp[0], x3_cmp[1],
			     X3_DEM_SLOT, trial);
		diff_eq_obj_(__FILE__, __LINE__, "the additional CP record",
			     "tagV90AdditionalCPinfo", x3_acp[0], x3_acp[1],
			     X3_ACP_SLOT, trial);
		diff_eq_obj_(__FILE__, __LINE__, "the phase 3 demodulator",
			     "V90Phase3Demodulator", x3_p3d[0], x3_p3d[1],
			     X3_P3D_SLOT, trial);
		scrub(x3_cmp[0], x3_equ[0], X3_EQU_SLOT, x3_equ_skip);
		scrub(x3_cmp[1], x3_equ[1], X3_EQU_SLOT, x3_equ_skip);
		diff_eq_obj_(__FILE__, __LINE__, "the equaliser",
			     "V90Equalizer", x3_cmp[0], x3_cmp[1],
			     X3_EQU_SLOT, trial);
		diff_eq_int("the equaliser's coefficients (%ld)",
			    memcmp(x3_le[0], x3_le[1], sizeof x3_le[0]) == 0 &&
			    memcmp(x3_dfe[0], x3_dfe[1], sizeof x3_dfe[0]) == 0,
			    1, trial);
		scrub(x3_cmp[0], x3_des[0], X3_DES_SLOT, x3_des_skip);
		scrub(x3_cmp[1], x3_des[1], X3_DES_SLOT, x3_des_skip);
		diff_eq_obj_(__FILE__, __LINE__, "the designer",
			     "V90TRN2Designer", x3_cmp[0], x3_cmp[1],
			     X3_DES_SLOT, trial);
		x3_arena_compare(trial);
		rd_compare("the phase 4 receiver after exitPhase3", trial);
		diff_eq_int("transcript (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);

		/*
		 * THE LATCH, AGAINST WHAT WAS THERE BEFORE THE CALL.  Two
		 * runs agreeing cannot tell "it returned early" from "it did
		 * the work", and neither can one trial's object compared
		 * against another's -- the fill moves with the trial, so a
		 * memcmp across trials differs whatever the function did.
		 * What separates the two is the SAME object before and after,
		 * which is why `x3_acp_pre` is taken above.
		 */
		if (latch_v[li] == 1u) {
			sawLate++;
			if (memcmp(x3_acp_pre, x3_acp[1], X3_ACP_SLOT) != 0)
				latchDiff++;
		} else {
			sawEarly++;
			diff_eq_int("the early exit changed nothing (%ld)",
				    memcmp(x3_acp_pre, x3_acp[1],
					   X3_ACP_SLOT) == 0 &&
				    dsplib_debug_capture_lines(1) == 0, 1,
				    trial);
		}

		if (latch_v[li] == 1u) {
			got = *(const short *)&x3_acp[1][0x14];
			diff_eq_int("the analog rate mask reached +0x14 "
				    "(%ld)",
				    (long)got,
				    (long)(short)PARAMS->ANALOG_RATE_MASK,
				    trial);
			diff_eq_int("the detector's word reached +0x0c (%ld)",
				    (long)X3_ACP(1)->word_0c,
				    (long)ADI->int_a960, trial);
			diff_eq_int("the phase 4 demodulator took the "
				    "quick-connect flag (%ld)",
				    (long)P4D(1).quickConnect, (long)qc,
				    trial);
			diff_eq_int("and its ucode came from the Phase 2 "
				    "record (%ld)",
				    (long)P4D(1).ucode,
				    (long)X3_PH2->Uinfo, trial);

			if (w30) {
				sawEnter++;
				diff_eq_int("phase 4 was entered (%ld)",
					    (long)X3_D(1)->inPhase3, 2, trial);
				if (eqs)
					sawEquEarly++;
				else
					sawEquDeep++;
			} else {
				sawSkip++;
				diff_eq_int("phase 4 was NOT entered (%ld)",
					    (long)X3_D(1)->inPhase3, 1, trial);
			}
			if (sf)
				sawJd92++;
			else
				sawJd++;
			if (CEV->delayedRetrainRequest == 1u)
				sawFail++;
			else
				sawOk++;
			if (dsplib_debug_capture_lines(1) > 0)
				printed++;
			if (lvl == 2 && CEV->delayedRetrainRequest == 1u)
				gated++;
			if (ratio_v[ri] > 0.0f)
				sawPlus++;
			else if (ratio_v[ri] < 0.0f)
				sawMinus++;
			else
				sawZero++;
		}

		rd_destruct();
		diff_eq_int("nothing left allocated (%ld)", harness_alloc.live,
			    0, trial);
		trial++;
	    }

	diff_eq_int("the latch returned early somewhere", sawEarly > 0, 1, 0);
	diff_eq_int("and did the work somewhere", sawLate > 0, 1, 0);
	diff_eq_int("and the late path moved the record it returned early "
		    "without touching", latchDiff > 0, 1, 0);
	diff_eq_int("phase 4 was entered somewhere", sawEnter > 0, 1, 0);
	diff_eq_int("and skipped somewhere", sawSkip > 0, 1, 0);
	diff_eq_int("the equaliser ran its body somewhere", sawEquDeep > 0, 1,
		    0);
	diff_eq_int("and took its early out somewhere", sawEquEarly > 0, 1, 0);
	diff_eq_int("the V.90 lookahead was read somewhere", sawJd > 0, 1, 0);
	diff_eq_int("the V.92 lookahead was read somewhere", sawJd92 > 0, 1, 0);
	diff_eq_int("the design succeeded somewhere", sawOk > 0, 1, 0);
	diff_eq_int("and failed somewhere", sawFail > 0, 1, 0);
	diff_eq_int("a positive ratio was printed", sawPlus > 0, 1, 0);
	diff_eq_int("a negative one too", sawMinus > 0, 1, 0);
	diff_eq_int("and exactly zero", sawZero > 0, 1, 0);
	diff_eq_int("something was printed", printed > 0, 1, 0);
	diff_eq_int("the gated line had a level to appear at", gated > 0, 1, 0);
	diff_eq_int("level 0 was driven", sawLevel[0] > 0, 1, 0);
	diff_eq_int("level 1 was driven", sawLevel[1] > 0, 1, 0);
	diff_eq_int("level 2 was driven", sawLevel[2] > 0, 1, 0);

	set_level(0);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_sweep(0);
	rc |= run_sweep(1);
	rc |= run_getdecision();
	rc |= run_p4d_reset();
	rc |= run_exit_phase3();

	return rc;
}
