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
#include <string.h>
#include <math.h>

#include "harness.h"
#include "dsplib/cadence.h"
#include "dsplib/cpfiltrs.h"
#include "dsplib/modem_params.h"

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

/*
 * Drive level.  The CP_450_630 cascade overflows above about 8000: its
 * internal gain is some 42 dB and the interstage shifts only take that back
 * out at the end, so a loud tone wraps inside section 2 and the envelope
 * stops being stable.  Measured -- 1000 to 8000 gives a rock-steady envelope
 * and 12000 does not.  5000 is comfortably inside.
 */
#define CADENCE_TEST_AMPLITUDE 5000

/* How often each return code was seen, over the whole run. */
static int code_seen[8];

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

	rc |= run_reset_and_delete();
	rc |= run_filter_dispatch();

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
	rc |= diff_end();

	return rc;
}
