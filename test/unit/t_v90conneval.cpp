/*
 * t_v90conneval.cpp -- differential test of V90ConnectionEvaluator: the
 * lifecycle, and the six small members of its processing half.
 *
 * THIS BINARY IS THE `v90conneval` MUTATION SUITE'S TARGET.  The set was
 * pointed at `t_v90leaves` while the class was three members long; six
 * mutations are recorded against the constructor and `reset`, and
 * `suites.json` names ONE binary per set -- so the lifecycle is driven here
 * as well, or repointing the set would report NOT CAUGHT for all six and look
 * exactly like an untested claim.  `t_v90leaves` keeps its own copy; the
 * duplication is deliberate and is what makes the repoint safe.
 *
 * WHAT THE SIX NEW MEMBERS NEED THAT THE LIFECYCLE DID NOT
 *
 *   THE OBJECT DECIDES SOMETHING, so agreeing with the blob is not enough --
 *   an evaluator that always answers the same thing agrees with anything
 *   (findings 149 and 223).  `indicateLocalRetrain` and
 *   `indicateRemoteRetrain` each have two verdicts, and the run below asserts
 *   that BOTH were observed for BOTH functions, and that the two really are
 *   different numbers rather than the same number twice.
 *
 *   THE LIMIT COMPARISON IS UNSIGNED, which every ordinary value hides.  The
 *   object has `cmp 0x460(%eax),%edx; ja` against a parameter slot the map
 *   calls `int`, and `ja` and `jg` agree on every pair of small positive
 *   numbers.  Two trials below are chosen so they DISAGREE: a negative limit,
 *   where the unsigned reading never fires and the signed one always does,
 *   and a counter stepped across 2^31, where it is the other way round.
 *
 *   THE AVERAGE ACCUMULATES, so one call proves nothing.  `updateAvePdsnr` is
 *   driven in runs of two hundred with the object compared after EVERY call:
 *   a divergence at the fortieth that the sixtieth washes out is still a
 *   defect.  The count is deliberately started near 2^31 in some runs, since
 *   `fildll` of a zero-extended count and `fild` of a signed one agree
 *   everywhere below that and nowhere above it.
 *
 *   THE DIAGNOSTICS ARE UNGATED.  Four of the six call `edprintf`, which
 *   encodes whether or not anything is listening and only then tests the
 *   level -- so unlike `V90MP::printNofRecievedMpMpNot` there is no branch in
 *   these functions to drive, and what the transcript checks is the format
 *   and the arguments.  Both sides run their OWN copy of `edprintf`, which is
 *   safe because it assigns `iEncodeOffset = 0` before encoding: the key is
 *   per call, so however many calls preceded it the same text encodes the
 *   same way (the argument t_v90p2info.cpp sets out).
 *
 * The standing rules: the same varied pseudorandom bytes into both sides,
 * never zeroed; a 68-byte guard past the object compared on both sides
 * against the seed; and every block asserting that the call changed something
 * and did not change it to the same something every trial.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"
/*
 * The NAMED 0x558 parameter map, not `V90PreFilter.h`'s 0x504 word block.  No
 * translation unit may include both (finding 1112), and the names are the
 * point here: the retrain limits are read by name.
 */
#include "dsplib/V90Parameters.h"
#include "dsplib/V90ConnectionEvaluator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void our_ce_ctor(void *, void *)
	asm("_ZN22V90ConnectionEvaluatorC1EP13V90Parameters");
void our_ce_ctor2(void *, void *)
	asm("_ZN22V90ConnectionEvaluatorC2EP13V90Parameters");
void ref_ce_ctor(void *, void *)
	asm("ref__ZN22V90ConnectionEvaluatorC1EP13V90Parameters");
void ref_ce_ctor2(void *, void *)
	asm("ref__ZN22V90ConnectionEvaluatorC2EP13V90Parameters");
void our_ce_dtor(void *) asm("_ZN22V90ConnectionEvaluatorD1Ev");
void our_ce_dtor2(void *) asm("_ZN22V90ConnectionEvaluatorD2Ev");
void ref_ce_dtor(void *) asm("ref__ZN22V90ConnectionEvaluatorD1Ev");
void ref_ce_dtor2(void *) asm("ref__ZN22V90ConnectionEvaluatorD2Ev");

void ref_ce_reset(void *) asm("ref__ZN22V90ConnectionEvaluator5resetEv");
void ref_ce_updateAvePdsnr(void *, float, unsigned int)
	asm("ref__ZN22V90ConnectionEvaluator14updateAvePdsnrEfj");
