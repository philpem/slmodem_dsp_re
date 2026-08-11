/*
 * V92Phase3Modulator.h -- the V.92 phase 3 UPSTREAM symbol source.
 *
 * Reconstructed from dsplibs.o `V92Phase3Modulator.cpp`.  This is the V.92
 * upstream sibling of `V90Phase3Modulator` and it is NOT that class with
 * different constants: different size, different field order, a different
 * state alphabet and a different scrambler offset.  Every offset below is
 * derived from THIS class's own bytes; the V.90 header was read first as a
 * hypothesis and is cited nowhere as evidence.
 *
 * NOT POLYMORPHIC.  `readelf -sW` lists `D1` and `D2` and no `D0`, and GCC
 * emits a deleting destructor only for a virtual one, so offset 0 is a real
 * member and there is no vptr.  The destructor's whole body is a tail call to
 * `Scrambler<unsigned char, int>::~Scrambler` on `this + 0x18` (.text+0x16290,
 * 22 bytes), which is the second, independent statement that the scrambler is
 * a SUBOBJECT and not a pointer.
 *
 * THE OBJECT IS 80 BYTES (0x50), AND THAT IS AN ALLOCATION, NOT A SCAN.
 * Finding 1107's rule: take the `sysdep_malloc` immediately before the
 * constructor's call site, never the largest displacement.  In
 * `V92Modulator::V92Modulator` at .text+0x15299:
 *
 *     15299:  c7 04 24 50 00 00 00   movl   $0x50,(%esp)
 *     152a0:  e8 ..                  call   sysdep_malloc
 *     152a5:  89 c3                  mov    %eax,%ebx      <- the modulator
 *     152aa:  89 1c 24               mov    %ebx,(%esp)
 *     152b1:  e8 ..                  call   V92Phase3Modulator::V92Phase3Modulator
 *     152b6:  89 5e 44               mov    %ebx,0x44(%esi)
 *
 * and identically at +0x15579 in the `C2` copy.  The largest displacement any
 * of the thirteen symbols uses is +0x4c, so the two readings agree here --
 * but they agree by luck, and +0x44 and +0x48 are the reason to say so: no
 * symbol of this class touches either.  They are `pad_44` below because this
 * class does not write them, NOT because nothing does; `V92Modulator` holds
 * the only pointer to the object and was not read for this batch.
 *
 * THE CONSTRUCTOR SETTLES THE FIRST TWO OFFSETS (.text+0x16d00, 105 bytes):
 * `lea 0x18(%ebx),%edx` then `Scrambler<unsigned char,int>::Scrambler(5, 23,
 * 99)` places the scrambler at +0x18; `mov 0x34(%esp),%eax; mov %eax,0x4c(%ebx)`
 * puts its `V92Parameters *` argument at +0x4c; and the body ends in
 * `reset(4000, (V92Phase3ModulatorState)0, 0, NULL, NULL, 0)`.
 *
 * TWO OF THE THIRTEEN SYMBOLS ARE DEFINED -- `reset` and `generateSymbol`.
 * The other eleven are declared for the record and deliberately left
 * undefined: one class, one owner applies to methods, and defining a method
 * whose callers are not written re-opens the link closure for the whole test
 * suite (docs/v90cpp.md).  Nothing defined here calls an undefined one.
 *
 * Data member names are invented and descriptive -- the mangling preserves
 * method and type names and never a data member's (finding 226).  Fields whose
 * purpose this batch did not establish carry an offset-derived name.
 */

#ifndef DSPLIB_V92PHASE3MODULATOR_H
#define DSPLIB_V92PHASE3MODULATOR_H

#include "dsplib/Scrambler.h"
#include "dsplib/V92Ja.h"

class V92Parameters;

/*
 * `reset`'s fifth parameter.  It is NEVER DEREFERENCED -- the whole use is
 * `mov 0x34(%esp),%ebx; test %ebx,%ebx` at .text+0x16ccd -- so an incomplete
 * type is all this header needs, and declaring it here rather than including
 * V90Phase3Modulator.h keeps the two classes independent.  The name is the
 * mangling's: `PK19tagV90DILdescriptor`, so it is the same nineteen-character
 * type V90Phase3Modulator.h defines and the two declarations agree.
 */
struct tagV90DILdescriptor;

