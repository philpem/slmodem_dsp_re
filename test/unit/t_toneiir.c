/*
 * t_toneiir.c -- differential test of the call-progress band filter.
 *
 * Three things are being proved, and they are not the same thing:
 *
 *   1. `_iir_filter_create` builds a byte-identical object.  Both buffers are
 *      pre-filled with the same junk first, so the comparison covers the
 *      bytes create does NOT write as well as the ones it does -- a
 *      reconstruction that helpfully cleared the unused coefficient slots, or
 *      forgot the two padding bytes, would fail here rather than pass
 *      silently.  The allocating path cannot do that (its slack really is
 *      uninitialised heap, and comparing it would be comparing nothing), so
 *      there the written fields are compared one at a time.
 *
 *   2. `_iir_filter_progress` produces the same samples and the same state.
 *      Driven with the real design and with synthetic coefficient sets
 *      chosen to make the accumulator wrap, since ordinary signals through
 *      the real design never do and wrapping is the one place the two
 *      implementations could plausibly disagree.
 *
 *   3. The coefficient table is transcribed correctly.  This one cannot be
 *      done differentially: the table is a file static with no symbol, so
 *      there is nothing to compare against.  Instead the filter is measured
 *      -- a 2712 Hz tone must come out at least 20 dB below an 800 Hz tone --
 *      which is a statement about the design rather than about the bytes, and
 *      is what catches a transposed digit.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "harness.h"
#include "dsplib/toneiir.h"
#include "dsplib/callprog_cfg.h"
#include "dsplib/cpfiltrs.h"

extern struct iir_filter *ref__iir_filter_create(struct iir_filter *f, int n_a,
					     int n_b, const short *a,
					     const short *b,
					     const short *shift);
extern void ref__iir_filter_delete(struct iir_filter *f);
extern void ref__iir_filter_progress(struct iir_filter *f, int count,
				     short *samples);

/*
 * Unlike the supervisor's own band filter, the four CPfiltrs.c designs are
 * global symbols, so they can be compared directly rather than only through
 * their effect.
 */
extern const short ref_CP_100_550_scales[], ref_CP_100_550_a[],
		   ref_CP_100_550_b[];
extern const short ref_CP_276_504_scales[], ref_CP_276_504_a[],
		   ref_CP_276_504_b[];
extern const short ref_CP_350_600_scales[], ref_CP_350_600_a[],
		   ref_CP_350_600_b[];
extern const short ref_CP_450_630_scales[], ref_CP_450_630_a[],
		   ref_CP_450_630_b[];

extern void ref_toneiir_get_default_configuration(struct toneiir_cfg *dst);
extern struct toneiir *ref_toneiir_create(struct toneiir *st,
					  const struct toneiir_cfg *cfg);
extern void ref_toneiir_delete(struct toneiir *st);
extern void ref_toneiir_reset(struct toneiir *st);
extern int ref_toneiir_progress(struct toneiir *st, short sample);
extern const struct toneiir_cfg ref_toneiir_configuration_allpass;

#define NSAMP 6000

/* Counts, across the whole run, how often a section output left 16 bits. */
static int wrap_seen;

static unsigned lfsr = 0x51EDu;

static short
noise(int amplitude)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
	return (short)(((int)(lfsr & 0xffffu) - 0x8000) / (0x8000 / amplitude));
}

/*
 * An independent model of one sample, in 64-bit arithmetic, used only to
 * decide whether the 32-bit path wrapped.  It carries its own state -- a
 * shadow filter advanced one sample at a time -- because reading the real
 * filter's state mid-block would read it from before the block ran.
 *
 * Deliberately not sharing code with the implementation: a shared helper
 * would agree with itself.
 */
static void
shadow_step(struct iir_filter *f, short x0)
{
	int v = x0 >> f->shift[0];
	int s;

	for (s = 0; s < IIR_FILTER_SECTIONS; s++) {
		const short *a = &f->a[3 * s];
		const short *b = &f->b[3 * s];
		short *x = &f->x[3 * s];
		short *y = &f->y[2 * s];
		long long acc = 0;
		int k, w;

		x[0] = (short)v;
		for (k = 0; k <= 2; k++)
			acc += (long long)x[k] * b[k];
		for (k = 1; k <= 2; k++)
			acc -= (long long)y[k - 1] * a[k];

		w = (int)(acc >> IIR_FILTER_SHIFT);
		if (w < -32768 || w > 32767)
			wrap_seen++;

		v = (short)w;
		x[2] = x[1];
		x[1] = x[0];
		y[1] = y[0];
		y[0] = (short)v;
		if (s < IIR_FILTER_SECTIONS - 1 || f->shift[s + 1] > 0)
			v >>= f->shift[s + 1];
	}
}

