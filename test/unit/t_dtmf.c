/*
 * t_dtmf.c -- differential test of the DTMF receiver.
 *
 * A detector that answers "no digit" to everything passes a weak test
 * perfectly, so nothing here is allowed to rest on the two sides agreeing.
 * Three things are asserted independently of our code:
 *
 *   - the REFERENCE reports all sixteen distinct digit indices over the tone
 *     set below.  Not "the results varied" -- all sixteen, by index.
 *   - the reference also reports nothing at all for silence, for one tone of
 *     a pair, for an out-of-band tone and for noise.  A detector that fired
 *     on everything would be as useless as one that never fired, and the two
 *     failures look identical from inside a comparison.
 *   - the coefficient banks are the object's own bytes, and are not all one
 *     value.
 *
 * The comparison itself is the whole 152-byte object after EVERY sample, not
 * just the digit at the end: the digit is one short out of an object holding
 * eight notch states, eight energies and a six-deep decision history, and
 * everything that goes wrong goes wrong there first.  Two guard bytes' worth
 * of slack past the end catch a write that runs off the end.
 *
 * The seeded-object pass comes first and is the one that pins the layout: it
 * fills both objects with the same varied pseudorandom bytes, never zeroes
 * them, and drives them with random samples.  Field offsets that are wrong
 * by two bytes show up there in one call.
 */

#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/dtmf.h"

extern int ref_dtmf_detect(float x, void *d, short mode);
extern short ref_dtmf_progress(void *d, const float *samples, short count,
			       short mode);
extern int ref_dtmf_test(const float *e, float total, short mode);
extern void ref_dtmf_set_easy(void *d);

/*
 * `create_dtmf` is outside this closure -- `detector_create` reaches it, not
 * dtmf_detect -- so it is not reconstructed.  The object's own copy is used
 * here to put BOTH sides into the state a real caller would start from,
 * which is better than a hand-written initialiser that could be wrong in the
 * same way our code is.
 */
extern void *ref_create_dtmf(void *d);

extern const float ref_eur_coef[];
extern const float ref_us_coef[];
extern const float ref_biascoef[];

#define GUARD	32

struct box {
	struct dtmf d;
	unsigned char guard[GUARD];
};

static unsigned long seed;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

static void
fill_bytes(void *p, size_t n)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rnd() & 0xff);
}

/*
 * Finite, varied and never zero.  Random BYTES over a float field give NaN
 * about one time in 256, and a NaN comparison is a property of how the
 * compiler ordered its fcom operands rather than of the modem -- it would
 * make this test a measurement of gcc.  Every float the object can actually
 * hold here is a sum of squares or a filter state, so finite is the honest
 * seeding and the byte fill still covers everything that is not a float.
 */
static float
rnd_float(void)
{
	long v = (long)(rnd() % 40001UL) - 20000L;

	if (v == 0)
		v = 4441;
	return (float)v / 7717.0f;
}

/*
 * Seed both objects identically, varied, and never zero.
 *
 * The counters are drawn from the range the object can actually hold rather
 * than from the byte fill, and that is not softening the test -- it is what
 * makes it a test at all.  `count` is compared for EQUALITY with 40 or 45,
 * so a `count` seeded anywhere in a short's range needs some 32,000 samples
 * to come back round to the block boundary and the whole second half of
 * dtmf_detect is never entered.  The first version of this file did exactly
 * that, and its own anti-vacuity assertion is what caught it: 4,800 calls
 * per mode returned -2 every time (findings 223 and 224, working).
 *
 * Everything the object treats as data -- the notch states, the energies,
 * the sixteen unmodelled bytes at +0x80 -- stays fully random.
 */
static void
seed_object(struct box *a, struct box *b)
{
	int i;

	fill_bytes(a, sizeof(*a));
	for (i = 0; i < DTMF_TONES; i++) {
		a->d.notch_state[i][0] = rnd_float();
		a->d.notch_state[i][1] = rnd_float();
		/* Spread over four decades so dtmf_test's ratios can pass. */
		a->d.energy[i] = (float)(rnd() % 10000UL + 1UL)
		    / (float)(1UL << (rnd() % 12UL));
		a->d.hist[i] = (short)((long)(rnd() % 17UL) - 1L);
	}
	a->d.total = (float)(rnd() % 10000UL) / 1000000.0f;
	a->d.bias_state[0] = rnd_float();
	a->d.bias_state[1] = rnd_float();
	a->d.count = (short)(rnd() % 46UL);
	a->d.phase = (short)(rnd() % 2UL);
	a->d.held = (short)(rnd() % 2UL);
	a->d.digit = (short)((long)(rnd() % 17UL) - 1L);
	a->d.easy = (short)(rnd() % 2UL);
	memcpy(b, a, sizeof(*a));
}