void ref_ce_updateConst(void *, short, float, float, float)
	asm("ref__ZN22V90ConnectionEvaluator30updateCurrentConstellationDataEsfff");
void ref_ce_remoteRateReneg(void *)
	asm("ref__ZN22V90ConnectionEvaluator23indicateRemoteRateRenegEv");
int ref_ce_localRetrain(void *)
	asm("ref__ZN22V90ConnectionEvaluator20indicateLocalRetrainEv");
int ref_ce_remoteRetrain(void *)
	asm("ref__ZN22V90ConnectionEvaluator21indicateRemoteRetrainEv");
int ref_ce_meanErrPhase4(void *, float, float)
	asm("ref__ZN22V90ConnectionEvaluator26evaluateMeanErrorStdPhase4Eff");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned char
next_byte(int mode, unsigned i)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0xa5;
	case 2:
		return 0xff;			/* the -1s reset writes  */
	case 3:
		return (unsigned char)((lfsr & 0xfe) | (unsigned)(i & 1u));
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

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

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static int transcripts_seen;

static void
transcript_matches(long tag)
{
	diff_eq_int("transcript matches (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("line counts match (%ld)",
		    (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), tag);
}

/* ----------------------------------------------------------- the storage */

#define CE_SLOT		((unsigned)sizeof(V90ConnectionEvaluator) + 68u)
#define PARM_SLOT	((unsigned)sizeof(V90Parameters) + 64u)

static unsigned char ce_a[CE_SLOT] __attribute__((aligned(8)));
static unsigned char ce_b[CE_SLOT] __attribute__((aligned(8)));
static unsigned char ce_s[CE_SLOT];
static unsigned char parm_a[PARM_SLOT] __attribute__((aligned(8)));
static unsigned char parm_b[PARM_SLOT] __attribute__((aligned(8)));

#define CEA	((V90ConnectionEvaluator *)ce_a)
#define CEB	((V90ConnectionEvaluator *)ce_b)
#define PA	((V90Parameters *)parm_a)

/*
 * ONE PARAMETER BLOCK FOR BOTH SIDES.  The pointer at +0x00 is the one field
 * that would otherwise hold two different addresses, and `diff_eq_obj`
 * compares the object whole; pointing both at `parm_a` is what lets the
 * comparison include +0x00 rather than skip it.
 */
static void
seed_pair(int trial, int mode)
{
	fill_pair(ce_a, ce_b, CE_SLOT, trial, mode);
	memcpy(ce_s, ce_b, CE_SLOT);
	CEA->params = PA;
	CEB->params = PA;
}

static void
guard_intact(long tag)
{
	unsigned o = (unsigned)sizeof(V90ConnectionEvaluator);
	unsigned n = CE_SLOT - o;

	diff_eq_int("ours stored past the object (%ld)",
		    memcmp(ce_a + o, ce_s + o, n) == 0, 1, tag);
	diff_eq_int("the blob stored past the object (%ld)",
		    memcmp(ce_b + o, ce_s + o, n) == 0, 1, tag);
	diff_eq_int("the parameter block was not written (%ld)",
		    memcmp(parm_a, parm_b, PARM_SLOT) == 0, 1, tag);
}

/* --------------------------------------------- the lifecycle, as before */

/* Every word `reset` writes.  +0x00, +0x88, +0x98 and +0xb8 are not in it. */
static const int ce_allow[] = {
	0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c,
	0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58,
	0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84,
	0x8c, 0x90, 0x94, 0x9c, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4
};
#define CE_NALLOW ((int)(sizeof(ce_allow) / sizeof(ce_allow[0])))

static int ce_seen[CE_NALLOW];

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