/*
 * --------------------------------------------------------------------------
 * 1. create
 */
static void
compare_written(const struct iir_filter *fb, const struct iir_filter *fa, int n_a,
		int n_b)
{
	int i;

	for (i = 0; i < IIR_FILTER_MAX_COEFF; i++) {
		diff_eq_int("x[%ld]", fb->x[i], fa->x[i], i);
		diff_eq_int("y[%ld]", fb->y[i], fa->y[i], i);
	}
	for (i = 0; i < n_a; i++)
		diff_eq_int("a[%ld]", fb->a[i], fa->a[i], i);
	for (i = 0; i < n_b; i++)
		diff_eq_int("b[%ld]", fb->b[i], fa->b[i], i);
	for (i = 0; i < IIR_FILTER_SCALES; i++)
		diff_eq_int("shift[%ld]", fb->shift[i], fa->shift[i], i);
	diff_eq_int("n_b", fb->n_b, fa->n_b, 0);
	diff_eq_int("n_a_minus_1", fb->n_a_minus_1, fa->n_a_minus_1, 0);
}

static int
run_create(const char *label, int n_a, int n_b, const short *a, const short *b,
	   const short *shift)
{
	unsigned char bufa[sizeof(struct iir_filter)];
	unsigned char bufb[sizeof(struct iir_filter)];
	struct iir_filter *fa, *fb;

	diff_begin(label);

	/*
	 * Identical junk in both, so the slack create never touches compares
	 * equal by construction and any *extra* write shows up as a mismatch.
	 */
	memset(bufa, 0xA5, sizeof(bufa));
	memset(bufb, 0xA5, sizeof(bufb));

	fa = ref__iir_filter_create((struct iir_filter *)bufa, n_a, n_b, a, b,
				    shift);
	fb = _iir_filter_create((struct iir_filter *)bufb, n_a, n_b, a, b, shift);

	diff_eq_int("returns its argument", fb == (struct iir_filter *)bufb, 1, 0);
	diff_eq_int("ref returns its argument", fa == (struct iir_filter *)bufa,
		    1, 0);

	diff_eq_obj("the whole filter", struct iir_filter, fb, fa, 0);

	return diff_end();
}

static int
run_create_alloc(void)
{
	struct iir_filter *fa, *fb;

	diff_begin("_iir_filter_create: allocated");

	harness_alloc_reset();
	fa = ref__iir_filter_create(0, 12, 12, CALLPROG_BandFilter_a,
				    CALLPROG_BandFilter_b,
				    CALLPROG_BandFilter_shift);
	fb = _iir_filter_create(0, 12, 12, CALLPROG_BandFilter_a,
				CALLPROG_BandFilter_b,
				CALLPROG_BandFilter_shift);

	/*
	 * Two objects, two allocations, and both must be 220 bytes: the
	 * original hard-codes 0xdc and the reconstruction uses sizeof, so on
	 * the 32-bit differential target they have to agree.  (They would not
	 * on a 64-bit host, which is precisely why the reconstruction does not
	 * use the literal.)
	 */
	diff_eq_int("allocs", harness_alloc.allocs, 2, 0);
	diff_eq_int("bytes", (int)harness_alloc.bytes, 2 * 0xdc, 0);

	/*
	 * Fields only.  The slack -- a[12..24], b[12..24] and the two padding
	 * bytes -- is heap create never writes, and comparing two different
	 * malloc results there would be comparing the allocator, not us.
	 */
	compare_written(fb, fa, 12, 12);

	ref__iir_filter_delete(fa);
	_iir_filter_delete(fb);
	diff_eq_int("frees", harness_alloc.frees, 2, 0);
	diff_eq_int("live", harness_alloc.live, 0, 0);
	diff_eq_int("bad frees", harness_alloc.bad_free, 0, 0);

	/* delete(NULL) is a no-op in both. */
	ref__iir_filter_delete(0);
	_iir_filter_delete(0);
	diff_eq_int("delete(NULL) frees nothing", harness_alloc.frees, 2, 0);
	diff_eq_int("delete(NULL) is not a bad free", harness_alloc.bad_free,
		    0, 0);

	return diff_end();
}

