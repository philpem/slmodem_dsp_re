/*
 * fpm_sre_cfg.c -- Fixed Point Modem: the symbol-rate recovery loop's built-in
 *                  configuration.
 *
 *   FPM_SRE_CFG   .rodata 0x00c4e0   56
 *
 * `R` in the object -- global and const -- unlike its equaliser counterpart
 * `FPM_FSE_CFG`, which is `D`.  The asymmetry is the object's.
 *
 * ALL SIX POINTERS ARE NULL, confirmed by a relocation sweep over the symbol's
 * own 56 bytes rather than by reading zeros: a table of pointers whose addends
 * happen to be small is exactly the misreading `tools/dis.py` exists to
 * prevent, so the absence of a relocation is the evidence and the zeros are
 * only consistent with it.  A caller copies this onto its stack, patches the
 * prototype, the discriminant, the two clock legs and the two gain arrays, and
 * calls `FPM_SRE_init`.
 *
 * `settle` IS 48, AND `fpm_sre.h` SAID 60.  Corrected there; the 60 is the
 * value of `coeffs` six bytes further on, so it was a transcription slip
 * rather than a different reading.  The number is unused by anything
 * reconstructed today -- every caller patches around it or overrides it -- but
 * a comment stating a value the bytes do not hold is the defect findings F6100
 * and F6103 are both instances of.  F9142.
 */

#include "dsplib/fpm_sre.h"

const struct fpm_sre_cfg FPM_SRE_CFG = {
	4,			/* +0x00 clock_len                           */
	1,			/* +0x02 groups_acq                          */
	24,			/* +0x04 groups_trk                          */
	48,			/* +0x06 settle                              */
	1365,			/* +0x08 acc_down   1/24 in Q15              */
	16384,			/* +0x0a acc_up     4 in Q12                 */
	60,			/* +0x0c coeffs     6 taps x 10 branches     */
	0,			/* +0x0e pad0e                               */
	0,			/* +0x10 proto      patched by the caller    */
	0,			/* +0x14 disc       patched by the caller    */
	0,			/* +0x18 xclock     patched by the caller    */
	0,			/* +0x1c yclock     patched by the caller    */
	0,			/* +0x20 pll_k1     patched by the caller    */
	0,			/* +0x24 pll_k2     patched by the caller    */
	2130,			/* +0x28 mag_hi                              */
	164,			/* +0x2a mag_lo                              */
	8192,			/* +0x2c err_hi     >> 3 = 1024              */
	3277,			/* +0x2e err_lo     >> 3 =  409              */
	2252,			/* +0x30 rms_min                             */
	9,			/* +0x32 rms_len                             */
	0,			/* +0x34 pad34                               */
	0			/* +0x36 pad36                               */
};
