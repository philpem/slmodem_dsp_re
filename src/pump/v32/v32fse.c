/*
 * v32fse.c -- V.32's slicers for the library equaliser.
 *
 * Reconstructed from dsplibs.o:
 *   FSE_decision_AB    .text 0x081260   678
 *   FSE_decision_4pt   .text 0x081080   468
 *   FSE_decision_trn   .text 0x081590   225
 *   FSE_decision_CD    .text 0x081510   122
 *   FSE_decision_128pt .text 0x0804e0  1032
 *   FSE_decision_32pt  .text 0x080bb0   721
 *   FSE_decision_64pt  .text 0x0808f0   694
 *   FSE_decision_16Tpt .text 0x080330   432
 *   FSE_decision_16pt  .text 0x080e90   490
 *
 * `_16pt` was left out for a long time -- finding 1603 and D302, because its
 * magnitude lookup reads thousands of entries past a three-entry table and
 * what it returns is a property of the LINK.  It is here now: everything
 * except that one store is ordinary and differentially testable, the store
 * itself is fixed behind `DSPLIB_REPRODUCE_BUGS`, and the one ring whose
 * out-of-bounds read stays inside the blob's own `.data` IS compared.
 * Findings 3800 and 3801.
 *
 * THE SEQUENCE THE FIRST FOUR IMPLEMENT.  Each installs its own successor in
 * `state->cfg.decision`, so the chain is the receiver's handshake:
 *
 *     AB  --(the two-tone phase alternation stops)-->  CD
 *     CD  --(after 15 symbols)-->                      trn
 *     trn --(after 1280 symbols)-->                    4pt
 *
 * and the datapump switches to a data-rate slicer from there.
 *
 * THE FOUR TRELLIS SLICERS are the data-rate end of that: 7200, 9600, 12000
 * and 14400 bit/s, one per V.32bis trellis constellation.  All four share the
 * same three-part shape --
 *
 *   1. narrow the search to a handful of candidates with a REGION TREE of
 *      integer comparisons (`_16Tpt` alone skips this and searches all 16),
 *   2. pick the nearest of those by a squared-distance metric, and
 *   3. write that point's ideal magnitude and angle, then hand the received
 *      symbol AS RECEIVED to `VTB_decoder` and return what it writes back.
 *
 * -- and none of them installs a successor or touches `cfg.decision`.  The
 * decision they return is the Viterbi decoder's, which is the decision made
 * sixteen symbols ago; the point this function decided is used only for the
 * equaliser's and the carrier loop's error terms.
 */

#include "dsplib/v32dec.h"
#include "dsplib/vtb.h"

/*
 * The common preamble of six of the nine slicers, and the only thing they
 * share.  It measures how far the symbol two back is from this one, smooths
 * that into `eqm`, and asks for a retrain if the constellation has gone quiet
 * for between 21 and 59 symbols while `eqm` is still small.
 *
 * `retrain` is written and never read here; it is a request to the datapump.
 */
static void
fse_quality(struct fpm_fse *state)
{
	struct v32_dec *m = (struct v32_dec *)state->cfg.owner;
	int i = state->out_i[(short)state->n_out];
	int q = state->out_q[(short)state->n_out];
	int di = (short)(m->sym_i1 - i);
	int dq = (short)(m->sym_q1 - q);
	int energy = (di * di + dq * dq) >> 16;
	short n;

	m->sym_q1 = m->sym_q;
	m->sym_i1 = m->sym_i;
	m->sym_q = (short)q;
	m->sym_i = (short)i;

	m->eqm = (short)(((15 * m->eqm) >> 4) + (energy >> 4));

	if (m->eqm > 0x4ff) {
		m->count = 0;
		return;
	}

	n = (short)(m->count + 1);
	if (m->eqm < (short)(energy >> 3) && n <= 0x3b && n > 0x14) {
		m->retrain = 2;
		m->count = 0;
		return;
	}

	m->count = n;
	if (m->count == 0x3c)
		m->retrain = 1;
}

