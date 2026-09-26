/*
 * mtk.h -- the MTK lookup tables dsplibs.o carries in .data.
 *
 * `MTK_` is the object's own prefix; `MTK_phasor` (0xb0690) is the only
 * function in the blob that reads the sine pair and the two sign vectors,
 * and it is what fixes their shape:
 *
 *   - a quarter wave 257 entries long, so that a linear interpolation
 *     between `[i]` and `[i + 1]` is defined for every index 0..255;
 *   - a four-entry sign vector per function, indexed by quadrant.
 *
 * `MTK_atan_table` and `MTK_xor_table` share the prefix and sit in the same
 * run of `.data` (0x8500..0x9364, uninterrupted); no reconstructed caller
 * reaches either yet, so their consumers are unknown and their names and
 * contents are all this header claims.
 *
 * ALIGNMENT IS THE COMPILER'S, NOT AN ATTRIBUTE.  The three 1,028-byte tables
 * and the 512-byte one land on 32-byte boundaries in the object and the two
 * 16-byte ones only on 4; that is exactly GCC's i386 DATA_ALIGNMENT boost for
 * an array of at least 32 bytes when optimising, so there is nothing to
 * declare here.  See src/service/TABLES.c for the derivations.
 */

#ifndef DSPLIB_MTK_H
#define DSPLIB_MTK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Quarter waves, i = 0..256, indexed by the low 8 bits of the scaled angle. */
extern const float MTK_sin_table[257];
extern const float MTK_cos_table[257];

/* Sign by quadrant, indexed by bits 8-9 of the same scaled angle. */
extern const float MTK_sin_sign[4];
extern const float MTK_cos_sign[4];

/* atan(i / 256.0), i = 0..256. */
extern const float MTK_atan_table[257];

/* popcount(i), i = 0..255. */
extern const short MTK_xor_table[256];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_MTK_H */
