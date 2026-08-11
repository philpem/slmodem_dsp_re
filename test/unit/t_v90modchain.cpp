/*
 * t_v90modchain.cpp -- differential test of the V.90 modulator construction
 * chain: V90Mapper, V90BitsToSymbol, V90Phase4Modulator and V90Modulator,
 * all four symbol variants (C1, C2, D1, D2) of each.
 *
 * C++ HAS NO SYNTAX FOR RUNNING A CONSTRUCTOR OVER STORAGE THAT ALREADY
 * EXISTS, so both sides are called by symbol through asm() labels over raw
 * static blocks, exactly as t_v90cp.cpp does.  `OBJ = V90Mapper(args)` would
 * build a temporary over uninitialised stack and copy the whole object, which
 * destroys the one property this fixture depends on.
 *
 * THE SEED IS THE TEST.  Both sides are filled with the SAME varied
 * pseudorandom bytes before every trial and are NEVER zeroed: a zero-filled
 * object would let a store that never happened pass, because the field was
 * already zero, and would make "did anything happen" unanswerable (findings
 * 223 and 224).  Every block carries 64 bytes of guard past the object, and
 * both sides' guards are compared against the seed as well as against each
 * other.
 *
 * THE HEAP POINTERS CANNOT AGREE AND ARE NOT ASKED TO.  `sysdep_malloc` in
 * the harness is plain `malloc`, so our side's allocations and the blob's are
 * at different addresses for ever.  Two mechanisms deal with that:
 *
 *   - A field KNOWN to hold an allocation is replaced by a constant in a copy
 *     of each side, and then asserted separately for being non-null and for
 *     being distinct from the class's other allocations.
 *   - A field inside a FOREIGN subobject -- the V90SpectralShaper embedded in
 *     V90Mapper, whose internals belong to t_v90spectral -- is found by
 *     value: every aligned word that holds a pointer the harness allocator
 *     handed out AND still owns, on BOTH sides, is replaced.  That needs no
 *     hand-maintained offset list and cannot go stale when that class grows a
 *     buffer.  A word that is not a pointer but coincides with one would have
 *     to coincide on both sides at the same offset to be masked.
 *
 * Everything else -- every borrowed pointer, every count, every flag, and
 * every byte the constructor did not touch -- is compared whole.
 *
 * THE ARGUMENTS ARE SHARED AND DISTINGUISHABLE.  Both sides are handed the
 * SAME instance of each pointed-to object, so a borrowed pointer compares by
 * value and an argument landing at the wrong offset fails instead of
 * agreeing.
 *
 * NO WILD POINTERS.  t_v90cp can hand a destructor six seeded garbage
 * pointers because it only frees them; these destructors DEREFERENCE what
 * they free -- `mapper->~V90Mapper()` before `sysdep_free(mapper)` -- so a
 * wild value is a segfault and not a test.  The null guards are driven
 * instead over a fully constructed object with one pointer at a time released
 * by hand and nulled, which exercises both arms of every guard, keeps the
 * live count returning to zero, and reads the difference off the harness
 * allocator's counters.
 *
 * WHY THE POST-DESTRUCTOR COMPARISON IS PER-SIDE.  After the call the
 * allocations are gone, so the mask that made the two sides comparable can no
 * longer be computed -- the pointers are no longer live.  The claim is made
 * the other way instead, and it is the stronger one: EACH side is compared
 * against ITSELF before the call, which says the destructor stored nothing at
 * all, where the two sides being equal would not.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90Mapper.h"
#include "dsplib/V90Parameters.h"

extern "C" {
void our_mapper_c1(void *, V90Parameters *)
	asm("_ZN9V90MapperC1EP13V90Parameters");
void our_mapper_c2(void *, V90Parameters *)
	asm("_ZN9V90MapperC2EP13V90Parameters");
void our_mapper_d1(void *) asm("_ZN9V90MapperD1Ev");
void our_mapper_d2(void *) asm("_ZN9V90MapperD2Ev");
void ref_mapper_c1(void *, V90Parameters *)
	asm("ref__ZN9V90MapperC1EP13V90Parameters");
void ref_mapper_c2(void *, V90Parameters *)
	asm("ref__ZN9V90MapperC2EP13V90Parameters");
void ref_mapper_d1(void *) asm("ref__ZN9V90MapperD1Ev");
void ref_mapper_d2(void *) asm("ref__ZN9V90MapperD2Ev");

void our_bts_c1(void *, unsigned int, V90Parameters *)
	asm("_ZN15V90BitsToSymbolC1EjP13V90Parameters");
void our_bts_c2(void *, unsigned int, V90Parameters *)
	asm("_ZN15V90BitsToSymbolC2EjP13V90Parameters");
void our_bts_d1(void *) asm("_ZN15V90BitsToSymbolD1Ev");
void our_bts_d2(void *) asm("_ZN15V90BitsToSymbolD2Ev");
void ref_bts_c1(void *, unsigned int, V90Parameters *)
	asm("ref__ZN15V90BitsToSymbolC1EjP13V90Parameters");
void ref_bts_c2(void *, unsigned int, V90Parameters *)
	asm("ref__ZN15V90BitsToSymbolC2EjP13V90Parameters");
void ref_bts_d1(void *) asm("ref__ZN15V90BitsToSymbolD1Ev");
void ref_bts_d2(void *) asm("ref__ZN15V90BitsToSymbolD2Ev");
}

typedef void (*mapper_ctor)(void *, V90Parameters *);
typedef void (*bts_ctor)(void *, unsigned int, V90Parameters *);
typedef void (*dtor)(void *);

#define GUARD		64
#define MAPPER_SIZE	0x704u
#define BTS_SIZE	0x24u
#define MAPPER_SLOT	(MAPPER_SIZE + GUARD)
#define BTS_SLOT	(BTS_SIZE + GUARD)
#define NTRIAL		16

/* The V90SpectralShaper embedded in V90Mapper: +0x68c, 0x6c bytes. */
#define SHAPER_LO	0x68cu
#define SHAPER_HI	0x6f8u

