/*
 * t_fdspdp.c -- the differential test for FDSP_DP_Create (blob 0xae5c0,
 * 650 bytes) and FDSP_DP_Delete (0xae520, 157 bytes), and for the two
 * globals they write, `pGlobalFDSPObj` and `uCorrelationReportsNo`.
 *
 * WHAT IS COMPARED.  Everything the pair allocates: the 28-byte kernel, the
 * 0x271c buffer block, both 0x16a8 channels and both 240-float tap arrays,
 * byte for byte, with only the four words that HOLD an address skipped --
 * +0x10, +0x14 and +0x18 of the kernel and +0x1680 of each channel -- and
 * every one of those followed and its target compared instead. The
 * allocator's books are compared as well, which is what says the six
 * allocations happened and in what sizes.
 *
 * THE THREE ARMS THAT CANNOT BE DRIVEN, and they are one cause. The harness
 * allocator does not fail on request, so the chain of six `if (ok)` steps
 * never takes its else. That leaves untested: the partial free, the two
 * `offset` stores that happen through a possibly-NULL channel before the
 * chain's result is tested (0xae6c2), and FDSP_DP_Delete's own dereference
 * of `chan_a` before it tests it (0xae533). All three are read from the
 * disassembly and recorded at the definitions rather than tested, which is
 * the same trade `t_fdspksil` makes for silence_create's out-of-memory arm.
 *
 * WHAT IS DRIVEN INSTEAD is both entry paths -- allocate and re-initialise
 * -- both arms of the `status` decision, and a delay sweep that reaches the
 * ends of a `short` in both directions, because the two arguments are
 * `movswl` and a test that only used small positive numbers would not
 * notice if they were `unsigned short`.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/sysdep.h"
#include "dsplib/debug.h"

extern unsigned int ref_dsplibs_debug_level;
extern struct fdsp_kernel *ref_pGlobalFDSPObj;
extern unsigned int ref_uCorrelationReportsNo;

/*
 * Ours: FILE-LOCAL in the object, so both are `static` in Fdspkrnl.c and
 * fdspkrnl.h no longer declares them.  The test tier links a globalized copy
 * (tools/testvisible.py).
 */
extern struct fdsp_kernel *pGlobalFDSPObj;
extern unsigned int uCorrelationReportsNo;

extern struct fdsp_kernel *ref_FDSP_DP_Create(struct fdsp_kernel *k,
					      short rx, short tx);
extern void ref_FDSP_DP_Delete(struct fdsp_kernel *k);

extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

/* Coverage counters (F134). */
static int seen_int00[2];
static int seen_path[2];		/* allocate, re-initialise */
static int seen_debug[2];

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * Compare a byte range, skipping whole words at the given offsets, and say
 * how many bytes it actually looked at -- a skip list that grew to cover
 * everything would otherwise pass in silence.
 */
static void
compare_bytes(const char *what, long tag, const void *ours,
	      const void *theirs, unsigned int n,
	      const unsigned int *skip, unsigned int nskip)
{
	const unsigned char *a = (const unsigned char *)theirs;
	const unsigned char *b = (const unsigned char *)ours;
	unsigned int off, s, looked = 0;
	int reported = 0;

	for (off = 0; off < n; off++) {
		int skipped = 0;

		for (s = 0; s < nskip; s++) {
			if ((off & ~3u) == skip[s])
				skipped = 1;
		}
		if (skipped)
			continue;
		looked++;
		if (a[off] != b[off] && reported < 10) {
			reported++;
			diff_eq_int("byte differs %ld", b[off], a[off],
				    tag * 100000 + (long)off);
		}
	}
	diff_eq_int("bytes actually compared %ld", (int)looked,
		    (int)(n - nskip * 4), tag);
	(void)what;
}

