/*
 * t_v90cp.cpp -- differential test of the constructors and destructors of
 * V90CP, V92CP and V90MP, all four symbol variants of each.
 *
 * SIX FUNCTIONS THAT TAKE NO ARGUMENTS.  The only thing that varies between
 * trials is the seed, so the never-zeroed rule and the guard past the end of
 * the object carry the whole test, and both are done thoroughly here:
 *
 *   - Both sides are seeded with the SAME varied pseudorandom bytes before
 *     every call and are NEVER zeroed.  A zero-filled object would let a
 *     clear that missed a field pass -- the field it failed to clear was
 *     already zero -- and would make "did anything happen" unanswerable.
 *   - The slot is 64 bytes longer than the object.  Every trial compares that
 *     tail between the two sides AND against the seed, so a store one byte
 *     past the end fails, and so does a store 60 bytes past it.
 *   - Each run asserts the call changed the object, and that the object is
 *     not the same on every trial (findings F223, F224: a passing comparison of
 *     memory neither side wrote proves nothing).
 *
 * RAW STORAGE, NOT A UNION.  t_v90jd's `union { V90Jd o; unsigned char raw[]; }`
 * works only because V90Jd has no user-declared constructor; these three
 * classes have both a constructor and a destructor, which deletes the union's
 * own, and a plain `static V90CP obj;` would emit __cxa_atexit into a link
 * done with $(CC).  The object is reached through a cast instead -- which
 * costs nothing, because C++ has no syntax for running a constructor over
 * storage that already exists and BOTH sides are therefore called by symbol
 * through asm() labels.
 *
 * THE ALLOCATOR IS THE ORACLE FOR V90CP.  Its constructor makes six
 * sysdep_malloc(0x200) calls and its destructor six null-guarded
 * sysdep_free's; the six pointers are the one part of the object that CANNOT
 * agree between the sides, so they are poisoned to a constant in a copy
 * before comparing and checked separately for being non-null and distinct.
 * What the destructor did is then read off the harness's allocation log,
 * which counts a free of NULL apart from a free of a pointer it never handed
 * out and swallows the latter instead of passing it to free().  That is what
 * makes the null guard testable at all: the wild-pointer run below hands the
 * destructor six seeded garbage pointers and reads the count back, where a
 * real allocator would abort the run.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90CP.h"
#include "dsplib/V92CP.h"
#include "dsplib/V90MP.h"

extern "C" {
void our_v90cp_c1(void *) asm("_ZN5V90CPC1Ev");
void our_v90cp_c2(void *) asm("_ZN5V90CPC2Ev");
void our_v90cp_d1(void *) asm("_ZN5V90CPD1Ev");
void our_v90cp_d2(void *) asm("_ZN5V90CPD2Ev");
void ref_v90cp_c1(void *) asm("ref__ZN5V90CPC1Ev");
void ref_v90cp_c2(void *) asm("ref__ZN5V90CPC2Ev");
void ref_v90cp_d1(void *) asm("ref__ZN5V90CPD1Ev");
void ref_v90cp_d2(void *) asm("ref__ZN5V90CPD2Ev");

void our_v92cp_c1(void *) asm("_ZN5V92CPC1Ev");
void our_v92cp_c2(void *) asm("_ZN5V92CPC2Ev");
void our_v92cp_d1(void *) asm("_ZN5V92CPD1Ev");
void our_v92cp_d2(void *) asm("_ZN5V92CPD2Ev");
void ref_v92cp_c1(void *) asm("ref__ZN5V92CPC1Ev");
void ref_v92cp_c2(void *) asm("ref__ZN5V92CPC2Ev");
void ref_v92cp_d1(void *) asm("ref__ZN5V92CPD1Ev");
void ref_v92cp_d2(void *) asm("ref__ZN5V92CPD2Ev");

void our_v90mp_c1(void *) asm("_ZN5V90MPC1Ev");
void our_v90mp_c2(void *) asm("_ZN5V90MPC2Ev");
void our_v90mp_d1(void *) asm("_ZN5V90MPD1Ev");
void our_v90mp_d2(void *) asm("_ZN5V90MPD2Ev");
void ref_v90mp_c1(void *) asm("ref__ZN5V90MPC1Ev");
void ref_v90mp_c2(void *) asm("ref__ZN5V90MPC2Ev");
void ref_v90mp_d1(void *) asm("ref__ZN5V90MPD1Ev");
void ref_v90mp_d2(void *) asm("ref__ZN5V90MPD2Ev");
}

typedef void (*member)(void *);

/* Room past the object, wide enough that a store a few words over is caught. */
#define GUARD	64

