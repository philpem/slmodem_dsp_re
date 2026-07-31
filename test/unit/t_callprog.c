/*
 * t_callprog.c -- differential test of the call-progress supervisor.
 *
 * Starts with the message-name table.  Comparing returned pointers would be
 * meaningless (they point into two different objects), so the strings are
 * compared by content, and the whole int range around the table is swept so
 * that the codes with no name -- 16 and 17, and everything outside 0..18 --
 * are checked to return the same empty string rather than something.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/callprog.h"

extern const char *ref_CALLPROG_Status_string(int status);

static int
run_status_string(void)
{
	int code;
	int named = 0;

	diff_begin("CALLPROG_Status_string");

	for (code = -8; code <= 40; code++) {
		const char *a = ref_CALLPROG_Status_string(code);
		const char *b = CALLPROG_Status_string(code);

		diff_eq_int("code %ld: ref returns non-NULL", a != 0, 1, code);
		diff_eq_int("code %ld: returns non-NULL", b != 0, 1, code);
		if (a == 0 || b == 0)
			continue;

		diff_eq_int("code %ld: same length", (int)strlen(b),
			    (int)strlen(a), code);
		diff_eq_int("code %ld: same text", strcmp(a, b) == 0, 1, code);
		if (a[0] != '\0')
			named++;
	}

	/*
	 * Anti-vacuity: two implementations that both returned "" for
	 * everything would agree perfectly.  Seventeen codes have names.
	 */
	diff_eq_int("named codes (%ld found)", named, 17, named);

	/* And the unnamed ones are empty, not merely equal. */
	diff_eq_int("code 16 is unnamed",
		    CALLPROG_Status_string(16)[0] == '\0', 1, 16);
	diff_eq_int("code 17 is unnamed",
		    CALLPROG_Status_string(17)[0] == '\0', 1, 17);
	diff_eq_int("code 19 is unnamed",
		    CALLPROG_Status_string(19)[0] == '\0', 1, 19);
	diff_eq_int("CALLPROG_BUSY is named",
		    strcmp(CALLPROG_Status_string(CALLPROG_BUSY),
			   "CALLPROG_BUSY") == 0, 1, CALLPROG_BUSY);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_status_string();

	return rc;
}
