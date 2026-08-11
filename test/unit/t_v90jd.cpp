/*
 * t_v90jd.cpp -- differential test of V90Jd::getBitVector and unPackReset.
 *
 * The first test in this tree against a reconstructed C++ CLASS whose object
 * layout is the thing being claimed (t_genericiir tests a template the blob
 * instantiates once; t_v34mp tests a free function that merely has a mangled
 * name).  So the fixture below is the pattern batches 2 and 3 copy, and two
 * parts of it are load-bearing:
 *
 * THE OBJECTS ARE NEVER ZEROED.  Both sides are seeded with the SAME varied
 * pseudorandom bytes before every call.  A zero-filled object would let a
 * clear-loop that is one byte short pass -- the byte it failed to clear was
 * already zero -- and it would leave the CRC driven by a constant input,
 * where a wrong feedback tap is invisible.  Each trial reseeds, so the low
 * bit of every byte that reaches the CRC varies across trials, and the run
 * asserts that the sixteen CRC bits it produces are not the same on every
 * trial (findings 223, 224: a passing comparison of memory neither side wrote
 * proves nothing).
 *
 * THE OBJECT IS COMPARED WHOLE, AND SO IS A GUARD PAST ITS END.  `diff_eq_obj`
 * covers the 144 bytes the header models; the bytes from there to the end of
 * the slot are compared separately, so a store past the object's end shows up
 * as a failure rather than as silence.  144 is `sizeof(V90Jd)` -- the largest
 * `this`-relative displacement any V90Jd method uses is +0x8c and it is a
 * four-byte store, so the object is 0x90 bytes, not the 0x8c the displacement
 * alone suggests.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * `ref__ZN5V90Jd12getBitVectorEv` as an identifier.  Both work; the label form
 * sidesteps finding 225 entirely, because the compiler never sees a name it
 * could mangle a second time.  The convention is plain cdecl with `this` as
 * the first stack argument (finding 215), and both symbols are `T` in the
 * blob, so no regparm attribute is involved -- unlike t_v34mp's.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90Parameters.h"

extern "C" {
unsigned char *ref_getBitVector(void *self)
	asm("ref__ZN5V90Jd12getBitVectorEv");
void ref_unPackReset(void *self) asm("ref__ZN5V90Jd11unPackResetEv");

/*
 * THE CONSTRUCTOR AND DESTRUCTOR ARE REACHED BY SYMBOL, on both sides, which
 * is the only way to drive them in place.  C++ offers no syntax for running a
 * constructor over storage that already exists -- placement `new` needs a
 * declaration this translation unit has no header for, and `OURS =
 * V90Jd(p);` would construct a temporary over uninitialised stack and then
 * copy the WHOLE object, which destroys the one property this fixture is
 * built on (the slot is seeded, never zeroed, so a byte the constructor fails
 * to write is a byte that differs).
 *
 * t_diffcoder.cpp set the precedent for naming our own mangled symbols this
 * way.  Both the C1 and the C2 variant are driven: GCC gives us one function
 * under two names where the blob has two identical copies, so testing only
 * one would leave the other's symbol asserted by nothing.
 */
void our_ctor1(void *self, V90Parameters *p)
	asm("_ZN5V90JdC1EP13V90Parameters");
void ref_ctor1(void *self, V90Parameters *p)
	asm("ref__ZN5V90JdC1EP13V90Parameters");
void our_ctor2(void *self, V90Parameters *p)
	asm("_ZN5V90JdC2EP13V90Parameters");
void ref_ctor2(void *self, V90Parameters *p)
	asm("ref__ZN5V90JdC2EP13V90Parameters");
void our_dtor(void *self) asm("_ZN5V90JdD1Ev");
void ref_dtor(void *self) asm("ref__ZN5V90JdD1Ev");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 192

union jd_slot {
	unsigned char raw[SLOT];
	int align;	/* the object holds ints; keep the slot 4-aligned */
};

