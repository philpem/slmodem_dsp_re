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
 * WHICH MEMBER PROVED WHICH OFFSET.  The constructor, the destructor and the
 * seven small members are in src/pump/v90/V92CP.cpp; `infoToBits` is there
 * too and is what proved everything below +0x104.  The rest of the map was
 * read out of members that are declared and deliberately left undefined:
 *
 *     +0x000..+0x103          `infoToBits` (0x4ec80), which packs the whole
 *                             block into `bits`, and `setV92CPpckFromParams-
 *                             Info` (0x33920), which fills it
 *     +0x104,+0x108           `setSUV` (0x4e920), whose whole body is
 *                             "+0x104 = 16; +0x108 = the argument"
 *     +0x114,+0x119,+0x11a,   `resetDetector` (0x4e830), whose whole body is
 *     +0x11c,+0x120           these five stores
 *     +0x118,+0x128           `infoToBits`
 *     +0x124                  `evaluateInfo` (0x4f400), the READ cursor
 *     +0x129                  `getBitVector` (0x4ebe0) returns `this+0x129`
 *     +0x8f9                  `resetCRC` (0x4e5d0) writes 1 to sixteen bytes
 *                             from here
 *     +0x90c,+0x910           `infoToBits` writes both and fixes the relation
 *                             between them; see `msgLen` and `vectorLen`
 *     +0x914                  `reset` (0x4e860) and the constructor set -1
 *
 * ALL TWELVE ARE NOW WRITTEN.  `evaluateInfo` and `bitsToInfo` were the last
 * two and they landed together, because `bitsToInfo` calls `evaluateInfo` at
 * four sites and nothing else in the object calls either.  They are the
 * RECEIVE half: `bitsToInfo` takes one bit at a time, lays it into `bits` and
 * runs the eleven-state detector, and `evaluateInfo` is the per-state decoder
 * that turns a completed block of `bits` back into the message fields.  It is
 * `infoToBits` run backwards, field for field, and reading the two against
 * each other is what settled +0x124 and the state numbering.  Findings
 * F6600-6607.
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

/*
 * THE MESSAGE IS SEVENTEEN-ENTRY GROUPS: one zero followed by sixteen
 * payload entries.  `infoToBits` writes the zero at every index that is a
 * multiple of seventeen -- 17, 34, 51, 68, 85, 102, 119 are all spelled out
 * as constant displacements -- and `calcCRC` skips exactly those indices,
 * `if (i % 17 == 0) i++`.  The first group, indices 0..16, is seventeen ONES
 * and carries no marker.
 */
#define V92CP_GROUP	17

/*
 * The two mask blocks at +0x042 and +0x0a2 hold six groups of eight 16-bit
 * words each, and both numbers are forced rather than modelled.  EIGHT: the
 * outer loop of `setV92CPpckFromParamsInfo` advances the block pointer by
 * `add $0x10,%edi` -- sixteen bytes -- and zeroes eight words with
 * `cmp $0x7,%eax; jbe` before filling them.  SIX: the two blocks abut, and
 * 0x0a2 - 0x042 = 0x60 = six times sixteen; 0x0a2 + 0x60 = 0x102, which is
 * where the next field's alignment padding begins.
 */
#define V92CP_GROUPS	6
#define V92CP_MASKS	8

/*
 * float2Bits(float, unsigned char *, int) -- 0x4ec00, the free function that
 * shares this class's translation unit: the greedy `fltTable_2`/`fltTable_1`
 * expansion standalone, one byte per bit.  The `Psi` sibling in V90CPpck.h
 * packs shorts against the other table pair; the two overload cleanly.
 * Nothing in the object calls this one.
 */
