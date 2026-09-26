/*
 * fpm_div32.c -- Fixed Point Modem: the 32-bit reciprocal helper and the
 *                library circular dot product.
 *
 * Reconstructed from dsplibs.o fpm_div32.c:
 *
 *   FPM_div_32      .text 0x0a6c90   147 bytes
 *   FPM_circ_dotp2  .text 0x0a6d30   195 bytes
 *
 * THE TRANSLATION UNIT IS RECOVERED FROM THE `.text` ADDRESS BRACKET, not
 * guessed.  `ld -r` concatenates each input's `.text` in link order and the
 * STT_FILE records are that order: the FILE sequence here is `fpm_div.c` (238),
 * `fpm_div32.c` (239), `fpm_ecc.c` (240).  `FPM_div` (0x0a6bf0, 150 bytes,
 * ending 0x0a6c86) is `fpm_div.c`'s function, `FPM_ECC_cancel` (0x0a6e00) is
 * `fpm_ecc.c`'s first function, and the block [0x0a6c90, 0x0a6e00) between
 * them -- `FPM_div_32` then `FPM_circ_dotp2`, contiguous and in that order --
 * is `fpm_div32.c` and nothing else fits.
 *
 * FPM_div_32 was previously grouped in `fpm_div.c`; FPM_circ_dotp2 in
 * `fpm_ecc.c`, where its own comment recorded `tools/tuattrib.py` as
 * `ambiguous` and bracketed `fpm_div.c|fpm_ecc.c` (F8164).  The FILE order
 * resolves both: the bracket is a real TU.  Every body is moved VERBATIM.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm.h"

/*
 * The 32-bit denominator.  Same contract, same table, same D4 overrun: the
 * mantissa is the top 16 bits of the normalised word, so it lies in
 * [0x8000, 0xffff] exactly as FPM_div's does and the index runs 0..128.
 *
 * The two differences from FPM_div are both in the normalisation: the shift
 * count can reach 31, and the loop tests the whole 32-bit word rather than a
 * 16-bit one, so a denominator whose top bit is already set is returned with
 * a shift of zero without the loop running at all.
 */
int
FPM_div_32(unsigned int denom, unsigned short *recip, unsigned short *shift)
{
	unsigned short count = 0;
	unsigned short mantissa;
	int index;

	if (denom == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Fatal error: Division by zero!\n");
		return 1;
	}

	/* Left-normalise until the top bit is set, counting the shifts. */
	while ((int)denom >= 0) {
		denom += denom;
		count++;
	}

	mantissa = (unsigned short)(denom >> 16);
	index = (int)((mantissa + 0x80) >> 8) - 0x80;

	*recip = FPM_div_table[index];
	*shift = count;
	return 0;
}

/*
 * FPM_circ_dotp2 -- .text 0x0a6d30, 195 bytes.
 *
 * The LIBRARY form of `ecc_filter` below: the same circular walk, the same
 * `>> 3` on each product, generalised with a coefficient STRIDE and with the
 * caller's residual shift folded in.  `ecc_filter` shifts each product by 3
 * and leaves the remaining 14 to `FPM_ECC_cancel`; this one is told the TOTAL
 * shift and applies `shift - 3` to the sum itself, so `shift == 17` is
 * `ecc_filter` plus what its caller does.  The two are not interchangeable in
 * the object -- `FPM_ECC_cancel` contains no `call` instruction at all, so its
 * copy is inlined and this one stands alone.
 *
 * WHICH TRANSLATION UNIT IT BELONGS TO IS NOT SETTLED BY THE ADDRESS.
 * `tools/tuattrib.py` reports it `ambiguous` and brackets it `fpm_div.c|
 * fpm_ecc.c`: it sits between `FPM_div_32` (0x0a6c90) and `FPM_ECC_cancel`
 * (0x0a6e00) with a surviving local symbol on neither side of it.  It is put
 * here, first in the file, because a circular dot product is this file's own
 * inner loop and is nothing to do with division, and because first-in-file
 * reproduces the object's emission order either way.  That is an inference
 * from content, not a derivation -- finding F8164 says so, and if a later pass
 * finds a `fpm_div.c` local symbol below 0x0a6d30 it moves.
 *
 * PUTTING IT FIRST COST THE REST OF THIS FILE NOTHING, and that was MEASURED
 * rather than assumed: emission order drives register allocation (findings
 * F7796 and F7800) and `make phase` cannot see a bystander regression at all
 * (F8111).  Built both ways with the period compiler and compared raw `.text`
 * bytes per symbol, `FPM_ECC_cancel` (1873 B), `FPM_ECC_init` (599 B) and
 * `FPM_ECC_free` (98 B) come out byte-identical with and without this function
 * above them, relocation targets included.  Finding F8167, which also records
 * that the first attempt at that measurement compared objdump TEXT and
 * reported all three CHANGED -- objdump prints absolute branch targets, and
 * 195 bytes of new function ahead of them moves every one.
 *
 * NO CALLER ANYWHERE IN THE OBJECT, so `coeff`, `stride` and `shift` are named
 * for what the instructions do with them and not for a role.
 *
 * `shift` BELOW 3 IS OUT OF CONTRACT.  The object computes the residual as
 * `shift - 3` into `%cl` and executes `sar %cl,%edi`, and x86 masks the count
 * to five bits -- so `shift == 2` shifts right by 31, not left by one.  Not
 * reproduced as a deviation because nothing calls it and there is no
 * behaviour to preserve; the tests stay inside `shift >= 3`.
 */
short
FPM_circ_dotp2(const short *coeff, const short *hist, short widx, short taps,
	       short stride, short shift)
{
	const short *c = coeff;
	int acc = 0;
	short i;

	for (i = widx; i >= 0; i--) {
		acc += (hist[i] * *c) >> 3;
		c += stride;
	}
	for (i = (short)(taps - 1); i > widx; i--) {
		acc += (hist[i] * *c) >> 3;
		c += stride;
	}

	return (short)(acc >> (shift - 3));
}
