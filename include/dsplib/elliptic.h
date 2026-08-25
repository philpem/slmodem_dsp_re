/*
 * elliptic.h -- Elliptic1/2/3.c: three banks of seven call-progress filters.
 *
 * Three pure-data translation units, no code between them, each holding one
 * family of seven eighth-order elliptic bandpasses that widen progressively.
 * `cadence_create` picks a family with `Get*CallProgressFilterIndex` and one
 * of its seven designs with `GetDialToneFilterSubindex` -- one-based, so
 * subindex 1 is element 0.
 *
 * In practice the designs are never reached: slmodemd answers
 * `GetDialToneFilterSubindex` with a literal zero, which is out of range, so
 * every bank selection falls back to the family's nearest single design in
 * cpfiltrs.h.  See finding F49.  They are reconstructed because they are what
 * the object contains, and because being global they can be compared against
 * the blob word for word.
 */

#ifndef DSPLIB_ELLIPTIC_H
#define DSPLIB_ELLIPTIC_H

#include "dsplib/toneiir.h"

extern const short Filter_350_500_scales[7 * IIR_FILTER_SCALES];
extern const short Filter_350_500_b[7 * IIR_FILTER_COEFF];
extern const short Filter_350_500_a[7 * IIR_FILTER_COEFF];
extern const short Filter_100_550_scales[7 * IIR_FILTER_SCALES];
extern const short Filter_100_550_b[7 * IIR_FILTER_COEFF];
extern const short Filter_100_550_a[7 * IIR_FILTER_COEFF];
extern const short Filter_276_504_scales[7 * IIR_FILTER_SCALES];
extern const short Filter_276_504_b[7 * IIR_FILTER_COEFF];
extern const short Filter_276_504_a[7 * IIR_FILTER_COEFF];
#endif /* DSPLIB_ELLIPTIC_H */
