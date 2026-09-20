/*
 * harness.c -- Tier-1 differential test scaffolding.  See harness.h.
 */

#include <stdlib.h>	/* getenv, strtol -- the report cap, below */
#include <string.h>	/* memcpy, in the float comparison */
#include <stdint.h>
#include <float.h>

#include "harness.h"

int diff_checks;
int diff_failures;
/*
 * Checks that passed ONLY because of the modern tier's relative float
 * tolerance (HARNESS_FLOAT_TOL).  Reset per group by diff_begin and printed by
 * diff_end, so the slack the tier used is never invisible.  Always zero in the
 * period build, where the macro is undefined.
 */
int diff_float_tolerant;

/*
 * A PER-FIXTURE override of the tier tolerance, and it is a no-op without
 * HARNESS_FLOAT_TOL so the period build gets no modern rounding allowance.
 * Scalar NaN/zero equality is distinct from raw storage. See harness.h for
 * why a fixture needs one.  Zero (or negative) means "use the tier default".
 *
 * `fixture_float_atol` is the absolute floor of the MIXED form; it is zero in
 * every pure-relative fixture, which is what keeps those comparisons on the
 * exact relative criterion they had.  The two setters clear each other.
 */
#ifdef HARNESS_FLOAT_TOL
static double fixture_float_tol;
static double fixture_float_atol;
static int fixture_float_mixed;
#endif

void
harness_float_tol_fixture(double eps)
{
#ifdef HARNESS_FLOAT_TOL
	fixture_float_tol = (eps > 0.0) ? eps : 0.0;
	fixture_float_atol = 0.0;
	fixture_float_mixed = 0;
#else
	(void)eps;	/* period build: the budget is and stays 0 */
#endif
}

void
harness_float_tol_fixture_mixed(double atol, double rtol)
{
#ifdef HARNESS_FLOAT_TOL
	fixture_float_atol = (atol > 0.0) ? atol : 0.0;
	fixture_float_tol = (rtol > 0.0) ? rtol : 0.0;
	fixture_float_mixed = 1;
#else
	/* period build: the budget is and stays 0 of both kinds */
	(void)atol;
	(void)rtol;
#endif
}

double
harness_float_tol(void)
{
#ifdef HARNESS_FLOAT_TOL
	if (fixture_float_mixed || fixture_float_tol > 0.0)
		return fixture_float_tol;
	return (double)HARNESS_FLOAT_TOL;
#else
	return 0.0;
#endif
}

double
harness_float_atol(void)
{
#ifdef HARNESS_FLOAT_TOL
	return fixture_float_atol;
#else
	return 0.0;
#endif
}
/*
 * Ten is right for reading a failure; it is wrong for MEASURING one.  A
 * truncated failure list is not a sample of the failures -- the DTMF batch
 * misdiagnosed a defective coefficient table from exactly that, because
 * eleven failures at one rate and none at another printed as what looked like
 * a mixture.  `DSPLIB_MAX_REPORT=0` lifts the cap for an investigation without
 * changing what any test asserts.
 */
int diff_max_report = 10;

__attribute__((constructor)) static void
diff_max_report_from_env(void)
{
	const char *s = getenv("DSPLIB_MAX_REPORT");

	if (s != 0 && *s != '\0') {
		long v = strtol(s, 0, 0);

		diff_max_report = (v <= 0) ? (1 << 30) : (int)v;
	}
}

static const char *diff_name = "?";

void
diff_begin(const char *name)
{
	diff_name = name;
	diff_checks = 0;
	diff_failures = 0;
	diff_float_tolerant = 0;
}

int
diff_end(void)
{
	if (diff_failures == 0) {
		printf("PASS %-24s %d checks", diff_name, diff_checks);
#ifdef HARNESS_FLOAT_TOL
		if (diff_float_tolerant > 0)
			printf(" (%d within modern tolerance)",
			       diff_float_tolerant);
#endif
		printf("\n");
		return 0;
	}
	printf("FAIL %-24s %d/%d checks failed", diff_name, diff_failures,
	       diff_checks);
#ifdef HARNESS_FLOAT_TOL
	if (diff_float_tolerant > 0)
		printf(" (%d within modern tolerance)", diff_float_tolerant);
#endif
	printf("\n");
	return 1;
}

