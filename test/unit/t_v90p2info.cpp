/*
 * t_v90p2info.cpp -- differential test of V90Phase2Info::printInfo() const.
 *
 * THE FUNCTION IS NOTHING BUT DIAGNOSTICS, so the transcript IS the result.
 * There is no return value, no output parameter, and -- being `const` -- not
 * even a store to the object.  Comparing the two objects afterwards is still
 * worth doing, but on its own it is exactly the vacuous check findings F223
 * and 224 warn about: two identically seeded objects that neither side writes
 * agree no matter what the code does.  What decides this test is
 * `dsplib_debug_capture_text`, character for character, on all six call
 * sites.
 *
 * WHY THE `edprintf` SITE COMPARES HERE AND DOES NOT IN t_v90p3mod.
 * `dsplibs_debug_printf` is imported by the blob, so a gated site goes
 * straight to the harness on both sides and the two transcripts line up.
 * `edprintf` is DEFINED in the blob, so each side runs its own copy over its
 * own key counter, and t_v90p3mod's fixture keeps away from it for that
 * reason.  It is safe here because `edprintf` assigns `iEncodeOffset = 0`
 * before it encodes anything (src/core/encode.c): the key is reset per call,
 * not carried, so one call on each side of one identical message produces one
 * identical encoded line however many calls preceded it.  t_encode compares
 * the two the same way.
 *
 * THE LEVEL IS SWEPT 0 TO 3, not just raised, which is finding F150's rule.
 * Every gate in this function is `> 1`, so 0 and 1 must print NOTHING and 2
 * and 3 must print all twenty-six lines; a site whose gate was dropped, or
 * set at `> 2` instead, is identical to the object at one level and differs
 * at another.  The line counts are asserted against the literal 26 and 0 as
 * well as against the blob's, because two silent sides agree about nothing
 * (finding F149) -- and 26 is 4 gated header lines + 1 `edprintf` line + 21
 * for L2, so a dropped site moves it.
 *
 * THE OBJECT IS NEVER ZEROED.  Both sides are seeded with the same varied
 * pseudorandom bytes before every call, then the fields the function reads
 * are set to the case's values.  `L2` has to be a real pointer or the seed
 * would be dereferenced, so both sides are pointed at ONE shared table: the
 * function is const, nothing writes through it, and using one array rather
 * than two identical ones keeps the pointer word itself comparable.
 *
 * THE FLOAT CASES ARE THE POINT OF THE TABLES.  `printInfo` prints a float as
 * sign, magnitude and scaled fraction because the channel has no %f, and the
 * three parts fail in different ways: the sign at zero (the object's test is
 * `0 < v`, so zero prints as negative), the magnitude at a negative value,
 * and the fraction wherever the scaled product lands near an integer -- which
 * is where an intermediate rounded to `float` instead of the object's x87
 * stack would cross.  Table 2 exists for that last one.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Parameters.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;
void ref_printInfo(const void *self) asm("ref__ZNK13V90Phase2Info9printInfoEv");

/*
 * The constructor by symbol on both sides.  C++ has no syntax for running a
 * constructor over storage that already exists, and `OURS = V90Phase2Info(p)`
 * would build a temporary over uninitialised stack and copy the WHOLE object,
 * which destroys the property this fixture is built on: the slot is seeded
 * and never zeroed, so a byte the constructor fails to write is a byte that
 * differs.  t_v90jd.cpp carries the argument in full.
 */
void our_ctor1(void *self, V90Parameters *p)
	asm("_ZN13V90Phase2InfoC1EP13V90Parameters");
void ref_ctor1(void *self, V90Parameters *p)
	asm("ref__ZN13V90Phase2InfoC1EP13V90Parameters");
void our_ctor2(void *self, V90Parameters *p)
	asm("_ZN13V90Phase2InfoC2EP13V90Parameters");
void ref_ctor2(void *self, V90Parameters *p)
	asm("ref__ZN13V90Phase2InfoC2EP13V90Parameters");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 64

union p2i_slot {
	unsigned char raw[SLOT];
	int align;	/* the object holds ints and pointers; 4-align it */
};

static union p2i_slot ours, theirs;

/*
 * The object lives IN the byte array.  V90Phase2Info has a user-declared
 * constructor now -- `vpcm_create` cannot link without the symbol -- and a
 * class with one has no default constructor, so a union with it as a variant
 * member has its own default constructor deleted.  t_v90jd.cpp carries the
 * full argument.
 */
#define OURS	(*(V90Phase2Info *)ours.raw)
#define THEIRS	(*(V90Phase2Info *)theirs.raw)

/* 4 gated header lines + 1 edprintf line + V90PHASE2INFO_L2. */
#define LINES_AT_LEVEL_2 (4 + 1 + V90PHASE2INFO_L2)

/* ------------------------------------------------------------------ seeds */

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

/* ------------------------------------------------------------ L2 fixtures */

