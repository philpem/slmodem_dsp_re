/* V.17 TX lowercase adapter wrappers, in reference address order. */
#include <stddef.h>
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v17fax.h"

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
v17tx_delete(struct faxvmi_link *dp)
{
	V17TX_delete((void *)(long)dp->int_0014);
}

void
v17tx_process(struct faxvmi_link *dp, short *out, unsigned short *count,
		     unsigned short *result)
{
	V17TX_modem((void *)(long)dp->int_0014, dp->buf, out, count);
	*result = *count;
	*count = 0;
}

int
v17tx_status(struct faxvmi_link *dp, struct v17_status *status)
{
	return V17TX_status((void *)(long)dp->int_0014, status);
}

int
v17tx_control(struct faxvmi_link *dp, const struct v17tx_control_req *arg)
{
	return V17TX_control((void *)(long)dp->int_0014, arg);
}