static unsigned char map_a[MAPPER_SLOT] __attribute__((aligned(8)));
static unsigned char map_b[MAPPER_SLOT] __attribute__((aligned(8)));
static unsigned char map_seed[MAPPER_SLOT];
static unsigned char bts_a[BTS_SLOT] __attribute__((aligned(8)));
static unsigned char bts_b[BTS_SLOT] __attribute__((aligned(8)));
static unsigned char bts_seed[BTS_SLOT];

/* Scratch for the canonicalised copies, big enough for the largest block. */
static unsigned char cmp_a[MAPPER_SLOT];
static unsigned char cmp_b[MAPPER_SLOT];

/* The one shared V90Parameters both sides are handed. */
static unsigned char par_store[sizeof(V90Parameters)]
	__attribute__((aligned(8)));
#define PARAMS		((V90Parameters *)(void *)par_store)

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * Fill both sides and the record of what was put there.  `mode` varies how
 * the bytes are chosen, because a store of a constant is invisible against a
 * seed that already holds it: mode 1 seeds 0x01 everywhere, which is what the
 * two constructors write to their trailing flag byte, and mode 2 seeds 0xff.
 * None of the four is zero.
 */
static void
fill(unsigned char *a, unsigned char *b, unsigned char *rec, unsigned n,
     int mode)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		unsigned char v;

		switch (mode) {
		case 1:
			v = 0x01;	/* extraSymbolsPending's value */
			break;
		case 2:
			v = 0xff;
			break;
		case 3:
			v = (unsigned char)(next_byte() | 1u);
			break;
		default:
			v = next_byte();
			break;
		}
		a[i] = v;
		b[i] = v;
		if (rec)
			rec[i] = v;
	}
}

static void
seed_trial(int trial)
{
	lfsr_state = 0x4d1u + 0x9e37u * (unsigned)trial;
	fill(map_a, map_b, map_seed, MAPPER_SLOT, trial & 3);
	fill(bts_a, bts_b, bts_seed, BTS_SLOT, (trial + 1) & 3);
	fill(par_store, par_store, (unsigned char *)0, sizeof(par_store),
	     trial & 3);
}

/* ------------------------------------------------ the live allocation set */

#define MAXLIVE	512
static void *live[MAXLIVE];
static int nlive;

static void
live_refresh(void)
{
	nlive = harness_alloc_live_set(live, MAXLIVE);
	if (nlive > MAXLIVE)
		nlive = MAXLIVE;
}

