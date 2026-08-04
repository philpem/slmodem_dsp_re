/*
 * t_v34demod.c -- V34demodulate, driven directly instead of through rxtiming.
 *
 * The last of finding 221's fifteen.  t_v34rx says why it matters, in the
 * comment above its V34agc block: "V34demodulate is a local symbol and can
 * only be reached through the interpolator, so every AGC defect it had
 * presented as a loop-shape failure."  That was true of the symbol table and
 * is not true any more -- --globalize-symbols promotes it and the rename map
 * then applies, so `ref_V34demodulate` links.
 *
 * THE CONVENTION, BECAUSE FINDING 51 SAYS TO CHECK
 *
 * "A `t` symbol is not a testing inconvenience, it is a signal that the
 * calling convention may not be the C one."  So:
 *
 *     5af10:  push   %ebp
 *     5af11:  push   %edi
 *     5af12:  mov    %eax,%edi        <- the receiver, in eax, before the
 *     5af14:  push   %esi                frame exists
 *     5af19:  mov    0x4(%eax),%ecx      (q->rd)
 *     5af1c:  movzwl (%eax),%edx         (q->count)
 *
 * and from rxtiming, which is the same statement from the caller's side:
 *
 *     5b491:  mov    %esi,%eax
 *     5b493:  call   5af10 <V34demodulate>
 *
 * -- one argument, in %eax, nothing on the stack.  `regparm(1)`, which is
 * what GCC picks for a static function whose callers it can all see.  Our own
 * copy has external linkage now and the ordinary convention.
 *
 * THE FIXTURE
 *
 * `rxinit` is the constructor to lean on, exactly as `x_create` is for the
 * four datapumps: it is global on both sides, t_v34rx already drives it, and
 * it settles the AGC and the phase accumulator.  What it does NOT settle is
 * the carrier table -- that is chosen elsewhere, by symbol rate -- so both
 * sides are pointed at ONE table declared here.  That is deliberate: a table
 * per side would compare two arithmetics on two inputs, and which coefficients
 * belong to which symbol rate is a different test's subject.  Pointing both at
 * the same array makes any divergence below the arithmetic's own.
 *
 * POINTERS
 *
 * Three, all of them aiming inside the receiver: the receive queue's `rd` and
 * `wr` cursors, and `rx_samples`, which advances by one short per call.  Each
 * is replaced by its byte offset from the receiver base, which is stronger
 * than skipping it -- t_v34rx skips `rx_samples` in four places, and a cursor
 * left one short behind is exactly the defect that field can have.  `carrier`
 * is the same pointer on both sides by construction, and `f2a4` is left at
 * the fill on both, so neither needs anything.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34rx.h"
#include "dsplib/v34recv.h"

/* See the header comment: one argument, in %eax. */
extern void ref_V34demodulate(struct v34_receiver *rx)
	__attribute__((regparm(1)));

extern void ref_rxinit(void *obj);

/* The receiver's offset inside the object, as t_v34rx spells it. */
#define RX(o)	((struct v34_receiver *)((char *)(o) + 0x264))

static struct v34_object oa, ob;
static struct v34_receiver na, nb, at_start;

/*
 * One carrier table for both sides.  A half-sine of `quarter` entries and its
 * continuation, so `carrier[phase]` and `carrier[phase + quarter]` are a
 * quadrature pair the way the real tables are.
 */
#define QUARTER	0x18
static short carrier_table[2 * QUARTER + 4];

static void
build_carrier(void)
{
	unsigned i;

	for (i = 0; i < sizeof(carrier_table) / sizeof(carrier_table[0]); i++) {
		/*
		 * Q14, one cycle over 2*QUARTER entries, by a fixed-point
		 * recurrence rather than sin() so the table is the same
		 * integer on every host.
		 */
		long x = (long)(i % (2 * QUARTER));
		long t = (x * 0x10000) / (2 * QUARTER);	/* turn, Q16 */
		long s;

		/* A parabolic half-wave: exact at 0, 1/4, 1/2 and monotone. */
		if (t < 0x8000)
			s = (4L * t * (0x8000 - t)) / 0x8000;
		else
			s = -(4L * (t - 0x8000) * (0x10000 - t)) / 0x8000;
		carrier_table[i] = (short)((s * 0x4000) / 0x8000);
	}
}

static void *
selfrel(const void *p, const void *base, size_t n)
{
	const char *c = p;
	const char *b = base;

	if (c < b || c >= b + n)
		return (void *)(size_t)c;
	return (void *)(size_t)(1 + (c - b));
}

static void
normalise(struct v34_receiver *dst, const struct v34_receiver *src)
{
	struct v34_queue *dq = (struct v34_queue *)dst;
	const struct v34_queue *sq = (const struct v34_queue *)src;

	*dst = *src;
	dq->rd = selfrel(sq->rd, src, sizeof(*src));
	dq->wr = selfrel(sq->wr, src, sizeof(*src));
	dst->rx_samples = selfrel(src->rx_samples, src, sizeof(*src));
}