void float2Bits(float f, unsigned char *bits, int mode);

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

	/*
	 * `int`, and deliberately so: the object ends `xor %eax,%eax;
	 * cmpb $0x0,..; sete %al` at .text+0x4ebca, which is a value
	 * constructed for the caller and not a leftover.  Non-zero means the
	 * sixteen computed CRC bits matched the sixteen received ones.
	 */
	int evaluateCRC();
	void evaluateInfo();
	void infoToBits();

	/*
	 * `int`, and for the same reason `evaluateCRC` is: every path through
	 * the object arranges %eax before returning -- `mov %edi,%eax` at all
	 * three `ret`s, with %edi zeroed on entry and set to 1..5 on five
	 * paths -- which is a value constructed for the caller and not a
	 * leftover.  See the source for what the five mean, and for why they
	 * are not named.
	 */
	int bitsToInfo(unsigned char);
	void setSUV(unsigned int);

	/* Public for the same reason as V90Jd's and V90CP's: it keeps the
	 * class standard-layout, so the offsetof assertions are well defined. */

	/*
	 * ===================================================================
	 * +0x000 .. +0x103  THE MESSAGE FIELDS, which `infoToBits` packs into
	 * `bits` and `setV92CPpckFromParamsInfo` fills.
	 *
	 * SHAPE IS MEASURED AND MEANING IS NOT.  Every type below is forced by
	 * an instruction -- a store width, a load's extension whose 32-bit
	 * result is used, or an index stride -- and every one of them is
	 * NAMED BY ITS OFFSET, because nothing written establishes what any of
	 * them holds.  `V92CPUnPck` is a different struct at
	 * `VPcmFloModem+0x254c` whose author-printed names are tempting and
	 * are NOT carried across: the correspondence would be adjacency and
	 * not evidence, and CLAUDE.md's "a wrong name is worse than a pad"
	 * covers exactly that.
	 *
	 * `bitsToInfo` and `evaluateInfo` were named here as the two unwritten
	 * members that might settle it.  THEY ARE NOW WRITTEN AND THEY DO NOT.
	 * `evaluateInfo` is the exact inverse of `infoToBits` -- every field
	 * comes back out of the same bit positions it went in at -- so the
	 * pair proves the LAYOUT twice over and says nothing more about what
	 * any field means than the packer already did.  Finding F6602; the two
	 * fields that DID gain something are +0x114 and +0x124 below.
	 * ===================================================================
	 */

	/* +0x000  `bits[18]`, stored whole rather than masked, and then
	 * tested against ONE -- `dec %al; je` -- to choose between the short
	 * two-group message and everything else. */
	unsigned char byte_00;

	/*
	 * +0x001  SIGNED, and forced: `cmp $0x1,%bl; jle` and `dec %bl; jle`
	 * are signed byte branches where an `unsigned char` would have given
	 * `jbe`, and `sar $1,%al` is an arithmetic shift of the byte.  Two of
	 * its bits go out at `bits[19]` and `bits[20]`, its whole value is
	 * copied to +0x118, and `<= 1` selects the long form of the message.
	 */
	signed char char_01;

	/*
	 * +0x002  SIGNED, and forced the strong way: `movsbl 0x2(%edi),%ecx`
	 * with the 32-bit result shifted arithmetically EIGHTEEN times.
	 * `infoToBits` takes five bits of it into `bits[21..25]` and then --
	 * out of the same register, with no reload -- thirteen more into
	 * `bits[36..48]`.  Those thirteen are the sign extension of a byte;
	 * that is what the object does and it is reproduced.
	 */
	signed char char_02;

	/* +0x003  `bits[35]`, stored whole. */
	unsigned char byte_03;

	/*
	 * +0x004  `bits[33]`, stored whole, and separately non-zero raises
	 * +0x110 at the end of `infoToBits`.  Cleared by the constructor and
	 * NOT by `reset`, which is the whole difference between the two.
	 * THREE MEMBERS OF ANOTHER CLASS ALSO WRITE IT:
	 * `V92Phase4Modulator::recivedCP` and `::recivedPartOneSilenceRrnSUV`
	 * set it to 1 and `::resetRRNSecondSection` clears it, all through the
	 * `V92CP *` that class holds at its own +0x74.  Findings F1282, F4700.
	 */
	unsigned char byte_04;

	unsigned char pad_05[3];	/* +0x005 alignment               */

	/* +0x008  UNSIGNED: `shr $1` on the 32-bit value.  Two bits, and they
	 * go out at `bits[31]` and `bits[32]` -- over the top of two of the
	 * seven zeros already written there -- only when `char_01 <= 1`. */
	unsigned int word_08;

	/* +0x00c  UNSIGNED, same shape: two bits at `bits[49]`, `bits[50]`. */
	unsigned int word_0c;

	/*
	 * +0x010  Sixteen magnitude entries at `bits[52..67]`, weights 4 down
	 * to 2^-13 from `fltTable_2`, MOST significant at the HIGHEST index.
	 * The only one of the five with no sign entry.
	 */
	float flt_10;

	/*
	 * +0x014 .. +0x020  Seven magnitude entries each from `fltTable_1`,
	 * weights 1 down to 2^-6, again most significant at the highest
	 * index, each followed by one entry that is `f < 0`.
	 */
	float flt_14;
	float flt_18;
	float flt_1c;
	float flt_20;

	/*
	 * +0x024  `bits[128]`, stored whole, and separately a GATE: the second
	 * mask block at +0x0a2 is emitted only when it is non-zero.
	 * `setV92CPpckFromParamsInfo` gates the same block on the same byte,
	 * `cmpb $0x0,0x24(%eax); je`.
	 */
	unsigned char byte_24;

	unsigned char pad_25[3];	/* +0x025 alignment               */

	/*
	 * +0x028 .. +0x03f  Six four-byte entries.  FOUR BYTES is forced by
	 * `setV92CPpckFromParamsInfo`, which zeroes +0x28 with `movl $0x0` and
	 * indexes the block `(%edi,%ecx,4)` under `cmpl $0x5`; `infoToBits`
	 * agrees on the stride, `0x28(%edi,%ebp,4)`.  Only FOUR BITS of each
	 * reach the message, which is why the load there is a 16-bit one.
	 *
	 * The six are emitted in two runs because a group boundary falls
	 * between them: entries 0..3 at `bits[103..118]` and entries 4..5 at
	 * `bits[120..127]`, with the marker at 119 in between.
	 */
	int word_28[V92CP_GROUPS];

	unsigned char pad_40[2];	/* +0x040 alignment               */

	/*
	 * +0x042 and +0x0a2  The two mask blocks, `V92CP_GROUPS` groups of
	 * `V92CP_MASKS` words.  `infoToBits` sends `word_10c` groups of eight,
	 * each word as its own seventeen-entry group -- a zero and then all
	 * sixteen bits, least significant first.
	 *
	 * `short` is the reader's type and not the writer's: `infoToBits`
	 * loads `movswl`, and `setV92CPpckFromParamsInfo` loads `movzwl` to
	 * OR a bit in.  Sixteen bits are extracted either way, so nothing
	 * observable turns on it and neither reading is preferred here.
	 */
	short short_42[V92CP_GROUPS][V92CP_MASKS];
	short short_a2[V92CP_GROUPS][V92CP_MASKS];

	unsigned char pad_102[2];	/* +0x102 alignment               */

	/* +0x104  `setSUV` stores 16 here before storing its argument, as a
	 * four-byte store; `infoToBits` reads the low half of it, sign
	 * extended, and sends five bits of it at `bits[27..31]`. */
	unsigned int word_104;

	/* +0x108  `setSUV`'s argument.  `infoToBits` sends its low BYTE whole
	 * at `bits[32]`. */
	unsigned int suv;

	/*
	 * +0x10c  UNSIGNED and SIXTEEN BITS, both forced:
	 * `setV92CPpckFromParamsInfo` stores it `mov %dx,0x10c(%edi)` and both
	 * functions load it `movzwl`.  It is the number of groups of eight
	 * that `infoToBits` takes out of each mask block, and NOTHING BOUNDS
	 * IT -- the blocks hold six and the loop trusts the field.  See
	 * docs/deviations.md.
	 */
	unsigned short word_10c;

	unsigned char pad_10e[2];	/* +0x10e alignment               */

	/*
	 * +0x110  `infoToBits` raises it to 1 when `byte_04` is non-zero, and
	 * that is the only writer inside this class.  Outside it, only
	 * V92Phase4Modulator writes it: its constructor clears it
	 * (`mov %edi,0x74(%ebx); mov %esi,0x110(%edi)` at .text+0x179ed with
	 * %esi zero), and so do `recivedCP`, `resetBeforRRN` and
	 * `resetRRNSecondSection`.
	 *
	 * `V92Phase4Modulator::recivedSUVtag` is the only READER written, and
	 * it requires the word non-zero before it will take a transition.  A
	 * four-byte store, so a word; what it counts is still not
	 * established.  Named out of `pad_10c` by finding F1282; the extra
	 * writers are finding F4700's batch.
	 */
	unsigned int word_110;

	/*
	 * +0x114  THE STATE, and both members that use it agree on it.
	 * `bitsToInfo` dispatches on it with `cmp $0xa; ja` over an
	 * eleven-entry table, so the receive states are 0..10 and every other
	 * value does nothing; `evaluateInfo` dispatches with `sub $0x3;
	 * cmp $0x5; ja` over a six-entry table whose second slot is the bare
	 * `ret`, so it decodes 3, 5, 6, 7 and 8 and 4 is a HOLE in the case
	 * list rather than an arm that does nothing.  That is the same shape
	 * V90CP's `word_ca4` has, read the same way, and the two machines line
	 * up: `bitsToInfo`'s state N fills a block of `bits` and then calls
	 * `evaluateInfo`, which is still in state N when it decodes it.
	 *
	 * UNSIGNED is forced by both dispatches: the range checks are `ja`.
	 * Zeroed by `resetDetector`, and so by `reset` and the constructor,
	 * which is state 0 -- waiting for the seventeen-one preamble.
	 */
	unsigned int rxState;

	/* +0x118  `infoToBits` copies `char_01` here whole, and nothing
	 * written reads it. */
	unsigned char byte_118;

	/*
	 * +0x119  THE RUN OF ONES.  `bitsToInfo` raises it by one on every
	 * `1` and clears it on every `0`, as an eight-bit `inc %al` that wraps
	 * at 255, and state 0 leaves for state 1 once it passes sixteen --
	 * which is the seventeen-one preamble `infoToBits` opens with.
	 * Cleared by `resetDetector`.
	 */
	unsigned char byte_119;

	/*
	 * +0x11a  THE RUN OF ZEROS, the other half of the same pair: raised on
	 * every `0` and cleared on every `1`.  `12 * bitsPerSymbol` of them
	 * arriving while `word_11c` is still at its home 18 is `bitsToInfo`'s
	 * answer 5, and that answer does NOT stop the state machine, which
	 * runs on afterwards and can overwrite it.
	 *
	 * IT IS READ BACK OUT OF THE OBJECT rather than out of a register --
	 * the object stores 0 and reloads it four instructions later -- which
	 * matters when `bitsPerSymbol` is zero, because the quantum is then
	 * zero and the test is true on a ONE bit as well.  Reproduced, and it
	 * is the same shape V90CP's `byte_caa` has.  Cleared by
	 * `resetDetector`.
	 */
	unsigned char byte_11a;

	unsigned char pad_11b[1];	/* +0x11b alignment               */

	/*
	 * +0x11c  Set to 18 by `resetDetector`.  In `infoToBits` it is the
	 * WRITE CURSOR into `bits`: set to 34 once the first two groups are
	 * out, then advanced by seventeen for every group after that, and
	 * finally left one past the message's closing marker.  Eighteen is
	 * the first payload index, so the detector's seed and the packer's
	 * cursor are the same quantity counted from the same place.
	 *
	 * AND THE RECEIVE SIDE DOES MEAN THE SAME THING BY IT.  This used to
	 * say that `bitsToInfo` and `evaluateInfo` would settle that and were
	 * not written.  They are, and they do: `bitsToInfo` stores each
	 * arriving bit at `bits[word_11c]` and advances it by one, from the
	 * same 18 `resetDetector` seeds, and stops each block at the same
	 * absolute index `infoToBits` writes it at -- 34 for the header, 136
	 * for the fixed part.  Kept neutral all the same, because what it
	 * counts is still an index into `bits` and nothing names it.
	 */
	int word_11c;

	/*
	 * +0x120  THE BITS TAKEN SO FAR IN THIS STATE, and it is a different
	 * quantity from `word_11c`: `bitsToInfo` clears it at every state
	 * change and compares it against the length of the block the state is
	 * collecting -- 17 for the CRC, `gamma` and `delta` for the two
	 * variable-length mask blocks.  Zeroed by `resetDetector`.
	 */
	int stateBitCount;

	/*
	 * +0x124  THE READ CURSOR, and the counterpart of `word_11c`.
	 * `evaluateInfo` is the only member that touches it: every arm walks
	 * it forward through `bits` -- `movzbl 0x129(%eax,%edi,1)` with %eax
	 * loaded from here and the incremented value stored straight back --
	 * and leaves it on the block boundary the next arm starts from.  The
	 * arms that are not the first also SET it outright: state 6 stores 52,
	 * which is the first magnitude bit `infoToBits` writes.
	 *
	 * FOUR BYTES is forced by the `incl` and by the 32-bit loads; the
	 * SIGNEDNESS is not, because every use is either an index into `bits`
	 * or an increment and neither reading differs over any value it holds.
	 * Spelled `int` to match `word_11c` and `stateBitCount`, the two cursors
	 * beside it, and not because anything measures it.
	 *
	 * It is NOT reset by `resetDetector`, where `word_11c` and `stateBitCount`
	 * both are, so a detector restart leaves it where the last decode left
	 * it.  Every arm that reads a variable-length block sets it first, so
	 * nothing written depends on that -- but it is the object's own
	 * asymmetry and it is reproduced.  Was `pad_124`; finding F6601.
	 */
	int word_124;

	/*
	 * +0x128  HOW MANY BITS GO INTO ONE SYMBOL, and the name is the
	 * CALLER'S rather than an inference from arithmetic:
	 * `V92Phase4Modulator::recivedRt` assigns it from that class's own
	 * `bitsPerSymbol` at its +0x43 -- `movzbl 0x43(%ebx),%eax; mov
	 * %al,0x128(%edx)` at .text+0x177e6 -- and +0x43 was named from the
	 * loop bound of `generateCPu`/`generateSUVu` and the count handed to
	 * `Scrambler<h,h>::processAllOnes`.  That is CLAUDE.md's second
	 * evidence tier, a callee or caller that types the field.
	 *
	 * It is consistent with what `infoToBits` does with it: the padded
	 * length is rounded up to a multiple of `12 * bitsPerSymbol` --
	 * `lea (%ebx,%ebx,2),%edx; lea 0x0(,%edx,4)` -- which is a whole
	 * number of twelve-symbol frames, and five members of
	 * V92Phase4Modulator then divide `vectorLen` by it to get a count in
	 * SYMBOLS.  What the twelve counts is still not established.
	 *
	 * It is also the divisor of an unsigned `div`, so a zero here divides
	 * by zero in the object as well as in ours; `infoToBits` stores 1
	 * when `char_01` is zero, and V92Phase4Modulator writes it at five
	 * sites.
	 */
	unsigned char bitsPerSymbol;

	/* +0x129  The bit vector, one byte per bit.  See V92CP_BITS. */
	unsigned char bits[V92CP_BITS];

	/* +0x8f9  The CRC register, one byte per bit; `resetCRC` sets all. */
	unsigned char crc[V92CP_CRC];

	unsigned char pad_909[3];	/* +0x909 alignment               */

	/*
	 * +0x90c  THE PADDED LENGTH, and what `getBitVector` reports.
	 * `infoToBits` computes it as the next multiple of `12 * bitsPerSymbol`
	 * STRICTLY GREATER than the message -- `n / q + 1` times `q`, so an
	 * exact multiple still gains a whole quantum -- and zero-fills
	 * `bits[msgLen + 1 .. vectorLen)` up to it.  Named with `msgLen`; see
	 * there for why the pair could not be named before.
	 */
	unsigned int vectorLen;

	/*
	 * +0x910  THE MESSAGE LENGTH, its sixteen CRC entries included and its
	 * padding excluded.  `infoToBits` sets it to one past the last CRC
	 * entry, and every other user agrees with that reading:
	 *
	 *   - `calcCRC` clocks over `bits[18 .. msgLen - 17)`, which stops
	 *     just before the marker that precedes the CRC.
	 *   - `evaluateCRC` finds the received CRC at `bits[msgLen - 16 ..
	 *     msgLen)`, so the last sixteen entries of the message are it.
	 *   - `infoToBits` writes the marker at `msgLen`, the padding from
	 *     `msgLen + 1`, and `vectorLen` above that.
	 *
	 * WHY IT COULD NOT BE NAMED BEFORE, and what settled it.  This was
	 * `word_910` for as long as the class held two lengths and only
	 * READERS of them: `calcCRC` and `evaluateCRC` forced the shape --
	 * an unsigned index bound into `bits` -- without saying which of the
	 * two lengths was the message and which the buffer, and naming one of
	 * two indistinguishable lengths is the guess CLAUDE.md calls worse
	 * than a pad.  `infoToBits` is the WRITER of both, in the same eight
	 * instructions, and the arithmetic between them is one-directional:
	 * +0x910 is the message and +0x90c is +0x910 rounded up and
	 * zero-filled.  Finding F4750.
	 */
	unsigned int msgLen;

	/*
	 * +0x914  A HOLD-OFF COUNTER over `bitsToInfo`'s answer, and -1 is its
	 * idle value -- which is why `reset` and the constructor set it there
	 * rather than to zero.  The epilogue of `bitsToInfo` is, in full:
	 *
	 *     if (word_914 >= 0) {
	 *             if (rc == 3 || rc == 4)   rc = 0;
	 *             if (++word_914 == 400)    word_914 = -1;
	 *     } else if (rc == 1 || rc == 2) {
	 *             word_914 = 0;
	 *     }
	 *
	 * so answers 1 and 2 start it, and while it runs -- 400 calls, one per
	 * bit -- answers 3 and 4 are suppressed and 5 is not.  The 400 is an
	 * immediate, `cmp $0x190`, and nothing in the object says what it
	 * counts; the four answers are unnamed for the reason `bitsToInfo`
	 * gives.  THE FIELD KEEPS ITS OFFSET NAME for that reason -- the
	 * derivation above is usage inference, CLAUDE.md's weakest tier, and a
	 * name like `holdoff` would be believed by every future reader on the
	 * strength of one block of arithmetic.  Finding F6606.
	 */
	int word_914;
};

#endif /* DSPLIB_V92CP_H */
