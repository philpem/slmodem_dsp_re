/*
 * fpm_pps_cfg.c -- Fixed Point Modem: the transmit pulse shaper's built-in
 *                  configuration, `R` at .rodata:0x00c4a0, 40 bytes.
 *
 * All four table pointers are NULL, confirmed by `tools/relocscan.py --at
 * .rodata:0xc4a0`: nothing in the whole object relocates into this range, so
 * the zeroes an int16 dump shows are the real bytes and not an unread
 * relocation -- the same check `fpm_sre_cfg.c` runs before trusting its own
 * zeroes. A caller copies this onto its stack and patches `imap`, `qmap`,
 * `coeff_i`, `coeff_q` and `scale`; `V29TX_create` (src/fax/v29.c) is that
 * caller for V.29, and it is also in `V17TX_create`'s and `V27TX_create`'s
 * own unwritten closures (`tools/closure.py --missing V17TX_create
 * V27TX_create` names it too), so this file benefits all three once they
 * land.
 */

#include "dsplib/fpm_pps.h"

const struct fpm_pps_cfg FPM_PPS_CFG = {
	10,			/* +0x00 phases                              */
	3,			/* +0x02 step                                */
	1,			/* +0x04 mapped -- every caller traced clears
					it before use; V29TX_create's own note */
	32767,			/* +0x08 scale                               */
	0,			/* +0x0c step_adj                            */
	0,			/* +0x0e pad0e                               */
	0,			/* +0x10 imap  -- patched by the caller      */
	0,			/* +0x14 qmap  -- patched by the caller      */
	0,			/* +0x18 coeff_i -- patched by the caller    */
	0,			/* +0x1c coeff_q -- patched by the caller    */
	120,			/* +0x20 coeffs                              */
	0,			/* +0x22 pad22                               */
	0			/* +0x24 aux                                 */
};
