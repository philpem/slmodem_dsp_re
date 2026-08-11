/*
 * t_v90designers.cpp -- differential test of five constructor/destructor
 * pairs: V90ConstellationDesigner, V90TRN2Designer, V90RDetector,
 * V90ConstellationPower and V90AutoDigitalImpDetector.
 *
 * All five are store-only.  Four of the five destructors and one of the five
 * constructors are a single `ret`, which makes the interesting claim a
 * NEGATIVE one -- "this touches nothing" -- and a negative claim is exactly
 * the kind that passes for the wrong reason.  Three things here are what stop
 * that:
 *
 * NEITHER SIDE IS EVER ZEROED.  Both objects are filled with the same varied
 * pseudorandom bytes before every call and the fill is reseeded per trial, so
 * "the object is unchanged" is a statement about sixty to forty-three-thousand
 * bytes of DIFFERENT content each time rather than about a field of zeros
 * agreeing with itself (findings 223, 224).  The run asserts that the seed
 * really did vary: `seed_varied` compares trial 0's fill against trial 1's.
 *
 * THE COLLABORATORS ARE SHARED AND THEY MOVE.  Every one of these
 * constructors stores pointers it is handed and never dereferences them, so
 * the two sides must be given the SAME pointer -- two separately allocated
 * blocks would compare unequal whatever the constructor did.  There is one
 * pool of four blocks per collaborator type and the trial index picks which,
 * so the value that lands in the object CHANGES from trial to trial and the
 * "not the same something every time" assertion has something to see.
 *
 * BOTH ABI VARIANTS ARE DRIVEN.  GCC emits a complete-object constructor
 * (`C1`) and a base-object constructor (`C2`), and a destructor likewise, and
 * the blob has all four for each of these classes at four different
 * addresses.  Testing only `C1` would leave half the symbols unexercised, so
 * every case below runs twice, once per variant, through a function pointer.
 *
 * C++ HAS NO SYNTAX for running a constructor over storage that already
 * exists, so BOTH sides are called by symbol through asm() labels -- ours by
 * its mangled name, the blob's by the `ref_` alias the harness builds.  That
 * also sidesteps finding 225 entirely: the compiler never sees a name it
 * could mangle a second time.  The convention is plain cdecl with `this` as
 * the first stack argument (finding 215).
 */

#include <string.h>

#include "harness.h"

#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90TRN2Designer.h"
#include "dsplib/V90RDetector.h"
#include "dsplib/V90ConstellationPower.h"
#include "dsplib/V90AutoDigitalImpDetector.h"

typedef void (*ctor1_t)(void *self, void *a);
typedef void (*ctor2_t)(void *self, void *a, void *b);
typedef void (*ctor3_t)(void *self, void *a, void *b, void *c);
typedef void (*ctor0_t)(void *self);
typedef void (*dtor_t)(void *self);