#define SLOT(cls)	(sizeof(cls) + GUARD)
#define BIGGEST		SLOT(V90CP)

static unsigned char ours[BIGGEST] __attribute__((aligned(8)));
static unsigned char theirs[BIGGEST] __attribute__((aligned(8)));
static unsigned char sown[BIGGEST];		/* the seed, for the guard   */
static unsigned char cmp_a[BIGGEST];		/* poisoned copies, so that  */
static unsigned char cmp_b[BIGGEST];		/* the heap pointers can be  */
						/* skipped without a memcmp  */

/*
 * Seeds.  `mode` varies how the bytes are chosen, because a constructor that
 * stores a constant is invisible against a seed that already holds it: mode 1
 * seeds 0x12 everywhere, which is the value V90CP writes to +0xcac, and mode
 * 2 seeds 0xff, which is the -1 the three of them write.  Neither is zero.
 */
static void
seed(int trial, int mode, unsigned slot)
{
	unsigned lfsr = 0x1234u + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < slot; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		switch (mode) {
		case 0:
			v = (unsigned char)(lfsr >> 3);
			break;
		case 1:
			v = 0x12;	/* what V90CP puts at +0xcac */
			break;
		case 2:
			v = 0xff;	/* the -1 at +0x3bbc         */
			break;
		default:
			v = (unsigned char)((lfsr >> 5) | 1u);
			break;
		}
		ours[i] = v;
		theirs[i] = v;
		sown[i] = v;
	}
}

/* The guard, on BOTH sides, against what the seed left there. */
static int
guard_intact(unsigned size, unsigned slot)
{
	return memcmp(ours + size, sown + size, slot - size) == 0
	    && memcmp(theirs + size, sown + size, slot - size) == 0;
}

/*
 * Compare the objects whole, with the pointer region -- which holds two
 * different heap addresses and always will -- replaced by a constant in a
 * copy of each.  `nptr` is 0 for the classes that allocate nothing, and then
 * this is a straight whole-object comparison.
 */
static void
compare(const char *what, const char *type, unsigned size,
	unsigned ptr_off, unsigned nptr, long trial)
{
	memcpy(cmp_a, ours, size);
	memcpy(cmp_b, theirs, size);
	if (nptr != 0) {
		memset(cmp_a + ptr_off, 0x77, nptr * sizeof(void *));
		memset(cmp_b + ptr_off, 0x77, nptr * sizeof(void *));
	}
	diff_eq_obj_(__FILE__, __LINE__, what, type, cmp_a, cmp_b,
		     (size_t)size, trial);
}

/* Read a pointer out of a slot without punning through an aligned type. */
static void *
slot_ptr(const unsigned char *o, unsigned ptr_off, unsigned i)
{
	void *p;

	memcpy(&p, o + ptr_off + i * sizeof(void *), sizeof(p));
	return p;
}

static void
slot_set_ptr(unsigned char *o, unsigned ptr_off, unsigned i, void *p)
{
	memcpy(o + ptr_off + i * sizeof(void *), &p, sizeof(p));
}

#define NTRIAL	24

/*
 * The constructor: both sides over the same seeded storage, then the whole
 * object, the allocation counts, and the guard.
 */
