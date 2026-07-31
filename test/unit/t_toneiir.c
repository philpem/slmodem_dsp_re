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
	unsigned i;

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

	for (i = 0; i < sizeof(struct iir_filter); i++)
		diff_eq_int("byte %ld", ((unsigned char *)fb)[i],
			    ((unsigned char *)fa)[i], (long)i);

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
	rc |= run_response();
	rc |= run_cp_response();

	/*
	 * Anti-vacuity.  Without this the wrapping case above could be
	 * agreeing about a path neither implementation ever entered.
	 */
	diff_begin("guards");
	diff_eq_int("section outputs left 16 bits (%ld seen)",
		    wrap_seen > 1000, 1, wrap_seen);
	rc |= diff_end();

	return rc;
}
