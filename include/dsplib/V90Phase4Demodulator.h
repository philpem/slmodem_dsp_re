/*
 * V90Phase4Demodulator.h -- the V.90 phase 4 receiver, so far as its
 * CONSTRUCTION PATH settles it.
 *
 * Reconstructed from dsplibs.o.  The class had no header and no .cpp in this
 * tree before this file.  `.symtab` carries `V90Phase4Demodulator.cpp` as a
 * FILE entry (index 231), so the original had a translation unit of its own
 * and this file's placement follows the original's rather than guessing at
 * one -- the same evidence `V90Demapper.h` cites for its own.
 *
 * THE OBJECT IS 0x351c = 13,596 BYTES, AND THAT IS AN ALLOCATION, not a
 * displacement scan -- finding 1107's rule.  `V90Demodulator::V90Demodulator`
 * has
 *
 *     1c8fe:  c7 04 24 1c 35 00 00   movl  $0x351c,(%esp)
 *     1c905:  e8 ..                  call  sysdep_malloc
 *     1c90a:  ...
 *     1c969:  e8 ..                  call  V90Phase4Demodulator::
 *                                             V90Phase4Demodulator(...)
 *     1c96e:  89 b3 e0 01 00 00      mov   %esi,0x1e0(%ebx)
 *
 * and the same sequence again in its C2 twin.  The highest offset the
 * constructor writes is +0x3514, so the last eight bytes are bounded by the
 * allocation alone -- which is the case the rule exists for.
 *
 * NOT POLYMORPHIC.  `nm` gives `D1` at 0x25b50 and `D2` at 0x25b10 and no
 * `D0`; GCC emits a deleting destructor only for a virtual class, so offset 0
 * is a real member and there is no vptr (finding 228).
 *
 * ---------------------------------------------------------------------------
 * THREE EXACT MEETINGS FIX THE THREE EMBEDDED SUBOBJECTS
 *
 * The constructor builds three members in place and the destructor destroys
 * the same three in reverse, so their bases are read and not guessed:
 *
 *     +0x0050  V90Phase4Modulator   sizeof 0x2fac   ends 0x2ffc
 *     +0x2ffc  V90RDetector         sizeof 0x002c   ends 0x3028
 *     +0x3028  V90RDetector         sizeof 0x002c   ends 0x3054
 *
 * Each end is the next base exactly, and 0x3054 is where the first of the
 * four trailing pointers lives.  Three independent bases and three exact
 * meetings, and `V90Phase4Demodulator.cpp` asserts all of them with
 * `__builtin_offsetof` rather than restating them in a comment -- so if
 * either subobject's own size ever moves, the compiler says so.
 *
 * ---------------------------------------------------------------------------
 * THE TWO MAPPING-PARAMETER POINTERS REACH THE MODULATOR SWAPPED
 *
 * The embedded modulator is built with
 *
 *     V90Phase4Modulator(params, mode, NULL, NULL,
 *                        mappingParams2, mappingParams1, NULL, 0xc)
 *
 * -- argument 2 into the modulator's fifth slot and argument 1 into its
 * sixth.  In the object that is `mov 0x48(%esp),%edx` reaching
 * `mov %edx,0x14(%esp)` and `mov 0x44(%esp),%ebp` reaching
 * `mov %ebp,0x18(%esp)`, two loads and two stores with nothing between them
 * that could have reordered a pair of independent stack slots by accident.
 * The demodulator stores them the OTHER way round in its own object
 * (+0x0c gets argument 1, +0x10 argument 2), so this really is a crossing and
 * not a misreading of which is which.
 *
 * A test that passed the same pointer twice could not see this at all, which
 * is why the two must be distinguishable.
 *
 * ---------------------------------------------------------------------------
 * THE THREE MEMBER CONSTRUCTIONS ARE THE COMPILER'S, AND THE ORDER PROVES THE
 * DECLARATION ORDER
 *
 * The constructor calls `V90Phase4Modulator`'s at +0x50, then
 * `V90RDetector`'s at +0x2ffc, then `V90RDetector`'s at +0x3028; the
 * destructor calls them at +0x3028, +0x2ffc and +0x50.  Exactly reversed, and
 * no body statement of either sits between any pair -- which is what a
 * compiler emits for three members in declaration order and nothing else.
 * So the .cpp writes none of the six calls: it writes the mem-initializer
 * list, and the twelve stores that ARE the body.
 *
 * Both member constructions of `V90RDetector` take the same argument, so the
 * two detectors are told apart by their offsets alone and by nothing in the
 * source.  `rDetector1` and `rDetector2` name positions, not roles.
 *
 * ---------------------------------------------------------------------------
 * ALL FOUR SYMBOLS MATCH THE BLOB'S INSTRUCTION SEQUENCE
 *
 * `make similarity` lists `C1`, `C2`, `D1` and `D2` -- 225, 225, 52 and 52
 * bytes -- among the identical mnemonic sequences.  That is a second,
 * independent tier agreeing with the differential one, and it is worth having
 * on a function whose whole body is eleven stores: `compare.py` compares
 * mnemonics and not operands, so it says nothing about WHICH field each store
 * reached, and the test's eleven placement assertions say nothing about the
 * instruction the compiler chose.  Neither alone is the claim; together they
 * are close to it.
 */