static int
run_ctor(const char *name, const char *type, unsigned size,
	 member our_ctor, member ref_ctor, member our_dtor, member ref_dtor,
	 unsigned ptr_off, unsigned nptr, unsigned bufsize)
{
	unsigned slot = size + GUARD;
	unsigned char first[BIGGEST];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes;
		unsigned i, j;

		seed(trial, trial % 4, slot);
		harness_alloc_reset();

		our_ctor(ours);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		ref_ctor(theirs);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		/*
		 * Absolute, not merely equal: the disassembly says six calls
		 * of 0x200 for V90CP and none at all for the other two, so a
		 * reconstruction that allocated the right total in the wrong
		 * number of pieces is a failure here.
		 */
		diff_eq_int("allocations (trial %ld)", a_allocs, (int)nptr,
			    trial);
		diff_eq_int("ref allocations (trial %ld)", b_allocs, (int)nptr,
			    trial);
		diff_eq_int("bytes allocated (trial %ld)", a_bytes,
			    nptr * bufsize, trial);
		diff_eq_int("ref bytes allocated (trial %ld)", b_bytes,
			    nptr * bufsize, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);

		/* Every pointer stored, non-null, and distinct from the rest. */
		for (i = 0; i < nptr; i++) {
			diff_eq_int("buf[%ld] is not null",
				    slot_ptr(ours, ptr_off, i) != 0, 1, i);
			diff_eq_int("ref buf[%ld] is not null",
				    slot_ptr(theirs, ptr_off, i) != 0, 1, i);
			for (j = 0; j < i; j++)
				diff_eq_int("buf[%ld] is its own allocation",
					    slot_ptr(ours, ptr_off, i)
					    != slot_ptr(ours, ptr_off, j), 1, i);
		}

		compare("after the constructor", type, size, ptr_off, nptr,
			trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(size, slot), 1, trial);

		/* Anti-vacuity, over the whole slot rather than one field. */
		if (memcmp(sown, ours, slot) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, ours, slot);
		else if (memcmp(first, ours, slot) != 0)
			distinct = 1;

		/* Give the six buffers back before the next trial reseeds. */
		our_dtor(ours);
		ref_dtor(theirs);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * The destructor over what the constructor built: the pointers are live, so
 * this is the path that really frees.
 */
static int
run_dtor_live(const char *name, const char *type, unsigned size,
	      member our_ctor, member ref_ctor, member our_dtor,
	      member ref_dtor, unsigned ptr_off, unsigned nptr)
{
	unsigned slot = size + GUARD;
	unsigned char before[BIGGEST];
	int trial, freed = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int a_frees, b_frees, a_null, b_null;

		seed(trial, trial % 4, slot);
		harness_alloc_reset();
		our_ctor(ours);
		ref_ctor(theirs);
		memcpy(before, ours, slot);

		a_frees = harness_alloc.frees;
		a_null = harness_alloc.free_null;
		our_dtor(ours);
		a_frees = harness_alloc.frees - a_frees;
		a_null = harness_alloc.free_null - a_null;

		b_frees = harness_alloc.frees;
		b_null = harness_alloc.free_null;
		ref_dtor(theirs);
		b_frees = harness_alloc.frees - b_frees;
		b_null = harness_alloc.free_null - b_null;

		diff_eq_int("frees (trial %ld)", a_frees, (int)nptr, trial);
		diff_eq_int("ref frees (trial %ld)", b_frees, (int)nptr, trial);
		diff_eq_int("sysdep_free(NULL) calls (trial %ld)", a_null, 0,
			    trial);
		diff_eq_int("ref sysdep_free(NULL) calls (trial %ld)", b_null,
			    0, trial);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);

		/*
		 * The destructor stores nothing -- not even a null over the
		 * pointer it just released.  Comparing our side against
		 * ITSELF before the call is what says so; the two sides being
		 * equal would not.
		 */
		diff_eq_int("the destructor stored nothing (trial %ld)",
			    memcmp(before, ours, slot) == 0, 1, trial);

		compare("after the destructor", type, size, ptr_off, nptr,
			trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(size, slot), 1, trial);

		if (a_frees > 0)
			freed = 1;
	}

	/* For the two classes that allocate nothing there is nothing to free,
	 * and `nptr == 0` is itself the claim being made. */
	diff_eq_int("the destructor released the buffers", freed, nptr != 0,
		    0);

	return diff_end();
}

