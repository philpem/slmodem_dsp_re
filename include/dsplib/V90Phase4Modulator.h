/*
 * V90Phase4Modulator.h -- the V.90 / V.92 phase 4 downstream symbol source.
 *
 * Reconstructed from dsplibs.o.  Forty-three members and 12,078 bytes of
 * code, of which THIRTY-SIX are written in
 * src/pump/v90/V90Phase4Modulator.cpp -- `grep -c '^V90Phase4Modulator::'`
 * is where that number comes from, so it can be re-measured rather than
 * believed.  Both symbol pumps, `reset` and `generateSymbol` (finding 7520,
 * the newest) are among them.
 *
 * This paragraph used to end "everything except `generateSymbol` and the six
 * `generate*` sequence sources", and that clause was ALREADY STALE before
 * 7520 -- `generateRdRt`, `generateRdRtNot`, `generateRf`, `generateRi` and
 * both `generateDataSymbolBefore*` are defined in the .cpp today.  It is
 * removed rather than re-derived: 7520 owns `generateSymbol` and not the
 * exception list, and a count with a command beside it is worth more than a
 * list with nothing behind it (findings 6100, 6103).
 *
 * NOT POLYMORPHIC: `~V90Phase4Modulator` is listed with `D1` and `D2` and no
 * `D0`, so offset 0 is a real member and there is no vptr.
 *
 * THE OBJECT IS 12,204 BYTES, and the shape of the evidence matters because
 * the number is so much larger than its siblings'.  The largest
 * `this`-relative displacement across all forty-five defined members is
 * +0x2fa8, four bytes wide, so the object ends at 0x2fac.  The displacements
 * are not scattered: they fall in two dense runs, +0x00..+0x58 in fours and
 * +0x2f64..+0x2fa8 in twos and then fours, with nothing between.  A run of
 * consecutive two-byte fields at +0x2f68..+0x2f8c, written by the constructor
 * and read by `recivedCPtag`, `recivedSUV`, `resetBeforRRN`, `reset` and the
 * destructor, is a field block and not an artefact of losing track of which
 * register held `this`.  The ~12 KB between the two runs is the constellation
 * the modulator maps into.
 *
 * That was a BOUND on the object, arrived at the same way as every other size
 * in that task (finding 215): the maximum displacement plus the width of what
 * sits at it.  It was not a claim that +0x5c..+0x2f63 contains no larger
 * member -- nothing reaches past +0x2fab, which is all a displacement scan
 * can say.
 *
 * THE BOUND IS NOW THE SIZE.  `V90Modulator`'s constructor does `movl
 * $0x2fac,(%esp) ; call sysdep_malloc` and then calls this class's `C1` on
 * what came back: finding 1246's oracle, which is the original compiler's own
 * `sizeof` and not a scan of anything.  It agrees with the displacement bound
 * to the byte, which is the first independent confirmation the number has had.
 *
 * THE CONSTRUCTOR'S THIRTEEN STORES fill in most of the first run and all of
 * the last.  Five of the eight arguments are borrowed pointers stored straight
 * through at +0x44..+0x54; the `Scrambler<unsigned char,unsigned char>` at
 * +0x58 is EMBEDDED (`lea 0x58(%esi),%edx` before the call, never a load) and
 * is built with (0x12, 0x17, 0x63), V.90's taps 18 and 23; the eighth argument
 * lands at +0x2f98 and the second at +0x00; +0x2f9c and +0x2fa0 are cleared;
 * and +0x2fa8 takes the V90Parameters.
 *
 * THE BITS-TO-SYMBOL CONVERTER IS EITHER SUPPLIED OR OWNED, AND +0x2fa4
 * RECORDS WHICH.  A non-null third argument is stored at +0x44 and +0x2fa4 is
 * set to 1; a null one makes the constructor allocate 0x24, build a
 * `V90BitsToSymbol(0x140, params)` in it, and set +0x2fa4 to 0.  The
 * destructor reads +0x2fa4 FIRST and skips the release entirely when it is
 * nonzero, so the flag is an ownership bit and 1 means "not ours".  Its two
 * arms are otherwise identical: both end in the scrambler's destructor, which
 * is why the blob has two copies of that call.
 *
 * The allocating arm passes the INCOMING PARAMETER as the converter's
 * V90Parameters and not the `params` member it has just stored -- `%edi` is
 * still live and is not reloaded from +0x2fa8, which it would have to be if
 * the original had named the member.  Nothing can tell the two apart by
 * behaviour; it is written the blob's way because the blob is the
 * specification.
 *
 * `sessionFlag` at +0x00 is named for the method that writes it, exactly as
 * `V90Phase3Modulator::setSessionFlag` and `V90Modulator::setSessionFlag`
 * write their own classes' +0x000; in `V90Phase3Modulator` the field's
 * meaning is settled -- nonzero selects V.92 -- and this class's setter has
 * the same signature and the same one-line body.
 *
 * ---------------------------------------------------------------------------
 * THE 12 KB IS ONE BIT BUFFER AND A TAIL OF TEN FIELDS
 *
 * The paragraph that used to close this comment said everything after
 * `sessionFlag` was `pad_`, "because forty-two unwritten members' state is not
 * something a scan of displacements can name".  A scan of displacements is not
 * what named it: the forty-two undefined members are all still undefined, but
 * they are all still in the object, and every one of their accesses is in the
 * blob.  +0x0078..+0x2f97 is now fields.  +0x0004..+0x0043 is not, and is left
 * alone.
 *
 * THE ARGUMENT THAT +0x0078 IS ONE ARRAY.  Over the class's whole extent --
 * .text+0x2c5a0..+0x2f72f, all forty-five symbols, `reset` at +0x2f630+0xff
 * being the last and `generateSymbol` at +0x2f600 the second last -- there are
 * exactly eleven distinct memory displacements in
 * [+0x78, +0x2f58) on ANY base register, and this holds without tracking which
 * register carries `this`, so no register-tracking bug can weaken it:
 *
 *     0x78(%esi)                   26 times, and every one of them a `lea`
 *     0x84/0x104/0x184/           only in `setRdRtSymbols` and `setRfSymbols`,
 *       0x204/0x284(%esi)         where `%esi` is ARGUMENT 2 (`mov 0x14(%esp),
 *                                 %esi` at +0x2d189 and +0x2d389) and the
 *                                 object is a `V90MappingParams`
 *     0x114(%ecx), 0x114(%edi)    reached through `mov 0x48(%..),%..` -- the
 *                                 `mp` member; `V90MP.h` names +0x114
 *     0xca0(%ecx)                 reached through `mov 0x54(%..),%ecx` -- the
 *                                 `cp` member; `V90CP.h` has +0xca0
 *     0x78/0x80(%esp)             stack frame
 *
 * So nothing in the span has an offset of its own.  Whatever is in there is
 * reached through the +0x78 base and no other, which is the same argument
 * `V90CP.h` makes for its own bit vector, in the same words: that the whole
 * span is ONE array is the MODELLING CHOICE, not a measurement.  What is
 * measured is the start and the fact that nothing else is in it.  `reset` was
 * read for a `memset`/`rep stos` that would have pinned the length instead;
 * it has none, and neither has the constructor.
 *
 * WHAT THE BUFFER IS.  Six `generate*` members and both symbol pumps do
 *
 *     lea 0x58(%esi),%eax                 the scrambler
 *     lea 0x78(%esi),%ebx                 this buffer
 *     call Scrambler<unsigned char,unsigned char>::process(const unsigned
 *                                         char *src, unsigned char *dst,
 *                                         unsigned int n)
 *     call V90BitsToSymbol::process(unsigned char *bits, unsigned int n)
 *
 * -- so it is the scrambler's OUTPUT and the bits-to-symbol converter's input,
 * one byte per bit, and `unsigned char` is the mangling's and not a guess
 * (`_ZN9ScramblerIhhE7processEPKhPhj`, `_ZN15V90BitsToSymbol7processEPhj`).
 * `generateB1d`, `generateTRN2d`, `generateEd`, `generateMP`, `generateCPd`
 * and `generateSUVd` reach it through `processAllOnes` instead, which fills it
 * with the scrambling of an all-ones input.
 *
 * AND ITS LENGTH IS THE SAME 12,000 AS `V90CP::bits`.  +0x2f58 - +0x78 is
 * 0x2ee0, which is `V90CP_BITS` to the byte.  That is not a coincidence to
 * shrug at: the bits this buffer receives are a scrambled COPY of exactly the
 * vector `V90CP::getBitVector` hands over, so a buffer that can hold the
 * longest CP sequence is what the class needs and 12,000 is what it has.  Two
 * independent readings agreeing is the strongest this can be short of a
 * `memset` -- and there is no `memset`.
 *
 * THE TAIL IS TWO PARALLEL TRIPLES AND TWO SYMBOL TABLES.  The MP triple and
 * the CP triple are the same three lines of code against two different
 * message objects:
 *
 *     +0x2f58/+0x2f5c/+0x2f60      `V90MP::getBitVector(&this->mpBitCount)`
 *                                  into `mpBits`, then
 *                                  `mpSequenceSymbols = 6 * mpBitCount /
 *                                  mp->groupSize` (`0x114`)
 *     +0x2f8c/+0x2f90/+0x2f94      `V90CP::getBitVector(&this->cpBitCount)`
 *                                  into `cpBits`, then
 *                                  `cpSequenceSymbols = 6 * cpBitCount /
 *                                  cp->word_3ba8` (the same "group size")
 *
 * `exitMP` at +0x2d0db..+0x2d113 is the MP one and `enterRepeatedCPd` at
 * +0x2c7b4..+0x2ec is the CP one; `recivedSUV`, `recivedCPtag` and
 * `generateV92Symbol` repeat the CP one verbatim.  Both divisors are named
 * "the group size" by their own headers, both from `calcSequenceLength`, so
 * `6 * bits / groupSize` is a count of SYMBOLS: six symbols carry one group.
 * `V90BitsToSymbol.h` already names a field of that exact shape --
 * `extraSymbols`, `(6 * mp[+0x624]) / mp[+0x620]` -- and calls it symbols.
 * Both fields are then used only as a modulus on the symbol counter at +0x08
 * (`divl`, then `test %edx,%edx`), which is a state machine asking whether a
 * whole repetition of the sequence has been sent.
 *
 * Everything else in the tail is left with a neutral name.  +0x2f64 is
 * `bitsToSymbol->extraSymbols + 12` and is compared for equality against the
 * symbol counter, but nothing establishes what the twelve is, and a name for
 * it would be a guess a future reader would believe.
 */

