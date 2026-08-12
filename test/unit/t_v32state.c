/*
 * t_v32state.c -- differential test of V32StateName against the blob.
 *
 * `statenames` is file-local on both sides, so the comparison is on what the
 * accessor returns: the STRING, compared by content, for every index the
 * table has and for the ones around and far outside it.  The out-of-range
 * cases are the point -- the bound is unsigned in the object, so -1 must take
 * the fallback rather than reading the word before the table.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32state.h"

extern const char *ref_V32StateName(int state);

static void
one(int state)
{
	const char *ours = V32StateName(state);
	const char *ref = ref_V32StateName(state);

	diff_eq_int("name is not null (%ld)", ours != 0, ref != 0, state);
	if (ours != 0 && ref != 0)
		diff_eq_int("name matches", strcmp(ours, ref) == 0, 1, state);
}

int
main(void)
{
	int i;

	diff_begin("V32StateName: every index in the table");
	for (i = 0; i < V32_STATE_COUNT; i++)
		one(i);
	/* The names are the spec's own lettering; spot-check three anchors. */
	diff_eq_int("state 0 is STATE_A",
		    strcmp(V32StateName(V32_STATE_A), "STATE_A") == 0, 1, 0);
	diff_eq_int("state 34 is STATE_DONT_CARE",
		    strcmp(V32StateName(V32_STATE_DONT_CARE),
			   "STATE_DONT_CARE") == 0, 1, 34);
	diff_eq_int("35 names",
		    strcmp(V32StateName(V32_STATE_CLEARDOWN),
			   "STATE_CLEARDOWN") == 0, 1, 31);
	if (diff_end())
		return 1;

	diff_begin("V32StateName: out of range takes the fallback");
	one(-1);
	one(-2147483647 - 1);
	one(35);
	one(36);
	one(1000);
	one(2147483647);
	diff_eq_int("the fallback is INVALID!",
		    strcmp(V32StateName(35), "INVALID!") == 0, 1, 35);
	return diff_end();
}
