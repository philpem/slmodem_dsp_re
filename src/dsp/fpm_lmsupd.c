/*
 * fpm_lmsupd.c -- Fixed Point Modem: one LMS coefficient update.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_lmsupd        .text 0x0abbc0  150
 *   FPM_lmsupd2       .text 0x0abc60  182
 *   FPM_block_update  .text 0x0abd20  244
 *
 * The three are contiguous in `.text` and `voice_dle_command` follows at
 * 0x0abe20.  This does not establish a translation-unit boundary.  The blob
 * has no `fpm_lmsupd.c` FILE record; nearby `fpm_adeq.c` and `voice.c` remain
 * ownership candidates, with no local-symbol evidence selecting either.
 * This file is a reconstruction grouping, not a recovered original TU.
 *
 * NOTHING IN THE OBJECT CALLS THE LAST TWO.  `FPM_lmsupd` has two callers,
 * both inside `FPM_FSE_receive`; `readelf -r` finds no relocation naming
 * `FPM_lmsupd2` or `FPM_block_update` anywhere in `dsplibs.o`, and
 * `tools/service.py` puts both in the class no entry point reaches.  So their
 * argument ROLES cannot be established the strong way -- there is no caller to
 * type them and no format string that prints them.  The parameter names below
 * describe what the instructions demonstrably do with each value and claim
 * nothing beyond that; see the comment on each.  Finding F8162.
 *
 * NOT PART OF THE EQUALISER BATCH: it is finding F1600's "leaf math" group,
 * written here because `FPM_FSE_receive` cannot be linked without it.  It is
 * not one of the two symbols that group shares with V.22.
 *
 * The walk is the same shape as the equaliser's own FIR: from the newest
 * sample down to the base of the history, then from the top of the buffer
 * back down to the newest.  `coeff` advances monotonically across both, so
 * the pairing is coefficient order against sample age.
 */

#include "dsplib/fpm.h"

void
FPM_lmsupd(short *coeff, const short *hist, short widx, short taps, short err)
{
	short *c = coeff;
	short k;

	for (k = widx; k >= 0; k--) {
		*c = (short)(*c + ((hist[k] * err + 0x20000) >> 18));
		c++;
	}

	for (k = (short)(taps - 1); k > widx; k--) {
		*c = (short)(*c + ((hist[k] * err + 0x20000) >> 18));
		c++;
	}
}

/*
 * FPM_lmsupd2 -- .text 0x0abc60, 182 bytes.
 *
 * FPM_lmsupd with the correction formed in TWO rounded stages instead of one.
 * The walk is identical -- newest sample down to the base, then the top of the
 * buffer back down to the newest, coefficients advancing monotonically across
 * both -- and only the arithmetic inside differs:
 *
 *     FPM_lmsupd    (hist[k] * err + 0x20000) >> 18
 *     FPM_lmsupd2   ((short)((hist[k] * mu + 0x10) >> 5) * err + 0x10000) >> 17
 *
 * Both stages round to nearest, and the total right shift is 5 + 17 = 22.  The
 * `(short)` between them is the object's `cwtl` at 0x0abcb1 and is NOT
 * cosmetic: the first stage's result is narrowed to 16 bits before the second
 * multiply, so a product that overflows a short wraps there rather than
 * carrying into the final sum.  That narrowing is the whole behavioural
 * difference between this and a single `>> 22`.
 *
 * WHERE `mu` AND `err` COME FROM, since there is no caller to type them.  Two
 * independent lines agree, and neither is guesswork:
 *
 *   - `ecc_adapt` in src/dsp/fpm_ecc.c is this arithmetic EXACTLY -- the same
 *     0x10/5 stage, the same `(short)` narrowing, the same 0x10000/17 stage,
 *     in the same order.  It was reconstructed from the code `FPM_ECC_cancel`
 *     inlines, where the echo canceller's own context supplies the meanings:
 *     the value multiplying the history sample is the adaptation step and the
 *     one multiplying the narrowed product is the residual.  This function is
 *     the library form of that loop, as `FPM_circ_dotp2` is of `ecc_filter`.
 *   - The argument slot: `err` here occupies position five, which is exactly
 *     the slot `FPM_lmsupd` above calls `err`.  On its own that is argument
 *     position plus a naming convention and would be the weakest class of
 *     evidence; landing on the same answer as the arithmetic twin is what
 *     makes it worth stating.
 *
 * NOTE THE ORDER: `err` comes BEFORE `mu` in the argument list, so this is
 * `FPM_lmsupd`'s signature with `mu` appended, not the arithmetic's order.
 * Finding F8162.
 *
 * `*c` is read with `movzwl` in the object, exactly as in `FPM_lmsupd` above,
 * and in both the upper half is discarded by the 16-bit store -- a dead
 * extension, free to the compiler either way (finding F614), and the same
 * `short *c` spelling reproduces it.
 */