#ifndef DSPLIB_V90PHASE4MODULATOR_H
#define DSPLIB_V90PHASE4MODULATOR_H

#include "dsplib/Scrambler.h"		/* embedded at +0x58, 0x20 bytes */
#include "dsplib/V90Phase3Modulator.h"	/* for `PcmType`; see +0x0038 below */

class V90BitsToSymbol;
class V90CP;
class V90MP;
class V90MappingParams;
class V90Parameters;

/*
 * ---------------------------------------------------------------------------
 * THE PHASE 4 STATE.  `Phase4ModulatorState` is the object's own type name:
 * it is in the mangling of two members, `setNextStateAfterTRN2d`
 * (`_ZN18V90Phase4Modulator22setNextStateAfterTRN2dE20Phase4ModulatorState`)
 * and `reset` (`...5resetE7PcmTypeh20Phase4ModulatorStatejj`).  The
 * ENUMERATOR names are not; they are derived one at a time and the derivation
 * is beside each.
 *
 * WHERE A NAME COMES FROM A STRING it is the strongest kind this tree
 * recognises (CLAUDE.md's evidence order, rule 1): the assignment `movl
 * $N,0x4(%reg)` and the `edprintf` that names the state are in the SAME
 * straight-line run, with no branch target and no jump between them, so the
 * message and the store cannot be paired wrongly.  That was checked
 * mechanically over all forty-five members of the class, not by eye.
 *
 * WHERE A NAME COMES FROM A METHOD NAME -- 0x00, 0x04, 0x08 -- the argument is
 * that `exitX()` acts only when the field holds one particular value, and it
 * is corroborated rather than assumed: `exitMPNot()` acts only on 0x0e, and
 * 0x0e is independently named "enter MPNot" by a string.  The same shape then
 * reads `exitMP()` on 0x04 as MP and `exitRi()` on 0x00 as Ri.
 * `enterRepeatedCPd()` assigns 0x08 and nothing else does under V.90.
 *
 * WHERE THERE IS NEITHER, THE VALUE KEEPS AN OFFSET NAME.  Nine of them do.
 * Eight are the same shape -- a state entered when the symbol counter is NOT
 * yet on a sequence boundary, which goes on emitting what it was emitting and
 * then hands on -- and `V90Phase3Modulator`'s `_END` states are exactly that,
 * but calling them `_END` here would be usage inference dressed as a
 * derivation, and 3120's rule says a wrong name is worse than an offset.
 * `P4D_STATE_UNNAMED_11` is the precedent for the spelling.
 *
 * 0x05 IS NAMED AND 0x05 IS THE ONE TO BE CAREFUL ABOUT.  Three different
 * messages precede an assignment of 5; two of them are "enter SUVd" and
 * "enter SUVd at RRN" and the third, "CPd Terminated", names the state being
 * LEFT rather than the one being entered.  Two independent sites naming it
 * SUVd is what carries it, and `recivedSUV()` acting only when the field is 5
 * agrees.
 *
 * 0x14 AND 0x1c ARE HERE NOW, AND 0x1f IS STILL ABSENT.  This paragraph used
 * to say all three were absent because "no member of the class stores or
 * compares them", and the two symbol pumps disprove it for two of the three:
 * `generateV90Symbol`'s jump table at `.rodata+0xa94` runs 0x00..0x1b and
 * `generateV92Symbol`'s at +0xb04 runs 0x00..0x1e, and the entry at 0x14 in
 * both -- and at 0x1c in the second -- is a distinct arm rather than the
 * default edge.  A jump-table slot that is not the default IS a `case` label,
 * so the enumeration has those two.  0x1f is past the end of the larger table
 * and nothing else in the class mentions it, so it stays out.
 *
 * A C++ enumeration does not have to be contiguous and inventing an
 * enumerator to make it look tidy would be inventing a name.
 *
 * THE BASE IS PINNED SIGNED, AND THAT IS MEASURED.  `recivedCPtag`,
 * `recivedSUVtag` and `recivedE2u` all dispatch with `cmp $0x5,%eax ; je ;
 * jl` -- a SIGNED `jl`.  An enumeration whose enumerators are all
 * non-negative gets an unsigned base and GCC emits `jb` there instead, which
 * would be a codegen difference at three sites.  One negative enumerator
 * makes the base signed and every `int` representable.  The pin is ours and
 * the object names no such value; `__tHardwareCodecTypes___BASE_PIN` in
 * V90CodecType.h and `P4D_STATE_BASE_PIN` in V90Phase4Demodulator.h are the
 * precedents, and the reason is written out in full in the first of those.
 * ---------------------------------------------------------------------------
 */
