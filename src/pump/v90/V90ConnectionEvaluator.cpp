/*
 * V90ConnectionEvaluator.cpp -- eleven of the fourteen members of the V.90
 * connection evaluator: the constructor, the destructor, `reset`, the six
 * small members of its processing half, and the two phase evaluators.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90ConnectionEvaluator.h`
 * carries the object map and the evidence for it, including why the class
 * used to live inside `V90Demodulator.h` and what did and did not move when
 * it came out.
 *
 * THE OTHER THREE ARE THE BIG END OF THE PROCESSING HALF -- `evaluateConnection`
 * (3,857 bytes), `evaluateMeanErrorStdPhase3` (234) and `printStatus` (177).
 * They are declared in the header for the record and not defined here.
 * `evaluatePhase3` (980 bytes) and `evaluatePhase4` (1,353) ARE defined here,
 * and both return `int` -- see the header's verdict block.
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
CE_OFF(word_10,				0x10, word10);
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
CE_OFF(word_78,				0x78, word78);
CE_OFF(word_7c,				0x7c, word7c);
CE_OFF(word_80,				0x80, word80);
CE_OFF(word_84,				0x84, word84);
CE_OFF(word_88,				0x88, word88);
CE_OFF(word_8c,				0x8c, word8c);
CE_OFF(word_90,				0x90, word90);
/*
 * +0x98 and +0xb8 were `pad_98[4]` and the middle of `pad_b6[6]` until
 * `evaluateConnection` was read; the header carries the store that proves
 * each.  With them the class has no unmodelled region left.
 */
CE_OFF(word_98,				0x98, word98);
CE_OFF(short_9c,			0x9c, short9c);
CE_OFF(curDmin,				0x9e, curdmin);
CE_OFF(threshUp,			0xa0, threshup);
CE_OFF(threshDown,			0xa4, threshdown);
CE_OFF(threshRetrain,			0xa8, threshretr);
CE_OFF(phase4ErrorForV34Fallback,	0xac, p4err);
CE_OFF(short_b0,			0xb0, shortb0);
CE_OFF(short_b2,			0xb2, shortb2);
CE_OFF(short_b4,			0xb4, shortb4);
CE_OFF(word_b8,				0xb8, wordb8);
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

/*
 * ===========================================================================
 * The `%c%d.%03d` diagnostics
 * ===========================================================================
 *
 * NINE SITES ACROSS THE TWO EVALUATORS, every one of them written out by the
 * object.  The shape is the one `V90Demodulator::reset` and `V90PreFilter`
 * already carry -- a sign character, the truncated magnitude, and the first
 * three decimals of the fraction -- and the spelling here is theirs:
 *
 *     (0.0f < v) ? '+' : '-',  (int)__builtin_fabsf(v),  ce_frac3(v)
 *
 * THE SIGN IS `fldz` AND A COMPARE WITH THE VALUE, NOT A BRANCH.  `sbb %edx,
 * %edx; and $0xfffffffe,%edx; add $0x2d,%edx` is `0x2d - 2*CF`, so '+' when
 * the carry is set and '-' when it is not, and the carry is set when
 * `0.0f < v` OR the compare was unordered.  ZERO THEREFORE PRINTS '-', both
 * signs of it, which is what makes the spelling `0.0f < v` rather than
 * `v < 0.0f`; and a NaN prints '+', which no reachable input produces because
 * every site is behind a `v > threshold` that a NaN fails.
 *
 * THE MAGNITUDE TAKES ITS ABSOLUTE VALUE IN THE FLOAT DOMAIN.  `d9 e1`
 * (`fabs`) runs BEFORE the `fistpl`, so it is `(int)fabsf(v)` and not
 * `abs((int)v)`.  The two agree over every float, including the out-of-range
 * case where both conversions give 0x80000000, so this is forced by the
 * instruction selection rather than by any value.
 */

/*
 * The three decimals: truncate, subtract, scale, truncate again, and take the
 * absolute value of the INTEGER.  Written as a helper because the object
 * inlines it at nine sites and nine copies of one expression is nine places
 * for one of them to drift; the object's own factoring is not recoverable
 * either way.
 *
 * `(float)(int)v` IS THE OBJECT'S ROUND TRIP: `fistl` to a stack slot with
 * round-to-zero forced by `or $0xc00,%ax`, then `fildl` straight back.  It is
 * not a no-op -- it is what makes the subtraction yield a fraction.
 *
 * `(frac < 0) ? -frac : frac` rather than `abs()`, for the reason
 * `VPcmFloModem.cpp` gives: `cltd; xor %edx,%eax; sub %edx,%eax` returns
 * INT_MIN at INT_MIN and C's `abs()` is undefined there.
 *
 * 1000 IS A `.rodata.cst4` CONSTANT, four bytes, so it cannot have been a
 * `double` that is not exactly representable -- and 1000 is exactly
 * representable, so the section does not distinguish `1000`, `1000.0f` and
 * `1000.0` (finding 1384).  The mutation set records that as `equivalent`.
 */
static int
ce_frac3(float v)
{
	int frac = (int)((v - (float)(int)v) * 1000.0f);

	return (frac < 0) ? -frac : frac;
}

/*
 * TWO decimals, for `evaluateConnection`'s five `%c%d.%02d` sites.  Same
 * shape, same round trip, `100.0f` out of `.rodata.cst4+0x2ec` rather than a
 * thousand -- 0x3eb38, 0x3ef44, 0x3f21f, 0x3f33d and 0x3f42b all load it.
 * The two evaluators print three decimals and this function prints two; that
 * is a real difference between them and not a transcription slip.
 *
 * The local is `hundredths` rather than `frac` so that the mutation set can
 * anchor on this function's absolute value and on `ce_frac3`'s separately;
 * the two bodies are otherwise the same text.
 */
static int
ce_frac2(float v)
{
	int hundredths = (int)((v - (float)(int)v) * 100.0f);

	return (hundredths < 0) ? -hundredths : hundredths;
}

/*
 * `params->unnamed_434` IS A FLOAT AND `V90Parameters.h` TYPES IT `int`.
 * `evaluatePhase4` loads it with `flds 0x434(%ecx)` and stores it straight to
 * +0xac with `fsts`, and 0x437a0000 -- the value `setToDefault` plants there
 * (finding 878) -- is 250.0f.  Both measurements say float.
 *
 * The header is a SHARED, FROZEN type that this batch does not own, so the
 * slot is read through the type the object uses rather than retyped.  A union
 * because GCC defines the pun and needs no header for it; the four bytes are
 * the same four bytes either way.
 */
