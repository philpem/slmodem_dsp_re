/*
 * v22_iir.h -- V.22 / V.22bis: the receiver's input IIR and its mixer.
 *
 * V.22 does NOT use the shared `FPM_iir_filt` engines.  It carries its own
 * third-order direct-form-I filter in the author's own translation unit
 * `v22_iir.c`, and the two differ in ways that are not cosmetic:
 *
 *   - three poles and three zeros, not a cascade of biquads;
 *   - the denominator coefficient array is read from index 1, so a[0] is
 *     never loaded -- it holds the 16384 that the >> 14 already implies;
 *   - the filter and the demodulating mixer are one function, so the
 *     filtered sample never exists as a separate array;
 *   - the block length is fixed at 160 samples in the code, not passed.
 *
 * THE STATE IS ONE FLAT ARRAY OF SIXTEEN SHORTS, laid out by
 * `V22IIRFilterInit` and then handed to `V22_iir_filt_demod` as four
 * separate pointers into itself:
 *
 *     +0   b[0..3]   numerator, copied from the caller's table
 *     +4   a[0..3]   denominator, copied; a[0] is stored but never read
 *     +8   x[0..3]   input history, cleared
 *     +12  y[0..3]   output history, cleared -- the filter uses three of
 *                    them, so y[3] is written by init and by nothing else
 *
 * V22FP_create calls the init with { IIR_b_coeff, IIR_a_coeff }, and
 * DemodDataV22 calls the filter with `state+0, state+4, state+8, state+12`
 * and a mixer array that FPM_TONE_generate_demod has just filled.
 */

#ifndef DSPLIB_V22_IIR_H
#define DSPLIB_V22_IIR_H

/* Numerator taps.  Third order, so four of them. */
#define V22_IIR_B_TAPS		4

/*
 * Denominator taps as STORED.  Only a[1], a[2] and a[3] are ever loaded --
 * the filter's inner loop runs three times from a base of a+1.
 */
#define V22_IIR_A_TAPS		4
#define V22_IIR_A_USED		3

/* History depths.  Init clears four of each; the filter uses 4 and 3. */
#define V22_IIR_X_HIST		4
#define V22_IIR_Y_HIST		4
#define V22_IIR_Y_USED		3

/* The whole state, in shorts, and the offsets of its four parts. */
#define V22_IIR_STATE_WORDS	16
#define V22_IIR_OFF_B		0
#define V22_IIR_OFF_A		4
#define V22_IIR_OFF_X		8
#define V22_IIR_OFF_Y		12

/*
 * The block length, which is a constant in the code rather than an argument.
 * 160 samples is 20 ms at 8 kHz -- the datapump interface's block.
 */
#define V22_IIR_BLOCK		160

/**
 * @brief Load the V.22 receive IIR's state with a coefficient set and clear both histories.
 * @param state  The state array; must have room for V22_IIR_STATE_WORDS shorts.
 * @param b      Numerator coefficients (V22_IIR_B_TAPS entries), copied in.
 * @param a      Denominator coefficients (V22_IIR_A_TAPS entries), copied in (a[0] is stored but never read).
 */
void V22IIRFilterInit(short *state, const short *b, const short *a);

/**
 * @brief Filter and demodulate one block through the V.22 receive IIR.
 *
 * Filters V22_IIR_BLOCK samples in place and multiplies each result by the
 * corresponding entry of @p mix.
 *
 * The four pointers are the four parts of one state array; they are
 * passed separately because that is how the original's single caller
 * passes them. @p mix is the demodulating carrier, at Q12 -- hence the
 * `>> 12` that the numerator's and denominator's own `>> 14` do not
 * account for.
 *
 * @param samples  The block, filtered and demodulated in place.
 * @param b        Numerator coefficients (state's +V22_IIR_OFF_B part).
 * @param a        Denominator coefficients (state's +V22_IIR_OFF_A part).
 * @param xhist    Input history (state's +V22_IIR_OFF_X part).
 * @param yhist    Output history (state's +V22_IIR_OFF_Y part).
 * @param mix      The demodulating carrier, Q12, one entry per sample.
 */
void V22_iir_filt_demod(short *samples, const short *b, const short *a,
			short *xhist, short *yhist, const short *mix);

/*
 * The receiver's fixed coefficients.  NOT const: the original places both in
 * .data, not .rodata.
 */
extern short IIR_b_coeff[V22_IIR_B_TAPS];
extern short IIR_a_coeff[V22_IIR_A_TAPS];

#endif /* DSPLIB_V22_IIR_H */
