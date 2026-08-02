/*
 * v34rx.c -- ITU-T V.34: the receiver and transmitter cores.
 *
 * Reconstructed under the fast pass (docs/fastpass.md): read from the
 * disassembly, differential-tested, structural comments only.  Coefficient
 * derivations and defect reachability are owed to task #47.
 *
 * Being written a few functions at a time; V34RX.c and V34TX.c share this
 * file because the object interleaves them and the boundary between the two
 * is not pinned by any local symbol.
 */

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"

/*
 * Advance a ring cursor, wrapping at `end` back to the first entry.
 *
 * The original compares the cursor against a hardcoded end address and
 * reloads it with `q + 0xc` -- the ring's own base -- so the wrap is a
 * pointer test rather than an index test.
 */
static int *
q_next(struct v34_queue *q, int *p, unsigned end)
{
	if ((char *)p >= (char *)q + end)
		return q->ring;
	return p;
}

void
rxreadqueue(struct v34_queue *q)
{
	/* The output sits immediately after the ring. */
	short *out = (short *)((char *)q + V34_RXQ_END);
	int *p = q->rd;
	int i;

	q->count = (short)(q->count - V34_QUEUE_BURST);

	for (i = 0; i < V34_QUEUE_BURST; i++) {
		out[i] = (short)*p;
		p++;
		p = q_next(q, p, V34_RXQ_END);
	}

	q->rd = p;
}

void
txwritequeue(struct v34_queue *q, const short *src)
{
	int *p = q->wr;
	int i;

	q->count = (short)(q->count + V34_QUEUE_BURST);

	for (i = 0; i < V34_QUEUE_BURST; i++) {
		/* Low half the sample, high half explicitly zeroed. */
		((short *)p)[1] = 0;
		((short *)p)[0] = src[i];
		p++;
		p = q_next(q, p, V34_TXQ_END);
	}

	q->wr = p;
}

int
bitreverse(unsigned short v, short nbits)
{
	int out = (v & 1) ? 1 : 0;
	short i;

	/*
	 * Starts at one, not zero: bit 0 is taken before the loop.  The
	 * accumulator is truncated to 16 bits on each shift, so a `nbits`
	 * above 16 silently drops the top of the result.
	 */
	for (i = 1; i < nbits; i = (short)(i + 1)) {
		v = (unsigned short)(v >> 1);
		out = (unsigned short)(out * 2);
		if (v & 1)
			out |= 1;
	}

	return out;
}

void
decision(struct v34_receiver *d, const int *pts, short npts)
{
	const int *best = pts;
	int best_dist = 0x7fff;
	int tx = (unsigned short)d->target_re;
	int ty = (unsigned short)d->target_im;
	short i;

	for (i = 0; i < npts; i++) {
		const int *p = &pts[i];
		int dx = (short)(tx - (unsigned short)*(const short *)p);
		int dy = (short)(ty - (unsigned short)((const short *)p)[1]);
		int dist;

		/*
		 * The sum of squares is shifted LOGICALLY and then truncated
		 * to 16 bits, so a pair far enough apart wraps to a small
		 * distance and can win.  Reproduced.
		 */
		dist = (short)((unsigned)(dx * dx + dy * dy) >> 14);

		if (dist < best_dist) {
			best = p;
			best_dist = dist;
		}
	}

	d->best_index = (short)(best - pts);
	d->dp.point = *best;
}

void
V34nlencoder(const short *in, short *out)
{
	int re = in[0];
	int im = in[1];
	int mag, g, t;

	/* |z|^2, in Q12, then scaled by 341/4096. */
	mag = (re * re + im * im + 0x800) >> 12;
	mag = (short)((mag * 0x155 + 0x800) >> 12);

	/* A cubic correction: mag + (mag^2 * 19661 >> 16), offset by 1.0. */
	t = (short)((mag * mag + 0x2000) >> 14);
	g = (short)(mag + ((t * 0x4ccd) >> 16) + 0x4000);

	/* And the gain itself, Q14. */
	g = (g * 0x3b17) >> 14;

	out[0] = (short)((re * g) >> 14);
	out[1] = (short)((im * g) >> 14);
}

void
updateAlpha(short *alpha, int energy, int apply_decay, int gain, int decay,
	    const char *tag)
{
	/* Read at entry, so the message reports the value before the update. */
	short was = *alpha;

	if (energy != 0) {
		int shift = 0;
		int r;

		/*
		 * Normalise `energy` up until bit 30 is set, counting the
		 * shifts.  The counter is truncated to 16 bits on every
		 * iteration -- `cwtl` sits inside the loop -- which cannot
		 * bite for any input that terminates, since bit 30 is
		 * reached in at most 31 steps.
		 */
		while ((energy & 0x40000000) == 0) {
			shift++;
			shift = (short)shift;
			energy += energy;
		}

		/* A reciprocal: (1 << (shift + 21)) / (normalised >> 16). */
		r = (short)((1 << (shift + 0x15))
			    / ((energy + 0x8000) >> 16));

		/*
		 * A negative quotient means the divide overflowed; the
		 * original substitutes a fixed 0x2000 rather than clamping.
		 */
		if (r < 0)
			r = 0x2000;

		*alpha = (short)(-((r * gain + 0x2000) >> 15));

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"updateAlpha%s: updated %d => %d\n", tag,
				(int)was, (int)*alpha);
	}

	if (apply_decay != 0)
		*alpha = (short)((*alpha * decay + 0x4000) >> 15);
}

