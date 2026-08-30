/*
 * v32cfg.c -- ITU-T V.32/V.32bis: the DSP blocks' configurations.
 *
 * Reconstructed from dsplibs.o:
 *   AGC_DEF_BETA   .data   0x007660     4   (global -- this TU owns it)
 *   AGC_DEF_ALPHA  .data   0x007664     4   (global -- this TU owns it)
 *   AGCv32Prc_CFG  .data   0x007630    24
 *   AGCv32_CFG     .data   0x007648    24
 *   MRFv32_COFFS   .rodata 0x0070a0   720
 *   MRFv32_CFG     .rodata 0x007370    16
 *   PPSv32_QCOFFS  .rodata 0x007b00   240
 *   PPSv32_ICOFFS  .rodata 0x007c00   240
 *   PPSv32_CFG     .rodata 0x007d00    40
 *
 * These are INSTANCES of structs this tree already has and already tests:
 * `struct fpm_agc_cfg` from `fpm_agc.h` and `struct fpm_mrf_cfg` from
 * `fpm_mrf.h`.  Both were derived from Bell 103's and V.23's copies, and both
 * fit V.32's without a field left over -- which is the check that says the
 * struct is the author's and not ours.  `MRFv32_CFG`'s pointer field lands on
 * `MRFv32_COFFS` and its `taps` on 360, exactly `branches * (taps/branches)`
 * = 9 * 40.
 *
 * THE RESAMPLER RATIO NAMES V.32'S INTERNAL RATE.  9:10 takes the 8000 Hz
 * datapump interface down to **7200 Hz**, which is three samples per symbol
 * at 2400 baud -- V.32's symbol rate.  40 taps per polyphase branch.
 *
 * THE TWO AGC CONFIGURATIONS DIFFER IN EXACTLY TWO FIELDS: the reference
 * level (9061 against 10000) and the measurement block (36 samples against
 * 40).  Everything else, including both smoother pointers, is identical.  40
 * samples at 8000 Hz is 5 ms; 36 is the length `FPM_rms`'s 1/36 scaling was
 * sized for, which is why the non-`Prc` one uses it.  Which block feeds which
 * is not established here -- no consumer of either is written yet -- so the
 * `Prc` name is left as the author's without a gloss.
 *
 * ---------------------------------------------------------------------------
 * D6's table has this pair's alpha wrong, and it is the row the argument
 * rests on
 *
 * `docs/deviations.md` D6 lists nine `AGC_DEF_ALPHA`/`AGC_DEF_BETA` objects
 * and says four of them break unity DC gain, the fourth being
 * `.data:0x7664 / 0x7660 (global)` at "32604 + 2277 = 34881, gain 1.064".
 *
 * The object says otherwise, three independent readings agreeing:
 *
 *     readelf -x .data:  0x7660  0040e508 00401b77
 *                                 ^16384  ^2277  ^16384  ^30491
 *
 * so alpha[1] is **30491**, not 32604, and 30491 + 2277 = **32768 exactly**.
 * This pair is CORRECT.  D6's defect list has three members, not four:
 * `.data:0x7810` (Bell 103) and `.data:0x778c` and `0x7794` (V.23), all at
 * alpha 32604 with beta 1638.
 *
 * The misreading matters more than one row of a table would, because D6's
 * next paragraph derives the *intended* values from it -- "31130 for beta =
 * 1638, 30491 for beta = 2277" -- and 30491 is what this object already
 * holds.  The author did fix this one; the register recorded the fix as
 * another instance of the bug.  Finding F1621.
 */

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/v32cfg.h"
#include "dsplib/v32dec.h"	/* SMCv32_IMAP16 / SMCv32_QMAP16, PPSv32_CFG's
				 * two constellation maps                     */

/*
 * SECTIONS ARE MIRRORED, not chosen.  `nm` gives AGC_DEF_ALPHA, AGC_DEF_BETA,
 * AGCv32_CFG and AGCv32Prc_CFG as `D` -- writable .data -- and MRFv32_CFG and
 * MRFv32_COFFS as `R`.  Nothing writes to any of the four, so `const` would
 * behave identically and the differential test cannot see the difference;
 * they are left non-const anyway so that the file says what the object says.
 */

/*
 * The smoother coefficient pairs, Q15: element 0 is the fast acquisition
 * pair, element 1 the slow tracking pair.  Both configurations below point at
 * element 0, as every AGC configuration in the object does (D6).
 *
 * GLOBAL, unlike the five file-static objects of the same name in other
 * translation units.  Two things say this TU owns them: they sit inside the
 * V.32 run of .data (0x7600 SREv32_PLL_K1 through 0x77xx), immediately after
 * AGCv32_CFG's 24 bytes; and the relocations in AGCv32_CFG and AGCv32Prc_CFG
 * name the symbol, which a translation unit cannot do for another's static.
 */
