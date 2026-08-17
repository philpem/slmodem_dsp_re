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
 *     +0x000..+0x014,         `infoToBits` (0x52230) and `evaluateInfo`
 *     +0x018..+0xc87,         (0x519f0), which are inverses of each other and
 *     +0xca0, +0xcb4          between them touch every byte of what used to
 *                             be `pad_00`, `pad_14`, `pad_ca0` and `pad_cb4`.
 *                             See THE MESSAGE below.
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
 * ONE `pad_*` IS LEFT, the alignment byte at +0xcab, and that is the whole of
 * the class not modelled as fields.  An offset landing in it is still the
 * answer; there is nowhere else for one to land.
 *
 * Two of the class's function-local statics are in .bss under their own
 * mangled names -- `V90CP::bitsToInfo(unsigned char)::alpha` and `::beta` --
 * so `bitsToInfo` carries state across calls.  Nothing here depends on that.
 *
 * THE MESSAGE IS SEVENTEEN-BIT FRAMES, and that is measured rather than
 * assumed.  `infoToBits` opens with seventeen ones at bits[0x00..0x10], and
 * every index that is a multiple of seventeen after that is written zero and
 * nothing else; the CRC loop, which walks bits[0x12] upwards, carries an
 * `if (i % 17 == 0) i++` that steps over exactly those positions.  So a frame
 * is one zero framing bit and sixteen information bits, the preamble is one
 * frame of ones, and every field below is placed inside that grid.  The
 * decoder walks the same grid backwards: `word_cb4` is one BELOW a frame's
 * framing bit, and each field is read most-significant bit first from the top
 * of its own span downwards.
 *
 * WHAT THE TWO HALVES AGREE ON, field by field.  The left column is what
 * `infoToBits` writes into `bits`, the right what `evaluateInfo` reads back
 * out of it, and every width and signedness below is the load or the store
 * that the object actually encodes:
 *
 *     +0x000  bits[0x12]      1 bit    NONZERO ENDS THE MESSAGE (see below)
 *     +0x004  bits[0x13]      1 bit    nonzero adds the +0x018 block
 *     +0x008  bits[0x14]      1 bit    nonzero adds the +0x048/+0x058 block
 *     +0x00c  bits[0x15]      1 bit    nonzero adds the +0xc58..+0xc9c block
 *     +0x010  bits[0x16..1a]  5 bits   `movsbl`, so signed
 *     +0x011  bits[0x1b..1c]  2 bits   a four-arm switch, not a shift loop
 *     +0x012  bits[0x1d]      1 bit
 *     +0x013  bits[0x21]      1 bit    in BOTH the long and the short form
 *     +0x014  bits[0x23..32] 16 bits   `movzwl`, so unsigned
 *     +0x018  6 frames       8+8 bits  six pairs, `movswl` on each
 *     +0x048  4 frames        9 bits   the four counts, `shr`, so unsigned
 *     +0x058  n frames       16 bits   nof_58[k] entries of short_58[k]
 *     +0xc70  2 frames        4 bits   six values, four to a frame
 *     +0xc58  3 frames       8+8 bits  the six counts, two to a frame
 *     +0xc88  n frames       16 bits   nof_buf[k] entries of buf[k]
 *
 * and the tail, which both the long and the short form reach: one zero, the
 * sixteen CRC bits, one zero, then zero padding out to a multiple of
 * +0x3ba8.  The CRC register is the CCITT one -- taps into positions 3 and 10
 * and a feed into 15 -- run one bit per byte over bits[0x12] up to the frame
 * the CRC itself occupies.
 *
 * THE SHORT FORM.  When +0x000 is nonzero `infoToBits` emits three frames and
 * stops: the preamble, bits[0x12] = +0x000 and bits[0x13..0x1f] zero, then
 * bits[0x20] = +0xca0 and bits[0x21] = +0x013, and straight to the CRC.  Its
 * inverse is `evaluateInfo`'s `case 3`, which reads exactly those two bits
 * back.  V90MP has a bit in the same position that the author's own
 * diagnostic calls `Type`, and there Type == 0 is what shortens the message;
 * here it is Type != 0, so the sense is opposite and the name is NOT carried
 * across.  It stays `word_00`.
 *
 * WHAT IS MODELLED BUT NOT NAMED, and why.  Every byte of the old `pad_14` is
 * now typed and dimensioned, because the two functions force the type, the
 * element size and the array bound of all of it.  Almost none of it is
 * NAMED, because nothing in the object says what any of it means:
 * `infoToBits` and `evaluateInfo` reference no string at all -- one
 * relocation between them, and it is `evaluateInfo`'s jump table -- and the
 * only three strings the whole translation unit reaches are `bitsToInfo`'s
 * two errors and `printNofRecievedMpMpNot`'s counter line.  So the fields
 * keep offset-anchored names and carry their derivation here.  The two
 * exceptions are the ones the code itself settles: `nof_58[k]` is the bound
 * of the loop over `short_58[k]` and `nof_buf[k]` the bound of the loop over
 * `buf[k]`, in BOTH directions, which makes "how many entries" a measured
 * fact and not a reading.  Finding 3540.
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

