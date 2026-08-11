/*
 * t_v92alloc.c -- differential test of the six leaf allocators: the four V.92
 * parameter-info array functions and the K56flex create/delete pair.
 *
 * WHAT IS HARD ABOUT TESTING A MALLOC WRAPPER, AND HOW THIS FILE AVOIDS IT.
 * Both sides call the SAME `sysdep_malloc` -- it is in symmap.py's
 * SHARED_IMPORTS and sharing an allocator between the two sides is safe -- so
 * the two runs get different addresses and every pointer field differs for a
 * reason that is not a defect.  The answer is finding 224's, applied to
 * pointers: compare what the pointer MEANS, not what it is.
 *
 *   - the creators are compared with each stored pointer CANONICALISED to its
 *     slot index, after checking separately that it is non-null, distinct from
 *     its siblings, and in the allocator's live set.  Every other byte of the
 *     object is compared as itself.
 *   - the deleters are compared with NO canonicalisation at all, because the
 *     fixture is driven with pointers the allocator never handed out.  The
 *     harness swallows a free of an unknown pointer and counts it, so the two
 *     sides can be given byte-identical fixtures and compared byte for byte.
 *     That is the strongest form available and it is available only because
 *     the deleters do not write anything back.
 *
 * WHAT IS NOT CHECKED, STATED PLAINLY.  The harness's allocation log counts
 * calls and totals bytes; it does not report a per-call size.  So "six
 * allocations of 512" is proved as "six allocations, 3072 bytes, and the blob
 * says the same", not element by element.  A reconstruction that asked for 256
 * and 768 alternately would total 3072 and pass -- but it would differ from
 * the blob's total the moment the counts stopped cancelling, and the blob's
 * total is measured here rather than asserted from the disassembly.
 *
 * THE ORDER OF THE SIX MALLOCS IS NOT CHECKED AND THERE IS NOTHING TO CHECK.
 * All six constellation requests are the same size and all four coefficient
 * requests are the same size, so which result lands in which slot is not
 * observable and not a property of the object.
 *
 * THE DELETERS LEAVE THE POINTERS DANGLING and that is asserted rather than
 * tolerated: `run_delete_live` checks that each slot still holds the pointer
 * it was given AFTER the free.  A reconstruction that helpfully nulled the
 * slot would pass every count in this file and fail exactly there.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/sysdep.h"

/*
 * `dsplib/K56FlexFloModem.h` declares a class and cannot be included from C,
 * so the two C-linkage helpers it also declares are repeated here.  The size
 * is written as the literal from .text+0x102a3 rather than taken from the
 * header on purpose: a test that imports the constant it is checking cannot
 * catch the constant changing.
 */
extern void *K56FLEX_Create(void *, void *, void *, int);
extern void K56FLEX_Delete(void *obj);
#define K56FLEX_BLOCK_BYTES	0x14

extern void ref_V92createConstellations(struct V92ParamsInfo *p);
extern void ref_V92createFilterCoefficients(struct V92ParamsInfo *p);
extern void ref_V92deleteConstellations(struct V92ParamsInfo *p);
extern void ref_V92deleteFilterCoefficients(struct V92ParamsInfo *p);
extern void *ref_K56FLEX_Create(void *, void *, void *, int);
extern void ref_K56FLEX_Delete(void *obj);

/*
 * The block plus a guard region.  A store one slot past the end of the
 * constellation array lands in `guard` and fails the comparison, instead of
 * running off into whatever the stack held.
 */
#define GUARD 64

struct fixture {
	struct V92ParamsInfo p;
	unsigned char guard[GUARD];
};

/* A cheap LCG: varied bytes, never zero-filled, and reproducible per seed. */
static void
seedfill(void *p, size_t n, unsigned seed)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++) {
		seed = seed * 1103515245u + 12345u;
		b[i] = (unsigned char)((seed >> 16) & 0xff);
		if (b[i] == 0)
			b[i] = (unsigned char)(0xa0 | (i & 0x0f));
	}
}

