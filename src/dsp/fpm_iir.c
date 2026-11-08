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
	short acc_in = x;
	/* The return carrier is assigned only inside the section loop. */
	short output;

#ifndef DSPLIB_REPRODUCE_BUGS
	/* An empty or invalid cascade leaves the input unchanged. */
	if (sections <= 0)
		return x;
#endif

	while (sections--) {
		int w1 = state[0];
		int w2;
		int acc;
		int ff;
		int w;

		acc = acc_in + ((*coeff++ * w1 + 0x2000) >> 14);
		ff = (*coeff++ * w1) >> 14;

		w2 = state[1];
		*state++ = (short)w2;		/* shift the delay line */

		acc += (*coeff++ * w2 + 0x2000) >> 14;
		ff += (*coeff++ * w2) >> 14;
		ff = (short)ff;

		/* Saturate only here, and asymmetrically -- see the note above. */
		if (acc > 0x7fff)
			w = 0x7fff;
		else if (acc <= -0x8000)
			w = -0x7fff;
		else
			w = (short)acc;

		*state++ = (short)w;

		/* The section output becomes the next section's input. */
		output = (short)((short)ff + ((*coeff++ * w) >> 14));
		acc_in = output;

	}

	return output;
}

/*
 * FPM_iir_filt_block -- .text 0x0a8b10, 287 bytes.
 *
 * FPM_iir_filt over a block, in place.  It is the SATURATING direct form II
 * engine applied `count` times. Its inner arithmetic matches the scalar
 * filter: the same recursive rounding, feedforward narrowing and asymmetric
 * clamp. Arithmetic identity alone does not establish that the original
 * block called the scalar helper and relied on compiler inlining.
 *
 * The reference saves the initial coefficient/history pointers before the
 * outer guard, restores those snapshots for each sample, and advances the
 * sample cursor before the section loop. The explicit body below preserves
 * those consumed parameter cursors. Passing independent by-value cursors to
 * the scalar helper does not recover that ownership graph. F12122.
 *
 * `sections - 1` is hoisted out of the outer loop by the compiler, and the
 * inner counter is decremented as a 16-bit value and tested against -1, the
 * same shape FPM_iir_filt_II uses below.  The outer counter is a `short` too:
 * the object re-narrows it with `cwtl` after each increment and compares 16
 * bits against `count`.
 *
 * THE REPRODUCTION DIFFERENTIAL DOES NOT DRIVE `sections == 0`. The write-
 * back at 0x0a8c19 stores `%dx`, which on that path is never assigned -- the
 * inner loop is guarded by `cmpw $0xffff` at 0x0a8b66 and the register carries
 * in whatever the caller left.  Any value our compiler puts there is as
 * defensible as the object's, so a differential check on that input would be
 * comparing two arbitrary choices. Normal-build contract tests separately
 * check that the guarded repair leaves samples unchanged. Deviation D393.
 */
void
FPM_iir_filt_block(short *samples, const short *coeff, short *state,
		   short sections, short count)
{
	const short *initial_coeff = coeff;
	short *initial_state = state;
	short i;

	for (i = 0; i < count; i++) {
		short acc_in = *samples++;
		short output;
		short left = sections;

		coeff = initial_coeff;
		state = initial_state;
#ifndef DSPLIB_REPRODUCE_BUGS
		if (sections <= 0) {
			samples[-1] = acc_in;
			continue;
		}
#endif
		while (left--) {
			int w1 = state[0];
			int w2;
			int acc;
			int ff;
			int w;

			acc = acc_in + ((*coeff++ * w1 + 0x2000) >> 14);
			ff = (*coeff++ * w1) >> 14;

			w2 = state[1];
			*state++ = (short)w2;		/* shift the delay line */

			acc += (*coeff++ * w2 + 0x2000) >> 14;
			ff += (*coeff++ * w2) >> 14;
			ff = (short)ff;

			/* Saturate only here, and asymmetrically -- see the note above. */
			if (acc > 0x7fff)
				w = 0x7fff;
			else if (acc <= -0x8000)
				w = -0x7fff;
			else
				w = (short)acc;

			*state++ = (short)w;

			/* The section output becomes the next section's input. */
			output = (short)((short)ff + ((*coeff++ * w) >> 14));
			acc_in = output;
		}

		samples[-1] = output;
	}
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
	const short *initial_coeff = coeff;
	short *initial_state = state;
	short i;

	for (i = 0; i < count; i++) {
		coeff = initial_coeff;
		state = initial_state;
		int acc = samples[i];
		short j = sections;

		/* The short countdown wraps: -1 processes 65535 sections.
		 * A positive-only guard would change the reference behaviour. */
		while (j--) {
			int num, den, x1, coeff2;

			/* Numerator: b0*x[n] + b2*x[n-2] + b1*x[n-1] */
			num = coeff[0] * acc + coeff[1] * state[0];
			x1 = state[1];
			state[0] = (short)x1;	/* x[n-2] <- x[n-1] */
			/* Read before state[1] can overwrite an aliased coefficient. */
			coeff2 = coeff[2];
			state[1] = (short)acc;	/* x[n-1] <- x[n]   */
			num += coeff2 * x1;
			num >>= 14;

			/* Denominator: a2*y[n-2] + a1*y[n-1] */
			den = coeff[3] * state[2];
			x1 = state[3];
			state[2] = (short)x1;	/* y[n-2] <- y[n-1] */
			den += coeff[4] * x1;
			den >>= 14;

			acc = (short)(num - den);
			state[3] = (short)acc;	/* y[n-1] <- y[n]   */

			coeff += FPM_IIR_II_COEFF_PER_SECTION;
			state += FPM_IIR_II_STATE_PER_SECTION;
		}

		samples[i] = (short)acc;
	}
}
