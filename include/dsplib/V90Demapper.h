/*
 * V90Demapper.h -- the V.90 demapper, so far as its DESTRUCTOR PATH needs it.
 *
 * Reconstructed from dsplibs.o.  The class had no header and no .cpp in this
 * tree before this file; the only mention of the name anywhere was one
 * sentence in `V90Demodulator.h`.  `.symtab` carries `V90Demapper.cpp` as a
 * FILE entry, so the original had a translation unit of its own.
 *
 * THE OBJECT IS 0x1eb8 = 7,864 BYTES, AND THAT IS AN ALLOCATION.  Finding
 * 1107's rule -- prefer the `sysdep_malloc` immediately before the
 * constructor's call site, because a displacement scan is bounded by the
 * symbol set it covered and cost `V90Equalizer` eight bytes.  Both call
 * sites agree:
 *
 *     1c4d9:  c7 04 24 b8 1e 00 00   movl  $0x1eb8,(%esp)
 *     1c4e0:  e8 ..                  call  sysdep_malloc
 *     1c4e5:  89 c6                  mov   %eax,%esi         <- the demapper
 *     ...
 *     1c503:  e8 ..                  call  V90Demapper::V90Demapper(
 *                                            unsigned, V90Parameters *,
 *                                            V90AutoDigitalImpDetector *)
 *     1c508:  89 b3 e4 01 00 00      mov   %esi,0x1e4(%ebx)
 *
 * inside `V90Demodulator::V90Demodulator`, and the same three instructions at
 * 0x1c8c9/0x1c8d0/0x1c8f3 inside its C2 twin.  The highest offset anything
 * written here touches is +0x1e90, so the last 0x24 bytes are bounded by the
 * allocation alone -- which is exactly the case the rule exists for.
 *
 * NOT POLYMORPHIC.  `nm` gives `D1` at 0x30f50 and `D2` at 0x30fd0 and no
 * `D0`; GCC emits a deleting destructor only for a virtual class, so offset 0
 * is a real member and there is no vptr (finding 228).  The two destructors
 * are byte-for-byte the same 123 bytes.
 *
 * THE FOUR PARALLEL ARRAYS ARE [6][128] AND THE SHAPE IS READ, NOT ASSUMED.
 * `printErrorHistogramAndReset` runs `i` over 0..5 (`cmpl $0x5,0x18(%esp)`
 * with `jbe`) and, for each `i`, `j` over 0..127 (`cmp $0x7f,%edx` with
 * `jbe`), addressing `0x690(%ebx,%edi,4)` and `0x1290(%ebx,%edi,4)` with
 * `edi = i * 128 + j` -- `shl $0x7` on `i`, then `inc %edi` per step.  So
 * each array is 6 * 128 * 4 = 3,072 bytes: 0x690..0x1290 and 0x1290..0x1e90,
 * which meet exactly.  The third array, at +0x30, is addressed
 * `0x30(%ebx,%edi,2)` by the same index, so it is 6 * 128 * 2 = 1,536 bytes,
 * 0x30..0x630 -- and 0x630 is where the six per-constellation counts start.
 * Four independent bases and three exact meetings.
 *
 * TWO THINGS THE COMPILER WAS FORCED TO ENCODE, so they are acted on
 * (CLAUDE.md's FORCED vs FREE rule):
 *
 *   - `0f bf 54 7b 30   movswl 0x30(%ebx,%edi,2),%edx` at 0x30c69 SIGN-extends
 *     and hands the 32-bit result to a varargs slot, so the constellation
 *     array is `short` and not `unsigned short`.  This is finding 613's class
 *     of defect -- invisible to every differential test whose values stay
 *     positive, visible in one instruction.
 *   - `f7 f1   div %ecx` at 0x30c57, not `idiv`, and no signed-division
 *     fixup anywhere near it: both histogram arrays are UNSIGNED.  The count
 *     comparison is the same story, `cmp %ebp,0x630(...)` followed by `ja`.
 */

#ifndef DSPLIB_V90DEMAPPER_H
#define DSPLIB_V90DEMAPPER_H

#include "dsplib/ModulusCoder.h"
#include "dsplib/V90SignBitsExtractor.h"

