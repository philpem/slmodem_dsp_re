/*
 * Vmi_v17.c -- the V.17 virtual-modem interface adapters and message
 * reporters, split out of V17rx.c/V17tx.c and class1tx.c into the blob's
 * Vmi_v17.c translation unit.  Bodies moved verbatim; see finding F11390.
 */
#include <stddef.h>
#include "dsplib/class1tx.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v17cfg.h"
#include "dsplib/v17fax.h"

char *V17RX_MESG[10] = {
	"V.17 Receive Data Mode",
	"V.17 Receive Protocol Mode",
	"V.17 Rx Start of Protocol",
	"ERROR: V.17 Rx Internal Error",
	"ERROR: Loss Carrier In V.17 Rx Protocol",
	"V.17 Rx Idle (End of Transmition)",
	"CONNECT: V.17 Receive 14400 bps",
	"CONNECT: V.17 Receive 12000 bps",
	"CONNECT: V.17 Receive 9600 bps",
	"CONNECT: V.17 Receive 7200 bps",
};

char *V17TX_MESG[10] = {
	"V.17 Transmit Data Mode",
	"V.17 Protocal Transmit Mode",
	"CONNECT: V.17 Transmit 14400 bps",
	"CONNECT: V.17 Transmit 12000 bps",
	"CONNECT: V.17 Transmit 9600 bps",
	"CONNECT: V.17 Transmit 7200 bps",
	"V.17 Transmit Idle Mode",
	"ERROR: V.17 Tx Internal Error",
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

MESSAGE_FN(v17tx_message, V17TX_MESG, 10)
MESSAGE_FN(v17rx_message, V17RX_MESG, 10)

void
v17tx_create(struct faxvmi_link *dp, const struct v17tx_cfg *cfg)
{
	struct v17tx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V17TX_CFG;

	handle = V17TX_create((void *)(long)dp->int_0014, &local);
	dp->pack_count = 0x30;
	dp->int_0014 = (int)(long)handle;

	if (local.bitrate == 14400) {
		dp->pack_width = 6;
		dp->unpack_width = 0;
	} else if (local.bitrate == 12000) {
		dp->pack_width = 5;
		dp->unpack_width = 0;
	} else {
		dp->unpack_width = 0;
		dp->pack_width = (short)((local.bitrate == 9600) ? 4 : 3);
	}
}

void
v17rx_create(struct faxvmi_link *dp, const struct v17rx_cfg *cfg)
{
	struct v17rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V17RX_CFG;

	handle = V17RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;

	if (local.bit_rate == 14400) {
		dp->unpack_width = 6;
		dp->pack_width = 0;
	} else if (local.bit_rate == 12000) {
		dp->unpack_width = 5;
		dp->pack_width = 0;
	} else {
		dp->pack_width = 0;
		dp->unpack_width = (local.bit_rate == 9600) ? 4 : 3;
	}
}

void
v17tx_delete(struct faxvmi_link *dp)
{
	V17TX_delete((void *)(long)dp->int_0014);
}

void
v17rx_delete(struct faxvmi_link *dp)
{
	V17RX_delete((void *)(long)dp->int_0014);
}

void
v17tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		     unsigned short *result)
{
	V17TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

void
v17rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		     unsigned short *count)
{
	V17RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf, count);
	*result = *count;
	*count = 0;
}

int
v17tx_status(struct faxvmi_link *dp, struct v17_status *status)
{
	return V17TX_status((void *)(long)dp->int_0014, status);
}

int
v17rx_status(struct faxvmi_link *dp, struct v17_status *status)
{
	return V17RX_status((void *)(long)dp->int_0014, status);
}

int
v17tx_control(struct faxvmi_link *dp, const struct v17tx_control_req *arg)
{
	return V17TX_control((void *)(long)dp->int_0014, arg);
}

int
v17rx_control(struct faxvmi_link *dp, const struct v17rx_ctl *arg)
{
	return V17RX_control((void *)(long)dp->int_0014, arg);
}
