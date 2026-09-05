/**
 * @file v27cfg.h
 * @brief ITU-T V.27ter (fax): the receiver's coefficient, gain and
 *        rate-selection tables.
 *
 * Declarations only.  Every object here is either a plain `short` array, an
 * array of two pointers, or an instance of a struct defined elsewhere
 * (`struct fpm_agc_cfg` in `fpm_agc.h`), so this header defines no type.
 *
 * V.27ter runs at two bit rates and the object expresses that as a level of
 * indirection, in three kinds of symbol (finding F9150):
 *
 *   - a SELECTOR: eight bytes, two pointers, `{ ..._2400, ..._4800 }`.
 *     Fourteen of them, confirmed by the relocations inside each selector's
 *     own eight bytes rather than by reading it as `short[4]`.
 *   - a PER-RATE SCALAR: four bytes, `short[2]`.  Eleven of them, forced by
 *     `V27RX_create`'s own sixteen-bit load at each one rather than inferred
 *     from the byte count.
 *   - a TABLE, named `..._2400` or `..._4800`, which is what a selector
 *     points at.  Twenty-eight of them, plus `V27_MTD_COEFF_2400/_4800`,
 *     which the object selects with a branch rather than a subscript.
 *
 * The rate index is 0 for 2400 bit/s and 1 for 4800: `V27RX_create` compares
 * the modem's own bit-rate field against the literals 2400 and 4800 before
 * storing it, so the index is tied to the Recommendation's own two rates and
 * not merely to this file's `_2400`/`_4800` suffixes (finding F9151).
 *
 * Every table's element count here is confirmed twice over -- once as the
 * symbol's `st_size`, once as the length field `V27RX_create` patches beside
 * the pointer when it builds its five DSP sub-configurations on the stack --
 * and the two readings agree throughout; a byte count alone would not have
 * separated e.g. `short[97]` from `int[48]` plus two bytes.  Finding F9150.
 *
 * V.27ter's carrier comes out at 1800 Hz at BOTH rates, which is where the
 * ramp and rail element strides above are confirmed rather than merely
 * assumed: the carrier-table entries are a Q16 phase ramp, the resampler
 * ahead of them is 9/10 at 2400 bit/s and 1/1 at 4800, and both paths reduce
 * to 1800 Hz -- corroborated a second, independent way by the equaliser
 * rails' own zero pattern (each rail is zero exactly where its cosine or
 * sine factor at that carrier is, with no exception over 97 or 81 taps).
 * Finding F9152 has the arithmetic in full.
 *
 * Storage classes are the object's: `nm -S` gives `D` (global, writable,
 * `.data`) for thirty-six of these symbols and `R` for twenty, which is why
 * eleven of the fourteen selectors are writable arrays while three are
 * `const short *const x[2]`, and why two of the writable eleven --
 * `V27RX_SRE_FILT` and `V27RX_MRF_FILT` -- hold `const short *`: they sit in
 * `.data` and point into `.rodata`.
 *
 * `AGC_DEF_ALPHA` and `AGC_DEF_BETA` are not declared here, on purpose: the
 * object defines each of those two names six times, so V.27ter's pair
 * (`.rodata` 0xab10/0xab0c) is file-static with no `ref_` alias and cannot
 * be compared against the blob by name.  They are `static` in `v27cfg.c`
 * and proved instead through `AGCv27_CFG.alpha`/`.beta`, the fields that
 * actually reach them -- the consumer is the comparison (findings
 * F9058/F9144).
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
 * The AGC.  24 bytes at `.rodata` 0xaaf4, and the only table in this file
 * that contains a pointer: two of them, at +0x0c and +0x10, both reaching
 * the file-static Q15 smoother pairs described above.
 */
extern const struct fpm_agc_cfg AGCv27_CFG;

/*
 * The transmit tables `V27TX_create` builds its DSP sub-objects from.  Same
 * shape as the receive tables above -- a per-rate scalar is `short[2]`, a
 * selector two pointers into a `_2400`/`_4800` pair -- confirmed the same
 * way, by the relocations inside each selector's own eight bytes, for every
 * one of `V27TX_PPS_IFILT/QFILT/IMAP/QMAP` and `V27TX_SMC_PMAP`.
 * `V27TX_PPS_SCALE` is the one exception: `V27TX_create` indexes it with a
 * four-byte stride and it carries no relocation, so it is `int[2]`, a
 * literal pair, not a selector.
 *
 * `V27TX_SMC_PMAP_24`/`_48` are `unsigned short`, typed by
 * `struct fpm_smc_cfg::pmap`'s own declared pointee (`fpm_smc.h`).
 * Everything else here is `short`, matching every `TxHdx*V27` read site's
 * own width.
 */
extern const short V27TX_PPS_IFILT_2400[120];
extern const short V27TX_PPS_QFILT_2400[120];
extern const short V27TX_PPS_IFILT_4800[60];
extern const short V27TX_PPS_QFILT_4800[60];
extern const short *const V27TX_PPS_IFILT[2];
extern const short *const V27TX_PPS_QFILT[2];

extern const short V27TX_PPS_IMAP_2400[5];
extern const short V27TX_PPS_QMAP_2400[5];
extern const short V27TX_PPS_IMAP_4800[9];
extern const short V27TX_PPS_QMAP_4800[9];
extern const short *const V27TX_PPS_IMAP[2];
extern const short *const V27TX_PPS_QMAP[2];

extern const int V27TX_PPS_SCALE[2];

extern const unsigned short V27TX_SMC_PMAP_24[4];
extern const unsigned short V27TX_SMC_PMAP_48[8];
extern const unsigned short *const V27TX_SMC_PMAP[2];

extern const short V27TX_ALT_COUNT[2];
extern const short V27TX_EQCOND_COUNT[2];
extern const short V27TX_FRMSIZE[2];
extern const short V27TX_NOCARR_SYMBOL[2];
extern const short V27TX_PATTERN_ALT[2];
extern const short V27TX_PATTERN_CARR[2];
extern const short V27TX_PATTERN_SCR1[2];
extern const short V27TX_PPS_DOWN_FACT[2];
extern const short V27TX_PPS_FILT_LEN[2];
extern const short V27TX_PPS_UP_FACT[2];
extern const short V27TX_SDM_NUM_BITS[2];
extern const short V27TX_SMC_CRR_ADJ[2];
extern const short V27TX_SMC_CRR_LEN[2];
extern const short V27TX_SMC_PHS_MASK[2];

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