/*
 * --------------------------------------------------------------------------
 * 2. progress
 */
static int
run_progress(const char *label, int n_a, int n_b, const short *a,
	     const short *b, const short *shift, int amplitude, int frag)
{
	struct iir_filter fa, fb, shadow;
	short ba[512], bb[512];
	int n, i;

	diff_begin(label);

	ref__iir_filter_create(&fa, n_a, n_b, a, b, shift);
	_iir_filter_create(&fb, n_a, n_b, a, b, shift);
	_iir_filter_create(&shadow, n_a, n_b, a, b, shift);

	for (n = 0; n < NSAMP / frag; n++) {
		for (i = 0; i < frag; i++)
			ba[i] = bb[i] = noise(amplitude);

		for (i = 0; i < frag; i++)
			shadow_step(&shadow, ba[i]);

		ref__iir_filter_progress(&fa, frag, ba);
		_iir_filter_progress(&fb, frag, bb);

		for (i = 0; i < frag; i++)
			diff_eq_int("block %ld: sample", bb[i], ba[i],
				    (long)n * frag + i);
		for (i = 0; i < IIR_FILTER_MAX_COEFF; i++) {
			diff_eq_int("block %ld: x state", fb.x[i], fa.x[i],
				    (long)n);
			diff_eq_int("block %ld: y state", fb.y[i], fa.y[i],
				    (long)n);
		}
	}

	return diff_end();
}

/*
 * Fragmentation: state must carry across calls, so the same stream split
 * three different ways has to give the same samples.  A filter that reset its
 * history per call would still pass the differential test above if the
 * original did too, but not this.
 */
static int
run_fragmentation(void)
{
	struct iir_filter f;
	short whole[1024], part[1024];
	static const int fragments[] = { 1, 3, 160 };
	unsigned k;
	int i;

	diff_begin("_iir_filter_progress: fragmentation");

	lfsr = 0x1234u;
	for (i = 0; i < 1024; i++)
		whole[i] = noise(8000);
	memcpy(part, whole, sizeof(part));

	_iir_filter_create(&f, 12, 12, CALLPROG_BandFilter_a,
			   CALLPROG_BandFilter_b, CALLPROG_BandFilter_shift);
	_iir_filter_progress(&f, 1024, whole);

	for (k = 0; k < sizeof(fragments) / sizeof(fragments[0]); k++) {
		short scratch[1024];
		int frag = fragments[k];

		memcpy(scratch, part, sizeof(scratch));
		_iir_filter_create(&f, 12, 12, CALLPROG_BandFilter_a,
				   CALLPROG_BandFilter_b,
				   CALLPROG_BandFilter_shift);
		for (i = 0; i < 1024; i += frag) {
			int n = 1024 - i < frag ? 1024 - i : frag;

			_iir_filter_progress(&f, n, scratch + i);
		}

		for (i = 0; i < 1024; i++)
			diff_eq_int("fragment size %ld", scratch[i], whole[i],
				    (long)frag);
	}

	return diff_end();
}

/*
 * --------------------------------------------------------------------------
 * 2b. the CPfiltrs.c tables, compared word for word
 */
struct cp_design {
	const char	*name;
	const short	*scales, *a, *b;
	const short	*ref_scales, *ref_a, *ref_b;
	double		lo, hi;		/* -6 dB band, Hz, at 8000 */
};

static const struct cp_design cp_designs[] = {
	{ "CP_100_550", CP_100_550_scales, CP_100_550_a, CP_100_550_b,
	  ref_CP_100_550_scales, ref_CP_100_550_a, ref_CP_100_550_b,
	  0.0, 596.0 },
	{ "CP_276_504", CP_276_504_scales, CP_276_504_a, CP_276_504_b,
	  ref_CP_276_504_scales, ref_CP_276_504_a, ref_CP_276_504_b,
	  230.0, 546.0 },
	{ "CP_350_600", CP_350_600_scales, CP_350_600_a, CP_350_600_b,
	  ref_CP_350_600_scales, ref_CP_350_600_a, ref_CP_350_600_b,
	  263.0, 898.0 },
	{ "CP_450_630", CP_450_630_scales, CP_450_630_a, CP_450_630_b,
	  ref_CP_450_630_scales, ref_CP_450_630_a, ref_CP_450_630_b,
	  396.0, 670.0 }
};

