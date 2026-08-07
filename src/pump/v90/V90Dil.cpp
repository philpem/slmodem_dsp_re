/*
 * V90Dil.cpp -- calculateDilLength.
 *
 * A free function with a mangled name, which the linker treats exactly like a
 * member: `_Z18calculateDilLengthP19tagV90DILdescriptor7PcmType`.  Nothing
 * here is a method, so there is no `this` and no calling-convention question;
 * both arguments are ordinary stack arguments.
 *
 * THE SEGMENT BOUNDARIES ARE A LOCAL ARRAY, NOT THE MODULATOR'S STATIC ONE.
 * `V90Phase3Modulator::codeSegmentsBoundriesLookupTable` is a defined data
 * symbol at .data:0x440 holding LINEAR levels -- 124, 380, 892, ... -- and
 * `resetDILGenerator` indexes it.  This function does not touch it.  It
 * copies sixteen ints out of .rodata:0xb80 onto its own stack frame with
 * `rep movsl` once per DIL entry, and those sixteen are CODE boundaries:
 * 0x0f, 0x1f, 0x2f ... 0x7f and 0x1f, 0x2f ... 0x7f, 0. An anonymous constant
 * pool entry rather than a symbol, so it closes no link requirement of its
 * own; being re-copied inside the loop is what says it is a local declared in
 * the loop body rather than a file-scope or `static` one.
 *
 * The row is chosen by `8 * pcmType`, and the row LENGTH by `7 + (pcmType !=
 * 1)`: eight boundaries under mu-law, seven under A-law.  That is why the
 * A-law row's eighth entry is a zero that is never read.
 */

#include <stddef.h>

#include "dsplib/V90Dil.h"

/*
 * The object reads the length code one past `segmentSize` when the search
 * falls off the end, so the read is spelled through a byte pointer here: see
 * the comment on `seg` below.
 */
#define DIL_SEGMENT_SIZE_OFF \
	((unsigned int)__builtin_offsetof(tagV90DILdescriptor, segmentSize))

unsigned int
calculateDilLength(tagV90DILdescriptor *dil, PcmType pcmType)
{
	unsigned int length = 0;
	unsigned int count;
	unsigned int i;

	if (dil == 0)
		return length;

	count = dil->dilCount;
	if (count == 0)
		return length;

	for (i = 0; i < count; i++) {
		/*
		 * Re-initialised on every iteration, which is what the object
		 * does; see the file comment.
		 */
		int codeSegmentsBoundries[2][8] = {
			{ 0x0f, 0x1f, 0x2f, 0x3f, 0x4f, 0x5f, 0x6f, 0x7f },
			{ 0x1f, 0x2f, 0x3f, 0x4f, 0x5f, 0x6f, 0x7f, 0x00 }
		};
		unsigned int rowLength = pcmType == PCM_TYPE_A_LAW ? 7u : 8u;
		unsigned int code = dil->dilCode[i];
		unsigned int seg;

		for (seg = 0; seg < rowLength; seg++)
			if (codeSegmentsBoundries[pcmType][seg] >= (int)code)
				break;

		/*
		 * THE OBJECT READS ONE PAST `segmentSize` AND THIS REPRODUCES
		 * IT.  Under mu-law the search runs eight boundaries, the
		 * largest of which is 0x7f, so a DIL code of 0x80 or more
		 * matches none of them and leaves `seg` at 8 -- and the byte
		 * taken is then +0x103 + 8, which is `segmentCode[0]`, the
		 * member after the one indexed.  Under A-law the row is seven
		 * long, so `seg` stops at 7 and the read is in range.
		 *
		 * Spelled through a byte pointer rather than as
		 * `segmentSize[seg]` so that the out-of-range index is
		 * explicit and cannot be assumed away by a compiler that
		 * believes the declared bound.
		 */
		length += 6u * ((const unsigned char *)dil)[
		    DIL_SEGMENT_SIZE_OFF + seg] + 6u;
	}

	return length;
}
