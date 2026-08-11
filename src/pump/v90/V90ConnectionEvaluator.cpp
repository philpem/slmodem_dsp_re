/*
 * V90ConnectionEvaluator.cpp -- nine of the fourteen members of the V.90
 * connection evaluator: the constructor, the destructor, `reset`, and the six
 * small members of its processing half.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90ConnectionEvaluator.h`
 * carries the object map and the evidence for it, including why the class
 * used to live inside `V90Demodulator.h` and what did and did not move when
 * it came out.
 *
 * THE OTHER FIVE ARE THE BIG END OF THE PROCESSING HALF -- `evaluateConnection`
 * (3,857 bytes), `evaluatePhase4` (1,353), `evaluatePhase3` (980),
 * `evaluateMeanErrorStdPhase3` (234) and `printStatus` (177).  They are
 * declared in the header for the record and not defined here.
 *
 * WHAT THE SIX ADDED HERE PROVED ABOUT THE OBJECT.  Between them they name
 * seven fields the lifecycle batch could only number -- +0x04, +0x08, +0x0c,
 * +0x9e, +0xa0, +0xa4, +0xa8 -- and correct the type of +0x70, all out of the
 * object's own diagnostic strings and the parameters it compares against.
 * Three of them also return something, which the header's earlier reading of
 * "a return type is not mangled and is therefore unknown" left as `void`.
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
CE_OFF(nofV90Retrains,			0x04, nofv90retr);
CE_OFF(nofRemoteRateReneg,		0x08, nofrrn);
CE_OFF(nofRemoteRetrains,		0x0c, nofremretr);
CE_OFF(word_18,				0x18, word18);
CE_OFF(word_1c,				0x1c, word1c);
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
CE_OFF(word_90,				0x90, word90);
CE_OFF(short_9c,			0x9c, short9c);
CE_OFF(curDmin,				0x9e, curdmin);
CE_OFF(threshUp,			0xa0, threshup);
CE_OFF(threshDown,			0xa4, threshdown);
CE_OFF(threshRetrain,			0xa8, threshretr);
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
	nofV90Retrains = 0;
	nofRemoteRetrains = 0;
	nofRemoteRateReneg = 0;
	word_24 = 0;
	word_1c = 0;
	word_20 = 0;

	word_94 = 0;
	word_78 = 0;
	word_7c = 0;
	word_74 = 0;

	curDmin = 0;
	enableRrnDown = params->ENABLE_RRN_DOWN;
	threshUp = 0;
	threshDown = 0;
	threshRetrain = 0;
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

/*
 * updateAvePdsnr -- fold one measurement into the running average at +0x70.
 *
 * A WEIGHTED MEAN, not an exponential one: the new sample carries the weight
 * `nofSymbols` and the old average carries every symbol that went into it, so
 * the first call sets the average outright and every later one divides the
 * combined sum by the combined count.  The zero test at 0x3e599 is on the
 * COUNT and not on the average, which is what makes the first call special
 * rather than the first non-zero one.
 *
 * BOTH CONVERSIONS OF THE COUNT ARE UNSIGNED and that is forced, not chosen:
 *
 *     3e5a4:  52                push %edx        %edx = 0
 *     3e5a7:  50                push %eax
 *     3e5a8:  df 2c 24          fildll (%esp)
 *
 * is a 64-bit load of a value whose high word is a hard zero -- the sequence
 * GCC emits for `unsigned int` to floating point and never for `int`, which
 * needs no high word at all.  The count really can exceed 2^31: it is a sum
 * of symbol counts and nothing resets it but `reset` and `enterPhase3`.
 *
 * `de f9` AT 0x3e5ca IS `FDIVP`, NOT THE `fdivrp` objdump prints (finding
 * 245 and CLAUDE.md).  So the quotient is sum/count and not count/sum, and
 * getting it backwards would have produced a plausible float that no
 * side-against-side comparison could distinguish -- both sides would have
 * been wrong together.
 *
 * ONE ROUNDING, at the end.  The whole expression stays on the x87 stack at
 * 80-bit extended and `fstps` rounds once; that is what the object does and
 * it is why this is written as a single expression rather than in steps.
 */
void
V90ConnectionEvaluator::updateAvePdsnr(float pdsnr, unsigned int nofSymbols)
{
	unsigned int nofOldSymbols = word_74;

	if (nofOldSymbols == 0) {
		word_70 = pdsnr;
		word_74 = nofSymbols;
		return;
	}

	word_70 = (nofOldSymbols * word_70 + pdsnr * nofSymbols)
		  / (nofOldSymbols + nofSymbols);
	word_74 = nofOldSymbols + nofSymbols;
}