enum Phase4ModulatorState {
	P4M_STATE_RI = 0x00,		/* `exitRi` acts only on 0         */
	P4M_STATE_UNNAMED_01 = 0x01,	/* `exitRi`'s non-boundary arm     */
	P4M_STATE_RI_NOT = 0x02,	/* "enter RiNot @ %d"              */
	P4M_STATE_TRN2D = 0x03,		/* "enter TRN2d @ %d"              */
	P4M_STATE_MP = 0x04,		/* `exitMP` acts only on 4         */
	P4M_STATE_SUVD = 0x05,		/* "enter SUVd @ %d"; see above    */
	P4M_STATE_UNNAMED_06 = 0x06,	/* `recivedSUV`'s non-boundary arm */
	P4M_STATE_CPD = 0x07,		/* "enter CPd @ %d"                */
	P4M_STATE_REPEATED_CPD = 0x08,	/* `enterRepeatedCPd` assigns it   */
	P4M_STATE_UNNAMED_09 = 0x09,	/* from CPd, not yet on a boundary */
	P4M_STATE_UNNAMED_0A = 0x0a,	/* from SUVd, likewise             */
	P4M_STATE_FINAL_SUVD = 0x0b,	/* "enter FinalSUVd @ %d"          */
	P4M_STATE_UNNAMED_0C = 0x0c,	/* `recivedCPtag`'s non-boundary   */
	P4M_STATE_UNNAMED_0D = 0x0d,	/* `exitMP`'s non-boundary arm     */
	P4M_STATE_MP_NOT = 0x0e,	/* "enter MPNot @ %d"              */
	P4M_STATE_UNNAMED_0F = 0x0f,	/* `exitMPNot`'s non-boundary arm  */
	P4M_STATE_ED = 0x10,		/* "enter Ed @ %d"                 */
	P4M_STATE_B1D = 0x11,		/* "enter B1d @ %d"                */
	P4M_STATE_TERMINATED = 0x12,	/* "Phase4 Terminated @ %d"        */
	P4M_STATE_UNNAMED_13 = 0x13,	/* both symbol pumps, no message   */
	/*
	 * 0x14  Both pumps dispatch it to `generateDataSymbolBeforeRRN`, and
	 * that member is where the "enter Rd @ %d" message and the move to
	 * 0x15 live.  Naming the state for the member would be one inferential
	 * step past what the dispatch proves, so it keeps the offset (3120).
	 */
	P4M_STATE_UNNAMED_14 = 0x14,
	P4M_STATE_RD = 0x15,		/* "enter Rd @ %d"                 */
	P4M_STATE_RD_NOT = 0x16,	/* "enter RdNot @ %d"              */
	P4M_STATE_UNNAMED_17 = 0x17,	/* tested by `exitSilence`, never  */
	P4M_STATE_UNNAMED_18 = 0x18,	/*   assigned anywhere in the class*/
	P4M_STATE_UNNAMED_19 = 0x19,	/* `exitSilence`'s non-boundary arm*/
	P4M_STATE_RT = 0x1a,		/* "enter Rt @ %d"                 */
	P4M_STATE_RT_NOT = 0x1b,	/* "enter RtNot @ %d"              */
	/*
	 * 0x1c  `generateV92Symbol` alone, and the V.90 pump's table stops one
	 * short of it.  It dispatches to `generateDataSymbolBeforeFPE`, which
	 * carries the "enter Rf @ %d" message and the move to 0x1d.  Same
	 * reading and same restraint as 0x14 above.
	 */
	P4M_STATE_UNNAMED_1C = 0x1c,
	P4M_STATE_RF = 0x1d,		/* "enter Rf @ %d"                 */
	P4M_STATE_RF_NOT = 0x1e,	/* "enter RfNot @ %d"              */
	P4M_STATE_BASE_PIN = -0x7fffffff - 1	/* ours; pins the base     */
};

