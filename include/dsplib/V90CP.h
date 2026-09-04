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
 * They hold the two counted blocks' bit lengths, seventeen to the entry:
 * `alpha` for `short_58` and `beta` for `buf`.  Finding F4363.
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
 * two diagnostics and `printNofRecievedMpMpNot`'s counter line.  So the
 * fields keep offset-anchored names and carry their derivation here.  The two
 * exceptions are the ones the code itself settles: `nof_58[k]` is the bound
 * of the loop over `short_58[k]` and `nof_buf[k]` the bound of the loop over
 * `buf[k]`, in BOTH directions, which makes "how many entries" a measured
 * fact and not a reading.  Finding F3540.
 *
 * `bitsToInfo` HAS NOW BEEN READ AND IT NAMES NOTHING EITHER.  Its two
 * strings are a bounds check on `bits` and a bad-CRC line that names the
 * message but no field of it, so all twenty-three members below keep their
 * offsets for names.  What it does settle is the ROLE of five of them --
 * `word_ca4` is the state, `word_cac` the cursor, `word_cb0` the count within
 * the current block, `byte_ca9` and `byte_caa` the run lengths of ones and of
 * zeros -- and roles are what the comments below now carry.  The sibling
 * `V90MP` reached the same four roles from its own driver and kept
 * `byte_19`, `byte_1a`, `byte_1b` and `word_14`, so this is the precedent and
 * not a new caution.  Finding F4360.
 */

#ifndef DSPLIB_V90CP_H
#define DSPLIB_V90CP_H

