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
#include "dsplib/v34det.h"	/* costbl: the receiver's carrier NCO */
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"
#include "dsplib/v34shell.h"
#include "dsplib/v34pcmif.h"

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

	obj->echo_alpha = 0;
	obj->short_3552 = 0;
	obj->tx_scr_sr = 0;
	obj->prev_quadrant = 0;
	obj->seg_symcount = 0;

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

	rx->rx_blocks = 0;
	rx->mix_carrier_step = 1;
	rx->mix_carrier_phase = 0;
	rx->pllcnt = -1;
	rx->slow_ramp = 1;
	rx->ppm_acc = 0;
	rx->ppm_count = 0;
	rx->timing_offset = 0;
	/* The slowest V.34 rate: what the receiver assumes until told. */
	rx->report_interval = 2400;
	rx->f1d4 = 0;
	rx->f1d8 = 0;
	rx->timing_integrator = 0;
	rx->f1e4 = 0;
	rx->f1e8 = 0;
	rx->f1f0 = 0;
	rx->f208 = 0;
	rx->f20a = 0;
	rx->dp.point = 0;
	rx->f22e = 0;
	rx->dwell_count = 0;
	rx->demod_i_prev = 0;
	rx->demod_q_prev = 0;
}

void
rxinit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);

	rx->agc_gain = 0x200;
	rx->agc_step = 0x3333;
	rx->mix_carrier_step = 1;

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
	 * 0x4000 goes to equ_step and cloop_cos ONLY.  THREE registers are loaded and
	 * all three are zeroed before their second use:
	 *
	 *     mov $0x4000,%ecx ; mov $0x4000,%edx ; xor %eax,%eax
	 *     mov %cx,0x218    ; xor %ecx,%ecx
	 *     mov %dx,0x1f2    ; xor %edx,%edx
	 *     mov %ax,0x138    ; mov %cx,0x134
	 *
	 * so agc_accum, agc_level and cloop_sin all get zero.  Four stores, two
	 * values, and the pairing is not the one the instruction order
	 * suggests at a glance -- which is exactly how this was read wrong
	 * the first time.  See finding F122.
	 */
	rx->equ_step = 0x4000;
	rx->cloop_cos = 0x4000;
	rx->agc_accum = 0;
	rx->agc_level = 0;
	rx->cloop_sin = 0;

	if (rx->flags & 0x0008) {
		rx->cloop_p_shift = 2;
		rx->cloop_i_shift = 10;		/* and cloop_integrator is left alone */
	} else {
		rx->cloop_integrator = 0;
		rx->cloop_p_shift = 2;
		rx->cloop_i_shift = 8;
	}

	rx->rtncount = 0;   rx->err_symcount = 0;  rx->cloop_phase_hi = 0;  rx->equerr = 0;
	rx->cloop_phase_lo = 0;   rx->preerr = 0;  rx->equerr_accum = 0;  rx->preerr_acc = 0;
	rx->trn_ref_sr = 0;   rx->demod_q_prev = 0;  rx->rx_blocks = 0;  rx->scrambler_sr = 0;
	rx->demod_i_prev = 0;   rx->mix_carrier_phase = 0;  rx->energy.sum = 0;
	rx->vectpp_idx = 0;   rx->agc_pair_count = 0;  obj->hist1_idx = 0;
	rx->rx_samples = (short *)((char *)rx + 0x10c);
	rx->f248 = 0;   rx->sig_energy_acc = 0;
}

