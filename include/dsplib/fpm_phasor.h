/*
 * fpm_phasor.h -- quarter-wave sine/cosine phase accumulator.
 *
 * The oscillator underneath every tone the library generates or correlates
 * against.  One call advances the phase by `inc` and leaves the sine and
 * cosine of the *previous* phase in the struct.
 *
 * Phase is 15-bit: a full cycle is 0x8000 units, so the increment for a tone
 * of f Hz at sample rate fs is f * 32768 / fs.  FPM_TONE_create and
 * FPM_TONE_set_freq compute that with fs fixed at 8000 -- see R-9 in
 * docs/rate_assumptions.md.
 */

#ifndef DSPLIB_FPM_PHASOR_H
#define DSPLIB_FPM_PHASOR_H

/* 256 entries per quadrant, plus one so interpolation can read idx+1. */
#define FPM_PHASOR_TABLE 257

/* A full cycle in phase units. */
#define FPM_PHASOR_CYCLE 0x8000

struct fpm_phasor {
	unsigned short phase;	/* +0x00 accumulator, 0 .. 0x7fff */
	short cos;		/* +0x02 output                   */
	short sin;		/* +0x04 output                   */
	unsigned short inc;	/* +0x06 phase increment          */
};

void FPM_phasor(struct fpm_phasor *p);

/*
 * The same, but cosine only.  `sin` is left UNTOUCHED, not zeroed -- a caller
 * that alternates the two functions sees a stale sine, and that is faithful.
 */
void FPM_phasor_demod(struct fpm_phasor *p);

/* Table introspection, for the generator self-check in the unit tests. */
unsigned short FPM_phasor_cos_entry(int i);
unsigned short FPM_phasor_sin_entry(int i);

#endif /* DSPLIB_FPM_PHASOR_H */
