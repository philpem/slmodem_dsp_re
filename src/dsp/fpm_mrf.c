/*
 * fpm_mrf.c -- Fixed Point Modem: Multi-Rate Filter (polyphase resampler).
 *
 * Reconstructed from dsplibs.o fpm_mrf.c:
 *   FPM_MRF_init  .text 0x0a8e00
 *   FPM_MRF_free  .text 0x0a8df0
 *
 *
 * The history buffer holds one phase's worth of samples -- taps / branches --
 * which is what makes this a polyphase implementation rather than a plain
 * upsample-filter-downsample.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/sysdep.h"

void
FPM_MRF_free(struct fpm_mrf *state)
{
	sysdep_free(state->history);
}
void
FPM_MRF_init(struct fpm_mrf *state, const struct fpm_mrf_cfg *cfg, int fresh)
{
	short per_phase;
	int allocate;
	int i;

	state->cfg = *cfg;
	state->need = 1;
	state->phase = 0;
	state->widx = 0;

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
		/* Existing buffer too small: replace it.  No newline in the
		 * original's message, unlike every other one here. */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Reallocate FPM_MRF buffer");
		sysdep_free(state->history);
		state->history_len = per_phase;
		allocate = 1;
	} else {
		/* Big enough to reuse; just adopt the new length. */
		state->history_len = per_phase;
		allocate = 0;
	}

	if (allocate)
		state->history = sysdep_malloc(
			(unsigned)per_phase * sizeof(short));

	for (i = 0; i < state->history_len; i++)
		state->history[i] = 0;
}




/* Branchless circular increment, as the original writes it. */
static int
advance(int idx, int len)
{
	int next = idx + 1;

	return (next < len) ? next : 0;
}

short
FPM_MRF_filter(struct fpm_mrf *state, const short *in, short *out, short count)
{
	const int branches = state->cfg.branches;
	const int decimate = state->cfg.decimate;
	const short *coeff = state->cfg.coeff;
	short *history = state->history;
	const int hlen = state->history_len;
	int phase = state->phase;
	int widx = state->widx;
	int need = state->need;
	int produced = 0;
	int remaining = count;

	while (remaining != 0) {
		const short *c;
		int acc = 0;
		int k;

		/*
		 * Not enough input left for another output.  Take what there
		 * is and carry the shortfall in `need` -- this is what lets a
		 * stream be fed in arbitrary fragments.
		 */
		if (remaining < need) {
			need -= remaining;
			while (remaining-- > 0) {
				widx = advance(widx, hlen);
				history[widx] = *in++;
			}
			break;
		}

		for (k = 0; k < need; k++) {
			widx = advance(widx, hlen);
			history[widx] = *in++;
		}
		remaining -= need;

		/*
		 * One output: the whole history, newest first, against
		 * coefficients strided by `branches` starting at `phase`.
		 * The walk wraps from index 0 round to the top of the buffer.
		 */
		c = coeff + phase;
		for (k = widx; k >= 0; k--) {
			acc += history[k] * *c;
			c += branches;
		}
		for (k = hlen - 1; k > widx; k--) {
			acc += history[k] * *c;
			c += branches;
		}

		out[produced++] = (short)(acc >> 15);

		/* Advance the phase; each wrap past `branches` costs an input. */
		phase += decimate;
		need = 0;
		while (phase >= branches) {
			phase -= branches;
			need++;
		}
	}

	state->phase = (short)phase;
	state->widx = (short)widx;
	state->need = (short)need;
	return (short)produced;
}

/*
 * The library default, from .data:0x81a0.  9:10 with no coefficients -- every
 * caller supplies its own filter and ratio, so this is a template rather than
 * a usable converter.
 */
const struct fpm_mrf_cfg FPM_MRF_CFG = {
	.branches = 9,
	.decimate = 10,
	.taps = 270
	/* coeff and aux are NULL: every caller supplies its own filter */
};
