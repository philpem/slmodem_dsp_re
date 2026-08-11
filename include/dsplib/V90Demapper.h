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
	 * Declared for the record and deliberately left undefined -- their
	 * signatures come from the mangling, so this list is a specification
	 * rather than a guess, and a return type is not mangled and is
	 * therefore unknown for all of them.  They are this class's
	 * processing half, 4,400-odd bytes, and belong to whichever batch
	 * writes it.
	 */
	void reset(V90MappingParams *);
	void resetNoSpectral(V90MappingParams *);
	void resetLinearMappStudy(unsigned int);
	void incrementRBSFramePosition();
	void hardDecision(short);
	void linearMappingStudy(short, short);
	void updateConstelation();
	void process(unsigned char *, unsigned int &);

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
	 * consecutive `movl $0x0`.  Nothing written here reads any of them.
	 */
	unsigned int word_04;
	unsigned int word_08;
	unsigned int word_0c;
	unsigned int word_10;
	unsigned int word_14;
	unsigned int word_18;

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
	 * so +0x1c holds `count_24` elements of four bytes and +0x20 holds
	 * `count_24` of one.  The ELEMENT WIDTH is derivable and the element
	 * TYPE is not -- four bytes is equally an `int`, an `unsigned` or a
	 * `float` -- so they are `void *` until something that reads them is
	 * written.  Both are OWNED: the destructor frees them.
	 */
	void *array_1c;
	void *array_20;

	/* +0x24  The element count both allocations were sized from. */
	unsigned int count_24;

	unsigned int word_28;		/* +0x28  zeroed by the constructor */
	unsigned int word_2c;		/* +0x2c  zeroed by the constructor */

	/*
	 * +0x30  The six constellations, up to 128 signed PCM levels each.
	 * `short` because the load that feeds `printErrorHistogramAndReset`'s
	 * per-level line is `movswl` and its 32-bit result is used; see the
	 * file comment.  `updateConstelation` fills it with `fistps`.
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
	 * +0x664  `movb $0x0,0x664(%esi)` -- one byte, so a `char`-width flag.
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
	 */
	unsigned char byte_664;
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
	 * +0x1e94 .. +0x1e9f  NOT MODELLED.  Three words `reset`,
	 * `resetNoSpectral`, `hardDecision` and `linearMappingStudy` touch
	 * and nothing written here does.
	 */
	unsigned char pad_1e94[12];

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
	 * +0x1ea4 .. +0x1eb7  NOT MODELLED, and the ONLY thing that bounds it
	 * is the 0x1eb8 allocation.  `reset`, `resetLinearMappStudy`,
	 * `linearMappingStudy` and `hardDecision` touch +0x1ea4, +0x1ea6,
	 * +0x1ea8, +0x1eac, +0x1eae, +0x1eb0 and +0x1eb4 between them, at
	 * mixed widths; nothing written here does.
	 */
	unsigned char pad_1ea4[0x14];
};

#endif /* DSPLIB_V90DEMAPPER_H */
