/*
 * t_v92tx.cpp -- differential test of V92Transmitter's and V92BitsToSymbol's
 * constructors and destructors, all four symbol variants of each.
 *
 * EIGHT SYMBOLS.  C1 and C2, D1 and D2 for both classes: GCC gives us one
 * function under two names where the blob has two copies, so driving one
 * leaves the other's symbol asserted by nothing.
 *
 * THE RULES THIS FIXTURE ENCODES, and why each is here:
 *
 *   - Both sides are seeded with the SAME varied pseudorandom bytes before
 *     every call and are NEVER zeroed.  A zero-filled object would let a
 *     store of 0 that never happened pass, and would make "did anything
 *     happen at all" unanswerable (findings 223, 224).
 *   - The slot is 64 bytes longer than the object and the tail is compared on
 *     both sides against the seed, so a store one byte past the end fails.
 *   - Every call is by SYMBOL through an asm() label.  C++ has no syntax for
 *     running a constructor over storage that already exists, and
 *     `OBJ = V92Transmitter();` would build a temporary over uninitialised
 *     stack and copy it, destroying the property the whole fixture rests on.
 *   - `V92BitsToSymbol`'s `V92Parameters *` argument is ONE shared, per-trial
 *     seeded block pointed at by both sides.  Two separately seeded blocks
 *     would agree whatever was read from them.
 *
 * WHAT WITNESSES THE ALLOCATION SIZES.  The six pointers a constructed
 * V92Transmitter holds are two different heap addresses per pair and always
 * will be, so they are poisoned before the objects are compared -- which
 * would hide `sysdep_malloc(0x50)` written as anything else, and
 * `n * sizeof(short)` written with any other multiplier.  What catches those
 * is `harness_alloc.bytes` and `.allocs` compared BETWEEN THE SIDES: the blob
 * is the oracle for the total and for the number of pieces it comes in, and
 * a wrong size or a missing allocation moves one of them.  The literals
 * themselves -- 0x60, 0x54, 0x2008, 0x80, 0x14, 0x20 -- are pinned a second
 * time as compile-time `sizeof` assertions in the two .cpp files.
 *
 * THE DESTRUCTORS' NULL GUARDS ARE DRIVEN OVER EVERY COMBINATION.  This
 * tree's `sysdep_free` tolerates NULL, so dropping a guard leaves every byte
 * of the object unchanged and moves only `harness_alloc.free_null` -- so the
 * subset runs below null out each subset of the owned pointers and read that
 * counter back, all 64 combinations for the transmitter and all 4 for the
 * bit-to-symbol stage.  The pointers that are nulled are NOT wild: three of
 * the transmitter's six get a destructor call that dereferences them, so the
 * subset run constructs a real object, nulls a subset, destroys it, and then
 * destroys the complement to give the rest back.  `harness_alloc.live == 0`
 * after the pair is what says the two halves covered the six exactly once.
 *
 * ONE DESTRUCTOR DOES STORE.  `~V92Transmitter` writes 0 over +0x4c after
 * releasing the precoder and nulls none of the other five, so the "the
 * destructor stored nothing" assertion the sibling fixtures make is FALSE
 * here and is parameterised rather than dropped -- with -fno-lifetime-dse in
 * CXXFLAGS that store survives, and it is the object's.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/V92BitsToSymbol.h"

extern "C" {
void our_tx_c1(void *) asm("_ZN14V92TransmitterC1Ev");
void our_tx_c2(void *) asm("_ZN14V92TransmitterC2Ev");
void our_tx_d1(void *) asm("_ZN14V92TransmitterD1Ev");
void our_tx_d2(void *) asm("_ZN14V92TransmitterD2Ev");
void ref_tx_c1(void *) asm("ref__ZN14V92TransmitterC1Ev");
void ref_tx_c2(void *) asm("ref__ZN14V92TransmitterC2Ev");
void ref_tx_d1(void *) asm("ref__ZN14V92TransmitterD1Ev");
void ref_tx_d2(void *) asm("ref__ZN14V92TransmitterD2Ev");

void our_bs_c1(void *, unsigned, void *)
	asm("_ZN15V92BitsToSymbolC1EjP13V92Parameters");
void our_bs_c2(void *, unsigned, void *)
	asm("_ZN15V92BitsToSymbolC2EjP13V92Parameters");
void our_bs_d1(void *) asm("_ZN15V92BitsToSymbolD1Ev");
void our_bs_d2(void *) asm("_ZN15V92BitsToSymbolD2Ev");
void ref_bs_c1(void *, unsigned, void *)
	asm("ref__ZN15V92BitsToSymbolC1EjP13V92Parameters");
void ref_bs_c2(void *, unsigned, void *)
	asm("ref__ZN15V92BitsToSymbolC2EjP13V92Parameters");
void ref_bs_d1(void *) asm("ref__ZN15V92BitsToSymbolD1Ev");
void ref_bs_d2(void *) asm("ref__ZN15V92BitsToSymbolD2Ev");
}

typedef void (*dtor_fn)(void *);

#define GUARD		64
#define BIGGEST		(sizeof(V92Transmitter) + GUARD)
#define MAXPTR		6

static unsigned char ours[BIGGEST] __attribute__((aligned(8)));
static unsigned char theirs[BIGGEST] __attribute__((aligned(8)));
static unsigned char sown[BIGGEST];
static unsigned char cmp_a[BIGGEST];
static unsigned char cmp_b[BIGGEST];
static unsigned char ctor_a[BIGGEST];
static unsigned char ctor_b[BIGGEST];

/* The shared V92Parameters both sides are pointed at.  Only its ADDRESS is
 * read by anything here; it is seeded so that a constructor reading through
 * it would find varied bytes rather than zeroes. */
