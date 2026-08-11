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
 * ===========================================================================
 * The four accessors, 0x11e60, 0x11eb0, 0x11f00 and 0x11f20.
 *
 * THREE OF THEM ARE V90Jd's, INSTRUCTION FOR INSTRUCTION, AND THE FOURTH IS
 * NOT WHERE ITS SIBLING IS.  Reading the four `this`-relative displacements
 * against this class's map:
 *
 *     getJdPhase           +0x4a..+0x59   phaseBits[0..15]
 *     getRatesMask         +0x02..+0x1d   bits[0..15], bits[16..27]
 *     getConstelationSize  +0x67, +0x68   phaseBits[29], phaseBits[30]
 *     getMaxLookahead      +0x20, +0x21   bits[30], bits[31]
 *
 * `getRatesMask` and `getMaxLookahead` are byte-identical to V90Jd's and read
 * `bits`; `getConstelationSize` reads the OTHER vector, and one index further
 * along than V90Jd's does -- V90Jd's is `bits[28], bits[29]` and this one is
 * `phaseBits[29], phaseBits[30]`, which is +0x48 (the second vector's
 * displacement) plus one.  Reproduced and recorded as D271, not reconciled.
 *
 * THE INDEX IS EXPLAINED AND THE VECTOR IS NOT.  All four read the UNFRAMED
 * layout the unpackers fill (D270), and in the phase message payload 28 is the
 * always-1 tag `unPackJdPhaseData` checks, so the constellation pair sits one
 * later there than in the data message -- payload 29..30 rather than 28..29.
 * What no reading accounts for is that this accessor takes them out of
 * `phaseBits` while its two siblings read `bits`: a V92Jd that has received a
 * data message answers `getConstelationSize` out of the phase vector.  D271 is
 * that half.
 *
 * THE PHASE IS THE CONSTRUCTOR'S CONVERSION RUN BACKWARDS.  Sixteen bytes of
 * `phaseBits` are gathered into an integer, least significant first, and
 * scaled by `.rodata.cst4+0x9c` = 1.52587890625e-05 = 2**-16 -- the reciprocal
 * of the constructor's `65536.0f` at +0x94.  So the field is Q16 and the round
 * trip is exact for any phase the constructor's low sixteen bits carried.
 *
 * THE ACCUMULATOR IS UNSIGNED, and the object had to say so: it pushes a zero
 * high word and loads the pair with `fildll`, which is how GCC converts an
 * `unsigned int` to a float -- a signed `int` would have been one `fildl` of
 * the value in place.  Sixteen bits cannot make the two differ in value; the
 * declared type is what the codegen tier sees, so it is written as the object
 * compiled it.  The counter is unsigned too (`jbe`), like the phase loop in
 * the constructor and unlike the two mask loops.
 *
 * THE DIVISION IS THE SOURCE'S AND THE MULTIPLY IS THE COMPILER'S.  Dividing
 * by 65536.0f and multiplying by 2**-16 are the same function of the same
 * argument -- the scale is a power of two, so neither rounds -- and GCC turns
 * the first into the second.  It is written as the division because the
 * constructor states the same constant that way round.
 * ===========================================================================
 */
float
V92Jd::getJdPhase()
{
	unsigned int phase = 0;
	unsigned int i;

	for (i = 0; i <= 15; i++)
		if (phaseBits[i])
			phase |= 1u << i;

	return (float)phase / 65536.0f;
}

int
V92Jd::getRatesMask()
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
V92Jd::getConstelationSize(unsigned char *first, unsigned char *second)
{
	*first = phaseBits[29];
	*second = phaseBits[30];
}

