/* V.17 RX lowercase adapter wrappers, in reference address order. */
#include <stddef.h>
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v17fax.h"

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
v17rx_delete(struct faxvmi_link *dp)
{
	V17RX_delete((void *)(long)dp->int_0014);
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
v17rx_status(struct faxvmi_link *dp, struct v17_status *status)
{
	return V17RX_status((void *)(long)dp->int_0014, status);
}

int
v17rx_control(struct faxvmi_link *dp, const struct v17rx_ctl *arg)
{
	return V17RX_control((void *)(long)dp->int_0014, arg);
}
