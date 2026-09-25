/*
 * V27t_int.c -- split out of the merged v27.c so the definitions sit in the
 * translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <string.h>

#include "dsplib/v27fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/faxfifo.h"
#include "dsplib/sdmv27.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v27cfg.h"

/*
 * The transmitter's scrambler.  `DescrambleDataV27` above is the same shape
 * over a different block: the scrambler lives in the TRANSMITTER's, at
 * tx + V27TX_SDM, and the descrambler in the receiver's.
 */
void
ScrambleDataV27(void *modem, unsigned short *data, short count)
{
	SDMv27_scrambler((struct sdmv27 *)(void *)
				&((struct v27_tx *)modem)->tx->sdm,
			 data, count);
}

/*
 * SetScramblerV27 .text 0x0a5e90, 92 bytes.
 *
 * Reseed the scrambler for the rate `TxNextStateV27`'s EQCOND arm has just
 * settled on.  `sdmv27.h` already carries this derivation -- written before
 * this file reconstructed the function that needed it -- and this is that
 * derivation typed out: `reg` is saved across `SDMv27_init` and put back by
 * hand, because `SDMv27_init` has no separate reset and would otherwise drop
 * the shift register's running state on every rate-driven reseed.
 */
void
SetScramblerV27(void *modem)
{
	struct sdmv27_cfg cfg;
	struct v27_tx_source *prm;
	struct sdmv27 *sdm;
	short rate;
	unsigned short reg;

	cfg = SDMv27_CFG;

	prm = ((struct v27_tx *)modem)->source;
	rate = prm->rate;
	cfg.nbits = (unsigned short)V27TX_SDM_NUM_BITS[rate];

	sdm = &((struct v27_tx *)modem)->tx->sdm;
	reg = sdm->reg;

	SDMv27_init(sdm, &cfg);

	sdm = &((struct v27_tx *)modem)->tx->sdm;
	sdm->reg = reg;
}

/*
 * One block through the transmit chain.
 *
 * `count` GOES TO BOTH CALLS UNCHANGED, and it is not the same unit in each:
 * `SMC_encoder` takes data words and `FPM_PPS_filter` takes symbols.  The
 * object holds the caller's count in `%ebx` across both and stores it into
 * `0xc(%esp)` twice, so there is no conversion to reproduce.
 *
 * The ring is loaded from `tx + 0x08` twice, once per call, and `tx` itself is
 * re-read from the instance in between -- the reload pattern of every function
 * in this file.
 */
unsigned short
ModDataV27(void *modem, const unsigned short *bits, short *samples,
	   unsigned short count)
{
	struct v27_tx_block *tx;

	tx = ((struct v27_tx *)modem)->tx;
	SMC_encoder(&tx->smc,
		    &tx->ring,
		    bits, count);

	tx = ((struct v27_tx *)modem)->tx;
	return FPM_PPS_filter(&tx->pps,
			      (struct fpm_smc_ring *)(void *)
					&tx->ring,
			      samples, count);
}

/*
 * TxNoCarrierV27 .text 0x0a5f50, 151 bytes.
 *
 * Fill `count` symbol-ring slots with `V27TX_NOCARR_SYMBOL[rate]` -- wrapping
 * `widx` against `struct fpm_smc_ring::len` by hand, one slot at a time,
 * rather than through `FPM_SMC_encoder` -- then run the pulse shaper over
 * `count` samples.  `sym` and `len` are read out of the ring ONCE, before the
 * loop, and only `rate` is re-read every iteration -- the object's own
 * register/reload discipline, and `in` is UNREAD, exactly as its callers'
 * own `in` buffers go untouched here.
 */
short
TxNoCarrierV27(void *modem, unsigned short *in, short *out,
	      unsigned short count)
{
	struct v27_tx_block *tx = ((struct v27_tx *)modem)->tx;
	struct fpm_smc_ring *ring =
		&tx->ring;
	short *sym = ring->sym;
	short len = ring->len;
	short widx = ring->widx;
	unsigned short i;
	short r;

	(void)in;

	for (i = 0; i < count; i++) {
		struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
		short rate = prm->rate;

		sym[widx] = V27TX_NOCARR_SYMBOL[rate];
		widx = (short)((widx + 1 < len) ? widx + 1 : 0);
	}

	r = (short)FPM_PPS_filter(
		&tx->pps,
		&tx->ring,
		out, count);

	tx = ((struct v27_tx *)modem)->tx;
	(&tx->ring)->widx = widx;

	return r;
}
