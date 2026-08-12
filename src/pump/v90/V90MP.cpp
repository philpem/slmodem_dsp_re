/*
 * V90MP.cpp -- V.90 MP message: construction and destruction.
 *
 * Reconstructed from dsplibs.o V90MP.cpp.  Two of the class's sixteen
 * symbols: the constructor (0x1f410, 40 bytes) and the destructor (0x1f130,
 * one byte -- a bare `ret`).  `include/dsplib/V90MP.h` carries the object map
 * and says which member proved which offset.
 *
 * THE CONSTRUCTOR AND `reset` ARE THE SAME FORTY BYTES, instruction for
 * instruction: the two symbols at 0x1f410 and 0x1f3e0 disassemble alike down
 * to the register allocation.  Both are ordinary GLOBAL symbols in `.text`
 * rather than in a linkonce section, so `reset` is not an in-class inline the
 * compiler folded into the constructor (GCC 3.4 at -O2 does not inline an
 * ordinary global function).  The original repeated the assignments, which is
 * why they are repeated here rather than written as `reset()`.  Finding 1237.
 *
 * The calling convention is plain cdecl -- `mov 0x4(%esp),%eax` -- not
 * thiscall (finding 215).
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/V90MP.h"

/* Hold the compiler to the map in the header; see V90CP.cpp for why. */
#if __SIZEOF_POINTER__ == 4
#define V90MP_OFF(field, off, tag) \
	typedef char v90mp_off_##tag[ \
	    ((int)__builtin_offsetof(V90MP, field) == (off)) ? 1 : -1]

V90MP_OFF(Type,			0x000, type0);
V90MP_OFF(Rate,			0x001, rate);
V90MP_OFF(Trellis,		0x002, trellis);
V90MP_OFF(NonLin,		0x003, nonlin);
V90MP_OFF(Shaping,		0x004, shaping);
V90MP_OFF(CPack,		0x005, cpack);
V90MP_OFF(rateMask,		0x006, ratemask);
V90MP_OFF(h1Real,		0x008, h1real);
V90MP_OFF(h1Imag,		0x00a, h1imag);
V90MP_OFF(h2Real,		0x00c, h2real);
V90MP_OFF(h2Imag,		0x00e, h2imag);
V90MP_OFF(h3Real,		0x010, h3real);
V90MP_OFF(h3Imag,		0x012, h3imag);
V90MP_OFF(word_14,		0x014, word14);
V90MP_OFF(type,			0x018, type18);
V90MP_OFF(byte_19,		0x019, byte19);
V90MP_OFF(byte_1a,		0x01a, byte1a);
V90MP_OFF(byte_1b,		0x01b, byte1b);
V90MP_OFF(bits,			0x01c, bits);
V90MP_OFF(crc,			0x102, crc);
V90MP_OFF(word_114,		0x114, word114);
V90MP_OFF(byte_118,		0x118, byte118);
V90MP_OFF(byte_119,		0x119, byte119);
V90MP_OFF(nofRecievedMp,	0x11c, nofmp);
V90MP_OFF(nofRecievedMpNot,	0x120, nofmpnot);
typedef char v90mp_size[(sizeof(V90MP) == 0x124) ? 1 : -1];
#endif

V90MP::V90MP()
{
	word_14 = 0;
	byte_19 = 0;
	byte_1a = 0;
	byte_1b = 18;

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;
}

/*
 * One byte in the object: `ret`.  The class allocates nothing -- unlike
 * V90CP, whose 173-byte destructor releases six buffers -- so there is
 * nothing for this to do, and the size is the evidence that it does none.
 */
V90MP::~V90MP()
{
}

/*
 * reset -- the constructor's forty bytes again, instruction for instruction.
 *
 * 0x1f3e0 and 0x1f410 differ in nothing but their address: the same six
 * stores in the same order, with the same two scratch registers zeroed ahead
 * of the pair of four-byte ones.  Finding 1237 is why the assignments are
 * repeated here rather than written as a call to `resetDetector` plus two
 * counters -- `resetDetector` is a separate GLOBAL symbol at 0x1f3c0 and GCC
 * 3.4 at -O2 does not inline one of those, so an original that called it
 * would have left a call behind.
 */
