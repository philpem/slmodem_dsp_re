/**
 * @file V92Phase4Modulator.h
 * @brief The V.92 phase 4 upstream symbol source: the state machine that
 *        generates one modulated symbol per call for every phase-4 signal
 *        (CP, SUV, E1u/E2u, TRN2u, B1u/FB1u, Ru/Rm and their data-phase
 *        lead-ins), plus the `recived*`/`exit*` transition handlers the
 *        layer above drives it with. Complete: all 34 members are written
 *        in src/pump/v90/V92Phase4Modulator.cpp.
 *
 * The constructor is at .text+0x17970/+0x17a20 (164 bytes each), the
 * destructor at +0x16de0/+0x16e40 (87 bytes each), and `generateSymbol`
 * (.text+0x18050, 4,055 B) is the state machine the other generators are the
 * arms of. `reset` (`_ZN18V92Phase4Modulator5resetEsh23V92Phase4ModulatorStatejj`,
 * 290 B at .text+0x19030) was the last of the 34 to be written -- it became
 * reachable the moment `generateSymbol` landed, since it calls it in a loop.
 *
 * The object's spelling of "received" is "recived" throughout, and it is the
 * mangling's -- `_ZN18V92Phase4Modulator13recivedSUVtagEv`. Reproduced.
 *
 * The object is 0x1cc bytes, and that is the allocation, not a bound.
 * `V92Modulator::V92Modulator` builds it:
 *
 *     152b9:  c7 04 24 cc 01 00 00   movl $0x1cc,(%esp)
 *     152c0:  e8 ..                  call sysdep_malloc
 *     152e6:  e8 ..                  call V92Phase4Modulator::V92Phase4Modulator
 *
 * which is the original compiler's own `sizeof` (finding F1249's oracle). The
 * furthest field the constructor writes is the four bytes at +0x1c8, and
 * 0x1c8 + 4 == 0x1cc exactly.
 *
 * ### The scrambler at +0x4c, with taps (5, 23)
 *
 * `Scrambler<unsigned char, unsigned char>::Scrambler(this + 0x4c, 5, 0x17,
 * 0x63)` opens the constructor and `Scrambler<unsigned char, unsigned char>::
 * ~Scrambler(this + 0x4c)` closes the destructor unconditionally -- the D1
 * variant, which is what GCC emits for a member rather than for a
 * non-virtual base. Its own header pins it at 0x20 bytes, so it runs
 * +0x4c..+0x6b and the next named field at +0x6c meets it exactly.
 *
 * These are V.92's upstream taps and they are the same (5, 23, 99) that
 * `V92Phase3Modulator` builds its own `Scrambler<unsigned char, int>` with
 * (finding F1255). The intermediate type differs -- `<h,h>` here against
 * `<h,i>` there -- and that is the mangling's, not a choice.
 *
 * ### The constructor writes one field of somebody else's object
 *
 *     179ed:  89 7b 74           mov %edi,0x74(%ebx)     this->cp = cp
 *     179f0:  89 b7 10 01 00 00  mov %esi,0x110(%edi)    cp->word_110 = 0
 *
 * -- a store straight through the third argument into the `V92CP` it has
 * just been handed, with %esi zeroed two instructions earlier. Four more
 * members write it too (`recivedCP`, `resetBeforRRN`, `resetRRNSecondSection`
 * and, a byte lower, `recivedPartOneSilenceRrnSUV`), so the constructor is
 * no longer its only writer; V92CP.h's note that it is written from outside
 * still holds.
 *
 * ### The state codes are the author's own, and thirteen of them have names
 *
 * `+0x00` is a small signed integer the twenty-four members switch on and
 * assign. Thirteen of its values are pinned by a format string that fires on
 * the assignment itself, which is the strongest evidence there is
 * (CLAUDE.md's evidence order, item 1) -- the message names the signal being
 * entered and the very next instruction stores the code. See the `#define`s
 * below for each value and its citation.
 *
 * Every value still bare is bare on purpose: nothing written names it, and a
 * guessed enumerator is exactly the wrong name that no test can fail on.
 * Four of those are all but named by a neighbour and that is not the same
 * thing -- `exitTRN2u` takes 3 to 4, and 4's own arm prints "on
 * TRN2uModulationExit enter SUV", which names the transition OUT of 4 rather
 * than 4; 23 and 24 stand in the same relation. 29 is set on three
 * different null-pointer errors and no message names it either. 7, 14, 21
 * and 22 are the switch's holes: their jump-table slots hold the default
 * label, which is what GCC fills a dense table's gaps with, so whether the
 * source listed them at all is not recoverable and they are not named.
 *
 * The field is a signed `int`, and that is forced, not chosen: `recivedEd`
 * and `recivedSUVtag` lower their switch as `cmp $0x5; je; jl <default>;
 * sub $0xc; cmp $0x1; ja <default>` -- the `jl` at .text+0x17657 and
 * +0x17331 is a signed branch, which GCC cannot emit for an unsigned switch
 * value. The enum `V92Phase4ModulatorState` that `reset`'s mangling names is
 * therefore NOT this field's type as far as anything measured goes, and
 * `reset` does not settle it either: it stores the argument whole with one
 * 32-bit `mov` that neither signedness would distinguish. So the field stays
 * `int` and the enum carries no state codes; see its definition below.
 * (Finding F4700.)
 *
 * ### What +0x7c is, and what its length is not
 *
 * `+0x7c` is the bit block handed to `V92Mapper::process` for one symbol:
 * `Scrambler<h,h>::process` fills it a byte at a time, `processAllOnes` and
 * `processAllZeros` fill it in bulk, and `V92BitsToSymbol::process(unsigned
 * char *, unsigned)` consumes it. Its declared length here is the distance
 * to the next field that is known (`+0x1a8`), not a measured bound. Nothing
 * in this class establishes how much of the 300 bytes is ever used; the
 * number of bits actually written is `+0x43` or
 * `V92BitsToSymbol::nofBitsForNextTime`.
 *
 * Data member names are invented and descriptive (finding F226).
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
 * base. `mov $0x5`, `mov $0x17`, `mov $0x63` at .text+0x17973..+0x1798e. */
