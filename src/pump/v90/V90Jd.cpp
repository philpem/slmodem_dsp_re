/*
 * V90Jd.cpp -- V.90 Jd message packing.
 *
 * Reconstructed from dsplibs.o V90Jd.cpp.  Two of the class's thirteen
 * methods: `getBitVector()` and `unPackReset()`, which are the two that are
 * leaves.  `include/dsplib/V90Jd.h` carries the object map.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x4(%esp),%eax` -- not %ecx, so these are not thiscall and
 * nothing here needs an attribute (finding 215).
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here;
 * see the Makefile.  No virtuals, no allocation, no static data members, so
 * the test binaries still link with $(CC).
 */

#include <stddef.h>

#include "dsplib/V90Jd.h"
#include "dsplib/V90Parameters.h"	/* the constructor's five fields */

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class has to assert its own -- and it is exactly the check that
 * catches an object right in size and wrong by four in every offset, which is
 * the failure docs/v90cpp.md warns about for the classes that DO have a vptr.
 */
#define V90JD_OFF(field, off, tag) \
	typedef char v90jd_off_##tag[ \
	    ((int)__builtin_offsetof(V90Jd, field) == (off)) ? 1 : -1]

V90JD_OFF(unpack,     0x00, unpack);
V90JD_OFF(bits,       0x02, bits);
V90JD_OFF(crc,        0x4c, crc);
V90JD_OFF(unpackWord, 0x8c, unpackword);
typedef char v90jd_size[(sizeof(V90Jd) == 0x90) ? 1 : -1];

/*
 * ===========================================================================
 * The constructor, 0x1e790, and it is where the header's bit map comes from.
 *
 * Five fields of V90Parameters, and the author's own names for all five
 * (finding 860's extraction of `loadParams`):
 *
 *     DIGITAL_RATE_MASK             +0x2c  28 bits, spread over two groups
 *     MAX_SPECTRAL_SHAPER_LOOKAHEAD +0x30  two bits
 *     V34_PHASE4_CONSTELLATION      +0x34  one byte
 *     V34_RRN_CONSTELLATION         +0x38  one byte
 *
 * THE TWO GROUPS ARE 16 AND 12 BITS, NOT 14 AND 14.  The first loop is
 * `cmp $0xf,%ecx; jle` and the second `cmp $0xb,%edx; jle`, so bits 0..15 of
 * the mask land in `bits[18..33]` and bits 16..27 in `bits[35..46]` -- 28
 * rates, with the group boundary falling inside the mask rather than between
 * two halves of it.  V92Jd's second loop stops one earlier; see there.
 *
 * BOTH COUNTERS ARE SIGNED.  `jle`, not `jbe`, in both loops here, against
 * V92Jd's `jbe` on its phase loop.  That is the induction variable's declared
 * type showing through, which the codegen tier can see and the differential
 * tier cannot, so it is written the way the object was compiled: `int` here.
 *
 * The store order below is the object's.  GCC is free to reorder it and does
 * (CLAUDE.md's rule for reading a codegen difference); what is not free is
 * which byte gets which value.
 * ===========================================================================
 */
V90Jd::V90Jd(V90Parameters *params)
{
	int mask;
	int i;

	unpack[0] = 0;
	unpack[1] = 0;
	unpackWord = 0;

	/*
	 * The maximum lookahead, two bits.  `movzbl` then `and $1` and
	 * `shr $1; and $1` -- the low two bits of the parameter's low byte,
	 * and nothing above them can reach the message.
	 */
	bits[49] = (unsigned char)(params->MAX_SPECTRAL_SHAPER_LOOKAHEAD & 1);
	bits[50] = (unsigned char)
	    ((params->MAX_SPECTRAL_SHAPER_LOOKAHEAD >> 1) & 1);

	/*
	 * The constellation size, two bits, one parameter each -- a whole-word
	 * load and a byte store, so only the low byte of either is carried.
	 */
	bits[47] = (unsigned char)params->V34_PHASE4_CONSTELLATION;
	bits[48] = (unsigned char)params->V34_RRN_CONSTELLATION;

	mask = params->DIGITAL_RATE_MASK;
	for (i = 0; i <= 15; i++)
		bits[V90JD_GROUP1 + 1 + i] = (unsigned char)((mask >> i) & 1);
	for (i = 0; i <= 11; i++)
		bits[V90JD_GROUP2 + 1 + i] =
		    (unsigned char)((mask >> (i + 16)) & 1);
}