unsigned char
V92Jd::getMaxLookahead()
{
	return (unsigned char)((bits[30] & 1) + ((bits[31] & 1) << 1));
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

/*
 * ===========================================================================
 * The two unpackers, 0x128e0 (1003 bytes) and 0x12500 (987 bytes).  Both are
 * V90Jd::unPackData's state machine -- see V90Jd.cpp for the whole of the
 * reading, which is not repeated here -- and both write the SAME
 * payload-contiguous layout the four accessors read, one byte per received
 * bit starting at their own vector's index 0, with no framing stored.  The
 * framed position 18+p is payload p for p in 0..15, 35+(p-16) for 16..31 and
 * 52+(p-32) for 32..47, in all three functions.
 *
 * THE THREE DIFFERENCES ARE THE POINT, and every one of them is a byte in the
 * disassembly rather than an argument from the protocol:
 *
 *   1. THE SECOND RATE RUN IS SPLIT, 11 + 1, WHERE V.90's IS 12.  The data
 *      unpacker's fourth state ends on `cmp $0x1b,%al` at 0x129cd -- eleven
 *      bytes, not V90Jd's twelve at `cmp $0x1c` -- and a state of its own at
 *      0x129e3 takes the twelfth with NO comparison at all, storing one byte
 *      and advancing unconditionally.  That is the extra jump-table entry:
 *      `cmp $0x9,%eax` over a ten-entry table at .rodata+0x5bc against
 *      V90Jd's `cmp $0x8` over nine.  It is the same eleven the constructor's
 *      `cmp $0xa,%edx; jle` writes (finding 1223), so the bit the twelfth
 *      state reads -- payload 27, framed 46 -- is the one V.92 does not use
 *      as a rate bit and `packJdData` forces to zero.  The BYTE RANGE written
 *      is unchanged: still bits[16..27] over the two states together.
 *
 *      The phase unpacker does NOT split it: `cmp $0x1c,%al` at 0x125f9, nine
 *      cases, `cmp $0x8,%eax` over the table at .rodata+0x598 -- V.90's shape
 *      exactly, on the other vector.
 *
 *   2. EACH ONE CHECKS A CONSTANT BIT BEFORE IT DECLARES A MESSAGE, and the
 *      two constants disagree.  V90Jd::unPackData returns 1 as soon as the
 *      trailing count reaches 0x34.  These two test one byte first:
 *      `cmpb $0x0,0x1e(%ebx)` at 0x12cac is bits[28], and `cmpb $0x1,
 *      0x66(%ebx)` at 0x128bc is phaseBits[28].  A mismatch takes the reset
 *      path instead and returns 0.
 *
 *      Payload 28 is framed 47, which is exactly the byte each constructor
 *      writes as a constant and neither pack touches: `bits[47] = 0` and
 *      `phaseBits[47] = 1`.  So the pair is a message-type tag -- the
 *      receiver refuses a phase message on the data unpacker and a data
 *      message on the phase one -- and it also settles D162: the constructor
 *      does not fill `bits[48]` because `packJdData` clears it, whereas
 *      `bits[47]` has to be written because it is the tag.
 *
 *   3. THE VECTOR AND THE STATE WORD.  The data unpacker stores through
 *      `0x2(%ebx,...)` and switches on +0xd4; the phase one through
 *      `0x4a(...,%ebx,1)` and +0xd8.  Their CRC registers are the SAME
 *      sixteen ints at +0x94, which is the sharing V92Jd.h already records
 *      for the two packs.  The restart in each is its own reset method's
 *      three stores in that method's order -- +0xd4 at 0x1297e and +0xd8 at
 *      0x125a6 -- and is written as a call to it.
 *
 * The single byte the unpackers agree on and V90Jd's does not have is the
 * check in 2; everything else in the phase unpacker is V90Jd::unPackData with
 * two displacements changed.
 * ===========================================================================
 */
int
V92Jd::unPackJdData(int bit)
{
	unsigned char ones = 0;
	unsigned char n;
	int sum;
	int i, k;

	if (bit)
		ones = (unsigned char)(unpack[0] + 1);

	switch (unpackWord) {
	case 0:
		unpack[0] = ones;
		if (ones > 16)
			unpackWord = 1;
		break;

	case 1:
		if (bit) {
			unPackJdReset();
			break;
		}
		unpack[0] = ones;
		unpackWord = 2;
		break;

	case 2:
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 16)
			unpackWord = 3;
		break;

	case 3:
		if (bit) {
			unPackJdReset();
			break;
		}
		unpack[0] = ones;
		unpackWord = 4;
		break;

	case 4:
		/* Eleven, where V90Jd::unPackData takes twelve. */
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 27)
			unpackWord = 5;
		break;

	case 5:
		/* And the twelfth on its own, unconditionally. */
		n = unpack[1];
		unpack[1] = (unsigned char)(n + 1);
		unpack[0] = ones;
		bits[n] = (unsigned char)bit;
		unpackWord = 6;
		break;

	case 6:
		n = unpack[1];
		bits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 32)
			unpackWord = 7;
		break;

	case 7:
		if (bit) {
			unPackJdReset();
			break;
		}
		unpack[0] = ones;
		unpackWord = 8;
		break;

	case 8:
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
			unPackJdReset();
			break;
		}

		unpack[1] = 48;
		unpack[0] = ones;
		unpackWord = 9;
		break;

	case 9:
		/* The four trailing bits, then the data message's own tag. */
		n = unpack[1];
		n++;
		if (n != 0x34) {
			unpack[1] = n;
			unpack[0] = ones;
			break;
		}
		if (bits[28] != 0) {
			unPackJdReset();
			break;
		}
		unpack[1] = 0x34;
		unpack[0] = ones;
		return 1;

	default:
		unpack[0] = ones;
		break;
	}

	return 0;
}

