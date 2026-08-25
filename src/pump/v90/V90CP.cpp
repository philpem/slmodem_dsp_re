/*
 * V90CP.cpp -- V.90 CP message: construction and destruction.
 *
 * Reconstructed from dsplibs.o V90CP.cpp.  Two of the class's fifteen
 * symbols: the constructor (0x51590, 193 bytes) and the destructor (0x51200,
 * 173 bytes).  `include/dsplib/V90CP.h` carries the object map and says which
 * member proved which offset.
 *
 * THE DESTRUCTOR IS THE INTERESTING ONE.  173 bytes against a 193-byte
 * constructor is not an empty class with a fat header: the constructor makes
 * six `sysdep_malloc(0x200)` calls and stores the results at +0xc88..+0xc9c,
 * and the destructor is six null-guarded `sysdep_free`s of exactly those six,
 * in the same order.  It does not clear them afterwards -- the object is left
 * holding six dangling pointers, which is visible in the differential test
 * because both sides leave them dangling identically.
 *
 * The guard is a real branch and not the compiler being careful: each pointer
 * is loaded, tested, and jumped over when null, so a reconstruction that
 * freed unconditionally would differ only in the null case.  The harness's
 * allocator counts `sysdep_free(NULL)` separately from a wild free, so
 * t_v90cp can see that difference without crashing.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x10(%esp),%ebx` after a push and an 8-byte frame -- not
 * %ecx, so nothing here needs an attribute (finding F215).
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here.
 * No virtuals, no static data members, no new/delete: the allocation goes
 * through the object's own `sysdep_malloc`, so the test binaries still link
 * with $(CC).
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90CP.h"

/*
 * THE REPLACEMENT `operator delete[]`, AND IT IS READ OFF THE OBJECT.  At a
 * destructor's LAST free the blob makes an ordinary `call sysdep_free` where
 * our explicit `if (p) sysdep_free(p)` makes a sibling `jmp` -- one
 * instruction fewer, and the sibcall drops the frame with it.  Eight spellings
 * were compiled and only `delete[]` reproduces the object's shape; finding
 * F7786 and `docs/method/refinement.md` lever 7 carry the enumeration.
 *
 * Behaviourally it is exactly the guard it replaces: the element type is a POD
 * with no destructor, so `delete[] p` is `if (p) operator delete[](p)` and
 * there is no array cookie to read.
 *
 * ONLY THE LAST FREE IN A DESTRUCTOR IS BYTE-EVIDENCE for this.  Away from
 * tail position the two spellings emit identically, so the others carry the
 * same spelling because a destructor written with `delete[]` uses it for every
 * member, not because the object distinguishes them.
 *
 * IT IS A LOCAL COPY AND NOT AN INCLUDE ON PURPOSE.  Hoisting this one
 * definition into `dsplib/sysdep.h` -- which every one of these files already
 * reaches transitively -- moved it earlier in the translation unit and cost
 * EIGHT destructors their byte identity, `FloatFIR` and `FloatARMA` among
 * them.  That is refinement.md lever 3 with an inline function as the carrier,
 * and finding F7815 is the measurement.
 */
inline void operator delete[](void *p) { sysdep_free(p); }

/*
 * Hold the compiler to the map in the header.  tools/offcheck.py does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class has to assert its own -- and it is exactly the check that
 * catches an object right in size and wrong by four in every offset.
 *
 * Guarded on the pointer width because `buf[6]` is 24 bytes in the 32-bit
 * build the blob is and 48 in the 64-bit syntax check `make check64` runs;
 * every offset past +0xc88 moves with it and the class is 24 bytes longer.
 */
#if __SIZEOF_POINTER__ == 4
#define V90CP_OFF(field, off, tag) \
	typedef char v90cp_off_##tag[ \
	    ((int)__builtin_offsetof(V90CP, field) == (off)) ? 1 : -1]

V90CP_OFF(word_00,		0x0000, word00);
V90CP_OFF(word_04,		0x0004, word04);
V90CP_OFF(word_08,		0x0008, word08);
V90CP_OFF(word_0c,		0x000c, word0c);
V90CP_OFF(byte_10,		0x0010, byte10);
V90CP_OFF(byte_11,		0x0011, byte11);
V90CP_OFF(byte_12,		0x0012, byte12);
V90CP_OFF(byte_13,		0x0013, byte13);
V90CP_OFF(word_14,		0x0014, word14);
V90CP_OFF(word_18,		0x0018, word18);
V90CP_OFF(nof_58,		0x0048, nof58);
V90CP_OFF(short_58,		0x0058, short58);
V90CP_OFF(nof_buf,		0x0c58, nofbuf);
V90CP_OFF(word_c70,		0x0c70, wordc70);
V90CP_OFF(buf,			0x0c88, buf);
V90CP_OFF(word_ca0,		0x0ca0, wordca0);
V90CP_OFF(byte_ca8,		0x0ca8, byteca8);
V90CP_OFF(word_cb4,		0x0cb4, wordcb4);
V90CP_OFF(word_ca4,		0x0ca4, wordca4);
V90CP_OFF(byte_ca9,		0x0ca9, byteca9);
V90CP_OFF(byte_caa,		0x0caa, bytecaa);
V90CP_OFF(word_cac,		0x0cac, wordcac);
V90CP_OFF(word_cb0,		0x0cb0, wordcb0);
V90CP_OFF(bits,			0x0cb8, bits);
V90CP_OFF(crc,			0x3b98, crc);
V90CP_OFF(word_3ba8,		0x3ba8, word3ba8);
V90CP_OFF(word_3bac,		0x3bac, word3bac);
V90CP_OFF(word_3bb0,		0x3bb0, word3bb0);
V90CP_OFF(nofRecievedMp,	0x3bb4, nofmp);
V90CP_OFF(nofRecievedMpNot,	0x3bb8, nofmpnot);
V90CP_OFF(word_3bbc,		0x3bbc, word3bbc);
typedef char v90cp_size[(sizeof(V90CP) == 0x3bc0) ? 1 : -1];
#endif