/* The bulk ring's wrap: reset to zero, not subtract.  See finding F116. */
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

	sym = (int)(((unsigned)(unsigned short)obj->txpoint.c[1] << 16)
		    | (unsigned short)obj->txpoint.c[0]);
	n = (short)V34ModulatorProcess(
		(struct v34_modulator *)((char *)obj + 0x1450), sym, local);

	if (n > 0) {
		int *wr = txq->wr;

		for (i = 0; i < n; i++) {
			int v = (local[i] * obj->tx_scale + 0x2000) >> 14;

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
	if ((obj->tx_flags & 0x0200) == 0)
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
		short idx = rx->rms_idx;

		rx->rms_idx = (short)(idx + 1);
		rx->rms_buf[idx] = buf[i];
		if ((short)(idx + 1) > V34_AGC_RMS_TAPS - 1)
			rx->rms_idx = 0;
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
void
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
	idx = rx->rms_idx;
	rx->rms_buf[idx] = (short)s0;
	rx->rms_idx = (short)(idx + 1);
	if ((short)(idx + 1) > V34_AGC_RMS_TAPS - 1)
		rx->rms_idx = 0;

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
	 * The AGC runs on every fourth pair.  `agc_pair_count` counts them and the
	 * energy sum accumulates across them; both are cleared whenever the
	 * loop is given a chance to adapt, whether or not it actually did.
	 */
	count = (short)((unsigned short)rx->agc_pair_count + 1);
	sum = rx->energy.sum + g0 * g0;

	if (count <= 3) {
		rx->agc_pair_count = count;
		rx->energy.sum = sum;
	} else if (rx->flags & V34_RX_FLAG_AGC_FREEZE) {
		rx->agc_pair_count = 0;
		rx->energy.sum = 0;
	} else {
		rx->agc_pair_count = count;
		if (agc_rms(rx->rms_buf) > V34_AGC_RMS_FLOOR) {
			/* agcadapt's input is the high half of this. */
			rx->energy.sum = sum;
			agcadapt(rx);
		}
		rx->agc_pair_count = 0;
		rx->energy.sum = 0;
	}

	/*
	 * Down-mix.  One table holds both phases: the carrier is read at the
	 * running index and again `half_len` further on, which is a quarter cycle,
	 * so the second read is the sine of the first.  The index wraps by
	 * subtracting half_len rather than masking.
	 */
	phase = (unsigned short)rx->mix_carrier_phase;
	quarter = (unsigned short)rx->half_len;
	step = (unsigned short)rx->mix_carrier_step;

	cos_v = rx->carrier[(short)phase];
	sin_v = rx->carrier[(short)phase + (short)quarter];

	rx->demod_i = (short)((g0 * sin_v + g1 * cos_v + 0x2000) >> 14);
	rx->demod_q = (short)((g1 * sin_v - g0 * cos_v + 0x2000) >> 14);

	phase += step;
	if ((short)quarter <= (short)phase)
		phase -= quarter;
	rx->mix_carrier_phase = (short)phase;
}

/*
 * The timing loop's resonator, run on the freshly demodulated pair.
 *
 * A second-order section whose input is scaled by 1/16 (`<< 10` against a
 * Q14 accumulator) and whose poles sit just inside the unit circle --
 * 1.4001 and -0.9801 -- so it rings at the baud rate rather than filtering.
 * Its state doubles as the interpolator's endpoint: `demod_i`/`demod_q` are both
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
	acc = (((int)rx->demod_i << 10) + (int)prev * V34_RXTIMING_IIR_A1
	       + (int)rx->dp.iir2.i * V34_RXTIMING_IIR_A2) >> 14;
	rx->dp.iir2.i = prev;
	rx->demod_i = (short)acc;
	rx->f208 = (short)acc;
	if (store_prev)
		rx->demod_i_prev = (short)acc;

	prev = rx->f20a;
	acc = (((int)rx->demod_q << 10) + (int)prev * V34_RXTIMING_IIR_A1
	       + (int)rx->dp.iir2.q * V34_RXTIMING_IIR_A2) >> 14;
	rx->dp.iir2.q = prev;
	rx->demod_q = (short)acc;
	rx->f20a = (short)acc;
	if (store_prev)
		rx->demod_q_prev = (short)acc;
}

/*
 * ---------------------------------------------------------------------------
 * rxtiming -- resample the demodulated signal onto the recovered clock.
 *
 * Produces `out_count` timing-error estimates into `timing_out[]`.  Each one is
 * the magnitude of a point interpolated between the previous demodulated
 * symbol (`demod_i_prev`/`demod_q_prev`) and the current one (`demod_i`/`demod_q`), at the
 * fractional phase `phase_frac`, passed through the timing high-pass.
 *
 * THE PART THAT TOOK SEVEN READINGS.  The phase advances by `phase_inc` per
 * output and wraps at `phase_wrap`; a wrap means the interpolator has run past the
 * current symbol and must pull a new one.  The object handles exactly one or
 * two wraps, on two distinct paths that converge:
 *
 *   - one wrap:  copy demod_i/demod_q down to demod_i_prev/demod_q_prev -- the old current symbol
 *                becomes the new previous one -- then pull ONE symbol.
 *   - two wraps: subtract twice and pull TWO, the first of which writes
 *                demod_i_prev/demod_q_prev from its own resonator output rather than from a
 *                copy.  That is the `store_prev` argument to rx_iir.
 *
 * Three or more wraps are not handled: the phase would still exceed phase_wrap on
 * exit.  With phase_inc < phase_wrap that cannot arise, so it is a bound on the caller
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

	for (i = 0; i < rx->out_count; i = (short)(i + 1)) {
		int wa = (unsigned short)rx->phase_frac;
		int wb = (short)((unsigned short)rx->phase_wrap
				 - (unsigned short)rx->phase_frac);
		int wrap = (short)rx->phase_wrap;
		int I, Q, m, pos;

		/* Linear interpolation between the two symbols, both axes. */
		I = (short)((rx->demod_i * wa + rx->demod_i_prev * wb + 0x2000) >> 14);
		Q = (short)((rx->demod_q * wa + rx->demod_q_prev * wb + 0x2000) >> 14);
		m = (short)((I * I + Q * Q + 0x2000) >> 14);

		rx->timing_out[i] = (short)V34TimingHPFilter(t, (short)m);

		pos = (unsigned short)rx->phase_frac + (unsigned short)rx->phase_inc;

		if ((int)(unsigned short)pos < wrap) {
			rx->phase_frac = (short)pos;
			continue;
		}

		pos -= (unsigned short)rx->phase_wrap;

		if ((int)(unsigned short)pos < wrap) {
			/* One wrap: the current symbol becomes the previous. */
			rx->phase_frac = (short)pos;
			rx->demod_i_prev = rx->demod_i;
			rx->demod_q_prev = rx->demod_q;
		} else {
			/* Two: the first pull supplies the previous symbol. */
			pos -= (unsigned short)rx->phase_wrap;
			rx->phase_frac = (short)pos;
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
 * Sets bit 2 of tx_flags and then dumps both cancellers' coefficients.  The
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

	obj->tx_flags = (short)(obj->tx_flags | V34_EC_FROZEN);

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
 * see finding F110, which is the same offset trap from the other side.
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
 * THE TIMING CONSTANTS.  `phase_wrap` is the interpolator's wrap and `phase_inc` its
 * step, so `phase_inc / phase_wrap` is the ratio between the symbol rate and the sample
 * rate; `symbol_period` keeps the step as configured, since the timing loop slews the
 * live one; `phase_frac` starts the phase at half a step.  2400 baud is the
 * degenerate case where step equals wrap -- one output per input, no
 * resampling -- and every other rate interpolates down from it.
 *
 * 2800 is the odd one out: 0x3e82 and 0x1f41 where every other rate uses
 * 0x3e80 and 0x1f40.  Two counts on the wrap, one on the initial phase.  A
 * wrap of 0x3e82 makes 2800's ratio 0x3594/0x3e82 rather than 0x3594/0x3e80,
 * which is a closer rational fit to 2800/9600 -- so it reads as deliberate
 * rather than as a typo, but the derivation is owed to task #47.
 *
 * THE CARRIER TABLES are each exactly twice `half_len` shorts long, which is
 * what makes V34demodulate's `carrier[i]` and `carrier[i + half_len]` a cosine
 * and its sine: the second half is the first shifted a quarter cycle.  That
 * relation holds for all eight and is the reason half_len is stored at all.
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
	rx->out_count = 4;

	switch (baud) {
	case 2400:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x3e80;
		rx->symbol_period = 0x3e80; rx->phase_frac = 0x1f40;
		break;
	case 2743:
		rx->phase_inc = 0x36b0; rx->phase_wrap = 0x3e80;
		rx->symbol_period = 0x36b0; rx->phase_frac = 0x1f40;
		break;
	case 2800:
		rx->phase_wrap = 0x3e82; rx->phase_inc = 0x3594;
		rx->symbol_period = 0x3594; rx->phase_frac = 0x1f41;
		break;
	case 3000:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x3200;
		rx->symbol_period = 0x3200; rx->phase_frac = 0x1f40;
		break;
	case 3200:
		rx->phase_inc = 0x2ee0; rx->phase_wrap = 0x3e80;
		rx->symbol_period = 0x2ee0; rx->phase_frac = 0x1f40;
		break;
	case 3429:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x2bc0;
		rx->symbol_period = 0x2bc0; rx->phase_frac = 0x1f40;
		break;
	default:
		break;
	}

	switch (carrier) {
	case 1600: rx->carrier = hsine1600; rx->half_len = 6;    break;
	case 1680: rx->carrier = hsine1680; rx->half_len = 0x28; break;
	case 1800: rx->carrier = hsine1800; rx->half_len = 0x10; break;
	case 1829: rx->carrier = hsine1829; rx->half_len = 0x15; break;
	case 1867: rx->carrier = hsine1867; rx->half_len = 0x24; break;
	case 1920: rx->carrier = hsine1920; rx->half_len = 5;    break;
	case 1959: rx->carrier = hsine1959; rx->half_len = 0x31; break;
	case 2000: rx->carrier = hsine2000; rx->half_len = 0x18; break;
	default:   break;
	}
}

/*
 * ---------------------------------------------------------------------------
 * adaptecho -- one echo-canceller step, and the schedule that tunes it.
 *
 * Always returns zero; the state is the output.
 *
 * Per call it dequeues one transmit sample, filters it through the near
 * canceller at a lag derived from how full the transmit queue is, and
 * subtracts the result from the residual.  Then, unless the canceller has
 * been frozen, it adapts -- and that adaptation is on a schedule rather than
 * every step:
 *
 *   calls 1..0x8f     accumulate energy, adapt with the current step
 *   call  0x90        measure the delay line, pick a step-size shift from
 *                     the energy, and recompute the LMS step
 *   after 0x8f        recompute the step whenever the call count is a
 *                     multiple of ten AND past echo_decay_start
 *
 * so it converges fast for the first 143 symbols and then only revisits its
 * step occasionally.  The step itself is computed by `updateAlpha`, which
 * the object inlines here; this calls it, because it is the same function
 * and the AGC lesson applies -- a second hand-transcribed copy of a
 * fixed-point chain is indistinguishable from a correct one until something
 * disagrees.  Reconstructing this is what exposed finding F126.
 */
int
adaptecho(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_queue *txq = &obj->txq;
	short lag;
	short acc;
	int y;
	int e;
	int count;
	int energy = 0;
	int decay_now = 0;

	/*
	 * How far behind to tap, from the queue depth.  Read BEFORE the
	 * count is decremented, so it describes the queue as the sample was
	 * taken rather than after.
	 */
	lag = (short)((unsigned short)obj->dmadelay - (unsigned short)txq->count);
	txq->count = (short)(txq->count - 1);

	/* The residual carries a one-shot correction, which is consumed. */
	acc = (short)((unsigned short)obj->echo_residual + (unsigned short)obj->echo_correction);

	obj->tx_sample = (short)*txq->rd;
	txq->rd = q_next(txq, txq->rd + 1, V34_TXQ_END);
	obj->echo_correction = 0;

	y = V34EchoFilter(&obj->echo0, lag);

	/* Q14 in, Q16 out: the filter's output is scaled by 4 then rounded. */
	e = (short)(acc + ((y * 4 + 0x8000) >> 16));
	obj->echo_residual = (short)e;

	if (obj->tx_flags & V34_EC_FROZEN)
		return 0;

	count = obj->echo_calls + 1;
	obj->echo_calls = count;

	if (count > 0x8f) {
		/*
		 * Past the initial burst.  The step is revisited only every
		 * tenth call once `echo_decay_start` has been passed, and the delay
		 * line is measured exactly once, on call 0x90.
		 */
		if (count > obj->echo_decay_start && count == (count / 10) * 10)
			decay_now = 1;

		if (count == 0x90) {
			energy = V34EchoEstimateDelayLineEnergy(&obj->echo0);

			/*
			 * Pick the step-size shift from the energy gathered
			 * over the first 0x8f calls.  A loud echo gets a
			 * smaller shift, i.e. a bigger step.
			 */
			if (obj->echo_beta > 4
			    && (unsigned)obj->echo_energy <= 0x26259f) {
				obj->echo_beta = 4;
				if ((unsigned)obj->echo_energy > 0xc65d40)
					obj->echo_beta = 2;
			} else if (obj->echo_beta > 5
				   && (unsigned)obj->echo_energy > 0xa7d8c0) {
				obj->echo_beta = 5;
				if ((unsigned)obj->echo_energy > 0xc65d40)
					obj->echo_beta = 2;
			} else if (obj->echo_beta > 2) {
				if ((unsigned)obj->echo_energy > 0xc65d40)
					obj->echo_beta = 2;
			}

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"Echo Energy = %d, BETA = %d\n",
					obj->echo_energy, obj->echo_beta);
		}

		updateAlpha(&obj->echo_alpha, energy, decay_now, 0x7999,
			    obj->echo_decay_fact, "NE");
	} else {
		/* Still gathering: the residual's energy, scaled by 1/64. */
		obj->echo_energy += ((int)acc * acc) >> 6;
	}

	/*
	 * The echo descriptor's +0x14 word, which v34filt.h recorded as
	 * having no reader or writer.  This is the writer: a 16-bit counter
	 * stepped once per call, with nothing found that reads it.
	 */
	obj->echo0.adapt_count = (short)(obj->echo0.adapt_count + 1);

	{
		short err = (short)(((int)obj->echo_alpha * obj->echo_beta * e
				     + 0x2000) >> 14);

		/*
		 * A negative lag means the queue ran past its base, so the
		 * tap the filter used was not the one intended; the original
		 * declines to adapt on it and says so.
		 */
		if (lag < 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90NEC, --------ERROR---"
						     "------ occured in "
						     "adaptecho\n");
			return 0;
		}

		V34EchoAdapt(&obj->echo0, err);
	}

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * modem_serrint -- the per-symbol tick.
 *
 * Named for a serial interrupt and shaped like one: it takes one transmit
 * sample off the queue, cancels the echo it will produce, turns the residual
 * into a complex sample, pushes that onto the RECEIVE queue for rxtiming to
 * pull, and then adapts both cancellers on their own schedules.  This is the
 * function that couples the two halves of the modem.
 *
 * Its opening is `adaptecho`'s, instruction for instruction -- same lag from
 * the queue depth, same dequeue, same wrap -- and then the two diverge
 * completely.  Kept as a shared helper here; see `serr_dequeue`.
 *
 * THREE WAYS TO MAKE THE COMPLEX SAMPLE, chosen by the receiver's flags:
 *
 *   bit 15 set    store the residual as-is, imaginary part zero
 *   bit 11 set    a 60-tap FIR from `fir_coeff`, whose delay line is ECHO1's
 *                 fractional coefficient array -- the same overlay finding
 *                 F100 found DPSK.c using, now with a third reader
 *   otherwise     V34HilbertFilter, giving a genuine analytic pair
 *
 * TWO ADAPTATION SCHEDULES, one per canceller, and they are not the same:
 * the near one measures its delay line on call 0x90 and stops adapting after
 * 0x464f; the far one measures at 0x90 past its own 0x2bb offset and starts
 * adapting only after 0x2bc.  Both recompute their step every 45th call once
 * past 2000, which is the `count % 45` the compiler renders as a multiply by
 * 0xb60b60b7.
 */
