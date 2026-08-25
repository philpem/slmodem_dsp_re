/*
 * t_v90cdadjust.cpp -- differential test of the four members that close
 * `V90ConstellationDesigner`: `adjustConstellationsPower`,
 * `adjustConstellationsToNewK`, `constellationDesign` and `process`, each
 * against the blob's own copy.
 *
 * It is a sibling of `t_v90cdesign.cpp` and `t_v90cdnoise.cpp` rather than
 * another group inside either, for the reason `t_v90cdnoise` gives: the
 * fixture is its own -- two `V90ConstellationPower` objects, a detector-sized
 * buffer and eight input arrays -- and the wiring helpers are copied rather
 * than shared, which is the call both of those files already made.
 *
 * NOTHING IN THE OBJECT CALLS ANY OF THE FOUR, `process` included -- the sweep
 * of every `R_386_PC32` in `.text` that findings F2140 and F2141 record covers
 * them too -- so each is driven directly by symbol on both sides, ours by its
 * mangled name and the blob's by the `ref_` alias, through an `asm()` label.
 * Plain cdecl with `this` as the first STACK argument (finding F215), and a
 * `float` occupies one stack slot.
 *
 * WHAT IS COMPARED, and why each is compared the way it is:
 *
 *   the designer object   84 bytes, but FIVE OF THEM ARE POINTERS the two
 *                         sides hold different values for by construction.
 *                         `same_designer` copies ours, overwrites those five
 *                         with the blob's, and compares the result whole --
 *                         so every other byte is checked, including the ones
 *                         no member below is expected to touch.
 *   the power object      144 bytes, and `constellation` at +0 is a pointer
 *                         INTO the mapping block, so the same normalisation
 *                         applies.
 *   the mapping block     0x650 bytes, whole.  This is where nearly all of
 *                         the observable output lands.
 *   the parameter block   0x558 bytes, whole -- `process` WRITES six fields
 *                         of it and the rest must be untouched.
 *   the eight inputs      whole, against a snapshot, on both sides.  Six of
 *                         them are read-only claims; `topUcode` is the one
 *                         `setConstellationToNoise_forceRate` writes through.
 *   the return            `process` alone returns a value, and it is compared.
 *
 * THE TRAPS THIS FIXTURE EXISTS TO SEPARATE, and none of them is separated by
 * an ordinary random fill:
 *
 *   TWO DROPPED ARGUMENTS.  `constellationDesign` passes its SIXTH argument
 *   in `setConstellationToNoise`'s fifth slot and never passes its fifth at
 *   all; `process` does the same with its tenth and ninth.  Both members of
 *   each pair are `unsigned char *`, so a fixture that fills them alike
 *   cannot tell the object's reading from the obvious one.  Here they are
 *   distinct buffers with distinct CONTENTS -- different per-phase spans --
 *   and the sweep counts the trials that take the arm.
 *
 *   THE EIGHT-BIT CLAMP IN `process` IS NOT ONE OF THEM, and finding out is
 *   what this paragraph now records.  `kMax` is
 *   `(unsigned char)(42 - (unsigned char)(6 - shaperSR))` in the object and
 *   `36 + shaperSR` in the obvious spelling; the two differ only outside
 *   -36..219, and there the `d > 42` clamp two statements later takes BOTH
 *   readings to the same `word_0` and the same `k`.  The first draft of this
 *   file counted the trials where the readings differed and asserted the
 *   count -- which was true and proved nothing, because the difference never
 *   reaches an output.  The mutation set carries the 32-bit spelling as an
 *   expected survivor with the proof attached; `run_process` carries the
 *   proof in full.
 *
 *   BOTH DIRECTIONS OF `adjustConstellationsToNewK`.  `UP_ROUND_K` is driven
 *   above and below the fractional part of K so that the add pass and the
 *   removal pass are both entered, and the counts are asserted.
 *
 * WHAT IS DELIBERATELY NOT DRIVEN, and why:
 *
 *   `constellationSize[k]` ABOVE 128, which is the other half of
 *   `adjustConstellationsToNewK`'s failure test.  The `u >= 128` half IS
 *   driven -- one trial in seven raises `short_0a` past anything the ramp
 *   reaches -- but the length half cannot be, and that is a property of the
 *   OBJECT and not of this fixture.  At a length of exactly 128 the shift
 *   that follows writes `constellation[k][128]`, which is
 *   `constellation[k + 1][0]`, and `codecConstellation[5][128]`, which is
 *   `constellationSize[0]`; the row's saved first byte is then no longer in
 *   the row, `reconstructInitialConditions` walks past the row looking for it
 *   and decrements the length more times than there are points in it, and the
 *   next round's `for (i = 0; i < n; i++)` over an `unsigned char` index and
 *   an `unsigned int` length of 0xFFFFFFFF never terminates.  That was
 *   reproduced, cored and read off the core before this paragraph was
 *   written; docs/deviations.md D351 and D352 carry it.  Every length in the
 *   sweep therefore starts at 45 or below and the add pass cannot double one
 *   past 90, since the pass stops as soon as the PRODUCT crosses the next
 *   power of two -- which one row doubling guarantees.
 *
 *   A ZERO `constellationSize`.  `getPower` reaches `__moddi3(x, 0)` on one,
 *   which is a divide by zero and a SIGFPE on both sides; a signal is not a
 *   diagnostic.  Every size in the sweep starts in 1..120 and the members
 *   themselves never leave one at zero -- both removal loops put the point
 *   back when a row reaches zero, which is what their "BUG !!!" diagnostics
 *   are about.
 *
 *   A SPAN ABOVE 120 in either `unsigned char *` per-phase array.
 *   `setConstellationToNoise` stages accepted indices into a 128-byte local
 *   and the span is `lastUcode[k] - params->unnamed_360`, so 256 of them
 *   would smash the frame -- ours and the blob's differently.  D333.  BOTH
 *   candidate arrays are kept span-safe, because a mutation that swaps them
 *   has to produce a DIFFERENCE and not a crash.
 *
 *   A `shaperSR` that puts `1LL << (shaperSR + word_0 - 6)` out of range.
 *   It cannot happen through these four: every `getPower` call in them is
 *   preceded by `word_0 = maxK - shaperSR + 6`, which makes the shift count
 *   `maxK` and so at most 42 whatever `shaperSR` is.  D349 is therefore not
 *   reachable from here and the sweep may drive `shaperSR` freely.
 *
 * THE ROWS ARE SEVEN DEEP AND NOT SIX, for `t_v90cdnoise`'s reason and for one
 * more of this batch's own: `adjustConstellationsToNewK` scans `ucode[k][u]`
 * with `u` running to 127 inclusive off a `k` that reaches 5, and computes the
 * companded level at the index the scan STOPPED on, which can be 128.  Six
 * rows would be a read past the end on both sides and the test would be
 * measuring the allocator.  D325's argument, with this member's arithmetic.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
extern "C" {
#include "dsplib/pcm.h"
}
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90ConstellationPower.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90ConstellationDesigner.h"

extern "C" {

/* The blob's own copy of the gate: harness.h says to raise BOTH. */
extern unsigned int ref_dsplibs_debug_level;

