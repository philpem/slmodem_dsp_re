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
 *   - `TONEv22_CFG`, `TONEv22INIT_CFG` and `V22_CFG` carry none, and were
 *     `short` arrays until the code that reads them was reconstructed.  IT
 *     NOW IS.  `V22FP_create` copies each to the stack and hands the copy to
 *     a function whose parameter type settles it: the two tone blocks go to
 *     `FPM_TONE_create` with the pointer at +0x10 patched from the library's
 *     own `FPM_TONE_CFG`, which is `struct fpm_tone_cfg::src` and nothing
 *     else, and `V22_CFG` is patched with 16-bit stores at +0x00, +0x02,
 *     +0x04, +0x14, +0x16 and +0x18, a 32-bit one at +0x08 and a
 *     read-modify-write of the byte at +0x11 before being copied into the
 *     object's first 28 bytes.  So they are typed now, and the type of the
 *     last one is `struct v22fp_params` in dsplib/v22fp.h.
 */

#ifndef DSPLIB_V22TAB_H
#define DSPLIB_V22TAB_H

/* For FPM_IIR_COEFF_PER_SECTION: the MTD banks are ordinary biquad banks. */
#include "dsplib/fpm_iir.h"

struct fpm_agc_cfg;
struct fpm_mtd_cfg;
struct fpm_tone_cfg;
struct v22fp_params;

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

/*
 * The tone configurations.  36 bytes each and BYTE-IDENTICAL to each other --
 * see the note in src/pump/v22/v22rxtab.c, and note that the identity means
 * no differential test can tell which of the two feeds which tone object.
 *
 * The word count is kept because t_v22tab.c compares them word for word
 * against the object's copies, which is a check on the bytes rather than on
 * the field mapping.
 */
#define V22_TONE_CFG_WORDS	18
extern const struct fpm_tone_cfg TONEv22_CFG;
extern const struct fpm_tone_cfg TONEv22INIT_CFG;

/*
 * The datapump's own parameter block: the template `V22FP_create` copies to
 * the stack, patches from the caller's configuration, and installs as the
 * first 28 bytes of the object.
 */
#define V22_CFG_WORDS		14
extern const struct v22fp_params V22_CFG;

#endif /* DSPLIB_V22TAB_H */
