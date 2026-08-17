/*
 * V92Phase4Modulator.h -- the V.92 phase 4 upstream symbol source, PARTIAL.
 *
 * Reconstructed from dsplibs.o.  The class has 34 distinct members; TWENTY-SIX
 * of them are written in src/pump/v90/V92Phase4Modulator.cpp -- the constructor
 * (C1 at .text+0x17970 and C2 at +0x17a20, 164 bytes each), the destructor (D2
 * at +0x16de0 and D1 at +0x16e40, 87 bytes each) and the twenty-four members of
 * the "generate / recived / exit / resetBefor" surface.  The EIGHT not written
 * are `generateSymbol` (4,055 B), `recivedCPtag`,
 * `recivedPartOneSilenceRrnSUVtag`, `reset`, `recivedRt`, `recivedSUV`,
 * `recivedPartTwoSilenceRrnSUV` and `enterRepeatedCP`.  Every one of those
 * needs `V92CP::infoToBits`, directly or through `generateSymbol`, and that is
 * unwritten.  They are not declared here: a declaration whose signature is
 * guessed is worse than no declaration.
 *
 * THE OBJECT'S SPELLING OF "received" IS "recived" throughout, and it is the
 * mangling's -- `_ZN18V92Phase4Modulator13recivedSUVtagEv`.  Reproduced.
 *
 * THE OBJECT IS 0x1cc BYTES AND THAT IS THE ALLOCATION, not a bound.
 * `V92Modulator::V92Modulator` builds it:
 *
 *     152b9:  c7 04 24 cc 01 00 00   movl $0x1cc,(%esp)
 *     152c0:  e8 ..                  call sysdep_malloc
 *     152e6:  e8 ..                  call V92Phase4Modulator::V92Phase4Modulator
 *
 * which is the original compiler's own `sizeof` (finding 1249's oracle).  The
 * furthest field the constructor writes is the four bytes at +0x1c8, and
 * 0x1c8 + 4 == 0x1cc exactly.
 *
 * ---------------------------------------------------------------------------
 * THE SCRAMBLER IS A MEMBER AT +0x4c AND ITS TAPS ARE (5, 23)
 *
 * `Scrambler<unsigned char, unsigned char>::Scrambler(this + 0x4c, 5, 0x17,
 * 0x63)` opens the constructor and `Scrambler<unsigned char, unsigned char>::
 * ~Scrambler(this + 0x4c)` closes the destructor unconditionally -- the D1
 * variant, which is what GCC emits for a MEMBER rather than for a non-virtual
 * base.  Its own header pins it at 0x20 bytes, so it runs +0x4c..+0x6b and
 * the next named field at +0x6c meets it exactly.
 *
 * These are V.92's UPSTREAM taps and they are the same (5, 23, 99) that
 * `V92Phase3Modulator` builds its own `Scrambler<unsigned char, int>` with
 * (finding 1255).  The INTERMEDIATE type differs -- `<h,h>` here against
 * `<h,i>` there -- and that is the mangling's, not a choice.
 *
 * ---------------------------------------------------------------------------
 * THE CONSTRUCTOR WRITES ONE FIELD OF SOMEBODY ELSE'S OBJECT
 *
 *     179ed:  89 7b 74           mov %edi,0x74(%ebx)     this->cp = cp
 *     179f0:  89 b7 10 01 00 00  mov %esi,0x110(%edi)    cp->word_110 = 0
 *
 * -- a store straight through the third argument into the `V92CP` it has just
 * been handed, with %esi zeroed two instructions earlier.  Four more members
 * write it now (`recivedCP`, `resetBeforRRN`, `resetRRNSecondSection` and, a
 * byte lower, `recivedPartOneSilenceRrnSUV`), so the constructor is no longer
 * its only writer; V92CP.h's note that it is written from outside still holds.
 *
 * ---------------------------------------------------------------------------
 * THE STATE CODES ARE THE AUTHOR'S OWN, AND FOUR OF THEM HAVE NAMES
 *
 * `+0x00` is a small signed integer the twenty-four members switch on and
 * assign.  Four of its values are pinned by a FORMAT STRING that fires on the
 * assignment itself, which is the strongest evidence there is (CLAUDE.md's
 * evidence order, item 1) -- the message names the signal being entered and
 * the very next instruction stores the code:
 *
 *     "V92Phase4Modulator: enter E1u @ %d"   ->  +0x00 = 2     exitCPt
 *     "V92Phase4Modulator: enter E2u @ %d"   ->  +0x00 = 15    recivedEd,
 *                                                              recivedSUVtag
 *     "V92Phase4Modulator: enter Ru @ %d"    ->  +0x00 = 19    generateData-
 *                                                              SymbolBeforeRRN
 *     "V92Phase4Modulator: enter Rm @ %d"    ->  +0x00 = 26    generateData-
 *                                                              SymbolBeforeFPE
 *
 * The other nine values this batch sees -- 0, 1, 3, 4, 5, 9, 11, 12, 13 --
 * are written as bare numbers on purpose.  Nothing written names them, and a
 * guessed enumerator is exactly the wrong name that no test can fail on.
 *
 * THE FIELD IS A SIGNED `int` AND THAT IS FORCED, not chosen.  `recivedEd` and
 * `recivedSUVtag` lower their switch as `cmp $0x5; je; jl <default>; sub $0xc;
 * cmp $0x1; ja <default>` -- the `jl` at .text+0x17657 and +0x17331 is a
 * SIGNED branch, which GCC cannot emit for an unsigned switch value.  The
 * enum `V92Phase4ModulatorState` that `reset`'s mangling names is therefore
 * NOT this field's type as far as anything measured goes; `reset` is unwritten
 * and the question is left to whoever writes it.
 *
 * ---------------------------------------------------------------------------
 * WHAT +0x7c IS, AND WHAT ITS LENGTH IS NOT
 *
 * `+0x7c` is the bit block handed to `V92Mapper::process` for one symbol:
 * `Scrambler<h,h>::process` fills it a byte at a time, `processAllOnes` and
 * `processAllZeros` fill it in bulk, and `V92BitsToSymbol::process(unsigned
 * char *, unsigned)` consumes it.  **Its declared length here is the distance
 * to the next field that is known (`+0x1a8`), not a measured bound.**  Nothing
 * in this batch establishes how much of the 300 bytes is ever used; the number
 * of bits actually written is `+0x43` or `V92BitsToSymbol::nofBitsForNextTime`.
 *
 * Data member names are invented and descriptive (finding 226).
 */