/*
 * updateCurrentConstellationData -- the three thresholds and the distance
 * they are thresholds on, plus the diagnostic that names all four.
 *
 * THE STRING IS THE DERIVATION for every field this writes; see the header.
 * The three printed values are each multiplied by a float 10.0 out of
 * `.rodata.cst4` and truncated toward zero -- `or $0xc00,%cx` sets both
 * rounding-control bits, which is round-to-zero and not round-to-nearest --
 * so `(int)` is the right spelling and `lrintf` would not be.
 *
 * THE SECTION THE RELOCATION NAMES DOES NOT SAY HOW THE TEN WAS SPELT, and a
 * first version of this comment claimed it did.  `.rodata.cst4` holds a
 * four-byte constant, so `10` against a float gives a float 10.0f there -- but
 * so does `10.0`, because ten is exactly representable and GCC narrows the
 * constant back.  Both spellings were compiled and `objdump -d` diffed: the
 * instruction text is identical and no `.rodata.cst8` appears.  The mutation
 * set records that as an `equivalent` entry rather than as a claim.  What the
 * four bytes DO rule out is a `double` that is not exactly representable.
 *
 * `jmp edprintf` at the end is an ordinary tail call and says NOTHING about
 * the return type -- a void function tail-jumping to a non-void one is what
 * GCC emits.  The type is void because nothing in the 147 bytes arranges
 * %eax for a caller.
 */
void
V90ConnectionEvaluator::updateCurrentConstellationData(short dmin,
						       float up,
						       float down,
						       float retrain)
{
	threshUp = up;
	threshDown = down;
	threshRetrain = retrain;
	curDmin = dmin;
	word_90 = 0;

	edprintf("V90ConnectionEvaluator UPDATE: curDmin = %d, "
		 "10*threshUp = %d, 10*threshDown = %d, "
		 "10*threshRetrain = %d\r\n",
		 dmin, (int)(10 * up), (int)(10 * down), (int)(10 * retrain));
}

/*
 * indicateRemoteRateReneg -- count one, and say so.
 *
 * Forty-two bytes: clear +0x90, increment +0x08, print the new value.  The
 * diagnostic is NOT gated -- there is no `dsplibs_debug_level` test in the
 * function and `edprintf` encodes whether or not anything is listening -- so
 * this moves the encoder's key on every call at every level.  That is the
 * same unconditional `edprintf` shape `reset` has.
 */
void
V90ConnectionEvaluator::indicateRemoteRateReneg()
{
	word_90 = 0;
	nofRemoteRateReneg++;

	edprintf("V90ConnectionEvaluator: remote rrn indicated. "
		 "(rrn no %d).\r\n", nofRemoteRateReneg);
}

/*
 * indicateLocalRetrain -- count a V.90 retrain, and give up if there have
 * been too many.
 *
 * The comparison is `ja`, unsigned, against `params->MAX_NOF_V90_RETRAINS`,
 * which the parameter map calls `int`: an unsigned branch between the two is
 * what GCC emits when the LEFT side is unsigned, and it is why +0x04 is
 * `unsigned int`.  A signed +0x04 would have given `jg` here.
 *
 * THE COUNTER IS CLEARED ONLY ON THE PATH THAT GIVES UP.  0x40085 stores
 * zero to +0x04 after the diagnostic; the ordinary path leaves the running
 * total alone.  So the object counts up to the limit once, reports, restarts
 * -- it does not reset every time.
 *
 * The two verdicts are the two immediates in %esi; see the header.
 */
int
V90ConnectionEvaluator::indicateLocalRetrain()
{
	word_90 = 0;
	nofV90Retrains++;

	if (nofV90Retrains > (unsigned int)params->MAX_NOF_V90_RETRAINS) {
		edprintf("V90ConnectionEvaluator: initiating fall back to V34 "
			 "due to %d V90 retrains\r\n", nofV90Retrains);
		nofV90Retrains = 0;
		word_1c = 0;
		word_18 = 0;
		return V90CE_VERDICT_FALLBACK_V34;
	}

	word_1c = 0;
	word_18 = 0;
	return V90CE_VERDICT_RETRAIN;
}

/*
 * indicateRemoteRetrain -- the same 136 bytes against +0x0c and
 * `params->MAX_NOF_REMOTE_RETRAINS`, with the remote wording in the string.
 *
 * The two functions are NOT one function with a flag: 0x40020 and 0x400b0 are
 * separate symbols with separate strings, and the only structural difference
 * is that this one loads `this->params` before it touches the counter while
 * the local one loads it after -- free scheduling, ignored (CLAUDE.md).
 */
int
V90ConnectionEvaluator::indicateRemoteRetrain()
{
	word_90 = 0;
	nofRemoteRetrains++;

	if (nofRemoteRetrains > (unsigned int)params->MAX_NOF_REMOTE_RETRAINS) {
		edprintf("V90ConnectionEvaluator: initiating fall back to V34 "
			 "due to %d remote retrains\r\n", nofRemoteRetrains);
		nofRemoteRetrains = 0;
		word_1c = 0;
		word_18 = 0;
		return V90CE_VERDICT_FALLBACK_V34;
	}

	word_1c = 0;
	word_18 = 0;
	return V90CE_VERDICT_RETRAIN;
}

/*
 * evaluateMeanErrorStdPhase4 -- three bytes, `31 c0 c3`.
 *
 *     3fac0:  31 c0    xor %eax,%eax
 *     3fac2:  c3       ret
 *
 * It reads neither argument and touches no memory, so it is a stub that
 * always answers the same thing -- and the `xor` is the evidence it is not
 * void, because a void function has no reason to clear %eax at all.  Its
 * phase 3 counterpart at 0x3f9d0 is 234 bytes and really computes something,
 * which is what makes this one worth writing down rather than skipping: the
 * pair is a real asymmetry in the original and not an artefact.
 */
int
V90ConnectionEvaluator::evaluateMeanErrorStdPhase4(float, float)
{
	return 0;
}
