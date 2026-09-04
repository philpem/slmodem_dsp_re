/*
 * v8handshak.c -- the handshake state machine.
 *
 * Two state variables, one per direction, which is why a single function
 * covers what looks like it should be two:
 *
 *   `f9d4` drives the transmitter and is dispatched inside a loop that runs
 *   until the transmit queue is full, so one call does as much work as the
 *   queue has room for.
 *
 *   `f9d6` drives the receiver and is dispatched once, after that loop ends
 *   and only if at least six symbols have arrived.
 *
 * The return value is 0 normally, 1 when a deadline expired, and 2 when the
 * handshake finished -- which is what `V8Process` reads as "something
 * changed".
 */

#include "dsplib/debug.h"
#include "dsplib/v8.h"

/*
 * The two long receive paths are in v8hsrx.c: the one that waits for the AGC
 * and the tone, and the one that demodulates and matches.  Split out because
 * between them they are most of this function's four kilobytes and neither
 * shares anything with the transmit side but the object.
 */

/* Transmit states, as `f9d4` holds them. */
#define V8_TX_SILENCE	5
#define V8_TX_ANSAM	6
#define V8_TX_FSK_TIMED	23
#define V8_TX_FSK	43
#define V8_TX_TONE	45

/* Receive states, as `f9d6` holds them. */
#define V8_RX_AGC	0x19
#define V8_RX_SETTLE	0x20
#define V8_RX_DRAIN	0x23
#define V8_RX_DEMOD	0x28
#define V8_RX_DONE	0x63

/* How long each receive state waits before giving up on it. */
#define V8_RX_AGC_BLOCKS	0x960
#define V8_RX_SETTLE_BLOCKS	0x258

/* Bits per character on the wire, and how many make a full CM. */
#define V8_HS_CM_BITS		0x3c

/*
 * Send the next four samples of whatever this state transmits.  Returns 0 to
 * carry on, or a value to return from the handshake.
 */
static int
transmit(struct v8 *v, int *done)
{
	struct v8_v21_params *p = &v->v21_params;
	int i;

	*done = 0;

	switch ((short)v->tx_state) {
	case V8_TX_SILENCE:
		for (i = 0; i < V8_QUEUE_BLOCK; i++)
			v->tx_stage[i] = 0;
		v8_txwritequeue(v);
		return 0;

	case V8_TX_TONE:
		v8_TONEq_generate(v, v->tx_stage);
		v8_txwritequeue(v);
		return 0;

	case V8_TX_ANSAM:
		if (v->deadline_a != -1 && v->elapsed >= v->deadline_a) {
			/*
			 * The equal case counts this block, the past-it case
			 * does not -- and the announcement sits inside it, so
			 * the timeout is reported exactly once however many
			 * blocks arrive afterwards.  That is what the odd
			 * count-once idiom is FOR; without the call site it
			 * reads as a pointless conditional increment.
			 */
			if (v->elapsed == v->deadline_a) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: Time Out Waiting For "
					    "CM...\r\n");
				v->elapsed++;
			}
			v->rx_state = 4;
			*done = 1;
			return 1;
		}
		v8_ansamgenerate(v, v->tx_stage);
		v8_txwritequeue(v);
		v->elapsed++;
		return 0;

	case V8_TX_FSK_TIMED:
		if (v->deadline_b != -1 && v->elapsed >= v->deadline_b) {
			v->rx_state = v->side == 1 ? 5 : 0xc;
			/*
			 * Announced once, as above.  Which message was being
			 * waited for follows the side: the answerer is waiting
			 * for the caller's CJ, the caller for the answerer's
			 * JM.
			 */
			if (v->elapsed == v->deadline_b) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: Timeout waiting for %s "
					    "message...\r\n",
					    v->side == 1 ? "CJ" : "JM");
				v->elapsed++;
			}
			*done = 1;
			return 1;
		}
		v8_fskmodulate(v, v->tx_bit);
		p->sample_count = (short)(p->sample_count + 4);
		if (p->samples_per_bit == p->sample_count) {
			v->tx_bit = (short)v8_getbit(v->tx_seq);
			p->sample_count = 0;
		}
		v->elapsed++;
		return 0;

	case V8_TX_FSK:
		v8_fskmodulate(v, v->tx_bit);
		p->sample_count = (short)(p->sample_count + 4);
		if (p->samples_per_bit == p->sample_count) {
			v->tx_bit = (short)v8_getbit(v->tx_seq);
			v->cm_bit_count = (short)(v->cm_bit_count + 1);
			if (v->cm_bit_count == V8_HS_CM_BITS) {
				/*
				 * A whole CM has gone out; switch to the
				 * timed state and to the other buffer.
				 */
				v->tx_state = V8_TX_FSK_TIMED;
				v->tx_seq = &v->seq[0];
			}
			p->sample_count = 0;
		}
		return 0;

	default:
		/* No such state: wait for the queue to drain. */
		return 0;
	}
}

int
v8handshak(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	int done = 0;
	int rc;

	/*
	 * Transmit until the queue is full.  The comparison is signed, and
	 * `f21c` does go negative -- `V8Process` decrements it once a sample
	 * whatever the queue is doing -- so this keeps transmitting where an
	 * unsigned one would stop.
	 */
	while ((short)v->tx_avail < (short)v->tx_fill_target) {
		int st = (short)v->tx_state - 5;

		if ((unsigned)st > 0x28)
			continue;
		rc = transmit(v, &done);
		if (done)
			return rc;
	}

	/* Then the receiver, once, and only with something to look at. */
	if ((short)v->sym_avail <= 5)
		return 0;

	switch ((short)v->rx_state) {
	case V8_RX_DRAIN:
		v8_rxreadqueue(v);
		return 0;

	case V8_RX_DONE:
		return 2;

	case V8_RX_SETTLE:
		V8agc(v);
		v->block_count = (short)(v->block_count + 1);
		if ((short)v->block_count <= V8_RX_SETTLE_BLOCKS)
			return 0;
		v->rx_state = V8_RX_DEMOD;
		v->rx_substate = V8_HS_HUNT;
		r->adapt_rate = 0x800;
		v->block_count = 0;
		return 0;

	case V8_RX_AGC:
		return v8_handshak_agc(v);

	case V8_RX_DEMOD:
		return v8_handshak_demod(v);

	default:
		return 0;
	}
}
