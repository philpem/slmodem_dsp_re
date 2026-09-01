/*
 * v29cfg.c -- ITU-T V.29 (fax): the receiver's coefficient and gain tables.
 *
 * Reconstructed from dsplibs.o:
 *
 *   AGCv29_CFG        .rodata 0x00b27c    24
 *   V29RX_MRF_FILT    .rodata 0x00b060   540
 *   V29RX_SRE_FILT    .rodata 0x00aee0   362
 *   V29RX_XB_COFFS    .rodata 0x00b04a    22
 *   V29_MTD_COEFF     .data   0x007da0    20
 *   V29RX_FSE_QFILT   .data   0x007ec0    98
 *   V29RX_FSE_IFILT   .data   0x007f40    98
 *   V29RX_FSE_PLLK2   .data   0x007fa2     6
 *   V29RX_FSE_PLLK1   .data   0x007fa8     6
 *   V29RX_CRR_TABLE   .data   0x007fc0   144
 *   V29RX_SRE_PLLK2   .data   0x008050     6
 *   V29RX_SRE_PLLK1   .data   0x008056     6
 *   V29RX_YCLOCK      .data   0x00805c     6
 *   V29RX_XCLOCK      .data   0x008062     6
 *   V29RX_DEC_QMAP    .data   0x007e20    32
 *   V29RX_DEC_IMAP    .data   0x007e40    32
 *   V29RX_DEC_ANGLE   .data   0x007e60    32
 *   V29RX_DEC_MAG     .data   0x007e80    32
 *   V29RX_DEC_PMAP    .data   0x007ea0    16
 *
 * plus the two file-static Q15 smoother pairs `AGCv29_CFG` points at, at
 * .rodata 0xb294 and 0xb298.  Read `v29cfg.h` for what types each of these and
 * why the byte count alone does not; this file carries the bytes.
 *
 * TWO CONSUMERS, NOT ONE.  The first fourteen are `V29RX_create`'s, and the
 * five `V29RX_DEC_*` at the end are `V29RX_decision`'s -- and they are the
 * WHOLE of that function's data blocker, so it becomes writable the moment
 * this file lands.  They are here because they are V.29 receiver tables like
 * the rest, not because the two functions share anything else.
 *
 * ORDER.  Definitions are in the object's own address order within each
 * section, .rodata before .data, which is the order the emitting translation
 * unit most plausibly had.  It is a guess about layout and nothing rests on it
 * -- a data symbol's bytes do not depend on where its definition sits -- but
 * where the object offers an order for free it is cheaper to keep it than to
 * invent one.
 *
 * THE TRANSLATION UNIT IS NOT SETTLED.  These fourteen sit in the same runs of
 * `.data` and `.rodata` as `V29RX_CFG`, `V29RX_CTL` and `V29RX_MESG`, so the
 * author's file was almost certainly the one holding `V29RX_create` itself.
 * They are here rather than in `src/fax/v29.c` because that module belongs to
 * another strand of this wave and a table is worth nothing until something can
 * link against it.  Moving them later is free.  This is `faxcfg.c`'s D1080
 * again, and it is recorded as D1100.
 *
 * NOTHING HERE IS A GENERATOR.  `V29RX_CRR_TABLE` has a closed form and the
 * header states it, but `docs/fastpass.md` defers coefficient derivations to
 * the 8 kHz retarget: a byte-exact copy is byte-exact, and the differential
 * test proves it with no derivation at all.  The derivation is recorded as
 * EVIDENCE FOR THE ELEMENT TYPE, which is the thing a byte copy cannot give.
 */

#include "dsplib/v29cfg.h"
#include "dsplib/fpm_agc.h"

/* ------------------------------------------------------------------ .rodata */

/*
 * The AGC smoother, Q15: [0] is the acquisition pair and [1] the tracking one.
 * FILE-STATIC, and that is the object's storage class, not a simplification --
 * `AGC_DEF_ALPHA` and `AGC_DEF_BETA` are each defined six times in the 1.2 MB,
 * five of them local, so there is no single blob symbol of that name to be
 * compared against.  V.29's copies are the pair at 0xb298 and 0xb294 and they
 * are reached only through `AGCv29_CFG`.
 *
 * BETA IS EMITTED FIRST because it sits at the lower address in the object.
 * That is the only claim the layout makes and it costs nothing to honour.
 */
