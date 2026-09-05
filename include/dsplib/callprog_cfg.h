/*
 * callprog_cfg.h -- Call Progress: the band filter's coefficients.
 *
 * File statics in the original.  Exposed here so the tests can drive the
 * filter engine with the real design as well as with synthetic ones.
 */

#ifndef DSPLIB_CALLPROG_CFG_H
#define DSPLIB_CALLPROG_CFG_H

#include "dsplib/toneiir.h"

/**
 * @brief Call-progress band filter: the Q13 IIR engine's per-section scale
 * shifts.
 *
 * With #CALLPROG_BandFilter_b and #CALLPROG_BandFilter_a, one 220-byte
 * three-section elliptic band-limiter passed straight to
 * `_iir_filter_create()`: -6 dB from 135 Hz to 1280 Hz, within 1.3 dB across
 * 350-620 Hz, a 105 dB null at 2260 Hz. See `src/callprog/callprog_cfg.c`.
 */
extern const short CALLPROG_BandFilter_shift[IIR_FILTER_SCALES];

/** @brief Call-progress band filter's numerator coefficients (see #CALLPROG_BandFilter_shift). */
extern const short CALLPROG_BandFilter_b[3 * IIR_FILTER_SECTIONS];

/** @brief Call-progress band filter's denominator coefficients (see #CALLPROG_BandFilter_shift). */
extern const short CALLPROG_BandFilter_a[3 * IIR_FILTER_SECTIONS];

#endif /* DSPLIB_CALLPROG_CFG_H */