#ifndef DSPLIB_V92PHASE4MODULATOR_H
#define DSPLIB_V92PHASE4MODULATOR_H

#include "dsplib/Scrambler.h"

class V92BitsToSymbol;
class V92CP;
class V92Mapper;
class V92MappingParams;
class V92Parameters;

/* The scrambler's three constructor arguments, in its own order: the near
 * tap, the far tap and the distance the restart point sits above the buffer's
 * base.  `mov $0x5`, `mov $0x17`, `mov $0x63` at .text+0x17973..+0x1798e. */
#define V92P4M_SCRAM_TAP1	5
#define V92P4M_SCRAM_TAP2	23
#define V92P4M_SCRAM_SLACK	99

/*
 * The four `state` codes a format string names.  See the block comment above
 * for the derivation and for why the other nine are left as bare numbers.
 */
#define V92P4M_STATE_E1U	2
#define V92P4M_STATE_E2U	15
#define V92P4M_STATE_RU		19
#define V92P4M_STATE_RM		26

/*
 * Three more, named by the six members added with `V92CP::infoToBits`.  Each
 * is the state stored immediately after a message that names it, and the
 * message is the author's own `.rodata.str1.4` text:
 *
 *   5   "on recivedRt enter SUV @ %d"          :0x3d44, then `movl $0x5`
 *   12  "on recivedSUV enter CPu @ %d"         :0x3c40, then `movl $0xc`
 *   13  "enter repeatedCPu @ %d"               :0x3be8, then `movl $0xd`
 *
 * The remaining nine (6, 8, 9, 10, 11, 23, 24, 29 and 1) stay bare: no string
 * fires on any of them.
 */
#define V92P4M_STATE_SUV	5
#define V92P4M_STATE_CPU	12
#define V92P4M_STATE_REPEATED_CPU 13

class V92Phase4Modulator {
public:
	V92Phase4Modulator(V92Parameters *params, V92BitsToSymbol *bitsToSymbol,
			   V92CP *cp, V92MappingParams *mappingParams);
	~V92Phase4Modulator();

	/*
	 * The thirty written members.  Return types are not mangled,
	 * so `int` here means "the object leaves a 32-bit value in %eax and
	 * the last thing done to it is a sign extension from 16 bits" and
	 * `void` means "nothing is left in %eax".  The one argument type is
	 * the mangling's.
	 */
	int generateCPt();
	int generateCPu();
	int generateSUVu();
	int generateE1u();
	int generateE2u();
	int generateB1u();
	int generateRm();
	int generateRu();
	int generateRuNot();
	int generateTRN2u();
	int generateDataSymbolBeforeFPE();
	int generateDataSymbolBeforeRRN();

