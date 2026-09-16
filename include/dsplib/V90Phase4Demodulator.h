/**
 * @file V90Phase4Demodulator.h
 * @brief The V.90 phase 4 receiver, so far as its construction path settles
 *        it.
 *
 * The class had no header and no .cpp in this tree before this file.
 * `.symtab` carries `V90Phase4Demodulator.cpp` as a FILE entry (index 231),
 * so the original had a translation unit of its own and this file's
 * placement follows the original's rather than guessing at one -- the same
 * evidence `V90Demapper.h` cites for its own.
 *
 * The object is 0x351c = 13,596 bytes, from the allocation rather than a
 * displacement scan (finding F1107's rule). `V90Demodulator::V90Demodulator`
 * has
 *
 *     1c8fe:  c7 04 24 1c 35 00 00   movl  $0x351c,(%esp)
 *     1c905:  e8 ..                  call  sysdep_malloc
 *     1c90a:  ...
 *     1c969:  e8 ..                  call  V90Phase4Demodulator::
 *                                             V90Phase4Demodulator(...)
 *     1c96e:  89 b3 e0 01 00 00      mov   %esi,0x1e0(%ebx)
 *
 * and the same sequence again in its C2 twin. The highest offset the
 * constructor writes is +0x3514, so the last eight bytes are bounded by the
 * allocation alone -- which is the case the rule exists for.
 *
 * Not polymorphic: `nm` gives `D1` at 0x25b50 and `D2` at 0x25b10 and no
 * `D0`; GCC emits a deleting destructor only for a virtual class, so offset 0
 * is a real member and there is no vptr (finding F228's technique).
 *
 * Three exact meetings fix the three embedded subobjects. The constructor
 * builds three members in place and the destructor destroys the same three
 * in reverse, so their bases are read and not guessed:
 *
 *     +0x0050  V90Phase4Modulator   sizeof 0x2fac   ends 0x2ffc
 *     +0x2ffc  V90RDetector         sizeof 0x002c   ends 0x3028
 *     +0x3028  V90RDetector         sizeof 0x002c   ends 0x3054
 *
 * Each end is the next base exactly, and 0x3054 is where the first of the
 * four trailing pointers lives. Three independent bases and three exact
 * meetings, and `V90Phase4Demodulator.cpp` asserts all of them with
 * `__builtin_offsetof` rather than restating them in a comment -- so if
 * either subobject's own size ever moves, the compiler says so.
 *
 * The two mapping-parameter pointers reach the embedded modulator swapped.
 * It is built with
 *
 *     V90Phase4Modulator(params, mode, NULL, NULL,
 *                        mappingParams2, mappingParams1, NULL, 0xc)
 *
 * -- argument 2 into the modulator's fifth slot and argument 1 into its
 * sixth. In the object that is `mov 0x48(%esp),%edx` reaching
 * `mov %edx,0x14(%esp)` and `mov 0x44(%esp),%ebp` reaching
 * `mov %ebp,0x18(%esp)`, two loads and two stores with nothing between them
 * that could have reordered a pair of independent stack slots by accident.
 * The demodulator stores them the other way round in its own object
 * (+0x0c gets argument 1, +0x10 argument 2), so this really is a crossing
 * and not a misreading of which is which. A test that passed the same
 * pointer twice could not see this at all, which is why the two must be
 * distinguishable.
 *
 * The three member constructions are the compiler's, and their order proves
 * the declaration order: the constructor calls `V90Phase4Modulator`'s at
 * +0x50, then `V90RDetector`'s at +0x2ffc, then `V90RDetector`'s at +0x3028;
 * the destructor calls them at +0x3028, +0x2ffc and +0x50. Exactly reversed,
 * and no body statement of either sits between any pair -- which is what a
 * compiler emits for three members in declaration order and nothing else.
 * So the .cpp writes none of the six calls: it writes the mem-initializer
 * list, and the twelve stores that are the body. Both member constructions
 * of `V90RDetector` take the same argument, so the two detectors are told
 * apart by their offsets alone and by nothing in the source -- `rDetector1`
 * and `rDetector2` name positions, not roles.
 *
 * All four symbols match the blob's instruction sequence: `make similarity`
 * lists `C1`, `C2`, `D1` and `D2` -- 225, 225, 52 and 52 bytes -- among the
 * identical mnemonic sequences. That is a second, independent tier agreeing
 * with the differential one, and it is worth having on a function whose
 * whole body is eleven stores: `compare.py` compares mnemonics and not
 * operands, so it says nothing about which field each store reached, and
 * the test's eleven placement assertions say nothing about the instruction
 * the compiler chose. Neither alone is the claim; together they are close
 * to it.
 */

