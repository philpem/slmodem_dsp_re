/**
 * @file V92CP.h
 * @brief ITU-T V.92 CP (Call Progress/parameter-exchange) message:
 *        `V92CP`, which packs a set of negotiated parameters into a bit
 *        vector for transmission (`infoToBits`) and decodes a received one
 *        back into fields (`bitsToInfo`/`evaluateInfo`).
 *
 * Not polymorphic (no vptr at offset 0: `tools/cppstruct.py` shows only the
 * two ordinary destructor variants, and GCC emits a deleting destructor only
 * for a virtual one). `sizeof == 0x918` is the allocation `V92Modem::V92Modem`
 * makes before calling the constructor, matching the last field's end.
 *
 * The layout below +0x104 comes from `infoToBits`, which packs the whole
 * struct into `bits`, and `setV92CPpckFromParamsInfo`, which fills it; the
 * rest came from the other members, each proving the few fields it touches
 * (finding F6600 has the per-member map). `bitsToInfo` and `evaluateInfo` are
 * the receive half and were the last two written: `bitsToInfo` takes one bit
 * at a time, lays it into `bits`, and runs an eleven-state detector, and
 * `evaluateInfo` is the per-state decoder that turns a completed block of
 * `bits` back into the message fields -- `infoToBits` run backwards, field
 * for field. Reading the two against each other settled `word_124` and the
 * state numbering. Findings F6600-F6607.
 *
 * The destructor is a bare `ret`, not an assumption about an empty class:
 * nothing here is allocated, unlike the V.90 sibling with the same shape
 * (`V90CP`), which frees the six buffers its constructor allocates.
 *
 * `V92CP::bitsToInfo(unsigned char)::gamma` and `::delta` are function-local
 * statics in .bss, so `bitsToInfo` carries state across calls. Nothing here
 * depends on that.
 */

#ifndef DSPLIB_V92CP_H
#define DSPLIB_V92CP_H

/*
 * The bit vector's extent.  The start is proven -- `getBitVector` hands back
 * `this+0x129` -- and the end is where the CRC register begins; that the
 * whole span is one array is a modelling choice, since no method establishes
 * the array's own length. Confirmed: over all twelve of the class's symbols,
 * every `this`-relative displacement between +0x129 and +0x8f9 lies below
 * +0x1aa, so nothing else lives in the span at an offset of its own.
 */
#define V92CP_BITS	0x7d0		/* 0x129 .. 0x8f8, 2000 bytes */

/* The CRC register: sixteen bytes, one per bit, `resetCRC` sets them all. */
#define V92CP_CRC	16

/*
 * The message is seventeen-entry groups: one framing zero followed by
 * sixteen payload entries. `infoToBits` writes the zero at every index that
 * is a multiple of seventeen, and `calcCRC` skips exactly those indices
 * (`if (i % 17 == 0) i++`). The first group, indices 0..16, is seventeen
 * ones and carries no marker.
 */
#define V92CP_GROUP	17

/*
 * The two mask blocks at +0x042 and +0x0a2 hold `V92CP_GROUPS` groups of
 * `V92CP_MASKS` 16-bit words each. Eight: `setV92CPpckFromParamsInfo`'s outer
 * loop advances the block pointer by sixteen bytes and zero-fills eight
 * words before filling them. Six: the two blocks abut (0x0a2 - 0x042 is six
 * times sixteen) and the next field's alignment padding begins right after
 * the second block.
 */
#define V92CP_GROUPS	6
#define V92CP_MASKS	8

/**
 * @brief Free function sharing `V92CP`'s translation unit: expand a
 *        magnitude/sign pair to one byte per bit, greedily, against the
 *        `fltTable_2`/`fltTable_1` weight tables. Mode 0 is the 16-entry
 *        unsigned Q3.13 form, mode 1 the 7-entry signed Q1.6 form (bit 7 is
 *        the sign). Not called anywhere in the object; the `Psi` sibling in
 *        V90CPpck.h packs the equivalent expansion into `short`s against a
 *        different table pair.
 * @param f     Value to expand.
 * @param bits  Destination, one byte per bit.
 * @param mode  0 for the unsigned 16-bit form, 1 for the signed 7-bit form;
 *              any other value writes nothing.
 */
void float2Bits(float f, unsigned char *bits, int mode);

