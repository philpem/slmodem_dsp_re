/*
 * t_v90leaves.cpp -- differential test of six one-method C++ leaves.
 *
 *     V90ConstellationDesigner::setMinMaxRates(unsigned, unsigned)
 *     V90SdDetector::reset()
 *     V90SpectralVerifier::reset()
 *     V92EchoCanceller::setEchoDelay(unsigned)
 *     ResamplerTimingOffset::setTimingOffset(float)
 *     V90Phase4Modulator::setSessionFlag(unsigned)
 *
 * The fixture is t_v90jd.cpp's, with one addition that the classes here need
 * and the Jd family did not.
 *
 * WHAT A WHOLE-OBJECT COMPARISON CANNOT SAY, AND WHAT `only_wrote` ADDS.
 * `diff_eq_obj` proves our object and the blob's ended up identical.  It does
 * NOT prove the header's offsets are the blob's: five of these six write two
 * or three words into an object of tens or thousands of bytes, and if a field
 * were declared four bytes off, BOTH sides would still write four bytes off
 * -- ours because the header says so, the blob's because our test wrote its
 * seed through the same header.  So every class here also asserts what the
 * BLOB's object did, against the seed it started from: which four-byte words
 * moved, by absolute offset, with no reference to a field name.  A word that
 * moves outside the declared set is a failure, and a declared word that never
 * moves across the whole run is a failure too -- that is findings F223 and
 * F224's rule, that a field the function never writes proves nothing, turned
 * into a check rather than a hope.
 *
 * THE OBJECTS ARE NEVER ZEROED (finding F230).  Both sides get the same varied
 * pseudorandom bytes before every call and are reseeded every trial, so a
 * store that fails to happen is visible and a store of zero into memory that
 * was already zero is not mistaken for one.  Four seed modes, because a fill
 * whose low bits are constant exercises nothing that branches on them.
 *
 * EACH OBJECT SITS IN A UNION WITH A LARGER BYTE ARRAY, and the bytes past
 * the object are compared separately, so a store past the end shows up as a
 * failure rather than as silence.  Two of the six -- `V90SdDetector` and
 * `V90SpectralVerifier` -- have had their constructors and destructors
 * reconstructed since, and a class with either cannot be a union MEMBER, so
 * those two unions now hold storage and alignment only and the object is
 * reached through a cast (`SD_A`, `SV_B` and the rest).  The rest are still
 * trivial enough to live in one (docs/v90cpp.md).
 *
 * THE DIAGNOSTICS ARE COMPARED AS TEXT.  Three of the six print, and
 * `dsplibs_debug_level` ships at zero, so a wrong format string behaves
 * exactly like a right one in every other check (finding F180).  Both sides'
 * levels are swept 0..2 together and the two transcripts compared; at level 2
 * they must be non-empty, and below the gate both must be silent.  `edprintf`
 * encodes its output, so a transcript that matches is a format string, an
 * argument and a character count that all match.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * the ref_-prefixed mangled name as an identifier -- see t_v90jd.cpp.  The
 * convention is plain cdecl with `this` as the first stack argument (finding
 * F215), so no attribute is involved.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90SdDetector.h"
#include "dsplib/V90SpectralVerifier.h"
#include "dsplib/V92EchoCanceller.h"
/* `V92EchoCanceller::reset` resets the FloatARMA at its +0x04. */
#include "dsplib/FloatARMA.h"
#include "dsplib/ResamplerTimingOffset.h"
#include "dsplib/V90Phase4Modulator.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/K56FlexFloModem.h"
/*
 * The NAMED V90Parameters map: `V90ConstellationDesigner::reset` and
 * `V90ConnectionEvaluator::reset` read twenty slots out of the parameter
 * block between them, and this test seeds those slots by name.  Finding
 * F1112 is why only one of the two definitions may be included.
 */
#include "dsplib/V90Parameters.h"
/*
 * The demapper's destructor path.  `V90Demapper.h` and
 * `V90SignBitsExtractor.h` forward-declare `V90Parameters` rather than
 * defining it, so both are safe to include after the named map -- finding
 * F1112 again.
 */
#include "dsplib/sysdep.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90SignBitsExtractor.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_cd_setMinMaxRates(void *self, unsigned int lo, unsigned int hi)
	asm("ref__ZN24V90ConstellationDesigner14setMinMaxRatesEjj");
void ref_sd_reset(void *self) asm("ref__ZN13V90SdDetector5resetEv");
void ref_sv_reset(void *self) asm("ref__ZN19V90SpectralVerifier5resetEv");
void ref_ec_setEchoDelay(void *self, unsigned int d)
	asm("ref__ZN16V92EchoCanceller12setEchoDelayEj");
void ref_ec_reset(void *self) asm("ref__ZN16V92EchoCanceller5resetEv");
/*
 * The state machine and the two signal paths.  `setState` takes the enum,
 * which is `int`-wide and passed as one word; declaring the parameter `int`
 * here lets the sweep pass values outside the four enumerators without a cast
 * at every call.
 */
void ref_ec_setState(void *self, int newState)
	asm("ref__ZN16V92EchoCanceller8setStateE21V92EchoCancellerState");
void ref_ec_update(void *self, float *in, unsigned int count)
	asm("ref__ZN16V92EchoCanceller17updateEchoHistoryEPfj");
void ref_ec_process(void *self, float *in, float *out, unsigned int count)
	asm("ref__ZN16V92EchoCanceller7processEPfS0_j");
/*
 * `updateEchoHistory` runs every sample through the ARMA at +0x04, so its
 * test needs a REAL one on each side -- `FloatARMA::process(float)` is
 * differentially tested by t_floatarma.cpp, and what is unproven here is that
 * this class hands it the right pointer and the right sample.  Both sides
 * build theirs with their own constructor, from the same coefficients.
 */
void our_arma_ctor(void *self, unsigned int nDen, unsigned int nNum,
		   float *den, float *num, unsigned int blockSize)
	asm("_ZN9FloatARMAC1EjjPfS0_j");
void ref_arma_ctor(void *self, unsigned int nDen, unsigned int nNum,
		   float *den, float *num, unsigned int blockSize)
	asm("ref__ZN9FloatARMAC1EjjPfS0_j");
void our_arma_dtor(void *self) asm("_ZN9FloatARMAD1Ev");
void ref_arma_dtor(void *self) asm("ref__ZN9FloatARMAD1Ev");
/*
 * BOTH SIDES BY MANGLED NAME, for the reason the connection evaluator's
 * lifecycle block gives: a destructor written as `p->~V92EchoCanceller()` on
 * our side and as a symbol on the blob's is not the same call, and the two
 * must be exactly symmetric.  `D1` and `D2` are 195 bytes each in the blob and
 * byte-identical to each other; both are driven, so the second copy is
 * measured rather than assumed to be a copy of its twin (finding F1270).
 */
void our_ec_dtor(void *self) asm("_ZN16V92EchoCancellerD1Ev");
void ref_ec_dtor(void *self) asm("ref__ZN16V92EchoCancellerD1Ev");
void our_ec_dtor2(void *self) asm("_ZN16V92EchoCancellerD2Ev");
void ref_ec_dtor2(void *self) asm("ref__ZN16V92EchoCancellerD2Ev");
void ref_rt_setTimingOffset(void *self, float ppm)
	asm("ref__ZN21ResamplerTimingOffset15setTimingOffsetEf");
void ref_p4_setSessionFlag(void *self, unsigned int f)
	asm("ref__ZN18V90Phase4Modulator14setSessionFlagEj");

/* Task #88's lifecycle members. */
void ref_cd_reset(void *self)
	asm("ref__ZN24V90ConstellationDesigner5resetEv");
void ref_ce_reset(void *self)
	asm("ref__ZN22V90ConnectionEvaluator5resetEv");
void ref_k56_externalReset(void *self)
	asm("ref__ZN15K56FlexFloModem13externalResetEv");

/*
 * THE CONSTRUCTOR AND DESTRUCTOR ARE REACHED BY THEIR MANGLED NAMES ON
 * BOTH SIDES, ours as well as the blob's.  A constructor cannot be
 * called on an existing buffer in C++ without placement new, and the
 * tree builds -nostdinc++ with no <new>; naming the symbol is what the
 * ABI does anyway, and it keeps the two sides exactly symmetric.
 */
void our_ce_ctor(void *self, void *params)
	asm("_ZN22V90ConnectionEvaluatorC1EP13V90Parameters");
void ref_ce_ctor(void *self, void *params)
	asm("ref__ZN22V90ConnectionEvaluatorC1EP13V90Parameters");
void our_ce_dtor(void *self) asm("_ZN22V90ConnectionEvaluatorD1Ev");
void ref_ce_dtor(void *self) asm("ref__ZN22V90ConnectionEvaluatorD1Ev");

/*
 * The V90Demapper destructor path.  Both sides are reached by their mangled
 * names for the reason given above: the two must be exactly symmetric, and
 * a destructor called as `p->~V90Demapper()` on one side and as a symbol on
 * the other is not.
 */
void our_dem_dtor(void *self) asm("_ZN11V90DemapperD1Ev");
void ref_dem_dtor(void *self) asm("ref__ZN11V90DemapperD1Ev");
void our_dem_hist(void *self)
	asm("_ZN11V90Demapper27printErrorHistogramAndResetEv");
void ref_dem_hist(void *self)
	asm("ref__ZN11V90Demapper27printErrorHistogramAndResetEv");
void our_sbe_ctor(void *self) asm("_ZN20V90SignBitsExtractorC1Ev");
void ref_sbe_ctor(void *self) asm("ref__ZN20V90SignBitsExtractorC1Ev");
void our_sbe_dtor(void *self) asm("_ZN20V90SignBitsExtractorD1Ev");
void ref_sbe_dtor(void *self) asm("ref__ZN20V90SignBitsExtractorD1Ev");

/*
 * THE C2/D2 VARIANTS ARE SEPARATE FUNCTIONS IN THE BLOB and are driven on
 * their own trials.  GCC emits the base-object and complete-object forms as
 * two symbols at ONE address for these three classes -- `nm` gives our D1 and
 * D2 the same value -- so on our side the alternation changes nothing, and on
 * the blob's it is 189 more bytes of `.text` that some test drives against
 * the object rather than being assumed to be a copy of its twin.
 */
void our_dem_dtor2(void *self) asm("_ZN11V90DemapperD2Ev");
void ref_dem_dtor2(void *self) asm("ref__ZN11V90DemapperD2Ev");
void our_sbe_ctor2(void *self) asm("_ZN20V90SignBitsExtractorC2Ev");
void ref_sbe_ctor2(void *self) asm("ref__ZN20V90SignBitsExtractorC2Ev");
void our_sbe_dtor2(void *self) asm("_ZN20V90SignBitsExtractorD2Ev");
void ref_sbe_dtor2(void *self) asm("ref__ZN20V90SignBitsExtractorD2Ev");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned char
next_byte(int mode, unsigned i)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0xa5;			/* every low bit set     */
	case 2:
		return 0x5a;			/* every low bit clear   */
	case 3:
		return (unsigned char)((lfsr & 0xfe) | (unsigned)(i & 1u));
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

/* The same varied bytes into both sides.  Never zeros -- finding F230. */
static void
fill_pair(void *a, void *b, unsigned n, int trial, int mode)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	unsigned i;

	lfsr = 0x1234u + 0x9e37u * (unsigned)trial + 0x51edu * (unsigned)mode;
	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte(mode, i);
}

/*
 * Which four-byte words of the BLOB's object moved, by absolute offset.
 *
 * Returns the number that moved and are not in `allow`; marks `seen[k]` for
 * each allowed offset that did move, so the caller can assert at the end of
 * the run that every field it declared is one the function really writes.
 * `first_bad` gets the offset of the first unexpected word, because that is
 * the number `tools/whichfield.py` wants.
 */
static int
only_wrote(const unsigned char *before, const unsigned char *after,
	   unsigned n, const int *allow, int nallow, int *seen, int *first_bad)
{
	unsigned i;
	int bad = 0;

	*first_bad = -1;
	for (i = 0; i + 4 <= n; i += 4) {
		int k, ok = 0;

		if (memcmp(before + i, after + i, 4) == 0)
			continue;
		for (k = 0; k < nallow; k++)
			if (allow[k] == (int)i) {
				ok = 1;
				if (seen != 0)
					seen[k] = 1;
			}
		if (!ok) {
			if (*first_bad < 0)
				*first_bad = (int)i;
			bad++;
		}
	}
	return bad;
}

/* Both sides' levels move together, or they take different branches. */
static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static int transcripts_seen;		/* asserted once, at the end of main */

/*
 * The transcript checks every printing member repeats.  `lines` is what
 * finding F149 says to count: text alone can be filled by the harness.
 */
static void
check_transcript(unsigned lvl, long tag, int *printed)
{
	diff_eq_int("transcript matches (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("line counts match (%ld)",
		    (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), tag);
	if (lvl > 1) {
		diff_eq_int("above the gate the blob printed (%ld)",
			    dsplib_debug_capture_lines(1) > 0, 1, tag);
		*printed = 1;
		transcripts_seen = 1;
	} else {
		diff_eq_int("below the gate ours was silent (%ld)",
			    (int)dsplib_debug_capture_lines(0), 0, tag);
		diff_eq_int("below the gate the blob was silent (%ld)",
			    (int)dsplib_debug_capture_lines(1), 0, tag);
	}
}

/* ------------------------------------- V90ConstellationDesigner (84 bytes) */

#define CD_SLOT 128

/*
 * STORAGE PLUS A CAST, WHERE THIS WAS A UNION OF THE CLASS AND A BYTE ARRAY.
 * `V90ConstellationDesigner` gained a user-declared constructor and destructor
 * when they were reconstructed, and a union may not hold a member with a
 * non-trivial one.  The alias below is the same reinterpretation the union
 * performed, and it is what this fixture wants anyway: seeded storage that no
 * constructor has run over.
 */
struct cd_slot {
	unsigned char raw[CD_SLOT];
} __attribute__((aligned(8)));

static struct cd_slot cd_a, cd_b;

#define cd_a_o	(*(V90ConstellationDesigner *)cd_a.raw)
#define cd_b_o	(*(V90ConstellationDesigner *)cd_b.raw)

static int
run_cd(void)
{
	/* The ladder the constructor's own defaults sit at either end of. */
	static const unsigned int rate[] = {
		0u, 1u, 28000u, 31200u, 33600u, 45333u, 56000u, 0xffffffffu
	};
	static const int allow[] = { 0x4c, 0x50 };
	int seen[2] = { 0, 0 };
	int trial, printed = 0;
	unsigned lvl;

	diff_begin("V90ConstellationDesigner::setMinMaxRates");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 32; trial++) {
			unsigned char before[CD_SLOT];
			unsigned int lo = rate[trial & 7];
			unsigned int hi = rate[(trial * 5 + 3) & 7];
			long tag = (long)lvl * 1000 + trial;
			int bad, first;

			fill_pair(cd_a.raw, cd_b.raw, CD_SLOT, trial,
				  trial & 3);
			memcpy(before, cd_b.raw, CD_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			cd_a_o.setMinMaxRates(lo, hi);
			ref_cd_setMinMaxRates(&cd_b_o, lo, hi);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after setMinMaxRates",
				    V90ConstellationDesigner,
				    &cd_a_o, &cd_b_o, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(cd_a.raw + sizeof(cd_a_o),
					   cd_b.raw + sizeof(cd_b_o),
					   CD_SLOT - sizeof(cd_a_o)) == 0,
				    1, tag);

			/* What the BLOB's object did, by absolute offset. */
			bad = only_wrote(before, cd_b.raw, CD_SLOT, allow, 2,
					 seen, &first);
			diff_eq_int("the blob wrote outside +0x4c/+0x50 at "
				    "+0x%lx", bad == 0 ? -1 : first, -1, tag);

			/*
			 * And that those two words are the ones the header
			 * names -- read out of the BLOB's object through our
			 * own declaration, which is the claim being made.
			 */
			diff_eq_int("blob's minRate (%ld)",
				    (long)cd_b_o.minRate, (long)lo, tag);
			diff_eq_int("blob's maxRate (%ld)",
				    (long)cd_b_o.maxRate, (long)hi, tag);

			check_transcript(lvl, tag, &printed);
		}
	}

	set_level(0);
	diff_eq_int("minRate is a word the function writes", seen[1], 1, 0);
	diff_eq_int("maxRate is a word the function writes", seen[0], 1, 0);
	diff_eq_int("the diagnostics were reached", printed, 1, 0);

	return diff_end();
}

/* --------------------------------------------- V90SdDetector (28 bytes) */

#define SD_SLOT 64
#define SD_HIST 24

/*
 * The union holds the STORAGE and the alignment only.  `V90SdDetector` has
 * a user-declared constructor and destructor since its lifecycle was
 * reconstructed, and a class with either cannot be a union member; the object
 * is reached through a cast instead, which is also what keeps the "seeded,
 * never zeroed" property this file depends on.
 */
union sd_slot {
	double align;
	unsigned char raw[SD_SLOT];
};

static union sd_slot sd_a, sd_b;

#define SD_A (*(V90SdDetector *)sd_a.raw)
#define SD_B (*(V90SdDetector *)sd_b.raw)
static float sd_ha[SD_HIST], sd_hb[SD_HIST];

