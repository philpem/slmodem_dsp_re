/*
 * t_v8hs.c -- differential test of the handshake state machine.
 *
 * Two state variables, one per direction, so the sweep is over both: every
 * transmit state that has a body against every receive state that has one,
 * driven for enough blocks that the deadlines and counters actually expire.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v8.h"

extern int ref_v8handshak(struct v8 *v);
extern int ref_v8_txinit(struct v8 *v);
extern int ref_v8_rxinit(struct v8 *v);
extern void ref_v8_V21_Init(struct v8 *v, short ch, short ans);
extern void ref_v8_ansaminit(struct v8 *v);
extern void ref_initTxSequence(struct v8 *v);
extern void ref_v8_phase_rev_init(struct v8_phase_rev *pr);
extern void ref_v8_detectorinit(struct v8 *v, struct v8_detector *d,
				const short *t, short a3, short a4, short a5,
				short a6, short a7);

static const short det_tab[8] = { 15000, -9000, 14000, -8000,
				  -7000, 13000, -6000, 12000 };

static struct v8 obj_a, obj_b;
static struct v8_cm cm_a, cm_b;

static void
fill(void *p, size_t n, unsigned seed)
{
	unsigned char *b = p;
	size_t i;

	for (i = 0; i < n; i++) {
		seed = seed * 1103515245u + 12345u;
		b[i] = (unsigned char)(seed >> 16);
	}
}

/* A pointer into the object, compared as an offset then blanked. */
static void
normalise(size_t off)
{
	uintptr_t pa, pb;
	long da, db;

	memcpy(&pa, (char *)&obj_a + off, sizeof(pa));
	memcpy(&pb, (char *)&obj_b + off, sizeof(pb));
	da = (long)(pa - (uintptr_t)&obj_a);
	db = (long)(pb - (uintptr_t)&obj_b);
	diff_eq_int("pointer at +%ld", db, da, (long)off);
	memcpy((char *)&obj_a + off, &da, sizeof(da));
	memcpy((char *)&obj_b + off, &db, sizeof(db));
}

static void
blank_filters(void)
{
	obj_a.v21.a = obj_b.v21.a = 0;
	obj_a.v21.b = obj_b.v21.b = 0;
	obj_a.v21.c = obj_b.v21.c = 0;
	obj_a.v21.d = obj_b.v21.d = 0;
	obj_a.detector.table = obj_b.detector.table = 0;
	obj_a.cm = obj_b.cm = 0;
}

