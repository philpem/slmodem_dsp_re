/*
 * v22tab.h -- V.22 / V.22bis: the receiver's static configuration.
 *
 * Everything the V.22 datapump hands to a shared `fpm_*` block at
 * construction time, plus the coefficient banks those configurations point
 * at.  Nothing here has behaviour; it is the data half of `V22FP_create`.
 *
 * WHICH TRANSLATION UNIT EACH BELONGS TO IS NOT RECOVERABLE.  `tools/tumap.py`
 * anchors `v22.c` exactly but reports `v22rxtab.c`, `v22txtab.c` and
 * `V22int.c` in one shared bracket, so the split between them cannot be read
 * off the object.  These are grouped by what they configure instead, and the
 * file name is the author's, not an attribution.
 *
 * TWO OF THESE ARE TYPED AND TWO ARE NOT, and the difference is evidence
 * rather than taste:
 *
 *   - `AGCv22_CFG`, `AGCv22_CFG2` and the three MTD configs carry R_386_32
 *     relocations at known offsets.  A relocation PROVES a pointer field, so
 *     these are written as the `struct fpm_agc_cfg` / `struct fpm_mtd_cfg`
 *     the shared modules already define.
 *   - `TONEv22_CFG`, `TONEv22INIT_CFG` and `V22_CFG` carry none.  Their sizes
 *     match `struct fpm_tone_cfg` (36) and nothing respectively, but size is
 *     not proof, so they stay `short` arrays until the code that reads them
 *     is reconstructed.  See the note in src/pump/v22/v22rxtab.c.
 */

#ifndef DSPLIB_V22TAB_H
#define DSPLIB_V22TAB_H

/* For FPM_IIR_COEFF_PER_SECTION: the MTD banks are ordinary biquad banks. */
#include "dsplib/fpm_iir.h"

struct fpm_agc_cfg;
struct fpm_mtd_cfg;

/*
 * The two AGC configurations.  Same smoother, same gate, different reference
 * level and different measurement block -- 36 samples against 40.
 */
extern const struct fpm_agc_cfg AGCv22_CFG;
extern const struct fpm_agc_cfg AGCv22_CFG2;

/*
 * Multi-tone detector banks.  `_COEF` and `_COEF2` are three biquad sections
 * each; `V22_S1_HC_COEF` is four, and is the bank `MTDs1_CFG` selects.
 */
#define V22_MTD_SECTIONS	3
#define V22_S1_SECTIONS		4

extern const short MTDv22_COEF[V22_MTD_SECTIONS * FPM_IIR_COEFF_PER_SECTION];
extern const short MTDv22_COEF2[V22_MTD_SECTIONS * FPM_IIR_COEFF_PER_SECTION];
extern const short V22_S1_HC_COEF[V22_S1_SECTIONS * FPM_IIR_COEFF_PER_SECTION];

extern const struct fpm_mtd_cfg MTDv22_CFG;
extern const struct fpm_mtd_cfg MTDv22_CFG2;
extern const struct fpm_mtd_cfg MTDs1_CFG;

/*
 * Carrier recovery.  `CRRv22_CLK` is six equally spaced phases -- 0, 5461,
 * 10923, 16384, 21845, 27306.  The obvious sixth-of-0x8000 reading is wrong
 * at one entry; see src/pump/v22/v22rxtab.c.  The two PLL gain tables hold
 * three entries each, one per operating condition.
 */
#define V22_CRR_CLK_STEPS	6
#define V22_CRR_PLL_SETS	3

extern const short CRRv22_CLK[V22_CRR_CLK_STEPS];
extern const short CRRv22_PLL_K1[V22_CRR_PLL_SETS];
extern const short CRRv22_PLL_K2[V22_CRR_PLL_SETS];

/* Eight rising thresholds; in .data, not .rodata, and nothing writes them. */
#define V22_DISCONNECT_THRESHOLDS 8
extern short V22DiconnectThreshTable[V22_DISCONNECT_THRESHOLDS];

/* The tone configurations, untyped -- see the header comment. */
#define V22_TONE_CFG_WORDS	18
extern const short TONEv22_CFG[V22_TONE_CFG_WORDS];
extern const short TONEv22INIT_CFG[V22_TONE_CFG_WORDS];

/* The datapump's own parameter block, untyped -- see the header comment. */
#define V22_CFG_WORDS		14
extern const short V22_CFG[V22_CFG_WORDS];

#endif /* DSPLIB_V22TAB_H */
