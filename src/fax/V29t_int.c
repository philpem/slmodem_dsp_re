/*
 * V29t_int.c -- split out of the merged v29.c / v29data.c so the definitions sit in
 * the translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <stddef.h>
#include <string.h>

#include "dsplib/v29fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v29cfg.h"
#include "dsplib/v29data.h"

/*
 * ---------------------------------------------------------------------------
 * ScrambleDataV29 -- .text 0x0a6560, 28 bytes.
 *
 * The transmit half of the pair, reaching the TRANSMIT block (`V29_OBJ_TX`,
 * +0x24) where its sibling reaches the receive one.  Same shape, same "no
 * intermediate local" ruling, and `R_386_PC32 SDM_scrambler` at 0x0a6577.
 *
 * Two bytes shorter than the receive one for one reason and it is not a
 * difference in the source: `add $0x1c,%eax` has an eight-bit displacement
 * and `add $0x4f3c,%eax` does not.
 */
void
ScrambleDataV29(void *modem, unsigned short *data, unsigned short count)
{
	SDM_scrambler((struct fpm_sdm *)(void *)
		      &((struct v29_tx_root *)modem)->tx->sdm,
		      data, count);
}

/*
 * ---------------------------------------------------------------------------
 * SeedScramblerV29 -- .text 0x0a6580, 15 bytes.
 */
void
SeedScramblerV29(void *modem, int seed)
{
	((struct v29_tx_root *)modem)->tx->sdm.reg = seed;
}

/*
 * ---------------------------------------------------------------------------
 * SetEncoderV29 -- .text 0x0a6590, 43 bytes.
 *
 * `which` is loaded `movswl` and the 32-bit result is compared and
 * decremented, so `short` is forced.  Only 0 and 1 write anything; the object
 * tests for each in turn and returns.  A two-case switch is also the source
 * shape that preserves the object's full-width decrement between the tests.
 *
 * WHAT IT WRITES IS `fpm_smc_cfg`'s `direct`, and that is not inference: the
 * transmitter's block puts `struct fpm_smc` at +0x34 (`v29data.h`, confirmed
 * by `V29TX_create` calling `SMC_init` there) and `direct` is that structure's
 * +0x04, so +0x38 is the one field and there is nothing to choose between.
 * `fpm_smc.h` describes it as "take the quadrant straight out of the data word
 * instead of accumulating pmap increments" -- differential encoding off.
 * Finding F8880.
 */
void
SetEncoderV29(void *modem, short which)
{
	switch (which) {
	case 0:
		((struct v29_tx_root *)modem)->tx->smc.cfg.direct = 0;
		break;
	case 1:
		((struct v29_tx_root *)modem)->tx->smc.cfg.direct = 1;
		break;
	}
}

/*
 * ---------------------------------------------------------------------------
 * ModDataV29 -- .text 0x0a65c0, 89 bytes.
 *
 * One block through the transmit chain: encode the caller's data words into
 * the ring, then shape the ring into samples.
 *
 * `count` GOES TO BOTH CALLS UNCHANGED and is not the same unit in each --
 * `SMC_encoder` takes data words and `FPM_PPS_filter` takes symbols.  The
 * object holds it in `%ebx` across both and stores it into `0xc(%esp)` twice
 * (0x0a65d6 and 0x0a65ef), so there is no conversion to reproduce.
 * `ModDataV27` is the same function over V.27ter's block.
 *
 * The block is re-read from the handle BETWEEN the two calls -- `mov
 * 0x24(%esi),%eax` at 0x0a65da and again at 0x0a65fb -- and the ring is taken
 * from it twice, once per call.  That is the reload pattern of every function
 * in this file and it is what the two locals below spell.
 *
 * THE THREE OFFSETS ARE `v29data.h`'s AND THEY CORROBORATE `V29TX_delete`.
 * `SMC_encoder` types +0x34 and `FPM_PPS_filter` types +0x64, and the delete
 * above releases +0x64 through `FPM_PPS_free` and the ring's three rails --
 * two readings of one block from two functions, neither derived from the
 * other.  Finding F9255.
 */
unsigned short
ModDataV29(void *modem, const unsigned short *bits, short *samples,
	   unsigned short count)
{
	struct v29tx *tx;

	tx = ((struct v29_tx_root *)modem)->tx;
	SMC_encoder(&tx->smc, &tx->ring, bits, count);

	tx = ((struct v29_tx_root *)modem)->tx;
	return FPM_PPS_filter(&tx->pps, &tx->ring, samples, count);
}

unsigned short
TxNoCarrierV29(void *modem, const unsigned short *data, short *out,
	       unsigned short count)
{
	struct fpm_smc_ring *ring;
	unsigned short i, produced;
	short widx, len;
	struct v29tx *fp;

	(void)data;			/* never read; see v29data.h */

	fp = ((struct v29_tx_root *)modem)->tx;
	ring = &fp->ring;
	widx = ring->widx;
	len = ring->len;

	for (i = 0; i < count; i++) {
		short next;

		ring->i[widx] = 0;
		ring->q[widx] = 0;
		next = (short)(widx + 1);
		widx = (short)(next < len ? next : 0);
	}

	produced = FPM_PPS_filter(&fp->pps, ring, out, count);

	/* Back through a FRESH read of the instance pointer, after the shaper. */
	fp = ((struct v29_tx_root *)modem)->tx;
	fp->ring.widx = widx;

	return produced;
}