/*
 * Six buffers, then the detector's five fields, the two frame counters, the
 * byte at +0x13 that only this member ever clears, and -1 at +0x3bbc.
 *
 * The five detector stores are `resetDetector`'s whole body -- but the
 * constructor holds no relocation against that symbol, and a call to a GLOBAL
 * in `.text` would carry one (findings F306, F333), so the original repeated
 * the assignments here rather than calling it.  Finding F1237, and it stands.
 *
 * WHAT 1237 DID NOT SETTLE WAS THE ORDER, and this batch did.  It used to
 * read +0xca4, +0xca9, +0xcaa, +0xcac, +0xcb0, which is the order the object
 * does NOT encode; +0xcac, +0xcb0, +0xca4, +0xca9, +0xcaa is, and with it the
 * constructor's mnemonic sequence became the blob's -- register allocation
 * apart, which `compare.py` does not compare.  The same order made
 * `resetDetector` and `reset` byte-for-byte identical to the object, operands
 * included, which is 617's acceptance test and not a store-order hint.  Three
 * symbols moved on one reordering, so it is the source order and not a
 * coincidence of scheduling.  Finding F4600.
 */
V90CP::V90CP()
{
	/*
	 * Six allocations written out, which is the object's shape.  Rolled,
	 * this is:
	 *     for (i = 0; i < 6; i++)
	 *             buf[i] = (int *)sysdep_malloc(V90CP_BUFSIZE);
	 */
	buf[0] = (int *)sysdep_malloc(V90CP_BUFSIZE);
	buf[1] = (int *)sysdep_malloc(V90CP_BUFSIZE);
	buf[2] = (int *)sysdep_malloc(V90CP_BUFSIZE);
	buf[3] = (int *)sysdep_malloc(V90CP_BUFSIZE);
	buf[4] = (int *)sysdep_malloc(V90CP_BUFSIZE);
	buf[5] = (int *)sysdep_malloc(V90CP_BUFSIZE);

	word_cac = 18;
	word_cb0 = 0;
	word_ca4 = 0;
	byte_ca9 = 0;
	byte_caa = 0;

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;

	byte_13 = 0;
	word_3bbc = -1;
}

/*
 * And back, in the same order.  Nothing else: the destructor touches no field
 * but the six pointers, and only reads them.
 */
V90CP::~V90CP()
{
	delete[] buf[0];
	delete[] buf[1];
	delete[] buf[2];
	delete[] buf[3];
	delete[] buf[4];
	delete[] buf[5];
}

/*
 * getBitVector -- 0x519d0, 22 bytes, and the member that PROVES where `bits`
 * starts.  Six instructions: load `this`, load the reference, read +0x3bac
 * into it, `add $0xcb8,%eax` and return.  The length it reports is the one
 * `calcSequenceLength` computed, not the array's own extent -- see the
 * comment on V90CP_BITS in the header for why those are different questions.
 */
unsigned char *
V90CP::getBitVector(unsigned int &length)
{
	length = word_3bac;
	return bits;
}

/*
 * resetDetector -- 0x51510, 46 bytes, and its whole body is five stores.
 * OURS IS BYTE-FOR-BYTE THE OBJECT'S, operands included, which is what
 * settles the order: +0xcac, +0xcb0, +0xca4, +0xca9, +0xcaa.  See the
 * constructor above for what that order also repaired, and finding F4600.
 *
 * 18 is the write cursor's home: one preamble frame of seventeen bits, then
 * the next frame's framing bit at 17 and its first data bit at 18.
 */
void
V90CP::resetDetector()
{
	word_cac = 18;
	word_cb0 = 0;
	word_ca4 = 0;
	byte_ca9 = 0;
	byte_caa = 0;
}

/*
 * reset -- 0x51540, 73 bytes.  `resetDetector`'s five stores, then the two
 * frame counters and -1 at +0x3bbc.
 *
 * IT REALLY CALLS `resetDetector`, and the compiler really inlines it.  The
 * object holds no call and no relocation here, which by itself is equally
 * consistent with the assignments being repeated a third time -- so the
 * question was settled by writing the call and measuring: at `-O3` GCC 3.4.2
 * folds it in and the 73 bytes it emits are the object's, instruction for
 * instruction and operand for operand.  The constructor above does NOT get
 * the same treatment from us, because that is 1237's ruling and this measures
 * nothing about it either way.  Finding F4600.
 *
 * WHAT SEPARATES IT FROM THE CONSTRUCTOR is `byte_13`, which the constructor
 * clears and this does not.  That is the only field of the two the object
 * treats differently, and it is why the header says +0x13 is cleared by the
 * constructor "and by NOTHING else".
 */
void
V90CP::reset()
{
	resetDetector();

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;

	word_3bbc = -1;
}

/*
 * calcSequenceLength -- 0x521c0, 101 bytes.  Round +0x3bb0 + 1 up to a whole
 * number of +0x3ba8 and leave it at +0x3bac.
 *
 * The object divides ONCE and then multiplies back to test exactness --
 * `div %ecx` then `imul %eax,%edx` then `cmp %edi,%edx` -- rather than
 * testing the remainder `div` has already left in %edx.  That is forced by
 * the source shape and not a choice the compiler made for us: a `%` here
 * would have reused %edx and needed no `imul` at all.  Both arms store the
 * product, which is why the equal arm has its own `mov` to +0x3bac.
 *
 * `infoToBits` carries this same computation inlined; that copy is left
 * exactly where it is.  The object holds both too -- this symbol and the
 * inlined arithmetic at 0x52230 -- which is finding F3532's shape.
 *
 * THE CONDITION IS `group * quot`, NOT `quot * group`, AND THAT ORDER IS THE
 * LAST TWO BYTES.  `imul r,r` takes its destination from whichever operand was
 * written first, so the object's `mov %ecx,%edx; imul %eax,%edx` is
 * `group * quot` and ours was `mov %eax,%edx; imul %ecx,%edx`.  All four
 * spellings of (condition order x then-arm order) were compiled: both with
 * `group * quot` in the CONDITION are byte-identical and both with
 * `quot * group` differ in exactly those two bytes, so what is decoded is the
 * CONDITION's order and the then-arm's is CSE'd away and undetermined.  The
 * blob confirms it at a second site: `infoToBits` carries this arithmetic
 * inlined at 0x52230 and spells it `mov %ecx,%edi; div %ecx; imul %eax,%edi`,
 * the same way round.  Finding F7981.
 *
 * Everything is unsigned: `div`, not `idiv`.
 */
