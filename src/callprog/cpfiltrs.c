/*
 * cpfiltrs.c -- Call Progress Filters: the band-limiting elliptic cascade.
 *
 * Reconstructed from dsplibs.o CPfiltrs.c:
 *
 *   _iir_filter_create     .text 0x07c960
 *   _iir_filter_delete     .text 0x07ca30
 *   _iir_filter_progress   .text 0x07ca50
 *
 * WHAT THE FILTER IS
 *
 * The coefficients CALLPROG_Create installs (.rodata+0x5d84, and reproduced
 * in callprog.c where they belong) describe an eighth-order elliptic
 * bandpass.  Every section has its zeros on the unit circle, which is what
 * makes it elliptic rather than Butterworth or Chebyshev-I, and gives the
 * response its four stopband nulls:
 *
 *   section  pole                     zero        what the zero rejects
 *   0        r 0.669  @  892 Hz       3845 Hz     top of the band
 *   1        r 0.800  @ 1571 Hz         38 Hz     hum and DC wander
 *   2        r 0.966  @  203 Hz       2712 Hz     the deep out-of-band null
 *   3        r 0.583  @ 2647 Hz       1179 Hz     shapes the passband
 *
 * End to end at 9600 Hz that is about -3 dB at DC, flat within a decibel from
 * 200 Hz to 1600 Hz, and past 1600 Hz rolling off into a 27 dB notch at
 * 2712 Hz.  Call-progress tones all live between 300 and 650 Hz, so the
 * passband is generous; what matters is the skirt, which keeps speech
 * energy and the far end's answer tone out of the cadence detector.
 *
 * SCALING
 *
 * Q13, not the Q14 used everywhere else in this library.  The extra bit of
 * headroom is spent on the coefficients: section 3's b0 is 2.0, which does
 * not fit in Q14 at all.  That is the whole reason for the difference.
 *
 * The cascade has roughly +30 dB of passband gain -- an artefact of writing
 * each section with b0 = 1.0 rather than normalising -- so shift[0] divides
 * the input by 32 before section 0 sees it.  Net gain is then about unity.
 * Doing it up front rather than at the end is the right choice here: the
 * high-Q section 2 would otherwise clip long before the output did.
 *
 * There is no saturation anywhere.  Each section truncates its accumulator to
 * 16 bits and wraps.  With the deployed shift that needs an input well past
 * full scale, but it is a property of the filter, not a guarantee.
 */

#include "dsplib/cpfiltrs.h"
#include "dsplib/sysdep.h"

struct cp_iir *
_iir_filter_create(struct cp_iir *f, int n_a, int n_b, const short *a,
		   const short *b, const short *shift)
{
	int i;

	/*
	 * Pass NULL to allocate.  Note what is missing: the result is not
	 * checked, and nothing records that this object owns its memory --
	 * `_iir_filter_delete` frees whatever it is given.  A caller that
	 * passes a stack buffer here and later deletes it will free a stack
	 * address.  CALLPROG_Create passes NULL, so the original never does.
	 */
	if (f == 0)
		f = (struct cp_iir *)sysdep_malloc(sizeof(*f));

	/* All 25 words of each, not just the 12 and 8 the four sections use. */
	for (i = 0; i < CP_IIR_MAX_COEFF; i++) {
		f->x[i] = 0;
		f->y[i] = 0;
	}

	for (i = 0; i < n_a; i++)
		f->a[i] = a[i];
	for (i = 0; i < n_b; i++)
		f->b[i] = b[i];

	f->n_b = n_b;
	f->n_a_minus_1 = n_a - 1;

	for (i = 0; i < CP_IIR_SECTIONS + 1; i++)
		f->shift[i] = shift[i];

	return f;
}

void
_iir_filter_delete(struct cp_iir *f)
{
	if (f != 0)
		sysdep_free(f);
}

void
_iir_filter_progress(struct cp_iir *f, int count, short *samples)
{
	int i;

	for (i = 0; i < count; i++) {
		int v = samples[i] >> f->shift[0];
		int s;

		for (s = 0; s < CP_IIR_SECTIONS; s++) {
			short *x = &f->x[3 * s];
			short *y = &f->y[2 * s];
			const short *a = &f->a[3 * s];
			const short *b = &f->b[3 * s];
			int acc = 0;
			int k;

			x[0] = (short)v;

			/*
			 * Accumulated in 32 bits and allowed to wrap, which is
			 * what the original's `add`/`sub` do.  Each individual
			 * product fits; five of them need not.  Spelled
			 * through unsigned so the wrap is defined rather than
			 * left to the compiler.
			 */
			for (k = 0; k <= 2; k++)
				acc = (int)((unsigned)acc
					    + (unsigned)(x[k] * b[k]));
			for (k = 1; k <= 2; k++)
				acc = (int)((unsigned)acc
					    - (unsigned)(y[k - 1] * a[k]));

			/* Truncating, not rounding; wrapping, not saturating. */
			v = (short)(acc >> CP_IIR_SHIFT);

			x[2] = x[1];
			x[1] = x[0];
			y[1] = y[0];
			y[0] = (short)v;

			/*
			 * Rescale on the way out.  The first three shifts are
			 * applied unconditionally; the last only when
			 * positive.  That asymmetry is the original's and is
			 * invisible unless a shift is negative, in which case
			 * x86 would mask the count to five bits and shift
			 * right by a large amount rather than left.
			 */
			if (s < CP_IIR_SECTIONS - 1 || f->shift[s + 1] > 0)
				v >>= f->shift[s + 1];
		}

		samples[i] = (short)v;
	}
}