static void
ce_check_values(V90ConnectionEvaluator *o, V90Parameters *p, long tag)
{
	static const unsigned int zero = 0;

	diff_eq_int("enableRrnDown (%ld)", o->enableRrnDown,
		    p->ENABLE_RRN_DOWN, tag);
	diff_eq_int("enableRrnUp (%ld)", o->enableRrnUp, p->ENABLE_RRN_UP,
		    tag);
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
		    o->retrainCounterFadeCount, p->RETRAIN_COUNTER_FADE_COUNT,
		    tag);
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

	diff_eq_int("word_64 (%ld)", (long)o->word_64, 1600, tag);
	diff_eq_int("word_68 (%ld)", (long)o->word_68, 1600, tag);
	diff_eq_int("word_8c (%ld)", (long)o->word_8c, -1, tag);
	diff_eq_int("short_9c (%ld)", (long)o->short_9c, -1, tag);
	diff_eq_int("curDmin (%ld)", (long)o->curDmin, 0, tag);
	diff_eq_int("short_b0 (%ld)", (long)o->short_b0, 1, tag);
	diff_eq_int("short_b2 (%ld)", (long)o->short_b2, 0, tag);
	diff_eq_int("short_b4 (%ld)", (long)o->short_b4, 0, tag);
	diff_eq_int("nofV90Retrains (%ld)", (long)o->nofV90Retrains, 0, tag);
	diff_eq_int("nofRemoteRateReneg (%ld)", (long)o->nofRemoteRateReneg, 0,
		    tag);
	diff_eq_int("nofRemoteRetrains (%ld)", (long)o->nofRemoteRetrains, 0,
		    tag);
	diff_eq_int("word_24 (%ld)", (long)o->word_24, 0, tag);

	/* The three thresholds are floats now: the bit pattern, not the value. */
	diff_eq_int("threshUp (%ld)", memcmp(&o->threshUp, &zero, 4) == 0, 1,
		    tag);
	diff_eq_int("threshDown (%ld)", memcmp(&o->threshDown, &zero, 4) == 0,
		    1, tag);
	diff_eq_int("threshRetrain (%ld)",
		    memcmp(&o->threshRetrain, &zero, 4) == 0, 1, tag);
	diff_eq_int("word_70 (%ld)", memcmp(&o->word_70, &zero, 4) == 0, 1,
		    tag);
}

static int
run_ce_reset(void)
{
	unsigned char first[CE_SLOT];
	int trial, varied = 0, printed = 0;
	unsigned lvl;

	diff_begin("V90ConnectionEvaluator::reset");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 24; trial++) {
			unsigned char before[CE_SLOT];
			long tag = (long)lvl * 1000 + trial;
			int bad, firstbad;

			seed_pair(trial, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 31,
				  trial & 3);
			memcpy(before, ce_b, CE_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			CEA->reset();
			ref_ce_reset(ce_b);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after reset", V90ConnectionEvaluator,
				    CEA, CEB, tag);
			guard_intact(tag);
			ce_check_values(CEB, PA, tag);

			bad = only_wrote(before, ce_b, CE_SLOT, ce_allow,
					 CE_NALLOW, ce_seen, &firstbad);
			diff_eq_int("the blob wrote a word reset does not "
				    "name, at +0x%lx",
				    bad == 0 ? -1 : firstbad, -1, tag);

			transcript_matches(tag);
			if (lvl > 1) {
				diff_eq_int("above the gate the blob printed "
					    "(%ld)",
					    dsplib_debug_capture_lines(1) > 0,
					    1, tag);
				printed = 1;
				transcripts_seen = 1;
			}

			if (trial == 0 && lvl == 0)
				memcpy(first, ce_b, CE_SLOT);
			else if (memcmp(first, ce_b, CE_SLOT) != 0)
				varied = 1;
		}
	}

	set_level(0);
	diff_eq_int("the diagnostics were reached", printed, 1, 0);
	diff_eq_int("the object varied between trials", varied, 1, 0);

	{
		int i;

		for (i = 0; i < CE_NALLOW; i++)
			diff_eq_int("+0x%lx is a word reset writes",
				    ce_seen[i], 1, ce_allow[i]);
	}

	return diff_end();
}

typedef void (*ctor)(void *, void *);
typedef void (*dtor)(void *);