/*
 * The differential half of the decision, shared by `_4pt` and `_AB`.
 * `_16pt` needs the decided point's own low two bits as well and has its own
 * copy below; see the comment there for why it is a copy.
 *
 * The decided point is looked up in the ENCODER's ordering of the sixteen
 * points, its quadrant is differenced against the previous symbol's, and the
 * rotation that difference names is the pair of differentially encoded bits.
 * A point that is not one of the sixteen leaves the index at zero.
 */
static int
fse_differential(struct v32_dec *m, short ib, short qb)
{
	int prev = m->prev_sym[(short)m->chan];
	int found = 0;
	int quad;
	int j;

	for (j = 0; j <= 15; j++)
		if (SMCv32_IMAP16[j] == ib && SMCv32_QMAP16[j] == qb) {
			found = j;
			break;
		}

	quad = found & 0xc;
	m->prev_sym[(short)m->chan] = (short)quad;
	return SMCv32_PMAP16[((quad - prev) & 0xf) >> 2];
}

/*
 * The four-point slicer: V.32's 4800 bit/s constellation, and the one the
 * training sequence hands over to.  Two bits out.
 *
 * The two squared terms are scaled by DIFFERENT amounts -- 15 places for the
 * in-phase one and 16 for the quadrature -- so the in-phase error counts
 * double and this is NOT the nearest-point decision.  It changes real
 * answers: at a received (2000, 2000) the object picks point 0 where the
 * symmetric metric picks point 3.  Reproduced as written; D301.
 */
unsigned short
FSE_decision_4pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m;
	int i, q, k;
	int best = 0;
	int rot;
	short min;

	fse_quality(state);
	m = (struct v32_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	min = 0x7fff;
	for (k = 0; k <= 3; k++) {
		int di = (short)(i - DECv32_IMAP4[k]);
		int dq = (short)(q - DECv32_QMAP4[k]);
		short d = (short)(((di * di) >> 15) + ((dq * dq) >> 16));

		if (d < min) {
			best = k;
			min = d;
		}
	}

	*angle = DECv32_ANGL1200[best];
	*mag = 0x3299;

	rot = fse_differential(m, DECv32_IMAP4[best], DECv32_QMAP4[best]);
	return (unsigned short)(rot >> 2);
}

/*
 * The training slicer.  It does not look at the received symbol at all: it
 * regenerates the TRN sequence from its own shift register and reports that
 * as the decision, so the equaliser adapts against a known reference.
 *
 * Three phases, by symbol count: up to 256 the sequence is forced to the two
 * points 1 and 3 (an alternating pattern the carrier PLL can pull in on),
 * from 256 to 1280 the full four-point sequence through `DECv32_MAP_TRN`,
 * and after 1280 it hands over to `FSE_decision_4pt`.
 */
unsigned short
FSE_decision_trn(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m = (struct v32_dec *)state->cfg.owner;
	unsigned int sr = m->scram;
	int c;
	short n;

	c = (int)(((sr >> m->scram_tap) ^ 3) ^ (sr >> 21)) & 3;
	m->scram = (sr << 2) | (unsigned int)c;

	n = (short)(m->count + 1);
	if (n > 0xff) {
		if (n > 0x4ff) {
			/*
			 * Hand over.  The empty loop the object runs over
			 * `cfg.taps` here has no effect and is not
			 * reproduced -- see finding 1604.
			 */
			m->count = 0;
			c = 0;
			state->cfg.decision = FSE_decision_4pt;
			m->rate_change = 1;
		} else {
			m->count = n;
			c = DECv32_MAP_TRN[c];
		}
	} else {
		m->count = n;
		c = (c >> 1) >= 1 ? 3 : 1;
	}

	*angle = DECv32_ANGL1200[c];
	*mag = 0x3299;
	return (unsigned short)c;
}

/*
 * The CD segment: fifteen symbols of the V.32 answer-tone phase reversal,
 * counted out and then handed to the training slicer.
 *
 * It ignores the received symbol and never writes `angle`, so the carrier
 * PLL sees an error of exactly zero for the whole segment -- which is the
 * point, since there is nothing yet worth tracking.  The decision alternates
 * between the two points 0 and 3, and adaptation is held off until the
 * handover.
 */