/*
 * The destructor over pointers it did not allocate: all 64 null/non-null
 * combinations of the six, the same on both sides.  The seeded values are
 * wild, and the harness counts a free of one instead of performing it, so
 * both branches of all six guards are exercised without a crash.
 */
static int
run_dtor_wild(const char *name, const char *type, unsigned size,
	      member our_dtor, member ref_dtor, unsigned ptr_off, unsigned nptr)
{
	unsigned slot = size + GUARD;
	unsigned char before[BIGGEST];
	/*
	 * Every null/non-null combination where there are pointers to null,
	 * and otherwise NTRIAL seeds -- because for a destructor that should
	 * store nothing, the seed IS the test: run it over a constructed
	 * object and a store of 0 into a field the constructor already zeroed
	 * is invisible.
	 */
	unsigned subset, nsubset = nptr != 0 ? (1u << nptr) : NTRIAL;
	int saw_null = 0, saw_live = 0;

	diff_begin(name);

	for (subset = 0; subset < nsubset; subset++) {
		int a_bad, b_bad, a_null, b_null, want;
		unsigned i;

		seed((int)subset, (int)(subset % 4), slot);
		harness_alloc_reset();

		want = 0;
		for (i = 0; i < nptr; i++) {
			if ((subset >> i) & 1u) {
				slot_set_ptr(ours, ptr_off, i, 0);
				slot_set_ptr(theirs, ptr_off, i, 0);
				saw_null = 1;
			} else {
				want++;
				saw_live = 1;
			}
		}
		memcpy(before, ours, slot);

		a_bad = harness_alloc.bad_free;
		a_null = harness_alloc.free_null;
		our_dtor(ours);
		a_bad = harness_alloc.bad_free - a_bad;
		a_null = harness_alloc.free_null - a_null;

		b_bad = harness_alloc.bad_free;
		b_null = harness_alloc.free_null;
		ref_dtor(theirs);
		b_bad = harness_alloc.bad_free - b_bad;
		b_null = harness_alloc.free_null - b_null;

		/*
		 * One free attempt per non-null pointer and NONE for a null
		 * one: an unguarded `sysdep_free(buf[i])` would show up as
		 * free_null instead, which is the whole point of counting the
		 * two apart.
		 */
		diff_eq_int("free attempts, subset 0x%02lx", a_bad, want,
			    (long)subset);
		diff_eq_int("ref free attempts, subset 0x%02lx", b_bad, want,
			    (long)subset);
		diff_eq_int("sysdep_free(NULL), subset 0x%02lx", a_null, 0,
			    (long)subset);
		diff_eq_int("ref sysdep_free(NULL), subset 0x%02lx", b_null, 0,
			    (long)subset);

		diff_eq_int("the destructor stored nothing, subset 0x%02lx",
			    memcmp(before, ours, slot) == 0, 1, (long)subset);
		/* Both sides hold the SAME wild pointers here, so nothing has
		 * to be skipped: compare the object whole. */
		compare("after the destructor", type, size, 0, 0, (long)subset);
		diff_eq_int("nothing stored past the object, subset 0x%02lx",
			    guard_intact(size, slot), 1, (long)subset);
	}

	diff_eq_int("a null pointer was tried", saw_null, nptr != 0, 0);
	diff_eq_int("a non-null pointer was tried", saw_live, nptr != 0, 0);

	return diff_end();
}