/*
 * Prime one side.  Everything V34demodulate reads that `rxinit` does not
 * settle, set to the same value on both sides.
 */
static void
prime(struct v34_object *o, int amp, int gain, int step, int freeze)
{
	struct v34_receiver *rx = RX(o);
	struct v34_queue *q = (struct v34_queue *)rx;
	unsigned b;

	rx->flags = (unsigned short)(freeze ? V34_RX_FLAG_AGC_FREEZE : 0);
	rx->agc_gain = (short)gain;
	rx->agc_level = 0;
	rx->agc_accum = 0;
	rx->agc_step = 0x3333;
	rx->f19c = 0;
	rx->f12a = 0;
	rx->energy.sum = 0;
	for (b = 0; b < V34_AGC_RMS_TAPS; b++)
		rx->rms_buf[b] = 0;

	rx->carrier = carrier_table;
	rx->f1ba = QUARTER;
	rx->f1b8 = (short)step;
	rx->f1bc = 0;
	rx->f240 = 0;
	rx->f242 = 0;

	/* The ring, and both cursors, as t_v34rx primes them for V34agc. */
	q->count = V34_RXQ_RING;
	for (b = 0; b < V34_RXQ_RING; b++)
		q->ring[b] = (int)(short)((b * amp) % 32768
					  - (int)(b & 1) * amp);
	q->rd = q->ring;
	q->wr = q->ring;

	/* Where rxtiming puts it at the top of every call. */
	rx->rx_samples = (short *)((char *)rx + 0x10c);
}

int
main(void)
{
	int rc = 0;
	int amp, gain, step, freeze;
	long distinct_out = 0, moved = 0, cases = 0;

	build_carrier();

	diff_begin("V34demodulate: the alias links");
	diff_eq_int("ref_V34demodulate resolves (%ld)",
		    (void *)ref_V34demodulate != 0, 1, 0);
	diff_eq_int("and it is not our own (%ld)",
		    (void *)ref_V34demodulate != (void *)V34demodulate, 1, 0);
	rc |= diff_end();

	/*
	 * The sweep has to cross what decides anything inside: the AGC freeze
	 * flag, the RMS floor at 31 -- below which the loop will not adapt at
	 * all -- and the every-fourth-pair counter, which needs more than four
	 * calls per fixture to turn over.  Amplitudes run from silence to
	 * clipping for the floor, and the gain from unity to well past it so
	 * that `agc_gain_sample` saturates on some of them.
	 */
	diff_begin("V34demodulate: the whole receiver, per call");
	for (freeze = 0; freeze <= 1; freeze++)
	for (amp = 0; amp < 32767; amp += 6073)
	for (gain = 0x100; gain < 0x7000; gain += 0x1a01)
	for (step = 1; step <= 7; step += 3) {
		int burst, k;
		long tag = ((long)freeze * 100 + amp / 6073) * 10000
			 + (long)(gain / 0x1a01) * 100 + step;

		memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
		memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
		RX(&oa)->flags = RX(&ob)->flags = 0;
		rxinit(&oa);
		ref_rxinit(&ob);
		prime(&oa, amp, gain, step, freeze);
		prime(&ob, amp, gain, step, freeze);

		normalise(&at_start, RX(&oa));
		cases++;

		/*
		 * Four bursts of six, with `rx_samples` reset between them the
		 * way rxtiming resets it -- twenty-four calls, so the
		 * every-fourth-pair adapt runs six times and the 36-entry RMS
		 * window fills.
		 */
		for (burst = 0; burst < 4; burst++) {
			RX(&oa)->rx_samples =
				(short *)((char *)RX(&oa) + 0x10c);
			RX(&ob)->rx_samples =
				(short *)((char *)RX(&ob) + 0x10c);

			for (k = 0; k < 6; k++) {
				V34demodulate(RX(&oa));
				ref_V34demodulate(RX(&ob));

				normalise(&na, RX(&oa));
				normalise(&nb, RX(&ob));
				diff_eq_obj("after V34demodulate",
					    struct v34_receiver, &na, &nb,
					    tag + burst * 10 + k);

				if (RX(&oa)->f240 != 0 || RX(&oa)->f242 != 0)
					distinct_out++;
				if (memcmp(&na, &at_start, sizeof(na)) != 0)
					moved++;
			}
		}
	}
	rc |= diff_end();

	/*
	 * Anti-vacuity.  A whole-object comparison is satisfied by two objects
	 * that never moved, and the down-mix is satisfied by two zeroes.
	 */
	diff_begin("guards");
	diff_eq_int("fixtures were built (%ld)", cases > 100, 1, cases);
	diff_eq_int("the receiver moved (%ld)", moved > 0, 1, moved);
	diff_eq_int("the mixer produced something (%ld)", distinct_out > 0, 1,
		    distinct_out);
	rc |= diff_end();

	return rc;
}
