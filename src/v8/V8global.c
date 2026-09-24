/*
 * V8global.c -- the leaf helpers the whole handshake is built on.
 *
 * The object's V8global.c is the unit between V8Interface.c and V8.c and
 * holds the shared signal and buffer plumbing: charFlip and the nibble
 * reverse, the Q14 multiply, absolute value and CRC bit step, the two staging
 * copies between the rings and the working buffers, the receive AGC and its
 * adaptation, the transmit/receive buffer arming and the transmit-sequence
 * bit reader.  Its only locals are `wordFlip` (charFlip) and the two entrance
 * filters and sqrt_table (V8agc), which is what bounds the unit; the
 * functions after it are proven to be outside V8.c because the object calls
 * them out of line rather than inlining them.
 *
 * V8agc is one pass over each block of four samples: take them out of the
 * symbol buffer, run them through a 40-tap filter chosen by which end of the
 * call this is, apply the current gain with saturation, and adapt that gain
 * from the energy just measured.
 *
 * The adaptation at the end is v8_agcadapt with one substitution -- the level
 * comes from the energy of these four samples rather than from the field that
 * function reads.  They are the two halves of one int, which is why that
 * field is a short of its own and not part of a wider one.
 */

#include "dsplib/debug.h"
#include "dsplib/v8.h"

static const short agc_taps_answer[40] = {
	   -15,      5,     27,     33,      0,    -62,    -93,    -26,
	   122,    218,    108,   -197,   -445,   -310,    270,    886,
	   842,   -323,  -2358,  -4317,  11279,  -4317,  -2358,   -323,
	   842,    886,    270,   -310,   -445,   -197,    108,    218,
	   122,    -26,    -93,    -62,      0,     33,     27,      5
};

static const short agc_taps_caller[40] = {
	    25,      6,    -11,    -12,      5,     13,    -37,   -156,
	  -257,   -181,    165,    654,    932,    642,   -245,  -1285,
	 -1776,  -1250,    122,   1560,   2170,   1560,    122,  -1250,
	 -1776,  -1285,   -245,    642,    932,    654,    165,   -181,
	  -257,   -156,    -37,     13,      5,    -12,    -11,      6
};

static const short agc_gain[192] = {
	 16384,  16511,  16638,  16763,  16888,  17011,  17134,  17256,
	 17377,  17498,  17617,  17736,  17854,  17971,  18087,  18203,
	 18317,  18432,  18545,  18658,  18770,  18881,  18992,  19102,
	 19211,  19320,  19429,  19536,  19643,  19750,  19856,  19961,
	 20066,  20170,  20274,  20377,  20480,  20582,  20683,  20784,
	 20885,  20985,  21085,  21184,  21283,  21381,  21479,  21577,
	 21673,  21770,  21866,  21962,  22057,  22152,  22246,  22341,
	 22434,  22528,  22620,  22713,  22805,  22897,  22988,  23079,
	 23170,  23260,  23350,  23440,  23529,  23618,  23707,  23795,
	 23883,  23971,  24058,  24145,  24232,  24318,  24404,  24490,
	 24576,  24661,  24746,  24830,  24914,  24999,  25082,  25166,
	 25249,  25332,  25415,  25497,  25579,  25661,  25742,  25824,
	 25905,  25986,  26066,  26147,  26227,  26307,  26386,  26465,
	 26545,  26624,  26702,  26781,  26859,  26937,  27014,  27092,
	 27169,  27246,  27323,  27400,  27476,  27553,  27629,  27704,
	 27780,  27855,  27930,  28005,  28080,  28155,  28229,  28303,
	 28377,  28451,  28525,  28598,  28672,  28745,  28817,  28890,
	 28963,  29035,  29107,  29179,  29251,  29322,  29394,  29465,
	 29536,  29607,  29678,  29748,  29819,  29889,  29959,  30029,
	 30099,  30168,  30238,  30307,  30376,  30445,  30514,  30583,
	 30651,  30720,  30788,  30856,  30924,  30991,  31059,  31126,
	 31194,  31261,  31328,  31395,  31461,  31528,  31595,  31661,
	 31727,  31793,  31859,  31925,  31990,  32056,  32121,  32186,
	 32251,  32316,  32381,  32446,  32510,  32575,  32639,  32703
};

