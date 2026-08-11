/*
 * t_vpcmguard.c -- the unwritten boundary, watched STOPPING.
 *
 * `vpcm_run` calls five entry points in `VPcmV34Main.cpp` that this tree has
 * not reconstructed.  They are declared WEAK in `src/pump/v90/vpcm.c`, so a
 * binary that does not supply them links with the references resolved to
 * zero -- which is what lets the other 77 test binaries, none of which calls
 * `vpcm_run`, link at all, and what keeps this tree from DEFINING a symbol
 * named after 7,278 bytes of the object it has not written.
 *
 * THIS BINARY IS THE ONE THAT DOES NOT SUPPLY THEM.  `t_vpcmrun.c` defines
 * the one that is still unwritten as a forwarder to the blob's `ref_*` copy;
 * this file deliberately defines none, so it is null here and `vpcm_run`'s
 * guard is on the only path there is.
 *
 * FOUR OF THE FIVE ARE NO LONGER UNWRITTEN.  `VPcmV34GetCleanedSamples` and
 * `VPcmV34GetCurrentSessionDP` are reconstructed in
 * `src/pump/v34/v34pcmif.c` and the two rate getters in
 * `src/pump/v34/v34pcmmain.cpp`, and every test binary links both, so all
 * four are non-null even here and no forwarder can make them otherwise.  The
 * block below asserts that too: the guard surface is what this file is
 * about, and it has to be counted in both directions or a definition that
 * silently stopped being linked would go unremarked.  `VPcmV34Progress` is
 * the one the abort actually rides on and it is untouched.
 *
 * WHY IT HAS TO BE WATCHED RATHER THAN REASONED ABOUT.  gates.md's pattern:
 * a guard that silently returned would leave a `.process` running and
 * carrying nothing, and its output -- a buffer of silence -- is exactly what
 * a modem that had correctly transmitted nothing produces.  Nothing about the
 * result distinguishes the two.  So the claim is made the only way it can be:
 * fork, call it, and require the child to have died of SIGABRT.
 *
 * The soft half is `v34hshak.c`'s rule, and finding 547's argument: a test
 * that dies cannot then be asked WHICH path it took, so the stop is what a
 * test opts out of BY NAME -- `vpcm_unwritten_reset` -- and the code is
 * always recorded either way.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "harness.h"

#include "dsplib/dp.h"
/*
 * WEAK HERE TOO, AND IT COST A RUN TO FIND OUT.  A plain declaration lets GCC
 * assume the address of a function is never null and fold `f == 0` to false
 * at compile time -- so the first version of this file reported all five
 * entry points PRESENT in a binary that had just aborted on their absence.
 * The attribute is what makes the comparison a comparison; `vpcm.c` carries
 * it for the same reason and finding 985 records the trap.
 */
#define DSPLIB_VPCM_UNWRITTEN	__attribute__((weak))
#include "dsplib/vpcm.h"

#define FRAG	48

/*
 * A root object built by hand rather than by `ref_vpcm_create`, because the
 * point of this binary is that NOTHING of the blob's V.PCM side runs in it.
 * Zeroed is a valid starting state for everything `vpcm_run` reads before it
 * reaches the first guard: both queues empty, the mute counter clear, `nbits`
 * zero so the bit pipe is not entered, and `mode` and `status` at the values
 * `vpcm_create` leaves at 0x3a9c and 0x3aa3.
 */
static struct vpcm_root root;

static void
root_reset(void)
{
	memset(&root, 0, sizeof(root));
	root.dp.id = 34;
	root.dp.modem = (void *)0xD1A1u;
	root.dp.dp_data = &root;
}

