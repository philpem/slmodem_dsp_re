/*
 * v22_fse.c -- V.22 / V.22bis: the fractionally spaced equaliser block.
 *
 * Reconstructed from dsplibs.o:
 *   V22_FSE_getdiag  .text   0x08c590     3 bytes
 *   V22_FSE_init     .text   0x08cd00   372 bytes
 *   V22_FSE_free     .text   0x08ce80    90 bytes
 *   V22_FSE_receive  .text   0x08c5a0  1885 bytes
 *   FSEv22_COFFS     .rodata 0x008640    98 bytes
 *   FSEv22_CFG       .rodata 0x0086a4     8 bytes
 *   v22_fse_mu       .rodata 0x008dee     4 bytes, LOCAL
 *
 * See dsplib/v22_fse.h for what the block is.
 */

#include "dsplib/sysdep.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_phasor.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22tab.h"

/*
 * The prototype the datapump hands to `V22_FSE_init`.  Symmetric: entry i and
 * entry 48 - i are equal for every i, which is why the reversal init performs
 * is invisible to any test driven with this table.  Byte-exact from .rodata;
 * no derivation is attempted (docs/fastpass.md defers that to the retarget).
 */
const short FSEv22_COFFS[V22_FSE_TAPS] = {
	    9,     9,    -6,   -23,   -30,    -8,    38,    75,
	   45,   -76,  -223,  -255,   -64,   280,   477,   144,
	 -844, -2032, -2331,  -445,  4405, 11801, 19980, 26405,
	28841, 26405, 19980, 11801,  4405,  -445, -2331, -2032,
	 -844,   144,   477,   280,   -64,  -255,  -223,   -76,
	   45,    75,    38,    -8,   -30,   -23,    -6,     9,
	    9,
};

/*
 * Eight zero bytes in .rodata, with no relocation on either word, so both
 * pointers really are null and the caller patches them.  Kept as a named
 * object rather than folded away because `V22FP_create` loads it by name.
 */
const struct v22_fse_cfg FSEv22_CFG = { 0, 0 };

/*
 * The LMS step size, two entries, indexed by `state->mu_sel`.  LOCAL in the
 * object -- `nm` shows a lowercase `r` -- and read at exactly one place,
 * 0x08ca5d, which is why it is here rather than in v22rxtab.c with the rest of
 * the receiver's tables: a local symbol is a statement about the translation
 * unit, and this is the only function that names it.
 */
static const short v22_fse_mu[V22_FSE_MU] = { 2620, 393 };

/*
 * The gain applied to both FIR branches, 2.1411 in Q14.  An immediate in the
 * object, and applied AFTER the accumulator has been truncated to sixteen
 * bits, not before.
 */
#define V22_FSE_FIR_GAIN	0x8908

/*
 * Both smoothers' weights, 0.95 and 0.05 in Q15.  Each term is shifted down
 * fifteen SEPARATELY and the two are then added, which is not the same as
 * shifting their sum -- the two readings differ by one on about half of all
 * inputs, and the test counts the symbols on which they do.
 */
#define V22_FSE_SMOOTH_OLD	0x799a
#define V22_FSE_SMOOTH_NEW	0x666

/*
 * The PLL gain band edges, against the magnitude of the smoothed phase error:
 * above the first the wide pair, at or below the second the narrow pair, and
 * between them the band stays where it was.  Both tests run, in that order, so
 * a magnitude cannot be in both bands but the code does not rely on it.
 */
#define V22_FSE_ERR_HI		0x1999
#define V22_FSE_ERR_LO		0x666

/*
 * The LMS update's two roundings, each half of the shift below it: the step
 * size is formed in Q12 and the per-tap correction lands in Q19.
 */
#define V22_FSE_MU_ROUND	0x800
#define V22_FSE_MU_SHIFT	12
#define V22_FSE_UPD_ROUND	0x40000
#define V22_FSE_UPD_SHIFT	19

/*
 * Three bytes: `xor %eax,%eax; ret`.  It does not touch `state`; the parameter
 * is declared because `V22FP_GetDiagnostics` is seen to pass one.
 */
