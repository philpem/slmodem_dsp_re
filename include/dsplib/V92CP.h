/*
 * V92CP.h -- the V.92 CP message, one byte per bit.
 *
 * Reconstructed from dsplibs.o V92CP.cpp.  `V92CP` is not polymorphic --
 * tools/cppstruct.py lists its destructor with the two ordinary variants and
 * not the deleting one, and GCC emits a deleting destructor only for a
 * virtual one -- so offset 0 is a real member and there is no vptr.
 *
 * THE SIZE IS THE ALLOCATION, not an inference from the highest offset.
 * `V92Modem::V92Modem` allocates the object and hands the block straight to
 * the constructor:
 *
 *     13dd3:  movl $0x918,(%esp)
 *     13dda:  call sysdep_malloc
 *     13de4:  call _ZN5V92CPC1Ev
 *
 * 0x918 = 2328, and the last field the class touches is the four-byte
 * +0x914, which ends exactly there.
 *
 * WHICH MEMBER PROVED WHICH OFFSET.  Only the constructor and the destructor
 * are defined here; the rest of the map was read out of members that are
 * declared and deliberately left undefined:
 *
 *     +0x004                  the constructor, and nothing else
 *     +0x104,+0x108           `setSUV` (0x4e920), whose whole body is
 *                             "+0x104 = 16; +0x108 = the argument"
 *     +0x114,+0x119,+0x11a,   `resetDetector` (0x4e830), whose whole body is
 *     +0x11c,+0x120           these five stores
 *     +0x129                  `getBitVector` (0x4ebe0) returns `this+0x129`
 *     +0x8f9                  `resetCRC` (0x4e5d0) writes 1 to sixteen bytes
 *                             from here
 *     +0x90c                  `getBitVector` reports it as the length
 *     +0x914                  `reset` (0x4e860) and the constructor set -1
 *
 * The destructor is one byte -- a bare `ret`.  That is not an assumption
 * about an empty class: nothing here is allocated, and the V.90 sibling with
 * the same shape (V90CP) is 173 bytes of frees for exactly the six buffers
 * its constructor allocates.
 *
 * `V92CP::bitsToInfo(unsigned char)::gamma` and `::delta` are function-local
 * statics in .bss, so `bitsToInfo` carries state across calls.  Nothing here
 * depends on that.
 */

#ifndef DSPLIB_V92CP_H
#define DSPLIB_V92CP_H

/*
 * The bit vector's extent.  START proven -- `getBitVector` hands back
 * `this+0x129` -- and END is where the CRC register begins.  That the whole
 * span is ONE array is a modelling choice: no method establishes the array's
 * own length.  Note the odd start, which is the object's own and not a
 * miscount: +0x129 is what the accessor adds.
 *
 * What IS measured is that nothing else lives in the span at an offset of its
 * own: over all twelve of the class's symbols, every `this`-relative
 * displacement between +0x129 and +0x8f9 lies below +0x1aa, and there is not
 * one at any higher offset until the CRC.
 */
#define V92CP_BITS	0x7d0		/* 0x129 .. 0x8f8, 2000 bytes */

/* The CRC register: sixteen bytes, one per bit, `resetCRC` sets them all. */
#define V92CP_CRC	16

class V92CP {
public:
	/*
	 * Clear the detector.  The only two members defined in
	 * src/pump/v90/V92CP.cpp.
	 */
	V92CP();
	~V92CP();

	/*
	 * Declared, not defined -- see V90CP.h.  Argument types are the
	 * mangling's and exact; return types are not mangled, so `void` means
	 * "not established" for all but `getBitVector`, which leaves
	 * `this+0x129` in %eax.
	 */
	unsigned char *getBitVector(unsigned int &length);
	void reset();
	void resetDetector();
	void resetCRC();
	void calcCRC();
	void evaluateCRC();
	void evaluateInfo();
	void infoToBits();
	void bitsToInfo(unsigned char);
	void setSUV(unsigned int);

	/* Public for the same reason as V90Jd's and V90CP's: it keeps the
	 * class standard-layout, so the offsetof assertions are well defined. */

	unsigned char pad_00[4];	/* +0x000 not modelled            */

	/*
	 * +0x004  Cleared by the constructor and by NOTHING else -- `reset`
	 * does not touch it, which is the whole difference between the two.
	 */
	unsigned char byte_04;

	unsigned char pad_05[0xff];	/* +0x005 not modelled            */

	/* +0x104  `setSUV` stores 16 here before storing its argument. */
	unsigned int word_104;

	/* +0x108  `setSUV`'s argument. */
	unsigned int suv;

	unsigned char pad_10c[8];	/* +0x10c read by methods not written */

	/* +0x114  Zeroed by `resetDetector`, and so by `reset` and the ctor. */
	unsigned int word_114;

	unsigned char pad_118[1];	/* +0x118 not modelled            */

	/* +0x119  Cleared by `resetDetector`.  One byte, stored as a byte. */
	unsigned char byte_119;

	/* +0x11a  Cleared by `resetDetector` alongside +0x119. */
	unsigned char byte_11a;

	unsigned char pad_11b[1];	/* +0x11b alignment               */

	/* +0x11c  Set to 18 by `resetDetector`. */
	int word_11c;

	/* +0x120  Zeroed by `resetDetector`. */
	int word_120;

	unsigned char pad_124[5];	/* +0x124 not modelled            */

	/* +0x129  The bit vector, one byte per bit.  See V92CP_BITS. */
	unsigned char bits[V92CP_BITS];

	/* +0x8f9  The CRC register, one byte per bit; `resetCRC` sets all. */
	unsigned char crc[V92CP_CRC];

	unsigned char pad_909[3];	/* +0x909 alignment               */

	/* +0x90c  The sequence length `getBitVector` reports through its
	 * reference argument. */
	unsigned int word_90c;

	unsigned char pad_910[4];	/* +0x910 not modelled            */

	/* +0x914  Set to -1 by `reset` and by the constructor. */
	int word_914;
};

#endif /* DSPLIB_V92CP_H */
