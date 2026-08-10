/*
 * V90ConnectionEvaluator.cpp -- three of the fourteen members of the V.90
 * connection evaluator: the constructor, the destructor and `reset`.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90ConnectionEvaluator.h`
 * carries the object map and the evidence for it, including why the class
 * used to live inside `V90Demodulator.h` and what did and did not move when
 * it came out.
 *
 * THE OTHER ELEVEN ARE THE CLASS'S PROCESSING HALF and are 8,000-odd bytes;
 * `evaluateConnection` alone is 3,857.  They are declared in the header for
 * the record and not defined here.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215):
 * `mov 0x10(%esp),%ebx` after one push and an eight-byte frame.
 *
 * `reset` HAS TWO DIAGNOSTICS AND ONLY ONE OF THEM IS GATED, which is not a
 * transcription slip.  The gate at 0x3e3a8 guards a `dsplibs_debug_printf`
 * that jumps BACK into the straight-line path, and the `edprintf` at 0x3e3b5
 * is on that path unconditionally.  `edprintf` encodes rather than prints
 * (src/core/encode.c) and the V.90 half of the object uses it for messages
 * that are always emitted; `V90Equalizer::reset` and
 * `V90Demodulator::reset` have exactly the same pairing.
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/encode.h"
/*
 * The NAMED 0x558 `V90Parameters` map, not `V90PreFilter.h`'s 0x504 word
 * block.  Nineteen of this file's stores are copies out of that block and all
 * nineteen slots have the original author's own names, which a numeric index
 * would throw away.  No translation unit may include both definitions;
 * finding 1112.
 */
#include "dsplib/V90Parameters.h"
#include "dsplib/V90ConnectionEvaluator.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own.  Skipped on the 64-bit `check64` pass, where a 32-bit layout is
 * not what the compiler lays out.
 *
 * The first four assertions and the size were in `V90Demodulator.cpp` until
 * task #88 gave the class its own files; they are unchanged.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define CE_OFF(field, off, tag) \
	typedef char v90ce_off_##tag[ \
	    ((int)__builtin_offsetof(V90ConnectionEvaluator, field) == (off)) \
	    ? 1 : -1]

CE_OFF(params,				0x00, params);
CE_OFF(word_04,				0x04, word04);
CE_OFF(word_24,				0x24, word24);
CE_OFF(enableRrnDown,			0x28, enrrndown);
CE_OFF(enableRrnUp,			0x2c, enrrnup);
CE_OFF(nofRemoteRateRenegBeforeRetrain,	0x30, nofremrrn);
CE_OFF(debugAlternateDebug,		0x34, dbgalt);
CE_OFF(debugRateDown,			0x44, dbgratedown);
CE_OFF(retrainCounterFadeCount,		0x48, retrfade);
CE_OFF(rateUpDetectDuration,		0x50, rateupdur);
CE_OFF(retrainDetectDuration,		0x60, retrdur);
CE_OFF(word_64,				0x64, word64);
CE_OFF(word_68,				0x68, word68);
CE_OFF(debugPeriod,			0x6c, dbgperiod);
CE_OFF(word_70,				0x70, word70);
CE_OFF(word_74,				0x74, word74);
CE_OFF(word_80,				0x80, word80);
CE_OFF(word_84,				0x84, word84);
CE_OFF(word_88,				0x88, word88);
CE_OFF(word_8c,				0x8c, word8c);
CE_OFF(short_9c,			0x9c, short9c);
CE_OFF(short_9e,			0x9e, short9e);
CE_OFF(word_a8,				0xa8, worda8);
CE_OFF(phase4ErrorForV34Fallback,	0xac, p4err);
CE_OFF(short_b0,			0xb0, shortb0);
CE_OFF(short_b4,			0xb4, shortb4);
typedef char v90ce_size[(sizeof(V90ConnectionEvaluator) == 0xbc) ? 1 : -1];
#endif

/*
 * The two 1600s.  `movl $0x640,0x64(%ebx)` and the same at +0x68, neither
 * carrying a relocation, so both are integers (CLAUDE.md's tools/dis.py rule)
 * -- and 1600 is not one of the parameter block's values, so it really is a
 * literal in the source and not a copy this reconstruction failed to find.
 */
