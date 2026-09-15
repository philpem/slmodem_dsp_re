/* V.29 RX lowercase adapter wrappers, in reference address order. */
#include <stddef.h>
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v29data.h"
#include "dsplib/v29fax.h"

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
v29rx_delete(struct faxvmi_link *dp)
{
	V29RX_delete((void *)(long)dp->int_0014);
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
v29rx_status(struct faxvmi_link *dp, void *status)
{
	return V29RX_status((void *)(long)dp->int_0014, status);
}

int
v29rx_control(struct faxvmi_link *dp, const struct v29rx_control_req *arg)
{
	return V29RX_control((void *)(long)dp->int_0014, arg);
}