/* create_dtmf's state, with the sixteen unmodelled bytes left random. */
static void
fresh(struct box *a, struct box *b)
{
	fill_bytes(a, sizeof(*a));
	ref_create_dtmf(&a->d);
	memcpy(b, a, sizeof(*a));
}

static void
compare(const char *what, const struct box *a, const struct box *b, long tag)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "%s: object after %%ld", what);
	diff_eq_obj(buf, struct dtmf, &a->d, &b->d, tag);
	snprintf(buf, sizeof(buf), "%s: guard after %%ld", what);
	diff_eq_int(buf, memcmp(a->guard, b->guard, GUARD), 0, tag);
}

/* ------------------------------------------------------------------ */
/* Anti-vacuity bookkeeping, all measured on the reference side.       */

static int seen_digit[16];
static int seen_quiet_case;
static int seen_report;
static int seen_hold;
static int seen_state_moved;
static int seen_state_varied;

/* ------------------------------------------------------------------ */

static const double low_hz[4] = { 697.0, 770.0, 852.0, 941.0 };
static const double high_hz[4] = { 1209.0, 1336.0, 1477.0, 1633.0 };

#define FS	8000.0
#define TWOPI	6.283185307179586

/*
 * Drive both sides with one segment and compare after every sample.
 * Returns the last digit the reference reported, or -1.
 */
static int
play(const char *what, struct box *a, struct box *b, short mode,
     double f1, double a1, double f2, double a2, int nsamples,
     double *phase1, double *phase2, int noise, long tag)
{
	int last = -1;
	int i;

	for (i = 0; i < nsamples; i++) {
		double v = 0.0;
		float x;
		int ra, rb;

		if (noise)
			v = ((double)(long)(rnd() % 20001UL) - 10000.0)
			    / 10000.0 * a1;
		else {
			if (a1 != 0.0)
				v += a1 * sin(*phase1);
			if (a2 != 0.0)
				v += a2 * sin(*phase2);
		}
		*phase1 += TWOPI * f1 / FS;
		*phase2 += TWOPI * f2 / FS;
		x = (float)v;

		ra = dtmf_detect(x, &a->d, mode);
		rb = ref_dtmf_detect(x, &b->d, mode);

		{
			char buf[128];

			snprintf(buf, sizeof(buf), "%s: return at %%ld", what);
			diff_eq_int(buf, ra, rb, tag * 100000L + i);
		}
		compare(what, a, b, tag * 100000L + i);

		if (rb >= 0) {
			last = rb;
			if (rb < 16)
				seen_digit[rb]++;
			seen_report++;
		}
		if (b->d.held)
			seen_hold++;
	}
	return last;
}

/*
 * One keypress: 60 ms of tone, then 60 ms of silence.  Three blocks of the
 * same verdict arm the detector and the digit comes out when the tone stops,
 * so both halves are needed and neither may be shortened.
 */
static int
keypress(const char *what, struct box *a, struct box *b, short mode,
	 double f1, double a1, double f2, double a2, long tag)
{
	double p1 = 0.0, p2 = 0.0;
	int r;

	r = play(what, a, b, mode, f1, a1, f2, a2, 480, &p1, &p2, 0, tag);
	r = play(what, a, b, mode, f1, 0.0, f2, 0.0, 480, &p1, &p2, 0,
		 tag + 1);
	return r;
}

static int
run_all_digits(const char *what, short mode, double amp, int easy, long tag)
{
	struct box a, b;
	int lo, hi;
	int found = 0;

	fresh(&a, &b);
	if (easy) {
		dtmf_set_easy(&a.d);
		ref_dtmf_set_easy(&b.d);
	}
	for (hi = 0; hi < 4; hi++) {
		for (lo = 0; lo < 4; lo++) {
			char buf[96];
			int want = lo + 4 * hi;
			int got;

			snprintf(buf, sizeof(buf), "%s d%d", what, want);
			got = keypress(buf, &a, &b, mode, low_hz[lo], amp,
				       high_hz[hi], amp,
				       tag + (long)want * 4);
			if (got == want)
				found++;
		}
	}
	return found;
}

