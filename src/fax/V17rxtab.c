/*
 * v17cfg.c -- ITU-T V.17 (fax): the receiver's coefficient, gain and
 *             constellation tables.
 *
 * Reconstructed from dsplibs.o:
 *
 *   FSEv17_QCOFF     .rodata 0x009780    98
 *   FSEv17_ICOFF     .rodata 0x009800    98
 *   SREv17_COFFS     .rodata 0x0099c0   362
 *   SREv17_XB_COFFS  .rodata 0x009b2a    22
 *   MRFv17_COFFS     .rodata 0x009b40   720
 *   AGCv17_CFG       .rodata 0x009e10    24
 *   V17_MTD_COEFF    .data   0x007940    20
 *   CRRv17_CLK       .data   0x0079c8     8
 *   CRRv17_PLL_K1_S  .data   0x0079d0     6
 *   CRRv17_PLL_K2    .data   0x0079d6     6
 *   CRRv17_PLL_K1    .data   0x0079dc     6
 *   SREv17_PLL_K1_S  .data   0x0079e2     6
 *   SREv17_PLL_K2    .data   0x0079e8     6
 *   SREv17_PLL_K1    .data   0x0079ee     6
 *   SREv17_yCLOCK    .data   0x0079f4     6
 *   SREv17_xCLOCK    .data   0x0079fa     6
 *
 * plus the two file-static Q15 smoother pairs `AGCv17_CFG` points at, at
 * .rodata 0x9e2c and 0x9e28.  Read `v17cfg.h` for what types each of these
 * and why the byte count alone does not; this file carries the bytes.
 *
 * ORDER.  Definitions are in the object's own address order within each
 * section, .rodata before .data.  It is a guess about the emitting
 * translation unit's layout and nothing rests on it -- a data symbol's bytes
 * do not depend on where its definition sits -- but where the object offers
 * an order for free it is cheaper to keep it than to invent one.  The one
 * departure is forced by C: the two file-static smoother pairs sit AFTER
 * `AGCv17_CFG` in the object and must be defined BEFORE it here.
 *
 * THE TRANSLATION UNIT IS NOT SETTLED.  These sit in the same runs of `.data`
 * and `.rodata` as `V17RX_CFG`, so the author's file was probably the one
 * holding `V17RX_create` itself.  They are here rather than in `src/fax/v17.c`
 * because that module is not written and a table is worth nothing until
 * something can link against it.  Moving them later is free.  This is
 * `faxcfg.c`'s D1080 and `v29cfg.c`'s D1100 again, and it is recorded as
 * D1110.
 *
 * NOTHING HERE IS A GENERATOR.  `CRRv17_CLK` has a closed form and the header
 * states it, but `docs/fastpass.md` defers coefficient derivations to the
 * 8 kHz retarget: a byte-exact copy is byte-exact, and the differential test
 * proves it with no derivation at all.  The derivation is recorded as
 * EVIDENCE FOR THE ELEMENT TYPE, which is the thing a byte copy cannot give.
 */

#include "dsplib/v17cfg.h"
#include "dsplib/fpm_agc.h"

/* ------------------------------------------------------------------ .rodata */

/*
 * The equaliser's initial coefficients, 49 taps in each rail --
 * `fse.taps = 0x31` is what `V17RX_create` writes beside them, and 98 bytes
 * is 49 shorts.
 *
 * THE TWO RAILS TILE THE TAPS.  The Q rail is zero at every EVEN index and
 * the I rail at every ODD one, so between them they cover all 49 taps and
 * never overlap.  The I rail is symmetric about tap 24 and the Q rail
 * antisymmetric about it, which is the I/Q pair of one passband filter.  That
 * is a different pattern from V.29's, whose rails are zero every THIRD tap,
 * and the difference is the object's: both receivers run three samples per
 * symbol, so the spacing here is not the interpolation factor.
 *
 * These two are used only when the caller supplies no coefficient set of its
 * own; see `v17cfg.h` for the switch that chooses.
 */
const short FSEv17_QCOFF[49] = {
	     0,    111,      0,     25,      0,    -60,      0,   -166,
	     0,   -366,      0,   -546,      0,   -274,      0,    814,
	     0,   2271,      0,   2475,      0,  -1134,      0, -18308,
	     0,  18308,      0,   1134,      0,  -2475,      0,  -2271,
	     0,   -814,      0,    274,      0,    546,      0,    366,
	     0,    166,      0,     60,      0,    -25,      0,   -111,
	     0,
};

