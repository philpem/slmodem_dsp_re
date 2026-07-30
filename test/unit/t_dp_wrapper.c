/*
 * t_dp_wrapper.c -- differential test of the host/datapump adaptation layer.
 *
 * This is the first module whose behaviour depends on a caller-supplied
 * function pointer, so the test installs a synthetic datapump (fakedp) on both
 * sides.  Comparing only the audio would be too weak: the wrapper could call
 * the datapump with the wrong fragment size, or twice, or with the halves of
 * its ring swapped, and still produce a stream that looked plausible.  So the
 * fake logs every call and the two logs are compared as well.
 *
 * Covered: the equal-rate pass-through, both directions of the 8000<->9600
 * bridge that dp_wrapper exists for, the 48 kHz pairs, ragged chunk sizes that
 * straddle fragment boundaries, and status propagation from the datapump.
 */

#include <string.h>

#include "harness.h"
#include "fakedp.h"
#include "dsplib/dp.h"
#include "dsplib/dp_wrapper.h"

extern void *ref_dp_wrapper_create(void *dp_data, dp_process_fn process,
				   int dp_frag, int host_srate, int dp_srate);
extern void ref_dp_wrapper_delete(void *w);
extern int ref_dp_wrapper_run(struct dp *dp, void *in, void *out, int count);

#define NSAMP 2400

static short input[NSAMP];

static void
make_input(void)
{
	unsigned lfsr = 0x2468u;
	int i;

	for (i = 0; i < NSAMP; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		input[i] = (short)((int)(lfsr & 0x3fff) - 0x2000
				   + 3000 * ((i % 8) < 4 ? 1 : -1));
	}
}

/*
 * `dp_data` is the fake datapump's state; the wrapper stores it at its own
 * offset 0 and the fake reaches it back through dp->dp_data, exactly as a
 * real datapump would.
 */
static int
run_case(const char *label, int dp_frag, int host_srate, int dp_srate,
	 const int *chunks, int nchunks, int status_on_call)
{
	struct fake_dp fa, fb;
	struct dp dpa, dpb;
	void *wa;
	struct dp_wrapper *wb;
	static short oa[NSAMP], ob[NSAMP];
	int pos = 0, ci = 0, rc;

	fake_dp_init(&fa, status_on_call, DPSTAT_CONNECT);
	fake_dp_init(&fb, status_on_call, DPSTAT_CONNECT);

	wa = ref_dp_wrapper_create(&fa, fake_dp_process, dp_frag, host_srate,
				   dp_srate);
	wb = dp_wrapper_create(&fb, fake_dp_process, dp_frag, host_srate,
			       dp_srate);

	diff_begin(label);
	if (wa == NULL || wb == NULL) {
		diff_eq_int("both refuse to build (%ld)", wb != NULL,
			    wa != NULL, 0);
		if (wa)
			ref_dp_wrapper_delete(wa);
		if (wb)
			dp_wrapper_delete(wb);
		return diff_end();
	}

	memset(&dpa, 0, sizeof(dpa));
	memset(&dpb, 0, sizeof(dpb));
	dpa.dp_data = wa;
	dpb.dp_data = wb;

	while (pos < NSAMP) {
		int n = chunks[ci++ % nchunks];
		int sa, sb, k;

		if (pos + n > NSAMP)
			n = NSAMP - pos;

		sa = ref_dp_wrapper_run(&dpa, input + pos, oa + pos, n);
		sb = dp_wrapper_run(&dpb, input + pos, ob + pos, n);

		diff_eq_int("status at %ld", sb, sa, pos);
		for (k = 0; k < n; k++)
			diff_eq_int("output sample %ld", ob[pos + k],
				    oa[pos + k], pos + k);
		pos += n;
	}

	/* The audio matching is not enough; the datapump must be driven alike. */
	fake_dp_compare(&fb, &fa);

	rc = diff_end();
	ref_dp_wrapper_delete(wa);
	dp_wrapper_delete(wb);
	return rc;
}

int
main(void)
{
	static const int bulk[] = { 48 };
	static const int ragged[] = { 7, 1, 96, 3, 48, 11, 2, 160 };
	static const int tiny[] = { 1 };
	int rc = 0;

	make_input();

	/* Equal rates: the wrapper should build no resampler at all. */
	rc |= run_case("dpw 9600=9600 bulk", 48, 9600, 9600, bulk, 1, 0);
	rc |= run_case("dpw 8000=8000 bulk", 40, 8000, 8000, bulk, 1, 0);

	/* The pair this module exists for. */
	rc |= run_case("dpw 9600->8000 f160", 160, 9600, 8000, bulk, 1, 0);
	rc |= run_case("dpw 9600->8000 f40", 40, 9600, 8000, bulk, 1, 0);
	rc |= run_case("dpw 8000->9600 f160", 160, 8000, 9600, bulk, 1, 0);

	/* The 48 kHz pairs the rate table also covers. */
	rc |= run_case("dpw 9600->48000", 100, 9600, 48000, bulk, 1, 0);
	rc |= run_case("dpw 48000->9600", 20, 48000, 9600, bulk, 1, 0);

	/* Chunk sizes that straddle fragment and ring-wrap boundaries. */
	rc |= run_case("dpw 9600->8000 ragged", 160, 9600, 8000, ragged,
		       sizeof(ragged) / sizeof(ragged[0]), 0);
	rc |= run_case("dpw 9600->8000 one-at-a-time", 40, 9600, 8000, tiny, 1, 0);
	rc |= run_case("dpw equal-rate ragged", 48, 9600, 9600, ragged,
		       sizeof(ragged) / sizeof(ragged[0]), 0);

	/* Status latching from the datapump. */
	rc |= run_case("dpw status on call 3", 160, 9600, 8000, bulk, 1, 3);

	/* Rejected geometry: zero and over-large fragments, zero rates. */
	rc |= run_case("dpw reject frag 0", 0, 9600, 8000, bulk, 1, 0);
	rc |= run_case("dpw reject frag 193", DPW_MAX_FRAG + 1, 9600, 8000,
		       bulk, 1, 0);
	rc |= run_case("dpw accept frag 192", DPW_MAX_FRAG, 9600, 9600,
		       bulk, 1, 0);

	return rc;
}