/*
 * POINTERS ONLY, so forward declarations are what belong here.  Two
 * incompatible definitions of `V90Parameters` exist in this tree and no
 * translation unit may include both -- finding 1112 -- so the header declares
 * and `V90Demapper.cpp` picks the NAMED 0x558 map, because
 * `DEBUG_DEMAPPER_ERROR_HISTOGRAM` is the author's own name for the field the
 * destructor branches on.
 */
class V90Parameters;
class V90AutoDigitalImpDetector;
class V90MappingParams;

/* The two dimensions of the four parallel arrays; see the file comment. */
#define V90DEMAPPER_CONSTELLATIONS	6
#define V90DEMAPPER_LEVELS		128

/*
 * The samples in one V.90 frame, and a SEPARATE constant from the count above
 * even though both are six.  `process` steps its cursor by it (`lea 0x6(%edx)`
 * at 0x31750) and refuses to run below it (`cmp $0x5,%ebx; jbe`); the six
 * above is how many constellations the RBS cycle holds.  They are the same
 * six for the same reason -- one sample per RBS position -- but a reader who
 * saw only one macro would not know that either use had been checked.
 */
#define V90DEMAPPER_FRAME		6u

class V90Demapper {
public:
	/*
	 * The three members this tree defines.  The mangled names are
	 * `_ZN11V90Demapper27printErrorHistogramAndResetEv`,
	 * `_ZN11V90DemapperD1Ev` and `_ZN11V90DemapperD2Ev`.
	 */
	void printErrorHistogramAndReset();
	~V90Demapper();

	/*
	 * THE CONSTRUCTOR IS NOW DEFINED, and what unblocked it was
	 * `ModulusDecoder` being written (finding 1247).  The comment this
	 * replaces said the class "has no header, no .cpp and no other symbol
	 * in this tree", which was true and is not any more:
	 * `include/dsplib/ModulusCoder.h` declares it, its default constructor
	 * is defined, and `sizeof(ModulusDecoder)` is 0x1c -- exactly the
	 * distance the pad measured.  So the member below is the real class
	 * and the constructor's first act is the compiler's, not ours.
	 */
	V90Demapper(unsigned int levels, V90Parameters *params,
		    V90AutoDigitalImpDetector *adi);

	/*
	 * Three more of the processing half, written.  A RETURN TYPE IS NOT
	 * MANGLED, so the two that return something are read off the epilogue
	 * and not off the name:
	 *
	 *   `hardDecision`  ends `movswl 0x20(%esp),%eax`, so a `short` --
	 *                   the 16-bit truncation is in the object and a
	 *                   wider return type would not have it.
	 *   `process`       ends `mov $0x1,%eax` on one path and
	 *                   `xor %eax,%eax` on the other, which is `int` or
	 *                   `bool` and the object cannot tell them apart.
	 *                   `int` is the reading here; nothing depends on it.
	 */
	short hardDecision(short in);
	int process(unsigned char *out, unsigned int &nbits);
	void resetLinearMappStudy(unsigned int);

	/*
	 * FOUR MORE, WRITTEN.  All four return nothing and the object says so
	 * -- each ends on a bare `ret` or a tail `jmp` with `%eax` never set
	 * on any path -- which is as much as an unmangled return type can be
	 * pinned to.  `updateConstelation` keeps the blob's own spelling, one
	 * `l`, because the SYMBOL is spelt that way.
	 */
	void resetNoSpectral(V90MappingParams *);
	void incrementRBSFramePosition();
	void linearMappingStudy(short sample, short level);
	void updateConstelation();

	/*
	 * THE ONE STILL DECLARED AND DELIBERATELY UNDEFINED.  Its signature
	 * comes from the mangling, so this is a specification and not a
	 * guess.  It is not unwritten for want of reading: its closure
	 * contains `V90SignBitsExtractor::reset`, which nothing in this tree
	 * has, and one unwritten callee fails EVERY differential binary at
	 * `t_encode` rather than only its own (finding 215).
	 */
	void reset(V90MappingParams *);

	/*
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable from the mangling, and a
	 * single access section is what lets the .cpp assert every offset
	 * below with __builtin_offsetof.
	 */

