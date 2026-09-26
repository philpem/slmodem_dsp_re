/*
 * Vmi_v27.c -- the V.27 virtual-modem interface adapters and message
 * reporters, split out of V17rx.c/V17tx.c and class1tx.c into the blob's
 * Vmi_v27.c translation unit (FILE order: V29txtab.c, Vmi_v17.c..Vmi_v29.c,
 * Vtb_tab.c).  Bodies moved verbatim; see finding F11390.
 */
#include <stddef.h>
#include "dsplib/class1tx.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v27cfg.h"
#include "dsplib/v27fax.h"

char *V27RX_MESG[8] = {
	"V.27 Receive Data Mode",
	"V.27 Receive Protocol Mode",
	"V.27 Rx Start of Protocol",
	"ERROR: V.27 Rx Internal Error",
	"ERROR: Loss Carrier In V.27 Rx Protocol",
	"V.27 Rx Idle (End of Transmition)",
	"CONNECT: V.27 Receive 2400 bps",
	"CONNECT: V.27 Receive 4800 bps",
};

char *V27TX_MESG[8] = {
	"V.27ter Transmit Data Mode",
	"V.27ter Protocal Transmit Mode",
	"CONNECT: V.27ter Transmit 4800 bps",
	"CONNECT: V.27ter Transmit 2400 bps",
	"V.27ter Transmit Idle Mode",
	"ERROR: V.27ter Tx Internal Error",
	"ERROR: Transmit Input Queue Under-run",
	"ERROR: Transmit Input Queue Over-run",
};

#define MESSAGE_FN(name, table, guard)					\
	void								\
	name(void *handle, int code, char **out)			\
	{								\
		(void)handle;						\
		if ((unsigned)code > (guard))				\
			*out = NULL;					\
		else							\
			*out = (table)[(unsigned char)code];		\
	}

MESSAGE_FN(v27tx_message, V27TX_MESG, 8)
MESSAGE_FN(v27rx_message, V27RX_MESG, 8)

void
v27tx_create(struct faxvmi_link *dp, const struct v27tx_cfg *cfg)
{
	struct v27tx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V27TX_CFG;

	handle = V27TX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;

	dp->pack_count = (local.bitrate == 2400) ? 24 : 32;
	dp->unpack_width = 0;
	dp->pack_width = (local.bitrate == 2400) ? 2 : 3;
}

void
v27rx_create(struct faxvmi_link *dp, const struct v27rx_cfg *cfg)
{
	struct v27rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V27RX_CFG;

	handle = V27RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;
	dp->unpack_width = (local.bit_rate == 2400) ? 2 : 3;
	dp->pack_width = 0;
}

void
v27tx_delete(struct faxvmi_link *dp)
{
	V27TX_delete((void *)(long)dp->int_0014);
}

void
v27rx_delete(struct faxvmi_link *dp)
{
	V27RX_delete((void *)(long)dp->int_0014);
}

void
v27tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		     unsigned short *result)
{
	V27TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

void
v27rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		     unsigned short *count)
{
	V27RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf, count);
	*result = *count;
	*count = 0;
}

int
v27tx_status(struct faxvmi_link *dp, void *status)
{
	return V27TX_status((const void *)(long)dp->int_0014, status);
}

int
v27rx_status(struct faxvmi_link *dp, void *status)
{
	return V27RX_status((void *)(long)dp->int_0014, status);
}

int
v27tx_control(struct faxvmi_link *dp, void *req)
{
	return V27TX_control((void *)(long)dp->int_0014, req);
}

int
v27rx_control(struct faxvmi_link *dp, void *req)
{
	return V27RX_control((void *)(long)dp->int_0014, req);
}