static int
run_sd(void)
{
	static const int allow[] = { 0x00 };
	int seen[1] = { 0 };
	int trial, cleared = 0, skipped = 0;

	diff_begin("V90SdDetector::reset");

	for (trial = 0; trial < 50; trial++) {
		unsigned char before[SD_SLOT];
		unsigned char hbefore[sizeof sd_ha];
		unsigned int n = (unsigned)(trial % (SD_HIST + 1));
		unsigned int i;
		int bad, first, allzero = 1;

		fill_pair(sd_a.raw, sd_b.raw, SD_SLOT, trial, trial & 3);
		fill_pair(sd_ha, sd_hb, sizeof sd_ha, trial + 77,
			  (trial + 1) & 3);

		/*
		 * The one field that can never compare equal: each side's
		 * buffer is its own (CLAUDE.md's "two heap pointers hold two
		 * different addresses and always will").  Both are asserted
		 * untouched afterwards and then blanked, so nothing is
		 * skipped silently -- finding F224.
		 */
		SD_A.history = sd_ha;
		SD_B.history = sd_hb;
		SD_A.historyLength = SD_B.historyLength = n;

		/*
		 * Forced non-zero, or clearing it would be invisible: the
		 * pseudorandom fill hits zero once in 2^32 but seed mode 1
		 * and 2 never do and mode 3 rarely does.
		 */
		SD_A.count = SD_B.count = 0x5a5a0000u + (unsigned)trial;

		memcpy(before, sd_b.raw, SD_SLOT);
		memcpy(hbefore, sd_hb, sizeof hbefore);

		SD_A.reset();
		ref_sd_reset(&SD_B);

		diff_eq_int("our history pointer untouched (%ld)",
			    SD_A.history == sd_ha, 1, trial);
		diff_eq_int("the blob's history pointer untouched (%ld)",
			    SD_B.history == sd_hb, 1, trial);
		diff_eq_int("historyLength untouched (%ld)",
			    (long)SD_B.historyLength, (long)n, trial);

		diff_eq_int("the two buffers agree (%ld)",
			    memcmp(sd_ha, sd_hb, sizeof sd_ha) == 0, 1, trial);

		for (i = 0; i < n; i++)
			if (sd_ha[i] != 0.0f)
				allzero = 0;
		diff_eq_int("cleared all %ld entries", allzero, 1, (long)n);
		diff_eq_int("nothing past historyLength (%ld)",
			    memcmp((const unsigned char *)sd_hb + n * 4,
				   hbefore + n * 4,
				   sizeof hbefore - n * 4) == 0, 1, trial);
		if (n > 0)
			cleared = 1;
		else
			skipped = 1;

		/* Which words of the blob's OBJECT moved: only the counter. */
		bad = only_wrote(before, sd_b.raw, SD_SLOT, allow, 1, seen,
				 &first);
		diff_eq_int("the blob wrote the object outside +0x00 at +0x%lx",
			    bad == 0 ? -1 : first, -1, trial);
		diff_eq_int("the blob's count is zero (%ld)",
			    (long)SD_B.count, 0, trial);

		SD_A.history = SD_B.history = (float *)0;
		diff_eq_obj("after reset", V90SdDetector, &SD_A, &SD_B,
			    trial);
		diff_eq_int("no store past the object (%ld)",
			    memcmp(sd_a.raw + sizeof(SD_A),
				   sd_b.raw + sizeof(SD_B),
				   SD_SLOT - sizeof(SD_A)) == 0, 1, trial);
	}

	diff_eq_int("count is a word the function writes", seen[0], 1, 0);
	diff_eq_int("a non-empty history was cleared", cleared, 1, 0);
	diff_eq_int("an empty history skipped the loop", skipped, 1, 0);

	return diff_end();
}

/* --------------------------------------- V90SpectralVerifier (44 bytes) */

#define SV_SLOT 96

/* Storage and alignment only; see `union sd_slot` above. */
union sv_slot {
	double align;
	unsigned char raw[SV_SLOT];
};

static union sv_slot sv_a, sv_b;

#define SV_A (*(V90SpectralVerifier *)sv_a.raw)
#define SV_B (*(V90SpectralVerifier *)sv_b.raw)

static int
run_sv(void)
{
	static const int allow[] = { 0x20, 0x24, 0x28 };
	int seen[3] = { 0, 0, 0 };
	int trial, printed = 0;
	unsigned lvl;

	diff_begin("V90SpectralVerifier::reset");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 24; trial++) {
			unsigned char before[SV_SLOT];
			long tag = (long)lvl * 1000 + trial;
			int bad, first;

			fill_pair(sv_a.raw, sv_b.raw, SV_SLOT, trial,
				  trial & 3);

			/* Forced non-zero: see run_sd. */
			SV_A.accumCount = SV_B.accumCount =
			    0x11110000u + (unsigned)trial;
			SV_A.accumulating = SV_B.accumulating =
			    1u + (unsigned)trial;
			SV_A.word_28 = SV_B.word_28 =
			    0x22220000u + (unsigned)trial;

			memcpy(before, sv_b.raw, SV_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			SV_A.reset();
			ref_sv_reset(&SV_B);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after reset", V90SpectralVerifier,
				    &SV_A, &SV_B, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(sv_a.raw + sizeof(SV_A),
					   sv_b.raw + sizeof(SV_B),
					   SV_SLOT - sizeof(SV_A)) == 0,
				    1, tag);

			bad = only_wrote(before, sv_b.raw, SV_SLOT, allow, 3,
					 seen, &first);
			diff_eq_int("the blob wrote outside +0x20..+0x28 at "
				    "+0x%lx", bad == 0 ? -1 : first, -1, tag);

			diff_eq_int("blob's accumCount (%ld)",
				    (long)SV_B.accumCount, 0, tag);
			diff_eq_int("blob's accumulating (%ld)",
				    (long)SV_B.accumulating, 0, tag);
			diff_eq_int("blob's word_28 (%ld)",
				    (long)SV_B.word_28, 0, tag);

			check_transcript(lvl, tag, &printed);
		}
	}

	set_level(0);
	diff_eq_int("accumCount is written", seen[0], 1, 0);
	diff_eq_int("accumulating is written", seen[1], 1, 0);
	diff_eq_int("word_28 is written", seen[2], 1, 0);
	diff_eq_int("the diagnostic was reached", printed, 1, 0);

	return diff_end();
}

/* ------------------------------------------ V92EchoCanceller (60 bytes) */

#define EC_SLOT 96

/*
 * The special members are written out because `V92EchoCanceller` now declares
 * a destructor -- the object has `D1` and `D2` and this file drives both --
 * and a union member with a non-trivial one deletes the union's.  Same shape
 * as `rt_slot` below, and for the same reason: nothing here constructs or
 * destroys the slot implicitly; the raw bytes are seeded and the members are
 * called on them.
 */
struct ec_slot {
	union {
		unsigned char raw[EC_SLOT];
		double align_;		/* alignment only; trivial */
	};
	V92EchoCanceller &o;

	ec_slot() : o(*(V92EchoCanceller *)raw) { }
};

static struct ec_slot ec_a, ec_b;

static int
run_ec(void)
{
	static const unsigned int val[] = {
		0u, 1u, 2u, 16u, 63u, 64u, 256u, 4095u, 0x7fffffffu,
		0xffffffffu
	};
	static const int allow[] = { 0x2c, 0x38 };
	int seen[2] = { 0, 0 };
	int trial, printed = 0, grew = 0, shrank = 0, still = 0;
	unsigned lvl;

	diff_begin("V92EchoCanceller::setEchoDelay");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		/*
		 * Forty trials of `nw = val[(trial * 7 + 3) % 10]` can never
		 * set `still` below: it needs `t == (7t + 3) mod 10`, which
		 * is `6t == 7 mod 10`, and `6t mod 10` is always even.  The
		 * schedule was unsatisfiable rather than unlucky.  Ten more
		 * trials set the new delay to the old one, so the branch that
		 * moves the tap count by zero is reached.
		 */
		for (trial = 0; trial < 50; trial++) {
			unsigned char before[EC_SLOT];
			unsigned int old = val[trial % 10];
			unsigned int nw = trial < 40
					  ? val[(trial * 7 + 3) % 10]
					  : val[trial % 10];
			unsigned int len = 0x1000u + 37u * (unsigned)trial;
			long tag = (long)lvl * 1000 + trial;
			int bad, first;

			fill_pair(ec_a.raw, ec_b.raw, EC_SLOT, trial,
				  trial & 3);

			ec_a.o.echoDelay = ec_b.o.echoDelay = old;
			ec_a.o.echoLength = ec_b.o.echoLength = len;

			memcpy(before, ec_b.raw, EC_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			ec_a.o.setEchoDelay(nw);
			ref_ec_setEchoDelay(&ec_b.o, nw);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after setEchoDelay", V92EchoCanceller,
				    &ec_a.o, &ec_b.o, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(ec_a.raw + sizeof(ec_a.o),
					   ec_b.raw + sizeof(ec_b.o),
					   EC_SLOT - sizeof(ec_a.o)) == 0,
				    1, tag);

			bad = only_wrote(before, ec_b.raw, EC_SLOT, allow, 2,
					 seen, &first);
			diff_eq_int("the blob wrote outside +0x2c/+0x38 at "
				    "+0x%lx", bad == 0 ? -1 : first, -1, tag);

			/*
			 * The arithmetic, spelled independently of the source
			 * under test: the tap count moves by the CHANGE in
			 * delay, unscaled, wrapping like the unsigned it is.
			 */
			diff_eq_int("blob's echoDelay (%ld)",
				    (long)ec_b.o.echoDelay, (long)nw, tag);
			diff_eq_int("blob's echoLength (%ld)",
				    (long)ec_b.o.echoLength,
				    (long)(unsigned int)(len + nw - old), tag);

			if (nw > old)
				grew = 1;
			else if (nw < old)
				shrank = 1;
			else
				still = 1;

			check_transcript(lvl, tag, &printed);
		}
	}

	set_level(0);
	diff_eq_int("echoLength is written", seen[0], 1, 0);
	diff_eq_int("echoDelay is written", seen[1], 1, 0);
	diff_eq_int("the delay grew at least once", grew, 1, 0);
	diff_eq_int("the delay shrank at least once", shrank, 1, 0);
	diff_eq_int("the delay stood still at least once", still, 1, 0);
	diff_eq_int("the diagnostic was reached", printed, 1, 0);

	return diff_end();
}

/* ------------------------------------- V92EchoCanceller::reset (195 B) */

/*
 * WHAT THIS HAS TO PROVE THAT A WHOLE-OBJECT COMPARISON CANNOT.  `reset`
 * clears two heap buffers whose lengths it computes, so the interesting
 * failures are all off the end of the object: a loop one entry short, a loop
 * one entry long, or the right count of the wrong buffer.  So both buffers
 * get a compared GUARD past their declared length, seeded and never written,
 * and the blob's guard is also compared against its own pre-call image -- the
 * side-against-side check catches a divergence, the before-against-after
 * check catches the two of us overrunning together.
 *
 * D72 IS WHAT THE HISTORY GUARD IS FOR and it is NOT driven.  `echoLength` is
 * built from three inputs and used on `echoHistory` with no reference to what
 * that buffer was allocated with; every trial here sizes the buffer to the
 * largest `echoLength` it generates, so the guard proves the loop stops where
 * the arithmetic says and nothing here ever asks the object to run past a
 * real allocation.  D72 is CONFIRMED and CANNOT FIRE; this is not the place
 * to re-open it.
 *
 * THE ARMA IS SYNTHETIC, and deliberately.  `FloatARMA::reset` is already
 * differentially tested by t_floatarma.cpp; what is unproven here is that
 * `V92EchoCanceller::reset` hands it the pointer at +0x04 and nothing else.
 * A hand-built FloatARMA -- two histories pointing at this file's arrays, two
 * lengths, two tap counts -- is enough for that, and it lets both sides share
 * one seeded 0x34-byte image rather than two constructor runs.
 *
 * NOTHING IS ZEROED (finding F230).  Both buffers and both ARMA histories are
 * seeded with varied bytes every trial, so "the loop wrote zeros" is visible;
 * against a zero-filled buffer a loop one entry short passes.
 */

#define ECR_COEFF	64	/* declared floats in echoCoeff             */
#define ECR_HIST	224	/* declared floats in echoHistory           */
#define ECR_ARMA	24	/* declared floats in each ARMA history     */
#define ECR_GUARD	8	/* compared floats past each of the four    */
#define ECR_PARM	0xdc	/* sizeof(V92Parameters)                    */

/*
 * The one field of the parameter block `reset` reads:
 * `V92Parameters::V92_ECHO_DELAY_OFFSET`, +0x074.  Reached by offset rather
 * than by name so that this file need not carry the V.92 map beside the V.90
 * one it already has (finding F1112's neighbourhood).
 */
#define ECR_DELAY_OFFSET	0x74

static float ecr_coeff[2][ECR_COEFF + ECR_GUARD];
static float ecr_hist[2][ECR_HIST + ECR_GUARD];
static float ecr_x[2][ECR_ARMA + ECR_GUARD];
static float ecr_y[2][ECR_ARMA + ECR_GUARD];
static unsigned char ecr_arma[2][sizeof(FloatARMA)];
static unsigned char ecr_parm[2][ECR_PARM];

static int
run_ec_reset(void)
{
	/* (filterLength, echoDelay, V92_ECHO_DELAY_OFFSET) */
	static const unsigned int shape[][3] = {
		{ 0u,  0u,  0u },	/* both loops skipped entirely      */
		{ 1u,  0u,  0u },	/* coeff runs once, history skipped */
		{ 0u,  1u,  0u },	/* coeff skipped, history runs once */
		{ 0u,  0u,  1u },
		{ 2u,  0u,  0u },	/* (2 >> 1) == 1: history runs once */
		{ 3u,  0u,  0u },	/* the shift TRUNCATES              */
		{ 8u,  4u,  3u },
		{ 40u, 60u, 16u },
		{ 63u, 17u, 5u },
		{ ECR_COEFF, 100u, 12u },
		{ ECR_COEFF, 0u, 0u },
		{ 17u, 0u, ECR_HIST - 8u }
	};
	static const int allow[] = { 0x08, 0x28, 0x2c, 0x30, 0x34 };
	int seen[5] = { 0, 0, 0, 0, 0 };
	int trial, printed = 0;
	int sawCoeff = 0, sawNoCoeff = 0, sawHist = 0, sawNoHist = 0;
	unsigned lvl;
	const int nshape = (int)(sizeof(shape) / sizeof(shape[0]));

	diff_begin("V92EchoCanceller::reset");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);

		for (trial = 0; trial < nshape * 4; trial++) {
			unsigned char before[EC_SLOT], sa[EC_SLOT], sb[EC_SLOT];
			unsigned char gbefore[2][sizeof(ecr_hist[0])];
			const unsigned int *sh = shape[trial % nshape];
			unsigned int fl = sh[0], ed = sh[1], doff = sh[2];
			unsigned int want = (fl >> 1) + ed + doff;
			long tag = (long)lvl * 1000 + trial;
			int side, bad, first, i;

			fill_pair(ec_a.raw, ec_b.raw, EC_SLOT, trial,
				  trial & 3);
			fill_pair(ecr_coeff[0], ecr_coeff[1],
				  (unsigned)sizeof(ecr_coeff[0]), trial + 1,
				  trial & 3);
			fill_pair(ecr_hist[0], ecr_hist[1],
				  (unsigned)sizeof(ecr_hist[0]), trial + 2,
				  trial & 3);
			fill_pair(ecr_x[0], ecr_x[1],
				  (unsigned)sizeof(ecr_x[0]), trial + 3,
				  trial & 3);
			fill_pair(ecr_y[0], ecr_y[1],
				  (unsigned)sizeof(ecr_y[0]), trial + 4,
				  trial & 3);
			fill_pair(ecr_arma[0], ecr_arma[1],
				  (unsigned)sizeof(ecr_arma[0]), trial + 5,
				  trial & 3);
			fill_pair(ecr_parm[0], ecr_parm[1], ECR_PARM,
				  trial + 6, trial & 3);

			for (side = 0; side < 2; side++) {
				V92EchoCanceller *e = side == 0 ? &ec_a.o
								: &ec_b.o;
				FloatARMA *m = (FloatARMA *)ecr_arma[side];

				e->params = (V92Parameters *)ecr_parm[side];
				e->arma = m;
				e->echoCoeff = ecr_coeff[side];
				e->echoHistory = ecr_hist[side];
				e->filterLength = fl;
				e->echoDelay = ed;
				memcpy(&ecr_parm[side][ECR_DELAY_OFFSET],
				       &doff, sizeof doff);

				m->m_xhist = ecr_x[side];
				m->m_yhist = ecr_y[side];
				m->m_xlen = ECR_ARMA;
				m->m_ylen = ECR_ARMA;
				m->m_nA = 4u + (unsigned)(trial % 5);
				m->m_nB = 8u + (unsigned)(trial % 3);
			}

			/* The bound must fit, or this test drives D72. */
			diff_eq_int("the trial fits the buffers (%ld)",
				    (fl <= ECR_COEFF && want <= ECR_HIST), 1,
				    tag);

			memcpy(before, ec_b.raw, EC_SLOT);
			memcpy(gbefore[0], ecr_coeff[1],
			       sizeof(ecr_coeff[0]));
			memcpy(gbefore[1], ecr_hist[1], sizeof(ecr_hist[0]));

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			ec_a.o.reset();
			ref_ec_reset(&ec_b.o);

			dsplib_debug_capture_on = 0;

			/*
			 * The four pointers are each side's own address and
			 * are not written by the method; they are checked
			 * unchanged and then made equal so the rest of the
			 * slot is compared rather than skipped.
			 */
			diff_eq_int("ours kept its four pointers (%ld)",
				    ec_a.o.params ==
					(V92Parameters *)ecr_parm[0]
				    && ec_a.o.arma == (FloatARMA *)ecr_arma[0]
				    && ec_a.o.echoCoeff == ecr_coeff[0]
				    && ec_a.o.echoHistory == ecr_hist[0],
				    1, tag);
			diff_eq_int("the blob kept its four pointers (%ld)",
				    ec_b.o.params ==
					(V92Parameters *)ecr_parm[1]
				    && ec_b.o.arma == (FloatARMA *)ecr_arma[1]
				    && ec_b.o.echoCoeff == ecr_coeff[1]
				    && ec_b.o.echoHistory == ecr_hist[1],
				    1, tag);

			memcpy(sa, ec_a.raw, EC_SLOT);
			memcpy(sb, ec_b.raw, EC_SLOT);
			memset(sa + 0x00, 0, 8);
			memset(sb + 0x00, 0, 8);
			memset(sa + 0x20, 0, 8);
			memset(sb + 0x20, 0, 8);
			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "V92EchoCanceller slot", sa, sb, EC_SLOT,
				     tag);

			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "echoCoeff and its guard", ecr_coeff[0],
				     ecr_coeff[1], sizeof(ecr_coeff[0]), tag);
			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "echoHistory and its guard", ecr_hist[0],
				     ecr_hist[1], sizeof(ecr_hist[0]), tag);
			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "the ARMA x history", ecr_x[0], ecr_x[1],
				     sizeof(ecr_x[0]), tag);
			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "the ARMA y history", ecr_y[0], ecr_y[1],
				     sizeof(ecr_y[0]), tag);
			{
				unsigned char aa[sizeof(FloatARMA)];
				unsigned char ab[sizeof(FloatARMA)];

				memcpy(aa, ecr_arma[0], sizeof aa);
				memcpy(ab, ecr_arma[1], sizeof ab);
				/* m_xhist and m_yhist, +0x08 and +0x0c. */
				memset(aa + 8, 0, 8);
				memset(ab + 8, 0, 8);
				diff_eq_obj_(__FILE__, __LINE__, "after reset",
					     "the ARMA object", aa, ab,
					     sizeof aa, tag);
			}

			/* Neither side may touch either guard. */
			diff_eq_int("the blob left echoCoeff's guard (%ld)",
				    memcmp(gbefore[0] + ECR_COEFF * 4,
					   (unsigned char *)ecr_coeff[1]
					   + ECR_COEFF * 4,
					   ECR_GUARD * 4) == 0, 1, tag);
			diff_eq_int("the blob left echoHistory's guard (%ld)",
				    memcmp(gbefore[1] + ECR_HIST * 4,
					   (unsigned char *)ecr_hist[1]
					   + ECR_HIST * 4,
					   ECR_GUARD * 4) == 0, 1, tag);

			bad = only_wrote(before, ec_b.raw, EC_SLOT, allow, 5,
					 seen, &first);
			diff_eq_int("the blob wrote outside the five fields "
				    "at +0x%lx", bad == 0 ? -1 : first, -1,
				    tag);

			/*
			 * The arithmetic, spelled independently of the source
			 * under test.  `>> 1` on the tap count, then two
			 * additions, all unsigned.
			 */
			diff_eq_int("the blob's echoLength (%ld)",
				    (long)ec_b.o.echoLength, (long)want, tag);
			diff_eq_int("the blob's historyIndex (%ld)",
				    (long)ec_b.o.historyIndex, 0, tag);
			diff_eq_int("the blob's state (%ld)",
				    (long)ec_b.o.state, 0, tag);
			diff_eq_int("the blob's echoBeta is +0.0f (%ld)",
				    ec_b.o.echoBeta == 0.0f, 1, tag);
			diff_eq_int("the blob's echoBetaDecay is +0.0f (%ld)",
				    ec_b.o.echoBetaDecay == 0.0f, 1, tag);
			diff_eq_int("the blob left filterLength (%ld)",
				    (long)ec_b.o.filterLength, (long)fl, tag);
			diff_eq_int("the blob left echoDelay (%ld)",
				    (long)ec_b.o.echoDelay, (long)ed, tag);

			/*
			 * Exactly `fl` coefficients zeroed, and no more.  The
			 * out-of-range half is checked against the SEED, not
			 * against zero: a seeded word is zero often enough
			 * that "it is not zero" is not the same claim as "it
			 * was not written", and only the second one is true.
			 */
			for (i = 0; i < ECR_COEFF; i++) {
				int ok;

				if (i < (int)fl) {
					ok = ecr_coeff[1][i] == 0.0f;
					if (ok)
						sawCoeff = 1;
				} else {
					ok = memcmp(gbefore[0] + i * 4,
						    &ecr_coeff[1][i], 4) == 0;
				}
				diff_eq_int("coefficient cleared iff in "
					    "range (%ld)", ok, 1,
					    tag * 100 + i);
			}
			/*
			 * The history loop is checked the same way, and
			 * `sawHist` needs an entry that CHANGED -- a seeded
			 * zero overwritten with zero is not evidence the loop
			 * ran (findings F223, F224).
			 */
			for (i = 0; i < ECR_HIST; i++) {
				int ok;

				if (i < (int)want) {
					ok = ecr_hist[1][i] == 0.0f;
					if (ok && memcmp(gbefore[1] + i * 4,
							 &ecr_hist[1][i], 4)
					    != 0)
						sawHist = 1;
				} else {
					ok = memcmp(gbefore[1] + i * 4,
						    &ecr_hist[1][i], 4) == 0;
				}
				diff_eq_int("history cleared iff in range "
					    "(%ld)", ok, 1, tag * 100 + i);
			}
			diff_eq_int("the first history entry past the bound "
				    "is untouched (%ld)",
				    want >= ECR_HIST
				    || memcmp(&ecr_hist[1][want],
					      gbefore[1] + want * 4, 4) == 0,
				    1, tag);
			if (fl == 0)
				sawNoCoeff = 1;
			if (want == 0)
				sawNoHist = 1;

			check_transcript(lvl, tag, &printed);
		}
	}

	set_level(0);
	diff_eq_int("the state is written", seen[0], 1, 0);
	diff_eq_int("historyIndex is written", seen[1], 1, 0);
	diff_eq_int("echoLength is written", seen[2], 1, 0);
	diff_eq_int("echoBeta is written", seen[3], 1, 0);
	diff_eq_int("echoBetaDecay is written", seen[4], 1, 0);
	diff_eq_int("the coefficient loop ran", sawCoeff, 1, 0);
	diff_eq_int("the coefficient loop was skipped", sawNoCoeff, 1, 0);
	diff_eq_int("the history loop ran", sawHist, 1, 0);
	diff_eq_int("the history loop was skipped", sawNoHist, 1, 0);
	diff_eq_int("both diagnostics were reached", printed, 1, 0);

	return diff_end();
}

