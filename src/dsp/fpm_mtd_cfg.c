/*
 * fpm_mtd_cfg.c -- the built-in multi-tone detector configuration.
 *
 * Extracted from dsplibs.o .data:0x81b0.  Mixed struct: +0x00 is a pointer
 * to the coefficient bank (R_386_32 into .data), which is why this is a
 * struct and not a short[].
 *
 * The pointer target is left NULL here: Bell 103 never uses this default --
 * B103FP_create builds its own config around MTDb103_COEF -- so wiring it up
 * would mean extracting a table nothing reachable reads.  Fill it in if a
 * caller of FPM_MTD_create(state, NULL) ever appears.
 */

#include "dsplib/fpm_mtd.h"

const struct fpm_mtd_cfg FPM_MTD_CFG_data = {
	0,	/* +0x00 coefficient bank -- see note above */
	2,	/* +0x04 tone count */
	24576,	/* +0x06 */
	246,	/* +0x08 */
	0	/* +0x0a */
};
