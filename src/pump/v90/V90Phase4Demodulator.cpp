/*
 * V90Phase4Demodulator.cpp -- the lifecycle pair, and the object map it came
 * from, asserted.
 *
 * `include/dsplib/V90Phase4Demodulator.h` carries the derivation: where each
 * of the fourteen fields came from, the three exact meetings that fix the
 * embedded subobjects, and the mapping-parameter swap the modulator sees.
 *
 * PLAIN CDECL, `this` AS THE FIRST STACK ARGUMENT (finding F215).  After
 * `sub $0x3c,%esp` and four register saves into the frame, the constructor's
 * twelve incoming words are this 0x40, mappingParams1 0x44, mappingParams2
 * 0x48, demapper 0x4c, cp 0x50, mp 0x54, descrambler 0x58,
 * connectionEvaluator 0x5c, params 0x60, phase3Demodulator 0x64,
 * autoDigitalImpDetector 0x68 and sessionFlag 0x6c.
 *
 * WHAT A FILE OF ASSERTIONS IS FOR is that it turns the header's arithmetic
 * into something the compiler checks.  The three meetings below are the whole
 * reason the map is believable, and each of them depends on a size this file
 * does not own:
 *
 *     0x0050 + sizeof(V90Phase4Modulator) == 0x2ffc     (0x2fac)
 *     0x2ffc + sizeof(V90RDetector)       == 0x3028     (0x002c)
 *     0x3028 + sizeof(V90RDetector)       == 0x3054     (0x002c)
 *
 * If either class's own size ever moves -- and both are bounds derived from
 * displacement scans, so both may -- the two `offsetof`s on the far side stop
 * agreeing and this file stops compiling.  Recording it in a comment instead
 * would have let the map rot silently.
 */

#include <stddef.h>
#include <math.h>

#include "dsplib/debug.h"
#include "dsplib/encode.h"
/* No linkage guard of its own, so it takes the usual wrapper. */
extern "C" {
#include "dsplib/pcm.h"
}
#include "dsplib/V90Parameters.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"

/*
 * Guarded on a 32-bit pointer because `check64` compiles the same source for
 * a host whose pointers are eight bytes, where none of these offsets can
 * hold.  Same shape as V90Demapper.cpp and V90Phase3Demodulator.cpp.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define P4D_OFF(field, off, tag) \
	typedef char v90p4d_off_##tag[ \
	    ((int)__builtin_offsetof(V90Phase4Demodulator, field) == (off)) \
	    ? 1 : -1]

P4D_OFF(sessionFlag,		0x0000, sessionflag);
P4D_OFF(params,			0x0004, params);
P4D_OFF(ucode,			0x0008, ucode);
P4D_OFF(mappingParams1,		0x000c, mp1);
P4D_OFF(mappingParams2,		0x0010, mp2);
P4D_OFF(cp,			0x0014, cp);
P4D_OFF(mp,			0x0018, mp);
P4D_OFF(phase3Demodulator,	0x001c, p3d);
P4D_OFF(phase4Modulator,	0x0050, p4mod);
P4D_OFF(rDetector1,		0x2ffc, rdet1);
P4D_OFF(rDetector2,		0x3028, rdet2);
P4D_OFF(demapper,		0x3054, demapper);
P4D_OFF(descrambler,		0x3058, descrambler);
P4D_OFF(connectionEvaluator,	0x34f8, conneval);
P4D_OFF(autoDigitalImpDetector,	0x3514, adid);

/* The state block, from the seven leaves below and from `reset`'s widths. */
P4D_OFF(state,			0x0020, state);
P4D_OFF(countInState,		0x0024, countinstate);
P4D_OFF(int_0028,		0x0028, i28);
P4D_OFF(trn2dDDLength,		0x002c, trn2dddlen);
P4D_OFF(uchar_0030,		0x0030, u30);
P4D_OFF(quickConnect,		0x0034, u34);
P4D_OFF(int_0038,		0x0038, i38);
P4D_OFF(int_003c,		0x003c, i3c);
P4D_OFF(int_0040,		0x0040, i40);
P4D_OFF(int_0044,		0x0044, i44);
P4D_OFF(int_0048,		0x0048, i48);
P4D_OFF(uint_004c,		0x004c, u4c);

/*
 * The phase 4 receiver's own state, from the two decision members.  The bit
 * buffer's BASE is asserted and its length is not, because 0x498 is a bound
 * and not a measurement -- see the header.  What the assertion does hold is
 * that `nbits` lands where `V90Demapper::process`'s reference argument
 * pointed, which is what would break if the array were resized carelessly.
 */
P4D_OFF(bits,			0x305c, bits);
P4D_OFF(nbits,			0x34f4, nbits);
P4D_OFF(uint_34fc,		0x34fc, u34fc);
P4D_OFF(b1dZeros,		0x3500, b1dzeros);
P4D_OFF(b1dBits,		0x3504, b1dbits);
P4D_OFF(errorEnergyBeforeEC,	0x3508, eebefore);
P4D_OFF(errorEnergyAfterEC,	0x350c, eeafter);
P4D_OFF(int_3510,		0x3510, i3510);
P4D_OFF(linearMappStudyStart,	0x3518, lmsstart);

/*
 * The enum is four bytes wide, which is what makes `state` a field at 0x20
 * and `countInState` one at 0x24 rather than two halves of one word.  GCC
 * 3.4.2 and GCC 13 both give it `int` here; the pin in the header is what
 * keeps that true under C++98's minimum-range rule.
 */
typedef char v90p4d_statesize[(sizeof(Phase4DemodulatorState) == 4) ? 1 : -1];

/*
 * The allocation, and finding F1107's point again: this number is
 * `movl $0x351c,(%esp); call sysdep_malloc` at 0x1c8fe inside
 * `V90Demodulator::V90Demodulator`, not the highest displacement any
 * V90Phase4Demodulator symbol uses -- which would have said 0x3518.
 */
typedef char v90p4d_size[(sizeof(V90Phase4Demodulator) == 0x351c) ? 1 : -1];

/*
 * And the two sizes the three meetings rest on, named so that a failure says
 * WHICH class moved rather than only that an offset is wrong.
 */
typedef char v90p4d_modsize[(sizeof(V90Phase4Modulator) == 0x2fac) ? 1 : -1];
typedef char v90p4d_rdetsize[(sizeof(V90RDetector) == 0x2c) ? 1 : -1];

#endif

/*
 * The modulator's eighth argument, `mov $0xc,%eax` into outgoing slot 0x20.
 * `V90Modulator` passes the same 0xc from its own constructor, so the two
 * call sites agree and the constant belongs to the modulator's contract
 * rather than to either caller.
 */
#define V90P4D_MODULATOR_ARG8	0xcu

/*
 * `V90Phase4Demodulator::V90Phase4Demodulator` -- 225 bytes at 0x25a20 (C1)
 * and again at 0x25930 (C2).
 *
 * THE EMBEDDED MODULATOR RECEIVES ITS TWO MAPPING-PARAMETER POINTERS
 * SWAPPED, and this is the one claim in the function a careless fixture
 * cannot see.  `mov 0x48(%esp),%edx` -- the SECOND argument -- reaches
 * outgoing slot 0x14, which is the modulator's fifth parameter, and
 * `mov 0x44(%esp),%ebp` -- the FIRST -- reaches slot 0x18, its sixth.  This
 * object stores them the other way round at +0x0c and +0x10, so it really is
 * a crossing.  Finding F1301; the V.90 modulator batch derived the same swap
 * independently from the far side of the call.
 *
 * THREE OF THE MODULATOR'S EIGHT ARGUMENTS ARE NULL AND ONE IS THE LITERAL
 * 12.  `xor %ecx,%ecx` into slot 0x0c, `xor %eax,%eax` into 0x10, another
 * zero into 0x1c and `mov $0xc,%eax` into 0x20: the bits-to-symbol converter,
 * the MP record and the CP record are all null from here, and the trailing
 * count is 0xc.  A phase 4 modulator built by `V90Modulator` gets real
 * pointers in those three slots; this one does not, and its ownership flag
 * comes out the other way as a result.
 *
 * THE SIX MEMBER CALLS ARE THE COMPILER'S.  Three constructions in
 * declaration order here and three destructions in reverse below, with no
 * body statement between any pair -- so the mem-initializer list is what this
 * file writes and the calls are what it must not.
 *
 * `sessionFlag` IS NAMED FOR ITS VALUE, NOT FOR THIS CONSTRUCTOR.
 * `V90Demodulator` passes its own +0x30 -- the field its `setSessionFlag`
 * writes -- and hands the same value to `V90Phase3Demodulator`'s third
 * argument, which that class already calls `sessionFlag`.
 */
