/*
 * v29cfg.h -- ITU-T V.29 (fax): the receiver's coefficient and gain tables.
 *
 * Declarations only.  Every object here is either a plain `short` array or an
 * instance of a struct defined elsewhere (`struct fpm_agc_cfg` in `fpm_agc.h`),
 * so this header defines no type.
 *
 * WHAT TYPES THESE, AND IT IS NOT THE BYTE COUNT ALONE.  `V29RX_create` builds
 * four DSP configurations on its stack -- an `fpm_mrf_cfg`, an `fpm_sre_cfg`,
 * an `fpm_fse_cfg` and two `fpm_mtd_cfg` -- by copying the library's built-in
 * instance and patching the tables and lengths in.  Every table below is the
 * value of one of those pointer fields, and the length field patched beside it
 * gives the element COUNT independently of `st_size`:
 *
 *   table                 st_size   count the consumer writes    stride
 *   V29RX_FSE_IFILT           98    fse.taps      = 0x31 =  49   2  short
 *   V29RX_FSE_QFILT           98    fse.taps      = 0x31 =  49   2  short
 *   V29RX_CRR_TABLE          144    fse.clk_mod   = 0x48 =  72   2  short
 *   V29RX_MRF_FILT           540    mrf.taps      = 0x10e = 270  2  short
 *   V29RX_SRE_FILT           362    sre.coeffs    = 0xb4 = 180   2  short
 *                                     ... proto holds coeffs + 1
 *   V29RX_XCLOCK               6    sre.clock_len = 3            2  short
 *   V29RX_YCLOCK               6    sre.clock_len = 3            2  short
 *   V29RX_XB_COFFS            22    FPM_SRE_DISC  = 11           2  short
 *   V29RX_SRE_PLLK1/PLLK2      6    FPM_SRE_MODES = 3            2  short
 *   V29RX_FSE_PLLK1/PLLK2      6    fpm_fse_cfg's three gains    2  short
 *   V29_MTD_COEFF             20    mtd.tones = 2, 5 shorts each 2  short
 *
 * Two independent readings agree for every one of them, which is what the
 * stride rests on -- a byte count alone would not separate `short[49]` from
 * `int[24]` plus two bytes, and wave 1's SGD failure was a layout error rather
 * than an arithmetic one.
 *
 * THE 72-ENTRY CARRIER TABLE IS DERIVED, NOT JUST COPIED.  `V29RX_CRR_TABLE[i]`
 * is `round(i * 32768 / 72)` for every one of its 72 entries -- a full turn of
 * phase in Q16 -- and `V29RX_create` sets `clk_mod = 72` and `clk_inc = 17`
 * beside it.  At three samples per symbol and 2400 baud the receiver runs at
 * 7200 Hz, and 7200 * 17 / 72 is 1700 Hz, which is V.29's carrier.  The
 * arithmetic closes on the recommendation's own number, so reading this table
 * as a phase ramp is measured and not inferred from its shape.
 *
 * THE RESAMPLER AGREES THE SAME WAY.  `V29RX_create` gives the `fpm_mrf_cfg`
 * 9 branches, decimation 10 and 270 taps, so it is the 8000 -> 7200 Hz
 * converter, 30 taps per branch, and `V29RX_MRF_FILT` is symmetric about its
 * centre pair (24841, 24841) as a linear-phase prototype must be.
 *
 * STORAGE CLASSES ARE THE OBJECT'S.  `nm -S` gives `D` -- global, writable,
 * `.data` -- for ten of these and `R` for the four in `.rodata`.  That split is
 * not ours to tidy: it is what a differential test compares, and `t_v29cfg.c`
 * asserts the sections.
 *
 *   D  V29_MTD_COEFF V29RX_CRR_TABLE V29RX_FSE_IFILT V29RX_FSE_QFILT
 *      V29RX_FSE_PLLK1 V29RX_FSE_PLLK2 V29RX_SRE_PLLK1 V29RX_SRE_PLLK2
 *      V29RX_XCLOCK V29RX_YCLOCK
 *   R  AGCv29_CFG V29RX_MRF_FILT V29RX_SRE_FILT V29RX_XB_COFFS
 *
 * `AGC_DEF_ALPHA` AND `AGC_DEF_BETA` ARE NOT DECLARED HERE, ON PURPOSE.  The
 * object defines each of those two names SIX times -- `nm` shows five local
 * (`d`/`r`) copies and one global -- so V.29's pair at .rodata 0xb298 and
 * 0xb294 is file-static, has no `ref_` alias, and cannot be compared against
 * the blob by name.  They are `static` in `v29cfg.c` and are proved instead
 * through `AGCv29_CFG.alpha` / `.beta`, which is what the pointers actually
 * reach.  This is the F9058 case: the consumer is the comparison.
 */

#ifndef DSPLIB_V29CFG_H
#define DSPLIB_V29CFG_H

#include "dsplib/fpm_sre.h"	/* FPM_SRE_DISC, FPM_SRE_MODES */

#ifdef __cplusplus
extern "C" {
#endif

struct fpm_agc_cfg;

/*
 * The AGC.  24 bytes at .rodata 0xb27c, and the only table in this file that
 * contains a pointer: two of them, at +0x0c and +0x10, found by sweeping the
 * relocations whose offset falls inside the symbol's own range.  Both targets
 * are the file-static Q15 smoother pairs described above.
 */
extern const struct fpm_agc_cfg AGCv29_CFG;

/*
 * The V.29 tone detector's biquad bank: two sections of five shorts, which is
 * what `mtd.tones = 2` over 20 bytes fixes.  `V29RX_create` pairs it with a
 * Q15 threshold of 0x199a and a minimum level of 50.
 */
extern short V29_MTD_COEFF[10];

/* The equaliser: 49 taps each, and the carrier phase ramp described above. */
extern short V29RX_FSE_IFILT[49];
extern short V29RX_FSE_QFILT[49];
extern short V29RX_CRR_TABLE[72];
extern short V29RX_FSE_PLLK1[3];
extern short V29RX_FSE_PLLK2[3];

/* The 8000 -> 7200 Hz resampler: 9 branches of 30 taps. */
extern const short V29RX_MRF_FILT[270];

/* The symbol-rate recovery loop: 10 branches of 18 taps, plus one. */
extern const short V29RX_SRE_FILT[181];
extern const short V29RX_XB_COFFS[FPM_SRE_DISC];
extern short V29RX_SRE_PLLK1[FPM_SRE_MODES];
extern short V29RX_SRE_PLLK2[FPM_SRE_MODES];
extern short V29RX_XCLOCK[3];
extern short V29RX_YCLOCK[3];

/*
 * The slicer's constellation, read only by `V29RX_decision`.  Sixteen points
 * with the eight-point 7200 bit/s subset FIRST, because the slicer's search
 * bound is 8 or 16 depending on the rate selector and it always starts at
 * zero.  Every amplitude is 2048 times one of V.29's own four -- 3, 5,
 * sqrt(2) and 3*sqrt(2) -- which is what types them as signed 16-bit
 * amplitudes rather than as anything else of the same width.  `v29cfg.c`
 * carries the derivation and the note on why the object's `movzwl` does not
 * make them unsigned.
 */
extern short V29RX_DEC_IMAP[16];
extern short V29RX_DEC_QMAP[16];
extern short V29RX_DEC_ANGLE[16];
extern short V29RX_DEC_MAG[16];
extern short V29RX_DEC_PMAP[8];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V29CFG_H */