unsigned short
FSE_decision_CD(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m = (struct v32_dec *)state->cfg.owner;
	short n = (short)(m->count + 1);

	if (n > 0x0e) {
		m->count = 0;
		state->mu_sel = 0;
		state->cfg.decision = FSE_decision_trn;
		state->lms_on = 1;
		m->scram = 0;
	} else {
		m->count = n;
		state->lms_on = 0;
	}

	*mag = 0x3299;
	return (unsigned short)((m->count & 1) ? 3 : 0);
}

/*
 * The AB segment, which is where the receiver starts.  A and B are two
 * alternating phases 180 degrees apart, so the slicer decides by the SIGN OF
 * THE PHASE STEP rather than by distance: 0x8000 is a full cycle in these
 * units, so a step of more than the 0x4000 half cycle is an A and anything
 * else a B, and each keeps its own smoothed magnitude.
 *
 * The exit is a signal-quality test, not a count: when the last three symbols
 * have moved further apart than four thirds of the two magnitudes' mean
 * square, and at least 151 symbols have gone by, the segment is over and
 * everything is zeroed for `FSE_decision_CD`.
 */
unsigned short
FSE_decision_AB(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m = (struct v32_dec *)state->cfg.owner;
	short prev_i1 = m->sym_i1;
	short prev_q1 = m->sym_q1;
	short eqm_a = m->eqm;
	short eqm_b = m->eqm_b;
	short delta;
	short sym;
	int i, q;
	int di, dq, d1, d2, sum, thr;
	int rot;

	delta = (short)(*angle - m->ang_prev);
	m->ang_prev = *angle;
	if (delta < 0)
		delta = (short)(delta - 0x8000);

	if (delta > 0x4000) {
		eqm_a = (short)(((31 * eqm_a) >> 5) + (*mag >> 5));
		sym = 1;
	} else {
		eqm_b = (short)(((31 * eqm_b) >> 5) + (*mag >> 5));
		sym = 2;
	}

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	di = (short)(m->sym_i1 - i);
	dq = (short)(m->sym_q1 - q);
	d1 = di * di + dq * dq;

	di = (short)(m->sym_i2 - m->sym_i);
	dq = (short)(m->sym_q2 - m->sym_q);
	d2 = di * di + dq * dq;

	sum = (short)((d1 >> 15) + (d2 >> 15));
	thr = (4 * (short)(((int)eqm_a * eqm_a + (int)eqm_b * eqm_b) >> 13))
	      / 3;

	if (sum > thr && (short)m->count > 0x96) {
		m->sym_q2 = 0;
		state->cfg.decision = FSE_decision_CD;
		m->sym_i2 = 0;
		m->sym_q1 = 0;
		m->sym_i1 = 0;
		m->sym_q = 0;
		m->sym_i = 0;
		m->ang_prev = 0;
		m->rate_change = 1;
		m->count = 0;
		eqm_a = 0;
		eqm_b = 0;
		sym = 3;
	} else {
		m->sym_q1 = m->sym_q;
		m->sym_i1 = m->sym_i;
		m->sym_q = (short)q;
		m->sym_i = (short)i;
		m->sym_q2 = prev_q1;
		m->sym_i2 = prev_i1;
	}

	m->count = (unsigned short)(m->count + 1);

	*angle = DECv32_ANGL1200[sym];
	*mag = 0x3299;

	rot = fse_differential(m, DECv32_IMAP4[sym], DECv32_QMAP4[sym]);

	m->eqm_b = eqm_b;
	m->eqm = eqm_a;
	return (unsigned short)(rot >> 2);
}

/*
 * The differential half again, plus the decided point's own low two bits.
 *
 * DELIBERATELY A SECOND COPY of `fse_differential` rather than a fourth
 * argument on it.  The object INLINES this at all three of its call sites, so
 * there is no shared symbol in the blob to preserve; an out-parameter would
 * take the address of a local in `_4pt` and `_AB`, which GCC 3.4 forces to
 * memory, and would move the code generation of two functions that already
 * match in order to save twelve lines.
 *
 * 9600 bit/s carries four bits and this is where they are assembled: the two
 * the quadrant rotation encodes -- which is the whole of what `_4pt` returns
 * -- OR'd with the two that name the point within its quadrant.  An OR is
 * enough because every `SMCv32_PMAP16` entry (4, 0, 8, 12) has its low two
 * bits clear, and the object encodes it as one: `or %ebx,%ebp` at 0x8104f
 * against `and $0x3,%ebx` at 0x8103b.
 */
