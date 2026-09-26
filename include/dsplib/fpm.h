/*
 * fpm.h -- Fixed Point Modem: arithmetic kernels.
 *
 * The `fpm_*` layer is Q15 fixed point throughout, so every function here is
 * held to bit-exact equivalence with the original -- no tolerance.
 */

#ifndef DSPLIB_FPM_H
#define DSPLIB_FPM_H

/**
 * @brief Q15 square root.
 * @param x  Unsigned Q15 fraction, 0x0000..0x7fff. The original reads past
 *           its lookup table for larger inputs; this version clamps.
 *           See src/dsp/fpm_sqrt.c.
 * @return sqrt(x) as an unsigned Q15 fraction.
 */
unsigned short FPM_sqrt(unsigned short x);

/**
 * @brief 32-bit square root, over the same table as FPM_sqrt().
 *
 * Unlike FPM_sqrt() this one clamps its table index, so it has no
 * out-of-range read -- see src/dsp/fpm_sqrt.c.
 *
 * @param x  Input value.
 * @return sqrt(x).
 */
unsigned short FPM_sqrt_dp(unsigned int x);

/**
 * @brief Scaled RMS of a block of samples: sqrt(sum(x^2) / 36).
 *
 * The 1/36 is headroom, keeping the sum inside 32 bits for up to 36
 * full-scale samples; beyond 72 it overflows anyway. See src/dsp/fpm_rms.c.
 *
 * @param samples  Input samples.
 * @param count    Number of samples.
 * @return The scaled RMS.
 */
short FPM_rms(const short *samples, unsigned short count);

/**
 * @brief Reciprocal lookup for fixed-point division.
 * @param denom  The denominator.
 * @param recip  Set to an approximate 1/denom.
 * @param shift  Set to the normalisation shift.
 * @return 1 if @p denom is zero, else 0.
 */
int FPM_div(unsigned short denom, unsigned short *recip, unsigned short *shift);

/**
 * @brief FPM_div() over a 32-bit denominator, off the same table.
 *
 * The mantissa is the top 16 bits after normalisation, so @p shift can
 * reach 31.
 *
 * @param denom  The denominator.
 * @param recip  Set to an approximate 1/denom.
 * @param shift  Set to the normalisation shift.
 * @return 1 if @p denom is zero, else 0.
 */
int FPM_div_32(unsigned int denom, unsigned short *recip,
	       unsigned short *shift);

/** @brief Read one entry of FPM_div()'s reciprocal table, for the generator self-check. */
unsigned short FPM_div_table_entry(int i);
/** @brief Recompute one entry of FPM_div()'s reciprocal table from its ideal formula. */
unsigned short FPM_div_table_generate(int i);

/*
 * `FPM_div_table` is GLOBAL (`R`) in the object, so it is not file-static;
 * the declaration is completed by the definition in `fpm_tables.c`.  The
 * definition is 129 entries -- the object's 128 plus the D4 over-read's
 * value, which is 0 in the bug build and the generated 16384 in the fixed
 * one.  Dropping the 129th in the bug build is not equivalent: our `.rodata`
 * does not reproduce the object's FPM_xor_table adjacency.
 */
extern const unsigned short FPM_div_table[];

/*
 * `FPM_sqrt_table` is a GLOBAL object in the object's `.rodata`
 * (`readelf -sW`), and it is defined in `fpm_tables.c` with the other seven
 * (F11402).  It was once a file-`r` static behind the accessors below; the
 * accessors are retained but now read the global.  The size is the object's
 * 192 plus this tree's documented over-read entry, so it is declared unsized
 * here.
 */
extern const unsigned short FPM_sqrt_table[];

/**
 * @brief Four-quadrant arctangent, built on FPM_div().
 *
 * The angle is in fpm_phasor.c's phase units, a full turn being 0x8000,
 * running anticlockwise from the positive x axis. Both arguments zero
 * gives 0x2000, which is what the `x == 0` branch does rather than a
 * special case. Not exact at two places, faithfully: a ratio of exactly
 * 127/32768 returns zero, and the octant with `x > 0, y < 0, |x| > |y|`
 * is one unit low. Both are explained in src/dsp/fpm_atan.c.
 *
 * @param y      Numerator, atan2 order.
 * @param x      Denominator, atan2 order.
 * @param angle  Set to the resulting angle. The function itself returns
 *               nothing.
 */
void FPM_atan(short y, short x, short *angle);

/* atan of i/256 in Q15 radians, rounded.  Global in the original. */
#define FPM_ATAN_TABLE 257
extern const short FPM_atan_table[FPM_ATAN_TABLE];

/*
 * Popcount of the byte index -- consumers XOR two words and read the
 * Hamming distance out.  256 entries, and its consumers can index it with
 * 16-bit values; see src/dsp/fpm_xor.c for what that means and for the
 * verification against the blob.
 */
extern const short FPM_xor_table[256];