V90Phase4Demodulator::V90Phase4Demodulator(V90MappingParams *mp1,
					   V90MappingParams *mp2,
					   V90Demapper *dem, V90CP *cpArg,
					   V90MP *mpArg,
					   Descrambler<unsigned char, int> *dsc,
					   V90ConnectionEvaluator *ce,
					   V90Parameters *par,
					   V90Phase3Demodulator *p3d,
					   V90AutoDigitalImpDetector *adid,
					   unsigned int flag)
	: phase4Modulator(par, flag, 0, 0, mp2, mp1, 0, V90P4D_MODULATOR_ARG8),
	  rDetector1(par), rDetector2(par)
{
	sessionFlag = flag;
	params = par;
	mappingParams1 = mp1;
	mappingParams2 = mp2;
	cp = cpArg;
	mp = mpArg;
	phase3Demodulator = p3d;
	demapper = dem;
	descrambler = dsc;
	connectionEvaluator = ce;
	autoDigitalImpDetector = adid;
}

/*
 * `~V90Phase4Demodulator` -- 52 bytes at 0x25b50 (D1) and 0x25b10 (D2).
 *
 * AN EMPTY BODY, AND THE FIFTY-TWO BYTES ARE ALL THE COMPILER'S: three calls
 * at +0x3028, +0x2ffc and +0x50, in that order, and the frame around them.
 * Nothing is freed -- this object owns no heap -- and nothing is stored, so
 * the eleven pointers it holds are still there after it runs.  A body that
 * cleared any of them would be a real difference and `-fno-lifetime-dse`
 * keeps it visible, which is what the destructor's mutations are for.
 */
V90Phase4Demodulator::~V90Phase4Demodulator()
{
}

/*
 * ===========================================================================
 * THE SEVEN LEAVES.
 *
 * All seven are state entries or the two detectors that cause one, and none
 * of them reaches anything outside this class except `V90RDetector`,
 * `edprintf` and one field of `V90Parameters`.  They are written here in the
 * blob's address order -- 0x25bb0, 0x25be0, 0x25c10, 0x25c40, 0x25ca0,
 * 0x25d30, 0x25d60 -- which is also GCC's emission order for a single
 * translation unit, so the file's order is the original's.
 *
 * `trn2dKnownDemod` at 0x25de0 sits between `resetBeforRRN` and `detectFPE`
 * in that run and IS WRITTEN NOW, in that position, by the VPcmV34Main leaf
 * pass -- the paragraph here used to call it deliberately absent on
 * unwritten callees, and its callees (`V90Phase4Modulator::generateSymbol`,
 * `linear2alaw`, `linear2ulaw`) are all written today.  The
 * `V90SpectralShaper` dependency the old text named was never this
 * function's: its three calls are the ones just listed.
 * ===========================================================================
 */

/*
 * `enterWaitForCP` -- 46 bytes at 0x25bb0.
 * `enterWaitForMP` -- 46 bytes at 0x25be0.
 * `enterWaitForEd` -- 46 bytes at 0x25c10.
 *
 * THREE BODIES THAT DIFFER IN TWO TOKENS, which is exactly why the test does
 * not stop at comparing the object: the state constant is observable in
 * +0x20 and the format string is not, so a pair with their strings swapped
 * would pass every object comparison ever written.  `t_v90p4dleaf` captures
 * the diagnostic transcript on both sides and compares it.
 *
 * The message prints `countInState` BEFORE it is cleared -- the blob loads
 * 0x24(%ebx) into %eax ahead of the call and stores the zero after it -- so
 * the value that reaches the log is how long the outgoing state lasted.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define SF_OFF_P4D(cls, field, off, tag) \
	typedef char sf_off_p4d_##tag[ \
	    ((int)__builtin_offsetof(cls, field) == (off)) ? 1 : -1]
SF_OFF_P4D(V90Phase4Demodulator, sessionFlag, 0x0000, p4d_flag);
SF_OFF_P4D(V90Phase4Demodulator, phase4Modulator, 0x0050, p4d_mod);
#undef SF_OFF_P4D
#endif

void
V90Phase4Demodulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
	phase4Modulator.setSessionFlag(flag);
}

void
V90Phase4Demodulator::enterWaitForCP()
{
	edprintf("V90Phase4Demodulator: enter WaitForV90CP state @ %d\r\n",
		 countInState);
	state = P4D_STATE_WAIT_FOR_V90CP;
	countInState = 0;
}

void
V90Phase4Demodulator::enterWaitForMP()
{
	edprintf("V90Phase4Demodulator: enter WaitForMP state @ %d\r\n",
		 countInState);
	state = P4D_STATE_WAIT_FOR_MP;
	countInState = 0;
}

void
V90Phase4Demodulator::enterWaitForEd()
{
	edprintf("V90Phase4Demodulator: enter WaitForEd state @ %d\r\n",
		 countInState);
	state = P4D_STATE_WAIT_FOR_ED;
	countInState = 0;
}

/*
 * `resetRRNDetector` -- 81 bytes at 0x25c40.
 *
 * THE ONLY THING IN THIS BATCH THAT DEREFERENCES `V90Parameters`, and the
 * field it reads is named rather than an offset:
 *
 *     25c51:  8b 53 04         mov 0x4(%ebx),%edx        ; this->params
 *     25c60:  8b 82 98 02 ..   mov 0x298(%edx),%eax      ; +0x298
 *
 * and V90Parameters.h already calls +0x298 `RRN_R_DETECTION_LENGTH`, which
 * is the strongest agreement a member called `resetRRNDetector` could have
 * asked for.  That is why this file includes the definition rather than the
 * forward declaration the header carries.
 *
 * THE THREE CONSTANTS ARE LEFT AS NUMBERS.  `V90RDetector::reset` rounds its
 * first argument down to multiples of 6 and 12 and its second the same way,
 * so 0x18, 0xb4 and 0xc are sample counts -- but which detector is the R one
 * and which the Rf one is a question about `rDetector1`/`rDetector2`, whose
 * roles this header has never established (see its own comment at +0x2ffc).
 * Naming them would be inventing that answer.
 */
void
V90Phase4Demodulator::resetRRNDetector()
{
	rDetector1.reset(params->RRN_R_DETECTION_LENGTH, 0x18);
	rDetector2.reset(0xb4, 0xc);
}

/*
 * `detectRRN` -- 143 bytes at 0x25ca0.
 *
 * Answers 0 until `rDetector1` says it has seen Rd, and on the sample that
 * it does, moves to state 9 and reports the detector's polarity.
 *
 * THE `sessionFlag == 0` ARM IS THE INTERESTING HALF.  `mov (%esi),%ebx ;
 * test %ebx,%ebx ; jne` reads +0x00, which is the field `setSessionFlag`
 * writes and which the constructor takes as its eleventh argument, and the
 * pair +0x38/+0x3c is set to 1,1 only when it is zero -- the same pair, and
 * the same values, that `resetBeforRRN` sets unconditionally.
 *
 * THE LOCAL POINTER IS THE OBJECT'S, AND IT WAS MEASURED RATHER THAN
 * PREFERRED.  Written the obvious way -- `rDetector1.detectR(sample)` and
 * then `rDetector1.polarity` -- this compiles to 123 bytes with one
 * callee-saved register: GCC re-derives the field address as `0x3020(%ebx)`
 * off `this` instead of keeping `&rDetector1` live across the call.  The
 * blob is 143 bytes, holds `this` in %esi and `&rDetector1` in %ebx, and
 * reads the polarity as `0x24(%ebx)` -- one address expression used twice,
 * which is what a local pointer gives and what two independent member
 * accesses do not.  With the pointer the function is byte-for-byte the
 * blob's, 0x8f bytes and 35 instructions with the same operands.
 *
 * This is NOT register allocation being chased (CLAUDE.md's free column):
 * the two spellings put a different expression tree in front of the
 * compiler, the difference is twenty bytes and a whole extra callee-save,
 * and the acceptance test is finding F617's full-text identity.  `detectFPE`
 * below is the control -- its two detector references are to DIFFERENT
 * objects, so no single pointer could serve both, and it is byte-identical
 * written the obvious way.  Finding F4321.
 */