extern "C" {

/* V90ConstellationDesigner(V90Parameters *, V90PreFilter *, V90ConstellationPower *) */
void our_cd_c1(void *, void *, void *, void *)
	asm("_ZN24V90ConstellationDesignerC1EP13V90ParametersP12V90PreFilterP21V90ConstellationPower");
void our_cd_c2(void *, void *, void *, void *)
	asm("_ZN24V90ConstellationDesignerC2EP13V90ParametersP12V90PreFilterP21V90ConstellationPower");
void ref_cd_c1(void *, void *, void *, void *)
	asm("ref__ZN24V90ConstellationDesignerC1EP13V90ParametersP12V90PreFilterP21V90ConstellationPower");
void ref_cd_c2(void *, void *, void *, void *)
	asm("ref__ZN24V90ConstellationDesignerC2EP13V90ParametersP12V90PreFilterP21V90ConstellationPower");
void our_cd_d1(void *) asm("_ZN24V90ConstellationDesignerD1Ev");
void our_cd_d2(void *) asm("_ZN24V90ConstellationDesignerD2Ev");
void ref_cd_d1(void *) asm("ref__ZN24V90ConstellationDesignerD1Ev");
void ref_cd_d2(void *) asm("ref__ZN24V90ConstellationDesignerD2Ev");

/* V90TRN2Designer(V90Parameters *, V90ConstellationPower *) */
void our_trn2_c1(void *, void *, void *)
	asm("_ZN15V90TRN2DesignerC1EP13V90ParametersP21V90ConstellationPower");
void our_trn2_c2(void *, void *, void *)
	asm("_ZN15V90TRN2DesignerC2EP13V90ParametersP21V90ConstellationPower");
void ref_trn2_c1(void *, void *, void *)
	asm("ref__ZN15V90TRN2DesignerC1EP13V90ParametersP21V90ConstellationPower");
void ref_trn2_c2(void *, void *, void *)
	asm("ref__ZN15V90TRN2DesignerC2EP13V90ParametersP21V90ConstellationPower");
void our_trn2_d1(void *) asm("_ZN15V90TRN2DesignerD1Ev");
void our_trn2_d2(void *) asm("_ZN15V90TRN2DesignerD2Ev");
void ref_trn2_d1(void *) asm("ref__ZN15V90TRN2DesignerD1Ev");
void ref_trn2_d2(void *) asm("ref__ZN15V90TRN2DesignerD2Ev");

/* V90RDetector(V90Parameters *) */
void our_rd_c1(void *, void *) asm("_ZN12V90RDetectorC1EP13V90Parameters");
void our_rd_c2(void *, void *) asm("_ZN12V90RDetectorC2EP13V90Parameters");
void ref_rd_c1(void *, void *) asm("ref__ZN12V90RDetectorC1EP13V90Parameters");
void ref_rd_c2(void *, void *) asm("ref__ZN12V90RDetectorC2EP13V90Parameters");
void our_rd_d1(void *) asm("_ZN12V90RDetectorD1Ev");
void our_rd_d2(void *) asm("_ZN12V90RDetectorD2Ev");
void ref_rd_d1(void *) asm("ref__ZN12V90RDetectorD1Ev");
void ref_rd_d2(void *) asm("ref__ZN12V90RDetectorD2Ev");

/* V90ConstellationPower() */
void our_cp_c1(void *) asm("_ZN21V90ConstellationPowerC1Ev");
void our_cp_c2(void *) asm("_ZN21V90ConstellationPowerC2Ev");
void ref_cp_c1(void *) asm("ref__ZN21V90ConstellationPowerC1Ev");
void ref_cp_c2(void *) asm("ref__ZN21V90ConstellationPowerC2Ev");
void our_cp_d1(void *) asm("_ZN21V90ConstellationPowerD1Ev");
void our_cp_d2(void *) asm("_ZN21V90ConstellationPowerD2Ev");
void ref_cp_d1(void *) asm("ref__ZN21V90ConstellationPowerD1Ev");
void ref_cp_d2(void *) asm("ref__ZN21V90ConstellationPowerD2Ev");

/* V90AutoDigitalImpDetector(V90Parameters *) */
void our_adid_c1(void *, void *)
	asm("_ZN25V90AutoDigitalImpDetectorC1EP13V90Parameters");
void our_adid_c2(void *, void *)
	asm("_ZN25V90AutoDigitalImpDetectorC2EP13V90Parameters");
void ref_adid_c1(void *, void *)
	asm("ref__ZN25V90AutoDigitalImpDetectorC1EP13V90Parameters");
void ref_adid_c2(void *, void *)
	asm("ref__ZN25V90AutoDigitalImpDetectorC2EP13V90Parameters");
void our_adid_d1(void *) asm("_ZN25V90AutoDigitalImpDetectorD1Ev");
void our_adid_d2(void *) asm("_ZN25V90AutoDigitalImpDetectorD2Ev");
void ref_adid_d1(void *) asm("ref__ZN25V90AutoDigitalImpDetectorD1Ev");
void ref_adid_d2(void *) asm("ref__ZN25V90AutoDigitalImpDetectorD2Ev");

}

