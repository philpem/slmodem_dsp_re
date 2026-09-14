/*
 * v27cfg.c -- ITU-T V.27ter (fax): the receiver's coefficient, gain and
 *             rate-selection tables.
 *
 * Fifty-six symbols, reconstructed from dsplibs.o.  Read `v27cfg.h` for what
 * types each of them and why the byte count alone does not; this file carries
 * the bytes.
 *
 *   V27_MTD_COEFF_2400         .data    0x007b20    20
 *   V27_MTD_COEFF_4800         .data    0x007b34    20
 *   V27RX_DEC_LAST_PHASE_2400  .data    0x007bc0     8
 *   V27RX_DEC_LAST_PHASE_4800  .data    0x007bc8    16
 *   V27RX_DEC_LAST_PHASE       .data    0x007bd8     8
 *   V27RX_DEC_PMAP_2400        .data    0x007be0     8
 *   V27RX_DEC_PMAP_4800        .data    0x007be8    16
 *   V27RX_DEC_PMAP             .data    0x007bf8     8
 *   V27RX_DEC_PHS_MASK         .data    0x007c00     4
 *   V27RX_FSE_PLLK2_2400       .data    0x007c04     6
 *   V27RX_FSE_PLLK1_2400       .data    0x007c0a     6
 *   V27RX_FSE_PLLK2_4800       .data    0x007c10     6
 *   V27RX_FSE_PLLK1_4800       .data    0x007c16     6
 *   V27RX_FSE_PLLK2            .data    0x007c1c     8
 *   V27RX_FSE_PLLK1            .data    0x007c24     8
 *   V27RX_CRR_ADJUST           .data    0x007c2c     4
 *   V27RX_CRR_TABLE_2400       .data    0x007c30     8
 *   V27RX_CRR_TABLE_4800       .data    0x007c40    80
 *   V27RX_CRR_TABLE_LEN        .data    0x007c90     4
 *   V27RX_CRR_TABLE            .data    0x007c94     8
 *   V27RX_FSE_MU_TRACK         .data    0x007c9c     4
 *   V27RX_FSE_MU_TRAIN         .data    0x007ca0     4
 *   V27RX_SRE_PLLK2_2400       .data    0x007ca4     6
 *   V27RX_SRE_PLLK1_2400       .data    0x007caa     6
 *   V27RX_SRE_PLLK2_4800       .data    0x007cb0     6
 *   V27RX_SRE_PLLK1_4800       .data    0x007cb6     6
 *   V27RX_SRE_PLLK2            .data    0x007cbc     8
 *   V27RX_SRE_PLLK1            .data    0x007cc4     8
 *   V27RX_YCLOCK_2400          .data    0x007ccc    12
 *   V27RX_YCLOCK_4800          .data    0x007cd8    10
 *   V27RX_YCLOCK               .data    0x007ce4     8
 *   V27RX_XCLOCK_2400          .data    0x007cec    12
 *   V27RX_XCLOCK_4800          .data    0x007cf8    10
 *   V27RX_XCLOCK               .data    0x007d04     8
 *   V27RX_SRE_FILT             .data    0x007d0c     8
 *   V27RX_MRF_FILT             .data    0x007d14     8
 *   V27RX_FSE_FILT_LEN         .rodata  0x00a120     4
 *   V27RX_FSE_QFILT_4800       .rodata  0x00a140   162
 *   V27RX_FSE_QFILT_2400       .rodata  0x00a200   194
 *   V27RX_FSE_QFILT            .rodata  0x00a2c4     8
 *   V27RX_FSE_IFILT_4800       .rodata  0x00a2e0   162
 *   V27RX_FSE_IFILT_2400       .rodata  0x00a3a0   194
 *   V27RX_FSE_IFILT            .rodata  0x00a464     8
 *   V27RX_SAMP_PER_BAUD        .rodata  0x00a46c     4
 *   V27RX_XB_COFFS_2400        .rodata  0x00a470    22
 *   V27RX_XB_COFFS_4800        .rodata  0x00a486    22
 *   V27RX_XB_COFFS             .rodata  0x00a49c     8
 *   V27RX_SRE_FILT_2400        .rodata  0x00a4c0   542
 *   V27RX_SRE_FILT_4800        .rodata  0x00a6e0   402
 *   V27RX_SRE_FILT_LEN         .rodata  0x00a872     4
 *   V27RX_MRF_FILT_2400        .rodata  0x00a880   540
 *   V27RX_MRF_FILT_4800        .rodata  0x00aaa0    72
 *   V27RX_MRF_FILT_LEN         .rodata  0x00aae8     4
 *   V27RX_MRF_DOWN             .rodata  0x00aaec     4
 *   V27RX_MRF_UP               .rodata  0x00aaf0     4
 *   AGCv27_CFG                 .rodata  0x00aaf4    24
 *
 * WHAT MAKES THIS FILE DIFFERENT FROM `v29cfg.c` IS THAT V.27ter HAS TWO
 * RATES, and the object expresses that as a level of indirection rather than
 * as two configurations.  Fourteen of these symbols are EIGHT BYTES in .data
 * or .rodata and are TWO POINTERS -- `{ ..._2400, ..._4800 }` -- and eleven
 * more are FOUR BYTES and are `short[2]`.  `V27RX_create` picks a rate index
 * once and subscripts every one of them with it.  Reading an eight-byte
 * selector as `short[4]` gives four plausible small integers, which is the
 * mistake `tools/dis.py` and the inner-relocation sweep exist to prevent.
 * Finding F9150.
 *
 * THE RATE INDEX IS 0 FOR 2400 bit/s AND 1 FOR 4800 bit/s, and that is read
 * off the object rather than off the symbol names.  `V27RX_create` at 99794
 * loads the modem's own +0x04, compares it against 0x960 and 0x12c0 -- 2400
 * and 4800, decimal, the two bit rates V.27ter defines -- and stores 0 or 1
 * into the shared block's +0x08.  Every later `movswl 0x8(%ecx)` is that
 * index.  The `cmpw $0x1,0x8(%ebx)` at 997e7 selecting `V27_MTD_COEFF_4800`
 * says the same thing a second time.  Finding F9151.
 *
 * ORDER.  Definitions are in the object's own address order within each
 * section, .rodata before .data, which is the order the emitting translation
 * unit most plausibly had.  Nothing rests on it -- a data symbol's bytes do
 * not depend on where its definition sits -- but where the object offers an
 * order for free it is cheaper to keep it than to invent one.  The one
 * departure is forced by C: `AGC_DEF_BETA` and `AGC_DEF_ALPHA` sit AFTER
 * `AGCv27_CFG` in .rodata and must precede it here, because its initialiser
 * names them.
 *
 * THE TRANSLATION UNIT IS NOT SETTLED.  These sit in the same runs of .data
 * and .rodata as `V27RX_CFG`, `V27RX_CTL` and `V27RX_MESG`, so the author's
 * file was almost certainly the one holding `V27RX_create` itself.  They are
 * here rather than in `src/fax/v27.c` because that module is not written and
 * a table is worth nothing until something can link against it.  Moving them
 * later is free.  This is `v29cfg.c`'s D1100 again, recorded as D1102.
 *
 * NOTHING HERE IS A GENERATOR.  Several of these tables have closed forms and
 * the header states them, but `docs/fastpass.md` defers coefficient
 * derivations to the 8 kHz retarget: a byte-exact copy is byte-exact, and the
 * differential test proves it with no derivation at all.  The derivations are
 * recorded as EVIDENCE FOR THE ELEMENT TYPE, which is the thing a byte copy
 * cannot give.
 */

