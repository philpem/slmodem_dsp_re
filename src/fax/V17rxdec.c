/*
 * V17rxdec.c -- the V.17 fax receiver's slicers for the library equaliser.
 *
 * Reconstructed from dsplibs.o:
 *   FAX_FSE_decision_128pt .text 0x097c40  910
 *   FAX_FSE_decision_64pt  .text 0x097fd0  517
 *   FAX_FSE_decision_32pt  .text 0x0981e0  571
 *   FAX_FSE_decision_16pt  .text 0x098420  282
 *   FSE_Bridge_det         .text 0x098540  227
 *   FAX_FSE_decision_AB    .text 0x098630  683
 *   FSE_decision_eqtrn     .text 0x0988e0  251
 *
 * `FPM_FSE_receive` calls `cfg.decision(state, &angle, &mag)` once per symbol.
 * The slicer reads the symbol the equaliser just wrote to
 * `state->out_i[n_out]` / `out_q[n_out]`, decides which constellation point it
 * is, writes that point's ideal angle back over `angle` -- which is what gives
 * the carrier PLL its error -- writes an ideal magnitude to `mag`, and returns
 * the decoded symbol.
 *
 * ===========================================================================
 * WHAT THIS SHARES WITH V.32bis, AND WHAT IT DOES NOT
 * ===========================================================================
 *
 * `src/pump/v32/v32fse.c` is the same family for the other protocol, and the
 * four rate slicers here are close relatives of its `_16Tpt`, `_32pt`, `_64pt`
 * and `_128pt`: the same region trees, the same squared-distance metrics at
 * the same shifts, the same 45-degree fold, and the same tail into
 * `VTB_decoder`.  Nineteen of the twenty-two tables they index are
 * byte-identical to V.32bis' (`include/dsplib/v17dec.h`).
 *
 * THE DIFFERENCES ARE READ OFF THE OBJECT, ONE AT A TIME, AND THERE ARE FIVE.
 * Assuming a V.32 property transferred is exactly the mistake this file has to
 * avoid, so each is named with the address that settles it:
 *
 *   1. NO `fse_quality`.  V.32's six share a preamble that smooths a decision-
 *      error measure and asks for a retrain; V.17's have none of it.  What
 *      they have instead is `fse_tick` below -- three instructions.
 *   2. `FAX_FSE_decision_16pt` DOES NOT CARRY D302.  V.32's `_16pt` shifts the
 *      L1 magnitude by 1 where 13 is right and reads thousands of entries past
 *      a three-entry table; V.17 shifts by 13 (`sar $0xd` at 0x984e2) and
 *      reads entries 0..2.  Same table, same length, different code.  V.17's
 *      `_16pt` is in fact V.32's `_16Tpt` -- the sixteen-point TRELLIS slicer,
 *      which tails into `VTB_decoder` -- and not V.32's `_16pt` at all.
 *   3. `FAX_FSE_decision_128pt`'s region tree cuts in SEVEN PLACES ONE HIGHER
 *      than V.32's.  See the comment on that function; the differences are one
 *      value wide and the test drives every one of them explicitly, on its own
 *      exact value, with a denominator per line -- finding F9411.
 *   4. `FAX_FSE_decision_128pt` CLAMPS its table index and reports the clamp
 *      through `dsplibs_debug_printf`.  V.32 has no such guard.
 *   5. THE HANDSHAKE IS A DIFFERENT MACHINE.  V.32 runs AB -> CD -> trn ->
 *      4pt with a differential decode on every symbol; V.17 runs AB -> eqtrn
 *      -> (Bridge_det) -> rate slicer and returns bare symbol numbers.
 *
 * ===========================================================================
 * THE HANDSHAKE, AND WHY `FSEv17_decision` IS A TABLE AND NOT A SWITCH
 * ===========================================================================
 *
 *     FAX_FSE_decision_AB      the two-phase alternation.  Decides by the SIGN
 *                              OF THE PHASE STEP, not by distance, and leaves
 *                              when the constellation has opened up.
 *     FSE_decision_eqtrn       the TRN segment.  Regenerates the sequence from
 *                              its own shift register and reports THAT as the
 *                              decision, so the equaliser adapts against a
 *                              known reference and never looks at the received
 *                              symbol at all.
 *     FSE_Bridge_det           the long-training bridge.  0x3e symbols of a
 *                              four-point decision, then the rate slicer.
 *
 * Each installs its successor in `state->cfg.decision`, and the last hop is
 * `FSEv17_decision[m->rate]` -- so the four rate slicers are reached only
 * through a stored function pointer and never through a call.  That is why
 * `FSEv17_decision` lives in `v17dec_tables.c` and why it could not be written
 * until all four existed.
 */

