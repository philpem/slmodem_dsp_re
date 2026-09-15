/*
 * V22Dec.c -- V.22 / V.22bis: the two receive slicers.
 *
 * Reconstructed from dsplibs.o:
 *   FSEv22_decision24 .text   0x0884a0  467 bytes
 *   FSEv22_decision12 .text   0x088680  294 bytes
 *   DECv22_ANGL12     .rodata 0x0086c4    8 bytes
 *   DECv22_QMAP12     .rodata 0x0086cc    8 bytes
 *   DECv22_IMAP12     .rodata 0x0086d4    8 bytes
 *   DECv22_MAG24      .rodata 0x0086dc    6 bytes
 *   DECv22_ANGL24     .rodata 0x008700   32 bytes
 *   DECv22_QMAP24     .rodata 0x008720   32 bytes
 *   DECv22_IMAP24     .rodata 0x008740   32 bytes
 *
 * The tables live here rather than in `v22rxtab.c` because these two functions
 * are the only things in the object that name them, and because the layout
 * argument below only makes sense beside the search that depends on it.
 *
 * See dsplib/v22dec.h for what the block is.
 */

#include "dsplib/v22dec.h"
#include "dsplib/v22txtab.h"

/*
 * The four-point set.  Angles are the object's phase units, a full turn being
 * 32768: 9869, 18061, 26253 and 1678 are 108, 198, 288 and 18 degrees, the
 * four V.22 signal phases.
 */
const short DECv22_ANGL12[4] = { 9869, 18061, 26253, 1678 };
const short DECv22_QMAP12[4] = { 12288, -4096, -12288, 4096 };
const short DECv22_IMAP12[4] = { -4096, -12288, 4096, 12288 };

/*
 * The three rings of the sixteen-point set, indexed by `(|i| + |q|) >> 12`
 * less one, so 4096 -> 5792, 8192 -> 12953, 12288 -> 17378.
 */
const short DECv22_MAG24[3] = { 5792, 12953, 17378 };

/*
 * The sixteen-point set, and the layout is what makes `FSEv22_decision24`'s
 * search two entries wide instead of sixteen.  The index is
 *
 *   bit 3   set when I is positive
 *   bit 2   set when Q is negative
 *   bit 1   set when |I| is the larger of the two amplitudes
 *   bit 0   which of the two Q amplitudes
 *
 * so the first three bits are decided by three comparisons before any distance
 * is computed, and only bit 0 is searched for.
 */
const short DECv22_ANGL24[16] = {
	12288, 14706,  9869, 12288, 18061, 20480, 20480, 22898,
	 6514,  4096,  4096,  1677, 28672, 26253, 31090, 28672,
};
const short DECv22_QMAP24[16] = {
	12288,  4096, 12288,  4096, -4096, -12288, -4096, -12288,
	12288,  4096, 12288,  4096, -4096, -12288, -4096, -12288,
};
const short DECv22_IMAP24[16] = {
	-12288, -12288, -4096, -4096, -12288, -12288, -4096, -4096,
	  4096,   4096, 12288, 12288,   4096,   4096, 12288, 12288,
};

/*
 * The magnitude the 1200 slicer reports.  An immediate in the object, and
 * equal to `DECv22_MAG24[1]` because the four-point set is the middle ring of
 * the sixteen-point one -- written as the literal it is encoded as rather than
 * as a reference to the other table, since nothing in the object connects the
 * two.
 */
#define V22_DEC12_MAG	0x3299

/*
 * How many amplitude thresholds the sixteen-point search walks: one, because
 * each axis carries two levels and one comparison separates them.  The object
 * spells this as a loop over an array with a runtime bound rather than as a
 * single test, and the loop is reproduced -- it is the shape of the code, and
 * the second and third array entries it would read are never initialised.
 */
#define V22_DEC24_LEVELS	1

/*
 * How wide the distance search is once the quadrant and the I amplitude are
 * fixed: two candidates, differing in the Q amplitude.
 */
#define V22_DEC24_WINDOW	2

/*
 * The amplitude threshold, in the units the coordinates are in once shifted up
 * twelve: 8192, half way between the 4096 and 12288 levels.
 *
 * It is used TWICE, and the second use is worth knowing about: it is also the
 * initial best distance of the Q search, which makes that a real threshold
 * rather than an infinity.  A symbol further than 8192 from BOTH candidates
 * falls back on index 0, which is not in the search window at all.  Reproduced,
 * not corrected.
 */
#define V22_DEC24_THRESH	2

/*
 * The 2400 bit/s slicer: sixteen candidates, but only two of them are ever
 * measured.  Three sign and magnitude comparisons pick the pair; see the
 * layout note on `DECv22_ANGL24`.
 */
