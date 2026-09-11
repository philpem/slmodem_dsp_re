/*
 * fpm_fse.c -- Fixed Point Modem: Fractionally Spaced Equaliser.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_FSE_receive  .text 0x0a7e00  2131
 *   FPM_FSE_free     .text 0x0a8660    68
 *   FPM_FSE_init     .text 0x0a86b0
 *   avg_err_show.0   .bss  0x0008d0     4   (LOCAL, function-scope static)
 *
 *   FSE_getdiag      .text 0x0a7d10   229
 *
 * The four are written in the object's own emission order: `FSE_getdiag` at
 * 0x0a7d10 is the translation unit's first symbol, then receive, free, init.
 *
 * See dsplib/fpm_fse.h for what the block is.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_phasor.h"
#include "dsplib/sysdep.h"

/*
 * Both smoothers' weights, 0.95 and 0.05 in Q15, and the same pair V.22's
 * copy of this block uses.  Each term is shifted down fifteen SEPARATELY and
 * the two are then added, which is not the same as shifting their sum.
 */
#define FPM_FSE_SMOOTH_OLD	0x799a
#define FPM_FSE_SMOOTH_NEW	0x666

/*
 * The carrier phase accumulator is a full cycle of `FPM_PHASOR_CYCLE` scaled
 * up by 2^15, so 0x40000000 is one turn and 0x20000000 is the half turn the
 * PLL wraps into.  `FPM_FSE_PHASE_ROUND` is half of the last bit the shift
 * throws away, which makes reading the carrier out a round-to-nearest.
 */
#define FPM_FSE_PHASE_SHIFT	15
#define FPM_FSE_PHASE_ROUND	0x4000
#define FPM_FSE_PHASE_CYCLE	0x40000000
#define FPM_FSE_PHASE_HALF	0x20000000

/*
 * The squared decision error is brought down eleven bits before it is
 * smoothed -- V.22's copy of the same statement uses fifteen, and this is one
 * of the places the two blocks genuinely differ.
 */
#define FPM_FSE_MSE_SHIFT	11
#define FPM_FSE_MSE_MAX		0x7fff

/*
 * The LMS step size is formed in Q12 with a round-to-nearest; `FPM_lmsupd`
 * does the second rounding, at 18 fractional bits.
 */
#define FPM_FSE_MU_ROUND	0x800
#define FPM_FSE_MU_SHIFT	12

/*
 * How many input samples go by between two `Decoder Error` lines.  Counted in
 * SAMPLES, added once per call, and tested once per symbol, so the report
 * comes out on the first symbol of the call that passes the total -- 7200
 * samples is 0.75 s at 9600, or three V.32 blocks of 144 at 3 samples a
 * symbol times sixteen.  Nothing else in the block reads it.
 */
#define FPM_FSE_SHOW_SAMPLES	0x1c1f

/*
 * Drain one of the two scatter logs into `out` and report how many points came
 * out.  A `which` that is neither 0 nor 1 copies nothing and reports none.
 *
 * THE TWO ARMS ARE NOT SYMMETRICAL, and both asymmetries are the object's:
 *
 *   - `diag` is discarded outright when it holds more than FPM_FSE_DIAG - 1,
 *     which `FPM_FSE_receive` can leave it holding.  `diag2` has no such
 *     guard.
 *   - `diag` is emptied AFTER the copy and `diag2` BEFORE it.  Neither order
 *     is observable unless `out` overlaps the block, which no caller in the
 *     object arranges; the object's order is written and no claim is made
 *     that it matters.
 *
 * Both counts are held down to `max` the same way, and the return is the
 * number actually copied and not the number that was waiting.
 */
int
FSE_getdiag(struct fpm_fse *state, int which, struct fpm_fse_point *out,
	    int max)
{
	int n, i;

	switch (which) {
	case 0:
		n = state->diag_n;
		if (n > FPM_FSE_DIAG - 1) {
			state->diag_n = 0;
			return 0;
		}
		if (n > max)
			n = max;
		for (i = 0; i < n; i++) {
			out[i].i = state->diag[i].i;
			out[i].q = state->diag[i].q;
		}
		state->diag_n = 0;
		return n;

	case 1:
		n = state->diag2_n;
		if (n > max)
			n = max;
		state->diag2_n = 0;
		for (i = 0; i < n; i++) {
			out[i].i = state->diag2[i].i;
			out[i].q = state->diag2[i].q;
		}
		return n;
	}

	return 0;
}

