/**
 * @file V92CPUnPck.h
 * @brief The unpacked V.92 CP message, as `V92setParamsInfoFromCPUnPck`
 *        reads it.
 *
 * This block is inline in `VPcmFloModem` at +0x254c, not separately
 * allocated: `VPcmFloModem::runPcmModem` calls the unpacker twice, both
 * times passing `lea 0x254c(%esi)` as the second argument. The byte the
 * caller checks right afterwards, +0x255e (this block's +0x12, `extendEu`
 * below), is a cheap independent check that the base is right.
 *
 * The struct's name is the only thing the author wrote about it -- the tail
 * of `V92setParamsInfoFromCPUnPck` -- since nothing mangles this type (the
 * unpacker is `extern "C"`, the caller reaches it by displacement). Its
 * field names are likewise not invented: the unpacker prints eighteen of
 * them, and each is the author's own `.rodata` string, resolved with
 * `tools/relocscan.py --at` (finding F604). `V92CPUnPck` and `V90CP` turn
 * out to be the same structure modelled from opposite ends -- thirteen
 * offset/name landmarks agree (finding F7572).
 *
 * The tail is not established: the block runs to at least +0xca0 (the end
 * of `const6`), but no method of `VPcmFloModem` refers to any displacement
 * between +0x255e and the end of the class, so nothing bounds it above.
 * This declaration ends where the evidence does; a later reader of
 * `V92CP::bitsToInfo`, which is what fills this block, is what will close
 * it. Nothing here allocates one, so a short declaration cannot
 * under-allocate anything the object writes.
 *
 * Signedness throughout is read off the loads, not chosen: `drn`,
 * `trellisState` and `extendEu` are one byte each, loaded with `movsbl`
 * (signed, 32-bit result used -- CLAUDE.md's forced case). `prefilterGain`
 * is loaded as the low half of a 64-bit integer with an explicit zero high
 * half, which is what GCC emits for `(float)` of an *unsigned* 32-bit value.
 */

#ifndef DSPLIB_V92CPUNPCK_H
#define DSPLIB_V92CPUNPCK_H

/**
 * @brief Entries in each of the four coefficient arrays (`z1`, `p1`, `z2`,
 *        `p2`): 0x300 bytes between one array and the next, each indexed
 *        `(%ebp,%edx,2)`, so 384 shorts. Larger than the unpacker's own
 *        clamp on filter length (`V92_PARAMSINFO_MAX_FILTER_LEN` = 328) --
 *        both numbers are the object's, and neither is adjusted to match
 *        the other.
 */
#define V92_CPUNPCK_COEFS	384

/** @brief Entries in `M[]` -- loop bound `cmp $0xb` in the `M[%d]` print loop. */
#define V92_CPUNPCK_MODULI	12
/** @brief Entries in `LC[]`/`indexConstel[]` -- loop bound `cmp $0x5`. */
#define V92_CPUNPCK_CONSTELS	6

struct V92CPUnPck {
	/* +0x00  Not read by the unpacker and named by nothing; kept as a pad
	 * so every field below sits where the object puts it. */
	unsigned char pad_00[4];

	/* +0x04  Source field's own name; this copy gates the unpack of the
	 * moduli block (F3600). */
	int modulosEncoderPresent;

	/* +0x08  Ditto, gates the filter block (F3600). */
	int prefilterPrecoderPresent;

	/* +0x0c  Ditto, gates the constellation block (F3600). */
	int constellationPresent;

	/* +0x10  Feeds `paramsInfo->K = 2 * (drn + 17)` and the "Upstream
	 * rate" diagnostic; what the two derived quantities count is not
	 * established (F3600). */
	signed char drn;

	/* +0x11  Sign-extended into the parameter block's `trellisType` (F3600). */
	signed char trellisState;

	/* +0x12  See the file comment above -- also the base-sanity check
	 * `runPcmModem` reads right after calling the unpacker (F3600). */
	signed char extendEu;

	/*
	 * +0x13 was `pad_13[1]` -- REMOVED (finding F10151): pure alignment,
	 * confirmed by `dis.py` finding no access to offset 0x13.
	 */

	/* +0x14  Scaled by 2^-18 then by 4000 to make the parameter block's
	 * `gain`, as two separate single-precision multiplications rather
	 * than one combined expression (F3603). Loaded as unsigned (F3600). */
	unsigned int prefilterGain;

	/* +0x18 .. +0x44  An array because the print loop indexes it (F3600, F3602). */
	int M[V92_CPUNPCK_MODULI];

	/*
	 * +0x48 .. +0x54  The four filter lengths, copied +4 into the
	 * parameter block (+0x48->+0x4c etc.); the print order there
	 * (lz1, lz2, lp1, lp2) is not the declaration order, so each name is
	 * tied to its offset by the load beside its string, not by position
	 * (F3600, F3601).
	 */
	unsigned int lz1;
	unsigned int lp1;
	unsigned int lz2;
	unsigned int lp2;

	/*
	 * +0x058, +0x358, +0x658, +0x958  The four coefficient arrays as the
	 * CP carries them: 16-bit, scaled on the way out -- z1/z2 by 2^-15,
	 * p1/p2 by 2^-14. Names are one remove from the author's: he names
	 * these arrays' *lengths* lz1/lp1/lz2/lp2 and the destination arrays
	 * z1/p1/z2/p2; the pairing (which length feeds which destination) is
	 * measured, and the four labels here are inherited from the
	 * destinations (F3600). `short`, not `unsigned short`, is forced by
	 * `movswl` on every load.
	 */
	short z1[V92_CPUNPCK_COEFS];
	short p1[V92_CPUNPCK_COEFS];
	short z2[V92_CPUNPCK_COEFS];
	short p2[V92_CPUNPCK_COEFS];

	/* +0xc58 .. +0xc6c  An array because the print loop indexes it (F3600, F3602). */
	unsigned int LC[V92_CPUNPCK_CONSTELS];

	/* +0xc70 .. +0xc84  An array, forced twice over: both the print loop
	 * and the copy into the parameter block index it (F3600, F3602). */
	int indexConstel[V92_CPUNPCK_CONSTELS];

	/*
	 * +0xc88 .. +0xc9c  The six constellations the CP carries -- six
	 * separate fields and not an array, the one place in this struct
	 * where that distinction is legible: each is printed from its own
	 * format string and every access (print or copy) is a constant
	 * displacement, never indexed, unlike the three arrays above (F3600,
	 * F3602). `int *`, since the print pushes four bytes per element for
	 * a `%d` (a `float` would promote to `double` and push eight).
	 */
	int *const1;
	int *const2;
	int *const3;
	int *const4;
	int *const5;
	int *const6;
};

#endif /* DSPLIB_V92CPUNPCK_H */
