/*
 * t_v92p4mod.cpp -- differential test of V92Phase4Modulator's constructor and
 * destructor, all four symbol variants.
 *
 * C1 and C2, D1 and D2: GCC gives us one function under two names where the
 * blob has two copies, so driving one leaves the other's symbol asserted by
 * nothing.
 *
 * THE FIXTURE'S RULES, and what each one is for here:
 *
 *   - Both sides are seeded with the SAME varied pseudorandom bytes before
 *     every call and are NEVER zeroed.  Four of this constructor's ten
 *     stores are zeroes; against a zero-filled object none of them would be
 *     visible (findings F223, F224).
 *   - The slot is 64 bytes longer than the object, and the tail is compared
 *     on both sides against the seed.  The last field is at +0x1c8 and the
 *     object is 0x1cc, so a store four bytes past the end is exactly the
 *     mistake this catches.
 *   - Every call is by SYMBOL through an asm() label; `OBJ =
 *     V92Phase4Modulator(...)` would build a temporary over uninitialised
 *     stack and copy it.
 *   - THE FOUR ARGUMENTS ARE FOUR DIFFERENT SHARED OBJECTS, one instance
 *     each, pointed at by both sides.  Two separately seeded blocks per
 *     argument would agree whatever was read, so a constructor that stored
 *     argument 3 where argument 4 belongs would be invisible; sharing them
 *     makes each argument's ADDRESS the witness for which field it lands in.
 *
 * AND ONE OF THE ARGUMENTS IS WRITTEN, WHICH NEEDS THE OPPOSITE TREATMENT.
 * The constructor clears `cp->word_110` in the caller's `V92CP`.  With one
 * shared V92CP, our side failing to write it would be covered up by the
 * reference writing it a moment later -- the final state is the same either
 * way.  So the run is done TWICE:
 *
 *     shared     one V92CP for both sides.  Proves +0x74 holds the third
 *                argument and nothing else in the V92CP moved.
 *     separate   two V92CPs seeded identically, one per side, compared
 *                against each other afterwards.  Proves OUR side wrote
 *                +0x110, and wrote only that.
 *
 * WHAT WITNESSES THE TWO ALLOCATIONS.  The mapper's 0x2c and the scrambler's
 * 1 + 23 + 99 bytes are both hidden by the pointer poisoning the comparison
 * needs, so `harness_alloc.allocs` and `.bytes` are asserted ABSOLUTELY --
 * two pieces and 167 bytes, hand-computed from the disassembly -- as well as
 * against the reference.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/V92CP.h"

extern "C" {
void our_c1(void *, void *, void *, void *, void *)
	asm("_ZN18V92Phase4ModulatorC1EP13V92ParametersP15V92BitsToSymbolP5V92C"
	    "PP16V92MappingParams");
void our_c2(void *, void *, void *, void *, void *)
	asm("_ZN18V92Phase4ModulatorC2EP13V92ParametersP15V92BitsToSymbolP5V92C"
	    "PP16V92MappingParams");
void our_d1(void *) asm("_ZN18V92Phase4ModulatorD1Ev");
void our_d2(void *) asm("_ZN18V92Phase4ModulatorD2Ev");
void ref_c1(void *, void *, void *, void *, void *)
	asm("ref__ZN18V92Phase4ModulatorC1EP13V92ParametersP15V92BitsToSymbolP5"
	    "V92CPP16V92MappingParams");
void ref_c2(void *, void *, void *, void *, void *)
	asm("ref__ZN18V92Phase4ModulatorC2EP13V92ParametersP15V92BitsToSymbolP5"
	    "V92CPP16V92MappingParams");
void ref_d1(void *) asm("ref__ZN18V92Phase4ModulatorD1Ev");
void ref_d2(void *) asm("ref__ZN18V92Phase4ModulatorD2Ev");
}

typedef void (*ctor_fn)(void *, void *, void *, void *, void *);
typedef void (*dtor_fn)(void *);

#define GUARD	64
#define OBJSZ	((unsigned)sizeof(V92Phase4Modulator))
#define SLOT	(OBJSZ + GUARD)
#define CPSZ	((unsigned)sizeof(V92CP))

/*
 * The two allocations the constructor makes, from the disassembly and not
 * from a run: `movl $0x2c` at .text+0x179ad for the mapper, and the
 * scrambler's own `(1 + b + c) * sizeof(T)` with (b, c) = (23, 99) and
 * `T = unsigned char`.
 */