#define CP_DESIGNS ((int)(sizeof(cp_designs) / sizeof(cp_designs[0])))

static int
run_cp_tables(void)
{
	int d, i;

	diff_begin("CPfiltrs.c tables");

	for (d = 0; d < CP_DESIGNS; d++) {
		const struct cp_design *c = &cp_designs[d];

		for (i = 0; i < IIR_FILTER_SCALES; i++)
			diff_eq_int("scales[%ld]", c->scales[i],
				    c->ref_scales[i], i);
		for (i = 0; i < IIR_FILTER_COEFF; i++) {
			diff_eq_int("a[%ld]", c->a[i], c->ref_a[i], i);
			diff_eq_int("b[%ld]", c->b[i], c->ref_b[i], i);
		}
	}

	return diff_end();
}

/*
 * --------------------------------------------------------------------------
 * 2c. toneiir -- the cascade with a detector on top
 */

/* Times each verdict was returned across the whole run. */
static int verdict_seen[3];

/*
 * Everything toneiir_create writes, compared field by field.  A whole-object
 * byte compare works too and is used where both objects were pre-filled with
 * the same junk, but not on the allocating path: create leaves the history
 * past n_a/n_b untouched, and there it is uninitialised heap.
 */
static void
compare_toneiir(const struct toneiir *b, const struct toneiir *a, long n)
{
	int i;

	diff_eq_int("%ld: n_a", b->cfg.n_a, a->cfg.n_a, n);
	diff_eq_int("%ld: n_b", b->cfg.n_b, a->cfg.n_b, n);
	diff_eq_int("%ld: interval", b->cfg.interval, a->cfg.interval, n);
	diff_eq_int("%ld: stability", b->cfg.stability, a->cfg.stability, n);
	diff_eq_int("%ld: status", b->cfg.status, a->cfg.status, n);
	diff_eq_int("%ld: threshold", b->cfg.threshold, a->cfg.threshold, n);
	diff_eq_int("%ld: duration_ms", b->cfg.duration_ms, a->cfg.duration_ms,
		    n);
	diff_eq_int("%ld: gap_tolerance", b->cfg.gap_tolerance,
		    a->cfg.gap_tolerance, n);
	diff_eq_int("%ld: keep_on_gap", b->cfg.keep_on_gap, a->cfg.keep_on_gap,
		    n);
	diff_eq_int("%ld: n", b->n, a->n, n);
	diff_eq_int("%ld: env_in", b->env_in, a->env_in, n);
	diff_eq_int("%ld: env_band", b->env_band, a->env_band, n);
	diff_eq_int("%ld: env_prev", b->env_prev, a->env_prev, n);
	diff_eq_int("%ld: need", b->need, a->need, n);
	diff_eq_int("%ld: total", b->total, a->total, n);
	diff_eq_int("%ld: run", b->run, a->run, n);
	for (i = 0; i < IIR_FILTER_MAX_COEFF; i++) {
		diff_eq_int("%ld: x", b->x[i], a->x[i], n);
		diff_eq_int("%ld: y", b->y[i], a->y[i], n);
	}
}

/*
 * Build the configuration cadence_create would: the numeric fields from the
 * template, the filter from one of the CPfiltrs.c designs.  `ref` selects the
 * blob's copy of the tables so the reference filter reads its own .rodata.
 */
static void
make_cfg(struct toneiir_cfg *c, int ref, int threshold, int duration_ms)
{
	if (ref)
		ref_toneiir_get_default_configuration(c);
	else
		toneiir_get_default_configuration(c);

	c->a = ref ? ref_CP_450_630_a : CP_450_630_a;
	c->b = ref ? ref_CP_450_630_b : CP_450_630_b;
	c->scales = ref ? ref_CP_450_630_scales : CP_450_630_scales;
	c->n_a = IIR_FILTER_COEFF;
	c->n_b = IIR_FILTER_COEFF;
	c->threshold = threshold;
	c->duration_ms = duration_ms;
}