	/*
	 * +0x00  The parameter block.  The constructor's third stack argument,
	 * stored last (`mov 0x18(%esp),%ecx; mov %ecx,(%esi)`), and the
	 * destructor's only dereference: `mov (%ebx),%edx` then
	 * `mov 0x52c(%edx),%eax`, which is `DEBUG_DEMAPPER_ERROR_HISTOGRAM`.
	 * Not owned.
	 */
	V90Parameters *params;

	/*
	 * +0x04 .. +0x18  Six words the constructor zeroes with six
	 * consecutive `movl $0x0`.  Five of the six are named from `process`
	 * and `hardDecision`; the sixth is read by nothing this tree has.
	 *
	 * THE FRAME IS SIX SAMPLES AND THE ARITHMETIC BELOW ALL HANGS OFF
	 * THAT.  `process` consumes samples six at a time (`lea 0x6(%edx)`,
	 * loop guard `frameStart + 6 <= sampleCount`), and one such frame
	 * yields `bitsPerFrame` bits laid out as `signBitsPerFrame` sign bits
	 * followed by whatever `ModulusDecoder::progress` writes after them.
	 */

	/*
	 * +0x04  Bits produced per six-sample frame.  `process` adds it to its
	 * running total once per frame and returns that total through its
	 * reference argument, and it is also the stride of the output buffer.
	 */
	unsigned int bitsPerFrame;

	/*
	 * +0x08  MODELLED, UNNAMED, AND THE "READ BY NOTHING" SENTENCE THAT
	 * STOOD HERE IS RETRACTED -- `resetNoSpectral` both computes it and
	 * reads it back:
	 *
	 *     30d09:  mov  0xc(%ecx),%ebp          <- signBitsPerFrame
	 *     30d0e:  mov  %ebx,0x4(%ecx)          <- bitsPerFrame = mapp->[0]
	 *     30d11:  sub  %ebp,%ebx
	 *     30d13:  mov  %ebx,0x8(%ecx)          <- HERE
	 *     ...
	 *     30e77:  mov  %esi,0x18(%eax)         <- modulusDecoder's +0x18
	 *
	 * So it is `bitsPerFrame - signBitsPerFrame` and it is the seventh
	 * word handed to the embedded `ModulusDecoder`.  Both halves of that
	 * are read straight off the object.
	 *
	 * IT IS STILL NOT NAMED, and that is a decision rather than an
	 * omission.  The arithmetic makes "the bits of a frame that are NOT
	 * sign bits" certain; calling it the MODULUS bit count additionally
	 * assumes what `ModulusDecoder` does with its seventh word, and that
	 * class's seven members are all `field_NN` because nothing in the
	 * object names them either.  Finding 3120's rule -- a wrong name is
	 * believed by every future reader and no test can fail on it -- so
	 * the derivation goes here and the name waits for
	 * `ModulusDecoder::progress`.
	 */
	unsigned int word_08;

	/*
	 * +0x0c  How many sign bits a frame carries, and therefore where the
	 * modulus bytes start: `process` hands `ModulusDecoder::progress` the
	 * output pointer already advanced by it.  It is the loop bound of the
	 * serial-decoder path too, so the two readings agree.
	 */
	unsigned int signBitsPerFrame;

	/*
	 * +0x10  How many sign-bit GROUPS a frame is cut into, and the switch
	 * between this class's two sign-bit paths: zero means the whole frame
	 * goes through the serial decoder at +0x664, non-zero means it is cut
	 * into this many groups and each goes through `signBits`.
	 *
	 * It is `V90SignBitsExtractor::reset`'s `spacing`: that member sets the
	 * extractor's width to `6 / spacing`, and `groups * groupSize == 6`
	 * below is what makes the two consistent.
	 */
	unsigned int signBitGroups;

	/*
	 * +0x14  Samples per group -- the extractor's `width`.  Each group
	 * consumes this many samples of `signs` and produces `width - 1` bits,
	 * which is why `process` strides its input by it and its output by one
	 * less.
	 */
	unsigned int signBitGroupSize;

