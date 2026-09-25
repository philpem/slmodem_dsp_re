/*
 * v22data.c -- V.22 / V.22bis: the datapump's small data-path leaves.
 *
 * Reconstructed from dsplibs.o:
 *
 *   Detect_v22          .text 0x08c1c0  375   the 0x8bd50..0x8c5a0 block,
 *                                             with the rest of the Detect_*
 *                                             family and v22prc.c's TxNOP
 *
 * ScrambleDataV22, DescrambleDataV22 and ModDataV22 are NOT here: the object
 * puts them in V22int.c, and they moved there in the TU reconciliation.  The
 * evidence below is kept because it is about the object, not about this file.
 *
 * Kept here because it is one layer and not one translation unit: Detect_v22
 * is a wrapper that picks a sub-object out of the datapump instance and calls
 * the module that owns it.  `tools/tumap.py` puts thirteen V.22 translation
 * units in one bracket and cannot say which of them this address block is.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE POINTER IS READ AGAIN AFTER EVERY CALL, AND THAT IS THE SOURCE
 *
 * `ModDataV22` reloads `0x54(%esi)` between its two calls, and `Detect_v22`
 * reloads `0x50(%edx)` between each pair of detector calls, with the instance
 * pointer itself live in a callee-saved register the whole time.  A local
 * holding the sub-object pointer across a call would have been SPILLED and
 * reloaded from the stack -- `mov 0x10(%esp),%eax`, not `mov 0x54(%esi),%eax`
 * -- so the original reads the field once per use.  Written that way here.
 * It is not observable through any call the object can make (no callee writes
 * either field), but it is what the compiler was forced to encode.
 */

#include "dsplib/v22data.h"
#include "dsplib/v22fp.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v22_pps.h"

/*
 * One block of the receive path's front end: gain-control 160 samples, then
 * run two tone detectors over four consecutive 40-sample sub-blocks of the
 * same buffer, and report on how long either has been quiet.
 *
 * `data` IS AN OUTPUT AS WELL AS AN INPUT.  The AGC scales it in place, and
 * each sub-block is then zeroed unless the FIRST detector has already
 * returned zero twice running.  `fpm_mtd.h` glosses a zero verdict as
 * FPM_MTD_ABSENT -- there is signal, but not in this detector's band -- so
 * the run advances only while SOMETHING is on the line that is not what
 * detector A watches; silence answers NOSIGNAL, which is non-zero and resets
 * the run instead.
 *
 * Measured through t_v22data.c's fixture, `MTDv22_CFG`'s detector answers
 * zero at 2200 Hz and at no other frequency between 100 and 3900, so on that
 * bank the mute lifts while a 2200 Hz tone is present.  What either detector
 * is FOR is not established and is not claimed: the coefficient bank belongs
 * to the shared object, which nothing reconstructed builds.
 *
 * ---------------------------------------------------------------------------
 * THE TWO COUNTERS ARE NOT INTERCHANGEABLE, THOUGH THE RETURN CANNOT SEE IT
 *
 * The result is `(run_a > 2) | (run_b > 2)`, which is symmetric: pairing each
 * counter with the other detector returns the same value on every input.  The
 * asymmetry is entirely in the two side effects -- the sub-block clear is
 * gated on run_a and the diagnostic on run_b -- which is why t_v22data.c
 * compares the whole sample buffer and the debug transcript rather than the
 * return alone.
 *
 * ---------------------------------------------------------------------------
 * THE CLEAR TEST, WHICH IS WRITTEN AS THE OBJECT LEAVES IT AMBIGUOUS
 *
 * On the detected path the object sets run_a to 0 and falls straight into the
 * clear with no comparison; on the quiet path it increments and tests.  Those
 * are the two arms of one `run_a <= 1` whose first arm the compiler folded,
 * and they are written that way below.  An `if (va) { run_a = 0; clear; } else
 * if (++run_a <= 1) clear;` compiles to the same thing and means the same
 * thing.
 *
 * The counters are 16 bits: `inc %eax; cwtl` on each, and `cmpw` against the
 * threshold.  Four iterations cannot take either past 4, so `short` and `int`
 * agree over everything reachable; `short` is what the object encodes.
 */
int
Detect_v22(void *modem, short *data)
{
	struct v22fp *v22 = (struct v22fp *)modem;
	short run_a = 0;	/* consecutive sub-blocks A returned zero on */
	short run_b = 0;	/* the same for B                            */
	short i, j;

	/*
	 * The whole block through the second AGC.  The object passes a FOURTH
	 * argument, the constant 1, which FPM_AGC_agc does not have -- the
	 * same extra argument `bwchdem.c` records at its own call site, and
	 * ignored in the same way.  Unlike bwchdem this caller discards the
	 * return as well, so there is nothing to read back out of the state.
	 */
	FPM_AGC_agc(&v22->dsp->agc2, data, V22_DETECT_BLOCK);

	for (i = 0; i < V22_DETECT_SUBBLOCKS; i++) {
		short *chunk = data + (int)i * V22_DETECT_SUBBLOCK;
		short va, vb;

		va = FPM_MTD_detect(v22->hdx->mtd,
				    chunk, V22_DETECT_SUBBLOCK);
		vb = FPM_MTD_detect(v22->hdx->mtd2,
				    chunk, V22_DETECT_SUBBLOCK);

		if (vb != 0)
			run_b = 0;
		else
			run_b = (short)(run_b + 1);

		if (va != 0)
			run_a = 0;
		else
			run_a = (short)(run_a + 1);

		if (run_a <= V22_DETECT_CLEAR_HOLD)
			for (j = 0; j <= V22_DETECT_SUBBLOCK - 1; j++)
				chunk[j] = 0;
	}

	/* The author's own words, and gated on the SECOND counter. */
	if (run_b > V22_DETECT_THRESHOLD && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V22: Detect_V22 OK!!!\n");

	return (run_a > V22_DETECT_THRESHOLD) | (run_b > V22_DETECT_THRESHOLD);
}
/*
 * ScrambleDataV22, DescrambleDataV22 and ModDataV22 are V22int.c's and now
 * live in src/pump/v22/V22int.c.  What remains here is Detect_v22, which the
 * object puts in another V.22 unit.
 */