static int
run_ce_lifecycle(void)
{
	static const ctor ours[2] = { our_ce_ctor, our_ce_ctor2 };
	static const ctor theirs[2] = { ref_ce_ctor, ref_ce_ctor2 };
	static const dtor ourd[2] = { our_ce_dtor, our_ce_dtor2 };
	static const dtor theird[2] = { ref_ce_dtor, ref_ce_dtor2 };
	int trial;

	diff_begin("V90ConnectionEvaluator: constructor and destructor");
	set_level(0);

	for (trial = 0; trial < 16; trial++) {
		unsigned char before[CE_SLOT];
		long tag = 4000 + trial;
		int v = trial & 1;

		seed_pair(trial + 500, trial & 3);
		memcpy(before, ce_b, CE_SLOT);
		fill_pair(parm_a, parm_b, PARM_SLOT, trial + 501, trial & 3);

		ours[v](ce_a, parm_a);
		theirs[v](ce_b, parm_a);

		diff_eq_obj("after construction", V90ConnectionEvaluator,
			    CEA, CEB, tag);
		diff_eq_int("the constructor stored the parameter block (%ld)",
			    CEB->params == PA, 1, tag);
		/*
		 * Against the SEED and not against `before`: `seed_pair` puts
		 * the parameter pointer in +0x00 itself, so the pre-call image
		 * already holds it and the comparison would be vacuous.
		 * `ce_s` is the fill as it was before that.
		 */
		diff_eq_int("and the fill is gone from +0x00 (%ld)",
			    memcmp(ce_s, ce_b, 4) != 0, 1, tag);
		ce_check_values(CEB, PA, tag);
		guard_intact(tag);

		/* The destructor: one byte, and it must write nothing. */
		memcpy(before, ce_b, CE_SLOT);
		ourd[v](ce_a);
		theird[v](ce_b);
		diff_eq_int("the destructor wrote nothing (%ld)",
			    memcmp(before, ce_b, CE_SLOT) == 0, 1, tag);
		diff_eq_obj("after destruction", V90ConnectionEvaluator,
			    CEA, CEB, tag);

		/*
		 * AND AGAIN OVER STORAGE NO CONSTRUCTOR HAS TOUCHED.  On a
		 * constructed object a destructor that cleared a field the
		 * constructor already zeroed would be invisible; over a seeded
		 * one it is a four-byte difference.  `-fno-lifetime-dse` is in
		 * CXXFLAGS, so the store is not deleted and the difference is
		 * real (finding 1224).  Both sides compare against the seed,
		 * because a mutation lands on OURS and `ce_b` would not move.
		 */
		{
			unsigned char seeded[CE_SLOT];

			seed_pair(trial + 600, (trial + 1) & 3);
			memcpy(seeded, ce_b, CE_SLOT);
			ourd[v](ce_a);
			theird[v](ce_b);
			diff_eq_int("over seeded storage ours wrote nothing "
				    "(%ld)",
				    memcmp(seeded, ce_a, CE_SLOT) == 0, 1,
				    tag);
			diff_eq_int("over seeded storage the blob wrote "
				    "nothing (%ld)",
				    memcmp(seeded, ce_b, CE_SLOT) == 0, 1,
				    tag);
			diff_eq_obj("after destroying seeded storage",
				    V90ConnectionEvaluator, CEA, CEB, tag);
		}
	}

	return diff_end();
}

/* ------------------------------------------ updateAvePdsnr (113 bytes) */

/*
 * The float values fed in.  Deliberately spanning zero, both signs, a
 * denormal and a value big enough that the running sum leaves the range where
 * a float and an 80-bit accumulator would agree if anything rounded early.
 */
static const unsigned int pdsnr_bits[] = {
	0x00000000u,	/* +0.0f      */
	0x80000000u,	/* -0.0f      */
	0x3f800000u,	/*  1.0f      */
	0xbf800000u,	/* -1.0f      */
	0x41200000u,	/* 10.0f      */
	0x42c80000u,	/* 100.0f     */
	0x4b7fffffu,	/* 16777215.0 */
	0x00000001u,	/* denormal   */
	0x3dcccccdu,	/* 0.1f       */
	0xc61c4000u,	/* -10000.0f  */
	0x7f7fffffu,	/* FLT_MAX    */
	0x39a2b3c4u
};
#define NPDSNR ((unsigned)(sizeof(pdsnr_bits) / sizeof(pdsnr_bits[0])))

static float
as_float(unsigned int bits)
{
	float f;

	memcpy(&f, &bits, 4);
	return f;
}