static int
run_toneiir_config(void)
{
	struct toneiir_cfg a, b;
	unsigned i;

	diff_begin("toneiir_get_default_configuration");

	memset(&a, 0xA5, sizeof(a));
	memset(&b, 0xA5, sizeof(b));
	ref_toneiir_get_default_configuration(&a);
	toneiir_get_default_configuration(&b);

	/* The pointers must differ -- each points into its own object. */
	diff_eq_int("copies 44 bytes", (int)sizeof(struct toneiir_cfg), 44, 0);
	diff_eq_int("n_a", b.n_a, a.n_a, 0);
	diff_eq_int("n_b", b.n_b, a.n_b, 0);
	diff_eq_int("interval", b.interval, a.interval, 0);
	diff_eq_int("pad12", b.pad12, a.pad12, 0);
	diff_eq_int("stability", b.stability, a.stability, 0);
	diff_eq_int("status", b.status, a.status, 0);
	diff_eq_int("threshold", b.threshold, a.threshold, 0);
	diff_eq_int("duration_ms", b.duration_ms, a.duration_ms, 0);
	diff_eq_int("gap_tolerance", b.gap_tolerance, a.gap_tolerance, 0);
	diff_eq_int("keep_on_gap", b.keep_on_gap, a.keep_on_gap, 0);

	/*
	 * The arrays the template points at are all zeros in both.  Asserted
	 * rather than assumed: this is what makes the default unusable as a
	 * filter, and finding F46 turns on it.
	 */
	for (i = 0; i < IIR_FILTER_COEFF; i++) {
		diff_eq_int("default a[%ld] is zero", b.a[i], 0, i);
		diff_eq_int("default b[%ld] is zero", b.b[i], 0, i);
		diff_eq_int("ref default a[%ld] is zero", a.a[i], 0, i);
		diff_eq_int("ref default b[%ld] is zero", a.b[i], 0, i);
	}
	for (i = 0; i < IIR_FILTER_SCALES; i++) {
		diff_eq_int("default scales[%ld] is zero", b.scales[i], 0, i);
		diff_eq_int("ref default scales[%ld] is zero", a.scales[i], 0,
			    i);
	}

	/* And the exported all-pass configuration, field by field. */
	diff_eq_int("allpass n_a", toneiir_configuration_allpass.n_a,
		    ref_toneiir_configuration_allpass.n_a, 0);
	diff_eq_int("allpass n_b", toneiir_configuration_allpass.n_b,
		    ref_toneiir_configuration_allpass.n_b, 0);
	diff_eq_int("allpass a is NULL", toneiir_configuration_allpass.a == 0,
		    ref_toneiir_configuration_allpass.a == 0, 0);
	diff_eq_int("allpass scales is NULL",
		    toneiir_configuration_allpass.scales == 0,
		    ref_toneiir_configuration_allpass.scales == 0, 0);
	diff_eq_int("allpass b[0]", toneiir_configuration_allpass.b[0],
		    ref_toneiir_configuration_allpass.b[0], 0);
	diff_eq_int("allpass threshold", toneiir_configuration_allpass.threshold,
		    ref_toneiir_configuration_allpass.threshold, 0);

	return diff_end();
}