void
V90MP::reset()
{
	word_14 = 0;
	byte_19 = 0;
	byte_1a = 0;
	byte_1b = 18;

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;
}

/*
 * getBitVector -- hand back the vector and its length.
 *
 * Twenty-one bytes and no branch:
 *
 *     1f708:  0f b6 88 18 01 00 00   movzbl 0x118(%eax),%ecx
 *     1f70f:  83 c0 1c               add    $0x1c,%eax
 *     1f712:  89 0a                  mov    %ecx,(%edx)
 *
 * so the length is the ONE BYTE at +0x118 widened without sign, written
 * whole into the caller's `unsigned int`, and the pointer is `this + 0x1c`
 * -- which is what fixes the bit vector's start.  `movzbl` into a register
 * whose whole 32 bits are then stored is the forced-signedness case
 * CLAUDE.md names: it is why +0x118 is `unsigned char` and not `char`.
 */
unsigned char *
V90MP::getBitVector(unsigned int &length)
{
	length = byte_118;
	return bits;
}

/*
 * printNofRecievedMpMpNot -- the two counters, by the debug string's words.
 *
 * The gate is the object's own: `cmpl $0x1,dsplibs_debug_level; ja`, which
 * is `DSPLIB_DEBUG_ON()`.  This one is NOT an `edprintf` -- the call at
 * 0x20bef relocates against `dsplibs_debug_printf` directly -- so unlike
 * every diagnostic in `V90ConnectionEvaluator` it says nothing at all below
 * the gate, and there is no encoder key to move.
 *
 * The argument order is the object's: +0x11c is the first `%d` and +0x120
 * the second, which is what names the two fields.
 */
void
V90MP::printNofRecievedMpMpNot()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90MP: received %d MP, %d MPNot\r\n",
				     nofRecievedMp, nofRecievedMpNot);
}

/*
 * evaluateInfo -- read the thirteen fields at +0x00 back out of `bits`.
 *
 * 482 bytes at 0x1f720, and the exact inverse of `infoToBits` field for field,
 * which is what makes the round trip a test rather than a restatement.
 *
 * TWO SHAPES OF UNPACK, and the object keeps them apart.  The four-bit `Rate`
 * and the six sixteen-bit h-values are accumulated MOST SIGNIFICANT BIT FIRST
 * by counting DOWN -- `bits[0x1b]` ends up weighing eight, `bits[0x43]` weighs
 * 0x8000 -- while the fourteen-bit rate mask is built by counting UP with a
 * variable shift, `1 << (i - 0x24)`, out of a running read-modify-write of the
 * field itself (`movzwl 0x6(%ebx)`, `or`, `mov %cx`).  Both orderings put bit
 * zero at the LOW index, so the two agree; only the code differs.
 *
 * `Type` IS THE GATE.  The six h-values are zeroed unconditionally and then
 * filled only when +0x18 is non-zero (`cmpb $0x0,0x3(%esp)` against the copy
 * saved on entry, `je` to the epilogue), because a type-zero message is five
 * frames long and stops before them.
 *
 * VOID, and measured: the two `ret` paths leave different leftovers in %eax --
 * a shifted mask on one, the last accumulator on the other -- and nothing
 * arranges it on either.
 */
