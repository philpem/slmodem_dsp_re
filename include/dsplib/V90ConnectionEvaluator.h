/**
 * @file V90ConnectionEvaluator.h
 * @brief `V90ConnectionEvaluator`, the V.90 connection-quality evaluator
 *        that decides between staying put, renegotiating rate, retraining,
 *        or falling back to V.34.
 *
 * Reconstructed from dsplibs.o. The class did not arrive with this file: it
 * was defined inside `V90Demodulator.h` from task #60 onwards, because the
 * only things in the tree that touched it were `V90Demodulator::enterPhase3`
 * (four words it clears) and `VPcmFloModem::getV90CpBits` (a copy from +0x78
 * to +0x7c). Task #88 wrote the three members the class actually owns, so it
 * has its own header now. No field moved and no field was renamed --
 * `word_70`, `word_74`, `word_78`, `word_7c`, `word_84` and `word_88` are the
 * names those two earlier batches gave them, and the offset assertions moved
 * from `V90Demodulator.cpp` to `V90ConnectionEvaluator.cpp` unchanged.
 *
 * Not polymorphic: `nm` gives `D1` and `D2` and no `D0`, and GCC emits a
 * deleting destructor only for a virtual class, so offset 0 is a real
 * member and there is no vptr (finding F228).
 *
 * The object is 0xbc = 188 bytes, sized by its one allocation site
 * (`V90Demodulator`'s constructor, `sysdep_malloc(0xbc)` passed straight to
 * this constructor). `reset`'s highest write is the two-byte store at
 * +0xb4, so six bytes at the end are untouched by anything written here.
 *
 * `reset` makes forty-eight stores. Nineteen are copies out of the
 * parameter block, and every one of those nineteen slots has the original
 * author's own name (findings F860-862, `tools/vparse.py`) -- the field
 * names below are taken from the parameter each field receives. The other
 * twenty-nine are zeroes, two -1s and two 1600s, and those are
 * offset-named because a zero says nothing about what a field holds.
 *
 * The constructor is a fifteen-byte tail call into `reset`, after storing
 * the parameter block pointer: "store the parameter block, then reset".
 * The destructor is a one-byte `ret`.
 */

#ifndef DSPLIB_V90CONNECTIONEVALUATOR_H
#define DSPLIB_V90CONNECTIONEVALUATOR_H

/*
 * A pointer only, so a forward declaration is what belongs here; the .cpp
 * includes `V90Parameters.h` directly, because the names are the point.
 * This used to be load-bearing rather than tidy -- `V90PreFilter.h` (which
 * `V90Demodulator.h` pulls in, and this header is included by that same
 * translation unit) once carried a second, incompatible `V90Parameters`
 * definition, so no translation unit could include both (finding F1112).
 * That duplication was retired at task #116 (finding F6402); the forward
 * declaration is kept because this header only ever needs a pointer.
 */
class V90Parameters;

/**
 * @brief The connection evaluator's verdict codes.
 *
 * `indicateLocalRetrain`/`indicateRemoteRetrain` return 4 ("retrain") or 5
 * ("fall back to V.34"), named by the diagnostic string on each path;
 * `evaluatePhase3`/`evaluatePhase4` add 0 ("nothing crossed a threshold") as
 * a real, register-cleared answer rather than a `void` artefact (finding
 * F1388). `evaluateConnection` completes the set: across its two epilogues,
 * exactly {0, 1, 2, 3, 4, 5} reach the return register, with 1, 2 and 3
 * named from the diagnostics on their paths -- RRN up, RRN down, and
 * unrestricted RRN respectively (finding F1389). `V90CE_VERDICT_NONE` is our
 * own name for 0; the object names no enumerator anywhere.
 */
#define V90CE_VERDICT_NONE		0
#define V90CE_VERDICT_RRN_UP		1
#define V90CE_VERDICT_RRN_DOWN		2
#define V90CE_VERDICT_RRN_NO_RESTRICT	3
#define V90CE_VERDICT_RETRAIN		4
#define V90CE_VERDICT_FALLBACK_V34	5

