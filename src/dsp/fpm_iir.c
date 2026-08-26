/*
 * fpm_iir.c -- Fixed Point Modem: IIR biquad cascades.
 *
 * Reconstructed from dsplibs.o fpm_iir.c.  The TU holds three filters:
 *
 *   FPM_iir_filt       .text 0x0a8a50   direct form II, one sample
 *   FPM_iir_filt_block .text 0x0a8b10   direct form II, a block
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
 * FPM_iir_filt_block -- .text 0x0a8b10, 287 bytes.
 *
 * FPM_iir_filt over a block, in place.  It is the SATURATING direct form II
 * engine applied `count` times, not a second filter: the object's inner loop
 * is FPM_iir_filt's body instruction for instruction -- the same 0x2000
 * rounding on the two recursive terms, the same truncation on the two
 * feedforward ones, and the same asymmetric clamp to -0x7fff.  GCC inlines the
 * call at -O3 within the translation unit, so the object shows no `call` here
 * and FPM_iir_filt keeps its own symbol regardless (compare finding F7940) --
 * and our build inlines it too, which was checked rather than assumed.
 *
 * THE CODEGEN IS NOT CLOSED: 72 instructions against the object's 86, 29 bytes
 * short, rejected at grade 1 as a missing statement rather than a renaming.
 * The differential tier is green over the whole driven domain and this is the
 * same class and size of residual `FPM_iir_filt` and `FPM_iir_filt_II` above
 * and below it already carry (10 and 19 bytes), so it is a refinement target
 * and not a defect.  Finding F8166 has the table; do not read the one-line
 * body below as "reproduces the object" without re-running `byteident.py`.
 *
 * `sections - 1` is hoisted out of the outer loop by the compiler, and the
 * inner counter is decremented as a 16-bit value and tested against -1, the
 * same shape FPM_iir_filt_II uses below.  The outer counter is a `short` too:
 * the object re-narrows it with `cwtl` after each increment and compares 16
 * bits against `count`.
 *
 * NOTHING HERE IS TESTED AT `sections == 0`, on purpose.  The object's write-
 * back at 0x0a8c19 stores `%dx`, which on that path is never assigned -- the
 * inner loop is guarded by `cmpw $0xffff` at 0x0a8b66 and the register carries
 * in whatever the caller left.  Any value our compiler puts there is as
 * defensible as the object's, so a differential check on that input would be
 * comparing two arbitrary choices.  Deviation D393.
 */
void
FPM_iir_filt_block(short *samples, const short *coeff, short *state,
		   short sections, short count)
{
	short i;

	for (i = 0; i < count; i++)
		samples[i] = FPM_iir_filt(samples[i], coeff, state, sections);
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