#ifndef DSPLIB_V90PHASE4DEMODULATOR_H
#define DSPLIB_V90PHASE4DEMODULATOR_H

#include "dsplib/Scrambler.h"
#include "dsplib/V90Phase4Modulator.h"
#include "dsplib/V90RDetector.h"

/*
 * Pointers only, so forward declarations are what belong here. Two
 * incompatible definitions of `V90Parameters` exist in this tree and no
 * translation unit may include both (finding F1112), so the class is
 * declared and never defined here.
 */
class V90Parameters;
class V90MappingParams;
class V90Demapper;
class V90CP;
class V90MP;
class V90ConnectionEvaluator;
class V90Phase3Demodulator;
class V90AutoDigitalImpDetector;

/*
 * The enum is the mangling's, not an invention. `V90Phase4Demodulator::reset`
 * is `_ZN20V90Phase4Demodulator5resetEh22Phase4DemodulatorStatejj`, so a type
 * spelled exactly `Phase4DemodulatorState` exists at namespace scope and is
 * that member's second parameter -- and `reset` stores that parameter, and
 * nothing else, into +0x20:
 *
 *     277ef:  8b 5c 24 38   mov 0x38(%esp),%ebx    ; argument 2
 *     27801:  89 5e 20      mov %ebx,0x20(%esi)
 *
 * with `this` at 0x30(%esp) and the four arguments at 0x34, 0x38, 0x3c and
 * 0x40. So the field below is that enum and not an `unsigned int` that
 * happens to hold the same numbers. Same shape as `Phase3DemodulatorState`
 * in V90Phase3Demodulator.h, and named the same way.
 *
 * Four of the five enumerators are the author's own words -- the `edprintf`
 * format string at the only site that stores each value:
 *
 *     4     "V90Phase4Demodulator: enter WaitForV90CP state @ %d\r\n"
 *     5     "V90Phase4Demodulator: enter WaitForMP state @ %d\r\n"
 *     6     "V90Phase4Demodulator: enter WaitForEd state @ %d\r\n"
 *     0x10  "V90Phase4Demodulator: enter FPE !"
 *
 * The fifth is usage inference, and is the weakest thing in this header.
 * `detectRRN` is the only writer of 9 and it writes it immediately after
 * printing "Rd detected"; the original's own name for the state is not
 * recoverable, so `P4D_STATE_RD_DETECTED` says what the object does and
 * claims nothing more.
 *
 * The gaps are now filled, by `getV90Decision` and `getV92Decision`, whose
 * switch is over the whole 0..0x11 range and whose arms carry the entry
 * messages for eleven more. Eight of those eleven are the author's own words
 * again -- the message printed on the transition into the state, at the only
 * site that stores it:
 *
 *     2     "RiNot detected @ %d, enter TRN2dKnownData state"
 *     3     "RdNot detected @ %d, enter TRN2d DD state"
 *     7     "Ed detected @ %d, enter B1d state"
 *     8     "Phase4 Terminated @ %d, ..."
 *     0xa   "First Ed at RRN detected @ %d, Silence state"
 *     0xb   "entering CalcErrorEnergyBeforeEchoCancellation state @ %d"
 *     0xc   "entering WaitForEchoCancellation state @ %d"
 *     0xd   "entering CalcErrorEnergyAfterEchoCancellation state @ %d"
 *     0xe   "entering WaitForRt state @ %d"
 *
 * The remaining three are the same construction applied to the same shape
 * of arm, which is weaker than a string but stronger than a guess. State 0xe
 * is called `WaitForRt` by the author, and its arm is exactly "run
 * `rDetector1.detectR`, and on success report `Rt detected` and move on".
 * States 0, 1 and 0xf have that arm with a different detector call and a
 * different message -- `detectR`/"Ri detected", `detectRNot`/"RiNot detected",
 * `detectRNot`/"RtNot detected" -- so `WAIT_FOR_RI`, `WAIT_FOR_RI_NOT` and
 * `WAIT_FOR_RT_NOT` name what the arm waits for, in the author's own scheme
 * (finding F4810).
 *
 * 0x11 is deliberately unnamed. Both functions send it to the same body as
 * state 8 -- return zero, touch nothing -- and no message anywhere in the
 * object stores or reports it, so what distinguishes it from 8 is not
 * recoverable. The switch writes the literal; naming it would be inventing
 * the distinction. Same ruling as `V90MP`'s, and finding 3120's.
 *
 * State 4 has two of the author's names, one per function, and that is the
 * object's rather than a slip here: `getV90Decision` reaches it through
 * `enterWaitForCP`'s "enter WaitForV90CP state" and `getV92Decision` through
 * "RfNot detected @ %d, enter WaitForCPu state". The V.90 spelling is kept
 * because it was here first.
 *
 * Spelled as a pin rather than a base for the reason `Phase3DemodulatorState`
 * gives: `: int` is C++11 and the author's compiler was C++98, where these
 * five values alone would give the enum a range of 0..31 and make a
 * differential sweep of a garbage-seeded +0x20 undefined. `_BASE_PIN` is
 * ours; see docs/method/compilers.md, V2.
 */