int
V22_FSE_getdiag(struct v22_fse *state)
{
	(void)state;
	return 0;
}
/*
 * One block in, one symbol per `V22_FSE_INTERP` samples out.  See the header
 * for the shape of the loop and for what the caller must not do.
 *
 * WHAT IS DELIBERATELY NOT TIDIED HERE:
 *
 *   - the object walks the history window with the whole FIR duplicated, one
 *     copy per side of the `hist_n > 48` test, and reads the window base out
 *     of the same stack slot in the LMS loop afterwards.  One pointer and one
 *     pair of loops is the same arithmetic in every case;
 *   - the phase accumulator is stored twice, once unwrapped and once wrapped.
 *     Written as the object's two statements, since the first store is only
 *     dead if the second is reached and the reading is the same either way;
 *   - `theta` is assigned the re-rotation angle at the end of a symbol and
 *     overwritten by `FPM_atan` at the start of the next.  That dead store is
 *     in the object, and it is what says the two are one variable.
 */
unsigned short
V22_FSE_receive(struct v22_fse *state, const short *in, unsigned short *out,
		short count)
{
	struct fpm_phasor ph;
	short *icoeff = state->icoeff;
	short *qcoeff = state->qcoeff;
	short *hist = state->hist;
	short *oi = state->out_i;
	short *oq = state->out_q;
	short hist_n = state->hist_n;
	short clk_phase = state->clk_phase;
	short need = state->need;
	short left = count;
	short carrier = 0;
	short theta = 0;

	state->n_in = count;
	state->n_out = 0;

	/*
	 * Zero for all three calls below: this block writes the phase itself
	 * every time and never lets the phasor advance.  The object sets it
	 * here and again inside the loop -- the calls in between take its
	 * address, so the second store is not redundant to the compiler.
	 */
	ph.inc = 0;

	while (left != 0) {
		short *win;
		short angle, mag, derot, half;
		short i_val, q_val, amp, perr, err_i, err_q;
		short k;
		int acc;

		/*
		 * Not enough for a whole symbol interval: stash what there is,
		 * charge it against `need` and leave.  A NEGATIVE count reaches
		 * here too and stashes nothing.
		 */
		if (left < need) {
			if (left > 0) {
				need = (short)(need - left);
				if (hist_n + left > V22_FSE_HIST) {
					sysdep_memcpy(hist,
						      &hist[V22_FSE_TAPS],
						      V22_FSE_TAPS
						      * sizeof(short));
					hist_n = (short)(hist_n
							 - V22_FSE_TAPS);
				}
				sysdep_memcpy(&hist[hist_n], in,
					      (unsigned int)left
					      * sizeof(short));
				hist_n = (short)(hist_n + left);
				clk_phase = (short)(clk_phase + 2 * left);
				while (clk_phase > V22_CRR_CLK_STEPS - 1)
					clk_phase = (short)(clk_phase
							    - V22_CRR_CLK_STEPS);
			}
			break;
		}

		/*
		 * The history is linear, so when the next block would pass the
		 * end the top 49 entries move down over the bottom 49.  That is
		 * `V22_FSE_TAPS` entries -- 98 BYTES -- and not the 98 ENTRIES
		 * the bound above is counted in; the object spells both 0x62.
		 */
		if (hist_n + need > V22_FSE_HIST) {
			sysdep_memcpy(hist, &hist[V22_FSE_TAPS],
				      V22_FSE_TAPS * sizeof(short));
			hist_n = (short)(hist_n - V22_FSE_TAPS);
		}
		sysdep_memcpy(&hist[hist_n], in,
			      (unsigned int)need * sizeof(short));
		in += need;
		hist_n = (short)(hist_n + need);

		/*
		 * Two clock steps per input sample, reduced modulo the length
		 * of `CRRv22_CLK`.  The 6 below is that table's length; the one
		 * `need` is set to is the symbol interval.
		 */
		clk_phase = (short)(clk_phase + 2 * need);
		while (clk_phase > V22_CRR_CLK_STEPS - 1)
			clk_phase = (short)(clk_phase - V22_CRR_CLK_STEPS);

		carrier = (short)(CRRv22_CLK[clk_phase] + state->phase);
		left = (short)(left - need);
		need = V22_FSE_INTERP;

		/*
		 * The 49 newest history entries, or the bottom 49 while there
		 * are not yet 49 of them.  The same window drives the LMS
		 * update at the end of the symbol.
		 */
		if (hist_n > V22_FSE_TAPS - 1)
			win = &hist[hist_n - V22_FSE_TAPS];
		else
			win = hist;

		acc = 0;
		for (k = 0; k <= V22_FSE_TAPS - 1; k++)
			acc += icoeff[k] * win[k];
		i_val = (short)(((short)(acc >> 15) * V22_FSE_FIR_GAIN) >> 14);

		acc = 0;
		for (k = 0; k <= V22_FSE_TAPS - 1; k++)
			acc += qcoeff[k] * win[k];
		q_val = (short)(((short)(acc >> 15) * V22_FSE_FIR_GAIN) >> 14);

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
		 * Derotate.  The angle is carried at HALF scale through the
		 * reduction -- one turn is 0x4000 there -- which throws its low
		 * bit away, and it is doubled back for the phasor.  The two
		 * conditional adjustments are not a loop: an angle far enough
		 * out to need two is left where the first put it.
		 */
		half = (short)((theta - carrier) >> 1);
		if (half < 0)
			half = (short)(half + FPM_PHASOR_CYCLE / 2);
		if (half > FPM_PHASOR_CYCLE / 2)
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
		 * The slicer indexes `out_i`/`out_q` with `n_out`, so the
		 * increment has to come after it -- it is reading the pair
		 * written just above.
		 */
		*out++ = state->decision(state, &angle, &mag);
		state->n_out++;

		/* The phase error, wrapped into half a turn either way. */
		perr = (short)(derot - angle);
		if (perr < -FPM_PHASOR_CYCLE / 2)
			perr = (short)(perr - FPM_PHASOR_CYCLE);
		if (perr > FPM_PHASOR_CYCLE / 2)
			perr = (short)(perr - FPM_PHASOR_CYCLE);

		state->err_avg = (short)
		    (((state->err_avg * V22_FSE_SMOOTH_OLD) >> 15)
		     + ((perr * V22_FSE_SMOOTH_NEW) >> 15));

		if (state->pll_on) {
			short sel;

			if (state->sym_count > V22_FSE_TRAIN) {
				short e = state->err_avg;

				e = (short)(e < 0 ? -e : e);
				if (e > V22_FSE_ERR_HI)
					state->pll_sel = 1;
				if (e <= V22_FSE_ERR_LO)
					state->pll_sel = 2;
			} else {
				/*
				 * Training: the integrator is held down and
				 * the band stays 0 until the last of the 49
				 * symbols, which leaves it at 1.
				 */
				short n = state->sym_count;

				state->freq = 0;
				state->sym_count = (short)(n + 1);
				if (n < V22_FSE_TRAIN)
					state->pll_sel = 0;
				else
					state->pll_sel = 1;
			}

			sel = state->pll_sel;
			state->phase = (short)
			    (state->phase
			     + ((CRRv22_PLL_K1[sel] * perr) >> 15)
			     + state->freq);
			state->freq = (short)
			    (state->freq
			     + ((CRRv22_PLL_K2[sel] * perr) >> 15));
			if (state->phase < -FPM_PHASOR_CYCLE / 2)
				state->phase = (short)(state->phase
						       - FPM_PHASOR_CYCLE);
			if (state->phase > FPM_PHASOR_CYCLE / 2)
				state->phase = (short)(state->phase
						       - FPM_PHASOR_CYCLE);
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
		if (half > FPM_PHASOR_CYCLE / 2)
			half = (short)(half - FPM_PHASOR_CYCLE / 2);
		theta = (short)(half * 2);
		ph.phase = (unsigned short)theta;
		FPM_phasor(&ph);

		err_i = (short)(((ph.cos * mag) >> 14) - i_val);
		err_q = (short)(((mag * ph.sin) >> 14) - q_val);

		/*
		 * The squared error, accumulated in a plain int -- two shorts
		 * squared and added can pass 2^31, and the object does nothing
		 * about it.
		 */
		state->mse = (short)
		    (((state->mse * V22_FSE_SMOOTH_OLD) >> 15)
		     + ((((short)((err_i * err_i + err_q * err_q) >> 15))
			 * V22_FSE_SMOOTH_NEW) >> 15));

		/*
		 * The gate is on the SMOOTHED error being strictly positive,
		 * which a real signal never fails -- the sum of two squares
		 * cannot pull it down and the 0.95 term cannot cross zero from
		 * above.  Only a caller that seeds `mse` negative gets here.
		 */
		if (state->mse > 0 && state->lms_on) {
			short mu = v22_fse_mu[state->mu_sel];
			short step_i = (short)((err_i * mu + V22_FSE_MU_ROUND)
					       >> V22_FSE_MU_SHIFT);
			short step_q = (short)((err_q * mu + V22_FSE_MU_ROUND)
					       >> V22_FSE_MU_SHIFT);

			for (k = 0; k <= V22_FSE_TAPS - 1; k++) {
				icoeff[k] = (short)
				    (((step_i * win[k] + V22_FSE_UPD_ROUND)
				      >> V22_FSE_UPD_SHIFT) + icoeff[k]);
				qcoeff[k] = (short)
				    (((step_q * win[k] + V22_FSE_UPD_ROUND)
				      >> V22_FSE_UPD_SHIFT) + qcoeff[k]);
			}
		}
	}

	state->clk_phase = clk_phase;
	state->need = need;
	state->hist_n = hist_n;
	return (unsigned short)state->n_out;
}

void
V22_FSE_init(struct v22_fse *state, const struct v22_fse_cfg *cfg, int fresh)
{
	short i;

	state->icoff = cfg->icoff;
	state->qcoff = cfg->qcoff;

	state->mu_sel = 0;
	state->pll_sel = 0;
	state->freq = 0;
	state->r10 = 0;
	state->pll_on = 1;
	state->r18 = 1;
	state->lms_on = 1;
	state->err_avg = 0;
	state->mse = 0;
	state->n_in = 0;
	state->n_out = 0;
	state->hist_n = 0;
	state->phase = 0;
	state->clk_phase = 0;
	state->r42 = 0;
	state->sym_count = 0;
	state->need = 1;

	/*
	 * Non-zero means the buffers do not exist yet.  Zero reuses them
	 * without inspecting them, which is why nothing here frees: see the
	 * note in the header about how this differs from `FPM_FSE_init`.
	 *
	 * Every size is a literal in the object -- 0x62, 0x62, 0xc4, 0x28,
	 * 0x28, 0x1c, 0x1c -- and none of them is derived from `cfg`.
	 */
	if (fresh) {
		state->icoeff = sysdep_malloc(2 * V22_FSE_TAPS);
		state->qcoeff = sysdep_malloc(2 * V22_FSE_TAPS);
		state->hist = sysdep_malloc(2 * V22_FSE_HIST);
		state->r44 = sysdep_malloc(2 * V22_FSE_AUX);
		state->r48 = sysdep_malloc(2 * V22_FSE_AUX);
		state->out_i = sysdep_malloc(2 * V22_FSE_OUT);
		state->out_q = sysdep_malloc(2 * V22_FSE_OUT);
	}

	/*
	 * The working coefficients are the configuration's arrays REVERSED and
	 * arithmetically shifted down two.  The reversal is what turns a
	 * causal prototype into the convolution order the filter walks; the
	 * shift is head-room for the LMS update, which adds to these in place.
	 *
	 * `hist` is zeroed here for its first 49 entries and again below for
	 * all 98.  The overlap is the object's, reproduced rather than tidied.
	 */
	for (i = 0; i <= V22_FSE_TAPS - 1; i++) {
		state->icoeff[V22_FSE_TAPS - 1 - i] = (short)(state->icoff[i] >> 2);
		state->qcoeff[V22_FSE_TAPS - 1 - i] = (short)(state->qcoff[i] >> 2);
		state->hist[i] = 0;
	}

	for (i = 0; i <= V22_FSE_HIST - 1; i++)
		state->hist[i] = 0;
}

void
V22_FSE_free(struct v22_fse *state)
{
	sysdep_free(state->out_q);
	sysdep_free(state->out_i);
	sysdep_free(state->r48);
	sysdep_free(state->r44);
	sysdep_free(state->hist);
	sysdep_free(state->qcoeff);
	sysdep_free(state->icoeff);
}
