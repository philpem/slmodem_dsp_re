/*
 * GenericToneDetector.cpp -- the constructor and the destructor.
 *
 * `include/dsplib/GenericToneDetector.h` carries the object map, the 60-byte
 * measurement, the eleven arguments and where each of them goes.  This file is
 * the two members and the three things about them that are not visible from
 * the field list.
 *
 * PLAIN CDECL with `this` as the first STACK argument (finding 215).  The
 * constructor's prologue is `sub $0x2c,%esp` after no push, then four
 * callee-saved registers written to the frame by hand -- `-fomit-frame-pointer`
 * and `-maccumulate-outgoing-args`, which is the toolchain the object was built
 * with (tools/toolchain/build.sh).  Nothing here needs a calling-convention
 * attribute, and the two `float` arguments occupy one four-byte stack slot each
 * because the call is prototyped.
 *
 * ---------------------------------------------------------------------------
 * THE FILTER IS ALLOCATED WITH `new` AND FREED WITH `delete`, and the reason
 * that is the reading rather than a hand-written malloc pair is set out at
 * length in `GenericIIR.h`, where the two allocation operators live.  In
 * summary: the constructor calls `sysdep_malloc` DIRECTLY with 0x34 -- which is
 * `sizeof(GenericIIR<float, double>)` -- and then the filter's `C1` with NO
 * null check between them, which is what a plain `operator new` gives; and the
 * destructor tests for null, calls `D1`, then calls `sysdep_free`, which is
 * what a delete-expression gives and a hand-written free would not.
 *
 * ---------------------------------------------------------------------------
 * THE CONSTRUCTOR'S TAIL IS `reset()` INLINED, and it is now spelled that way.
 * `_ZN19GenericToneDetector5resetEv` is in the blob at 0x10640, 68 bytes, and
 * its body is: call `GenericIIR<float,double>::reset` on +0x00, then zero
 * +0x2c, +0x0c, +0x10, +0x14, +0x18, +0x30, +0x24, +0x38.  The constructor
 * ends with a call to the SAME `GenericIIR` member and stores to exactly those
 * eight fields and no others -- in a different order, which is scheduling and
 * free (CLAUDE.md).  Set intersection over eight fields and one call is not a
 * coincidence, so the original's constructor said `reset();` and GCC inlined
 * it.
 *
 * It was written out here while `reset()` was a later batch.  That batch has
 * landed, the tail is the call, and the generated code is the same code.
 *
 * ---------------------------------------------------------------------------
 * ROUNDING UP IS A MULTIPLY, NOT A REMAINDER.  Both duration arguments are
 * converted with
 *
 *     div  %edi                ; n = samples / blockLen
 *     mov  %eax,%ecx
 *     imul %edi,%eax           ; n * blockLen
 *     cmp  %ebx,%eax
 *     jae  ...                 ; store n
 *     lea  0x1(%ecx),%ebx      ; else store n + 1
 *
 * -- and the `imul` is the point.  The remainder of that same division is
 * already sitting in %edx, so a source that had said `if (samples % blockLen)`
 * would have tested %edx and needed no multiply at all.  The object multiplies
 * the quotient back out, so the source multiplied.  The two forms agree over
 * every input, which is exactly why the instruction and not the behaviour is
 * what settles which one was written.
 *
 * The division is NOT guarded against a zero `blockLen`.  Reproduced; see
 * docs/deviations.md.
 */

#include <stddef.h>

#include "dsplib/GenericIIR.h"
#include "dsplib/GenericToneDetector.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts its
 * own -- and this is the check that catches an object right in size and wrong
 * in its offsets.
 *
 * GUARDED ON THE POINTER WIDTH, because the object leads with one and
 * `make check64` compiles this file for the native target, where it is eight
 * bytes and every offset after it moves.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define GTD_OFF(field, off, tag) \
	typedef char gtd_off_##tag[ \
	    ((int)__builtin_offsetof(GenericToneDetector, field) \
	     == (off)) ? 1 : -1]

GTD_OFF(filter,      0x00, filter);
GTD_OFF(threshold,   0x04, threshold);
GTD_OFF(ratio,       0x08, ratio);
GTD_OFF(acc_0c,      0x0c, acc0c);
GTD_OFF(acc_10,      0x10, acc10);
GTD_OFF(acc_14,      0x14, acc14);
GTD_OFF(acc_18,      0x18, acc18);
GTD_OFF(blocks1,     0x1c, blocks1);
GTD_OFF(blocks2,     0x20, blocks2);
GTD_OFF(sampleCount, 0x24, samplecount);
GTD_OFF(blockLen,    0x28, blocklen);
GTD_OFF(count_2c,    0x2c, count2c);
GTD_OFF(count_30,    0x30, count30);
GTD_OFF(flag,        0x34, flag);
GTD_OFF(detected,    0x38, detected);
typedef char gtd_size[(sizeof(GenericToneDetector) == 0x3c) ? 1 : -1];
#endif

GenericToneDetector::GenericToneDetector(unsigned int nden, unsigned int nnum,
					 double *den, double *num,
					 unsigned int samples1,
					 unsigned int samples2,
					 float threshold_, unsigned int flag_,
					 float ratio_, unsigned int blockLen_,
					 unsigned int blockSize)
{
	unsigned int n;

	filter = new GenericIIR<float, double>(nden, nnum, den, num, blockSize);

	blockLen = blockLen_;
	threshold = threshold_;
	ratio = ratio_;

	n = samples1 / blockLen_;
	if (n * blockLen_ < samples1)
		n++;
	blocks1 = n;

	n = samples2 / blockLen_;
	if (n * blockLen_ < samples2)
		n++;
	blocks2 = n;

	flag = flag_;

	reset();
}

/*
 * ONE CALL AND EIGHT ZEROES, in the object's own order: +0x2c first, then the
 * four accumulators out of one zeroed register, then +0x30, +0x24, +0x38.
 * Storing an integer zero into a `float` is what the compiler does for `= 0`
 * and says nothing about the field's type; GenericToneDetector.h reads those
 * types off the `fadds`/`fsts` in `process` instead.
 *
 * What it does NOT touch is the whole of the configuration -- `threshold`,
 * `ratio`, `blocks1`, `blocks2`, `blockLen` and `flag` -- so a reset detector
 * is the same detector, and the test asserts that rather than only asserting
 * the eight zeroes.
 */
void GenericToneDetector::reset()
{
	filter->reset();

	count_2c = 0;
	acc_0c = 0;
	acc_10 = 0;
	acc_14 = 0;
	acc_18 = 0;
	count_30 = 0;
	sampleCount = 0;
	detected = 0;
}

/*
 * One `delete`, and nothing else: the object is not written, so `filter` is
 * left dangling rather than nulled.  The test asserts that, because "the
 * destructor leaves the object alone" is a claim about forty bytes of code and
 * not an absence of one.
 */
GenericToneDetector::~GenericToneDetector()
{
	delete filter;
}
