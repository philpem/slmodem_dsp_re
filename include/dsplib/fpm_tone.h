/*
 * fpm_tone.h -- Fixed Point Modem: tone generation and detection.
 *
 * One object carries three things tuned to the same frequency: a phase
 * accumulator, an exact-frequency Goertzel, and a damped resonator.  That is
 * why a single config drives generation, detection and the phase-reversal
 * search -- see docs/findings.md sections 18 and 19.
 *
 * The default config is the ITU-T V.25 answer tone: 2100 Hz with a 180 degree
 * phase reversal every 450 ms, which disables network echo cancellers.
 *
 * STATUS: every function in the translation unit is reconstructed, and the
 * whole 0x108 bytes is now modelled as fields.  The last unattributed span,
 * carried as `r4c[84]` while FPM_TONE_find_rev was unwritten, turned out to be
 * that function's entire working set: two running accumulators, a counter, an
 * 80-word delay line and its index.
 */

#ifndef DSPLIB_FPM_TONE_H
#define DSPLIB_FPM_TONE_H

/* Object size, from FPM_TONE_create's own allocation. */
#define FPM_TONE_STATE_SIZE 0x108

/*
 * The built-in configuration, laid out as a struct so the pointer at +0x10 is
 * a pointer.  Total size must stay 36 bytes to match the original.
 */
struct fpm_tone_cfg {
	short freq;		/* +0x00 Hz                                  */
	short scale;		/* +0x02 generator output gain, Q14 applied  */
	short rev_period;	/* +0x04 phase reversals, in 8-sample ticks;
				 *       zero or negative disables them      */
	short ratio;		/* +0x06 detector: the share of the energy
				 *       the tone must hold, Q15             */
	short f08;		/* +0x08                                     */
	short min_level;	/* +0x0a detector: below this, no signal     */
	short damp;		/* +0x0c the notch's pole radius r, Q15      */
	short pad0e;
	const short *src;	/* +0x10 correlator prototype, copied into
				 *       the object's own kernel by create   */
	short len;		/* +0x14 its length, and the detector's tap
				 *       count                               */
	short r16[3];		/* +0x16 .. +0x1a                            */
	short f1c;		/* +0x1c NOT padding: 16384 in the built-in
				 *       config.  FPM_TONE_find_rev's only
				 *       reader -- the Q15 fraction of the
				 *       windowed energy that twice the lag
				 *       correlation must fall below for the
				 *       search to fire.  Left
				 *       spelled `f1c` only because the
				 *       initialiser lives in fpm_tone_cfg.c;
				 *       a rename belongs with that file.    */
	short f1e;		/* +0x1e NOT padding: 40.  FPM_TONE_find_rev's
				 *       correlation LAG in samples, and half
				 *       the length of `rev_hist` -- the
				 *       delay line is indexed modulo 2*f1e,
				 *       and 2*40 is exactly the 80 words the
				 *       object reserves.                    */
	short extra;		/* +0x20 added to the history buffer's length */
	short pad22;
};

/*
 * The object.  264 bytes, and laid out here rather than reached by offset --
 * an address like `+0x4a` says nothing about what lives there, and this
 * module has three separate sub-systems sharing one allocation.
 *
 * The span that used to be carried here as `r4c[84]` is now named.  It is the
 * phase-reversal search's working set and nothing else, and the split is not
 * inferred from FPM_TONE_find_rev alone: FPM_TONE_create clears the object in
 * two loops, +0x40..+0x50 and +0x52..+0xf0, with a separate store of zero to
 * +0xf2 after the second.  Those bounds are the array's own -- 80 words at
 * +0x52 with an index behind them -- and they were visible in the object
 * before any of this was read.
 *
 * 32-BIT LAYOUT: the layout below is from a build where pointers are four
 * bytes.  See the assertions in src/dsp/fpm_tone.c.
 */
struct fpm_tone {
	struct fpm_tone_cfg cfg;	/* +0x00 copied wholesale by create   */

	/* --- the oscillator ------------------------------------------- */
	unsigned short phase;		/* +0x24 accumulator                  */
	unsigned short inc;		/* +0x26 increment; hz * 32768 / 8000 */
	unsigned short rev_count;	/* +0x28 8-sample ticks since the last
					 *       phase reversal.  The
					 *       GENERATOR's, and only the
					 *       generator's -- FPM_TONE_generate
					 *       is the sole reader and writer.
					 *       An earlier note here guessed
					 *       that find_rev or kill might use
					 *       it; neither touches +0x28.  The
					 *       receive side's own counter is
					 *       `rev_age` below, and it counts
					 *       SAMPLES rather than ticks.    */
	short pad2a;

