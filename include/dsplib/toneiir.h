/*
 * toneiir.h -- Tone IIR: the four-section filter engine.
 *
 * The engine every call-progress filter runs on.  Fixed at four biquad
 * sections, direct form I, Q13, with a rescaling shift between sections.  Its
 * callers hand it one of the designs in cpfiltrs.h or the supervisor's own
 * band filter in callprog_cfg.h.
 *
 * Relationship to the other IIR engine in this library:
 *
 *   FPM_iir_filt / FPM_iir_filt_II  (fpm_iir.h)  general, Q14, variable
 *                                   section count, shared by six datapumps
 *   _iir_filter_progress            (here)       fixed at four sections, Q13,
 *                                   per-section rescaling shifts
 *
 * They are not interchangeable and neither is a special case of the other:
 * different scaling, different coefficient order, different history layout.
 *
 * The odd leading underscore is the original's, kept so the differential
 * tests can link both implementations side by side.
 *
 * WHICH TU THIS IS
 *
 * `_iir_filter_*` occupies .text 0x07c960..0x07cd7f, between toneiir.c's
 * other functions and Cadence.c's.  The translation-unit order recovered from
 * the object's STT_FILE entries is toneiir.c, Cadence.c, CPfiltrs.c,
 * Elliptic1.c, Elliptic2.c, Elliptic3.c, and the .rodata each of those files
 * contributes appears in exactly that order, so the sequence is trustworthy.
 * Since `cadence_*` lies at a HIGHER address than `_iir_filter_*`, this code
 * cannot be in CPfiltrs.c: CPfiltrs.c and the three Elliptic files contribute
 * no .text at all, being the pure coefficient tables in cpfiltrs.h.  See
 * docs/modules.md.
 */

#ifndef DSPLIB_TONEIIR_H
#define DSPLIB_TONEIIR_H

/*
 * Sections are fixed at four.  `_iir_filter_create` will copy as many as 25
 * coefficients into each array -- room for eight sections -- but
 * `_iir_filter_progress` is hand-unrolled and always runs exactly four, so
 * anything past section three is stored and never read.
 */
#define IIR_FILTER_SECTIONS	4
#define IIR_FILTER_MAX_COEFF	25

/* Q13, not the Q14 the rest of the library uses.  See src/callprog/toneiir.c. */
#define IIR_FILTER_SHIFT	13
#define IIR_FILTER_ONE		(1 << IIR_FILTER_SHIFT)

/* Coefficients per filter, and the shift array's length. */
#define IIR_FILTER_COEFF	(3 * IIR_FILTER_SECTIONS)
#define IIR_FILTER_SCALES	(IIR_FILTER_SECTIONS + 1)

struct iir_filter {
	/*
	 * Direct form I, so each section keeps three past inputs and two past
	 * outputs.  Both arrays are sized for the coefficient maximum rather
	 * than for the four sections actually run, and `_iir_filter_create`
	 * clears all 25 words of each.
	 *
	 * Section s uses x[3*s .. 3*s+2] and y[2*s .. 2*s+1].
	 */
	short	x[IIR_FILTER_MAX_COEFF];	/* +0x00  past inputs      */
	short	y[IIR_FILTER_MAX_COEFF];	/* +0x32  past outputs     */

	/*
	 * Section s uses a[3*s .. 3*s+2] and b[3*s .. 3*s+2].  a[3*s] is the
	 * normalised leading denominator coefficient: stored, never read.
	 */
	short	a[IIR_FILTER_MAX_COEFF];	/* +0x64  denominator, Q13 */
	short	b[IIR_FILTER_MAX_COEFF];	/* +0x96  numerator, Q13   */

	/*
	 * Right shifts applied between sections: shift[0] scales the input
	 * before section 0, shift[s+1] scales section s's output on its way
	 * into section s+1, and shift[4] scales the final output.
	 *
	 * They exist because these designs are written with b0 = 1.0 per
	 * section rather than normalised, which leaves 30 dB or so of
	 * passband gain that would overflow 16 bits at any realistic input
	 * level.  Every deployed set spends its headroom at or near the
	 * input, which is what keeps the high-Q sections from clipping long
	 * before the output would.
	 */
	short	shift[IIR_FILTER_SCALES];	/* +0xc8 */

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
 * always read as IIR_FILTER_SCALES words.  Nothing is range checked: the
 * original will happily overrun both coefficient arrays past 25.
 */
struct iir_filter *_iir_filter_create(struct iir_filter *f, int n_a, int n_b,
				      const short *a, const short *b,
				      const short *shift);

void _iir_filter_delete(struct iir_filter *f);

/* Filter `count` samples in place. */
void _iir_filter_progress(struct iir_filter *f, int count, short *samples);

#endif /* DSPLIB_TONEIIR_H */
