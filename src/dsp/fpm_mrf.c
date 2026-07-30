/*
 * fpm_mrf.c -- multi-rate (polyphase resampling) filter.
 *
 * Reconstructed from dsplibs.o fpm_mrf.c:
 *   FPM_MRF_init  .text 0x0a8e00
 *   FPM_MRF_free  .text 0x0a8df0
 *
 * FPM_MRF_filter (.text 0x0a8ed0, 533 bytes) is not yet reconstructed.
 *
 * The history buffer holds one phase's worth of samples -- taps / branches --
 * which is what makes this a polyphase implementation rather than a plain
 * upsample-filter-downsample.
 */

#include "dsplib/fpm_mrf.h"

extern void *sysdep_malloc(unsigned size);
extern void sysdep_free(void *ptr);

void
FPM_MRF_init(struct fpm_mrf *state, const struct fpm_mrf_cfg *cfg, int fresh)
{
	short per_phase;
	int allocate;
	int i;

	state->cfg = *cfg;
	state->f10 = 1;
	state->f12 = 0;
	state->f14 = 0;

	per_phase = (short)(cfg->taps / cfg->branches);

	if (fresh) {
		/*
		 * Caller asserts the state is uninitialised, so the existing
		 * buffer pointer is not inspected -- and notably not freed.
		 * Calling init twice with `fresh` set therefore leaks; that is
		 * the original's behaviour and is reproduced.
		 */
		state->history_len = per_phase;
		allocate = 1;
	} else if (state->history_len < per_phase) {
		/* Existing buffer too small: replace it. */
		sysdep_free(state->history);
		state->history_len = per_phase;
		allocate = 1;
	} else {
		/* Big enough to reuse; just adopt the new length. */
		state->history_len = per_phase;
		allocate = 0;
	}

	if (allocate)
		state->history = (short *)sysdep_malloc(
			(unsigned)per_phase * sizeof(short));

	for (i = 0; i < state->history_len; i++)
		state->history[i] = 0;
}

void
FPM_MRF_free(struct fpm_mrf *state)
{
	sysdep_free(state->history);
}
