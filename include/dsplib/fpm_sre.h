/*
 * fpm_sre.h -- Fixed Point Modem: Symbol-timing REcovery.
 *
 * The library's generic interpolating resampler and timing PLL in one pass.
 * `v22_sre.c` is the author's own SPECIALISATION of this block for V.22 --
 * the same three-section discriminant, the same gear-shifted loop filter,
 * the same ten polyphase branches and the same asymmetric [-18432, +20480]
 * step clamp -- with every table and threshold folded in as a literal.  This
 * one takes all of them from a configuration struct, which is why it is 403
 * bytes larger.  Read `include/dsplib/v22_sre.h` for the block's shape stated
 * at length; this header records only what differs, and what the generic form
 * carries that the V.22 one does not.
 *
 * FOUR THINGS DIFFER, AND EACH IS A REAL DIFFERENCE IN BEHAVIOUR:
 *
 *   1. THE HISTORY IS CIRCULAR, NOT SLIDING.  V.22 keeps 2 * taps entries and
 *      memcpy's the top half down every taps samples; this keeps exactly
 *      `taps` and wraps `fill` to zero, so the dot product is split into two
 *      runs -- hist[fill] down to hist[0], then hist[taps-1] down to
 *      hist[fill+1] -- newest first either way.
 *   2. THE PROTOTYPE IS NOT PERMUTED.  V22_SRE_init de-interleaves the taps
 *      into ten contiguous branches; init here copies the prototype in design
 *      order and the dot product SELECTS branch `b` by starting at coeff[b]
 *      and striding ten.  Same filter, no permutation, and it is why the
 *      interpolator is a single flat walk rather than a nested one.
 *   3. THERE IS A LEVEL GATE IN FRONT OF THE LOOP.  While `rms_on` is set,
 *      every input sample is also written to a `cfg.rms_len`-entry ring and
 *      the discriminant is FORCED TO ZERO -- the PLL runs on nothing.  The
 *      first block whose FPM_rms exceeds `cfg.rms_min` clears `rms_on` for
 *      good and the loop starts.  V.22 has no such gate.
 *   4. IT MEASURES AND REPORTS A TIMING OFFSET.  See the `ppm_*` fields.
 *
 * AND ONE THING IS THE SAME AND SHOULD NOT BE ASSUMED TO BE: the smoothers.
 * V.22 indexes SRE_ALPHA_AVG / SRE_BETA_AVG by mode; this block hard-codes
 * 15/16 for both, with no rounding and no table.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_SRE_recover  .text   0x0a9e90  2286 B
 *   FPM_SRE_init     .text   0x0aa7c0   574 B
 *   FPM_SRE_free     .text   0x0aa780    57 B
 *   FPM_SRE_CFG      .rodata 0x00c4e0    56 B
 *   SREv32_COFFS     .rodata 0x006ea0   362 B
 *   SREv32_PLL_K2    .data   0x007600     6 B
 *   SREv32_PLL_K1    .data   0x007606     6 B
 *   SREv32_XB_COFFS  .data   0x00760c    22 B
 *   SREv32_yCLOCK    .data   0x007622     6 B
 *   SREv32_xCLOCK    .data   0x007628     6 B
 */

#ifndef DSPLIB_FPM_SRE_H
#define DSPLIB_FPM_SRE_H

/*
 * Ten polyphase branches, and this is MEASURED rather than carried over from
 * V.22: the dot product's coefficient stride is `add $0x14,%esi`, ten shorts,
 * and `FPM_SRE_init` divides the configured coefficient count by ten (the
 * 0x66666667 / `sar $2` pair) to get the tap count.  The phase advances ten
 * branch-steps per output and every wrap past nine is another input sample
 * the next output will need.
 */
#define FPM_SRE_BRANCHES	10

/*
 * Three cascaded second-order sections, each { now, previous }.  The `clk`
 * buffer is a fixed twelve bytes -- `FPM_SRE_init` calls sysdep_malloc(12)
 * with the constant inline, so this is not derived from anything.
 */
#define FPM_SRE_CLOCK		6

/*
 * The discriminant's coefficient table.  Eleven entries, from the eleven
 * distinct offsets `FPM_SRE_recover` reads out of `cfg.disc`, and confirmed
 * independently by `SREv32_XB_COFFS` being 22 bytes.
 */
