/*
 * fpm_iir.h -- Fixed Point Modem: IIR biquad cascades.
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

/*
 * Wideband reference filter: one section, used for total-energy estimates.
 *
 * NOT `const`, because the object's is `D` and not `R` -- 0x081d0 in `.data`.
 * It is defined in `src/dsp/fpm_mtd.c`, not here and not in a coefficient
 * file of its own: the object's `.data` runs DEF_COEFS (a fpm_mtd.c local)
 * straight into COEF_DC with no padding, and COEF_DC straight into
 * fpm_phasor.c's `FPM_sin_sign` with the two bytes a translation-unit
 * boundary costs.  In the OBJECT that makes COEF_DC's tail the SINE's
 * quadrant sign over a quarter of the phasor's phase range; in OURS it no
 * longer does, because `fpm_phasor.c` carries those four words as values of
 * its own rather than reading whatever the linker put below its table.  The
 * attribution above is unaffected -- it rests on the address, on the single
 * reference at .text 0x0a921a and on the `D` binding.  Findings F3621, F3624
 * and 3700, deviation D392.
 */
extern short COEF_DC[FPM_IIR_COEFF_PER_SECTION];

#endif /* DSPLIB_FPM_IIR_H */