class V92CP {
public:
	/**
	 * @brief Construct an idle CP message: clears `byte_04`, resets the
	 *        receive detector's state (`rxState`, both run counters and
	 *        the cursor `bitIndex`), and idles the hold-off counter
	 *        (`word_914 = -1`). Does not clear the message fields.
	 */
	V92CP();

	/**
	 * @brief Destroy the CP message. Frees nothing -- the class owns no
	 *        allocated storage (contrast `V90CP`, whose destructor frees
	 *        six buffers).
	 */
	~V92CP();

	/**
	 * @brief Get the packed bit vector produced by infoToBits(), one byte
	 *        per bit.
	 * @param length  Set to the vector's length (`vectorLen`), including
	 *                its zero padding.
	 * @return Pointer to `bits`.
	 */
	unsigned char *getBitVector(unsigned int &length);

	/**
	 * @brief Reset the message to its just-constructed state: resetDetector()
	 *        plus idling the hold-off counter. Does not clear `byte_04`,
	 *        which only the constructor touches.
	 */
	void reset();

	/**
	 * @brief Reset the receive detector's state machine (`rxState`, both
	 *        run counters `onesRun`/`zerosRun`, the write cursor
	 *        `bitIndex`, and `stateBitCount`) without touching the hold-off
	 *        counter or the message fields already decoded.
	 */
	void resetDetector();

	/**
	 * @brief Set all sixteen bits of the CRC shift register to one (the
	 *        CCITT convention), not to zero.
	 */
	void resetCRC();

	/**
	 * @brief Clock the CRC shift register (taps at stages 3, 10 and 15)
	 *        over `bits[18 .. msgLen - 17)`, skipping the framing zero at
	 *        every seventeenth position.
	 */
	void calcCRC();

	/**
	 * @brief Check a received message's CRC: reset the register, clock it
	 *        over the message, and sum the absolute per-bit differences
	 *        against the sixteen received CRC bits at the message's end.
	 * @return Non-zero if the computed and received CRC agree.
	 */
	int evaluateCRC();

	/**
	 * @brief Decode one completed block of `bits` into the message fields,
	 *        for the block `rxState` says has just arrived. The exact
	 *        inverse of infoToBits(), field for field; `word_124` is the
	 *        read cursor and this is its only user.
	 */
	void evaluateInfo();

	/**
	 * @brief Pack the message fields at +0x000..+0x10c into `bits`, one
	 *        byte per bit, followed by a CRC and zero padding out to a
	 *        whole number of `12 * bitsPerSymbol`-bit frames. Sets
	 *        `msgLen` and `vectorLen`.
	 */
	void infoToBits();

	/**
	 * @brief Feed one received bit through the detector/state machine that
	 *        drives evaluateInfo(). Advances `rxState` and the run
	 *        counters, decodes each completed block as it arrives, and
	 *        checks the CRC at the end of the message.
	 * @param bit  The received bit (0 or 1).
	 * @return 0 while no message boundary has been reached; on a boundary,
	 *         1-4 encode the two bits (`byte_00`, `byte_04`) that survive
	 *         the whole message, and 5 means a run of `12 * bitsPerSymbol`
	 *         zeros arrived with nothing yet collected -- what exactly
	 *         each value means downstream is not established.
	 */
	int bitsToInfo(unsigned char bit);

	/**
	 * @brief Set `suv` (and unconditionally reset `word_104` to 16, whose
	 *        role is not established).
	 * @param value New value of `suv`.
	 */
	void setSUV(unsigned int value);

	/* Public for the same reason as V90Jd's and V90CP's: it keeps the
	 * class standard-layout, so the offsetof assertions are well defined. */