int
V90Phase4Demodulator::detectRRN(short sample)
{
	V90RDetector *rd = &rDetector1;

	if (!rd->detectR(sample))
		return 0;

	edprintf("V90Phase4Demodulator: Rd detected, polarity = %d\r\n",
		 rd->polarity);
	state = P4D_STATE_RD_DETECTED;
	countInState = 0;
	int_0028 = 0;
	if (sessionFlag == 0) {
		int_0038 = 1;
		int_003c = 1;
	}
	return 1;
}

/*
 * `resetBeforRRN` -- 37 bytes at 0x25d30.  The spelling is the blob's.
 *
 * FIVE STORES AND NOTHING ELSE, AND THE ORDER IS THE OBJECT'S: +0x38, +0x3c,
 * +0x44, +0x48 and the byte at +0x30 LAST, not ascending by offset.  For a
 * body that is nothing but stores the object's order is a testable
 * hypothesis about the source's, and the acceptance test is finding F617's --
 * full-text identity of the disassembly, operands included -- not "the same
 * mnemonics".  Writing it ascending gives the same five instructions in a
 * different order and fails that test.
 */
void
V90Phase4Demodulator::resetBeforRRN()
{
	int_0038 = 1;
	int_003c = 1;
	int_0044 = 0;
	int_0048 = 0;
	uchar_0030 = 0;
}

/*
 * `trn2dKnownDemod` -- 170 bytes at 0x25de0, between `resetBeforRRN` and
 * `detectFPE` in the blob as here.
 *
 * The known TRN2d symbol, re-derived: run the EMBEDDED modulator's own
 * generator one symbol forward, split the result into sign and magnitude,
 * put the magnitude back through the companding law in force, and read the
 * learned level for (phase, code) out of the impairment detector's
 * `linMapp`.  The SHORT ARGUMENT IS NEVER READ -- no instruction touches
 * 0x24(%esp) -- so it is unnamed, exactly like the K56 stubs' parameters.
 *
 * FOUR ENCODINGS THE INSTRUCTIONS FORCE, all reproduced:
 *
 *   - the magnitude goes through a SHORT intermediate and is
 *     absolute-valued AGAIN inside each arm (`movswl %ax,%ebx` at +0x35,
 *     then `sar/xor/sub` at +0x58 and +0x9a) -- observable only at
 *     generateSymbol() == -32768, where the double abs hands the compander
 *     32768 rather than -32768;
 *   - the law test is a SIXTEEN-BIT compare (`cmpw $0x0,0xa95c`) of the
 *     four-byte `pcmType`, hence the `(short)` cast: an upper half left
 *     non-zero reads as mu-law here and as A-law to a `cmpl`;
 *   - the A-law code is `linear2alaw(..) ^ 0xd5` and the mu-law code
 *     `0xff - linear2ulaw(..)` widened through a short -- the same
 *     complement pair `resetDILGenerator` documents;
 *   - the table read is FLAT, `idx * 128 + code` off `linMapp[0]`, and the
 *     code can exceed 127 (it is a full eight-bit value), so the flat
 *     spelling is kept rather than a two-subscript one that would claim the
 *     row bounds it.  `movzwl`, so the level is read unsigned; the product
 *     against the +/-1 sign is truncated to short by the CALLEE
 *     (`movswl %di,%eax`), which is what makes the return `int`.
 */
int
V90Phase4Demodulator::trn2dKnownDemod(short)
{
	short gen = (short)phase4Modulator.generateSymbol();
	short sign = (short)((gen < 0) ? -1 : 1);
	short mag = (short)((gen < 0) ? -gen : gen);
	unsigned int idx = (countInState - 1u) % 6u;
	const unsigned short *tab =
	    (const unsigned short *)autoDigitalImpDetector->linMapp;
	int code;
	unsigned short level;

	if ((short)autoDigitalImpDetector->pcmType != 0)
		code = linear2alaw((mag < 0) ? -mag : mag) ^ 0xd5;
	else
		code = (short)(0xff - linear2ulaw((mag < 0) ? -mag : mag));

	level = tab[idx * 128u + (unsigned int)code];
	return (short)(sign * level);
}

/*
 * `detectFPE` -- 115 bytes at 0x25d60.
 *
 * IT PRINTS THE OTHER DETECTOR'S POLARITY, and that is the blob's:
 *
 *     25d6d:  8d 83 28 30 ..   lea 0x3028(%ebx),%eax   ; rDetector2
 *     25d7a:  call             V90RDetector::detectRf
 *     25d90:  8b 8b 20 30 ..   mov 0x3020(%ebx),%ecx   ; 0x2ffc + 0x24
 *
 * -- the detection runs on `rDetector2` at +0x3028 and the "%d" comes from
 * +0x3020, which is `rDetector1.polarity`.  `rDetector2.polarity` would be
 * +0x304c.  Both are `lea`/`mov` off the same base in the same 22
 * instructions, so this is not a misread of which object is which; it is a
 * copy of `detectRRN` whose second reference was not updated.  Finding F4320.
 * A test that seeded the two detectors alike could not see it, so
 * `t_v90p4dleaf` gives them different polarities.
 *
 * THE SECOND MESSAGE TAKES NO ARGUMENT and has no "\r\n".  Two separate
 * `edprintf` calls, not one string: 0x65e4 then 0x6618.
 */
int
V90Phase4Demodulator::detectFPE(short sample)
{
	if (!rDetector2.detectRf(sample))
		return 0;

	edprintf("V90Phase4Demodulator: Rf detected, polarity = %d\r\n",
		 rDetector1.polarity);
	edprintf("V90Phase4Demodulator: enter FPE !");
	state = P4D_STATE_FPE;
	countInState = 0;
	int_0028 = 0;
	return 1;
}

/*
 * ===========================================================================
 * THE THREE DECISION MEMBERS.
 *
 * `getDecision` at 0x27780 (52 bytes), `getV90Decision` at 0x25ea0 (3,095)
 * and `getV92Decision` at 0x26ac0 (3,252).  One sample in, one soft decision
 * out, and the whole phase 4 state machine in between.
 *
 * THE TWO ARE NOT TWINS, AND THE ASYMMETRY IS THE POINT.  `getV90Decision`
 * reaches `V90MP` -- `bitsToInfo`, `reset`, `printNofRecievedMpMpNot` -- and
 * never touches `V90CP`; `getV92Decision` reaches `V90CP` and never touches
 * `V90MP`.  A relocation scan of the whole object finds `getV92Decision` is
 * the ONLY caller of `V90CP::bitsToInfo` anywhere.  Beyond that, V.92 has
 * five arms V.90 does not have any form of -- the `WaitForCPu` state and its
 * five-way switch on the CP decoder -- and V.90 has two, the MP arms, that
 * V.92 sends straight to the exit.  Eleven of the eighteen jump-table entries
 * do differ in body, so the file writes both out rather than sharing a helper
 * that would have to be parameterised eleven ways.
 *
 * THE RETURNED DECISION IS READ UNINITIALISED ON FIVE OF THE EIGHTEEN ARMS,
 * and that is the object's and not a slip here.  Both functions build the
 * answer in %edi, and %edi is never written on the paths that reach the
 * epilogue from states 4 and 0x10 (V.90), states 5 and 6 (V.92), or from the
 * out-of-range `ja` -- so what comes back is whatever the CALLER left in the
 * register.  A `short decision;` with no initialiser is what puts that in
 * front of the compiler and it is what GCC 3.4.2 reproduces.  Deviation D600
 * carries it and D321 is the same shape one class along in
 * `V90Phase3Demodulator`, and `t_v90p4ddec` asserts the OBJECT and the transcript on
 * those arms and never the return value, because there is no value there to
 * agree about.
 *
 * THE SWITCH IS OVER 0..0x11 IN BOTH, `cmp $0x11,%eax` then `ja`, then an
 * eighteen-entry jump table at .rodata:0x8dc (V.90) and 0x924 (V.92).  The
 * range test is unsigned, which `state` being an enum pinned to `int` does
 * not give on its own -- `P4D_STATE_MAX` and the cast below are what do.
 * ===========================================================================
 */

