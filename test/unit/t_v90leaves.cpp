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
 * moves across the whole run is a failure too -- that is findings 223 and
 * 224's rule, that a field the function never writes proves nothing, turned
 * into a check rather than a hope.
 *
 * THE OBJECTS ARE NEVER ZEROED (finding 230).  Both sides get the same varied
 * pseudorandom bytes before every call and are reseeded every trial, so a
 * store that fails to happen is visible and a store of zero into memory that
 * was already zero is not mistaken for one.  Four seed modes, because a fill
 * whose low bits are constant exercises nothing that branches on them.
 *
 * EACH OBJECT SITS IN A UNION WITH A LARGER BYTE ARRAY, and the bytes past
 * the object are compared separately, so a store past the end shows up as a
 * failure rather than as silence.  No constructor or destructor is declared
 * for any of these classes, which is what keeps them trivial enough to live
 * in a union (docs/v90cpp.md).
 *
 * THE DIAGNOSTICS ARE COMPARED AS TEXT.  Three of the six print, and
 * `dsplibs_debug_level` ships at zero, so a wrong format string behaves
 * exactly like a right one in every other check (finding 180).  Both sides'
 * levels are swept 0..2 together and the two transcripts compared; at level 2
 * they must be non-empty, and below the gate both must be silent.  `edprintf`
 * encodes its output, so a transcript that matches is a format string, an
 * argument and a character count that all match.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * the ref_-prefixed mangled name as an identifier -- see t_v90jd.cpp.  The
 * convention is plain cdecl with `this` as the first stack argument (finding
 * 215), so no attribute is involved.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90SdDetector.h"
#include "dsplib/V90SpectralVerifier.h"
#include "dsplib/V92EchoCanceller.h"
#include "dsplib/ResamplerTimingOffset.h"
#include "dsplib/V90Phase4Modulator.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/K56FlexFloModem.h"
/*
 * The NAMED V90Parameters map: `V90ConstellationDesigner::reset` and
 * `V90ConnectionEvaluator::reset` read twenty slots out of the parameter
 * block between them, and this test seeds those slots by name.  Finding
 * 1112 is why only one of the two definitions may be included.
 */
#include "dsplib/V90Parameters.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_cd_setMinMaxRates(void *self, unsigned int lo, unsigned int hi)
	asm("ref__ZN24V90ConstellationDesigner14setMinMaxRatesEjj");
void ref_sd_reset(void *self) asm("ref__ZN13V90SdDetector5resetEv");
void ref_sv_reset(void *self) asm("ref__ZN19V90SpectralVerifier5resetEv");
void ref_ec_setEchoDelay(void *self, unsigned int d)
	asm("ref__ZN16V92EchoCanceller12setEchoDelayEj");
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

/* The same varied bytes into both sides.  Never zeros -- finding 230. */
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
 * finding 149 says to count: text alone can be filled by the harness.
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

union cd_slot {
	V90ConstellationDesigner o;
	unsigned char raw[CD_SLOT];
};

static union cd_slot cd_a, cd_b;

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

			cd_a.o.setMinMaxRates(lo, hi);
			ref_cd_setMinMaxRates(&cd_b.o, lo, hi);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after setMinMaxRates",
				    V90ConstellationDesigner,
				    &cd_a.o, &cd_b.o, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(cd_a.raw + sizeof(cd_a.o),
					   cd_b.raw + sizeof(cd_b.o),
					   CD_SLOT - sizeof(cd_a.o)) == 0,
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
				    (long)cd_b.o.minRate, (long)lo, tag);
			diff_eq_int("blob's maxRate (%ld)",
				    (long)cd_b.o.maxRate, (long)hi, tag);

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

union sd_slot {
	V90SdDetector o;
	unsigned char raw[SD_SLOT];
};