#define WANT_ALLOCS	2
#define WANT_BYTES	(0x2c + (1 + 23 + 99))

static unsigned char ours[SLOT] __attribute__((aligned(8)));
static unsigned char theirs[SLOT] __attribute__((aligned(8)));
static unsigned char sown[SLOT];
static unsigned char cmp_a[SLOT];
static unsigned char cmp_b[SLOT];
static unsigned char ctor_a[SLOT];
static unsigned char ctor_b[SLOT];

/* The four arguments.  `params`, `bitsToSymbol` and `mappingParams` are never
 * dereferenced by anything under test; the V92CPs are. */
static unsigned char arg_params[64] __attribute__((aligned(8)));
static unsigned char arg_bts[64] __attribute__((aligned(8)));
static unsigned char arg_mp[64] __attribute__((aligned(8)));
static unsigned char cp_a[CPSZ] __attribute__((aligned(8)));
static unsigned char cp_b[CPSZ] __attribute__((aligned(8)));
static unsigned char cp_seed[CPSZ];

/*
 * What cannot agree between the sides: the scrambler's seven pointers at
 * +0x4c..+0x67 -- its `tailLength` at +0x68 is a count and IS compared -- and
 * the mapper at +0x70.
 */
struct region { unsigned off, len; };
static const struct region skip[] = {
	{ 0x4c, 7 * (unsigned)sizeof(void *) },
	{ 0x70, (unsigned)sizeof(void *) },
};
#define NSKIP	((unsigned)(sizeof(skip) / sizeof(skip[0])))

/* The two owned pointers, for the null-combination runs: the mapper, and the
 * scrambler's buffer inside the member subobject. */
static const unsigned owned[2] = { 0x70, 0x4c };

static unsigned
lfsr_step(unsigned *s)
{
	*s = (*s >> 1) ^ (-(int)(*s & 1u) & 0xb400u);
	return *s;
}

