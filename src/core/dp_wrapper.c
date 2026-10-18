/*
 * dp_wrapper.c -- DataPump: rate and fragment adaptation.
 *
 * Reconstructed from dsplibs.o dp_wrapper.c:
 *   dp_wrapper_create  .text 0x005a80
 *   dp_wrapper_delete  .text 0x005a20
 *   dp_wrapper_run     .text 0x005d40
 *
 * See docs/findings.md section 15 for the object layout this mirrors.
 */

#include <string.h>

#include "dsplib/dp_wrapper.h"
#include "dsplib/fixedrc.h"
#include "dsplib/sysdep.h"

/*
 * One direction's ring.  Two host fragments deep, so the datapump can be
 * filling one half while the caller drains the other; `dp_pos` and the
 * caller-side position therefore just alternate between 0 and host_frag.
 */

/* Six literal rate pairs, with their original immediate converter modes.
 * An unsupported unequal pair leaves the first converter NULL and fails
 * before reverse dispatch. Both failures share the original cleanup. */
static struct rc *
dpw_create_for(int from, int to)
{
	struct rc *result = 0;
	if (from == 8000 && to == 9600)
		result = RcFixed_Create(2);
	else if (from == 9600 && to == 8000)
		result = RcFixed_Create(3);
	else if (from == 8000 && to == 48000)
		result = RcFixed_Create(4);
	else if (from == 48000 && to == 8000)
		result = RcFixed_Create(5);
	else if (from == 9600 && to == 48000)
		result = RcFixed_Create(6);
	else if (from == 48000 && to == 9600)
		result = RcFixed_Create(7);
	return result;
}

void
dp_wrapper_delete(struct dp_wrapper *w)
{
	w->dp_data = NULL;
	if (w->rc_to_dp)
		RcFixed_Delete(w->rc_to_dp);
	if (w->rc_to_host)
		RcFixed_Delete(w->rc_to_host);
	sysdep_free(w);
}

struct dp_wrapper *
dp_wrapper_create(void *dp_data, dp_process_fn process, int dp_frag,
		  int host_srate, int dp_srate)
{
	struct dp_wrapper *w;

	if (dp_frag == 0 || host_srate == 0)
		return NULL;
	if (dp_srate == 0 || dp_frag > DPW_MAX_FRAG)
		return NULL;

	w = sysdep_malloc(sizeof(*w));
	if (w == NULL)
		return NULL;
	sysdep_memset(w, 0, sizeof(*w));

	w->dp_data = dp_data;
	w->process = process;

	/*
	 * The caller speaks in host-rate samples, so the fragment it must
	 * accumulate before the datapump can run is the datapump's fragment
	 * scaled by the rate ratio.
	 */
	w->host_frag = dp_frag * host_srate / dp_srate;

	/*
	 * The output ring starts one fragment "ahead": the datapump writes to
	 * the far half while the caller drains the near one, and out.total is
	 * pre-loaded so the very first run() can return a full fragment
	 * without waiting.  That fragment is silence, which is the one
	 * fragment of latency this layer costs.
	 *
	 * The input ring starts at zero throughout -- nothing has arrived yet.
	 */
	w->out.total = w->host_frag;
	w->out.dp_pos = w->host_frag;

	/* Equal rates need no conversion at all -- the common 8 kHz case. */
	if (host_srate != dp_srate) {
		struct rc *to_dp = 0;
		struct rc *to_host = 0;

		to_dp = dpw_create_for(host_srate, dp_srate);
		w->rc_to_dp = to_dp;
		if (to_dp == 0) {
			goto failed;
		}
		to_host = dpw_create_for(dp_srate, host_srate);
		w->rc_to_host = to_host;
		if (to_host == 0) {
			goto failed;
		}
	}

	return w;

failed:
	dp_wrapper_delete(w);
	return NULL;
}



static unsigned int
imin(unsigned int a, unsigned int b)
{
	return a < b ? a : b;
}

int
dp_wrapper_run(struct dp *dp, void *in, void *out, int count)
{
	struct dp_wrapper *w = (struct dp_wrapper *)dp->dp_data;
	const short *inp = (const short *)in;
	short *outp = (short *)out;
	int status = 0;

	while (count > 0) {
		/*
		 * Take as much as fits: no more than one fragment, and no more
		 * than the space left before either ring position wraps.
		 */
		unsigned int span = 2U * w->host_frag;
		int n = imin(count, w->host_frag);

		n = imin(n, span - w->in.wr);
		n = imin(n, span - w->out.rd);

		sysdep_memcpy(&w->in.data[w->in.wr], inp, n * sizeof(short));
		inp += n;
		w->in.total += n;
		w->in.wr = (w->in.wr + n) % (2U * w->host_frag);

		/* Run the datapump only once a whole fragment has arrived. */
		if (w->in.total >= w->host_frag) {
			short *dp_in = &w->in.data[w->in.dp_pos];
			short *dp_out = &w->out.data[w->out.dp_pos];
			int dp_n = w->host_frag;
			int st;

			if (w->rc_to_dp) {
				int got = DPW_MAX_FRAG;

				RcFixed_Resample(w->rc_to_dp, dp_in,
						 w->host_frag,
						 w->scratch_in, &got);
				dp_in = w->scratch_in;
				dp_n = got;
			}
			if (w->rc_to_host)
				dp_out = w->scratch_out;

			st = w->process(dp, dp_in, dp_out, dp_n);
			if (st != 0)
				status = st;

			if (w->rc_to_host) {
				int got = w->host_frag;

				RcFixed_Resample(w->rc_to_host, w->scratch_out,
						 dp_n,
						 &w->out.data[w->out.dp_pos],
						 &got);
			}

			/*
			 * Flip both ends to the other half of their ring.  The
			 * original writes this as "if the position is non-zero
			 * go to 0, else go to host_frag", which is the same
			 * alternation.
			 */
			w->in.total -= w->host_frag;
			w->in.dp_pos = (w->in.dp_pos != 0) ? 0 : w->host_frag;

			w->out.total += w->host_frag;
			w->out.dp_pos = (w->out.dp_pos != 0) ? 0 : w->host_frag;
		}

		sysdep_memcpy(outp, &w->out.data[w->out.rd],
			      n * sizeof(short));
		w->out.total -= n;
		outp += n;
		w->out.rd = (w->out.rd + n) % (2U * w->host_frag);

		count -= n;
	}

	return status;
}
