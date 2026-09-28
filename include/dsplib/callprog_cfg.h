/*
 * callprog_cfg.h -- Call Progress: the band filter's coefficients.
 *
 * The design lives as file statics in `src/callprog/Callprog.c` under the
 * blob's own names (`_filter_a_coef`, `_filter_b_coef`,
 * `_apply_biquad_scales`); the blob binds them LOCAL, so they are no longer
 * exported.  The unit test drives the engine from the reference object's
 * copies (`ref__filter_a_coef`, ...) instead, which is the same data.
 */

#ifndef DSPLIB_CALLPROG_CFG_H
#define DSPLIB_CALLPROG_CFG_H

#include "dsplib/toneiir.h"

#endif /* DSPLIB_CALLPROG_CFG_H */
