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

extern "C" {
unsigned char *ref_getBitVector(void *self)
	asm("ref__ZN5V90Jd12getBitVectorEv");
void ref_unPackReset(void *self) asm("ref__ZN5V90Jd11unPackResetEv");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 192

union jd_slot {
	V90Jd o;
	unsigned char raw[SLOT];
};

static union jd_slot ours, theirs;

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

		pa = ours.o.getBitVector();
		pb = ref_getBitVector(&theirs.o);

		/*
		 * The returned pointer, checked against THIS side's own object
		 * rather than merely for being non-null: both are `this + 2`,
		 * and the two objects are at different addresses (finding 224).
		 */
		diff_eq_int("getBitVector() return offset (trial %ld)",
			    pa - ours.raw, pb - theirs.raw, trial);

		diff_eq_obj("after getBitVector", V90Jd, &ours.o, &theirs.o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		/* Anti-vacuity: the call did something, and not the same
		 * something every time. */
		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first_crc, &ours.o.bits[V90JD_GROUP3 + 1], 16);
		else if (memcmp(first_crc, &ours.o.bits[V90JD_GROUP3 + 1], 16))
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
		ours.o.unpack[0] = theirs.o.unpack[0] = (unsigned char)(trial | 1);
		ours.o.unpack[1] = theirs.o.unpack[1] = (unsigned char)(trial | 2);
		ours.o.unpackWord = theirs.o.unpackWord = 0x5a5a0000 + trial;

		ours.o.unPackReset();
		ref_unPackReset(&theirs.o);

		diff_eq_obj("after unPackReset", V90Jd, &ours.o, &theirs.o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("unPackReset cleared +0x8c (trial %ld)",
			    ours.o.unpackWord, 0, trial);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_getbitvector();
	rc |= run_unpackreset();

	return rc;
}
