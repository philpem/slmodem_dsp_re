/*
 * v8sig.c -- the handshake's staging copies and control entry.
 *
 * The pieces that move samples about: the two staging copies between the
 * rings and the working buffers, and the receive AGC.  None of them decide
 * anything -- they are what the state machine drives.  The tone and
 * phase-reversal detector is `V8Detector.c`'s, and the V.21 FSK modulator,
 * demodulator and shaping filter are `V8Fsk.c`'s, which is where the object
 * keeps them.
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
