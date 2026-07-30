/*
 * fakedp.h -- a synthetic datapump for testing the wrapper layers.
 *
 * dp_wrapper calls the datapump through a function pointer supplied at
 * create time, so testing it means installing a datapump we control on both
 * sides of the differential rig.  A real datapump would drag in the whole
 * modulation; this one does the minimum needed to detect a wrapper bug.
 *
 * What it is designed to catch:
 *
 *   - the transform is order-dependent and carries state between calls, so
 *     samples delivered in the wrong sequence, duplicated, or dropped change
 *     the output rather than cancelling out
 *   - every call is logged (count, fragment size, first and last sample), so
 *     a test can assert the two sides drove their datapump *identically*, not
 *     merely that the final audio matched
 *   - it can be told to return a non-zero status on a chosen call, to check
 *     the wrapper latches and propagates it
 *
 * Each side of a differential test gets its own instance; they never share
 * state.  The instance is reached through the `struct dp` the wrapper passes
 * in, exactly as a real datapump reaches its own state.
 */

#ifndef DSPLIB_TEST_FAKEDP_H
#define DSPLIB_TEST_FAKEDP_H

#define FAKE_DP_MAX_CALLS 512

struct fake_dp_call {
	int count;		/* fragment size requested                */
	short first_in;		/* first input sample of the fragment     */
	short last_in;		/* last input sample                      */
	short first_out;	/* first output sample produced           */
};

struct fake_dp {
	unsigned lfsr;			/* running state: makes order matter */
	int calls;
	int samples;
	int status_on_call;		/* 1-based; 0 disables               */
	int status_value;
	struct fake_dp_call log[FAKE_DP_MAX_CALLS];
};

/*
 * `status_on_call` is 1-based: 3 means the third call returns
 * `status_value`.  Pass 0 to always return success.
 */
void fake_dp_init(struct fake_dp *d, int status_on_call, int status_value);

/*
 * The datapump entry point, matching what dp_wrapper stores and calls.
 * `dp` is a `struct dp *`; the instance is found through it -- see fakedp.c.
 */
int fake_dp_process(void *dp, void *in, void *out, int count);

/* Compare two call logs; reports through the diff_* harness. */
void fake_dp_compare(const struct fake_dp *ours, const struct fake_dp *ref);

#endif /* DSPLIB_TEST_FAKEDP_H */
