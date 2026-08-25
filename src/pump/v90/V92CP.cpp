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
 * assignments.  Finding F1237.
 *
 * The calling convention is plain cdecl -- `mov 0x4(%esp),%eax` -- not
 * thiscall (finding F215).
 */

#include <math.h>
#include <stddef.h>

#include "dsplib/debug.h"
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
V92CP_OFF(word_124,	0x124, word124);
V92CP_OFF(bitsPerSymbol,	0x128, byte128);
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
 * finding F4510 says so rather than naming a rule it has not checked.
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
 * increment, which is free (finding F2411).  The message therefore carries a
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
 * `fltTable2` at .data+0xb00 and `fltTable1` at .data+0xb40 -- and finding
 * F826 measured that the two pairs are separate symbols and not aliases.
 *
 * "WHICH NOTHING WRITTEN REFERENCES" USED TO END THAT SENTENCE AND IS
 * RETRACTED.  `src/pump/v90/V90CPpck.cpp` defines the unsuffixed pair now and
 * `float2Bits(float, short *, int)` reads them; the four symbols are still
 * four, and the two files must not be merged.  A reference to one of these
 * two names from anywhere but this file is a defect: check the relocation,
 * not the spelling.
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
 * that rounded UP to the next strictly greater multiple of `12 * bitsPerSymbol`
 * with the gap zero-filled.  This function is what settled which is which; the
 * header carries the argument.  The rounding is an unsigned `div`, so
 * `bitsPerSymbol == 0` divides by zero here as it does in the object, and the
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
			bitsPerSymbol = 1;

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

	quantum = 12u * bitsPerSymbol;
	vectorLen = ((unsigned int)word_11c / quantum + 1) * quantum;

	for (i = (unsigned int)word_11c; i < vectorLen; i++)
		bits[i] = 0;

	if (e != 0)
		word_110 = 1;
}

/*
 * ===========================================================================
 * binaryTable (.data+0x6a40, 64 bytes)
 *
 * THE RULE IS `binaryTable[i] == 1 << i`, i = 0..15, and it reproduces the
 * blob's sixteen words byte for byte:
 *
 *     6a40  01000000 02000000 04000000 08000000
 *     6a50  10000000 20000000 40000000 80000000
 *     6a60  00010000 00020000 00040000 00080000
 *     6a70  00100000 00200000 00400000 00800000
 *
 * -- little-endian 1, 2, 4, ... 0x8000, with no gap and no repeat, which is
 * what separates it from `fltTable_2` two symbols along, whose missing 2^-8
 * is why THAT one is transcribed and not generated.  There are no relocations
 * anywhere in the range, so these are the integers they look like and not a
 * pointer table that objdump has flattened.
 *
 * FIVE RELOCATIONS REFERENCE IT IN THE WHOLE OBJECT and all five are inside
 * `V92CP::evaluateInfo`, which is why it lives here.  It also sits
 * immediately after `fltTable_1`, the last `.data` symbol this file already
 * defines, and `.data` follows link order exactly as `.text` does.
 *
 * Not `const`: a namespace-scope `const` array has internal linkage in C++
 * and this is a GLOBAL `D` symbol, the same argument the two float tables
 * carry.  `int` rather than `unsigned` is NOT forced -- every use is one
 * operand of a 32-bit `imul` against a byte, where the two readings agree
 * over the whole range the table holds.
 * ===========================================================================
 */
int binaryTable[16] = {
	1 <<  0,	1 <<  1,	1 <<  2,	1 <<  3,
	1 <<  4,	1 <<  5,	1 <<  6,	1 <<  7,
	1 <<  8,	1 <<  9,	1 << 10,	1 << 11,
	1 << 12,	1 << 13,	1 << 14,	1 << 15
};

