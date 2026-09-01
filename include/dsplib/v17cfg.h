/*
 * v17cfg.h -- ITU-T V.17 (fax): the receiver's coefficient, gain and
 *             constellation tables.
 *
 * Declarations only.  Every object here is either a plain `short` array or an
 * instance of a struct defined elsewhere (`struct fpm_agc_cfg` in
 * `fpm_agc.h`), so this header defines no type.
 *
 * WHAT TYPES THESE, AND IT IS NOT THE BYTE COUNT ALONE.  `V17RX_create`
 * (.text 0x96eb0, 3,201 bytes) builds five DSP configurations on its stack --
 * two `fpm_mtd_cfg`, an `fpm_mrf_cfg`, an `fpm_sre_cfg` and an
 * `fpm_fse_cfg` -- by copying the library's built-in instance and patching
 * the tables and the lengths in.  Every table below is the value of one of
 * those pointer fields, and the length field patched beside it gives the
 * element COUNT independently of `st_size`:
 *
 *   table                 st_size   count the consumer writes    stride
 *   FSEv17_ICOFF              98    fse.taps    = 0x31  =  49   2  short
 *   FSEv17_QCOFF              98    fse.taps    = 0x31  =  49   2  short
 *   CRRv17_CLK                 8    fse.clk_mod = 4            2  short
 *   CRRv17_PLL_K1/K1_S/K2      6    fpm_fse_cfg's three gains  2  short
 *   MRFv17_COFFS             720    mrf.taps    = 0x168 = 360  2  short
 *   SREv17_COFFS             362    sre.coeffs  = 0xb4  = 180  2  short
 *                                     ... proto holds coeffs + 1
 *   SREv17_XB_COFFS           22    FPM_SRE_DISC  = 11         2  short
 *   SREv17_PLL_K1/K1_S/K2      6    FPM_SRE_MODES = 3          2  short
 *   SREv17_xCLOCK/yCLOCK       6    sre.clock_len = 3          2  short
 *   V17_MTD_COEFF             20    mtd.tones = 2, 5 shorts    2  short
 *   VTBv17_?MAP16T            34    vtb.nsub = 1 ->  16 points 2  short
 *   VTBv17_?MAP32             66    vtb.nsub = 2 ->  32 points 2  short
 *   VTBv17_?MAP64            130    vtb.nsub = 3 ->  64 points 2  short
 *   VTBv17_?MAP128           258    vtb.nsub = 4 -> 128 points 2  short
 *
 * The two readings agree for every one of them, which is what the stride
 * rests on -- a byte count alone would not separate `short[49]` from
 * `int[24]` plus two bytes, and wave 1's SGD failure was a layout error
 * rather than an arithmetic one.
 *
 * THE CARRIER REFERENCE IS DERIVED, NOT JUST COPIED.  `CRRv17_CLK[i]` is
 * `round(i * 32768 / 4)` for all four of its entries -- a full turn of phase
 * in Q16 -- and `V17RX_create` sets `clk_mod = 4` and `clk_inc = 1` beside
 * it.  At three samples per symbol and 2400 baud the receiver runs at
 * 7200 Hz, and 7200 * 1 / 4 is 1800 Hz, which is V.17's carrier.  The
 * arithmetic closes on the Recommendation's own number, so reading this table
 * as a phase ramp is measured and not inferred from its shape.  This is
 * F9141's argument for V.29's 72-entry ramp, made again with four entries.
 *
 * THE RESAMPLER AGREES THE SAME WAY.  `V17RX_create` gives the `fpm_mrf_cfg`
 * 9 branches, decimation 10 and 360 taps, so it is the 8000 -> 7200 Hz
 * converter at 40 taps a branch, and `MRFv17_COFFS` is symmetric about its
 * centre pair (24237, 24237) as a linear-phase prototype must be.
 *
 * THE FOUR CONSTELLATIONS ARE V.32bis' OWN, AND THEY CARRY ONE SPARE ENTRY.
 * `V17RX_create` maps the rate selector (7200/9600/12000/14400 baud) onto
 * `vtb.nsub` 1/2/3/4, sets `grid = 2 * nsub` and
 * `mask = (1 << (nsub + 2)) - 1`, and installs `VTB_BOUND_*` and
 * `VTB_REGION_*` -- which are V.32's tables, already written in
 * `src/pump/v32/v32vtb_tables.c` and shared verbatim.  Each map's first N
 * entries are byte-identical to the matching `VTBv32_?MAP*`.  What differs is
 * the SIZE: V.32's symbols are 2*N bytes and V.17's are 2*N+2, and the extra
 * short is zero in all eight.  `VTB_decoder` cannot reach it -- it indexes
 * these by a point index out of `bound[]`, bounded at N-1 by `nsub` -- so the
 * DECODER fixes the count at N and the SYMBOL fixes it at N+1.  The arrays
 * are declared N+1 because that is the object's size; an array declared N
 * would compare equal over its own length and still be two bytes short.
 *
 * TWO K1 ARRAYS AND ONE K2, IN BOTH LOOPS, AND THE SWITCH IS NOT A BIT RATE.
 * `V17RX_create` reads one flag -- the receiver object's +0x10, copied from
 * the modem's +0x14, which the caller's parameter block supplied -- and it
 * governs three things at once:
 *
 *   flag non-zero                       flag zero
 *   ------------------------------      -------------------------------
 *   fse.icoff/qcoff = the CALLER's      = FSEv17_ICOFF / FSEv17_QCOFF
 *      arrays (modem +0x18 / +0x1c)
 *   fse.pll_k1 = CRRv17_PLL_K1_S        = CRRv17_PLL_K1
 *   fse.train_sym = 256                 = 1500
 *   sre.pll_k1 = SREv17_PLL_K1_S        = SREv17_PLL_K1
 *   sre.settle = 48                     = 85
 *
 * So the `_S` arrays are the ones used when the caller hands the receiver a
 * pre-loaded equaliser rather than a cold one, and the integral gains
 * (`*_PLL_K2`) are shared between the two cases, which is why there is one of
 * each.  That is what the object does; what the suffix letter STANDS for is
 * not written anywhere in the object and is not asserted here.
 *
 * The two SRE arrays hold identical values (2336, 3049, 3049) at two
 * different addresses, so for the timing loop the switch changes only
 * `settle`.  The two carrier arrays differ in gear 0 alone, 602 against
 * 10347.
 *
 * STORAGE CLASSES ARE THE OBJECT'S.  `nm -S` gives `D` -- global, writable,
 * `.data` -- for ten of these and `R` for the fourteen in `.rodata`.  That
 * split is not ours to tidy: it is what a differential test compares, and
 * `t_v17cfg.c` asserts the sizes that go with it.
 *
 *   D  V17_MTD_COEFF CRRv17_CLK CRRv17_PLL_K1_S CRRv17_PLL_K2 CRRv17_PLL_K1
 *      SREv17_PLL_K1_S SREv17_PLL_K2 SREv17_PLL_K1 SREv17_yCLOCK
 *      SREv17_xCLOCK
 *   R  AGCv17_CFG FSEv17_QCOFF FSEv17_ICOFF SREv17_COFFS SREv17_XB_COFFS
 *      MRFv17_COFFS VTBv17_{Q,I}MAP{128,64,32,16T}
 *
 * `AGC_DEF_ALPHA` AND `AGC_DEF_BETA` ARE NOT DECLARED HERE, ON PURPOSE.  The
 * object defines each of those two names SIX times -- five local (`d`/`r`)
 * copies and one global -- so V.17's pair at .rodata 0x9e2c and 0x9e28 is
 * file-static, has no `ref_` alias, and cannot be compared against the blob
 * by name.  They are `static` in `v17cfg.c` and are proved instead through
 * `AGCv17_CFG.alpha` / `.beta`, which is what the pointers actually reach.
 * This is the F9058 / F9144 case: the consumer is the comparison.
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
