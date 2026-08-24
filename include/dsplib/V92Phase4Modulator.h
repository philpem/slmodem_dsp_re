/*
 * V92Phase4Modulator.h -- the V.92 phase 4 upstream symbol source, COMPLETE.
 *
 * Reconstructed from dsplibs.o.  The class has 34 distinct members and ALL
 * THIRTY-FOUR are written in src/pump/v90/V92Phase4Modulator.cpp --
 * the constructor (C1 at .text+0x17970 and C2 at +0x17a20, 164 bytes each),
 * the destructor (D2 at +0x16de0 and D1 at +0x16e40, 87 bytes each), the
 * thirty members of the "generate / recived / exit / resetBefor" surface, and
 * `generateSymbol` (.text+0x18050, 4,055 B), which is the state machine the
 * other generators are the arms of.
 *
 * ALL 34 ARE NOW WRITTEN.  The last was `reset`
 * (`_ZN18V92Phase4Modulator5resetEsh23V92Phase4ModulatorStatejj`, 290 B at
 * .text+0x19030), which became READY the moment `generateSymbol` landed -- it
 * calls it in a loop.  The enum its mangling names is defined below and
 * carries no state codes; the note there says why.
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
 * Nine more values were bare when that paragraph was written and six of them
 * are named now, by `generateSymbol`'s own messages -- see the block below
 * the four `#define`s.  Every value still bare is bare on purpose: nothing
 * written names it, and a guessed enumerator is exactly the wrong name that
 * no test can fail on.
 *
 * THE FIELD IS A SIGNED `int` AND THAT IS FORCED, not chosen.  `recivedEd` and
 * `recivedSUVtag` lower their switch as `cmp $0x5; je; jl <default>; sub $0xc;
 * cmp $0x1; ja <default>` -- the `jl` at .text+0x17657 and +0x17331 is a
 * SIGNED branch, which GCC cannot emit for an unsigned switch value.  The
 * enum `V92Phase4ModulatorState` that `reset`'s mangling names is therefore
 * NOT this field's type as far as anything measured goes, and `reset` -- now
 * written -- does not settle it either: it stores the argument whole with one
 * 32-bit `mov` that neither signedness would distinguish.  So the field stays
 * `int` and the enum carries no state codes; see its definition below.
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
 * THE TYPE THE MANGLING NAMES.  `reset`'s third argument is
 * `23V92Phase4ModulatorState` and the object stores it whole into `state` with
 * one 32-bit `mov %eax,(%esi)` at .text+0x19068, so the enum's value set IS
 * `state`'s value set and this is its one home.
 *
 * IT IS DELIBERATELY EMPTY OF ENUMERATORS.  `state` itself stays an `int`,
 * because `recivedEd`'s and `recivedSUVtag`'s `jl` are SIGNED branches and
 * GCC cannot emit those for an unsigned switch value -- and an enum whose
 * enumerators are all non-negative may have an unsigned underlying type.
 * Naming the codes here as well as in the thirteen `#define`s below would be
 * two spellings of one alphabet, which is what "one type, one home" exists to
 * stop; naming them here INSTEAD would change `state`'s comparisons from
 * `int` to enum ones.  So the type carries the mangled name and nothing else,
 * and the codes stay where they were derived.
 *
 * The one enumerator is what C++98 requires: an enum with no enumerators is
 * ill-formed.  It is not a state code and no arm dispatches on it -- 0 is one
 * of the fifteen values NOTHING NAMES, and `V92Modulator::enterPhase4` is the
 * only caller in the object, passing a literal zero (.text+0x148d3).
 */
enum V92Phase4ModulatorState {
	V92P4M_RESET_STATE_ZERO = 0
};

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

/*
 * THE BIT BLOCK, AND THE FOUR BYTES BELOW IT.  `bitsExt` holds `prevBit` and
 * the block in ONE array object, because the object addresses one byte below
 * the block; see the member itself for the instructions that say so.
 *
 *   bitsExt[V92P4M_BITS_BELOW + i]   IS the object's `bits[i]`, +0x7c + i
 *   bitsExt[V92P4M_BITS_BELOW - 1]   IS the object's `bits[-1]`, +0x7b
 *
 * `V92P4M_BITS_BELOW` is four and not one because the union has to cover the
 * whole of `prevBit`; only its top byte is ever addressed as part of the
 * block.  `V92P4M_BITS_LEN` is the distance from `bits[0]` to `pattern` and
 * NOT a measured bound -- see the block comment above.
 */
#define V92P4M_BITS_BELOW	4
#define V92P4M_BITS_LEN		0x12c