	/*
	 * +0x18  THE RBS FRAME POSITION, 0..5, and the name is the author's:
	 * `incrementRBSFramePosition` is a symbol of this class.
	 * `hardDecision` selects the constellation with it, advances it
	 * `(pos + 1) % 6` with an UNSIGNED division by six
	 * (`mul $0xaaaaaaab; shr $0x2`), and copies its pre-advance value to
	 * +0x1eae.
	 *
	 * `incrementRBSFramePosition` IS THE SAME ADVANCE AGAIN, all 33 bytes
	 * of it, and `hardDecision` does NOT call it -- it repeats it inline.
	 * So the member exists for a caller outside this class, and the two
	 * copies are a second, independent witness that the division is
	 * unsigned.
	 */
	unsigned int rbsFramePosition;

	/*
	 * +0x1c and +0x20  THE TWO HEAP BLOCKS THE DESTRUCTOR FREES, each
	 * behind its own `test`/`jne`, and each sized from the constructor's
	 * first argument:
	 *
	 *     30674:  8d 04 9d 00 00 00 00  lea  0x0(,%ebx,4),%eax
	 *     30684:  e8 ..                 call sysdep_malloc     ->  +0x1c
	 *     3068c:  89 1c 24              mov  %ebx,(%esp)
	 *     3068f:  e8 ..                 call sysdep_malloc     ->  +0x20
	 *     30699:  89 5e 24              mov  %ebx,0x24(%esi)
	 *
	 * so +0x1c holds `sampleCapacity` elements of four bytes and +0x20
	 * holds `sampleCapacity` of one.
	 *
	 * THE ELEMENT TYPES ARE NOW FORCED AND THE `void *` IS GONE.  That
	 * comment said they would stay `void *` "until something that reads
	 * them is written", and `process` is it: +0x1c is passed to
	 * `ModulusDecoder::progress(unsigned char *, unsigned int *)` as its
	 * second operand, and +0x20 to
	 * `V90SignBitsExtractor::process(unsigned char *, unsigned char *)`.
	 * Both types come from a mangling, which is the second-strongest kind
	 * of evidence there is here.
	 *
	 * WHAT THEY HOLD, one entry per sample decided by `hardDecision`:
	 * `codes` the constellation index it chose, `signs` a 1 for a sample
	 * that was positive and a 0 for one that was negative.  "Code" is the
	 * author's word for a constellation index -- `clearCamulativeVal(short
	 * phase, short code)` and `setMaxUcodeArray` are
	 * `V90AutoDigitalImpDetector`'s, over the same 0..127 index.
	 *
	 * Both are OWNED: the destructor frees them.
	 */
	unsigned int *codes;
	unsigned char *signs;

	/*
	 * +0x24  The element count both allocations were sized from, and the
	 * ceiling `hardDecision` refuses to write past -- `cmp 0x24(%ebx),%edx`
	 * with `jae` to the diagnostic.
	 */
	unsigned int sampleCapacity;

	/*
	 * +0x28  Where the frame `process` is working on starts.  Zero at
	 * construction, advanced by six per frame, and reset to zero when
	 * `process` compacts what is left down to the front.
	 */
	unsigned int frameStart;

	/*
	 * +0x2c  How many samples `codes` and `signs` hold.  `hardDecision`
	 * appends one and increments it; `process` consumes whole frames from
	 * `frameStart` and leaves the remainder here.
	 */
	unsigned int sampleCount;

	/*
	 * +0x30  The six constellations, up to 128 signed PCM levels each.
	 * `short` because the load that feeds `printErrorHistogramAndReset`'s
	 * per-level line is `movswl` and its 32-bit result is used; see the
	 * file comment.  `updateConstelation` fills it with `fistps`.
	 *
	 * TWO OF ITS FIVE USERS INDEX IT OUTSIDE [6][128] AND BOTH ARE THE
	 * OBJECT'S, not this reconstruction's.  `updateConstelation` and
	 * `resetNoSpectral` form `i * 128 + j` with `j` bounded by twice the
	 * row length rather than by 128, so a long enough row runs into the
	 * next one and the sixth runs past the array into `constellationSize`
	 * behind it; `linearMappingStudy` reads `[phase][code - 1]` with the
	 * code at zero, which is two bytes BELOW the row.  `t_v90demap.cpp`
	 * reaches both on purpose.  Anything that re-shapes this array has to
	 * keep the flat arithmetic, because the flattening is what the object
	 * encodes -- there is one `shl $0x7` and an `add`, and no bound.
	 */
	short constellation[V90DEMAPPER_CONSTELLATIONS][V90DEMAPPER_LEVELS];