class V90ConnectionEvaluator {
public:
	/**
	 * @brief Construct the evaluator: store the parameter block and reset.
	 * @param params  The V.90 parameter block this evaluator reads from.
	 */
	V90ConnectionEvaluator(V90Parameters *params);

	/** @brief Trivial destructor (a one-byte `ret` in the object). */
	~V90ConnectionEvaluator();

	/** @brief Reset all evaluator state and reload the configuration fields from `params`. */
	void reset();

	/**
	 * @brief Fold one PDSNR measurement into the running average at +0x70.
	 * @param pdsnr       The PDSNR measurement to fold in.
	 * @param nofSymbols  How many symbols this measurement covers.
	 */
	void updateAvePdsnr(float pdsnr, unsigned int nofSymbols);

	/**
	 * @brief Record the current constellation distance and the three verdict thresholds.
	 * @param curDmin       The current minimum constellation distance.
	 * @param threshUp      The rate-up decision threshold.
	 * @param threshDown    The rate-down decision threshold.
	 * @param threshRetrain The retrain decision threshold.
	 */
	void updateCurrentConstellationData(short curDmin, float threshUp,
					    float threshDown,
					    float threshRetrain);

	/** @brief Count one remote rate renegotiation and print "(rrn no %d)". */
	void indicateRemoteRateReneg();

	/**
	 * @brief Phase-4 mean-error-standard-deviation stub.
	 *
	 * A three-byte body (`xor %eax,%eax; ret`) that reads neither
	 * argument and always answers 0 -- a real asymmetry against its
	 * phase-3 counterpart, evaluateMeanErrorStdPhase3(), which is 234
	 * bytes and actually computes something.
	 *
	 * @return Always 0.
	 */
	int evaluateMeanErrorStdPhase4(float, float);

	/**
	 * @brief Count one local V.90 retrain and check it against the retrain limit.
	 * @return #V90CE_VERDICT_RETRAIN or #V90CE_VERDICT_FALLBACK_V34.
	 */
	int indicateLocalRetrain();

	/**
	 * @brief Count one remote retrain and check it against the retrain limit.
	 * @return #V90CE_VERDICT_RETRAIN or #V90CE_VERDICT_FALLBACK_V34.
	 */
	int indicateRemoteRetrain();

	/**
	 * @brief Evaluate the running PDSNR average against the phase-3 fall-back and retrain thresholds.
	 * @return #V90CE_VERDICT_NONE, #V90CE_VERDICT_RETRAIN or #V90CE_VERDICT_FALLBACK_V34.
	 */
	int evaluatePhase3();

	/**
	 * @brief Evaluate the running PDSNR average against the phase-4 fall-back and retrain thresholds.
	 * @param meanErrBefToAftUpdateRatio  Named after the two diagnostic strings that print it and nothing else.
	 * @return #V90CE_VERDICT_NONE, #V90CE_VERDICT_RETRAIN or #V90CE_VERDICT_FALLBACK_V34.
	 */
	int evaluatePhase4(float meanErrBefToAftUpdateRatio);

	/**
	 * @brief The main per-call connection-quality decision, 3,857 bytes at 0x3e6d0.
	 *
	 * Weighs the constellation distance, the RRN and retrain counters,
	 * the echo-RRN state machine and the minimum-dwell timers against the
	 * configured thresholds. See the verdict block above for the six
	 * possible return values and where each comes from.
	 *
	 * @return One of the six #V90CE_VERDICT_* values.
	 */
	int evaluateConnection();

	/**
	 * @brief Phase-3 mean-error-standard-deviation evaluator, 234 bytes at 0x3f9d0.
	 * @param std  The measured error standard deviation.
	 * @return #V90CE_VERDICT_NONE or #V90CE_VERDICT_FALLBACK_V34.
	 */
	int evaluateMeanErrorStdPhase3(float std);