/*
 * `fsqrt` and log10 on the coprocessor, as in the reference.
 *
 * The square root uses the ordinary builtin with -fno-math-errno, retaining
 * the instruction's negative-input behavior.  The ordinary log10l call expands
 * through the period compiler/math header under the C++ source fast-math flags.
 * These do not uniquely recover the original flags; modern portability is a
 * separate, forthcoming issue.  See docs/issue19-inline-asm.md.
 */
/*
 * THE FLOAT-AS-`%c%d.%0Nd` TRIPLE, the same three helpers `V92Transmitter`
 * and `V90ConstellationDesigner` carry, and for the same reason: the object
 * has no float conversion in `edprintf` and prints every real number as a
 * sign character, a whole part and a scaled fraction.
 *
 *   sign    `fldz` then an ordered compare then `sbb`/`and $-2`/`add $0x2d`,
 *           so '+' when the value is strictly above zero and '-' otherwise.
 *   whole   `fabs` then a TRUNCATING `fistpl` -- magnitude toward zero.
 *   frac    the value less its truncation, scaled, truncated, then integer
 *           `abs`.  The `abs` is what makes the order of the two conversions
 *           unobservable (finding F256).
 */
static char
p4d_sign_of(float v)
{
	return (0.0f < v) ? '+' : '-';
}

static int
p4d_whole_of(long double v)
{
	return (int)fabsl(v);
}

static int
p4d_frac4_of(long double v)
{
	return __builtin_abs((int)((v - (long double)(int)v) * 10000.0f));
}

static int
p4d_frac8_of(long double v)
{
	return __builtin_abs((int)((v - (long double)(int)v) * 100000000.0f));
}

/*
 * ONE FRAME IS SIX SAMPLES, and the three silence states step on frame
 * boundaries: each tests `countInState % 6 == 0` before acting, which the
 * object encodes as the unsigned 0xaaaaaaab reciprocal.  Spelled out because
 * it is the same six in six places and because `V90DEMAPPER_FRAME` is the
 * demapper's own constant for the same thing and this class does not include
 * it for that purpose.
 */
#define P4D_FRAME	6u

/*
 * `getDecision` -- 52 bytes at 0x27780.
 *
 * The whole body is `mov (%edx),%ecx ; test %ecx,%ecx ; je`, which is +0x00,
 * `sessionFlag`.  Non-zero picks V.92.
 *
 * THE `cwtl` AFTER EACH CALL IS THE ONLY EVIDENCE FOR ANY OF THE THREE RETURN
 * TYPES, and the header says so at the declaration: a caller widens only what
 * the callee left narrow.
 */
int
V90Phase4Demodulator::getDecision(short sample)
{
	if (sessionFlag != 0)
		return getV92Decision(sample);

	return getV90Decision(sample);
}

/*
 * `getV90Decision` -- 3,095 bytes at 0x25ea0.
 *
 * TWELVE OF THE EIGHTEEN STATES HAVE A BODY HERE.  4 and 0x10 fall to the
 * exit and are absent from the switch, which is what the jump table says: the
 * two entries point at the same block as the out-of-range `ja`.
 *
 * THE TWO ARMS THAT NAME A DETECTOR'S POLARITY HOLD A LOCAL POINTER, and
 * that is finding F4321 again rather than register allocation being chased.
 * `detectR` and then `rDetector1.polarity` written as two independent member
 * accesses makes GCC re-derive the field address off `this`; the object keeps
 * `&rDetector1` live across the call and reads `0x24(%ebx)`, which is one
 * address expression used twice.  The arms that do NOT print a polarity --
 * every `detectRNot` one -- take the address once and are written the obvious
 * way, which is the same control `detectFPE` provides for the leaves.
 *
 * THE SIGN OF THE TWO ENERGY LINES IS TAKEN FROM THE ENERGY AND THE MAGNITUDE
 * FROM ITS SQUARE ROOT.  `fldz ; fcomps 0x350c(%esi)` compares the FIELD,
 * while the `%d.%04d` pair comes from `fsqrt` of it -- at both sites here and
 * at both in `getV92Decision`, so it is a property of the original's own
 * print idiom and not an accident of one line.  Finding F4803.
 */
