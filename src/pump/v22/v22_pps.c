/*
 * v22_pps.c -- V.22/V.22bis transmit pulse shaper: a 40-phase, 3-tap
 * interpolator over a symbol ring.
 *
 * Reconstructed from dsplibs.o v22_pps.c:
 *   V22_PPS_filter  .text   0x08d3c0  722 bytes
 *   V22_PPS_init    .text   0x08d6a0  290 bytes
 *   V22_PPS_free    .text   0x08d7d0   35 bytes
 *   PPSv22_COFFS    .rodata 0x008c20  240 bytes
 *   PPSv22_CFG      .rodata 0x008d10   16 bytes
 *
 * `include/dsplib/v22_pps.h` carries the rate argument and the field
 * evidence.  What is worth having beside the code is that everything here is
 * `v22_mrf` with different constants -- a sliding history of twice
 * `history_len`, a coefficient array permuted in place by init, `need` as the
 * count of symbols owed before the next output -- run twice over, once for I
 * and once for Q, with the two results subtracted.
 *
 * The one place the two differ, and it is not a difference in the constants:
 * the startup branch here indexes the coefficients at `phase`, where the
 * steady-state branch indexes them at `phase * history_len`.  `v22_mrf` uses
 * `phase * history_len` in both.  That is the object's, it is reachable for
 * the first forty-odd outputs of a stream, and it is recorded at D299.
 */

#include "dsplib/fpm_smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v22_pps.h"

void
V22_PPS_init(struct v22_pps *state, const struct v22_pps_cfg *cfg, int fresh)
{
	short work_q[V22_PPS_COEFFS];
	short work_i[V22_PPS_COEFFS];
	short *ci, *cq;
	short i, j, k;

	state->cfg = *cfg;
	state->need = 0;
	state->phase = V22_PPS_STEP;
	state->widx = 0;
	state->history_len = V22_PPS_TAPS;

	/*
	 * Two allocations of the same twelve bytes, spelled two different
	 * ways in the object -- a folded constant for the first and
	 * `history_len * 4` for the second.  Kept as written.
	 */
	if (fresh) {
		state->hist_i = (short *)sysdep_malloc(V22_PPS_HISTORY *
						       sizeof(short));
		state->hist_q = (short *)sysdep_malloc(
			(unsigned int)(state->history_len * 4));
	}

	for (i = 0; i < state->history_len; i++) {
		state->hist_i[i] = 0;
		state->hist_q[i] = 0;
	}

	/*
	 * Both coefficient arrays, in place, from natural order into forty
	 * contiguous phases of three taps reversed in time:
	 *
	 *      after[p * 3 + t] = before[40 * (2 - t) + p]
	 */
	ci = state->cfg.coeff_i;
	cq = state->cfg.coeff_q;
	k = 0;
	for (j = V22_PPS_COEFFS - V22_PPS_PHASES; j < V22_PPS_COEFFS; j++)
		for (i = j; i >= 0; i -= V22_PPS_PHASES) {
			work_i[k] = ci[i];
			work_q[k] = cq[i];
			k++;
		}
	for (i = 0; i < V22_PPS_COEFFS; i++) {
		ci[i] = work_i[i];
		cq[i] = work_q[i];
	}
}

void
V22_PPS_free(struct v22_pps *state)
{
	/* Q first, which is the reverse of the order init allocates them. */
	sysdep_free(state->hist_q);
	sysdep_free(state->hist_i);
}