/*
 * One block in, one symbol per `cfg.interp` samples out.  `count` samples are
 * read from `in`, one decoded symbol per symbol interval is written to `out`,
 * and the equaliser's own I/Q pair for each of those symbols is left in
 * `state->out_i` / `state->out_q` for the slicer to have looked at.  The
 * return is the number of symbols produced, which is also `state->n_out`.
 *
 * WHAT IS DELIBERATELY NOT TIDIED HERE:
 *
 *   - the sample-stashing loop and the clock-phase reduction appear twice,
 *     once for a whole symbol interval and once for the short tail.  The
 *     object has both copies and they are not identical -- the tail charges
 *     what it took against `need` and then leaves;
 *   - `theta` is assigned the re-rotation angle at the end of a symbol and
 *     overwritten by `FPM_atan` at the start of the next.  That dead store is
 *     in the object, and it is what says the two are one variable;
 *   - `state->tilt_out` is zeroed and then immediately overwritten by the
 *     4-tap filter's own result.  The object stores both, and the zero is not
 *     dead to the compiler because the accumulator between them is a local.
 *
 * THE LOOP COUNTERS ARE TWO TYPES AND THAT IS MEASURED, NOT STYLE.  The two
 * FIR walks decrement a full 32-bit register and branch on `jns`; the sample
 * stash, the tilt shift and the tilt MAC all re-narrow with `movswl` on every
 * iteration.  `FPM_lmsupd`, whose walk is the same shape as the FIR's, is the
 * control: it is written with a `short` and the object narrows it, so the
 * difference is the declaration and not the loop.
 */
