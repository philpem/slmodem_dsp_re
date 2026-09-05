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
 *   (findings F149 and F223).  `indicateLocalRetrain` and
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
 * translation unit may include both (finding F1112), and the names are the
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
	diff_eq_int("externalDemandCode (%ld)", (long)o->externalDemandCode, -1, tag);
	diff_eq_int("initDmin (%ld)", (long)o->initDmin, -1, tag);
	diff_eq_int("curDmin (%ld)", (long)o->curDmin, 0, tag);
	diff_eq_int("meanErrorCheckArmed (%ld)", (long)o->meanErrorCheckArmed, 1, tag);
	diff_eq_int("altRbsDetectedOnQc (%ld)", (long)o->altRbsDetectedOnQc, 0, tag);
	diff_eq_int("echoRrnState (%ld)", (long)o->echoRrnState, 0, tag);
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
	diff_eq_int("avePdsnr (%ld)", memcmp(&o->avePdsnr, &zero, 4) == 0, 1,
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
		 * real (finding F1224).  Both sides compare against the seed,
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
		CEA->avePdsnrNofSymbols = starts[s];
		CEB->avePdsnrNofSymbols = starts[s];
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
			unsigned int before74 = CEB->avePdsnrNofSymbols;

			CEA->updateAvePdsnr(p, n);
			ref_ce_updateAvePdsnr(ce_b, p, n);

			diff_eq_obj("after updateAvePdsnr",
				    V90ConnectionEvaluator, CEA, CEB, tag);
			guard_intact(tag);

			diff_eq_int("the count advanced by n (%ld)",
				    (long)(unsigned int)(CEB->avePdsnrNofSymbols
							 - before74),
				    (long)n, tag);
			if (CEB->avePdsnrNofSymbols >= 0x80000000u)
				huge_total = 1;

			if (s == 0 && block == 0)
				memcpy(&firstavg, &CEB->avePdsnr, 4);
			else if (memcmp(&firstavg, &CEB->avePdsnr, 4) != 0)
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

			CEA->avePdsnrNofSymbols = 0;
			CEB->avePdsnrNofSymbols = 0;
			CEA->updateAvePdsnr(p, 4242u);
			ref_ce_updateAvePdsnr(ce_b, p, 4242u);

			diff_eq_obj("after the first call",
				    V90ConnectionEvaluator, CEA, CEB, tag);
			memcpy(&got, &CEB->avePdsnr, 4);
			memcpy(&want, &p, 4);
			diff_eq_int("the average is the sample (%ld)",
				    got == want, 1, tag);
			diff_eq_int("the count is n (%ld)",
				    (long)CEB->avePdsnrNofSymbols, 4242, tag);
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
 *   `avePdsnrNofSymbols` per call, and both evaluators CONSUME the average on the way out
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
 *   compares state, verdict and LINE COUNT and not the text.  Finding F1388.
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
/* The retrain fired with an unordered `unnamed_434`.  Finding F2410. */
static int p4_nan_thresh;
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
	diff_eq_int("the count was consumed (%ld)", (long)CEB->avePdsnrNofSymbols, 0, tag);
	diff_eq_int("the average was cleared (%ld)",
		    memcmp(&CEB->avePdsnr, &fzero, 4) == 0, 1, tag);
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
			SET_CE(avePdsnrNofSymbols, 0u);
			SET_CE(altRbsDetectedOnQc, (short)1);
			SET_CE(trn1dEvalEnabled, 1u);
			SET_CE(phase3EvalEnabled, 1u);
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
			SET_CE(avePdsnrNofSymbols, 37u);
			SET_CEF(avePdsnr, avg_bits[(unsigned)trial % NAVG]);
			SET_CE(altRbsDetectedOnQc, (short)0);
			SET_CE(trn1dEvalEnabled, 0u);
			SET_CE(phase3EvalEnabled, 0u);
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
			SET_CE(trn1dEvalEnabled, 1u);
			SET_CE(phase3EvalEnabled, 1u);
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

				SET_CE(avePdsnrNofSymbols, 11u + (unsigned)call);
				SET_CEF(avePdsnr,
					avg_bits[(unsigned)call % NAVG]);
				SET_CE(altRbsDetectedOnQc, (short)(1 + call));

				vb = p3_call(tag, 1);
				diff_eq_int("the arm decided (%ld)",
					    vb == 4 || vb == 5, 1, tag);
				diff_eq_int("altRbs was cleared (%ld)",
					    (long)CEB->altRbsDetectedOnQc, 0, tag);
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
			SET_CE(altRbsDetectedOnQc, (short)0);
			SET_CE(trn1dEvalEnabled, 1u);
			SET_CE(phase3EvalEnabled, 1u);
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

				SET_CE(avePdsnrNofSymbols, n);
				SET_CEF(avePdsnr, above ? 0x41a00000u
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
			SET_CE(altRbsDetectedOnQc, (short)0);
			SET_CE(trn1dEvalEnabled, 0u);
			SET_CE(phase3EvalEnabled, 1u);
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

				SET_CE(avePdsnrNofSymbols, n);
				SET_CEF(avePdsnr, 0x41a00000u);	/* 20.0f */
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
			SET_CE(altRbsDetectedOnQc, (short)0);
			SET_CE(trn1dEvalEnabled, 0u);
			SET_CE(phase3EvalEnabled, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 4444u);
			SET_CE(word_64, 100000u);
			SET_CE(retrainDetectDuration, 3);
			SET_CE(avePdsnrNofSymbols, 17u);
			SET_CEF(avePdsnr, avg_bits[(unsigned)trial % NAVG]);

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
			SET_CE(altRbsDetectedOnQc, (short)0);
			SET_CE(trn1dEvalEnabled, 1u);
			SET_CE(phase3EvalEnabled, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 1u);
			SET_CE(word_68, 100000u);
			SET_CE(avePdsnrNofSymbols, 1u);
			SET_CEF(avePdsnr, avg_bits[(unsigned)trial % NAVG]);

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
			SET_CE(altRbsDetectedOnQc, (short)0);
			SET_CE(trn1dEvalEnabled, 0u);
			SET_CE(phase3EvalEnabled, 1u);
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

				SET_CE(avePdsnrNofSymbols, 100u);
				SET_CEF(avePdsnr, 0x41a00000u);	/* 20.0f */
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
			SET_CE(altRbsDetectedOnQc, (short)0);
			SET_CE(trn1dEvalEnabled, 0u);
			SET_CE(phase3EvalEnabled, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(retrainDetectDuration, 10);
			SET_CE(avePdsnrNofSymbols, 20u);
			SET_CEF(avePdsnr, 0x41a00000u);		/* 20.0f */

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
			SET_CE(avePdsnrNofSymbols, 0u);
			SET_CE(meanErrorCheckArmed, (short)1);
			SET_CE(delayedRetrainRequest, 1u);
			SET_CE(delayedRetrainArmed, 1u);
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
			SET_CE(meanErrorCheckArmed, (short)1);
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
			SET_CE(word_10, 990u);
			SET_CE(word_18, 777u);
			SET_CE(word_64, 12345u);
			SET_CE(word_68, 1000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(avePdsnrNofSymbols, 20u);
			SET_CEF(avePdsnr, 0x42c80000u);		/* 100.0f */
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
					    (long)CEB->meanErrorCheckArmed, 0, tag);
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
					    (long)CEB->meanErrorCheckArmed, 1, tag);
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
			SET_CE(meanErrorCheckArmed, (short)(b0 ? 1 : 0));
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(avePdsnrNofSymbols, 20u);
			SET_CEF(avePdsnr, 0x40000000u);		/* 2.0f */
			SET_CEF(phase4ErrorForV34Fallback, 0x42c80000u);

			vb = p4_call(tag, as_float(0x42c80000u), 1);
			if (b0 && room) {
				diff_eq_int("all three guards passed (%ld)", vb,
					    4, tag);
			} else {
				diff_eq_int("a guard blocked it (%ld)", vb, 0,
					    tag);
				diff_eq_int("+0xb0 was left alone (%ld)",
					    (long)CEB->meanErrorCheckArmed, b0 ? 1 : 0,
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
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

				SET_CE(avePdsnrNofSymbols, n);
				SET_CEF(avePdsnr, above ? 0x41a00000u
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
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

				SET_CE(avePdsnrNofSymbols, n);
				SET_CEF(avePdsnr, 0x42c80000u);	/* 100.0f */
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
		 * THE SAME RETRAIN WITH AN UNORDERED `unnamed_434`, WHICH IS
		 * THE ONLY WAY A NaN REACHES A SIGN PRINTER IN THIS FILE.
		 *
		 * Nineteen of this object's branchless sign selects are in
		 * these three methods, and eighteen of them print `avePdsnr`
		 * from inside `if (avePdsnr > threshold)` -- the object's
		 * `flds; fcoms; ja`, which is FALSE for an unordered compare,
		 * so a NaN average provably cannot reach any of them.  The
		 * nineteenth prints `t`, the replacement threshold read out of
		 * `params->unnamed_434`, and its gate is on `avePdsnr` and the
		 * counters and not on `t` -- so an unordered parameter gets
		 * there with the average left ordered at 100.0f.
		 *
		 * `!(0.0f >= t)` prints '+' for it, which is what `sbb
		 * %esi,%esi; and $0xfffffffe,%esi; add $0x2d,%esi` at 0x3fdb7
		 * computes; `(0.0f < t)` prints '-'.  Findings F2300 and F2410.
		 *
		 * ITS OWN BLOCK, NOT A DIMENSION OF THE ONE ABOVE: +0xac keeps
		 * what the retrain stored, and that block's later iterations
		 * depend on it holding 250.0f.  Here the store is checked and
		 * the block ends.
		 */
		{
			long tag = (long)lvl * 100000 + 7500;
			unsigned int got;
			int vb;

			seed_pair(3450 + lvl, (lvl + 2) & 3);
			fill_pair(parm_a, parm_b, PARM_SLOT, 3451 + lvl,
				  (lvl + 2) & 3);
			p34_params();
			SET_PF(unnamed_434, 0x7fc00000u);	/* a quiet NaN */
			SET_P(MAX_NOF_V90_RETRAINS, 4);
			SET_P(MAX_NOF_REMOTE_RETRAINS, -5);
			SET_P(unnamed_45c, -5);
			SET_CE(nofV90Retrains, 0u);
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 7u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 90);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);
			SET_CE(avePdsnrNofSymbols, 100u);
			SET_CEF(avePdsnr, 0x42c80000u);		/* 100.0f */

			vb = p4_call(tag, as_float(0x00000000u), 1);

			diff_eq_int("the retrain fired on an unordered "
				    "threshold (%ld)", vb, 4, tag);
			memcpy(&got, &CEB->phase4ErrorForV34Fallback, 4);
			diff_eq_int("+0xac took the unordered unnamed_434 "
				    "(%ld)", got == 0x7fc00000u, 1, tag);
			if (vb == 4)
				p4_nan_thresh = 1;
			consumed(tag);
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, w78);
			SET_CE(delayedRetrainArmed, w7c);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(avePdsnrNofSymbols, 5u);
			SET_CEF(avePdsnr, 0x3f800000u);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0x00000000u), 1);
			if (w78 != 0 && w7c != 0) {
				diff_eq_int("the delayed retrain fired (%ld)",
					    vb, 4, tag);
				diff_eq_int("+0x78 was cleared (%ld)",
					    (long)CEB->delayedRetrainRequest, 0, tag);
				diff_eq_int("+0x7c was cleared (%ld)",
					    (long)CEB->delayedRetrainArmed, 0, tag);
				diff_eq_int("the counter kept counting (%ld)",
					    (long)CEB->nofV90Retrains, 3, tag);
				p4_delayed = 1;
			} else {
				diff_eq_int("half a request is nothing (%ld)",
					    vb, 0, tag);
				diff_eq_int("+0x78 was left alone (%ld)",
					    (long)CEB->delayedRetrainRequest, (long)w78, tag);
				diff_eq_int("+0x7c was left alone (%ld)",
					    (long)CEB->delayedRetrainArmed, (long)w7c, tag);
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 1u);
			SET_CE(delayedRetrainArmed, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(avePdsnrNofSymbols, 5u);
			SET_CEF(avePdsnr, 0x3f800000u);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0x00000000u), 0);
			diff_eq_int("over the limit it gives up (%ld)", vb, 5,
				    tag);
			diff_eq_int("the counter restarted (%ld)",
				    (long)CEB->nofV90Retrains, 0, tag);
			diff_eq_int("+0x78 was cleared (%ld)",
				    (long)CEB->delayedRetrainRequest, 0, tag);
			diff_eq_int("+0x7c was cleared (%ld)",
				    (long)CEB->delayedRetrainArmed, 0, tag);
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
		 * Finding F1388.
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 1u);
			SET_CE(delayedRetrainArmed, 1u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 10);
			SET_CE(avePdsnrNofSymbols, 50u);
			SET_CEF(avePdsnr, 0x42c80000u);		/* 100.0f */
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(avePdsnrNofSymbols, 13u);
			SET_CEF(avePdsnr, avg_bits[(unsigned)trial % NAVG]);
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, -1);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			for (call = 0; call < 6; call++) {
				int vb;

				SET_CE(avePdsnrNofSymbols, 100u);
				SET_CEF(avePdsnr, 0x42c80000u);	/* 100.0f */
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 10);
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);
			SET_CE(avePdsnrNofSymbols, 20u);
			SET_CEF(avePdsnr, 0x42c80000u);		/* 100.0f */
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
			SET_CE(meanErrorCheckArmed, (short)1);
			SET_CE(delayedRetrainRequest, 0u);
			SET_CE(delayedRetrainArmed, 0u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(avePdsnrNofSymbols, 20u);
			SET_CEF(avePdsnr, 0x40000000u);		/* 2.0f */
			SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);

			vb = p4_call(tag, as_float(0x42c80000u), 1);
			diff_eq_int("a negative second ceiling admits every "
				    "count (%ld)", vb, 4, tag);
			diff_eq_int("so the arm fired and cleared +0xb0 (%ld)",
				    (long)CEB->meanErrorCheckArmed, 0, tag);
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
			SET_CE(meanErrorCheckArmed, (short)0);
			SET_CE(delayedRetrainRequest, 3u);
			SET_CE(delayedRetrainArmed, 4u);
			SET_CE(word_10, 0u);
			SET_CE(word_18, 0u);
			SET_CE(word_64, 100000u);
			SET_CE(word_68, 100000u);
			SET_CE(retrainDetectDuration, 100000);
			SET_CE(avePdsnrNofSymbols, 5u);
			SET_CEF(avePdsnr, 0x3f800000u);		/* 1.0f */
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

