/*
 * t_printtitle -- `V90Modem::printTitle` and `V92Modem::printTitle` against
 * the blob, through the debug-capture tier.
 *
 * THE TRANSCRIPT IS THE ONLY OUTPUT THESE FUNCTIONS HAVE.  They write no
 * memory, return nothing and do not touch `this`; every observable thing
 * either produces is a line of text.  So this is a tier-4 test in the sense
 * of docs/method/tiers.md and there is no tier-1 comparison to fall back on.
 *
 * BOTH LEVELS HAVE TO BE RAISED.  `dsplibs_debug_level` gates our side and
 * `ref_dsplibs_debug_level` the blob's, and raising one alone sends the two
 * sides down different branches for a reason that has nothing to do with the
 * modem -- the reconstruction would print nine lines and the blob none, and
 * the failure would read as a missing call site.
 *
 * LEVEL 0 IS THE VACUOUS CASE AND IT IS TESTED ANYWAY, deliberately and with
 * its own guard.  With the level down, `edprintf` still formats and encodes
 * but prints nothing, and the three gated calls do not fire, so BOTH sides
 * emit an empty transcript -- and two empty transcripts compare equal
 * whatever either function does.  A test that ran only at level 0 would pass
 * against an empty body.  So level 0 asserts the transcript IS empty, level 2
 * asserts it is NOT, and the interesting comparison is the one at 2 and 3.
 *
 * THE TEXT IS ENCODED, and that is not a problem.  `edprintf` puts its
 * message through the rotating key of src/core/encode.c before handing it to
 * `dsplibs_debug_printf`, so six of the nine lines are `$!$ ...????` rather
 * than words.  Both sides encode -- ours with our `cEncodeChar`, the blob's
 * with its own -- and the key is reset at the top of every `edprintf` call,
 * so the comparison is exact and order-independent of anything that ran
 * before.  `tools/eddecode.py` reads a captured line if a human needs to.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90Modem.h"
#include "dsplib/V92Modem.h"

extern "C" {
extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;

void ref_v90_printtitle(void *self) asm("ref__ZN8V90Modem10printTitleEv");
void ref_v92_printtitle(void *self) asm("ref__ZN8V92Modem10printTitleEv");
}

/*
 * `this` IS NEVER READ, so what it points at does not matter -- but handing
 * both sides the same non-null pointer to poisoned memory is what turns
 * "does not read it" from a claim in the header into something this file
 * would notice being wrong: a member that dereferenced it would fault, on
 * both sides, rather than quietly reading a zero.
 */
static unsigned char slot[64];

static void
run(int which, unsigned level, int expect_output, long tag)
{
	const char *ours, *theirs;
	unsigned lines_ours, lines_theirs;

	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();
	memset(slot, 0xa5, sizeof(slot));
	dsplibs_debug_level = level;
	ref_dsplibs_debug_level = level;

	if (which == 90) {
		((V90Modem *)slot)->printTitle();
		ref_v90_printtitle(slot);
	} else {
		((V92Modem *)slot)->printTitle();
		ref_v92_printtitle(slot);
	}

	dsplibs_debug_level = 0u;
	ref_dsplibs_debug_level = 0u;
	dsplib_debug_capture_on = 0;

	ours = dsplib_debug_capture_text(0);
	theirs = dsplib_debug_capture_text(1);
	lines_ours = dsplib_debug_capture_lines(0);
	lines_theirs = dsplib_debug_capture_lines(1);

	/*
	 * Line count first, then the text.  A count difference says "a call
	 * site is missing"; a text difference at an equal count says "a call
	 * site is wrong", and they want different things looked at.
	 */
	diff_eq_int("transcript line count, case %ld",
		    (long)lines_ours, (long)lines_theirs, tag);
	diff_eq_int("transcript text, case %ld",
		    strcmp(ours, theirs) == 0, 1, tag);

	/*
	 * The anti-vacuity guard.  Without it every check above is satisfied
	 * by two functions that both do nothing, which is precisely what both
	 * of them DO at level 0 and 1.
	 */
	diff_eq_int("transcript is non-empty exactly when it should be, "
		    "case %ld",
		    lines_ours > 0, expect_output, tag);

	/*
	 * AND THE BLOB'S SIDE IS CHECKED SEPARATELY, because `lines_ours > 0`
	 * is a statement about the reconstruction.  If the reference printed
	 * nothing at level 2 the harness would not be wired up and every
	 * comparison in this file would be two empty strings agreeing.
	 */
	diff_eq_int("the blob printed too, case %ld",
		    lines_theirs > 0, expect_output, tag);
}

