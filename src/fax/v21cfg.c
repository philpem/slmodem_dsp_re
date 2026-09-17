/*
 * v21cfg.c -- Class 1 fax, V.21 channel: the nine tables `V21RX_create`
 *             references directly.
 *
 * `include/dsplib/v21cfg.h` carries the derivations -- where each element
 * count comes from, why the two AGC coefficient arrays are globals here and
 * file statics in every other modulation, and how the tone-detector bank
 * closes on V.21's own four frequencies.  This file is the bytes.
 *
 * NONE OF THESE NINE CONTAINS A POINTER EXCEPT `AGCv21_CFG`.  Checked with a
 * relocation sweep over each symbol's own byte range, which is the question
 * `relocscan.py --range` does NOT answer -- that one is "who points AT it"
 * (F9050).  The sweep was shown to fire first on `AGCb103_CFG` and
 * `FPM_MTD_CFG`, whose inner relocations F9140 had already measured, because
 * a sweep that reports "none" everywhere is indistinguishable from a sweep
 * that is broken (F134).  Two hits, both in `AGCv21_CFG`, at +0x0c and +0x10.
 *
 * THE VALUES WERE NOT TRANSCRIBED.  They were emitted from `dsplibs.o`'s own
 * bytes, so a typo inside a 360-entry filter is not a failure mode this file
 * has.  `t_v21cfg.c` compares every entry against the blob anyway.
 */

#include "dsplib/v21cfg.h"

#include "dsplib/fpm_agc.h"

/*
 * The AGC smoother's coefficients, `R` at .rodata 0x0a100 and 0x0a0fc.
 *
 * GLOBAL and uniquely named, unlike the six-times-defined `AGC_DEF_ALPHA` and
 * `AGC_DEF_BETA` that F9144 had to reach through their consumer.  They are
 * declared in the header and compared BY NAME in the test.
 *
 * Element 0 and element 1 each sum to 32768, which is unity DC gain in Q15
 * for `y += alpha*y + beta*x`.  Only element 0 is ever selected.
 */
const short AGC_DEF_ALPHA_v21[2] = { 16384, 32604 };
const short AGC_DEF_BETA_v21[2] = { 16384,   164 };

/*
 * `R` at .rodata 0x0a0e4, 24 bytes, and the only table here with relocations
 * inside it: +0x0c and +0x10 hold the two arrays above.  Both targets had to
 * be written before this symbol could be, because the link constraint binds
 * on a STORED POINTER exactly as it does on a call (F8492/F8493).
 *
 * `f16` is 6553 -- 0.2 in Q15 -- which F1621 measured as the value V.32's two
 * configs carry and Bell 103's and V.23's leave at zero.  It is not padding.
 */
const struct fpm_agc_cfg AGCv21_CFG = {
	10000,			/* +0x00 ref_level; output settles at 5000  */
	2,			/* +0x02 acquire_level                      */
	32,			/* +0x04 squelch_level                      */
	1000,			/* +0x06 f06                                */
	1,			/* +0x08 f08                                */
	40,			/* +0x0a block_len                          */
	AGC_DEF_ALPHA_v21,	/* +0x0c alpha                              */
	AGC_DEF_BETA_v21,	/* +0x10 beta                               */
	158,			/* +0x14 f14                                */
	6553			/* +0x16 f16; 0.2 in Q15, see F1621         */
};

/*
 * `D` at .data 0x7ab4, 24 bytes.  The fourth member of `faxcfg.h`'s family
 * and a fourth TYPE -- see the header for the `cmpw` that forces the width of
 * `chan2` and separates it from `struct v29rx_cfg`.
 *
 * Writable in the object even though nothing in the 1.2 MB writes it, which
 * is what the other three tables do too.
 */
struct v21rx_cfg V21RX_CFG = {
	1,			/* +0x00 chan2: default to the answer side  */
	0,			/* +0x02 short_0002                         */
	300,			/* +0x04 bit_rate; V.21's only rate         */
	0,			/* +0x06 short_0006                         */
	60000,			/* +0x08 int_0008; 60000 in all four        */
	0,			/* +0x0c int_000c                           */
	0,			/* +0x10 int_0010                           */
	0			/* +0x14 aux                                */
};

