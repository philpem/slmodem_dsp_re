/*
 * t_v90demap.cpp -- differential test of the V.90 demapper cluster: the five
 * members that turn PCM samples into bits.
 *
 *     V90SignBitsExtractor::applyFrameAction(ACTIONS, uchar *, uchar *)
 *     V90SignBitsExtractor::process(uchar *, uchar *)
 *     V90Demapper::resetLinearMappStudy(unsigned)
 *     V90Demapper::hardDecision(short)
 *     V90Demapper::process(uchar *, unsigned &)
 *
 * WHY THIS IS ITS OWN BINARY and not another suite inside t_v90demapctor.cpp:
 * finding 1264's rule, that a mutation suite is a (source file, test binary)
 * pair and what must not be shared is ANCHOR TEXT.  These five and the
 * constructor live in two source files between them and both files already
 * carry a suite; a third suite over the same two files needs anchors that
 * appear once, which is what `v90demap.json` and `v90sbe.json` are checked
 * for.  A shared binary would also make every trial of the constructor pay
 * for the 40 KB detector this file needs.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS COMPARED, AND WHAT CANNOT BE
 *
 * THE OBJECTS ARE NEVER ZEROED (finding 230).  Every slot gets varied
 * pseudorandom bytes before every trial and is reseeded every trial, so a
 * store that fails to happen is visible and a store of zero into memory that
 * was already zero is not mistaken for one.  Then the fields each function
 * actually reads are planted on top, identically on both sides.
 *
 * THREE WORDS PER DEMAPPER HOLD ADDRESSES AND ARE EXCLUDED FROM THE OBJECT
 * COMPARISON: `codes` (+0x1c), `signs` (+0x20) and the embedded extractor's
 * decoder buffer (+0x684).  What is compared instead is what they point AT,
 * over their full length, plus the object either side of them.  The parameter
 * block is shared between the two sides on purpose (finding 1105): identical
 * argument pointers give identical stored pointers, so +0x00 stays IN the
 * comparison rather than being blanked out of it.
 *
 * THE DETECTOR IS SHARED WHERE IT IS READ AND SPLIT WHERE IT IS WRITTEN.
 * `hardDecision` only reads `short_2800`, so both sides get one detector and
 * +0x1ea0 compares equal.  `resetLinearMappStudy` CLEARS 768 cells of three
 * arrays in it, so that suite gives each side its own detector, excludes
 * +0x1ea0, and compares the two detectors byte for byte -- which is where the
 * whole of that function's work lands.
 *
 * THE RETURN VALUES ARE COMPARED, and for `hardDecision` that is most of what
 * it does: the object it writes gains two array entries and two counters, and
 * the nearest constellation level with the sign put back on it comes out in
 * `%ax` and nowhere else.
 *
 * THE TRANSCRIPTS ARE COMPARED AS TEXT, at `dsplibs_debug_level` 0 and 2.
 * `hardDecision` has one diagnostic of its own -- the over-capacity refusal --
 * and reaches four more through `printErrorHistogramAndReset`.  `edprintf`
 * ENCODES its output, so a transcript that matches is a format string, an
 * argument list and a character count that all match (finding 180).
 *
 * ANTI-VACUITY, and it is per finding 3509 rather than per path.  Every
 * counter below names an OBSERVABLE difference -- a return value, a byte of
 * the object, a line of transcript -- and not a branch believed to have been
 * taken.  "The refusal arm was reached" is counted as `sampleCount did not
 * move AND a line was printed`, both of which a caller can see; "the histogram
 * accumulated" as `errorCount rose`; "the histogram was printed and reset" as
 * `errorHistogramCount rose`.  A counter that only proved a path was entered
 * would prove nothing, which is what four of five in one earlier batch did.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/DiffCoder.h"
/*
 * The NAMED 0x558 `V90Parameters` map, the one V90Demapper.cpp compiles
 * against.
 */
#include "dsplib/V90Parameters.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/ModulusCoder.h"
#include "dsplib/V90SignBitsExtractor.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Demapper.h"

extern "C" {
/*
 * Both sides through asm() labels, so the two calls are the same declared
 * signature and the convention is stated once.  Plain cdecl with `this` as the
 * first STACK argument, finding 215, so no attribute is involved.
 */
short our_harddec(void *self, short in) asm("_ZN11V90Demapper12hardDecisionEs");
short ref_harddec(void *self, short in)
	asm("ref__ZN11V90Demapper12hardDecisionEs");

int our_demproc(void *self, unsigned char *out, unsigned int *nbits)
	asm("_ZN11V90Demapper7processEPhRj");
int ref_demproc(void *self, unsigned char *out, unsigned int *nbits)
	asm("ref__ZN11V90Demapper7processEPhRj");

void our_rlms(void *self, unsigned int n)
	asm("_ZN11V90Demapper20resetLinearMappStudyEj");
void ref_rlms(void *self, unsigned int n)
	asm("ref__ZN11V90Demapper20resetLinearMappStudyEj");

void our_sbeproc(void *self, unsigned char *in, unsigned char *out)
	asm("_ZN20V90SignBitsExtractor7processEPhS0_");
void ref_sbeproc(void *self, unsigned char *in, unsigned char *out)
	asm("ref__ZN20V90SignBitsExtractor7processEPhS0_");

void our_sbeafa(void *self, int action, unsigned char *in, unsigned char *out)
	asm("_ZN20V90SignBitsExtractor16applyFrameActionENS_7ACTIONSEPhS1_");
void ref_sbeafa(void *self, int action, unsigned char *in, unsigned char *out)
	asm("ref__ZN20V90SignBitsExtractor16applyFrameActionENS_7ACTIONSEPhS1_");

void our_incrbs(void *self)
	asm("_ZN11V90Demapper25incrementRBSFramePositionEv");
void ref_incrbs(void *self)
	asm("ref__ZN11V90Demapper25incrementRBSFramePositionEv");

void our_updconst(void *self) asm("_ZN11V90Demapper18updateConstelationEv");
void ref_updconst(void *self)
	asm("ref__ZN11V90Demapper18updateConstelationEv");

void our_resetns(void *self, void *mp)
	asm("_ZN11V90Demapper15resetNoSpectralEP16V90MappingParams");
void ref_resetns(void *self, void *mp)
	asm("ref__ZN11V90Demapper15resetNoSpectralEP16V90MappingParams");

void our_lms(void *self, short sample, short level)
	asm("_ZN11V90Demapper18linearMappingStudyEss");
void ref_lms(void *self, short sample, short level)
	asm("ref__ZN11V90Demapper18linearMappingStudyEss");

extern unsigned int ref_dsplibs_debug_level;
}

/* ------------------------------------------------------------------ storage */

#define DEM_SIZE	0x1eb8
#define DEM_SLOT	(DEM_SIZE + 64)
#define SBE_SLOT	(0x28 + 32)
#define PARM_SLOT	(sizeof(V90Parameters) + 64)

/* Comfortably more than any trial below asks for. */
#define NSAMPLE		72
#define NOUT		1024

/* The three words of a demapper that hold an address; see the file comment. */
#define SBE_DECODER_PTR	0x684u

static unsigned char dem_s[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char sbe_s[2][SBE_SLOT] __attribute__((aligned(8)));
static unsigned char parm_s[PARM_SLOT] __attribute__((aligned(8)));
/*
 * ONE MAPPING BLOCK, SHARED.  `resetNoSpectral` only READS it, so giving both
 * sides the same pointer keeps the inputs identical by construction; nothing
 * stores the pointer, so nothing has to be excluded from the comparison for
 * it.  The 64 bytes past the end catch a store off the end of the last array.
 */
#define MAPP_SLOT	(sizeof(V90MappingParams) + 64)
static unsigned char mapp_s[MAPP_SLOT] __attribute__((aligned(8)));
#define MAPP	((V90MappingParams *)mapp_s)
static unsigned char adi_s[2][sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));

static unsigned int code_s[2][NSAMPLE];
static unsigned char sign_s[2][NSAMPLE];
static unsigned char sbstate_s[2][V90SBE_DECODER_SIZE];
static unsigned char out_s[2][NOUT];
static unsigned char in_s[V90SBE_DECODER_SIZE];

