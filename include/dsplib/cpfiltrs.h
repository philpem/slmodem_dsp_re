/*
 * cpfiltrs.h -- Call Progress Filters: four named bandpass designs.
 *
 * CPfiltrs.c is a pure data translation unit -- no code at all -- holding
 * four eighth-order elliptic bandpasses for the engine in toneiir.h.  Each is
 * named after its passband in hertz, which the measured response confirms
 * (the call-progress path runs at 8000 Hz; see docs/configuration.md):
 *
 *   name          -6 dB band     peak       what it is for
 *   CP_100_550       0 -  596    452 Hz     everything below the tones
 *   CP_276_504     230 -  546    416 Hz     dial tone, 350 + 440
 *   CP_350_600     263 -  898    285 Hz     ringback, 440 + 480
 *   CP_450_630     396 -  670    640 Hz     busy and congestion, 480 + 620
 *
 * Each design is three arrays, the argument triple `_iir_filter_create`
 * takes: `_scales` is the five interstage shifts, `_a` the twelve denominator
 * coefficients and `_b` the twelve numerator coefficients, all Q13.
 *
 * WHO USES THEM
 *
 * `cadence_create`, and nothing else -- these four and the nine `Filter_*`
 * banks in Elliptic1/2/3.c are the per-country filter bank the cadence
 * detector picks from.  Which design detects which tone is chosen by
 * `GetDialToneCallProgressFilterIndex` and its three siblings (see
 * docs/parameters.md), so a British modem and an American one listen for busy
 * tone through different filters.
 *
 * They are not the filter `CALLPROG_Create` installs -- that is a separate
 * file static in Callprog.c, reproduced in callprog_cfg.h, and is a
 * band-limiter rather than a tone filter.
 */

#ifndef DSPLIB_CPFILTRS_H
#define DSPLIB_CPFILTRS_H

#include "dsplib/toneiir.h"

/** @brief 0-596 Hz bandpass (peak 452 Hz), for tones below the standard band. Interstage shifts; see #CP_100_550_a/#CP_100_550_b. */
extern const short CP_100_550_scales[IIR_FILTER_SCALES];
/** @brief Denominator coefficients for #CP_100_550_scales. */
extern const short CP_100_550_a[IIR_FILTER_COEFF];
/** @brief Numerator coefficients for #CP_100_550_scales. */
extern const short CP_100_550_b[IIR_FILTER_COEFF];

/** @brief 230-546 Hz bandpass (peak 416 Hz), dial tone (350 + 440 Hz). Interstage shifts; see #CP_276_504_a/#CP_276_504_b. */
extern const short CP_276_504_scales[IIR_FILTER_SCALES];
/** @brief Denominator coefficients for #CP_276_504_scales. */
extern const short CP_276_504_a[IIR_FILTER_COEFF];
/** @brief Numerator coefficients for #CP_276_504_scales. */
extern const short CP_276_504_b[IIR_FILTER_COEFF];

/** @brief 263-898 Hz bandpass (peak 285 Hz), ringback (440 + 480 Hz). Interstage shifts; see #CP_350_600_a/#CP_350_600_b. */
extern const short CP_350_600_scales[IIR_FILTER_SCALES];
/** @brief Denominator coefficients for #CP_350_600_scales. */
extern const short CP_350_600_a[IIR_FILTER_COEFF];
/** @brief Numerator coefficients for #CP_350_600_scales. */
extern const short CP_350_600_b[IIR_FILTER_COEFF];

/** @brief 396-670 Hz bandpass (peak 640 Hz), busy/congestion (480 + 620 Hz). Interstage shifts; see #CP_450_630_a/#CP_450_630_b. */
extern const short CP_450_630_scales[IIR_FILTER_SCALES];
/** @brief Denominator coefficients for #CP_450_630_scales. */
extern const short CP_450_630_a[IIR_FILTER_COEFF];
/** @brief Numerator coefficients for #CP_450_630_scales. */
extern const short CP_450_630_b[IIR_FILTER_COEFF];

#endif /* DSPLIB_CPFILTRS_H */