short AGC_DEF_BETA[2] = { 16384, 2277 };
short AGC_DEF_ALPHA[2] = { 16384, 30491 };

struct fpm_agc_cfg AGCv32Prc_CFG = {
	10000,			/* ref_level      -- output settles at /2 */
	2,			/* acquire_level                          */
	80,			/* squelch_level                          */
	1000,			/* f06                                    */
	1,			/* f08                                    */
	40,			/* block_len      -- 5 ms at 8000 Hz      */
	AGC_DEF_ALPHA,
	AGC_DEF_BETA,
	158,			/* f14                                    */
	6553			/* f16 -- 0.2 in Q15; see fpm_agc.h       */
};

struct fpm_agc_cfg AGCv32_CFG = {
	9061,			/* ref_level                              */
	2,			/* acquire_level                          */
	80,			/* squelch_level                          */
	1000,			/* f06                                    */
	1,			/* f08                                    */
	36,			/* block_len      -- FPM_rms's own length */
	AGC_DEF_ALPHA,
	AGC_DEF_BETA,
	158,			/* f14                                    */
	6553			/* f16                                    */
};

/*
 * 8000 -> 7200 Hz, 9 polyphase branches of 40 taps.  Byte-exact from the
 * object; deriving the design that produced it is deferred with every other
 * coefficient derivation (docs/fastpass.md).
 */
const short MRFv32_COFFS[360] = {
	84, 85, 83, 76, 66, 54, 40, 26,
	13, 4, 0, 0, 7, 21, 41, 67,
	96, 127, 157, 185, 207, 221, 227, 223,
	210, 187, 157, 121, 84, 48, 17, -4,
	-16, -15, 0, 27, 68, 118, 175, 233,
	287, 334, 368, 386, 386, 366, 326, 270,
	200, 123, 43, -30, -94, -141, -166, -167,
	-142, -93, -23, 62, 156, 251, 337, 407,
	452, 467, 448, 394, 308, 194, 59, -87,
	-233, -369, -483, -565, -610, -611, -569, -487,
	-371, -231, -80, 68, 200, 300, 358, 364,
	313, 206, 46, -157, -391, -640, -884, -1107,
	-1289, -1417, -1480, -1472, -1393, -1248, -1052, -819,
	-571, -332, -124, 29, 110, 107, 11, -175,
	-445, -783, -1165, -1565, -1953, -2299, -2575, -2758,
	-2831, -2785, -2623, -2355, -2002, -1593, -1163, -751,
	-395, -132, 6, 1, -159, -473, -927, -1491,
	-2129, -2794, -3435, -3999, -4438, -4710, -4784, -4645,
	-4294, -3749, -3046, -2236, -1382, -556, 169, 722,
	1039, 1074, 798, 203, -691, -1840, -3175, -4607,
	-6029, -7325, -8376, -9068, -9300, -8992, -8089, -6571,
	-4451, -1779, 1360, 4853, 8555, 12309, 15944, 19291,
	22189, 24499, 26105, 26929, 26929, 26105, 24499, 22189,
	19291, 15944, 12309, 8555, 4853, 1360, -1779, -4451,
	-6571, -8089, -8992, -9300, -9068, -8376, -7325, -6029,
	-4607, -3175, -1840, -691, 203, 798, 1074, 1039,
	722, 169, -556, -1382, -2236, -3046, -3749, -4294,
	-4645, -4784, -4710, -4438, -3999, -3435, -2794, -2129,
	-1491, -927, -473, -159, 1, 6, -132, -395,
	-751, -1163, -1593, -2002, -2355, -2623, -2785, -2831,
	-2758, -2575, -2299, -1953, -1565, -1165, -783, -445,
	-175, 11, 107, 110, 29, -124, -332, -571,
	-819, -1052, -1248, -1393, -1472, -1480, -1417, -1289,
	-1107, -884, -640, -391, -157, 46, 206, 313,
	364, 358, 300, 200, 68, -80, -231, -371,
	-487, -569, -611, -610, -565, -483, -369, -233,
	-87, 59, 194, 308, 394, 448, 467, 452,
	407, 337, 251, 156, 62, -23, -93, -142,
	-167, -166, -141, -94, -30, 43, 123, 200,
	270, 326, 366, 386, 386, 368, 334, 287,
	233, 175, 118, 68, 27, 0, -15, -16,
	-4, 17, 48, 84, 121, 157, 187, 210,
	223, 227, 221, 207, 185, 157, 127, 96,
	67, 41, 21, 7, 0, 0, 4, 13,
	26, 40, 54, 66, 76, 8, 85, 84,
};

const struct fpm_mrf_cfg MRFv32_CFG = {
	9,			/* branches                               */
	10,			/* decimate                               */
	MRFv32_COFFS,
	360,			/* taps, across all nine branches         */
	0,			/* pad0a                                  */
	0			/* aux                                    */
};

