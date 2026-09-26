/*
 * V17t_stc.c -- split out of the merged v17.c so the definitions sit in
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

/*
 * ---------------------------------------------------------------------------
 * V17TX_control -- .text 0x0a1b30, 148 bytes.  See v17fax.h for the five
 * effects, the request type's derivation and the `params` identity this
 * function settles for `V17TX_status`.
 */
int
V17TX_control(void *fp, const struct v17tx_control_req *req)
{
	struct v17tx_priv *priv;
	struct v17tx_fp *block;
	struct fpm_pps *pps;
	short mode;

	if (req == 0)
		return 0;

	priv = TXPRIV(fp);
	block = TXBLOCK(fp);
	mode = priv->mode;

	pps = &block->pps;
	pps->cfg.scale = req->scale_mul;
	pps->cfg.scale = V17TX_PPS_SCALE[mode] * req->scale_mul;

	((struct v17tx_cfg *)fp)->int_0018 = req->int_0010;
	((struct v17tx_cfg *)fp)->int_0008 = req->int_0004;

	if (req->ctl0 & V17TXCTL_CTL0_BIT2)
		*(unsigned char *)(void *)&TXROOT(fp)->cfg.int_0010
			|= V17_STATUS_FLAG_04;	/* struct v17tx_cfg::
							 * int_0010's low byte */

	priv->r08 = 0;
	if (req->ctl1 & V17TXCTL_CTL1_BIT4)
		priv->r08 = 1;

	if (req->ctl1 & V17TXCTL_CTL1_BIT1) {
		V17TX_create(fp, fp);
		return 1;
	}

	return 1;
}

int
V17TX_status(void *params, struct v17_status *status)
{
	unsigned char *p;

	if (status == 0)
		return 0;

	p = (unsigned char *)params;

	status->protocol = TXROOT(params)->cfg.protocol;
	status->tx_bps = TXROOT(params)->cfg.bitrate;
	status->rx_bps = 0;
	status->snr_ok = 0;
	status->snr = 0;
	status->short_0a = 0;
	status->short_0c = 0;
	status->short_10 = TXROOT(params)->cfg.bitrate;
	status->short_12 = 0;

	/*
	 * The first of these two writes to `flags` is dead and is the
	 * object's; see v17fax.h and D1032.  It stays because the load of
	 * `p[0x10]` sits between them and may alias.
	 */
	status->flags &= (unsigned char)~V17_STATUS_FLAGS_CLEAR;
	status->flags1 &= (unsigned char)~V17_STATUS_FLAGS1_CLEAR;
	status->flags = (unsigned char)(p[0x10] & V17_STATUS_FLAG_04);

	status->int_18 = TXROOT(params)->cfg.int_0018;

	return 1;
}
