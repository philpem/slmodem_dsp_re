/*
 * fpm_ecc.c -- Fixed Point Modem: Echo Canceller.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_ECC_cancel  .text 0x0a6e00
 *   FPM_ECC_init    .text 0x0a7610
 *   FPM_ECC_free    .text 0x0a7870
 *
 * See dsplib/fpm_ecc.h for the shape of the thing.  In brief: the transmitted
 * symbols go into `line`, two taps `far_lag` apart replay them into two short
 * complex histories, and each history is convolved against its own adaptive
 * coefficients to build the echo estimate that is subtracted from the input.
 */

#include "dsplib/fpm_ecc.h"
#include "dsplib/sysdep.h"

void
FPM_ECC_init(struct fpm_ecc *state, const struct fpm_ecc_cfg *cfg, int fresh)
{
	short line_len;
	short taps;
	short i, j;
	int back;

	if (cfg != 0)
		state->cfg = *cfg;
	else
		state->cfg = ECC_CFG;

	/*
	 * The delay line has to hold the far tap's whole reach.  `taps` is the
	 * complex coefficient count of one section pair, so one coefficient
	 * set is 2 * taps entries.
	 */
	taps = (short)(state->cfg.near_taps + state->cfg.far_taps);
	line_len = (short)(state->cfg.far_lag + state->far_delay +
			   state->near_delay);

	if (fresh) {
		/*
		 * Unconditional, unlike FPM_MRF_init: no existing buffer is
		 * inspected and none is freed, so calling init twice with
		 * `fresh` set leaks.  That is the original's behaviour.
		 */
		state->near_i = (short *)sysdep_malloc(
			(unsigned)state->cfg.near_taps * sizeof(short));
		state->near_q = (short *)sysdep_malloc(
			(unsigned)state->cfg.near_taps * sizeof(short));
		state->far_i = (short *)sysdep_malloc(
			(unsigned)state->cfg.far_taps * sizeof(short));
		state->far_q = (short *)sysdep_malloc(
			(unsigned)state->cfg.far_taps * sizeof(short));
		state->line = (short *)sysdep_malloc(
			(unsigned)line_len * sizeof(short));
		for (j = 0; j <= 2; j++)
			state->coef[j] = (short *)sysdep_malloc(
				(unsigned)(2 * taps) * sizeof(short));
	}

	state->hold_power = 0;
	state->unk1a = 0;
	state->enabled = 1;
	state->pwr_in = 0;
	state->pwr_out = 0;
	state->freeze = 0;
	state->adapt_near = 0;
	state->adapt_far = 0;

	/*
	 * The two read taps.  `back` is where the near tap sits measured from
	 * the top of the line; the far tap is `far_lag` symbols older, and is
	 * NOT reduced modulo the length -- only the near one is.
	 */
	back = line_len - state->near_delay;
	state->near_rd = (short)(back % line_len);
	state->far_rd = (short)(back - state->cfg.far_lag);
	state->line_len = line_len;

	state->near_idx = 0;
	state->near_len = state->cfg.near_taps;
	state->far_idx = 0;
	state->far_len = state->cfg.far_taps;
	state->phase = 0;
	state->mu = 0x29;

	for (i = 0; i < state->near_len; i++) {
		state->near_i[i] = 0;
		state->near_q[i] = 0;
	}
	for (i = 0; i < state->far_len; i++) {
		state->far_i[i] = 0;
		state->far_q[i] = 0;
	}
	for (j = 0; j <= 2; j++)
		for (i = 0; i < 2 * taps; i++)
			state->coef[j][i] = 0;
	for (i = 0; i < line_len; i++)
		state->line[i] = state->cfg.fill;
}

void
FPM_ECC_free(struct fpm_ecc *state)
{
	short j;

	for (j = 2; j >= 0; j--)
		sysdep_free(state->coef[j]);
	sysdep_free(state->line);
	sysdep_free(state->far_q);
	sysdep_free(state->far_i);
	sysdep_free(state->near_q);
	sysdep_free(state->near_i);
}

/*
 * The library default, from .data:0x8114.  Not const: see the header.  No
 * maps and only four far taps -- a template every caller replaces, exactly as
 * FPM_MRF_CFG_data is for the resampler.
 */
struct fpm_ecc_cfg ECC_CFG = {
	480,	/* far_lag   */
	40,	/* near_taps */
	4,	/* far_taps  */
	0,	/* pad06     */
	0,	/* imap      */
	0,	/* qmap      */
	0,	/* fill      */
	0,	/* pad12     */
	0	/* aux       */
};
