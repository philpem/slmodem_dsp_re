/*
 * sdm.h -- the fax pumps' copy of the Scrambler/Descrambler Module.
 *
 * `SDM_init`, `SDM_scrambler` and `SDM_descrambler` are BYTE-FOR-BYTE the
 * same code as `FPM_SDM_init`, `FPM_SDM_scrambler` and `FPM_SDM_descrambler`
 * -- 86, 146 and 167 bytes each, and every byte of all three compares equal
 * between the two addresses.  So this is the same source compiled into a
 * second translation unit under a shorter name, and the geometry, the bias on
 * the taps and the register's width are all documented once, in
 * `include/dsplib/fpm_sdm.h`.  Read that file; there is nothing extra here.
 *
 * The types are shared for the same reason: `struct fpm_sdm` and
 * `struct fpm_sdm_cfg` are the objects these functions operate on, and one
 * type has one home.
 *
 * ---------------------------------------------------------------------------
 * SDM_CFG and why its contents do not matter much
 *
 * SDM_CFG lives in `.data`, not `.rodata`, and holds { 4, 5, 23 }.  That is
 * NOT the polynomial the fax modems run: every caller copies it into a local
 * and then overwrites the fields.  Measured, not assumed --
 *
 *     V17RX_create   0x0976a8   nbits = <rate> + 3, tap1 = 0x12, tap2 = 0x17
 *     V29TX_create   0x09bc28   nbits = <rate> + 3, tap1 = 0x12, tap2 = 0x17
 *
 * -- and 18 with 23 is ITU-T V.29 section 5.2's scrambler, 1 + x^-18 + x^-23,
 * which V.17 (and V.33) share.  V17TX_create, V29RX_create and SetTxModeV17
 * also reference it; they were not traced instruction by instruction.
 *
 * The six bytes are reproduced because they are six bytes of the object's
 * `.data`, not because anything reads them as they stand.
 */

#ifndef DSPLIB_SDM_H
#define DSPLIB_SDM_H

#include "dsplib/fpm_sdm.h"

/* Writable: the object puts it in `.data`, and nothing in `src/` may make it
 * const without moving it to `.rodata` and changing the section layout. */
extern struct fpm_sdm_cfg SDM_CFG;

/**
 * @brief Load a config and clear the shift register.
 *
 * Byte for byte FPM_SDM_init(); see include/dsplib/fpm_sdm.h.
 *
 * @param sdm  State to initialise.
 * @param cfg  Configuration (`nbits`, and the two taps biased by it).
 */
void SDM_init(struct fpm_sdm *sdm, const struct fpm_sdm_cfg *cfg);

/**
 * @brief Scramble @p count words in place.
 *
 * Byte for byte FPM_SDM_scrambler(); see include/dsplib/fpm_sdm.h.
 *
 * @param sdm    Scrambler state.
 * @param data   Words to scramble in place, @p count of them.
 * @param count  Number of words in @p data.
 */
void SDM_scrambler(struct fpm_sdm *sdm, unsigned short *data,
		   unsigned short count);

/**
 * @brief Descramble @p count words in place.
 *
 * Byte for byte FPM_SDM_descrambler(); see include/dsplib/fpm_sdm.h.
 *
 * @param sdm    Descrambler state.
 * @param data   Words to descramble in place, @p count of them.
 * @param count  Number of words in @p data.
 */
void SDM_descrambler(struct fpm_sdm *sdm, unsigned short *data,
		     unsigned short count);

#endif /* DSPLIB_SDM_H */