/* --------------------------------- V92EchoCanceller::~V92EchoCanceller */

/*
 * THE WHOLE FUNCTION IS THREE NULL TESTS, so a sweep that never passes a NULL
 * -- or never passes a real pointer -- exercises one arm and reports success.
 * All eight combinations of the three are driven, and both outcomes of each
 * are required to have been seen.
 *
 * A SEEDED POINTER IS NOT A POINTER.  Left as `fill_pair` found them, all
 * three fields are wild addresses handed straight to `sysdep_free`, which the
 * harness swallows into `bad_free`; the run survives and proves nothing.  So
 * every non-null arm gets a real `sysdep_malloc`, `bad_free` is asserted zero,
 * and the free count is asserted against the number the mask predicts.
 *
 * THE ARMA COSTS FOUR FREES, NOT ONE.  `~FloatARMA` frees `m_a`, `m_b`,
 * `m_xhist` and `m_yhist`, each guarded, and does NOT null them -- so the
 * destroyed ARMA still holds four dangling addresses and cannot be compared
 * between the sides.  It is built here with all four allocated, which makes
 * the expected free count for a non-null `arma` five: four inside the ARMA
 * and one for the ARMA itself.
 */

#define ECD_N	16u		/* floats in each allocated block */

static int
run_ec_dtor(void)
{
	static const int allow[] = { 0x04, 0x20, 0x24 };
	int seen[3] = { 0, 0, 0 };
	int trial, printed = 0;
	int sawNull[3] = { 0, 0, 0 }, sawReal[3] = { 0, 0, 0 };
	unsigned lvl;

	diff_begin("V92EchoCanceller::~V92EchoCanceller");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);

		for (trial = 0; trial < 8 * 2 * 2; trial++) {
			unsigned char before[EC_SLOT];
			int mask = trial & 7;
			int useD2 = (trial >> 3) & 1;
			long tag = (long)lvl * 1000 + trial;
			int side, bad, first, k;
			int wantFrees = ((mask & 1) ? 1 : 0)
					+ ((mask & 2) ? 1 : 0)
					+ ((mask & 4) ? 5 : 0);

			fill_pair(ec_a.raw, ec_b.raw, EC_SLOT, trial,
				  trial & 3);

			harness_alloc_reset();

			for (side = 0; side < 2; side++) {
				V92EchoCanceller *e = side == 0 ? &ec_a.o
								: &ec_b.o;

				/*
				 * `params` is never read by this method, so
				 * both sides get the same value and the slot
				 * compares whole.
				 */
				e->params = 0;
				e->echoCoeff = (mask & 1)
				    ? (float *)sysdep_malloc(ECD_N * 4) : 0;
				e->echoHistory = (mask & 2)
				    ? (float *)sysdep_malloc(ECD_N * 4) : 0;
				if (mask & 4) {
					FloatARMA *m = (FloatARMA *)
					    sysdep_malloc(sizeof(FloatARMA));

					m->m_a = (float *)
					    sysdep_malloc(ECD_N * 4);
					m->m_b = (float *)
					    sysdep_malloc(ECD_N * 4);
					m->m_xhist = (float *)
					    sysdep_malloc(ECD_N * 4);
					m->m_yhist = (float *)
					    sysdep_malloc(ECD_N * 4);
					e->arma = m;
				} else {
					e->arma = 0;
				}
			}

			diff_eq_int("the fixture allocated what the mask "
				    "says (%ld)", harness_alloc.allocs,
				    2 * wantFrees, tag);

			memcpy(before, ec_b.raw, EC_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			if (useD2) {
				our_ec_dtor2(&ec_a.o);
				ref_ec_dtor2(&ec_b.o);
			} else {
				our_ec_dtor(&ec_a.o);
				ref_ec_dtor(&ec_b.o);
			}

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after the destructor", V92EchoCanceller,
				    &ec_a.o, &ec_b.o, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(ec_a.raw + sizeof(ec_a.o),
					   ec_b.raw + sizeof(ec_b.o),
					   EC_SLOT - sizeof(ec_a.o)) == 0,
				    1, tag);

			bad = only_wrote(before, ec_b.raw, EC_SLOT, allow, 3,
					 seen, &first);
			diff_eq_int("the blob wrote outside +0x04/+0x20/+0x24 "
				    "at +0x%lx", bad == 0 ? -1 : first, -1,
				    tag);

			diff_eq_int("the blob nulled echoCoeff (%ld)",
				    ec_b.o.echoCoeff == 0, 1, tag);
			diff_eq_int("the blob nulled echoHistory (%ld)",
				    ec_b.o.echoHistory == 0, 1, tag);
			diff_eq_int("the blob nulled arma (%ld)",
				    ec_b.o.arma == 0, 1, tag);

			diff_eq_int("both sides freed what they held (%ld)",
				    harness_alloc.frees, 2 * wantFrees, tag);
			diff_eq_int("nothing outstanding (%ld)",
				    harness_alloc.live, 0, tag);
			diff_eq_int("no wild free (%ld)",
				    harness_alloc.bad_free, 0, tag);
			diff_eq_int("no free of NULL reached the allocator "
				    "(%ld)", harness_alloc.free_null, 0, tag);

			for (k = 0; k < 3; k++) {
				if (mask & (1 << k))
					sawReal[k] = 1;
				else
					sawNull[k] = 1;
			}

			check_transcript(lvl, tag, &printed);
		}
	}

	set_level(0);
	diff_eq_int("arma is nulled", seen[0], 1, 0);
	diff_eq_int("echoCoeff is nulled", seen[1], 1, 0);
	diff_eq_int("echoHistory is nulled", seen[2], 1, 0);
	diff_eq_int("echoCoeff was null at least once", sawNull[0], 1, 0);
	diff_eq_int("echoCoeff was real at least once", sawReal[0], 1, 0);
	diff_eq_int("echoHistory was null at least once", sawNull[1], 1, 0);
	diff_eq_int("echoHistory was real at least once", sawReal[1], 1, 0);
	diff_eq_int("arma was null at least once", sawNull[2], 1, 0);
	diff_eq_int("arma was real at least once", sawReal[2], 1, 0);
	diff_eq_int("the diagnostic was reached", printed, 1, 0);

	return diff_end();
}

/* ------------------------ V92EchoCanceller::V92EchoCanceller (313 bytes) */

/*
 * THE CONSTRUCTOR IS WHERE THE SIZING HAPPENS, so this is the first test in
 * the tree that can see a mis-sizing.  `reset`'s test above puts a synthetic
 * guard past two buffers IT owns; here the buffers are the constructor's own,
 * `historyAlloc` and `filterLength` are its own arithmetic, and the checks
 * are built the other way round:
 *
 *   - the total BYTES each side asked the allocator for are predicted here,
 *     from the parameter block and the two scalar arguments, and compared.
 *     A buffer one float short is a byte count, not a silence.
 *   - `reset` then clears `echoLength` floats of a buffer `historyAlloc`
 *     floats long, and the TAIL between them must still hold the harness's
 *     0xa5 fill on both sides.  That is D72's guard made out of the object's
 *     own numbers rather than out of the test's.
 *   - every trial asserts `echoLength <= historyAlloc` BEFORE it believes any
 *     of that.  Driving the overrun would corrupt this process's heap, which
 *     is not the same thing as testing D72; D72 is CONFIRMED and CANNOT FIRE
 *     (finding F1188) and this is not the place to re-open it.
 *
 * TWO ARMS ARE NOT DRIVEN AND THE REASON IS THE SAME BOTH TIMES: the object
 * faults before anything could be compared.
 *
 *   - A NEGATIVE `V92_ECHO_FILTER_LENGTH` takes the `js; add $0x3` arm at
 *     +0x131.  The result is a huge unsigned `filterLength`, and the next
 *     instruction but four asks `sysdep_malloc` for four times it.  So the
 *     signed `x / 4 * 4` and the `& ~3` that D72 and finding F1188 write are
 *     indistinguishable to any test that survives, and the correction is
 *     recorded rather than measured (finding F1312).
 *   - `blockLen == 0` divides by zero at +0x98.  Finding F1188 already calls
 *     that a different defect; every trial below uses a nonzero divisor and
 *     says so rather than avoiding it quietly.
 *
 * `filterLength == 0` IS DRIVEN, and it is not the same thing.  `word_18` is
 * then `0 - 1`, and the wrap is real: the history length is built from it as
 * unsigned, so an initial delay of 1 brings the sum back to 0 and the object
 * allocates a small buffer.  Both sides wrap identically; the trials that do
 * it keep the delay at or above 1, because at zero the wrap survives into the
 * allocation size.
 *
 * THE PARAMETER BLOCK IS SHARED between the sides.  The constructor only
 * reads it, so one block means +0x00 compares as a value rather than being
 * excluded -- which is one more field measured, not one fewer.
 */

extern "C" {
void our_ec_ctor(void *self, void *params, unsigned int blockLen,
		 unsigned int extra)
	asm("_ZN16V92EchoCancellerC1EP13V92Parametersjj");
void ref_ec_ctor(void *self, void *params, unsigned int blockLen,
		 unsigned int extra)
	asm("ref__ZN16V92EchoCancellerC1EP13V92Parametersjj");
void our_ec_ctor2(void *self, void *params, unsigned int blockLen,
		  unsigned int extra)
	asm("_ZN16V92EchoCancellerC2EP13V92Parametersjj");
void ref_ec_ctor2(void *self, void *params, unsigned int blockLen,
		  unsigned int extra)
	asm("ref__ZN16V92EchoCancellerC2EP13V92Parametersjj");
}

/* The three fields of V92Parameters the constructor reads. */
#define ECC_FILTER_LENGTH	0x6c
#define ECC_INITIAL_DELAY	0x70
#define ECC_DELAY_OFFSET	0x74

/* What FloatARMA(12, 12, den, num, 99) allocates, in floats. */
#define ECC_ARMA_NA	12u
#define ECC_ARMA_NB	12u
#define ECC_ARMA_XLEN	(ECC_ARMA_NB + 99u)
#define ECC_ARMA_YLEN	(ECC_ARMA_NA + 99u)

/* FloatARMA's four owned pointers, +0x00..+0x0f. */
#define ECC_ARMA_PTRS	0x10

static unsigned char ecc_parm[ECR_PARM] __attribute__((aligned(8)));

struct ecc_case {
	int filterLength;	/* V92_ECHO_FILTER_LENGTH, +0x6c */
	int initialDelay;	/* V92_ECHO_INITIAL_DELAY, +0x70 */
	int delayOffset;	/* V92_ECHO_DELAY_OFFSET,  +0x74 */
	unsigned int blockLen;	/* argument 2, the divisor        */
	unsigned int extra;	/* argument 3, the final addend   */
};

/*
 * The shipped configuration is the last row, scaled down: 180, 840 and -14 are
 * the real defaults (finding F1188) and are used as they stand, because the
 * allocation they imply is 2,298 floats, which is nothing.
 */
static const struct ecc_case ecc_cases[] = {
	{   4,   1,   0,	1u,	0u   },
	{   4,   1,   0,	1u,	7u   },
	{   8,   4,   3,	2u,	1u   },
	{   9,   4,   3,	2u,	1u   },	/* 9/4*4 == 8: the truncation */
	{  10,   4,   3,	3u,	0u   },
	{  11,   4,   3,	3u,	5u   },
	{  40,  60,  16,	7u,	19u  },
	{  64, 100,  12,	40u,	199u },
	{  63,  17,   5,	5u,	3u   },
	{   0,   1,   0,	1u,	4u   },	/* word_18 wraps; see the head */
	{   0,   8,   2,	4u,	9u   },
	{ 180, 840, -14,	40u,	199u }	/* the shipped configuration  */
};
#define ECC_NCASE ((int)(sizeof(ecc_cases) / sizeof(ecc_cases[0])))

