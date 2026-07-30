/*
 * t_b103alloc.c -- Bell 103: does create/delete balance?
 *
 * The one question a field-by-field differential test cannot answer.  Two
 * implementations can agree on every byte of a built object and still
 * disagree about who owns it, and the symptom of getting that wrong is a leak
 * or a double free rather than a wrong number.
 *
 * dsplibs makes this worth checking properly: it uses the "pass NULL to
 * allocate" idiom at three nesting levels -- b103_create -> B103FP_create ->
 * FPM_*_create -- with a separate ownership flag at each.
 *
 * What is asserted here is the ORIGINAL's behaviour, measured.  That includes
 * a defect: B103FP_delete frees the object even when the caller supplied it
 * (D8).  Asserting the `bad_free` count rather than asserting zero is
 * deliberate -- it stops the defect being quietly tidied away, in either
 * implementation, without the test noticing.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/b103fp.h"

extern void *ref_B103FP_create(void *state, const void *cfg);
extern void ref_B103FP_delete(void *state);

/*
 * Allocation counts for one create/delete cycle.  `bad` counts frees of
 * pointers the allocator never handed out; the harness swallows those rather
 * than passing them to free(), so the run survives to report them.
 */
struct cycle {
	int allocs;
	int frees;
	int live;
	int bad;
};

static struct cycle
run_cycle(int call_type, struct b103fp *supplied)
{
	struct b103_cfg cfg = B103_CFG_data;
	struct cycle c;
	struct b103fp *fp;

	cfg.call_type = call_type;

	harness_alloc_reset();
	fp = ref_B103FP_create(supplied, &cfg);
	c.allocs = harness_alloc.allocs;
	ref_B103FP_delete(fp);
	c.frees = harness_alloc.frees;
	c.live = harness_alloc.live;
	c.bad = harness_alloc.bad_free;
	return c;
}

int
main(void)
{
	static const struct {
		int call_type;
		const char *name;
		int allocs;
	} types[] = {
		{ B103_CALL_ORIGINATE, "originate", 28 },
		{ B103_CALL_ANSWER,    "answer",    28 },
		{ B103_CALL_LOOPBACK,  "loopback",  23 }
	};
	static struct b103fp supplied;
	int rc = 0;
	unsigned k;

	/*
	 * 1. The object allocates its own storage.  Everything must come back.
	 */
	diff_begin("B103FP create/delete balance, self-allocated");
	for (k = 0; k < sizeof(types) / sizeof(types[0]); k++) {
		struct cycle c = run_cycle(types[k].call_type, 0);

		printf("  %-10s %2d allocs, %2d frees, %d live, %d bad\n",
		       types[k].name, c.allocs, c.frees, c.live, c.bad);

		diff_eq_int("%s: allocation count", c.allocs,
			    types[k].allocs, (long)k);
		diff_eq_int("%s: nothing leaked", c.live, 0, (long)k);
		diff_eq_int("%s: frees match allocs", c.frees, c.allocs, (long)k);
		diff_eq_int("%s: no bad frees", c.bad, 0, (long)k);
	}
	rc |= diff_end();

	/*
	 * 2. The caller supplies the object.  One fewer allocation -- and one
	 *    free too many, because B103FP_delete frees it anyway.  See D8.
	 *
	 *    The buffer must be zeroed first: a caller-supplied state means a
	 *    caller-supplied sub-object tree too, and create tests those
	 *    pointers for NULL to decide whether to allocate.  Handing it
	 *    uninitialised memory segfaults, which is its own lesson about the
	 *    contract.
	 */
	diff_begin("B103FP create/delete balance, caller-supplied");
	for (k = 0; k < sizeof(types) / sizeof(types[0]); k++) {
		struct cycle c;

		memset(&supplied, 0, sizeof(supplied));
		c = run_cycle(types[k].call_type, &supplied);

		printf("  %-10s %2d allocs, %2d frees, %d live, %d bad\n",
		       types[k].name, c.allocs, c.frees, c.live, c.bad);

		diff_eq_int("%s: one fewer allocation", c.allocs,
			    types[k].allocs - 1, (long)k);
		diff_eq_int("%s: nothing leaked", c.live, 0, (long)k);
		diff_eq_int("%s: the sub-objects balance", c.frees,
			    types[k].allocs - 1, (long)k);
		/*
		 * D8.  Not a zero: the original frees the caller's own buffer
		 * and this asserts that it still does, so tidying it away in
		 * either implementation fails here rather than diverging
		 * silently.
		 */
		diff_eq_int("%s: D8 -- the caller's buffer is freed too",
			    c.bad, 1, (long)k);
	}
	rc |= diff_end();

	/*
	 * 3. Repeated cycles, in case something is only leaked the second time
	 *    -- a static that is initialised once, or a buffer reused rather
	 *    than reallocated.
	 */
	diff_begin("B103FP repeated create/delete");
	{
		struct b103_cfg cfg = B103_CFG_data;
		int i;

		cfg.call_type = B103_CALL_ORIGINATE;
		harness_alloc_reset();
		for (i = 0; i < 20; i++) {
			struct b103fp *fp = ref_B103FP_create(0, &cfg);

			ref_B103FP_delete(fp);
		}
		printf("  20 cycles: %d allocs, %d frees, %d live, %d bad\n",
		       harness_alloc.allocs, harness_alloc.frees,
		       harness_alloc.live, harness_alloc.bad_free);
		diff_eq_int("20 cycles leak nothing (%ld)",
			    harness_alloc.live, 0, 0);
		diff_eq_int("20 cycles allocate 20x one (%ld)",
			    harness_alloc.allocs, 28 * 20, 0);
		diff_eq_int("20 cycles free everything (%ld)",
			    harness_alloc.frees, harness_alloc.allocs, 0);
		diff_eq_int("the live set never overflowed (%ld)",
			    harness_alloc.overflow, 0, 0);
	}
	rc |= diff_end();

	return rc;
}