/*
 * One byte of `ret` in the blob, at 0x1e890 and 0x1e8a0.  It exists here
 * because the object has the symbols and a caller that destroys a V90Jd needs
 * them; see the header for why an implicit one would not do.
 */
V90Jd::~V90Jd()
{
}

/*
 * ===========================================================================
 * The three accessors, 0x1e8b0, 0x1e900 and 0x1e920 -- AND THEY DO NOT READ
 * THE LAYOUT THE CONSTRUCTOR WRITES.
 *
 * The constructor and `getBitVector` build a FRAMED message: seventeen 1 bits,
 * then a 0 at `bits[17]`, the low sixteen mask bits at `bits[18..33]`, a 0 at
 * `bits[34]`, the high twelve at `bits[35..46]`, and the two small fields at
 * `bits[47..48]` and `bits[49..50]`.  These three read
 *
 *     getRatesMask         +0x02..+0x11 and +0x12..+0x1d   bits[0..15], [16..27]
 *     getConstelationSize  +0x1e, +0x1f                    bits[28], bits[29]
 *     getMaxLookahead      +0x20, +0x21                    bits[30], bits[31]
 *
 * -- a payload-contiguous layout with no group markers in it, 28 rate bits
 * then two and two, ending at `bits[31]`.  Nothing that PACKS in this class
 * writes that, so the accessors and the packer disagree about where the same
 * four quantities live.  Recorded as D270 rather than reconciled: the
 * disassembly is unambiguous, both readings are reproduced as the object has
 * them, and each is driven against the blob.
 *
 * A BYTE COUNTS AS SET IF IT IS NON-ZERO, and that is not the same rule the
 * two small fields use.  `getRatesMask` tests `cmpb $0x0`, so a byte of 2
 * contributes its bit; `getMaxLookahead` does `and $0x1` on both of its, so a
 * byte of 2 contributes nothing.  The test seeds the vector with varied bytes
 * rather than with 0 and 1 so that the two rules are told apart.
 *
 * THE RETURN TYPES ARE MEASURED, not assumed -- a return type is not mangled.
 * `getRatesMask` leaves a 32-bit value in `%eax` (`mov %edx,%eax` on an
 * accumulator built with `shl`/`or`), so it returns an `int`.
 * `getMaxLookahead` ends `movzbl %dl,%eax` on a sum computed in a byte
 * register, which is an `unsigned char` result widened at the return.
 * `getConstelationSize` writes through both pointers and sets `%eax` to
 * nothing, so it returns void.
 * ===========================================================================
 */
int
V90Jd::getRatesMask()
{
	int mask = 0;
	int i;

	for (i = 0; i <= 15; i++)
		if (bits[i])
			mask |= 1 << i;

	for (i = 0; i <= 11; i++)
		if (bits[16 + i])
			mask |= 1 << (i + 16);

	return mask;
}

void
V90Jd::getConstelationSize(unsigned char *first, unsigned char *second)
{
	*first = bits[28];
	*second = bits[29];
}

/*
 * The high bit is `bits[31]` and the low one `bits[30]`, which is the order
 * `setMaxLookahead` and the constructor use for the pair they write -- at
 * `bits[50]` and `bits[49]`, nineteen bytes further on.
 */
unsigned char
V90Jd::getMaxLookahead()
{
	return (unsigned char)((bits[30] & 1) + ((bits[31] & 1) << 1));
}

/*
 * The CRC-16 the message carries, as the object computes it: sixteen ints,
 * one per bit, shifting down toward crc[0], with the feedback
 *
 *     t = <input bit> + crc[0]
 *
 * folded into positions 3, 10 and 15.  The three sums are `add` and the
 * result is masked to one bit afterwards, which is XOR spelled as addition;
 * that is why the input byte is used unmasked -- only its low bit can reach
 * the answer.
 *
 * Sixteen ints to hold sixteen bits is the original's choice, not a
 * convenience here: the register lives at +0x4c as `int crc[16]` and
 * `resetCrc()` writes it there.
 */