static short
serr_dequeue(struct v34_object *obj)
{
	struct v34_queue *txq = &obj->txq;
	short lag;

	lag = (short)((unsigned short)obj->dmadelay - (unsigned short)txq->count);
	txq->count = (short)(txq->count - 1);
	obj->tx_sample = (short)*txq->rd;
	txq->rd = q_next(txq, txq->rd + 1, V34_TXQ_END);
	return lag;
}

int
modem_serrint(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	struct v34_queue *rxq = (struct v34_queue *)rx;
	short lag = serr_dequeue(obj);
	short acc;
	int cancel = 0;
	short sample;
	int out;
	int near_energy = 0, far_energy = 0;
	int near_step = 0, far_step = 0;
	short near_err, far_err = 0;
	int count, prev;
	short *wr;
	short idx;

	/*
	 * The residual, and its own leaky update.  `echo_correction` is a one-shot
	 * correction that adaptecho merely consumes; here it is recomputed,
	 * so the two functions are the producer and the consumer of it.
	 */
	acc = (short)((unsigned short)obj->echo_residual + (unsigned short)obj->echo_correction);
	obj->echo_correction = (short)((acc * 0xed8 - ((int)obj->echo_residual << 12)) >> 12);

	if (obj->tx_flags & V34_EC_FEED) {
		int near = V34EchoFilter(&obj->echo0, lag);
		int far = 0;

		if (obj->far_echo_enable != 0)
			far = V34EchoFilter(&obj->echo1, lag);

		cancel = (near * 4 + far * 4 + 0x8000) >> 16;
	}

	sample = (short)(acc + cancel);
	out = sample;

	/*
	 * Per-symbol history, wrapping at 0x257: the RAW residual.
	 *
	 * THE WRAP TEST IS UNSIGNED, and it was signed here until a whole call
	 * drove the index negative.  0x5d04c is `cmp $0x257,%dx` and 0x5d059
	 * is `jbe`, so the object treats `idx + 1` as a sixteen-bit UNSIGNED
	 * quantity: a negative index wraps to zero where a signed reading
	 * leaves it alone and walks the write further and further below the
	 * ring.  The two readings agree over 0..0x257 and over nothing else,
	 * which is why `t_v34rx.c` could not see it -- it seeds `hist2_idx` to
	 * zero and runs 300 calls, so `idx + 1` never leaves 1..300.
	 *
	 * The `hist1_idx` ring below has always been `(unsigned short)`, from the
	 * same instruction pair one branch along, so the two sites now agree
	 * with each other as well as with the object.  Finding F781.
	 */
	idx = obj->hist2_idx;
	obj->hist_2f58[idx] = sample;
	obj->hist2_idx = (short)(idx + 1);
	if ((unsigned short)(idx + 1) > 0x257)
		obj->hist2_idx = 0;

	rxq->count = (short)(rxq->count + 1);
	wr = (short *)rxq->wr;

	if ((short)rx->flags < 0) {
		wr[0] = sample;
		wr[1] = 0;
	} else if (rx->flags & V34_RX_FLAG_FIR) {
		/*
		 * The 60-tap filter.  Its delay line is echo1's fractional
		 * coefficient array -- one region, now three readings.  The
		 * history is shifted UP as it is read down, so the newest
		 * sample goes in at [0] before the loop and each tap moves
		 * one place as its product is accumulated.
		 */
		short *dl = obj->echo1.coeff_frac;
		const short *c = rx->fir_coeff;
		int sum = 0x8000;
		int i;

		dl[0] = sample;
		for (i = 0; i < 60; i++) {
			short v = dl[0x3b - i];

			dl[0x3c - i] = v;
			sum += (int)c[i] * v;
		}
		/*
		 * And the filtered value REPLACES the residual from here
		 * on: the second history ring, the energy estimate and both
		 * error terms all see this rather than what came in.  The
		 * other two paths leave `out` as the raw sample.  Missing
		 * that is what made this mode, and only this mode, diverge.
		 *
		 * It is not truncated to 16 bits either -- the register is
		 * used at full width downstream and only the queue store
		 * narrows it.
		 */
		out = sum >> 16;
		wr[0] = (short)out;
		wr[1] = 0;
	} else {
		int re = 0, im = 0;

		V34HilbertFilter((short *)((char *)obj + 0xa1b8), sample,
				 &re, &im);
		wr[0] = (short)((re + 0x2000) >> 14);
		wr[1] = (short)((im + 0x2000) >> 14);
	}

	rxq->wr = q_next(rxq, (int *)wr + 1, V34_RXQ_END);

	if (obj->tx_flags & V34_EC_FROZEN)
		return 0;

	/* The second history ring: the residual in both halves of an entry. */
	{
		short k = obj->hist1_idx;

		obj->hist1_idx = (short)(k + 1);
		obj->hist_2aa8[k][0] = (short)out;
		obj->hist_2aa8[k][1] = (short)out;
		if ((unsigned short)obj->hist1_idx > 0x12b)
			obj->hist1_idx = 0;
	}

	obj->echo_resid_energy = (short)(((out * out) >> 10)
			     + (((int)obj->echo_resid_energy * 0x3f48) >> 14));

	prev = obj->echo_calls;
	count = prev + 1;
	obj->echo_calls = count;

	if (count == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34NEC, Start NEC Adaptation\n");
	} else {
		/*
		 * THE FAR COUNTER IS ONE BEHIND THE NEAR ONE, and that is the
		 * object's doing rather than an accident here.  It loads
		 * `echo_calls`, adds one, stores the sum back, and subtracts
		 * 0x2bb from THE VALUE IT LOADED:
		 *
		 *      mov  0x354c(%ebx),%eax     ; the old count
		 *      lea  0x1(%eax),%ebx        ; count = old + 1
		 *      mov  %ebx,0x354c(%edx)
		 *      sub  $0x2bb,%eax           ; far_count = old - 0x2bb
		 *
		 * Everything downstream then splits: `%ebx` drives the near
		 * milestones and `%edi`, sign-extended from `%ax`, the far
		 * ones.  So every far event -- the energy estimate at 0x90,
		 * the 2000-call step -- happens one call LATER than the same
		 * near event would.
		 *
		 * NOT written as `count - 0x2bc`.  That is the same number
		 * and it hides which counter it came from, which is the only
		 * interesting thing about it.  Taking it from `count` cost a
		 * real divergence -- finding F200.
		 */
		int far_count = (short)(prev - 0x2bb);
		int every45 = (count % 45) == 0;

		near_step = every45 && count > 2000;
		far_step = every45 && far_count > 2000;

		if (count == 0x90)
			near_energy =
				V34EchoEstimateDelayLineEnergy(&obj->echo0);
		if (far_count == 0x90 && obj->far_echo_enable != 0)
			far_energy =
				V34EchoEstimateDelayLineEnergy(&obj->echo1);

		updateAlpha(&obj->echo_alpha, near_energy, near_step, 0x6666,
			    0x7f5c, "NE");
		if (obj->far_echo_enable != 0)
			updateAlpha(&obj->short_3552, far_energy, far_step, 0x2b84,
				    0x7f5c, "FE");
	}

	obj->echo0.adapt_count = (short)(obj->echo0.adapt_count + 1);
	near_err = (short)((((int)obj->echo_alpha * out) * 2 + 0x2000) >> 14);

	if (obj->far_echo_enable != 0) {
		obj->echo1.adapt_count = (short)(obj->echo1.adapt_count + 1);
		far_err = (short)((((int)obj->short_3552 * out) * 2 + 0x2000)
				  >> 14);
	}

	if (lag < 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34NEC, --------ERROR--------- "
					     "occured in modem_serrint\n");
		return 0;
	}

	if (obj->echo_calls <= 0x464f) {
		V34EchoAdapt(&obj->echo0, near_err);
	} else if (obj->echo_calls == 0x4650) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34NEC - stop NEC adaptation\n");
	}

	if (obj->far_echo_enable != 0) {
		if (obj->echo_calls > 0x2bc)
			V34EchoAdapt(&obj->echo1, far_err);
		else if (obj->echo_calls == 0x2bc && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34FEC - start FEC adaptation\n");
	}

	return 0;
}


