/*
 * t_v90dataph.cpp -- V90Demodulator's phase-4 and data-phase entries, and its
 * three diagnostics accessors, against the blob.
 *
 *     V90Demodulator::enterDataSteadyState()              548 bytes
 *     V90Demodulator::getAT_UD(TAG_DiagnosticResults *)   418
 *     V90Demodulator::enterDataPhase()                    322
 *     V90Demodulator::enterRRN()                          225
 *     V90Demodulator::enterFPE()                          110
 *     V90Demodulator::enterPhase4()                       110
 *     V90Demodulator::getRbsPattern(unsigned int *)        51
 *     V90Demodulator::indicateRemoteRateReneg()            40
 *
 * ---------------------------------------------------------------------------
 * WHY THIS FILE DOES NOT USE test/harness/v90demfix.h
 *
 * It very nearly could: the fixture already builds the demodulator, the
 * parameter block, the Phase 2 record, the equaliser, the connection
 * evaluator and the detector, and `t_v90demod.cpp` drives `sessionTermination`
 * through it -- which is 400 of `enterDataSteadyState`'s 548 bytes.
 *
 * WHAT STOPS IT IS A SENTENCE IN THAT FIXTURE'S OWN COMMENT.  `snap_dem` says
 * of the pointers it does NOT neutralise -- "the phase 4 demodulator, the
 * demapper, the constellation designer, the detector" -- that they "stay in
 * the comparison for exactly that reason: a store that lands on one of them
 * should fail the test".  Four of the eight members here DEREFERENCE those
 * pointers, so using the fixture would mean wiring them to per-side blocks,
 * and every per-side block has to be neutralised out of `snap_dem` or it
 * differs for ever.  That would silently convert four compared words into
 * four ignored ones in `t_v90demod.cpp` and `t_vpcmep3.cpp`, which are green
 * today and whose claims nobody asked to weaken.  Duplicating apparatus is
 * cheap; a comparison that quietly stops comparing is finding F2400's shape.
 *
 * So the fixture below is this file's own, and the blocks the shared one does
 * not have -- the demapper, the phase 4 demodulator, the constellation
 * designer, the `tagV90AdditionalCPinfo` and the alternate mapping parameters
 * -- are wired here and neutralised here.
 *
 * ---------------------------------------------------------------------------
 * THE OBJECTS ARE NEVER ZEROED -- finding F230
 *
 * Every slot gets varied pseudorandom bytes before every trial, so a store
 * that fails to happen is visible and a store of zero into memory that was
 * already zero is not mistaken for one.  The fields each arm reads are then
 * planted on top, identically on both sides.
 *
 * THE FLOAT-BEARING FIELDS ARE PLANTED, NOT SEEDED, and for a reason this
 * batch had to think about rather than inherit.  `getAT_UD` puts
 * `Agc<float>::level` and `V90Equalizer::meanErrorEnergyCurrent` through
 * `fldlg2 / fyl2x`, and log10 of a negative or of a NaN is precisely where
 * two compilations may legitimately part company.  Both are therefore planted
 * from a finite POSITIVE table.  Zero and negative are not accidents to be
 * discovered from seeded bytes; if they belong in the grid they belong there
 * deliberately, and `run_atud` says below why they are not in it.
 *
 * ---------------------------------------------------------------------------
 * ANTI-VACUITY, per finding F3509 rather than per path
 *
 * Every counter here names an OBSERVABLE difference -- a byte of some object,
 * a word of the diagnostics record, a line of transcript.  None of them says
 * "a branch believed to have been taken".  The mutations in
 * test/mutations/v90dataph.json are what adjudicate; the counters only stop a
 * green run that measured nothing.
 *
 * THE FIVE ENTRIES SHARE ONE LATCH AND IT IS THE FIRST TRAP.  `enterRRN`,
 * `enterFPE` and `enterPhase4` all return early when `inPhase3` is already 2
 * AND all set it to 2, so a slot seeded with 2 makes all three no-ops and
 * every comparison passes for the worst possible reason.  `run_latch` sweeps
 * the field explicitly, requires both outcomes to have been seen for each of
 * the five members, and requires the two outcomes to leave DIFFERENT objects
 * behind -- which is the only form of the claim a seeded slot cannot fake.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Phase4Demodulator.h"
#include "dsplib/TAG_DiagnosticResults.h"
#include "dsplib/tagV90AdditionalCPinfo.h"

extern "C" {

void ref_enterDataPhase(void *self)
    asm("ref__ZN14V90Demodulator14enterDataPhaseEv");
void ref_enterDataSteadyState(void *self)
    asm("ref__ZN14V90Demodulator20enterDataSteadyStateEv");
void ref_enterRRN(void *self) asm("ref__ZN14V90Demodulator8enterRRNEv");
void ref_enterFPE(void *self) asm("ref__ZN14V90Demodulator8enterFPEEv");
void ref_enterPhase4(void *self) asm("ref__ZN14V90Demodulator11enterPhase4Ev");
void ref_getRbsPattern(const void *self, unsigned int *rbs)
    asm("ref__ZNK14V90Demodulator13getRbsPatternEPj");
void ref_getAT_UD(const void *self, void *results)
    asm("ref__ZNK14V90Demodulator8getAT_UDEP21TAG_DiagnosticResults");
void ref_indicateRemoteRateReneg(const void *self)
    asm("ref__ZNK14V90Demodulator23indicateRemoteRateRenegEv");

float ref_timingHistoryMean(void *self)
    asm("ref__ZN12V90Resampler20getTimingHistoryMeanEv");
float ref_timingHistoryStd(void *self)
    asm("ref__ZN12V90Resampler19getTimingHistoryStdEv");

extern unsigned int ref_dsplibs_debug_level;

}

/*
 * ===========================================================================
 * THE FIXTURE
 * ===========================================================================
 *
 * Every slot is oversized by a guard region that nothing under test may
 * touch.  For `TAG_DiagnosticResults` that guard is not decoration: the
 * record's declared length is a LOWER BOUND (see its header -- no allocation
 * site in the object bounds it, and 0x22c is only the highest offset any
 * writer reaches).  The guard past the tail is what turns "at least 0x22c"
 * from a comment into a checked claim, and it is the only thing that would
 * catch a store this batch mis-read.
 */

