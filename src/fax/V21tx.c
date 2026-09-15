/* V.21 TX lowercase adapter wrappers, in reference address order. */
#include <stddef.h>
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"

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
v21tx_delete(struct faxvmi_link *dp)
{
	V21TX_delete((void *)(long)dp->int_0014);
}

void
v21tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		     unsigned short *result)
{
	V21TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

int
v21tx_status(struct faxvmi_link *dp, struct v21_status *status)
{
	return V21TX_status((void *)(long)dp->int_0014, status);
}

int
v21tx_control(struct faxvmi_link *dp, const struct v21tx_ctl *arg)
{
	return V21TX_control((void *)(long)dp->int_0014, arg);
}
