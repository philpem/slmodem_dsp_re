/* V.29 TX lowercase adapter wrappers, in reference address order. */
#include <stddef.h>
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v29data.h"
#include "dsplib/v29fax.h"

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
v29tx_delete(struct faxvmi_link *dp)
{
	V29TX_delete((void *)(long)dp->int_0014);
}

void
v29tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		     unsigned short *result)
{
	V29TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

int
v29tx_status(struct faxvmi_link *dp, void *status)
{
	return V29TX_status((void *)(long)dp->int_0014, status);
}

int
v29tx_control(struct faxvmi_link *dp, const struct v29tx_control_req *arg)
{
	return V29TX_control((void *)(long)dp->int_0014, arg);
}