#define NTABLE 5

static float l2[NTABLE][V90PHASE2INFO_L2];

static void
build_tables(void)
{
	unsigned lfsr = 0xacedu;
	int i;

	for (i = 0; i < V90PHASE2INFO_L2; i++) {
		int n;

		/* 0: a spread of both signs with three decimals. */
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		n = (int)(lfsr % 200000u) - 100000;
		l2[0][i] = (float)n / 1000.0f;

		/* 1: large magnitudes, so the whole part is not a digit. */
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		n = (int)(lfsr % 65535u) - 32767;
		l2[1][i] = (float)n * 7.375f;

		/*
		 * 2: the scaled fraction just under and just over an
		 * integer, which is where a `float` intermediate and the
		 * object's x87 one part company.
		 */
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		n = (int)(lfsr % 2000u) - 1000;
		l2[2][i] = (float)n / 1000.0f
			   + ((i & 1) ? 4.999e-4f : -4.999e-4f);

		/* 3: tiny, where the whole part is zero and the sign is all
		 * that separates the two halves. */
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		n = (int)(lfsr % 4000u) - 2000;
		l2[3][i] = (float)n * 1.0e-5f;
	}

	/* 4: the named edge cases, in a fixed order. */
	{
		static const float edge[V90PHASE2INFO_L2] = {
			0.0f, -0.0f, 1.0f, -1.0f,
			0.5f, -0.5f, 0.999f, -0.999f,
			0.9999f, -0.9999f, 0.001f, -0.001f,
			0.0005f, -0.0005f, 123.456f, -123.456f,
			1000.0f, -1000.0f, 32767.5f, -32767.5f,
			2.0e6f
		};

		for (i = 0; i < V90PHASE2INFO_L2; i++)
			l2[4][i] = edge[i];
	}
}

/* --------------------------------------------------------------- the runs */

/*
 * One invocation on each side, with the transcripts compared.  Returns the
 * number of lines OUR side printed, so the caller can check the total is not
 * zero.
 */
