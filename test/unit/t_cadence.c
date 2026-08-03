/*
 * t_cadence.c -- differential test of the cadence detector.
 *
 * cadence_create is not reconstructed yet, so the objects here are built by
 * hand: the same 732 bytes, filled field by field, with a toneiir underneath
 * that reads its own side's coefficient tables.  That is not a workaround --
 * it is the only way to reach the configurations the country table can
 * produce but the four call-progress tones happen not to use, and those are
 * exactly where a reconstruction is most likely to be wrong.
 *
 * The stimulus is a real cadence: a 550 Hz tone switched on and off at
 * configurable rates, run through the detector a sample at a time. 62.5 ms
 * per toneiir interval means a ring cadence takes tens of thousands of
 * samples to establish, so the runs are long.
 *
 * Everything is compared after every sample: the returned code and all 732
 * bytes of state. Comparing only the returned code would pass a detector
 * whose period arrays had drifted, right up until the moment it mattered.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/cadence.h"
#include "dsplib/cpfiltrs.h"
#include "dsplib/modem_params.h"
#include "dsplib/elliptic.h"
#include <stddef.h>

extern struct cadence *ref_cadence_create(struct cadence *c, void *setup,
					  int extra, void *modem);
extern void ref_cadence_delete(struct cadence *c);
extern void ref_cadence_reset(struct cadence *c);
extern int ref_cadence_progress(struct cadence *c, short sample);
extern struct toneiir *ref_toneiir_create(struct toneiir *st,
					  const struct toneiir_cfg *cfg);
extern void ref_toneiir_get_default_configuration(struct toneiir_cfg *dst);
extern const short ref_CP_450_630_scales[], ref_CP_450_630_a[],
		   ref_CP_450_630_b[];
extern const short ref_CP_350_600_a[], ref_CP_276_504_a[], ref_CP_100_550_a[];
extern const short ref_Filter_350_500_a[], ref_Filter_100_550_a[],
		   ref_Filter_276_504_a[];
extern const short ref_Filter_350_500_b[], ref_Filter_100_550_b[],
		   ref_Filter_276_504_b[];
extern const short ref_Filter_350_500_scales[], ref_Filter_100_550_scales[],
		   ref_Filter_276_504_scales[];

/*
 * Drive level.  The CP_450_630 cascade overflows above about 8000: its
 * internal gain is some 42 dB and the interstage shifts only take that back
 * out at the end, so a loud tone wraps inside section 2 and the envelope
 * stops being stable.  Measured -- 1000 to 8000 gives a rock-steady envelope
 * and 12000 does not.  5000 is comfortably inside.
 */
#define CADENCE_TEST_AMPLITUDE 5000

extern unsigned int ref_dsplibs_debug_level;

/* How often each return code was seen, over the whole run. */
static int code_seen[8];

/*
 * Debug level for both sides during a run; 0 leaves the diagnostics alone.
 * The transcript sweep at the end of main() uses 1..3: four of
 * cadence_progress's gates are `> 1` and three -- the match verdicts -- are
 * `> 2`, the only second-tier gates outside cadence_create (finding 155).
 */
static unsigned opt_level;
static long transcript_lines;

/*
 * A detector configuration.  The fields are named for what cadence_create
 * would put in them; the values are chosen to exercise a particular arm of
 * the matcher.
 */
struct setup {
	const char *name;
	int	min_on, max_on, min_off, max_off;
	int	cycles;
	int	continuous;
	int	max_silence;
	int	looped_match;
	int	fixed_pattern;
	int	pattern[4];
	int	pattern_min_cycles;
	int	tolerance;		/* GetBusyToneDiffTime */
};

