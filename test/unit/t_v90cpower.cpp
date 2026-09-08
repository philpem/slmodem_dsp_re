/*
 * t_v90cpower.cpp -- differential test of V90ConstellationPower's five
 * members and its static power ladder, each against the blob's own copy.
 *
 * NOTHING IN THE OBJECT CALLS ANY OF THEM except `getPower`, which calls the
 * other two mutators itself, so there is no call site to drive them through
 * and none to take argument types from.  Each is called directly by symbol on
 * both sides -- ours by its mangled name, the blob's by the `ref_` alias --
 * through an `asm()` label, which is also how the ctor/dtor test reaches a
 * member with no spellable C++ form and how finding F225's double mangling is
 * sidestepped.  The convention is plain cdecl with `this` as the first stack
 * argument (finding F215).
 *
 * ONE `V90MappingParams`, SHARED.  Every other V.90 test here gives each side
 * its own copy and compares the two afterwards.  That cannot work for this
 * class: `getConstellationInfo` stores a pointer INTO the mapping block at
 * `this + 0`, so two separately allocated blocks would put two different
 * addresses in the two objects and `diff_eq_obj` would fail at offset 0
 * whatever the code did.  The two sides therefore share one block, and the
 * negative claim -- that none of the five writes to it -- is asserted the
 * other way round instead: the block is snapshotted before each call and
 * compared against the snapshot after each side has run.
 *
 * WHAT A RANDOM FILL WOULD NOT REACH, and what is forced here instead:
 *
 *   THE SIXTH MODULUS.  `calcModulusParameters` computes five of its six
 *   digits as `remaining[i] % constellationSize[i]` and the sixth as a plain
 *   truncation of `remaining[5]` to 32 bits (finding F3052).  The two readings
 *   AGREE for every `remaining[5]` below `constellationSize[5]`, which is what
 *   an ordinary rate and an ordinary set of constellation sizes produce -- so
 *   a sweep that only drives realistic values passes with the modulo that
 *   everyone would write in place of the truncation the object has.  The
 *   sweep below counts the trials where `remaining[5] >= constellationSize[5]`
 *   and asserts the count is not zero.
 *
 *   ALL THREE ARMS of `getPower`'s inner test -- `modulus[i]` above, equal to
 *   and below the point index -- are counted over the whole sweep and each is
 *   asserted non-zero.  The equal arm is the one with a different expression
 *   in it, and it is reached for exactly one `j` per constellation.
 *
 *   BOTH COMPANDING LAWS and a `point` of 0, 1 AND 2.  The object's point test
 *   is an equality against 1 rather than a `!= 0`, so a sweep over {0, 1}
 *   alone cannot tell the two readings apart; 2 is what separates them, and
 *   the enum is pinned wide in the header so that passing it is meaningful.
 *
 *   THE LADDER'S ENDS AND ITS EXACT BOUNDARY.  Random floats never land on a
 *   limit, so `getPowerIndexForPower` is driven at every entry's exact value,
 *   just above and just below it, past both ends, and at both infinities.
 *   Every entry below 8388608 is exactly representable as a float and the
 *   larger ones are not, so the exact-value cases are computed through
 *   `(float)` and the assertion is on agreement between the two sides rather
 *   than on a predicted index.
 *
 *   THE UNSIGNED BOUND of `isLegalPowerIndex`: 0xFFFFFFFF is what proves the
 *   comparison is `setbe` and not a signed one.
 *
 * WHAT IS DELIBERATELY NOT DRIVEN:
 *
 *   `constellationSize[i] == 0`.  `__moddi3(x, 0)` is a divide by zero and
 *   raises SIGFPE on both sides; a signal is not a diagnostic.  Every size in
 *   the sweep is in 1..128, which is also the domain `V90MappingParams.h`
 *   measures for the field -- 0x80 bytes per row and six rows.
 *
 *   A shift count outside 0..63.  `1LL << n` is undefined there and the two
 *   compilers are free to differ for reasons that are not the source's.  The
 *   object has no guard; docs/deviations.md D349.
 *
 *   A NaN power.  `getPowerIndexForPower`'s compare is a bare ordered `fcom`,
 *   so a NaN would be comparing the two builds' float-compare flags rather
 *   than the reconstruction; docs/deviations.md D350.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "v90table1.h"

#include "dsplib/V90MappingParams.h"
#include "dsplib/V90ConstellationPower.h"

extern "C" {

void our_calcmod(void *, void *)
	asm("_ZN21V90ConstellationPower21calcModulusParametersEP16V90MappingParams");
void ref_calcmod(void *, void *)
	asm("ref__ZN21V90ConstellationPower21calcModulusParametersEP16V90MappingParams");

void our_getinfo(void *, void *, int, unsigned int)
	asm("_ZN21V90ConstellationPower20getConstellationInfoEP16V90MappingParams26V90TxPowerMeasurementPointj");
void ref_getinfo(void *, void *, int, unsigned int)
	asm("ref__ZN21V90ConstellationPower20getConstellationInfoEP16V90MappingParams26V90TxPowerMeasurementPointj");

float our_getpower(void *, void *, int, int)
	asm("_ZN21V90ConstellationPower8getPowerEP16V90MappingParams26V90TxPowerMeasurementPoint7PcmType");
float ref_getpower(void *, void *, int, int)
	asm("ref__ZN21V90ConstellationPower8getPowerEP16V90MappingParams26V90TxPowerMeasurementPoint7PcmType");

unsigned int our_pindex(void *, float)
	asm("_ZN21V90ConstellationPower21getPowerIndexForPowerEf");
unsigned int ref_pindex(void *, float)
	asm("ref__ZN21V90ConstellationPower21getPowerIndexForPowerEf");

/*
 * A `bool` return lives in %al with the rest of %eax unspecified, and the
 * return type is not mangled, so the blob's own choice is not knowable.
 * Reading one byte is right under either reading: the object's body is
 * `xor %eax,%eax ; setbe %al`, so the low byte carries the answer whether the
 * original wrote `bool` or `int`.
 */