static int
run_toneiir_create(void)
{
	struct toneiir_cfg ca, cb;
	unsigned char bufa[sizeof(struct toneiir)];
	unsigned char bufb[sizeof(struct toneiir)];
	struct toneiir *a, *b;

	diff_begin("toneiir_create");

	diff_eq_int("object is 168 bytes", (int)sizeof(struct toneiir), 0xa8,
		    0);

	make_cfg(&ca, 1, 300, 2200);
	make_cfg(&cb, 0, 300, 2200);

	/*
	 * Identical junk in both.  create reads env_band before writing it and
	 * copies what it finds into env_prev, so the pre-fill is what makes
	 * that field comparable at all -- see D14.
	 */
	memset(bufa, 0x3C, sizeof(bufa));
	memset(bufb, 0x3C, sizeof(bufb));

	a = ref_toneiir_create((struct toneiir *)bufa, &ca);
	b = toneiir_create((struct toneiir *)bufb, &cb);
	diff_eq_int("returns its argument", b == (struct toneiir *)bufb, 1, 0);
	compare_toneiir(b, a, 0);
	diff_eq_int("env_prev carries the pre-fill", b->env_prev,
		    (short)0x3C3C, 0);
	diff_eq_int("need is 35 intervals", b->need, 35, 0);

	/* A short coefficient list, to check the clamp and the clear bounds. */
	make_cfg(&ca, 1, 300, 500);
	make_cfg(&cb, 0, 300, 500);
	ca.n_a = cb.n_a = 40;
	ca.n_b = cb.n_b = 3;
	memset(bufa, 0x11, sizeof(bufa));
	memset(bufb, 0x11, sizeof(bufb));
	a = ref_toneiir_create((struct toneiir *)bufa, &ca);
	b = toneiir_create((struct toneiir *)bufb, &cb);
	compare_toneiir(b, a, 1);
	diff_eq_int("n_a clamped to 25", b->cfg.n_a, IIR_FILTER_MAX_COEFF, 1);
	diff_eq_int("n_b left at 3", b->cfg.n_b, 3, 1);
	diff_eq_int("x[3] untouched by the clear", b->x[3], (short)0x1111, 1);
	diff_eq_int("y[24] cleared", b->y[24], 0, 1);

	/* Allocation. */
	harness_alloc_reset();
	make_cfg(&ca, 1, 300, 2200);
	make_cfg(&cb, 0, 300, 2200);
	a = ref_toneiir_create(0, &ca);
	b = toneiir_create(0, &cb);
	diff_eq_int("allocs", harness_alloc.allocs, 2, 0);
	diff_eq_int("bytes", (int)harness_alloc.bytes, 2 * 0xa8, 0);
	ref_toneiir_delete(a);
	toneiir_delete(b);
	diff_eq_int("live", harness_alloc.live, 0, 0);
	diff_eq_int("bad frees", harness_alloc.bad_free, 0, 0);

	return diff_end();
}

static int
run_toneiir_reset(void)
{
	struct toneiir_cfg ca, cb;
	unsigned char bufa[sizeof(struct toneiir)];
	unsigned char bufb[sizeof(struct toneiir)];
	struct toneiir *a, *b;
	int i;

	diff_begin("toneiir_reset");

	make_cfg(&ca, 1, 300, 2200);
	make_cfg(&cb, 0, 300, 2200);
	memset(bufa, 0, sizeof(bufa));
	memset(bufb, 0, sizeof(bufb));
	a = ref_toneiir_create((struct toneiir *)bufa, &ca);
	b = toneiir_create((struct toneiir *)bufb, &cb);

	/* Run some signal in so reset has something to clear. */
	for (i = 0; i < 900; i++) {
		short v = noise(9000);

		ref_toneiir_progress(a, v);
		toneiir_progress(b, v);
	}
	diff_eq_int("state is dirty before reset", a->env_band != 0, 1, 0);

	ref_toneiir_reset(a);
	toneiir_reset(b);
	compare_toneiir(b, a, 0);
	diff_eq_int("env_prev took the band envelope", b->env_prev != 0, 1, 0);
	diff_eq_int("history is NOT cleared", b->x[0] != 0 || b->y[0] != 0, 1,
		    0);

	return diff_end();
}

/*
 * `want` is the verdict this stimulus should mostly produce once the detector
 * has settled, or -1 to make no claim.
 */
static int
run_toneiir_progress(const char *label, double freq, int amplitude,
		     int samples, int threshold, int want)
{
	struct toneiir_cfg ca, cb;
	struct toneiir a, b;
	double phase = 0.0;
	int hits = 0, decided = 0;
	int hist[3];
	int i;

	diff_begin(label);
	memset(hist, 0, sizeof(hist));

	make_cfg(&ca, 1, threshold, 2200);
	make_cfg(&cb, 0, threshold, 2200);
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	ref_toneiir_create(&a, &ca);
	toneiir_create(&b, &cb);

	for (i = 0; i < samples; i++) {
		short v;
		int va, vb;

		if (freq > 0.0) {
			v = (short)(amplitude * sin(phase));
			phase += 2.0 * 3.14159265358979323846 * freq / 8000.0;
		} else {
			v = amplitude ? noise(amplitude) : 0;
		}

		va = ref_toneiir_progress(&a, v);
		vb = toneiir_progress(&b, v);

		diff_eq_int("sample %ld: verdict", vb, va, i);
		if (va != 0 || (i % 100) == 0)
			compare_toneiir(&b, &a, i);

		if (va >= 0 && va < 3) {
			verdict_seen[va]++;
			hist[va]++;
		}
		if (va != TONEIIR_UNDECIDED) {
			decided++;
			/* Judge only the second half, after settling. */
			if (i > samples / 2 && va == want)
				hits++;
		}
	}

	if (want >= 0) {
		char msg[128];

		snprintf(msg, sizeof(msg),
			 "verdict %d dominates late: %d hits, %d intervals "
			 "[%d %d %d]", want, hits, decided, hist[0], hist[1],
			 hist[2]);
		diff_eq_int(msg, hits * 4 > decided, 1, want);
	}

	return diff_end();
}