static unsigned char dem_before[2][DEM_SLOT];
/* One snapshot of the SHARED detector, for the members that only read it. */
static unsigned char adi_before[sizeof(V90AutoDigitalImpDetector)];
static unsigned char scratch[2][DEM_SLOT];

#define DEM(s)	(*(V90Demapper *)dem_s[s])
#define SBE(s)	(*(V90SignBitsExtractor *)sbe_s[s])
#define ADI(s)	(*(V90AutoDigitalImpDetector *)adi_s[s])
#define PARAMS	((V90Parameters *)parm_s)

/* Varied, never zero, never the same twice: findings 223, 224, 230. */
static unsigned
fill(unsigned char *p, int n, unsigned lfsr)
{
	int i;

	for (i = 0; i < n; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		p[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
	return lfsr;
}

/* ------------------------------------------------------- object comparison */

static void
cmp_obj(const char *what, const unsigned *skip, size_t n, const char *type,
	long trial)
{
	int i;

	memcpy(scratch[0], dem_s[0], DEM_SLOT);
	memcpy(scratch[1], dem_s[1], DEM_SLOT);
	for (i = 0; skip[i] != ~0u; i++) {
		memset(scratch[0] + skip[i], 0, 4);
		memset(scratch[1] + skip[i], 0, 4);
	}
	diff_eq_obj_(__FILE__, __LINE__, what, type, scratch[0], scratch[1], n,
		     trial);
	diff_eq_int("no store past the object (%ld)",
		    memcmp(scratch[0] + n, scratch[1] + n, DEM_SLOT - n) == 0,
		    1, trial);
}

/* The two demappers, with the three address words taken out. */
static const unsigned skip_read[] = { 0x1cu, 0x20u, SBE_DECODER_PTR, ~0u };
/* Plus the detector, where the two sides hold two different ones. */
static const unsigned skip_write[] = { 0x1cu, 0x20u, SBE_DECODER_PTR, 0x1ea0u,
				       ~0u };

/* ================================================================= SBE ==== */

/*
 * Plant one standalone extractor per side.  `reset` is not written, so the
 * three words it would set are planted directly and the parallel decoder is
 * driven through its own `reset`, which is.
 */
static void
sbe_setup(int s, unsigned int width, unsigned int state, unsigned int prev,
	  unsigned lfsr)
{
	V90SignBitsExtractor *e = &SBE(s);

	fill(sbe_s[s], SBE_SLOT, lfsr);
	fill(sbstate_s[s], V90SBE_DECODER_SIZE, lfsr ^ 0x1234u);

	e->spacing = width ? V90SBE_DECODER_SIZE / width : 0u;
	e->width = width;
	e->state = state;
	e->oddDecoder.prev_ = (unsigned char)prev;
	e->decoder.state_ = sbstate_s[s];
	e->decoder.capacity_ = V90SBE_DECODER_SIZE;
	e->decoder.size_ = 0u;
	e->decoder.reset(width, 0);
}

static void
sbe_cmp(const char *what, long trial)
{
	unsigned char a[SBE_SLOT], b[SBE_SLOT];

	memcpy(a, sbe_s[0], SBE_SLOT);
	memcpy(b, sbe_s[1], SBE_SLOT);
	/* +0x1c is the decoder's state_ pointer and holds two addresses. */
	memset(a + 0x1c, 0, 4);
	memset(b + 0x1c, 0, 4);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SignBitsExtractor", a, b,
		     sizeof(V90SignBitsExtractor), trial);
	diff_eq_int("nothing past the extractor (%ld)",
		    memcmp(a + sizeof(V90SignBitsExtractor),
			   b + sizeof(V90SignBitsExtractor),
			   SBE_SLOT - sizeof(V90SignBitsExtractor)) == 0, 1,
		    trial);
	diff_eq_int("the decoder's own state (%ld)",
		    memcmp(sbstate_s[0], sbstate_s[1], V90SBE_DECODER_SIZE)
		    == 0, 1, trial);
	diff_eq_int("the output buffer (%ld)",
		    memcmp(out_s[0], out_s[1], NOUT) == 0, 1, trial);
}

static const unsigned int width_v[] = { 1u, 2u, 3u, 6u };
#define NWIDTH ((int)(sizeof(width_v) / sizeof(width_v[0])))

/*
 * `applyFrameAction` first, because `process` inlines it: if the four arms are
 * wrong this suite says so against a function whose only other input is a
 * width, and `process`'s failures are then its own.
 */
static int
run_apply(void)
{
	long trial = 300000;
	int wi, act, pat, s;
	int distinct = 0;
	unsigned char seen[64][V90SBE_DECODER_SIZE];

	diff_begin("V90SignBitsExtractor::applyFrameAction");

	for (wi = 0; wi < NWIDTH; wi++)
	    for (act = -1; act < 5; act++)
		for (pat = 0; pat < 8; pat++) {
			unsigned int width = width_v[wi];
			int i, k;

			trial++;
			for (i = 0; i < (int)V90SBE_DECODER_SIZE; i++)
				in_s[i] = (unsigned char)
				    ((pat >> (i & 2)) & 1 ? 0 : (i + pat + 1));
			for (s = 0; s < 2; s++) {
				sbe_setup(s, width, 0u, 0u,
					  0x51a3u + 0x2f11u *
					  (unsigned)trial);
				fill(out_s[s], NOUT,
				     0x7b19u + (unsigned)trial);
			}
			our_sbeafa(&SBE(0), act, in_s, out_s[0]);
			ref_sbeafa(&SBE(1), act, in_s, out_s[1]);

			sbe_cmp("after applyFrameAction", trial);

			/*
			 * OBSERVABLE SEPARATION: the bytes the function
			 * wrote, not the arm it took.  Four actions that all
			 * produced the same output would be indistinguishable
			 * and this counter would not move.
			 */
			for (k = 0; k < distinct; k++)
				if (memcmp(seen[k], out_s[1],
					   V90SBE_DECODER_SIZE) == 0)
					break;
			if (k == distinct && distinct < 64)
				memcpy(seen[distinct++], out_s[1],
				       V90SBE_DECODER_SIZE);
		}

	diff_eq_int("the four actions produce distinct outputs", distinct >= 8,
		    1, distinct);

	return diff_end();
}

static int
run_sbe_process(void)
{
	long trial = 310000;
	int wi, st, pat, prev, s;
	int distinct = 0;
	int saw_state_up = 0, saw_state_down = 0;
	unsigned char seen[64][V90SBE_DECODER_SIZE];

	diff_begin("V90SignBitsExtractor::process");

	for (wi = 0; wi < NWIDTH; wi++)
	    for (st = 0; st < 2; st++)
		for (prev = 0; prev < 2; prev++)
		    for (pat = 0; pat < 16; pat++) {
			unsigned int width = width_v[wi];
			unsigned int before;
			int i, k;

			trial++;
			for (i = 0; i < (int)V90SBE_DECODER_SIZE; i++)
				in_s[i] = (unsigned char)
				    ((pat >> i) & 1 ? 0 : (unsigned char)
				     (0x40 + i * 3 + pat));
			for (s = 0; s < 2; s++) {
				sbe_setup(s, width, (unsigned int)st,
					  (unsigned int)prev,
					  0x2c07u + 0x6d0bu *
					  (unsigned)trial);
				fill(out_s[s], NOUT, 0x413fu +
				     (unsigned)trial);
			}
			before = SBE(1).state;

			our_sbeproc(&SBE(0), in_s, out_s[0]);
			ref_sbeproc(&SBE(1), in_s, out_s[1]);

			sbe_cmp("after process", trial);

			/*
			 * THE STATE IS AN OBSERVABLE OUTPUT, and it must go
			 * both ways across the run or the two-state machine
			 * is being tested in one direction only.
			 */
			if (SBE(1).state > before)
				saw_state_up = 1;
			if (SBE(1).state < before)
				saw_state_down = 1;

			for (k = 0; k < distinct; k++)
				if (memcmp(seen[k], out_s[1],
					   V90SBE_DECODER_SIZE) == 0)
					break;
			if (k == distinct && distinct < 64)
				memcpy(seen[distinct++], out_s[1],
				       V90SBE_DECODER_SIZE);
		}

	diff_eq_int("the state rose on some trial", saw_state_up, 1, 0);
	diff_eq_int("and fell on another", saw_state_down, 1, 0);
	diff_eq_int("and the output was not one value throughout",
		    distinct >= 6, 1, distinct);

	return diff_end();
}

/* ============================================================ demapper ==== */

#define CBASE	30000
#define CSTEP	232
#define CROW	16

static short
level_at(int row, int col)
{
	return (short)(CBASE - col * CSTEP - row * CROW);
}

/*
 * Plant one demapper per side.  Nothing here calls the constructor: that is
 * t_v90demapctor's job, and running it would allocate three blocks a trial and
 * hide the seed the comparison rests on.
 */
static void
dem_setup(int s, int adi_side, int dup, unsigned lfsr)
{
	V90Demapper *d = &DEM(s);
	int i;

	lfsr = fill(dem_s[s], DEM_SLOT, lfsr);
	fill((unsigned char *)code_s[s], (int)sizeof code_s[s], lfsr ^ 0x77u);
	fill(sign_s[s], NSAMPLE, lfsr ^ 0x31u);
	fill(sbstate_s[s], V90SBE_DECODER_SIZE, lfsr ^ 0x9bu);
	fill(out_s[s], NOUT, lfsr ^ 0xa5u);

	d->params = PARAMS;
	d->adiDetector = &ADI(adi_side);
	d->codes = code_s[s];
	d->signs = sign_s[s];
	d->sampleCapacity = NSAMPLE;

	/*
	 * A DESCENDING ROW PER CONSTELLATION, which is the order the search
	 * assumes: it walks forward while the level is still at or above the
	 * magnitude.  The offset per row keeps the six rows distinguishable,
	 * so a reconstruction that indexed the wrong one is caught by the
	 * return value and not only by the object.
	 *
	 * THE STEP IS EVEN ON PURPOSE.  Half of it is an exact integer, so
	 * `level_at(p, c) - CSTEP / 2` is exactly equidistant from two
	 * adjacent levels and the tie-break between them is reachable at all.
	 * With an odd step no input can produce a tie and `<` against `<=`
	 * would be untestable.
	 */
	for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++) {
		int j;

		for (j = 0; j < V90DEMAPPER_LEVELS; j++)
			d->constellation[i][j] = level_at(i, j);
		/*
		 * A PLATEAU OF TWO, on half the trials.  A strictly descending
		 * row cannot tell `constellation[code] >= mag` from
		 * `> mag`: the scan stops one code apart and the two-neighbour
		 * comparison that follows puts both back on the same level.
		 * With two adjacent codes holding the SAME level the scan ends
		 * either side of the pair and the two readings choose
		 * different codes, which `codes[]` records.  Rounding two
		 * adjacent mu-law levels to one short is what makes this a
		 * table the class can really hold, not a contrived one.
		 */
		if (dup)
			d->constellation[i][6] = level_at(i, 5);
	}

	d->signBits.decoder.state_ = sbstate_s[s];
	d->signBits.decoder.capacity_ = V90SBE_DECODER_SIZE;
	d->signBits.decoder.size_ = 0u;
	d->signBits.oddDecoder.prev_ = 0;
	d->signDecoder.prev_ = 0;

	memcpy(dem_before[s], dem_s[s], DEM_SLOT);
}