#define DEM_SLOT	(0x298 + 64)
#define PARM_SLOT	(V90PARAMETERS_BOUND + 64)
#define PH2_SLOT	(sizeof(V90Phase2Info) + 32)
#define EQU_SLOT	0x150
#define CE_SLOT		0xbc
#define DMP_SLOT	0x1eb8
#define P4D_SLOT	0x351c
#define CD_SLOT		0x54
#define ACP_SLOT	0x18
#define MPAR_SLOT	(sizeof(V90MappingParams) + 32)
#define BLK_SLOT	0x80
#define ADID_SLOT	(sizeof(V90AutoDigitalImpDetector))

/* The diagnostics record, plus a guard the object must never reach. */
#define DR_BOUND	0x22c
#define DR_GUARD	64
#define DR_SLOT		(DR_BOUND + DR_GUARD)

#define ST_HIST		24

/* Offsets into the demodulator slot, for the fields reached as raw bytes. */
#define RS_HIST		(0x094 + 0x0a4)	/* V90Resampler::timingHistory    */
#define RS_HLEN		(0x094 + 0x0a8)	/* V90Resampler::timingHistoryLen */

#define PARAMS_EVAL	0x160		/* TIMING_HISTORY_EVALUATION_ENABLED */
#define PARAMS_MINSTD	0x16c		/* TIMING_OFFESET_MIN_STD_FOR_SAVE   */
#define PARAMS_W268	0x268
#define PARAMS_W26C	0x26c
#define PARAMS_W27C	0x27c
#define PARAMS_W280	0x280

#define BLK_DEVIATION	0x4c		/* _tagModemParameters, thousandths */

static unsigned char dem[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char parm[2][PARM_SLOT] __attribute__((aligned(8)));
static unsigned char ph2[2][PH2_SLOT] __attribute__((aligned(8)));
static unsigned char equ[2][EQU_SLOT] __attribute__((aligned(8)));
static unsigned char ce[2][CE_SLOT] __attribute__((aligned(8)));
static unsigned char dmp[2][DMP_SLOT] __attribute__((aligned(8)));
static unsigned char p4d[2][P4D_SLOT] __attribute__((aligned(8)));
static unsigned char cd[2][CD_SLOT] __attribute__((aligned(8)));
static unsigned char acp[2][ACP_SLOT] __attribute__((aligned(8)));
static unsigned char mpar[2][MPAR_SLOT] __attribute__((aligned(8)));
static unsigned char blk[2][BLK_SLOT] __attribute__((aligned(8)));
static unsigned char adid_[2][ADID_SLOT] __attribute__((aligned(8)));
static unsigned char drr[2][DR_SLOT] __attribute__((aligned(8)));
static float sthist[2][ST_HIST];

#define adid	((V90AutoDigitalImpDetector *)adid_)

static V90Demodulator *
D(int side)
{
	return (V90Demodulator *)dem[side];
}

static V90Phase2Info *
P2(int side)
{
	return (V90Phase2Info *)ph2[side];
}

static unsigned lfsr_state;

static unsigned char
lfsr(void)
{
	lfsr_state = lfsr_state * 1103515245u + 12345u;
	return (unsigned char)(lfsr_state >> 17);
}

static void
fill_pair(void *a, void *b, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		unsigned char v = lfsr();

		((unsigned char *)a)[i] = v;
		((unsigned char *)b)[i] = v;
	}
}

static void
set_int(int side, int off, int v)
{
	memcpy(&parm[side][off], &v, sizeof v);
}

static void
set_float(int side, int off, float v)
{
	memcpy(&parm[side][off], &v, sizeof v);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
}

/*
 * Finite, POSITIVE values for the two fields `getAT_UD` takes the logarithm
 * of.  See the file comment: a seeded 32-bit pattern is a NaN or a negative
 * often enough that this would be measuring the two compilations' `log10` of
 * a domain error rather than this reconstruction.
 */
static const float lvl_v[] = {
	1.0f, 0.5f, 2.0f, 1.0e-6f, 1.0e6f, 0.125f, 37.5f, 1.0f / 3.0f
};
#define NLVL	((int)(sizeof(lvl_v) / sizeof(lvl_v[0])))

