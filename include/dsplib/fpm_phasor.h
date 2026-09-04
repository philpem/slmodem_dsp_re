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

/*
 * A full cycle on the DOUBLE-PRECISION accumulator, which carries fifteen
 * fractional bits below the phase.  Exactly FPM_PHASOR_CYCLE << 15.
 */
#define FPM_PHASOR_DP_CYCLE 0x40000000

struct fpm_phasor {
	unsigned short phase;	/* +0x00 accumulator, 0 .. 0x7fff */
	short cos;		/* +0x02 output                   */
	short sin;		/* +0x04 output                   */
	unsigned short inc;	/* +0x06 phase increment          */
};

/**
 * @brief Advance a phase accumulator by one step and produce sin/cos.
 *
 * Leaves `p->sin`/`p->cos` holding the sine and cosine of the phase as it
 * stood *before* this call, then advances `p->phase` by `p->inc`.
 *
 * @param p Phasor state: accumulator, increment and the two outputs.
 */
void FPM_phasor(struct fpm_phasor *p);

/*
 * The same oscillator with a FRACTIONAL phase carried between calls, so the
 * increment need not be a whole phase unit.  Two fields wider than
 * `struct fpm_phasor` and NOT a superset of it -- the two are separate types
 * because the object's two functions take different objects, and nothing
 * passes one to the other.
 *
 * The accumulator is 30-bit: `phase` is the top 15 bits and `frac_phase` the
 * bottom 15, and the wrap subtracts 0x40000000 = one cycle at that scale.
 * `FPM_phasor`'s wrap is the same rule at the coarse scale, 0x8000.
 *
 * The two fractional fields are summed and HALVED before they join the
 * accumulator (deviation D950).  That halving is in the object and is not
 * explained by anything visible, so neither field is named for a Q-format on
 * the strength of it: what is established is where they sit and what the
 * arithmetic does with them, which is finding F8168's decode.
 */
struct fpm_phasor_dp {
	unsigned short phase;		/* +0x00 accumulator, high 15 bits  */
	short cos;			/* +0x02 output                     */
	short sin;			/* +0x04 output                     */
	unsigned short inc;		/* +0x06 phase increment            */
	short frac_phase;		/* +0x08 accumulator, low 15 bits   */
	short frac_inc;			/* +0x0a fractional increment       */
};

/**
 * @brief Advance a fractional-phase accumulator by one step and produce
 *        sin/cos.
 *
 * Same role as FPM_phasor(), but the accumulator carries a fractional phase
 * (`frac_phase`) between calls, so the increment need not be a whole phase
 * unit; see the struct comment above for how `phase`/`frac_phase` combine.
 *
 * @param p Fractional-phase phasor state.
 */
void FPM_phasor_dp(struct fpm_phasor_dp *p);

/**
 * @brief Advance a phase accumulator and produce cosine only.
 *
 * Same as FPM_phasor() but does not touch `p->sin` -- it is left holding
 * whatever it last held, not zeroed, so a caller that alternates this with
 * FPM_phasor() sees a stale sine value. That is the object's own behaviour
 * and is reproduced.
 *
 * @param p Phasor state; only `cos` and `phase` are updated.
 */
void FPM_phasor_demod(struct fpm_phasor *p);

/**
 * @brief Look up one entry of the internal cosine table.
 * @param i Table index. Used by the unit tests' generator self-check.
 * @return The raw Q15 table entry.
 */
unsigned short FPM_phasor_cos_entry(int i);

/**
 * @brief Look up one entry of the internal sine table.
 * @param i Table index. Used by the unit tests' generator self-check.
 * @return The raw Q15 table entry.
 */
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
 * D392 and finding F3700 for the whole derivation.
 */
#define FPM_PHASOR_SIGN_BELOW 4

extern short FPM_cos_sign_ext[FPM_PHASOR_SIGN_BELOW + 4];
extern short FPM_sin_sign_ext[FPM_PHASOR_SIGN_BELOW + 4];

#endif /* DSPLIB_FPM_PHASOR_H */