	/** @brief Print the evaluator's current state, 177 bytes at 0x40140, the class's last symbol in the object. */
	void printStatus() const;

	/*
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable from the mangling, and a
	 * single access section is what lets the .cpp assert every offset
	 * below with __builtin_offsetof.
	 */

	/*
	 * +0x00  The parameter block, stored by the constructor and read by
	 * `reset`.  Not owned.
	 */
	V90Parameters *params;

	/*
	 * +0x04 .. +0x24  Nine consecutive counters `reset` zeroes. Three of
	 * them have the original author's own names, from the strings the
	 * three `indicate*` members print and the parameters they compare
	 * against (finding F1381); the other six are still offset-named
	 * because a zero says nothing about what a field holds. All three
	 * named ones are `unsigned`, forced by the branch on the comparison
	 * against their ceiling (finding F1381).
	 */

	/*
	 * +0x04  Local V.90 retrains. `indicateLocalRetrain` increments it,
	 * compares it against `params->MAX_NOF_V90_RETRAINS` and prints it as
	 * "%d V90 retrains" when it is over.
	 */
	unsigned int nofV90Retrains;

	/*
	 * +0x08  Remote rate renegotiations. `indicateRemoteRateReneg`
	 * increments it and prints it as "(rrn no %d)". Nothing compares it
	 * against `params->NOF_REMOTE_RATE_RENEG_BEFORE_RETRAIN` in anything
	 * reconstructed so far, so the ceiling is somewhere unread.
	 */
	unsigned int nofRemoteRateReneg;

	/*
	 * +0x0c  Remote retrains.  `indicateRemoteRetrain` increments it,
	 * compares it against `params->MAX_NOF_REMOTE_RETRAINS` and prints it
	 * as "%d remote retrains".
	 */
	unsigned int nofRemoteRetrains;

	/*
	 * +0x10  A duration in symbols, and `evaluatePhase3` and
	 * `evaluatePhase4` are what say so: each adds `word_74` -- the count
	 * of symbols the running average covers -- to it whenever the average
	 * is over its fall-back threshold, clears it outright whenever the
	 * average is not, and gives up on V.90 when it reaches the 1600 at
	 * +0x64 (phase 3) or +0x68 (phase 4). So it measures how long the
	 * error has been too large, in symbols, and the two 1600s are the two
	 * patiences. Unsigned: `cmp 0x64(%ebx),%eax; jb` at 0x3f6a0.
	 *
	 * It keeps its offset name: "how long the error has been large" is a
	 * reading of what the arithmetic does, not the author's word for it,
	 * and finding F226's rule is that a reading does not earn a name.
	 */
	unsigned int word_10;
	unsigned int word_14;
	/*
	 * +0x18 and +0x1c  Zeroed by both `indicateLocalRetrain` and
	 * `indicateRemoteRetrain`, on both of their paths, and by nothing
	 * else the lifecycle batch read.
	 *
	 * +0x18 is the second duration, and it is +0x10's twin: both
	 * evaluators add `word_74` to it while the average is over
	 * `PDSNR_THRESHOLD_IN_PHASE3` or `..._IN_PHASE4`, clear it when the
	 * average is not, and ask for a retrain when it reaches the +0x60 copy
	 * of `RETRAIN_DETECT_DURATION`. The comparison is unsigned against a
	 * slot the parameter map calls `int` (`cmp 0x60(%ebx),%eax; jb` at
	 * 0x3f797), which is what fixes the field's own signedness.
	 *
	 * It is not cleared when it merely accumulates: 0x3f797 and 0x3fd12
	 * both jump past the store that zeroes it, so the clear happens on
	 * exactly two paths -- the average fell below the threshold, or the
	 * duration ran out and a verdict was printed. A version that cleared
	 * it every call could never accumulate past one call and would still
	 * agree with the blob on any single-call test.
	 *
	 * What +0x1c counts is symbols since the last decision, and
	 * `evaluateConnection` is what says so: `add %ebx,0x1c(%edi)` at
	 * 0x3e6f8 adds the symbol count of every non-empty call, and every arm
	 * that decides anything clears it. It is a minimum dwell rather than a
	 * patience: three tests read it -- against
	 * `minDurationInDataBeforeRrnUp` (0x3e949), against
	 * `minDurationInDataBeforeRrnDown` (0x3ebec) and against a 2.3x, 1.3x
	 * and 0.6x of the same slot on the echo-RRN path -- and all three
	 * block a rate change that has otherwise been earned. It keeps its
	 * offset name; "how long since the last decision" is a reading of the
	 * arithmetic rather than a derivation.
	 *
	 * +0x20 is the fade clock, read only at 0x3e706..0x3e754: the three
	 * retrain/reneg counters each lose one whenever `word_20 / fadeCount`
	 * crosses an integer, and +0x20 then advances by the symbol count.
	 *
	 * +0x24 is `int` and not `unsigned`, the one place in these eight
	 * where the two readings part. All five debug arms of
	 * `evaluateConnection` do `word_24 += word_74; cmp 0x6c(%edi),%eax;
	 * jl` (0x3ec92, 0x3ed39, 0x3ef02, 0x3efda, 0x3f3ba) -- a signed branch
	 * against `debugPeriod`, which the map calls `int`. An `unsigned int`
	 * +0x24 would have made the sum unsigned and the branch `jb`; the
	 * signed compare needs the sum stored back into an `int` first, which
	 * is exactly the `+=` the object emits before the compare.
	 */
	unsigned int word_18;
	unsigned int word_1c;
	unsigned int word_20;
	int word_24;

