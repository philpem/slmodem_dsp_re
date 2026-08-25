/*
 * V90Jd.h -- the V.90 Jd message, one byte per bit.
 *
 * Reconstructed from dsplibs.o V90Jd.cpp.  `V90Jd` is not polymorphic --
 * tools/cppstruct.py lists its destructor with the two ordinary variants and
 * not the deleting one, and GCC emits a deleting destructor only for a
 * virtual one -- so offset 0 is a real member and there is no vptr.  Finding
 * F228 is the four classes where that is not true.
 *
 * THE OBJECT IS MOSTLY A BIT VECTOR.  `getBitVector()` returns `this + 2`,
 * and everything from there to +0x49 is 72 bytes each holding 0 or 1.  Its
 * geometry is a 17-byte stride, which is what makes the offsets in the
 * accessors legible:
 *
 *     bits[ 0 ..16]   group 0: seventeen 1 bits
 *     bits[17]        group 1's leading 0
 *     bits[18 ..33]   rate mask, bits 0..15, least significant first
 *     bits[34]        group 2's leading 0
 *     bits[35 ..46]   rate mask, bits 16..27
 *     bits[47 ..48]   constellation size, 2 bits
 *     bits[49 ..50]   maximum lookahead, 2 bits
 *     bits[51]        group 3's leading 0
 *     bits[52 ..67]   the CRC computed over groups 1 and 2
 *     bits[68 ..71]   four trailing 0 bits
 *
 * That map is read out of the constructor and the accessors, not out of a
 * protocol document: `V90Jd::V90Jd(V90Parameters *)` copies the parameter
 * block's +0x2c word bit by bit into +0x14..+0x23 and +0x25..+0x30, its +0x34
 * and +0x38 into +0x31 and +0x32, and the two low bits of its +0x30 into
 * +0x33 and +0x34; `setRatesMask` writes +0x14 and +0x25, `setConstelSize`
 * +0x31 and +0x32, `setMaxLookahead` +0x33 and +0x34.  The names are the
 * author's own, from the method names the mangling preserves.
 *
 * THAT MAP IS THE TRANSMIT SIDE ONLY.  `unPackData` fills a second, flat one
 * in the same bytes -- `bits[0..27]` the rate mask, `bits[28..29]` the
 * constellation size, `bits[30..31]` the lookahead and `bits[32..47]` the CRC
 * as received, with no group markers anywhere -- and the three accessors read
 * that one.  Framed position 18+p is payload p for p in 0..15, 35+(p-16) for
 * 16..31 and 52+(p-32) for 32..47.  D270 recorded the two as an unexplained
 * inconsistency; they are the two directions.
 *
 * Nine of the class's thirteen methods are written.  `packData` joined them:
 * it calls nothing at all -- `resetCrc` is inlined into it in the object -- so
 * defining it costs the link nothing, which is the test docs/v90cpp.md sets.
 * The four that SET the message's fields (`setRatesMask`, `resetCrc`,
 * `setMaxLookahead`, `setConstelSize`) are still declared for the record and
 * deliberately left undefined, because defining a method whose callees are not
 * yet written breaks the link for the whole test suite.  Nothing calls the
 * undefined ones, and NOTHING CALLS `packData` EITHER: no relocation in the
 * object names it, and none is added here.  V90Jd.cpp says why that is not an
 * accident.
 */

#ifndef DSPLIB_V90JD_H
#define DSPLIB_V90JD_H

/*
 * Declared, not defined: the constructor takes one only to read five fields
 * out of it, and `V90Parameters.h` is a 342-slot header no user of this class
 * should be made to parse.  `V90Jd.cpp` includes it.
 */
class V90Parameters;

/* The 72 bit positions, by the 17-byte stride the accessors step in. */
#define V90JD_BITS	72
#define V90JD_GROUP	17
#define V90JD_GROUP1	(1 * V90JD_GROUP)	/* 17 */
#define V90JD_GROUP2	(2 * V90JD_GROUP)	/* 34 */
#define V90JD_GROUP3	(3 * V90JD_GROUP)	/* 51 */