static int
in_live_set(void *q)
{
	void *live[64];
	int n, i;

	n = harness_alloc_live_set(live, 64);
	if (n > 64)
		n = 64;
	for (i = 0; i < n; i++)
		if (live[i] == q)
			return 1;
	return 0;
}

/*
 * ============================================================================
 * The creators.
 * ============================================================================
 */

/*
 * Run one creator on a fresh fixture, check the pointers it stored, hand them
 * back to the allocator, and replace each slot with its index so that what is
 * left can be compared byte for byte against the other side's.
 *
 * `slots` is where in the fixture the pointers are; `nslots` how many.
 */
struct create_result {
	struct fixture f;	/* canonicalised			*/
	int allocs;
	unsigned bytes;
	int live;
	int distinct;		/* pointers unequal to every sibling	*/
	int nonnull;
	int tracked;		/* pointers the allocator admits to	*/
	int untouched;		/* bytes outside the slots left alone	*/
};

static void
run_create(void (*fn)(struct V92ParamsInfo *), size_t slotoff, int nslots,
	   unsigned seed, struct create_result *out)
{
	struct fixture before;
	void *got[8];
	int i, j;
	size_t k;

	seedfill(&out->f, sizeof(out->f), seed);
	memcpy(&before, &out->f, sizeof(before));

	harness_alloc_reset();
	fn(&out->f.p);

	out->allocs = harness_alloc.allocs;
	out->bytes = harness_alloc.bytes;
	out->live = harness_alloc.live;

	for (i = 0; i < nslots; i++)
		memcpy(&got[i], (unsigned char *)&out->f + slotoff
		       + (size_t)i * sizeof(void *), sizeof(void *));

	out->nonnull = 0;
	out->tracked = 0;
	out->distinct = 0;
	for (i = 0; i < nslots; i++) {
		if (got[i] != 0)
			out->nonnull++;
		if (in_live_set(got[i]))
			out->tracked++;
		for (j = 0; j < nslots; j++)
			if (j != i && got[j] == got[i])
				break;
		if (j == nslots)
			out->distinct++;
	}

	/*
	 * Every byte that is not one of the stored pointers must still hold
	 * what the seed put there.  This is checked against the SEED and not
	 * against the other side, so a store both sides make is still caught.
	 */
	out->untouched = 1;
	for (k = 0; k < sizeof(out->f); k++) {
		if (k >= slotoff
		    && k < slotoff + (size_t)nslots * sizeof(void *))
			continue;
		if (((unsigned char *)&out->f)[k]
		    != ((unsigned char *)&before)[k])
			out->untouched = 0;
	}

	for (i = 0; i < nslots; i++)
		sysdep_free(got[i]);

	/* Canonicalise: slot k now reads k + 1, or 0 if it held NULL. */
	for (i = 0; i < nslots; i++) {
		void *token = got[i] == 0 ? (void *)0
					  : (void *)(long)(i + 1);

		memcpy((unsigned char *)&out->f + slotoff
		       + (size_t)i * sizeof(void *), &token, sizeof(void *));
	}
}

