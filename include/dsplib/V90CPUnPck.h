/*
 * V90CPUnPck.h -- the unpacked V.90 CP message, as
 * `setParamsInfoFromCPUnPck` reads it.
 *
 * THE NAME IS THE FUNCTION'S OWN WORD, exactly as `V92CPUnPck.h` derives its
 * own: nothing in the object mangles this type -- the reader is `extern "C"`
 * -- so `V90CPUnPck` is taken from the only place the author wrote anything
 * about it, the name of the function that consumes it.
 *
 * ===========================================================================
 * ONE READER, AND IT HAS NO CALLER.  `setParamsInfoFromCPUnPck` (.text
 * +0x336b0, 622 bytes) is named by ZERO relocations of any kind in the whole
 * 1.2 MB object, so this layout rests on that ONE function's displacements
 * and on nothing else.  There is no second reader to cross-check it, no
 * writer to bound the fields it does not touch, and no `sizeof` anywhere.
 * Everything below says which of the three ranks of evidence in CLAUDE.md it
 * came from, and the unmodelled spans are `pad_*` rather than guesses.
 * ===========================================================================
 *
 * IT IS NOT A `V90CP`, AND THAT WAS CHECKED RATHER THAN ASSUMED.  The name
 * invites the reading, and the sibling `setParamsInfoFromV92CPUnPck`
 * (.text+0x33c60) really does take a `V92CP` -- its displacements are
 * `V92CP`'s `char_01`, `byte_2`, `byte_24`, `word_28`, `short_42` and
 * `short_a2`, field for field.  This one is not the same shape:
 *
 *   - `V90CP+0x014` is sixteen bits read `movzwl`; this function reads a full
 *     32-bit word there (`mov 0x14(%ecx),%edx` at .text+0x338f2).
 *   - `V90CP+0x018` is six frames of two eight-bit values on an 8-byte
 *     stride; this function reads six consecutive 32-bit words from +0x18.
 *   - THE DECISIVE ONE: this function loads a POINTER from +0x0fc and
 *     dereferences its +0x04 (`mov 0xfc(%ecx),%ebp ; mov 0x4(%ebp),%esi`).
 *     `V90CP+0x0fc` is in the middle of the 0x300-byte short array at
 *     +0x058, so on a `V90CP` that load takes two coefficient shorts and
 *     dereferences them.  A structure cannot be both.
 *
 * WHAT IT IS INSTEAD is the V.90 counterpart of `V92CP` for this one
 * purpose: the same six spectral words, the same "which constellations are
 * distinct" list, and the same pair of occupancy bitmaps, in the V.90
 * message's own widths.  The correspondence is one-to-one and is the reason
 * most of the names below are not invented:
 *
 *       V92CP (setParamsInfoFromV92CPUnPck)   this (setParamsInfoFromCPUnPck)
 *       ----------------------------------    ------------------------------
 *       char_01        the long-form flag     info->word_04, through a POINTER
 *       byte_2         signed char            dataBitRate, a 32-bit word
 *       +0x08..+0x20   six spectral words     +0x18..+0x2c, six spectral words
 *       byte_24        the codec gate         codecConstellationPresent
 *       +0x28  int[6]  the distinct list      +0x31  unsigned char[6]
 *       +0x42  the constellation bitmaps      +0x3a
 *       +0xa2  the codec bitmaps              +0x9c
 *
 * AND THE TWO ARRAYS DO NOT ABUT HERE, WHERE THEY DO IN THE V.92 TWIN.
 * 0xa2 - 0x42 is 0x60, which is exactly 6 * 16, so `V92CP`'s two bitmap
 * blocks are adjacent.  0x9c - 0x3a is 0x62, so this one has TWO BYTES
 * between them that nothing reads.  They are `pad_9a` below and are not
 * alignment: two `short` arrays need none between them, and the trailing
 * pointer at +0x0fc would be aligned either way (0x3a + 0x60 = 0x9a, and
 * 0x9a + 0x60 = 0xfa, which would pad to 0xfc regardless).  So there is
 * something declared there, and this header does not know what.
 *
 * SIX ENTRIES IN EACH BITMAP ARRAY IS INFERENCE AND NOT A MEASUREMENT.  The
 * index is `distinctIndex[i]`, an unbounded `unsigned char` that the function
 * neither masks nor checks, so the reads themselves bound nothing.  Six comes
 * from two places: the V.92 twin's arrays are exactly 6 * 16 bytes apart, and
 * the destination `V90MappingParams` has six of everything.  A caller that
 * put 6 or more in `distinctIndex` would read past the array and this
 * function would not notice; that is the object's and is reproduced.
 */

#ifndef DSPLIB_V90CPUNPCK_H
#define DSPLIB_V90CPUNPCK_H

struct tagV90AdditionalCPinfo;

/*
 * One constellation's occupancy bitmap: 128 bits, sixteen to a `short`, and
 * the reader walks `mask[7]` down to `mask[0]` testing bit 0 upwards each
 * time.  See `setParamsInfoFromCPUnPck` in
 * `src/pump/v90/V90MappingParamsInt.cpp` for what that ordering means.
 */
#define V90_CPUNPCK_MASK_WORDS	8

