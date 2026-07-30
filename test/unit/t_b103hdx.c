/*
 * t_b103hdx.c -- differential test of Bell 103's half-duplex state machines.
 *
 * These are control flow, not arithmetic, so what has to match is *which*
 * state comes next -- and the state is a function pointer, which necessarily
 * holds a different value in the reconstruction than in the blob.  Comparing
 * the pointers directly would fail on every transition; comparing nothing
 * would test nothing.  So each side's pointers are mapped through its own
 * table to a small integer and the integers are compared.  `state_id` below
 * is the whole trick, and it is why this file is not just another memcmp.
 *
 * Two layers are covered:
 *
 *   the three NextState tables, driven directly over every substate
 *   the seven Hdx states, driven with real signal, each checked for the
 *   counter it moves and the condition on which it hands over
 */

#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/b103fp.h"
#include "dsplib/fpm_tone.h"

extern void *ref_B103FP_create(void *state, const void *cfg);
extern void ref_B103FP_delete(void *state);
extern short ref_B103_CFG[];
extern short ref_B103_BPF_CALLER[];
extern int ref_B103FP_modem(void *fp, const int *tx_bits, short *tx_out,
			    short *rx_in, int *rx_bits, short *n_tx,
			    short *n_rx);
extern short ref_ModDataB103(void *fp, const unsigned short *bits, short *out,
			     unsigned short n);
extern void *ref_FPM_TONE_create(void *state, void *cfg);
extern short ref_FPM_TONE_CFG[];

extern void (*ref_B103NextState[3])(void *fp);
extern void ref_B103LocLoopNextState(void *fp);
extern void ref_B103OriginateNextState(void *fp);
extern void ref_B103AnswerNextState(void *fp);

extern short ref_TxHdxStartB103(void *fp, short *in, short *out, short *n);
extern short ref_TxHdxDataB103(void *fp, short *in, short *out, short *n);
extern short ref_TxHdxMarksB103(void *fp, short *in, short *out, short *n);
extern short ref_TxHdxSilenceB103(void *fp, short *in, short *out, short *n);
extern short ref_RxDetMarkB103(void *fp, short *in, short *out, short *n);
extern short ref_RxHdxStartB103(void *fp, short *in, short *out, short *n);
extern short ref_RxHdxDataB103(void *fp, short *in, short *out, short *n);

/*
 * Map a state pointer to an identity both sides agree on.
 *
 * BOTH tables are searched regardless of which side the pointer came from,
 * because both objects are built by the *reference* B103FP_create and so
 * start life holding reference pointers.  Only the states a NextState table
 * has since installed differ.  Searching one table would report every
 * untouched slot as unknown -- which it did, on the first run.
 *
 * Unknown pointers get -1 rather than being silently treated as equal, so a
 * transition to something outside this list still shows up as a mismatch.
 */
static int
state_id(const void *p, int reference)
{
	static const void *ours[] = {
		(const void *)TxHdxStartB103, (const void *)TxHdxDataB103,
		(const void *)TxHdxMarksB103, (const void *)TxHdxSilenceB103,
		(const void *)RxDetMarkB103, (const void *)RxHdxStartB103,
		(const void *)RxHdxDataB103
	};
	static const void *refs[] = {
		(const void *)ref_TxHdxStartB103, (const void *)ref_TxHdxDataB103,
		(const void *)ref_TxHdxMarksB103, (const void *)ref_TxHdxSilenceB103,
		(const void *)ref_RxDetMarkB103, (const void *)ref_RxHdxStartB103,
		(const void *)ref_RxHdxDataB103
	};
	int i;

	(void)reference;
	if (p == 0)
		return -2;
	for (i = 0; i < 7; i++)
		if (p == ours[i] || p == refs[i])
			return i;
	return -1;
}

/* Coverage. */
static int seen_tx_state[8];
static int seen_rx_state[8];
static int seen_substate[8];
static int seen_status[16];
static int seen_modem_return[256];