void our_acp(void *)
	asm("_ZN24V90ConstellationDesigner25adjustConstellationsPowerEv");
void ref_acp(void *)
	asm("ref__ZN24V90ConstellationDesigner25adjustConstellationsPowerEv");

void our_ank(void *, void *, void *, short *, void *)
	asm("_ZN24V90ConstellationDesigner26adjustConstellationsToNewKEPA128_"
	    "sS1_PsPA128_h");
void ref_ank(void *, void *, void *, short *, void *)
	asm("ref__ZN24V90ConstellationDesigner26adjustConstellationsToNewKEPA128"
	    "_sS1_PsPA128_h");

void our_cdes(void *, float, void *, void *, short *, unsigned char *,
	      unsigned char *, void *)
	asm("_ZN24V90ConstellationDesigner19constellationDesignEfPA128_sS1_PsPh"
	    "S3_PA128_h");
void ref_cdes(void *, float, void *, void *, short *, unsigned char *,
	      unsigned char *, void *)
	asm("ref__ZN24V90ConstellationDesigner19constellationDesignEfPA128_sS1_"
	    "PsPhS3_PA128_h");

int our_proc(void *, unsigned int, void *, float, int, void *, void *, void *,
	     short *, unsigned char *, unsigned char *, unsigned char, int,
	     unsigned int, int)
	asm("_ZN24V90ConstellationDesigner7processEjP25V90AutoDigitalImpDetector"
	    "fiP16V90MappingParamsPA128_sS5_PsPhS7_h23__tHardwareCodecTypes__j28"
	    "V90SpecialSpectralConditions");
int ref_proc(void *, unsigned int, void *, float, int, void *, void *, void *,
	     short *, unsigned char *, unsigned char *, unsigned char, int,
	     unsigned int, int)
	asm("ref__ZN24V90ConstellationDesigner7processEjP25V90AutoDigitalImpDete"
	    "ctorfiP16V90MappingParamsPA128_sS5_PsPhS7_h23__tHardwareCodecTypes_"
	    "_j28V90SpecialSpectralConditions");

}

/*
 * THE SWEEP LENGTH.  Every counter this file asserts on is reached inside the
 * first few dozen trials, and the set of `test/mutations/v90cdadjust.json` is
 * run once per mutation -- so a sweep three times longer than the counters
 * need is a tax on every `make phase` in the tree and on every mutation run,
 * and buys nothing.  Raised only if a new claim needs a case the current
 * length misses.
 */
#define ADJ_TRIALS	61
#define ADJ_LOUD	61

/* The same generator the other two designer tests use. */
static unsigned lfsr;

static void
reseed(unsigned s)
{
	lfsr = s | 1u;
}

static unsigned
nextrand(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	lfsr = lfsr * 1103515245u + 12345u;
	return lfsr;
}

/*
 * ===========================================================================
 * The two sides
 * ===========================================================================
 *
 * File scope throughout: a detector is 43,432 bytes and there are two of them,
 * which is not what a stack frame wants and not what the period compiler's
 * frame limits want to be discovered on.
 */
static V90MappingParams mpA;
static V90MappingParams mpB;
static unsigned char mpSnapA[sizeof(V90MappingParams)];

static unsigned char parAbuf[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char parBbuf[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char cdAbuf[sizeof(V90ConstellationDesigner)]
	__attribute__((aligned(8)));
static unsigned char cdBbuf[sizeof(V90ConstellationDesigner)]
	__attribute__((aligned(8)));
static unsigned char cpAbuf[sizeof(V90ConstellationPower)]
	__attribute__((aligned(8)));
static unsigned char cpBbuf[sizeof(V90ConstellationPower)]
	__attribute__((aligned(8)));
static unsigned char detAbuf[sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));
static unsigned char detBbuf[sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));

static V90Parameters *parA;
static V90Parameters *parB;
static V90ConstellationDesigner *cdA;
static V90ConstellationDesigner *cdB;
static V90ConstellationPower *cpA;
static V90ConstellationPower *cpB;
static V90AutoDigitalImpDetector *detA;
static V90AutoDigitalImpDetector *detB;

/* Seven rows; see the file header. */
static short ucA[7][128];
static short ucB[7][128];
static short alA[7][128];
static short alB[7][128];
static unsigned char okA[7][128];
static unsigned char okB[7][128];
static short dminA[6];
static short dminB[6];
static unsigned char lastA[6];		/* the DROPPED one on the plain arm */
static unsigned char lastB[6];
static unsigned char topA[6];		/* the one that takes its place     */
static unsigned char topB[6];
static unsigned char spareA[7][128];	/* newK's unread fourth argument    */
static unsigned char spareB[7][128];

/*
 * `constelTable` is at +0x14 and `determineDminForRrn` reads +0x280c off it,
 * so the buffer behind it has to reach at least 0x2812 bytes.  For the three
 * members driven directly it is a plain buffer; `process` points it at the
 * detector itself, which is what the object does.
 */
static unsigned char tblA[0x3000] __attribute__((aligned(8)));
static unsigned char tblB[0x3000] __attribute__((aligned(8)));

static void
wire(void)
{
	parA = (V90Parameters *)parAbuf;
	parB = (V90Parameters *)parBbuf;
	cdA = (V90ConstellationDesigner *)cdAbuf;
	cdB = (V90ConstellationDesigner *)cdBbuf;
	cpA = (V90ConstellationPower *)cpAbuf;
	cpB = (V90ConstellationPower *)cpBbuf;
	detA = (V90AutoDigitalImpDetector *)detAbuf;
	detB = (V90AutoDigitalImpDetector *)detBbuf;
}

/*
 * The five pointer fields, normalised.  Two separately allocated collaborators
 * hold two different addresses and always will, so the comparison is made
 * against a copy of ours with the blob's five written into it -- which leaves
 * every other byte of the 84 compared, rather than field by field.
 */