#define FPM_SRE_DISC		11

/* `frac` is an 11-bit fraction of one branch, exactly as in V.22. */
#define FPM_SRE_FRAC_ONE	0x800

/* `mode` indexes `cfg.pll_k1` and `cfg.pll_k2`; three modes, three entries. */
#define FPM_SRE_MODES		3

/*
 * Configuration, 56 bytes, copied wholesale into the state by init with a
 * 14-dword `rep movsl`.
 *
 * The six pointers are ZERO in `FPM_SRE_CFG`, the built-in instance -- the
 * same shape as `fpm_fse_cfg`'s `owner` and `decision` and `fpm_mrf_cfg`'s
 * `coeff`: a caller copies the static onto the stack and patches the tables
 * in before calling init.  So the built-in is a set of default THRESHOLDS and
 * nothing else, and its numbers are quoted below only to show the scale.
 */
struct fpm_sre_cfg {
	/*
	 * Points in the clock reference -- `tick` runs 0 .. clock_len-1 and
	 * indexes `xclock` and `yclock`.  V.32's tables are three entries, so
	 * it configures three; the built-in says four.
	 *
	 * IT IS ALSO THE PHASE-ERROR GAIN, and that is not a guess about two
	 * fields: both uses read offset +0x00 of the same object.  The update
	 * forms `e = (clock_len * angle) >> 3`, which is the generic form of
	 * V.22's hard-coded `(3 * angle) >> 1` -- there the correlation runs
	 * over six points and the gain is 6/4.
	 */
	short clock_len;	/* +0x00  4                                  */
	short groups_acq;	/* +0x02  1   clock groups per PLL update in
				 *            modes 0 and 1                  */
	short groups_trk;	/* +0x04 24   ... and in mode 2              */
	short settle;		/* +0x06 60   updates in mode 0 before the
				 *            loop shifts to mode 1          */
	/*
	 * What the integrator is scaled by when the gear changes, so that its
	 * resolution follows the update rate.  Down is Q15 and up is Q12; the
	 * two are not reciprocals in the built-in config (1365/32768 is 1/24,
	 * 16384/4096 is 4) and that asymmetry is the object's.
	 */
	short acc_down;		/* +0x08 1365   mode 2 -> 1, >> 15           */
	short acc_up;		/* +0x0a 16384  mode 1 -> 2, >> 12           */
	/*
	 * Total polyphase coefficients: FPM_SRE_BRANCHES * taps.  `taps` is
	 * not carried -- init divides this by ten.  `proto` must hold ONE
	 * MORE than this, because the interpolator reads proto[i+1] at
	 * i == coeffs-1; SREv32_COFFS is 181 for a configured 180.
	 */
	short coeffs;		/* +0x0c 60                                  */
	short pad0e;		/* +0x0e  0 in the built-in instance         */
	const short *proto;	/* +0x10 coeffs + 1 entries                  */
	const short *disc;	/* +0x14 FPM_SRE_DISC entries                */
	const short *xclock;	/* +0x18 clock_len entries                   */
	const short *yclock;	/* +0x1c clock_len entries                   */
	const short *pll_k1;	/* +0x20 FPM_SRE_MODES proportional gains    */
	const short *pll_k2;	/* +0x24 FPM_SRE_MODES integral gains        */
	/*
	 * The squelch, with hysteresis, on the smoothed discriminant
	 * magnitude.  V.22's equivalents are the literals 2 and 1.
	 */
	short mag_hi;		/* +0x28 2130  at or above this the loop runs */
	short mag_lo;		/* +0x2a  164  at or below it, it stops       */
	/*
	 * The gear-shift thresholds, on |err_avg|, and BOTH ARE COMPARED
	 * AFTER `>> 3`.  V.22's equivalents are 0x7ff and 4 compared directly.
	 */
	short err_hi;		/* +0x2c 8192  >> 3 = 1024: leave mode 2      */
	short err_lo;		/* +0x2e 3277  >> 3 =  409: enter mode 2      */
	short rms_min;		/* +0x30 2252  the level gate's threshold     */
	short rms_len;		/* +0x32    9  samples FPM_rms is given       */
	short pad34;		/* +0x34  0 in the built-in instance         */
	short pad36;		/* +0x36  0 in the built-in instance         */
};

