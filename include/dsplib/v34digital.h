/*
 * v34digital.h -- bringing V.34/V.90's digital half up.
 * See src/pump/v34/v34digital.c.
 */

#ifndef DSPLIB_V34DIGITAL_H
#define DSPLIB_V34DIGITAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring the V.34/V.90 digital pump's shell and scrambler state up.
 *
 * Clears both shell contexts (transmit and receive) and both halves of the
 * scrambler pair, then installs the polynomial each direction gets --
 * originate and answer scramble with opposite polynomials, selected by
 * `role == 0x65`.
 *
 * @param obj  The V.34 modem object (`struct v34_object *`).
 */
void preinitdigital(void *obj);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34DIGITAL_H */