static const short AGC_DEF_BETA[2] = { 16384, 3277 };
static const short AGC_DEF_ALPHA[2] = { 16384, 29491 };

/*
 * 24 bytes at .rodata 0xb27c.  The two pointers at +0x0c and +0x10 are the
 * whole reason this is a struct and not a `short[12]`: `tabdump.py` reads them
 * as -19816 and -19820, which are plausible coefficients and are addends.
 *
 * `ref_level` 11211 is the loop's target; the gain applied works out as
 * `ref_level / (2 * level)`, so the output settles near 5605 RMS.  The gate
 * floors are 2 before the first gain exists and 50 afterwards, and the
 * measurement block is 40 samples -- 5.6 ms at the receiver's 7200 Hz.
 *
 * `f06`, `f08`, `f14` and `f16` keep their offset names: no `fpm_agc` function
 * reads any of them, so their values (1000, 1, 158, 6553) are known and their
 * meaning is not.  6553 at +0x16 is the same 0.2 in Q15 that both V.32 configs
 * carry there, which is why F1621 established that field is not padding.
 */
const struct fpm_agc_cfg AGCv29_CFG = {
	11211,			/* +0x00 ref_level                           */
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

/*
 * The symbol-timing recovery prototype: `sre.coeffs` is set to 180, ten
 * branches of eighteen taps, and `cfg.proto` must hold ONE MORE than that
 * because the interpolator reads `proto[i+1]` at `i == coeffs-1`.  181 shorts
 * is 362 bytes, which is the symbol's size.  Symmetric about its centre value
 * 15925, as a linear-phase prototype must be.
 */
const short V29RX_SRE_FILT[181] = {
	    41,     57,     69,     76,     75,     66,     48,     22,
	   -10,    -46,    -82,   -115,   -141,   -155,   -155,   -139,
	  -106,    -59,      0,     66,    133,    195,    243,    272,
	   277,    254,    204,    127,     30,    -79,   -192,   -297,
	  -383,   -439,   -456,   -430,   -358,   -245,    -96,     76,
	   255,    426,    571,    672,    715,    692,    599,    439,
	   220,    -39,   -317,   -590,   -828,  -1007,  -1102,  -1098,
	  -986,   -766,   -450,    -60,    373,    811,   1212,   1534,
	  1737,   1789,   1670,   1373,    906,    296,   -417,  -1177,
	 -1916,  -2564,  -3045,  -3291,  -3243,  -2857,  -2109,  -1001,
	   445,   2181,   4135,   6219,   8332,  10365,  12209,  13763,
	 14940,  15675,  15925,  15675,  14940,  13763,  12209,  10365,
	  8332,   6219,   4135,   2181,    445,  -1001,  -2109,  -2857,
	 -3243,  -3291,  -3045,  -2564,  -1916,  -1177,   -417,    296,
	   906,   1373,   1670,   1789,   1737,   1534,   1212,    811,
	   373,    -60,   -450,   -766,   -986,  -1098,  -1102,  -1007,
	  -828,   -590,   -317,    -39,    220,    439,    599,    692,
	   715,    672,    571,    426,    255,     76,    -96,   -245,
	  -358,   -430,   -456,   -439,   -383,   -297,   -192,    -79,
	    30,    127,    204,    254,    277,    272,    243,    195,
	   133,     66,      0,    -59,   -106,   -139,   -155,   -155,
	  -141,   -115,    -82,    -46,    -10,     22,     48,     66,
	    75,     76,     69,     57,     41
};

/*
 * The timing discriminant, `FPM_SRE_DISC` = 11 coefficients.  That constant
 * was derived from V.32's `SREv32_XB_COFFS` being 22 bytes; V.29's is 22 bytes
 * as well, so the two agree independently.  Unlike V.32's, this one is const:
 * the object has it in `.rodata`.
 */
const short V29RX_XB_COFFS[FPM_SRE_DISC] = {
	-29401,  16058,  26574,  16058,  14700,   6855, -13287,   9304,
	   324, -16056,  15735
};

/*
 * The 8000 -> 7200 Hz input resampler, 9 branches of 30 taps.  `V29RX_create`
 * writes `branches = 9`, `decimate = 10` and `taps = 270` into the
 * `fpm_mrf_cfg` it builds, so the count is the consumer's own and not this
 * symbol's size divided by a guess.  Symmetric about the centre pair.
 */
const short V29RX_MRF_FILT[270] = {
	  -434,   -632,   -836,  -1030,  -1199,  -1329,  -1410,  -1437,
	 -1405,  -1318,  -1182,  -1008,   -809,   -602,   -405,   -232,
	  -100,    -20,      0,    -43,   -147,   -306,   -509,   -740,
	  -983,  -1219,  -1429,  -1596,  -1708,  -1754,  -1730,  -1638,
	 -1484,  -1279,  -1040,   -785,   -536,   -313,   -134,    -16,
	    31,      0,   -109,   -288,   -526,   -805,  -1105,  -1402,
	 -1673,  -1896,  -2053,  -2130,  -2120,  -2021,  -1841,  -1591,
	 -1290,   -963,   -634,   -331,    -78,    101,    190,    180,
	    67,   -143,   -436,   -791,  -1182,  -1580,  -1954,  -2272,
	 -2508,  -2641,  -2657,  -2551,  -2328,  -2003,  -1598,  -1143,
	  -673,   -226,    161,    455,    628,    661,    545,    283,
	  -111,   -610,  -1181,  -1779,  -2361,  -2877,  -3284,  -3543,
	 -3628,  -3522,  -3223,  -2745,  -2116,  -1379,   -585,    205,
	   928,   1520,   1926,   2099,   2008,   1641,   1003,    123,
	  -950,  -2150,  -3394,  -4589,  -5635,  -6436,  -6898,  -6946,
	 -6519,  -5583,  -4128,  -2174,    228,   3004,   6052,   9251,
	 12470,  15567,  18405,  20854,  22799,  24150,  24841,  24841,
	 24150,  22799,  20854,  18405,  15567,  12470,   9251,   6052,
	  3004,    228,  -2174,  -4128,  -5583,  -6519,  -6946,  -6898,
	 -6436,  -5635,  -4589,  -3394,  -2150,   -950,    123,   1003,
	  1641,   2008,   2099,   1926,   1520,    928,    205,   -585,
	 -1379,  -2116,  -2745,  -3223,  -3522,  -3628,  -3543,  -3284,
	 -2877,  -2361,  -1779,  -1181,   -610,   -111,    283,    545,
	   661,    628,    455,    161,   -226,   -673,  -1143,  -1598,
	 -2003,  -2328,  -2551,  -2657,  -2641,  -2508,  -2272,  -1954,
	 -1580,  -1182,   -791,   -436,   -143,     67,    180,    190,
	   101,    -78,   -331,   -634,   -963,  -1290,  -1591,  -1841,
	 -2021,  -2120,  -2130,  -2053,  -1896,  -1673,  -1402,  -1105,
	  -805,   -526,   -288,   -109,      0,     31,    -16,   -134,
	  -313,   -536,   -785,  -1040,  -1279,  -1484,  -1638,  -1730,
	 -1754,  -1708,  -1596,  -1429,  -1219,   -983,   -740,   -509,
	  -306,   -147,    -43,      0,    -20,   -100,   -232,   -405,
	  -602,   -809,  -1008,  -1182,  -1318,  -1405,  -1437,  -1410,
	 -1329,  -1199,  -1030,   -836,   -632,   -434
};

/* -------------------------------------------------------------------- .data */

/*
 * The tone detector's two biquad sections, five shorts each.  Nothing here
 * names the tones: the reading that this is two sections comes from
 * `V29RX_create` writing `tones = 2` beside it, and 20 bytes over two sections
 * is five shorts each, which is the same shape `V21_CHAN2_MTD_COEFF` has.
 */
short V29_MTD_COEFF[10] = {
	-13271,  16384,  27246, -30274,  16384,
	-13271,  16384, -19153,  21281,  16384
};

/*
 * The equaliser's initial coefficients, 49 taps in each rail.  Every third
 * entry is zero, which is the three-samples-per-symbol spacing showing through
 * a filter that only has energy at the symbol instants; the I rail is
 * symmetric about tap 24 and the Q rail antisymmetric about it, which is the
 * I/Q pair of one passband filter.
 */
short V29RX_FSE_QFILT[49] = {
	     0,     -3,      7,      0,     45,     -6,      0,     13,
	  -191,      0,   -298,   -167,      0,   -342,    556,      0,
	   688,   1072,      0,   1842,   -903,      0,   -964, -11202,
	     0,  11202,    964,      0,    903,  -1842,      0,  -1072,
	  -688,      0,   -556,    342,      0,    167,    298,      0,
	   191,    -13,      0,      6,    -45,      0,     -7,      3,
	     0
};

short V29RX_FSE_IFILT[49] = {
	     0,     -6,     -2,      0,     -8,    -73,      0,   -146,
	   -34,      0,   -109,    359,      0,    489,    467,      0,
	   820,   -750,      0,   -859,  -2480,      0,  -5468,    980,
	 13653,    980,  -5468,      0,  -2480,   -859,      0,   -750,
	   820,      0,    467,    489,      0,    359,   -109,      0,
	   -34,   -146,      0,    -73,     -8,      0,     -2,     -6,
	     0
};

/*
 * The equaliser PLL's three gain pairs, indexed by `fpm_fse::pll_sel`.  K2 is
 * the integral term and its first entry is zero, so gear 0 is proportional
 * only -- the same shape V.32's pair has.
 */
short V29RX_FSE_PLLK2[3] = { 0, 240, 35 };
short V29RX_FSE_PLLK1[3] = { 1516, 7930, 3032 };

/*
 * The carrier phase ramp.  72 entries, `round(i * 32768 / 72)` for every one
 * of them, and `V29RX_create` steps it by `clk_inc = 17` per input sample:
 * 7200 * 17 / 72 = 1700 Hz, V.29's carrier.  See `v29cfg.h`.
 */
short V29RX_CRR_TABLE[72] = {
	     0,    455,    910,   1365,   1820,   2276,   2731,   3186,
	  3641,   4096,   4551,   5006,   5461,   5916,   6372,   6827,
	  7282,   7737,   8192,   8647,   9102,   9557,  10012,  10468,
	 10923,  11378,  11833,  12288,  12743,  13198,  13653,  14108,
	 14564,  15019,  15474,  15929,  16384,  16839,  17294,  17749,
	 18204,  18660,  19115,  19570,  20025,  20480,  20935,  21390,
	 21845,  22300,  22756,  23211,  23666,  24121,  24576,  25031,
	 25486,  25941,  26396,  26852,  27307,  27762,  28217,  28672,
	 29127,  29582,  30037,  30492,  30948,  31403,  31858,  32313
};

/*
 * The timing loop's three gain pairs, indexed by `fpm_sre::mode`.  Same shape
 * as the equaliser's: K2[0] is zero, so mode 0 is proportional only.
 */
short V29RX_SRE_PLLK2[FPM_SRE_MODES] = { 0, 8, 70 };
short V29RX_SRE_PLLK1[FPM_SRE_MODES] = { 3732, 2037, 6064 };

/*
 * The clock reference, `sre.clock_len` = 3 points.  X is the cosine leg and Y
 * the sine leg of three phases 120 degrees apart: 16384 is 1.0 in Q14 and
 * -8192 is its cosine at +-120 degrees, 14189 is 0.866 in Q14.  That reading
 * is arithmetic on the values themselves and does not depend on the names.
 */
short V29RX_YCLOCK[3] = { 0, 14189, -14189 };
short V29RX_XCLOCK[3] = { 16384, -8192, -8192 };

/*
 * ------------------------------------------------------------------------
 * THE DECISION TABLES.  `V29RX_decision` (0x9b8f0, 260 bytes) is their only
 * reader and it references nothing else that is unwritten, so these five are
 * the whole of its data blocker.
 *
 * ALL FIVE ARE LOADED WITH `movzwl TABLE(%reg,%reg,1)`, which is a stride of
 * two -- the index is doubled by the addressing mode, not scaled by a size --
 * so the element type is sixteen bits and the counts below are `st_size / 2`
 * confirmed by the loop's own bound.
 *
 * THE BOUND IS 8 OR 16 AND THE OBJECT COMPUTES IT WITHOUT A BRANCH:
 *
 *      cmp  $0x1,%ebp          ebp = rx->f10, the rate selector
 *      sbb  %esi,%esi          esi = -1 when f10 == 0, else 0
 *      and  $0xfffffff8,%esi   esi = -8 or 0
 *      lea  0x10(%esi),%ebp    ebp = 8 or 16
 *
 * so the slicer searches the first EIGHT points at the low rate and all
 * SIXTEEN at the high one. Eight points is three bits a symbol at 2400 baud,
 * which is V.29's 7200 bit/s fallback; sixteen is four bits, which is 9600.
 * That is why the tables are sixteen entries with the eight-point
 * constellation FIRST, and it is measured from the code rather than inferred
 * from the layout.
 *
 * AND THE CONSTELLATION IS THE RECOMMENDATION'S OWN, TO THE UNIT.  V.29's
 * Table 1 gives amplitudes 3 and 5 on the axes and sqrt(2) and 3*sqrt(2) on
 * the diagonals. Every value in `V29RX_DEC_IMAP`, `_QMAP` and `_MAG` is one of
 * those four multiplied by 2048:
 *
 *      3 * 2048        = 6144         sqrt(2) * 2048  = 2896
 *      5 * 2048        = 10240      3*sqrt(2) * 2048  = 8689
 *
 * Four independent numbers agreeing with a published table is what makes this
 * a derivation rather than a resemblance, and it is the evidence that the
 * element type is a signed sixteen-bit amplitude. `t_v29cfg.c` asserts all
 * sixteen points against `2048 * A * {cos,sin}(k * 45 degrees)` computed from
 * the amplitudes alone.
 *
 * `V29RX_DEC_ANGLE` is the same eight phases twice, 4096 per 45 degrees in a
 * 32768-count turn, so the second ring repeats the first ring's angles -- as
 * it must, both rings being at the same eight phases.
 *
 * `V29RX_DEC_PMAP` is indexed by `(phase - prev_phase) & 7` and is the
 * differential decoder's Gray map. Eight entries, and it is a PERMUTATION of
 * 0..7 -- asserted as one, because a table of eight small integers is exactly
 * where a transcription slip hides.
 *
 * THE LOADS ARE `movzwl` AND THE VALUES ARE NEGATIVE, WHICH IS NOT A
 * CONTRADICTION.  Each load is followed by a subtraction and then a
 * `movswl %dx,%edx` that discards the upper half, so the extension is 614's
 * free case: `unsigned short` and `short` compile to the same bytes and behave
 * identically here. `short` is chosen because `V29RX_DEC_IMAP` holds -6144 and
 * the amplitudes above are signed. Recorded as a choice, not as a reading.
 */

short V29RX_DEC_QMAP[16] = {
	     0,   2048,   6144,   2048,      0,  -2048,  -6144,  -2048,
	     0,   6144,  10240,   6144,      0,  -6144, -10240,  -6144
};

short V29RX_DEC_IMAP[16] = {
	  6144,   2048,      0,  -2048,  -6144,  -2048,      0,   2048,
	 10240,   6144,      0,  -6144, -10240,  -6144,      0,   6144
};

short V29RX_DEC_ANGLE[16] = {
	     0,   4096,   8192,  12288,  16384,  20480,  24576,  28672,
	     0,   4096,   8192,  12288,  16384,  20480,  24576,  28672
};

short V29RX_DEC_MAG[16] = {
	  6144,   2896,   6144,   2896,   6144,   2896,   6144,   2896,
	 10240,   8689,  10240,   8689,  10240,   8689,  10240,   8689
};

short V29RX_DEC_PMAP[8] = { 1, 0, 2, 3, 7, 6, 4, 5 };