static int
fse_differential_16(struct v32_dec *m, short ib, short qb)
{
	int prev = m->prev_sym[(short)m->chan];
	int found = 0;
	int quad;
	int j;

	for (j = 0; j <= 15; j++)
		if (SMCv32_IMAP16[j] == ib && SMCv32_QMAP16[j] == qb) {
			found = j;
			break;
		}

	quad = found & 0xc;
	m->prev_sym[(short)m->chan] = (short)quad;
	return SMCv32_PMAP16[((quad - prev) & 0xf) >> 2] | (found & 3);
}

/*
 * 9600 bit/s WITHOUT the trellis: the same sixteen-point constellation
 * `_16Tpt` decides on, but reported as four differentially encoded bits
 * instead of handed to the Viterbi decoder.  It is `FSEv32_decision[1]`
 * (`.data` 0x74cc, three function pointers) and `SetRxModeV32` installs it on
 * one arm of its seven-way jump table, so it is live code and not a leftover.
 *
 * TWO DEFECTS, INDEPENDENT OF EACH OTHER, AND WE TREAT THEM DIFFERENTLY.
 * The magnitude lookup is D302 and is FIXED here, behind
 * `DSPLIB_REPRODUCE_BUGS` -- see the long comment at the store.  The search
 * metric is D451 and is REPRODUCED, because there is nothing to fix it to
 * that would not be a shift we invented.
 *
 * THE METRIC IS NOT SCALED, SO IT WRAPS, and that is the object's, not a
 * misreading: 0x80f78..0x80f82 is `imul`, `imul`, `add`, `cwtl` with no `sar`
 * anywhere between, where every sibling scales both squared terms before
 * summing them (`_16Tpt` 16 and 16, `_4pt` 15 and 16, `_64pt` and `_128pt` 13
 * and 13).  The consequence is not a rounding difference.  Both coordinates
 * of every point are multiples of 4096, so at an exact constellation point
 * every difference is a multiple of 8192 and every squared difference a
 * multiple of 2**26 -- zero in the low sixteen bits.  All sixteen candidates
 * therefore score 0, `min` starts at 0x7fff, the first candidate takes it and
 * nothing can beat it, and the function decides POINT 0 for every one of the
 * sixteen symbols it could be shown.  Over the whole (I, Q) plane only eight
 * of the sixteen points are reachable at all -- 0, 1, 2, 3, 4, 5, 8 and 10 --
 * and only one of those, point 3, is on the inner ring.  Finding 3801.
 */
