/*
 * fpm_fse.h -- Fixed Point Modem: Fractionally Spaced Equaliser.
 *
 * The library's passband adaptive equaliser and carrier recovery, shared by
 * more than one datapump: `FSEv32_CFG` configures it for V.32/V.32bis, and
 * the object also holds `FSEv17_*`, `FSEv22_CFG` and a `FPM_FSE_CFG` default,
 * exactly as `fpm_mrf.h`'s block does.
 *
 * WHAT THE BLOCK DOES, from `FPM_FSE_receive`:
 *
 *   - a complex FIR over a circular history of `taps` input samples, with two
 *     coefficient sets (`icoff`, `qcoff`) that are ADAPTED in place by
 *     `FPM_lmsupd`, producing one I/Q pair per `interp` input samples;
 *   - a phase index that steps by `clk_inc` modulo `clk_mod` per input sample
 *     and looks a nominal carrier phase up in `clk[]`;
 *   - `FPM_atan` / `FPM_phasor` to derotate that pair by the recovered
 *     carrier;
 *   - a caller-supplied SLICER (`cfg.decision`) that turns the derotated
 *     point into a symbol and reports back the ideal angle and magnitude;
 *   - a second-order PLL over the slicer's angle error, whose two gains are
 *     indexed out of `pll_k1[]` / `pll_k2[]` by an error-magnitude band.
 *
 * The V.32 coefficient tables interleave: `FSEv32_ICOFF` is non-zero only at
 * even indices and `FSEv32_QCOFF` only at odd ones, which is what makes one
 * 103-tap walk over one history produce both halves of an analytic signal.
 */

#ifndef DSPLIB_FPM_FSE_H
#define DSPLIB_FPM_FSE_H

struct fpm_fse;

/*
 * The slicer.  `angle` is in/out: the caller writes the measured angle and
 * the slicer overwrites it with the constellation point's ideal angle, so the
 * difference is the phase error the PLL runs on.  `mag` is out only.  The
 * return value is the decoded symbol; every slicer in the object returns it
 * zero-extended from 16 bits.
 */
typedef unsigned short (*fpm_fse_decision)(struct fpm_fse *state,
					   short *angle, short *mag);

/*
 * Configuration, 56 bytes, copied wholesale into the state by init.
 *
 * `owner` and `decision` are zero in every static instance in the object:
 * like `fpm_mrf_cfg::coeff`, a caller copies the static onto the stack and
 * patches them before calling init.  That is why the slicer is chosen by
 * code and not by this table.
 */
struct fpm_fse_cfg {
	short block;		/* +0x00 max input samples per receive call  */
	short interp;		/* +0x02 input samples per symbol            */
	const short *icoff;	/* +0x04 initial I coefficients, `taps`      */
	const short *qcoff;	/* +0x08 initial Q coefficients, `taps`      */
	short taps;		/* +0x0c                                     */
	/*
	 * LMS step size, indexed by `fpm_fse::mu_sel`.  Three entries: the
	 * bound is not stated in the object, but +0x14 is the next field, so
	 * three is all there is room for.
	 */
	short mu[3];		/* +0x0e                                     */
	const short *clk;	/* +0x14 nominal carrier phase per clk step  */
	short clk_mod;		/* +0x18 `clk` has this many entries         */
	short clk_inc;		/* +0x1a phase step per input sample         */
	short train_sym;	/* +0x1c symbols before the PLL narrows      */
	short err_hi;		/* +0x1e >= this reselects the wide gain     */
	short err_lo;		/* +0x20 <= this reselects the narrow gain   */
	short pad22;		/* +0x22 zero in every instance              */
	const short *pll_k1;	/* +0x24 3 proportional gains                */
	const short *pll_k2;	/* +0x28 3 integral gains                    */
	void *owner;		/* +0x2c passed to the slicer as its context */
	fpm_fse_decision decision;	/* +0x30                             */
	void *reserved34;	/* +0x34 zero in every instance              */
};

