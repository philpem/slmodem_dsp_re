/*
 * V90Resampler.cpp -- the V.90 receiver's resampler.
 *
 * Reconstructed from dsplibs.o.  dsplib/V90Resampler.h carries the object
 * map, the sixteen BLL states and where their names came from;
 * dsplib/Resampler.h carries the chain and the four vtables.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * WHAT THIS CLASS ADDS to `ResamplerTiming` is the band-limited-loop STATE
 * MACHINE and a ring of timing measurements:
 *
 *   - `setBllState` chooses the loop gains `bllK1`/`bllK2` -- which live in
 *     the base -- from a `V90Parameters` field pair, sixteen states, and says
 *     which one it picked through `edprintf`.
 *   - `resample` wraps the base's and, every
 *     TIMING_HISTORY_EVALUATION_PERIOD samples, appends
 *     `getTimingOffsetPPM()` to a ring of
 *     TIMING_HISTORY_EVALUATION_BUFFER_LENGTH floats, which
 *     `getTimingHistoryMean` and `getTimingHistoryStd` then summarise.
 *
 * `resample` is NOT virtual -- it is in no vtable -- so this one hides the
 * base's rather than overriding it, and calls it explicitly.  `reset()` IS
 * virtual and this class overrides it; `timingCorrection` is not overridden
 * here and stays `ResamplerTiming`'s.
 *
 * ---------------------------------------------------------------------------
 * A NOTE ON +0x0f4 OF `V90Parameters`, WHICH THIS FILE READS AS A FLOAT
 *
 * `include/dsplib/V90Parameters.h` is frozen and declares +0x0f0
 * `BLL_TRN1_QC_SLOW_K2` (with `BLL_TRN1_QC_SLOW_K1` recorded beside it as an
 * alias) and +0x0f4 `unnamed_0f4`, an `int` written by `setToDefault` and by
 * nothing else.  `setBllState`'s TRN1_QC_SLOW arm settles what those two are:
 * it copies +0x0f0 into `bllK1` and +0x0f4 into `bllK2`, with two plain
 * 32-bit `mov`s and no conversion, exactly as its thirteen sibling arms copy
 * the (K1, K2) pair at +0x088, +0x090, +0x098 and so on.  So +0x0f0 is the
 * K1 of the pair and +0x0f4 is the K2, and +0x0f4 holds a FLOAT.  The frozen
 * header is not edited here -- `asFloat` below reinterprets the four bytes,
 * which is what the object's `mov` does and what an `int`-typed read would
 * NOT do, since that would convert rather than copy.
 */

#include <stddef.h>

#include "dsplib/DspMath.h"
#include "dsplib/encode.h"
#include "dsplib/V90Resampler.h"
/*
 * Explicitly: the header declares `V90Parameters` and holds one as a pointer,
 * and this file is where it is dereferenced.
 */
#include "dsplib/V90Parameters.h"
#include "dsplib/sysdep.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define VR_OFF(field, off, tag) \
	typedef char vr_off_##tag[ \
	    ((int)__builtin_offsetof(V90Resampler, field) == (off)) ? 1 : -1]

VR_OFF(bllState,		0x94, bllstate);
VR_OFF(stateSamples,		0x98, statesamples);
VR_OFF(countStateSamples,	0x9c, countstatesamples);
VR_OFF(params,			0xa0, params);
VR_OFF(timingHistory,		0xa4, timinghistory);
VR_OFF(timingHistoryLen,	0xa8, timinghistorylen);
VR_OFF(timingHistoryIndex,	0xac, timinghistoryindex);
VR_OFF(periodSamples,		0xb0, periodsamples);
typedef char vr_size[(sizeof(V90Resampler) == 0xb4) ? 1 : -1];
#endif

/* See the file comment.  A copy of the bits, not a conversion. */
static float
asFloat(int bits)
{
	float f;

	__builtin_memcpy(&f, &bits, sizeof f);
	return f;
}

/*
 * `params` and `timingHistory` are initialised in the member-initialiser list
 * and `timingHistory` is then assigned in the body, which is why the object
 * stores a zero at +0xa4 that the `sysdep_malloc` result immediately
 * overwrites: the compiler cannot prove the first store dead across an opaque
 * call.  The order of the two initialiser-list stores -- +0xa0 then +0xa4 --
 * is declaration order and matches.
 */