	/*
	 * ===================================================================
	 * +0x000 .. +0x103  THE MESSAGE FIELDS, which `infoToBits` packs into
	 * `bits` and `setV92CPpckFromParamsInfo` fills.
	 *
	 * SHAPE AND ROLE ARE MEASURED.  Every type below is forced by an
	 * instruction -- a store width, a load's extension whose 32-bit result
	 * is used, or an index stride -- and the names are the object's own:
	 * `setV92CPpckFromParamsInfo` (V90MappingParamsInt.cpp) copies these
	 * fields directly from author-named `V90MappingParams` members --
	 * `shaperSR`, `shaperId`, `shaperA1/A2/B1/B2` -- so a destination
	 * holding a source's value is that source, not an adjacency guess.
	 * `dataBitRate` is the rate the same file hands
	 * `setDataBitRateInline`/`getDataBitRate`, and the mask blocks are
	 * filled by the object's own `getConstellationMask`,
	 * `getCodecConstellationMask` and `getConstellationsIndex`, whose
	 * names state the roles directly.
	 *
	 * THE OFFSET-ONLY STANCE THAT STOOD HERE IS SUPERSEDED.  It read shape
	 * from instructions and refused to name anything because nothing
	 * written established what the fields held; `setV92CPpckFromParamsInfo`
	 * and that family of symbols are what settled it.  The retained
	 * offset-named fields (`byte_00`, `char_01`, `byte_03`, `byte_04`,
	 * `flt_10`, `byte_24`, `word_104`, `word_10c`, `word_110`, `byte_118`,
	 * `word_124`, `word_914`) keep their offset names because their role
	 * is still unstated.
	 *
	 * `V92CPUnPck` is a different struct at `VPcmFloModem+0x254c` whose
	 * author-printed names are tempting and are NOT carried across: the
	 * correspondence would be adjacency and not evidence.  The names above
	 * come from the direct copies, not from that struct.
	 *
	 * `bitsToInfo` and `evaluateInfo` were named here as the two unwritten
	 * members that might settle the layout.  THEY ARE NOW WRITTEN, and
	 * `evaluateInfo` is the exact inverse of `infoToBits` -- every field
	 * comes back out of the same bit positions it went in at -- so the
	 * pair proves the LAYOUT twice over and says nothing more about what
	 * any field means than the packer already did.  Finding F6602; the two
	 * fields that DID gain something from that pair are +0x114 and +0x124
	 * below.
	 * ===================================================================
	 */

	/* +0x000  `bits[18]`, stored whole rather than masked, and then
	 * tested against ONE -- `dec %al; je` -- to choose between the short
	 * two-group message and everything else. */
	unsigned char byte_00;

	/*
	 * +0x001  Signed, and forced: `cmp $0x1,%bl; jle` and `dec %bl; jle`
	 * are signed byte branches where an `unsigned char` would have given
	 * `jbe`, and `sar $1,%al` is an arithmetic shift of the byte. Two of
	 * its bits go out at `bits[19]` and `bits[20]`, its whole value is
	 * copied to +0x118, and `<= 1` selects the long form of the message.
	 */
	signed char char_01;

	/*
	 * +0x002  Signed, and forced the strong way: `movsbl 0x2(%edi),%ecx`
	 * with the 32-bit result shifted arithmetically eighteen times.
	 * `infoToBits` takes five bits of it into `bits[21..25]` and then --
	 * out of the same register, with no reload -- thirteen more into
	 * `bits[36..48]`. Those thirteen are the sign extension of a byte;
	 * that is what the object does and it is reproduced.
	 */
	signed char dataBitRate;

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

	/*
	 * +0x005..+0x007 was `pad_05[3]`: `byte_04` ends at +0x005 and
	 * `shaperSR` below is a 4-byte-aligned `unsigned int`, so natural
	 * alignment inserts exactly these three bytes with the member deleted
	 * -- the existing `V92CP_OFF(shaperSR, 0x008, word08)` (V92CP.cpp) is
	 * what proves it. Zero readers/writers anywhere in the object
	 * (F10142); removed F10150.
	 */

	/* +0x008  UNSIGNED: `shr $1` on the 32-bit value.  Two bits, and they
	 * go out at `bits[31]` and `bits[32]` -- over the top of two of the
	 * seven zeros already written there -- only when `char_01 <= 1`. */
	unsigned int shaperSR;

	/* +0x00c  UNSIGNED, same shape: two bits at `bits[49]`, `bits[50]`. */
	unsigned int shaperId;

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
	float shaperA1;
	float shaperA2;
	float shaperB1;
	float shaperB2;

	/*
	 * +0x024  `bits[128]`, stored whole, and separately a GATE: the second
	 * mask block at +0x0a2 is emitted only when it is non-zero.
	 * `setV92CPpckFromParamsInfo` gates the same block on the same byte,
	 * `cmpb $0x0,0x24(%eax); je`.
	 */
	unsigned char byte_24;