#define V92P4M_SCRAM_TAP1	5
#define V92P4M_SCRAM_TAP2	23
#define V92P4M_SCRAM_SLACK	99

/*
 * The type the mangling names. `reset`'s third argument is
 * `23V92Phase4ModulatorState` and the object stores it whole into `state`
 * with one 32-bit `mov %eax,(%esi)` at .text+0x19068, so the enum's value
 * set IS `state`'s value set and this is its one home.
 *
 * It is deliberately empty of enumerators. `state` itself stays an `int`,
 * because `recivedEd`'s and `recivedSUVtag`'s `jl` are signed branches and
 * GCC cannot emit those for an unsigned switch value -- and an enum whose
 * enumerators are all non-negative may have an unsigned underlying type.
 * Naming the codes here as well as in the `#define`s below would be two
 * spellings of one alphabet, which is what "one type, one home" exists to
 * stop; naming them here INSTEAD would change `state`'s comparisons from
 * `int` to enum ones. So the type carries the mangled name and nothing
 * else, and the codes stay where they were derived.
 *
 * The one enumerator is what C++98 requires: an enum with no enumerators is
 * ill-formed. It is not a state code and no arm dispatches on it -- 0 is one
 * of the values nothing names, and `V92Modulator::enterPhase4` is the only
 * caller in the object, passing a literal zero (.text+0x148d3).
 */
enum V92Phase4ModulatorState {
	V92P4M_RESET_STATE_ZERO = 0
};

/*
 * The four `state` codes a format string names directly (finding F4700):
 *
 *     "V92Phase4Modulator: enter E1u @ %d"   ->  2    exitCPt
 *     "V92Phase4Modulator: enter E2u @ %d"   ->  15   recivedEd, recivedSUVtag
 *     "V92Phase4Modulator: enter Ru @ %d"    ->  19   generateDataSymbolBeforeRRN
 *     "V92Phase4Modulator: enter Rm @ %d"    ->  26   generateDataSymbolBeforeFPE
 */
#define V92P4M_STATE_E1U	2
#define V92P4M_STATE_E2U	15
#define V92P4M_STATE_RU		19
#define V92P4M_STATE_RM		26

/*
 * Three more, named by the six members added with `V92CP::infoToBits`
 * (finding F4755). Each is the state stored immediately after a message
 * that names it, the author's own `.rodata.str1.4` text:
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
 * The bit block, and the four bytes below it. `bitsExt` holds `prevBit` and
 * the block in ONE array object, because the object addresses one byte below
 * the block; see the member itself for the instructions that say so.
 *
 *   bitsExt[V92P4M_BITS_BELOW + i]   IS the object's `bits[i]`, +0x7c + i
 *   bitsExt[V92P4M_BITS_BELOW - 1]   IS the object's `bits[-1]`, +0x7b
 *
 * `V92P4M_BITS_BELOW` is four and not one because the union has to cover the
 * whole of `prevBit`; only its top byte is ever addressed as part of the
 * block. `V92P4M_BITS_LEN` is the distance from `bits[0]` to `pattern` and
 * NOT a measured bound -- see the block comment above.
 */
#define V92P4M_BITS_BELOW	4
#define V92P4M_BITS_LEN		0x12c

/*
 * Six more, and they are `generateSymbol`'s (finding F4820) -- that one
 * function carries a thirty-arm switch and thirteen `.rodata.str1.4`
 * messages, and six of the messages name the signal being entered with the
 * state store on the very next instructions:
 *
 *   3   "RRN: enter TRN2uModulation @ %d"     :0x402c, then `movl $0x3`
 *   10  "enter FinalSUVu @ %d"                :0x3ec4, then `movl $0xa`
 *   16  "enter B1u @ %d"                      :0x3e9c, then `movl $0x10`
 *   17  "enter FB1u @ %d"                     :0x3f4c, then `movl $0x11`
 *   23  "enter TRN2u Second at RRN @ %d"      :0x4108, then `movl $0x17`
 *   28  "Phase4 Terminated @ %d"              :0x3f1c, then `movl $0x1c`
 *
 * Still bare, and deliberately: 0, 1, 4, 6, 8, 9, 11, 18, 19, 20, 24, 25, 26,
 * 27 and 29 (18/19/25/26 are named below, from a different source). 7, 14,
 * 21 and 22 are the switch's holes.
 */