	/*
	 * +0x630  How many levels each of the six constellations actually
	 * has.  The constructor zeroes all six in a rolled loop
	 * (`cmp $0x5,%eax; jbe`), and `printErrorHistogramAndReset` both
	 * gates on all six being non-zero and uses them as its inner-loop
	 * bound with an UNSIGNED comparison (`ja`).
	 */
	unsigned int constellationSize[V90DEMAPPER_CONSTELLATIONS];

	/*
	 * +0x648 .. +0x663  An embedded `ModulusDecoder`, constructed by
	 * `V90Demapper::V90Demapper` at `lea 0x648(%esi)` and NOT destroyed
	 * by `~V90Demapper` -- which is what says its destructor is trivial,
	 * and `ModulusCoder.h` declares none.  Its seven `unsigned int` fields
	 * are 0x1c bytes, which is exactly the distance to the field below;
	 * the pad this replaces was that distance measured, and the class now
	 * fills it exactly.
	 */
	ModulusDecoder modulusDecoder;

	/*
	 * +0x664  A `SerialDifferentialDecoder<unsigned char>`, ONE BYTE, and
	 * the type is forced by a `this` and not inferred from the store:
	 * `process` does `lea 0x664(%esi),%edx` and passes it as the first
	 * stack argument of `_ZN25SerialDifferentialDecoderIhE7processEh` at
	 * 0x31887.  It was `unsigned char byte_664` while the constructor's
	 * `movb $0x0,0x664(%esi)` was the only thing that touched it.
	 *
	 * DiffCoder.h PREDICTED THIS BEFORE IT WAS READ, and the prediction is
	 * worth keeping because it is what makes the store below evidence
	 * rather than coincidence: the serial coders emit no constructor of
	 * their own, so an enclosing class that value-initialises one gets
	 * exactly one inlined `movb $0x0` where the member sits.
	 *
	 * IT IS A MEM-INITIALIZER AND NOT A BODY STATEMENT, and the position of
	 * that one instruction is the whole argument:
	 *
	 *     3071d:  lea   0x648(%esi),%eax
	 *     30726:  call  ModulusDecoder::ModulusDecoder()
	 *     3072b:  movb  $0x0,0x664(%esi)          <-- here
	 *     30732:  lea   0x668(%esi),%ecx
	 *     3073b:  call  V90SignBitsExtractor::V90SignBitsExtractor()
	 *
	 * A store to `this + 0x664` cannot be sunk past a call to a function
	 * that may alias it, nor hoisted before the one in front of it, so the
	 * compiler was not free to put it there: it is between the two member
	 * constructors because it IS a member construction.  Every body
	 * statement runs after all of them.  That fixes the declaration order
	 * as modulusDecoder, byte_664, signBits and the constructor's list as
	 * `: byte_664(0)`.
	 *
	 * THIS IS A CODEGEN CLAIM AND NOT A DIFFERENTIAL ONE.  The byte holds
	 * zero either way, so `make phase` cannot see the difference.  The
	 * evidence is the instruction above plus `make similarity`, which
	 * lists both `_ZN11V90DemapperC1E...` and `_ZN11V90DemapperC2E...`
	 * among the identical mnemonic sequences -- and a mnemonic sequence is
	 * exactly what a claim about POSITION needs, since where the `movb`
	 * sits relative to the two `call`s is in the sequence even though its
	 * operands are not compared.
	 *
	 * WHAT IT IS FOR: `process`'s other sign-bit path.  With
	 * `signBitGroups` zero the frame does not go through `signBits` at all
	 * and every sign bit goes through this one decoder instead.
	 */
	SerialDifferentialDecoder<unsigned char> signDecoder;
	unsigned char pad_665[3];