	/*
	 * +0x025..+0x027 was `pad_25[3]`: `byte_24` ends at +0x025 and
	 * `distinctIndex` below is a 4-byte-aligned `int[]`, so natural alignment
	 * inserts exactly these three bytes with the member deleted -- the
	 * existing `V92CP_OFF(distinctIndex, 0x028, word28)` (V92CP.cpp) is what
	 * proves it. Zero readers/writers anywhere in the object (F10142);
	 * removed F10150.
	 */

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
	int distinctIndex[V92CP_GROUPS];

	/*
	 * +0x040  NOT REMOVABLE under the pad-removal workstream (F10150):
	 * `constellationMask` below needs only 2-byte alignment and +0x040 is already
	 * 4-byte (and so 2-byte) aligned, so a field-to-field gap here would
	 * be 0 bytes, not 2, if the member vanished -- the compiler's own
	 * implicit padding does NOT reproduce this span.  Confirmed dead (zero
	 * readers/writers anywhere in the object, F10142) but not
	 * alignment-driven, same shape as `VPcmFloModem::pad_6fb8` -- stays
	 * explicit.
	 */
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
	short constellationMask[V92CP_GROUPS][V92CP_MASKS];
	short codecConstellationMask[V92CP_GROUPS][V92CP_MASKS];

	/*
	 * +0x102..+0x103 was `pad_102[2]`: `codecConstellationMask` ends at +0x102 and
	 * `word_104` below is a 4-byte-aligned `unsigned int`, so natural
	 * alignment inserts exactly these two bytes with the member deleted
	 * -- the existing `V92CP_OFF(word_104, 0x104, word104)` (V92CP.cpp) is
	 * what proves it. Zero readers/writers anywhere in the object
	 * (F10142); removed F10150.
	 */

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