#define CONNEVAL_INITIAL_PERIOD	1600

/*
 * reset -- forty-eight stores, nineteen of them the configuration.
 *
 * The order below is the object's store order, which is NOT the order a
 * reader would write these in: the compiler interleaved the parameter loads
 * with the zeroes to fill load latency, so the nine counters at +0x04..+0x24
 * come out in the middle and the configuration copies come out in two runs.
 * Statement order is the one thing GCC does not simply preserve (CLAUDE.md),
 * so this is a transcription of the stores and not a claim about the source.
 */
void
V90ConnectionEvaluator::reset()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConnectionEvaluator reset called !\r\n");

	edprintf(" *********** Echo Rrn Mechanism *********** \n");

	word_10 = 0;
	short_b4 = 0;
	short_b0 = 1;
	word_80 = 0;
	phase4ErrorForV34Fallback = params->PHASE4_ERROR_FOR_V34_FALLBACK;
	short_b2 = 0;
	word_84 = 0;
	short_9c = -1;
	word_90 = 0;
	word_8c = -1;

	word_14 = 0;
	word_18 = 0;
	word_04 = 0;
	word_0c = 0;
	word_08 = 0;
	word_24 = 0;
	word_1c = 0;
	word_20 = 0;

	word_94 = 0;
	word_78 = 0;
	word_7c = 0;
	word_74 = 0;

	short_9e = 0;
	enableRrnDown = params->ENABLE_RRN_DOWN;
	word_a0 = 0;
	word_a4 = 0;
	word_a8 = 0;
	word_70 = 0;

	enableRrnUp = params->ENABLE_RRN_UP;
	nofRemoteRateRenegBeforeRetrain =
	    params->NOF_REMOTE_RATE_RENEG_BEFORE_RETRAIN;
	debugAlternateDebug =
	    params->DEBUG_CONNECTION_EVALUATOR_ALTERNATE_DEBUG;
	debugFallBack = params->DEBUG_CONNECTION_EVALUATOR_FALL_BACK;
	debugRetrain = params->DEBUG_CONNECTION_EVALUATOR_RETRAIN;
	debugRateUp = params->DEBUG_CONNECTION_EVALUATOR_RATE_UP;
	debugRateDown = params->DEBUG_CONNECTION_EVALUATOR_RATE_DOWN;
	retrainCounterFadeCount = params->RETRAIN_COUNTER_FADE_COUNT;
	remoteRrnCounterFadeCount = params->REMOTE_RRN_COUNTER_FADE_COUNT;
	rateUpDetectDuration = params->RATE_UP_DETECT_DURATION;
	minDurationInDataBeforeRrnUp =
	    params->MINIMUM_DURATION_IN_DATA_BEFORE_RRN_UP;
	rateDownDetectDuration = params->RATE_DOWN_DETECT_DURATION;
	minDurationInDataBeforeRrnDown =
	    params->MINIMUM_DURATION_IN_DATA_BEFORE_RRN_DOWN;
	retrainDetectDuration = params->RETRAIN_DETECT_DURATION;

	word_64 = CONNEVAL_INITIAL_PERIOD;
	word_68 = CONNEVAL_INITIAL_PERIOD;
	debugPeriod = params->DEBUG_CONNECTION_EVALUATOR_PERIOD;
}

/*
 * The constructor: one store and a TAIL CALL to `reset`.  Fifteen bytes, and
 * `C1` and `C2` are byte-for-byte the same function at 0x3e560 and 0x3e570,
 * which is what a class with no base and no virtual member compiles to.
 */
V90ConnectionEvaluator::V90ConnectionEvaluator(V90Parameters *p)
{
	params = p;
	reset();
}

/*
 * The destructor: one byte, `c3`.  `D1` at 0x3e390 and `D2` at 0x3e380, and
 * no `D0` -- so it is not virtual, and the class owns no allocation.
 */
V90ConnectionEvaluator::~V90ConnectionEvaluator()
{
}
