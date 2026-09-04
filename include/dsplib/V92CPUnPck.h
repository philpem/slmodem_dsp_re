/*
 * V92CPUnPck.h -- the unpacked V.92 CP message, as
 * `V92setParamsInfoFromCPUnPck` reads it.
 *
 * WHAT IT IS AND WHERE IT LIVES.  `VPcmFloModem::runPcmModem` calls the
 * unpacker twice, at .text+0xeade and +0xeb27, and both times the second
 * argument is `lea 0x254c(%esi)` -- so this block is INLINE in
 * `VPcmFloModem`, at +0x254c, and is not separately allocated.  The byte the
 * caller tests immediately afterwards, `cmpb $0x0,0x255e(%esi)`, is +0x12 of
 * this block, which is `extendEu` below; that is a small independent check
 * that the base is right.
 *
 * THE NAME IS THE FUNCTION'S OWN WORD.  Nothing in the object mangles this
 * type -- the unpacker is `extern "C"` and the caller reaches it by
 * displacement -- so `V92CPUnPck` is taken from the only place the author
 * wrote anything about it, the tail of `V92setParamsInfoFromCPUnPck`.  The
 * FIELDS are not invented: the unpacker prints eighteen of them, and every
 * one of those names below is the author's own text from
 * `.rodata.str1.1` / `.rodata.str1.4`, resolved with `tools/relocscan.py
 * --at` (finding F604).  Every name carries the string that proves it.
 *
 * WHAT IS NOT ESTABLISHED IS THE TAIL.  The block runs to at least +0xca0,
 * which is the end of `const6`.  Nothing bounds it above: no method of
 * `VPcmFloModem` refers to any displacement between +0x255e and the end of
 * the class, so the object gives no next field to stop at.  This declaration
 * therefore ends where the evidence does, exactly as
 * `include/dsplib/V92ParamsInfo.h` used to end at its own +0x9c, and a later
 * reader of `V92CP::bitsToInfo` -- which is what FILLS this block -- is what
 * will close it.  Nothing here allocates one, so a short declaration cannot
 * under-allocate anything the object writes.
 *
 * SIGNEDNESS IS READ AND NOT CHOSEN.  `drn`, `trellisState` and `extendEu`
 * are one byte and every load of them is `movsbl` with the 32-bit result
 * used, which is the forced case of CLAUDE.md's codegen rule.
 * `prefilterGain` is loaded as the low half of a 64-bit integer whose high
 * half is an explicit zero (`push %eax` with %eax = 0, then `fildll`), which
 * is what GCC emits for `(float)` of an *unsigned* 32-bit value and not for a
 * signed one.
 */

#ifndef DSPLIB_V92CPUNPCK_H
#define DSPLIB_V92CPUNPCK_H

/*
 * 0x300 bytes between one coefficient array and the next -- +0x058, +0x358,
 * +0x658, +0x958 -- and each is indexed `(%ebp,%edx,2)`, so 384 shorts.
 *
 * That is NOT the same number as the ceiling the unpacker clamps the lengths
 * to, which is V92_PARAMSINFO_MAX_FILTER_LEN = 0x148 = 328.  The declared
 * array is larger than the largest length that can be read out of it; both
 * numbers are the object's and neither is adjusted to fit the other.
 */
#define V92_CPUNPCK_COEFS	384

/* Twelve `M[%d]`, six `LC[%d]`, six `indexConstel[%d]` -- the loop bounds are
 * `cmp $0xb` and `cmp $0x5` in the three print loops. */
#define V92_CPUNPCK_MODULI	12
#define V92_CPUNPCK_CONSTELS	6

struct V92CPUnPck {
	/*
	 * +0x00  Not read by the unpacker and named by nothing.  Four bytes
	 * kept as a pad so that every field below sits where the object puts
	 * it.
	 */
	unsigned char pad_00[4];

	/* +0x04  "CPObj->modulosEncoderPresent = %d", .rodata.str1.4:0x32ac.
	 * The author's spelling of "modulus"; kept as he wrote it. */
	int modulosEncoderPresent;

	/* +0x08  "CPObj->prefilterPrecoderPresent = %d", str1.4:0x32d0. */
	int prefilterPrecoderPresent;

	/* +0x0c  "CPObj->constellationPresent = %d", str1.4:0x32f8. */
	int constellationPresent;

	/*
	 * +0x10  "CPObj->drn = %d", str1.1:0x94d.  The unpacker makes two
	 * things of it and prints both:
	 *
	 *     paramsInfo->K = 2 * (drn + 17)          lea 0x22(%edi,%edi,1)
	 *     "Upstream rate = %d" of (drn + 17) * 1333.3333740234375
	 *
	 * Both are transcribed; what the two quantities count is not
	 * established here and is not guessed.
	 */
	signed char drn;