/*
 * ===========================================================================
 * Value-correlation capture: apparatus for tools/fieldcorrelate.py.
 *
 * `diff_eq_obj` already has, at every checkpoint in the whole differential
 * corpus, the one thing field-naming-by-usage-inference never gets to see: a
 * live instance of a named type with every field at a real, test-driven
 * value.  This taps that stream without adding a single test input -- it is
 * off unless BOTH env vars below are set, so an ordinary `make test` or
 * `make period` run never touches it and pays one `getenv` pair per process.
 *
 * FIELDLOG_TYPE is matched against `type` by exact string equality: the same
 * spelling the call site already passes to `diff_eq_obj`/`diff_eq_obj_`
 * (`"V90CP"`, `"struct v34_receiver"`, ...).  Each matching checkpoint
 * appends one JSON line: which test (`diff_name`), the input identifier
 * already threaded through every other diff_eq_* call, and the reference
 * side's raw bytes as hex.  The reference side, not ours -- this is mining
 * the BLOB's own behaviour, and on a passing test the two are identical
 * anyway.
 *
 * tools/fieldcorrelate.py resolves the byte ranges back into fields itself,
 * via the same DWARF tools/whichfield.py already reads, so this stays a
 * dumb, cheap byte-and-metadata sink and carries no struct layout of its
 * own to go stale.
 */
static FILE *fieldlog_fp;
static const char *fieldlog_type;
static int fieldlog_checked;

static void
fieldlog_capture(const char *type, const void *want, size_t n, long input)
{
	const unsigned char *b;
	size_t i;

	if (!fieldlog_checked) {
		const char *path = getenv("DSPLIB_FIELDLOG_OUT");

		fieldlog_checked = 1;
		fieldlog_type = getenv("DSPLIB_FIELDLOG_TYPE");
		if (fieldlog_type && path)
			fieldlog_fp = fopen(path, "a");
	}
	if (fieldlog_fp == 0 || fieldlog_type == 0 ||
	    strcmp(fieldlog_type, type) != 0)
		return;

	fprintf(fieldlog_fp, "{\"test\":\"%s\",\"n\":%lu,\"input\":%ld,"
		"\"hex\":\"", diff_name, (unsigned long)n, input);
	b = want;
	for (i = 0; i < n; i++)
		fprintf(fieldlog_fp, "%02x", b[i]);
	fprintf(fieldlog_fp, "\"}\n");
	fflush(fieldlog_fp);
}

/*
 * Compare two objects and say WHERE they differ, not just that they do.
 *
 * This is the loop at t_v34rx.c:187 -- `for (i = 0; i < sizeof(da); i++)
 * diff_eq_int(..., bytes[i], bytes[i], i)` -- which several tests open-code
 * because a plain memcmp gives a boolean and a boolean sends you back to the
 * disassembly with nothing to go on.
 *
 * Three things it adds over writing that loop again:
 *
 *   It reports the field, not the offset.  `type` is the C type name, and
 *   tools/whichfield.py resolves (type, offset) against the DWARF our own
 *   objects already carry, so the message names `dp.point` rather than
 *   leaving byte 42 to be looked up by hand.
 *
 *   It groups.  One wrong computation corrupts one field, which is a run of
 *   two or four bytes; reported per byte that is four failures and four
 *   copies of the same news.  Runs are coalesced and counted as one check.
 *
 *   It reports the FIRST difference by offset.  Where the divergence starts
 *   is the diagnostic; everything downstream of it is consequence.
 */