/* Six, for the reason in the file comment -- inference, not measurement. */
#define V90_CPUNPCK_CONSTELS	6

struct V90CPUnPck {
	/*
	 * +0x000  Twenty bytes this function never touches.  Nothing else in
	 * the object touches the structure at all, so there is no evidence
	 * for what is in them and they are not modelled.
	 */
	unsigned char pad_00[0x14];			/* +0x000 */

	/*
	 * +0x014  THE DATA BIT RATE, and the name is the object's own by way
	 * of `getDataBitRate`.  That function returns `params->word_0` less
	 * 0x14 or less 8 according to its `islong` argument; the tail of this
	 * function computes `params->word_0` as this field PLUS 0x14 or PLUS
	 * 8 under the same condition, so the two are exact inverses and this
	 * field is what `getDataBitRate` gives back.
	 *
	 * Neither width nor signedness is forced: the object does `mov` and
	 * `lea`, which say 32 bits and nothing else.  `unsigned int` follows
	 * the destination, `V90MappingParams::word_0`, which is where the sum
	 * lands.
	 */
	unsigned int dataBitRate;			/* +0x014 */

	/*
	 * +0x018..+0x02c  The six spectral-shaper words, copied straight into
	 * `V90MappingParams::shaperSR` .. `shaperB2` in order and with no
	 * arithmetic.  THE NAMES AND THE TYPES ARE THE DESTINATION'S, which
	 * `include/dsplib/V90MappingParams.h` derived from
	 * `V90ConstellationDesigner::spectralDesign` and `tools/vparse.py` --
	 * a copy carries the type of what it is copied into, which is
	 * CLAUDE.md's rank 2.  A 32-bit `mov` forces the width and nothing
	 * else, so the four floats are floats because their destinations are.
	 */
	int shaperSR;					/* +0x018 */
	unsigned int shaperId;				/* +0x01c */
	float shaperA1;					/* +0x020 */
	float shaperA2;					/* +0x024 */
	float shaperB1;					/* +0x028 */
	float shaperB2;					/* +0x02c */

	/*
	 * +0x030  NON-ZERO MEANS THE CODEC CONSTELLATIONS HAVE THEIR OWN
	 * BITMAPS.  It is read twice and does two things: it is turned into
	 * an exact 0 or 1 (`cmpb $0x0 ; setne`) and stored in
	 * `V90MappingParams::word_61c`, and it selects which of the two
	 * bitmap arrays below the codec tables are unpacked from -- its own
	 * when set, and the ORDINARY constellation bitmaps when clear.
	 *
	 * THE NAME IS USAGE INFERENCE, which is CLAUDE.md's weakest rank, and
	 * it is recorded as such.  No format string prints it and no callee
	 * types it; what is established is only that it is one byte, that it
	 * is tested against zero, and what the one reader does with it.  Its
	 * positional twin in the V.92 message is `V92CP::byte_24`, which
	 * `setV92CPpckFromParamsInfo` uses as the same gate on the same pair
	 * of blocks.
	 */
	unsigned char codecConstellationPresent;	/* +0x030 */

	/*
	 * +0x031  WHICH BITMAP EACH OF THE SIX CONSTELLATIONS USES.  Copied
	 * element for element into `V90MappingParams::distinctIndex[]`, which
	 * is where the name comes from (rank 2, a destination that is already
	 * named), and used in the same loop to select the bitmap the
	 * constellation is unpacked from.
	 *
	 * `movzbl`, so `unsigned char`, and the function neither masks nor
	 * bounds it -- see the file comment.
	 */
	unsigned char distinctIndex[V90_CPUNPCK_CONSTELS];	/* +0x031 */

	unsigned char pad_37[3];			/* +0x037 */

	/*
	 * +0x03a  The six constellations' occupancy bitmaps, `movswl` on each
	 * word.  `short` is forced: a sign-extending 16-bit load whose 32-bit
	 * result is then arithmetically shifted is the case CLAUDE.md's
	 * codegen rule calls forced.
	 */
	short constellationMask[V90_CPUNPCK_CONSTELS]
			       [V90_CPUNPCK_MASK_WORDS];	/* +0x03a */

	/* +0x09a  Two bytes nothing reads.  See the file comment. */
	unsigned char pad_9a[2];			/* +0x09a */

	/* +0x09c  The codec constellations' bitmaps, same shape. */
	short codecConstellationMask[V90_CPUNPCK_CONSTELS]
				    [V90_CPUNPCK_MASK_WORDS];	/* +0x09c */

	/*
	 * +0x0fc  THE RECORD BESIDE THE MESSAGE, and the type is rank-2
	 * evidence rather than a guess.  The only use is `info->word_04`
	 * tested against zero to choose between adding 0x14 and adding 8,
	 * which is precisely `getDataBitRate`'s `islong`; and `V90CPPacker`
	 * -- whose mangling names both types --
	 * passes `info->word_04` as that argument at .text+0x3c996.  So the
	 * pointed-at record is a `tagV90AdditionalCPinfo` and +0x04 of it is
	 * the field `include/dsplib/tagV90AdditionalCPinfo.h` already calls
	 * `word_04`.
	 */
	struct tagV90AdditionalCPinfo *info;		/* +0x0fc */
};

#endif /* DSPLIB_V90CPUNPCK_H */
