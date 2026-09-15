/*
 * fpm_sdm.c -- Fixed Point Modem: Scrambler/Descrambler Module.
 *
 * Reconstructed from dsplibs.o fpm_sdm.c:
 *   FPM_SDM_scrambler    .text 0x0a9a10   146 bytes
 *   FPM_SDM_descrambler  .text 0x0a9ab0   167 bytes
 *   FPM_SDM_init         .text 0x0a9b60    86 bytes
 *
 * The geometry -- why the taps are stored biased by `nbits`, and which bit of
 * a word is which bit of the stream -- is in include/dsplib/fpm_sdm.h.  This
 * file is the mechanics.
 *
 * Both loops count in 16 bits: `count` is an `unsigned short` parameter and
 * the object decrements it as one, so a caller passing 0x10000 (impossible
 * through the declared type, reachable through a cast) processes nothing.
 */

#include "dsplib/fpm_sdm.h"

void
FPM_SDM_scrambler(struct fpm_sdm *sdm, unsigned short *data,
		  unsigned short count)
{
	const int nbits = sdm->cfg.nbits;
	const int shift1 = sdm->shift1;
	const int shift2 = sdm->shift2;
	const int mask = sdm->mask;
	const int notmask = sdm->notmask;
	unsigned int reg = sdm->reg;

	while (count--) {
		/*
		 * All `nbits` output bits at once, from the register as it
		 * stood before this word.  The mask is what keeps the result
		 * to the word's own bits: the tap terms carry the whole
		 * register above them.
		 */
		unsigned short out = (unsigned short)
			(((reg >> shift1) ^ *data ^ (reg >> shift2)) & mask);

		*data++ = out;
		/*
		 * Shift the word in.  `& notmask` is redundant -- the shift
		 * has already zeroed those bits -- and the object does it
		 * anyway, so it is here.
		 */
		reg = ((reg << nbits) & notmask) | out;
	}

	sdm->reg = reg;
}
void
FPM_SDM_init(struct fpm_sdm *sdm, const struct fpm_sdm_cfg *cfg)
{
	sdm->reg = 0;
	sdm->cfg = *cfg;
	sdm->mask = (1 << sdm->cfg.nbits) - 1;
	sdm->notmask = ~sdm->mask;
	sdm->shift1 = (short)(sdm->cfg.tap1 - sdm->cfg.nbits);
	sdm->shift2 = (short)(sdm->cfg.tap2 - sdm->cfg.nbits);
}




void
FPM_SDM_descrambler(struct fpm_sdm *sdm, unsigned short *data,
		    unsigned short count)
{
	const int nbits = sdm->cfg.nbits;
	const int shift1 = sdm->shift1;
	const int shift2 = sdm->shift2;
	const int mask = sdm->mask;
	const int notmask = sdm->notmask;
	unsigned int reg = sdm->reg;

	while (count--) {
		/*
		 * Feed-forward, so the register takes the word that ARRIVED,
		 * not the one produced -- and takes it whole, without the
		 * mask.  A word with rubbish above `nbits` therefore stays in
		 * the register until it shifts past tap2.
		 */
		unsigned short in = *data;

		*data = (unsigned short)
			((reg >> shift1) ^ in ^ (reg >> shift2));
		reg = ((reg << nbits) & notmask) | in;
		/*
		 * The mask is a second pass over the word just written, not
		 * part of the expression above; it matters only in that the
		 * store is 16 bits wide either way.
		 */
		*data &= mask;
		data++;
	}

	sdm->reg = reg;
}