static void
compare_kernels(long tag, const struct fdsp_kernel *ours,
		const struct fdsp_kernel *theirs)
{
	static const unsigned int kskip[] = { 0x10, 0x14, 0x18 };
	static const unsigned int cskip[] = { 0x1680 };
	int i;

	compare_bytes("kernel", tag, ours, theirs,
		      (unsigned int)sizeof(struct fdsp_kernel), kskip, 3);
	compare_bytes("buffers", tag + 1, ours->buffers, theirs->buffers,
		      (unsigned int)sizeof(struct fdsp_buffers), 0, 0);
	compare_bytes("chan_a", tag + 2, ours->chan_a, theirs->chan_a,
		      (unsigned int)sizeof(struct fdsp_channel), cskip, 1);
	compare_bytes("chan_b", tag + 3, ours->chan_b, theirs->chan_b,
		      (unsigned int)sizeof(struct fdsp_channel), cskip, 1);
	for (i = 0; i < 240; i++) {
		unsigned long x = 0, y = 0;

		memcpy(&x, &ours->chan_a->coef[i], 4);
		memcpy(&y, &theirs->chan_a->coef[i], 4);
		diff_eq_int("chan_a tap %ld", (long)x, (long)y,
			    tag * 1000 + i);
		memcpy(&x, &ours->chan_b->coef[i], 4);
		memcpy(&y, &theirs->chan_b->coef[i], 4);
		diff_eq_int("chan_b tap %ld", (long)x, (long)y,
			    tag * 1000 + 500 + i);
	}
}

static void
compare_globals(long tag, const struct fdsp_kernel *ours,
		const struct fdsp_kernel *theirs)
{
	diff_eq_int("pGlobalFDSPObj tracks the result %ld",
		    pGlobalFDSPObj == ours, ref_pGlobalFDSPObj == theirs, tag);
	diff_eq_int("both globals are set or both clear %ld",
		    pGlobalFDSPObj != 0, ref_pGlobalFDSPObj != 0, tag);
	diff_eq_int("uCorrelationReportsNo %ld",
		    (long)uCorrelationReportsNo,
		    (long)ref_uCorrelationReportsNo, tag);
	diff_eq_int("uCorrelationReportsNo is cleared %ld",
		    (long)uCorrelationReportsNo, 0, tag);
}

