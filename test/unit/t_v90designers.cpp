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
 * agreeing with itself (findings F223, F224).  The run asserts that the seed
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
 * also sidesteps finding F225 entirely: the compiler never sees a name it
 * could mangle a second time.  The convention is plain cdecl with `this` as
 * the first stack argument (finding F215).
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

/*
 * V90RDetector's run members.  `this` is the first stack argument as
 * everywhere else, and the sample is declared `int` on purpose: a `short`
 * argument occupies a whole stack slot in cdecl, and passing the widened
 * value is what the object's `movswl 0xc(%esp)` reads.
 */
void our_rd_reset(void *, unsigned, unsigned)
	asm("_ZN12V90RDetector5resetEjj");
void ref_rd_reset(void *, unsigned, unsigned)
	asm("ref__ZN12V90RDetector5resetEjj");
int our_rd_detR(void *, int) asm("_ZN12V90RDetector7detectREs");
int ref_rd_detR(void *, int) asm("ref__ZN12V90RDetector7detectREs");
int our_rd_detRNot(void *, int) asm("_ZN12V90RDetector10detectRNotEs");
int ref_rd_detRNot(void *, int) asm("ref__ZN12V90RDetector10detectRNotEs");
int our_rd_detRf(void *, int) asm("_ZN12V90RDetector8detectRfEs");
int ref_rd_detRf(void *, int) asm("ref__ZN12V90RDetector8detectRfEs");
int our_rd_detRfNot(void *, int) asm("_ZN12V90RDetector11detectRfNotEs");
int ref_rd_detRfNot(void *, int) asm("ref__ZN12V90RDetector11detectRfNotEs");

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
					->powerLadderIndex;
			/*
			 * The two rate defaults are constants, so the value
			 * that has to VARY across trials is the collaborator
			 * pointer the constructor stores.  Trial 0's is kept
			 * and every later one compared against it, which is
			 * the check finding F224 asks for -- not merely "it
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


/* ------------------------------------- V90RDetector, the five run members */

/*
 * `reset` turns two SAMPLE COUNTS into four limits by rounding down, so the
 * arguments are swept around every multiple of 6 and 12 and past the point
 * where the reciprocal division could go wrong.  0xffffffff is there because
 * the object divides UNSIGNED: a signed division of it would round the other
 * way and land on a different limit.
 */
static const unsigned rd_counts[] = {
	0u, 1u, 5u, 6u, 7u, 11u, 12u, 13u, 23u, 24u, 71u, 72u, 0xffffffffu
};
#define RD_NCOUNT ((int)(sizeof(rd_counts) / sizeof(rd_counts[0])))

/*
 * Sign patterns, oldest bit first as the register shifts them in.  The four
 * the detectors look for, their complements, and three that match nothing --
 * because the branch that matches NEITHER pattern clears both run counters
 * and is as much a decision as the ones that do.
 */
static const unsigned rd_pat6[] = { 0x38u, 0x07u, 0x3fu, 0x00u, 0x2au, 0x15u,
				    0x1cu };
#define RD_NPAT6 ((int)(sizeof(rd_pat6) / sizeof(rd_pat6[0])))

static const unsigned rd_pat12[] = { 0xcccu, 0x333u, 0xfffu, 0x000u, 0x555u,
				     0xaaau, 0x666u };
#define RD_NPAT12 ((int)(sizeof(rd_pat12) / sizeof(rd_pat12[0])))

/*
 * The sample for one bit.  A set bit must be STRICTLY positive and a clear
 * one must not be, so zero belongs on the clear side -- the object tests
 * `jle` on the 16-bit value and a reconstruction using `>= 0` or `!= 0` would
 * differ on exactly that sample.  The magnitudes vary so that nothing can be
 * reading the value rather than its sign.
 */
static short
rd_sample(int bit, int k)
{
	static const short pos[] = { 1, 2, 300, 32767, 7 };
	static const short neg[] = { 0, -1, -300, -32768, -7 };

	return bit ? pos[k % 5] : neg[k % 5];
}

