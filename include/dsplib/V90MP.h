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
 * WHICH MEMBER PROVED WHICH OFFSET.  The first block was read out of members
 * that are declared and deliberately left undefined; the last two come from
 * `evaluateInfo`, `infoToBits` and `bitsToInfo`, which are defined:
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
 *     +0x000..+0x013          the decoded message, named by `bitsToInfo`'s own
 *                             diagnostic:  "MP detected. Type%d,Rate%d,
 *                             Trellis%d,NonLin%d,Shaping%d,CPack%d" for the
 *                             six chars, "Rate Mask - %s" for +0x006, and
 *                             "h1 real = %d, imag = %d" (h2, h3) for the six
 *                             shorts.  Widths are the loads at 0x20b26..0x20b4c
 *                             (`movsbl` on +0x00..+0x05) and 0x2021d..0x20273
 *                             (`movswl` on +0x08..+0x12).
 *
 *     +0x018                  `bitsToInfo` state 2 stores the incoming bit
 *                             there (0x203b5), `infoToBits` copies +0x000 into
 *                             it (0x1fa66) and `evaluateInfo` copies it back
 *                             out (0x1f730).
 *
 * THE MESSAGE IS ELEVEN 17-BIT FRAMES (five when `Type` is zero), which is
 * what `infoToBits` lays out and `evaluateInfo` reads back:
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
 * message stops after frame 4 with the CRC at 0x45..0x54.  Findings F1385,
 * F1386.
 *
 * THE CONSTRUCTOR AND `reset` ARE THE SAME FORTY BYTES, instruction for
 * instruction, and both are plain GLOBAL symbols in `.text` rather than in a
 * linkonce section -- so `reset` is not an in-class inline that the compiler
 * folded into the constructor (GCC 3.4 at -O2 does not inline an ordinary
 * global function).  The original repeated the assignments.  Finding F1237.
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
	/* Clear the detector. */
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
	void calcSequenceLength();

	/*
	 * Defined in src/pump/v90/V90MP.cpp.  `evaluateInfo` and `infoToBits`
	 * really are void -- neither arranges %eax on any path, and the two
	 * `ret`s of `evaluateInfo` (0x1f901) leave different leftovers there.
	 *
	 * `bitsToInfo` is NOT: %edi is zeroed at entry (0x201ab), set to 3 for
	 * Ed, 1 for MP and 2 for MPnot, and moved to %eax at both returns
	 * (0x20290, 0x2040e).  It takes one received bit and answers what that
	 * bit completed.
	 */
	void evaluateInfo();
	void infoToBits();
	int bitsToInfo(int bit);

	/*
	 * The CRC pair, also defined in src/pump/v90/V90MP.cpp.  `calcCRC`
	 * (0x1f170) is void -- it ends `add $0x10,%esp` / four pops / `ret`
	 * with nothing arranging %eax, and the byte left in %al is the last
	 * feedback bit by accident.
	 *
	 * `evaluateCRC` (0x1f470) is NOT, and this declaration used to say it
	 * was: the epilogue at 0x1f6ec is `xor %eax,%eax` / `test %bl,%bl` /
	 * `sete %al`, and a leftover is never built with a `sete`.  It is the
	 * same correction the V90CP twin's comment records making, and the
	 * mangling cannot see it because return types are not mangled -- the
	 * symbol is `_ZN5V90MP11evaluateCRCEv` either way.  It answers 1 when
	 * the sixteen received CRC bits match the sixteen it computed.
	 */
	void calcCRC();
	int evaluateCRC();
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

	/*
	 * +0x000..+0x013  THE DECODED MESSAGE.  `evaluateInfo` writes every
	 * one of these out of `bits`, `infoToBits` reads every one back, and
	 * `bitsToInfo` prints six of them by name.  The six chars then the
	 * seven shorts fill the twenty bytes exactly, with no padding, which
	 * is what keeps `word_14` four-aligned and `sizeof` at 0x124.
	 *
	 * SIGNED, not unsigned: the diagnostic loads +0x00..+0x05 with
	 * `movsbl` (0x20b26) and +0x06..+0x12 with `movswl` (0x2021d), and
	 * `infoToBits` shifts `Rate` right with `sar` (0x1f9e4) and switches
	 * on `Trellis` with `jle` (0x1fa00).  For `Rate` and `Trellis` that is
	 * the load width alone -- `evaluateInfo` only ever puts 0..15 and 0..3
	 * in them, so no value they can hold reads differently either way.
	 */
	char Type;			/* +0x000 bits[0x12]              */
	char Rate;			/* +0x001 bits[0x18..0x1b]        */
	char Trellis;			/* +0x002 bits[0x1d..0x1e]        */
	char NonLin;			/* +0x003 bits[0x1f]              */
	char Shaping;			/* +0x004 bits[0x20]              */

	/*
	 * +0x005  bits[0x21], and the message's own discriminator: zero is
	 * "MP detected", anything else "MPnot detected", and it picks which of
	 * the two counters `bitsToInfo` bumps (0x2031d).
	 */
	char CPack;

	/* +0x006  bits[0x24..0x31], printed base 2 with the mask 0x2000. */
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
	 * +0x112 was `pad_112[2]` -- REMOVED (finding F10145).  Already
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
	 * +0x11a was `pad_11a[2]` -- REMOVED (finding F10145).  Already
	 * correctly described as alignment; proved mechanically by the
	 * existing `V90MP_OFF(byte_119, 0x119, ...)`/`V90MP_OFF
	 * (nofRecievedMp, 0x11c, ...)` and by `dis.py` finding no access to
	 * 0x11a/0x11b.
	 */

	/*
	 * +0x11c  MP frames received, by the debug string's own words.
	 *
	 * UNSIGNED, which the `%d` of `printNofRecievedMpMpNot` cannot tell
	 * you and `bitsToInfo` can: having bumped the counter it gates its
	 * diagnostics on `cmp $0x2,%ebp; ja` (0x20a34), and `ja` is the
	 * unsigned comparison.  The two readings part at 0x80000000, where the
	 * unsigned one is above two and the signed one below it, so the
	 * transcript at level 2 decides -- t_v90mp.cpp drives exactly that.
	 */
	unsigned int nofRecievedMp;

	/* +0x120  MPNot frames received; `ja` at 0x2033b likewise. */
	unsigned int nofRecievedMpNot;
};

#endif /* DSPLIB_V90MP_H */
