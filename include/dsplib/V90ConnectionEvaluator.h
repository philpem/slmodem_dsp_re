/*
 * V90ConnectionEvaluator.h -- the V.90 connection evaluator, so far as its
 * constructor, destructor and `reset` need it.
 *
 * Reconstructed from dsplibs.o.  THE CLASS DID NOT ARRIVE WITH THIS FILE.  It
 * was defined inside `V90Demodulator.h` from task #60 onwards, because the
 * only things in the tree that touched it were `V90Demodulator::enterPhase3`
 * (four words it clears) and `VPcmFloModem::getV90CpBits` (a copy from +0x78
 * to +0x7c).  Task #88 wrote the three members the class actually owns, so it
 * has its own header now.  **No field moved and no field was renamed** --
 * `word_70`, `word_74`, `word_78`, `word_7c`, `word_84` and `word_88` are the
 * names those two earlier batches gave them, and the offset assertions moved
 * from `V90Demodulator.cpp` to `V90ConnectionEvaluator.cpp` unchanged.
 *
 * NOT POLYMORPHIC.  `nm` gives `D1` at 0x3e390 and `D2` at 0x3e380 and no
 * `D0`; GCC emits a deleting destructor only for a virtual class, so offset 0
 * is a real member and there is no vptr (finding F228).
 *
 * THE OBJECT IS 0xbc = 188 BYTES, and that is an allocation and not a
 * displacement: `movl $0xbc,(%esp); call sysdep_malloc` at 0x1c47e inside
 * `V90Demodulator`'s constructor, with the result passed straight to
 * `V90ConnectionEvaluator(V90Parameters *)`.  `reset`'s highest write is the
 * two-byte store at +0xb4, so six bytes at the end are untouched by anything
 * written here.
 *
 * WHAT `reset` SAYS ABOUT THE OBJECT.  Nineteen of its forty-eight stores are
 * copies out of the parameter block, and every one of those nineteen slots
 * has the ORIGINAL AUTHOR'S OWN NAME (findings F860-862, `tools/vparse.py`).
 * The field names below are taken from the parameter each field receives --
 * `retrainDetectDuration` holds `params->RETRAIN_DETECT_DURATION` and nothing
 * else assigns to it in anything reconstructed so far -- which is a
 * derivation and not a guess.  The other twenty-nine are zeroes, two -1s and
 * two 1600s, and those are offset-named because a zero says nothing about
 * what a field holds.
 *
 * THE CONSTRUCTOR IS FIFTEEN BYTES AND MOST OF `reset` IS ITS BODY:
 *
 *     3e560:  8b 54 24 04     mov 0x4(%esp),%edx     this
 *     3e564:  8b 44 24 08     mov 0x8(%esp),%eax     the V90Parameters *
 *     3e568:  89 02           mov %eax,(%edx)
 *     3e56a:  e9 ..           jmp <V90ConnectionEvaluator::reset>
 *
 * -- a tail call, so the object is "store the parameter block, then reset".
 * The destructor is one byte, `c3`.
 */

#ifndef DSPLIB_V90CONNECTIONEVALUATOR_H
#define DSPLIB_V90CONNECTIONEVALUATOR_H

/*
 * A POINTER ONLY, so a forward declaration is what belongs here.  Two
 * incompatible definitions of `V90Parameters` exist in this tree -- the 0x504
 * word block in `V90PreFilter.h` and the 0x558 named map in
 * `V90Parameters.h` -- and no translation unit may include both.  This header
 * is included by `V90Demodulator.h`, which pulls in the first; the .cpp picks
 * the second, because the names are the point.  Finding F1112.
 */
class V90Parameters;

