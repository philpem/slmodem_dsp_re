/*
 * Vmi_v29.c -- the V.29 virtual-modem interface adapters and message
 * reporters, split out of V29rx.c/V29tx.c and class1tx.c into the blob's
 * Vmi_v29.c translation unit.  Bodies moved verbatim; see finding F11390.
 */
#include <stddef.h>
#include "dsplib/class1tx.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v29cfg.h"
#include "dsplib/v29fax.h"

char *V29RX_MESG[8] = {
	"V.29 Receive Data Mode",
	"V.29 Receive Protocol Mode",
	"V.29 Rx Start of Protocol",
	"ERROR: V.29 Rx Internal Error",
	"ERROR: Loss Carrier In V.29 Rx Protocol",
	"V.29 Rx Idle (End of Transmition)",
	"CONNECT: V.29 Receive 9600 bps",
	"CONNECT: V.29 Receive 7200 bps",
};

char *V29TX_MESG[8] = {
	"V.29 Transmit Data Mode",
	"V.29 Protocal Transmit Mode",
	"CONNECT: V.29 Transmit 9600 bps",
	"CONNECT: V.29 Transmit 7600 bps",	/* sic: 7200 misspelled */
	"V.29 Transmit Idle Mode",
	"ERROR: V.29 Tx Internal Error",
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

MESSAGE_FN(v29tx_message, V29TX_MESG, 8)
MESSAGE_FN(v29rx_message, V29RX_MESG, 8)

void
v29tx_create(struct faxvmi_link *dp, const struct v29tx_cfg *cfg)
{
	struct v29tx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V29TX_CFG;

	handle = V29TX_create((void *)(long)dp->int_0014, &local);
	dp->pack_count = 0x30;
	dp->int_0014 = (int)(long)handle;
	dp->pack_width = (local.bitrate == 7200) ? 3 : 4;
	dp->unpack_width = 0;
}

void
v29rx_create(struct faxvmi_link *dp, const struct v29rx_cfg *cfg)
{
	struct v29rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V29RX_CFG;

	handle = V29RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;
	dp->unpack_width = (local.bit_rate == 7200) ? 3 : 4;
	dp->pack_width = 0;
}

void
v29tx_delete(struct faxvmi_link *dp)
{
	V29TX_delete((void *)(long)dp->int_0014);
}

void
v29rx_delete(struct faxvmi_link *dp)
{
	V29RX_delete((void *)(long)dp->int_0014);
}

void
v29tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		     unsigned short *result)
{
	V29TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

void
v29rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		     unsigned short *count)
{
	V29RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf, count);
	*result = *count;
	*count = 0;
}

int
v29tx_status(struct faxvmi_link *dp, void *status)
{
	return V29TX_status((void *)(long)dp->int_0014, status);
}

int
v29rx_status(struct faxvmi_link *dp, void *status)
{
	return V29RX_status((void *)(long)dp->int_0014, status);
}

int
v29tx_control(struct faxvmi_link *dp, const struct v29tx_control_req *arg)
{
	return V29TX_control((void *)(long)dp->int_0014, arg);
}

int
v29rx_control(struct faxvmi_link *dp, const struct v29rx_control_req *arg)
{
	return V29RX_control((void *)(long)dp->int_0014, arg);
}