static void
same_designer(long input)
{
	unsigned char tmp[sizeof(V90ConstellationDesigner)];
	V90ConstellationDesigner *t = (V90ConstellationDesigner *)tmp;

	memcpy(tmp, cdAbuf, sizeof(tmp));
	t->params = cdB->params;
	t->mappingParams = cdB->mappingParams;
	t->constelTable = cdB->constelTable;
	t->power = cdB->power;
	t->preFilter = cdB->preFilter;

	diff_eq_obj("designer", V90ConstellationDesigner, tmp, cdBbuf, input);
}

/* The same, for the one pointer `V90ConstellationPower` keeps. */
static void
same_power(long input)
{
	unsigned char tmp[sizeof(V90ConstellationPower)];
	V90ConstellationPower *t = (V90ConstellationPower *)tmp;

	memcpy(tmp, cpAbuf, sizeof(tmp));
	t->constellation = cpB->constellation;

	diff_eq_obj("power", V90ConstellationPower, tmp, cpBbuf, input);
}

static void
same_mapping(long input)
{
	diff_eq_obj("mapping block", V90MappingParams, &mpA, &mpB, input);
}

static void
same_params(long input)
{
	diff_eq_int("the parameter blocks differ on trial %ld",
		    memcmp(parAbuf, parBbuf, sizeof(parAbuf)), 0, input);
}

/*
 * Every input array, both sides against each other and both against the state
 * they were handed.  `topUcode` is the only one any of the four may write, and
 * it is written only through `setConstellationToNoise_forceRate`, so the rest
 * are negative claims and are checked as such.
 */
static unsigned char inSnap[sizeof(ucA) + sizeof(alA) + sizeof(okA)
			    + sizeof(dminA) + sizeof(lastA) + sizeof(topA)
			    + sizeof(spareA)];

static void
snap_inputs(void)
{
	unsigned char *p = inSnap;

	memcpy(p, ucA, sizeof(ucA));		p += sizeof(ucA);
	memcpy(p, alA, sizeof(alA));		p += sizeof(alA);
	memcpy(p, okA, sizeof(okA));		p += sizeof(okA);
	memcpy(p, dminA, sizeof(dminA));	p += sizeof(dminA);
	memcpy(p, lastA, sizeof(lastA));	p += sizeof(lastA);
	memcpy(p, topA, sizeof(topA));		p += sizeof(topA);
	memcpy(p, spareA, sizeof(spareA));
}

static void
same_inputs(long input, int topMayMove)
{
	unsigned char *p = inSnap;

	diff_eq_int("ucode differs between the sides on trial %ld",
		    memcmp(ucA, ucB, sizeof(ucA)), 0, input);
	diff_eq_int("alt differs between the sides on trial %ld",
		    memcmp(alA, alB, sizeof(alA)), 0, input);
	diff_eq_int("the flag table differs between the sides on trial %ld",
		    memcmp(okA, okB, sizeof(okA)), 0, input);
	diff_eq_int("dmin differs between the sides on trial %ld",
		    memcmp(dminA, dminB, sizeof(dminA)), 0, input);
	diff_eq_int("lastUcode differs between the sides on trial %ld",
		    memcmp(lastA, lastB, sizeof(lastA)), 0, input);
	diff_eq_int("topUcode differs between the sides on trial %ld",
		    memcmp(topA, topB, sizeof(topA)), 0, input);
	diff_eq_int("the spare table differs between the sides on trial %ld",
		    memcmp(spareA, spareB, sizeof(spareA)), 0, input);

	diff_eq_int("ucode was written on trial %ld",
		    memcmp(p, ucA, sizeof(ucA)), 0, input);
	p += sizeof(ucA);
	diff_eq_int("alt was written on trial %ld",
		    memcmp(p, alA, sizeof(alA)), 0, input);
	p += sizeof(alA);
	diff_eq_int("the flag table was written on trial %ld",
		    memcmp(p, okA, sizeof(okA)), 0, input);
	p += sizeof(okA);
	diff_eq_int("dmin was written on trial %ld",
		    memcmp(p, dminA, sizeof(dminA)), 0, input);
	p += sizeof(dminA);
	diff_eq_int("lastUcode was written on trial %ld",
		    memcmp(p, lastA, sizeof(lastA)), 0, input);
	p += sizeof(lastA);
	if (!topMayMove)
		diff_eq_int("topUcode was written on trial %ld",
			    memcmp(p, topA, sizeof(topA)), 0, input);
	p += sizeof(topA);
	diff_eq_int("the unread fourth argument was written on trial %ld",
		    memcmp(p, spareA, sizeof(spareA)), 0, input);
}

/*
 * ===========================================================================
 * The fixture
 * ===========================================================================
 *
 * THE 16-BIT TABLES ARE RAMPS AND NOT NOISE, for finding F2164's reason, which
 * `t_v90cdnoise.cpp`'s header states in full: over uniform noise the inner
 * counts are RECORD counts and barely move with the seed, so every claim about
 * a threshold goes untested.  A ramp makes them smooth and tunable.
 *
 * Nothing below is random except the jitter on the tables and the row lengths;
 * every discriminating value is indexed off the trial with a period coprime to
 * the others, so the sweep crosses them rather than walking one at a time.
 */
struct adj_case {
	float noise;
	int shaperSR;
	int forced;			/* FORCE_RATE_ENABLE                */
	int power;			/* ENABLE_DIGITAL_POWER_REDUCTION   */
	int redundancy;			/* ENABLE_REDUNDANCY_OPTIMIZATION   */
	float upRoundK;
	int w2c;
	int w28;
	unsigned char byte08;
	unsigned char byte38;
	unsigned int minRate;
	unsigned int maxRate;
	unsigned int word24;
	int rateMask;
	int cond;
	int start;
};

static struct adj_case cur;

/*
 * PAST BOTH ENDS OF THE EIGHT-BIT AGREEMENT.  The byte reading of `kMax` and
 * the 32-bit one agree for every `shaperSR` in -36..219; the four values
 * outside that range are what separate them, and `run_process` counts how many
 * trials land on one whose byte reading is also small enough for the clamp to
 * bite.  The rest are ordinary shaper values.
 */