/*
 * TWO OF THE VERDICTS THE CLASS RETURNS, and only two: `indicateLocalRetrain`
 * and `indicateRemoteRetrain` each load one of a pair of immediates into %esi
 * and move it to %eax on the way out --
 *
 *     40031:  be 04 00 00 00   mov $0x4,%esi     the ordinary path
 *     40074:  be 05 00 00 00   mov $0x5,%esi     the path that also prints
 *                                               "initiating fall back to V34"
 *
 * -- so 5 is "give up on V.90 and fall back to V.34" and 4 is "retrain", by
 * the string on the path that returns it.  A void function would not build
 * %eax at all, and this is what proves the two are not void; the tail `jmp
 * edprintf` in `updateCurrentConstellationData` proves nothing of the kind,
 * because a void function tail-jumping to a non-void one is ordinary output.
 *
 * A THIRD ANSWER, ADDED BY `evaluatePhase3` AND `evaluatePhase4`: zero, and it
 * is a real answer rather than an artefact.  Both functions clear the register
 * on entry (`xor %esi,%esi`, `xor %eax,%eax`) and both have paths that reach a
 * `ret` without ever loading an immediate into it -- the early return on a
 * zero symbol count, and the ordinary "nothing crossed a threshold" tail.  A
 * caller therefore sees 0 far more often than 4 or 5.  THE NAME IS OURS; the
 * object names no enumerator anywhere, and 4 and 5 are named only because a
 * string on each path says what it means.
 *
 * THE ENUMERATION IS NOW COMPLETE, and `evaluateConnection` is what completes
 * it.  That function has TWO epilogues and between them SIX distinct values
 * reach %eax:
 *
 *   0x3e8b7  `xor %eax,%eax` at 0x3e6ee      the empty call, 0
 *            `mov %ecx,%eax` at 0x3e8a2      %ecx is 4 (0x3e80d), 2 (0x3e857),
 *                                            5 (0x3f014) or 4 (0x3f2f0)
 *   0x3ed18  `mov %ebp,%eax` at 0x3ed0c      %ebp is 0 (0x3e6d1, 0x3ed7a),
 *                                            1, 2, 3, 4 or 5 -- twenty
 *                                            immediates in all
 *
 * so the set is {0, 1, 2, 3, 4, 5} and -1 IS NOT ONE OF THEM.  The two
 * negative-looking immediates near the epilogues belong to fields and not to
 * the answer: `mov $0x0,%esi` at 0x3ed07 feeds `mov %esi,0x70(%edi)` (the
 * average, cleared), `mov $0xffffffff,%esi` at 0x3ede4 feeds `mov
 * %si,0x9c(%edi)` (+0x9c, set to -1), and `mov $0xffffffff,%ebp` at 0x3f1f3
 * does the same before %ebp is reloaded with 4 at 0x3f292.
 *
 * THE THREE NEW NAMES ARE THE STRINGS' -- four diagnostics each:
 *
 *   1  "Initiating RRN up (external demand)", "Debug One Rate Up", "Alternate
 *      Debug RRN up", "initiating One Rate Up demand"
 *   2  "Initiating RRN Down (external demand)", "Debug One Rate Down",
 *      "Alternate Debug RRN down", "initiating One Rate Down demand"
 *   3  "Initiating No Restriction RRN (external demand)" -- one string, and
 *      the only path that reaches it is the external-demand arm for +0x8c
 *      values 1 and 4
 */
#define V90CE_VERDICT_NONE		0
#define V90CE_VERDICT_RRN_UP		1
#define V90CE_VERDICT_RRN_DOWN		2
#define V90CE_VERDICT_RRN_NO_RESTRICT	3
#define V90CE_VERDICT_RETRAIN		4
#define V90CE_VERDICT_FALLBACK_V34	5

class V90ConnectionEvaluator {
public:
	/*
	 * The three members task #88 defines.  The signatures are the
	 * mangling's: `_ZN22V90ConnectionEvaluatorC1EP13V90Parameters`,
	 * `_ZN22V90ConnectionEvaluatorD1Ev` and
	 * `_ZN22V90ConnectionEvaluator5resetEv`.
	 */
	V90ConnectionEvaluator(V90Parameters *params);
	~V90ConnectionEvaluator();
	void reset();

	/*
	 * SIX MORE, DEFINED.  Argument types are the mangling's; the return
	 * types are NOT mangled and were read out of what each function
	 * leaves in %eax -- void where nothing arranges it, `int` for the
	 * three that do.  See the verdict macros above.
	 */
	void updateAvePdsnr(float pdsnr, unsigned int nofSymbols);
	void updateCurrentConstellationData(short curDmin, float threshUp,
					    float threshDown,
					    float threshRetrain);
	void indicateRemoteRateReneg();
	int evaluateMeanErrorStdPhase4(float, float);
	int indicateLocalRetrain();
	int indicateRemoteRetrain();

