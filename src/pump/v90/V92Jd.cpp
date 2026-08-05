/*
 * V92Jd.cpp -- V.92 Jd message packing, data and phase.
 *
 * Reconstructed from dsplibs.o V92Jd.cpp.  Six of the class's nineteen
 * methods: the two packs, the two accessors that call them, and the two
 * unpack resets.  `include/dsplib/V92Jd.h` carries the object map and says
 * where it does and does not follow V90Jd's.
 *
 * Plain cdecl, `this` as the first stack argument (finding 215).  The CRC
 * helper is a duplicate of V90Jd.cpp's rather than shared: the original keeps
 * these in two translation units and each carries its own copy, and a shared
 * header would be a claim about the original that nothing here supports.
 */

#include <stddef.h>

#include "dsplib/V92Jd.h"

/* Hold the compiler to the header's map; offcheck.py cannot see a class. */
#define V92JD_OFF(field, off, tag) \
	typedef char v92jd_off_##tag[ \
	    ((int)__builtin_offsetof(V92Jd, field) == (off)) ? 1 : -1]

V92JD_OFF(unpack,          0x00, unpack);
V92JD_OFF(bits,            0x02, bits);
V92JD_OFF(phaseBits,       0x4a, phasebits);
V92JD_OFF(crc,             0x94, crc);
V92JD_OFF(unpackWord,      0xd4, unpackword);
V92JD_OFF(unpackPhaseWord, 0xd8, unpackphaseword);
typedef char v92jd_size[(sizeof(V92Jd) == 0xdc) ? 1 : -1];

/*
 * The same CRC-16 V90Jd.cpp computes: sixteen ints shifting down toward
 * crc[0], feedback t = <input bit> + crc[0] folded into positions 3, 10 and
 * 15.  Addition with a one-bit mask afterwards is XOR, which is why the input
 * byte goes in unmasked -- only its low bit can reach the answer.
 */
static void
v92jd_crc_bits(int *crc, const unsigned char *in)
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

void
V92Jd::packJdData()
{
	int i, g;

	for (i = 0; i <= 16; i++)
		bits[i] = 1;
	bits[V90JD_GROUP1] = 0;
	bits[V90JD_GROUP2] = 0;
	bits[V90JD_GROUP3] = 0;
	bits[68] = 0;
	bits[69] = 0;
	bits[70] = 0;
	bits[71] = 0;

	/*
	 * And the part V90Jd::getBitVector does not do: six of the second
	 * rate-mask run and one of the two constellation-size bits are forced
	 * to zero before the CRC sees them.
	 */
	for (i = 0; i <= 5; i++)
		bits[V90JD_GROUP2 + 7 + i] = 0;
	bits[V90JD_GROUP2 + 14] = 0;

	for (i = 0; i <= 15; i++)
		crc[i] = 1;

	for (g = 0; g <= 1; g++)
		v92jd_crc_bits(crc,
			       &bits[V90JD_GROUP1 + 1 + g * V90JD_GROUP]);

	for (i = 0; i <= 15; i++)
		bits[V90JD_GROUP3 + 1 + i] = (unsigned char)crc[i];
}

void
V92Jd::packJdPhaseData()
{
	int i, g;

	for (i = 0; i <= 16; i++)
		phaseBits[i] = 1;
	phaseBits[V90JD_GROUP1] = 0;
	phaseBits[V90JD_GROUP2] = 0;
	phaseBits[V90JD_GROUP3] = 0;
	phaseBits[68] = 0;
	phaseBits[69] = 0;
	phaseBits[70] = 0;
	phaseBits[71] = 0;

	/*
	 * Twelve here, not six, and from position 1 rather than 7: the phase
	 * message clears the whole of group 2's second rate-mask run, and its
	 * last bit rather than the constellation-size one.  Only +0x79..+0x7b
	 * survive, which is exactly what the constructor and setConstelSize
	 * write.
	 */
	for (i = 0; i <= 11; i++)
		phaseBits[V90JD_GROUP2 + 1 + i] = 0;
	phaseBits[V90JD_GROUP2 + 16] = 0;

	for (i = 0; i <= 15; i++)
		crc[i] = 1;

	for (g = 0; g <= 1; g++)
		v92jd_crc_bits(crc,
			       &phaseBits[V90JD_GROUP1 + 1 + g * V90JD_GROUP]);

	for (i = 0; i <= 15; i++)
		phaseBits[V90JD_GROUP3 + 1 + i] = (unsigned char)crc[i];
}

unsigned char *
V92Jd::getJdBitVector()
{
	packJdData();
	return bits;
}

unsigned char *
V92Jd::getJdPhaseBitVector()
{
	packJdPhaseData();
	return phaseBits;
}

void
V92Jd::unPackJdReset()
{
	unpack[1] = 0;
	unpackWord = 0;
	unpack[0] = 0;
}

void
V92Jd::unPackJdPhaseReset()
{
	unpack[1] = 0;
	unpackPhaseWord = 0;
	unpack[0] = 0;
}