/*
 * The modulator's phase 3 state.  Sixteen values: `generateSymbol` bounds the
 * field with `cmp $0xf,%eax; ja` -- UNSIGNED -- and dispatches through a
 * sixteen-entry jump table at .rodata:0x5e4.
 *
 * THE NAMES COME FROM THE OBJECT'S OWN METHOD NAMES, not from a guess.  Each
 * generating arm is one of the small `generate*` methods inlined verbatim, and
 * matching the two is a byte comparison: arm 0 is `generateRu`'s body
 * (.text+0x163a0), arm 1 `generateRuNot`'s (+0x16400), arms 7/9/10
 * `genereteSu`'s (+0x16460, the misspelling is the original's), arms 8/11
 * `genereteSuNot`'s (+0x164e0), arms 4/5 `generateJa`'s (+0x16570) and arms
 * 3/12/13 `generateTRN1u`'s (+0x165c0).
 *
 * The four `exit*` methods name the rest, and each one is a guard on exactly
 * one state value:
 *
 *   `exitJa`        (+0x162b0) acts on 4 and moves it to 6 on the 12-symbol
 *                   boundary, otherwise to 5 -- so 4 is Ja and 5 is Ja run on
 *                   to that boundary, and 6 is what follows.
 *   `exitSilence`   (+0x16300) acts on 6 and moves it to 7 -- 6 is Silence,
 *                   which is also what its arm emits: zero.
 *   `exitSuSecond`  (+0x16330) acts on 9, to 11 on the boundary else 10.
 *   `exitTRN1u`     (+0x16380) acts on 12 and moves it to 13.
 *
 * The sequence the arms themselves wire up is
 *
 *      Ru --384--> RuNot --24--> TRN1u --trn1uLength--> Ja
 *        ...[exitJa]--> JaEnd --12--> Silence
 *        ...[exitSilence]--> Su --144--> SuNot --24--> SuSecond
 *        ...[exitSuSecond]--> SuSecondEnd --12--> SuSecondNot --24-->
 *      TRN1uSecond ...[exitTRN1u]--> TRN1uSecondEnd --2040--> End
 *
 * STATE 2 IS NOT A CASE.  Its jump-table entry at .rodata:0x5ec holds the
 * DEFAULT label (.text+0x16625, the one that prints "Illegal state"), while
 * 6, 14 and 15 share a different, silent block at +0x1666e.  The enumerator is
 * named for what the table says it does, not for a role nobody can show it
 * has.
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
	/*
	 * THE FIRST PARAMETER IS DEAD.  `reset`'s `short` arrives at
	 * `0x24(%esp)` and that slot is never read; the amplitude is the
	 * literal `movw $0xfa0,0x4(%esi)` in both of the tail-duplicated
	 * blocks at .text+0x16c35 and +0x16cc1.  Its only two callers pass
	 * 4000 (`enterPhase3` at .text+0x144ab and the constructor at
	 * +0x16d4b), so the object cannot tell the difference either.  It is
	 * kept, unnamed, because it is in the mangled name.
	 */
	void reset(short, V92Phase3ModulatorState, unsigned int, V92Ja *,
		   const tagV90DILdescriptor *, unsigned int);

	/*
	 * RETURNS the symbol as a linear level.  A return type is not mangled,
	 * so it is measured: every path ends `mov %ebx,%eax` on a value that
	 * has just been through `movswl %bx,%ebx`, which is what a `short`
	 * local widened at an `int` return looks like.  The value itself is a
	 * `short` throughout -- each negation is taken modulo 2**16 before the
	 * widening.
	 */
	int generateSymbol();

	/*
	 * Declared, not defined -- see the file comment.  A return type is not
	 * mangled, so none of these has a known one.  The constructor and
	 * destructor are NOT declared, deliberately: declaring either makes
	 * the class non-trivial, which deletes the default members of a union
	 * holding one -- and the test fixture is exactly such a union.
	 */
	void generateRu();
	void generateRuNot();
	void genereteSu();		/* the object's own spelling */
	void genereteSuNot();		/* likewise */
	void generateJa();
	void generateTRN1u();
	void exitJa();
	void exitSilence();
	void exitSuSecond();
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
	 * How long the TRN1u state runs, in symbols.  THE NAME IS THE
	 * ORIGINAL AUTHOR'S: `reset` prints it as "V92Phase3Modulator: TRN1u
	 * state length set to %d" (.rodata.str1.4+0x3b5c).  Computed from two
	 * V92Parameters fields, rounded up to a multiple of twelve and floored
	 * at 8160.
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
	 * The differential encoder's running sign, used by the Ja arms only:
	 * `polarity ^= scrambler.process(bit)` and then `polarity ? -codeLevel
	 * : codeLevel`.  TRN1u seeds it from the sign of the symbol it ends
	 * on, through `test %bx,%bx; setle` -- so the seed is 1 for a symbol
	 * less than OR EQUAL TO zero.
	 */
	unsigned int polarity;		/* +0x038                          */

	unsigned char *jaBits;		/* +0x03c the V92Ja vector, `ja + 4` */
	unsigned int jaBitCount;	/* +0x040 `*(unsigned int *)ja`      */

	/*
	 * NOT TOUCHED BY ANY OF THIS CLASS'S THIRTEEN SYMBOLS.  See the file
	 * comment: the allocation says the object is 80 bytes, so these eight
	 * exist; what wrote them, if anything, is `V92Modulator`'s business
	 * and finding 1107 is the reason not to call them absent.
	 */
	unsigned char pad_44[8];	/* +0x044                          */

	V92Parameters *params;		/* +0x04c the constructor's argument */
};

#endif /* DSPLIB_V92PHASE3MODULATOR_H */
