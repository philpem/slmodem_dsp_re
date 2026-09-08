/*
 * t_v90demctor.cpp -- differential test of `V90Demodulator`'s lifecycle pair:
 * the 1002-byte constructor at 0x1c2b0/0x1c6a0 and the 669-byte destructor at
 * 0x1ad70/0x1b010.
 *
 * Both sides go through asm() labels and both the C1 and the C2 variant of
 * each.  C++ has no syntax for running a constructor over storage that already
 * exists, and `OBJ = V90Demodulator(...)` would build a temporary over
 * uninitialised stack and copy it in, throwing away the seed the whole fixture
 * rests on (findings F223, F224).  The blob holds C1 and C2 as two identical
 * copies at different addresses and our compiler emits one function under both
 * names, so calling only one leaves half the pair untested.
 *
 * ---------------------------------------------------------------------------
 * IT IS A PLACEMENT TEST FIRST AND AN ALLOCATION TEST SECOND
 *
 * Fourteen arguments, ten of them pointers, and the whole body is twelve
 * stores and thirteen allocations.  So every pointed-to object is a SEPARATE
 * block with its own seeded contents and the two sides are pointed at the SAME
 * ten -- finding F1105's rule, which makes every stored pointer compare equal
 * and keeps all ten words IN the comparison instead of excluded from it.  Ten
 * identical pointers would make nine of the ten placement claims vacuous.
 *
 * TWO ARGUMENTS ARE NOT STORED AND BOTH ARE CLAIMS.  `compMode` (argument 13)
 * reaches nothing but the equaliser's eleventh argument, and the sweep proves
 * that by varying it and looking INSIDE the equaliser's block; argument 14 is
 * what lands at +0x30, so the two are checked against each other rather than
 * assumed distinct.
 *
 * ---------------------------------------------------------------------------
 * THE PARAMETER BLOCK IS BUILT BY `initSession(); init();` AND NOT SEEDED
 *
 * Thirteen constructors run under this one, and between them they read some
 * thirty parameters, allocate arrays whose lengths come out of the block and
 * do x87 arithmetic on floats that come out of it.  A seeded block gives every
 * one of those a random 32-bit pattern -- a signalling NaN about one time in
 * 250, and an allocation length of two billion rather more often than that.
 * `V90Parameters::init()` is the object's OWN answer to what those values are,
 * it is already differentially tested (t_v90params.cpp), and it is declared on
 * the block form of the class for exactly this kind of use (V90PreFilter.h).
 * The handful of slots the sweep wants to VARY are set on top of it and each
 * one says why.
 *
 * The block is 0x558 bytes, not `V90PARAMETERS_BOUND`: `setToDefault` writes
 * as far as +0x554 and the bound is only how far the five methods in
 * V90PreFilter.h reach.
 *
 * ---------------------------------------------------------------------------
 * WHAT CANNOT BE COMPARED ACROSS THE SIDES, AND WHY EACH ONE IS SAFE
 *
 *   - Any word holding a pointer into a block one side allocated.  Discovered
 *     rather than listed, by the same `harness_alloc_live_set` walk
 *     t_v90rxctor.cpp uses, and for the same reason: listing them by hand
 *     means reading thirteen constructors this file does not own and
 *     re-reading them whenever one changes.  INSIDE a live allocation, not
 *     equal to one -- the descrambler holds seven pointers into the MIDDLE of
 *     its buffer.
 *   - `preFilter.coefficients`, which `V90PreFilter::reset` points at that
 *     side's own copy of a static table.  t_v90prefilter.cpp's claim, not this
 *     file's.
 *   - The `V90Resampler` vptr at +0x094, which is that side's own vtable.  The
 *     rest of the resampler subobject -- its BLL state, its history length,
 *     its counters -- stays in the comparison, so what the constructor told it
 *     is still visible.
 *
 * Everything else is compared, including all thirteen slots' CONTENTS: the
 * pointers are excluded, so without looking inside the blocks nothing the
 * thirteen constructors were told would be visible at all.  That is the lesson
 * t_v90rxctor.cpp's `cmp_block` records after six mutations proved it.
 * Pointers those blocks hold BACK INTO the demodulator -- the equaliser's
 * prefilter, resampler and verifier, the phase 4 demodulator's descrambler --
 * are translated to their offset from `this` rather than dropped, so a
 * subobject address landing at the wrong offset still fails.
 *
 * ---------------------------------------------------------------------------
 * THE DESTRUCTOR CALLS `sessionTermination()` BEFORE ANY GUARD, AND ONE GUARD
 * IS THEREFORE UNREACHABLE
 *
 * `~V90Demodulator`'s first instruction after the prologue is an unconditional
 * `call _ZN14V90Demodulator18sessionTerminationEv`, whose 572 bytes read
 * `params`, the embedded prefilter and the embedded resampler, print, write
 * `params->modemParams`'s +0x4c, and end with
 * `phase3Demodulator->clearVerificationStatus()` with NO null test.  So:
 *
 *   - every arm of the null sweep needs a coherent object, which is why each
 *     trial builds one with the constructor and then takes individual slots
 *     away rather than planting raw blocks;
 *   - the guard at +0x1dc CANNOT be driven with a null, because
 *     `sessionTermination` has already dereferenced that field.  It is exercised
 *     in the true direction only, and this comment is the record of that rather
 *     than a check that pretends otherwise;
 *   - `inPhase3` at +0x34 decides which of `sessionTermination`'s arms runs and
 *     the CONSTRUCTOR DOES NOT WRITE IT, so the sweep sets it explicitly --
 *     including to 3, the data state, which is the only value that reaches the
 *     write into the modem parameter block.
 *
 * ---------------------------------------------------------------------------
 * THE SHARED FIXTURE IS SNAPSHOTTED ROUND EACH SIDE
 *
 * Both sides are handed the SAME parameter block, modem parameter block, Phase
 * 2 record and eight argument blocks, which is what makes the placement claims
 * work -- and `sessionTermination` WRITES into the modem parameter block.  Run
 * naively, the second side's write lands on top of the first's and a
 * disagreement about the value written is invisible.  So the fixture is saved
 * before our side runs, saved again after it, restored, and compared against
 * the second save once the blob's side has run.
 */

#include <string.h>
#include <malloc.h>

#include "harness.h"
#include "dsplib/debug.h"

/*
 * The class under test.  It brings the BLOCK form of `V90Parameters` with it
 * (finding F1112), so every parameter this file names is spelled as an index
 * and the names in the comments are the author's, out of
 * include/dsplib/V90Parameters.h, which no translation unit may hold as well.
 */
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"
#include "dsplib/V90TRN2Designer.h"
#include "dsplib/modem_params.h"