unsigned short
FSE_decision_16pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m;
	int i, q, ib, qb;
	short k, best = 0;
	short min, n;

	fse_quality(state);
	m = (struct v32_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	min = 0x7fff;
	for (k = 0; k <= 15; k++) {
		int di = (short)(i - DECv32_IMAP16[k]);
		int dq = (short)(q - DECv32_QMAP16[k]);
		/*
		 * The sum is taken unsigned only to keep it defined.  Each
		 * product fits an `int` on its own; their sum reaches exactly
		 * 2**31 at di = dq = -32768, which (-28672, -28672) against
		 * point 9 produces.  The object's `add` wraps, so wrapping is
		 * the behaviour to keep, and unsigned addition is how C says
		 * it without leaning on overflow.
		 */
		short d = (short)((unsigned)(di * di) + (unsigned)(dq * dq));

		if (d < min) {
			best = k;
			min = d;
		}
	}

	ib = DECv32_IMAP16[best];
	qb = DECv32_QMAP16[best];
	n = (short)((ib < 0 ? -ib : ib) + (qb < 0 ? -qb : qb));

	/*
	 * D302 -- THE MAGNITUDE LOOKUP, AND THE ONE NIBBLE THAT BREAKS IT.
	 *
	 * THE BUG.  The object has `sar $1,%edi` at 0x80fcb where `sar
	 * $0xd,%edi` belongs, and then reads `DECv32_MAG9600[edi - 1]`.
	 * `n` is `|I| + |Q|` for the decided point, and because the sixteen
	 * points sit at +-4096 and +-12288 it takes exactly three values.
	 * `>> 13` maps those onto 1, 2, 3 and so onto the table's entries 0,
	 * 1, 2.  `>> 1` maps them onto 4096, 8192, 12288 and so onto entries
	 * 4095, 8191, 12287 of a THREE-entry table.
	 *
	 * WHAT THE OBJECT ACTUALLY RETURNS, measured in the linked daemon
	 * `slmodemd/slmodemd`, where `DECv32_MAG9600` is at 0x110878:
	 *
	 *   point | |I|+|Q| | index | lands in              | value | correct
	 *   ------+---------+-------+-----------------------+-------+--------
	 *   inner |    8192 |  4095 | .data                 |     0 |    5792
	 *   mid   |   16384 |  8191 | .bss                  |     0 |   12953
	 *   outer |   24576 | 12287 | past the last section | fault |   17378
	 *                                              (or arbitrary)
	 *
	 * THE EFFECT.  `*mag` is what `FPM_FSE_receive` turns into the
	 * equaliser's error term -- `err_i = ((cos * mag) >> 14) - i_val` and
	 * the quadrature twin, at src/dsp/fpm_fse.c:385.  A zero magnitude
	 * says the decided point is the ORIGIN, so the LMS update adapts on
	 * the whole received vector instead of on the decision error and the
	 * taps cannot converge.
	 *
	 * HOW WE CONCLUDED 13 IS THE FIX.  Four independent lines, none of
	 * them a preference:
	 *
	 *   1. `>> 13` is the only shift that covers the table exactly.  The
	 *      three sums are 8192, 16384 and 24576 and the table has three
	 *      entries; `>> 13` less one is 0, 1, 2.
	 *   2. The table is right and complete.  { 5792, 12953, 17378 } are
	 *      the three L2 magnitudes of the constellation to the unit:
	 *      4096*sqrt(2) = 5792.6, sqrt(12288^2 + 4096^2) = 12952.99,
	 *      12288*sqrt(2) = 17377.9.  It is an L1 -> L2 conversion table.
	 *   3. `FSE_decision_16Tpt` (0x80330) computes the same quantity off
	 *      the same two tables and shifts it `sar $0xd` (0x80472).  Same
	 *      code, one nibble apart.
	 *   4. `_4pt` and `_trn` hard-code `*mag = 0x3299` = 12953 =
	 *      `DECv32_MAG9600[1]`, which corroborates both the table and its
	 *      scaling from a third place.
	 *
	 * WHAT THE REPRODUCE_BUGS ARM RETURNS, AND WHY IT IS A FLAT ZERO.
	 * Zero is the measured answer for the inner and mid rings above.  It
	 * is a stated CHOICE for the outer one: there is no value to
	 * reproduce, because the object's read is past the last section.  An
	 * out-of-range subscript is not written here at any setting -- it
	 * would be undefined behaviour in a build that has to stay 64-bit
	 * clean, and it would read OUR bytes rather than the object's.
	 *
	 * ONLY THE INNER RING IS DIFFERENTIALLY COMPARABLE, and that is a
	 * narrower claim than the daemon table above.  In the differential
	 * tier the object is linked as `dsplibs_ref.o`, so its read is at
	 * `ref_DECv32_MAG9600 + 8190/16382/24574`.  The blob's `.data` is
	 * 0x9594 bytes and the table is at 0x74f8, so only the first of those
	 * -- .data+0x94f6, 158 bytes short of the end -- stays inside the
	 * blob's own section and travels with it into any link.  That byte is
	 * 0.  The other two leave the section and read whatever the LINKER
	 * put there, which is the argument finding 1603 made about all three.
	 * `t_v32fse.c` compares `*mag` on the inner ring only and asserts
	 * that it excluded the other two.
	 */
#ifdef DSPLIB_REPRODUCE_BUGS
	(void)n;	/* the index the object computes and then misuses */
	*mag = 0;
#else
	*mag = DECv32_MAG9600[(n >> 13) - 1];
#endif
	*angle = DECv32_ANGL9600[best];

	return (unsigned short)fse_differential_16(m, (short)ib, (short)qb);
}