/* ------------------------------------- evaluateConnection (3,857 bytes) */

/*
 * WHAT THIS ONE NEEDS THAT THE TWO PHASE EVALUATORS DID NOT
 *
 *   IT ANSWERS SIX THINGS, not three.  0, 1, 2, 3, 4 and 5 all reach %eax,
 *   from twenty-four immediates across two epilogues, and the block at the
 *   bottom of `main` asserts every one was OBSERVED -- findings F149 and F223
 *   are twice over the same failure, a decider that always decides the same
 *   way agreeing with anything.  VERDICT 3 IS THE FRAGILE ONE: it is raised
 *   only by the external-demand arm for +0x8c in {1, 4} and then survives
 *   three later stages any of which would overwrite it, so it is driven with
 *   every one of them deliberately quiet.
 *
 *   THERE ARE TWO EPILOGUES AND ONLY ONE IS ON THE ORDINARY PATH.  0x3e8b7 is
 *   reached from the empty call and from the external-demand default arm --
 *   the only place the answer travels in %ecx -- and that arm RETURNS, so
 *   stages 3 to 5 never run.  The block that drives it arms `debugFallBack`
 *   with a period of one first: a version that fell through instead of
 *   returning would answer 5 and be caught on the spot.
 *
 *   IT WRITES THROUGH THE PARAMETER POINTER, which nothing else in the class
 *   does.  `RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE` takes 2.0f, 0.65f or
 *   1.8f on three different paths, so `guard_intact`'s "the parameter block
 *   was not written" is the wrong check here.  `ec_call` snapshots the block,
 *   runs ours, keeps what ours wrote, RESTORES the block, runs the blob and
 *   compares the two results -- so the claim is that the two sides write it
 *   alike, and `ec_param_written` asserts at the end that some trial really
 *   did write it.
 *
 *   IT ACCUMULATES FIVE COUNTERS, so a block below is one measurement -- set
 *   +0x74 and +0x70, call, compare -- and the runs are up to a dozen of them
 *   with the whole object compared after EVERY one.  +0x20 in particular is a
 *   clock that only the run can move.
 *
 *   THE UNSIGNED READINGS ARE INVISIBLE AT ORDINARY VALUES.  Seven
 *   comparisons are `jb`/`ja` against slots the map calls `int`, and one --
 *   the debug period at 0x3ec92 -- is `jl`, SIGNED, which is what makes +0x24
 *   an `int`.  Each is driven at a negative limit, where the two readings
 *   part company, and the +0x24 one is driven from a negative counter as well
 *   because a negative limit alone does not separate them.
 *
 *   THE FADE COMPARISON IS SIGNED ON AN UNSIGNED DIVISION, which no ordinary
 *   value can show either: `divl` twice and then `jg`.  One trial puts the
 *   clock at 0x7fffffff with a fade count of 1, where the quotient crosses
 *   2^31 and `jg` says "did not advance" while `ja` says it did.
 *
 *   TWO x87 ROUNDINGS ARE REACHABLE AND BOTH ARE DRIVEN.  `avePdsnr *
 *   word_b8` is read three times at 80 bits and 10.0f * 2.3f is 22.99999952,
 *   which prints "22.999" unrounded and "23.000" rounded; and +0x5c reaches
 *   the x87 through `fildll`, so 16777217 stays itself where a round trip
 *   through `float` would make it 16777216 and take a different arm.
 *
 *   NO NaN GOES NEAR `threshUp`.  0x3e933 is `fcomps 0xa0(%edi); jae`, the
 *   complement of `avePdsnr < threshUp` taken without the parity flag, so the
 *   blob runs the rate-up arm for an unordered compare and GCC 13 does not.
 *   NaN IS fed wherever `enableRrnUp` is zero: the other four float
 *   comparisons are `ja`/`jbe`, which agree on both compilers.  It is also
 *   kept away from any path that PRINTS the average, because the sign
 *   character is `sbb` off the carry and reads unordered as '+'.  Finding
 *   F1389.
 */