#define V92P4M_STATE_TRN2U_MOD		3
#define V92P4M_STATE_FINAL_SUVU		10
#define V92P4M_STATE_B1U		16
#define V92P4M_STATE_FB1U		17
#define V92P4M_STATE_TRN2U_SECOND	23
#define V92P4M_STATE_TERMINATED		28

/*
 * Two more, and they come from outside this class (finding F7547).
 * `V92Modulator::initiateRRN` and `::initiateFPE` choose the state they hand
 * `reset` and print which one they chose on the instruction that loads it:
 *
 *   18  "V92Modulator: Phase4Modulator state initialized to
 *        DataToRuModulation"                  :0x363c, then `mov $0x12,%esi`
 *   25  "V92Modulator: Phase4Modulator state initialized to
 *        DataToRmModulation"                  :0x3728, then `mov $0x19,%esi`
 *
 * The same two members put a SECOND string on 19 and 26 -- "RuModulation"
 * and "RmModulation" -- which agrees with the "enter Ru @ %d" and "enter
 * Rm @ %d" the arms themselves print. So 18 and 25 are the states the
 * modulator enters when it still has DATA to finish before the
 * renegotiation signal starts, and 19 and 26 are the signals proper. The
 * messages belong to V92Modulator.cpp and the codes belong here.
 *
 * That leaves 0, 1, 4, 6, 8, 9, 11, 20, 24, 27 and 29 bare, and 7, 14, 21, 22
 * as the switch's holes.
 */
#define V92P4M_STATE_DATA_TO_RU		18
#define V92P4M_STATE_DATA_TO_RM		25

class V92Phase4Modulator {
public:
	/**
	 * @brief Allocate and construct the mapper sub-object, store the
	 *        four constructor arguments, and zero the CP's `word_110`
	 *        through the caller-owned @p cp.
	 * @param params        Stored, not owned.
	 * @param bitsToSymbol  Stored, not owned; used by the generators
	 *                       whose bits come from the bit-to-symbol stage
	 *                       rather than from `pattern`.
	 * @param cp            Stored, not owned; its `word_110` is zeroed
	 *                       here and by four other members.
	 * @param mappingParams Stored, not owned.
	 */
	V92Phase4Modulator(V92Parameters *params, V92BitsToSymbol *bitsToSymbol,
			   V92CP *cp, V92MappingParams *mappingParams);

	/**
	 * @brief Release the owned mapper. Does not null the pointer
	 *        afterwards, so a second destruction double-frees it
	 *        (reproduced; see docs/deviations.md).
	 */
	~V92Phase4Modulator();

	/**
	 * @brief Reinitialise the state machine for a new phase-4 signal and,
	 *        if @p nSymbols is non-zero, immediately generate that many
	 *        symbols (discarding their output) to run it forward.
	 *
	 *        `bitsPerSymbol` is computed as `bitsArg + 2` in one 8-bit
	 *        add and wraps at 254/255 (D571/D700); the CP's own
	 *        `bitsPerSymbol` is forced to 1 before `infoToBits` is
	 *        called here, so `reset` itself does not trip over it -- see
	 *        finding F5901 for where the fault actually fires.
	 * @param amplitudeArg Stored as `amplitude`, and on to
	 *                      `V92Mapper::reset`.
	 * @param bitsArg      Stored raw as `byte_42`; `bitsPerSymbol` is
	 *                      derived from it as `bitsArg + 2`.
	 * @param stateArg     Stored as `state`.
	 * @param nSymbols     How many times `generateSymbol` is called
	 *                      before returning; zero means not at all.
	 * @param suvLimit     Stored as `this->suvLimit`, the SUV threshold
	 *                      state 5 compares `word_18` against, offset by
	 *                      800.
	 */
	void reset(short amplitudeArg, unsigned char bitsArg,
		   V92Phase4ModulatorState stateArg, unsigned int nSymbols,
		   unsigned int suvLimit);

	/*
	 * The thirty written members below. Return types are not mangled, so
	 * `int` means "the object leaves a 32-bit value in %eax, sign-extended
	 * from the 16-bit symbol" and `void` means "nothing is left in %eax".
	 */

	/**
	 * @brief Generate one CPt symbol: alternates below 25 symbols, then
	 *        differentially encodes the scrambled `pattern` (indexed from
	 *        a base of 25).
	 * @return The modulated symbol, +-`amplitude`.
	 */
	int generateCPt();

	/**
	 * @brief Generate one CPu symbol from `pattern`, or from
	 *        `bitsToSymbol` when `flag_3c` is set. Identical instructions
	 *        to `generateSUVu` (finding F4702); which of CPu/SUVu a call
	 *        is depends only on what `pattern` currently holds.
	 * @return The modulated symbol from `mapper`, or from `bitsToSymbol`.
	 */
	int generateCPu();