/*
 * ===========================================================================
 * THE TRELLIS SLICERS
 * ===========================================================================
 */

/*
 * The 45-degree rotation `_32pt` and `_128pt` open with, and the only thing
 * those two share beyond the preamble.
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
 */
static int
fse_rotate(int i, int q, short *ri, short *rq)
{
	short ai = (short)(i < 0 ? -i : i);
	short aq = (short)(q < 0 ? -q : q);
	int sel = (ai > aq) + (i > q ? 2 : 0);
	int c = DECv32_COS_ROT_ANGLE[sel];
	int s = DECv32_SIN_ROT_ANGLE[sel];

	*ri = (short)(((i * c) >> 15) - ((q * s) >> 15));
	*rq = (short)(((q * c) >> 15) + ((i * s) >> 15));
	return sel;
}

/*
 * 7200 bit/s: the sixteen-point trellis constellation, and the only one of
 * the four with no region tree -- it searches all sixteen points.
 *
 * The metric here is SYMMETRIC, `(di*di + dq*dq) >> 16` with the two terms
 * shifted separately, unlike `_4pt`'s deliberately lopsided one (D301).  So
 * the two slicers over the same four quadrants do not agree about what is
 * nearest, and that is in the object, not in this reading.
 *
 * `*mag` is `MAG9600[(|I| + |Q|)/8192 - 1]`.  The sixteen points have
 * |I| + |Q| in {8192, 16384, 24576}, so the index is 0, 1 or 2 and the
 * three-entry table is exactly covered -- which is finding 1603's
 * cross-check: `_16pt` computes the same quantity with `>> 1` instead of
 * `>> 13` and reads 8190 bytes past the table.  That is D302, and `_16pt` is
 * now written with it fixed behind `DSPLIB_REPRODUCE_BUGS`.
 */
unsigned short
FSE_decision_16Tpt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m;
	int i, q, ib, qb, n;
	short k, best = 0;
	short min, sym;

	fse_quality(state);
	m = (struct v32_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	min = 0x7fff;
	for (k = 0; k <= 15; k++) {
		int di = (short)(i - DECv32_IMAP16[k]);
		int dq = (short)(q - DECv32_QMAP16[k]);
		short d = (short)(((di * di) >> 16) + ((dq * dq) >> 16));

		if (d < min) {
			best = k;
			min = d;
		}
	}

	ib = DECv32_IMAP16[best];
	qb = DECv32_QMAP16[best];
	n = ((ib < 0 ? -ib : ib) + (qb < 0 ? -qb : qb)) >> 13;

	*mag = DECv32_MAG9600[n - 1];
	*angle = DECv32_ANGL9600[best];

	VTB_decoder((struct vtb *)m->vtb, (short)i, (short)q, &sym);
	return (unsigned short)sym;
}

/*
 * 12000 bit/s: the sixty-four-point constellation, and the plainest of the
 * three region trees -- a four-by-four grid on the symbol as received, with
 * no rotation, cutting the plane at 0 and +-8192 on each axis and searching
 * the four points of the cell that names.
 *
 * THE TWO AXES DO NOT SPLIT AT THE SAME PLACE, and it is one count, not a
 * misreading.  In I the outer band is `i > 8192` and `i <= -8192`; in Q it is
 * `q > 8191` and `q < -8192`.  So a symbol at exactly (8192, 8192) is treated
 * as inner in I and outer in Q, and one at exactly (-8192, -8192) as outer in
 * I and inner in Q.  Writing either axis's test for both -- the tidy reading
 * -- agrees everywhere except on those two lines; the test drives them
 * explicitly for that reason.
 */
unsigned short
FSE_decision_64pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m;
	int i, q;
	short k, base, best = 0;
	short min, sym;

	fse_quality(state);
	m = (struct v32_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

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
		int di = (short)(i - DECv32_IMAP64[k]);
		int dq = (short)(q - DECv32_QMAP64[k]);
		short d = (short)(((di * di) >> 13) + ((dq * dq) >> 13));

		if (d < min) {
			best = k;
			min = d;
		}
	}

	*mag = DECv32_MAG12000[best];
	*angle = DECv32_ANGL12000[best];

	VTB_decoder((struct vtb *)m->vtb, (short)i, (short)q, &sym);
	return (unsigned short)sym;
}