typedef char v90p4m_state_is_signed[
    ((Phase4ModulatorState)-1 < (Phase4ModulatorState)0) ? 1 : -1];

/*
 * The scrambled-bit buffer's extent: +0x0078 up to the first field of the
 * tail.  Numerically identical to `V90CP_BITS`, and spelled here rather than
 * shared because this header must not include `V90CP.h` -- V90CP is a forward
 * declaration above and stays one.  If the two ever disagree, this one is
 * wrong: this buffer receives a scrambled copy of `V90CP::bits`.
 */
#define V90P4M_BITS		0x2ee0	/* +0x0078 .. +0x2f57, 12,000 bytes */

/*
 * The two symbol tables' lengths, and they are MEASURED, not counted off the
 * stores.  `generateRdRt` and `generateRdRtNot` index +0x2f68 with
 * `(symbolCounter - 1) % 6` -- `mov $0xaaaaaaab`, `mul`, `shr $0x2`, then
 * `lea (%edx,%edx,2)` and `add %edx,%edx` for the multiply-back by six.
 * `generateRf` and `generateRfNot` index +0x2f74 the same way with `shr $0x3`
 * and `shl $0x2`, which is twelve.  `setRdRtSymbols` writes exactly six
 * shorts and `setRfSymbols` exactly twelve, and 6 * 2 lands the second array
 * on +0x2f74 exactly.
 */
