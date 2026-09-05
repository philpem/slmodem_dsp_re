/*
 * v8sig.c -- the handshake's signal plumbing.
 *
 * The pieces that move samples about: the tone generator's phase
 * accumulator, the two staging copies between the rings and the working
 * buffers, and the transmit shaping filter.  None of them decide anything --
 * they are what the state machine drives.
 */

#include <string.h>

#include "dsplib/debug.h"
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
	v->tone.mod_phase = 0;
	v->tone.carrier_step = 0x1a;
	v->tone.mod_step = 0xe00;
	v->tone.reversal_count = 0;
	v->tone.carrier_phase = 0;
	v->tone.amplitude = v8_mpyint(0x3e80, v->tx_gain);
	v->tone.reversal_enable = 1;
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

		phase = ((unsigned)(unsigned short)p->carrier_phase
			 + (unsigned short)step) & 0x1fff;
		p->carrier_phase = (short)phase;

		c = v8_cosread((unsigned char)(phase >> 5));
		v->tx_stage[i] = v8_fsktxfilter(v, v8_mpyint(c, p->tx_level));
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

		carrier = ((unsigned)(unsigned short)t->carrier_phase
			   + (unsigned short)t->carrier_step) & 0x3fff;
		t->carrier_phase = (short)carrier;

		modulator = ((unsigned)(unsigned short)t->mod_phase
			     + (unsigned short)t->mod_step) & 0x3fff;
		t->mod_phase = (short)modulator;

		depth = v8_mpyint(V8_ANSAM_DEPTH,
				  v8_cosread((unsigned char)((carrier + 0x20)
							     >> 6)));
		level = v8_mpyint((short)(depth + V8_ANSAM_UNITY), t->amplitude);

		out[i] = v8_fsktxfilter(v,
			v8_mpyint(v8_cosread((unsigned char)((t->mod_phase + 0x20)
							     >> 6)), level));
	}

	if (t->reversal_enable == 0)
		return;

	if ((unsigned short)(t->reversal_count + 1) == V8_ANSAM_REVERSAL) {
		t->reversal_count = 0;
		t->amplitude = (short)-t->amplitude;
	} else {
		t->reversal_count = (short)(t->reversal_count + 1);
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

/*
 * Look for ANSam's phase reversals.
 *
 * Each new sample is correlated against the one half a window back.  While
 * the carrier's phase is steady that product stays positive; when it inverts
 * the product goes sharply negative, and the run of samples since the last
 * such event is what gets measured.  The comparison is against a smoothed
 * energy rather than a fixed threshold, so it holds at whatever level the
 * AGC settles on.
 */
void
v8_phase_rev_detect(struct v8_phase_rev *pr, const short *in, short count)
{
	int corr = pr->corr;
	int energy = pr->energy;
	int smoothed = pr->smoothed;
	short widx = pr->widx;
	int half = pr->half;
	int full = half * 2;
	int spacing = 0;
	short i;

	for (i = 0; i < count; i++) {
		int back;
		int x, old, diff;
		int level;

		/* Advance the write cursor, wrapping at the window's end. */
		widx = (short)(widx + 1);
		if (widx >= full)
			widx = 0;

		back = widx - half;
		if ((short)back < 0)
			back = (short)(back + full);

		x = in[i];
		old = pr->window[widx];
		diff = x - old;

		/* The window's energy, one sample in and one sample out. */
		energy += (x * x + 0x8000) >> 16;
		energy -= (old * old + 0x8000) >> 16;

		corr += (pr->window[back] * diff + 0x8000) >> 16;

		level = smoothed * 0x7fe2 + (energy * 15) * 2;
		smoothed = level >> 15;

		if (corr * 2 < (level >> 17)) {
			/* A reversal, if the run since the last one is long
			 * enough to be one rather than noise. */
			if ((short)pr->run > (short)(pr->half * 4)) {
				spacing = ((short)pr->run * 0xd55) >> 15;
				pr->run = 0;
				pr->reversals = (short)(pr->reversals + 1);
			} else {
				pr->run = (short)(pr->run + 1);
			}
		} else {
			pr->run = (short)(pr->run + 1);
		}

		if ((unsigned)(spacing - V8_PHASE_REV_MIN) <= V8_PHASE_REV_SPAN
		    && (short)pr->reversals > 1) {
			/*
			 * The author's word for the spacing is "delay", and
			 * this is the only place it is named.
			 */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "ANSAM phase reversals detected " "delay = %d\n", spacing);
			pr->detected = 1;
		}

		pr->window[widx] = (short)x;
	}

	pr->widx = widx;
	pr->corr = corr;
	pr->energy = energy;
	pr->smoothed = smoothed;
}

