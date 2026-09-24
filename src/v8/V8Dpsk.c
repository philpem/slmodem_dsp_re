/*
 * V8Dpsk.c -- the V.21 modem's state and its receive correlator.
 *
 * The object's own unit owns the V.21 delay line and filter selection
 * (`V8_V21_reset`, `V8_setFilters`) and the filter side of the signal path
 * (`v8_fskdemodulate`, `v8_fsktxfilter`).  V8Fsk.c holds the initialiser and
 * the modulator, which call back into here -- the object keeps those cross
 * calls out of line, so the two units are separate.
 */

#include <string.h>

#include "dsplib/v8.h"

/*
 * Clear the V.21 delay line and the three accumulators behind it.
 *
 * The counter is UNSIGNED because the object's loop test is `cmp $0x27` +
 * `jbe`, and the signedness of a comparison is something the compiler was
 * forced to encode: a signed `int i` gives `jle` here and everything else in
 * the function byte for byte.  It cannot change behaviour -- the counter runs
 * 0..39 and the two readings agree over every value it holds -- so no
 * differential test can see it, which is why it is settled against the
 * instruction and not against a test.  Finding F2952.
 */
void
V8_V21_reset(struct v8 *v)
{
	unsigned i;

	for (i = 0; i < V8_V21_DELAY; i++)
		v->v21.delay[i] = 0;
	v->v21.pos = 0;
	v->v21.space_run = 0;
	v->v21.mark_run = 0;
}

/*
 * Point the V.21 modem at a set of filter designs.  The four are swapped
 * together, which is how one modem serves both channels of V.21: the
 * handshake calls this again whenever it changes direction.
 */
void
V8_setFilters(struct v8 *v, const short *a, const short *b, const short *c,
	      const short *d)
{
	v->v21.a = a;
	v->v21.b = b;
	v->v21.c = c;
	v->v21.d = d;
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