int
main(void)
{
	int rc = 0;

	/*
	 * Levels 0 and 1 print nothing -- every gate in both functions is
	 * `> 1` -- and 2 and 3 print everything.  Sweeping past the threshold
	 * in both directions is what tells a gate at the wrong level from a
	 * gate at the right one; debug.h's own comment makes the point about
	 * `cadence_progress`'s `> 2` sites, and a function whose gates were
	 * all one step out would otherwise compare equal at whichever single
	 * level the test happened to pick.
	 */
	diff_begin("V90Modem::printTitle: transcript across the debug levels");
	run(90, 0u, 0, 900);
	run(90, 1u, 0, 901);
	run(90, 2u, 1, 902);
	run(90, 3u, 1, 903);
	rc |= diff_end();

	diff_begin("V92Modem::printTitle: transcript across the debug levels");
	run(92, 0u, 0, 920);
	run(92, 1u, 0, 921);
	run(92, 2u, 1, 922);
	run(92, 3u, 1, 923);
	rc |= diff_end();

	/*
	 * THE TWO ARE NOT THE SAME FUNCTION, and this is the check that says
	 * so.  They differ by one message and by whether the closing banner is
	 * gated, and everything else about them is identical -- which makes
	 * "reconstruct one and copy it" the obvious mistake and this the one
	 * assertion that catches it.  Both transcripts are taken at level 2
	 * from the reference side, so it is a statement about the OBJECT and
	 * not about our copy of it.
	 *
	 * THIS SECTION MUST STAY LAST, and the reason is not obvious.  It
	 * calls the reference side twice and our side not at all, so from here
	 * on the two `edprintf` key counters have seen different numbers of
	 * calls.  That is harmless because nothing follows -- but `edprintf`
	 * resets its key at the top of every call EXCEPT the "too long print
	 * string" path (include/dsplib/encode.h), so a section appended after
	 * this one could see its first comparison fail for a reason that has
	 * nothing to do with `printTitle`.  Append above, or call both sides
	 * here and keep the two counters in step.
	 */
	diff_begin("V90 and V92 printTitle do not print the same thing");
	{
		static char v90[4096];
		unsigned n90, n92;

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2u;
		ref_dsplibs_debug_level = 2u;
		ref_v90_printtitle(slot);
		n90 = dsplib_debug_capture_lines(1);
		strncpy(v90, dsplib_debug_capture_text(1), sizeof(v90) - 1);
		v90[sizeof(v90) - 1] = '\0';

		dsplib_debug_capture_reset();
		ref_v92_printtitle(slot);
		n92 = dsplib_debug_capture_lines(1);
		dsplibs_debug_level = 0u;
		ref_dsplibs_debug_level = 0u;
		dsplib_debug_capture_on = 0;

		diff_eq_int("the blob's two banners differ in text, tag %ld",
			    strcmp(v90, dsplib_debug_capture_text(1)) != 0,
			    1, 940);
		/*
		 * V.90 has the extra "Components:" line, so it prints one more
		 * than V.92.  Asserted as the DIFFERENCE rather than as two
		 * absolute counts, so it keeps meaning the same thing if the
		 * harness ever changes what it counts as a line.
		 */
		diff_eq_int("V90 prints exactly one line more than V92, "
			    "tag %ld",
			    (long)n90 - (long)n92, 1, 941);
	}
	rc |= diff_end();

	return rc;
}