int
V34descrambler(struct v34_receiver *s, short bits, short nbits)
{
	unsigned sr = s->scrambler_sr;
	unsigned mask = 1;
	int out = 0;
	short i;
	int tap = (s->flags & V34_SCR_ANSWERER) ? 18 : 5;

	if (nbits <= 0)
		return 0;

	for (i = 0; i < nbits; i = (short)(i + 1)) {
		unsigned in = ((unsigned)(unsigned short)bits & mask) ? 1u : 0u;
		unsigned bit;

		bit = ((sr >> 23) & 1) ^ in;
		bit ^= (sr >> tap) & 1;

		/*
		 * The register takes the input bit at position 0 and is then
		 * shifted up, so the bit lands at position 1 rather than 0 --
		 * which is why the taps read as 5/18 and 23 rather than the
		 * recommendation's 6/19 and 24.
		 */
		sr = (sr + in) * 2;

		if (bit)
			out = (short)(out | (int)mask);

		mask = (unsigned short)(mask * 2);
	}

	s->scrambler_sr = sr;
	return out;
}

void
txinit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->f3550 = 0;
	obj->f3552 = 0;
	obj->f25cc = 0;
	obj->f25c6 = 0;
	obj->f25c0 = 0;

	V34EchoCleanUp(&obj->echo0);
	V34EchoCleanUp(&obj->echo1);

	/*
	 * The transmit queue is PRIMED, not emptied: the write cursor starts
	 * 32 entries ahead of the read cursor and the count says so, giving
	 * the modulator a full block of silence to draw on before the first
	 * symbol arrives.
	 */
	obj->txq.count = 0x20;
	obj->txq.rd = obj->txq.ring;
	obj->txq.wr = obj->txq.ring + 0x20;
	sysdep_memset(obj->txq.ring, 0, V34_TXQ_RING * sizeof(int));

	/* The receive queue is emptied outright. */
	obj->rxq.count = 0;
	obj->rxq.rd = obj->rxq.ring;
	obj->rxq.wr = obj->rxq.ring;
	sysdep_memset(obj->rxq.ring, 0, V34_RXQ_RING * sizeof(int));

	/* And the pre-filter's 42-tap history. */
	sysdep_memset(obj->prefilter.state, 0,
		      V34_ECHO_PREFILTER_TAPS * sizeof(short));
}

/*
 * The AGC's square-root table: 192 entries, Q15 in and Q15 out, at
 * .rodata+0x2860.  Covers mantissas in [0.25, 1), which is what normalising
 * to bit 30 and halving on an odd exponent leaves.
 *
 *     v34_sqrt_table[i] = floor(sqrt((i + 0x40) / 256) * 32768)
 *
 * Exact for all 192 entries -- TRUNCATED, not rounded, which is worth
 * stating because rounding misses 98 of them by one.  Emitted as data all
 * the same: the generator is a claim about intent, the bytes are the
 * reference.
 */
static const unsigned short v34_sqrt_table[192] = {
	0x4000, 0x407f, 0x40fe, 0x417b, 0x41f8, 0x4273, 0x42ee, 0x4368,
	0x43e1, 0x445a, 0x44d1, 0x4548, 0x45be, 0x4633, 0x46a7, 0x471b,
	0x478d, 0x4800, 0x4871, 0x48e2, 0x4952, 0x49c1, 0x4a30, 0x4a9e,
	0x4b0b, 0x4b78, 0x4be5, 0x4c50, 0x4cbb, 0x4d26, 0x4d90, 0x4df9,
	0x4e62, 0x4eca, 0x4f32, 0x4f99, 0x5000, 0x5066, 0x50cb, 0x5130,
	0x5195, 0x51f9, 0x525d, 0x52c0, 0x5323, 0x5385, 0x53e7, 0x5449,
	0x54a9, 0x550a, 0x556a, 0x55ca, 0x5629, 0x5688, 0x56e6, 0x5745,
	0x57a2, 0x5800, 0x585c, 0x58b9, 0x5915, 0x5971, 0x59cc, 0x5a27,
	0x5a82, 0x5adc, 0x5b36, 0x5b90, 0x5be9, 0x5c42, 0x5c9b, 0x5cf3,
	0x5d4b, 0x5da3, 0x5dfa, 0x5e51, 0x5ea8, 0x5efe, 0x5f54, 0x5faa,
	0x6000, 0x6055, 0x60aa, 0x60fe, 0x6152, 0x61a7, 0x61fa, 0x624e,
	0x62a1, 0x62f4, 0x6347, 0x6399, 0x63eb, 0x643d, 0x648e, 0x64e0,
	0x6531, 0x6582, 0x65d2, 0x6623, 0x6673, 0x66c3, 0x6712, 0x6761,
	0x67b1, 0x6800, 0x684e, 0x689d, 0x68eb, 0x6939, 0x6986, 0x69d4,
	0x6a21, 0x6a6e, 0x6abb, 0x6b08, 0x6b54, 0x6ba1, 0x6bed, 0x6c38,
	0x6c84, 0x6ccf, 0x6d1a, 0x6d65, 0x6db0, 0x6dfb, 0x6e45, 0x6e8f,
	0x6ed9, 0x6f23, 0x6f6d, 0x6fb6, 0x7000, 0x7049, 0x7091, 0x70da,
	0x7123, 0x716b, 0x71b3, 0x71fb, 0x7243, 0x728a, 0x72d2, 0x7319,
	0x7360, 0x73a7, 0x73ee, 0x7434, 0x747b, 0x74c1, 0x7507, 0x754d,
	0x7593, 0x75d8, 0x761e, 0x7663, 0x76a8, 0x76ed, 0x7732, 0x7777,
	0x77bb, 0x7800, 0x7844, 0x7888, 0x78cc, 0x790f, 0x7953, 0x7996,
	0x79da, 0x7a1d, 0x7a60, 0x7aa3, 0x7ae5, 0x7b28, 0x7b6b, 0x7bad,
	0x7bef, 0x7c31, 0x7c73, 0x7cb5, 0x7cf6, 0x7d38, 0x7d79, 0x7dba,
	0x7dfb, 0x7e3c, 0x7e7d, 0x7ebe, 0x7efe, 0x7f3f, 0x7f7f, 0x7fbf,
};