void
V90CP::calcSequenceLength()
{
	unsigned int total, group, quot;

	total = word_3bb0 + 1;
	group = word_3ba8;
	quot = total / group;
	if (group * quot == total)
		word_3bac = group * quot;
	else
		word_3bac = (quot + 1) * group;
}

/*
 * printNofRecievedMpMpNot -- 0x53680, 56 bytes, and the ONLY string this
 * class reaches that names a field of it.
 *
 * IT PRINTS "V90MP", NOT "V90CP".  The format at .rodata.str1.4+0xd6b0 is the
 * very same string `V90MP::printNofRecievedMpMpNot` uses -- one pooled copy,
 * two referrers -- so the author copied the line across and left the tag
 * wrong.  We reproduce the object, so the tag stays wrong here; changing it
 * would be a difference the differential test cannot see and the string table
 * can.
 *
 * The argument order is the object's: +0x3bb4 goes into the first `%d` and
 * +0x3bb8 into the second, which is what names `nofRecievedMp` and
 * `nofRecievedMpNot` and is the strongest evidence tier in the class.
 *
 * The gate is `cmpl $0x1,dsplibs_debug_level; ja`, which is
 * `DSPLIB_DEBUG_ON()`, and the call relocates against `dsplibs_debug_printf`
 * directly rather than through the encoder, so below the gate it says nothing
 * at all.
 */
void
V90CP::printNofRecievedMpMpNot()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90MP: received %d MP, %d MPNot\r\n",
				     nofRecievedMp, nofRecievedMpNot);
}

/*
 * calcCRC -- 0x512d0, 570 bytes.  The WRITE side of the CRC register, and the
 * same CCITT shift register `evaluateCRC` runs on the read side: taps out of
 * positions 4 and 11 into 3 and 10, and the feedback bit into 15.
 *
 * TWO THINGS SEPARATE IT FROM `evaluateCRC`, and both are absences.  It does
 * not seed the register -- there is no store of 1 anywhere in it, so it
 * continues from whatever `resetCRC` (or the previous call) left -- and it
 * does not compare anything afterwards, so it returns nothing.  The extent
 * and the frame skip are identical: information bits run from 0x12 up to
 * `word_3bb0 - 0x11`, and every index that is a multiple of seventeen is a
 * framing bit and is stepped over.
 *
 * The whole register lives in the sixteen bytes of the object's stack frame
 * for the duration of the loop and is written back at the end.  That is
 * register promotion the compiler is free to do and we do not encode: our
 * source touches `crc[]` directly, exactly as `evaluateCRC`'s does.
 *
 * The `if (i % 17 == 0) i++` is `mul $0xf0f0f0f1` / `shr $4` for the divide
 * and `cmp $1` / `adc $0` for the branchless increment.  Unsigned throughout:
 * `jae` and `jb` on the bounds.
 */
void
V90CP::calcCRC()
{
	unsigned int i, end;
	unsigned char a;

	end = word_3bb0 - 0x11;
	for (i = 0x12; i < end; ) {
		if (i % 17 == 0)
			i++;
		a = (unsigned char)((crc[0] + bits[i]) & 1);
		i++;

		crc[0] = crc[1];
		crc[1] = crc[2];
		crc[2] = crc[3];
		crc[3] = (unsigned char)((crc[4] + a) & 1);
		crc[4] = crc[5];
		crc[5] = crc[6];
		crc[6] = crc[7];
		crc[7] = crc[8];
		crc[8] = crc[9];
		crc[9] = crc[10];
		crc[10] = (unsigned char)((crc[11] + a) & 1);
		crc[11] = crc[12];
		crc[12] = crc[13];
		crc[13] = crc[14];
		crc[14] = crc[15];
		crc[15] = a;
	}
}

/*
 * resetCRC -- 0x512b0, 32 bytes.  Sixteen ones, one per bit of the register,
 * written by a counted loop the object leaves rolled: `mov $0x1,%cl` hoisted
 * out, `inc %eax`, `cmp $0xf,%eax`, `jle`.  The `<= 0xf` is the object's own
 * bound and is why this is written that way rather than `< 16`.
 */
void
V90CP::resetCRC()
{
	int i;

	for (i = 0; i <= 0xf; i++)
		crc[i] = 1;
}

/*
 * evaluateInfo -- 0x519f0, 1986 bytes.  ONE SWITCH OVER `word_ca4` and
 * nothing else: `sub $0x3` / `cmp $0x8` / `jmp *0xe50(,%eax,4)`, so the arms
 * are 3..11 and everything outside falls through to the same `ret`.  The
 * table's nine entries are 0x51d3a, 0x51af0, 0x51d56, 0x51de8, 0x51a14,
 * 0x51e88, 0x51af0, 0x52031, 0x51af8 -- 0x51af0 is the `ret`, which is how
 * we know 4 and 9 are holes in the case list and not arms that do nothing.
 *
 * Every arm decodes ONE BLOCK of the CP message and leaves `word_cb4`, the
 * read cursor, one below the next frame's framing bit.  Each field is read
 * most-significant bit first, walking DOWNWARDS -- `movzbl 0xcb8(%ecx,%esi,1)`
 * with `dec %ecx` -- which is the exact inverse of `infoToBits` writing it
 * least-significant bit first walking upwards.
 *
 * TWO ARMS END WITH AN ABSOLUTE CONSTANT (0x32 and 0x98) rather than with the
 * arithmetic, and that is what pins the fixed part of the layout: the header
 * ends at bit 0x32 and the six pairs end at bit 0x98, which is one below
 * 0x99, which is where `infoToBits` has laid the next framing bit after
 * 0x33 + 6*17.  The two halves were read independently and agree.
 *
 * The accumulator's WIDTH is the field's own: `%cl` for the five-bit byte at
 * +0x10, `%di` as a 16-bit register for `short_58`, and 32 bits everywhere
 * else.  That is forced -- a wider accumulator would not have needed the
 * `movzwl %di,%eax` the object encodes -- and it is what makes `short_58`
 * shorts and `word_18` ints.
 */
