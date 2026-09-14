/*
 * fpm_mtd.c -- Fixed Point Modem: Multi-Tone Detector.
 *
 * Reconstructed from dsplibs.o fpm_mtd.c:
 *   FPM_MTD_create  .text 0x0a90f0
 *   FPM_MTD_delete  .text 0x0a91b0
 *
 *   FPM_MTD_detect  .text 0x0a91d0
 */

#include <stddef.h>

#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_iir.h"
#include "dsplib/sysdep.h"

/*
 * COEF_DC (.data:0x081d0) -- the wideband reference filter, one section, used
 * to estimate total signal energy.  Layout is FPM_iir_filt's: two recursive
 * coefficients, two feedforward, and an output scale.
 *
 * IT IS DEFINED HERE, AND WHERE IT IS DEFINED IS MEASURABLE.  The object's
 * `.data` puts it at 0x081d0, immediately after `DEF_COEFS` at 0x081bc --
 * a LOCAL symbol whose STT_FILE association is `fpm_mtd.c` -- with no padding
 * between, and the six `*_CFG` blocks below it appear in exactly the STT_FILE
 * order of the modules that own them (ECC_CFG 0x08114 / fpm_ecc.c #614
 * .. FPM_MTD_CFG 0x081b0 / fpm_mtd.c #623).  Its ONE reference in the whole
 * object is at .text 0x0a921a, inside `FPM_MTD_detect`.  It cannot be in
 * fpm_iir.c, which is #619 and would place it BELOW DEF_COEFS.
 *
 * IT ALSO USED TO BE LOAD-BEARING FOR fpm_phasor.c AND IS NOT ANY MORE.  In
 * the OBJECT, `FPM_sin_sign` is the next `.data` object in the link and is
 * read four entries BEFORE its base for any phase of 0x8000 or more, so
 * COEF_DC's last three words and the two bytes of boundary padding after them
 * ARE the sine's sign table over a quarter of the phasor's range.  We used to
 * reproduce that by arranging for the same thing to happen in OUR link, which
 * made the phasor's correctness a property of the linker; `fpm_phasor.c` now
 * carries those four words as values, so nothing about where this array lands
 * can reach the phasor, and `t_fpm_phasor` compares both outputs over all
 * 65536 phases.  Only the DEPENDENCE was removed -- the attribution above
 * rests on the address, the single reference and the binding, and stands
 * without it.  Not `const`: the object's symbol is `D`.  Findings F3620, F3621,
 * F3623, F3624 and F3700-3703, deviation D392.
 */
short COEF_DC[FPM_IIR_COEFF_PER_SECTION] = {
	-12971, 12917, 28620, -25834, 12917,
};

/*
 * fpm_mtd.c -- Fixed Point Modem: Multi-Tone Detector configuration.
 *
 * Extracted from dsplibs.o .data:0x81b0.  Mixed struct: +0x00 is a pointer
 * to the coefficient bank (R_386_32 into .data), which is why this is a
 * struct and not a short[].
 *
 * THE OBJECT'S OWN `FPM_MTD_CFG` IS NOW HERE, AND IT IS NOT `FPM_MTD_CFG_data`.
 * The two differ in exactly one field.  When this file was written the
 * coefficient bank the object points at -- `DEF_COEFS`, .data 0x81bc, file
 * static -- had no caller that needed it, so the stub below was given a NULL
 * `coeff` under its own name and the blob symbol was left unwritten.  The
 * three fax receiver constructors reference `FPM_MTD_CFG` directly, so it has
 * to exist, and a `src/` reference to an unwritten blob symbol cannot link at
 * all (F8492).
 *
 * BOTH ARE KEPT, DELIBERATELY, AND THAT IS A DEVIATION AND NOT A DESIGN.
 * `FPM_MTD_CFG_data` is read by `FPM_MTD_create` and by `B103FP_create`, and
 * unifying the two means editing `src/pump/b103/b103fp.c`, which is outside
 * this pass's scope.  The unification is not cosmetic: the object's
 * `FPM_MTD_create(state, NULL)` installs `DEF_COEFS`, and ours installs NULL,
 * so the copy that is reachable through a NULL `cfg` is the one that diverges.
 * No test covers that path today.  Recorded as D1101 and F9143; the fix is to
 * delete `FPM_MTD_CFG_data` and point its two readers at `FPM_MTD_CFG`.
 */

#include "dsplib/fpm_mtd.h"

const struct fpm_mtd_cfg FPM_MTD_CFG_data = {
	.coeff = 0,		/* deliberately NULL -- see D1101 above */
	.tones = 2,
	.ratio = 24576,		/* 0.75 in Q15 */
	.min_level = 246
	/* f0a is zero */
};

/*
 * The bank `FPM_MTD_CFG` points at.  `d` in the object -- LOCAL and writable,
 * so file-static here, with no `ref_` alias and nothing to compare it against
 * by name.  Ten shorts is what `tones = 2` over 20 bytes fixes, five per
 * biquad section, the same shape `V21_CHAN2_MTD_COEFF` and `V29_MTD_COEFF`
 * have.  Every entry is 10000, which is not a filter: it is a placeholder
 * bank, consistent with `FPM_MTD_CFG` being a default nothing configures.
 */