/*
 * ---------------------------------------------------------------------------
 * The AGC's measurement chain.
 *
 * The object carries this code THREE times: once in `agcadapt`, which is a
 * real global; once inlined in `V34agc`; and once inlined in `V34demodulate`.
 * They were almost certainly one set of functions in the original -- the
 * instruction sequences match one for one -- so they are one set here, with
 * the two places the copies genuinely differ passed in as arguments rather
 * than duplicated.  The differences are real and neither is a slip:
 *
 *   - V34demodulate rounds before the Q10 gain shift (`lea 0x200(%ecx)`);
 *     V34agc truncates (`sar $0xa` with nothing added).
 *   - the two overflow messages name their own function.
 *
 * Keeping one copy is also the only way to keep them honest: the reason this
 * function was reconstructed six times is that a hand-transcribed second copy
 * of a 40-line fixed-point chain is indistinguishable from a correct one
 * until a differential test disagrees.
 */

/*
 * Apply the current gain to one sample, saturating rather than wrapping.
 *
 * In range when the top seven bits of the product are all zero or all one,
 * which is the object's own spelling of "still fits after the Q10 shift".
 * Out of range it is replaced by a rail at +/-0x7f00 -- note that the
 * negative rail is 0x8100, one count short of a symmetric -0x7f00... which
 * it is, exactly: 0x8100 == -32512 == -0x7f00.  The asymmetric-looking
 * constant is just the two's-complement spelling.
 */
static int
agc_gain_sample(struct v34_receiver *rx, int x, int round, const char *fmt)
{
	int g = (int)rx->agc_gain * x;
	unsigned top = (unsigned)g >> 25;
	int over;

	if (top == 0 || top == 0x7f)
		return (short)((g + round) >> 10);

	/*
	 * V34agc tests this 16 bits wide and V34demodulate 32; with both
	 * operands 16-bit the product cannot leave [-2^30, 2^30], so the
	 * shifted value cannot leave a short and the two always agree.
	 */
	over = g >> 16;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(fmt, over);

	return over > 0 ? 0x7f00 : (short)0x8100;
}

/*
 * The 36-sample RMS, and the gate it feeds.
 *
 * Worth being clear about what this returns and what uses it: the square
 * root is computed in full -- normalise, halve the mantissa on an odd
 * exponent, look up `sqrt_table`, shift back -- and then the ONLY thing
 * either caller does with it is compare it against 31.  The quantity that
 * actually drives the loop is the energy sum, not this.  So the whole
 * square root exists to answer "is there enough signal to adapt on?".
 *
 * 0x38e is 910, and 910/32768 is 1/36.008 -- the reciprocal of the window
 * length, so the accumulator is a mean square rather than a sum.
 */
static int
agc_rms(const short *buf)
{
	int acc = 0;
	int i;
	unsigned shift = 0;
	unsigned mant;
	unsigned v;

	for (i = 0; i < V34_AGC_RMS_TAPS; i++)
		acc += ((buf[i] * V34_AGC_RMS_SCALE) >> 15) * buf[i];

	if (acc == 0)
		return 0;

	/* Normalise into the top three bits, counting the shifts. */
	v = (unsigned)acc;
	if (v <= 0x1fffffffu) {
		do {
			v += v;
			shift++;
		} while (v <= 0x1fffffffu);
	}
	mant = v >> 15;

	/* An odd exponent is carried as a halved mantissa, not a half-shift. */
	if (shift != ((shift >> 1) * 2))
		mant = (unsigned short)mant >> 1;

	/*
	 * The index is formed as a signed round-and-bias and then compared
	 * UNSIGNED, so a mantissa small enough to make it negative wraps to
	 * a huge value and clamps to the top of the table rather than the
	 * bottom.  The normalisation above keeps `mant` >= 0x2000, which is
	 * exactly the value that makes the biased index zero, so the wrap is
	 * unreachable from here -- but it is the code that is written.
	 */
	{
		unsigned idx = (unsigned short)((((unsigned short)mant + 0x40)
						 >> 7) - 0x40);

		if (idx > 0xbf)
			idx = 0xbf;

		return (short)(unsigned short)(v34_sqrt_table[idx]
					       >> ((shift >> 1) & 31));
	}
}