/*
 * THE COLLABORATOR POOL.  Four blocks, one address each, shared between the
 * two sides and picked by the trial index so that what the constructor stores
 * is different from trial to trial.  Nothing reads them -- none of these
 * constructors dereferences a pointer -- but they are filled anyway, so that a
 * constructor that DID read one would be reading varied bytes rather than
 * zeros.
 */
#define POOL	4
#define BLK	64

static unsigned char pool[POOL][BLK] __attribute__((aligned(8)));

/* The largest object under test, plus room past its end for the guard. */
#define GUARD		96
#define MAX_OBJ		((int)sizeof(V90AutoDigitalImpDetector))
#define MAX_SLOT	(MAX_OBJ + GUARD)

static unsigned char ours[MAX_SLOT] __attribute__((aligned(8)));
static unsigned char theirs[MAX_SLOT] __attribute__((aligned(8)));
static unsigned char before[MAX_SLOT];
static unsigned char trial0[MAX_SLOT];

static unsigned lfsr;

static void
seed(int slot_bytes, int trial)
{
	int i;

	lfsr = 0x2f1bu + 0x9e37u * (unsigned)trial + 1u;
	for (i = 0; i < slot_bytes; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		/*
		 * Three fills, because a constructor that stores a constant is
		 * invisible against a fill that happens to equal it.  0xa5 is
		 * the harness's own malloc pattern and 0x00 is what a zeroed
		 * object would look like, so both of the values a lazy pass
		 * could hide behind are included on purpose.
		 */
		switch (trial % 4) {
		case 1:
			v = 0xa5;
			break;
		case 2:
			v = 0x00;
			break;
		default:
			v = (unsigned char)(lfsr >> 3);
			break;
		}
		ours[i] = v;
		theirs[i] = v;
	}
	for (i = 0; i < POOL; i++) {
		int j;

		for (j = 0; j < BLK; j++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
			pool[i][j] = (unsigned char)(lfsr >> 5);
		}
	}
	memcpy(before, ours, (size_t)slot_bytes);
	if (trial == 0)
		memcpy(trial0, ours, (size_t)slot_bytes);
}

/*
 * Everything past the object's declared end must be untouched on BOTH sides
 * -- not merely equal to each other, which a store both sides made would also
 * satisfy.  So this compares each side against its own pre-call snapshot.
 */
static int
guard_intact(int obj_bytes, int slot_bytes)
{
	return memcmp(ours + obj_bytes, before + obj_bytes,
		      (size_t)(slot_bytes - obj_bytes)) == 0 &&
	       memcmp(theirs + obj_bytes, before + obj_bytes,
		      (size_t)(slot_bytes - obj_bytes)) == 0;
}

#define NTRIAL	16

/*
 * The five cases differ only in the arity of the constructor, so each one is
 * written out rather than driven from a table: a table would need a cast per
 * call anyway, and the explicit form is what makes the argument order visible
 * next to the mangled name it has to agree with.
 */