/*
 * The fixed input biquad every tone detector shares, in Q14.  The originals
 * are called `a` and `b` -- local symbols of V8Detector.c, at .rodata+0x5724
 * and +0x572a -- and both are three entries: `a[0]` is 0x4000, the implicit
 * 1.0, and every reader skips it.  Kept two entries here because that is
 * what the code uses.
 */
static const short tone_in_b[3] = { 15565, -8057, 15565 };
static const short tone_in_a[2] = { -8057, 14787 };

/*
 * One biquad, direct form I, with the histories kept as four shorts: x1, x2
 * then y1, y2.  The original writes them back in a fixed order that matters,
 * because x2 takes the old x1 and y2 the old y1.
 */
static int
biquad(short *x, short *y, const short *b, const short *a, int in)
{
	int acc = in;

	acc += v8_mpyint(x[0], b[0]);
	acc += v8_mpyint(x[1], b[1]);
	acc -= v8_mpyint(y[0], a[0]);
	acc -= v8_mpyint(y[1], a[1]);

	x[1] = x[0];
	y[1] = y[0];
	x[0] = (short)in;
	y[0] = (short)acc;

	return acc;
}

/*
 * The same two sections again, standing on their own.
 *
 * Nothing in the object calls either of them.  The compiler inlined copies
 * into `v8_tone_detect` -- the same coefficients at .rodata+0x5726, the same
 * histories -- and left the out-of-line originals behind, so these are what
 * that code was written from.  Reconstructed because they are in the
 * translation unit, not because anything reaches them.
 *
 * They are not quite the inlined code, either: each product is truncated to
 * a short before it is accumulated here, and `v8_tone_detect` accumulates
 * the full result.  That is visible in the object as a `cwtl` after every
 * call, and it is why these cannot just call the helpers above.
 */
short
notch_filter(const short *in, struct v8_detector *d)
{
	int acc = 0;
	int i;

	d->acc_c[0] = *in;
	for (i = 0; i < 3; i++)
		acc += (short)v8_mpyint(d->acc_c[i], tone_in_b[i]);
	for (i = 0; i < 2; i++)
		acc -= (short)v8_mpyint(d->acc_d[i], tone_in_a[i]);

	d->acc_c[2] = d->acc_c[1];
	d->acc_d[2] = d->acc_d[1];
	d->acc_c[1] = d->acc_c[0];
	d->acc_d[1] = d->acc_d[0];
	d->acc_d[0] = (short)acc;
	return (short)acc;
}