/*
 * 9600 bit/s: thirty-two points, and the decision is ONE-DIMENSIONAL.
 *
 * After the rotation every symbol is in one octant, where the eight
 * candidates lie on a line and only their Q coordinates differ -- so the
 * search is `|rq - ANA_QMAP[k]|` over at most three of them and there is no
 * `ANA_IMAP` table at all.  The bands of `ri` choose which three: below 5792
 * the first three, above it the second three, and in the far corner (`ri`
 * past 11585 with `rq` below it) only two.
 *
 * THE TIE-BREAK IS THE TWO-DIMENSIONAL PART.  Points 3 and 6 are the two ends
 * of the fold and share a Q band, so when the linear search lands on either
 * the function re-decides between them with a real squared distance -- and
 * that is the only place `_32pt` uses an I coordinate, which is why the two
 * appear as the literals 8689 and 14481 rather than as a table.  Both match
 * `ANA_QMAP` entries, crossed over: point 3 is (8689, 14481) and point 6 is
 * (14481, 8689), which is the mirror in the octant's diagonal.
 */
unsigned short
FSE_decision_32pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m;
	int i, q, rot;
	short ri, rq;
	short k, first, count, best = 0;
	short min, sym, n;

	fse_quality(state);
	m = (struct v32_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

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
		short d = (short)(rq >= DECv32_ANA_QMAP[k]
				  ? rq - DECv32_ANA_QMAP[k]
				  : DECv32_ANA_QMAP[k] - rq);

		if (d < min) {
			best = k;
			min = d;
		}
	}

	if (best == 3 || best == 6) {
		short d3i = (short)(0x21f1 - ri);
		short d3q = (short)(DECv32_ANA_QMAP[3] - rq);
		short d6i = (short)(0x3891 - ri);
		short d6q = (short)(DECv32_ANA_QMAP[6] - rq);
		short e3 = (short)(((d3q * d3q) >> 15) + ((d3i * d3i) >> 15));
		short e6 = (short)(((d6q * d6q) >> 15) + ((d6i * d6i) >> 15));

		/* The NEARER wins, and a tie goes to 3. */
		best = (short)(e6 < e3 ? 6 : 3);
	}

	n = (short)(best + 8 * rot);
	*mag = DECv32_MAG9600T[n];
	*angle = DECv32_ANGL9600T[n];

	VTB_decoder((struct vtb *)m->vtb, (short)i, (short)q, &sym);
	return (unsigned short)sym;
}

/*
 * 14400 bit/s: a hundred and twenty-eight points, folded by the rotation onto
 * the thirty-two of `ANA_{I,Q}MAP128` and cut by a ten-leaf region tree on
 * `ri` and `rq` at 5792, 11585 and 14481.  Eight leaves name a cell of four
 * consecutive points and search it; two more name a cell straddling the
 * diagonal, at 14 and 26, and search four from there.
 *
 * TWO LEAVES DO NOT SEARCH AT ALL.  Where the tree cannot separate the point
 * -- the far diagonal corner, on either side of the line -- the function
 * skips the loop and decides between one specific PAIR of points with a
 * squared distance.  `amb` carries which pair, and its two arms disagree
 * about ties: the outer one takes the FARTHER of the two on a tie (D370) and
 * the inner one the nearer.  Both are reproduced as written.
 *
 * `amb` is `unsigned` because the object shifts it `shrl`, which is a logical
 * shift and so is forced.  Nothing can measure it: the variable only ever
 * holds 0, `FSE128_AMB_OUTER` or `FSE128_AMB_INNER`.  The second test is
 * written as the object encodes it -- a shift, not a mask against the second
 * name -- because `shrl $1` then `test` is what the object does and a mask
 * would move the code generation off it.
 *
 * THE I COORDINATES OF THOSE FOUR POINTS ARE LITERALS AND ONE IS WRONG.
 * Three of them -- 10137, 10137, 13033 -- equal `ANA_IMAP128[14]`, `[15]` and
 * `[24]`; the fourth is 15930 where `ANA_IMAP128[26]` is 15929.  That one-off
 * is what proves they are literals in the source rather than a compiler's
 * fold of a const table, and it is reproduced: D371.
 */