/*
 * ===========================================================================
 * V92CP::evaluateInfo (.text+0x4f400, 1,124 bytes)
 *
 * The receive half's decoder, and the exact inverse of `infoToBits`: one
 * switch over `word_114` and nothing else, with each arm lifting ONE BLOCK of
 * the message back out of `bits` into the fields at +0x000..+0x10c.  The
 * dispatch is `sub $0x3; cmp $0x5; ja` over a six-entry table at
 * .rodata+0xe0c whose entries are 0x4f420, 0x4f45f, 0x4f508, 0x4f56d,
 * 0x4f7cc and 0x4f467.  The second is the function's own `ret`, so state 4 is
 * a HOLE in the case list and not an arm that does nothing -- the same
 * reading V90CP::evaluateInfo's nine-entry table gets, and for the same
 * reason.
 *
 * EVERY BIT INDEX BELOW WAS READ OUT OF THIS FUNCTION AND THEN CHECKED
 * AGAINST `infoToBits` ABOVE, which is the strongest cross-check available
 * short of the differential test: the two were reconstructed from different
 * addresses at different times and they agree on all of
 *
 *     bits[27..31] word_104   bits[32] suv        bits[33] byte_04
 *     bits[19,20]  char_01    bits[21..25] char_02
 *     bits[31,32]  word_08    bits[35] byte_03    bits[49,50] word_0c
 *     bits[52..67] flt_10     bits[69..76] flt_14 bits[77..84] flt_18
 *     bits[86..93] flt_1c     bits[94..101] flt_20
 *     bits[103..118] word_28[0..3]              bits[120..127] word_28[4..5]
 *     bits[128] byte_24
 *
 * ONE FIELD DOES NOT ROUND TRIP, and it is the mask blocks.  `infoToBits`
 * writes each sixteen-bit mask word LEAST significant bit first (`*p++ =
 * s & 1; s >>= 1`); this function reads it back MOST significant first
 * (`binaryTable[15 - i]` with i ascending).  Every other field agrees.
 * docs/deviations.md D920.
 *
 * THE WEIGHTS ARE A TABLE, NOT A SHIFT.  Where V90CP::evaluateInfo
 * accumulates `(acc << 1) | (bits[q] & 1)` walking DOWNWARDS, this one
 * accumulates `bits[p] * binaryTable[w]` walking UPWARDS.  That is the
 * object's own difference between the two classes -- `imul 0x0(,%ecx,4)`
 * against `binaryTable` at five sites here and not one anywhere in V90CP --
 * and it is not carried across by analogy in either direction.
 *
 * WHERE THE ACCUMULATOR LIVES IS ALSO THE OBJECT'S, and it differs between
 * arms in a way that is observable rather than cosmetic.  `word_28` is
 * accumulated IN PLACE -- `mov 0x28(%edi,%esi,4),%eax; add %eax,%edx; mov
 * %edx,0x28(...)`, a reload every iteration -- while the two mask arms hold
 * the sum in a register and store once at the end, over a destination that
 * has already been zeroed.  The difference only shows when the destination
 * aliases something the loop reads, or the loop bound, and `word_10c` is
 * unbounded (D570), so it can: `short_42[12][5]` IS `word_10c`, and
 * `short_42[22][0]` is inside `bits`.  Reproduced as read.
 *
 * `word_124` IS THE READ CURSOR and this is its only user; see V92CP.h.
 * ===========================================================================
 */