/*
 * One entry of the diagnostic scatter log.  `FSE_getdiag` copies these out
 * and resets the count, so they are a constellation display and nothing the
 * datapump itself reads back.
 */
struct fpm_fse_point {
	int i;
	int q;
};

/* The two logs' capacities, from `FPM_FSE_receive`'s bound and `FSE_getdiag`'s
 * base offsets: 0x8c..0xf8c is 480 entries and 0xf8c..0x4e0c is 2000. */
#define FPM_FSE_DIAG	480
#define FPM_FSE_DIAG2	2000

struct fpm_fse {
	struct fpm_fse_cfg cfg;	/* +0x00 copied wholesale by init            */
	short mu_sel;		/* +0x38 index into cfg.mu[]                 */
	short pll_sel;		/* +0x3a index into cfg.pll_k1[]/pll_k2[]    */
	int freq;		/* +0x3c PLL integrator                      */
	int lms_force;		/* +0x40 adapt even when the gate below says
				 *       no; zeroed by init                  */
	int pll_on;		/* +0x44 run carrier recovery, init 1        */
	int tilt_on;		/* +0x48 run the 4-tap error filter, init 1  */
	int lms_on;		/* +0x4c adapt the coefficients, init 1      */
	short err_avg;		/* +0x50 smoothed phase error                */
	short mse;		/* +0x52 smoothed squared decision error     */
	short *out_i;		/* +0x54 one entry per symbol this call      */
	short *out_q;		/* +0x58                                     */
	unsigned short n_in;	/* +0x5c samples handed to the last call     */
	unsigned short n_out;	/* +0x5e symbols it produced                  */
	short *icoeff;		/* +0x60 working copy of cfg.icoff, adapted  */
	short *qcoeff;		/* +0x64                                     */
	short *hist;		/* +0x68 circular input history, `taps`      */
	short widx;		/* +0x6c newest entry of `hist`              */
	short pad6e;		/* +0x6e never written by init or receive    */
	int phase_acc;		/* +0x70 PLL phase accumulator               */
	short clk_phase;	/* +0x74 index into cfg.clk[]                */
	short tilt_out;		/* +0x76 the 4-tap filter's last output      */
	/*
	 * A 4-tap FIR over recent phase errors whose output biases the
	 * derotation.  Init zeroes the coefficients and nothing in this block
	 * writes them, so it is inert until the datapump sets them.
	 *
	 * UNVERIFIED SHAPE: both are read `movzwl` and every use is truncated
	 * back to 16 bits, so the object does not say whether they are signed.
	 */
	short tilt_coeff[4];	/* +0x78                                     */
	short tilt_hist[4];	/* +0x80                                     */
	short sym_count;	/* +0x88 symbols since init, held at
				 *       cfg.train_sym                       */
	short need;		/* +0x8a input samples owed before the next
				 *       symbol; init 1, then cfg.interp     */
	struct fpm_fse_point diag[FPM_FSE_DIAG];	/* +0x008c           */
	struct fpm_fse_point diag2[FPM_FSE_DIAG2];	/* +0x0f8c           */
	int diag_n;		/* +0x4e0c                                   */
	int unknown_4e10;	/* +0x4e10 zeroed by init, read by nothing
				 *         reconstructed so far              */
	int diag2_n;		/* +0x4e14                                   */
};

/*
 * `fresh` non-zero means the state is uninitialised: allocate without
 * inspecting the five existing buffers.  Zero means re-init, which frees all
 * five and allocates again unconditionally -- unlike `FPM_MRF_init`, there is
 * no reuse path.
 */
void FPM_FSE_init(struct fpm_fse *state, const struct fpm_fse_cfg *cfg,
		  int fresh);
void FPM_FSE_free(struct fpm_fse *state);

#endif /* DSPLIB_FPM_FSE_H */