static int
run_ec_ctor(void)
{
	static const int allow[] = {
		0x00, 0x04, 0x08, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c,
		0x30, 0x34, 0x38
	};
	int seen[13];
	int trial, printed = 0, i;
	int sawWrap = 0, sawDistinct = 0;
	unsigned int firstAlloc = 0;
	unsigned lvl;

	for (i = 0; i < 13; i++)
		seen[i] = 0;

	diff_begin("V92EchoCanceller::V92EchoCanceller");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);

		for (trial = 0; trial < ECC_NCASE * 4 * 2; trial++) {
			const struct ecc_case *c =
			    &ecc_cases[trial % ECC_NCASE];
			int useC2 = (trial / ECC_NCASE) & 1;
			unsigned char before[EC_SLOT], sa[EC_SLOT], sb[EC_SLOT];
			long tag = (long)lvl * 1000 + trial;
			int bad, first;
			unsigned int len, w18, span, want, need, wantBytes;
			float *coeff[2], *hist[2];
			unsigned char *arma[2];
			int liveWas, allocWas;
			unsigned int bytesWas;

			/*
			 * The arithmetic, spelled independently of the source
			 * under test: a signed divide-and-multiply, an
			 * unsigned decrement that may wrap, and the two
			 * unsigned sums the object builds from them.
			 */
			len = (unsigned int)(c->filterLength / 4 * 4);
			w18 = len - 1u;
			span = w18 + (unsigned int)c->initialDelay;
			need = span + 2u * c->blockLen
			       + span / c->blockLen * c->blockLen + c->extra;
			want = (len >> 1) + (unsigned int)c->initialDelay
			       + (unsigned int)c->delayOffset;
			wantBytes = len * 4u + need * 4u
				    + (unsigned int)sizeof(FloatARMA)
				    + (ECC_ARMA_NA + ECC_ARMA_NB
				       + ECC_ARMA_XLEN + ECC_ARMA_YLEN) * 4u;

			/*
			 * Believed before anything is dereferenced: `reset`
			 * clears `want` floats of a buffer `need` long.
			 */
			diff_eq_int("the trial fits its own allocation (%ld)",
				    want <= need && need < 0x100000u, 1, tag);
			if (want > need || need >= 0x100000u)
				continue;

			fill_pair(ec_a.raw, ec_b.raw, EC_SLOT, trial,
				  trial & 3);
			fill_pair(ecc_parm, ecc_parm, ECR_PARM, trial + 9,
				  trial & 3);
			memcpy(&ecc_parm[ECC_FILTER_LENGTH], &c->filterLength,
			       sizeof(int));
			memcpy(&ecc_parm[ECC_INITIAL_DELAY], &c->initialDelay,
			       sizeof(int));
			memcpy(&ecc_parm[ECC_DELAY_OFFSET], &c->delayOffset,
			       sizeof(int));

			memcpy(before, ec_b.raw, EC_SLOT);

			liveWas = harness_alloc.live;
			allocWas = harness_alloc.allocs;
			bytesWas = harness_alloc.bytes;

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			if (useC2) {
				our_ec_ctor2(&ec_a.o, ecc_parm, c->blockLen,
					     c->extra);
				ref_ec_ctor2(&ec_b.o, ecc_parm, c->blockLen,
					     c->extra);
			} else {
				our_ec_ctor(&ec_a.o, ecc_parm, c->blockLen,
					    c->extra);
				ref_ec_ctor(&ec_b.o, ecc_parm, c->blockLen,
					    c->extra);
			}

			dsplib_debug_capture_on = 0;

			/*
			 * SEVEN ALLOCATIONS A SIDE, and the BYTES are the
			 * check that matters: three here -- the coefficients,
			 * the history and the ARMA object -- and four inside
			 * `FloatARMA`.
			 */
			diff_eq_int("fourteen allocations, seven a side (%ld)",
				    harness_alloc.allocs - allocWas, 14, tag);
			diff_eq_int("both sides asked for the same %ld bytes",
				    (long)(harness_alloc.bytes - bytesWas),
				    (long)(2u * wantBytes), tag);

			coeff[0] = ec_a.o.echoCoeff;
			coeff[1] = ec_b.o.echoCoeff;
			hist[0] = ec_a.o.echoHistory;
			hist[1] = ec_b.o.echoHistory;
			arma[0] = (unsigned char *)ec_a.o.arma;
			arma[1] = (unsigned char *)ec_b.o.arma;

			diff_eq_int("six pointers, all non-null and no two "
				    "shared (%ld)",
				    coeff[0] != 0 && coeff[1] != 0 &&
				    hist[0] != 0 && hist[1] != 0 &&
				    arma[0] != 0 && arma[1] != 0 &&
				    coeff[0] != coeff[1] &&
				    hist[0] != hist[1] &&
				    arma[0] != arma[1], 1, tag);
			if (coeff[0] == 0 || coeff[1] == 0 || hist[0] == 0 ||
			    hist[1] == 0 || arma[0] == 0 || arma[1] == 0)
				return diff_end();

			diff_eq_int("both sides kept the parameter block they "
				    "were given (%ld)",
				    ec_a.o.params == (V92Parameters *)ecc_parm
				    && ec_b.o.params ==
					(V92Parameters *)ecc_parm, 1, tag);

			/*
			 * The three heap pointers are each side's own address
			 * and always will be; everything else in the slot,
			 * INCLUDING the parameter pointer and the guard past
			 * the object, is compared as it stands.
			 */
			memcpy(sa, ec_a.raw, EC_SLOT);
			memcpy(sb, ec_b.raw, EC_SLOT);
			memset(sa + 0x04, 0, 4);
			memset(sb + 0x04, 0, 4);
			memset(sa + 0x20, 0, 8);
			memset(sb + 0x20, 0, 8);
			diff_eq_obj_(__FILE__, __LINE__, "after the constructor",
				     "V92EchoCanceller slot", sa, sb, EC_SLOT,
				     tag);

			/* Both buffers, in full, at the length it chose. */
			diff_eq_int("the coefficients match, all %ld bytes",
				    memcmp(coeff[0], coeff[1], len * 4u) == 0,
				    1, (long)(len * 4u));
			diff_eq_int("the history matches, all %ld bytes",
				    memcmp(hist[0], hist[1], need * 4u) == 0,
				    1, (long)(need * 4u));

			/*
			 * D72's TAIL.  `reset` cleared `want` floats; the
			 * remaining `need - want` are still the harness's
			 * fill, on both sides.  A history one float short
			 * would have this fail on the side that overran, and
			 * a `reset` bounded by the ALLOCATION rather than by
			 * `echoLength` would have it fail on both.
			 */
			{
				unsigned int k;
				int tailA = 1, tailB = 1;

				for (k = want * 4u; k < need * 4u; k++) {
					if (((unsigned char *)hist[0])[k]
					    != HARNESS_MALLOC_FILL)
						tailA = 0;
					if (((unsigned char *)hist[1])[k]
					    != HARNESS_MALLOC_FILL)
						tailB = 0;
				}
				diff_eq_int("ours left the history's tail "
					    "unwritten (%ld)", tailA, 1, tag);
				diff_eq_int("the blob left the history's tail "
					    "unwritten (%ld)", tailB, 1, tag);
			}

			/*
			 * The ARMA: its four owned pointers are two sets of
			 * four addresses, so they are excluded and the four
			 * buffers they name are compared instead.
			 */
			{
				unsigned char aa[sizeof(FloatARMA)];
				unsigned char ab[sizeof(FloatARMA)];
				FloatARMA *ma = (FloatARMA *)arma[0];
				FloatARMA *mb = (FloatARMA *)arma[1];

				diff_eq_int("the ARMA's four buffers are all "
					    "there and no two shared (%ld)",
					    ma->m_a != 0 && mb->m_a != 0 &&
					    ma->m_b != 0 && mb->m_b != 0 &&
					    ma->m_xhist != 0 &&
					    mb->m_xhist != 0 &&
					    ma->m_yhist != 0 &&
					    mb->m_yhist != 0 &&
					    ma->m_a != mb->m_a &&
					    ma->m_b != mb->m_b &&
					    ma->m_xhist != mb->m_xhist &&
					    ma->m_yhist != mb->m_yhist, 1, tag);

				memcpy(aa, arma[0], sizeof aa);
				memcpy(ab, arma[1], sizeof ab);
				memset(aa, 0, ECC_ARMA_PTRS);
				memset(ab, 0, ECC_ARMA_PTRS);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after the constructor",
					     "FloatARMA", aa, ab, sizeof aa,
					     tag);

				diff_eq_int("the denominator the ARMA copied "
					    "matches (%ld)",
					    memcmp(ma->m_a, mb->m_a,
						   ECC_ARMA_NA * 4u) == 0,
					    1, tag);
				diff_eq_int("the numerator the ARMA copied "
					    "matches (%ld)",
					    memcmp(ma->m_b, mb->m_b,
						   ECC_ARMA_NB * 4u) == 0,
					    1, tag);
				diff_eq_int("the ARMA's x history matches "
					    "(%ld)",
					    memcmp(ma->m_xhist, mb->m_xhist,
						   ECC_ARMA_XLEN * 4u) == 0,
					    1, tag);
				diff_eq_int("the ARMA's y history matches "
					    "(%ld)",
					    memcmp(ma->m_yhist, mb->m_yhist,
						   ECC_ARMA_YLEN * 4u) == 0,
					    1, tag);
			}

			/* The numbers, predicted rather than read back. */
			diff_eq_int("the blob's filterLength (%ld)",
				    (long)ec_b.o.filterLength, (long)len, tag);
			diff_eq_int("the blob's word_18 (%ld)",
				    (long)ec_b.o.word_18, (long)w18, tag);
			diff_eq_int("the blob's historyAlloc (%ld)",
				    (long)ec_b.o.historyAlloc, (long)need, tag);
			diff_eq_int("the blob's echoDelay (%ld)",
				    (long)ec_b.o.echoDelay,
				    (long)(unsigned int)c->initialDelay, tag);
			diff_eq_int("the blob's echoLength (%ld)",
				    (long)ec_b.o.echoLength, (long)want, tag);

			bad = only_wrote(before, ec_b.raw, EC_SLOT, allow, 13,
					 seen, &first);
			diff_eq_int("the blob wrote outside the thirteen "
				    "fields at +0x%lx", bad == 0 ? -1 : first,
				    -1, tag);

			diff_eq_int("transcript matches (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("line counts match (%ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);
			if (lvl > 1 && dsplib_debug_capture_lines(1) != 0)
				printed = 1;
			if (lvl == 0)
				diff_eq_int("silent below the gate (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    0, tag);

			if (c->filterLength == 0)
				sawWrap = 1;
			if (trial == 0)
				firstAlloc = ec_b.o.historyAlloc;
			else if (ec_b.o.historyAlloc != firstAlloc)
				sawDistinct = 1;

			/* Hand all fourteen back before the next trial. */
			our_ec_dtor(&ec_a.o);
			ref_ec_dtor(&ec_b.o);

			diff_eq_int("both destructors freed all fourteen "
				    "(%ld)", harness_alloc.live - liveWas, 0,
				    tag);
			diff_eq_int("no wild free (%ld)",
				    harness_alloc.bad_free, 0, tag);
		}
	}

	set_level(0);
	for (i = 0; i < 13; i++)
		diff_eq_int("every declared field is one the constructor "
			    "writes (+0x%lx)", seen[i] ? -1 : allow[i], -1, 0);
	diff_eq_int("the diagnostics were reached", printed, 1, 0);
	diff_eq_int("the zero filter length was driven", sawWrap, 1, 0);
	diff_eq_int("the history length was not the same every trial",
		    sawDistinct, 1, 0);

	return diff_end();
}

/* ------------------------------------- ResamplerTimingOffset (76 bytes) */

#define RT_SLOT 128

/*
 * The special members are written out because `ResamplerTimingOffset` is now
 * a real polymorphic class with a user-declared constructor and destructor,
 * which deletes a union's implicit ones.  Nothing here constructs the object;
 * the raw bytes are seeded and the member called on them, exactly as before.
 */
struct rt_slot {
	union {
		unsigned char raw[RT_SLOT];
		double align_;		/* alignment only; trivial */
	};
	ResamplerTimingOffset &o;

	rt_slot() : o(*(ResamplerTimingOffset *)raw) { }
};

static struct rt_slot rt_a, rt_b;

static void *const vptr_seed = (void *)0xdeadbeefu;

static int
run_rt(void)
{
	static const float scale[] = {
		1.0f, -1.0f, 8000.0f, 9600.0f, 3200.0f, 0.5f, 1.0e6f,
		1.0e-6f, 12345.678f, -273.15f, 0.0f, 16777216.0f
	};
	static const float ppm[] = {
		0.0f, 1.0f, -1.0f, 100.0f, -100.0f, 0.125f, 7.3f, -0.0009f,
		1000000.0f, 3.14159265f, 1.0e-8f, 65536.0f, -1234.5f
	};
	static const int allow[] = { 0x48 };
	int seen[1] = { 0 };
	int trial, distinct = 0, nonzero = 0;
	float firstval = 0.0f;

	diff_begin("ResamplerTimingOffset::setTimingOffset");

	for (trial = 0; trial < 12 * 13; trial++) {
		unsigned char before[RT_SLOT];
		float s = scale[trial % 12];
		float p = ppm[(trial / 12) % 13];
		int bad, first;

		fill_pair(rt_a.raw, rt_b.raw, RT_SLOT, trial, trial & 3);

		/*
		 * The vptr is seeded identically on both sides and asserted
		 * untouched: this member does not dispatch, and if the header
		 * had left offset 0 out, `ppmScale` and `timingOffset` would
		 * both sit four bytes low and the blob's +0x48 would land in
		 * a word this test declares as padding.  That is finding
		 * F228's trap, and `only_wrote` below is what catches it.
		 *
		 * It is reached through `raw` rather than as a member, because
		 * the vptr is now what `virtual` puts at +0x00 and not a field
		 * the header declares.  The check is the same one.
		 */
		memcpy(rt_a.raw, &vptr_seed, sizeof vptr_seed);
		memcpy(rt_b.raw, &vptr_seed, sizeof vptr_seed);
		rt_a.o.ppmScale = rt_b.o.ppmScale = s;

		memcpy(before, rt_b.raw, RT_SLOT);

		rt_a.o.setTimingOffset(p);
		ref_rt_setTimingOffset(&rt_b.o, p);

		diff_eq_obj("after setTimingOffset", ResamplerTimingOffset,
			    &rt_a.o, &rt_b.o, trial);
		diff_eq_int("no store past the object (%ld)",
			    memcmp(rt_a.raw + sizeof(rt_a.o),
				   rt_b.raw + sizeof(rt_b.o),
				   RT_SLOT - sizeof(rt_a.o)) == 0, 1, trial);

		bad = only_wrote(before, rt_b.raw, RT_SLOT, allow, 1, seen,
				 &first);
		diff_eq_int("the blob wrote outside +0x48 at +0x%lx",
			    bad == 0 ? -1 : first, -1, trial);

		diff_eq_int("the vptr is untouched (%ld)",
			    memcmp(rt_b.raw, &vptr_seed, sizeof vptr_seed)
			    == 0, 1, trial);
		diff_eq_int("ppmScale is untouched (%ld)",
			    rt_b.o.ppmScale == s, 1, trial);

		/*
		 * Anti-vacuity for a function whose only output is one float:
		 * the results must not all be zero and must not all be the
		 * same.  A wrong constant -- 1e-6 the double rather than
		 * 1e-6f -- changes every non-zero product, which the
		 * bit-exact whole-object comparison above is what catches.
		 */
		if (trial == 0)
			firstval = rt_a.o.timingOffset;
		else if (rt_a.o.timingOffset != firstval)
			distinct = 1;
		if (rt_a.o.timingOffset != 0.0f)
			nonzero = 1;
	}

	diff_eq_int("timingOffset is written", seen[0], 1, 0);
	diff_eq_int("the results are not all equal", distinct, 1, 0);
	diff_eq_int("the results are not all zero", nonzero, 1, 0);

	return diff_end();
}

/* ------------------------------------ V90Phase4Modulator (12204 bytes) */

#define P4_SLOT 12288

/*
 * RAW STORAGE, NOT A UNION.  `V90Phase4Modulator` gained a constructor and a
 * destructor when the V.90 modulator chain landed, which deletes a union's
 * own and makes `static union p4_slot p4_a;` stop compiling.  The block is
 * reached through a cast instead, which changes nothing about what is
 * measured: `setSessionFlag` is still called over seeded storage that no
 * constructor has run over.
 */
static unsigned char p4_raw[2][P4_SLOT] __attribute__((aligned(8)));

#define p4_a_raw	(p4_raw[0])
#define p4_b_raw	(p4_raw[1])
#define p4_a		((V90Phase4Modulator *)(void *)p4_raw[0])
#define p4_b		((V90Phase4Modulator *)(void *)p4_raw[1])

static int
run_p4(void)
{
	static const unsigned int flagv[] = {
		0u, 1u, 2u, 0x80u, 0x8000u, 0xffffu, 0x7fffffffu, 0xffffffffu
	};
	static const int allow[] = { 0x0000 };
	int seen[1] = { 0 };
	int trial;

	diff_begin("V90Phase4Modulator::setSessionFlag");

	for (trial = 0; trial < 32; trial++) {
		unsigned char before[P4_SLOT];
		unsigned int f = flagv[trial & 7];
		int bad, first;

		fill_pair(p4_a_raw, p4_b_raw, P4_SLOT, trial, trial & 3);

		/* Forced to differ from the value about to be stored. */
		p4_a->sessionFlag = p4_b->sessionFlag = ~f;

		memcpy(before, p4_b_raw, P4_SLOT);

		p4_a->setSessionFlag(f);
		ref_p4_setSessionFlag(p4_b, f);

		diff_eq_obj("after setSessionFlag", V90Phase4Modulator,
			    p4_a, p4_b, trial);
		diff_eq_int("no store past the object (%ld)",
			    memcmp(p4_a_raw + sizeof(V90Phase4Modulator),
				   p4_b_raw + sizeof(V90Phase4Modulator),
				   P4_SLOT - sizeof(V90Phase4Modulator))
			    == 0, 1, trial);

		bad = only_wrote(before, p4_b_raw, P4_SLOT, allow, 1, seen,
				 &first);
		diff_eq_int("the blob wrote outside +0x0000 at +0x%lx",
			    bad == 0 ? -1 : first, -1, trial);
		diff_eq_int("blob's sessionFlag (%ld)",
			    (long)p4_b->sessionFlag, (long)f, trial);
	}

	diff_eq_int("sessionFlag is written", seen[0], 1, 0);

	return diff_end();
}

/* ------------------------------------------ the parameter block, shared */

/*
 * One V90Parameters per side, seeded pairwise, and the ONE thing the three
 * lifecycle members below have in common: each copies configuration out of
 * it.  The two sides get identical bytes, so a copy that read the wrong slot
 * would still produce the same value on both sides -- which is why the checks
 * also assert what the BLOB stored against the parameter READ BY NAME through
 * our own header.  That is the claim: this field receives that parameter.
 */
#define PARM_SLOT_L (sizeof(V90Parameters) + 64)

static unsigned char parm_a[PARM_SLOT_L] __attribute__((aligned(8)));
static unsigned char parm_b[PARM_SLOT_L] __attribute__((aligned(8)));

#define PA ((V90Parameters *)parm_a)
#define PB ((V90Parameters *)parm_b)

/* --------------------------------- V90ConstellationDesigner::reset (47 B) */

static int
run_cd_reset(void)
{
	/* Every word the object writes, by absolute offset. */
	static const int allow[] = { 0x08, 0x0c, 0x10, 0x24, 0x48 };
	int seen[5] = { 0, 0, 0, 0, 0 };
	int trial, printed = 0;
	unsigned lvl;

	diff_begin("V90ConstellationDesigner::reset");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 24; trial++) {
			unsigned char before[CD_SLOT];
			long tag = (long)lvl * 1000 + trial;
			int bad, first;

			fill_pair(cd_a.raw, cd_b.raw, CD_SLOT, trial,
				  trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT_L, trial + 77,
				  trial & 3);
			cd_a_o.params = PA;
			cd_b_o.params = PA;
			memcpy(before, cd_b.raw, CD_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			cd_a_o.reset();
			ref_cd_reset(&cd_b_o);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after reset", V90ConstellationDesigner,
				    &cd_a_o, &cd_b_o, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(cd_a.raw + sizeof(cd_a_o),
					   cd_b.raw + sizeof(cd_b_o),
					   CD_SLOT - sizeof(cd_a_o)) == 0,
				    1, tag);
			diff_eq_int("the parameter block was not written (%ld)",
				    memcmp(parm_a, parm_b, PARM_SLOT_L) == 0,
				    1, tag);

			bad = only_wrote(before, cd_b.raw, CD_SLOT, allow, 5,
					 seen, &first);
			diff_eq_int("the blob wrote outside the five words at "
				    "+0x%lx", bad == 0 ? -1 : first, -1, tag);

			/*
			 * The fill is GONE from every one of them, and the
			 * copy came from the slot the header names.  Two
			 * never-reset objects compare equal (finding F1105),
			 * so the values are asserted and not only compared.
			 */
			diff_eq_int("blob's word_48 (%ld)",
				    (long)cd_b_o.word_48, 0, tag);
			diff_eq_int("blob's short_0a (%ld)",
				    (long)cd_b_o.short_0a, 0, tag);
			diff_eq_int("blob's short_0c (%ld)",
				    (long)cd_b_o.short_0c, 0, tag);
			diff_eq_int("blob's short_0e (%ld)",
				    (long)cd_b_o.short_0e, 0, tag);
			diff_eq_int("blob's short_10 (%ld)",
				    (long)cd_b_o.short_10, 0, tag);
			diff_eq_int("blob's word_24 is params->unnamed_39c "
				    "(%ld)", (long)cd_b_o.word_24,
				    (long)(unsigned int)PA->unnamed_39c, tag);

			/*
			 * NO DIAGNOSTIC AT ALL: 47 bytes, no call, no gate.
			 * Both sides must be silent at every level, which is
			 * the only thing the transcript can say here and is
			 * still worth saying -- an invented message would be
			 * invisible to every other check in this block.
			 */
			(void)printed;
			diff_eq_int("ours said nothing (%ld)",
				    (int)dsplib_debug_capture_lines(0), 0,
				    tag);
			diff_eq_int("the blob said nothing (%ld)",
				    (int)dsplib_debug_capture_lines(1), 0,
				    tag);
		}
	}

	set_level(0);
	diff_eq_int("+0x08 is a word reset writes", seen[0], 1, 0);
	diff_eq_int("+0x0c is a word reset writes", seen[1], 1, 0);
	diff_eq_int("+0x10 is a word reset writes", seen[2], 1, 0);
	diff_eq_int("+0x24 is a word reset writes", seen[3], 1, 0);
	diff_eq_int("+0x48 is a word reset writes", seen[4], 1, 0);

	return diff_end();
}

