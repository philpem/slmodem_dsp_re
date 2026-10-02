/* Fixed owner-hook witnesses: twenty real factory modes, all four
 * constructor/destructor side combinations. No resampling or planted history.
 * Empty-handle probes below are explicitly synthetic guard checks. */
#include "harness.h"
#include "dsplib/fixedrc.h"
#include "dsplib/sysdep.h"

extern struct rc *ref_RcFixed_Create(int mode);
extern void ref_RcFixed_Delete(struct rc *h);

static struct alloc_log
cycle(int mode, int create_ref, int delete_ref)
{
	struct rc *h;
	harness_alloc_reset();
	h = create_ref ? ref_RcFixed_Create(mode) : RcFixed_Create(mode);
	diff_eq_int("mode %ld constructed", h != NULL, 1, mode);
	diff_eq_int("mode %ld owned blocks", harness_alloc.allocs,
		    mode <= 1 ? 5 : 2, mode);
	if (delete_ref)
		ref_RcFixed_Delete(h);
	else
		RcFixed_Delete(h);
	return harness_alloc;
}

int
main(void)
{
	int mode, cr, dr, rc;
	diff_begin("RcFixed ownership: 80 real cycles, four synthetic/null guards");
	for (mode = 0; mode < RCFIXED_NMODES; mode++) {
		struct alloc_log expected = cycle(mode, 1, 1);
		for (cr = 0; cr <= 1; cr++) {
			for (dr = 0; dr <= 1; dr++) {
				struct alloc_log got;
				if (cr == 1 && dr == 1)
					got = expected;
				else
					got = cycle(mode, cr, dr);
				diff_eq_obj("owner hooks", struct alloc_log,
					    &got, &expected, mode * 4 + cr * 2 + dr);
				diff_eq_int("mode %ld no live blocks", got.live, 0, mode);
				diff_eq_int("mode %ld frees all", got.frees, got.allocs, mode);
				diff_eq_int("mode %ld no bad free", got.bad_free, 0, mode);
				diff_eq_int("mode %ld no null free", got.free_null, 0, mode);
				diff_eq_int("mode %ld no overflow", got.overflow, 0, mode);
			}
		}
	}
	for (dr = 0; dr <= 1; dr++) {
		struct rc *h;
		harness_alloc_reset();
		/* A raw zeroed two-pointer-sized record has kind0/stateNULL.
		 * This is a synthetic component guard probe, not a factory result. */
		h = sysdep_malloc(2 * sizeof(void *));
		memset(h, 0, 2 * sizeof(void *));
		if (dr) ref_RcFixed_Delete(h); else RcFixed_Delete(h);
		diff_eq_int("empty guard %ld frees handle", harness_alloc.frees, 1, dr);
		diff_eq_int("empty guard %ld skips state free", harness_alloc.free_null, 0, dr);
		diff_eq_int("empty guard %ld no leak", harness_alloc.live, 0, dr);
		harness_alloc_reset();
		if (dr) ref_RcFixed_Delete(NULL); else RcFixed_Delete(NULL);
		diff_eq_int("null guard %ld no allocations", harness_alloc.allocs, 0, dr);
		diff_eq_int("null guard %ld no free", harness_alloc.frees, 0, dr);
		diff_eq_int("null guard %ld no null free", harness_alloc.free_null, 0, dr);
	}
	rc = diff_end();
	printf("ownership denominator: 80 real cycles, 2 synthetic empty handles, 2 null handles\n");
	return rc;
}