#ifndef DSPLIB_V90PHASE4DEMODULATOR_H
#define DSPLIB_V90PHASE4DEMODULATOR_H

#include "dsplib/Scrambler.h"
#include "dsplib/V90Phase4Modulator.h"
#include "dsplib/V90RDetector.h"

/*
 * POINTERS ONLY, so forward declarations are what belong here.  Two
 * incompatible definitions of `V90Parameters` exist in this tree and no
 * translation unit may include both -- finding 1112 -- so the class is
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
 * THE ENUM IS THE MANGLING'S, NOT AN INVENTION.  `V90Phase4Demodulator::reset`
 * is `_ZN20V90Phase4Demodulator5resetEh22Phase4DemodulatorStatejj`, so a type
 * spelled exactly `Phase4DemodulatorState` exists at namespace scope and is
 * that member's second parameter -- and `reset` stores that parameter, and
 * nothing else, into +0x20:
 *
 *     277ef:  8b 5c 24 38   mov 0x38(%esp),%ebx    ; argument 2
 *     27801:  89 5e 20      mov %ebx,0x20(%esi)
 *
 * with `this` at 0x30(%esp) and the four arguments at 0x34, 0x38, 0x3c and
 * 0x40.  So the field below is that enum and not an `unsigned int` that
 * happens to hold the same numbers.  Same shape as `Phase3DemodulatorState`
 * in V90Phase3Demodulator.h, and named the same way.
 *
 * FOUR OF THE FIVE ENUMERATORS ARE THE AUTHOR'S OWN WORDS -- the `edprintf`
 * format string at the only site that stores each value:
 *
 *     4     "V90Phase4Demodulator: enter WaitForV90CP state @ %d\r\n"
 *     5     "V90Phase4Demodulator: enter WaitForMP state @ %d\r\n"
 *     6     "V90Phase4Demodulator: enter WaitForEd state @ %d\r\n"
 *     0x10  "V90Phase4Demodulator: enter FPE !"
 *
 * THE FIFTH IS USAGE INFERENCE and is the weakest thing in this header.
 * `detectRRN` is the only writer of 9 and it writes it immediately after
 * printing "Rd detected"; the original's own name for the state is not
 * recoverable, so `P4D_STATE_RD_DETECTED` says what the object does and
 * claims nothing more.
 *
 * THE GAPS ARE NOW FILLED, by `getV90Decision` and `getV92Decision`, whose
 * switch is over the whole 0..0x11 range and whose arms carry the entry
 * messages for eleven more.  Eight of those eleven are the author's own words
 * again -- the message printed on the transition INTO the state, at the only
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
 * THE REMAINING THREE ARE THE SAME CONSTRUCTION APPLIED TO THE SAME SHAPE OF
 * ARM, and that is weaker than a string but stronger than a guess.  State 0xe
 * is called `WaitForRt` by the author, and its arm is exactly "run
 * `rDetector1.detectR`, and on success report `Rt detected` and move on".
 * States 0, 1 and 0xf have that arm with a different detector call and a
 * different message -- `detectR`/"Ri detected", `detectRNot`/"RiNot detected",
 * `detectRNot`/"RtNot detected" -- so `WAIT_FOR_RI`, `WAIT_FOR_RI_NOT` and
 * `WAIT_FOR_RT_NOT` name what the arm waits for, in the author's own scheme.
 * Finding 4810.
 *
 * 0x11 IS DELIBERATELY UNNAMED.  Both functions send it to the same body as
 * state 8 -- return zero, touch nothing -- and no message anywhere in the
 * object stores or reports it, so what distinguishes it from 8 is not
 * recoverable.  The switch writes the literal; naming it would be inventing
 * the distinction.  Same ruling as `V90MP`'s, and 3120's.
 *
 * STATE 4 HAS TWO OF THE AUTHOR'S NAMES, one per function, and that is the
 * object's rather than a slip here: `getV90Decision` reaches it through
 * `enterWaitForCP`'s "enter WaitForV90CP state" and `getV92Decision` through
 * "RfNot detected @ %d, enter WaitForCPu state".  The V.90 spelling is kept
 * because it was here first.
 *
 * SPELLED AS A PIN RATHER THAN A BASE for the reason Phase3DemodulatorState
 * gives: `: int` is C++11 and the author's compiler was C++98, where these
 * five values alone would give the enum a range of 0..31 and make a
 * differential sweep of a garbage-seeded +0x20 undefined.  `_BASE_PIN` is
 * ours; docs/method/compilers.md, V2.
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
	 * MODELLED, UNNAMED, and the name says so on purpose.  Both decision
	 * members carry a case label for 0x11 -- GCC sizes a jump table by
	 * the case range, and both tables are eighteen entries under
	 * `cmp $0x11` -- so the value exists in the original's source.  What
	 * it MEANS is not recoverable: no store anywhere in the object puts
	 * it in +0x20, no message reports it, and its arm is the same three
	 * instructions as `TERMINATED`'s.  `P4D_STATE_11` would be an offset
	 * wearing a name; this spelling claims exactly what is known.
	 */
	P4D_STATE_UNNAMED_11 = 0x11,
	P4D_STATE_BASE_PIN = -0x7fffffff - 1	/* ours: pins the base */
};

