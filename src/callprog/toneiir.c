/*
 * toneiir.c -- Tone IIR: the four-section filter engine.
 *
 * Reconstructed from dsplibs.o toneiir.c:
 *
 *   _iir_filter_create     .text 0x07c960
 *   _iir_filter_delete     .text 0x07ca30
 *   _iir_filter_progress   .text 0x07ca50
 *
 *   toneiir_create         .text 0x07c330
 *   toneiir_reset          .text 0x07c460
 *   toneiir_progress       .text 0x07c4a0
 *   toneiir_delete         .text 0x07c920
 *   toneiir_get_default_configuration
 *                          .text 0x07c930
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
#include "dsplib/fp_math.h"

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

/*
 * ---------------------------------------------------------------------------
 * toneiir -- the cascade with a tone detector on top.
 *
 * See toneiir.h for what it decides.  Three details here are easy to lose:
 *
 *   - the envelope update takes the absolute value AFTER shifting, not
 *     before.  For a negative sample those differ, because an arithmetic
 *     shift rounds toward minus infinity: -1 * 164 >> 14 is -1, whose
 *     magnitude is 1, while 1 * 164 >> 14 is 0.
 *
 *   - the verdict compares the band envelope against the threshold as a
 *     16-bit quantity even though the threshold field is an int.
 *
 *   - the counters are updated before the verdict is read, so an interval
 *     that ends a tone still counts toward it.
 */

/*
 * The built-in configuration, .rodata+0x61a0.  Every coefficient array it
 * points at is zeros -- 5 scales, 12 denominator and 12 numerator words, all
 * of them -- so a filter built from it unmodified outputs silence.  It is a
 * template for the numeric fields, and cadence_create, its only user,
 * overwrites both coefficient pointers, both counts, the interval, the
 * threshold, the duration, `keep_on_gap` and the scales before using it.
 */
static const short toneiir_default_scales[IIR_FILTER_SCALES];
static const short toneiir_default_a[IIR_FILTER_COEFF];
static const short toneiir_default_b[IIR_FILTER_COEFF];

static const struct toneiir_cfg toneiir_default_configuration = {
	.a		= toneiir_default_a,
	.b		= toneiir_default_b,
	.n_a		= 12,
	.n_b		= 12,
	.interval	= 500,
	.pad12		= 0,
	.stability	= 11467,		/* 0.7 in Q14 */
	.status		= 2,
	.threshold	= 80,
	.duration_ms	= 2200,
	.gap_tolerance	= 4,
	.keep_on_gap	= 0,
	.scales		= toneiir_default_scales
};

/*
 * .rodata+0x626c, one word.  In Q13 a lone numerator tap of 1 is a gain of
 * 1/8192, so whatever this was meant to be it is not the unity its name
 * implies -- see below for why that never matters.
 */
static const short toneiir_allpass_b[1] = { 1 };

/*
 * `toneiir_configuration_allpass`.  Its scales and denominator pointers are
 * NULL, which toneiir_progress would dereference on its first sample -- but
 * the only branch that passes it to toneiir_create is unreachable:
 * cadence_create clears the flag guarding it at entry and never sets it.  Kept
 * because it is a global the object defines.
 */
const struct toneiir_cfg toneiir_configuration_allpass = {
	.a		= 0,
	.b		= toneiir_allpass_b,
	.n_a		= 0,
	.n_b		= 1,
	.interval	= 500,
	.pad12		= 0,
	.stability	= 11467,
	.status		= 2,
	.threshold	= 80,
	.duration_ms	= 2200,
	.gap_tolerance	= 4,
	.keep_on_gap	= 0,
	.scales		= 0
};

void
toneiir_get_default_configuration(struct toneiir_cfg *dst)
{
	*dst = toneiir_default_configuration;
}

/*
 * Begin a fresh interval.  The band envelope becomes the one the next
 * interval will be compared against, which is the whole reason `env_prev`
 * exists.
 */
void
toneiir_reset(struct toneiir *st)
{
	short prev = st->env_band;

	st->env_band = 0;
	st->n = 0;
	st->env_prev = prev;
	st->env_in = 0;
	st->total = 0;
	st->run = 0;
}