void
V92CP::evaluateInfo()
{
	int i;
	unsigned int j, k, n;

	switch (word_114) {
	case 3:
		/*
		 * The short form.  Five bits of `word_104`, then the two whole
		 * bytes, and the cursor is not moved: every index here is an
		 * absolute displacement in the object.
		 */
		word_104 = 0;
		for (i = 0; i <= 4; i++)
			word_104 += bits[27 + i] * binaryTable[i];

		suv = bits[32];
		byte_04 = bits[33];
		break;

	case 5:
		/*
		 * The header.  `char_01` is two bits and `char_02` five, and
		 * `word_08` is present only for the long form -- the object's
		 * `dec %bl; jg` is a SIGNED test of `char_01`, which is what
		 * makes that field a `signed char` here and in `infoToBits`.
		 */
		char_01 = (signed char)((bits[19] & 1) | (bits[20] << 1));
		byte_118 = (unsigned char)char_01;

		char_02 = 0;
		for (i = 25; i > 20; i--)
			char_02 = (signed char)((char_02 << 1) |
						(bits[i] & 1));

		if (char_01 <= 1)
			word_08 = (bits[31] & 1) | (bits[32] << 1);

		byte_04 = bits[33];
		break;

	case 6: {
		/*
		 * The fixed part of the long form: two small fields, the five
		 * floats, the six four-bit counts, and the group count that
		 * sizes everything after it.  This is the only arm that seeds
		 * the cursor -- `mov $0x34,%ecx; mov %ecx,0x124(%edi)` -- and
		 * 52 is exactly where `infoToBits` puts the first magnitude
		 * bit.
		 */
		int m;
		int max;
		float f;

		byte_03 = bits[35];
		word_0c = (bits[49] & 1) | (bits[50] << 1);

		word_124 = 52;

		/*
		 * `flt_10` alone: sixteen magnitude entries and no sign, the
		 * heaviest weight at the HIGHEST index, so reading forwards
		 * takes the table backwards.
		 */
		f = 0.0f;
		for (i = 0; i <= 15; i++) {
			if (bits[word_124] != 0)
				f += fltTable_2[15 - i];
			word_124++;
		}
		flt_10 = f;

		word_124++;		/* the framing position at 68 */

		/*
		 * THE FOUR SIGNED MAGNITUDES ARE TWO LOOPS OF TWO, and the
		 * object says so: it stores through `0x14(%edi,%esi,4)` and
		 * `0x1c(%edi,%esi,4)` with %esi running 0 then 1, which is a
		 * variable index and therefore an array in the source.  The
		 * header keeps four scalars all the same -- turning them into
		 * `flt_14[2]` and `flt_1c[2]` would rename fifty sites across
		 * two RECORDED mutation snapshots, and no tier here can tell
		 * the two spellings apart.  The `if (m == 0)` is the cost of
		 * that and is the one place in this function written for the
		 * header rather than from the object.
		 *
		 * The sign entry follows the seven magnitude entries, and the
		 * object applies it as `xor $0x80000000` on the bit pattern --
		 * GCC 3.4.2's x87 spelling of `-f`, and not a separate
		 * operation.  It flips the sign of a zero too, so a magnitude
		 * of zero with the sign entry set decodes to -0.0f.
		 */
		for (m = 0; m <= 1; m++) {
			f = 0.0f;
			for (i = 0; i <= 6; i++) {
				if (bits[word_124] != 0)
					f += fltTable_1[6 - i];
				word_124++;
			}
			if (bits[word_124] != 0)
				f = -f;
			word_124++;

			if (m == 0)
				flt_14 = f;
			else
				flt_18 = f;
		}

		word_124++;		/* the framing position at 85 */

		for (m = 0; m <= 1; m++) {
			f = 0.0f;
			for (i = 0; i <= 6; i++) {
				if (bits[word_124] != 0)
					f += fltTable_1[6 - i];
				word_124++;
			}
			if (bits[word_124] != 0)
				f = -f;
			word_124++;

			if (m == 0)
				flt_1c = f;
			else
				flt_20 = f;
		}

		word_124++;		/* the framing position at 102 */

		/*
		 * The six four-bit counts, in two runs because the framing
		 * position at 119 falls between them, and `word_10c` is the
		 * LARGEST of the six plus one.  The maximum starts at zero and
		 * the comparison is signed (`cmp %ebp,%edx; jle`), so a block
		 * of all-zero counts still asks for one group.
		 */
		max = 0;

		for (k = 0; k <= 3; k++) {
			word_28[k] = 0;
			for (i = 0; i <= 3; i++) {
				word_28[k] += bits[word_124] * binaryTable[i];
				word_124++;
			}
			if (word_28[k] > max)
				max = word_28[k];
		}

		word_124++;		/* the framing position at 119 */

		for (k = 4; k <= 5; k++) {
			word_28[k] = 0;
			for (i = 0; i <= 3; i++) {
				word_28[k] += bits[word_124] * binaryTable[i];
				word_124++;
			}
			if (word_28[k] > max)
				max = word_28[k];
		}

		word_10c = (unsigned short)(max + 1);

		byte_24 = bits[word_124];
		word_124++;

		/*
		 * And then seven positions are skipped, which lands the cursor
		 * on 136 -- `word_11c`'s value at the same point in
		 * `infoToBits`, and 8 * 17, the start of a group.
		 *
		 * THE OBJECT SPELLS THIS AS A LOOP WITH NOTHING LEFT IN IT:
		 * `mov $0x6,%eax; dec %eax; jns .-2` beside a `lea 0x7(%edx)`
		 * that does all the work.  That is what a seven-iteration
		 * source loop looks like once GCC 3.4.2 has strength-reduced
		 * the induction variable and declined to delete the empty
		 * shell.  Written as the addition, because the empty loop is
		 * unobservable and an empty loop in the source would read as a
		 * defect.  docs/deviations.md D921.
		 */
		word_124 += 7;
		break;
	}

	case 7: {
		/*
		 * The first mask block: `word_10c` groups of eight sixteen-bit
		 * words, each word its own seventeen-position frame -- one
		 * framing position and then the sixteen bits.
		 *
		 * THE BOUND IS TAKEN ONCE.  The object loads `word_10c` before
		 * the loop and keeps it in a stack slot, so a store into
		 * `short_42[12][5]` -- which IS `word_10c`, the two being 0xca
		 * apart -- does not change the number of groups.
		 * `infoToBits` reads it into a local the same way.
		 */
		n = word_10c;

		for (k = 0; k < n; k++) {
			for (j = 0; j <= 7; j++) {
				int acc = 0;

				word_124++;
				short_42[k][j] = 0;

				for (i = 0; i <= 15; i++) {
					/*
					 * D920 -- the mask word comes back
					 * bit-reversed.  `infoToBits` emits bit 0
					 * first; this gives that same position
					 * weight 2^15, so a mask does not survive
					 * a round trip through the class.
					 *
					 * EFFECT: every Ucode in a received mask
					 * is mirrored -- Ucode n is read as
					 * Ucode 15 - n within its chord.
					 *
					 * FIX: ascending, as every other field in
					 * this class already is.
					 *
					 * EVIDENCE: ITU-T V.90 Table 14 and V.92
					 * Table 23 -- "bit 137 corresponds to
					 * Ucode 0", "Bit 0 is transmitted first"
					 * -- fix the WIRE; our own consumer
					 * getConstellationMask fixes the STORAGE,
					 * `mask[v >> 4] |= 1 << (v & 15)`.  Both
					 * make bit j of word k Ucode 16k + j and
					 * both convict the reader.  Finding F6800.
					 */
#ifdef DSPLIB_REPRODUCE_BUGS
					acc += bits[word_124] *
					       binaryTable[15 - i];
#else
					acc += bits[word_124] *
					       binaryTable[i];
#endif
					word_124++;
				}

				short_42[k][j] = (short)acc;
			}
		}
		break;
	}

	case 8: {
		/* The second mask block, sent only when `byte_24` is set. */
		n = word_10c;

		for (k = 0; k < n; k++) {
			for (j = 0; j <= 7; j++) {
				int acc = 0;

				word_124++;
				short_a2[k][j] = 0;

				for (i = 0; i <= 15; i++) {
					/* The second mask block, D920 again.  Finding F6800. */
#ifdef DSPLIB_REPRODUCE_BUGS
					acc += bits[word_124] *
					       binaryTable[15 - i];
#else
					acc += bits[word_124] *
					       binaryTable[i];
#endif
					word_124++;
				}

				short_a2[k][j] = (short)acc;
			}
		}
		break;
	}

	default:
		break;
	}
}

