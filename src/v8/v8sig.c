/*
 * v8sig.c -- the handshake's signal plumbing.
 *
 * The pieces that move samples about: the two staging copies between the
 * rings and the working buffers, the transmit shaping filter and the V.21
 * FSK demodulator.  None of them decide anything -- they are what the state
 * machine drives.  The tone and phase-reversal detector that used to sit
 * here is `V8Detector.c`'s, which is where the object keeps it.
 */

#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/v8.h"

/*
 * Take four samples out of the symbol buffer and into the receive staging
 * buffer, stepping four bytes at a time -- every other short, so the real
 * half of each complex pair.  The buffer is a ring and wraps at its end.
 */
int
v8_rxreadqueue(struct v8 *v)
{
	short *src = v->tx_sym_a;
	short *dst = v->rx_stage;
	int i;

	v->sym_avail = (short)(v->sym_avail - V8_QUEUE_BLOCK);

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		*dst++ = *src;
		src += 2;
		if (src >= v->tx_symbols + V8_TX_SYMBOLS)
			src = v->tx_symbols;
	}
	v->tx_sym_a = src;
	return 0;
}

/* The other direction: staging buffer into the transmit ring. */
int
v8_txwritequeue(struct v8 *v)
{
	const short *src = v->tx_stage;
	short *dst = v->tx_ring_half;
	int i;

	v->tx_avail = (short)(v->tx_avail + V8_QUEUE_BLOCK);

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		*dst++ = *src++;
		if (dst >= v->tx_ring + V8_TX_RING_END)
			dst = v->tx_ring;
	}
	v->tx_ring_half = dst;
	return 0;
}

/*
 * One sample through the transmit shaping filter: a 61-tap FIR whose delay
 * line is the head of the receive scratch buffer, walked backwards so the
 * shift and the multiply-accumulate happen in one pass.  The 0x8000 the
 * accumulator starts at is the rounding for the final shift.
 */
short
v8_fsktxfilter(struct v8 *v, short sample)
{
	int acc = 0x8000;
	int i;

	v->rx_scratch[0] = sample;

	for (i = 0; i < V8_V21_TAPS; i++) {
		short x = v->rx_scratch[V8_FSK_TAP_TOP - i];

		v->rx_scratch[V8_FSK_TAP_TOP + 1 - i] = x;
		acc += x * v->v21_taps[i];
	}

	return (short)(acc >> 16);
}

/*
 * One step of the receive AGC.
 *
 * A smoothed level estimate, then two nested dead bands: the level has to be
 * more than 2000 away from its target before anything happens at all, and the
 * correction that accumulates from that has to reach 1000 before the gain
 * moves.  The gain then goes down by a factor or up by a smaller one, which
 * is the usual fast-attack slow-release asymmetry.
 *
 * THE WIDTHS BELOW ARE READ OFF THE OBJECT AND NOT CHOSEN.  Every one of them
 * is something the compiler was forced to encode, and with these four
 * declarations the function is byte-identical to the blob's -- 70
 * instructions, operands included, which is 617's acceptance test:
 *
 *   `delta` and `acc` are SHORT because each dead-band test is `test %dx,%dx`
 *   and not `test %edx,%edx`; the width of a test is the width of the value
 *   being tested.
 *
 *   `sum` is the UNTRUNCATED accumulator and is what gets stored back, because
 *   the object stores `%cx` -- the raw sum -- and not the sign-extended `%dx`.
 *   The two hold the same sixteen bits, so no test can tell them apart.
 *
 *   `mag` is a NAMED SHORT TEMPORARY rather than a cast inside the `if`,
 *   because the object sign-extends the difference (`cwtl`) before testing it
 *   and then narrows the test back to `%ax`.  Written as a cast in the
 *   condition, this compiler folds the conversion away and emits two
 *   instructions fewer.
 *
 * None of it changes behaviour: every value here already ranged over a short.
 * Finding F2952.
 */
