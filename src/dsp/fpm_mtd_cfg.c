/*
 * fpm_mtd_cfg.c -- Fixed Point Modem: Multi-Tone Detector configuration.
 *
 * Extracted from dsplibs.o .data:0x81b0.  Mixed struct: +0x00 is a pointer
 * to the coefficient bank (R_386_32 into .data), which is why this is a
 * struct and not a short[].
 *
 * THE OBJECT'S OWN `FPM_MTD_CFG` IS NOW HERE, AND IT IS NOT `FPM_MTD_CFG_data`.
 * The two differ in exactly one field.  When this file was written the
 * coefficient bank the object points at -- `DEF_COEFS`, .data 0x81bc, file
 * static -- had no caller that needed it, so the stub below was given a NULL
 * `coeff` under its own name and the blob symbol was left unwritten.  The
 * three fax receiver constructors reference `FPM_MTD_CFG` directly, so it has
 * to exist, and a `src/` reference to an unwritten blob symbol cannot link at
 * all (F8492).
 *
 * BOTH ARE KEPT, DELIBERATELY, AND THAT IS A DEVIATION AND NOT A DESIGN.
 * `FPM_MTD_CFG_data` is read by `FPM_MTD_create` and by `B103FP_create`, and
 * unifying the two means editing `src/pump/b103/b103fp.c`, which is outside
 * this pass's scope.  The unification is not cosmetic: the object's
 * `FPM_MTD_create(state, NULL)` installs `DEF_COEFS`, and ours installs NULL,
 * so the copy that is reachable through a NULL `cfg` is the one that diverges.
 * No test covers that path today.  Recorded as D1101 and F9143; the fix is to
 * delete `FPM_MTD_CFG_data` and point its two readers at `FPM_MTD_CFG`.
 */

#include "dsplib/fpm_mtd.h"

const struct fpm_mtd_cfg FPM_MTD_CFG_data = {
	.coeff = 0,		/* deliberately NULL -- see D1101 above */
	.tones = 2,
	.ratio = 24576,		/* 0.75 in Q15 */
	.min_level = 246
	/* f0a is zero */
};

/*
 * The bank `FPM_MTD_CFG` points at.  `d` in the object -- LOCAL and writable,
 * so file-static here, with no `ref_` alias and nothing to compare it against
 * by name.  Ten shorts is what `tones = 2` over 20 bytes fixes, five per
 * biquad section, the same shape `V21_CHAN2_MTD_COEFF` and `V29_MTD_COEFF`
 * have.  Every entry is 10000, which is not a filter: it is a placeholder
 * bank, consistent with `FPM_MTD_CFG` being a default nothing configures.
 */
static short DEF_COEFS[10] = {
	10000, 10000, 10000, 10000, 10000,
	10000, 10000, 10000, 10000, 10000
};

/*
 * `D` in the object -- global and writable -- hence not `const`.  The pointer
 * at +0x00 was found by sweeping the relocations inside the symbol's own 12
 * bytes; `tabdump.py` renders its addend as -32324, which is a plausible Q15
 * coefficient and is an address.
 */
struct fpm_mtd_cfg FPM_MTD_CFG = {
	DEF_COEFS,		/* +0x00 coeff                             */
	2,			/* +0x04 tones                             */
	24576,			/* +0x06 ratio      0.75 in Q15            */
	246,			/* +0x08 min_level                         */
	0			/* +0x0a f0a                               */
};