	/*
	 * TWO MORE, DEFINED, and both were on the undefined list below with a
	 * `void` return until the batch that wrote them read the epilogues.
	 * Both build %eax and both answer 0, 4 or 5; see the verdict block
	 * above.  `evaluatePhase4`'s argument is named after the two strings
	 * that print it and nothing else.
	 */
	int evaluatePhase3();
	int evaluatePhase4(float meanErrBefToAftUpdateRatio);

	/*
	 * THE BIG ONE, 3,857 BYTES AT 0x3e6d0, AND IT RETURNS `int`.  Both of
	 * its epilogues build %eax -- `mov %ecx,%eax` at 0x3e8a2 and `mov
	 * %ebp,%eax` at 0x3ed0c -- and the entry `xor %ebp,%ebp` at 0x3e6d1
	 * gives the do-nothing paths their 0.  See the verdict block above for
	 * the six values and where each comes from.
	 */
	int evaluateConnection();

	/*
	 * Declared for the record and deliberately left undefined -- their
	 * signatures come from the mangling, so this list is a specification
	 * rather than a guess, and a return type is not mangled and is
	 * therefore unknown for both of them.  They belong to whichever batch
	 * writes the last two members of this class's processing half.
	 */
	void evaluateMeanErrorStdPhase3(float);
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
	 * +0x04 .. +0x24  Nine consecutive counters `reset` zeroes.  THREE OF
	 * THEM NOW HAVE THE ORIGINAL AUTHOR'S OWN NAMES, out of the strings
	 * the three `indicate*` members print and the parameters they compare
	 * against; the other six are still offset-named because a zero says
	 * nothing about what a field holds.
	 *
	 * All three are UNSIGNED, and that is forced rather than chosen:
	 * `cmp 0x460(%eax),%edx; ja` at 0x40045 is an unsigned branch against
	 * a slot the parameter map calls `int`, which is what a comparison
	 * between an `unsigned int` and an `int` compiles to.  A signed field
	 * would have given `jg`.
	 */

	/*
	 * +0x04  Local V.90 retrains.  `indicateLocalRetrain` increments it,
	 * compares it against `params->MAX_NOF_V90_RETRAINS` and prints it as
	 * "%d V90 retrains" when it is over.
	 */
	unsigned int nofV90Retrains;