static int
run_ce_avepdsnr(void)
{
	/*
	 * The counts.  The last three are what make the zero-extension
	 * visible: a total above 2^31 converts one way as `unsigned` and
	 * another as `int`, and everything below it converts identically.
	 */
	static const unsigned int starts[] = {
		0u, 1u, 7u, 1000u, 0x7ffffff0u, 0x80000000u, 0xfffff000u
	};
	unsigned s;
	int firstarm = 0, secondarm = 0, huge_total = 0, varied = 0;
	unsigned int firstavg = 0;

	diff_begin("V90ConnectionEvaluator::updateAvePdsnr");
	set_level(0);

	for (s = 0; s < sizeof(starts) / sizeof(starts[0]); s++) {
		int block;

		seed_pair((int)s + 700, (int)s & 3);
		fill_pair(parm_a, parm_b, PARM_SLOT, (int)s + 701, (int)s & 3);

		/*
		 * The count starts where the sweep says and the average keeps
		 * whatever the seed left in it -- NOT zero, so a first call
		 * that failed to overwrite it would show.
		 */
		CEA->word_74 = starts[s];
		CEB->word_74 = starts[s];
		if (starts[s] == 0)
			firstarm = 1;
		else
			secondarm = 1;

		/*
		 * TWO HUNDRED BLOCKS, COMPARED AFTER EVERY ONE.  The average
		 * is a running one; a divergence that later calls wash out is
		 * still a divergence.
		 */
		for (block = 0; block < 200; block++) {
			long tag = (long)s * 1000 + block;
			float p = as_float(pdsnr_bits[
			    ((unsigned)block + s) % NPDSNR]);
			unsigned int n = 1u + ((unsigned)block * 7919u
					       % 100000u);
			unsigned int before74 = CEB->word_74;

			CEA->updateAvePdsnr(p, n);
			ref_ce_updateAvePdsnr(ce_b, p, n);

			diff_eq_obj("after updateAvePdsnr",
				    V90ConnectionEvaluator, CEA, CEB, tag);
			guard_intact(tag);

			diff_eq_int("the count advanced by n (%ld)",
				    (long)(unsigned int)(CEB->word_74
							 - before74),
				    (long)n, tag);
			if (CEB->word_74 >= 0x80000000u)
				huge_total = 1;

			if (s == 0 && block == 0)
				memcpy(&firstavg, &CEB->word_70, 4);
			else if (memcmp(&firstavg, &CEB->word_70, 4) != 0)
				varied = 1;
		}

		/*
		 * THE FIRST CALL TAKES THE OTHER ARM.  Reset the count to zero
		 * and check the average is the sample itself, bit for bit --
		 * which is what distinguishes "assign" from "average with a
		 * weight of zero", a difference no accumulating run can see.
		 */
		{
			float p = as_float(pdsnr_bits[s % NPDSNR]);
			long tag = (long)s * 1000 + 900;
			unsigned int got, want;

			CEA->word_74 = 0;
			CEB->word_74 = 0;
			CEA->updateAvePdsnr(p, 4242u);
			ref_ce_updateAvePdsnr(ce_b, p, 4242u);

			diff_eq_obj("after the first call",
				    V90ConnectionEvaluator, CEA, CEB, tag);
			memcpy(&got, &CEB->word_70, 4);
			memcpy(&want, &p, 4);
			diff_eq_int("the average is the sample (%ld)",
				    got == want, 1, tag);
			diff_eq_int("the count is n (%ld)",
				    (long)CEB->word_74, 4242, tag);
			firstarm = 1;
		}
	}

	diff_eq_int("the count-is-zero arm was taken", firstarm, 1, 0);
	diff_eq_int("the accumulate arm was taken", secondarm, 1, 0);
	diff_eq_int("a total above 2^31 was reached", huge_total, 1, 0);
	diff_eq_int("the average varied between calls", varied, 1, 0);
	return diff_end();
}

/* ------------------------ updateCurrentConstellationData (147 bytes) */