#include "dsplib/debug.h"
#include "dsplib/v17dec.h"
#include "dsplib/vtb.h"

/*
 * THE ONE THING ALL SIX SHARE, and it is three instructions rather than
 * V.32's `fse_quality`.
 *
 * `sym_count` counts symbols since the receiver started and is never zeroed.
 * Instead of wrapping to zero it is pulled back to 0x4000 on the way past
 * 0x8000, so after the first 32768 symbols it cycles through the top half of
 * the range only.  Nothing reconstructed reads it back; what it is FOR is not
 * established, and `include/dsplib/v17dec.h` says so.
 *
 * `FAX_FSE_decision_AB` does the increment WITHOUT this test -- see the note
 * there -- so it does not use this helper.
 */
static void
fse_tick(struct v17_dec *m)
{
	unsigned short n = (unsigned short)(m->sym_count + 1);

	if (n == 0x8000)
		m->sym_count = 0x4000;
	else
		m->sym_count = n;
}

/*
 * The 45-degree rotation `_32pt` and `_128pt` open with, and the only thing
 * those two share beyond the tick.  It is V.32's `fse_rotate` over V.17's
 * copies of the two tables, instruction for instruction (0x9823d and 0x97c7e
 * against V.32's 0x80c0f).
 *
 * It folds the plane into one octant so that a rectangular region tree can
 * work on eight points instead of thirty-two or a hundred and twenty-eight:
 * `sel` names the octant, the two tables hold its cosine and sine, and the
 * caller carries `sel` through to the magnitude and angle lookup, which is
 * indexed `point + 8*sel` and `point + 32*sel`.
 *
 * BOTH TABLES ARE +-23170, which is 32768/sqrt(2) rounded, so this is exactly
 * a rotation by an odd multiple of 45 degrees with no scaling.  `sel` is
 * `(|I| > |Q|) + (I > Q ? 2 : 0)`: the second term is the diagonal I = Q and
 * the first the anti-diagonal, and together they cut the plane into four.
 *
 * The two products of each output are shifted by 15 SEPARATELY and only then
 * combined, which is not the same as shifting the sum: each term is rounded
 * towards minus infinity on its own, so the result can be one less than the
 * combined form.  The object does it this way and so do we.
 *
 * THE ABSOLUTE VALUES ARE TAKEN IN 32 BITS AND TRUNCATED AFTERWARDS, AND THE
 * TWO STEPS MUST BE SEPARATE STATEMENTS -- findings F8240 and F8241, measured
 * on V.32's copy over twelve spellings.  `short ai = (short)(i < 0 ? -i : i)`
 * distributes the narrowing cast into the COND_EXPR's arms, the result is no
 * longer an ABS_EXPR, and 3.4.2 emits `test`/`jns`/`neg` where the object has
 * `cltd`/`xor`/`sub`.  The object's V.17 copies have the same `cltd`/`xor`/
 * `sub`, so the same spelling is required here.
 */
static int
fse_rotate(int i, int q, short *ri, short *rq)
{
	int ti = i < 0 ? -i : i;
	int tq = q < 0 ? -q : q;
	short ai = (short)ti;
	short aq = (short)tq;
	int sel = (ai > aq) + (i > q ? 2 : 0);
	int c = DECv17_COS_ROT_ANGLE[sel];
	int s = DECv17_SIN_ROT_ANGLE[sel];

	*ri = (short)(((i * c) >> 15) - ((q * s) >> 15));
	*rq = (short)(((q * c) >> 15) + ((i * s) >> 15));
	return sel;
}

/*
 * ===========================================================================
 * THE FOUR RATE SLICERS
 * ===========================================================================
 */