void
diff_eq_obj_(const char *file, int line, const char *what, const char *type,
	     const void *got, const void *want, size_t n, long input)
{
	const unsigned char *a = got, *b = want;
	size_t i = 0, runs = 0;

	fieldlog_capture(type, want, n, input);

	diff_checks++;
	while (i < n) {
		size_t s;
		if (a[i] == b[i]) { i++; continue; }
		s = i;
		while (i < n && a[i] != b[i])
			i++;
		if (runs == 0)
			diff_failures++;		/* one object, one failure */
		if (runs < 4 && diff_failures <= diff_max_report) {
			fprintf(stderr, "%s:%d: %s [input %ld]: %s+%zu",
				file, line, what, input, type, s);
			if (i - s > 1)
				fprintf(stderr, "..+%zu", i - 1);
			fprintf(stderr, "  got");
			for (size_t k = s; k < i && k < s + 8; k++)
				fprintf(stderr, " %02x", a[k]);
			fprintf(stderr, ", reference");
			for (size_t k = s; k < i && k < s + 8; k++)
				fprintf(stderr, " %02x", b[k]);
			fprintf(stderr, "\n");
			if (runs == 0)
				fprintf(stderr, "    which field: "
					"tools/whichfield.py %s %zu\n", type, s);
		}
		runs++;
	}
	if (runs > 4 && diff_failures <= diff_max_report)
		fprintf(stderr, "    ... and %zu more differing runs\n",
			runs - 4);
}

/*
 * The exact-byte half of a field-typed object compare.  It is diff_eq_obj_'s
 * own loop over a sub-range, so a non-float field that changes reports the
 * same way and with the same run coalescing.
 */
static void
diff_obj_exact_(const char *file, int line, const char *what, const char *type,
		const unsigned char *a, const unsigned char *b, size_t s,
		size_t e, long input)
{
	size_t i = s, runs = 0;

	if (s >= e)
		return;
	diff_checks++;
	while (i < e) {
		size_t r;

		if (a[i] == b[i]) { i++; continue; }
		r = i;
		while (i < e && a[i] != b[i])
			i++;
		if (runs == 0)
			diff_failures++;
		if (runs < 4 && diff_failures <= diff_max_report) {
			size_t k;

			fprintf(stderr, "%s:%d: %s [input %ld]: %s+%zu",
				file, line, what, input, type, r);
			if (i - r > 1)
				fprintf(stderr, "..+%zu", i - 1);
			fprintf(stderr, "  got");
			for (k = r; k < i && k < r + 8; k++)
				fprintf(stderr, " %02x", a[k]);
			fprintf(stderr, ", reference");
			for (k = r; k < i && k < r + 8; k++)
				fprintf(stderr, " %02x", b[k]);
			fprintf(stderr, "\n");
			if (runs == 0)
				fprintf(stderr, "    which field: "
					"tools/whichfield.py %s %zu\n",
					type, r);
		}
		runs++;
	}
	if (runs > 4 && diff_failures <= diff_max_report)
		fprintf(stderr, "    ... and %zu more differing runs\n",
			runs - 4);
}

/*
 * ===========================================================================
 * FIELD-TYPED OBJECT COMPARISON.
 *
 * WHY THIS EXISTS.  diff_eq_obj_ carries no field-type information -- it
 * compares bytes -- so a float field whose value differs by a rounding-level
 * ULP reads exactly like a wrong index or a changed decision.  On the modern
 * tier (GCC 14, x32->x64) that ULP is a portability artefact and the tier's
 * relative float tolerance must reach it; a changed index, flag or decision
 * in the same object must stay a hard failure.  Widening diff_eq_obj itself,
 * or HARNESS_FLOAT_TOL, is the wrong move for exactly that reason.
 *
 * The caller names {byte offset, element count, element size} spans relative
 * to the object's start. Modern builds compare their numerical values;
 * period/no-define builds compare their raw storage (zero signs and NaN
 * payloads included). Every byte OUTSIDE the spans is compared exactly, with the same
 * reporting diff_eq_obj_ uses.  The spans must be sorted by offset and
 * non-overlapping, which is how they are written at every call site.
 *
 * THE NEGATIVE CONTROL is test/safety/t_field_typed.c: a float perturbed
 * beyond the tolerance must still fail, and a changed non-float byte must
 * still fail.  Findings F134/F2401 applied to a comparison.
 */
