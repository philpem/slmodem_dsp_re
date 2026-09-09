/*
 * t_vpcmguard.c -- the unwritten boundary, and THE BOUNDARY IS NOW EMPTY.
 *
 * ===========================================================================
 * WHAT CHANGED, AND WHY TWO OF THIS FILE'S THREE GROUPS ARE GONE
 * ===========================================================================
 *
 * This file watched a guard STOP.  `VPcmV34Progress` called seven symbols
 * nobody had reconstructed and carried a weak-reference-plus-guard for each;
 * this binary put the object into the line-verification state, whose only
 * unwritten callee was `VPcmFloModem::qcLineVerification`, forked, and
 * required the child to have died of SIGABRT.
 *
 * `qcLineVerification` and `vPcmResetPhase3Modem` were the last two, and both
 * are now in src/pump/v90/VpcmFloModem.cpp.  **All seven are written**, so
 * every guard below `VPcmV34Progress` is unreachable and there is nothing
 * left in this tree for a fork to watch abort.  The two groups that drove one
 * are therefore deleted rather than weakened -- a group whose premise has
 * ceased to hold does not become a better test by being made to pass.
 *
 * WHAT SURVIVES IS THE CLAIM THAT MATTERS, and it survives in both
 * directions.  Every one of the twelve symbols is asserted PRESENT, through a
 * WEAK declaration so that the comparison is a comparison; and the soft path
 * is still driven end to end, with `v34pcm_unwritten()` required to report
 * `V34PCM_WRITTEN` afterwards.  That second assertion is the whole of the old
 * watch turned the right way up: if any of the seven ever stops being linked,
 * its guard fires, the recorder takes its code, and this reads non-WRITTEN.
 *
 * `v34pcmmain.cpp` keeps the machinery -- four `v34pcm_notwritten` call sites,
 * the recorder and the abort -- and it is now dead code.  Retiring it is a
 * separate change to a file this batch had no other reason to touch, and it
 * would take the recorder with it; finding F7606.
 *
 * ===========================================================================
 * THE SESSION HAS TO SURVIVE A REAL CALL NOW
 * ===========================================================================
 *
 * The third group used to reach a guard that returned 0 without touching the
 * hand-built session.  It now reaches `qcLineVerification` itself, which
 * dereferences two pointers before it does anything else: `V90Modem::progress`
 * through `modem` and then `modem.demodulator->word_3c`.  So the session is
 * given a `V90ModemSide` of 2 -- outside {0, 1}, the arm that fans out to
 * neither half and only prints -- and a real (zeroed) block for the
 * demodulator to point at.  With `word_3c` zero the dispatch takes its
 * default, `qcVerifyState` is zero so the silence half runs, and the member
 * returns 0, which is the same 0 the guard used to return.  Every assertion
 * below it is therefore unchanged, including `progress`.
 *
 * That is t_v90rundemod.cpp's `side = 2` device for the same reason: keep the
 * real demodulator out of a binary that is not about it.
 *
 * ---------------------------------------------------------------------------
 * The historical note follows, because it is still the argument for the
 * assertions that remain.
 * ---------------------------------------------------------------------------
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
 * THE SEVEN ONE LEVEL DOWN went the same way, one and two at a time:
 * `GenericToneDetector::process(float *, unsigned)` at 422 bytes, then
 * `v90RateReneg` and `v90RateRenegSilence` at 555 and 983 in
 * `src/pump/v34/v34pcmmain.cpp`, then `runPcmModem` and `v90RunDemodulator`,
 * and finally `qcLineVerification` and `vPcmResetPhase3Modem`.  The tone
 * detector's weak reference resolves; the two transmitters have no weak
 * reference left at all, because the file that called them defines them, so
 * their guards are gone rather than satisfied.
 *
 * NONE OF THEM IS ON A V.34 CALL, which is finding F1454's measurement and the
 * reason `t_vpcmrun`'s four-way comparison of a real 33,600 connect passed
 * with all seven absent -- and still passes now that one of them is present.
 * The guard is what stands between "a path this tree cannot take" and a call
 * through a null pointer.
 *
 * WHY IT HAD TO BE WATCHED RATHER THAN REASONED ABOUT.  gates.md's pattern:
 * a guard that silently returned would leave a `.process` running and
 * carrying nothing, and its output -- a buffer of silence -- is exactly what
 * a modem that had correctly transmitted nothing produces.  Nothing about the
 * result distinguishes the two.  So the claim was made the only way it could
 * be: fork, call it, and require the child to have died of SIGABRT.  That is
 * the argument the surviving `V34PCM_WRITTEN` assertion inherits.
 *
 * The soft half is `V34hshak.c`'s rule, and finding F547's argument: a test
 * that dies cannot then be asked WHICH path it took, so the stop is what a
 * test opts out of BY NAME -- `v34pcm_unwritten_reset` -- and the code is
 * always recorded either way.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 * it for the same reason and finding F985 records the trap.
 */
