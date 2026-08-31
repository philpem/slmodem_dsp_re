/*
 * t_voicedpdel.c -- differential test of `detector_delete` (0xad620).
 *
 * A DESTRUCTOR CANNOT BE COMPARED BY COMPARING ITS OUTPUT, because it has
 * none: it frees things.  What it can be compared by is WHICH ALLOCATIONS ARE
 * STILL LIVE afterwards, and the harness allocator already knows -- so each
 * side gets its own identically-shaped graph, both are torn down, and the
 * per-pointer liveness vectors are compared entry for entry.  A destructor
 * that freed the wrong field, or missed one, moves that vector; a bare
 * `harness_alloc.live` count would not see a swap.
 *
 * `bad_free` is the other half.  The allocator swallows a free of a pointer it
 * never handed out rather than passing it to free(), and counts it, so a
 * double free or a wild pointer is a number at the end of the run instead of
 * a crash in the middle of it.  It has to stay zero.
 *
 * THE SHAPE IS SWEPT, not fixed.  `detector_delete` guards its three cadence
 * pointers and does NOT guard the +0x04 block or any of its four tones, and
 * `TONE_delete` in turn guards its own four allocations on a `fir_len > 0`
 * that is a signed 16-bit test.  Every combination of the three cadences and
 * the four tone shapes is built (2^3 * 2^4 = 128), so both branch structures
 * are driven at both ends.
 *
 * NOTHING HERE IS CONSTRUCTED BY `detector_create`, which is unwritten: the
 * graphs are built by hand out of `sysdep_malloc`, filled with 0xa5 -- never
 * zero, which makes "never written" look deliberate -- and only the fields
 * the two destructors read are planted.  That is D955/F8587's rule for a
 * fixture, and here it is the whole test.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/cadence.h"
#include "dsplib/detector.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/sysdep.h"

extern void ref_detector_delete(struct detector *d);

#define MAXPTR	32

struct graph {
	struct detector *d;
	void *ptr[MAXPTR];	/* every allocation, in creation order */
	int n;
};

static long n_graphs, arm_cadence, arm_no_cadence, arm_firs, arm_no_firs;

static void *
take(struct graph *g, unsigned size)
{
	void *p = sysdep_malloc(size);

	if (p != 0) {
		memset(p, HARNESS_MALLOC_FILL, size);
		if (g->n < MAXPTR)
			g->ptr[g->n++] = p;
	}
	return p;
}

/*
 * `cads` is a three-bit mask of which cadence slots are non-NULL; `firs` is a
 * four-bit mask of which tones have a positive `fir_len` and so carry the
 * four allocations `TONE_delete` frees.
 */
static int
build(struct graph *g, unsigned cads, unsigned firs)
{
	struct cadence **slot[3];
	int i;

	g->n = 0;
	g->d = take(g, sizeof(struct detector));
	if (g->d == 0)
		return 0;
	g->d->ptr_0004 = take(g, 24);

	for (i = 0; i < 4; i++) {
		struct fdsp_tone *t = take(g, sizeof(struct fdsp_tone));

		if (t == 0)
			return 0;
		if (firs & (1u << i)) {
			t->fir_len = 3;
			t->fir_coef = take(g, 12);
			t->fir_dly = take(g, 12);
			t->ptr_01b4 = take(g, 12);
			t->ptr_01b8 = take(g, 12);
		} else {
			t->fir_len = 0;
		}
		g->d->tone[i] = t;
	}

	slot[0] = &g->d->cadence_0008;
	slot[1] = &g->d->cadence_000c;
	slot[2] = &g->d->cadence_0010;
	for (i = 0; i < 3; i++) {
		if (cads & (1u << i)) {
			struct cadence *c = take(g, sizeof(struct cadence));

			if (c == 0)
				return 0;
			c->filter = 0;
			*slot[i] = c;
		} else {
			*slot[i] = 0;
		}
	}
	return 1;
}