extern "C" {
int ref_ce_evalconn(void *)
	asm("ref__ZN22V90ConnectionEvaluator18evaluateConnectionEv");
}

static int ec_verdict[6];
static int ec_param_written, ec_epilogue_ecx, ec_epilogue_ebp;
static int ec_empty, ec_quiet, ec_faded, ec_fade_signed, ec_fade_unsigned;
static int ec_echo_23, ec_echo_thirds_only, ec_retrain_at_limit;
static int ec_echo_scaled_cmp, ec_echo_neg_dur, ec_case2_neg_max;
static int ec_verdict5_no_case, ec_ext_no_override, ec_rateup_two_counters;
static int ec_neg_max_4b, ec_no_ratedown_clears;
static int ec_ext_blocked, ec_ext_down, ec_ext_retrain, ec_ext_fallback;
static int ec_ext_override, ec_ext_returned_early, ec_ext_untouched;
static int ec_ext_code[8];
static int ec_rateup, ec_rateup_wait, ec_ratedown, ec_ratedown_wait;
static int ec_case2_retrain, ec_case2_fallback, ec_case2_down;
static int ec_retrain, ec_retrain_fallback, ec_retrain_wait;
static int ec_reneg_forced, ec_reneg_no_fallback, ec_override;
static int ec_echo_152, ec_echo_139, ec_echo_terminal, ec_echo_after3;
static int ec_echo_fire_thirds, ec_echo_fire_fifths, ec_echo_065, ec_echo_18;
static int ec_alt[5], ec_dbg_fallback, ec_dbg_retrain, ec_dbg_up, ec_dbg_down;
static int ec_dbg_chain, ec_dbg_wait;
static int ec_neg_max, ec_neg_retrain_dur, ec_neg_rateup_dur, ec_neg_rateup_min;
static int ec_neg_ratedown_dur, ec_neg_ratedown_min, ec_neg_reneg_limit;
static int ec_neg_period, ec_neg_word24;
static int ec_prod_exact, ec_mindur_exact, ec_nan_fed;

static unsigned char ec_parm_pre[PARM_SLOT];
static unsigned char ec_parm_ours[PARM_SLOT];

/* the guard past the object; the parameter block is compared separately */
static void
ec_guard(long tag)
{
	unsigned o = (unsigned)sizeof(V90ConnectionEvaluator);
	unsigned n = CE_SLOT - o;

	diff_eq_int("ours stored past the object (%ld)",
		    memcmp(ce_a + o, ce_s + o, n) == 0, 1, tag);
	diff_eq_int("the blob stored past the object (%ld)",
		    memcmp(ce_b + o, ce_s + o, n) == 0, 1, tag);
}

static int
ec_call(long tag)
{
	int va, vb;

	memcpy(ec_parm_pre, parm_a, PARM_SLOT);

	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	va = CEA->evaluateConnection();
	memcpy(ec_parm_ours, parm_a, PARM_SLOT);
	memcpy(parm_a, ec_parm_pre, PARM_SLOT);
	vb = ref_ce_evalconn(ce_b);

	dsplib_debug_capture_on = 0;

	diff_eq_obj("after evaluateConnection", V90ConnectionEvaluator, CEA,
		    CEB, tag);
	ec_guard(tag);
	diff_eq_int("the two sides wrote the parameter block alike (%ld)",
		    memcmp(parm_a, ec_parm_ours, PARM_SLOT) == 0, 1, tag);
	diff_eq_int("the verdict matches (%ld)", va, vb, tag);
	diff_eq_int("the verdict is one of the six (%ld)",
		    vb >= 0 && vb <= 5, 1, tag);
	transcript_matches(tag);

	if (dsplib_debug_capture_lines(1) > 0)
		transcripts_seen = 1;
	if (memcmp(parm_a, ec_parm_pre, PARM_SLOT) != 0)
		ec_param_written = 1;
	if (vb >= 0 && vb <= 5)
		ec_verdict[vb] = 1;

	memcpy(parm_b, parm_a, PARM_SLOT);
	return vb;
}

/*
 * Everything the function reads, planted.  The seven fields it must NOT read
 * -- +0x64, +0x68, +0x78, +0x7c, +0x84, +0x88, +0xac, +0xb0, +0xb2 -- are
 * left at their seeded random values, so a reconstruction that read one of
 * them would diverge on the first trial.  Every parameter that has a copy in
 * the object gets 1, which is a value no run below crosses: reading the
 * parameter instead of the copy fires on the first call.
 */
static void
ec_base(int trial)
{
	seed_pair(trial + 400, trial & 3);
	fill_pair(parm_a, parm_b, PARM_SLOT, trial + 7000, trial & 3);

	SET_P(RRN_SILENCE_REQUESTED, 0x00abcdef);
	SET_P(MAX_NOF_V90_RETRAINS, 4);
	SET_P(MAX_NOF_RATES_DIFF_BEFORE_RETRAIN, 9);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 0);
	SET_PF(RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE, 0x11223344u);
	SET_P(MAX_NOF_REMOTE_RETRAINS, 0);

	SET_P(ENABLE_RRN_DOWN, 1);
	SET_P(ENABLE_RRN_UP, 1);
	SET_P(NOF_REMOTE_RATE_RENEG_BEFORE_RETRAIN, 1);
	SET_P(DEBUG_CONNECTION_EVALUATOR_ALTERNATE_DEBUG, 1);
	SET_P(DEBUG_CONNECTION_EVALUATOR_FALL_BACK, 1);
	SET_P(DEBUG_CONNECTION_EVALUATOR_RETRAIN, 1);
	SET_P(DEBUG_CONNECTION_EVALUATOR_RATE_UP, 1);
	SET_P(DEBUG_CONNECTION_EVALUATOR_RATE_DOWN, 1);
	SET_P(RETRAIN_COUNTER_FADE_COUNT, 1);
	SET_P(REMOTE_RRN_COUNTER_FADE_COUNT, 1);
	SET_P(RATE_UP_DETECT_DURATION, 1);
	SET_P(MINIMUM_DURATION_IN_DATA_BEFORE_RRN_UP, 1);
	SET_P(RATE_DOWN_DETECT_DURATION, 1);
	SET_P(MINIMUM_DURATION_IN_DATA_BEFORE_RRN_DOWN, 1);
	SET_P(RETRAIN_DETECT_DURATION, 1);
	SET_P(DEBUG_CONNECTION_EVALUATOR_PERIOD, 1);

	SET_CE(externalDemandCode, -1);
	SET_CE(initDmin, -1);
	SET_CE(curDmin, 0);
	SET_CE(nofV90Retrains, 0);
	SET_CE(nofRemoteRetrains, 0);
	SET_CE(nofRemoteRateReneg, 0);
	SET_CE(word_10, 0);
	SET_CE(word_14, 0);
	SET_CE(word_18, 0);
	SET_CE(word_1c, 0);
	SET_CE(word_20, 0);
	SET_CE(word_24, 0);
	SET_CE(debugAlternateState, 0);
	SET_CE(word_90, 0);
	SET_CE(retrainInsteadOfRateDown, 0);
	SET_CE(word_98, 0);
	SET_CE(echoRrnState, 0);

	SET_CE(enableRrnDown, 0);
	SET_CE(enableRrnUp, 0);
	SET_CE(nofRemoteRateRenegBeforeRetrain, 1000);
	SET_CE(debugAlternateDebug, 0);
	SET_CE(debugFallBack, 0);
	SET_CE(debugRetrain, 0);
	SET_CE(debugRateUp, 0);
	SET_CE(debugRateDown, 0);
	SET_CE(retrainCounterFadeCount, 1000000);
	SET_CE(remoteRrnCounterFadeCount, 1000001);
	SET_CE(rateUpDetectDuration, 300);
	SET_CE(minDurationInDataBeforeRrnUp, 200);
	SET_CE(rateDownDetectDuration, 600);
	SET_CE(minDurationInDataBeforeRrnDown, 400);
	SET_CE(retrainDetectDuration, 800);
	SET_CE(debugPeriod, 500);

	SET_CEF(threshUp, 0x41200000u);		/* 10.0f */
	SET_CEF(threshDown, 0x41700000u);	/* 15.0f */
	SET_CEF(threshRetrain, 0x41a00000u);	/* 20.0f */
	SET_CEF(word_b8, 0x3f800000u);		/*  1.0f */
	SET_CEF(avePdsnr, 0);
	SET_CE(avePdsnrNofSymbols, 0);
}

#define EC_5	0x40a00000u	/*  5.0f, below every threshold      */
#define EC_18	0x41900000u	/* 18.0f, over threshDown only       */
#define EC_25	0x41c80000u	/* 25.0f, over threshRetrain too     */
#define EC_10	0x41200000u	/* 10.0f, the product trial          */
#define EC_12	0x41400000u	/* 12.0f, under threshDown but over
				 * threshDown / 1.52                 */
#define EC_M125	0xc1480000u	/* -12.5f, a '-' sign and 50 decimals */

static int
ec_step(long tag, unsigned int n, unsigned int avg)
{
	SET_CE(avePdsnrNofSymbols, n);
	SET_CEF(avePdsnr, avg);
	return ec_call(tag);
}