/*
 * Round-trip delays for `getAT_UD`'s one piece of integer arithmetic.  See
 * the comment at the point of use for why the negatives are here and why the
 * magnitudes stop where they do.
 */
static const int rtd_v[] = {
	0, 1, 95, 96, 0x1234, -1, -95, -96, -1000, 0x7ffffff, -0x7ffffff
};
#define NRTD	((int)(sizeof(rtd_v) / sizeof(rtd_v[0])))

struct trial_args {
	unsigned int latch;	/* +0x34 inPhase3            */
	unsigned int quick;	/* +0x294                    */
	int enabled;		/* +0x280, gates getBitRate  */
	unsigned int bits;	/* mappingParamsAlt->word_0  */
	int idx;		/* picks the planted floats  */
};

static void
setup(int trial, const struct trial_args *t)
{
	int side;

	lfsr_state = 0x51edu + 0x9e37u * (unsigned)trial;

	fill_pair(dem[0], dem[1], DEM_SLOT);
	fill_pair(parm[0], parm[1], PARM_SLOT);
	fill_pair(ph2[0], ph2[1], PH2_SLOT);
	fill_pair(equ[0], equ[1], EQU_SLOT);
	fill_pair(ce[0], ce[1], CE_SLOT);
	fill_pair(dmp[0], dmp[1], DMP_SLOT);
	fill_pair(p4d[0], p4d[1], P4D_SLOT);
	fill_pair(cd[0], cd[1], CD_SLOT);
	fill_pair(acp[0], acp[1], ACP_SLOT);
	fill_pair(mpar[0], mpar[1], MPAR_SLOT);
	fill_pair(blk[0], blk[1], BLK_SLOT);
	fill_pair(adid_[0], adid_[1], ADID_SLOT);
	fill_pair(drr[0], drr[1], DR_SLOT);

	for (side = 0; side < 2; side++) {
		V90Demodulator *d = D(side);

		d->phase2Info = P2(side);
		d->params = (V90Parameters *)parm[side];
		d->equalizer = (V90Equalizer *)equ[side];
		d->connectionEvaluator = (V90ConnectionEvaluator *)ce[side];
		d->demapper = (V90Demapper *)dmp[side];
		d->phase4Demodulator = (V90Phase4Demodulator *)p4d[side];
		d->constellationDesigner =
		    (V90ConstellationDesigner *)cd[side];
		d->additionalCPinfo = (tagV90AdditionalCPinfo *)acp[side];
		d->autoDigitalImpDetector = &adid[side];
		d->mappingParamsAlt = (V90MappingParams *)mpar[side];

		d->inPhase3 = t->latch;
		d->quickConnect = t->quick;
		d->rateValid = (unsigned char)t->enabled;

		/*
		 * `resetLinearMappStudy` walks the detector through the
		 * demapper's OWN pointer, not the demodulator's, so both have
		 * to be wired or the 6 x 128 loop writes through a seeded
		 * address.
		 */
		((V90Demapper *)dmp[side])->adiDetector = &adid[side];

		adid[side].params = (V90Parameters *)parm[side];

		/*
		 * The resampler keeps its OWN parameter pointer at +0xa0 and
		 * `setBllState` reads four coefficients through it, so wiring
		 * the demodulator's is not enough.  `enterDataPhase` is what
		 * found that.
		 */
		d->resampler.params = (V90Parameters *)parm[side];

		((V90MappingParams *)mpar[side])->word_0 = t->bits;

		/* The two-step store target of `enterDataSteadyState`. */
		*(void **)&parm[side][0] = blk[side];

		/* The two fields the logarithms are taken of. */
		((V90Equalizer *)equ[side])->meanErrorEnergyCurrent =
		    lvl_v[t->idx % NLVL];
		d->agc.level = lvl_v[(t->idx + 3) % NLVL];

		P2(side)->rtd = 0x1234 + trial;
	}
}

/*
 * A copy of one side's demodulator with every pointer this fixture set to a
 * per-side address replaced by a boolean saying whether it still holds that
 * address.  Same idiom as `snap_dem` in t_v90demod.cpp.
 *
 * NOTHING ELSE IS NEUTRALISED, DELIBERATELY.  Every other word of the slot
 * came out of `fill_pair` and is byte-identical on the two sides, so nulling
 * it would turn a compared word into an ignored one.
 */
