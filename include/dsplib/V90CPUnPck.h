/**
 * @file V90CPUnPck.h
 * @brief The unpacked V.90 CP message, as `setParamsInfoFromCPUnPck` reads it.
 *
 * The name is the function's own word, exactly as `V92CPUnPck.h` derives
 * its own: the reader is `extern "C"`, so nothing in the object mangles
 * this type, and `V90CPUnPck` is taken from the only place the author
 * wrote anything about it -- the name of the function that consumes it.
 *
 * `setParamsInfoFromCPUnPck` (.text+0x336b0, 622 bytes) has zero relocations
 * of any kind pointing at it anywhere in the object, so it has no caller and
 * this layout rests on that one function's displacements alone -- no second
 * reader to cross-check it, no writer to bound the untouched fields, no
 * `sizeof` anywhere. That it is the sole reader is measured, not assumed: of
 * every symbol touching an address at this layout's four distinctive
 * displacements (+0x31, +0x3a, +0x9c, +0xfc), it is the only one that
 * touches all four (finding F7570).
 *
 * It is not a `V90CP`, despite the name inviting that reading -- checked via
 * three of its loads (a 32-bit read where `V90CP+0x014` is 16 bits, six
 * consecutive words where `V90CP+0x018` is 8-byte-stride byte pairs, and
 * decisively, a pointer dereference through +0x0fc that on `V90CP` would
 * land inside its 0x300-byte coefficient array). What it is instead is the
 * V.90 counterpart of `V92CP` for this one purpose -- the same six spectral
 * words, the same "which constellations are distinct" list, and the same
 * pair of occupancy bitmaps, in the V.90 message's own widths -- and that
 * one-to-one correspondence with `setParamsInfoFromV92CPUnPck` (`V92CP`'s
 * `char_01`/`byte_2`/`byte_24`/`word_28`/`short_42`/`short_a2`, field for
 * field) is why most of the names below are not invented. Unlike the V.92
 * message, this one's two bitmap arrays do not abut (0x9c - 0x3a = 0x62, not
 * 0x60 = 6*16) -- the two bytes between them are `pad_9a` and are not
 * alignment padding, since neither the arrays nor the trailing pointer at
 * +0x0fc need it.
 *
 * Six entries in each bitmap array is inference, not measurement: the index
 * `distinctIndex[i]` is an unbounded `unsigned char` the function neither
 * masks nor checks, so the reads themselves bound nothing. Six comes from
 * the V.92 twin's array spacing and from the destination `V90MappingParams`
 * having six of everything; a caller passing 6 or more would read past the
 * array here exactly as the object does.
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
	/* +0x000  Twenty bytes this function never touches; unmodelled (no other reader exists to bound them). */
	unsigned char pad_00[0x14];			/* +0x000 */

	/* +0x014  The data bit rate: exact inverse of what `getDataBitRate` returns. See F7570. */
	unsigned int dataBitRate;			/* +0x014 */

	/*
	 * +0x018..+0x02c  The six spectral-shaper words, copied straight into
	 * `V90MappingParams::shaperSR`..`shaperB2` with no arithmetic; names
	 * and types are the destination's (F7570).
	 */
	int shaperSR;					/* +0x018 */
	unsigned int shaperId;				/* +0x01c */
	float shaperA1;					/* +0x020 */
	float shaperA2;					/* +0x024 */
	float shaperB1;					/* +0x028 */
	float shaperB2;					/* +0x02c */

	/*
	 * +0x030  Non-zero means the codec constellations have their own
	 * bitmaps: stored as an exact 0/1 into `V90MappingParams::word_61c`,
	 * and selects which bitmap array the codec tables are unpacked from.
	 * Usage inference only (CLAUDE.md's weakest rank) -- positional twin
	 * of `V92CP::byte_24`. See F7570.
	 */
	unsigned char codecConstellationPresent;	/* +0x030 */

	/* +0x031  Which bitmap each of the six constellations uses; copied into `V90MappingParams::distinctIndex[]`, unbounded `unsigned char`. See F7570. */
	unsigned char distinctIndex[V90_CPUNPCK_CONSTELS];	/* +0x031 */

	unsigned char pad_37[3];			/* +0x037 */

	/* +0x03a  The six constellations' occupancy bitmaps; `short` is forced by a sign-extending load. See F7570. */
	short constellationMask[V90_CPUNPCK_CONSTELS]
			       [V90_CPUNPCK_MASK_WORDS];	/* +0x03a */

	/* +0x09a  Two bytes nothing reads.  See the file comment. */
	unsigned char pad_9a[2];			/* +0x09a */

	/* +0x09c  The codec constellations' bitmaps, same shape. */
	short codecConstellationMask[V90_CPUNPCK_CONSTELS]
				    [V90_CPUNPCK_MASK_WORDS];	/* +0x09c */

	/* +0x0fc  The record beside the message: a `tagV90AdditionalCPinfo *`, whose +0x04 is `getDataBitRate`'s `islong` (F7570). */
	struct tagV90AdditionalCPinfo *info;		/* +0x0fc */
};

#endif /* DSPLIB_V90CPUNPCK_H */
