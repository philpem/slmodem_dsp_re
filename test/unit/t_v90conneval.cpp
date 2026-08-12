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

/* ------------------- evaluatePhase3 (980 B) / evaluatePhase4 (1353 B) */

/*
 * WHAT THESE TWO NEED THAT THE SMALL MEMBERS DID NOT
 *
 *   THEY ACCUMULATE.  +0x10 and +0x18 are durations in symbols that grow by
 *   `word_74` per call, and both evaluators CONSUME the average on the way out
 *   -- +0x74 and +0x70 are zeroed on every path that got past the entry test.
 *   So a block below is "one measurement": set the count and the average, call,
 *   compare.  Sixty of them in a run, compared after EVERY one, because a
 *   divergence at the fortieth that the sixtieth washes out is still a defect.
 *
 *   THE CLEAR IS NOT ON THE ACCUMULATING PATH.  0x3f797 and 0x3fd12 jump PAST
 *   the store that zeroes +0x18, so a reconstruction that cleared it every call
 *   would still agree with the blob on any single-call trial and never
 *   accumulate.  The runs below are the only thing that can see it.
 *
 *   EVERY SLOT THEY READ HAS A DIFFERENT VALUE.  `p34_params` and the setup in
 *   each block give distinct values to +0x64 against +0x68, to the +0x60 COPY
 *   of `RETRAIN_DETECT_DURATION` against the parameter itself, to
 *   `MAX_NOF_V90_RETRAINS` against `MAX_NOF_REMOTE_RETRAINS` and against
 *   `unnamed_45c`, and to all six float thresholds -- otherwise "it read +0x64
 *   and not +0x68" is not a claim any test makes.  The `EIA6_*` thresholds the
 *   task predicted are set to +FLT_MAX: neither function reads one, and a run
 *   that read one instead would never cross a threshold at all.
 *
 *   THEY DECIDE THREE WAYS, not two.  0, 4 and 5 are all reachable and the
 *   coverage flags at the bottom of `main` assert every one was observed for
 *   both functions, plus the paths that only exist in one of them.
 *
 *   ONE TRANSCRIPT CANNOT BE COMPARED, and it is the object's fault rather than
 *   the harness's.  0x3ffcf calls `edprintf` with a format that has a `%d` and
 *   stores nothing to 0x4(%esp), so `vsnprintf` formats whatever the outgoing
 *   argument slot held -- a different frame on each side.  That one path
 *   compares state, verdict and LINE COUNT and not the text.  Finding 1388.
 *
 *   NO NaN IS FED AS `evaluatePhase4`'s ARGUMENT.  Its comparison is the one
 *   place in either function where the parameter is the LEFT operand
 *   (`flds 0x438(%ecx); fcomp %st(1); jae`), and `jae` is false when the
 *   compare is unordered, so the blob RUNS the arm for a NaN where C says it
 *   must not.  That is GCC 3.4.2's choice of complement, which GCC 13 does not
 *   repeat, so a NaN argument would disagree between `make period` and `make
 *   phase` for a reason that is not in our source.  NaN IS fed as the average:
 *   every +0x70 comparison is `flds; fcoms; ja/jbe`, which is unordered-correct
 *   on both compilers, and it exercises the else arms.
 */

extern "C" {
int ref_ce_phase3(void *) asm("ref__ZN22V90ConnectionEvaluator14evaluatePhase3Ev");
int ref_ce_phase4(void *, float)
	asm("ref__ZN22V90ConnectionEvaluator14evaluatePhase4Ef");
}

#define PB	((V90Parameters *)parm_b)

#define SET_P(f, v)	do { PA->f = (v); PB->f = (v); } while (0)
#define SET_PF(f, bits)	do { unsigned int b_ = (bits); \
			     memcpy(&PA->f, &b_, 4); \
			     memcpy(&PB->f, &b_, 4); } while (0)
#define SET_CE(f, v)	do { CEA->f = (v); CEB->f = (v); } while (0)
#define SET_CEF(f, bits) do { unsigned int b_ = (bits); \
			      memcpy(&CEA->f, &b_, 4); \
			      memcpy(&CEB->f, &b_, 4); } while (0)

/* 0 / 4 / 5, indexed 0 / 1 / 2. */
static int p3_verdict[3];
static int p4_verdict[3];

/* The paths each function has that the verdict alone does not distinguish. */
static int p3_altrbs, p3_trn1d, p3_large, p3_retrain, p3_none, p3_empty;
static int p3_five_then_four, p3_cleared_10, p3_cleared_18;
static int p4_mean_arm, p4_mean_skipped, p4_large, p4_retrain, p4_none;
static int p4_empty, p4_delayed, p4_delayed_over, p4_thresh_replaced;
static int p4_cleared_10, p4_cleared_18, p4_guard_b0, p4_guard_ratio;
static int p4_guard_count, p4_delayed_half, p4_missing_arg;
static int p3_unsigned_dur, p3_unsigned_max;
static int p4_unsigned_dur, p4_unsigned_max;
static int p4_unsigned_45c, p4_unsigned_delayed;

static const unsigned int fzero = 0u;

/*
 * The values every slot either evaluator reads, all different from each other
 * and from the ones they must not read.
 */
static void
p34_params(void)
{
	SET_PF(TRN1D_ERROR_FOR_V34_FALLBACK,		0x41200000u); /* 10 */
	SET_PF(PHASE3_ERROR_FOR_V34_FALLBACK,		0x41300000u); /* 11 */
	SET_PF(PDSNR_THRESHOLD_IN_PHASE3,		0x41400000u); /* 12 */
	SET_PF(PDSNR_THRESHOLD_IN_PHASE4,		0x41500000u); /* 13 */
	SET_PF(PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH,
							0x41600000u); /* 14 */
	SET_PF(unnamed_434,				0x437a0000u); /* 250 */

	/*
	 * The slots neither function reads.  +FLT_MAX, so a comparison that
	 * picked one of these up would never fire and every accumulating run
	 * below would answer 0 instead of 4 or 5.
	 */
	SET_PF(PHASE4_ERROR_FOR_V34_FALLBACK,		0x7f7fffffu);
	SET_PF(QC_PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH,
							0x7f7fffffu);
	SET_PF(EIA6_PDSNR_THRESHOLD_IN_PHASE3,		0x7f7fffffu);
	SET_PF(EIA6_PDSNR_THRESHOLD_IN_PHASE4,		0x7f7fffffu);
	SET_PF(EIA6_TRN1D_ERROR_FOR_V34_FALLBACK,	0x7f7fffffu);
	SET_PF(TRN1D_MAX_MEAN_ERROR_STD_IN_PHASE3,	0x7f7fffffu);
	SET_PF(TRN2D_MAX_MEAN_ERROR_STD_IN_PHASE4,	0x7f7fffffu);

	/*
	 * The parameter `RETRAIN_DETECT_DURATION`, which is NOT what either
	 * evaluator reads -- both read the +0x60 copy.  One symbol, so a
	 * reconstruction reading the parameter would trip on the first call of
	 * every run.
	 */
	SET_P(RETRAIN_DETECT_DURATION, 1);
	SET_P(NOF_REMOTE_RATE_RENEG_BEFORE_RETRAIN, 1);
	SET_P(MAX_NOF_RATES_DIFF_BEFORE_RETRAIN, 1);
}