short
V22_PPS_filter(struct v22_pps *state, struct fpm_smc_syms *src, short *out,
	       unsigned short count)
{
	const short *imap = state->imap;
	const short *qmap = state->qmap;
	const short *ci = state->cfg.coeff_i;
	const short *cq = state->cfg.coeff_q;
	short *hi = state->hist_i;
	short *hq = state->hist_q;
	const short *sym = src->sym;
	short size = src->size;
	short rd = src->rd;
	short need = state->need;
	short phase = state->phase;
	short widx = state->widx;
	short hlen = state->history_len;
	unsigned short produced = 0;
	unsigned short remaining = count;

	while (remaining != 0) {
		int acci;
		int accq;
		short d;
		int k;

		/*
		 * A new symbol is taken only when the phase wrapped on the
		 * previous output, so most outputs interpolate between the
		 * three symbols already in the history.
		 */
		if (need != 0) {
			unsigned char s = (unsigned char)sym[rd];
			short vi = imap[s];
			short vq = qmap[s];

			remaining -= need;
			rd++;
			rd = (rd < size) ? rd : 0;

			widx++;
			if (widx > V22_PPS_HISTORY - 1) {
				sysdep_memcpy(hi, hi + V22_PPS_TAPS,
					      V22_PPS_TAPS * sizeof(short));
				sysdep_memcpy(hq, hq + V22_PPS_TAPS,
					      V22_PPS_TAPS * sizeof(short));
				widx -= V22_PPS_TAPS;
			}
			hi[widx] = vi;
			hq[widx] = vq;
		}

		acci = 0;
		accq = 0;
		if (widx < V22_PPS_TAPS) {
			/*
			 * Startup.  The coefficient base is `phase`, not
			 * `phase * hlen` as it is below -- the object's, and
			 * D299 has the reachability.
			 */
			for (k = 0; k < V22_PPS_TAPS; k++) {
				acci += hi[k] * ci[phase + k];
				accq += hq[k] * cq[phase + k];
			}
		} else {
			for (k = 0; k < V22_PPS_TAPS; k++) {
				acci += hi[widx - (V22_PPS_TAPS - 1) + k] *
					ci[phase * hlen + k];
				accq += hq[widx - (V22_PPS_TAPS - 1) + k] *
					cq[phase * hlen + k];
			}
		}

		/* I minus Q, truncated to 16 bits before the scaling. */
		d = (short)((acci >> 15) - (accq >> 15));
		*out++ = (short)(d << 2);
		produced++;

		phase += state->cfg.step + V22_PPS_STEP;
		need = 0;
		if (phase > V22_PPS_PHASES - 1) {
			phase -= V22_PPS_PHASES;
			need = 1;
		}
	}

	state->need = need;
	state->phase = phase;
	state->widx = widx;
	src->rd = rd;
	return produced;
}

/*
 * The template.  Sixteen bytes of zero and `const`, so `.rodata` -- where
 * `V22_MRF_CFG`, the same idea without the `const`, is `.bss`.
 */
const struct v22_pps_cfg PPSv22_CFG = { 0, 0, 0, 0, 0 };

/*
 * The 120-tap prototype, in natural order: symmetric about the pair at [59]
 * and [60], peak 2940.  Never passed to init as it stands -- `V22FP_create`
 * builds two heap copies, one per carrier phase, and passes those.
 */
const short PPSv22_COFFS[V22_PPS_COEFFS] = {
	13, -2, -21, -41, -62, -84, -107, -130,
	-154, -176, -198, -219, -238, -255, -268, -279,
	-286, -289, -287, -280, -267, -249, -224, -193,
	-155, -111, -59, 0, 65, 138, 217, 304,
	396, 495, 599, 709, 822, 940, 1061, 1184,
	1309, 1436, 1562, 1687, 1811, 1932, 2050, 2164,
	2273, 2376, 2472, 2561, 2642, 2715, 2778, 2831,
	2874, 2907, 2929, 2940, 2940, 2929, 2907, 2874,
	2831, 2778, 2715, 2642, 2561, 2472, 2376, 2273,
	2164, 2050, 1932, 1811, 1687, 1562, 1436, 1309,
	1184, 1061, 940, 822, 709, 599, 495, 396,
	304, 217, 138, 65, 0, -59, -111, -155,
	-193, -224, -249, -267, -280, -287, -289, -286,
	-279, -268, -255, -238, -219, -198, -176, -154,
	-130, -107, -84, -62, -41, -21, -2, 13,
};
