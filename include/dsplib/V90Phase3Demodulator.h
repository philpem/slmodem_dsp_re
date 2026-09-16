/**
 * @file V90Phase3Demodulator.h
 * @brief The receiver half of V.90 phase 3: two large state machines
 *        (V.90 and V.92 flavours) driving Sd detection, TRN1 training,
 *        data-directed tracking, DIL and impairment probing, and the exit
 *        to phase 4.
 *
 * Reconstructed from dsplibs.o. The class was declared in V90SessionFlag.h
 * as a four-field sketch, because the only member that batch wrote was the
 * eleven-byte `setSessionFlag`; that header's own comment said the first
 * batch to give the class real weight should split it out. This is that
 * split, and everything the sketch said is preserved below -- including that
 * the phase 3 modulator is embedded at +0x34 rather than pointed at.
 *
 * The size is settled, which the sketch said it was not.
 *
 * Finding F268 is right that a `this`-relative displacement scan lies about
 * this class: its largest hits, 0xa948 and 0xa95c, are off a
 * V90AutoDigitalImpDetector* and not off `this`. But the object is
 * heap-allocated, and the allocation is the oracle -- `V90Demodulator`'s
 * constructor reads
 *
 *     movl $0x42c,(%esp); call sysdep_malloc; ... ; call V90Phase3DemodulatorC1
 *     mov  %esi,0x1dc(%ebx)
 *
 * so the object is 0x42c bytes. Two independent facts agree with that and
 * neither was used to derive it: the last field the constructor writes is the
 * pointer at +0x428, which ends at 0x42c, and the largest displacement
 * `reset` uses is the byte at +0x424. Finding F291.
 *
 * Three interior boundaries fall out exactly, which is the check that the
 * subobjects below are subobjects and not coincidence:
 *
 *     +0x034 V90Phase3Modulator   0x398   ends 0x3cc, and +0x3cc is a field
 *     +0x3d0 Descrambler<int,int> 0x020   ends 0x3f0, and +0x3f0 is a field
 *
 * Data member names are invented; the mangling never carries one (finding
 * F226). `sessionFlag` is the exception and is the author's own: `reset`
 * prints +0x08 as "V90Phase3Demodulator: Reset called, sessionFlag = %d".
 * Where nothing names a field it is `word_`/`short_`/`byte_` plus its offset,
 * as in V90SpectralVerifier.h and V90AutoDigitalImpDetector.h -- a guessed
 * name in the record is worse than no name.
 *
 * The pointer types are not guesses: they come from the constructor's
 * mangling, `V90Phase3Demodulator(V90Parameters *, V90SpectralVerifier *,
 * unsigned int, V90AutoDigitalImpDetector *)`, whose four arguments the
 * constructor stores at +0x0c, (used, not stored), +0x08 and +0x00; and from
 * the callee each field is passed to.
 */

#ifndef DSPLIB_V90PHASE3DEMODULATOR_H
#define DSPLIB_V90PHASE3DEMODULATOR_H

#include "dsplib/DiffCoder.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90Phase3Modulator.h"
#include "dsplib/V90SdDetector.h"
#include "dsplib/V92Jd.h"

class V90Parameters;

/*
 * Declared, not included.  `V90SpectralVerifier` is a constructor argument
 * this class accepts and never stores, so nothing here needs its layout;
 * `ANSamToneDetector` is a pointer member, so nothing here needs its layout
 * either, and including it would pull `GenericToneDetector.h` into every
 * translation unit that includes this file.  `V90Phase3Demodulator.cpp`
 * includes the one definition the destructor's call needs.
 */
class V90SpectralVerifier;
class ANSamToneDetector;

/*
 * The state `reset` is told to start in.  The mangled name is
 * `22Phase3DemodulatorState`, so the TYPE's name is the author's; the
 * enumerators' are not, and only three of them are recoverable at all --
 * `reset` names those three in the diagnostics it emits for them:
 *
 *     "initial state set to WaitForSd"           0
 *     "initial state set to TRN1dKnownData"      3
 *     "initial state set to WaitForQTS"          26
 *
 * and calls everything else IRREGULAR, printing the number.  So the gaps are
 * real states this function does not distinguish, not missing enumerators.
 *
 * The underlying type is fixed so that every `int` value is representable and
 * the differential test may sweep the irregular range without reaching for
 * undefined behaviour.  It does not affect the mangling, which is by name.
 *
 * Spelled as a pin rather than a base, because `: int` on an enum is C++11
 * and the author's compiler was C++98.  Dropping it alone would not be
 * harmless here: the three values below give C++98 a range of 0..31, and the
 * sweep this comment relies on would then be undefined for every state above
 * 31.  `_BASE_PIN` is ours and restores exactly what `: int` gave.  Measured
 * identical -- 4 bytes, signed -- under both compilers.
 * docs/method/compilers.md, V2.
 */