short
V90Phase4Demodulator::getV90Decision(short sample)
{
	short decision;

	countInState++;
	int_0028 = 0;

	switch (state) {
	/* Ri.  The polarity arm holds a local pointer; see above. */
	case P4D_STATE_WAIT_FOR_RI: {
		V90RDetector *rd = &rDetector1;

		decision = sample;
		if (rd->detectR(sample)) {
			edprintf("V90Phase4Demodulator: Ri detected @ %d, "
				 "polarity = %d\r\n", countInState, rd->polarity);
			state = P4D_STATE_WAIT_FOR_RI_NOT;
			countInState = 0;
			int_0028 = 0x16;
		}
		break;
	}

	/* RiNot, and the two study lengths it chooses between. */
	case P4D_STATE_WAIT_FOR_RI_NOT:
		decision = sample;
		if (rDetector1.detectRNot(sample)) {
			edprintf("V90Phase4Demodulator: RiNot detected @ %d, "
				 "enter TRN2dKnownData state\r\n", countInState);
			state = P4D_STATE_TRN2D_KNOWN_DATA;
			countInState = 0;
			int_0028 = 0x17;
			if (quickConnect != 0) {
				demapper->resetLinearMappStudy(0x960);
				linearMappStudyStart = 0x258;
			} else {
				demapper->resetLinearMappStudy(0x1c20);
				linearMappStudyStart = 0x7d0;
			}
		}
		break;

	/*
	 * TRN2dKnownData IS ONE SAMPLE LONG.  Its entry sets the next state
	 * and then falls into it -- `movl $0x3,0x20(%esi)` at 0x25ed5 with
	 * the next instruction the head of state 3's body, and the jump table
	 * pointing at both -- so the sample that arrives in state 2 is
	 * demodulated by state 3's code, not deferred.
	 */
	case P4D_STATE_TRN2D_KNOWN_DATA:
		state = P4D_STATE_TRN2D_DD;
		/* FALLTHROUGH */
	case P4D_STATE_TRN2D_DD:
		decision = demapper->hardDecision(sample);
		if (countInState == linearMappStudyStart) {
			demapper->linearMappStudyEnabled = 1;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Phase4Demodulator " "reset & enable linear "
						     "mapping study in " "TRN2.\n");
		}
		if (demapper->linearMappStudyEnabled != 0)
			demapper->linearMappingStudy(sample, decision);
		demapper->process(bits, nbits);
		if (countInState == trn2dDDLength / 2)
			int_0028 = 0x18;
		if (countInState == trn2dDDLength) {
			int_0028 = 0x19;
			demapper->linearMappStudyEnabled = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Phase4Demodulator: " "disable linear mapping "
						     "study\n");
		}
		break;

	case P4D_STATE_WAIT_FOR_MP: {
		unsigned int i;

		decision = demapper->hardDecision(sample);
		if (demapper->process(bits, nbits))
			for (i = 0; i < nbits; i++) {
				int info = mp->bitsToInfo(
					descrambler->process(bits[i]));

				if (info == 1) {
					int_0028 = 0x1a;
					edprintf("V90Phase4Demodulator: MP " "detected @ %d\r\n",
						 countInState);
				} else if (info == 2) {
					int_0028 = 0x1b;
					edprintf("V90Phase4Demodulator: MPnot " "detected @ %d\r\n",
						 countInState);
				}
			}
		break;
	}

	case P4D_STATE_WAIT_FOR_ED: {
		unsigned int i;

		decision = demapper->hardDecision(sample);
		if (demapper->process(bits, nbits))
			for (i = 0; i < nbits; i++) {
				int info = mp->bitsToInfo(
					descrambler->process(bits[i]));

				if (info == 2) {
					int_0028 = 0x1b;
					/*
					 * THE ONE SITE IN EITHER FUNCTION AT
					 * LEVEL 3.  `cmpl $0x2` and not
					 * `cmpl $0x1`, so it needs one more
					 * than every other gate here --
					 * exactly the distinction finding F150
					 * says a single macro would flatten.
					 */
					if (DSPLIB_DEBUG_VERBOSE())
						dsplibs_debug_printf(
							"V90Phase4Demodulator:" " MPnot detected on " "WaitForEd @ %d\r\n",
							countInState);
				} else if (info == 3 &&
					   state == P4D_STATE_WAIT_FOR_ED) {
					int_0028 = 0x1c;
					mp->printNofRecievedMpMpNot();
					if (connectionEvaluator->word_90 != 0 &&
					    int_003c != 0 && int_0038 != 0) {
						edprintf("V90Phase4Demodulator:" " Ed detected @ %d, "
							 "enter wait for Rt " "state\r\n",
							 countInState);
						countInState = 0;
						errorEnergyBeforeEC = 0.0f;
						state = P4D_STATE_SILENCE;
						resetRRNDetector();
					} else {
						edprintf("V90Phase4Demodulator:" " Ed detected @ %d, "
							 "enter B1d state\r\n",
							 countInState);
						state = P4D_STATE_B1D;
						countInState = 0;
						demapper->resetNoSpectral(
							mappingParams2);
						b1dZeros = 0;
						b1dBits = 0;
					}
				}
			}
		break;
	}

	/*
	 * B1d.  Every bit `process` hands back is descrambled and counted,
	 * and the zeros among them are counted once the count has passed
	 * `3 * mappingParams2->word_0 + 0x17` -- the "(after delay)" of the
	 * termination message.  At 0x120 samples the phase ends and the bit
	 * error ratio is reported.
	 */
	case P4D_STATE_B1D: {
		unsigned int i;

		decision = demapper->hardDecision(sample);
		if (demapper->process(bits, nbits))
			for (i = 0; i < nbits; i++) {
				int bit = descrambler->process(bits[i]);

				b1dBits++;
				if (bit == 0 &&
				    b1dBits > 3 * mappingParams2->word_0 + 0x17)
					b1dZeros++;
			}
		if (countInState == 0x120) {
			unsigned int n;

			edprintf("V90Phase4Demodulator: Phase4 Terminated @ "
				 "%d,  nof B1d bits = %d,  B1d Zeros (after "
				 "delay) = %d\r\n", countInState, b1dBits,
				 b1dZeros);
			n = b1dBits - 3 * mappingParams2->word_0 - 0x17;
			edprintf("V90Phase4Demodulator: B1d BER = " "%c%d.%08d\r\n",
				 p4d_sign_of((float)((long double)b1dZeros /
						     (long double)n)),
				 p4d_whole_of((long double)b1dZeros /
					      (long double)n),
				 p4d_frac8_of((long double)b1dZeros /
					      (long double)n));
			int_0028 = 0x1d;
			state = P4D_STATE_TERMINATED;
			countInState = 0;
			int_003c = 0;
		}
		break;
	}

	/*
	 * Terminated, and whatever 0x11 is.  Both arms of the jump table
	 * point at the same three instructions: zero the answer and leave.
	 * The header says why 0x11 has no name.
	 */
	case P4D_STATE_TERMINATED:
	case P4D_STATE_UNNAMED_11:
		decision = 0;
		break;

	/*
	 * TRN2d DD -- the Rd detection that ends it.  The detector runs on
	 * the DECISION and not on the sample, which is what tells this arm
	 * apart from the three that look like it.
	 */
	case P4D_STATE_RD_DETECTED:
		decision = demapper->hardDecision(sample);
		demapper->process(bits, nbits);
		if (rDetector1.detectRNot(decision)) {
			edprintf("V90Phase4Demodulator: RdNot detected @ %d, "
				 "enter TRN2d DD state\r\n", countInState);
			demapper->resetLinearMappStudy(0x1c20);
			countInState = 0;
			linearMappStudyStart = 0x7d0;
			state = P4D_STATE_TRN2D_DD;
			int_0028 = 0x2c;
			trn2dDDLength = params->RRN_TRN2D_DD_LENGTH;
			mp->reset();
			mp->groupSize = mappingParams1->word_0;
			demapper->resetNoSpectral(mappingParams1);
		}
		break;

	/*
	 * The three silence states, in the order the receiver walks them:
	 * wait, measure the echo before cancellation, wait for the canceller,
	 * measure it after, and report the ratio in dB.
	 */
	case P4D_STATE_SILENCE:
		decision = sample;
		demapper->incrementRBSFramePosition();
		if (countInState >= (unsigned int)
				    params->RRN_SILENCE_WAIT_BEFORE_ECHO_CALC &&
		    countInState % P4D_FRAME == 0) {
			edprintf("V90Phase4Demodulator: entering "
				 "CalcErrorEnergyBeforeEchoCancellation state " "@ %d\r\n", countInState);
			state = P4D_STATE_CALC_ENERGY_BEFORE_EC;
			countInState = 0;
		}
		break;

	/* The echo measurement before cancellation. */
	case P4D_STATE_CALC_ENERGY_BEFORE_EC: {
		float energy;

		decision = sample;
		demapper->incrementRBSFramePosition();
		energy = errorEnergyBeforeEC + decision * decision;
		if (countInState >= (unsigned int)
				    params->RRN_SILENCE_ECHO_CALC_PERIOD &&
		    countInState % P4D_FRAME == 0) {
			errorEnergyBeforeEC = energy / countInState;
			edprintf("V90Phase4Demodulator: entering "
				 "WaitForEchoCancellation state @ %d\r\n",
				 countInState);
			edprintf("V90Phase4Demodulator: error energy before "
				 "echo cancellation  = %c%d.%04d\r\n",
				 p4d_sign_of(errorEnergyBeforeEC),
				 p4d_whole_of(sqrt(
					 (long double)errorEnergyBeforeEC)),
				 p4d_frac4_of(sqrt(
					 (long double)errorEnergyBeforeEC)));
			state = P4D_STATE_WAIT_FOR_ECHO_CANCEL;
			countInState = 0;
		} else {
			errorEnergyBeforeEC = energy;
		}
		break;
	}

	/* Waiting for the canceller to settle. */
	case P4D_STATE_WAIT_FOR_ECHO_CANCEL:
		decision = sample;
		demapper->incrementRBSFramePosition();
		if (countInState >= (unsigned int)
				    (params->RRN_SILENCE_SCR_LENGTH -
				     3 * params->RRN_SILENCE_ECHO_CALC_PERIOD) &&
		    countInState % P4D_FRAME == 0) {
			edprintf("V90Phase4Demodulator: entering "
				 "CalcErrorEnergyAfterEchoCancellation state " "@ %d\r\n", countInState);
			state = P4D_STATE_CALC_ENERGY_AFTER_EC;
			countInState = 0;
			errorEnergyAfterEC = 0.0f;
		}
		break;

	/* The measurement after cancellation, and the dB ratio. */
	case P4D_STATE_CALC_ENERGY_AFTER_EC: {
		float energy;

		decision = sample;
		demapper->incrementRBSFramePosition();
		energy = errorEnergyAfterEC + decision * decision;
		if (countInState >= (unsigned int)
				    params->RRN_SILENCE_ECHO_CALC_PERIOD &&
		    countInState % P4D_FRAME == 0) {
			float ratio;
			float dB;

			errorEnergyAfterEC = energy / countInState;
			edprintf("V90Phase4Demodulator: entering WaitForRt "
				 "state @ %d\r\n", countInState);
			edprintf("V90Phase4Demodulator: error energy after "
				 "echo cancellation  = %c%d.%04d\r\n",
				 p4d_sign_of(errorEnergyAfterEC),
				 p4d_whole_of(sqrt(
					 (long double)errorEnergyAfterEC)),
				 p4d_frac4_of(sqrt(
					 (long double)errorEnergyAfterEC)));
			ratio = 1.0f / errorEnergyAfterEC * errorEnergyBeforeEC;
			dB = (float)(10.0f *
				     log10l((long double)ratio));
			edprintf("V90Phase4Demodulator: silence SCR echo "
				 "energy [dB]  = %c%d.%04d\r\n",
				 p4d_sign_of(dB), p4d_whole_of(dB),
				 p4d_frac4_of(dB));
			int_0028 = 0x2a;
			state = P4D_STATE_WAIT_FOR_RT;
			/*
			 * THE PARAMETER IS THE LEFT OPERAND, and that is
			 * measured rather than preferred.  The object is
			 * `flds 0x10(%esp) ; flds 0x404(%ebx) ; fcompp ;
			 * setb %dl` -- one ORDERED compare with no parity
			 * test, so an unordered result leaves CF set and the
			 * flag comes out 1.  Written `dB > PARAM` it comes
			 * out 0 there instead, and `dB` IS unordered whenever
			 * the two energies have opposite signs, because then
			 * the ratio is negative and `fyl2x` answers a NaN.
			 * `t_v90p4ddec` seeds a negative accumulator on a
			 * third of its trials and caught it; findings F2300,
			 * F2301 and F4812.
			 */
			int_3510 =
			    (params->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE
			     < dB);
			countInState = 0;
		} else {
			errorEnergyAfterEC = energy;
		}
		break;
	}

	/* Rt.  V.90 records no progress code here and V.92 does. */
	case P4D_STATE_WAIT_FOR_RT: {
		V90RDetector *rd = &rDetector1;

		decision = sample;
		demapper->incrementRBSFramePosition();
		if (rd->detectR(sample)) {
			edprintf("V90Phase4Demodulator: Rt detected @ %d, "
				 "polarity = %d\r\n", countInState, rd->polarity);
			state = P4D_STATE_WAIT_FOR_RT_NOT;
			countInState = 0;
			int_0038 = 0;
		}
		break;
	}

	/* RtNot, and the WaitForMP entry inlined into it. */
	case P4D_STATE_WAIT_FOR_RT_NOT:
		decision = sample;
		demapper->incrementRBSFramePosition();
		if (rDetector1.detectRNot(sample)) {
			edprintf("V90Phase4Demodulator: RtNot detected @ " "%d\r\n", countInState);
			enterWaitForMP();
			int_0028 = 0x28;
			mp->reset();
			mp->groupSize = mappingParams1->word_0;
			edprintf("V90Phase4Demodulator: No reset to demapper, "
				 "current Phase - %d\r\n",
				 demapper->rbsFramePosition);
		}
		break;

	default:
		break;
	}

	return decision;
}

