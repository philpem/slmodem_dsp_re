/*
 * t_v92jd.cpp -- differential test of V92Jd's six closed methods.
 *
 * Same fixture as t_v90jd.cpp, and for the same reasons: both sides seeded
 * with the SAME varied pseudorandom bytes and reseeded every trial, the whole
 * object compared with diff_eq_obj, a guard region past its end compared
 * separately, and the returned pointers checked against each side's OWN base.
 * Read that file's header for why none of those is optional.
 *
 * sizeof(V92Jd) is 0xdc: the largest this-relative displacement any V92Jd
 * method uses is +0xd8 and it is a four-byte store, so 216 -- the number
 * docs/v90cpp.md gave -- is the displacement and not the size.
 *
 * WHAT THIS TEST ADDS OVER t_v90jd.  The two packs share one CRC register at
 * +0x94, so the order they run in is observable; the run below alternates and
 * also does data-then-phase and phase-then-data within a trial, so a
 * reconstruction that gave each pack its own register would diverge.  And the
 * two accessors are the batch's only non-leaves: each calls its pack, so a
 * comparison of the whole object after `getJdBitVector()` is a comparison of
 * `packJdData()` reached through a call.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Jd.h"

extern "C" {
void ref_packJdData(void *self) asm("ref__ZN5V92Jd10packJdDataEv");
void ref_packJdPhaseData(void *self) asm("ref__ZN5V92Jd15packJdPhaseDataEv");
unsigned char *ref_getJdBitVector(void *self)
	asm("ref__ZN5V92Jd14getJdBitVectorEv");
unsigned char *ref_getJdPhaseBitVector(void *self)
	asm("ref__ZN5V92Jd19getJdPhaseBitVectorEv");
void ref_unPackJdReset(void *self) asm("ref__ZN5V92Jd13unPackJdResetEv");
void ref_unPackJdPhaseReset(void *self)
	asm("ref__ZN5V92Jd18unPackJdPhaseResetEv");
}

#define SLOT 288

union jd_slot {
	V92Jd o;
	unsigned char raw[SLOT];
};

static union jd_slot ours, theirs;

static void
seed(int trial, int mode)
{
	unsigned lfsr = 0x2f6bu + 0x9e37u * (unsigned)trial;
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
	return memcmp(ours.raw + sizeof(V92Jd), theirs.raw + sizeof(V92Jd),
		      SLOT - sizeof(V92Jd)) == 0;
}

#define NTRIAL 24

/* Both packs, in both orders, so the shared CRC register is observable. */
static int
run_packs(void)
{
	unsigned char first[16];
	int trial, distinct = 0, moved = 0;

	diff_begin("V92Jd::packJdData / packJdPhaseData");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		if (trial & 1) {
			ours.o.packJdPhaseData();
			ref_packJdPhaseData(&theirs.o);
			diff_eq_obj("after packJdPhaseData", V92Jd, &ours.o,
				    &theirs.o, trial);
			ours.o.packJdData();
			ref_packJdData(&theirs.o);
			diff_eq_obj("after packJdData", V92Jd, &ours.o,
				    &theirs.o, trial);
		} else {
			ours.o.packJdData();
			ref_packJdData(&theirs.o);
			diff_eq_obj("after packJdData", V92Jd, &ours.o,
				    &theirs.o, trial);
			ours.o.packJdPhaseData();
			ref_packJdPhaseData(&theirs.o);
			diff_eq_obj("after packJdPhaseData", V92Jd, &ours.o,
				    &theirs.o, trial);
		}

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, &ours.o.bits[V90JD_GROUP3 + 1], 16);
		else if (memcmp(first, &ours.o.bits[V90JD_GROUP3 + 1], 16))
			distinct = 1;
	}

	diff_eq_int("the packs changed the object", moved, 1, 0);
	diff_eq_int("the data CRC is not the same on every trial", distinct,
		    1, 0);

	return diff_end();
}

static int
run_getters(void)
{
	unsigned char first[16];
	int trial, distinct = 0;

	diff_begin("V92Jd::getJdBitVector / getJdPhaseBitVector");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char *pa, *pb;

		seed(trial, trial % 4);

		pa = ours.o.getJdBitVector();
		pb = ref_getJdBitVector(&theirs.o);
		diff_eq_int("getJdBitVector() return offset (trial %ld)",
			    pa - ours.raw, pb - theirs.raw, trial);
		diff_eq_obj("after getJdBitVector", V92Jd, &ours.o, &theirs.o,
			    trial);

		pa = ours.o.getJdPhaseBitVector();
		pb = ref_getJdPhaseBitVector(&theirs.o);
		diff_eq_int("getJdPhaseBitVector() return offset (trial %ld)",
			    pa - ours.raw, pb - theirs.raw, trial);
		diff_eq_obj("after getJdPhaseBitVector", V92Jd, &ours.o,
			    &theirs.o, trial);

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (trial == 0)
			memcpy(first, &ours.o.phaseBits[V90JD_GROUP3 + 1], 16);
		else if (memcmp(first, &ours.o.phaseBits[V90JD_GROUP3 + 1], 16))
			distinct = 1;
	}

	diff_eq_int("the phase CRC is not the same on every trial", distinct,
		    1, 0);

	return diff_end();
}

static int
run_resets(void)
{
	int trial;

	diff_begin("V92Jd::unPackJdReset / unPackJdPhaseReset");

	for (trial = 0; trial < NTRIAL; trial++) {
		seed(trial, trial % 4);

		/* Forced non-zero on both sides, so the clear is observable. */
		ours.o.unpack[0] = theirs.o.unpack[0] =
			(unsigned char)(trial | 1);
		ours.o.unpack[1] = theirs.o.unpack[1] =
			(unsigned char)(trial | 2);
		ours.o.unpackWord = theirs.o.unpackWord = 0x5a5a0000 + trial;
		ours.o.unpackPhaseWord = theirs.o.unpackPhaseWord =
			0x33330000 + trial;

		if (trial & 1) {
			ours.o.unPackJdPhaseReset();
			ref_unPackJdPhaseReset(&theirs.o);
			diff_eq_obj("after unPackJdPhaseReset", V92Jd, &ours.o,
				    &theirs.o, trial);
			/*
			 * The phase reset must leave the DATA word alone --
			 * the two share their bytes and not their words.
			 */
			diff_eq_int("unPackJdPhaseReset spared +0xd4 (%ld)",
				    ours.o.unpackWord, 0x5a5a0000 + trial,
				    trial);
			ours.o.unPackJdReset();
			ref_unPackJdReset(&theirs.o);
			diff_eq_obj("after unPackJdReset", V92Jd, &ours.o,
				    &theirs.o, trial);
		} else {
			ours.o.unPackJdReset();
			ref_unPackJdReset(&theirs.o);
			diff_eq_obj("after unPackJdReset", V92Jd, &ours.o,
				    &theirs.o, trial);
			diff_eq_int("unPackJdReset spared +0xd8 (%ld)",
				    ours.o.unpackPhaseWord,
				    0x33330000 + trial, trial);
			ours.o.unPackJdPhaseReset();
			ref_unPackJdPhaseReset(&theirs.o);
			diff_eq_obj("after unPackJdPhaseReset", V92Jd, &ours.o,
				    &theirs.o, trial);
		}

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_packs();
	rc |= run_getters();
	rc |= run_resets();

	return rc;
}