V90Resampler::V90Resampler(unsigned int nPhases, float scale,
			   unsigned int nTaps, float cutoff,
			   V90Parameters *p, float ppm,
			   unsigned int minHistory)
	: ResamplerTiming(nPhases, scale, nTaps, cutoff, ppm, minHistory),
	  params(p), timingHistory(0)
{
	timingHistory = (float *)sysdep_malloc(
	    p->TIMING_HISTORY_EVALUATION_BUFFER_LENGTH * sizeof(float));
	timingHistoryLen = params->TIMING_HISTORY_EVALUATION_BUFFER_LENGTH;

	reset();
	setTimingOffset(ppm);
}

V90Resampler::V90Resampler(unsigned int nPhases, float scale,
			   unsigned int nTaps, float *bank,
			   V90Parameters *p, float ppm,
			   unsigned int minHistory)
	: ResamplerTiming(nPhases, scale, nTaps, bank, ppm, minHistory),
	  params(p), timingHistory(0)
{
	timingHistory = (float *)sysdep_malloc(
	    p->TIMING_HISTORY_EVALUATION_BUFFER_LENGTH * sizeof(float));
	timingHistoryLen = params->TIMING_HISTORY_EVALUATION_BUFFER_LENGTH;

	reset();
	setTimingOffset(ppm);
}

/* `timingHistory` is the only thing this level owns. */
V90Resampler::~V90Resampler()
{
	if (timingHistory)
		sysdep_free(timingHistory);
}

/*
 * `bllState` IS SEEDED WITH 1 BEFORE `setBllState(V90_BLL_FROZEN, 1)`, and
 * that is not a typo for 0: `setBllState` returns immediately when the state
 * it is given is the one already recorded, so seeding with FROZEN itself
 * would make the call that follows do nothing and leave the gains at whatever
 * `ResamplerTiming::reset` left them.  1 is SECOND_ORDER_FROZEN; any value
 * other than 0 would do, and the object's is 1.
 */
void
V90Resampler::reset()
{
	unsigned int i;

	ResamplerTiming::reset(1);

	stateSamples = 0;
	bllState = V90_BLL_SECOND_ORDER_FROZEN;
	setBllState(V90_BLL_FROZEN, 1);

	for (i = 0; i < timingHistoryLen; i++)
		timingHistory[i] = 0;

	timingHistoryIndex = 0;
	periodSamples = 0;
}

/*
 * A sixteen-way jump table at .rodata:0xbc0.  Every arm loads a consecutive
 * (K1, K2) pair out of `V90Parameters` and prints its own name; the two that
 * do not -- FROZEN and SECOND_ORDER_FROZEN -- zero both gains and just the
 * second one respectively.
 *
 * THE EARLY RETURN IS FIRST AND IT MATTERS.  Nothing happens at all when the
 * state is unchanged: not the gains, not `countStateSamples`, and not the
 * `stateSamples` reset.  A state that is entered twice in a row therefore
 * keeps counting from where it was.
 *
 * The range test is the switch's own: `cmp $0xf,%esi / jbe` guards the jump
 * table, and a state outside 0..15 falls through to the tail, recording
 * itself without touching the gains.
 */
