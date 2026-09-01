/*
 * fpm_fse_cfg.c -- Fixed Point Modem: the fractionally-spaced equaliser's
 *                  built-in configuration.
 *
 *   FPM_FSE_CFG   .data 0x008160   56
 *
 * `D` in the object -- global and WRITABLE -- which is why it is not `const`
 * here.  Nothing in the 1.2 MB writes it, but the storage class is the
 * object's and a differential test compares the section it lands in.
 *
 * IT IS A SET OF DEFAULTS WITH EVERY POINTER NULL, and that is the shape the
 * whole `fpm_*_cfg` family has: a caller copies this static onto its stack
 * with a 14-dword `rep movsl` and patches the tables, the lengths and the
 * slicer in before calling `FPM_FSE_init`.  The relocation sweep over the
 * symbol's own 56 bytes finds NONE, which is the direct evidence for that --
 * the six pointer fields hold literal zero and not an unrelocated address.
 *
 * SO THE VALUES THAT SURVIVE A CALLER ARE THE FEW IT DOES NOT PATCH.
 * `V29RX_create` overrides `interp`, `taps`, `clk_mod`, `clk_inc`,
 * `train_sym`, `err_hi`, `err_lo` and `mu[0..1]`, and leaves `block` = 144 and
 * `mu[2]` = 0 standing.  That is what this table is FOR, and it is why the
 * whole thing has to be right even though most of it is zero.
 */

#include "dsplib/fpm_fse.h"

struct fpm_fse_cfg FPM_FSE_CFG = {
	144,			/* +0x00 block      max samples per receive  */
	3,			/* +0x02 interp     samples per symbol       */
	0,			/* +0x04 icoff      patched by the caller    */
	0,			/* +0x08 qcoff      patched by the caller    */
	0,			/* +0x0c taps       patched by the caller    */
	{ 0, 0, 0 },		/* +0x0e mu[3]                               */
	0,			/* +0x14 clk        patched by the caller    */
	4,			/* +0x18 clk_mod                             */
	1,			/* +0x1a clk_inc                             */
	96,			/* +0x1c train_sym                           */
	6554,			/* +0x1e err_hi     0.2 in Q15               */
	1638,			/* +0x20 err_lo     0.05 in Q15              */
	0,			/* +0x22 pad22                               */
	0,			/* +0x24 pll_k1     patched by the caller    */
	0,			/* +0x28 pll_k2     patched by the caller    */
	0,			/* +0x2c owner      patched by the caller    */
	0,			/* +0x30 decision   patched by the caller    */
	0			/* +0x34 reserved34                          */
};