extern "C" {
/*
 * The two enum arguments are declared `int` here.  Both are
 * `enum X : int` -- opaque, with a fixed underlying type -- so the parameter
 * passing is identical and the driver avoids a cast at every call site.
 */
void dem_ctor1(void *self, unsigned int levels, V90Phase2Info *phase2,
	       void *jd, void *jdV92, void *dil, void *mp1, void *mp2,
	       void *cpInfo, void *cp, void *mp, int codec,
	       V90Parameters *params, int compMode, unsigned int flag)
	asm("_ZN14V90DemodulatorC1EjP13V90Phase2InfoP5V90JdP5V92JdP19tagV90DIL"
	    "descriptorP16V90MappingParamsS9_P22tagV90AdditionalCPinfoP5V90CPP"
	    "5V90MP23__tHardwareCodecTypes__P13V90Parameters20V90Computational"
	    "Modej");
void dem_ctor2(void *self, unsigned int levels, V90Phase2Info *phase2,
	       void *jd, void *jdV92, void *dil, void *mp1, void *mp2,
	       void *cpInfo, void *cp, void *mp, int codec,
	       V90Parameters *params, int compMode, unsigned int flag)
	asm("_ZN14V90DemodulatorC2EjP13V90Phase2InfoP5V90JdP5V92JdP19tagV90DIL"
	    "descriptorP16V90MappingParamsS9_P22tagV90AdditionalCPinfoP5V90CPP"
	    "5V90MP23__tHardwareCodecTypes__P13V90Parameters20V90Computational"
	    "Modej");
void ref_dem_ctor1(void *self, unsigned int levels, V90Phase2Info *phase2,
		   void *jd, void *jdV92, void *dil, void *mp1, void *mp2,
		   void *cpInfo, void *cp, void *mp, int codec,
		   V90Parameters *params, int compMode, unsigned int flag)
	asm("ref__ZN14V90DemodulatorC1EjP13V90Phase2InfoP5V90JdP5V92JdP19tagV9"
	    "0DILdescriptorP16V90MappingParamsS9_P22tagV90AdditionalCPinfoP5V9"
	    "0CPP5V90MP23__tHardwareCodecTypes__P13V90Parameters20V90Computati"
	    "onalModej");
void ref_dem_ctor2(void *self, unsigned int levels, V90Phase2Info *phase2,
		   void *jd, void *jdV92, void *dil, void *mp1, void *mp2,
		   void *cpInfo, void *cp, void *mp, int codec,
		   V90Parameters *params, int compMode, unsigned int flag)
	asm("ref__ZN14V90DemodulatorC2EjP13V90Phase2InfoP5V90JdP5V92JdP19tagV9"
	    "0DILdescriptorP16V90MappingParamsS9_P22tagV90AdditionalCPinfoP5V9"
	    "0CPP5V90MP23__tHardwareCodecTypes__P13V90Parameters20V90Computati"
	    "onalModej");

void dem_dtor1(void *self) asm("_ZN14V90DemodulatorD1Ev");
void dem_dtor2(void *self) asm("_ZN14V90DemodulatorD2Ev");
void ref_dem_dtor1(void *self) asm("ref__ZN14V90DemodulatorD1Ev");
void ref_dem_dtor2(void *self) asm("ref__ZN14V90DemodulatorD2Ev");

extern unsigned int ref_dsplibs_debug_level;

void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);
}

/* ===================================================== the fixture */

#define DEM_SIZE	0x298
#define DEM_SLOT	(DEM_SIZE + 64)

/*
 * 0x558, because `setToDefault` writes as far as +0x554 and `V90Modem`
 * allocates exactly that many bytes for the class.  The block form's union is
 * only `V90PARAMETERS_BOUND` wide, so the storage is a byte array and the
 * class pointer is a cast onto it.
 */
#define PARM_BYTES	0x558
#define PARM_SLOT	(PARM_BYTES + 64)
/*
 * The modem parameter block.  `sessionTermination` writes its `clockDeviation`
 * at +0x4c, so the slot has to span that WITHOUT relying on the slack: the
 * assertion below is what says it does, because a struct that stopped short
 * would put the write in the guard bytes and `shared_compare` would still pass.
 */
#define MP_SLOT		(sizeof(struct _tagModemParameters) + 64)
typedef char mp_spans_clock_deviation[
    (sizeof(struct _tagModemParameters) >= 0x50) ? 1 : -1];
#define PH2_SLOT	(sizeof(V90Phase2Info) + 32)
#define MEAS_SLOT	0x80

/* jd, jdV92, dil, mappingParams1, mappingParams2, cpInfo, cp, mp. */
#define NARG		8
#define ARG_JD		0
#define ARG_JDV92	1
#define ARG_DIL		2
#define ARG_MP1		3
#define ARG_MP2		4
#define ARG_CPINFO	5
#define ARG_CP		6
#define ARG_MP		7
#define ARG_BYTES	96