	/*
	 * +0x28 .. +0x6c  The configuration, copied out of the parameter
	 * block one word at a time.  The names are the parameters' -- see the
	 * file comment -- and the two that are not copied are the pair of
	 * 1600s at +0x64 and +0x68, which are an immediate `movl $0x640` each
	 * and carry no relocation, so they are integers and not addresses.
	 */
	int enableRrnDown;			/* +0x28 ENABLE_RRN_DOWN     */
	int enableRrnUp;			/* +0x2c ENABLE_RRN_UP       */
	int nofRemoteRateRenegBeforeRetrain;	/* +0x30                     */
	int debugAlternateDebug;		/* +0x34                     */
	int debugFallBack;			/* +0x38                     */
	int debugRetrain;			/* +0x3c                     */
	int debugRateUp;			/* +0x40                     */
	int debugRateDown;			/* +0x44                     */
	int retrainCounterFadeCount;		/* +0x48                     */
	int remoteRrnCounterFadeCount;		/* +0x4c                     */
	int rateUpDetectDuration;		/* +0x50                     */
	int minDurationInDataBeforeRrnUp;	/* +0x54                     */
	int rateDownDetectDuration;		/* +0x58                     */
	int minDurationInDataBeforeRrnDown;	/* +0x5c                     */
	int retrainDetectDuration;		/* +0x60                     */
	/*
	 * +0x64 and +0x68  Two different limits, which only the disassembly
	 * separates because `reset` gives them the same starting value:
	 * `evaluatePhase3` compares +0x10 against +0x64 (0x3f69a, 0x3f879) and
	 * `evaluatePhase4` compares it against +0x68 (0x3fb13).  A
	 * reconstruction that read the wrong one of the pair would pass every
	 * test that left them at 1600.
	 */
	unsigned int word_64;			/* +0x64 reset plants 1600   */
	unsigned int word_68;			/* +0x68 reset plants 1600   */
	int debugPeriod;			/* +0x6c                     */