	void enterRepeatedCP();

	void recivedCP();
	void recivedCPtag();
	void recivedEd();
	void recivedFirstRrnEd();
	void recivedRt();
	void recivedSUV();
	void recivedSUVtag();
	void recivedPartOneSilenceRrnSUV();
	void recivedPartOneSilenceRrnSUVtag();
	void recivedPartTwoSilenceRrnSUV();
	void recivedPartTwoSilenceRrnSUVtag();

	void exitCPt();
	void exitTRN2u();
	void resetBeforFPE();
	void resetBeforRRN();
	void resetRRNSecondSection();

	void setMappingParams(V92MappingParams *mappingParams);

	/* Public for `offsetof`, which wants standard layout; and one access
	 * section, for the same reason V92Precoder.h gives. */

	/*
	 * +0x00  Which phase 4 upstream signal is being generated.  SIGNED --
	 * see the block comment; four of its values have names.
	 */
	int state;

	/*
	 * +0x04  Symbols emitted since the current signal segment started.
	 * Unsigned: every use is an unsigned `divl` or an unsigned compare
	 * (`cmp $0x18,%eax; ja` at .text+0x17ffb).  It is what the four
	 * "enter X @ %d" messages print, and every transition clears it.
	 */
	unsigned int symbolCount;

	/*
	 * +0x08  Where in `pattern` the next bit comes from, advanced as
	 * `(patternIndex + 1) % patternLength` after each byte.  `generateCPu`
	 * and `generateSUVu` are its only users.
	 */
	unsigned int patternIndex;

	/* +0x0c .. +0x17  Not touched by anything written here. */
	unsigned char pad_0c[0x0c];

	/* +0x18  Cleared by the constructor.  A four-byte store; its role is
	 * not established. */
	unsigned int word_18;

	/* +0x1c  Cleared by the constructor, `movb $0x0` -- one byte, and the
	 * last thing the constructor does. */
	unsigned char byte_1c;

	/* +0x1d .. +0x1f  Not touched. */
	unsigned char pad_1d[3];

	/*
	 * +0x20  Set to 1 by every member that takes a state transition on a
	 * received tag (`recivedEd`, `recivedFirstRrnEd`, `recivedSUVtag`) and
	 * tested at the top of each of them to make the transition happen at
	 * most once; cleared by `resetBeforRRN` and `resetRRNSecondSection`.
	 * `recivedSUVtag` also requires it clear before it will act.
	 */
	unsigned int flag_20;

	/* +0x24 .. +0x27  Not touched. */
	unsigned char pad_24[4];

	/* +0x28  Set to 1 by `resetBeforRRN`; nothing written reads it. */
	unsigned int word_28;

	/* +0x2c  Cleared by `resetBeforRRN`. */
	unsigned int word_2c;

	/* +0x30  Cleared by `resetBeforRRN`. */
	unsigned int word_30;

	/* +0x34  Cleared by `resetBeforRRN`, set to 1 by
	 * `resetRRNSecondSection`. */
	unsigned int word_34;

	/* +0x38  Cleared by `resetBeforRRN`. */
	unsigned int word_38;

	/*
	 * +0x3c  Selects where `generateCPu`, `generateSUVu` and `generateE2u`
	 * take their bits from: zero means the class's own `pattern` or a
	 * constant, straight into `mapper`; non-zero means
	 * `V92BitsToSymbol::nofBitsForNextTime` and the two `process`
	 * overloads.  `resetBeforFPE` sets it to 1 and nothing written clears
	 * it.  Left unnamed: what the two arms MEAN is not established by
	 * anything written, only which is taken.
	 */
	unsigned int flag_3c;

	/*
	 * +0x40  The magnitude of the two-level symbol `generateCPt`,
	 * `generateE1u`, `generateRu` and `generateRuNot` return: each of the
	 * four returns `+this->amplitude` or `-this->amplitude` and nothing
	 * else.  SIGNED, and that is forced -- `movswl 0x40(%ecx),%ebx` at
	 * .text+0x17041 sign-extends it into the returned 32-bit value.
	 * Usage inference across those four sites, which is CLAUDE.md's
	 * weakest tier; no format string prints it.
	 */
	short amplitude;

	/* +0x42  Alignment; nothing writes it. */
	unsigned char pad_42[1];