static const int srtab[11] = {
	0, 1, 2, 3, -2, 6, 12, 250, 255, 240, -40
};
/*
 * AND A NARROWER ONE FOR THE COMPOSED GROUPS.  `forceRate`'s exponent is
 * `bits + shaperSR - 6` and a value at or below zero puts it in the doubling
 * loop for 2^32 turns (D336), so the composed sweep's floor is -14 against the
 * smallest `bits` a 28000 rate force produces.  The spread matters: `d` is
 * `maxK - shaperSR + 6`, so ten shaper values walk `d` across the range where
 * its 42 clamp and the `42 - d` that seeds `byte_08` have their boundaries.
 */
static const int csrtab[10] = { 0, 1, 2, 3, -2, 6, 12, -8, -14, -11 };
static const float noisetab[9] = {
	0.0f, 1.0f, 4.5f, 7.9f, 12.25f, 40.0f, 0.125f, 2.5f, 19.0f
};
static const float uprtab[7] = {
	-1.0f, 0.0f, 0.05f, 0.25f, 0.5f, 0.9f, 2.0f
};
/*
 * THE RATE WINDOW IS NARROWED ON PURPOSE, and `setConstellationToNoise_
 * forceRate` is why.  That member derives a bit count `n` from
 * `params->RATE_FORCE` -- `(short)(rate * 0.00075 + 0.5f)` -- and walks the
 * product of its six per-phase counts up to 2^n one increment at a time, so
 * `n + 2a + b` outside [12, 45] either never terminates or divides a count to
 * zero and never terminates (D336, D337, and the header of
 * `t_v90cdnoise.cpp`, which solves the same constraint).  `a` is the number
 * of phases with a non-zero `dmin` and `b` the number with a non-zero
 * `halfPhase`.
 *
 * `process` WRITES `RATE_FORCE` ITSELF, to `maxRate` or `minRate`, and then
 * goes round again -- so the fixture cannot simply choose a safe value once.
 * What it can do is bound the window: a forced `maxRate` is only ever
 * reached when the design's rate EXCEEDS it, which 56000 cannot be, so the
 * largest `RATE_FORCE` any pass can see is 40000 and `n` is at most 30.  The
 * fixture then holds `a` at 3 or under and `b` at 2, which keeps `n + 2a + b`
 * inside [21, 38].
 */
static const unsigned int minratetab[3] = { 28000u, 30000u, 33600u };
static const unsigned int maxratetab[3] = { 33600u, 40000u, 56000u };

/*
 * `composed` IS FOR THE TWO GROUPS THAT RUN THE WHOLE CHAIN, and it exists
 * because `setConstellationToNoise` can leave a phase with NO points -- its
 * staging loop accepts nothing when every `ucode[k][i]` in the span is under
 * the threshold -- and `adjustConstellationsPower`'s first act is a `getPower`
 * whose `calcModulusParameters` then reaches `__moddi3(x, 0)`.  That is a
 * SIGFPE on both sides, and a signal is not a diagnostic: the same reason
 * `t_v90cpower.cpp` gives for never driving a zero length directly.
 *
 * So the composed groups use a fixture that cannot produce one: every entry
 * of the allow table set, a span that starts well inside the row, and a ramp
 * steep enough that the first entry of every span clears any threshold the
 * noise energies below can produce.  The three arms the flags select are
 * still all driven; what is excluded is an EMPTY constellation, and
 * docs/deviations.md D356 records the composition rather than the fixture
 * hiding it.
 */
static void
adj_fixture(int trial, int composed)
{
	unsigned ramp = composed ? (12u + (unsigned)(trial % 11))
				 : (1u + (unsigned)(trial % 23));
	int k;
	int j;

	reseed(0x5ac3u + 149u * (unsigned)trial);

	cur.noise = composed ? noisetab[trial % 4] : noisetab[trial % 9];
	cur.shaperSR = composed ? csrtab[trial % 10] : srtab[trial % 11];
	cur.forced = (trial % 5) < 2;
	cur.power = (trial % 3) != 0;
	cur.redundancy = (trial % 7) != 0;
	cur.upRoundK = uprtab[trial % 7];
	cur.w2c = (trial % 4) < 2;
	cur.w28 = (trial % 4) & 1;
	/*
	 * ODD AND EVEN BOTH, so that the `byte_08 > 13` threshold has its own
	 * boundary value in the sweep and not only values either side of it.
	 */
	cur.byte08 = (unsigned char)(((trial % 13) * 2u)
				     + (unsigned)((trial / 13) % 2));
	cur.byte38 = (unsigned char)(trial % 29);
	cur.minRate = minratetab[trial % 3];
	cur.maxRate = maxratetab[(trial + 1) % 3];
	/*
	 * AND ONE TRIAL IN SEVEN HAS THE WINDOW THE WRONG WAY ROUND, which is
	 * the only thing that reaches `process`' `minRate > maxRate` clamp.
	 */
	if (trial % 7 == 5) {
		cur.minRate = 40000u;
		cur.maxRate = 33600u;
	}
	cur.word24 = ((trial % 9) < 4) ? 0u : (28000u + 100u * (unsigned)trial);
	cur.rateMask = (int)(0x00fffff0u ^ (unsigned)(trial * 2654435761u));
	cur.cond = (trial % 6 == 0) ? 2 : (trial % 3);
	/*
	 * AND THE BIT BUDGET INCLUDES `shaperSR`.  `forceRate` forms its
	 * exponent as `(short)(RATE_FORCE * 0.00075 + 0.5f) + shaperSR - 6`,
	 * so one of the four out-of-band shaper values the sweep needs for the
	 * eight-bit `kMax` reading (finding F3403) makes 2^n an infinity and the
	 * refinement loop never terminates -- D337 again, reached through a
	 * term the first draft of this fixture did not know was in it, and
	 * cored to find out.
	 *
	 * The two are kept apart rather than traded off: a trial whose
	 * `shaperSR` is out of band never forces the rate, and never can, since
	 * it is given the WIDEST window and a `d` clamped at 42 puts its rate
	 * at exactly 56000 -- inside it at both ends, so `process` settles on
	 * the first pass and leaves `FORCE_RATE_ENABLE` alone.  Every other
	 * trial keeps `n + 2a + b` inside [21, 39].
	 */
	if (composed && (cur.shaperSR > 12 || cur.shaperSR < -14)) {
		cur.forced = 0;
		cur.minRate = 28000u;
		cur.maxRate = 56000u;
	}

	cur.start = composed ? 4 + (trial % 8)
		     : (trial % 3 == 0) ? 0
		     : (trial % 3 == 1) ? (int)(nextrand() % 24u)
					: 100 + (int)(nextrand() % 30u);

	for (k = 0; k < 7; k++)
		for (j = 0; j < 128; j++) {
			short u = (short)((unsigned)j * ramp
					  + nextrand() % (2u * ramp))
				- (short)ramp;
			short a = (short)(u + (int)(nextrand() % 64u) - 32);
			unsigned char ok = (unsigned char)
			    (composed ? 1u : ((nextrand() % 8u) != 0));
			unsigned char sp = (unsigned char)(nextrand() >> 11);

			ucA[k][j] = u;
			ucB[k][j] = u;
			alA[k][j] = a;
			alB[k][j] = a;
			okA[k][j] = ok;
			okB[k][j] = ok;
			spareA[k][j] = sp;
			spareB[k][j] = sp;
		}

	for (k = 0; k < 6; k++) {
		/*
		 * THE SPANS ARE CAPPED AT 110 AND THAT IS THE FRAME GUARD (see
		 * the file header and D333), and the TWO ARRAYS ARE GIVEN
		 * DIFFERENT ONES, which is what makes the dropped argument
		 * visible: swap them and the staging loop runs a different
		 * number of times.
		 */
		int spanLast = composed ? (12 + ((trial + k) % 20))
					: 1 + (int)(nextrand() % 45u);
		int spanTop = composed ? (20 + ((trial + 3 * k) % 20))
				       : 1 + (int)(nextrand() % 45u);

		if (cur.start + spanLast - 1 > 255)
			spanLast = 256 - cur.start;
		if (cur.start + spanTop - 1 > 255)
			spanTop = 256 - cur.start;
		if (spanLast < 1)
			spanLast = 1;
		if (spanTop < 1)
			spanTop = 1;
		/* Keep them apart, so an equal pair never hides the swap. */
		if (spanTop == spanLast)
			spanTop = (spanTop > 1) ? spanTop - 1 : spanTop + 1;

		/*
		 * `b` IS HELD AT TWO: `lastUcode` is `forceRate`'s `halfPhase`
		 * flag array, and the count of non-zero entries in it is one
		 * of the three terms the bit budget above is over.  Holding it
		 * at two also keeps the two candidate arrays apart on four
		 * phases out of six, which is the other thing this array is
		 * for.
		 */
		lastA[k] = (unsigned char)((k < 2)
					   ? cur.start + spanLast - 1 : 0);
		lastB[k] = lastA[k];
		/*
		 * NEVER ZERO: `forceRate` walks backwards from `topUcode[k] -
		 * 1` and compares it UNSIGNED against `unnamed_360`, so a top
		 * of zero reads off the front of the row.  D340.
		 */
		topA[k] = (unsigned char)(cur.start + spanTop);
		topB[k] = topA[k];

		dminA[k] = 0;
		dminB[k] = 0;
	}
	/*
	 * `a` IS HELD AT THREE, for the bit budget above, and both of newK's
	 * inner scans run in every trial: the three phases the trial's own
	 * rotation picks carry a non-zero dmin and the other three carry zero.
	 */
	for (k = 0; k < 3; k++) {
		int phase = (trial + 2 * k) % 6;

		dminA[phase] = (short)(1 + ((trial + k) % 7));
		dminB[phase] = dminA[phase];
	}
}

