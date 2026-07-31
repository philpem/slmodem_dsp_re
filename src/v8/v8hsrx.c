/*
 * v8hsrx.c -- the handshake's two long receive paths.
 *
 * Split out of v8handshak.c because between them they are most of that
 * function's four kilobytes and neither shares anything with the transmit
 * side except the object.
 *
 * `v8_handshak_agc` waits for the line to settle and then for a tone.
 * `v8_handshak_demod` turns the demodulator's bits into characters and
 * decides when a whole message has arrived.
 */

#include "dsplib/v8.h"

/* How long each wait runs before it gives up. */
#define V8_AGC_BLOCKS		0x960

/* Consecutive zero bits that end a character. */
#define V8_HS_ZERO_RUN		6

/* Bits a character must be at least this long to count. */
#define V8_HS_MIN_ONES		9

int
v8_handshak_agc(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	short scratch[V8_QUEUE_BLOCK];
	int i;

	V8agc(v);

	if (v->f9d8 == 0x19) {
		/*
		 * Listening for the answer tone.  The deadline is checked
		 * first, and -1 means there is not one.
		 */
		if (v->deadline_a != -1 && v->fe64 >= v->deadline_a) {
			if (v->fe64 == v->deadline_a)
				v->fe64++;
			v->f9d6 = 0xb;
			return 1;
		}
		v->fe64++;
		if (v8_tone_detect(v, &v->detector, v->rx_stage) != 0) {
			v->f9d8 = 0x24;
			v->fdb6 = 0;
			v->fe64 = 0;
			r->flags &= (unsigned short)~V8_RX_DETECTOR_ARMED;
		}
		return 0;
	}

	/*
	 * Still settling.  The rectified block feeds the DFT once the gain
	 * has stopped moving; the phase-reversal detector sees the block
	 * either way.
	 */
	for (i = 0; i < V8_QUEUE_BLOCK; i++)
		scratch[i] = v8_absfn(v->rx_stage[i]);

	checkSignalStability(v);
	if (r->f8a != 0)
		v8_dftupdate((struct v8_dft_bin *)&v->fd94, 1, scratch,
			     V8_QUEUE_BLOCK);

	v8_phase_rev_detect(&v->phase_rev, v->rx_stage, V8_QUEUE_BLOCK);

	v->fdb6 = (short)(v->fdb6 + 1);
	if ((short)v->fdb6 <= V8_AGC_BLOCKS)
		return 0;

	r->flags |= V8_RX_DETECTOR_ARMED;
	if (r->f8a != 0)
		v8_dftenergy((struct v8_dft_bin *)&v->fd94, 1, 1);

	/*
	 * Time is up.  What happens next depends on whether the far end
	 * asked for something this end can offer.
	 */
	if ((unsigned short)v->fda0 > 0x18f || v->phase_rev.detected != 0) {
		if (((v->cm->b2 >> 4) & 1 & (short)v->fdd0) != 0) {
			v->f9d4 = 0x2d;
			return 0;
		}
		if (v->fdbe == 0)
			return 0;

		/* Turn round: answer on the other channel. */
		v8_V21_Init(v, 0, 1);
		r->f20 = 0x800;
		v->fa3c = 1;
		v->fdb6 = 0;
		v->fdb4 = 0;
		v->f9d6 = 0x28;
		v->f9d8 = 0x29;
		v->f9d4 = (v->cm->b2 & 0x10) ? 0x2b : 0x17;
		return 0;
	}

	if (((v->cm->b2 >> 4) & 1 & (short)v->fdd0) == 0)
		return 1;
	if (v->f9d4 == 0x2d) {
		v->f9d4 = 5;
		v->f9d6 = 0x63;
		return 2;
	}
	v->f9d4 = 5;
	return 1;
}

int
v8_handshak_demod(struct v8 *v)
{
	struct v8_v21_params *p = &v->v21_params;
	int before;
	int got;
	int k;

	V8agc(v);
	before = (short)p->f18;
	v8_fskdemodulate(v);
	got = (short)p->f18 - before;

	/*
	 * Walk the bits that just arrived, most recent last.  A one extends
	 * the current character; six zeros in a row end it, and a character
	 * of at least nine ones counts as received.
	 */
	for (k = 0; k < got; k++) {
		int shift = got - k - 1;

		if (((unsigned short)p->f1a >> shift) & 1) {
			p->f1e = 0;
			p->f20 = (short)(p->f20 + 1);
			p->f22 = p->f20;
			continue;
		}
		p->f1e = (short)(p->f1e + 1);
		if (p->f1e == V8_HS_ZERO_RUN
		    && (short)p->f22 > V8_HS_MIN_ONES) {
			p->f26 = p->f24;
			p->f24 = (short)(p->f24 + 1);
		}
		p->f20 = 0;
	}

	return 0;
}