enum Phase3DemodulatorState {
	P3D_STATE_WAIT_FOR_SD = 0,		/* the Sd detector runs      */
	P3D_STATE_TRN1D_KNOWN_DATA = 3,		/* TRN1d, known data         */
	P3D_STATE_WAIT_FOR_QTS = 26,		/* V.92: wait for QTS        */

	/*
	 * Added by the phase 3 receive batch, on the same footing as the three
	 * above.  `enterWaitForANSpcmDrop` prints "V90Phase3Demodulator: enter
	 * WaitForANSpcmDrop" -- the author's own words, .rodata.str1.4+0x5ae4
	 * -- and then stores 0x1e here, and the MEMBER's name is the same words
	 * again.  A format string that names the thing is CLAUDE.md's strongest
	 * evidence; the other 30-odd states have no such string and stay as the
	 * casts the two decision functions already spell them with.
	 */
	P3D_STATE_WAIT_FOR_ANS_PCM_DROP = 30,	/* the ANSpcm energy drop    */

	P3D_STATE_BASE_PIN = -0x7fffffff - 1	/* ours: pins the base       */
};

class V90Phase3Demodulator {
public:
	/**
	 * @brief Store the session flag (0 = V.90, non-zero = V.92) that
	 * getDecision() dispatches on.
	 * @param flag  The new session flag value.
	 */
	void setSessionFlag(unsigned int flag);

	/**
	 * @brief Construct the demodulator, wiring up its subobjects and the
	 * two heap-allocated detectors, then reset() to the all-zero/WaitForSd
	 * starting state.
	 *
	 * @p unused is accepted (the caller passes the address of its own
	 * embedded `V90SpectralVerifier`) but never read -- nothing here
	 * stores it.
	 * @param params      The V.90 parameter block.
	 * @param unused      Accepted and dropped; see above.
	 * @param sessionFlag Initial session flag (0 = V.90, non-zero = V.92).
	 * @param adid        The impairment detector this class drives during
	 *                    the DIL/probing states.
	 */
	V90Phase3Demodulator(V90Parameters *params, V90SpectralVerifier *unused,
			     unsigned int sessionFlag,
			     V90AutoDigitalImpDetector *adid);

	/**
	 * @brief Destructor. Releases the heap-allocated SD and ANSam
	 * detectors; the embedded modulator and descrambler subobjects are
	 * destroyed by the compiler-generated tail.
	 */
	~V90Phase3Demodulator();

	/**
	 * @brief Put phase 3 receive back to a known state ahead of one of
	 * four starting points (WaitForSd, TRN1dKnownData, WaitForQTS, or an
	 * "irregular" state), resetting every subobject and storing the
	 * caller's session parameters.
	 * @param pcmType      Companding law in use.
	 * @param ucode        The u-law/A-law code the impairment detector is
	 *                     to study.
	 * @param state        The state to start in; selects which of the
	 *                     four openings runs.
	 * @param word2c       Forwarded to the embedded modulator's reset only
	 *                     from the TRN1dKnownData opening (every other
	 *                     opening passes it 0).
	 * @param jd           V.90 Jd helper, or null.
	 * @param jdV92        V.92 Jd helper, or null.
	 * @param dil          DIL descriptor used for `dilLength`.
	 * @param altRbs       Forwarded to the impairment detector's reset and
	 *                     not stored.
	 * @param short414     Stored as `short_414`.
	 * @param float418     Stored as `float_418`.
	 * @param timeoutBase  Long-timeout baseline the two decision functions
	 *                     compare against.
	 */
	void reset(PcmType pcmType, unsigned char ucode,
		   Phase3DemodulatorState state, unsigned int word2c,
		   V90Jd *jd, V92Jd *jdV92, tagV90DILdescriptor *dil,
		   short altRbs, short short414, float float418,
		   unsigned int timeoutBase);

