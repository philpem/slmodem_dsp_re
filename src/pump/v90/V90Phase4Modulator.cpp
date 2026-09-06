/*
 * V90Phase4Modulator.cpp -- the phase 4 modulator: construction, the two
 * symbol tables, the six symbol readers, the fifteen state-machine edges and
 * the two data pumps.
 *
 * Reconstructed from dsplibs.o.  THIRTY-NINE of the class's forty-three
 * members -- `grep -c '^V90Phase4Modulator::'` here, against the 43 distinct
 * members `nm -S ref/slmodemd/dsplibs.o | grep _ZN18V90Phase4Modulator`
 * leaves once C1/C2 and D1/D2 are folded.  Re-measure both rather than
 * believing this sentence; the last two revisions of it were stale before
 * they were read (findings F6100, F6103).
 *
 * ALL FORTY-THREE ARE NOW HERE: `generateB1d`, `generateTRN2d`,
 * `generateEd` and `recivedPartTwoSilenceRrnSUVtag` -- the four this
 * paragraph used to list as missing -- were claimed by the VPcmV34Main leaf
 * pass.  `setMappingParams`,
 * `reset`, `generateSymbol`, `generateV90Symbol` and `generateV92Symbol` used
 * to be on that list and are now written; the last two are 2,235 and 3,922
 * bytes and are where the state machine is dispatched rather than edged,
 * which is the block comment above them.
 * `include/dsplib/V90Phase4Modulator.h` carries the object map, the 0x2fac
 * size, the ownership argument and `Phase4ModulatorState`.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215).
 *
 * `V90Phase3Modulator::setSessionFlag` is the same eleven bytes against the
 * same offset in a different class, and src/pump/v90/V90Phase3Modulator.cpp
 * is where that one lives.
 *
 * THE OWNED CONVERTER IS BUILT WITH ORDINARY PLACEMENT `new`.  This file used
 * to reach `V90BitsToSymbol`'s constructor through a hand-mangled
 * `asm("_ZN15V90BitsToSymbolC1EjP13V90Parameters")` label, on the belief
 * (finding F1340) that a user-declared placement `operator new` would make
 * GCC emit a null test the blob does not have.  Finding F10155 retracts
 * that: the check is tied to a `throw()`-declared placement operator,
 * `-fcheck-new` was never in this project's flags, and
 * `include/dsplib/sysdep.h`'s shared non-throw placement `operator new`
 * reproduces the blob's construct-then-check-later shape with no flag
 * changes, verified under the real period compiler (finding F10157).  The
 * instruction sequence is the blob's either way.
 *
 * THAT LAST CLAUSE IS WHY THIS RULING SURVIVES FINDING F7786 AND THE
 * DESTRUCTORS' DOES NOT.  7786 shows the object DOES replace global
 * `operator delete` -- its compiler-generated `D0Ev` destructors tail-call
 * `sysdep_free` instead of `_ZdlPv` -- and `src/dsp/` now writes `delete[]`
 * where the free is the last statement, because there the two spellings emit
 * DIFFERENTLY: an explicit guarded `sysdep_free` in tail position becomes a
 * sibling `jmp` and the object makes an ordinary `call`.  Here the two
 * spellings emit the SAME instructions, so the well-formed one costs nothing
 * and is kept.  Ill-formedness is the tie-breaker only when the object does
 * not break the tie first.
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
#include "dsplib/debug.h"
#include "dsplib/encode.h"
}
#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Phase4Modulator.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90P4_OFF(field, off, tag) \
	typedef char v90p4_off_##tag[ \
	    ((int)__builtin_offsetof(V90Phase4Modulator, field) == (off)) \
	    ? 1 : -1]

V90P4_OFF(sessionFlag,		0x0000, sessionflag);
V90P4_OFF(state,		0x0004, state);
V90P4_OFF(symbolCount,		0x0008, symbolcount);
V90P4_OFF(eventCode,		0x000c, eventcode);
V90P4_OFF(nextStateAfterTRN2d,	0x0010, nextafter);
V90P4_OFF(delayedMpNotExit,		0x0014, delayedmpnotexit);
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
 * THE ORDER OF THE DEFINITIONS BELOW IS LOAD-BEARING.  DO NOT REGROUP THEM.
 *
 * GCC 3.4.2 emits leaf functions in source-definition order, and where a
 * function is emitted CHANGES THE CODE IT EMITS -- not just its address.
 * Reordering this file's definitions to the blob's emission order took nine
 * grade-1 near-misses and one 1,005-byte size mismatch to byte-exact, and
 * moved nothing the wrong way.
 *
 * So the definitions are in the BLOB'S EMISSION ORDER, which is the original
 * author's source order and is NOT grouped by role.  `nm -n` on the blob is
 * the authority; the `.text+0x...` addresses in the per-function comments run
 * in increasing order down the file and are the cheap check that they still
 * do.  Tidying two related handlers back together will silently un-match
 * whatever sits between them.  Finding F7782.
 *
 * A macro or a file-scope `static` must therefore live ABOVE the definitions,
 * not beside its first user -- which is why `V90P4M_RI_PERIOD` is here and not
 * next to `generateRdRt`: the reorder moved two users above where it sat and
 * the file stopped compiling.  A macro is a compile-time substitution and
 * cannot move codegen, so hoisting it is free.
 * ===========================================================================
 */
#define V90P4M_RI_PERIOD	6	/* the same six as rdRtSymbols */

/* The companding expansion `setRdRtSymbols` and `setRfSymbols` are eighteen
 * copies of; the block comment above those two says why it is a macro and why
 * that is the point.  Up here for the reason above -- it used to sit beside
 * them and travelled with them when the file was reordered. */
#define P4M_LEVEL(m, k) \
	(pcmType != PCM_TYPE_MU_LAW \
	    ? (short)alaw2linear((unsigned char) \
		  (((m)->constellation[k][0] & 0x7f) ^ 0xd5)) \
	    : (short)ulaw2linear((unsigned char) \
		  (((m)->constellation[k][0] & 0x7f) ^ 0xff)))

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

		/* C1, the complete-object variant, is what a `new`
		 * expression uses and what the relocation at 0x2d8f5
		 * names. */
		own = (V90BitsToSymbol *)
		    sysdep_malloc(sizeof(V90BitsToSymbol));
		new (own) V90BitsToSymbol(0x140, p);
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
 *
 * NOT GRADE 0: blob 28 instructions against our 26 (byteident's lever-2
 * "absence" reading).  Disassembly shows this is a register-allocation
 * choice, not a missing statement: the blob holds `this` in %esi AND
 * `bitsToSymbol` in %ebx as two separate callee-saved registers across the
 * `V90BitsToSymbol` destructor call, so it never re-reads the field; we hold
 * only `this` and reload the field from memory after the call clobbers the
 * scratch that held it.  The reload/no-reload difference costs +1 in the
 * shared tail and the missing second callee-saved restore costs +1 in the
 * early-exit path -- accounts for the whole delta 2.
 *
 * Two alternate spellings were compiled and declined (F7782's hill-climbing
 * rule -- neither is closer to a match, both moved further away):
 *   `V90BitsToSymbol *bts = bitsToSymbol; if (!ext && bts) {...}`
 *       -> blob 28 / ours 30: the combined `&&` condition made GCC merge the
 *          two tests with sete/setne instead of the blob's two sequential
 *          branches.
 *   the same local hoisted into a nested `if (!ext) { if (bts) {...} }`
 *       -> blob 28 / ours 21: over-shrank the body instead.
 * Left as the direct field-access spelling above (2-cell enumeration, no
 * cell reached zero); a wider search was not run since this pair is outside
 * this pass's assignment (v90p4mod refinement task, F10192-adjacent).
 * ===========================================================================
 */
V90Phase4Modulator::~V90Phase4Modulator()
{
	if (!externalBitsToSymbol && bitsToSymbol) {
		bitsToSymbol->~V90BitsToSymbol();
		sysdep_free(bitsToSymbol);
	}
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

void
V90Phase4Modulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
}

