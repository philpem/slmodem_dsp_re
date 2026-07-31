/*
 * dp_param.c -- DataPump: access to modem parameters.
 *
 * Reconstructed from dsplibs.o dp_param.c, .text 0x0058c0.
 *
 * The whole translation unit is one accessor.  The datapumps need the modem's
 * dp_runtime pointer -- the per-connection scratch block that survives a
 * datapump change -- and this is how they fetch it, rather than each pump
 * hard-coding the parameter number.
 */

#include "dsplib/dp_param.h"
#include "dsplib/modem_params.h"

/* Provided by the modem core (slmodemd/modem_param.c). */

void *
dp_param_get(void *modem)
{
	return (void *)modem_get_param(modem, MDMPRM_DPRUNTIME);
}