static void
ec_is(const char *what, long got, long want, long tag)
{
	diff_eq_int(what, got, want, tag);
}

static void
ec_bits(const char *what, const void *got, unsigned int bits, long tag)
{
	diff_eq_int(what, memcmp(got, &bits, 4) == 0, 1, tag);
}

static void
ec_scenarios(int lvl)
{
	long b = (long)lvl * 100000;
	int v, i;

	/* ---------------------------------------------- the empty call */
	ec_base(1);
	SET_CE(curDmin, 77);
	v = ec_call(b + 1);
	ec_is("the empty call answers 0 (%ld)", v, 0, b + 1);
	ec_is("and latched initDmin anyway (%ld)", (long)CEB->initDmin, 77,
	      b + 1);
	ec_is("and printed nothing (%ld)",
	      (long)dsplib_debug_capture_lines(1), 0, b + 1);
	ec_empty = 1;

	/* an empty call with initDmin already set leaves it alone */
	ec_base(2);
	SET_CE(curDmin, 77);
	SET_CE(initDmin, 12);
	v = ec_call(b + 2);
	ec_is("initDmin latches only once (%ld)", (long)CEB->initDmin, 12,
	      b + 2);

	/* ------------------------------------------- a call with nothing on */
	ec_base(3);
	v = ec_step(b + 3, 100, EC_5);
	ec_is("a quiet call answers 0 (%ld)", v, 0, b + 3);
	ec_is("the average was consumed (%ld)", (long)CEB->avePdsnrNofSymbols, 0, b + 3);
	ec_bits("the average was cleared (%ld)", &CEB->avePdsnr, 0u, b + 3);
	ec_is("+0x1c counted the symbols (%ld)", (long)CEB->word_1c, 100,
	      b + 3);
	ec_is("+0x20 advanced (%ld)", (long)CEB->word_20, 100, b + 3);
	ec_quiet = 1;
	ec_epilogue_ebp = 1;		/* it ran to 0x3ed18 */

	/* ---------------------------------------------- 1: the fade clock */
	ec_base(4);
	SET_CE(retrainCounterFadeCount, 100);
	SET_CE(remoteRrnCounterFadeCount, 250);
	SET_CE(nofV90Retrains, 3);
	SET_CE(nofRemoteRetrains, 3);
	SET_CE(nofRemoteRateReneg, 3);
	for (i = 0; i < 12; i++)
		ec_step(b + 40 + i, 60, EC_5);
	ec_is("the fade clock ran (%ld)", (long)CEB->word_20, 720, b + 60);
	ec_is("+0x04 faded out (%ld)", (long)CEB->nofV90Retrains, 0, b + 60);
	ec_is("+0x0c faded on the SAME count (%ld)",
	      (long)CEB->nofRemoteRetrains, 0, b + 60);
	ec_is("+0x08 faded on the OTHER count (%ld)",
	      (long)CEB->nofRemoteRateReneg, 1, b + 60);
	ec_faded = 1;

	/* the quotient comparison is SIGNED: at 2^31 it says "no advance" */
	ec_base(5);
	SET_CE(retrainCounterFadeCount, 1);
	SET_CE(word_20, 0x7fffffffu);
	SET_CE(nofV90Retrains, 3);
	ec_step(b + 61, 100, EC_5);
	ec_is("the fade quotient is compared signed (%ld)",
	      (long)CEB->nofV90Retrains, 3, b + 61);
	ec_fade_signed = 1;

	/*
	 * And the division itself is UNSIGNED, which is a separate claim: at a
	 * clock of 2^31 and a fade count of 2^30 the unsigned quotients are
	 * 2 and 2 and the signed ones are -2 and -1, so `divl` says the clock
	 * did not advance and `idivl` says it did.
	 */
	ec_base(66);
	SET_CE(retrainCounterFadeCount, 0x40000000);
	SET_CE(word_20, 0x80000000u);
	SET_CE(nofV90Retrains, 3);
	ec_step(b + 62, 100, EC_5);
	ec_is("the fade division is unsigned (%ld)",
	      (long)CEB->nofV90Retrains, 3, b + 62);
	ec_fade_unsigned = 1;

	/* --------------------------------------- 2: every +0x8c code */
	{
		static const int code[9] = { -2, 0, 1, 2, 3, 4, 5, 6, 7 };
		static const int want[9] = {  0, 0, 3, 2, 1, 3, 2, 0, 0 };

		for (i = 0; i < 9; i++) {
			long t = b + 70 + i;

			ec_base(6 + i);
			SET_CE(externalDemandCode, code[i]);
			SET_CE(initDmin, 10);
			SET_CE(curDmin, 5);
			v = ec_step(t, 100, EC_5);
			ec_is("the external demand decided (%ld)", v, want[i],
			      t);
			ec_is("+0x8c was acknowledged (%ld)",
			      (long)CEB->externalDemandCode, code[i] > -1 ? -1 : code[i],
			      t);
			if (code[i] >= 0 && code[i] <= 7)
				ec_ext_code[code[i] > 7 ? 7 : code[i]] = 1;
			if (code[i] == -2)
				ec_ext_untouched = 1;
		}
		/*
		 * +0x90 and +0x98 are what the code 5 arm computes -- and
		 * initDmin has to be planted, because code 5 raises verdict 2
		 * and stage 4d then runs the rate-down arm, which clears
		 * +0x90 outright on the path where the distance has doubled.
		 * Left at -1 the entry latch makes initDmin equal to curDmin
		 * and `curDmin >= 2 * initDmin` is 0 >= 0, which is that path.
		 */
		ec_base(20);
		SET_CE(externalDemandCode, 5);
		SET_CE(initDmin, 10);
		SET_CE(curDmin, 5);
		ec_step(b + 80, 100, EC_5);
		ec_is("+0x90 is (externalDemandCode > 3) (%ld)", (long)CEB->word_90, 1,
		      b + 80);
		ec_is("+0x98 is (externalDemandCode == 5) (%ld)", (long)CEB->word_98, 1,
		      b + 80);
		ec_base(21);
		SET_CE(externalDemandCode, 4);
		ec_step(b + 81, 100, EC_5);
		ec_is("+0x90 is 1 for code 4 (%ld)", (long)CEB->word_90, 1,
		      b + 81);
		ec_is("+0x98 is 0 for code 4 (%ld)", (long)CEB->word_98, 0,
		      b + 81);
		ec_base(22);
		SET_CE(externalDemandCode, 1);
		ec_step(b + 82, 100, EC_5);
		ec_is("+0x90 is 0 for code 1 (%ld)", (long)CEB->word_90, 0,
		      b + 82);
	}

	/* -------------------- 2b: the default arm, which has its own return */

	/* blocked, because the object is already at its lowest rate */
	ec_base(23);
	SET_CE(externalDemandCode, 0);
	SET_CE(enableRrnDown, 0);
	v = ec_step(b + 90, 100, EC_5);
	ec_is("the blocked arm answers 0 (%ld)", v, 0, b + 90);
	ec_ext_blocked = 1;

	/*
	 * One rate down, with `debugFallBack` armed and a period of one: the
	 * arm RETURNS, so stage 5 never runs and the answer stays 2.
	 */
	ec_base(24);
	SET_CE(externalDemandCode, 7);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 5);
	SET_CE(debugFallBack, 1);
	SET_CE(debugPeriod, 1);
	v = ec_step(b + 91, 100, EC_5);
	ec_is("the EC rate down answers 2 (%ld)", v, 2, b + 91);
	ec_is("and stage 5 did not run (%ld)", (long)CEB->word_24, 0, b + 91);
	ec_is("+0x90 took RRN_SILENCE_REQUESTED (%ld)", (long)CEB->word_90,
	      0x00abcdef, b + 91);
	ec_is("+0x8c was acknowledged (%ld)", (long)CEB->externalDemandCode, -1, b + 91);
	ec_ext_down = 1;
	ec_ext_returned_early = 1;
	ec_epilogue_ecx = 1;

	/*
	 * A CODE 3 THAT SURVIVES TO A VERDICT 5, which is the only way the
	 * arm's own `+0x90 = 0` is observable: verdict 1 always runs the
	 * rate-up case, which clears +0x90 again, and the retrain and
	 * rate-down cases write it too -- but 5 has no case at all.
	 */
	ec_base(71);
	SET_CE(externalDemandCode, 3);
	SET_CE(retrainDetectDuration, 100);
	SET_CE(nofV90Retrains, 4);
	v = ec_step(b + 96, 300, EC_25);
	ec_is("a code 3 that becomes a fall-back (%ld)", v, 5, b + 96);
	ec_is("and +0x90 is what the code 3 arm left (%ld)",
	      (long)CEB->word_90, 0, b + 96);
	ec_verdict5_no_case = 1;

	/* the V42 override needs verdict 2, not merely +0x94 */
	ec_base(72);
	SET_CE(externalDemandCode, 0);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 25);
	SET_CE(retrainInsteadOfRateDown, 1);
	v = ec_step(b + 97, 100, EC_5);
	ec_is("the V42 retrain is not overridden (%ld)", v, 4, b + 97);
	ec_ext_no_override = 1;

	/* the same, overridden by +0x94 */
	ec_base(25);
	SET_CE(externalDemandCode, 0);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 5);
	SET_CE(retrainInsteadOfRateDown, 1);
	v = ec_step(b + 92, 100, EC_5);
	ec_is("+0x94 turns the rate down into a retrain (%ld)", v, 4, b + 92);
	ec_is("and clears +0x90 (%ld)", (long)CEB->word_90, 0, b + 92);
	ec_ext_override = 1;

	/* the distance doubled: a retrain instead */
	ec_base(26);
	SET_CE(externalDemandCode, 0);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 25);
	v = ec_step(b + 93, 100, EC_5);
	ec_is("the doubled distance retrains (%ld)", v, 4, b + 93);
	ec_is("and counted one (%ld)", (long)CEB->nofV90Retrains, 1, b + 93);
	ec_is("and re-armed initDmin (%ld)", (long)CEB->initDmin, -1, b + 93);
	ec_ext_retrain = 1;

	/* and once too often, a fall-back */
	ec_base(27);
	SET_CE(externalDemandCode, 0);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 25);
	SET_CE(nofV90Retrains, 4);
	v = ec_step(b + 94, 100, EC_5);
	ec_is("the fifth retrain falls back (%ld)", v, 5, b + 94);
	ec_is("and restarted the count (%ld)", (long)CEB->nofV90Retrains, 0,
	      b + 94);
	ec_ext_fallback = 1;

	/* the limit is UNSIGNED: a negative one is never exceeded */
	ec_base(28);
	SET_CE(externalDemandCode, 0);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 25);
	SET_CE(nofV90Retrains, 4);
	SET_P(MAX_NOF_V90_RETRAINS, -1);
	v = ec_step(b + 95, 100, EC_5);
	ec_is("a negative retrain limit is never exceeded (%ld)", v, 4,
	      b + 95);
	ec_neg_max = 1;

	/*
	 * 3: rate up.  THE AVERAGE IS NEGATIVE ON PURPOSE -- the arm's own
	 * diagnostic prints it as `%c%d.%02d`, and the decimals go through an
	 * integer absolute value in the object, so -12.5f is the only kind of
	 * input that can tell `abs` from no `abs`.
	 */
	ec_base(30);
	SET_CE(enableRrnUp, 1);
	for (i = 0; i < 4; i++) {
		long t = b + 100 + i;

		v = ec_step(t, 100, EC_M125);
		if (i < 2) {
			ec_is("the rate up is still waiting (%ld)", v, 0, t);
			ec_is("and +0x10 accumulated (%ld)", (long)CEB->word_10,
			      100 * (i + 1), t);
			ec_rateup_wait = 1;
		} else if (i == 2) {
			ec_is("the rate up fired (%ld)", v, 1, t);
			ec_is("and +0x10 restarted (%ld)", (long)CEB->word_10,
			      0, t);
			ec_is("and +0x1c restarted (%ld)", (long)CEB->word_1c,
			      0, t);
			ec_is("and +0x90 was cleared (%ld)",
			      (long)CEB->word_90, 0, t);
			ec_rateup = 1;
		}
	}

	/* the average at or above threshUp clears +0x10 instead */
	ec_base(31);
	SET_CE(enableRrnUp, 1);
	SET_CE(word_10, 250);
	ec_step(b + 110, 100, EC_18);
	ec_is("an average over threshUp clears +0x10 (%ld)",
	      (long)CEB->word_10, 0, b + 110);

	/* both rate-up limits are unsigned */
	ec_base(32);
	SET_CE(enableRrnUp, 1);
	SET_CE(rateUpDetectDuration, -1);
	SET_CE(word_1c, 5000);
	v = ec_step(b + 111, 100, EC_5);
	ec_is("a negative rate-up duration never fires (%ld)", v, 0, b + 111);
	ec_neg_rateup_dur = 1;

	ec_base(33);
	SET_CE(enableRrnUp, 1);
	SET_CE(minDurationInDataBeforeRrnUp, -1);
	SET_CE(word_10, 5000);
	v = ec_step(b + 112, 100, EC_5);
	ec_is("a negative rate-up dwell never fires (%ld)", v, 0, b + 112);
	ec_neg_rateup_min = 1;

	/*
	 * THE DWELL IS ON +0x1c AND THE DURATION ON +0x10, which the ordinary
	 * run cannot separate because both counters reach 300 together.  Here
	 * they are 350 and 600 against limits of 300 and 400.
	 */
	ec_base(73);
	SET_CE(enableRrnUp, 1);
	SET_CE(minDurationInDataBeforeRrnUp, 400);
	SET_CE(word_10, 250);
	SET_CE(word_1c, 500);
	v = ec_step(b + 113, 100, EC_5);
	ec_is("the two rate-up counters are different counters (%ld)", v, 1,
	      b + 113);
	ec_rateup_two_counters = 1;

	/* ----------------------------------------- 4: the plain rate down */
	ec_base(34);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 5);
	for (i = 0; i < 4; i++) {
		long t = b + 120 + i;

		v = ec_step(t, 200, EC_18);
		if (i < 2) {
			ec_is("the rate down is still waiting (%ld)", v, 0, t);
			ec_ratedown_wait = 1;
		} else if (i == 2) {
			ec_is("the rate down fired (%ld)", v, 2, t);
			ec_is("+0x90 took the parameter (%ld)",
			      (long)CEB->word_90, 0x00abcdef, t);
			ec_is("+0x98 was cleared (%ld)", (long)CEB->word_98, 0,
			      t);
			ec_ratedown = 1;
			ec_case2_down = 1;
		}
	}

	/* an average at or below threshDown clears +0x14 */
	ec_base(35);
	SET_CE(enableRrnDown, 1);
	SET_CE(word_14, 500);
	ec_step(b + 130, 100, EC_5);
	ec_is("an average under threshDown clears +0x14 (%ld)",
	      (long)CEB->word_14, 0, b + 130);

	/* the rate down turning into a retrain, and then a fall-back */
	ec_base(36);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 25);
	SET_CE(word_14, 500);
	SET_CE(word_1c, 500);
	v = ec_step(b + 131, 200, EC_18);
	ec_is("the rate down retrains when the distance doubled (%ld)", v, 4,
	      b + 131);
	ec_is("and re-armed initDmin (%ld)", (long)CEB->initDmin, -1, b + 131);
	ec_case2_retrain = 1;

	ec_base(37);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 25);
	SET_CE(word_14, 500);
	SET_CE(word_1c, 500);
	SET_CE(nofV90Retrains, 4);
	v = ec_step(b + 132, 200, EC_18);
	ec_is("and falls back once too often (%ld)", v, 5, b + 132);
	ec_case2_fallback = 1;

	/* the rate-down arm's own copy of the unsigned limit */
	ec_base(70);
	SET_CE(enableRrnDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 25);
	SET_CE(word_14, 500);
	SET_CE(word_1c, 500);
	SET_CE(nofV90Retrains, 4);
	SET_P(MAX_NOF_V90_RETRAINS, -1);
	v = ec_step(b + 135, 200, EC_18);
	ec_is("the rate-down retrain limit is unsigned too (%ld)", v, 4,
	      b + 135);
	ec_case2_neg_max = 1;

	/* both rate-down limits are unsigned */
	ec_base(38);
	SET_CE(enableRrnDown, 1);
	SET_CE(rateDownDetectDuration, -1);
	SET_CE(word_1c, 5000);
	v = ec_step(b + 133, 200, EC_18);
	ec_is("a negative rate-down duration never fires (%ld)", v, 0,
	      b + 133);
	ec_neg_ratedown_dur = 1;

	ec_base(39);
	SET_CE(enableRrnDown, 1);
	SET_CE(minDurationInDataBeforeRrnDown, -1);
	SET_CE(word_14, 5000);
	v = ec_step(b + 134, 200, EC_18);
	ec_is("a negative rate-down dwell never fires (%ld)", v, 0, b + 134);
	ec_neg_ratedown_min = 1;

	/* ------------------------------------------- 4: the echo-RRN state */
	ec_base(40);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	SET_CE(enableRrnDown, 0);	/* the echo arm ignores it */
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 5);
	for (i = 0; i < 7; i++) {
		long t = b + 140 + i;

		v = ec_step(t, 200, EC_18);
		if (i == 0) {
			ec_bits("state 0 scales by 1.52 (%ld)", &CEB->word_b8,
				0x3fc28f5cu, t);
			ec_echo_152 = 1;
		} else if (i == 2) {
			ec_is("the scaled rate down fired (%ld)", v, 2, t);
			ec_is("and advanced the state (%ld)",
			      (long)CEB->echoRrnState, 1, t);
			ec_bits("and set the keep-rate energy to 0.65 (%ld)",
				&PA->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE,
				0x3f266666u, t);
			ec_echo_fire_thirds = 1;
			ec_echo_065 = 1;
		} else if (i == 3) {
			ec_bits("state 1 scales by 1.39 (%ld)", &CEB->word_b8,
				0x3fb1eb85u, t);
			ec_echo_139 = 1;
		} else if (i == 4) {
			ec_is("the dwell ran out and the state went to 3 "
			      "(%ld)", (long)CEB->echoRrnState, 3, t);
			ec_bits("and the keep-rate energy went to 2.0 (%ld)",
				&PA->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE,
				0x40000000u, t);
			ec_echo_terminal = 1;
		} else if (i == 6) {
			ec_is("state 3 uses the unscaled test (%ld)", v, 2, t);
			ec_echo_after3 = 1;
		}
	}

	/*
	 * THE 2.3 IN THE FIRST GUARD, on its own trial: +0x1c at 900 is under
	 * 400 * 2.3f (which truncates to 919) and over 400 * 2.0f, so the
	 * multiplier decides between the 1.52 scale and the terminal arm.
	 */
	ec_base(64);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	SET_CE(word_1c, 850);
	ec_step(b + 153, 50, EC_18);
	ec_bits("the 2.3 guard kept state 0 (%ld)", &CEB->word_b8,
		0x3fc28f5cu, b + 153);
	ec_is("and the state did not go terminal (%ld)", (long)CEB->echoRrnState,
	      0, b + 153);
	ec_echo_23 = 1;

	/*
	 * THE THIRDS CONDITION ON ITS OWN: +0x14 at 210 is over 600/3 and
	 * under 600/2, and state 0 keeps the fifths alternative shut, so this
	 * is the only trial where a third really is a third.
	 */
	ec_base(65);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	SET_CE(word_1c, 400);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 5);
	v = ec_step(b + 154, 210, EC_18);
	ec_is("a third of the duration fired it (%ld)", v, 2, b + 154);
	ec_is("and advanced the state (%ld)", (long)CEB->echoRrnState, 1, b + 154);
	ec_echo_thirds_only = 1;

	/* the second fire condition: a fifth of the duration, in state 1 */
	ec_base(41);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	SET_CE(echoRrnState, 1);
	SET_CE(word_1c, 250);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 5);
	v = ec_step(b + 150, 130, EC_18);
	ec_is("the fifths condition fired (%ld)", v, 2, b + 150);
	ec_is("and advanced the state to 2 (%ld)", (long)CEB->echoRrnState, 2,
	      b + 150);
	ec_bits("and set the keep-rate energy to 1.8 (%ld)",
		&PA->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE, 0x3fe66666u,
		b + 150);
	ec_echo_fire_fifths = 1;
	ec_echo_18 = 1;

	/*
	 * +0x5c REACHES THE x87 EXACTLY.  16777217 is the first integer a
	 * `float` cannot hold; the object's `fildll` keeps it, so state 1 with
	 * +0x1c exactly 16777216 takes the 1.39 arm.  A round trip through
	 * `float` would make the two equal and take the terminal arm instead.
	 */
	ec_base(42);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	SET_CE(echoRrnState, 1);
	SET_CE(minDurationInDataBeforeRrnDown, 16777217);
	SET_CE(word_1c, 16777116);
	SET_CE(word_14, 0);
	ec_step(b + 151, 100, EC_18);
	ec_bits("+0x5c is not rounded through float (%ld)", &CEB->word_b8,
		0x3fb1eb85u, b + 151);
	ec_is("and the state did not go terminal (%ld)", (long)CEB->echoRrnState,
	      1, b + 151);
	ec_mindur_exact = 1;

	/*
	 * THE PRODUCT IS NOT ROUNDED EITHER.  10.0f * 2.3f is 22.99999952 at
	 * 80 bits and 23.0 at 32, and the terminal arm's diagnostic prints
	 * `(int)` of it and its first three decimals.
	 */
	ec_base(43);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	SET_CE(echoRrnState, 2);
	SET_CEF(word_b8, 0x40133333u);		/* 2.3f */
	ec_step(b + 152, 100, EC_10);
	ec_is("the terminal arm ran (%ld)", (long)CEB->echoRrnState, 3, b + 152);
	if (lvl > 1)
		ec_prod_exact = 1;

	/* and the same diagnostic on a negative product, for the `abs` */
	ec_base(67);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	SET_CE(echoRrnState, 2);
	ec_step(b + 155, 100, EC_M125);
	ec_is("the terminal arm ran on a negative average (%ld)",
	      (long)CEB->echoRrnState, 3, b + 155);

	/*
	 * THE COMPARISON IS AGAINST THE SCALED AVERAGE.  12.0f is under
	 * `threshDown` and 12.0f * 1.52f is over it, so this is the only
	 * trial where dropping the scale changes the answer.
	 */
	ec_base(68);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	ec_step(b + 156, 100, EC_12);
	ec_is("the scaled average is what is compared (%ld)",
	      (long)CEB->word_14, 100, b + 156);
	ec_echo_scaled_cmp = 1;

	/*
	 * THE THIRDS AND FIFTHS DIVISIONS ARE UNSIGNED.  A duration of -1
	 * gives 1431655765 unsigned and 0 signed, so a signed divide makes
	 * "+0x14 has reached a third of it" true on the first call.
	 */
	ec_base(69);
	SET_P(HIGH_LEVEL_TX_ACTIVE, 1);
	SET_CE(rateDownDetectDuration, -1);
	SET_CE(word_1c, 600);
	v = ec_step(b + 157, 50, EC_18);
	ec_is("a negative echo duration never fires (%ld)", v, 0, b + 157);
	ec_echo_neg_dur = 1;

	/* ------------------------------------------------------ 4b: retrain */
	ec_base(44);
	SET_CE(retrainDetectDuration, 900);	/* the third call hits it
						 * exactly, so `>=` fires and
						 * `>` does not */
	for (i = 0; i < 3; i++) {
		long t = b + 160 + i;

		v = ec_step(t, 300, EC_25);
		if (i < 2) {
			ec_is("the retrain is still waiting (%ld)", v, 0, t);
			ec_is("and +0x18 accumulated (%ld)", (long)CEB->word_18,
			      300 * (i + 1), t);
			ec_retrain_wait = 1;
		} else {
			ec_is("the retrain fired (%ld)", v, 4, t);
			ec_is("and counted one (%ld)",
			      (long)CEB->nofV90Retrains, 1, t);
			ec_is("and +0x18 restarted (%ld)", (long)CEB->word_18,
			      0, t);
			ec_is("and initDmin was re-armed (%ld)",
			      (long)CEB->initDmin, -1, t);
			ec_retrain = 1;
		}
	}

	ec_base(45);
	SET_CE(nofV90Retrains, 4);
	SET_CE(word_18, 600);
	v = ec_step(b + 170, 300, EC_25);
	ec_is("the retrain falls back once too often (%ld)", v, 5, b + 170);
	ec_retrain_fallback = 1;

	/* the limit is `>` and not `>=`: a count that reaches it is allowed */
	ec_base(63);
	SET_CE(nofV90Retrains, 3);
	SET_CE(word_18, 600);
	v = ec_step(b + 173, 300, EC_25);
	ec_is("reaching the limit is not exceeding it (%ld)", v, 4, b + 173);
	ec_is("and the count stands (%ld)", (long)CEB->nofV90Retrains, 4,
	      b + 173);
	ec_retrain_at_limit = 1;

	ec_base(46);
	SET_CE(retrainDetectDuration, -1);
	SET_CE(word_18, 600);
	v = ec_step(b + 171, 300, EC_25);
	ec_is("a negative retrain duration never fires (%ld)", v, 0, b + 171);
	ec_neg_retrain_dur = 1;

	/* the 4b arm has its own copy of the unsigned limit */
	ec_base(74);
	SET_CE(retrainDetectDuration, 100);
	SET_CE(nofV90Retrains, 4);
	SET_P(MAX_NOF_V90_RETRAINS, -1);
	v = ec_step(b + 174, 300, EC_25);
	ec_is("the 4b retrain limit is unsigned too (%ld)", v, 4, b + 174);
	ec_neg_max_4b = 1;

	/* +0x14 is cleared when neither rate-down arm is armed */
	ec_base(75);
	SET_CE(word_14, 500);
	ec_step(b + 175, 100, EC_5);
	ec_is("+0x14 is cleared with no rate down armed (%ld)",
	      (long)CEB->word_14, 0, b + 175);
	ec_no_ratedown_clears = 1;

	/* an average at or below threshRetrain clears +0x18 */
	ec_base(47);
	SET_CE(word_18, 600);
	ec_step(b + 172, 300, EC_18);
	ec_is("an average under threshRetrain clears +0x18 (%ld)",
	      (long)CEB->word_18, 0, b + 172);

	/* ------------------------------- 4c: too many renegotiations */
	ec_base(48);
	SET_CE(nofRemoteRateReneg, 3);
	SET_CE(nofRemoteRateRenegBeforeRetrain, 3);
	SET_CE(nofV90Retrains, 10);
	v = ec_step(b + 180, 100, EC_5);
	ec_is("the renegotiation count forces a retrain (%ld)", v, 4, b + 180);
	ec_is("and does NOT check the retrain limit (%ld)",
	      (long)CEB->nofV90Retrains, 11, b + 180);
	ec_is("and restarted the renegotiation count (%ld)",
	      (long)CEB->nofRemoteRateReneg, 0, b + 180);
	ec_reneg_forced = 1;
	ec_reneg_no_fallback = 1;

	ec_base(49);
	SET_CE(nofRemoteRateReneg, 3);
	SET_CE(nofRemoteRateRenegBeforeRetrain, -1);
	v = ec_step(b + 181, 100, EC_5);
	ec_is("a negative renegotiation limit never fires (%ld)", v, 0,
	      b + 181);
	ec_neg_reneg_limit = 1;

	/* -------------------------------------- 4d: the +0x94 override */
	ec_base(50);
	SET_CE(enableRrnDown, 1);
	SET_CE(retrainInsteadOfRateDown, 1);
	SET_CE(initDmin, 10);
	SET_CE(curDmin, 5);
	SET_CE(word_14, 500);
	SET_CE(word_1c, 500);
	v = ec_step(b + 190, 200, EC_18);
	ec_is("+0x94 overrides a rate down (%ld)", v, 4, b + 190);
	ec_is("and clears +0x90 (%ld)", (long)CEB->word_90, 0, b + 190);
	ec_override = 1;

	/* ---------------------------------------------- 5: the debug arms */
	{
		static const int want[10] = { 0, 4, 0, 1, 0, 4, 0, 2, 0, 4 };

		ec_base(51);
		SET_CE(debugAlternateDebug, 1);
		for (i = 0; i < 10; i++) {
			long t = b + 200 + i;

			v = ec_step(t, 300, EC_5);
			ec_is("the alternate debug arm cycles (%ld)", v,
			      want[i], t);
			if (i == 1)
				ec_alt[0] = 1;
			if (i == 3)
				ec_alt[1] = 1;
			if (i == 5)
				ec_alt[2] = 1;
			if (i == 7) {
				ec_alt[3] = 1;
				ec_is("the RRN down arm took the parameter "
				      "(%ld)", (long)CEB->word_90, 0x00abcdef,
				      t);
				ec_is("and cleared +0x98 (%ld)",
				      (long)CEB->word_98, 0, t);
			}
			if (i == 0)
				ec_dbg_wait = 1;
		}

		/* +0x80 out of range: the switch does nothing at all */
		ec_base(52);
		SET_CE(debugAlternateDebug, 1);
		SET_CE(debugAlternateState, 4);
		SET_CE(word_24, 400);
		v = ec_step(b + 210, 300, EC_5);
		ec_is("an out-of-range +0x80 decides nothing (%ld)", v, 0,
		      b + 210);
		ec_is("and does not even restart +0x24 (%ld)",
		      (long)CEB->word_24, 700, b + 210);
		ec_alt[4] = 1;
	}

	ec_base(53);
	SET_CE(debugFallBack, 1);
	SET_CE(word_24, 400);
	SET_CE(debugPeriod, 600);	/* reached exactly: `>=` fires, `>`
					 * does not */
	v = ec_step(b + 211, 200, EC_5);
	ec_is("the debug fall back (%ld)", v, 5, b + 211);
	ec_is("and restarted +0x24 (%ld)", (long)CEB->word_24, 0, b + 211);
	ec_dbg_fallback = 1;

	ec_base(54);
	SET_CE(debugRetrain, 1);
	SET_CE(word_24, 400);
	v = ec_step(b + 212, 300, EC_5);
	ec_is("the debug retrain (%ld)", v, 4, b + 212);
	ec_dbg_retrain = 1;

	ec_base(55);
	SET_CE(debugRateUp, 1);
	SET_CE(word_24, 400);
	v = ec_step(b + 213, 300, EC_5);
	ec_is("the debug rate up (%ld)", v, 1, b + 213);
	ec_dbg_up = 1;

	ec_base(56);
	SET_CE(debugRateDown, 1);
	SET_CE(word_24, 400);
	v = ec_step(b + 214, 300, EC_5);
	ec_is("the debug rate down (%ld)", v, 2, b + 214);
	ec_is("and took the parameter (%ld)", (long)CEB->word_90, 0x00abcdef,
	      b + 214);
	ec_dbg_down = 1;

	/* the five are an else-if chain: the first non-zero flag wins */
	ec_base(57);
	SET_CE(debugFallBack, 1);
	SET_CE(debugRetrain, 1);
	SET_CE(debugRateUp, 1);
	SET_CE(debugRateDown, 1);
	SET_CE(word_24, 400);
	v = ec_step(b + 215, 300, EC_5);
	ec_is("the debug arms are an else-if chain (%ld)", v, 5, b + 215);
	ec_dbg_chain = 1;

	/*
	 * +0x24 IS SIGNED AND SO IS THE PERIOD.  A negative period fires at
	 * once signed and never unsigned; a negative +0x24 is the other way
	 * round.  Nothing else in the function separates `jl` from `jb`.
	 */
	ec_base(58);
	SET_CE(debugRetrain, 1);
	SET_CE(debugPeriod, -1);
	v = ec_step(b + 216, 100, EC_5);
	ec_is("a negative debug period fires at once (%ld)", v, 4, b + 216);
	ec_neg_period = 1;

	ec_base(59);
	SET_CE(debugRetrain, 1);
	SET_CE(debugPeriod, 100);
	SET_CE(word_24, -1000);
	v = ec_step(b + 217, 100, EC_5);
	ec_is("a negative +0x24 does not (%ld)", v, 0, b + 217);
	ec_is("and it stayed negative (%ld)", (long)CEB->word_24, -900,
	      b + 217);
	ec_neg_word24 = 1;
}

