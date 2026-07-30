/*
 * dp_wrapper.c -- rate and fragment adaptation between the host and a
 * datapump.
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

extern void *sysdep_malloc(unsigned size);
extern void sysdep_free(void *ptr);
extern void *sysdep_memcpy(void *dst, const void *src, unsigned n);
extern void *sysdep_memset(void *dst, int c, unsigned n);

/*
 * One direction's ring.  Two host fragments deep, so the datapump can be
 * filling one half while the caller drains the other; `dp_pos` and the
 * caller-side position therefore just alternate between 0 and host_frag.
 */
struct dpw_ring {
	int total;			/* samples currently held        */
	int wr;				/* caller write position (input) */
	int rd;				/* caller read position (output) */
	int dp_pos;			/* the datapump's end            */
	short data[DPW_RING_SAMPLES];
};

struct dp_wrapper {
	void *dp_data;			/* +0x000 datapump's own state   */
	dp_process_fn process;		/* +0x004                        */
	struct dp *dp;			/* +0x008 set by the caller      */
	struct rc *rc_to_dp;		/* +0x00c host rate -> dp rate   */
	struct rc *rc_to_host;		/* +0x010 dp rate -> host rate   */
	short scratch_in[DPW_MAX_FRAG];	 /* +0x014 resampled input       */
	short scratch_out[DPW_MAX_FRAG]; /* +0x194 datapump output       */
	int host_frag;			/* +0x314 dp_frag scaled to host */
	struct dpw_ring out;		/* +0x318                        */
	struct dpw_ring in;		/* +0x628                        */
};

/*
 * Rate pairs the original supports, and the RcFixed mode for each.
 *
 * The original spells this out as a chain of literal comparisons rather than
 * calling RcFixed_Check_Combination -- which is why that function, though
 * exported, is dead code (deviation D3).
 *
 * Worth recording: these six entries are *exactly* what Check_Combination
 * returns for the same pairs. 8000->9600 reduces to 5:6 = mode 2, 9600->8000
 * to 6:5 = mode 3, 8000->48000 to 1:6 = mode 4, and so on. The wrapper simply
 * inlined the general lookup for the rates it cared about.
 *
 * The table is kept explicit rather than delegating to Check_Combination,
 * because the two are only equivalent over these pairs: Check_Combination
 * would happily build a converter for, say, 7200->8000, where the original
 * builds none.  Matching the original's silence there is the point.
 */
struct dpw_rate_pair {
	int from;
	int to;
	int mode;
};

static const struct dpw_rate_pair dpw_rate_pairs[] = {
	{  8000,  9600, 2 },		/* x6/5 */
	{  9600,  8000, 3 },		/* x5/6 */
	{  8000, 48000, 4 },		/* x6   */
	{ 48000,  8000, 5 },		/* /6   */
	{  9600, 48000, 6 },		/* x5   */
	{ 48000,  9600, 7 },		/* /5   */
};

/* Returns the RcFixed mode, or -1 if this conversion is not offered. */
static int
dpw_mode_for(int from, int to)
{
	unsigned i;

	for (i = 0; i < sizeof(dpw_rate_pairs) / sizeof(dpw_rate_pairs[0]); i++) {
		if (dpw_rate_pairs[i].from == from && dpw_rate_pairs[i].to == to)
			return dpw_rate_pairs[i].mode;
	}
	return -1;
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

	w = (struct dp_wrapper *)sysdep_malloc(sizeof(*w));
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
		int to_dp = dpw_mode_for(host_srate, dp_srate);
		int to_host = dpw_mode_for(dp_srate, host_srate);

		if (to_dp >= 0)
			w->rc_to_dp = RcFixed_Create(to_dp);
		if (to_host >= 0)
			w->rc_to_host = RcFixed_Create(to_host);

		if ((to_dp >= 0 && w->rc_to_dp == NULL)
		    || (to_host >= 0 && w->rc_to_host == NULL)) {
			dp_wrapper_delete(w);
			return NULL;
		}
	}

	return w;
}

void
dp_wrapper_delete(struct dp_wrapper *w)
{
	if (w == NULL)
		return;

	w->dp_data = NULL;
	if (w->rc_to_dp)
		RcFixed_Delete(w->rc_to_dp);
	if (w->rc_to_host)
		RcFixed_Delete(w->rc_to_host);
	sysdep_free(w);
}

static int
imin(int a, int b)
{
	return a < b ? a : b;
}

int
dp_wrapper_run(struct dp *dp, void *in, void *out, int count)
{
	struct dp_wrapper *w = (struct dp_wrapper *)dp->dp_data;
	const short *inp = (const short *)in;
	short *outp = (short *)out;
	int span = 2 * w->host_frag;
	int status = 0;

	while (count > 0) {
		/*
		 * Take as much as fits: no more than one fragment, and no more
		 * than the space left before either ring position wraps.
		 */
		int n = imin(count, w->host_frag);

		n = imin(n, span - w->in.wr);
		n = imin(n, span - w->out.rd);

		sysdep_memcpy(&w->in.data[w->in.wr], inp, n * sizeof(short));
		inp += n;
		w->in.total += n;
		w->in.wr = (w->in.wr + n) % span;

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
		w->out.rd = (w->out.rd + n) % span;

		count -= n;
	}

	return status;
}