enum Phase4DemodulatorState {
	P4D_STATE_WAIT_FOR_RI = 0,	/* arm shape; see above      */
	P4D_STATE_WAIT_FOR_RI_NOT = 1,	/* arm shape; see above      */
	P4D_STATE_TRN2D_KNOWN_DATA = 2,	/* "enter TRN2dKnownData"    */
	P4D_STATE_TRN2D_DD = 3,		/* "enter TRN2d DD state"    */
	P4D_STATE_WAIT_FOR_V90CP = 4,	/* enterWaitForCP            */
	P4D_STATE_WAIT_FOR_MP = 5,	/* enterWaitForMP            */
	P4D_STATE_WAIT_FOR_ED = 6,	/* enterWaitForEd            */
	P4D_STATE_B1D = 7,		/* "enter B1d state"         */
	P4D_STATE_TERMINATED = 8,	/* "Phase4 Terminated @ %d"  */
	P4D_STATE_RD_DETECTED = 9,	/* detectRRN; inferred       */
	P4D_STATE_SILENCE = 0xa,	/* "Silence state"           */
	P4D_STATE_CALC_ENERGY_BEFORE_EC = 0xb,	/* "entering Calc..."*/
	P4D_STATE_WAIT_FOR_ECHO_CANCEL = 0xc,	/* "entering WaitFor"*/
	P4D_STATE_CALC_ENERGY_AFTER_EC = 0xd,	/* "entering Calc..."*/
	P4D_STATE_WAIT_FOR_RT = 0xe,	/* "entering WaitForRt"      */
	P4D_STATE_WAIT_FOR_RT_NOT = 0xf,/* arm shape; see above      */
	P4D_STATE_FPE = 0x10,		/* detectFPE                 */
	/*
	 * Modelled, unnamed, and the name says so on purpose. Both decision
	 * members carry a case label for 0x11 -- GCC sizes a jump table by
	 * the case range, and both tables are eighteen entries under
	 * `cmp $0x11` -- so the value exists in the original's source. What
	 * it means is not recoverable: no store anywhere in the object puts
	 * it in +0x20, no message reports it, and its arm is the same three
	 * instructions as `TERMINATED`'s. `P4D_STATE_11` would be an offset
	 * wearing a name; this spelling claims exactly what is known.
	 */
	P4D_STATE_UNNAMED_11 = 0x11,
	P4D_STATE_BASE_PIN = -0x7fffffff - 1	/* ours: pins the base */
};

class V90Phase4Demodulator {
public:
	/* Store the session flag and forward it to the embedded modulator. */
	void setSessionFlag(unsigned int flag);

