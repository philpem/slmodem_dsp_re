/*
 * t_class1names.c -- differential test of `states_names` and `status_names`
 * (F9802, D1330): the two `{int id; char *name}` tables `fax_class1_progress`
 * would search, written on their own merit because their reader is not.
 *
 * NEITHER TABLE HAS A READER IN `src/` YET, so there is no call to drive and
 * nothing but `nm`/nothing observes the difference in storage class (D1330).
 * A memcmp of two byte-identical globals is exactly the shape CLAUDE.md
 * warns can look green while comparing nothing (F134), so this file proves
 * the comparison actually fires: it corrupts one entry of a scratch copy
 * first and checks that the SAME comparison catches it, before trusting a
 * clean run against the real tables.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"

extern struct class1_name ref_states_names[20];
extern struct class1_name ref_status_names[11];

/*
 * `char *` in both tables points at a `.rodata.str1.1`/`.rodata.str1.4`
 * string; two different link units place those strings at different
 * addresses, so the pointers themselves can never match -- only what they
 * point AT can.  `id` compares directly.
 */
static int
names_equal(const struct class1_name *a, const struct class1_name *b,
	   unsigned n)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		if (a[i].id != b[i].id)
			return 0;
		if (strcmp(a[i].name, b[i].name) != 0)
			return 0;
	}
	return 1;
}

static int
run_self_test(void)
{
	struct class1_name scratch[20];

	diff_begin("names_equal fires on a perturbed copy (F134)");
	memcpy(scratch, states_names, sizeof(scratch));
	diff_eq_int("unperturbed: equal (%ld)",
		    names_equal(scratch, states_names, 20), 1, 0);

	scratch[7].id = states_names[7].id + 1;
	diff_eq_int("id perturbed at [7]: caught (%ld)",
		    names_equal(scratch, states_names, 20), 0, 1);
	scratch[7].id = states_names[7].id;

	scratch[12].name = "not the real string";
	diff_eq_int("name perturbed at [12]: caught (%ld)",
		    names_equal(scratch, states_names, 20), 0, 2);
	scratch[12].name = states_names[12].name;

	diff_eq_int("restored: equal again (%ld)",
		    names_equal(scratch, states_names, 20), 1, 3);
	return diff_end();
}

static int
run_states(void)
{
	int i;

	diff_begin("states_names against the blob");
	diff_eq_int("names_equal (%ld)",
		    names_equal(states_names, ref_states_names, 20), 1, 0);
	for (i = 0; i < 20; i++) {
		diff_eq_int("id[%ld]", states_names[i].id,
			    ref_states_names[i].id, (long)i);
		diff_eq_int("strcmp(name[%ld])",
			    strcmp(states_names[i].name,
				   ref_states_names[i].name),
			    0, (long)i);
	}
	return diff_end();
}

static int
run_status(void)
{
	int i;

	diff_begin("status_names against the blob");
	diff_eq_int("names_equal (%ld)",
		    names_equal(status_names, ref_status_names, 11), 1, 0);
	for (i = 0; i < 11; i++) {
		diff_eq_int("id[%ld]", status_names[i].id,
			    ref_status_names[i].id, (long)i);
		diff_eq_int("strcmp(name[%ld])",
			    strcmp(status_names[i].name,
				   ref_status_names[i].name),
			    0, (long)i);
	}
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_self_test();
	rc |= run_states();
	rc |= run_status();
	return rc;
}
