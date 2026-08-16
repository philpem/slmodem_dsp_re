/*
 * t_v22fpdel.c -- V.22: does V22FP_delete release exactly what create took?
 *
 * The destructor is nothing but offsets: sixteen loads through the object and
 * its two blocks, each feeding one free.  So the test that matters is not
 * "does it run" but "does OUR reading of those offsets agree with the
 * object's" -- and the sharp way to ask that is to build the tree with the
 * BLOB'S constructor and tear it down with ours.  Every pointer our delete
 * picks up is then one the blob itself stored, and a field named two bytes
 * out shows up immediately as a free of something that was never allocated.
 *
 * `live == 0`, `bad_free == 0` and `frees == allocs` together are a complete
 * statement about the free set, not a sample of it: they say every block the
 * run handed out was released exactly once and nothing else was.  A wrong
 * offset cannot satisfy all three -- it frees some pointer twice or some
 * unallocated word once, and misses a real block either way.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v22fp.h"

extern void *ref_V22FP_create(void *fp, const void *cfg);
extern void ref_V22FP_delete(void *fp);

struct ledger {
	int allocs;
	int frees;
	int live;
	int bad;
	int null;
	int overflow;
};

/*
 * `v22_create`'s own configuration, with the two selectors left to the
 * caller.  Everything else is what the only caller in the object passes.
 */
static struct v22fp_cfg
base_cfg(int mode, int rate)
{
	struct v22fp_cfg c;

	c.mode = mode;
	c.rate = rate;
	c.f08 = 60000;
	c.f0c = 0;
	c.f10 = 700;
	c.f14 = 0;
	c.f18 = 1;
	return c;
}

/*
 * One build/tear-down cycle.  `ours` selects which side's destructor runs;
 * the constructor is always the blob's, so both arms start from a tree the
 * object itself laid out.
 */
static struct ledger
cycle(int mode, int rate, int ours, int *allocs_after_create)
{
	struct v22fp_cfg cfg = base_cfg(mode, rate);
	struct ledger l;
	void *fp;

	harness_alloc_reset();
	fp = ref_V22FP_create(0, &cfg);
	if (allocs_after_create != 0)
		*allocs_after_create = harness_alloc.allocs;
	if (ours)
		V22FP_delete((struct v22fp *)fp);
	else
		ref_V22FP_delete(fp);

	l.allocs = harness_alloc.allocs;
	l.frees = harness_alloc.frees;
	l.live = harness_alloc.live;
	l.bad = harness_alloc.bad_free;
	l.null = harness_alloc.free_null;
	l.overflow = harness_alloc.overflow;
	return l;
}

int
main(void)
{
	static const struct {
		int mode;
		int rate;
		const char *name;
	} arms[] = {
		{ 0, 0, "mode 0, 2400" },
		{ 1, 0, "mode 1, 2400" },
		{ 2, 0, "mode 2, 2400" },
		{ 0, 1, "mode 0, 1200" },
		{ 1, 2, "mode 1, 1200" },
		{ 9, 9, "out of range" }
	};
	int rc = 0;
	unsigned k;

	/*
	 * 1. The blob builds, the blob tears down.  This is the baseline: it
	 *    says what the object's own ledger looks like, so the comparison
	 *    below is against a measurement and not against an assumption.
	 */
	diff_begin("V22FP_delete: the object's own ledger");
	for (k = 0; k < sizeof(arms) / sizeof(arms[0]); k++) {
		int made = 0;
		struct ledger r = cycle(arms[k].mode, arms[k].rate, 0, &made);

		printf("  %-14s create %2d allocs; delete %2d frees, "
		       "%d live, %d bad, %d null\n",
		       arms[k].name, made, r.frees, r.live, r.bad, r.null);

		diff_eq_int("nothing leaked (arm %ld)", r.live, 0, (long)k);
		diff_eq_int("frees match allocs (arm %ld)", r.frees, r.allocs,
			    (long)k);
		diff_eq_int("no bad frees (arm %ld)", r.bad, 0, (long)k);
		diff_eq_int("no free(NULL) (arm %ld)", r.null, 0, (long)k);
		diff_eq_int("the live set never overflowed (arm %ld)", r.overflow, 0,
			    (long)k);
		/*
		 * The eleven direct allocations are create's own; the rest
		 * belong to the four sub-objects and the six `*_init` calls.
		 * Asserting the total pins all of them at once, which is what
		 * makes the free count below mean something.
		 */
		diff_eq_int("create allocates 40 blocks (arm %ld)", made, 40,
			    (long)k);
	}
	rc |= diff_end();

	/*
	 * 2. The blob builds, WE tear down.  Same tree, our offsets.  Any
	 *    field named wrong here frees a pointer the blob never handed
	 *    out, and the counters say so.
	 */
	diff_begin("V22FP_delete: our destructor on the object's own tree");
	for (k = 0; k < sizeof(arms) / sizeof(arms[0]); k++) {
		struct ledger r = cycle(arms[k].mode, arms[k].rate, 0, 0);
		struct ledger o = cycle(arms[k].mode, arms[k].rate, 1, 0);

		printf("  %-14s ours: %2d frees, %d live, %d bad, %d null\n",
		       arms[k].name, o.frees, o.live, o.bad, o.null);

		diff_eq_int("same number of frees (arm %ld)", o.frees, r.frees,
			    (long)k);
		diff_eq_int("same live count (arm %ld)", o.live, r.live, (long)k);
		diff_eq_int("same bad-free count (arm %ld)", o.bad, r.bad, (long)k);
		diff_eq_int("same free(NULL) count (arm %ld)", o.null, r.null,
			    (long)k);
		diff_eq_int("nothing leaked (arm %ld)", o.live, 0, (long)k);
		diff_eq_int("no bad frees (arm %ld)", o.bad, 0, (long)k);
	}
	rc |= diff_end();

	/*
	 * 3. Repeated cycles, in case something is only leaked the second
	 *    time round -- a buffer reused rather than reallocated, or a
	 *    sub-object create that quietly keeps its old one.
	 */
	diff_begin("V22FP_delete: twenty cycles balance");
	{
		struct v22fp_cfg cfg = base_cfg(0, 0);
		int i;

		harness_alloc_reset();
		for (i = 0; i < 20; i++) {
			void *fp = ref_V22FP_create(0, &cfg);

			V22FP_delete((struct v22fp *)fp);
		}
		printf("  20 cycles: %d allocs, %d frees, %d live, %d bad\n",
		       harness_alloc.allocs, harness_alloc.frees,
		       harness_alloc.live, harness_alloc.bad_free);
		diff_eq_int("20 cycles leak nothing (%ld)",
			    harness_alloc.live, 0, 0);
		diff_eq_int("20 cycles allocate 20x one build (%ld)",
			    harness_alloc.allocs, 40 * 20, 0);
		diff_eq_int("20 cycles free everything (%ld)",
			    harness_alloc.frees, harness_alloc.allocs, 0);
		diff_eq_int("no bad frees over 20 cycles (%ld)",
			    harness_alloc.bad_free, 0, 0);
		diff_eq_int("the live set never overflowed (%ld)",
			    harness_alloc.overflow, 0, 0);
	}
	rc |= diff_end();

	return rc;
}