#include "dsplib/v27cfg.h"
#include "dsplib/fpm_agc.h"

/* ------------------------------------------------------------------ .rodata */

/*
 * The equaliser's tap count per rate, `fpm_fse_cfg::taps`, patched in at
 * 99aec.  97 taps at 2400 bit/s and 81 at 4800, which is exactly the 194 and
 * 162 bytes of the four rail tables below.
 */
const short V27RX_FSE_FILT_LEN[2] = {
	    97,     81,
};


/*
 * The equaliser's initial coefficients, one I rail and one Q rail per rate.
 *
 * THE ZERO PATTERN IS THE CARRIER, AND IT IS ARITHMETIC RATHER THAN
 * OBSERVATION.  Each rail is one real prototype multiplied by a cosine or a
 * sine at the carrier, so a rail is zero exactly where its own trigonometric
 * factor is.  At 2400 bit/s the receiver runs at 7200 Hz and the carrier is
 * 1800 Hz -- a quarter of the sample rate -- so the I rail is zero at every
 * ODD tap and the Q rail at every EVEN one.  At 4800 bit/s it runs at 8000 Hz
 * and 1800/8000 is 9/40, so the pattern has period 20: the I rail is zero at
 * taps 10, 30, 50 and 70 and the Q rail at 0, 20, 40, 60 and 80.  Both
 * predictions hold with no exceptions in either direction, and `t_v27cfg.c`
 * asserts them as `if and only if`.
 *
 * The I rails are symmetric about their centre tap and the Q rails
 * antisymmetric about it, which is the I/Q pair of one passband filter.
 */
const short V27RX_FSE_QFILT_4800[81] = {
	     0,    -12,      2,    -21,    -13,      1,    -17,     12,
	     8,      3,     36,      3,     14,     21,    -35,     -4,
	   -24,    -47,      5,    -46,      0,     49,    -12,    109,
	    73,    -17,     88,    -72,    -42,    -29,   -344,    -34,
	  -262,   -425,   1057,    608,    417,   2892,  -1885,  -8250,
	     0,   8250,   1885,  -2892,   -417,   -608,  -1057,    425,
	   262,     34,    344,     29,     42,     72,    -88,     17,
	   -73,   -109,     12,    -49,      0,     46,     -5,     47,
	    24,      4,     35,    -21,    -14,     -3,    -36,     -3,
	    -8,    -12,     17,     -1,     13,     21,     -2,     12,
	     0,
};


const short V27RX_FSE_QFILT_2400[97] = {
	     0,    -16,      0,    -19,      0,     22,      0,     20,
	     0,    -22,      0,    -28,      0,     29,      0,     37,
	     0,    -44,      0,    -38,      0,     45,      0,     62,
	     0,    -67,      0,    -97,      0,    127,      0,     99,
	     0,   -136,      0,   -249,      0,    295,      0,    679,
	     0,  -1454,      0,   -414,      0,   5239,      0,  -9620,
	     0,   9620,      0,  -5239,      0,    414,      0,   1454,
	     0,   -679,      0,   -295,      0,    249,      0,    136,
	     0,    -99,      0,   -127,      0,     97,      0,     67,
	     0,    -62,      0,    -45,      0,     38,      0,     44,
	     0,    -37,      0,    -29,      0,     28,      0,     22,
	     0,    -20,      0,    -22,      0,     19,      0,     16,
	     0,
};