class V90Jd {
public:
	/*
	 * Fill the message in from the parameter block: the rate mask, the
	 * constellation size and the maximum lookahead, plus the unpacker
	 * cleared.  See the bit map above -- the constructor is where it comes
	 * from.
	 *
	 * THE CLASS IS NO LONGER TRIVIAL, and that is a deliberate cost.  The
	 * blob has `_ZN5V90JdD1Ev` and `_ZN5V90JdD2Ev` as one-byte `ret`s, and
	 * GCC emits an out-of-line destructor symbol only for a user-declared
	 * one -- a trivial implicit destructor produces no symbol at all.  So
	 * the original declared this destructor, and a reconstruction that
	 * leaves it out leaves two symbols undefined for every caller that
	 * destroys a V90Jd.  The test fixture pays for it by holding the object
	 * in a byte array rather than in a union; see t_v90jd.cpp.
	 */
	V90Jd(V90Parameters *params);
	~V90Jd();

	/*
	 * Pack the CRC into the vector and hand it back.  Returns `this + 2`,
	 * which is `bits`.
	 */
	unsigned char *getBitVector();

	/* Clear the unpacker's state.  The constructor clears the same three. */
	void unPackReset();

	/*
	 * Feed the receiver one bit and say whether that completed a message.
	 * DEFINED, and it is the one thing in the class that WRITES the
	 * payload-contiguous layout the three accessors below read: the
	 * framing is stripped and `bits[0..47]` filled in arrival order, so
	 * the class's two layouts are its two directions rather than a
	 * contradiction.  V90Jd.cpp has the state machine and the offsets.
	 *
	 * `int` is measured, not mangled: the completion path at 0x1ec18
	 * jumps past the `xor %edx,%edx` every other path runs through, and
	 * `%eax` is set from `%edx` at the return.
	 */
	int unPackData(int);

	/*
	 * DEFINED, and callerless in the object by construction: it is the
	 * whole packer, byte for byte the body `getBitVector` carries inlined,
	 * and it writes the TRANSMIT layout mapped above.  It leaves the three
	 * unpacker fields alone.  Its `void` return is measured -- the object
	 * sets %eax to nothing on the way out, where `getBitVector` ends
	 * `lea 0x2(%edi),%eax`.
	 */
	void packData();

	/*
	 * Declared, not defined -- see the file comment.  Their signatures are
	 * the mangling's, so this list is a specification rather than a guess;
	 * a return type is not mangled and is therefore unknown for all of them.
	 */
	void setRatesMask(int);
	void resetCrc();
	void setMaxLookahead(unsigned char);
	void setConstelSize(unsigned char, unsigned char);

	/*
	 * DEFINED, and they read a DIFFERENT layout from the one the
	 * constructor and `getBitVector` write: `bits[0..27]` for the rate
	 * mask, `bits[28..29]` for the constellation size and `bits[30..31]`
	 * for the lookahead, with none of the framing.  D270; V90Jd.cpp has the
	 * offsets and the argument.  The return types are read off the object,
	 * since the mangling does not carry one.
	 */
	int getRatesMask();
	void getConstelationSize(unsigned char *, unsigned char *);
	unsigned char getMaxLookahead();

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling (tools/cppstruct.py says so), and
	 * because a single access section is what keeps the class POD and
	 * __builtin_offsetof well defined -- the .cpp asserts every offset
	 * below against what the compiler lays out.
	 */

	/*
	 * The unpacker's state, and `unPackData` says what each byte holds.
	 * `unpack[0]` is the length of the current run of 1 bits, kept as a
	 * byte and wrapping at 255; `unpack[1]` counts the payload bytes
	 * stored so far, and then the four trailing bits; `unpackWord` is the
	 * state, 0 to 8.  `unPackReset()` and the constructor clear all three.
	 */
	unsigned char unpack[2];	/* +0x00 */

	unsigned char bits[V90JD_BITS];	/* +0x02 the vector, one byte per bit */

	unsigned char pad_4a[2];	/* +0x4a alignment ahead of crc         */

	/*
	 * The CRC register, one int per bit, low bit first.  `resetCrc()` is
	 * the method that names it, and +0x4c is the offset it writes.
	 */
	int crc[16];			/* +0x4c */

	int unpackWord;			/* +0x8c the unpacker's third field   */
};

#endif /* DSPLIB_V90JD_H */