	/*
	 * +0x668  The sign-bit extractor, embedded.  The constructor runs its
	 * constructor at `lea 0x668(%esi)` and the destructor ends with the
	 * matching `lea 0x668(%ebx)` and a call to
	 * `V90SignBitsExtractor::~V90SignBitsExtractor` -- which is what the
	 * compiler emits for us, in exactly that position, because it is the
	 * only member with a non-trivial destructor.
	 */
	V90SignBitsExtractor signBits;

	/*
	 * +0x690  Accumulated error per constellation level, and +0x1290 the
	 * number of samples that went into it.  UNSIGNED, because
	 * `printErrorHistogramAndReset` divides the first by the second with
	 * `div` and not `idiv`.  `hardDecision` is what fills them -- `add`
	 * at +0x690 and `incl` at +0x1290, which is how the roles were told
	 * apart -- and `printErrorHistogramAndReset` is what empties them.
	 */
	unsigned int errorSum[V90DEMAPPER_CONSTELLATIONS][V90DEMAPPER_LEVELS];
	unsigned int errorCount[V90DEMAPPER_CONSTELLATIONS][V90DEMAPPER_LEVELS];

	/*
	 * +0x1e90  How many times the histogram has been asked for.  Zeroed
	 * by the constructor, printed by `printErrorHistogramAndReset` as
	 * "Error histogram #%d" BEFORE it is incremented, and incremented on
	 * every call whether anything was printed or not -- the `incl` at
	 * 0x30cd0 is the join point of both arms.
	 */
	unsigned int errorHistogramCount;

	/*
	 * +0x1e94  THE DELAY BEFORE THE HISTOGRAM STARTS FILLING, counted
	 * down.  `hardDecision` tests it and, while it is non-zero, decrements
	 * it and accumulates nothing; when it reaches zero every decision goes
	 * into the histogram.  The parameter block has the author's name for
	 * what seeds it -- `DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM` at +0x530,
	 * beside the two this class does read -- and `reset` is what seeds it.
	 */
	int histogramDelay;

	/*
	 * +0x1e98  HOW LONG THE CURRENT HISTOGRAM HAS BEEN INTEGRATING.
	 * `hardDecision` compares it against
	 * `params->DEMAPPER_ERROR_HISTOGRAM_INTEGRATION_TIME` (+0x534) with a
	 * SIGNED `jge`, which is why both are `int`; on reaching it the
	 * histogram is printed, reset and this goes back to zero, otherwise it
	 * is incremented.
	 */
	int histogramIntegration;

	/*
	 * +0x1e9c  MODELLED, UNNAMED.  `resetLinearMappStudy` clears it with a
	 * 16-bit store; `linearMappingStudy` increments it once per completed
	 * run and compares the result against TWO (`movzwl 0x1e9c(%edx),%ebx;
	 * inc %ebx; cmp $0x2,%bx; mov %bx,0x1e9c(%edx); je`).  The compare is
	 * an equality, so it carries no signedness evidence either.
	 *
	 * RECONSTRUCTING `linearMappingStudy` ADDS THE PART THAT MAKES THE
	 * COUNT MEAN SOMETHING: it is a count of COMPLETED STUDY RUNS -- the
	 * increment is on the path where `uint_1eb0 + 1` reaches `uint_1ea8`
	 * -- and NOTHING IN THAT FUNCTION EVER CLEARS IT.  Only
	 * `resetLinearMappStudy` and `reset` do.  So the `== 2` test fires
	 * exactly once per study, on the second completed run, and what it
	 * does there is raise `short_1ea6` below.  The old comment's "counts
	 * something that happens twice" is right and this is what it counts.
	 */
	short short_1e9c;
	unsigned char pad_1e9e[2];

	/*
	 * +0x1ea0  The automatic digital-impairment detector: the
	 * constructor's fourth stack argument
	 * (`mov 0x1c(%esp),%edx; mov %edx,0x1ea0(%esi)`).  Not owned -- the
	 * destructor does not free it.  Three of the processing members
	 * reach through it at +0x1000, +0x1c00 and +0x2800, which is how it
	 * was told apart from an array of this object's own.
	 */
	V90AutoDigitalImpDetector *adiDetector;

