/*
 * v27cfg.h -- ITU-T V.27ter (fax): the receiver's coefficient, gain and
 *             rate-selection tables.
 *
 * Declarations only.  Every object here is either a plain `short` array, an
 * array of two pointers, or an instance of a struct defined elsewhere
 * (`struct fpm_agc_cfg` in `fpm_agc.h`), so this header defines no type.
 *
 * ---------------------------------------------------------------------------
 * THE SHAPE, AND IT IS NOT V.29'S
 *
 * V.27ter runs at two bit rates and the object expresses that as a level of
 * indirection.  There are three kinds of symbol here, and finding F9150 is
 * the long form of this section:
 *
 *   - a SELECTOR: eight bytes, TWO POINTERS, `{ ..._2400, ..._4800 }`.
 *     Fourteen of them.  Reading one as `short[4]` yields four plausible
 *     small integers; what settles it is the relocation sweep over the
 *     symbol's own eight bytes, which finds `R_386_32` at +0x00 and +0x04
 *     against two NAMED GLOBALS.  Both targets therefore have to be written
 *     or nothing links (F8492/F8493), and all twenty-eight are.
 *   - a PER-RATE SCALAR: four bytes, `short[2]`.  Eleven of them.  This is
 *     not inferred from the byte count: `V27RX_create` reads each one with
 *     `movzwl base(%reg,%reg,1)`, a SIXTEEN-BIT load at `base + 2*index`,
 *     so there are two 16-bit elements and not one 32-bit one.
 *   - a TABLE, named `..._2400` or `..._4800`, which is what a selector
 *     points at.  Twenty-eight of them, plus `V27_MTD_COEFF_2400/_4800`,
 *     which the object selects with a branch rather than a subscript.
 *
 * THE RATE INDEX IS 0 FOR 2400 bit/s AND 1 FOR 4800, AND THAT IS MEASURED.
 * The names are not the evidence.  `V27RX_create` at 99794 loads the modem's
 * own +0x04, compares it against 0x960 and 0x12c0 -- 2400 and 4800 in
 * decimal, the two bit rates the Recommendation defines -- and stores 0 for
 * the first and 1 for the second into the shared block's +0x08.  Every later
 * `movswl 0x8(%ecx)` is that index.  The `cmpw $0x1,0x8(%ebx)` at 997e7,
 * which takes `V27_MTD_COEFF_4800` on equality, says it a second time.
 * Finding F9151.
 *
 * ---------------------------------------------------------------------------
 * WHAT TYPES THESE, AND IT IS NOT THE BYTE COUNT ALONE
 *
 * `V27RX_create` builds four DSP configurations on its stack -- an
 * `fpm_mrf_cfg`, an `fpm_sre_cfg`, an `fpm_fse_cfg` and two `fpm_mtd_cfg` --
 * by copying the library's built-in instance and patching the tables AND the
 * lengths in.  So every table's element count is written down twice in the
 * object: once as `st_size`, and once as the length field the constructor
 * stores beside the pointer.  Both readings agree for every one of them:
 *
 *   table                     st_size    the count the constructor writes
 *   V27RX_FSE_IFILT_2400/Q        194    fse.taps    = FSE_FILT_LEN[0] =  97
 *   V27RX_FSE_IFILT_4800/Q        162    fse.taps    = FSE_FILT_LEN[1] =  81
 *   V27RX_CRR_TABLE_2400            8    fse.clk_mod = CRR_TABLE_LEN[0]=   4
 *   V27RX_CRR_TABLE_4800           80    fse.clk_mod = CRR_TABLE_LEN[1]=  40
 *   V27RX_MRF_FILT_2400           540    mrf.taps    = MRF_FILT_LEN[0] = 270
 *   V27RX_MRF_FILT_4800            72    mrf.taps    = MRF_FILT_LEN[1] =  36
 *   V27RX_SRE_FILT_2400           542    sre.coeffs  = SRE_FILT_LEN[0] = 270
 *   V27RX_SRE_FILT_4800           402    sre.coeffs  = SRE_FILT_LEN[1] = 200
 *                                          ... proto holds coeffs + 1
 *   V27RX_XCLOCK/YCLOCK_2400       12    sre.clock_len = SAMP_PER_BAUD[0]= 6
 *   V27RX_XCLOCK/YCLOCK_4800       10    sre.clock_len = SAMP_PER_BAUD[1]= 5
 *   V27RX_XB_COFFS_2400/_4800      22    FPM_SRE_DISC  = 11
 *   V27RX_SRE_PLLK1/K2_*            6    FPM_SRE_MODES = 3
 *   V27RX_FSE_PLLK1/K2_*            6    fpm_fse_cfg's three gains
 *   V27RX_DEC_PMAP_2400             8    DEC_PHS_MASK[0] + 1 = 4
 *   V27RX_DEC_PMAP_4800            16    DEC_PHS_MASK[1] + 1 = 8
 *   V27RX_DEC_LAST_PHASE_2400/48  8/16   the same two counts
 *   V27_MTD_COEFF_2400/_4800       20    mtd.tones = 2, 5 shorts a section
 *
 * A byte count alone would not separate `short[97]` from `int[48]` plus two
 * bytes, and wave 1's SGD failure was a layout error rather than an
 * arithmetic one, so the second reading is not a nicety.  Every stride here
 * is 2.
 *
 * ---------------------------------------------------------------------------
 * THE CARRIER IS 1800 Hz AT BOTH RATES, AND THE ARITHMETIC CLOSES ON IT
 *
 * This is where the element STRIDE comes from for the ramp and the rails,
 * which is the one thing a byte-for-byte copy cannot give.
 *
 * `V27RX_CRR_TABLE_2400[i]` is `round(i * 32768 / 4)` and `..._4800[i]` is
 * `round(i * 32768 / 40)` -- one full turn of phase in Q16, in 4 steps and in
 * 40 -- and `V27RX_create` installs each as `fpm_fse_cfg::clk` with
 * `clk_mod` from `V27RX_CRR_TABLE_LEN` and `clk_inc` from
 * `V27RX_CRR_ADJUST`.  The resampler in front of it is 9/10 at 2400 bit/s
 * and 1/1 at 4800, so:
 *
 *     2400 bit/s:  8000 * 9/10 = 7200 Hz,  7200 * 1 / 4  = 1800 Hz
 *     4800 bit/s:  8000 * 1/1  = 8000 Hz,  8000 * 9 / 40 = 1800 Hz
 *
 * 1800 Hz is V.27ter's carrier as the Recommendation defines it, for both
 * rates.  The symbol rates fall out of the same three numbers: 7200/6 is
 * 1200 baud at four phases (two bits a symbol, 2400 bit/s) and 8000/5 is
 * 1600 baud at eight phases (three bits, 4800 bit/s), and the phase counts
 * are `V27RX_DEC_PHS_MASK` + 1 independently.
 *
 * THE EQUALISER'S ZERO PATTERN IS THE SAME FACT SEEN AGAIN.  Each rail is a
 * real prototype times a cosine or a sine at the carrier, so it is zero
 * exactly where its trigonometric factor is.  At 1800/7200 = 1/4 the I rail
 * is zero at every odd tap and the Q rail at every even one; at 1800/8000 =
 * 9/40 the period is 20 taps and the I rail is zero at n == 10 (mod 20)
 * while the Q rail is zero at n == 0 (mod 20).  All four hold with no
 * exception in either direction over 97 and 81 taps, which no other reading
 * of the element width would produce.  `t_v27cfg.c` asserts them, and finding
 * F9152 records the derivation.
 *
 * ---------------------------------------------------------------------------
 * STORAGE CLASSES ARE THE OBJECT'S
 *
 * `nm -S` gives `D` -- global, writable, `.data` -- for thirty-six of these
 * and `R` for twenty.  That split is not ours to tidy; it is what a
 * differential comparison sees, and it is why eleven of the fourteen
 * selectors are writable arrays while three are `const short *const x[2]`,
 * and why two of the writable eleven -- `V27RX_SRE_FILT` and
 * `V27RX_MRF_FILT` -- hold `const short *`, because they sit in `.data` and
 * point into `.rodata`.
 *
 * `AGC_DEF_ALPHA` AND `AGC_DEF_BETA` ARE NOT DECLARED HERE, ON PURPOSE.  The
 * object defines each of those two names SIX times -- five local, one global
 * -- so V.27ter's pair at .rodata 0xab10 and 0xab0c is file-static, has no
 * `ref_` alias, and cannot be compared against the blob by name.  They are
 * `static` in `v27cfg.c` and are proved instead through `AGCv27_CFG.alpha`
 * and `.beta`, which is what the pointers actually reach.  This is the F9058
 * and F9144 case: the consumer is the comparison.
 */