static union sd_slot sd_a, sd_b;
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
		 * skipped silently -- finding 224.
		 */
		sd_a.o.history = sd_ha;
		sd_b.o.history = sd_hb;
		sd_a.o.historyLength = sd_b.o.historyLength = n;

		/*
		 * Forced non-zero, or clearing it would be invisible: the
		 * pseudorandom fill hits zero once in 2^32 but seed mode 1
		 * and 2 never do and mode 3 rarely does.
		 */
		sd_a.o.count = sd_b.o.count = 0x5a5a0000u + (unsigned)trial;

		memcpy(before, sd_b.raw, SD_SLOT);
		memcpy(hbefore, sd_hb, sizeof hbefore);

		sd_a.o.reset();
		ref_sd_reset(&sd_b.o);

		diff_eq_int("our history pointer untouched (%ld)",
			    sd_a.o.history == sd_ha, 1, trial);
		diff_eq_int("the blob's history pointer untouched (%ld)",
			    sd_b.o.history == sd_hb, 1, trial);
		diff_eq_int("historyLength untouched (%ld)",
			    (long)sd_b.o.historyLength, (long)n, trial);

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
			    (long)sd_b.o.count, 0, trial);

		sd_a.o.history = sd_b.o.history = (float *)0;
		diff_eq_obj("after reset", V90SdDetector, &sd_a.o, &sd_b.o,
			    trial);
		diff_eq_int("no store past the object (%ld)",
			    memcmp(sd_a.raw + sizeof(sd_a.o),
				   sd_b.raw + sizeof(sd_b.o),
				   SD_SLOT - sizeof(sd_a.o)) == 0, 1, trial);
	}

	diff_eq_int("count is a word the function writes", seen[0], 1, 0);
	diff_eq_int("a non-empty history was cleared", cleared, 1, 0);
	diff_eq_int("an empty history skipped the loop", skipped, 1, 0);

	return diff_end();
}

/* --------------------------------------- V90SpectralVerifier (44 bytes) */

#define SV_SLOT 96

union sv_slot {
	V90SpectralVerifier o;
	unsigned char raw[SV_SLOT];
};

static union sv_slot sv_a, sv_b;

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
			sv_a.o.accumCount = sv_b.o.accumCount =
			    0x11110000u + (unsigned)trial;
			sv_a.o.accumulating = sv_b.o.accumulating =
			    1u + (unsigned)trial;
			sv_a.o.word_28 = sv_b.o.word_28 =
			    0x22220000u + (unsigned)trial;

			memcpy(before, sv_b.raw, SV_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			sv_a.o.reset();
			ref_sv_reset(&sv_b.o);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after reset", V90SpectralVerifier,
				    &sv_a.o, &sv_b.o, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(sv_a.raw + sizeof(sv_a.o),
					   sv_b.raw + sizeof(sv_b.o),
					   SV_SLOT - sizeof(sv_a.o)) == 0,
				    1, tag);

			bad = only_wrote(before, sv_b.raw, SV_SLOT, allow, 3,
					 seen, &first);
			diff_eq_int("the blob wrote outside +0x20..+0x28 at "
				    "+0x%lx", bad == 0 ? -1 : first, -1, tag);

			diff_eq_int("blob's accumCount (%ld)",
				    (long)sv_b.o.accumCount, 0, tag);
			diff_eq_int("blob's accumulating (%ld)",
				    (long)sv_b.o.accumulating, 0, tag);
			diff_eq_int("blob's word_28 (%ld)",
				    (long)sv_b.o.word_28, 0, tag);

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

union ec_slot {
	V92EchoCanceller o;
	unsigned char raw[EC_SLOT];
};

static union ec_slot ec_a, ec_b;

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

/* ------------------------------------- ResamplerTimingOffset (76 bytes) */

#define RT_SLOT 128

/*
 * The special members are written out because `ResamplerTimingOffset` is now
 * a real polymorphic class with a user-declared constructor and destructor,
 * which deletes a union's implicit ones.  Nothing here constructs the object;
 * the raw bytes are seeded and the member called on them, exactly as before.
 */
union rt_slot {
	ResamplerTimingOffset o;
	unsigned char raw[RT_SLOT];
	rt_slot() { }
	~rt_slot() { }
};

static union rt_slot rt_a, rt_b;

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
		 * 228's trap, and `only_wrote` below is what catches it.
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

union p4_slot {
	V90Phase4Modulator o;
	unsigned char raw[P4_SLOT];
};

