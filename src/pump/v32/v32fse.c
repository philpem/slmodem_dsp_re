/*
 * v32fse.c -- V.32's slicers for the library equaliser.
 *
 * Reconstructed from dsplibs.o:
 *   FSE_decision_AB   .text 0x081260   678
 *   FSE_decision_4pt  .text 0x081080   468
 *   FSE_decision_trn  .text 0x081590   225
 *   FSE_decision_CD   .text 0x081510   122
 *
 * The five that are not here are `_16pt` (finding 1603: it indexes
 * `DECv32_MAG9600` out of bounds and cannot be reproduced across builds) and
 * `_16Tpt`, `_32pt`, `_64pt` and `_128pt`, which call `VTB_decoder`.
 *
 * THE SEQUENCE THEY IMPLEMENT.  Each slicer installs its own successor in
 * `state->cfg.decision`, so the chain is the receiver's handshake:
 *
 *     AB  --(the two-tone phase alternation stops)-->  CD
 *     CD  --(after 15 symbols)-->                      trn
 *     trn --(after 1280 symbols)-->                    4pt
 *
 * and the datapump switches to a data-rate slicer from there.
 */

#include "dsplib/v32dec.h"

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
 * The differential half of the decision, shared by `_4pt` and `_AB` (and by
 * `_16pt`, which is not reconstructed -- finding 1603).
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