	/**
	 * @brief Generate one SUVu symbol. See `generateCPu`, whose body this
	 *        shares byte for byte (finding F4702).
	 * @return The modulated symbol from `mapper`, or from `bitsToSymbol`.
	 */
	int generateSUVu();

	/**
	 * @brief Generate one E1u symbol: one scrambled zero, differentially
	 *        encoded.
	 * @return The modulated symbol, +-`amplitude`.
	 */
	int generateE1u();

	/**
	 * @brief Generate one E2u symbol: `generateCPu`'s shape with the
	 *        pattern loop replaced by one bulk `processAllZeros` call.
	 * @return The modulated symbol from `mapper`, or from `bitsToSymbol`.
	 */
	int generateE2u();

	/**
	 * @brief Generate one B1u symbol via `bitsToSymbol`, unconditionally
	 *        scrambling all-ones bits first. Identical instructions to
	 *        `generateRm` (finding F4702); `generateSymbol`'s five arms
	 *        that inline this body could equally be calling either --
	 *        which one is a naming choice, not a reading (finding F4820).
	 * @return The modulated symbol from `bitsToSymbol`.
	 */
	int generateB1u();

	/**
	 * @brief Generate one Rm symbol. See `generateB1u`, whose body this
	 *        shares byte for byte (finding F4702).
	 * @return The modulated symbol from `bitsToSymbol`.
	 */
	int generateRm();

	/**
	 * @brief Generate one Ru symbol: a fixed six-symbol +/- pattern
	 *        indexed by `(symbolCount - 1) % 6`, needing neither
	 *        `mapper` nor `bitsToSymbol`. The switch has no `default`
	 *        and leaves `sym` uninitialised on a path the modulus proves
	 *        unreachable -- reproduced as the object has it (finding
	 *        F4704).
	 * @return `+amplitude` for residues 0-2, `-amplitude` for 3-5.
	 */
	int generateRu();

	/**
	 * @brief Generate one Ru-complement symbol: `generateRu`'s pattern
	 *        with both signs exchanged.
	 * @return `-amplitude` for residues 0-2, `+amplitude` for 3-5.
	 */
	int generateRuNot();

	/**
	 * @brief Generate one TRN2u symbol: `generateE2u`'s first arm with
	 *        `processAllOnes` in place of `processAllZeros`, and no
	 *        `flag_3c` test at all.
	 * @return The modulated symbol from `mapper`.
	 */
	int generateTRN2u();

	/**
	 * @brief Generate one symbol from `bitsToSymbol` while still finishing
	 *        data before an FPE renegotiation; on running out of bits
	 *        mid-block, transitions to Rm and reaches three levels down to
	 *        flag the modulus encoder (`bitsToSymbol->transmitter->
	 *        modulusEncoder->field_50 = 1`).
	 * @return The modulated symbol from `bitsToSymbol`.
	 */
	int generateDataSymbolBeforeFPE();

	/**
	 * @brief The RRN counterpart of `generateDataSymbolBeforeFPE`: the
	 *        same shape, transitioning to Ru instead of Rm and without
	 *        the modulus-encoder reach.
	 * @return The modulated symbol from `bitsToSymbol`.
	 */
	int generateDataSymbolBeforeRRN();

	/**
	 * @brief The phase 4 upstream state machine itself: advance
	 *        `symbolCount`, clear `eventCode`, dispatch on `state` through
	 *        a thirty-entry jump table (four slots -- 7, 14, 21, 22 --
	 *        are the table's holes and are not real states), and
	 *        generate that segment's symbol, taking any transition the
	 *        segment boundary calls for in the same arm. Called once per
	 *        symbol by `V92Modulator::progress`.
	 * @return The modulated symbol for this call.
	 */
	int generateSymbol();

	/**
	 * @brief Re-enter the repeated-CP state unconditionally: clear the
	 *        SUV trace fields, announce the transition, rebuild the CP
	 *        message and its bit vector, and restart the symbol count.
	 */
	void enterRepeatedCP();

	/**
	 * @brief Record that a CP has been received: set `cpReceived` and the
	 *        CP's own `byte_04`, and clear the CP's `word_110`.
	 */
	void recivedCP();

	/**
	 * @brief Record a received CP tag. The first tag (`cpReceived` still
	 *        clear) raises the CP flags and rebuilds the message; every
	 *        tag after it takes the same E2u transition as `recivedEd`.
	 */
	void recivedCPtag();

	/**
	 * @brief Record a received Ed: take the E2u transition on a period
	 *        boundary (or the appropriate non-boundary state otherwise),
	 *        guarded by `flag_20`.
	 */
	void recivedEd();

	/**
	 * @brief The first Ed received during RRN: forces the following
	 *        state to 12 regardless of `e2uExtended`, warning if that
	 *        flag says the segment was in fact extended.
	 */
	void recivedFirstRrnEd();

	/**
	 * @brief Record a received Rt: past symbol 2399 and on a boundary of
	 *        12, hands this class's `bitsPerSymbol` to the CP, rebuilds
	 *        its message and enters SUV. A null `cp` is a real,
	 *        object-modelled error path here (state 29), not a guard
	 *        this reconstruction added.
	 */
	void recivedRt();