/*
 * The author's own words, from .rodata.str1.4+0xd534, reproduced byte for
 * byte -- "recieved" included.  One referrer, the bad-CRC arm below, and it
 * is the only string in either of the two members reconstructed here.  It is
 * also the strongest evidence in this file for what state 9 is: the object
 * says in plain text that what has just been checked is a received CP
 * message's CRC.
 */
#define V92CP_BADCRC	"V92CP: recieved CP with bad CRC\r\n"

/*
 * ===========================================================================
 * V92CP::bitsToInfo (.text+0x4f870, 1,957 bytes)
 *
 * One bit in, one answer out: the detector and the state machine that drive
 * `evaluateInfo`.  Eleven states, dispatched `cmp $0xa; ja` over a table at
 * .rodata+0xe24, and every state that fills a block of `bits` calls
 * `evaluateInfo` to decode it -- four call sites, and the only four
 * relocations against that symbol in the object.
 *
 * THE RETURN TYPE IS `int` AND THE VALUE IS CONSTRUCTED.  All three `ret`s
 * are reached through `mov %edi,%eax`, %edi is zeroed on entry and assigned
 * on five paths, and the epilogue then MASKS it -- which is not worth doing
 * to a leftover.  Same argument as `evaluateCRC`, and as
 * `V90CP::bitsToInfo`, about which the opposite mistake was once made.
 *
 * WHAT THE FIVE ANSWERS MEAN IS NOT ESTABLISHED and they are left as numbers.
 * 5 is "the far end has stopped" -- a run of `12 * bitsPerSymbol` zeros
 * arriving while the cursor is still at its home 18 -- and it does NOT stop
 * the state machine below, which runs on and can overwrite it.  1..4 are the
 * four combinations of the two bits that survive the whole message,
 * `byte_00`, which picks the short form, and `byte_04`:
 *
 *      byte_00 != 1, byte_04 == 0 -> 1     byte_00 != 1, byte_04 != 0 -> 2
 *      byte_00 == 1, byte_04 == 0 -> 3     byte_00 == 1, byte_04 != 0 -> 4
 *
 * -- which is the shape `V90CP::bitsToInfo` has and which finding F4360
 * declined to name there.  Declined here for the same reason: nothing in the
 * object, the one string it carries included, names any of them.
 *
 * THE TWO STATICS ARE FUNCTION-LOCAL AND IN .bss, mangled
 * `_ZZN5V92CP10bitsToInfoEhE5gamma` at +0x0 and `...E5delta` at +0x4, so
 * their C++ names are the author's.  THEIR ADDRESSES DO NOT FIX A
 * DECLARATION ORDER and finding F6610 measured that: GCC 3.4.2 lays them out
 * the same way whichever order they are declared in, and the way it lays them
 * out is not the blob's.  Each holds the bit length of one mask
 * block, computed once `evaluateInfo` has decoded the group count and
 * compared against `word_120` while the block arrives:
 *
 *      gamma = 136 * word_10c     set entering state 7, tested in state 7
 *      delta = 136 * word_10c     set entering state 8, tested in state 8
 *
 * 136 is 8 * 17 -- eight mask words to the group, seventeen positions to the
 * word -- and the object spells it `shl $4; add; shl $3`.  The two are taken
 * from the same field at different moments, so they differ only if something
 * moved `word_10c` in between; `evaluateInfo` case 7 does not, and case 8 has
 * not run yet.  Their signedness is not established -- every use is an
 * equality compare -- and they are spelled to match V90CP's `alpha` and
 * `beta`, which is a convention and not a measurement.
 *
 * `resetDetector`, `resetCRC`, `calcCRC` and `evaluateCRC` are all INLINED by
 * the object -- five, one, one and one site, and no relocation against any of
 * them anywhere in the range -- which is finding F4600's shape, and is what
 * makes the CRC arm four hundred bytes of shift register in the middle of a
 * state machine.  Called here; GCC 3.4.2 at -O3 folds them back in.
 * ===========================================================================
 */