/*
 * --------------------------------------------------------------------------
 * 3. the design itself
 */
static double
design_response_db(const short *a, const short *b, const short *shift,
		   double freq)
{
	struct iir_filter f;
	short buf[128];
	double sum = 0.0;
	double phase = 0.0;
	double step = 2.0 * 3.14159265358979323846 * freq / 9600.0;
	const int settle = 40;	/* blocks discarded while the filter settles */
	const int measure = 60;
	int i, n;

	_iir_filter_create(&f, 12, 12, a, b, shift);

	for (n = 0; n < settle + measure; n++) {
		for (i = 0; i < 128; i++) {
			buf[i] = (short)(16000.0 * sin(phase));
			phase += step;
		}
		_iir_filter_progress(&f, 128, buf);
		if (n >= settle)
			for (i = 0; i < 128; i++)
				sum += (double)buf[i] * buf[i];
	}

	sum = sqrt(sum / ((double)measure * 128.0));
	/* Relative to the 16000-amplitude input, whose RMS is 16000/sqrt(2). */
	return 20.0 * log10((sum + 1e-9) / (16000.0 / sqrt(2.0)));
}

static double
tone_response_db(double freq)
{
	return design_response_db(CALLPROG_BandFilter_a, CALLPROG_BandFilter_b,
				  CALLPROG_BandFilter_shift, freq);
}

/*
 * Each CPfiltrs.c design must pass mid-band and reject an octave above its
 * upper edge.  Byte equality against the reference already proves the
 * transcription; this proves the names mean what they say, which byte
 * equality cannot.
 */
static int
run_cp_response(void)
{
	int d;

	diff_begin("CPfiltrs.c designs: response");

	for (d = 0; d < CP_DESIGNS; d++) {
		const struct cp_design *c = &cp_designs[d];
		double mid = (c->lo + c->hi) / 2.0;
		double out = c->hi * 2.0;
		double mid_db = design_response_db(c->a, c->b, c->scales, mid);
		double out_db = design_response_db(c->a, c->b, c->scales, out);
		char msg[128];

		snprintf(msg, sizeof(msg),
			 "%s passes %.0f Hz at %.1f dB", c->name, mid, mid_db);
		diff_eq_int(msg, mid_db > -6.0 && mid_db < 6.0, 1, d);

		snprintf(msg, sizeof(msg),
			 "%s rejects %.0f Hz at %.1f dB", c->name, out, out_db);
		diff_eq_int(msg, out_db < -25.0, 1, d);
	}

	return diff_end();
}

static int
run_response(void)
{
	static const struct {
		double freq;
		double lo, hi;		/* permitted dB range */
		const char *what;
	} points[] = {
		{   50.0, -70.0, -20.0, "50 Hz rejected"           },
		{  350.0,  -5.0,   3.0, "350 Hz dial tone passes"  },
		{  440.0,  -5.0,   3.0, "440 Hz ringback passes"   },
		{  480.0,  -5.0,   3.0, "480 Hz busy passes"       },
		{  620.0,  -5.0,   3.0, "620 Hz busy passes"       },
		{ 1200.0,  -5.0,   3.0, "1200 Hz passes"           },
		{ 2712.0, -60.0, -20.0, "2712 Hz notch"            },
		{ 4400.0, -60.0, -25.0, "4400 Hz rejected"         }
	};
	unsigned k;

	diff_begin("call-progress band filter: response");

	for (k = 0; k < sizeof(points) / sizeof(points[0]); k++) {
		double db = tone_response_db(points[k].freq);
		char msg[96];

		snprintf(msg, sizeof(msg), "%s: %.1f dB, want %.0f..%.0f",
			 points[k].what, db, points[k].lo, points[k].hi);
		diff_eq_int(msg, db >= points[k].lo && db <= points[k].hi,
			    1, (long)points[k].freq);
	}

	return diff_end();
}

/*
 * --------------------------------------------------------------------------
 */