void
FPM_lmsupd2(short *coeff, const short *hist, short widx, short taps, short err,
	    short mu)
{
	short *c = coeff;
	short k;
	int t;

	for (k = widx; k >= 0; k--) {
		t = (short)((hist[k] * mu + 0x10) >> 5);
		*c = (short)(((t * err + 0x10000) >> 17) + *c);
		c++;
	}

	for (k = (short)(taps - 1); k > widx; k--) {
		t = (short)((hist[k] * mu + 0x10) >> 5);
		*c = (short)(((t * err + 0x10000) >> 17) + *c);
		c++;
	}
}

/*
 * FPM_block_update -- .text 0x0abd20, 244 bytes.
 *
 * Correlate a block of `count` samples against a circular history and add the
 * scaled result into each of `taps` coefficients.  Nine arguments, no caller
 * anywhere in the object, and every name below is a description of what the
 * instructions do with the value rather than a claim about its role.
 *
 *     for i in 0 .. taps-1:
 *         acc = 0                                 16-bit, truncating
 *         pos = widx - i
 *         for j in 0 .. count-1:
 *             idx  = pos < 0 ? pos + hlen : pos   ONE conditional add
 *             acc  = (short)(acc + x[j] * hist[idx])
 *             pos -= step
 *         coeff[i] = (short)(coeff[i] + gain * acc)
 *
 * `acc` IS A SHORT and is re-truncated on every inner iteration (`movswl
 * %dx,%esi` at 0x0abdbd), so this is not an int accumulator narrowed at the
 * end.  **That is a codegen claim and nothing else**: `acc` is used exactly
 * once, in `(short)(coeff[i] + gain * acc)`, so a 32-bit accumulator is
 * congruent to this one modulo 65536 at every step and produces a bit-
 * identical coefficient for every input.  Written this way because it is what
 * the object's instructions say and what makes the compiler emit them; do not
 * expect a differential test to defend it, and see the `equivalent` entry in
 * `test/mutations/fpmlmsupd2.json` for the proof that none can.  Finding F8163.
 *
 * TWO THINGS HERE ARE EASY TO SMOOTH OVER AND DO CHANGE THE ANSWER:
 *
 *   - THE WRAP IS A SINGLE CONDITIONAL ADD, NOT A MODULO.  `pos` runs down
 *     monotonically in its own register and the wrap is computed into a
 *     different one (0x0abdd4-0x0abddd adds `hlen` to a copy and leaves `pos`
 *     alone), so once `pos` falls below `-hlen` the "wrapped" index is still
 *     negative and the load goes below `hist`.  Writing `pos = (pos + hlen) %
 *     hlen`, or updating `pos` in place, is a different function.
 *   - `pos` is 16-bit throughout: the object reads it with `movswl %cx,%eax`
 *     every iteration, so `pos -= step` wraps at 16 bits.
 *
 * The history and sample loads are `movzwl` and the coefficient load is too;
 * every one of them is discarded above bit 15 by a 16-bit store, so all three
 * are dead extensions in the sense of finding F614 and carry no type
 * information.  Finding F8163.
 */
void
FPM_block_update(short *coeff, short taps, const short *hist, short widx,
		 short hlen, const short *x, short count, short step,
		 short gain)
{
	short i;

	for (i = 0; i < taps; i++) {
		short acc = 0;
		short pos = (short)(widx - i);
		short j;

		for (j = 0; j < count; j++) {
			short idx = (short)(pos < 0 ? (short)(pos + hlen) : pos);

			acc = (short)(acc + x[j] * hist[idx]);
			pos = (short)(pos - step);
		}

		coeff[i] = (short)(coeff[i] + gain * acc);
	}
}