/*
 * rxvect4 -- the four-point constellation the non-trellis path slices
 * against, at .rodata+0x5360.  Packed (re, im) per entry.
 *
 * Note 2289 against -2290: the points are not symmetric about zero but about
 * -0.5, which is what rounding a symmetric constellation to integers gives
 * when the rounding is toward negative infinity.
 */
static const int rxvect4[4] = {
	(int)((unsigned short)2289  | ((unsigned)(unsigned short)2289  << 16)),
	(int)((unsigned short)2289  | ((unsigned)(unsigned short)-2290 << 16)),
	(int)((unsigned short)-2290 | ((unsigned)(unsigned short)-2290 << 16)),
	(int)((unsigned short)-2290 | ((unsigned)(unsigned short)2289  << 16)),
};

/*
 * ---------------------------------------------------------------------------
 * decoderv34 -- turn one demodulated point into bits.
 *
 * Two entirely separate decoders, chosen by three flag bits together:
 *
 *   flags & 0x98 == 0x98   the full 8D trellis path.  Step the sub-frame
 *                          counter and hand the point to demapFrame, which
 *                          accumulates eight of them into a frame.
 *   otherwise              a four-point slice against `rxvect4`, whose index
 *                          is DIFFERENTIALLY decoded: the change since the
 *                          last symbol, modulo four, run through the same
 *                          V34descrambler the data path uses.
 *
 * The second is the handshake's decoder -- QPSK with differential quadrant
 * coding is what V.34 sends before the trellis is trained -- which is why
 * all three flag bits have to agree before the real one is used.
 *
 * `equ_step` is set to 0x2000 or 0x4000 on the way out, and what selects between
 * them is `rx_blocks` against the frame length at +0xaa96 and against half of it:
 * half way through gives 0x4000, the end gives 0x2000.  So it is a progress
 * signal for whoever is counting symbols, not a decode result.
 */
void
decoderv34(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);

	if ((rx->flags & 0x98) == 0x98) {
		short n = rx->subframe_idx;

		rx->subframe_idx = (short)(n + 1);

		if (demapFrame(obj, (char *)obj + 0x474,
			       (char *)obj + 0x470, n)) {
			rx->flags = (unsigned short)(rx->flags & ~0x100);
		} else {
			/*
			 * The frame was rejected: clear the timing IIR's
			 * second-order history and say so.  Those two shorts
			 * are the +0x20c overlay -- `decision` writes them
			 * as one packed point, rxtiming as two taps, and
			 * this zeroes them as two.
			 */
			rx->dp.iir2.i = 0;
			rx->dp.iir2.q = 0;
			rx->flags = (unsigned short)(rx->flags | 0x100);
		}

		/*
		 * A narrow band, not a threshold: the message fires only for
		 * -70 < rtncount < -64.  Below -64 the flag is set regardless.
		 */
		if ((short)rx->rtncount < -64) {
			rx->flags = (unsigned short)(rx->flags | 0x100);
			if ((short)rx->rtncount > -70 && DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V34RENEG, may be renegotiation,"
					"equalizer adaptation disabled\n");
		}

		rx->equ_step = 0x2000;
		return;
	}

	/* Slice against the four points, same shape as `decision`. */
	{
		int best_dist = 0x7fff;
		int bestidx = 0;
		int tx = (unsigned short)rx->target_re;
		int ty = (unsigned short)rx->target_im;
		int i;
		int diff;

		for (i = 0; i < 4; i++) {
			const short *p = (const short *)&rxvect4[i];
			int dx = (short)(tx - (unsigned short)p[0]);
			int dy = (short)(ty - (unsigned short)p[1]);
			/* Logical shift then truncate, as `decision` does. */
			int d = (short)((unsigned)(dx * dx + dy * dy) >> 14);

			if (d < best_dist) {
				best_dist = d;
				bestidx = i;
			}
		}

		rx->dp.point = rxvect4[bestidx];

		/* Differential: the change in quadrant since last symbol. */
		diff = (bestidx - (unsigned short)rx->prev_quadrant) & 3;
		rx->prev_quadrant = (short)bestidx;

		rx->best_index = (short)V34descrambler(rx, (short)diff, 2);
	}

	if (rx->rx_blocks == (short)(obj->baud_rate >> 1))
		rx->equ_step = 0x4000;
	else if (rx->rx_blocks == obj->baud_rate)
		rx->equ_step = 0x2000;
}


/*
 * ---------------------------------------------------------------------------
 * polyValue -- the quadratic setInitialPhase fits its timing metric against.
 *
 *     P(k) = -21k^2 + 837k - 354,  truncated to a short.
 *
 * It peaks near k = 20 and is what turns a measured ratio into a phase
 * index; the derivation belongs with task #47.
 */
int
polyValue(short k)
{
	return (short)(-21 * (int)k * k + 837 * k - 354);
}

/*
 * ---------------------------------------------------------------------------
 * setInitialPhase -- put the interpolator's phase where the timing metric
 * says the symbol centre is.
 *
 * Three steps.  First find where `timing_out[]` changes sign: the metric is
 * a discriminant, so the crossing is the symbol boundary.  The search only
 * runs when the first two samples already disagree in sign, and it gives up
 * at index 5 with a message rather than looking further.
 *
 * Second, interpolate across the crossing -- `(b - a) / (b + a)` in Q13,
 * with both negated first if `a` is negative so the ratio is computed on the
 * positive side either way.
 *
 * Third, find which of twenty candidate phases the ratio matches, by running
 * the same ratio over `polyValue(i + 20)` against `polyValue(i)` and keeping
 * the closest.  That index then moves the phase by `(10 - i) * 560`, clamped
 * one below the wrap -- so index 10 means "already centred".
 *
 * The two error paths divide by zero if not guarded, and the original guards
 * both and says so in the message; that is a check the author put in, not
 * hardening added here.
 */
void
setInitialPhase(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	short k = 1;
	int a, b;
	int ratio = 0;
	int best = 0x7d00;
	int besti = 0;
	int i;
	int pos;

	/* Only search when the first pair already straddles the crossing. */
	if ((int)rx->timing_out[0] * rx->timing_out[1] < 0) {
		do {
			k = (short)(k + 1);
		} while ((int)rx->timing_out[k - 1] * rx->timing_out[k] < 0
			 && k <= 4);

		if (k == 5 && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"setInitialPhase: Error (i==5)  !!!!!!!\n");
		if (k == 5)
			k = 1;
	}

	a = (short)rx->timing_out[k - 1];
	b = (short)rx->timing_out[k];
	if (a < 0) {
		a = (short)-a;
		b = (short)-b;
	}

	/* The order of the pair records which way the metric was going. */
	if (k == 2) {
		rx->timing_idx_a = 2;
		rx->timing_idx_b = 1;
	} else {
		rx->timing_idx_a = 1;
		rx->timing_idx_b = 2;
	}

	if (a + b == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("setInitialPhase() : Error - "
					     "deviding by 0 (samp2+samp1=0)\n");
	} else {
		ratio = (((b - a) << 13) + (a + b) / 2) / (a + b);
	}

	for (i = 0; i <= 0x13; i++) {
		int p0 = polyValue((short)i);
		int p1 = polyValue((short)(i + 20));
		int r = 0;
		int e;

		if (p1 + p0 == 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"setInitialPhase() : Error - deviding "
					"by 0 (polyValue(k2)+polyValue(k)=0)\n");
		} else {
			r = ((((p1 - p0) << 13) + (p1 + p0) / 2)
			     / (p1 + p0));
		}

		e = (short)((((ratio - r) * (ratio - r)) + 0x1000) >> 13);
		if (e < best) {
			best = e;
			besti = i;
		}
	}

	pos = (10 - besti) * 0x230 + (unsigned short)rx->phase_frac;

	if ((int)(unsigned short)pos < (int)(short)rx->phase_wrap)
		rx->phase_frac = (short)pos;
	else
		rx->phase_frac = (short)(rx->phase_wrap - 1);
}


/*
 * ---------------------------------------------------------------------------
 * setTimingStateParameters -- install the timing loop's gains for a state.
 *
 * A nine-way switch on `pllcnt`, the timing state, spelled as a jump table.
 * There are TWO tables, chosen by whether `role` is 0x65, and they agree
 * except on states 5, 6 and 7 -- the fast part of the acquisition ramp --
 * so the second is a retuning of the same schedule rather than a different
 * one.  Both are reproduced as one switch with the variant inline, since
 * splitting them would hide how little differs.
 *
 * States 0 and 1 install nothing.  States 5 and 8 additionally report the
 * timing offset to the V.90 side, as `timing_offset * 10` -- the only place that
 * number leaves the datapump.
 *
 * The state is compared UNSIGNED against 8, so a negative `pllcnt` misses the
 * table entirely rather than indexing behind it.  That is the bounds check
 * the other three tables in this reconstruction do not have (finding F129).
 */