static void
snap_dem(unsigned char *dst, int side)
{
	V90Demodulator *s;
	V90Demodulator *l = D(side);

	memcpy(dst, dem[side], DEM_SLOT);
	s = (V90Demodulator *)dst;

	s->phase2Info = (V90Phase2Info *)(long)(l->phase2Info == P2(side));
	s->params = (V90Parameters *)(long)
	    (l->params == (V90Parameters *)parm[side]);
	s->equalizer = (V90Equalizer *)(long)
	    (l->equalizer == (V90Equalizer *)equ[side]);
	s->connectionEvaluator = (V90ConnectionEvaluator *)(long)
	    (l->connectionEvaluator == (V90ConnectionEvaluator *)ce[side]);
	s->demapper = (V90Demapper *)(long)
	    (l->demapper == (V90Demapper *)dmp[side]);
	s->phase4Demodulator = (V90Phase4Demodulator *)(long)
	    (l->phase4Demodulator == (V90Phase4Demodulator *)p4d[side]);
	s->constellationDesigner = (V90ConstellationDesigner *)(long)
	    (l->constellationDesigner == (V90ConstellationDesigner *)cd[side]);
	s->additionalCPinfo = (tagV90AdditionalCPinfo *)(long)
	    (l->additionalCPinfo == (tagV90AdditionalCPinfo *)acp[side]);
	s->autoDigitalImpDetector = (V90AutoDigitalImpDetector *)(long)
	    (l->autoDigitalImpDetector == &adid[side]);
	s->mappingParamsAlt = (V90MappingParams *)(long)
	    (l->mappingParamsAlt == (V90MappingParams *)mpar[side]);

	s->resampler.params = (V90Parameters *)(long)
	    (l->resampler.params == (V90Parameters *)parm[side]);

	/* `timingHistory` is a per-side array and can never compare equal. */
	memset(dst + RS_HIST, 0, sizeof(void *));
}

/*
 * The parameter block's FIRST WORD is the pointer to the modem parameter
 * block -- the two-step dereference `enterDataSteadyState` stores through --
 * so it is a per-side address like any other and is neutralised the same way.
 * Nothing else in the block may move, and the rest stays compared.
 */
static void
snap_parm(unsigned char *dst, int side)
{
	long same = (*(void *const *)&parm[side][0] == (void *)blk[side]);

	memcpy(dst, parm[side], PARM_SLOT);
	memcpy(dst, &same, sizeof(void *));
}

/*
 * The demapper likewise: `adiDetector` is per-side and everything else in
 * the 0x1eb8 block is seeded and identical.
 */
static void
snap_dmp(unsigned char *dst, int side)
{
	memcpy(dst, dmp[side], DMP_SLOT);
	((V90Demapper *)dst)->adiDetector = (V90AutoDigitalImpDetector *)(long)
	    (((V90Demapper *)dmp[side])->adiDetector == &adid[side]);
}

static void
snap_adid(unsigned char *dst, int side)
{
	memcpy(dst, adid_[side], ADID_SLOT);
	((V90AutoDigitalImpDetector *)dst)->params = (V90Parameters *)(long)
	    (adid[side].params == (V90Parameters *)parm[side]);
}

/*
 * Every peer object, compared whole.  The demodulator itself, the four
 * blocks the entries write through, the detector the demapper's reset walks,
 * and the parameter block -- which nothing under test may write.
 */
static void
compare_all(const char *what, long tag)
{
	static unsigned char sa[DEM_SLOT], sb[DEM_SLOT];
	static unsigned char da[DMP_SLOT], db[DMP_SLOT];
	static unsigned char aa[ADID_SLOT], ab[ADID_SLOT];

	snap_dem(sa, 0);
	snap_dem(sb, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "the demodulator",
		     sa, sb, DEM_SLOT, tag);

	snap_dmp(da, 0);
	snap_dmp(db, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "the demapper",
		     da, db, DMP_SLOT, tag);

	snap_adid(aa, 0);
	snap_adid(ab, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "the detector",
		     aa, ab, ADID_SLOT, tag);

	diff_eq_obj_(__FILE__, __LINE__, what, "the phase 4 demodulator",
		     p4d[0], p4d[1], P4D_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the constellation designer",
		     cd[0], cd[1], CD_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the additional CP info",
		     acp[0], acp[1], ACP_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the connection evaluator",
		     ce[0], ce[1], CE_SLOT, tag);
	{
		static unsigned char pa[PARM_SLOT], pb[PARM_SLOT];

		snap_parm(pa, 0);
		snap_parm(pb, 1);
		diff_eq_obj_(__FILE__, __LINE__, what, "the parameter block",
			     pa, pb, PARM_SLOT, tag);
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "the modem parameter block",
		     blk[0], blk[1], BLK_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the mapping parameters",
		     mpar[0], mpar[1], MPAR_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the Phase 2 record",
		     ph2[0], ph2[1], PH2_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the equaliser",
		     equ[0], equ[1], EQU_SLOT, tag);
}

/* Which member a sweep index selects, and what state it claims. */
enum entry_id { E_RRN = 0, E_FPE, E_PHASE4, E_DATA, E_STEADY, E_COUNT };

static const unsigned int entry_state[E_COUNT] = { 2u, 2u, 2u, 3u, 4u };

static void
call_entry(int which, int side)
{
	if (side == 0) {
		switch (which) {
		case E_RRN:	D(0)->enterRRN(); break;
		case E_FPE:	D(0)->enterFPE(); break;
		case E_PHASE4:	D(0)->enterPhase4(); break;
		case E_DATA:	D(0)->enterDataPhase(); break;
		default:	D(0)->enterDataSteadyState(); break;
		}
	} else {
		switch (which) {
		case E_RRN:	ref_enterRRN(D(1)); break;
		case E_FPE:	ref_enterFPE(D(1)); break;
		case E_PHASE4:	ref_enterPhase4(D(1)); break;
		case E_DATA:	ref_enterDataPhase(D(1)); break;
		default:	ref_enterDataSteadyState(D(1)); break;
		}
	}
}