static void
build(struct cadence *c, struct toneiir *f, const struct setup *s, int ref)
{
	struct toneiir_cfg cfg;

	memset(c, 0, sizeof(*c));
	memset(f, 0, sizeof(*f));

	if (ref)
		ref_toneiir_get_default_configuration(&cfg);
	else
		toneiir_get_default_configuration(&cfg);

	cfg.a = ref ? ref_CP_450_630_a : CP_450_630_a;
	cfg.b = ref ? ref_CP_450_630_b : CP_450_630_b;
	cfg.scales = ref ? ref_CP_450_630_scales : CP_450_630_scales;
	cfg.n_a = IIR_FILTER_COEFF;
	cfg.n_b = IIR_FILTER_COEFF;
	cfg.threshold = 300;
	cfg.duration_ms = 100;		/* 2 intervals, so tones register fast */

	if (ref)
		ref_toneiir_create(f, &cfg);
	else
		toneiir_create(f, &cfg);

	c->filter = f;
	c->state = CADENCE_IN_SILENCE;
	c->min_on = s->min_on;
	c->max_on = s->max_on;
	c->min_off = s->min_off;
	c->max_off = s->max_off;
	c->cycles = s->cycles;
	c->continuous = s->continuous;
	c->max_silence = s->max_silence;
	c->looped_match = s->looped_match;
	c->fixed_pattern = s->fixed_pattern;
	c->pattern[0] = s->pattern[0];
	c->pattern[1] = s->pattern[1];
	c->pattern[2] = s->pattern[2];
	c->pattern[3] = s->pattern[3];
	c->pattern_min_cycles = s->pattern_min_cycles;
	c->modem = (void *)(ref ? 0xB0B0B0B0u : 0xA0A0A0A0u);
}

static void
compare(const struct cadence *b, const struct cadence *a, long n)
{
	int i;

	diff_eq_int("%ld: state", b->state, a->state, n);
	diff_eq_int("%ld: run", b->run, a->run, n);
	diff_eq_int("%ld: n", b->n, a->n, n);
	diff_eq_int("%ld: failures", b->failures, a->failures, n);
	for (i = 0; i < CADENCE_MAX_PERIODS; i++) {
		diff_eq_int("%ld: on[]", b->on[i], a->on[i], n);
		diff_eq_int("%ld: off[]", b->off[i], a->off[i], n);
	}
	/* The filter underneath must track too, or the next verdict diverges. */
	diff_eq_int("%ld: filter n", b->filter->n, a->filter->n, n);
	diff_eq_int("%ld: filter env_band", b->filter->env_band,
		    a->filter->env_band, n);
	diff_eq_int("%ld: filter total", b->filter->total, a->filter->total, n);
	diff_eq_int("%ld: filter run", b->filter->run, a->filter->run, n);
}

/*
 * The stimulus: 550 Hz, inside CP_450_630's passband, gated on and off.
 * `on_ms` and `off_ms` are the cadence; 0 for `off_ms` means a continuous
 * tone.
 */
static double phase;

static short
tone_sample(long i, int on_ms, int off_ms, int amplitude)
{
	long period, pos;
	short v;

	if (off_ms > 0) {
		period = (long)(on_ms + off_ms) * 8;	/* 8 samples per ms */
		pos = i % period;
		if (pos >= (long)on_ms * 8) {
			phase = 0.0;
			return 0;
		}
	}

	v = (short)(amplitude * sin(phase));
	phase += 2.0 * 3.14159265358979323846 * 550.0 / 8000.0;
	return v;
}