int
agcadapt(struct v34_receiver *a)
{
	int level;
	int err;
	int acc;

	/* Smooth: 0.85 of the old level plus the new measurement. */
	level = ((a->agc_level * V34_AGC_SMOOTH) >> 15) + a->energy.h.agc_input;

	/*
	 * Range check, spelled as the original does it: valid when the top
	 * 17 bits are all zero or all one, i.e. when the sum still fits a
	 * signed short.  Anything else is replaced by 0x7f00 rather than
	 * clamped to the rail -- and the clamped value then goes on to be
	 * used, since the check falls through rather than returning.
	 */
	if (((unsigned)level >> 15) == 0 || ((unsigned)level >> 15) == 0x1ffff)
		a->agc_level = (short)level;
	else
		a->agc_level = 0x7f00;

	if (a->flags & V34_RX_FLAG_AGC_FREEZE)
		return 0;

	/* How far off target, and is it outside the deadband? */
	err = (short)((unsigned short)a->agc_level - V34_AGC_TARGET);
	if ((short)((err < 0 ? -err : err) - V34_AGC_DEADBAND) <= 0)
		return 0;

	/* Integrate the error, and check that against its own deadband. */
	acc = (short)(((a->agc_step * err) >> 16) + (unsigned short)a->agc_accum);
	if ((short)((acc < 0 ? -acc : acc) - V34_AGC_ACCUM_LIMIT) <= 0) {
		a->agc_accum = (short)acc;
		return 0;
	}

	/* Tripped: reset the integrator and move the gain one step. */
	a->agc_accum = 0;

	if (acc > 0) {
		a->agc_gain = (short)((a->agc_gain * V34_AGC_GAIN_DOWN) >> 14);
	} else if ((short)(unsigned short)a->agc_gain <= V34_AGC_GAIN_CEILING) {
		/*
		 * Up and down are not inverses: 0.883 * 1.122 is 0.9907, so
		 * a signal that oscillates about the target drifts downward.
		 */
		a->agc_gain = (short)((a->agc_gain * V34_AGC_GAIN_UP) >> 14);
	}

	return 0;
}

void
rxtiminginit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);

	V34TimingFiltersInit((struct v34_timing *)((char *)obj + 0x50c));

	/* Where rxreadqueue leaves its four samples. */
	rx->rx_samples = (short *)((char *)obj + 0x370);

	rx->f124 = 0;
	rx->f1b8 = 1;
	rx->f1bc = 0;
	rx->f1c0 = -1;
	rx->f1c8 = 1;
	rx->f1cc = 0;
	rx->f1ce = 0;
	rx->f1d0 = 0;
	/* The slowest V.34 rate: what the receiver assumes until told. */
	rx->baud = 2400;
	rx->f1d4 = 0;
	rx->f1d8 = 0;
	rx->f1e0 = 0;
	rx->f1e4 = 0;
	rx->f1e8 = 0;
	rx->f1f0 = 0;
	rx->f208 = 0;
	rx->f20a = 0;
	rx->dp.point = 0;
	rx->f22e = 0;
	rx->f230 = 0;
	rx->f244 = 0;
	rx->f246 = 0;
}

void
rxinit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);

	rx->agc_gain = 0x200;
	rx->agc_step = 0x3333;
	rx->f1b8 = 1;

	V34EqualizerCleanUp((struct v34_equalizer *)((char *)obj + 0x630));

	/*
	 * The fill is %ecx, which held 1 before V34EqualizerCleanUp and is
	 * caller-saved -- so what reaches the memset is whatever that call
	 * left behind, which is zero.
	 */
	sysdep_memset((char *)obj + 0x4ec, 0, 0xc);
	sysdep_memset((char *)obj + 0x4f8, 0, 0x10);

	V34InitHilbertFilter((short *)((char *)obj + 0xa1b8));

	/*
	 * 0x4000 goes to f218 and f1f2 ONLY.  THREE registers are loaded and
	 * all three are zeroed before their second use:
	 *
	 *     mov $0x4000,%ecx ; mov $0x4000,%edx ; xor %eax,%eax
	 *     mov %cx,0x218    ; xor %ecx,%ecx
	 *     mov %dx,0x1f2    ; xor %edx,%edx
	 *     mov %ax,0x138    ; mov %cx,0x134
	 *
	 * so agc_accum, agc_level and f1f4 all get zero.  Four stores, two
	 * values, and the pairing is not the one the instruction order
	 * suggests at a glance -- which is exactly how this was read wrong
	 * the first time.  See finding 122.
	 */
	rx->f218 = 0x4000;
	rx->f1f2 = 0x4000;
	rx->agc_accum = 0;
	rx->agc_level = 0;
	rx->f1f4 = 0;

	if (rx->flags & 0x0008) {
		rx->f200 = 2;
		rx->f202 = 10;		/* and f1f8 is left alone */
	} else {
		rx->f1f8 = 0;
		rx->f200 = 2;
		rx->f202 = 8;
	}

	rx->f798 = 0;   rx->f21c = 0;  rx->f206 = 0;  rx->f21a = 0;
	rx->f204 = 0;   rx->f224 = 0;  rx->f220 = 0;  rx->f228 = 0;
	rx->f1a0 = 0;   rx->f246 = 0;  rx->f124 = 0;  rx->scrambler_sr = 0;
	rx->f244 = 0;   rx->f1bc = 0;  rx->energy.sum = 0;
	rx->f120 = 0;   rx->f12a = 0;  obj->f2aa4 = 0;
	rx->rx_samples = (short *)((char *)rx + 0x10c);
	rx->f248 = 0;   rx->f24c = 0;
}

