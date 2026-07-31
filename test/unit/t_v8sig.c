/*
 * t_v8sig.c -- differential test of the handshake's signal plumbing.
 *
 * All five take the whole object and write scattered parts of it, so the
 * comparison is the whole object every time, with the pointers the queue
 * functions move normalised to offsets first.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v8.h"

extern void ref_v8_ansaminit(struct v8 *v);
extern void ref_v8_TONEq_generate(struct v8 *v, short *out);
extern int ref_v8_rxreadqueue(struct v8 *v);
extern int ref_v8_txwritequeue(struct v8 *v);
extern short ref_v8_fsktxfilter(struct v8 *v, short sample);
extern int ref_v8_txinit(struct v8 *v);

static struct v8 obj_a, obj_b;

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

/* A pointer into the object, compared as an offset and then blanked. */
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
whole(long tag)
{
	size_t i;
	const unsigned char *a = (const unsigned char *)&obj_a;
	const unsigned char *b = (const unsigned char *)&obj_b;

	for (i = 0; i < sizeof(struct v8); i++)
		diff_eq_int("byte at +%ld", b[i], a[i], (long)i);
	(void)tag;
}

int
main(void)
{
	int rc = 0;
	int k, i;
	long moved = 0;

	diff_begin("v8_ansaminit");
	for (k = 0; k < 8; k++) {
		fill(&obj_a, sizeof(obj_a), 11u + k);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		obj_a.fa42 = obj_b.fa42 = (short)(k * 4001 - 16000);
		ref_v8_ansaminit(&obj_a);
		v8_ansaminit(&obj_b);
		whole(k);
	}
	diff_eq_int("the scaled field was set", obj_b.tone.f0e, 1, 0);
	rc |= diff_end();

	diff_begin("v8_TONEq_generate");
	{
		short out_a[4], out_b[4];
		int nonzero = 0;

		for (k = 0; k < 64; k++) {
			fill(&obj_a, sizeof(obj_a), 700u + k);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));
			obj_a.toneq_period = obj_b.toneq_period =
				(short)(k * 101);
			memset(out_a, 0x5a, sizeof(out_a));
			memset(out_b, 0x5a, sizeof(out_b));
			ref_v8_TONEq_generate(&obj_a, out_a);
			v8_TONEq_generate(&obj_b, out_b);
			for (i = 0; i < 4; i++) {
				diff_eq_int("sample %ld", out_b[i], out_a[i],
					    i);
				if (out_a[i] != 0)
					nonzero++;
			}
			whole(k);
		}
		diff_eq_int("a tone was generated (%ld)", nonzero > 100, 1,
			    nonzero);
	}
	rc |= diff_end();

	/*
	 * The queue functions walk pointers the transmitter set up, so the
	 * object is built by v8_txinit first rather than filled at random --
	 * a random pointer would walk off the object.
	 */
	diff_begin("v8_rxreadqueue and v8_txwritequeue");
	for (k = 0; k < 40; k++) {
		fill(&obj_a, sizeof(obj_a), 900u + k);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		ref_v8_txinit(&obj_a);
		ref_v8_txinit(&obj_b);

		for (i = 0; i < k % 7 + 1; i++) {
			diff_eq_int("rxread returns (%ld)",
				    v8_rxreadqueue(&obj_b),
				    ref_v8_rxreadqueue(&obj_a), k);
			diff_eq_int("txwrite returns (%ld)",
				    v8_txwritequeue(&obj_b),
				    ref_v8_txwritequeue(&obj_a), k);
			moved++;
		}
		normalise(offsetof(struct v8, tx_sym_a));
		normalise(offsetof(struct v8, tx_sym_b));
		normalise(offsetof(struct v8, tx_ring_base));
		normalise(offsetof(struct v8, tx_ring_half));
		whole(k);
	}
	diff_eq_int("blocks were moved (%ld)", moved > 50, 1, moved);
	rc |= diff_end();

	diff_begin("v8_fsktxfilter");
	for (k = 0; k < 24; k++) {
		fill(&obj_a, sizeof(obj_a), 1300u + k);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		for (i = 0; i < 80; i++) {
			short s = (short)((i * 811 + k * 97) % 20001 - 10000);

			diff_eq_int("sample %ld", v8_fsktxfilter(&obj_b, s),
				    ref_v8_fsktxfilter(&obj_a, s), i);
		}
		whole(k);
	}
	rc |= diff_end();

	return rc;
}