static void
dem_cmp_arrays(long trial)
{
	diff_eq_int("codes (%ld)",
		    memcmp(code_s[0], code_s[1], sizeof code_s[0]) == 0, 1,
		    trial);
	diff_eq_int("signs (%ld)",
		    memcmp(sign_s[0], sign_s[1], sizeof sign_s[0]) == 0, 1,
		    trial);
	diff_eq_int("the extractor's decoder state (%ld)",
		    memcmp(sbstate_s[0], sbstate_s[1], V90SBE_DECODER_SIZE)
		    == 0, 1, trial);
}

static unsigned
hist_total(int s)
{
	V90Demapper *d = &DEM(s);
	unsigned t = 0;
	int i, j;

	for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++)
		for (j = 0; j < V90DEMAPPER_LEVELS; j++)
			t += d->errorCount[i][j];
	return t;
}

/* --------------------------------------------- resetLinearMappStudy ------ */

static int
run_rlms(void)
{
	long trial = 320000;
	int ni, s;
	int changed = 0;
	static const unsigned int n_v[] = { 0u, 1u, 7u, 0x1234u, 0xffffffffu };

	diff_begin("V90Demapper::resetLinearMappStudy");

	for (ni = 0; ni < (int)(sizeof(n_v) / sizeof(n_v[0])); ni++) {
		unsigned lf = 0x6b21u + 0x51a7u * (unsigned)ni;

		trial++;
		for (s = 0; s < 2; s++) {
			/* Each side its own detector: this one WRITES. */
			fill(adi_s[s], (int)sizeof adi_s[s], lf + 0x900u);
			dem_setup(s, s, 0, lf);
		}

		our_rlms(&DEM(0), n_v[ni]);
		ref_rlms(&DEM(1), n_v[ni]);

		cmp_obj("after resetLinearMappStudy", skip_write, DEM_SIZE,
			"V90Demapper", trial);
		diff_eq_int("the detector (%ld)",
			    memcmp(adi_s[0], adi_s[1], sizeof adi_s[0]) == 0,
			    1, trial);
		dem_cmp_arrays(trial);

		/*
		 * THE ARGUMENT LANDS AT +0x1ea8 AND NOWHERE ELSE, asserted
		 * rather than only compared: two reconstructions that both
		 * dropped it would agree (finding 224).
		 */
		diff_eq_int("uint_1ea8 (%ld)", (long)DEM(1).uint_1ea8,
			    (long)n_v[ni], trial);
		diff_eq_int("short_1e9c (%ld)", (long)DEM(1).short_1e9c, 0,
			    trial);
		diff_eq_int("short_1ea4 (%ld)", (long)DEM(1).short_1ea4, 0,
			    trial);
		diff_eq_int("short_1ea6 (%ld)", (long)DEM(1).short_1ea6, 0,
			    trial);
		diff_eq_int("decisionFramePosition (%ld)",
			    (long)DEM(1).decisionFramePosition, 0, trial);
		diff_eq_int("uint_1eb0 (%ld)", (long)DEM(1).uint_1eb0, 0,
			    trial);
		/*
		 * AND +0x1eac IS NOT ONE OF THEM.  `decisionCode` sits between
		 * two words this function clears and is left alone, which a
		 * loop written one field too wide would break.  The seed is
		 * never zero, so this is a real distinction.
		 */
		diff_eq_int("decisionCode is untouched (%ld)",
			    DEM(1).decisionCode ==
			    ((V90Demapper *)dem_before[1])->decisionCode, 1,
			    trial);

		/*
		 * OBSERVABLE: the detector's own bytes moved.  A no-op
		 * `clearCamulativeVal` loop would leave both detectors at
		 * their seed and every comparison above would still pass.
		 */
		if (memcmp(adi_s[1], adi_s[0], sizeof adi_s[0]) == 0 &&
		    DEM(1).uint_1eb0 != ((V90Demapper *)dem_before[1])->
					uint_1eb0)
			changed = 1;
	}

	{
		/*
		 * The 768 cells, checked by value on the last trial: every
		 * (phase, code) pair of both cumulative arrays is zero and the
		 * variance beside them is not, which is what says the loop
		 * bounds are 6 and 128 and not something that happens to
		 * cover them.
		 */
		V90AutoDigitalImpDetector *a = &ADI(1);
		int p, c, allz = 1, anyother = 0;

		for (p = 0; p < V90ADID_PHASES; p++)
			for (c = 0; c < V90ADID_CODES; c++)
				if (a->uint_1c00[p][c] != 0 ||
				    a->float_1000[p][c] != 0.0f)
					allz = 0;
		for (p = 0; p < V90ADID_PHASES; p++)
			for (c = 0; c < V90ADID_CODES; c++)
				if (a->linMapp[p][c] != 0)
					anyother = 1;
		diff_eq_int("all 6 x 128 cumulative cells are clear", allz, 1,
			    0);
		diff_eq_int("and the arrays beside them are not", anyother, 1,
			    0);
	}
	diff_eq_int("the detector and the object both moved", changed, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------- hardDecision ---- */

static const unsigned int csize_v[] = { 1u, 2u, 5u, 33u, 64u };
#define NCSIZE ((int)(sizeof(csize_v) / sizeof(csize_v[0])))

static const short sample_v[] = {
	0, 1, -1, 100, -100, 4000, -4000, 15000, -15000, 29999, -29999,
	32767, -32768, 6931, -6931
};
#define NSAMP ((int)(sizeof(sample_v) / sizeof(sample_v[0])))

/*
 * SIX MORE INPUTS THAT ONLY EXIST RELATIVE TO THE ROW BEING SEARCHED, and
 * they are what make two of the search's comparisons testable at all:
 *
 *   an EXACT level     distinguishes `>=` from `>` in the scan, which decide
 *                      different codes for a sample that lands on a level.
 *   a MIDPOINT         distinguishes `<` from `<=` in the tie-break, which is
 *                      the only input where the two neighbours are equally
 *                      far away.
 *
 * A fixed table cannot carry either, because both depend on the phase.  The
 * last two are the same two negated, so the tie-break is exercised on the arm
 * that also puts a sign back on.
 */
#define NEXTRA	8

static short
extra_sample(int phase, int k)
{
	short lvl;

	if (k >= 6)
		lvl = level_at(phase, 5);	/* the plateau, when there  */
	else					/* is one                   */
		lvl = level_at(phase, (k & 2) ? 3 : 0);
	if (k < 6 && (k & 1))
		lvl = (short)(lvl - CSTEP / 2);
	return (k == 5 || k == 4 || k == 7) ? (short)-lvl : lvl;
}

static int
run_harddec(void)
{
	long trial = 330000;
	int ci, si, phase, flag, dbg, s;
	int saw_pos = 0, saw_neg = 0, saw_zero = 0;
	int saw_refuse = 0, saw_accum = 0, saw_print = 0, saw_delay = 0;
	int distinct = 0;
	short seen[64];

	diff_begin("V90Demapper::hardDecision");

	dsplib_debug_capture_on = 1;

	for (ci = 0; ci < NCSIZE * 2; ci++)
	  for (flag = 0; flag < 2; flag++)
	    for (phase = 0; phase < V90DEMAPPER_CONSTELLATIONS; phase++)
	      for (dbg = 0; dbg < 4; dbg++)
		for (si = 0; si < NSAMP + NEXTRA; si++) {
			unsigned lf = 0x30a7u + 0x4e6du * (unsigned)trial;
			int dup = ci >= NCSIZE;
			unsigned before_hist, after_hist;
			unsigned before_count;
			short in, got0, got1;
			int i, k;

			trial++;
			in = (si < NSAMP) ? sample_v[si]
					  : extra_sample(phase, si - NSAMP);
			dsplibs_debug_level = ref_dsplibs_debug_level =
			    (dbg & 1) ? 2u : 0u;

			/*
			 * One detector, shared: `hardDecision` only READS
			 * `short_2800`, so the two sides store the same
			 * pointer and +0x1ea0 stays in the comparison.
			 */
			fill(adi_s[0], (int)sizeof adi_s[0], lf ^ 0x5eedu);
			for (i = 0; i < V90ADID_PHASES; i++)
				ADI(0).short_2800[i] = (short)(flag ? i + 1
								    : 0);

			fill(parm_s, (int)PARM_SLOT, lf ^ 0x2b1u);
			PARAMS->DEBUG_DEMAPPER_ERROR_HISTOGRAM = (dbg & 2)
								 ? 1 : 0;
			PARAMS->DEMAPPER_ERROR_HISTOGRAM_INTEGRATION_TIME =
			    (int)(si % 3);

			for (s = 0; s < 2; s++) {
				dem_setup(s, 0, dup, lf);
				for (i = 0; i < V90DEMAPPER_CONSTELLATIONS;
				     i++)
					DEM(s).constellationSize[i] =
					    csize_v[ci % NCSIZE];
				DEM(s).rbsFramePosition = (unsigned)phase;
				/*
				 * The refusal arm on one trial in five, and
				 * every other trial a cursor well inside the
				 * arrays.
				 */
				DEM(s).sampleCount =
				    (si == 3) ? NSAMPLE :
				    (si == 4) ? NSAMPLE - 1 :
				    (unsigned)(si % 7);
				DEM(s).histogramDelay = (si % 5 == 1) ? 3 : 0;
				DEM(s).histogramIntegration = (int)(si % 4);
				memset(DEM(s).errorSum, 0,
				       sizeof DEM(s).errorSum);
				memset(DEM(s).errorCount, 0,
				       sizeof DEM(s).errorCount);
				DEM(s).errorHistogramCount = 0;
				memcpy(dem_before[s], dem_s[s], DEM_SLOT);
			}
			before_hist = DEM(1).errorHistogramCount;
			before_count = hist_total(1);
			dsplib_debug_capture_reset();

			got0 = our_harddec(&DEM(0), in);
			got1 = ref_harddec(&DEM(1), in);

			diff_eq_int("return (%ld)", (long)got0, (long)got1,
				    trial);
			cmp_obj("after hardDecision", skip_read, DEM_SIZE,
				"V90Demapper", trial);
			dem_cmp_arrays(trial);
			diff_eq_int("the detector is untouched (%ld)",
				    ADI(0).short_2800[phase] ==
				    (short)(flag ? phase + 1 : 0), 1, trial);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, trial);

			after_hist = DEM(1).errorHistogramCount;

			if (got1 > 0)
				saw_pos = 1;
			if (got1 < 0)
				saw_neg = 1;
			if (got1 == 0)
				saw_zero = 1;
			/*
			 * THE REFUSAL, named by two things a caller sees: the
			 * cursor did not move and a line came out.
			 */
			if (DEM(1).sampleCount ==
			    ((V90Demapper *)dem_before[1])->sampleCount &&
			    dsplibs_debug_level != 0 &&
			    dsplib_debug_capture_text(1)[0] != '\0')
				saw_refuse = 1;
			if (hist_total(1) > before_count)
				saw_accum = 1;
			if (after_hist > before_hist)
				saw_print = 1;
			if (DEM(1).histogramDelay <
			    ((V90Demapper *)dem_before[1])->histogramDelay)
				saw_delay = 1;

			for (k = 0; k < distinct; k++)
				if (seen[k] == got1)
					break;
			if (k == distinct && distinct < 64)
				seen[distinct++] = got1;
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("a positive level came back", saw_pos, 1, 0);
	diff_eq_int("a negative one did too", saw_neg, 1, 0);
	diff_eq_int("and a zero", saw_zero, 1, 0);
	diff_eq_int("the refusal arm printed and moved nothing", saw_refuse, 1,
		    0);
	diff_eq_int("the histogram accumulated on some trial", saw_accum, 1, 0);
	diff_eq_int("was printed and reset on another", saw_print, 1, 0);
	diff_eq_int("and the delay was spent on a third", saw_delay, 1, 0);
	diff_eq_int("the return took many values", distinct >= 16, 1, distinct);

	return diff_end();
}

/* ------------------------------------------------------------ process ---- */

/*
 * `signBitGroups` x `signBitGroupSize` is the six-sample frame, and
 * `signBitsPerFrame` is what the sign path then writes: one bit fewer per
 * group, because the extractor eats the first of each group as its state
 * signal.  Zero groups is the other path entirely -- the serial decoder --
 * and there `signBitsPerFrame` is a plain byte count.
 */
struct shape {
	unsigned int groups;
	unsigned int groupsz;
	unsigned int signbits;
	unsigned int bits;
};

static const struct shape shape_v[] = {
	{ 1u, 6u, 5u, 24u },
	{ 2u, 3u, 4u, 20u },
	{ 3u, 2u, 3u, 18u },
	{ 6u, 1u, 0u, 16u },
	{ 0u, 0u, 6u, 22u },
	{ 0u, 0u, 1u, 12u },
	{ 0u, 0u, 0u, 10u }
};
#define NSHAPE ((int)(sizeof(shape_v) / sizeof(shape_v[0])))

static const unsigned int count_v[] = { 0u, 1u, 5u, 6u, 7u, 12u, 13u, 41u };
#define NCOUNT ((int)(sizeof(count_v) / sizeof(count_v[0])))

static int
run_demproc(void)
{
	long trial = 340000;
	int sh, ct, st, s;
	int saw_true = 0, saw_false = 0, saw_leftover = 0, saw_bits = 0;
	int distinct = 0;
	unsigned seen[64];

	diff_begin("V90Demapper::process");

	for (sh = 0; sh < NSHAPE; sh++)
	  for (ct = 0; ct < NCOUNT; ct++)
	    for (st = 0; st < 3; st++) {
		const struct shape *p = &shape_v[sh];
		unsigned lf = 0x1d47u + 0x3af1u * (unsigned)trial;
		unsigned int nb[2];
		int rc[2];
		unsigned int start;
		int k;

		trial++;
		start = (st == 0) ? 0u : (st == 1) ? 1u : 6u;
		if (start > count_v[ct])
			start = 0u;

		fill(adi_s[0], (int)sizeof adi_s[0], lf ^ 0x4321u);
		fill(parm_s, (int)PARM_SLOT, lf ^ 0x88u);

		for (s = 0; s < 2; s++) {
			dem_setup(s, 0, 0, lf);
			DEM(s).signBitGroups = p->groups;
			DEM(s).signBitGroupSize = p->groupsz;
			DEM(s).signBitsPerFrame = p->signbits;
			DEM(s).bitsPerFrame = p->bits;
			DEM(s).sampleCount = count_v[ct];
			DEM(s).frameStart = start;
			/*
			 * The five moduli and the bit count.  `progress`
			 * multiplies by the first five and writes the sixth
			 * many bytes, so the count is what has to stay inside
			 * the caller's buffer; the rest only have to be the
			 * same on both sides.
			 */
			DEM(s).modulusDecoder.field_00 = 7u;
			DEM(s).modulusDecoder.field_04 = 11u;
			DEM(s).modulusDecoder.field_08 = 5u;
			DEM(s).modulusDecoder.field_0c = 13u;
			DEM(s).modulusDecoder.field_10 = 3u;
			DEM(s).modulusDecoder.field_14 = 0u;
			DEM(s).modulusDecoder.field_18 = 9u;
			/* The embedded extractor, as `reset` would leave it. */
			DEM(s).signBits.spacing = p->groups ? p->groups : 1u;
			DEM(s).signBits.width = p->groupsz;
			DEM(s).signBits.state = 0u;
			DEM(s).signBits.decoder.size_ = 0u;
			DEM(s).signBits.decoder.reset(p->groupsz, 0);
			memcpy(dem_before[s], dem_s[s], DEM_SLOT);
		}
		nb[0] = 0xdeadbeefu;
		nb[1] = 0xdeadbeefu;

		rc[0] = our_demproc(&DEM(0), out_s[0], &nb[0]);
		rc[1] = ref_demproc(&DEM(1), out_s[1], &nb[1]);

		diff_eq_int("return (%ld)", (long)rc[0], (long)rc[1], trial);
		diff_eq_int("nbits (%ld)", (long)nb[0], (long)nb[1], trial);
		cmp_obj("after process", skip_read, DEM_SIZE, "V90Demapper",
			trial);
		dem_cmp_arrays(trial);
		diff_eq_int("the bit buffer (%ld)",
			    memcmp(out_s[0], out_s[1], NOUT) == 0, 1, trial);

		if (rc[1])
			saw_true = 1;
		else
			saw_false = 1;
		if (rc[1] && DEM(1).sampleCount != 0)
			saw_leftover = 1;
		if (nb[1] != 0)
			saw_bits = 1;

		for (k = 0; k < distinct; k++)
			if (seen[k] == nb[1])
				break;
		if (k == distinct && distinct < 64)
			seen[distinct++] = nb[1];

		/*
		 * THE REFUSAL BELOW SIX SAMPLES IS NOT AN EMPTY DRAIN.  On
		 * that arm nothing is compacted and no cursor moves, and the
		 * object says so through more than the return value.
		 */
		if (!rc[1]) {
			diff_eq_int("nbits is zeroed on refusal (%ld)",
				    (long)nb[1], 0, trial);
			diff_eq_int("frameStart is untouched (%ld)",
				    (long)DEM(1).frameStart, (long)start,
				    trial);
			diff_eq_int("sampleCount is untouched (%ld)",
				    (long)DEM(1).sampleCount,
				    (long)count_v[ct], trial);
		} else {
			diff_eq_int("frameStart is rewound (%ld)",
				    (long)DEM(1).frameStart, 0, trial);
		}
	}

	diff_eq_int("the drain ran and returned 1", saw_true, 1, 0);
	diff_eq_int("and refused below six samples", saw_false, 1, 0);
	diff_eq_int("a remainder was carried forward", saw_leftover, 1, 0);
	diff_eq_int("bits came out", saw_bits, 1, 0);
	diff_eq_int("and the count was not one value throughout",
		    distinct >= 5, 1, distinct);

	return diff_end();
}

/* ------------------------------------- incrementRBSFramePosition -------- */

/*
 * THE POSITIONS THAT SEPARATE THE TWO READINGS OF `% 6` ARE THE HIGH ONES,
 * and they are the reason this trivial function gets a suite of its own.
 * Unsigned and signed agree over every value a frame position can really
 * hold; they disagree from 0x80000000 up, where 0xfffffffe + 1 is 3 unsigned
 * and -1 signed.  The blob divides with `mul` and not `idiv`, so the
 * assertion below -- that the answer is always one of the six -- is the
 * blob's own behaviour and fails on the signed reading.
 */
static int
run_incrbs(void)
{
	long trial = 350000;
	int pi, s;
	int distinct = 0;
	unsigned int seen[8];
	static const unsigned int pos_v[] = {
		0u, 1u, 3u, 4u, 5u, 6u, 7u, 11u, 12u, 0x7ffffffeu,
		0x7fffffffu, 0x80000000u, 0xfffffffdu, 0xfffffffeu,
		0xffffffffu
	};

	diff_begin("V90Demapper::incrementRBSFramePosition");

	for (pi = 0; pi < (int)(sizeof(pos_v) / sizeof(pos_v[0])); pi++) {
		unsigned lf = 0x1f3bu + 0x7a5du * (unsigned)pi;
		int k;

		trial++;
		fill(adi_s[0], (int)sizeof adi_s[0], lf ^ 0x2211u);
		fill(parm_s, (int)PARM_SLOT, lf ^ 0x66u);
		for (s = 0; s < 2; s++) {
			dem_setup(s, 0, 0, lf);
			DEM(s).rbsFramePosition = pos_v[pi];
			memcpy(dem_before[s], dem_s[s], DEM_SLOT);
		}

		our_incrbs(&DEM(0));
		ref_incrbs(&DEM(1));

		cmp_obj("after incrementRBSFramePosition", skip_read, DEM_SIZE,
			"V90Demapper", trial);
		dem_cmp_arrays(trial);
		diff_eq_int("the position stays inside the six (%ld)",
			    DEM(1).rbsFramePosition <
			    V90DEMAPPER_CONSTELLATIONS, 1, trial);
		/*
		 * AND NOTHING ELSE OF THE OBJECT MOVED.  `cmp_obj` compares
		 * the two sides against each other; this compares the blob's
		 * side against its own seed, so a reconstruction that touched
		 * a neighbouring word IN THE SAME WAY as the blob would still
		 * be caught by the first and one that touched none would be
		 * caught by neither without this.
		 */
		diff_eq_int("only +0x18 moved (%ld)",
			    memcmp(dem_s[1], dem_before[1], 0x18) == 0 &&
			    memcmp(dem_s[1] + 0x1c, dem_before[1] + 0x1c,
				   DEM_SLOT - 0x1c) == 0, 1, trial);

		for (k = 0; k < distinct; k++)
			if (seen[k] == DEM(1).rbsFramePosition)
				break;
		if (k == distinct && distinct < 8)
			seen[distinct++] = DEM(1).rbsFramePosition;
	}

	diff_eq_int("all six positions came out", distinct, 6, distinct);

	return diff_end();
}

/* --------------------------------------------- updateConstelation ------- */

/*
 * The detector's two cumulative arrays, planted so that the quotient is
 * exactly known and so that BOTH of the body's decisions are reachable:
 *
 *   a count of ZERO on one cell in seven, which is the `test %eax,%eax; je`
 *   skip -- the cell keeps whatever the seed put there, and the seed is never
 *   zero (finding 230), so "skipped" and "written zero" are distinguishable.
 *
 *   a count of TWO against an ODD sum, which puts the quotient exactly half
 *   way between two integers.  That is the only input that separates
 *   `(short)(mean + 0.5f)` under a truncating `fistps` from a plain
 *   round-to-nearest, and it is reached on both signs.
 */
static void
adi_plant_cells(int s, int zpat)
{
	V90AutoDigitalImpDetector *a = &ADI(s);
	int p, c;

	for (p = 0; p < V90ADID_PHASES; p++)
		for (c = 0; c < V90ADID_CODES; c++) {
			a->uint_1c00[p][c] =
			    ((c + p + zpat) % 7 == 0)
			    ? 0u : (unsigned int)(1 + ((c + p) % 4));
			a->float_1000[p][c] =
			    (float)((c * 13 + p * 101) % 2001 - 1000);
		}
}

/*
 * The row lengths.  70 with the detector's flag set gives 140, which is the
 * deliberate overrun: the object indexes `i * 128 + j` with no bound on `j`,
 * so the sixth row runs into `constellationSize` behind it.  Reproducing that
 * is the point -- a reconstruction that clamped `j` to 128 would pass every
 * other trial here.
 */
static const unsigned int usize_v[] = { 0u, 1u, 3u, 64u, 70u };
#define NUSIZE ((int)(sizeof(usize_v) / sizeof(usize_v[0])))

static int
run_updconst(void)
{
	long trial = 360000;
	int ui, flag, zpat, dbg, s;
	int saw_written = 0, saw_skipped = 0, saw_half = 0;
	int saw_double = 0, saw_line = 0, saw_quiet = 0;

	diff_begin("V90Demapper::updateConstelation");

	dsplib_debug_capture_on = 1;

	for (ui = 0; ui < NUSIZE; ui++)
	  for (flag = 0; flag < 3; flag++)
	    for (zpat = 0; zpat < 3; zpat++)
	      for (dbg = 0; dbg < 2; dbg++) {
		unsigned lf = 0x4c1du + 0x2b93u * (unsigned)trial;
		int i;

		trial++;
		dsplibs_debug_level = ref_dsplibs_debug_level = dbg ? 2u : 0u;

		/*
		 * ONE DETECTOR, SHARED: this function only reads it, so the
		 * two sides store the same pointer and +0x1ea0 stays inside
		 * the object comparison.
		 */
		fill(adi_s[0], (int)sizeof adi_s[0], lf ^ 0x5eedu);
		adi_plant_cells(0, zpat);
		for (i = 0; i < V90ADID_PHASES; i++)
			ADI(0).short_2800[i] = (short)
			    (flag == 0 ? 0 : flag == 1 ? i + 1 : (i & 1));

		fill(parm_s, (int)PARM_SLOT, lf ^ 0x2b1u);

		for (s = 0; s < 2; s++) {
			dem_setup(s, 0, 0, lf);
			for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++)
				DEM(s).constellationSize[i] = usize_v[ui];
			memcpy(dem_before[s], dem_s[s], DEM_SLOT);
		}
		memcpy(adi_before, adi_s[0], sizeof adi_before);
		dsplib_debug_capture_reset();

		our_updconst(&DEM(0));
		ref_updconst(&DEM(1));

		cmp_obj("after updateConstelation", skip_read, DEM_SIZE,
			"V90Demapper", trial);
		dem_cmp_arrays(trial);
		diff_eq_int("the detector is untouched (%ld)",
			    memcmp(adi_s[0], adi_before, sizeof adi_before)
			    == 0, 1, trial);
		diff_eq_int("transcript (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);

		/*
		 * OBSERVABLE SEPARATION, off the BLOB's object rather than
		 * restated from our own source (finding 3509).
		 */
		if (dbg) {
			if (dsplib_debug_capture_text(1)[0] != '\0')
				saw_line = 1;
		} else if (dsplib_debug_capture_text(1)[0] == '\0') {
			saw_quiet = 1;
		}

		{
			V90Demapper *d = &DEM(1);
			V90Demapper *b = (V90Demapper *)dem_before[1];
			unsigned int n = usize_v[ui];
			int j;

			for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++)
			    for (j = 0; j < (int)n && j < V90DEMAPPER_LEVELS;
				 j++) {
				unsigned int cnt = ADI(0).uint_1c00[i][j];
				float sum = ADI(0).float_1000[i][j];

				if (cnt == 0) {
					if (d->constellation[i][j] ==
					    b->constellation[i][j])
						saw_skipped = 1;
					continue;
				}
				if (d->constellation[i][j] !=
				    b->constellation[i][j])
					saw_written = 1;
				/*
				 * The exact half.  A quotient of x.5 must come
				 * out as x + 1 and not as the even neighbour.
				 */
				if (cnt == 2u &&
				    ((int)sum & 1) != 0 && sum > 0.0f &&
				    d->constellation[i][j] ==
				    (short)(((int)sum + 1) / 2))
					saw_half = 1;
			    }
			/*
			 * THE DOUBLED BOUND WROTE PAST THE ROW LENGTH.  With
			 * the flag set the row runs to 2 * size, so a cell at
			 * `size` itself moved -- which a reconstruction that
			 * ignored `short_2800` would leave at its seed.
			 */
			if (flag != 0 && n != 0u && n < 64u &&
			    ADI(0).short_2800[0] != 0 &&
			    ADI(0).uint_1c00[0][n] != 0u &&
			    d->constellation[0][n] != b->constellation[0][n])
				saw_double = 1;
		}
	      }

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0u;

	diff_eq_int("cells with a count were rewritten", saw_written, 1, 0);
	diff_eq_int("and cells without one were left alone", saw_skipped, 1,
		    0);
	diff_eq_int("an exact half rounded away from zero", saw_half, 1, 0);
	diff_eq_int("the flag doubled the row", saw_double, 1, 0);
	diff_eq_int("the diagnostic printed at level 2", saw_line, 1, 0);
	diff_eq_int("and said nothing at level 0", saw_quiet, 1, 0);

	return diff_end();
}

/* ------------------------------------------------- resetNoSpectral ------ */

/*
 * The detector's two mapping tables, planted so that the LARGER-FIRST rule
 * has all three cases: `linMapp` above, below and EXACTLY EQUAL to
 * `linMappAlt`.  The equal case is the only input that separates the object's
 * `>` (which puts `linMappAlt` first) from a `>=` (which would not), and no
 * amount of varied seeding produces it by accident.
 */
static void
adi_plant_maps(int s, int eqpat)
{
	V90AutoDigitalImpDetector *a = &ADI(s);
	int p, c;

	for (p = 0; p < V90ADID_PHASES; p++)
		for (c = 0; c < V90ADID_CODES; c++) {
			short lo = (short)(c * 7 + p * 3 - 400);
			short hi = (short)(lo + 1 + ((c + p) % 5) * 40);

			if ((c + eqpat) % 6 == 0) {
				a->linMapp[p][c] = lo;
				a->linMappAlt[p][c] = lo;
			} else if ((c + eqpat) % 3 == 0) {
				a->linMapp[p][c] = lo;
				a->linMappAlt[p][c] = hi;
			} else {
				a->linMapp[p][c] = hi;
				a->linMappAlt[p][c] = lo;
			}
		}
}

static const unsigned int msize_v[] = { 0u, 1u, 2u, 17u, 63u };
#define NMSIZE ((int)(sizeof(msize_v) / sizeof(msize_v[0])))

static int
run_resetns(void)
{
	long trial = 370000;
	int mi, flag, eqpat, hist, dbg, s;
	int saw_hist = 0, saw_nohist = 0, saw_pair = 0, saw_plain = 0;
	int saw_equal = 0, saw_spill = 0;

	diff_begin("V90Demapper::resetNoSpectral");

	dsplib_debug_capture_on = 1;

	for (mi = 0; mi < NMSIZE; mi++)
	  for (flag = 0; flag < 3; flag++)
	    for (eqpat = 0; eqpat < 3; eqpat++)
	      for (hist = 0; hist < 2; hist++)
		for (dbg = 0; dbg < 2; dbg++) {
			unsigned lf = 0x6d31u + 0x1a4bu * (unsigned)trial;
			int i;

			trial++;
			dsplibs_debug_level = ref_dsplibs_debug_level =
			    dbg ? 2u : 0u;

			fill(adi_s[0], (int)sizeof adi_s[0], lf ^ 0x11a7u);
			adi_plant_maps(0, eqpat);
			for (i = 0; i < V90ADID_PHASES; i++)
				ADI(0).short_2800[i] = (short)
				    (flag == 0 ? 0 : flag == 1 ? i + 1
							       : (i & 1));

			fill(parm_s, (int)PARM_SLOT, lf ^ 0x4c9u);
			PARAMS->DEBUG_DEMAPPER_ERROR_HISTOGRAM = hist;
			PARAMS->DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM =
			    (int)(trial % 97) - 13;

			/*
			 * The mapping block, shared.  The byte tables keep
			 * their varied seed: a code of 128 or more indexes
			 * past its own row of `linMapp` into the next one,
			 * which is what the object does and is worth
			 * exercising rather than avoiding.
			 */
			fill(mapp_s, (int)MAPP_SLOT, lf ^ 0x3fe1u);
			MAPP->word_0 = (unsigned int)(trial % 61) + 3u;
			for (i = 0; i < V90_CONSTELLATIONS; i++)
				MAPP->constellationSize[i] =
				    msize_v[(mi + i) % NMSIZE];

			for (s = 0; s < 2; s++) {
				dem_setup(s, 0, 0, lf);
				/*
				 * THE ROW LENGTHS THIS FUNCTION FINDS, not the
				 * ones it leaves: `printErrorHistogramAndReset`
				 * runs its print loop off them before anything
				 * is rewritten, so a varied seed here is an
				 * unbounded loop rather than a hard trial.
				 * Zero on one phase in five reaches the
				 * six-way guard's other arm.
				 */
				for (i = 0; i < V90DEMAPPER_CONSTELLATIONS;
				     i++)
					DEM(s).constellationSize[i] =
					    (unsigned int)((trial + i) % 5);
				DEM(s).signBitsPerFrame =
				    (unsigned int)(trial % 7);
				DEM(s).histogramDelay = 0x5a5a;
				DEM(s).histogramIntegration = 0x3c3c;
				/*
				 * `dem_setup` leaves the serial decoder at
				 * zero because every other member here wants
				 * it there, and zero is what this function
				 * WRITES -- so a seed of zero cannot tell a
				 * store from an omission.  Finding 230, at
				 * the one byte the general seeding misses.
				 */
				DEM(s).signDecoder.prev_ = 0xa7;
				memset(DEM(s).errorSum, 0,
				       sizeof DEM(s).errorSum);
				memset(DEM(s).errorCount, 0,
				       sizeof DEM(s).errorCount);
				DEM(s).errorHistogramCount = 0;
				memcpy(dem_before[s], dem_s[s], DEM_SLOT);
			}
			dsplib_debug_capture_reset();

			our_resetns(&DEM(0), MAPP);
			ref_resetns(&DEM(1), MAPP);

			cmp_obj("after resetNoSpectral", skip_read, DEM_SIZE,
				"V90Demapper", trial);
			dem_cmp_arrays(trial);
			diff_eq_int("the mapping block is untouched (%ld)",
				    MAPP->word_0 ==
				    (unsigned int)(trial % 61) + 3u, 1, trial);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, trial);

			{
				V90Demapper *d = &DEM(1);
				unsigned int w08;

				/*
				 * THE FOUR SCALARS, ASSERTED AND NOT ONLY
				 * COMPARED: two reconstructions that both
				 * dropped the argument would agree with each
				 * other (finding 224).
				 */
				diff_eq_int("bitsPerFrame (%ld)",
					    (long)d->bitsPerFrame,
					    (long)MAPP->word_0, trial);
				w08 = MAPP->word_0 - d->signBitsPerFrame;
				diff_eq_int("word_08 (%ld)", (long)d->word_08,
					    (long)w08, trial);
				diff_eq_int("modulusDecoder tail (%ld)",
					    (long)d->modulusDecoder.field_18,
					    (long)w08, trial);
				diff_eq_int("signDecoder cleared (%ld)",
					    (long)d->signDecoder.prev_, 0,
					    trial);
				for (i = 0; i < V90DEMAPPER_CONSTELLATIONS;
				     i++) {
					diff_eq_int("constellationSize (%ld)",
						    (long)
						    d->constellationSize[i],
						    (long)
						    MAPP->constellationSize[i],
						    trial);
					diff_eq_int("modulus word (%ld)",
						    (long)(&d->modulusDecoder.
							   field_00)[i],
						    (long)
						    d->constellationSize[i],
						    trial);
				}

				if (hist) {
					diff_eq_int("the delay is reseeded"
						    " (%ld)",
						    (long)d->histogramDelay,
						    (long)PARAMS->
						    DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM,
						    trial);
					diff_eq_int("the integration restarts"
						    " (%ld)",
						    (long)
						    d->histogramIntegration, 0,
						    trial);
					if (d->errorHistogramCount != 0u)
						saw_hist = 1;
				} else {
					diff_eq_int("the delay is untouched"
						    " (%ld)",
						    (long)d->histogramDelay,
						    0x5a5a, trial);
					diff_eq_int("the histogram did not"
						    " run (%ld)",
						    (long)
						    d->errorHistogramCount, 0,
						    trial);
					saw_nohist = 1;
				}

				/*
				 * THE ORDER OF THE PAIR IS THE OBSERVABLE.
				 * With the flag set each code yields two
				 * levels, larger first; without it, one.
				 */
				for (i = 0; i < V90DEMAPPER_CONSTELLATIONS;
				     i++) {
					unsigned int n =
					    MAPP->constellationSize[i];
					unsigned char c0;

					if (n == 0u)
						continue;
					c0 = MAPP->constellation[i][0];
					if (ADI(0).short_2800[i] != 0) {
						if (d->constellation[i][0] >=
						    d->constellation[i][1])
							saw_pair = 1;
						if (ADI(0).linMapp[0][
						      i * V90ADID_CODES + c0]
						    == ADI(0).linMappAlt[0][
						      i * V90ADID_CODES + c0])
							saw_equal = 1;
					} else if (d->constellation[i][0] ==
						   ADI(0).linMapp[0][
						     i * V90ADID_CODES + c0]) {
						saw_plain = 1;
					}
					if (c0 >= V90ADID_CODES)
						saw_spill = 1;
				}
			}
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0u;

	diff_eq_int("the histogram branch ran", saw_hist, 1, 0);
	diff_eq_int("and was skipped on other trials", saw_nohist, 1, 0);
	diff_eq_int("the doubled arm laid the pair down larger first",
		    saw_pair, 1, 0);
	diff_eq_int("the plain arm copied linMapp straight over", saw_plain, 1,
		    0);
	diff_eq_int("two equal levels were reached", saw_equal, 1, 0);
	diff_eq_int("a code of 128 or more spilled into the next row",
		    saw_spill, 1, 0);

	return diff_end();
}

/* ----------------------------------------------- linearMappingStudy ----- */

/*
 * THE ARGUMENT PAIRS, AND WHY THESE.  The first thing the function does is
 * `|sample| - |level|` compared against ZERO, and the branch is `jbe` on the
 * fall-through, so the test is strictly greater.  A pair with EQUAL
 * magnitudes is the only input that separates `>` from `>=`, and it is
 * reached from both signs.  The rest straddle the 0.4 gate: with a row step
 * of CSTEP the gate opens at 0.4 * CSTEP, so a difference either side of that
 * decides whether the cell is accumulated at all.
 */
static const short lms_x[] = {
	 0,   100, -100,  100,  -100,  4000, -4000,  92,  -92,   93,
	 -93, 500,  -500, 30000, -30000
};
static const short lms_y[] = {
	 0,   100,  100, -100,   100,  3000, -3000,   0,    0,    0,
	   0, 600,   400,  4000,   4000
};
#define NLMS ((int)(sizeof(lms_x) / sizeof(lms_x[0])))

static int
run_lms(void)
{
	long trial = 380000;
	int xi, code, flag, prog, dup, dbg, s;
	int saw_accum = 0, saw_skip = 0, saw_end = 0, saw_mid = 0;
	int saw_two = 0, saw_line = 0, saw_below = 0;
	static const int code_v[] = { 0, 1, 5, 6, 31 };
	static const unsigned int lsize_v[] = { 1u, 7u, 32u };

	diff_begin("V90Demapper::linearMappingStudy");

	dsplib_debug_capture_on = 1;

	for (xi = 0; xi < NLMS; xi++)
	  for (code = 0; code < (int)(sizeof(code_v) / sizeof(code_v[0]));
	       code++)
	    for (flag = 0; flag < 2; flag++)
	      for (prog = 0; prog < 3; prog++)
		for (dup = 0; dup < 2; dup++)
		  for (dbg = 0; dbg < 2; dbg++) {
			unsigned lf = 0x2a9du + 0x53b7u * (unsigned)trial;
			int phase = (int)(trial % V90DEMAPPER_CONSTELLATIONS);
			int i;
			unsigned int cnt_before, cnt_after;

			trial++;
			dsplibs_debug_level = ref_dsplibs_debug_level =
			    dbg ? 2u : 0u;

			fill(parm_s, (int)PARM_SLOT, lf ^ 0x71cu);

			for (s = 0; s < 2; s++) {
				/*
				 * EACH SIDE ITS OWN DETECTOR: this one
				 * WRITES, both into the two cumulative cells
				 * and, on the end-of-run pass, over all 768
				 * of them through `clearCamulativeVal`.
				 */
				fill(adi_s[s], (int)sizeof adi_s[s],
				     lf ^ 0x9a1u);
				adi_plant_cells(s, xi);
				for (i = 0; i < V90ADID_PHASES; i++)
					ADI(s).short_2800[i] =
					    (short)(flag ? i + 1 : 0);

				dem_setup(s, s, dup, lf);
				for (i = 0; i < V90DEMAPPER_CONSTELLATIONS;
				     i++)
					DEM(s).constellationSize[i] =
					    lsize_v[(xi + i) % 3];
				DEM(s).decisionFramePosition = (short)phase;
				DEM(s).decisionCode = (short)code_v[code];
				DEM(s).uint_1ea8 = 4u;
				DEM(s).uint_1eb0 = (unsigned int)
				    (prog == 0 ? 0 : prog == 1 ? 2 : 3);
				DEM(s).short_1e9c = (short)(xi & 1);
				DEM(s).short_1ea4 = 0;
				DEM(s).short_1ea6 = 0;
				memcpy(dem_before[s], dem_s[s], DEM_SLOT);
			}
			cnt_before = ADI(1).uint_1c00[phase][code_v[code]];
			dsplib_debug_capture_reset();

			our_lms(&DEM(0), lms_x[xi], lms_y[xi]);
			ref_lms(&DEM(1), lms_x[xi], lms_y[xi]);

			cmp_obj("after linearMappingStudy", skip_write,
				DEM_SIZE, "V90Demapper", trial);
			diff_eq_int("the detector (%ld)",
				    memcmp(adi_s[0], adi_s[1], sizeof adi_s[0])
				    == 0, 1, trial);
			dem_cmp_arrays(trial);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, trial);

			cnt_after = ADI(1).uint_1c00[phase][code_v[code]];

			if (prog == 2) {
				V90Demapper *d = &DEM(1);
				V90Demapper *b =
				    (V90Demapper *)dem_before[1];
				int p, c, allz = 1;

				/*
				 * THE END OF A RUN, and every part of it is
				 * asserted separately: the progress rewinds,
				 * the per-run flag goes up, the run counter
				 * moves, and all 768 cumulative cells are
				 * clear -- which is `updateConstelation`
				 * having run BEFORE `clearCamulativeVal`
				 * wiped what it read.
				 */
				diff_eq_int("progress rewound (%ld)",
					    (long)d->uint_1eb0, 0, trial);
				diff_eq_int("short_1ea4 raised (%ld)",
					    (long)d->short_1ea4, 1, trial);
				diff_eq_int("short_1e9c advanced (%ld)",
					    (long)d->short_1e9c,
					    (long)(short)(b->short_1e9c + 1),
					    trial);
				diff_eq_int("short_1ea6 tracks the second run"
					    " (%ld)",
					    (long)d->short_1ea6,
					    (long)(d->short_1e9c == 2 ? 1 : 0),
					    trial);
				for (p = 0; p < V90ADID_PHASES; p++)
					for (c = 0; c < V90ADID_CODES; c++)
						if (ADI(1).uint_1c00[p][c] !=
						    0u ||
						    ADI(1).float_1000[p][c] !=
						    0.0f)
							allz = 0;
				diff_eq_int("all 768 cells cleared (%ld)",
					    allz, 1, trial);
				saw_end = 1;
				if (d->short_1ea6 != 0)
					saw_two = 1;
				if (dbg &&
				    dsplib_debug_capture_text(1)[0] != '\0')
					saw_line = 1;
			} else {
				diff_eq_int("progress advanced (%ld)",
					    (long)DEM(1).uint_1eb0,
					    (long)(((V90Demapper *)
						    dem_before[1])->uint_1eb0
						   + 1u), trial);
				diff_eq_int("short_1ea4 untouched (%ld)",
					    (long)DEM(1).short_1ea4, 0, trial);
				if (cnt_after == cnt_before + 1u)
					saw_accum = 1;
				if (cnt_after == cnt_before)
					saw_skip = 1;
				/*
				 * THE MID-ROW PAIR WITH A ZERO GAP.  With the
				 * plateau planted, code 6 and an exactly
				 * equal magnitude give `|diff| == 0` against
				 * a gap of 0 on the arm the object does NOT
				 * take and a gap of 2 * CSTEP on the arm it
				 * does.  That pair is what separates `> 0`
				 * from `>= 0` and `<` from `<=` at once.
				 */
				if (dup && code_v[code] == 6 &&
				    lms_x[xi] == lms_y[xi] &&
				    cnt_after != cnt_before)
					saw_mid = 1;
				if (code_v[code] == 0 &&
				    DEM(1).constellationSize[phase] == 1u &&
				    !flag)
					saw_below = 1;
			}
		  }

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0u;

	diff_eq_int("a sample was accumulated", saw_accum, 1, 0);
	diff_eq_int("and another was rejected by the gate", saw_skip, 1, 0);
	diff_eq_int("a run completed", saw_end, 1, 0);
	diff_eq_int("and a second one raised +0x1ea6", saw_two, 1, 0);
	diff_eq_int("the zero-gap pair was reached", saw_mid, 1, 0);
	diff_eq_int("the below-the-row read was reached", saw_below, 1, 0);
	diff_eq_int("the update diagnostic printed at level 2", saw_line, 1,
		    0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_apply();
	rc |= run_sbe_process();
	rc |= run_rlms();
	rc |= run_harddec();
	rc |= run_demproc();
	rc |= run_incrbs();
	rc |= run_updconst();
	rc |= run_resetns();
	rc |= run_lms();

	return rc;
}
