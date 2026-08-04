/*
 * harness.c -- Tier-1 differential test scaffolding.  See harness.h.
 */

#include "harness.h"

int diff_checks;
int diff_failures;
int diff_max_report = 10;

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