#define V90P4M_RDRT_SYMBOLS	6
#define V90P4M_RF_SYMBOLS	12

class V90Phase4Modulator {
public:
	/* Defined in src/pump/v90/V90Phase4Modulator.cpp. */
	V90Phase4Modulator(V90Parameters *params, unsigned int sessionFlag,
			   V90BitsToSymbol *bitsToSymbol, V90MP *mp,
			   V90MappingParams *mappingParams,
			   V90MappingParams *mappingParams2, V90CP *cp,
			   unsigned int ctorArg8);
	~V90Phase4Modulator();
	void setSessionFlag(unsigned int);

	/*
	 * `reset` -- .text+0x2f630, 255 bytes.  FOUR OF THE FIVE ARGUMENT
	 * TYPES ARE THE MANGLING'S
	 * (`...5resetE7PcmTypeh20Phase4ModulatorStatejj`), and `void` is the
	 * return because neither exit sets `%eax`.
	 *
	 * THE FOURTH ARGUMENT IS A TRIP COUNT AND REACHES NO FIELD.  It is
	 * the bound of a loop that calls `generateV92Symbol` or
	 * `generateV90Symbol` -- chosen by `sessionFlag`, RELOADED from the
	 * object on every iteration, because either callee may move it.  The
	 * fifth lands in `word_0040` and is not otherwise touched.
	 */
	void reset(PcmType law, unsigned char code, Phase4ModulatorState st,
		   unsigned int nofSymbols, unsigned int arg5);

	/*
	 * The twenty-eight members of src/pump/v90/V90Phase4Modulator.cpp's
	 * second half.  Six read a symbol out of a table, two fill those
	 * tables, and the rest are the state machine's edges: an `exitX`
	 * leaves a state when the symbol counter reaches the end of a
	 * repetition, and a `recivedX` is the demodulator's news arriving.
	 */
	short generateRdRt();
	short generateRdRtNot();
	short generateRf();
	short generateRfNot();
	short generateRi();
	short generateRiNot();
	short generateDataSymbolBeforeFPE();
	short generateDataSymbolBeforeRRN();

	/*
	 * The two symbol pumps.  One `switch` over `state` each, and the
	 * source is in src/pump/v90/V90Phase4Modulator.cpp with the jump
	 * tables' addresses and what they prove about the case labels.
	 */
	short generateV90Symbol();
	short generateV92Symbol();

