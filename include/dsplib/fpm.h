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

/*
 * The same over a 32-bit denominator, off the same table: the mantissa is the
 * top 16 bits after normalisation, so *shift can reach 31.
 */
int FPM_div_32(unsigned int denom, unsigned short *recip,
	       unsigned short *shift);

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

/*
 * Base-10 logarithm of `mantissa * 2^-exponent`, result in Q12.
 *
 * The mantissa is normalised internally, so the exponent is whatever the
 * caller has already taken out.  Zero returns zero and announces itself at
 * debug level 2.  Reads one element past its table for mantissas of
 * 0x7fc0..0x7fff -- reproduced; see src/dsp/fpm_log10.c.
 */
short FPM_log10(unsigned short mantissa, short exponent);

short FPM_log10_table_generate(int index);
int FPM_log10_table_derived(void);	/* entries the generator produces */
short FPM_log10_table_entry(int index);
int FPM_log10_table_size(void);

/* Table introspection, for the generator self-check in the unit tests. */
unsigned short FPM_sqrt_table_generate(int index);
unsigned short FPM_sqrt_table_entry(int index);
int FPM_sqrt_table_size(void);

/*
 * One LMS coefficient update.  `coeff` holds `taps` entries and `hist` is a
 * circular buffer of the same length whose newest sample is at `widx`; the
 * two are walked in opposite directions, so `coeff[0]` is paired with the
 * newest sample and `coeff[taps-1]` with the oldest.  Every coefficient moves
 * by `(hist * err + 0x20000) >> 18`, which is a round-to-nearest at 18
 * fractional bits.
 *
 * `FPM_FSE_receive` calls it twice per symbol, once for each half of the
 * complex filter.
 */
void FPM_lmsupd(short *coeff, const short *hist, short widx, short taps,
		short err);

/*
 * The same walk with the correction formed in two rounded stages:
 *
 *     ((short)((hist[k] * mu + 0x10) >> 5) * err + 0x10000) >> 17
 *
 * The intermediate narrowing to 16 bits is load-bearing, not cosmetic.  Note
 * the argument ORDER -- `err` first, in `FPM_lmsupd`'s slot, then `mu`, which
 * is the reverse of the order they are applied in.  Nothing in the object
 * calls this; the names come from `ecc_adapt`, which is the same arithmetic
 * with a caller to type it.  See src/dsp/fpm_lmsupd.c and finding F8162.
 */
void FPM_lmsupd2(short *coeff, const short *hist, short widx, short taps,
		 short err, short mu);

/*
 * Correlate `count` samples against a circular history and accumulate the
 * scaled result into `taps` coefficients.  The accumulator is 16-bit and
 * truncates every iteration, and the circular wrap is a single conditional
 * add rather than a modulo -- both are load-bearing.  No caller in the object.
 * See src/dsp/fpm_lmsupd.c and finding F8163.
 */
void FPM_block_update(short *coeff, short taps, const short *hist, short widx,
		      short hlen, const short *x, short count, short step,
		      short gain);

/*
 * Dot a circular history against a strided coefficient array, walking `widx`
 * down to 0 and then `taps - 1` back down to `widx + 1` while the coefficients
 * advance by `stride` across both halves.  Each product is shifted down 3
 * inside the loop and the remaining `shift - 3` is applied to the sum, so
 * `shift` is the TOTAL right shift.  Defined in src/dsp/fpm_ecc.c; see finding
 * F8164 for why there rather than in fpm_div.c.
 */
short FPM_circ_dotp2(const short *coeff, const short *hist, short widx,
		     short taps, short stride, short shift);

#endif /* DSPLIB_FPM_H */