	/*
	 * Declared for the record and deliberately not defined; their callees
	 * are not written, and defining one re-opens the link closure.
	 *
	 * The constructor and destructor are now written, and what unblocked
	 * them was `ANSamToneDetector` landing.  Declaring them makes the
	 * class non-trivial, which deletes the special members of a union
	 * holding one -- `t_v90p3dreset.cpp`'s `p3d_slot` already provides its
	 * own pair for exactly that reason, so nothing had to move.
	 *
	 * Argument 2 -- the `V90SpectralVerifier *` -- is never loaded.  The
	 * constructor reads 0x40 (`this`), 0x44, 0x4c and 0x50 off its frame
	 * and `0x48(%esp)` appears nowhere in the 365 bytes.  So it is
	 * accepted and dropped, no field holds it, and no test of this
	 * constructor can assert a placement for it.  `V90Demodulator` passes
	 * the address of its own embedded verifier, which is why the argument
	 * looks load-bearing from the caller's side and is not.  The parameter
	 * is named `unused` below and the definition casts it to void.
	 *
	 * A return type is not mangled, so every one below is spelled `void`
	 * for want of evidence rather than because the blob returns nothing.
	 * getV90Decision()/getV92Decision() and the four functions below them
	 * are the exceptions: their return types are typed from a caller's
	 * `cwtl`/`test` on the result, not guessed (see each function's own
	 * comment, and docs/v90p3ddecision.md for the two decision functions,
	 * reconstructed independently and landed in agreement).
	 */
	/**
	 * @brief The V.90 phase 3 receive state machine: consume one sample
	 * and drive the appropriate one of its ~34 states (Sd detection, TRN1
	 * training and data-directed tracking, DIL, impairment probing, and
	 * exit to phase 4).
	 *
	 * This and getV92Decision() are two large, independently
	 * reconstructed state machines over the same object; see
	 * `V90Phase3Demodulator.cpp`'s file comment for the idioms they share.
	 * @param sample  The next demodulated sample.
	 * @return An event/decision code for this sample (widened from a
	 *         16-bit result the object computes).
	 */
	short getV90Decision(float sample);

	/**
	 * @brief The V.92 phase 3 receive state machine -- getV90Decision()'s
	 * counterpart when `sessionFlag` selects a V.92 session (adds the
	 * QTS/quick-connect states V.90 does not have).
	 * @param sample  The next demodulated sample.
	 * @return An event/decision code for this sample (widened from a
	 *         16-bit result the object computes).
	 */
	short getV92Decision(float sample);

	/**
	 * @brief Dispatch to getV92Decision() or getV90Decision() by
	 * `sessionFlag`.
	 * @param sample  The next demodulated sample.
	 * @return The chosen decision function's result, widened to `int`.
	 */
	int getDecision(float sample);

	/**
	 * @brief Slice one sample against the (possibly alternate) linear
	 * mapping table, differentially decode and descramble the resulting
	 * bit.
	 *
	 * Picks between the normal and alternate linear-mapping table
	 * (`isAltRbs`), negates the level for a negative sample (truncated to
	 * 16 bits to match the object), then runs the bit through the
	 * embedded differential decoder and descrambler.
	 * @param sample  The next demodulated sample.
	 * @param bit     Set to the decoded/descrambled bit (1 for a positive
	 *                sample, 0 otherwise, before the transforms).
	 * @return The signed mapping-table level for @p sample.
	 */
	int twoLevelDemod(float, int &);

	/**
	 * @brief Leave the DIL states, if currently in one, and check whether
	 * the embedded modulator has since terminated phase 3.
	 */
	void exitDIL();

	/**
	 * @brief Run-length detector for consecutive zero symbols: once more
	 * than 11 zero symbols have been seen, report a hit on the one frame
	 * position in 72 that is 12.
	 * @param symbol  The next symbol (0 extends the run, non-zero resets
	 *                it).
	 * @return 1 on a hit, else 0.
	 */
	int JdNotDetector(int symbol);

	/**
	 * @brief Clear `verificationStatus` (with a diagnostic). Called by
	 * `V90Demodulator::reInit` and `::enterChannelVerification`.
	 */
	void clearVerificationStatus();

	/**
	 * @brief Run the impairment detector's max-ucode / pad-gain pipeline:
	 * determineMaxUcode(), then findPadGain(), then
	 * applyPadGainToLinMapp(), in that order (later steps depend on the
	 * earlier ones' output).
	 */
	void setDigitalImairmentsInfo();

	/**
	 * @brief Idempotently enter the ANSam-energy-drop wait state, if not
	 * already in it (resets `word_2c` and prints the state-entry
	 * diagnostic only on the actual transition).
	 */
	void enterWaitForANSpcmDrop();

	/**
	 * @brief Advance `framePosition` through its 0..5 cycle, wrapping
	 * back to 0.
	 */
	void incrementFramePosition();

	/**
	 * @brief Copy the alternate mean-error-ratio threshold parameter into
	 * the live one used by the phase-4 mean-error update.
	 */
	void setAltRbsParams();

	/**
	 * @brief Zero the JdNot run-length counter (`jdNotRunLength`).
	 */
	void resetJdNotDetector();