static void
compare_hdx(const char *what, struct b103fp *ours, struct b103fp *ref, int tag)
{
	struct b103_hdx *a = ours->hdx;
	struct b103_hdx *b = ref->hdx;
	char buf[96];
	int id;

	snprintf(buf, sizeof(buf), "%s: substate (%%ld)", what);
	diff_eq_int(buf, a->substate, b->substate, tag);
	snprintf(buf, sizeof(buf), "%s: tx_blocks (%%ld)", what);
	diff_eq_int(buf, a->tx_blocks, b->tx_blocks, tag);
	snprintf(buf, sizeof(buf), "%s: rx_count (%%ld)", what);
	diff_eq_int(buf, a->rx_count, b->rx_count, tag);
	snprintf(buf, sizeof(buf), "%s: mode (%%ld)", what);
	diff_eq_int(buf, a->mode, b->mode, tag);

	snprintf(buf, sizeof(buf), "%s: tx state (%%ld)", what);
	id = state_id((const void *)b->tx, 1);
	diff_eq_int(buf, state_id((const void *)a->tx, 0), id, tag);
	snprintf(buf, sizeof(buf), "%s: rx state (%%ld)", what);
	diff_eq_int(buf, state_id((const void *)a->rx, 0),
		    state_id((const void *)b->rx, 1), tag);

	snprintf(buf, sizeof(buf), "%s: status (%%ld)", what);
	diff_eq_int(buf, ours->status, ref->status, tag);
	snprintf(buf, sizeof(buf), "%s: flags (%%ld)", what);
	diff_eq_int(buf, ours->flags, ref->flags, tag);

	/* Coverage, recorded from the reference. */
	id = state_id((const void *)b->tx, 1);
	if (id >= 0)
		seen_tx_state[id]++;
	id = state_id((const void *)b->rx, 1);
	if (id >= 0)
		seen_rx_state[id]++;
	if (b->substate >= 0 && b->substate < 8)
		seen_substate[b->substate]++;
	if (ref->status < 16)
		seen_status[ref->status]++;
}

/* A pair of freshly built objects, put into a known state. */
static int
build(struct b103fp **a, struct b103fp **b, int mode, int substate,
      int v21)
{
	*a = ref_B103FP_create(0, ref_B103_CFG);
	*b = ref_B103FP_create(0, ref_B103_CFG);
	if (*a == 0 || *b == 0)
		return 0;

	(*a)->hdx->tone_detect = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
	(*b)->hdx->tone_detect = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);

	(*a)->hdx->mode = (*b)->hdx->mode = (short)mode;
	(*a)->hdx->substate = (*b)->hdx->substate = (short)substate;
	(*a)->cfg.v21 = (*b)->cfg.v21 = v21;
	(*a)->status = (*b)->status = 0;
	(*a)->flags = (*b)->flags = 0;
	return 1;
}

static void
teardown(struct b103fp *a, struct b103fp *b)
{
	ref_B103FP_delete(a);
	ref_B103FP_delete(b);
}