class V90Phase4Demodulator {
public:
	/*
	 * Defined in src/pump/v90/V90Phase4Demodulator.cpp.  The parameter
	 * list is the mangling's and not a choice: an `int` where the original
	 * had `unsigned` emits a different symbol that links against nothing.
	 * C1 at 0x25a20 and C2 at 0x25930, 225 bytes each; D1 at 0x25b50 and
	 * D2 at 0x25b10, 52 bytes each.
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
	~V90Phase4Demodulator();

	/*
	 * THE SEVEN LEAVES, all in src/pump/v90/V90Phase4Demodulator.cpp.
	 * Between them they are the whole state-entry surface of the class:
	 * everything that stores `state` except `reset` and the two decision
	 * members.
	 *
	 * `detectRRN` and `detectFPE` return `int` and not `bool` -- the blob
	 * builds the answer in a full 32-bit register (`xor %edx,%edx` /
	 * `mov $0x1,%edx` / `mov %edx,%eax`) rather than in `%al`.  Neither
	 * return type reaches the mangling, so this is the only evidence
	 * there is for it.
	 */
	void resetRRNDetector();
	void resetBeforRRN();
	void enterWaitForCP();
	void enterWaitForMP();
	void enterWaitForEd();
	int detectRRN(short sample);
	int detectFPE(short sample);

	/*
	 * THE THREE DECISION MEMBERS.  `getDecision` is the switch on
	 * `sessionFlag` and nothing else; the other two are one arm each of
	 * it, and between them they are the whole phase 4 state machine.
	 *
	 * THE RETURN TYPES ARE READ OFF `getDecision`, WHICH IS THE ONLY
	 * PLACE THEY SHOW.  A return type is not mangled, so the evidence is
	 * `cwtl` -- sign-extend %ax into %eax -- on the result of each of the
	 * two calls, at 0x2779e and 0x277af.  A caller only widens what the
	 * callee left narrow, so the two arms return `short`; and a caller
	 * that widens is one whose own result is `int`, so `getDecision`
	 * does.  Nothing else in the object separates the three.
	 *
	 * The rest of the class -- `trn2dKnownDemod`, `setSessionFlag` and
	 * the rest -- is declared nowhere yet and belongs to whichever batch
	 * writes it.  `trn2dKnownDemod` is blocked on `V90SpectralShaper`,
	 * which is not written.  `reset` used to head that list and is now
	 * below.
	 */
	int getDecision(short sample);
	short getV90Decision(short sample);
	short getV92Decision(short sample);