/*
 * `getV92Decision` -- 3,252 bytes at 0x26ac0.
 *
 * THE SAME EIGHTEEN-WAY SWITCH ON THE SAME FIELD, AND ELEVEN DIFFERENT ARMS.
 * What it shares with `getV90Decision` is the frame -- increment, clear
 * +0x28, dispatch, return %edi -- and the silence chain, which is the same
 * five states doing the same arithmetic on the same two floats.  What it does
 * not share:
 *
 *   - `V90CP` everywhere `getV90Decision` has `V90MP`.  This function is the
 *     ONLY caller of `V90CP::bitsToInfo` in the whole object.
 *   - state 0x10, the FPE state, which does nothing at all in V.90 and here
 *     runs the RfNot detection on `rDetector2` and enters WaitForCPu.
 *   - state 4 has a body.  In V.90 it is a state the receiver sits in and
 *     this function's jump table sends it to a five-way switch on the CP
 *     decoder, which is where the four flags at +0x3c/+0x40/+0x44/+0x48 and
 *     the byte at +0x30 are read.
 *   - states 5 and 6, the two MP states, fall to the exit.
 *   - the Rt arm sets +0x28 to 0x27 and the V.90 one sets nothing, and the
 *     RtNot arm does not print the WaitForMP message, so it is not
 *     `enterWaitForMP` inlined the way V.90's is.
 *
 * THE CP SWITCH IS `cmp $0x5,%eax ; ja` AND A SIX-ENTRY TABLE at
 * .rodata:0x96c -- six, not eighteen: entry six onwards belongs to a
 * different function, which is why counting the table by the gap to the next
 * one gets it wrong.  Answer 0 has no arm and rejoins the loop.
 */