	/* --- the detector --------------------------------------------- */
	short *kernel;			/* +0x2c cfg.len taps, filled by
					 *       create from cfg.src          */
	short *history;			/* +0x30 cfg.len + cfg.extra entries,
					 *       circular                     */
	short hist_idx;			/* +0x34 write position               */
	short iir_coeff[5];		/* +0x36 the notch at cfg.freq -- see
					 *       FPM_TONE_detect              */
	short iir_state[4];		/* +0x40 direct form I, so four       */
	short e_tone;			/* +0x48 smoothed tone energy.  The
					 *       original's own naming would
					 *       call this out-of-band; it is
					 *       not.  See finding F33.        */
	short e_total;			/* +0x4a smoothed total energy        */

	/* --- the phase-reversal search -------------------------------- */
	/*
	 * All five of these are FPM_TONE_find_rev's, and it is the only
	 * function in the object that reads or writes any of them.
	 */
	short rev_age;			/* +0x4c samples since the last
					 *       reversal was REPORTED.  It
					 *       advances once per sample and is
					 *       reset only when a reversal is
					 *       both seen and older than 160
					 *       samples, which is what debounces
					 *       the search.                  */
	short rev_corr;			/* +0x4e the running correlation of the
					 *       input against itself delayed by
					 *       cfg.f1e, over a window of
					 *       cfg.f1e products.  Held as an
					 *       int inside the loop and stored
					 *       back SATURATED -- see the note
					 *       in FPM_TONE_find_rev.        */
	short rev_energy;		/* +0x50 the running energy of the same
					 *       input over 2*cfg.f1e samples,
					 *       Q15-scaled per term and stored
					 *       back saturated the same way.  */
	short rev_hist[80];		/* +0x52 the delay line both of those
					 *       slide over, indexed modulo
					 *       2*cfg.f1e.  Eighty words is
					 *       FPM_TONE_create's own second
					 *       clearing loop, +0x52..+0xf0.  */
	short rev_idx;			/* +0xf2 write position in rev_hist   */

	short *rev_block;		/* +0xf4 10 bytes: the { b0, b2, b1,
					 *       a2, a1 } of ONE biquad, and
					 *       typed by its use -- find_rev
					 *       hands it to FPM_iir_filt_II as
					 *       the coefficient argument with a
					 *       section count of 1, which is
					 *       also why create allocates
					 *       exactly ten bytes.  Built at
					 *       phase zero, so the section is
					 *       a notch at DC.               */
	short *rev_acc;			/* +0xf8 8 bytes: that filter's direct
					 *       form I state, four words for
					 *       the one section, by the same
					 *       argument                     */
	short *iir_self;		/* +0xfc points at iir_coeff[0]; the
					 *       original stores it rather than
					 *       recomputing it               */
	/*
	 * Direct form I state for FPM_TONE_kill's pass of the SAME notch --
	 * four words because FPM_iir_filt_II keeps two past inputs and two
	 * past outputs per section, and there is one section.  Separate from
	 * `iir_state` at +0x40 so that killing the tone in the caller's buffer
	 * does not disturb the detector's own running estimate; the two run
	 * the identical coefficients over different sample streams.
	 *
	 * Named from FPM_TONE_kill, which is the only reader: FPM_TONE_find_rev
	 * touches +0xf4 and +0xf8 and nothing else in the tail.
	 */
	short kill_state[4];		/* +0x100 .. +0x107                   */
};

extern const struct fpm_tone_cfg FPM_TONE_CFG_data;
extern const short *const FPM_TONE_CFG;
extern const short ToneLPF[53];

/*
 * Build a tone object.  Passing NULL for `state` allocates one (and its
 * buffers); supplying your own means you supply its buffers too.  Passing
 * NULL for `cfg` uses the built-in V.25 answer-tone configuration.
 */
struct fpm_tone *FPM_TONE_create(struct fpm_tone *state,
				 const struct fpm_tone_cfg *cfg);

/*
 * Free a tone object and its buffers.  Frees unconditionally, including the
 * object itself -- see the note in src/dsp/fpm_tone.c before calling it on
 * anything not built by FPM_TONE_create(NULL, ...).
 */
void FPM_TONE_delete(struct fpm_tone *state);

/* Set the tone frequency in Hz.  Assumes an 8 kHz sample rate -- see R-9. */
void FPM_TONE_set_freq(struct fpm_tone *state, short hz);

/* Set the output gain, applied as (sample * scale) >> 14. */
void FPM_TONE_set_scale(struct fpm_tone *state, short scale);

/*
 * Generate `count` samples.  Every `rev_period` units of 8 samples the phase
 * jumps 180 degrees, which is what makes this an ANSam generator rather than
 * a plain oscillator.  A zero or negative period disables reversals.
 */
