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
 * has its own header now. No field moved when it did -- `word_70`,
 * `word_74`, `word_78`, `word_7c`, `word_84` and `word_88` were the names
 * those two earlier batches gave them, and the offset assertions moved from
 * `V90Demodulator.cpp` to `V90ConnectionEvaluator.cpp` unchanged. ALL SIX are
 * renamed now: `word_78` and `word_7c` are `delayedRetrainRequest`
 * and `delayedRetrainArmed` below, in the naming pass that also settled
 * `initDmin`, `altRbsDetectedOnQc` and `echoRrnState` (finding F9480);
 * `word_70` and `word_74` are `avePdsnr` and `avePdsnrNofSymbols` below,
 * carried through in the second-pass batch that also settled
 * `externalDemandCode` and `meanErrorCheckArmed` (finding F10134); `word_84`
 * and `word_88` are `phase3EvalEnabled` and `trn1dEvalEnabled` below, wave 6
 * (finding F10174), which also renamed the neighbouring `word_80` and
 * `word_94` even though neither was part of this file's original pair.
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
 * twenty-nine are zeroes, two -1s and two 1600s; their names are recovered
 * from diagnostics and consumers, not from the initial values alone.
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
 * A three-byte body (`xor %eax,%eax; ret`) that ignores both arguments
 * and always answers 0 -- a real asymmetry against its
	 * phase-3 counterpart, evaluateMeanErrorStdPhase3(), which is 234
	 * bytes and actually computes something.
	 *
	 * @return Always 0.
	 */