/*
 * ---------------------------------------------------------------------------
 * THE PULSE SHAPER.
 *
 *   PPSv32_QCOFFS  .rodata 0x007b00  240
 *   PPSv32_ICOFFS  .rodata 0x007c00  240
 *   PPSv32_CFG     .rodata 0x007d00   40
 *
 * `PPSv32_CFG` is an instance of `struct fpm_pps_cfg` and it fits without a
 * field left over, which is the same check `MRFv32_CFG` passes above: ten
 * dwords, four of them relocations, and `coeffs` at +0x20 lands on 120 --
 * exactly the two coefficient tables' length.  `phases` is 10 and
 * `coeffs / phases` is 12 taps per branch.
 *
 * THE FOUR POINTERS ARE READ FROM THE RELOCATIONS AND NOT FROM THE BYTES.
 * All four dwords are zero in the file; `tools/relocscan.py` resolves them to
 * `SMCv32_IMAP16`, `SMCv32_QMAP16`, `PPSv32_ICOFFS` and `PPSv32_QCOFFS` in
 * that order, and `mapped` being 1 is what says the constellation maps are
 * used at all -- the ring carries indices, and `imap`/`qmap` turn them into
 * I and Q.  Reading the bytes instead would have given four nulls and a
 * shaper that filtered nothing.
 *
 * `scale` is 131072, which is 4.0 in the Q15 the field's name implies rather
 * than a gain below unity.  `t_v32data.c` and `t_v32txhdx.c` already carry
 * that number as `PPS_SCALE`, quoted from this table's +0x08 before the table
 * itself was written; they now have the table to quote instead.
 *
 * THE TWO COEFFICIENT TABLES ARE NOT THE SAME FILTER.  `ICOFFS` is symmetric
 * about its centre and `QCOFFS` is ANTI-symmetric -- element k against
 * element 119 - k is +1 times in the first and -1 times in the second -- which
 * is the in-phase/quadrature pair of a passband shaping filter and not two
 * copies of one prototype.  Byte-exact from the object; deriving the design
 * is deferred with every other coefficient derivation.
 */
const short PPSv32_QCOFFS[120] = {
	0, -1, -2, -2, -1, 0, 0, -2, -6, -12,
	-14, -12, -5, 0, 1, -7, -20, -31, -29, -12,
	12, 30, 28, 7, -19, -29, -6, 44, 100, 129,
	112, 59, 8, 3, 65, 169, 258, 273, 193, 60,
	-41, -36, 88, 263, 366, 297, 51, -259, -461, -421,
	-151, 168, 260, -94, -911, -1942, -2754, -2923, -2240, -840,
	840, 2240, 2923, 2754, 1942, 911, 94, -260, -168, 151,
	421, 461, 259, -51, -297, -366, -263, -88, 36, 41,
	-60, -193, -273, -258, -169, -65, -3, -8, -59, -112,
	-129, -100, -44, 6, 29, 19, -7, -28, -30, -12,
	12, 29, 31, 20, 7, -1, 0, 5, 12, 14,
	12, 6, 2, 0, 0, 1, 2, 2, 1, 0,
};

const short PPSv32_ICOFFS[120] = {
	-1, -1, 0, 0, 0, 0, -2, -5, -5, -2,
	3, 10, 13, 9, 1, -4, -1, 12, 34, 50,
	51, 35, 11, 0, 11, 47, 88, 107, 86, 31,
	-27, -50, -20, 48, 106, 104, 20, -113, -226, -251,
	-172, -42, 36, -20, -224, -485, -657, -626, -393, -101,
	36, -143, -629, -1195, -1486, -1190, -216, 1210, 2623, 3499,
	3499, 2623, 1210, -216, -1190, -1486, -1195, -629, -143, 36,
	-101, -393, -626, -657, -485, -224, -20, 36, -42, -172,
	-251, -226, -113, 20, 104, 106, 48, -20, -50, -27,
	31, 86, 107, 88, 47, 11, 0, 11, 35, 51,
	50, 34, 12, -1, -4, 1, 9, 13, 10, 3,
	-2, -5, -5, -2, 0, 0, 0, 0, -1, -1,
};

const struct fpm_pps_cfg PPSv32_CFG = {
	10,			/* phases                                 */
	3,			/* step                                   */
	1,			/* mapped                                 */
	131072,			/* scale                                  */
	0,			/* step_adj                               */
	0,			/* pad0e                                  */
	SMCv32_IMAP16,
	SMCv32_QMAP16,
	PPSv32_ICOFFS,
	PPSv32_QCOFFS,
	120,			/* coeffs -- 12 taps in each of 10 phases */
	0,			/* pad22                                  */
	0			/* aux                                    */
};
