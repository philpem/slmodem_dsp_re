/*
 * V90MP.h -- the V.90 MP message, one byte per bit.
 *
 * Reconstructed from dsplibs.o V90MP.cpp.  `V90MP` is not polymorphic --
 * tools/cppstruct.py lists its destructor with the two ordinary variants and
 * not the deleting one, and GCC emits a deleting destructor only for a
 * virtual one -- so offset 0 is a real member and there is no vptr.
 *
 * THE SIZE IS THE GAP BETWEEN TWO MEMBERS OF V90Modem, which builds both in
 * place:
 *
 *     194ef:  lea 0xcd0(%esi),%ebp        this object
 *     194f5:  lea 0xdf4(%esi),%edi        the V90CP member
 *     194fe:  call _ZN5V90MPC1Ev
 *     19506:  call _ZN5V90CPC1Ev
 *
 * 0xdf4 - 0xcd0 = 0x124 = 292, and the last field the class touches is the
 * four-byte +0x120, which ends exactly there.  Neither class is more than
 * four-byte aligned (+0xdf4 is not eight-byte aligned), so no padding hides
 * between them.
 *
 * WHICH MEMBER PROVED WHICH OFFSET.  Only the constructor and the destructor
 * are defined here; the rest was read out of members that are declared and
 * deliberately left undefined:
 *
 *     +0x014,+0x019,+0x01a,   `resetDetector` (0x1f3c0), whose whole body is
 *     +0x01b                  these four stores
 *     +0x01c                  `getBitVector` (0x1f700) returns `this+0x1c`
 *     +0x102                  `resetCRC` (0x1f150) writes 1 to sixteen bytes
 *                             from here
 *     +0x114,+0x118,+0x119    `calcSequenceLength` (0x1f910) rounds +0x119+1
 *                             up to a multiple of +0x114 into +0x118
 *     +0x11c,+0x120           `printNofRecievedMpMpNot` (0x20bc0) prints them
 *                             as "received %d MP, %d MPNot"
 *
 * THE CONSTRUCTOR AND `reset` ARE THE SAME FORTY BYTES, instruction for
 * instruction, and both are plain GLOBAL symbols in `.text` rather than in a
 * linkonce section -- so `reset` is not an in-class inline that the compiler
 * folded into the constructor (GCC 3.4 at -O2 does not inline an ordinary
 * global function).  The original repeated the assignments.  Finding 1237.
 */

#ifndef DSPLIB_V90MP_H
#define DSPLIB_V90MP_H

/*
 * The bit vector's extent.  START proven -- `getBitVector` hands back
 * `this+0x1c` -- and END is where the CRC register begins.  That the whole
 * span is ONE array is a modelling choice: no method establishes the array's
 * own length, and the length `getBitVector` reports is the byte at +0x118,
 * which `calcSequenceLength` computes at run time.
 *
 * What IS measured is that nothing else lives in the span at an offset of its
 * own: over all sixteen of the class's symbols, every `this`-relative
 * displacement between +0x1c and +0x102 lies below +0xd8, and there is not
 * one at any higher offset until the CRC.
 */
#define V90MP_BITS	0xe6		/* 0x1c .. 0x101, 230 bytes */

/* The CRC register: sixteen bytes, one per bit, `resetCRC` sets them all. */
#define V90MP_CRC	16

class V90MP {
public:
	/*
	 * Clear the detector.  The only two members defined in
	 * src/pump/v90/V90MP.cpp.
	 */
	V90MP();
	~V90MP();

	/*
	 * Declared, not defined -- see V90CP.h.  Argument types are the
	 * mangling's and exact; return types are not mangled, so `void` means
	 * "not established" for all but `getBitVector`, which leaves
	 * `this+0x1c` in %eax.
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
	void bitsToInfo(int);
	/*
	 * Void, and MEASURED rather than assumed: 0x20bef is a CALL to
	 * `dsplibs_debug_printf` and the two instructions after it are
	 * `add $0xc,%esp; ret`.  Nothing arranges %eax, so whatever is in it
	 * is the callee's return by accident.  `reset` reads the same way:
	 * %eax is left holding `this` because that is where the argument was
	 * loaded, not because anything returns it.
	 */
	void printNofRecievedMpMpNot();
	void PrintBase2(char *, unsigned long, unsigned short);

	/* Public for the same reason as V90Jd's: it keeps the class
	 * standard-layout, so the offsetof assertions are well defined. */

	unsigned char pad_00[0x14];	/* +0x000 not modelled            */

	/* +0x014  Zeroed by `resetDetector`, and so by `reset` and the ctor. */
	unsigned int word_14;

	unsigned char pad_18[1];	/* +0x018 not modelled            */

	/* +0x019  Cleared by `resetDetector`.  One byte, stored as a byte. */
	unsigned char byte_19;

	/* +0x01a  Cleared by `resetDetector` alongside +0x19. */
	unsigned char byte_1a;

	/*
	 * +0x01b  Set to 18 by `resetDetector`.  A BYTE here, where the two
	 * CP classes hold the same 18 in a four-byte field: `movb $0x12` at
	 * 0x1f3c4 against `movl $0x12` at 0x51514 and 0x4e834.  The three
	 * detectors are not one shared sub-object.
	 */
	unsigned char byte_1b;

	/* +0x01c  The bit vector, one byte per bit.  See V90MP_BITS. */
	unsigned char bits[V90MP_BITS];

	/* +0x102  The CRC register, one byte per bit; `resetCRC` sets all. */
	unsigned char crc[V90MP_CRC];

	unsigned char pad_112[2];	/* +0x112 alignment               */

	/* +0x114  `calcSequenceLength`'s divisor: the group size. */
	unsigned int word_114;

	/*
	 * +0x118  The sequence length, which `getBitVector` reports through
	 * its reference argument.  Unsigned and one byte: `calcSequenceLength`
	 * stores it with `mov %al`, and `getBitVector` loads it with `movzbl`
	 * into a 32-bit result that is then stored whole.
	 */
	unsigned char byte_118;

	/*
	 * +0x119  `calcSequenceLength`'s input, likewise read with `movzbl`.
	 */
	unsigned char byte_119;

	unsigned char pad_11a[2];	/* +0x11a alignment               */

	/* +0x11c  MP frames received, by the debug string's own words. */
	int nofRecievedMp;

	/* +0x120  MPNot frames received. */
	int nofRecievedMpNot;
};

#endif /* DSPLIB_V90MP_H */
