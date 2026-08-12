/*
 * v32fse_tables.c -- V.32's equaliser coefficients and configuration.
 *
 * Reconstructed from dsplibs.o:
 *   FSEv32_ICOFF   .data 0x073a0   206
 *   FSEv32_QCOFF   .data 0x072c0   206
 *   FSEv32_CFG     .data 0x07480    56
 *   CRRv32_CLK     .data 0x074b8     8   (carrier recovery, see the header)
 *   CRRv32_PLL_K2  .data 0x074c0     6
 *   CRRv32_PLL_K1  .data 0x074c6     6
 *
 * The element width is the consumers': `FPM_FSE_init` copies the two
 * coefficient arrays with a 2-byte stride, `FPM_FSE_receive` walks them with
 * `movswl (%esi); add $0x2,%esi`, and it indexes `clk`, `pll_k1` and `pll_k2`
 * with `movswl (%reg,%idx,2)`.  All six are shorts.
 */

#include "dsplib/v32fse.h"

/*
 * Antisymmetric about tap 51 and non-zero only at even taps: the in-phase
 * half of a T/2-spaced analytic bandpass filter.
 */
short FSEv32_ICOFF[FSEV32_TAPS] = {
	   -5,     0,    -2,     0,     2,     0,     6,     0,
	   10,     0,    10,     0,     9,     0,     3,     0,
	   -4,     0,   -11,     0,   -19,     0,   -20,     0,
	  -18,     0,    -7,     0,     9,     0,    28,     0,
	   51,     0,    57,     0,    61,     0,    26,     0,
	  -43,     0,  -142,     0,  -415,     0,  -582,     0,
	-2129,     0,-16043,     0, 16043,     0,  2129,     0,
	  582,     0,   415,     0,   142,     0,    43,     0,
	  -26,     0,   -61,     0,   -57,     0,   -51,     0,
	  -28,     0,    -9,     0,     7,     0,    18,     0,
	   20,     0,    19,     0,    11,     0,     4,     0,
	   -3,     0,    -9,     0,   -10,     0,   -10,     0,
	   -6,     0,    -2,     0,     2,     0,     5
};

/* Symmetric about tap 51 and non-zero only at odd taps: the quadrature half. */
short FSEv32_QCOFF[FSEV32_TAPS] = {
	    0,     6,     0,     8,     0,     7,     0,     4,
	    0,     0,     0,    -6,     0,   -11,     0,   -14,
	    0,   -14,     0,    -8,     0,    -1,     0,    12,
	    0,    23,     0,    31,     0,    33,     0,    21,
	    0,     3,     0,   -39,     0,   -77,     0,  -126,
	    0,  -167,     0,  -112,     0,   -86,     0,   863,
	    0,  3710,     0,-22796,     0,  3710,     0,   863,
	    0,   -86,     0,  -112,     0,  -167,     0,  -126,
	    0,   -77,     0,   -39,     0,     3,     0,    21,
	    0,    33,     0,    31,     0,    23,     0,    12,
	    0,    -1,     0,    -8,     0,   -14,     0,   -14,
	    0,   -11,     0,    -6,     0,     0,     0,     4,
	    0,     7,     0,     8,     0,     6,     0
};

/*
 * Carrier recovery's tables.  `CRRv32_CLK` is a quarter-cycle per entry
 * (0x8000 is a full cycle in the phasor's units), which is the nominal
 * carrier advance over one of the four clock phases.  The two gain tables are
 * indexed by the error band: 0 while training, then 1 for a large error and
 * 2 for a small one -- so the loop is widest during acquisition (3050) and
 * tightest when locked (766, with an integral term of 1).
 */
short CRRv32_CLK[4] = { 0, 8192, 16384, 24576 };
short CRRv32_PLL_K1[3] = { 602, 3050, 766 };
short CRRv32_PLL_K2[3] = { 0, 18, 1 };

/*
 * 144 samples per call at 3 per symbol is 48 symbols; 1500 symbols of wide
 * tracking before the band selection starts; the two error thresholds are
 * 6536 and 1638 out of the 0x4000 half-cycle the error is wrapped into.
 *
 * `owner` and `decision` are zero here and patched by the datapump.
 */
struct fpm_fse_cfg FSEv32_CFG = {
	144,			/* block     */
	3,			/* interp    */
	FSEv32_ICOFF,		/* icoff     */
	FSEv32_QCOFF,		/* qcoff     */
	FSEV32_TAPS,		/* taps      */
	{ 3243, 865, 0 },	/* mu        */
	CRRv32_CLK,		/* clk       */
	4,			/* clk_mod   */
	1,			/* clk_inc   */
	1500,			/* train_sym */
	6536,			/* err_hi    */
	1638,			/* err_lo    */
	0,			/* pad22     */
	CRRv32_PLL_K1,		/* pll_k1    */
	CRRv32_PLL_K2,		/* pll_k2    */
	0,			/* owner     */
	0,			/* decision  */
	0			/* reserved34 */
};
