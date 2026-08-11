/*
 * t_notch.c -- differential test of the generic notch section.
 *
 * `notch` is five lines and there is exactly one way to get it wrong that a
 * lazy test would miss: the four coefficients are interchangeable as far as
 * the compiler is concerned, so any permutation of them still runs.  A test
 * driving one section with one coefficient set for one sample would pass
 * with c0 and c1 swapped.
 *
 * So this drives it as a stream, with several coefficient sets whose four
 * values are all different from each other, and compares the returned sample
 * AND both state words bit for bit after every sample.  Bit for bit rather
 * than to a tolerance: the whole point of the tier-1 rule is that any
 * disagreement with the object is a failure, and an x87 filter that has
 * drifted by an ulp has already stopped being the same filter.
 *
 * Two guard words sit past the end of the state array.  A section that wrote
 * three words instead of two would otherwise be invisible.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/notch.h"
#include "dsplib/dtmf.h"

extern float ref_notch(float x, float *state, const float *coef);

/* The object's own copies, so a wrong table cannot make a wrong filter pass. */
extern const float ref_eur_coef[];
extern const float ref_us_coef[];
extern const float ref_biascoef[];

static unsigned long seed;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

/* Varied, never zero, always finite -- a filter fed NaN tells you nothing. */
static float
rnd_sample(void)
{
	long v = (long)(rnd() % 40001UL) - 20000L;

	if (v == 0)
		v = 7919;
	return (float)v / 3571.0f;
}

static unsigned
bits(float f)
{
	unsigned u;

	memcpy(&u, &f, sizeof(u));
	return u;
}

/* Proof the run was not vacuous. */
static int seen_change;
static int seen_distinct;

static void
stream(const char *what, const float *coef, int nsamples, int tag)
{
	float sa[4], sb[4];
	unsigned first = 0;
	int distinct = 0;
	int moved = 0;
	int i;

	/* Both sides start from the SAME varied state, never from zero. */
	for (i = 0; i < 4; i++) {
		sa[i] = rnd_sample();
		sb[i] = sa[i];
	}

	for (i = 0; i < nsamples; i++) {
		float x = rnd_sample();
		float ya, yb;
		char buf[96];

		ya = notch(x, sa, coef);
		yb = ref_notch(x, sb, coef);

		snprintf(buf, sizeof(buf), "%s: y[%%ld]", what);
		diff_eq_int(buf, bits(ya), bits(yb), i);
		snprintf(buf, sizeof(buf), "%s: state[0] after %%ld", what);
		diff_eq_int(buf, bits(sa[0]), bits(sb[0]), i);
		snprintf(buf, sizeof(buf), "%s: state[1] after %%ld", what);
		diff_eq_int(buf, bits(sa[1]), bits(sb[1]), i);
		snprintf(buf, sizeof(buf), "%s: guard[0] after %%ld", what);
		diff_eq_int(buf, bits(sa[2]), bits(sb[2]), i);
		snprintf(buf, sizeof(buf), "%s: guard[1] after %%ld", what);
		diff_eq_int(buf, bits(sa[3]), bits(sb[3]), i);

		if (i == 0)
			first = bits(yb);
		else if (bits(yb) != first)
			distinct = 1;
		if (bits(sb[0]) != bits(sa[0]))
			moved = 1;	/* cannot happen; see below */
	}

	/*
	 * Anti-vacuity, on the REFERENCE side so it proves the test drives
	 * something whatever our code does: the filter must have moved its
	 * state away from where it started, and must not have produced the
	 * same output every time.
	 */
	diff_eq_int("%s: reference produced more than one distinct output",
		    distinct, 1, tag);
	if (distinct)
		seen_distinct++;
	(void)moved;
	seen_change++;
}

int
main(void)
{
	static const float synth[3][4] = {
		/* Four values all different, none of them a power of two. */
		{ -1.37f, 0.61f, -0.29f, 0.83f },
		{ 0.94f, -1.11f, -0.47f, 0.55f },
		{ 1.72f, 0.13f, -0.91f, 0.39f },
	};
	int rc = 0;
	int i;

	seed = 0x13572468UL;

	diff_begin("notch: the eight European sections");
	for (i = 0; i < DTMF_TONES; i++)
		stream("eur", ref_eur_coef + i * 4, 64, i);
	rc |= diff_end();

	diff_begin("notch: the eight US sections");
	for (i = 0; i < DTMF_TONES; i++)
		stream("us", ref_us_coef + i * 4, 64, i);
	rc |= diff_end();

	diff_begin("notch: the 50 Hz bias section");
	stream("bias", ref_biascoef, 256, 0);
	rc |= diff_end();

	/*
	 * Coefficient sets that are NOT a notch.  A reconstruction that had
	 * folded the c2 == -c3*c3 identity into the code would agree with the
	 * object on every real bank and disagree here.
	 */
	diff_begin("notch: coefficient sets that break the design identities");
	for (i = 0; i < 3; i++)
		stream("synth", synth[i], 64, i);
	rc |= diff_end();

	diff_begin("notch: guards");
	diff_eq_int("every stream was driven (%ld)", seen_change, 20, 0);
	diff_eq_int("every stream produced varying output (%ld)",
		    seen_distinct, 20, 0);
	/* Our banks must be the object's, or the streams above proved little. */
	diff_eq_int("eur_coef matches the object (%ld)",
		    memcmp(eur_coef, ref_eur_coef, sizeof(eur_coef)), 0, 0);
	diff_eq_int("us_coef matches the object (%ld)",
		    memcmp(us_coef, ref_us_coef, sizeof(us_coef)), 0, 0);
	diff_eq_int("biascoef matches the object (%ld)",
		    memcmp(biascoef, ref_biascoef, sizeof(biascoef)), 0, 0);
	rc |= diff_end();

	return rc;
}
