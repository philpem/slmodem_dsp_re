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
 * NOTHING IN THE OBJECT REFERENCES THEM.
 *
 * All twelve symbols are global and every one is dead: no relocation anywhere
 * in dsplibs.o points at them, and the filter CALLPROG_Create actually
 * installs is a separate file static in Callprog.c (see callprog_cfg.h).
 * They survive only because being global stopped the linker collecting them.
 *
 * The natural reading is that these are an earlier or alternative detector's
 * filter bank, kept against a caller outside this library.  They are
 * reproduced because they are part of the object's published surface and
 * something linking against it may still want them -- but if you are looking
 * for the filters the modem uses, they are the Filter_* banks in
 * Elliptic1/2/3.c, which cadence_create selects from, and the supervisor's
 * own band filter in callprog_cfg.h -- not here.
 */

#ifndef DSPLIB_CPFILTRS_H
#define DSPLIB_CPFILTRS_H

#include "dsplib/toneiir.h"

extern const short CP_100_550_scales[IIR_FILTER_SCALES];
extern const short CP_100_550_a[IIR_FILTER_COEFF];
extern const short CP_100_550_b[IIR_FILTER_COEFF];

extern const short CP_276_504_scales[IIR_FILTER_SCALES];
extern const short CP_276_504_a[IIR_FILTER_COEFF];
extern const short CP_276_504_b[IIR_FILTER_COEFF];

extern const short CP_350_600_scales[IIR_FILTER_SCALES];
extern const short CP_350_600_a[IIR_FILTER_COEFF];
extern const short CP_350_600_b[IIR_FILTER_COEFF];

extern const short CP_450_630_scales[IIR_FILTER_SCALES];
extern const short CP_450_630_a[IIR_FILTER_COEFF];
extern const short CP_450_630_b[IIR_FILTER_COEFF];

#endif /* DSPLIB_CPFILTRS_H */
