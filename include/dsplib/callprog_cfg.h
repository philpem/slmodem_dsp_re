/*
 * callprog_cfg.h -- Call Progress: the band filter's coefficients.
 *
 * File statics in the original.  Exposed here so the tests can drive the
 * filter engine with the real design as well as with synthetic ones.
 */

#ifndef DSPLIB_CALLPROG_CFG_H
#define DSPLIB_CALLPROG_CFG_H

#include "dsplib/toneiir.h"

extern const short CALLPROG_BandFilter_shift[IIR_FILTER_SCALES];
extern const short CALLPROG_BandFilter_b[3 * IIR_FILTER_SECTIONS];
extern const short CALLPROG_BandFilter_a[3 * IIR_FILTER_SECTIONS];

#endif /* DSPLIB_CALLPROG_CFG_H */