	/*
	 * +0x08  Remote rate renegotiations.  `indicateRemoteRateReneg`
	 * increments it and prints it as "(rrn no %d)".  Nothing compares it
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
	 * +0x10  A DURATION IN SYMBOLS, and `evaluatePhase3` and
	 * `evaluatePhase4` are what say so: each adds `word_74` -- the count
	 * of symbols the running average covers -- to it whenever the average
	 * is over its fall-back threshold, clears it outright whenever the
	 * average is not, and gives up on V.90 when it reaches the 1600 at
	 * +0x64 (phase 3) or +0x68 (phase 4).  So it measures how long the
	 * error has been too large, in symbols, and the two 1600s are the two
	 * patiences.  Unsigned: `cmp 0x64(%ebx),%eax; jb` at 0x3f6a0.
	 *
	 * IT KEEPS ITS OFFSET NAME.  "How long the error has been large" is a
	 * reading of what the arithmetic does, not the author's word for it,
	 * and finding F226's rule is that a reading does not earn a name.
	 */
	unsigned int word_10;
	unsigned int word_14;
	/*
	 * +0x18 and +0x1c  Zeroed by BOTH `indicateLocalRetrain` and
	 * `indicateRemoteRetrain`, on both of their paths, and by nothing
	 * else the lifecycle batch read.
	 *
	 * +0x18 IS THE SECOND DURATION, and it is +0x10's twin: both
	 * evaluators add `word_74` to it while the average is over
	 * `PDSNR_THRESHOLD_IN_PHASE3` or `..._IN_PHASE4`, clear it when the
	 * average is not, and ask for a retrain when it reaches the +0x60 copy
	 * of `RETRAIN_DETECT_DURATION`.  `cmp 0x60(%ebx),%eax; jb` at 0x3f797
	 * is unsigned against a slot the parameter map calls `int`, which is
	 * what fixes the field's own signedness.
	 *
	 * IT IS NOT CLEARED WHEN IT MERELY ACCUMULATES.  0x3f797 and 0x3fd12
	 * both jump PAST the store that zeroes it, so the clear happens on
	 * exactly two paths -- the average fell below the threshold, or the
	 * duration ran out and a verdict was printed.  A version that cleared
	 * it every call could never accumulate past one call and would still
	 * agree with the blob on any single-call test.
	 *
	 * WHAT +0x1c COUNTS IS SYMBOLS SINCE THE LAST DECISION, and
	 * `evaluateConnection` is what says so: `add %ebx,0x1c(%edi)` at
	 * 0x3e6f8 adds the symbol count of every non-empty call, and every arm
	 * that decides anything clears it.  It is a MINIMUM DWELL rather than a
	 * patience: three tests read it -- against
	 * `minDurationInDataBeforeRrnUp` (0x3e949), against
	 * `minDurationInDataBeforeRrnDown` (0x3ebec) and against a 2.3x, 1.3x
	 * and 0.6x of the same slot on the echo-RRN path -- and all three
	 * BLOCK a rate change that has otherwise been earned.  It keeps its
	 * offset name; "how long since the last decision" is a reading of the
	 * arithmetic and finding F226's rule is that a reading does not earn a
	 * name.
	 *
	 * +0x20 IS THE FADE CLOCK, read only at 0x3e706..0x3e754: the three
	 * retrain/reneg counters each lose one whenever `word_20 / fadeCount`
	 * crosses an integer, and +0x20 then advances by the symbol count.
	 *
	 * +0x24 IS `int` AND NOT `unsigned`, which is the one place in these
	 * eight where the two readings part.  All five debug arms of
	 * `evaluateConnection` do `word_24 += word_74; cmp 0x6c(%edi),%eax;
	 * jl` (0x3ec92, 0x3ed39, 0x3ef02, 0x3efda, 0x3f3ba) -- a SIGNED branch
	 * against `debugPeriod`, which the map calls `int`.  An `unsigned int`
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
	 * +0x64 and +0x68  THE TWO 1600s ARE TWO DIFFERENT LIMITS, which only
	 * the disassembly separates because `reset` gives them the same value:
	 * `evaluatePhase3` compares +0x10 against +0x64 (0x3f69a, 0x3f879) and
	 * `evaluatePhase4` compares it against +0x68 (0x3fb13).  A
	 * reconstruction that read the wrong one of the pair would pass every
	 * test that left them at 1600.
	 */
	unsigned int word_64;			/* +0x64 reset plants 1600   */
	unsigned int word_68;			/* +0x68 reset plants 1600   */
	int debugPeriod;			/* +0x6c                     */