/*
 * 14400 bit/s: a hundred and twenty-eight points, folded by the rotation onto
 * the thirty-two of `ANA_{I,Q}MAP128` and cut by a ten-leaf region tree.
 *
 * TWO LEAVES DO NOT SEARCH AT ALL.  Where the tree cannot separate the point
 * -- the far diagonal corner, on either side of the line -- the function skips
 * the loop and decides between one specific PAIR of points with a squared
 * distance.  `amb` carries which pair, and its two arms disagree about ties:
 * the outer one takes the FARTHER of the two on a tie and the inner one the
 * nearer.  Both are reproduced as written; V.32's copy records the outer tie
 * as D370 and this one behaves the same way.
 *
 * `amb` is `unsigned` because the object shifts it `shrl` (0x97e60), which is
 * a logical shift and so is forced.  Nothing can measure it: the variable only
 * ever holds 0, `FSE128_AMB_OUTER` or `FSE128_AMB_INNER`.  The second test is
 * written as the object encodes it -- a shift, not a mask against the second
 * name -- because a mask would move the code generation off it.
 *
 * THE I COORDINATES OF THOSE FOUR POINTS ARE LITERALS AND ONE IS WRONG, in
 * V.17 exactly as in V.32bis.  Three of them -- 10137, 10137, 13033 -- equal
 * `DECv17_ANA_IMAP128[14]`, `[15]` and `[24]`; the fourth is 15930 where
 * `DECv17_ANA_IMAP128[26]` is 15929.  That one-off is what proves they are
 * literals in the source rather than a compiler's fold of a const table, and
 * it is reproduced: D1200.
 *
 * FOUR OF THE REGION TREE'S CUTS ARE ONE HIGHER THAN V.32's, and each is one
 * value wide.  Reading V.32's thresholds across would be wrong at exactly one
 * point on each of four lines, which no sampled input finds:
 *
 *   here                     V.32's copy              differ at
 *   ri > 0x16a1              ri > 0x16a0              ri == 0x16a1
 *   rq < 0x16a2 ? 0x1c:0x18  rq <= 0x16a0 ? 0x1c:0x18 rq == 0x16a1
 *   ri <= 0x3892             ri <= 0x3891             ri == 0x3892
 *   rq > 0x3892  -> 0x0e     rq > 0x3891  -> 0x0e     rq == 0x3892
 *   rq <= 0x3891 -> 0x1a     rq <  0x3891 -> 0x1a     rq == 0x3891
 *   rq > 0x16a0 ? 0x10:0x14  rq <= 0x169f ? 0x14:0x10 rq == 0x16a0
 *   rq <= 0x16a0 ? 0x08:0x04 rq <= 0x169f ? 0x08:0x04 rq == 0x16a0
 *
 * The spellings above are the object's own encodings and not a normalisation
 * of them: 0x97f98 is `cmp $0x16a2` with `setl` where 0x97e42 is
 * `cmp $0x16a0` with `setle`, and both appear in this one function.
 *
 * THE INDEX CLAMP, AND WHY IT IS THERE.  `n` is `best + 32*rot`, which the
 * region tree bounds at 0x1f + 96 = 127 -- so on every reachable path the
 * clamp cannot fire.  It is in the object because `best` is UNINITIALISED
 * there: the function has exactly two zeroing stores in its prologue, for
 * `base` (0x97c50) and `amb` (0x97c60), where V.32's has three.  The search
 * loop assigns `best` unless all four candidates score exactly 0x7fff, and the
 * author's guard is what happens if they do.  We initialise it, which is D1201
 * -- writing an uninitialised read into `src/` is not something this tree does,
 * and the difference is confined to a case the loop's four simultaneous
 * equations in two unknowns make unreachable.
 */

/*
 * The two bits of `amb`, one per cell of the region tree that cannot be
 * separated.  Which cell each is comes straight out of the tree and is not an
 * inference: bit 0 is set only where both rotated coordinates are past 0x3892
 * and 0x3891, the far corner of the diagonal, and bit 1 only where both are in
 * the cell one step in from it.  Local to this function, because the same bit
 * values mean something else everywhere else.
 */
#define FSE128_AMB_OUTER	(1 << 0)
#define FSE128_AMB_INNER	(1 << 1)

/* The largest index `DECv17_MAG14400` and `DECv17_ANGL14400` can take. */
#define FSE128_INDEX_MAX	127

