/*
 * V27r_stc.c -- split out of the merged v27.c so the definitions sit in the
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
 * V27RX_control .text 0x0a32a0, 125 bytes.
 *
 * See v27fax.h for the request's own layout.  `int_0008` and the
 * `V27SH_INT_0004` reset are unconditional; the mask byte's two disables and
 * the flags byte's force/reinit are each gated on their own bit.
 */
int
V27RX_control(void *rx, void *req)
{
	struct v27rx_ctl *ctl = (struct v27rx_ctl *)req;
	struct v27_rx_shared *sh;
	struct v27_rx_block *rxb;
	unsigned char flags;
	unsigned char mask;

	if (req == 0)
		return 0;

	((struct v27_rx *)rx)->cfg.int_0008 = ctl->int_0004;
	sh = ((struct v27_rx *)rx)->shared;
	sh->int_0004 = 0;

	flags = ctl->flags;
	if (flags & V27RXCTL_FLAGS_FORCE_NOCARRIER)
		sh->int_0004 = 1;
	if (flags & V27RXCTL_FLAGS_REINIT)
		V27RX_create(rx, &((struct v27_rx *)rx)->cfg);

	mask = ctl->mask;
	rxb = ((struct v27_rx *)rx)->rx;
	if (mask & V27RXCTL_MASK_DISABLE_00)
		rxb->int_0000 = 0;
	if (mask & V27RXCTL_MASK_DISABLE_FSE_LMS)
		rxb->en_fse_lms = 0;

	return 1;
}

/*
 * Report whether the caller supplied a receive status block.
 *
 * The block is not filled: eleven bytes, and the first argument is not read.
 */
int
V27RX_status(void *rx, void *status)
{
	(void)rx;
	return status != 0;
}