/* The bulk ring's wrap: reset to zero, not subtract.  See finding 116. */
static int
bulk_next(int idx, int len)
{
	idx++;
	return idx & -(int)((unsigned)len > (unsigned)idx);
}

void
txmit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_queue *txq = &obj->txq;
	short local[38];
	int sym, n, i;

	sym = (int)(((unsigned)(unsigned short)obj->f25d2 << 16)
		    | (unsigned short)obj->f25d0);
	n = (short)V34ModulatorProcess(
		(struct v34_modulator *)((char *)obj + 0x1450), sym, local);

	if (n > 0) {
		int *wr = txq->wr;

		for (i = 0; i < n; i++) {
			int v = (local[i] * obj->f25d4 + 0x2000) >> 14;

			/*
			 * One per sample.  txwritequeue adds four per call to
			 * the same field; two producers, two conventions.
			 */
			txq->count = (short)(txq->count + 1);
			((short *)wr)[0] = (short)v;
			((short *)wr)[1] = 0;
			wr++;
			if ((char *)wr >= (char *)obj + 0x25c0)
				wr = txq->ring;
		}
		txq->wr = wr;
	}

	V34EchoPreFilter(local, (short)n, &obj->prefilter);

	/* Bit 9 of the short at +0x25c2; the original tests byte 0x25c3 for 2. */
	if ((obj->f25c2 & 0x0200) == 0)
		return;

	for (i = 0; i < n; i++) {
		short *ring = obj->bulk_ring;
		int len = obj->bulk_len;
		int delayed = ring[obj->bulk_head];

		obj->bulk_head = bulk_next(obj->bulk_head, len);
		ring[obj->bulk_tail] = local[i];
		obj->bulk_tail = bulk_next(obj->bulk_tail, len);

		/* Near end takes the current sample, far end the delayed one. */
		V34EchoUpdateDelayLine(&obj->echo0, (short)(local[i] >> 1));
		V34EchoUpdateDelayLine(&obj->echo1, (short)(delayed >> 1));
	}
}

/*
 * ---------------------------------------------------------------------------
 * V34agc -- gain a burst and adapt, with no timing recovery.
 *
 * The handshake runs this while it is still listening for tones, before the
 * timing loop has anything to lock to.  It is the same chain V34demodulate
 * runs, in the same order, over four samples instead of two:
 *
 *     read a burst          -> +0x10c
 *     record it for the RMS -> rms_buf, UNGAINED
 *     apply the gain        -> in place at +0x10c, saturating
 *     measure and adapt     -> agcadapt
 *
 * The gain applied is the one adapted on the PREVIOUS call: agcadapt runs
 * last, so a correction always takes effect one burst later.  That ordering
 * is load-bearing and is the thing this reconstruction got wrong before.
 */
void
V34agc(struct v34_receiver *rx)
{
	short *buf = (short *)((char *)rx + V34_RXQ_END);
	int i;
	int sum;

	rxreadqueue((struct v34_queue *)rx);

	/* Where the gained burst will be, for whoever consumes it next. */
	rx->rx_samples = buf + V34_QUEUE_BURST;

	/*
	 * The energy window is fed the RAW samples, before the gain -- so it
	 * measures the line, not the AGC's own output, and the loop it closes
	 * is therefore feed-forward rather than feedback.
	 */
	for (i = 0; i < V34_QUEUE_BURST; i++) {
		short idx = rx->f19c;

		rx->f19c = (short)(idx + 1);
		rx->rms_buf[idx] = buf[i];
		if ((short)(idx + 1) > V34_AGC_RMS_TAPS - 1)
			rx->f19c = 0;
	}

	for (i = 0; i < V34_QUEUE_BURST; i++)
		buf[i] = (short)agc_gain_sample(rx, buf[i], 0,
						"V34AGC, overflow = 0x%x,\n");

	if (rx->flags & V34_RX_FLAG_AGC_FREEZE)
		return;

	if (agc_rms(rx->rms_buf) <= V34_AGC_RMS_FLOOR)
		return;

	/* The gained burst's energy is what the loop actually integrates. */
	sum = 0;
	for (i = 0; i < V34_QUEUE_BURST; i++)
		sum += (int)buf[i] * buf[i];

	rx->energy.sum = sum;
	agcadapt(rx);
}

/*
 * ---------------------------------------------------------------------------
 * V34demodulate -- gain one sample pair, adapt, and mix it down to baseband.
 *
 * Called once per half-baud by rxtiming, and never by anything else: this is
 * a file-static in the object and the interpolator is its only caller.
 *
 * The order is the whole of it, and it is not the order a fresh design would
 * choose.  The gain applied to THIS pair is the one adapted on a PREVIOUS
 * call -- the AGC runs after both samples have already been scaled -- so a
 * correction always lands one pair late.  Writing it the other way round is
 * self-consistent, passes a smoke test, and diverges from the object on the
 * first sample that trips the loop.
 */
