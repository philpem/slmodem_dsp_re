/*
 * toneiir.h -- Tone IIR: the four-section filter engine.
 *
 * Two filters live here, sharing a shape and nothing else.
 *
 *   _iir_filter_*   a bare four-section biquad cascade.  Copies its
 *                   coefficients, filters a block in place, returns nothing.
 *                   CALLPROG_Progress runs every received block through one.
 *
 *   toneiir_*       the same cascade with a tone detector bolted on top.
 *                   Points at its coefficients rather than copying them,
 *                   takes one sample at a time, and returns a verdict every
 *                   `interval` samples.  cadence_create builds one.
 *
 * Both are fixed at four sections, direct form I, Q13, with a rescaling shift
 * between sections.  Their callers hand them one of the designs in cpfiltrs.h
 * or the supervisor's own band filter in callprog_cfg.h.
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

/*
 * ---------------------------------------------------------------------------
 * toneiir -- the same cascade, plus "is a tone present in that band?"
 *
 * The detector is an energy comparison made once every `interval` samples.
 * Two rectified envelopes are tracked, one on the incoming signal and one on
 * the filter's output, and at the end of each interval the band is judged to
 * hold a tone when all four of these hold:
 *
 *   band >= input / 4        the band carries at least a quarter of the total
 *   band >= previous * 0.7   and the previous interval's band envelope
 *   previous >= band * 0.7   agrees with it to within 30%
 *   band >= threshold        and there is enough of it to bother with
 *
 * The stability pair is what separates a tone from speech: a voice moves more
 * than 30% in 62 milliseconds and a dial tone does not.
 */

/* Verdicts from toneiir_progress. */
#define TONEIIR_UNDECIDED	0	/* mid-interval; nothing to report  */
#define TONEIIR_ABSENT		1	/* interval ended, no sustained tone */
#define TONEIIR_PRESENT		2	/* interval ended, tone sustained    */

/*
 * The configuration, copied whole into the head of the object.  cadence_create
 * builds one on the stack from `toneiir_get_default_configuration` and then
 * overwrites everything except `stability`, `status` and `pad`.
 */
struct toneiir_cfg {
	/*
	 * Pointed at, not copied.  Section s uses a[3*s..3*s+2] and
	 * b[3*s..3*s+2]; whatever these point at must outlive the filter.
	 */
	const short	*a;		/* +0x00  denominator, Q13         */
	const short	*b;		/* +0x04  numerator, Q13           */

	/*
	 * How much history to clear at create.  Clamped to 25 there, and read
	 * for nothing else -- toneiir_progress is hand-unrolled to four
	 * sections and never looks at them.
	 */
	int		n_a;		/* +0x08 */
	int		n_b;		/* +0x0c */

	/* Samples between verdicts.  500, which is 62.5 ms at 8000 Hz. */
	short		interval;	/* +0x10 */
	short		pad12;		/* +0x12 */

	/*
	 * How closely consecutive intervals must agree for the band envelope
	 * to count as steady.  Q14; 11467 is 0.7.
	 */
	short		stability;	/* +0x14 */

	/*
	 * Written back as 2 by every verdict, over the 2 already there.  It
	 * costs a store per interval and changes nothing.
	 */
	short		status;		/* +0x16 */

	/*
	 * Envelope floor.  80 in the template; cadence_create replaces it with
	 * the country's value from Get_Detection_Threshold_Table.
	 */
	int		threshold;	/* +0x18 */

	/*
	 * How long the tone must persist, in milliseconds.  Converted to a
	 * count of intervals at create -- 2200 ms becomes 35 intervals.
	 */
	int		duration_ms;	/* +0x1c */

	/* Consecutive absent intervals tolerated inside one tone. */
	int		gap_tolerance;	/* +0x20 */

	/* When zero, a gap shorter than the tolerance resets both counters. */
	int		keep_on_gap;	/* +0x24 */

	/* Interstage shifts, as for struct iir_filter. */
	const short	*scales;	/* +0x28 */
};

struct toneiir {
	struct toneiir_cfg	cfg;		/* +0x00 */
	int			n;		/* +0x2c  samples into the interval */
	short			x[IIR_FILTER_MAX_COEFF];	/* +0x30 */
	short			y[IIR_FILTER_MAX_COEFF];	/* +0x62 */

	/*
	 * Rectified envelopes, one-pole, 0.99 old plus 0.01 new.  Reset at
	 * every verdict, with the band envelope carried into `env_prev` so
	 * the next interval has something to compare against.
	 */
	short			env_in;		/* +0x94  before the filter */
	short			env_band;	/* +0x96  after it          */
	short			env_prev;	/* +0x98  last interval's   */

	int			need;		/* +0x9c  intervals required */
	int			total;		/* +0xa0  intervals counted  */
	int			run;		/* +0xa4  consecutive absent */
};

/*
 * The one configuration the object exports by name.  cadence_create is its
 * only reference, on a branch that cannot be taken -- see
 * src/callprog/toneiir.c.
 */
extern const struct toneiir_cfg toneiir_configuration_allpass;

/*
 * Copy the built-in configuration into `dst`, which must have room for one.
 * Every coefficient pointer in it is to an array of zeros, so the template is
 * useful only for the numeric fields; a caller is expected to fill in the
 * filter.  cadence_create does exactly that.
 */
void toneiir_get_default_configuration(struct toneiir_cfg *dst);

/* Pass NULL for `st` to allocate, or NULL for `cfg` to take the default. */
struct toneiir *toneiir_create(struct toneiir *st,
			       const struct toneiir_cfg *cfg);

void toneiir_delete(struct toneiir *st);

/* Start a fresh interval, keeping the coefficients and the derived count. */
void toneiir_reset(struct toneiir *st);

/* Feed one sample; returns one of the TONEIIR_* verdicts. */
int toneiir_progress(struct toneiir *st, short sample);

#endif /* DSPLIB_TONEIIR_H */
