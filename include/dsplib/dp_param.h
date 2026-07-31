/*
 * dp_param.h -- DataPump: access to modem parameters.
 */

#ifndef DSPLIB_DP_PARAM_H
#define DSPLIB_DP_PARAM_H

/*
 * The whole parameter numbering now lives in one place; MDMPRM_DPRUNTIME used
 * to be defined here alone, back when it was the only index anything needed.
 */
#include "dsplib/modem_params.h"

/* Fetch the modem's dp_runtime pointer. */
void *dp_param_get(void *modem);

#endif /* DSPLIB_DP_PARAM_H */
