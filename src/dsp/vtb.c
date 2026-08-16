/*
 * vtb.c -- the eight-state Viterbi trellis decoder.
 *
 * Reconstructed from dsplibs.o:
 *   VTB_decoder   .text   0xab4d0  1773
 *   VTB_DIFF_TBL  .rodata 0x0ed60    32   (file-local in the object)
 *
 * PROTOCOL-INDEPENDENT.  Everything specific to a modulation reaches this
 * through the state: the constellation, the region and boundary tables, the
 * grid size, the mask and the shift.  V.32bis installs its set in
 * `VTBv32_init` (src/pump/v32/v32vtb.c) and the fax V.17 receiver installs
 * `VTBv17_*` the same way, which is why this is not in src/pump/v32/.
 *
 * ONE SYMBOL IN, ONE SYMBOL OUT, SIXTEEN APART.  The survivor ring is 16
 * slots of 8 states.  Each call advances the slot, saves the eight symbols
 * that slot still holds -- they are 16 symbols old -- overwrites the slot
 * with this symbol's decisions, traces back 16 steps from the state of least
 * metric, and emits the saved symbol of the state it lands on.
 *
 * THE FOUR-CANDIDATE TRICK.  A Viterbi decoder over a 128-point
 * constellation does not search 128 points per branch.  The received point
 * is quantised onto a coarse grid, `region[]` turns that cell into an offset
 * into `bound[]`, and `bound[]` names the nearest point of each trellis
 * subset for that cell.  Four squared distances is then the whole branch
 * metric computation, and it is done twice: once for the four subsets the
 * even states use and once for the four the odd states use.
 *
 * TWO FRAMES, AND THE READING THAT LOOKS THE SAME.  For `nsub` 1 and 3 --
 * the 16- and 64-point constellations, which are the 45-degree-rotated ones
 * -- the region lookup runs on the ROTATED point while the branch metric
 * runs on the point as received.  Rotating both, or neither, agrees with
 * this over any input near a constellation point and disagrees between them,
 * which is why `t_v32vtb` counts the trials that separate the readings
 * rather than only counting passes.
 *
 * THE METRIC LEAKS.  Every state metric is scaled by 0x799a/0x8000 (0.95)
 * before the add, which is what stops the accumulators saturating; the
 * branch metric is scaled by 0x666/0x1000 (0.4) after a 16-bit right shift.
 * Both are exact as written -- the intermediate is a 32-bit product and only
 * the final store is narrowed.
 */

#include "dsplib/vtb.h"

/*
 * The differential decoder, indexed `(quadrant << 2) + prev_quadrant`... no:
 * indexed `(this quadrant) + 4 * (previous quadrant)`, which is how the
 * object computes it -- `(sym >> shift) + prev * 4`.  Sixteen entries for
 * the sixteen ordered pairs, and the value is the two differential bits.
 *
 * `r` and not `R` in the object's symbol table, so file-local; it is
 * therefore `static` here and is tested through the decoder rather than
 * against `ref_VTB_DIFF_TBL` directly.
 */
static const short VTB_DIFF_TBL[16] = {
	0, 1, 2, 3, 1, 0, 3, 2,
	3, 2, 0, 1, 2, 3, 1, 0,
};

/*
 * One state's add-compare-select.  The four incoming branches come from
 * states `p0`..`p0 + 3`, and the branch from `p0 + k` carries subset
 * `k ^ x`.  The winner is the smallest sum of leaked state metric and
 * branch metric, ties going to the lower source state.
 */
static void
vtb_acs(struct vtb *state, struct vtb_path *node, int s, int p0, int x,
	const short *old, const short *bm, const short *pt)
{
	int best = p0;
	int m = (short)(old[p0] + bm[x]);
	int k, d;

	for (k = 1; k <= 3; k++) {
		d = (short)(old[p0 + k] + bm[k ^ x]);
		if (d < m) {
			best = p0 + k;
			m = d;
		}
	}

	state->metric[s] = (short)m;
	node[s].surv = (short)best;
	node[s].sym = pt[(best - p0) ^ x];
}

/*
 * The four branch metrics for one group of four boundary points, and the
 * four points themselves so the winner can be turned back into a symbol.
 */
static void
vtb_branch(struct vtb *state, const short *bp, short i, short q,
	   short *pt, short *bm)
{
	int k, p, di, dq, e;

	for (k = 0; (short)k <= 3; k++) {
		p = bp[k];
		pt[k] = (short)p;

		di = (short)(i - state->imap[p]);
		dq = (short)(q - state->qmap[p]);
		e = (di * di + dq * dq) >> 16;
		bm[k] = (short)((e * 0x666) >> 12);
	}
}

