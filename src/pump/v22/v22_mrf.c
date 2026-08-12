/*
 * v22_mrf.c -- V.22/V.22bis Multi-Rate Filter: the receiver's 9:20 polyphase
 * resampler.
 *
 * Reconstructed from dsplibs.o v22_mrf.c:
 *   V22_MRF_init    .text   0x08d060  228 bytes
 *   V22_MRF_free    .text   0x08d150   16 bytes
 *   V22_MRF_filter  .text   0x08d160  599 bytes
 *   MRFv22_COFFS    .rodata 0x0089c0  540 bytes
 *   V22_MRF_CFG     .bss    0x0006a0    8 bytes
 *
 * `include/dsplib/v22_mrf.h` carries the argument for why this is its own
 * type rather than `struct fpm_mrf`, and what the in-place permutation in
 * init does.  What is worth having beside the code is the shape of the
 * history buffer, because it is the part that reads oddly:
 *
 *   - it holds 60 entries, twice a phase's worth of taps, and only the first
 *     30 are zeroed;
 *   - samples are appended at `widx` and nothing wraps.  When an append would
 *     run past 60 the upper half is copied down over the lower and `widx`
 *     drops by 30, so the newest 30 samples are always contiguous;
 *   - the convolution window is `history[widx - 30 .. widx - 1]`, oldest
 *     first, against 30 contiguous coefficients chosen by `phase`.
 *
 * That last window is clamped to `history[0 .. 29]` while `widx` is still
 * below 30, which is the first thirteen or so outputs of a stream.  It is not
 * the same window a wrapping buffer would give -- the new samples land at the
 * FRONT of the window rather than the back -- but the tail it reads is the
 * zeroed startup region either way.  Recorded at D298, and reproduced.
 */

#include "dsplib/sysdep.h"
#include "dsplib/v22_mrf.h"

void
V22_MRF_init(struct v22_mrf *state, const struct v22_mrf_cfg *cfg, int fresh)
{
	short work[V22_MRF_COEFFS];
	short *coeff;
	short i, j, k;

	state->cfg = *cfg;
	state->need = 2;
	state->phase = 0;
	state->widx = 0;
	state->history_len = V22_MRF_TAPS;

	/*
	 * Two paths, not `fpm_mrf`'s three: `fresh` allocates, and anything
	 * else adopts whatever pointer the state already holds without
	 * looking at its size.  A second `fresh` init leaks the old buffer,
	 * as it does there.
	 */
	if (fresh)
		state->history =
			(short *)sysdep_malloc(V22_MRF_HISTORY *
					       sizeof(short));

	for (i = 0; i < state->history_len; i++)
		state->history[i] = 0;

	/*
	 * Rewrite the coefficients from natural impulse-response order into
	 * nine contiguous phases of thirty taps, each phase reversed in time:
	 *
	 *      after[p * 30 + t] = before[9 * (29 - t) + p]
	 *
	 * The outer index walks the last nine entries and each inner walk
	 * steps back by the interpolation factor, which enumerates one phase
	 * newest-first.  Done in place, through the caller's array.
	 */
	coeff = state->cfg.coeff;
	k = 0;
	for (j = V22_MRF_COEFFS - V22_MRF_PHASES; j < V22_MRF_COEFFS; j++)
		for (i = j; i >= 0; i -= V22_MRF_PHASES)
			work[k++] = coeff[i];
	for (i = 0; i < V22_MRF_COEFFS; i++)
		coeff[i] = work[i];
}

void
V22_MRF_free(struct v22_mrf *state)
{
	sysdep_free(state->history);
}