static void
v90jd_crc_bits(int *crc, const unsigned char *in)
{
	int k;

	for (k = 0; k <= 15; k++) {
		int t = in[k] + crc[0];
		int tap3 = (crc[4] + t) & 1;
		int tap10 = (crc[11] + t) & 1;
		int i;

		for (i = 0; i <= 14; i++)
			crc[i] = crc[i + 1];
		crc[3] = tap3;
		crc[10] = tap10;
		crc[15] = t & 1;
	}
}

unsigned char *
V90Jd::getBitVector()
{
	int i, g;

	/* Group 0 is seventeen 1 bits; the later three each open with a 0. */
	for (i = 0; i <= 16; i++)
		bits[i] = 1;
	bits[V90JD_GROUP1] = 0;
	bits[V90JD_GROUP2] = 0;
	bits[V90JD_GROUP3] = 0;
	bits[68] = 0;
	bits[69] = 0;
	bits[70] = 0;
	bits[71] = 0;

	/* The CRC register starts all ones. */
	for (i = 0; i <= 15; i++)
		crc[i] = 1;

	/*
	 * Over groups 1 and 2, sixteen bits each, skipping the leading 0 of
	 * each: the object walks a pointer that starts at `this + 0x14` and
	 * advances by the 17-byte stride once.
	 */
	for (g = 0; g <= 1; g++)
		v90jd_crc_bits(crc, &bits[V90JD_GROUP1 + 1 + g * V90JD_GROUP]);

	/* And group 3 is the CRC, low bit first. */
	for (i = 0; i <= 15; i++)
		bits[V90JD_GROUP3 + 1 + i] = (unsigned char)crc[i];

	return bits;
}

void
V90Jd::unPackReset()
{
	unpack[1] = 0;
	unpackWord = 0;
	unpack[0] = 0;
}

/*
 * ===========================================================================
 * The unpacker, 0x1eba0, 879 bytes -- AND IT IS THE PRODUCER THE THREE
 * ACCESSORS WERE WRITTEN FOR.  D270 asked whether anything writes the
 * payload-contiguous layout `getRatesMask` and friends read.  This does, and
 * nothing else in the class does:
 *
 *     bits[ 0..15]   `mov %cl,0x2(%ebx,%esi,1)` in the case at 0x1ec51
 *     bits[16..27]   the same store at 0x1ec89
 *     bits[28..31]   the same store at 0x1ecaf
 *     bits[32..47]   the same store at 0x1ecfd -- the CRC as received
 *
 * -- written strictly in arrival order, one byte per received bit, starting
 * at `bits[0]`, and no group marker is ever stored.  So the class has two
 * layouts because it has two directions: the constructor and `getBitVector`
 * build the FRAMED message for the wire, and the unpacker strips the framing
 * off an incoming one and leaves the payload flat for the accessors.  Framed
 * position 18+p is payload p for p in 0..15, 35+(p-16) for p in 16..31 and
 * 52+(p-32) for p in 32..47, which is exactly the run the two sides agree on.
 *
 * THE THREE STATE FIELDS, now that something reads them.  `unpack[0]` is the
 * length of the current run of 1 bits -- the entry sequence is `test %ecx;
 * je` over `movzbl (%ebx); inc %al`, so it is set to `unpack[0] + 1` on a
 * non-zero bit and to 0 on a zero one, as a BYTE, wrapping at 255.
 * `unpack[1]` is how many payload bytes have been stored, and `unpackWord` is
 * the state: `cmp $0x8,%eax; ja <default>` over a nine-entry jump table at
 * .rodata+0x794, so `switch` on an `int` with cases 0 through 8.
 *
 * THE RETURN TYPE IS `int`, measured: every path but one reaches `xor %edx,
 * %edx` before `mov %edx,%eax`, and the case-8 completion at 0x1ec18 jumps
 * past it with `%edx` holding 1.  A return type is not mangled, so this is
 * the only way to know it.
 *
 * The state machine walks the framing the packer writes:
 *
 *     0  hunt          `cmp $0x10,%dl; jbe` -- seventeen 1 bits, unsigned
 *     1  group 1's 0   a 1 here restarts
 *     2  16 payload    `cmp $0x10,%al`
 *     3  group 2's 0   a 1 here restarts
 *     4  12 payload    `cmp $0x1c,%al`   (V92Jd's data unpacker splits this)
 *     5  4 payload     `cmp $0x20,%al`   -- constellation size and lookahead
 *     6  group 3's 0   a 1 here restarts
 *     7  16 payload    `cmp $0x30,%al`, then the CRC is checked
 *     8  4 trailing    `cmp $0x34,%al`, then 1 is returned
 *
 * THE RESTART IS `unPackReset()`'s THREE STORES IN ITS ORDER, byte for byte,
 * at 0x1ec40: `movb $0x0,0x1(%ebx)`, then the word, then `movb $0x0,(%ebx)`.
 * That is consistent with an inlined call to it and is written as one; the
 * run length computed on the way in is discarded, so a stray 1 where a marker
 * belongs does not count toward the next seventeen.
 *
 * THE CRC IS `getBitVector`'s, over ONE run of thirty-two rather than two of
 * sixteen: `cmpl $0x1f,0x3c(%esp)` where the packer's helper stops at 15 and
 * is called twice with a 17-byte stride.  The payload is contiguous here, so
 * the two are the same thirty-two bytes in the same order and the register
 * agrees with what the packer put on the wire.  It is written inline rather
 * than through `v90jd_crc_bits` because the object's loop bound is 31.
 *
 * THE COMPARISON IS A SUM OF ABSOLUTE DIFFERENCES, not a bitwise test:
 * `sub %edi,%edx; mov %edx,%eax; sar $0x1f,%eax; xor %eax,%edx; sub %eax,%edx`
 * accumulated over sixteen and tested for zero.  Since the received byte is
 * stored unmasked, a CRC byte of 2 fails here where `(x ^ y) & 1` would pass,
 * and the test drives exactly that.
 * ===========================================================================
 */