/*
 * Six more, and they are `generateSymbol`'s -- that one function carries a
 * thirty-arm switch and thirteen `.rodata.str1.4` messages, and six of the
 * messages name the signal being entered with the state store on the very
 * next instructions.  Same evidence tier as the seven above:
 *
 *   3   "RRN: enter TRN2uModulation @ %d"     :0x402c, then `movl $0x3`
 *   10  "enter FinalSUVu @ %d"                :0x3ec4, then `movl $0xa`
 *   16  "enter B1u @ %d"                      :0x3e9c, then `movl $0x10`
 *   17  "enter FB1u @ %d"                     :0x3f4c, then `movl $0x11`
 *   23  "enter TRN2u Second at RRN @ %d"      :0x4108, then `movl $0x17`
 *   28  "Phase4 Terminated @ %d"              :0x3f1c, then `movl $0x1c`
 *
 * STILL BARE, and deliberately: 0, 1, 4, 6, 8, 9, 11, 18, 19, 20, 24, 25,
 * 26, 27 and 29.  Four of those are all but named by a NEIGHBOUR and that is
 * not the same thing -- `exitTRN2u` takes 3 to 4, and 4's own arm prints "on
 * TRN2uModulationExit enter SUV", which names the TRANSITION OUT of 4 rather
 * than 4; 23 and 24 stand in the same relation.  29 is set on three
 * different null-pointer errors and no message names it either.
 *
 * 7, 14, 21 and 22 are the switch's HOLES.  Their jump-table slots hold the
 * default label, which is what GCC fills a dense table's gaps with, so
 * whether the source listed them at all is not recoverable and they are not
 * named.
 */
#define V92P4M_STATE_TRN2U_MOD		3
#define V92P4M_STATE_FINAL_SUVU		10
#define V92P4M_STATE_B1U		16
#define V92P4M_STATE_FB1U		17
#define V92P4M_STATE_TRN2U_SECOND	23
#define V92P4M_STATE_TERMINATED		28

/*
 * TWO MORE, AND THEY COME FROM OUTSIDE THIS CLASS.  `V92Modulator::initiateRRN`
 * and `::initiateFPE` choose the state they hand `reset` and print which one
 * they chose on the instruction that loads it:
 *
 *   18  "V92Modulator: Phase4Modulator state initialized to
 *        DataToRuModulation"                  :0x363c, then `mov $0x12,%esi`
 *   25  "V92Modulator: Phase4Modulator state initialized to
 *        DataToRmModulation"                  :0x3728, then `mov $0x19,%esi`
 *
 * The same two members name 19 and 26 the same way -- "RuModulation" and
 * "RmModulation" -- which is the SECOND string to land on each of those, and
 * both agree with the "enter Ru @ %d" and "enter Rm @ %d" the arms themselves
 * print.  So 18 and 25 are the states the modulator enters when it still has
 * DATA to finish before the renegotiation signal starts, and 19 and 26 are the
 * signals proper.  Same evidence tier as the thirteen above; the messages
 * belong to V92Modulator.cpp and the codes belong here.
 *
 * That leaves 0, 1, 4, 6, 8, 9, 11, 20, 24, 27 and 29 bare, and 7, 14, 21, 22
 * as the switch's holes.
 */
#define V92P4M_STATE_DATA_TO_RU		18
#define V92P4M_STATE_DATA_TO_RM		25

class V92Phase4Modulator {
public:
	V92Phase4Modulator(V92Parameters *params, V92BitsToSymbol *bitsToSymbol,
			   V92CP *cp, V92MappingParams *mappingParams);
	~V92Phase4Modulator();

	/*
	 * .text+0x19030, 290 bytes.  All five argument types are the
	 * mangling's; the return type is measured `void`.
	 *
	 *   amplitudeArg  -> `amplitude` (+0x40) and on to `V92Mapper::reset`
	 *   bitsArg       -> `byte_42` raw, and `bitsPerSymbol = bitsArg + 2`
	 *   stateArg      -> `state`
	 *   nSymbols      -> how many times `generateSymbol` is called before
	 *                    returning; zero means not at all
	 *   suvLimit      -> `word_44`, the SUV threshold state 5 compares
	 *                    `word_18` against, offset by 800
	 */
	void reset(short amplitudeArg, unsigned char bitsArg,
		   V92Phase4ModulatorState stateArg, unsigned int nSymbols,
		   unsigned int suvLimit);

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

	/*
	 * The phase 4 upstream state machine itself: one symbol per call,
	 * dispatched on `state` through a thirty-entry jump table, with the
	 * segment boundaries and their transitions in the same arms.
	 */
	int generateSymbol();

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