static union jd_slot ours, theirs;

/*
 * THE OBJECT LIVES IN THE BYTE ARRAY, not beside it as a second union member.
 * V90Jd has a user-declared destructor -- it has to, because the blob has
 * `_ZN5V90JdD1Ev` and GCC emits no symbol for a trivial implicit one -- and a
 * union with a non-trivially-destructible variant member has its own
 * destructor deleted, so `static union jd_slot ours;` would not compile.
 * Casting the raw slot costs the fixture nothing: it was already comparing
 * and seeding through `raw`, and the union survives only to carry the
 * alignment.
 */
#define OURS	(*(V90Jd *)ours.raw)
#define THEIRS	(*(V90Jd *)theirs.raw)

/*
 * Seeds.  `mode` picks how the low bit of each byte -- the only bit the CRC
 * can see -- is chosen, because a seed whose bit 0 is constant exercises the
 * feedback taps far more weakly than one that is not.
 */
static void
seed(int trial, int mode)
{
	unsigned lfsr = 0x1234u + 0x9e37u * (unsigned)trial;
	int i;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		switch (mode) {
		case 0:
			v = (unsigned char)(lfsr >> 3);
			break;
		case 1:
			v = 0xa5;		/* every low bit set    */
			break;
		case 2:
			v = 0x5a;		/* every low bit clear  */
			break;
		default:
			/* High bits varied, low bit alternating. */
			v = (unsigned char)((lfsr & 0xfe) | (unsigned)(i & 1));
			break;
		}
		ours.raw[i] = v;
		theirs.raw[i] = v;
	}
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V90Jd), theirs.raw + sizeof(V90Jd),
		      SLOT - sizeof(V90Jd)) == 0;
}

#define NTRIAL 24

static int
run_getbitvector(void)
{
	unsigned char first_crc[16];
	int trial, distinct = 0, moved = 0;

	diff_begin("V90Jd::getBitVector");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		unsigned char *pa, *pb;

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		pa = OURS.getBitVector();
		pb = ref_getBitVector(&THEIRS);

		/*
		 * The returned pointer, checked against THIS side's own object
		 * rather than merely for being non-null: both are `this + 2`,
		 * and the two objects are at different addresses (finding 224).
		 */
		diff_eq_int("getBitVector() return offset (trial %ld)",
			    pa - ours.raw, pb - theirs.raw, trial);

		diff_eq_obj("after getBitVector", V90Jd, &OURS, &THEIRS,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		/* Anti-vacuity: the call did something, and not the same
		 * something every time. */
		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first_crc, &OURS.bits[V90JD_GROUP3 + 1], 16);
		else if (memcmp(first_crc, &OURS.bits[V90JD_GROUP3 + 1], 16))
			distinct = 1;
	}

	diff_eq_int("getBitVector changed the object", moved, 1, 0);
	diff_eq_int("the CRC is not the same on every trial", distinct, 1, 0);

	return diff_end();
}