	/*
	 * `reset` -- .text+0x277c0, 504 bytes, and the WHOLE receiver's
	 * entry point: it puts the eleven scalars back, reseeds both
	 * `V90RDetector`s, resets whichever of the CP and the MP the session
	 * uses, hands the mapping block to the demapper and to the embedded
	 * modulator, and then runs the decision member `sessionFlag` selects
	 * `nofSamples` times.
	 *
	 * ALL FOUR ARGUMENT TYPES ARE THE MANGLING'S,
	 * `_ZN20V90Phase4Demodulator5resetEh22Phase4DemodulatorStatejj`, and
	 * the second is what proves `Phase4DemodulatorState` exists as a
	 * namespace-scope type at all (see the enum above).  `void` is the
	 * return: neither exit sets `%eax`.
	 *
	 * THE THIRD ARGUMENT IS A TRIP COUNT AND REACHES NO FIELD, exactly as
	 * `V90Phase4Modulator::reset`'s fourth does; the fourth lands in
	 * `quickConnect`.
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
	 * +0x0000  The constructor's ELEVENTH argument.  `V90Demodulator`
	 * passes its own +0x30 -- the field its `setSessionFlag` writes -- and
	 * hands the same value to `V90Phase3Demodulator`'s third argument,
	 * which that class's header already calls `sessionFlag`.  The name
	 * follows the value, not this constructor.
	 */
	unsigned int sessionFlag;

	/* +0x0004  The constructor's eighth argument. */
	V90Parameters *params;

