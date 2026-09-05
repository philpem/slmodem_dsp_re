/**
 * @file V92Phase3Modulator.h
 * @brief ITU-T V.92 phase 3 upstream symbol source: `V92Phase3Modulator`,
 *        the state machine that generates the V.92 phase 3 training
 *        sequence (Ru, Ja, Su and TRN1u segments) as a stream of linear
 *        samples.
 *
 * This is the V.92 upstream sibling of `V90Phase3Modulator` and not that
 * class with different constants -- different size, field order, state
 * alphabet and scrambler offset; every offset here comes from this class's
 * own bytes, not by reading across from the V.90 header.
 *
 * Not polymorphic (no vptr: `readelf -sW` lists no `D0` destructor variant).
 * The destructor's whole body is a tail call to the scrambler subobject's
 * destructor, confirming the scrambler is embedded rather than pointed to.
 *
 * `sizeof == 0x50` is the allocation `V92Modulator::V92Modulator` makes
 * before calling the constructor (finding F1107's rule: the `sysdep_malloc`
 * immediately before the call site, not the largest offset any member
 * touches -- the largest here happens to agree, at +0x4c, but +0x44 and
 * +0x48 are untouched by anything in this class and are `pad_44` for that
 * reason, not because nothing else uses them; `V92Modulator`, which holds
 * the only pointer to the object, was not read for this batch).
 *
 * Four of the thirteen symbols are defined here -- the constructor, the
 * destructor, `reset` and `generateSymbol`. The other nine are declared for
 * the record and deliberately left undefined, one class one owner applying
 * to methods just as it does to types (docs/v90cpp.md); nothing defined here
 * calls an undefined one.
 *
 * Data member names are invented and descriptive: the mangling preserves
 * method and type names but never a data member's (finding F226). A field
 * whose purpose this batch did not establish carries an offset-derived name.
 */

#ifndef DSPLIB_V92PHASE3MODULATOR_H
#define DSPLIB_V92PHASE3MODULATOR_H

#include "dsplib/Scrambler.h"
#include "dsplib/V92Ja.h"

class V92Parameters;

/*
 * `reset`'s fifth parameter. Never dereferenced (the whole use is a null
 * check), so an incomplete type is all this header needs; declaring it here
 * rather than including V90Phase3Modulator.h keeps the two classes
 * independent. The name is the mangling's (`PK19tagV90DILdescriptor`), the
 * same type V90Phase3Modulator.h defines.
 */
struct tagV90DILdescriptor;

/*
 * The modulator's phase 3 state. Sixteen values, unsigned-bounded and
 * dispatched through a sixteen-entry jump table in `generateSymbol`.
 *
 * The names come from the object's own method names, not a guess: each
 * generating arm is one of the small `generate*` methods inlined verbatim
 * (arm 0 is `generateRu`'s body, arm 1 `generateRuNot`'s, arms 7/9/10
 * `genereteSu`'s -- the misspelling is the original's -- arms 8/11
 * `genereteSuNot`'s, arms 4/5 `generateJa`'s, arms 3/12/13
 * `generateTRN1u`'s), matched to their state by a byte-for-byte comparison.
 * The four `exit*` methods name the rest, each a guard on exactly one state
 * value: `exitJa` acts on 4 and moves it to 6 on a 12-symbol boundary,
 * otherwise to 5; `exitSilence` acts on 6 and moves it to 7; `exitSuSecond`
 * acts on 9, to 11 on the boundary else 10; `exitTRN1u` acts on 12 and moves
 * it to 13.
 *
 * The sequence the arms themselves wire up is
 *
 *      Ru --384--> RuNot --24--> TRN1u --trn1uLength--> Ja
 *        ...[exitJa]--> JaEnd --12--> Silence
 *        ...[exitSilence]--> Su --144--> SuNot --24--> SuSecond
 *        ...[exitSuSecond]--> SuSecondEnd --12--> SuSecondNot --24-->
 *      TRN1uSecond ...[exitTRN1u]--> TRN1uSecondEnd --2040--> End
 *
 * State 2 is not a case: its jump-table entry holds the default label (the
 * one that prints "Illegal state"), while 6, 14 and 15 share a different,
 * silent block. The enumerator is named for what the table says it does,
 * not for a role nobody can show it has.
 */