unsigned char our_islegal(void *, unsigned int)
	asm("_ZN21V90ConstellationPower17isLegalPowerIndexEj");
unsigned char ref_islegal(void *, unsigned int)
	asm("ref__ZN21V90ConstellationPower17isLegalPowerIndexEj");

extern unsigned int ref_averagePowerLimits[V90CP_POWER_INDICES]
	asm("ref__ZN21V90ConstellationPower18averagePowerLimitsE");

}

typedef float (*getpower_fn)(void *, void *, int, int);

/*
 * The same cheap varied fill the other V.90 tests use, so a value that happens
 * to be a constant the code stores is not mistaken for a store.
 */
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
 * File scope: 0x650 plus two 0x90 objects plus a snapshot is more than a stack
 * frame wants, and the period compiler's frame limits are not worth
 * discovering here.
 */
static V90MappingParams mp;
static unsigned char mpSnap[sizeof(V90MappingParams)];

static unsigned char cpAbuf[sizeof(V90ConstellationPower)]
	__attribute__((aligned(8)));
static unsigned char cpBbuf[sizeof(V90ConstellationPower)]
	__attribute__((aligned(8)));
static V90ConstellationPower *cpA;
static V90ConstellationPower *cpB;

/* Trial 0's object fill, kept so that "the seed really varied" has something
 * to compare against. */
static unsigned char fill0[sizeof(V90ConstellationPower)];

static void
fillbytes(void *p, size_t n)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(nextrand() >> 13);
}

/*
 * Fill the mapping block, then pin the two things that decide whether the
 * call is defined at all: every constellation size into 1..128 and the shift
 * count into 0..63.
 */