/* ------------------------------- V90ConnectionEvaluator (188 bytes) */

/*
 * NOT A UNION.  The class has a user-declared constructor and destructor, so
 * it is not trivial and cannot be a union member; the slot is a byte array
 * and the object is reached through a cast.
 */
#define CE_SLOT_L 256

static unsigned char ce_a[CE_SLOT_L] __attribute__((aligned(8)));
static unsigned char ce_b[CE_SLOT_L] __attribute__((aligned(8)));

#define CEA ((V90ConnectionEvaluator *)ce_a)
#define CEB ((V90ConnectionEvaluator *)ce_b)

/* Every word `reset` writes.  +0x00, +0x88, +0x98 and +0xb8 are not in it. */
static const int ce_allow[] = {
	0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c,
	0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58,
	0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84,
	0x8c, 0x90, 0x94, 0x9c, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4
};
#define CE_NALLOW ((int)(sizeof(ce_allow) / sizeof(ce_allow[0])))

static int ce_seen[CE_NALLOW];

/*
 * What the configuration slots must come out holding, read out of the BLOB's
 * object and compared against the parameter block BY NAME.
 */
static void
ce_check_values(V90ConnectionEvaluator *o, V90Parameters *p, long tag)
{
	diff_eq_int("enableRrnDown (%ld)", o->enableRrnDown,
		    p->ENABLE_RRN_DOWN, tag);
	diff_eq_int("enableRrnUp (%ld)", o->enableRrnUp,
		    p->ENABLE_RRN_UP, tag);
	diff_eq_int("nofRemoteRateRenegBeforeRetrain (%ld)",
		    o->nofRemoteRateRenegBeforeRetrain,
		    p->NOF_REMOTE_RATE_RENEG_BEFORE_RETRAIN, tag);
	diff_eq_int("debugAlternateDebug (%ld)", o->debugAlternateDebug,
		    p->DEBUG_CONNECTION_EVALUATOR_ALTERNATE_DEBUG, tag);
	diff_eq_int("debugFallBack (%ld)", o->debugFallBack,
		    p->DEBUG_CONNECTION_EVALUATOR_FALL_BACK, tag);
	diff_eq_int("debugRetrain (%ld)", o->debugRetrain,
		    p->DEBUG_CONNECTION_EVALUATOR_RETRAIN, tag);
	diff_eq_int("debugRateUp (%ld)", o->debugRateUp,
		    p->DEBUG_CONNECTION_EVALUATOR_RATE_UP, tag);
	diff_eq_int("debugRateDown (%ld)", o->debugRateDown,
		    p->DEBUG_CONNECTION_EVALUATOR_RATE_DOWN, tag);
	diff_eq_int("retrainCounterFadeCount (%ld)",
		    o->retrainCounterFadeCount,
		    p->RETRAIN_COUNTER_FADE_COUNT, tag);
	diff_eq_int("remoteRrnCounterFadeCount (%ld)",
		    o->remoteRrnCounterFadeCount,
		    p->REMOTE_RRN_COUNTER_FADE_COUNT, tag);
	diff_eq_int("rateUpDetectDuration (%ld)", o->rateUpDetectDuration,
		    p->RATE_UP_DETECT_DURATION, tag);
	diff_eq_int("minDurationInDataBeforeRrnUp (%ld)",
		    o->minDurationInDataBeforeRrnUp,
		    p->MINIMUM_DURATION_IN_DATA_BEFORE_RRN_UP, tag);
	diff_eq_int("rateDownDetectDuration (%ld)", o->rateDownDetectDuration,
		    p->RATE_DOWN_DETECT_DURATION, tag);
	diff_eq_int("minDurationInDataBeforeRrnDown (%ld)",
		    o->minDurationInDataBeforeRrnDown,
		    p->MINIMUM_DURATION_IN_DATA_BEFORE_RRN_DOWN, tag);
	diff_eq_int("retrainDetectDuration (%ld)", o->retrainDetectDuration,
		    p->RETRAIN_DETECT_DURATION, tag);
	diff_eq_int("debugPeriod (%ld)", o->debugPeriod,
		    p->DEBUG_CONNECTION_EVALUATOR_PERIOD, tag);
	diff_eq_int("phase4ErrorForV34Fallback (%ld)",
		    memcmp(&o->phase4ErrorForV34Fallback,
			   &p->PHASE4_ERROR_FOR_V34_FALLBACK, 4) == 0, 1, tag);

	/* The constants, and the widths that make two of them different. */
	diff_eq_int("word_64 (%ld)", (long)o->word_64, 1600, tag);
	diff_eq_int("word_68 (%ld)", (long)o->word_68, 1600, tag);
	diff_eq_int("word_8c (%ld)", (long)o->word_8c, -1, tag);
	diff_eq_int("short_9c (%ld)", (long)o->short_9c, -1, tag);
	diff_eq_int("curDmin (%ld)", (long)o->curDmin, 0, tag);
	diff_eq_int("short_b0 (%ld)", (long)o->short_b0, 1, tag);
	diff_eq_int("short_b2 (%ld)", (long)o->short_b2, 0, tag);
	diff_eq_int("short_b4 (%ld)", (long)o->short_b4, 0, tag);
	diff_eq_int("nofV90Retrains (%ld)", (long)o->nofV90Retrains, 0, tag);
	diff_eq_int("word_24 (%ld)", (long)o->word_24, 0, tag);
	/* A float now, so the bit pattern and not the value: -0.0f is not 0. */
	{
		static const unsigned int zero = 0;

		diff_eq_int("threshRetrain (%ld)",
			    memcmp(&o->threshRetrain, &zero, 4) == 0, 1, tag);
	}
}

static int
run_ce(void)
{
	int trial, printed = 0;
	unsigned lvl;

	diff_begin("V90ConnectionEvaluator::reset");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 24; trial++) {
			unsigned char before[CE_SLOT_L];
			long tag = (long)lvl * 1000 + trial;
			int bad, first;

			fill_pair(ce_a, ce_b, CE_SLOT_L, trial, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT_L, trial + 31,
				  trial & 3);
			CEA->params = PA;
			CEB->params = PA;
			memcpy(before, ce_b, CE_SLOT_L);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			CEA->reset();
			ref_ce_reset(CEB);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after reset", V90ConnectionEvaluator,
				    CEA, CEB, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(ce_a + sizeof(*CEA),
					   ce_b + sizeof(*CEB),
					   CE_SLOT_L - sizeof(*CEA)) == 0,
				    1, tag);
			diff_eq_int("the parameter block was not written (%ld)",
				    memcmp(parm_a, parm_b, PARM_SLOT_L) == 0,
				    1, tag);
			diff_eq_int("the parameter pointer survived (%ld)",
				    CEB->params == PA, 1, tag);

			bad = only_wrote(before, ce_b, CE_SLOT_L, ce_allow,
					 CE_NALLOW, ce_seen, &first);
			diff_eq_int("the blob wrote a word reset does not "
				    "name, at +0x%lx",
				    bad == 0 ? -1 : first, -1, tag);

			ce_check_values(CEB, PA, tag);

			check_transcript(lvl, tag, &printed);
		}
	}

	set_level(0);
	diff_eq_int("the diagnostics were reached", printed, 1, 0);

	/*
	 * Every offset the allow-list names is one the BLOB really writes.
	 * Without this the list is a permission and not a claim -- the file's
	 * own rule, and `run_cd_reset` asserts the same five lines below its
	 * sweep.
	 */
	{
		int i;

		for (i = 0; i < CE_NALLOW; i++)
			diff_eq_int("+0x%lx is a word reset writes",
				    ce_seen[i], 1, ce_allow[i]);
	}

	return diff_end();
}

/*
 * The constructor and the destructor.
 *
 * The constructor is "store the parameter block, then reset", so the check is
 * that a constructed object is byte-for-byte a reset one with +0x00 filled
 * in.  The destructor is a bare `ret`, and comparing two objects AFTER a
 * destructor would compare the allocator (finding F1105) -- so what is checked
 * is that it wrote NOTHING, against the object's own image taken immediately
 * before the call.  Two empty things compare equal, and this is the shape
 * that says so.
 */
static int
run_ce_lifecycle(void)
{
	int trial;

	diff_begin("V90ConnectionEvaluator: constructor and destructor");
	set_level(0);

	for (trial = 0; trial < 16; trial++) {
		unsigned char before[CE_SLOT_L];
		long tag = 4000 + trial;

		fill_pair(ce_a, ce_b, CE_SLOT_L, trial + 500, trial & 3);
		memcpy(before, ce_b, CE_SLOT_L);
		fill_pair(parm_a, parm_b, PARM_SLOT_L, trial + 501,
			  trial & 3);

		our_ce_ctor(ce_a, parm_a);
		ref_ce_ctor(ce_b, parm_a);

		diff_eq_obj("after construction", V90ConnectionEvaluator,
			    CEA, CEB, tag);
		diff_eq_int("the constructor stored the parameter block (%ld)",
			    CEB->params == PA, 1, tag);
		diff_eq_int("and the fill is gone from +0x00 (%ld)",
			    memcmp(before, ce_b, 4) != 0, 1, tag);

		/* It really did run reset: the configuration is in place. */
		ce_check_values(CEB, PA, tag);

		diff_eq_int("no store past the object (%ld)",
			    memcmp(ce_a + sizeof(*CEA), ce_b + sizeof(*CEB),
				   CE_SLOT_L - sizeof(*CEA)) == 0, 1, tag);

		/* The destructor: it must write nothing at all. */
		memcpy(before, ce_b, CE_SLOT_L);
		our_ce_dtor(ce_a);
		ref_ce_dtor(ce_b);
		diff_eq_int("the destructor wrote nothing (%ld)",
			    memcmp(before, ce_b, CE_SLOT_L) == 0, 1, tag);
		diff_eq_obj("after destruction", V90ConnectionEvaluator,
			    CEA, CEB, tag);
	}

	return diff_end();
}

/* --------------------------------- K56FlexFloModem::externalReset (1 byte) */

/*
 * One byte, `c3`.  The whole claim is that it touches nothing, and a
 * comparison of two objects neither side wrote is the archetypal vacuous
 * pass -- so the object is compared against its OWN pre-call image as well as
 * against the blob's, and the slot is far bigger than the class so a store
 * anywhere near it would show.
 */
#define K56_SLOT 128

static unsigned char k56_a[K56_SLOT] __attribute__((aligned(8)));
static unsigned char k56_b[K56_SLOT] __attribute__((aligned(8)));

static int
run_k56(void)
{
	int trial;

	diff_begin("K56FlexFloModem::externalReset");
	set_level(0);

	for (trial = 0; trial < 16; trial++) {
		unsigned char before[K56_SLOT];
		long tag = 6000 + trial;

		fill_pair(k56_a, k56_b, K56_SLOT, trial + 900, trial & 3);
		memcpy(before, k56_b, K56_SLOT);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();

		((K56FlexFloModem *)k56_a)->externalReset();
		ref_k56_externalReset(k56_b);

		dsplib_debug_capture_on = 0;

		diff_eq_int("the two objects agree (%ld)",
			    memcmp(k56_a, k56_b, K56_SLOT) == 0, 1, tag);
		diff_eq_int("the blob wrote nothing at all (%ld)",
			    memcmp(before, k56_b, K56_SLOT) == 0, 1, tag);
		diff_eq_int("and neither did ours (%ld)",
			    memcmp(before, k56_a, K56_SLOT) == 0, 1, tag);
		diff_eq_int("nothing was printed (%ld)",
			    (int)dsplib_debug_capture_lines(1), 0, tag);
	}

	return diff_end();
}

/* --------------------------- V90SignBitsExtractor: constructor, destructor */

/*
 * Forty bytes, three stores and one embedded parallel differential decoder.
 *
 * THE ONE WORD THE COMPARISON EXCLUDES IS +0x1c, `decoder.state_`, and it is
 * excluded because each side's constructor calls `sysdep_malloc` for itself
 * and two allocations are never the same address.  Finding F1113's shared
 * arena is not available here -- the pointer is produced by the code under
 * test rather than seeded into it -- so instead the two allocations are
 * checked to be DISTINCT, both live, and to hold the same six zeroed bytes.
 */
#define SBE_SLOT_L	(0x28 + 32)
#define SBE_OFF_STATE	0x1c

static unsigned char sbe_a[SBE_SLOT_L] __attribute__((aligned(8)));
static unsigned char sbe_b[SBE_SLOT_L] __attribute__((aligned(8)));

#define SA ((V90SignBitsExtractor *)sbe_a)
#define SB ((V90SignBitsExtractor *)sbe_b)

/* Everything but the four bytes at +0x1c, raw, so a stray store fails. */
static int
sbe_same_but_state(void)
{
	return memcmp(sbe_a, sbe_b, SBE_OFF_STATE) == 0 &&
	       memcmp(sbe_a + SBE_OFF_STATE + 4, sbe_b + SBE_OFF_STATE + 4,
		      SBE_SLOT_L - SBE_OFF_STATE - 4) == 0;
}

static int
run_sbe_lifecycle(void)
{
	int trial;

	diff_begin("V90SignBitsExtractor: constructor and destructor");
	set_level(0);

	for (trial = 0; trial < 16; trial++) {
		unsigned char before[SBE_SLOT_L];
		struct alloc_log base;
		long tag = 6000 + trial;
		int i, nonzero;

		fill_pair(sbe_a, sbe_b, SBE_SLOT_L, trial + 600, trial & 3);
		memcpy(before, sbe_b, SBE_SLOT_L);

		base = harness_alloc;
		if (trial & 1) {
			our_sbe_ctor2(sbe_a);
			ref_sbe_ctor2(sbe_b);
		} else {
			our_sbe_ctor(sbe_a);
			ref_sbe_ctor(sbe_b);
		}

		diff_eq_int("each constructor allocated once (%ld)",
			    harness_alloc.allocs - base.allocs, 2, tag);
		diff_eq_int("identical but for the state pointer (%ld)",
			    sbe_same_but_state(), 1, tag);

		/* The three stores, read off the BLOB's object. */
		diff_eq_int("+0x10 is zeroed (%ld)", (long)SB->state, 0, tag);
		diff_eq_int("+0x18 is zeroed (%ld)",
			    (long)SB->oddDecoder.prev_, 0, tag);
		diff_eq_int("the decoder's capacity is six (%ld)",
			    (long)SB->decoder.capacity_, 6, tag);
		diff_eq_int("and its active width is zero (%ld)",
			    (long)SB->decoder.size_, 0, tag);

		/*
		 * Anti-vacuity: the fill must be GONE from both stored
		 * fields, or "it is zero" is a statement about the seed.
		 */
		diff_eq_int("the fill is gone from +0x10 (%ld)",
			    memcmp(before + 0x10, sbe_b + 0x10, 4) != 0, 1,
			    tag);
		diff_eq_int("the fill is gone from +0x18 (%ld)",
			    before[0x18] != sbe_b[0x18], 1, tag);

		/* And what the constructor must NOT have touched. */
		diff_eq_int("+0x00..+0x0f is untouched (%ld)",
			    memcmp(before, sbe_b, 0x10) == 0, 1, tag);
		diff_eq_int("+0x14 is untouched (%ld)",
			    memcmp(before + 0x14, sbe_b + 0x14, 4) == 0, 1,
			    tag);
		diff_eq_int("no store past the object (%ld)",
			    memcmp(before + 0x28, sbe_b + 0x28,
				   SBE_SLOT_L - 0x28) == 0, 1, tag);

		/* Two allocations, not one shared one, and both are cleared. */
		diff_eq_int("the two states are distinct blocks (%ld)",
			    SA->decoder.state_ != SB->decoder.state_, 1, tag);
		diff_eq_int("and neither is null (%ld)",
			    SA->decoder.state_ != 0 &&
			    SB->decoder.state_ != 0, 1, tag);
		nonzero = 0;
		for (i = 0; i < 6; i++)
			nonzero |= SA->decoder.state_[i] |
				   SB->decoder.state_[i];
		diff_eq_int("the six state bytes are cleared (%ld)", nonzero,
			    0, tag);

		/*
		 * The destructor.  Finding F1113: compare what it WROTE, taken
		 * immediately before the call -- two objects compared after a
		 * free compare the allocator.  It must write nothing, and
		 * free exactly what the constructor took.
		 */
		memcpy(before, sbe_b, SBE_SLOT_L);
		base = harness_alloc;
		if (trial & 1) {
			our_sbe_dtor2(sbe_a);
			ref_sbe_dtor2(sbe_b);
		} else {
			our_sbe_dtor(sbe_a);
			ref_sbe_dtor(sbe_b);
		}

		diff_eq_int("each destructor freed once (%ld)",
			    harness_alloc.frees - base.frees, 2, tag);
		diff_eq_int("live is back where it started (%ld)",
			    harness_alloc.live, base.live - 2, tag);
		diff_eq_int("no bad free (%ld)", harness_alloc.bad_free, 0,
			    tag);
		diff_eq_int("the destructor wrote nothing (%ld)",
			    memcmp(before, sbe_b, SBE_SLOT_L) == 0, 1, tag);
	}

	/*
	 * The null arm.  `~ParallelDifferentialDecoder` tests its pointer, so
	 * a destructor run over a null state must reach `sysdep_free` not at
	 * all -- `free_null` is what tells that apart from a call that was
	 * made and swallowed.
	 */
	for (trial = 0; trial < 4; trial++) {
		unsigned char before[SBE_SLOT_L];
		struct alloc_log base;
		long tag = 6100 + trial;

		fill_pair(sbe_a, sbe_b, SBE_SLOT_L, trial + 700, trial & 3);
		SA->decoder.state_ = 0;
		SB->decoder.state_ = 0;
		memcpy(before, sbe_b, SBE_SLOT_L);

		base = harness_alloc;
		our_sbe_dtor(sbe_a);
		ref_sbe_dtor(sbe_b);

		diff_eq_int("a null state frees nothing (%ld)",
			    harness_alloc.frees - base.frees, 0, tag);
		diff_eq_int("and does not call free(NULL) either (%ld)",
			    harness_alloc.free_null - base.free_null, 0, tag);
		diff_eq_int("nothing written on the null arm (%ld)",
			    memcmp(before, sbe_b, SBE_SLOT_L) == 0, 1, tag);
		diff_eq_int("still identical (%ld)",
			    memcmp(sbe_a, sbe_b, SBE_SLOT_L) == 0, 1, tag);
	}

	return diff_end();
}