void
V90MP::evaluateInfo()
{
	unsigned int i;
	unsigned char t = (unsigned char)type;

	Type = (char)t;

	Rate = 0;
	for (i = 0x1b; i > 0x17; i--)
		Rate = (char)((Rate << 1) | (bits[i] & 1));

	Trellis = (char)(((bits[0x1e] & 1) << 1) | (bits[0x1d] & 1));

	/*
	 * WHOLE BYTES, not masked bits.  `movzbl 0x3b(%ebx),%eax; mov
	 * %al,0x3(%ebx)` -- there is no `and $0x1` on these three, so a
	 * `bits` element holding 2 arrives in the field as 2.
	 */
	NonLin = (char)bits[0x1f];
	Shaping = (char)bits[0x20];
	CPack = (char)bits[0x21];

	rateMask = 0;
	for (i = 0x24; i <= 0x31; i++)
		if (bits[i])
			rateMask = (short)(rateMask | (1 << (i - 0x24)));

	h1Real = 0;
	h1Imag = 0;
	h2Real = 0;
	h2Imag = 0;
	h3Real = 0;
	h3Imag = 0;

	if (t == 0)
		return;

	for (i = 0x43; i > 0x33; i--)
		h1Real = (short)((h1Real << 1) | (bits[i] & 1));
	for (i = 0x54; i > 0x44; i--)
		h1Imag = (short)((h1Imag << 1) | (bits[i] & 1));
	for (i = 0x65; i > 0x55; i--)
		h2Real = (short)((h2Real << 1) | (bits[i] & 1));
	for (i = 0x76; i > 0x66; i--)
		h2Imag = (short)((h2Imag << 1) | (bits[i] & 1));
	for (i = 0x87; i > 0x77; i--)
		h3Real = (short)((h3Real << 1) | (bits[i] & 1));
	for (i = 0x98; i > 0x88; i--)
		h3Imag = (short)((h3Imag << 1) | (bits[i] & 1));
}

/*
 * infoToBits -- lay the thirteen fields out as an MP sequence.
 *
 * 1,948 bytes at 0x1f990.  Seventeen ones, then 17-bit frames of a zero
 * framing bit and sixteen data bits; eleven frames when `Type` is non-zero and
 * five when it is zero.  V90MP.h has the map.
 *
 * THE CRC IS WRITTEN OUT HERE RATHER THAN CALLED.  `resetCRC` (0x1f150) and
 * `calcCRC` (0x1f170) are plain GLOBAL symbols in `.text`, and GCC 3.4 at -O2
 * does not inline one of those, so an original that had called them would have
 * left two calls behind and there are none.  It is finding 1237's argument
 * again -- the same argument that says `reset` repeats the constructor -- and
 * it applies three times over, since `bitsToInfo` carries two more copies.
 *
 * TWO DEFECTS OF THE ORIGINAL ARE REPRODUCED HERE ON PURPOSE.  Finding 1386.
 *
 *   - The type-zero arm pads from 0x45, which is where it has just put the
 *     CRC, so it destroys it: `movb $0x0,0x61(%ebx)` and a loop from 0x45,
 *     against the type-one arm's correct 0xbb and 0xbc.  +0x118 is at least
 *     0x56 for every non-zero group size, so this happens every time.
 *
 *   - +0x118 is read ONCE into a register before that pad loop, and the loop
 *     can reach far enough to overwrite +0x118 itself (index 0xfc).  `len`
 *     here is that register: writing `i < byte_118` instead would re-read the
 *     field and stop early.
 */
