/*
 * t_faxdelete.c -- differential test of `FAX_delete` (`src/service/voice.c`,
 * `.text` 0x001450, 172 bytes).  See `fax.h` for `struct fax_ctx`.
 *
 * TWO TECHNIQUES, because `rc_a`/`rc_b` and `class1` are not observable the
 * same way:
 *
 *   - `class1` IS observable through the allocation log, `t_class1delete.c`'s
 *     own technique: both sides build an identically shaped `fax_ctx`
 *     (`rc_a`/`rc_b` NULL in this half) inside the SAME
 *     `harness_alloc_reset()` window as the delete call, and the test
 *     compares `frees`/`live`/`bad_free` between them.  A minimal
 *     `fax_class1` with every torn-down field NULL makes
 *     `fax_class1_delete` itself contribute exactly one tracked free (the
 *     session object), proven separately by `t_class1delete.c`.
 *
 *   - `rc_a`/`rc_b` are NOT: `src/core/FixedRC.c`'s own `RcFixed_Create`/
 *     `RcFixed_Delete` call plain `calloc`/`free`, not `sysdep_malloc`/
 *     `sysdep_free`, so neither side's traffic through them is TRACKED by
 *     `harness_alloc` at all -- an existing property of already-merged
 *     code this file does not touch, not something introduced here (an
 *     `ours`-vs-`blob` allocation-log comparison across a real
 *     `RcFixed_Create`/`_Delete` pair shows 0 tracked frees on `ours` and 2
 *     on `blob`, since `ref_RcFixed_Create`/`_Delete` -- the blob's own
 *     compiled code -- DOES call the tracked `sysdep_malloc`/`sysdep_free`;
 *     that gap is `FixedRC.c`'s, not `FAX_delete`'s, and is reported
 *     separately rather than fixed here).  So the `rc_a`/`rc_b` half uses
 *     REAL `RcFixed_Create` handles (a fabricated pointer is unsafe:
 *     `RcFixed_Delete` dereferences `h->state` unconditionally on a non-NULL
 *     `h`, and `struct rc`'s layout is opaque outside `FixedRC.c`) and only
 *     asserts the run completes with no bad free and no TRACKED leak
 *     (`ctx` itself, which is on the tracked path either way) -- proving
 *     `FAX_delete` reaches its final `sysdep_free(ctx)` on every presence
 *     combination rather than crashing or returning early.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/debug.h"
#include "dsplib/fax.h"
#include "dsplib/fixedrc.h"
#include "dsplib/sysdep.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_FAX_delete(struct fax_ctx *ctx);
extern struct rc *ref_RcFixed_Create(int mode);

/* ------------------------------------------------------------------- */
/* `class1` alone: rc_a = rc_b = NULL, full allocation-log agreement.   */

static void
run_class1_case(int have_class1, int level, long tag)
{
	struct fax_ctx *ca, *cb;
	long frees_a, live_a, bad_a;
	long frees_b, live_b, bad_b;

	dsplibs_debug_level = level;
	ref_dsplibs_debug_level = level;

	/*
	 * `ctx` itself must be allocated INSIDE the same
	 * `harness_alloc_reset()` window as the delete call -- the lesson
	 * `t_class1delete.c` already recorded (F10054) -- or FAX_delete's
	 * own final free of `ctx` reads as a bad free.
	 */
	harness_alloc_reset();
	ca = sysdep_malloc(sizeof(*ca));
	memset(ca, 0, sizeof(*ca));
	if (have_class1) {
		ca->class1 = sysdep_malloc(sizeof(*ca->class1));
		memset(ca->class1, 0, sizeof(*ca->class1));
	}
	FAX_delete(ca);
	frees_a = harness_alloc.frees;
	live_a = harness_alloc.live;
	bad_a = harness_alloc.bad_free;

	harness_alloc_reset();
	cb = sysdep_malloc(sizeof(*cb));
	memset(cb, 0, sizeof(*cb));
	if (have_class1) {
		cb->class1 = sysdep_malloc(sizeof(*cb->class1));
		memset(cb->class1, 0, sizeof(*cb->class1));
	}
	ref_FAX_delete(cb);
	frees_b = harness_alloc.frees;
	live_b = harness_alloc.live;
	bad_b = harness_alloc.bad_free;

	diff_eq_int("class1: frees agree, input %ld", frees_a, frees_b, tag);
	diff_eq_int("class1: live, ours, input %ld", live_a, 0, tag);
	diff_eq_int("class1: live, blob, input %ld", live_b, 0, tag);
	diff_eq_int("class1: bad_free, ours, input %ld", bad_a, 0, tag);
	diff_eq_int("class1: bad_free, blob, input %ld", bad_b, 0, tag);
}

static int
test_class1(void)
{
	int c, level;
	long tag = 0;

	diff_begin("FAX_delete (class1 field)");

	for (c = 0; c < 2; c++)
		for (level = 0; level < 3; level++)
			run_class1_case(c, level, tag++);

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}

/* ------------------------------------------------------------------- */
/* `rc_a`/`rc_b`: real handles, completion + no-bad-free proof only.    */

static void
run_rc_case(int have_a, int have_b, long tag)
{
	struct fax_ctx *ca, *cb;
	long live_a, bad_a, live_b, bad_b;

	harness_alloc_reset();
	ca = sysdep_malloc(sizeof(*ca));
	memset(ca, 0, sizeof(*ca));
	if (have_a)
		ca->rc_a = RcFixed_Create(2);
	if (have_b)
		ca->rc_b = RcFixed_Create(2);
	FAX_delete(ca);
	live_a = harness_alloc.live;
	bad_a = harness_alloc.bad_free;

	harness_alloc_reset();
	cb = sysdep_malloc(sizeof(*cb));
	memset(cb, 0, sizeof(*cb));
	if (have_a)
		cb->rc_a = ref_RcFixed_Create(2);
	if (have_b)
		cb->rc_b = ref_RcFixed_Create(2);
	ref_FAX_delete(cb);
	live_b = harness_alloc.live;
	bad_b = harness_alloc.bad_free;

	/*
	 * `ctx` is the only TRACKED allocation left once `rc_a`/`rc_b` are
	 * excluded (see the file banner) -- `live == 0` here means
	 * `sysdep_free(ctx)` was reached, on every presence combination.
	 */
	diff_eq_int("rc: live, ours, input %ld", live_a, 0, tag);
	diff_eq_int("rc: live, blob, input %ld", live_b, 0, tag);
	diff_eq_int("rc: bad_free, ours, input %ld", bad_a, 0, tag);
	diff_eq_int("rc: bad_free, blob, input %ld", bad_b, 0, tag);
}

static int
test_rc(void)
{
	int a, b;
	long tag = 0;

	diff_begin("FAX_delete (rc_a/rc_b fields)");

	for (a = 0; a < 2; a++)
		for (b = 0; b < 2; b++)
			run_rc_case(a, b, tag++);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_class1();
	rc |= test_rc();

	return rc;
}