static void
one(unsigned cads, unsigned firs, long tag)
{
	struct graph ga, gb;
	int live_a[MAXPTR], live_b[MAXPTR];
	int bad_before, i;

	harness_alloc_reset();
	if (!build(&ga, cads, firs) || !build(&gb, cads, firs)) {
		diff_eq_int("both graphs built", 0, 1, tag);
		return;
	}
	diff_eq_int("the two graphs have the same shape", gb.n, ga.n, tag);
	bad_before = harness_alloc.bad_free;

	ref_detector_delete(ga.d);
	detector_delete(gb.d);

	for (i = 0; i < ga.n; i++)
		live_a[i] = harness_alloc_ordinal(ga.ptr[i]) != 0;
	for (i = 0; i < gb.n; i++)
		live_b[i] = harness_alloc_ordinal(gb.ptr[i]) != 0;
	for (i = 0; i < ga.n && i < gb.n; i++)
		diff_eq_int("allocation liveness after detector_delete",
			    live_b[i], live_a[i], tag * 100 + i);

	/* Absolutely: everything the graph owns is gone, and nothing else. */
	for (i = 0; i < gb.n; i++)
		diff_eq_int("every allocation was released", live_b[i], 0,
			    tag * 100 + i);
	diff_eq_int("no bad frees", harness_alloc.bad_free, bad_before, tag);
	diff_eq_int("the arena is empty", harness_alloc.live, 0, tag);

	n_graphs++;
	if (cads != 0)
		arm_cadence++;
	else
		arm_no_cadence++;
	if (firs != 0)
		arm_firs++;
	else
		arm_no_firs++;
}

static int
t_sweep(void)
{
	unsigned cads, firs;
	long tag = 0;

	diff_begin("detector_delete over every cadence and tone shape");
	for (cads = 0; cads < 8; cads++)
		for (firs = 0; firs < 16; firs++)
			one(cads, firs, tag++);
	return diff_end();
}

/*
 * `TONE_delete`'s guard is a SIGNED 16-bit test (`cmpw $0x0,0x20(%ebx);
 * jle`), so a `fir_len` with the high bit set is NEGATIVE and skips the four
 * frees.  A fixture filled with 0xa5 has exactly that -- which is why this
 * case is spelled out rather than left to the fill.
 */
static int
t_negative_fir(void)
{
	struct graph ga, gb;
	int i;

	diff_begin("a negative fir_len skips TONE_delete's four frees");

	harness_alloc_reset();
	if (!build(&ga, 7, 0) || !build(&gb, 7, 0)) {
		diff_eq_int("both graphs built", 0, 1, 0);
		return diff_end();
	}
	for (i = 0; i < 4; i++) {
		ga.d->tone[i]->fir_len = (short)0xa5a5;
		gb.d->tone[i]->fir_len = (short)0xa5a5;
	}
	diff_eq_int("0xa5a5 is negative as a short", (short)0xa5a5 < 0, 1, 0);

	ref_detector_delete(ga.d);
	detector_delete(gb.d);

	for (i = 0; i < gb.n; i++)
		diff_eq_int("liveness", harness_alloc_ordinal(gb.ptr[i]) != 0,
			    harness_alloc_ordinal(ga.ptr[i]) != 0, i);
	diff_eq_int("no bad frees", harness_alloc.bad_free, 0, 0);
	diff_eq_int("the arena is empty", harness_alloc.live, 0, 0);
	return diff_end();
}

static int
t_coverage(void)
{
	diff_begin("every shape was built");
	diff_eq_int("graphs torn down", n_graphs, 128, 0);
	diff_eq_int("with cadences", arm_cadence, 112, 0);
	diff_eq_int("without cadences", arm_no_cadence, 16, 0);
	diff_eq_int("with FIR allocations", arm_firs, 120, 0);
	diff_eq_int("without FIR allocations", arm_no_firs, 8, 0);
	fprintf(stderr, "t_voicedpdel: %ld graphs, cad=%ld/%ld fir=%ld/%ld\n",
		n_graphs, arm_cadence, arm_no_cadence, arm_firs, arm_no_firs);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_sweep();
	failed |= t_negative_fir();
	failed |= t_coverage();
	return failed;
}
