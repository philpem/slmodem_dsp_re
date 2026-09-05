/**
 * @file V92Jd.h
 * @brief The V.92 Jd message: `V90Jd`'s object with a second bit vector.
 *
 * Not polymorphic, for the same reason `V90Jd` is not: `tools/cppstruct.py`
 * lists the destructor's two ordinary variants and not the deleting one,
 * which GCC emits only for a virtual destructor. Offset 0 is a real member.
 *
 * The object is `V90Jd` with one insertion: a second 72-byte vector at
 * +0x4a, after which everything shifts by exactly 0x48 (the CRC register
 * moves from +0x4c to +0x94, the two-byte alignment pad from +0x4a to
 * +0x92). The two vectors share identical geometry -- 17-byte stride, four
 * groups, four trailing bits -- so `V90Jd.h`'s map reads across unchanged.
 *
 * The shared displacement does not carry two things, and both matter:
 *
 *   - There is one CRC register, not two. `packJdData` and
 *     `packJdPhaseData` both use +0x94, so a phase pack destroys whatever
 *     the data pack left there. Nothing reads it between packs -- the
 *     register is scratch that happens to live in the object.
 *
 *   - The bits each pack forces to zero are not parallel. `packJdData`
 *     clears group 2's positions 7..12 and 14; `packJdPhaseData` clears
 *     group 2's positions 1..12 and 16. So the phase message keeps only
 *     what `V92Jd::V92Jd` and `setConstelSize` write at +0x79..+0x7b, and
 *     the data message keeps the first six of its second rate-mask run.
 *
 * The unpacker's two bytes at +0x00 are shared, but each direction gets its
 * own word: `unPackJdReset` clears +0xd4, `unPackJdPhaseReset` clears +0xd8.
 * `unpack[0]` is the run of 1 bits, `unpack[1]` the payload count; the two
 * unpackers cannot run at once, which is what sharing them costs.
 *
 * The unpackers fill a different map from the one the packs write. Each
 * strips the framing and leaves its vector flat -- index 0..27 the rate
 * mask, 28..31 the constellation size and lookahead, 32..47 the CRC as
 * received -- which is the layout all four accessors read. Framed position
 * 18+p is payload p for p in 0..15, 35+(p-16) for 16..31, and 52+(p-32) for
 * 32..47. Payload 28 is framed 47, the constant byte each constructor
 * writes, and each unpacker refuses a message whose copy of it is not its
 * own.
 *
 * Fourteen of the class's nineteen methods are defined; the five setters
 * are declared for the record and left undefined, because a defined method
 * whose callees are not yet written breaks the link for the whole test
 * suite (docs/v90cpp.md). Nothing calls the undefined ones.
 */

#ifndef DSPLIB_V92JD_H
#define DSPLIB_V92JD_H

#include "dsplib/V90Jd.h"		/* the shared V90JD_GROUP* geometry */

class V92Jd {
public:
	/**
	 * @brief Construct, filling both messages in from the parameter
	 *        block.
	 * @param params  A `V90Parameters *` despite the class being V92Jd
	 *                (the mangling, `_ZN5V92JdC1EP13V90Parameters`, is
	 *                explicit about it): the three fields read here
	 *                (`V92_DIGITAL_RATE_MASK`,
	 *                `V92_MAX_SPECTRAL_SHAPER_LOOKAHEAD`, `V92_JD_PHASE`)
	 *                live in `V90Parameters` at +0x3c, +0x40 and +0x44.
	 *                The V.92 parameters are split across both blocks;
	 *                this class reads only the V.90 one.
	 */
	V92Jd(V90Parameters *params);
	/** @brief Destroy. One-byte `ret`, as `V90Jd`'s is. */
	~V92Jd();

	/** @brief Fill in the data message and its CRC. */
	void packJdData();
	/** @brief Fill in the phase message and its CRC. */
	void packJdPhaseData();

	/** @brief Pack the data message and hand back its bit vector.
	 *  @return Pointer to `bits` (`this + 2`). */
	unsigned char *getJdBitVector();
	/** @brief Pack the phase message and hand back its bit vector.
	 *  @return Pointer to `phaseBits` (`this + 0x4a`). */
	unsigned char *getJdPhaseBitVector();

	/** @brief Clear the data-direction unpacker (shared `unpack[]` plus
	 *  `unpackWord`). */
	void unPackJdReset();
	/** @brief Clear the phase-direction unpacker (shared `unpack[]`
	 *  plus `unpackPhaseWord`). */
	void unPackJdPhaseReset();

	/**
	 * @brief Feed one received bit to the data-direction unpacker.
	 *        Together with unPackJdPhaseData(), writes the
	 *        payload-contiguous layout the four getters below read --
	 *        `bits[0..47]`, framing stripped, in arrival order.
	 * @param bit  The received bit.
	 * @return Nonzero once a complete message has been received and its
	 *         tag byte (framed position 47, `bits[28] == 0`) checks out.
	 */
	int unPackJdData(int bit);
	/**
	 * @brief Feed one received bit to the phase-direction unpacker.
	 *        Same shape as unPackJdData(), differing in its second
	 *        rate-mask run length (12 in one state rather than 11+1 in
	 *        two) and its tag check (`phaseBits[28] == 1`).
	 * @param bit  The received bit.
	 * @return Nonzero once a complete message has been received and its
	 *         tag byte checks out.
	 */
	int unPackJdPhaseData(int bit);

	/*
	 * Declared, not defined -- see the file comment.  The signatures are
	 * the mangling's; a return type is not mangled, so none is known.
	 */
	/** @brief Declared, not defined -- see the file comment. */
	void setJdPhase(float);
	/** @brief Declared, not defined -- see the file comment. */
	void setRatesMask(int);
	/** @brief Declared, not defined -- see the file comment. */
	void resetCrc();
	/** @brief Declared, not defined -- see the file comment. */
	void setMaxLookahead(unsigned char);
	/** @brief Declared, not defined -- see the file comment. */
	void setConstelSize(unsigned char, unsigned char);

	/** @brief Read back the phase message's phase value.
	 *  @return `phaseBits[0..15]` as Q16, byte for byte identical to
	 *  `V90Jd`'s getter. */
	float getJdPhase();
	/** @brief Read back the data message's rate mask.
	 *  @return `bits[0..27]`, byte for byte identical to `V90Jd`'s
	 *  getter. */
	int getRatesMask();
	/**
	 * @brief Read back the constellation size fields. Unlike `V90Jd`,
	 *        which reads `bits[28..29]`, this reads `phaseBits[29..30]`
	 *        (deviation D271).
	 * @param[out] p1  First constellation-size byte.
	 * @param[out] p2  Second constellation-size byte.
	 */
	void getConstelationSize(unsigned char *p1, unsigned char *p2);
	/** @brief Read back the maximum spectral-shaper lookahead.
	 *  @return `bits[30..31]`, byte for byte identical to `V90Jd`'s
	 *  getter (deviation D270 describes the unframed layout both read). */
	unsigned char getMaxLookahead();

	/* Public, and one access section, for the reasons V90Jd.h gives. */

	unsigned char unpack[2];		/* +0x00 shared unpack state  */
	unsigned char bits[V90JD_BITS];		/* +0x02 the data message     */
	unsigned char phaseBits[V90JD_BITS];	/* +0x4a the phase message    */
	/*
	 * +0x92 was `pad_92[2]` -- REMOVED (finding F10151).  Already
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