/* ------------------------------------------------------ the V.90 demapper */

/*
 * 7,864 bytes, and the size is the `movl $0x1eb8,(%esp)` in
 * `V90Demodulator`'s constructor rather than a displacement bound -- finding
 * F1107, which is what cost `V90Equalizer` eight bytes.
 *
 * EVERY REGION CHECK BELOW IS BY ABSOLUTE OFFSET, deliberately.  The seeding
 * goes through the header's field names, so if the header had an array at the
 * wrong offset both sides would be seeded at the wrong offset and agree; what
 * cannot agree is the set of bytes the BLOB's object actually moved, and that
 * is compared against a partition written as numbers.  Findings F223 and F224.
 */
#define DEM_SZ		0x1eb8
#define DEM_SLOT_L	(DEM_SZ + 64)
#define DEM_OFF_CONST	0x0030		/* short constellation[6][128]      */
#define DEM_OFF_SIZE	0x0630		/* unsigned constellationSize[6]    */
#define DEM_OFF_SBST	(0x668 + 0x1c)	/* signBits.decoder.state_          */
#define DEM_OFF_SUM	0x0690		/* unsigned errorSum[6][128]        */
#define DEM_OFF_COUNT	0x1290		/* unsigned errorCount[6][128]      */
#define DEM_OFF_HIST	0x1e90		/* unsigned errorHistogramCount     */
#define DEM_ARR		(6 * 128 * 4)

static unsigned char dem_a[DEM_SLOT_L] __attribute__((aligned(8)));
static unsigned char dem_b[DEM_SLOT_L] __attribute__((aligned(8)));

#define DA ((V90Demapper *)dem_a)
#define DB ((V90Demapper *)dem_b)

/*
 * The six per-constellation lengths, and what each set is for.  `lines` is the
 * exact number of `edprintf` calls the printing arm makes: two banners, the
 * histogram number, six constellation headers, and one row per LEVEL THAT
 * EXISTS -- which is the whole distinction between the printing bound
 * (`constellationSize[i]`) and the zeroing bound (a constant 128).
 */
struct dem_case {
	unsigned int size[6];
	int gated;			/* the six-way guard rejects       */
	unsigned int lines;		/* edprintf calls when it does not */
};

static const struct dem_case dem_cases[] = {
	{ { 1, 2, 3, 4, 5, 6 },		0, 9 + 21  },
	{ { 8, 1, 7, 2, 6, 3 },		0, 9 + 27  },
	{ { 2, 2, 2, 60, 2, 2 },	0, 9 + 70  },
	{ { 0, 4, 4, 4, 4, 4 },		1, 0	   },
	{ { 4, 4, 0, 4, 4, 4 },		1, 0	   },
	{ { 4, 4, 4, 4, 4, 0 },		1, 0	   },
};
#define DEM_NCASE ((int)(sizeof dem_cases / sizeof dem_cases[0]))

/*
 * Seed both objects identically.  `params` is the SAME block on both sides on
 * purpose: the destructor only reads one word of it, and one pointer value is
 * what lets the object comparison run raw.
 */
static void
dem_seed(int trial, const struct dem_case *c)
{
	unsigned int i;

	fill_pair(dem_a, dem_b, DEM_SLOT_L, trial + 800, trial & 3);
	DA->params = PA;
	DB->params = PA;
	for (i = 0; i < 6; i++) {
		DA->constellationSize[i] = c->size[i];
		DB->constellationSize[i] = c->size[i];
		/*
		 * One level per constellation with a zero sample count, so
		 * the `test`/`je` on the divisor at 0x30c48 is exercised on
		 * every trial rather than whenever the fill happens to lay
		 * down four zero bytes in a row.  Its error SUM is left
		 * filled, so an average of zero and an average of "the sum"
		 * are different numbers.
		 */
		DA->errorCount[i][0] = 0;
		DB->errorCount[i][0] = 0;
	}
}

/* Was any byte of the range non-zero before the call?  Anti-vacuity. */
static int
dem_any_set(const unsigned char *p, unsigned off, unsigned n)
{
	unsigned i;

	for (i = 0; i < n; i++)
		if (p[off + i] != 0)
			return 1;
	return 0;
}

static int
dem_all_clear(const unsigned char *p, unsigned off, unsigned n)
{
	unsigned i;

	for (i = 0; i < n; i++)
		if (p[off + i] != 0)
			return 0;
	return 1;
}

/*
 * What the BLOB's object did, by absolute offset, as a partition of the whole
 * slot: everything below +0x690 untouched, the two arrays either emptied or
 * left alone, the counter up by one, everything above +0x1e94 untouched.
 */
static void
dem_check_effect(const unsigned char *before, int gated, long tag)
{
	unsigned int was, now;

	memcpy(&was, before + DEM_OFF_HIST, 4);
	memcpy(&now, dem_b + DEM_OFF_HIST, 4);
	diff_eq_int("the histogram number went up by one (%ld)",
		    (long)(unsigned int)(now - was), 1, tag);

	diff_eq_int("nothing below +0x690 moved (%ld)",
		    memcmp(before, dem_b, DEM_OFF_SUM) == 0, 1, tag);
	diff_eq_int("nothing above +0x1e94 moved (%ld)",
		    memcmp(before + DEM_OFF_HIST + 4, dem_b + DEM_OFF_HIST + 4,
			   DEM_SLOT_L - DEM_OFF_HIST - 4) == 0, 1, tag);

	if (gated) {
		/*
		 * The guard suppresses the RESET as well as the print, which
		 * is the part the function's name does not tell you.
		 */
		diff_eq_int("gated: the error sums are left alone (%ld)",
			    memcmp(before + DEM_OFF_SUM, dem_b + DEM_OFF_SUM,
				   DEM_ARR) == 0, 1, tag);
		diff_eq_int("gated: the error counts are left alone (%ld)",
			    memcmp(before + DEM_OFF_COUNT,
				   dem_b + DEM_OFF_COUNT, DEM_ARR) == 0, 1,
			    tag);
		return;
	}

	/*
	 * ALL 6 * 128 entries of both arrays, not `constellationSize[i]` of
	 * them: the zeroing loop's bound is the constant 0x7f and the row
	 * length is nowhere in it.
	 */
	diff_eq_int("the fill was there to remove (%ld)",
		    dem_any_set(before, DEM_OFF_SUM, DEM_ARR) &&
		    dem_any_set(before, DEM_OFF_COUNT, DEM_ARR), 1, tag);
	diff_eq_int("every error sum is zero (%ld)",
		    dem_all_clear(dem_b, DEM_OFF_SUM, DEM_ARR), 1, tag);
	diff_eq_int("every error count is zero (%ld)",
		    dem_all_clear(dem_b, DEM_OFF_COUNT, DEM_ARR), 1, tag);
}

/* The transcript, with the exact line count this case must produce. */
static void
dem_transcript(unsigned lvl, unsigned want, long tag, int *printed)
{
	diff_eq_int("transcript matches (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("line counts match (%ld)",
		    (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), tag);
	if (lvl > 1) {
		diff_eq_int("the blob printed exactly the rows that exist "
			    "(%ld)", (int)dsplib_debug_capture_lines(1),
			    (int)want, tag);
		if (want != 0) {
			*printed = 1;
			transcripts_seen = 1;
		}
	} else {
		diff_eq_int("below the gate ours was silent (%ld)",
			    (int)dsplib_debug_capture_lines(0), 0, tag);
		diff_eq_int("below the gate the blob was silent (%ld)",
			    (int)dsplib_debug_capture_lines(1), 0, tag);
	}
}

static int
run_dem_histogram(void)
{
	unsigned lvl;
	int trial, printed = 0;

	diff_begin("V90Demapper::printErrorHistogramAndReset");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < DEM_NCASE * 2; trial++) {
			const struct dem_case *c =
				&dem_cases[trial % DEM_NCASE];
			unsigned char before[DEM_SLOT_L];
			long tag = (long)lvl * 1000 + trial;

			dem_seed(trial, c);
			memcpy(before, dem_b, DEM_SLOT_L);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			our_dem_hist(dem_a);
			ref_dem_hist(dem_b);

			dsplib_debug_capture_on = 0;

			/*
			 * No pointer is produced or freed here, so the two
			 * objects compare RAW over the whole slot -- finding
			 * F1113's point about not weakening the comparison.
			 */
			diff_eq_int("the two objects are identical (%ld)",
				    memcmp(dem_a, dem_b, DEM_SLOT_L) == 0, 1,
				    tag);
			dem_check_effect(before, c->gated, tag);
			dem_transcript(lvl, c->lines, tag, &printed);
		}
	}

	diff_eq_int("the histogram printed at some level", printed, 1, 0);
	return diff_end();
}

/*
 * The destructor.  Three heap blocks per side, because each side frees its
 * own: +0x1c, +0x20 and the sign-bit extractor's decoder state at +0x684.
 * Those three words are the ONLY ones the object comparison excludes.
 */
#define DEM_NPTR 3

static int
dem_pointers(int null_them, long tag)
{
	if (null_them) {
		DA->codes = 0;
		DB->codes = 0;
		DA->signs = 0;
		DB->signs = 0;
		DA->signBits.decoder.state_ = 0;
		DB->signBits.decoder.state_ = 0;
		return 1;
	}
	DA->codes = (unsigned int *)sysdep_malloc(16);
	DB->codes = (unsigned int *)sysdep_malloc(16);
	DA->signs = (unsigned char *)sysdep_malloc(8);
	DB->signs = (unsigned char *)sysdep_malloc(8);
	DA->signBits.decoder.state_ =
		(unsigned char *)sysdep_malloc(V90SBE_DECODER_SIZE);
	DB->signBits.decoder.state_ =
		(unsigned char *)sysdep_malloc(V90SBE_DECODER_SIZE);
	diff_eq_int("six blocks handed out (%ld)",
		    DA->codes != 0 && DB->codes != 0 &&
		    DA->signs != 0 && DB->signs != 0 &&
		    DA->signBits.decoder.state_ != 0 &&
		    DB->signBits.decoder.state_ != 0, 1, tag);
	return 0;
}

/* Everything but the three pointer words, raw. */
static int
dem_same_but_pointers(void)
{
	return memcmp(dem_a, dem_b, 0x1c) == 0 &&
	       memcmp(dem_a + 0x24, dem_b + 0x24, DEM_OFF_SBST - 0x24) == 0 &&
	       memcmp(dem_a + DEM_OFF_SBST + 4, dem_b + DEM_OFF_SBST + 4,
		      DEM_SLOT_L - DEM_OFF_SBST - 4) == 0;
}

static int
run_dem_lifecycle(void)
{
	unsigned lvl;
	int trial, printed = 0;

	diff_begin("V90Demapper: the destructor path");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < DEM_NCASE * 3; trial++) {
			const struct dem_case *c =
				&dem_cases[trial % DEM_NCASE];
			/*
			 * Every third trial takes the null arm, and every
			 * third has the parameter gate OFF -- which is the
			 * arm on which the destructor must do nothing at all
			 * but free.
			 */
			int null_them = (trial % 3) == 1;
			int gate_off = (trial % 3) == 2;
			int gated = c->gated || gate_off;
			unsigned char before[DEM_SLOT_L];
			struct alloc_log base;
			long tag = 4000 + (long)lvl * 1000 + trial;
			int want = null_them ? 0 : 2 * DEM_NPTR;

			dem_seed(trial + 40, c);
			fill_pair(parm_a, parm_b, PARM_SLOT_L, trial + 41,
				  trial & 3);
			/*
			 * THE GATE IS A PARAMETER-BLOCK FIELD, NOT
			 * `dsplibs_debug_level`.  Seeded with the harness
			 * fill it is non-zero, so it has to be set on every
			 * trial or the "off" arm never runs.
			 */
			PA->DEBUG_DEMAPPER_ERROR_HISTOGRAM = gate_off ? 0 : 1;

			dem_pointers(null_them, tag);
			memcpy(before, dem_b, DEM_SLOT_L);
			base = harness_alloc;

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			if (trial & 1) {
				our_dem_dtor2(dem_a);
				ref_dem_dtor2(dem_b);
			} else {
				our_dem_dtor(dem_a);
				ref_dem_dtor(dem_b);
			}

			dsplib_debug_capture_on = 0;

			diff_eq_int("the two objects are identical but for "
				    "the three heap pointers (%ld)",
				    dem_same_but_pointers(), 1, tag);
			diff_eq_int("both destructors freed their blocks "
				    "(%ld)", harness_alloc.frees - base.frees,
				    want, tag);
			diff_eq_int("live is back where it started (%ld)",
				    harness_alloc.live, base.live - want, tag);
			diff_eq_int("no bad free (%ld)", harness_alloc.bad_free,
				    0, tag);
			/*
			 * The three `if (p)` guards.  `sysdep_free` tolerates
			 * NULL, so dropping them changes no byte anywhere --
			 * this counter is the only thing that sees it.
			 */
			diff_eq_int("and free(NULL) was never called (%ld)",
				    harness_alloc.free_null - base.free_null,
				    0, tag);

			if (gate_off) {
				diff_eq_int("gate off: the destructor wrote "
					    "nothing (%ld)",
					    memcmp(before, dem_b, DEM_SLOT_L)
					    == 0, 1, tag);
				diff_eq_int("gate off: and printed nothing "
					    "(%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    0, tag);
				diff_eq_int("gate off: ours printed nothing "
					    "either (%ld)",
					    (int)dsplib_debug_capture_lines(0),
					    0, tag);
				continue;
			}
			dem_check_effect(before, gated, tag);
			dem_transcript(lvl, gated ? 0 : c->lines, tag,
				       &printed);
		}
	}

	diff_eq_int("the destructor reached the histogram", printed, 1, 0);
	return diff_end();
}

/* ------------------- V92EchoCanceller: the state machine and the signal --
 *
 * `setState` (1104 B), `updateEchoHistory` (244 B) and `process` (794 B), the
 * three that carry a sample.  One fixture, because they share an object and
 * because `process` calls `setState`.
 *
 * WHY THE COMPARISON HAS TO BE PER BLOCK AND BIT-EXACT.  `process` ADAPTS:
 * every sample changes `echoCoeff` and `echoBeta`, and the next sample's
 * output depends on them.  A divergence in the last bit of one coefficient at
 * block 40 that is swamped by convergence at block 60 is still a defect, so
 * the whole object, both buffers and both output blocks are compared after
 * EVERY call and floats are compared by their BYTES.  A tolerance is exactly
 * how an adapting filter's slow drift hides.
 *
 * THE ECHO PATH IS REAL.  `echoHistory` is filled with a persistently
 * exciting +-0.5 sequence and the input is a near-end signal plus that
 * sequence run through a six-tap response, taken from the same window the
 * canceller is about to look at -- so the LMS update has something to
 * converge to and the coefficients really move.  `ecx_moved` asserts they
 * did, and `ecx_varied` that consecutive blocks did not produce one repeated
 * answer (findings F223, F224): a `process` that never touched a coefficient
 * would otherwise pass as two identical do-nothings.
 *
 * NOTHING IS ZEROED (finding F230), and the parts the methods must not touch
 * keep their seed: `echoCoeff` past `filterLength`, `echoHistory` past
 * `historyAlloc`, `out` past `count`, and a compared GUARD past every buffer.
 * D72's overrun would land in one of those and fail rather than pass quietly.
 * The seed is also why `out[0]` is set explicitly on every block -- a seeded
 * word is a NaN often enough, and a NaN in `out[0]` takes the 177.0f path.
 */

#define ECX_COEFF	40		/* declared floats in echoCoeff     */
#define ECX_HIST	320		/* declared floats in echoHistory   */
#define ECX_GUARD	8		/* compared floats past each buffer */
#define ECX_LEAD	8		/* and BEFORE the history           */
#define ECX_BLK		128		/* longest block driven             */
#define ECX_ECHO	6		/* taps in the test's echo path     */

/* The six V92Parameters fields `setState` reads, by offset (finding F1112). */
#define ECX_FAST_BETA	0x78
#define ECX_FAST_DECAY	0x7c
#define ECX_SLOW_BETA	0x80
#define ECX_SLOW_DECAY	0x84
#define ECX_FAST_DUR	0x88
#define ECX_SLOW_DUR	0x8c

static float ecx_coeff[2][ECX_COEFF + ECX_GUARD];
/*
 * THE HISTORY HAS A GUARD AT BOTH ENDS, and the leading one is not
 * decoration: `updateEchoHistory`'s compaction copies DOWNWARD to
 * `echoHistory[filterLength - 2]`, so a count one too long walks off the
 * FRONT of the buffer, where a trailing guard sees nothing.  `ECX_H` is the
 * pointer the object is given; the comparisons cover the whole array.
 */
static float ecx_hist[2][ECX_LEAD + ECX_HIST + ECX_GUARD];
#define ECX_H(side)	(&ecx_hist[side][ECX_LEAD])
static float ecx_out[2][ECX_BLK + ECX_GUARD];
static float ecx_in[ECX_BLK];
static unsigned char ecx_parm[2][ECR_PARM];
static unsigned char ecx_arma[2][sizeof(FloatARMA)];

/* A float from its bytes: the table below is bit patterns, not decimals. */
static float
ecx_bits(unsigned int u)
{
	float f;

	memcpy(&f, &u, sizeof f);
	return f;
}

static int
ecx_same_bits(float a, float b)
{
	return memcmp(&a, &b, sizeof a) == 0;
}

/*
 * The values the two beta parameters are swept over.  Written as bytes
 * because the interesting ones -- both zeroes, a NaN, both infinities, a
 * denormal -- have no decimal spelling, and because the diagnostic prints a
 * SIGN, an integer part and a scaled fraction of each: `sign_of` takes the
 * '+' arm for a NaN where `0.0f < v` would take the '-' one, and only a NaN
 * in the parameter block can tell those apart.
 */
