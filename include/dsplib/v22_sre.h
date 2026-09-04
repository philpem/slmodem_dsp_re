/*
 * v22_sre.h -- V.22 / V.22bis symbol-timing recovery (Symbol Rate Estimator).
 *
 * The author's own translation unit `v22_sre.c`.  There is a family of these
 * -- `fpm_sre.c` (FPM_SRE_*), and the V.17, V.27, V.29 and V.32 variants that
 * share the SRE_ALPHA_AVG / SRE_BETA_AVG smoothing constants -- but each
 * carries its own loop and its own tables, and only V.22's is reconstructed
 * here.
 *
 * WHAT THE BLOCK IS
 *
 * It is an interpolating resampler and a timing PLL in one pass.  Input
 * arrives at 3600 Hz; one output is produced per symbol interval, and *where*
 * in the input stream that output is taken from is what the PLL steers.
 *
 *   1. Samples are appended to a 54-entry history.  When the next write would
 *      pass the end, the top 27 entries slide down to the bottom and the fill
 *      level drops by 27.
 *   2. One output is the 27-tap dot product of the history's newest 27
 *      entries against polyphase branch `branch` of `coeff`, >> 15.
 *   3. That output drives three cascaded second-order sections held in
 *      `clk[6]`.  The third section's slope, (clk[4] - clk[5]) / 2, is the
 *      timing discriminant.
 *   4. The discriminant is correlated against SREv22_xCLOCK / SREv22_yCLOCK
 *      -- a six-point complex exponential at Q14, i.e. one cycle of 600 Hz at
 *      3600 Hz, which is exactly one V.22 symbol -- and accumulated into
 *      `acc_x` and `acc_y`.
 *   5. Every `groups` groups of six samples, FPM_atan turns that accumulated
 *      vector into a phase error, which a first- or second-order loop filter
 *      turns into an adjustment of (`branch`, `frac`).
 *
 * THE LOOP IS GEAR-SHIFTED, NOT FIXED ORDER
 *
 * `mode` selects one entry of each of four three-entry tables, and the three
 * modes are three different loops:
 *
 *   mode 0  settling.  SREv22_PLL_K2[0] is ZERO, so the integrator never
 *           charges and the loop is purely proportional -- first order.  The
 *           accumulator `pll_acc` is held at 0 throughout.
 *   mode 1  acquisition.  K1 = 762, K2 = 4: second order, `groups` = 3, so
 *           the loop updates every 18 samples.
 *   mode 2  tracking.  Same coefficients, `groups` = 12 (72 samples), and
 *           `pll_acc` is scaled up by 4 on entry and back down by 4 on exit
 *           so the integrator's resolution follows the slower update rate.
 *
 * Mode 2 is also the only mode that re-interpolates `coeff`: at every update
 * the ten polyphase branches are rebuilt from SREv22_COFFS by linear
 * interpolation between adjacent taps, weighted by `frac`.  In modes 0 and 1
 * the branches are the ones V22_SRE_init laid down and the phase moves only
 * in whole branches.
 *
 * SREv22_COFFS HAS 271 ENTRIES AND THE BUFFER HAS 270
 *
 * That is not an error either way.  V22_SRE_init copies COFFS[0..269] into
 * the 270-short buffer; the interpolator reads COFFS[k] and COFFS[k+1] for
 * k up to 269, so the 271st entry exists solely to be the right-hand end of
 * the last interpolation.
 */

#ifndef DSPLIB_V22_SRE_H
#define DSPLIB_V22_SRE_H

/* Polyphase geometry.  10 branches of 27 taps: 270 of COFFS's 271 entries. */
#define V22_SRE_BRANCHES	10
#define V22_SRE_TAPS		27
#define V22_SRE_COEFFS		(V22_SRE_BRANCHES * V22_SRE_TAPS)
#define V22_SRE_COFFS_LEN	(V22_SRE_COEFFS + 1)

/*
 * The sample history.  Twice the tap count, so a whole tap window is always
 * contiguous; the slide costs one 27-short memcpy per 27 samples rather than
 * a modulo on every access.
 */
#define V22_SRE_HIST		(2 * V22_SRE_TAPS)

/*
 * Six-point clock reference, and the number of samples in one V.22 symbol at
 * the 3600 Hz this block runs at.
 */
#define V22_SRE_CLOCK		6

/* The three-entry tables `mode` indexes. */
#define V22_SRE_MODES		3

/*
 * `frac` is an 11-bit fraction of one polyphase branch: 0 .. 2047.  The
 * interpolator's weight is frac << 4, so a full branch is Q15's 1.0.
 */
#define V22_SRE_FRAC_ONE	0x800

/*
 * NOTE the two heap buffers whose length is NOT what init clears.  `hist` is
 * V22_SRE_HIST entries and init clears only the first V22_SRE_TAPS of them,
 * so hist[27..53] holds allocator fill until V22_SRE_recover has written it.
 * That is the original's behaviour, it is harmless -- the filter never reads
 * above the fill level -- and a whole-buffer comparison after init alone will
 * fail on it.
 */