	/*
	 * +0x70 and +0x74  The running average PDSNR and its weight, which is
	 * what `updateAvePdsnr` does to them and the reason the pair is
	 * cleared together by `enterPhase3`:
	 *
	 *     +0x74 == 0 ->  +0x70 = pdsnr;  +0x74 = nofSymbols
	 *     otherwise  ->  +0x70 = (+0x74 * +0x70 + pdsnr * nofSymbols)
	 *                            / (+0x74 + nofSymbols);
	 *                    +0x74 += nofSymbols
	 *
	 * +0x70 is a float -- `fmuls 0x70(%ebx)` and `fstps 0x70(%ebx)`, a
	 * four-byte load and a four-byte store, never an integer move -- and
	 * +0x74 is an unsigned count, because both conversions of it are
	 * `push $0; push %reg; fildll`, the zero-extending 64-bit form GCC
	 * uses for `unsigned int` and never for `int`.
	 *
	 * The names are not changed here, deliberately: `V90Demodulator.cpp`
	 * and `VPcmFloModem.cpp` refer to these four by their offset names
	 * and belong to other work, so renaming them would edit files this
	 * batch does not own. The derivation is recorded instead, and the
	 * type of +0x70 is corrected, which those files do not notice --
	 * both assign it a literal 0, and `0` into a float is the same four
	 * zero bytes `0` into an `unsigned int` was.
	 */
	float word_70;			/* +0x70 the average PDSNR           */
	unsigned int word_74;		/* +0x74 the symbols it averages     */
	/*
	 * +0x78 and +0x7c were `pad_78` until the VPcmFloModem batch read a
	 * second function that touches this object: `getV90CpBits` copies
	 * +0x78 to +0x7c each time a CP sequence finishes.  Two batches, two
	 * functions, one object -- neither would have found both.
	 *
	 * A third function says what the pair is for: `evaluatePhase4`'s last
	 * arm runs only when both are non-zero, prints "Initiating retrain
	 * (delayed)...", clears both, and then counts a retrain exactly as
	 * the other arms do -- so this is a retrain that was asked for
	 * earlier and is honoured here, and it is the only thing in the class
	 * that reads either slot. The pair is a request and its
	 * acknowledgement, which is why `getV90CpBits` copying one to the
	 * other arms it.
	 *
	 * A fourth function supplies the author's own word for +0x78:
	 * `V90Demodulator::exitPhase3` raises the request -- `movl $0x1,
	 * 0x78(%esi)` at 0x1bd54 -- when `V90TRN2Design` cannot design the
	 * TRN2 constellations, beside a `dsplibs_debug_printf` reading
	 * "V90Demodulator::exitPhase3() delayedRetrainRequest !!!", which is
	 * CLAUDE.md's evidence rule 1 and names the slot exactly: the request
	 * half of the pair is `delayedRetrainRequest`. It agrees with
	 * `evaluatePhase4`'s "Initiating retrain (delayed)..." reached from
	 * the same slot.
	 *
	 * The names are still not changed, and the reason here is
	 * coordination rather than evidence: `word_78` is spelled 45 times
	 * across nine files, `V90Equalizer` has a different member of the
	 * same name at its own +0x78, and `VPcmFloModem.cpp` and two
	 * mutation sets refer to this one by its offset name and belong to
	 * other work. A rename here would be a nine-file edit whose only
	 * checkable part is that nothing broke. The evidence is recorded so
	 * that the pass which owns those files can make it in one move.
	 * Finding F7485.
	 */
	unsigned int word_78;		/* +0x78 copied to word_7c           */
	unsigned int word_7c;		/* +0x7c                             */
	unsigned int word_80;		/* +0x80 zeroed by reset             */
	unsigned int word_84;		/* +0x84 cleared by enterPhase3      */
	unsigned int word_88;		/* +0x88 cleared by enterPhase3;
					 *       reset does NOT touch it     */

	/*
	 * +0x8c  Set to -1 by `reset`, not to 0 -- `mov $0xffffffff,%eax`, a
	 * full 32-bit store -- which is why it is `int` and not `unsigned`.
	 */
	int word_8c;