static const unsigned int ecx_pat[] = {
	0x00000000u,	/* +0.0f, which prints as NEGATIVE zero      */
	0x80000000u,	/* -0.0f                                     */
	0x3f800000u,	/* +1.0f                                     */
	0xbf800000u,	/* -1.0f                                     */
	0x3c23d70au,	/* +0.01f                                    */
	0xbb03126fu,	/* -0.002f                                   */
	0x42f6e979u,	/* +123.456f                                 */
	0x7fc00000u,	/* a quiet NaN                               */
	0x7f800000u,	/* +infinity                                 */
	0xff800000u,	/* -infinity                                 */
	0x00000001u,	/* the smallest denormal                     */
	0x4b189680u,	/* +1e7f, past what the %d field can hold     */
	0xc2f6e979u	/* -123.456f: a NEGATIVE with a whole part    */
};
#define ECX_NPAT ((int)(sizeof(ecx_pat) / sizeof(ecx_pat[0])))

static void
ecx_param_word(unsigned int off, unsigned int v)
{
	memcpy(&ecx_parm[0][off], &v, sizeof v);
	memcpy(&ecx_parm[1][off], &v, sizeof v);
}

/*
 * The transcript check the three share.  `check_transcript` cannot be used:
 * it demands that the blob printed at level 2, and both `setState`'s no-op
 * arm and every `process` block that does not change state are legitimately
 * silent.  So non-emptiness is asserted once per run instead.
 */
static void
ecx_transcript(unsigned lvl, long tag, int *printed)
{
	diff_eq_int("transcript matches (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("line counts match (%ld)",
		    (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), tag);
	if (lvl > 1) {
		if (dsplib_debug_capture_lines(1) > 0) {
			*printed = 1;
			transcripts_seen = 1;
		}
	} else {
		diff_eq_int("below the gate ours was silent (%ld)",
			    (int)dsplib_debug_capture_lines(0), 0, tag);
		diff_eq_int("below the gate the blob was silent (%ld)",
			    (int)dsplib_debug_capture_lines(1), 0, tag);
	}
}

/*
 * The two slot images, with the four words that are each side's own address
 * neutralised -- params, arma, echoCoeff, echoHistory.  Everything else is
 * compared, including the two cursors and the two betas.
 */
static void
ecx_cmp_slot(const char *what, long tag)
{
	unsigned char sa[EC_SLOT], sb[EC_SLOT];

	memcpy(sa, ec_a.raw, EC_SLOT);
	memcpy(sb, ec_b.raw, EC_SLOT);
	memset(sa + 0x00, 0, 8);
	memset(sb + 0x00, 0, 8);
	memset(sa + 0x20, 0, 8);
	memset(sb + 0x20, 0, 8);
	diff_eq_obj_(__FILE__, __LINE__, what, "V92EchoCanceller slot", sa, sb,
		     EC_SLOT, tag);
}

/* --------------------------------- V92EchoCanceller::setState (1104 B) */

/*
 * EVERY VALUE THE DISPATCH TESTS, FROM EVERY VALUE IT COULD BE IN, and four
 * that are outside it.  The illegal arm, the no-op arm and the coefficient
 * dump are each reachable only from particular pairs, so the sweep is the
 * whole 8x8 square rather than a diagonal: the dump needs old in {2,3} and
 * new == 0, the no-op needs old == new, and the illegal arm needs a new
 * outside 0..3.  All three are asserted to have been reached.
 *
 * THE FILTER LENGTH IS SWEPT WITH IT because the dump is `filterLength` calls
 * to `edprintf` -- zero of them when the length is zero, which is a different
 * transcript, and one per coefficient otherwise.  The coefficients are
 * whatever the seed left, so the printed floats are wild by construction.
 */
static int
run_ec_setstate(void)
{
	static const int st[] = { 0, 1, 2, 3, -1, 4, 7, 0x7fffffff };
	static const unsigned int flen[] = { 0u, 1u, 3u, 8u, ECX_COEFF };
	static const int allow[] = { 0x08, 0x0c, 0x10, 0x30, 0x34 };
	const int nst = (int)(sizeof(st) / sizeof(st[0]));
	const int nfl = (int)(sizeof(flen) / sizeof(flen[0]));
	int seen[5] = { 0, 0, 0, 0, 0 };
	int printed = 0, sawSame = 0, sawDump = 0, sawNoDump = 0, sawIllegal = 0;
	int o, n, v;
	unsigned lvl;
	int trial = 0;

	diff_begin("V92EchoCanceller::setState");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);

		for (o = 0; o < nst; o++)
		for (n = 0; n < nst; n++)
		for (v = 0; v < 2; v++, trial++) {
			unsigned char before[EC_SLOT];
			float gbefore[ECX_COEFF + ECX_GUARD];
			unsigned int fl = flen[(o + n + v) % nfl];
			unsigned int dur = 0x30000000u + 7u * (unsigned)trial;
			long tag = (long)lvl * 100000 + trial;
			int side, bad, first, i;
			int dump = (st[o] == 2 || st[o] == 3) && st[n] == 0;

			fill_pair(ec_a.raw, ec_b.raw, EC_SLOT, trial, trial & 3);
			fill_pair(ecx_coeff[0], ecx_coeff[1],
				  (unsigned)sizeof(ecx_coeff[0]), trial + 1,
				  trial & 3);
			fill_pair(ecx_parm[0], ecx_parm[1], ECR_PARM,
				  trial + 2, trial & 3);

			/*
			 * The six the method reads, the same into both.
			 *
			 * INDEXED BY `o` AND `v`, NOT BY THE TRIAL NUMBER.
			 * Only two of the eight new states load a beta at
			 * all, so a schedule that walks the patterns with the
			 * trial reaches those two arms on a fixed residue and
			 * never shows them the rest -- the first spelling
			 * here stepped by five modulo twelve and could not
			 * reach the NaN in either training arm, which left
			 * `sign_of`'s unordered arm untested and a mutation
			 * of it alive.  `o * 2 + v` runs 0..15 inside each
			 * arm, so every pattern reaches both.  Same trap as
			 * the unsatisfiable `still` schedule in `run_ec`.
			 */
			ecx_param_word(ECX_FAST_BETA,
				       ecx_pat[(o * 2 + v + 0) % ECX_NPAT]);
			ecx_param_word(ECX_FAST_DECAY,
				       ecx_pat[(o * 2 + v + 3) % ECX_NPAT]);
			ecx_param_word(ECX_SLOW_BETA,
				       ecx_pat[(o * 2 + v + 6) % ECX_NPAT]);
			ecx_param_word(ECX_SLOW_DECAY,
				       ecx_pat[(o * 2 + v + 9) % ECX_NPAT]);
			ecx_param_word(ECX_FAST_DUR, dur);
			ecx_param_word(ECX_SLOW_DUR, dur ^ 0x0f0f0f0fu);

			/*
			 * The dump prints whatever the coefficients hold, and
			 * a seed is a NaN, or a negative with a whole part,
			 * only by luck -- which is what `sign_of`'s unordered
			 * arm and `whole_of`'s magnitude turn on.  The first
			 * `filterLength` entries are the patterns; past them
			 * the seed stands, so a dump one entry long is still
			 * a different transcript.
			 */
			for (i = 0; i < (int)fl; i++)
				ecx_coeff[0][i] = ecx_coeff[1][i] =
					ecx_bits(ecx_pat[(i + trial)
							 % ECX_NPAT]);

			for (side = 0; side < 2; side++) {
				V92EchoCanceller *e = side == 0 ? &ec_a.o
								: &ec_b.o;

				e->params = (V92Parameters *)ecx_parm[side];
				e->arma = 0;
				e->echoCoeff = ecx_coeff[side];
				e->echoHistory = 0;
				e->filterLength = fl;
				e->state = (V92EchoCancellerState)st[o];
				e->echoDelay = 500u + 13u * (unsigned)trial;
				/*
				 * A value the arms that clear it can never
				 * leave behind, so `only_wrote` sees the
				 * store even when the field held zero.
				 */
				e->word_10 = 0x11223344u;
			}

			memcpy(before, ec_b.raw, EC_SLOT);
			memcpy(gbefore, ecx_coeff[1], sizeof gbefore);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			ec_a.o.setState((V92EchoCancellerState)st[n]);
			ref_ec_setState(&ec_b.o, st[n]);

			dsplib_debug_capture_on = 0;

			ecx_cmp_slot("after setState", tag);
			diff_eq_obj_(__FILE__, __LINE__, "after setState",
				     "echoCoeff and its guard", ecx_coeff[0],
				     ecx_coeff[1], sizeof(ecx_coeff[0]), tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(ec_a.raw + sizeof(ec_a.o),
					   ec_b.raw + sizeof(ec_b.o),
					   EC_SLOT - sizeof(ec_a.o)) == 0,
				    1, tag);
			/*
			 * THE METHOD READS THE COEFFICIENTS AND MUST NOT WRITE
			 * THEM.  Compared against the pre-call image, not
			 * against the other side, so the two of us dumping and
			 * corrupting together would still fail.
			 */
			diff_eq_int("the blob left echoCoeff alone (%ld)",
				    memcmp(gbefore, ecx_coeff[1],
					   sizeof gbefore) == 0, 1, tag);

			bad = only_wrote(before, ec_b.raw, EC_SLOT, allow, 5,
					 seen, &first);
			diff_eq_int("the blob wrote outside the five fields "
				    "at +0x%lx", bad == 0 ? -1 : first, -1,
				    tag);

			/*
			 * What the blob's object holds, spelled independently
			 * of the source under test.
			 */
			if (st[o] == st[n]) {
				diff_eq_int("a repeated state writes nothing "
					    "(%ld)",
					    memcmp(before, ec_b.raw, EC_SLOT)
					    == 0, 1, tag);
				sawSame = 1;
			} else {
				diff_eq_int("the sample count restarted (%ld)",
					    (long)ec_b.o.word_10, 0, tag);
			}

			if (st[n] == 0 && st[o] != 0) {
				diff_eq_int("the blob's state is FILTER_ONLY "
					    "(%ld)", (long)ec_b.o.state, 0,
					    tag);
				diff_eq_int("the blob cleared echoBeta (%ld)",
					    ecx_same_bits(ec_b.o.echoBeta,
							  ecx_bits(0u)), 1,
					    tag);
				diff_eq_int("the blob cleared echoBetaDecay "
					    "(%ld)",
					    ecx_same_bits(ec_b.o.echoBetaDecay,
							  ecx_bits(0u)), 1,
					    tag);
				diff_eq_int("and left the duration alone "
					    "(%ld)",
					    memcmp(before + 0x0c,
						   ec_b.raw + 0x0c, 4) == 0,
					    1, tag);
				if (dump && fl > 0)
					sawDump = 1;
				else
					sawNoDump = 1;
			} else if (st[n] == 1 && st[o] != 1) {
				diff_eq_int("the blob's state is COUNT_DELAY "
					    "(%ld)", (long)ec_b.o.state, 1,
					    tag);
				diff_eq_int("the duration is the delay plus "
					    "400 (%ld)",
					    (long)ec_b.o.updateDuration,
					    (long)(ec_b.o.echoDelay + 400u),
					    tag);
			} else if ((st[n] == 2 || st[n] == 3)
				   && st[o] != st[n]) {
				unsigned int bo = st[n] == 2 ? ECX_FAST_BETA
							     : ECX_SLOW_BETA;
				unsigned int co = st[n] == 2 ? ECX_FAST_DECAY
							     : ECX_SLOW_DECAY;
				unsigned int du = st[n] == 2 ? ECX_FAST_DUR
							     : ECX_SLOW_DUR;

				diff_eq_int("the blob's state is the training "
					    "one (%ld)", (long)ec_b.o.state,
					    st[n], tag);
				diff_eq_int("echoBeta is the parameter (%ld)",
					    memcmp(&ec_b.o.echoBeta,
						   &ecx_parm[1][bo], 4) == 0,
					    1, tag);
				diff_eq_int("echoBetaDecay is the parameter "
					    "(%ld)",
					    memcmp(&ec_b.o.echoBetaDecay,
						   &ecx_parm[1][co], 4) == 0,
					    1, tag);
				diff_eq_int("the duration is the parameter "
					    "(%ld)",
					    memcmp(&ec_b.o.updateDuration,
						   &ecx_parm[1][du], 4) == 0,
					    1, tag);
			} else if (st[n] > 3 || st[n] < 0) {
				diff_eq_int("an illegal state leaves the "
					    "state alone (%ld)",
					    (long)ec_b.o.state, st[o], tag);
				diff_eq_int("and the two betas (%ld)",
					    memcmp(before + 0x30,
						   ec_b.raw + 0x30, 8) == 0,
					    1, tag);
				diff_eq_int("and the duration (%ld)",
					    memcmp(before + 0x0c,
						   ec_b.raw + 0x0c, 4) == 0,
					    1, tag);
				sawIllegal = 1;
			}

			/*
			 * The dump is `filterLength` lines on top of the
			 * three the transition prints, so at level 2 a
			 * transition that dumps a long filter cannot be
			 * confused with one that does not.
			 */
			if (lvl > 1 && dump && fl >= 8)
				diff_eq_int("the blob dumped the filter "
					    "(%ld)",
					    dsplib_debug_capture_lines(1)
					    > fl, 1, tag);

			ecx_transcript(lvl, tag, &printed);

			/* Nothing may have crept past the guard either. */
			for (i = ECX_COEFF; i < ECX_COEFF + ECX_GUARD; i++)
				diff_eq_int("the coefficient guard is intact "
					    "(%ld)",
					    memcmp(&gbefore[i],
						   &ecx_coeff[1][i], 4) == 0,
					    1, tag);
		}
	}

	set_level(0);
	diff_eq_int("the state is written", seen[0], 1, 0);
	diff_eq_int("the duration is written", seen[1], 1, 0);
	diff_eq_int("the sample count is written", seen[2], 1, 0);
	diff_eq_int("echoBeta is written", seen[3], 1, 0);
	diff_eq_int("echoBetaDecay is written", seen[4], 1, 0);
	diff_eq_int("the no-op arm was reached", sawSame, 1, 0);
	diff_eq_int("the coefficient dump ran", sawDump, 1, 0);
	diff_eq_int("and was skipped", sawNoDump, 1, 0);
	diff_eq_int("the illegal arm was reached", sawIllegal, 1, 0);
	diff_eq_int("the diagnostics were reached", printed, 1, 0);

	return diff_end();
}

/* -------------------------- V92EchoCanceller::updateEchoHistory (244 B) */

/*
 * THE FAST PATH AND THE COMPACTION ARE DIFFERENT CODE and the guard between
 * them is `echoLength + count < historyAlloc`, so the shapes below straddle
 * it: one that fits by a single sample, one that misses by one, and two that
 * compact several times in one call.  Each shape is driven for six
 * consecutive calls with the object carried over, because the compaction is
 * the only thing that moves `echoLength` DOWN and one call is not enough to
 * reach it twice.
 *
 * `historyAlloc` IS SET BELOW THE DECLARED BUFFER on two shapes, so the
 * region between it and the guard is a second, wider guard: the writer must
 * never touch a word at or past `historyAlloc`, and that is checked against
 * the pre-call image rather than against the other side.
 *
 * THE COMPACTION IS NOT DRIVEN WITH `filterLength < 2`.  Its loop is
 * bottom-tested with a count of `filterLength - 1` (D273), so one tap would
 * ask it to copy four billion words over the top of everything; the two
 * shapes with a short filter stay on the fast path, where the count is never
 * used.
 */

/* den[0] is exactly 1.0f, so FloatARMA's constructor skips the rescale. */
static float ecx_den[2][4] = {
	{ 1.0f, -0.4f, 0.15f, 0.05f },
	{ 1.0f, -0.4f, 0.15f, 0.05f }
};
static float ecx_num[2][4] = {
	{ 0.5f, 0.3f, 0.2f, 0.1f },
	{ 0.5f, 0.3f, 0.2f, 0.1f }
};