short
V90Phase4Demodulator::getV92Decision(short sample)
{
	short decision;

	countInState++;
	int_0028 = 0;

	switch (state) {
	/* Ri, and V.92's copy of it is V.90's sample for sample. */
	case P4D_STATE_WAIT_FOR_RI: {
		V90RDetector *rd = &rDetector1;

		decision = sample;
		if (rd->detectR(sample)) {
			edprintf("V90Phase4Demodulator: Ri detected @ %d, "
				 "polarity = %d\r\n", countInState, rd->polarity);
			state = P4D_STATE_WAIT_FOR_RI_NOT;
			countInState = 0;
			int_0028 = 0x16;
		}
		break;
	}

	/* RiNot, V.92's copy. */
	case P4D_STATE_WAIT_FOR_RI_NOT:
		decision = sample;
		if (rDetector1.detectRNot(sample)) {
			edprintf("V90Phase4Demodulator: RiNot detected @ %d, "
				 "enter TRN2dKnownData state\r\n", countInState);
			state = P4D_STATE_TRN2D_KNOWN_DATA;
			countInState = 0;
			int_0028 = 0x17;
			if (quickConnect != 0) {
				demapper->resetLinearMappStudy(0x960);
				linearMappStudyStart = 0x258;
			} else {
				demapper->resetLinearMappStudy(0x1c20);
				linearMappStudyStart = 0x7d0;
			}
		}
		break;

	/* TRN2dKnownData and TRN2d DD, V.92's copies. */
	case P4D_STATE_TRN2D_KNOWN_DATA:
		state = P4D_STATE_TRN2D_DD;
		/* FALLTHROUGH */
	case P4D_STATE_TRN2D_DD:
		decision = demapper->hardDecision(sample);
		if (countInState == linearMappStudyStart) {
			demapper->linearMappStudyEnabled = 1;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Phase4Demodulator " "reset & enable linear "
						     "mapping study in " "TRN2.\n");
		}
		if (demapper->linearMappStudyEnabled != 0)
			demapper->linearMappingStudy(sample, decision);
		demapper->process(bits, nbits);
		if (countInState == trn2dDDLength / 2)
			int_0028 = 0x18;
		if (countInState == trn2dDDLength) {
			int_0028 = 0x19;
			demapper->linearMappStudyEnabled = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Phase4Demodulator: " "disable linear mapping "
						     "study\n");
		}
		break;

	/*
	 * WaitForCPu.  Every descrambled bit goes to the CP decoder and its
	 * answer picks one of five arms; 3 and 4 differ only in where the
	 * `int_003c` test sits relative to the byte at +0x30 and in the four
	 * progress codes, which is the sort of near-duplicate a shared helper
	 * would hide.
	 */
	case P4D_STATE_WAIT_FOR_V90CP: {
		unsigned int i;

		decision = demapper->hardDecision(sample);
		if (demapper->process(bits, nbits))
			for (i = 0; i < nbits; i++)
				switch (cp->bitsToInfo(
						descrambler->process(bits[i]))) {
				case 1:
					int_0028 = 0x2d;
					edprintf("V90Phase4Demodulator: CP " "detected @ %d\r\n",
						 countInState);
					break;

				case 2:
					int_0028 = 0x2e;
					edprintf("V90Phase4Demodulator: CPnot " "detected @ %d\r\n",
						 countInState);
					break;

				case 3:
					uchar_0030 = 1;
					if (int_003c == 0) {
						int_0028 = 0x2f;
						break;
					}
					uint_004c = cp->word_ca0;
					if (int_0040 == 0 && uint_004c == 0) {
						int_0028 = 0x2f;
						break;
					}
					if (int_0048 != 0) {
						int_0028 = 0x32;
						break;
					}
					int_0044 = 1;
					int_0028 = 0x31;
					break;

				case 4:
					if (int_003c == 0) {
						int_0028 = 0x30;
						break;
					}
					uchar_0030 = 1;
					uint_004c = cp->word_ca0;
					if (int_0040 == 0 && uint_004c == 0) {
						int_0028 = 0x30;
						break;
					}
					if (int_0048 != 0) {
						int_0028 = 0x34;
						break;
					}
					int_0044 = 1;
					int_0028 = 0x33;
					break;

				case 5:
					if (uchar_0030 == 0)
						break;
					if (int_003c != 0 && int_0044 != 0 &&
					    int_0048 == 0) {
						int_0028 = 0x35;
						edprintf("V90Phase4Demodulator:" " First Ed at RRN " "detected @ %d, "
							 "Silence state\r\n",
							 countInState);
						countInState = 0;
						errorEnergyBeforeEC = 0.0f;
						resetRRNDetector();
						int_0048 = 1;
						uchar_0030 = 0;
						state = uint_004c != 0
							? P4D_STATE_WAIT_FOR_RT
							: P4D_STATE_SILENCE;
						break;
					}
					int_0028 = 0x1c;
					edprintf("V90Phase4Demodulator: Ed " "detected @ %d, enter B1d "
						 "state\r\n", countInState);
					state = P4D_STATE_B1D;
					countInState = 0;
					demapper->resetNoSpectral(
						mappingParams2);
					b1dZeros = 0;
					b1dBits = 0;
					break;

				default:
					break;
				}
		break;
	}

	/* B1d, V.92's copy, and it reads the same mapping block. */
	case P4D_STATE_B1D: {
		unsigned int i;

		decision = demapper->hardDecision(sample);
		if (demapper->process(bits, nbits))
			for (i = 0; i < nbits; i++) {
				int bit = descrambler->process(bits[i]);

				b1dBits++;
				if (bit == 0 &&
				    b1dBits > 3 * mappingParams2->word_0 + 0x17)
					b1dZeros++;
			}
		if (countInState == 0x120) {
			unsigned int n;

			edprintf("V90Phase4Demodulator: Phase4 Terminated @ "
				 "%d,  nof B1d bits = %d,  B1d Zeros (after "
				 "delay) = %d\r\n", countInState, b1dBits,
				 b1dZeros);
			n = b1dBits - 3 * mappingParams2->word_0 - 0x17;
			edprintf("V90Phase4Demodulator: B1d BER = " "%c%d.%08d\r\n",
				 p4d_sign_of((float)((long double)b1dZeros /
						     (long double)n)),
				 p4d_whole_of((long double)b1dZeros /
					      (long double)n),
				 p4d_frac8_of((long double)b1dZeros /
					      (long double)n));
			int_0028 = 0x1d;
			state = P4D_STATE_TERMINATED;
			countInState = 0;
			int_003c = 0;
		}
		break;
	}

	case P4D_STATE_TERMINATED:
	case P4D_STATE_UNNAMED_11:
		decision = 0;
		break;

	case P4D_STATE_RD_DETECTED:
		decision = demapper->hardDecision(sample);
		demapper->process(bits, nbits);
		if (rDetector1.detectRNot(decision)) {
			edprintf("V90Phase4Demodulator: RdNot detected @ %d, "
				 "enter TRN2d DD state\r\n", countInState);
			demapper->resetLinearMappStudy(0x1c20);
			countInState = 0;
			linearMappStudyStart = 0x7d0;
			state = P4D_STATE_TRN2D_DD;
			int_0028 = 0x2c;
			trn2dDDLength = params->RRN_TRN2D_DD_LENGTH;
			cp->reset();
			cp->byte_13 = 0;
			cp->word_3ba8 = mappingParams1->word_0;
			uchar_0030 = 0;
			demapper->resetNoSpectral(mappingParams1);
		}
		break;

	/* The silence chain again, state for state as V.90's. */
	case P4D_STATE_SILENCE:
		decision = sample;
		demapper->incrementRBSFramePosition();
		if (countInState >= (unsigned int)
				    params->RRN_SILENCE_WAIT_BEFORE_ECHO_CALC &&
		    countInState % P4D_FRAME == 0) {
			edprintf("V90Phase4Demodulator: entering "
				 "CalcErrorEnergyBeforeEchoCancellation state " "@ %d\r\n", countInState);
			state = P4D_STATE_CALC_ENERGY_BEFORE_EC;
			countInState = 0;
		}
		break;

	/* The echo measurement before cancellation, V.92's copy. */
	case P4D_STATE_CALC_ENERGY_BEFORE_EC: {
		float energy;

		decision = sample;
		demapper->incrementRBSFramePosition();
		energy = errorEnergyBeforeEC + decision * decision;
		if (countInState >= (unsigned int)
				    params->RRN_SILENCE_ECHO_CALC_PERIOD &&
		    countInState % P4D_FRAME == 0) {
			errorEnergyBeforeEC = energy / countInState;
			edprintf("V90Phase4Demodulator: entering "
				 "WaitForEchoCancellation state @ %d\r\n",
				 countInState);
			edprintf("V90Phase4Demodulator: error energy before "
				 "echo cancellation  = %c%d.%04d\r\n",
				 p4d_sign_of(errorEnergyBeforeEC),
				 p4d_whole_of(sqrt(
					 (long double)errorEnergyBeforeEC)),
				 p4d_frac4_of(sqrt(
					 (long double)errorEnergyBeforeEC)));
			state = P4D_STATE_WAIT_FOR_ECHO_CANCEL;
			countInState = 0;
		} else {
			errorEnergyBeforeEC = energy;
		}
		break;
	}

	/* Waiting for the canceller, V.92's copy. */
	case P4D_STATE_WAIT_FOR_ECHO_CANCEL:
		decision = sample;
		demapper->incrementRBSFramePosition();
		if (countInState >= (unsigned int)
				    (params->RRN_SILENCE_SCR_LENGTH -
				     3 * params->RRN_SILENCE_ECHO_CALC_PERIOD) &&
		    countInState % P4D_FRAME == 0) {
			edprintf("V90Phase4Demodulator: entering "
				 "CalcErrorEnergyAfterEchoCancellation state " "@ %d\r\n", countInState);
			state = P4D_STATE_CALC_ENERGY_AFTER_EC;
			countInState = 0;
			errorEnergyAfterEC = 0.0f;
		}
		break;

	/* The measurement after cancellation and the dB report. */
	case P4D_STATE_CALC_ENERGY_AFTER_EC: {
		float energy;

		decision = sample;
		demapper->incrementRBSFramePosition();
		energy = errorEnergyAfterEC + decision * decision;
		if (countInState >= (unsigned int)
				    params->RRN_SILENCE_ECHO_CALC_PERIOD &&
		    countInState % P4D_FRAME == 0) {
			float ratio;
			float dB;

			errorEnergyAfterEC = energy / countInState;
			edprintf("V90Phase4Demodulator: entering WaitForRt "
				 "state @ %d\r\n", countInState);
			edprintf("V90Phase4Demodulator: error energy after "
				 "echo cancellation  = %c%d.%04d\r\n",
				 p4d_sign_of(errorEnergyAfterEC),
				 p4d_whole_of(sqrt(
					 (long double)errorEnergyAfterEC)),
				 p4d_frac4_of(sqrt(
					 (long double)errorEnergyAfterEC)));
			ratio = 1.0f / errorEnergyAfterEC * errorEnergyBeforeEC;
			dB = (float)(10.0f *
				     log10l((long double)ratio));
			edprintf("V90Phase4Demodulator: silence SCR echo "
				 "energy [dB]  = %c%d.%04d\r\n",
				 p4d_sign_of(dB), p4d_whole_of(dB),
				 p4d_frac4_of(dB));
			int_0028 = 0x2a;
			state = P4D_STATE_WAIT_FOR_RT;
			/*
			 * THE PARAMETER IS THE LEFT OPERAND, and that is
			 * measured rather than preferred.  The object is
			 * `flds 0x10(%esp) ; flds 0x404(%ebx) ; fcompp ;
			 * setb %dl` -- one ORDERED compare with no parity
			 * test, so an unordered result leaves CF set and the
			 * flag comes out 1.  Written `dB > PARAM` it comes
			 * out 0 there instead, and `dB` IS unordered whenever
			 * the two energies have opposite signs, because then
			 * the ratio is negative and `fyl2x` answers a NaN.
			 * `t_v90p4ddec` seeds a negative accumulator on a
			 * third of its trials and caught it; findings F2300,
			 * F2301 and F4812.
			 */
			int_3510 =
			    (params->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE
			     < dB);
			countInState = 0;
		} else {
			errorEnergyAfterEC = energy;
		}
		break;
	}

	/* Rt, and V.92 records a progress code where V.90 records none. */
	case P4D_STATE_WAIT_FOR_RT: {
		V90RDetector *rd = &rDetector1;

		decision = sample;
		demapper->incrementRBSFramePosition();
		if (rd->detectR(sample)) {
			edprintf("V90Phase4Demodulator: Rt detected @ %d, "
				 "polarity = %d\r\n", countInState, rd->polarity);
			int_0028 = 0x27;
			state = P4D_STATE_WAIT_FOR_RT_NOT;
			countInState = 0;
			int_0038 = 0;
		}
		break;
	}

	/* RtNot, and it does NOT print the WaitForMP message V.90 does. */
	case P4D_STATE_WAIT_FOR_RT_NOT:
		decision = sample;
		demapper->incrementRBSFramePosition();
		if (rDetector1.detectRNot(sample)) {
			edprintf("V90Phase4Demodulator: RtNot detected @ " "%d\r\n", countInState);
			state = P4D_STATE_WAIT_FOR_V90CP;
			countInState = 0;
			int_0028 = 0x28;
			cp->reset();
			cp->word_3ba8 = mappingParams1->word_0;
			edprintf("V90Phase4Demodulator: No reset to demapper, "
				 "current Phase - %d\r\n",
				 demapper->rbsFramePosition);
		}
		break;

	/*
	 * FPE, and the state that gives V.92 its extra detection.  The
	 * message calls the destination "WaitForCPu" where the V.90 side
	 * calls the same state 4 "WaitForV90CP"; both are the author's.
	 */
	case P4D_STATE_FPE:
		decision = demapper->hardDecision(sample);
		demapper->process(bits, nbits);
		if (rDetector2.detectRfNot(decision)) {
			edprintf("V90Phase4Demodulator: RfNot detected @ %d, "
				 "enter WaitForCPu state\r\n", countInState);
			state = P4D_STATE_WAIT_FOR_V90CP;
			countInState = 0;
			cp->reset();
			cp->byte_13 = 0;
			cp->word_3ba8 = mappingParams2->word_0;
			uchar_0030 = 0;
		}
		break;

	default:
		break;
	}

	return decision;
}