void
VTB_decoder(struct vtb *state, short i, short q, short *out)
{
	struct vtb_path *node;
	short old[8];			/* the leaked state metrics       */
	short sym16[8];			/* what the slot held: 16 old     */
	short pt[4], bm[4];		/* this group's points and metrics*/
	int slot, ri, rq, ci, cq, lim, grid, idx, base;
	int j, best, m, d, sym, sh;

	slot = (state->ring + 1) & 0xf;
	state->ring = (unsigned short)slot;
	slot *= 8;
	node = state->paths + slot;
	grid = state->grid;

	for (j = 0; (short)j <= 7; j++) {
		old[j] = (short)((state->metric[j] * 0x799a) >> 15);
		sym16[j] = node[j].sym;
	}

	/*
	 * The rotation.  `nsub` 1 scales by four and does not narrow; `nsub`
	 * 3 narrows the sum to 16 bits and scales by two.  Both are the same
	 * 45-degree rotation at the scale that puts the constellation back
	 * on the region grid.
	 */
	ri = i;
	rq = q;
	if (state->nsub == 1) {
		ri = (i + q) >> 2;
		rq = (q - i) >> 2;
	} else if (state->nsub == 3) {
		ri = (short)(i + q) >> 1;
		rq = (short)(q - i) >> 1;
	}

	/* The coarse grid cell, one axis at a time, saturating at the edge. */
	ci = ((ri < 0 ? -ri : ri) + 0x400) >> 11;
	cq = ((rq < 0 ? -rq : rq) + 0x400) >> 11;
	lim = grid - 1;
	if (ci > lim)
		ci = lim;
	if (cq > lim)
		cq = lim;

	/*
	 * The two triangles of the cell.  The table is indexed as if it were
	 * `grid` by `grid` twice over, the second copy taken when the
	 * quadrature coordinate is the larger -- which is the reflection
	 * about the diagonal that the constellation's own symmetry allows.
	 */
	idx = (short)(cq + (cq > ci ? grid : 0));
	idx = (short)(ci + grid * idx);
	base = state->region[idx];

	/* And then the quadrant, which the region table does not carry. */
	if ((short)ri > 0)
		base += ((short)rq > 0) ? 0x00 : 0x18;
	else
		base += ((short)rq > 0) ? 0x08 : 0x10;
	base = (short)base;

	/* Even states: the first group of four subsets. */
	vtb_branch(state, state->bound + base, i, q, pt, bm);
	vtb_acs(state, node, 0, 0, 0, old, bm, pt);
	vtb_acs(state, node, 2, 0, 1, old, bm, pt);
	vtb_acs(state, node, 4, 0, 2, old, bm, pt);
	vtb_acs(state, node, 6, 0, 3, old, bm, pt);

	/* Odd states: the second group, four entries further on. */
	vtb_branch(state, state->bound + (short)(base + 4), i, q, pt, bm);
	vtb_acs(state, node, 1, 4, 0, old, bm, pt);
	vtb_acs(state, node, 3, 4, 1, old, bm, pt);
	vtb_acs(state, node, 5, 4, 3, old, bm, pt);
	vtb_acs(state, node, 7, 4, 2, old, bm, pt);

	/* The state to trace back from: least metric, ties to the lowest. */
	best = 0;
	m = state->metric[0];
	for (j = 1; (short)j <= 7; j++) {
		d = (short)state->metric[j];
		if (d < m) {
			best = j;
			m = d;
		}
	}

	/*
	 * Sixteen steps back through the ring, one slot of eight nodes at a
	 * time, wrapping at 0x80 -- the ring is 16 slots and the mask is on
	 * the node index, not the slot index.
	 */
	for (j = 0; (short)j < state->depth; j++) {
		best = state->paths[slot + best].surv;
		slot = (slot - 8) & 0x78;
	}

	/*
	 * The symbol that fell out, differentially decoded.  The two
	 * quadrant bits are above `shift`; everything below it passes
	 * through unchanged.
	 */
	sym = (short)(sym16[best] & state->mask);
	sh = state->shift;
	d = VTB_DIFF_TBL[(short)((sym >> sh) + state->prev * 4)];
	*out = (short)((d << sh) + (sym & ((1 << sh) - 1)));
	state->prev = (short)(sym >> state->shift);
}
