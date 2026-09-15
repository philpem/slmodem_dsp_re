/*
 * v17data.c -- ITU-T V.17 (fax): the transmitter's data-path leaves.
 *
 * Reconstructed from dsplibs.o:
 *
 *   ModDataV17       .text 0x0a0d40   98
 *   TxNoCarrierV17   .text 0x0a0db0  143
 *
 * `tools/tumap.py` brackets 95 translation units together as `class1tx.c
 * +94`, so it cannot say which of them these two are; they are kept in one
 * file because they are one layer -- the V.17 transmitter's two ways of
 * getting symbols into the shared shaper -- and not because a translation
 * unit has been established.  `include/dsplib/v17data.h` carries the offset
 * evidence.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE TWO HAVE IN COMMON, AND WHERE THEY PART
 *
 * Both end in the same call: `FPM_PPS_filter(fp + 0x48, fp + 0x08, out,
 * count)`, the shaper reading the symbol ring.  What differs is who fills the
 * ring.  `ModDataV17` dispatches through the instance's own three-entry
 * encoder table; `TxNoCarrierV17` writes the ring itself, one constant index
 * per symbol, and never touches the coder.
 *
 * So the ring is the interface between the two halves, exactly as it is for
 * V.22 -- and here the object says so twice, because `ModDataV17` computes
 * `fp + 0x08` for the encoder and again for the filter, three instructions
 * apart, from two separate loads of the instance pointer.
 *
 * ---------------------------------------------------------------------------
 * THE NO-CARRIER SYMBOL IS RE-READ ON EVERY ITERATION, AND THAT IS FORCED
 *
 * `movzwl 0x1e(%edi),%eax` sits at the TOP of TxNoCarrierV17's loop, not
 * above it.  The store in the same loop is through a `short *`, which may
 * alias the `unsigned short` being loaded, so a compiler cannot hoist that
 * load out -- but neither can it SINK a load the source put outside the loop
 * into it.  The load being inside means the source's was.  Written that way,
 * which is observable only if the parameter block overlaps the ring.
 */

#include "dsplib/v17fax.h"

#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

unsigned short
ModDataV17(void *modem, const unsigned short *data, short *out,
	   unsigned short count)
{
	struct v17tx *tx = (struct v17tx *)modem;
	struct v17tx_fp *fp;
	short sel;

	fp = tx->fp;
	sel = fp->encoder_sel;
	fp->encoders[sel](&fp->smc, &fp->ring, data, count);

	return FPM_PPS_filter(&tx->fp->pps, &tx->fp->ring,
			      out, count);
}

unsigned short
TxNoCarrierV17(void *modem, const unsigned short *data, short *out,
	       unsigned short count)
{
	struct v17tx *tx = (struct v17tx *)modem;
	struct fpm_smc_ring *ring;
	unsigned short i;
	short widx, len;
	struct v17tx_fp *fp;

	(void)data;			/* never read; see v17data.h */

	fp = tx->fp;
	ring = &fp->ring;
	widx = ring->widx;
	len = ring->len;

	if (count != 0) {
		struct v17tx_priv *prm = tx->priv;

		for (i = 0; i < count; i++) {
			short next;

			/*
			 * Re-read every iteration; see the header comment.
			 * `%ax` alone is used, so the extension is free and
			 * the type follows the ring's element being an index.
			 */
			ring->sym[widx] = (short)prm->no_carrier_sym;
			next = (short)(widx + 1);
			widx = (short)(next < len ? next : 0);
		}
	}

	i = FPM_PPS_filter(&fp->pps, ring, out, count);

	/*
	 * The cursor goes back through a FRESH read of the instance pointer,
	 * after the shaper has run.  Both are what the object encodes.
	 */
	fp = tx->fp;
	fp->ring.widx = widx;

	return i;
}