/*
 * ===========================================================================
 * `V90Phase4Demodulator::reset` -- .text+0x277c0, 504 bytes.
 *
 * THE WHOLE RECEIVER'S ENTRY POINT, and its closure is the batch: eleven
 * scalars, both `V90RDetector`s, one of the CP and the MP, the demapper, the
 * embedded modulator's own `reset` and `setMappingParams`, and then a loop
 * over the decision member `sessionFlag` selects.
 *
 * THE ORDER ACROSS THE CALLS IS THE OBJECT'S AND IS NOT FREE.  GCC cannot
 * move a store through `this` across an opaque call in either direction, so
 * the calls are fenceposts and the sequence below is what the object's
 * interleaving proves.  `linearMappStudyStart = 0` at +0x278cf sits BEFORE
 * the modulator's `reset` and after the demapper's, which is a real
 * constraint and not a preference.  Inside each run the schedule is GCC's.
 *
 * WHICH MESSAGE RECORD IS RESET IS THE SESSION'S, AND THE TWO ARMS ARE NOT
 * MIRROR IMAGES.  Under V.92 the CP is reset, its group size is taken from
 * `mappingParams1->word_0`, AND the same word is kept at +0x34fc; under V.90
 * the MP is reset and its group size taken the same way, and +0x34fc is left
 * exactly as it was found.  One store, on one arm -- 0x27885 against
 * 0x27995 -- and it is the only asymmetry between them.
 *
 * THE DEMAPPER'S RESET IS DOUBLY GUARDED and the ORDER of the two tests is
 * the object's: `test %ecx,%ecx` on `mappingParams1` at 0x27891 and only then
 * `test %eax,%eax` on `autoDigitalImpDetector` at 0x2789b.
 *
 * AND THE DETECTOR IS THEN DEREFERENCED UNCONDITIONALLY, which is the object
 * and not a defect here.  On the arm where the guard rejected, GCC threads
 * the jump straight past the reload at 0x278c0 into 0x278c6 -- it knows the
 * register already holds zero -- and 0x278d5's `mov 0xa95c(%eax),%ecx` reads
 * through it.  So the original's source guards the demapper call on a pointer
 * it then trusts; writing the guard any other way would emit the reload.
 *
 * `V90RDetector::reset` TAKES THE SAME PAIR TWICE.  Both detectors get
 * `(params->PHASE4_R_DETECTION_LENGTH, 0x18)`, and nothing in the source
 * tells them apart -- V90Phase4Demodulator.h says the same of the
 * constructor's two calls.
 *
 * THE MODULATOR IS RESET INTO TRN2d WITH NOTHING TO PUMP: state 3, trip count
 * zero and a fifth argument of zero, with the companding law taken from
 * `autoDigitalImpDetector->pcmType` and the code from this class's own
 * `ucode` -- which `reset` stored eight instructions earlier and reloads,
 * `movzbl 0x8(%esi)` at 0x278ec.
 *
 * TWO DIAGNOSTICS, AT TWO DIFFERENT GATES.  The `quickConnect` line is behind
 * `dsplibs_debug_level > 1` and comes FIRST; the `trn2dDDLength` line is an
 * unconditional `edprintf` and comes after the field it reports has been
 * assigned.  Between them they are what names both fields (CLAUDE.md's rule
 * 1); the parameter the flag selects, `TRN2D_QC_DD_LENGTH`, carries the same
 * abbreviation the string does.
 *
 * THE LOOP RELOADS `sessionFlag` EVERY ITERATION, `mov (%esi),%edi` at
 * 0x27958 inside the back edge, for the reason the modulator's own loop does:
 * either decision member may store through `this`.
 * ===========================================================================
 */
void
V90Phase4Demodulator::reset(unsigned char code, Phase4DemodulatorState st,
			    unsigned int nofSamples,
			    unsigned int quickConnectArg)
{
	unsigned int i;

	int_0028 = 0;
	int_3510 = 0;
	ucode = code;
	countInState = 0;
	int_0038 = 1;
	state = st;
	int_003c = 0;
	int_0040 = 0;
	int_0044 = 0;
	int_0048 = 0;
	uchar_0030 = 0;

	rDetector1.reset((unsigned int)params->PHASE4_R_DETECTION_LENGTH, 0x18);
	rDetector2.reset((unsigned int)params->PHASE4_R_DETECTION_LENGTH, 0x18);

	quickConnect = quickConnectArg;

	if (sessionFlag != 0) {
		/*
		 * ONE LOAD, TWO STORES, and the local is the object's rather
		 * than a tidying: `mov (%ecx),%edx` at 0x27883 feeds both
		 * 0x27885 and 0x2788b.  Written as two independent reads of
		 * `mappingParams1->word_0`, GCC reloads for the second --
		 * the store to `this->uint_34fc` is an `unsigned int` write
		 * that may alias an `unsigned int` read -- and emits an extra
		 * `mov (%ecx),%ebx`.  Measured, not assumed.
		 */
		unsigned int groupSize;

		cp->reset();
		groupSize = mappingParams1->word_0;
		uint_34fc = groupSize;
		cp->word_3ba8 = groupSize;
	} else {
		mp->reset();
		mp->groupSize = mappingParams1->word_0;
	}

	if (mappingParams1 != 0 && autoDigitalImpDetector != 0)
		demapper->reset(mappingParams1);

	linearMappStudyStart = 0;

	phase4Modulator.reset(autoDigitalImpDetector->pcmType, ucode,
			      P4M_STATE_TRN2D, 0, 0);
	phase4Modulator.setMappingParams(mappingParams1);

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("V90Phase4Demodulator: reset called, "
				     "quickConnect indication is %d\r\n",
				     quickConnect);

	trn2dDDLength = quickConnect != 0
	    ? (unsigned int)params->TRN2D_QC_DD_LENGTH
	    : (unsigned int)params->TRN2D_DD_LENGTH;
	edprintf("V90Phase4Demodulator: trn2dDDLength = %d symbols\r\n",
		 trn2dDDLength);

	for (i = 0; i < nofSamples; i++) {
		if (sessionFlag != 0)
			getV92Decision(0);
		else
			getV90Decision(0);
	}
}