/*
 * How many entries fit in one of those six buffers.  `evaluateInfo` writes a
 * full 32-bit word at `(%edi,%ebp,4)` and `infoToBits` reads the signed low
 * half of the same slot with `movswl`, so the element is four bytes wide and
 * 0x200 bytes hold 0x80 of them.  Nothing bounds the count against this --
 * the count travels in eight bits, so a peer may legally ask for 255 -- and
 * `bitsToInfo` is where that is caught, with "*** error CP bit , not enouch
 * memory in the buffer ***".  See docs/deviations.md D390.
 */
#define V90CP_BUFENTS	(V90CP_BUFSIZE / 4)

/*
 * The four equal spans between +0x058 and +0xc58, 0x300 bytes each.  Element
 * type is `short`: `infoToBits` indexes them `(%esi,%ebp,2)` and
 * `evaluateInfo` stores back through `movw`, so the stride is two and the
 * accumulator it builds is sixteen bits wide.  0x300 / 2 = 384.
 */
#define V90CP_SHORTS	384

class V90CP {
public:
	/*
	 * Allocate the six buffers and clear the detector.  The only two
	 * members defined in src/pump/v90/V90CP.cpp.
	 */
	V90CP();
	~V90CP();

	/*
	 * ALSO DEFINED, in src/pump/v90/V90CP.cpp.  Six of the seven leave
	 * %eax alone on every path and really are `void`; `getBitVector` is
	 * the exception and returns `this+0xcb8`.  The return type is not
	 * mangled, so that had to be read out of each epilogue rather than
	 * off the symbol.
	 *
	 * `reset` calls `resetDetector` and the compiler inlines it; the
	 * constructor repeats the five stores instead.  Findings 1237 and
	 * 4300 for why those two are spelled differently.
	 */
	unsigned char *getBitVector(unsigned int &length);
	void reset();
	void resetDetector();
	void resetCRC();
	void calcCRC();
	void calcSequenceLength();
	void printNofRecievedMpMpNot();

	/*
	 * Declared, not defined -- defining a method whose callees are not
	 * written breaks the link for the whole suite (docs/v90cpp.md), and a
	 * declaration is a specification where a definition is a claim.  The
	 * argument type is the mangling's and is exact; the return type is
	 * not mangled, so `void` here means "not established".
	 */
	void bitsToInfo(unsigned char);

	/*
	 * Defined in src/pump/v90/V90CP.cpp.  Both really are void: neither
	 * arranges %eax on any path, and `infoToBits`'s single `ret` leaves
	 * the sequence length there only because that is what the last
	 * expression computed.
	 */
	void evaluateInfo();
	void infoToBits();

	/*
	 * ALSO DEFINED, AND NOT VOID.  `evaluateCRC` closes with
	 * `xor %eax,%eax` / `sete %al`, which is a return value being built
	 * and not a leftover, so the declaration it used to carry was wrong
	 * in a way the mangling cannot see.  Nonzero means the sixteen bits
	 * the peer sent match the sixteen this end computed.
	 */
	int evaluateCRC();

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling (tools/cppstruct.py says so), and
	 * because a single access section keeps the class standard-layout, so
	 * that the __builtin_offsetof assertions in the .cpp are well defined.
	 */