	/*
	 * +0x0c  THE ONE THING THIS CLASS REPORTS BACK PER SYMBOL, and its
	 * shape is established while its meaning is not.
	 *
	 * `generateSymbol` clears it before the switch, on every call and
	 * whatever the state; exactly one arm then writes it, the value 9,
	 * beside the message "Phase4 Terminated @ %d".  `reset` clears it too.
	 * `V92Modulator::progress` is the reader and it latches rather than
	 * consumes -- `sym = p4->generateSymbol(); ...; if (p4->word_0c)
	 * this->word_34 = p4->word_0c;` at .text+0x14e2b and +0x14f12 -- and
	 * then tests its own copy against 9.  `V92Phase3Modulator` is read
	 * the same way at its own +0x14.
	 *
	 * So: a code, zero meaning "nothing happened this symbol", set once
	 * and read by the layer above.  What the code 9 MEANS to that layer
	 * is `V92Modulator::progress`'s business and that function is
	 * unwritten, so the field keeps an offset name.
	 */
	unsigned int word_0c;

	/* +0x10 .. +0x17  Not touched by anything written here. */
	unsigned char pad_10[8];

	/*
	 * +0x18  A COUNTER, and `byte_1c` is its enable.  `generateSymbol`'s
	 * state 5 arm increments it once per symbol and only while `byte_1c`
	 * is non-zero, and enters the repeated CP once it passes
	 * `word_44 + 800`.  Cleared by the constructor, by `reset`, by
	 * `enterRepeatedCP`, by `recivedSUVtag`, by `recivedCPtag`, by
	 * `resetRRNSecondSection` and on both of `generateSymbol`'s two
	 * remaining paths that touch it.  What it counts is symbols in SUV,
	 * but only because state 5 is where it is counted -- nothing prints
	 * it and nothing else reads it.
	 */
	unsigned int word_18;

	/*
	 * +0x1c  Whether `word_18` is counting.  Set to 1 at exactly one site
	 * -- `generateSymbol`'s state 12 arm, beside "CPu Terminated @ %d" --
	 * and cleared by the constructor, `reset`, `enterRepeatedCP`,
	 * `recivedSUVtag`, `recivedCPtag` and `resetRRNSecondSection`, always
	 * alongside `word_18`.  One byte, stored as a byte.
	 */
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

	/*
	 * +0x24  A LENGTH IN SYMBOLS, written at one site and read at one
	 * site, both inside `generateSymbol`.
	 *
	 * On entry to state 23 ("enter TRN2u Second at RRN") it is set to
	 * 4000 or 8004 according to `word_38` -- the object's branchless
	 * `cmp $0x1; sbb; and $0xfa4; add $0xfa0` -- and state 23's own arm
	 * will not terminate the segment until `symbolCount` has reached it.
	 * Nothing else in the class touches it, `reset` included.
	 *
	 * Not named beyond that: it is a bound on one segment's length and
	 * the two constants are not established as anything but themselves.
	 */
	unsigned int word_24;

	/*
	 * +0x28, +0x30, +0x34  ONE THREE-WAY CONDITION, and that is all that
	 * is established about them.  `generateSymbol`'s E2u arm takes the
	 * "TRN2u Second at RRN" transition when `word_28 != 0 && word_30 != 0
	 * && word_34 == 0` and otherwise falls through to B1u; the three are
	 * loaded and tested in that order at .text+0x18830.  `resetBeforRRN`
	 * sets +0x28 to 1 and clears the other two, `resetRRNSecondSection`
	 * and `generateSymbol`'s own state 23 and 24 arms set +0x34 to 1, and
	 * `reset` clears all three.  Nothing prints any of them.
	 */
	unsigned int word_28;

	/*
	 * +0x2c  THE SUV VALUE, and the name is the CALLEE'S: two of
	 * `generateSymbol`'s arms pass it straight to `V92CP::setSUV(unsigned
	 * int)` -- `mov 0x2c(%esi),%edx` then the call, at .text+0x18193 and
	 * +0x18f39 -- which stores it at `V92CP::suv`.  Cleared by
	 * `resetBeforRRN` and by `reset`, and nothing written assigns it
	 * anything else, so what it ever holds besides zero is not
	 * established and the offset name stays.
	 */
	unsigned int word_2c;

	/* +0x30  See `word_28`. */
	unsigned int word_30;

	/* +0x34  See `word_28`. */
	unsigned int word_34;

	/*
	 * +0x38  Cleared by `resetBeforRRN` and by `reset`, and read at two
	 * sites: `recivedRt` will not act while it is clear, and
	 * `generateSymbol` picks `word_24` as 4000 when it is set and 8004
	 * when it is not.  Nothing written sets it.
	 */
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