static union p4_slot p4_a, p4_b;

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

		fill_pair(p4_a.raw, p4_b.raw, P4_SLOT, trial, trial & 3);

		/* Forced to differ from the value about to be stored. */
		p4_a.o.sessionFlag = p4_b.o.sessionFlag = ~f;

		memcpy(before, p4_b.raw, P4_SLOT);

		p4_a.o.setSessionFlag(f);
		ref_p4_setSessionFlag(&p4_b.o, f);

		diff_eq_obj("after setSessionFlag", V90Phase4Modulator,
			    &p4_a.o, &p4_b.o, trial);
		diff_eq_int("no store past the object (%ld)",
			    memcmp(p4_a.raw + sizeof(p4_a.o),
				   p4_b.raw + sizeof(p4_b.o),
				   P4_SLOT - sizeof(p4_a.o)) == 0, 1, trial);

		bad = only_wrote(before, p4_b.raw, P4_SLOT, allow, 1, seen,
				 &first);
		diff_eq_int("the blob wrote outside +0x0000 at +0x%lx",
			    bad == 0 ? -1 : first, -1, trial);
		diff_eq_int("blob's sessionFlag (%ld)",
			    (long)p4_b.o.sessionFlag, (long)f, trial);
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
			cd_a.o.params = PA;
			cd_b.o.params = PA;
			memcpy(before, cd_b.raw, CD_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			cd_a.o.reset();
			ref_cd_reset(&cd_b.o);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after reset", V90ConstellationDesigner,
				    &cd_a.o, &cd_b.o, tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(cd_a.raw + sizeof(cd_a.o),
					   cd_b.raw + sizeof(cd_b.o),
					   CD_SLOT - sizeof(cd_a.o)) == 0,
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
			 * never-reset objects compare equal (finding 1105),
			 * so the values are asserted and not only compared.
			 */
			diff_eq_int("blob's word_48 (%ld)",
				    (long)cd_b.o.word_48, 0, tag);
			diff_eq_int("blob's short_0a (%ld)",
				    (long)cd_b.o.short_0a, 0, tag);
			diff_eq_int("blob's short_0c (%ld)",
				    (long)cd_b.o.short_0c, 0, tag);
			diff_eq_int("blob's short_0e (%ld)",
				    (long)cd_b.o.short_0e, 0, tag);
			diff_eq_int("blob's short_10 (%ld)",
				    (long)cd_b.o.short_10, 0, tag);
			diff_eq_int("blob's word_24 is params->unnamed_39c "
				    "(%ld)", (long)cd_b.o.word_24,
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
	diff_eq_int("short_9e (%ld)", (long)o->short_9e, 0, tag);
	diff_eq_int("short_b0 (%ld)", (long)o->short_b0, 1, tag);
	diff_eq_int("short_b2 (%ld)", (long)o->short_b2, 0, tag);
	diff_eq_int("short_b4 (%ld)", (long)o->short_b4, 0, tag);
	diff_eq_int("word_04 (%ld)", (long)o->word_04, 0, tag);
	diff_eq_int("word_24 (%ld)", (long)o->word_24, 0, tag);
	diff_eq_int("word_a8 (%ld)", (long)o->word_a8, 0, tag);
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

	return diff_end();
}

/*
 * The constructor and the destructor.
 *
 * The constructor is "store the parameter block, then reset", so the check is
 * that a constructed object is byte-for-byte a reset one with +0x00 filled
 * in.  The destructor is a bare `ret`, and comparing two objects AFTER a
 * destructor would compare the allocator (finding 1105) -- so what is checked
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

int
main(void)
{
	int rc = 0;

	rc |= run_cd();
	rc |= run_sd();
	rc |= run_sv();
	rc |= run_ec();
	rc |= run_rt();
	rc |= run_p4();

	/* Task #88's lifecycle members. */
	rc |= run_cd_reset();
	rc |= run_ce();
	rc |= run_ce_lifecycle();
	rc |= run_k56();

	set_level(0);
	dsplib_debug_capture_on = 0;

	diff_begin("the diagnostic tier ran at all");
	diff_eq_int("some transcript was captured", transcripts_seen, 1, 0);
	rc |= diff_end();

	return rc;
}
