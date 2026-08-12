/*
 * fpm_lmsupd.c -- Fixed Point Modem: one LMS coefficient update.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_lmsupd  .text 0x0abbc0  150
 *
 * NOT PART OF THE EQUALISER BATCH: it is finding 1600's "leaf math" group,
 * written here because `FPM_FSE_receive` cannot be linked without it.  It is
 * not one of the two symbols that group shares with V.22.
 *
 * The walk is the same shape as the equaliser's own FIR: from the newest
 * sample down to the base of the history, then from the top of the buffer
 * back down to the newest.  `coeff` advances monotonically across both, so
 * the pairing is coefficient order against sample age.
 */

#include "dsplib/fpm.h"

void
FPM_lmsupd(short *coeff, const short *hist, short widx, short taps, short err)
{
	short *c = coeff;
	short k;

	for (k = widx; k >= 0; k--) {
		*c = (short)(*c + ((hist[k] * err + 0x20000) >> 18));
		c++;
	}

	for (k = (short)(taps - 1); k > widx; k--) {
		*c = (short)(*c + ((hist[k] * err + 0x20000) >> 18));
		c++;
	}
}
