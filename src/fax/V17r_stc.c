/*
 * V17r_stc.c -- split out of the merged v17.c so the definitions sit in
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
 * V17RX_control -- .text 0x0a0880, 131 bytes.
 *
 * See v17fax.h for the derivation of `struct v17rx_ctl` and for why the
 * self-referential `V17RX_create(modem, modem)` call below is a legitimate
 * reinit-with-current-config and not the aliasing defect it first looks
 * like (finding F9470, the receive instance's head IS its own config
 * struct).
 *
 * THE MERGE IS BEHAVIOURAL, NOT A SIMPLIFICATION.  The object tests
 * `flags_0d`'s bit 4 twice -- once on each of the two paths through bit 1 --
 * and both paths converge on the same `flags_0c` tail (`jmp 0x0a08af`).
 * Written straight-line, that is exactly the order below: the `int_0008`
 * write, then the `V17RXC_INT_0008` write, then the conditional reinit,
 * then both `flags_0c` clears unconditionally.
 */
int
V17RX_control(void *modem, const struct v17rx_ctl *arg)
{
	struct v17rx_cfg *cfg = (struct v17rx_cfg *)modem;

	if (arg == NULL)
		return 0;

	cfg->int_0008 = arg->int_0004;

	RXCTL(modem)->r08 =
		(arg->flags_0d & V17RXCTL_SET_CTL_INT_0008) != 0;

	if (arg->flags_0d & V17RXCTL_REINIT) {
		cfg->short_train = arg->short_train;
		V17RX_create(modem, (const struct v17rx_cfg *)modem);
	}

	if (arg->flags_0c & V17RXCTL_CLEAR_STATE0)
		RXSTATE(modem)->r00 = 0;

	if (arg->flags_0c & V17RXCTL_CLEAR_STATE10)
		RXSTATE(modem)->r10 = 0;

	return 1;
}

/*
 * V17RX_status -- .text 0x0a0910, 190 bytes.
 *
 * THE FLAGS BYTE IS FOUR STORES AND THE VALUE IT SETTLES ON IS DETERMINISTIC.
 * Reading the object's chain from the incoming byte `b`, with `x` for the
 * three state bits:
 *
 *     store 1  b & 0xfe
 *     store 2  ((b & 0xfc) | x1) & 0xfb          =  (b & 0xf8) | x1
 *     store 3  (((b & 0xf8) & 0xf3) | x3) | 0x10 = ((b & 0xf0) | x1 | x3 | 0x10)
 *     store 4  ((... & 0xdf) | x5 | 0x40) & 0x7f
 *
 * -- and the last line leaves `(b & 0x50) | x1 | x3 | x5 | 0x10 | 0x40`, in
 * which bits 4 and 6 of `b` are re-set by the two constants anyway.  So the
 * result is `0x50 | x1 | x3 | x5` and NOTHING of the caller's byte survives.
 * The three intermediate stores are the object's and are kept: each is
 * separated from the next by a load of `V17RX_OBJ_STATE`, which is reached
 * through a character type and may alias the status block, so a source that
 * assigned once could not have produced them.  They are observable only to a
 * caller that overlaps its two arguments, which is what deviation D1092
 * records.
 */
int
V17RX_status(void *modem, struct v17_status *status)
{
	unsigned char *rx;

	if (status == 0)
		return 0;

	/*
	 * `modem` stays a byte pointer for the same reason `V17TX_status`'s
	 * `params` does: it is an unmodelled block, and it is what keeps the
	 * stores above from being merged.
	 */
	rx = (unsigned char *)modem;

	status->protocol = (short)RXROOT(modem)->cfg.protocol;
	status->tx_bps = 0;
	status->rx_bps = RXROOT(modem)->cfg.bit_rate;
	status->snr_ok = (short)
		((rx[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_LOW_SNR) == 0);
	status->snr = GetSNRV17(modem);
	status->short_0a = 0;
	status->short_0e = 0;
	status->short_10 = 0;
	status->short_12 = RXROOT(modem)->cfg.bit_rate;

	status->flags &= (unsigned char)~V17_STATUS_FLAG_01;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_02)
		 | ((RXSTATE(modem)->r1c & V17RXS_001C_BIT0)
		    << 1));
	status->flags &= (unsigned char)~V17_STATUS_FLAG_04;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_08)
		 | ((RXSTATE(modem)->r00 == 0) << 3));
	status->flags |= V17_STATUS_FLAG_10;
	status->flags1 &= (unsigned char)~V17_STATUS_FLAGS1_CLEAR;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_20)
		 | ((RXSTATE(modem)->r10 == 0) << 5));
	status->flags |= V17_STATUS_FLAG_40;
	status->flags &= (unsigned char)~V17_STATUS_FLAG_80;

	return 1;
}
