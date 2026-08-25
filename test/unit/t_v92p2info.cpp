/*
 * t_v92p2info.cpp -- differential test of V92Phase2Info::V92Phase2Info.
 *
 * The class's first test.  Until the constructor there was nothing to drive:
 * `include/dsplib/V92Phase2Info.h` was a data-only header, its offsets
 * asserted from src/pump/v90/VPcmFloModem.cpp because the class had no source
 * file, and the only thing that touched an instance was VPcmFloModem's own
 * fixture comparing a block of bytes.
 *
 * THE CONSTRUCTOR IS DRIVEN BY SYMBOL, on both sides.  C++ offers no syntax
 * for running a constructor over storage that already exists, and
 * `OURS = V92Phase2Info(p)` would construct a temporary over uninitialised
 * stack and then copy the WHOLE object -- which destroys the one property
 * this fixture is built on.  t_v90jd.cpp carries the argument in full.
 *
 * THE SLOT IS NEVER ZEROED.  Both sides are seeded with the same varied
 * pseudorandom bytes before every call.  It matters more here than usual: the
 * constructor writes fourteen of the object's forty-four bytes, so the four
 * array pointers at +0x18..+0x27, `pad_0a` and everything else must come
 * through untouched, and a clear-loop invented in the reconstruction would be
 * invisible against a zeroed object.
 *
 * WHAT THE SWEEP IS FOR.  Two of the five parameter copies go through
 * `cmpl $0x0` + `setne`, so the value is TESTED and not carried: a
 * reconstruction that copied the word agrees whenever the parameter happens
 * to be 0 or 1 and differs otherwise.  `seed_params` forces both arms of both
 * of them across the run, and the run asserts that each value was actually
 * observed -- two silent sides agree about nothing (finding F149).
 *
 * Three more copies are a whole-word load with a byte store, so only the low
 * byte of each survives; the seed puts values above 255 into all three so
 * that a reconstruction storing the whole word fails rather than passing by
 * luck.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase2Info.h"

extern "C" {
void our_ctor1(void *self, V92Parameters *p)
	asm("_ZN13V92Phase2InfoC1EP13V92Parameters");
void ref_ctor1(void *self, V92Parameters *p)
	asm("ref__ZN13V92Phase2InfoC1EP13V92Parameters");
void our_ctor2(void *self, V92Parameters *p)
	asm("_ZN13V92Phase2InfoC2EP13V92Parameters");
void ref_ctor2(void *self, V92Parameters *p)
	asm("ref__ZN13V92Phase2InfoC2EP13V92Parameters");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 64

union p2i_slot {
	unsigned char raw[SLOT];
	int align;
};

static union p2i_slot ours, theirs;

#define OURS	(*(V92Phase2Info *)ours.raw)
#define THEIRS	(*(V92Phase2Info *)theirs.raw)

union param_slot {
	unsigned char raw[sizeof(V92Parameters)];
	int align;
};

/*
 * ONE PARAMETER BLOCK, SHARED.  The constructor only reads it, and pointing
 * both sides at one block is what makes "which field was read" testable: two
 * separately seeded blocks would agree whatever the answer.
 */
static union param_slot params;

static void
seed(int trial)
{
	unsigned lfsr = 0x1234u + 0x9e37u * (unsigned)trial;
	int i;

	for (i = 0; i < SLOT; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		ours.raw[i] = theirs.raw[i] = (unsigned char)(lfsr >> 3);
	}
}

static void
seed_params(int trial)
{
	unsigned lfsr = 0x51edu + 0x4f1bu * (unsigned)trial;
	unsigned i;
	V92Parameters *p = (V92Parameters *)params.raw;

	for (i = 0; i < sizeof(params.raw); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		params.raw[i] = (unsigned char)(lfsr >> 5);
	}

	/* Both arms of both `setne` copies, across the run. */
	p->V92_PHASE2_INFO_A_OR_MU = (trial & 1) ? 0 : (trial * 7 + 1);
	p->V92_PHASE2_INFO_TX_POWER_MEASURE_POINT =
	    (trial & 2) ? 0 : (trial + 3);

	/*
	 * The five load-word-store-byte copies, given values whose low byte
	 * is not the whole value.  A reconstruction that stored the word
	 * would differ here and agree on a small one.
	 */
	p->V92_PHASE2_INFO_UINFO = 0x1200 + trial;
	p->V92_PHASE2_INFO_MAX_TX_POWER = 0x3400 + trial * 3;
	p->V92_NOF_FILTER_SECTIONS = 0x5600 + trial * 5;
	p->V92_MAX_TOTAL_NOF_COEFFS = 0x7800 + trial * 7;
	p->V92_MAX_NOF_COEFFS_IN_EACH_SECTION = 0x9a00 + trial * 11;

	p->V92_PHASE2_INFO_RTD = (int)(0x51a7 * (unsigned)trial) - 40000;
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V92Phase2Info),
		      theirs.raw + sizeof(V92Phase2Info),
		      SLOT - sizeof(V92Phase2Info)) == 0;
}

#define NCTOR 32

static int
run_ctor(void)
{
	int trial, moved = 0, saw_zero = 0, saw_one = 0;

	diff_begin("V92Phase2Info::V92Phase2Info(V92Parameters *)");

	for (trial = 0; trial < NCTOR; trial++) {
		unsigned char before[SLOT];
		V92Parameters *p = (V92Parameters *)params.raw;

		seed(trial);
		seed_params(trial);
		memcpy(before, ours.raw, SLOT);

		our_ctor1(&OURS, p);
		ref_ctor1(&THEIRS, p);

		diff_eq_obj("after V92Phase2Info(params) [C1]", V92Phase2Info,
			    &OURS, &THEIRS, trial);
		diff_eq_int("no store past the object, C1 (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("params was stored at +0x28 (trial %ld)",
			    OURS.params == p, 1, trial);

		/*
		 * The four constants, asserted against their literals as well
		 * as against the blob: a reconstruction that dropped the store
		 * altogether would agree with a blob-side byte that happened
		 * to be seeded the same, and would not agree with these.
		 */
		diff_eq_int("v92CapabilitiesLocal is 1 (trial %ld)",
			    OURS.v92CapabilitiesLocal, 1, trial);
		diff_eq_int("shortPhase2Local is 0 (trial %ld)",
			    OURS.shortPhase2Local, 0, trial);
		diff_eq_int("shortPhase2Remote is 0 (trial %ld)",
			    OURS.shortPhase2Remote, 0, trial);
		diff_eq_int("v92CapabilitiesRemote is 0 (trial %ld)",
			    OURS.v92CapabilitiesRemote, 0, trial);
		diff_eq_int("v90UseHighCarrier is 0 (trial %ld)",
			    OURS.v90UseHighCarrier, 0, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (OURS.pcmType == 0)
			saw_zero = 1;
		if (OURS.pcmType == 1)
			saw_one = 1;

		seed(trial);
		our_ctor2(&OURS, p);
		ref_ctor2(&THEIRS, p);

		diff_eq_obj("after V92Phase2Info(params) [C2]", V92Phase2Info,
			    &OURS, &THEIRS, trial);
		diff_eq_int("C1 and C2 agree (trial %ld)",
			    memcmp(ours.raw, theirs.raw, SLOT), 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("pcmType took the value 0 somewhere in the run", saw_zero,
		    1, 0);
	diff_eq_int("pcmType took the value 1 somewhere in the run", saw_one,
		    1, 0);

	return diff_end();
}

int
main(void)
{
	return run_ctor();
}