/*
 * The selector, and it is `R` -- the ARRAY of pointers is itself const here,
 * unlike the eleven selectors in .data further down.  That split is the
 * object's.  Both entries found by sweeping the relocations inside the
 * symbol's own eight bytes: +0x00 -> _2400, +0x04 -> _4800.
 */
const short *const V27RX_FSE_QFILT[2] = {
	V27RX_FSE_QFILT_2400, V27RX_FSE_QFILT_4800
};

const short V27RX_FSE_IFILT_4800[81] = {
	   -20,     -2,     -7,    -11,     18,      1,     12,     24,
	    -3,     20,      0,    -21,      5,    -41,    -25,      4,
	   -32,     24,     16,      7,     82,      8,     38,     55,
	  -100,    -17,    -64,   -140,     13,   -183,      0,    214,
	   -85,    833,    768,   -608,    574,  -1473,  -5801,   1307,
	  9209,   1307,  -5801,  -1473,    574,   -608,    768,    833,
	   -85,    214,      0,   -183,     13,   -140,    -64,    -17,
	  -100,     55,     38,      8,     82,      7,     16,     24,
	   -32,      4,    -25,    -41,      5,    -21,      0,     20,
	    -3,     24,     12,      1,     18,    -11,     -7,     -2,
	   -20,
};


const short V27RX_FSE_IFILT_2400[97] = {
	   -23,      0,     -1,      0,     29,      0,     -2,      0,
	   -30,      0,     -1,      0,     40,      0,      2,      0,
	   -55,      0,      6,      0,     61,      0,      4,      0,
	   -91,      0,     -7,      0,    150,      0,    -27,      0,
	  -176,      0,    -30,      0,    384,      0,     86,      0,
	 -1249,      0,    961,      0,   2597,      0,  -7781,      0,
	 10290,      0,  -7781,      0,   2597,      0,    961,      0,
	 -1249,      0,     86,      0,    384,      0,    -30,      0,
	  -176,      0,    -27,      0,    150,      0,     -7,      0,
	   -91,      0,      4,      0,     61,      0,      6,      0,
	   -55,      0,      2,      0,     40,      0,     -1,      0,
	   -30,      0,     -2,      0,     29,      0,     -1,      0,
	   -23,
};


const short *const V27RX_FSE_IFILT[2] = {
	V27RX_FSE_IFILT_2400, V27RX_FSE_IFILT_4800
};

/*
 * Samples per baud, and it is read THREE times by `V27RX_create` into three
 * different fields: `fpm_sre_cfg::clock_len` (9993c), `fpm_fse_cfg::interp`
 * (99acd) and, tripled, `fpm_sre_cfg::rms_len` (99a18).  Six and five.
 *
 * IT IS ALSO WHAT SIZES THE CLOCK LEGS.  `clock_len` is how many entries
 * `xclock` and `yclock` hold, and `V27RX_XCLOCK_2400` is twelve bytes against
 * `_4800`'s ten -- six shorts and five.  Two independent readings of the same
 * number, which is what the element stride rests on.
 *
 * And it closes on the Recommendation: 8000 * 9/10 / 6 is 1200 baud, four
 * phases, two bits a symbol, 2400 bit/s; 8000 * 1/1 / 5 is 1600 baud, eight
 * phases, three bits, 4800 bit/s.
 *
 * THE OBJECT LOADS IT BOTH WAYS -- `movzwl` at 9993c and 99acd, `movswl` at
 * 99a18 -- which by F7803 follows the declared type of the LOCAL and not of
 * this array.  `short` is what is written here, matching every other table in
 * the file; the two readings agree over 6 and 5.
 */
const short V27RX_SAMP_PER_BAUD[2] = {
	     6,      5,
};


/*
 * The symbol-timing discriminant, `FPM_SRE_DISC` = 11 coefficients per rate.
 * That constant came from V.32's `SREv32_XB_COFFS` being 22 bytes; both of
 * V.27ter's are 22 bytes as well, so it now has three independent witnesses.
 */
const short V27RX_XB_COFFS_2400[11] = {
	-15565,  14787,  15565,  14787,   7782,  13480,  -7782,  13480,
	   163,  16220,  16058,
};


const short V27RX_XB_COFFS_4800[11] = {
	-22012,  14787,  14133,  14787,  11006,  11006,  -7066,  13868,
	   163,  10025,  16058,
};


const short *const V27RX_XB_COFFS[2] = {
	V27RX_XB_COFFS_2400, V27RX_XB_COFFS_4800
};

/*
 * The symbol-timing recovery prototypes.  `fpm_sre_cfg::coeffs` is patched to
 * 270 and 200 -- ten polyphase branches of 27 and 20 taps -- and `proto` must
 * hold ONE MORE than that, because the interpolator reads `proto[i+1]` at
 * `i == coeffs-1`.  271 and 201 shorts is 542 and 402 bytes, which is what
 * `nm -S` gives.  Symmetric about their centre entries.
 */