void FPM_TONE_generate(struct fpm_tone *state, short *out, short count);

/*
 * The reference oscillator for the demodulator: the same tone as
 * FPM_TONE_generate but taken from the cosine, and with no phase reversals.
 * Returns `count`.
 */
short FPM_TONE_generate_demod(struct fpm_tone *state, short *out,
			      short count);

/*
 * The QUADRATURE pair from one oscillator: `cos_out` and `sin_out` get the
 * cosine and the sine of the same phase, sample for sample.  Two outputs, not
 * two tones -- the object still holds a single frequency.  No phase
 * reversals, and it returns `count`.
 */
short FPM_TONE_generate2(struct fpm_tone *state, short *cos_out,
			 short *sin_out, short count);

/*
 * Verdicts.  NOTE THE POLARITY -- zero means the tone IS present.
 *
 * This was recorded the other way round at first, by analogy with
 * FPM_MTD_detect, and it is wrong: the biquad FPM_TONE_create builds at +0x36
 * is a NOTCH at the tone frequency, not a bandpass.  So the value the code
 * calls "out of band" is what is left after the tone is removed, and the
 * quantity compared against `ratio` is the tone's own share of the energy.
 * Measured directly: a 2100 Hz input yields 0, 2000 and 2200 Hz yield 1,
 * silence yields 2.  See findings F30 and F33.
 *
 * FPM_MTD_detect uses the OPPOSITE convention, and correctly so -- it has been
 * swept and checked.  Its filters are per-tone bandpasses rather than a notch,
 * so its "out of band" really is the leftover and 1 means detected.  Two
 * functions in one library, same shape, opposite polarity: check, never infer.
 */
#define FPM_TONE_PRESENT  0	/* the configured tone is there      */
#define FPM_TONE_OTHER    1	/* signal present, but not this tone */
#define FPM_TONE_NOSIGNAL 2	/* below the minimum level           */

/*
 * Run `count` samples through the correlator and resonator and report whether
 * the configured tone is present.  Energy estimates persist in the state, so
 * the answer reflects a running average rather than this block alone.
 */
short FPM_TONE_detect(struct fpm_tone *state, const short *samples,
		      short count);

/*
 * Time the sign changes of `samples`' autocorrelation at a lag of `cfg.f1e`
 * samples.  `samples` is filtered IN PLACE on the way in, through the one
 * biquad at `rev_block`.
 *
 * A sign change there is a 180 degree phase reversal only where the carrier's
 * period divides `cfg.f1e`, which the built-in 2100 Hz config's does NOT --
 * read the derivation above FPM_TONE_find_rev in src/dsp/fpm_tone.c before
 * treating this as an answer-tone reversal detector for a given tone.
 *
 * Returns 0 when nothing was found, and otherwise the interval since the last
 * one it reported, in units of eight samples -- the same units
 * `cfg.rev_period` is expressed in, so the two are directly comparable.  A
 * report is suppressed unless more than 160 samples have passed since the last
 * one, so the shortest interval this can ever return is 20.
 *
 * A NEGATIVE `count` does nothing here, which is not what the rest of the
 * module does: FPM_TONE_detect, _generate2, _generate_demod and _filter all
 * count down through a 16-bit value and run about 65536 times.  This one
 * compares against `count` instead.
 */
short FPM_TONE_find_rev(struct fpm_tone *state, short *samples, short count);

/*
 * Run `samples` through the detector's correlator, in place.
 *
 * It is the first half of FPM_TONE_detect and nothing else: the same circular
 * `history` of `cfg.len` words, the same `kernel`, the same two-loop wrap and
 * the same `>> 15`.  What it does not do is square, smooth or decide, and it
 * shares `hist_idx` with the detector -- so a filter pass and a detect pass on
 * one object walk the same write position and interleave.
 *
 * Nothing in dsplibs.o calls it: it is the only FPM_TONE entry point with no
 * relocation naming it anywhere in the object.
 */
void FPM_TONE_filter(struct fpm_tone *state, short *samples, short count);

/*
 * Remove the configured tone from `samples`, in place, `count` at a time.
 *
 * It is FPM_TONE_detect's notch run over the caller's own buffer: the same
 * one biquad, through the stored self-pointer at +0xfc rather than
 * `iir_coeff` directly, with its own persistent state so the two passes do
 * not interfere.  The filter is `FPM_iir_filt_II`, which does not saturate.
 *
 * `count` is read as a signed 16-bit value and widened, so it is the
 * declared type here and not an int.
 */
void FPM_TONE_kill(struct fpm_tone *state, short *samples, short count);

#endif /* DSPLIB_FPM_TONE_H */
