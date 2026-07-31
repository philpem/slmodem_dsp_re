/*
 * v8sig.c -- the handshake's signal plumbing.
 *
 * The pieces that move samples about: the tone generator's phase
 * accumulator, the two staging copies between the rings and the working
 * buffers, and the transmit shaping filter.  None of them decide anything --
 * they are what the state machine drives.
 */

#include "dsplib/v8.h"

/*
 * Arm the ANSam tone generator.
 *
 * These are the same seven stores `v8handshakinit` makes inline in its
 * answering shape; the compiler inlined this function there rather than
 * calling it.  Kept as a function because that is what the object says it
 * is, and because the constants belong in one place.
 */
void
v8_ansaminit(struct v8 *v)
{
	v->tone.f02 = 0;
	v->tone.f04 = 0x1a;
	v->tone.f06 = 0xe00;
	v->tone.f0a = 0;
	v->tone.f00 = 0;
	v->tone.f08 = v8_mpyint(0x3e80, v->fa42);
	v->tone.f0e = 1;
}

/*
 * Four samples of the queued tone.  A 14-bit phase accumulator stepped by
 * the period, read out of the cosine table with the usual rounding -- the
 * same idiom as the dialler's DTMF, at a different width.
 */
void
v8_TONEq_generate(struct v8 *v, short *out)
{
	int i;

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		unsigned phase = (unsigned)(unsigned short)v->toneq_pending
				 + (unsigned short)v->toneq_period;

		phase &= 0x3fff;
		v->toneq_pending = (short)phase;
		out[i] = v8_cosread((unsigned char)((phase + 0x20) >> 6));
	}
}

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

	v->f110 = (short)(v->f110 - V8_QUEUE_BLOCK);

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

	v->f21c = (short)(v->f21c + V8_QUEUE_BLOCK);

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
 * Advance a sliding DFT.  One oscillator per bin rather than a transform over
 * a block: each bin steps its own phase, reads cosine and sine out of the one
 * table a quarter cycle apart, and adds the products into its running sums.
 */
void
v8_dftupdate(struct v8_dft_bin *bins, short nbins, const short *samples,
	     short nsamples)
{
	short j;

	for (j = 0; j < nsamples; j++) {
		short i;

		for (i = 0; i < nbins; i++) {
			struct v8_dft_bin *b = &bins[i];
			unsigned phase;
			unsigned idx;
			int x = samples[j];

			phase = ((unsigned)(unsigned short)b->phase
				 + (unsigned short)b->step) & 0x3fff;
			b->phase = (short)phase;

			idx = phase >> 6;
			b->re += (v8_cosread((unsigned char)idx) * x) >> 6;
			b->im += (v8_cosread((unsigned char)(idx + 0x40)) * x)
				 >> 6;
		}
	}
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
	short step = which != 0 ? p->carrier_b : p->carrier_a;
	int i;

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		unsigned phase;
		short c;

		phase = ((unsigned)(unsigned short)p->f00
			 + (unsigned short)step) & 0x1fff;
		p->f00 = (short)phase;

		c = v8_cosread((unsigned char)(phase >> 5));
		v->tx_stage[i] = v8_fsktxfilter(v, v8_mpyint(c, p->f0a));
	}

	return v8_txwritequeue(v);
}

/*
 * One step of the receive AGC.
 *
 * A smoothed level estimate, then two nested dead bands: the level has to be
 * more than 2000 away from its target before anything happens at all, and the
 * correction that accumulates from that has to reach 1000 before the gain
 * moves.  The gain then goes down by a factor or up by a smaller one, which
 * is the usual fast-attack slow-release asymmetry.
 */
int
v8_agcadapt(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	int level;
	int delta;
	int acc;

	level = ((r->f1a * 0x6ccd) >> 15) + (unsigned short)r->f16;

	/*
	 * Accept the new estimate only when it has not run away: the top
	 * bits must be all zero or all one, which is the original's way of
	 * asking whether it still fits in a short.
	 */
	if (((unsigned)level >> 15) == 0 || ((unsigned)level >> 15) == 0x1ffff)
		r->f1a = (short)level;
	else
		r->f1a = 0x7f00;

	if (r->flags & V8_RX_DETECTOR_ARMED)
		return 0;

	delta = (short)((unsigned short)r->f1a - 0xfa0);
	if ((short)((delta < 0 ? -delta : delta) - 0x7d0) <= 0)
		return 0;

	acc = ((r->f20 * delta) >> 16) + (unsigned short)r->f1e;
	acc = (short)acc;

	if ((short)((acc < 0 ? -acc : acc) - 0x3e8) <= 0) {
		r->f1e = (short)acc;
		return 0;
	}
	r->f1e = 0;

	if (acc > 0) {
		r->f1c = (short)((r->f1c * 0x390a) >> 14);
	} else if ((short)(unsigned short)r->f1c <= 0x6a00) {
		r->f1c = (short)((r->f1c * 0x47cf) >> 14);
	}
	return 0;
}

/*
 * Four samples of ANSam.
 *
 * Two phase accumulators: the carrier, and a slower one that modulates its
 * amplitude by five percent either way.  The amplitude itself is negated
 * every 1080 blocks, and that inversion is the whole point -- it is what
 * tells a listening modem this is ANSam and not a bare answer tone.
 *
 * The reversal counter only runs while the enable at +0x0e is set, so a
 * caller can have the tone without the reversals.
 */
void
v8_ansamgenerate(struct v8 *v, short *out)
{
	struct v8_tone *t = &v->tone;
	int i;

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		unsigned carrier;
		unsigned modulator;
		short depth;
		short level;

		carrier = ((unsigned)(unsigned short)t->f00
			   + (unsigned short)t->f04) & 0x3fff;
		t->f00 = (short)carrier;

		modulator = ((unsigned)(unsigned short)t->f02
			     + (unsigned short)t->f06) & 0x3fff;
		t->f02 = (short)modulator;

		depth = v8_mpyint(V8_ANSAM_DEPTH,
				  v8_cosread((unsigned char)((carrier + 0x20)
							     >> 6)));
		level = v8_mpyint((short)(depth + V8_ANSAM_UNITY), t->f08);

		out[i] = v8_fsktxfilter(v,
			v8_mpyint(v8_cosread((unsigned char)((t->f02 + 0x20)
							     >> 6)), level));
	}

	if (t->f0e == 0)
		return;

	if ((unsigned short)(t->f0a + 1) == V8_ANSAM_REVERSAL) {
		t->f0a = 0;
		t->f08 = (short)-t->f08;
	} else {
		t->f0a = (short)(t->f0a + 1);
	}
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
	switch (what) {
	case V8_CONTROL_START:
		if (v->mode != 0 || v->f9d6 != 0x19 || v->fdbe != 0)
			return -1;
		v->fdbe = 1;
		return 0;

	case V8_CONTROL_ANSWER:
		if (v->f9d8 != 0x32)
			return -1;
		v->f9d8 = 0x23;
		return 0;

	case V8_CONTROL_PROCEED:
		if (v->f9d8 != 0x33)
			return -1;
		v->f9d8 = 0x2a;
		v->fe64 = 0;
		v->f9d4 = 0x17;
		return 0;

	default:
		return -1;
	}
}
