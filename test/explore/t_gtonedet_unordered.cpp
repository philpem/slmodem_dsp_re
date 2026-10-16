/* Exploratory differential probe, excluded from test/unit auto-discovery.
 * Known hard failure: the scalar threshold accepts an external quiet NaN
 * where the original rejects it.  Run with tools/probe_generic_tone_unordered.py.
 * Reuse the fixture's constructor, allocation and whole-object comparisons. */
#define main gtonedet_existing_fixture_main
#include "../unit/t_gtonedet.cpp"
#undef main

/* Constructor-created component boundary: finite controls and a quiet-NaN
 * sample supplied by the caller.  This does not establish modem-path
 * reachability of that nonfinite sample; no internal state is planted. */
static int
run_scalar_threshold_boundary(void)
{
	static const unsigned samples[] = {
		0x3f000000u, 0x3f800000u, 0x40000000u, 0x7fc00000u
	};
	int trial;

	diff_begin("GenericToneDetector scalar threshold component boundary");
	for (trial = 0; trial < 4; trial++) {
		struct arms a;
		int hits;

		memset(&a, 0, sizeof a);
		seed(0);
		den[0] = 1.0;
		num[0] = 1.0;
		stream[0] = as_float(samples[trial]);
		hits = drive_scalar(1u, 1u, 0u, 1u, 1u, 1.0f,
				    0u, 1.0f, 1u, 1, 400 + trial, &a);
		diff_eq_int("the blob threshold outcome (trial %ld)", hits,
			    trial == 1 || trial == 2, trial);
	}
	return diff_end();
}

int
main(void)
{
	return run_scalar_threshold_boundary();
}