	/* +0x11  "CPObj->trellisState = %d", str1.1:0x8ff.  Sign-extended into
	 * the parameter block's `trellisType`. */
	signed char trellisState;

	/* +0x12  "CPObj->extendEu = %d", str1.1:0x936. */
	signed char extendEu;

	/*
	 * +0x13 was `pad_13[1]` -- REMOVED (finding F10145).  Already
	 * correctly described as alignment; proved mechanically now by the
	 * next field's own `+0x14` annotation, which `tools/offcheck.py`
	 * checks against the compiler's own `offsetof` on every build, and
	 * by `dis.py` over `V92setParamsInfoFromCPUnPck` (the only
	 * reconstructed function that touches this struct) finding no
	 * access to offset 0x13.
	 */

	/*
	 * +0x14  "CPObj->prefilterGain = %d", str1.1:0x91a.  Scaled by 2^-18
	 * and then by 4000 to make the parameter block's `gain`; the two
	 * multiplications are printed either side as the "constellation gain"
	 * before and after "Lu multiplication".
	 */
	unsigned int prefilterGain;

	/*
	 * +0x18 .. +0x44  "CPObj->M[%d] = %d", str1.1:0x8bc.  An ARRAY, and
	 * that is forced rather than modelled: the print loop indexes it,
	 * `mov 0x18(%ebp,%ebx,4),%eax` under `cmp $0xb`.
	 */
	int M[V92_CPUNPCK_MODULI];

	/*
	 * +0x48 .. +0x54  The four filter lengths.  The four sit in the same
	 * order in both blocks and the unpacker's copy is +4 across the board:
	 * +0x48 -> +0x4c, +0x4c -> +0x50, +0x50 -> +0x54, +0x54 -> +0x58, so
	 * lz1, lp1, lz2, lp2 here are lz1, lp1, lz2, lp2 there.
	 *
	 * "CPObj->lz1" str1.1:0x974, "CPObj->lz2" 0x986, "CPObj->lp1" 0x998,
	 * "CPObj->lp2" 0x9aa -- and the print order there (lz1, lz2, lp1, lp2)
	 * is not the declaration order, which is why each is tied to its
	 * offset by the load beside the string rather than by position.
	 */
	unsigned int lz1;
	unsigned int lp1;
	unsigned int lz2;
	unsigned int lp2;

	/*
	 * +0x058, +0x358, +0x658, +0x958  The four coefficient arrays as the
	 * CP carries them: 16-bit, and scaled on the way out -- z1 and z2 by
	 * 2^-15, p1 and p2 by 2^-14 (`.rodata.cst4` +0xb0 and +0xb4).
	 *
	 * The NAMES here are one remove from the author's: he prints these
	 * four arrays' lengths as lz1/lp1/lz2/lp2 and prints the *destination*
	 * arrays as z1/p1/z2/p2, and each source array is read with the
	 * matching length into the matching destination.  So the pairing is
	 * measured and the four labels are inherited.  `movswl` on every load
	 * makes `short` rather than `unsigned short` forced.
	 */
	short z1[V92_CPUNPCK_COEFS];
	short p1[V92_CPUNPCK_COEFS];
	short z2[V92_CPUNPCK_COEFS];
	short p2[V92_CPUNPCK_COEFS];

	/*
	 * +0xc58 .. +0xc6c  "CPObj->LC[%d] = %d", str1.1:0x8d0.  An array, and
	 * forced: the print loop indexes it under `cmp $0x5`.
	 */
	unsigned int LC[V92_CPUNPCK_CONSTELS];

	/*
	 * +0xc70 .. +0xc84  "CPObj->indexConstel[%d] = %d", str1.4:0x328c.
	 * Forced twice over -- the print loop indexes it AND the copy into the
	 * parameter block is a real indexed loop.
	 */
	int indexConstel[V92_CPUNPCK_CONSTELS];

	/*
	 * +0xc88 .. +0xc9c  The six constellations the CP carries.
	 *
	 * SIX FIELDS AND NOT AN ARRAY, which is the one place in this struct
	 * where that distinction is legible.  The author prints them from six
	 * DIFFERENT format strings -- "\tconst1[%d] = %d" (str1.1:0xa1b),
	 * const2 (0xa08), const3 (0x9f5), const4 (0x9e2), const5 (0x9cf),
	 * const6 (0x9bc) -- and no access to any of the six, in the prints or
	 * in the copies, is indexed: every one is a constant displacement.
	 * The three arrays above are the other way round on both counts.
	 *
	 * `int *` because the print pushes four bytes per element for a `%d`;
	 * a float would have been promoted to double and pushed as eight.
	 */
	int *const1;
	int *const2;
	int *const3;
	int *const4;
	int *const5;
	int *const6;
};

#endif /* DSPLIB_V92CPUNPCK_H */
