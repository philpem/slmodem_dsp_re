/*
 * fpm_fsd.c -- FSK demodulator.
 *
 * Reconstructed from dsplibs.o fpm_fsd.c:
 *   FPM_FSD_init  .text 0x0a78e0
 *   FPM_FSD_free  .text 0x0a7ce0
 *
 * FPM_FSD_demodulate (.text 0x0a79f0, 745 bytes) is not yet reconstructed.
 */

#include "dsplib/fpm_fsd.h"

extern void *sysdep_malloc(unsigned size);
extern void sysdep_free(void *ptr);

void
FPM_FSD_init(struct fpm_fsd *state, const struct fpm_fsd_cfg *cfg, int fresh)
{
	int i;

	state->cfg = *cfg;
	state->f28 = 0;
	state->f30 = 0;
	state->f32 = 0;
	state->f34 = 0;

	/*
	 * Unlike FPM_MRF_init, `fresh` here does not consult the existing
	 * buffers at all -- it simply allocates.  Re-initialising in place
	 * (fresh == 0) reuses whatever is there, so the caller must not change
	 * the lengths between calls.
	 */
	if (fresh) {
		state->fir_hist = (short *)sysdep_malloc(
			(unsigned)state->cfg.fir_taps * sizeof(short));
		state->iir_hist = (short *)sysdep_malloc(
			(unsigned)state->cfg.iir_len * 2 * sizeof(short));
		state->buf1c = (short *)sysdep_malloc(
			(unsigned)state->cfg.f16 * sizeof(short));
	}

	for (i = 0; i < state->cfg.fir_taps; i++)
		state->fir_hist[i] = 0;

	for (i = 0; i < 2 * state->cfg.iir_len; i++)
		state->iir_hist[i] = 0;

	state->f20 = 0;
	state->f22 = (short)(state->cfg.f12 >> 1);

	for (i = 0; i < state->cfg.f16; i++)
		state->buf1c[i] = 0;
}

/*
 * Frees all three buffers unconditionally but *not* the state, which is the
 * opposite of FPM_TONE_delete.  The caller owns the state -- B103FP_create
 * embeds it in a larger block rather than allocating it separately.
 */
void
FPM_FSD_free(struct fpm_fsd *state)
{
	sysdep_free(state->buf1c);
	sysdep_free(state->iir_hist);
	sysdep_free(state->fir_hist);
}