unsigned short
FPM_FSE_receive(struct fpm_fse *state, const short *in, unsigned short *out,
		unsigned short count)
{
	/*
	 * `avg_err_show.0` in the object: LOCAL, four bytes of .bss, and read
	 * at exactly the three sites in this function.  The `.0` suffix is
	 * GCC's function-scope-static mangling and is why it has to be
	 * declared here rather than at file scope.  Signed -- the threshold
	 * test below is a `cmpl` with a signed branch.
	 */
	static int avg_err_show;

	struct fpm_phasor ph;
	short *icoeff = state->icoeff;
	short *qcoeff = state->qcoeff;
	short *hist = state->hist;
	short *oi = state->out_i;
	short *oq = state->out_q;
	short widx = state->widx;
	short clk_phase = state->clk_phase;
	short need = state->need;
	short taps = state->cfg.taps;
	short clk_mod = state->cfg.clk_mod;
	short clk_inc = state->cfg.clk_inc;
	short left = (short)count;
	/*
	 * NEITHER IS INITIALISED, and that is measured: the object's prologue
	 * has no zero store for either.  `theta` is written by `FPM_atan`
	 * before anything reads it, and `carrier` before anything reads it in
	 * the same iteration -- so the reads are all defined and the zeroing
	 * V.22's copy of this block does is absent here.
	 */
	short theta;

	state->n_in = count;
	state->n_out = 0;
	avg_err_show += count;

	/*
	 * Zero for all three calls below: this block writes the phase itself
	 * every time and never lets the phasor advance.  The object sets it
	 * here and again inside the loop -- the calls in between take its
	 * address, so the second store is not redundant to the compiler.
	 */
	ph.inc = 0;

	while (left != 0) {
		int i;
		short n, j, k;
		short carrier;
		short angle, mag, amp, derot, half;
		short i_val, q_val, perr, err_i, err_q;
		int acc;

		/*
		 * Not enough for a whole symbol interval: stash what there is,
		 * charge it against `need` and leave.  A NEGATIVE count
		 * reaches here too and stashes nothing.
		 */
		if (left < need) {
			if (left > 0) {
				need = (short)(need - left);
				for (n = left; n != 0; n--) {
					widx = (short)(widx + 1);
					if (widx >= taps)
						widx = 0;
					hist[widx] = *in++;
				}
				clk_phase = (short)(clk_phase
						    + left * clk_inc);
				while (clk_phase >= clk_mod)
					clk_phase = (short)(clk_phase
							    - clk_mod);
			}
			break;
		}

		/*
		 * The history is CIRCULAR, unlike V.22's copy of this block:
		 * `widx` is the newest entry and wraps at `taps`, so nothing
		 * is ever moved down.
		 */
		for (n = need; n != 0; n--) {
			widx = (short)(widx + 1);
			if (widx >= taps)
				widx = 0;
			hist[widx] = *in++;
		}
		clk_phase = (short)(clk_phase + need * clk_inc);
		while (clk_phase >= clk_mod)
			clk_phase = (short)(clk_phase - clk_mod);

		/*
		 * The nominal carrier for this symbol, plus whatever the PLL
		 * has accumulated.  `clk` is indexed by the reduced phase and
		 * holds the nominal advance per clock step.
		 */
		carrier = (short)(state->cfg.clk[clk_phase]
				  + ((state->phase_acc + FPM_FSE_PHASE_ROUND)
				     >> FPM_FSE_PHASE_SHIFT));
		left = (short)(left - need);
		need = state->cfg.interp;

		/*
		 * One complex FIR over the circular history: newest sample
		 * against `icoeff[0]`, then down to the base of the buffer and
		 * round from the top.  The coefficient index runs straight
		 * through both halves, which is the pairing `FPM_lmsupd`
		 * assumes when it adapts them.
		 *
		 * There is no gain stage here -- V.22 multiplies by 2.1411 in
		 * Q14 and this one simply doubles.
		 */
		acc = 0;
		k = 0;
		for (i = widx; i >= 0; i--)
			acc += icoeff[k++] * hist[i];
		for (i = taps - 1; i > widx; i--)
			acc += icoeff[k++] * hist[i];
		i_val = (short)(2 * (short)(acc >> 15));

		acc = 0;
		k = 0;
		for (i = widx; i >= 0; i--)
			acc += qcoeff[k++] * hist[i];
		for (i = taps - 1; i > widx; i--)
			acc += qcoeff[k++] * hist[i];
		q_val = (short)(2 * (short)(acc >> 15));

		/*
		 * To polar, and `amp` is the projection of the pair onto its
		 * own angle -- which is its magnitude, since that is where
		 * `FPM_atan` just put the angle.
		 */
		FPM_atan(q_val, i_val, &theta);
		ph.phase = (unsigned short)theta;
		FPM_phasor(&ph);
		amp = (short)((ph.cos * i_val + ph.sin * q_val) >> 15);

		/*
		 * Derotate, biased by the tilt filter's last output.  The
		 * angle is carried at HALF scale through the reduction -- one
		 * turn is 0x4000 there -- which throws its low bit away, and
		 * it is doubled back for the phasor.  Both adjustments are
		 * `>=` here where V.22's are `>`; the object spells the upper
		 * test `cmp $0x3fff` with a `jle` past it.
		 */
		half = (short)(((theta - carrier) >> 1) - state->tilt_out);
		if (half < 0)
			half = (short)(half + FPM_PHASOR_CYCLE / 2);
		if (half >= FPM_PHASOR_CYCLE / 2)
			half = (short)(half - FPM_PHASOR_CYCLE / 2);
		ph.inc = 0;
		derot = (short)(half * 2);
		ph.phase = (unsigned short)derot;
		FPM_phasor(&ph);

		/*
		 * Back to rectangular at the derotated angle, and note the
		 * shift is 13 and not 15: the pair the slicer sees is four
		 * times the magnitude the FIR produced.
		 */
		*oi++ = (short)((ph.cos * amp) >> 13);
		*oq++ = (short)((amp * ph.sin) >> 13);

		/*
		 * The scatter log, at half the output scale.  Full means
		 * RESET rather than wrap: the entry that would have gone in
		 * at 480 is dropped and the next call starts again at 0.
		 */
		if (state->diag_n > FPM_FSE_DIAG - 1) {
			state->diag_n = 0;
		} else {
			state->diag[state->diag_n].i = oi[-1] >> 1;
			state->diag[state->diag_n].q = oq[-1] >> 1;
			state->diag_n++;
		}

		/*
		 * The slicer's two arguments are IN/OUT, not out: it is handed
		 * the measured angle and magnitude and overwrites them with
		 * the constellation point's.  V.22's copy of this block passes
		 * both uninitialised, so this is a real difference and not a
		 * reading of the same code.
		 */
		angle = derot;
		mag = amp;
		*out++ = state->cfg.decision(state, &angle, &mag);
		state->n_out++;

		/* The phase error, wrapped into half a turn either way. */
		perr = (short)(derot - angle);
		if (perr < -FPM_PHASOR_CYCLE / 2)
			perr = (short)(perr - FPM_PHASOR_CYCLE);
		if (perr > FPM_PHASOR_CYCLE / 2)
			perr = (short)(perr - FPM_PHASOR_CYCLE);

		state->err_avg = (short)
		    (((state->err_avg * FPM_FSE_SMOOTH_OLD) >> 15)
		     + ((perr * FPM_FSE_SMOOTH_NEW) >> 15));

		if (state->pll_on) {
			short sel;

			if (state->sym_count > state->cfg.train_sym) {
				short e = state->err_avg;

				e = (short)(e < 0 ? -e : e);
				/*
				 * Both edges are inclusive, and both tests
				 * run: a magnitude cannot be in both bands
				 * but the code does not rely on it.  The
				 * object compares the CONFIGURATION against
				 * the error rather than the other way round.
				 */
				if (state->cfg.err_hi <= e)
					state->pll_sel = 1;
				if (state->cfg.err_lo >= e)
					state->pll_sel = 2;
			} else {
				/*
				 * Training: the band stays 0 until the last
				 * of the `train_sym` symbols, which leaves it
				 * at 1.  Unlike V.22 the integrator is NOT
				 * held down here -- nothing writes `freq`
				 * before the update below.
				 */
				short m = state->sym_count;

				state->sym_count = (short)(m + 1);
				if (m < state->cfg.train_sym)
					state->pll_sel = 0;
				else
					state->pll_sel = 1;
			}

			/*
			 * A plain second-order loop with no scaling on either
			 * gain: the accumulator carries the fifteen extra bits
			 * the carrier read above shifts back out.
			 */
			sel = state->pll_sel;
			state->phase_acc = state->phase_acc
			    + state->cfg.pll_k1[sel] * perr + state->freq;
			state->freq = state->freq
			    + state->cfg.pll_k2[sel] * perr;
			if (state->phase_acc < -FPM_FSE_PHASE_HALF)
				state->phase_acc += FPM_FSE_PHASE_CYCLE;
			if (state->phase_acc > FPM_FSE_PHASE_HALF)
				state->phase_acc -= FPM_FSE_PHASE_CYCLE;

			/*
			 * A 4-tap FIR over recent phase errors, nested inside
			 * the PLL gate -- with `pll_on` clear the object jumps
			 * straight past this test to the re-rotation.  The
			 * newest entry is the error PLUS the filter's own last
			 * output, so the block is recursive through
			 * `tilt_out`, and `tilt_coeff` is left zero by init:
			 * until a datapump writes it this computes zero and
			 * the derotation bias above is inert.
			 */
			if (state->tilt_on) {
				for (j = 3; j > 0; j--)
					state->tilt_hist[j] =
					    state->tilt_hist[j - 1];
				state->tilt_hist[0] = (short)(perr
							      + state->tilt_out);
				state->tilt_out = 0;
				for (j = 0; j <= 3; j++)
					state->tilt_out = (short)
					    (state->tilt_out
					     + state->tilt_coeff[j]
					       * state->tilt_hist[j]);
			}
		}

		/*
		 * The decision, rotated BACK into the equaliser's own frame,
		 * against the FIR output: that difference is what the taps
		 * adapt on.  Same half-scale reduction as the derotation, and
		 * the shift here is 14 rather than the 13 the outputs use.
		 */
		half = (short)((angle + carrier) >> 1);
		if (half < 0)
			half = (short)(half + FPM_PHASOR_CYCLE / 2);
		if (half >= FPM_PHASOR_CYCLE / 2)
			half = (short)(half - FPM_PHASOR_CYCLE / 2);
		theta = (short)(half * 2);
		ph.phase = (unsigned short)theta;
		FPM_phasor(&ph);

		err_i = (short)(((ph.cos * mag) >> 14) - i_val);
		err_q = (short)(((mag * ph.sin) >> 14) - q_val);

		/*
		 * The smoothed squared error, saturated -- and the object's
		 * test is UNSIGNED, so a negative smoothed error saturates
		 * HIGH rather than being clamped at zero.  It can be negative:
		 * the sum of two squares is truncated to sixteen bits by the
		 * cast below before it is weighted.  Three spellings of the
		 * source produce this instruction -- an `unsigned int`
		 * accumulator, a `(unsigned)` cast in the test, and
		 * `m < 0 || m > 0x7fff` -- and nothing in the object separates
		 * them; this one is a choice.
		 */
		{
			unsigned int m =
			    ((state->mse * FPM_FSE_SMOOTH_OLD) >> 15)
			    + ((((short)((err_i * err_i + err_q * err_q)
					 >> FPM_FSE_MSE_SHIFT))
				* FPM_FSE_SMOOTH_NEW) >> 15);

			if (m > FPM_FSE_MSE_MAX)
				state->mse = FPM_FSE_MSE_MAX;
			else
				state->mse = (short)m;
		}

		/*
		 * `lms_force` is an OR and not an AND: it drives the update
		 * through a `mse` that has gone non-positive, which is the
		 * only way that gate can fail on a real signal.
		 */
		if ((state->mse > 0 && state->lms_on) || state->lms_force) {
			short mu = state->cfg.mu[state->mu_sel];

			FPM_lmsupd(icoeff, hist, widx, taps,
				   (short)((err_i * mu + FPM_FSE_MU_ROUND)
					   >> FPM_FSE_MU_SHIFT));
			FPM_lmsupd(qcoeff, hist, widx, taps,
				   (short)((err_q * mu + FPM_FSE_MU_ROUND)
					   >> FPM_FSE_MU_SHIFT));
		}

		/*
		 * The counter is charged in samples once per call and tested
		 * once per symbol, so it is reset by the FIRST symbol of the
		 * call that passes the threshold -- and reset whether or not
		 * the level was high enough to print.
		 */
		if (avg_err_show > FPM_FSE_SHOW_SAMPLES) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Decoder Error = %d\n",
						     state->mse);
			avg_err_show = 0;
		}
	}

	state->widx = widx;
	state->clk_phase = clk_phase;
	state->need = need;
	return state->n_out;
}

