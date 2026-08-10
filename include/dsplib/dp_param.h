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

/*
 * ...and the other end of it.  THE HOST CALLS THESE TWO, NOT THE LIBRARY:
 * `slmodemd/modem.c:1136` does `m->dp_runtime = dp_runtime_create(m)` when it
 * starts a call and `:1197` frees it when it stops, so neither appears in the
 * closure of any datapump `create`.  What they are is the DEFINITION of what
 * MDMPRM_DPRUNTIME answers with, which is the block every V.PCM constructor
 * is configured from -- so a test that constructs V.PCM builds one of these
 * rather than inventing a buffer.
 */
void *dp_runtime_create(void *modem);
void dp_runtime_delete(void *runtime);

#endif /* DSPLIB_DP_PARAM_H */