	/**
	 * @brief Construct the phase 4 demodulator: store the eleven
	 *        constructor arguments and build the embedded modulator and
	 *        the two `V90RDetector`s in place.
	 *
	 * The embedded `V90Phase4Modulator` is built with `mappingParams1`
	 * and `mappingParams2` swapped relative to this class's own field
	 * order -- see the file comment.
	 *
	 * The parameter list is the mangling's and not a choice: an `int`
	 * where the original had `unsigned` emits a different symbol that
	 * links against nothing. C1 at 0x25a20 and C2 at 0x25930, 225 bytes
	 * each; D1 at 0x25b50 and D2 at 0x25b10, 52 bytes each.
	 * @param mappingParams1  Stored as-is; reaches the embedded
	 *                         modulator's sixth constructor argument.
	 * @param mappingParams2  Stored as-is; reaches the embedded
	 *                         modulator's fifth constructor argument.
	 * @param demapper  Not owned.
	 * @param cp  Not owned.
	 * @param mp  Not owned.
	 * @param descrambler  Not owned.
	 * @param connectionEvaluator  Not owned.
	 * @param params  Not owned; shared with the embedded modulator and
	 *                 both `V90RDetector`s.
	 * @param phase3Demodulator  Not owned.
	 * @param autoDigitalImpDetector  Not owned.
	 * @param sessionFlag  V.90 (0) versus V.92 (non-zero); selects
	 *                      between `getV90Decision` and `getV92Decision`
	 *                      in `getDecision`.
	 */
	V90Phase4Demodulator(V90MappingParams *mappingParams1,
			     V90MappingParams *mappingParams2,
			     V90Demapper *demapper, V90CP *cp, V90MP *mp,
			     Descrambler<unsigned char, int> *descrambler,
			     V90ConnectionEvaluator *connectionEvaluator,
			     V90Parameters *params,
			     V90Phase3Demodulator *phase3Demodulator,
			     V90AutoDigitalImpDetector *autoDigitalImpDetector,
			     unsigned int sessionFlag);
	/**
	 * @brief Empty body; destroys the embedded modulator and the two
	 *        detectors and frees nothing else, since this class owns no
	 *        heap memory.
	 */
	~V90Phase4Demodulator();

	/*
	 * The seven leaves, all in src/pump/v90/V90Phase4Demodulator.cpp.
	 * Between them they are the whole state-entry surface of the class:
	 * everything that stores `state` except `reset` and the two decision
	 * members.
	 *
	 * `detectRRN` and `detectFPE` return `int` and not `bool` -- the blob
	 * builds the answer in a full 32-bit register (`xor %edx,%edx` /
	 * `mov $0x1,%edx` / `mov %edx,%eax`) rather than in `%al`. Neither
	 * return type reaches the mangling, so this is the only evidence
	 * there is for it.
	 */
	/** @brief Reseed both `V90RDetector`s for a fresh RRN search. */
	void resetRRNDetector();
	/**
	 * @brief Reset the five scalars `detectRRN` and the RRN state
	 *        machine share, ahead of an RRN search.
	 *
	 * Five stores and nothing else, in the object's own order (+0x38,
	 * +0x3c, +0x44, +0x48, then the byte at +0x30 last).
	 */
	void resetBeforRRN();
	/** @brief Enter #P4D_STATE_WAIT_FOR_V90CP, logging the outgoing
	 *         state's duration first. */
	void enterWaitForCP();
	/** @brief Enter #P4D_STATE_WAIT_FOR_MP, logging the outgoing state's
	 *         duration first. */
	void enterWaitForMP();
	/** @brief Enter #P4D_STATE_WAIT_FOR_ED, logging the outgoing state's
	 *         duration first. */
	void enterWaitForEd();
	/**
	 * @brief Feed one sample to `rDetector1`'s Rd search; on detection,
	 *        move to #P4D_STATE_RD_DETECTED and log the polarity.
	 * @param sample  The next receive sample.
	 * @return Non-zero once Rd is detected, zero otherwise.
	 */
	int detectRRN(short sample);
	/**
	 * @brief Feed one sample to `rDetector2`'s Rf search; on detection,
	 *        move to #P4D_STATE_FPE.
	 *
	 * The polarity it logs is `rDetector1`'s, not `rDetector2`'s -- the
	 * object's own copy/paste slip from `detectRRN` (finding F4320).
	 * @param sample  The next receive sample.
	 * @return Non-zero once Rf is detected, zero otherwise.
	 */
	int detectFPE(short sample);