/*
 * The two bits of `amb`, one per cell of the region tree that cannot be
 * separated.  Which cell each is comes straight out of the tree and is not an
 * inference: bit 0 is set only where both rotated coordinates are past 14481,
 * the far corner of the diagonal, and bit 1 only where both are between 11585
 * and 14481, the cell one step in from it.  Local to this function, because
 * the same bit values mean something else everywhere else.
 */
#define FSE128_AMB_OUTER	(1 << 0)
#define FSE128_AMB_INNER	(1 << 1)

unsigned short
FSE_decision_128pt(struct fpm_fse *state, short *angle, short *mag)
{
	struct v32_dec *m;
	int i, q, rot;
	unsigned int amb = 0;
	short ri, rq;
	short k, base = 0, best = 0;
	short min, sym, n;

	fse_quality(state);
	m = (struct v32_dec *)state->cfg.owner;

	i = state->out_i[(short)state->n_out];
	q = state->out_q[(short)state->n_out];

	rot = fse_rotate(i, q, &ri, &rq);

	if (ri > 0x16a0) {
		base = 0x0c;
		if (ri > 0x2d41) {
			if (rq <= 0x2d41)
				base = (short)(rq <= 0x16a0 ? 0x1c : 0x18);
			else if (ri <= 0x3891) {
				if (rq > 0x3891)
					base = 0x0e;
				else
					amb = FSE128_AMB_INNER;
			} else {
				if (rq < 0x3891)
					base = 0x1a;
				else
					amb = FSE128_AMB_OUTER;
			}
		} else if (rq <= 0x2d40) {
			base = (short)(rq <= 0x169f ? 0x14 : 0x10);
		}
	} else if (rq <= 0x2d40) {
		base = (short)(rq <= 0x169f ? 0x08 : 0x04);
	}

	if (amb & FSE128_AMB_OUTER) {
		short d14i = (short)(0x2799 - ri);
		short d14q = (short)(DECv32_ANA_QMAP128[14] - rq);
		short d26i = (short)(0x3e3a - ri);
		short d26q = (short)(DECv32_ANA_QMAP128[26] - rq);
		short e14 = (short)(((d14q * d14q) >> 13)
				    + ((d14i * d14i) >> 13));
		short e26 = (short)(((d26q * d26q) >> 13)
				    + ((d26i * d26i) >> 13));

		/* The FARTHER wins on a tie, which is D370. */
		best = (short)(e26 >= e14 ? 26 : 14);
	} else if (amb >> 1) {
		short d15i = (short)(0x2799 - ri);
		short d15q = (short)(DECv32_ANA_QMAP128[15] - rq);
		short d24i = (short)(0x32e9 - ri);
		short d24q = (short)(DECv32_ANA_QMAP128[24] - rq);
		short e15 = (short)(((d15q * d15q) >> 13)
				    + ((d15i * d15i) >> 13));
		short e24 = (short)(((d24q * d24q) >> 13)
				    + ((d24i * d24i) >> 13));

		/* And here the nearer, with the tie the other way. */
		best = (short)(e24 <= e15 ? 24 : 15);
	} else {
		min = 0x7fff;
		for (k = base; k < base + 4; k++) {
			int di = (short)(ri - DECv32_ANA_IMAP128[k]);
			int dq = (short)(rq - DECv32_ANA_QMAP128[k]);
			short d = (short)(((di * di) >> 13)
					  + ((dq * dq) >> 13));

			if (d < min) {
				best = k;
				min = d;
			}
		}
	}

	n = (short)(best + 32 * rot);
	*mag = DECv32_MAG14400[n];
	*angle = DECv32_ANGL14400[n];

	VTB_decoder((struct vtb *)m->vtb, (short)i, (short)q, &sym);
	return (unsigned short)sym;
}