/*
 * ===========================================================================
 * `V90Phase4Modulator::reset` -- .text+0x2f630, 255 bytes.
 *
 * SIXTEEN STORES, ONE G.711 EXPANSION, ONE CALL INTO THE SCRAMBLER AND A
 * PUMP LOOP.  Everything the state machine's edges maintain is put back:
 * `symbolCount` and `eventCode` to zero, the five flags at +0x14..+0x20, the
 * four at +0x24..+0x30 -- but NOT +0x34, which only `resetBeforRRN` writes --
 * and the two latches at +0x2f9c and +0x2fa0.  What it does NOT touch is
 * every field that describes a MESSAGE: `mpBits`, `cpBits`, both counts, both
 * sequence lengths, `word_2f64` and the two symbol tables are the
 * constructor's and the edges', and a `reset` leaves them exactly as it found
 * them.
 *
 * THE ORDER ACROSS THE CALLS IS THE OBJECT'S AND IS NOT FREE.  GCC may not
 * move a store through `this` across an opaque call in either direction, so
 * the three fenceposts partition the body:
 *
 *     +0x38, +0x40, +0x08, +0x04, +0x0c   before alaw2linear/ulaw2linear
 *     +0x3c                                between it and Scrambler::reset
 *     the eleven zero/flag stores          after Scrambler::reset
 *
 * Inside each run the interleaving is the scheduler's and nothing here was
 * permuted to chase it.
 *
 * THE COMPANDING EXPANSION HAS NO CAST, AND THAT IS MEASURED RATHER THAN
 * COPIED FROM `P4M_LEVEL`.  Both arms are 32-bit: `and $0x7f,%eax ; xor
 * $0xd5,%eax` at +0x2f670 and `and $0x7f,%edx ; xor $0xff,%edx` --
 * `81 f2 ff 00 00 00`, a full-word immediate -- at +0x2f719.  The macro's
 * mu-law arm truncates to `unsigned char` first and GCC emits an 8-bit `not`
 * for it there; here it does not, so the argument reaches `ulaw2linear` as an
 * `int` and the source cannot carry the cast.  Same two conversions, two
 * different spellings, and the object is what separates them.
 *
 * `nextStateAfterTRN2d` IS SEEDED FROM `sessionFlag`, branchlessly:
 * `cmp $0x1,%edx ; sbb %eax,%eax ; add $0x5,%eax` is 4 when the flag is zero
 * and 5 when it is not -- MP under V.90 and SUVd under V.92, which are
 * exactly the two states TRN2d hands on to (finding F7452's second item).
 *
 * THE LOOP RELOADS `sessionFlag` EVERY ITERATION.  `mov (%esi),%edx` at
 * +0x2f6fd is inside the back edge, not above it: either pump may store
 * through `this`, so the compiler must re-read the selector.  Writing the
 * test outside the loop would be a different program.
 *
 * ITS POSITION IN THE TU IS A LEVER-3 CANDIDATE, TRIED AND REVERTED. Both the
 * blob and our own object emit `reset` dead last among this class's members
 * even though it is declared early in this file, and this function was moved
 * to the end of the file (after `generateSymbol`) on that strength.  It was a
 * NULL: the whole tree's grade-0 counts (736 EXACT / 796 grade-0-or-1) did not
 * move by one symbol, and the resulting `nm -n` order still did not match the
 * blob's (ours put `reset` immediately before `generateSymbol`; the blob has
 * it after the whole generate-family, dead last).  Reverted rather than kept,
 * per refinement.md's "a null result can cost too much to keep" -- this one
 * was cheap, but it also isn't a durable measured fact worth the diff noise,
 * since it never reached the order it was aimed at.
 * ===========================================================================
 */
void
V90Phase4Modulator::reset(PcmType law, unsigned char code,
			  Phase4ModulatorState st, unsigned int nofSymbols,
			  unsigned int arg5)
{
	unsigned int i;

	pcmType = law;
	word_0040 = arg5;
	symbolCount = 0;
	state = st;
	eventCode = 0;

	codeLevel = (law != PCM_TYPE_MU_LAW)
	    ? (short)alaw2linear((code & 0x7f) ^ 0xd5)
	    : (short)ulaw2linear((code & 0x7f) ^ 0xff);

	scrambler.reset(0);

	delayedMpNotExit = 0;
	word_0024 = 0;
	word_0028 = 0;
	word_002c = 0;
	nextStateAfterTRN2d = sessionFlag != 0 ? P4M_STATE_SUVD : P4M_STATE_MP;
	word_2f9c = 0;
	word_2fa0 = 0;
	word_0030 = 0;
	word_0018 = 0;
	byte_001c = 0;
	word_0020 = 0;

	for (i = 0; i < nofSymbols; i++) {
		if (sessionFlag != 0)
			generateV92Symbol();
		else
			generateV90Symbol();
	}
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
 * enterRepeatedCPd -- .text+0x2c780.  The only member of the class whose
 * message is a bare `dsplibs_debug_printf` behind `DSPLIB_DEBUG_ON()` rather
 * than an `edprintf`: the call at +0x2c805 relocates against
 * `dsplibs_debug_printf` directly and the gate is the object's own
 * `cmpl $0x1,dsplibs_debug_level ; ja`.
 */
void
V90Phase4Modulator::enterRepeatedCPd()
{
	byte_001c = 0;
	word_0018 = 0;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Phase4Modulator: enter repeatedCPd "
				     "@ %d\r\n", symbolCount);
	state = P4M_STATE_REPEATED_CPD;
	cp->word_00 = 0;
	cp->infoToBits();
	cpBits = cp->getBitVector(cpBitCount);
	cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
	symbolCount = 0;
}

/*
 * ===========================================================================
 * THE SIX SYMBOL READERS -- 38 to 100 bytes each.  `generateRi`/`generateRiNot`
 * are above and `resetRRNSecondSection` sits between two of the others: the
 * definitions are in the blob's emission order, not grouped by role, and this
 * banner heads a role and not a block.  See the note above `V90P4M_RI_PERIOD`.
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

/*
 * exitMPNot -- .text+0x2c910.  Entering Ed sets the deadline at +0x2f64; five
 * other members do the same three lines.
 */
