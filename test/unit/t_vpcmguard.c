/*
 * t_vpcmguard.c -- the unwritten boundary, watched STOPPING.
 *
 * THE BOUNDARY MOVED, AND THAT IS WHAT THIS FILE IS NOW ABOUT.  It used to
 * watch `vpcm_run` stop on the five `VPcmV34Main.cpp` entry points it calls,
 * because `VPcmV34Progress` -- 7,278 bytes and the whole V.PCM run path --
 * was not reconstructed.  It is now, in `src/pump/v34/v34pcmmain.cpp`, and so
 * are the other four, so every one of the five is a real definition in every
 * binary and `vpcm_run`'s guards can no longer fire.  That claim is made
 * below rather than dropped, for the reason it always was: a definition that
 * quietly stopped being linked would put `vpcm_run` back on `vpcm_notwritten`
 * and NOTHING else in this tree would notice.
 *
 * WHAT IS UNWRITTEN NOW IS ONE LEVEL DOWN, AND IT IS FOUR.  `VPcmV34Progress`
 * called seven symbols nobody had reconstructed -- 7,922 bytes belonging to
 * the V.90 and V.92 arms -- and carries the same weak-reference-plus-guard
 * arrangement for them that `vpcm.c` carried for the five.  Three have since
 * been written: `GenericToneDetector::process(float *, unsigned)` at 422
 * bytes, and `v90RateReneg` and `v90RateRenegSilence` at 555 and 983 in
 * `src/pump/v34/v34pcmmain.cpp`.  The tone detector's weak reference now
 * resolves; the two transmitters have no weak reference left at all, because
 * the file that called them defines them, so their guards are gone rather
 * than satisfied.  FOUR remain, 5,962 bytes, and all four are
 * `VPcmFloModem` members.
 *
 * This binary is the one that DRIVES one of those guards: it puts the object
 * in the line-verification state, whose `VPcmFloModem::qcLineVerification` is
 * among the four, and requires the child to have died of SIGABRT.
 *
 * NONE OF THEM IS ON A V.34 CALL, which is finding 1454's measurement and the
 * reason `t_vpcmrun`'s four-way comparison of a real 33,600 connect passed
 * with all seven absent -- and still passes now that one of them is present.
 * The guard is what stands between "a path this tree cannot take" and a call
 * through a null pointer.
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
 * test opts out of BY NAME -- `v34pcm_unwritten_reset` -- and the code is
 * always recorded either way.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "harness.h"

#include "dsplib/dp.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"
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

/*
 * THE SEVEN, BY THEIR LINK NAMES -- four still unresolved and three defined,
 * declared here so that all three crossings are asserted.
 *
 * Five of them are C++ members and their mangled names are ordinary C
 * identifiers, so a C file can name them directly and does -- declaring the
 * class here would drag `VPcmFloModem.h` into a `.c`, and the point is the
 * SYMBOL rather than the signature.  The parameter lists are deliberately
 * empty: nothing here calls any of them.
 *
 * Weak for the reason above, and the reason is sharper here: four of these are
 * genuinely undefined in this binary, so a plain declaration would both fold
 * the test and leave an undefined reference at the link.  The other three are
 * weak only so that all seven claims are made the same way.
 */
extern void _ZN12VPcmFloModem11runPcmModemEPfS0_jPiS1_S1_S1_(void)
	__attribute__((weak));
extern void _ZN12VPcmFloModem17v90RunDemodulatorEPfjPiS1_(void)
	__attribute__((weak));
extern void _ZN12VPcmFloModem18qcLineVerificationEPfS0_jPiS1_S1_S1_(void)
	__attribute__((weak));
extern void _ZN12VPcmFloModem20vPcmResetPhase3ModemEv(void)
	__attribute__((weak));
extern void _ZN19GenericToneDetector7processEPfj(void) __attribute__((weak));
extern int v90RateReneg(void *obj) __attribute__((weak));
extern int v90RateRenegSilence(void *obj) __attribute__((weak));

#define FRAG	48

/*
 * A root object built by hand rather than by `ref_vpcm_create`, because the
 * point of this binary is that NOTHING of the blob's V.PCM side runs in it.
 * Zeroed is a valid starting state for everything `vpcm_run` reads before it
 * reaches `VPcmV34Progress`: both queues empty, the mute counter clear,
 * `nbits` zero so the bit pipe is not entered, and `mode` and `status` at the
 * values `vpcm_create` leaves at 0x3a9c and 0x3aa3.
 */
static struct vpcm_root root;

