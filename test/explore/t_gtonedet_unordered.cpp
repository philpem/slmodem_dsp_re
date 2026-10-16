/* Standalone replay of the scalar threshold regression now included in the
 * default unit fixture.  Its constructor-created component boundary has
 * finite controls and one external quiet NaN; no modem reachability claim.
 * Before recovery this failed 2/64 checks.  Any future nonzero exit remains
 * a hard failure.  Run with tools/probe_generic_tone_unordered.py. */
#define main gtonedet_existing_fixture_main
#include "../unit/t_gtonedet.cpp"
#undef main

int
main(void)
{
	return run_scalar_threshold_boundary();
}