#define DSPLIB_VPCM_UNWRITTEN	__attribute__((weak))
#include "dsplib/vpcm.h"

/*
 * THE SEVEN, BY THEIR LINK NAMES -- all seven defined now, declared here so
 * that every crossing is asserted.
 *
 * Five of them are C++ members and their mangled names are ordinary C
 * identifiers, so a C file can name them directly and does -- declaring the
 * class here would drag `VPcmFloModem.h` into a `.c`, and the point is the
 * SYMBOL rather than the signature.  The parameter lists are deliberately
 * empty: nothing here calls any of them.
 *
 * Weak for the reason above.  None of the seven is undefined any more, so the
 * attribute no longer saves a link -- what it still does is stop GCC folding
 * `f != 0` to true at compile time, which would make all seven assertions
 * vacuous and leave a symbol dropping out of the build undetected.  That is
 * the whole remaining point of this file, so the attribute stays.
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
 * What `modem.demodulator` points at.  `qcLineVerification` reads exactly one
 * word of it -- `word_3c`, the demodulator's event code -- and zero is its
 * default arm.  0x100 is comfortably more than the one field and is not a
 * claim about `sizeof(V90Demodulator)`; nothing here depends on the size.
 */
static unsigned char demod[0x100];

/*
 * Three offsets inside the session, spelled as literals because this is a C
 * file and the classes are C++.  `VPcmFloModem` embeds a `V90Modem` at
 * +0x1758 (include/dsplib/VPcmFloModem.h); that object's `demodulator` is its
 * +0x04 and its `side` is its +0x49bc, so 0x175c and 0x6114 here.  Both are
 * asserted against the object map by t_vpcmflomodem.cpp's offset checks,
 * which is what keeps these literals honest.
 */
#define SESS_DEMODULATOR	0x175c
#define SESS_MODEM_SIDE		0x6114

/* Outside {0, 1}: V90Modem::progress fans out to neither half. */
#define V90_SIDE_ILLEGAL	2

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
	memset(demod, 0, sizeof(demod));
	root.dp.id = 34;
	root.dp.modem = (void *)0xD1A1u;
	root.dp.dp_data = &root;

	obj = (struct v34_object *)&root.v34;
	obj->status = GUARD_STATUS;
	obj->p3548 = session;
	*(short *)((unsigned char *)obj + 0x262) = 1;

	/* See the file comment: the line-verification arm is a real call now. */
	*(void **)(session + SESS_DEMODULATOR) = demod;
	*(unsigned int *)(session + SESS_MODEM_SIDE) = V90_SIDE_ILLEGAL;
}