/*
 * The session `VPcmV34Progress` reads one byte of before it dispatches --
 * `p3548->byte_7f5c`, the entrance-filter switch at .text+0xb4dc.  Zeroed, so
 * the filter is off; it exists only because the load is unconditional and a
 * null `p3548` would fault before any guard could fire.  0x7f68 is
 * `sizeof(VPcmFloModem)`, spelled as a literal because this is a C file.
 */
static unsigned char session[0x7f68];

/*
 * +0x0000 is `status`, and 4 is the line-verification state -- the arm at
 * .text+0xb9f7 whose one call is the unwritten `qcLineVerification`.
 * +0x0262 is the running flag; zero makes `VPcmV34Progress` return at its
 * first instruction and reach no guard at all.  Both are written through
 * `struct v34_object` rather than by offset, which is what keeps this test
 * honest if either field moves.
 */
#define GUARD_STATUS	4

static void
root_reset(void)
{
	struct v34_object *obj;

	memset(&root, 0, sizeof(root));
	memset(session, 0, sizeof(session));
	root.dp.id = 34;
	root.dp.modem = (void *)0xD1A1u;
	root.dp.dp_data = &root;

	obj = (struct v34_object *)&root.v34;
	obj->status = GUARD_STATUS;
	obj->p3548 = session;
	*(short *)((unsigned char *)obj + 0x262) = 1;
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

	diff_begin("all five VPcmV34* entry points are WRITTEN, and of the "
		   "seven below them four are not");
	/*
	 * ALL FIVE ARE NOW DEFINED, two in `src/pump/v34/v34pcmif.c` and
	 * three in `src/pump/v34/v34pcmmain.cpp`, and this block is where
	 * that is recorded.  It is asserted rather than dropped for the
	 * reason the weak attribute exists at all: the guard surface is the
	 * claim, so it has to be counted in both directions.
	 */
	diff_eq_int("VPcmV34Progress is DEFINED", VPcmV34Progress != 0, 1, 0);
	diff_eq_int("VPcmV34GetCleanedSamples is DEFINED",
		    VPcmV34GetCleanedSamples != 0, 1, 0);
	diff_eq_int("VPcmV34GetCurrentSessionDP is DEFINED",
		    VPcmV34GetCurrentSessionDP != 0, 1, 0);
	diff_eq_int("VPcmV34GetCurrentRxBitRate is DEFINED",
		    VPcmV34GetCurrentRxBitRate != 0, 1, 0);
	diff_eq_int("VPcmV34GetCurrentTxBitRate is DEFINED",
		    VPcmV34GetCurrentTxBitRate != 0, 1, 0);
	/*
	 * And the four that are not.  Without the null ones the abort below
	 * proves nothing: a guard that fired because the symbol was null is
	 * only interesting if the symbol really is null.
	 */
	diff_eq_int("runPcmModem is now DEFINED, so the four are three",
		    _ZN12VPcmFloModem11runPcmModemEPfS0_jPiS1_S1_S1_ != 0, 1,
		    0);
	diff_eq_int("v90RunDemodulator is unresolved",
		    _ZN12VPcmFloModem17v90RunDemodulatorEPfjPiS1_ == 0, 1, 0);
	diff_eq_int("qcLineVerification is unresolved",
		    _ZN12VPcmFloModem18qcLineVerificationEPfS0_jPiS1_S1_S1_
		    == 0, 1, 0);
	diff_eq_int("vPcmResetPhase3Modem is unresolved",
		    _ZN12VPcmFloModem20vPcmResetPhase3ModemEv == 0, 1, 0);
	/*
	 * SIX NOW, BECAUSE ONE OF THE SEVEN HAS BEEN WRITTEN.
	 * `GenericToneDetector::process(float *, unsigned)` is 422 bytes in
	 * `src/dsp/GenericToneDetector.cpp` with its own differential test,
	 * so the weak reference `v34pcmmain.cpp` holds for it now RESOLVES
	 * and the modem-on-hold arm of `VPcmV34Progress` calls the detector
	 * instead of stopping.  The assertion is inverted rather than deleted,
	 * which is this file's whole argument applied in the other direction:
	 * the guard surface is the claim, so a symbol crossing it is counted
	 * on the side it crossed to.  If this ever reads null again, something
	 * has stopped linking `GenericToneDetector.o` and `VPcmV34Progress`
	 * has quietly gone back to the stub.
	 */
	diff_eq_int("GenericToneDetector::process is now DEFINED, so the "
		    "seven are five",
		    _ZN19GenericToneDetector7processEPfj != 0, 1, 0);
	/*
	 * AND NOW THE SIX ARE FOUR.  The same crossing again, twice:
	 * `v90RateReneg` and `v90RateRenegSilence` are 555 and 983 bytes in
	 * `src/pump/v34/v34pcmmain.cpp` with their own differential test
	 * (`t_v90p34.cpp`), so `v34pcmmain.cpp` no longer defines
	 * `DSPLIB_V34HSHAK_UNWRITTEN` at all and the two call sites in
	 * `VPcmV34Progress` are unconditional calls rather than guarded ones.
	 * Counted on the side they crossed to, exactly as the tone detector
	 * above.
	 */
	diff_eq_int("v90RateReneg is now DEFINED", v90RateReneg != 0, 1, 0);
	diff_eq_int("v90RateRenegSilence is now DEFINED",
		    v90RateRenegSilence != 0, 1, 0);
	/* The layout the hand-built root depends on. */
	diff_eq_int("the root is vpcm_create's allocation",
		    (int)sizeof(struct vpcm_root), 0xd258, 0);
	diff_eq_int("...and the V.34 object is the block vpcm_create clears",
		    (int)sizeof(struct v34_object), VPCM_V34_BYTES, 0);
	rc |= diff_end();

	/* --- the guard STOPS ------------------------------------------- */

	diff_begin("an unwritten callee inside VPcmV34Progress aborts, and it "
		   "is watched doing it");
	root_reset();
	fflush(stdout);
	fflush(stderr);
	pid = fork();
	if (pid == 0) {
		/*
		 * No `v34pcm_unwritten_reset` here: this child has NOT said it
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
	diff_eq_int("nothing recorded before the call", v34pcm_unwritten(),
		    V34PCM_WRITTEN, 0);
	v34pcm_unwritten_reset();
	diff_eq_int("...and the reset leaves it that way", v34pcm_unwritten(),
		    V34PCM_WRITTEN, 0);

	memset(out, 0x5a, sizeof(out));
	diff_eq_int("vpcm_run returns DPSTAT_OK in soft mode",
		    vpcm_run(&root.dp, in, out, FRAG), DPSTAT_OK, 0);
	/*
	 * FIRST WINS, and the first this fixture can reach is
	 * `qcLineVerification`: state 4 is the one arm of the seventeen whose
	 * only unwritten call is that member, and the arm it returns into
	 * cannot reach any of the other three.  The remaining three are
	 * recorded here as UNREACHED by this fixture rather than claimed --
	 * each needs a session object this file has no way to build.
	 */
	diff_eq_int("...having recorded the callee it could not reach",
		    v34pcm_unwritten(), V34PCM_UNWRITTEN_QCLINE, 0);
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
	/*
	 * AND VPcmV34Progress ITSELF RAN.  The line-verification arm sets
	 * `f0004` to 10 when the unwritten member "returns" 0, and then walks
	 * the whole block into the echo history at +0x2f58 -- so the progress
	 * word and the history cursor are what say the arm was entered rather
	 * than skipped.  `VPcmV34GetCleanedSamples` is called by `vpcm_run`
	 * immediately afterwards and CLEARS the cursor, which is why the
	 * assertion is on the samples it reported rather than on the field.
	 */
	diff_eq_int("...the line-verification arm set the progress code",
		    ((struct v34_object *)&root.v34)->f0004,
		    VPCM_PROG_SAME_LINE, 0);
	diff_eq_int("...and the echo history took the whole block",
		    ((struct v34_object *)&root.v34)->hist_2f58[FRAG - 1],
		    in[FRAG - 1], 0);
	/*
	 * The eight codes are eight DIFFERENT codes.  Two of them equal would
	 * make an unwritten path report the wrong one for ever, and nothing
	 * else in this tree looks.
	 */
	diff_eq_int("the eight codes are distinct",
		    V34PCM_WRITTEN != V34PCM_UNWRITTEN_RUNPCM
		    && V34PCM_UNWRITTEN_RUNPCM != V34PCM_UNWRITTEN_V90RUN
		    && V34PCM_UNWRITTEN_V90RUN != V34PCM_UNWRITTEN_QCLINE
		    && V34PCM_UNWRITTEN_QCLINE != V34PCM_UNWRITTEN_RESETP3
		    && V34PCM_UNWRITTEN_RESETP3 != V34PCM_UNWRITTEN_TONEPROC
		    && V34PCM_UNWRITTEN_TONEPROC != V34PCM_UNWRITTEN_RRN
		    && V34PCM_UNWRITTEN_RRN != V34PCM_UNWRITTEN_RRNSILENCE, 1,
		    0);
	rc |= diff_end();

	return rc;
}