static float
ce_param_float(int bits)
{
	union {
		int i;
		float f;
	} u;

	u.i = bits;
	return u.f;
}

/*
 * ===========================================================================
 * evaluatePhase3 -- 980 bytes, 0x3f5f0
 * ===========================================================================
 *
 * IT RETURNS `int`, AND THE HEADER USED TO SAY `void`.  There is one epilogue,
 * at 0x3f667, and two ways in: the `je` at 0x3f608 arrives with the `xor
 * %eax,%eax` of 0x3f5f3 still standing, and the fall-through arrives after
 * `mov %esi,%eax` at 0x3f662.  %esi is loaded with an immediate 4 or 5 on
 * every path that decides something and is left at its entry zero on every
 * path that does not, which is the same two-immediates-into-%esi idiom
 * `indicateLocalRetrain` uses.  So the answer is 0, 4 or 5 -- and 0 is a real
 * third answer rather than an artefact, because the function has a do-nothing
 * path that reaches the same `ret`.
 *
 * THE THREE ARMS ARE AN `else if` CHAIN AND NOT THREE `if`s.  0x3f612 jumps
 * over the +0xb2 arm to the +0x88 test at 0x3f673, and 0x3f67b jumps over the
 * +0x88 arm to the +0x84 test at 0x3f751; nothing falls from one into the
 * next.  Only the third arm has two consecutive tests inside it.
 *
 * WHAT THE ARMS ARE, out of the strings on them:
 *
 *   +0xb2 != 0   "altRbsDetectedOnQc => initiating Retrain"  -- and the arm
 *                clears +0xb2, so the flag is the request and this is what
 *                services it.  See the header.
 *   +0x88 != 0   "(end of TRN1d) ... due to large error" -- compared against
 *                `params->TRN1D_ERROR_FOR_V34_FALLBACK`, which is the same
 *                phase named twice over.
 *   +0x84 != 0   the ordinary phase-3 arm, and the only one with two tests:
 *                a fall-back on `PHASE3_ERROR_FOR_V34_FALLBACK` and then, on
 *                BOTH outcomes of it, a retrain on `PDSNR_THRESHOLD_IN_PHASE3`.
 *
 * THE SECOND TEST IS REACHED FROM BOTH SIDES OF THE FIRST, which is the one
 * piece of this control flow a reader would get wrong.  0x3f87f jumps back to
 * 0x3f77a when the duration has not run out, and 0x3f926 -- the tail of the
 * fall-back diagnostic -- jumps to the same place.  So a single call can print
 * "fall back to V34 due to large error" with %esi = 5 and then overwrite %esi
 * with 4 at 0x3f93f and RETURN 4.  The test drives that.
 *
 * `word_10 >= word_64` AND `word_18 >= retrainDetectDuration` ARE UNSIGNED
 * (`jb` both, 0x3f6a0 and 0x3f797).  The first is unsigned on both sides
 * already; the second compares against the +0x60 COPY of
 * `RETRAIN_DETECT_DURATION`, which the map calls `int`, and an `unsigned int`
 * on the left is what makes the branch `jb` rather than `jl`.
 *
 * THE COUNTER RESTARTS ONLY WHERE THE VERDICT IS FALL-BACK.  0x3f745, 0x3f83d
 * and 0x3f910 each store zero to +0x04 and each is on a path that has just
 * printed a fall-back message; the retrain paths at 0x3f647 and 0x3f9bf skip
 * the store.  That is `indicateLocalRetrain`'s asymmetry again.
 *
 * THE TAIL CLEARS THE AVERAGE.  +0x74 and +0x70 are both zeroed on every path
 * that got past the entry test, so the running mean `updateAvePdsnr` builds is
 * consumed once per phase-3 evaluation and started again.  The early return
 * does NOT clear them, which is why the entry test is on the count.
 */
int
V90ConnectionEvaluator::evaluatePhase3()
{
	unsigned int nofSymbols = word_74;
	int verdict = V90CE_VERDICT_NONE;

	if (nofSymbols == 0)
		return V90CE_VERDICT_NONE;

	if (short_b2 != 0) {
		short_b2 = 0;
		nofV90Retrains++;
		if (nofV90Retrains
		    > (unsigned int)params->MAX_NOF_V90_RETRAINS) {
			verdict = V90CE_VERDICT_FALLBACK_V34;
			edprintf("V90ConnectionEvaluator (phase3): "
				 "altRbsDetectedOnQc => initiating fall back "
				 "to V34 due to %d V90 retrains\r\n",
				 nofV90Retrains);
			nofV90Retrains = 0;
		} else {
			verdict = V90CE_VERDICT_RETRAIN;
			edprintf("V90ConnectionEvaluator (phase3): "
				 "altRbsDetectedOnQc => initiating Retrain "
				 "(retrain no %d)\r\n", nofV90Retrains);
		}
		word_1c = 0;
		word_90 = 0;
	} else if (word_88 != 0) {
		if (word_70 > params->TRN1D_ERROR_FOR_V34_FALLBACK) {
			word_10 += nofSymbols;
			if (word_10 >= word_64) {
				verdict = V90CE_VERDICT_FALLBACK_V34;
				edprintf("V90ConnectionEvaluator (end of "
					 "TRN1d): initiating fall back to V34 "
					 "due to large error, avePdsnr = "
					 "%c%d.%03d\r\n",
					 (0.0f < word_70) ? '+' : '-',
					 (int)__builtin_fabsf(word_70),
					 ce_frac3(word_70));
				nofV90Retrains = 0;
				word_1c = 0;
				word_90 = 0;
			}
		} else {
			word_10 = 0;
		}
	} else if (word_84 != 0) {
		if (word_70 > params->PHASE3_ERROR_FOR_V34_FALLBACK) {
			word_10 += nofSymbols;
			if (word_10 >= word_64) {
				verdict = V90CE_VERDICT_FALLBACK_V34;
				edprintf("V90ConnectionEvaluator (phase3): "
					 "initiating fall back to V34 due to "
					 "large error, avePdsnr = %c%d.%03d\r\n",
					 (0.0f < word_70) ? '+' : '-',
					 (int)__builtin_fabsf(word_70),
					 ce_frac3(word_70));
				nofV90Retrains = 0;
				word_1c = 0;
				word_90 = 0;
			}
		} else {
			word_10 = 0;
		}

		if (word_70 > params->PDSNR_THRESHOLD_IN_PHASE3) {
			word_18 += nofSymbols;
			if (word_18 >= (unsigned int)retrainDetectDuration) {
				nofV90Retrains++;
				if (nofV90Retrains
				    > (unsigned int)params->MAX_NOF_V90_RETRAINS) {
					verdict = V90CE_VERDICT_FALLBACK_V34;
					edprintf("V90ConnectionEvaluator "
						 "(phase3): initiating fall "
						 "back to V34 due to %d V90 "
						 "retrains, avePdsnr = "
						 "%c%d.%03d\r\n",
						 nofV90Retrains,
						 (0.0f < word_70) ? '+' : '-',
						 (int)__builtin_fabsf(word_70),
						 ce_frac3(word_70));
					nofV90Retrains = 0;
				} else {
					verdict = V90CE_VERDICT_RETRAIN;
					edprintf("V90ConnectionEvaluator "
						 "(phase3): initiating Retrain "
						 "(retrain no %d), due to "
						 "avePdsnr = %c%d.%03d\r\n",
						 nofV90Retrains,
						 (0.0f < word_70) ? '+' : '-',
						 (int)__builtin_fabsf(word_70),
						 ce_frac3(word_70));
				}
				word_1c = 0;
				word_90 = 0;
				word_18 = 0;
			}
		} else {
			word_18 = 0;
		}
	}

	word_74 = 0;
	word_70 = 0;
	return verdict;
}

