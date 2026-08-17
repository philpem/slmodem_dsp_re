/*
 * V90Phase4Modulator.cpp -- the phase 4 modulator's construction, destruction
 * and session flag.
 *
 * Reconstructed from dsplibs.o.  Three of the class's forty-three members:
 * the constructor, the destructor, and `setSessionFlag`, which is the one
 * `v34handshak` reaches.  `include/dsplib/V90Phase4Modulator.h` carries the
 * object map, the 0x2fac size and the ownership argument.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * `V90Phase3Modulator::setSessionFlag` is the same eleven bytes against the
 * same offset in a different class, and src/pump/v90/V90Phase3Modulator.cpp
 * is where that one lives.
 *
 * WHY THE CONVERTER IS BUILT THROUGH AN asm() LABEL RATHER THAN `new`: the
 * argument is src/pump/v90/V90BitsToSymbol.cpp's, and it is the same one --
 * `-nostdinc++` leaves no <new>, a replacement global `operator new` is
 * ill-formed, and a user-declared placement form makes GCC emit a null test
 * the blob does not have.  The instruction sequence is the blob's either way.
 */

#include <stddef.h>

/*
 * `pcm.h`, `debug.h` and `encode.h` are C headers with no linkage guard of
 * their own, so they take the wrapper every other C++ consumer of them uses
 * (V90ConstellationPower.cpp, V90AutoDigitalImpDetector.cpp): without it
 * `alaw2linear` and `edprintf` mangle and the references resolve to nothing.
 */
extern "C" {
#include "dsplib/pcm.h"
}
#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Phase4Modulator.h"

extern "C" {
/*
 * V90BitsToSymbol's constructor, by the name the blob calls.  C1 is the
 * complete-object variant, which is what a `new` expression uses and what the
 * relocation at 0x2d8f5 names.
 */
void v90p4_bts_ctor(void *self, unsigned int nofSymbols, V90Parameters *params)
	asm("_ZN15V90BitsToSymbolC1EjP13V90Parameters");
}

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90P4_OFF(field, off, tag) \
	typedef char v90p4_off_##tag[ \
	    ((int)__builtin_offsetof(V90Phase4Modulator, field) == (off)) \
	    ? 1 : -1]

V90P4_OFF(sessionFlag,		0x0000, sessionflag);
V90P4_OFF(state,		0x0004, state);
V90P4_OFF(symbolCount,		0x0008, symbolcount);
V90P4_OFF(nextStateAfterTRN2d,	0x0010, nextafter);
V90P4_OFF(byte_0014,		0x0014, b0014);
V90P4_OFF(word_0018,		0x0018, w0018);
V90P4_OFF(byte_001c,		0x001c, b001c);
V90P4_OFF(word_0020,		0x0020, w0020);
V90P4_OFF(word_0024,		0x0024, w0024);
V90P4_OFF(word_0034,		0x0034, w0034);
V90P4_OFF(pcmType,		0x0038, pcmtype);
V90P4_OFF(codeLevel,		0x003c, codelevel);
V90P4_OFF(word_0040,		0x0040, w0040);
V90P4_OFF(bitsToSymbol,		0x0044, bts);
V90P4_OFF(mp,			0x0048, mp);
V90P4_OFF(mappingParams,	0x004c, mp1);
V90P4_OFF(mappingParams2,	0x0050, mp2);
V90P4_OFF(cp,			0x0054, cp);
V90P4_OFF(scrambler,		0x0058, scrambler);
V90P4_OFF(scrambledBits,	0x0078, bits);
V90P4_OFF(mpBits,		0x2f58, mpbits);
V90P4_OFF(mpBitCount,		0x2f5c, mpcount);
V90P4_OFF(mpSequenceSymbols,	0x2f60, mpsym);
V90P4_OFF(word_2f64,		0x2f64, w2f64);
V90P4_OFF(rdRtSymbols,		0x2f68, rdrt);
V90P4_OFF(rfSymbols,		0x2f74, rf);
V90P4_OFF(cpBits,		0x2f8c, cpbits);
V90P4_OFF(cpBitCount,		0x2f90, cpcount);
V90P4_OFF(cpSequenceSymbols,	0x2f94, cpsym);
/*
 * The one that proves the region was renamed and not resized: everything from
 * here on was named before this pass, so if the fields above have taken one
 * byte too many or too few, this fails.
 */
