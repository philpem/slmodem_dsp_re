/*
 * fakedp.c -- a synthetic datapump for testing the wrapper layers.  See
 * fakedp.h for what it is for and what it is designed to catch.
 */

#include <string.h>

#include "harness.h"
#include "fakedp.h"
#include "dsplib/dp.h"

/*
 * Find our instance from the `struct dp` the wrapper hands us.
 *
 * This is the same route a real datapump takes: the wrapper state lives in
 * dp->dp_data, and its first word is whatever was passed as `dp_data` to
 * dp_wrapper_create -- which for b103 is the Bell 103 state, and for us is the
 * fake_dp.  Going through that path rather than a side channel means the test
 * also proves the wrapper stored and preserved the pointer correctly.
 */
static struct fake_dp *
instance(void *dpv)
{
	struct dp *dp = (struct dp *)dpv;
	void **wrapper = (void **)dp->dp_data;

	return (struct fake_dp *)*wrapper;
}

void
fake_dp_init(struct fake_dp *d, int status_on_call, int status_value)
{
	memset(d, 0, sizeof(*d));
	d->lfsr = 0xBEEFu;
	d->status_on_call = status_on_call;
	d->status_value = status_value;
}

int
fake_dp_process(void *dpv, void *in, void *out, int count)
{
	struct fake_dp *d = instance(dpv);
	const short *ins = (const short *)in;
	short *outs = (short *)out;
	int i;

	if (d->calls < FAKE_DP_MAX_CALLS) {
		struct fake_dp_call *c = &d->log[d->calls];

		c->count = count;
		c->first_in = count > 0 ? ins[0] : 0;
		c->last_in = count > 0 ? ins[count - 1] : 0;
	}

	for (i = 0; i < count; i++) {
		/*
		 * Mix each input into a running LFSR before using it, so the
		 * output depends on the entire history rather than just the
		 * current sample.  A wrapper that reordered, repeated or lost
		 * a fragment then produces visibly different audio instead of
		 * something that happens to match.
		 */
		d->lfsr = (d->lfsr >> 1)
			  ^ (-(int)(d->lfsr & 1u) & 0xB400u);
		d->lfsr ^= (unsigned short)ins[i];
		outs[i] = (short)(ins[i] / 2 + (short)(d->lfsr & 0x0fff) - 0x800);
	}

	if (d->calls < FAKE_DP_MAX_CALLS)
		d->log[d->calls].first_out = count > 0 ? outs[0] : 0;

	d->calls++;
	d->samples += count;

	if (d->status_on_call != 0 && d->calls == d->status_on_call)
		return d->status_value;
	return 0;
}

void
fake_dp_compare(const struct fake_dp *ours, const struct fake_dp *ref)
{
	int n, i;

	diff_eq_int("datapump call count (%ld)", ours->calls, ref->calls, 0);
	diff_eq_int("datapump samples seen (%ld)", ours->samples, ref->samples, 0);

	n = ours->calls < ref->calls ? ours->calls : ref->calls;
	if (n > FAKE_DP_MAX_CALLS)
		n = FAKE_DP_MAX_CALLS;

	for (i = 0; i < n; i++) {
		diff_eq_int("call %ld: fragment size", ours->log[i].count,
			    ref->log[i].count, i);
		diff_eq_int("call %ld: first input", ours->log[i].first_in,
			    ref->log[i].first_in, i);
		diff_eq_int("call %ld: last input", ours->log[i].last_in,
			    ref->log[i].last_in, i);
		diff_eq_int("call %ld: first output", ours->log[i].first_out,
			    ref->log[i].first_out, i);
	}
}
