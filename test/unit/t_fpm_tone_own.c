/*
 * t_fpm_tone_own.c -- FPM_TONE ownership: the path D5 is about.
 *
 * D5 says FPM_TONE_create and FPM_TONE_delete disagree about who owns the
 * four buffers:
 *
 *     create:  if (owned && len > 0)  allocate kernel, history, rev_block,
 *                                     rev_acc
 *     delete:  if (len > 0)           free all four, then free the object
 *
 * so a caller that supplies its own state gets four slots it allocated
 * itself passed to sysdep_free, and its own storage freed on top.  Every
 * call site in the library passes NULL, so nothing in dsplibs.o reaches it --
 * which is precisely why it needs a test rather than an argument.
 *
 * t_fpm_tone.c's FPM_TONE_delete block does NOT reach this: it builds both
 * objects with a NULL state (so `owned` is 1 every time) and gets to the
 * second branch by clearing `cfg.len` on a finished object.  It asserts
 * bad_free == 0, which is the opposite of what this path produces.
 *
 * WHAT A CALLER-SUPPLIED STATE ACTUALLY REQUIRES.  D5's wording -- "four
 * never-allocated pointer slots passed to free" -- reads as though the slots
 * could hold anything at delete time.  They cannot hold anything at CREATE
 * time: create dereferences all four unconditionally, outside the guard that
 * skips the allocation.  The correlation loop writes `kernel[i]` and
 * `history[i]` for i < len, and the reversal block writes rev_block[0..4] and
 * rev_acc[0..3] whatever len is.  So the caller must supply four real
 * buffers, at the sizes create would have allocated (len*2, (len+extra)*2,
 * 10, 8 bytes), and this fixture does.  Handing it a pattern instead is a
 * wild write, not a comparable input.
 *
 * The fixture therefore changes exactly ONE thing against the self-allocated
 * path: who owns the storage.  Config, length and prototype are the built-in
 * ones, unmodified -- D7's lesson, that a state the library cannot produce
 * proves nothing about the library.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_tone.h"

extern void *ref_FPM_TONE_create(void *state, const void *cfg);
extern void ref_FPM_TONE_delete(void *state);

/* The built-in config's length, asserted against the config at run time. */
#define TONE_LEN	53
#define TONE_EXTRA	0

/*
 * Not HARNESS_MALLOC_FILL: "the caller prefilled this" has to be
 * distinguishable from "sysdep_malloc handed this back untouched".
 */
#define PREFILL		0x5a

/*
 * One side's storage.  Separate per side -- a shared kernel buffer would
 * have the content comparison checking one buffer against itself.
 */
struct fixture {
	struct fpm_tone state;
	short kernel[TONE_LEN];
	short history[TONE_LEN + TONE_EXTRA];
	short rev_block[5];		/* 10 bytes */
	short rev_acc[4];		/*  8 bytes */
};

static void
fixture_init(struct fixture *f)
{
	memset(f, PREFILL, sizeof(*f));
	f->state.kernel = f->kernel;
	f->state.history = f->history;
	f->state.rev_block = f->rev_block;
	f->state.rev_acc = f->rev_acc;
}

/* Allocation bookkeeping for one create or one delete, as a delta. */
struct counts {
	int allocs;
	int frees;
	int live;
	int bad;
	int null;
};

static struct counts
snapshot(void)
{
	struct counts c;

	c.allocs = harness_alloc.allocs;
	c.frees = harness_alloc.frees;
	c.live = harness_alloc.live;
	c.bad = harness_alloc.bad_free;
	c.null = harness_alloc.free_null;
	return c;
}

static struct counts
since(struct counts before)
{
	struct counts now = snapshot();

	now.allocs -= before.allocs;
	now.frees -= before.frees;
	now.live -= before.live;
	now.bad -= before.bad;
	now.null -= before.null;
	return now;
}

static void
show(const char *what, struct counts c)
{
	printf("  %-28s allocs=%d frees=%d live%+d bad_free=%d free_null=%d\n",
	       what, c.allocs, c.frees, c.live, c.bad, c.null);
}

/*
 * The five pointer-valued slots in the object.  cfg.src at +0x10 is NOT one
 * of them here: both sides are handed the same config, so it must be equal
 * and is compared.  The other five hold each side's own addresses.
 */
static int
is_pointer_slot(int off)
{
	return off == 0x2c || off == 0x30 || off == 0xf4 || off == 0xf8
	       || off == 0xfc;
}