	/*
	 * The three decision members. `getDecision` is the switch on
	 * `sessionFlag` and nothing else; the other two are one arm each of
	 * it, and between them they are the whole phase 4 state machine.
	 *
	 * The return types are read off `getDecision`, which is the only
	 * place they show. A return type is not mangled, so the evidence is
	 * `cwtl` -- sign-extend %ax into %eax -- on the result of each of the
	 * two calls, at 0x2779e and 0x277af. A caller only widens what the
	 * callee left narrow, so the two arms return `short`; and a caller
	 * that widens is one whose own result is `int`, so `getDecision`
	 * does. Nothing else in the object separates the three.
	 *
	 * `setSessionFlag` is declared above. `trn2dKnownDemod` headed
	 * that list -- with a stale note blaming `V90SpectralShaper`, which
	 * was never its dependency -- and is written now (the VPcmV34Main
	 * leaf pass); `reset` moved below earlier.
	 */
	/**
	 * @brief Dispatch to `getV92Decision` when `sessionFlag` is set,
	 *        `getV90Decision` otherwise.
	 * @param sample  The next receive sample.
	 * @return Whatever the selected decision member returns.
	 */
	int getDecision(short sample);
	/**
	 * @brief The V.90 phase 4 state machine: advance `state` for one
	 *        incoming sample and act on the transition.
	 * @param sample  The next receive sample.
	 * @return The state machine's per-sample decision value.
	 */
	short getV90Decision(short sample);
	/**
	 * @brief The V.92 phase 4 state machine: advance `state` for one
	 *        incoming sample and act on the transition.
	 * @param sample  The next receive sample.
	 * @return The state machine's per-sample decision value.
	 */
	short getV92Decision(short sample);

	/**
	 * @brief Re-derive the known TRN2d symbol from the embedded
	 *        modulator's generator and the impairment detector's
	 *        `linMapp` table.
	 *
	 * Runs the embedded modulator's generator one symbol forward, splits
	 * the result into sign and magnitude, companding-encodes the
	 * magnitude with the law currently in force, and looks the learned
	 * level up in `linMapp` by (phase, code). The `short` argument is
	 * never read.
	 * @param unused Ignored; the known symbol comes from the local modulator.
	 * @return The signed, learned symbol level.
	 */
	int trn2dKnownDemod(short unused);

	/**
	 * @brief The whole receiver's entry point: restore the eleven state
	 *        scalars, reseed both `V90RDetector`s, reset whichever of
	 *        the CP and the MP the session uses, hand the mapping block
	 *        to the demapper and to the embedded modulator, then run the
	 *        session's decision member `nofSamples` times.
	 * @param code            The G.711 companding code the embedded
	 *                         phase 4 modulator is to send; stored in
	 *                         #ucode.
	 * @param st              The state to enter.
	 * @param nofSamples      Trip count for the decision-member loop;
	 *                         reaches no field.
	 * @param quickConnectArg Stored in #quickConnect, and selects between
	 *                         `TRN2D_QC_DD_LENGTH` and `TRN2D_DD_LENGTH`
	 *                         for #trn2dDDLength.
	 */
	void reset(unsigned char code, Phase4DemodulatorState st,
		   unsigned int nofSamples, unsigned int quickConnectArg);

	/*
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable from the mangling, and a
	 * single access section is what lets the .cpp assert every offset
	 * below with `__builtin_offsetof`.
	 */

	/*
	 * +0x0000  The constructor's eleventh argument. `V90Demodulator`
	 * passes its own +0x30 -- the field its `setSessionFlag` writes -- and
	 * hands the same value to `V90Phase3Demodulator`'s third argument,
	 * which that class's header already calls `sessionFlag`. The name
	 * follows the value, not this constructor.
	 */
	unsigned int sessionFlag;

	/* +0x0004  The constructor's eighth argument. */
	V90Parameters *params;

	/*
	 * +0x0008  The G.711 code the phase 4 modulator is to send. One
	 * byte, `reset`'s first argument; its meaning comes from the
	 * embedded `V90Phase4Modulator::reset`'s own second argument, which
	 * it feeds and which that class documents as the code behind
	 * `codeLevel`. Was `pad_0008[4]` until `reset` was written. Named for
	 * `V90Phase3Demodulator::ucode`, which plays the same role. Finding
	 * F7472.
	 */
	unsigned char ucode;