	/**
	 * @brief Record that SUV has been received: acts once per period,
	 *        latched by `word_1c4`, and only while in the SUV state.
	 */
	void recivedSUV();

	/**
	 * @brief Record a received SUV tag: from state 5 or 12/13, take the
	 *        E2u transition on a period boundary. Traces unconditionally
	 *        via `dsplibs_debug_printf`, not `edprintf`.
	 */
	void recivedSUVtag();

	/**
	 * @brief Part one of the silence-during-RRN SUV path: writes the
	 *        CP's `word_110` from outside on the way through (see the
	 *        file comment).
	 */
	void recivedPartOneSilenceRrnSUV();

	/**
	 * @brief Part one of the silence-during-RRN SUV-tag path: `cp->byte_04`
	 *        is both the branch selector and the value written on every
	 *        path out, including the early return.
	 */
	void recivedPartOneSilenceRrnSUVtag();

	/**
	 * @brief Part two of the silence-during-RRN SUV path. Byte-identical
	 *        to `recivedSUV` (finding F4702) -- the same 177 bytes,
	 *        written out a second time because the object has two
	 *        symbols for it, not one shared with `recivedSUV`.
	 */
	void recivedPartTwoSilenceRrnSUV();

	/**
	 * @brief Part two of the silence-during-RRN SUV-tag path: a one-line
	 *        forwarding call to `recivedSUVtag` (a sibling call in the
	 *        object, hence its own five-byte symbol rather than an
	 *        alias).
	 */
	void recivedPartTwoSilenceRrnSUVtag();

	/**
	 * @brief Exit CPt: ends on a whole number of periods counted from
	 *        symbol 24; anything else extends the segment (state 1)
	 *        instead of entering E1u.
	 */
	void exitCPt();

	/**
	 * @brief Exit TRN2u: moves state 3 to state 4 once at least one
	 *        symbol has been generated.
	 */
	void exitTRN2u();

	/**
	 * @brief Prepare the RRN-second-section fields (`word_1c4`,
	 *        `word_34`, `flag_20`, `byte_1c`, `word_18`, `cpReceived`) and
	 *        the CP's `word_110`/`byte_04` ahead of that section.
	 */
	void resetRRNSecondSection();

	/**
	 * @brief Zero `cpReceived`/`word_1c4`/`symbolCount`, the CP's
	 *        `word_110`, `silenceRrnRequest`/`word_30`/`word_34`/`word_38`/
	 *        `flag_20`, and set `word_28` to 1, ahead of an RRN
	 *        renegotiation.
	 */
	void resetBeforRRN();

	/**
	 * @brief Zero `symbolCount` and set `flag_3c`, ahead of an FPE
	 *        renegotiation.
	 */
	void resetBeforFPE();

	/**
	 * @brief Hand @p mappingParams to `bitsToSymbol` (not stored on this
	 *        object) and force its symbol block back to one. A null
	 *        argument only produces a diagnostic.
	 * @param mappingParams The mapping parameters to install on
	 *                       `bitsToSymbol`.
	 */
	void setMappingParams(V92MappingParams *mappingParams);

	/* Public for `offsetof`, which wants standard layout; and one access
	 * section, for the same reason V92Precoder.h gives. */

	/*
	 * +0x00  Which phase 4 upstream signal is being generated. Signed --
	 * see the file comment; thirteen of its values have names.
	 */
	int state;

	/*
	 * +0x04  Symbols emitted since the current signal segment started.
	 * Unsigned: every use is an unsigned `divl` or an unsigned compare
	 * (`cmp $0x18,%eax; ja` at .text+0x17ffb). It is what the four
	 * "enter X @ %d" messages print, and every transition clears it.
	 */
	unsigned int symbolCount;

	/*
	 * +0x08  Where in `pattern` the next bit comes from, advanced as
	 * `(patternIndex + 1) % patternLength` after each byte. `generateCPu`
	 * and `generateSUVu` are its only users.
	 */
	unsigned int patternIndex;

	/*
	 * +0x0c  The one thing this class reports back per symbol.
	 *
	 * `generateSymbol` clears it before the switch, on every call and
	 * whatever the state; exactly one arm then writes it, the value 9,
	 * beside the message "Phase4 Terminated @ %d". `reset` clears it
	 * too. `V92Modulator::progress` is the reader and it latches rather
	 * than consumes -- `sym = p4->generateSymbol(); ...; if (p4->eventCode)
	 * this->eventCode = p4->eventCode;` at .text+0x14e2b and +0x14f12 --
	 * and then tests its own copy against 9 to decide `enterDataPhase`.
	 * `V92Phase3Modulator` is read the same way at its own +0x14.
	 *
	 * Named `eventCode` on a matched-sibling cross-check: this class's
	 * own `V90Phase4Modulator` twin carries the identical field at the
	 * identical +0x0c, with the identical shape -- "cleared on (almost)
	 * every state entry, set to a small code... and copied into the
	 * caller's own `eventCode` field" -- and `V92Modulator::eventCode`'s
	 * own header already named this exact field (`V92Phase4Modulator`'s
	 * +0x0c) as one of the two sources it copies from, so the name was
	 * sitting one file away rather than missing (finding F4822 first
	 * established the shape; this applies the name once the cross-file
	 * evidence closed it). What the code 9 means to `V92Modulator::
	 * progress` is that function's own business, fully reconstructed and
	 * cited above -- the earlier note that it was unwritten was stale.
	 */
	unsigned int eventCode;

