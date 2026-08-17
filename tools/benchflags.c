/*
 * benchflags.c -- turn the experimental V.34 knobs on from the environment.
 *
 * LINKED ONLY INTO THE BENCH HYBRID, never into the library, the unit tests or
 * the differential tiers.  That separation is the point: `debug.h` says nothing
 * in library code sets these flags, and this file is not library code.  It
 * exists so a bench arm can be selected without a second build of the datapump,
 * which is what the pre-emphasis A/B runs did -- two binaries per experiment,
 * leaving the compiler itself as an uncontrolled variable between the arms
 * (finding 1901 could not rule it out).  One binary, two environments, cannot
 * have that problem.
 *
 *   DSPLIB_V34_FIT_PREEMP=1      least-squares tilt estimator (#144)
 *   DSPLIB_V34_DUMP_PROBE_BINS=1  print all 25 probe DFT bins at level 3
 *   DSPLIB_V34_SEED_DEFECT=1      halve the equaliser error: proves the
 *                                 replay harness can see a difference
 *   DSPLIB_V34_BLOB_PREEMP=1     force the OBJECT's two-point counter, which
 *                                 can only reach 6-10; shape matching over all
 *                                 eleven templates is the default (1956, 1961)
 *   DSPLIB_V34_RRN_ON_BADBLOCK=1  our bad-block run renegotiates (V.34
 *                                 §11.6) instead of retraining (#149)
 */
#include <stdlib.h>

extern int dsplib_v34_fit_preemp;
extern int dsplib_v34_dump_probe_bins;
extern int dsplib_v34_seed_defect;
extern int dsplib_v34_rrn_on_badblock;
extern int dsplib_v34_blob_preemp;
extern int dsplib_v34_dump_eq_taps;

static int
flag(const char *name)
{
	const char *v = getenv(name);

	return v && *v && *v != '0';
}

__attribute__((constructor))
static void
dsplib_benchflags_init(void)
{
	dsplib_v34_fit_preemp = flag("DSPLIB_V34_FIT_PREEMP");
	dsplib_v34_dump_probe_bins = flag("DSPLIB_V34_DUMP_PROBE_BINS");
	dsplib_v34_seed_defect = flag("DSPLIB_V34_SEED_DEFECT");
	dsplib_v34_rrn_on_badblock = flag("DSPLIB_V34_RRN_ON_BADBLOCK");
	dsplib_v34_blob_preemp   = flag("DSPLIB_V34_BLOB_PREEMP");
	dsplib_v34_dump_eq_taps   = flag("DSPLIB_V34_DUMP_EQ_TAPS");
}
