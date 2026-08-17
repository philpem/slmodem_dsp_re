/*
 * V92CP.cpp -- V.92 CP message: construction and destruction.
 *
 * Reconstructed from dsplibs.o V92CP.cpp.  Two of the class's twelve
 * symbols: the constructor (0x4e8a0, 61 bytes) and the destructor (0x4e5b0,
 * one byte -- a bare `ret`).  `include/dsplib/V92CP.h` carries the object map
 * and says which member proved which offset.
 *
 * THE CONSTRUCTOR IS `reset` PLUS ONE STORE.  `V92CP::reset` (0x4e860, 57
 * bytes) writes +0x11c = 18, +0x120 = 0, +0x114 = 0, +0x119 = 0, +0x11a = 0
 * and +0x914 = -1; the constructor writes those six and `movb $0x0,0x4(%eax)`
 * as well.  Both are ordinary GLOBAL symbols in `.text` rather than linkonce,
 * so `reset` is not an in-class inline the compiler folded in (GCC 3.4 at -O2
 * does not inline an ordinary global function): the original repeated the
 * assignments.  Finding 1237.
 *
 * The calling convention is plain cdecl -- `mov 0x4(%esp),%eax` -- not
 * thiscall (finding 215).
 */

#include <stddef.h>

#include "dsplib/V92CP.h"

/* Hold the compiler to the map in the header; see V90CP.cpp for why.  This
 * class has no pointer members, so the layout is the same at both widths --
 * the guard is kept for consistency with the sibling classes. */
#if __SIZEOF_POINTER__ == 4
#define V92CP_OFF(field, off, tag) \
	typedef char v92cp_off_##tag[ \
	    ((int)__builtin_offsetof(V92CP, field) == (off)) ? 1 : -1]

V92CP_OFF(byte_04,	0x004, byte04);
V92CP_OFF(word_104,	0x104, word104);
V92CP_OFF(suv,		0x108, suv);
V92CP_OFF(word_114,	0x114, word114);
V92CP_OFF(byte_119,	0x119, byte119);
V92CP_OFF(byte_11a,	0x11a, byte11a);
V92CP_OFF(word_11c,	0x11c, word11c);
V92CP_OFF(word_120,	0x120, word120);
V92CP_OFF(bits,		0x129, bits);
V92CP_OFF(crc,		0x8f9, crc);
V92CP_OFF(word_90c,	0x90c, word90c);
V92CP_OFF(word_910,	0x910, word910);
V92CP_OFF(word_914,	0x914, word914);
typedef char v92cp_size[(sizeof(V92CP) == 0x918) ? 1 : -1];
#endif

V92CP::V92CP()
{
	byte_04 = 0;

	word_114 = 0;
	byte_119 = 0;
	byte_11a = 0;
	word_11c = 18;
	word_120 = 0;

	word_914 = -1;
}

/*
 * One byte in the object: `ret`.  The class allocates nothing -- unlike
 * V90CP, whose 173-byte destructor releases six buffers -- so there is
 * nothing for this to do, and the size is the evidence that it does none.
 */
V92CP::~V92CP()
{
}

/*
 * ===========================================================================
 * V92CP::getBitVector (.text+0x4ebe0, 22 bytes)
 *
 * Six instructions.  The length is reported through the reference and the
 * vector itself is the return value -- `add $0x129,%eax`, which is the
 * accessor that pinned `bits`'s offset in the first place.
 * ===========================================================================
 */
unsigned char *
V92CP::getBitVector(unsigned int &length)
{
	length = word_90c;

	return bits;
}

/*
 * ===========================================================================
 * V92CP::setSUV (.text+0x4e920, 26 bytes)
 *
 * Two stores, the constant first.  What the sixteen counts is not
 * established; `word_104`'s name says so.
 * ===========================================================================
 */
void
V92CP::setSUV(unsigned int v)
{
	word_104 = 16;
	suv = v;
}

/*
 * ===========================================================================
 * V92CP::resetCRC (.text+0x4e5d0, 32 bytes)
 *
 * All sixteen bits of the register to ONE, which is the CCITT convention and
 * not the zero a reader expects.
 *
 * THE COUNTER IS SIGNED and that is forced: `cmp $0xf,%eax; jle`.  An
 * unsigned bound would have been `jbe`.  It costs nothing here -- the range
 * is 0..15 either way -- and it is written as the object has it because the
 * loop is repeated inside `evaluateCRC` and a difference there would read as
 * a codegen mismatch in a function that has one to spare.
 * ===========================================================================
 */
void
V92CP::resetCRC()
{
	int i;

	for (i = 0; i <= V92CP_CRC - 1; i++)
		crc[i] = 1;
}

/*
 * ===========================================================================
 * V92CP::resetDetector (.text+0x4e830, 46 bytes)
 *
 * Five stores and nothing else.  The order is the object's: +0x11c first,
 * then +0x120, +0x114, and the two bytes.  It is NOT the constructor's order,
 * which is why the two are written separately rather than one calling the
 * other -- `V92CP::V92CP` clears +0x04 first and this does not touch it at
 * all, which the header records as the whole difference between them.
 * ===========================================================================
 */
void
V92CP::resetDetector()
{
	word_11c = 18;
	word_120 = 0;
	word_114 = 0;
	byte_119 = 0;
	byte_11a = 0;
}

