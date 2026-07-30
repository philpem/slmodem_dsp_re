/*
 * fp_math.h -- Q14 fixed-point helpers used by the V.32 diagnostics path.
 */

#ifndef DSPLIB_FP_MATH_H
#define DSPLIB_FP_MATH_H

/*
 * Signed Q14 divide: returns ceil((|a| << 14) / |b|), negated when a and b
 * have opposite signs.  The result is truncated to 16 bits.
 *
 * WARNING: b == 0 hangs, as it does in the original -- the loop subtracts
 * zero forever.  Callers must not pass it.
 */
short GetFP_Value(short a, short b);

/*
 * e^x in Q14: 1 + x + x^2/2! + ... up to seven terms, stopping early once a
 * term rounds to zero.
 */
int FP_Pow(int x);

/* Table introspection for the generator self-check. */
short FP_Pow_coefficient(int i);
short FP_Pow_coefficient_generate(int i);

#endif /* DSPLIB_FP_MATH_H */