static void
tables(void)
{
	int i;
	int distinct = 0;

	diff_eq_int("eur_coef matches the object (%ld)",
		    memcmp(eur_coef, ref_eur_coef, sizeof(eur_coef)), 0, 0);
	diff_eq_int("us_coef matches the object (%ld)",
		    memcmp(us_coef, ref_us_coef, sizeof(us_coef)), 0, 0);
	diff_eq_int("biascoef matches the object (%ld)",
		    memcmp(biascoef, ref_biascoef, sizeof(biascoef)), 0, 0);

	/* A table of one repeated value would match a memcmp and mean nothing. */
	for (i = 1; i < 4 * DTMF_TONES; i++)
		if (eur_coef[i] != eur_coef[0])
			distinct = 1;
	diff_eq_int("eur_coef is not all one value (%ld)", distinct, 1, 0);
	distinct = 0;
	for (i = 1; i < 4 * DTMF_TONES; i++)
		if (us_coef[i] != us_coef[0])
			distinct = 1;
	diff_eq_int("us_coef is not all one value (%ld)", distinct, 1, 0);
	distinct = 0;
	for (i = 1; i < 4; i++)
		if (biascoef[i] != biascoef[0])
			distinct = 1;
	diff_eq_int("biascoef is not all one value (%ld)", distinct, 1, 0);

	/*
	 * The two plans share their zero angles exactly and differ only in
	 * the pole radius.  That is the claim dtmf_coeffs.c's header makes,
	 * and it is cheap to hold it to it.
	 */
	distinct = 0;
	for (i = 0; i < DTMF_TONES; i++) {
		double c0 = eur_coef[i * 4];
		double c1 = eur_coef[i * 4 + 1];
		double c2 = eur_coef[i * 4 + 2];
		double c3 = eur_coef[i * 4 + 3];

		if (eur_coef[i * 4] != us_coef[i * 4])
			distinct = 1;
		/*
		 * A tolerance is right HERE and only here: this is not a
		 * comparison against the object, it is a check that the
		 * design the header claims actually fits the bytes.  The
		 * bytes are floats, so the fit cannot be exact and 1e-6
		 * relative is what a float carries.
		 */
		diff_eq_int("c2 == -c3*c3 to 1e-6, eur section %ld",
			    fabs(c2 + c3 * c3) < 1e-6, 1, i);
		diff_eq_int("c1 == -c0*c3 to 1e-6, eur section %ld",
			    fabs(c1 + c0 * c3) < 1e-6, 1, i);
	}
	diff_eq_int("the two banks share every zero angle (%ld)", distinct, 0, 0);
}

static void
seeded_pass(short mode)
{
	struct box a, b;
	int trial;
	int first = 0;
	int varied = 0;
	int moved = 0;

	first = 0x7fffffff;
	for (trial = 0; trial < 24; trial++) {
		struct box before;
		int i;

		seed_object(&a, &b);
		before = b;
		for (i = 0; i < 400; i++) {
			float x = rnd_float();
			int ra, rb;

			ra = dtmf_detect(x, &a.d, mode);
			rb = ref_dtmf_detect(x, &b.d, mode);
			diff_eq_int("seeded: return (%ld)", ra, rb,
				    trial * 1000 + i);
			compare("seeded", &a, &b, trial * 1000L + i);

			/*
			 * "The results varied" has to be measured over EVERY
			 * call, not over the last one of each trial: the last
			 * sample of a 200-sample run is almost never the one
			 * that closes a block, so a per-trial check would sit
			 * at -2 for ever and read as vacuous when it is not.
			 */
			if (first == 0x7fffffff)
				first = rb;
			else if (rb != first)
				varied = 1;
		}
		if (memcmp(&before.d, &b.d, sizeof(before.d)) != 0)
			moved = 1;
	}
	seen_state_moved += moved;
	seen_state_varied += varied;
}