	/*
	 * +0x90  Zeroed by `reset` and by all four of the members that tell
	 * the evaluator something changed -- `indicateLocalRetrain`,
	 * `indicateRemoteRetrain`, `indicateRemoteRateReneg` and
	 * `updateCurrentConstellationData`. Four functions, one store each,
	 * and it is the first thing three of them do. A "how long since the
	 * picture last moved" counter is the obvious reading and it is a
	 * reading, not a derivation, so the field keeps its offset name.
	 */
	unsigned int word_90;		/* +0x90 zeroed by reset and by four */
	unsigned int word_94;		/* +0x94 zeroed by reset             */
	/*
	 * +0x98  Was `pad_98[4]`, and `evaluateConnection` is its only
	 * writer: five 32-bit stores, four of them a plain zero (0x3e869,
	 * 0x3eac0, 0x3ec02, 0x3ece1) and one at 0x3ee4f the value of
	 * `word_8c == 5`:
	 *
	 *     3ee43:  83 fa 05        cmp    $0x5,%edx
	 *     3ee4c:  0f 94 c0        sete   %al
	 *     3ee4f:  89 87 98 00 00  mov    %eax,0x98(%edi)
	 *
	 * Every store is beside a store to +0x90, and the four zeroes are on
	 * exactly the paths that also set +0x90 -- so the two travel together
	 * and +0x98 is a second flag of whatever +0x90 is the first of.
	 * Nothing reconstructed so far reads it, which is why it keeps an
	 * offset name: a store with no reader says how wide the slot is and
	 * nothing about what it means. Four bytes, `movl`, so `unsigned int`
	 * -- and the width is what retires the pad.
	 */
	unsigned int word_98;

	/*
	 * +0x9c, +0x9e  A 16-bit pair, -1 and 0.  The store widths are the
	 * evidence: `mov %cx,0x9c(%ebx)` with %ecx holding 0xffffffff writes
	 * two bytes and leaves 0xffff, which is a different value from the
	 * -1 at +0x8c even though the source constant is the same.
	 *
	 * The author's own name for +0x9c is `initDmin`: two diagnostics in
	 * `evaluateConnection` print the pair together and name both halves
	 * ("before EC RRN: curDmin = %d, initDmin = %d" / "on Rate Down:
	 * curDmin = %d, initDmin = %d"). It is the initial minimum distance:
	 * the entry test copies +0x9e into it the first time it is -1, and
	 * both arms that ask for a retrain put it back to -1 so the next call
	 * re-latches. What the pair decides is `curDmin >= 2 * initDmin` --
	 * the distance having doubled since the connection settled is what
	 * turns a rate-down demand into a retrain. Finding F1388.
	 *
	 * The field keeps its offset name: `test/unit/t_v90leaves.cpp` refers
	 * to it as `short_9c` and belongs to other work, so renaming here
	 * would edit a file this batch does not own -- the same reason +0xac
	 * and +0xb2 kept theirs.
	 */
	short short_9c;

	/**
	 * @brief The current minimum distance, named from the object's own
	 *        diagnostic ("curDmin = %d"), stored here by
	 *        updateCurrentConstellationData().
	 */
	short curDmin;

	/*
	 * +0xa0 .. +0xa8  The three thresholds, in the order the same string
	 * names them ("V90ConnectionEvaluator UPDATE: curDmin = %d,
	 * 10*threshUp = %d, 10*threshDown = %d, 10*threshRetrain = %d") --
	 * the three `fsts` at 0x3e61c, 0x3e624 and 0x3e62c fix the order.
	 * Floats: every access is a four-byte x87 store, and the printed
	 * form needs a multiply by 10.0f to make an integer of it.
	 */
	float threshUp;			/* +0xa0                             */
	float threshDown;		/* +0xa4                             */
	float threshRetrain;		/* +0xa8                             */