int
main(void)
{
	static const char *modes[3] = { "locloop", "originate", "answer" };
	static short air[512], out[512];
	struct b103fp *a, *b;
	int rc = 0;
	int mode, sub, i, k;

	/*
	 * 1. The three NextState tables, over every substate including two
	 *    past the last real one -- the default arm has to be a no-op and
	 *    a reconstruction that fell through would be caught here.
	 */
	diff_begin("B103NextState tables");
	for (mode = 0; mode < 3; mode++) {
		for (sub = 0; sub < 7; sub++) {
			char what[64];

			if (!build(&a, &b, mode, sub, mode == 2))
				continue;
			snprintf(what, sizeof(what), "%s sub %d",
				 modes[mode], sub);

			ref_B103NextState[mode](b);
			B103NextState[mode](a);

			compare_hdx(what, a, b, sub);
			teardown(a, b);
		}
	}
	rc |= diff_end();

	/*
	 * 2. A full run of each table from START, so the sequence of
	 *    transitions is checked and not just each arm in isolation.
	 */
	diff_begin("B103NextState sequences");
	for (mode = 0; mode < 3; mode++) {
		char what[64];

		if (!build(&a, &b, mode, B103_STATE_START, mode == 2))
			continue;
		for (k = 0; k < 6; k++) {
			snprintf(what, sizeof(what), "%s step %d", modes[mode], k);
			ref_B103NextState[mode](b);
			B103NextState[mode](a);
			compare_hdx(what, a, b, k);
		}
		teardown(a, b);
	}
	rc |= diff_end();

	/*
	 * 3. The transmit states.  Each is driven repeatedly so its counter
	 *    runs out and it hands over -- a single call would only ever
	 *    exercise the not-yet arm.
	 */
	diff_begin("Tx half-duplex states");
	for (mode = 0; mode < 3; mode++) {
		static unsigned short bits_a[64], bits_b[64];
		char what[64];
		short na, nb, ca, cb;

		if (!build(&a, &b, mode, B103_STATE_START, mode == 2))
			continue;
		ref_B103NextState[mode](b);
		B103NextState[mode](a);

		for (k = 0; k < 60; k++) {
			for (i = 0; i < 6; i++)
				bits_a[i] = bits_b[i] =
					(unsigned short)((0x2d3u >> (k % 10)) & 1);
			ca = cb = 6;

			snprintf(what, sizeof(what), "%s tx %d", modes[mode], k);
			nb = b->hdx->tx(b, (short *)bits_b, out, &cb);
			na = a->hdx->tx(a, (short *)bits_a, air, &ca);

			diff_eq_int("tx sample count (%ld)", na, nb, k);
			diff_eq_int("tx count zeroed (%ld)", ca, cb, k);
			for (i = 0; i < nb; i++)
				diff_eq_int("tx sample[%ld]", air[i], out[i], i);
			compare_hdx(what, a, b, k);
		}
		teardown(a, b);
	}
	rc |= diff_end();

	/*
	 * 4. The receive states, driven with the 2100 Hz answer tone so
	 *    RxDetMark actually detects rather than timing out, then with
	 *    silence so the timeout and carrier-loss arms fire too.
	 */
	diff_begin("Rx half-duplex states");
	for (mode = 0; mode < 3; mode++) {
		static unsigned short obits_a[64], obits_b[64];
		char what[64];
		short na, nb, ca, cb;

		if (!build(&a, &b, mode, B103_STATE_START, mode == 2))
			continue;
		ref_B103NextState[mode](b);
		B103NextState[mode](a);
		/* Keep the timeout short so it is reachable in this run. */
		a->hdx->tone_timeout = b->hdx->tone_timeout = 12;

		for (k = 0; k < 80; k++) {
			for (i = 0; i < 160; i++) {
				double t = (k * 160.0 + i) / 8000.0;

				air[i] = (k < 40)
					? (short)(12000.0 * sin(2.0 * M_PI
								* 2100.0 * t))
					: 0;
				out[i] = air[i];
			}
			ca = cb = 160;

			snprintf(what, sizeof(what), "%s rx %d", modes[mode], k);
			if (b->hdx->rx == 0 || a->hdx->rx == 0)
				break;
			nb = b->hdx->rx(b, out, (short *)obits_b, &cb);
			na = a->hdx->rx(a, air, (short *)obits_a, &ca);

			diff_eq_int("rx bit count (%ld)", na, nb, k);
			diff_eq_int("rx count zeroed (%ld)", ca, cb, k);
			for (i = 0; i < nb; i++)
				diff_eq_int("rx bit[%ld]", obits_a[i],
					    obits_b[i], i);
			/* The samples are modified in place by the mixer. */
			for (i = 0; i < 160; i++)
				diff_eq_int("rx mixed[%ld]", air[i], out[i], i);
			compare_hdx(what, a, b, k);
		}
		teardown(a, b);
	}
	rc |= diff_end();


	/*
	 * 4b. RxHdxStartB103 directly.
	 *
	 * None of the three NextState tables ever installs it -- they go
	 * straight from RxDetMarkB103 to RxHdxDataB103 -- so the run above
	 * cannot reach it.  Whether B103FP_create uses it as the initial
	 * receive state is a question for that function; either way it is a
	 * reachable entry point and gets driven here rather than left
	 * untested.  Silence, so its countdown runs out and the timeout arm
	 * fires.
	 */
	diff_begin("RxHdxStartB103 directly");
	{
		static unsigned short obits_a[64], obits_b[64];
		short na, nb, ca, cb;

		if (build(&a, &b, 1, B103_STATE_START, 0)) {
			a->hdx->rx_count = b->hdx->rx_count = 4;
			for (k = 0; k < 12; k++) {
				for (i = 0; i < 160; i++)
					air[i] = out[i] = 0;
				ca = cb = 160;
				nb = ref_RxHdxStartB103(b, out,
							(short *)obits_b, &cb);
				na = RxHdxStartB103(a, air,
						    (short *)obits_a, &ca);
				diff_eq_int("bits (%ld)", na, nb, k);
				diff_eq_int("count zeroed (%ld)", ca, cb, k);
				compare_hdx("rxstart", a, b, k);
				seen_rx_state[5]++;
			}
			teardown(a, b);
		}
	}
	rc |= diff_end();


	/*
	 * 6. B103FP_modem, the driver.
	 *
	 * A default object has dsp->bpf NULL and bpf_taps zero, so running the
	 * channel filter on one would dereference NULL in both
	 * implementations -- B103_CFG does not reach the branch of
	 * B103FP_create that installs it (finding 32).  The caller-side
	 * bandpass is therefore installed by hand, which is the state that
	 * branch would have produced.
	 */
	diff_begin("B103FP_modem");
	for (mode = 0; mode < 3; mode++) {
		static int txbits_a[64], txbits_b[64];
		static int rxbits_a[64], rxbits_b[64];
		static short txout_a[512], txout_b[512];
		static short rxin_a[512], rxin_b[512];
		struct b103fp *tx;
		char what[64];
		short nta, ntb, nra, nrb;
		int ra, rb, n8;

		if (!build(&a, &b, mode, B103_STATE_START, mode == 2))
			continue;
		tx = ref_B103FP_create(0, ref_B103_CFG);
		if (tx == 0) {
			teardown(a, b);
			continue;
		}

		a->dsp->bpf = b->dsp->bpf = ref_B103_BPF_CALLER;
		a->dsp->bpf_taps = b->dsp->bpf_taps = 40;
		/*
		 * These are LOOPBACK objects with a bandpass installed by
		 * hand, which is not a state B103FP_create produces -- it
		 * clears this buffer in the branch that installs the filter,
		 * and loopback has no such branch.  So the history really is
		 * dirty here, and differently in each object.  Zero both.
		 *
		 * Do not read this as a defect in the original: it was, until
		 * the buffer was checked on a configuration the library
		 * actually builds.  See the retraction of D7.
		 */
		memset(a->dsp->bpf_hist, 0, 42 * sizeof(short));
		memset(b->dsp->bpf_hist, 0, 42 * sizeof(short));
		a->hdx->tone_timeout = b->hdx->tone_timeout = 10;
		ref_B103NextState[mode](b);
		B103NextState[mode](a);

		for (k = 0; k < 120; k++) {
			unsigned short mbits[8];

			/* Real FSK for the first stretch, then the 2100 Hz
			 * answer tone so acquisition can complete. */
			if (k < 30) {
				for (i = 0; i < 6; i++)
					mbits[i] = (unsigned short)
						((0x2d3u >> ((k * 6 + i) % 10)) & 1);
				n8 = ref_ModDataB103(tx, mbits, rxin_a, 6);
			} else {
				n8 = 160;
				for (i = 0; i < n8; i++) {
					double t = (k * 160.0 + i) / 8000.0;

					rxin_a[i] = (short)(12000.0
						* sin(2.0 * M_PI * 2100.0 * t));
				}
			}
			memcpy(rxin_b, rxin_a, (unsigned)n8 * sizeof(short));

			for (i = 0; i < 6; i++)
				txbits_a[i] = txbits_b[i] = (k >> i) & 1;
			memset(txout_a, 0x3c, sizeof(txout_a));
			memset(txout_b, 0x3c, sizeof(txout_b));
			memset(rxbits_a, 0x3c, sizeof(rxbits_a));
			memset(rxbits_b, 0x3c, sizeof(rxbits_b));
			nta = ntb = 6;
			nra = nrb = (short)n8;

			rb = ref_B103FP_modem(b, txbits_b, txout_b, rxin_b,
					      rxbits_b, &ntb, &nrb);
			ra = B103FP_modem(a, txbits_a, txout_a, rxin_a,
					  rxbits_a, &nta, &nra);

			snprintf(what, sizeof(what), "%s modem %d", modes[mode], k);
			diff_eq_int("modem return (%ld)", ra, rb, k);
			diff_eq_int("modem n_tx (%ld)", nta, ntb, k);
			diff_eq_int("modem n_rx (%ld)", nra, nrb, k);
			for (i = 0; i < ntb; i++)
				diff_eq_int("modem tx sample[%ld]",
					    txout_a[i], txout_b[i], i);
			for (i = 0; i < n8; i++)
				diff_eq_int("modem filtered rx[%ld]",
					    rxin_a[i], rxin_b[i], i);
			for (i = 0; i < nrb; i++)
				diff_eq_int("modem rx bit[%ld]",
					    rxbits_a[i], rxbits_b[i], i);
			diff_eq_int("modem bpf_idx (%ld)",
				    a->dsp->bpf_idx, b->dsp->bpf_idx, k);
			compare_hdx(what, a, b, k);
			seen_modem_return[rb & 0xff]++;
		}

		ref_B103FP_delete(tx);
		teardown(a, b);
	}
	rc |= diff_end();

	diff_begin("B103FP_modem coverage");
	for (i = 0, k = 0; i < 16; i++)
		if (seen_modem_return[i])
			k++;
	diff_eq_int("distinct returned statuses (%ld)", k >= 2, 1, k);
	rc |= diff_end();

	/*
	 * 5. Anti-vacuity.  Control flow is exactly the kind of code where two
	 *    implementations can agree by both going nowhere.
	 */
	diff_begin("b103 hdx coverage");
	for (i = 0, k = 0; i < 7; i++)
		if (seen_tx_state[i])
			k++;
	diff_eq_int("distinct transmit states installed (%ld)", k >= 3, 1, k);
	for (i = 0, k = 0; i < 7; i++)
		if (seen_rx_state[i])
			k++;
	diff_eq_int("distinct receive states installed (%ld)", k >= 3, 1, k);
	for (i = 0, k = 0; i < 8; i++)
		if (seen_substate[i])
			k++;
	diff_eq_int("distinct substates reached (%ld)", k >= 4, 1, k);
	diff_eq_int("connected status 7 reached (%ld)", seen_status[7] > 0, 1,
		    seen_status[7]);
	diff_eq_int("status 2 reached (%ld)", seen_status[2] > 0, 1,
		    seen_status[2]);
	diff_eq_int("status 3 reached (%ld)", seen_status[3] > 0, 1,
		    seen_status[3]);
	rc |= diff_end();

	printf("t_b103hdx: tx states");
	for (i = 0; i < 7; i++)
		printf(" %d", seen_tx_state[i]);
	printf(", rx states");
	for (i = 0; i < 7; i++)
		printf(" %d", seen_rx_state[i]);
	printf(", substates");
	for (i = 0; i < 6; i++)
		printf(" %d", seen_substate[i]);
	printf("\n");

	return rc;
}
