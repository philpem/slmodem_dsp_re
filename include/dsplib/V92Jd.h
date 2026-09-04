/*
 * V92Jd.h -- the V.92 Jd message: V90Jd's object with a second bit vector.
 *
 * Reconstructed from dsplibs.o V92Jd.cpp.  Not polymorphic, for the same
 * reason V90Jd is not: tools/cppstruct.py lists the destructor's two ordinary
 * variants and not the deleting one, which GCC emits only for a virtual
 * destructor.  Offset 0 is a real member.
 *
 * THE OBJECT IS V90Jd WITH ONE INSERTION.  A second 72-byte vector goes in at
 * +0x4a, and everything after it moves by exactly 0x48: the CRC register goes
 * from +0x4c to +0x94 and the two-byte alignment pad from +0x4a to +0x92.  The
 * two vectors have identical geometry -- the 17-byte stride, four groups, four
 * trailing bits -- so V90Jd.h's map reads across unchanged.
 *
 * The displacement does NOT carry two things, and both matter:
 *
 *   - There is one CRC register, not two.  `packJdData` and
 *     `packJdPhaseData` both use +0x94, so a phase pack destroys whatever the
 *     data pack left there.  Nothing reads it between packs; the register is
 *     scratch that happens to live in the object.
 *
 *   - The bits each pack forces to zero are NOT parallel.  `packJdData`
 *     clears group 2's positions 7..12 and 14; `packJdPhaseData` clears
 *     group 2's positions 1..12 and 16.  So the phase message keeps only what
 *     `V92Jd::V92Jd` and `setConstelSize` write at +0x79..+0x7b, and the data
 *     message keeps the first six of its second rate-mask run.
 *
 * The unpacker's two bytes at +0x00 are shared, but it has a word per
 * direction: `unPackJdReset` clears +0xd4 and `unPackJdPhaseReset` +0xd8, and
 * both clear the same two bytes.  `unpack[0]` is the run of 1 bits and
 * `unpack[1]` the payload count; the two unpackers cannot run at once, which
 * is what sharing them costs.
 *
 * AND THE UNPACKERS FILL A DIFFERENT MAP FROM THE ONE THE PACKS WRITE.  Each
 * strips the framing and leaves its vector flat -- index 0..27 the rate mask,
 * 28..31 the constellation size and lookahead, 32..47 the CRC as received --
 * which is the layout all four accessors read.  Framed position 18+p is
 * payload p for p in 0..15, 35+(p-16) for 16..31 and 52+(p-32) for 32..47.
 * Payload 28 is framed 47, the constant byte each constructor writes, and
 * each unpacker refuses a message whose copy of it is not its own.
 *
 * Fourteen of the class's nineteen methods are defined; the five setters are
 * declared
 * for the record and left undefined, because a defined method whose callees
 * are not yet written breaks the link for the whole test suite
 * (docs/v90cpp.md).  Nothing calls the undefined ones.
 */

#ifndef DSPLIB_V92JD_H
#define DSPLIB_V92JD_H

#include "dsplib/V90Jd.h"		/* the shared V90JD_GROUP* geometry */

class V92Jd {
public:
	/*
	 * Fill both messages in from the parameter block -- and the argument
	 * really is a `V90Parameters *`, not a `V92Parameters *`: the mangling
	 * is `_ZN5V92JdC1EP13V90Parameters`, and the three fields it reads
	 * (`V92_DIGITAL_RATE_MASK`, `V92_MAX_SPECTRAL_SHAPER_LOOKAHEAD`,
	 * `V92_JD_PHASE`) all live in V90Parameters at +0x3c, +0x40 and +0x44.
	 * The V.92 parameters are split across both blocks; this class reads
	 * only the V.90 one.
	 *
	 * The destructor is a one-byte `ret` and exists for the reason
	 * V90Jd.h gives for its own.
	 */
	V92Jd(V90Parameters *params);
	~V92Jd();

	/* Fill in the message and its CRC.  No return value is used. */
	void packJdData();
	void packJdPhaseData();

	/* pack, then hand back the vector: `this + 2` and `this + 0x4a`. */
	unsigned char *getJdBitVector();
	unsigned char *getJdPhaseBitVector();

	/* Clear the unpacker.  Two bytes shared, one word each. */
	void unPackJdReset();
	void unPackJdPhaseReset();

	/*
	 * Feed one received bit to one direction's receiver and say whether
	 * that completed a message.  DEFINED, and between them they are what
	 * WRITES the payload-contiguous layout the four accessors read --
	 * `bits[0..47]` and `phaseBits[0..47]`, framing stripped, in arrival
	 * order.  `int` is measured from `%eax`, not mangled.
	 *
	 * The two are V90Jd::unPackData with three differences: the data one
	 * takes its second rate run as 11 + 1 in two states where V.90 and
	 * the phase one take 12 in one, and each refuses a completed message
	 * unless its own constant tag byte is right -- `bits[28] == 0` for
	 * the data message and `phaseBits[28] == 1` for the phase one, which
	 * is framed position 47 in each, the byte the constructor writes as a
	 * constant.  V92Jd.cpp cites the instruction for each.
	 */
	int unPackJdData(int);
	int unPackJdPhaseData(int);

	/*
	 * Declared, not defined -- see the file comment.  The signatures are
	 * the mangling's; a return type is not mangled, so none is known.
	 */
	void setJdPhase(float);
	void setRatesMask(int);
	void resetCrc();
	void setMaxLookahead(unsigned char);
	void setConstelSize(unsigned char, unsigned char);

	/*
	 * DEFINED.  Three of the four are V90Jd's byte for byte and read the
	 * unframed layout D270 describes -- `bits[0..27]`, `bits[30..31]` --
	 * and the phase comes out of `phaseBits[0..15]` as Q16.  The fourth,
	 * `getConstelationSize`, reads `phaseBits[29..30]` where V90Jd's reads
	 * `bits[28..29]`: D271.  Return types are measured, not mangled.
	 */
	float getJdPhase();
	int getRatesMask();
	void getConstelationSize(unsigned char *, unsigned char *);
	unsigned char getMaxLookahead();

	/* Public, and one access section, for the reasons V90Jd.h gives. */

	unsigned char unpack[2];		/* +0x00 shared unpack state  */
	unsigned char bits[V90JD_BITS];		/* +0x02 the data message     */
	unsigned char phaseBits[V90JD_BITS];	/* +0x4a the phase message    */
	/*
	 * +0x92 was `pad_92[2]` -- REMOVED (finding F10145).  Already
	 * correctly described as alignment ahead of `crc`; proved
	 * mechanically by the existing `V92JD_OFF(phaseBits, 0x4a, ...)`/
	 * `V92JD_OFF(crc, 0x94, ...)` and by `dis.py` over all twenty-one
	 * `V92Jd` methods finding no access to offset 0x92/0x93.
	 */
	int crc[16];				/* +0x94 shared by both packs */
	int unpackWord;				/* +0xd4                      */
	int unpackPhaseWord;			/* +0xd8                      */
};

#endif /* DSPLIB_V92JD_H */