static int
word_is_live(const unsigned char *w)
{
	void *p;
	int i;

	memcpy(&p, w, sizeof(p));
	if (p == 0)
		return 0;
	for (i = 0; i < nlive; i++)
		if (live[i] == p)
			return 1;
	return 0;
}

static void
mask_word(unsigned char *a, unsigned char *b, unsigned off)
{
	memset(a + off, 0x5a, sizeof(void *));
	memset(b + off, 0x5a, sizeof(void *));
}

/* Every aligned word in [lo, hi) that holds a live allocation on both sides. */
static void
mask_live(unsigned char *a, unsigned char *b, unsigned lo, unsigned hi)
{
	unsigned o;

	for (o = lo; o + sizeof(void *) <= hi; o += sizeof(void *))
		if (word_is_live(a + o) && word_is_live(b + o))
			mask_word(a, b, o);
}

/* V90Mapper: the 0x50 buffer, plus whatever the spectral shaper allocated. */
static void
canon_mapper(unsigned char *a, unsigned char *b)
{
	mask_word(a, b, 0x018);
	mask_live(a, b, SHAPER_LO, SHAPER_HI);
}

/* V90BitsToSymbol: the mapper it owns and the symbol buffer. */
static void
canon_bts(unsigned char *a, unsigned char *b)
{
	mask_word(a, b, 0x00);
	mask_word(a, b, 0x08);
}

static void *
slot_ptr(const unsigned char *o, unsigned off)
{
	void *p;

	memcpy(&p, o + off, sizeof(p));
	return p;
}

static void
compare_mapper(const char *what, const unsigned char *a, const unsigned char *b,
	       long tag)
{
	memcpy(cmp_a, a, MAPPER_SIZE);
	memcpy(cmp_b, b, MAPPER_SIZE);
	canon_mapper(cmp_a, cmp_b);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Mapper", cmp_a, cmp_b,
		     (size_t)MAPPER_SIZE, tag);
}

static void
compare_bts(const char *what, const unsigned char *a, const unsigned char *b,
	    long tag)
{
	memcpy(cmp_a, a, BTS_SIZE);
	memcpy(cmp_b, b, BTS_SIZE);
	canon_bts(cmp_a, cmp_b);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90BitsToSymbol", cmp_a, cmp_b,
		     (size_t)BTS_SIZE, tag);
}

/* The guard, on BOTH sides, against what the seed left there. */
static int
guard_intact(const unsigned char *a, const unsigned char *b,
	     const unsigned char *rec, unsigned size, unsigned slot)
{
	return memcmp(a + size, rec + size, slot - size) == 0
	    && memcmp(b + size, rec + size, slot - size) == 0;
}

/* ================================================================ V90Mapper */

