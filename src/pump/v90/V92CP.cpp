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

#include <math.h>
#include <stddef.h>

#include "dsplib/V92CP.h"

/* Hold the compiler to the map in the header; see V90CP.cpp for why.  This
 * class has no pointer members, so the layout is the same at both widths --
 * the guard is kept for consistency with the sibling classes. */
#if __SIZEOF_POINTER__ == 4
#define V92CP_OFF(field, off, tag) \
	typedef char v92cp_off_##tag[ \
	    ((int)__builtin_offsetof(V92CP, field) == (off)) ? 1 : -1]

V92CP_OFF(byte_00,	0x000, byte00);
V92CP_OFF(char_01,	0x001, char01);
V92CP_OFF(char_02,	0x002, char02);
V92CP_OFF(byte_03,	0x003, byte03);
V92CP_OFF(byte_04,	0x004, byte04);
V92CP_OFF(word_08,	0x008, word08);
V92CP_OFF(word_0c,	0x00c, word0c);
V92CP_OFF(flt_10,	0x010, flt10);
V92CP_OFF(flt_14,	0x014, flt14);
V92CP_OFF(flt_18,	0x018, flt18);
V92CP_OFF(flt_1c,	0x01c, flt1c);
V92CP_OFF(flt_20,	0x020, flt20);
V92CP_OFF(byte_24,	0x024, byte24);
V92CP_OFF(word_28,	0x028, word28);
V92CP_OFF(short_42,	0x042, short42);
V92CP_OFF(short_a2,	0x0a2, shorta2);
V92CP_OFF(word_104,	0x104, word104);
V92CP_OFF(suv,		0x108, suv);
V92CP_OFF(word_10c,	0x10c, word10c);
V92CP_OFF(word_110,	0x110, word110);
V92CP_OFF(word_114,	0x114, word114);
V92CP_OFF(byte_118,	0x118, byte118);
V92CP_OFF(byte_119,	0x119, byte119);
V92CP_OFF(byte_11a,	0x11a, byte11a);
V92CP_OFF(word_11c,	0x11c, word11c);
V92CP_OFF(word_120,	0x120, word120);
V92CP_OFF(byte_128,	0x128, byte128);
V92CP_OFF(bits,		0x129, bits);
V92CP_OFF(crc,		0x8f9, crc);
V92CP_OFF(vectorLen,	0x90c, vectorlen);
V92CP_OFF(msgLen,	0x910, msglen);
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
	length = vectorLen;

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
 * alias.  MEASURED RATHER THAN ARGUED: the in-place form below reproduces the
 * object at every length the fixture drives, three of them past the point
 * where `bits[i]` addresses `crc`, under GCC 3.4.2 and GCC 13 alike.  So
 * loop-invariant motion hoists the loads and sinks the stores and the sixteen
 * slots are the compiler's.  WHY it is allowed to is NOT established here and
 * finding 4510 says so rather than naming a rule it has not checked.
 *
 * The two forms are not the same FUNCTION -- `bits` is 2,000 entries and `crc`
 * follows it, so a `msgLen` above 2,017 makes the loop read the register it
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
 * THE LOOP STARTS AT 18 and ends 17 short of `msgLen`, both literals the
 * object spells out; the guard `cmp %ebp,%esi; jae` at +0x4e609 is unsigned,
 * so a `msgLen` below 18 + 17 does nothing at all rather than running
 * backwards.
 * ===========================================================================
 */
