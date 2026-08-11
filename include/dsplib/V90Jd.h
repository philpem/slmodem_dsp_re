/*
 * V90Jd.h -- the V.90 Jd message, one byte per bit.
 *
 * Reconstructed from dsplibs.o V90Jd.cpp.  `V90Jd` is not polymorphic --
 * tools/cppstruct.py lists its destructor with the two ordinary variants and
 * not the deleting one, and GCC emits a deleting destructor only for a
 * virtual one -- so offset 0 is a real member and there is no vptr.  Finding
 * 228 is the four classes where that is not true.
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
 * Only `getBitVector` and `unPackReset` are written here; the rest of the
 * class is declared for the record and deliberately left undefined, because
 * defining a method whose callees are not yet written breaks the link for the
 * whole test suite (docs/v90cpp.md).  Nothing calls the undefined ones.
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
	 * Declared, not defined -- see the file comment.  Their signatures are
	 * the mangling's, so this list is a specification rather than a guess;
	 * a return type is not mangled and is therefore unknown for all of them.
	 */
	void unPackData(int);
	void packData();
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
	 * The unpacker's state.  `unPackReset()`, the constructor and
	 * `unPackData(int)` are the only things that touch these three, and
	 * what each holds is not settled here: `unPackData` is not in this
	 * batch.  Named for the one method whose whole body is clearing them.
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