int evaluateMeanErrorStdPhase4(float unused0, float unused1);

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
	 * against (finding F1381); the others are named from status diagnostics
	 * and the duration comparisons described below. All three
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
	 * `evaluatePhase4` are what say so: each adds `avePdsnrNofSymbols` -- the count
	 * of symbols the running average covers -- to it whenever the average
	 * is over its fall-back threshold, clears it outright whenever the
	 * average is not, and gives up on V.90 when it reaches the 1600 at
	 * +0x64 (phase 3) or +0x68 (phase 4). So it measures how long the
	 * error has been too large, in symbols, and the two 1600s are the two
	 * patiences. Unsigned: `cmp 0x64(%ebx),%eax; jb` at 0x3f6a0.
	 *
	 * `printStatus` names this slot "rateUpCounter". The phase evaluators
	 * reuse the same counter for the fall-back duration described above.
	 */
	/** Symbol counter printed as "rateUpCounter"; also times phase fallback. */
	unsigned int rateUpCounter;
	/** Symbol counter printed as "rateDownCounter". */
	unsigned int rateDownCounter;
	/*
	 * +0x18 and +0x1c  Zeroed by both `indicateLocalRetrain` and
	 * `indicateRemoteRetrain`, on both of their paths, and by nothing
	 * else the lifecycle batch read.
	 *
	 * +0x18 is the second duration, and it is +0x10's twin: both
	 * evaluators add `avePdsnrNofSymbols` to it while the average is over
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
	 * block a rate change that has otherwise been earned. The name
	 * `dataDurationCounter` is inferred from these dwell-time comparisons.
	 *
	 * +0x20 is the fade clock, read only at 0x3e706..0x3e754: the three
	 * retrain/reneg counters each lose one whenever `fadeCounter / fadeCount`
	 * crosses an integer, and +0x20 then advances by the symbol count.
	 *
	 * +0x24 is `int` and not `unsigned`, the one place in these eight
	 * where the two readings part. All five debug arms of
	 * `evaluateConnection` do `debugPeriodCounter += avePdsnrNofSymbols; cmp 0x6c(%edi),%eax;
	 * jl` (0x3ec92, 0x3ed39, 0x3ef02, 0x3efda, 0x3f3ba) -- a signed branch
	 * against `debugPeriod`, which the map calls `int`. An `unsigned int`
	 * +0x24 would have made the sum unsigned and the branch `jb`; the
	 * signed compare needs the sum stored back into an `int` first, which
	 * is exactly the `+=` the object emits before the compare.
	 */
	/** Symbol counter printed as "retrainCounter". */
	unsigned int retrainCounter;
	/** Symbols accumulated toward the minimum data dwell before rate changes. */
	unsigned int dataDurationCounter;
	/** Symbol clock printed as "fadeCounter"; ages retrain/renegotiation counts. */
	unsigned int fadeCounter;
	/** Symbols accumulated between debug actions; inferred from debugPeriod use. */
	int debugPeriodCounter;

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
	/** Phase-3 fallback duration in symbols; reset to CONNEVAL_INITIAL_PERIOD. */
	unsigned int phase3FallbackDuration;			/* +0x64 reset plants 1600   */
	/** Phase-4 fallback duration in symbols; reset to CONNEVAL_INITIAL_PERIOD. */
	unsigned int phase4FallbackDuration;			/* +0x68 reset plants 1600   */
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
	 * THE NAMES ARE NOW CHANGED.  This pair used to keep its offset names
	 * solely because `V90Demodulator.cpp` refers to +0x70/+0x74 through
	 * `connectionEvaluator->` (four sites, all in `enterPhase3`) and
	 * belonged to other work at the time; the second naming pass carried
	 * the rename through that file, which this batch does own.  (The
	 * earlier claim that `VpcmFloModem.cpp` also names these two was
	 * stale -- that file's own reference is to +0x78/+0x7c, the
	 * DIFFERENT pair below, and grep over the whole tree outside `re/`
	 * finds no `word_70`/`word_74` there at all.)  `avePdsnr` is rule 1:
	 * eighteen `edprintf`/`dsplibs_debug_printf` sites across
	 * `evaluatePhase3`, `evaluatePhase4` and `evaluateConnection` print
	 * its value beside the literal word "avePdsnr" -- e.g. "due to large
	 * error, avePdsnr = %c%d.%03d".  `avePdsnrNofSymbols` has no string of
	 * its own; it is named on usage inference, being the running weight
	 * `updateAvePdsnr` folds every new measurement's `nofSymbols` into and
	 * the class's own local variable naming (`nofOldSymbols`,
	 * `nofSymbols`) already calls it that.  Finding F10134.  The TYPE of
	 * +0x70 is corrected as before, which `V90Demodulator.cpp` does not
	 * notice: it assigns a literal 0, and `0` into a float is the same
	 * four zero bytes `0` into an `unsigned int` was.
	 */
	float avePdsnr;			/* +0x70 the average PDSNR           */
	unsigned int avePdsnrNofSymbols;	/* +0x74 the symbols it averages    */
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
	 *
	 * THE NAMES ARE NOW CHANGED; finding F7485 recorded the evidence and
	 * F9480 is the batch that carried it through.  The old name,
	 * `word_78`, was spelled 45 times across nine files and
	 * `V90Equalizer` has a DIFFERENT member of the same name at its own
	 * +0x78 -- so this rename touched every referrer of THIS class's
	 * +0x78 (`VpcmFloModem.cpp`, this file's own offset asserts, and the
	 * unit tests) and left `V90Equalizer::word_78` alone, being a
	 * different field of a different class at a coincident offset.
	 *
	 * +0x7c HAS NO STRING OF ITS OWN.  `delayedRetrainArmed` is usage
	 * inference, not rule 1: it is the copy `getV90CpBits` makes of
	 * +0x78, and `evaluatePhase4` fires its delayed-retrain arm only once
	 * BOTH are non-zero -- so the request is not honoured until the copy
	 * "arms" it, which is the object's own reading and not a name it
	 * states.  Weaker evidence than +0x78's, and said so here rather than
	 * left silent.
	 */
	unsigned int delayedRetrainRequest;	/* +0x78 copied to delayedRetrainArmed */
	unsigned int delayedRetrainArmed;	/* +0x7c                              */

	/*
	 * +0x80  Wave 6 (F10174): PURELY INTERNAL TO THE DEBUG-ALTERNATE ARM.
	 * `evaluateConnection`'s tail, gated on `debugAlternateDebug != 0`,
	 * runs a `switch (debugAlternateState)` that cycles 0 -> 1 -> 2 -> 3 -> 0
	 * (case 3 resets it), issuing a different forced verdict each time --
	 * "Alternate Debug Retrain", "...RRN up", "...Retrain" again, "...RRN
	 * down" -- named on CLAUDE.md rule 3 (usage inference): the field's
	 * only role anywhere in the object is that four-state cycle, and
	 * `debugAlternateState` says so without claiming a name the object
	 * states -- no format string prints the NUMBER, only the four verdict
	 * strings.
	 */
	unsigned int debugAlternateState;	/* +0x80 zeroed by reset      */

	/*
	 * +0x84 and +0x88  Wave 6 (F10174): TWO EVALUATION-REGIME FLAGS,
	 * mutually exclusive in `evaluatePhase3`'s `if (altRbsDetectedOnQc) ...
	 * else if (trn1dEvalEnabled) ... else if (phase3EvalEnabled) ...`
	 * chain -- `trn1dEvalEnabled`'s arm compares `avePdsnr` against
	 * `TRN1D_ERROR_FOR_V34_FALLBACK`, `phase3EvalEnabled`'s against
	 * `PHASE3_ERROR_FOR_V34_FALLBACK` and `PDSNR_THRESHOLD_IN_PHASE3` --
	 * so which parameter family each guards is what types the pair.
	 *
	 * THE EVIDENCE IS ASYMMETRIC, and said so rather than smoothed over.
	 * `trn1dEvalEnabled` is CLAUDE.md rule 1: `V90Demodulator::progress`'s
	 * `word_3c == 0x06` arm sets +0x88 to 1 beside
	 * `edprintf("V90Demodulator: enabling connectionEvaluator of
	 * TRN1d\r\n")` -- the author's own words for exactly this store.
	 * `phase3EvalEnabled` has NO comparable "enabling" site anywhere in
	 * this tree; a whole-tree grep for `word_84 = 1`/`->word_84 =` outside
	 * this header finds only clears (`progress`'s `word_3c == 0x08` and
	 * `0x11` arms, beside `edprintf("V90Demodulator: disabling
	 * connectionEvaluator of Phase3\r\n")`, and `enterPhase3` itself), so
	 * the only support for `phase3EvalEnabled` is the structural symmetry
	 * with its already-named twin and the parameter family its own arm
	 * reads -- rule 3, not rule 1, and the name is offered on that
	 * strength alone. If a caller that sets it turns up outside this
	 * tree's reach, re-check this derivation rather than assuming it
	 * holds.
	 */
	unsigned int phase3EvalEnabled;	/* +0x84 cleared by enterPhase3      */
	unsigned int trn1dEvalEnabled;		/* +0x88 cleared by enterPhase3;
						 *       reset does NOT touch it     */

	/*
	 * +0x8c  Set to -1 by `reset`, not to 0 -- `mov $0xffffffff,%eax`, a
	 * full 32-bit store -- which is why it is `int` and not `unsigned`.
	 *
	 * NAMED ON USAGE INFERENCE.  `evaluateConnection`'s own leading comment
	 * (see the .cpp) calls this "the external demand" and says so from the
	 * dispatch itself: nothing in the class ever writes it except `reset`,
	 * so some UNWRITTEN caller plants a code here between calls, and stage
	 * 2 of `evaluateConnection` is one `if (externalDemandCode > -1)` guard
	 * followed by a `switch` whose six live values (1, 2, 3, 4, 5, 6) are
	 * exactly the caller-demanded forms of the same verdicts the class
	 * otherwise reaches by measurement -- RRN up, RRN down, no-restriction
	 * RRN, retrain, fall back to V34, and a sixth ("fast parameter
	 * exchange") no other path reaches at all.  No format string prints
	 * the field itself, so this is CLAUDE.md's weakest tier, but the
	 * mechanism it names is not in doubt: `externalDemandCode == -1` is
	 * idle and every other value in range is a demand the switch services
	 * and then re-arms to -1.  Finding F10134.
	 */
	int externalDemandCode;

	/*
	 * +0x90  Zeroed by `reset` and by all four of the members that tell
	 * the evaluator something changed -- `indicateLocalRetrain`,
	 * `indicateRemoteRetrain`, `indicateRemoteRateReneg` and
	 * `updateCurrentConstellationData`. Four functions, one store each,
	 * and it is the first thing three of them do. Rate-change paths set
	 * it from RRN_SILENCE_REQUESTED; consumers forward its low 16 bits as
	 * a request value, so it must not be reduced to a Boolean.
	 */
	/** Current silence-RRN request value, copied from RRN_SILENCE_REQUESTED. */
	unsigned int silenceRrnRequest;		/* +0x90 zeroed by reset and by four */

	/*
	 * +0x94  Wave 6 (F10174): CLAUDE.md rule 1, a format string matched
	 * almost verbatim.  `evaluateConnection` checks it twice -- once right
	 * after the "One Rate Down" arm decides, once in the tail's "rate-down
	 * override, the second of its two copies" -- and both times the same
	 * shape: `if (retrainInsteadOfRateDown != 0 && verdict ==
	 * V90CE_VERDICT_RRN_DOWN)` upgrades the verdict to a retrain, beside
	 * the diagnostic "V90ConnectionEvaluator: initiating Retrain instead
	 * of One Rate Down !!" -- the field's own name, near enough to quote.
	 * It is set to 1 in exactly one place, `V90Demodulator::progress`'s
	 * "'Problematic' ISP Modem detected on USB" arm (masking the drop to
	 * V.34 for a signature-matched MP that would otherwise demand a rate
	 * down), and cleared only by `reset`.
	 */
	unsigned int retrainInsteadOfRateDown;	/* +0x94 zeroed by reset      */
	/*
	 * +0x98  Was `pad_98[4]`, and `evaluateConnection` is its only
	 * writer: five 32-bit stores, four of them a plain zero (0x3e869,
	 * 0x3eac0, 0x3ec02, 0x3ece1) and one at 0x3ee4f the value of
	 * `externalDemandCode == 5`:
	 *
	 *     3ee43:  83 fa 05        cmp    $0x5,%edx
	 *     3ee4c:  0f 94 c0        sete   %al
	 *     3ee4f:  89 87 98 00 00  mov    %eax,0x98(%edi)
	 *
	 * Every store is beside a store to +0x90, and the four zeroes are on
	 * exactly the paths that also set +0x90 -- so the two travel together
	 * and +0x98 selects forced rate-down in the silence-RRN consumer,
	 * which prints "FORCED rate down on silence rrn". Its four-byte
	 * stores remain unsigned; naming the flag does not narrow its storage.
	 */
	/** Nonzero requests forced rate-down during silence RRN. */
	unsigned int forceRateDownOnSilenceRrn;

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
	 * The field is now named `initDmin`. It used to keep its offset name
	 * solely because `test/unit/t_v90leaves.cpp` and `t_v90conneval.cpp`
	 * referred to it by that name and belonged to other work; finding
	 * F9480's batch carried the rename through both, `altRbsDetectedOnQc`
	 * and `echoRrnState` below the same way.
	 */
	short initDmin;

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
	 * with `params->PDSNR_CURRENT_V34_DROP_THRESH_PHASE4` (a plain float
	 * read; 250.0f by default, finding F878).
	 *
	 * The author's own name for it is `pdsnrCurrentV34DropThreshPhase4`,
	 * legible in `evaluatePhase4`: 0x3fd38 is `fsts 0xac(%ebx)` and the
	 * diagnostic beside exactly that store reads "V90ConnectionEvaluator
	 * (phase4): pdsnrCurrentV34DropThreshPhase4 set to = %c%d.%03d". Same
	 * derivation `curDmin` above rests on -- a store and a string naming
	 * the value stored.
	 *
	 * The field keeps the name the lifecycle batch gave it,
	 * `phase4ErrorForV34Fallback` -- a derivation too (it is the parameter
	 * `reset` copies in), and unrelated to `initDmin`, `altRbsDetectedOnQc`
	 * and `echoRrnState` being offset-named below until F9480: this field
	 * was never offset-named, so there was nothing to carry through here.
	 * The two names are consistent -- `reset` initialises the threshold
	 * from `PHASE4_ERROR_FOR_V34_FALLBACK` and `evaluatePhase4` replaces it
	 * with `params->PDSNR_CURRENT_V34_DROP_THRESH_PHASE4` (250.0f by
	 * default, finding F878) the first time it asks for a retrain, so the
	 * slot is the current threshold and the parameter is only where it
	 * starts.
	 *
	 * `params->PDSNR_CURRENT_V34_DROP_THRESH_PHASE4` is a float:
	 * `flds 0x434(%ecx)` into `fsts 0xac(%ebx)` is a float load and a
	 * float store with no conversion between them, and 0x437a0000 is
	 * 250.0f. `V90Parameters.h` now types the slot `float` and
	 * `V90ConnectionEvaluator.cpp` reads it directly; the `int` typing and
	 * the union workaround are gone (finding F878, issue #127).
	 */
	float phase4ErrorForV34Fallback;

	/*
	 * +0xb0 .. +0xb4  Three 16-bit flags, 1, 0 and 0.  +0xb4 is the
	 * highest byte anything written here touches; the object runs to
	 * 0xbc.
	 *
	 * +0xb0, `meanErrorCheckArmed`, is the one-shot that arms
	 * `evaluatePhase4`'s mean-error arm. `reset` sets it to 1 and nothing
	 * else in the class sets it again; `evaluatePhase4` tests it first of
	 * three guards and clears it when all three pass, so the arm fires at
	 * most once per reset. Sixteen bits: `cmpw $0x0,0xb0(%ebx)` and
	 * `mov %dx,0xb0(%ebx)`. Named on usage inference -- no string of its
	 * own, but the mechanism is exactly what `delayedRetrainArmed` above
	 * was named for: a one-shot latch that permits a check to fire once.
	 * Finding F10134.
	 *
	 * +0xb2 is `altRbsDetectedOnQc`, and that is the object's own word for
	 * it: `evaluatePhase3`'s first arm runs when +0xb2 is non-zero, clears
	 * it, and prints "V90ConnectionEvaluator (phase3): altRbsDetectedOnQc
	 * => initiating Retrain" -- the flag is the detection and the arm is
	 * what services it. The field is now named for it, `t_v90leaves.cpp`
	 * and `t_v90conneval.cpp` carried through in finding F9480's batch.
	 *
	 * The claim that nothing sets it is retracted; it was already stale
	 * when this paragraph was last true. `V90Demodulator::progress`'s
	 * phase-3 leg does, at quick connect, out of
	 * `autoDigitalImpDetector->isThereAnyAltRbsPhase()` -- see that
	 * function's own comment for the site.
	 *
	 * +0xb4 is `echoRrnState`, and that is the object's own word for it
	 * too. `evaluateConnection` increments it at 0x3ea9c and then prints
	 * exactly the value it stored:
	 *
	 *     3ea99:  8d 43 01        lea    0x1(%ebx),%eax
	 *     3ea9c:  66 89 87 b4 ..  mov    %ax,0xb4(%edi)
	 *     3f489:  98              cwtl
	 *     3f48e:  e8 ..           call   dsplibs_debug_printf
	 *                             "V90-mod3 CHANGE echoRrnState = %d"
	 *
	 * -- a store and a diagnostic naming the value stored, the derivation
	 * `curDmin` rests on. A second string, 0xa2f4, prints the same slot as
	 * "V90-mod3  Echo_Rrn_State = %d", and `reset`'s unconditional
	 * " *********** Echo Rrn Mechanism *********** " is the third mention.
	 * It is a four-state machine: 0 and 1 pick the 1.52 and 1.39
	 * multipliers the rate-down threshold is scaled by, the increment at
	 * 0x3ea9c advances it every time a rate down is demanded, and 3 --
	 * planted at 0x3eec0 -- is the terminal state in which the multiplier
	 * is dropped altogether. Sixteen bits and signed: `cmpw $0x1`,
	 * `cmpw $0x2; jg` and `movswl %bx,%esi` before the diagnostic.
	 *
	 * The field is now named, for the third time in this batch:
	 * `t_v90leaves.cpp` and `t_v90conneval.cpp` carried through to match.
	 */
	short meanErrorCheckArmed;
	short altRbsDetectedOnQc;
	short echoRrnState;

	/*
	 * +0xb6 WAS `pad_b6[2]`, REMOVED (2026-09-04, pad-audit).  A `short`
	 * ending at +0xb6 followed by the 4-byte-aligned `word_b8` float below
	 * needs exactly this 2-byte gap for natural alignment, no dis.py
	 * reader/writer touches +0xb6/+0xb7 anywhere in the object, and
	 * `CE_OFF(word_b8, 0xb8, ...)` in the .cpp already asserts the next
	 * field's offset -- so the compiler's own padding reproduces the
	 * member being deleted.
	 */

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
	/**
	 * Applied only as a scale in avePdsnr * word_b8; assigned from a local
	 * named scale. Its wider semantic role is not established.
	 */
	float word_b8;
};

#endif /* DSPLIB_V90CONNECTIONEVALUATOR_H */
