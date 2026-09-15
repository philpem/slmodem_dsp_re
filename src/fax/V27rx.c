/* V.27 RX lowercase adapter wrappers, in reference address order. */
#include <stddef.h>
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v27cfg.h"
#include "dsplib/v27fax.h"

void
v27rx_create(struct faxvmi_link *dp, const struct v27rx_cfg *cfg)
{
	struct v27rx_cfg local;
	void *handle;

	local = (cfg != NULL) ? *cfg : V27RX_CFG;

	handle = V27RX_create((void *)(long)dp->int_0014, &local);
	dp->int_0014 = (int)(long)handle;
	dp->pack_count = 0;
	dp->unpack_width = (local.bit_rate == 2400) ? 2 : 3;
	dp->pack_width = 0;
}

void
v27rx_delete(struct faxvmi_link *dp)
{
	V27RX_delete((void *)(long)dp->int_0014);
}

void
v27rx_process(struct faxvmi_link *dp, short *in, unsigned short *result,
		     unsigned short *count)
{
	V27RX_modem((void *)(long)dp->int_0014, in, (short *)dp->buf, count);
	*result = *count;
	*count = 0;
}

int
v27rx_status(struct faxvmi_link *dp, void *status)
{
	return V27RX_status((void *)(long)dp->int_0014, status);
}

int
v27rx_control(struct faxvmi_link *dp, void *req)
{
	return V27RX_control((void *)(long)dp->int_0014, req);
}