/*
 * The bit vector's extent, AND IT IS NOW MEASURED FROM BOTH ENDS.  Its START
 * is proven -- `getBitVector` hands back `this+0xcb8` -- and its LENGTH is
 * what the author's own bounds check says it is: five arms of `bitsToInfo`
 * refuse to store when the cursor is above 0x2edf and print "not enouch
 * memory in the buffer" instead, so 0x2edf is the last index that fits and
 * 0xcb8 + 0x2ee0 is 0x3b98, which is exactly where `crc` begins.  Lower bound
 * meets upper bound, as for the class itself.  Finding F4361; this used to
 * read "the modelling choice, not a measurement", which it was until
 * `bitsToInfo` was read.
 *
 * The length `getBitVector` reports is a different quantity: +0x3bac, which
 * `calcSequenceLength` computes at run time and which can be shorter.
 *
 * Also measured is that nothing else lives in the span at an offset of its
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
 * 0x200 bytes hold 0x80 of them.  NOTHING bounds the count against this: the
 * count travels in eight bits, so a peer may legally ask for 255, and both
 * loops run to it.  See docs/deviations.md D390 -- which used to say that
 * `bitsToInfo`'s "not enouch memory in the buffer" was the guard that caught
 * it, and that is wrong.  That guard is on the BIT VECTOR's index, one layer
 * further out, and nothing guards this.  Finding F4361.
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
	 * constructor repeats the five stores instead.  Findings F1237 and
	 * F4600 for why those two are spelled differently.
	 */
	unsigned char *getBitVector(unsigned int &length);
	void reset();
	void resetDetector();
	void resetCRC();
	void calcCRC();
	void calcSequenceLength();
	void printNofRecievedMpMpNot();

	/*
	 * DEFINED in src/pump/v90/V90CP.cpp, AND NOT VOID -- which is what
	 * this declaration used to say, on no evidence, because a return type
	 * is not mangled.  %edi is zeroed at entry and moved into %eax at both
	 * `ret`s, and 0, 1, 2, 3, 4 and 5 all reach it.  Same mistake as
	 * `evaluateCRC` above and as the sibling `V90MP::bitsToInfo`, in the
	 * same class and for the same reason.  Finding F4360.
	 *
	 * It is the receive-side driver: one arriving bit per call, storing
	 * into `bits` at `word_cac`, counting within the block in `word_cb0`,
	 * and stepping `word_ca4` through the states listed there.  Its two
	 * function-local statics `alpha` and `beta` are the two counted
	 * blocks' bit lengths.
	 */
	int bitsToInfo(unsigned char);

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
	 * +0x0013  Cleared by the constructor, and `reset` does not touch it,
	 * which is what separates the two.  `infoToBits` puts it at bits[0x21]
	 * in both the long and the short form, which is the only field that
	 * appears in both.
	 *
	 * THE SENTENCE THAT USED TO SAY "AND BY NOTHING ELSE" IS RETRACTED.
	 * Five `V90Phase4Modulator` members write it -- `recivedCP`,
	 * `recivedPartOneSilenceRrnSUV`, `recivedCPtag` and
	 * `recivedPartOneSilenceRrnSUVtag` set it to 1 and
	 * `resetRRNSecondSection` clears it -- so it is a bit the modulator
	 * raises when the demodulator reports a CP and lowers when the second
	 * section of a rate renegotiation begins.  The name stays the
	 * offset's: what bits[0x21] means on the wire is not something this
	 * object states.  Finding F4936.
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
	 *
	 * `bitsToInfo` IS WHAT DRIVES IT, over a wider range: its own switch
	 * is `cmp $0xd` / `ja` and a fourteen-entry table, so the states are
	 * 0..13 with 9 a hole there too.  What each one is collecting:
	 *
	 *      0   the preamble -- seventeen ones
	 *      1   the framing zero after it
	 *      2   the type bit at index 18
	 *      3   the short form's remaining fifteen bits
	 *      4   the three block flags
	 *      5   the rest of the header, to 0x33
	 *      6   six frames of pairs, to 0x99
	 *      7   the four nine-bit counts, 0x44 bits
	 *      8   the four counted lists, `alpha` bits
	 *      10  the values and the buffer counts, 0x55 bits
	 *      11  the six buffers, `beta` bits
	 *      12  the CRC frame, 0x11 bits
	 *      13  the tail, to the next cursor position divisible by six
	 *
	 * so the seven values `evaluateInfo` decodes are a subset of the
	 * fourteen the receiver walks, and 0, 1, 2, 4, 12 and 13 exist only
	 * on this side.  Finding F4360.
	 */
	unsigned int word_ca4;

	/*
	 * +0x0ca8  One byte, and `infoToBits` clearing it to zero on the long
	 * path is the second access -- enough to settle the width at one, not
	 * enough to say anything else about it.
	 */
	unsigned char byte_ca8;

	/*
	 * +0x0ca9  THE LENGTH OF THE CURRENT RUN OF ONES, in `bitsToInfo`:
	 * every one bit increments it and every zero clears it, and state 0
	 * leaves the preamble when it passes 0x10 -- seventeen ones, since
	 * sixteen are not enough.  Eight bits and it wraps: the object's
	 * `inc %cl` and `cmp $0x10,%cl` are both byte-wide.
	 */
	unsigned char byte_ca9;

	/*
	 * +0x0caa  AND THE LENGTH OF THE CURRENT RUN OF ZEROS, the mirror of
	 * +0xca9: every zero increments it and every one clears it.  A run of
	 * `2 * word_3ba8` zeros with the cursor still at its home 18 is the
	 * far end having stopped, and `bitsToInfo` answers 5.  The member is
	 * re-read out of the object after being cleared, which is why a
	 * `word_3ba8` of zero makes that test true on a ONE bit as well.
	 */
	unsigned char byte_caa;

	/*
	 * +0x0cab was `pad_cab[1]`: `byte_caa` ends at +0x0cab and `word_cac`
	 * below is a 4-byte-aligned `unsigned int`, so natural alignment
	 * inserts exactly this one byte with the member deleted -- the
	 * existing `V90CP_OFF(word_cac, 0x0cac, wordcac)` (V90CP.cpp) is what
	 * proves it.  Zero readers/writers anywhere in the object (confirmed
	 * via `tools/dis.py` over every `V90CP::` member function,
	 * `0x51150..0x53830`, and a whole-object grep for `0xcab(`); removed
	 * F10145.
	 */

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
	 * Finding F4362.
	 */
	unsigned int word_cac;

	/*
	 * +0x0cb0  Zeroed by `resetDetector`, and IT IS THE RECEIVE COUNTER:
	 * `bitsToInfo` counts the bits of the block currently arriving in it
	 * and compares the count against that block's length -- 0xf, 3, 0x44,
	 * 0x55, 0x11, or `alpha` or `beta` for the two whose length the
	 * message itself carries -- then clears it for the next block.
	 *
	 * UNSIGNED, and one instruction settles it.  `bitsToInfo`'s `case 4`
	 * switches over this field for the three block flags and the tree it
	 * compiles to reads `cmp $0x1` / `je` / `jb`, taking the `x < 1` edge
	 * straight to the `case 0` body.  That is only correct for an
	 * unsigned index; a signed one admits negatives below 1 and GCC emits
	 * `jl` plus a second test against zero.  Every other use is an
	 * equality compare and says nothing, and no test can hold this --
	 * the two readings agree over every value the field takes.  Finding
	 * F4365.
	 */
	unsigned int word_cb0;

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

	/*
	 * +0x3bbc  Set to -1 by `reset` and by the constructor, and READ BY
	 * ONE MEMBER ONLY: `bitsToInfo`'s tail.  It is a hold-off counter.
	 * At -1 it is idle, and an answer of 1 or 2 starts it at 0; from then
	 * on every call increments it until 0x320, where it goes back to -1,
	 * and while it is running the answers 4 and 2 are suppressed to 0
	 * (1, 3 and 5 are not).  `js` on the idle test, so signed.
	 *
	 * WHAT IT IS A HOLD-OFF FOR is not stated anywhere in the object, so
	 * the field keeps its offset for a name.  Finding F4360.
	 */
	int word_3bbc;
};

#endif /* DSPLIB_V90CP_H */
