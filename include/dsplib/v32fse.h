/*
 * v32fse.h -- V.32/V.32bis' instance of the library equaliser, and the
 * slicers that go with it.
 *
 * `FSEv32_CFG` is an instance of `struct fpm_fse_cfg`, the same way
 * `MRFv32_CFG` is one of `struct fpm_mrf_cfg`: the datapump copies it onto
 * the stack, patches `owner` and `decision`, and hands it to `FPM_FSE_init`.
 *
 * The slicers live at .text 0x80330-0x81600, inside the V.32 translation
 * unit's range, so they are V.32's own code rather than library code.
 */

#ifndef DSPLIB_V32FSE_H
#define DSPLIB_V32FSE_H

#include "dsplib/fpm_fse.h"

/*
 * 103 taps each, T/2 spaced: `FSEv32_ICOFF` is non-zero only at even indices
 * and `FSEv32_QCOFF` only at odd ones.  Both are `.data`, not `.rodata`, in
 * the object -- they are the INITIAL coefficients and the object's own
 * declaration was not const, even though init copies them before adapting.
 */
#define FSEV32_TAPS	103

extern short FSEv32_ICOFF[FSEV32_TAPS];
extern short FSEv32_QCOFF[FSEV32_TAPS];

extern struct fpm_fse_cfg FSEv32_CFG;

/*
 * NOT PART OF THE EQUALISER BLOCK.  These three belong to V.32's carrier
 * recovery (`CRRv32_*`) and are defined here only because `FSEv32_CFG` points
 * at them and nothing else in the tree defines them yet.  Their shapes are
 * measured, not assumed: `clk` is indexed modulo `cfg.clk_mod` (4) and both
 * gain tables by `pll_sel` (0..2), and the object's sizes are 8, 6 and 6.
 */
extern short CRRv32_CLK[4];
extern short CRRv32_PLL_K1[3];
extern short CRRv32_PLL_K2[3];

#endif /* DSPLIB_V32FSE_H */