	/*
	 * +0x1ea4 and +0x1ea6  MODELLED, UNNAMED, AND TWO CLAIMS THAT USED TO
	 * STAND HERE ARE WITHDRAWN.  They were that nothing in the object
	 * LOADS either, and that the two stores at 0x315b6 and 0x3170f are
	 * "the two points where the end-of-run pass begins".  Both are wrong.
	 *
	 * THEY ARE READ, BY `V90Equalizer::process`, and the identification is
	 * not a displacement coincidence -- the same function CALLS
	 * `linearMappingStudy` thirty bytes earlier:
	 *
	 *     3a3dd:  call  V90Demapper::linearMappingStudy(short, short)
	 *     ...
	 *     3a3fd:  mov   0x3054(%edx),%ecx
	 *     3a403:  cmpw  $0x0,0x1ea4(%ecx)     ; je  -> skip
	 *     ...
	 *     3a42c:  mov   0x3054(%edx),%eax
	 *     3a432:  cmpw  $0x0,0x1ea6(%eax)     ; je  -> skip
	 *
	 * so +0x3054 of the equaliser's argument is this demapper, each flag
	 * is tested against zero, and each gates a second `cmpw $0x0` on a
	 * flag at +0x144 / +0x146 of another object.  Finding 3531's rule is
	 * why this matters and finding 4342 records it: a claim that NOTHING
	 * reads a field is a claim about every function in the object, and
	 * the way to test it is a displacement grep -- which works here only
	 * because `1ea4` is a rare displacement and would prove nothing for,
	 * say, `0x08`.
	 *
	 * AND THE TWO STORES ARE UNDER DIFFERENT CONDITIONS.  0x315b6 sets
	 * +0x1ea4 on EVERY completed run; 0x3170f sets +0x1ea6 only when
	 * `short_1e9c` reaches two, which is the SECOND completed run and
	 * happens once per study.  So one is "a run has finished" and the
	 * other "a second run has finished" -- bounded, but what the equaliser
	 * does with the distinction is in a function nobody has written, so
	 * the names stay neutral and this comment carries the derivation.
	 */
	short short_1ea4;
	short short_1ea6;

	/*
	 * +0x1ea8 and +0x1eb0  THE LINEAR-MAPPING STUDY'S LENGTH AND ITS
	 * PROGRESS, and the pair is what says which is which:
	 *
	 *     314e2:  mov  0x1eb0(%edx),%eax
	 *     314e8:  inc  %eax
	 *     314e9:  cmp  0x1ea8(%edx),%eax
	 *     314ef:  je   31578            -> the study's end-of-run pass
	 *     314f9:  mov  %eax,0x1eb0(%ebx)
	 *
	 * inside `linearMappingStudy`, against `resetLinearMappStudy` storing
	 * its ONE ARGUMENT at +0x1ea8 and zeroing +0x1eb0.  One is set once
	 * and only read; the other starts at zero, is incremented per call and
	 * is compared against the first.  The comparison is an EQUALITY, so
	 * neither carries signedness evidence and both are `unsigned int` by
	 * the argument's type rather than by the branch.
	 *
	 * `linearMappingStudy` IS NOW RECONSTRUCTED AND BOTH NAMES SURVIVE IT,
	 * with the reading upgraded from the weakest of CLAUDE.md's three
	 * evidence ranks to the function's own behaviour.  The test is
	 * `uint_1eb0 + 1 == uint_1ea8` and the progress is stored ONLY on the
	 * arm that fails it; the arm that passes REWINDS it to zero
	 * (`mov %edx,0x1eb0(%ecx)` with `%edx` zeroed at 0x31599) and runs the
	 * end-of-run pass.  A length that is set once and only read, against a
	 * progress that counts to it and restarts, is exactly what the pair
	 * was named for.
	 */
	unsigned int uint_1ea8;			/* the length  */