/*
 * D923 -- `bitsToInfo` stores into `bits[word_11c]` at seven sites and the
 * object guards none of them, while `word_11c` advances on every call in
 * states 2 to 10 with no upper bound anywhere.
 *
 * EFFECT: a stream staying in one collecting state past 2,000 positions writes
 * through `crc`, `vectorLen`, `msgLen` and `word_914` and then off the end of
 * the 0x918-byte allocation.  ITU-T V.90 Table 14 and V.92 Table 23 bound each
 * of the six constellation indices to "an integer between 0 and 5", so a
 * CONFORMANT peer reaches about 1,786 of 2,000 and cannot get there; an index
 * of 6 to 15 -- which the recommendations forbid and nothing here rejects --
 * can.  The exposure is to a malformed or hostile peer.
 *
 * FIX: refuse the store above the last index that fits, exactly as the sibling
 * does.  `V90CP::bitsToInfo` carries `cmp $0x2edf` / `ja` at five of its ten
 * store sites and prints the author's own "not enouch memory in the buffer"
 * instead, so the guard, its shape and its message are the original author's;
 * they are simply absent from the V.92 class.  That string is already in the
 * blob, pooled in .rodata, so it is not an invention.
 *
 * EVIDENCE: finding F6800 (the recommendations, and the translation verified
 * faithful at .text+0x4f2f3 and +0x4f7f4 before anything was attributed), D923,
 * and D520 / finding F4361 for the sibling's guard.
 *
 * Under DSPLIB_REPRODUCE_BUGS the macro is the object's bare store, so the
 * reproduce build is unchanged instruction for instruction.
 */
