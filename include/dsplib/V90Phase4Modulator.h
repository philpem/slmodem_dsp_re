/*
 * V90Phase4Modulator.h -- the V.90 / V.92 phase 4 downstream symbol source.
 *
 * Reconstructed from dsplibs.o.  Forty-three members and 12,078 bytes of
 * code, of which THREE are written here: the constructor, the destructor and
 * `setSessionFlag`, the only member of the class `v34handshak` reaches.
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
 * being the last -- there are exactly eleven distinct memory displacements in
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

class V90BitsToSymbol;
class V90CP;
class V90MP;
class V90MappingParams;
class V90Parameters;

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

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int sessionFlag;		/* +0x0000 argument 2      */
	unsigned char pad_0004[0x40];		/* +0x0004 not modelled    */
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
	unsigned int cleared_2f9c;		/* +0x2f9c zeroed by ctor  */
	unsigned int cleared_2fa0;		/* +0x2fa0 zeroed by ctor  */
	unsigned int externalBitsToSymbol;	/* +0x2fa4 1 = not ours    */
	V90Parameters *params;			/* +0x2fa8 argument 1      */
};

#endif /* DSPLIB_V90PHASE4MODULATOR_H */