int
v8_agcadapt(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	int level;
	short delta;
	int sum;
	short acc;
	short mag;

	level = ((r->level * 0x6ccd) >> 15) + (unsigned short)r->energy_hi;

	/*
	 * Accept the new estimate only when it has not run away: the top
	 * bits must be all zero or all one, which is the original's way of
	 * asking whether it still fits in a short.
	 */
	if (((unsigned)level >> 15) == 0 || ((unsigned)level >> 15) == 0x1ffff)
		r->level = (short)level;
	else
		r->level = 0x7f00;

	if (r->flags & V8_RX_DETECTOR_ARMED)
		return 0;

	delta = (short)((unsigned short)r->level - 0xfa0);
	mag = (short)((delta < 0 ? -delta : delta) - 0x7d0);
	if (mag <= 0)
		return 0;

	sum = ((r->adapt_rate * delta) >> 16) + (unsigned short)r->accum;
	acc = (short)sum;

	mag = (short)((acc < 0 ? -acc : acc) - 0x3e8);
	if (mag <= 0) {
		r->accum = (short)sum;
		return 0;
	}
	r->accum = 0;

	if (acc > 0) {
		r->gain = (short)((r->gain * 0x390a) >> 14);
	} else if ((short)(unsigned short)r->gain <= 0x6a00) {
		r->gain = (short)((r->gain * 0x47cf) >> 14);
	}
	return 0;
}

/*
 * Four samples of FSK.  The two carriers differ only in which increment is
 * added to the shared phase, so the branch is one field apart; everything
 * after -- table lookup, amplitude, shaping filter -- is common.
 */
int
v8_fskmodulate(struct v8 *v, short which)
{
	struct v8_v21_params *p = &v->v21_params;
	short step = (short)(which != 0 ? p->carrier_b : p->carrier_a);
	int i;

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		unsigned phase;
		short c;

		phase = ((unsigned)(unsigned short)p->carrier_phase
			 + (unsigned short)step) & 0x1fff;
		p->carrier_phase = (short)phase;

		c = v8_cosread((unsigned char)(phase >> 5));
		v->tx_stage[i] = v8_fsktxfilter(v, v8_mpyint(c, p->tx_level));
	}

	return v8_txwritequeue(v);
}

/*
 * Nudge the handshake from outside.
 *
 * Each request is accepted only from the one state it makes sense in, and
 * refused otherwise -- there is no queueing and no error beyond the return
 * value, so a caller that asks at the wrong moment simply gets -1.
 */
