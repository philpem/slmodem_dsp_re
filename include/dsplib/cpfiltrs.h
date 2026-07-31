/*
 * cpfiltrs.h -- Call Progress Filters: the band-limiting elliptic cascade.
 *
 * One filter, used once: CALLPROG_Create builds it and CALLPROG_Progress runs
 * every received block through it before anything tries to detect a tone.  It
 * is a fourth-order-section (eighth-order overall) elliptic bandpass that
 * keeps roughly 200 Hz to 2.5 kHz and puts a deep notch at 2712 Hz.
 *
 * Relationship to the other IIR engine in this library:
 *
 *   FPM_iir_filt / FPM_iir_filt_II  (fpm_iir.h)  general, Q14, variable
 *                                   section count, shared by six datapumps
 *   _iir_filter_progress            (here)       fixed at four sections, Q13,
 *                                   per-section rescaling shifts, one caller
 *
 * They are not interchangeable and neither is a special case of the other:
 * different scaling, different coefficient order, different history layout.
 *
 * The odd leading underscore is the original's, kept so the differential
 * tests can link both implementations side by side.
 */

#ifndef DSPLIB_CPFILTRS_H
#define DSPLIB_CPFILTRS_H

/*
 * Sections are fixed at four.  `_iir_filter_create` will copy as many as 25
 * coefficients into each array -- room for eight sections -- but
 * `_iir_filter_progress` is hand-unrolled and always runs exactly four, so
 * anything past section three is stored and never read.
 */
#define CP_IIR_SECTIONS		4
#define CP_IIR_MAX_COEFF	25

/* Q13, not the Q14 the rest of the library uses.  See src/callprog/cpfiltrs.c. */
#define CP_IIR_SHIFT		13
#define CP_IIR_ONE		(1 << CP_IIR_SHIFT)

struct cp_iir {
	/*
	 * Direct form I, so each section keeps three past inputs and two past
	 * outputs.  Both arrays are sized for the coefficient maximum rather
	 * than for the four sections actually run, and `_iir_filter_create`
	 * clears all 25 words of each.
	 *
	 * Section s uses x[3*s .. 3*s+2] and y[2*s .. 2*s+1].
	 */
	short	x[CP_IIR_MAX_COEFF];	/* +0x00  past inputs             */
	short	y[CP_IIR_MAX_COEFF];	/* +0x32  past outputs            */

	/*
	 * Section s uses a[3*s .. 3*s+2] and b[3*s .. 3*s+2].  a[3*s] is the
	 * normalised leading denominator coefficient: stored, never read.
	 */
	short	a[CP_IIR_MAX_COEFF];	/* +0x64  denominator, Q13        */
	short	b[CP_IIR_MAX_COEFF];	/* +0x96  numerator, Q13          */

	/*
	 * Right shifts applied between sections: shift[0] scales the input
	 * before section 0, shift[s+1] scales section s's output on its way
	 * into section s+1, and shift[4] scales the final output.
	 *
	 * They exist because the cascade has about +30 dB of passband gain,
	 * which would overflow 16 bits at any realistic input level.  The
	 * deployed set is { 5, 0, 0, 0, 0 } -- all the headroom taken up
	 * front, leaving the filter roughly unity gain end to end.
	 */
	short	shift[CP_IIR_SECTIONS + 1];	/* +0xc8 */

	/*
	 * Coefficient counts, recorded by create and read by nothing.  Kept
	 * because they are part of the object's 220-byte footprint and a
	 * differential test compares the whole thing.
	 */
	int	n_b;			/* +0xd4  as passed              */
	int	n_a_minus_1;		/* +0xd8  as passed, less one    */
};

/*
 * Build a filter.  Pass NULL for `f` to allocate one -- the same ownership
 * idiom the datapumps use, except that here nothing records who allocated it
 * and `_iir_filter_delete` frees unconditionally.
 *
 * `a` and `b` are read as n_a and n_b coefficients respectively; `shift` is
 * always read as CP_IIR_SECTIONS + 1 words.  Nothing is range checked: the
 * original will happily overrun both coefficient arrays past 25.
 */
struct cp_iir *_iir_filter_create(struct cp_iir *f, int n_a, int n_b,
				  const short *a, const short *b,
				  const short *shift);

void _iir_filter_delete(struct cp_iir *f);

/* Filter `count` samples in place. */
void _iir_filter_progress(struct cp_iir *f, int count, short *samples);

#endif /* DSPLIB_CPFILTRS_H */