void
diff_eq_obj_float_(const char *file, int line, const char *what,
		   const char *type, const void *got, const void *want,
		   size_t n, const struct diff_float_span *spans,
		   size_t nspans, long input)
{
	const unsigned char *a = got, *b = want;
	size_t i = 0, si = 0, end = 0;

	/* Validate the ENTIRE list before reading either object or comparing any
	 * prefix. Never round down an incomplete element or clip a descriptor. */
	if (nspans && spans == 0) {
		diff_eq_int_(file, line, "null float descriptor list", 0, 1, input);
		return;
	}
	for (si = 0; si < nspans; ++si) {
		size_t off = spans[si].off, count = spans[si].count;
		unsigned sz = spans[si].size;
		if ((sz != 4 && sz != 8) || off < end || off > n ||
		    count > (size_t)-1 / sz || count > (n - off) / sz) {
			char label[256];
			snprintf(label, sizeof label, "%s invalid float descriptor %zu", what, si);
			diff_eq_int_(file, line, label, 0, 1, input);
			return;
		}
		end = off + count * sz;
	}
	si = 0;

	fieldlog_capture(type, want, n, input);

	while (i < n && si < nspans) {
		size_t off = spans[si].off;
		unsigned k, sz = spans[si].size;
		if (off > i)
			diff_obj_exact_(file, line, what, type, a, b, i, off,
					input);
		for (k = 0; k < spans[si].count; k++) {
			size_t o = off + (size_t)sz * (size_t)k;
#ifndef HARNESS_FLOAT_TOL
			diff_obj_exact_(file, line, what, type, a, b, o, o + sz, input);
#else
			char buf[256];
			snprintf(buf, sizeof buf, "%s [%s+%zu]", what, type, o);
			if (sz == 8) {
				double ga, wa;

				memcpy(&ga, a + o, 8);
				memcpy(&wa, b + o, 8);
				diff_eq_double_(file, line, buf, ga, wa, input);
			} else {
				float ga, wa;

				memcpy(&ga, a + o, 4);
				memcpy(&wa, b + o, 4);
				diff_eq_float_(file, line, buf, ga, wa, 0UL,
					       0.0, input);
			}
#endif
		}
		i = off + (size_t)sz * (size_t)spans[si].count;
		si++;
	}
	if (i < n)
		diff_obj_exact_(file, line, what, type, a, b, i, n, input);
}

void
diff_eq_int_(const char *file, int line, const char *fmt,
	     long got, long want, long input)
{
	diff_checks++;
	if (got == want)
		return;

	if (diff_failures < diff_max_report) {
		fprintf(stderr, "%s:%d: ", file, line);
		fprintf(stderr, fmt, input);
		/*
		 * THE INPUT IS THE DIAGNOSIS, and half the suite was throwing
		 * it away.  `diff_eq_int` takes an input identifier and passes
		 * it to `fmt` -- but 819 of the 1667 call sites give a format
		 * with no conversion in it, so the argument was formatted by
		 * nothing and silently dropped.  The commonest shape is the
		 * byte-compare loop
		 *
		 *     diff_eq_int("decoder state", a[i], b[i], i);
		 *
		 * which computes the offset, hands it over, and prints "got
		 * 68, reference 136" -- six of those in a row and not one says
		 * which byte.  That is a whole debugging session's worth of
		 * re-derivation, per failure.
		 *
		 * Appending it when the format did not consume it fixes all
		 * 819 without touching a single call site.
		 */
		if (strchr(fmt, '%') == NULL)
			fprintf(stderr, " [input %ld]", input);
		fprintf(stderr, "  got %ld, reference %ld\n", got, want);
	} else if (diff_failures == diff_max_report) {
		fprintf(stderr, "  ... further mismatches suppressed\n");
	}
	diff_failures++;
}

/*
 * ===========================================================================
 * Comparing floats AS floats.
 *
 * WHY THIS EXISTS.  Float fields were being compared by punning them to `int`
 * and calling `diff_eq_int`, which is exact -- correctly so -- but reports a
 * one-ULP difference as
 *
 *     word 103007  got 1078160182, reference 1078160184
 *
 * Two nine-digit integers that share no visible relationship with each other
 * or with the quantity.  Nothing in that line says the values are floats, that
 * they differ in the last place, or that 3.6470108 and 3.6470113 are what is
 * actually being disputed.  A reader has to know to reinterpret the bits.
 *
 * These are SCALAR NUMERICAL comparisons, not raw storage comparisons:
 * signed zeros and pairs of NaNs compare equal, even with zero budgets.
 * Use diff_eq_obj_, diff_eq_obj_float_ in period, or diff_eq_float_word_ in
 * period when a storage assertion must distinguish those encodings. A budget
 * above 0 is only correct where bit-exactness is UNACHIEVABLE rather than
 * merely unmet -- the modern build carries x87 intermediates at 80 bits where
 * the blob's compiler spilled them to 32, so it declines to discard precision
 * the object discarded, and no amount of source change makes the two agree.
 * Every budgeted call site must say why at the call.
 *
 * ULP DISTANCE, NOT RELATIVE ERROR, is the measure: it is exact in integers,
 * it behaves sensibly across binades, and 1 ULP is the smallest difference a
 * float can express, so a budget reads as "n representable values apart"
 * rather than as a fraction someone has to calibrate.  Denormals and zero
 * compare correctly under it; NaN is handled explicitly below.
 * ===========================================================================
 */
