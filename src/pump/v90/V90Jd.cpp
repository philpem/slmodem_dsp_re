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