int
main(void)
{
	short in[FRAG], out[FRAG];
	int rc = 0;
	int i;
	pid_t pid;
	int wstatus = 0;

	for (i = 0; i < FRAG; i++)
		in[i] = (short)(i * 37 - 500);

	diff_begin("one VPcmV34* entry point is ABSENT from this binary and "
		   "four are WRITTEN");
	/*
	 * Without the null one the abort below proves nothing: a guard that
	 * fired because the symbol was null is only interesting if the symbol
	 * really is null, and a binary that had quietly linked the blob's
	 * copies would abort for some other reason entirely.
	 *
	 * FOUR OF THE FIVE ARE NOW DEFINED, two in `src/pump/v34/v34pcmif.c`
	 * and two in `src/pump/v34/v34pcmmain.cpp`, and this block is where
	 * that is recorded.  It is asserted rather than
	 * dropped for the reason the weak attribute exists at all: the guard
	 * surface is the claim, so it has to be counted in both directions.
	 * A definition that quietly disappeared -- the file dropped from the
	 * link, or the definition compiled under the weak macro and outranked
	 * -- would put `vpcm_run` back on `vpcm_notwritten` and NOTHING else
	 * in this tree would notice, because a run that never reaches the
	 * connect arm never asks either of them anything.
	 */
	diff_eq_int("VPcmV34Progress is unresolved", VPcmV34Progress == 0, 1,
		    0);
	diff_eq_int("VPcmV34GetCleanedSamples is DEFINED",
		    VPcmV34GetCleanedSamples != 0, 1, 0);
	diff_eq_int("VPcmV34GetCurrentSessionDP is DEFINED",
		    VPcmV34GetCurrentSessionDP != 0, 1, 0);
	diff_eq_int("VPcmV34GetCurrentRxBitRate is DEFINED",
		    VPcmV34GetCurrentRxBitRate != 0, 1, 0);
	diff_eq_int("VPcmV34GetCurrentTxBitRate is DEFINED",
		    VPcmV34GetCurrentTxBitRate != 0, 1, 0);
	/* The layout the hand-built root depends on. */
	diff_eq_int("the root is vpcm_create's allocation",
		    (int)sizeof(struct vpcm_root), 0xd258, 0);
	rc |= diff_end();

	/* --- the guard STOPS ------------------------------------------- */

	diff_begin("an unwritten entry point aborts, and it is watched doing "
		   "it");
	root_reset();
	fflush(stdout);
	fflush(stderr);
	pid = fork();
	if (pid == 0) {
		/*
		 * No `vpcm_unwritten_reset` here: this child has NOT said it
		 * intends to read the code afterwards, so the rule is that it
		 * stops.  If it returns, `_exit(0)` records that it did and
		 * the parent's claim fails on the exit status rather than on
		 * a signal that never arrived.
		 */
		(void)vpcm_run(&root.dp, in, out, FRAG);
		_exit(0);
	}
	diff_eq_int("fork", pid > 0, 1, 0);
	if (pid > 0) {
		diff_eq_int("waitpid", waitpid(pid, &wstatus, 0) == pid, 1, 0);
		diff_eq_int("the child did not return from vpcm_run",
			    WIFEXITED(wstatus), 0, 0);
		diff_eq_int("...it was killed by a signal",
			    WIFSIGNALED(wstatus), 1, 0);
		diff_eq_int("...and the signal is SIGABRT",
			    WIFSIGNALED(wstatus) ? WTERMSIG(wstatus) : 0,
			    SIGABRT, 0);
	}
	rc |= diff_end();

	/* --- and it says WHICH ----------------------------------------- */

	diff_begin("...and a test that asks by name gets the code instead");
	root_reset();
	/*
	 * Before: nothing has been recorded.  A record that was already set
	 * would make the assertion below true whatever this call did, which
	 * is the vacuous shape gates.md is about.
	 */
	diff_eq_int("nothing recorded before the call", vpcm_unwritten(),
		    VPCM_WRITTEN, 0);
	vpcm_unwritten_reset();
	diff_eq_int("...and the reset leaves it that way", vpcm_unwritten(),
		    VPCM_WRITTEN, 0);

	memset(out, 0x5a, sizeof(out));
	diff_eq_int("vpcm_run returns DPSTAT_OK in soft mode",
		    vpcm_run(&root.dp, in, out, FRAG), DPSTAT_OK, 0);
	/*
	 * FIRST WINS, and the first is `VPcmV34Progress`: it is the only one
	 * of the five on the path a block takes before the dispatch, and with
	 * it stubbed the progress code is 0 and the connect arm -- where the
	 * other three live -- is never reached.  So this fixture can reach
	 * two of the five codes and records the earlier; the remaining three
	 * are recorded here as UNREACHED rather than claimed.
	 */
	diff_eq_int("...having recorded the entry point it could not call",
		    vpcm_unwritten(), VPCM_UNWRITTEN_PROGRESS, 0);
	/*
	 * AND IT RAN TO THE END.  The tail at 0x3f19 moves `count` samples out
	 * of the output queue and compacts it whether or not anything was
	 * processed, so both counts back at zero and the caller's buffer
	 * overwritten say the guard returned INTO the function rather than out
	 * of it.  Without this the soft path could be a `return` at the guard
	 * and every claim above would still hold.
	 */
	diff_eq_int("the input queue is empty again", root.inq.count, 0, 0);
	diff_eq_int("...and so is the output queue", root.outq.count, 0, 0);
	for (i = 0; i < FRAG; i++)
		if (out[i] != 0)
			break;
	diff_eq_int("...and the caller's buffer was written, all 48 samples",
		    i, FRAG, i);
	/*
	 * The five codes are five DIFFERENT codes.  Two of them equal would
	 * make an unwritten path report the wrong one for ever, and nothing
	 * else in this tree looks.
	 */
	diff_eq_int("the five codes are distinct",
		    VPCM_UNWRITTEN_PROGRESS != VPCM_UNWRITTEN_CLEANED
		    && VPCM_UNWRITTEN_CLEANED != VPCM_UNWRITTEN_SESSIONDP
		    && VPCM_UNWRITTEN_SESSIONDP != VPCM_UNWRITTEN_RXBITRATE
		    && VPCM_UNWRITTEN_RXBITRATE != VPCM_UNWRITTEN_TXBITRATE
		    && VPCM_UNWRITTEN_PROGRESS != VPCM_WRITTEN, 1, 0);
	rc |= diff_end();

	return rc;
}