enum V92Phase3ModulatorState {
	V92P3M_STATE_RU = 0,		/* Ru, 384 symbols                 */
	V92P3M_STATE_RU_NOT = 1,	/* its inversion, 24 symbols       */
	V92P3M_STATE_ILLEGAL_2 = 2,	/* dispatches to the default arm   */
	V92P3M_STATE_TRN1U = 3,		/* scrambled all-ones              */
	V92P3M_STATE_JA = 4,		/* Ja, with no timeout             */
	V92P3M_STATE_JA_END = 5,	/* Ja to the 12-symbol boundary    */
	V92P3M_STATE_SILENCE = 6,	/* emits zero                      */
	V92P3M_STATE_SU = 7,		/* Su, 144 symbols                 */
	V92P3M_STATE_SU_NOT = 8,	/* its inversion, 24 symbols       */
	V92P3M_STATE_SU_SECOND = 9,	/* Su again, with no timeout       */
	V92P3M_STATE_SU_SECOND_END = 10,/* Su to the 12-symbol boundary    */
	V92P3M_STATE_SU_SECOND_NOT = 11,/* SuNot again, 24 symbols         */
	V92P3M_STATE_TRN1U_SECOND = 12,	/* TRN1u again, with no timeout    */
	V92P3M_STATE_TRN1U_SECOND_END = 13,/* TRN1u past 2039, to the boundary */
	V92P3M_STATE_END = 14,		/* silent; entered from 13         */
	V92P3M_STATE_SILENT_15 = 15	/* silent; no arm enters it        */
};

class V92Phase3Modulator {
public:
	/**
	 * @brief Construct the modulator: build the (5, 23, 99) scrambler
	 *        subobject, store `params`, and reset() into state
	 *        #V92P3M_STATE_RU with no symbols pre-generated. The `params`
	 *        store must precede the reset() call -- reset() computes
	 *        `trn1uLength` from two `V92Parameters` fields, so the other
	 *        order would read an uninitialized pointer (both orderings
	 *        are mutation-tested in test/mutations/v92p3mod.json).
	 * @param p  Negotiated V.92 parameters; stored, not owned.
	 */
	V92Phase3Modulator(V92Parameters *);

	/**
	 * @brief Destroy the modulator. Frees the scrambler's history buffer;
	 *        nothing else here is allocated.
	 */
	~V92Phase3Modulator();

	/**
	 * @brief Reinitialize the modulator: reset the scrambler, symbol
	 *        count and event code, recompute `trn1uLength` from
	 *        `params`, set the amplitudes, and generate `nSymbols`
	 *        symbols before returning (discarding them).
	 * @param levelArg  Dead -- never read; the amplitude is `reset`'s
	 *                  own literal 4000. Kept because it is part of the
	 *                  mangled signature.
	 * @param stateArg  The state to enter.
	 * @param nSymbols  Number of symbols to generate() and discard before
	 *                  returning (0 from the constructor).
	 * @param jaArg     Source of the Ja bit vector, or NULL to leave
	 *                  `jaBits`/`jaBitCount` at NULL/0 (state Ja must not
	 *                  be reached in that case: generateJa() divides by
	 *                  `jaBitCount` unchecked).
	 * @param dilArg    Unused except to trigger a debug complaint if
	 *                  non-NULL while `jaArg` is NULL; never dereferenced.
	 * @param lastArg   Stored verbatim into `word_00`.
	 */
	void reset(short, V92Phase3ModulatorState, unsigned int, V92Ja *,
		   const tagV90DILdescriptor *, unsigned int);

	/**
	 * @brief Advance the state machine by one symbol: increment
	 *        `symbolCount`, dispatch on `state` to produce one linear
	 *        sample, and let each state's exit test (inlined here, not
	 *        via the `exit*` methods) move to the next state at its own
	 *        boundary.
	 * @return The generated symbol, sign-extended from the underlying
	 *         `short`.
	 */
	int generateSymbol();

	/**
	 * @brief Ru: a six-symbol cycle, three symbols at +codeLevel then
	 *        three at -codeLevel.
	 * @return The next Ru symbol.
	 */
	int generateRu();

	/**
	 * @brief RuNot: generateRu()'s inversion, the same six-symbol cycle
	 *        with the two amplitudes exchanged.
	 * @return The next RuNot symbol.
	 */
	int generateRuNot();

	/**
	 * @brief Su: a six-symbol cycle at `suLevel`, +0 -0 - with the two
	 *        zeros one position apart rather than adjacent.
	 * @return The next Su symbol.
	 */
	int genereteSu();		/* the object's own spelling */

	/**
	 * @brief SuNot: genereteSu()'s inversion; the zeros stay in place,
	 *        the two non-zero amplitudes are exchanged.
	 * @return The next SuNot symbol.
	 */
	int genereteSuNot();		/* likewise */

	/**
	 * @brief Ja: twenty-five symbols of a constant 1 bit, then the
	 *        `V92Ja` bit vector read cyclically, each bit fed through the
	 *        scrambler and XORed into `polarity` to choose the sign of
	 *        `codeLevel`.
	 * @return The next Ja symbol.
	 */
	int generateJa();

	/**
	 * @brief TRN1u: the scrambler driven with a constant 1 bit, whose
	 *        output chooses the sign of `codeLevel`.
	 * @return The next TRN1u symbol.
	 */
	int generateTRN1u();

