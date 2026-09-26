/*
 * V27t_stc.c -- split out of the merged v27.c so the definitions sit in the
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
 * V27TX_control .text 0x0a3e30, 148 bytes.
 *
 * See v27fax.h for the request's own layout.  The pulse shaper's gain and
 * `int_0008`/`int_0018` are unconditional; `V27TX_HANDLE_FLAGS`'s bit and
 * `int_0008`'s force/reinit are each gated on their own bit -- `V27RX_
 * control`'s own shape, transmit side.
 */
int
V27TX_control(void *modem, void *req)
{
	struct v27tx_ctl *ctl = (struct v27tx_ctl *)req;
	struct v27_tx_source *prm;
	struct fpm_pps *pps;
	short rate;
	unsigned char mask;
	unsigned char flags;

	if (req == 0)
		return 0;

	prm = ((struct v27_tx *)modem)->source;
	pps = (struct fpm_pps *)(void *)
		&((struct v27_tx *)modem)->tx->pps;
	rate = prm->rate;

	pps->cfg.scale = ctl->scale_mul *
		V27TX_PPS_SCALE[rate];

	((struct v27_tx *)modem)->cfg.int_0018 = ctl->int_0010;
	((struct v27_tx *)modem)->cfg.int_0008 = ctl->int_0004;

	mask = ctl->mask;
	if (mask & V27TXCTL_MASK_HANDLE_FLAG_04)
		*((unsigned char *)(void *)&((struct v27_tx *)modem)->cfg.flags) |= 0x04;

	prm->int_0008 = 0;

	flags = ctl->flags;
	if (flags & V27TXCTL_FLAGS_FORCE_INT_0008)
		prm->int_0008 = 1;
	if (flags & V27TXCTL_FLAGS_REINIT)
		V27TX_create(modem, &((struct v27_tx *)modem)->cfg);

	return 1;
}

/*
 * Fill the caller's status block from the transmitter's.
 *
 * THE TWO WRITES TO +0x14 ARE BOTH THE OBJECT'S, and the first is not dead.
 * The second reads the SOURCE's +0x10, and nothing tells the compiler the two
 * blocks do not overlap -- so the store has to happen first, and it is
 * observable exactly when they do.  See D1033.
 */
int
V27TX_status(const void *tx, void *status)
{
	struct v27_status_prefix *st = (struct v27_status_prefix *)status;
	unsigned char flags;

	if (status == 0)
		return 0;

	st->protocol = ((const struct v27_tx *)tx)->cfg.protocol;
	st->tx_bps = ((const struct v27_tx *)tx)->cfg.bitrate;
	st->rx_bps = 0;
	st->quality = 0;
	st->zero_08 = 0;
	st->zero_0a = 0;
	st->zero_0c = 0;
	/*
	 * The SOURCE IS READ AGAIN, not reused: `movzwl 0x2(%ebx),%eax` at
	 * a3f0b after the store at a3efb.  Observable only if the two blocks
	 * overlap, and what the compiler was forced to encode.
	 */
	st->word_10 = ((const struct v27_tx *)tx)->cfg.bitrate;
	st->zero_12 = 0;

	/*
	 * V.17, V.21 and V.29 spell this `flags &= ~(BIT0 | BIT1)`.  V.27ter
	 * SETS bit 0 instead of clearing it, which changes what the last line
	 * of the function produces.  Finding F8866, deviation D1033.
	 */
	flags = (unsigned char)(st->flags
				| V27STAT_FLAGS_BIT0);
	st->flags =
			(unsigned char)(flags & (unsigned char)~V27STAT_FLAGS_BIT1);
	/*
	 * Compute the final byte before clearing +0x15.  The object loads the
	 * source's +0x10 first, then performs the clear, then stores this value.
	 * Reusing `flags` is the ordinary-source form that preserves that order
	 * under GCC 3.4.2; see finding F10231.
	 */
	flags = (unsigned char)((flags & V27STAT_FLAGS_BIT0)
				| ((unsigned char)((const struct v27_tx *)tx)->cfg.flags
				   & V27STAT_FLAGS_FROM_TX));
	st->flags2 &= (unsigned char)~V27STAT_FLAGS2_BIT0;
	st->flags = flags;

	st->word_18 = ((const struct v27_tx *)tx)->cfg.int_0018;

	return 1;
}