/*
 * Plant one trial's state on both sides.  The mapping block is filled with
 * varied bytes and then the two things that decide whether the call is defined
 * at all are pinned: every constellation size into 1..120, and the shaper.
 */
static void
adj_state(int trial, int useDetector, int bigDmin)
{
	unsigned int i;
	unsigned char *a = (unsigned char *)&mpA;
	unsigned char *b = (unsigned char *)&mpB;

	reseed(0x71b3u + 97u * (unsigned)trial);
	for (i = 0; i < sizeof(mpA); i++) {
		unsigned char v = (unsigned char)(nextrand() >> 13);

		a[i] = v;
		b[i] = v;
	}

	for (i = 0; i < V90_CONSTELLATIONS; i++) {
		unsigned int n;

		if (trial % 3 == 0)
			n = 2u + (nextrand() % 6u);
		else if (trial % 3 == 1)
			n = 1u + (nextrand() % 30u);
		else
			n = 25u + (nextrand() % 21u);
		mpA.constellationSize[i] = n;
		mpB.constellationSize[i] = n;
	}
	mpA.shaperSR = cur.shaperSR;
	mpB.shaperSR = cur.shaperSR;
	mpA.word_0 = 21u + (unsigned)(trial % 22);
	mpB.word_0 = mpA.word_0;

	memset(parAbuf, 0, sizeof(parAbuf));
	memset(parBbuf, 0, sizeof(parBbuf));
	parA->unnamed_360 = cur.start;
	parA->USE_RESTRICED_DMIN = (trial % 11) < 4;
	parA->FORCED_DMIN = (trial % 7) - 2;
	parA->FORCE_RATE_ENABLE = cur.forced;
	parA->ENABLE_DIGITAL_POWER_REDUCTION = cur.power;
	parA->ENABLE_REDUNDANCY_OPTIMIZATION = cur.redundancy;
	parA->UP_ROUND_K = cur.upRoundK;
	parA->RATE_FORCE = (int)cur.minRate;
	parA->ENABLE_RRN_UP = 0;
	parA->ENABLE_RRN_DOWN = 0;
	parA->SPECTRAL_SHAPER_ID = 30 + (trial % 15);
	parA->SPECTRAL_SHAPER_SR = cur.shaperSR;
	parA->SPECTRAL_SHAPER_A1 = 0.25f;
	parA->SPECTRAL_SHAPER_A2 = -0.5f;
	parA->SPECTRAL_SHAPER_B1 = 1.5f;
	parA->SPECTRAL_SHAPER_B2 = 0.75f;
	parA->GERMAN_PBX_SPECTRAL_SHAPER_ID = 25 + (trial % 13);
	parA->GERMAN_PBX_SPECTRAL_SHAPER_SR = cur.shaperSR;
	parA->GERMAN_PBX_SPECTRAL_SHAPER_A1 = -0.125f;
	parA->GERMAN_PBX_SPECTRAL_SHAPER_A2 = 0.625f;
	parA->GERMAN_PBX_SPECTRAL_SHAPER_B1 = -1.25f;
	parA->GERMAN_PBX_SPECTRAL_SHAPER_B2 = 2.5f;
	memcpy(parBbuf, parAbuf, sizeof(parAbuf));

	memset(cdAbuf, 0, sizeof(cdAbuf));
	memset(cdBbuf, 0, sizeof(cdBbuf));
	memset(cpAbuf, 0, sizeof(cpAbuf));
	memset(cpBbuf, 0, sizeof(cpBbuf));

	cdA->params = parA;
	cdB->params = parB;
	cdA->mappingParams = &mpA;
	cdB->mappingParams = &mpB;
	cdA->power = cpA;
	cdB->power = cpB;
	cdA->byte_08 = cur.byte08;
	cdB->byte_08 = cur.byte08;
	cdA->byte_38 = cur.byte38;
	cdB->byte_38 = cur.byte38;
	cdA->word_28 = cur.w28;
	cdB->word_28 = cur.w28;
	cdA->word_2c = cur.w2c;
	cdB->word_2c = cur.w2c;
	cdA->word_24 = cur.word24;
	cdB->word_24 = cur.word24;
	cdA->minRate = cur.minRate;
	cdB->minRate = cur.minRate;
	cdA->maxRate = cur.maxRate;
	cdB->maxRate = cur.maxRate;
	/*
	 * A LARGE `short_0a` ON ONE TRIAL IN SEVEN, and that is what drives the
	 * `u >= 128` half of `adjustConstellationsToNewK`'s failure test: no
	 * entry of a ramped `ucode` row clears a threshold 12,000 above the one
	 * the scan started from, so the scan runs off the end of the row and
	 * the member takes its "reached Max constellation length" exit.
	 */
	/*
	 * A LARGE `short_0a` ON ONE TRIAL IN SEVEN, and only where the caller
	 * asks for it: it drives the `u >= 128` half of
	 * `adjustConstellationsToNewK`'s failure test, because no entry of a
	 * ramped `ucode` row clears a threshold 12,000 above the one the scan
	 * started from.  It is NOT set for the two groups that reach
	 * `setConstellationToNoise_forceRate`, whose extend arm needs a ramp
	 * slope large against the dMin it derives or it too fails to
	 * terminate (D338).
	 */
	cdA->short_0a = (short)((bigDmin && trial % 7 == 3)
				? 12000 : (3 + (trial % 40)));
	cdB->short_0a = cdA->short_0a;
	cdA->short_10 = (short)(2 + (trial % 27));
	cdB->short_10 = cdA->short_10;

	if (useDetector) {
		memset(detAbuf, 0x33, sizeof(detAbuf));
		memset(detBbuf, 0x33, sizeof(detBbuf));
		detA->pcmType = (PcmType)cur.w28;
		detB->pcmType = (PcmType)cur.w28;
		detA->int_a960 = cur.w2c;
		detB->int_a960 = cur.w2c;
		for (i = 0; i < V90ADID_PHASES; i++)
			detA->byte_280c[i] = detB->byte_280c[i] =
			    (unsigned char)((trial + i) % 3);
	} else {
		memset(tblA, 0x5a, sizeof(tblA));
		memset(tblB, 0x5a, sizeof(tblB));
		for (i = 0; i < V90ADID_PHASES; i++)
			tblA[0x280c + i] = tblB[0x280c + i] =
			    (unsigned char)((trial + i) % 3);
		cdA->constelTable = (short (*)[128])tblA;
		cdB->constelTable = (short (*)[128])tblB;
	}

	snap_inputs();
	memcpy(mpSnapA, &mpA, sizeof(mpSnapA));
}