/*
 * Reverse the bits of a nibble.  The original stores this rather than
 * computing it, and `charFlip` uses it twice.
 */
static const unsigned char nibble_reverse[16] = {
	0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
};

unsigned char
charFlip(unsigned char b)
{
	return (unsigned char)((nibble_reverse[b & 0x0f] << 4)
			       | nibble_reverse[b >> 4]);
}

/* Where the gain table runs out. */
#define V8_AGC_GAIN_MAX		0xbf

/* Below this the block is too quiet to adapt from. */
#define V8_AGC_FLOOR		0x1f

/* How many saturating blocks before the gain is forced back down. */
#define V8_AGC_CLIP_LIMIT	0xa

/* Q14 multiply: the product of two Q14 values, back in Q14. */
short
v8_mpyint(short a, short b)
{
	return (short)((a * b) >> 14);
}

/*
 * Absolute value, with the usual two's-complement corner left in place:
 * `v8_absfn(-32768)` is -32768, because negating it overflows and the result
 * is narrowed back to a short.  No caller reaches it -- the signal path is
 * scaled well below full scale -- so it is reproduced rather than fixed.
 */
short
v8_absfn(short x)
{
	if (x < 0)
		return (short)(-x);
	return x;
}

/*
 * One bit into the CRC-16-CCITT register the handshake carries in its state.
 * Polynomial 0x1021, MSB first, no reflection: shift up, and if the bit
 * leaving the top disagrees with the bit going in, fold the polynomial back.
 *
 * `bit` is compared 16 bits at a time, so a value whose low half is zero
 * counts as a zero bit whatever the upper half holds.
 */
void
v8_crc(struct v8_handshake *hs, int bit)
{
	unsigned int crc = (unsigned short)hs->crc;
	int msb = ((int)(short)crc) < 0 ? 1 : 0;

	crc += crc;
	if ((short)bit != 0)
		msb ^= 1;
	if (msb != 0)
		crc ^= 0x1021;
	hs->crc = (short)crc;
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

int
V8agc(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	const short *taps = v->side != 0 ? agc_taps_caller : agc_taps_answer;
	int energy = 0;
	int gain = 0;
	int i;

	v8_rxreadqueue(v);
	r->buf = v->rx_stage + V8_QUEUE_BLOCK;

	/* The band filter, a sample at a time, shifting its own line. */
	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		int acc = 0;
		int j;

		v->agc_line[0] = v->rx_stage[i];
		for (j = 0; j < V8_AGC_TAPS; j++)
			acc += v->agc_line[j] * taps[j];
		v->rx_stage[i] = (short)(acc >> 14);

		for (j = V8_AGC_TAPS - 1; j > 0; j--)
			v->agc_line[j] = v->agc_line[j - 1];
	}

	/* Into the running history, which wraps at 36. */
	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		short at = r->hist_idx;

		r->hist_idx = (short)(at + 1);
		r->hist[at] = v->rx_stage[i];
		if (at + 1 > V8_AGC_HIST)
			r->hist_idx = 0;
	}

	/* Its energy, each term pre-scaled so the sum cannot overflow. */
	for (i = 0; i < V8_AGC_HIST; i++) {
		int x = r->hist[i];

		energy += ((x * 0x38e) >> 15) * x;
	}

	if (energy != 0) {
		unsigned acc = (unsigned)energy;
		int shift = 0;
		int e;
		int idx;

		/* Normalise, remembering by how much. */
		while (acc <= 0x1fffffff) {
			acc += acc;
			shift++;
		}
		e = (int)(acc >> 15);
		if (shift & 1)
			e = (unsigned short)e >> 1;

		idx = (((unsigned short)e + 0x40) >> 7) - 0x40;
		if ((unsigned short)idx > V8_AGC_GAIN_MAX)
			idx = V8_AGC_GAIN_MAX;

		gain = (unsigned short)agc_gain[(unsigned short)idx]
		       >> (shift >> 1);
	}

	/*
	 * Apply the gain, saturating rather than wrapping.  A run of
	 * saturating blocks forces the gain back to a known value, and if the
	 * handshake was waiting on the tone detector it is restarted -- the
	 * clip means whatever it thought it heard was an artefact.
	 */
	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		int scaled = r->gain * v->rx_stage[i];
		unsigned top = (unsigned)scaled >> 25;

		if (top == 0 || top == 0x7f) {
			v->rx_stage[i] = (short)(scaled >> 10);
			continue;
		}
		v->rx_stage[i] = (short)((scaled >> 16) > 0 ? 0x7f00 : 0x8100);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V8AGC, overflow = 0x%x,\n",
					     scaled >> 16);

		r->clip_count = (short)(r->clip_count + 1);
		if (r->clip_count != V8_AGC_CLIP_LIMIT)
			continue;
		r->gain = 0x400;
		if (v->rx_substate != 0x24)
			continue;
		v->rx_substate = 0x19;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V8: Due to overflow, looking "
					     "for ANSam again...\n");
		v->detector.counter = 0;
		v->detector.warmup = 0;
		r->clip_count = 0;
	}

	if ((short)gain <= V8_AGC_FLOOR)
		return 0;

	/* Adapt, from the energy of the four samples just scaled. */
	energy = 0;
	for (i = 0; i < V8_QUEUE_BLOCK; i++)
		energy += v->rx_stage[i] * v->rx_stage[i];
	r->energy_lo = (short)energy;
	r->energy_hi = (short)(energy >> 16);

	return v8_agcadapt(v);
}