V90P4_OFF(ctorArg8,		0x2f98, arg8);
V90P4_OFF(word_2f9c,		0x2f9c, c2f9c);
V90P4_OFF(word_2fa0,		0x2fa0, c2fa0);
V90P4_OFF(externalBitsToSymbol,	0x2fa4, external);
V90P4_OFF(params,		0x2fa8, params);
typedef char v90p4_size[(sizeof(V90Phase4Modulator) == 0x2fac) ? 1 : -1];
#endif

/*
 * ===========================================================================
 * V90Phase4Modulator::V90Phase4Modulator -- .text+0x2d830 (C1) and +0x2d910
 * (C2), 213 bytes each.
 *
 * The scrambler's mem-initializer runs before the body, which is where the
 * blob's leading `lea 0x58(%esi),%edx ; call Scrambler<h,h>::C1` comes from.
 * `pad_0004` and everything from `scrambledBits` to `cpSequenceSymbols` are
 * left exactly as they were found; forty-two unwritten members' state is not
 * this function's business.  (That second region was `pad_0078` until it was
 * named out; the constructor's behaviour is unchanged, which is the point.)
 * ===========================================================================
 */
V90Phase4Modulator::V90Phase4Modulator(V90Parameters *p, unsigned int flag,
				       V90BitsToSymbol *bts, V90MP *mpArg,
				       V90MappingParams *mpsA,
				       V90MappingParams *mpsB, V90CP *cpArg,
				       unsigned int arg8)
	: scrambler(0x12, 0x17, 0x63)
{
	params = p;
	cp = cpArg;
	ctorArg8 = arg8;
	mp = mpArg;
	mappingParams = mpsA;
	sessionFlag = flag;
	word_2f9c = 0;
	mappingParams2 = mpsB;
	word_2fa0 = 0;
	if (bts) {
		bitsToSymbol = bts;
		externalBitsToSymbol = 1;
	} else {
		V90BitsToSymbol *own;

		own = (V90BitsToSymbol *)
		    sysdep_malloc(sizeof(V90BitsToSymbol));
		v90p4_bts_ctor(own, 0x140, p);
		bitsToSymbol = own;
		externalBitsToSymbol = 0;
	}
}

/*
 * ===========================================================================
 * V90Phase4Modulator::~V90Phase4Modulator -- .text+0x2c5a0 (D2) and +0x2c600
 * (D1), 94 bytes each.
 *
 * The ownership flag is read FIRST and short-circuits the whole release, so a
 * supplied converter is never touched however non-null it is.  Neither the
 * pointer nor the flag is cleared afterwards.
 * ===========================================================================
 */
V90Phase4Modulator::~V90Phase4Modulator()
{
	if (!externalBitsToSymbol && bitsToSymbol) {
		bitsToSymbol->~V90BitsToSymbol();
		sysdep_free(bitsToSymbol);
	}
}

void
V90Phase4Modulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
}

/*
 * ===========================================================================
 * THE TWO SYMBOL TABLES -- setRdRtSymbols (.text+0x2d180, 512 bytes) and
 * setRfSymbols (+0x2d380, 1,005).
 *
 * Both read the FIRST byte of each of the six constellations,
 * `V90MappingParams::constellation[k][0]`, expand it with the companding law
 * in `pcmType`, and store 16 bits.  The expansion is the tree's existing
 * idiom, byte for byte the same instructions as
 * `V90AutoDigitalImpDetector::reset` and `V90Phase3Demodulator`'s: A-law is
 * `alaw2linear((code & 0x7f) ^ 0xd5)` and mu-law `ulaw2linear((code & 0x7f)
 * ^ 0xff)`, the second of which GCC emits as an 8-bit `not`.
 *
 * `pcmType` IS RELOADED FOR EVERY ELEMENT -- eighteen `test 0x38(%ebx)`
 * between the eighteen calls, never hoisted -- because `alaw2linear` is an
 * opaque call that the compiler must assume can write through `this`.  So the
 * source is a straight run of independent statements and not a loop with the
 * test outside it; there is no `rep`, no back edge and no induction variable
 * anywhere in either function.
 *
 * P4M_LEVEL IS A MACRO AND THE MACRO IS THE POINT.  Writing eighteen
 * six-line if/else blocks and writing this are the same translation unit
 * after preprocessing -- a macro is a textual substitution and cannot move
 * code generation (CLAUDE.md) -- and the eighteen assignment lines below are
 * still one statement each, which is what a mutation anchors on.
 *
 * THE SIGNS ARE THE OBJECT'S.  Six symbols for Rd/Rt with the last three
 * negated; twelve for Rf, cycling the same six sources twice with 2, 3, 6, 7,
 * 10 and 11 negated.  Each `neg %eax` is between the call and the 16-bit
 * store, so the negation happens in `int` and the truncation after it.
 *
 * `setRdRtSymbols` IS IDENTICAL TO THE OBJECT AND `setRfSymbols` IS NOT, AND
 * THE DIFFERENCE IS SIX INSTRUCTIONS' SCHEDULING.  Same source shape, same
 * macro, twice the length; on the longer one GCC moves a `mov` or a `neg`
 * one or two slots against its neighbour six times and changes nothing else
 * -- same count, same instructions, same operands.  That is the free column
 * (CLAUDE.md), and the twin matching exactly is what says the shape is right
 * rather than a coincidence of length.
 * ===========================================================================
 */
