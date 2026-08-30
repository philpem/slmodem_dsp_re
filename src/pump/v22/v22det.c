/*
 * v22det.c -- V.22 / V.22bis: transmit patterns and received-pattern
 * detection.  See include/dsplib/v22det.h for what these are and for the
 * sixteen-bit wrap that decides every verdict.
 *
 * Written in the object's own order -- MakeTxData 0x08bd50, Detect_Retrain
 * 0x08bef0, Detect_Rmloop2_ACK 0x08c010, Detect_1s 0x08c0f0 -- because
 * emission order is a register-allocation carrier (finding F7796) and there
 * is no reason to spend it.
 */

#include "dsplib/v22det.h"

#include "dsplib/v22prc.h"

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