static int
compare_once(long tag, int want_lines)
{
	unsigned char before[SLOT];

	memcpy(before, ours.raw, SLOT);
	dsplib_debug_capture_reset();

	OURS.printInfo();
	ref_printInfo(&THEIRS);

	diff_eq_int("transcript line count (case %ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);
	if (want_lines >= 0)
		diff_eq_int("our line count is the object's six sites "
			    "(case %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)want_lines, tag);
	diff_eq_int("transcript text (case %ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);

	/* const means const: neither side may have written anything. */
	diff_eq_obj("after printInfo", V90Phase2Info, &OURS, &THEIRS, tag);
	diff_eq_int("printInfo wrote nothing (case %ld)",
		    memcmp(before, ours.raw, SLOT) == 0, 1, tag);
	diff_eq_int("no store past the object (case %ld)",
		    memcmp(ours.raw + sizeof(V90Phase2Info),
			   theirs.raw + sizeof(V90Phase2Info),
			   SLOT - sizeof(V90Phase2Info)) == 0, 1, tag);

	return (int)dsplib_debug_capture_lines(0);
}

static void
set_case(int trial, int table)
{
	OURS.pcmType = THEIRS.pcmType = (trial % 4) - 1;
	OURS.rtd = THEIRS.rtd = (int)(0x51a7 * trial) - 40000;
	OURS.Uinfo = THEIRS.Uinfo = (unsigned char)(trial * 37);
	OURS.maxTxPower = THEIRS.maxTxPower = (unsigned char)(trial * 11);
	OURS.txPowerMeasurementPoint = THEIRS.txPowerMeasurementPoint =
	    (trial % 3) - 1;
	OURS.L2 = THEIRS.L2 = l2[table];
}

/*
 * The main sweep: every table against every debug level.
 */
static int
run_sweep(void)
{
	int lvl, trial, table, printed = 0, ref_printed = 0;

	diff_begin("V90Phase2Info::printInfo, levels 0..3");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl < 4; lvl++) {
		dsplibs_debug_level = ref_dsplibs_debug_level =
		    (unsigned int)lvl;

		for (table = 0; table < NTABLE; table++)
		for (trial = 0; trial < 12; trial++) {
			long tag = lvl * 10000 + table * 100 + trial;

			seed(trial + table * 12);
			set_case(trial, table);

			printed += compare_once(tag,
			    (lvl > 1) ? LINES_AT_LEVEL_2 : 0);
			ref_printed += (int)dsplib_debug_capture_lines(1);
		}
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	/*
	 * Anti-vacuity, finding F149: half of the sweep is at a level where
	 * nothing prints, and two empty transcripts agree about nothing.
	 */
	diff_eq_int("our sites printed something", printed > 0, 1, 0);
	diff_eq_int("the blob's sites printed something", ref_printed > 0, 1,
		    0);

	return diff_end();
}

/*
 * maxTxPower is one byte and the printed value is (maxTxPower + 1) * -0.5, so
 * all 256 of them fit in one loop and half have a fractional half-decibel.
 * Worth doing exhaustively: this is the only field whose printed form is an
 * arithmetic function of it rather than a copy.
 */
static int
run_maxtxpower(void)
{
	int b;

	diff_begin("V90Phase2Info::printInfo, every maxTxPower");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (b = 0; b < 256; b++) {
		seed(b & 7);
		set_case(b & 3, b % NTABLE);
		OURS.maxTxPower = THEIRS.maxTxPower = (unsigned char)b;

		compare_once(200000 + b, LINES_AT_LEVEL_2);
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}

/*
 * The two string-valued fields, over values either side of the one the
 * object tests for.  `pcmType` and `txPowerMeasurementPoint` are both
 * `== 1 ? A : B`, so 0, 1 and 2 are the three cases that matter and a
 * negative one shows the comparison is not `>= 1` or a truncation.
 */
static int
run_enums(void)
{
	int a, b;

	diff_begin("V90Phase2Info::printInfo, the two %s fields");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 3;

	for (a = -2; a <= 3; a++)
	for (b = -2; b <= 3; b++) {
		seed(a * 7 + b + 20);
		set_case(a + 2, (b + 2) % NTABLE);
		OURS.pcmType = THEIRS.pcmType = a;
		OURS.txPowerMeasurementPoint =
		    THEIRS.txPowerMeasurementPoint = b;
		/* 0x10001 keeps the low byte and low half-word from being the
		 * whole of the difference between two cases. */
		OURS.rtd = THEIRS.rtd = a * 0x10001 + b;

		compare_once(300000 + (a + 2) * 10 + (b + 2),
			     LINES_AT_LEVEL_2);
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}

/*
 * ===========================================================================
 * The constructor.
 *
 * ONE PARAMETER BLOCK, SHARED and reseeded per trial: the constructor only
 * reads it, and two separately seeded blocks would agree whatever was read,
 * so a divergence in WHICH field was read would be invisible.
 *
 * TWO OF THE FIVE COPIES GO THROUGH `setne`, so what has to be swept is not
 * only the value but whether it is zero: a parameter of 0 and a parameter of
 * 7 must produce 0 and 1, and a reconstruction that copied the word instead
 * of testing it agrees on the first and not the second.  `boolean_case`
 * below forces both, on both fields, on top of the random seed.
 * ===========================================================================
 */
union param_slot {
	unsigned char raw[sizeof(V90Parameters)];
	int align;
};

static union param_slot params;

static void
seed_params(int trial)
{
	unsigned lfsr = 0x51edu + 0x4f1bu * (unsigned)trial;
	unsigned i;
	V90Parameters *p = (V90Parameters *)params.raw;

	for (i = 0; i < sizeof(params.raw); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		params.raw[i] = (unsigned char)(lfsr >> 5);
	}

	/* Force both arms of both `setne`s across the run. */
	p->PHASE2_INFO_A_OR_MU = (trial & 1) ? 0 : (trial * 7 + 1);
	p->PHASE2_INFO_TX_POWER_MEASURE_POINT = (trial & 2) ? 0 : (trial + 3);
}

#define NCTOR 32

static int
run_ctor(void)
{
	int trial, moved = 0, saw_zero = 0, saw_one = 0;

	diff_begin("V90Phase2Info::V90Phase2Info(V90Parameters *)");

	for (trial = 0; trial < NCTOR; trial++) {
		unsigned char before[SLOT];
		V90Parameters *p = (V90Parameters *)params.raw;

		seed(trial);
		seed_params(trial);
		memcpy(before, ours.raw, SLOT);

		our_ctor1(&OURS, p);
		ref_ctor1(&THEIRS, p);

		diff_eq_obj("after V90Phase2Info(params) [C1]", V90Phase2Info,
			    &OURS, &THEIRS, trial);
		diff_eq_int("no store past the object, C1 (trial %ld)",
			    memcmp(ours.raw + sizeof(V90Phase2Info),
				   theirs.raw + sizeof(V90Phase2Info),
				   SLOT - sizeof(V90Phase2Info)) == 0,
			    1, trial);
		/*
		 * The pointer it parks at +0x20 is checked against the block
		 * we passed rather than merely for being non-null: both sides
		 * were handed the same one.
		 */
		diff_eq_int("params was stored (trial %ld)",
			    OURS.params == p, 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (OURS.pcmType == 0)
			saw_zero = 1;
		if (OURS.pcmType == 1)
			saw_one = 1;

		seed(trial);
		our_ctor2(&OURS, p);
		ref_ctor2(&THEIRS, p);

		diff_eq_obj("after V90Phase2Info(params) [C2]", V90Phase2Info,
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
	int rc = 0;

	build_tables();

	rc |= run_sweep();
	rc |= run_maxtxpower();
	rc |= run_enums();
	rc |= run_ctor();

	return rc;
}