void
setTimingStateParameters(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	int variant = (obj->role == 0x65);
	int state = (short)rx->pllcnt;
	int report = 0;

	if ((unsigned)state <= 8) {
		switch (state) {
		case 2:
			rx->timing_p_gain = 0x36b0; rx->timing_i_gain = 0;    rx->dwell_limit = 0x190;
			break;
		case 3:
			rx->timing_p_gain = 0x2ee0; rx->timing_i_gain = 0xd2; rx->dwell_limit = 0x3e8;
			break;
		case 4:
			rx->timing_p_gain = 0x1770; rx->timing_i_gain = 0x5a; rx->dwell_limit = 0x3e8;
			break;
		case 5:
			rx->timing_p_gain = 0xdac;  rx->timing_i_gain = 0x1e;
			rx->dwell_limit = (short)(variant ? -1 : 0x3e8);
			report = 1;
			break;
		case 6:
			if (variant) {
				rx->timing_p_gain = 0xdac; rx->timing_i_gain = 3;
			} else {
				rx->timing_p_gain = 0x7d0; rx->timing_i_gain = 0xa;
			}
			rx->dwell_limit = 0x7d0;
			break;
		case 7:
			if (variant) {
				rx->timing_p_gain = 0x3e8; rx->timing_i_gain = 2;
				rx->dwell_limit = 0x7d0;
			} else {
				rx->timing_p_gain = 0x5dc; rx->timing_i_gain = 2;
				rx->dwell_limit = 0xfa0;
			}
			break;
		case 8:
			rx->timing_p_gain = 0x1f4;  rx->timing_i_gain = 1;    rx->dwell_limit = -1;
			report = 1;
			break;
		default:		/* 0 and 1 install nothing */
			break;
		}
	}

	if (report)
		VPcmV34LogTimingOffset(obj, (short)(rx->timing_offset * 10));

	/* State 2 alone also sets the dwell from the frame length. */
	if ((unsigned short)rx->pllcnt == 2)
		rx->report_interval = (short)(obj->baud_rate >> 3);
}


/*
 * ---------------------------------------------------------------------------
 * TimingV34 -- the timing recovery loop's state machine and its integrator.
 *
 * Called once per symbol from `receiver`.  Three parts:
 *
 *   THE STATE MACHINE.  `pllcnt` is the state.  -1 means done and returns
 *   immediately.  1 means "start": zero the dwell counter, centre the phase
 *   with setInitialPhase, and move to state 2 -- or to state 6 if `slow_ramp`
 *   says to skip the slow part of the ramp.  Otherwise, once the dwell
 *   counter `dwell_count` reaches the limit `dwell_limit`, the state advances by one and
 *   the gains are reinstalled.  A limit of -1 means never advance.
 *
 *   THE PHASE DETECTOR.  Two timing_out[] entries, indexed by the pair
 *   setInitialPhase chose, are each squared and summed as (I^2 + Q^2) for
 *   two positions; the loop then forms (b - a) / (b + a) in Q15 after
 *   normalising both up until neither has bits above 30.  That
 *   normalisation is a loop with its own 16-step cap, and the shift is
 *   applied to both so the ratio is unaffected -- it is there for the
 *   divide's range, not for accuracy.
 *
 *   THE INTEGRATOR.  error * timing_i_gain in Q15 accumulates into the 32-bit timing_integrator;
 *   error * timing_p_gain in Q11 is added on top per symbol.  The sum is carried in
 *   f1d8 and its whole part, in units of 1/32768 of a symbol, is added to
 *   the interpolator's step phase_inc.  Every 0x1d2 symbols the accumulated
 *   offset is converted to parts per million -- the `* 10000 / n` then
 *   `* 100 / symbol_period` -- and stored in timing_offset for setTimingStateParameters to
 *   report onward.
 */
void
TimingV34(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	int state;
	int i0, i1;
	int a, b;
	int err = 0;
	int acc;
	int whole;
	int n;

	state = (unsigned short)rx->pllcnt;

	if (state == 0xffff)
		return;

	if (state == 1) {
		rx->dwell_count = 0;
		setInitialPhase(obj);
		if (rx->slow_ramp == 1) {
			rx->pllcnt = 2;
			rx->slow_ramp = 0;
		} else {
			rx->pllcnt = 6;
		}
		setTimingStateParameters(obj);
		state = (unsigned short)rx->pllcnt;
	}

	/* Dwell: advance a state once dwell_count reaches dwell_limit, which -1 disables. */
	if ((unsigned short)rx->dwell_limit != 0xffff && state != 0) {
		int next = (unsigned short)(rx->dwell_count + 1);

		if (next == (int)(short)rx->dwell_limit) {
			rx->dwell_count = 0;
			rx->pllcnt = (short)(state + 1);
			setTimingStateParameters(obj);
			state = (unsigned short)rx->pllcnt;
		} else {
			rx->dwell_count = (short)next;
		}
	}

	/* The phase detector: two squared magnitudes, differenced. */
	i0 = rx->timing_idx_a;
	i1 = rx->timing_idx_b;
	/*
	 * EARLY AND LATE, either side of the index -- [i-1] and [i+1], not
	 * [i-1] and [i].  Reading them as adjacent gives a discriminator
	 * with no gap in the middle and an error term about 2.5x too large.
	 */
	a = (int)rx->timing_out[i0 - 1] * rx->timing_out[i0 - 1]
	  + (int)rx->timing_out[i0 + 1] * rx->timing_out[i0 + 1];
	b = (int)rx->timing_out[i1 - 1] * rx->timing_out[i1 - 1]
	  + (int)rx->timing_out[i1 + 1] * rx->timing_out[i1 + 1];

	if (state == 0) {
		acc = rx->timing_integrator;
	} else {
		int sh = 0;

		/*
		 * Normalise both up together, capped at 16 steps.  Applied to
		 * both, so the ratio below is unchanged -- this is range for
		 * the divide, not precision.
		 */
		if (a >= 0 && b >= 0) {
			unsigned m = 0x80000000u;

			/*
			 * The counter is incremented BEFORE the first test,
			 * so an exit on `a` still counts the step.  Doing it
			 * after leaves the shift one short and the error
			 * term exactly twice too large.
			 */
			for (;;) {
				m >>= 1;
				sh = (short)(sh + 1);
				if ((unsigned)a & m)
					break;
				if (((unsigned)b & m) != 0 || sh > 15)
					break;
			}
		}

		a = (unsigned short)((a >> ((16 - sh) & 31)));
		b = (unsigned short)((b >> ((16 - sh) & 31)));

		if ((a | b) != 0)
			err = (((b - a) << 15) + (a + b) / 2) / (a + b);

		acc = rx->timing_integrator + (((int)rx->timing_i_gain * err + 0x4000) >> 15);
		rx->timing_integrator = acc;
		acc += ((int)rx->timing_p_gain * err + 0x200) >> 11;
	}

	/* Carry the fraction, hand the whole part to the interpolator. */
	acc += rx->f1d8;
	whole = (short)((acc + 0x4000) >> 15);
	rx->f1d8 = acc - (whole << 15);
	rx->phase_inc = (short)(whole + (unsigned short)rx->symbol_period);

	n = (unsigned short)(rx->ppm_count + 1);
	acc = whole + (unsigned short)rx->ppm_acc;

	if (n < (int)(short)rx->report_interval) {
		rx->ppm_count = (short)n;
		rx->ppm_acc = (short)acc;
		return;
	}

	/* Every report_interval symbols: convert the accumulated slip to ppm. */
	{
		int ppm = ((short)acc * 10000 + n / 2) / n;

		ppm = (ppm * 25 * 4 + (short)rx->symbol_period / 2)
		      / (short)rx->symbol_period;
		rx->timing_offset = (short)ppm;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"TimingV34: Timing Offset [ppm] = %d\n",
				(int)(short)ppm);
	}

	rx->ppm_count = 0;
	rx->ppm_acc = 0;
}

/*
 * ---------------------------------------------------------------------------
 * vectpp -- the phase-reference sequence the handshake slices against.
 *
 * Forty-eight complex points at .rodata+0x2c80, packed (re, im) per entry,
 * every one of magnitude 6476 at a multiple of 60 degrees: (6476, 0),
 * (+/-3238, +/-5609) and (-6476, 0).  Six phases, which is V.34's PP signal
 * (10.1.3.5) -- the periodic sequence sent during phase 3 so the receiver can
 * measure the channel's phase response.
 *
 * `receiver` halves both halves on the way out, so the constellation it
 * actually compares against has magnitude 3238.
 *
 * GLOBAL, which is the object's binding and not a convenience: the blob
 * exports `vectpp` because `v34handshak` reads it as well.  Table 1's 20
 * `PPSEG` (v34hstx1.cpp) loads it as forty-eight FOUR-BYTE entries, one
 * (re, im) pair each, where this file reads the same bytes as ninety-six
 * shorts.  It was static while this file was its only reader; the second
 * reader is what made it global, and `t_v34hstx1.c` proves it against
 * `ref_vectpp` the way `probe` and `vect4` are proved.
 */