/*
 * The accumulating run: a dozen configurations driven for a dozen
 * measurements each with the object compared after every one, so a
 * divergence at the fourth that the tenth washes out is still caught.
 * `enableRrnUp` is zero throughout the NaN half; see the note above.
 */
static void
ec_sweep(int lvl)
{
	static const unsigned int avg[10] = {
		0x40a00000u, 0x41900000u, 0x41c80000u, 0x00000000u,
		0x80000000u, 0x00000001u, 0xc1a00000u, 0x42fe0000u,
		0x3dcccccdu, 0xbdcccccdu
	};
	static const unsigned int nan_avg[4] = {
		0x7fc00000u, 0xffc00000u, 0x7f800000u, 0xff800000u
	};
	long b = (long)lvl * 100000 + 500;
	int cfg, i;

	for (cfg = 0; cfg < 12; cfg++) {
		ec_base(60 + cfg);
		SET_CE(enableRrnUp, (cfg & 1) ? 1 : 0);
		SET_CE(enableRrnDown, (cfg & 2) ? 1 : 0);
		SET_P(HIGH_LEVEL_TX_ACTIVE, (cfg & 4) ? 1 : 0);
		SET_CE(retrainInsteadOfRateDown, (cfg == 5) ? 1 : 0);
		SET_CE(retrainCounterFadeCount, 700 + cfg);
		SET_CE(remoteRrnCounterFadeCount, 900 + cfg);
		SET_CE(nofV90Retrains, cfg % 5);
		SET_CE(nofRemoteRetrains, cfg % 3);
		SET_CE(nofRemoteRateReneg, cfg % 4);
		SET_CE(nofRemoteRateRenegBeforeRetrain, 6);
		SET_CE(initDmin, 8 + cfg);
		SET_CE(curDmin, 3 * cfg);
		SET_CE(debugAlternateDebug, (cfg == 9) ? 1 : 0);
		SET_CE(debugRateDown, (cfg == 10) ? 1 : 0);
		SET_CE(debugPeriod, 900);
		SET_CE(rateUpDetectDuration, 250 + 10 * cfg);
		SET_CE(rateDownDetectDuration, 450 + 10 * cfg);
		SET_CE(retrainDetectDuration, 650 + 10 * cfg);
		SET_CE(minDurationInDataBeforeRrnUp, 120 + cfg);
		SET_CE(minDurationInDataBeforeRrnDown, 220 + cfg);

		for (i = 0; i < 12; i++) {
			long t = b + cfg * 20 + i;
			unsigned int a;

			if ((cfg & 1) == 0 && i == 6) {
				a = nan_avg[cfg % 4];
				ec_nan_fed = 1;
			} else {
				a = avg[(cfg * 3 + i) % 10];
			}
			if (i == 4)
				SET_CE(externalDemandCode, (cfg % 9) - 1);
			ec_step(t, 70u + 23u * (unsigned)i, a);
		}
	}
}

