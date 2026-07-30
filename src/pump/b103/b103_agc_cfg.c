/*
 * b103_agc_cfg.c -- Bell 103's AGC configuration.
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
 *     .data:0x7810/0x780c    32604 +  1638  = 34242   1.045
 *     .data:0x778c/0x7788    32604 +  1638  = 34242   1.045
 *     .data:0x7794/0x7790    32604 +  1638  = 34242   1.045
 *     .data:0x7664/0x7660    32604 +  2277  = 34881   1.064
 *
 * One TU got alpha = 32768 - beta right; the rest kept that TU's alpha while
 * changing beta.  A smoother with DC gain 1.045 would make the level estimate
 * climb until it wrapped, and a wrapped (negative) estimate is exactly the
 * case that makes FPM_AGC_agc's shift go negative.  It is unreachable only
 * because nothing selects element 1.  Recorded as D6 in docs/deviations.md.
 */

#include "dsplib/fpm_agc.h"

/* { acquisition, tracking } -- only [0] is ever selected.  See above. */
static const short AGC_DEF_ALPHA[2] = { 16384, 32604 };
static const short AGC_DEF_BETA[2] = { 16384, 1638 };

const struct fpm_agc_cfg AGCb103_CFG = {
	16384,		/* +0x00 ref_level: output settles at 8192      */
	10,		/* +0x02 acquire_level                          */
	80,		/* +0x04 squelch_level                          */
	1000,		/* +0x06 f06, not read by fpm_agc               */
	1,		/* +0x08 f08, not read by fpm_agc               */
	36,		/* +0x0a block_len -- matches FPM_rms's 1/36    */
	AGC_DEF_ALPHA,	/* +0x0c                                        */
	AGC_DEF_BETA,	/* +0x10                                        */
	158,		/* +0x14 f14, not read by fpm_agc               */
	0		/* +0x16 pad                                    */
};