void
V90CP::evaluateInfo()
{
	unsigned int i, j, k, p, q, n;
	int *dst;

	switch (word_ca4) {
	case 3:
		/* The short form: two bits, and no cursor movement. */
		word_ca0 = bits[0x20];
		byte_13 = bits[0x21];
		break;

	case 5:
		/* The header, ending at the absolute bit 0x32. */
		byte_10 = 0;
		for (p = 0x1a; p > 0x15; p--)
			byte_10 = (signed char)((byte_10 << 1) | (bits[p] & 1));

		byte_11 = (unsigned char)((bits[0x1b] & 1) |
					  ((bits[0x1c] & 1) << 1));
		byte_12 = bits[0x1d];
		byte_13 = bits[0x21];

		word_14 = 0;
		for (p = 0x32; p > 0x22; p--)
			word_14 = (word_14 << 1) | (bits[p] & 1);

		word_cb4 = 0x32;
		break;

	case 6:
		/* Six frames of two eight-bit halves, ending at 0x98. */
		for (i = 0; i <= 0xb; i++)
			word_18[i] = 0;

		p = word_cb4;
		for (k = 0; k <= 5; k++) {
			p += 9;
			word_cb4 = p;
			for (i = 0, q = p; i <= 7; i++, q--)
				word_18[2 * k] =
				    (word_18[2 * k] << 1) | (bits[q] & 1);

			p += 8;
			word_cb4 = p;
			for (i = 0, q = p; i <= 7; i++, q--)
				word_18[2 * k + 1] =
				    (word_18[2 * k + 1] << 1) | (bits[q] & 1);
		}
		word_cb4 = 0x98;
		break;

	case 7:
		/* Four nine-bit counts, one to a frame. */
		for (k = 0; k <= 3; k++)
			nof_58[k] = 0;

		p = word_cb4 + 0xa;
		q = p;
		for (k = 0; k <= 3; k++) {
			word_cb4 = p;
			for (i = 0, q = p; i <= 8; i++, q--)
				nof_58[k] = (nof_58[k] << 1) | (bits[q] & 1);
			p += 0x11;
		}
		word_cb4 = q + 0x10;
		break;

	case 8:
		/* The four counted lists, one entry to a frame. */
		for (k = 0; k <= 3; k++) {
			n = nof_58[k];
			for (j = 0; j < n; j++) {
				short_58[k][j] = 0;
				p = word_cb4 + 0x11;
				word_cb4 = p;
				for (i = 0, q = p; i <= 0xf; i++, q--)
					short_58[k][j] = (short)
					    ((short_58[k][j] << 1) |
					     (bits[q] & 1));
				word_cb4 = q + 0x10;
			}
		}
		break;

	case 10:
		/* Six four-bit values, four to a frame and then two, and then
		 * the six eight-bit buffer counts, two to a frame. */
		for (i = 0; i <= 5; i++)
			word_c70[i] = 0;

		word_cb4++;
		p = word_cb4;
		for (k = 0; k <= 3; k++) {
			p += 4;
			word_cb4 = p;
			for (i = 0, q = p; i <= 3; i++, q--)
				word_c70[k] =
				    (word_c70[k] << 1) | (bits[q] & 1);
		}

		p++;
		word_cb4 = p;
		for (k = 4; k <= 5; k++) {
			p += 4;
			word_cb4 = p;
			for (i = 0, q = p; i <= 3; i++, q--)
				word_c70[k] =
				    (word_c70[k] << 1) | (bits[q] & 1);
		}
		p += 8;
		word_cb4 = p;

		for (i = 0; i <= 5; i++)
			nof_buf[i] = 0;

		for (k = 0; k <= 2; k++) {
			p += 9;
			word_cb4 = p;
			for (i = 0, q = p; i <= 7; i++, q--)
				nof_buf[2 * k] =
				    (nof_buf[2 * k] << 1) | (bits[q] & 1);

			p += 8;
			word_cb4 = p;
			for (i = 0, q = p; i <= 7; i++, q--)
				nof_buf[2 * k + 1] =
				    (nof_buf[2 * k + 1] << 1) | (bits[q] & 1);
		}
		break;

	case 11:
		/*
		 * The six buffers.  `word_cb4` is stepped through MEMORY here
		 * rather than through a register, because the store into
		 * `dst[j]` may alias it -- that is the object's own reading
		 * and it is why this arm reloads where the others do not.
		 */
		for (k = 0; k <= 5; k++) {
			if (nof_buf[k] == 0)
				continue;
			dst = buf[k];
			for (j = 0; j < nof_buf[k]; j++) {
				word_cb4 += 0x11;
				dst[j] = 0;
				for (i = 0; i <= 0xf; i++) {
					dst[j] = (dst[j] << 1) |
						 (bits[word_cb4] & 1);
					word_cb4--;
				}
				word_cb4 += 0x10;
			}
		}
		break;

	default:
		break;
	}
}