int
V8Control(struct v8 *v, int what)
{
	int rc;

	switch (what) {
	case V8CTRL_START_CM:
		if (v->side != 0 || v->rx_state != 0x19 || v->cm_ready != 0) {
			rc = -1;
		} else {
			v->cm_ready = 1;
			rc = 0;
		}
		break;

	case V8CTRL_START_CJ:
		if (v->rx_substate != V8_HS_TAKEN_RX) {
			rc = -1;
		} else {
			v->rx_substate = V8_HS_DRAIN;
			rc = 0;
		}
		break;

	case V8CTRL_START_JM:
		if (v->rx_substate != V8_HS_TAKEN_TX) {
			rc = -1;
		} else {
			v->rx_substate = V8_HS_CJ;
			v->elapsed = 0;
			v->tx_state = 0x17;
			rc = 0;
		}
		break;

	default:
		/*
		 * The only exit that does not name the request: there is no
		 * name to give it, so it reports the number instead -- and it
		 * is the one message here that ends \r\n.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: V8Control called with currently not "
			    "supported control type (type=%d)\r\n", what);
		return -1;
	}

	/*
	 * Announced whether it was accepted or refused -- the object has two
	 * copies of this, one per return value, sharing the one call.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V8: V8Control called - control type is %s\n",
		    v8ControlName[what]);
	return rc;
}

/* Below this the correlator calls the line silent rather than guessing. */
#define V8_FSK_SILENCE	0xc34f

/* Four decisions the same way make one bit. */
#define V8_FSK_RUN	4

/*
 * Push `n` copies of one bit into the receive accumulator.  The count is a
 * plain running total, not a modulo, and the accumulator is not masked --
 * the caller reads whatever it wants off the bottom.
 */
static void
push_bits(struct v8_v21_params *p, int n, short bit)
{
	while (n-- > 0) {
		p->bits = (short)(((unsigned short)p->bits << 1)
				 | (unsigned short)bit);
		p->bitcount = (short)(p->bitcount + 1);
	}
}

/*
 * Turn a run of like decisions into bits when the decision changes, leaving
 * the remainder behind.  Returns what is left of the run.
 *
 * The count is rounded rather than truncated, and the rounding is not
 * symmetric: a remainder counts as another bit only once it is at least
 * `4 - min(whole + 1, 3)`, so a short run needs three of four to round up
 * while a long one needs only one.  That is what stops a run of three
 * being thrown away at 300 baud.
 */
static int
drain_run(struct v8_v21_params *p, int run, short bit)
{
	int whole = run >> 2;
	int rem = run & 3;
	int need = 4 - (whole > 2 ? 3 : whole + 1);
	int n = whole + 1 - (rem < need);

	if (n != 0)
		push_bits(p, n, bit);
	return rem;
}

/* At the end of a block a leftover run is truncated, not rounded. */
static int
flush_run(struct v8_v21_params *p, int run, short bit)
{
	int whole = run >> 2;

	if (whole != 0)
		push_bits(p, whole, bit);
	return run & 3;
}

void
v8_fskdemodulate(struct v8 *v)
{
	struct v8_v21_params *p = &v->v21_params;
	int limit;
	int i;
	int pos;

	/* Take this block's four samples into the twelve-sample input. */
	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		short idx = p->inbuf_pos;

		p->inbuf_pos = (short)(idx + 1);
		v->v21.inbuf[idx] = v->rx_stage[i];
	}
	if (p->inbuf_pos != V8_V21_INBUF)
		return;
	p->inbuf_pos = 0;

	limit = v->v21.pos;

	for (pos = 0; pos < V8_V21_INBUF; pos++) {
		int a0 = 0x2000, a1 = 0x2000, a2 = 0x2000, a3 = 0x2000;
		int e_mark, e_space;
		int j;
		int top;

		if (limit > pos)
			continue;
		limit += 8;

		top = pos > V8_V21_DELAY - 1 ? V8_V21_DELAY - 1 : pos;

		/* Back through this block's samples... */
		for (j = 0; j <= top; j++) {
			int s = v->v21.inbuf[pos - j];

			a0 += v->v21.a[j] * s;
			a1 += v->v21.b[j] * s;
			a2 += v->v21.c[j] * s;
			a3 += v->v21.d[j] * s;
		}
		/* ...then on into the previous forty. */
		for (j = pos + 1; j <= V8_V21_DELAY - 1; j++) {
			int s = v->v21.delay[V8_V21_DELAY + pos - j];

			a0 += v->v21.a[j] * s;
			a1 += v->v21.b[j] * s;
			a2 += v->v21.c[j] * s;
			a3 += v->v21.d[j] * s;
		}

		a0 = (short)(a0 >> 14);
		a1 = (short)(a1 >> 14);
		a2 = (short)(a2 >> 14);
		a3 = (short)(a3 >> 14);

		e_space = a0 * a0 + a1 * a1;
		e_mark = a2 * a2 + a3 * a3;

		if (e_space <= V8_FSK_SILENCE && e_mark <= V8_FSK_SILENCE) {
			/* Nothing there: forget both runs. */
			v->v21.space_run = 0;
			v->v21.mark_run = 0;
			continue;
		}

		if (e_mark > e_space) {
			if (v->v21.mark_run != 0)
				v->v21.mark_run =
					drain_run(p, v->v21.mark_run, p->mark_bit);
			v->v21.space_run++;
			v->v21.mark_run = 0;
		} else {
			if (v->v21.space_run != 0)
				v->v21.space_run =
					drain_run(p, v->v21.space_run, p->space_bit);
			v->v21.mark_run++;
			v->v21.space_run = 0;
		}
	}

	/* Flush whatever is left of either run. */
	if (v->v21.mark_run > V8_FSK_RUN)
		v->v21.mark_run = flush_run(p, v->v21.mark_run, p->mark_bit);
	if (v->v21.space_run > V8_FSK_RUN)
		v->v21.space_run = flush_run(p, v->v21.space_run, p->space_bit);

	v->v21.pos = limit - V8_V21_INBUF;

	/*
	 * Slide the forty-tap line along by twelve, one step at a time --
	 * which is how the original does it, forty shorts moved twelve times
	 * rather than a single shift.
	 *
	 * Each pass reads one short PAST the end of the line, taking the
	 * first half of the field that follows it into the last tap.  That is
	 * well defined in the object and would be undefined as an array index
	 * here, so it is done as a byte-wise copy from inside the object.
	 */
	for (i = 0; i < V8_V21_INBUF; i++) {
		const char *src = (const char *)v->v21.delay + sizeof(short);
		int k;

		for (k = 0; k < V8_V21_DELAY; k++) {
			short t;

			memcpy(&t, src + k * sizeof(short), sizeof(t));
			v->v21.delay[k] = t;
		}
	}
	/* Then take this block's twelve into the end of it. */
	for (i = 0; i < V8_V21_INBUF; i++)
		v->v21.delay[V8_V21_DELAY - 1 - i] =
			v->v21.inbuf[V8_V21_INBUF - 1 - i];
}