/*
 * The state.  144 bytes, and NOTHING BOUNDS THAT: no allocation site for an
 * `fpm_sre` is reconstructed, so unlike `struct v22_sre` -- which V22FP_create
 * embeds at a known +0x128 -- the tail here is bounded only by the highest
 * offset the two functions touch, which is `ppm_first` at +0x8e.  If a
 * datapump is later found that embeds one, check this size against it before
 * trusting a whole-object comparison past +0x90.
 *
 * Everything from `mode` down to `need` is the V.22 block's state with a
 * +0x38 displacement for the configuration copy in front of it, field for
 * field and type for type.  What follows `need` is new.
 */
struct fpm_sre {
	struct fpm_sre_cfg cfg;	/* +0x00 copied wholesale by init            */

	short mode;		/* +0x38 0..2; indexes pll_k1 / pll_k2       */
	short pll_acc;		/* +0x3a loop-filter integrator              */
	short err_avg;		/* +0x3c smoothed phase error, 15/16, signed */
	short mag_avg;		/* +0x3e smoothed |discriminant|; the squelch
				 *       runs on this.  Clamped at 0x7fff and
				 *       never negative -- see the note on the
				 *       LOGICAL shift in src/dsp/fpm_sre.c. */
	int active;		/* +0x40 the squelch let the PLL run         */
	int acquiring;		/* +0x44 1 until mode 2 is entered.  Written
				 *       here, read only by the caller.      */
	int adapt;		/* +0x48 caller's enable for the phase update;
				 *       set by init, never written by
				 *       recover, and it gates the update
				 *       WITHOUT gating the error smoother   */
	short taps;		/* +0x4c cfg.coeffs / FPM_SRE_BRANCHES       */
	short fill;		/* +0x4e newest entry of `hist`, 0 .. taps-1 */
	short *coeff;		/* +0x50 cfg.coeffs entries, in the
				 *       prototype's own order               */
	short *hist;		/* +0x54 `taps` entries, CIRCULAR            */
	short *clk;		/* +0x58 FPM_SRE_CLOCK entries               */
	int acc_x;		/* +0x5c discriminant . cfg.xclock           */
	int acc_y;		/* +0x60 discriminant . cfg.yclock, SUBTRACTED */
	short frac;		/* +0x64 sub-branch phase, 0 .. 0x7ff        */
	short branch;		/* +0x66 polyphase branch, 0 .. 9            */
	short groups;		/* +0x68 clock groups per PLL update         */
	short settle;		/* +0x6a update counter, held at cfg.settle  */
	short group;		/* +0x6c groups since the last update        */
	short tick;		/* +0x6e sample within the group             */
	short need;		/* +0x70 input samples owed before the next
				 *       output; carried across calls        */
	short pad72;		/* +0x72 written by neither function         */
	short *rms_buf;		/* +0x74 cfg.rms_len entries, circular       */
	/*
	 * The level gate.  Set by init and cleared for good by the first
	 * block whose FPM_rms exceeds cfg.rms_min; while it is set the
	 * discriminant is forced to zero, so the PLL cannot lock to noise
	 * before there is a signal.
	 */
	short rms_on;		/* +0x78 init 1                              */
	short rms_idx;		/* +0x7a write position in rms_buf           */