static int
check_create(const char *what, void (*ours)(struct V92ParamsInfo *),
	     void (*theirs)(struct V92ParamsInfo *), size_t slotoff,
	     int nslots, unsigned wantbytes)
{
	struct create_result a, b, a2;
	unsigned seed;

	diff_begin(what);

	for (seed = 0x51ed3b17u; seed != 0x51ed3b1au; seed++) {
		run_create(ours, slotoff, nslots, seed, &a);
		run_create(theirs, slotoff, nslots, seed, &b);

		diff_eq_obj("the block after the call", struct fixture, &a.f,
			    &b.f, (long)seed);

		diff_eq_int("allocations (seed %lx)", a.allocs, b.allocs,
			    (long)seed);
		diff_eq_int("bytes requested (seed %lx)", a.bytes, b.bytes,
			    (long)seed);
		diff_eq_int("live after the call (seed %lx)", a.live, b.live,
			    (long)seed);

		diff_eq_int("we allocate %ld times", a.allocs, nslots, nslots);
		diff_eq_int("the blob allocates %ld times", b.allocs, nslots,
			    nslots);
		diff_eq_int("we request %ld bytes", (long)a.bytes,
			    (long)wantbytes, (long)wantbytes);
		diff_eq_int("the blob requests %ld bytes", (long)b.bytes,
			    (long)wantbytes, (long)wantbytes);

		diff_eq_int("every slot is non-null (ours)", a.nonnull, nslots,
			    nslots);
		diff_eq_int("every slot is non-null (blob)", b.nonnull, nslots,
			    nslots);
		diff_eq_int("every slot is distinct (ours)", a.distinct,
			    nslots, nslots);
		diff_eq_int("every slot is distinct (blob)", b.distinct,
			    nslots, nslots);
		diff_eq_int("every slot came from the allocator (ours)",
			    a.tracked, nslots, nslots);
		diff_eq_int("every slot came from the allocator (blob)",
			    b.tracked, nslots, nslots);

		diff_eq_int("nothing outside the slots is written (ours)",
			    a.untouched, 1, (long)seed);
		diff_eq_int("nothing outside the slots is written (blob)",
			    b.untouched, 1, (long)seed);
	}

	/*
	 * Anti-vacuity, findings 223 and 224.  The call must CHANGE the block,
	 * and the block must not come out the same whatever went in -- either
	 * would make every comparison above a comparison of memory nobody
	 * wrote.
	 */
	{
		struct fixture virgin;

		seedfill(&virgin, sizeof(virgin), 0x51ed3b17u);
		run_create(ours, slotoff, nslots, 0x51ed3b17u, &a);
		run_create(ours, slotoff, nslots, 0x0badc0deu, &a2);

		diff_eq_int("the call changed the block",
			    memcmp(&a.f, &virgin, sizeof(virgin)) != 0, 1, 0);
		diff_eq_int("a different input gives a different block",
			    memcmp(&a.f, &a2.f, sizeof(a.f)) != 0, 1, 0);
	}

	return diff_end();
}

/*
 * ============================================================================
 * The deleters, driven with pointers the allocator never issued.
 *
 * `sysdep_free` counts those and swallows them, so the fixture is untouched by
 * anything but the function under test and the two sides can be compared as
 * bytes.  `mode` picks which slots are NULL: 0 none, 1 all, 2 alternate.
 * ============================================================================
 */
static void
poison(struct fixture *f, size_t slotoff, int nslots, unsigned seed, int mode)
{
	int i;

	seedfill(f, sizeof(*f), seed);
	for (i = 0; i < nslots; i++) {
		void *v;

		if (mode == 1 || (mode == 2 && (i & 1)))
			v = (void *)0;
		else
			v = (void *)(long)(0x40000000 + 0x1000 * (i + 1)
					   + (int)(seed & 0xff0));
		memcpy((unsigned char *)f + slotoff
		       + (size_t)i * sizeof(void *), &v, sizeof(void *));
	}
}