static int
run_rd_detect(void)
{
	static const int obj = (int)sizeof(V90RDetector);
	const int slot = obj + GUARD;
	int seen0 = 0, seenPos = 0, seenNeg = 0, seenNot = 0, seenClear = 0;
	int which, trial;

	diff_begin("V90RDetector::reset and the four detectors");

	/* First `reset` alone, over every count pair. */
	for (trial = 0; trial < RD_NCOUNT * RD_NCOUNT; trial++) {
		unsigned a = rd_counts[trial % RD_NCOUNT];
		unsigned b = rd_counts[(trial / RD_NCOUNT) % RD_NCOUNT];

		seed(slot, trial);
		our_rd_reset(ours, a, b);
		ref_rd_reset(theirs, a, b);

		diff_eq_obj("after reset", V90RDetector, ours, theirs, trial);
		diff_eq_int("reset stored nothing past the object (%ld)",
			    guard_intact(obj, slot), 1, trial);

		/*
		 * The four limits at their ABSOLUTE offsets on the BLOB's
		 * object, computed here a different way -- by subtracting the
		 * remainder rather than by dividing and multiplying -- so the
		 * check is not the implementation restated.
		 */
		diff_eq_int("blob: +0x04 is argument 1 down to a six (%ld)",
			    (unsigned)((V90RDetector *)theirs)->rLimit,
			    a - a % 6u, a);
		diff_eq_int("blob: +0x08 is argument 2 down to a six (%ld)",
			    (unsigned)((V90RDetector *)theirs)->rNotLimit,
			    b - b % 6u, b);
		diff_eq_int("blob: +0x0c is argument 1 down to a twelve (%ld)",
			    (unsigned)((V90RDetector *)theirs)->rfLimit,
			    a - a % 12u, a);
		diff_eq_int("blob: +0x10 is argument 2 down to a twelve (%ld)",
			    (unsigned)((V90RDetector *)theirs)->rfNotLimit,
			    b - b % 12u, b);
		diff_eq_int("blob: the polarity starts at +1 (%ld)",
			    ((V90RDetector *)theirs)->polarity, 1, trial);
		diff_eq_int("ours: the polarity starts at +1 (%ld)",
			    ((V90RDetector *)ours)->polarity, 1, trial);
		diff_eq_int("blob: +0x28 is not reset's business (%ld)",
			    memcmp(theirs + 0x28, before + 0x28, 4) == 0, 1,
			    trial);
	}

	/*
	 * Then each detector over a repeated pattern, COMPARED AFTER EVERY
	 * SAMPLE.  Every one of them answers 0 five times in six and does its
	 * work on the sixth, so comparing at the end of a run would compare
	 * two objects that had just been cleared.
	 */
	for (which = 0; which < 4; which++) {
		int longGroup = which >= 2;
		int npat = longGroup ? RD_NPAT12 : RD_NPAT6;
		int bits = longGroup ? 12 : 6;

		for (trial = 0; trial < npat * 6; trial++) {
			unsigned pat = longGroup
				? rd_pat12[trial % RD_NPAT12]
				: rd_pat6[trial % RD_NPAT6];
			unsigned limR = rd_counts[(trial / npat) % 6 + 4];
			unsigned limNot = rd_counts[(trial / npat) % 5 + 6];
			int polarity = (trial % 3) - 1;
			int step;

			/*
			 * THE TWO LIMITS ARE DIFFERENT, and that is not
			 * decoration.  `detectR` measures against +0x04 and
			 * `detectRNot` against +0x08; reset fills them from
			 * its two arguments, so passing one count twice makes
			 * the two fields equal and a detector reading the
			 * wrong one indistinguishable from one reading the
			 * right one.  Same for +0x0c against +0x10.
			 *
			 * AND THE POLARITY TAKES THREE VALUES, including 0:
			 * the `Not` detectors choose their pattern with a
			 * signed `> 0`, so a reconstruction using `>= 0` or
			 * `!= 0` differs on exactly that value and on no
			 * other.
			 */
			seed(slot, trial + 500);
			our_rd_reset(ours, limR, limNot);
			ref_rd_reset(theirs, limR, limNot);

			/*
			 * The two `Not` detectors choose their pattern from
			 * the polarity, so it is set from OUTSIDE on both
			 * sides: reaching -1 through detectR first would test
			 * one arm of the choice and never the other.
			 */
			((V90RDetector *)ours)->polarity = polarity;
			((V90RDetector *)theirs)->polarity = polarity;

			for (step = 0; step < bits * 8; step++) {
				/*
				 * THE PATTERN CHANGES EVERY THIRD GROUP.  A
				 * run of one pattern can never show what a
				 * non-matching group does to a run counter
				 * that is already up, and both the "clear
				 * both" branch and the "clear the Not run"
				 * branch are only visible against a counter
				 * with something in it.
				 */
				unsigned live = ((step / bits) % 3) == 2
					? (longGroup
					   ? rd_pat12[(trial + 1) % RD_NPAT12]
					   : rd_pat6[(trial + 1) % RD_NPAT6])
					: pat;
				int b = (int)((live
					       >> (bits - 1 - step % bits))
					      & 1u);
				short v = rd_sample(b, step);
				int ra, rb;

				memcpy(before, ours, (size_t)slot);
				switch (which) {
				case 0:
					ra = our_rd_detR(ours, v);
					rb = ref_rd_detR(theirs, v);
					break;
				case 1:
					ra = our_rd_detRNot(ours, v);
					rb = ref_rd_detRNot(theirs, v);
					break;
				case 2:
					ra = our_rd_detRf(ours, v);
					rb = ref_rd_detRf(theirs, v);
					break;
				default:
					ra = our_rd_detRfNot(ours, v);
					rb = ref_rd_detRfNot(theirs, v);
					break;
				}

				diff_eq_int("the verdict (sample %ld)", ra, rb,
					    step);
				diff_eq_obj("after a sample", V90RDetector,
					    ours, theirs, step);
				diff_eq_int("nothing past the object"
					    " (sample %ld)",
					    guard_intact(obj, slot), 1, step);

				if (rb == 0)
					seen0++;
				if (rb == 1
				    && ((V90RDetector *)theirs)->polarity > 0)
					seenPos++;
				if (rb == 1
				    && ((V90RDetector *)theirs)->polarity < 0)
					seenNeg++;
				if (rb == -1)
					seenNot++;
				if (((step + 1) % bits) == 0
				    && ((V90RDetector *)theirs)->positiveRunLength == 0
				    && ((V90RDetector *)theirs)->negativeRunLength == 0)
					seenClear++;
			}
		}
	}

	/*
	 * Every outcome, or the sweep proves only that two objects agree
	 * about saying no (findings F149 and F223).
	 */
	diff_eq_int("a group answered 0 on %ld samples", seen0 > 0, 1, seen0);
	diff_eq_int("a run reached its limit positive %ld times",
		    seenPos > 0, 1, seenPos);
	diff_eq_int("a run reached its limit negative %ld times",
		    seenNeg > 0, 1, seenNeg);
	diff_eq_int("a Not detector fired %ld times", seenNot > 0, 1, seenNot);
	diff_eq_int("a group matched neither pattern %ld times",
		    seenClear > 0, 1, seenClear);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_cd();
	rc |= run_trn2();
	rc |= run_rd();
	rc |= run_rd_detect();
	rc |= run_cp();
	rc |= run_adid();

	return rc;
}