short
V22_MRF_filter(struct v22_mrf *state, const short *in, short *out, short count)
{
	short *coeff = state->cfg.coeff;
	short need = state->need;
	short phase = state->phase;
	short widx = state->widx;
	short hlen = state->history_len;
	short *history = state->history;
	short produced = 0;
	short remaining = count;

	while (remaining != 0) {
		int acc;
		short k;
		short r;

		/*
		 * Not enough input left to complete an output.  Take what
		 * there is, carry the shortfall in `need`, and return -- this
		 * is what lets a stream be fed in arbitrary fragments.
		 */
		if (remaining < need) {
			need -= remaining;
			if (widx + remaining > V22_MRF_HISTORY) {
				sysdep_memcpy(history,
					      history + V22_MRF_TAPS,
					      V22_MRF_TAPS * sizeof(short));
				widx -= V22_MRF_TAPS;
			}
			sysdep_memcpy(history + widx, in,
				      (unsigned)remaining * sizeof(short));
			widx += remaining;
			break;
		}

		if (widx + need > V22_MRF_HISTORY) {
			sysdep_memcpy(history, history + V22_MRF_TAPS,
				      V22_MRF_TAPS * sizeof(short));
			widx -= V22_MRF_TAPS;
		}
		sysdep_memcpy(history + widx, in,
			      (unsigned)need * sizeof(short));
		in += need;
		widx += need;
		remaining -= need;

		acc = 0;
		if (widx < hlen) {
			/* Startup: the window has not filled yet. */
			for (k = 0; k < V22_MRF_TAPS; k++)
				acc += history[k] * coeff[phase * hlen + k];
		} else {
			for (k = 0; k < V22_MRF_TAPS; k++)
				acc += history[widx - hlen + k] *
				       coeff[phase * hlen + k];
		}
		r = (short)(acc >> 15);

		/*
		 * Advance the phase by the decimation factor; each wrap past
		 * the interpolation factor costs one more input sample.
		 */
		phase += V22_MRF_DECIMATE;
		need = 0;
		while (phase >= V22_MRF_PHASES) {
			phase -= V22_MRF_PHASES;
			need++;
		}

		out[produced++] = r;
	}

	state->need = need;
	state->phase = phase;
	state->widx = widx;
	return produced;
}

/*
 * The configuration template.  All-zero and not `const`, so it lands in
 * `.bss`; nothing in the object ever assigns to it.  See the header.
 */
struct v22_mrf_cfg V22_MRF_CFG = { 0, 0 };

/*
 * The 270-tap prototype, in natural impulse-response order: symmetric about
 * the pair at [134] and [135], and the array init permutes.  `V22FP_create`
 * never passes this array itself -- it multiplies it by a generated tone into
 * a heap buffer first -- which is why it can live in `.rodata` at all.
 */
const short MRFv22_COFFS[V22_MRF_COEFFS] = {
	231, 233, 234, 234, 232, 229, 225, 219,
	212, 202, 191, 179, 164, 148, 129, 109,
	87, 63, 37, 9, -21, -53, -87, -122,
	-159, -198, -238, -280, -324, -368, -414, -460,
	-508, -556, -605, -654, -704, -753, -802, -850,
	-898, -945, -991, -1036, -1079, -1120, -1158, -1195,
	-1229, -1259, -1287, -1311, -1332, -1348, -1360, -1368,
	-1371, -1369, -1361, -1349, -1330, -1306, -1275, -1238,
	-1195, -1145, -1088, -1024, -953, -875, -790, -697,
	-597, -490, -375, -253, -123, 14, 158, 310,
	468, 633, 806, 984, 1170, 1361, 1558, 1761,
	1969, 2183, 2401, 2624, 2850, 3081, 3314, 3551,
	3790, 4032, 4275, 4519, 4763, 5009, 5253, 5498,
	5741, 5982, 6221, 6458, 6691, 6920, 7146, 7367,
	7583, 7793, 7997, 8195, 8385, 8569, 8745, 8912,
	9071, 9221, 9362, 9494, 9615, 9726, 9827, 9918,
	9997, 10066, 10123, 10169, 10204, 10227, 10239, 10239,
	10227, 10204, 10169, 10123, 10066, 9997, 9918, 9827,
	9726, 9615, 9494, 9362, 9221, 9071, 8912, 8745,
	8569, 8385, 8195, 7997, 7793, 7583, 7367, 7146,
	6920, 6691, 6458, 6221, 5982, 5741, 5498, 5253,
	5009, 4763, 4519, 4275, 4032, 3790, 3551, 3314,
	3081, 2850, 2624, 2401, 2183, 1969, 1761, 1558,
	1361, 1170, 984, 806, 633, 468, 310, 158,
	14, -123, -253, -375, -490, -597, -697, -790,
	-875, -953, -1024, -1088, -1145, -1195, -1238, -1275,
	-1306, -1330, -1349, -1361, -1369, -1371, -1368, -1360,
	-1348, -1332, -1311, -1287, -1259, -1229, -1195, -1158,
	-1120, -1079, -1036, -991, -945, -898, -850, -802,
	-753, -704, -654, -605, -556, -508, -460, -414,
	-368, -324, -280, -238, -198, -159, -122, -87,
	-53, -21, 9, 37, 63, 87, 109, 129,
	148, 164, 179, 191, 202, 212, 219, 225,
	229, 232, 234, 234, 233, 231,
};
