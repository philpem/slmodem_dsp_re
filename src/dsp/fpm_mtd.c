/*
 * fpm_mtd.c -- multi-tone detector.
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

/* The wideband reference filter, shared with the rest of the library. */
extern const short COEF_DC[];

/*
 * Leaky-integrator weights: alpha = 820/32768 (~0.025) against
 * 31948/32768 (~0.975), a time constant of roughly 40 samples.
 */
#define FPM_MTD_ALPHA     0x334
#define FPM_MTD_ONE_ALPHA 0x7ccc

extern void *sysdep_malloc(unsigned size);
extern void sysdep_free(void *ptr);

struct fpm_mtd *
FPM_MTD_create(struct fpm_mtd *state, const struct fpm_mtd_cfg *cfg)
{
	int owned = 0;
	int i;

	if (state == NULL) {
		state = (struct fpm_mtd *)sysdep_malloc(sizeof(*state));
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
		state->acc = (short *)sysdep_malloc(
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