void
V90MP::infoToBits()
{
	unsigned int i;
	int k;
	int v;
	unsigned char t = (unsigned char)Type;
	unsigned char x, n, end, len;

	for (i = 0; i <= 0x10; i++)
		bits[i] = 1;
	bits[0x11] = 0;
	bits[0x12] = t;
	for (i = 0x13; i <= 0x17; i++)
		bits[i] = 0;

	v = Rate;
	for (i = 0; i <= 3; i++) {
		bits[0x18 + i] = (unsigned char)(v & 1);
		v >>= 1;
	}

	bits[0x1c] = 0;

	/*
	 * A SWITCH, not two shifts: the object tests 1, then `jle` to a test
	 * against 0, then 2, then 3, and writes NOTHING outside 0..3 -- so a
	 * `Trellis` of 4 leaves the two bits holding whatever they held.  The
	 * `jle` is also what makes the field signed.
	 */
	switch (Trellis) {
	case 0:
		bits[0x1d] = 0;
		bits[0x1e] = 0;
		break;
	case 1:
		bits[0x1d] = 1;
		bits[0x1e] = 0;
		break;
	case 2:
		bits[0x1d] = 0;
		bits[0x1e] = 1;
		break;
	case 3:
		bits[0x1d] = 1;
		bits[0x1e] = 1;
		break;
	}

	bits[0x1f] = (unsigned char)NonLin;
	bits[0x20] = (unsigned char)Shaping;
	bits[0x21] = (unsigned char)CPack;
	bits[0x22] = 0;
	bits[0x23] = 0;
	for (i = 0x24; i <= 0x31; i++)
		bits[i] = (unsigned char)(((rateMask >> (i - 0x24)) & 1) != 0);
	bits[0x32] = 0;
	bits[0x33] = 0;

	type = (char)t;
	n = t ? 0xbb : 0x55;
	byte_119 = n;

	/*
	 * calcSequenceLength's body, repeated for the reason above: round
	 * +0x119 + 1 up to a multiple of the group size.  The divide is
	 * `div`, unsigned, and it faults on a group size of zero exactly as
	 * the object does.
	 */
	{
		unsigned int want = (unsigned int)n + 1;
		unsigned int q = want / word_114;

		if (q * word_114 == want)
			byte_118 = (unsigned char)want;
		else
			byte_118 = (unsigned char)((q + 1) * word_114);
	}

	if (t != 0) {
		v = h1Real;
		for (i = 0x34; i <= 0x43; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x44] = 0;
		v = h1Imag;
		for (i = 0x45; i <= 0x54; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x55] = 0;
		v = h2Real;
		for (i = 0x56; i <= 0x65; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x66] = 0;
		v = h2Imag;
		for (i = 0x67; i <= 0x76; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x77] = 0;
		v = h3Real;
		for (i = 0x78; i <= 0x87; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x88] = 0;
		v = h3Imag;
		for (i = 0x89; i <= 0x98; i++) {
			bits[i] = (unsigned char)(v & 1);
			v >>= 1;
		}
		bits[0x99] = 0;
		for (i = 0x9a; i <= 0xa9; i++)
			bits[i] = 0;
		bits[0xaa] = 0;
	} else {
		for (i = 0x34; i <= 0x43; i++)
			bits[i] = 0;
		bits[0x44] = 0;
	}

	/*
	 * The CRC covers the message but not its framing bits, and it stops at
	 * the last frame before the CRC's own -- 0xaa for the long message,
	 * 0x44 for the short one.  Both are `type ? 0xaa : 0x44` in the
	 * object, taken from the type flag and not from +0x119.
	 */
	end = t ? 0xaa : 0x44;

	for (k = 0; k <= 15; k++)
		crc[k] = 1;

	for (i = 0x12; i < end; i++) {
		if (i % 17 == 0)
			i++;
		x = (unsigned char)(crc[0] + bits[i]);
		for (k = 0; k < 15; k++)
			crc[k] = crc[k + 1];
		crc[3] = (unsigned char)((crc[3] + x) & 1);
		crc[10] = (unsigned char)((crc[10] + x) & 1);
		crc[15] = (unsigned char)(x & 1);
	}

	for (k = 0; k <= 15; k++)
		bits[end + 1 + k] = crc[k];

	len = byte_118;
	if (t != 0) {
		bits[0xbb] = 0;
		for (i = 0xbc; i < len; i++)
			bits[i] = 0;
	} else {
		bits[0x45] = 0;
		for (i = 0x45; i < len; i++)
			bits[i] = 0;
	}
}

