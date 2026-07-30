/*
 * fpm_tone.h -- tone generator and detector.
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

/*
 * Object size, from FPM_TONE_create's own allocation (0x108).  Matches the
 * highest field offset observed (+0x106), so the object is fully accounted
 * for even though most fields are still unnamed.
 */
#define FPM_TONE_STATE_SIZE 0x108

/*
 * Config offsets.  NOTE +0x10 holds a POINTER in the original -- dumping the
 * config as int16 hides that behind a plausible-looking scalar.  See the
 * relocation note in docs/findings.md.
 */
#define FPM_TONE_CFG_FREQ      0x00	/* s16 Hz                            */
#define FPM_TONE_CFG_SCALE     0x02	/* s16 output gain                   */
#define FPM_TONE_CFG_REVPERIOD 0x04	/* s16 reversal period, 8-sample units */
#define FPM_TONE_CFG_DAMP      0x0c	/* s16 feeds the Goertzel coefficients */
#define FPM_TONE_CFG_SRC       0x10	/* short * -- source waveform        */
#define FPM_TONE_CFG_LEN       0x14	/* s16 buffer size and fill length    */
#define FPM_TONE_CFG_EXTRA     0x20	/* s16 added to the second buffer     */
#define FPM_TONE_CFG_BYTES     0x24	/* 36 bytes copied wholesale          */

/*
 * The built-in configuration, laid out as a struct so the pointer at +0x10 is
 * a pointer.  Total size must stay 36 bytes to match the original.
 */
struct fpm_tone_cfg {
	short freq;		/* +0x00 */
	short scale;		/* +0x02 */
	short rev_period;	/* +0x04 */
	short pad06[3];		/* +0x06 .. +0x0a */
	short damp;		/* +0x0c */
	short pad0e;		/* +0x0e */
	const short *src;	/* +0x10 pointer to the filter prototype */
	short len;		/* +0x14 */
	short pad16[5];		/* +0x16 .. +0x1e */
	short extra;		/* +0x20 */
	short pad22;		/* +0x22 */
};

extern const struct fpm_tone_cfg FPM_TONE_CFG_data;
extern const short *const FPM_TONE_CFG;
extern const short ToneLPF[53];

/*
 * Build a tone object.  Passing NULL for `state` allocates one (and its
 * buffers); supplying your own means you supply its buffers too.  Passing
 * NULL for `cfg` uses the built-in V.25 answer-tone configuration.
 */
void *FPM_TONE_create(void *state, const void *cfg);

/* Field offsets established so far; the rest of the object is detector state. */
#define FPM_TONE_OFF_SCALE     0x02	/* s16 output gain, Q14 applied      */
#define FPM_TONE_OFF_REV_PERIOD 0x04	/* s16 reversal period, 8-sample units */
#define FPM_TONE_OFF_PHASE     0x24	/* u16 phase accumulator             */
#define FPM_TONE_OFF_INC       0x26	/* u16 phase increment               */
#define FPM_TONE_OFF_REV_COUNT 0x28	/* u16 samples since last reversal   */

/*
 * Free a tone object and its buffers.  Frees unconditionally, including the
 * object itself -- see the note in src/dsp/fpm_tone.c before calling it on
 * anything not built by FPM_TONE_create(NULL, ...).
 */
void FPM_TONE_delete(void *state);

/* Set the tone frequency in Hz.  Assumes an 8 kHz sample rate -- see R-9. */
void FPM_TONE_set_freq(void *state, short hz);

/* Set the output gain, applied as (sample * scale) >> 14. */
void FPM_TONE_set_scale(void *state, short scale);

/*
 * Generate `count` samples.  Every `rev_period` units of 8 samples the phase
 * jumps 180 degrees, which is what makes this an ANSam generator rather than
 * a plain oscillator.  A zero or negative period disables reversals.
 */
void FPM_TONE_generate(void *state, short *out, short count);

/*
 * The reference oscillator for the demodulator: the same tone as
 * FPM_TONE_generate but taken from the cosine, and with no phase reversals.
 * Returns `count`.
 */
short FPM_TONE_generate_demod(void *state, short *out, short count);

/* Detector field offsets. */
#define FPM_TONE_OFF_RATIO      0x06	/* s16 out-of-band fraction allowed, Q15 */
#define FPM_TONE_OFF_MIN_LEVEL  0x0a	/* s16 below this, report no signal      */
#define FPM_TONE_OFF_TAPS       0x14	/* s16 correlator length                 */
#define FPM_TONE_OFF_KERNEL     0x2c	/* short * correlator coefficients       */
#define FPM_TONE_OFF_HISTORY    0x30	/* short * circular history, TAPS words  */
#define FPM_TONE_OFF_HIST_IDX   0x34	/* s16 write position                    */
#define FPM_TONE_OFF_IIR_COEFF  0x36	/* s16[5] Goertzel resonator             */
#define FPM_TONE_OFF_IIR_STATE  0x40	/* s16[4] its direct form I state        */
#define FPM_TONE_OFF_E_EXCESS   0x48	/* s16 smoothed out-of-band energy       */
#define FPM_TONE_OFF_E_TOTAL    0x4a	/* s16 smoothed total energy             */

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
 * FPM_MTD_detect's constants were named by the same analogy and have NOT been
 * re-checked; do not assume they match.
 */
#define FPM_TONE_PRESENT  0	/* the configured tone is there      */
#define FPM_TONE_OTHER    1	/* signal present, but not this tone */
#define FPM_TONE_NOSIGNAL 2	/* below the minimum level           */

/*
 * Run `count` samples through the correlator and resonator and report whether
 * the configured tone is present.  Energy estimates persist in the state, so
 * the answer reflects a running average rather than this block alone.
 */
short FPM_TONE_detect(void *state, const short *samples, short count);

#endif /* DSPLIB_FPM_TONE_H */