struct toneiir *
toneiir_create(struct toneiir *st, const struct toneiir_cfg *cfg)
{
	short scale;
	int i;

	if (st == 0)
		st = (struct toneiir *)sysdep_malloc(sizeof(*st));
	if (cfg == 0)
		cfg = &toneiir_default_configuration;

	st->cfg = *cfg;

	/* Clamp to what the history arrays can hold. */
	if (st->cfg.n_b > IIR_FILTER_MAX_COEFF - 1)
		st->cfg.n_b = IIR_FILTER_MAX_COEFF;
	if (st->cfg.n_a > IIR_FILTER_MAX_COEFF - 1)
		st->cfg.n_a = IIR_FILTER_MAX_COEFF;

	/*
	 * The same sequence toneiir_reset performs, inlined -- which means the
	 * band envelope is read before it has been written.  On the allocating
	 * path that is uninitialised heap, and it lands in env_prev, where the
	 * first interval's stability test will compare against it.  See D14.
	 */
	{
		short prev = st->env_band;

		st->n = 0;
		st->env_band = 0;
		st->env_prev = prev;
		st->env_in = 0;
	}

	for (i = 0; i < st->cfg.n_b; i++)
		st->x[i] = 0;
	for (i = 0; i < st->cfg.n_a; i++)
		st->y[i] = 0;

	/*
	 * Milliseconds to intervals.  GetFP_Value(a, b) is ceil(a << 14 / b),
	 * so this is duration_ms * 8 / interval -- and the 8 is samples per
	 * millisecond, which is 8000 Hz stated a third way.  2200 ms at 500
	 * samples an interval gives 35.
	 */
	scale = GetFP_Value(8, st->cfg.interval);
	st->need = ((int)scale * st->cfg.duration_ms) >> 14;

	st->total = 0;
	st->run = 0;

	return st;
}

void
toneiir_delete(struct toneiir *st)
{
	sysdep_free(st);
}

/* env = 0.99 * env + 0.01 * |x|, with both terms rounded before the abs. */
#define TONEIIR_ENV_NEW		164		/* 0.01 in Q14 */
#define TONEIIR_ENV_OLD		16220		/* 0.99 in Q14 */

static int
iabs(int v)
{
	return v < 0 ? -v : v;
}

static short
envelope(short env, int x)
{
	return (short)(iabs((TONEIIR_ENV_NEW * x) >> 14)
		       + iabs((TONEIIR_ENV_OLD * (int)env) >> 14));
}

int
toneiir_progress(struct toneiir *st, short sample)
{
	int v = sample >> st->cfg.scales[0];
	int present;
	int result;
	int s;

	for (s = 0; s < IIR_FILTER_SECTIONS; s++) {
		short *x = &st->x[3 * s];
		short *y = &st->y[2 * s];
		const short *a = &st->cfg.a[3 * s];
		const short *b = &st->cfg.b[3 * s];
		int acc = 0;
		int k;

		x[0] = (short)v;

		for (k = 0; k <= 2; k++)
			acc = (int)((unsigned)acc + (unsigned)(x[k] * b[k]));
		for (k = 1; k <= 2; k++)
			acc = (int)((unsigned)acc - (unsigned)(y[k - 1] * a[k]));

		v = (short)(acc >> IIR_FILTER_SHIFT);

		x[2] = x[1];
		x[1] = x[0];
		y[1] = y[0];
		y[0] = (short)v;

		if (s < IIR_FILTER_SECTIONS - 1 || st->cfg.scales[s + 1] > 0)
			v >>= st->cfg.scales[s + 1];
	}

	st->env_band = envelope(st->env_band, (short)v);
	st->env_in = envelope(st->env_in, sample);

	if (++st->n != st->cfg.interval)
		return TONEIIR_UNDECIDED;

	st->cfg.status = 2;

	/*
	 * Four conditions, all evaluated -- the original computes them
	 * branchlessly with setge and ands them together, so none of them
	 * short-circuits.  Reproduced with & rather than && for the same
	 * reason: nothing here has a side effect, but the intent is the
	 * original's.
	 */
	present = (st->env_band >= st->env_in / 4)
		& (st->env_band >= (int)((st->env_prev * st->cfg.stability)
					 >> 14))
		& (st->env_prev >= (int)((st->env_band * st->cfg.stability)
					 >> 14));

	/* The threshold is an int but the comparison is 16-bit. */
	if (st->env_band < (short)st->cfg.threshold)
		present = 0;

	if (present) {
		st->run++;
		st->total++;
	} else if (st->run > st->cfg.gap_tolerance) {
		st->total++;
		st->run = 0;
	} else if (st->cfg.keep_on_gap == 0) {
		st->run = 0;
		st->total = 0;
	}

	result = (st->total > st->need && present)
		 ? TONEIIR_PRESENT : TONEIIR_ABSENT;

	{
		short prev = st->env_band;

		st->n = 0;
		st->env_band = 0;
		st->env_in = 0;
		st->env_prev = prev;
	}

	return result;
}