/*
 * ===========================================================================
 * evaluatePhase4 -- 1,353 bytes, 0x3fad0
 * ===========================================================================
 *
 * IT RETURNS `int` TOO, and it has THREE epilogues rather than one: 0x3fbd7
 * for the entry test, 0x3fd06 with `mov %esi,%eax`, and 0x3fbcf, which is the
 * odd one -- `mov $0x5,%eax` at 0x3fbaa, an immediate straight into the return
 * register with %esi never consulted.  That path is written as an early
 * `return` below because that is what it is.
 *
 * THE ARGUMENT IS `meanErrBefToAftUpdateRatio`, and the name is the object's
 * own: the two diagnostics that print the argument -- and no other value --
 * end "due to meanErrBefToAftUpdateRatio = %c%d.%03d".  It is compared against
 * `params->PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH`, which is the
 * same name a third time.
 *
 * THE PARAMETER IS THE LEFT OPERAND OF THAT COMPARISON, and that is forced.
 * The argument is already on the x87 stack from 0x3fadd, so `fcoms 0x438(%ecx)`
 * would have been one instruction; the object instead does `flds 0x438(%ecx);
 * fcomp %st(1)`, which is what GCC emits when the MEMORY operand is written
 * first.  Every other float compare in these two functions is the other way
 * round (`flds 0x70(%ebx); fcoms <param>`) and is spelt that way here.
 *
 * THE FIRST ARM IS THREE GUARDS AND IT SKIPS THE WHOLE REST OF THE FUNCTION.
 * 0x3fcb6 jumps past both of the +0x70 tests and past the delayed-retrain
 * test to the tail, so a call that services the mean-error request neither
 * accumulates +0x10 nor +0x18 nor honours a pending delayed retrain.  The
 * three guards are +0xb0 non-zero, the ratio over its threshold, and
 * `nofV90Retrains` STRICTLY BELOW `params->unnamed_45c` -- a second and lower
 * ceiling than `MAX_NOF_V90_RETRAINS`, read from a different slot, and
 * `setToDefault` plants 2 in it.  +0xb0 is cleared only when all three pass.
 *
 * `word_10` IS COMPARED AGAINST +0x68 HERE AND AGAINST +0x64 IN PHASE 3.
 * `reset` plants 1600 in both, so nothing but the disassembly separates them:
 * 0x3fb13 is `cmp 0x68(%ebx),%ecx` and 0x3f69a is `cmp 0x64(%ebx),%eax`.  The
 * test gives them different values for exactly that reason.
 *
 * +0xac IS BOTH READ AND WRITTEN HERE.  It is read as the fall-back threshold
 * for the average, and on the retrain path it is REPLACED by
 * `params->unnamed_434` -- and the diagnostic that announces the store names
 * the field: "pdsnrCurrentV34DropThreshPhase4 set to = %c%d.%03d".  That is
 * the author's own name for +0xac, recorded in the header; the field keeps the
 * name the lifecycle batch gave it because `t_v90leaves.cpp` uses that name and
 * belongs to other work.
 *
 * THAT ONE DIAGNOSTIC IS GATED AND THE OTHER EIGHT ARE NOT.  0x3fd31 is
 * `cmpl $0x1,dsplibs_debug_level` and the call at 0x3fdc3 is
 * `dsplibs_debug_printf`, the readable channel; every other call in these two
 * functions is `edprintf`, which encodes whether or not anything is listening.
 *
 * THE LAST ARM HAS A FORMAT ARGUMENT MISSING, IN THE OBJECT.  0x3ffcf pushes
 * the string at 0xab34 -- "initiating fall back to V34 due to %d V90 retrains
 * (last one delayed)" -- and calls `edprintf` with NOTHING stored to
 * 0x4(%esp), while its sibling at 0x3fffb stores `nofV90Retrains` there for
 * the string that also has a `%d`.  So `vsnprintf` reads the outgoing-argument
 * slot as an `int` and prints whatever the frame happened to hold.  This is
 * reproduced rather than repaired -- it is what the object does -- and it is
 * why the test compares state and verdict but not the transcript on that one
 * path.  Finding 1388.
 */