static void
compare_state(const struct fpm_tone *ours, const struct fpm_tone *ref)
{
	const unsigned char *a = (const unsigned char *)ours;
	const unsigned char *b = (const unsigned char *)ref;
	int i;

	for (i = 0; i < FPM_TONE_STATE_SIZE; i += 2) {
		if (is_pointer_slot(i) || is_pointer_slot(i - 2))
			continue;
		diff_eq_int("supplied state, offset 0x%02lx",
			    *(const short *)(a + i), *(const short *)(b + i),
			    i);
	}
}

static void
compare_shorts(const char *what, const short *ours, const short *ref, int n)
{
	int i;

	for (i = 0; i < n; i++)
		diff_eq_int(what, ours[i], ref[i], i);
}

int
main(void)
{
	static struct fixture fa;	/* the blob's         */
	static struct fixture fb;	/* the reconstruction */
	struct fpm_tone_cfg cfg = FPM_TONE_CFG;
	struct fpm_tone *ra, *rb;
	struct counts base, ca, cb, da, db;
	int rc = 0;

	/*
	 * ---------------------------------------------------------------
	 * 1. The caller-supplied path: create allocates nothing, and the
	 *    buffers it fills are the caller's.
	 * ---------------------------------------------------------------
	 */
	diff_begin("FPM_TONE_create, caller-supplied state");

	diff_eq_int("the built-in config is %ld taps", cfg.len, TONE_LEN,
		    TONE_LEN);
	diff_eq_int("the built-in config's extra is %ld", cfg.extra,
		    TONE_EXTRA, TONE_EXTRA);

	fixture_init(&fa);
	fixture_init(&fb);

	harness_alloc_reset();
	base = snapshot();
	ra = (struct fpm_tone *)ref_FPM_TONE_create(&fa.state, &cfg);
	ca = since(base);

	base = snapshot();
	rb = FPM_TONE_create(&fb.state, &cfg);
	cb = since(base);

	show("blob create", ca);
	show("ours create", cb);

	diff_eq_int("create returns the supplied state (%ld)",
		    rb == &fb.state, ra == &fa.state, 0);
	diff_eq_int("create allocations agree (%ld)", cb.allocs, ca.allocs, 0);
	/*
	 * D5's first half, as a number.  `owned` is 0 on this path, so the
	 * four buffers are not allocated -- and neither is anything else.
	 */
	diff_eq_int("blob: create allocates nothing (%ld)", ca.allocs, 0, 0);
	diff_eq_int("ours: create allocates nothing (%ld)", cb.allocs, 0, 0);

	/*
	 * ANTI-VACUITY.  "Allocates nothing" is also what a create that
	 * faulted straight back out would report, so witness that it ran to
	 * the end THROUGH THE SUPPLIED POINTERS.  rev_block and rev_acc are
	 * written last; 0x3afb is 0.96^2, the resonator constant.
	 */
	diff_eq_int("blob wrote rev_block[0] (%ld)", fa.rev_block[0], 0x4000,
		    0);
	diff_eq_int("ours wrote rev_block[0] (%ld)", fb.rev_block[0], 0x4000,
		    0);
	diff_eq_int("blob wrote rev_block[3] (%ld)", fa.rev_block[3], 0x3afb,
		    3);
	diff_eq_int("ours wrote rev_block[3] (%ld)", fb.rev_block[3], 0x3afb,
		    3);
	/* And the kernel really was filled, not left at the prefill. */
	diff_eq_int("blob filled the kernel (%ld)",
		    fa.kernel[1] != (short)0x5a5a, 1, 1);
	diff_eq_int("ours filled the kernel (%ld)",
		    fb.kernel[1] != (short)0x5a5a, 1, 1);

	compare_state(&fb.state, &fa.state);
	compare_shorts("supplied kernel[%ld]", fb.kernel, fa.kernel, TONE_LEN);
	compare_shorts("supplied history[%ld]", fb.history, fa.history,
		       TONE_LEN + TONE_EXTRA);
	compare_shorts("supplied rev_block[%ld]", fb.rev_block, fa.rev_block,
		       5);
	compare_shorts("supplied rev_acc[%ld]", fb.rev_acc, fa.rev_acc, 4);

	rc |= diff_end();

	/*
	 * ---------------------------------------------------------------
	 * 2. D5 itself: delete frees five things it never allocated.
	 *
	 *    No reset between create and delete -- that would clear the live
	 *    set and turn a legitimate free into a bad one.  Deltas instead.
	 * ---------------------------------------------------------------
	 */
	diff_begin("FPM_TONE_delete, caller-supplied state (D5)");

	base = snapshot();
	ref_FPM_TONE_delete(ra);
	da = since(base);

	base = snapshot();
	FPM_TONE_delete(rb);
	db = since(base);

	show("blob delete", da);
	show("ours delete", db);

	/* The differential part: whatever it does, both sides must do it. */
	diff_eq_int("delete: real frees agree (%ld)", db.frees, da.frees, 0);
	diff_eq_int("delete: live-set change agrees (%ld)", db.live, da.live,
		    0);
	diff_eq_int("delete: bad frees agree (%ld)", db.bad, da.bad, 0);
	diff_eq_int("delete: null frees agree (%ld)", db.null, da.null, 0);

	/*
	 * The absolute values, so neither side can be quietly tidied into
	 * agreement at zero.  Five unknown pointers reach sysdep_free:
	 * rev_acc, rev_block, history, kernel -- none of which create
	 * allocated -- and then the caller's own state.
	 *
	 * free_null must be 0 as well, or "five unknown pointers were freed"
	 * would be indistinguishable from "some slots were NULL and were
	 * swallowed cheaply".
	 */
	diff_eq_int("blob: D5 -- five unowned frees (%ld)", da.bad, 5, 0);
	diff_eq_int("ours: D5 -- five unowned frees (%ld)", db.bad, 5, 0);
	diff_eq_int("blob: nothing real was freed (%ld)", da.frees, 0, 0);
	diff_eq_int("ours: nothing real was freed (%ld)", db.frees, 0, 0);
	diff_eq_int("blob: no NULL slots (%ld)", da.null, 0, 0);
	diff_eq_int("ours: no NULL slots (%ld)", db.null, 0, 0);

	rc |= diff_end();

	/*
	 * ---------------------------------------------------------------
	 * 3. The contrast, in the same run: the self-allocated path, where
	 *    create and delete agree.  Without this every count above could
	 *    be read as "the allocator is not wired up", since zero allocs
	 *    and zero frees is also what a dead counter reports.
	 * ---------------------------------------------------------------
	 */
	diff_begin("FPM_TONE self-allocated, for contrast");
	{
		struct fpm_tone *sa, *sb;

		harness_alloc_reset();

		base = snapshot();
		sa = (struct fpm_tone *)ref_FPM_TONE_create(0, &cfg);
		ca = since(base);

		base = snapshot();
		sb = FPM_TONE_create(0, &cfg);
		cb = since(base);

		show("blob create(NULL)", ca);
		show("ours create(NULL)", cb);

		diff_eq_int("both built (%ld)", sb != 0, sa != 0, 0);
		diff_eq_int("create(NULL) allocations agree (%ld)", cb.allocs,
			    ca.allocs, 0);
		/* Object plus the four buffers. */
		diff_eq_int("blob: create(NULL) allocates five (%ld)",
			    ca.allocs, 5, 0);
		diff_eq_int("ours: create(NULL) allocates five (%ld)",
			    cb.allocs, 5, 0);

		if (sa != 0 && sb != 0) {
			base = snapshot();
			ref_FPM_TONE_delete(sa);
			da = since(base);

			base = snapshot();
			FPM_TONE_delete(sb);
			db = since(base);

			show("blob delete(owned)", da);
			show("ours delete(owned)", db);

			diff_eq_int("delete(owned): frees agree (%ld)",
				    db.frees, da.frees, 0);
			diff_eq_int("delete(owned): bad frees agree (%ld)",
				    db.bad, da.bad, 0);
			diff_eq_int("blob: five real frees (%ld)", da.frees, 5,
				    0);
			diff_eq_int("ours: five real frees (%ld)", db.frees, 5,
				    0);
			diff_eq_int("blob: nothing unowned freed (%ld)",
				    da.bad, 0, 0);
			diff_eq_int("ours: nothing unowned freed (%ld)",
				    db.bad, 0, 0);
			diff_eq_int("nothing leaked (%ld)", harness_alloc.live,
				    0, 0);
		}
		harness_alloc_reset();
	}
	rc |= diff_end();

	return rc;
}