static int
run_mapper_ctor(const char *name, mapper_ctor our_c, mapper_ctor ref_c,
		dtor our_d, dtor ref_d)
{
	unsigned char first[MAPPER_SLOT];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes;

		seed_trial(trial);
		harness_alloc_reset();

		our_c(map_a, PARAMS);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		ref_c(map_b, PARAMS);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		live_refresh();

		/*
		 * The class allocates 0x50 for itself and hands the rest to
		 * V90SpectralShaper, so no literal count belongs here; what IS
		 * absolute is that both sides did the same thing, in the same
		 * number of pieces and to the same total.
		 */
		diff_eq_int("allocations match the blob (trial %ld)",
			    a_allocs, b_allocs, trial);
		diff_eq_int("bytes allocated match the blob (trial %ld)",
			    (int)a_bytes, (int)b_bytes, trial);
		diff_eq_int("the 0x50 buffer and the shaper's two, at least "
			    "(trial %ld)", a_allocs >= 3, 1, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		diff_eq_int("live set did not overflow (trial %ld)",
			    harness_alloc.overflow, 0, trial);

		diff_eq_int("buf is not null (trial %ld)",
			    slot_ptr(map_a, 0x018) != 0, 1, trial);
		diff_eq_int("ref buf is not null (trial %ld)",
			    slot_ptr(map_b, 0x018) != 0, 1, trial);
		diff_eq_int("params is the argument (trial %ld)",
			    slot_ptr(map_a, 0x000) == (void *)PARAMS, 1, trial);
		diff_eq_int("ref params is the argument (trial %ld)",
			    slot_ptr(map_b, 0x000) == (void *)PARAMS, 1, trial);

		compare_mapper("after the constructor", map_a, map_b, trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(map_a, map_b, map_seed, MAPPER_SIZE,
					 MAPPER_SLOT), 1, trial);

		if (memcmp(map_seed, map_a, MAPPER_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, map_a, MAPPER_SLOT);
		else if (memcmp(first, map_a, MAPPER_SLOT) != 0)
			distinct = 1;

		our_d(map_a);
		ref_d(map_b);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * The destructor over what the constructor built, with `buf` either live or
 * released by hand and nulled.  Nulling it must remove exactly one free and
 * must NOT turn into a `sysdep_free(NULL)`: counting the two apart is what
 * makes the guard testable.  The spectral shaper's own frees are identical in
 * both arms, so the DIFFERENCE between them is this class's contribution and
 * nothing else.
 */
static int
run_mapper_dtor(const char *name, mapper_ctor our_c, mapper_ctor ref_c,
		dtor our_d, dtor ref_d)
{
	unsigned char before_a[MAPPER_SLOT], before_b[MAPPER_SLOT];
	int trial, saw_live = 0, saw_null = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int base_frees = 0, arm;

		for (arm = 0; arm < 2; arm++) {
			int a_frees, b_frees, a_null, b_null;

			seed_trial(trial);
			harness_alloc_reset();
			our_c(map_a, PARAMS);
			ref_c(map_b, PARAMS);

			if (arm == 1) {
				sysdep_free(slot_ptr(map_a, 0x018));
				sysdep_free(slot_ptr(map_b, 0x018));
				memset(map_a + 0x018, 0, sizeof(void *));
				memset(map_b + 0x018, 0, sizeof(void *));
				saw_null = 1;
			} else {
				saw_live = 1;
			}
			memcpy(before_a, map_a, MAPPER_SLOT);
			memcpy(before_b, map_b, MAPPER_SLOT);

			a_frees = harness_alloc.frees;
			a_null = harness_alloc.free_null;
			our_d(map_a);
			a_frees = harness_alloc.frees - a_frees;
			a_null = harness_alloc.free_null - a_null;

			b_frees = harness_alloc.frees;
			b_null = harness_alloc.free_null;
			ref_d(map_b);
			b_frees = harness_alloc.frees - b_frees;
			b_null = harness_alloc.free_null - b_null;

			diff_eq_int("frees match the blob (arm %ld)", a_frees,
				    b_frees, arm);
			diff_eq_int("sysdep_free(NULL) calls (arm %ld)", a_null,
				    0, arm);
			diff_eq_int("ref sysdep_free(NULL) calls (arm %ld)",
				    b_null, 0, arm);
			diff_eq_int("no bad free (arm %ld)",
				    harness_alloc.bad_free, 0, arm);
			diff_eq_int("nothing left allocated (arm %ld)",
				    harness_alloc.live, 0, arm);

			/*
			 * The destructor stores nothing of its own, on either
			 * side.  The shaper's span is excluded because that
			 * class's destructor is not this one's claim to make.
			 */
			diff_eq_int("stored nothing below the shaper (arm %ld)",
				    memcmp(before_a, map_a, SHAPER_LO) == 0, 1,
				    arm);
			diff_eq_int("stored nothing above the shaper (arm %ld)",
				    memcmp(before_a + SHAPER_HI,
					   map_a + SHAPER_HI,
					   MAPPER_SLOT - SHAPER_HI) == 0, 1,
				    arm);
			diff_eq_int("ref stored nothing below the shaper "
				    "(arm %ld)",
				    memcmp(before_b, map_b, SHAPER_LO) == 0, 1,
				    arm);
			diff_eq_int("ref stored nothing above the shaper "
				    "(arm %ld)",
				    memcmp(before_b + SHAPER_HI,
					   map_b + SHAPER_HI,
					   MAPPER_SLOT - SHAPER_HI) == 0, 1,
				    arm);

			if (arm == 0)
				base_frees = a_frees;
			else
				diff_eq_int("nulling buf removes exactly one "
					    "free (trial %ld)",
					    base_frees - a_frees, 1, trial);
		}
	}

	diff_eq_int("a live buffer was tried", saw_live, 1, 0);
	diff_eq_int("a null buffer was tried", saw_null, 1, 0);

	return diff_end();
}

/* ========================================================== V90BitsToSymbol */

/* Varied, and never zero: nofSymbols scales the second allocation. */
static const unsigned int bts_n[] = {
	1u, 2u, 3u, 7u, 0x10u, 0x40u, 0x140u, 0x200u
};

static int
run_bts_ctor(const char *name, bts_ctor our_c, bts_ctor ref_c, dtor our_d,
	     dtor ref_d)
{
	unsigned char first[BTS_SLOT];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int n = bts_n[trial & 7];
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes;
		void *am, *bm;

		seed_trial(trial);
		harness_alloc_reset();

		our_c(bts_a, n, PARAMS);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		ref_c(bts_b, n, PARAMS);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		live_refresh();

		diff_eq_int("allocations match the blob (trial %ld)", a_allocs,
			    b_allocs, trial);
		diff_eq_int("bytes allocated match the blob (trial %ld)",
			    (int)a_bytes, (int)b_bytes, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		diff_eq_int("live set did not overflow (trial %ld)",
			    harness_alloc.overflow, 0, trial);

		am = slot_ptr(bts_a, 0x00);
		bm = slot_ptr(bts_b, 0x00);
		diff_eq_int("mapper is not null (trial %ld)", am != 0, 1,
			    trial);
		diff_eq_int("ref mapper is not null (trial %ld)", bm != 0, 1,
			    trial);
		diff_eq_int("symbols is not null (trial %ld)",
			    slot_ptr(bts_a, 0x08) != 0, 1, trial);
		diff_eq_int("symbols is its own allocation (trial %ld)",
			    slot_ptr(bts_a, 0x08) != am, 1, trial);
		diff_eq_int("params is the argument (trial %ld)",
			    slot_ptr(bts_a, 0x04) == (void *)PARAMS, 1, trial);
		diff_eq_int("ref params is the argument (trial %ld)",
			    slot_ptr(bts_b, 0x04) == (void *)PARAMS, 1, trial);

		compare_bts("after the constructor", bts_a, bts_b, trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(bts_a, bts_b, bts_seed, BTS_SIZE,
					 BTS_SLOT), 1, trial);

		/*
		 * The mapper it owns, compared whole.  This is what says the
		 * mapper was built with THIS object's `params` and not with
		 * something else: the field is at a known offset inside a
		 * block neither side wrote directly.
		 */
		compare_mapper("the mapper the constructor built",
			       (const unsigned char *)am,
			       (const unsigned char *)bm, trial);
		diff_eq_int("the mapper holds the same params (trial %ld)",
			    slot_ptr((const unsigned char *)am, 0x000)
			    == (void *)PARAMS, 1, trial);

		if (memcmp(bts_seed, bts_a, BTS_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, bts_a, BTS_SLOT);
		else if (memcmp(first, bts_a, BTS_SLOT) != 0)
			distinct = 1;

		our_d(bts_a);
		ref_d(bts_b);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * Both guards, all four combinations.  The pointers are REAL -- the mapper is
 * dereferenced before it is freed, so a wild value is a crash and not a test
 * -- and the one nulled is released by hand first, so `live` still returns to
 * zero and a leak in the destructor is still visible.
 *
 * The expected free count is exact.  Subset 0 measures the whole chain; the
 * mapper's share of it is that total minus the one free of `symbols`, so
 * nulling the mapper must leave exactly one free and nulling `symbols` must
 * leave exactly the mapper's share.
 */
static int
run_bts_dtor(const char *name, bts_ctor our_c, bts_ctor ref_c, dtor our_d,
	     dtor ref_d, dtor our_md, dtor ref_md)
{
	unsigned char before_a[BTS_SLOT], before_b[BTS_SLOT];
	int trial, saw_null = 0, saw_live = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int n = bts_n[trial & 7];
		int base_frees = 0;
		unsigned subset;

		for (subset = 0; subset < 4; subset++) {
			int a_frees, b_frees, a_null, b_null, want;

			seed_trial(trial);
			harness_alloc_reset();
			our_c(bts_a, n, PARAMS);
			ref_c(bts_b, n, PARAMS);

			if (subset & 1u) {
				void *am = slot_ptr(bts_a, 0x00);
				void *bm = slot_ptr(bts_b, 0x00);

				our_md(am);
				ref_md(bm);
				sysdep_free(am);
				sysdep_free(bm);
				memset(bts_a + 0x00, 0, sizeof(void *));
				memset(bts_b + 0x00, 0, sizeof(void *));
				saw_null = 1;
			}
			if (subset & 2u) {
				sysdep_free(slot_ptr(bts_a, 0x08));
				sysdep_free(slot_ptr(bts_b, 0x08));
				memset(bts_a + 0x08, 0, sizeof(void *));
				memset(bts_b + 0x08, 0, sizeof(void *));
				saw_null = 1;
			}
			if (subset == 0)
				saw_live = 1;
			memcpy(before_a, bts_a, BTS_SLOT);
			memcpy(before_b, bts_b, BTS_SLOT);

			a_frees = harness_alloc.frees;
			a_null = harness_alloc.free_null;
			our_d(bts_a);
			a_frees = harness_alloc.frees - a_frees;
			a_null = harness_alloc.free_null - a_null;

			b_frees = harness_alloc.frees;
			b_null = harness_alloc.free_null;
			ref_d(bts_b);
			b_frees = harness_alloc.frees - b_frees;
			b_null = harness_alloc.free_null - b_null;

			diff_eq_int("frees match the blob, subset %ld",
				    a_frees, b_frees, (long)subset);
			diff_eq_int("sysdep_free(NULL), subset %ld", a_null, 0,
				    (long)subset);
			diff_eq_int("ref sysdep_free(NULL), subset %ld", b_null,
				    0, (long)subset);
			diff_eq_int("no bad free, subset %ld",
				    harness_alloc.bad_free, 0, (long)subset);
			diff_eq_int("nothing left allocated, subset %ld",
				    harness_alloc.live, 0, (long)subset);
			diff_eq_int("the destructor stored nothing, subset %ld",
				    memcmp(before_a, bts_a, BTS_SLOT) == 0, 1,
				    (long)subset);
			diff_eq_int("ref stored nothing, subset %ld",
				    memcmp(before_b, bts_b, BTS_SLOT) == 0, 1,
				    (long)subset);

			if (subset == 0) {
				base_frees = a_frees;
				continue;
			}
			/* base_frees - 1 is the mapper chain; 1 is `symbols`. */
			want = 0;
			if ((subset & 1u) == 0)
				want += base_frees - 1;
			if ((subset & 2u) == 0)
				want += 1;
			diff_eq_int("one free per live pointer, subset %ld",
				    a_frees, want, (long)subset);
		}
	}

	diff_eq_int("a null pointer was tried", saw_null, 1, 0);
	diff_eq_int("a live pointer was tried", saw_live, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_mapper_ctor("V90Mapper::V90Mapper (C1)", our_mapper_c1,
			      ref_mapper_c1, our_mapper_d1, ref_mapper_d1);
	rc |= run_mapper_ctor("V90Mapper::V90Mapper (C2)", our_mapper_c2,
			      ref_mapper_c2, our_mapper_d2, ref_mapper_d2);
	rc |= run_mapper_dtor("V90Mapper::~V90Mapper (D1)", our_mapper_c1,
			      ref_mapper_c1, our_mapper_d1, ref_mapper_d1);
	rc |= run_mapper_dtor("V90Mapper::~V90Mapper (D2)", our_mapper_c2,
			      ref_mapper_c2, our_mapper_d2, ref_mapper_d2);

	rc |= run_bts_ctor("V90BitsToSymbol::V90BitsToSymbol (C1)",
			   our_bts_c1, ref_bts_c1, our_bts_d1, ref_bts_d1);
	rc |= run_bts_ctor("V90BitsToSymbol::V90BitsToSymbol (C2)",
			   our_bts_c2, ref_bts_c2, our_bts_d2, ref_bts_d2);
	rc |= run_bts_dtor("V90BitsToSymbol::~V90BitsToSymbol (D1)",
			   our_bts_c1, ref_bts_c1, our_bts_d1, ref_bts_d1,
			   our_mapper_d1, ref_mapper_d1);
	rc |= run_bts_dtor("V90BitsToSymbol::~V90BitsToSymbol (D2)",
			   our_bts_c2, ref_bts_c2, our_bts_d2, ref_bts_d2,
			   our_mapper_d2, ref_mapper_d2);

	return rc;
}