	/*
	 * +0x0008  THE G.711 CODE THE PHASE 4 MODULATOR IS TO SEND, and it
	 * was `pad_0008[4]` until `reset` was written -- which is finding
	 * 7453's shape in the other class: the constructor does not touch it,
	 * so the construction path could say nothing about it at all.
	 *
	 * ONE BYTE, not four: `mov %cl,0x8(%esi)` at 0x277ec, `reset`'s FIRST
	 * argument, which the mangling types `h`.  The only read anywhere is
	 * `movzbl 0x8(%esi),%eax` at 0x278ec, feeding the embedded
	 * `V90Phase4Modulator::reset`'s own second argument -- typed `h` by
	 * ITS mangling and documented there as the code whose linear
	 * expansion becomes `codeLevel`.  So the meaning is the callee's
	 * (CLAUDE.md's evidence order, rule 2) and the name is the one this
	 * tree already uses for the same byte in the same role:
	 * `V90Phase3Demodulator::ucode`.
	 */
	unsigned char ucode;
	unsigned char pad_0009[3];	/* +0x009  alignment before +0x0c  */

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
	 * +0x0020 .. +0x004f  THE STATE BLOCK.  The construction path does
	 * not reach it -- the constructor writes nothing here -- so every
	 * offset and width below is read off the seven members this batch
	 * wrote plus `reset`, which IS now written and which supplied the
	 * store widths.  Signedness is NOT established for the `int_NNNN`
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
	 * +0x0024  THE COUNT OF SAMPLES SINCE THE LAST STATE CHANGE.  Every
	 * function that stores `state` zeroes this in the same breath -- the
	 * three `enterWaitFor*`, `detectRRN`, `detectFPE`, `reset` and every
	 * transition in the two decision members -- and the three
	 * `enterWaitFor*` print its old value as the "@ %d" of their message
	 * before doing so.  `getV90Decision` and `getV92Decision` increment
	 * it once per call, and a call is one sample, which is what settles
	 * the unit; `trn2dDDLength` below is printed as "%d symbols".
	 *
	 * UNSIGNED, AND THAT IS FORCED THREE WAYS, against the `int` this
	 * field carried when only the leaves had been read.  The `%d` in the
	 * author's own format string was the whole of the old argument, and
	 * it is tier-3 usage inference; all three of these are encodings the
	 * compiler had no choice about (CLAUDE.md's FORCED column):
	 *
	 *   - `cmp 0x400(%edx),%ecx ; jb` at 0x26195 and three more, against
	 *     `V90Parameters::RRN_SILENCE_ECHO_CALC_PERIOD`, which is `int`.
	 *     Two `int`s compare with `jl`; `jb` needs one side unsigned.
	 *   - `mov $0xaaaaaaab,%ebx ; mul %ebx ; shr $0x2,%edx` -- the
	 *     UNSIGNED magic for `% 6`, at 0x261a3 and five more.  A signed
	 *     `% 6` is `imul` with 0x2aaaaaab plus a sign fixup.
	 *   - `xor %edx,%edx ; push %edx ; push %eax ; fildll` at 0x267ed and
	 *     three more: the zero high word is the unsigned-to-float
	 *     widening.  A signed `int` converts with a plain `fildl`.
	 *
	 * Finding 4800.
	 */
	unsigned int countInState;

	/*
	 * +0x0028  Zeroed by `detectRRN`, `detectFPE` and `reset` -- the three
	 * that move to a state on their own evidence -- and NOT by the three
	 * `enterWaitFor*`, which is what tells it apart from `countInState`.
	 */
	int int_0028;

	/*
	 * +0x002c  `trn2dDDLength`, AND THE NAME IS THE AUTHOR'S OWN.  `reset`
	 * prints exactly this field:
	 *
	 *     27926:  8b 80 c0 04 ..   mov 0x4c0(%eax),%eax   ; params
	 *     2792c:  89 46 2c         mov %eax,0x2c(%esi)
	 *     27933:  c7 04 24 a8 6c   movl $0x6ca8,(%esp)
	 *             "V90Phase4Demodulator: trn2dDDLength = %d symbols\r\n"
	 *
	 * -- one store and the format string that reports it, which is the
	 * strongest evidence this project recognises.  The old comment here
	 * said `reset` stored "the result of getV90Decision or getV92Decision"
	 * in it; that was a misreading of the same instruction and IS
	 * RETRACTED.  It is a duration, and both decision members use it as
	 * one: state `TRN2D_DD` marks `countInState` reaching half of it and
	 * then all of it.
	 *
	 * The two writers disagree about which parameter fills it --
	 * `reset` uses `TRN2D_QC_DD_LENGTH` (+0x4c0) and the RdNot arm of both
	 * decision members uses `RRN_TRN2D_DD_LENGTH` (+0x370) -- so the field
	 * is the length in force, not either parameter.
	 *
	 * UNSIGNED, forced: `mov %ecx,%ebx ; shr $1,%ebx` at 0x25f5b takes the
	 * half with a LOGICAL shift.  A signed `/2` carries the bias fixup and
	 * a signed `>>1` is `sar`.
	 */
	unsigned int trn2dDDLength;

	/*
	 * +0x0030  ONE byte: `movb $0x0,0x30(...)` in `resetBeforRRN` and in
	 * `reset`, and nothing here reads it.
	 */
	unsigned char uchar_0030;
	unsigned char pad_0031[3];	/* +0x31  alignment before +0x34   */

	/*
	 * +0x0034  `quickConnect`, AND THE NAME IS THE AUTHOR'S OWN.  It is
	 * `reset`'s FOURTH argument, which the mangling types `unsigned int`
	 * (`...jj`, and this is the second of the two), and `reset` prints
	 * exactly this field:
	 *
	 *     279a0:  8b 5e 34         mov 0x34(%esi),%ebx
	 *     279a3:  c7 04 24 dc 6c   movl $0x6cdc,(%esp)
	 *        "V90Phase4Demodulator: reset called, quickConnect
	 *         indication is %d\r\n"
	 *
	 * -- one load and the format string that reports it, which is the
	 * strongest evidence this project recognises (CLAUDE.md, rule 1).
	 * It is CORROBORATED by what the flag then selects: non-zero takes
	 * `V90Parameters::TRN2D_QC_DD_LENGTH` into `trn2dDDLength` and zero
	 * takes `TRN2D_DD_LENGTH`, and the QC in the first parameter's own
	 * name is the same abbreviation.  It was `uint_0034` until `reset`
	 * was written; the two decision members' RiNot arms, which choose
	 * between the two linear-mapping study lengths on it, are the other
	 * two readers.
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
	 * +0x004c  MODELLED, UNNAMED, and the role is bounded rather than
	 * settled.  `getV92Decision` copies `V90CP::word_ca0` into it at two
	 * sites and, at one more, chooses `P4D_STATE_SILENCE` when it is zero
	 * and `P4D_STATE_WAIT_FOR_RT` when it is not:
	 *
	 *     27761:  83 f8 01   cmp $0x1,%eax      ; the copy
	 *     27764:  19 d2      sbb %edx,%edx
	 *     27766:  83 e2 fc   and $0xfffffffc,%edx
	 *     27769:  83 c2 0e   add $0xe,%edx      ; 0xe, or 0xa if zero
	 *
	 * So it holds one bit of the CP message and picks one of two paths
	 * with it.  `V90CP.h` calls +0xca0 "the short form's payload" and
	 * declines to say what the bit means; nothing here settles that
	 * either, so this keeps the offset name.  3120's ruling.
	 *
	 * UNSIGNED, from the `cmp $0x1` / `sbb` above -- that is the
	 * branchless form of an unsigned `!= 0`; a signed one needs `test`.
	 * The source type agrees: `V90CP::word_ca0` is `unsigned int`.
	 */
	unsigned int uint_004c;

	/*
	 * +0x0050  EMBEDDED, not pointed at: `lea 0x50(%ebx),%edx` in the
	 * constructor and `add $0x50,%ebx` in the destructor, both address
	 * arithmetic rather than a load.  Built with the two mapping-parameter
	 * pointers SWAPPED; see the file comment.
	 */
	V90Phase4Modulator phase4Modulator;

	/*
	 * +0x2ffc and +0x3028  EMBEDDED, one each, both built with `params`
	 * and both destroyed -- second first -- by the destructor.  Which is
	 * which is not established: the two constructor calls are identical
	 * apart from the base, and nothing reconstructed here reads either.
	 */
	V90RDetector rDetector1;
	V90RDetector rDetector2;

	/* +0x3054  The constructor's third argument.  Not owned. */
	V90Demapper *demapper;

	/* +0x3058  The constructor's sixth argument.  Not owned. */
	Descrambler<unsigned char, int> *descrambler;

	/*
	 * +0x305c  THE DEMAPPER'S OUTPUT BUFFER, and both its base and its
	 * element type are read rather than chosen.  Both decision members
	 * pass `lea 0x305c(%esi)` as the first argument of
	 * `V90Demapper::process(unsigned char *, unsigned int &)`, whose
	 * mangling types the element, and then walk it with
	 * `movzbl 0x305c(%ebx,%esi,1)` -- a byte load with a stride of one.
	 *
	 * THE LENGTH IS A BOUND, NOT A MEASUREMENT, and the scan behind it is
	 * named so a later reader can widen it rather than repeat it.  Every
	 * `.text` symbol whose name carries `V90Phase4Demodulator` or
	 * `V90Demodulator` -- forty of them, which is this class's own members
	 * plus the only class that holds one (at its +0x1e0) -- was
	 * disassembled and searched for a displacement in 0x3060..0x34f3 off
	 * any register.  There are NONE.  So 0x498 is how much room there is
	 * and nothing in the object subdivides it; no allocation, no memset
	 * and no bounds test states the array's declared size.  If a later
	 * batch finds a field inside this run -- through a pointer this scan
	 * cannot follow, say -- the array shortens and nothing else moves.
	 */
	unsigned char bits[0x498];

	/*
	 * +0x34f4  HOW MANY OF THEM `process` PRODUCED.  Passed as
	 * `lea 0x34f4(%esi)` into that member's `unsigned int &` -- so the
	 * type is the callee's mangling and not an inference -- and used as
	 * the loop bound over `bits` with `ja`/`jbe`.
	 */
	unsigned int nbits;

	/* +0x34f8  The constructor's seventh argument.  Not owned. */
	V90ConnectionEvaluator *connectionEvaluator;

	/*
	 * +0x34fc  MODELLED, UNNAMED, and the width is now settled: `movl`,
	 * four bytes.  `reset` writes it on the V.92 arm ALONE --
	 * `mov %edx,0x34fc(%esi)` at 0x27885, where `%edx` is
	 * `mappingParams1->word_0` -- and the V.90 arm, twenty-five bytes
	 * further down, stores the same value into `mp->word_114` and leaves
	 * this field exactly as it found it.  Nothing else in the object
	 * reaches it.
	 *
	 * What the value IS is bounded and not established.  The same word
	 * goes into `V90CP::word_3ba8` in the same breath, and both V90CP.h
	 * and V90MP.h call their copy "`calcSequenceLength`'s divisor: the
	 * group size" -- so this is a shadow of the group size the CP was
	 * just given.  Whether the class keeps it as that, or as the frame
	 * width `V90Mapper` and `V90BitsToSymbol` take the same
	 * `V90MappingParams::word_0` to be, is not decidable from one store
	 * with no reader, so the name stays the offset's (3120).
	 */
	unsigned int uint_34fc;

	/*
	 * +0x3500 and +0x3504  THE B1d BIT COUNTERS, and the names are the
	 * author's: the one message that reports them prints both, in this
	 * order, from these two offsets --
	 *
	 *     "Phase4 Terminated @ %d,  nof B1d bits = %d,
	 *      B1d Zeros (after delay) = %d"
	 *
	 * with +0x3504 in the second slot and +0x3500 in the third.  State
	 * `B1D` descrambles every bit `process` returns, counts them all in
	 * `b1dBits`, and counts the zeros in `b1dZeros` once `b1dBits` has
	 * passed `3 * mappingParams2->word_0 + 0x17` -- which is the "(after
	 * delay)" the message names.
	 *
	 * BOTH UNSIGNED, forced: `cmp %eax,%edx ; jbe` at 0x26576 guards the
	 * zero count, and the BER at 0x26447 converts both with the
	 * zero-high-word `fildll` idiom rather than a signed `fildl`.
	 */
	unsigned int b1dZeros;
	unsigned int b1dBits;

	/*
	 * +0x3508 and +0x350c  THE TWO SILENCE ERROR ENERGIES, named by the
	 * states that compute them.  `CALC_ENERGY_BEFORE_EC` accumulates
	 * `decision * decision` into +0x3508 and `CALC_ENERGY_AFTER_EC` into
	 * +0x350c; each then divides by `countInState` in place and reports
	 * itself as "error energy before/after echo cancellation".  Floats,
	 * from `fadds`/`fstps`/`flds` throughout -- never `faddl`.
	 *
	 * The ratio of the two is what the dB line prints, and +0x350c is the
	 * divisor there, which is the second reason the pairing is this way
	 * round and not the other.
	 */
	float errorEnergyBeforeEC;
	float errorEnergyAfterEC;

	/*
	 * +0x3510  MODELLED, UNNAMED.  Written once, by the tail of
	 * `CALC_ENERGY_AFTER_EC`, as the one-bit result of
	 *
	 *     dB > params->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE
	 *
	 * (`flds 0x404(%ebx) ; fcompp ; setb %dl ; movzbl %dl,%ecx`), and
	 * read by nothing in the object that this tree has written.  The
	 * parameter's name says what the comparison is FOR; it does not say
	 * whether this field is the condition, its negation, or a request,
	 * and `reset` also writes it.  A neutral name and the derivation, per
	 * CLAUDE.md and 3120.
	 */
	int int_3510;

	/* +0x3514  The constructor's tenth argument.  Not owned. */
	V90AutoDigitalImpDetector *autoDigitalImpDetector;

	/*
	 * +0x3518  WHEN THE LINEAR MAPPING STUDY STARTS.  Its only reader is
	 * state `TRN2D_DD`, which turns the study on when `countInState`
	 * reaches it exactly (`cmp %eax,0x24(%esi) ; jne`), and its only
	 * writers are the two RiNot/RdNot arms, which set 0x258 or 0x7d0
	 * beside the matching `V90Demapper::resetLinearMappStudy(0x960)` or
	 * `(0x1c20)`.  One reader and three writers, all saying the same
	 * thing, so the role is established even though no string names it.
	 *
	 * It used to be `pad_3518[4]`, bounded only by the 0x351c allocation.
	 */
	unsigned int linearMappStudyStart;
};

#endif /* DSPLIB_V90PHASE4DEMODULATOR_H */