#define P4M_LEVEL(m, k) \
	(pcmType != PCM_TYPE_MU_LAW \
	    ? (short)alaw2linear((unsigned char) \
		  (((m)->constellation[k][0] & 0x7f) ^ 0xd5)) \
	    : (short)ulaw2linear((unsigned char) \
		  (((m)->constellation[k][0] & 0x7f) ^ 0xff)))

void
V90Phase4Modulator::setRdRtSymbols(V90MappingParams *m)
{
	rdRtSymbols[0] = P4M_LEVEL(m, 0);
	rdRtSymbols[1] = P4M_LEVEL(m, 1);
	rdRtSymbols[2] = P4M_LEVEL(m, 2);
	rdRtSymbols[3] = -P4M_LEVEL(m, 3);
	rdRtSymbols[4] = -P4M_LEVEL(m, 4);
	rdRtSymbols[5] = -P4M_LEVEL(m, 5);
}

void
V90Phase4Modulator::setRfSymbols(V90MappingParams *m)
{
	rfSymbols[0] = P4M_LEVEL(m, 0);
	rfSymbols[1] = P4M_LEVEL(m, 1);
	rfSymbols[2] = -P4M_LEVEL(m, 2);
	rfSymbols[3] = -P4M_LEVEL(m, 3);
	rfSymbols[4] = P4M_LEVEL(m, 4);
	rfSymbols[5] = P4M_LEVEL(m, 5);
	rfSymbols[6] = -P4M_LEVEL(m, 0);
	rfSymbols[7] = -P4M_LEVEL(m, 1);
	rfSymbols[8] = P4M_LEVEL(m, 2);
	rfSymbols[9] = P4M_LEVEL(m, 3);
	rfSymbols[10] = -P4M_LEVEL(m, 4);
	rfSymbols[11] = -P4M_LEVEL(m, 5);
}

/*
 * ===========================================================================
 * THE SIX SYMBOL READERS -- 38 to 100 bytes each.
 *
 * Every one of them indexes on `(symbolCount - 1) % 6` or `% 12`: the object
 * multiplies by 0xaaaaaaab and shifts by 2 or 3, which is the unsigned
 * division idiom, and `symbolCount` is unsigned for that reason.  Rd/Rt and
 * Rf read a table; Ri has no table and uses `codeLevel` with the same
 * three-positive-then-three-negative shape the Rd/Rt table is filled with.
 *
 * THE FOUR `*Not` READERS TAKE THEIR VALUE THROUGH AN `int`, AND THAT IS
 * FORCED.  `return -rdRtSymbols[k]` compiles to `movzwl ; neg ; cwtl` -- the
 * extension is free there because `cwtl` throws the upper half away again --
 * but the object loads `movswl`.  Naming an `int` and negating that is what
 * makes GCC load signed, and it takes `generateRdRtNot` and `generateRfNot`
 * from differing to identical.  This is 613's case rather than 614's: the
 * two spellings agree over every value, so no differential test can separate
 * them and only the codegen tier can.
 *
 * `generateRi` AND `generateRiNot` ARE A `switch` AND NOT AN if/else CHAIN,
 * AND THAT IS FORCED TOO.  Written `if (k <= 2) ... else if (k <= 5) ...`,
 * GCC puts the first arm in the fall-through and emits `cmp $0x2 ; ja`.  The
 * object has `cmp $0x2 ; jbe` to a forward block and then `cmp $0x5 ; ja`,
 * which is the balanced two-range decision tree GCC builds for a `switch`
 * over six labels with two destinations.  With the `switch` both functions
 * are identical to the object, mnemonic for mnemonic.
 *
 * BOTH OF THEM THEN READ AN UNINITIALISED `short` ON A PATH THE MODULUS
 * CANNOT REACH.  `(symbolCount - 1) % 6` is at most 5, so the `cmp $0x5,%eax
 * ; ja` -- the switch's own default edge -- jumps to a tail that uses
 * whatever the caller left in that register.  The object does it, no input
 * can enter it, and adding a `default:` would add an instruction the object
 * does not have.  Deviation D660.
 * ===========================================================================
 */