	/**
	 * @brief Return the impairment detector's per-phase max-ucode table.
	 * @return Pointer to `autoDigitalImpDetector->maxUcode[0]`.
	 */
	unsigned char *getMaxUcode();

	/* --- data members; see the file comment on the naming --- */

	/*
	 * +0x000  The constructor's fourth argument, and `reset` hands it to
	 * `V90AutoDigitalImpDetector::reset` and `::resetLinearMapping`.
	 */
	V90AutoDigitalImpDetector *autoDigitalImpDetector;

	/*
	 * +0x004  The frame position within the current 6-position TRN1d
	 * cycle (0..5, wrapping), zeroed by `reset`. Named from the author's
	 * own diagnostic in `getV92Decision` ("waitForJd framePosition = %d")
	 * (finding F10139). Read at three different widths across the object
	 * (32-bit index/argument, sign-extended `short`, zero-extended row
	 * index into `linMapp`/`linMappAlt`), each forced by its use site, so
	 * the reconstruction casts explicitly at every one rather than letting
	 * the compiler choose.
	 */
	unsigned int framePosition;

	/*
	 * +0x008  The constructor's third argument and what `setSessionFlag`
	 * stores.  `reset` prints it, which is where the name comes from.
	 */
	unsigned int sessionFlag;

	/* +0x00c  The constructor's first argument.  `reset` never reads it. */
	V90Parameters *params;

	/*
	 * +0x010  `reset`'s first argument, and it is re-read out of the
	 * object rather than out of the argument for both the
	 * `V90AutoDigitalImpDetector::reset` call and the
	 * `calculateDilLength` call.
	 */
	PcmType pcmType;

	/*
	 * +0x014  `reset`'s last argument.  THIS COMMENT USED TO SAY "stored
	 * and not otherwise used", and that was true only until the decision
	 * functions landed: both `getV90Decision` and `getV92Decision`
	 * compare `word_2c` against this field plus a float constant to fire
	 * a long timeout -- `+ 12000.0f` for "WaitForSd TimeOut" and
	 * `+ 38760.0f` for "JdDemod TimeOut"/"V92JdDemod TimeOut" -- which is
	 * exactly the role `V90Phase3Modulator::timeoutBase` plays for ITS
	 * two long timeouts (`+ 24804` and `+ 40000`) in the paired class.
	 * Named by that structural symmetry (usage inference, strengthened by
	 * the sibling class already using this exact name for this exact
	 * role) rather than by any format string or typed callee of its own.
	 */
	unsigned int timeoutBase;

	/*
	 * +0x018  `reset`'s second argument, the u-law or A-law code the
	 * detector is to study.  Named as in V90AutoDigitalImpDetector.h,
	 * which is where `reset` passes it.
	 */
	unsigned char ucode;

	/*
	 * +0x019 WAS `pad_19[1]`, REMOVED (2026-09-04, pad-audit).  An
	 * `unsigned char` ending at +0x019 followed by the 2-byte-aligned
	 * `ucodeLevel` at +0x01a below needs exactly this 1-byte gap for
	 * natural alignment, no dis.py reader/writer touches +0x019 anywhere
	 * in the object, and `P3D_OFF(ucodeLevel, 0x01a, ...)` in the .cpp
	 * already asserts the next field's offset -- so the compiler's own
	 * padding reproduces the member being deleted.
	 */

	/*
	 * +0x01a  The linear level of `ucode`.  `reset` computes it as
	 * `ulaw2linear((ucode & 0x7f) ^ 0xff)` for mu-law and
	 * `alaw2linear((ucode & 0x7f) ^ 0xd5)` for A-law -- the same
	 * complement-then-decode the rest of the object uses.
	 */
	short ucodeLevel;

	/* +0x01c  `reset`'s seventh argument; `calculateDilLength` reads it. */
	tagV90DILdescriptor *dil;

	/* +0x020  `reset`'s fifth argument; `unPackReset`ed when non-null. */
	V90Jd *jd;

	/*
	 * +0x024  `reset`'s sixth argument.  When non-null BOTH
	 * `unPackJdReset` and `unPackJdPhaseReset` are called on it, and the
	 * second call re-loads the pointer out of the object rather than
	 * reusing the register.
	 */
	V92Jd *jdV92;

	/* +0x028  `reset`'s third argument, the state selector. */
	Phase3DemodulatorState state;

	/*
	 * +0x02c  `reset`'s fourth argument.  Stored here always, and passed
	 * on to `V90Phase3Modulator::reset` only from the TRN1dKnownData
	 * branch -- every other branch passes it 0.
	 */
	unsigned int word_2c;