/*
 * ===========================================================================
 * adjustConstellationsPower
 * ===========================================================================
 */
static int
run_acp(void)
{
	int trial;
	long ranLoop = 0, restored = 0, hitFloor = 0;

	diff_begin("V90ConstellationDesigner::adjustConstellationsPower");

	for (trial = 0; trial < ADJ_TRIALS; trial++) {
		unsigned int before;

		adj_fixture(trial, 0);
		adj_state(trial, 0, 1);
		before = mpA.word_0;

		our_acp(cdA);
		ref_acp(cdB);

		same_designer(trial);
		same_power(trial);
		same_mapping(trial);
		same_params(trial);
		same_inputs(trial, 0);

		if (memcmp(mpSnapA, &mpA, sizeof(mpSnapA)) != 0)
			ranLoop++;
		if (mpA.word_0 <= 20)
			hitFloor++;
		if (mpA.word_0 != before)
			restored++;
	}

	diff_eq_int("trials that changed the mapping block: %ld", ranLoop > 0,
		    1, ranLoop);
	diff_eq_int("trials whose d moved: %ld", restored > 0, 1, restored);
	diff_eq_int("trials landing at or under the d floor: %ld",
		    hitFloor > 0, 1, hitFloor);
	/*
	 * THE THREE ROUND SIZES ARE NOT COUNTED, and that is deliberate rather
	 * than an omission.  Which of 1, 10 and 20 a round takes is decided by
	 * `target - index` at the head of that round, and `index` is
	 * recomputed inside the loop from a `getPower` this test cannot see
	 * between iterations -- so any counter written here would be
	 * classifying the inputs and calling it the arms.  The mutation set is
	 * what carries the claim: it swaps the 1 and the 10, and moves the
	 * `byte_08` threshold.  `t_v90cdnoise.cpp` makes the same call for the
	 * same reason (finding F2186).
	 */

	return diff_end();
}

/*
 * ===========================================================================
 * adjustConstellationsToNewK
 * ===========================================================================
 */
static int
run_ank(void)
{
	int trial;
	long grew = 0, shrank = 0;
	long zeroDmin = 0, nonZeroDmin = 0;

	diff_begin("V90ConstellationDesigner::adjustConstellationsToNewK");

	for (trial = 0; trial < ADJ_TRIALS; trial++) {
		unsigned int sumBefore = 0;
		unsigned int sumAfter = 0;
		unsigned int i;

		adj_fixture(trial, 0);
		adj_state(trial, 0, 1);

		for (i = 0; i < V90_CONSTELLATIONS; i++) {
			sumBefore += mpA.constellationSize[i];
			if (dminA[i] == 0)
				zeroDmin++;
			else
				nonZeroDmin++;
		}

		our_ank(cdA, ucA, alA, dminA, spareA);
		ref_ank(cdB, ucB, alB, dminB, spareB);

		same_designer(trial);
		same_power(trial);
		same_mapping(trial);
		same_params(trial);
		same_inputs(trial, 0);

		for (i = 0; i < V90_CONSTELLATIONS; i++)
			sumAfter += mpA.constellationSize[i];

		if (sumAfter > sumBefore)
			grew++;
		else if (sumAfter < sumBefore)
			shrank++;
	}

	diff_eq_int("trials that ADDED points: %ld", grew > 0, 1, grew);
	diff_eq_int("trials that REMOVED points: %ld", shrank > 0, 1, shrank);
	diff_eq_int("rows with a zero dmin: %ld", zeroDmin > 0, 1, zeroDmin);
	diff_eq_int("rows with a non-zero dmin: %ld", nonZeroDmin > 0, 1,
		    nonZeroDmin);

	return diff_end();
}