int
V90ConnectionEvaluator::evaluatePhase4(float meanErrBefToAftUpdateRatio)
{
	unsigned int nofSymbols = word_74;
	int verdict = V90CE_VERDICT_NONE;

	if (nofSymbols == 0)
		return V90CE_VERDICT_NONE;

	if (short_b0 != 0
	    && params->PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH
	       < meanErrBefToAftUpdateRatio
	    && nofV90Retrains < (unsigned int)params->unnamed_45c) {
		nofV90Retrains++;
		short_b0 = 0;
		if (nofV90Retrains
		    > (unsigned int)params->MAX_NOF_V90_RETRAINS) {
			verdict = V90CE_VERDICT_FALLBACK_V34;
			edprintf("V90ConnectionEvaluator (phase4): initiating "
				 "fall back to V34 due to %d V90 retrains, "
				 "meanErrBefToAftUpdateRatio = %c%d.%03d\r\n",
				 nofV90Retrains,
				 (0.0f < meanErrBefToAftUpdateRatio)
				     ? '+' : '-',
				 (int)__builtin_fabsf(
				     meanErrBefToAftUpdateRatio),
				 ce_frac3(meanErrBefToAftUpdateRatio));
			nofV90Retrains = 0;
		} else {
			verdict = V90CE_VERDICT_RETRAIN;
			edprintf("V90ConnectionEvaluator (phase4): initiating "
				 "Retrain (retrain no %d), due to "
				 "meanErrBefToAftUpdateRatio = %c%d.%03d\r\n",
				 nofV90Retrains,
				 (0.0f < meanErrBefToAftUpdateRatio)
				     ? '+' : '-',
				 (int)__builtin_fabsf(
				     meanErrBefToAftUpdateRatio),
				 ce_frac3(meanErrBefToAftUpdateRatio));
		}
		word_1c = 0;
		word_90 = 0;
		word_18 = 0;
	} else {
		if (word_70 > phase4ErrorForV34Fallback) {
			word_10 += nofSymbols;
			if (word_10 >= word_68) {
				edprintf("V90ConnectionEvaluator (phase4): "
					 "initiating fall back to V34 due to "
					 "large error, avePdsnr = %c%d.%03d\r\n",
					 (0.0f < word_70) ? '+' : '-',
					 (int)__builtin_fabsf(word_70),
					 ce_frac3(word_70));
				nofV90Retrains = 0;
				word_1c = 0;
				word_90 = 0;
				word_74 = 0;
				word_70 = 0;
				return V90CE_VERDICT_FALLBACK_V34;
			}
		} else {
			word_10 = 0;
		}

		if (word_70 > params->PDSNR_THRESHOLD_IN_PHASE4) {
			word_18 += nofSymbols;
			if (word_18 >= (unsigned int)retrainDetectDuration) {
				nofV90Retrains++;
				if (nofV90Retrains
				    > (unsigned int)params->MAX_NOF_V90_RETRAINS) {
					verdict = V90CE_VERDICT_FALLBACK_V34;
					edprintf("V90ConnectionEvaluator "
						 "(phase4): initiating fall "
						 "back to V34 due to %d V90 "
						 "retrains, avePdsnr = "
						 "%c%d.%03d\r\n",
						 nofV90Retrains,
						 (0.0f < word_70) ? '+' : '-',
						 (int)__builtin_fabsf(word_70),
						 ce_frac3(word_70));
					nofV90Retrains = 0;
				} else {
					float t = ce_param_float(
					    params->unnamed_434);

					phase4ErrorForV34Fallback = t;
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "V90ConnectionEvaluator "
						    "(phase4): "
						    "pdsnrCurrentV34DropThresh"
						    "Phase4 set to = "
						    "%c%d.%03d\r\n",
						    (0.0f < t) ? '+' : '-',
						    (int)__builtin_fabsf(t),
						    ce_frac3(t));
					verdict = V90CE_VERDICT_RETRAIN;
					edprintf("V90ConnectionEvaluator "
						 "(phase4): initiating Retrain "
						 "(retrain no %d), due to "
						 "avePdsnr = %c%d.%03d\r\n",
						 nofV90Retrains,
						 (0.0f < word_70) ? '+' : '-',
						 (int)__builtin_fabsf(word_70),
						 ce_frac3(word_70));
				}
				word_1c = 0;
				word_90 = 0;
				word_18 = 0;
			}
		} else {
			word_18 = 0;
		}

		if (word_78 != 0 && word_7c != 0) {
			edprintf("V90ConnectionEvaluator (phase4): Initiating "
				 "retrain (delayed)...\r\n");
			word_78 = 0;
			word_7c = 0;
			nofV90Retrains++;
			if (nofV90Retrains
			    > (unsigned int)params->MAX_NOF_V90_RETRAINS) {
				verdict = V90CE_VERDICT_FALLBACK_V34;
				/*
				 * NO ARGUMENT FOR THE `%d`, and that is the
				 * object's: 0x3ffcf stores only the string.
				 * See the block comment above.
				 */
				edprintf("V90ConnectionEvaluator (phase4): "
					 "initiating fall back to V34 due to "
					 "%d V90 retrains (last one "
					 "delayed)\r\n");
				nofV90Retrains = 0;
			} else {
				verdict = V90CE_VERDICT_RETRAIN;
				edprintf("V90ConnectionEvaluator (phase4): "
					 "initiating Retrain (retrain no %d), "
					 "due to delayed retrain request\r\n",
					 nofV90Retrains);
			}
			word_1c = 0;
			word_90 = 0;
		}
	}

	word_74 = 0;
	word_70 = 0;
	return verdict;
}

