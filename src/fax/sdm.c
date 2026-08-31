/*
 * sdm.c -- the fax pumps' Scrambler/Descrambler Module.
 *
 * Reconstructed from dsplibs.o:
 *   SDM_scrambler    .text 0x09f150   146 bytes
 *   SDM_descrambler  .text 0x09f1f0   167 bytes
 *   SDM_init         .text 0x09f2a0    86 bytes
 *   SDM_CFG          .data 0x0080d8      6 bytes
 *
 * ALL THREE FUNCTIONS ARE BYTE-FOR-BYTE `FPM_SDM_*`.  Measured, not assumed:
 * the 86, 146 and 167 bytes at these addresses compare equal to the 86, 146
 * and 167 at 0x0a9b60, 0x0a9a10 and 0x0a9ab0, with no differing byte in any
 * of the three.  So this file is `src/dsp/fpm_sdm.c` with the names shortened
 * and nothing else, and it is kept as a separate translation unit because the
 * object has two of them.
 *
 * The comments that explain WHY each line is what it is live once, in
 * `src/dsp/fpm_sdm.c` and `include/dsplib/fpm_sdm.h`.  Duplicating them here
 * would give the tree two copies to keep in step, which is the failure this
 * file exists to avoid rather than to demonstrate.  What is kept below is
 * only what a reader needs to not mis-edit one copy: the two places where the
 * object does something redundant on purpose.
 */

#include "dsplib/sdm.h"

/*
 * The object's initialiser.  Every caller traced overwrites all three fields
 * after copying it, so this is a default nothing runs; see `dsplib/sdm.h`.
 */
struct fpm_sdm_cfg SDM_CFG = { 4, 5, 23 };

void
SDM_init(struct fpm_sdm *sdm, const struct fpm_sdm_cfg *cfg)
{
	sdm->reg = 0;
	sdm->cfg = *cfg;
	sdm->mask = (1 << sdm->cfg.nbits) - 1;
	sdm->notmask = ~sdm->mask;
	sdm->shift1 = (short)(sdm->cfg.tap1 - sdm->cfg.nbits);
	sdm->shift2 = (short)(sdm->cfg.tap2 - sdm->cfg.nbits);
}

void
SDM_scrambler(struct fpm_sdm *sdm, unsigned short *data, unsigned short count)
{
	const int nbits = sdm->cfg.nbits;
	const int shift1 = sdm->shift1;
	const int shift2 = sdm->shift2;
	const int mask = sdm->mask;
	const int notmask = sdm->notmask;
	unsigned int reg = sdm->reg;

	while (count--) {
		unsigned short out = (unsigned short)
			(((reg >> shift1) ^ *data ^ (reg >> shift2)) & mask);

		*data++ = out;
		/* `& notmask` is redundant after the shift and the object
		 * does it anyway. */
		reg = ((reg << nbits) & notmask) | out;
	}

	sdm->reg = reg;
}

void
SDM_descrambler(struct fpm_sdm *sdm, unsigned short *data, unsigned short count)
{
	const int nbits = sdm->cfg.nbits;
	const int shift1 = sdm->shift1;
	const int shift2 = sdm->shift2;
	const int mask = sdm->mask;
	const int notmask = sdm->notmask;
	unsigned int reg = sdm->reg;

	while (count--) {
		/* Feed-forward: the register takes the word that ARRIVED, and
		 * takes it WHOLE, without the mask. */
		unsigned short in = *data;

		*data = (unsigned short)
			((reg >> shift1) ^ in ^ (reg >> shift2));
		reg = ((reg << nbits) & notmask) | in;
		/* A second pass over the word just written, not part of the
		 * expression above. */
		*data &= mask;
		data++;
	}

	sdm->reg = reg;
}