static unsigned char demo[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char parm[PARM_SLOT] __attribute__((aligned(8)));
static unsigned char mpb[MP_SLOT] __attribute__((aligned(8)));
static unsigned char ph2[PH2_SLOT] __attribute__((aligned(8)));
static unsigned char meas[MEAS_SLOT] __attribute__((aligned(8)));
static unsigned char argblk[NARG][ARG_BYTES] __attribute__((aligned(8)));

#define PARAMS	((V90Parameters *)parm)
#define PH2	((V90Phase2Info *)ph2)

static V90Demodulator *
D(int side)
{
	return (V90Demodulator *)demo[side];
}

/*
 * The parameter indices this file names.  The names are the ORIGINAL AUTHOR'S,
 * out of include/dsplib/V90Parameters.h; writing the offsets down without them
 * would throw information away, and this translation unit cannot include that
 * header.
 */
#define PARAMS_MODEM_PARAMS		0x000	/* the pointer at +0x000  */
#define PARAMS_HARDWARE_CODEC_TYPE	(0x008 / 4)
#define PARAMS_TIMING_HISTORY_EVAL	(0x160 / 4)
#define PARAMS_TIMING_MIN_STD_FOR_SAVE	(0x16c / 4)	/* float */
#define PARAMS_LINEAR_EQU_LENGTH	(0x170 / 4)
#define PARAMS_LINEAR_EQU_HISTORY_LEN	(0x174 / 4)
#define PARAMS_DFE_LENGTH		(0x1fc / 4)

/* Where the demodulator's own subobjects sit, for the pointer translation. */
#define OFF_PREFILTER		0x06c
#define OFF_RESAMPLER		0x094
#define OFF_CONSTELLATION_POWER	0x148
#define OFF_DESCRAMBLER		0x1e8
#define OFF_SPECTRAL_VERIFIER	0x210

/*
 * The two words of the demodulator that hold a static address of that SIDE's
 * own, which no fixture can make agree and no allocator walk can find.
 *
 * `preFilter.coefficients` is set by `V90PreFilter::reset` to
 * `&preFilterCoefType1[0][0]`, which is in our `.rodata` for one side and in
 * the blob's for the other -- t_v90prefilter.cpp's claim.  The word at
 * +0x094 is the `V90Resampler` vptr, likewise each side's own; the rest of
 * that subobject stays in the comparison.
 */
#define OFF_FIR_COEFFICIENTS	(OFF_PREFILTER + 0x00)
#define OFF_RESAMPLER_VPTR	OFF_RESAMPLER

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	/* `| 1` so no seeded byte is ever zero; finding F230. */
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
fill(void *p, size_t n)
{
	unsigned char *q = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		q[i] = nextb();
}

/* ================================ the shared blocks, saved round each side */

/*
 * `sessionTermination` writes `params->modemParams`'s +0x4c and the thirteen
 * constructors may write anywhere in what they are handed.  Both sides share
 * one copy of all of it, so the second side's writes would land on top of the
 * first's; saving, restoring and comparing is what makes them visible.
 */
struct shared_snap {
	unsigned char parm[PARM_SLOT];
	unsigned char mpb[MP_SLOT];
	unsigned char ph2[PH2_SLOT];
	unsigned char meas[MEAS_SLOT];
	unsigned char arg[NARG][ARG_BYTES];
};

static struct shared_snap snap_pre, snap_ours;

static void
shared_save(struct shared_snap *s)
{
	memcpy(s->parm, parm, sizeof s->parm);
	memcpy(s->mpb, mpb, sizeof s->mpb);
	memcpy(s->ph2, ph2, sizeof s->ph2);
	memcpy(s->meas, meas, sizeof s->meas);
	memcpy(s->arg, argblk, sizeof s->arg);
}

static void
shared_restore(const struct shared_snap *s)
{
	memcpy(parm, s->parm, sizeof s->parm);
	memcpy(mpb, s->mpb, sizeof s->mpb);
	memcpy(ph2, s->ph2, sizeof s->ph2);
	memcpy(meas, s->meas, sizeof s->meas);
	memcpy(argblk, s->arg, sizeof s->arg);
}

/*
 * The Phase 2 record's `L2` and the parameter block's `modemParams` are this
 * fixture's own addresses and are the same object for both sides, so they
 * compare equal and need no neutralising.
 */
static void
shared_compare(const struct shared_snap *s, long trial)
{
	diff_eq_obj_(__FILE__, __LINE__, "the parameter block",
		     "V90Parameters", parm, s->parm, PARM_SLOT, trial);
	diff_eq_obj_(__FILE__, __LINE__, "the modem parameter block",
		     "_tagModemParameters", mpb, s->mpb, MP_SLOT, trial);
	diff_eq_obj_(__FILE__, __LINE__, "the Phase 2 record", "V90Phase2Info",
		     ph2, s->ph2, PH2_SLOT, trial);
	diff_eq_obj_(__FILE__, __LINE__, "the L2 measurements", "float[]",
		     meas, s->meas, MEAS_SLOT, trial);
	diff_eq_obj_(__FILE__, __LINE__, "the eight argument blocks", "bytes",
		     argblk, s->arg, sizeof s->arg, trial);
}

/* ================================================== what a trial looks like */

struct trial_args {
	unsigned int levels;
	int codec;		/* argument 11, `__tHardwareCodecTypes__` */
	int compMode;		/* argument 13, never stored              */
	unsigned int flag;	/* argument 14, lands at +0x30            */
	int linearEquLen;	/* the parameter block's +0x170           */
	int dfeLen;		/* the parameter block's +0x1fc           */
	int inPhase3;		/* +0x34, which the constructor never writes */
	int timingEval;		/* the parameter block's +0x160           */
};

static void
seed_all(long trial, const struct trial_args *t)
{
	unsigned int i;

	lfsr = 0x71c3u + 0x9e37u * (unsigned)trial;

	fill(demo[0], DEM_SLOT);
	memcpy(demo[1], demo[0], DEM_SLOT);
	fill(argblk, sizeof argblk);
	fill(ph2, PH2_SLOT);

	/*
	 * FINITE MEASUREMENTS, NOT SEEDED BYTES, for v90demfix.h's reason: a
	 * random 32-bit pattern is as likely to be a NaN as anything and no
	 * test is where two compilations' `(int)NaN` should be discovered.
	 */
	for (i = 0; i < MEAS_SLOT / sizeof(float); i++) {
		float v = -40.0f + 0.75f * (float)(int)
		    ((i * 7u + (unsigned)trial) % 61u);

		memcpy(&meas[i * sizeof(float)], &v, sizeof v);
	}
	PH2->L2 = (float *)meas;
	PH2->pcmType = (int)((unsigned)trial & 1u);
	PH2->rtd = (short)(0x1234 + trial);
	PH2->Uinfo = (unsigned char)(trial * 11u + 3u);

	/*
	 * The modem parameter block is ZEROED and not seeded.  `init()` reads
	 * four of its fields and multiplies one of them; `sessionTermination`
	 * writes +0x4c.  A seeded `powerReductionTenths` makes the parameter
	 * block's power reduction a different number every trial for no gain,
	 * and a seeded `paramFile` is a pointer nothing here owns.
	 */
	memset(mpb, 0, sizeof mpb);

	memset(parm, 0, sizeof parm);
	*(void **)&parm[PARAMS_MODEM_PARAMS] = mpb;
	PARAMS->initSession();
	PARAMS->init();

	/*
	 * -1 keeps `V90PreFilter`'s constructor on the arm that takes the
	 * CODEC ARGUMENT rather than the one that reads the codec index out of
	 * the parameter block -- which is the arm that makes argument 11
	 * observable at all.  `init()` leaves it at its default and the
	 * default is not always negative.
	 */
	V90PW(PARAMS)[PARAMS_HARDWARE_CODEC_TYPE] = -1;

	/*
	 * The equaliser's two lengths, which are the ONLY two words this
	 * constructor reads out of the parameter block.  They are varied so
	 * that a reconstruction reading +0x1fc where the object reads +0x170
	 * fails; the history length is held above both, because
	 * `V90Equalizer`'s constructor walks the history backwards from it
	 * over `linearEquLength` entries.
	 */
	V90PW(PARAMS)[PARAMS_LINEAR_EQU_LENGTH] = t->linearEquLen;
	V90PW(PARAMS)[PARAMS_DFE_LENGTH] = t->dfeLen;
	V90PW(PARAMS)[PARAMS_LINEAR_EQU_HISTORY_LEN] = 512;

	/* Which of `sessionTermination`'s two arms the destructor takes. */
	V90PW(PARAMS)[PARAMS_TIMING_HISTORY_EVAL] = t->timingEval;
	V90PF(PARAMS)[PARAMS_TIMING_MIN_STD_FOR_SAVE] = 0.1f;
}

/* ============================================ comparing across the two sides */

/*
 * WHICH live allocation `p` is in, or -1.  INSIDE one, not equal to one: the
 * descrambler holds seven pointers that address the MIDDLE of its buffer, and
 * an equality test finds the base and misses the six beside it -- which is
 * exactly how t_v90rxctor.cpp's first version failed.
 */
static int
live_index(const void *p, void **live, int nlive)
{
	unsigned long v = (unsigned long)p;
	int i;

	for (i = 0; i < nlive; i++) {
		unsigned long base = (unsigned long)live[i];

		if (v >= base && v <= base + malloc_usable_size(live[i]))
			return i;
	}
	return -1;
}

static int
is_live(const void *p, void **live, int nlive)
{
	return live_index(p, live, nlive) >= 0;
}

/*
 * A pointer as both sides can agree about it.
 *
 *   - into that side's own demodulator slot   ->  a tag plus the OFFSET, so a
 *                                                 subobject address landing at
 *                                                 the wrong offset still fails
 *   - into the block that side put in slot k  ->  a tag carrying k and the
 *                                                 offset
 *   - into any other live allocation          ->  zero; the two sides allocate
 *                                                 separately and always will
 *   - anything else                           ->  itself
 *
 * THE SLOT TAG IS WHAT MAKES AN ARGUMENT SWAP VISIBLE, and it was added
 * because the obvious version is not enough.  With every heap pointer mapped
 * to zero, handing `V90Phase3Demodulator` the connection evaluator instead of
 * the impairment detector changes one word inside a block from one live
 * allocation to another -- both of which become zero, on both sides, so
 * nothing sees it.  Mapping to WHICH of the thirteen blocks it is keeps the
 * claim; the correspondence between the two sides is by slot, which is exactly
 * what the two objects are supposed to agree about.
 */
#define MAXLIVE 256

struct ptrmap {
	void *slot[2][13];
	unsigned long slotend[2][13];
	int nslot;
	void *live[MAXLIVE];
	unsigned long liveend[MAXLIVE];
	int nlive;
};

/*
 * A POINTER INTO AN ALLOCATION KEEPS ITS OFFSET, and that is not a detail.
 * The obvious version replaces every heap pointer with zero, and then the
 * descrambler's near tap -- which decides nothing but where `pTap1` and
 * `pInitTap1` sit inside a buffer both sides allocate separately -- is a
 * number no comparison anywhere can see.  Keeping the offset within the block
 * keeps the claim, and the block's IDENTITY is still lost, which is why the
 * thirteen slots are matched by slot above.
 */
static unsigned long
translate(unsigned long v, int side, const struct ptrmap *m)
{
	unsigned long base = (unsigned long)demo[side];
	int k;

	if (v == 0)
		return v;
	if (v >= base && v < base + DEM_SIZE)
		return 0x40000000ul + (v - base);
	for (k = 0; k < m->nslot; k++) {
		unsigned long p = (unsigned long)m->slot[side][k];

		if (p != 0 && v >= p && v <= m->slotend[side][k])
			return 0x50000000ul + (unsigned long)k * 0x00100000ul
			    + (v - p);
	}
	for (k = 0; k < m->nlive; k++) {
		unsigned long p = (unsigned long)m->live[k];

		if (v >= p && v <= m->liveend[k])
			return 0x60000000ul + (v - p);
	}
	return v;
}

static void
translate_block(unsigned char *dst, const unsigned char *src, size_t n,
		int side, const struct ptrmap *m)
{
	size_t o;

	memcpy(dst, src, n);
	for (o = 0; o + 4 <= n; o += 4) {
		unsigned int w;

		memcpy(&w, dst + o, sizeof w);
		w = (unsigned int)translate((unsigned long)w, side, m);
		memcpy(dst + o, &w, sizeof w);
	}
}

/*
 * TRANSLATE THE TWO SIDES TOGETHER, AND LEAVE A WORD ALONE WHERE THEY ALREADY
 * AGREE.  This is `translate_block` twice over, minus the one case that made
 * the fixture flaky, and the argument that it is safe is exact:
 *
 *   TWO SEPARATELY ALLOCATED BLOCKS NEVER HAVE THE SAME ADDRESS.  So a word
 *   whose raw value is IDENTICAL on both sides is not a pointer into either
 *   side's allocations -- it is data, or a null, or a pointer to something the
 *   two sides share -- and translating it can only turn an equality into an
 *   inequality.  Skipping it cannot hide a difference, because equal bytes are
 *   equal whatever is done to them afterwards.
 *
 * Words that DO differ are translated exactly as before, so every genuine
 * pointer is still canonicalised and every claim the slot tags carry survives.
 */
static void
translate_pair(unsigned char *da, unsigned char *db,
	       const unsigned char *sa, const unsigned char *sb,
	       size_t n, const struct ptrmap *m)
{
	size_t o;

	memcpy(da, sa, n);
	memcpy(db, sb, n);
	for (o = 0; o + 4 <= n; o += 4) {
		unsigned int wa, wb;

		memcpy(&wa, da + o, sizeof wa);
		memcpy(&wb, db + o, sizeof wb);
		if (wa == wb)
			continue;
		wa = (unsigned int)translate((unsigned long)wa, 0, m);
		wb = (unsigned int)translate((unsigned long)wb, 1, m);
		memcpy(da + o, &wa, sizeof wa);
		memcpy(db + o, &wb, sizeof wb);
	}
}

/*
 * DID THE TRANSLATION INVENT A DIFFERENCE?  Returns the first offset where the
 * two translated blocks disagree and the two RAW blocks agreed, or -1.
 *
 * This is not a belt-and-braces check, it is the one this fixture was missing.
 * `translate` decides whether a word is a pointer FROM ITS VALUE and against
 * THAT SIDE'S live ranges, so a word that is not a pointer at all -- seeded
 * data, a float, a counter -- is rewritten whenever its bit pattern happens to
 * land inside some allocation.  The two sides' allocations are at different
 * addresses, so the same coincidental value can come out as one side's slot
 * and the other side's generic live block, and two blocks that were equal
 * become unequal.  Nothing else in the fixture can tell that apart from a real
 * disagreement: both arrive as a byte difference in `diff_eq_obj`.
 */
static long
translation_invented(const unsigned char *ta, const unsigned char *tb,
		     const unsigned char *ra, const unsigned char *rb,
		     size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (ta[i] != tb[i] && ra[i] == rb[i])
			return (long)i;
	return -1;
}

/*
 * THE GUARD IS SHOWN TO FIRE, because a detector that has never fired is
 * indistinguishable from a broken one -- finding F134, and this file is where
 * that mattered.  The scenario below is the observed failure exactly: ONE
 * value, the SAME on both sides, that lies inside a block side 0 knows as a
 * numbered slot and side 1 knows only as some live allocation.  The old
 * one-side-at-a-time translation turns it into 0x50000000 for side 0 and
 * 0x60000000 for side 1 -- which is byte for byte what the flake reported,
 * `60 50` against `00 60` in the high half of a word -- and `translate_pair`
 * leaves it alone.
 *
 * Returns 0 if both halves of that hold.
 */
static int
guard_selfcheck(void)
{
	unsigned char ra[64], rb[64], ta[64], tb[64];
	static struct ptrmap m;
	void *blk = sysdep_malloc(64);
	unsigned int w;
	int ok;

	if (blk == 0)
		return -1;

	memset(&m, 0, sizeof m);
	m.nslot = 1;
	m.slot[0][0] = blk;			/* side 0: a numbered slot   */
	m.slotend[0][0] = (unsigned long)blk + 64;
	m.slot[1][0] = 0;			/* side 1: no such slot      */
	m.slotend[1][0] = 0;
	m.nlive = 1;
	m.live[0] = blk;			/* but it IS live            */
	m.liveend[0] = (unsigned long)blk + 64;

	memset(ra, 0x11, sizeof ra);
	memcpy(rb, ra, sizeof rb);
	w = (unsigned int)(unsigned long)blk;
	memcpy(ra + 16, &w, sizeof w);
	memcpy(rb + 16, &w, sizeof w);

	/*
	 * The old way: one side at a time, unconditionally.  The invented
	 * difference is INSIDE the planted word but not necessarily at its
	 * first byte -- 0x50000000 and 0x60000000 agree in their low three --
	 * which is why the real flake reported a two-byte run at +2 of a word
	 * and not a four-byte one.
	 */
	translate_block(ta, ra, sizeof ra, 0, &m);
	translate_block(tb, rb, sizeof rb, 1, &m);
	{
		long at = translation_invented(ta, tb, ra, rb, sizeof ra);

		ok = at >= 16 && at < 20;
	}

	/* The new way: together, skipping the words that already agree. */
	translate_pair(ta, tb, ra, rb, sizeof ra, &m);
	ok = ok && translation_invented(ta, tb, ra, rb, sizeof ra) == -1;

	sysdep_free(blk);
	return ok ? 0 : -1;
}

/*
 * COMPARE A BLOCK THE CONSTRUCTOR ALLOCATED, not just the pointer to it.  The
 * thirteen pointers are excluded from the object comparison because the two
 * sides allocate separately, so without this nothing any of the thirteen
 * constructors was told would be visible -- `compMode` reaches no field of
 * this class at all and lives only inside the equaliser.
 */
static unsigned char cmpa[0xa9b0], cmpb[0xa9b0];

static void
cmp_block(const char *what, const char *type, const void *pa, const void *pb,
	  size_t n, const struct ptrmap *m, long trial)
{
	if (n > sizeof cmpa)
		n = sizeof cmpa;
	/*
	 * PAIRWISE HERE TOO.  These blocks are the constructors' own buffers,
	 * so most of what is in them is the allocator's 0xa5 fill, which is not
	 * a plausible address -- but the ones the constructors fill with real
	 * data have the same exposure the object had, and there is no reason to
	 * leave one of the two call sites carrying the defect.
	 */
	translate_pair(cmpa, cmpb, (const unsigned char *)pa,
		       (const unsigned char *)pb, n, m);
	diff_eq_int("the translation invented a difference in the block, "
		    "trial %ld -- got the offset",
		    translation_invented(cmpa, cmpb, (const unsigned char *)pa,
					 (const unsigned char *)pb, n),
		    -1, trial);
	diff_eq_obj_(__FILE__, __LINE__, what, type, cmpa, cmpb, n, trial);
}

/*
 * The words of the demodulator that hold two different addresses, discovered
 * rather than listed.  Every one is a pointer into a block one side allocated,
 * so it is exactly the set of four-byte-aligned words whose value is inside a
 * LIVE ALLOCATION on that side.  The discovery runs once and the list is then
 * fixed: a set that varied per trial could hide a genuine difference by
 * growing to cover it.
 */
static unsigned dem_skip[128];
static int n_dem_skip;

static void
discover_regions(const unsigned char *obj, unsigned size, void **live,
		 int nlive, unsigned *out, int *nout, int max)
{
	unsigned o;

	*nout = 0;
	for (o = 0; o + 4 <= size; o += 4) {
		unsigned int w;

		memcpy(&w, obj + o, sizeof w);
		if (w != 0 && is_live((const void *)(unsigned long)w, live,
				      nlive) && *nout < max)
			out[(*nout)++] = o;
	}
}

/* ===================================================== the thirteen slots */

/*
 * The order is the DESTRUCTOR's, which is not the order of the fields: the
 * equaliser first at +0x1d8, then the two phase demodulators and the demapper,
 * the TRN2 designer from +0x1c, the constellation designer, the impairment
 * detector, the connection evaluator, and last the five bare blocks that have
 * no destructor at all.
 */
struct slot {
	unsigned off;
	const char *name;
	unsigned size;		/* sizeof the class, or 0 for a bare block */
	unsigned width;		/* bytes per `levels` for a bare block     */
	int nullable;
};

static const struct slot slot_v[] = {
	{ 0x1d8, "equalizer",		sizeof(V90Equalizer),		0, 1 },
	/*
	 * NOT NULLABLE.  `sessionTermination` runs first and ends with
	 * `phase3Demodulator->clearVerificationStatus()` with no null test, so
	 * the guard below it can never be reached with a null in the object.
	 */
	{ 0x1dc, "phase3Demodulator",	sizeof(V90Phase3Demodulator),	0, 0 },
	{ 0x1e0, "phase4Demodulator",	sizeof(V90Phase4Demodulator),	0, 1 },
	{ 0x1e4, "demapper",		sizeof(V90Demapper),		0, 1 },
	{ 0x01c, "trn2Designer",	sizeof(V90TRN2Designer),	0, 1 },
	{ 0x208, "constellationDesigner", sizeof(V90ConstellationDesigner), 0, 1 },
	{ 0x23c, "autoDigitalImpDetector",
	  sizeof(V90AutoDigitalImpDetector),				0, 1 },
	{ 0x20c, "connectionEvaluator",	sizeof(V90ConnectionEvaluator),	0, 1 },
	{ 0x244, "array_244",		0,				 4, 1 },
	{ 0x248, "array_248",		0,				12, 1 },
	{ 0x250, "array_250",		0,				 4, 1 },
	{ 0x254, "array_254",		0,				 8, 1 },
	{ 0x25c, "array_25c",		0,				 8, 1 }
};

#define NSLOT ((int)(sizeof(slot_v) / sizeof(slot_v[0])))

static void *
slot_ptr(int side, unsigned off)
{
	void *p;

	memcpy(&p, demo[side] + off, sizeof p);
	return p;
}

static void
slot_set(int side, unsigned off, void *p)
{
	memcpy(demo[side] + off, &p, sizeof p);
}

/*
 * Take one slot away and null the field, the way the destructor would have.
 *
 * A SLOT WHOSE DESTRUCTOR IS NON-TRIVIAL MAY NOT BE REPLACED BY A RAW BLOCK
 * and may not simply be freed: every one of the eight reaches further --
 * `~V90Phase4Demodulator` runs `~V90Phase4Modulator`, `~V90Phase3Demodulator`
 * runs `~ANSamToneDetector`, which runs two `~GenericIIR`s, which free two
 * history buffers.  So the object is destroyed properly and then the field is
 * nulled; that is the only shape that works, and it cost t_v90rxctor.cpp a
 * crash to learn.
 */
static void
slot_kill(int side, int k)
{
	void *p = slot_ptr(side, slot_v[k].off);

	if (p == 0)
		return;

	switch (slot_v[k].off) {
	case 0x1d8: ((V90Equalizer *)p)->~V90Equalizer(); break;
	case 0x1dc: ((V90Phase3Demodulator *)p)->~V90Phase3Demodulator(); break;
	case 0x1e0: ((V90Phase4Demodulator *)p)->~V90Phase4Demodulator(); break;
	case 0x1e4: ((V90Demapper *)p)->~V90Demapper(); break;
	case 0x01c: ((V90TRN2Designer *)p)->~V90TRN2Designer(); break;
	case 0x208:
		((V90ConstellationDesigner *)p)->~V90ConstellationDesigner();
		break;
	case 0x23c:
		((V90AutoDigitalImpDetector *)p)->~V90AutoDigitalImpDetector();
		break;
	case 0x20c:
		((V90ConnectionEvaluator *)p)->~V90ConnectionEvaluator();
		break;
	default:
		break;			/* the five bare blocks */
	}
	sysdep_free(p);
	slot_set(side, slot_v[k].off, 0);
}

/* ============================================================== the sweep */

/*
 * The scenarios.  A full cross product of eight varying inputs would be
 * thousands of graph builds, each with a 43 KB impairment detector in it, and
 * would say nothing the table below does not: every input is varied against
 * every other one at least once, `levels` takes 0 and 128 as well as the small
 * values, and the two equaliser lengths take their two orderings.
 */
static const struct trial_args trial_v[] = {
	{   0, 0, 0, 0u,	  0,  0, 0, 1 },
	{   1, 1, 1, 1u,	  4, 12, 3, 1 },
	{   2, 2, 2, 0xffffffffu, 16,  4, 1, 0 },
	{   3, 3, 0, 7u,	 12, 16, 3, 0 },
	{   8, 0, 2, 0xdeadbeefu, 32,  0, 5, 1 },
	{  16, 2, 1, 2u,	  0, 32, 3, 1 },
	{  64, 1, 0, 3u,	 20,  8, 0, 1 },
	{ 128, 3, 2, 0x80000000u, 40, 40, 3, 0 }
};

#define NTRIAL ((int)(sizeof(trial_v) / sizeof(trial_v[0])))

static void
build(int side, int which, const struct trial_args *t)
{
	if (which)
		(side ? ref_dem_ctor1 : dem_ctor1)(demo[side], t->levels, PH2,
		    argblk[ARG_JD], argblk[ARG_JDV92], argblk[ARG_DIL],
		    argblk[ARG_MP1], argblk[ARG_MP2], argblk[ARG_CPINFO],
		    argblk[ARG_CP], argblk[ARG_MP], t->codec, PARAMS,
		    t->compMode, t->flag);
	else
		(side ? ref_dem_ctor2 : dem_ctor2)(demo[side], t->levels, PH2,
		    argblk[ARG_JD], argblk[ARG_JDV92], argblk[ARG_DIL],
		    argblk[ARG_MP1], argblk[ARG_MP2], argblk[ARG_CPINFO],
		    argblk[ARG_CP], argblk[ARG_MP], t->codec, PARAMS,
		    t->compMode, t->flag);
}

static void
destroy(int side, int which)
{
	if (which)
		(side ? ref_dem_dtor1 : dem_dtor1)(demo[side]);
	else
		(side ? ref_dem_dtor2 : dem_dtor2)(demo[side]);
}

/*
 * The fifteen words plus one three-byte hole the constructor NEVER WRITES,
 * and the 0x90 bytes of `V90ConstellationPower` -- whose constructor is one
 * `ret`, so its storage keeps the seed too.  "The constructor wrote too much"
 * passes a comparison, because both sides would write the same wrong thing.
 */
struct hole {
	unsigned off;
	unsigned len;
	const char *name;
};

static const struct hole hole_v[] = {
	{ 0x034, 4, "inPhase3" },
	{ 0x038, 4, "samplesInPhase" },
	{ 0x03c, 4, "word_3c" },
	{ 0x040, 4, "energyDropDetectorArmed" },
	{ 0x044, 4, "phase4ElapsedSamples" },
	{ 0x048, 4, "phase4TimeoutDeadline" },
	{ 0x148, 0x90, "constellationPower" },
	{ 0x240, 4, "pad_240" },
	{ 0x260, 4, "word_260" },
	{ 0x270, 4, "word_270" },
	{ 0x274, 4, "pad_274" },
	{ 0x281, 3, "pad_281" },
	{ 0x284, 4, "errorEnergyPrintCounter" },
	{ 0x288, 4, "errorEnergyPrintPeriod" },
	{ 0x28c, 4, "timingOffsetPrintCounter" },
	{ 0x290, 4, "timingOffsetPrintPeriod" },
	{ 0x294, 4, "quickConnect" }
};

#define NHOLE ((int)(sizeof(hole_v) / sizeof(hole_v[0])))

/* The ten pointer arguments, by the offset each must reach. */
struct place {
	unsigned off;
	int arg;		/* an index into argblk, or -1 */
	const char *name;
};

static const struct place place_v[] = {
	{ 0x004, -1,		"phase2Info" },
	{ 0x008, ARG_JD,	"jd" },
	{ 0x00c, ARG_JDV92,	"jdV92" },
	{ 0x010, ARG_DIL,	"dil" },
	{ 0x014, ARG_MP1,	"mappingParams" },
	{ 0x018, ARG_MP2,	"mappingParamsAlt" },
	{ 0x020, ARG_CPINFO,	"additionalCPinfo" },
	{ 0x024, ARG_CP,	"cp" },
	{ 0x028, ARG_MP,	"mp" },
	{ 0x02c, -2,		"params" }
};

#define NPLACE ((int)(sizeof(place_v) / sizeof(place_v[0])))

static int
run_ctor(void)
{
	long trial = 900000;
	int ti, which, lvl, i, k;
	unsigned seen[64];
	int nseen = 0, changed = 0;

	diff_begin("V90Demodulator::V90Demodulator");

	dsplib_debug_capture_on = 1;

	for (ti = 0; ti < NTRIAL; ti++)
	    for (which = 0; which < 2; which++)
		for (lvl = 0; lvl < 2; lvl++) {
			const struct trial_args *t = &trial_v[ti];
			struct alloc_log a0, a1, a2, a3;
			static unsigned char before[DEM_SLOT];
			static struct ptrmap pm;
			void *live[256];
			int nlive;
			unsigned h;

			trial++;

			seed_all(trial, t);
			memcpy(before, demo[0], DEM_SLOT);

			dsplibs_debug_level = ref_dsplibs_debug_level =
			    lvl ? 2u : 0u;
			dsplib_debug_capture_reset();

			shared_save(&snap_pre);
			a0 = harness_alloc;
			build(0, which, t);
			a1 = harness_alloc;
			shared_save(&snap_ours);
			shared_restore(&snap_pre);
			build(1, which, t);
			a2 = harness_alloc;
			if (t->flag == 0) {
				int side;
				for (side = 0; side < 2; side++) {
				Descrambler<unsigned char, int> *s =
				    &D(side)->descrambler;
				diff_eq_int("V.90 demod tap1/out is 18 (%ld)",
					    s->pTap1 - s->pOut, 18, trial);
				diff_eq_int("V.90 demod tap2/out is 23 (%ld)",
					    s->pTap2 - s->pOut, 23, trial);
				diff_eq_int("V.90 demod tail is 23 (%ld)",
					    s->tailLength, 23, trial);
				diff_eq_int("V.90 demod slack is 99 (%ld)",
					    s->pOut - s->pLimit, 99, trial);
				}
			}

			nlive = harness_alloc_live_set(live, MAXLIVE);
			if (nlive > MAXLIVE)
				nlive = MAXLIVE;
			pm.nslot = NSLOT;
			pm.nlive = nlive;
			for (k = 0; k < nlive; k++) {
				pm.live[k] = live[k];
				pm.liveend[k] = (unsigned long)live[k] +
				    malloc_usable_size(live[k]);
			}
			for (k = 0; k < NSLOT; k++) {
				int side;

				for (side = 0; side < 2; side++) {
					void *p = slot_ptr(side,
							   slot_v[k].off);

					pm.slot[side][k] = p;
					pm.slotend[side][k] = p == 0 ? 0
					    : (unsigned long)p +
					      malloc_usable_size(p);
				}
			}
			/*
			 * The words that hold two different addresses, found
			 * the way t_v90rxctor.cpp finds them.  This
			 * test does not DROP them: it canonicalises them,
			 * which is strictly stronger.  The discovery is kept
			 * because the count is a claim of its own -- a
			 * constructor that stopped allocating would leave it
			 * at zero -- and it runs once, because a set that
			 * varied per trial could hide a real difference by
			 * growing to cover it.
			 */
			if (n_dem_skip == 0)
				discover_regions(demo[0], DEM_SIZE, live, nlive,
						 dem_skip, &n_dem_skip, 128);

			{
				static unsigned char sa[DEM_SLOT], sb[DEM_SLOT];

				translate_pair(sa, sb, demo[0], demo[1],
					       DEM_SLOT, &pm);
				memset(sa + OFF_FIR_COEFFICIENTS, 0, 4);
				memset(sb + OFF_FIR_COEFFICIENTS, 0, 4);
				memset(sa + OFF_RESAMPLER_VPTR, 0, 4);
				memset(sb + OFF_RESAMPLER_VPTR, 0, 4);
				/*
				 * `got` is the OFFSET; the conversion in the
				 * label takes the trial, which is how
				 * diff_eq_int reads its arguments.
				 */
				diff_eq_int("the translation invented a "
					    "difference, trial %ld -- got the "
					    "offset",
					    translation_invented(sa, sb,
								 demo[0],
								 demo[1],
								 DEM_SIZE),
					    -1, trial);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after the constructor",
					     "V90Demodulator", sa, sb, DEM_SIZE,
					     trial);
				diff_eq_int("no store past the object (%ld)",
					    memcmp(demo[0] + DEM_SIZE,
						   demo[1] + DEM_SIZE,
						   DEM_SLOT - DEM_SIZE) == 0, 1,
					    trial);
				h = 0x811c9dc5u;
				for (i = 0; i < DEM_SIZE; i++)
					h = (h ^ sa[i]) * 0x01000193u;
			}

			/*
			 * THE RESAMPLER'S TWO BUFFERS, which are the only
			 * place its cutoff frequency ever appears.  `Resampler`
			 * designs a `taps * phases` bank from it and stores
			 * nothing else about it, so without this the fourth
			 * constructor argument is a number no comparison can
			 * see.  The history is compared beside it because it is
			 * as cheap as saying why it is not.
			 */
			{
				V90Demodulator *a = D(0);
				V90Demodulator *b = D(1);
				size_t nc = (size_t)a->resampler.taps *
				    a->resampler.phases * sizeof(float);

				diff_eq_int("the two banks are the same shape "
					    "(%ld)",
					    a->resampler.taps ==
					    b->resampler.taps &&
					    a->resampler.phases ==
					    b->resampler.phases &&
					    a->resampler.historyLen ==
					    b->resampler.historyLen, 1, trial);
				if (a->resampler.coeffs != 0 &&
				    b->resampler.coeffs != 0 && nc != 0)
					diff_eq_obj_(__FILE__, __LINE__,
						     "the resampler's "
						     "coefficient bank",
						     "float[]",
						     a->resampler.coeffs,
						     b->resampler.coeffs, nc,
						     trial);
				if (a->resampler.history != 0 &&
				    b->resampler.history != 0)
					diff_eq_obj_(__FILE__, __LINE__,
						     "the resampler's history",
						     "float[]",
						     a->resampler.history,
						     b->resampler.history,
						     (size_t)a->resampler
						     .historyLen *
						     sizeof(float), trial);
			}

			/*
			 * THE TEN PLACEMENTS, ASSERTED BY VALUE.  Every
			 * argument is a distinct block, so a store landing at
			 * the wrong offset holds a value no other argument
			 * could have supplied; two constructors that both
			 * stored nothing would compare equal (finding F1105).
			 */
			for (k = 0; k < NPLACE; k++) {
				const void *want;
				void *got;

				if (place_v[k].arg == -1)
					want = PH2;
				else if (place_v[k].arg == -2)
					want = PARAMS;
				else
					want = argblk[place_v[k].arg];
				memcpy(&got, demo[1] + place_v[k].off,
				       sizeof got);
				diff_eq_int("a pointer landed at +0x%lx",
					    got == want, 1,
					    (long)place_v[k].off);
			}

			/*
			 * ARGUMENT 11 AT +0x00 AND ARGUMENT 14 AT +0x30, and
			 * they are checked against EACH OTHER as well: argument
			 * 13 is never stored, so a reconstruction that stored
			 * it instead of 14 would put the wrong word at +0x30.
			 */
			diff_eq_int("codecType (%ld)",
				    (long)(int)D(1)->codecType, (long)t->codec,
				    trial);
			diff_eq_int("sessionFlag is argument 14 (%ld)",
				    (long)D(1)->sessionFlag, (long)t->flag,
				    trial);

			/* And the six words the constructor zeroes. */
			diff_eq_int("nofResampled (%ld)", (long)D(1)->nofResampled, 0,
				    trial);
			diff_eq_int("nofSymbols (%ld)", (long)D(1)->nofSymbols, 0,
				    trial);
			diff_eq_int("word_264 (%ld)", (long)D(1)->word_264, 0,
				    trial);
			diff_eq_int("word_268 (%ld)", (long)D(1)->word_268, 0,
				    trial);
			diff_eq_int("word_26c (%ld)", (long)D(1)->word_26c, 0,
				    trial);
			diff_eq_int("timingHistoryEval (%ld)", (long)D(1)->timingHistoryEval, 0,
				    trial);
			diff_eq_int("noEnergyDuration (%ld)", (long)D(1)->noEnergyDuration, 0,
				    trial);
			diff_eq_int("rateValid (%ld)", (long)D(1)->rateValid, 0,
				    trial);

			/* What it does NOT write, on both sides. */
			for (k = 0; k < NHOLE; k++) {
				diff_eq_int("+0x%lx keeps its seed, ours",
					    memcmp(demo[0] + hole_v[k].off,
						   before + hole_v[k].off,
						   hole_v[k].len) == 0, 1,
					    (long)hole_v[k].off);
				diff_eq_int("+0x%lx keeps its seed, theirs",
					    memcmp(demo[1] + hole_v[k].off,
						   before + hole_v[k].off,
						   hole_v[k].len) == 0, 1,
					    (long)hole_v[k].off);
			}

			/*
			 * THE THIRTEEN BLOCKS, AND NOT JUST THE THIRTEEN
			 * POINTERS.  Every one of the eight class-typed slots
			 * is compared whole, which is where `compMode`, the
			 * equaliser's two lengths and every argument the
			 * thirteen constructors were handed become visible.
			 * The five bare ones hold no initialised bytes, so
			 * what is checked there is the SIZE each was asked
			 * for -- which pins the n*4, n*12 and n*8 classes
			 * against a slot swap.
			 */
			for (k = 0; k < NSLOT; k++) {
				void *pa = slot_ptr(0, slot_v[k].off);
				void *pb = slot_ptr(1, slot_v[k].off);

				diff_eq_int("+0x%lx is a real block, ours",
					    pa != 0, 1, (long)slot_v[k].off);
				diff_eq_int("+0x%lx is a real block, theirs",
					    pb != 0, 1, (long)slot_v[k].off);
				if (pa == 0 || pb == 0)
					continue;
				if (slot_v[k].size != 0) {
					cmp_block(slot_v[k].name, "block", pa,
						  pb, slot_v[k].size, &pm,
						  trial);
				} else {
					unsigned want = t->levels *
					    slot_v[k].width;

					diff_eq_int("+0x%lx was asked for the "
						    "right size, ours",
						    malloc_usable_size(pa) >=
						    want, 1,
						    (long)slot_v[k].off);
					diff_eq_int("+0x%lx was asked for the "
						    "right size, theirs",
						    malloc_usable_size(pb) >=
						    want, 1,
						    (long)slot_v[k].off);
				}
			}

			/* The thirteen pointers are pairwise distinct. */
			for (k = 0; k < NSLOT; k++) {
				int j;

				for (j = k + 1; j < NSLOT; j++)
					diff_eq_int("+0x%lx is its own block",
						    slot_ptr(1, slot_v[k].off)
						    != slot_ptr(1,
								slot_v[j].off),
						    1, (long)slot_v[k].off);
			}

			diff_eq_int("allocations (%ld)", a1.allocs - a0.allocs,
				    a2.allocs - a1.allocs, trial);
			diff_eq_int("bytes asked for (%ld)",
				    (long)(a1.bytes - a0.bytes),
				    (long)(a2.bytes - a1.bytes), trial);
			diff_eq_int("nothing wild was freed (%ld)",
				    a2.bad_free - a0.bad_free, 0, trial);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0, 1,
				    trial);
			shared_compare(&snap_ours, trial);

			if (memcmp(before, demo[0], DEM_SLOT) != 0)
				changed = 1;
			for (i = 0; i < nseen; i++)
				if (seen[i] == h)
					break;
			if (i == nseen && nseen < 64)
				seen[nseen++] = h;

			/* Give it all back, through both destructors. */
			destroy(0, which);
			destroy(1, which);
			a3 = harness_alloc;
			diff_eq_int("the pair balances (%ld)", a3.live, a0.live,
				    trial);
			diff_eq_int("no free(NULL) (%ld)",
				    a3.free_null - a0.free_null, 0, trial);
			diff_eq_int("and nothing wild (%ld)",
				    a3.bad_free - a0.bad_free, 0, trial);
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the constructor changed the object", changed, 1, 0);
	diff_eq_int("and not to the same thing every trial", nseen >= 4, 1,
		    nseen);
	diff_eq_int("some words had to be excluded", n_dem_skip >= NSLOT, 1,
		    n_dem_skip);

	return diff_end();
}