/*
 * NaN BY ITS BITS, NOT BY `x != x`, AND THE REASON IS A COMPILER FLAG.
 *
 * `make period` builds this apparatus with the object's own flags, which
 * include -mno-ieee-fp (finding F1990).  That flag tells GCC it may assume
 * every comparison is ordered, and the first thing it does with the licence
 * is fold `x != x` to zero -- so the self-comparison idiom does not detect a
 * NaN there, it detects nothing at all, silently, in a build that is
 * otherwise the one that decides.  `diff_eq_float_` then took two NaNs down
 * the ULP path, where the distance between two different payloads is
 * enormous and the verdict is noise.
 *
 * The bit test cannot be folded away and is exact.  Finding F2303.
 */
int
diff_isnan_f(float x)
{
	unsigned bits;

	memcpy(&bits, &x, 4);
	return (bits & 0x7f800000u) == 0x7f800000u
	    && (bits & 0x007fffffu) != 0;
}

int
diff_isnan_ld(long double x)
{
	double d = (double)x;		/* a NaN narrows to a NaN */
	unsigned lo, hi;

	memcpy(&lo, (const unsigned char *)&d, 4);
	memcpy(&hi, (const unsigned char *)&d + 4, 4);
	return (hi & 0x7ff00000u) == 0x7ff00000u
	    && ((hi & 0x000fffffu) != 0 || lo != 0);
}

unsigned long
float_ulps(float a, float b)
{
	uint32_t ia, ib;

	memcpy(&ia, &a, sizeof ia);
	memcpy(&ib, &b, sizeof ib);
	/* Unsigned monotone keys, with BOTH zeros at the same key. All
	 * differences fit uint32_t; no native-long high bytes or signed overflow.
	 * NaNs are classified separately by the scalar assertion. */
	ia = (ia & 0x80000000u) ? 0x80000000u - (ia & 0x7fffffffu)
	                           : 0x80000000u + ia;
	ib = (ib & 0x80000000u) ? 0x80000000u - (ib & 0x7fffffffu)
	                           : 0x80000000u + ib;
	return (unsigned long)(ia > ib ? ia - ib : ib - ia);
}

/*
 * TWO BOUNDS, AND A CALL SITE STATES THE ONE THAT FITS.
 *
 * ULP is the right measure for a scalar field: it is exact in integers and 1
 * ULP is the smallest expressible difference.  It is the WRONG measure near
 * zero, and an FFT is the case that proves it -- measured over the whole
 * failing set rather than the first ten, `four1`'s worst ULP distance is
 * 121,933 and its worst ABSOLUTE error is 6.1e-05.  Those are different
 * comparisons: the 121,933 is a value of -0.000618 against -0.000626, seven
 * millionths apart, while the largest real disagreement anywhere is 2 ULP on
 * a value of -295.7.  A ULP budget wide enough for the near-zero bins would
 * be meaningless for the rest.
 *
 * So an absolute epsilon exists too, and the test a budget of either kind has
 * to meet is not "how close can we get" but **would this change a decision
 * downstream**.  An error beneath the resolution of everything that consumes
 * the value is noise, and the call site should say what consumes it.
 */