/*
 * The FSK discriminator's lowpass, `R` at .rodata 0x0a088.  Three biquad
 * sections of five shorts: `V21RX_create` writes `fsd.iir_len = 3` at
 * 0x09900d, and 3 * 5 * 2 is the symbol's 30 bytes.  Shared by both channels
 * -- neither arm of the `chan2` test replaces it.
 */
const short V21RX_IIR_LPF[15] = {
	-12333,    445,  28281,   -454,    445, -14208,   4528,  29701,
	 -8164,   4528, -15782,   9514,  30961, -17829,   9514,
};
/*
 * The channel-2 discriminator FIR, `R` at .rodata 0x0a0a6.  Fifteen taps:
 * `V21RX_create` writes `fsd.fir_taps = 15` at 0x098ffc beside whichever of
 * these two it installed, and 15 * 2 is the symbol's 30 bytes.
 *
 * Selected when `V21RX_CFG.chan2` is non-zero, together with `fsd.delay = 3`.
 */
const short V21RX_CHAN2_INTRP[15] = {
	   229,   -274,    341,   -451,    667,  -1282,  16185,   1523,
	  -727,    478,   -356,    283,   -235,    201,   -176,
};
/*
 * The channel-1 discriminator FIR, `R` at .rodata 0x0a0c4.  The same fifteen
 * taps' worth of shape, selected when `chan2` is zero, with `fsd.delay = 5`.
 *
 * Its first and last entries are 0 and its third-from-last is 3, so it is a
 * shorter response padded into the same fifteen-tap slot as channel 2's --
 * which is consistent with channel 1 sitting lower in frequency.  Stated as
 * an observation about the bytes; nothing depends on it.
 */
const short V21RX_CHAN1_INTRP[15] = {
	     0,     -3,     26,   -133,    501,  -1604,   6016,  13751,
	 -3008,   1146,   -401,    112,    -23,      3,      0,
};
/*
 * The receive rate converter's prototype, `R` at .rodata 0x0c000 -- 720
 * bytes, the largest table here.
 *
 * 360 taps: `V21RX_create` writes `mrf.taps = 0x168` at 0x098f50, and 360 * 2
 * is the symbol's size.  It keeps `FPM_MRF_CFG`'s own 9 branches and
 * decimation of 10 rather than patching them, so the block runs 9/10 -- 8000
 * Hz in, 7200 Hz out -- at 40 taps a branch.  V.29's equivalent is the same
 * 9 branches at 30 taps (F9140), so the branch count is the library's and the
 * per-branch length is the modulation's.
 */