	/*
	 * +0x10 .. +0x17  Not touched by anything written here.
	 *
	 * NOT removable under the pad-removal workstream (F10150): `eventCode`
	 * ends at +0x10, already 4-byte aligned, and `word_18` below needs
	 * only that same 4-byte alignment -- a field-to-field gap here would
	 * be 0 bytes, not 8, if the member vanished. The compiler's own
	 * implicit padding does not reproduce this eight-byte span, same
	 * shape as `VPcmFloModem::pad_6fb8` -- stays explicit.
	 */
	unsigned char pad_10[8];

	/*
	 * +0x18  A counter, and `byte_1c` is its enable. `generateSymbol`'s
	 * state 5 arm increments it once per symbol and only while `byte_1c`
	 * is non-zero, and enters the repeated CP once it passes
	 * `suvLimit + 800`. Cleared by the constructor, by `reset`, by
	 * `enterRepeatedCP`, by `recivedSUVtag`, by `recivedCPtag`, by
	 * `resetRRNSecondSection` and on both of `generateSymbol`'s two
	 * remaining paths that touch it. What it counts is symbols in SUV,
	 * but only because state 5 is where it is counted -- nothing prints
	 * it and nothing else reads it.
	 */
	unsigned int word_18;

	/*
	 * +0x1c  Whether `word_18` is counting. Set to 1 at exactly one site
	 * -- `generateSymbol`'s state 12 arm, beside "CPu Terminated @ %d" --
	 * and cleared by the constructor, `reset`, `enterRepeatedCP`,
	 * `recivedSUVtag`, `recivedCPtag` and `resetRRNSecondSection`, always
	 * alongside `word_18`. One byte, stored as a byte.
	 */
	unsigned char byte_1c;

	/*
	 * +0x1d..+0x1f was `pad_1d[3]`: `byte_1c` ends at +0x1d and `flag_20`
	 * below is a 4-byte-aligned `unsigned int`, so natural alignment
	 * inserts exactly these three bytes with the member deleted --
	 * proved by the existing offset assertion on `flag_20` (0x020,
	 * V92Phase4Modulator.cpp). Zero readers/writers anywhere in the
	 * object (`tools/dis.py` over every `V92Phase4Modulator::` member
	 * function, `0x16de0..0x19160`); removed, finding F10150.
	 */

	/*
	 * +0x20  Set to 1 by every member that takes a state transition on a
	 * received tag (`recivedEd`, `recivedFirstRrnEd`, `recivedSUVtag`)
	 * and tested at the top of each of them to make the transition
	 * happen at most once; cleared by `resetBeforRRN` and
	 * `resetRRNSecondSection`. `recivedSUVtag` also requires it clear
	 * before it will act.
	 */
	unsigned int flag_20;

	/*
	 * +0x24  A length in symbols, written at one site and read at one
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
	 * +0x28, +0x30, +0x34  One three-way condition, and that is all that
	 * is established about them. `generateSymbol`'s E2u arm takes the
	 * "TRN2u Second at RRN" transition when `word_28 != 0 && word_30 != 0
	 * && word_34 == 0` and otherwise falls through to B1u; the three are
	 * loaded and tested in that order at .text+0x18830. `resetBeforRRN`
	 * sets +0x28 to 1 and clears the other two, `resetRRNSecondSection`
	 * and `generateSymbol`'s own state 23 and 24 arms set +0x34 to 1,
	 * and `reset` clears all three. Nothing prints any of them.
	 */
	unsigned int word_28;

	/*
	 * +0x2c  The SUV silence threshold an RRN carries. Two of
	 * `generateSymbol`'s arms pass it straight to `V92CP::setSUV(unsigned
	 * int)` -- `mov 0x2c(%esi),%edx` then the call, at .text+0x18193 and
	 * +0x18f39 -- which stores it at `V92CP::suv`; and
	 * `VpcmFloModem::runPcmModem`'s two RRN arms assign it from the
	 * already-named `V90ConnectionEvaluator::silenceRrnRequest` before
	 * calling `initiateRRN` (VpcmFloModem.cpp:1735 and :1746). Cleared by
	 * `resetBeforRRN` and by `reset`. The writer names the source and the
	 * callee names the destination, so the field takes the writer's name:
	 * the earlier "nothing written assigns it anything else" was stale.
	 */
	unsigned int silenceRrnRequest;

	/* +0x30  See `word_28`. */
	unsigned int word_30;

	/* +0x34  See `word_28`. */
	unsigned int word_34;

	/*
	 * +0x38  Read at two sites: `recivedRt` will not act while it is
	 * clear, and `generateSymbol` picks `word_24` as 4000 when it is set
	 * and 8004 when it is not. Assigned from the demodulator's counted
	 * block (`cp->word_ca0`) by the two silence-during-RRN arms of
	 * `VpcmFloModem::runPcmModem` (VpcmFloModem.cpp:1826 and :1834);
	 * `resetBeforRRN` clears it, and `reset` does NOT. The earlier
	 * comment's "nothing written sets it" and its implication that
	 * `reset` clears it were both stale. The name stays neutral.
	 */
	unsigned int word_38;