void
V90Phase4Modulator::exitMPNot()
{
	if (state == P4M_STATE_MP_NOT && symbolCount != 0) {
		if (symbolCount % mpSequenceSymbols != 0) {
			state = P4M_STATE_UNNAMED_0F;
		} else {
			edprintf("V90Phase4Modulator: enter Ed @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			state = P4M_STATE_ED;
			word_2f64 = bitsToSymbol->extraSymbols + 12;
		}
	}
}

/*
 * recivedSUV -- .text+0x2c980.  The CPd entry, and the first of the three
 * that set +0x2fa0 once the CP sequence has been rebuilt.
 */
void
V90Phase4Modulator::recivedSUV()
{
	if (word_2fa0 == 0 && state == P4M_STATE_SUVD) {
		if (symbolCount % cpSequenceSymbols != 0) {
			state = P4M_STATE_UNNAMED_06;
		} else {
			edprintf("V90Phase4Modulator: enter CPd @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			cp->word_00 = 0;
			state = P4M_STATE_CPD;
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
			word_2fa0 = 1;
		}
	}
}

void
V90Phase4Modulator::recivedPartOneSilenceRrnSUV()
{
	cp->byte_13 = 1;
}

/*
 * recivedPartTwoSilenceRrnSUV -- .text+0x2ca50.  `recivedSUV`'s body behind
 * two extra range guards that the final `state == SUVd` already implies.
 * They are the object's, in the object's order; see the block comment above.
 */
void
V90Phase4Modulator::recivedPartTwoSilenceRrnSUV()
{
	if (state == P4M_STATE_UNNAMED_17 || state == P4M_STATE_UNNAMED_18)
		return;
	if (state == P4M_STATE_UNNAMED_19 || state == P4M_STATE_RT ||
	    state == P4M_STATE_RT_NOT)
		return;
	if (word_2fa0 == 0 && state == P4M_STATE_SUVD) {
		if (symbolCount % cpSequenceSymbols != 0) {
			state = P4M_STATE_UNNAMED_06;
		} else {
			edprintf("V90Phase4Modulator: enter CPd @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			cp->word_00 = 0;
			state = P4M_STATE_CPD;
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
			word_2fa0 = 1;
		}
	}
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

/*
 * recivedSUVtag -- .text+0x2cb40.  Two cases whose boundary arms are
 * identical, which is why the object has one copy of the Ed block reached
 * from both.
 */
void
V90Phase4Modulator::recivedSUVtag()
{
	byte_001c = 0;
	word_0018 = 0;
	if (word_2f9c != 0 && word_0020 == 0) {
		switch (state) {
		case P4M_STATE_SUVD:
			if (symbolCount % cpSequenceSymbols != 0) {
				state = P4M_STATE_UNNAMED_0A;
			} else {
				edprintf("V90Phase4Modulator: enter Ed @ " "%d\r\n", symbolCount);
				state = P4M_STATE_ED;
				word_2f64 = bitsToSymbol->extraSymbols + 12;
				symbolCount = 0;
			}
			word_0020 = 1;
			break;
		case P4M_STATE_CPD:
		case P4M_STATE_REPEATED_CPD:
			if (symbolCount % cpSequenceSymbols != 0) {
				state = P4M_STATE_UNNAMED_09;
			} else {
				edprintf("V90Phase4Modulator: enter Ed @ " "%d\r\n", symbolCount);
				state = P4M_STATE_ED;
				word_2f64 = bitsToSymbol->extraSymbols + 12;
				symbolCount = 0;
			}
			word_0020 = 1;
			break;
		default:
			break;
		}
	}
}

/*
 * recivedPartOneSilenceRrnSUVtag -- .text+0x2cbf0.  `recivedFirstRrnE2u` with
 * the plain "enter Ed" message and the CP's tag byte raised first.
 */
void
V90Phase4Modulator::recivedPartOneSilenceRrnSUVtag()
{
	cp->byte_13 = 1;
	if (word_0020 == 0) {
		if (symbolCount % cpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter Ed @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_ED;
			word_2f64 = bitsToSymbol->extraSymbols + 12;
			symbolCount = 0;
		} else {
			state = P4M_STATE_UNNAMED_0A;
		}
		word_0020 = 1;
	}
}

/*
 * recivedPartTwoSilenceRrnSUVtag -- .text+0x2cc60, 5 bytes: a single `jmp`
 * with a relocation on it, a sibling call to `recivedSUVtag` -- a distinct
 * function whose body is one call, not an alias.  The V.92 sibling at
 * .text+0x174f0 is the same five bytes and the same spelling
 * (V92Phase4Modulator.cpp).
 */
void
V90Phase4Modulator::recivedPartTwoSilenceRrnSUVtag()
{
	recivedSUVtag();
}

/*
 * recivedCPtag -- .text+0x2cc70, the largest of the fifteen.  The one member
 * that acts when `word_0020` is NON-zero, and the only one that sets
 * `cp->word_00` to 1 rather than 0 -- V90CP.h reads that as selecting the
 * short form of the message.
 *
 * NOT GRADE 0, BUT ONLY BY A REGISTER SWAP: `--why` reports BYTES (4 bytes
 * differ), grade 1 ACCEPT.  `dis.py` on both objects shows the entire body
 * byte-for-byte identical except the FIRST two loads -- blob puts
 * `0x20(%ebx)` in %eax and `0x2f9c(%ebx)` in %edx; ours has the two swapped.
 * Nothing else in the function differs.  This is the same REGALLOC-cursor
 * family as `setMappingParams`/`setRdRtSymbols`/`setRfSymbols` below --
 * traced, not fixed, and DECLINED for the same reason: see the shared note
 * at `setRdRtSymbols`.
 */
void
V90Phase4Modulator::recivedCPtag()
{
	byte_001c = 0;
	word_0018 = 0;
	if (word_0020 != 0) {
		if (word_2f9c != 0) {
			if (symbolCount % cpSequenceSymbols == 0) {
				edprintf("V90Phase4Modulator: enter Ed @ " "%d\r\n", symbolCount);
				state = P4M_STATE_ED;
				word_2f64 = bitsToSymbol->extraSymbols + 12;
				symbolCount = 0;
				word_0020 = 1;
			} else {
				switch (state) {
				case P4M_STATE_SUVD:
					state = P4M_STATE_UNNAMED_0A;
					word_0020 = 1;
					break;
				case P4M_STATE_CPD:
				case P4M_STATE_REPEATED_CPD:
					state = P4M_STATE_UNNAMED_09;
					word_0020 = 1;
					break;
				default:
					break;
				}
			}
		} else {
			word_2f9c = 1;
			cp->byte_13 = 1;
			cp->word_00 = 1;
			if (symbolCount % cpSequenceSymbols != 0) {
				state = P4M_STATE_UNNAMED_0C;
				word_0020 = 1;
			} else {
				state = P4M_STATE_FINAL_SUVD;
				symbolCount = 0;
				word_0020 = 1;
				cpBits = cp->getBitVector(cpBitCount);
				cpSequenceSymbols =
				    6 * cpBitCount / cp->word_3ba8;
			}
		}
	}
}

/* recivedE2u -- .text+0x2cd90.  The boundary test first, the state second. */
void
V90Phase4Modulator::recivedE2u()
{
	if (word_0020 == 0) {
		if (symbolCount % cpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter Ed @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_ED;
			word_2f64 = bitsToSymbol->extraSymbols + 12;
			symbolCount = 0;
			word_0020 = 1;
		} else {
			switch (state) {
			case P4M_STATE_SUVD:
				state = P4M_STATE_UNNAMED_0A;
				word_0020 = 1;
				break;
			case P4M_STATE_CPD:
			case P4M_STATE_REPEATED_CPD:
				state = P4M_STATE_UNNAMED_09;
				word_0020 = 1;
				break;
			default:
				break;
			}
		}
	}
}

/*
 * recivedFirstRrnE2u -- .text+0x2ce20.  `recivedE2u` with no switch and its
 * own message: this is the only site that says "enter Ed first at RRN".
 */
void
V90Phase4Modulator::recivedFirstRrnE2u()
{
	if (word_0020 == 0) {
		if (symbolCount % cpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter Ed first at RRN "
				 "@ %d\r\n", symbolCount);
			state = P4M_STATE_ED;
			word_2f64 = bitsToSymbol->extraSymbols + 12;
			symbolCount = 0;
		} else {
			state = P4M_STATE_UNNAMED_0A;
		}
		word_0020 = 1;
	}
}

/* exitSilence -- .text+0x2ce90.  The same shape leaving the silence pair. */
void
V90Phase4Modulator::exitSilence()
{
	if ((state == P4M_STATE_UNNAMED_17 || state == P4M_STATE_UNNAMED_18) &&
	    symbolCount != 0) {
		if (symbolCount % V90P4M_RI_PERIOD != 0) {
			state = P4M_STATE_UNNAMED_19;
		} else {
			edprintf("V90Phase4Modulator: enter Rt @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RT;
			symbolCount = 0;
		}
	}
}

/*
 * recivedFirstSUVuPartTwoRrn -- .text+0x2cf00.  `exitSilence`'s body and
 * `recivedSUV`'s body as the two arms of one test on the state.
 */
void
V90Phase4Modulator::recivedFirstSUVuPartTwoRrn()
{
	if (state == P4M_STATE_UNNAMED_17 || state == P4M_STATE_UNNAMED_18) {
		if (symbolCount != 0) {
			if (symbolCount % V90P4M_RI_PERIOD != 0) {
				state = P4M_STATE_UNNAMED_19;
			} else {
				edprintf("V90Phase4Modulator: enter Rt @ " "%d\r\n", symbolCount);
				state = P4M_STATE_RT;
				symbolCount = 0;
			}
		}
	} else if (state == P4M_STATE_SUVD && word_2fa0 == 0) {
		if (symbolCount % cpSequenceSymbols != 0) {
			state = P4M_STATE_UNNAMED_06;
		} else {
			edprintf("V90Phase4Modulator: enter CPd @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			cp->word_00 = 0;
			state = P4M_STATE_CPD;
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
			word_2fa0 = 1;
		}
	}
}

/*
 * ===========================================================================
 * THE FIFTEEN STATE-MACHINE EDGES.  They are scattered through the file, not
 * gathered below this banner: the definitions are in the blob's emission
 * order, not grouped by role.  See the note above `V90P4M_RI_PERIOD`.
 *
 * Two shapes, and everything here is one or the other.
 *
 * AN `exitX` LEAVES A STATE ON A SEQUENCE BOUNDARY.  It checks that the
 * machine is in the state it is named for, that the symbol counter has moved
 * at all, and then whether the counter is a whole multiple of the current
 * sequence's length in symbols.  On a boundary it announces the next state
 * and resets the counter; off one it moves to a state that goes on emitting
 * the same thing until the boundary arrives.
 *
 * A `recivedX` IS THE DEMODULATOR'S NEWS ARRIVING, and the same boundary test
 * decides whether it can be acted on now or has to wait.
 *
 * THE GUARDS ARE THE OBJECT'S AND THEIR ORDER IS THE OBJECT'S.
 * `recivedPartTwoSilenceRrnSUV` tests the 0x17..0x18 range, then 0x19..0x1b,
 * then +0x2fa0, and only then `state == SUVd` -- the first two are dead given
 * the last, but they are in the object and in that order, and GCC will not
 * re-derive a redundant guard that is dropped.  `recivedSUV` asks the same
 * questions in a different order and is a different function for it.
 *
 * AND THAT ONE'S TWO RANGES HAVE TO BE TWO STATEMENTS.  0x17..0x18 and
 * 0x19..0x1b are adjacent, so written as one `&&` chain GCC folds them into
 * a single `(state - 0x17) <= 4` and emits ONE `lea ; cmp ; jbe` where the
 * object has two.  The same set either way -- so no differential trial can
 * tell -- but as two early returns the function is identical to the object
 * again.  It is 617's territory and it passed 617's test.
 *
 * `word_0020` IS NOT A SIMPLE "ALREADY DONE" LATCH AND THE CODE SAYS SO.
 * Four of the five members that read it act when it is ZERO; `recivedCPtag`
 * acts when it is NOT.  That is why the field keeps an offset name: a name
 * that fitted four sites and contradicted the fifth would be worse than none.
 *
 * THE CP SEQUENCE IS RE-DERIVED IN FIVE PLACES BY THE SAME THREE LINES --
 * `getBitVector` into `cpBits`/`cpBitCount`, then `6 * cpBitCount /
 * cp->word_3ba8` into `cpSequenceSymbols` -- and `exitMP` is the MP copy of
 * it against `mp->groupSize`.  Six symbols carry one group, so the quotient is
 * a count of symbols; V90Phase4Modulator.h has the argument in full.
 * ===========================================================================
 */

/*
 * exitRi -- .text+0x2d010.  The Ri period is the same six the Rd/Rt table is
 * indexed on, and it is a literal here because it is not a table length.
 */
void
V90Phase4Modulator::exitRi()
{
	if (state == P4M_STATE_RI && symbolCount != 0) {
		if (symbolCount % V90P4M_RI_PERIOD != 0) {
			state = P4M_STATE_UNNAMED_01;
		} else {
			edprintf("V90Phase4Modulator: enter RiNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RI_NOT;
			symbolCount = 0;
		}
	}
}

/*
 * ===========================================================================
 * THE THREE STATE-MACHINE MEMBERS THAT SAY NOTHING AND CALL NOTHING.  Two of
 * them are below; `resetBeforRRN` is at the top of the file.  The definitions
 * are in the blob's emission order, not grouped by role.  See the note above
 * `V90P4M_RI_PERIOD`.
 * ===========================================================================
 */
void
V90Phase4Modulator::setNextStateAfterTRN2d(Phase4ModulatorState next)
{
	nextStateAfterTRN2d = next;
}

/* exitMP -- .text+0x2d090.  The MP half of the sequence-length triple. */
void
V90Phase4Modulator::exitMP()
{
	if (state == P4M_STATE_MP && symbolCount != 0) {
		if (symbolCount % mpSequenceSymbols != 0) {
			state = P4M_STATE_UNNAMED_0D;
		} else {
			edprintf("V90Phase4Modulator: enter MPNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_MP_NOT;
			symbolCount = 0;
			mpBits = mp->getBitVector(mpBitCount);
			mpSequenceSymbols = 6 * mpBitCount / mp->groupSize;
		}
	}
}

/*
 * ===========================================================================
 * `V90Phase4Modulator::setMappingParams` -- 96 bytes at .text+0x2d120.
 *
 * A NULL CHECK AND TWO CALLS INTO THE CONVERTER.  A usable block is handed to
 * `bitsToSymbol->reset(mp, pcmType)` -- the companding law comes out of +0x38
 * and not out of the argument -- and then the block size is set to ONE
 * symbol.  Both calls go through the pointer at +0x44, and the object RELOADS
 * it after the first call (`mov 0x44(%ebx),%ecx` at +0x2d13b, `mov
 * 0x44(%ebx),%eax` at +0x2d14f) rather than keeping it, which is what GCC
 * does for two calls through a member it cannot prove the first did not move.
 *
 * THE ONE IS A LITERAL AND NOT A COUNT.  `mov $0x1,%edx` into the outgoing
 * slot; nothing in the class is read to form it.  What comes back from
 * `setSymbolsBlockSize` -- the bit demand for that one symbol -- is dropped,
 * and the sibling call is how the object drops it.
 *
 * THE NULL PATH IS A MESSAGE AND NOTHING ELSE: no store, no call into the
 * converter, and the same `dsplibs_debug_level > 1` gate as everywhere else.
 * The string is the author's own words for what a null block means here.
 * ===========================================================================
 */
void
V90Phase4Modulator::setMappingParams(V90MappingParams *mp)
{
	if (mp == 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("V90Phase4Modulator: ERROR: Null "
					     "mappingParams @ setMappingParams" "\r\n");
		return;
	}

	bitsToSymbol->reset(mp, pcmType);
	bitsToSymbol->setSymbolsBlockSize(1);
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
 * NEITHER IS GRADE 0, AND THE EARLIER CLAIM HERE THAT `setRdRtSymbols` MATCHED
 * WAS STALE -- corrected against `dis.py`/`--why` rather than repeated
 * (CLAUDE.md's own warning about a comment's shelf life).  Both differences
 * are register-allocation, not a shape defect, and both are DECLINED for this
 * pass:
 *
 *   setRdRtSymbols  2 bytes differ.  The function reserves one dead stack
 *                   slot (`sub $0x4,%esp`) purely as an outgoing-argument
 *                   scratch for the repeated `alaw2linear`/`ulaw2linear`
 *                   calls, and discards it at each of its two epilogues with
 *                   a `pop` whose destination register is never read
 *                   afterwards.  The object pops %eax there; we pop %edx.
 *                   Both are the i386.md:17507 `add $imm,%esp` -> `pop %reg`
 *                   conversion's `match_scratch`, filled by GCC 3.4.2's
 *                   round-robin `peep2_find_free_register` cursor
 *                   (`docs/method/refinement.md` lever 3b) -- a property of
 *                   what came before this function in the TU's EMISSION
 *                   order, not of anything in this function's own source.
 *
 *   setRfSymbols    10 bytes differ, `--why`'s row 7 non-register-operand
 *                   is `and $0x7f,%al` (blob, the AL-specific 2-byte
 *                   encoding) against `and $0x7f,%dl` (ours, the general
 *                   3-byte r8 form) -- the SAME instruction; only which
 *                   register holds the byte changes the encoding length.
 *                   Same mechanism as `setRdRtSymbols`, one register over.
 *
 * BOTH TRACE TO THE SAME ROOT, which the derivation above the destructor
 * (`~V90Phase4Modulator`, this class's first-emitted member) now documents:
 * the destructor itself is not grade 0 for a register-allocation reason, sits
 * at TU emission index 0 where lever 3's reorder mechanism categorically
 * cannot reach it (finding F7808's corollary -- nothing precedes the first
 * symbol to move), and a 2-cell enumeration there did not close it.
 * `recivedCPtag` (this file, above `recivedE2u`) is the same family again --
 * a pure register swap, nothing else differing.  All three read as one
 * finding, not three: the scratch-register cursor's state reaching this file
 * is set upstream of every one of them, and closing it means closing the
 * destructor first.  DECLINED here on that basis; not hill-climbed.
 * ===========================================================================
 */
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
 * THE TWO DATA-SYMBOL PUMPS -- generateDataSymbolBeforeFPE (.text+0x2d770)
 * and generateDataSymbolBeforeRRN (+0x2d7d0), 96 bytes each and identical
 * apart from their message and the state they move to.
 *
 * Each asks the bits-to-symbol converter for one symbol and reads the BIT
 * DEMAND back through the reference argument -- `mov 0x10(%esp),%eax ; test
 * %eax,%eax` after the call, never the return value in %eax, which is the
 * status.  A non-zero demand means the block was not complete, and that is
 * what ends the state.
 *
 * `nofBits` IS PASSED UNINITIALISED AND `V90BitsToSymbol::process` LEAVES IT
 * UNINITIALISED WHEN `symbolsBlockSize` IS ZERO.  Nothing writes 0x10(%esp)
 * before the call in either function, so the object has the same hole.
 * Deviation D661; `test/unit/t_v90p4mgen.cpp` says which trials are kept out
 * of the grid because of it.
 * ===========================================================================
 */
short
V90Phase4Modulator::generateDataSymbolBeforeFPE()
{
	unsigned int nofBits;
	short sym;

	bitsToSymbol->process(nofBits, &sym);
	if (nofBits != 0) {
		edprintf("V90Phase4Modulator: enter Rf @ %d\r\n", symbolCount);
		state = P4M_STATE_RF;
		symbolCount = 0;
	}
	return sym;
}

short
V90Phase4Modulator::generateDataSymbolBeforeRRN()
{
	unsigned int nofBits;
	short sym;

	bitsToSymbol->process(nofBits, &sym);
	if (nofBits != 0) {
		edprintf("V90Phase4Modulator: enter Rd @ %d\r\n", symbolCount);
		state = P4M_STATE_RD;
		symbolCount = 0;
	}
	return sym;
}

/*
 * ===========================================================================
 * THE THREE MESSAGE SOURCES -- generateMP (.text+0x2dbd0), generateCPd
 * (+0x2e540) and generateSUVd (+0x2e5f0).  `nm -S` gives 0x000000a7 = 167
 * bytes against each of the three mangled names, and they sit in the object
 * in this file's order: generateMP between generateEd and generateV90Symbol,
 * the other two between generateV90Symbol and generateV92Symbol.
 *
 * WHAT ONE OF THEM DOES.  Ask the converter how many bits it wants next.  If
 * it wants none, drain one symbol out of it and return that.  If it wants
 * some, first scramble the whole of a phase 4 message into `scrambledBits`
 * and feed that to the converter, and only then drain.  Two calls on one arm,
 * four on the other, and no field of this object is written by any of them.
 *
 * ALL THREE ARE CALLERLESS AND NO CALLER IS ADDED.  `readelf -r` over the
 * whole 1.2 MB object finds ZERO relocations of any type naming any of the
 * three -- against 43 naming `V90BitsToSymbol::nofBitsForNextTime`, which is
 * the denominator that stops the zero being a broken grep.  Neither symbol
 * pump reaches them: `generateV90Symbol`'s MP arms and `generateV92Symbol`'s
 * CPd and SUVd arms are written out inline above and below, which is why
 * three functions that plainly belong to those states are not dispatched from
 * them.  Supplying a call would be new behaviour with no blob behaviour to
 * compare it against, which is not reconstruction; 7570's rule for the
 * orphaned CP unpacker, and the same one here.
 *
 * ---------------------------------------------------------------------------
 * THE THREE BODIES ARE THE SAME BODY, AND THAT IS MEASURED RATHER THAN
 * ASSUMED.  Over the 167 bytes at each address:
 *
 *   generateCPd and generateSUVd     IDENTICAL, all 167 bytes
 *   generateMP against either        three bytes differ, at body offsets
 *                                    +0x44, +0x58 and +0x6a: 5c/58/5c here
 *                                    against 90/8c/90 there
 *
 * Those six bytes are the low bytes of four-byte `this`-relative
 * displacements -- 0x2f5c/0x2f58/0x2f5c against 0x2f90/0x2f8c/0x2f90 -- so
 * the ONLY difference between the MP source and the other two is which of the
 * two (pointer, length) pairs feeds the scrambler.  `generateMP` transmits
 * `V90MP`'s bit vector; generateCPd and generateSUVd BOTH transmit `V90CP`'s.
 * That the CP pair is spelled twice under two names is the object's, not a
 * copy left here for symmetry.
 *
 * THEY ARE WRITTEN OUT THREE TIMES RATHER THAN FACTORED.  The blob emits
 * three full 167-byte bodies; a shared helper that the compiler CALLS rather
 * than inlines is a missing call by 7480's rule, and one it inlines would
 * have to be proved to inline at all three sites before the source could be
 * believed.  This file already duplicates for the same reason -- generateRdRt
 * against generateRdRtNot, generateDataSymbolBeforeFPE against ...RRN -- and
 * the three below were each derived from `tools/dis.py` separately and only
 * then diffed against one another.
 *
 * ---------------------------------------------------------------------------
 * THE RETURN TYPE IS NOT FORCED BY THESE 167 BYTES, and saying so is the
 * point.  The tail is `movswl 0x22(%esp),%eax ; add $0x24,%esp ; pop ; pop ;
 * ret`, which is what GCC 3.4.2 emits for a `short` return AND for an `int`
 * return of a `short` local; the two are indistinguishable here.  What
 * settles it is the class:
 *
 *   - `generateDataSymbolBeforeFPE` and `...RRN` are declared `short` above,
 *     are already written and differentially tested, and close with the
 *     byte-identical idiom (`movswl 0x16(%esp),%eax` at +0x2d79b);
 *   - the six sequence readers are `short` for the same reason;
 *   - `generateSymbol` is the ONE member of this class declared `int`, and
 *     the header's evidence for that is the `cwtl` AT ITS CALL SITES -- which
 *     is exactly the evidence a callerless function cannot have.
 *
 * The mangled names carry the parameter types and not the return type, so
 * they say nothing here either.
 *
 * ---------------------------------------------------------------------------
 * TWO THINGS THE INSTRUCTIONS FORCE.
 *
 * THE BIT COUNT IS RE-READ AFTER THE SCRAMBLER RETURNS -- `mov 0x2f5c(%esi),
 * %ebx` at +0x2dc12 for the scrambler's third argument and `mov 0x2f5c(%esi),
 * %ecx` again at +0x2dc38 for the converter's second.  That is the compiler
 * being unable to prove `Scrambler<h,h>::process` leaves `*this` alone, not a
 * second expression in the source; it is one field named twice and must not
 * be "tidied" into a local, which would delete the reload.
 *
 * THE STATUS IS DISCARDED, BOTH TIMES.  Both `V90BitsToSymbol::process`
 * overloads answer a status in %eax and neither answer is looked at; the
 * value returned is the `short` the second one wrote through its pointer.
 *
 * ---------------------------------------------------------------------------
 * THE SYMBOL SLOT IS UNINITIALISED, WHICH IS D661's SHAPE ON THE OTHER
 * PARAMETER.  Nothing writes 0x22(%esp) before the call, and
 * `V90BitsToSymbol::process(unsigned int &, short *)` writes through
 * `outSymbols` only when `symbolsBlockSize` is non-zero AND the arm it takes
 * has something to hand over -- with `symbolsBlockSize` zero it prints
 * SIZE_NOT_SET and returns, and on the underflow arm it copies
 * `symbolsDone` entries, which is none when that is zero.  So there are
 * reachable configurations in which all three return whatever was on the
 * stack.  Reproduced rather than guarded, exactly as D661 is: an initialiser
 * would be an instruction the object does not have.  `test/unit/
 * t_v90p4seq.cpp` keeps those configurations out of its grid and says so,
 * which is D561's rule -- a trial that reaches undefined behaviour in the
 * reconstruction is not a differential trial.
 *
 * D661 ITSELF DOES NOT APPLY HERE, and a reader coming from the two data
 * pumps will assume it does.  Their `nofBits` is passed uninitialised; ours
 * is written by `nofBitsForNextTime` before it is passed, which is the
 * `mov %eax,0x1c(%esp)` at +0x2dbe4 -- the same store the branch then tests.
 * ===========================================================================
 */
/*
 * ===========================================================================
 * THE THREE TRAINING SOURCES -- generateB1d (.text+0x2d9f0), generateTRN2d
 * (+0x2da90) and generateEd (+0x2db30), 149 bytes each, immediately before
 * `generateMP` in the blob as here.  The "obvious next batch" of the block
 * comment below, claimed by the VPcmV34Main leaf pass.
 *
 * The same shape as the three message sources below with the scrambler's
 * input GENERATED rather than fetched: ask the converter how many bits it
 * wants; if none, drain a symbol; otherwise scramble that many constant ones
 * (B1d, TRN2d) or zeros (Ed) into `scrambledBits`, feed them in, and drain.
 * The branch is the other way round from `generateMP`'s -- the drain-only
 * arm returns EARLY here (`test %eax,%eax; jne` at +0x2da0a) where the
 * message sources fall through -- and the count is the LOCAL `nofBits`, not
 * a field, so nothing is re-read after the scrambler returns.
 *
 * B1d AND TRN2d ARE BYTE-IDENTICAL BODIES UNDER TWO NAMES (all 160 bytes,
 * `cmp` against `cmp`), both `processAllOnes`; Ed differs in the one callee,
 * `processAllZeros`.  Written out three times because the object emits three
 * bodies, exactly as for the message sources below.
 *
 * `short`, by the class convention the block comment below derives; the
 * status both `V90BitsToSymbol::process` overloads return is discarded, both
 * times, as there.  The symbol slot is uninitialised on the same terms too.
 * ===========================================================================
 */
short
V90Phase4Modulator::generateB1d()
{
	unsigned int nofBits;
	short symbol;

	nofBits = bitsToSymbol->nofBitsForNextTime();
	if (nofBits != 0) {
		scrambler.processAllOnes(scrambledBits, nofBits);
		bitsToSymbol->process(scrambledBits, nofBits);
	}
	bitsToSymbol->process(nofBits, &symbol);
	return symbol;
}

short
V90Phase4Modulator::generateTRN2d()
{
	unsigned int nofBits;
	short symbol;

	nofBits = bitsToSymbol->nofBitsForNextTime();
	if (nofBits != 0) {
		scrambler.processAllOnes(scrambledBits, nofBits);
		bitsToSymbol->process(scrambledBits, nofBits);
	}
	bitsToSymbol->process(nofBits, &symbol);
	return symbol;
}

short
V90Phase4Modulator::generateEd()
{
	unsigned int nofBits;
	short symbol;

	nofBits = bitsToSymbol->nofBitsForNextTime();
	if (nofBits != 0) {
		scrambler.processAllZeros(scrambledBits, nofBits);
		bitsToSymbol->process(scrambledBits, nofBits);
	}
	bitsToSymbol->process(nofBits, &symbol);
	return symbol;
}

short
V90Phase4Modulator::generateMP()
{
	unsigned int nofBits;
	short symbol;

	nofBits = bitsToSymbol->nofBitsForNextTime();
	if (nofBits != 0) {
		scrambler.process(mpBits, scrambledBits, mpBitCount);
		bitsToSymbol->process(scrambledBits, mpBitCount);
	}
	bitsToSymbol->process(nofBits, &symbol);
	return symbol;
}

/*
 * ===========================================================================
 * THE TWO SYMBOL PUMPS -- generateV90Symbol (.text+0x2dc80, 0x8bb = 2,235
 * bytes) and generateV92Symbol (+0x2e6a0, 0xf52 = 3,922).
 *
 * ONE `switch` OVER `state` EACH, AND THE JUMP TABLES ARE WHAT SAY SO.
 * `.rodata+0xa94` holds twenty-eight `R_386_32 .text` entries for the V.90
 * pump and `.rodata+0xb04` thirty-one for the V.92 one, so the case labels run
 * 0x00..0x1b and 0x00..0x1e respectively and `cmp $0x1b,%eax ; ja` /
 * `cmp $0x1e,%eax ; ja` is the range check GCC puts in front of each.  A slot
 * holding the default label is a state with no `case`; a slot holding anything
 * else is a `case` this file has to spell.  Read with `tools/tabdump.py --at
 * .rodata:0xa94 --type u32 --count 28`, which prints the relocations rather
 * than dropping them.
 *
 * WHICH LABELS EACH PUMP HAS IS THE WHOLE OF THE V.90/V.92 DIFFERENCE IN
 * SHAPE.  The V.90 table dispatches 0x04 and 0x0d..0x0f -- the MP ladder --
 * and sends 0x05..0x0c to the default edge; the V.92 table does exactly the
 * reverse, dispatching the SUVd/CPd ladder at 0x05..0x0c against
 * `cpSequenceSymbols` and sending 0x04 and 0x0d..0x0f to the default.  That is
 * §9.4.1 the right way round: under V.90 the digital modem sends MP, and under
 * V.92 the rate renegotiation puts CP in its place.  V.92 also has 0x18, 0x19,
 * 0x1a, 0x1b, 0x1c, 0x1d and 0x1e, which is the silence/Rt/Rf ladder the
 * shorter table stops before.
 *
 * THE ARM SHARED BY NINE V.90 STATES AND FOURTEEN V.92 ONES IS THIS:
 *
 *     nofBits = bitsToSymbol->nofBitsForNextTime();
 *     if (nofBits != 0) {
 *             <fill>;
 *             bitsToSymbol->process(scrambledBits, <count>);
 *     }
 *     bitsToSymbol->process(nofBits, &sym);
 *
 * -- one variable for the demand and for the status, because the object uses
 * one stack slot for both and `process(unsigned int &, short *)` writes the
 * reference on the way out.  Three fills appear, and which one a state gets is
 * the state's own business: `scrambler.processAllOnes` for TRN2d, B1d and the
 * terminated state, `processAllZeros` for Ed, and `scrambler.process` over the
 * message vector -- `mpBits`/`mpBitCount` under V.90, `cpBits`/`cpBitCount`
 * under V.92 -- for every state that is emitting a sequence.  THE MESSAGE FILL
 * PASSES THE MESSAGE'S OWN LENGTH AND NOT `nofBits`: `mov 0x2f5c(%esi),%ebx ;
 * mov %ebx,0xc(%esp)` at +0x2e34f, where the all-ones fill passes the demand
 * it was just given.
 *
 * EACH `case` HAS ITS OWN PAIR OF LOCALS, and the object proves it: the nine
 * V.90 arms use nine disjoint pairs of stack slots (0x1c/0x40, 0x20/0x42, ...
 * 0x38/0x4e) where one shared pair would have been one.  So they are declared
 * inside the arms.
 *
 * THE MEMBERS ARE CALLED, NOT COPIED, AND A REDUNDANT GUARD IS THE EVIDENCE.
 * The V.90 pump's `case P4M_STATE_MP_NOT` re-tests `cmpl $0xe,0x4(%esi)` at
 * +0x2e0a4 -- a question the jump table has already answered.  GCC does not
 * invent a guard it can fold away, so what is inlined there is `exitMPNot()`,
 * whose own body opens with `state == P4M_STATE_MP_NOT`.  The same argument
 * names `enterRepeatedCPd()` inside the V.92 SUVd arm (its seven stores and
 * its trailing `symbolCount = 0` appear verbatim and in order),
 * `resetRRNSecondSection()` inside the V.92 RtNot arm, and
 * `generateRi`/`generateRiNot`/`generateRdRt`/`generateRdRtNot`/`generateRf`/
 * `generateRfNot`/`generateDataSymbolBeforeRRN`/`generateDataSymbolBeforeFPE`
 * wherever their bodies appear.  Where a member's OWN guard would reject --
 * `case P4M_STATE_UNNAMED_01` cannot call `exitRi()`, which guards on
 * `state == P4M_STATE_RI` -- the arm is written out, and that is why the
 * boundary block appears more than once below.
 *
 * THE COMPARISON CONSTANTS ARE LITERALS ON PURPOSE.  0x18, 0x120, 0x180,
 * 0x3e7c, 0x95f and 0x320 are how long the object lets a state run; naming
 * them would be inventing a meaning for a number the object only ever
 * compares.  0x3e7c against `symbolCount` and then `0x3e7c` again in the
 * message is ONE `symbolCount` in the source -- GCC 3.4 propagates the
 * constant out of the equality test, which it does identically at +0x2ddd1,
 * +0x2df29 and +0x2e1b8.
 *
 * A RESIDUAL SIZE GAP ON BOTH PUMPS, DECLINED AFTER A REAL ENUMERATION.
 * `generateV90Symbol` is SIZE, ours 0x8b4 against the object's 0x8bb (-7);
 * `generateV92Symbol` is SIZE, ours 0xf4f against 0xf52 (+3).  Both have the
 * OBJECT'S OWN instruction count -- 541 for 541, 941 for 941
 * (`instrcount.py`) -- so per lever 2 this is encoding, not a missing or
 * extra statement, and both pumps' every `jmp`/`jcc` was walked and paired by
 * TARGET STATE rather than trusted from the total: e.g. `generateRi()`'s own
 * return in `generateV90Symbol` is a near `jmp` (5 bytes) in the object,
 * because the object places `case P4M_STATE_RI` far from the shared return
 * path, and a short `jmp` (2 bytes) here, because our compile places it
 * close -- one clean 3-byte swap, with more of the same sign scattered
 * through the rest of the switch to make up the other 4.  `generateV92Symbol`
 * has the mirror shape at the trampoline shared by `P4M_STATE_UNNAMED_13` and
 * `P4M_STATE_UNNAMED_17` (`symbol = 0; break;`): short in the object (2
 * bytes, ~0x37 from the return path) and near in ours (5 bytes, ~0x2d1 from
 * it).  So the object's case bodies sit in a PHYSICAL ORDER in `.text` that
 * is not ours, and every short/near choice downstream of that follows.
 *
 * TESTED AND REFUTED: it is not source case-label order.  Lever 1 says
 * enumerate before reading a cell, and the candidate here is small and
 * concrete -- move ONE case group to a different position in the switch and
 * rebuild.  Both pumps were tried at their two most extreme placements: the
 * `UNNAMED_13`/`UNNAMED_17` (`UNNAMED_13`/`RT`/`RT_NOT` in the V.90 pump)
 * group as the very FIRST case right after `switch (state) {`, and again as
 * the very LAST case immediately before `default:`.  All four rebuilds
 * (`make tc`) produced OBJECT CODE IDENTICAL, byte for byte, to the
 * unperturbed source -- same SIZE verdict, same byte count, same `--why`
 * row.  That is lever 1's own "branch that would kill it": the compiler
 * emits the same layout for every spelling tried, so the map is constant and
 * this is not a statement/case-order difference open to that lever.  (A
 * `nm -S` diff before/after each rebuild confirms the trampoline's compiled
 * address does not move.)
 *
 * DECLINED, not fitted.  The actual carrier is GCC 3.4's basic-block layout
 * for this switch's jump table -- `-freorder-blocks` under `-O3`, operating
 * on properties of the compiled CFG this project has no source-level lever
 * to steer once case order is shown not to be it -- and reaching for one
 * anyway (`__attribute__((noinline))`, splitting a case, `volatile`) would be
 * fitting the compiler rather than deriving the source, which CLAUDE.md
 * forbids.  `--why`'s "row 28 MNEMONIC xor vs mov" on both pumps is
 * downstream of the same cause: `alpha_why` walks each object's instructions
 * in that object's OWN address order, so a different physical block order
 * puts genuinely different code at the same row index without either side
 * missing or gaining an instruction -- it is not a second, independent
 * defect, and closing the SIZE gap (if a lever for it is ever found) should
 * be expected to close this row too.
 * ===========================================================================
 */
short
V90Phase4Modulator::generateV90Symbol()
{
	short symbol;

	symbolCount++;
	eventCode = 0;

	switch (state) {
	case P4M_STATE_RI:
		symbol = generateRi();
		break;

	case P4M_STATE_UNNAMED_01:
		symbol = generateRi();
		if (symbolCount % V90P4M_RI_PERIOD == 0) {
			edprintf("V90Phase4Modulator: enter RiNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RI_NOT;
			symbolCount = 0;
		}
		break;

	case P4M_STATE_RI_NOT:
		symbol = generateRiNot();
		if (symbolCount == 0x18) {
			state = P4M_STATE_TRN2D;
			symbolCount = 0;
			edprintf("V90Phase4Modulator: enter TRN2d\r\n");
			scrambler.reset(0);
		}
		break;

	case P4M_STATE_TRN2D: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.processAllOnes(scrambledBits, nofBits);
			bitsToSymbol->process(scrambledBits, nofBits);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount == 0x3e7c) {
			if (mp == 0) {
				state = P4M_STATE_UNNAMED_13;
				symbolCount = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase4Modulator: ERROR: Null " "MP @ end of TRN2d\r\n");
				break;
			}
			state = nextStateAfterTRN2d;
			edprintf("V90Phase4Modulator: enter %s @ %d\n",
				 state == P4M_STATE_MP ? "MP"
						       : "DIRECTLY to MPNot",
				 symbolCount);
			symbolCount = 0;
			mpBits = mp->getBitVector(mpBitCount);
			mpSequenceSymbols = 6 * mpBitCount / mp->groupSize;
		}
		break;
	}

	case P4M_STATE_MP: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(mpBits, scrambledBits, mpBitCount);
			bitsToSymbol->process(scrambledBits, mpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;
		break;
	}

	case P4M_STATE_UNNAMED_0D: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(mpBits, scrambledBits, mpBitCount);
			bitsToSymbol->process(scrambledBits, mpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount % mpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter MPNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_MP_NOT;
			symbolCount = 0;
			mpBits = mp->getBitVector(mpBitCount);
			mpSequenceSymbols = 6 * mpBitCount / mp->groupSize;
		}
		break;
	}

	case P4M_STATE_MP_NOT: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(mpBits, scrambledBits, mpBitCount);
			bitsToSymbol->process(scrambledBits, mpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (delayedMpNotExit != 0) {
			delayedMpNotExit = 0;
			exitMPNot();
		}
		break;
	}

	case P4M_STATE_UNNAMED_0F: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(mpBits, scrambledBits, mpBitCount);
			bitsToSymbol->process(scrambledBits, mpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount % mpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter Ed @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			state = P4M_STATE_ED;
			word_2f64 = bitsToSymbol->extraSymbols + 12;
		}
		break;
	}

	case P4M_STATE_ED: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.processAllZeros(scrambledBits, nofBits);
			bitsToSymbol->process(scrambledBits, nofBits);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount == word_2f64) {
			if (mappingParams == 0) {
				state = P4M_STATE_UNNAMED_13;
				symbolCount = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase4Modulator: ERROR: Null " "dataPhaseMappingParams @ end of "
					    "Ed\r\n");
				break;
			}
			edprintf("V90Phase4Modulator: enter B1d @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_B1D;
			symbolCount = 0;
			bitsToSymbol->resetNoSpectral(mappingParams, pcmType);
			scrambler.reset(0);
		}
		break;
	}

	case P4M_STATE_B1D: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.processAllOnes(scrambledBits, nofBits);
			bitsToSymbol->process(scrambledBits, nofBits);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount == 0x120) {
			edprintf("V90Phase4Modulator: Phase4 Terminated @ " "%d\r\n", symbolCount);
			state = P4M_STATE_TERMINATED;
			symbolCount = 0;
			eventCode = 7;
		}
		break;
	}

	case P4M_STATE_TERMINATED: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.processAllOnes(scrambledBits, nofBits);
			bitsToSymbol->process(scrambledBits, nofBits);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;
		break;
	}

	case P4M_STATE_UNNAMED_13:
	case P4M_STATE_RT:
	case P4M_STATE_RT_NOT:
		symbol = 0;
		break;

	case P4M_STATE_UNNAMED_14:
		symbol = generateDataSymbolBeforeRRN();
		break;

	case P4M_STATE_RD:
		symbol = generateRdRt();
		if (symbolCount == 0x180) {
			edprintf("V90Phase4Modulator: enter RdNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RD_NOT;
			symbolCount = 0;
		}
		break;

	case P4M_STATE_RD_NOT:
		symbol = generateRdRtNot();
		if (symbolCount == 0x18) {
			edprintf("V90Phase4Modulator: enter TRN2d @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_TRN2D;
			symbolCount = 0;
			bitsToSymbol->resetNoSpectral(mappingParams2, pcmType);
			edprintf("V90Phase4Modulator: TRN2d spectral " "parameters:\r\n");
			displaySpectralParams(mappingParams2);
			edprintf("V90Phase4Modulator: TRN2d D = %d\r\n",
				 mappingParams2->word_0);
		}
		break;

	default:
		symbol = 0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Phase4Modulator: Illegal " "state\r\n");
		break;
	}

	return symbol;
}

/*
 * generateCPd -- .text+0x2e540, 167 bytes.  The block above generateMP is the
 * derivation for all three; what is particular to this one is the field pair,
 * `cpBits`/`cpBitCount` at +0x2f8c/+0x2f90, read at +0x2e582, +0x2e596 and
 * +0x2e5a8.
 */
short
V90Phase4Modulator::generateCPd()
{
	unsigned int nofBits;
	short symbol;

	nofBits = bitsToSymbol->nofBitsForNextTime();
	if (nofBits != 0) {
		scrambler.process(cpBits, scrambledBits, cpBitCount);
		bitsToSymbol->process(scrambledBits, cpBitCount);
	}
	bitsToSymbol->process(nofBits, &symbol);
	return symbol;
}

/*
 * generateSUVd -- .text+0x2e5f0, 167 bytes, and BYTE-FOR-BYTE the function
 * above: all 167 compared equal, so the CP pair is read here too, at +0x2e632,
 * +0x2e646 and +0x2e658.  Two names for one body is what the object has, and
 * the SUVd and CPd states are two states, so it is kept as two members rather
 * than aliased.  Any test able to separate this from `generateCPd` would be
 * measuring its own fixture: they cannot differ on any input.
 */
short
V90Phase4Modulator::generateSUVd()
{
	unsigned int nofBits;
	short symbol;

	nofBits = bitsToSymbol->nofBitsForNextTime();
	if (nofBits != 0) {
		scrambler.process(cpBits, scrambledBits, cpBitCount);
		bitsToSymbol->process(scrambledBits, cpBitCount);
	}
	bitsToSymbol->process(nofBits, &symbol);
	return symbol;
}

/*
 * ===========================================================================
 * generateV92Symbol -- .text+0x2e6a0, 0xf52 = 3,922 bytes.
 *
 * THE SAME SHAPE AND SEVEN GENUINE DIFFERENCES, and they are listed here
 * because everything else is the arm above with `cpSequenceSymbols` in place
 * of `mpSequenceSymbols`.  Nothing below was transcribed from the V.90 pump;
 * each is at the address named.
 *
 *  1. RiNot ends differently.  V.90 prints "enter TRN2d" and then
 *     `scrambler.reset(0)`; V.92 prints "RiNot Terminated" -- .rodata.str1.4
 *     +0x8504, a message no other member of the class references -- and does
 *     NOT reset the scrambler.  +0x2ef60..+0x2efba against +0x2e245..+0x2e2b0,
 *     and the two are otherwise instruction for instruction the same.
 *
 *  2. TRN2d hands on to SUVd and not to `nextStateAfterTRN2d`.  The null guard
 *     is on `cp` rather than `mp` (+0x2f003), the message names CP, the state
 *     is 5 unconditionally, and BOTH exits set `eventCode` to 4 -- the only
 *     site in the class that stores that value.  Where V.90 rebuilds the MP
 *     sequence, V.92 sets `cp->word_00 = 1`, copies `word_0028` into
 *     `cp->word_ca0` and rebuilds the CP sequence.
 *
 *  3. The whole SUVd/CPd ladder at 0x05..0x0c exists only here, and its Ed
 *     entry at 0x09/0x0a, its FinalSUVd entry at 0x0c and its CPd termination
 *     at 0x07 are what carry the SUV half of the rate renegotiation.
 *
 *  4. SUVd counts CPd repetitions.  `if (byte_001c) word_0018++` at +0x2ee7c,
 *     and once `word_0018` passes `word_0040 + 0x320` the arm runs
 *     `enterRepeatedCPd()`.  Nothing in the V.90 pump reads either field.
 *
 *  5. Ed has a THIRD exit.  `word_0024 && word_002c && !word_0030` sends it to
 *     "enter Silence" and to state 0x17 or 0x18 chosen by `word_0034`
 *     (+0x2f5c5: `cmp $0x1,%edx ; sbb %eax,%eax ; not %eax ; add $0x18,%eax`,
 *     which is 0x17 for non-zero and 0x18 for zero).  V.90's Ed has the null
 *     guard and the B1d arm and nothing else.
 *
 *  6. RdNot copies `mappingParams2->word_0` into `cp->word_3ba8` before it
 *     resets the converter (+0x2ea60).  V.90's RdNot is the same five calls
 *     without that store.
 *
 *  7. The silence/Rt/Rf ladder at 0x18..0x1e is here alone, and RtNot's
 *     boundary runs `resetRRNSecondSection()` -- its seven stores in its own
 *     order at +0x2eb85..+0x2eba8 -- before rebuilding the CP sequence.
 *
 * The RANGE CHECK is `cmp $0x1e` and not `cmp $0x1b`, which is what makes
 * 0x1c, 0x1d and 0x1e reachable here and not there.
 *
 * RESIDUAL: SIZE, ours 0xf4f against 0xf52 (+3 to the object), same 941
 * instructions both sides.  See the DECLINE note above `generateV90Symbol`
 * -- the mechanism, the two-position enumeration that refuted case order,
 * and the "row 28" reading are all shared between the two pumps and
 * recorded there rather than twice.
 * ===========================================================================
 */
short
V90Phase4Modulator::generateV92Symbol()
{
	short symbol;

	symbolCount++;
	eventCode = 0;

	switch (state) {
	case P4M_STATE_RI:
		symbol = generateRi();
		break;

	case P4M_STATE_UNNAMED_01:
		symbol = generateRi();
		if (symbolCount % V90P4M_RI_PERIOD == 0) {
			edprintf("V90Phase4Modulator: enter RiNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RI_NOT;
			symbolCount = 0;
		}
		break;

	case P4M_STATE_RI_NOT:
		symbol = generateRiNot();
		if (symbolCount == 0x18) {
			state = P4M_STATE_TRN2D;
			symbolCount = 0;
			edprintf("V90Phase4Modulator: RiNot Terminated\r\n");
		}
		break;

	case P4M_STATE_TRN2D: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.processAllOnes(scrambledBits, nofBits);
			bitsToSymbol->process(scrambledBits, nofBits);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount == 0x3e7c) {
			if (cp == 0) {
				state = P4M_STATE_UNNAMED_13;
				symbolCount = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase4Modulator: ERROR: Null " "CP @ end of TRN2d\r\n");
			} else {
				state = P4M_STATE_SUVD;
				edprintf("V90Phase4Modulator: enter SUVd @ " "%d\r\n", symbolCount);
				symbolCount = 0;
				cp->word_00 = 1;
				cp->word_ca0 = word_0028;
				cp->infoToBits();
				cpBits = cp->getBitVector(cpBitCount);
				cpSequenceSymbols =
				    6 * cpBitCount / cp->word_3ba8;
			}
			eventCode = 4;
		}
		break;
	}

	case P4M_STATE_SUVD: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(cpBits, scrambledBits, cpBitCount);
			bitsToSymbol->process(scrambledBits, cpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (byte_001c != 0)
			word_0018++;
		if (symbolCount % cpSequenceSymbols == 0 && symbolCount != 0) {
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
			if (word_0018 > word_0040 + 0x320)
				enterRepeatedCPd();
		}
		break;
	}

	case P4M_STATE_UNNAMED_06: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(cpBits, scrambledBits, cpBitCount);
			bitsToSymbol->process(scrambledBits, cpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount % cpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter CPd @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			cp->word_00 = 0;
			state = P4M_STATE_CPD;
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
			word_2fa0 = 1;
		}
		break;
	}

	case P4M_STATE_CPD: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(cpBits, scrambledBits, cpBitCount);
			bitsToSymbol->process(scrambledBits, cpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount == cpSequenceSymbols) {
			edprintf("V90Phase4Modulator: CPd Terminated @ " "%d\r\n", symbolCount);
			word_0018 = 0;
			byte_001c = 1;
			state = P4M_STATE_SUVD;
			symbolCount = 0;
			cp->word_00 = 1;
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
		}
		break;
	}

	case P4M_STATE_REPEATED_CPD: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(cpBits, scrambledBits, cpBitCount);
			bitsToSymbol->process(scrambledBits, cpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount % cpSequenceSymbols == 0 && symbolCount != 0) {
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
		}
		break;
	}

	case P4M_STATE_UNNAMED_09: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(cpBits, scrambledBits, cpBitCount);
			bitsToSymbol->process(scrambledBits, cpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount % cpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter Ed @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			state = P4M_STATE_ED;
			word_2f64 = bitsToSymbol->extraSymbols + 12;
		}
		break;
	}

	case P4M_STATE_UNNAMED_0A: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(cpBits, scrambledBits, cpBitCount);
			bitsToSymbol->process(scrambledBits, cpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount % cpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter Ed @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			state = P4M_STATE_ED;
			word_2f64 = bitsToSymbol->extraSymbols + 12;
		}
		break;
	}

	case P4M_STATE_FINAL_SUVD: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(cpBits, scrambledBits, cpBitCount);
			bitsToSymbol->process(scrambledBits, cpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount == cpSequenceSymbols) {
			edprintf("V90Phase4Modulator: enter Ed @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			state = P4M_STATE_ED;
			word_2f64 = bitsToSymbol->extraSymbols + 12;
		}
		break;
	}

	case P4M_STATE_UNNAMED_0C: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.process(cpBits, scrambledBits, cpBitCount);
			bitsToSymbol->process(scrambledBits, cpBitCount);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount % cpSequenceSymbols == 0) {
			edprintf("V90Phase4Modulator: enter FinalSUVd @ " "%d\r\n", symbolCount);
			state = P4M_STATE_FINAL_SUVD;
			symbolCount = 0;
			cp->word_00 = 1;
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
		}
		break;
	}

	case P4M_STATE_ED: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.processAllZeros(scrambledBits, nofBits);
			bitsToSymbol->process(scrambledBits, nofBits);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount == word_2f64) {
			if (word_0024 != 0 && word_002c != 0 &&
			    word_0030 == 0) {
				edprintf("V90Phase4Modulator: enter Silence @ " "%d\r\n", symbolCount);
				symbolCount = 0;
				state = word_0034 != 0
				    ? P4M_STATE_UNNAMED_17
				    : P4M_STATE_UNNAMED_18;
				break;
			}
			if (mappingParams == 0) {
				state = P4M_STATE_UNNAMED_13;
				symbolCount = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase4Modulator: ERROR: Null " "dataPhaseMappingParams @ end of "
					    "Ed\r\n");
				break;
			}
			edprintf("V90Phase4Modulator: enter B1d @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_B1D;
			symbolCount = 0;
			bitsToSymbol->resetNoSpectral(mappingParams, pcmType);
			scrambler.reset(0);
		}
		break;
	}

	case P4M_STATE_B1D: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.processAllOnes(scrambledBits, nofBits);
			bitsToSymbol->process(scrambledBits, nofBits);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;

		if (symbolCount == 0x120) {
			edprintf("V90Phase4Modulator: Phase4 Terminated @ " "%d\r\n", symbolCount);
			state = P4M_STATE_TERMINATED;
			symbolCount = 0;
			eventCode = 7;
		}
		break;
	}

	case P4M_STATE_TERMINATED: {
		unsigned int nofBits;
		short sym;

		nofBits = bitsToSymbol->nofBitsForNextTime();
		if (nofBits != 0) {
			scrambler.processAllOnes(scrambledBits, nofBits);
			bitsToSymbol->process(scrambledBits, nofBits);
		}
		bitsToSymbol->process(nofBits, &sym);
		symbol = sym;
		break;
	}

	case P4M_STATE_UNNAMED_13:
	case P4M_STATE_UNNAMED_17:
		symbol = 0;
		break;

	case P4M_STATE_UNNAMED_14:
		symbol = generateDataSymbolBeforeRRN();
		break;

	case P4M_STATE_RD:
		symbol = generateRdRt();
		if (symbolCount == 0x180) {
			edprintf("V90Phase4Modulator: enter RdNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RD_NOT;
			symbolCount = 0;
		}
		break;

	case P4M_STATE_RD_NOT:
		symbol = generateRdRtNot();
		if (symbolCount == 0x18) {
			edprintf("V90Phase4Modulator: enter TRN2d @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			state = P4M_STATE_TRN2D;
			cp->word_3ba8 = mappingParams2->word_0;
			bitsToSymbol->resetNoSpectral(mappingParams2, pcmType);
			edprintf("V90Phase4Modulator: TRN2d spectral " "parameters:\r\n");
			displaySpectralParams(mappingParams2);
			edprintf("V90Phase4Modulator: TRN2d D = %d\r\n",
				 mappingParams2->word_0);
		}
		break;

	case P4M_STATE_UNNAMED_18:
		symbol = 0;
		if (symbolCount > 0x95f &&
		    symbolCount % V90P4M_RI_PERIOD == 0) {
			edprintf("V90Phase4Modulator: enter Rt @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RT;
			symbolCount = 0;
		}
		break;

	case P4M_STATE_UNNAMED_19:
		symbol = 0;
		if (symbolCount % V90P4M_RI_PERIOD == 0) {
			edprintf("V90Phase4Modulator: enter Rt @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RT;
			symbolCount = 0;
		}
		break;

	case P4M_STATE_RT:
		symbol = generateRdRt();
		if (symbolCount == 0x180) {
			edprintf("V90Phase4Modulator: enter RtNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RT_NOT;
			symbolCount = 0;
		}
		break;

	case P4M_STATE_RT_NOT:
		symbol = generateRdRtNot();
		if (symbolCount == 0x18) {
			edprintf("V90Phase4Modulator: enter SUVd at RRN @ " "%d\r\n", symbolCount);
			state = P4M_STATE_SUVD;
			symbolCount = 0;
			resetRRNSecondSection();
			cp->word_00 = 1;
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
		}
		break;

	case P4M_STATE_UNNAMED_1C:
		symbol = generateDataSymbolBeforeFPE();
		break;

	case P4M_STATE_RF:
		symbol = generateRf();
		if (symbolCount == 0x180) {
			edprintf("V90Phase4Modulator: enter RfNot @ %d\r\n",
				 symbolCount);
			state = P4M_STATE_RF_NOT;
			symbolCount = 0;
		}
		break;

	case P4M_STATE_RF_NOT:
		symbol = generateRfNot();
		if (symbolCount == 0x18) {
			edprintf("V90Phase4Modulator: enter SUVd @ %d\r\n",
				 symbolCount);
			symbolCount = 0;
			state = P4M_STATE_SUVD;
			cp->word_00 = 1;
			cp->word_3ba8 = mappingParams->word_0;
			cp->word_ca0 = word_0028;
			cp->infoToBits();
			cpBits = cp->getBitVector(cpBitCount);
			cpSequenceSymbols = 6 * cpBitCount / cp->word_3ba8;
		}
		break;

	default:
		symbol = 0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Phase4Modulator: Illegal " "state\r\n");
		break;
	}

	return symbol;
}

/*
 * ===========================================================================
 * `V90Phase4Modulator::generateSymbol` -- .text+0x2f600, 45 bytes
 *
 * THE SMALLEST MEMBER OF THE CLASS AND THE ONLY ONE ITS CALLER USES.
 * `V90Modulator::progress` calls this and neither pump directly, in both of
 * its phase 4 arms, so this fork is where the session flag decides which
 * modulation a V.90 call transmits.  `reset` makes the same fork for its
 * warm-up loop and reloads the flag on every iteration; here it is read once
 * and the whole function is that test plus two calls.
 *
 * NOT A TAIL JUMP, and that is the return type.  0x2f610 and 0x2f623 are
 * `call`s followed by `cwtl`, where a forward to a callee of the same type
 * would be a `jmp`: the widening is work done after the callee returns, so
 * the caller's type is wider than the callee's.  See V90Phase4Modulator.h.
 * ===========================================================================
 */
int
V90Phase4Modulator::generateSymbol()
{
	if (sessionFlag)
		return generateV92Symbol();

	return generateV90Symbol();
}