struct v22_sre {
	short mode;		/* +0x00 0..2; indexes the four 3-entry tables  */
	short pll_acc;		/* +0x02 loop-filter integrator, Q(frac)        */
	short err_avg;		/* +0x04 smoothed phase error from FPM_atan     */
	short mag_avg;		/* +0x06 smoothed |discriminant|; the squelch   */
	int active;		/* +0x08 mag_avg cleared the gate: run the PLL  */
	int acquiring;		/* +0x0c 1 until mode 2 is entered.  Written by
				 *       this module, read only by its caller.  */
	int adapt;		/* +0x10 caller's enable for the phase update.
				 *       Set by init, never written by recover. */
	short taps;		/* +0x14 V22_SRE_TAPS; sizes both heap buffers  */
	short fill;		/* +0x16 live entries in `hist`, 0 .. 54        */
	short *coeff;		/* +0x18 V22_SRE_COEFFS entries, branch-major   */
	short *hist;		/* +0x1c V22_SRE_HIST entries                   */
	short *clk;		/* +0x20 V22_SRE_CLOCK entries: three 2nd-order
				 *       sections, each { now, previous }       */
	int acc_x;		/* +0x24 discriminant . SREv22_xCLOCK           */
	int acc_y;		/* +0x28 discriminant . SREv22_yCLOCK           */
	short frac;		/* +0x2c sub-branch phase, 0 .. 2047            */
	short branch;		/* +0x2e polyphase branch, 0 .. 9               */
	short groups;		/* +0x30 six-sample groups per PLL update: 3
				 *       in modes 0 and 1, 12 in mode 2         */
	short settle;		/* +0x32 update counter, 0 .. 65; at 64 the
				 *       loop leaves mode 0 for mode 1          */
	short group;		/* +0x34 groups counted since the last update   */
	short tick;		/* +0x36 sample within the group, 0 .. 5;
				 *       indexes SREv22_xCLOCK / yCLOCK         */
	short need;		/* +0x38 input samples owed before the next
				 *       output; carried across calls           */
	/*
	 * Named, not implied.  This object is EMBEDDED at +0x128 inside the
	 * V.22 datapump (V22FP_create passes `dp + 0x128`), so the tail
	 * padding is inside a larger allocation and lands in any whole-object
	 * comparison.  The fpm_mrf.h convention: give padding a name so it is
	 * a member that assignment and initialisers have to honour.
	 */
	short pad3a;
};

/**
 * @brief Initialise (or re-arm) the V.22 symbol-timing recovery loop.
 *
 * @p fresh non-zero allocates the three buffers without inspecting the
 * existing pointers -- so, exactly like `FPM_MRF_init`, calling it twice
 * with @p fresh set leaks. Zero re-initialises in place and requires the
 * three pointers to be valid already.
 *
 * Either way every scalar is reset, `coeff` is rebuilt from SREv22_COFFS,
 * and `clk` is cleared.
 *
 * @param sre    The SRE state to initialise.
 * @param fresh  Non-zero to allocate the three buffers; zero to reuse existing ones.
 */
void V22_SRE_init(struct v22_sre *sre, int fresh);

/** @brief Release the V.22 SRE's three buffers. Does not clear the pointers. @param sre The SRE state to tear down. */
void V22_SRE_free(struct v22_sre *sre);

/**
 * @brief Recover symbol timing and resample `count` V.22 receive samples.
 *
 * Consumes @p count input samples and writes one output per recovered
 * symbol. `need` and `fill` persist across calls, so a stream may be fed
 * in arbitrary fragments, including fragments too short to produce
 * anything. `DemodDataV22` widens the result with `movzwl`; the internal
 * counter is signed, and the two readings agree over every count a
 * 160-sample block can produce.
 *
 * @param sre    The SRE state.
 * @param in     Input samples at 3600 Hz.
 * @param out    Output for the recovered symbols.
 * @param count  How many input samples.
 * @return The number of symbols produced, roughly `count * V22_SRE_BRANCHES / (10 + steer)`.
 */
short V22_SRE_recover(struct v22_sre *sre, const short *in, short *out,
		      short count);

/*
 * The prototype filter, 271 taps at Q15, symmetric about COFFS[135].  NOT
 * `static`: the original places it in .rodata as a global.
 */
extern const short SREv22_COFFS[V22_SRE_COFFS_LEN];

/*
 * One cycle of a complex exponential at Q14, six points -- 600 Hz sampled at
 * 3600 Hz.  x is the cosine (16384, 8192, -8192, ...), y the sine
 * (0, 14189, ...).  `tick` indexes both.
 */
extern const short SREv22_xCLOCK[V22_SRE_CLOCK];
extern const short SREv22_yCLOCK[V22_SRE_CLOCK];

/*
 * The first-order smoother shared with the other SRE variants:
 *
 *     avg = (avg * ALPHA >> 15) + (new * BETA >> 15)
 *
 * ALPHA + BETA is 32768 for all three modes, so the smoother is unity gain.
 * Modes 0 and 1 share a time constant; mode 2's is about three times longer.
 */
extern const short SRE_ALPHA_AVG[V22_SRE_MODES];
extern const short SRE_BETA_AVG[V22_SRE_MODES];

/*
 * Loop-filter gains, Q12.  K2[0] is zero: see the gear-shift note above.
 */
extern const short SREv22_PLL_K1[V22_SRE_MODES];
extern const short SREv22_PLL_K2[V22_SRE_MODES];

#endif /* DSPLIB_V22_SRE_H */
