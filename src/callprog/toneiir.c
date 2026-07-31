/*
 * toneiir.c -- Tone IIR: the four-section filter engine.
 *
 * Reconstructed from dsplibs.o toneiir.c:
 *
 *   _iir_filter_create     .text 0x07c960
 *   _iir_filter_delete     .text 0x07ca30
 *   _iir_filter_progress   .text 0x07ca50
 *
 * The rest of the TU -- toneiir_create, toneiir_reset, toneiir_progress,
 * toneiir_delete, toneiir_get_default_configuration at .text 0x07c330 -- is
 * not reconstructed yet and belongs in this file when it is.
 *
 * WHAT THE ENGINE COMPUTES
 *
 * Four cascaded biquads, direct form I:
 *
 *   v      = x[n] >> shift[0]
 *   y_s[n] = (b0 v + b1 v[n-1] + b2 v[n-2]
 *             - a1 y_s[n-1] - a2 y_s[n-2]) >> 13
 *   v      = y_s[n] >> shift[s+1]        and on into section s+1
 *
 * Everything the callers hand it is an elliptic design -- every section has
 * its zeros on the unit circle, which is what puts the deep nulls in the
 * stopband and is why the tables are named after Cauer filters.
 *
 * SCALING
 *
 * Q13, not the Q14 used everywhere else in this library.  The extra bit of
 * headroom is spent on the coefficients: the supervisor's own band filter has
 * b0 = 2.0 in its last section, which does not fit in Q14 at all.  That is
 * the whole reason for the difference.
 *
 * These designs are written with b0 = 1.0 per section rather than normalised,
 * which leaves about 30 dB of passband gain, so the shifts take it back out.
 * Every deployed set spends its headroom at or near the input rather than at
 * the output, which is what keeps the high-Q sections from clipping long
 * before the output would.
 *
 * There is no saturation anywhere.  Each section truncates its accumulator to
 * 16 bits and wraps.  With the deployed shifts that needs an input well past
 * full scale, but it is a property of the filter, not a guarantee.
 */

#include "dsplib/toneiir.h"
#include "dsplib/sysdep.h"

struct iir_filter *
_iir_filter_create(struct iir_filter *f, int n_a, int n_b, const short *a,
		   const short *b, const short *shift)
{
	int i;

	/*
	 * Pass NULL to allocate.  Note what is missing: the result is not
	 * checked, and nothing records that this object owns its memory --
	 * `_iir_filter_delete` frees whatever it is given.  A caller that
	 * passes a stack buffer here and later deletes it will free a stack
	 * address.  Its callers pass NULL, so the original never does.
	 */
	if (f == 0)
		f = (struct iir_filter *)sysdep_malloc(sizeof(*f));

	/* All 25 words of each, not just the 12 and 8 the four sections use. */
	for (i = 0; i < IIR_FILTER_MAX_COEFF; i++) {
		f->x[i] = 0;
		f->y[i] = 0;
	}

	for (i = 0; i < n_a; i++)
		f->a[i] = a[i];
	for (i = 0; i < n_b; i++)
		f->b[i] = b[i];

	f->n_b = n_b;
	f->n_a_minus_1 = n_a - 1;

	for (i = 0; i < IIR_FILTER_SECTIONS + 1; i++)
		f->shift[i] = shift[i];

	return f;
}

void
_iir_filter_delete(struct iir_filter *f)
{
	if (f != 0)
		sysdep_free(f);
}

void
_iir_filter_progress(struct iir_filter *f, int count, short *samples)
{
	int i;

	for (i = 0; i < count; i++) {
		int v = samples[i] >> f->shift[0];
		int s;

		for (s = 0; s < IIR_FILTER_SECTIONS; s++) {
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
			v = (short)(acc >> IIR_FILTER_SHIFT);

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
			if (s < IIR_FILTER_SECTIONS - 1 || f->shift[s + 1] > 0)
				v >>= f->shift[s + 1];
		}

		samples[i] = (short)v;
	}
}