static void
setup_mp(unsigned trial, unsigned bits)
{
	unsigned int i;

	fillbytes(&mp, sizeof(mp));

	for (i = 0; i < V90_CONSTELLATIONS; i++) {
		unsigned int s;

		/*
		 * Small sizes early in the sweep, because a small product of
		 * the first five is what leaves `remaining[5]` large enough to
		 * separate the truncation from a modulo.
		 */
		if (trial % 3 == 0)
			s = 2u + (nextrand() % 3u);
		else if (trial % 3 == 1)
			s = 1u + (nextrand() % 16u);
		else
			s = 1u + (nextrand() % V90_CONSTELLATION_MAX);
		mp.constellationSize[i] = s;
	}

	/*
	 * The count the object forms is `shaperSR + word_0 - 6`.  Split it two
	 * ways so that neither term is always the whole of it.
	 */
	mp.word_0 = 6u + (trial % 5u);
	mp.shaperSR = (int)bits - (int)(trial % 5u);
}

static int
run_calcmod(void)
{
	unsigned trial;
	long sixthSeparated = 0;
	long seedVaried = 0;

	diff_begin("V90ConstellationPower::calcModulusParameters");

	for (trial = 0; trial < 315; trial++) {
		/*
		 * 0..62 and not 0..63: `1LL << 63` shifts into the sign bit of
		 * a signed 64-bit type, which is undefined, and the object's
		 * own lack of a guard is recorded as a deviation rather than
		 * driven here.
		 */
		unsigned bits = trial % 63u;

		reseed(trial + 1u);
		setup_mp(trial, bits);

		fillbytes(cpAbuf, sizeof(cpAbuf));
		memcpy(cpBbuf, cpAbuf, sizeof(cpAbuf));
		if (trial == 0)
			memcpy(fill0, cpAbuf, sizeof(fill0));
		else if (memcmp(fill0, cpAbuf, sizeof(fill0)) != 0)
			seedVaried = 1;

		memcpy(mpSnap, &mp, sizeof(mp));

		our_calcmod(cpA, &mp);
		diff_eq_int("ours wrote the mapping block on trial %ld",
			    memcmp(mpSnap, &mp, sizeof(mp)), 0, trial);
		ref_calcmod(cpB, &mp);
		diff_eq_int("the blob wrote the mapping block on trial %ld",
			    memcmp(mpSnap, &mp, sizeof(mp)), 0, trial);

		diff_eq_obj("calcModulusParameters", V90ConstellationPower,
			    cpA, cpB, trial);

		/*
		 * The discriminating case for finding F3052: only where
		 * `remaining[5]` is at least the sixth size does a truncation
		 * differ from a remainder.
		 */
		if (cpA->remaining[V90CP_CONSTELLATIONS - 1]
		    >= (long long)mp.constellationSize[V90CP_CONSTELLATIONS - 1])
			sixthSeparated++;
	}

	diff_eq_int("the object fill varied between trials (%ld)",
		    seedVaried, 1, seedVaried);
	diff_eq_int("trials separating the sixth digit from a modulo: %ld",
		    sixthSeparated > 0, 1, sixthSeparated);

	return diff_end();
}

