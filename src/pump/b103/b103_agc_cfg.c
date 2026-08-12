/*
 * b103_agc_cfg.c -- Bell 103 / V.21: the AGC configuration.
 *
 * Extracted from dsplibs.o .data:0x77f4 (AGCb103_CFG), with the two
 * coefficient objects it points at, .data:0x7810 and .data:0x780c.
 *
 * MIXED STRUCT.  +0x0c and +0x10 carry R_386_32 relocations into .data.
 * Dumped as int16 they read as 30736 and 30732, which look entirely like
 * plausible coefficients and are in fact addresses.  The relocation audit in
 * tools/tabdump.py exists because of this table.
 *
 * ---------------------------------------------------------------------------
 * The smoother coefficients, and a bug that never fires
 *
 * AGC_DEF_ALPHA and AGC_DEF_BETA are two-element arrays: element 0 is a fast
 * "acquisition" pair and element 1 a slow "tracking" pair.  Every AGC config
 * in the blob -- eight of them, across Bell 103, V.23 and the shared default
 * -- points at element **0**, verified by scanning all 723 R_386_32
 * relocations into .data.  Element 1 is present everywhere and selected
 * nowhere.
 *
 * A first-order smoother `y += alpha*y + beta*x` has unity DC gain when
 * alpha + beta == 1.0, i.e. 32768 in Q15.  Element 0 satisfies that exactly
 * everywhere (16384 + 16384).  Element 1 does not:
 *
 *     .data:0x77bc/0x77b8    32604 +   164  = 32768   correct
 *     .data:0x7664/0x7660    30491 +  2277  = 32768   correct (V.32's)
 *     .data:0x7810/0x780c    32604 +  1638  = 34242   1.045
 *     .data:0x778c/0x7788    32604 +  1638  = 34242   1.045
 *     .data:0x7794/0x7790    32604 +  1638  = 34242   1.045
 *
 * TWO TUs got alpha = 32768 - beta right; the other three kept the 0x77b8
 * TU's alpha while changing beta.  This list said 0x7664 carried 32604 and
 * was a fourth broken pair until finding 1621 read the bytes: it is 30491,
 * and V.32's is correct.  A smoother with DC gain 1.045 would make the level estimate
 * climb until it wrapped, and a wrapped (negative) estimate is exactly the
 * case that makes FPM_AGC_agc's shift go negative.  It is unreachable only
 * because nothing selects element 1.  Recorded as D6 in docs/deviations.md.
 */

#include "dsplib/fpm_agc.h"

/* { acquisition, tracking } -- only [0] is ever selected.  See above. */
static const short AGC_DEF_ALPHA[2] = { 16384, 32604 };
static const short AGC_DEF_BETA[2] = { 16384, 1638 };

const struct fpm_agc_cfg AGCb103_CFG_data = {
	.ref_level = 16384,	/* output settles at half this: 8192 */
	.acquire_level = 10,
	.squelch_level = 80,
	.f06 = 1000,
	.f08 = 1,
	.block_len = 36,	/* matches FPM_rms's 1/36 scaling */
	.alpha = AGC_DEF_ALPHA,
	.beta = AGC_DEF_BETA,
	.f14 = 158
};
