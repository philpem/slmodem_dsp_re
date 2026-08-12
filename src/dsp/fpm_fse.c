/*
 * fpm_fse.c -- Fixed Point Modem: Fractionally Spaced Equaliser.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_FSE_init  .text 0x0a86b0
 *   FPM_FSE_free  .text 0x0a8660
 *
 * See dsplib/fpm_fse.h for what the block is.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/sysdep.h"

void
FPM_FSE_init(struct fpm_fse *state, const struct fpm_fse_cfg *cfg, int fresh)
{
	short coeff_bytes;
	short sym_bytes;
	short i;

	state->cfg = *cfg;

	state->lms_force = 0;
	state->pll_on = 1;
	state->tilt_on = 1;
	state->lms_on = 1;
	state->freq = 0;
	state->mu_sel = 0;
	state->pll_sel = 0;
	state->err_avg = 0;
	state->mse = 0;
	state->n_in = 0;
	state->n_out = 0;
	state->widx = 0;
	state->phase_acc = 0;
	state->clk_phase = 0;
	state->tilt_out = 0;
	state->sym_count = 0;
	state->need = 1;

	/*
	 * Zero means "re-init": release the five buffers first.  Non-zero
	 * means the state has never been initialised and the pointers are
	 * garbage, so they are not looked at.  The announcement has no
	 * newline, like `FPM_MRF_init`'s.
	 */
	if (!fresh) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Reallocating FPM_FSE buffers");
		sysdep_free(state->out_q);
		sysdep_free(state->out_i);
		sysdep_free(state->hist);
		sysdep_free(state->qcoeff);
		sysdep_free(state->icoeff);
	}

	/*
	 * Both sizes are computed in 16 bits and only then widened, so a
	 * `taps` above 16383 or a `block / interp` above 16381 wraps.  That
	 * is the object's arithmetic, reproduced rather than corrected.
	 */
	coeff_bytes = (short)(2 * state->cfg.taps);
	state->icoeff = (short *)sysdep_malloc(coeff_bytes);
	state->qcoeff = (short *)sysdep_malloc(coeff_bytes);
	state->hist = (short *)sysdep_malloc(coeff_bytes);

	/*
	 * `block / interp` symbols fit in one call's worth of input; the two
	 * spare entries are what lets a slicer look at out_i[n_out] before
	 * n_out has been advanced.
	 */
	sym_bytes = (short)(2 * (state->cfg.block / state->cfg.interp) + 4);
	state->out_i = (short *)sysdep_malloc(sym_bytes);
	state->out_q = (short *)sysdep_malloc(sym_bytes);

	for (i = 0; i < state->cfg.taps; i++) {
		state->icoeff[i] = state->cfg.icoff[i];
		state->qcoeff[i] = state->cfg.qcoff[i];
		state->hist[i] = 0;
	}

	for (i = 0; i <= 3; i++) {
		state->tilt_coeff[i] = 0;
		state->tilt_hist[i] = 0;
	}

	state->diag_n = 0;
	state->unknown_4e10 = 0;
	state->diag2_n = 0;
}

void
FPM_FSE_free(struct fpm_fse *state)
{
	sysdep_free(state->out_q);
	sysdep_free(state->out_i);
	sysdep_free(state->hist);
	sysdep_free(state->qcoeff);
	sysdep_free(state->icoeff);
}