void
V92CP::calcCRC()
{
	unsigned int n = msgLen - 17;
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
 * from `this + 0x8f9` and `%edi` holding `msgLen` is `bits[msgLen - 16 +
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
		int d = (int)crc[j] - (int)bits[msgLen - V92CP_CRC + j];

		acc = (unsigned char)(acc + (d < 0 ? -d : d));
	}

	return acc == 0;
}

/*
 * ===========================================================================
 * fltTable_2 (.data+0x69e0, 64 bytes) and fltTable_1 (.data+0x6a20, 28 bytes)
 *
 * The two weight tables `infoToBits` expands a magnitude against, greedily and
 * most significant first.  Both are GLOBAL `D` symbols, so neither is `const`
 * here: a namespace-scope `const` array has INTERNAL linkage in C++ and the
 * symbol would disappear.  The object holds a second, identical pair --
 * `fltTable2` at .data+0xb00 and `fltTable1` at .data+0xb40 -- which nothing
 * written references; finding 826 measured that the two pairs are separate
 * symbols and not aliases.
 *
 * THEY ARE TRANSCRIBED AND NOT GENERATED, and fltTable_2 is why.  Read as a
 * geometric sequence it is 2^2 down to 2^-13, sixteen entries -- except that
 * 2^-8 IS ABSENT and 2^-9 appears twice:
 *
 *     6a00  0000803c 0000003c 0000003b 0000003b
 *           2^-6     2^-7     2^-9     2^-9
 *
 * 0x3b800000 (2^-8) would sit between the third and fourth of those and does
 * not.  Any generator reproduces the wrong table, and the difference is
 * observable: the expansion is greedy, so a magnitude in [2^-8, 2^-7) sets a
 * different entry under each reading.  test/mutations/v92info.json carries the
 * repaired sequence as a mutation for exactly that reason.
 * ===========================================================================
 */
float fltTable_2[16] = {
	4.0f,		2.0f,		1.0f,		0.5f,
	0.25f,		0.125f,		0.0625f,	0.03125f,
	0.015625f,	0.0078125f,	0.001953125f,	0.001953125f,
	0.0009765625f,	0.00048828125f,	0.000244140625f, 0.0001220703125f
};

float fltTable_1[7] = {
	1.0f,		0.5f,		0.25f,		0.125f,
	0.0625f,	0.03125f,	0.015625f
};

/*
 * ===========================================================================
 * V92CP::infoToBits (.text+0x4ec80, 1,916 bytes)
 *
 * The transmit half of the class: the message fields at +0x000..+0x103 become
 * one byte per bit in `bits`, with a CRC and the padding that follows it.
 * `tools/dis.py` reports FIVE relocations in the whole range and all five are
 * `R_386_32` against the two tables above -- no call relocation anywhere -- so
 * the closure of this function is itself and those two symbols, and
 * `resetCRC` and `calcCRC` appear here INLINED rather than called.
 *
 * ---------------------------------------------------------------------------
 * THE MESSAGE IS SEVENTEEN-ENTRY GROUPS.  Group 0 is `bits[0..16]`, seventeen
 * ONES.  Every group after it is a zero at an index that is a multiple of
 * seventeen followed by sixteen payload entries, and `calcCRC` skips exactly
 * those indices.  Every literal displacement below is the object's:
 *
 *     0..16     seventeen ones                       every message
 *     17        marker
 *     18..33    byte_00; then either the SUV form or the field form
 *     34        marker                               char_01 <= 1 only
 *     35..50    byte_03, thirteen more of char_02, two of word_0c
 *     51        marker
 *     52..67    sixteen magnitude entries of flt_10
 *     68        marker
 *     69..84    flt_14 and flt_18, seven magnitude and one sign each
 *     85        marker
 *     86..101   flt_1c and flt_20, likewise
 *     102       marker
 *     103..118  four bits each of word_28[0..3]
 *     119       marker
 *     120..135  four bits each of word_28[4..5], byte_24, seven zeros
 *     then      word_10c groups of eight from short_42, and as many again
 *               from short_a2 when bits[128] is non-zero
 *     then      a marker, the sixteen CRC entries, a marker, and zeros to
 *               `vectorLen`
 *
 * ---------------------------------------------------------------------------
 * THREE READINGS THAT ARE FORCED AND LOOK LIKE MISTAKES.  Each is reproduced
 * because the object encodes it and no reading of the source can avoid it.
 *
 *  1. `t` IS NOT RELOADED between the five bits of `char_02` at 21..25 and the
 *     thirteen at 36..48.  `%ecx` holds it across the branch at .text+0x4ed10
 *     and there is no second `movsbl`; the thirteen are therefore the sign
 *     extension of a signed byte, thirteen copies of bit 7.  A version that
 *     re-reads the field differs the moment any of bits 0..3 is set, which is
 *     what the `+0x02 = 0x0f` trials in t_v92info.cpp are for -- 0xff cannot
 *     tell the two apart, because both give thirteen ones.
 *
 *  2. `word_104` is FOUR BYTES -- `setSUV` stores it with a 32-bit `mov` and
 *     dropping to `short` would leave +0x106 untouched -- and is read here
 *     `movswl`, whose 32-bit result is then shifted, which is CLAUDE.md's
 *     forced case.  The two constraints together are a `short` local, and that
 *     is what is written.  Only five bits leave, so nothing observable turns
 *     on it.
 *
 *  3. THE SECOND MASK BLOCK IS GATED ON `bits[128]` AND NOT ON `byte_24`.
 *     The object loads `0x1a9(%edi)`, which is the entry it has just written,
 *     rather than `0x24(%edi)`.  Nothing writes either between the two, so the
 *     two readings agree over every input; the load is what says which one the
 *     author wrote.
 *
 * ---------------------------------------------------------------------------
 * FIVE MAGNITUDE EXPANSIONS, WRITTEN OUT.  Each walks its table from the
 * heaviest weight down, subtracting where it fits, and writes the entries
 * DESCENDING -- `dec %ecx` on the pointer -- so the most significant lands at
 * the highest index and the vector reads least-significant-first like every
 * other field.  The comparison is `fcom` against the table entry with `ja`, so
 * an entry heavier than the residue is a zero and no subtraction.
 *
 * The sign entry is the ORIGINAL value against zero and not the residue:
 * `fldz; fcom %st(1); seta` for the first and `fcom %st(1); setb` for the
 * other three, which are the same predicate with the operands the other way
 * up.  They are not the same predicate for a NaN, and no trial feeds one:
 * `-mno-ieee-fp` withdraws the parity test, so a NaN would take a different
 * arm on each and the object has no defence against it either.
 *
 * ---------------------------------------------------------------------------
 * THE TWO LENGTHS.  `msgLen` is the message including its CRC, `vectorLen` is
 * that rounded UP to the next strictly greater multiple of `12 * byte_128`
 * with the gap zero-filled.  This function is what settled which is which; the
 * header carries the argument.  The rounding is an unsigned `div`, so
 * `byte_128 == 0` divides by zero here as it does in the object, and the
 * fixture's grid keeps away from it rather than letting the compiler
 * adjudicate our own undefined behaviour.
 * ===========================================================================
 */
void
V92CP::infoToBits()
{
	unsigned int i;
	unsigned int pos;
	unsigned int quantum;
	int j;
	int t;
	unsigned char e;
	unsigned char *p;
	float x;

	for (i = 0; i <= 16; i++)
		bits[i] = 1;

	bits[17] = 0;
	bits[18] = byte_00;

	if (byte_00 == 1) {
		short s;

		for (i = 19; i <= 25; i++)
			bits[i] = 0;

		bits[26] = 0;

		s = (short)word_104;
		for (i = 0; i <= 4; i++) {
			bits[27 + i] = (unsigned char)(s & 1);
			s = (short)(s >> 1);
		}

		bits[32] = (unsigned char)suv;

		e = byte_04;
		word_11c = 34;
		bits[33] = e;
	} else {
		signed char b = char_01;

		byte_118 = (unsigned char)b;
		bits[19] = (unsigned char)(b & 1);
		bits[20] = (unsigned char)((b >> 1) & 1);

		if (b == 0)
			byte_128 = 1;

		t = char_02;
		for (i = 0; i <= 4; i++) {
			bits[21 + i] = (unsigned char)(t & 1);
			t >>= 1;
		}

		for (i = 26; i <= 32; i++)
			bits[i] = 0;

		if (b <= 1) {
			bits[31] = (unsigned char)(word_08 & 1);
			bits[32] = (unsigned char)((word_08 >> 1) & 1);
		}

		e = byte_04;
		word_11c = 34;
		bits[33] = e;

		if (b <= 1) {
			unsigned short n;
			unsigned int k;

			bits[34] = 0;
			bits[35] = byte_03;

			for (i = 0; i <= 12; i++) {
				bits[36 + i] = (unsigned char)(t & 1);
				t >>= 1;
			}

			bits[49] = (unsigned char)(word_0c & 1);
			bits[50] = (unsigned char)((word_0c >> 1) & 1);
			bits[51] = 0;

			x = (float)fabs(flt_10);
			p = &bits[67];
			for (j = 0; j <= 15; j++) {
				if (fltTable_2[j] > x) {
					*p = 0;
				} else {
					*p = 1;
					x -= fltTable_2[j];
				}
				p--;
			}

			bits[68] = 0;

			bits[76] = (unsigned char)(flt_14 < 0);
			x = (float)fabs(flt_14);
			p = &bits[75];
			for (j = 0; j <= 6; j++) {
				if (fltTable_1[j] > x) {
					*p = 0;
				} else {
					*p = 1;
					x -= fltTable_1[j];
				}
				p--;
			}

			bits[84] = (unsigned char)(flt_18 < 0);
			x = (float)fabs(flt_18);
			p = &bits[83];
			for (j = 0; j <= 6; j++) {
				if (fltTable_1[j] > x) {
					*p = 0;
				} else {
					*p = 1;
					x -= fltTable_1[j];
				}
				p--;
			}

			bits[85] = 0;

			bits[93] = (unsigned char)(flt_1c < 0);
			x = (float)fabs(flt_1c);
			p = &bits[92];
			for (j = 0; j <= 6; j++) {
				if (fltTable_1[j] > x) {
					*p = 0;
				} else {
					*p = 1;
					x -= fltTable_1[j];
				}
				p--;
			}

			bits[101] = (unsigned char)(flt_20 < 0);
			x = (float)fabs(flt_20);
			p = &bits[100];
			for (j = 0; j <= 6; j++) {
				if (fltTable_1[j] > x) {
					*p = 0;
				} else {
					*p = 1;
					x -= fltTable_1[j];
				}
				p--;
			}

			bits[102] = 0;

			for (i = 0; i <= 3; i++) {
				short s = (short)word_28[i];

				p = &bits[103 + 4 * i];
				for (j = 3; j >= 0; j--) {
					*p++ = (unsigned char)(s & 1);
					s = (short)(s >> 1);
				}
			}

			bits[119] = 0;

			for (i = 0; i <= 1; i++) {
				short s = (short)word_28[4 + i];

				p = &bits[120 + 4 * i];
				for (j = 3; j >= 0; j--) {
					*p++ = (unsigned char)(s & 1);
					s = (short)(s >> 1);
				}
			}

			bits[128] = byte_24;

			for (i = 129; i <= 135; i++)
				bits[i] = 0;

			word_11c = 136;

			n = word_10c;

			for (i = 0; i < n; i++) {
				for (k = 0; k <= 7; k++) {
					short s = short_42[i][k];

					pos = (unsigned int)word_11c;
					bits[pos] = 0;
					word_11c = (int)(pos + 1);
					p = &bits[pos + 1];
					for (j = 15; j >= 0; j--) {
						*p++ = (unsigned char)(s & 1);
						s = (short)(s >> 1);
					}
					word_11c = (int)(pos + 17);
				}
			}

			if (bits[128] != 0) {
				for (i = 0; i < n; i++) {
					for (k = 0; k <= 7; k++) {
						short s = short_a2[i][k];

						pos = (unsigned int)word_11c;
						bits[pos] = 0;
						word_11c = (int)(pos + 1);
						p = &bits[pos + 1];
						for (j = 15; j >= 0; j--) {
							*p++ = (unsigned char)
							    (s & 1);
							s = (short)(s >> 1);
						}
						word_11c = (int)(pos + 17);
					}
				}
			}

			e = byte_04;
		}
	}

	pos = (unsigned int)word_11c;
	bits[pos] = 0;
	word_11c = (int)(pos + 1);
	msgLen = pos + 17;

	resetCRC();
	calcCRC();

	pos = (unsigned int)word_11c;
	for (i = 0; i <= 15; i++)
		bits[pos + i] = crc[i];
	bits[pos + 16] = 0;
	word_11c = (int)(pos + 17);

	quantum = 12u * byte_128;
	vectorLen = ((unsigned int)word_11c / quantum + 1) * quantum;

	for (i = (unsigned int)word_11c; i < vectorLen; i++)
		bits[i] = 0;

	if (e != 0)
		word_110 = 1;
}
