/**
 * @file V90MP.h
 * @brief `V90MP`: the V.90 MP message, one byte per bit.
 *
 * `V90MP` is not polymorphic (its destructor has no deleting variant, so
 * there is no vptr and offset 0 is a real member). Its size, `sizeof == 0x124`,
 * comes from the gap between it and the sibling `V90CP` member `V90Modem`
 * constructs right after it in place.
 *
 * The message itself is eleven 17-bit frames (five when `Type` is zero),
 * laid out by infoToBits() and read back by evaluateInfo():
 *
 *     frame 0   bits[0x00..0x10]  seventeen ones
 *     frame k   bits[17k]         a zero framing bit, then sixteen data bits
 *
 *     0x12 Type   0x13..0x17 zero   0x18..0x1b Rate    0x1c zero
 *     0x1d..0x1e Trellis           0x1f NonLin  0x20 Shaping  0x21 CPack
 *     0x24..0x31 rate mask (14 bits)
 *     0x34..0x43 h1Real  0x45..0x54 h1Imag  0x56..0x65 h2Real
 *     0x67..0x76 h2Imag  0x78..0x87 h3Real  0x89..0x98 h3Imag
 *     0x9a..0xa9 zero               0xab..0xba the CRC
 *
 * so the sequence is 0xbb bits long, or 0x55 when `Type` is zero and the
 * message stops after frame 4 with the CRC at 0x45..0x54. Findings F1385,
 * F1386.
 *
 * The constructor and reset() are the same code, instruction for
 * instruction -- both plain global symbols, so reset() is not an in-class
 * inline the compiler folded into the constructor; the original simply
 * repeated the assignments. Finding F1237.
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
	/** @brief Construct an MP message object and clear the detector state. */
	V90MP();
	/** @brief Destroy an MP message object (clears the frame counters). */
	~V90MP();

	/**
	 * @brief Return the decoded/encoded bit vector and its length.
	 * @param length  Set to the sequence length computed by calcSequenceLength().
	 * @return Pointer to `bits`, this object's bit vector (`this+0x1c`).
	 */
	unsigned char *getBitVector(unsigned int &length);
	/** @brief Reset the whole object to its just-constructed state. */
	void reset();
	/** @brief Reset the receiver's frame-detector state (word_14, byte_19, byte_1a, byte_1b). */
	void resetDetector();
	/** @brief Reset the sixteen-byte CRC register to all ones. */
	void resetCRC();
	/** @brief Compute the sequence length (byte_118) from the received type bit. */
	void calcSequenceLength();

	/**
	 * @brief Decode the packed bit vector into the message fields.
	 *
	 * Reads `bits` back into `Type`..`h3Imag` and `type`, the inverse of
	 * infoToBits().
	 */
	void evaluateInfo();
	/**
	 * @brief Encode the message fields into the packed bit vector.
	 *
	 * Lays `Type`..`h3Imag` out into `bits` as the eleven 17-bit frames
	 * (five when `Type` is zero) described in the file comment (F1385).
	 */
	void infoToBits();
	/**
	 * @brief Feed one received bit into the frame detector.
	 *
	 * Advances the detector's state machine (word_14) bit by bit --
	 * hunting the preamble, the framing bit, the type bit, the message
	 * body, then the trailing pad -- and, once a full sequence has
	 * arrived, calls evaluateInfo() and bumps the appropriate frame
	 * counter.
	 *
	 * @param bit  The next received bit (0 or 1).
	 * @return 3 if this bit completed an "Ed" (short/no-message) sequence, 1 for MP, 2 for MPnot.
	 */
	int bitsToInfo(int bit);

	/** @brief Compute the sixteen-bit CRC over `bits` into `crc`. */
	void calcCRC();
	/**
	 * @brief Check the received CRC against the computed one.
	 * @return 1 if the sixteen received CRC bits match the sixteen computed ones, 0 otherwise.
	 */
	int evaluateCRC();
	/** @brief Log the received MP/MPnot frame counts via the debug printf. */
	void printNofRecievedMpMpNot();
	/**
	 * @brief Render `value`'s low `nofBits` bits as a base-2 string.
	 * @param out      Destination buffer, `nofBits + 1` bytes.
	 * @param value    The value to render.
	 * @param nofBits  How many low-order bits to render.
	 */
	void PrintBase2(char *out, unsigned long value, unsigned short nofBits);

	/* Public for the same reason as V90Jd's: it keeps the class
	 * standard-layout, so the offsetof assertions are well defined. */

	/*
	 * +0x000..+0x013  The decoded message: `evaluateInfo` writes every one
	 * of these out of `bits`, `infoToBits` reads every one back, and
	 * `bitsToInfo` prints six of them by name. The six chars then seven
	 * shorts fill the twenty bytes exactly with no padding (keeping
	 * `word_14` four-aligned and `sizeof` at 0x124), and all are signed --
	 * forced by the diagnostic's sign-extending loads. See finding F1385.
	 */
	char Type;			/* +0x000 bits[0x12]              */
	char Rate;			/* +0x001 bits[0x18..0x1b]        */
	char Trellis;			/* +0x002 bits[0x1d..0x1e]        */
	char NonLin;			/* +0x003 bits[0x1f]              */
	char Shaping;			/* +0x004 bits[0x20]              */

	/*
	 * +0x005  bits[0x21], the message's own discriminator: zero is "MP
	 * detected", anything else "MPnot detected", and picks which of the
	 * two counters `bitsToInfo` bumps. See F1385.
	 */
	char CPack;

	/* +0x006  bits[0x24..0x31], printed base 2 with the mask 0x2000. See F1385. */
	short rateMask;

	short h1Real;			/* +0x008 bits[0x34..0x43]        */
	short h1Imag;			/* +0x00a bits[0x45..0x54]        */
	short h2Real;			/* +0x00c bits[0x56..0x65]        */
	short h2Imag;			/* +0x00e bits[0x67..0x76]        */
	short h3Real;			/* +0x010 bits[0x78..0x87]        */
	short h3Imag;			/* +0x012 bits[0x89..0x98]        */

	/*
	 * +0x014  Zeroed by `resetDetector`, and so by `reset` and the ctor.
	 *
	 * It is the receiver's state, and `bitsToInfo` switches on it over
	 * 0..4 through a five-entry jump table at `.rodata+0x7b8`, guarded by
	 * `cmp $0x4,%eax; ja` -- an UNSIGNED comparison, which is what makes
	 * the field unsigned rather than the `movl $0x3` stores.
	 *
	 *     0  hunting for the seventeen-ones preamble
	 *     1  seen it, waiting for the framing zero
	 *     2  the type bit, which fixes the sequence length
	 *     3  filling `bits` until +0x119 bits have arrived, then the CRC
	 *     4  running out the padding to +0x118, then `evaluateInfo`
	 */
	unsigned int word_14;

	/*
	 * +0x018  The received message's type bit, kept apart from `Type`
	 * because the receiver needs it before `evaluateInfo` has run: state 2
	 * of `bitsToInfo` stores the incoming bit here and every later
	 * decision -- the sequence length, the extent of the CRC -- is taken
	 * from it.  `infoToBits` writes it too, so a message that is packed
	 * and then unpacked agrees with itself.
	 */
	char type;

	/*
	 * +0x019  Cleared by `resetDetector`.  One byte, stored as a byte.
	 *
	 * The run of one bits: `bitsToInfo` increments it for every one and
	 * clears it on a zero, and state 0 leaves for state 1 when it passes
	 * sixteen -- the seventeen-bit preamble.
	 */
	unsigned char byte_19;

	/*
	 * +0x01a  Cleared by `resetDetector` alongside +0x19: the run of ZERO
	 * bits, the mirror image.  A run of 2 * +0x114 zeros with the bit
	 * index still at its initial 18 is the object's "Ed detected".
	 */
	unsigned char byte_1a;

	/*
	 * +0x01b  Set to 18 by `resetDetector`.  A BYTE here, where the two
	 * CP classes hold the same 18 in a four-byte field: `movb $0x12` at
	 * 0x1f3c4 against `movl $0x12` at 0x51514 and 0x4e834.  The three
	 * detectors are not one shared sub-object.
	 *
	 * It is the index of the next bit of `bits` to fill, and 18 is where
	 * the message's own content starts -- frame 0 is seventeen ones and
	 * bit 17 is a framing zero, so neither is ever stored.
	 */
	unsigned char byte_1b;

	/* +0x01c  The bit vector, one byte per bit.  See V90MP_BITS. */
	unsigned char bits[V90MP_BITS];

	/* +0x102  The CRC register, one byte per bit; `resetCRC` sets all. */
	unsigned char crc[V90MP_CRC];

	/*
	 * +0x112 was `pad_112[2]` -- REMOVED (finding F10151).  Already
	 * correctly described as alignment; proved mechanically by the
	 * existing `V90MP_OFF(crc, 0x102, ...)`/`V90MP_OFF(word_114, 0x114,
	 * ...)` and by `dis.py` over every `V90MP` method finding no access
	 * to 0x112/0x113.
	 */

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

	/*
	 * +0x11a was `pad_11a[2]` -- REMOVED (finding F10151).  Already
	 * correctly described as alignment; proved mechanically by the
	 * existing `V90MP_OFF(byte_119, 0x119, ...)`/`V90MP_OFF
	 * (nofRecievedMp, 0x11c, ...)` and by `dis.py` finding no access to
	 * 0x11a/0x11b.
	 */

	/*
	 * +0x11c  MP frames received, by the debug string's own words.
	 * Unsigned: `bitsToInfo` gates its diagnostics on an unsigned `ja`
	 * compare against the bumped counter, not the signed reading the
	 * printf's `%d` alone would suggest. See F1385.
	 */
	unsigned int nofRecievedMp;

	/* +0x120  MPNot frames received; same unsigned proof. See F1385. */
	unsigned int nofRecievedMpNot;
};

#endif /* DSPLIB_V90MP_H */