/*
 * ===========================================================================
 * evaluateConnection -- 3,857 bytes, 0x3e6d0
 * ===========================================================================
 *
 * The whole of the class's decision making, and the only member that answers
 * anything but 0, 4 and 5.  Six values reach %eax and the header's verdict
 * block lists every immediate that produces one; -1 is NOT among them.
 *
 * IT IS FIVE STAGES, in this order, and every one of them can raise a verdict
 * that a later one then overrides:
 *
 *   1  the fade clock          three counters lose one each time +0x20
 *                              crosses a multiple of their fade count
 *   2  the external demand     +0x8c holds a code somebody else planted;
 *                              THIS STAGE RETURNS on its own default arm
 *   3  rate up                 avePdsnr below `threshUp` for long enough
 *   4  rate down and retrain   avePdsnr above `threshDown` / `threshRetrain`,
 *                              with an alternative "echo RRN" reading of the
 *                              rate-down threshold selected by a parameter
 *   5  the debug arms          five mutually exclusive forced verdicts
 *
 * THE EXTERNAL-DEMAND STAGE HAS ITS OWN EPILOGUE.  0x3e8b7 is reached only
 * from the empty call and from that stage's default arm, and that arm is the
 * only place in the function where the answer travels in %ecx rather than
 * %ebp.  It is written as a `return` below because that is what it is: the
 * arm clears the average, plants -1 in +0x8c and answers, and stages 3 to 5
 * never run.
 *
 * WHAT THE SWITCH ON +0x8c IS.  0x3e760 tests `word_8c > -1` and 0x3e769
 * range-checks 0..6 before a bit-test dispatch, so the two comparisons are
 * two source-level things: an `if` that skips the whole stage when nothing is
 * pending, and a `switch` whose default arm catches 0 and everything above 6.
 *
 *     mov $0x1,%eax; shl %cl,%eax
 *     test $0x36,%al   ->  1, 2, 4, 5      the external RRN demands
 *     test $0x08,%al   ->  3               RRN up
 *     test $0x40,%al   ->  6               fast parameter exchange
 *
 * `word_8c = -1` IS THE ACKNOWLEDGEMENT and every arm reaches it -- the three
 * cases through 0x3ed63 and the default arm through its own epilogue at
 * 0x3e8a4.
 *
 * THE FADE DIVISIONS ARE UNSIGNED AND THE COMPARISON IS SIGNED, which is not
 * a contradiction and is the one shape here a reader would spell wrong.
 * `divl 0x48(%edi)` twice and then `cmp %eax,0x1c(%esp); jg` (0x3e715 ..
 * 0x3e727): an unsigned quotient stored into an `int`, and two `int`s
 * compared.  Writing the condition as one unsigned expression would have
 * given `ja`.
 *
 * THE ECHO-RRN VARIANT IS SELECTED BY A PARAMETER AND NOT BY A FIELD.
 * `params->HIGH_LEVEL_TX_ACTIVE` at 0x3e97b chooses between scaling
 * `avePdsnr` by 1.52 or 1.39 before the `threshDown` comparison and not
 * scaling it at all, and the state machine that picks which is +0xb4, the
 * `echoRrnState` the header names.  Its terminal state 3 falls back to the
 * unscaled reading -- 0x3ee9f jumps straight into the plain arm -- which is
 * why the plain arm is a helper here rather than written out twice.
 *
 * IT WRITES A PARAMETER.  `params->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE`
 * takes 2.0f at 0x3eecc and 0.65f or 1.8f at 0x3ead8, depending on whether
 * the new `echoRrnState` is 1.  A `movl $0x40000000` into a `float` slot is
 * how GCC stores a float constant to memory, so these are float stores and
 * not integer ones.  It is the only member of the class that writes through
 * the parameter pointer, which is why the test compares the two sides'
 * parameter blocks against each other rather than against a shadow.
 *
 * THREE DIAGNOSTICS ARE GATED AND THE OTHER TWENTY-ONE ARE NOT.  `edprintf`
 * encodes whether or not anything is listening; the three
 * `dsplibs_debug_printf` calls at 0x3f02c, 0x3f0d1 and 0x3f174 -- and their
 * duplicates at 0x3f48e, 0x3f533 and 0x3f5d7 -- each sit behind their OWN
 * `cmpl $0x1,dsplibs_debug_level`, three separate tests and not one.
 *
 * `%02d` HERE, `%03d` IN THE TWO PHASE EVALUATORS.  Five sites load 100.0f
 * and the two echo-RRN diagnostics load 1000.0 as a DOUBLE (`.rodata.cst8
 * +0xd8`, and once built on the stack as 0x408f4000 in the high word).  A
 * thousand is exact in both formats and the product is formed at 80 bits
 * either way, so `ce_frac3` serves the double sites too; the spelling is
 * recorded here rather than in the code.
 *
 * WHAT IS NOT MODELLED, AND IT IS THE COMPILER'S RATHER THAN OURS.  0x3e933
 * is `fcomps 0xa0(%edi); jae`, the complement of `avePdsnr < threshUp` taken
 * without consulting the parity flag -- so the blob RUNS the rate-up arm for
 * a NaN average, where C says it must not.  GCC 13 complements the same
 * source correctly.  Finding 1389; the test keeps NaN away from that one
 * comparison and feeds it to the other four, which are `ja`/`jbe` and agree.
 */

/*
 * The two diagnostics the echo-RRN machine prints after whichever of its two
 * headline lines applies -- byte for byte the same code at 0x3f03e and
 * 0x3f4a0, and again at 0x3f0e3 and 0x3f545.  Each has its own gate.
 *
 * THE PRODUCT IS NOT ROUNDED TO `float` BETWEEN THE THREE USES.  The object
 * forms `avePdsnr * word_b8` in an x87 register and reads it three times at 80
 * bits; the exact product of two floats needs up to 48 mantissa bits, so
 * rounding it back to a float would change `(int)` of it wherever it sits just
 * under an integer -- 10.0f * 2.3f is 22.99999952, which truncates to 22
 * unrounded and to 23 rounded, and the test plants exactly those two values.
 *
 * THE TWO-ARGUMENT HELPER IS BELT AND BRACES AND NOT A NECESSITY, which was
 * measured rather than assumed: `ce_frac3((float)(word_70 * word_b8))` was
 * compiled beside this and it computes the SAME number, because
 * FLT_EVAL_METHOD is 2 on an x87 target and neither the cast nor the
 * parameter narrows the value.  The multiply is kept inside anyway, because
 * that is the one spelling no compiler can round, and the object's own
 * factoring is not recoverable either way.  Finding 1389.
 *
 * THE THOUSAND IS A DOUBLE HERE.  0x3f08b is `fldl .rodata.cst8+0xd8`, eight
 * bytes, where `ce_frac3`'s sites in the two phase evaluators load four -- but
 * a thousand is exact in both formats and the product is formed at 80 bits, so
 * this is a difference in the source and not in the answer.
 */
static int
ce_frac3_of(float a, float b)
{
	int thousandths = (int)((a * b - (float)(int)(a * b)) * 1000.0);

	return (thousandths < 0) ? -thousandths : thousandths;
}

static void
ce_echo_rrn_debug(V90ConnectionEvaluator *ce)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("error -- = %c%d.%03d\r\n",
		    (0.0f < ce->word_70 * ce->word_b8) ? '+' : '-',
		    (int)__builtin_fabsf(ce->word_70 * ce->word_b8),
		    ce_frac3_of(ce->word_70, ce->word_b8));

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("threshold -- = %c%d.%03d\r\n",
		    (0.0f < ce->threshDown) ? '+' : '-',
		    (int)__builtin_fabsf(ce->threshDown),
		    ce_frac3(ce->threshDown));
}

