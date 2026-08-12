/*
 * v32scram_tables.c -- ITU-T V.32/V.32bis: the scrambler's three data objects.
 *
 * Reconstructed from dsplibs.o:
 *   SDMv32_GPA  .data 0x007678   8
 *   SDMv32_GPC  .data 0x007680   8
 *   SDMv32_CFG  .data 0x007688   6
 *
 * ALL THREE ARE UNREFERENCED.  `tools/relocscan.py --into SDMv32` resolves
 * all 10,514 R_386_32 relocations in the object and finds nothing pointing at
 * any of them: no code takes their address and no other table contains it.
 * They are global, so a caller in a translation unit that was not linked in
 * could have used them, but within this object they are dead.
 *
 * That has a consequence for what can be claimed about them: **the element
 * width is verifiable and the SHAPE is not.** Width, because the state they
 * would fill (`struct v32_sdm`) carries its taps as two adjacent `short`s and
 * `SDMv32_CFG`'s three values read as sensible shorts and as nothing else.
 * Shape, because 8 bytes is equally `short[4]` and two `short[2]` pairs, and
 * without a consumer nothing in the object decides between them.  They are
 * emitted as `short[4]` and `short[3]` with no structure asserted.
 *
 * WHAT THE VALUES LOOK LIKE, said as a reading and not as a derivation.
 * V.32 section 4.4 specifies two scrambling polynomials, one per direction:
 *
 *     GPA = 1 + x^-18 + x^-23        (the answering modem's)
 *     GPC = 1 + x^-5  + x^-23        (the calling modem's)
 *
 * and these two objects hold **5 and 18 in the two orders**, followed by a
 * pair that is the same in both.  The names match the Recommendation's, the
 * two non-trivial exponents are exactly the two values present, and the
 * order differs exactly as the two polynomials differ.  What is NOT
 * established is how either reaches `tap1`/`tap2`: the third exponent, 23,
 * is absent from both and is the register LENGTH, which appears instead in
 * `SDMv32_CFG` -- so at least one of the two taps cannot be a raw exponent.
 * The function that would settle it is not in the object.
 */

const short SDMv32_GPA[4] = { 5, 18, 18, 18 };
const short SDMv32_GPC[4] = { 18, 5, 18, 18 };

/*
 * 4, 5, 23.  23 is V.32's register length in bits.  The other two are not
 * established; 4 is the bits per symbol at 9600 with the trellis bit removed,
 * and 5 is one of the two tap exponents, but nothing here reads this object.
 */
const short SDMv32_CFG[3] = { 4, 5, 23 };