int
V90Jd::unPackData(int bit)
{
	unsigned char ones = 0;
	unsigned char n;
	int sum;
	int i, k;

	if (bit)
		ones = (unsigned char)(unpack[0] + 1);

	switch (unpackWord) {
	case 0:
		/* Group 0: seventeen 1 bits, counted as an unsigned byte. */
		unpack[0] = ones;
		if (ones > 16)
			unpackWord = 1;
		break;

	case 1:
		/* Group 1's leading 0. */
		if (bit) {
			unPackReset();
			break;
		}
		unpack[0] = ones;
		unpackWord = 2;
		break;

	case 2:
		/* The low sixteen rate bits. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 16)
			unpackWord = 3;
		break;

	case 3:
		/* Group 2's leading 0. */
		if (bit) {
			unPackReset();
			break;
		}
		unpack[0] = ones;
		unpackWord = 4;
		break;

	case 4:
		/* The high twelve rate bits. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 28)
			unpackWord = 5;
		break;

	case 5:
		/* The constellation size and the maximum lookahead. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 32)
			unpackWord = 6;
		break;

	case 6:
		/* Group 3's leading 0. */
		if (bit) {
			unPackReset();
			break;
		}
		unpackWord = 7;
		unpack[0] = ones;
		break;

	case 7:
		/* The sixteen CRC bits, then the check. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		n++;
		if (n != 48) {
			unpack[1] = n;
			unpack[0] = ones;
			break;
		}

		for (i = 0; i <= 15; i++)
			crc[i] = 1;

		for (k = 0; k <= 31; k++) {
			int t = bits[k] + crc[0];
			int tap3 = (crc[4] + t) & 1;
			int tap10 = (crc[11] + t) & 1;

			for (i = 0; i <= 14; i++)
				crc[i] = crc[i + 1];
			crc[3] = tap3;
			crc[10] = tap10;
			crc[15] = t & 1;
		}

		sum = 0;
		for (i = 0; i <= 15; i++) {
			int d = crc[i] - bits[32 + i];

			sum += d < 0 ? -d : d;
		}
		if (sum != 0) {
			unPackReset();
			break;
		}

		unpack[1] = 48;
		unpackWord = 8;
		unpack[0] = ones;
		break;

	case 8:
		/* The four trailing bits, whose values are not looked at. */
		n = unpack[1];
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 0x34)
			return 1;
		break;

	default:
		unpack[0] = ones;
		break;
	}

	return 0;
}