	/*
	 * +0x009 WAS `pad_0009[3]`, REMOVED (2026-09-04, pad-audit).  An
	 * `unsigned char` ending at +0x009 followed by the 4-byte-aligned
	 * `mappingParams1` pointer at +0x000c below needs exactly this 3-byte
	 * gap for natural alignment, no dis.py reader/writer touches
	 * +0x009/+0x00a/+0x00b anywhere in the object, and
	 * `P4D_OFF(mappingParams1, 0x000c, ...)` in the .cpp already asserts
	 * the next field's offset -- so the compiler's own padding reproduces
	 * the member being deleted.
	 */

	/* +0x000c  The constructor's FIRST argument. */
	V90MappingParams *mappingParams1;

	/* +0x0010  The constructor's SECOND argument. */
	V90MappingParams *mappingParams2;

	/* +0x0014  The constructor's fourth argument. */
	V90CP *cp;

	/* +0x0018  The constructor's fifth argument. */
	V90MP *mp;

	/* +0x001c  The constructor's ninth argument. */
	V90Phase3Demodulator *phase3Demodulator;

	/*
	 * +0x0020 .. +0x004f: the state block. The construction path does
	 * not reach it -- the constructor writes nothing here -- so every
	 * offset and width below is read off the seven members this batch
	 * wrote plus `reset`, which is now written and which supplied the
	 * store widths. Signedness is not established for the `int_NNNN`
	 * ones: `movl $0x0` and `movl $0x1` encode a width and nothing else,
	 * which is the same bound V90RDetector.h states for its own fields.
	 */

	/*
	 * +0x0020  The state.  `reset` stores its `Phase4DemodulatorState`
	 * argument here and the three `enterWaitFor*`, `detectRRN` and
	 * `detectFPE` store constants of it; see the enum above.
	 */
	Phase4DemodulatorState state;

	/*
	 * +0x0024  The count of samples since the last state change. Every
	 * function that stores `state` zeroes this in the same breath, and
	 * the three `enterWaitFor*` print its old value as the "@ %d" of
	 * their message first. `getV90Decision` and `getV92Decision`
	 * increment it once per call, one sample each. Unsigned, forced by
	 * three encodings the compiler had no choice about. Finding F4800.
	 */
	unsigned int countInState;

	/*
	 * +0x0028  Zeroed by `detectRRN`, `detectFPE` and `reset` -- the three
	 * that move to a state on their own evidence -- and NOT by the three
	 * `enterWaitFor*`, which is what tells it apart from `countInState`.
	 */
	int int_0028;

	/*
	 * +0x002c  `trn2dDDLength`, and the name is the author's own: `reset`
	 * stores a parameter copy here and immediately prints "trn2dDDLength
	 * = %d symbols" from it. A duration, used by both decision members --
	 * state `TRN2D_DD` marks `countInState` reaching half of it and then
	 * all of it. The two writers disagree about which parameter fills
	 * it (`reset` uses `TRN2D_QC_DD_LENGTH`, the RdNot arm of both
	 * decision members uses `RRN_TRN2D_DD_LENGTH`), so the field is the
	 * length in force and not either parameter. Unsigned, forced (a
	 * logical rather than arithmetic shift takes its half). An older
	 * comment here misread the same store as a decision result; retracted.
	 * Finding F4800.
	 */
	unsigned int trn2dDDLength;

	/*
	 * +0x0030  ONE byte: `movb $0x0,0x30(...)` in `resetBeforRRN` and in
	 * `reset`, and nothing here reads it.
	 */
	unsigned char uchar_0030;

	/*
	 * +0x031 WAS `pad_0031[3]`, REMOVED (2026-09-04, pad-audit).  An
	 * `unsigned char` ending at +0x031 followed by the 4-byte-aligned
	 * `quickConnect` at +0x034 below needs exactly this 3-byte gap for
	 * natural alignment, no dis.py reader/writer touches
	 * +0x031/+0x032/+0x033 anywhere in the object, and
	 * `P4D_OFF(quickConnect, 0x0034, ...)` in the .cpp already asserts the
	 * next field's offset -- so the compiler's own padding reproduces the
	 * member being deleted.
	 */

