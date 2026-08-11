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