	/*
	 * `generateSymbol` -- .text+0x2f600, 45 bytes.  The dispatcher over
	 * the two above, and `sessionFlag` at +0x0000 is the whole body:
	 * nonzero takes V.92, zero takes V.90.  It is the same fork `reset`
	 * makes for its warm-up loop, so the two agree on which pump this
	 * object drives.
	 *
	 * `int` AND NOT `short`, from the `cwtl` at 0x2f615 and 0x2f628: the
	 * callees are declared `short` above, so the widening is the RETURN
	 * conversion and not a leftover.  `V90Phase3Modulator::generateSymbol`
	 * is the same shape and reads the same way -- and unlike that one,
	 * this pair is instruction-exact, because that header declares its own
	 * two pumps `int` and so emits the extension twice.
	 */
	int generateSymbol();

	void setRdRtSymbols(V90MappingParams *);
	void setRfSymbols(V90MappingParams *);
	void setNextStateAfterTRN2d(Phase4ModulatorState);

	/*
	 * `setMappingParams` -- 96 bytes at .text+0x2d120, and `void`
	 * BECAUSE THE OBJECT TAIL-JUMPS OUT OF IT.  Its last act on the live
	 * path is `jmp V90BitsToSymbol::setSymbolsBlockSize`, whose answer it
	 * therefore returns by accident, and its null path `ret`s with %eax
	 * holding whatever was in it.  A function returning a value would
	 * have to agree with itself across the two and this one does not --
	 * the same reading `V90Mapper::process` records for its own epilogue.
	 *
	 * IT DOES NOT STORE THE ARGUMENT ANYWHERE.  `mappingParams` at +0x4c
	 * and `mappingParams2` at +0x50 are the constructor's and are left
	 * alone; the block is passed through to the converter and forgotten.
	 */
	void setMappingParams(V90MappingParams *);

	void resetBeforRRN();
	void resetRRNSecondSection();
	void enterRepeatedCPd();

	void exitMP();
	void exitMPNot();
	void exitRi();
	void exitSilence();

	void recivedCP();
	void recivedCPtag();
	void recivedE2u();
	void recivedFirstRrnE2u();
	void recivedFirstSUVuPartTwoRrn();
	void recivedPartOneSilenceRrnSUV();
	void recivedPartOneSilenceRrnSUVtag();
	void recivedPartTwoSilenceRrnSUV();
	void recivedSUV();
	void recivedSUVtag();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int sessionFlag;		/* +0x0000 argument 2      */

	/*
	 * +0x0004  The phase 4 state.  `reset`'s THIRD ARGUMENT lands here --
	 * `mov 0x2c(%esp),%edx ; mov %edx,0x4(%esi)` at .text+0x2f647 and
	 * +0x2f65e -- and the mangling types that argument
	 * `Phase4ModulatorState`, so the field's type is the object's own and
	 * not an inference from the constants stored into it.
	 */
	Phase4ModulatorState state;

	/*
	 * +0x0008  Symbols emitted since the current state was entered.  It
	 * is what every one of the class's "@ %d" messages prints, it is set
	 * to 0 beside almost every assignment to `state`, and it is the
	 * dividend of the fifteen `divl` sites that ask
	 * `symbolCount % cpSequenceSymbols`.  `divl` and not `idivl` is what
	 * makes it unsigned.
	 */
	unsigned int symbolCount;

	/*
	 * +0x000c  WRITTEN BY THREE MEMBERS AND READ BY NONE OF THE
	 * FORTY-FIVE.  `reset` zeroes it; both symbol pumps zero it on entry,
	 * before the state is dispatched, and then set it to 7 on the
	 * "Phase4 Terminated" arm -- and `generateV92Symbol` alone sets it to
	 * 4 on both exits from TRN2d.  Four `movl` sites over the class's
	 * whole extent and not one `0xc(%` load anywhere in
	 * .text+0x2c5a0..+0x2f730, so what reads it is outside this class and
	 * the meaning is not established here.  `unsigned int` is the stores'
	 * width; the name stays the offset's.  It was `pad_000c[4]` until the
	 * pumps were written.
	 *
	 * THE READER IS NOW KNOWN AND IS STILL OUTSIDE THIS CLASS:
	 * `V90Modulator::progress` copies it into `V90Modulator::eventCode`
	 * after every `generateSymbol`, ignoring zero, and acts on the 7 --
	 * phase 4 terminated, so enter the data phase.  The name stays here
	 * because `V90Modulator` is where the value is INTERPRETED, and one
	 * caller reading one value does not establish what the other three
	 * stores mean.  Finding 7520.
	 */
	unsigned int word_000c;

