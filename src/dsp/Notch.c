/*
 * Notch.c -- reconstructed from dsplibs.o Notch.c.
 *
 *   notch  .text 0x0af150  56 bytes
 *
 * The whole translation unit.  See include/dsplib/notch.h for the transfer
 * function and for how the coefficient banks pin the meaning of each of the
 * four coefficients.
 *
 * The statement order below is the object's own: the recursive node `w` is
 * formed first, then state[0], then state[1], and the return value is `w`
 * itself and not a re-read of state.  Everything is float, computed on the
 * x87 stack at 80-bit intermediate precision -- the object is -mfpmath=387
 * and so is this build.
 */

#include "dsplib/notch.h"

float
notch(float x, float *state, const float *coef)
{
	float in = coef[3] * x;
	float w = state[0] + in;

	state[0] = coef[1] * w + coef[0] * in + state[1];
	state[1] = in + coef[2] * w;

	return w;
}