/*
 * The unscaled rate-down test, reached from two places -- the ordinary
 * `enableRrnDown` arm at 0x3ebc0 and the echo-RRN machine's terminal state,
 * which jumps into the middle of it at 0x3ebc7.  A helper because the object
 * has one copy and two entries, and "did it fire" is all the caller needs.
 */
static int
ce_plain_rate_down(V90ConnectionEvaluator *ce, unsigned int nofSymbols)
{
	int demanded = 0;

	if (ce->word_70 > ce->threshDown) {
		ce->word_14 += nofSymbols;
		if (ce->word_14 >= (unsigned int)ce->rateDownDetectDuration
		    && ce->word_1c
		       >= (unsigned int)ce->minDurationInDataBeforeRrnDown) {
			ce->word_98 = 0;
			ce->word_90 = ce->params->RRN_SILENCE_REQUESTED;
			ce->word_14 = 0;
			ce->word_1c = 0;
			demanded = 1;
		}
	} else {
		ce->word_14 = 0;
	}

	return demanded;
}

/*
 * The echo-RRN machine's rate-down test: the same accumulation against a
 * `threshDown` the average has first been SCALED against, and a fire
 * condition that is two alternatives rather than one.  0x3e9dd .. 0x3eaf0.
 *
 * `word_14 >= rateDownDetectDuration / 3` AND `word_14 >= .../ 5` ARE BOTH
 * UNSIGNED DIVISIONS: 0x3ea0c is `mov $0xaaaaaaab,%edx; mul %edx; shr $1,%edx`
 * and 0x3ea4d is `mov $0xcccccccd,%edx; mul %edx; shr $2,%edx`, the reciprocal
 * sequences for an unsigned divide by three and by five.  A signed divide by
 * three is `imul $0x55555556` and a different shift.
 *
 * THE DWELL COMPARISONS ARE IN DOUBLE, and 1.3 and 0.6 are eight-byte
 * constants (`.rodata.cst8+0xc8` and `+0xd0`) because neither is exactly
 * representable -- so unlike the tens and thousands elsewhere in this file the
 * section really does fix the type.  `word_1c` reaches the x87 through `push
 * $0; push %eax; fildll`, the zero-extending form, which is what an
 * `unsigned int` converts with.
 */
static int
ce_echo_rate_down(V90ConnectionEvaluator *ce, unsigned int nofSymbols,
		  float scale)
{
	unsigned int minDur =
	    (unsigned int)ce->minDurationInDataBeforeRrnDown;
	unsigned int dur = (unsigned int)ce->rateDownDetectDuration;
	int demanded = 0;

	ce->word_b8 = scale;

	if (ce->word_70 * scale > ce->threshDown) {
		ce->word_14 += nofSymbols;
		if ((ce->word_14 >= dur / 3 && ce->word_1c >= minDur * 1.3)
		    || (ce->word_14 >= dur / 5 && ce->short_b4 == 1
			&& ce->word_1c >= minDur * 0.6)) {
			ce->short_b4++;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V90-mod3 CHANGE echoRrnState = %d",
				    ce->short_b4);
			ce_echo_rrn_debug(ce);
			ce->word_98 = 0;
			ce->word_90 = ce->params->RRN_SILENCE_REQUESTED;
			ce->params->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE =
			    (ce->short_b4 == 1) ? 0.65f : 1.8f;
			ce->word_14 = 0;
			ce->word_1c = 0;
			demanded = 1;
		}
	} else {
		ce->word_14 = 0;
	}

	return demanded;
}