int
main(void)
{
	/*
	 * The five states that have bodies.  An unknown state is deliberately
	 * NOT swept: the original's default path jumps back to re-read a
	 * counter that only this loop advances, so it spins forever.  That is
	 * faithful, and it means the test would hang rather than fail.
	 */
	static const short tx_states[] = { 5, 6, 23, 43, 45 };
	static const short rx_states[] = { 0x19, 0x20, 0x23, 0x28, 0x63, 0 };
	int rc = 0;
	unsigned t, r;
	int step, i;
	long ran = 0;
	long returns[4];

	memset(returns, 0, sizeof(returns));
	diff_begin("v8handshak");

	for (t = 0; t < sizeof(tx_states) / sizeof(tx_states[0]); t++) {
		for (r = 0; r < sizeof(rx_states) / sizeof(rx_states[0]); r++) {
			fill(&obj_a, sizeof(obj_a), 4200u + t * 16 + r);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));

			memset(&cm_a, 0, sizeof(cm_a));
			cm_a.b0 = (unsigned char)(0x11 * (t + 1));
			cm_a.b1 = (unsigned char)(0x22 * (r + 1));
			cm_a.b2 = (unsigned char)(t & 1 ? 0x14 : 0x04);
			cm_a.ext1[0] = 'G';
			memcpy(&cm_b, &cm_a, sizeof(cm_a));

			ref_v8_txinit(&obj_a);
			ref_v8_txinit(&obj_b);
			ref_v8_rxinit(&obj_a);
			ref_v8_rxinit(&obj_b);
			obj_a.fa42 = obj_b.fa42 = 16384;
			ref_v8_V21_Init(&obj_a, 1, 0);
			ref_v8_V21_Init(&obj_b, 1, 0);
			ref_v8_ansaminit(&obj_a);
			ref_v8_ansaminit(&obj_b);
			/*
			 * Both detectors need arming: the phase-reversal one
			 * takes its window size from a field that is fill
			 * pattern otherwise, and walks off the object.
			 */
			ref_v8_phase_rev_init(&obj_a.phase_rev);
			ref_v8_phase_rev_init(&obj_b.phase_rev);
			ref_v8_detectorinit(&obj_a, &obj_a.detector, det_tab,
					    0, 100, 50, 1500, 0);
			ref_v8_detectorinit(&obj_b, &obj_b.detector, det_tab,
					    0, 100, 50, 1500, 0);

			obj_a.cm = &cm_a;
			obj_b.cm = &cm_b;
			obj_a.tx_seq = &obj_a.seq[0];
			obj_b.tx_seq = &obj_b.seq[0];
			obj_a.seq_alt = &obj_a.seq[2];
			obj_b.seq_alt = &obj_b.seq[2];
			obj_a.seq_spare = &obj_a.seq[4];
			obj_b.seq_spare = &obj_b.seq[4];

			/*
			 * Real sequences in the buffers.  Without them
			 * v8_getbit sees a zero-length message set to
			 * repeat, resets, and recurses on nothing.
			 */
			ref_initTxSequence(&obj_a);
			initTxSequence(&obj_b);
			for (i = 0; i < 5; i++) {
				obj_a.seq[i] = obj_a.seq[0];
				obj_b.seq[i] = obj_b.seq[0];
			}

			obj_a.f9d4 = obj_b.f9d4 = tx_states[t];
			obj_a.f9d6 = obj_b.f9d6 = rx_states[r];
			obj_a.f9d8 = obj_b.f9d8 = (short)(r & 1 ? 0x19 : 0x24);
			obj_a.fa3e = obj_b.fa3e = 0x60;
			obj_a.deadline_a = obj_b.deadline_a = 40;
			obj_a.deadline_b = obj_b.deadline_b = 60;
			obj_a.fe64 = obj_b.fe64 = 0;
			obj_a.f110 = obj_b.f110 = 8;
			obj_a.tone.f08 = obj_b.tone.f08 = 6000;
			obj_a.v21_params.f06 = obj_b.v21_params.f06 = 0x20;

			for (step = 0; step < 30; step++) {
				int ra = ref_v8handshak(&obj_a);
				int rb = v8handshak(&obj_b);

				diff_eq_int("return (%ld)", rb, ra,
					    (long)(t * 16 + r));
				if (ra >= 0 && ra < 4)
					returns[ra]++;
				ran++;
			}

			normalise(offsetof(struct v8, tx_seq));
			normalise(offsetof(struct v8, seq_alt));
			normalise(offsetof(struct v8, seq_spare));
			normalise(offsetof(struct v8, tx_sym_a));
			normalise(offsetof(struct v8, tx_sym_b));
			normalise(offsetof(struct v8, tx_ring_base));
			normalise(offsetof(struct v8, tx_ring_half));
			normalise(offsetof(struct v8, rx)
				  + offsetof(struct v8_rx, buf));
			blank_filters();

			for (i = 0; i < (int)sizeof(struct v8); i++)
				diff_eq_int("byte at +%ld",
					    ((unsigned char *)&obj_b)[i],
					    ((unsigned char *)&obj_a)[i],
					    (long)i);
			diff_eq_int("menu after (%ld)",
				    memcmp(&cm_a, &cm_b, sizeof(cm_a)) == 0, 1,
				    (long)(t * 16 + r));
		}
	}

	diff_eq_int("the machine ran (%ld)", ran > 100, 1, ran);
	diff_eq_int("more than one return value (%ld)",
		    (returns[0] > 0) + (returns[1] > 0) + (returns[2] > 0) >= 2,
		    1, returns[0]);
	rc |= diff_end();
	return rc;
}