	/*
	 * +0x70 and +0x74  THE RUNNING AVERAGE PDSNR AND ITS WEIGHT, which is
	 * what `updateAvePdsnr` does to them and the reason the pair is
	 * cleared together by `enterPhase3`:
	 *
	 *     +0x74 == 0 ->  +0x70 = pdsnr;  +0x74 = nofSymbols
	 *     otherwise  ->  +0x70 = (+0x74 * +0x70 + pdsnr * nofSymbols)
	 *                            / (+0x74 + nofSymbols);
	 *                    +0x74 += nofSymbols
	 *
	 * so +0x70 is a FLOAT -- `fmuls 0x70(%ebx)` and `fstps 0x70(%ebx)`,
	 * a four-byte load and a four-byte store, never an integer move --
	 * and +0x74 is an UNSIGNED count, because both conversions of it are
	 * `push $0; push %reg; fildll`, the zero-extending 64-bit form GCC
	 * uses for `unsigned int` and never for `int`.
	 *
	 * THE NAMES ARE NOT CHANGED, deliberately.  `V90Demodulator.cpp` and
	 * `VPcmFloModem.cpp` refer to these four by their offset names and
	 * belong to other work; renaming them here would edit files this
	 * batch does not own.  The derivation is recorded instead -- and the
	 * TYPE of +0x70 is corrected, which those files do not notice: both
	 * assign it a literal 0, and `0` into a float is the same four zero
	 * bytes `0` into an `unsigned int` was.
	 */
	float word_70;			/* +0x70 the average PDSNR           */
	unsigned int word_74;		/* +0x74 the symbols it averages     */
	/*
	 * +0x78 and +0x7c were `pad_78` until the VPcmFloModem batch read a
	 * second function that touches this object: `getV90CpBits` copies
	 * +0x78 to +0x7c each time a CP sequence finishes.  Two batches, two
	 * functions, one object -- neither would have found both.
	 *
	 * A THIRD FUNCTION SAYS WHAT THE PAIR IS FOR.  `evaluatePhase4`'s last
	 * arm runs only when BOTH are non-zero, prints "Initiating retrain
	 * (delayed)...", clears both, and then counts a retrain exactly as the
	 * other arms do -- so this is a retrain that was asked for earlier and
	 * is honoured here, and it is the only thing in the class that reads
	 * either slot.  The pair is a request and its acknowledgement, which is
	 * why `getV90CpBits` copying one to the other arms it.
	 *
	 * AND A FOURTH FUNCTION SUPPLIES THE AUTHOR'S OWN WORD FOR +0x78.
	 * `V90Demodulator::exitPhase3` raises the request -- `movl $0x1,
	 * 0x78(%esi)` at 0x1bd54 -- when `V90TRN2Design` cannot design the
	 * TRN2 constellations, and the `dsplibs_debug_printf` beside that
	 * store is
	 *
	 *     "V90Demodulator::exitPhase3() delayedRetrainRequest !!!"
	 *
	 * which is CLAUDE.md's evidence rule 1 and names the slot exactly:
	 * the request half of the pair is `delayedRetrainRequest`.  It agrees
	 * with `evaluatePhase4`'s "Initiating retrain (delayed)..." reached
	 * from the same slot.
	 *
	 * THE NAMES ARE STILL NOT CHANGED, and the reason is coordination
	 * rather than evidence.  `word_78` is spelled 45 times across nine
	 * files, `V90Equalizer` has a DIFFERENT member of the same name at
	 * its own +0x78, and `VPcmFloModem.cpp` and two mutation sets refer
	 * to this one by its offset name and belong to other work.  A rename
	 * here would be a nine-file edit whose only checkable part is that
	 * nothing broke.  The evidence is recorded so that the pass which
	 * owns those files can make it in one move; finding F7485.
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
	 * +0x90  Zeroed by `reset` AND by all four of the members that tell
	 * the evaluator something changed --  `indicateLocalRetrain`,
	 * `indicateRemoteRetrain`, `indicateRemoteRateReneg` and
	 * `updateCurrentConstellationData`.  Four functions, one store each,
	 * and it is the first thing three of them do.  A "how long since the
	 * picture last moved" counter is the obvious reading and it is a
	 * reading, not a derivation, so the field keeps its offset name.
	 */
	unsigned int word_90;		/* +0x90 zeroed by reset and by four */
	unsigned int word_94;		/* +0x94 zeroed by reset             */
	/*
	 * +0x98  WAS `pad_98[4]` AND `evaluateConnection` IS ITS ONLY WRITER.
	 * Five 32-bit stores, four of them a plain zero (0x3e869, 0x3eac0,
	 * 0x3ec02, 0x3ece1) and one at 0x3ee4f the value of `word_8c == 5`:
	 *
	 *     3ee43:  83 fa 05        cmp    $0x5,%edx
	 *     3ee4c:  0f 94 c0        sete   %al
	 *     3ee4f:  89 87 98 00 00  mov    %eax,0x98(%edi)
	 *
	 * Every store is beside a store to +0x90, and the four zeroes are on
	 * exactly the paths that also set +0x90 -- so the two travel together
	 * and +0x98 is a second flag of whatever +0x90 is the first of.
	 * Nothing reconstructed so far READS it, which is why it keeps an
	 * offset name: a store with no reader says how wide the slot is and
	 * nothing about what it means.  Four bytes, `movl`, so `unsigned int`
	 * -- and the width is what retires the pad.
	 */
	unsigned int word_98;