void
diff_eq_float_word_(const char *file, int line, const char *fmt,
                   unsigned int got, unsigned int want,
                   double atol, double rtol, long input)
{
#ifdef HARNESS_FLOAT_TOL
	float a, b;
	double saved_atol = fixture_float_atol, saved_rtol = fixture_float_tol;
	int saved_mixed = fixture_float_mixed;
	memcpy(&a, &got, 4);
	memcpy(&b, &want, 4);
	harness_float_tol_fixture_mixed(atol, rtol);
	diff_eq_float_(file, line, fmt, a, b, 0, 0, input);
	fixture_float_atol = saved_atol;
	fixture_float_tol = saved_rtol;
	fixture_float_mixed = saved_mixed;
#else
	(void)atol;
	(void)rtol;
	diff_eq_int_(file, line, fmt, got, want, input);
#endif
}

int
harness_float_within(float got, float want)
{
	double ag, aw, d, scale;
	unsigned int ug, uw;
	if (diff_isnan_f(got) || diff_isnan_f(want))
		return diff_isnan_f(got) && diff_isnan_f(want);
	if (got == want)
		return 1;
	/* Infinite differences must not pass via an infinite relative bound. */
	memcpy(&ug, &got, 4);
	memcpy(&uw, &want, 4);
	if ((ug & 0x7f800000u) == 0x7f800000u ||
	    (uw & 0x7f800000u) == 0x7f800000u)
		return 0;
	ag = got < 0 ? -(double)got : (double)got;
	aw = want < 0 ? -(double)want : (double)want;
	d = (double)got - (double)want;
	if (d < 0) d = -d;
	scale = ag > aw ? ag : aw;
#ifdef HARNESS_FLOAT_TOL
	if (fixture_float_mixed) scale = aw;
#endif
	return d <= harness_float_atol() + harness_float_tol() * scale;
}

void
diff_eq_float_(const char *file, int line, const char *fmt, float got,
	       float want, unsigned long ulp_budget, double abs_eps,
	       long input)
{
	unsigned long d;
	int got_nan = diff_isnan_f(got), want_nan = diff_isnan_f(want);
	double diff;

	diff_checks++;

	/*
	 * NaN is not ordered, so ULP distance is meaningless for it.  Two NaNs
		 * count as equal under the established SCALAR numerical policy.
		 * Raw-storage assertions use the object/word APIs instead.
	 */
	if (got_nan || want_nan) {
		if (got_nan && want_nan)
			return;
		d = (unsigned long)-1;
		diff = 0.0;
	} else {
		d = float_ulps(got, want);
		diff = (double)got - (double)want;
		if (diff < 0.0)
			diff = -diff;
		/* Either bound satisfies; both are 0 for an exact compare. */
		if (d <= ulp_budget || diff <= abs_eps)
			return;
#ifdef HARNESS_FLOAT_TOL
		/*
		 * THE MODERN TIER'S TOLERANCE, and only where the call site
		 * stated no budget of its own.  It is reached only after the
		 * exact test above has already failed -- the counter therefore
		 * counts non-exact matches and nothing else.  The period build
		 * never defines the macro and this block is not compiled there.
		 *
		 * A fixture that named a MIXED budget gets `atol + rtol*|b|`
		 * instead of the pure-relative `rtol*max(|a|,|b|)`: the
		 * absolute floor carries the values that pass through zero,
		 * where a relative test is meaningless.  A pure-relative
			 * fixture keeps the max-based criterion. Mixed mode is explicit:
			 * zero atol still reference-scales and zero rtol is really zero.
		 */
		if (ulp_budget == 0UL && abs_eps == 0.0 &&
		    harness_float_within(got, want)) {
			diff_float_tolerant++;
			return;
		}
#endif
	}

	if (diff_failures < diff_max_report) {
		fprintf(stderr, "%s:%d: ", file, line);
		fprintf(stderr, fmt, input);
		if (strchr(fmt, '%') == NULL)
			fprintf(stderr, " [input %ld]", input);
		if (got_nan || want_nan)
			fprintf(stderr, "  got %.9g, reference %.9g"
				"  (one side is NaN)\n", (double)got,
				(double)want);
		else
			fprintf(stderr, "  got %.9g, reference %.9g"
				"  (%lu ULP, |diff| %.3g", (double)got,
				(double)want, d, diff);
		if (!(got_nan || want_nan)) {
			if (ulp_budget)
				fprintf(stderr, "; ULP budget %lu",
					ulp_budget);
			if (abs_eps > 0.0)
				fprintf(stderr, "; |diff| budget %.3g",
					abs_eps);
			fprintf(stderr, ")\n");
		}
	} else if (diff_failures == diff_max_report) {
		fprintf(stderr, "  ... further mismatches suppressed\n");
	}
	diff_failures++;
}

