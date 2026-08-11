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
#include "dsplib/V90Parameters.h"	/* the constructor's three fields */

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
 * ===========================================================================
 * The constructor, 0x11c80.  Three fields of V90Parameters, and one of them
 * goes through the coprocessor.
 *
 * THE PHASE IS A FIXED-POINT CONVERSION, 16 BITS OF FRACTION.  The object
 * loads 65536.0f from .rodata.cst4+0x94, multiplies by `V92_JD_PHASE`, sets
 * the x87 rounding mode to truncate (`fnstcw`, `or $0xc00`, `fldcw`) and
 * stores with `fistpll` -- a SIXTY-FOUR bit store, of which it then reads only
 * the low word back.  So the source's intermediate is a 64-bit integer type:
 * a 32-bit one would have emitted `fistpl`.  That distinction is invisible to
 * the differential test for any phase in range and is written the way the
 * object was compiled, because the two spellings differ for a phase that
 * overflows an `int` and the object's answer there is the one we want.
 *
 * The 0xc00 control word is round-toward-zero, which is C's own rule for a
 * float-to-integer conversion, so the cast IS the sequence and no rounding
 * helper is needed.
 *
 * THE RATE MASK IS ONE BIT SHORTER THAN V.90's.  `cmp $0xa,%edx; jle` -- 0
 * through 10, eleven bits in the second group against V90Jd's twelve, so 27
 * rates rather than 28.  Both loops are otherwise the same shape as V90Jd's
 * and land on the same offsets in `bits`.
 *
 * AND THE PHASE LOOP'S COUNTER IS UNSIGNED where the two mask loops' are
 * signed: `cmp $0xf,%ecx; jbe` against `jle`.  Written that way here for the
 * codegen tier; the values are identical either way.
 *
 * `bits[48]` IS NOT WRITTEN.  V90Jd's constructor fills both constellation
 * bits from two parameters; this one stores 0 into `bits[47]` and leaves
 * `bits[48]` holding whatever was in the storage.  Recorded as D162,
 * unmeasured -- `packJdData` is not written here and may fill it later.
 * ===========================================================================
 */
V92Jd::V92Jd(V90Parameters *params)
{
	long long scaled;
	int phase;
	int mask;
	unsigned u;
	int i;

	unpack[0] = 0;
	unpack[1] = 0;
	bits[47] = 0;
	phaseBits[47] = 1;
	unpackWord = 0;
	unpackPhaseWord = 0;

	scaled = (long long)(65536.0f * params->V92_JD_PHASE);
	phase = (int)scaled;
	for (u = 0; u <= 15; u++)
		phaseBits[V90JD_GROUP1 + 1 + u] =
		    (unsigned char)((phase & (1 << u)) != 0);

	/* The maximum lookahead, two bits, high one stored first. */
	bits[50] = (unsigned char)
	    ((params->V92_MAX_SPECTRAL_SHAPER_LOOKAHEAD >> 1) & 1);
	bits[49] = (unsigned char)
	    (params->V92_MAX_SPECTRAL_SHAPER_LOOKAHEAD & 1);

	mask = params->V92_DIGITAL_RATE_MASK;
	for (i = 0; i <= 15; i++)
		bits[V90JD_GROUP1 + 1 + i] = (unsigned char)((mask >> i) & 1);
	for (i = 0; i <= 10; i++)
		bits[V90JD_GROUP2 + 1 + i] =
		    (unsigned char)((mask >> (i + 16)) & 1);

	phaseBits[48] = 0;
	phaseBits[49] = 0;
}

/* One byte of `ret` in the blob, at 0x11e40 and 0x11e50. */
V92Jd::~V92Jd()
{
}

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
