/**
 * @file V90Jd.h
 * @brief `V90Jd`, the V.90 Jd message: a 72-bit vector, one byte per bit.
 *
 * Reconstructed from dsplibs.o V90Jd.cpp. `V90Jd` is not polymorphic --
 * `tools/cppstruct.py` lists its destructor with the two ordinary variants
 * and not the deleting one, and GCC emits a deleting destructor only for a
 * virtual one -- so offset 0 is a real member and there is no vptr. Finding
 * F228 is the four classes where that is not true.
 *
 * The object is mostly a bit vector. `getBitVector()` returns `this + 2`,
 * and everything from there to +0x49 is 72 bytes each holding 0 or 1. Its
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
 * block's +0x2c word bit by bit into +0x14..+0x23 and +0x25..+0x30, its
 * +0x34 and +0x38 into +0x31 and +0x32, and the two low bits of its +0x30
 * into +0x33 and +0x34; `setRatesMask` writes +0x14 and +0x25,
 * `setConstelSize` +0x31 and +0x32, `setMaxLookahead` +0x33 and +0x34. The
 * names are the author's own, from the method names the mangling preserves.
 *
 * That map is the transmit side only. `unPackData` fills a second, flat one
 * in the same bytes -- `bits[0..27]` the rate mask, `bits[28..29]` the
 * constellation size, `bits[30..31]` the lookahead and `bits[32..47]` the
 * CRC as received, with no group markers anywhere -- and the three
 * accessors read that one. Framed position 18+p is payload p for p in
 * 0..15, 35+(p-16) for 16..31 and 52+(p-32) for 32..47. D270 recorded the
 * two as an unexplained inconsistency; they are the two directions.
 *
 * Nine of the class's thirteen methods are written. `packData` joined
 * them: it calls nothing at all -- `resetCrc` is inlined into it in the
 * object -- so defining it costs the link nothing, which is the test
 * docs/v90cpp.md sets. The four that SET the message's fields
 * (`setRatesMask`, `resetCrc`, `setMaxLookahead`, `setConstelSize`) are
 * still declared for the record and deliberately left undefined, because
 * defining a method whose callees are not yet written breaks the link for
 * the whole test suite. Nothing calls the undefined ones, and nothing
 * calls `packData` either: no relocation in the object names it, and none
 * is added here. `V90Jd.cpp` says why that is not an accident.
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
	/**
	 * @brief Build a Jd message from a V.90 parameter block.
	 *
	 * Fills in the framed bit vector -- the rate mask, the constellation
	 * size and the maximum lookahead, per the bit map in the file
	 * comment -- and clears the unpacker's three state fields.
	 *
	 * The class is no longer trivial, and that is a deliberate cost. The
	 * blob has `_ZN5V90JdD1Ev` and `_ZN5V90JdD2Ev` as one-byte `ret`s,
	 * and GCC emits an out-of-line destructor symbol only for a
	 * user-declared one -- a trivial implicit destructor produces no
	 * symbol at all. So the original declared this destructor, and a
	 * reconstruction that leaves it out leaves two symbols undefined
	 * for every caller that destroys a `V90Jd`. The test fixture pays
	 * for it by holding the object in a byte array rather than in a
	 * union; see `t_v90jd.cpp`.
	 *
	 * @param params  The V.90 parameter block to read
	 *                `DIGITAL_RATE_MASK`, `MAX_SPECTRAL_SHAPER_LOOKAHEAD`,
	 *                `V34_PHASE4_CONSTELLATION` and `V34_RRN_CONSTELLATION`
	 *                from.
	 */
	V90Jd(V90Parameters *params);

	/** @brief Trivial destructor, declared only so the object's symbols exist. */
	~V90Jd();

	/**
	 * @brief Assemble the CRC and hand back the wire-ready bit vector.
	 * @return `this + 2`, i.e. `bits`.
	 */
	unsigned char *getBitVector();

	/** @brief Clear the unpacker's state. The constructor clears the same three fields. */
	void unPackReset();

	/**
	 * @brief Feed the receiver one bit of an incoming Jd message.
	 *
	 * Runs the framing state machine: strips the wire framing (the
	 * seventeen-1s preamble and the three group-leading 0 bits) and
	 * fills `bits[0..47]` in arrival order, so this is the one method
	 * in the class that writes the payload-contiguous layout the three
	 * accessors below read -- the class's two layouts are its two
	 * directions, not a contradiction. `V90Jd.cpp` has the full state
	 * machine and the byte offsets.
	 *
	 * @param bit  The next received bit (0 or 1).
	 * @return 1 once a complete, CRC-checked message has been unpacked; 0 otherwise.
	 */
	int unPackData(int bit);

	/**
	 * @brief Assemble the transmit-side framed bit vector, CRC included.
	 *
	 * Writes the same layout `getBitVector()` returns, byte for byte
	 * identical to the body inlined into `getBitVector()` in the
	 * object -- calling it from here is measured, not assumed
	 * (finding F7944). It touches only the framed message fields and
	 * leaves the three unpacker fields untouched.
	 *
	 * Present in the object but reached by no relocation of any kind:
	 * nothing in the 1.2 MB blob calls it directly, only `getBitVector()`
	 * (into which the compiler inlined a copy of its body). It is
	 * reproduced here because the vendor shipped it, not because
	 * anything but `getBitVector()` reaches it.
	 */
	void packData();

	/*
	 * Declared, not defined -- see the file comment.  Their signatures are
	 * the mangling's, so this list is a specification rather than a guess;
	 * a return type is not mangled and is therefore unknown for all of them.
	 */

	/** @brief Set the 28-bit digital rate mask into the framed message. Declared only; see the file comment. @param mask The rate mask, bits 0..27. */
	void setRatesMask(int mask);
	/** @brief Reset the CRC register to all ones. Declared only; see the file comment. */
	void resetCrc();
	/** @brief Set the maximum spectral-shaper lookahead field. Declared only; see the file comment. @param v The two-bit lookahead value. */
	void setMaxLookahead(unsigned char v);
	/** @brief Set the two constellation-size fields. Declared only; see the file comment. @param first The phase-4 constellation size. @param second The RRN constellation size. */
	void setConstelSize(unsigned char first, unsigned char second);

	/*
	 * Defined, and they read a different layout from the one the constructor
	 * and `getBitVector` write: the unframed, payload-contiguous layout
	 * `unPackData` produces (D270; `V90Jd.cpp` has the offsets and the
	 * argument).  The return types are read off the object, since the
	 * mangling does not carry one.
	 */

	/**
	 * @brief Read back the 28-bit digital rate mask from an unpacked message.
	 * @return The rate mask, bits 0..27, as received.
	 */
	int getRatesMask();
	/**
	 * @brief Read back the two constellation-size fields from an unpacked message.
	 * @param first   Receives the phase-4 constellation size bit.
	 * @param second  Receives the RRN constellation size bit.
	 */
	void getConstelationSize(unsigned char *first, unsigned char *second);
	/**
	 * @brief Read back the maximum spectral-shaper lookahead from an unpacked message.
	 * @return The two-bit lookahead value.
	 */
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