static int
run_ce_constdata(void)
{
	static const short dmins[] = {
		0, 1, -1, 100, -100, 32767, (short)-32768, 4096
	};
	static const unsigned int thr[] = {
		0x00000000u, 0x3f800000u, 0xbf800000u, 0x41200000u,
		0x42c80000u, 0xc2c80000u, 0x3dcccccdu, 0x4b7fffffu,
		0x7f7fffffu, 0xff7fffffu, 0x00000001u, 0x39a2b3c4u
	};
	unsigned lvl, nthr = sizeof(thr) / sizeof(thr[0]);
	unsigned ndm = sizeof(dmins) / sizeof(dmins[0]);
	int trial, printed = 0, silent = 0, varied = 0, negative_dmin = 0;
	short firstdmin = 0;

	diff_begin("V90ConnectionEvaluator::updateCurrentConstellationData");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 24; trial++) {
			long tag = (long)lvl * 1000 + trial;
			short d = dmins[(unsigned)trial % ndm];
			float up = as_float(thr[(unsigned)trial % nthr]);
			float dn = as_float(thr[((unsigned)trial + 4) % nthr]);
			float rt = as_float(thr[((unsigned)trial + 8) % nthr]);
			unsigned char before[CE_SLOT];
			unsigned int got, want;

			seed_pair(trial + 800, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 801,
				  trial & 3);
			memcpy(before, ce_b, CE_SLOT);
			if (d < 0)
				negative_dmin = 1;

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			CEA->updateCurrentConstellationData(d, up, dn, rt);
			ref_ce_updateConst(ce_b, d, up, dn, rt);

			dsplib_debug_capture_on = 0;

			diff_eq_obj("after updateCurrentConstellationData",
				    V90ConnectionEvaluator, CEA, CEB, tag);
			guard_intact(tag);

			/*
			 * WHICH ARGUMENT LANDED WHERE, read out of the BLOB's
			 * object.  Three floats in a row and a comparison of
			 * the two sides would agree if all three were stored
			 * in the wrong order on both.
			 */
			memcpy(&got, &CEB->threshUp, 4);
			memcpy(&want, &up, 4);
			diff_eq_int("threshUp is the first float (%ld)",
				    got == want, 1, tag);
			memcpy(&got, &CEB->threshDown, 4);
			memcpy(&want, &dn, 4);
			diff_eq_int("threshDown is the second float (%ld)",
				    got == want, 1, tag);
			memcpy(&got, &CEB->threshRetrain, 4);
			memcpy(&want, &rt, 4);
			diff_eq_int("threshRetrain is the third float (%ld)",
				    got == want, 1, tag);
			diff_eq_int("curDmin (%ld)", (long)CEB->curDmin,
				    (long)d, tag);
			diff_eq_int("word_90 was cleared (%ld)",
				    (long)CEB->word_90, 0, tag);
			diff_eq_int("it changed the object (%ld)",
				    memcmp(before, ce_b,
					   sizeof(V90ConnectionEvaluator)) != 0,
				    1, tag);

			transcript_matches(tag);
			if (lvl > 1) {
				diff_eq_int("above the gate the blob printed "
					    "one line (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    1, tag);
				printed = 1;
				transcripts_seen = 1;
			} else {
				diff_eq_int("below the gate nothing printed "
					    "(%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    0, tag);
				silent = 1;
			}

			if (trial == 0 && lvl == 0)
				firstdmin = CEB->curDmin;
			else if (CEB->curDmin != firstdmin)
				varied = 1;
		}
	}

	set_level(0);
	diff_eq_int("the diagnostic was reached", printed, 1, 0);
	diff_eq_int("and was silent below the gate", silent, 1, 0);
	diff_eq_int("a negative curDmin was tried", negative_dmin, 1, 0);
	diff_eq_int("curDmin varied between trials", varied, 1, 0);
	return diff_end();
}

/* ------------------------------ indicateRemoteRateReneg (42 bytes) */

static int
run_ce_rrn(void)
{
	static const unsigned int starts[] = {
		0u, 1u, 0x7ffffffeu, 0xfffffffeu
	};
	unsigned lvl, s;
	int printed = 0, wrapped = 0;

	diff_begin("V90ConnectionEvaluator::indicateRemoteRateReneg");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level(lvl);
		for (s = 0; s < sizeof(starts) / sizeof(starts[0]); s++) {
			int call;

			seed_pair((int)s + 900, (int)s & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, (int)s + 901,
				  (int)s & 3);
			CEA->nofRemoteRateReneg = starts[s];
			CEB->nofRemoteRateReneg = starts[s];

			for (call = 0; call < 8; call++) {
				long tag = (long)lvl * 10000 + (long)s * 100
					   + call;
				unsigned int before = CEB->nofRemoteRateReneg;

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();

				CEA->indicateRemoteRateReneg();
				ref_ce_remoteRateReneg(ce_b);

				dsplib_debug_capture_on = 0;

				diff_eq_obj("after indicateRemoteRateReneg",
					    V90ConnectionEvaluator, CEA, CEB,
					    tag);
				guard_intact(tag);
				diff_eq_int("the counter advanced by one "
					    "(%ld)",
					    (long)(unsigned int)
					    (CEB->nofRemoteRateReneg - before),
					    1, tag);
				diff_eq_int("word_90 was cleared (%ld)",
					    (long)CEB->word_90, 0, tag);
				if (CEB->nofRemoteRateReneg < before)
					wrapped = 1;

				transcript_matches(tag);
				if (lvl > 1) {
					diff_eq_int("the blob printed one "
						    "line (%ld)",
						    (int)
						    dsplib_debug_capture_lines(1),
						    1, tag);
					printed = 1;
					transcripts_seen = 1;
				}
			}
		}
	}

	set_level(0);
	diff_eq_int("the diagnostic was reached", printed, 1, 0);
	diff_eq_int("the counter was driven through its wrap", wrapped, 1, 0);
	return diff_end();
}

/* ------------------- indicateLocalRetrain / indicateRemoteRetrain */