static unsigned char shared_params[256] __attribute__((aligned(8)));

/* Where the owned pointers live, per class.  The transmitter's six are not
 * contiguous, which is why this is a list and not an offset and a count. */
static const unsigned tx_ptr[MAXPTR] = { 0x08, 0x48, 0x4c, 0x50, 0x54, 0x58 };
static const unsigned bs_ptr[2] = { 0x00, 0x08 };

static void
seed(int trial, unsigned slot)
{
	unsigned lfsr = 0x1234u + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < slot; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		switch (trial & 3) {
		case 0:
			v = (unsigned char)(lfsr >> 3);
			break;
		case 1:
			v = 0x01;	/* the 1 the bit-to-symbol writes */
			break;
		case 2:
			v = 0xff;
			break;
		default:
			v = (unsigned char)((lfsr >> 5) | 1u);
			break;
		}
		ours[i] = v;
		theirs[i] = v;
		sown[i] = v;
	}
	for (i = 0; i < sizeof(shared_params); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		shared_params[i] = (unsigned char)(lfsr >> 3);
	}
}

static int
guard_intact(unsigned size, unsigned slot)
{
	return memcmp(ours + size, sown + size, slot - size) == 0
	    && memcmp(theirs + size, sown + size, slot - size) == 0;
}

static void *
slot_ptr(const unsigned char *o, unsigned off)
{
	void *p;

	memcpy(&p, o + off, sizeof(p));
	return p;
}

static void
slot_set_ptr(unsigned char *o, unsigned off, void *p)
{
	memcpy(o + off, &p, sizeof(p));
}

/*
 * The two objects, whole, with the owned pointers replaced by a constant in
 * a copy of each.  Everything else -- including the shared `params` pointer,
 * which really is the same address on both sides -- is compared byte for
 * byte.
 */
