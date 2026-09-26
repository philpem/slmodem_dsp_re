/*
 * V34TX.c -- ITU-T V.34: the transmitter core.
 *
 * Split out of the shared V34RX.c/V34TX.c reconstruction.  The blob's FILE
 * order is `V34RX.c`(90), `V34TX.c`(91), `V34hshak.c`(92); `V34RX.c`'s
 * `V34demodulate` is a LOCAL FUNC at 0x05af10 and `V34hshak.c`'s
 * `ApplyBulkDelay` a LOCAL FUNC at 0x05dd10, so the run between them is the
 * RX/TX pair.  The boundary is the one `updateAlpha` pins: the object INLINES
 * `updateAlpha` into `adaptecho` (the `test $0x40000000` fold and the
 * `updateAlpha%s` string are both inside adaptecho's body) while `modem_serrint`
 * only CALLS it, so `updateAlpha` shares a translation unit with `adaptecho`
 * and not with `modem_serrint`; the unit is therefore
 * [updateAlpha 0x05d5c0, V34hshak.c), i.e. V34TX.c.
 *
 * Emission order is the object's address order.  Every body moved VERBATIM.
 * `q_next` is the one file-static helper both units use, so it is reproduced
 * here as it is in V34RX.c; `bulk_next` travels with its only caller, txmit.
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

/* The bulk ring's wrap: reset to zero, not subtract.  See finding F116. */
static int
bulk_next(int idx, int len)
{
	idx++;
	return idx & -(int)((unsigned)len > (unsigned)idx);
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
void
txinit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->echo_alpha = 0;
	obj->far_echo_alpha = 0;
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
	if ((obj->tx_flags & V34_EC_FEED) == 0)
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
				dsplibs_debug_printf("V90NEC, --------ERROR---" "------ occured in "
						     "adaptecho\n");
			return 0;
		}

		V34EchoAdapt(&obj->echo0, err);
	}

	return 0;
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