	/*
	 * The timing-offset meter, which has no counterpart in V.22.
	 *
	 * `ppm_offset` is named from the object's own words -- the debug line
	 * is "TimingVxx: Timing Offset [ppm] = %d" and this is its argument,
	 * which is evidence class 1.  THE REST OF THIS GROUP IS USAGE
	 * INFERENCE and is named accordingly: what each one does is stated
	 * below and is measured, what it MEANS is read from the one field
	 * whose meaning the object states.
	 *
	 * How it works.  Ten branch-steps per output is one input sample at
	 * the nominal rate, so the number of inputs the next output needs is
	 * normally 1.  `ppm_slip` counts 2 as +1 and 0 as -1, which makes it
	 * the net number of samples the recovered clock has gained over the
	 * nominal one.  Every `ppm_period` worth of `ppm_step` -- both set by
	 * the caller, not by init -- the meter closes an interval, folds
	 * `ppm_slip * ppm_scale` into a running total and republishes the
	 * mean.  Once `ppm_n` reaches `ppm_n_max` the average restarts FROM
	 * ITS OWN MEAN rather than from zero, so it decays instead of
	 * freezing.
	 *
	 * FOUR OF THESE ARE NEVER WRITTEN BY init -- `ppm_step`, `ppm_scale`,
	 * `ppm_period` and `ppm_n_max` are read-only to both functions, so a
	 * caller has to fill them and an all-zero state divides by zero in
	 * `ppm_n`... which init does set, to 1.
	 */
	short ppm_step;		/* +0x7c added to ppm_count once per call
				 *       that drains its input exactly       */
	short ppm_count;	/* +0x7e reset when it reaches ppm_period    */
	short ppm_acc;		/* +0x80 running total of slip * scale       */
	short ppm_offset;	/* +0x82 the published mean -- the debug
				 *       line's "Timing Offset [ppm]"        */
	short ppm_n;		/* +0x84 intervals in the running total      */
	short ppm_slip;		/* +0x86 net sample slips this interval      */
	short ppm_scale;	/* +0x88 ppm per slipped sample              */
	short ppm_period;	/* +0x8a interval length, in ppm_step units  */
	short ppm_n_max;	/* +0x8c restart the average above this      */
	short ppm_first;	/* +0x8e init 1; discards the first interval */
};

/*
 * `fresh` non-zero means the four buffers do not exist yet: allocate without
 * inspecting them.
 *
 * Zero means re-initialise, and UNLIKE `FPM_FSE_init` THERE IS A REUSE PATH:
 * the four buffers are freed and reallocated only if the existing `taps` is
 * smaller than the configuration now asks for.  Every scalar is reset either
 * way, and `coeff` is rebuilt from `cfg.proto` either way.
 */
void FPM_SRE_init(struct fpm_sre *sre, const struct fpm_sre_cfg *cfg,
		  int fresh);

/* Releases the four buffers.  Does not clear the pointers. */
void FPM_SRE_free(struct fpm_sre *sre);

/*
 * Consume `count` input samples, write one output per recovered symbol, and
 * return how many that was.
 *
 * `need` and `fill` persist across calls, so a stream may be fed in arbitrary
 * fragments -- including fragments too short to produce anything, which take
 * the early path and skip the timing meter entirely.
 *
 * THE RETURN IS ZERO-EXTENDED.  The counter is a `short` -- it is incremented
 * with `cwtl` -- and the return converts it with `movzwl`, so the declared
 * return type is `unsigned short` and not `short`.  The two readings agree
 * over every count a real block can produce; the object was not free to
 * choose the instruction, so this follows the object.
 */
unsigned short FPM_SRE_recover(struct fpm_sre *sre, const short *in,
			       short *out, short count);

/*
 * `FPM_SRE_CFG` -- .rodata 0x00c4e0, 56 bytes, the built-in configuration --
 * is NOT declared here and is NOT part of this batch.  Neither of the two
 * functions reaches it: init takes its configuration by pointer, so the
 * built-in instance belongs to whichever caller copies and patches it, and
 * that caller is not reconstructed.  Its contents are quoted in the field
 * comments above as evidence of scale, nothing more.
 */

/*
 * The interpolation filter.  181 entries, and that width is MEASURED:
 * FPM_SRE_init copies it out with `movzwl (%ecx,%edx,2)`.  Its length is
 * carried separately in the configuration as 180, one short of the table --
 * which is exactly the shape `cfg.proto` needs, and the same relationship
 * SREv22_COFFS's 271 has to V.22's 270.  180 is 18 taps by ten branches.
 */
extern const short SREv32_COFFS[181];

/*
 * The other five are V.32's instances of the four configuration tables above.
 * They are not const: all five live in .data.  `XB_COFFS` is the eleven
 * discriminant coefficients, and its 22 bytes are where FPM_SRE_DISC comes
 * from; `xCLOCK` and `yCLOCK` being three entries each is what V.32's
 * `clock_len` must be.
 */
extern short SREv32_XB_COFFS[FPM_SRE_DISC];
extern short SREv32_PLL_K1[FPM_SRE_MODES];
extern short SREv32_PLL_K2[FPM_SRE_MODES];
extern short SREv32_xCLOCK[3];
extern short SREv32_yCLOCK[3];

#endif /* DSPLIB_FPM_SRE_H */
