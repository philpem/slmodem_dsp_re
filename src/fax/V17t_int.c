/*
 * V17t_int.c -- split out of the merged v17.c so the definitions sit in
 * the translation unit the object's FILE order gives them.  Bodies
 * moved verbatim; no source text changed.  See finding F11390.
 */
#include <string.h>

#include "dsplib/v17fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17cfg.h"
#include "dsplib/v17dec.h"
#include "dsplib/v32smc.h"
#include "dsplib/vtb.h"

#define RXROOT(modem)		((struct v17rx *)(modem))
#define TXROOT(modem)		((struct v17tx *)(modem))
#define RXCTL(modem)		(RXROOT(modem)->ctl)
#define RXSTATE(modem)		(RXROOT(modem)->state)
#define TXPRIV(modem)		(TXROOT(modem)->priv)
#define TXBLOCK(modem)		(TXROOT(modem)->fp)
#define CTL(modem)		RXCTL(modem)
#define RXS(modem)		RXSTATE(modem)
#define TXP(modem)		TXPRIV(modem)
#define TXFP(modem)		TXBLOCK(modem)

#include "dsplib/smc.h"
#include "dsplib/v17data.h"

void
ScrambleDataV17(void *modem, unsigned short *data, unsigned short count)
{
	SDM_scrambler(&TXBLOCK(modem)->sdm, data, count);
}

void
SeedScramblerV17(void *modem, unsigned int seed)
{
	TXBLOCK(modem)->sdm.reg = seed;
}

void
SetEncoderV17(void *modem, short which, short arg)
{
	struct v17tx_fp *fp;

	switch (which) {
	case V17_ENCODER_DIF:
		TXBLOCK(modem)->encoder_sel = V17_ENCODER_DIF;
		TXBLOCK(modem)->smc.quad = arg;
		break;
	case V17_ENCODER_ABS:
		/* No second value on this arm; see v17fax.h. */
		TXBLOCK(modem)->encoder_sel = V17_ENCODER_ABS;
		break;
	case V17_ENCODER_TCM:
		TXBLOCK(modem)->encoder_sel = V17_ENCODER_TCM;
		TXBLOCK(modem)->smc.quad = arg;
		break;
	default:
		break;
	}
}

/*
 * SMCv17_init -- .text 0x0a0a60, 87 bytes.  See v17data.h for what this
 * confirms about the five neutral fields it clears.
 */
void
SMCv17_init(void *smc, const short *cfg)
{
	struct v17_smc *statep = (struct v17_smc *)smc;

	if (cfg == NULL)
		cfg = SMCv17_CFG;

	memcpy(&statep->mode, cfg, sizeof(short[2]));
	statep->quad = 0;
	statep->state = 0;
	statep->trellis = 0;
	statep->prev = 0;
	statep->r10 = 0;
}

/*
 * SetTxModeV17 -- .text 0x0a0ac0, 625 bytes.
 *
 * (Re)configures the transmitter's training-sequence detector, descrambler
 * and SMCv17 coder state for one of four symbol rates and returns nothing.
 * `mode` 0..3 select 16T/32/64/128-point constellations (V17TX_SYM_SIZE[mode]
 * bits/symbol: 3, 4, 5, 6) in that order; anything else writes the same
 * "unsupported" pair `V17TX_modem` writes on a short FIFO write, with one
 * extra bit -- see `V17TX_RESULT_BYTE_07` in v17fax.h.
 *
 * THE SHIFT REGISTER SURVIVES ITS OWN RE-INIT.  `SDM_init` clears
 * `struct fpm_sdm::reg` like every other field of its target, but this
 * function reads `V17FP_SDM`'s `reg` BEFORE calling it and writes the same
 * value back AFTER (`0xa0b51` / `0xa0baa`) -- so a caller re-arming the
 * transmitter for a rate change keeps the descrambler's running state rather
 * than restarting it at zero.  Reproduced, not second-guessed: nothing here
 * says why, and the object does it on every call, recognised mode or not.
 *
 * SGD_CREATE'S CFG IS SGD_CFG WITH ONLY `sym_bits` CHANGED -- no det.*
 * override the way `V17RX_create` gives its own training detector, so the
 * request threshold and pattern stay whatever `SGD_CFG` ships.  The existing
 * `struct sgd *` at `V17TXP_SGD` is reused if the caller already built one;
 * `SGD_create` allocates only when it is NULL.
 */
void
SetTxModeV17(void *modem, short mode)
{
	struct sgd_cfg sgdcfg;
	struct fpm_sdm_cfg sdmcfg;
	struct fpm_sdm *sdm;
	struct v17tx_fp *fp;
	struct v17tx_priv *prm;
	short sym_size;
	unsigned int saved_reg;

	sym_size = V17TX_SYM_SIZE[mode];

	sgdcfg = SGD_CFG;
	sgdcfg.sym_bits = sym_size;
	prm = TXPRIV(modem);
	prm->sgd =
		SGD_create((struct sgd *)prm->sgd, &sgdcfg);

	sdmcfg = SDM_CFG;
	sdmcfg.nbits = sym_size;
	sdmcfg.tap1 = 0x12;
	sdmcfg.tap2 = 0x17;

	fp = TXBLOCK(modem);
	sdm = &fp->sdm;
	saved_reg = sdm->reg;
	SDM_init(sdm, &sdmcfg);
	sdm->reg = saved_reg;

	fp = TXBLOCK(modem);
	memcpy(&fp->smc, SMCv17_CFG, sizeof(SMCv17_CFG));
	fp->smc.state = 0;
	fp->smc.quad = 0;
	fp->smc.prev = 0;
	fp->smc.trellis = 0;
	fp->smc.r10 = 0;
	fp->smc.r02 = 2;
	fp->encoder_sel = 2;

	switch (mode) {
	case 0:
		fp->smc.nbits = 1;
		fp->smc.mode.word = 3;
		fp->pps.cfg.imap = VTBv17_IMAP16T;
		fp->pps.cfg.qmap = VTBv17_QMAP16T;
		prm = TXPRIV(modem);
		prm->no_carrier_sym = 0x10;
		break;
	case 1:
		fp->smc.nbits = 2;
		fp->smc.mode.word = 2;
		fp->pps.cfg.imap = VTBv17_IMAP32;
		fp->pps.cfg.qmap = VTBv17_QMAP32;
		prm = TXPRIV(modem);
		prm->no_carrier_sym = 0x20;
		break;
	case 2:
		fp->smc.nbits = 3;
		fp->smc.mode.word = 4;
		fp->pps.cfg.imap = VTBv17_IMAP64;
		fp->pps.cfg.qmap = VTBv17_QMAP64;
		prm = TXPRIV(modem);
		prm->no_carrier_sym = 0x40;
		break;
	case 3:
		fp->smc.nbits = 4;
		fp->smc.mode.word = 5;
		fp->pps.cfg.imap = VTBv17_IMAP128;
		fp->pps.cfg.qmap = VTBv17_QMAP128;
		prm = TXPRIV(modem);
		prm->no_carrier_sym = 0x80;
		break;
	default:
		TXROOT(modem)->result.byte.status = V17TX_RESULT_BYTE_07;
		TXROOT(modem)->result.byte.flags = (unsigned char)
			((TXROOT(modem)->result.byte.flags
			  | V17TX_RESULT_B1_BIT1) & ~1);
		break;
	}
}

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