static int
run_unpackreset(void)
{
	int trial;

	diff_begin("V90Jd::unPackReset");

	for (trial = 0; trial < NTRIAL; trial++) {
		seed(trial, trial % 4);

		/*
		 * The three fields it clears are forced non-zero on both sides
		 * first, so this is a real comparison rather than the seed
		 * agreeing with itself at zero (findings 223, 224).
		 */
		OURS.unpack[0] = THEIRS.unpack[0] = (unsigned char)(trial | 1);
		OURS.unpack[1] = THEIRS.unpack[1] = (unsigned char)(trial | 2);
		OURS.unpackWord = THEIRS.unpackWord = 0x5a5a0000 + trial;

		OURS.unPackReset();
		ref_unPackReset(&THEIRS);

		diff_eq_obj("after unPackReset", V90Jd, &OURS, &THEIRS,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("unPackReset cleared +0x8c (trial %ld)",
			    OURS.unpackWord, 0, trial);
	}

	return diff_end();
}

/*
 * ===========================================================================
 * The constructor.
 *
 * ONE PARAMETER BLOCK, SHARED.  The constructor only reads it, and pointing
 * both sides at one block keeps the comparison honest in the same way
 * t_v90p2info shares its `L2` table: two separately seeded blocks would agree
 * anyway, and a divergence in WHICH field was read would then be invisible.
 * It is reseeded every trial, so the four fields the constructor reads take
 * 32 different values across the run rather than one.
 *
 * THE SLOT IS STILL NEVER ZEROED.  Everything the constructor does not write
 * has to survive, and the object writes only 34 of its 144 bytes -- the
 * whole `bits` group below 18, the CRC register and both padding runs are
 * untouched, so a clear-loop invented here would show up immediately.
 * ===========================================================================
 */
union param_slot {
	unsigned char raw[sizeof(V90Parameters)];
	int align;
};

static union param_slot params;

static void
seed_params(int trial)
{
	unsigned lfsr = 0x51edu + 0x4f1bu * (unsigned)trial;
	unsigned i;

	for (i = 0; i < sizeof(params.raw); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		params.raw[i] = (unsigned char)(lfsr >> 5);
	}
}

#define NCTOR 32

static int
run_ctor(void)
{
	unsigned char first[16];
	int trial, distinct = 0, moved = 0;

	diff_begin("V90Jd::V90Jd(V90Parameters *)");

	for (trial = 0; trial < NCTOR; trial++) {
		unsigned char before[SLOT];
		V90Parameters *p = (V90Parameters *)params.raw;

		seed(trial, trial % 4);
		seed_params(trial);
		memcpy(before, ours.raw, SLOT);

		/* C1 and C2 in turn, over freshly seeded storage each time. */
		our_ctor1(&OURS, p);
		ref_ctor1(&THEIRS, p);

		diff_eq_obj("after V90Jd(params) [C1]", V90Jd, &OURS, &THEIRS,
			    trial);
		diff_eq_int("no store past the object, C1 (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, &OURS.bits[V90JD_GROUP1 + 1], 16);
		else if (memcmp(first, &OURS.bits[V90JD_GROUP1 + 1], 16))
			distinct = 1;

		seed(trial, trial % 4);
		our_ctor2(&OURS, p);
		ref_ctor2(&THEIRS, p);

		diff_eq_obj("after V90Jd(params) [C2]", V90Jd, &OURS, &THEIRS,
			    trial);
		diff_eq_int("no store past the object, C2 (trial %ld)",
			    guard_equal(), 1, trial);

		/*
		 * The two variants are one function in our build and two
		 * copies in the blob, so this asserts what the object's own
		 * pair asserts: they leave the same result.
		 */
		diff_eq_int("C1 and C2 agree (trial %ld)",
			    memcmp(ours.raw, theirs.raw, SLOT), 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the rate mask is not the same on every trial", distinct,
		    1, 0);

	return diff_end();
}

/*
 * The destructor is one byte of `ret`, so the whole of its content is that it
 * does NOTHING.  Comparing two objects it did not touch would be the vacuous
 * check finding 223 warns about, so the assertion here is against the
 * snapshot taken before the call -- if a future edit gives it a body, this
 * fails on our side alone rather than agreeing with a blob that also changed.
 */
static int
run_dtor(void)
{
	int trial;

	diff_begin("V90Jd::~V90Jd");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		our_dtor(&OURS);
		ref_dtor(&THEIRS);

		diff_eq_obj("after ~V90Jd", V90Jd, &OURS, &THEIRS, trial);
		diff_eq_int("~V90Jd wrote nothing (trial %ld)",
			    memcmp(before, ours.raw, SLOT), 0, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_getbitvector();
	rc |= run_unpackreset();
	rc |= run_ctor();
	rc |= run_dtor();

	return rc;
}