/*
 * infoToBits -- 0x52230, 2785 bytes, and the inverse of `evaluateInfo` field
 * for field.  It lays the CP sequence out one byte per bit into `bits`,
 * running the write cursor `word_cac` forward, and closes by generating the
 * CRC over what it just wrote and padding out to a whole number of whatever
 * +0x3ba8 counts.
 *
 * THE ONE FIELD THAT IS NOT A SHIFT LOOP is +0x11: the object writes its two
 * bits from a four-arm switch over 0, 1, 2 and 3, so a value outside that
 * range leaves bits[0x1b] and bits[0x1c] holding whatever was there before.
 * A shift loop cannot do that, and t_v90cp drives a fifth value through it
 * precisely so that the difference is observed rather than assumed.
 *
 * THE CRC IS THE CCITT REGISTER RUN ONE BIT PER BYTE.  Sixteen bytes, all set
 * to 1, then for each information bit `a = (crc[0] + bit) & 1`, the register
 * shifts down one place, `a` is added into what becomes crc[3] and crc[10],
 * and `a` itself lands in crc[15] -- x^16 + x^12 + x^5 + 1 with the register
 * held least-significant-first.  Framing bits are NOT covered: the walk
 * carries an `if (i % 17 == 0) i++`, which the object encodes as the
 * 0xf0f0f0f1 reciprocal followed by `cmp $1` / `adc $0`.
 */
void
V90CP::infoToBits()
{
	unsigned int i, j, k;
	unsigned int pos, start, nbits, total, group, quot;
	int c;
	unsigned char a;

	/* One frame of ones, then the first framing bit. */
	for (i = 0; i <= 0x10; i++)
		bits[i] = 1;
	bits[0x11] = 0;
	bits[0x12] = (unsigned char)word_00;

	if (word_00 != 0) {
		/* The short form: three frames and straight to the CRC. */
		for (i = 0x13; i <= 0x1f; i++)
			bits[i] = 0;
		word_cac = 0x22;
		bits[0x20] = (unsigned char)word_ca0;
		bits[0x21] = byte_13;
	} else {
		int v;
		unsigned int u;

		bits[0x13] = (unsigned char)word_04;
		bits[0x14] = (unsigned char)word_08;
		bits[0x15] = (unsigned char)word_0c;

		v = byte_10;			/* movsbl, then sar */
		for (i = 0; i <= 4; i++) {
			bits[0x16 + i] = (unsigned char)(v & 1);
			v >>= 1;
		}

		switch (byte_11) {
		case 0:
			bits[0x1b] = 0;
			bits[0x1c] = 0;
			break;
		case 1:
			bits[0x1b] = 1;
			bits[0x1c] = 0;
			break;
		case 2:
			bits[0x1b] = 0;
			bits[0x1c] = 1;
			break;
		case 3:
			bits[0x1b] = 1;
			bits[0x1c] = 1;
			break;
		}

		bits[0x1d] = byte_12;
		for (i = 0; i <= 2; i++)
			bits[0x1e + i] = 0;
		bits[0x21] = byte_13;
		bits[0x22] = 0;

		u = (unsigned short)word_14;	/* movzwl, then shr */
		for (i = 0; i <= 0xf; i++) {
			bits[0x23 + i] = (unsigned char)(u & 1);
			u >>= 1;
		}

		byte_ca8 = 0;
		word_cac = 0x33;

		if (word_04 != 0) {
			for (k = 0; k <= 5; k++) {
				int lo = (short)word_18[2 * k];
				int hi = (short)word_18[2 * k + 1];

				pos = word_cac;
				bits[pos] = 0;
				pos++;
				word_cac = pos;
				for (i = 0; i <= 7; i++) {
					bits[pos + i] = (unsigned char)(lo & 1);
					lo >>= 1;
				}
				pos += 8;
				word_cac = pos;
				for (i = 0; i <= 7; i++) {
					bits[pos + i] = (unsigned char)(hi & 1);
					hi >>= 1;
				}
				word_cac = pos + 8;
			}
		}

		if (word_08 != 0) {
			unsigned int t[4];
			unsigned int n[4];

			for (k = 0; k <= 3; k++) {
				t[k] = nof_58[k];
				n[k] = nof_58[k];
			}

			for (k = 0; k <= 3; k++) {
				pos = word_cac;
				bits[pos] = 0;
				pos++;
				word_cac = pos;
				for (i = 0; i <= 8; i++) {
					bits[pos + i] = (unsigned char)(t[k] & 1);
					t[k] >>= 1;
				}
				word_cac = pos + 9;
				for (i = 0; i <= 6; i++)
					bits[pos + 9 + i] = 0;
				word_cac = pos + 16;
			}

			for (k = 0; k <= 3; k++) {
				for (j = 0; j < n[k]; j++) {
					int w = short_58[k][j];

					pos = word_cac;
					bits[pos] = 0;
					pos++;
					word_cac = pos;
					for (i = 0; i <= 0xf; i++) {
						bits[pos + i] =
						    (unsigned char)(w & 1);
						w >>= 1;
					}
					word_cac = pos + 16;
				}
			}
		}

		if (word_0c != 0) {
			pos = word_cac;
			bits[pos] = 0;
			word_cac = pos + 1;
			for (k = 0; k <= 3; k++) {
				int w = (short)word_c70[k];

				pos = word_cac;
				for (i = 0; i <= 3; i++) {
					bits[pos + i] = (unsigned char)(w & 1);
					w >>= 1;
				}
				word_cac = pos + 4;
			}

			pos = word_cac;
			bits[pos] = 0;
			word_cac = pos + 1;
			for (k = 4; k <= 5; k++) {
				int w = (short)word_c70[k];

				pos = word_cac;
				for (i = 0; i <= 3; i++) {
					bits[pos + i] = (unsigned char)(w & 1);
					w >>= 1;
				}
				word_cac = pos + 4;
			}

			pos = word_cac;
			for (i = 0; i <= 7; i++)
				bits[pos + i] = 0;
			word_cac = pos + 8;

			for (k = 0; k <= 2; k++) {
				int lo = (short)nof_buf[2 * k];
				int hi = (short)nof_buf[2 * k + 1];

				pos = word_cac;
				bits[pos] = 0;
				pos++;
				word_cac = pos;
				for (i = 0; i <= 7; i++) {
					bits[pos + i] = (unsigned char)(lo & 1);
					lo >>= 1;
				}
				pos += 8;
				word_cac = pos;
				for (i = 0; i <= 7; i++) {
					bits[pos + i] = (unsigned char)(hi & 1);
					hi >>= 1;
				}
				word_cac = pos + 8;
			}

			for (k = 0; k <= 5; k++) {
				unsigned int n = nof_buf[k];
				const int *src;

				if (n == 0)
					continue;
				src = buf[k];
				for (j = 0; j < n; j++) {
					int w = (short)src[j];

					pos = word_cac;
					bits[pos] = 0;
					pos++;
					word_cac = pos;
					for (i = 0; i <= 0xf; i++) {
						bits[pos + i] =
						    (unsigned char)(w & 1);
						w >>= 1;
					}
					word_cac = pos + 16;
				}
			}
		}
	}

	/* --- the tail, which both forms reach --- */

	start = (unsigned int)word_cac;
	bits[start] = 0;
	word_cac = start + 1;
	nbits = start + 0x11;
	word_3bb0 = nbits;

	/* SIGNED, exactly as in `evaluateCRC` and `resetCRC`: `jle`. */
	for (c = 0; c <= 0xf; c++)
		crc[c] = 1;

	for (i = 0x12; i < start; ) {
		if (i % 17 == 0)
			i++;
		a = (unsigned char)((crc[0] + bits[i]) & 1);
		i++;

		crc[0] = crc[1];
		crc[1] = crc[2];
		crc[2] = crc[3];
		crc[3] = (unsigned char)((crc[4] + a) & 1);
		crc[4] = crc[5];
		crc[5] = crc[6];
		crc[6] = crc[7];
		crc[7] = crc[8];
		crc[8] = crc[9];
		crc[9] = crc[10];
		crc[10] = (unsigned char)((crc[11] + a) & 1);
		crc[11] = crc[12];
		crc[12] = crc[13];
		crc[13] = crc[14];
		crc[14] = crc[15];
		crc[15] = a;
	}

	pos = (unsigned int)word_cac;
	for (i = 0; i <= 0xf; i++) {
		bits[pos] = crc[i];
		pos++;
	}
	bits[pos] = 0;
	word_cac = pos + 1;

	/* calcSequenceLength, inlined: round the bit count up to a whole
	 * number of +0x3ba8 and record it at +0x3bac. */
	total = nbits + 1;
	group = word_3ba8;
	quot = total / group;
	if (group * quot == total)
		word_3bac = group * quot;
	else
		word_3bac = (quot + 1) * group;

	pos = (unsigned int)word_cac;
	while (word_3bac > pos) {
		bits[pos] = 0;
		pos++;
	}
}