	/*
	 * +0x3c  Selects where `generateCPu`, `generateSUVu` and `generateE2u`
	 * take their bits from: zero means the class's own `pattern` or a
	 * constant, straight into `mapper`; non-zero means
	 * `V92BitsToSymbol::nofBitsForNextTime` and the two `process`
	 * overloads. `resetBeforFPE` sets it to 1 and nothing written clears
	 * it. Left unnamed: what the two arms mean is not established by
	 * anything written, only which is taken.
	 */
	unsigned int flag_3c;

	/*
	 * +0x40  The magnitude of the two-level symbol `generateCPt`,
	 * `generateE1u`, `generateRu` and `generateRuNot` return: each of the
	 * four returns `+this->amplitude` or `-this->amplitude` and nothing
	 * else. Signed, and that is forced -- `movswl 0x40(%ecx),%ebx` at
	 * .text+0x17041 sign-extends it into the returned 32-bit value.
	 * Usage inference across those four sites, which is CLAUDE.md's
	 * weakest tier; no format string prints it.
	 */
	short amplitude;

	/*
	 * +0x42  `reset`'s SECOND argument, raw: `mov %dl,0x42(%esi)` at
	 * .text+0x19062 is a byte store of that argument, separate from the
	 * 16-bit store above it and from the `+0x43` store below. The offset
	 * name stays: what the field is FOR is `reset`'s business and
	 * nothing else written here touches it (finding F5401).
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
	 * `reset` computes it as `arg + 2` in one byte -- `add $0x2,%dl;
	 * mov %dl,0x43(%esi)` at .text+0x19065 -- so a second argument of 254
	 * gives zero, the constructor does not initialise the field at all,
	 * and `bits[bitsPerSymbol - 1]` at zero is the fold D561 records.
	 * Finding F5401.
	 */
	unsigned char bitsPerSymbol;

	/*
	 * +0x44  `reset`'s FIFTH argument, stored and read once:
	 * `mov 0x34(%esp),%eax; mov %eax,0x44(%esi)` at .text+0x1903f, and
	 * `generateSymbol`'s state 5 arm gives up on SUV and enters the
	 * repeated CP once `word_18` has passed `suvLimit + 800`. So it is
	 * a threshold that the caller of `reset` sets, offset by 800; what
	 * it counts is `word_18`'s business and nothing written
	 * establishes that.
	 *
	 * Named `suvLimit` after `reset`'s own parameter of that name
	 * (rank 2, a typed source already carrying the word) -- this
	 * class's own header already documented the parameter as
	 * "Stored as `word_44`, the SUV threshold state 5 gives up at"
	 * before this rename, so the name was one property lookup away
	 * rather than missing. `reset`'s body disambiguates the
	 * now-identical parameter and member name with `this->`, a
	 * compile-time-only qualifier that cannot move generated code.
	 */
	unsigned int suvLimit;

	/* +0x48  The constructor's FOURTH argument, stored and not owned. */
	V92MappingParams *mappingParams;

	/* +0x4c  The upstream scrambler, built (5, 23, 99). A member: the
	 * destructor calls its D1. */
	Scrambler<unsigned char, unsigned char> scrambler;

	/* +0x6c  The constructor's SECOND argument, stored and not owned. */
	V92BitsToSymbol *bitsToSymbol;

	/*
	 * +0x70  `sysdep_malloc(0x2c)` and constructed. The one thing this
	 * class owns, and the one pointer its destructor releases.
	 */
	V92Mapper *mapper;

	/*
	 * +0x74  The constructor's THIRD argument. Not owned -- and the
	 * constructor writes a zero into its +0x110 on the way past.
	 */
	V92CP *cp;

	/*
	 * +0x78 .. +0x1a7  The differential bit and the bit block, in one
	 * array object, because the object addresses one byte below the
	 * block.
	 *
	 * +0x78 `prevBit` is the differentially encoded bit carried from one
	 * symbol to the next. Six generators exclusive-OR it into the bit
	 * they are about to emit and store the result back here; `generateCPt`
	 * toggles it with `^ 1` while the symbol count is still below 25, and
	 * `reset` clears it (`movl $0x0,0x78(%esi)` at .text+0x1909c).
	 *
	 * +0x7c `bitsExt[V92P4M_BITS_BELOW]` onwards is the bit block handed
	 * to `V92Mapper::process` for one symbol. Its declared length is the
	 * distance to `pattern`, not a measured bound -- see the file
	 * comment.
	 *
	 * The union is the object's own aliasing and not a convenience. The
	 * four differential generators fold into `bits[bitsPerSymbol - 1]`,
	 * and the blob forms that address as `0x7b(%count,%this,1)` with the
	 * count -- `bitsPerSymbol`, zero-extended from +0x43 -- in the index
	 * register (four load/store pairs, in `generateTRN2u`, `generateE2u`,
	 * `generateCPu` and `generateSUVu`, at .text+0x17c3a/+0x17c44,
	 * +0x17c91/+0x17c9b, +0x17dc0/+0x17dc7 and +0x17ef0/+0x17ef7).
	 *
	 * At `bitsPerSymbol == 0` that address is `this + 0x7b`, the top byte
	 * of `prevBit` -- so the load and the store overlap the very field
	 * the same statement assigns. Declared as two separate members,
	 * `bits[-1]` is out of bounds, the two stores may be emitted in
	 * either order, and they were: GCC 13 kept our order and GCC 3.4.2
	 * did not, which cost 240 checks on unmutated source (finding
	 * F4705). One array object spanning both makes the access defined
	 * C, states the aliasing where a reader will find it, and returns
	 * the store order to the source. D561, and it is D392's fix in the
	 * same shape -- an out-of-range window becoming a value inside our
	 * own array.
	 */
	union {
		unsigned int prevBit;
		unsigned char bitsExt[V92P4M_BITS_BELOW + V92P4M_BITS_LEN];
	};