static void
seed(int trial)
{
	unsigned lfsr = 0x1234u + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		lfsr_step(&lfsr);
		switch (trial & 3) {
		case 0:
			v = (unsigned char)(lfsr >> 3);
			break;
		case 1:
			v = 0x17;	/* the far tap, so a copy of it shows */
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
	for (i = 0; i < sizeof(arg_params); i++) {
		arg_params[i] = (unsigned char)(lfsr_step(&lfsr) >> 3);
		arg_bts[i] = (unsigned char)(lfsr_step(&lfsr) >> 3);
		arg_mp[i] = (unsigned char)(lfsr_step(&lfsr) >> 3);
	}
	for (i = 0; i < CPSZ; i++) {
		unsigned char v = (unsigned char)((lfsr_step(&lfsr) >> 3) | 1u);

		cp_a[i] = v;
		cp_b[i] = v;
		cp_seed[i] = v;
	}
}

static int
guard_intact(void)
{
	return memcmp(ours + OBJSZ, sown + OBJSZ, GUARD) == 0
	    && memcmp(theirs + OBJSZ, sown + OBJSZ, GUARD) == 0;
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

static void
compare(const char *what, int shared_cp, long trial)
{
	unsigned i;

	memcpy(cmp_a, ours, OBJSZ);
	memcpy(cmp_b, theirs, OBJSZ);
	for (i = 0; i < NSKIP; i++) {
		memset(cmp_a + skip[i].off, 0x77, skip[i].len);
		memset(cmp_b + skip[i].off, 0x77, skip[i].len);
	}
	/* With a V92CP each, +0x74 is two different addresses by design. */
	if (!shared_cp) {
		memset(cmp_a + 0x74, 0x77, sizeof(void *));
		memset(cmp_b + 0x74, 0x77, sizeof(void *));
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Phase4Modulator", cmp_a,
		     cmp_b, (size_t)OBJSZ, trial);
}

/*
 * The scrambler subobject's geometry, in ELEMENTS, checked against the three
 * constructor arguments.  Its own pointers cannot be compared between the
 * sides; the distances between them can, and they are absolute.
 *
 * `reset` has already run inside the scrambler's constructor, so the three
 * running cursors are at their initial values too, and that is asserted here
 * rather than left to the poisoned comparison.
 */
static void
scram_geometry(long trial)
{
	const unsigned char *lim = (const unsigned char *)slot_ptr(ours, 0x4c);
	const unsigned char *out = (const unsigned char *)slot_ptr(ours, 0x50);
	const unsigned char *t1 = (const unsigned char *)slot_ptr(ours, 0x54);
	const unsigned char *t2 = (const unsigned char *)slot_ptr(ours, 0x58);
	const unsigned char *rlim =
	    (const unsigned char *)slot_ptr(theirs, 0x4c);
	const unsigned char *rout =
	    (const unsigned char *)slot_ptr(theirs, 0x50);
	const unsigned char *rt1 =
	    (const unsigned char *)slot_ptr(theirs, 0x54);
	const unsigned char *rt2 =
	    (const unsigned char *)slot_ptr(theirs, 0x58);
	unsigned tail;

	memcpy(&tail, ours + 0x68, sizeof(tail));

	diff_eq_int("the near tap is 5 elements up (trial %ld)",
		    (int)(t1 - out), V92P4M_SCRAM_TAP1, trial);
	diff_eq_int("the far tap is 23 elements up (trial %ld)",
		    (int)(t2 - out), V92P4M_SCRAM_TAP2, trial);
	diff_eq_int("the restart point is 99 elements above the base "
		    "(trial %ld)", (int)(out - lim), V92P4M_SCRAM_SLACK,
		    trial);
	diff_eq_int("tailLength is the far tap (trial %ld)", (int)tail,
		    V92P4M_SCRAM_TAP2, trial);
	diff_eq_int("pOut is at its initial value (trial %ld)",
		    slot_ptr(ours, 0x5c) == (void *)out, 1, trial);
	diff_eq_int("pTap1 is at its initial value (trial %ld)",
		    slot_ptr(ours, 0x60) == (void *)t1, 1, trial);
	diff_eq_int("pTap2 is at its initial value (trial %ld)",
		    slot_ptr(ours, 0x64) == (void *)t2, 1, trial);

	diff_eq_int("ref near tap is 5 elements up (trial %ld)",
		    (int)(rt1 - rout), V92P4M_SCRAM_TAP1, trial);
	diff_eq_int("ref far tap is 23 elements up (trial %ld)",
		    (int)(rt2 - rout), V92P4M_SCRAM_TAP2, trial);
	diff_eq_int("ref restart point is 99 elements above the base "
		    "(trial %ld)", (int)(rout - rlim), V92P4M_SCRAM_SLACK,
		    trial);
}

/* The V92CP, compared against the seed everywhere but the one word the
 * constructor is entitled to write. */
static int
cp_untouched_but_110(const unsigned char *cp)
{
	return memcmp(cp, cp_seed, 0x110) == 0
	    && memcmp(cp + 0x114, cp_seed + 0x114, CPSZ - 0x114) == 0;
}

#define NTRIAL	24

static int
run_ctor(const char *name, ctor_fn our_ctor, ctor_fn ref_ctor,
	 dtor_fn our_dtor, dtor_fn ref_dtor, int shared_cp)
{
	unsigned char first[SLOT];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes, i;

		seed(trial);
		harness_alloc_reset();

		our_ctor(ours, arg_params, arg_bts, cp_a, arg_mp);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		ref_ctor(theirs, arg_params, arg_bts,
			 shared_cp ? cp_a : cp_b, arg_mp);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		/* Absolute, from the disassembly, and against the reference. */
		diff_eq_int("allocations (trial %ld)", a_allocs, WANT_ALLOCS,
			    trial);
		diff_eq_int("ref allocations (trial %ld)", b_allocs,
			    WANT_ALLOCS, trial);
		diff_eq_int("bytes allocated (trial %ld)", (int)a_bytes,
			    WANT_BYTES, trial);
		diff_eq_int("ref bytes allocated (trial %ld)", (int)b_bytes,
			    WANT_BYTES, trial);
		diff_eq_int("no free of NULL (trial %ld)",
			    harness_alloc.free_null, 0, trial);

		for (i = 0; i < 2; i++) {
			diff_eq_int("owned pointer +0x%02lx is not null",
				    slot_ptr(ours, owned[i]) != 0, 1, owned[i]);
			diff_eq_int("ref owned pointer +0x%02lx is not null",
				    slot_ptr(theirs, owned[i]) != 0, 1,
				    owned[i]);
		}
		diff_eq_int("the mapper and the scrambler buffer are separate "
			    "allocations (trial %ld)",
			    slot_ptr(ours, 0x70) != slot_ptr(ours, 0x4c), 1,
			    trial);

		/*
		 * The three arguments that are only stored, checked against
		 * the addresses they were passed as -- which is what says
		 * argument four lands at +0x48 and not at +0x1c8.
		 */
		diff_eq_int("+0x48 is the fourth argument (trial %ld)",
			    slot_ptr(ours, 0x48) == (void *)arg_mp, 1, trial);
		diff_eq_int("+0x6c is the second argument (trial %ld)",
			    slot_ptr(ours, 0x6c) == (void *)arg_bts, 1, trial);
		diff_eq_int("+0x1c8 is the first argument (trial %ld)",
			    slot_ptr(ours, 0x1c8) == (void *)arg_params, 1,
			    trial);
		diff_eq_int("+0x74 is the third argument (trial %ld)",
			    slot_ptr(ours, 0x74) == (void *)cp_a, 1, trial);

		/*
		 * The caller's V92CP: the one word cleared, and NOTHING else
		 * disturbed.  With a V92CP each, the two are compared against
		 * one another as well -- that is the run in which our side
		 * failing to write +0x110 is visible at all.
		 */
		diff_eq_int("cp->word_110 is cleared (trial %ld)",
			    cp_a[0x110] == 0 && cp_a[0x111] == 0
			    && cp_a[0x112] == 0 && cp_a[0x113] == 0, 1, trial);
		diff_eq_int("nothing else in the V92CP moved (trial %ld)",
			    cp_untouched_but_110(cp_a), 1, trial);
		if (!shared_cp)
			diff_eq_obj_(__FILE__, __LINE__,
				     "the caller's V92CP", "V92CP", cp_a, cp_b,
				     (size_t)CPSZ, trial);

		/*
		 * THE SCRAMBLER'S THREE ARGUMENTS, which are otherwise
		 * invisible: `a` reaches only `pInitTap1`, and all seven of
		 * the scrambler's words are heap addresses the comparison has
		 * to poison.  Their DIFFERENCES are not addresses, and they
		 * are the three arguments back again -- `pInitTap1 -
		 * pInitOut` is `a`, `pInitTap2 - pInitOut` is `b`, and
		 * `pInitOut - pLimit` is `c`, each in units of `sizeof(T)`,
		 * which is one byte for this instantiation.
		 */
		scram_geometry(trial);

		compare("after the constructor", shared_cp, trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(), 1, trial);

		if (memcmp(sown, ours, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, ours, SLOT);
		else if (memcmp(first, ours, SLOT) != 0)
			distinct = 1;

		our_dtor(ours);
		ref_dtor(theirs);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * The destructor over every null/non-null combination of the two pointers it
 * releases -- its own mapper and, through the member subobject, the
 * scrambler's buffer.  The complement is destroyed afterwards so that
 * `live == 0` says the two halves covered both exactly once.
 */
static int
run_dtor(const char *name, ctor_fn our_ctor, ctor_fn ref_ctor,
	 dtor_fn our_dtor, dtor_fn ref_dtor)
{
	unsigned char before[SLOT];
	unsigned subset;
	int saw_null = 0, saw_live = 0, freed = 0;

	diff_begin(name);

	for (subset = 0; subset < 4u; subset++) {
		int a_frees, b_frees, a_null, b_null, a_bad, b_bad;
		unsigned i;

		seed((int)subset);
		harness_alloc_reset();

		our_ctor(ours, arg_params, arg_bts, cp_a, arg_mp);
		ref_ctor(theirs, arg_params, arg_bts, cp_a, arg_mp);
		memcpy(ctor_a, ours, SLOT);
		memcpy(ctor_b, theirs, SLOT);

		for (i = 0; i < 2; i++) {
			if ((subset >> i) & 1u) {
				slot_set_ptr(ours, owned[i], 0);
				slot_set_ptr(theirs, owned[i], 0);
				saw_null = 1;
			} else {
				saw_live = 1;
			}
		}
		memcpy(before, ours, SLOT);

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

		/* One free per live pointer, absolutely: two guards, and
		 * dropping either shows up here or in free_null. */
		diff_eq_int("frees, subset 0x%lx", a_frees,
			    2 - (int)((subset & 1u) + ((subset >> 1) & 1u)),
			    (long)subset);
		diff_eq_int("ref frees, subset 0x%lx", b_frees, a_frees,
			    (long)subset);
		diff_eq_int("sysdep_free(NULL), subset 0x%lx", a_null, 0,
			    (long)subset);
		diff_eq_int("ref sysdep_free(NULL), subset 0x%lx", b_null, 0,
			    (long)subset);
		diff_eq_int("no bad free, subset 0x%lx", a_bad, 0,
			    (long)subset);
		diff_eq_int("ref no bad free, subset 0x%lx", b_bad, 0,
			    (long)subset);

		/*
		 * Nothing is nulled after a free and nothing else is written,
		 * which our own state before the call is what proves -- the
		 * two sides agreeing would not.
		 */
		diff_eq_int("the destructor stored nothing, subset 0x%lx",
			    memcmp(before, ours, SLOT) == 0, 1, (long)subset);

		compare("after the destructor", 1, (long)subset);
		diff_eq_int("nothing stored past the object, subset 0x%lx",
			    guard_intact(), 1, (long)subset);

		if (a_frees > 0)
			freed = 1;

		memcpy(ours, ctor_a, SLOT);
		memcpy(theirs, ctor_b, SLOT);
		for (i = 0; i < 2; i++) {
			if (((subset >> i) & 1u) == 0) {
				slot_set_ptr(ours, owned[i], 0);
				slot_set_ptr(theirs, owned[i], 0);
			}
		}
		our_dtor(ours);
		ref_dtor(theirs);
		diff_eq_int("the two halves freed everything, subset 0x%lx",
			    harness_alloc.live, 0, (long)subset);
		diff_eq_int("and freed nothing twice, subset 0x%lx",
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
	int rc = 0;

	rc |= run_ctor("V92Phase4Modulator::V92Phase4Modulator (C1), one "
		       "shared V92CP", our_c1, ref_c1, our_d1, ref_d1, 1);
	rc |= run_ctor("V92Phase4Modulator::V92Phase4Modulator (C2), one "
		       "shared V92CP", our_c2, ref_c2, our_d2, ref_d2, 1);
	rc |= run_ctor("V92Phase4Modulator::V92Phase4Modulator (C1), a V92CP "
		       "each", our_c1, ref_c1, our_d1, ref_d1, 0);
	rc |= run_ctor("V92Phase4Modulator::V92Phase4Modulator (C2), a V92CP "
		       "each", our_c2, ref_c2, our_d2, ref_d2, 0);
	rc |= run_dtor("V92Phase4Modulator::~V92Phase4Modulator, all four null "
		       "combinations (D1)", our_c1, ref_c1, our_d1, ref_d1);
	rc |= run_dtor("V92Phase4Modulator::~V92Phase4Modulator, all four null "
		       "combinations (D2)", our_c2, ref_c2, our_d2, ref_d2);

	return rc;
}