int
main(void)
{
	struct box a, b;
	double p1, p2;
	int rc = 0;
	int i;
	int found_us, found_eur, found_easy;
	int quiet;

	seed = 0x2468ace0UL;

	diff_begin("dtmf: the coefficient banks");
	tables();
	rc |= diff_end();

	/*
	 * The layout pass.  Random state, random samples, whole object after
	 * every call.  This is what catches a field at the wrong offset.
	 */
	diff_begin("dtmf: seeded objects, random samples, US plan");
	seeded_pass(0);
	rc |= diff_end();

	diff_begin("dtmf: seeded objects, random samples, European plan");
	seeded_pass(DTMF_MODE_EUR);
	rc |= diff_end();

	diff_begin("dtmf: seeded-object guards");
	diff_eq_int("the calls changed the object (%ld)", seen_state_moved, 2, 0);
	diff_eq_int("they did not all end the same way (%ld)",
		    seen_state_varied, 2, 0);
	rc |= diff_end();

	/* ---- signals ---- */

	diff_begin("dtmf: all sixteen digits, US plan, full level");
	found_us = run_all_digits("us", 0, 0.5, 0, 1);
	rc |= diff_end();

	diff_begin("dtmf: all sixteen digits, European plan, full level");
	found_eur = run_all_digits("eur", DTMF_MODE_EUR, 0.5, 0, 1000);
	rc |= diff_end();

	diff_begin("dtmf: all sixteen digits, US plan, easy validity");
	found_easy = run_all_digits("easy", 0, 0.5, 1, 2000);
	rc |= diff_end();

	diff_begin("dtmf: quieter and louder, US plan");
	(void)run_all_digits("loud", 0, 0.95, 0, 3000);
	(void)run_all_digits("quiet", 0, 0.05, 0, 4000);
	rc |= diff_end();

	/*
	 * Everything that must NOT be a digit.  Counted, because "the two
	 * sides agreed on -1" is exactly what a broken detector produces.
	 */
	diff_begin("dtmf: silence, one tone, twist, out of band, noise");
	quiet = 0;
	for (i = 0; i < 2; i++) {
		short mode = (short)(i ? DTMF_MODE_EUR : 0);
		const char *nm = i ? "eur" : "us";

		fresh(&a, &b);
		p1 = p2 = 0.0;
		if (play(nm, &a, &b, mode, 0.0, 0.0, 0.0, 0.0, 960,
			 &p1, &p2, 0, 5000 + i) < 0)
			quiet++;

		/* one tone of a pair, low group only */
		if (keypress(nm, &a, &b, mode, 697.0, 0.5, 1209.0, 0.0,
			     5100 + i * 2) < 0)
			quiet++;
		/* one tone of a pair, high group only */
		if (keypress(nm, &a, &b, mode, 697.0, 0.0, 1209.0, 0.5,
			     5200 + i * 2) < 0)
			quiet++;
		/* twist far beyond the limit */
		if (keypress(nm, &a, &b, mode, 697.0, 0.5, 1209.0, 0.002,
			     5300 + i * 2) < 0)
			quiet++;
		if (keypress(nm, &a, &b, mode, 697.0, 0.002, 1209.0, 0.5,
			     5400 + i * 2) < 0)
			quiet++;
		/* out of band, one below the plan and one above */
		if (keypress(nm, &a, &b, mode, 400.0, 0.5, 1900.0, 0.5,
			     5500 + i * 2) < 0)
			quiet++;
		/* noise */
		p1 = p2 = 0.0;
		if (play(nm, &a, &b, mode, 0.0, 0.7, 0.0, 0.0, 960,
			 &p1, &p2, 1, 5600 + i) < 0)
			quiet++;
	}
	seen_quiet_case = quiet;
	rc |= diff_end();

	/* ---- dtmf_progress, the block entry point ---- */

	diff_begin("dtmf: progress over a buffer");
	{
		static float buf[960];
		double ph1 = 0.0, ph2 = 0.0;
		int n;
		short ra, rb;

		fresh(&a, &b);
		for (n = 0; n < 480; n++) {
			buf[n] = (float)(0.5 * sin(ph1) + 0.5 * sin(ph2));
			ph1 += TWOPI * 941.0 / FS;
			ph2 += TWOPI * 1633.0 / FS;
		}
		for (; n < 960; n++)
			buf[n] = 0.0f;

		ra = dtmf_progress(&a.d, buf, 480, 0);
		rb = ref_dtmf_progress(&b.d, buf, 480, 0);
		diff_eq_int("progress: first half (%ld)", ra, rb, 0);
		compare("progress first half", &a, &b, 0);

		ra = dtmf_progress(&a.d, buf + 480, 480, 0);
		rb = ref_dtmf_progress(&b.d, buf + 480, 480, 0);
		diff_eq_int("progress: second half (%ld)", ra, rb, 1);
		compare("progress second half", &a, &b, 1);
		diff_eq_int("progress: reference reported digit 15 (%ld)",
			    rb, 15, 1);

		/* a zero-length and a one-sample call */
		ra = dtmf_progress(&a.d, buf, 0, 0);
		rb = ref_dtmf_progress(&b.d, buf, 0, 0);
		diff_eq_int("progress: empty buffer (%ld)", ra, rb, 2);
		ra = dtmf_progress(&a.d, buf, 1, 0);
		rb = ref_dtmf_progress(&b.d, buf, 1, 0);
		diff_eq_int("progress: one sample (%ld)", ra, rb, 3);
		compare("progress one sample", &a, &b, 3);
	}
	rc |= diff_end();

	/* ---- dtmf_test on its own, including the paths signals never reach ---- */

	diff_begin("dtmf: dtmf_test on constructed energy vectors");
	{
		float e[8];
		int trial;

		for (trial = 0; trial < 4000; trial++) {
			short mode = (short)(trial & 1);
			float total;
			int ra, rb;

			for (i = 0; i < 8; i++) {
				/*
				 * Positive, spread over four decades, so the
				 * ratio tests land on both sides of every
				 * threshold rather than always failing the
				 * first one.
				 */
				unsigned long u = rnd();

				e[i] = (float)(u % 10000UL + 1UL)
				    / (float)(1UL << (u % 12UL));
			}
			total = (float)(rnd() % 10000UL) / 1000000.0f;
			ra = dtmf_test(e, total, mode);
			rb = ref_dtmf_test(e, total, mode);
			diff_eq_int("dtmf_test (%ld)", ra, rb, trial);
			if (rb >= 0 && rb < 16)
				seen_digit[rb]++;
		}

		/* the degenerate vectors: all equal, and all zero */
		for (i = 0; i < 8; i++)
			e[i] = 1.0f;
		diff_eq_int("dtmf_test all-equal (%ld)",
			    dtmf_test(e, 1.0f, 0), ref_dtmf_test(e, 1.0f, 0), 0);
		for (i = 0; i < 8; i++)
			e[i] = 0.0f;
		diff_eq_int("dtmf_test all-zero (%ld)",
			    dtmf_test(e, 0.0f, 0), ref_dtmf_test(e, 0.0f, 0), 0);
		diff_eq_int("dtmf_test all-zero, loud (%ld)",
			    dtmf_test(e, 1.0f, 0), ref_dtmf_test(e, 1.0f, 0), 0);
		/* exactly at the two thresholds */
		for (i = 0; i < 8; i++)
			e[i] = 1.0f;
		e[0] = 0.001f;
		e[5] = 0.001f;
		diff_eq_int("dtmf_test at the US threshold (%ld)",
			    dtmf_test(e, 0.002f, 0), ref_dtmf_test(e, 0.002f, 0),
			    0);
		diff_eq_int("dtmf_test at the European threshold (%ld)",
			    dtmf_test(e, 0.0022f, 1),
			    ref_dtmf_test(e, 0.0022f, 1), 0);
	}
	rc |= diff_end();

	/*
	 * Vectors built to sit ON each of the six decision boundaries.
	 *
	 * Random energy vectors reach almost none of them: a DTMF energy
	 * vector has a SHAPE -- the notch at the low tone passes the high
	 * tone, the notch at the high tone passes the low tone, and the other
	 * six pass both -- so `rest` and `elo + ehi` are naturally within
	 * about 30% of each other and the two bounds on their ratio bracket
	 * that.  Nothing drawn uniformly lands there.  These are built from
	 * that shape and then walked off it one condition at a time.
	 */
	diff_begin("dtmf: dtmf_test on each decision boundary");
	{
		/* E_low = 1, E_high = 2: the shape a real pair produces. */
		static const float shape[8] = {
			2.0f, 3.0f, 3.0f, 3.0f, 1.0f, 3.0f, 3.0f, 3.0f
		};
		float e[8];
		int m;

		memcpy(e, shape, sizeof(e));
		for (m = 0; m < 2; m++) {
			short mode = (short)m;

			diff_eq_int("boundary: the plain shape decodes (%ld)",
				    dtmf_test(e, 1.0f, mode),
				    ref_dtmf_test(e, 1.0f, mode), m);
			diff_eq_int("boundary: reference decoded it (%ld)",
				    ref_dtmf_test(e, 1.0f, mode), 0, m);
		}
		/* between the two level thresholds: 0.002 <= t < 0.0022 */
		diff_eq_int("boundary: 0.0021 is above the US threshold (%ld)",
			    dtmf_test(e, 0.0021f, 0),
			    ref_dtmf_test(e, 0.0021f, 0), 0);
		diff_eq_int("boundary: 0.0021 is below Europe's (%ld)",
			    dtmf_test(e, 0.0021f, 1),
			    ref_dtmf_test(e, 0.0021f, 1), 0);
		diff_eq_int("boundary: the US decodes it (%ld)",
			    ref_dtmf_test(e, 0.0021f, 0), 0, 0);
		diff_eq_int("boundary: Europe does not (%ld)",
			    ref_dtmf_test(e, 0.0021f, 1), -1, 0);

		/* twist: ehi * 2.82 < elo <= ehi * 7.94, everything else clear */
		e[0] = 4.0f;
		e[1] = e[2] = e[3] = 5.0f;
		e[4] = 1.0f;
		e[5] = e[6] = e[7] = 5.0f;
		diff_eq_int("boundary: past the tight twist limit (%ld)",
			    dtmf_test(e, 1.0f, 0), ref_dtmf_test(e, 1.0f, 0), 0);
		diff_eq_int("boundary: the reference rejects that twist (%ld)",
			    ref_dtmf_test(e, 1.0f, 0), -1, 0);

		/* the low group's own-group test, with unequal group maxima */
		e[0] = 2.0f;
		e[1] = e[2] = e[3] = 4.0f;
		e[4] = 1.0f;
		e[5] = e[6] = e[7] = 1.5f;
		diff_eq_int("boundary: unequal group maxima (%ld)",
			    dtmf_test(e, 1.0f, 0), ref_dtmf_test(e, 1.0f, 0), 0);
		diff_eq_int("boundary: the reference still decodes it (%ld)",
			    ref_dtmf_test(e, 1.0f, 0), 0, 0);
		/* and the mirror, so max_hi is the one that matters */
		e[0] = 1.0f;
		e[1] = e[2] = e[3] = 1.5f;
		e[4] = 2.0f;
		e[5] = e[6] = e[7] = 4.0f;
		diff_eq_int("boundary: unequal group maxima, mirrored (%ld)",
			    dtmf_test(e, 1.0f, 0), ref_dtmf_test(e, 1.0f, 0), 0);

		/*
		 * Every entry of a group at or above the search's starting
		 * value, so the search never runs and the index stays 0.  It
		 * is the one input for which "the running minimum" and "the
		 * array entry the index names" are different numbers, and the
		 * object re-reads the array.
		 */
		e[0] = e[1] = e[2] = e[3] = 1.5e38f;
		e[4] = 0.9e38f;
		e[5] = e[6] = e[7] = 1.4e38f;
		diff_eq_int("boundary: no low entry below the search's start (%ld)",
			    dtmf_test(e, 1.0f, 0), ref_dtmf_test(e, 1.0f, 0), 0);
		/*
		 * Tuned so the two readings of `elo` land on OPPOSITE sides of
		 * the own-group test: 1.5e38 is above 0.96 * max_lo and the
		 * search's untouched starting value is below it.  The
		 * reference says "no digit"; anything that reused the running
		 * minimum would say 0.
		 */
		diff_eq_int("boundary: the reference refuses it (%ld)",
			    ref_dtmf_test(e, 1.0f, 0), -1, 0);
	}
	rc |= diff_end();

	/*
	 * The validity rules' back half.  `check_for_valid` looks six blocks
	 * back and `check_for_valid_easy` three, and the blocks past the
	 * second are reachable only when the SAME code repeats while no digit
	 * is being held -- which a tone never produces, because the first
	 * repeat arms the hold.  Reached here by seeding the history and the
	 * energies together and letting one sample close the block.
	 */
	diff_begin("dtmf: the deep entries of both validity rules");
	{
		static const float shape[8] = {
			2.0f, 3.0f, 3.0f, 3.0f, 1.0f, 3.0f, 3.0f, 3.0f
		};
		static const short pattern[6][8] = {
			/* {v,v,v,u,u,u,u,u} -> after the shift h[0..3] all v */
			{ 0, 0, 0, 5, 5, 5, 5, 5 },
			/* h[3] differs, h[4] does not */
			{ 0, 5, 0, 5, 5, 5, 5, 5 },
			/* the easy rule's two blocks, and one past them */
			{ 0, 5, 5, 0, 5, 5, 5, 5 },
			{ 0, 5, 5, 5, 0, 5, 5, 5 },
			/* the strict rule's ordinary accept */
			{ 0, 5, 5, 5, 5, 5, 5, 5 },
			/* everything the same, six deep */
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
		};
		int p, easy;

		for (easy = 0; easy < 2; easy++)
			for (p = 0; p < 6; p++) {
				int k;

				fresh(&a, &b);
				for (k = 0; k < DTMF_TONES; k++) {
					a.d.notch_state[k][0] = 0.0f;
					a.d.notch_state[k][1] = 0.0f;
					a.d.energy[k] = shape[k];
					a.d.hist[k] = pattern[p][k];
				}
				a.d.bias_state[0] = 0.0f;
				a.d.bias_state[1] = 0.0f;
				a.d.total = 1.0f;
				a.d.count = 39;
				a.d.phase = 1;
				a.d.held = 0;
				a.d.digit = -1;
				a.d.easy = (short)easy;
				memcpy(&b, &a, sizeof(a));

				{
					int ra = dtmf_detect(0.0f, &a.d, 0);
					int rb = ref_dtmf_detect(0.0f, &b.d, 0);
					char nm[64];

					snprintf(nm, sizeof(nm),
						 "validity easy=%d pattern %%ld",
						 easy);
					diff_eq_int(nm, ra, rb, p);
					compare("validity", &a, &b,
						easy * 100L + p);
					if (b.d.held)
						seen_hold++;
				}
			}
	}
	rc |= diff_end();

	/*
	 * dtmf_progress over a buffer holding TWO keypresses, so the "keep
	 * the last real digit" rule has something to choose between.  One
	 * digit per call cannot distinguish first from last.
	 */
	diff_begin("dtmf: progress over two digits in one call");
	{
		static float two[2400];
		double q1 = 0.0, q2 = 0.0;
		int n = 0;
		int k;
		short ra, rb;
		static const double f[2][2] = {
			{ 697.0, 1209.0 }, { 941.0, 1633.0 }
		};

		fresh(&a, &b);
		for (k = 0; k < 2; k++) {
			int j;

			for (j = 0; j < 480; j++, n++) {
				two[n] = (float)(0.5 * sin(q1)
						 + 0.5 * sin(q2));
				q1 += TWOPI * f[k][0] / FS;
				q2 += TWOPI * f[k][1] / FS;
			}
			for (j = 0; j < 720; j++, n++)
				two[n] = 0.0f;
		}
		ra = dtmf_progress(&a.d, two, (short)n, 0);
		rb = ref_dtmf_progress(&b.d, two, (short)n, 0);
		diff_eq_int("two digits in one call (%ld)", ra, rb, 0);
		compare("two digits", &a, &b, 0);
		diff_eq_int("the reference kept the LAST of the two (%ld)",
			    rb, 15, 0);
	}
	rc |= diff_end();

	/* ---- dtmf_set_easy ---- */

	diff_begin("dtmf: set_easy");
	seed_object(&a, &b);
	dtmf_set_easy(&a.d);
	ref_dtmf_set_easy(&b.d);
	diff_eq_int("set_easy: easy is set (%ld)", a.d.easy, 1, 0);
	compare("set_easy", &a, &b, 0);
	rc |= diff_end();

	/* ---- the assertions that make the run non-vacuous ---- */

	diff_begin("dtmf: guards");
	for (i = 0; i < 16; i++)
		diff_eq_int("the reference reported digit %ld at least once",
			    seen_digit[i] > 0, 1, i);
	diff_eq_int("all sixteen digits decoded, US plan (%ld)", found_us, 16, 0);
	diff_eq_int("all sixteen digits decoded, European plan (%ld)",
		    found_eur, 16, 0);
	diff_eq_int("all sixteen digits decoded, easy rule (%ld)",
		    found_easy, 16, 0);
	diff_eq_int("every non-digit case stayed quiet (%ld)",
		    seen_quiet_case, 14, 0);
	diff_eq_int("the reference held a digit (%ld)", seen_hold > 0, 1, 0);
	diff_eq_int("the reference reported digits (%ld)", seen_report > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
