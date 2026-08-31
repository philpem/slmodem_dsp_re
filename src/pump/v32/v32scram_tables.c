/*
 * v32scram_tables.c -- ITU-T V.32/V.32bis: the scrambler's three data objects.
 *
 * Reconstructed from dsplibs.o:
 *   SDMv32_GPA  .data 0x007678   8
 *   SDMv32_GPC  .data 0x007680   8
 *   SDMv32_CFG  .data 0x007688   6
 *
 * THIS PARAGRAPH USED TO SAY ALL THREE WERE UNREFERENCED, AND IT WAS WRONG.
 * `V32FP_recreate` (0x7e870) reads every one of them -- `SDMv32_CFG` twice at
 * 7e9a6 and 7e9b2, `SDMv32_GPC` at 7e9eb and `SDMv32_GPA` at 7eb5c, four
 * `R_386_32` relocations that were there all along.  The claim, and the
 * "width is verifiable and the SHAPE is not" that followed from it, is
 * retracted; finding F8655 has what the consumer settles.
 *
 * What it settles.  `SDMv32_CFG` is a THREE-WORD TEMPLATE for the first six
 * bytes of a `struct v32_sdm`: `group`, then the two absolute tap positions
 * v32fpctl.h names V32_SDM_TAP1_POS and V32_SDM_TAP2_POS.  Entries 0 and 1
 * are overwritten before the template is installed -- entry 0 by the group
 * width, 2 or 4 by the transmit rate index, and entry 1 by GPC or GPA -- so
 * only the 23 at entry 2 survives, and it is `tap2_pos`.  `SDMv32_GPC` and
 * `SDMv32_GPA` are each indexed by the half-duplex mode, which
 * `V32FP_recreate` sets to 0, 1 or 2, so at least three of the four entries
 * are reachable and `short[4]` stands.  The element width is unchanged and
 * was never in doubt.
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
 * order differs exactly as the two polynomials differ.
 *
 * HOW THEY REACH `tap1`/`tap2` IS NOW SETTLED, by `V32FP_recreate`.  The
 * entry these two supply becomes `tap1_pos` and `SDMv32_CFG[2]`'s 23 becomes
 * `tap2_pos`; the stored `tap1`/`tap2` are then each of those LESS the group
 * shift, which is `v32scram.h`'s reading of the pair from the consumer's
 * side.  So the second tap is the register length in both directions -- the
 * x^-23 term the two polynomials share -- and the first is the exponent that
 * distinguishes them, 5 for GPC and 18 for GPA at mode 0, swapped at mode 1.
 * Finding F8655.
 */

const short SDMv32_GPA[4] = { 5, 18, 18, 18 };
const short SDMv32_GPC[4] = { 18, 5, 18, 18 };

/*
 * 4, 5, 23.  23 is V.32's register length in bits and is the one entry that
 * reaches a `struct v32_sdm` -- `V32FP_recreate` overwrites the other two
 * before installing the template, entry 0 with the group width and entry 1
 * with `SDMv32_GPC` or `SDMv32_GPA`.  So the 4 and the 5 are DEAD in the
 * object, which is why they look like a plausible group width and a
 * plausible tap and cannot be checked against anything.
 */
const short SDMv32_CFG[3] = { 4, 5, 23 };
