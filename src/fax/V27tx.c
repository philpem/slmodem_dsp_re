/* V.27 TX lowercase adapter wrappers, in reference address order. */
#include <stddef.h>
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v27cfg.h"
#include "dsplib/v27fax.h"

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
v27tx_delete(struct faxvmi_link *dp)
{
	V27TX_delete((void *)(long)dp->int_0014);
}

void
v27tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		     unsigned short *result)
{
	V27TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

int
v27tx_status(struct faxvmi_link *dp, void *status)
{
	return V27TX_status((const void *)(long)dp->int_0014, status);
}

int
v27tx_control(struct faxvmi_link *dp, void *req)
{
	return V27TX_control((void *)(long)dp->int_0014, req);
}