int
main(void)
{
	int failed = 0;
	unsigned int di;

	/*
	 * SECTION 1 -- the allocating path, with the delay sweep.
	 */
	diff_begin("FDSP_DP_Create allocating");
	{
		static const short delays[] = {
			0, 1, 2, 159, 160, 320, 32767, -1, -2, -160, -32768
		};
		static const unsigned int nd = sizeof(delays)
					       / sizeof(delays[0]);
		unsigned int ti;

		set_level(0);
		for (di = 0; di < nd; di++) {
			short rx = delays[di];
			for (ti = 0; ti < nd; ti += 3) {
				short tx = delays[ti];
				struct fdsp_kernel *ka, *kb;
				struct alloc_log la, lb;
				long tag = (long)(di * 10 + ti);

				pGlobalFDSPObj = 0;
				ref_pGlobalFDSPObj = 0;
				uCorrelationReportsNo = 0xdeadbeefu;
				ref_uCorrelationReportsNo = 0xdeadbeefu;

				/*
				 * ONE reset for the whole create/delete
				 * cycle, and the two sides read off as
				 * DIFFERENCES.  Resetting between them
				 * would drop the slots the allocator
				 * recorded, and every free that followed
				 * would be counted as a wild pointer on
				 * both sides -- agreeing while measuring
				 * nothing, which is how the first version
				 * of this section reported six frees as
				 * zero.
				 */
				harness_alloc_reset();
				ka = ref_FDSP_DP_Create(0, rx, tx);
				la = harness_alloc;
				kb = FDSP_DP_Create(0, rx, tx);
				lb = harness_alloc;

				diff_eq_int("allocs %ld",
					    lb.allocs - la.allocs,
					    la.allocs, tag);
				diff_eq_int("alloc bytes %ld",
					    (int)(lb.bytes - la.bytes),
					    (int)la.bytes, tag);
				diff_eq_int("six blocks %ld", la.allocs, 6,
					    tag);
				diff_eq_int("returned non-NULL %ld", kb != 0,
					    ka != 0, tag);
				compare_kernels(tag, kb, ka);
				compare_globals(tag, kb, ka);

				/* the two delays land where the string says */
				diff_eq_int("rx delay on chan_a %ld",
					    kb->chan_a->offset, rx, tag);
				diff_eq_int("tx delay on chan_b %ld",
					    kb->chan_b->offset, tx, tag);
				diff_eq_int("status %ld", kb->status,
					    ka->status, tag);
				seen_int00[rx >= 0 ? 1 : 0] = 1;
				seen_path[0] = 1;

				ref_FDSP_DP_Delete(ka);
				la = harness_alloc;
				FDSP_DP_Delete(kb);
				lb = harness_alloc;
				diff_eq_int("frees %ld",
					    lb.frees - la.frees,
					    la.frees - 0, tag);
				diff_eq_int("six frees each %ld", la.frees, 6,
					    tag);
				diff_eq_int("twelve frees in all %ld",
					    lb.frees, 12, tag);
				diff_eq_int("no wild frees %ld", lb.bad_free,
					    0, tag);
				diff_eq_int("nothing left live %ld",
					    (int)lb.live, 0, tag);
				diff_eq_int("delete clears the global %ld",
					    pGlobalFDSPObj == 0,
					    ref_pGlobalFDSPObj == 0, tag);
				diff_eq_int("and it really is clear %ld",
					    pGlobalFDSPObj == 0, 1, tag);
				harness_alloc_reset();
			}
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 2 -- the re-initialisation path.  The kernel handed in is
	 * one this test built, filled with a recognisable pattern first, so
	 * everything InitObj is supposed to overwrite is visibly
	 * overwritten and the two `offset` fields are visibly NOT: the
	 * delay arguments are only stored on the allocating path.
	 */
	diff_begin("FDSP_DP_Create re-initialising");
	{
		static const short delays[] = { 0, 7, -7, 32767, -32768 };
		static const unsigned int nd = sizeof(delays)
					       / sizeof(delays[0]);
		struct fdsp_kernel *ka, *kb;

		set_level(0);
		harness_alloc_reset();
		ka = ref_FDSP_DP_Create(0, 11, 22);
		kb = FDSP_DP_Create(0, 11, 22);

		for (di = 0; di < nd; di++) {
			struct alloc_log la, lb;
			struct fdsp_kernel *ra, *rb;
			long tag = 100 + (long)di;

			/* a pattern InitObj must scrub */
			memset(ka->buffers, 0x71, sizeof(*ka->buffers));
			memset(kb->buffers, 0x71, sizeof(*kb->buffers));
			memset(ka->chan_a->coef, 0x72, 240 * sizeof(float));
			memset(kb->chan_a->coef, 0x72, 240 * sizeof(float));
			memset(ka->chan_b->coef, 0x73, 240 * sizeof(float));
			memset(kb->chan_b->coef, 0x73, 240 * sizeof(float));
			ka->chan_a->offset = 0x1234;
			kb->chan_a->offset = 0x1234;
			ka->chan_b->offset = 0x5678;
			kb->chan_b->offset = 0x5678;

			harness_alloc_reset();
			ra = ref_FDSP_DP_Create(ka, delays[di], delays[di]);
			la = harness_alloc;
			harness_alloc_reset();
			rb = FDSP_DP_Create(kb, delays[di], delays[di]);
			lb = harness_alloc;

			diff_eq_int("re-init allocates nothing %ld",
				    lb.allocs, la.allocs, tag);
			diff_eq_int("really nothing %ld", lb.allocs, 0, tag);
			diff_eq_int("returns the same kernel %ld", rb == kb,
				    ra == ka, tag);
			compare_kernels(tag, kb, ka);
			compare_globals(tag, kb, ka);
			/*
			 * The delays are NOT re-applied on this path: that
			 * is what the two stores being inside the `k == 0`
			 * arm means, and a reader who moved them out would
			 * pass every other check here.
			 */
			diff_eq_int("chan_a keeps its old offset %ld",
				    kb->chan_a->offset, 0x1234, tag);
			diff_eq_int("chan_b keeps its old offset %ld",
				    kb->chan_b->offset, 0x5678, tag);
			seen_int00[delays[di] >= 0 ? 1 : 0] = 1;
			seen_path[1] = 1;
		}
		ref_FDSP_DP_Delete(ka);
		FDSP_DP_Delete(kb);
		harness_alloc_reset();
	}
	failed |= diff_end();

	/*
	 * SECTION 3 -- the debug lines.  One on entry with both delays in
	 * it, one more only on the re-initialising path.
	 */
	diff_begin("FDSP_DP_Create debug");
	{
		unsigned int lvl;

		for (lvl = 0; lvl <= 3; lvl++) {
			struct fdsp_kernel *ka, *kb;
			int i;

			for (i = 0; i < 2; i++) {
				set_level(lvl);
				dsplib_debug_capture_reset();
				dsplib_debug_capture_on = 1;
				ka = ref_FDSP_DP_Create(0, -5, 9);
				kb = FDSP_DP_Create(0, -5, 9);
				if (i == 1) {
					ka = ref_FDSP_DP_Create(ka, 1, 2);
					kb = FDSP_DP_Create(kb, 1, 2);
				}
				dsplib_debug_capture_on = 0;

				diff_eq_int("debug lines %ld",
					    (long)dsplib_debug_capture_lines(0),
					    (long)dsplib_debug_capture_lines(1),
					    (long)(lvl * 10 + i));
				diff_eq_int("debug text %ld",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1)),
					    0, (long)(lvl * 10 + i));
				seen_debug[dsplib_debug_capture_lines(0) ? 1
									: 0] = 1;
				ref_FDSP_DP_Delete(ka);
				FDSP_DP_Delete(kb);
			}
		}
		set_level(0);
		harness_alloc_reset();
	}
	failed |= diff_end();

	/*
	 * SECTION 4 -- FDSP_DP_Delete's NULL arm, and the coverage the
	 * sections above are supposed to have produced (F134).
	 */
	diff_begin("FDSP_DP_Delete and coverage");
	{
		struct alloc_log la, lb;

		pGlobalFDSPObj = (struct fdsp_kernel *)&failed;
		ref_pGlobalFDSPObj = (struct fdsp_kernel *)&failed;
		harness_alloc_reset();
		ref_FDSP_DP_Delete(0);
		la = harness_alloc;
		harness_alloc_reset();
		FDSP_DP_Delete(0);
		lb = harness_alloc;
		diff_eq_int("NULL frees nothing", lb.frees, la.frees, 0);
		diff_eq_int("really nothing", lb.frees, 0, 0);
		diff_eq_int("NULL passed to free", lb.free_null, la.free_null,
			    0);
		/* and it leaves the global ALONE rather than clearing it */
		diff_eq_int("the global survives a NULL delete",
			    pGlobalFDSPObj == (struct fdsp_kernel *)&failed,
			    ref_pGlobalFDSPObj
			    == (struct fdsp_kernel *)&failed, 0);
		diff_eq_int("and it really survived",
			    pGlobalFDSPObj == (struct fdsp_kernel *)&failed,
			    1, 0);
		pGlobalFDSPObj = 0;
		ref_pGlobalFDSPObj = 0;
		harness_alloc_reset();

		diff_eq_int("a negative rx delay was driven", seen_int00[0],
			    1, 0);
		diff_eq_int("a non-negative rx delay was driven",
			    seen_int00[1], 1, 0);
		diff_eq_int("the allocating path ran", seen_path[0], 1, 0);
		diff_eq_int("the re-initialising path ran", seen_path[1], 1,
			    0);
		diff_eq_int("the quiet debug arm ran", seen_debug[0], 1, 0);
		diff_eq_int("the printing debug arm ran", seen_debug[1], 1,
			    0);
	}
	failed |= diff_end();

	return failed;
}