	/*
	 * +0x1a8  A repeating bit pattern, one bit per byte, read at
	 * `pattern[patternIndex]` by `generateCPu`/`generateSUVu` and at
	 * `pattern[(symbolCount - 25) % patternLength]` by `generateCPt`, and
	 * in every case fed straight through the scrambler. `unsigned char *`
	 * is forced: `movzbl (%eax,%ecx,1),%edx` at .text+0x17d86. Nothing
	 * written assigns it.
	 */
	unsigned char *pattern;

	/* +0x1ac  The modulus `patternIndex` is kept below, and the same
	 * modulus `generateCPt` reduces its own index by. */
	unsigned int patternLength;

	/*
	 * +0x1b0  The period, in symbols, that `symbolCount` is reduced
	 * modulo to decide whether a segment may end: `symbolCount % word_1b0
	 * == 0` in `recivedEd`, `recivedFirstRrnEd` and `recivedSUVtag`, and
	 * `(symbolCount - 24) % word_1b0 == 0` in `exitCPt`. Left unnamed:
	 * four sites agree on the shape and none says what the period is OF.
	 */
	unsigned int word_1b0;

	/*
	 * +0x1b4 .. +0x1b7  Not touched.
	 *
	 * NOT removable under the pad-removal workstream (F10150): `word_1b0`
	 * ends at +0x1b4, already 4-byte aligned, and the field at +0x1b8
	 * needs only that same 4-byte alignment -- a field-to-field gap here
	 * would be 0 bytes, not 4, if the member vanished. The compiler's own
	 * implicit padding does not reproduce this four-byte span, same shape
	 * as `VPcmFloModem::pad_6fb8` -- stays explicit.
	 */
	unsigned char pad_1b4[4];

	/*
	 * +0x1b8  Written only on entry to E2u, and only ever with 12 or 13
	 * -- two of the three `state` values `recivedSUVtag` will act on. So
	 * it holds a state code, and holding one is all that is established;
	 * nothing written reads it back. `+0x910` in V92CP is the same
	 * restraint for the same reason.
	 */
	unsigned int word_1b8;

	/*
	 * +0x1bc  Non-zero means E2u has been extended. Named from a format
	 * string that fires on exactly this test -- `recivedFirstRrnEd`
	 * prints "V92Phase4Modulator: ERROR: E2u is extended in RRN !!!"
	 * when it is set (.text+0x17720), CLAUDE.md's strongest evidence
	 * tier (finding F4701). `recivedEd` and `recivedSUVtag` use it to
	 * pick `word_1b8`: 13 when it is set and 12 when it is not.
	 */
	unsigned int e2uExtended;

	/*
	 * +0x1c0  The "a CP has already been received" latch. Set by
	 * `recivedCP` and by `recivedCPtag`'s first-tag path, and cleared by
	 * the constructor, `resetBeforRRN`, `resetRRNSecondSection` and
	 * `reset`. Read at two sites: `recivedSUVtag` will not take its
	 * transition while it is clear, and `recivedCPtag` uses it to tell
	 * the first CP tag from every one after it. Usage inference across
	 * those sites, CLAUDE.md's weakest tier; no format string prints it.
	 * The earlier "Cleared by the constructor." was stale.
	 */
	unsigned int cpReceived;

	/*
	 * +0x1c4  The once-per-SUV latch: written by `recivedSUV`,
	 * `recivedPartTwoSilenceRrnSUV` and the `SUVu`->`CPu` arm of
	 * `generateSymbol`, and cleared by the constructor, `resetBeforRRN`,
	 * `resetRRNSecondSection` and `reset`. Both `recivedSUV` handlers
	 * read it as their early-out guard. The name stays neutral: the
	 * three writers do not agree on one semantic, and the earlier
	 * "Cleared by the constructor." was stale.
	 */
	unsigned int word_1c4;

	/*
	 * +0x1c8  The constructor's FIRST argument, stored and not owned.
	 * The last four bytes of the object.
	 */
	V92Parameters *params;
};

#endif /* DSPLIB_V92PHASE4MODULATOR_H */