	/*
	 * +0x0000  NONZERO SELECTS THE SHORT FORM.  `infoToBits` tests it,
	 * copies its low byte into bits[0x12] either way, and on nonzero
	 * emits three frames and stops.  Read 32 bits, so `int` and not the
	 * one bit it travels as.
	 */
	int word_00;

	/*
	 * +0x0004, +0x0008, +0x000c  Three more whole-word flags, each one
	 * bit on the wire and each gating one block of the long form:
	 * +0x0004 the six pairs at +0x0018, +0x0008 the four counted lists at
	 * +0x0048, +0x000c everything from +0xc58 on.  `evaluateInfo` never
	 * writes them -- the decoder is told which block to expect by
	 * `word_ca4` instead -- so only `infoToBits` establishes them.
	 */
	int word_04;
	int word_08;
	int word_0c;

	/*
	 * +0x0010  Five bits, and SIGNED: `infoToBits` loads it `movsbl` and
	 * shifts the sign-extended word with `sar`, which is the reading a
	 * negative value would need and an unsigned one would not.
	 * `evaluateInfo` rebuilds it eight bits at a time in `%cl`.
	 */
	signed char byte_10;

	/*
	 * +0x0011  Two bits, and the ONE FIELD THE OBJECT DOES NOT SHIFT.
	 * `infoToBits` switches over 0, 1, 2 and 3 and writes the pair of
	 * bits as constants; a value outside that range leaves bits[0x1b]
	 * and bits[0x1c] AS THEY WERE, which a shift loop could not do and
	 * which the differential test checks.  Four arms written out is what
	 * an enumeration compiles to, but nothing here names one.
	 */
	unsigned char byte_11;

	/* +0x0012  One bit, copied whole into bits[0x1d]. */
	unsigned char byte_12;

	/*
	 * +0x0013  Cleared by the constructor and by NOTHING else -- `reset`
	 * does not touch it, which is what separates the two.  `infoToBits`
	 * puts it at bits[0x21] in both the long and the short form, which is
	 * the only field that appears in both.
	 */
	unsigned char byte_13;

	/*
	 * +0x0014  Sixteen bits, and UNSIGNED where +0x0010 is signed:
	 * `movzwl` and `shr` against the other's `movsbl` and `sar`.  Stored
	 * back 32 bits wide by `evaluateInfo`, so the member is `int` and the
	 * narrowing is at the use.
	 */
	int word_14;

	/*
	 * +0x0018  Six frames of two eight-bit values.  `infoToBits` reads
	 * them `movswl 0x18(%esi,%ebp,8)` and `movswl 0x1c(...)`, so the pair
	 * is eight bytes and each half is read as a signed short; the flat
	 * twelve is `evaluateInfo`'s, which clears the whole block with a
	 * single `(%esi,%eax,4)` loop of twelve before decoding it as pairs.
	 */
	int word_18[12];

	/*
	 * +0x0048  HOW MANY ENTRIES OF `short_58[k]` ARE LIVE, and the one
	 * thing about this block that is not a reading: it is the bound of
	 * the loop in `infoToBits` and of the loop in `evaluateInfo`, both
	 * unsigned, both against the member itself.  Nine bits on the wire.
	 */
	unsigned int nof_58[4];

	/*
	 * +0x0058  Four lists of shorts, one frame per entry, sixteen bits
	 * each.  What they hold is not established anywhere in the object.
	 */
	short short_58[4][V90CP_SHORTS];

	/*
	 * +0x0c58  HOW MANY ENTRIES OF `buf[k]` ARE LIVE -- same argument as
	 * `nof_58`, in both directions.  Eight bits on the wire, packed two
	 * to a frame, which is why `infoToBits` walks them
	 * `0xc58(%esi,%ebp,8)` and `0xc5c(...)` in three passes and
	 * `evaluateInfo` clears all six with a stride of four.
	 */
	unsigned int nof_buf[V90CP_BUFS];