/*
 * evaluateCRC -- 0x51730, 668 bytes.  The read side of the register above:
 * recompute the CRC over the information bits of a RECEIVED sequence and
 * compare it against the sixteen the peer sent.
 *
 * The extent is `word_3bb0`, which `bitsToInfo` has by then set the way
 * `infoToBits` sets it -- information bits run from 0x12 up to
 * word_3bb0 - 0x11, and the peer's CRC occupies the sixteen bits ending at
 * word_3bb0 - 1.  The object reaches those through a single displacement,
 * `-0x2ef0(%ecx,%edi,1)` with %ecx walking `crc` and %edi holding
 * word_3bb0, and 0x3b98 - 0x2ef0 - 0xcb8 is -0x10, which is where the -0x10
 * below comes from.
 *
 * IT RETURNS A VALUE, which the header used to say it did not: the epilogue
 * is `xor %eax,%eax` / `cmpb $0x0,...` / `sete %al`, and a leftover is never
 * built with a `sete`.  The comparison accumulates ABSOLUTE DIFFERENCES in a
 * single byte -- `cltd` / `xor %edx,%eax` / `sub %edx,%eax` is the object's
 * inlined `abs`, and the accumulator wraps at 256 exactly as ours does.
 */
int
V90CP::evaluateCRC()
{
	unsigned int i, end;
	int c;
	unsigned char a;
	unsigned char diff;

	/* SIGNED: the object's bound is `cmp $0xf` / `jle`, as in `resetCRC`. */
	for (c = 0; c <= 0xf; c++)
		crc[c] = 1;

	end = word_3bb0 - 0x11;
	for (i = 0x12; i < end; ) {
		if (i % 17 == 0)
			i++;
		a = (unsigned char)((crc[0] + bits[i]) & 1);
		i++;

		crc[0] = crc[1];
		crc[1] = crc[2];
		crc[2] = crc[3];
		crc[3] = (unsigned char)((crc[4] + a) & 1);
		crc[4] = crc[5];
		crc[5] = crc[6];
		crc[6] = crc[7];
		crc[7] = crc[8];
		crc[8] = crc[9];
		crc[9] = crc[10];
		crc[10] = (unsigned char)((crc[11] + a) & 1);
		crc[11] = crc[12];
		crc[12] = crc[13];
		crc[13] = crc[14];
		crc[14] = crc[15];
		crc[15] = a;
	}

	diff = 0;
	for (i = 0; i <= 0xf; i++) {
		int d = (int)crc[i] - (int)bits[word_3bb0 - 0x10 + i];

		if (d < 0)
			d = -d;
		diff = (unsigned char)(diff + d);
	}

	return diff == 0;
}