int
V92Jd::unPackJdPhaseData(int bit)
{
	unsigned char ones = 0;
	unsigned char n;
	int sum;
	int i, k;

	if (bit)
		ones = (unsigned char)(unpack[0] + 1);

	switch (unpackPhaseWord) {
	case 0:
		unpack[0] = ones;
		if (ones > 16)
			unpackPhaseWord = 1;
		break;

	case 1:
		if (bit) {
			unPackJdPhaseReset();
			break;
		}
		unpack[0] = ones;
		unpackPhaseWord = 2;
		break;

	case 2:
		n = unpack[1];
		phaseBits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 16)
			unpackPhaseWord = 3;
		break;

	case 3:
		if (bit) {
			unPackJdPhaseReset();
			break;
		}
		unpack[0] = ones;
		unpackPhaseWord = 4;
		break;

	case 4:
		/* Twelve here, as V90Jd::unPackData has it. */
		n = unpack[1];
		phaseBits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 28)
			unpackPhaseWord = 5;
		break;

	case 5:
		n = unpack[1];
		phaseBits[n] = (unsigned char)bit;
		unpack[0] = ones;
		n++;
		unpack[1] = n;
		if (n == 32)
			unpackPhaseWord = 6;
		break;

	case 6:
		if (bit) {
			unPackJdPhaseReset();
			break;
		}
		unpack[0] = ones;
		unpackPhaseWord = 7;
		break;

	case 7:
		n = unpack[1];
		phaseBits[n] = (unsigned char)bit;
		n++;
		if (n != 48) {
			unpack[1] = n;
			unpack[0] = ones;
			break;
		}

		for (i = 0; i <= 15; i++)
			crc[i] = 1;

		for (k = 0; k <= 31; k++) {
			int t = phaseBits[k] + crc[0];
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
			int d = crc[i] - phaseBits[32 + i];

			sum += d < 0 ? -d : d;
		}
		if (sum != 0) {
			unPackJdPhaseReset();
			break;
		}

		unpack[1] = 48;
		unpack[0] = ones;
		unpackPhaseWord = 8;
		break;

	case 8:
		/* The four trailing bits, then the phase message's own tag. */
		n = unpack[1];
		n++;
		if (n != 0x34) {
			unpack[1] = n;
			unpack[0] = ones;
			break;
		}
		if (phaseBits[28] != 1) {
			unPackJdPhaseReset();
			break;
		}
		unpack[1] = 0x34;
		unpack[0] = ones;
		return 1;

	default:
		unpack[0] = ones;
		break;
	}

	return 0;
}
