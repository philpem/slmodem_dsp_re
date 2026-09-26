/*
 * V34RX.c -- ITU-T V.34: the receiver and transmitter cores.
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
	rx->timing_frac = 0;
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

	if (!(rx->flags & V34_RX_FLAG_LATE_TRN)) {
		rx->cloop_integrator = 0;
		rx->cloop_p_shift = 2;
		rx->cloop_i_shift = 8;
	} else {
		rx->cloop_p_shift = 2;
		rx->cloop_i_shift = 10;	/* and cloop_integrator is left alone */
	}

	rx->rtncount = 0;   rx->err_symcount = 0;  rx->cloop_phase_hi = 0;  rx->equerr = 0;
	rx->cloop_phase_lo = 0;   rx->preerr = 0;  rx->equerr_accum = 0;  rx->preerr_acc = 0;
	rx->trn_ref_sr = 0;   rx->demod_q_prev = 0;  rx->rx_blocks = 0;  rx->scrambler_sr = 0;
	rx->demod_i_prev = 0;   rx->mix_carrier_phase = 0;  rx->energy.sum = 0;
	rx->vectpp_idx = 0;   rx->agc_pair_count = 0;  obj->hist1_idx = 0;
	rx->rx_samples = (short *)((char *)rx + 0x10c);
	rx->sig_energy = 0;   rx->sig_energy_acc = 0;
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
 *
 * LOCAL in the blob, so `static` here; the differential test declares it
 * `regparm(1)` and reaches it through the globalized test copy
 * (tools/testvisible.py).
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
			updateAlpha(&obj->far_echo_alpha, far_energy, far_step, 0x2b84,
				    0x7f5c, "FE");
	}

	obj->echo0.adapt_count = (short)(obj->echo0.adapt_count + 1);
	near_err = (short)((((int)obj->echo_alpha * out) * 2 + 0x2000) >> 14);

	if (obj->far_echo_enable != 0) {
		obj->echo1.adapt_count = (short)(obj->echo1.adapt_count + 1);
		far_err = (short)((((int)obj->far_echo_alpha * out) * 2 + 0x2000)
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
 *
 * GLOBAL (`R`) in the object, so it is not `static` here.  Finding F2802
 * shows the object's own definition is in the handshake unit and this unit
 * sees a declaration only; matching that placement is a separate, recorded
 * change, and this one is the binding alone.
 */
const int rxvect4[4] = {
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
			rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_TRAINED);
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
			rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_TRAINED);
		}

		/*
		 * A narrow band, not a threshold: the message fires only for
		 * -70 < rtncount < -64.  Below -64 the flag is set regardless.
		 */
		if ((short)rx->rtncount < -64) {
			rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_TRAINED);
			if ((short)rx->rtncount > -70 && DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V34RENEG, may be renegotiation," "equalizer adaptation disabled\n");
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
				    "Detected shifted TRN2... assuming 32 " "symbols Snot\n");
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
					    "V34RETRAIN, retrain request " "detected, rtncount = %d \n",
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
					    "V34RENEG, RRN request detected," "rtncount = %d\n", (int)n);
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
				"V34AGC, abcddetect gain = 0x%x, " "AGC frozen\n", (int)rx->agc_gain);
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
					    "S-S1 is detected,rxsymcnt= %d," "pllcnt= %d,gain= 0x%x\n",
					    (int)n, (int)rx->pllcnt,
					    (int)rx->agc_gain);
				rx->rx_blocks = 0;
				flags = (rx->flags & ~V34_RX_FLAG_TRAINED)
					| (V34_RX_FLAG_DATA | V34_RX_FLAG_DET_PENDING);
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
					    "Signal Energy below Threshold " "%d, initiate a disconnection",
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
	 * The carrier loop.  target_re/target_im STOP being the received point here and
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
			rx->sig_energy = rx->sig_energy_acc >> 8;
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
 * note as in DPSK.c and b103fp.c.
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
