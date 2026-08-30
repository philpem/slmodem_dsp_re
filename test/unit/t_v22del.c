/*
 * t_v22del.c -- V.22: does v22_delete release exactly what v22_create took?
 *
 * THE CONSTRUCTOR IS ALWAYS THE BLOB'S, and that is the whole design.
 * `v22_create` is not reconstructed -- it stores a pointer to `v22_process`,
 * which waits on the seven V22_PROTOCOL handlers -- so there is no way to
 * build this object from our side at all.  There does not need to be: every
 * pointer our destructor picks up is then one the object itself stored, and a
 * field named four bytes out shows up immediately as a free of something that
 * was never allocated.  That is t_v22fpdel.c's method one level up, and it is
 * SHARPER here than driving both sides would be, because the thing being
 * tested is a set of offsets into a struct only the blob knows how to fill.
 *
 * `live == 0`, `bad_free == 0` and `frees == allocs` together are a complete
 * statement about the free set rather than a sample of it: they say every
 * block the run handed out was released exactly once and nothing else was. A
 * wrong offset cannot satisfy all three.
 *
 * The reference arm runs first on every configuration, so the numbers the
 * comparison uses are measured rather than assumed -- and the two arms are
 * compared against each other, not against a literal, so a change in what
 * `V22FP_create` allocates does not silently become a failure here.
 *
 * WHAT THE THREE IDS ARE FOR.  `v22_create` picks the V22FP rate from the
 * datapump id -- 212 and 22 give 1200, anything else gives 2400 -- and picks
 * the station role from `caller`.  Six combinations, and they build different
 * trees: the 2400 arm and the answer arm allocate different sub-objects.  A
 * destructor tested on one arm has been tested on one tree.
 *
 * WHY THE HOST RATE IS THE DATAPUMP'S OWN 8000 AND NOT SOMETHING THE WRAPPER
 * HAS TO CONVERT.  At 9600 `dp_wrapper_create` builds two rate converters and
 * the ledger goes four blocks short -- not because `v22_delete` misses them,
 * but because `src/core/fixedrc.c` releases them through libc `free` where
 * the object calls `sysdep_free`, so the harness's counters never see the
 * calls.  That is finding F8530, it is a real divergence in a file outside
 * this one's scope, and it is not this test's to paper over.  At 8000 the
 * wrapper builds no converters, every block goes through the hook, and the
 * two ledgers are equal rather than merely close.  Re-add a 9600 arm when
 * F8530 is settled: it is four more blocks of coverage for nothing.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/dp.h"
#include "dsplib/v22.h"

extern struct dp *ref_v22_create(void *modem, int id, int caller, int srate,
				 int max_frag, struct dp_operations *op);
extern int ref_v22_delete(struct dp *dp);

struct ledger {
	int allocs;
	int frees;
	int live;
	int bad;
	int null;
	int overflow;
	int ret;
};

/*
 * One build/tear-down cycle.  `ours` selects which side's destructor runs;
 * the constructor is always the blob's.
 */
static struct ledger
cycle(int id, int caller, int ours, int *allocs_after_create)
{
	static struct dp_operations op;
	struct ledger l;
	struct dp *dp;

	memset(&op, 0, sizeof(op));
	op.name = "v22";

	harness_alloc_reset();
	dp = ref_v22_create(0, id, caller, V22_DP_SRATE, V22_DP_FRAG, &op);
	if (allocs_after_create != 0)
		*allocs_after_create = harness_alloc.allocs;

	l.ret = ours ? v22_delete(dp) : ref_v22_delete(dp);

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
		int id;
		int caller;
		const char *name;
	} arms[] = {
		{ V22_DP_ID_V22BIS,  1, "V.22bis, caller"   },
		{ V22_DP_ID_V22,     1, "V.22, caller"      },
		{ V22_DP_ID_BELL212, 1, "Bell 212, caller"  },
		{ V22_DP_ID_V22BIS,  0, "V.22bis, answer"   },
		{ V22_DP_ID_V22,     0, "V.22, answer"      },
		{ V22_DP_ID_BELL212, 0, "Bell 212, answer"  }
	};
	int rc = 0;
	unsigned k;

	/*
	 * 1. The blob builds, the blob tears down.  The baseline, so the
	 *    comparison below is against a measurement.
	 */
	diff_begin("v22_delete: the object's own ledger");
	for (k = 0; k < sizeof(arms) / sizeof(arms[0]); k++) {
		int made = 0;
		struct ledger r = cycle(arms[k].id, arms[k].caller, 0, &made);

		printf("  %-18s create %2d allocs; delete %2d frees, "
		       "%d live, %d bad, %d null\n",
		       arms[k].name, made, r.frees, r.live, r.bad, r.null);

		diff_eq_int("nothing leaked (arm %ld)", r.live, 0, (long)k);
		diff_eq_int("frees match allocs (arm %ld)", r.frees, r.allocs,
			    (long)k);
		diff_eq_int("no bad frees (arm %ld)", r.bad, 0, (long)k);
		diff_eq_int("no free(NULL) (arm %ld)", r.null, 0, (long)k);
		diff_eq_int("the live set never overflowed (arm %ld)",
			    r.overflow, 0, (long)k);
		/*
		 * The guard that keeps the rest from going vacuous: a create
		 * that failed and returned NULL would leave every counter at
		 * zero and every check above green.
		 */
		diff_eq_int("create actually built something (arm %ld)",
			    made > 0, 1, (long)k);
	}
	rc |= diff_end();

	/*
	 * 2. The blob builds, WE tear down.  Same tree, our offsets.  A field
	 *    named wrong frees a pointer the blob never handed out.
	 */
	diff_begin("v22_delete: ours against the same tree");
	for (k = 0; k < sizeof(arms) / sizeof(arms[0]); k++) {
		struct ledger r = cycle(arms[k].id, arms[k].caller, 0, 0);
		struct ledger o = cycle(arms[k].id, arms[k].caller, 1, 0);

		diff_eq_int("allocs (arm %ld)", o.allocs, r.allocs, (long)k);
		diff_eq_int("frees (arm %ld)", o.frees, r.frees, (long)k);
		diff_eq_int("live (arm %ld)", o.live, r.live, (long)k);
		diff_eq_int("bad frees (arm %ld)", o.bad, r.bad, (long)k);
		diff_eq_int("free(NULL) (arm %ld)", o.null, r.null, (long)k);
		diff_eq_int("return (arm %ld)", o.ret, r.ret, (long)k);
	}
	rc |= diff_end();

	return rc;
}
