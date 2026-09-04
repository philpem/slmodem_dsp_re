/**
 * @file v17cfg.h
 * @brief ITU-T V.17 (fax): the receiver's coefficient, gain and
 *        constellation tables.
 *
 * Declarations only.  Every object here is either a plain `short` array or an
 * instance of a struct defined elsewhere (`struct fpm_agc_cfg` in
 * `fpm_agc.h`), so this header defines no type.
 *
 * Each table's element type and count come from `V17RX_create` (.text
 * 0x96eb0, 3,201 bytes), which builds five DSP configurations on its stack --
 * two `fpm_mtd_cfg`, an `fpm_mrf_cfg`, an `fpm_sre_cfg` and an `fpm_fse_cfg`
 * -- by copying the library's built-in instance and patching the tables and
 * lengths in.  Every table below is one of those pointer fields, and the
 * length field patched beside it gives the element count independently of
 * `st_size`, agreeing with the byte count for all fourteen: see finding
 * F9170 for the per-table stride table.
 *
 * The carrier table (`CRRv17_CLK`), the resampler (`MRFv17_COFFS`) and the
 * four trellis constellations (`VTBv17_?MAP*`) are each independently
 * derived from the arithmetic V.17 implies, not just copied from their byte
 * layout -- see findings F9171 (`CRRv17_CLK[i] = round(i*32768/4)`, closing
 * on V.17's 1800 Hz carrier), and F9172 (the four constellations are
 * V.32bis' own, byte for byte, each carrying one spare entry the decoder
 * cannot reach, which is why they are declared N+1 rather than N).
 *
 * `V17RX_create` reads one flag -- the receiver object's +0x10, copied from
 * the modem's +0x14 -- that switches the FSE/SRE equaliser gains and
 * settle counts between a cold-start pair and a pre-loaded (`_S`) pair;
 * what the suffix letter stands for is not established.  See finding F9173
 * for the full switch table, and F9174 for why the two SRE arrays are
 * numerically identical (the switch there only ever changes `settle`).
 *
 * Storage classes are the object's own: `nm -S` gives `D` (global, writable,
 * `.data`) for ten of these tables and `R` (`.rodata`) for the other
 * fourteen, which `t_v17cfg.c` asserts alongside the sizes.
 *
 *   D  V17_MTD_COEFF CRRv17_CLK CRRv17_PLL_K1_S CRRv17_PLL_K2 CRRv17_PLL_K1
 *      SREv17_PLL_K1_S SREv17_PLL_K2 SREv17_PLL_K1 SREv17_yCLOCK
 *      SREv17_xCLOCK
 *   R  AGCv17_CFG FSEv17_QCOFF FSEv17_ICOFF SREv17_COFFS SREv17_XB_COFFS
 *      MRFv17_COFFS VTBv17_{Q,I}MAP{128,64,32,16T}
 *
 * `AGC_DEF_ALPHA` and `AGC_DEF_BETA` are not declared here, on purpose: the
 * object defines each of those two names six times (five local `static`
 * copies and one global), so V.17's pair at .rodata 0x9e2c/0x9e28 has no
 * `ref_` alias and cannot be compared against the blob by name.  They are
 * `static` in `v17cfg.c` and are proved instead through `AGCv17_CFG.alpha` /
 * `.beta`, which is what the pointers actually reach (findings F9058/F9144).
 */

#ifndef DSPLIB_V17CFG_H
#define DSPLIB_V17CFG_H

#include "dsplib/fpm_sre.h"	/* FPM_SRE_DISC, FPM_SRE_MODES */

#ifdef __cplusplus
extern "C" {
#endif

struct fpm_agc_cfg;

/*
 * The AGC.  24 bytes at .rodata 0x9e10, and the only symbol in this file that
 * contains a pointer: two of them, at +0x0c and +0x10, found by sweeping the
 * relocations whose offset falls inside the symbol's own range.  Both targets
 * are the file-static Q15 smoother pairs described above.
 */
extern const struct fpm_agc_cfg AGCv17_CFG;

/*
 * The V.17 tone detector's biquad bank: two sections of five shorts, which is
 * what `mtd.tones = 2` over 20 bytes fixes.  `V17RX_create` pairs it with a
 * Q15 threshold of 0x4ccd and a minimum level of 100.
 */
extern short V17_MTD_COEFF[10];

/* The equaliser: 49 taps each, and the carrier phase ramp described above. */
extern const short FSEv17_QCOFF[49];
extern const short FSEv17_ICOFF[49];
extern short CRRv17_CLK[4];
extern short CRRv17_PLL_K1_S[3];
extern short CRRv17_PLL_K2[3];
extern short CRRv17_PLL_K1[3];

/* The 8000 -> 7200 Hz resampler: 9 branches of 40 taps. */
extern const short MRFv17_COFFS[360];

/* The symbol-rate recovery loop: 10 branches of 18 taps, plus one. */
extern const short SREv17_COFFS[181];
extern const short SREv17_XB_COFFS[FPM_SRE_DISC];
extern short SREv17_PLL_K1_S[FPM_SRE_MODES];
extern short SREv17_PLL_K2[FPM_SRE_MODES];
extern short SREv17_PLL_K1[FPM_SRE_MODES];
extern short SREv17_yCLOCK[3];
extern short SREv17_xCLOCK[3];

/*
 * The trellis decoder's four constellations, N points and one spare entry
 * each.  16 points is 7200 bit/s, 32 is 9600, 64 is 12000 and 128 is 14400 --
 * the association is `V17RX_create`'s own, through the rate selector it
 * derives from the baud rate and then uses to pick `nsub`.
 */
extern const short VTBv17_QMAP128[129];
extern const short VTBv17_IMAP128[129];
extern const short VTBv17_QMAP64[65];
extern const short VTBv17_IMAP64[65];
extern const short VTBv17_QMAP32[33];
extern const short VTBv17_IMAP32[33];
extern const short VTBv17_QMAP16T[17];
extern const short VTBv17_IMAP16T[17];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V17CFG_H */
