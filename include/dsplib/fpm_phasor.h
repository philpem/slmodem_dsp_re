/*
 * fpm_phasor.h -- Fixed Point Modem: sine/cosine phase accumulator.
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

/*
 * The quadrant sign tables, GLOBAL and in `.data` because the object's are
 * (`D` at 0x081dc and 0x081e4, not `R`).  These two hold exactly what the
 * object's two symbols hold, and nothing indexes them out of range; they are
 * the reconstruction's copy of the object's symbol table, and the unit test
 * compares them word for word against `dsplibs_ref.o`'s own.
 */
extern short FPM_cos_sign[4];
extern short FPM_sin_sign[4];

/*
 * ... and the arrays the phasor ACTUALLY indexes.  `FPM_phasor` does not mask
 * the quadrant, which therefore runs -4 .. 3 rather than 0 .. 3, so the object
 * reads four entries BEFORE each of its two symbols.  Those eight words are
 * constants -- `FPM_sin_sign` itself before `FPM_cos_sign`, and `COEF_DC`'s
 * tail plus a padding word before `FPM_sin_sign` -- so they are carried HERE,
 * as leading entries of one array, and the phasor indexes
 * `ext[FPM_PHASOR_SIGN_BELOW + quad]`.  Every index it can form then lands
 * inside a single array object and the behaviour is defined C rather than a
 * bet on what the linker puts where.  D4 is the same fix one word forward.
 * D392 and finding 3700 for the whole derivation.
 */
#define FPM_PHASOR_SIGN_BELOW 4

extern short FPM_cos_sign_ext[FPM_PHASOR_SIGN_BELOW + 4];
extern short FPM_sin_sign_ext[FPM_PHASOR_SIGN_BELOW + 4];

#endif /* DSPLIB_FPM_PHASOR_H */