int
main(void)
{
	unsigned cp_ptr = (unsigned)__builtin_offsetof(V90CP, buf);
	int rc = 0;

	/* C1 and C2, D1 and D2: four symbols per class, all four aliased to
	 * the same body in the blob and all four given a ref_ name. */
	rc |= run_ctor("V90CP::V90CP (C1)", "V90CP", sizeof(V90CP),
		       our_v90cp_c1, ref_v90cp_c1, our_v90cp_d1, ref_v90cp_d1,
		       cp_ptr, V90CP_BUFS, V90CP_BUFSIZE);
	rc |= run_ctor("V90CP::V90CP (C2)", "V90CP", sizeof(V90CP),
		       our_v90cp_c2, ref_v90cp_c2, our_v90cp_d2, ref_v90cp_d2,
		       cp_ptr, V90CP_BUFS, V90CP_BUFSIZE);
	rc |= run_dtor_live("V90CP::~V90CP (D1)", "V90CP", sizeof(V90CP),
			    our_v90cp_c1, ref_v90cp_c1, our_v90cp_d1,
			    ref_v90cp_d1, cp_ptr, V90CP_BUFS);
	rc |= run_dtor_live("V90CP::~V90CP (D2)", "V90CP", sizeof(V90CP),
			    our_v90cp_c2, ref_v90cp_c2, our_v90cp_d2,
			    ref_v90cp_d2, cp_ptr, V90CP_BUFS);
	rc |= run_dtor_wild("V90CP::~V90CP over wild pointers (D1)", "V90CP",
			    sizeof(V90CP), our_v90cp_d1, ref_v90cp_d1, cp_ptr,
			    V90CP_BUFS);
	rc |= run_dtor_wild("V90CP::~V90CP over wild pointers (D2)", "V90CP",
			    sizeof(V90CP), our_v90cp_d2, ref_v90cp_d2, cp_ptr,
			    V90CP_BUFS);

	rc |= run_ctor("V92CP::V92CP (C1)", "V92CP", sizeof(V92CP),
		       our_v92cp_c1, ref_v92cp_c1, our_v92cp_d1, ref_v92cp_d1,
		       0, 0, 0);
	rc |= run_ctor("V92CP::V92CP (C2)", "V92CP", sizeof(V92CP),
		       our_v92cp_c2, ref_v92cp_c2, our_v92cp_d2, ref_v92cp_d2,
		       0, 0, 0);
	rc |= run_dtor_live("V92CP::~V92CP (D1)", "V92CP", sizeof(V92CP),
			    our_v92cp_c1, ref_v92cp_c1, our_v92cp_d1,
			    ref_v92cp_d1, 0, 0);
	rc |= run_dtor_live("V92CP::~V92CP (D2)", "V92CP", sizeof(V92CP),
			    our_v92cp_c2, ref_v92cp_c2, our_v92cp_d2,
			    ref_v92cp_d2, 0, 0);
	rc |= run_dtor_wild("V92CP::~V92CP over a seeded object (D1)", "V92CP",
			    sizeof(V92CP), our_v92cp_d1, ref_v92cp_d1, 0, 0);
	rc |= run_dtor_wild("V92CP::~V92CP over a seeded object (D2)", "V92CP",
			    sizeof(V92CP), our_v92cp_d2, ref_v92cp_d2, 0, 0);

	rc |= run_ctor("V90MP::V90MP (C1)", "V90MP", sizeof(V90MP),
		       our_v90mp_c1, ref_v90mp_c1, our_v90mp_d1, ref_v90mp_d1,
		       0, 0, 0);
	rc |= run_ctor("V90MP::V90MP (C2)", "V90MP", sizeof(V90MP),
		       our_v90mp_c2, ref_v90mp_c2, our_v90mp_d2, ref_v90mp_d2,
		       0, 0, 0);
	rc |= run_dtor_live("V90MP::~V90MP (D1)", "V90MP", sizeof(V90MP),
			    our_v90mp_c1, ref_v90mp_c1, our_v90mp_d1,
			    ref_v90mp_d1, 0, 0);
	rc |= run_dtor_live("V90MP::~V90MP (D2)", "V90MP", sizeof(V90MP),
			    our_v90mp_c2, ref_v90mp_c2, our_v90mp_d2,
			    ref_v90mp_d2, 0, 0);
	rc |= run_dtor_wild("V90MP::~V90MP over a seeded object (D1)", "V90MP",
			    sizeof(V90MP), our_v90mp_d1, ref_v90mp_d1, 0, 0);
	rc |= run_dtor_wild("V90MP::~V90MP over a seeded object (D2)", "V90MP",
			    sizeof(V90MP), our_v90mp_d2, ref_v90mp_d2, 0, 0);

	return rc;
}
