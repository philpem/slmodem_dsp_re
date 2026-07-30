/*
 * fpm_iir.c -- Q14 biquad cascade.
 *
 * Reconstructed from dsplibs.o fpm_iir.c, FPM_iir_filt at .text 0x0a8a50.
 * FPM_iir_filt_block and FPM_iir_filt_II are not yet reconstructed.
 *
 * Direct form II, 5 coefficients and 2 state words per section.  Two
 * asymmetries are deliberate and must be preserved:
 *
 *   - Only the recursive node saturates, and its lower limit is -0x7fff, not
 *     -0x8000.  The feedforward sum is merely truncated to 16 bits.
 *   - The two terms feeding the recursive accumulator round (they add 0x2000
 *     before shifting); the two feeding the feedforward sum truncate.
 *
 * Making either symmetric would change the output.
 */

#include "dsplib/fpm_iir.h"

short
FPM_iir_filt(short x, const short *coeff, short *state, short sections)
{
	int acc_in = x;
	int i;

	for (i = 0; i < sections; i++) {
		int w1 = state[0];
		int w2;
		int acc;
		int ff;
		int w;

		acc = acc_in + ((coeff[0] * w1 + 0x2000) >> 14);
		ff = (coeff[1] * w1) >> 14;

		w2 = state[1];
		state[0] = (short)w2;		/* shift the delay line */

		acc += (coeff[2] * w2 + 0x2000) >> 14;
		ff += (coeff[3] * w2) >> 14;

		/* Saturate only here, and asymmetrically -- see the note above. */
		if (acc > 0x7fff)
			w = 0x7fff;
		else if (acc <= -0x8000)
			w = -0x7fff;
		else
			w = (short)acc;

		state[1] = (short)w;

		/* The section output becomes the next section's input. */
		acc_in = (short)((short)ff + ((coeff[4] * w) >> 14));

		coeff += FPM_IIR_COEFF_PER_SECTION;
		state += FPM_IIR_STATE_PER_SECTION;
	}

	return (short)acc_in;
}