static int
run_cd(void)
{
	static const int obj = (int)sizeof(V90ConstellationDesigner);
	const int slot = obj + GUARD;
	int trial, variant, moved = 0, distinct = 0;
	unsigned first = 0;
	void *firstStored = 0;

	diff_begin("V90ConstellationDesigner::V90ConstellationDesigner");

	for (variant = 0; variant < 2; variant++) {
		ctor3_t ours_ctor = variant ? (ctor3_t)our_cd_c2
					    : (ctor3_t)our_cd_c1;
		ctor3_t ref_ctor = variant ? (ctor3_t)ref_cd_c2
					   : (ctor3_t)ref_cd_c1;

		for (trial = 0; trial < NTRIAL; trial++) {
			void *p = pool[trial % POOL];
			void *f = pool[(trial + 1) % POOL];
			void *w = pool[(trial + 2) % POOL];

			seed(slot, trial);

			ours_ctor(ours, p, f, w);
			ref_ctor(theirs, p, f, w);

			diff_eq_obj("after constructor",
				    V90ConstellationDesigner, ours, theirs,
				    trial * 2 + variant);
			diff_eq_int("no store past the object (trial %ld)",
				    guard_intact(obj, slot), 1,
				    trial * 2 + variant);

			if (memcmp(before, ours, (size_t)slot) != 0)
				moved = 1;
			if (trial == 0 && variant == 0)
				first = ((V90ConstellationDesigner *)ours)
					->byte_38;
			/*
			 * The two rate defaults are constants, so the value
			 * that has to VARY across trials is the collaborator
			 * pointer the constructor stores.  Trial 0's is kept
			 * and every later one compared against it, which is
			 * the check finding 224 asks for -- not merely "it
			 * stored something" but "it did not store the same
			 * thing every time".
			 */
			{
				void *got = (void *)((V90ConstellationDesigner *)
						     ours)->power;

				if (trial == 0 && variant == 0)
					firstStored = got;
				else if (got != firstStored)
					distinct = 1;
			}
		}
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("what it stored was not the same every trial", distinct,
		    1, 0);
	diff_eq_int("+0x38 is 0x16", (int)first, 0x16, 0);
	diff_eq_int("the seed varied between trials",
		    memcmp(trial0, before, (size_t)slot) != 0, 1, 0);

	/* The destructor: one byte, and the claim is that it writes nothing. */
	for (variant = 0; variant < 2; variant++) {
		dtor_t ours_dtor = variant ? (dtor_t)our_cd_d2
					   : (dtor_t)our_cd_d1;
		dtor_t ref_dtor = variant ? (dtor_t)ref_cd_d2
					  : (dtor_t)ref_cd_d1;

		for (trial = 0; trial < NTRIAL; trial++) {
			seed(slot, trial);
			ours_dtor(ours);
			ref_dtor(theirs);

			diff_eq_obj("after destructor",
				    V90ConstellationDesigner, ours, theirs,
				    trial * 2 + variant);
			diff_eq_int("the destructor wrote nothing (trial %ld)",
				    memcmp(before, ours, (size_t)slot) == 0 &&
				    memcmp(before, theirs, (size_t)slot) == 0,
				    1, trial * 2 + variant);
		}
	}

	return diff_end();
}

static int
run_trn2(void)
{
	static const int obj = (int)sizeof(V90TRN2Designer);
	/*
	 * The slot is 256 bytes, far more than the eight the header claims,
	 * and deliberately: `V90TRN2Design` was not read, so the object may be
	 * larger than the four members that were.  Comparing the whole slot is
	 * what turns "the constructor writes +0x00 and +0x04 and nothing else"
	 * into a measurement that would fail if it wrote at +0x40.
	 */
	const int slot = 256;
	int trial, variant, moved = 0, distinct = 0;
	void *firstStored = 0;

	diff_begin("V90TRN2Designer::V90TRN2Designer");

	for (variant = 0; variant < 2; variant++) {
		ctor2_t ours_ctor = variant ? (ctor2_t)our_trn2_c2
					    : (ctor2_t)our_trn2_c1;
		ctor2_t ref_ctor = variant ? (ctor2_t)ref_trn2_c2
					   : (ctor2_t)ref_trn2_c1;

		for (trial = 0; trial < NTRIAL; trial++) {
			void *p = pool[trial % POOL];
			void *w = pool[(trial + 2) % POOL];

			seed(slot, trial);

			ours_ctor(ours, p, w);
			ref_ctor(theirs, p, w);

			diff_eq_obj("after constructor", V90TRN2Designer,
				    ours, theirs, trial * 2 + variant);
			diff_eq_int("no store past +0x07 (trial %ld)",
				    guard_intact(obj, slot), 1,
				    trial * 2 + variant);

			if (memcmp(before, ours, (size_t)slot) != 0)
				moved = 1;
			{
				void *got = (void *)((V90TRN2Designer *)ours)
						->params;

				if (trial == 0 && variant == 0)
					firstStored = got;
				else if (got != firstStored)
					distinct = 1;
			}
		}
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("what it stored was not the same every trial", distinct,
		    1, 0);

	for (variant = 0; variant < 2; variant++) {
		dtor_t ours_dtor = variant ? (dtor_t)our_trn2_d2
					   : (dtor_t)our_trn2_d1;
		dtor_t ref_dtor = variant ? (dtor_t)ref_trn2_d2
					  : (dtor_t)ref_trn2_d1;

		for (trial = 0; trial < NTRIAL; trial++) {
			seed(slot, trial);
			ours_dtor(ours);
			ref_dtor(theirs);

			diff_eq_int("the destructor wrote nothing (trial %ld)",
				    memcmp(before, ours, (size_t)slot) == 0 &&
				    memcmp(before, theirs, (size_t)slot) == 0,
				    1, trial * 2 + variant);
		}
	}

	return diff_end();
}

static int
run_rd(void)
{
	static const int obj = (int)sizeof(V90RDetector);
	const int slot = obj + GUARD;
	int trial, variant, moved = 0, distinct = 0;
	void *firstStored = 0;

	diff_begin("V90RDetector::V90RDetector");

	for (variant = 0; variant < 2; variant++) {
		ctor1_t ours_ctor = variant ? (ctor1_t)our_rd_c2
					    : (ctor1_t)our_rd_c1;
		ctor1_t ref_ctor = variant ? (ctor1_t)ref_rd_c2
					   : (ctor1_t)ref_rd_c1;

		for (trial = 0; trial < NTRIAL; trial++) {
			void *p = pool[trial % POOL];

			seed(slot, trial);

			ours_ctor(ours, p);
			ref_ctor(theirs, p);

			diff_eq_obj("after constructor", V90RDetector,
				    ours, theirs, trial * 2 + variant);
			diff_eq_int("no store past the object (trial %ld)",
				    guard_intact(obj, slot), 1,
				    trial * 2 + variant);

			/*
			 * The constructor's whole claim is one store, so the
			 * complement is worth asserting on its own: every byte
			 * below +0x28 is exactly what the seed left there.
			 */
			diff_eq_int("nothing below +0x28 moved (trial %ld)",
				    memcmp(before, ours, 0x28) == 0, 1,
				    trial * 2 + variant);

			if (memcmp(before, ours, (size_t)slot) != 0)
				moved = 1;
			{
				void *got = (void *)((V90RDetector *)ours)
						->params;

				if (trial == 0 && variant == 0)
					firstStored = got;
				else if (got != firstStored)
					distinct = 1;
			}
		}
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("what it stored was not the same every trial", distinct,
		    1, 0);

	for (variant = 0; variant < 2; variant++) {
		dtor_t ours_dtor = variant ? (dtor_t)our_rd_d2
					   : (dtor_t)our_rd_d1;
		dtor_t ref_dtor = variant ? (dtor_t)ref_rd_d2
					  : (dtor_t)ref_rd_d1;

		for (trial = 0; trial < NTRIAL; trial++) {
			seed(slot, trial);
			ours_dtor(ours);
			ref_dtor(theirs);

			diff_eq_int("the destructor wrote nothing (trial %ld)",
				    memcmp(before, ours, (size_t)slot) == 0 &&
				    memcmp(before, theirs, (size_t)slot) == 0,
				    1, trial * 2 + variant);
		}
	}

	return diff_end();
}

static int
run_cp(void)
{
	static const int obj = (int)sizeof(V90ConstellationPower);
	const int slot = obj + GUARD;
	int trial, variant;

	/*
	 * Both the constructor and the destructor are a single `ret`, so there
	 * is nothing here to assert changed -- the whole claim is that 144
	 * bytes of varied content survive four calls untouched.  What keeps
	 * that from being vacuous is the seed: it is reseeded per trial and
	 * asserted to have varied, so the four calls are being run over
	 * genuinely different memory each time.
	 */
	diff_begin("V90ConstellationPower::V90ConstellationPower");

	for (variant = 0; variant < 2; variant++) {
		ctor0_t ours_ctor = variant ? (ctor0_t)our_cp_c2
					    : (ctor0_t)our_cp_c1;
		ctor0_t ref_ctor = variant ? (ctor0_t)ref_cp_c2
					   : (ctor0_t)ref_cp_c1;
		dtor_t ours_dtor = variant ? (dtor_t)our_cp_d2
					   : (dtor_t)our_cp_d1;
		dtor_t ref_dtor = variant ? (dtor_t)ref_cp_d2
					  : (dtor_t)ref_cp_d1;

		for (trial = 0; trial < NTRIAL; trial++) {
			seed(slot, trial);

			ours_ctor(ours);
			ref_ctor(theirs);
			ours_dtor(ours);
			ref_dtor(theirs);

			diff_eq_obj("after constructor and destructor",
				    V90ConstellationPower, ours, theirs,
				    trial * 2 + variant);
			diff_eq_int("neither side wrote anything (trial %ld)",
				    memcmp(before, ours, (size_t)slot) == 0 &&
				    memcmp(before, theirs, (size_t)slot) == 0,
				    1, trial * 2 + variant);
			diff_eq_int("no store past the object (trial %ld)",
				    guard_intact(obj, slot), 1,
				    trial * 2 + variant);
		}
	}

	diff_eq_int("the seed varied between trials",
		    memcmp(trial0, before, (size_t)slot) != 0, 1, 0);

	return diff_end();
}

static int
run_adid(void)
{
	static const int obj = (int)sizeof(V90AutoDigitalImpDetector);
	const int slot = obj + GUARD;
	int trial, variant, moved = 0, distinct = 0;
	void *firstStored = 0;

	diff_begin("V90AutoDigitalImpDetector::V90AutoDigitalImpDetector");

	for (variant = 0; variant < 2; variant++) {
		ctor1_t ours_ctor = variant ? (ctor1_t)our_adid_c2
					    : (ctor1_t)our_adid_c1;
		ctor1_t ref_ctor = variant ? (ctor1_t)ref_adid_c2
					   : (ctor1_t)ref_adid_c1;

		for (trial = 0; trial < NTRIAL; trial++) {
			void *p = pool[trial % POOL];

			seed(slot, trial);

			ours_ctor(ours, p);
			ref_ctor(theirs, p);

			diff_eq_obj("after constructor",
				    V90AutoDigitalImpDetector, ours, theirs,
				    trial * 2 + variant);
			diff_eq_int("no store past the object (trial %ld)",
				    guard_intact(obj, slot), 1,
				    trial * 2 + variant);
			/*
			 * One store into 43,440 bytes: assert the complement
			 * on both sides of it, so an extra clear loop anywhere
			 * in the object would fail here rather than pass.
			 */
			diff_eq_int("nothing below +0x2814 moved (trial %ld)",
				    memcmp(before, ours, 0x2814) == 0, 1,
				    trial * 2 + variant);
			diff_eq_int("nothing above +0x2817 moved (trial %ld)",
				    memcmp(before + 0x2818, ours + 0x2818,
					   (size_t)(obj - 0x2818)) == 0, 1,
				    trial * 2 + variant);

			if (memcmp(before, ours, (size_t)slot) != 0)
				moved = 1;
			{
				void *got = (void *)
					((V90AutoDigitalImpDetector *)ours)
						->params;

				if (trial == 0 && variant == 0)
					firstStored = got;
				else if (got != firstStored)
					distinct = 1;
			}
		}
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("what it stored was not the same every trial", distinct,
		    1, 0);

	for (variant = 0; variant < 2; variant++) {
		dtor_t ours_dtor = variant ? (dtor_t)our_adid_d2
					   : (dtor_t)our_adid_d1;
		dtor_t ref_dtor = variant ? (dtor_t)ref_adid_d2
					  : (dtor_t)ref_adid_d1;

		for (trial = 0; trial < NTRIAL; trial++) {
			seed(slot, trial);
			ours_dtor(ours);
			ref_dtor(theirs);

			diff_eq_int("the destructor wrote nothing (trial %ld)",
				    memcmp(before, ours, (size_t)slot) == 0 &&
				    memcmp(before, theirs, (size_t)slot) == 0,
				    1, trial * 2 + variant);
		}
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_cd();
	rc |= run_trn2();
	rc |= run_rd();
	rc |= run_cp();
	rc |= run_adid();

	return rc;
}