/*
 * A DOUBLE, with the same tier tolerance and NaN handling as a float.
 *
 * WHY IT EXISTS.  `diff_eq_obj_float_` names a field's type, and the object
 * has one double that matters: the resampler's `phase` at +0x0c, the field
 * `ResamplerTimingOffset` moves and the one t_v92modstate exists to compare.
 * A double has exactly the same x32->x64 rounding-level portability problem a
 * float does, so it needs the same reach; comparing it as two four-byte words
 * would not be the same test.
 *
 * ULP distance is computed in unsigned 64 bits. Without HARNESS_FLOAT_TOL
 * no rounding allowance is applied, but this scalar API still equates signed
 * zeros and pairs of NaNs; it is NOT a raw-storage assertion.
 */
void
diff_eq_double_(const char *file, int line, const char *fmt, double got,
		double want, long input)
{
	uint64_t ia, ib, d;
	int got_nan = diff_isnan_ld(got), want_nan = diff_isnan_ld(want);
	double diff;

	diff_checks++;

	if (got_nan || want_nan) {
		if (got_nan && want_nan)
			return;
		d = ~0ULL;
		diff = 0.0;
	} else {
		memcpy(&ia, &got, 8);
		memcpy(&ib, &want, 8);
		/* Equal infinities and both zeros retain scalar numerical equality.
		 * Reject every UNEQUAL infinity before any floating arithmetic. */
		if (got == want)
			return;
		if ((ia & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL ||
		    (ib & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL) {
			uint64_t infinity = 0x7ff0000000000000ULL;
			d = ~0ULL;
			memcpy(&diff, &infinity, sizeof diff);
			goto mismatch;
		}
		ia = (ia & 0x8000000000000000ULL) ?
		     0x8000000000000000ULL - (ia & 0x7fffffffffffffffULL) :
		     0x8000000000000000ULL + ia;
		ib = (ib & 0x8000000000000000ULL) ?
		     0x8000000000000000ULL - (ib & 0x7fffffffffffffffULL) :
		     0x8000000000000000ULL + ib;
		d = ia > ib ? ia - ib : ib - ia;
		{
			double ag = got < 0 ? -got : got, aw = want < 0 ? -want : want;
			if ((got < 0) != (want < 0) && ag > DBL_MAX - aw) {
				uint64_t infinity = 0x7ff0000000000000ULL;
				memcpy(&diff, &infinity, sizeof diff); /* diagnostic only */
			} else {
				diff = got - want;
				if (diff < 0) diff = -diff;
			}
		}
		if (d == 0ULL)
			return;
#ifdef HARNESS_FLOAT_TOL
		{
			double ag = got < 0.0 ? -got : got;
			double aw = want < 0.0 ? -want : want;
			double scale = ag > aw ? ag : aw;
			double eps = harness_float_tol();
			double atol = harness_float_atol();

			/* Normalize BEFORE adding/multiplying. Finite extremes must
			 * not produce the false comparison infinity <= infinity. */
			double distance = (got < 0) != (want < 0) ?
			                  ag / scale + aw / scale : diff / scale;
			double bound = atol / scale + eps *
			               (fixture_float_mixed ? aw / scale : 1.0);
			if (distance <= bound) {
				diff_float_tolerant++;
				return;
			}
		}
#endif
	}

	mismatch:
	if (diff_failures < diff_max_report) {
		fprintf(stderr, "%s:%d: ", file, line);
		fprintf(stderr, fmt, input);
		if (strchr(fmt, '%') == NULL)
			fprintf(stderr, " [input %ld]", input);
		if (got_nan || want_nan)
			fprintf(stderr, "  got %.17g, reference %.17g"
				"  (one side is NaN)\n", got, want);
		else
			fprintf(stderr, "  got %.17g, reference %.17g"
				"  (%llu ULP, |diff| %.3g)\n", got, want, (unsigned long long)d,
				diff);
	} else if (diff_failures == diff_max_report) {
		fprintf(stderr, "  ... further mismatches suppressed\n");
	}
	diff_failures++;
}
