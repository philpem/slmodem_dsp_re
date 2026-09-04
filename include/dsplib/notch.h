/*
 * notch.h -- the library's generic second-order notch section.
 *
 * `Notch.c` in the original is one function and nothing else: 56 bytes at
 * .text 0x0af150, sitting alone between `Fifo8.c` and `Rx.c` in the link
 * order.  It is NOT the same thing as `notch_filter` (0x0783b0, 161 bytes,
 * a different translation unit) -- that confusion is easy to make from the
 * names alone and there is nothing else linking the two.
 *
 * The section is a transposed direct form II biquad, four coefficients and
 * two state words, all `float`:
 *
 *     w      = state[0] + coef[3] * x
 *     state[0] = coef[1] * w + coef[0] * (coef[3] * x) + state[1]
 *     state[1] = coef[3] * x + coef[2] * w
 *     return w
 *
 * which is
 *
 *                       1 + c0 z^-1 + z^-2
 *     H(z) =  c3  * ---------------------------
 *                    1 - c1 z^-1 - c2 z^-2
 *
 * The callers' coefficient banks all have the same shape, and it is what
 * makes the reading above checkable rather than asserted (finding F1411):
 *
 *     c0 = -2 cos(w0)        zeros ON the unit circle, at +-w0
 *     c1 =  2 r cos(w0)      poles at r e^{+-j w0}
 *     c2 = -r^2
 *     c3 =  r                the section's gain
 *
 * so c2 = -c3*c3 and c1 = -c0*c3 hold in every bank in the object, to the
 * last digit a float carries.  A notch, then: the zeros cancel w0 outright
 * and the poles at radius r set how narrow the null is.
 */

#ifndef DSPLIB_NOTCH_H
#define DSPLIB_NOTCH_H

/**
 * @brief Run one sample through one transposed direct-form-II notch section.
 *
 * Computes the recursive node first, then updates both state words, and
 * returns the node itself (not a re-read of state) -- the object's own
 * statement order. All arithmetic is `float`, at the x87's natural 80-bit
 * intermediate precision.
 *
 * @param x      Input sample.
 * @param state  Section state, two floats; updated in place.
 * @param coef   Section coefficients, four floats (see the file comment
 *               above for what each one means). Neither array is
 *               bounds-checked; the caller strides them itself.
 * @return The filtered output sample.
 */
float notch(float x, float *state, const float *coef);

#endif /* DSPLIB_NOTCH_H */