	/*
	 * +0x030  Zeroed by `reset`, and on every entry to both decision
	 * functions -- `word_2c++; eventCode = 0; switch (state) { ... }` is
	 * the whole of each function's own opening, per
	 * `V90Phase3Demodulator.cpp`'s own comment on `getV92Decision`. Every
	 * arm that has news for the caller sets it to a small constant before
	 * returning (the state numbers the comment there catalogues,
	 * 1..0x3a), and nothing in this class or `V90Demodulator` reads it
	 * except as an equality test against one such constant --
	 * `V90Demodulator::exitPhase3` checks `phase3Demodulator->eventCode ==
	 * 0x14` for "Phase3 Terminated" before entering phase 4.
	 *
	 * Named by usage inference, strengthened by the paired class: this is
	 * the demodulator's own analogue of `V90Phase3Modulator::eventCode`
	 * at +0x01c, which CLAUDE.md's own evidence order ranks below a
	 * format string or a typed callee -- nothing in the object spells
	 * this field's name -- but the two fields play an identical role in
	 * the two halves of one state machine: cleared on entry, set by
	 * whichever transition has something to report, read by the caller
	 * and by nothing internal.
	 */
	unsigned int eventCode;

	/*
	 * +0x034  Embedded, not pointed at: `setSessionFlag` reaches it with
	 * `add $0x34,%eax` before a tail call and `reset` with
	 * `lea 0x34(%ebx),%ebp`, both address arithmetic rather than a load.
	 * 0x398 bytes, ending exactly at the field below.
	 */
	V90Phase3Modulator phase3Modulator;