static int
check_delete_bytes(const char *what, void (*ours)(struct V92ParamsInfo *),
		   void (*theirs)(struct V92ParamsInfo *), size_t slotoff,
		   int nslots)
{
	struct fixture a, b, before;
	int mode;

	diff_begin(what);

	for (mode = 0; mode <= 2; mode++) {
		int wantnull, wantbad;
		int abad, anull, bbad, bnull;

		wantnull = mode == 1 ? nslots
				     : (mode == 2 ? nslots / 2 : 0);
		wantbad = nslots - wantnull;

		poison(&a, slotoff, nslots, 0x2f19a3c5u + (unsigned)mode, mode);
		poison(&b, slotoff, nslots, 0x2f19a3c5u + (unsigned)mode, mode);
		memcpy(&before, &a, sizeof(before));

		harness_alloc_reset();
		ours(&a.p);
		abad = harness_alloc.bad_free;
		anull = harness_alloc.free_null;
		diff_eq_int("we free nothing real (mode %ld)",
			    harness_alloc.frees, 0, mode);

		harness_alloc_reset();
		theirs(&b.p);
		bbad = harness_alloc.bad_free;
		bnull = harness_alloc.free_null;
		diff_eq_int("the blob frees nothing real (mode %ld)",
			    harness_alloc.frees, 0, mode);

		diff_eq_obj("the block after the call", struct fixture, &a, &b,
			    mode);
		diff_eq_int("the block is untouched (mode %ld)",
			    memcmp(&a, &before, sizeof(before)) == 0, 1, mode);

		diff_eq_int("frees of unknown pointers agree (mode %ld)", abad,
			    bbad, mode);
		diff_eq_int("frees of NULL agree (mode %ld)", anull, bnull,
			    mode);
		diff_eq_int("we call free once per non-null slot (%ld)", abad,
			    wantbad, wantbad);
		diff_eq_int("the blob calls free once per non-null slot (%ld)",
			    bbad, wantbad, wantbad);

		/*
		 * THE NULL ARM MAKES NO CALL AT ALL, which is the thing this
		 * mode exists to show.  `harness_alloc.free_null` counts
		 * `sysdep_free(NULL)`, so a version that delegated the test to
		 * the allocator -- `sysdep_free(p->x)` unconditionally, which
		 * is legal and behaves the same -- would score `wantnull`
		 * here.  The object tests the pointer itself at
		 * .text+0x12dee and its five siblings, so the count is zero
		 * whatever `mode` puts in the slots.
		 */
		diff_eq_int("we never reach free with NULL (mode %ld, %ld "
			    "null slots)", anull, 0, wantnull);
		diff_eq_int("the blob never reaches free with NULL (mode %ld)",
			    bnull, 0, mode);
	}

	return diff_end();
}

/*
 * The same functions against REAL allocations, which is the only way to see
 * that the free reaches the allocator at all.  The two sides cannot share the
 * blocks -- the first side frees them -- so this compares outcomes rather than
 * bytes, and it is the check that the dangling slots survive.
 */
static void
run_delete_live(void (*fn)(struct V92ParamsInfo *), size_t slotoff, int nslots,
		unsigned size, int *frees, int *live, int *bad, int *kept)
{
	struct fixture f;
	void *blocks[8];
	int i;

	seedfill(&f, sizeof(f), 0x77c1e502u);
	harness_alloc_reset();
	for (i = 0; i < nslots; i++) {
		blocks[i] = sysdep_malloc(size);
		memcpy((unsigned char *)&f + slotoff
		       + (size_t)i * sizeof(void *), &blocks[i],
		       sizeof(void *));
	}

	fn(&f.p);

	*frees = harness_alloc.frees;
	*live = harness_alloc.live;
	*bad = harness_alloc.bad_free;

	*kept = 0;
	for (i = 0; i < nslots; i++) {
		void *now;

		memcpy(&now, (unsigned char *)&f + slotoff
		       + (size_t)i * sizeof(void *), sizeof(void *));
		if (now == blocks[i])
			(*kept)++;
	}
}