#ifndef DSPLIB_V27CFG_H
#define DSPLIB_V27CFG_H

#include "dsplib/fpm_sre.h"	/* FPM_SRE_DISC, FPM_SRE_MODES */

#ifdef __cplusplus
extern "C" {
#endif

struct fpm_agc_cfg;

/* ------------------------------------------------------------------ .rodata */

/* `fpm_fse_cfg::taps` per rate: 97 and 81. */
extern const short V27RX_FSE_FILT_LEN[2];

/* The equaliser's initial coefficients, `FSE_FILT_LEN[rate]` taps each. */
extern const short V27RX_FSE_QFILT_4800[81];
extern const short V27RX_FSE_QFILT_2400[97];
extern const short *const V27RX_FSE_QFILT[2];
extern const short V27RX_FSE_IFILT_4800[81];
extern const short V27RX_FSE_IFILT_2400[97];
extern const short *const V27RX_FSE_IFILT[2];

/*
 * `fpm_sre_cfg::clock_len`, `fpm_fse_cfg::interp` and (tripled)
 * `fpm_sre_cfg::rms_len`.  Six and five.
 */
extern const short V27RX_SAMP_PER_BAUD[2];

/* The symbol-timing discriminant, FPM_SRE_DISC coefficients per rate. */
extern const short V27RX_XB_COFFS_2400[FPM_SRE_DISC];
extern const short V27RX_XB_COFFS_4800[FPM_SRE_DISC];
extern const short *const V27RX_XB_COFFS[2];

/* The timing-recovery prototypes: ten branches of 27 and of 20 taps, plus
 * one, because the interpolator reads `proto[i+1]` at `i == coeffs-1`. */
extern const short V27RX_SRE_FILT_2400[271];
extern const short V27RX_SRE_FILT_4800[201];
extern const short V27RX_SRE_FILT_LEN[2];

/* The input resamplers: 9 branches of 30 taps at 2400 bit/s, and a single
 * 36-tap band filter with no rate change at 4800. */
extern const short V27RX_MRF_FILT_2400[270];
extern const short V27RX_MRF_FILT_4800[36];
extern const short V27RX_MRF_FILT_LEN[2];
extern const short V27RX_MRF_DOWN[2];
extern const short V27RX_MRF_UP[2];

/*
 * The AGC.  24 bytes at .rodata 0xaaf4, and the only table in this file that
 * contains a pointer: two of them, at +0x0c and +0x10, found by sweeping the
 * relocations whose offset falls inside the symbol's own range.  Both targets
 * are the file-static Q15 smoother pairs described above.
 */
extern const struct fpm_agc_cfg AGCv27_CFG;

/* -------------------------------------------------------------------- .data */

/* The V.27ter tone detector's biquad bank: two sections of five shorts. */
extern short V27_MTD_COEFF_2400[10];
extern short V27_MTD_COEFF_4800[10];

/* The decoder's constellation: `i * 0x8000 / n` for four phases and eight. */
extern short V27RX_DEC_LAST_PHASE_2400[4];
extern short V27RX_DEC_LAST_PHASE_4800[8];
extern short *V27RX_DEC_LAST_PHASE[2];

/* The bits each phase STEP carries. */
extern short V27RX_DEC_PMAP_2400[4];
extern short V27RX_DEC_PMAP_4800[8];
extern short *V27RX_DEC_PMAP[2];

/* The phase index mask: 3 and 7, which is four phases and eight. */
extern short V27RX_DEC_PHS_MASK[2];

/* The equaliser PLL's three gain pairs per rate; the two rates agree. */
extern short V27RX_FSE_PLLK2_2400[3];
extern short V27RX_FSE_PLLK1_2400[3];
extern short V27RX_FSE_PLLK2_4800[3];
extern short V27RX_FSE_PLLK1_4800[3];
extern short *V27RX_FSE_PLLK2[2];
extern short *V27RX_FSE_PLLK1[2];

/* The carrier ramps and the two numbers that step them; see the header
 * comment for why both rates come out at 1800 Hz. */
extern short V27RX_CRR_ADJUST[2];
extern short V27RX_CRR_TABLE_2400[4];
extern short V27RX_CRR_TABLE_4800[40];
extern short V27RX_CRR_TABLE_LEN[2];
extern short *V27RX_CRR_TABLE[2];

/* `fpm_fse_cfg::mu[0]` and `mu[1]`: the training and tracking LMS steps. */
extern short V27RX_FSE_MU_TRACK[2];
extern short V27RX_FSE_MU_TRAIN[2];

/* The timing loop's three gain pairs per rate; these two rates DIFFER. */
extern short V27RX_SRE_PLLK2_2400[FPM_SRE_MODES];
extern short V27RX_SRE_PLLK1_2400[FPM_SRE_MODES];
extern short V27RX_SRE_PLLK2_4800[FPM_SRE_MODES];
extern short V27RX_SRE_PLLK1_4800[FPM_SRE_MODES];
extern short *V27RX_SRE_PLLK2[2];
extern short *V27RX_SRE_PLLK1[2];

/* The clock reference, `SAMP_PER_BAUD[rate]` phases round one turn in Q14. */
extern short V27RX_YCLOCK_2400[6];
extern short V27RX_YCLOCK_4800[5];
extern short *V27RX_YCLOCK[2];
extern short V27RX_XCLOCK_2400[6];
extern short V27RX_XCLOCK_4800[5];
extern short *V27RX_XCLOCK[2];

/* The two selectors that live in `.data` and point into `.rodata`. */
extern const short *V27RX_SRE_FILT[2];
extern const short *V27RX_MRF_FILT[2];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V27CFG_H */