/*
 * THE LIMITS, and the two that decide the signedness.
 *
 *   limit    start        unsigned says            signed would say
 *   ------------------------------------------------------------------
 *    0       0            over at once (5)         over at once (5)
 *    3       0            over on the fourth (5)   the same
 *   -1       0            NEVER over (4)           over at once (5)
 *   0x7fff-  0x7fffffff   over at once (5)         NEVER over (4)
 *   fffe
 *
 * The last two are the whole reason this block exists: `ja` and `jg` agree on
 * every ordinary pair and disagree on exactly those.
 */
struct limit_case {
	int limit;
	unsigned int start;
	int calls;
};

static const struct limit_case limits[] = {
	{ 0,		0u,		4 },
	{ 3,		0u,		9 },
	{ 1,		0u,		5 },
	{ -1,		0u,		4 },
	{ -1000,	0u,		4 },
	{ 0x7ffffffe,	0x7fffffffu,	3 },
	{ 0x7fffffff,	0xfffffff0u,	4 },
	{ 100,		95u,		12 }
};
#define NLIMITS ((int)(sizeof(limits) / sizeof(limits[0])))

/* Which verdicts were seen, indexed by verdict - 4. */
static int local_verdict[2];
static int remote_verdict[2];

static int
run_ce_retrain(int remote)
{
	int c, lvl, printed = 0;
	int *seen = remote ? remote_verdict : local_verdict;

	diff_begin(remote ? "V90ConnectionEvaluator::indicateRemoteRetrain"
			  : "V90ConnectionEvaluator::indicateLocalRetrain");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level((unsigned)lvl);
		for (c = 0; c < NLIMITS; c++) {
			int call;

			seed_pair(c + 1100 + remote * 50, c & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, c + 1101,
				  c & 3);
			/*
			 * THE TWO LIMITS ARE NEVER THE SAME NUMBER.  The one
			 * this member does not read gets the complement, so a
			 * reconstruction that read the other slot would take
			 * the opposite branch -- with both set alike, "it read
			 * +0x460 and not +0x464" is not a claim any test can
			 * make.
			 *
			 * BOTH COPIES of the block, because the shadow is what
			 * the "nobody wrote the parameters" check compares
			 * against; only `parm_a` is ever read by the object.
			 */
			{
				V90Parameters *pb = (V90Parameters *)parm_b;
				int mine = limits[c].limit;
				int other = ~limits[c].limit;

				if (remote) {
					PA->MAX_NOF_REMOTE_RETRAINS = mine;
					PA->MAX_NOF_V90_RETRAINS = other;
					pb->MAX_NOF_REMOTE_RETRAINS = mine;
					pb->MAX_NOF_V90_RETRAINS = other;
				} else {
					PA->MAX_NOF_V90_RETRAINS = mine;
					PA->MAX_NOF_REMOTE_RETRAINS = other;
					pb->MAX_NOF_V90_RETRAINS = mine;
					pb->MAX_NOF_REMOTE_RETRAINS = other;
				}
			}
			if (remote) {
				CEA->nofRemoteRetrains = limits[c].start;
				CEB->nofRemoteRetrains = limits[c].start;
			} else {
				CEA->nofV90Retrains = limits[c].start;
				CEB->nofV90Retrains = limits[c].start;
			}

			for (call = 0; call < limits[c].calls; call++) {
				long tag = (long)lvl * 10000 + (long)c * 100
					   + call;
				int va, vb;

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();

				if (remote) {
					va = CEA->indicateRemoteRetrain();
					vb = ref_ce_remoteRetrain(ce_b);
				} else {
					va = CEA->indicateLocalRetrain();
					vb = ref_ce_localRetrain(ce_b);
				}

				dsplib_debug_capture_on = 0;

				diff_eq_obj("after the indication",
					    V90ConnectionEvaluator, CEA, CEB,
					    tag);
				guard_intact(tag);
				diff_eq_int("the verdict matches (%ld)", va,
					    vb, tag);
				diff_eq_int("the verdict is 4 or 5 (%ld)",
					    vb == 4 || vb == 5, 1, tag);
				diff_eq_int("word_90 was cleared (%ld)",
					    (long)CEB->word_90, 0, tag);
				diff_eq_int("word_18 was cleared (%ld)",
					    (long)CEB->word_18, 0, tag);
				diff_eq_int("word_1c was cleared (%ld)",
					    (long)CEB->word_1c, 0, tag);
				if (vb == 4 || vb == 5)
					seen[vb - 4] = 1;

				/*
				 * THE COUNTER RESTARTS ONLY WHEN IT GIVES UP.
				 * That asymmetry is the one thing a test that
				 * only looked at the verdict would miss.
				 */
				{
					unsigned int n = remote
					    ? CEB->nofRemoteRetrains
					    : CEB->nofV90Retrains;

					if (vb == V90CE_VERDICT_FALLBACK_V34)
						diff_eq_int("the counter "
							    "restarted (%ld)",
							    (long)n, 0, tag);
					else
						diff_eq_int("the counter kept "
							    "counting (%ld)",
							    n != 0, 1, tag);
				}

				transcript_matches(tag);
				if (lvl > 1
				    && vb == V90CE_VERDICT_FALLBACK_V34) {
					diff_eq_int("giving up printed one "
						    "line (%ld)",
						    (int)
						    dsplib_debug_capture_lines(1),
						    1, tag);
					printed = 1;
					transcripts_seen = 1;
				}
				if (lvl > 1 && vb == V90CE_VERDICT_RETRAIN)
					diff_eq_int("carrying on printed "
						    "nothing (%ld)",
						    (int)
						    dsplib_debug_capture_lines(1),
						    0, tag);
			}
		}
	}

	set_level(0);
	diff_eq_int("the fall-back diagnostic was reached", printed, 1, 0);
	return diff_end();
}