static void
compare(const char *what, const char *type, unsigned size,
	const unsigned *ptr, unsigned nptr, long trial)
{
	unsigned i;

	memcpy(cmp_a, ours, size);
	memcpy(cmp_b, theirs, size);
	for (i = 0; i < nptr; i++) {
		memset(cmp_a + ptr[i], 0x77, sizeof(void *));
		memset(cmp_b + ptr[i], 0x77, sizeof(void *));
	}
	diff_eq_obj_(__FILE__, __LINE__, what, type, cmp_a, cmp_b,
		     (size_t)size, trial);
}

#define NTRIAL	24

/*
 * Construct on both sides over the same seed, then compare the object, the
 * allocation totals, the pointers and the guard.
 */
static int
run_ctor(const char *name, const char *type, unsigned size, int which,
	 int variant, const unsigned *ptr, unsigned nptr)
{
	unsigned slot = size + GUARD;
	unsigned char first[BIGGEST];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes, n = (unsigned)(trial * 3 + 1);
		unsigned i, j;

		seed(trial, slot);
		harness_alloc_reset();

		if (which == 0) {
			if (variant == 1)
				our_tx_c1(ours);
			else
				our_tx_c2(ours);
		} else {
			if (variant == 1)
				our_bs_c1(ours, n, shared_params);
			else
				our_bs_c2(ours, n, shared_params);
		}
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		if (which == 0) {
			if (variant == 1)
				ref_tx_c1(theirs);
			else
				ref_tx_c2(theirs);
		} else {
			if (variant == 1)
				ref_bs_c1(theirs, n, shared_params);
			else
				ref_bs_c2(theirs, n, shared_params);
		}
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		/*
		 * The reference is the oracle for BOTH numbers.  The pieces
		 * catch a missing or an extra allocation; the total catches
		 * every size expression, which the poisoned pointers hide.
		 */
		diff_eq_int("allocations (trial %ld)", a_allocs, b_allocs,
			    trial);
		diff_eq_int("bytes allocated (trial %ld)", (int)a_bytes,
			    (int)b_bytes, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		diff_eq_int("no free of NULL (trial %ld)",
			    harness_alloc.free_null, 0, trial);

		for (i = 0; i < nptr; i++) {
			diff_eq_int("pointer at +0x%02lx is not null",
				    slot_ptr(ours, ptr[i]) != 0, 1, ptr[i]);
			diff_eq_int("ref pointer at +0x%02lx is not null",
				    slot_ptr(theirs, ptr[i]) != 0, 1, ptr[i]);
			for (j = 0; j < i; j++)
				diff_eq_int("pointer at +0x%02lx is its own "
					    "allocation",
					    slot_ptr(ours, ptr[i])
					    != slot_ptr(ours, ptr[j]), 1,
					    ptr[i]);
		}

		/*
		 * The one-byte buffer at +0x58 is cleared THROUGH the
		 * returned pointer before the pointer is stored, and its
		 * contents are behind a pointer the comparison has to poison
		 * -- so it is asserted here, absolutely, on both sides.
		 */
		if (which == 0) {
			const unsigned char *b;

			b = (const unsigned char *)slot_ptr(ours, 0x58);
			diff_eq_int("the byte at +0x58 is cleared (trial %ld)",
				    b != 0 && *b == 0, 1, trial);
			b = (const unsigned char *)slot_ptr(theirs, 0x58);
			diff_eq_int("ref byte at +0x58 is cleared (trial %ld)",
				    b != 0 && *b == 0, 1, trial);
		}

		compare("after the constructor", type, size, ptr, nptr, trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(size, slot), 1, trial);

		if (memcmp(sown, ours, slot) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, ours, slot);
		else if (memcmp(first, ours, slot) != 0)
			distinct = 1;

		/* Give it all back before the next trial reseeds. */
		if (which == 0) {
			our_tx_d1(ours);
			ref_tx_d1(theirs);
		} else {
			our_bs_d1(ours);
			ref_bs_d1(theirs);
		}
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * The destructor over a subset of its pointers nulled, every combination.
 * The complement is destroyed afterwards so nothing leaks and so that
 * `live == 0` says the two halves between them covered every allocation
 * exactly once.
 *
 * `stores_at` is the one offset ~V92Transmitter writes (its +0x4c) or ~0u for
 * a destructor that writes nothing; that difference is the object's and is
 * asserted rather than tolerated.
 */
static int
run_dtor_subsets(const char *name, const char *type, unsigned size, int which,
		 dtor_fn our_dtor, dtor_fn ref_dtor, const unsigned *ptr,
		 unsigned nptr, unsigned stores_at, unsigned stores_idx)
{
	unsigned slot = size + GUARD;
	unsigned char before[BIGGEST];
	unsigned subset, nsubset = 1u << nptr;
	int saw_null = 0, saw_live = 0, freed = 0;

	diff_begin(name);

	for (subset = 0; subset < nsubset; subset++) {
		int a_frees, b_frees, a_null, b_null, a_bad, b_bad;
		unsigned i, n = (unsigned)(subset * 5u + 3u);

		seed((int)subset, slot);
		harness_alloc_reset();

		if (which == 0) {
			our_tx_c1(ours);
			ref_tx_c1(theirs);
		} else {
			our_bs_c1(ours, n, shared_params);
			ref_bs_c1(theirs, n, shared_params);
		}
		memcpy(ctor_a, ours, slot);
		memcpy(ctor_b, theirs, slot);

		for (i = 0; i < nptr; i++) {
			if ((subset >> i) & 1u) {
				slot_set_ptr(ours, ptr[i], 0);
				slot_set_ptr(theirs, ptr[i], 0);
				saw_null = 1;
			} else {
				saw_live = 1;
			}
		}
		memcpy(before, ours, slot);

		a_frees = harness_alloc.frees;
		a_null = harness_alloc.free_null;
		a_bad = harness_alloc.bad_free;
		our_dtor(ours);
		a_frees = harness_alloc.frees - a_frees;
		a_null = harness_alloc.free_null - a_null;
		a_bad = harness_alloc.bad_free - a_bad;

		b_frees = harness_alloc.frees;
		b_null = harness_alloc.free_null;
		b_bad = harness_alloc.bad_free;
		ref_dtor(theirs);
		b_frees = harness_alloc.frees - b_frees;
		b_null = harness_alloc.free_null - b_null;
		b_bad = harness_alloc.bad_free - b_bad;

		diff_eq_int("frees, subset 0x%02lx", a_frees, b_frees,
			    (long)subset);
		/*
		 * The whole point of counting these apart: an unguarded
		 * `sysdep_free(p)` over a null p shows up here and NOWHERE
		 * else, because the object is unchanged either way.
		 */
		diff_eq_int("sysdep_free(NULL), subset 0x%02lx", a_null, 0,
			    (long)subset);
		diff_eq_int("ref sysdep_free(NULL), subset 0x%02lx", b_null, 0,
			    (long)subset);
		diff_eq_int("no bad free, subset 0x%02lx", a_bad, 0,
			    (long)subset);
		diff_eq_int("ref no bad free, subset 0x%02lx", b_bad, 0,
			    (long)subset);

		/*
		 * What the destructor stored, compared against OUR OWN state
		 * before the call -- the two sides being equal would not say
		 * this, because both would have stored it.
		 */
		if (stores_at == ~0u || ((subset >> stores_idx) & 1u) != 0) {
			/*
			 * Either the destructor writes nothing at all, or the
			 * one pointer it nulls was already null and its whole
			 * guarded block -- store included -- was skipped.
			 */
			diff_eq_int("the destructor stored nothing, "
				    "subset 0x%02lx",
				    memcmp(before, ours, slot) == 0, 1,
				    (long)subset);
		} else {
			memcpy(cmp_a, before, slot);
			memset(cmp_a + stores_at, 0x77, sizeof(void *));
			memcpy(cmp_b, ours, slot);
			memset(cmp_b + stores_at, 0x77, sizeof(void *));
			diff_eq_int("the destructor stored nothing but "
				    "+0x%02lx",
				    memcmp(cmp_a, cmp_b, slot) == 0, 1,
				    (long)stores_at);
			/* ...and what it stored there is NULL. */
			diff_eq_int("the freed pointer was nulled, "
				    "subset 0x%02lx",
				    slot_ptr(ours, stores_at) == 0, 1,
				    (long)subset);
		}

		compare("after the destructor", type, size, ptr, nptr,
			(long)subset);
		diff_eq_int("nothing stored past the object, subset 0x%02lx",
			    guard_intact(size, slot), 1, (long)subset);

		if (a_frees > 0)
			freed = 1;

		/*
		 * Give the complement back: restore what the constructor
		 * built, null the pointers the call above already released,
		 * and destroy again.
		 */
		memcpy(ours, ctor_a, slot);
		memcpy(theirs, ctor_b, slot);
		for (i = 0; i < nptr; i++) {
			if (((subset >> i) & 1u) == 0) {
				slot_set_ptr(ours, ptr[i], 0);
				slot_set_ptr(theirs, ptr[i], 0);
			}
		}
		our_dtor(ours);
		ref_dtor(theirs);
		diff_eq_int("the two halves freed everything, subset 0x%02lx",
			    harness_alloc.live, 0, (long)subset);
		diff_eq_int("and freed nothing twice, subset 0x%02lx",
			    harness_alloc.bad_free, 0, (long)subset);
	}

	diff_eq_int("a null pointer was tried", saw_null, 1, 0);
	diff_eq_int("a non-null pointer was tried", saw_live, 1, 0);
	diff_eq_int("the destructor released something", freed, 1, 0);

	return diff_end();
}

int
main(void)
{
	unsigned txsz = (unsigned)sizeof(V92Transmitter);
	unsigned bssz = (unsigned)sizeof(V92BitsToSymbol);
	int rc = 0;

	rc |= run_ctor("V92Transmitter::V92Transmitter (C1)", "V92Transmitter",
		       txsz, 0, 1, tx_ptr, MAXPTR);
	rc |= run_ctor("V92Transmitter::V92Transmitter (C2)", "V92Transmitter",
		       txsz, 0, 2, tx_ptr, MAXPTR);
	rc |= run_dtor_subsets("V92Transmitter::~V92Transmitter, all 64 null "
			       "combinations (D1)", "V92Transmitter", txsz, 0,
			       our_tx_d1, ref_tx_d1, tx_ptr, MAXPTR, 0x4c, 2);
	rc |= run_dtor_subsets("V92Transmitter::~V92Transmitter, all 64 null "
			       "combinations (D2)", "V92Transmitter", txsz, 0,
			       our_tx_d2, ref_tx_d2, tx_ptr, MAXPTR, 0x4c, 2);

	rc |= run_ctor("V92BitsToSymbol::V92BitsToSymbol (C1)",
		       "V92BitsToSymbol", bssz, 1, 1, bs_ptr, 2);
	rc |= run_ctor("V92BitsToSymbol::V92BitsToSymbol (C2)",
		       "V92BitsToSymbol", bssz, 1, 2, bs_ptr, 2);
	rc |= run_dtor_subsets("V92BitsToSymbol::~V92BitsToSymbol, all four "
			       "null combinations (D1)", "V92BitsToSymbol",
			       bssz, 1, our_bs_d1, ref_bs_d1, bs_ptr, 2, ~0u,
			       0);
	rc |= run_dtor_subsets("V92BitsToSymbol::~V92BitsToSymbol, all four "
			       "null combinations (D2)", "V92BitsToSymbol",
			       bssz, 1, our_bs_d2, ref_bs_d2, bs_ptr, 2, ~0u,
			       0);

	return rc;
}
