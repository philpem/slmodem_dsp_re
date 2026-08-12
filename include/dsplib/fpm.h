/*
 * fpm.h -- Fixed Point Modem: arithmetic kernels.
 *
 * The `fpm_*` layer is Q15 fixed point throughout, so every function here is
 * held to bit-exact equivalence with the original -- no tolerance.
 */

#ifndef DSPLIB_FPM_H
#define DSPLIB_FPM_H

/*
 * Q15 square root.  Input and output are unsigned Q15 fractions.
 *
 * Defined for 0x0000..0x7fff.  The original reads past its lookup table for
 * larger inputs; this version clamps.  See src/dsp/fpm_sqrt.c.
 */
unsigned short FPM_sqrt(unsigned short x);

/*
 * 32-bit square root over the same table.  Unlike FPM_sqrt this one clamps its
 * table index, so it has no out-of-range read -- see src/dsp/fpm_sqrt.c.
 */
unsigned short FPM_sqrt_dp(unsigned int x);

/*
 * Scaled RMS of a block of samples.
 *
 * Computes sqrt(sum(x^2) / 36) -- the 1/36 is headroom, keeping the sum inside
 * 32 bits for up to 36 full-scale samples.  Beyond 72 it overflows anyway; see
 * src/dsp/fpm_rms.c.
 */
short FPM_rms(const short *samples, unsigned short count);

/*
 * Reciprocal lookup for fixed-point division.  Leaves an approximate 1/denom
 * in *recip and the normalisation shift in *shift; returns 1 if denom is zero.
 */
int FPM_div(unsigned short denom, unsigned short *recip, unsigned short *shift);

unsigned short FPM_div_table_entry(int i);
unsigned short FPM_div_table_generate(int i);

/*
 * Four-quadrant arctangent, built on FPM_div.  Argument order is atan2's:
 * `y` first, then `x`.  The answer goes through the pointer -- the function
 * returns nothing.
 *
 * The angle is in fpm_phasor.c's phase units, a full turn being 0x8000, and
 * runs anticlockwise from the positive x axis.  Both arguments zero gives
 * 0x2000, which is what the x == 0 branch does rather than a special case.
 *
 * Not exact at two places, faithfully: a ratio of exactly 127/32768 returns
 * zero, and the octant with x > 0, y < 0, |x| > |y| is one unit low.  Both
 * are explained in src/dsp/fpm_atan.c.
 */
void FPM_atan(short y, short x, short *angle);

/* atan of i/256 in Q15 radians, rounded.  Global in the original. */
#define FPM_ATAN_TABLE 257
extern const short FPM_atan_table[FPM_ATAN_TABLE];

/* Table introspection, for the generator self-check in the unit tests. */
unsigned short FPM_sqrt_table_generate(int index);
unsigned short FPM_sqrt_table_entry(int index);
int FPM_sqrt_table_size(void);

#endif /* DSPLIB_FPM_H */
