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
 * both clear the same two bytes.
 *
 * Only the six methods this batch closes are defined; the rest are declared
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
	 * Declared, not defined -- see the file comment.  The signatures are
	 * the mangling's; a return type is not mangled, so none is known.
	 */
	void unPackJdData(int);
	void unPackJdPhaseData(int);
	void setJdPhase(float);
	void getJdPhase();
	void getRatesMask();
	void setRatesMask(int);
	void resetCrc();
	void getMaxLookahead();
	void setMaxLookahead(unsigned char);
	void getConstelationSize(unsigned char *, unsigned char *);
	void setConstelSize(unsigned char, unsigned char);

	/* Public, and one access section, for the reasons V90Jd.h gives. */

	unsigned char unpack[2];		/* +0x00 shared unpack state  */
	unsigned char bits[V90JD_BITS];		/* +0x02 the data message     */
	unsigned char phaseBits[V90JD_BITS];	/* +0x4a the phase message    */
	unsigned char pad_92[2];		/* +0x92 ahead of crc         */
	int crc[16];				/* +0x94 shared by both packs */
	int unpackWord;				/* +0xd4                      */
	int unpackPhaseWord;			/* +0xd8                      */
};

#endif /* DSPLIB_V92JD_H */
