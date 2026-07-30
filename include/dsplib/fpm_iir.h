/*
 * fpm_iir.h -- Q14 biquad cascade.
 *
 * The shared filter engine: six callers across the tone detector, the FSK
 * demodulator, CID, DTMF, the bandpass helper and the channel-bandwidth
 * detector.
 *
 * Layout is 5 coefficients and 2 state words per section, direct form II.
 */

#ifndef DSPLIB_FPM_IIR_H
#define DSPLIB_FPM_IIR_H

#define FPM_IIR_COEFF_PER_SECTION 5
#define FPM_IIR_STATE_PER_SECTION 2

/*
 * Filter one sample through `sections` cascaded biquads.  `state` is updated
 * in place and must hold 2 * sections words.
 */
short FPM_iir_filt(short x, const short *coeff, short *state, short sections);

/* Wideband reference filter: one section, used for total-energy estimates. */
extern const short COEF_DC[FPM_IIR_COEFF_PER_SECTION];

#endif /* DSPLIB_FPM_IIR_H */
