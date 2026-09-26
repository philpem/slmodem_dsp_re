/*
 * v22prc.c -- V.22 / V.22bis: the process/timing primitives.
 *
 * The blob's v22prc.c translation unit, recovered from the over-split layer
 * files.  `ld -r` concatenates .text in FILE order, so the run between
 * v22mod.c's last function (`v22_originate`, ending at 0x08bd4f) and
 * v22stc.c's first (`V22FP_control`, 0x08c3b0) is v22prc.c's.  It holds the
 * detector, timing and clamp primitives -- MakeTxData, the Detect_* family,
 * TxNOP, RxClampV22 and ReadGTimer -- which the v22mod.c state machine calls
 * OUT OF LINE: a trivial `ReadGTimer` would have been inlined had it shared
 * the state machine's unit.
 *
 * Functions in the object's own emission order:
 *
 *   MakeTxData          .text 0x08bd50
 *   RxTrained1200       .text 0x08be20
 *   RxTrained2400       .text 0x08be60
 *   Detect_Retrain      .text 0x08bef0
 *   Detect_Rmloop2_ACK  .text 0x08c010
 *   Detect_1s           .text 0x08c0f0
 *   Detect_v22          .text 0x08c1c0
 *   TxNOP               .text 0x08c340
 *   RxClampV22          .text 0x08c370
 *   ReadGTimer          .text 0x08c3a0
 *
 * Every body moved VERBATIM.  `iabs` is v22det.c's file-static helper and
 * travels with its only callers.
 */

#include "dsplib/v22prc.h"
#include "dsplib/v22fp.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/v22data.h"
#include "dsplib/v22det.h"
#include "dsplib/v22_pps.h"

/*
 * The same helper `v22_sre.c` and `fpm_sre.c` use.  The object computes the
 * magnitude in 32 bits and only then truncates it for the comparison, so the
 * absolute value cannot be taken on a `short`.
 */
static int
iabs(int v)
{
	return v < 0 ? -v : v;
}

/*
 * Lay down `*count` symbols of one of five fixed patterns.
 *
 * The object duplicates the countdown loop once per pattern rather than
 * selecting a constant and sharing one loop, and the jump table it dispatches
 * through has five entries in this order.  Written out the same way.
 */
void
MakeTxData(short *out, const short *count, short pattern)
{
	short i;

	switch (pattern) {
	case V22_TXDATA_S1:
		/*
		 * Two symbols per step, so the counter moves by two and the
		 * loop is entered only when it is already non-zero.
		 */
		for (i = *count; i != 0; i = (short)(i - 2)) {
			out[0] = V22_TXDATA_S1_EVEN;
			out[1] = V22_TXDATA_S1_ODD;
			out += 2;
		}
		break;
	case V22_TXDATA_ONES_1200:
		for (i = *count; i != 0; i = (short)(i - 1))
			*out++ = V22_TRAINED_1200_SYMBOL;
		break;
	case V22_TXDATA_ONES_2400:
		for (i = *count; i != 0; i = (short)(i - 1))
			*out++ = V22_TRAINED_2400_SYMBOL;
		break;
	case V22_TXDATA_SYMBOL_2:
		for (i = *count; i != 0; i = (short)(i - 1))
			*out++ = 2;
		break;
	case V22_TXDATA_SYMBOL_10:
		for (i = *count; i != 0; i = (short)(i - 1))
			*out++ = 10;
		break;
	default:
		break;
	}
}
/*
 * True when every one of `*count` symbols is 3.
 *
 * An empty array is trained: the object's first comparison is `0 >= n`, which
 * jumps straight to the `i == n` test with i still zero.  Preserved, and
 * covered by the test, because "no symbols yet" reading as "training
 * complete" is exactly the kind of thing a rewrite quietly changes.
 */
int
RxTrained1200(const short *symbols, const unsigned short *count)
{
	unsigned short n = *count;
	short i = 0;

	if (i < (int)n && symbols[0] == V22_TRAINED_1200_SYMBOL) {
		do {
			i = (short)(i + 1);
			if (i >= (int)n)
				break;
		} while (symbols[i] == V22_TRAINED_1200_SYMBOL);
	}

	return i == (int)n;
}
/*
 * True when MORE THAN seven symbols at the END of the array are 15.
 *
 * Backwards from `*count - 1`, and the run has to reach the end of the array
 * or eight entries, whichever comes first -- the loop stops on a mismatch OR
 * on running out, and both exits land on the same `> 7` test.  So a
 * nine-entry array of 15s passes, and so does an eight-entry one.
 */
int
RxTrained2400(const short *symbols, const unsigned short *count)
{
	unsigned short n = *count;
	short run = 0;
	short i;

	if ((int)run >= (int)n)
		return 0;

	i = (short)(n - 1);
	if (symbols[i] != V22_TRAINED_2400_SYMBOL)
		return 0;

	i = (short)(i - 1);
	for (;;) {
		run = (short)(run + 1);
		if ((int)run >= (int)n)
			break;
		if (symbols[i] != V22_TRAINED_2400_SYMBOL)
			break;
		i = (short)(i - 1);
	}

	return run > V22_TRAINED_2400_RUN;
}
/*
 * A retrain request: the quadrant sequence correlates against a constant 3
 * AND the symbol stream repeats with period two.
 *
 * The two loops are the object's.  The first walks EVERY symbol and builds
 * the energy of the received quadrants; the second walks only the EVEN ones
 * and builds the correlation against the ideal.  The two therefore run over
 * different sets, which is unusual enough to be worth saying out loud -- it
 * is not a transcription slip, and pairing them differently changes the
 * verdict on every input whose odd symbols differ from its even ones.
 */