const short vectpp[96] = {
	6476, 0, 6476, 0, 6476, 0, 6476, 0,
	-3238, 5609, -5609, 3238, -6476, 0, -5609, -3238,
	6476, 0, 3238, 5609, -3238, 5609, -6476, 0,
	6476, 0, 0, 6476, -6476, 0, 0, -6476,
	-3238, 5609, -3238, -5609, 6476, 0, -3238, 5609,
	6476, 0, -5609, 3238, 3238, -5609, 0, 6476,
	6476, 0, -6476, 0, 6476, 0, -6476, 0,
	-3238, 5609, 5609, -3238, -6476, 0, 5609, 3238,
	6476, 0, -3238, -5609, -3238, 5609, 6476, 0,
	6476, 0, 0, -6476, -6476, 0, 0, 6476,
	-3238, 5609, 3238, 5609, 6476, 0, 3238, -5609,
	6476, 0, 5609, -3238, 3238, -5609, 0, -6476,
};

/*
 * The squared distance from the derotated point to the current decision,
 * in the same Q14 the rest of the receiver works in.  Three call sites,
 * all in `receiver`, all reading the two fields rather than arguments --
 * which is why this takes the object and not four shorts.
 */
static int
rx_slice_err(const struct v34_receiver *rx)
{
	int dr = (short)((unsigned short)rx->target_re
			 - (unsigned short)rx->dp.iir2.i);
	int di = (short)((unsigned short)rx->target_im
			 - (unsigned short)rx->dp.iir2.q);

	return (dr * dr + di * di) >> 14;
}

/*
 * The imaginary part of decision* x target, shifted up two: the carrier
 * loop's phase error.  All four of `receiver`'s decision paths end here,
 * with the same expression over the same two pairs of fields.
 */
static int
rx_phase_error(const struct v34_receiver *rx)
{
	int p = (short)rx->dp.iir2.i * (short)rx->target_im
		- (short)rx->dp.iir2.q * (short)rx->target_re;

	return (int)((unsigned)p << 2);
}

/*
 * ---------------------------------------------------------------------------
 * rx_predict -- the three-tap complex predictor, and its history shift.
 *
 * `*px`/`*py` are both the input and the output: the prediction formed from
 * the three previous inputs is ADDED to the current one, and the current one
 * is then pushed into the history.  `receiver` runs this twice per symbol
 * over one shared set of coefficients and one shared history -- once on the
 * equaliser output and once on the decision error.
 *
 * THE ROUNDING CONSTANT IS SUBTRACTED ON THE REAL AXIS.  The imaginary
 * accumulator starts at +0x2000 as everything else in this file does, but
 * the real one is formed as `(b.hist_i) - (0x2000 + a.hist_q)`, so its half
 * -LSB lands on the wrong side.  That is what the object does; it costs one
 * count of bias and is reproduced rather than corrected.
 */
static void
rx_predict(struct v34_receiver *rx, short *px, short *py)
{
	int acc;
	int outi, outq;
	int k;

	acc = 0x2000;
	for (k = 0; k < 3; k++)
		acc += rx->pred_a[k] * rx->pred_q[2 - k];
	acc = -acc;
	for (k = 0; k < 3; k++)
		acc += rx->pred_b[k] * rx->pred_i[2 - k];
	outi = (short)(acc >> 14);

	/*
	 * The shift and the imaginary accumulator are one pass in the object,
	 * and have to stay one here: each entry is read for the product and
	 * then copied up, so splitting them would read the shifted value.
	 */
	acc = 0x2000;
	for (k = 0; k < 3; k++) {
		rx->pred_i[3 - k] = rx->pred_i[2 - k];
		acc += rx->pred_i[2 - k] * rx->pred_a[k];
	}
	for (k = 0; k < 3; k++) {
		rx->pred_q[3 - k] = rx->pred_q[2 - k];
		acc += rx->pred_q[2 - k] * rx->pred_b[k];
	}
	outq = (short)(acc >> 14);

	rx->pred_i[0] = *px;
	rx->pred_q[0] = *py;
	*px = (short)((unsigned short)*px + outi);
	*py = (short)((unsigned short)*py + outq);
}

/*
 * TRN, the training sequence: the reference is the scrambler's own output
 * sliced onto `rxvect4`, so both ends generate it and neither transmits it.
 *
 * Two scramblers run, not one.  The first drives the reference.  The second,
 * over `scrambler_sr`, is stepped for sixteen symbols (0x143..0x152) and its
 * output DISCARDED on every symbol but one -- it exists to be in the right
 * state at symbol 0x153, where it names the point to fall back to if the
 * equaliser has locked onto the wrong TRN.
 *
 * That last check is the "shifted TRN2" case, and it is decided by energy:
 * if taps 60..75 of the equaliser hold more than taps 32..47, the impulse
 * response has settled a symbol period late, and the whole equaliser is
 * thrown away rather than nudged.
 *
 * Returns the caller's `flags`, refreshed wherever a call could have moved
 * it -- see the note on `receiver`.
 */
static unsigned
rx_train_point(struct v34_receiver *rx, struct v34_equalizer *eq, short n,
	       unsigned flags)
{
	short k;

	k = (short)V34scrambler((unsigned *)&rx->trn_ref_sr, (short)(flags & 4),
				3, 2);
	rx->dp.point = rxvect4[k];
	flags = rx->flags;

	if (!(flags & V34_RX_FLAG_TRN_WATCH))
		return flags;

	if ((unsigned short)(n - 0x143) <= 0xf)
		k = (short)V34scrambler(&rx->scrambler_sr,
					(short)(flags & 4), 3, 2);

	if (n == 0x153) {
		int early = 0, late = 0;
		int z;

		for (z = 0x20; z <= 0x2f; z++)
			early += eq->re[z] * eq->re[z]
				 + eq->im[z] * eq->im[z];
		for (z = 0x3c; z <= 0x4b; z++)
			late += eq->re[z] * eq->re[z]
				+ eq->im[z] * eq->im[z];

		if (late > early) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "Detected shifted TRN2... assuming 32 "
				    "symbols Snot\n");
			rx->trn_ref_sr = (int)rx->scrambler_sr;
			V34EqualizerCleanUp(eq);
			rx->dp.point = rxvect4[k];
		}
		/*
		 * The second scrambler is reset HERE and only here -- not on
		 * every symbol of the window that stepped it, and not when
		 * the check decides the equaliser was fine.  It has done its
		 * one job; the next TRN starts it again from zero.
		 */
		rx->scrambler_sr = 0;
	}

	return rx->flags;
}

/*
 * ---------------------------------------------------------------------------
 * receiver -- the per-symbol receive chain, end to end.
 *
 * Resample onto the recovered clock, equalise, predict, derotate, decide,
 * and close both loops: the carrier NCO on the decision's phase error and
 * the equaliser on its amplitude error.  4326 bytes, and the last function
 * of V34RX.c.
 *
 * THE OPENING LOOP IS NOT rxtiming'S.  It has the same interpolation and the
 * same one-or-two-pull wrap, but two differences that make adapting rxtiming
 * a mistake (finding F133):
 *
 *   - there is no resonator.  rxtiming runs a two-tap IIR after each pull
 *     and feeds its output back as the interpolation endpoint; this does
 *     not, so `demod_i_prev`/`demod_q_prev` are a plain copy of `demod_i`/`demod_q` and the copy
 *     sits BETWEEN the two pulls rather than before a single one.
 *   - outputs are tested for parity.  Every output produces a timing metric
 *     through V34TimingFilter, but only the ODD ones advance the equaliser's
 *     delay line, through V34TimingPrefilter.  That is the two-samples-per
 *     -symbol structure, and it is why the loop cannot share rxtiming's
 *     shape however similar the arithmetic looks.
 *
 * `flags` IS A CACHED LOCAL, NOT A FIELD.  The object loads +0x122 into a
 * register once and refreshes it only after a call that could have changed
 * it -- decoderv34, V34EqualizerClearCenterTaps, V34scrambler's second path
 * and each debug printf.  Reading the field at every gate instead would be a
 * different function on any path where decoderv34 writes it.
 */
