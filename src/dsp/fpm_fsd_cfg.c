/*
 * fpm_fsd_cfg.c -- Fixed Point Modem: the FSK Demodulator's library built-in
 *                  configuration, `D` at .data:0x812c, 28 bytes.
 *
 * THE OBJECT'S OWN `FPM_FSD_CFG` IS HERE, AND IT IS NOT `FPM_FSD_CFG_data`.
 * This is exactly `fpm_mtd_cfg.c`'s situation and it is resolved the same way,
 * for the same reason: when `src/dsp/fpm_fsd.c` was written the blob symbol
 * had no caller that needed it, so a stub was given its own `_data` name and
 * `FPM_FSD_CFG` was left unwritten.  `V21RX_create` (0x098e70) references
 * `FPM_FSD_CFG` directly -- six `mov` loads at 0x098f79 through 0x098fb1 that
 * copy it onto the stack -- and a `src/` reference to an unwritten blob symbol
 * cannot link at all (F8492).  So it has to exist under the object's name.
 *
 * WHERE THIS DIFFERS FROM `FPM_MTD_CFG`, AND IT IS THE HAPPY DIRECTION.
 * D1101 had to record a real divergence, because the object's `FPM_MTD_CFG`
 * points at `DEF_COEFS` and the `_data` stub carries a NULL there, so the two
 * are not the same bytes.  These two ARE the same bytes: `FPM_FSD_CFG` holds
 * no pointer at all -- a relocation sweep over its own 28-byte range finds
 * nothing inside it -- and every scalar in it equals the one
 * `FPM_FSD_CFG_data` already carries.  `t_v21cfg.c` asserts that field for
 * field, so the claim is measured and not asserted.
 *
 * The two therefore differ only in STORAGE CLASS: the object's is `D`, global
 * and writable in `.data`, and the stub is `R`, const in `.rodata`.  Keeping
 * both is a duplicate symbol the object does not have, recorded as D1180.
 * The fix is one line in `src/pump/b103/b103fp.c` -- its `fsd =
 * FPM_FSD_CFG_data;` at line 1093 is the stub's only reader -- plus deleting
 * the stub and its declaration.  That file is under `src/pump/`, outside this
 * pass's scope, which is the only reason it was not done here.
 *
 * `f18` AND `pad1a` ARE LEFT AS `fpm_fsd.h` HAS THEM.  `V21RX_create` writes
 * that dword with a single 32-bit `mov` at 0x098f75, which is what a `void *`
 * or an `int` there would take and not what two `short` stores would, so the
 * pair is probably one 32-bit slot -- the same `aux` that `struct fpm_mrf_cfg`
 * carries at its own end, and it receives the SAME value in this constructor.
 * Retyping it is a change to a header B.103 shares, it moves no bytes here
 * (both spellings are four bytes and both are zero in this table), and it is
 * not needed by anything this pass writes.  Recorded as D1181 rather than
 * done blind.
 */

#include "dsplib/fpm_fsd.h"

/*
 * `D` in the object -- global and writable -- hence not `const`.
 *
 * Both filters are absent: the caller supplies them, and every caller does.
 * `V21RX_create` patches `fir`, `fir_taps`, `delay`, `iir`, `iir_len`,
 * `high_bit` and `bit_samples`, and takes `slice_level`, `max_bits` and
 * `trace_len` from here unchanged.
 */
struct fpm_fsd_cfg FPM_FSD_CFG = {
	0,			/* +0x00 fir; the caller supplies it        */
	0,			/* +0x04 fir_taps                           */
	0,			/* +0x06 delay                              */
	0,			/* +0x08 iir; the caller supplies it        */
	0,			/* +0x0c iir_len                            */
	10,			/* +0x0e slice_level                        */
	1,			/* +0x10 high_bit                           */
	8,			/* +0x12 bit_samples                        */
	6,			/* +0x14 max_bits                           */
	160,			/* +0x16 trace_len                          */
	0,			/* +0x18 f18                                */
	0			/* +0x1a pad1a                              */
};