static int
run_ce_evalconn(void)
{
	int lvl;

	diff_begin("V90ConnectionEvaluator::evaluateConnection");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned)lvl);
		ec_scenarios(lvl);
		ec_sweep(lvl);
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
	rc |= run_ce_evalconn();

	set_level(0);
	dsplib_debug_capture_on = 0;

	/*
	 * THE DECISION WAS MADE BOTH WAYS.  An evaluator that always returns
	 * the same verdict agrees with the blob on every trial and proves
	 * nothing (findings F149, F223), so the two outcomes of each of the two
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
	/*
	 * The one site in this file a NaN can reach: the replacement
	 * threshold's own sign report.  Finding F2410.
	 */
	diff_eq_int("phase4: an unordered unnamed_434 reached the sign printer",
		    p4_nan_thresh, 1, 0);
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

	/*
	 * `evaluateConnection` ANSWERS SIX THINGS AND HAS FIVE STAGES.  Every
	 * one of the six is asserted to have been observed, and so is every
	 * arm the verdict alone cannot separate -- an evaluator that always
	 * answered the same thing would agree with the blob on all of it
	 * (findings F149 and F223), and an assertion that fails here means the
	 * fixture stopped reaching an arm and the checks that looked green
	 * were vacuous.
	 */
	diff_eq_int("evaluateConnection answered 0", ec_verdict[0], 1, 0);
	diff_eq_int("evaluateConnection answered 1", ec_verdict[1], 1, 0);
	diff_eq_int("evaluateConnection answered 2", ec_verdict[2], 1, 0);
	diff_eq_int("evaluateConnection answered 3", ec_verdict[3], 1, 0);
	diff_eq_int("evaluateConnection answered 4", ec_verdict[4], 1, 0);
	diff_eq_int("evaluateConnection answered 5", ec_verdict[5], 1, 0);
	diff_eq_int("and the six are six different numbers",
		    V90CE_VERDICT_NONE == 0 && V90CE_VERDICT_RRN_UP == 1
		    && V90CE_VERDICT_RRN_DOWN == 2
		    && V90CE_VERDICT_RRN_NO_RESTRICT == 3
		    && V90CE_VERDICT_RETRAIN == 4
		    && V90CE_VERDICT_FALLBACK_V34 == 5, 1, 0);

	diff_eq_int("evalconn: both epilogues were reached",
		    ec_epilogue_ecx && ec_epilogue_ebp, 1, 0);
	diff_eq_int("evalconn: the parameter block really was written",
		    ec_param_written, 1, 0);
	diff_eq_int("evalconn: the empty call", ec_empty, 1, 0);
	diff_eq_int("evalconn: a call with nothing armed", ec_quiet, 1, 0);
	diff_eq_int("evalconn: the fade clock faded all three counters",
		    ec_faded, 1, 0);
	diff_eq_int("evalconn: the fade quotients are compared signed",
		    ec_fade_signed, 1, 0);
	diff_eq_int("evalconn: the fade divisions are unsigned",
		    ec_fade_unsigned, 1, 0);
	diff_eq_int("evalconn: the 2.3 guard on its own", ec_echo_23, 1, 0);
	diff_eq_int("evalconn: a third of the duration on its own",
		    ec_echo_thirds_only, 1, 0);
	diff_eq_int("evalconn: reaching the retrain limit is allowed",
		    ec_retrain_at_limit, 1, 0);
	diff_eq_int("evalconn: the scaled average is what is compared",
		    ec_echo_scaled_cmp, 1, 0);
	diff_eq_int("evalconn: a negative duration on the echo path",
		    ec_echo_neg_dur, 1, 0);
	diff_eq_int("evalconn: a negative retrain limit in the rate-down arm",
		    ec_case2_neg_max, 1, 0);
	diff_eq_int("evalconn: a negative retrain limit in the retrain arm",
		    ec_neg_max_4b, 1, 0);
	diff_eq_int("evalconn: verdict 5 has no case in the switch",
		    ec_verdict5_no_case, 1, 0);
	diff_eq_int("evalconn: +0x94 alone does not override a retrain",
		    ec_ext_no_override, 1, 0);
	diff_eq_int("evalconn: the rate up reads two different counters",
		    ec_rateup_two_counters, 1, 0);
	diff_eq_int("evalconn: +0x14 cleared with no rate down armed",
		    ec_no_ratedown_clears, 1, 0);

	diff_eq_int("evalconn: +0x8c code 0", ec_ext_code[0], 1, 0);
	diff_eq_int("evalconn: +0x8c code 1", ec_ext_code[1], 1, 0);
	diff_eq_int("evalconn: +0x8c code 2", ec_ext_code[2], 1, 0);
	diff_eq_int("evalconn: +0x8c code 3", ec_ext_code[3], 1, 0);
	diff_eq_int("evalconn: +0x8c code 4", ec_ext_code[4], 1, 0);
	diff_eq_int("evalconn: +0x8c code 5", ec_ext_code[5], 1, 0);
	diff_eq_int("evalconn: +0x8c code 6", ec_ext_code[6], 1, 0);
	diff_eq_int("evalconn: +0x8c code 7", ec_ext_code[7], 1, 0);
	diff_eq_int("evalconn: +0x8c below -1 is left alone",
		    ec_ext_untouched, 1, 0);
	diff_eq_int("evalconn: the blocked V42 rate down", ec_ext_blocked, 1,
		    0);
	diff_eq_int("evalconn: the V42 rate down", ec_ext_down, 1, 0);
	diff_eq_int("evalconn: and it returned before stage 5",
		    ec_ext_returned_early, 1, 0);
	diff_eq_int("evalconn: the V42 retrain", ec_ext_retrain, 1, 0);
	diff_eq_int("evalconn: the V42 fall-back", ec_ext_fallback, 1, 0);
	diff_eq_int("evalconn: the V42 rate down overridden by +0x94",
		    ec_ext_override, 1, 0);

	diff_eq_int("evalconn: the rate up accumulated", ec_rateup_wait, 1, 0);
	diff_eq_int("evalconn: the rate up fired", ec_rateup, 1, 0);
	diff_eq_int("evalconn: the rate down accumulated", ec_ratedown_wait, 1,
		    0);
	diff_eq_int("evalconn: the rate down fired", ec_ratedown, 1, 0);
	diff_eq_int("evalconn: the rate down printed its demand",
		    ec_case2_down, 1, 0);
	diff_eq_int("evalconn: the rate down became a retrain",
		    ec_case2_retrain, 1, 0);
	diff_eq_int("evalconn: and then a fall-back", ec_case2_fallback, 1, 0);

	diff_eq_int("evalconn: echo state 0 scaled by 1.52", ec_echo_152, 1,
		    0);
	diff_eq_int("evalconn: echo state 1 scaled by 1.39", ec_echo_139, 1,
		    0);
	diff_eq_int("evalconn: the thirds fire condition",
		    ec_echo_fire_thirds, 1, 0);
	diff_eq_int("evalconn: the fifths fire condition",
		    ec_echo_fire_fifths, 1, 0);
	diff_eq_int("evalconn: the keep-rate energy took 0.65", ec_echo_065, 1,
		    0);
	diff_eq_int("evalconn: the keep-rate energy took 1.8", ec_echo_18, 1,
		    0);
	diff_eq_int("evalconn: the echo machine went terminal",
		    ec_echo_terminal, 1, 0);
	diff_eq_int("evalconn: and state 3 uses the unscaled test",
		    ec_echo_after3, 1, 0);
	diff_eq_int("evalconn: +0x5c is not rounded through float",
		    ec_mindur_exact, 1, 0);
	diff_eq_int("evalconn: the printed product is not rounded either",
		    ec_prod_exact, 1, 0);

	diff_eq_int("evalconn: the retrain accumulated", ec_retrain_wait, 1,
		    0);
	diff_eq_int("evalconn: the retrain fired", ec_retrain, 1, 0);
	diff_eq_int("evalconn: the retrain fell back", ec_retrain_fallback, 1,
		    0);
	diff_eq_int("evalconn: the renegotiation count forced a retrain",
		    ec_reneg_forced, 1, 0);
	diff_eq_int("evalconn: and did NOT consult the retrain limit",
		    ec_reneg_no_fallback, 1, 0);
	diff_eq_int("evalconn: the +0x94 override", ec_override, 1, 0);

	diff_eq_int("evalconn: the debug arm waited out its period",
		    ec_dbg_wait, 1, 0);
	diff_eq_int("evalconn: alternate debug +0x80 == 0", ec_alt[0], 1, 0);
	diff_eq_int("evalconn: alternate debug +0x80 == 1", ec_alt[1], 1, 0);
	diff_eq_int("evalconn: alternate debug +0x80 == 2", ec_alt[2], 1, 0);
	diff_eq_int("evalconn: alternate debug +0x80 == 3", ec_alt[3], 1, 0);
	diff_eq_int("evalconn: alternate debug +0x80 out of range", ec_alt[4],
		    1, 0);
	diff_eq_int("evalconn: the debug fall back", ec_dbg_fallback, 1, 0);
	diff_eq_int("evalconn: the debug retrain", ec_dbg_retrain, 1, 0);
	diff_eq_int("evalconn: the debug rate up", ec_dbg_up, 1, 0);
	diff_eq_int("evalconn: the debug rate down", ec_dbg_down, 1, 0);
	diff_eq_int("evalconn: the debug arms are an else-if chain",
		    ec_dbg_chain, 1, 0);

	diff_eq_int("evalconn: a negative retrain limit", ec_neg_max, 1, 0);
	diff_eq_int("evalconn: a negative retrain duration",
		    ec_neg_retrain_dur, 1, 0);
	diff_eq_int("evalconn: a negative rate-up duration",
		    ec_neg_rateup_dur, 1, 0);
	diff_eq_int("evalconn: a negative rate-up dwell", ec_neg_rateup_min, 1,
		    0);
	diff_eq_int("evalconn: a negative rate-down duration",
		    ec_neg_ratedown_dur, 1, 0);
	diff_eq_int("evalconn: a negative rate-down dwell",
		    ec_neg_ratedown_min, 1, 0);
	diff_eq_int("evalconn: a negative renegotiation limit",
		    ec_neg_reneg_limit, 1, 0);
	diff_eq_int("evalconn: a negative debug period", ec_neg_period, 1, 0);
	diff_eq_int("evalconn: a negative +0x24", ec_neg_word24, 1, 0);
	diff_eq_int("evalconn: a NaN average was fed", ec_nan_fed, 1, 0);

	rc |= diff_end();

	return rc;
}