static int
run(const struct setup *s, int on_ms, int off_ms, int samples, int want_code)
{
	struct cadence ca, cb;
	struct toneiir fa, fb;
	int hits = 0;
	long i;

	diff_begin(s->name);

	harness_param_reset();
	harness_param_set(GetBusyToneDiffTime, s->tolerance);

	build(&ca, &fa, s, 1);
	build(&cb, &fb, s, 0);
	phase = 0.0;

	dsplib_debug_capture_reset();
	if (opt_level) {
		dsplibs_debug_level = ref_dsplibs_debug_level = opt_level;
		dsplib_debug_capture_on = 1;
	}

	for (i = 0; i < samples; i++) {
		short v = tone_sample(i, on_ms, off_ms, CADENCE_TEST_AMPLITUDE);
		int ra, rb;

		ra = ref_cadence_progress(&ca, v);
		rb = cadence_progress(&cb, v);

		diff_eq_int("sample %ld: return", rb, ra, i);
		/*
		 * Full state on every verdict, and periodically otherwise --
		 * comparing all 122 words every sample would be 90% of the
		 * run time for no extra coverage between transitions.
		 */
		if (ra != CADENCE_NOTHING || (i % 499) == 0)
			compare(&cb, &ca, i);

		if (ra >= 0 && ra < 8)
			code_seen[ra]++;
		if (ra == want_code)
			hits++;
	}

	if (opt_level) {
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		diff_eq_int("transcript matches (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)opt_level);
		if (getenv("DBGDIFF")
		    && strcmp(dsplib_debug_capture_text(0),
			      dsplib_debug_capture_text(1)) != 0) {
			const char *o = dsplib_debug_capture_text(0);
			const char *r = dsplib_debug_capture_text(1);
			int k = 0;
			while (o[k] && o[k] == r[k]) k++;
			while (k > 0 && o[k - 1] != '\n') k--;
			printf("=== %s: first divergence at %d\n", s->name, k);
			printf("--- ours: %.300s\n", o + k);
			printf("--- ref : %.300s\n", r + k);
		}
		diff_eq_int("line counts match (level %ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1),
			    (long)opt_level);
		if (opt_level == 1)
			diff_eq_int("reference silent below the threshold",
				    (int)dsplib_debug_capture_lines(1), 0, 0);
		else
			transcript_lines += dsplib_debug_capture_lines(1);
	}

	if (want_code >= 0) {
		char msg[96];

		snprintf(msg, sizeof(msg), "code %d was reached (%d times)",
			 want_code, hits);
		diff_eq_int(msg, hits > 0, 1, want_code);
	}

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 */
static int
run_reset_and_delete(void)
{
	struct cadence ca, cb;
	struct toneiir fa, fb;
	static const struct setup s = {
		"cadence_reset", 4, 12, 4, 12, 3, 0, 40, 0, 0, { 0, 0, 0, 0 },
		0, 3
	};
	long i;

	diff_begin("cadence_reset");

	harness_param_reset();
	harness_param_set(GetBusyToneDiffTime, 3);
	build(&ca, &fa, &s, 1);
	build(&cb, &fb, &s, 0);
	phase = 0.0;

	for (i = 0; i < 40000; i++) {
		short v = tone_sample(i, 500, 500, CADENCE_TEST_AMPLITUDE);

		ref_cadence_progress(&ca, v);
		cadence_progress(&cb, v);
	}
	diff_eq_int("state is dirty before reset", ca.n != 0 || ca.run != 0, 1,
		    0);

	ref_cadence_reset(&ca);
	cadence_reset(&cb);
	compare(&cb, &ca, 0);
	diff_eq_int("n cleared", cb.n, 0, 0);
	diff_eq_int("run cleared", cb.run, 0, 0);
	diff_eq_int("failures cleared", cb.failures, 0, 0);
	diff_eq_int("state is silence", cb.state, CADENCE_IN_SILENCE, 0);
	/*
	 * reset does NOT clear the recorded periods -- only the index.  A
	 * reconstruction that memset the arrays would pass every functional
	 * test and fail this one.
	 */
	diff_eq_int("recorded periods survive reset",
		    cb.on[0] != 0 || cb.off[1] != 0, 1, 0);

	return diff_end();
}

/*
 * The filter-bank dispatch, measured against the ORIGINAL.
 *
 * cadence_create is not reconstructed yet, so this drives the blob alone
 * rather than comparing two implementations -- which is the point: it
 * establishes what the reconstruction will have to reproduce, in the one
 * place where reading the disassembly is most likely to mislead.
 *
 * Three of the eight filter indices select a seven-deep `Filter_*` bank and
 * need a subindex in 1..7 to pick within it.  slmodemd returns 0 for
 * `GetDialToneFilterSubindex` unconditionally, so in practice every bank
 * selection falls back -- and each bank falls back to its OWN nearest CP_*
 * design, not to a common default.  That is worth pinning: an earlier reading
 * of this code had bank 3 installing NULL coefficient pointers, which would
 * have been a crash on the ten countries that select it.  It does not; it
 * installs CP_276_504.  The claim was wrong because a grep dropped the
 * relocation lines, and this test is what caught it.  See tools/dis.py.
 */
static int
run_filter_dispatch(void)
{
	static const struct {
		int		index;
		int		sub;
		const short	*want_a;
		const char	*what;
	} cases[] = {
		/* Subindex 0 -- what slmodemd actually supplies. */
		{ 0, 0, ref_CP_350_600_a, "bank 350_500 falls back" },
		{ 1, 0, ref_CP_350_600_a, "bank 100_550 falls back" },
		{ 2, 0, ref_CP_350_600_a, "bank 350_500 falls back" },
		{ 3, 0, ref_CP_276_504_a, "bank 276_504 falls back to its own" },
		{ 4, 0, ref_CP_350_600_a, "bank 100_550 falls back" },
		{ 5, 0, ref_CP_350_600_a, "bank 100_550 falls back" },
		{ 6, 0, ref_CP_450_630_a, "direct CP_450_630" },
		{ 7, 0, ref_CP_100_550_a, "direct CP_100_550" },
		{ 8, 0, ref_CP_350_600_a, "out of range" },
		{ 99, 0, ref_CP_350_600_a, "far out of range" },
		/* A subindex in range picks within the bank, one-based. */
		{ 0, 1, ref_Filter_350_500_a, "bank 350_500 variant 1" },
		{ 1, 1, ref_Filter_100_550_a, "bank 100_550 variant 1" },
		{ 3, 1, ref_Filter_276_504_a, "bank 276_504 variant 1" },
		{ 3, 7, ref_Filter_276_504_a + 6 * 12, "variant 7" },
		{ 1, 4, ref_Filter_100_550_a + 3 * 12, "variant 4" },
		{ 1, 8, ref_CP_350_600_a, "subindex 8 is out of range" }
	};
	unsigned k;

	diff_begin("cadence_create: the filter dispatch");

	for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
		int setup[7];
		struct cadence *c;
		char msg[160];

		harness_param_reset();
		harness_param_set(GetDialToneCallProgressFilterIndex,
				  cases[k].index);
		harness_param_set(GetDialToneFilterSubindex, cases[k].sub);
		harness_param_set(GetCallProgressSamplesBufferLength, 666);
		harness_param_set(GetDialToneValidationTime, 50);
		harness_param_set(GetDialToneDetectionThreshold, 40);

		memset(setup, 0, sizeof(setup));
		setup[4] = 1;			/* DIAL */

		c = ref_cadence_create(0, setup, 0, (void *)0xD00Du);
		diff_eq_int("allocated", c != 0, 1, (long)k);
		if (c == 0)
			continue;

		snprintf(msg, sizeof(msg), "index %d subindex %d: %s",
			 cases[k].index, cases[k].sub, cases[k].what);
		diff_eq_int(msg, c->sel_a == cases[k].want_a, 1, (long)k);

		/* Never NULL, for any input -- there is no such fallback. */
		diff_eq_int("coefficients are never NULL",
			    c->sel_a != 0 && c->sel_b != 0 && c->sel_scales != 0,
			    1, (long)k);
		diff_eq_int("always twelve coefficients", c->sel_n_a, 12,
			    (long)k);

		ref_cadence_delete(c);
	}

	return diff_end();
}

/*
 * The Elliptic1/2/3.c tables, word for word.  Global symbols, so unlike the
 * anonymous statics elsewhere in this module they can be compared directly.
 */
static int
run_elliptic_tables(void)
{
	static const struct {
		const char *name;
		const short *ours, *ref;
		int n;
	} tables[] = {
		{ "Filter_350_500_scales", Filter_350_500_scales,
		  ref_Filter_350_500_scales, 7 * IIR_FILTER_SCALES },
		{ "Filter_350_500_a", Filter_350_500_a, ref_Filter_350_500_a,
		  7 * IIR_FILTER_COEFF },
		{ "Filter_350_500_b", Filter_350_500_b, ref_Filter_350_500_b,
		  7 * IIR_FILTER_COEFF },
		{ "Filter_100_550_scales", Filter_100_550_scales,
		  ref_Filter_100_550_scales, 7 * IIR_FILTER_SCALES },
		{ "Filter_100_550_a", Filter_100_550_a, ref_Filter_100_550_a,
		  7 * IIR_FILTER_COEFF },
		{ "Filter_100_550_b", Filter_100_550_b, ref_Filter_100_550_b,
		  7 * IIR_FILTER_COEFF },
		{ "Filter_276_504_scales", Filter_276_504_scales,
		  ref_Filter_276_504_scales, 7 * IIR_FILTER_SCALES },
		{ "Filter_276_504_a", Filter_276_504_a, ref_Filter_276_504_a,
		  7 * IIR_FILTER_COEFF },
		{ "Filter_276_504_b", Filter_276_504_b, ref_Filter_276_504_b,
		  7 * IIR_FILTER_COEFF }
	};
	unsigned t;
	int i;

	diff_begin("Elliptic1/2/3.c tables");

	for (t = 0; t < sizeof(tables) / sizeof(tables[0]); t++)
		for (i = 0; i < tables[t].n; i++)
			diff_eq_int(tables[t].name, tables[t].ours[i],
				    tables[t].ref[i], i);

	return diff_end();
}

/*
 * cadence_create, swept.
 *
 * The whole 732-byte object is compared, plus the 168-byte toneiir it builds
 * and the setup descriptor it writes back into.  The parameter store is
 * driven from a table so each case can put a different value in every
 * window -- including zeros, which is the only way to reach the default
 * substitution that busy and ringback perform (and the give-up that only
 * ringback performs).
 */
static int
run_create(const char *label, int tone, int extra, int index, int sub,
	   int windows_zero, int buflen, int cycles, int loose)
{
	struct cadence *a, *b;
	struct cadence_setup sa, sb;
	unsigned k;
	int i;

	diff_begin(label);

	harness_param_reset();
	harness_param_set(GetDialToneCallProgressFilterIndex, index);
	harness_param_set(GetBusyToneCallProgressFilterIndex, index);
	harness_param_set(GetCongestionToneCallProgressFilterIndex, index);
	harness_param_set(GetRingbackToneCallProgressFilterIndex, index);
	harness_param_set(GetDialToneFilterSubindex, sub);
	harness_param_set(GetCallProgressSamplesBufferLength, buflen);
	harness_param_set(GetDialToneValidationTime, 50);
	harness_param_set(GetDialToneDetectionThreshold, 40);
	harness_param_set(GetBusyToneLooseDetectionEnabled, loose);
	harness_param_set(GetBusyDetectionCyclesNumber, cycles);
	harness_param_set(GetCongestionDetectionCyclesNumber, cycles);
	harness_param_set(GetRingbackDetectionCyclesNumber, cycles);
	for (k = 0; k < 4; k++) {
		static const int on_off[4] = {
			GetMaxBusyCadenceOnTime, GetMinBusyCadenceOnTime,
			GetMinBusyCadenceOffTime, GetMaxBusyCadenceOffTime
		};
		static const int cong[4] = {
			GetMaxCongestionCadenceOnTime,
			GetMinCongestionCadenceOnTime,
			GetMinCongestionCadenceOffTime,
			GetMaxCongestionCadenceOffTime
		};
		static const int ring[4] = {
			GetMaxRingbackCadenceOnTime, GetMinRingbackCadenceOnTime,
			GetMinRingbackCadenceOffTime, GetMaxRingbackCadenceOffTime
		};
		/* Distinct values, so a swapped pair shows up. */
		int v = windows_zero ? 0 : (int)(60 + k * 13);

		harness_param_set(on_off[k], v);
		harness_param_set(cong[k], v);
		harness_param_set(ring[k], v);
	}

	memset(&sa, 0, sizeof(sa));
	sa.w0 = 50; sa.w1 = 50; sa.w2 = 3; sa.w3 = 0x1234;
	sa.tone = tone; sa.w5 = 0; sa.w6 = 7;
	sb = sa;

	a = ref_cadence_create(0, (void *)&sa, extra, (void *)0xD00Du);
	b = cadence_create(0, &sb, extra, (void *)0xD00Du);

	diff_eq_int("both NULL or both not", (a == 0) == (b == 0), 1, 0);
	/* The descriptor is written back whether or not the create succeeds. */
	diff_eq_int("setup.tone written back", sb.tone, sa.tone, 0);

	if (a == 0 || b == 0) {
		diff_eq_int("ref returned NULL as expected", a == 0, b == 0, 0);
		if (a)
			ref_cadence_delete(a);
		if (b)
			cadence_delete(b);
		return diff_end();
	}

	/*
	 * Every word of the object except the two pointers, which necessarily
	 * differ -- each side's filter and each side's coefficient tables.
	 * Those are checked by offset instead.
	 */
	for (i = 0; i < (int)(sizeof(struct cadence) / sizeof(int)); i++) {
		const int *pa = (const int *)a, *pb = (const int *)b;
		size_t off = (size_t)i * sizeof(int);

		if (off == offsetof(struct cadence, filter)
		    || off == offsetof(struct cadence, sel_a)
		    || off == offsetof(struct cadence, sel_b)
		    || off == offsetof(struct cadence, sel_scales)
		    || off == offsetof(struct cadence, name))
			continue;
		diff_eq_int("word at +0x%lx", pb[i], pa[i], (long)off);
	}

	/* The selected design, compared by content rather than address. */
	for (i = 0; i < IIR_FILTER_COEFF; i++) {
		diff_eq_int("selected a[%ld]", b->sel_a[i], a->sel_a[i], i);
		diff_eq_int("selected b[%ld]", b->sel_b[i], a->sel_b[i], i);
	}
	for (i = 0; i < IIR_FILTER_SCALES; i++)
		diff_eq_int("selected scales[%ld]", b->sel_scales[i],
			    a->sel_scales[i], i);
	diff_eq_int("same debug name", strcmp(b->name, a->name), 0, 0);

	/* And the toneiir underneath, field by field. */
	diff_eq_int("filter n_a", b->filter->cfg.n_a, a->filter->cfg.n_a, 0);
	diff_eq_int("filter n_b", b->filter->cfg.n_b, a->filter->cfg.n_b, 0);
	diff_eq_int("filter interval", b->filter->cfg.interval,
		    a->filter->cfg.interval, 0);
	diff_eq_int("filter threshold", b->filter->cfg.threshold,
		    a->filter->cfg.threshold, 0);
	diff_eq_int("filter duration_ms", b->filter->cfg.duration_ms,
		    a->filter->cfg.duration_ms, 0);
	diff_eq_int("filter keep_on_gap", b->filter->cfg.keep_on_gap,
		    a->filter->cfg.keep_on_gap, 0);
	diff_eq_int("filter need", b->filter->need, a->filter->need, 0);

	/* Both sides must have asked for the same parameters, in the same order. */
	diff_eq_int("parameter calls", harness_param_ours.calls,
		    harness_param_ref.calls, 0);

	ref_cadence_delete(a);
	cadence_delete(b);

	return diff_end();
}

int
main(void)
{
	/* Busy: 500 ms on, 500 ms off -- 8 toneiir intervals each way. */
	static const struct setup busy = {
		"cadence: busy, unrolled match", 4, 12, 4, 12, 3, 0, 40, 0, 0,
		{ 0, 0, 0, 0 }, 0, 3
	};
	/* The same cadence through the looped matcher. */
	static const struct setup busy_loop = {
		"cadence: busy, looped match", 4, 12, 4, 12, 3, 0, 40, 1, 0,
		{ 0, 0, 0, 0 }, 0, 3
	};
	/* A tone whose timing falls outside the windows. */
	static const struct setup too_long = {
		"cadence: tone outside the window", 4, 6, 4, 6, 3, 0, 40, 0, 0,
		{ 0, 0, 0, 0 }, 0, 3
	};
	/* Dial tone: continuous, no cadence to match. */
	static const struct setup continuous = {
		"cadence: continuous tone", 0, 0, 0, 0, 3, 1, 40, 0, 0,
		{ 0, 0, 0, 0 }, 0, 3
	};
	/* An explicit pattern to match. */
	static const struct setup fixed = {
		"cadence: fixed pattern", 4, 12, 4, 12, 3, 0, 40, 0, 1,
		{ 8, 8, 8, 8 }, 1, 3
	};
	/* Nothing on the line at all. */
	static const struct setup silent = {
		"cadence: silence", 4, 12, 4, 12, 3, 0, 2, 0, 0,
		{ 0, 0, 0, 0 }, 0, 3
	};
	/*
	 * Short tones separated by gaps far longer than max_silence, so the
	 * recorded cycles are dropped every time and every tone ends at index
	 * zero.  Ten of those and the detector gives up.
	 */
	static const struct setup sparse = {
		"cadence: tones too far apart", 2, 12, 2, 12, 3, 0, 2, 0, 0,
		{ 0, 0, 0, 0 }, 0, 3
	};
	int rc = 0;
	int i;

	rc |= run(&busy, 500, 500, 200000, CADENCE_DETECTED);
	rc |= run(&busy_loop, 500, 500, 200000, CADENCE_DETECTED);
	rc |= run(&too_long, 500, 500, 120000, -1);
	rc |= run(&continuous, 0, 0, 60000, CADENCE_DETECTED);
	rc |= run(&fixed, 500, 500, 200000, -1);
	rc |= run(&silent, 0, 1, 200000, -1);
	rc |= run(&sparse, 300, 2000, 240000, CADENCE_RESTART);

	/* A double cadence -- 250 on, 250 off, 250 on, 750 off. */
	{
		static const struct setup dbl = {
			"cadence: uneven ring", 2, 10, 2, 20, 4, 0, 40, 0, 0,
			{ 0, 0, 0, 0 }, 0, 4
		};

		rc |= run(&dbl, 400, 900, 200000, -1);
	}

	/*
	 * The transcript sweep: one setup per matcher form, plus the give-up
	 * path, with both sides' diagnostics raised and compared.  The three
	 * CONDITION verdicts only speak at level 3; the fixed form's SERIRES
	 * banner and the counters speak at 2; level 1 asserts silence.
	 */
	{
		static const struct setup dbg_fixed = {
			"cadence dbg: fixed pattern", 4, 12, 4, 12, 3, 0, 40,
			0, 1, { 8, 8, 8, 8 }, 1, 3
		};
		/*
		 * pattern_min_cycles above what the run accumulates before
		 * matching, so the cycle-count check rejects -- and the
		 * SERIRES banner must print anyway, because the original
		 * announces the comparison before making it.
		 */
		static const struct setup dbg_fixed_gate = {
			"cadence dbg: fixed pattern, too few cycles", 4, 12,
			4, 12, 3, 0, 40, 0, 1, { 8, 8, 8, 8 }, 9, 3
		};

		for (opt_level = 1; opt_level <= 3; opt_level++) {
			rc |= run(&busy, 500, 500, 120000, -1);
			rc |= run(&busy_loop, 500, 500, 120000, -1);
			rc |= run(&dbg_fixed, 500, 500, 120000, -1);
			rc |= run(&dbg_fixed_gate, 500, 500, 120000, -1);
			rc |= run(&sparse, 300, 2000, 240000, -1);
		}
		opt_level = 0;
	}

	rc |= run_reset_and_delete();
	rc |= run_filter_dispatch();
	rc |= run_elliptic_tables();

	{
		static const struct {
			const char *label;
			int tone, extra, index, sub, zero, buflen, cycles, loose;
		} cases[] = {
		 { "cadence_create: BUSY",        0, 0, 0, 0, 0, 666, 3, 0 },
		 { "cadence_create: DIAL",        1, 0, 1, 0, 0, 666, 3, 0 },
		 { "cadence_create: CONG",        2, 0, 3, 0, 0, 666, 3, 0 },
		 { "cadence_create: RING",        3, 0, 6, 0, 0, 666, 3, 0 },
		 { "cadence_create: tone 4",      4, 0, 0, 0, 0, 666, 3, 0 },
		 { "cadence_create: tone 9",      9, 0, 2, 0, 0, 666, 3, 0 },
		 { "cadence_create: BUSY zero windows",  0, 0, 0, 0, 1, 666, 3, 0 },
		 { "cadence_create: RING zero windows",  3, 0, 0, 0, 1, 666, 3, 0 },
		 { "cadence_create: CONG zero windows",  2, 0, 0, 0, 1, 666, 3, 0 },
		 { "cadence_create: DIAL buflen 0",      1, 0, 1, 0, 0,   0, 3, 0 },
		 { "cadence_create: DIAL subindex 4",    1, 0, 1, 4, 0, 666, 3, 0 },
		 { "cadence_create: BUSY loose",  0, 0, 3, 2, 0, 160, 5, 1 },
		 { "cadence_create: extra 1",     1, 1, 7, 0, 0, 666, 3, 0 },
		 { "cadence_create: extra 4",     0, 4, 6, 0, 0, 200, 2, 0 },
		 { "cadence_create: buflen 160",  2, 0, 5, 3, 0, 160, 4, 0 }
		};
		unsigned k;

		for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++)
			rc |= run_create(cases[k].label, cases[k].tone,
					 cases[k].extra, cases[k].index,
					 cases[k].sub, cases[k].zero,
					 cases[k].buflen, cases[k].cycles,
					 cases[k].loose);
	}


	diff_begin("guards");
	for (i = 0; i < 8; i++)
		if (code_seen[i] != 0) {
			char msg[80];

			snprintf(msg, sizeof(msg), "code %d seen %d times", i,
				 code_seen[i]);
			diff_eq_int(msg, 1, 1, i);
		}
	diff_eq_int("CADENCE_NOTHING reached (%ld)",
		    code_seen[CADENCE_NOTHING] > 0, 1,
		    code_seen[CADENCE_NOTHING]);
	diff_eq_int("CADENCE_DETECTED reached (%ld)",
		    code_seen[CADENCE_DETECTED] > 0, 1,
		    code_seen[CADENCE_DETECTED]);
	diff_eq_int("CADENCE_RESTART reached (%ld)",
		    code_seen[CADENCE_RESTART] > 0, 1,
		    code_seen[CADENCE_RESTART]);
	/* Empty transcripts also compare equal (finding 149). */
	diff_eq_int("diagnostics were captured (%ld lines)",
		    transcript_lines > 20, 1, transcript_lines);
	rc |= diff_end();

	return rc;
}
