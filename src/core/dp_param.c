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
#include "dsplib/sysdep.h"

/* Provided by the modem core (slmodemd/modem_param.c). */

void *
dp_param_get(void *modem)
{
	return (void *)modem_get_param(modem, MDMPRM_DPRUNTIME);
}

/*
 * dp_runtime_create -- build the block MDMPRM_DPRUNTIME then answers with.
 *
 * The host owns this: slmodemd calls it once per call and hands the result
 * back to whichever datapump asks for MDMPRM_DPRUNTIME.  It is therefore the
 * whole of "configured" in "constructed and configured" for V.PCM, and it is
 * a 0x88-byte `struct _tagModemParameters` -- see that header for how the
 * type is pinned.
 *
 * Two things here look wrong and are not:
 *
 *   - THE BLOCK IS MEMSET TWICE, once before `modem_get_param` and once
 *     after, both `sysdep_memset(runtime, 0, 0x88)` (0x5919 and 0x593e).  The
 *     second makes the first dead.  It is in the object, so it is here.
 *   - EVERY FIELD SET TO ZERO BELOW IS ALSO ALREADY ZERO from that memset,
 *     and the object stores the zero anyway (0x5983, 0x599f, 0x59ca-0x59ed).
 *     Deleting them would be tidier and would stop matching.
 *
 * The four `dsp_info` reads are the only thing the host tells it that is not
 * a parameter index, and two of them come back out again in `vpcm_delete`.
 */
void *
dp_runtime_create(void *modem)
{
	struct _tagModemParameters *rt;
	struct dsp_info *info;

	rt = (struct _tagModemParameters *)sysdep_malloc(sizeof(*rt));
	if (!rt)
		return 0;

	sysdep_memset(rt, 0, sizeof(*rt));
	info = (struct dsp_info *)modem_get_param(modem, MDMPRM_DSPINFO);
	sysdep_memset(rt, 0, sizeof(*rt));

	rt->qcFlags |= 0x10;
	rt->qcFlags &= (unsigned char)~0x20;
	rt->qcFlags = (unsigned char)((rt->qcFlags & ~0x40)
				      | ((info->qc_lapm & 1) << 6));
	rt->unnamed_0003 &= (unsigned char)~0x07;
	rt->qcIndex = info->qc_index ? (int)info->qc_index : 9;
	rt->qcFlags &= (unsigned char)~0x80;

	rt->unnamed_000c = 0;
	rt->unnamed_0004 = 60;
	rt->unnamed_0008 = 40;
	rt->unnamed_0014 = 700;
	rt->powerReductionTenths = 0;
	rt->unnamed_0044 = 6;
	rt->clockDeviation = info->clock_deviation;
	rt->modeFlags = 1;
	rt->connectionType = (int)info->connection_type;
	rt->codecType = (int)modem_get_param(modem, MDMPRM_CODECTYPE);
	rt->unnamed_0058 = 0;
	rt->unnamed_005c = 0;
	rt->unnamed_0060 = 0;
	rt->hwDelay = 0;
	rt->dmaDelay = 0;
	rt->unnamed_006c = 0;

	return rt;
}

/* 0x5a10: three bytes, `jmp sysdep_free`. */
void
dp_runtime_delete(void *runtime)
{
	sysdep_free(runtime);
}
