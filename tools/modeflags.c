/*
 * modeflags.c -- host-side setter for dsplib MODE flags, from the environment.
 *
 * WHY THIS IS NOT IN THE LIBRARY.  Nothing under `src/` calls `getenv`: it is
 * a library, the host owns its configuration, and the object never read the
 * environment either.  A mode still has to be selectable from a bench script
 * without a rebuild, so the reader lives out here and the library keeps a
 * plain global with a safe default.
 *
 * WHY IT IS NOT `benchflags.c`.  That file is on `v34-instrumentation` and
 * carries DEBUG DUMPS and A/B arms -- things that only observe.  This one
 * carries MODES, which change what the modem does and are a master feature.
 * Keeping them apart is what stops an instrumentation build and a shipping
 * build differing in behaviour rather than only in verbosity.
 *
 * Link it into a bench build to make the modes settable:
 *
 *     DSPLIB_V34_DIGITAL_TERM=1 slmodemd ...
 *
 * A CONSTRUCTOR, so it runs before the datapump is created and there is no
 * ordering question about who reads the flag first.
 */

#include <stdlib.h>

extern int dsplib_v34_digital_term;

static int flag_from_env(const char *name, int dflt)
{
	const char *v = getenv(name);

	/* An UNSET variable keeps the default.  An empty one is a caller who
	 * meant to say something and said nothing, and is also the default --
	 * never a 1, because `FOO= cmd` is the shell's idiom for "unset". */
	if (!v || !*v)
		return dflt;
	return (int)strtol(v, NULL, 0);
}

__attribute__((constructor))
static void dsplib_modeflags_init(void)
{
	dsplib_v34_digital_term =
		flag_from_env("DSPLIB_V34_DIGITAL_TERM",
			      dsplib_v34_digital_term);
}