static int
p3_call(long tag, int cmp_text)
{
	int va, vb;

	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	va = CEA->evaluatePhase3();
	vb = ref_ce_phase3(ce_b);

	dsplib_debug_capture_on = 0;

	diff_eq_obj("after evaluatePhase3", V90ConnectionEvaluator, CEA, CEB,
		    tag);
	guard_intact(tag);
	diff_eq_int("the verdict matches (%ld)", va, vb, tag);
	diff_eq_int("the verdict is 0, 4 or 5 (%ld)",
		    vb == 0 || vb == 4 || vb == 5, 1, tag);
	if (cmp_text)
		transcript_matches(tag);
	else
		diff_eq_int("line counts match (%ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), tag);
	if (dsplib_debug_capture_lines(1) > 0)
		transcripts_seen = 1;

	p3_verdict[vb == 0 ? 0 : (vb == 4 ? 1 : 2)] = 1;
	return vb;
}

static int
p4_call(long tag, float arg, int cmp_text)
{
	int va, vb;

	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	va = CEA->evaluatePhase4(arg);
	vb = ref_ce_phase4(ce_b, arg);

	dsplib_debug_capture_on = 0;

	diff_eq_obj("after evaluatePhase4", V90ConnectionEvaluator, CEA, CEB,
		    tag);
	guard_intact(tag);
	diff_eq_int("the verdict matches (%ld)", va, vb, tag);
	diff_eq_int("the verdict is 0, 4 or 5 (%ld)",
		    vb == 0 || vb == 4 || vb == 5, 1, tag);
	if (cmp_text)
		transcript_matches(tag);
	else
		diff_eq_int("line counts match (%ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), tag);
	if (dsplib_debug_capture_lines(1) > 0)
		transcripts_seen = 1;

	p4_verdict[vb == 0 ? 0 : (vb == 4 ? 1 : 2)] = 1;
	return vb;
}

/* The average consumed at the end of every call: +0x74 and +0x70 both zero. */
static void
consumed(long tag)
{
	diff_eq_int("the count was consumed (%ld)", (long)CEB->word_74, 0, tag);
	diff_eq_int("the average was cleared (%ld)",
		    memcmp(&CEB->word_70, &fzero, 4) == 0, 1, tag);
}

/*
 * The averages fed in.  Spanning both signs, both zeros, a denormal, a
 * NaN -- which every `flds; fcoms; ja` reads as "not above", so it drives the
 * else arms -- and values chosen to sit above and below the thresholds
 * `p34_params` plants.
 */
static const unsigned int avg_bits[] = {
	0x41a00000u,	/*  20.0f  above all four phase thresholds */
	0x41200000u,	/*  10.0f  exactly TRN1D's, so NOT above it */
	0x41400000u,	/*  12.0f  exactly PDSNR_P3's                */
	0x41480000u,	/*  12.5f  above PDSNR_P3 and below PDSNR_P4 */
	0x00000000u,	/*  +0.0f                                    */
	0x80000000u,	/*  -0.0f                                    */
	0x00000001u,	/*  denormal                                 */
	0xc1a00000u,	/* -20.0f                                    */
	0x7fc00000u,	/*  NaN: every compare says "not above"      */
	0x7f7fffffu,	/*  FLT_MAX                                  */
	0x42fe0000u,	/* 127.0f                                    */
	0x3dcccccdu,	/*   0.1f                                    */
	0xbdcccccdu,	/*  -0.1f: a '-' sign with a zero magnitude  */
	0xc1480000u	/* -12.5f: a '-' sign and 500 decimals       */
};
#define NAVG ((unsigned)(sizeof(avg_bits) / sizeof(avg_bits[0])))

static int
run_ce_phase3(void)
{
	int lvl, trial, c;

	diff_begin("V90ConnectionEvaluator::evaluatePhase3");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned)lvl);

		/*
		 * A ZERO SYMBOL COUNT RETURNS AT ONCE AND WRITES NOTHING.  The
		 * average is left alone too, which is what makes the entry test
		 * be on the count and not on the average.
		 */
		for (trial = 0; trial < 4; trial++) {
			unsigned char before[CE_SLOT];
			long tag = (long)lvl * 100000 + trial;
			int vb;

			seed_pair(trial + 2000, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 2001,
				  trial & 3);
			p34_params();
			SET_CE(word_74, 0u);
			SET_CE(short_b2, (short)1);
			SET_CE(word_88, 1u);
			SET_CE(word_84, 1u);
			memcpy(before, ce_b, CE_SLOT);

			vb = p3_call(tag, 1);
			diff_eq_int("the empty call answered nothing (%ld)", vb,
				    0, tag);
			diff_eq_int("and wrote nothing (%ld)",
				    memcmp(before, ce_b, CE_SLOT) == 0, 1, tag);
			diff_eq_int("and printed nothing (%ld)",
				    (int)dsplib_debug_capture_lines(1), 0, tag);
			p3_empty = 1;
		}

		/*
		 * NO ARM ARMED: all three flags zero.  The verdict is 0 and the
		 * only thing that moved is the average.
		 */
		for (trial = 0; trial < 4; trial++) {
			unsigned char before[CE_SLOT];
			long tag = (long)lvl * 100000 + 100 + trial;
			int vb;

			seed_pair(trial + 2100, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 2101,
				  trial & 3);
			p34_params();
			SET_CE(word_74, 37u);
			SET_CEF(word_70, avg_bits[(unsigned)trial % NAVG]);
			SET_CE(short_b2, (short)0);
			SET_CE(word_88, 0u);
			SET_CE(word_84, 0u);
			memcpy(before, ce_b, CE_SLOT);

			vb = p3_call(tag, 1);
			diff_eq_int("no arm answered nothing (%ld)", vb, 0, tag);
			consumed(tag);
			diff_eq_int("and touched nothing else (%ld)",
				    memcmp(before, ce_b, 0x70) == 0, 1, tag);
			diff_eq_int("and printed nothing (%ld)",
				    (int)dsplib_debug_capture_lines(1), 0, tag);
			p3_none = 1;
		}

		/*
		 * THE altRbsDetectedOnQc ARM, over the same limit table the two
		 * `indicate*` members use -- including the two rows where `ja`
		 * and `jg` disagree.  +0x88 and +0x84 are BOTH set, so the arm
		 * also proves the chain is `else if` and not three `if`s.
		 */
		for (c = 0; c < NLIMITS; c++) {
			int call;

			seed_pair(c + 2200, c & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, c + 2201, c & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, limits[c].limit);
			SET_P(MAX_NOF_REMOTE_RETRAINS, ~limits[c].limit);
			SET_P(unnamed_45c, ~limits[c].limit);
			SET_CE(nofV90Retrains, limits[c].start);
			SET_CE(word_88, 1u);
			SET_CE(word_84, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 1u);	/* would trip at once */
			SET_CE(retrainDetectDuration, 1);

			for (call = 0; call < limits[c].calls; call++) {
				long tag = (long)lvl * 100000 + 200
					   + (long)c * 100 + call;
				unsigned int b10 = CEB->word_10;
				unsigned int b18 = CEB->word_18;
				int vb;

				SET_CE(word_74, 11u + (unsigned)call);
				SET_CEF(word_70,
					avg_bits[(unsigned)call % NAVG]);
				SET_CE(short_b2, (short)(1 + call));

				vb = p3_call(tag, 1);
				diff_eq_int("the arm decided (%ld)",
					    vb == 4 || vb == 5, 1, tag);
				diff_eq_int("altRbs was cleared (%ld)",
					    (long)CEB->short_b2, 0, tag);
				diff_eq_int("word_1c was cleared (%ld)",
					    (long)CEB->word_1c, 0, tag);
				diff_eq_int("word_90 was cleared (%ld)",
					    (long)CEB->word_90, 0, tag);
				diff_eq_int("the arm did not accumulate +0x10 "
					    "(%ld)",
					    (long)CEB->word_10, (long)b10, tag);
				diff_eq_int("the arm did not accumulate +0x18 "
					    "(%ld)",
					    (long)CEB->word_18, (long)b18, tag);
				consumed(tag);
				if (vb == V90CE_VERDICT_FALLBACK_V34)
					diff_eq_int("the counter restarted "
						    "(%ld)",
						    (long)CEB->nofV90Retrains, 0,
						    tag);
				else
					diff_eq_int("the counter kept counting "
						    "(%ld)",
						    CEB->nofV90Retrains != 0, 1,
						    tag);
				p3_altrbs = 1;
			}
		}

		/*
		 * THE end-of-TRN1d ARM, driven as a run.  +0x10 grows by the
		 * symbol count while the average is over
		 * `TRN1D_ERROR_FOR_V34_FALLBACK` and is cleared outright when it
		 * is not; the fall-back fires when it reaches +0x64.  +0x68 is
		 * given a DIFFERENT value, so reading phase 4's slot here would
		 * change the block the verdict lands on.
		 */
		{
			int block;
			unsigned int want10 = 0;

			seed_pair(2300 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 2301 + lvl,
				  lvl & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100000);
			SET_CE(nofV90Retrains, 3u);
			SET_CE(short_b2, (short)0);
			SET_CE(word_88, 1u);
			SET_CE(word_84, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 500u);
			SET_CE(word_68, 999u);
			SET_CE(retrainDetectDuration, 7);

			for (block = 0; block < 60; block++) {
				long tag = (long)lvl * 100000 + 3000 + block;
				unsigned int n = 40u + (unsigned)(block % 3);
				int above = (block % 20) != 19;
				int vb;

				SET_CE(word_74, n);
				SET_CEF(word_70, above ? 0x41a00000u
						       : 0x40000000u);
				vb = p3_call(tag, 1);

				if (above) {
					want10 += n;
					if (want10 >= 500u)
						diff_eq_int("the duration ran "
							    "out (%ld)", vb, 5,
							    tag);
					else
						diff_eq_int("still counting "
							    "(%ld)", vb, 0, tag);
				} else {
					want10 = 0;
					diff_eq_int("below the threshold "
						    "answered nothing (%ld)",
						    vb, 0, tag);
					p3_cleared_10 = 1;
				}
				diff_eq_int("+0x10 is the running duration "
					    "(%ld)",
					    (long)CEB->word_10, (long)want10,
					    tag);
				diff_eq_int("+0x18 never moved (%ld)",
					    (long)CEB->word_18, 0, tag);
				consumed(tag);
				if (vb == 5)
					p3_trn1d = 1;
			}
		}

		/*
		 * THE ORDINARY PHASE-3 ARM, and the one place where a single
		 * call takes two decisions.  The average is above BOTH
		 * `PHASE3_ERROR_FOR_V34_FALLBACK` and
		 * `PDSNR_THRESHOLD_IN_PHASE3`, so when +0x10 runs out the
		 * fall-back prints and sets 5, and the code then falls into the
		 * retrain test which sets 4 -- the call returns 4 having printed
		 * the fall-back message.
		 */
		{
			int block;
			unsigned int want10 = 0, want18 = 0;

			seed_pair(2400 + lvl, (lvl + 1) & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 2401 + lvl,
				  (lvl + 1) & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 3);
			SET_P(MAX_NOF_REMOTE_RETRAINS, -4);
			SET_P(unnamed_45c, -4);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b2, (short)0);
			SET_CE(word_88, 0u);
			SET_CE(word_84, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 300u);
			SET_CE(word_68, 999u);
			SET_CE(retrainDetectDuration, 120);

			for (block = 0; block < 60; block++) {
				long tag = (long)lvl * 100000 + 4000 + block;
				unsigned int n = 25u;
				int lines;
				int vb;

				SET_CE(word_74, n);
				SET_CEF(word_70, 0x41a00000u);	/* 20.0f */
				vb = p3_call(tag, 1);
				lines = (int)dsplib_debug_capture_lines(1);

				want10 += n;
				want18 += n;
				if (want18 >= 120u)
					want18 = 0;
				diff_eq_int("+0x10 accumulated (%ld)",
					    (long)CEB->word_10, (long)want10,
					    tag);
				diff_eq_int("+0x18 accumulated (%ld)",
					    (long)CEB->word_18, (long)want18,
					    tag);
				consumed(tag);
				/*
				 * TWO DECISIONS IN ONE CALL.  Two lines out and
				 * a verdict of 4 can only mean the fall-back
				 * message was printed with %esi = 5 and then 4
				 * was written over it at 0x3f93f -- no other
				 * path in the function prints twice.
				 */
				if (lvl > 1 && vb == 4 && lines == 2)
					p3_five_then_four = 1;
				if (vb == 4)
					p3_retrain = 1;
				if (vb == 5)
					p3_large = 1;
			}
		}

		/*
		 * THE SAME ARM WITH THE AVERAGE BELOW THE RETRAIN THRESHOLD, so
		 * +0x18 is cleared rather than accumulated and the fall-back
		 * half runs on its own.
		 */
		for (trial = 0; trial < (int)NAVG; trial++) {
			long tag = (long)lvl * 100000 + 5000 + trial;
			int vb;

			seed_pair(trial + 2500, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 2501,
				  trial & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100000);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b2, (short)0);
			SET_CE(word_88, 0u);
			SET_CE(word_84, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 4444u);
			SET_CE(word_64, 100000u);
			SET_CE(retrainDetectDuration, 3);
			SET_CE(word_74, 17u);
			SET_CEF(word_70, avg_bits[(unsigned)trial % NAVG]);

			vb = p3_call(tag, 1);
			if (avg_bits[(unsigned)trial % NAVG] == 0x41a00000u
			    || avg_bits[(unsigned)trial % NAVG] == 0x41480000u
			    || avg_bits[(unsigned)trial % NAVG] == 0x7f7fffffu
			    || avg_bits[(unsigned)trial % NAVG] == 0x42fe0000u)
				diff_eq_int("above the retrain threshold "
					    "(%ld)", vb, 4, tag);
			else {
				diff_eq_int("not above it (%ld)", vb, 0, tag);
				diff_eq_int("+0x18 was cleared (%ld)",
					    (long)CEB->word_18, 0, tag);
				p3_cleared_18 = 1;
			}
			consumed(tag);
		}

		/*
		 * THE PRINTED NUMBER, over the whole range of averages.  The
		 * threshold is put at -FLT_MAX and +0x64 at one symbol so that
		 * EVERY average reaches the diagnostic -- including both zeros,
		 * which the object prints with a '-' because its sign test is
		 * `0.0f < v` and not `0.0f <= v`, and the negatives, where the
		 * magnitude and the three decimals are both taken through an
		 * absolute value.  A NaN cannot get here: `flds; fcoms; ja` is
		 * false for it, which is itself worth driving.
		 */
		for (trial = 0; trial < (int)NAVG; trial++) {
			long tag = (long)lvl * 100000 + 8000 + trial;
			int vb;

			seed_pair(trial + 2600, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 2601,
				  trial & 3);
			p34_params();
			SET_PF(TRN1D_ERROR_FOR_V34_FALLBACK, 0xff7fffffu);
			SET_P(MAX_NOF_V90_RETRAINS, 100000);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b2, (short)0);
			SET_CE(word_88, 1u);
			SET_CE(word_84, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 1u);
			SET_CE(word_68, 100000u);
			SET_CE(word_74, 1u);
			SET_CEF(word_70, avg_bits[(unsigned)trial % NAVG]);

			vb = p3_call(tag, 1);
			if (avg_bits[(unsigned)trial % NAVG] == 0x7fc00000u) {
				diff_eq_int("a NaN is not above anything (%ld)",
					    vb, 0, tag);
				diff_eq_int("and +0x10 was cleared (%ld)",
					    (long)CEB->word_10, 0, tag);
			} else {
				diff_eq_int("everything else printed (%ld)", vb,
					    5, tag);
				if (lvl > 1)
					diff_eq_int("one line (%ld)",
						    (int)
						    dsplib_debug_capture_lines(1),
						    1, tag);
			}
			consumed(tag);
		}

		/*
		 * THE FOUR UNSIGNED COMPARISONS PHASE 3 MAKES, driven where `ja`
		 * and `jb` DISAGREE with `jg` and `jl`.  Every ordinary value
		 * hides the difference; a negative limit does not, because the
		 * unsigned reading turns it into a number no counter reaches and
		 * the signed one into a number every counter is already past.
		 * Both slots the map calls `int` get one.
		 */
		{
			int call;
			long tag = (long)lvl * 100000 + 9000;

			seed_pair(2700 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 2701 + lvl,
				  lvl & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100000);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b2, (short)0);
			SET_CE(word_88, 0u);
			SET_CE(word_84, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			/*
			 * -1 as UNSIGNED is 0xffffffff, which nothing reaches;
			 * as SIGNED it is below every count and the retrain
			 * fires on the first call.
			 */
			SET_CE(retrainDetectDuration, -1);

			for (call = 0; call < 6; call++) {
				int vb;

				SET_CE(word_74, 100u);
				SET_CEF(word_70, 0x41a00000u);	/* 20.0f */
				vb = p3_call(tag + call, 1);
				diff_eq_int("a negative duration is never "
					    "reached (%ld)", vb, 0, tag + call);
				diff_eq_int("and +0x18 kept accumulating (%ld)",
					    (long)CEB->word_18,
					    (long)(100 * (call + 1)),
					    tag + call);
				consumed(tag + call);
			}
			p3_unsigned_dur = 1;
		}

		{
			long tag = (long)lvl * 100000 + 9100;
			int vb;

			seed_pair(2800 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 2801 + lvl,
				  lvl & 3);
			p34_params();
			/* -1 unsigned: no counter is ever over it. */
			SET_P(MAX_NOF_V90_RETRAINS, -1);
			SET_P(MAX_NOF_REMOTE_RETRAINS, 0);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b2, (short)0);
			SET_CE(word_88, 0u);
			SET_CE(word_84, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(retrainDetectDuration, 10);
			SET_CE(word_74, 20u);
			SET_CEF(word_70, 0x41a00000u);		/* 20.0f */

			vb = p3_call(tag, 1);
			diff_eq_int("a negative retrain limit is never "
				    "exceeded (%ld)", vb, 4, tag);
			diff_eq_int("so the counter kept counting (%ld)",
				    (long)CEB->nofV90Retrains, 1, tag);
			consumed(tag);
			p3_unsigned_max = 1;
		}
	}

	set_level(0);
	return diff_end();
}