const short FSEv17_ICOFF[49] = {
	   -63,      0,   -144,      0,   -168,      0,   -171,      0,
	  -154,      0,     47,      0,    590,      0,   1191,      0,
	   957,      0,  -1005,      0,  -4549,      0,  -8105,      0,
	 22751,      0,  -8105,      0,  -4549,      0,  -1005,      0,
	   957,      0,   1191,      0,    590,      0,     47,      0,
	  -154,      0,   -171,      0,   -168,      0,   -144,      0,
	   -63,
};

/*
 * The symbol-timing recovery prototype.  `sre.coeffs` is set to 180, ten
 * polyphase branches of eighteen taps, and `cfg.proto` must hold ONE MORE
 * than that because the interpolator reads `proto[i+1]` at `i == coeffs-1`.
 * 181 shorts is 362 bytes, which is the symbol's size.
 *
 * The palindrome runs over all 181 entries, centre value at index 90 -- so
 * the extra entry is not an appendix bolted onto a symmetric 180, it is the
 * centre-and-both-halves of a 181-tap linear-phase design.
 */
const short SREv17_COFFS[181] = {
	    45,     62,     75,     82,     81,     72,     52,     24,
	   -10,    -50,    -89,   -126,   -153,   -169,   -169,   -152,
	  -116,    -64,      0,     72,    146,    213,    266,    298,
	   303,    278,    223,    139,     33,    -86,   -210,   -325,
	  -419,   -480,   -499,   -470,   -392,   -267,   -105,     82,
	   279,    467,    625,    735,    783,    758,    656,    480,
	   241,    -42,   -347,   -645,   -907,  -1102,  -1207,  -1202,
	 -1079,   -838,   -492,    -65,    408,    888,   1327,   1679,
	  1902,   1959,   1829,   1503,    992,    324,   -456,  -1288,
	 -2099,  -2808,  -3335,  -3604,  -3551,  -3128,  -2310,  -1096,
	   487,   2388,   4528,   6812,   9126,  11352,  13371,  15074,
	 16364,  17168,  17442,  17168,  16364,  15074,  13371,  11352,
	  9126,   6812,   4528,   2388,    487,  -1096,  -2310,  -3128,
	 -3551,  -3604,  -3335,  -2808,  -2099,  -1288,   -456,    324,
	   992,   1503,   1829,   1959,   1902,   1679,   1327,    888,
	   408,    -65,   -492,   -838,  -1079,  -1202,  -1207,  -1102,
	  -907,   -645,   -347,    -42,    241,    480,    656,    758,
	   783,    735,    625,    467,    279,     82,   -105,   -267,
	  -392,   -470,   -499,   -480,   -419,   -325,   -210,    -86,
	    33,    139,    223,    278,    303,    298,    266,    213,
	   146,     72,      0,    -64,   -116,   -152,   -169,   -169,
	  -153,   -126,    -89,    -50,    -10,     24,     52,     72,
	    81,     82,     75,     62,     45,
};

/*
 * The timing discriminant, `FPM_SRE_DISC` = 11 coefficients.  That constant
 * was derived from V.32's `SREv32_XB_COFFS` being 22 bytes; V.17's is 22
 * bytes as well, so the two agree independently.  Like V.29's and unlike
 * V.32's, this one is const: the object has it in `.rodata`.
 */
const short SREv17_XB_COFFS[FPM_SRE_DISC] = {
	-28156,  16128,  28156,  16128,  14078,   8128, -14078,   8128,
	   992, -15360,  14399,
};

/*
 * The 8000 -> 7200 Hz input resampler, 9 branches of 40 taps.
 * `V17RX_create` writes `branches = 9`, `decimate = 10` and `taps = 0x168`
 * into the `fpm_mrf_cfg` it builds, so 360 is the consumer's own count and
 * not this symbol's size divided by a guess.  9/10 of 8000 is 7200, which is
 * V.17's internal rate -- three samples per symbol at 2400 baud.
 *
 * Symmetric about its centre pair, as a linear-phase prototype must be.
 * V.29's equivalent is 30 taps a branch; V.17's is 40.
 */