int
Detect_Retrain(const unsigned short *sym, const unsigned short *count)
{
	unsigned short n = *count;
	short sumsq = 0;	/* sum q*q over all symbols            */
	short corr = 0;		/* sum q*ideal over the even symbols   */
	short energy = 0;	/* sum ideal*ideal, likewise           */
	short run = 0;		/* consecutive period-2 pairs          */
	unsigned short ms = 0;
	short i;

	for (i = 0; i < n; i = (short)(i + 1)) {
		short q = (short)((sym[i] >> V22_DET_QUAD_SHIFT)
				  & V22_DET_QUAD_MASK);

		sumsq = (short)(sumsq + q * q);
	}

	for (i = 0; i < n; i = (short)(i + 2)) {
		short q = (short)((sym[i] >> V22_DET_QUAD_SHIFT)
				  & V22_DET_QUAD_MASK);

		corr = (short)(corr + q * V22_DET_RETRAIN_QUAD);
		energy = (short)(energy + V22_DET_RETRAIN_QUAD
					  * V22_DET_RETRAIN_QUAD);

		/*
		 * The first pair has nothing to compare against, and the last
		 * one is only half present when the count is odd; the object
		 * skips both without disturbing the run.
		 */
		if (i != 0 && i + 1 < n) {
			if (sym[i] == sym[i - 2] && sym[i + 1] == sym[i - 1])
				run = (short)(run + 1);
			else
				run = 0;
		}
	}

	/*
	 * Both conditions are evaluated -- the object computes each with a
	 * `setcc` and ANDs the two, rather than branching on the first.
	 */
	if (((short)iabs(2 * corr - sumsq)
	     >= (short)((energy * V22_DET_THRESH_Q15) >> 15))
	    & (run > V22_DET_RETRAIN_RUN))
		ms = (unsigned short)((n * V22_DET_SYMBOL_MS_Q14) >> 14);

	return ms;
}
/*
 * The RMLOOP2 acknowledgement.  Same correlator, two loops again -- energy
 * and correlation in the second, the received energy alone in the first --
 * and a built-in threshold.
 *
 * The comparison here is `>=` where Detect_1s uses `>`.  Both are single
 * instructions in the object (`jl` against `jle`) and the asymmetry is the
 * original's.
 */
int
Detect_Rmloop2_ACK(const unsigned short *sym, const unsigned short *count,
		   short bps)
{
	unsigned short n = *count;
	short ideal = (short)(bps == V22_DET_BPS_2400 ? V22_DET_RMLOOP2_2400
						     : V22_DET_RMLOOP2_1200);
	short sumsq = 0;
	short corr = 0;
	short energy = 0;
	unsigned short ms = 0;
	short i;

	for (i = 0; i < n; i = (short)(i + 1))
		sumsq = (short)(sumsq + sym[i] * sym[i]);

	for (i = 0; i < n; i = (short)(i + 1)) {
		corr = (short)(corr + sym[i] * ideal);
		energy = (short)(energy + ideal * ideal);
	}

	if ((short)iabs(2 * corr - sumsq)
	    >= (short)((energy * V22_DET_THRESH_Q15) >> 15))
		ms = (unsigned short)((n * V22_DET_SYMBOL_MS_Q14) >> 14);

	return ms;
}
/*
 * Continuous ones.  One loop rather than two, so all three sums run over the
 * same symbols, and the threshold is the caller's.
 */
int
Detect_1s(const unsigned short *sym, const unsigned short *count,
	  short bps, short thresh)
{
	unsigned short n = *count;
	short ideal = (short)(bps == V22_DET_BPS_2400 ? V22_TRAINED_2400_SYMBOL
						     : V22_TRAINED_1200_SYMBOL);
	short sumsq = 0;
	short corr = 0;
	short energy = 0;
	unsigned short ms = 0;
	short i;

	for (i = 0; i < n; i = (short)(i + 1)) {
		sumsq = (short)(sumsq + sym[i] * sym[i]);
		corr = (short)(corr + sym[i] * ideal);
		energy = (short)(energy + ideal * ideal);
	}

	if ((short)iabs(2 * corr - sumsq) > (short)((energy * thresh) >> 15))
		ms = (unsigned short)((n * V22_DET_SYMBOL_MS_Q14) >> 14);

	return ms;
}
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
void
TxNOP(void *modem, void *arg1, short *out, short *count)
{
	short i;

	(void)modem;
	(void)arg1;

	for (i = 0; i <= V22_TX_BLOCK - 1; i++)
		out[i] = 0;

	*count = V22_TX_BLOCK;
}
void
RxClampV22(void *modem, void *arg1, short *out, short *count)
{
	short i;

	(void)modem;
	(void)arg1;

	for (i = 0; i <= V22_CLAMP_BLOCK - 1; i++)
		out[i] = V22_CLAMP_VALUE;

	*count = V22_CLAMP_BLOCK;
}
/*
 * One block of the datapump is 20 ms, so the shared clock is in
 * milliseconds.  The new value is returned, not the old one.
 */
int
ReadGTimer(void *modem)
{
	struct v22fp *v22 = (struct v22fp *)modem;
	int *timer = &v22->hdx->gtimer;

	*timer += 20;
	return *timer;
}