	/*
	 * +0x3cc  Zeroed by the constructor AND by `reset`.
	 *
	 * This comment used to say "`reset` does not touch it", and that was
	 * wrong. `reset` has `movl $0x0,0x3cc(%ebx)` of its own, and since
	 * the constructor's last act is to call `reset`, the constructor's
	 * store is overwritten by an identical one microseconds later.  A
	 * mutation that deletes the constructor's store is therefore
	 * behaviourally invisible -- it was written, it read NOT CAUGHT, and
	 * that is how the error was found.  See test/mutations/v90p3dctor.json
	 * for why it is not in the suite.
	 *
	 * IT IS A MEM-INITIALIZER, by finding F1302's argument and this is the
	 * second instance of it.  `mov %ecx,0x3cc(%ebx)` with `%ecx` zero sits
	 * between the call to `V90Phase3Modulator`'s constructor and the call
	 * to `Descrambler<int,int>`'s, and a store to `this + 0x3cc` can be
	 * moved across neither.  Body statements run after every member
	 * construction, so it is not one; members are constructed in
	 * declaration order, so this field is declared between the two
	 * subobjects, which is where it sits.
	 *
	 * The period build corroborates it, and the corroboration
	 * discriminates. This paragraph used to say `make similarity` could
	 * not reach the argument because this translation unit "is one of the
	 * fifteen the period toolchain cannot compile at all".  That reason
	 * was already untrue when it was written (finding F2119), and the
	 * corroboration it was standing in for has now been taken (2151).
	 *
	 * Built with `tools/toolchain/build.sh`'s exact flag set, the period
	 * compiler puts the store in the object's position:
	 *
	 *     477  call  _ZN18V90Phase3ModulatorC1EP13V90Parametersj
	 *     47c  xor   %eax,%eax
	 *     47e  mov   %eax,0x3cc(%edi)          <-- word_3cc
	 *     484  movl  $0x17,0x1c(%ebx)          <-- descrambler, %ebx = this+0x3d0
	 *
	 * against the object's `call` at 0x212e0, `mov %ecx,0x3cc(%ebx)` at
	 * 0x212f1 and `call _ZN11DescramblerIiiEC1Ejjj` at 0x21311.
	 *
	 * **The alternative spelling is EXCLUDED, not merely unpreferred.**
	 * The same source with `word_3cc()` dropped from the ctor-init-list and
	 * `word_3cc.prev_ = 0;` written as the first body statement compiles to
	 * the same store at 0x4d6 -- AFTER the whole descrambler construction,
	 * its allocation and its clear loop -- so the two spellings are
	 * distinguishable in the emitted code and only the mem-initializer
	 * reproduces the object.  An ordering argument is evidence only if the
	 * other spelling orders differently, and here it does.
	 *
	 * One difference that is not a source difference: the object calls
	 * `Descrambler<int,int>`'s constructor and the period build of this
	 * source INLINES it.  What the argument uses is where the store sits
	 * relative to the START of the descrambler's construction, and that is
	 * the same in both.  Inlining is the compiler's to choose (CLAUDE.md's
	 * rule for reading a codegen difference), so it is recorded and not
	 * chased.
	 *
	 * This corroborates a spelling the tree already had; it is not a
	 * source change derived from statement order, which would need the
	 * full-text identity test that 617 sets.
	 *
	 * IT IS A `SerialDifferentialDecoder<int>`, WHICH THE SKETCH ABOVE
	 * could not see, and two independent reconstructions say so.  Neither
	 * could see the other's work; findings F2102 and F2110 are the same
	 * conclusion reached twice from opposite ends of the class.
	 *
	 *   `getV90Decision` calls `_ZN25SerialDifferentialDecoderIiE7processEi`
	 *   on `this + 0x3cc` at 0x23f6b, 0x2449e, 0x24596 and 0x24649, and
	 *   `twoLevelDemod` at 0x215fa does the same.
	 *
	 *   `getV92Decision` passes `this + 0x3cc` as the `this` of the same
	 *   method five times over -- `lea 0x3cc(%ebx),%ebp` then the `call`.
	 *
	 * The template has one `T` member and no constructor by design -- see
	 * DiffCoder.h, whose own comment already said the `int` pair belonged
	 * to this class and records the ten calls -- so +0x3cc is a SUBOBJECT
	 * with one `int` in it, the size is unchanged at four bytes, every
	 * offset below still holds, and the mem-initializer argument above is
	 * strengthened rather than weakened: a trivial member value-initialised
	 * in the ctor-init-list is exactly the `mov %ecx,0x3cc(%ebx)` that sits
	 * between the two subobject constructions.  This field is the
	 * differential decoder that pairs with the `Descrambler<int,int>`
	 * immediately below it.  The NAME is left as it was, because nothing
	 * names it and a guess is worse than no name.
	 *
	 * While the two were in flight the declaration stayed `unsigned int`
	 * and `getV92Decision` cast at its call sites, so as not to move the
	 * ground under a function being written in another session; that side's
	 * comment said correcting the type "belongs to whoever lands second".
	 * Both have landed.  The member has its real type, the casts are gone,
	 * and both functions now call `word_3cc.process(...)` directly.
	 *
	 * And the spelling was measured, not assumed.  `word_3cc(0)` on an
	 * `unsigned int` obviously emits the store; `word_3cc()` on a class
	 * type is C++98 DEFAULT-initialization, which TC1 changed to
	 * value-initialization, so whether GCC 3.4.2 still zeroes it is a
	 * question about that compiler and not about the standard.  No test
	 * here can answer it -- the constructor's last act is `reset`, which
	 * writes the same zero, which is why test/mutations/v90p3dctor.json
	 * records the store as behaviourally invisible.  The period build was
	 * disassembled instead: `xor %eax,%eax; mov %eax,0x3cc(%edi)` sits
	 * between the call to `V90Phase3Modulator`'s constructor and the
	 * `Descrambler` construction, exactly where the object puts it.  That
	 * measurement is re-taken and extended above, where the body spelling
	 * is shown to put the same store somewhere else -- so this answers the
	 * value-initialization question AND the mem-initializer one.
	 */
	SerialDifferentialDecoder<int> word_3cc;

	/*
	 * +0x3d0  EMBEDDED, 0x20 bytes, ending exactly at the field below.
	 * The constructor builds it with (0x12, 0x17, 0x63) and `reset` calls
	 * `reset(0)` on it -- always 0, never a value derived from an
	 * argument.
	 */
	Descrambler<int, int> descrambler;

	/*
	 * +0x3f0  Allocated by the constructor with `sysdep_malloc(0x1c)`,
	 * which is exactly `sizeof(V90SdDetector)`.
	 */
	V90SdDetector *sdDetector;

	/*
	 * +0x3f4  A 32-bit word, and the comment here used to say nothing
	 * reaches it -- it was the head of `pad_3f4[5]`.  BOTH decision
	 * functions write it, independently reconstructed and agreeing to the
	 * parameter: as each enters its TRN1d data-directed state it stores
	 * `params->unnamed_4a4` when `quickConnect` is set (0x24094, 0x2410b
	 * in `getV90Decision`) and `params->unnamed_344` when it is not
	 * (0x25749), always in the same breath as +0x420 below.  Nothing in
	 * either function -- 17 KB between them -- READS it, so its width and
	 * its source are settled and what it is FOR is not.  Finding F2118.
	 */
	unsigned int word_3f4;		/* +0x3f4                        */