/*
 * The destructor, one slot at a time.
 *
 * `pattern` == -1 leaves every slot alone; 0 <= `pattern` < NSLOT nulls every
 * NULLABLE slot except that one, so the difference in `frees` against the
 * all-null case names which arm fired.  `pattern` == NSLOT nulls all of them.
 * A guard reading the wrong field shows up as a free count that moves on the
 * wrong trial, and a guard deleted altogether shows up as `free_null`, because
 * this tree's `sysdep_free` tolerates NULL and counts it.
 */
static int
run_dtor(void)
{
	long trial = 950000;
	int ti, which, pattern, k, lvl;
	int frees_by_pattern[NSLOT + 2];
	int saw_intact = 0;

	diff_begin("V90Demodulator::~V90Demodulator");

	for (k = 0; k < NSLOT + 2; k++)
		frees_by_pattern[k] = -1;

	dsplib_debug_capture_on = 1;

	for (ti = 0; ti < NTRIAL; ti++)
	    for (which = 0; which < 2; which++)
		for (pattern = -1; pattern <= NSLOT; pattern++) {
			const struct trial_args *t = &trial_v[ti];
			struct alloc_log a0, a1, a2;
			int live_before;
			static unsigned char beforeA[DEM_SLOT];
			static unsigned char beforeB[DEM_SLOT];
			int side;

			/*
			 * Only the nullable slots get a one-hot trial;
			 * `phase3Demodulator` has no null arm to drive.
			 */
			if (pattern >= 0 && pattern < NSLOT &&
			    !slot_v[pattern].nullable)
				continue;

			trial++;
			lvl = (int)((unsigned)trial & 1u);
			dsplibs_debug_level = ref_dsplibs_debug_level =
			    lvl ? 2u : 0u;

			seed_all(trial, t);
			live_before = harness_alloc.live;
			build(0, which, t);
			build(1, which, t);

			for (side = 0; side < 2; side++) {
				/*
				 * SET BY HAND, because the constructor does
				 * not write it and `sessionTermination` tests
				 * it against 3.  Both sides get the same
				 * value.
				 */
				D(side)->inPhase3 = (unsigned int)t->inPhase3;

				if (pattern < 0)
					continue;
				for (k = 0; k < NSLOT; k++) {
					if (!slot_v[k].nullable)
						continue;
					if (k == pattern)
						continue;
					slot_kill(side, k);
				}
			}

			memcpy(beforeA, demo[0], DEM_SLOT);
			memcpy(beforeB, demo[1], DEM_SLOT);
			dsplib_debug_capture_reset();

			shared_save(&snap_pre);
			a0 = harness_alloc;
			destroy(0, which);
			a1 = harness_alloc;
			shared_save(&snap_ours);
			shared_restore(&snap_pre);
			destroy(1, which);
			a2 = harness_alloc;

			diff_eq_int("frees (%ld)", a1.frees - a0.frees,
				    a2.frees - a1.frees, trial);
			diff_eq_int("free(NULL) (%ld)",
				    a1.free_null - a0.free_null,
				    a2.free_null - a1.free_null, trial);
			diff_eq_int("wild frees (%ld)",
				    a1.bad_free - a0.bad_free,
				    a2.bad_free - a1.bad_free, trial);
			/*
			 * NEVER a `sysdep_free(NULL)`: every one of the
			 * thirteen arms is guarded and this tree's allocator
			 * counts a null free rather than ignoring it, which is
			 * the only thing that can see a guard being dropped.
			 */
			diff_eq_int("never freed a null (%ld)",
				    a2.free_null - a0.free_null, 0, trial);
			diff_eq_int("nothing wild was freed (%ld)",
				    a2.bad_free - a0.bad_free, 0, trial);
			/*
			 * NOTHING IS LEAKED AND NOTHING IS ALLOCATED.  Two
			 * constructions, a hand-applied null pattern and two
			 * destructions have to come back to exactly the live
			 * count this trial started with -- which is what says
			 * an arm that was skipped had already given its block
			 * back and an arm that ran gave back all of it.
			 */
			diff_eq_int("nothing is left over (%ld)", a2.live,
				    live_before, trial);
			diff_eq_int("and nothing was allocated (%ld)",
				    a2.allocs - a0.allocs, 0, trial);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0, 1,
				    trial);
			shared_compare(&snap_ours, trial);

			/*
			 * THE DESTRUCTOR WRITES EXACTLY FOUR BYTES OF `*this`,
			 * AND THEY ARE THE RESAMPLER'S VPTR.
			 *
			 * Not one of the thirteen slots is nulled after being
			 * released -- which is what makes a second destruction
			 * a double free, and is the blob's behaviour -- and no
			 * arm of the body stores anything.  What DOES store is
			 * the compiler: `~V90Resampler` is the destructor of a
			 * polymorphic class, so it walks +0x094 back down the
			 * chain, installing `V90Resampler`'s vtable, then
			 * `ResamplerTiming`'s, then `ResamplerTimingOffset`'s,
			 * then `Resampler`'s, as each base subobject's
			 * destructor runs.  The last of those is what the word
			 * holds afterwards; it is a different address on each
			 * side, so the CHANGE is asserted per side and the
			 * value is not compared across them.
			 *
			 * THAT THE CHAIN RAN TO THE BOTTOM IS PINNED
			 * ELSEWHERE, and does not need the vtable address.  If
			 * the member were typed as one of the bases -- so that
			 * `~ResamplerTiming` ran instead of `~V90Resampler` --
			 * the resampler's `timingHistory` would never be given
			 * back, because that buffer is `V90Resampler`'s own.
			 * "nothing is left over" below is what fails then.
			 *
			 * `-fno-lifetime-dse` is on, so a store to `*this` here
			 * survives to be seen and both halves of this check can
			 * fail.  Each side is compared against its OWN state,
			 * because the freed pointers hold different addresses
			 * on the two sides.
			 */
			for (side = 0; side < 2; side++) {
				const unsigned char *b = side ? beforeB
							      : beforeA;

				diff_eq_int("everything below the vptr is "
					    "untouched, side %ld",
					    memcmp(b, demo[side],
						   OFF_RESAMPLER_VPTR) == 0, 1,
					    (long)side);
				diff_eq_int("everything above it too, side %ld",
					    memcmp(b + OFF_RESAMPLER_VPTR + 4,
						   demo[side] +
						   OFF_RESAMPLER_VPTR + 4,
						   DEM_SLOT -
						   OFF_RESAMPLER_VPTR - 4) == 0,
					    1, (long)side);
				diff_eq_int("and the vptr was walked back down "
					    "the chain, side %ld",
					    memcmp(b + OFF_RESAMPLER_VPTR,
						   demo[side] +
						   OFF_RESAMPLER_VPTR, 4) != 0,
					    1, (long)side);
			}

			/*
			 * Recorded now and judged after the sweep, because the
			 * all-null case is the LAST pattern of each round and
			 * the comparison needs it first.  The counts come from
			 * one round only -- the last -- so that they are all
			 * measured against the same `levels` and the same
			 * parameter block.
			 */
			if (pattern >= 0)
				frees_by_pattern[pattern] = a1.frees - a0.frees;
			else
				saw_intact = 1;
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	/*
	 * THE ALL-NULL CASE IS THE FLOOR: the five subobject destructions the
	 * compiler emits, and nothing else.  Every one-hot case must free
	 * strictly more than it, which is what says the arm under test fired
	 * rather than some other arm freeing something.
	 */
	diff_eq_int("the all-null floor was measured",
		    frees_by_pattern[NSLOT] >= 0, 1, frees_by_pattern[NSLOT]);
	for (k = 0; k < NSLOT; k++) {
		if (!slot_v[k].nullable)
			continue;
		diff_eq_int("the arm at +0x%lx freed something of its own",
			    frees_by_pattern[k] > frees_by_pattern[NSLOT], 1,
			    (long)slot_v[k].off);
	}
	diff_eq_int("the intact object was destroyed too", saw_intact, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	/*
	 * FIRST, because the two runs below are only worth what the comparison
	 * they use is worth, and this fixture spent a while reporting a
	 * difference its own canonicalisation had manufactured.
	 */
	diff_begin("the pointer canonicalisation, and the guard on it");
	diff_eq_int("the guard fires on the old translation and not the new",
		    guard_selfcheck(), 0, 0);
	rc |= diff_end();

	rc |= run_ctor();
	rc |= run_dtor();

	return rc;
}
