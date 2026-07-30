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
 * STATUS: partial.  set_freq, set_scale and generate are reconstructed and
 * verified.  create, delete and the detector half are not yet done, so the
 * object is treated as opaque storage of a known size and the tests build it
 * with the reference implementation.
 */

#ifndef DSPLIB_FPM_TONE_H
#define DSPLIB_FPM_TONE_H

/*
 * Object size, from FPM_TONE_create's own allocation (0x108).  Matches the
 * highest field offset observed (+0x106), so the object is fully accounted
 * for even though most fields are still unnamed.
 */
#define FPM_TONE_STATE_SIZE 0x108

/* Field offsets established so far; the rest of the object is detector state. */
#define FPM_TONE_OFF_SCALE     0x02	/* s16 output gain, Q14 applied      */
#define FPM_TONE_OFF_REV_PERIOD 0x04	/* s16 reversal period, 8-sample units */
#define FPM_TONE_OFF_PHASE     0x24	/* u16 phase accumulator             */
#define FPM_TONE_OFF_INC       0x26	/* u16 phase increment               */
#define FPM_TONE_OFF_REV_COUNT 0x28	/* u16 samples since last reversal   */

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

#endif /* DSPLIB_FPM_TONE_H */