/**
 * @brief Base-10 logarithm of `mantissa * 2^-exponent`, result in Q12.
 *
 * The mantissa is normalised internally, so @p exponent is whatever the
 * caller has already taken out. Reads one element past its table for
 * mantissas of 0x7fc0..0x7fff -- reproduced; see src/dsp/fpm_log10.c.
 *
 * @param mantissa  Normalised mantissa.
 * @param exponent  Exponent already taken out by the caller.
 * @return log10(mantissa * 2^-exponent) in Q12, or 0 for a zero mantissa
 *         (which also announces itself at debug level 2).
 */
short FPM_log10(unsigned short mantissa, short exponent);

/** @brief Recompute one entry of FPM_log10()'s table from its ideal formula. */
short FPM_log10_table_generate(int index);
/** @brief Number of table entries the generator actually derives (vs. copies verbatim). */
int FPM_log10_table_derived(void);
/** @brief Read one entry of FPM_log10()'s table, for the generator self-check. */
short FPM_log10_table_entry(int index);
/** @brief Size of FPM_log10()'s table. */
int FPM_log10_table_size(void);

/* Table introspection, for the generator self-check in the unit tests. */
/** @brief Recompute one entry of FPM_sqrt()'s table from its ideal formula. */
unsigned short FPM_sqrt_table_generate(int index);
/** @brief Read one entry of FPM_sqrt()'s table, for the generator self-check. */
unsigned short FPM_sqrt_table_entry(int index);
/** @brief Size of FPM_sqrt()'s table. */
int FPM_sqrt_table_size(void);

/**
 * @brief One LMS coefficient update.
 *
 * @p coeff and @p hist are walked in opposite directions, so `coeff[0]`
 * is paired with the newest sample and `coeff[taps-1]` with the oldest.
 * Every coefficient moves by `(hist * err + 0x20000) >> 18`, a
 * round-to-nearest at 18 fractional bits. `FPM_FSE_receive` calls this
 * twice per symbol, once for each half of the complex filter.
 *
 * @param coeff  Coefficients to update in place, @p taps entries.
 * @param hist   Circular history buffer, @p taps entries.
 * @param widx   Index of the newest sample in @p hist.
 * @param taps   Number of coefficients/history entries.
 * @param err    Error term driving the update.
 */
void FPM_lmsupd(short *coeff, const short *hist, short widx, short taps,
		short err);

/**
 * @brief Same walk as FPM_lmsupd(), with the correction formed in two
 * rounded stages: `((short)((hist[k] * mu + 0x10) >> 5) * err + 0x10000) >> 17`.
 *
 * The intermediate narrowing to 16 bits is load-bearing, not cosmetic.
 * Argument order is @p err then @p mu -- FPM_lmsupd()'s slot for @p err,
 * then @p mu, the reverse of the order they are applied in. Nothing in
 * the object calls this; the names come from `ecc_adapt`, the same
 * arithmetic with a caller to type it. See src/dsp/fpm_lmsupd.c and
 * finding F8162.
 *
 * @param coeff  Coefficients to update in place, @p taps entries.
 * @param hist   Circular history buffer, @p taps entries.
 * @param widx   Index of the newest sample in @p hist.
 * @param taps   Number of coefficients/history entries.
 * @param err    Error term.
 * @param mu     Step size.
 */
void FPM_lmsupd2(short *coeff, const short *hist, short widx, short taps,
		 short err, short mu);

/**
 * @brief Correlate samples against a circular history and accumulate the
 * scaled result into a coefficient array.
 *
 * The accumulator is 16-bit and truncates every iteration, and the
 * circular wrap is a single conditional add rather than a modulo -- both
 * load-bearing. No caller in the object. See src/dsp/fpm_lmsupd.c and
 * finding F8163.
 *
 * @param coeff  Coefficients to accumulate into, @p taps entries.
 * @param taps   Number of coefficients.
 * @param hist   Circular history buffer, @p hlen entries.
 * @param widx   Index of the newest sample in @p hist.
 * @param hlen   Length of @p hist.
 * @param x      Samples to correlate.
 * @param count  Number of samples in @p x.
 * @param step   Scale applied to each accumulation.
 * @param gain   Additional gain factor.
 */
void FPM_block_update(short *coeff, short taps, const short *hist, short widx,
		      short hlen, const short *x, short count, short step,
		      short gain);

/**
 * @brief Dot a circular history against a strided coefficient array.
 *
 * Walks @p widx down to 0 and then `taps - 1` back down to `widx + 1`
 * while the coefficients advance by @p stride across both halves. Each
 * product is shifted down 3 inside the loop and the remaining
 * `shift - 3` applied to the sum, so @p shift is the TOTAL right shift.
 * Defined in src/dsp/fpm_ecc.c; see finding F8164 for why there rather
 * than in fpm_div.c.
 *
 * @param coeff   Coefficients, strided by @p stride.
 * @param hist    Circular history buffer.
 * @param widx    Index of the newest sample in @p hist.
 * @param taps    Number of taps.
 * @param stride  Stride between successive coefficients.
 * @param shift   Total right shift applied to the sum.
 * @return The dot product, scaled by `2^-shift`.
 */
short FPM_circ_dotp2(const short *coeff, const short *hist, short widx,
		     short taps, short stride, short shift);

#endif /* DSPLIB_FPM_H */
