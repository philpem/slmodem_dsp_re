/*
 * fpm_iir_coeffs.c -- shared biquad coefficient sets.
 *
 * Extracted from dsplibs.o.  These are reference bytes; where a design has
 * been recovered it is recorded in docs/coefficients.md.
 */

#include "dsplib/fpm_iir.h"

/*
 * COEF_DC (.data:0x81d0) -- one section, the wideband reference filter used
 * by the tone and DTMF detectors to estimate total signal energy.
 *
 * Layout is FPM_iir_filt's: two recursive coefficients, two feedforward, and
 * an output scale.
 */
const short COEF_DC[FPM_IIR_COEFF_PER_SECTION] = {
	-12971, 12917, 28620, -25834, 12917,
};