	/*
	 * +0x3f8  IT IS A FIELD NOW, and this comment used to say nothing
	 * reaches it.  `setDigitalImairmentsInfo` reads it with
	 * `movzbl 0x3f8(%ebx),%eax` and hands it to
	 * `V90AutoDigitalImpDetector::determineMaxUcode(short)` as that
	 * method's only argument -- so it is ONE BYTE, unsigned, and widened
	 * rather than sign-extended.
	 *
	 * The name is deliberately not `maxCode`.  That is the name this tree
	 * gave `determineMaxUcode`'s parameter when it reconstructed it; the
	 * mangling carries `s` and no name, so calling the field after it would
	 * be promoting our own invention into a second place.  Nothing writes
	 * this byte in anything written so far and no format string prints it,
	 * which leaves usage inference alone -- CLAUDE.md's weakest tier, and
	 * not enough.
	 */
	unsigned char byte_3f8;

	/* +0x3f9  Zeroed by `reset`. */
	unsigned char byte_3f9;

	/*
	 * +0x3fa WAS `pad_3fa[2]`, REMOVED (2026-09-04, pad-audit).  An
	 * `unsigned char` ending at +0x3fa followed by the 4-byte-aligned
	 * `word_3fc` below needs exactly this 2-byte gap for natural
	 * alignment, no dis.py reader/writer touches +0x3fa/+0x3fb anywhere in
	 * the object, and `P3D_OFF(word_3fc, 0x3fc, ...)` in the .cpp already
	 * asserts the next field's offset -- so the compiler's own padding
	 * reproduces the member being deleted.
	 */

	unsigned int word_3fc;		/* +0x3fc zeroed by `reset`      */
	short short_400;		/* +0x400 zeroed by `reset`      */

	/*
	 * +0x402 WAS `pad_402[2]`, REMOVED (2026-09-04, pad-audit).  A `short`
	 * ending at +0x402 followed by the 4-byte-aligned `jdNotRunLength` at
	 * +0x404 below needs exactly this 2-byte gap for natural alignment, no
	 * dis.py reader/writer touches +0x402/+0x403 anywhere in the object,
	 * and `P3D_OFF(jdNotRunLength, 0x404, ...)` in the .cpp already
	 * asserts the next field's offset -- so the compiler's own padding
	 * reproduces the member being deleted.
	 */

	/*
	 * +0x404  Named (wave 3, F10139) by a typed callee and by the
	 * object's own method name. `JdNotDetector(int symbol)` -- the one
	 * member of this class whose whole body is this field -- increments
	 * it on a zero symbol and clears it otherwise, then answers yes once
	 * it exceeds 11 on the one frame position in 72 that is 12; both
	 * `getV90Decision` and `getV92Decision` inline the identical shape at
	 * their own JdNot arm rather than calling it (three copies of one
	 * counter, not three different fields). `resetJdNotDetector` -- a
	 * name this tree already gave the method that zeroes it -- is the
	 * fourth site naming the same role.  Unlike +0x408 just below, every
	 * site that touches this field does so for the SAME purpose, which is
	 * what makes the rename safe here and not there.
	 */
	unsigned int jdNotRunLength;	/* +0x404                        */

	/*
	 * +0x408  Zeroed by `reset`, and NOT renamed despite one strong-
	 * looking lead: state 5 (study reference Ucode) stores
	 * `V90AutoDigitalImpDetector::studyUrefHandler`'s return value here
	 * and later tests it `== 2` for "the reference study is done" -- a
	 * real, typed-callee-backed role. But roughly twenty OTHER sites
	 * across the DIL and probing states store a bare 0 or 1 into this
	 * same field with no relation to that call, which is the overloaded-
	 * scratch pattern CLAUDE.md's naming section warns about: a single
	 * name here would be right for one arm and wrong-but-plausible for
	 * the rest, and the rule is that a wrong name is worse than a padded
	 * one. Left as `word_408`.
	 */
	unsigned int word_408;		/* +0x408 zeroed by `reset`      */

	/*
	 * +0x40c  `calculateDilLength(dil, pcmType)`, computed last of all and
	 * from the two fields above rather than from the arguments.
	 */
	unsigned int dilLength;