	/*
	 * +0x43  How many bits go into one symbol when the bits come from
	 * `pattern` rather than from `bitsToSymbol`: it is the loop bound in
	 * `generateCPu`/`generateSUVu`, the count handed to
	 * `Scrambler<h,h>::processAllOnes`/`processAllZeros` in
	 * `generateTRN2u`/`generateE2u`, and the index of the last bit
	 * (`bits[bitsPerSymbol - 1]`) that carries the differential state.
	 */
	unsigned char bitsPerSymbol;

	/* +0x44 .. +0x47  Not touched. */
	unsigned char pad_44[4];

	/* +0x48  The constructor's FOURTH argument, stored and not owned. */
	V92MappingParams *mappingParams;

	/* +0x4c  The upstream scrambler, built (5, 23, 99).  A member: the
	 * destructor calls its D1. */
	Scrambler<unsigned char, unsigned char> scrambler;

	/* +0x6c  The constructor's SECOND argument, stored and not owned. */
	V92BitsToSymbol *bitsToSymbol;

	/*
	 * +0x70  `sysdep_malloc(0x2c)` and constructed.  The one thing this
	 * class owns, and the one pointer its destructor releases.
	 */
	V92Mapper *mapper;

	/*
	 * +0x74  The constructor's THIRD argument.  Not owned -- and the
	 * constructor writes a zero into its +0x110 on the way past.
	 */
	V92CP *cp;

	/*
	 * +0x78  The differentially encoded bit carried from one symbol to the
	 * next.  Six generators exclusive-OR it into the bit they are about to
	 * emit and store the result back here; `generateCPt` toggles it with
	 * `^ 1` while the symbol count is still below 25.
	 */
	unsigned int prevBit;

	/*
	 * +0x7c  The bit block for one symbol.  THE LENGTH IS THE DISTANCE TO
	 * `pattern`, NOT A MEASURED BOUND -- see the block comment.
	 */
	unsigned char bits[0x12c];

	/*
	 * +0x1a8  A repeating bit pattern, one bit per byte, read at
	 * `pattern[patternIndex]` by `generateCPu`/`generateSUVu` and at
	 * `pattern[(symbolCount - 25) % patternLength]` by `generateCPt`, and
	 * in every case fed straight through the scrambler.  `unsigned char *`
	 * is forced: `movzbl (%eax,%ecx,1),%edx` at .text+0x17d86.  Nothing
	 * written assigns it.
	 */
	unsigned char *pattern;

	/* +0x1ac  The modulus `patternIndex` is kept below, and the same
	 * modulus `generateCPt` reduces its own index by. */
	unsigned int patternLength;

	/*
	 * +0x1b0  The period, in symbols, that `symbolCount` is reduced modulo
	 * to decide whether a segment may end: `symbolCount % word_1b0 == 0`
	 * in `recivedEd`, `recivedFirstRrnEd` and `recivedSUVtag`, and
	 * `(symbolCount - 24) % word_1b0 == 0` in `exitCPt`.  Left unnamed:
	 * four sites agree on the shape and none says what the period is OF.
	 */
	unsigned int word_1b0;

	/* +0x1b4 .. +0x1b7  Not touched. */
	unsigned char pad_1b4[4];

	/*
	 * +0x1b8  Written only on entry to E2u, and only ever with 12 or 13 --
	 * two of the three `state` values `recivedSUVtag` will act on.  So it
	 * holds a state code, and holding one is all that is established;
	 * nothing written reads it back.  `+0x910` in V92CP is the same
	 * restraint for the same reason.
	 */
	unsigned int word_1b8;

	/*
	 * +0x1bc  Non-zero means E2u has been extended.  Named from a format
	 * string that fires on exactly this test -- `recivedFirstRrnEd` prints
	 * "V92Phase4Modulator: ERROR: E2u is extended in RRN !!!" when it is
	 * set (.text+0x17720) -- which is CLAUDE.md's strongest evidence tier.
	 * `recivedEd` and `recivedSUVtag` use it to pick word_1b8: 13 when it
	 * is set and 12 when it is not.
	 */
	unsigned int e2uExtended;

	/* +0x1c0  Cleared by the constructor. */
	unsigned int word_1c0;

	/* +0x1c4  Cleared by the constructor. */
	unsigned int word_1c4;

	/*
	 * +0x1c8  The constructor's FIRST argument, stored and not owned.
	 * The last four bytes of the object.
	 */
	V92Parameters *params;
};

#endif /* DSPLIB_V92PHASE4MODULATOR_H */