#define V92CP_NOMEM \
	"\n *** error CP bit , not enouch memory in the buffer *** \n"

#ifdef DSPLIB_REPRODUCE_BUGS
#define V92CP_PUT_BIT(b)						\
	do {							\
		bits[word_11c] = (b);				\
		word_11c++;					\
	} while (0)
#else
#define V92CP_PUT_BIT(b)						\
	do {							\
		if (word_11c <= V92CP_BITS - 1) {		\
			bits[word_11c] = (b);			\
			word_11c++;				\
		} else if (DSPLIB_DEBUG_ON()) {			\
			dsplibs_debug_printf(V92CP_NOMEM);	\
		}						\
	} while (0)
#endif

int
V92CP::bitsToInfo(unsigned char bit)
{
	/*
	 * Declared gamma first because that is the order they occupy in the
	 * blob's .bss -- gamma at +0x0, delta at +0x4 -- and NOT because the
	 * declaration order puts them there.  MEASURED, both ways round: GCC
	 * 3.4.2 emits `delta` at +0x0 and `gamma` at +0x4 whichever order the
	 * declarations are in, so the blob's order is not recoverable from
	 * this and the source is written to document it rather than to
	 * reproduce it.  Nothing observes either offset.  Finding F6610.
	 */
	static unsigned int gamma;
	static unsigned int delta;

	unsigned int frameBits;
	int rc = 0;

	/*
	 * THE RUN COUNTERS COME FIRST and are independent of the state: every
	 * bit lengthens one run and clears the other.  `byte_11a` is read
	 * back out of the object rather than out of a register -- the object
	 * stores 0 and reloads it four instructions later -- which matters
	 * when `bitsPerSymbol` is zero, because the quantum is then zero and
	 * the test is true on a ONE bit as well.
	 */
	if (bit != 0) {
		byte_119++;
		byte_11a = 0;
	} else {
		byte_119 = 0;
		byte_11a++;
	}

	/*
	 * Spelled `frameBits` and not `quantum`, which is what `infoToBits`
	 * calls the same product: test/mutations/v92info.json anchors a
	 * recorded mutation on the exact text `\tquantum = 12u *
	 * bitsPerSymbol;`, and a second occurrence in this file would make
	 * that anchor ambiguous and fail `mutsnap.py --check` for a suite
	 * this batch has no other reason to touch.
	 */
	frameBits = 12u * bitsPerSymbol;

	if (byte_11a == frameBits && word_11c == 18)
		rc = 5;

	switch (word_114) {
	case 0:
		/* Seventeen ones is the preamble; sixteen are not enough. */
		if (byte_119 > 16)
			word_114 = 1;
		break;

	case 1:
		/* The framing zero, or start again. */
		if (bit == 0)
			word_114 = 2;
		else
			resetDetector();
		break;

	case 2:
		/*
		 * The type bit, at index 18, stored whole into `byte_00` and
		 * tested against ONE -- which is `infoToBits`'s own test of
		 * the same field.  It picks the short form's state 3 or the
		 * long form's 5.  State 4 is `evaluateInfo`'s hole and nothing
		 * ever enters it.
		 */
		byte_00 = bit;
		V92CP_PUT_BIT(bit);
		word_114 = (bit == 1) ? 3 : 5;
		break;

	case 3:
		/*
		 * The short form: on to index 34, which is where `infoToBits`
		 * leaves `word_11c` for the same message.
		 */
		V92CP_PUT_BIT(bit);
		if (word_11c == 34) {
			evaluateInfo();
			word_114 = 9;
			word_120 = 0;
		}
		break;

	case 4:
		/*
		 * Reachable from nothing -- state 2 chooses 3 or 5, and no
		 * other arm writes 4 -- but it is a case and not the default:
		 * the table's fifth entry is a block of its own rather than
		 * the block `ja` falls through to.
		 */
		break;

	case 5:
		/*
		 * The long form's header, to the same index 34.  `char_01` has
		 * been decoded by then and picks what comes next.
		 */
		V92CP_PUT_BIT(bit);
		if (word_11c == 34) {
			evaluateInfo();
			word_114 = (char_01 > 1) ? 9 : 6;
			word_120 = 0;
		}
		break;

	case 6:
		/*
		 * The fixed part, to index 136 -- 8 * 17, and the value
		 * `infoToBits` sets `word_11c` to at the same point.
		 */
		V92CP_PUT_BIT(bit);
		if (word_11c == 136) {
			evaluateInfo();
			word_114 = 7;
			word_120 = 0;
			gamma = 136u * word_10c;
		}
		break;

	case 7:
		/*
		 * The first mask block, `gamma` positions of it, and then the
		 * second only if `byte_24` asked for one.
		 */
		V92CP_PUT_BIT(bit);
		word_120++;
		if ((unsigned int)word_120 == gamma) {
			evaluateInfo();
			word_114 = (byte_24 != 0) ? 8 : 9;
			word_120 = 0;
			delta = 136u * word_10c;
		}
		break;

	case 8:
		/* The second mask block, `delta` positions of it. */
		V92CP_PUT_BIT(bit);
		word_120++;
		if ((unsigned int)word_120 == delta) {
			evaluateInfo();
			word_114 = 9;
			word_120 = 0;
		}
		break;

	case 9:
		/*
		 * The closing frame: one framing position and sixteen CRC
		 * entries, seventeen in all.  `msgLen` is set one past the
		 * last of them, which is what `infoToBits` computes, and the
		 * register is then reset, clocked over the message and
		 * compared against what arrived.
		 *
		 * THAT IS `evaluateCRC` AND NOTHING ELSE.  This arm read
		 * `resetCRC(); calcCRC(); evaluateCRC();` until the mutation
		 * "the register is not reset before it is clocked" came back
		 * NOT CAUGHT: `evaluateCRC` does the reset and the clocking
		 * itself, so the first two were redundant and no input could
		 * tell.  Redundant AND wrong -- the object's arm is 660 bytes
		 * against `evaluateCRC`'s 668, which is ONE inline of it and
		 * not two of `calcCRC` plus one of the comparison.  Finding
		 * F6609.
		 *
		 * A BAD CRC RESTARTS THE DETECTOR, and says so.
		 */
		V92CP_PUT_BIT(bit);
		word_120++;
		if (word_120 == 17) {
			msgLen = (unsigned int)word_11c;

			if (evaluateCRC()) {
				word_114 = 10;
			} else {
				resetDetector();
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(V92CP_BADCRC);
			}
		}
		break;

	case 10:
		/*
		 * The message is complete and what is left is to land on a
		 * frame boundary.  Nothing is stored into `bits` here: the
		 * cursor advances alone, a ONE restarts the detector, and the
		 * answer is delivered when the cursor reaches a whole number
		 * of `12 * bitsPerSymbol` -- the same quantum `infoToBits`
		 * pads the transmitted vector out to.
		 *
		 * THE DIVISION IS UNSIGNED (`div`, not `idiv`), so a zero
		 * `bitsPerSymbol` divides by zero here exactly as it does in
		 * `infoToBits`.  docs/deviations.md D571 records it.
		 */
		word_11c++;
		if (bit != 0)
			resetDetector();

		if ((unsigned int)word_11c % frameBits == 0) {
			resetDetector();

			if (byte_00 == 1)
				rc = (byte_04 == 0) ? 3 : 4;
			else
				rc = (byte_04 == 0) ? 1 : 2;
		}
		break;

	default:
		break;
	}

	/*
	 * The hold-off, in full; see `word_914` in V92CP.h.  Answers 1 and 2
	 * start it from idle, and while it runs 3 and 4 are suppressed and 5
	 * is not.
	 */
	if (word_914 >= 0) {
		if (rc == 3 || rc == 4)
			rc = 0;

		word_914++;
		if (word_914 == 400)
			word_914 = -1;
	} else if (rc == 1 || rc == 2) {
		word_914 = 0;
	}

	return rc;
}