const short V27RX_SRE_FILT_2400[271] = {
	   -23,     -4,     16,     37,     57,     75,     88,     96,
	    98,     94,     82,     64,     40,     12,    -19,    -51,
	   -81,   -108,   -128,   -141,   -145,   -139,   -123,    -98,
	   -64,    -24,     21,     66,    109,    148,    178,    198,
	   205,    198,    177,    143,     97,     42,    -18,    -81,
	  -142,   -196,   -239,   -268,   -280,   -274,   -248,   -204,
	  -144,    -70,     11,     96,    179,    254,    315,    357,
	   377,    372,    341,    286,    208,    112,      3,   -111,
	  -223,   -326,   -411,   -472,   -504,   -503,   -467,   -398,
	  -298,   -172,    -29,    125,    277,    418,    538,    627,
	   678,    685,    646,    560,    431,    265,     72,   -136,
	  -347,   -546,   -719,   -852,   -935,   -958,   -917,   -810,
	  -641,   -417,   -149,    146,    451,    746,   1010,   1223,
	  1367,   1428,   1394,   1260,   1028,    705,    305,   -153,
	  -643,  -1136,  -1599,  -1999,  -2302,  -2479,  -2501,  -2348,
	 -2007,  -1472,   -746,    158,   1217,   2403,   3678,   4998,
	  6318,   7588,   8761,   9791,  10638,  11269,  11658,  11790,
	 11658,  11269,  10638,   9791,   8761,   7588,   6318,   4998,
	  3678,   2403,   1217,    158,   -746,  -1472,  -2007,  -2348,
	 -2501,  -2479,  -2302,  -1999,  -1599,  -1136,   -643,   -153,
	   305,    705,   1028,   1260,   1394,   1428,   1367,   1223,
	  1010,    746,    451,    146,   -149,   -417,   -641,   -810,
	  -917,   -958,   -935,   -852,   -719,   -546,   -347,   -136,
	    72,    265,    431,    560,    646,    685,    678,    627,
	   538,    418,    277,    125,    -29,   -172,   -298,   -398,
	  -467,   -503,   -504,   -472,   -411,   -326,   -223,   -111,
	     3,    112,    208,    286,    341,    372,    377,    357,
	   315,    254,    179,     96,     11,    -70,   -144,   -204,
	  -248,   -274,   -280,   -268,   -239,   -196,   -142,    -81,
	   -18,     42,     97,    143,    177,    198,    205,    198,
	   178,    148,    109,     66,     21,    -24,    -64,    -98,
	  -123,   -139,   -145,   -141,   -128,   -108,    -81,    -51,
	   -19,     12,     40,     64,     82,     94,     98,     96,
	    88,     75,     57,     37,     16,     -4,    -23,
};


const short V27RX_SRE_FILT_4800[201] = {
	    68,     78,     83,     82,     73,     58,     36,      8,
	   -24,    -58,    -91,   -120,   -143,   -157,   -159,   -149,
	  -126,    -91,    -45,      9,     67,    126,    180,    225,
	   256,    270,    264,    236,    188,    121,     39,    -53,
	  -148,   -239,   -320,   -382,   -419,   -426,   -401,   -343,
	  -255,   -139,     -4,    141,    287,    422,    534,    613,
	   651,    642,    582,    474,    320,    131,    -82,   -306,
	  -522,   -715,   -867,   -965,   -995,   -952,   -832,   -638,
	  -380,    -72,    267,    613,    941,   1224,   1437,   1559,
	  1570,   1462,   1230,    880,    427,   -105,   -685,  -1276,
	 -1834,  -2314,  -2673,  -2867,  -2862,  -2630,  -2153,  -1426,
	  -458,    732,   2109,   3628,   5235,   6868,   8460,   9947,
	 11265,  12356,  13172,  13676,  13847,  13676,  13172,  12356,
	 11265,   9947,   8460,   6868,   5235,   3628,   2109,    732,
	  -458,  -1426,  -2153,  -2630,  -2862,  -2867,  -2673,  -2314,
	 -1834,  -1276,   -685,   -105,    427,    880,   1230,   1462,
	  1570,   1559,   1437,   1224,    941,    613,    267,    -72,
	  -380,   -638,   -832,   -952,   -995,   -965,   -867,   -715,
	  -522,   -306,    -82,    131,    320,    474,    582,    642,
	   651,    613,    534,    422,    287,    141,     -4,   -139,
	  -255,   -343,   -401,   -426,   -419,   -382,   -320,   -239,
	  -148,    -53,     39,    121,    188,    236,    264,    270,
	   256,    225,    180,    126,     67,      9,    -45,    -91,
	  -126,   -149,   -159,   -157,   -143,   -120,    -91,    -58,
	   -24,      8,     36,     58,     73,     82,     83,     78,
	    68,
};


/* `fpm_sre_cfg::coeffs`, patched in at 9994d.  See the two tables above. */
const short V27RX_SRE_FILT_LEN[2] = {
	   270,    200,
};


/*
 * The input resamplers.  `V27RX_create` writes `branches`, `decimate` and
 * `taps` from the three tables below, so both counts are the consumer's own
 * and not this symbol's size divided by a guess: 270 taps over 9 branches at
 * 2400 bit/s, converting 8000 -> 7200 Hz at 30 taps a branch, and 36 taps
 * over 1 branch at 4800, which is a plain 8 kHz band filter with no rate
 * change at all.  Both are symmetric about their centre pair.
 */