/*
 * ===========================================================================
 * V92CP::reset (.text+0x4e860, 57 bytes)
 *
 * `resetDetector` and one more store.  The object holds the five stores
 * inlined -- there is no call and no relocation in the range -- and the extra
 * `movl $0xffffffff` is interleaved with them, which is scheduling.
 *
 * +0x04 IS NOT TOUCHED.  The constructor clears it and nothing else ever
 * writes it, so an object that has been reset is not an object that has been
 * constructed.  Reproduced.
 * ===========================================================================
 */
void
V92CP::reset()
{
	resetDetector();

	word_914 = -1;
}

/*
 * ===========================================================================
 * V92CP::calcCRC (.text+0x4e5f0, 570 bytes)
 *
 * A sixteen-stage shift register, one byte per stage, clocked over the
 * message.  Feedback is `crc[0] + bits[i]` and it enters at stages 3, 10 and
 * 15 -- the CCITT x^16 + x^12 + x^5 + 1 taps counted from the other end --
 * with every stage masked to one bit as it is written.
 *
 * THE OBJECT HOLDS THE REGISTER IN SIXTEEN STACK SLOTS ACROSS THE LOOP, AND
 * THAT IS THE COMPILER RATHER THAN THE SOURCE.  It loads all sixteen bytes of
 * `crc` before the loop and stores them all back after it, and writes nothing
 * to the object in between.  That looked like proof the author had copied the
 * register into locals -- `bits` and `crc` are both `unsigned char` and abut,
 * so an in-place loop would have to reload after every store IF they could
 * alias.  They cannot: GCC's alias analysis is component-based, two distinct
 * members of one class never alias whatever their types, and loop-invariant
 * motion is free to hoist the loads and sink the stores out of a loop that
 * only reads `bits`.  Written in place, that is exactly the code that comes
 * out -- which is why this is written in place.  Finding 4510, and it is a
 * correction of a reading this file carried for one revision.
 *
 * The two forms are not the same FUNCTION -- `bits` is 2,000 entries and `crc`
 * follows it, so a `word_910` above 2,017 makes the loop read the register it
 * is updating, and the C-level answers then differ.  Neither the differential
 * tier nor the codegen tier can see it, because the compiler resolves the
 * question the same way in both.  t_v92cpcrc.cpp drives past 2,017 anyway; a
 * trial that cannot fail today is still the trial that catches this if the
 * layout ever moves.
 *
 * EVERY SEVENTEENTH POSITION IS SKIPPED.  `i % 17 == 0` costs the 0xf0f0f0f1
 * reciprocal and a `cmp $0x1; adc $0x0` -- the object's if-conversion of an
 * increment, which is free (finding 2411).  The message therefore carries a
 * fill bit in every seventeenth slot and the CRC does not see it.
 *
 * THE LOOP STARTS AT 18 and ends 17 short of `word_910`, both literals the
 * object spells out; the guard `cmp %ebp,%esi; jae` at +0x4e609 is unsigned,
 * so a `word_910` below 18 + 17 does nothing at all rather than running
 * backwards.
 * ===========================================================================
 */
void
V92CP::calcCRC()
{
	unsigned int n = word_910 - 17;
	unsigned int i;
	unsigned char t;

	for (i = 18; i < n; i++) {
		if (i % 17 == 0)
			i++;

		t = (unsigned char)(crc[0] + bits[i]);

		crc[0] = crc[1];
		crc[1] = crc[2];
		crc[2] = crc[3];
		crc[3] = (unsigned char)((crc[4] + t) & 1);
		crc[4] = crc[5];
		crc[5] = crc[6];
		crc[6] = crc[7];
		crc[7] = crc[8];
		crc[8] = crc[9];
		crc[9] = crc[10];
		crc[10] = (unsigned char)((crc[11] + t) & 1);
		crc[11] = crc[12];
		crc[12] = crc[13];
		crc[13] = crc[14];
		crc[14] = crc[15];
		crc[15] = (unsigned char)(t & 1);
	}
}

/*
 * ===========================================================================
 * V92CP::evaluateCRC (.text+0x4e940, 668 bytes)
 *
 * `resetCRC`, `calcCRC`, and then the check -- the first two INLINED, which
 * is most of the 668 bytes and why this is nearly as large as `calcCRC`
 * itself.  There is no call and no relocation anywhere in the range.
 *
 * THE COMPARISON IS AN ABSOLUTE DIFFERENCE SUMMED INTO A BYTE, not a memcmp
 * and not a boolean fold: `sub; cltd; xor %edx,%eax; sub %edx,%eax` is the
 * branchless absolute value, and only `%al` is added to the accumulator.  The
 * distinction matters if either array ever holds something other than 0 or 1
 * -- sixteen differences of 16 would sum to 256 and wrap to zero, and the
 * message would verify.  Reproduced; docs/deviations.md D503.
 *
 * WHERE THE RECEIVED CRC LIVES.  `-0x7e0(%ecx,%edi,1)` with `%ecx` walking
 * from `this + 0x8f9` and `%edi` holding `word_910` is `bits[word_910 - 16 +
 * j]`, so the sixteen bits under test are the last sixteen of the message.
 * The header's +0x910 entry carries the arithmetic.
 * ===========================================================================
 */
int
V92CP::evaluateCRC()
{
	unsigned char acc = 0;
	int j;

	resetCRC();
	calcCRC();

	for (j = 0; j < V92CP_CRC; j++) {
		int d = (int)crc[j] - (int)bits[word_910 - V92CP_CRC + j];

		acc = (unsigned char)(acc + (d < 0 ? -d : d));
	}

	return acc == 0;
}