static int
run_getinfo(void)
{
	unsigned trial;
	long wroteNothing = 0;

	diff_begin("V90ConstellationPower::getConstellationInfo");

	for (trial = 0; trial < 243; trial++) {
		unsigned int index = trial % 9u;		/* 6..8 are out of range */
		int point = (int)((trial / 9u) % 3u);	/* 2 separates == 1 from != 0 */
		unsigned char before[sizeof(V90ConstellationPower)];

		reseed(trial + 4001u);
		setup_mp(trial, trial % 63u);

		fillbytes(cpAbuf, sizeof(cpAbuf));
		memcpy(cpBbuf, cpAbuf, sizeof(cpAbuf));
		memcpy(before, cpAbuf, sizeof(before));
		memcpy(mpSnap, &mp, sizeof(mp));

		our_getinfo(cpA, &mp, point, index);
		diff_eq_int("ours wrote the mapping block on trial %ld",
			    memcmp(mpSnap, &mp, sizeof(mp)), 0, trial);
		ref_getinfo(cpB, &mp, point, index);
		diff_eq_int("the blob wrote the mapping block on trial %ld",
			    memcmp(mpSnap, &mp, sizeof(mp)), 0, trial);

		diff_eq_obj("getConstellationInfo", V90ConstellationPower,
			    cpA, cpB, (long)(index * 4u + (unsigned)point));

		/*
		 * Out of range writes NOTHING, and that is a claim about the
		 * whole 144 bytes rather than about the two fields the
		 * in-range arms set.
		 */
		if (index > 5) {
			diff_eq_int("index %ld left ours untouched",
				    memcmp(before, cpAbuf, sizeof(before)), 0,
				    (long)index);
			diff_eq_int("index %ld left the blob's untouched",
				    memcmp(before, cpBbuf, sizeof(before)), 0,
				    (long)index);
			wroteNothing++;
		}
	}

	diff_eq_int("out-of-range indices driven: %ld", wroteNothing > 0, 1,
		    wroteNothing);

	return diff_end();
}

static int
run_getpower(void)
{
	unsigned trial;
	long armGreater = 0, armEqual = 0, armLess = 0;
	long lawA = 0, lawMu = 0;
	long pointCodec = 0, pointOther = 0, pointTwo = 0;

	diff_begin("V90ConstellationPower::getPower");

	for (trial = 0; trial < 300; trial++) {
		unsigned int bits = 6u + trial % 50u;
		int point = (int)(trial % 3u);
		int pcmType = (int)((trial / 3u) % 2u);
		float a, b;
		unsigned int i, j;

		reseed(trial + 9001u);
		setup_mp(trial, bits);

		fillbytes(cpAbuf, sizeof(cpAbuf));
		memcpy(cpBbuf, cpAbuf, sizeof(cpAbuf));
		memcpy(mpSnap, &mp, sizeof(mp));

		a = our_getpower(cpA, &mp, point, pcmType);
		diff_eq_int("ours wrote the mapping block on trial %ld",
			    memcmp(mpSnap, &mp, sizeof(mp)), 0, trial);
		b = ref_getpower(cpB, &mp, point, pcmType);
		diff_eq_int("the blob wrote the mapping block on trial %ld",
			    memcmp(mpSnap, &mp, sizeof(mp)), 0, trial);

		/*
		 * EXACT, no ULP budget.  Both sides are the same instruction
		 * sequence over the same 387 stack; a last-place difference
		 * here would be a difference in the arithmetic and not in the
		 * spilling.
		 */
		diff_eq_float("getPower trial %ld", a, b, trial);
		diff_eq_obj("getPower", V90ConstellationPower, cpA, cpB, trial);

		if (pcmType == 0)
			lawMu++;
		else
			lawA++;
		if (point == 1)
			pointCodec++;
		else if (point == 2)
			pointTwo++;
		else
			pointOther++;

		/*
		 * The three arms, recomputed from the state the call left
		 * behind: `modulus` does not change while the loop runs, so
		 * classifying every (i, j) the loop visited is exact.
		 */
		for (i = 0; i < V90CP_CONSTELLATIONS; i++)
			for (j = 0; j < mp.constellationSize[i]; j++) {
				if (cpA->modulus[i] > j)
					armGreater++;
				else if (cpA->modulus[i] == j)
					armEqual++;
				else
					armLess++;
			}
	}

	diff_eq_int("the modulus-above arm ran %ld times", armGreater > 0, 1,
		    armGreater);
	diff_eq_int("the modulus-equal arm ran %ld times", armEqual > 0, 1,
		    armEqual);
	diff_eq_int("the modulus-below arm ran %ld times", armLess > 0, 1,
		    armLess);
	diff_eq_int("A-law trials: %ld", lawA > 0, 1, lawA);
	diff_eq_int("mu-law trials: %ld", lawMu > 0, 1, lawMu);
	diff_eq_int("point == 1 trials: %ld", pointCodec > 0, 1, pointCodec);
	diff_eq_int("point == 0 trials: %ld", pointOther > 0, 1, pointOther);
	diff_eq_int("point == 2 trials: %ld", pointTwo > 0, 1, pointTwo);

	return diff_end();
}