const short V21_MRF_FILT[360] = {
	  -149,    -29,    -25,    -16,      0,     22,     49,     82,    117,    154,
	   191,    226,    255,    277,    291,    293,    284,    262,    229,    186,
	   134,     77,     18,    -40,    -93,   -137,   -170,   -189,   -192,   -180,
	  -152,   -111,    -59,      0,     61,    120,    172,    213,    238,    246,
	   235,    205,    157,     95,     23,    -54,   -131,   -200,   -256,   -295,
	  -312,   -306,   -275,   -221,   -147,    -58,     39,    137,    229,    306,
	   363,    394,    395,    365,    304,    218,    110,    -11,   -136,   -255,
	  -360,   -440,   -490,   -502,   -476,   -411,   -311,   -182,    -33,    124,
	   278,    417,    528,    602,    631,    612,    544,    430,    277,     96,
	  -100,   -296,   -477,   -628,   -735,   -788,   -782,   -713,   -585,   -405,
	  -186,     58,    306,    541,    743,    895,    982,    995,    929,    787,
	   576,    311,      9,   -306,   -611,   -881,  -1093,  -1228,  -1271,  -1215,
	 -1060,   -814,   -491,   -114,    290,    690,   1054,   1353,   1558,   1649,
	  1613,   1447,   1157,    759,    279,   -249,   -786,  -1290,  -1719,  -2036,
	 -2208,  -2213,  -2041,  -1695,  -1190,   -557,    163,    919,   1654,   2308,
	  2825,   3152,   3250,   3093,   2671,   1996,   1097,     25,  -1153,  -2358,
	 -3498,  -4478,  -5204,  -5590,  -5563,  -5069,  -4075,  -2575,   -593,   1823,
	  4596,   7627,  10798,  13979,  17035,  19830,  22238,  24150,  25477,  26156,
	 26156,  25477,  24150,  22238,  19830,  17035,  13979,  10798,   7627,   4596,
	  1823,   -593,  -2575,  -4075,  -5069,  -5563,  -5590,  -5204,  -4478,  -3498,
	 -2358,  -1153,     25,   1097,   1996,   2671,   3093,   3250,   3152,   2825,
	  2308,   1654,    919,    163,   -557,  -1190,  -1695,  -2041,  -2213,  -2208,
	 -2036,  -1719,  -1290,   -786,   -249,    279,    759,   1157,   1447,   1613,
	  1649,   1558,   1353,   1054,    690,    290,   -114,   -491,   -814,  -1060,
	 -1215,  -1271,  -1228,  -1093,   -881,   -611,   -306,      9,    311,    576,
	   787,    929,    995,    982,    895,    743,    541,    306,     58,   -186,
	  -405,   -585,   -713,   -782,   -788,   -735,   -628,   -477,   -296,   -100,
	    96,    277,    430,    544,    612,    631,    602,    528,    417,    278,
	   124,    -33,   -182,   -311,   -411,   -476,   -502,   -490,   -440,   -360,
	  -255,   -136,    -11,    110,    218,    304,    365,    395,    394,    363,
	   306,    229,    137,     39,    -58,   -147,   -221,   -275,   -306,   -312,
	  -295,   -256,   -200,   -131,    -54,     23,     95,    157,    205,    235,
	   246,    238,    213,    172,    120,     61,      0,    -59,   -111,   -152,
	  -180,   -192,   -189,   -170,   -137,    -93,    -40,     18,     77,    134,
	   186,    229,    262,    284,    293,    291,    277,    255,    226,    191,
	   154,    117,     82,     49,     22,      0,    -16,    -25,    -29,   -149,
};
/*
 * The channel-1 tone detector's coefficients, `D` at .data 0x7a74.
 *
 * Two resonator sections of five shorts in Q14, at 980 Hz and 1180 Hz --
 * V.21 channel 1's mark and space.  All ten entries are reproduced exactly by
 * rounding { -r^2, 1.0, 2*r*cos(w), -2*cos(w), 1.0 } with r = 0.9 and
 * w = 2*pi*f/8000; see the header, and F9350 for the measurement over both
 * banks at once.
 *
 * `V21_CHAN2_MTD_COEFF` is its channel-2 twin and lives in `faxcfg.c`,
 * because all three fax receivers reference that one and only V.21 references
 * this one.
 */
short V21_CHAN1_MTD_COEFF[10] = {
	-13271,  16384,  21178, -23532,  16384,
	-13271,  16384,  17707, -19675,  16384,
};

/*
 * `D` at .data 0x07af8, 28 bytes.  `V21TX_create`'s own default; see the
 * header for the derivation and for why the SHORT/INT split is the
 * receiver's precedent rather than something this table's own instructions
 * force.
 */
struct v21tx_cfg V21TX_CFG = {
	1,		/* +0x00 protocol                           */
	300,		/* +0x02 bit_rate                           */
	0,		/* +0x04 short_0004                         */
	0,		/* +0x06 short_0006                         */
	60000,		/* +0x08 int_0008                           */
	3200,		/* +0x0c int_000c                           */
	0,		/* +0x10 flags                              */
	0,		/* +0x14 int_0014                           */
	0		/* +0x18 int_0018                           */
};