static int
run_ec_update(void)
{
	static const struct {
		unsigned int fl, alloc, len, count;
	} shape[] = {
		{ 16u, ECX_HIST,	0u,		40u },
		{ 16u, ECX_HIST,	100u,		40u },
		{ 16u, ECX_HIST,	ECX_HIST - 41u,	40u },	/* fits by 1 */
		{ 16u, ECX_HIST,	ECX_HIST - 40u,	40u },	/* misses by 1 */
		{ 16u, ECX_HIST,	ECX_HIST - 1u,	1u },
		{ 16u, ECX_HIST,	ECX_HIST - 1u,	40u },
		{ 16u, 64u,		60u,		40u },
		{ 2u,  64u,		60u,		40u },
		/*
		 * THE ONE SHAPE THAT MAKES THE COPY'S DIRECTION VISIBLE.  The
		 * compaction moves `filterLength - 1` samples from the top of
		 * the buffer to the bottom, and the two regions only OVERLAP
		 * when `historyAlloc - 1 <= 2 * (filterLength - 2)`: 40 taps
		 * in 64 words is source [25,63] into destination [0,38], and
		 * the object's downward copy reads words it has already
		 * written where an upward one would not.  Every other shape
		 * here has them disjoint, where the two directions agree.
		 */
		{ 40u, 64u,		60u,		40u },
		{ 40u, ECX_HIST,	ECX_HIST - 1u,	128u },
		{ 16u, ECX_HIST,	0u,		0u },
		{ 1u,  ECX_HIST,	0u,		40u },
		{ 0u,  ECX_HIST,	0u,		40u }
	};
	const int nshape = (int)(sizeof(shape) / sizeof(shape[0]));
	int printed = 0, sawFast = 0, sawCompact = 0, sawWrote = 0;
	int trial;
	unsigned lvl;

	diff_begin("V92EchoCanceller::updateEchoHistory");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level(lvl);

		for (trial = 0; trial < nshape; trial++) {
			unsigned int fl = shape[trial].fl;
			unsigned int alloc = shape[trial].alloc;
			unsigned int count = shape[trial].count;
			int side, call, i;
			float hbefore[ECX_LEAD + ECX_HIST + ECX_GUARD];

			fill_pair(ec_a.raw, ec_b.raw, EC_SLOT, trial, trial & 3);
			fill_pair(ecx_hist[0], ecx_hist[1],
				  (unsigned)sizeof(ecx_hist[0]), trial + 1,
				  trial & 3);
			fill_pair(ecx_arma[0], ecx_arma[1],
				  (unsigned)sizeof(ecx_arma[0]), trial + 2,
				  trial & 3);

			our_arma_ctor(ecx_arma[0], 4u, 4u, ecx_den[0],
				      ecx_num[0], 64u);
			ref_arma_ctor(ecx_arma[1], 4u, 4u, ecx_den[1],
				      ecx_num[1], 64u);

			for (side = 0; side < 2; side++) {
				V92EchoCanceller *e = side == 0 ? &ec_a.o
								: &ec_b.o;

				e->params = 0;
				e->arma = (FloatARMA *)ecx_arma[side];
				e->echoCoeff = 0;
				e->echoHistory = ECX_H(side);
				e->filterLength = fl;
				e->word_18 = fl - 1u;
				e->historyAlloc = alloc;
				e->echoLength = shape[trial].len;
				e->historyIndex = 0x55aa55aau;
			}

			for (call = 0; call < 6; call++) {
				long tag = (long)lvl * 100000 + trial * 100
					   + call;
				unsigned int lenBefore = ec_b.o.echoLength;
				FloatARMA *ma = (FloatARMA *)ecx_arma[0];
				FloatARMA *mb = (FloatARMA *)ecx_arma[1];
				unsigned char aa[sizeof(FloatARMA)];
				unsigned char ab[sizeof(FloatARMA)];

				for (i = 0; i < (int)count; i++)
					ecx_in[i] = (float)((i & 7) - 3)
						    * 0.125f
						    + (float)(call + trial)
						      * 0.0625f;

				memcpy(hbefore, ecx_hist[1], sizeof hbefore);

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();

				ec_a.o.updateEchoHistory(ecx_in, count);
				ref_ec_update(&ec_b.o, ecx_in, count);

				dsplib_debug_capture_on = 0;

				ecx_cmp_slot("after updateEchoHistory", tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after updateEchoHistory",
					     "echoHistory and its guard",
					     ecx_hist[0], ecx_hist[1],
					     sizeof(ecx_hist[0]), tag);

				/* The ARMA the writer runs every sample. */
				memcpy(aa, ecx_arma[0], sizeof aa);
				memcpy(ab, ecx_arma[1], sizeof ab);
				memset(aa, 0, 16);
				memset(ab, 0, 16);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after updateEchoHistory",
					     "the ARMA object", aa, ab,
					     sizeof aa, tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after updateEchoHistory",
					     "the ARMA x history", ma->m_xhist,
					     mb->m_xhist, ma->m_xlen * 4, tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after updateEchoHistory",
					     "the ARMA y history", ma->m_yhist,
					     mb->m_yhist, ma->m_ylen * 4, tag);

				/*
				 * NOTHING AT OR PAST `historyAlloc`, ever.
				 * This is the bound D72 says the other
				 * consumers do not share, checked against the
				 * seed so that both sides overrunning
				 * together is still a failure.
				 */
				diff_eq_int("the blob wrote nothing past "
					    "historyAlloc (%ld)",
					    memcmp(&hbefore[ECX_LEAD + alloc],
						   &ecx_hist[1][ECX_LEAD
								+ alloc],
						   (ECX_HIST + ECX_GUARD
						    - alloc) * 4) == 0,
					    1, tag);
				diff_eq_int("or before the buffer (%ld)",
					    memcmp(hbefore, ecx_hist[1],
						   ECX_LEAD * 4) == 0, 1,
					    tag);
				diff_eq_int("the write cursor stayed inside "
					    "(%ld)",
					    ec_b.o.echoLength < alloc
					    || count == 0, 1, tag);
				/* The reader's cursor is not this one's. */
				diff_eq_int("historyIndex is untouched (%ld)",
					    (long)ec_b.o.historyIndex,
					    (long)0x55aa55aau, tag);

				if (count != 0) {
					if (ec_b.o.echoLength
					    == lenBefore + count)
						sawFast = 1;
					if (ec_b.o.echoLength < lenBefore)
						sawCompact = 1;
					if (memcmp(hbefore, ecx_hist[1],
						   sizeof hbefore) != 0)
						sawWrote = 1;
				}

				ecx_transcript(lvl, tag, &printed);
			}

			our_arma_dtor(ecx_arma[0]);
			ref_arma_dtor(ecx_arma[1]);
		}
	}

	set_level(0);
	diff_eq_int("the fast path ran", sawFast, 1, 0);
	diff_eq_int("the compaction ran", sawCompact, 1, 0);
	diff_eq_int("the history was written", sawWrote, 1, 0);
	diff_eq_int("neither side printed anything", printed, 0, 0);

	return diff_end();
}

/* ----------------------------------- V92EchoCanceller::process (794 B) */

/*
 * The echo path the input carries: six taps, alternating sign, decaying.
 * Applied to the same window the canceller is about to read, so the LMS
 * update has an optimum to walk towards and `sawMoved` is not an accident of
 * arithmetic on garbage.
 */
static float
ecx_echo_of(const float *hist, unsigned int at)
{
	static const float g[ECX_ECHO] = {
		0.60f, -0.42f, 0.29f, -0.20f, 0.14f, -0.10f
	};
	double s = 0.0;
	int k;

	for (k = 0; k < ECX_ECHO; k++)
		s += (double)g[k] * (double)hist[at + k];
	return (float)s;
}

static int
run_ec_process(void)
{
	static const struct {
		int state;
		unsigned int fl, blocks, count, dur;
		int wide;
	} shape[] = {
		{ 2, 16u, 12u, 40u, 200u, 0 },	/* fast training -> slow    */
		{ 3, 16u, 12u, 40u, 200u, 0 },	/* slow training -> filter  */
		{ 0, 16u,  6u, 40u, 200u, 0 },	/* filter only: no counter  */
		{ 1, 16u,  6u, 40u, 120u, 0 },	/* count delay -> fast      */
		{ 2,  0u,  4u, 40u, 200u, 0 },	/* the zero-length arm      */
		{ 2,  1u,  4u, 40u, 200u, 0 },	/* tail loop only           */
		{ 2,  3u,  4u, 40u, 200u, 0 },	/* tail loop, three         */
		{ 2,  4u,  4u, 40u, 200u, 0 },	/* one unrolled pass        */
		{ 2,  5u,  4u, 40u, 200u, 0 },	/* one pass and a tail      */
		{ 7, 16u,  4u, 40u, 100u, 0 },	/* an illegal state adapts  */
		{ 2, 16u,  3u,  0u,  60u, 0 },	/* an empty block           */
		{ 2, 16u,  6u,  1u,   3u, 0 },	/* one sample at a time     */
		{ 1, 16u,  3u, 128u, 300u, 0 },	/* a block past the modulus */
		{ 2, 16u, 44u,  8u, 400u, 0 },	/* enough blocks to wrap    */
		/*
		 * THE ONE SHAPE THAT CAN SEE THE SUMMATION ORDER.  The
		 * object's dot product runs two accumulators and adds them at
		 * the end, and over ordinary data that is INVISIBLE: a
		 * product of two floats needs 48 significand bits and the
		 * accumulator has 64, so sixteen taps of similar magnitude
		 * sum EXACTLY however they are grouped, and a merged-
		 * accumulator mutation passes every check.  What the grouping
		 * decides is where a cancellation lands, so this shape gives
		 * the filter a uniform history and coefficients spanning
		 * 2**100: the even taps cancel each other exactly and the odd
		 * ones are far below the rounding of the pair.  Two
		 * accumulators keep the small terms, one loses all but the
		 * last of them.  Recorded because the merged form is a
		 * mutation that must be caught rather than argued about.
		 */
		{ 2, 16u,  6u, 40u, 400u, 1 }	/* wide dynamic range       */
	};
	const int nshape = (int)(sizeof(shape) / sizeof(shape[0]));
	int printed = 0, sawMoved = 0, sawVaried = 0, sawWrap = 0;
	int sawSentinel = 0, sawTransition = 0, sawCut = 0;
	int trial;
	unsigned lvl;

	diff_begin("V92EchoCanceller::process");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);

		for (trial = 0; trial < nshape; trial++) {
			unsigned int fl = shape[trial].fl;
			unsigned int count = shape[trial].count;
			unsigned int mod = ECX_HIST - (fl - 1u);
			unsigned int lfsr = 0xace1u + 7u * (unsigned)trial;
			float prev[ECX_COEFF + ECX_GUARD];
			float lastOut = 0.0f;
			int side, blk, i;

			fill_pair(ec_a.raw, ec_b.raw, EC_SLOT, trial, trial & 3);
			fill_pair(ecx_coeff[0], ecx_coeff[1],
				  (unsigned)sizeof(ecx_coeff[0]), trial + 1,
				  trial & 3);
			fill_pair(ecx_hist[0], ecx_hist[1],
				  (unsigned)sizeof(ecx_hist[0]), trial + 2,
				  trial & 3);
			fill_pair(ecx_parm[0], ecx_parm[1], ECR_PARM,
				  trial + 3, trial & 3);
			ecx_param_word(ECX_FAST_BETA, 0x3ca3d70au);/* 0.02f */
			ecx_param_word(ECX_FAST_DECAY, 0x3f7ff972u);/* .99990 */
			ecx_param_word(ECX_SLOW_BETA, 0x3c23d70au);/* 0.01f */
			ecx_param_word(ECX_SLOW_DECAY, 0x3f7fbe77u);/* .99900 */
			ecx_param_word(ECX_FAST_DUR, shape[trial].dur);
			ecx_param_word(ECX_SLOW_DUR, shape[trial].dur);

			/*
			 * The history is the echo SOURCE: a persistently
			 * exciting +-0.5 sequence, identical on both sides,
			 * written over the seed only as far as `historyAlloc`
			 * so the guard past it keeps its varied bytes.
			 */
			for (i = 0; i < ECX_HIST; i++) {
				float s;

				lfsr = (lfsr >> 1)
				       ^ (-(int)(lfsr & 1u) & 0xb400u);
				s = (lfsr & 1u) ? 0.5f : -0.5f;
				if (shape[trial].wide)
					s = 0.5f;
				ECX_H(0)[i] = ECX_H(1)[i] = s;
			}
			/* Small, varied, and never zero -- finding F230. */
			for (i = 0; i < (int)fl; i++) {
				float c = (float)((i % 5) - 2) * 0.03125f
					  + 0.015625f;

				if (shape[trial].wide)
					c = (i & 1) ? 1.0e-10f
					    : ((i & 2) ? -1.0e20f : 1.0e20f);
				ecx_coeff[0][i] = ecx_coeff[1][i] = c;
			}

			for (side = 0; side < 2; side++) {
				V92EchoCanceller *e = side == 0 ? &ec_a.o
								: &ec_b.o;

				e->params = (V92Parameters *)ecx_parm[side];
				e->arma = 0;
				e->echoCoeff = ecx_coeff[side];
				e->echoHistory = ECX_H(side);
				e->filterLength = fl;
				e->word_18 = fl - 1u;
				e->historyAlloc = ECX_HIST;
				e->echoLength = 0x33333333u;
				e->historyIndex = 0u;
				e->state = (V92EchoCancellerState)
					   shape[trial].state;
				e->echoDelay = 120u;
				e->updateDuration = shape[trial].dur;
				e->word_10 = 0u;
				e->echoBeta = ecx_bits(0x3ca3d70au);
				e->echoBetaDecay = ecx_bits(0x3f7ff972u);
			}
			memcpy(prev, ecx_coeff[1], sizeof prev);

			for (blk = 0; blk < (int)shape[trial].blocks; blk++) {
				long tag = (long)lvl * 1000000 + trial * 1000
					   + blk;
				unsigned int at = ec_b.o.historyIndex;
				unsigned int hi0 = ec_b.o.historyIndex;
				unsigned int stBefore = (unsigned int)
							ec_b.o.state;
				float hbefore[ECX_LEAD + ECX_HIST
					      + ECX_GUARD];
				/*
				 * ORDINARY OR SENTINEL, AND NEVER A NaN.
				 * `out[0]` decides the path and the third
				 * value it used to take here was a quiet NaN,
				 * which the object's single `fcoms`/`je`
				 * treats as EQUAL and so filters -- an
				 * unordered code sets ZF exactly as an equal
				 * one does.  GCC 13 emits the parity test
				 * whatever it is told (finding F2304), so the
				 * modern build runs the filter on that block
				 * instead, and from there every later block
				 * of the trial diverges: 273 of this group's
				 * 4570 checks, all of them downstream of one
				 * block.  That made the whole BINARY red on
				 * the modern build, and a red binary cannot
				 * score a mutation set at all -- five suites
				 * pinned here went unscoreable for one arm
				 * (findings F2157 and F3002).
				 *
				 * So the unordered arm now lives in
				 * `t_v92ecnan`, its own binary, where it is
				 * declared in `tools/gccdiverge.json`.
				 * NOTHING ELSE MOVES: the blob takes the same
				 * path for 177.0f as for a NaN, so every
				 * block here evolves exactly as it did and
				 * the group keeps all fifteen shapes, three
				 * levels and both cursors.  Finding F6000.
				 */
				int sentinel = (blk % 7) == 6 ? 1 : 0;

				/*
				 * The input: near end plus the echo of the
				 * window the canceller is about to walk.
				 */
				for (i = 0; i < (int)count; i++) {
					unsigned int a = at;

					ecx_in[i] = 0.05f
						    * (float)(((blk + i) % 9)
							      - 4)
						    + ecx_echo_of(ECX_H(1),
								  a);
					at = (at + 1u == mod) ? 0u : at + 1u;
				}

				fill_pair(ecx_out[0], ecx_out[1],
					  (unsigned)sizeof(ecx_out[0]),
					  trial * 40 + blk, blk & 3);
				/*
				 * `out[0]` DECIDES THE PATH, so it is never
				 * left to the seed: 0.25f for the ordinary
				 * one and 177.0f for the sentinel.  The
				 * unordered arm is `t_v92ecnan`'s -- see the
				 * note where `sentinel` is computed.
				 */
				ecx_out[0][0] = ecx_out[1][0] =
					sentinel == 0 ? 0.25f : 177.0f;
				if (sentinel == 1)
					sawSentinel = 1;

				memcpy(hbefore, ecx_hist[1], sizeof hbefore);

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();

				ec_a.o.process(ecx_in, ecx_out[0], count);
				ref_ec_process(&ec_b.o, ecx_in, ecx_out[1],
					       count);

				dsplib_debug_capture_on = 0;

				ecx_cmp_slot("after process", tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after process",
					     "the output block and its guard",
					     ecx_out[0], ecx_out[1],
					     sizeof(ecx_out[0]), tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after process",
					     "echoCoeff and its guard",
					     ecx_coeff[0], ecx_coeff[1],
					     sizeof(ecx_coeff[0]), tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after process",
					     "echoHistory and its guard",
					     ecx_hist[0], ecx_hist[1],
					     sizeof(ecx_hist[0]), tag);
				diff_eq_int("no store past the object (%ld)",
					    memcmp(ec_a.raw + sizeof(ec_a.o),
						   ec_b.raw + sizeof(ec_b.o),
						   EC_SLOT - sizeof(ec_a.o))
					    == 0, 1, tag);

				/*
				 * `process` READS the history and never
				 * writes it -- checked against the pre-call
				 * image, so both sides writing it together
				 * still fails.
				 */
				diff_eq_int("the blob left echoHistory alone "
					    "(%ld)",
					    memcmp(hbefore, ecx_hist[1],
						   sizeof hbefore) == 0, 1,
					    tag);
				/* The writer's cursor is not this one's. */
				diff_eq_int("echoLength is untouched (%ld)",
					    (long)ec_b.o.echoLength,
					    (long)0x33333333u, tag);
				diff_eq_int("the read cursor stayed inside "
					    "the modulus (%ld)",
					    ec_b.o.historyIndex < mod, 1, tag);
				/*
				 * The window it will read next must fit: this
				 * is D72's arithmetic on the OTHER cursor,
				 * and the guard past ECX_HIST is what would
				 * catch it if it did not.
				 */
				diff_eq_int("and the window past it fits "
					    "(%ld)",
					    ec_b.o.historyIndex + fl
					    <= ECX_HIST, 1, tag);

				if (sentinel != 0) {
					diff_eq_int("the sentinel copied the "
						    "block (%ld)",
						    count == 0
						    || memcmp(ecx_in + 1,
							      &ecx_out[1][1],
							      (count - 1) * 4)
						       == 0, 1, tag);
					diff_eq_int("and left the state alone "
						    "(%ld)",
						    (unsigned int)
						    ec_b.o.state, stBefore,
						    tag);
				} else {
					if ((unsigned int)ec_b.o.state
					    != stBefore)
						sawTransition = 1;
					if (memcmp(prev, ecx_coeff[1],
						   sizeof prev) != 0)
						sawMoved = 1;
					if (count != 0 && fl != 0
					    && memcmp(ecx_in, ecx_out[1],
						      count * 4) != 0)
						sawCut = 1;
					if (count != 0
					    && !ecx_same_bits(lastOut,
							      ecx_out[1][0]))
						sawVaried = 1;
				}
				if (count != 0)
					lastOut = ecx_out[1][0];
				/*
				 * THE CURSOR WENT BACKWARDS, which it can
				 * only do by wrapping.  `at` is this file's
				 * own copy and ends where the object's does,
				 * so comparing against IT proves nothing --
				 * the flag it fed was satisfied by the empty
				 * block at the start of a trial and never by
				 * a wrap.
				 */
				if (count != 0 && ec_b.o.historyIndex < hi0)
					sawWrap = 1;

				memcpy(prev, ecx_coeff[1], sizeof prev);
				ecx_transcript(lvl, tag, &printed);
			}
		}
	}

	set_level(0);
	diff_eq_int("the adaptation moved the coefficients", sawMoved, 1, 0);
	diff_eq_int("and did not produce one repeated answer", sawVaried, 1, 0);
	diff_eq_int("the output differs from the input", sawCut, 1, 0);
	diff_eq_int("the read cursor wrapped", sawWrap, 1, 0);
	diff_eq_int("the 177.0f path was taken", sawSentinel, 1, 0);
	diff_eq_int("process drove a state transition", sawTransition, 1, 0);
	diff_eq_int("the transitions were announced", printed, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_cd();
	rc |= run_sd();
	rc |= run_sv();
	rc |= run_ec();
	rc |= run_ec_reset();
	rc |= run_ec_setstate();
	rc |= run_ec_update();
	rc |= run_ec_process();
	rc |= run_ec_dtor();
	rc |= run_ec_ctor();
	rc |= run_rt();
	rc |= run_p4();

	/* Task #88's lifecycle members. */
	rc |= run_cd_reset();
	rc |= run_ce();
	rc |= run_ce_lifecycle();
	rc |= run_k56();

	/* The V90Demapper destructor path. */
	rc |= run_sbe_lifecycle();
	rc |= run_dem_histogram();
	rc |= run_dem_lifecycle();

	set_level(0);
	dsplib_debug_capture_on = 0;

	diff_begin("the diagnostic tier ran at all");
	diff_eq_int("some transcript was captured", transcripts_seen, 1, 0);
	rc |= diff_end();

	return rc;
}
