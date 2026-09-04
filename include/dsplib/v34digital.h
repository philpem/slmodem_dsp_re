/*
 * v34digital.h -- bringing V.34/V.90's digital half up.
 * See src/pump/v34/v34digital.c.
 */

#ifndef DSPLIB_V34DIGITAL_H
#define DSPLIB_V34DIGITAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Clear both shell contexts and both halves of the scrambler pair, and
 * install the polynomial each direction gets.  Which way round that goes is
 * `role == 0x65`, the originate/answer flag.
 */
void preinitdigital(void *obj);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34DIGITAL_H */
