/*
 * fpm_iir.c -- Q14 biquad cascade.
 *
 * Reconstructed from dsplibs.o fpm_iir.c.  The TU holds three filters:
 *
 *   FPM_iir_filt       .text 0x0a8a50   direct form II, one sample
 *   FPM_iir_filt_block .text 0x0a8b10   not yet reconstructed
 *   FPM_iir_filt_II    .text 0x0a8c30   direct form I, a block
 *
 * FPM_iir_filt: direct form II, 5 coefficients and 2 state words per
 * section.  Two
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

/*
 * FPM_iir_filt_II -- .text 0x0a8c30, 232 bytes.
 *
 * The other biquad engine.  Despite the name it is direct form **I**, not
 * direct form II; the "_II" distinguishes it from FPM_iir_filt above, which
 * is the direct form II one.  Six callers: the tone detector, the fax class 1
 * progress monitor, v23FP's receiver, the channel-bandwidth detector, and
 * FPM_TONE's reversal finder and killer.
 *
 * Per section it keeps four state words -- two past inputs and two past
 * outputs -- rather than the two of a form II cascade:
 *
 *     state[0] = x[n-2]   state[1] = x[n-1]
 *     state[2] = y[n-2]   state[3] = y[n-1]
 *
 * OLDEST FIRST, which is why the coefficients are stored { b0, b2, b1, a2, a1 }
 * and not the textbook { b0, b1, b2, a1, a2 }: coefficient k simply multiplies
 * state word k-1.  Regular in the code, surprising on paper, and swapping the
 * pairs yields a filter that still runs and is simply the wrong one.
 *
 * Two details that matter for bit-exactness:
 *
 *   - the numerator and denominator are shifted down by 14 SEPARATELY and
 *     then subtracted, so there are two truncations per section, not one;
 *   - nothing saturates.  The section output is a plain 16-bit truncation,
 *     unlike FPM_iir_filt, which clamps.  An unstable or over-driven section
 *     wraps around here rather than sticking at the rail.
 */
void
FPM_iir_filt_II(short *samples, const short *coeff, short *state,
		short sections, short count)
{
	int i;

	for (i = 0; i < count; i++) {
		const short *c = coeff;
		short *s = state;
		int acc = samples[i];
		int j;

		/*
		 * Counted down from sections-1 and tested against -1, as a
		 * 16-bit value.  Written out rather than as `j < sections`
		 * because a negative `sections` behaves differently: the
		 * original runs 65535 sections and walks off the state array,
		 * and a `j > 0` loop would quietly do nothing instead.
		 */
		for (j = (short)(sections - 1); j != -1; j = (short)(j - 1)) {
			int num, den, x1;

			/* Numerator: b0*x[n] + b2*x[n-2] + b1*x[n-1] */
			num = c[0] * acc + c[1] * s[0];
			x1 = s[1];
			s[0] = (short)x1;	/* x[n-2] <- x[n-1] */
			s[1] = (short)acc;	/* x[n-1] <- x[n]   */
			num += c[2] * x1;
			num >>= 14;

			/* Denominator: a2*y[n-2] + a1*y[n-1] */
			den = c[3] * s[2];
			x1 = s[3];
			s[2] = (short)x1;	/* y[n-2] <- y[n-1] */
			den += c[4] * x1;
			den >>= 14;

			acc = (short)(num - den);
			s[3] = (short)acc;	/* y[n-1] <- y[n]   */

			c += FPM_IIR_II_COEFF_PER_SECTION;
			s += FPM_IIR_II_STATE_PER_SECTION;
		}

		samples[i] = (short)acc;
	}
}