/*
 * bitsToInfo -- take one received bit, and say what it completed.
 *
 * 2,597 bytes at 0x20190, and the only member of the class that returns
 * anything: 0 for nothing, 1 for an MP, 2 for an MPnot, 3 for Ed.  %edi is
 * zeroed at entry and moved to %eax at both `ret`s.
 *
 * THE RUN COUNTERS COME FIRST and are independent of the state: every bit
 * lengthens one run and clears the other, and a run of 2 * the group size of
 * zeros arriving while the bit index is still at its initial 18 is Ed -- the
 * far end has stopped transmitting.  That answer does not stop the state
 * machine, which runs on and can overwrite it with 1 or 2.
 *
 * THE CRC IS CHECKED TWICE.  If the sixteen received CRC bits do not match the
 * sixteen computed ones, and the message is the long one (+0x119 above 0x6f),
 * the object INVERTS bits[0x70] and tries again, reporting "modified good CRC"
 * if that rescued it.  It is a repair of one known-bad bit position, and it is
 * destructive: the flipped bit stays flipped whether or not it helped.
 *
 * THE COMPARISON IS A BYTE.  `add %al,0x4e(%esp)` accumulates sixteen absolute
 * differences into one byte and `cmpb $0x0` tests it, so sixteen differences
 * summing to a multiple of 256 read as a match.  An `int` sum would not.
 * The truncation is therefore the DECLARATION's job and `sum` is accumulated
 * with `+=`; written `sum = (unsigned char)(sum + ...)` the cast would do the
 * truncating instead, the declared width would stop mattering, and the
 * mutation that tests this claim would survive while looking exactly like a
 * missing test.  It did, until the suite was pointed at it.
 *
 * THE DIAGNOSTICS ARE NOT AT THE SAME LEVEL IN THE TWO ARMS.  Both announce
 * the message at level 2, but the MP arm's four follow-ups need level 3
 * (`cmpl $0x2`) and the MPnot arm's need only level 2 (`cmpl $0x1`).  That is
 * finding 150's trap exactly, and it is why the test sweeps 0..3.
 *
 * `PrintBase2` (0x20130) is a plain global symbol, so as with the CRC the two
 * copies of its body here are the original's own repetition, not an inlining.
 */
