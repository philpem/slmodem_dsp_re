/*
 * V90CP.h -- the V.90 CP message, one byte per bit, plus six heap buffers.
 *
 * Reconstructed from dsplibs.o V90CP.cpp (docs/attribution.md lists fifteen
 * symbols for that translation unit).  `V90CP` is not polymorphic --
 * tools/cppstruct.py lists its destructor with the two ordinary variants and
 * not the deleting one, and GCC emits a deleting destructor only for a
 * virtual one -- so offset 0 is a real member and there is no vptr.
 *
 * THE SIZE IS PINNED FROM BOTH ENDS, which is why it is asserted rather than
 * padded to a guess.  `V90Modem::V90Modem` builds the object in place:
 *
 *     194ef:  lea 0xcd0(%esi),%ebp        the V90MP member
 *     194f5:  lea 0xdf4(%esi),%edi        this object
 *     194fe:  call _ZN5V90MPC1Ev
 *     19506:  call _ZN5V90CPC1Ev
 *     ...
 *     19622:  mov 0x49b4(%esi),%edx       V90Parameters *, the next member
 *
 * so the object occupies V90Modem+0xdf4 up to +0x49b4 -- 0x3bc0 bytes -- and
 * the last field the class itself touches is the four-byte +0x3bbc, which
 * ends exactly there.  Lower bound meets upper bound.
 *
 * WHICH MEMBER PROVED WHICH OFFSET.  Only the constructor and the destructor
 * are defined here; every other offset below was read out of a member that
 * is declared and deliberately left undefined:
 *
 *     +0x013                  the constructor, and nothing else
 *     +0xc88..+0xc9c          the constructor's six sysdep_malloc(0x200)
 *                             calls and the destructor's six sysdep_free's
 *     +0xca4,+0xca9,+0xcaa,   `resetDetector` (0x51510), whose whole body is
 *     +0xcac,+0xcb0           these five stores
 *     +0xcb8                  `getBitVector` (0x519d0) returns `this+0xcb8`
 *     +0x3b98                 `resetCRC` (0x512b0) writes 1 to sixteen bytes
 *                             from here
 *     +0x3ba8,+0x3bac,+0x3bb0 `calcSequenceLength` (0x521c0) rounds +0x3bb0+1
 *                             up to a multiple of +0x3ba8 into +0x3bac
 *     +0x3bb4,+0x3bb8         `printNofRecievedMpMpNot` (0x53680) prints them
 *                             as "received %d MP, %d MPNot"
 *     +0x3bbc                 `reset` (0x51540) and the constructor set -1
 *
 * The regions between are `pad_*`: that part is not modelled as fields yet,
 * and an offset landing in one is itself the answer.
 *
 * Two of the class's function-local statics are in .bss under their own
 * mangled names -- `V90CP::bitsToInfo(unsigned char)::alpha` and `::beta` --
 * so `bitsToInfo` carries state across calls.  Nothing here depends on that.
 */

#ifndef DSPLIB_V90CP_H
#define DSPLIB_V90CP_H

/*
 * The bit vector's extent.  Its START is proven -- `getBitVector` hands back
 * `this+0xcb8` -- and its END is where the CRC register begins.  That the
 * whole span is ONE array is the modelling choice, not a measurement: no
 * method establishes the array's own length, and the length `getBitVector`
 * reports is +0x3bac, which `calcSequenceLength` computes at run time.
 *
 * What IS measured is that nothing else lives in the span at an offset of its
 * own.  Over all fifteen of the class's symbols, the only `this`-relative
 * displacements between +0xcb8 and +0x3b98 are +0xcb8 itself and +0xcc9..
 * +0xcdb -- constant indices into the first two dozen bytes -- and there is
 * not one at any higher offset until the CRC.  Whatever is in there is
 * reached through this base and no other.
 */
#define V90CP_BITS	0x2ee0		/* 0xcb8 .. 0x3b97, 12000 bytes */

/* The CRC register: sixteen bytes, one per bit, `resetCRC` sets them all. */
#define V90CP_CRC	16

