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

/**
 * @brief Fetch the modem's per-connection `dp_runtime` block.
 *
 * The single accessor any datapump uses instead of hard-coding a parameter
 * number: `return modem_get_param(modem, MDMPRM_DPRUNTIME)`.
 *
 * @param modem  The host's modem object.
 * @return The `struct _tagModemParameters *` set up by dp_runtime_create(),
 *         opaque to callers of this accessor.
 */
void *dp_param_get(void *modem);

/**
 * @brief Build the `dp_runtime` block that dp_param_get() answers with.
 *
 * Host-owned, not library-owned: `slmodemd/modem.c` calls this once per call
 * (storing the result as `m->dp_runtime`) and dp_runtime_delete() once the
 * call ends, so neither appears in the closure of any datapump `create` --
 * this is the definition of what `MDMPRM_DPRUNTIME` answers with, the block
 * every V.PCM constructor is configured from. A test that constructs V.PCM
 * builds one of these rather than inventing a buffer.
 *
 * @param modem  The host's modem object, queried for `MDMPRM_DSPINFO`,
 *               `MDMPRM_CODECTYPE` and the fields copied from `dsp_info`.
 * @return A freshly allocated, zeroed and populated
 *         `struct _tagModemParameters *`, or NULL on allocation failure.
 */
void *dp_runtime_create(void *modem);

/**
 * @brief Free a block built by dp_runtime_create().
 * @param runtime  The block to free.
 */
void dp_runtime_delete(void *runtime);

#endif /* DSPLIB_DP_PARAM_H */