/* ---------------------- evaluateMeanErrorStdPhase4 (3 bytes) */

static int
run_ce_meanerr4(void)
{
	unsigned n = NPDSNR;
	int trial;

	diff_begin("V90ConnectionEvaluator::evaluateMeanErrorStdPhase4");
	set_level(2);

	for (trial = 0; trial < 24; trial++) {
		unsigned char before_a[CE_SLOT], before_b[CE_SLOT];
		long tag = 5000 + trial;
		float x, y;
		int va, vb;

		x = as_float(pdsnr_bits[(unsigned)trial % n]);
		y = as_float(pdsnr_bits[((unsigned)trial + 5) % n]);

		seed_pair(trial + 1200, trial & 3);
		fill_pair(parm_a, parm_b, PARM_SLOT, trial + 1201, trial & 3);
		memcpy(before_a, ce_a, CE_SLOT);
		memcpy(before_b, ce_b, CE_SLOT);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();

		va = CEA->evaluateMeanErrorStdPhase4(x, y);
		vb = ref_ce_meanErrPhase4(ce_b, x, y);

		dsplib_debug_capture_on = 0;

		diff_eq_int("the answer matches (%ld)", va, vb, tag);
		diff_eq_int("the answer is zero (%ld)", vb, 0, tag);
		diff_eq_int("ours wrote nothing (%ld)",
			    memcmp(before_a, ce_a, CE_SLOT) == 0, 1, tag);
		diff_eq_int("the blob wrote nothing (%ld)",
			    memcmp(before_b, ce_b, CE_SLOT) == 0, 1, tag);
		diff_eq_int("and printed nothing (%ld)",
			    (int)dsplib_debug_capture_lines(1), 0, tag);
	}

	set_level(0);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_ce_reset();
	rc |= run_ce_lifecycle();
	rc |= run_ce_avepdsnr();
	rc |= run_ce_constdata();
	rc |= run_ce_rrn();
	rc |= run_ce_retrain(0);
	rc |= run_ce_retrain(1);
	rc |= run_ce_meanerr4();

	set_level(0);
	dsplib_debug_capture_on = 0;

	/*
	 * THE DECISION WAS MADE BOTH WAYS.  An evaluator that always returns
	 * the same verdict agrees with the blob on every trial and proves
	 * nothing (findings 149, 223), so the two outcomes of each of the two
	 * deciding members are asserted here to have been OBSERVED -- and
	 * asserted to be different numbers, since "both seen" is satisfied
	 * trivially if 4 and 5 are the same constant.
	 */
	diff_begin("every verdict was observed");
	diff_eq_int("indicateLocalRetrain answered 4", local_verdict[0], 1, 0);
	diff_eq_int("indicateLocalRetrain answered 5", local_verdict[1], 1, 0);
	diff_eq_int("indicateRemoteRetrain answered 4", remote_verdict[0], 1,
		    0);
	diff_eq_int("indicateRemoteRetrain answered 5", remote_verdict[1], 1,
		    0);
	diff_eq_int("and the two verdicts are different numbers",
		    V90CE_VERDICT_RETRAIN != V90CE_VERDICT_FALLBACK_V34, 1, 0);
	diff_eq_int("some transcript was captured", transcripts_seen, 1, 0);
	rc |= diff_end();

	return rc;
}