void
FPM_FSE_init(struct fpm_fse *state, const struct fpm_fse_cfg *cfg, int fresh)
{
#ifdef DSPLIB_REPRODUCE_BUGS
	short coeff_bytes;
	short sym_bytes;
#endif
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

	/* The blob narrows both allocation sizes; preserve that only on the
	 * differential reconstruction path. */
#ifdef DSPLIB_REPRODUCE_BUGS
	coeff_bytes = 2 * state->cfg.taps;
	state->icoeff = sysdep_malloc(coeff_bytes);
	state->qcoeff = sysdep_malloc(coeff_bytes);
	state->hist = sysdep_malloc(coeff_bytes);

	/*
	 * `block / interp` symbols fit in one call's worth of input; the two
	 * spare entries are what lets a slicer look at out_i[n_out] before
	 * n_out has been advanced.
	 */
	sym_bytes = 2 * (state->cfg.block / state->cfg.interp) + 4;
	state->out_i = sysdep_malloc(sym_bytes);
	state->out_q = sysdep_malloc(sym_bytes);
#else
	unsigned taps = state->cfg.taps;
	unsigned symbols = state->cfg.block / state->cfg.interp;

	state->icoeff = sysdep_malloc(2U * taps);
	state->qcoeff = sysdep_malloc(2U * taps);
	state->hist = sysdep_malloc(2U * taps);

	state->out_i = sysdep_malloc(2U * symbols + 4U);
	state->out_q = sysdep_malloc(2U * symbols + 4U);
#endif

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