	/*
	 * +0x1eac  THE CODE THE LAST `hardDecision` CHOSE, and +0x1eae the RBS
	 * frame position it chose it in -- the position BEFORE the advance,
	 * stored at the very top of the function and therefore recorded even
	 * on the over-capacity arm that decides nothing.  Both are 16-bit
	 * stores and both are written on every path.
	 *
	 * THEY ARE NOT DIAGNOSTICS, and an earlier draft of this comment said
	 * they were on the strength of nothing in the five members this batch
	 * read touching them.  `linearMappingStudy` reads BOTH, at 0x31459 and
	 * 0x31470, and uses them as the (phase, code) cell it accumulates the
	 * study's error into -- `shl $0x7` on the frame position, add the
	 * code, index +0x1000 and +0x1c00 of the detector.  So this pair is
	 * the hand-off from the decision to the study, and a batch that
	 * changed either would change what the study measures.  Finding 3531
	 * is the same mistake in the neighbouring header and this is why its
	 * rule is worth having: a claim that NOTHING reads a field is a claim
	 * about every function in the object, not about the ones in hand.
	 *
	 * BOTH ARE SIGNED.  The loads are `movzwl` and each is followed
	 * immediately by a `movswl` of the same register's low half
	 * (0x31459/0x31460 and 0x31470/0x3147b), so the value in use is
	 * sign-extended and the zero-extending load is the free half of
	 * finding 614 -- an extension whose upper bits are discarded by the
	 * next instruction.
	 *
	 * AND `decisionCode`'S SIGNEDNESS IS NOW FORCED BY MORE THAN THAT,
	 * which matters because 614's half is by itself only an absence of
	 * evidence.  `linearMappingStudy` tests `decisionCode - 1` with `js`
	 * (0x31466) and `decisionCode + 1` against the row length with `jge`
	 * (0x31545): a SIGN test and a SIGNED compare, neither of which an
	 * `unsigned short` promoted to `int` can ever produce, since such a
	 * value is never negative and GCC knows it.  The consequence is in the
	 * object too -- with the code at zero and the row length at one, the
	 * second test holds and `constellation[phase][-1]` is read.
	 */
	short decisionCode;
	short decisionFramePosition;

	unsigned int uint_1eb0;			/* the progress; see +0x1ea8 */

	/*
	 * +0x1eb4  NAMED, AND BY THE ONLY EVIDENCE CLAUDE.md RATES FIRST.
	 *
	 * This was `short_1eb4`, "MODELLED, UNNAMED ... `reset` writes it
	 * 16-bit wide (`mov %si,0x1eb4(%ebp)` at 0x30b34) and nothing else in
	 * the object touches it".  The width is unchanged and still forced;
	 * the "nothing else" was true of the tree at the time and is now
	 * false.  `V90Demodulator`'s two data-phase entries write it, and each
	 * prints what it just did:
	 *
	 *   enterDataPhase        resetLinearMappStudy(...); this = 1
	 *     "V90Demodulator: reset and enable linear mapping study in data"
	 *
	 *   enterDataSteadyState  this = 0
	 *     "V90Demodulator: disable linear mapping study."
	 *
	 * Two writers, opposite values, and a format string beside each saying
	 * "enable" for the 1 and "disable" for the 0.  So the field is the
	 * study's enable flag and the polarity is the object's own.
	 *
	 * AND THE READER SETTLES IT INDEPENDENTLY.  `V90Phase4Demodulator`'s
	 * two decision members -- landed before this batch and not changed by
	 * it -- do the same pair of writes with their own pair of strings
	 * ("reset & enable linear mapping study in TRN2", "disable linear
	 * mapping study") and, between them, GATE the call:
	 *
	 *     if (demapper->linearMappStudyEnabled != 0)
	 *             demapper->linearMappingStudy(sample, decision);
	 *
	 * That is the third tier of evidence agreeing with the first, and it
	 * is what makes the name a description of the field's role rather
	 * than of one writer's intent: the flag is read, and what it gates is
	 * `linearMappingStudy`.
	 *
	 * It stays a `short` and does NOT become a flag constant: CLAUDE.md
	 * names flags by bit value where a mask test reads a bit, and nothing
	 * masks this -- all three writers store a whole halfword.  The two
	 * bytes after it are the tail of the 0x1eb8 allocation and nothing
	 * reaches them.
	 */
	short linearMappStudyEnabled;
	unsigned char pad_1eb6[2];
};

#endif /* DSPLIB_V90DEMAPPER_H */