	/*
	 * +0x9c, +0x9e  A 16-bit pair, -1 and 0.  The store widths are the
	 * evidence: `mov %cx,0x9c(%ebx)` with %ecx holding 0xffffffff writes
	 * two bytes and leaves 0xffff, which is a different value from the
	 * -1 at +0x8c even though the source constant is the same.
	 *
	 * THE AUTHOR'S OWN NAME FOR +0x9c IS `initDmin`.  Two diagnostics in
	 * `evaluateConnection` print the pair together and name both halves:
	 * 0x9d84 is "before EC RRN: curDmin = %d, initDmin = %d" and 0xa348 is
	 * "on Rate Down: curDmin = %d, initDmin = %d", and at both sites the
	 * second conversion is `movswl 0x9e(%edi)` and the third is `movswl
	 * 0x9c(%edi)` (0x3e79b, 0x3f19a).  It is the initial minimum distance:
	 * the entry test at 0x3e6dd copies +0x9e into it the first time it is
	 * -1, and both arms that ask for a retrain put it back to -1 so the
	 * next call re-latches.  What the pair decides is `curDmin >= 2 *
	 * initDmin` -- the distance having doubled since the connection
	 * settled is what turns a rate-down demand into a retrain.
	 *
	 * THE FIELD KEEPS ITS OFFSET NAME.  `test/unit/t_v90leaves.cpp` refers
	 * to it as `short_9c` and belongs to other work; renaming here would
	 * edit a file this batch does not own, which is the reason +0xac and
	 * +0xb2 kept theirs.
	 */
	short short_9c;

	/*
	 * +0x9e  THE CURRENT MINIMUM DISTANCE, and the name is the object's
	 * own: `updateCurrentConstellationData` stores its `short` argument
	 * here and passes the same value to a diagnostic whose format string
	 * begins "curDmin = %d".  Two bytes, `mov %dx`, and sign-extended
	 * into the diagnostic with `movswl`, which is what makes it `short`
	 * and not `unsigned short`.
	 */
	short curDmin;

	/*
	 * +0xa0 .. +0xa8  THE THREE THRESHOLDS, in the order the same string
	 * names them:
	 *
	 *   "V90ConnectionEvaluator UPDATE: curDmin = %d, 10*threshUp = %d,
	 *    10*threshDown = %d, 10*threshRetrain = %d"
	 *
	 * The three `fsts` at 0x3e61c, 0x3e624 and 0x3e62c take the first,
	 * second and third float argument in that order, and the same three
	 * values -- each multiplied by 10.0f and truncated -- are the second,
	 * third and fourth conversion.  FLOATS, because every access is a
	 * four-byte x87 store and the printed form needs a multiply to make
	 * an integer of it.
	 */
	float threshUp;			/* +0xa0                             */
	float threshDown;		/* +0xa4                             */
	float threshRetrain;		/* +0xa8                             */

	/*
	 * +0xac  = params->PHASE4_ERROR_FOR_V34_FALLBACK.  Copied as a 32-bit
	 * word and never loaded into the x87 stack, so it is moved as a float
	 * rather than converted -- the same shape as V90Phase3Demodulator's
	 * +0x418 and V90Equalizer's +0x90.
	 *
	 * THE AUTHOR'S OWN NAME FOR IT IS `pdsnrCurrentV34DropThreshPhase4`,
	 * and `evaluatePhase4` is where that is legible: 0x3fd38 is
	 * `fsts 0xac(%ebx)` and the diagnostic that announces exactly that
	 * store reads "V90ConnectionEvaluator (phase4):
	 * pdsnrCurrentV34DropThreshPhase4 set to = %c%d.%03d".  It is the same
	 * derivation `curDmin` above rests on -- a store and a string naming
	 * the value stored.
	 *
	 * THE FIELD KEEPS THE NAME THE LIFECYCLE BATCH GAVE IT.  That name is
	 * a derivation too (it is the parameter `reset` copies in), and
	 * `test/unit/t_v90leaves.cpp` refers to the field by it and belongs to
	 * other work; renaming here would edit a file this batch does not own.
	 * The two names are consistent -- `reset` initialises the threshold
	 * from `PHASE4_ERROR_FOR_V34_FALLBACK` and `evaluatePhase4` replaces it
	 * with `params->unnamed_434` (250.0f by default, finding F878) the first
	 * time it asks for a retrain, so the slot is the CURRENT threshold and
	 * the parameter is only where it starts.
	 *
	 * `params->unnamed_434` IS A FLOAT.  `flds 0x434(%ecx)` into `fsts
	 * 0xac(%ebx)` is a float load and a float store with no conversion
	 * between them, and 0x437a0000 is 250.0f.  `V90Parameters.h` types the
	 * slot `int`; that header is a shared frozen type this batch does not
	 * own, so `V90ConnectionEvaluator.cpp` reads the four bytes through a
	 * union rather than retyping it.
	 */
	float phase4ErrorForV34Fallback;

