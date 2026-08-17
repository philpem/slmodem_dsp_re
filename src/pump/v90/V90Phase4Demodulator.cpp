/*
 * V90Phase4Demodulator.cpp -- the lifecycle pair, and the object map it came
 * from, asserted.
 *
 * `include/dsplib/V90Phase4Demodulator.h` carries the derivation: where each
 * of the fourteen fields came from, the three exact meetings that fix the
 * embedded subobjects, and the mapping-parameter swap the modulator sees.
 *
 * PLAIN CDECL, `this` AS THE FIRST STACK ARGUMENT (finding 215).  After
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

#include "dsplib/encode.h"
#include "dsplib/V90Parameters.h"
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
P4D_OFF(int_002c,		0x002c, i2c);
P4D_OFF(uchar_0030,		0x0030, u30);
P4D_OFF(uint_0034,		0x0034, u34);
P4D_OFF(int_0038,		0x0038, i38);
P4D_OFF(int_003c,		0x003c, i3c);
P4D_OFF(int_0040,		0x0040, i40);
P4D_OFF(int_0044,		0x0044, i44);
P4D_OFF(int_0048,		0x0048, i48);

/*
 * The enum is four bytes wide, which is what makes `state` a field at 0x20
 * and `countInState` one at 0x24 rather than two halves of one word.  GCC
 * 3.4.2 and GCC 13 both give it `int` here; the pin in the header is what
 * keeps that true under C++98's minimum-range rule.
 */
typedef char v90p4d_statesize[(sizeof(Phase4DemodulatorState) == 4) ? 1 : -1];

/*
 * The allocation, and finding 1107's point again: this number is
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
 * a crossing.  Finding 1301; the V.90 modulator batch derived the same swap
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
 * in that run and is deliberately absent: it calls `V90Phase4Modulator` and
 * `V90SpectralShaper` members that nothing in this tree has written, and one
 * unwritten callee fails every differential binary rather than only its own
 * (finding 215).
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
 * then `rDetector1.int_24` -- this compiles to 123 bytes with one
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
 * and the acceptance test is finding 617's full-text identity.  `detectFPE`
 * below is the control -- its two detector references are to DIFFERENT
 * objects, so no single pointer could serve both, and it is byte-identical
 * written the obvious way.  Finding 4321.
 */
int
V90Phase4Demodulator::detectRRN(short sample)
{
	V90RDetector *rd = &rDetector1;

	if (!rd->detectR(sample))
		return 0;

	edprintf("V90Phase4Demodulator: Rd detected, polarity = %d\r\n",
		 rd->int_24);
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
 * hypothesis about the source's, and the acceptance test is finding 617's --
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
 * `detectFPE` -- 115 bytes at 0x25d60.
 *
 * IT PRINTS THE OTHER DETECTOR'S POLARITY, and that is the blob's:
 *
 *     25d6d:  8d 83 28 30 ..   lea 0x3028(%ebx),%eax   ; rDetector2
 *     25d7a:  call             V90RDetector::detectRf
 *     25d90:  8b 8b 20 30 ..   mov 0x3020(%ebx),%ecx   ; 0x2ffc + 0x24
 *
 * -- the detection runs on `rDetector2` at +0x3028 and the "%d" comes from
 * +0x3020, which is `rDetector1.int_24`.  `rDetector2.int_24` would be
 * +0x304c.  Both are `lea`/`mov` off the same base in the same 22
 * instructions, so this is not a misread of which object is which; it is a
 * copy of `detectRRN` whose second reference was not updated.  Finding 4320.
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
		 rDetector1.int_24);
	edprintf("V90Phase4Demodulator: enter FPE !");
	state = P4D_STATE_FPE;
	countInState = 0;
	int_0028 = 0;
	return 1;
}