unsigned short
FAX_FSE_decision_128pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v17_dec *m;
	int i, q, rot;
	unsigned int amb = 0;
	short ri, rq;
	short k, base = 0, best = 0;
	short min, sym, n;

	m = (struct v17_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	fse_tick(m);

	rot = fse_rotate(i, q, &ri, &rq);

	if (ri > 0x16a1) {
		base = 0x0c;
		if (ri > 0x2d41) {
			if (rq <= 0x2d41)
				base = (short)(rq < 0x16a2 ? 0x1c : 0x18);
			else if (ri <= 0x3892) {
				if (rq > 0x3892)
					base = 0x0e;
				else
					amb = FSE128_AMB_INNER;
			} else {
				if (rq <= 0x3891)
					base = 0x1a;
				else
					amb = FSE128_AMB_OUTER;
			}
		} else if (rq <= 0x2d40) {
			base = (short)(rq > 0x16a0 ? 0x10 : 0x14);
		}
	} else if (rq <= 0x2d40) {
		base = (short)(rq <= 0x16a0 ? 0x08 : 0x04);
	}

	if (amb & FSE128_AMB_OUTER) {
		short d14i = (short)(0x2799 - ri);
		short d14q = (short)(DECv17_ANA_QMAP128[14] - rq);
		short d26i = (short)(0x3e3a - ri);
		short d26q = (short)(DECv17_ANA_QMAP128[26] - rq);
		short e14 = (short)(((d14q * d14q) >> 13)
				    + ((d14i * d14i) >> 13));
		short e26 = (short)(((d26q * d26q) >> 13)
				    + ((d26i * d26i) >> 13));

		/* The FARTHER wins on a tie; V.32's copy records it as D370. */
		best = (short)(e26 >= e14 ? 26 : 14);
	} else if (amb >> 1) {
		short d15i = (short)(0x2799 - ri);
		short d15q = (short)(DECv17_ANA_QMAP128[15] - rq);
		short d24i = (short)(0x32e9 - ri);
		short d24q = (short)(DECv17_ANA_QMAP128[24] - rq);
		short e15 = (short)(((d15q * d15q) >> 13)
				    + ((d15i * d15i) >> 13));
		short e24 = (short)(((d24q * d24q) >> 13)
				    + ((d24i * d24i) >> 13));

		/* And here the nearer, with the tie the other way. */
		best = (short)(e24 <= e15 ? 24 : 15);
	} else {
		min = 0x7fff;
		for (k = base; k < base + 4; k++) {
			int di = (short)(ri - DECv17_ANA_IMAP128[k]);
			int dq = (short)(rq - DECv17_ANA_QMAP128[k]);
			short d = (short)(((di * di) >> 13)
					  + ((dq * dq) >> 13));

			if (d < min) {
				best = k;
				min = d;
			}
		}
	}

	n = (short)(best + 32 * rot);
	if ((unsigned short)n > FSE128_INDEX_MAX) {
		/*
		 * The author's own words, `.rodata.str1.4` 0x11da0, and the
		 * only format string this file reaches.  The gate is
		 * `cmpl $0x1,dsplibs_debug_level` with `ja` at 0x97dc4, which
		 * is `DSPLIB_DEBUG_ON()`.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"******************Index fault %d, changed to 0\n",
				n);
		n = 0;
	}

	*mag = DECv17_MAG14400[n];
	*angle = DECv17_ANGL14400[n];

	VTB_decoder(&m->vtb, (short)i, (short)q, &sym);
	return (unsigned short)sym;
}

/*
 * 12000 bit/s: the sixty-four-point constellation, and the plainest of the
 * three region trees -- a four-by-four grid on the symbol as received, with no
 * rotation, cutting the plane at 0 and +-8192 on each axis and searching the
 * four points of the cell that names.
 *
 * THE TWO AXES DO NOT SPLIT AT THE SAME PLACE, and it is one count, not a
 * misreading.  In I the outer band is `i > 8192` and `i <= -8192`; in Q it is
 * `q > 8191` and `q < -8192`.  So a symbol at exactly (8192, 8192) is treated
 * as inner in I and outer in Q, and one at exactly (-8192, -8192) as outer in
 * I and inner in Q.  Writing either axis's test for both -- the tidy reading
 * -- agrees everywhere except on those two lines; the test drives them
 * explicitly for that reason.  This is V.32's tree unchanged, thresholds
 * included, which was checked against the object rather than assumed.
 */
unsigned short
FAX_FSE_decision_64pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v17_dec *m;
	int i, q;
	short k, base, best = 0;
	short min, sym;

	m = (struct v17_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	fse_tick(m);

	if (i > 0) {
		if (q > 0) {
			if (i > 0x2000)
				base = (short)(q > 0x1fff ? 0x28 : 0x2c);
			else
				base = (short)(q > 0x1fff ? 0x20 : 0x24);
		} else {
			if (i > 0x2000)
				base = (short)(q >= -0x2000 ? 0x38 : 0x3c);
			else
				base = (short)(q >= -0x2000 ? 0x30 : 0x34);
		}
	} else {
		if (q > 0) {
			if (i > -0x2000)
				base = (short)(q > 0x1fff ? 0x08 : 0x0c);
			else
				base = (short)(q > 0x1fff ? 0x00 : 0x04);
		} else {
			if (i > -0x2000)
				base = (short)(q >= -0x2000 ? 0x18 : 0x1c);
			else
				base = (short)(q >= -0x2000 ? 0x10 : 0x14);
		}
	}

	min = 0x7fff;
	for (k = base; k < base + 4; k++) {
		int di = (short)(i - DECv17_IMAP64[k]);
		int dq = (short)(q - DECv17_QMAP64[k]);
		short d = (short)(((di * di) >> 13) + ((dq * dq) >> 13));

		if (d < min) {
			best = k;
			min = d;
		}
	}

	*mag = DECv17_MAG12000[best];
	*angle = DECv17_ANGL12000[best];

	VTB_decoder(&m->vtb, (short)i, (short)q, &sym);
	return (unsigned short)sym;
}

/*
 * 9600 bit/s: thirty-two points, and the decision is ONE-DIMENSIONAL.
 *
 * After the rotation every symbol is in one octant, where the eight candidates
 * lie on a line and only their Q coordinates differ -- so the search is
 * `|rq - ANA_QMAP[k]|` over at most three of them and there is no `ANA_IMAP`
 * table at all.  The bands of `ri` choose which three: below 5792 the first
 * three, above it the second three, and in the far corner (`ri` past 11585
 * with `rq` below it) only two.
 *
 * THE TIE-BREAK IS THE TWO-DIMENSIONAL PART.  Points 3 and 6 are the two ends
 * of the fold and share a Q band, so when the linear search lands on either
 * the function re-decides between them with a real squared distance -- and
 * that is the only place `_32pt` uses an I coordinate, which is why the two
 * appear as the literals 8689 and 14481 rather than as a table.  Both match
 * `DECv17_ANA_QMAP` entries, crossed over: point 3 is (8689, 14481) and point
 * 6 is (14481, 8689), which is the mirror in the octant's diagonal.
 *
 * This one is V.32's `_32pt` with the thresholds unchanged -- 0x16a0, 0x2d41
 * and 0x2d40 all read the same at 0x982a7, 0x982b1 and 0x983e9 -- which was
 * checked against the object because `_128pt`'s did move.
 */
unsigned short
FAX_FSE_decision_32pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v17_dec *m;
	int i, q, rot;
	short ri, rq;
	short k, first, count, best = 0;
	short min, sym, n;

	m = (struct v17_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	fse_tick(m);

	rot = fse_rotate(i, q, &ri, &rq);

	first = 0;
	count = 3;
	if (ri > 0x16a0) {
		first = 3;
		if (ri > 0x2d41 && rq <= 0x2d40) {
			first = 6;
			count = 2;
		}
	}

	min = 0x7fff;
	for (k = first; k < first + count; k++) {
		short d = (short)(rq >= DECv17_ANA_QMAP[k]
				  ? rq - DECv17_ANA_QMAP[k]
				  : DECv17_ANA_QMAP[k] - rq);

		if (d < min) {
			best = k;
			min = d;
		}
	}

	if (best == 3 || best == 6) {
		short d3i = (short)(0x21f1 - ri);
		short d3q = (short)(DECv17_ANA_QMAP[3] - rq);
		short d6i = (short)(0x3891 - ri);
		short d6q = (short)(DECv17_ANA_QMAP[6] - rq);
		short e3 = (short)(((d3q * d3q) >> 15) + ((d3i * d3i) >> 15));
		short e6 = (short)(((d6q * d6q) >> 15) + ((d6i * d6i) >> 15));

		/* The NEARER wins, and a tie goes to 3. */
		best = (short)(e6 < e3 ? 6 : 3);
	}

	n = (short)(best + 8 * rot);
	*mag = DECv17_MAG9600T[n];
	*angle = DECv17_ANGL9600T[n];

	VTB_decoder(&m->vtb, (short)i, (short)q, &sym);
	return (unsigned short)sym;
}

/*
 * 7200 bit/s: the sixteen-point trellis constellation, and the only one of the
 * four with no region tree -- it searches all sixteen points.
 *
 * The metric is SYMMETRIC, `(di*di + dq*dq) >> 16` with the two terms shifted
 * separately, unlike `FSE_Bridge_det`'s deliberately lopsided one below.  So
 * the two slicers over the same plane do not agree about what is nearest, and
 * that is in the object, not in this reading.
 *
 * `*mag` is `MAG7200[(|I| + |Q|)/8192 - 1]`.  The sixteen points have
 * |I| + |Q| in {8192, 16384, 24576}, so the index is 0, 1 or 2 and the
 * three-entry table is exactly covered.  V.32's `FSE_decision_16pt` computes
 * that same quantity with `>> 1` instead of `>> 13` and reads 8190 bytes past
 * its table -- D302 -- and THIS FUNCTION DOES NOT: `sar $0xd` at 0x984e2.
 * That was checked rather than assumed, because the two functions are
 * otherwise the same shape.
 */
unsigned short
FAX_FSE_decision_16pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v17_dec *m;
	int i, q, ib, qb, n;
	short k, best = 0;
	short min, sym;

	m = (struct v17_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	fse_tick(m);

	min = 0x7fff;
	for (k = 0; k <= 15; k++) {
		int di = (short)(i - DECv17_IMAP16[k]);
		int dq = (short)(q - DECv17_QMAP16[k]);
		short d = (short)(((di * di) >> 16) + ((dq * dq) >> 16));

		if (d < min) {
			best = k;
			min = d;
		}
	}

	ib = DECv17_IMAP16[best];
	qb = DECv17_QMAP16[best];
	n = ((ib < 0 ? -ib : ib) + (qb < 0 ? -qb : qb)) >> 13;

	*mag = DECv17_MAG7200[n - 1];
	*angle = DECv17_ANGL7200[best];

	VTB_decoder(&m->vtb, (short)i, (short)q, &sym);
	return (unsigned short)sym;
}

/*
 * ===========================================================================
 * THE HANDSHAKE
 * ===========================================================================
 */

/*
 * The magnitude every handshake slicer reports, whatever it decided:
 * `DECv17_MAG7200[1]` = 12953 = the radius of the four handshake points.  The
 * object writes it as the immediate 0x3299 in all three (0x9860d, 0x98876,
 * 0x9898c) and never reads the table, so it is written as the object writes it.
 */
#define FSE_HANDSHAKE_MAG	0x3299

/* The bridge segment's length, from `cmpw $0x3e,0x5a(%edx)` at 0x9854e. */
#define FSE_BRIDGE_SYMBOLS	0x3e

/*
 * The long-training bridge, and the only slicer `V17RX_create` does not
 * install: it is reached solely through `FSE_decision_eqtrn`'s long arm.
 *
 * It decides the four-point handshake constellation for 0x3e symbols, then
 * clears the counter and the TRN register and hands over to
 * `FSEv17_decision[rate]`.  THE HANDOVER IS TESTED AT THE TOP AND SO FIRES ONE
 * SYMBOL LATE: this call still runs its own decision after installing the
 * successor, which is what the object does (0x98553 jumps over the install and
 * falls into the search either way) and not an ordering slip.
 *
 * THE METRIC IS LOPSIDED, exactly as V.32's `FSE_decision_4pt` is (D301): the
 * two squared terms are scaled by DIFFERENT amounts -- 15 places for the
 * in-phase one at 0x985d4 and 16 for the quadrature at 0x985d7 -- so the
 * in-phase error counts double and this is not the nearest-point decision.
 *
 * IT RETURNS A CONSTANT ZERO.  `xor %eax,%eax` at 0x985f8 and nothing writes
 * `%eax` after it: the decided point reaches the caller only through `*angle`,
 * which is the carrier PLL's error term, and never as data.
 */
unsigned short
FSE_Bridge_det(struct fpm_fse *state, short *angle, short *mag)
{
	struct v17_dec *m = (struct v17_dec *)state->cfg.owner;
	int i, q;
	short k, best = 0;
	short min;

	if (m->count > FSE_BRIDGE_SYMBOLS) {
		short rate = m->rate;

		m->count = 0;
		m->scram = 0;
		state->cfg.decision = FSEv17_decision[rate];
	}

	m->count = (short)(m->count + 1);
	fse_tick(m);

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	min = 0x7fff;
	for (k = 0; k <= 3; k++) {
		int di = (short)(i - DECv17_IMAP4[k]);
		int dq = (short)(q - DECv17_QMAP4[k]);
		short d = (short)(((di * di) >> 15) + ((dq * dq) >> 16));

		if (d < min) {
			best = k;
			min = d;
		}
	}

	*angle = DECv17_ANGL4800[best];
	*mag = FSE_HANDSHAKE_MAG;
	return 0;
}

/*
 * The AB segment, which is where the receiver starts -- `V17RX_create` stores
 * this function's address into the equaliser configuration at 0x974d4.
 *
 * A and B are two alternating phases 180 degrees apart, so the slicer decides
 * by the SIGN OF THE PHASE STEP rather than by distance: 0x8000 is a full
 * cycle in these units, so a step of more than the 0x4000 half cycle is an A
 * and anything else a B, and each keeps its own smoothed magnitude.
 *
 * THE ANGLE INDEX AND THE RETURNED SYMBOL ARE NOT THE SAME NUMBER, which V.32's
 * copy of this function does not prepare you for -- there they are one value
 * put through a differential map.  Here they are two independent immediates
 * per arm (0x98694/0x986ba, 0x98806/0x98822, 0x9879a/0x98795), and the
 * pairings are A -> (1, 3), B -> (2, 2) and the handover -> (3, 0).
 *
 * THE EXIT IS A SIGNAL-QUALITY TEST, NOT A COUNT: when the last three symbols
 * have moved further apart than four thirds of the two magnitudes' mean square
 * and at least 0xc8 symbols have gone by, the segment is over, everything is
 * zeroed, the TRN generator is seeded and `FSE_decision_eqtrn` takes over.
 *
 * THE SYMBOL COUNTER IS INCREMENTED WITHOUT `fse_tick`'s wrap test.  0x98651
 * is `movzwl`, `inc`, `mov` with no `cmp $0x8000` anywhere, where the other
 * five slicers all have one.  It cannot matter -- AB is the first segment and
 * leaves after 0xc8 symbols, not 0x8000 -- but it is the object's own asymmetry
 * and is reproduced rather than tidied: D1202.
 */

/*
 * Four thirds in Q14: 4 * 16384 / 3 = 21845.33, and 21845 is 0x5555.  The
 * object multiplies by that immediate and shifts by 14 (0x98768), where V.32's
 * copy of the same threshold shifts left by two and does a real signed divide
 * by three (`imul $0x55555556` at 0x8135b).  So the two are NOT the same
 * expression and V.32's `/ 3` must not be carried across: this one rounds
 * towards minus infinity and is one unit low on a third of its inputs.
 */
#define FSE_AB_THRESHOLD_Q14	21845

/* The two segment lengths AB counts against, both signed `cmpw`. */
#define FSE_AB_PLL_SYMBOLS	0x96
#define FSE_AB_EXIT_SYMBOLS	0xc8

/*
 * What AB hands the TRN generator when it seeds it, `movl $0xbb3754` at
 * 0x987f2.  Only the bits `FSE_decision_eqtrn`'s taps reach ever matter.
 */
#define FSE_AB_SCRAM_SEED	0x00bb3754

/* The LMS step size AB leaves behind for the training segment, 0x987e8. */
#define FSE_AB_MU0		0x147b

unsigned short
FAX_FSE_decision_AB(struct fpm_fse *state, short *angle, short *mag)
{
	struct v17_dec *m = (struct v17_dec *)state->cfg.owner;
	short prev_i1 = m->sym_i1;
	short prev_q1 = m->sym_q1;
	short eqm_a = m->eqm_a;
	short eqm_b = m->eqm_b;
	short delta;
	short sym, idx;
	int i, q;
	int di, dq, d1, d2, sum, thr;
	short e;

	state->lms_on = 0;

	m->count = (short)(m->count + 1);
	m->sym_count = (unsigned short)(m->sym_count + 1);

	delta = (short)(*angle - m->ang_prev);
	m->ang_prev = *angle;
	if (delta < 0)
		delta = (short)(delta - 0x8000);

	if (delta > 0x4000) {
		eqm_a = (short)(((31 * eqm_a) >> 5) + (*mag >> 5));
		sym = 3;
		idx = 1;
	} else {
		eqm_b = (short)(((31 * eqm_b) >> 5) + (*mag >> 5));
		sym = 2;
		idx = 2;
	}

	/*
	 * On the short-training path the carrier loop is turned on part way
	 * through AB rather than at the handover.  `pll_on` is written and
	 * never read back here; `FPM_FSE_receive` is what acts on it.
	 */
	if (m->short_train && m->count > FSE_AB_PLL_SYMBOLS)
		state->pll_on = 1;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	di = (short)(m->sym_i1 - i);
	dq = (short)(m->sym_q1 - q);
	d1 = di * di + dq * dq;

	di = (short)(m->sym_i2 - m->sym_i);
	dq = (short)(m->sym_q2 - m->sym_q);
	d2 = di * di + dq * dq;

	sum = (short)((d1 >> 15) + (d2 >> 15));
	e = (short)(((int)eqm_a * eqm_a + (int)eqm_b * eqm_b) >> 15);
	thr = (e * FSE_AB_THRESHOLD_Q14) >> 14;

	if (sum > thr && m->count > FSE_AB_EXIT_SYMBOLS) {
		m->sym_q2 = 0;
		m->sym_i2 = 0;
		m->sym_q1 = 0;
		m->sym_i1 = 0;
		m->sym_q = 0;
		m->sym_i = 0;
		m->ang_prev = 0;
		state->lms_on = 1;
		state->lms_force = 1;
		m->count = 0;
		state->cfg.decision = FSE_decision_eqtrn;
		m->int_0050 = 1;
		state->cfg.mu[0] = FSE_AB_MU0;
		m->scram = FSE_AB_SCRAM_SEED;
		eqm_a = 0;
		eqm_b = 0;
		sym = 0;
		idx = 3;
	} else {
		m->sym_q1 = m->sym_q;
		m->sym_i1 = m->sym_i;
		m->sym_q = (short)q;
		m->sym_i = (short)i;
		m->sym_q2 = prev_q1;
		m->sym_i2 = prev_i1;
	}

	*angle = DECv17_ANGL4800[idx];
	*mag = FSE_HANDSHAKE_MAG;

	m->eqm_a = eqm_a;
	m->eqm_b = eqm_b;
	return (unsigned short)sym;
}

/*
 * The equaliser-training slicer.  It does not look at the received symbol at
 * all: it regenerates the TRN sequence from its own shift register and reports
 * that as the decision, so the equaliser adapts against a known reference.
 *
 * THE GENERATOR IS V.32bis' OWN, with the tap fixed rather than carried in a
 * field.  `FSE_decision_trn` in `src/pump/v32/v32fse.c` computes
 * `(((sr >> tap) ^ 3) ^ (sr >> 21)) & 3` and shifts the register left by two;
 * this is the same expression with `tap` = 16, which is a second, independent
 * reading of the same generator.
 *
 * THE TWO SEGMENT LENGTHS ARE WHAT NAME `short_train`.  0x25 symbols against
 * 0xb9f is a factor of eighty, and the object picks between them with
 * `cmp $0x1` / `sbb` / `and $0xb7a` / `lea 0x25` at 0x988ed -- one branchless
 * select on `m->short_train` alone.
 *
 * AND THE TWO ARMS OF THE HANDOVER DIFFER IN MORE THAN LENGTH: the short path
 * goes straight to `FSEv17_decision[rate]` and the long one goes to
 * `FSE_Bridge_det` first, which then spends another 0x3e symbols before doing
 * the same thing.
 */

/* The training segment's length, short path and long path. */
#define FSE_EQTRN_SHORT		0x25
#define FSE_EQTRN_LONG		0xb9f

/* The LMS step size the handover selects, and the index it selects it by. */
#define FSE_EQTRN_MU1		0x51f
#define FSE_EQTRN_MU_SEL	1

unsigned short
FSE_decision_eqtrn(struct fpm_fse *state, short *angle, short *mag)
{
	struct v17_dec *m = (struct v17_dec *)state->cfg.owner;
	int limit = m->short_train ? FSE_EQTRN_SHORT : FSE_EQTRN_LONG;
	unsigned int sr = (unsigned int)m->scram;
	int c;
	short pt;

	/*
	 * `sr >> 16` is ARITHMETIC in the object (`sar $0x10`, 0x98902) and
	 * `sr >> 21` is LOGICAL (`shr $0x15`, 0x98908).  Only the low two bits
	 * survive the mask, so the two readings agree over every value -- but
	 * the first shift is what makes `scram` a signed field, and the cast
	 * here is what keeps the second one logical without making the shift
	 * left undefined.
	 */
	c = (int)((((int)sr >> 16) ^ 3) ^ (sr >> 21)) & 3;

	m->count = (short)(m->count + 1);
	fse_tick(m);

	if (m->count < limit) {
		m->scram = (int)((sr << 2) | (unsigned int)c);
	} else {
		state->mu_sel = FSE_EQTRN_MU_SEL;
		if (m->short_train)
			state->cfg.decision = FSEv17_decision[m->rate];
		else
			state->cfg.decision = FSE_Bridge_det;
		state->cfg.mu[1] = FSE_EQTRN_MU1;
		m->count = 0;
		m->scram = 0;
		m->int_0050 = 0;
	}

	pt = DECv17_MAP_TRN[c];
	*angle = DECv17_ANGL4800[pt];
	*mag = FSE_HANDSHAKE_MAG;
	return (unsigned short)pt;
}