	/*
	 * +0x0010  `setNextStateAfterTRN2d`'s whole body is `mov %edx,0x10
	 * (%eax)`, so the field is named for the method that sets it and the
	 * type is that method's argument type.  `reset` seeds it from
	 * `sessionFlag`: `cmp $0x1,%edx ; sbb %eax,%eax ; add $0x5,%eax`,
	 * which is 4 when the flag is zero and 5 when it is not -- MP under
	 * V.90 and SUVd under V.92, the two states TRN2d hands on to.
	 */
	Phase4ModulatorState nextStateAfterTRN2d;

	unsigned char byte_0014;		/* +0x0014 `movb $0x0` in  */
	unsigned char pad_0015[3];		/*   reset; nothing else   */

	/*
	 * +0x0018 and +0x001c  Written together and only ever to zero, by
	 * `reset`, `resetRRNSecondSection`, `enterRepeatedCPd`,
	 * `recivedCPtag` and `recivedSUVtag`; read by neither those nor
	 * anything else in this file.  The widths are the stores' -- `movl`
	 * and `movb`.  Nothing establishes a meaning, so they keep offset
	 * names.
	 */
	unsigned int word_0018;
	unsigned char byte_001c;
	unsigned char pad_001d[3];

	/*
	 * +0x0020  Set to 1 on every path of `recivedCPtag`, `recivedE2u`,
	 * `recivedFirstRrnE2u`, `recivedPartOneSilenceRrnSUVtag` and
	 * `recivedSUVtag` that moves `state`, and tested at the top of the
	 * first four as a reason to do nothing.  So it latches "this has
	 * already been acted on"; `reset`, `resetBeforRRN` and
	 * `resetRRNSecondSection` clear it.  That bounds the role without
	 * establishing it -- nothing in the object says what "this" is -- so
	 * the name stays the offset's.
	 */
	unsigned int word_0020;

	/*
	 * +0x0024 .. +0x0034  `resetBeforRRN` writes all six (1 into +0x24,
	 * zero into the rest) and `reset` writes four of them; the reads are
	 * in members this batch has not written.  Offset names, `movl` widths.
	 */
	unsigned int word_0024;
	unsigned int word_0028;
	unsigned int word_002c;
	unsigned int word_0030;
	unsigned int word_0034;

	/*
	 * +0x0038  The companding law.  `reset`'s FIRST argument lands here
	 * (`mov 0x24(%esp),%eax ; mov %eax,0x38(%esi)`) and the mangling
	 * types it `PcmType`, the enumeration V90Phase3Modulator.h defines.
	 * Every read in this class is the same `!= 0` choosing A-law, which
	 * is the sense that header already measured.
	 */
	PcmType pcmType;

	/*
	 * +0x003c  The linear level of `reset`'s second argument, an
	 * `unsigned char` G.711 code: `reset` runs it through
	 * `alaw2linear((code & 0x7f) ^ 0xd5)` or
	 * `ulaw2linear((code & 0x7f) ^ 0xff)` and stores 16 bits of the
	 * result.  `generateRi` returns it and `generateRiNot` returns its
	 * negation, both through `movswl`, which is what makes it signed.
	 * The name is `V90Phase3Modulator::codeLevel`'s, for the same field
	 * filled the same way by that class's own `reset`.
	 */
	short codeLevel;
	unsigned char pad_003e[2];

	/*
	 * +0x0040  `reset`'s FIFTH argument, stored and not otherwise touched
	 * by anything this batch wrote.  `unsigned int` is the mangling's
	 * (`...20Phase4ModulatorStatejj`); the meaning is not established.
	 */
	unsigned int word_0040;

	V90BitsToSymbol *bitsToSymbol;		/* +0x0044 argument 3      */
	V90MP *mp;				/* +0x0048 argument 4      */
	V90MappingParams *mappingParams;	/* +0x004c argument 5      */
	V90MappingParams *mappingParams2;	/* +0x0050 argument 6      */
	V90CP *cp;				/* +0x0054 argument 7      */
	Scrambler<unsigned char, unsigned char> scrambler;
						/* +0x0058 32 bytes        */
	/*
	 * +0x0078  The scrambler's output and the bits-to-symbol converter's
	 * input, one byte per bit.  See V90P4M_BITS above for why the length
	 * is the whole span to +0x2f58 and what that claim rests on.
	 */
	unsigned char scrambledBits[V90P4M_BITS];