static int
run_ce_phase4(void)
{
	static const unsigned int arg_bits[] = {
		0x00000000u,	/*  +0.0f                       */
		0x80000000u,	/*  -0.0f                       */
		0x00000001u,	/*  denormal                    */
		0x80000001u,	/* -denormal                    */
		0x3f800000u,	/*   1.0f                       */
		0xbf800000u,	/*  -1.0f                       */
		0x41600000u,	/*  14.0f, the threshold exactly */
		0x41600001u,	/*  a ulp above it              */
		0x415fffffu,	/*  a ulp below it              */
		0x42c80000u,	/* 100.0f                       */
		0xc2c80000u,	/* -100.0f                      */
		0x7f7fffffu,	/*  FLT_MAX                     */
		0xff7fffffu,	/* -FLT_MAX                     */
		0x7f800000u,	/*  +inf                        */
		0xff800000u,	/*  -inf                        */
		0x4b7fffffu,	/* 16777215.0f                  */
		0x39a2b3c4u	/*  small positive              */
	};
	unsigned narg = sizeof(arg_bits) / sizeof(arg_bits[0]);
	int lvl, trial;

	diff_begin("V90ConnectionEvaluator::evaluatePhase4");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned)lvl);

		/* A zero symbol count: nothing, whatever the argument is. */
		for (trial = 0; trial < (int)narg; trial++) {
			unsigned char before[CE_SLOT];
			long tag = (long)lvl * 100000 + trial;
			int vb;

			seed_pair(trial + 3000, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 3001,
				  trial & 3);
			p34_params();
			SET_CE(word_74, 0u);
			SET_CE(short_b0, (short)1);
			SET_CE(word_78, 1u);
			SET_CE(word_7c, 1u);
			memcpy(before, ce_b, CE_SLOT);

			vb = p4_call(tag, as_float(arg_bits[trial]), 1);
			diff_eq_int("the empty call answered nothing (%ld)", vb,
				    0, tag);
			diff_eq_int("and wrote nothing (%ld)",
				    memcmp(before, ce_b, CE_SLOT) == 0, 1, tag);
			diff_eq_int("and printed nothing (%ld)",
				    (int)dsplib_debug_capture_lines(1), 0, tag);
			p4_empty = 1;
		}

		/*
		 * THE ARGUMENT SWEEP AGAINST THE MEAN-ERROR ARM.  +0xb0 is
		 * re-armed every trial and the counter is kept below
		 * `unnamed_45c`, so the arm fires for exactly the arguments
		 * strictly above 14.0f and for no others.  The average is put
		 * ABOVE +0xac and +0x10 one short of +0x68, so a call that took
		 * the arm and did NOT skip the rest of the function would fall
		 * back to V.34 instead of retraining -- which is how "the arm
		 * skips everything" is tested rather than assumed.
		 */
		for (trial = 0; trial < (int)narg; trial++) {
			long tag = (long)lvl * 100000 + 200 + trial;
			unsigned int bits = arg_bits[trial];
			int fires = (bits == 0x41600001u || bits == 0x42c80000u
				     || bits == 0x7f7fffffu
				     || bits == 0x7f800000u
				     || bits == 0x4b7fffffu);
			int vb;

			seed_pair(trial + 3100, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 3101,
				  trial & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100);
			SET_P(MAX_NOF_REMOTE_RETRAINS, -101);
			SET_P(unnamed_45c, 100);
			SET_CE(nofV90Retrains, 5u);
			SET_CE(short_b0, (short)1);
			SET_CE(word_78, 0u);
			SET_CE(word_7c, 0u);
			SET_CE(word_10, 990u);
			SET_CE(word_18, 777u);
			SET_CE(word_64, 12345u);
			SET_CE(word_68, 1000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(word_74, 20u);
			SET_CEF(word_70, 0x42c80000u);		/* 100.0f */
			SET_CEF(phase4ErrorForV34Fallback, 0x41a00000u);

			vb = p4_call(tag, as_float(bits), 1);
			{
				unsigned int thr;

				memcpy(&thr, &CEB->phase4ErrorForV34Fallback,
				       4);
				diff_eq_int("+0xac was left alone (%ld)",
					    thr == 0x41a00000u, 1, tag);
			}
			if (fires) {
				diff_eq_int("the arm fired (%ld)", vb, 4, tag);
				diff_eq_int("and cleared +0xb0 (%ld)",
					    (long)CEB->short_b0, 0, tag);
				diff_eq_int("and skipped the rest: +0x10 stood "
					    "still (%ld)",
					    (long)CEB->word_10, 990, tag);
				diff_eq_int("and it cleared +0x18 (%ld)",
					    (long)CEB->word_18, 0, tag);
				p4_mean_arm = 1;
			} else {
				/*
				 * The arm did not fire, so the fall-back half
				 * ran instead: +0x10 reached +0x68 and the
				 * function left by its own epilogue WITHOUT
				 * reaching the +0x18 half at all.
				 */
				diff_eq_int("the arm did not fire (%ld)", vb, 5,
					    tag);
				diff_eq_int("and +0xb0 still stands (%ld)",
					    (long)CEB->short_b0, 1, tag);
				diff_eq_int("and +0x10 accumulated (%ld)",
					    (long)CEB->word_10, 1010, tag);
				diff_eq_int("and +0x18 was never reached (%ld)",
					    (long)CEB->word_18, 777, tag);
				p4_mean_skipped = 1;
				p4_guard_ratio = 1;
			}
			consumed(tag);
		}

		/*
		 * THE OTHER TWO GUARDS, one at a time.  The argument is over the
		 * threshold in both, so whichever guard is false is the only
		 * reason the arm does not fire.
		 */
		for (trial = 0; trial < 4; trial++) {
			long tag = (long)lvl * 100000 + 300 + trial;
			int b0 = (trial & 1) != 0;
			int room = (trial & 2) != 0;
			int vb;

			seed_pair(trial + 3200, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 3201,
				  trial & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100);
			SET_P(unnamed_45c, room ? 100 : 5);
			SET_CE(nofV90Retrains, 5u);
			SET_CE(short_b0, (short)(b0 ? 1 : 0));
			SET_CE(word_78, 0u);
			SET_CE(word_7c, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(word_74, 20u);
			SET_CEF(word_70, 0x40000000u);		/* 2.0f */
			SET_CEF(phase4ErrorForV34Fallback, 0x42c80000u);

			vb = p4_call(tag, as_float(0x42c80000u), 1);
			if (b0 && room) {
				diff_eq_int("all three guards passed (%ld)", vb,
					    4, tag);
			} else {
				diff_eq_int("a guard blocked it (%ld)", vb, 0,
					    tag);
				diff_eq_int("+0xb0 was left alone (%ld)",
					    (long)CEB->short_b0, b0 ? 1 : 0,
					    tag);
				if (!b0)
					p4_guard_b0 = 1;
				else
					p4_guard_count = 1;
			}
			consumed(tag);
		}

		/*
		 * THE FALL-BACK WITH ITS OWN EPILOGUE.  +0x10 runs out against
		 * +0x68 -- NOT +0x64, which is given a different value -- and the
		 * function returns 5 from an immediate rather than from %esi,
		 * having cleared neither +0x10 nor +0x18.
		 */
		{
			int block;
			unsigned int want10 = 0, want18 = 8888u;

			seed_pair(3300 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 3301 + lvl,
				  lvl & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100000);
			SET_CE(nofV90Retrains, 9u);
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, 0u);
			SET_CE(word_7c, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 8888u);
			SET_CE(word_64, 3u);		/* would trip at once */
			SET_CE(word_68, 400u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CEF(phase4ErrorForV34Fallback, 0x41200000u);

			for (block = 0; block < 40; block++) {
				long tag = (long)lvl * 100000 + 6000 + block;
				unsigned int n = 30u + (unsigned)(block % 5);
				int above = (block % 20) != 19;
				int vb;

				SET_CE(word_74, n);
				SET_CEF(word_70, above ? 0x41a00000u
						       : 0x3f800000u);
				vb = p4_call(tag, as_float(0x00000000u), 1);

				if (above) {
					want10 += n;
					if (want10 >= 400u) {
						diff_eq_int("the fall-back "
							    "epilogue (%ld)", vb,
							    5, tag);
						diff_eq_int("the counter "
							    "restarted (%ld)",
							    (long)
							    CEB->nofV90Retrains,
							    0, tag);
						p4_large = 1;
					} else {
						/*
						 * The early return never
						 * happened, so the +0x18 half
						 * ran: 20.0f is over
						 * PDSNR_THRESHOLD_IN_PHASE4.
						 */
						want18 += n;
						diff_eq_int("still counting "
							    "(%ld)", vb, 0, tag);
					}
				} else {
					want10 = 0;
					want18 = 0;
					p4_cleared_10 = 1;
				}
				diff_eq_int("+0x10 is the running duration "
					    "(%ld)", (long)CEB->word_10,
					    (long)want10, tag);
				diff_eq_int("+0x18 moves only when the early "
					    "return does not (%ld)",
					    (long)CEB->word_18, (long)want18,
					    tag);
				consumed(tag);
			}
		}

		/*
		 * THE PHASE-4 RETRAIN, and the store into +0xac that comes with
		 * it.  The first retrain replaces the threshold with
		 * `params->unnamed_434` -- 250.0f -- which the next block's
		 * comparison then uses, so this also proves the field is read
		 * back and not only written.
		 */
		{
			int block;
			unsigned int want18 = 0;
			int replaced = 0;

			seed_pair(3400 + lvl, (lvl + 2) & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 3401 + lvl,
				  (lvl + 2) & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 4);
			SET_P(MAX_NOF_REMOTE_RETRAINS, -5);
			SET_P(unnamed_45c, -5);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, 0u);
			SET_CE(word_7c, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 7u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 90);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			for (block = 0; block < 60; block++) {
				long tag = (long)lvl * 100000 + 7000 + block;
				unsigned int n = 20u;
				unsigned int got;
				int vb;

				SET_CE(word_74, n);
				SET_CEF(word_70, 0x42c80000u);	/* 100.0f */
				vb = p4_call(tag, as_float(0x00000000u), 1);

				want18 += n;
				if (want18 >= 90u) {
					want18 = 0;
					p4_cleared_18 = 1;
					diff_eq_int("a decision was taken "
						    "(%ld)",
						    vb == 4 || vb == 5, 1, tag);
					if (vb == 4) {
						memcpy(&got,
						    &CEB->
						    phase4ErrorForV34Fallback,
						    4);
						diff_eq_int("+0xac took "
							    "unnamed_434 (%ld)",
							    got == 0x437a0000u,
							    1, tag);
						replaced = 1;
						p4_thresh_replaced = 1;
						p4_retrain = 1;
					} else {
						diff_eq_int("the counter "
							    "restarted (%ld)",
							    (long)
							    CEB->nofV90Retrains,
							    0, tag);
					}
					if (lvl > 1 && vb == 4)
						diff_eq_int("the gated line came "
							    "out too (%ld)",
							    (int)
							    dsplib_debug_capture_lines(1),
							    2, tag);
					if (lvl <= 1)
						diff_eq_int("below the gate "
							    "nothing printed "
							    "(%ld)",
							    (int)
							    dsplib_debug_capture_lines(1),
							    0, tag);
				} else {
					diff_eq_int("still counting (%ld)", vb,
						    0, tag);
				}
				diff_eq_int("+0x18 is the running duration "
					    "(%ld)", (long)CEB->word_18,
					    (long)want18, tag);
				consumed(tag);

				/*
				 * Once +0xac holds 250.0f the average at 100.0f
				 * is below it, so the fall-back half stays
				 * quiet and +0x10 is cleared every call.
				 */
				if (replaced)
					diff_eq_int("+0x10 stays clear (%ld)",
						    (long)CEB->word_10, 0, tag);
			}
		}

		/*
		 * THE DELAYED RETRAIN.  Both slots must be non-zero; the three
		 * other combinations must leave both alone.
		 */
		for (trial = 0; trial < 4; trial++) {
			long tag = (long)lvl * 100000 + 400 + trial;
			unsigned int w78 = (trial & 1) ? 7u : 0u;
			unsigned int w7c = (trial & 2) ? 9u : 0u;
			int vb;

			seed_pair(trial + 3500, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 3501,
				  trial & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100);
			SET_CE(nofV90Retrains, 2u);
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, w78);
			SET_CE(word_7c, w7c);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(word_74, 5u);
			SET_CEF(word_70, 0x3f800000u);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0x00000000u), 1);
			if (w78 != 0 && w7c != 0) {
				diff_eq_int("the delayed retrain fired (%ld)",
					    vb, 4, tag);
				diff_eq_int("+0x78 was cleared (%ld)",
					    (long)CEB->word_78, 0, tag);
				diff_eq_int("+0x7c was cleared (%ld)",
					    (long)CEB->word_7c, 0, tag);
				diff_eq_int("the counter kept counting (%ld)",
					    (long)CEB->nofV90Retrains, 3, tag);
				p4_delayed = 1;
			} else {
				diff_eq_int("half a request is nothing (%ld)",
					    vb, 0, tag);
				diff_eq_int("+0x78 was left alone (%ld)",
					    (long)CEB->word_78, (long)w78, tag);
				diff_eq_int("+0x7c was left alone (%ld)",
					    (long)CEB->word_7c, (long)w7c, tag);
				p4_delayed_half = 1;
			}
			consumed(tag);
		}

		/*
		 * THE DELAYED RETRAIN OVER ITS LIMIT -- and the one transcript
		 * this file does not compare.  0x3ffcf's format has a `%d` and
		 * the object stores no argument for it, so `vsnprintf` formats
		 * the outgoing-argument slot of whichever frame it is in.  State,
		 * verdict and line count are compared; the text is not.
		 */
		for (trial = 0; trial < 4; trial++) {
			long tag = (long)lvl * 100000 + 500 + trial;
			int vb;

			seed_pair(trial + 3600, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 3601,
				  trial & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, trial);
			SET_P(MAX_NOF_REMOTE_RETRAINS, ~trial);
			SET_CE(nofV90Retrains, (unsigned int)trial);
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, 1u);
			SET_CE(word_7c, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(word_74, 5u);
			SET_CEF(word_70, 0x3f800000u);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0x00000000u), 0);
			diff_eq_int("over the limit it gives up (%ld)", vb, 5,
				    tag);
			diff_eq_int("the counter restarted (%ld)",
				    (long)CEB->nofV90Retrains, 0, tag);
			diff_eq_int("+0x78 was cleared (%ld)",
				    (long)CEB->word_78, 0, tag);
			diff_eq_int("+0x7c was cleared (%ld)",
				    (long)CEB->word_7c, 0, tag);
			if (lvl > 1)
				diff_eq_int("two lines came out (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    2, tag);
			consumed(tag);
			p4_delayed_over = 1;
		}

		/*
		 * A RETRAIN AND THE DELAYED GIVE-UP IN ONE CALL -- four lines out
		 * of one invocation, which no other setup here produces.
		 *
		 * THE ATTEMPT TO MAKE THE MISSING ARGUMENT COMPARABLE IS
		 * RECORDED HERE BECAUSE IT FAILED.  The absent `%d` formats
		 * 0x4(%esp), the second outgoing-argument slot, so the idea was
		 * to have the call immediately before it -- the phase-4 retrain,
		 * which passes `nofV90Retrains` in exactly that slot -- leave a
		 * known value there on both sides.  It was measured and the two
		 * transcripts still differ, and only in that one line: GCC 13
		 * does not leave our frame's slot holding what GCC 3.4's leaves
		 * in the object's.  So the text is not compared here either, and
		 * the claim is a note in the mutation set rather than a mutation.
		 * Finding 1388.
		 */
		{
			long tag = (long)lvl * 100000 + 700;
			int vb;

			seed_pair(3800 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 3801 + lvl,
				  lvl & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 3);
			SET_P(MAX_NOF_REMOTE_RETRAINS, -4);
			SET_P(unnamed_45c, -4);
			SET_CE(nofV90Retrains, 2u);
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, 1u);
			SET_CE(word_7c, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 10);
			SET_CE(word_74, 50u);
			SET_CEF(word_70, 0x42c80000u);		/* 100.0f */
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0x00000000u), 0);
			diff_eq_int("a retrain and then the delayed give-up "
				    "(%ld)", vb, 5, tag);
			diff_eq_int("the counter restarted (%ld)",
				    (long)CEB->nofV90Retrains, 0, tag);
			if (lvl > 1)
				diff_eq_int("four lines came out (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    4, tag);
			consumed(tag);
			p4_missing_arg = 1;
		}

		/* Nothing armed at all: the verdict is 0 and the average goes. */
		for (trial = 0; trial < (int)NAVG; trial++) {
			long tag = (long)lvl * 100000 + 600 + trial;
			int vb;

			seed_pair(trial + 3700, trial & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, trial + 3701,
				  trial & 3);
			p34_params();
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, 0u);
			SET_CE(word_7c, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(word_74, 13u);
			SET_CEF(word_70, avg_bits[(unsigned)trial % NAVG]);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0xbf800000u), 1);
			diff_eq_int("nothing to decide (%ld)", vb, 0, tag);
			consumed(tag);
			p4_none = 1;
		}

		/*
		 * THE FOUR UNSIGNED COMPARISONS PHASE 4 MAKES, each driven at a
		 * negative limit -- the only kind of value on which `jb`/`ja`
		 * and `jl`/`jg` answer differently.  +0x45c gets one too: it is
		 * `jae` at 0x3fbf4 and it gates the mean-error arm, so a signed
		 * reading would BLOCK the arm where the object runs it.
		 */
		{
			int call;
			long tag = (long)lvl * 100000 + 9000;

			seed_pair(3900 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 3901 + lvl,
				  lvl & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100000);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, 0u);
			SET_CE(word_7c, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, -1);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			for (call = 0; call < 6; call++) {
				int vb;

				SET_CE(word_74, 100u);
				SET_CEF(word_70, 0x42c80000u);	/* 100.0f */
				vb = p4_call(tag + call, as_float(0u), 1);
				diff_eq_int("a negative duration is never "
					    "reached (%ld)", vb, 0, tag + call);
				diff_eq_int("and +0x18 kept accumulating (%ld)",
					    (long)CEB->word_18,
					    (long)(100 * (call + 1)),
					    tag + call);
				consumed(tag + call);
			}
			p4_unsigned_dur = 1;
		}

		{
			long tag = (long)lvl * 100000 + 9100;
			int vb;

			seed_pair(4000 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 4001 + lvl,
				  lvl & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, 100000);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, 0u);
			SET_CE(word_7c, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 10);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);
			SET_CE(word_74, 20u);
			SET_CEF(word_70, 0x42c80000u);		/* 100.0f */
			SET_P(MAX_NOF_V90_RETRAINS, -1);
			SET_P(MAX_NOF_REMOTE_RETRAINS, 0);

			vb = p4_call(tag, as_float(0u), 1);
			diff_eq_int("a negative retrain limit is never "
				    "exceeded (%ld)", vb, 4, tag);
			diff_eq_int("so the counter kept counting (%ld)",
				    (long)CEB->nofV90Retrains, 1, tag);
			consumed(tag);
			p4_unsigned_max = 1;
		}

		{
			long tag = (long)lvl * 100000 + 9200;
			int vb;

			seed_pair(4100 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 4101 + lvl,
				  lvl & 3);
			p34_params();
			/*
			 * BOTH of the arm's integer limits negative at once:
			 * +0x45c gates it (`jae`) and +0x460 ends it (`ja`), so
			 * one trial has to make each of them unsigned or the
			 * other's mutation survives.
			 */
			SET_P(MAX_NOF_V90_RETRAINS, -1);
			SET_P(MAX_NOF_REMOTE_RETRAINS, 0);
			SET_P(unnamed_45c, -1);
			SET_CE(nofV90Retrains, 7u);
			SET_CE(short_b0, (short)1);
			SET_CE(word_78, 0u);
			SET_CE(word_7c, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(word_74, 20u);
			SET_CEF(word_70, 0x40000000u);		/* 2.0f */
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0x42c80000u), 1);
			diff_eq_int("a negative second ceiling admits every "
				    "count (%ld)", vb, 4, tag);
			diff_eq_int("so the arm fired and cleared +0xb0 (%ld)",
				    (long)CEB->short_b0, 0, tag);
			consumed(tag);
			p4_unsigned_45c = 1;
		}

		{
			long tag = (long)lvl * 100000 + 9300;
			int vb;

			seed_pair(4200 + lvl, lvl & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 4201 + lvl,
				  lvl & 3);
			p34_params();
			SET_P(MAX_NOF_V90_RETRAINS, -1);
			SET_P(MAX_NOF_REMOTE_RETRAINS, 0);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(short_b0, (short)0);
			SET_CE(word_78, 3u);
			SET_CE(word_7c, 4u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(word_74, 5u);
			SET_CEF(word_70, 0x3f800000u);		/* 1.0f */
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0u), 1);
			diff_eq_int("the delayed arm's limit is unsigned too "
				    "(%ld)", vb, 4, tag);
			diff_eq_int("so the counter kept counting (%ld)",
				    (long)CEB->nofV90Retrains, 1, tag);
			consumed(tag);
			p4_unsigned_delayed = 1;
		}
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
	rc |= run_ce_phase3();
	rc |= run_ce_phase4();

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

	/*
	 * THE TWO EVALUATORS DECIDE THREE WAYS AND HAVE MORE PATHS THAN
	 * VERDICTS, so "all three answers were seen" is necessary and nowhere
	 * near sufficient.  Every named path below is one the run above claims
	 * to have driven; an assertion here that fails means the fixture stopped
	 * reaching it and the checks that looked green were vacuous.
	 */
	diff_eq_int("evaluatePhase3 answered 0", p3_verdict[0], 1, 0);
	diff_eq_int("evaluatePhase3 answered 4", p3_verdict[1], 1, 0);
	diff_eq_int("evaluatePhase3 answered 5", p3_verdict[2], 1, 0);
	diff_eq_int("evaluatePhase4 answered 0", p4_verdict[0], 1, 0);
	diff_eq_int("evaluatePhase4 answered 4", p4_verdict[1], 1, 0);
	diff_eq_int("evaluatePhase4 answered 5", p4_verdict[2], 1, 0);

	diff_eq_int("phase3: the empty call", p3_empty, 1, 0);
	diff_eq_int("phase3: no arm armed", p3_none, 1, 0);
	diff_eq_int("phase3: the altRbsDetectedOnQc arm", p3_altrbs, 1, 0);
	diff_eq_int("phase3: the end-of-TRN1d fall-back", p3_trn1d, 1, 0);
	diff_eq_int("phase3: the ordinary fall-back", p3_large, 1, 0);
	diff_eq_int("phase3: the ordinary retrain", p3_retrain, 1, 0);
	diff_eq_int("phase3: +0x10 cleared below its threshold", p3_cleared_10,
		    1, 0);
	diff_eq_int("phase3: +0x18 cleared below its threshold", p3_cleared_18,
		    1, 0);
	diff_eq_int("phase3: one call printed the fall-back and returned 4",
		    p3_five_then_four, 1, 0);

	diff_eq_int("phase4: the empty call", p4_empty, 1, 0);
	diff_eq_int("phase4: no arm armed", p4_none, 1, 0);
	diff_eq_int("phase4: the mean-error arm fired", p4_mean_arm, 1, 0);
	diff_eq_int("phase4: and skipped the rest when it did",
		    p4_mean_skipped, 1, 0);
	diff_eq_int("phase4: the +0xb0 guard blocked it", p4_guard_b0, 1, 0);
	diff_eq_int("phase4: the ratio guard blocked it", p4_guard_ratio, 1, 0);
	diff_eq_int("phase4: the unnamed_45c guard blocked it", p4_guard_count,
		    1, 0);
	diff_eq_int("phase4: the fall-back with its own epilogue", p4_large, 1,
		    0);
	diff_eq_int("phase4: the retrain", p4_retrain, 1, 0);
	diff_eq_int("phase4: +0xac was replaced by unnamed_434",
		    p4_thresh_replaced, 1, 0);
	diff_eq_int("phase4: +0x10 cleared below its threshold", p4_cleared_10,
		    1, 0);
	diff_eq_int("phase4: +0x18 accumulated and was cleared", p4_cleared_18,
		    1, 0);
	diff_eq_int("phase4: the delayed retrain", p4_delayed, 1, 0);
	diff_eq_int("phase4: half a delayed request does nothing",
		    p4_delayed_half, 1, 0);
	diff_eq_int("phase4: the delayed retrain over its limit",
		    p4_delayed_over, 1, 0);
	diff_eq_int("phase4: the missing-argument print, with the slot made "
		    "deterministic", p4_missing_arg, 1, 0);

	/*
	 * THE FIVE UNSIGNED COMPARISONS, each driven at a negative limit --
	 * the only place `ja`/`jb` and `jg`/`jl` part company.
	 */
	diff_eq_int("phase3: a negative retrain duration", p3_unsigned_dur, 1,
		    0);
	diff_eq_int("phase3: a negative retrain limit", p3_unsigned_max, 1, 0);
	diff_eq_int("phase4: a negative retrain duration", p4_unsigned_dur, 1,
		    0);
	diff_eq_int("phase4: a negative retrain limit", p4_unsigned_max, 1, 0);
	diff_eq_int("phase4: a negative second ceiling", p4_unsigned_45c, 1, 0);
	diff_eq_int("phase4: a negative limit in the delayed arm",
		    p4_unsigned_delayed, 1, 0);

	diff_eq_int("and 0 is not one of the other two verdicts",
		    V90CE_VERDICT_NONE != V90CE_VERDICT_RETRAIN
		    && V90CE_VERDICT_NONE != V90CE_VERDICT_FALLBACK_V34, 1, 0);
	rc |= diff_end();

	return rc;
}
