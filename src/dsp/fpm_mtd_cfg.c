/*
 * fpm_mtd_cfg.c -- Fixed Point Modem: Multi-Tone Detector configuration.
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
	.coeff = 0,		/* deliberately NULL -- see the note above */
	.tones = 2,
	.ratio = 24576,		/* 0.75 in Q15 */
	.min_level = 246
	/* f0a is zero */
};