void
checkSignalStability(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	int elapsed = (unsigned short)r->refresh_timer + 4;
	int settled;
	int delta;

	if ((short)elapsed > V8_STABLE_PERIOD) {
		/*
		 * Time to refresh the reference.  Note that the comparison
		 * below then measures the gain against the value just taken
		 * from it, so it is always zero on this pass -- the original
		 * does the store first and the arithmetic afterwards.
		 */
		r->refresh_timer = 0;
		r->gain_ref = r->gain;
	} else {
		r->refresh_timer = (short)elapsed;
	}

	/*
	 * The relative change, in Q14.  A zero reference would divide by zero
	 * in the original; nothing reaches it, because the gain is only ever
	 * this function's reference after having been non-zero.
	 */
	if (r->gain_ref == 0)
		delta = 0;
	else
		delta = (short)((((int)r->gain - r->gain_ref) << 14) / r->gain_ref);

	if (delta < 0)
		delta = -(short)delta;

	if ((short)delta > V8_STABLE_TOLERANCE) {
		r->stable_timer = 0;
		r->stable = 0;
		return;
	}

	settled = (unsigned short)r->stable_timer + 4;
	r->stable_timer = (short)settled;
	if ((short)settled > V8_STABLE_PERIOD)
		r->stable = 1;
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
 * Hand out the next bit of a sequence, least significant first.
 *
 * The sequence is 10-bit characters; this is what turns them into a bit
 * stream. A shift register holds whatever has been loaded and not yet handed
 * out, `nleft` says how much of it is still owed, and each load folds the new
 * bits into the CRC.
 *
 * Three things happen at the end of a message, in this order: the finished
 * CRC is appended as sixteen more bits, then four more bits of ones, and only
 * then does the sequence either repeat from the top or report that it is
 * done.  The two overruns are recognised by how far past the length the bit
 * position has gone, which is why `remaining` is allowed to go negative
 * rather than being clamped.
 */
int
v8_getbit(struct v8_tx_sequence *s)
{
	int remaining;
	int loaded = 0;
	int pos;

	if (s->nleft != 0)
		goto emit;

	pos = (unsigned short)s->bitpos;
	remaining = (short)((unsigned short)s->nbits - (unsigned short)pos);

	if (remaining <= 0) {
		if (remaining == 0 && s->crc_enable != 0) {
			/* The CRC itself, sixteen bits of it. */
			s->shifter = (unsigned short)s->crc;
			s->nleft = 16;
			s->bitpos = (short)(pos + 16);
			goto emit;
		}
		if (remaining == -16) {
			/* Four ones behind the CRC. */
			s->shifter = 0xf;
			s->nleft = 4;
			s->bitpos = (short)(pos + 4);
			goto emit;
		}
		if (s->repeat == 0)
			return V8_GETBIT_END;

		s->crc = (short)0xffff;
		s->bitpos = 0;
		s->wordidx = 0;
		s->repeats = (short)(s->repeats + 1);
		s->shifter = s->shifter0;
		s->nleft = s->nleft0;
		return (short)v8_getbit(s);
	}

	/*
	 * Load a whole character, or whatever is left of one when the message
	 * ends part way through.
	 */
	loaded = (unsigned short)s->wordbits;
	if (loaded > remaining)
		loaded = remaining;

	s->nleft = (short)loaded;
	s->shifter = (s->shifter << loaded)
		     | (unsigned short)s->word[(unsigned short)s->wordidx];
	if ((unsigned short)s->wordbits <= (unsigned)remaining)
		s->wordidx = (short)(s->wordidx + 1);
	s->bitpos = (short)(pos + loaded);

	if (s->crc_enable != 0 && loaded != 0) {
		int c = loaded;

		do {
			unsigned crc = (unsigned short)s->crc;
			int msb = (short)crc < 0 ? 1 : 0;

			c--;
			if ((s->shifter >> c) & 1)
				msb ^= 1;
			crc += crc;
			if (msb)
				crc ^= 0x1021;
			s->crc = (short)crc;
		} while (c != 0);
	}

emit:
	s->nleft = (short)(s->nleft - 1);
	return (s->shifter >> (short)s->nleft) & 1;
}

/* Copy `n` coefficients.  The counter is a short, so `n` above 32767 never
 * terminates -- no caller comes close. */
void
v8_copycoeff(short *dst, const short *src, short n)
{
	short i;

	for (i = 0; i < n; i++)
		dst[i] = src[i];
}

/*
 * Arm the transmitter.  Three buffers are cleared and four pointers set to
 * point inside them: the symbol buffer gets two pointers to its start, and
 * the ring gets one to its start and one to the sixty-fourth sample -- a read
 * and a write cursor half a buffer apart, which is how the shaping filter is
 * kept fed while the modulator drains behind it.
 */
int
v8_txinit(struct v8 *v)
{
	int i;

	v->short_014 = 1;
	v->short_00c = 0;
	v->short_018 = 0;
	v->int_004 = 0;

	for (i = 0; i < V8_TX_SHAPE; i++)
		v->tx_shape[i] = 0;

	v->tx_ring_base = v->tx_ring;
	for (i = 0; i < V8_TX_RING; i++)
		v->tx_ring[i] = 0;

	v->tx_avail = 0x20;
	v->tx_ring_half = v->tx_ring + V8_TX_RING_HALF;

	v->tx_sym_a = v->tx_symbols;
	v->tx_sym_b = v->tx_symbols;
	v->sym_avail = 0;
	for (i = 0; i < V8_TX_SYMBOLS; i++)
		v->tx_symbols[i] = 0;

	return 0;
}

/*
 * Arm the receiver.  Note the order at the top: the scratch buffer is cleared
 * and then one element of it is written again.  Reproduced as written --
 * seeding after the clear is what the original does, and doing it the tidy
 * way round would be the same result only by luck of the index.
 */
int
v8_rxinit(struct v8 *v)
{
	int i;

	for (i = 0; i < V8_RX_SCRATCH; i++)
		v->rx_scratch[i] = 0;
	v->rx_scratch[V8_RX_SCRATCH_SEED_INDEX] = V8_RX_SCRATCH_SEED;

	v->rx.gain_ref = 0x200;
	v->rx.hist_idx = 0;
	v->rx.gain = 0x200;
	v->rx.adapt_rate = 0x3333;

	for (i = 0; i < V8_RX_HIST; i++)
		v->rx.hist[i] = 0;

	v->rx.accum = 0;
	v->rx.refresh_timer = 0;
	v->rx.stable_timer = 0;
	v->rx.stable = 0;
	v->rx.fc2 = 0x50;
	v->rx.fc8 = 0;
	v->rx.fc6 = 0;
	v->rx.fda = 0;
	v->rx.fd8 = 0;

	v->rx.buf = v->rx_stage;
	v->rx.level = 0;
	/*
	 * One 32-bit store in the original, covering both halves.  They are
	 * two shorts here because v8_agcadapt reads the upper one on its own.
	 */
	v->rx.energy_lo = 0;
	v->rx.energy_hi = 0;
	v->rx.clip_count = 0;

	return 0;
}
