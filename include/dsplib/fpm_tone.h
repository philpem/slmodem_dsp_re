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
 * STATUS: create, delete, set_freq, set_scale, generate and detect are all
 * reconstructed and verified.  The object is still treated as opaque storage
 * of a known size, accessed by offset, because a good half of its 0x108 bytes
 * has no known purpose yet -- FPM_TONE_find_rev and FPM_TONE_kill use fields
 * this module does not.
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
				 *       config.  Nothing reconstructed reads
				 *       it yet -- FPM_TONE_find_rev and
				 *       _kill are the candidates.          */
	short f1e;		/* +0x1e NOT padding: 40                     */
	short extra;		/* +0x20 added to the history buffer's length */
	short pad22;
};

/*
 * The object.  264 bytes, and laid out here rather than reached by offset --
 * an address like `+0x4a` says nothing about what lives there, and this
 * module has three separate sub-systems sharing one allocation.
 *
 * Roughly a third of it is still unattributed: `r4c` covers everything
 * between the detector's energy estimates and the reversal buffers, which
 * FPM_TONE_find_rev and FPM_TONE_kill presumably use.  It is a named
 * reserved region rather than a hole, so a field can be sited in it later
 * without recounting anything.
 *
 * 32-BIT LAYOUT: the reserved region is a byte count from a build where
 * pointers are four bytes.  See the assertions in src/dsp/fpm_tone.c.
 */
struct fpm_tone {
	struct fpm_tone_cfg cfg;	/* +0x00 copied wholesale by create   */

	/* --- the oscillator ------------------------------------------- */
	unsigned short phase;		/* +0x24 accumulator                  */
	unsigned short inc;		/* +0x26 increment; hz * 32768 / 8000 */
	unsigned short rev_count;	/* +0x28 8-sample ticks since the last
					 *       phase reversal               */
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
					 *       not.  See finding 33.        */
	short e_total;			/* +0x4a smoothed total energy        */

	/* --- unattributed --------------------------------------------- */
	short r4c[84];			/* +0x4c .. +0xf3                     */

	/* --- the phase-reversal search -------------------------------- */
	short *rev_block;		/* +0xf4 10 bytes                     */
	short *rev_acc;			/* +0xf8 8 bytes                      */
	short *iir_self;		/* +0xfc points at iir_coeff[0]; the
					 *       original stores it rather than
					 *       recomputing it               */
	short r100[4];			/* +0x100 .. +0x107                   */
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
 * Verdicts.  NOTE THE POLARITY -- zero means the tone IS present.
 *
 * This was recorded the other way round at first, by analogy with
 * FPM_MTD_detect, and it is wrong: the biquad FPM_TONE_create builds at +0x36
 * is a NOTCH at the tone frequency, not a bandpass.  So the value the code
 * calls "out of band" is what is left after the tone is removed, and the
 * quantity compared against `ratio` is the tone's own share of the energy.
 * Measured directly: a 2100 Hz input yields 0, 2000 and 2200 Hz yield 1,
 * silence yields 2.  See findings 30 and 33.
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

#endif /* DSPLIB_FPM_TONE_H */