	/*
	 * +0x2f58  What `V90MP::getBitVector` returned.  Borrowed: it points
	 * into the `V90MP` at +0x0048 and nothing here frees it.
	 */
	unsigned char *mpBits;

	/*
	 * +0x2f5c  The length `V90MP::getBitVector` reported.  `unsigned int`
	 * is the mangling's -- `_ZN5V90MP12getBitVectorERj` takes `unsigned
	 * int &` -- and this field is what the reference is bound to
	 * (`lea 0x2f5c(%ebx),%ecx` at +0x2d0db).
	 */
	unsigned int mpBitCount;

	/*
	 * +0x2f60  `6 * mpBitCount / mp->groupSize`, the MP sequence's length
	 * in symbols.  Read only as `symbolCounter % mpSequenceSymbols` in
	 * `exitMP`, `exitMPNot` and `generateV90Symbol`; `divl` makes it
	 * unsigned.
	 */
	unsigned int mpSequenceSymbols;

	/*
	 * +0x2f64  Set to `bitsToSymbol->extraSymbols + 12` on entry to state
	 * 0x10 -- the state whose debug line is "enter Ed @ %d" -- by
	 * `exitMPNot` (+0x2c96b), `recivedCPtag` (+0x2cd28), `recivedE2u`
	 * (+0x2cdf7) and both symbol pumps, and read only as
	 * `symbolCounter == word_2f64`, which ends that state.  A deadline in
	 * symbols; what the twelve is is not established, so the name stays
	 * neutral.
	 */
	unsigned int word_2f64;

	/*
	 * +0x2f68  Six symbols, indexed `(symbolCounter - 1) % 6`.
	 * `setRdRtSymbols` fills them from a `V90MappingParams` through
	 * `alaw2linear`/`ulaw2linear`, negating the last three, and
	 * `generateRdRt`/`generateRdRtNot` return one.  `short` is forced:
	 * the loads are `movswl` whose 32-bit result is the return value
	 * (finding 613's case), and the `neg %eax` before the stores to
	 * +0x2f6e, +0x2f70 and +0x2f72 puts negative values in them.
	 */
	short rdRtSymbols[V90P4M_RDRT_SYMBOLS];

	/*
	 * +0x2f74  Twelve symbols, indexed `(symbolCounter - 1) % 12`, filled
	 * by `setRfSymbols` and read by `generateRf`/`generateRfNot`.  Same
	 * two proofs of the signed type.
	 */
	short rfSymbols[V90P4M_RF_SYMBOLS];

	/*
	 * +0x2f8c  What `V90CP::getBitVector` returned; the CP half of the
	 * same triple.  Borrowed from the `V90CP` at +0x0054.
	 */
	unsigned char *cpBits;

	/* +0x2f90  The length it reported; the reference's home. */
	unsigned int cpBitCount;

	/*
	 * +0x2f94  `6 * cpBitCount / cp->word_3ba8`, the CP sequence's length
	 * in symbols, and the most-read field in the class: fifteen `divl`
	 * sites take `symbolCounter % cpSequenceSymbols`.
	 */
	unsigned int cpSequenceSymbols;

	unsigned int ctorArg8;			/* +0x2f98 argument 8      */

	/*
	 * +0x2f9c  Zeroed by the constructor, by `reset`, by `resetBeforRRN`
	 * and by `resetRRNSecondSection`; set to 1 by `recivedCP`, and by
	 * `recivedCPtag` on the one path where it was still zero -- the path
	 * that also sets `cp->word_00` and the CP byte at +0x13 and rebuilds
	 * the CP sequence.  Read as a gate by `recivedCPtag` and
	 * `recivedSUVtag`.  A latch, and the object does not say for what, so
	 * it keeps the offset name; it was `cleared_2f9c`, which recorded
	 * only that the constructor cleared it.
	 */
	unsigned int word_2f9c;

	/*
	 * +0x2fa0  The same shape one step later: cleared by the same four,
	 * set to 1 by `recivedSUV`, `recivedPartTwoSilenceRrnSUV` and
	 * `recivedFirstSUVuPartTwoRrn` immediately after each has called
	 * `V90CP::infoToBits` and recomputed `cpSequenceSymbols`, and read by
	 * the last two as a reason not to do that twice.  Was `cleared_2fa0`.
	 */
	unsigned int word_2fa0;
	unsigned int externalBitsToSymbol;	/* +0x2fa4 1 = not ours    */
	V90Parameters *params;			/* +0x2fa8 argument 1      */
};

#endif /* DSPLIB_V90PHASE4MODULATOR_H */