const short V27RX_MRF_FILT_2400[270] = {
	   -17,    -19,    -18,    -13,     -5,      6,     21,     38,
	    58,     78,     98,    116,    131,    142,    146,    145,
	   138,    124,    106,     84,     61,     38,     19,      5,
	    -1,      3,     18,     44,     81,    128,    181,    239,
	   297,    351,    397,    432,    451,    452,    435,    399,
	   345,    278,    200,    118,     38,    -35,    -93,   -132,
	  -147,   -136,    -98,    -34,     52,    154,    264,    375,
	   476,    557,    610,    628,    605,    539,    429,    281,
	   100,   -104,   -319,   -532,   -728,   -894,  -1019,  -1092,
	 -1109,  -1066,   -968,   -820,   -635,   -427,   -216,    -21,
	   139,    245,    281,    234,    100,   -122,   -426,   -799,
	 -1223,  -1673,  -2121,  -2541,  -2901,  -3177,  -3347,  -3396,
	 -3316,  -3110,  -2790,  -2375,  -1896,  -1390,   -896,   -458,
	  -120,     81,    113,    -46,   -406,   -964,  -1700,  -2583,
	 -3566,  -4591,  -5591,  -6496,  -7232,  -7728,  -7924,  -7767,
	 -7222,  -6273,  -4923,  -3195,  -1135,   1193,   3707,   6313,
	  8908,  11387,  13643,  15581,  17115,  18177,  18720,  18720,
	 18177,  17115,  15581,  13643,  11387,   8908,   6313,   3707,
	  1193,  -1135,  -3195,  -4923,  -6273,  -7222,  -7767,  -7924,
	 -7728,  -7232,  -6496,  -5591,  -4591,  -3566,  -2583,  -1700,
	  -964,   -406,    -46,    113,     81,   -120,   -458,   -896,
	 -1390,  -1896,  -2375,  -2790,  -3110,  -3316,  -3396,  -3347,
	 -3177,  -2901,  -2541,  -2121,  -1673,  -1223,   -799,   -426,
	  -122,    100,    234,    281,    245,    139,    -21,   -216,
	  -427,   -635,   -820,   -968,  -1066,  -1109,  -1092,  -1019,
	  -894,   -728,   -532,   -319,   -104,    100,    281,    429,
	   539,    605,    628,    610,    557,    476,    375,    264,
	   154,     52,    -34,    -98,   -136,   -147,   -132,    -93,
	   -35,     38,    118,    200,    278,    345,    399,    435,
	   452,    451,    432,    397,    351,    297,    239,    181,
	   128,     81,     44,     18,      3,     -1,      5,     19,
	    38,     61,     84,    106,    124,    138,    145,    146,
	   142,    131,    116,     98,     78,     58,     38,     21,
	     6,     -5,    -13,    -18,    -19,    -17,
};


const short V27RX_MRF_FILT_4800[36] = {
	    76,     -3,    171,    130,     96,    414,   -190,    458,
	  -363,   -348,   -134,  -2003,      8,  -3252,  -1347,  -1834,
	 -6921,  14943,  14943,  -6921,  -1834,  -1347,  -3252,      8,
	 -2003,   -134,   -348,   -363,    458,   -190,    414,     96,
	   130,    171,     -3,     76,
};


/* `fpm_mrf_cfg::taps`, patched in at 998b7. */
const short V27RX_MRF_FILT_LEN[2] = {
	   270,     36,
};


/* `fpm_mrf_cfg::decimate` (99886) and `::branches` (99872).  9/10 is
 * 8000 -> 7200 Hz; 1/1 is no conversion. */
const short V27RX_MRF_DOWN[2] = {
	    10,      1,
};


const short V27RX_MRF_UP[2] = {
	     9,      1,
};


/*
 * The AGC smoother, Q15: [0] is the acquisition pair and [1] the tracking one.
 * FILE-STATIC, and that is the object's storage class, not a simplification --
 * `AGC_DEF_ALPHA` and `AGC_DEF_BETA` are each defined SIX times in the 1.2 MB,
 * five of them local, so there is no single blob symbol of either name to be
 * compared against.  V.27ter's copies are the pair at .rodata 0xab10 and
 * 0xab0c and they are reached only through `AGCv27_CFG`.  Finding F9144.
 *
 * BETA IS EMITTED FIRST because it sits at the lower address in the object.
 * Both of them sit AFTER `AGCv27_CFG` there and must precede it here, since
 * its initialiser names them; that is the one place this file departs from
 * the object's address order.
 *
 * The values are identical to V.29's pair, and that is worth stating because
 * `AGCv27_CFG.ref_level` is NOT: 12953 against V.29's 11211.  A copy-paste
 * between the two files would have been caught by that field.
 */
static const short AGC_DEF_BETA[2] = { 16384, 3277 };
static const short AGC_DEF_ALPHA[2] = { 16384, 29491 };

/*
 * 24 bytes at .rodata 0xaaf4.  The two pointers at +0x0c and +0x10 are the
 * whole reason this is a struct and not a `short[12]`: `tabdump.py` reads them
 * as -21744 and -21748, which are plausible coefficients and are addends.
 *
 * `f06`, `f08`, `f14` and `f16` keep their offset names: no `fpm_agc` function
 * reads any of them, so their values are known and their meaning is not.  6553
 * at +0x16 is the same 0.2 in Q15 that both V.32 configs carry there, which is
 * why F1621 established that field is not padding.
 */