int
main(void)
{
	/*
	 * A coefficient set with no headroom: unity input shift and near-unit
	 * denominators, so the accumulator leaves 16 bits regularly.  The real
	 * design never does, and the wrap path would otherwise go untested.
	 */
	static const short hot_a[IIR_FILTER_MAX_COEFF] = {
		8192, -16000, 7900,  8192, -16100, 7950,
		8192, -16200, 8000,  8192, -16300, 8050,
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
	};
	static const short hot_b[IIR_FILTER_MAX_COEFF] = {
		8192, 0, -8192,  8192, 0, -8192,
		8192, 0, -8192,  8192, 0, -8192,
		-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13
	};
	static const short hot_shift[5] = { 0, 0, 0, 0, 0 };

	/* Short coefficient lists, to check create's copy bounds. */
	static const short short_a[3] = { 8192, -4096, 2048 };
	static const short short_b[3] = { 8192, 1024, 8192 };
	static const short odd_shift[5] = { 3, 1, 2, 1, 4 };

	int rc = 0;

	rc |= run_create("_iir_filter_create: real design",
			 12, 12, CALLPROG_BandFilter_a, CALLPROG_BandFilter_b,
			 CALLPROG_BandFilter_shift);
	rc |= run_create("_iir_filter_create: partial coefficients",
			 3, 3, short_a, short_b, odd_shift);
	rc |= run_create("_iir_filter_create: full 25 coefficients",
			 IIR_FILTER_MAX_COEFF, IIR_FILTER_MAX_COEFF, hot_a, hot_b,
			 odd_shift);
	rc |= run_create_alloc();

	rc |= run_progress("_iir_filter_progress: real design, quiet",
			   12, 12, CALLPROG_BandFilter_a,
			   CALLPROG_BandFilter_b, CALLPROG_BandFilter_shift,
			   2000, 160);
	rc |= run_progress("_iir_filter_progress: real design, full scale",
			   12, 12, CALLPROG_BandFilter_a,
			   CALLPROG_BandFilter_b, CALLPROG_BandFilter_shift,
			   32000, 160);
	rc |= run_progress("_iir_filter_progress: real design, single sample",
			   12, 12, CALLPROG_BandFilter_a,
			   CALLPROG_BandFilter_b, CALLPROG_BandFilter_shift,
			   32000, 1);
	rc |= run_progress("_iir_filter_progress: wrapping accumulator",
			   12, 12, hot_a, hot_b, hot_shift, 32000, 160);
	rc |= run_progress("_iir_filter_progress: nonzero interstage shifts",
			   12, 12, CALLPROG_BandFilter_a,
			   CALLPROG_BandFilter_b, odd_shift, 32000, 64);

	rc |= run_fragmentation();
	rc |= run_cp_tables();

	rc |= run_toneiir_config();
	rc |= run_toneiir_create();
	rc |= run_toneiir_reset();
	rc |= run_toneiir_progress("toneiir_progress: 550 Hz in band",
				  550.0, 9000, 40000, 300, TONEIIR_PRESENT);
	rc |= run_toneiir_progress("toneiir_progress: 1500 Hz out of band",
				  1500.0, 9000, 20000, 300, TONEIIR_ABSENT);
	rc |= run_toneiir_progress("toneiir_progress: silence",
				  0.0, 0, 20000, 300, TONEIIR_ABSENT);
	rc |= run_toneiir_progress("toneiir_progress: noise",
				  0.0, 9000, 20000, 300, -1);
	rc |= run_toneiir_progress("toneiir_progress: 550 Hz below threshold",
				  550.0, 9000, 20000, 30000, TONEIIR_ABSENT);

	rc |= run_response();
	rc |= run_cp_response();

	/*
	 * Anti-vacuity.  Without this the wrapping case above could be
	 * agreeing about a path neither implementation ever entered.
	 */
	diff_begin("guards");
	diff_eq_int("toneiir reported UNDECIDED (%ld)", verdict_seen[0] > 0, 1,
		    verdict_seen[0]);
	diff_eq_int("toneiir reported ABSENT (%ld)", verdict_seen[1] > 0, 1,
		    verdict_seen[1]);
	diff_eq_int("toneiir reported PRESENT (%ld)", verdict_seen[2] > 0, 1,
		    verdict_seen[2]);
	diff_eq_int("section outputs left 16 bits (%ld seen)",
		    wrap_seen > 1000, 1, wrap_seen);
	rc |= diff_end();

	return rc;
}
