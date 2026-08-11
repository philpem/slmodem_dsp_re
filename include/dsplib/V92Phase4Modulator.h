/*
 * V92Phase4Modulator.h -- the V.92 phase 4 upstream symbol source, PARTIAL.
 *
 * Reconstructed from dsplibs.o.  The class has 31 symbols; TWO of them are
 * written in src/pump/v90/V92Phase4Modulator.cpp -- the constructor (C1 at
 * .text+0x17970 and C2 at +0x17a20, 164 bytes each) and the destructor (D2 at
 * +0x16de0 and D1 at +0x16e40, 87 bytes each).  The other twenty-nine are not
 * declared here at all: `generateSymbol` alone is 4,055 bytes, none of them
 * has been read, and a declaration whose signature is guessed is worse than
 * no declaration.  Whoever writes them extends this file.
 *
 * SO THE MAP BELOW IS THE CONSTRUCTOR'S AND THE DESTRUCTOR'S, AND NOTHING
 * ELSE.  Eleven of the object's 460 bytes are named; the other 449 are `pad_`
 * regions, which `tools/whichfield.py` reports as such and which is the
 * honest answer -- that part is not modelled as fields yet.
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
 * been handed, with %esi zeroed two instructions earlier.  It is the only
 * writer of that word anywhere in this tree, which is why V92CP.h names it
 * out of a `pad_` region and says who does it.
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

class V92Phase4Modulator {
public:
	V92Phase4Modulator(V92Parameters *params, V92BitsToSymbol *bitsToSymbol,
			   V92CP *cp, V92MappingParams *mappingParams);
	~V92Phase4Modulator();

	/* Public for `offsetof`, which wants standard layout; and one access
	 * section, for the same reason V92Precoder.h gives. */

	/*
	 * +0x00 .. +0x17  Not touched by either member written here.  The
	 * constructor leaves them as the allocation left them.
	 */
	unsigned char pad_00[0x18];

	/* +0x18  Cleared by the constructor.  A four-byte store; its role is
	 * not established. */
	unsigned int word_18;

	/* +0x1c  Cleared by the constructor, `movb $0x0` -- one byte, and the
	 * last thing the constructor does. */
	unsigned char byte_1c;

	/* +0x1d .. +0x47  Not touched. */
	unsigned char pad_1d[0x2b];

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

	/* +0x78 .. +0x1bf  Not touched by either member written here.  Three
	 * hundred and twenty-eight bytes that belong to the twenty-nine
	 * members this file does not carry. */
	unsigned char pad_78[0x148];

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
