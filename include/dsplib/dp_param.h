/*
 * dp_param.h -- DataPump: access to modem parameters.
 */

#ifndef DSPLIB_DP_PARAM_H
#define DSPLIB_DP_PARAM_H

/*
 * Parameter index for the dp_runtime block, from slmodemd's
 * enum MODEM_PARAMETER_NAMES (modem_param.h).  Duplicated as a bare constant
 * because the reconstruction does not include slmodemd's headers, and the
 * value is fixed by the existing ABI -- modem.o is already compiled against
 * it.
 */
#define MDMPRM_DPRUNTIME 10

/* Fetch the modem's dp_runtime pointer. */
void *dp_param_get(void *modem);

#endif /* DSPLIB_DP_PARAM_H */
