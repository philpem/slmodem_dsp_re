/*
 * harness.c -- Tier-1 differential test scaffolding.  See harness.h.
 */

#include <stdlib.h>	/* getenv, strtol -- the report cap, below */
#include <string.h>	/* memcpy, in the float comparison */

#include "harness.h"

int diff_checks;
int diff_failures;
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
}

int
diff_end(void)
{
	if (diff_failures == 0) {
		printf("PASS %-24s %d checks\n", diff_name, diff_checks);
		return 0;
	}
	printf("FAIL %-24s %d/%d checks failed\n",
	       diff_name, diff_failures, diff_checks);
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
 * WHAT THIS DOES NOT DO IS RELAX EXACTNESS.  `ulp_budget` of 0 is the default
 * and is a bit-for-bit comparison: the same test, better reported.  A budget
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
	long ia, ib;

	memcpy(&ia, &a, sizeof ia < sizeof a ? sizeof ia : sizeof a);
	memcpy(&ib, &b, sizeof ib < sizeof b ? sizeof ib : sizeof b);

	/* Map the sign-magnitude float order onto a monotone integer order. */
	if (ia < 0)
		ia = (long)0x80000000L - ia;
	if (ib < 0)
		ib = (long)0x80000000L - ib;
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
	 * count as equal -- the object produces them and a test that demanded
	 * one bit pattern would be asserting which NaN the coprocessor chose,
	 * which is not a property of the reconstruction.
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
