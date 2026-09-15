/*
 * v22_iir.c -- V.22 / V.22bis: the receiver's input IIR and its mixer.
 *
 * Reconstructed from dsplibs.o v22_iir.c:
 *
 *   V22_iir_filt_demod   .text 0x08cee0, 232 bytes
 *   V22IIRFilterInit     .text 0x08cfd0, 130 bytes
 *   IIR_b_coeff          .data 0x0077a0,   8 bytes
 *   IIR_a_coeff          .data 0x007798,   8 bytes
 *
 * The two call sites settle the signatures, so neither is inferred:
 *
 *   V22FP_create   +0x858  V22IIRFilterInit(obj->..., IIR_b_coeff, IIR_a_coeff)
 *   DemodDataV22   +0x1f2  V22_iir_filt_demod(block, s, s+4, s+8, s+12, mix)
 *
 * where `s` is the sixteen-short state the init laid out and `mix` is the
 * 160-entry carrier FPM_TONE_generate_demod filled two instructions earlier.
 * So this one function is the whole front of the V.22 receiver: bandpass,
 * then coherent mixdown.
 *
 * ARITHMETIC, and every one of these is load-bearing:
 *
 *   - each product is shifted down by 14 SEPARATELY before it joins the
 *     accumulator, so a 160-sample block truncates 7 times per sample rather
 *     than once.  Accumulating first and shifting once gives a different
 *     answer and a perfectly reasonable-looking filter.
 *   - the accumulator is 32-bit and is truncated to 16 bits exactly once,
 *     where it is stored into y[0].  The mixer then multiplies the TRUNCATED
 *     value, not the accumulator -- `mov %bx,(%esi)` and `movswl %bx,%eax`
 *     read the same low half.
 *   - nothing saturates anywhere.  An over-driven filter wraps.
 *   - the histories are shifted AFTER the accumulate and BEFORE y[0] is
 *     stored, which is why x[0] survives one iteration longer than it looks
 *     like it should: it is overwritten at the top of the next pass.
 *
 * The loop counters are all 16-bit.  They are written as `short` here for the
 * same reason FPM_iir_filt_II's are: the comparisons in the object are `cmpw`,
 * and a 32-bit counter would differ if a count ever reached 32768.  It cannot
 * here -- the bounds are constants -- but the shape is the original's.
 */

#include "dsplib/v22_iir.h"

/*
 * Numerator, third order.  Antisymmetric about its centre
 * ({ k, -3k, 3k, -k } to within a percent), which is the signature of a
 * differencing highpass rather than of a designed bandpass -- it is the
 * zeros of a third-order Butterworth highpass at Q14.
 *
 * Reference bytes, extracted by tools/tabdump.py.  Deriving the design that
 * produced them is deferred with every other coefficient derivation in this
 * tree; the differential test proves the copy, not the design.
 */
short IIR_b_coeff[V22_IIR_B_TAPS] = {
	3064, -9191, 9191, -3064,
};

/*
 * Denominator.  a[0] is 16384 -- Q14's 1.0 -- and is never loaded by
 * anything: the >> 14 in the accumulate already divides by it.  Init copies
 * it anyway, so it is part of the state and part of what the differential
 * test compares.
 */
short IIR_a_coeff[V22_IIR_A_TAPS] = {
	16384, -2359, 5551, -215,
};

void
V22_iir_filt_demod(short *samples, const short *b, const short *a,
		   short *xhist, short *yhist, const short *mix)
{
	short i;

	for (i = 0; i <= V22_IIR_BLOCK - 1; i++) {
		int acc = 0;
		short j;
		short y;

		xhist[0] = samples[i];

		/* Numerator: four taps over x[0..3], x[0] being this sample. */
		for (j = 0; j <= V22_IIR_B_TAPS - 1; j++)
			acc += (b[j] * xhist[j]) >> 14;

		/*
		 * Denominator: three taps over y[0..2], read from a + 1.  The
		 * object's addressing is `movswl 0x2(%edx,%ecx,2)` with ecx
		 * running 0..2 -- a[1], a[2], a[3].
		 */
		for (j = 0; j <= V22_IIR_A_USED - 1; j++)
			acc -= (a[j + 1] * yhist[j]) >> 14;

		/* Age both histories.  x has four slots, y three. */
		for (j = V22_IIR_X_HIST - 1; j > 0; j--)
			xhist[j] = xhist[j - 1];
		for (j = V22_IIR_Y_USED - 1; j > 0; j--)
			yhist[j] = yhist[j - 1];

		/* One truncation, and the mixer sees the truncated value. */
		y = (short)acc;
		yhist[0] = y;

		samples[i] = (short)((y * mix[i]) >> 12);
	}
}

void
V22IIRFilterInit(short *state, const short *b, const short *a)
{
	short i;

	for (i = 0; i <= V22_IIR_B_TAPS - 1; i++)
		state[V22_IIR_OFF_B + i] = b[i];

	for (i = 0; i <= V22_IIR_A_TAPS - 1; i++)
		state[V22_IIR_OFF_A + i] = a[i];

	for (i = 0; i <= V22_IIR_X_HIST - 1; i++)
		state[V22_IIR_OFF_X + i] = 0;

	for (i = 0; i <= V22_IIR_Y_HIST - 1; i++)
		state[V22_IIR_OFF_Y + i] = 0;
}