	/*
	 * +0x0034  `quickConnect`, and the name is the author's own: `reset`
	 * loads this field straight into "reset called, quickConnect
	 * indication is %d\r\n". Corroborated by what the flag then selects
	 * -- non-zero takes `TRN2D_QC_DD_LENGTH` into `trn2dDDLength`, zero
	 * takes `TRN2D_DD_LENGTH`, and QC in the parameter's own name is the
	 * same abbreviation. It was `uint_0034` until `reset` was written;
	 * the two decision members' RiNot arms, choosing between the two
	 * linear-mapping study lengths on it, are the other two readers.
	 * Finding F7472.
	 */
	unsigned int quickConnect;

	/*
	 * +0x0038 and +0x003c  A PAIR, always written together.
	 * `resetBeforRRN` sets both to 1; `detectRRN` sets both to 1 when
	 * `sessionFlag` is zero; `reset` sets +0x38 to 1 and +0x3c to 0, which
	 * is the one place they differ and the reason they are two fields and
	 * not one.
	 */
	int int_0038;
	int int_003c;

	/* +0x0040  Zeroed by `reset` alone. */
	int int_0040;

	/*
	 * +0x0044 and +0x0048  Zeroed by `resetBeforRRN` and by `reset`, and
	 * both set to 1 by `getV92Decision` -- +0x44 when a CP or CPnot
	 * arrives with the conditions met, +0x48 when the first Ed at RRN is
	 * accepted.  Their three-way test with +0x3c is what separates
	 * "Ed detected, enter B1d" from "First Ed at RRN detected, Silence",
	 * and `getV90Decision` reads neither.
	 */
	int int_0044;
	int int_0048;

	/*
	 * +0x004c  Modelled, unnamed, and the role is bounded rather than
	 * settled. `getV92Decision` copies `V90CP::word_ca0` into it at two
	 * sites and, at one more, chooses `P4D_STATE_SILENCE` when it is zero
	 * and `P4D_STATE_WAIT_FOR_RT` when it is not:
	 *
	 *     27761:  83 f8 01   cmp $0x1,%eax      ; the copy
	 *     27764:  19 d2      sbb %edx,%edx
	 *     27766:  83 e2 fc   and $0xfffffffc,%edx
	 *     27769:  83 c2 0e   add $0xe,%edx      ; 0xe, or 0xa if zero
	 *
	 * So it holds one bit of the CP message and picks one of two paths
	 * with it. `V90CP.h` calls +0xca0 "the short form's payload" and
	 * declines to say what the bit means; nothing here settles that
	 * either, so this keeps the offset name (3120's ruling).
	 *
	 * Unsigned, from the `cmp $0x1` / `sbb` above -- that is the
	 * branchless form of an unsigned `!= 0`; a signed one needs `test`.
	 * The source type agrees: `V90CP::word_ca0` is `unsigned int`.
	 */
	unsigned int uint_004c;

	/*
	 * +0x0050  Embedded, not pointed at: `lea 0x50(%ebx),%edx` in the
	 * constructor and `add $0x50,%ebx` in the destructor, both address
	 * arithmetic rather than a load. Built with the two mapping-parameter
	 * pointers swapped; see the file comment.
	 */
	V90Phase4Modulator phase4Modulator;

	/*
	 * +0x2ffc and +0x3028  Embedded, one each, both built with `params`
	 * and both destroyed -- second first -- by the destructor. Which is
	 * which is not established: the two constructor calls are identical
	 * apart from the base, and nothing reconstructed here reads either.
	 */
	V90RDetector rDetector1;
	V90RDetector rDetector2;

	/* +0x3054  The constructor's third argument. Not owned. */
	V90Demapper *demapper;

	/* +0x3058  The constructor's sixth argument. Not owned. */
	Descrambler<unsigned char, int> *descrambler;

	/*
	 * +0x305c  The demapper's output buffer; both its base and its
	 * element type are read rather than chosen. Both decision members
	 * pass this address as the first argument of
	 * `V90Demapper::process(unsigned char *, unsigned int &)`, whose
	 * mangling types the element, and then walk it byte by byte.
	 *
	 * The length is a bound, not a measurement: every `.text` symbol
	 * touching this class or its owner was disassembled and searched for
	 * a displacement into 0x3060..0x34f3 off any register, and none was
	 * found -- so 0x498 is how much room there is and nothing in the
	 * object subdivides it. If a later batch finds a field inside this
	 * run, the array shortens and nothing else moves.
	 */
	unsigned char bits[0x498];