/*
 * bitsToInfo -- 0x52d20, 2391 bytes, and the RECEIVE-SIDE DRIVER of the class:
 * take one arriving bit, drive `word_ca4` (the decoder state), `word_cac` (the
 * cursor) and `word_cb0` (the count within the current block), and say what
 * the bit completed.
 *
 * IT IS NOT `void`, which the header used to say it was, and the mangling
 * cannot see the difference.  %edi is zeroed at entry, moved to %eax at BOTH
 * `ret`s, and six distinct values reach it -- 0 for nothing, 5 from the
 * run-counter test at the top, and 1, 2, 3 or 4 from the end of the message.
 * That is the same mistake `evaluateCRC` was carrying and the same one the
 * sibling `V90MP::bitsToInfo` turned out to have; a value built in %eax and a
 * value merely left there are told apart by whether every path arranges it,
 * and every path here does.
 *
 * WHAT THE FOUR MEAN, as far as the object states it: the two bits that
 * survive the whole message are `word_00`, which selects the short form, and
 * `byte_13`, which `infoToBits` places at bits[0x21] in BOTH forms.  The
 * answer is one of four combinations of those two --
 *
 *      byte_13 == 0, word_00 == 0   ->  1
 *      byte_13 == 0, word_00 != 0   ->  3
 *      byte_13 != 0, word_00 == 0   ->  2
 *      byte_13 != 0, word_00 != 0   ->  4
 *
 * -- and nothing in the object names any of them.  `V90MP::bitsToInfo`'s
 * corresponding 1, 2 and 3 ARE named, by its own diagnostics, and this member
 * has no such line, so the numbers stay numbers.  Finding F4360.
 *
 * THE TWO STATICS ARE THE BATCH.  `alpha` and `beta` are function-local
 * statics -- .bss, mangled `_ZZN5V90CP10bitsToInfoEhE5alpha` and `...E4beta`,
 * so their C++ names are the author's -- and each holds the bit LENGTH of one
 * counted block, computed once when the block's counts have been decoded and
 * compared against `word_cb0` while the block arrives.  Seventeen bits to the
 * entry, which is one frame each:
 *
 *      alpha = 17 * (nof_58[0] + ... + nof_58[3])   the four shorts lists
 *      beta  = 17 * (nof_buf[0] + ... + nof_buf[5])  the six buffers
 *
 * Their signedness is NOT established -- every use is an equality compare and
 * the `shl $4` / `add` that makes the product is the same either way -- so
 * they are spelled to match the counts they sum.  Finding F4363.
 *
 * THE ONE STRING THAT BOUNDS AN ARRAY.  Five of the arms guard the store into
 * `bits` with `cmp $0x2edf` / `ja` and print "not enouch memory in the
 * buffer" instead, which is the author saying in his own words that index
 * 0x2edf is the last one that fits.  0xcb8 + 0x2ee0 is 0x3b98, which is where
 * `crc` starts, so the two ends meet and V90CP_BITS is measured rather than
 * modelled.  FIVE OF THE TEN STORE SITES ARE GUARDED AND FIVE ARE NOT --
 * docs/deviations.md D520.  Finding F4361.
 *
 * THE RECEIVER HARDCODES SIX WHERE THE TRANSMITTER USES `word_3ba8`.
 * `infoToBits` pads the sequence out to a whole number of +0x3ba8; `case 13`
 * here waits for `word_cac % 6 == 0`, with the six as an immediate.  Finding
 * F4364.
 *
 * `evaluateInfo` is CALLED -- eight relocations against it -- and
 * `resetDetector` and `evaluateCRC` are not: those two are global symbols with
 * no relocation at their sites, so GCC 3.4.2 at -O3 folded them in, which is
 * finding F4600's shape in the constructor and `reset`.
 */

/*
 * The author's own spelling, reproduced byte for byte: the leading newline,
 * "enouch", the space before the comma and the space before the trailing
 * newline are all in .rodata.str1.4+0xd650.  One pooled copy, five referrers.
 */
#define V90CP_NOMEM \
	"\n *** error CP bit , not enouch memory in the buffer *** \n"