void
receiver(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	struct v34_timing *t = (struct v34_timing *)((char *)obj + 0x50c);
	struct v34_equalizer *eq =
		(struct v34_equalizer *)((char *)rx + V34_RX_EQ_OFFSET);
	unsigned flags;
	int eq_re, eq_im;
	int pherr;
	short i;

	rx->rx_samples = (short *)((char *)obj + 0x370);

	for (i = 0; i < rx->out_count; i = (short)(i + 1)) {
		int wa = (unsigned short)rx->phase_frac;
		int wb = (short)((unsigned short)rx->phase_wrap
				 - (unsigned short)rx->phase_frac);
		int wrap = (short)rx->phase_wrap;
		int ir, ii, pos;

		ir = (rx->demod_i * wa + rx->demod_i_prev * wb + 0x2000) >> 14;
		ii = (rx->demod_q * wa + rx->demod_q_prev * wb + 0x2000) >> 14;

		rx->timing_out[i] = (short)V34TimingFilter(t,
			(int)(((unsigned)ii << 16)
			      | (unsigned short)ir));

		/* Odd outputs also step the equaliser's delay line. */
		if (i & 1) {
			int v = V34TimingPrefilter(t);

			V34EqualizerUpdateDelayLine(eq, (short)v,
						    (short)(v >> 16));
		}

		pos = (unsigned short)rx->phase_frac + (unsigned short)rx->phase_inc;

		if ((int)(unsigned short)pos < wrap) {
			rx->phase_frac = (short)pos;
			continue;
		}

		pos -= (unsigned short)rx->phase_wrap;

		if ((int)(unsigned short)pos >= wrap) {
			/* Two wraps: the first pull is the previous symbol. */
			pos -= (unsigned short)rx->phase_wrap;
			rx->phase_frac = (short)pos;
			V34demodulate(rx);
		} else {
			rx->phase_frac = (short)pos;
		}

		rx->demod_i_prev = rx->demod_i;
		rx->demod_q_prev = rx->demod_q;
		V34demodulate(rx);
	}

	TimingV34(obj);
	V34EqualizerFilter(eq, &eq_re, &eq_im);

	flags = rx->flags;
	rx->f208 = (short)((eq_re + 0x2000) >> 14);
	rx->f20a = (short)((eq_im + 0x2000) >> 14);

	/*
	 * The retrain detector.  How far the equalised point moved since the
	 * last symbol, and since the one before that, both in Q14 -- and a
	 * counter that runs UP while the first distance stays under 128 and
	 * DOWN while only the second does.  A signal that stops moving is a
	 * signal that has stopped carrying data, so 0x8c consecutive still
	 * symbols is a retrain request; passing back down through -0x84..
	 * -0x78 on the way out is a renegotiation request instead.
	 */
	if (flags & V34_RX_FLAG_DATA) {
		int d1, d2;

		d1 = (((short)((unsigned short)rx->f208
			       - (unsigned short)rx->eq_out_i1)
		       * (short)((unsigned short)rx->f208
				 - (unsigned short)rx->eq_out_i1))
		      + ((short)((unsigned short)rx->f20a
				 - (unsigned short)rx->eq_out_q1)
			 * (short)((unsigned short)rx->f20a
				   - (unsigned short)rx->eq_out_q1))) >> 14;
		d2 = (((short)((unsigned short)rx->f208
			       - (unsigned short)rx->eq_out_i2)
		       * (short)((unsigned short)rx->f208
				 - (unsigned short)rx->eq_out_i2))
		      + ((short)((unsigned short)rx->f20a
				 - (unsigned short)rx->eq_out_q2)
			 * (short)((unsigned short)rx->f20a
				   - (unsigned short)rx->eq_out_q2))) >> 14;

		/* Both pairs shift along, as one 32-bit move each. */
		rx->eq_out_i2 = rx->eq_out_i1;
		rx->eq_out_q2 = rx->eq_out_q1;
		rx->eq_out_i1 = rx->f208;
		rx->eq_out_q1 = rx->f20a;

		if ((short)((short)d1 - 0x80) <= 0) {
			int n = (unsigned short)rx->rtncount + 1;

			if ((short)n <= 0x8c) {
				rx->rtncount = (short)n;
			} else {
				flags |= V34_RX_FLAG_RETRAIN;
				rx->rtncount = 0;
				rx->flags = (unsigned short)flags;
				if (DSPLIB_DEBUG_ON()) {
					dsplibs_debug_printf(
					    "V34RETRAIN, retrain request "
					    "detected, rtncount = %d \n",
					    (int)rx->rtncount);
					flags = rx->flags;
				}
			}
		} else if ((short)((short)d2 - 0x80) <= 0) {
			rx->rtncount = (short)((unsigned short)rx->rtncount - 1);
		} else {
			short n = rx->rtncount;

			if (n + 0x78 <= 0 && n + 0x84 > 0) {
				flags |= V34_RX_FLAG_RENEG;
				rx->flags = (unsigned short)flags;
				if (DSPLIB_DEBUG_ON()) {
					dsplibs_debug_printf(
					    "V34RENEG, RRN request detected,"
					    "rtncount = %d\n", (int)n);
					flags = rx->flags;
				}
			}
			rx->rtncount = 0;
		}
	}

	/*
	 * The precoder: predict the equaliser output from its own three-symbol
	 * history and add the prediction back in, before anything else sees
	 * it.  Coefficients shared with the adapting copy below.
	 */
	if (flags & V34_RX_FLAG_PRECODE)
		rx_predict(rx, &rx->f208, &rx->f20a);

	/* Derotate by the recovered carrier: target = conj(nco) * equalised. */
	{
		int cr = (short)rx->cloop_cos;
		int ci = (short)rx->cloop_sin;
		int xr = (short)rx->f208;
		int xi = (short)rx->f20a;
		short k;

		rx->target_re = (short)((cr * xr + ci * xi + 0x2000) >> 14);
		rx->target_im = (short)((cr * xi - ci * xr + 0x2000) >> 14);

		/* And into the per-symbol history ring modem_serrint shares. */
		k = obj->hist1_idx;
		obj->hist1_idx = (short)(k + 1);
		obj->hist_2aa8[k][0] = rx->target_re;
		obj->hist_2aa8[k][1] = rx->target_im;
		if ((unsigned short)obj->hist1_idx > 0x12b)
			obj->hist1_idx = 0;
	}

	flags = rx->flags;

	if (flags & V34_RX_FLAG_DATA) {
		/*
		 * Data mode.  Which reference the error is measured against
		 * is a ladder on the symbol count, and 0x8 shifts the whole
		 * ladder on by 0x120 symbols -- a late start to TRN.
		 */
		short n;

		if (rx->rx_blocks <= 0x11) {
			/* Before TRN there is nothing to predict from. */
			sysdep_memset(rx->pred_b, 0,
				      sizeof rx->pred_b + sizeof rx->pred_a);
			rx->vectpp_idx = 0;
			rx->scrambler_sr = 0;
			return;
		}

		n = rx->rx_blocks;
		if (flags & V34_RX_FLAG_LATE_TRN)
			n = (short)(n + 0x120);

		if (!(flags & V34_RX_FLAG_LATE_TRN) && n <= 0x132) {
			/* Phase 3: the PP sequence, known and generated. */
			int k = (short)rx->vectpp_idx;

			rx->dp.iir2.i = (short)(vectpp[k * 2] >> 1);
			rx->dp.iir2.q = (short)(vectpp[k * 2 + 1] >> 1);
			k = (unsigned short)rx->vectpp_idx + 1;
			rx->vectpp_idx = (short)((short)k <= 0x2f ? k : 0);
		} else if (n > 0x332) {
			/* Past TRN: the real decoder. */
			decoderv34(obj);
			flags = rx->flags;
		} else {
			flags = rx_train_point(rx, eq, n, flags);
		}
	} else if (rx->pllcnt > 1) {
		/*
		 * The handshake's reference generator.  Two of the four
		 * `rxvect4` points, alternating with the symbol count -- the
		 * sequence is known, so this is not a decision.  What it
		 * produces is the error, and the error closes both loops.
		 */
		short n;
		int err;

		if (rx->rx_blocks == 0) {
			/*
			 * Symbol zero has no parity to carry forward, so both
			 * candidates are tried and the closer one names the
			 * phase every later symbol alternates from.
			 */
			int e3, e0;

			rx->dp.point = rxvect4[3];
			e3 = rx_slice_err(rx);
			rx->dp.point = rxvect4[0];
			e0 = rx_slice_err(rx);

			rx->rx_blocks = (short)((short)e0 <= (short)e3 ? 2 : 1);
		}

		rx->dp.point = (rx->rx_blocks & 1) ? rxvect4[3] : rxvect4[0];
		rx->equ_step = 0x7000;

		err = rx_slice_err(rx);
		n = rx->rx_blocks;

		if (n == 0x40 && !(flags & V34_RX_FLAG_DET_PENDING)
		    && DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(
				"V34AGC, abcddetect gain = 0x%x, "
				"AGC frozen\n", (int)rx->agc_gain);
			flags = rx->flags;
		}

		if (n > 0x40) {
			flags |= V34_RX_FLAG_DET_PENDING;
			rx->flags = (unsigned short)flags;
		}
		if (n > 0x68) {
			flags |= V34_RX_FLAG_TRAINED;
			rx->flags = (unsigned short)flags;

			/*
			 * Trained, and still this far out: the centre taps
			 * are wrong rather than merely unconverged.  Zero
			 * them, restart the symbol count, and go back to
			 * data mode -- 0x600 is both DATA and DET_PENDING,
			 * and setting DATA here is the only way the
			 * loss-of-signal check below is ever reached.
			 */
			if ((short)err > 0x600) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "S-S1 is detected,rxsymcnt= %d,"
					    "pllcnt= %d,gain= 0x%x\n",
					    (int)n, (int)rx->pllcnt,
					    (int)rx->agc_gain);
				rx->rx_blocks = 0;
				flags = (rx->flags & ~V34_RX_FLAG_TRAINED)
					| 0x600;
				rx->flags = (unsigned short)flags;
				V34EqualizerClearCenterTaps(eq);
				flags = rx->flags;
			}
		}

		rx->cloop_phase_err = rx_phase_error(rx);
		pherr = rx->cloop_phase_err;

		if (flags & V34_RX_FLAG_DATA) {
			/*
			 * Loss of signal.  A fourth copy of the 36-sample
			 * RMS, and the only place in the receiver that stops
			 * the datapump outright.
			 */
			if ((short)agc_rms(rx->rms_buf)
			    <= obj->rx_energy_floor) {
				if (DSPLIB_DEBUG_ON()) {
					dsplibs_debug_printf(
					    "Signal Energy below Threshold "
					    "%d, initiate a disconnection",
					    obj->rx_energy_floor);
					flags = rx->flags;
				}
				obj->status = 10;
			}
			return;
		}

		goto carrier_loop;
	} else {
		return;
	}

	rx->cloop_phase_err = rx_phase_error(rx);
	pherr = rx->cloop_phase_err;