short
biquad_filter(short in, struct v8_detector *d, const short *coeff)
{
	int acc = in >> 4;
	int stage1;
	/*
	 * `unsigned short`, AND THE OBJECT IS WHAT DECIDES IT.  Both history
	 * heads are read at four sites whose 32-bit result is discarded by a
	 * 16-bit store, so the extension is F614's dead one -- and lever 8
	 * measured that a dead extension follows the DECLARED TYPE OF THE LOCAL
	 * being loaded into, not the field's, not the store's and not a cast's.
	 * The blob loads all four `movzwl`; `short` here gave `movswl`.
	 *
	 * Ten cells, two distinct emissions: {short, unsigned short, int,
	 * unsigned int} x {`x0, y0`, `y0, x0`, one declaration each}, and
	 * `unsigned short` is the ONLY one of the four that emits the object's
	 * encoding -- an exhausted domain with a unique preimage on the type.
	 * It is value-preserving: both are read from `short` fields and stored
	 * straight back to `short` fields, so every store truncates.
	 *
	 * It does not close the symbol and is not expected to: 222 bytes
	 * against the blob's 224 either way, and the residual two are one
	 * `lea (%eax,%ebx,1),%ebx` where we emit `add %eax,%ebx`.  This is
	 * 7803's shape -- a named declaration property recovered on its own
	 * axis, which makes what is left legible.
	 */
	unsigned short x0, y0;
	int i;

	for (i = 0; i < 2; i++) {
		acc += (short)v8_mpyint(d->acc_a[i], coeff[i]);
		acc -= (short)v8_mpyint(d->acc_b[i], coeff[4 + i]);
	}

	/* Old before new, both histories. */
	y0 = d->acc_b[0];
	x0 = d->acc_a[0];
	d->acc_b[0] = (short)acc;
	d->acc_a[0] = (short)(in >> 4);
	d->acc_b[1] = y0;
	d->acc_a[1] = x0;

	stage1 = (short)(acc >> 4);
	acc = stage1;
	for (i = 0; i < 2; i++) {
		acc += (short)v8_mpyint(d->acc_a[2 + i], coeff[2 + i]);
		acc -= (short)v8_mpyint(d->acc_b[2 + i], coeff[6 + i]);
	}

	y0 = d->acc_b[2];
	x0 = d->acc_a[2];
	d->acc_b[2] = (short)acc;
	d->acc_a[2] = (short)stage1;
	d->acc_b[3] = y0;
	d->acc_a[3] = x0;
	return (short)acc;
}

int
v8_tone_detect(struct v8 *v, struct v8_detector *d, short *in)
{
	while (in < v->rx.buf) {
		int acc;
		int stage1;
		int stage2;
		int i;

		/* The fixed input section, over its own three-deep history. */
		d->acc_c[0] = *in;
		acc = 0;
		for (i = 0; i < 3; i++)
			acc += v8_mpyint(d->acc_c[i], tone_in_b[i]);
		for (i = 0; i < 2; i++)
			acc -= v8_mpyint(d->acc_d[i], tone_in_a[i]);

		d->acc_c[2] = d->acc_c[1];
		d->acc_d[2] = d->acc_d[1];
		d->acc_c[1] = d->acc_c[0];
		d->acc_d[1] = d->acc_d[0];
		d->acc_d[0] = (short)acc;

		*in++ = (short)acc;

		/* Then the detector's own two sections. */
		stage1 = biquad(&d->acc_a[0], &d->acc_b[0], &d->table[0],
				&d->table[4], (short)acc >> 4);
		stage2 = biquad(&d->acc_a[2], &d->acc_b[2], &d->table[2],
				&d->table[6], (short)stage1 >> 4);

		d->integrator = (short)(v8_absfn((short)((short)stage2 >> 4))
				 + v8_mpyint(d->integrator, 0x3ccd));
	}

	if (d->lo_rule != 0) {
		if ((short)d->integrator < d->lo_thresh)
			d->counter = (short)(d->counter + 1);
		if ((short)d->integrator > d->hi_thresh)
			d->counter = 0;
		return d->counter > d->count_limit;
	}

	if (d->armed != 0) {
		if ((short)d->integrator > d->hi_thresh)
			d->counter = (short)(d->counter + 1);
		else
			d->counter = 0;
		return d->counter > d->count_limit;
	}

	/*
	 * Not armed yet: wait for the level to stay up for 0x33 blocks, then
	 * switch to the second rule and take the detector out of the
	 * receiver's flag word.
	 */
	if ((short)d->integrator <= 0x30) {
		d->warmup = 0;
		return 0;
	}
	d->warmup = (short)(d->warmup + 1);
	if (d->warmup == 0x33) {
		d->armed = 1;
		v->rx.flags &= (unsigned short)~V8_RX_DETECTOR_ARMED;
		d->counter = 0;
	}
	return 0;
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