	/*
	 * +0x10e..+0x10f was `pad_10e[2]`: `word_10c` ends at +0x10e and
	 * `word_110` below is a 4-byte-aligned `unsigned int`, so natural
	 * alignment inserts exactly these two bytes with the member deleted
	 * -- the existing `V92CP_OFF(word_110, 0x110, word110)` (V92CP.cpp) is
	 * what proves it. Zero readers/writers anywhere in the object
	 * (F10142); removed F10150.
	 */

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
	 * +0x114  The receive detector's own state, shared by both members
	 * that dispatch on it: `bitsToInfo` runs states 0..10, and
	 * `evaluateInfo` decodes 3, 5, 6, 7 and 8, with 4 a hole in its case
	 * list rather than an arm that does nothing (the same shape V90CP's
	 * `rxState` has). The two machines line up: `bitsToInfo`'s state N
	 * fills a block of `bits` and then calls `evaluateInfo`, which is
	 * still in state N when it decodes it. Unsigned is forced by both
	 * dispatches' range checks (`ja`). Zeroed by `resetDetector` (and so
	 * by `reset` and the constructor) to state 0, waiting for the
	 * seventeen-one preamble. Renamed from `word_114`; finding F10130.
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
	unsigned char onesRun;

	/*
	 * +0x11a  THE RUN OF ZEROS, the other half of the same pair: raised on
	 * every `0` and cleared on every `1`.  `12 * bitsPerSymbol` of them
	 * arriving while `bitIndex` is still at its home 18 is `bitsToInfo`'s
	 * answer 5, and that answer does NOT stop the state machine, which
	 * runs on afterwards and can overwrite it.
	 *
	 * IT IS READ BACK OUT OF THE OBJECT rather than out of a register --
	 * the object stores 0 and reloads it four instructions later -- which
	 * matters when `bitsPerSymbol` is zero, because the quantum is then
	 * zero and the test is true on a ONE bit as well.  Reproduced, and it
	 * is the same shape V90CP's `zerosRun` has.  Cleared by
	 * `resetDetector`.
	 */
	unsigned char zerosRun;

	/*
	 * +0x11b was `pad_11b[1]`: `zerosRun` ends at +0x11b and `bitIndex`
	 * below is a 4-byte-aligned `int`, so natural alignment inserts
	 * exactly this one byte with the member deleted -- the existing
	 * `V92CP_OFF(bitIndex, 0x11c, word11c)` (V92CP.cpp) is what proves it.
	 * Zero readers/writers anywhere in the object (F10142); removed
	 * F10150.
	 */

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
	 * arriving bit at `bits[bitIndex]` and advances it by one, from the
	 * same 18 `resetDetector` seeds, and stops each block at the same
	 * absolute index `infoToBits` writes it at -- 34 for the header, 136
	 * for the fixed part.  The sibling `V90CP`/`V90MP` calls this same
	 * write cursor `bitIndex`, so this field follows the sibling.
	 */
	int bitIndex;

	/*
	 * +0x120  Bits taken so far in the current state -- a different
	 * quantity from `bitIndex`: `bitsToInfo` clears it at every state
	 * change and compares it against the length of the block the state is
	 * collecting (17 for the CRC, `gamma` and `delta` for the two
	 * variable-length mask blocks). Zeroed by `resetDetector`. Renamed
	 * from `word_120`; finding F10130.
	 */
	int stateBitCount;

	/*
	 * +0x124  THE READ CURSOR, and the counterpart of `bitIndex`.
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
	 * Spelled `int` to match `bitIndex` and `stateBitCount`, the two cursors
	 * beside it, and not because anything measures it.
	 *
	 * It is NOT reset by `resetDetector`, where `bitIndex` and `stateBitCount`
	 * both are, so a detector restart leaves it where the last decode left
	 * it.  Every arm that reads a variable-length block sets it first, so
	 * nothing written depends on that -- but it is the object's own
	 * asymmetry and it is reproduced.  Was `pad_124`; finding F6601.
	 */
	int word_124;

	/*
	 * +0x128  How many bits go into one symbol. Named from the caller,
	 * not from arithmetic: `V92Phase4Modulator::recivedRt` assigns it
	 * from that class's own `bitsPerSymbol` field (finding F4755).
	 *
	 * `infoToBits` rounds the padded length up to a whole number of
	 * `12 * bitsPerSymbol`-bit frames, and five members of
	 * V92Phase4Modulator then divide `vectorLen` by it to get a count in
	 * symbols; what the twelve counts is still not established. It is
	 * also the divisor of an unsigned `div`, so a zero here divides by
	 * zero in the object as well as in ours -- `infoToBits` stores 1
	 * when `char_01` is zero, and V92Phase4Modulator writes it at five
	 * sites.
	 */
	unsigned char bitsPerSymbol;

	/* +0x129  The bit vector, one byte per bit.  See V92CP_BITS. */
	unsigned char bits[V92CP_BITS];

	/* +0x8f9  The CRC register, one byte per bit; `resetCRC` sets all. */
	unsigned char crc[V92CP_CRC];

	/*
	 * +0x909..+0x90b was `pad_909[3]`: `crc` ends at +0x909 and
	 * `vectorLen` below is a 4-byte-aligned `unsigned int`, so natural
	 * alignment inserts exactly these three bytes with the member deleted
	 * -- the existing `V92CP_OFF(vectorLen, 0x90c, vectorlen)` (V92CP.cpp)
	 * is what proves it. Zero readers/writers anywhere in the object
	 * (F10142); removed F10150.
	 */

	/*
	 * +0x90c  The padded length, and what `getBitVector` reports.
	 * `infoToBits` computes it as the next multiple of `12 * bitsPerSymbol`
	 * strictly greater than the message, and zero-fills
	 * `bits[msgLen + 1 .. vectorLen)` up to it.
	 */
	unsigned int vectorLen;

	/*
	 * +0x910  The message length, its sixteen CRC entries included and its
	 * padding excluded. `infoToBits` sets it to one past the last CRC
	 * entry, and every other user agrees with that reading:
	 *
	 *   - `calcCRC` clocks over `bits[18 .. msgLen - 17)`, which stops
	 *     just before the marker that precedes the CRC.
	 *   - `evaluateCRC` finds the received CRC at `bits[msgLen - 16 ..
	 *     msgLen)`, so the last sixteen entries of the message are it.
	 *   - `infoToBits` writes the marker at `msgLen`, the padding from
	 *     `msgLen + 1`, and `vectorLen` above that.
	 *
	 * Settled by `infoToBits`, the only writer of both lengths in the same
	 * eight instructions -- the readers alone (`calcCRC`, `evaluateCRC`)
	 * could not tell which of the two indistinguishable lengths was the
	 * message and which was the padded buffer. Finding F4750.
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
