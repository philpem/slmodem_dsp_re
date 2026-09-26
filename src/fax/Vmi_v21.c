/*
 * Vmi_v21.c -- the V.21 virtual-modem interface adapters and message
 * reporters, split out of V21rx.c/V21tx.c and class1tx.c into the blob's
 * Vmi_v21.c translation unit.  Bodies moved verbatim; see finding F11390.
 */
#include <stddef.h>
#include "dsplib/class1tx.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"

char *V21RX_MESG[7] = {
	"V.21 Receive Data Mode",
	"V.21 Rx Start of Protocol",
	"V.21 Rx Wait for Valid Data Protocol",
	"ERROR: V.21 Rx Internal Error",
	"ERROR: Loss Carrier In V.21 Rx Protocol",
	"V.21 Rx Idle (End of Transmition)",
	"CONNECT: V.21 Receive 300 bps",
};

char *V21TX_MESG[6] = {
	"V.21 Transmit Data Mode",
	"V.21 Transmit Idle Mode",
	"ERROR: V.21 Tx Internal Error",
	"ERROR: Transmit Input Queue Under-run",
	"ERROR: Transmit Input Queue Over-run",
	"CONNECT: V.21 Transmit 300 bps",
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

MESSAGE_FN(v21tx_message, V21TX_MESG, 6)
MESSAGE_FN(v21rx_message, V21RX_MESG, 7)

void
v21tx_create(struct faxvmi_link *dp, const struct v21tx_cfg *cfg)
{
	struct v21tx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V21TX_CFG;

	handle = V21TX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 6;
	dp->pack_width = 1;
	dp->unpack_width = 0;
}

void
v21rx_create(struct faxvmi_link *dp, const struct v21rx_cfg *cfg)
{
	struct v21rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V21RX_CFG;

	handle = V21RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;
	dp->unpack_width = 1;
	dp->pack_width = 0;
}

void
v21tx_delete(struct faxvmi_link *dp)
{
	V21TX_delete((void *)(long)dp->int_0014);
}

void
v21rx_delete(struct faxvmi_link *dp)
{
	V21RX_delete((void *)(long)dp->int_0014);
}

void
v21tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		     unsigned short *result)
{
	V21TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

void
v21rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		     unsigned short *count)
{
	V21RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf,
		    (short *)count);
	*result = *count;
	*count = 0;
}

int
v21tx_status(struct faxvmi_link *dp, struct v21_status *status)
{
	return V21TX_status((void *)(long)dp->int_0014, status);
}

int
v21rx_status(struct faxvmi_link *dp, struct v21_status *status)
{
	return V21RX_status((void *)(long)dp->int_0014, status);
}

int
v21tx_control(struct faxvmi_link *dp, const struct v21tx_ctl *arg)
{
	return V21TX_control((void *)(long)dp->int_0014, arg);
}

int
v21rx_control(struct faxvmi_link *dp, const struct v21rx_ctl *arg)
{
	return V21RX_control((void *)(long)dp->int_0014, arg);
}