carrier_loop:
	/*
	 * The predictor proper: the same three taps, now driven by the
	 * decision error, adapted by a complex LMS, and with the error energy
	 * accumulated into `preerr`.
	 */
	if (flags & V34_RX_FLAG_PREDICT) {
		int ei, eqv;
		int k;

		rx->pred_err_re = (short)((unsigned short)rx->target_re
				   - (unsigned short)rx->dp.iir2.i);
		rx->pred_err_im = (short)((unsigned short)rx->target_im
				   - (unsigned short)rx->dp.iir2.q);

		rx_predict(rx, &rx->pred_err_re, &rx->pred_err_im);

		ei = rx->pred_err_re;
		eqv = rx->pred_err_im;
		rx->preerr_acc += ei * ei + eqv * eqv;

		/*
		 * dc = -e . conj(hist), at 32-bit precision with the taps
		 * carried in the high half of an int.  The history is read
		 * at 3..1 rather than 2..0 because rx_predict has already
		 * shifted it: those are the same three entries the
		 * prediction used.
		 */
		for (k = 0; k < 3; k++) {
			int hi = rx->pred_i[3 - k];
			int hq = rx->pred_q[3 - k];
			unsigned acc;

			acc = (unsigned)rx->pred_b[k] << 16;
			acc -= (unsigned)(hi * ei);
			acc -= (unsigned)(hq * eqv);
			rx->pred_b[k] = (short)((int)(acc + 0x8000) >> 16);

			acc = (unsigned)rx->pred_a[k] << 16;
			acc -= (unsigned)(hi * eqv);
			acc += (unsigned)(hq * ei);
			rx->pred_a[k] = (short)((int)(acc + 0x8000) >> 16);
		}
	}

	/*
	 * The carrier loop.  f210/f212 STOP being the received point here and
	 * become the DECISION rotated back up by the same carrier, which is
	 * the reference the equaliser's error is measured against below.
	 *
	 * The NCO itself: the phase error drives a direct term shifted by
	 * cloop_p_shift and an integrator shifted by cloop_i_shift, summing into a 32-bit phase
	 * whose high half indexes `costbl` and whose low five bits rotate
	 * between two entries to first order.  50/65536 radian per count is
	 * one 256-entry step over 32, so that interpolation is exact by
	 * construction rather than by tuning.
	 */
	{
		int cr = (short)rx->cloop_cos;
		int ci = (short)rx->cloop_sin;
		int xr = (short)rx->dp.iir2.i;
		int xi = (short)rx->dp.iir2.q;
		int phase, idx, frac;
		int c, s;

		rx->target_re = (short)((cr * xr - ci * xi + 0x2000) >> 14);
		rx->target_im = (short)((ci * xr + cr * xi + 0x2000) >> 14);

		rx->cloop_integrator += pherr >> ((short)rx->cloop_i_shift & 31);

		phase = (pherr >> ((short)rx->cloop_p_shift & 31))
			+ (rx->cloop_integrator >> 5)
			+ (int)(((unsigned)(short)rx->cloop_phase_hi << 16)
				+ (unsigned)(short)rx->cloop_phase_lo);
		rx->cloop_phase_lo = (short)phase;
		phase >>= 16;

		rx->cloop_phase_hi = (short)(phase & 0x1fff);
		idx = (phase & 0x1fff) >> 5;
		frac = (phase & 0x1f) * 50;

		c = costbl[idx];
		s = (short)-(unsigned short)costbl[(idx + 0x40) & 0xff];

		rx->cloop_sin = (short)((int)(((unsigned)s << 16)
					 + (unsigned)(frac * c) + 0x8000)
				   >> 16);
		rx->cloop_cos = (short)((int)(((unsigned)c << 16)
					 - (unsigned)(frac * s) + 0x8000)
				   >> 16);
	}

	if (flags & V34_RX_FLAG_TRAINED)
		return;

	/*
	 * The equaliser's error, and the two ways of applying it:
	 * V34EqualizerAdapt over all eighty taps in data mode, and
	 * V34EqualizerCenterAdapt over the middle eight at twice the gain
	 * while still acquiring.
	 */
	{
		int dr = (short)((unsigned short)rx->f208
				 - (unsigned short)rx->target_re);
		int di = (short)((unsigned short)rx->f20a
				 - (unsigned short)rx->target_im);
		int er = (dr * (short)rx->equ_step) >> 16;
		int ei = (di * (short)rx->equ_step) >> 16;
		int mag = dr * dr + di * di + rx->equerr_accum;
		int n;

		rx->sig_energy_acc += ((int)rx->target_re * rx->target_re
			     + (int)rx->target_im * rx->target_im) >> 8;

		n = ((unsigned short)rx->err_symcount + 1) & 0x3ff;
		rx->err_symcount = (short)n;

		if (n != 0) {
			rx->equerr_accum = mag;
		} else {
			/*
			 * Published as shorts, and saturated rather than
			 * wrapped -- but only against a NEGATIVE accumulator,
			 * which is the sign of overflow rather than of a
			 * small value.  A sum of squares cannot be negative
			 * otherwise.
			 */
			rx->equerr = (short)(mag < 0 ? 0x7fff : mag >> 16);
			rx->preerr = (short)(rx->preerr_acc < 0 ? 0x7fff
							: rx->preerr_acc >> 16);
			rx->equerr_accum = 0;
			rx->preerr_acc = 0;
			rx->f248 = rx->sig_energy_acc >> 8;
			rx->sig_energy_acc = 0;

			if (rx->rx_blocks <= 0x7530 && DSPLIB_DEBUG_ON()) {
				dsplibs_debug_printf(
					"V34EQU, equerr = %d, preerr = %d,\n",
					(int)rx->equerr, (int)rx->preerr);
				flags = rx->flags;
			}
		}

		if (flags & V34_RX_FLAG_DATA)
			V34EqualizerAdapt(eq, (short)er, (short)ei);
		else
			V34EqualizerCenterAdapt(eq, (short)(er * 2),
						(short)(ei * 2));
	}
}

/*
 * ---------------------------------------------------------------------------
 * Where `struct v34_receiver` is pinned.
 *
 * Every field `receiver` added is bounded by an explicit pad, so a future
 * edit that miscounts one moves the next field somewhere else and compiles
 * clean.  `pred_a` is the load-bearing one: the pre-TRN reset zeroes twelve
 * bytes from `pred_b` and relies on `pred_a` abutting it, and `timing_out`
 * being seven entries rather than twenty-one is the only thing keeping the
 * metric array out of the coefficients (finding F141).  So its SIZE is
 * asserted too -- the offsets either side would still line up if it were
 * declared [7] and the pad after it shrank to match, which is exactly the
 * mistake worth catching.
 *
 * Guarded to a 32-bit ABI because the struct contains pointers; the same
 * note as in dpsk.c and b103fp.c.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34RX_ASSERT(name, off) \
	typedef char v34rx_off_##name[ \
		((int)__builtin_offsetof(struct v34_receiver, name) == (off)) \
		? 1 : -1]

V34RX_ASSERT(flags,           0x122);
V34RX_ASSERT(out_count,       0x128);
V34RX_ASSERT(rms_buf,         0x13c);
V34RX_ASSERT(trn_ref_sr,      0x1a0);
V34RX_ASSERT(scrambler_sr,    0x1a4);
V34RX_ASSERT(cloop_integrator, 0x1f8);
V34RX_ASSERT(cloop_phase_err, 0x1fc);
V34RX_ASSERT(f208,            0x208);
V34RX_ASSERT(target_re,       0x210);
V34RX_ASSERT(pred_err_re,     0x214);
V34RX_ASSERT(pred_err_im,     0x216);
V34RX_ASSERT(preerr_acc,      0x228);
V34RX_ASSERT(sig_energy_acc,  0x24c);
V34RX_ASSERT(eq_out_i1,       0x268);
V34RX_ASSERT(eq_out_q2,       0x26e);
V34RX_ASSERT(timing_out,      0x27a);
V34RX_ASSERT(pred_b,          0x288);
V34RX_ASSERT(pred_a,          0x28e);
V34RX_ASSERT(pred_i,          0x294);
V34RX_ASSERT(pred_q,          0x29c);
V34RX_ASSERT(fir_coeff,       0x2a4);
V34RX_ASSERT(rtncount,        0x798);

typedef char v34rx_timing_out_len[
	(sizeof ((struct v34_receiver *)0)->timing_out == 14) ? 1 : -1];

/*
 * The equaliser is reached by offset rather than declared, so the constant
 * has to be checked against something.  It sits between `fir_coeff` and `rtncount`
 * and is 0x3cc bytes, which is exactly the gap.
 */
typedef char v34rx_eq_extent[
	(V34_RX_EQ_OFFSET + (int)sizeof(struct v34_equalizer)
	 == (int)__builtin_offsetof(struct v34_receiver, rtncount)) ? 1 : -1];

#endif