	/*
	 * +0x0c70  Six four-bit values, four to a frame and then two, one per
	 * buffer by position but not by any evidence in the object.
	 */
	int word_c70[V90CP_BUFS];

	/*
	 * +0x0c88  Six buffers of 0x200 bytes, allocated in this order by the
	 * constructor and released in the same order by the destructor, each
	 * guarded by its own null test.  Six separate members and one array
	 * of six are indistinguishable here -- the accesses are individual,
	 * at constant offsets, in both directions.
	 *
	 * THE ELEMENT TYPE IS NOW ESTABLISHED and the `void *` this used to
	 * be is gone: `evaluateInfo` builds a 32-bit accumulator and stores
	 * it whole at `(%edi,%ebp,4)`, and `infoToBits` reads the same slot
	 * back with `movswl`.  A four-byte store settles the stride and the
	 * width; the `movswl` is a narrowing at the use and not a second
	 * type, because a two-byte element would have made the stride two.
	 */
	int *buf[V90CP_BUFS];

	/*
	 * +0x0ca0  The short form's payload: `infoToBits` puts its low byte
	 * at bits[0x20] and `evaluateInfo`'s `case 3` reads that bit back
	 * into all 32 bits of it.  Written as a word, so it is one.
	 */
	unsigned int word_ca0;

	/*
	 * +0x0ca4  Zeroed by `resetDetector`, and so by `reset` and the ctor.
	 * IT IS THE DECODER'S STATE: `evaluateInfo` is one switch over it and
	 * nothing else, `sub $3` then `cmp $8` then a nine-entry jump table,
	 * so the live values are 3..11 and 4 and 9 fall through the table to
	 * the same `ret` as everything outside the range.  Each arm decodes
	 * one block of the message, in the order the blocks appear on the
	 * wire.  Which value means which arm is in src/pump/v90/V90CP.cpp.
	 */
	unsigned int word_ca4;

	/*
	 * +0x0ca8  One byte, and `infoToBits` clearing it to zero on the long
	 * path is the second access -- enough to settle the width at one, not
	 * enough to say anything else about it.
	 */
	unsigned char byte_ca8;

	/* +0x0ca9  Cleared by `resetDetector`.  One byte, stored as a byte. */
	unsigned char byte_ca9;

	/* +0x0caa  Cleared by `resetDetector` alongside +0xca9. */
	unsigned char byte_caa;

	unsigned char pad_cab[1];	/* +0x0cab alignment              */

	/*
	 * +0x0cac  Set to 18 by `resetDetector`; the most-read field here.
	 * IT IS THE CURSOR, and it is the SAME cursor in both directions:
	 * `infoToBits` leaves the index of the next free bit in it after
	 * every field it lays down, and `bitsToInfo` stores each arriving bit
	 * at it and steps it on.  18 is where the first data bit goes -- one
	 * preamble frame of seventeen, then the next frame's framing bit at
	 * 17 and its first data bit at 18.
	 *
	 * UNSIGNED, and that is forced rather than chosen.  `bitsToInfo`
	 * bounds it with `cmp $0x2edf,%eax` / `ja` -- an unsigned above, not
	 * `jg` -- and divides it by six with the 0xaaaaaaab reciprocal and a
	 * plain `shr`, which is the unsigned magic; a signed `% 6` needs the
	 * sign correction the object does not encode.  Nothing anywhere in
	 * the class forces signed, so `unsigned int` is the simpler source.
	 * Finding 4362.
	 */
	unsigned int word_cac;

	/* +0x0cb0  Zeroed by `resetDetector`. */
	int word_cb0;

	/*
	 * +0x0cb4  THE READ CURSOR, and the mirror of +0x0cac: every arm of
	 * `evaluateInfo` starts from it, walks DOWNWARDS through one field's
	 * bits -- most significant first -- and leaves it one below the next
	 * frame's framing bit.  Two arms end by storing an absolute constant
	 * into it instead (0x32 and 0x98), which is how the fixed part of the
	 * layout above is pinned rather than inferred.
	 */
	unsigned int word_cb4;

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