/*
 * ===========================================================================
 * constellationDesign
 * ===========================================================================
 */
static int
run_cdes(void)
{
	int trial;
	long forced = 0, plain = 0, distinct = 0;

	diff_begin("V90ConstellationDesigner::constellationDesign");

	for (trial = 0; trial < ADJ_TRIALS; trial++) {
		int i;

		adj_fixture(trial, 1);
		adj_state(trial, 0, 0);

		/*
		 * The two candidate arrays differ on at least one phase, which
		 * is what makes the dropped-argument reading observable.  It is
		 * counted rather than assumed.
		 */
		for (i = 0; i < 6; i++)
			if (lastA[i] != topA[i]) {
				distinct++;
				break;
			}

		our_cdes(cdA, cur.noise, ucA, alA, dminA, lastA, topA, okA);
		ref_cdes(cdB, cur.noise, ucB, alB, dminB, lastB, topB, okB);

		same_designer(trial);
		same_power(trial);
		same_mapping(trial);
		same_params(trial);
		same_inputs(trial, cur.forced);

		if (cur.forced)
			forced++;
		else
			plain++;
	}

	diff_eq_int("FORCE_RATE_ENABLE trials: %ld", forced > 0, 1, forced);
	diff_eq_int("plain trials -- the arm that DROPS an argument: %ld",
		    plain > 0, 1, plain);
	diff_eq_int("trials whose two candidate arrays differ: %ld",
		    distinct > 0, 1, distinct);

	return diff_end();
}

/*
 * ===========================================================================
 * process
 * ===========================================================================
 */
static int
run_process(void)
{
	int trial;
	long forced = 0, plain = 0;
	long german = 0, ordinary = 0;
	long failedOne = 0, failedZero = 0;
	long tooLarge = 0, tooSmall = 0, settled = 0;
	long rrnUp = 0, rrnDown = 0;

	diff_begin("V90ConstellationDesigner::process");

	for (trial = 0; trial < ADJ_TRIALS; trial++) {
		int a;
		int b;
		int sr = cur.shaperSR;
		unsigned int byteKMax = (unsigned int)(unsigned char)
					(42 - (unsigned char)(6 - sr));
		unsigned int intKMax = (unsigned int)(36 + sr);

		adj_fixture(trial, 1);
		adj_state(trial, 1, 0);

		a = our_proc(cdA, 40u + (unsigned)(trial % 20), detA, cur.noise,
			     cur.rateMask, &mpA, ucA, alA, dminA, lastA, topA,
			     cur.byte38, trial % 16,
			     0x1000u + (unsigned)trial, cur.cond);
		b = ref_proc(cdB, 40u + (unsigned)(trial % 20), detB, cur.noise,
			     cur.rateMask, &mpB, ucB, alB, dminB, lastB, topB,
			     cur.byte38, trial % 16,
			     0x1000u + (unsigned)trial, cur.cond);

		diff_eq_int("process returned differently on trial %ld", a, b,
			    trial);

		same_designer(trial);
		same_power(trial);
		same_mapping(trial);
		same_params(trial);
		/*
		 * `process` CAN TURN FORCING ON ITSELF, so "the plain arm does
		 * not write topUcode" is only a claim about a trial that
		 * neither started forced nor became forced -- and the flag it
		 * leaves behind is what says which.
		 */
		same_inputs(trial, cur.forced || parA->FORCE_RATE_ENABLE);

		diff_eq_int("the detectors differ on trial %ld",
			    memcmp(detAbuf, detBbuf, sizeof(detAbuf)), 0, trial);

		/*
		 * THE SEVEN SLOTS `process` PLANTS, asserted against the
		 * arguments rather than only against the other side.  Two of
		 * them need it: `same_designer` NORMALISES `constelTable` --
		 * the two sides hold two different detectors -- so a wrong
		 * assignment there would compare equal, and `word_61c` is in
		 * the mapping block and would pass on both sides being wrong
		 * together only if the value were dropped.
		 */
		diff_eq_int("constelTable is the detector (trial %ld)",
			    (void *)cdA->constelTable == (void *)detAbuf, 1,
			    trial);
		diff_eq_int("mappingParams is argument 5 (trial %ld)",
			    cdA->mappingParams == &mpA, 1, trial);
		diff_eq_int("codecType is argument 12 (trial %ld)",
			    (int)cdA->codecType, trial % 16, trial);
		diff_eq_int("word_40 is argument 13 (trial %ld)",
			    (long)cdA->word_40, (long)(0x1000u + (unsigned)trial),
			    trial);
		diff_eq_int("byte_38 is argument 11 (trial %ld)",
			    cdA->byte_38, cur.byte38, trial);
		diff_eq_int("word_28 is the detector's pcmType (trial %ld)",
			    cdA->word_28, cur.w28, trial);
		diff_eq_int("word_2c is the detector's int_a960 (trial %ld)",
			    cdA->word_2c, cur.w2c, trial);
		diff_eq_int("the mapping block's +0x61c is 1 (trial %ld)",
			    (long)mpA.word_61c, 1, trial);

		if (cur.forced)
			forced++;
		else
			plain++;
		if (cur.cond == 2)
			german++;
		else
			ordinary++;
		if (a != 0)
			failedOne++;
		else
			failedZero++;
		if (parA->RATE_FORCE == (int)cdA->maxRate
		    && parA->FORCE_RATE_ENABLE)
			tooLarge++;
		else if (parA->RATE_FORCE == (int)cdA->minRate
			 && parA->FORCE_RATE_ENABLE)
			tooSmall++;
		else
			settled++;
		if (parA->ENABLE_RRN_UP)
			rrnUp++;
		if (parA->ENABLE_RRN_DOWN)
			rrnDown++;

		/*
		 * THE EIGHT-BIT `kMax` IS NOT OBSERVABLE, and the count that
		 * used to stand here was wrong to claim it was.  The two
		 * readings differ only when `36 + shaperSR` leaves 0..255,
		 * which is `shaperSR` above 219 or below -36 -- and for either
		 * of those the NEXT statement erases the difference:
		 *
		 *   d = k - shaperSR + 6, unsigned, and `d > 42` clamps.
		 *   shaperSR > 219 needs k >= 213 for d to stay under 43, and
		 *   k is at most maxK, which is at most 42 because six
		 *   constellations of at most 128 points have a product of at
		 *   most 2^42.  shaperSR < -36 makes d at least k + 43.
		 *
		 * So both readings take the clamp, both set `word_0` to 42 and
		 * both set `k` to `shaperSR + 36`, and no state and no
		 * diagnostic can tell them apart.  The mutation set carries
		 * the 32-bit spelling as an EXPECTED SURVIVOR with that proof
		 * rather than as a claim this test can settle, and the source
		 * comment says the same.  Finding F3403.
		 */
		(void)sr;
		(void)byteKMax;
		(void)intKMax;
	}

	diff_eq_int("FORCE_RATE_ENABLE trials: %ld", forced > 0, 1, forced);
	diff_eq_int("plain trials -- the arm that DROPS an argument: %ld",
		    plain > 0, 1, plain);
	diff_eq_int("GERMAN_PBX shaper trials: %ld", german > 0, 1, german);
	diff_eq_int("ordinary shaper trials: %ld", ordinary > 0, 1, ordinary);
	diff_eq_int("trials returning non-zero: %ld", failedOne > 0, 1,
		    failedOne);
	diff_eq_int("trials returning zero: %ld", failedZero > 0, 1,
		    failedZero);
	diff_eq_int("rate forced to maxRate: %ld", tooLarge > 0, 1, tooLarge);
	diff_eq_int("rate forced to minRate: %ld", tooSmall > 0, 1, tooSmall);
	diff_eq_int("rate inside the window: %ld", settled > 0, 1, settled);
	diff_eq_int("ENABLE_RRN_UP set: %ld", rrnUp > 0, 1, rrnUp);
	diff_eq_int("ENABLE_RRN_DOWN set: %ld", rrnDown > 0, 1, rrnDown);

	return diff_end();
}