	/*
	 * +0xac  Initialised from `params->PHASE4_ERROR_FOR_V34_FALLBACK`, but
	 * this is the current threshold rather than a fixed copy: the first
	 * time `evaluatePhase4` asks for a retrain it replaces the value here
	 * with `params->unnamed_434` (a float despite `V90Parameters.h`
	 * calling the slot `int`; read through a union rather than retyping a
	 * header this file does not own -- 250.0f by default, finding F878).
	 *
	 * The author's own name is `pdsnrCurrentV34DropThreshPhase4`, from the
	 * diagnostic beside the store that sets it in `evaluatePhase4`
	 * (finding F1389). The field keeps its offset name here because
	 * `test/unit/t_v90leaves.cpp` refers to it that way and belongs to
	 * other work; renaming would edit a file this batch does not own.
	 */
	float phase4ErrorForV34Fallback;

	/*
	 * +0xb0 .. +0xb4  Three 16-bit flags, 1, 0 and 0.  +0xb4 is the
	 * highest byte anything written here touches; the object runs to
	 * 0xbc.
	 *
	 * +0xb0 is the one-shot that arms `evaluatePhase4`'s mean-error arm.
	 * `reset` sets it to 1 and nothing else in the class sets it again;
	 * `evaluatePhase4` tests it first of three guards and clears it when
	 * all three pass, so the arm fires at most once per reset. Sixteen
	 * bits: `cmpw $0x0,0xb0(%ebx)` and `mov %dx,0xb0(%ebx)`.
	 *
	 * +0xb2 is `altRbsDetectedOnQc`, the object's own word for it:
	 * `evaluatePhase3`'s first arm runs when +0xb2 is non-zero, clears
	 * it, and prints "altRbsDetectedOnQc => initiating Retrain" -- the
	 * flag is the detection and the arm is what services it. It keeps
	 * its offset name for the same reason +0xac does: `t_v90leaves.cpp`
	 * uses `short_b2` and is not this batch's file. Nothing reconstructed
	 * so far sets it, so whatever detects alternate RBS on the QC path is
	 * somewhere still unread.
	 *
	 * +0xb4 is `echoRrnState`, also the object's own word: `evaluateConnection`
	 * increments it and then prints exactly the value it stored
	 * ("V90-mod3 CHANGE echoRrnState = %d"; a second string prints the
	 * same slot as "Echo_Rrn_State = %d", and `reset`'s unconditional
	 * "*** Echo Rrn Mechanism ***" banner is a third mention). It is a
	 * four-state machine: 0 and 1 pick the 1.52 and 1.39 multipliers the
	 * rate-down threshold is scaled by, the increment advances it every
	 * time a rate-down is demanded, and 3 is the terminal state in which
	 * the multiplier is dropped altogether. Signed 16-bit. It keeps its
	 * offset name, for the third time and the same reason:
	 * `t_v90leaves.cpp` says `short_b4`. Finding F1388.
	 */
	short short_b0;
	short short_b2;
	short short_b4;

	unsigned char pad_b6[2];	/* +0xb6 .. +0xb7                    */

	/*
	 * +0xb8  Was inside `pad_b6[6]`, and it is a float:
	 * `evaluateConnection` stores the rate-down multiplier there with a
	 * four-byte x87 store the instant it picks one --
	 *
	 *     3e9dd:  d9 05 ..        flds   .rodata.cst4+0x2e4   1.52f
	 *     3e9e8:  d9 97 b8 ..     fsts   0xb8(%edi)
	 *
	 * -- and reads it back with `fmuls 0xb8(%edi)` (0x3f041, 0x3f4a3) to
	 * print `avePdsnr * that` as "error -- =" beside the raw `threshDown`
	 * as "threshold -- =". Never an integer move in either direction, so
	 * `float`; and the two accesses are the whole of what is known, so it
	 * keeps an offset name.
	 *
	 * Nothing initialises it: `reset` stops at +0xb4, and the diagnostic
	 * that reads it back is on the path where no multiplier was chosen
	 * this call, so it prints whatever the slot last held -- which is why
	 * the test plants a value in it rather than letting the seed decide.
	 *
	 * +0xb8..+0xbb is the last four bytes of the 0xbc object, so the
	 * class now has no unmodelled region at all.
	 */
	float word_b8;
};

#endif /* DSPLIB_V90CONNECTIONEVALUATOR_H */