#define V90P4M_RI_PERIOD	6	/* the same six as rdRtSymbols */

short
V90Phase4Modulator::generateRdRt()
{
	return rdRtSymbols[(symbolCount - 1) % V90P4M_RDRT_SYMBOLS];
}

short
V90Phase4Modulator::generateRdRtNot()
{
	int sym = rdRtSymbols[(symbolCount - 1) % V90P4M_RDRT_SYMBOLS];

	return -sym;
}

short
V90Phase4Modulator::generateRf()
{
	return rfSymbols[(symbolCount - 1) % V90P4M_RF_SYMBOLS];
}

short
V90Phase4Modulator::generateRfNot()
{
	int sym = rfSymbols[(symbolCount - 1) % V90P4M_RF_SYMBOLS];

	return -sym;
}

short
V90Phase4Modulator::generateRi()
{
	unsigned int k = (symbolCount - 1) % V90P4M_RI_PERIOD;
	short sym;

	switch (k) {
	case 0:
	case 1:
	case 2:
		sym = codeLevel;
		break;
	case 3:
	case 4:
	case 5:
		sym = -codeLevel;
		break;
	}
	return sym;
}

short
V90Phase4Modulator::generateRiNot()
{
	unsigned int k = (symbolCount - 1) % V90P4M_RI_PERIOD;
	short sym;

	switch (k) {
	case 0:
	case 1:
	case 2:
		sym = codeLevel;
		break;
	case 3:
	case 4:
	case 5:
		sym = -codeLevel;
		break;
	}
	return -sym;
}

/*
 * ===========================================================================
 * THE THREE STATE-MACHINE MEMBERS THAT SAY NOTHING AND CALL NOTHING.
 * ===========================================================================
 */
void
V90Phase4Modulator::setNextStateAfterTRN2d(Phase4ModulatorState next)
{
	nextStateAfterTRN2d = next;
}

/*
 * resetBeforRRN -- .text+0x2c660, 63 bytes.  Eight stores and no reads; the
 * spelling of the name is the object's.
 */
void
V90Phase4Modulator::resetBeforRRN()
{
	word_2f9c = 0;
	word_2fa0 = 0;
	word_0024 = 1;
	word_0028 = 0;
	word_002c = 0;
	word_0030 = 0;
	word_0034 = 0;
	word_0020 = 0;
}

/*
 * resetRRNSecondSection -- .text+0x2c870, 53 bytes.  The same shape one field
 * along, and it also clears the CP's +0x13 -- which `recivedCP`,
 * `recivedCPtag`, `recivedPartOneSilenceRrnSUV` and
 * `recivedPartOneSilenceRrnSUVtag` set.
 */
void
V90Phase4Modulator::resetRRNSecondSection()
{
	word_0030 = 1;
	word_0020 = 0;
	byte_001c = 0;
	word_0018 = 0;
	cp->byte_13 = 0;
	word_2f9c = 0;
	word_2fa0 = 0;
}

/*
 * recivedCP -- .text+0x2cb20, 23 bytes -- and recivedPartOneSilenceRrnSUV --
 * +0x2ca40, 12.  The second is the first without the latch.
 */
void
V90Phase4Modulator::recivedCP()
{
	word_2f9c = 1;
	cp->byte_13 = 1;
}

void
V90Phase4Modulator::recivedPartOneSilenceRrnSUV()
{
	cp->byte_13 = 1;
}