	/*
	 * +0x410  Zeroed by `reset`, and then immediately overwritten by
	 * `V90Demodulator::enterPhase3` with its own +0x294 -- which is the
	 * one place in wave 2 where the caller's write ordering is observable.
	 *
	 * Named (wave 3, F10139) by the caller's own, already-established
	 * field, which is CLAUDE.md's rule-2 tier: `V90Demodulator`'s own
	 * +0x294 is `quickConnect`, named in that header from two independent
	 * derivations (`V90Demodulator::reset`'s own parameter name, and the
	 * format string that names `V90Phase4Demodulator::quickConnect`), and
	 * `V90Demodulator::enterPhase3` copies it here with
	 * `phase3Demodulator->quickConnect = quickConnect;` straight after
	 * `V90Phase3Demodulator::reset` has zeroed this field -- see that
	 * header's own +0x294 comment, which already named this copy before
	 * the rename landed. It is also the flag `resetStudyUrefHandler`
	 * receives as its `qc` argument and the one this class's own +0x3f4
	 * and +0x420 switch their parameter source on (both above and
	 * below), so every QC-suffixed-vs-plain parameter choice in this
	 * class traces back to the same caller-named field.
	 */
	unsigned int quickConnect;

	/* +0x414  `reset`'s ninth argument, stored as a 16-bit quantity. */
	short short_414;

	/*
	 * +0x416 WAS `pad_416[2]`, REMOVED (2026-09-04, pad-audit).  A `short`
	 * ending at +0x416 followed by the 4-byte-aligned `float_418` below
	 * needs exactly this 2-byte gap for natural alignment, no dis.py
	 * reader/writer touches +0x416/+0x417 anywhere in the object, and
	 * `P3D_OFF(float_418, 0x418, ...)` in the .cpp already asserts the
	 * next field's offset -- so the compiler's own padding reproduces the
	 * member being deleted.
	 */

	/*
	 * +0x418  `reset`'s tenth argument.  Copied as a 32-bit word and
	 * never loaded into the x87 stack, so the reconstruction must copy it
	 * as a `float` and not convert it.
	 */
	float float_418;

	/*
	 * +0x41c  The verification status.  `clearVerificationStatus()` is
	 * nothing but a gated diagnostic and `movl $0x0` here, so the field is
	 * named for the member that clears it and what it HOLDS is not
	 * established by anything written so far.  A full 32-bit store.
	 */
	unsigned int verificationStatus;

	/*
	 * +0x420  The length of the TRN1d data-directed stage, and likewise no
	 * longer padding.  BOTH decision functions load it from
	 * `params->TRN1_QC_DD_LENGTH` (+0x4a0) when `quickConnect` is set and
	 * from `params->TRN1D_DD_LENGTH` (+0x2fc) when it is not, and then COMPARE
	 * the counter `word_2c` against it to decide when to leave -- state 4
	 * in `getV90Decision`, state 5 in `getV92Decision`.  So unlike +0x3f4
	 * this one is both written and read, and it holds a length.  The two
	 * reconstructions disagree only on the UNIT, one saying samples and the
	 * other symbols; nothing here settles which, and at 8 kHz on a
	 * one-sample-in one-decision-out function they are the same count.
	 * Finding F2118.
	 */
	unsigned int word_420;		/* +0x420                        */

	/* +0x424  Zeroed by `reset`, before anything else it does. */
	unsigned char byte_424;

	/*
	 * +0x425 WAS `pad_425[3]`, REMOVED (2026-09-04, pad-audit).  An
	 * `unsigned char` ending at +0x425 followed by the 4-byte-aligned
	 * pointer at +0x428 below needs exactly this 3-byte gap for natural
	 * alignment, no dis.py reader/writer touches
	 * +0x425/+0x426/+0x427 anywhere in the object, and
	 * `P3D_OFF(ansamToneDetector, 0x428, ...)` in the .cpp already asserts
	 * the next field's offset -- so the compiler's own padding reproduces
	 * the member being deleted.
	 */

	/*
	 * +0x428  Allocated by the constructor with `sysdep_malloc(0x3c)` --
	 * which is `sizeof(ANSamToneDetector)` exactly -- and built with
	 * (0x190, 0x64, 307200.0f, 0, 0.5f, 8000, 50, 99).  OWNED: the
	 * destructor destroys it and frees it.  It is what makes the object
	 * 0x42c.
	 *
	 * The type is a pointer to an INCOMPLETE class here on purpose.  A
	 * pointer member needs only the declaration, and pulling
	 * `ANSamToneDetector.h` in would drag `GenericToneDetector.h` into
	 * every one of the six translation units that include this header.
	 * `V90Phase3Demodulator.cpp` includes the definition, which is all the
	 * destructor's call needs.
	 */
	ANSamToneDetector *ansamToneDetector;
};

/*
 * `reset`'s eighth argument is a `short` and its caller narrows an `int` to
 * reach it, so the width is load-bearing rather than cosmetic; see
 * V90Demodulator.h.  It is stored nowhere -- `reset` forwards it to
 * `V90AutoDigitalImpDetector::reset` and drops it.
 */

#endif /* DSPLIB_V90PHASE3DEMODULATOR_H */