	/*
	 * +0x42  `reset`'s SECOND argument, raw.  NOT alignment, which is what
	 * this comment used to say: `mov %dl,0x42(%esi)` at .text+0x19062 is a
	 * byte store of that argument, separate from the 16-bit
	 * `mov %bx,0x40(%esi)` above it and from the `+0x43` store below.  It
	 * was true of every member reconstructed so far and false of the
	 * object -- `reset` is one of the eight still outstanding, so nothing
	 * had read it.  The offset name STAYS: what the field is FOR is
	 * `reset`'s business and nothing written here touches it.  Finding
	 * 5401.
	 */
	unsigned char byte_42;

	/*
	 * +0x43  How many bits go into one symbol when the bits come from
	 * `pattern` rather than from `bitsToSymbol`: it is the loop bound in
	 * `generateCPu`/`generateSUVu`, the count handed to
	 * `Scrambler<h,h>::processAllOnes`/`processAllZeros` in
	 * `generateTRN2u`/`generateE2u`, and the index of the last bit
	 * (`bits[bitsPerSymbol - 1]`) that carries the differential state.
	 *
	 * `reset` COMPUTES IT AS `arg + 2` IN ONE BYTE -- `add $0x2,%dl;
	 * mov %dl,0x43(%esi)` at .text+0x19065 -- so a second argument of 254
	 * gives ZERO, the constructor does not initialise the field at all,
	 * and `bits[bitsPerSymbol - 1]` at zero is the fold D561 records.
	 * Finding 5401.
	 */
	unsigned char bitsPerSymbol;

	/*
	 * +0x44  `reset`'s FIFTH argument, stored and read once:
	 * `mov 0x34(%esp),%eax; mov %eax,0x44(%esi)` at .text+0x1903f, and
	 * `generateSymbol`'s state 5 arm gives up on SUV and enters the
	 * repeated CP once `word_18` has passed `word_44 + 800`.  So it is a
	 * threshold that the caller of `reset` sets, offset by 800; what it
	 * counts is `word_18`'s business and nothing written establishes
	 * that.
	 */
	unsigned int word_44;

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
	 * +0x78 .. +0x1a7  THE DIFFERENTIAL BIT AND THE BIT BLOCK, IN ONE
	 * ARRAY OBJECT, BECAUSE THE OBJECT ADDRESSES ONE BYTE BELOW THE BLOCK.
	 *
	 * +0x78 `prevBit` is the differentially encoded bit carried from one
	 * symbol to the next.  Six generators exclusive-OR it into the bit
	 * they are about to emit and store the result back here; `generateCPt`
	 * toggles it with `^ 1` while the symbol count is still below 25, and
	 * `reset` clears it (`movl $0x0,0x78(%esi)` at .text+0x1909c).
	 *
	 * +0x7c `bitsExt[V92P4M_BITS_BELOW]` onwards is the bit block handed
	 * to `V92Mapper::process` for one symbol.  ITS LENGTH IS THE DISTANCE
	 * TO `pattern`, NOT A MEASURED BOUND -- see the block comment.
	 *
	 * THE UNION IS THE OBJECT'S OWN ALIASING AND NOT A CONVENIENCE.  The
	 * four differential generators fold into `bits[bitsPerSymbol - 1]`,
	 * and the blob forms that address as `0x7b(%count,%this,1)` with the
	 * count -- `bitsPerSymbol`, zero-extended from +0x43 -- in the index
	 * register.  Four load/store pairs say it:
	 *
	 *   generateTRN2u  movzbl 0x7b(%edx,%ebx,1),%eax   .text+0x17c3a
	 *                  mov    %al,0x7b(%edx,%ebx,1)           +0x17c44
	 *   generateE2u                                           +0x17c91
	 *                                                         +0x17c9b
	 *   generateCPu                                           +0x17dc0
	 *                                                         +0x17dc7
	 *   generateSUVu                                          +0x17ef0
	 *                                                         +0x17ef7
	 *
	 * At `bitsPerSymbol == 0` that address is `this + 0x7b`, the TOP BYTE
	 * of `prevBit` -- so the load and the store overlap the very field the
	 * same statement assigns.  Declared as two separate members, `bits[-1]`
	 * is out of bounds, the two stores may be emitted in either order, and
	 * they were: GCC 13 kept our order and GCC 3.4.2 did not, which cost
	 * 240 checks on unmutated source (finding 4705).  One array object
	 * spanning both makes the access defined C, states the aliasing where
	 * a reader will find it, and returns the store order to the source.
	 * D561, and it is D392's fix in the same shape -- an out-of-range
	 * window becoming a value inside our own array.
	 */
	union {
		unsigned int prevBit;
		unsigned char bitsExt[V92P4M_BITS_BELOW + V92P4M_BITS_LEN];
	};

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