/*
 * TABLE 1/V.90 THROUGH `getPower`'S OWN UCODE CONVERSION.
 *
 * Make a one-codeword mapping: every one of the six mapping-frame
 * constellations contains the same single Ucode, `word_0 + shaperSR - 6` is
 * zero, and therefore every selected level has fraction one.  The method's
 * sum and final factor of 1/6 reduce to exactly that Table 1 level squared.
 * This enters the production code which applies the mu-law/A-law Ucode mask;
 * it does not duplicate either mask in the test.
 *
 * The expected value is formed from the Recommendation's integer linear
 * level, squared in double (exact throughout this range), then converted to
 * float once.  `diff_eq_float` is intentionally bit-exact: there is no ULP
 * allowance for a result that is exactly representable by that specified
 * conversion.  Reconstruction and blob each get their own oracle assertion.
 */
static int
run_getpower_table1_side(const char *name, getpower_fn getpower,
			 V90ConstellationPower *cp)
{
	int law;

	diff_begin(name);

	for (law = 0; law < 2; law++) {
		unsigned int u;

		for (u = 0; u < 128; u++) {
			const struct v90_table1_row *row = &v90_table1[u];
			int level = law ? row->a_linear : row->mu_linear;
			float want = (float)((double)level * (double)level);
			float got;
			unsigned int i;
			long tag = (long)(law * 128 + u);

			memset(&mp, 0, sizeof(mp));
			mp.word_0 = 6;
			mp.shaperSR = 0;
			for (i = 0; i < V90CP_CONSTELLATIONS; i++) {
				mp.constellationSize[i] = 1;
				mp.constellation[i][0] = (unsigned char)u;
				mp.codecConstellation[i][0] = (unsigned char)u;
			}
			memcpy(mpSnap, &mp, sizeof(mp));

			reseed((unsigned)tag + 13001u);
			fillbytes(cp, sizeof(*cp));

			got = getpower(cp, &mp, 0, law);
			diff_eq_int("left the Table 1 mapping unchanged (%ld)",
				    memcmp(mpSnap, &mp, sizeof(mp)), 0, tag);

			diff_eq_float("Table 1 squared level for Ucode %ld",
				      got, want, tag);
		}
	}

	return diff_end();
}

static int
run_getpower_table1(void)
{
	int rc = 0;

	rc |= run_getpower_table1_side(
	    "V90ConstellationPower::getPower, reconstruction vs Table 1/V.90",
	    our_getpower, cpA);
	rc |= run_getpower_table1_side(
	    "V90ConstellationPower::getPower, blob vs Table 1/V.90",
	    ref_getpower, cpB);

	return rc;
}

