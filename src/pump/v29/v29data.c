/*
 * v29data.c -- ITU-T V.29 (fax): the transmitter's no-carrier leaf.
 *
 * Reconstructed from dsplibs.o:
 *
 *   TxNoCarrierV29   .text 0x0a6620  146
 *
 * ---------------------------------------------------------------------------
 * WHY THE SILENCE IS TWO ZEROED RAILS AND NOT A ZERO INDEX
 *
 * `V29TX_create` builds the shaper's configuration with `mapped` clear, so
 * `FPM_PPS_filter` reads `src->i[ridx]` and `src->q[ridx]` and never looks at
 * `sym`.  Zeroing the two rails is therefore how this modulation says "no
 * symbol"; writing a zero into `sym` would be read by nothing.  V.17's
 * `mapped` is set and its no-carrier path writes `sym` alone.  Neither
 * reading is derived from the other -- see finding 3641.
 *
 * Both rail pointers are loaded once, above the loop, and that is FREE: the
 * loop stores `short`s, which cannot alias a `short *` object, so a compiler
 * may hoist them whether the source read them once or per iteration.  Written
 * once, which is the plainer source.
 */

#include "dsplib/v29data.h"

#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

/* The instance is not modelled; see v29data.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))

unsigned short
TxNoCarrierV29(void *modem, const unsigned short *data, short *out,
	       unsigned short count)
{
	struct fpm_smc_ring *ring;
	unsigned short i, produced;
	short widx, len;
	void *fp;

	(void)data;			/* never read; see v29data.h */

	fp = FIELD_PTR(modem, V29TX_OBJ_FP);
	ring = (struct fpm_smc_ring *)(void *)FIELD(fp, V29FP_SMC_RING);
	widx = ring->widx;
	len = ring->len;

	for (i = 0; i < count; i++) {
		short next;

		ring->i[widx] = 0;
		ring->q[widx] = 0;
		next = (short)(widx + 1);
		widx = next < len ? next : 0;
	}

	produced = FPM_PPS_filter((struct fpm_pps *)(void *)
					FIELD(fp, V29FP_PPS),
				  ring, out, count);

	/* Back through a FRESH read of the instance pointer, after the shaper. */
	fp = FIELD_PTR(modem, V29TX_OBJ_FP);
	((struct fpm_smc_ring *)(void *)FIELD(fp, V29FP_SMC_RING))->widx = widx;

	return produced;
}