/* Each of the six buffers the constructor allocates. */
#define V90CP_BUFSIZE	0x200

/* And how many there are: six calls, six stores, six frees. */
#define V90CP_BUFS	6

class V90CP {
public:
	/*
	 * Allocate the six buffers and clear the detector.  The only two
	 * members defined in src/pump/v90/V90CP.cpp.
	 */
	V90CP();
	~V90CP();

	/*
	 * Declared, not defined -- defining a method whose callees are not
	 * written breaks the link for the whole suite (docs/v90cpp.md), and a
	 * declaration is a specification where a definition is a claim.  The
	 * argument types are the mangling's and are exact; the return types
	 * are not mangled, so `void` here means "not established" for all but
	 * `getBitVector`, which leaves `this+0xcb8` in %eax.
	 */
	unsigned char *getBitVector(unsigned int &length);
	void reset();
	void resetDetector();
	void resetCRC();
	void calcCRC();
	void evaluateCRC();
	void evaluateInfo();
	void calcSequenceLength();
	void infoToBits();
	void bitsToInfo(unsigned char);
	void printNofRecievedMpMpNot();

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling (tools/cppstruct.py says so), and
	 * because a single access section keeps the class standard-layout, so
	 * that the __builtin_offsetof assertions in the .cpp are well defined.
	 */

	unsigned char pad_00[0x13];	/* +0x0000 not modelled           */

	/*
	 * +0x0013  Cleared by the constructor and by NOTHING else -- `reset`
	 * does not touch it, which is what separates the two.
	 */
	unsigned char byte_13;

	unsigned char pad_14[0xc74];	/* +0x0014 not modelled           */

	/*
	 * +0x0c88  Six buffers of 0x200 bytes, allocated in this order by the
	 * constructor and released in the same order by the destructor, each
	 * guarded by its own null test.  `void *` because the element type is
	 * not established: neither member does anything with them but store
	 * and free.  Six separate members and one array of six are
	 * indistinguishable here -- the accesses are individual, at constant
	 * offsets, in both directions.
	 */
	void *buf[V90CP_BUFS];

	unsigned char pad_ca0[4];	/* +0x0ca0 not modelled           */

	/* +0x0ca4  Zeroed by `resetDetector`, and so by `reset` and the ctor. */
	unsigned int word_ca4;

	unsigned char pad_ca8[1];	/* +0x0ca8 one access, width unknown */

	/* +0x0ca9  Cleared by `resetDetector`.  One byte, stored as a byte. */
	unsigned char byte_ca9;

	/* +0x0caa  Cleared by `resetDetector` alongside +0xca9. */
	unsigned char byte_caa;

	unsigned char pad_cab[1];	/* +0x0cab alignment              */

	/* +0x0cac  Set to 18 by `resetDetector`; the most-read field here. */
	int word_cac;

	/* +0x0cb0  Zeroed by `resetDetector`. */
	int word_cb0;

	unsigned char pad_cb4[4];	/* +0x0cb4 read by methods not written */

	/* +0x0cb8  The bit vector, one byte per bit.  See V90CP_BITS. */
	unsigned char bits[V90CP_BITS];

	/* +0x3b98  The CRC register, one byte per bit; `resetCRC` sets all. */
	unsigned char crc[V90CP_CRC];

	/* +0x3ba8  `calcSequenceLength`'s divisor: the group size. */
	unsigned int word_3ba8;

	/* +0x3bac  The sequence length `calcSequenceLength` computes and
	 * `getBitVector` reports through its reference argument. */
	unsigned int word_3bac;

	/* +0x3bb0  `calcSequenceLength`'s input: one less than the bit count
	 * it rounds up. */
	unsigned int word_3bb0;

	/* +0x3bb4  MP frames received, by the debug string's own words. */
	int nofRecievedMp;

	/* +0x3bb8  MPNot frames received. */
	int nofRecievedMpNot;

	/* +0x3bbc  Set to -1 by `reset` and by the constructor. */
	int word_3bbc;
};

#endif /* DSPLIB_V90CP_H */