static void
V34demodulate(struct v34_receiver *rx)
{
	struct v34_queue *q = (struct v34_queue *)rx;
	static const char over[] = "V34demodulate, agc overflow = 0x%x,\n";
	const short *in = (const short *)q->rd;
	short *out = rx->rx_samples;
	int s0 = in[0];
	int s1 = in[1];
	short idx;
	short count;
	int g0, g1;
	int sum;
	int cos_v, sin_v;
	int phase, quarter, step;

	/* One int off the ring, so one pair of shorts. */
	q->count = (short)(q->count - 1);
	q->rd = q_next(q, q->rd + 1, V34_RXQ_END);

	/* Only the first of the pair joins the energy window, ungained. */
	idx = rx->f19c;
	rx->rms_buf[idx] = (short)s0;
	rx->f19c = (short)(idx + 1);
	if ((short)(idx + 1) > V34_AGC_RMS_TAPS - 1)
		rx->f19c = 0;

	/*
	 * Both samples are gained; only the first is kept.  The second goes
	 * straight into the mixer below -- `rx_samples` advances by one short
	 * per call, not two.
	 */
	g0 = agc_gain_sample(rx, s0, 0x200, over);
	rx->rx_samples = out + 1;
	out[0] = (short)g0;
	g1 = agc_gain_sample(rx, s1, 0x200, over);

	/*
	 * The AGC runs on every fourth pair.  `f12a` counts them and the
	 * energy sum accumulates across them; both are cleared whenever the
	 * loop is given a chance to adapt, whether or not it actually did.
	 */
	count = (short)((unsigned short)rx->f12a + 1);
	sum = rx->energy.sum + g0 * g0;

	if (count <= 3) {
		rx->f12a = count;
		rx->energy.sum = sum;
	} else if (rx->flags & V34_RX_FLAG_AGC_FREEZE) {
		rx->f12a = 0;
		rx->energy.sum = 0;
	} else {
		rx->f12a = count;
		if (agc_rms(rx->rms_buf) > V34_AGC_RMS_FLOOR) {
			/* agcadapt's input is the high half of this. */
			rx->energy.sum = sum;
			agcadapt(rx);
		}
		rx->f12a = 0;
		rx->energy.sum = 0;
	}

	/*
	 * Down-mix.  One table holds both phases: the carrier is read at the
	 * running index and again `f1ba` further on, which is a quarter cycle,
	 * so the second read is the sine of the first.  The index wraps by
	 * subtracting f1ba rather than masking.
	 */
	phase = (unsigned short)rx->f1bc;
	quarter = (unsigned short)rx->f1ba;
	step = (unsigned short)rx->f1b8;

	cos_v = rx->carrier[(short)phase];
	sin_v = rx->carrier[(short)phase + (short)quarter];

	rx->f240 = (short)((g0 * sin_v + g1 * cos_v + 0x2000) >> 14);
	rx->f242 = (short)((g1 * sin_v - g0 * cos_v + 0x2000) >> 14);

	phase += step;
	if ((short)quarter <= (short)phase)
		phase -= quarter;
	rx->f1bc = (short)phase;
}

/*
 * The timing loop's resonator, run on the freshly demodulated pair.
 *
 * A second-order section whose input is scaled by 1/16 (`<< 10` against a
 * Q14 accumulator) and whose poles sit just inside the unit circle --
 * 1.4001 and -0.9801 -- so it rings at the baud rate rather than filtering.
 * Its state doubles as the interpolator's endpoint: `f240`/`f242` are both
 * the output and the next interpolation's far end.
 *
 * `store_prev` is the one difference between the object's two copies of
 * this block.  See rxtiming.
 */
static void
rx_iir(struct v34_receiver *rx, int store_prev)
{
	int acc;
	short prev;

	prev = rx->f208;
	acc = (((int)rx->f240 << 10) + (int)prev * V34_RXTIMING_IIR_A1
	       + (int)rx->dp.iir2.i * V34_RXTIMING_IIR_A2) >> 14;
	rx->dp.iir2.i = prev;
	rx->f240 = (short)acc;
	rx->f208 = (short)acc;
	if (store_prev)
		rx->f244 = (short)acc;

	prev = rx->f20a;
	acc = (((int)rx->f242 << 10) + (int)prev * V34_RXTIMING_IIR_A1
	       + (int)rx->dp.iir2.q * V34_RXTIMING_IIR_A2) >> 14;
	rx->dp.iir2.q = prev;
	rx->f242 = (short)acc;
	rx->f20a = (short)acc;
	if (store_prev)
		rx->f246 = (short)acc;
}

/*
 * ---------------------------------------------------------------------------
 * rxtiming -- resample the demodulated signal onto the recovered clock.
 *
 * Produces `f128` timing-error estimates into `timing_out[]`.  Each one is
 * the magnitude of a point interpolated between the previous demodulated
 * symbol (`f244`/`f246`) and the current one (`f240`/`f242`), at the
 * fractional phase `f1ac`, passed through the timing high-pass.
 *
 * THE PART THAT TOOK SEVEN READINGS.  The phase advances by `f1ae` per
 * output and wraps at `f1b0`; a wrap means the interpolator has run past the
 * current symbol and must pull a new one.  The object handles exactly one or
 * two wraps, on two distinct paths that converge:
 *
 *   - one wrap:  copy f240/f242 down to f244/f246 -- the old current symbol
 *                becomes the new previous one -- then pull ONE symbol.
 *   - two wraps: subtract twice and pull TWO, the first of which writes
 *                f244/f246 from its own resonator output rather than from a
 *                copy.  That is the `store_prev` argument to rx_iir.
 *
 * Three or more wraps are not handled: the phase would still exceed f1b0 on
 * exit.  With f1ae < f1b0 that cannot arise, so it is a bound on the caller
 * rather than a defect -- worth confirming when `receiver` is reconstructed.
 *
 * Both paths jump to the same second pull, which is why a control-flow
 * partition reports one loop here and why reading it as a `while` is so
 * tempting.  It is a loop in the graph and a two-way branch in the source.
 */