	/*
	 * +0xb0 .. +0xb4  Three 16-bit flags, 1, 0 and 0.  +0xb4 is the
	 * highest byte anything written here touches; the object runs to
	 * 0xbc.
	 *
	 * +0xb0 IS THE ONE-SHOT THAT ARMS `evaluatePhase4`'s MEAN-ERROR ARM.
	 * `reset` sets it to 1 and nothing else in the class sets it again;
	 * `evaluatePhase4` tests it first of three guards and clears it when
	 * all three pass, so the arm fires at most once per reset.  Sixteen
	 * bits: `cmpw $0x0,0xb0(%ebx)` and `mov %dx,0xb0(%ebx)`.
	 *
	 * +0xb2 IS `altRbsDetectedOnQc`, and that is the object's own word for
	 * it: `evaluatePhase3`'s first arm runs when +0xb2 is non-zero, clears
	 * it, and prints "V90ConnectionEvaluator (phase3): altRbsDetectedOnQc
	 * => initiating Retrain" -- the flag is the detection and the arm is
	 * what services it.  THE FIELD KEEPS ITS OFFSET NAME for the same
	 * reason +0xac does: `t_v90leaves.cpp` uses `short_b2` and is not this
	 * batch's file.  Nothing reconstructed so far SETS it, so whatever
	 * detects alternate RBS on the QC path is somewhere still unread.
	 *
	 * +0xb4 IS `echoRrnState`, and that is the object's own word for it
	 * too.  `evaluateConnection` increments it at 0x3ea9c and then prints
	 * exactly the value it stored:
	 *
	 *     3ea99:  8d 43 01        lea    0x1(%ebx),%eax
	 *     3ea9c:  66 89 87 b4 ..  mov    %ax,0xb4(%edi)
	 *     3f489:  98              cwtl
	 *     3f48e:  e8 ..           call   dsplibs_debug_printf
	 *                             "V90-mod3 CHANGE echoRrnState = %d"
	 *
	 * -- a store and a diagnostic naming the value stored, the derivation
	 * `curDmin` rests on.  A second string, 0xa2f4, prints the same slot as
	 * "V90-mod3  Echo_Rrn_State = %d", and `reset`'s unconditional
	 * " *********** Echo Rrn Mechanism *********** " is the third mention.
	 * It is a four-state machine: 0 and 1 pick the 1.52 and 1.39
	 * multipliers the rate-down threshold is scaled by, the increment at
	 * 0x3ea9c advances it every time a rate down is demanded, and 3 --
	 * planted at 0x3eec0 -- is the terminal state in which the multiplier
	 * is dropped altogether.  SIXTEEN BITS AND SIGNED: `cmpw $0x1`,
	 * `cmpw $0x2; jg` and `movswl %bx,%esi` before the diagnostic.
	 *
	 * THE FIELD KEEPS ITS OFFSET NAME, for the third time and the same
	 * reason: `t_v90leaves.cpp` says `short_b4`.
	 */
	short short_b0;
	short short_b2;
	short short_b4;

	unsigned char pad_b6[2];	/* +0xb6 .. +0xb7                    */

	/*
	 * +0xb8  WAS INSIDE `pad_b6[6]`, AND IT IS A FLOAT.
	 * `evaluateConnection` stores the rate-down multiplier there with a
	 * four-byte x87 store the instant it picks it --
	 *
	 *     3e9dd:  d9 05 ..        flds   .rodata.cst4+0x2e4   1.52f
	 *     3e9e8:  d9 97 b8 ..     fsts   0xb8(%edi)
	 *
	 * -- and reads it back with `fmuls 0xb8(%edi)` (0x3f041, 0x3f4a3) to
	 * print `avePdsnr * that` as "error -- =" beside the raw `threshDown`
	 * as "threshold -- =".  Never an integer move in either direction, so
	 * `float`; and the two accesses are the whole of what is known, so it
	 * keeps an offset name.
	 *
	 * NOTHING INITIALISES IT.  `reset` stops at +0xb4, and the diagnostic
	 * at 0x3f041 is on the path where no multiplier was chosen this call,
	 * so it prints whatever the slot last held -- which is why the test
	 * plants a value in it rather than letting the seed decide.
	 *
	 * +0xb8..+0xbb IS THE LAST FOUR BYTES OF THE 0xbc OBJECT, so the class
	 * now has no unmodelled region at all.
	 */
	float word_b8;
};

#endif /* DSPLIB_V90CONNECTIONEVALUATOR_H */