/*
 * ===========================================================================
 * All four, loud
 * ===========================================================================
 *
 * TWENTY-THREE OF THE THIRTY-ONE DIAGNOSTICS IN THIS BATCH ARE `edprintf`,
 * and none of their arguments is visible in any object the four groups above
 * compare: an `edprintf` goes to `dsplibs_debug_printf` and nowhere else.  So
 * every one of the `%c%d.%03d` computations -- the `fsqrt`, the sign taken
 * from the POWER where the magnitude is taken from its ROOT, the
 * `(index + 1) * -0.5f` dBm0, the 1000.0f, 10.0f and 100000.0f scales, and
 * the block that evaluates `realK` four separate times -- would be
 * reconstructed with nothing checking it.
 *
 * This group raises BOTH sides' debug level (raising one alone makes the two
 * take different branches for reasons that are not the modem -- harness.h says
 * so) and compares the transcripts.  The text is ENCODED and is not parsed
 * here; what is being asked is whether the two sides say the same thing, which
 * is what catches a wrong argument.  `dsplib_debug_capture_lines(1) > 0` is
 * asserted so that a dead capture cannot read as agreement, and the mutation
 * set carries a scale change that ONLY this group can catch.
 *
 * ONE DIAGNOSTIC IN THE BATCH STAYS DARK and it is named rather than left as a
 * number in `make phase`'s rollup: `adjustConstellationsToNewK`'s "BUG !!!
 * reached constellation length 0!!!", the removal pass's guard against
 * emptying a row.  Reaching it needs a target K far enough under the design's
 * own that the pass takes a whole constellation away, and the fixture is
 * bounded off that -- an empty row is what D356's SIGFPE is, one `getPower`
 * later.  The sibling guard in `adjustConstellationsPower` IS reached, and its
 * text differs only in the class name the author misspelled.
 */
static int
run_loud(void)
{
	int trial;
	long printed = 0;

	diff_begin("the four members' diagnostics, both sides talking");

	for (trial = 0; trial < ADJ_LOUD; trial++) {
		int a;
		int b;

		adj_fixture(trial, 0);
		adj_state(trial, 0, 1);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		our_acp(cdA);
		ref_acp(cdB);
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("adjustConstellationsPower transcripts (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0 ? 1 : 0,
			    1, trial);
		if (dsplib_debug_capture_lines(1) > 0)
			printed++;
		same_designer(trial);
		same_power(trial);
		same_mapping(trial);

		adj_fixture(trial, 0);
		adj_state(trial, 0, 1);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		our_ank(cdA, ucA, alA, dminA, spareA);
		ref_ank(cdB, ucB, alB, dminB, spareB);
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("adjustConstellationsToNewK transcripts (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0 ? 1 : 0,
			    1, trial);
		if (dsplib_debug_capture_lines(1) > 0)
			printed++;
		same_designer(trial);
		same_power(trial);
		same_mapping(trial);

		adj_fixture(trial, 1);
		adj_state(trial, 0, 0);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		our_cdes(cdA, cur.noise, ucA, alA, dminA, lastA, topA, okA);
		ref_cdes(cdB, cur.noise, ucB, alB, dminB, lastB, topB, okB);
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("constellationDesign transcripts (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0 ? 1 : 0,
			    1, trial);
		if (dsplib_debug_capture_lines(1) > 0)
			printed++;
		same_designer(trial);
		same_mapping(trial);

		adj_fixture(trial, 1);
		adj_state(trial, 1, 0);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		a = our_proc(cdA, 40u + (unsigned)(trial % 20), detA, cur.noise,
			     cur.rateMask, &mpA, ucA, alA, dminA, lastA, topA,
			     cur.byte38, trial % 16,
			     0x1000u + (unsigned)trial, cur.cond);
		b = ref_proc(cdB, 40u + (unsigned)(trial % 20), detB, cur.noise,
			     cur.rateMask, &mpB, ucB, alB, dminB, lastB, topB,
			     cur.byte38, trial % 16,
			     0x1000u + (unsigned)trial, cur.cond);
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("process transcripts (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0 ? 1 : 0,
			    1, trial);
		diff_eq_int("process returned differently, loud (trial %ld)",
			    a, b, trial);
		if (dsplib_debug_capture_lines(1) > 0)
			printed++;
		same_designer(trial);
		same_mapping(trial);
		same_params(trial);
	}

	diff_eq_int("calls that printed anything: %ld", printed > 0, 1,
		    printed);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	wire();

	rc |= run_acp();
	rc |= run_ank();
	rc |= run_cdes();
	rc |= run_process();
	rc |= run_loud();

	return rc;
}
