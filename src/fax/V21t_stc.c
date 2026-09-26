/*
 * V21t_stc.c -- split out of the merged v21.c so the definitions sit in the
 * translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21fax.h"

/*
 * V21TX_control -- .text 0x0a2ba0, 94 bytes.
 *
 * `arg == NULL` returns 0 and touches nothing.  Otherwise, in the object's
 * own order: `dsp->fsm.cfg.scale` is set from `arg->scale`, narrowed to
 * `short` as the object narrows it; `cfg->int_0008` (the transmit handle's
 * own head, `struct v21tx_cfg`) is set unconditionally from `arg->int_0004`;
 * `arg->mask`'s bit 2 ORs into `V21TX_FLAGS(modem)`; `arg->flags`'s
 * bit 4 sets `V21TXP_INT_0004` (of the params block at `V21TX_OBJ_PARAMS`)
 * to a boolean; and bit 1 of the same byte calls `V21TX_create(modem,
 * modem)` -- the self-referential reinit `V17RX_control`'s header comment
 * documents at length (finding F9900): the transmit handle's head IS its
 * own `struct v21tx_cfg`, so passing it as its own `params` re-copies its
 * current configuration onto itself and reruns construction.
 */
int
V21TX_control(void *modem, const struct v21tx_ctl *arg)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21tx_cfg *cfg = &tx->cfg;
	struct v21_tx_hdx *hdx = tx->hdx;

	if (arg == NULL)
		return 0;

	tx->dsp->fsm.cfg.scale = (short)arg->scale;
	cfg->int_0008 = arg->int_0004;

	if (arg->mask & V21TXCTL_SET_TXFLAGS_BIT2)
		((unsigned char *)(void *)&tx->cfg)[V21TX_OBJ_FLAGS] |=
			V21TXCTL_SET_TXFLAGS_BIT2;

	hdx->int_0004 =
		(arg->flags & V21TXCTL_SET_PARAMS_INT0004) != 0;

	if (arg->flags & V21TXCTL_REINIT)
		V21TX_create(modem, (const struct v21tx_cfg *)modem);

	return 1;
}

/*
 * Fill the caller's status block.
 *
 * The last statement ASSIGNS the flags byte rather than merging into it, so
 * the two bits cleared four statements earlier are cleared for nothing and
 * every other bit the caller had is lost.  That is the object's; see D1037.
 */
int
V21TX_status(void *modem, struct v21_status *st)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	unsigned char tx_flags;

	if (st == NULL)
		return 0;

	tx_flags = ((unsigned char *)(void *)&tx->cfg)[V21TX_OBJ_FLAGS];
	st->protocol = tx->cfg.protocol;
	st->tx_bps = V21_STATUS_BPS;
	st->rx_bps = 0;
	st->quality = 0;
	st->snr = 0;
	st->short_0a = 0;
	st->short_0c = 0;
	st->flags &= (unsigned char)~(V21_STATUS_BIT0 | V21_STATUS_BIT1);
	st->short_10 = 0;
	st->short_12 = 0;
	st->flags1 &= (unsigned char)~V21_STATUS1_BIT0;
	st->flags = (unsigned char)(tx_flags & V21_STATUS_BIT2);

	return 1;
}
