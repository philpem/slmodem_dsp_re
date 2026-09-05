/**
 * @file v29cfg.h
 * @brief ITU-T V.29 (fax): the receiver's coefficient and gain tables.
 *
 * Declarations only.  Every object here is either a plain `short` array or an
 * instance of a struct defined elsewhere (`struct fpm_agc_cfg` in `fpm_agc.h`),
 * so this header defines no type.
 *
 * `V29RX_create` builds four DSP configurations on its stack -- an
 * `fpm_mrf_cfg`, an `fpm_sre_cfg`, an `fpm_fse_cfg` and two `fpm_mtd_cfg` --
 * by copying the library's built-in instance and patching the tables and
 * lengths in, so every table's element count here is confirmed twice: once
 * as the symbol's `st_size` and once as the length field patched in beside
 * the pointer.  A byte count alone would not separate e.g. `short[49]` from
 * `int[24]` plus two bytes; the two readings agree throughout.  Finding
 * F9140.
 *
 * `V29RX_CRR_TABLE`'s 72 entries are a Q16 phase ramp and not just a copied
 * table: `V29RX_CRR_TABLE[i] == round(i * 32768 / 72)`, and at three samples
 * per symbol and 2400 baud the receiver runs at 7200 Hz, so
 * `7200 * 17 / 72 == 1700 Hz` -- V.29's carrier to the digit.  The resampler
 * agrees the same way: `fpm_mrf_cfg` is configured for 9 branches,
 * decimation 10 and 270 taps, the 8000 -> 7200 Hz converter, and
 * `V29RX_MRF_FILT` is symmetric about its centre pair as a linear-phase
 * prototype must be.  Finding F9141.
 *
 * Storage classes are the object's: `nm -S` gives `D` (global, writable,
 * `.data`) for ten of these symbols and `R` for the four in `.rodata`
 * (`AGCv29_CFG`, `V29RX_MRF_FILT`, `V29RX_SRE_FILT`, `V29RX_XB_COFFS`);
 * `t_v29cfg.c` asserts the split.
 *
 * `AGC_DEF_ALPHA` and `AGC_DEF_BETA` are not declared here, on purpose: the
 * object defines each of those two names six times, so V.29's pair
 * (`.rodata` 0xb298/0xb294) is file-static with no `ref_` alias and cannot
 * be compared against the blob by name.  They are `static` in `v29cfg.c`
 * and proved instead through `AGCv29_CFG.alpha`/`.beta`, the fields that
 * actually reach them -- the consumer is the comparison (finding F9058).
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