	/*
	 * +0x34f4  How many of them `process` produced. Passed as an
	 * `unsigned int &` output parameter -- so the type is the callee's
	 * mangling and not an inference -- and used as the loop bound over
	 * `bits`.
	 */
	unsigned int nbits;

	/* +0x34f8  The constructor's seventh argument.  Not owned. */
	V90ConnectionEvaluator *connectionEvaluator;

	/*
	 * +0x34fc  Modelled, unnamed, and the width is now settled: `movl`,
	 * four bytes. `reset` writes it on the V.92 arm alone -- the same
	 * value the V.90 arm stores into `mp->groupSize` instead -- and
	 * nothing else in the object reaches it.
	 *
	 * What the value is is bounded and not established. The same word
	 * goes into `V90CP::word_3ba8` in the same breath, and both V90CP.h
	 * and V90MP.h call their copy "`calcSequenceLength`'s divisor: the
	 * group size" -- so this is a shadow of the group size the CP was
	 * just given. Whether the class keeps it as that, or as the frame
	 * width `V90Mapper` and `V90BitsToSymbol` take the same
	 * `V90MappingParams::word_0` to be, is not decidable from one store
	 * with no reader, so the name stays the offset's (3120).
	 */
	unsigned int uint_34fc;

	/*
	 * +0x3500 and +0x3504  The B1d bit counters, and the names are the
	 * author's: the one message that reports them prints both, "nof B1d
	 * bits" and "B1d Zeros (after delay)", from these two offsets. State
	 * `B1D` descrambles every bit `process` returns, counts them all in
	 * `b1dBits`, and counts the zeros in `b1dZeros` once `b1dBits` has
	 * passed `3 * mappingParams2->word_0 + 0x17` -- the "(after delay)"
	 * the message names. Both unsigned, forced by the compare and the
	 * float conversion idiom at their use sites.
	 */
	unsigned int b1dZeros;
	unsigned int b1dBits;

	/*
	 * +0x3508 and +0x350c  The two silence error energies, named by the
	 * states that compute them. `CALC_ENERGY_BEFORE_EC` accumulates
	 * `decision * decision` into +0x3508 and `CALC_ENERGY_AFTER_EC` into
	 * +0x350c; each then divides by `countInState` in place and reports
	 * itself as "error energy before/after echo cancellation". The ratio
	 * of the two is what the dB line prints, with +0x350c as the divisor
	 * -- the second reason the pairing is this way round and not the
	 * other.
	 */
	float errorEnergyBeforeEC;
	float errorEnergyAfterEC;

	/*
	 * +0x3510  Modelled, unnamed. Written once, by the tail of
	 * `CALC_ENERGY_AFTER_EC`, as the one-bit result of
	 *
	 *     dB > params->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE
	 *
	 * (`flds 0x404(%ebx) ; fcompp ; setb %dl ; movzbl %dl,%ecx`), and
	 * read by nothing in the object that this tree has written. The
	 * parameter's name says what the comparison is for; it does not say
	 * whether this field is the condition, its negation, or a request,
	 * and `reset` also writes it. A neutral name and the derivation, per
	 * CLAUDE.md and 3120.
	 */
	int int_3510;

	/* +0x3514  The constructor's tenth argument.  Not owned. */
	V90AutoDigitalImpDetector *autoDigitalImpDetector;

	/*
	 * +0x3518  When the linear mapping study starts. Its only reader is
	 * state `TRN2D_DD`, which turns the study on when `countInState`
	 * reaches it exactly, and its only writers are the two RiNot/RdNot
	 * arms, which set 0x258 or 0x7d0 beside the matching
	 * `V90Demapper::resetLinearMappStudy(0x960)` or `(0x1c20)`. One
	 * reader and three writers, all saying the same thing, so the role
	 * is established even though no string names it.
	 *
	 * It used to be `pad_3518[4]`, bounded only by the 0x351c allocation.
	 */
	unsigned int linearMappStudyStart;
};

#endif /* DSPLIB_V90PHASE4DEMODULATOR_H */