static int
run_ladder(void)
{
	unsigned int i;
	long walkedToZero = 0, stoppedAtTop = 0, stoppedInside = 0;

	diff_begin("V90ConstellationPower::getPowerIndexForPower");

	/*
	 * The table itself, byte for byte against the blob's own copy.  The
	 * `ref_` alias makes this a comparison of two distinct objects rather
	 * than of a symbol with itself.
	 */
	diff_eq_int("averagePowerLimits differs from the blob's (%ld bytes)",
		    memcmp(V90ConstellationPower::averagePowerLimits,
			   ref_averagePowerLimits,
			   sizeof(ref_averagePowerLimits)),
		    0, (long)sizeof(ref_averagePowerLimits));

	reseed(20001u);
	fillbytes(cpAbuf, sizeof(cpAbuf));
	memcpy(cpBbuf, cpAbuf, sizeof(cpAbuf));
	memcpy(mpSnap, cpAbuf, sizeof(cpAbuf));

	for (i = 0; i < V90CP_POWER_INDICES; i++) {
		float exact = (float)V90ConstellationPower::averagePowerLimits[i];
		float cases[3];
		unsigned int c;

		cases[0] = exact;
		cases[1] = exact * 1.0001f;
		cases[2] = exact * 0.9999f;

		for (c = 0; c < 3; c++) {
			unsigned int a = our_pindex(cpA, cases[c]);
			unsigned int b = ref_pindex(cpB, cases[c]);

			diff_eq_int("getPowerIndexForPower(limits[%ld] case)",
				    a, b, (long)i);
			if (a == 0)
				walkedToZero++;
			else if (a == V90CP_POWER_INDICES - 1)
				stoppedAtTop++;
			else
				stoppedInside++;
		}
	}

	{
		static const float edge[] = {
			0.0f, 1.0f, -1.0f, -1.0e30f, 1.0e30f,
			4549688.0f, 4549689.0f, 4549690.0f,
			228735375.0f, 228735376.0f, 228735377.0f
		};
		unsigned int c;

		for (c = 0; c < sizeof(edge) / sizeof(edge[0]); c++) {
			unsigned int a = our_pindex(cpA, edge[c]);
			unsigned int b = ref_pindex(cpB, edge[c]);

			diff_eq_int("getPowerIndexForPower(edge %ld)", a, b,
				    (long)c);
		}
	}

	/* Both infinities, formed without a division the compiler folds. */
	{
		float huge = 1.0e30f;
		float pinf = huge * huge * huge;
		unsigned int a = our_pindex(cpA, pinf);
		unsigned int b = ref_pindex(cpB, pinf);

		diff_eq_int("getPowerIndexForPower(+inf)", a, b, 0);
		a = our_pindex(cpA, -pinf);
		b = ref_pindex(cpB, -pinf);
		diff_eq_int("getPowerIndexForPower(-inf)", a, b, 0);
	}

	/*
	 * Neither side touches the object, and that is a negative claim, so
	 * each is compared against the state it had BEFORE the sweep rather
	 * than against the other -- two sides that both wrote the same thing
	 * would compare equal to each other and prove nothing.
	 */
	diff_eq_int("ours wrote the object (%ld)",
		    memcmp(mpSnap, cpAbuf, sizeof(cpAbuf)), 0, 0);
	diff_eq_int("the blob wrote the object (%ld)",
		    memcmp(mpSnap, cpBbuf, sizeof(cpBbuf)), 0, 0);

	diff_eq_int("the walk reached 0 %ld times", walkedToZero > 0, 1,
		    walkedToZero);
	diff_eq_int("the guard exited at 34 %ld times", stoppedAtTop > 0, 1,
		    stoppedAtTop);
	diff_eq_int("the walk stopped inside %ld times", stoppedInside > 0, 1,
		    stoppedInside);

	return diff_end();
}

static int
run_islegal(void)
{
	static const unsigned int cases[] = {
		0u, 1u, 2u, 33u, 34u, 35u, 36u, 100u,
		0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu
	};
	unsigned int c;

	diff_begin("V90ConstellationPower::isLegalPowerIndex");

	for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
		unsigned char a = our_islegal(cpA, cases[c]);
		unsigned char b = ref_islegal(cpB, cases[c]);

		diff_eq_int("isLegalPowerIndex(%ld)", a, b, (long)cases[c]);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	cpA = (V90ConstellationPower *)cpAbuf;
	cpB = (V90ConstellationPower *)cpBbuf;

	rc |= run_calcmod();
	rc |= run_getinfo();
	rc |= run_getpower();
	rc |= run_getpower_table1();
	rc |= run_ladder();
	rc |= run_islegal();

	return rc;
}