static short DEF_COEFS[10] = {
	10000, 10000, 10000, 10000, 10000,
	10000, 10000, 10000, 10000, 10000
};

/*
 * `D` in the object -- global and writable -- hence not `const`.  The pointer
 * at +0x00 was found by sweeping the relocations inside the symbol's own 12
 * bytes; `tabdump.py` renders its addend as -32324, which is a plausible Q15
 * coefficient and is an address.
 */
struct fpm_mtd_cfg FPM_MTD_CFG = {
	DEF_COEFS,		/* +0x00 coeff                             */
	2,			/* +0x04 tones                             */
	24576,			/* +0x06 ratio      0.75 in Q15            */
	246,			/* +0x08 min_level                         */
	0			/* +0x0a f0a                               */
};


/*
 * Leaky-integrator weights: alpha = 820/32768 (~0.025) against
 * 31948/32768 (~0.975), a time constant of roughly 40 samples.
 */
#define FPM_MTD_ALPHA     0x334
#define FPM_MTD_ONE_ALPHA 0x7ccc

struct fpm_mtd *
FPM_MTD_create(struct fpm_mtd *state, const struct fpm_mtd_cfg *cfg)
{
	int owned = 0;
	int i;

	if (state == NULL) {
		state = sysdep_malloc(sizeof(*state));
		if (state == NULL)
			return NULL;
		owned = 1;
	}

	/* Only 12 bytes are copied -- the config's first three words. */
	if (cfg != NULL) {
		state->cfg.coeff = cfg->coeff;
		state->cfg.tones = cfg->tones;
		state->cfg.ratio = cfg->ratio;
		state->cfg.min_level = cfg->min_level;
		state->cfg.f0a = cfg->f0a;
	} else {
		state->cfg = FPM_MTD_CFG_data;
	}

	/* As elsewhere in fpm_*, buffers follow the object's ownership. */
	if (owned)
		state->acc = sysdep_malloc(
			(unsigned)state->cfg.tones * 2 * sizeof(short));

	/* Two accumulators per tone. */
	for (i = 0; i < state->cfg.tones; i++) {
		state->acc[i * 2] = 0;
		state->acc[i * 2 + 1] = 0;
	}

	state->dc_state[0] = 0;
	state->dc_state[1] = 0;
	state->out_of_band = 0;
	state->wideband = 0;

	return state;
}

/*
 * Frees the accumulator array and the state, both unconditionally -- the same
 * shape as FPM_TONE_delete, and carrying the same asymmetry with create (see
 * D5).  Bell 103 does not call it: B103FP_create supplies its own embedded
 * state, so tearing that down through here would free memory it does not own.
 */
void
FPM_MTD_delete(struct fpm_mtd *state)
{
	sysdep_free(state->acc);
	sysdep_free(state);
}

short
FPM_MTD_detect(struct fpm_mtd *state, const short *samples, short count)
{
	int wideband = state->wideband;
	int tone;
	int out_of_band;
	int threshold;
	int i;

	/*
	 * Only two energies are stored, so the third is recovered: the state
	 * holds total and out-of-band, and tone energy is their difference.
	 */
	tone = (short)(wideband - state->out_of_band);

	for (i = 0; i < count; i++) {
		int wide_sample;
		int tone_sample;
		int e_wide;
		int e_tone;

		/* >> 5 of headroom before the filters, to keep the squares in range. */
		wide_sample = FPM_iir_filt((short)(samples[i] >> 5), COEF_DC,
					   state->dc_state, 1);
		tone_sample = FPM_iir_filt((short)wide_sample, state->cfg.coeff,
					   state->acc, state->cfg.tones);

		e_wide = (wide_sample * wide_sample) >> 5;
		e_tone = (tone_sample * tone_sample) >> 5;

		tone = (tone * FPM_MTD_ONE_ALPHA + e_tone * FPM_MTD_ALPHA) >> 15;
		tone = (short)tone;
		wideband = (wideband * FPM_MTD_ONE_ALPHA
			    + e_wide * FPM_MTD_ALPHA) >> 15;
		wideband = (short)wideband;
	}

	state->wideband = (short)wideband;

	/* Never negative: a tone estimate above the total would be nonsense. */
	out_of_band = (short)(wideband - tone);
	if (out_of_band < 0)
		out_of_band = 0;
	state->out_of_band = (short)out_of_band;

	if (wideband < state->cfg.min_level)
		return FPM_MTD_NOSIGNAL;

	/*
	 * Detected when the energy outside the tone is a small enough fraction
	 * of the total.  The second test is unreachable -- out_of_band is
	 * clamped at or below wideband -- but is reproduced.
	 */
	threshold = ((int)state->cfg.ratio * wideband) >> 15;
	if (out_of_band <= threshold)
		return FPM_MTD_PRESENT;

	return (wideband >= out_of_band) ? FPM_MTD_ABSENT : FPM_MTD_PRESENT;
}