int
V90ConnectionEvaluator::evaluateConnection()
{
	int verdict = V90CE_VERDICT_NONE;
	unsigned int nofSymbols;
	unsigned int fadeClock;

	/*
	 * The first non-empty call after a retrain latches the distance the
	 * connection settled at; 0x3e6dd, and both retrain arms put it back.
	 */
	if (short_9c == -1)
		short_9c = curDmin;

	nofSymbols = word_74;
	if (nofSymbols == 0)
		return V90CE_VERDICT_NONE;

	word_1c += nofSymbols;

	/* ----------------------------------------------- 1: the fade clock */

	fadeClock = word_20;

	if (nofV90Retrains != 0) {
		int now = (fadeClock + nofSymbols) / retrainCounterFadeCount;
		int was = fadeClock / retrainCounterFadeCount;

		if (now > was)
			nofV90Retrains--;
	}
	if (nofRemoteRetrains != 0) {
		int now = (fadeClock + nofSymbols) / retrainCounterFadeCount;
		int was = fadeClock / retrainCounterFadeCount;

		if (now > was)
			nofRemoteRetrains--;
	}
	if (nofRemoteRateReneg != 0) {
		int now = (fadeClock + nofSymbols) / remoteRrnCounterFadeCount;
		int was = fadeClock / remoteRrnCounterFadeCount;

		if (now > was)
			nofRemoteRateReneg--;
	}

	word_20 = fadeClock + nofSymbols;

	/* ------------------------------------------- 2: the external demand */

	if (word_8c > -1) {
		switch (word_8c) {
		case 1:
		case 2:
		case 4:
		case 5:
			if (word_8c == 1 || word_8c == 4) {
				verdict = V90CE_VERDICT_RRN_NO_RESTRICT;
				edprintf("V90ConnectionEvaluator: Initiating "
					 "No Restriction RRN (external "
					 "demand)\r\n");
			} else {
				verdict = V90CE_VERDICT_RRN_DOWN;
				edprintf("V90ConnectionEvaluator: Initiating "
					 "RRN Down (external demand)\r\n");
			}
			word_90 = (word_8c > 3);
			word_98 = (word_8c == 5);
			word_1c = 0;
			word_14 = 0;
			word_10 = 0;
			word_18 = 0;
			break;

		case 3:
			verdict = V90CE_VERDICT_RRN_UP;
			edprintf("V90ConnectionEvaluator: Initiating RRN up "
				 "(external demand)\r\n");
			word_90 = 0;
			word_1c = 0;
			word_14 = 0;
			word_10 = 0;
			word_18 = 0;
			break;

		case 6:
			edprintf("V90ConnectionEvaluator: Initiating Fast "
				 "Parameters Exchange called - NOT "
				 "IMPLEMENTED\r\n");
			break;

		default:
			/*
			 * The V.42 error corrector asked for a rate down.  The
			 * only arm of the function with its own epilogue.
			 */
			if (enableRrnDown == 0) {
				edprintf("V90ConnectionEvaluator: can't "
					 "responed to V42 RRN down because "
					 "currently at min allowed rate.\r\n");
				break;
			}

			edprintf("V90ConnectionEvaluator: before EC RRN: "
				 "curDmin = %d, initDmin = %d\r\n",
				 curDmin, short_9c);

			if (curDmin >= 2 * short_9c) {
				nofV90Retrains++;
				edprintf("V90ConnectionEvaluator: error "
					 "correction mechanism demanded rate "
					 "down,\r\n");
				edprintf("V90ConnectionEvaluator: initiating "
					 "retrain (retrain no %d), due to %d "
					 "rate renegotiations down.\r\n",
					 nofV90Retrains,
					 params->
					 MAX_NOF_RATES_DIFF_BEFORE_RETRAIN);
				word_18 = 0;
				verdict = V90CE_VERDICT_RETRAIN;
				word_1c = 0;
				short_9c = -1;
				if (nofV90Retrains
				    > (unsigned int)
				      params->MAX_NOF_V90_RETRAINS) {
					edprintf("V90ConnectionEvaluator: "
						 "initiating fall back to V34 "
						 "due to %d V90 retrains\r\n",
						 nofV90Retrains);
					nofV90Retrains = 0;
					verdict = V90CE_VERDICT_FALLBACK_V34;
				}
				word_90 = 0;
			} else {
				edprintf("V90ConnectionEvaluator: initiating "
					 "One Rate Down, due to error "
					 "correction mechanism demand.\r\n");
				word_14 = 0;
				word_10 = 0;
				verdict = V90CE_VERDICT_RRN_DOWN;
				word_1c = 0;
				word_98 = 0;
				word_90 = params->RRN_SILENCE_REQUESTED;
			}

			/* the external-demand copy of the override */
			if (word_94 != 0
			    && verdict == V90CE_VERDICT_RRN_DOWN) {
				edprintf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
					 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
					 "@@@@@@@@@@\r\n");
				edprintf("V90ConnectionEvaluator: initiating "
					 "Retrain instead of One Rate Down "
					 "!!\r\n");
				edprintf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
					 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
					 "@@@@@@@@@@\r\n");
				word_90 = 0;
				verdict = V90CE_VERDICT_RETRAIN;
			}

			word_74 = 0;
			word_70 = 0;
			word_8c = -1;
			return verdict;
		}

		word_8c = -1;
	}

	/* ------------------------------------------------------ 3: rate up */

	if (enableRrnUp != 0 && word_70 < threshUp) {
		word_10 += nofSymbols;
		if (word_10 >= (unsigned int)rateUpDetectDuration
		    && word_1c >= (unsigned int)minDurationInDataBeforeRrnUp) {
			word_10 = 0;
			verdict = V90CE_VERDICT_RRN_UP;
			word_1c = 0;
		}
	} else {
		word_10 = 0;
	}

	/* ---------------------------------------------------- 4: rate down */

	if (params->HIGH_LEVEL_TX_ACTIVE != 0) {
		/*
		 * +0x5c REACHES THE x87 ZERO-EXTENDED: 0x3e98c is `xor
		 * %ebx,%ebx; push %ebx; push %ecx; fildll (%esp)`, the 64-bit
		 * form GCC uses for `unsigned int` and never for `int`, so the
		 * scaled threshold is computed from the UNSIGNED reading of a
		 * slot the map calls `int`.  The cast is here rather than on
		 * the field because `t_v90leaves.cpp` names the field and is
		 * not this batch's file.
		 *
		 * THE SECOND COMPARISON IS PLAIN INTEGER.  0x3ee7d converts
		 * the same x87 value back with `fistpll` and compares that,
		 * which is exact for every 32-bit input -- `fildll` and
		 * `fistpll` round-trip a 32-bit integer through a 64-bit
		 * mantissa without loss -- so `word_1c < minDur` is what it
		 * computes.
		 *
		 * A FIRST VERSION OF THIS COMMENT SAID SPELLING IT AS
		 * `(unsigned)(float)minDur` WOULD ROUND AT 2^24 AND BE WRONG.
		 * IT WOULD NOT, and the mutation set is what found that out:
		 * driven at 16777217, the first integer a `float` cannot
		 * hold, the two spellings agree.  On an x87 target
		 * FLT_EVAL_METHOD is 2, so the cast to `float` is evaluated at
		 * 80 bits and never narrows -- the mutant emits a different
		 * instruction sequence and computes the same thing.  The plain
		 * compare is written because it is what the object's
		 * arithmetic reduces to, not because the other one is unsafe.
		 * Finding 1389.
		 */
		unsigned int minDur =
		    (unsigned int)minDurationInDataBeforeRrnDown;
		float scale = 0.0f;
		int scaled = 1;

		if (word_1c < (unsigned int)(minDur * 2.3f) && short_b4 == 0)
			scale = 1.52f;
		else if (word_1c < minDur && short_b4 == 1)
			scale = 1.39f;
		else {
			scaled = 0;
			if (short_b4 <= 2) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90-mod3  Echo_Rrn_State = %d ",
					    short_b4);
				ce_echo_rrn_debug(this);
				word_14 = 0;
				short_b4 = 3;
				params->
				    RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE
				    = 2.0f;
			}
		}

		if (scaled) {
			if (ce_echo_rate_down(this, nofSymbols, scale))
				verdict = V90CE_VERDICT_RRN_DOWN;
		} else if (ce_plain_rate_down(this, nofSymbols)) {
			verdict = V90CE_VERDICT_RRN_DOWN;
		}
	} else if (enableRrnDown != 0) {
		if (ce_plain_rate_down(this, nofSymbols))
			verdict = V90CE_VERDICT_RRN_DOWN;
	} else {
		word_14 = 0;
	}

	/* ------------------------------------------------------ 4b: retrain */

	if (word_70 > threshRetrain) {
		word_18 += nofSymbols;
		if (word_18 >= (unsigned int)retrainDetectDuration) {
			nofV90Retrains++;
			if (nofV90Retrains
			    > (unsigned int)
			      params->MAX_NOF_V90_RETRAINS) {
				verdict = V90CE_VERDICT_FALLBACK_V34;
				edprintf("V90ConnectionEvaluator: initiating "
					 "fall back to V34 due to %d V90 "
					 "retrains, avePdsnr = %c%d.%02d\r\n",
					 nofV90Retrains,
					 (0.0f < word_70) ? '+' : '-',
					 (int)__builtin_fabsf(word_70),
					 ce_frac2(word_70));
				nofV90Retrains = 0;
			} else {
				verdict = V90CE_VERDICT_RETRAIN;
				edprintf("V90ConnectionEvaluator: initiating "
					 "Retrain (retrain no %d) due to "
					 "avePdsnr = %c%d.%02d\r\n",
					 nofV90Retrains,
					 (0.0f < word_70) ? '+' : '-',
					 (int)__builtin_fabsf(word_70),
					 ce_frac2(word_70));
			}
			word_1c = 0;
			word_18 = 0;
		}
	} else {
		word_18 = 0;
	}

	/* ------------------------------ 4c: too many remote renegotiations */

	if (nofRemoteRateReneg
	    >= (unsigned int)nofRemoteRateRenegBeforeRetrain) {
		nofV90Retrains++;
		verdict = V90CE_VERDICT_RETRAIN;
		edprintf("V90ConnectionEvaluator: initiating Retrain (retrain "
			 "no %d) due to %d remote rate reneg\r\n",
			 nofV90Retrains, nofRemoteRateReneg);
		nofRemoteRateReneg = 0;
		word_1c = 0;
		word_18 = 0;
	}

	/* ----------------------------- 4d: what the verdict so far implies */

	switch (verdict) {
	case V90CE_VERDICT_RRN_UP:
		edprintf("V90ConnectionEvaluator: initiating One Rate Up "
			 "demand, avePdsnr = %c%d.%02d\r\n",
			 (0.0f < word_70) ? '+' : '-',
			 (int)__builtin_fabsf(word_70), ce_frac2(word_70));
		word_90 = 0;
		break;

	case V90CE_VERDICT_RRN_DOWN:
		edprintf("V90ConnectionEvaluator: on Rate Down: curDmin = %d, "
			 "initDmin = %d\r\n", curDmin, short_9c);
		if (curDmin >= 2 * short_9c) {
			nofV90Retrains++;
			edprintf("V90ConnectionEvaluator: initiating retrain, "
				 "due to %d rate renegotiations down, "
				 "avePdsnr = %c%d.%02d\r\n",
				 params->MAX_NOF_RATES_DIFF_BEFORE_RETRAIN,
				 (0.0f < word_70) ? '+' : '-',
				 (int)__builtin_fabsf(word_70),
				 ce_frac2(word_70));
			edprintf("V90ConnectionEvaluator: retrain no %d\r\n",
				 nofV90Retrains);
			word_18 = 0;
			short_9c = -1;
			verdict = V90CE_VERDICT_RETRAIN;
			if (nofV90Retrains
			    > (unsigned int)
			      params->MAX_NOF_V90_RETRAINS) {
				edprintf("V90ConnectionEvaluator: initiating "
					 "fall back to V34 due to %d V90 "
					 "retrains\r\n", nofV90Retrains);
				verdict = V90CE_VERDICT_FALLBACK_V34;
				nofV90Retrains = 0;
			}
			word_90 = 0;
		} else {
			edprintf("V90ConnectionEvaluator: initiating One Rate "
				 "Down demand, avePdsnr = %c%d.%02d\r\n",
				 (0.0f < word_70) ? '+' : '-',
				 (int)__builtin_fabsf(word_70),
				 ce_frac2(word_70));
		}
		break;

	case V90CE_VERDICT_RETRAIN:
		short_9c = -1;
		word_90 = 0;
		break;
	}

	/* the rate-down override, the second of its two copies */
	if (word_94 != 0 && verdict == V90CE_VERDICT_RRN_DOWN) {
		edprintf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
			 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n");
		edprintf("V90ConnectionEvaluator: initiating Retrain instead "
			 "of One Rate Down !!\r\n");
		edprintf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
			 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n");
		word_90 = 0;
		verdict = V90CE_VERDICT_RETRAIN;
	}

	/* ------------------------------------------------ 5: the debug arms */

	if (debugAlternateDebug != 0) {
		word_24 += nofSymbols;
		if (word_24 >= debugPeriod) {
			switch (word_80) {
			case 0:
			case 2:
				verdict = V90CE_VERDICT_RETRAIN;
				edprintf("V90ConnectionEvaluator: Alternate "
					 "Debug Retrain\r\n");
				word_80++;
				word_24 = 0;
				word_90 = 0;
				break;
			case 1:
				verdict = V90CE_VERDICT_RRN_UP;
				edprintf("V90ConnectionEvaluator: Alternate "
					 "Debug RRN up\r\n");
				word_80++;
				word_24 = 0;
				word_90 = 0;
				break;
			case 3:
				edprintf("V90ConnectionEvaluator: Alternate "
					 "Debug RRN down\r\n");
				word_24 = 0;
				verdict = V90CE_VERDICT_RRN_DOWN;
				word_80 = 0;
				word_98 = 0;
				word_90 = params->RRN_SILENCE_REQUESTED;
				break;
			}
		}
	} else if (debugFallBack != 0) {
		word_24 += nofSymbols;
		if (word_24 >= debugPeriod) {
			verdict = V90CE_VERDICT_FALLBACK_V34;
			edprintf("V90ConnectionEvaluator: Debug fall back to "
				 "V34\r\n");
			word_24 = 0;
			word_90 = 0;
		}
	} else if (debugRetrain != 0) {
		word_24 += nofSymbols;
		if (word_24 >= debugPeriod) {
			verdict = V90CE_VERDICT_RETRAIN;
			edprintf("V90ConnectionEvaluator: Debug initiating "
				 "Retrain\r\n");
			word_24 = 0;
			word_90 = 0;
		}
	} else if (debugRateUp != 0) {
		word_24 += nofSymbols;
		if (word_24 >= debugPeriod) {
			verdict = V90CE_VERDICT_RRN_UP;
			edprintf("V90ConnectionEvaluator: Debug One Rate "
				 "Up\r\n");
			word_24 = 0;
			word_90 = 0;
		}
	} else if (debugRateDown != 0) {
		word_24 += nofSymbols;
		if (word_24 >= debugPeriod) {
			verdict = V90CE_VERDICT_RRN_DOWN;
			edprintf("V90ConnectionEvaluator: Debug One Rate "
				 "Down\r\n");
			word_24 = 0;
			word_98 = 0;
			word_90 = params->RRN_SILENCE_REQUESTED;
		}
	}

	word_74 = 0;
	word_70 = 0;
	return verdict;
}