/*
 * ===========================================================================
 * THE LATCH
 * ===========================================================================
 *
 * Each of the five entries returns immediately when `inPhase3` already equals
 * the state it sets.  The claim this run makes is not "both arms ran" -- that
 * is a path counter and finding F3509 rules it worthless -- but "the two arms
 * leave DIFFERENT objects behind", which a seeded slot cannot fake and which
 * a mutation that deletes the guard kills.
 */
static int
run_latch(void)
{
	static const unsigned int latch_v[] = { 0u, 2u, 3u, 4u, 0xffffffffu };
	int which, li, seen_ret[E_COUNT], seen_ran[E_COUNT], differed[E_COUNT];

	diff_begin("V90Demodulator phase entries: the latch");

	memset(seen_ret, 0, sizeof seen_ret);
	memset(seen_ran, 0, sizeof seen_ran);
	memset(differed, 0, sizeof differed);

	set_level(0);

	for (which = 0; which < E_COUNT; which++) {
		static unsigned char before[DEM_SLOT], ran[DEM_SLOT];
		int haveRet = 0, haveRan = 0;

		for (li = 0; li < (int)(sizeof latch_v / sizeof latch_v[0]);
		     li++) {
			struct trial_args t;
			long tag = (long)which * 1000 + li;
			int early;

			t.latch = latch_v[li];
			t.quick = (unsigned int)li;
			t.enabled = 1;
			t.bits = 40u + (unsigned int)li;
			t.idx = li;

			setup(li, &t);
			set_int(0, PARAMS_EVAL, 0);
			set_int(1, PARAMS_EVAL, 0);

			memcpy(before, dem[1], DEM_SLOT);

			call_entry(which, 0);
			call_entry(which, 1);

			compare_all("the latch sweep", tag);

			early = (latch_v[li] == entry_state[which]);
			if (early) {
				/*
				 * The whole object must be untouched, which
				 * is a stronger statement than "it returned".
				 */
				diff_eq_obj_(__FILE__, __LINE__,
					     "an early return wrote nothing",
					     "the demodulator",
					     before, dem[1], DEM_SLOT, tag);
				seen_ret[which] = 1;
				haveRet = 1;
			} else {
				seen_ran[which] = 1;
				if (memcmp(before, dem[1], DEM_SLOT) != 0) {
					memcpy(ran, dem[1], DEM_SLOT);
					haveRan = 1;
				}
			}
		}

		differed[which] = (haveRet && haveRan);
	}

	for (which = 0; which < E_COUNT; which++) {
		diff_eq_int("the early return was reached (%ld)",
			    seen_ret[which], 1, which);
		diff_eq_int("the working arm was reached (%ld)",
			    seen_ran[which], 1, which);
		diff_eq_int("the two arms left different objects (%ld)",
			    differed[which], 1, which);
	}

	return diff_end();
}

/*
 * ===========================================================================
 * enterRRN, enterFPE, enterPhase4
 * ===========================================================================
 *
 * The three that share state 2.  What separates them is
 *
 *   - the deadline constant and its multiplier (0x10680 + 2*rtd against
 *     0x28230 + 5*rtd);
 *   - whether +0x44 is CLEARED or ACCUMULATED;
 *   - `enterRRN`'s three-way conjunction into `additionalCPinfo`, its clear
 *     of `rateValid` and its second diagnostic.
 *
 * THE CONJUNCTION IS THE ONE THING A SWEEP CAN GET WRONG.  The store is 1 on
 * exactly one of four paths, so a grid that never produces a 1 -- or never
 * produces a 0 -- proves nothing about the `&&` chain, and a counter that
 * says "the true path ran" is the vacuity finding F3509 names.  The three
 * terms are therefore driven independently and the claim is on the STORED
 * VALUE: both values must have been observed, and observed with each of the
 * three terms as the one that was false.
 */