static int
check_delete_live(const char *what, void (*ours)(struct V92ParamsInfo *),
		  void (*theirs)(struct V92ParamsInfo *), size_t slotoff,
		  int nslots, unsigned size)
{
	int af, al, ab, ak, bf, bl, bb, bk;

	diff_begin(what);

	run_delete_live(ours, slotoff, nslots, size, &af, &al, &ab, &ak);
	run_delete_live(theirs, slotoff, nslots, size, &bf, &bl, &bb, &bk);

	diff_eq_int("frees agree", af, bf, 0);
	diff_eq_int("live afterwards agrees", al, bl, 0);
	diff_eq_int("bad frees agree", ab, bb, 0);
	diff_eq_int("dangling slots agree", ak, bk, 0);

	diff_eq_int("we free all %ld blocks", af, nslots, nslots);
	diff_eq_int("the blob frees all %ld blocks", bf, nslots, nslots);
	diff_eq_int("nothing is left live (ours)", al, 0, 0);
	diff_eq_int("nothing is left live (blob)", bl, 0, 0);
	diff_eq_int("no bad free (ours)", ab, 0, 0);
	diff_eq_int("no bad free (blob)", bb, 0, 0);

	/*
	 * THE POINT OF THIS FUNCTION.  Neither deleter nulls the slot it just
	 * freed -- there is no store to any of those offsets in either -- so
	 * all of them still hold the freed pointer.
	 */
	diff_eq_int("we leave all %ld slots dangling", ak, nslots, nslots);
	diff_eq_int("the blob leaves all %ld slots dangling", bk, nslots,
		    nslots);

	return diff_end();
}

/*
 * ============================================================================
 * K56FLEX_Create and K56FLEX_Delete.
 * ============================================================================
 */
static int
run_k56(void)
{
	unsigned char args[3][32], argsbefore[3][32];
	void *ours, *theirs;
	int oallocs, ballocs, i;
	unsigned obytes, bbytes;

	diff_begin("K56FLEX_Create and K56FLEX_Delete");

	for (i = 0; i < 3; i++)
		seedfill(args[i], sizeof(args[i]), 0x1c0ffee0u + (unsigned)i);
	memcpy(argsbefore, args, sizeof(args));

	harness_alloc_reset();
	ours = K56FLEX_Create(args[0], args[1], args[2], 0x1234);
	oallocs = harness_alloc.allocs;
	obytes = harness_alloc.bytes;
	diff_eq_int("our block is in the live set", in_live_set(ours), 1, 0);

	harness_alloc_reset();
	theirs = ref_K56FLEX_Create(args[0], args[1], args[2], 0x1234);
	ballocs = harness_alloc.allocs;
	bbytes = harness_alloc.bytes;
	diff_eq_int("the blob's block is in the live set", in_live_set(theirs),
		    1, 0);

	diff_eq_int("allocations agree", oallocs, ballocs, 0);
	diff_eq_int("bytes agree", (long)obytes, (long)bbytes, 0);
	diff_eq_int("exactly one allocation", oallocs, 1, 0);
	diff_eq_int("of %ld bytes", (long)obytes, K56FLEX_BLOCK_BYTES,
		    K56FLEX_BLOCK_BYTES);
	diff_eq_int("we return non-null", ours != 0, 1, 0);
	diff_eq_int("the blob returns non-null", theirs != 0, 1, 0);
	diff_eq_int("the two blocks are not the same block", ours != theirs, 1,
		    0);

	/*
	 * None of the four arguments is read, so none of the three buffers can
	 * have changed.  Asserted rather than assumed: a reconstruction that
	 * "initialised" the object through one of them would pass every count
	 * above.
	 */
	diff_eq_int("neither side touched its arguments",
		    memcmp(args, argsbefore, sizeof(args)) == 0, 1, 0);

	/* The delete pair: the real block, an unknown pointer, and NULL. */
	harness_alloc_reset();
	{
		void *live = sysdep_malloc(K56FLEX_BLOCK_BYTES);

		K56FLEX_Delete(live);
		diff_eq_int("we free a live block", harness_alloc.frees, 1, 0);
		diff_eq_int("nothing is left live", harness_alloc.live, 0, 0);
	}
	harness_alloc_reset();
	{
		void *live = sysdep_malloc(K56FLEX_BLOCK_BYTES);

		ref_K56FLEX_Delete(live);
		diff_eq_int("the blob frees a live block", harness_alloc.frees,
			    1, 0);
		diff_eq_int("the blob leaves nothing live", harness_alloc.live,
			    0, 0);
	}

	/*
	 * The null arm: the test is at .text+0x102c7 and is the function's,
	 * so a NULL argument reaches the allocator in no form whatever --
	 * not as a free, not as a bad free, and not as `sysdep_free(NULL)`.
	 */
	harness_alloc_reset();
	K56FLEX_Delete((void *)0);
	diff_eq_int("we never reach free with NULL", harness_alloc.free_null,
		    0, 0);
	diff_eq_int("and free nothing", harness_alloc.frees, 0, 0);
	diff_eq_int("and report no bad free", harness_alloc.bad_free, 0, 0);
	harness_alloc_reset();
	ref_K56FLEX_Delete((void *)0);
	diff_eq_int("the blob never reaches free with NULL",
		    harness_alloc.free_null, 0, 0);
	diff_eq_int("and frees nothing", harness_alloc.frees, 0, 0);
	diff_eq_int("and reports no bad free", harness_alloc.bad_free, 0, 0);

	harness_alloc_reset();
	K56FLEX_Delete((void *)0x40100000);
	diff_eq_int("we take the free arm on an unknown pointer",
		    harness_alloc.bad_free, 1, 0);
	harness_alloc_reset();
	ref_K56FLEX_Delete((void *)0x40100000);
	diff_eq_int("the blob takes the free arm on an unknown pointer",
		    harness_alloc.bad_free, 1, 0);

	/*
	 * The two blocks the create checks made are deliberately left
	 * outstanding: `harness_alloc_reset` has already forgotten them, so
	 * freeing them here would be counted as a bad free rather than
	 * returning them, and there is nothing left to measure.
	 */
	return diff_end();
}