void
rxtiming(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	struct v34_timing *t = (struct v34_timing *)((char *)obj + 0x50c);
	short i;

	/* Set before the count is even tested, so it lands on an empty call. */
	rx->rx_samples = (short *)((char *)obj + 0x370);

	for (i = 0; i < rx->f128; i = (short)(i + 1)) {
		int wa = (unsigned short)rx->f1ac;
		int wb = (short)((unsigned short)rx->f1b0
				 - (unsigned short)rx->f1ac);
		int wrap = (short)rx->f1b0;
		int I, Q, m, pos;

		/* Linear interpolation between the two symbols, both axes. */
		I = (short)((rx->f240 * wa + rx->f244 * wb + 0x2000) >> 14);
		Q = (short)((rx->f242 * wa + rx->f246 * wb + 0x2000) >> 14);
		m = (short)((I * I + Q * Q + 0x2000) >> 14);

		rx->timing_out[i] = (short)V34TimingHPFilter(t, (short)m);

		pos = (unsigned short)rx->f1ac + (unsigned short)rx->f1ae;

		if ((int)(unsigned short)pos < wrap) {
			rx->f1ac = (short)pos;
			continue;
		}

		pos -= (unsigned short)rx->f1b0;

		if ((int)(unsigned short)pos < wrap) {
			/* One wrap: the current symbol becomes the previous. */
			rx->f1ac = (short)pos;
			rx->f244 = rx->f240;
			rx->f246 = rx->f242;
		} else {
			/* Two: the first pull supplies the previous symbol. */
			pos -= (unsigned short)rx->f1b0;
			rx->f1ac = (short)pos;
			V34demodulate(rx);
			rx_iir(rx, 1);
		}

		V34demodulate(rx);
		rx_iir(rx, 0);
	}
}

/*
 * ---------------------------------------------------------------------------
 * txrxdmainit -- build the twelve-short coefficient block from three pairs.
 *
 * `src` is read from +0x4, as three complex coefficients packed (re, im).
 * `dst` gets each of them twice, in the two arrangements a fixed-point
 * complex multiply needs:
 *
 *     dst[0..5]    (re, -im) for each pair -- the conjugates
 *     dst[6..11]   (im,  re) for each pair -- real and imaginary swapped
 *
 * So a caller wanting `a * conj(c)` dots against the first half and `a * c`
 * against the second, without either having to negate or swap at run time.
 * The original spells all twelve stores out; the negations reload the source
 * rather than reusing the register they just negated, which is why each
 * source short is read twice.
 */
void
txrxdmainit(short *dst, const short *src)
{
	int i;

	for (i = 0; i < 3; i++) {
		int re = (unsigned short)src[2 + i * 2];
		int im = (unsigned short)src[3 + i * 2];

		dst[i * 2] = (short)re;
		dst[i * 2 + 1] = (short)-im;
		dst[6 + i * 2] = (short)im;
		dst[6 + i * 2 + 1] = (short)re;
	}
}

/*
 * ---------------------------------------------------------------------------
 * v34FreezeEcho -- stop both cancellers adapting.
 *
 * Sets bit 2 of f25c2 and then dumps both cancellers' coefficients.  The
 * dump is the whole reason the debug hooks are carried (see debug.h): with
 * `dsplibs_debug_level` at its shipped zero this function is three stores
 * and two calls, and the message names -- "Near" and "Far" -- are what fix
 * which of echo0 and echo1 is which.
 */
void
v34FreezeEcho(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34HSHAK: Freeze EC\n");

	obj->f25c2 = (short)(obj->f25c2 | V34_EC_FROZEN);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"==== Near Echo Canceller report ======\n");
	V34EchoReportCoeff(&obj->echo0);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"==== Far Echo Canceller report ======\n");
	V34EchoReportCoeff(&obj->echo1);
}

/*
 * ---------------------------------------------------------------------------
 * V34scrambler -- the transmit side of V34descrambler.
 *
 * Same two generators and the same register, run the other way round: the
 * scrambled bit is fed back into the register, where the descrambler feeds
 * back the bit it received.  `mode` selects the generator directly here
 * rather than through the flags word.
 *
 * THE REGISTER SHIFTS RIGHT, which is what makes the tap positions look
 * wrong.  A new bit is OR-ed in at bit 31 and the register is then shifted,
 * so it lands at bit 30 and the bit generated `m` steps ago sits at 30 - m:
 *
 *     bit 26  ->  m = 4   ->  five steps back   ->  x^-5
 *     bit 13  ->  m = 17  ->  eighteen back     ->  x^-18
 *     bit  8  ->  m = 22  ->  twenty-three back ->  x^-23
 *
 * giving 1 + x^-5 + x^-23 for the caller and 1 + x^-18 + x^-23 for the
 * answerer, exactly as V.34 4.2 specifies.  V34descrambler reaches the same
 * two polynomials with taps at 5/18 and 23 because it shifts the other way;
 * see finding 110, which is the same offset trap from the other side.
 *
 * The three-way XOR is spelled as a running increment and a parity test,
 * not as `^`, so a tap that fires twice cancels the same way.
 */