int
main(void)
{
	short in[FRAG], out[FRAG];
	int rc = 0;
	int i;

	for (i = 0; i < FRAG; i++)
		in[i] = (short)(i * 37 - 500);

	diff_begin("all five VPcmV34* entry points are WRITTEN, and so are "
		   "all seven below them");
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
	/*
	 * AND `v90RunDemodulator` IS DEFINED TOO, so the three are two.  It
	 * is `VPcmFloModem`'s other entry point (.text+0xd860, 3,013 bytes),
	 * reconstructed in src/pump/v90/VpcmFloModem.cpp with
	 * test/unit/t_v90rundemod.cpp against the blob; finding F7580.  The
	 * assertion is INVERTED rather than deleted, for the same reason the
	 * five above are asserted at all: the guard surface is the claim, and
	 * a symbol silently dropping out of it is exactly what this file
	 * exists to notice.
	 */
	diff_eq_int("v90RunDemodulator is now DEFINED, so the three are two",
		    _ZN12VPcmFloModem17v90RunDemodulatorEPfjPiS1_ != 0, 1, 0);
	/*
	 * AND THESE TWO CLOSE THE SET.  `qcLineVerification` (.text+0xf750,
	 * 779 bytes) and `vPcmResetPhase3Modem` (.text+0xf200, 149 bytes) are
	 * reconstructed in src/pump/v90/VpcmFloModem.cpp; findings F7603 and
	 * F7604.  Both assertions are INVERTED rather than deleted, exactly as
	 * `v90RunDemodulator`'s was: the guard surface is the claim, and a
	 * symbol silently dropping out of it is what this file notices.  With
	 * these two the boundary below `VPcmV34Progress` is EMPTY, which is
	 * why the two groups that used to follow are gone -- see the head of
	 * this file.
	 */
	diff_eq_int("qcLineVerification is now DEFINED",
		    _ZN12VPcmFloModem18qcLineVerificationEPfS0_jPiS1_S1_S1_
		    != 0, 1, 0);
	diff_eq_int("vPcmResetPhase3Modem is now DEFINED, so the boundary is "
		    "empty",
		    _ZN12VPcmFloModem20vPcmResetPhase3ModemEv != 0, 1, 0);
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

	/*
	 * THE GROUP THAT WATCHED THE GUARD STOP IS GONE, and so is the one
	 * that then asked it which callee it had stopped on.  Both forked,
	 * called `vpcm_run` with `qcLineVerification` unresolved, and required
	 * SIGABRT; there is no unresolved callee left below `VPcmV34Progress`
	 * for either to reach, and a test whose premise has ceased to hold
	 * does not improve by being made to pass.  The head of this file has
	 * the argument and finding F7606 the disposition.
	 */

	/* --- and the whole path still runs ----------------------------- */

	diff_begin("VPcmV34Progress runs the line-verification arm end to "
		   "end, and no guard fires");
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
	 * AND NOTHING WAS RECORDED, WHICH IS THE WHOLE WATCH TURNED THE RIGHT
	 * WAY UP.  State 4 used to be the one arm of the seventeen whose only
	 * unwritten call was `qcLineVerification`, and this assertion used to
	 * read `V34PCM_UNWRITTEN_QCLINE`.  The member is written, so the arm
	 * calls it and the recorder stays at `V34PCM_WRITTEN`.  If any of the
	 * seven ever stops being linked, its guard fires on this very call,
	 * the recorder takes its code, and this fails -- which is what the
	 * SIGABRT group used to be for and is the only part of it that can
	 * still be true.
	 *
	 * `v34pcm_unwritten_reset` above is what puts the recorder in SOFT
	 * mode, so a guard that did fire would be recorded rather than
	 * aborting; that is finding F547's rule and is why this assertion can
	 * exist at all.
	 */
	diff_eq_int("...and no guard fired: nothing was recorded",
		    v34pcm_unwritten(), V34PCM_WRITTEN, 0);
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
	 * `progress` to 10 when the unwritten member "returns" 0, and then walks
	 * the whole block into the echo history at +0x2f58 -- so the progress
	 * word and the history cursor are what say the arm was entered rather
	 * than skipped.  `VPcmV34GetCleanedSamples` is called by `vpcm_run`
	 * immediately afterwards and CLEARS the cursor, which is why the
	 * assertion is on the samples it reported rather than on the field.
	 */
	diff_eq_int("...the line-verification arm set the progress code",
		    ((struct v34_object *)&root.v34)->progress,
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