int
V90CP::bitsToInfo(unsigned char bit)
{
	/*
	 * Declared in this order because that is the order they occupy in
	 * .bss -- alpha at +0x8, beta at +0xc.
	 */
	static unsigned int alpha;
	static unsigned int beta;

	int rc = 0;

	/*
	 * THE RUN COUNTERS COME FIRST and are independent of the state: every
	 * bit lengthens one run and clears the other.  A run of 2 * the group
	 * size of zeros arriving while the cursor is still at its home 18 is
	 * the far end having stopped, and that answer does NOT stop the state
	 * machine below, which runs on and can overwrite it.
	 *
	 * `byte_caa` is read back out of the object rather than out of a
	 * local -- the object stores 0 and reloads it four instructions later
	 * -- which matters when `word_3ba8` is zero, because then the test is
	 * true on a ONE bit as well.
	 */
	if (bit != 0) {
		byte_ca9++;
		byte_caa = 0;
	} else {
		byte_caa++;
		byte_ca9 = 0;
	}

	if (byte_caa == 2 * word_3ba8 && word_cac == 18)
		rc = 5;

	switch (word_ca4) {
	case 0:
		/* Seventeen ones is the preamble; sixteen are not enough. */
		if (byte_ca9 > 0x10)
			word_ca4 = 1;
		break;

	case 1:
		/* The framing zero, or start again. */
		if (bit == 0)
			word_ca4 = 2;
		else
			resetDetector();
		break;

	case 2:
		/*
		 * The type bit, at index 18.  It is stored whole into
		 * `word_00` and read BACK from there for the branch -- 32-bit
		 * `cmp $1`, not an 8-bit test of the argument -- and it picks
		 * the short form's state 3 or the long form's 4.  State 4 is
		 * one of `evaluateInfo`'s two holes, so nothing decodes it.
		 */
		word_00 = bit;
		bits[word_cac] = bit;
		word_cac++;
		word_ca4 = (word_00 != 0) ? 3 : 4;
		word_cb0 = 0;
		break;

	case 3:
		/*
		 * The short form: fifteen more bits, ending at 0x21, which is
		 * exactly the two `evaluateInfo`'s `case 3` reads back.
		 */
		if (word_cac <= V90CP_BITS - 1) {
			bits[word_cac] = bit;
			word_cac++;
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(V90CP_NOMEM);
		}
		word_cb0++;
		if (word_cb0 == 0xf) {
			evaluateInfo();
			word_ca4 = 12;
			word_cb0 = 0;
		}
		break;

	case 4:
		/*
		 * The three block flags, one bit each, dispatched on the
		 * count rather than shifted.  The object's `jb` on the tree's
		 * `x < 1` arm is what makes `word_cb0` unsigned: a signed
		 * index would have needed a second test for zero.
		 */
		bits[word_cac] = bit;
		word_cac++;
		switch (word_cb0) {
		case 0:
			word_04 = bit;
			break;
		case 1:
			word_08 = bit;
			break;
		case 2:
			word_0c = bit;
			break;
		}
		word_cb0++;
		if (word_cb0 == 3) {
			word_ca4 = 5;
			word_cb0 = 0;
		}
		break;

	case 5:
		/*
		 * The rest of the header, to the absolute 0x33 -- one past
		 * `evaluateInfo`'s `case 5`, which ends at 0x32.  Then the
		 * first of the two places that pick the next block, in the
		 * order the blocks travel: +0x18, then +0x48/+0x58, then
		 * everything from +0xc58, then the CRC.
		 */
		bits[word_cac] = bit;
		word_cac++;
		if (word_cac == 0x33) {
			evaluateInfo();
			if (word_04 != 0)
				word_ca4 = 6;
			else if (word_08 != 0)
				word_ca4 = 7;
			else
				word_ca4 = (word_0c != 0) ? 10 : 12;
			word_cb0 = 0;
		}
		break;

	case 6:
		/* Six frames of pairs, to 0x99 -- one past the 0x98 that
		 * `evaluateInfo`'s `case 6` stores.  The two halves were read
		 * independently and agree. */
		bits[word_cac] = bit;
		word_cac++;
		if (word_cac == 0x99) {
			evaluateInfo();
			if (word_08 != 0)
				word_ca4 = 7;
			else
				word_ca4 = (word_0c != 0) ? 10 : 12;
			word_cb0 = 0;
		}
		break;

	case 7:
		/* The four nine-bit counts: four frames, 0x44 bits.  Once
		 * they are decoded the next block's length is known. */
		bits[word_cac] = bit;
		word_cac++;
		word_cb0++;
		if (word_cb0 == 0x44) {
			evaluateInfo();
			word_ca4 = 8;
			word_cb0 = 0;
			alpha = 17 * (nof_58[0] + nof_58[1] + nof_58[2] +
				      nof_58[3]);
		}
		break;

	case 8:
		/* The four counted lists, `alpha` bits of them. */
		if (word_cac <= V90CP_BITS - 1) {
			bits[word_cac] = bit;
			word_cac++;
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(V90CP_NOMEM);
		}
		word_cb0++;
		if (word_cb0 == alpha) {
			evaluateInfo();
			word_ca4 = (word_0c != 0) ? 10 : 12;
			word_cb0 = 0;
		}
		break;

	case 10:
		/* Five frames, 0x55 bits: the six four-bit values and the six
		 * eight-bit buffer counts.  Then the buffers' length. */
		if (word_cac <= V90CP_BITS - 1) {
			bits[word_cac] = bit;
			word_cac++;
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(V90CP_NOMEM);
		}
		word_cb0++;
		if (word_cb0 == 0x55) {
			evaluateInfo();
			word_ca4 = 11;
			word_cb0 = 0;
			beta = 17 * (nof_buf[0] + nof_buf[1] + nof_buf[2] +
				     nof_buf[3] + nof_buf[4] + nof_buf[5]);
		}
		break;

	case 11:
		/* The six buffers, `beta` bits of them. */
		if (word_cac <= V90CP_BITS - 1) {
			bits[word_cac] = bit;
			word_cac++;
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(V90CP_NOMEM);
		}
		word_cb0++;
		if (word_cb0 == beta) {
			evaluateInfo();
			word_ca4 = 12;
			word_cb0 = 0;
		}
		break;

	case 12:
		/*
		 * The CRC frame: one framing zero and sixteen CRC bits.
		 * `word_3bb0` is then the cursor itself, which is what
		 * `evaluateCRC` wants -- it runs from 0x12 to
		 * word_3bb0 - 0x11 and compares against the sixteen bits
		 * ending at word_3bb0 - 1.  `infoToBits` computes the same
		 * number as start + 0x11 from the other side.
		 *
		 * The call is INLINED by the compiler, exactly as `reset`'s
		 * call of `resetDetector` is: `evaluateCRC` is a global symbol
		 * and there is no relocation against it here.
		 */
		if (word_cac <= V90CP_BITS - 1) {
			bits[word_cac] = bit;
			word_cac++;
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(V90CP_NOMEM);
		}
		word_cb0++;
		if (word_cb0 == 0x11) {
			word_3bb0 = word_cac;
			if (evaluateCRC()) {
				word_cb0 = 0x11;
				word_ca4 = 13;
			} else {
				resetDetector();
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90CP: recieved CP with bad CRC"
					    "\r\n");
			}
		}
		break;

	case 13:
		/*
		 * The tail.  Zeros pad the sequence out and a one restarts
		 * the detector; either way the message is handed over on the
		 * next cursor position that is a multiple of SIX -- an
		 * immediate, where `infoToBits` pads to a multiple of
		 * `word_3ba8`.  A one therefore reports as well, because
		 * `resetDetector` leaves the cursor at 18 and 18 % 6 is 0;
		 * `evaluateInfo` is then called with a state of 0 and does
		 * nothing.
		 */
		if (bit != 0)
			resetDetector();
		else
			word_cac++;
		if (word_cac % 6 == 0) {
			evaluateInfo();
			resetDetector();
			if (byte_13 != 0)
				rc = (word_00 != 0) ? 4 : 2;
			else
				rc = (word_00 != 0) ? 3 : 1;
		}
		break;

	default:
		break;
	}

	/*
	 * THE HOLD-OFF, and it is the only thing in the class that reads
	 * +0x3bbc.  It sits at -1 until an answer of 1 or 2 starts it, then
	 * counts one per call to 0x320 and stops itself; while it is running,
	 * the answers 4 and 2 are suppressed to 0 and 1, 3 and 5 are not.
	 * What it is a hold-off FOR is not stated anywhere in the object, so
	 * the field keeps its offset name.
	 */
	if (word_3bbc >= 0) {
		if (rc == 4 || rc == 2)
			rc = 0;
		word_3bbc++;
		if (word_3bbc == 0x320)
			word_3bbc = -1;
	} else if (rc == 1 || rc == 2) {
		word_3bbc = 0;
	}

	return rc;
}