const struct fpm_agc_cfg AGCv27_CFG = {
	12953,			/* +0x00 ref_level                           */
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
 * ---------------------------------------------------------------------------
 * THE TRANSMIT TABLES, `v27cfg.h`'s own derivation
 */
const short V27TX_PPS_IFILT_2400[120] = {
	   -21,     61,     69,    -21,   -185,   -355,   -453,   -429,
	  -288,    -91,     72,    126,     50,   -105,   -236,   -236,
	   -49,    295,    680,    950,    982,    749,    343,    -58,
	  -264,   -176,    156,    531,    672,    355,   -470,  -1619,
	 -2710,  -3299,  -3065,  -1967,   -298,   1401,   2544,   2755,
	  2055,    898,     14,    103,   1493,   3900,   6417,   7776,
	  6805,   2937,  -3428, -10872, -17286, -20483, -18936, -12362,
	 -1951,   9870,  20092,  26004,  26004,  20092,   9870,  -1951,
	-12362, -18936, -20483, -17286, -10872,  -3428,   2937,   6805,
	  7776,   6417,   3900,   1493,    103,     14,    898,   2055,
	  2755,   2544,   1401,   -298,  -1967,  -3065,  -3299,  -2710,
	 -1619,   -470,    355,    672,    531,    156,   -176,   -264,
	   -58,    343,    749,    982,    950,    680,    295,    -49,
	  -236,   -236,   -105,     50,    126,     72,    -91,   -288,
	  -429,   -453,   -355,   -185,    -21,     69,     61,    -21,
};
const short V27TX_PPS_QFILT_2400[120] = {
	    -5,     52,    166,    273,    302,    217,     36,   -178,
	  -338,   -381,   -299,   -147,    -21,     -8,   -145,   -385,
	  -618,   -713,   -580,   -228,    236,    640,    827,    734,
	   431,    108,    -12,    220,    786,   1479,   1959,   1896,
	  1122,   -260,  -1878,  -3210,  -3783,  -3382,  -2173,   -662,
	   493,    767,     33,  -1311,  -2437,  -2390,   -505,   3221,
	  7968,  12233,  14277,  12730,   7160,  -1612, -11604, -20173,
	-24786, -23828, -17160,  -6243,   6243,  17160,  23828,  24786,
	 20173,  11604,   1612,  -7160, -12730, -14277, -12233,  -7968,
	 -3221,    505,   2390,   2437,   1311,    -33,   -767,   -493,
	   662,   2173,   3382,   3783,   3210,   1878,    260,  -1122,
	 -1896,  -1959,  -1479,   -786,   -220,     12,   -108,   -431,
	  -734,   -827,   -640,   -236,    228,    580,    713,    618,
	   385,    145,      8,     21,    147,    299,    381,    338,
	   178,    -36,   -217,   -302,   -273,   -166,    -52,      5,
};
const short V27TX_PPS_IFILT_4800[60] = {
	   -62,    -12,    -37,   -141,      6,    -56,    -36,    109,
	   -23,    158,    167,    -14,    235,    -93,   -246,     10,
	  -429,   -136,   -163,   -546,    619,     28,    680,   3388,
	  -278,   -636,   1288, -12667, -11245,  19992,  19992, -11245,
	-12667,   1288,   -636,   -278,   3388,    680,     28,    619,
	  -546,   -163,   -136,   -429,     10,   -246,    -93,    235,
	   -14,    167,    158,    -23,    109,    -36,    -56,      6,
	  -141,    -37,    -12,    -62,
};
const short V27TX_PPS_QFILT_4800[60] = {
	    73,     -7,     90,    -34,    -80,      4,   -150,    -45,
	   -37,   -135,    143,     23,     97,    387,    -19,    131,
	   103,   -328,    100,   -640,   -725,     17,  -1642,    813,
	  3536,     50,   5367,   5247, -18350, -17074,  17074,  18350,
	 -5247,  -5367,    -50,  -3536,   -813,   1642,    -17,    725,
	   640,   -100,    328,   -103,   -131,     19,   -387,    -97,
	   -23,   -143,    135,     37,     45,    150,     -4,     80,
	    34,    -90,      7,    -73,
};
const short *const V27TX_PPS_IFILT[2] = {
	V27TX_PPS_IFILT_2400, V27TX_PPS_IFILT_4800
};
const short *const V27TX_PPS_QFILT[2] = {
	V27TX_PPS_QFILT_2400, V27TX_PPS_QFILT_4800
};

const short V27TX_PPS_IMAP_2400[5] = {
	  6144,      0,  -6144,      0,      0,
};
const short V27TX_PPS_QMAP_2400[5] = {
	     0,   6144,      0,  -6144,      0,
};
const short V27TX_PPS_IMAP_4800[9] = {
	  6144,   4344,      0,  -4344,  -6144,  -4344,      0,   4344,
	     0,
};
const short V27TX_PPS_QMAP_4800[9] = {
	     0,   4344,   6144,   4344,      0,  -4344,  -6144,  -4344,
	     0,
};
const short *const V27TX_PPS_IMAP[2] = {
	V27TX_PPS_IMAP_2400, V27TX_PPS_IMAP_4800
};
const short *const V27TX_PPS_QMAP[2] = {
	V27TX_PPS_QMAP_2400, V27TX_PPS_QMAP_4800
};

const int V27TX_PPS_SCALE[2] = { 34767, 34767 };

const unsigned short V27TX_SMC_PMAP_24[4] = { 0, 1, 3, 2 };
const unsigned short V27TX_SMC_PMAP_48[8] = { 1, 0, 2, 3, 6, 7, 5, 4 };
const unsigned short *const V27TX_SMC_PMAP[2] = {
	V27TX_SMC_PMAP_24, V27TX_SMC_PMAP_48
};

const short V27TX_ALT_COUNT[2]       = { 14, 50 };
const short V27TX_EQCOND_COUNT[2]    = { 58, 1074 };
const short V27TX_FRMSIZE[2]         = { 24, 32 };
const short V27TX_NOCARR_SYMBOL[2]   = { 4, 8 };
const short V27TX_PATTERN_ALT[2]     = { 3, 7 };
const short V27TX_PATTERN_CARR[2]    = { 0, 1 };
const short V27TX_PATTERN_SCR1[2]    = { 3, 7 };
const short V27TX_PPS_DOWN_FACT[2]   = { 3, 1 };
const short V27TX_PPS_FILT_LEN[2]    = { 120, 60 };
const short V27TX_PPS_UP_FACT[2]     = { 20, 5 };
const short V27TX_SDM_NUM_BITS[2]    = { 2, 3 };
const short V27TX_SMC_CRR_ADJ[2]     = { 2, 1 };
const short V27TX_SMC_CRR_LEN[2]     = { 4, 8 };
const short V27TX_SMC_PHS_MASK[2]    = { 3, 7 };

/* -------------------------------------------------------------------- .data */

/*
 * The V.27ter tone detector's biquad bank, one per rate: two sections of five
 * shorts, which is what `mtd.tones = 2` over 20 bytes fixes.  `V27RX_create`
 * pairs it with a Q15 threshold of 0x199a and a minimum level of 100, and
 * selects between the two with `cmpw $0x1,0x8(%ebx)` at 997e7 rather than by
 * subscripting -- the one place in the module where the rate is a branch and
 * not an index.
 */
short V27_MTD_COEFF_2400[10] = {
	-13271,  16384,  17334, -19261,  16384,
	-13271,  16384,  -9113,  10126,  16384,
};


short V27_MTD_COEFF_4800[10] = {
	-13271,  16384,  20853, -23170,  16384,
	-13271,  16384, -13389,  14876,  16384,
};


/*
 * The decoder's constellation: the phase angle of each index, and the bits
 * each phase STEP carries.  Four entries at 2400 bit/s and eight at 4800,
 * which is `V27RX_DEC_PHS_MASK` + 1 and is what `V27RX_decision` selects on
 * `dec->eight_phase`.  The angles are `i * 0x8000 / n` exactly -- one full
 * revolution divided into four or eight -- in the units `FPM_atan` produces.
 */
short V27RX_DEC_LAST_PHASE_2400[4] = {
	     0,   8192,  16384,  24576,
};


short V27RX_DEC_LAST_PHASE_4800[8] = {
	     0,   4096,   8192,  12288,  16384,  20480,  24576,  28672,
};


/*
 * The selector, and here the pointer array is `D` -- writable.  Eleven of the
 * fourteen selectors are, and the other three are `R`; the split is the
 * object's and is reproduced rather than tidied.
 */
short *V27RX_DEC_LAST_PHASE[2] = {
	V27RX_DEC_LAST_PHASE_2400, V27RX_DEC_LAST_PHASE_4800
};

short V27RX_DEC_PMAP_2400[4] = {
	     0,      1,      3,      2,
};


short V27RX_DEC_PMAP_4800[8] = {
	     1,      0,      2,      3,      7,      6,      4,      5,
};


short *V27RX_DEC_PMAP[2] = {
	V27RX_DEC_PMAP_2400, V27RX_DEC_PMAP_4800
};

/*
 * The phase index mask, stored into the decoder block's +0x14 at 99c57.
 * 3 and 7, which is four phases and eight.
 */
short V27RX_DEC_PHS_MASK[2] = {
	     3,      7,
};


/*
 * The equaliser PLL's three gain pairs per rate, indexed by
 * `fpm_fse::pll_sel`.  K2 is the integral term and its first entry is zero,
 * so gear 0 is proportional only -- the same shape V.29's and V.32's have.
 * The two rates carry IDENTICAL gains, which is a fact about the object and
 * not a reason to share one table: the object has four symbols and so does
 * this file.
 */
short V27RX_FSE_PLLK2_2400[3] = {
	     0,     32,      0,
};


short V27RX_FSE_PLLK1_2400[3] = {
	 13080,   6096,   1096,
};


short V27RX_FSE_PLLK2_4800[3] = {
	     0,     32,      0,
};


short V27RX_FSE_PLLK1_4800[3] = {
	 13080,   6096,   1096,
};


short *V27RX_FSE_PLLK2[2] = {
	V27RX_FSE_PLLK2_2400, V27RX_FSE_PLLK2_4800
};

short *V27RX_FSE_PLLK1[2] = {
	V27RX_FSE_PLLK1_2400, V27RX_FSE_PLLK1_4800
};

/*
 * `fpm_fse_cfg::clk_inc`, the carrier phase step per input sample, patched in
 * at 99b58.  With `clk_mod` from `V27RX_CRR_TABLE_LEN` below it is what makes
 * the carrier 1800 Hz at BOTH rates, which is V.27ter's own number:
 *
 *     2400 bit/s:  7200 Hz * 1 / 4  = 1800 Hz
 *     4800 bit/s:  8000 Hz * 9 / 40 = 1800 Hz
 */
short V27RX_CRR_ADJUST[2] = {
	     1,      9,
};


/*
 * The carrier phase ramps, `round(i * 32768 / n)` for every entry of both --
 * one full turn of phase in Q16 divided into 4 steps and into 40.  See
 * `v27cfg.h`; the arithmetic is what fixes the element type independently of
 * the byte count.
 */
short V27RX_CRR_TABLE_2400[4] = {
	     0,   8192,  16384,  24576,
};


short V27RX_CRR_TABLE_4800[40] = {
	     0,    819,   1638,   2458,   3277,   4096,   4915,   5734,
	  6554,   7373,   8192,   9011,   9830,  10650,  11469,  12288,
	 13107,  13926,  14746,  15565,  16384,  17203,  18022,  18842,
	 19661,  20480,  21299,  22118,  22938,  23757,  24576,  25395,
	 26214,  27034,  27853,  28672,  29491,  30310,  31130,  31949,
};


/* `fpm_fse_cfg::clk_mod`, patched in at 99b2e: how many entries the ramp
 * beside it holds.  4 and 40, against 8 and 80 bytes. */
short V27RX_CRR_TABLE_LEN[2] = {
	     4,     40,
};


short *V27RX_CRR_TABLE[2] = {
	V27RX_CRR_TABLE_2400, V27RX_CRR_TABLE_4800
};

/*
 * The LMS step sizes, `fpm_fse_cfg::mu[0]` and `mu[1]`, patched in at 99b05
 * and 99b16.  `V27RX_eq_train` runs the equaliser on `mu[0]` and switches it
 * to `mu[1]` when training ends, which is what names them; `mu[2]` is left as
 * the library built-in has it.
 *
 * TRACK IS EMITTED FIRST because it sits at the lower address.
 */
short V27RX_FSE_MU_TRACK[2] = {
	  5244,   4720,
};


short V27RX_FSE_MU_TRAIN[2] = {
	   524,   2360,
};


/*
 * The timing loop's three gain pairs per rate, indexed by `fpm_sre::mode`.
 * Same shape as the equaliser's: K2[0] is zero, so mode 0 is proportional
 * only.  Unlike the equaliser's, these DO differ between the two rates.
 */
short V27RX_SRE_PLLK2_2400[3] = {
	     0,     18,     10,
};


short V27RX_SRE_PLLK1_2400[3] = {
	  1586,   1516,   1168,
};


short V27RX_SRE_PLLK2_4800[3] = {
	     0,     10,     41,
};


short V27RX_SRE_PLLK1_4800[3] = {
	  1586,   1168,   2316,
};


short *V27RX_SRE_PLLK2[2] = {
	V27RX_SRE_PLLK2_2400, V27RX_SRE_PLLK2_4800
};

short *V27RX_SRE_PLLK1[2] = {
	V27RX_SRE_PLLK1_2400, V27RX_SRE_PLLK1_4800
};

/*
 * The clock reference, `sre.clock_len` points per rate -- six at 2400 bit/s
 * and five at 4800, which is `V27RX_SAMP_PER_BAUD`.  X is the cosine leg and
 * Y the sine leg of that many phases equally spaced round one turn, in Q14:
 * at six points 16384, 8192, -8192 ... is cos of 0, 60, 120 degrees and
 * 14189 is sin 60; at five points 5063 is cos 72 and 15582 is sin 72.  That
 * reading is arithmetic on the values themselves and does not depend on the
 * names.  Both legs of both rates sum to zero, which is what makes the
 * correlation a discriminant rather than a level measurement.
 */
short V27RX_YCLOCK_2400[6] = {
	     0,  14189,  14189,      0, -14189, -14189,
};


short V27RX_YCLOCK_4800[5] = {
	     0,  15582,   9630,  -9630, -15582,
};


short *V27RX_YCLOCK[2] = {
	V27RX_YCLOCK_2400, V27RX_YCLOCK_4800
};

short V27RX_XCLOCK_2400[6] = {
	 16384,   8192,  -8192, -16384,  -8192,   8192,
};


short V27RX_XCLOCK_4800[5] = {
	 16384,   5063, -13255, -13255,   5063,
};


short *V27RX_XCLOCK[2] = {
	V27RX_XCLOCK_2400, V27RX_XCLOCK_4800
};

/*
 * The last two selectors, and the only two that live in .data while pointing
 * INTO .rodata -- which is why their element type is `const short *` where
 * the eight above are `short *`.  The object's storage classes again.
 */
const short *V27RX_SRE_FILT[2] = {
	V27RX_SRE_FILT_2400, V27RX_SRE_FILT_4800
};

const short *V27RX_MRF_FILT[2] = {
	V27RX_MRF_FILT_2400, V27RX_MRF_FILT_4800
};
