/*
 * dp_wrapper.h -- rate and fragment adaptation between the host and a
 * datapump.
 *
 * The classic datapumps run natively at 8 kHz while the host runs at 9600.
 * This layer hides that: it buffers the host's sample stream into whole
 * fragments, rate-converts in both directions if needed, and calls the
 * datapump with the fragment size it expects.
 *
 * It is why B103, V.22, V.23 and V.32 contain no sample-rate logic at all.
 */

#ifndef DSPLIB_DP_WRAPPER_H
#define DSPLIB_DP_WRAPPER_H

#include "dsplib/dp.h"

/*
 * Ring geometry.  Each direction gets one ring holding two host fragments,
 * so the datapump can be filling one while the caller drains the other.
 *
 * The maximum fragment follows from the ring size rather than standing on its
 * own -- the original's create() rejects anything above 192, which is exactly
 * half of the 768-byte ring.  Deriving it keeps the relationship visible.
 */
#define DPW_RING_BYTES   768
#define DPW_RING_SAMPLES (DPW_RING_BYTES / (int)sizeof(short))	/* 384 */
#define DPW_MAX_FRAG     (DPW_RING_SAMPLES / 2)			/* 192 */

struct dp_wrapper;

/*
 * `dp_frag` is the fragment size the datapump wants, in samples at
 * `dp_srate`.  `host_srate` is the rate the caller will supply.
 *
 * Returns NULL if dp_frag is zero or above DPW_MAX_FRAG, if either rate is
 * zero, or if a needed rate conversion could not be built.
 */
struct dp_wrapper *dp_wrapper_create(void *dp_data, dp_process_fn process,
				     int dp_frag, int host_srate,
				     int dp_srate);
void dp_wrapper_delete(struct dp_wrapper *w);

/*
 * Push `count` host-rate samples through, taking the same number back.
 * Returns the last non-zero status the datapump reported, or 0.
 */
int dp_wrapper_run(struct dp *dp, void *in, void *out, int count);

#endif /* DSPLIB_DP_WRAPPER_H */
