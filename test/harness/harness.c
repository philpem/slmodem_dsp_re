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
		fprintf(stderr, "  got %ld, reference %ld\n", got, want);
	} else if (diff_failures == diff_max_report) {
		fprintf(stderr, "  ... further mismatches suppressed\n");
	}
	diff_failures++;
}