int
V34scrambler(unsigned *sr, short mode, short bits, short nbits)
{
	int mask = (short)((int)((unsigned)1 << ((int)nbits & 31)) - 1);
	unsigned reg = *sr;
	short i;

	/*
	 * The two variants differ only in the second tap, but the original
	 * emits the loop twice rather than testing per bit; the branch is
	 * hoisted out.  Kept as one loop with the tap chosen up front, which
	 * computes the same thing without duplicating the body.
	 */
	unsigned tap = mode ? 0x00002000u : 0x04000000u;

	for (i = 0; i < nbits; i = (short)(i + 1)) {
		int parity = (bits & 1) ? 1 : 0;

		/* Arithmetic, so a negative `bits` feeds ones for ever. */
		bits = (short)(bits >> 1);

		if (reg & tap)
			parity = (short)(parity + 1);
		if (reg & 0x00000100u)
			parity = (short)(parity + 1);

		if (parity & 1)
			reg |= 0x80000000u;

		reg >>= 1;
	}

	/* Written once, after the loop -- and not at all when nbits <= 0. */
	if (nbits > 0)
		*sr = reg;

	/*
	 * The newest bit sits at 30, so shifting down by 31 - nbits leaves
	 * the run of them at the bottom, oldest first -- the same order the
	 * input was consumed in.
	 */
	return (short)((reg >> ((0x1f - (int)nbits) & 31)) & (unsigned)mask);
}

/*
 * ---------------------------------------------------------------------------
 * V34SetupDemodulator -- point the receiver at one of the six symbol rates
 * and one of the eight carriers.
 *
 * Two independent lookups, both spelled as compare chains in the original.
 * Neither has a default: an unrecognised rate leaves the timing constants
 * alone and an unrecognised carrier leaves the table pointer alone, so a bad
 * argument keeps whatever the previous call installed rather than failing.
 *
 * THE TIMING CONSTANTS.  `f1b0` is the interpolator's wrap and `f1ae` its
 * step, so `f1ae / f1b0` is the ratio between the symbol rate and the sample
 * rate; `f1be` keeps the step as configured, since the timing loop slews the
 * live one; `f1ac` starts the phase at half a step.  2400 baud is the
 * degenerate case where step equals wrap -- one output per input, no
 * resampling -- and every other rate interpolates down from it.
 *
 * 2800 is the odd one out: 0x3e82 and 0x1f41 where every other rate uses
 * 0x3e80 and 0x1f40.  Two counts on the wrap, one on the initial phase.  A
 * wrap of 0x3e82 makes 2800's ratio 0x3594/0x3e82 rather than 0x3594/0x3e80,
 * which is a closer rational fit to 2800/9600 -- so it reads as deliberate
 * rather than as a typo, but the derivation is owed to task #47.
 *
 * THE CARRIER TABLES are each exactly twice `f1ba` shorts long, which is
 * what makes V34demodulate's `carrier[i]` and `carrier[i + f1ba]` a cosine
 * and its sine: the second half is the first shifted a quarter cycle.  That
 * relation holds for all eight and is the reason f1ba is stored at all.
 */
void
V34SetupDemodulator(void *objp, short baud, short carrier)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34SetupDemodulator: baudrate %ld, carrier %ld\n",
			(long)baud, (long)carrier);

	/* Four outputs per call, whatever the rate. */
	rx->f128 = 4;

	switch (baud) {
	case 2400:
		rx->f1b0 = 0x3e80; rx->f1ae = 0x3e80;
		rx->f1be = 0x3e80; rx->f1ac = 0x1f40;
		break;
	case 2743:
		rx->f1ae = 0x36b0; rx->f1b0 = 0x3e80;
		rx->f1be = 0x36b0; rx->f1ac = 0x1f40;
		break;
	case 2800:
		rx->f1b0 = 0x3e82; rx->f1ae = 0x3594;
		rx->f1be = 0x3594; rx->f1ac = 0x1f41;
		break;
	case 3000:
		rx->f1b0 = 0x3e80; rx->f1ae = 0x3200;
		rx->f1be = 0x3200; rx->f1ac = 0x1f40;
		break;
	case 3200:
		rx->f1ae = 0x2ee0; rx->f1b0 = 0x3e80;
		rx->f1be = 0x2ee0; rx->f1ac = 0x1f40;
		break;
	case 3429:
		rx->f1b0 = 0x3e80; rx->f1ae = 0x2bc0;
		rx->f1be = 0x2bc0; rx->f1ac = 0x1f40;
		break;
	default:
		break;
	}

	switch (carrier) {
	case 1600: rx->carrier = hsine1600; rx->f1ba = 6;    break;
	case 1680: rx->carrier = hsine1680; rx->f1ba = 0x28; break;
	case 1800: rx->carrier = hsine1800; rx->f1ba = 0x10; break;
	case 1829: rx->carrier = hsine1829; rx->f1ba = 0x15; break;
	case 1867: rx->carrier = hsine1867; rx->f1ba = 0x24; break;
	case 1920: rx->carrier = hsine1920; rx->f1ba = 5;    break;
	case 1959: rx->carrier = hsine1959; rx->f1ba = 0x31; break;
	case 2000: rx->carrier = hsine2000; rx->f1ba = 0x18; break;
	default:   break;
	}
}
