/* V.21 RX lowercase adapter wrappers, in reference address order. */
#include <stddef.h>
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"

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
v21rx_delete(struct faxvmi_link *dp)
{
	V21RX_delete((void *)(long)dp->int_0014);
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
v21rx_status(struct faxvmi_link *dp, struct v21_status *status)
{
	return V21RX_status((void *)(long)dp->int_0014, status);
}

int
v21rx_control(struct faxvmi_link *dp, const struct v21rx_ctl *arg)
{
	return V21RX_control((void *)(long)dp->int_0014, arg);
}