int
V90MP::bitsToInfo(int bit)
{
	unsigned int i;
	int k;
	int rc = 0;
	unsigned char x, sum, n, next, end;
	char str[76];

	if (bit != 0) {
		byte_19++;
		byte_1a = 0;
	} else {
		byte_1a++;
		byte_19 = 0;
	}

	if (byte_1a == 2 * word_114 && byte_1b == 18) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90MP: Ed detected\r\n");
		rc = 3;
	}

	switch (word_14) {
	case 0:
		/* Seventeen ones is the preamble; sixteen are not enough. */
		if (byte_19 > 0x10)
			word_14 = 1;
		break;

	case 1:
		/* The framing zero, or start again. */
		if (bit == 0) {
			word_14 = 2;
		} else {
			byte_1b = 18;
			word_14 = 0;
			byte_19 = 0;
			byte_1a = 0;
		}
		break;

	case 2:
		/*
		 * The type bit.  It is stored as a byte -- `mov %cl` -- so a
		 * `bit` of 0x100 counts as a one for the run counters above
		 * and as a type of zero here.
		 */
		type = (char)bit;
		bits[byte_1b] = (unsigned char)bit;
		byte_1b++;
		n = type ? 0xbb : 0x55;
		byte_119 = n;
		{
			unsigned int want = (unsigned int)n + 1;
			unsigned int q = want / word_114;

			if (q * word_114 == want)
				byte_118 = (unsigned char)want;
			else
				byte_118 = (unsigned char)((q + 1) * word_114);
		}
		word_14 = 3;
		break;

	case 3:
		bits[byte_1b] = (unsigned char)bit;
		next = (unsigned char)(byte_1b + 1);
		n = byte_119;
		if (next != n) {
			byte_1b = next;
			break;
		}

		end = type ? 0xaa : 0x44;

		for (k = 0; k <= 15; k++)
			crc[k] = 1;
		for (i = 0x12; i < end; i++) {
			if (i % 17 == 0)
				i++;
			x = (unsigned char)(crc[0] + bits[i]);
			for (k = 0; k < 15; k++)
				crc[k] = crc[k + 1];
			crc[3] = (unsigned char)((crc[3] + x) & 1);
			crc[10] = (unsigned char)((crc[10] + x) & 1);
			crc[15] = (unsigned char)(x & 1);
		}

		sum = 0;
		for (k = 0; k <= 15; k++) {
			int d = (int)crc[k] - (int)bits[n - 0x10 + k];

			sum += (unsigned char)(d < 0 ? -d : d);
		}

		if (sum == 0) {
			byte_1b = next;
			word_14 = 4;
			break;
		}

		/* One known-bad bit, inverted in place, and try again. */
		if (n > 0x6f)
			bits[0x70] = (unsigned char)(bits[0x70] == 0);

		for (k = 0; k <= 15; k++)
			crc[k] = 1;
		for (i = 0x12; i < end; i++) {
			if (i % 17 == 0)
				i++;
			x = (unsigned char)(crc[0] + bits[i]);
			for (k = 0; k < 15; k++)
				crc[k] = crc[k + 1];
			crc[3] = (unsigned char)((crc[3] + x) & 1);
			crc[10] = (unsigned char)((crc[10] + x) & 1);
			crc[15] = (unsigned char)(x & 1);
		}

		sum = 0;
		for (k = 0; k <= 15; k++) {
			int d = (int)crc[k] - (int)bits[n - 0x10 + k];

			sum += (unsigned char)(d < 0 ? -d : d);
		}

		if (sum == 0) {
			byte_1b = next;
			word_14 = 4;
			if (DSPLIB_DEBUG_VERBOSE())
				dsplibs_debug_printf("V90MP: recieved MP with "
						     "modified good CRC\r\n");
		} else {
			byte_1b = 18;
			byte_19 = 0;
			byte_1a = 0;
			word_14 = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90MP: recieved MP with "
						     "bad CRC\r\n");
		}
		break;

	case 4:
		/* The padding, which is counted and not stored. */
		byte_1b++;
		if (byte_1b != byte_118)
			break;

		evaluateInfo();

		byte_1b = 18;
		word_14 = 0;
		byte_19 = 0;
		byte_1a = 0;

		if (CPack == 0) {
			nofRecievedMp++;
			rc = 1;
			if (nofRecievedMp <= 2) {
				char *p = str;
				unsigned int mask;

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: MP detect"
					    "ed. Type%d,Rate%d,Trellis%d,NonLi"
					    "n%d,Shaping%d,CPack%d\r\n",
					    Type, Rate * 2400, Trellis, NonLin,
					    Shaping, CPack);

				for (mask = 0x2000; mask != 0; mask >>= 1)
					*p++ = (rateMask & mask) ? '1' : '0';
				*p = 0;

				if (DSPLIB_DEBUG_VERBOSE())
					dsplibs_debug_printf("V90MP: Rate Mask"
							     " - %s\r\n", str);
				if (DSPLIB_DEBUG_VERBOSE())
					dsplibs_debug_printf("V90MP: h1 real ="
							     " %d, imag = %d\r"
							     "\n", h1Real,
							     h1Imag);
				if (DSPLIB_DEBUG_VERBOSE())
					dsplibs_debug_printf("V90MP: h2 real ="
							     " %d, imag = %d\r"
							     "\n", h2Real,
							     h2Imag);
				if (DSPLIB_DEBUG_VERBOSE())
					dsplibs_debug_printf("V90MP: h3 real ="
							     " %d, imag = %d\r"
							     "\n", h3Real,
							     h3Imag);
			}
		} else {
			nofRecievedMpNot++;
			rc = 2;
			if (nofRecievedMpNot <= 2) {
				char *q = str;
				unsigned int mask;

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: MPnot det"
					    "ected. Type%d,Rate%d,Trellis%d,No"
					    "nLin%d,Shaping%d,CPack%d\r\n",
					    Type, Rate * 2400, Trellis, NonLin,
					    Shaping, CPack);

				for (mask = 0x2000; mask != 0; mask >>= 1)
					*q++ = (rateMask & mask) ? '1' : '0';
				*q = 0;

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: Rate Mask"
							     " - %s\r\n", str);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: h1 real ="
							     " %d, imag = %d\r"
							     "\n", h1Real,
							     h1Imag);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: h2 real ="
							     " %d, imag = %d\r"
							     "\n", h2Real,
							     h2Imag);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90MP: h3 real ="
							     " %d, imag = %d\r"
							     "\n", h3Real,
							     h3Imag);
			}
		}
		break;
	}

	return rc;
}