void
V90Resampler::setBllState(V90BllState state, unsigned int countSamples)
{
	if (bllState == state)
		return;

	countStateSamples = countSamples;

	switch (state) {
	case V90_BLL_FROZEN:
		bllK1 = 0;
		bllK2 = 0;
		edprintf("V90Resampler: state = FROZEN\r\n");
		break;
	case V90_BLL_SECOND_ORDER_FROZEN:
		bllK2 = 0;
		edprintf("V90Resampler: state = SECOND ORDER FROZEN\r\n");
		break;
	case V90_BLL_INITIAL:
		bllK1 = params->BLL_INITIAL_K1;
		bllK2 = params->BLL_INITIAL_K2;
		edprintf("V90Resampler: state = INITIAL\r\n");
		break;
	case V90_BLL_FAST:
		bllK1 = params->BLL_FAST_K1;
		bllK2 = params->BLL_FAST_K2;
		edprintf("V90Resampler: state = FAST\r\n");
		break;
	case V90_BLL_MEDIUM:
		bllK1 = params->BLL_MEDIUM_K1;
		bllK2 = params->BLL_MEDIUM_K2;
		edprintf("V90Resampler: state = MEDIUM\r\n");
		break;
	case V90_BLL_SLOW:
		bllK1 = params->BLL_SLOW_K1;
		bllK2 = params->BLL_SLOW_K2;
		edprintf("V90Resampler: state = SLOW\r\n");
		break;
	case V90_BLL_SLOW2:
		bllK1 = params->BLL_SLOW2_K1;
		bllK2 = params->BLL_SLOW2_K2;
		edprintf("V90Resampler: state = SLOW2\r\n");
		break;
	case V90_BLL_DIL:
		bllK1 = params->BLL_DIL_K1;
		bllK2 = params->BLL_DIL_K2;
		edprintf("V90Resampler: state = DIL\r\n");
		break;
	case V90_BLL_TRN2_INITIAL:
		bllK1 = params->BLL_TRN2_INITIAL_K1;
		bllK2 = params->BLL_TRN2_INITIAL_K2;
		edprintf("V90Resampler: state = TRN2 INITIAL\r\n");
		break;
	case V90_BLL_TRN2:
		bllK1 = params->BLL_TRN2_K1;
		bllK2 = params->BLL_TRN2_K2;
		edprintf("V90Resampler: state = TRN2 \r\n");
		break;
	case V90_BLL_STEADY_STATE:
		bllK1 = params->BLL_STEADY_STATE_K1;
		bllK2 = params->BLL_STEADY_STATE_K2;
		edprintf("V90Resampler: state = STEADY_STATE\r\n");
		break;
	case V90_BLL_PRE_ANSPCM:
		bllK1 = params->BLL_PRE_ANSPCM_K1;
		bllK2 = params->BLL_PRE_ANSPCM_K2;
		edprintf("V90Resampler: state = PRE_ANSPCM\r\n");
		break;
	case V90_BLL_TRN1_QC_INITIAL:
		bllK1 = params->BLL_TRN1_QC_INITIAL_K1;
		bllK2 = params->BLL_TRN1_QC_INITIAL_K2;
		edprintf("V90Resampler: state = TRN1_QC_INITIAL\r\n");
		break;
	case V90_BLL_TRN1_QC_FAST:
		bllK1 = params->BLL_TRN1_QC_FAST_K1;
		bllK2 = params->BLL_TRN1_QC_FAST_K2;
		edprintf("V90Resampler: state = TRN1_QC_FAST\r\n");
		break;
	case V90_BLL_TRN1_QC_MEDIUM:
		bllK1 = params->BLL_TRN1_QC_MEDIUM_K1;
		bllK2 = params->BLL_TRN1_QC_MEDIUM_K2;
		edprintf("V90Resampler: state = TRN1_QC_MEDIUM\r\n");
		break;
	case V90_BLL_TRN1_QC_SLOW:
		/* See the file comment for +0x0f0 and +0x0f4. */
		bllK1 = params->BLL_TRN1_QC_SLOW_K2;
		bllK2 = asFloat(params->unnamed_0f4);
		edprintf("V90Resampler: state = TRN1_QC_SLOW\r\n");
		break;
	}

	bllState = state;
	stateSamples = 0;
}

/*
 * `stateSamples` only advances while `countStateSamples` -- the second
 * argument the state was entered with -- is non-zero.
 *
 * The timing-history ring is appended to once every
 * TIMING_HISTORY_EVALUATION_PERIOD samples, and `periodSamples` is compared
 * UNSIGNED against that `int` parameter (`jae`), which is what C++ does to a
 * mixed comparison anyway; the cast is written out to keep -Wextra quiet.
 */
void
V90Resampler::resample(const float *in, unsigned int n, float *out,
		       unsigned int &nOut)
{
	Resampler::resample(in, n, out, nOut);

	if (countStateSamples)
		stateSamples += n;

	if (params->TIMING_HISTORY_EVALUATION_ENABLED) {
		periodSamples += n;
		if (periodSamples
		    >= (unsigned int)params->TIMING_HISTORY_EVALUATION_PERIOD) {
			unsigned int next = timingHistoryIndex + 1;

			periodSamples = 0;
			timingHistory[timingHistoryIndex] =
			    getTimingOffsetPPM();
			if (next == timingHistoryLen)
				next = 0;
			timingHistoryIndex = next;
		}
	}
}

float
V90Resampler::getTimingHistoryMean()
{
	return mean(timingHistory, timingHistoryLen);
}

/*
 * `sqrt` OF THE MAGNITUDE, and the object takes it by MULTIPLYING: `fcoms`
 * against the 0.0f at .rodata.cst4+0x1cc, then either `fld1` or the -1.0f at
 * +0x1d0, then `fmulp` and `fsqrt`.  Written the same way rather than as
 * `fabs`, which is a different instruction and would be a different function.
 */
float
V90Resampler::getTimingHistoryStd()
{
	float var = Var(timingHistory, timingHistoryLen);

	return __builtin_sqrt(var * (var < 0.0f ? -1.0f : 1.0f));
}