const short MRFv17_COFFS[360] = {
	    76,     77,     75,     69,     60,     49,     36,     23,
	    12,      4,      0,      0,      7,     19,     38,     60,
	    87,    114,    142,    167,    186,    200,    205,    202,
	   189,    169,    141,    110,     76,     44,     16,     -4,
	   -15,    -14,     -1,     25,     61,    107,    158,    210,
	   259,    301,    332,    348,    348,    329,    294,    243,
	   181,    111,     40,    -28,    -85,   -127,   -150,   -151,
	  -128,    -84,    -21,     56,    141,    226,    304,    366,
	   407,    420,    404,    355,    278,    175,     53,    -78,
	  -210,   -332,   -435,   -509,   -549,   -550,   -513,   -439,
	  -334,   -208,    -72,     62,    180,    270,    322,    328,
	   282,    185,     41,   -142,   -353,   -576,   -796,   -996,
	 -1161,  -1276,  -1333,  -1325,  -1254,  -1124,   -947,   -737,
	  -515,   -299,   -112,     26,    100,     97,     10,   -158,
	  -401,   -705,  -1049,  -1409,  -1758,  -2070,  -2318,  -2483,
	 -2548,  -2507,  -2361,  -2120,  -1802,  -1434,  -1047,   -676,
	  -356,   -120,      6,      1,   -144,   -427,   -834,  -1343,
	 -1917,  -2515,  -3092,  -3600,  -3995,  -4239,  -4306,  -4181,
	 -3865,  -3375,  -2742,  -2013,  -1244,   -500,    152,    650,
	   936,    967,    718,    183,   -622,  -1656,  -2858,  -4147,
	 -5426,  -6593,  -7539,  -8162,  -8371,  -8093,  -7281,  -5915,
	 -4007,  -1602,   1225,   4368,   7700,  11079,  14350,  17362,
	 19971,  22049,  23495,  24237,  24237,  23495,  22049,  19971,
	 17362,  14350,  11079,   7700,   4368,   1225,  -1602,  -4007,
	 -5915,  -7281,  -8093,  -8371,  -8162,  -7539,  -6593,  -5426,
	 -4147,  -2858,  -1656,   -622,    183,    718,    967,    936,
	   650,    152,   -500,  -1244,  -2013,  -2742,  -3375,  -3865,
	 -4181,  -4306,  -4239,  -3995,  -3600,  -3092,  -2515,  -1917,
	 -1343,   -834,   -427,   -144,      1,      6,   -120,   -356,
	  -676,  -1047,  -1434,  -1802,  -2120,  -2361,  -2507,  -2548,
	 -2483,  -2318,  -2070,  -1758,  -1409,  -1049,   -705,   -401,
	  -158,     10,     97,    100,     26,   -112,   -299,   -515,
	  -737,   -947,  -1124,  -1254,  -1325,  -1333,  -1276,  -1161,
	  -996,   -796,   -576,   -353,   -142,     41,    185,    282,
	   328,    322,    270,    180,     62,    -72,   -208,   -334,
	  -439,   -513,   -550,   -549,   -509,   -435,   -332,   -210,
	   -78,     53,    175,    278,    355,    404,    420,    407,
	   366,    304,    226,    141,     56,    -21,    -84,   -128,
	  -151,   -150,   -127,    -85,    -28,     40,    111,    181,
	   243,    294,    329,    348,    348,    332,    301,    259,
	   210,    158,    107,     61,     25,     -1,    -14,    -15,
	    -4,     16,     44,     76,    110,    141,    169,    189,
	   202,    205,    200,    186,    167,    142,    114,     87,
	    60,     38,     19,      7,      0,      0,      4,     12,
	    23,     36,     49,     60,     69,     75,     77,     76,
};

/*
 * The AGC smoother, Q15: [0] is the acquisition pair and [1] the tracking
 * one.  FILE-STATIC, and that is the object's storage class, not a
 * simplification -- `AGC_DEF_ALPHA` and `AGC_DEF_BETA` are each defined six
 * times in the 1.2 MB, five of them local, so there is no single blob symbol
 * of that name to be compared against (F9144).  V.17's copies are the pair at
 * 0x9e2c and 0x9e28 and they are reached only through `AGCv17_CFG`.
 *
 * BETA IS EMITTED FIRST because it sits at the lower address in the object.
 * The values happen to equal V.29's; `AGCv17_CFG`'s other fields do not.
 */
static const short AGC_DEF_BETA[2] = { 16384, 3277 };
static const short AGC_DEF_ALPHA[2] = { 16384, 29491 };

/*
 * 24 bytes at .rodata 0x9e10.  The two pointers at +0x0c and +0x10 are the
 * whole reason this is a struct and not a `short[12]`, and they are the ONLY
 * relocations anywhere inside any of this file's twenty-four symbols -- the
 * inner-relocation sweep was run over all of them and shown firing on
 * `AGCb103_CFG` first.
 *
 * `ref_level` 9011 is the loop's target, against V.29's 11211, so the two
 * modulations are not a copy of each other.  The gate floors are 2 before the
 * first gain exists and 50 afterwards, and the measurement block is 40
 * samples -- 5.6 ms at the receiver's 7200 Hz.
 *
 * `f06`, `f08`, `f14` and `f16` keep their offset names: no `fpm_agc`
 * function reads any of them.  6553 at +0x16 is the same 0.2 in Q15 that both
 * V.32 configs and V.29's carry there, which is why F1621 established that
 * field is not padding.
 */
