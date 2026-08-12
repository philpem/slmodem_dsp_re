/*
 * v22txtab.h -- the V.22/V.22bis transmit-side configuration and maps.
 *
 * Definitions and derivations are in src/pump/v22/v22txtab.c; the struct
 * types are in dsplib/fpm_sdm.h and dsplib/fpm_smc.h.
 */

#ifndef DSPLIB_V22TXTAB_H
#define DSPLIB_V22TXTAB_H

#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"

extern const struct fpm_smc_cfg SMCv22_CFG;
extern const short SMCv22_QMAP_1200BPS[16];
extern const short SMCv22_IMAP_1200BPS[16];
extern const short SMCv22_QMAP_2400BPS[16];
extern const short SMCv22_IMAP_2400BPS[16];
extern const unsigned short SMCv22_PMAP[4];
extern const struct fpm_sdm_cfg SDMv22_CFG;

#endif /* DSPLIB_V22TXTAB_H */