int
main(void)
{
	int rc = 0;
	size_t coff = offsetof(struct fixture, p.constellations);
	size_t foff = offsetof(struct fixture, p.filterCoefficients);

	/* The block is exactly what V92Modem's constructor allocates. */
	diff_begin("the parameter-info block's size");
	diff_eq_int("sizeof(struct V92ParamsInfo) is %ld",
		    (long)sizeof(struct V92ParamsInfo), 0xb4, 0xb4);
	diff_eq_int("the constellation array is at +0x%lx", (long)coff, 0x84,
		    0x84);
	diff_eq_int("the coefficient array is at +0x%lx", (long)foff, 0x5c,
		    0x5c);
	rc |= diff_end();

	rc |= check_create("V92createConstellations", V92createConstellations,
			   ref_V92createConstellations, coff,
			   V92_PARAMSINFO_CONSTELLATIONS,
			   V92_PARAMSINFO_CONSTELLATIONS
			   * V92_PARAMSINFO_CONSTELLATION_SZ);
	rc |= check_create("V92createFilterCoefficients",
			   V92createFilterCoefficients,
			   ref_V92createFilterCoefficients, foff,
			   V92_PARAMSINFO_FILTERCOEFS,
			   V92_PARAMSINFO_FILTERCOEFS
			   * V92_PARAMSINFO_FILTERCOEF_SZ);

	rc |= check_delete_bytes("V92deleteConstellations",
				 V92deleteConstellations,
				 ref_V92deleteConstellations, coff,
				 V92_PARAMSINFO_CONSTELLATIONS);
	rc |= check_delete_bytes("V92deleteFilterCoefficients",
				 V92deleteFilterCoefficients,
				 ref_V92deleteFilterCoefficients, foff,
				 V92_PARAMSINFO_FILTERCOEFS);

	rc |= check_delete_live("V92deleteConstellations on live blocks",
				V92deleteConstellations,
				ref_V92deleteConstellations, coff,
				V92_PARAMSINFO_CONSTELLATIONS,
				V92_PARAMSINFO_CONSTELLATION_SZ);
	rc |= check_delete_live("V92deleteFilterCoefficients on live blocks",
				V92deleteFilterCoefficients,
				ref_V92deleteFilterCoefficients, foff,
				V92_PARAMSINFO_FILTERCOEFS,
				V92_PARAMSINFO_FILTERCOEF_SZ);

	rc |= run_k56();

	return rc;
}