static int
run_state2(void)
{
	int which, ci, li, ri;
	int sawOne = 0, sawZero = 0, falseTerm[3];
	int sawAccum = 0, sawCleared = 0;

	diff_begin("V90Demodulator::enterRRN / enterFPE / enterPhase4");

	memset(falseTerm, 0, sizeof falseTerm);

	dsplib_debug_capture_on = 1;

	for (li = 0; li <= 2; li++) {
		set_level((unsigned int)li);

		for (which = E_RRN; which <= E_PHASE4; which++)
		for (ci = 0; ci < 8; ci++)
		for (ri = 0; ri < 4; ri++) {
			struct trial_args t;
			long tag = (long)li * 100000 + which * 1000 + ci * 10
			    + ri;
			int side;
			unsigned int w44before;

			t.latch = 0u;
			t.quick = 0u;
			t.enabled = 1;
			t.bits = 40u;
			t.idx = ci;

			setup(ci * 4 + ri, &t);

			for (side = 0; side < 2; side++) {
				((V90ConnectionEvaluator *)ce[side])->silenceRrnRequest =
				    (ci & 1) ? 1u : 0u;
				((V90Phase4Demodulator *)p4d[side])->int_003c =
				    (ci & 2) ? 1 : 0;
				((V90Phase4Demodulator *)p4d[side])->int_0038 =
				    (ci & 4) ? 1 : 0;

				/* rtd drives both `lea` forms. */
				P2(side)->rtd = (int)(0x100 * ri + ri);

				D(side)->samplesInPhase = 0x1000u + (unsigned)ri;
				D(side)->phase4ElapsedSamples = 0x2000u + (unsigned)ci;

				set_int(side, PARAMS_W268, 0x5150 + ci);
				set_int(side, PARAMS_W27C, 0x6160 + ri);
			}

			w44before = D(1)->phase4ElapsedSamples;
			dsplib_debug_capture_reset();

			call_entry(which, 0);
			call_entry(which, 1);

			compare_all("the state-2 entries", tag);

			diff_eq_int("the transcript (%ld)",
				    (long)strcmp(dsplib_debug_capture_text(0),
						 dsplib_debug_capture_text(1)),
				    0, tag);

			/* The deadline, by name as well as by object. */
			diff_eq_int("the deadline (%ld)",
				    (long)D(0)->phase4TimeoutDeadline,
				    (long)D(1)->phase4TimeoutDeadline, tag);

			if (which == E_PHASE4) {
				if (D(1)->phase4ElapsedSamples != w44before)
					sawAccum = 1;
			} else if (D(1)->phase4ElapsedSamples == 0) {
				sawCleared = 1;
			}

			if (which == E_RRN) {
				unsigned int got =
				    ((tagV90AdditionalCPinfo *)acp[1])->word_10;

				if (got == 1u)
					sawOne = 1;
				if (got == 0u) {
					sawZero = 1;
					if (!(ci & 1))
						falseTerm[0] = 1;
					else if (!(ci & 2))
						falseTerm[1] = 1;
					else if (!(ci & 4))
						falseTerm[2] = 1;
				}
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the conjunction stored a 1", sawOne, 1, 0);
	diff_eq_int("the conjunction stored a 0", sawZero, 1, 0);
	diff_eq_int("the first term was the false one", falseTerm[0], 1, 0);
	diff_eq_int("the second term was the false one", falseTerm[1], 1, 0);
	diff_eq_int("the third term was the false one", falseTerm[2], 1, 0);
	diff_eq_int("enterPhase4 accumulated +0x44", sawAccum, 1, 0);
	diff_eq_int("enterRRN/enterFPE cleared +0x44", sawCleared, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * enterDataPhase
 * ===========================================================================
 *
 * The two literals -- 12600 and 30000 -- and the inlined `getBitRate` in the
 * diagnostic.  The study length is not stored anywhere this test can read as
 * itself; what it reaches is `V90Demapper::resetLinearMappStudy`, which puts
 * it into the demapper's +0x1ea8, so the object comparison carries it and the
 * anti-vacuity claim is that BOTH values have been seen there.
 */
static int
run_dataphase(void)
{
	int li, qi, bi, ei;
	int sawShort = 0, sawLong = 0, sawRate = 0, sawZeroRate = 0;

	diff_begin("V90Demodulator::enterDataPhase");

	dsplib_debug_capture_on = 1;

	for (li = 0; li <= 2; li++) {
		set_level((unsigned int)li);

		for (qi = 0; qi < 2; qi++)
		for (bi = 0; bi < 2; bi++)
		for (ei = 0; ei < 6; ei++) {
			struct trial_args t;
			long tag = (long)li * 10000 + qi * 1000 + bi * 100 + ei;
			int side;
			unsigned int study;

			t.latch = 0u;
			t.quick = qi ? 0x5au : 0u;
			t.enabled = bi;
			t.bits = (unsigned int)(ei * 7 + 3);
			t.idx = ei;

			setup(qi * 100 + bi * 10 + ei, &t);

			for (side = 0; side < 2; side++) {
				set_int(side, PARAMS_W26C, 0x7170 + ei);
				set_int(side, PARAMS_W280, 0x8180 + ei);
			}

			dsplib_debug_capture_reset();

			D(0)->enterDataPhase();
			ref_enterDataPhase(D(1));

			compare_all("enterDataPhase", tag);

			diff_eq_int("the transcript (%ld)",
				    (long)strcmp(dsplib_debug_capture_text(0),
						 dsplib_debug_capture_text(1)),
				    0, tag);

			study = ((V90Demapper *)dmp[1])->studyLength;
			if (study == 12600u)
				sawShort = 1;
			if (study == 30000u)
				sawLong = 1;

			/*
			 * The rate reaches the transcript only at level 2.
			 * That the ENABLED and DISABLED forms differ in the
			 * text is what says the guard is read; comparing the
			 * two texts against each other is what says both
			 * sides computed it the same way.
			 */
			if (li > 1) {
				const char *txt = dsplib_debug_capture_text(1);

				if (strstr(txt, "Rate = 0 ") != 0)
					sawZeroRate = 1;
				else if (strstr(txt, "Rate = ") != 0)
					sawRate = 1;
			}

			diff_eq_int("the study enable flag (%ld)",
				    (long)((V90Demapper *)dmp[0])
					->linearMappStudyEnabled,
				    (long)((V90Demapper *)dmp[1])
					->linearMappStudyEnabled, tag);
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the quick-connect study length was used", sawShort, 1, 0);
	diff_eq_int("the full study length was used", sawLong, 1, 0);
	diff_eq_int("a non-zero rate was printed", sawRate, 1, 0);
	diff_eq_int("the disabled rate of 0 was printed", sawZeroRate, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * enterDataSteadyState
 * ===========================================================================
 *
 * `sessionTermination`'s timing-history block with a different guard.  The
 * value patterns are that suite's, including the quiet NaN, because the sign
 * character is built branchlessly as `0x2d - 2*CF` and an unordered compare
 * sets CF -- so a NaN prints '+' where the naive `0.0f < mean` prints '-'.
 * That input is the one that tells the two spellings apart.
 */
static float
st_value(int pattern, int i)
{
	switch (pattern) {
	case 0:	return 1.0f / 3.0f + 0.01f * (float)i;
	case 1:	return -1.0f / 7.0f - 0.013f * (float)i;
	case 2:	return (i & 1) ? 2.5f : -2.5f;
	case 3:	return 123.4567f + 0.9f * (float)i;
	case 4:	return 0.00009f * (float)(i + 1);
	case 5:	return 7.25f;
	case 6:	return -0.99995f;
	case 7:	return (float)(i - ST_HIST / 2) * 0.3125f;
	case 8:	return 0.0f;
	default: {
		float q;
		unsigned int b = 0x7fc00000u;

		memcpy(&q, &b, sizeof q);
		return q;
	}
	}
}

#define ST_PATTERNS	10

static int
run_steady(void)
{
	static const unsigned int st_len[] = { 1u, 2u, 7u, ST_HIST };
	static const float st_minstd[] = { -1.0f, 0.0f, 0.5f, 1.0e9f };
	int li, ii;
	int sawSaved = 0, sawRefused = 0, sawDisabled = 0, sawPrinted = 0;

	diff_begin("V90Demodulator::enterDataSteadyState");

	dsplib_debug_capture_on = 1;

	for (li = 0; li <= 2; li++) {
		set_level((unsigned int)li);

		for (ii = 0; ii < ST_PATTERNS * 4 * 4 * 2; ii++) {
			static unsigned char bb[BLK_SLOT];
			struct trial_args t;
			long tag = (long)li * 100000 + ii;
			int side, pat, lj, mj, ej;
			unsigned int n;
			float minstd, rstd;

			pat = ii % ST_PATTERNS;
			lj = (ii / ST_PATTERNS) % 4;
			mj = (ii / (ST_PATTERNS * 4)) % 4;
			ej = (ii / (ST_PATTERNS * 4 * 4)) % 2;

			n = st_len[lj];
			minstd = st_minstd[mj];

			t.latch = 0u;
			t.quick = 0u;
			t.enabled = 1;
			t.bits = 40u;
			t.idx = ii;

			setup(ii, &t);

			for (side = 0; side < 2; side++) {
				unsigned int i;
				void *hp;

				for (i = 0; i < ST_HIST; i++)
					sthist[side][i] =
					    st_value(pat, (int)i);

				hp = sthist[side];
				memcpy(&dem[side][RS_HIST], &hp, sizeof hp);
				memcpy(&dem[side][RS_HLEN], &n, sizeof n);

				set_int(side, PARAMS_EVAL, ej);
				set_float(side, PARAMS_MINSTD, minstd);
			}

			memcpy(bb, blk[1], BLK_SLOT);
			rstd = ref_timingHistoryStd(&dem[1][0x94]);

			dsplib_debug_capture_reset();

			D(0)->enterDataSteadyState();
			ref_enterDataSteadyState(D(1));

			compare_all("enterDataSteadyState", tag);

			diff_eq_int("the transcript (%ld)",
				    (long)strcmp(dsplib_debug_capture_text(0),
						 dsplib_debug_capture_text(1)),
				    0, tag);

			/*
			 * The evaluation flag is COPIED, not just read.  That
			 * is what separates this member from
			 * `sessionTermination` and it is asserted by name
			 * because a copy of a value that was already there
			 * would be invisible in the object comparison.
			 */
			diff_eq_int("the evaluation flag was copied (%ld)",
				    (long)D(1)->timingHistoryEval, (long)ej, tag);

			if (ej == 0) {
				sawDisabled = 1;
			} else if (memcmp(bb, blk[1], BLK_SLOT) != 0) {
				sawSaved = 1;
			} else if (!(minstd >= rstd)) {
				sawRefused = 1;
			}

			if (dsplib_debug_capture_lines(1) != 0)
				sawPrinted = 1;
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the saving arm wrote the registry", sawSaved, 1, 0);
	diff_eq_int("the too-noisy arm refused", sawRefused, 1, 0);
	diff_eq_int("the evaluation-disabled arm was reached", sawDisabled,
		    1, 0);
	diff_eq_int("the blob printed", sawPrinted, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * getRbsPattern, getAT_UD, indicateRemoteRateReneg
 * ===========================================================================
 */
static int
run_accessors(void)
{
	int li, ii;
	int sawSetBits = 0, sawClearBits = 0, sawDistinct = 0;
	unsigned int firstPattern = 0;
	int havePattern = 0;

	diff_begin("V90Demodulator diagnostics accessors");

	dsplib_debug_capture_on = 1;

	for (li = 0; li <= 2; li++) {
		set_level((unsigned int)li);

		for (ii = 0; ii < 64 * NLVL; ii++) {
			struct trial_args t;
			long tag = (long)li * 100000 + ii;
			int side;
			unsigned int rbs[2][V90ADID_PHASES];
			unsigned int bits = (unsigned int)(ii % 64);
			unsigned int p;

			t.latch = 0u;
			t.quick = 0u;
			t.enabled = (ii & 64) ? 0 : 1;
			t.bits = (unsigned int)(ii % 53) + 1u;
			t.idx = ii / 64;

			setup(ii, &t);

			for (side = 0; side < 2; side++) {
				for (p = 0; p < V90ADID_PHASES; p++)
					adid[side].byte_280c[p] =
					    (unsigned char)((bits >> p) & 1u);

				/*
				 * THE ROUND TRIP DELAY MUST GO NEGATIVE HERE
				 * OR THE DIVIDE'S SIGNEDNESS IS UNTESTED.
				 * The object divides `rtd * 10` by 96 with
				 * `mul $0xaaaaaaab` and no sign correction,
				 * so the arithmetic is unsigned; over the
				 * non-negative values `setup` plants, the
				 * signed and unsigned spellings agree on
				 * every one and the mutation that swaps them
				 * survived a green run.  This table is what
				 * separates them, and it is a differential
				 * trial rather than a fabricated one: the
				 * blob is the authority on what it computes
				 * for a negative field, and OUR spelling is
				 * unsigned throughout, so nothing here is
				 * undefined on this side (D561).
				 *
				 * The magnitudes stay under 2^31 / 10 so the
				 * SIGNED spelling does not overflow either;
				 * a mutation that is undefined rather than
				 * merely wrong proves nothing about the
				 * claim it was written for.
				 */
				P2(side)->rtd = rtd_v[ii % NRTD];
			}

			/* --- getRbsPattern --- */
			memset(rbs, 0xa5, sizeof rbs);
			D(0)->getRbsPattern(rbs[0]);
			ref_getRbsPattern(D(1), rbs[1]);
			diff_eq_int("getRbsPattern (%ld)",
				    (long)memcmp(rbs[0], rbs[1], sizeof rbs[0]),
				    0, tag);
			compare_all("getRbsPattern", tag);

			/* --- indicateRemoteRateReneg --- */
			D(0)->indicateRemoteRateReneg();
			ref_indicateRemoteRateReneg(D(1));
			compare_all("indicateRemoteRateReneg", tag);
			diff_eq_int("the designer's flag (%ld)",
				    (long)((V90ConstellationDesigner *)cd[1])
					->rateAction, 1L, tag);

			/* --- getAT_UD --- */
			dsplib_debug_capture_reset();
			D(0)->getAT_UD((TAG_DiagnosticResults *)drr[0]);
			ref_getAT_UD(D(1), drr[1]);

			diff_eq_obj_(__FILE__, __LINE__, "getAT_UD",
				     "the diagnostics record",
				     drr[0], drr[1], DR_SLOT, tag);
			compare_all("getAT_UD", tag);

			diff_eq_int("the transcript (%ld)",
				    (long)strcmp(dsplib_debug_capture_text(0),
						 dsplib_debug_capture_text(1)),
				    0, tag);

			/*
			 * THE GUARD PAST THE DECLARED TAIL.  The record's
			 * length is a lower bound, so this is the only thing
			 * that would catch a store past +0x22c that this
			 * batch failed to read.  Both sides are seeded
			 * identically, so comparing the blob's guard against
			 * what it was seeded with is the sharp form.
			 */
			{
				static unsigned char seed[DR_GUARD];
				unsigned int k;
				int touched = 0;

				memcpy(seed, drr[0] + DR_BOUND, DR_GUARD);
				for (k = 0; k < DR_GUARD; k++)
					if (drr[1][DR_BOUND + k] != seed[k])
						touched = 1;
				diff_eq_int("nothing was written past the "
					    "declared tail (%ld)",
					    touched, 0, tag);
			}

			p = ((TAG_DiagnosticResults *)drr[1])->rbsPattern;
			diff_eq_int("the RBS pattern (%ld)", (long)p,
				    (long)bits, tag);
			if (p != 0u)
				sawSetBits = 1;
			if (p == 0u)
				sawClearBits = 1;
			if (!havePattern) {
				firstPattern = p;
				havePattern = 1;
			} else if (p != firstPattern) {
				sawDistinct = 1;
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("a non-zero RBS pattern was produced", sawSetBits, 1, 0);
	diff_eq_int("a zero RBS pattern was produced", sawClearBits, 1, 0);
	diff_eq_int("the pattern varied with the detector's bytes",
		    sawDistinct, 1, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_latch();
	bad |= run_state2();
	bad |= run_dataphase();
	bad |= run_steady();
	bad |= run_accessors();

	return bad;
}
