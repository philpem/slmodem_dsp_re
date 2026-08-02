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
decision(struct v34_decoder *d, const int *pts, short npts)
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
	d->best_point = *best;
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
	    int tag)
{
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
			dsplibs_debug_printf("updateAlpha %d %d\n", tag,
					     *alpha);
	}

	if (apply_decay != 0)
		*alpha = (short)((*alpha * decay + 0x4000) >> 15);
}

int
V34descrambler(struct v34_scrambler *s, short bits, short nbits)
{
	unsigned sr = s->sr;
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

	s->sr = sr;
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
