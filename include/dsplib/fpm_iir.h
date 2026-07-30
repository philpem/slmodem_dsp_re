/*
 * fpm_iir.h -- Q14 biquad cascade.
 *
 * The shared filter engine: six callers across the tone detector, the FSK
 * demodulator, CID, DTMF, the bandpass helper and the channel-bandwidth
 * detector.
 *
 * There are two of them and they are NOT interchangeable:
 *
 *   FPM_iir_filt      one sample, direct form II, 2 state words/section,
 *                     saturating
 *   FPM_iir_filt_II   a block, direct form I, 4 state words/section, no
 *                     saturation at all
 *
 * The `_II` is a version number, not "direct form II" -- it is the direct
 * form *I* one.  Reading it the other way gets you the wrong state size and
 * the wrong coefficient order, and the result still filters, just wrongly.
 */

#ifndef DSPLIB_FPM_IIR_H
#define DSPLIB_FPM_IIR_H

#define FPM_IIR_COEFF_PER_SECTION 5
#define FPM_IIR_STATE_PER_SECTION 2

/* Direct form I keeps two past inputs and two past outputs per section. */
#define FPM_IIR_II_COEFF_PER_SECTION 5
#define FPM_IIR_II_STATE_PER_SECTION 4

/*
 * Filter one sample through `sections` cascaded biquads.  `state` is updated
 * in place and must hold 2 * sections words.
 */
short FPM_iir_filt(short x, const short *coeff, short *state, short sections);

/*
 * Filter `count` samples in place through `sections` cascaded biquads,
 * direct form I.  `state` must hold 4 * sections words.
 *
 * COEFFICIENT ORDER IS { b0, b2, b1, a2, a1 } -- oldest-first within each
 * half, to match the history layout.  Not the usual { b0, b1, b2, a1, a2 }.
 * See src/dsp/fpm_iir.c.
 */
void FPM_iir_filt_II(short *samples, const short *coeff, short *state,
		     short sections, short count);

/* Wideband reference filter: one section, used for total-energy estimates. */
extern const short COEF_DC[FPM_IIR_COEFF_PER_SECTION];

#endif /* DSPLIB_FPM_IIR_H */