const struct fpm_agc_cfg AGCv17_CFG = {
	  9011,			/* +0x00 ref_level                           */
	     2,			/* +0x02 acquire_level                       */
	    50,			/* +0x04 squelch_level                       */
	  1000,			/* +0x06 f06                                 */
	     1,			/* +0x08 f08                                 */
	    40,			/* +0x0a block_len                           */
	AGC_DEF_ALPHA,		/* +0x0c                                     */
	AGC_DEF_BETA,		/* +0x10                                     */
	   158,			/* +0x14 f14                                 */
	  6553			/* +0x16 f16, 0.2 in Q15                     */
};


/* -------------------------------------------------------------------- .data */

/*
 * The tone detector's two biquad sections, five shorts each.  Nothing here
 * names the tones: the reading that this is two sections comes from
 * `V17RX_create` writing `tones = 2` beside it, and 20 bytes over two
 * sections is five shorts each -- the same shape `V21_CHAN2_MTD_COEFF` and
 * `V29_MTD_COEFF` have.  The thresholds the constructor pairs it with are a
 * Q15 ratio of 0x4ccd and a minimum level of 100.
 */
short V17_MTD_COEFF[10] = {
	-13271,  16384,  26277, -29197,  16384,
	-13271,  16384, -20853,  23170,  16384,
};

/*
 * The carrier phase reference.  FOUR entries, and that is `fse.clk_mod = 4`
 * from the constructor as well as 8 bytes from the symbol table -- two
 * readings, as everything else here has.  `V17RX_create` sets `clk_inc = 1`
 * beside it, and at three samples per symbol at 2400 baud the receiver runs
 * at 7200 Hz, so
 *
 *     7200 Hz * 1 / 4 = 1800 Hz
 *
 * which is V.17's carrier as the Recommendation defines it.  The values are
 * `round(i * 32768 / 4)` for all four, one full turn of phase in Q16.  A
 * reading that made this table anything but four shorts of Q16 phase would
 * not produce that number.
 */
short CRRv17_CLK[4] = {
	     0,   8192,  16384,  24576,
};

/*
 * The equaliser PLL's gain triples, indexed by `fpm_fse::pll_sel`; three
 * entries each, which is all the room `fpm_fse_cfg` leaves.
 *
 * THERE ARE TWO K1 ARRAYS AND ONE K2, AND THE SWITCH IS NOT A RATE.  See
 * `v17cfg.h`: `V17RX_create` takes `_S` when the caller has supplied a
 * pre-loaded equaliser coefficient set and the plain one otherwise, and the
 * integral gains are shared between the two cases.  The two differ in gear 0
 * alone -- 602 against 10347 -- and agree in gears 1 and 2.
 * K2[0] is zero, so gear 0 is proportional only, the same shape V.29's and
 * V.32's pairs have.
 */
short CRRv17_PLL_K1_S[3] = {
	 10347,   3050,    766,
};

short CRRv17_PLL_K2[3] = {
	     0,     18,      1,
};

short CRRv17_PLL_K1[3] = {
	   602,   3050,    766,
};

/*
 * The timing loop's gain triples, indexed by `fpm_sre::mode`; `FPM_SRE_MODES`
 * entries each.
 *
 * AND THESE TWO ARE ELEMENT-FOR-ELEMENT EQUAL.  `SREv17_PLL_K1` and
 * `SREv17_PLL_K1_S` hold the same three values and are still two distinct
 * symbols at two addresses, which is a fact about the object and not a
 * transcription slip -- `t_v17cfg.c` asserts the equality so that a future
 * edit to one of them fails rather than passing quietly.  What the switch
 * actually changes for the timing loop is `sre.settle`, 48 against 85; the
 * gains do not move.
 */
short SREv17_PLL_K1_S[FPM_SRE_MODES] = {
	  2336,   3049,   3049,
};

short SREv17_PLL_K2[FPM_SRE_MODES] = {
	     0,     17,     17,
};

short SREv17_PLL_K1[FPM_SRE_MODES] = {
	  2336,   3049,   3049,
};

/*
 * The clock reference, `sre.clock_len` = 3 points.  X is the cosine leg and Y
 * the sine leg of three phases 120 degrees apart: 16384 is 1.0 in Q14, -8192
 * its cosine at +-120 degrees, and 14189 is 0.866 in Q14.  Both legs sum to
 * zero, which is what makes the correlation a discriminant rather than a
 * level measurement.  V.29's pair holds the same six values.
 */
short SREv17_yCLOCK[3] = {
	     0,  14189, -14189,
};

short SREv17_xCLOCK[3] = {
	 16384,  -8192,  -8192,
};