	/**
	 * @brief From state Ja, move on once at least one symbol has run:
	 *        to Silence on a 12-symbol boundary, otherwise to JaEnd (run
	 *        Ja on to the next boundary). No-op in any other state.
	 */
	void exitJa();

	/**
	 * @brief From state Silence, move to Su once at least one symbol has
	 *        run. No-op in any other state.
	 */
	void exitSilence();

	/**
	 * @brief From state SuSecond, move on once at least one symbol has
	 *        run: to SuSecondNot on a 12-symbol boundary, otherwise to
	 *        SuSecondEnd. No-op in any other state.
	 */
	void exitSuSecond();

	/**
	 * @brief From state TRN1uSecond, move to TRN1uSecondEnd once at least
	 *        one symbol has run, leaving `symbolCount` running (state
	 *        13's own exit test in generateSymbol() needs the count
	 *        state 12 started). No-op in any other state.
	 */
	void exitTRN1u();

	/* --- data members; see the file comment on the naming --- */

	/*
	 * `reset`'s LAST argument, stored by `mov %ecx,(%esi)` at
	 * .text+0x16bd5 and read by nothing in this class.  `enterPhase3`
	 * sources it from `*(int *)(this->+0x10 + 4)`, an object this batch
	 * did not read, so the field has no established purpose and takes an
	 * offset-derived name.
	 */
	unsigned int word_00;		/* +0x000                          */

	/*
	 * The Ru / Ja / TRN1u amplitude.  Set to the literal 4000 by `reset`
	 * and read by six arms as `+codeLevel` or `(short)-codeLevel`.
	 */
	short codeLevel;		/* +0x004                          */

	/*
	 * The Su amplitude, `(short)((float)(codeLevel * 1.224744871391589)
	 * + 0.5f)` -- 4899 for the only codeLevel the object ever has.  The
	 * FLOAT intermediate is forced: .text+0x16c4f multiplies by an
	 * EIGHT-byte constant (`fmull .rodata.cst8+0x20`) and then rounds the
	 * product through memory as four bytes (`fstps`/`flds`) before adding
	 * a FOUR-byte 0.5 (`fadds .rodata.cst4+0xe4`).  A double expression
	 * throughout would need neither the round trip nor a `float` 0.5.
	 * 1.224744871391589 is sqrt(3/2) to sixteen digits.
	 */
	short suLevel;			/* +0x006                          */

	V92Phase3ModulatorState state;	/* +0x008                          */

	/*
	 * Symbols emitted in the current state.  `generateSymbol` increments
	 * it BEFORE the dispatch, so every arm's tests see the count including
	 * the symbol it is about to produce.  UNSIGNED: all four residues
	 * (`% 6` and `% 12`) use the 0xaaaaaaab reciprocal with a plain `mul`
	 * and a logical shift and no sign fixup, and the two range tests are
	 * `ja`/`jbe`.
	 */
	unsigned int symbolCount;	/* +0x00c                          */

	/*
	 * How long the TRN1u state runs, in symbols. The name is the original
	 * author's own: `reset` prints it as "V92Phase3Modulator: TRN1u state
	 * length set to %d". Computed from two `V92Parameters` fields, rounded
	 * up to a multiple of twelve and floored at 8160.
	 */
	unsigned int trn1uLength;	/* +0x010                          */

	/*
	 * The caller's per-symbol notification: zero unless this symbol was
	 * the last of a state.  Every arm writes it, `reset` clears it, and
	 * nothing in this class reads it.  Five values occur -- 2 leaving
	 * RuNot, 3 leaving TRN1u, 5 leaving SuNot, 7 leaving SuSecondNot and 8
	 * leaving TRN1uSecondEnd.
	 */
	unsigned int eventCode;		/* +0x014                          */

	/* Constructed (5, 23, 99); 32 bytes, so it ends at +0x38. */
	Scrambler<unsigned char, int> scrambler;	/* +0x018          */

	/*
	 * The differential encoder's running sign, used by the Ja arm only:
	 * `polarity ^= scrambler.process(bit)`, then `polarity ? -codeLevel :
	 * codeLevel`. TRN1u seeds it from the sign of the symbol it ends on
	 * (1 for a symbol less than or equal to zero).
	 */
	unsigned int polarity;		/* +0x038                          */

	unsigned char *jaBits;		/* +0x03c the V92Ja vector, `ja + 4` */
	unsigned int jaBitCount;	/* +0x040 `*(unsigned int *)ja`      */

	/*
	 * Not touched by any of this class's thirteen symbols -- see the file
	 * comment: the allocation says the object is 80 bytes, so these eight
	 * exist, but what writes them (if anything) is `V92Modulator`'s
	 * business (finding F1107).
	 */
	unsigned char pad_44[8];	/* +0x044                          */

	V92Parameters *params;		/* +0x04c the constructor's argument */
};

#endif /* DSPLIB_V92PHASE3MODULATOR_H */