unsigned short
FSEv22_decision24(struct v22_fse *state, short *angle, short *mag)
{
	short thresh[V22_DEC24_LEVELS];
	short si, sq;
	short base, sign, step;
	short bestd, best = 0;
	short limit;
	short sym = 0;
	short ci, cq, quad, prev;
	short i, k, j;
	int amp;

	thresh[0] = V22_DEC24_THRESH;

	/*
	 * The quadrant, from the two signs.  `sign` is carried out of the I
	 * test because the amplitude threshold below is compared against the
	 * signed sample rather than against its magnitude.
	 */
	si = state->out_i[state->n_out];
	if (si < 0) {
		base = 0;
		sign = -1;
	} else {
		base = 8;
		sign = 1;
	}

	sq = state->out_q[state->n_out];
	if (sq < 0)
		base = (short)(base + 4);

	/*
	 * The I amplitude.  One threshold, and the loop halves its step each
	 * time round as a binary search would -- with a single level that step
	 * is 2 and the halving is the only trace left of the general case.
	 */
	step = 4;
	for (i = 0; i < V22_DEC24_LEVELS; i++) {
		step = (short)(step >> 1);
		if ((short)((thresh[i] * sign) << V22_DEC_LEVEL_SHIFT) <= si)
			base = (short)(base + step);
	}

	/*
	 * The Q amplitude, by distance rather than by threshold, over the two
	 * candidates the index bits above have left.
	 */
	bestd = (short)(thresh[0] << V22_DEC_LEVEL_SHIFT);
	limit = (short)(base + V22_DEC24_WINDOW);
	for (k = base; k < limit; k++) {
		short qm = DECv22_QMAP24[k];
		short d = (short)(sq >= qm ? sq - qm : qm - sq);

		if (d < bestd) {
			best = k;
			bestd = d;
		}
	}

	ci = DECv22_IMAP24[best];
	cq = DECv22_QMAP24[best];
	*angle = DECv22_ANGL24[best];

	/*
	 * The ring, from the city-block distance of the HALVED coordinates:
	 * 4096, 8192 or 12288, so one of three entries.
	 */
	amp = (ci >> 1) < 0 ? -(ci >> 1) : (ci >> 1);
	amp += (cq >> 1) < 0 ? -(cq >> 1) : (cq >> 1);
	*mag = DECv22_MAG24[((short)amp >> V22_DEC_LEVEL_SHIFT) - 1];

	for (j = 0; j <= 15; j++) {
		if ((short)(SMCv22_IMAP_2400BPS[j] >> 1) != ci)
			continue;
		if ((short)(SMCv22_QMAP_2400BPS[j] >> 1) != cq)
			continue;
		sym = j;
		break;
	}

	quad = (short)(sym & V22_SYM_QUAD);
	prev = *state->prev_quad;
	*state->prev_quad = quad;

	/*
	 * The quadrant CHANGE is differential; the amplitude pair in the low
	 * two bits is not, and is passed straight through.
	 */
	return (unsigned short)
	    (SMCv22_PMAP[(((unsigned short)quad - (unsigned short)prev)
			  & V22_SYM_MODULO) >> V22_SYM_QUAD_SHIFT]
	     | (sym & V22_SYM_AMP));
}
/*
 * The 1200 bit/s slicer: four candidates, full squared-distance search.
 *
 * The distance is accumulated in SIXTEEN bits -- each squared axis error is
 * shifted down sixteen before the two are added, and the sum is truncated back
 * to a short before the comparison -- so a point far enough from every
 * candidate can wrap.  That is the object's arithmetic.
 */
unsigned short
FSEv22_decision12(struct v22_fse *state, short *angle, short *mag)
{
	short si = state->out_i[state->n_out];
	short sq = state->out_q[state->n_out];
	short bestd = 0x7fff;
	short best = 0;
	short sym = 0;
	short ci, cq, quad, prev;
	short k, j;

	for (k = 0; k <= 3; k++) {
		short di = (short)(si - DECv22_IMAP12[k]);
		short dq = (short)(sq - DECv22_QMAP12[k]);
		short d = (short)(((di * di) >> 16) + ((dq * dq) >> 16));

		if (d < bestd) {
			best = k;
			bestd = d;
		}
	}

	ci = DECv22_IMAP12[best];
	cq = DECv22_QMAP12[best];
	*angle = DECv22_ANGL12[best];
	*mag = V22_DEC12_MAG;

	/*
	 * Back from a decision to the transmit numbering.  The transmit tables
	 * are at half this scale, so they are shifted rather than these being
	 * doubled; no match leaves `sym` at zero.
	 */
	for (j = 0; j <= 15; j++) {
		if ((short)(SMCv22_IMAP_1200BPS[j] >> 1) != ci)
			continue;
		if ((short)(SMCv22_QMAP_1200BPS[j] >> 1) != cq)
			continue;
		sym = j;
		break;
	}

	quad = (short)(sym & V22_SYM_QUAD);
	prev = *state->prev_quad;
	*state->prev_quad = quad;

	return (unsigned short)
	    (SMCv22_PMAP[(((unsigned short)quad - (unsigned short)prev)
			  & V22_SYM_MODULO) >> V22_SYM_QUAD_SHIFT]
	     >> V22_SYM_QUAD_SHIFT);
}
