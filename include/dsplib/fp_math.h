/*
 * fp_math.h -- Fixed Point maths helpers.
 */

#ifndef DSPLIB_FP_MATH_H
#define DSPLIB_FP_MATH_H

/**
 * @brief Signed Q14 divide.
 *
 * Returns ceil((|a| << 14) / |b|), negated when @p a and @p b have opposite
 * signs, truncated to 16 bits.
 *
 * @param a  Dividend.
 * @param b  Divisor. Must not be 0: the original divides by repeated
 *           subtraction and a zero divisor hangs it forever; this
 *           reconstruction computes the result directly but preserves that
 *           same precondition rather than defining a behavior the object
 *           never had.
 * @return The Q14 quotient, sign-adjusted.
 */
short GetFP_Value(short a, short b);

/**
 * @brief e^x in Q14 fixed point, via a truncated Maclaurin series.
 *
 * Computes 1 + x + x^2/2! + ... up to seven terms, stopping early once a
 * term rounds to zero. Not a general power function -- see the table's own
 * comment in fp_math.c for why the coefficients pin this down to exp().
 *
 * @param x  Argument in Q14.
 * @return e^x in Q14.
 */
int FP_Pow(int x);

/**
 * @brief Read one of FP_Pow()'s seven precomputed Maclaurin coefficients.
 * @param i  Term index, 0..6. Out of range returns 0.
 * @return The stored Q14 coefficient (16384 / (i+1)!, rounded).
 */
short FP_Pow_coefficient(int i);

/**
 * @brief Recompute a Maclaurin coefficient from its ideal value, for the
 * generator self-check.
 *
 * Compared against FP_Pow_coefficient() within a tolerance of 1, not for
 * exact equality -- the object's own rounding of the seven stored constants
 * is not fully consistent (see fp_math.c), so this recovers the design
 * without claiming to reproduce the exact rounding.
 *
 * @param i  Term index, 0..6.
 * @return floor(16384.0 / (i+1)!) as a double-precision computation cast to short.
 */
short FP_Pow_coefficient_generate(int i);

#endif /* DSPLIB_FP_MATH_H */
