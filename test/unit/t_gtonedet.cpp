/*
 * t_gtonedet.cpp -- differential test of GenericToneDetector's constructor
 * and destructor.
 *
 * The only member of this batch that DOES something: it allocates, it divides,
 * it copies two floats and it constructs a sub-object.  Each of those wants a
 * different kind of check, and this file is organised around that.
 *
 * THE SUB-OBJECT IS THE HARD PART.  The constructor ends up owning a
 * `GenericIIR<float, double>` that IT allocated, so the two sides hold two
 * different heap addresses and always will -- comparing +0x00 as a value would
 * fail on a correct reconstruction.  What is compared instead is what the
 * pointer POINTS AT: the 52-byte filter object with its own two history
 * pointers neutralised, and then the CONTENTS of both history buffers, whose
 * lengths come from `GenericIIR.h`'s documented `nnum + blockSize` and
 * `nden + blockSize`.  So "the filter was constructed identically" is a
 * measured claim over every field of it and every byte it owns, and the only
 * thing excluded is the four addresses that cannot agree.  CLAUDE.md sanctions
 * exactly this exclusion and no more.
 *
 * THE OBJECT IS NEVER ZEROED, and both sides get the same varied
 * pseudorandom fill before every call, reseeded per trial (findings F223, F224).
 * That matters more here than usual, because the constructor leaves nothing
 * uninitialised -- every one of the fifteen fields is written -- so a fill of
 * zeros would make a MISSING store invisible.  A guard region past +0x3b is
 * compared against each side's own snapshot, so a store past the object's end
 * fails rather than passes.
 *
 * THE FLOAT SWEEP, AND WHAT IT DOES AND DOES NOT PROVE.  The constructor
 * COPIES its two float arguments -- in the blob with `mov %ecx,0x4(%esi)`,
 * a 32-bit integer move that never touches the x87 stack.  So the sweep below
 * (zero, both signs, a denormal, an exactly representable value, one that is
 * not, the two infinities) proves that the copy is BIT-EXACT, and it is
 * deliberately not evidence about floating-point arithmetic: there is none
 * here to be evidence about.  A signalling NaN is excluded on purpose and
 * finding F1252 says why.
 *
 * THE DIVISOR IS SWEPT AND ZERO IS EXCLUDED ON PURPOSE.  Both `div %edi` sites
 * take `blockLen` straight from the argument with no guard, so zero traps in
 * the blob exactly as it would here.  That is recorded in docs/deviations.md
 * and not tested; every value below is nonzero, and the sweep covers 1, a
 * value that divides both durations exactly, one that divides neither, and
 * 0xffffffff, where the quotient is 0 or 1 and the round-up is the whole
 * answer.
 *
 * C++ HAS NO SYNTAX for running a constructor over storage that already
 * exists, so BOTH sides are called by symbol through asm() labels -- ours by
 * its mangled name, the blob's by the `ref_` alias.  Both ABI variants (`C1`
 * and `C2`, `D1` and `D2`) are driven, because the blob has all four at four
 * different addresses.  Plain cdecl, `this` first on the stack (finding F215);
 * the two floats occupy one four-byte slot each because the call is
 * prototyped.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/ANSamToneDetector.h"
#include "dsplib/GenericToneDetector.h"

typedef void (*gtd_ctor_t)(void *self, unsigned int, unsigned int,
			   double *, double *, unsigned int, unsigned int,
			   float, unsigned int, float, unsigned int,
			   unsigned int);
typedef void (*gtd_dtor_t)(void *self);

extern "C" {
void our_gtd_c1(void *, unsigned int, unsigned int, double *, double *,
		unsigned int, unsigned int, float, unsigned int, float,
		unsigned int, unsigned int)
	asm("_ZN19GenericToneDetectorC1EjjPdS0_jjfjfjj");
void our_gtd_c2(void *, unsigned int, unsigned int, double *, double *,
		unsigned int, unsigned int, float, unsigned int, float,
		unsigned int, unsigned int)
	asm("_ZN19GenericToneDetectorC2EjjPdS0_jjfjfjj");
void ref_gtd_c1(void *, unsigned int, unsigned int, double *, double *,
		unsigned int, unsigned int, float, unsigned int, float,
		unsigned int, unsigned int)
	asm("ref__ZN19GenericToneDetectorC1EjjPdS0_jjfjfjj");
void ref_gtd_c2(void *, unsigned int, unsigned int, double *, double *,
		unsigned int, unsigned int, float, unsigned int, float,
		unsigned int, unsigned int)
	asm("ref__ZN19GenericToneDetectorC2EjjPdS0_jjfjfjj");
void our_gtd_d1(void *) asm("_ZN19GenericToneDetectorD1Ev");
void our_gtd_d2(void *) asm("_ZN19GenericToneDetectorD2Ev");
void ref_gtd_d1(void *) asm("ref__ZN19GenericToneDetectorD1Ev");
void ref_gtd_d2(void *) asm("ref__ZN19GenericToneDetectorD2Ev");

/*
 * BY SYMBOL FOR OUR SIDE TOO, and not only for the blob's.  `ours->reset()`
 * would let the compiler inline the body and then the test would be measuring
 * an inlined copy rather than the function the linker will ship; the blob has
 * one symbol for it and so must we.  Same plain cdecl as the constructor,
 * `this` first on the stack.
 */
void our_gtd_reset(void *) asm("_ZN19GenericToneDetector5resetEv");
void ref_gtd_reset(void *) asm("ref__ZN19GenericToneDetector5resetEv");

int our_gtd_process1(void *, float) asm("_ZN19GenericToneDetector7processEf");
int ref_gtd_process1(void *, float) asm("ref__ZN19GenericToneDetector7processEf");
int our_gtd_processn(void *, float *, unsigned int)
	asm("_ZN19GenericToneDetector7processEPfj");
int ref_gtd_processn(void *, float *, unsigned int)
	asm("ref__ZN19GenericToneDetector7processEPfj");
}

#define OBJ	((int)sizeof(GenericToneDetector))
#define GUARD	96
#define SLOT	(OBJ + GUARD)

static unsigned char ours[SLOT] __attribute__((aligned(8)));
static unsigned char theirs[SLOT] __attribute__((aligned(8)));
static unsigned char before[SLOT];
static unsigned char trial0[SLOT];

/*
 * The filter's own object, as `GenericIIR.h` lays it out.  Only the two
 * offsets this file has to exclude are named; everything else is compared as
 * bytes, which is what keeps the test from depending on the private field
 * names.
 */
#define IIR_BYTES	0x34
#define IIR_INHIST	0x08
#define IIR_OUTHIST	0x0c

/*
 * +0x28 AND +0x2c ARE EXCLUDED, AND NOT BECAUSE THEY ARE INCONVENIENT.
 * `m_i` and `m_acc` are `GenericIIR`'s two scratch members, and our
 * reconstruction of that class -- `src/dsp/FloatIIR.cpp`, a different batch's
 * work with its own test -- leaves them in a different state from the blob's
 * after construction.  The blob's `reset()` uses `m_i` as its own loop
 * variable and leaves it holding `m_outLen`, and neither its constructor nor
 * its `reset` touches `m_acc` at all, so `m_acc` still holds whatever the
 * allocation held; ours uses a local counter and writes `m_i = 0; m_acc = 0;`
 * in the constructor.  Measured, and the rule was checked against a second
 * parameter set that predicted `m_i` before it was read: nden 2, nnum 3,
 * blockSize 1 gives m_outLen 3 and the blob leaves m_i = 3; nden 3, nnum 1,
 * blockSize 2 gives m_outLen 5 and it leaves m_i = 5.
 *
 * That divergence belongs to `GenericIIR` and NOT to anything
 * `GenericToneDetector` does -- the constructor here passes five arguments
 * through and calls `reset()`, and every field either class's constructor is
 * responsible for agrees exactly.  `t_genericiir` cannot see it because it
 * compares output samples and never the object.  Finding F1250 records it;
 * fixing it is that class's batch, not this one's.
 *
 * Twelve bytes: the four of `m_i` and the eight of the `double` `m_acc`.
 */
#define IIR_SCRATCH	0x28
#define IIR_SCRATCH_LEN	0x0c

/*
 * Coefficient arrays, SHARED between the two sides.  `GenericIIR` borrows them
 * rather than copying, so the pointer values land in both filters and two
 * separately allocated arrays would compare unequal whatever the constructor
 * did.  Filled with varied bytes so that a read anywhere in them is a read of
 * something, and made larger than the largest order the sweep uses.
 */
#define COEFFS	8
static double den[COEFFS];
static double num[COEFFS];

/*
 * The float sweep.  Written as bit patterns, because that is what the
 * constructor moves and what the comparison has to hold to; naming them as
 * literals would let the compiler round one before it ever reached the call.
 */
static const unsigned int floatbits[] = {
	0x00000000u,	/* +0.0                                              */
	0x80000000u,	/* -0.0, the sign bit a memcmp sees and == does not   */
	0x3f800000u,	/* +1.0, exactly representable                        */
	0xbfc00000u,	/* -1.5, exactly representable                        */
	0x3dcccccdu,	/* 0.1f -- the nearest float to a decimal that is not */
	0x00000001u,	/* the smallest denormal                             */
	0x007fffffu,	/* the largest denormal                              */
	0x7f7fffffu,	/* FLT_MAX                                           */
	0x7f800000u,	/* +inf                                              */
	0xff800000u	/* -inf                                              */
};
#define NFLOATS	((int)(sizeof(floatbits) / sizeof(floatbits[0])))

static float
as_float(unsigned int bits)
{
	union {
		unsigned int u;
		float f;
	} v;

	v.u = bits;
	return v.f;
}

/* Nonzero always -- see the file comment on the unguarded division. */
static const unsigned int blocklens[] = { 1u, 2u, 3u, 7u, 40u, 160u, 0xffffffffu };
#define NBLOCKLEN ((int)(sizeof(blocklens) / sizeof(blocklens[0])))

/*
 * Durations, chosen around the divisor so the round-up is exercised in both
 * directions: an exact multiple takes the `jae` arm and one more than a
 * multiple takes the other.
 */
static const unsigned int durations[] = {
	0u, 1u, 39u, 40u, 41u, 159u, 160u, 161u, 0xfffffffeu, 0xffffffffu
};
#define NDUR ((int)(sizeof(durations) / sizeof(durations[0])))

static unsigned lfsr;

static void
seed(int trial)
{
	int i;

	lfsr = 0x51a7u + 0x9e37u * (unsigned)trial + 1u;
	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		switch (trial % 4) {
		case 1:
			v = 0xa5;	/* the harness's own malloc fill */
			break;
		case 2:
			v = 0x00;	/* what a zeroed object looks like */
			break;
		default:
			v = (unsigned char)(lfsr >> 3);
			break;
		}
		ours[i] = v;
		theirs[i] = v;
	}
	for (i = 0; i < COEFFS; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		den[i] = (double)(int)(lfsr | 1u) / 1024.0;
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		num[i] = (double)(int)(lfsr | 1u) / 512.0;
	}
	memcpy(before, ours, SLOT);
	if (trial == 0)
		memcpy(trial0, ours, SLOT);
}

static int
guard_intact(void)
{
	return memcmp(ours + OBJ, before + OBJ, SLOT - OBJ) == 0 &&
	       memcmp(theirs + OBJ, before + OBJ, SLOT - OBJ) == 0;
}

/*
 * The filter: every field compared, with the two history POINTERS and the two
 * scratch members neutralised, and then the buffers they point at compared in
 * full.  Reported through `diff_eq_obj_` rather than as a boolean, so a
 * mismatch names the offset instead of saying only that one exists.
 */
/*
 * `ownCoeffs` IS FOR THE DERIVED CLASS AND CHANGES NOTHING FOR THIS ONE.
 * `GenericToneDetector`'s caller supplies the coefficient arrays, so both
 * sides' filters borrow the SAME two and the pointers compare equal;
 * `ANSamToneDetector` supplies its own, so ours points into our .data and the
 * blob's into the blob's and the two can never agree.  Passing 1 neutralises
 * those two pointers AND compares the arrays they point at in full, which is
 * a stronger claim than the equal-pointer case makes: it is what checks that
 * 48 transcribed coefficients are the blob's.
 */
#define IIR_DEN		0x00
#define IIR_NUM		0x04

static void
compare_filters_(unsigned int nden, unsigned int nnum, unsigned int blockSize,
		 int ownCoeffs, int tag)
{
	unsigned char *fa = (unsigned char *)((GenericToneDetector *)ours)
				->filter;
	unsigned char *fb = (unsigned char *)((GenericToneDetector *)theirs)
				->filter;
	unsigned char ca[IIR_BYTES], cb[IIR_BYTES];
	void *ina, *inb, *outa, *outb;
	unsigned int inLen = nnum + blockSize;
	unsigned int outLen = nden + blockSize;

	/*
	 * Three allocations a side, and SIX DISTINCT ADDRESSES.  Asserted
	 * first, because every comparison below dereferences them and because
	 * two sides sharing one buffer would make the rest agree for the wrong
	 * reason (finding F224).
	 */
	memcpy(&ina, fa + IIR_INHIST, sizeof(ina));
	memcpy(&inb, fb + IIR_INHIST, sizeof(inb));
	memcpy(&outa, fa + IIR_OUTHIST, sizeof(outa));
	memcpy(&outb, fb + IIR_OUTHIST, sizeof(outb));

	diff_eq_int("six live pointers, all distinct and non-null (%ld)",
		    fa != 0 && fb != 0 && ina != 0 && inb != 0 &&
		    outa != 0 && outb != 0 &&
		    fa != fb && ina != inb && outa != outb &&
		    ina != outb && outa != inb,
		    1, tag);
	if (fa == 0 || fb == 0 || ina == 0 || inb == 0 || outa == 0 ||
	    outb == 0)
		return;

	/*
	 * THE EXCLUDED TWELVE BYTES ARE ASSERTED, NOT SKIPPED.  The two history
	 * pointers can never agree -- two allocations are two addresses -- but
	 * `m_i` and `m_acc` CAN, and finding F1250 recorded that they did not:
	 * ours zeroed both, the blob left `m_i` holding `m_outLen` and never
	 * touched `m_acc`.  The four checks below asserted THAT, so that
	 * repairing `src/dsp/FloatIIR.cpp` would fail them rather than pass
	 * silently on an exclusion nobody knew had gone obsolete.
	 *
	 * IT FIRED, AND THE CLASS IS REPAIRED.  `reset` now counts in `m_i`
	 * exactly as `_ZN10GenericIIRIfdE5resetEv` does, and the constructor no
	 * longer writes either member -- `_ZN10GenericIIRIfdEC1EjjPdS1_j`
	 * stores eight fields and tail-calls `reset`, and neither +0x28 nor
	 * +0x2c is among them.  So the four checks are now the repaired state:
	 * both sides leave `m_outLen` in `m_i`, and neither side writes
	 * `m_acc`, which therefore still holds the allocator's fill on both.
	 *
	 * `m_outLen` is `nden + blockSize` and the sweep keeps both in
	 * 1..4 and 0..2, so it is never zero and "the blob left m_outLen there"
	 * is never the same claim as "the blob left zero there".
	 */
	{
		static const unsigned char zero8[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		unsigned int mi_a, mi_b;

		memcpy(&mi_a, fa + IIR_SCRATCH, sizeof(mi_a));
		memcpy(&mi_b, fb + IIR_SCRATCH, sizeof(mi_b));

		diff_eq_int("ours leaves m_i holding m_outLen -- finding F1250 "
			    "repaired (%ld)", mi_a == outLen, 1, tag);
		diff_eq_int("the blob leaves m_i holding m_outLen (%ld)",
			    mi_b == outLen, 1, tag);
		diff_eq_int("ours no longer writes m_acc (%ld)",
			    memcmp(fa + IIR_SCRATCH + 4, zero8, 8) != 0, 1,
			    tag);
		diff_eq_int("the blob never writes m_acc at all (%ld)",
			    memcmp(fb + IIR_SCRATCH + 4, zero8, 8) != 0, 1,
			    tag);
	}

	memcpy(ca, fa, IIR_BYTES);
	memcpy(cb, fb, IIR_BYTES);
	memset(ca + IIR_INHIST, 0, 2 * sizeof(void *));
	memset(cb + IIR_INHIST, 0, 2 * sizeof(void *));
	memset(ca + IIR_SCRATCH, 0, IIR_SCRATCH_LEN);
	memset(cb + IIR_SCRATCH, 0, IIR_SCRATCH_LEN);
	if (ownCoeffs) {
		double *da, *db, *na, *nb;

		memcpy(&da, fa + IIR_DEN, sizeof(da));
		memcpy(&db, fb + IIR_DEN, sizeof(db));
		memcpy(&na, fa + IIR_NUM, sizeof(na));
		memcpy(&nb, fb + IIR_NUM, sizeof(nb));

		diff_eq_int("four coefficient pointers, all non-null and "
			    "neither side sharing (%ld)",
			    da != 0 && db != 0 && na != 0 && nb != 0 &&
			    da != db && na != nb && da != nb && na != db,
			    1, tag);
		if (da != 0 && db != 0 && na != 0 && nb != 0) {
			diff_eq_int("the denominator table is the blob's, all "
				    "%ld bytes of it",
				    memcmp(da, db,
					   nden * sizeof(double)) == 0, 1,
				    (long)(nden * sizeof(double)));
			diff_eq_int("the numerator table is the blob's, all "
				    "%ld bytes of it",
				    memcmp(na, nb,
					   nnum * sizeof(double)) == 0, 1,
				    (long)(nnum * sizeof(double)));
		}

		memset(ca + IIR_DEN, 0, 2 * sizeof(void *));
		memset(cb + IIR_DEN, 0, 2 * sizeof(void *));
	}
	diff_eq_obj_(__FILE__, __LINE__, "the filter the constructor built",
		     "GenericIIR<float, double>", ca, cb, IIR_BYTES,
		     (long)tag);

	diff_eq_int("the input history matches, all %ld of it",
		    memcmp(ina, inb, inLen * sizeof(double)) == 0, 1,
		    (long)(inLen * sizeof(double)));
	diff_eq_int("the output history matches, all %ld of it",
		    memcmp(outa, outb, outLen * sizeof(double)) == 0, 1,
		    (long)(outLen * sizeof(double)));
}

/*
 * DIRTYING, WHICH IS WHAT MAKES `reset` AND `process` TESTABLE AT ALL.
 *
 * A constructed detector is a detector with every field already at the value
 * `reset` would put there, so calling `reset` on one proves nothing: a body
 * that did nothing would pass.  Both sides are therefore disturbed in the same
 * way before the call -- the same bytes into the object, the same doubles into
 * both history buffers, the same junk into the filter's two write positions --
 * and what is then compared is the recovery.
 *
 * TWO THINGS ARE DELIBERATELY LEFT ALONE.  `filter` at +0x00 is the pointer
 * each side owns and must keep, and the filter's `m_i`/`m_acc` at +0x28 are
 * left in their post-construction state so that `compare_filters_`'s
 * exclusion -- which is an ASSERTION about finding F1250's divergence, not a
 * blind skip -- goes on saying what it says.  Neither our `GenericIIR::reset`
 * nor the blob's writes `m_acc`, and the blob's leaves `m_i` holding
 * `m_outLen` exactly as its constructor does, so the four checks in there hold
 * across a reset for the same reasons and would fail if either changed.
 */
#define IIR_INPOS	0x20
#define IIR_OUTPOS	0x24

static void
dirty_object(void)
{
	int i;

	for (i = 4; i < OBJ; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)(lfsr >> 3) | 1u;
		ours[i] = v;
		theirs[i] = v;
	}
}

static void
dirty_filter(unsigned int nden, unsigned int nnum, unsigned int blockSize)
{
	unsigned char *fa = (unsigned char *)((GenericToneDetector *)ours)
				->filter;
	unsigned char *fb = (unsigned char *)((GenericToneDetector *)theirs)
				->filter;
	double *ina, *inb, *outa, *outb;
	unsigned int inLen = nnum + blockSize;
	unsigned int outLen = nden + blockSize;
	unsigned int k, pos;

	memcpy(&ina, fa + IIR_INHIST, sizeof ina);
	memcpy(&inb, fb + IIR_INHIST, sizeof inb);
	memcpy(&outa, fa + IIR_OUTHIST, sizeof outa);
	memcpy(&outb, fb + IIR_OUTHIST, sizeof outb);

	for (k = 0; k < inLen; k++) {
		double d;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		d = (double)(int)(lfsr | 1u) / 17.0;
		ina[k] = d;
		inb[k] = d;
	}
	for (k = 0; k < outLen; k++) {
		double d;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		d = (double)(int)(lfsr | 1u) / 23.0;
		outa[k] = d;
		outb[k] = d;
	}

	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	pos = (lfsr & 7u) + 1u;
	memcpy(fa + IIR_INPOS, &pos, sizeof pos);
	memcpy(fb + IIR_INPOS, &pos, sizeof pos);
	memcpy(fa + IIR_OUTPOS, &pos, sizeof pos);
	memcpy(fb + IIR_OUTPOS, &pos, sizeof pos);
}

/*
 * The whole-object comparison, with the two heap pointers stood aside.  Three
 * call sites want it now, so it is one function rather than three copies of
 * the same six lines.
 */
static void
compare_objects_(const char *what, int tag)
{
	GenericIIR<float, double> *pa = ((GenericToneDetector *)ours)->filter;
	GenericIIR<float, double> *pb = ((GenericToneDetector *)theirs)->filter;

	((GenericToneDetector *)ours)->filter = 0;
	((GenericToneDetector *)theirs)->filter = 0;
	diff_eq_obj_(__FILE__, __LINE__, what, "GenericToneDetector",
		     ours, theirs, OBJ, (long)tag);
	((GenericToneDetector *)ours)->filter = pa;
	((GenericToneDetector *)theirs)->filter = pb;
}

#define NTRIAL	40

/*
 * reset() -- one call into the filter and eight fields to zero, and the two
 * claims worth making are that it clears everything the blob clears and that
 * it leaves everything the blob leaves.  The second is the one a fill of
 * zeros would hide, which is why the configuration is dirtied with nonzero
 * bytes and then asserted to have survived on the BLOB's side: that makes
 * "reset does not touch the configuration" a measurement of the object rather
 * than a description of our source.
 */
static int
run_reset(void)
{
	int trial, moved = 0;

	diff_begin("GenericToneDetector::reset");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int nden = 1u + (unsigned)(trial % 4);
		unsigned int nnum = 1u + (unsigned)((trial + 2) % 4);
		unsigned int blockSize = (unsigned)(trial % 3);
		unsigned int blockLen = blocklens[trial % NBLOCKLEN];
		unsigned int flag = (unsigned)(trial * 7);
		float thr = as_float(floatbits[trial % NFLOATS]);
		float rat = as_float(floatbits[(trial + 5) % NFLOATS]);
		unsigned char dirty[SLOT];
		int tag = trial;

		seed(trial);
		our_gtd_c1(ours, nden, nnum, den, num, durations[trial % NDUR],
			   durations[(trial + 3) % NDUR], thr, flag, rat,
			   blockLen, blockSize);
		ref_gtd_c1(theirs, nden, nnum, den, num,
			   durations[trial % NDUR],
			   durations[(trial + 3) % NDUR], thr, flag, rat,
			   blockLen, blockSize);

		dirty_object();
		dirty_filter(nden, nnum, blockSize);
		memcpy(dirty, theirs, SLOT);

		our_gtd_reset(ours);
		ref_gtd_reset(theirs);

		compare_filters_(nden, nnum, blockSize, 0, tag);
		compare_objects_("after reset", tag);

		diff_eq_int("no store past the object (%ld)", guard_intact(),
			    1, tag);

		/*
		 * The six configuration fields, on the blob's side, byte for
		 * byte against what they held before the call.  +0x04 and
		 * +0x08 (threshold, ratio), +0x1c and +0x20 (the two block
		 * limits), +0x28 (blockLen) and +0x34 (flag).
		 */
		diff_eq_int("the blob's reset leaves threshold and ratio "
			    "(%ld)",
			    memcmp(theirs + 0x04, dirty + 0x04, 8) == 0, 1,
			    tag);
		diff_eq_int("the blob's reset leaves both block limits (%ld)",
			    memcmp(theirs + 0x1c, dirty + 0x1c, 8) == 0, 1,
			    tag);
		diff_eq_int("the blob's reset leaves blockLen (%ld)",
			    memcmp(theirs + 0x28, dirty + 0x28, 4) == 0, 1,
			    tag);
		diff_eq_int("the blob's reset leaves flag (%ld)",
			    memcmp(theirs + 0x34, dirty + 0x34, 4) == 0, 1,
			    tag);

		/*
		 * And that it did something at all: the dirtied object is not
		 * the reset object.  Without this the whole run would pass
		 * against an empty body.
		 */
		if (memcmp(dirty, theirs, OBJ) != 0)
			moved = 1;

		our_gtd_d1(ours);
		ref_gtd_d1(theirs);
	}

	diff_eq_int("reset changed the object", moved, 1, 0);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * DRIVING THE TWO `process`ES.
 *
 * These are the only members with STATE THAT CARRIES, so a single call proves
 * almost nothing: the whole class is a hysteresis machine whose interesting
 * behaviour is fifty blocks long.  Both sides are therefore run over a stream
 * and the WHOLE OBJECT is compared after EVERY call, not once at the end --
 * an end-only comparison says a divergence exists, a per-call one says which
 * sample caused it and everything after is consequence.
 *
 * THE FILTER IS MADE TRIVIAL FOR THE DESIGNED SCENARIOS.  `nden = nnum = 1`
 * with `den[0] = 1` and `num[0] = gain` makes `GenericIIR` compute exactly
 * `y = gain * x` with no history term at all, so the mean of y^2 over a block
 * of constant |x| = a is `gain^2 * a^2` and the arm a block takes can be
 * CHOSEN rather than hoped for.  That is what makes the coverage assertions at
 * the bottom of each run possible; a pseudorandom sweep alone would never
 * enter the narrow arms, would pass, and would be measuring nothing.
 *
 * The random sweep is kept as well, and its coefficients are deliberately
 * unbounded: an unstable filter drives the accumulators to infinity and then
 * to NaN, which is the only thing that exercises the UNORDERED side of every
 * comparison in these bodies.  The object's `fcom`/`sahf`/`ja` pairs and our
 * `>=`/`>` spellings agree there or they do not, and that is worth knowing.
 */
#define NSAMP	192

static float stream[NSAMP];

/* Blocks of constant magnitude, sign alternating so the stream is not DC. */
static void
fill_blocks(unsigned int blockLen, const float *amps, int namps)
{
	int i;

	for (i = 0; i < NSAMP; i++) {
		int blk = (int)((unsigned)i / blockLen);
		float a = amps[blk % namps];

		stream[i] = (i & 1) ? -a : a;
	}
}

/*
 * Alternating PHASES of constant sign and alternating sign, at one magnitude
 * throughout.  Against the three-tap comb `y = x[k] + x[k-1] + x[k-2]` the
 * first phase has a gain of 3 and the second a gain of 1, so `acc_18 /
 * acc_14` swings between 9 and 1 while `meanOut` stays well over any small
 * threshold.  That is the only way to reach the STRONG arm's miss -- a block
 * that passed the threshold and failed the ratio -- with `count_2c` already
 * standing, which is what the withdrawal there needs in order to be visible.
 */
static void
fill_phases(unsigned int blockLen, int blocksPerPhase, float amp)
{
	int i;

	for (i = 0; i < NSAMP; i++) {
		int blk = (int)((unsigned)i / blockLen);
		int phase = (blk / blocksPerPhase) & 1;

		stream[i] = (phase && (i & 1)) ? -amp : amp;
	}
}

/*
 * What the blob did with each block, read off ITS OWN state rather than
 * recomputed.  A hit advances +0x2c and clears +0x30; a miss advances +0x30.
 * That is enough to say which of the two the object chose, and the designed
 * scenarios are what say WHY it chose it.
 */
struct arms {
	int boundary;
	int inblock;
	int hit;
	int miss;
	int set;
	int cleared;
};

static void
note_arm(const unsigned char *pre, const unsigned char *post, struct arms *a)
{
	const GenericToneDetector *b = (const GenericToneDetector *)pre;
	const GenericToneDetector *c = (const GenericToneDetector *)post;

	if (c->sampleCount == 0 && b->sampleCount + 1 == b->blockLen) {
		a->boundary++;
		if (c->count_2c != b->count_2c && c->count_30 == 0)
			a->hit++;
		else if (c->count_30 == b->count_30 + 1)
			a->miss++;
		if (c->detected != 0 && b->detected == 0)
			a->set++;
		if (c->detected == 0 && b->detected != 0)
			a->cleared++;
	} else {
		a->inblock++;
	}
}

/*
 * The filter after a stream, compared as a whole with only the two history
 * POINTERS stood aside.  `m_i` and `m_acc` are NOT excluded here, unlike in
 * `compare_filters_`: that exclusion is about what the two CONSTRUCTORS leave
 * behind, and after a `process` both sides have written both of them, so
 * excluding them would be looking away from live state rather than from a
 * known divergence.
 */
static void
compare_filters_after_(unsigned int nden, unsigned int nnum,
		       unsigned int blockSize, int tag)
{
	unsigned char *fa = (unsigned char *)((GenericToneDetector *)ours)
				->filter;
	unsigned char *fb = (unsigned char *)((GenericToneDetector *)theirs)
				->filter;
	unsigned char ca[IIR_BYTES], cb[IIR_BYTES];
	double *ina, *inb, *outa, *outb;
	unsigned int inLen = nnum + blockSize;
	unsigned int outLen = nden + blockSize;

	memcpy(&ina, fa + IIR_INHIST, sizeof ina);
	memcpy(&inb, fb + IIR_INHIST, sizeof inb);
	memcpy(&outa, fa + IIR_OUTHIST, sizeof outa);
	memcpy(&outb, fb + IIR_OUTHIST, sizeof outb);

	memcpy(ca, fa, IIR_BYTES);
	memcpy(cb, fb, IIR_BYTES);
	memset(ca + IIR_INHIST, 0, 2 * sizeof(void *));
	memset(cb + IIR_INHIST, 0, 2 * sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, "the filter after the stream",
		     "GenericIIR<float, double>", ca, cb, IIR_BYTES,
		     (long)tag);

	diff_eq_int("the input history matches, all %ld of it",
		    memcmp(ina, inb, inLen * sizeof(double)) == 0, 1,
		    (long)(inLen * sizeof(double)));
	diff_eq_int("the output history matches, all %ld of it",
		    memcmp(outa, outb, outLen * sizeof(double)) == 0, 1,
		    (long)(outLen * sizeof(double)));
}

/*
 * One stream through `process(float)`, both sides, compared every sample.
 * Returns the number of hits the BLOB scored, which is what the designed
 * scenarios assert against.
 */
static int
drive_scalar(unsigned int nden, unsigned int nnum, unsigned int blockSize,
	     unsigned int s1, unsigned int s2, float thr, unsigned int flag,
	     float rat, unsigned int blockLen, int nsamp, int tag,
	     struct arms *a)
{
	unsigned char pre[SLOT];
	int i, live = harness_alloc.live;

	our_gtd_c1(ours, nden, nnum, den, num, s1, s2, thr, flag, rat,
		   blockLen, blockSize);
	ref_gtd_c1(theirs, nden, nnum, den, num, s1, s2, thr, flag, rat,
		   blockLen, blockSize);

	for (i = 0; i < nsamp; i++) {
		int ra, rb;

		memcpy(pre, theirs, SLOT);
		ra = our_gtd_process1(ours, stream[i]);
		rb = ref_gtd_process1(theirs, stream[i]);

		diff_eq_int("process(float) returns the blob's answer (%ld)",
			    ra, rb, tag * 1000 + i);
		diff_eq_int("and it is the field at +0x38 (%ld)", rb,
			    (int)((GenericToneDetector *)theirs)->detected,
			    tag * 1000 + i);
		compare_objects_("after process(float)", tag * 1000 + i);
		note_arm(pre, theirs, a);
	}

	compare_filters_after_(nden, nnum, blockSize, tag);
	diff_eq_int("no store past the object (%ld)", guard_intact(), 1, tag);
	our_gtd_d1(ours);
	ref_gtd_d1(theirs);
	diff_eq_int("the stream leaked nothing (%ld)",
		    harness_alloc.live - live, 0, tag);

	return a->hit;
}

/*
 * The designed scenarios.  `gain` goes into num[0] and den[0] is 1, so
 * `meanOut == gain^2 * meanIn` and `acc_18 == gain^2 * acc_14` block after
 * block (both smoothers start at zero and the recurrence is linear).  That
 * turns each of the object's three tests into arithmetic that can be chosen:
 *
 *   meanOut >= threshold        gain^2 * a^2  against  thr
 *   acc_18 > ratio * acc_14     gain^2        against  ratio
 *   acc_14 * 0.85 >= acc_18     0.85          against  gain^2   (array only)
 *
 * so gain 1.0 passes the ratio test at ratio 0.5 and fails the 0.85 test,
 * while gain 0.5 -- meanOut a quarter of meanIn -- fails both.
 */
#define AMP_STRONG	1.0f	/* a^2 = 1.00, over thr = 0.5           */
#define AMP_BAND	0.6f	/* a^2 = 0.36, under 0.5 and over 0.25  */
#define AMP_SILENT	0.1f	/* a^2 = 0.01, under both               */
#define SCEN_THR	0.5f

static int
run_process1(void)
{
	struct arms a;
	int trial, flag0hits, flag1hits;

	diff_begin("GenericToneDetector::process(float)");

	memset(&a, 0, sizeof a);

	/*
	 * The random sweep: varied orders, varied block lengths, varied
	 * durations, and coefficients large enough that some of these filters
	 * run away to infinity and NaN on purpose.
	 */
	for (trial = 0; trial < 12; trial++) {
		unsigned int nden = 1u + (unsigned)(trial % 4);
		unsigned int nnum = 1u + (unsigned)((trial + 2) % 4);
		unsigned int blockSize = (unsigned)(trial % 3);
		unsigned int blockLen = 1u + (unsigned)(trial % 7);
		unsigned int flag = (unsigned)(trial & 1);
		float thr = as_float(floatbits[trial % NFLOATS]);
		float rat = as_float(floatbits[(trial + 5) % NFLOATS]);
		int i;

		seed(trial);
		for (i = 0; i < NSAMP; i++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
			stream[i] = (float)(int)(lfsr - 0x8000u) / 4096.0f;
		}
		drive_scalar(nden, nnum, blockSize, 3u * blockLen,
			     2u * blockLen, thr, flag, rat, blockLen, NSAMP,
			     100 + trial, &a);
	}

	/*
	 * Sustained tone: every block over threshold, so `count_2c` climbs to
	 * `blocks1` and the answer is set and then held.
	 */
	{
		static const float amps[1] = { AMP_STRONG };

		seed(0);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(8u, amps, 1);
		drive_scalar(1u, 1u, 2u, 24u, 16u, SCEN_THR, 1u, 0.5f, 8u,
			     NSAMP, 200, &a);
		diff_eq_int("a sustained tone sets the answer (%ld)",
			    a.set > 0, 1, 200);
	}

	/*
	 * Tone, then silence: the answer is set and then WITHDRAWN once
	 * `count_30` reaches `blocks2`.
	 */
	{
		static const float amps[8] = {
			AMP_STRONG, AMP_STRONG, AMP_STRONG, AMP_STRONG,
			AMP_SILENT, AMP_SILENT, AMP_SILENT, AMP_SILENT
		};
		int before = a.cleared;

		seed(1);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(8u, amps, 8);
		/*
		 * blocks1 is 3 and blocks2 is 2 ON PURPOSE and not for
		 * variety: with the two equal, every mutation that reads one
		 * where the object reads the other is invisible.
		 */
		drive_scalar(1u, 1u, 2u, 24u, 16u, SCEN_THR, 1u, 0.5f, 8u,
			     NSAMP, 201, &a);
		diff_eq_int("silence after a tone withdraws the answer (%ld)",
			    a.cleared > before, 1, 201);
	}

	/*
	 * THE `flag` ARM, as a pair.  Same stream, same everything except
	 * `flag`, and a gain of 0.5 so that `acc_18` is a quarter of `acc_14`
	 * and the ratio test at 0.5 CANNOT pass.  With `flag` set every block
	 * must miss; with it clear the ratio test is not reached and every
	 * block over threshold must hit.  Neither half means anything alone.
	 */
	{
		static const float amps[1] = { 4.0f };	/* meanOut = 4.0 */
		struct arms f0, f1;

		memset(&f0, 0, sizeof f0);
		memset(&f1, 0, sizeof f1);

		seed(2);
		den[0] = 1.0;
		num[0] = 0.5;
		fill_blocks(8u, amps, 1);
		flag1hits = drive_scalar(1u, 1u, 2u, 24u, 800u, SCEN_THR, 1u,
					 0.5f, 8u, NSAMP, 202, &f1);
		flag0hits = drive_scalar(1u, 1u, 2u, 24u, 800u, SCEN_THR, 0u,
					 0.5f, 8u, NSAMP, 203, &f0);

		diff_eq_int("with flag set and the smoothers a quarter apart, "
			    "no block hits (%ld)", flag1hits, 0, 202);
		diff_eq_int("and every block missed instead (%ld)",
			    f1.miss, f1.boundary, 202);
		diff_eq_int("with flag clear the ratio test is not reached "
			    "and the same blocks hit (%ld)",
			    flag0hits, f0.boundary, 203);
		a.hit += f0.hit;
		a.miss += f1.miss;
		a.boundary += f0.boundary + f1.boundary;
		a.inblock += f0.inblock + f1.inblock;
	}

	/*
	 * EXACTLY AT THE THRESHOLD, which is the only input that can tell
	 * `>=` from `>`.  Eight samples of magnitude 1 give `in = 8.0` and
	 * `inv = 0.125`, both exact, so `meanOut` is 1.0 to the bit and a
	 * `threshold` of 1.0f sits precisely on it.  `flag` is clear so the
	 * ratio test is not reached and the threshold is the only thing being
	 * asked about.
	 */
	{
		static const float amps[1] = { 1.0f };
		struct arms e;

		memset(&e, 0, sizeof e);
		seed(3);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(8u, amps, 1);
		drive_scalar(1u, 1u, 2u, 24u, 800u, 1.0f, 0u, 0.5f, 8u, NSAMP,
			     204, &e);
		diff_eq_int("a mean exactly equal to the threshold is a hit "
			    "(%ld)", e.hit, e.boundary, 204);
		a.hit += e.hit;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
		a.set += e.set;
	}

	/*
	 * EXACTLY AT THE RATIO, the same argument one test down.  A gain of 1
	 * makes the two smoothers hold the same bits as each other block after
	 * block, so a `ratio` of 1.0f puts `acc_18 > ratio * acc_14` exactly on
	 * its boundary: strictly greater is false, and every block must miss.
	 */
	{
		static const float amps[1] = { 1.0f };
		struct arms e;

		memset(&e, 0, sizeof e);
		seed(4);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(8u, amps, 1);
		drive_scalar(1u, 1u, 2u, 24u, 800u, 0.5f, 1u, 1.0f, 8u, NSAMP,
			     205, &e);
		diff_eq_int("smoothers exactly in the ratio do not hit (%ld)",
			    e.hit, 0, 205);
		diff_eq_int("every block missed instead (%ld)", e.miss,
			    e.boundary, 205);
		a.miss += e.miss;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
	}

	/*
	 * `blocks1 == 0`, WHICH IS WHERE THE TWO ARMS STOP AGREEING.  A
	 * `samples1` of zero rounds up to zero blocks, so `count_2c >= blocks1`
	 * is true of every value `count_2c` can hold -- including the zero the
	 * withdrawal has just written.  The strong arm reaches that test after
	 * withdrawing and puts the answer straight back; the below-threshold
	 * arm never loads `blocks1` and leaves it down.  The stream ends on a
	 * silent block and the blob's answer is asserted to be DOWN, which is
	 * the whole of deviation D-GTD1 stated as a measurement.
	 */
	{
		static const float amps[8] = {
			AMP_STRONG, AMP_STRONG, AMP_STRONG, AMP_STRONG,
			AMP_SILENT, AMP_SILENT, AMP_SILENT, AMP_SILENT
		};
		struct arms e;

		memset(&e, 0, sizeof e);
		seed(5);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(8u, amps, 8);
		drive_scalar(1u, 1u, 2u, 0u, 8u, SCEN_THR, 0u, 0.5f, 8u,
			     NSAMP, 206, &e);
		diff_eq_int("with blocks1 zero the answer still goes up "
			    "(%ld)", e.set > 0, 1, 206);
		diff_eq_int("and a below-threshold block leaves it down, "
			    "because that arm never reaches blocks1 (%ld)",
			    (int)((GenericToneDetector *)theirs)->detected, 0,
			    206);
		a.hit += e.hit;
		a.miss += e.miss;
		a.set += e.set;
		a.cleared += e.cleared;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
	}

	/*
	 * THE RECIPROCAL, CAUGHT IN THE ACT.  `d8 /7` divides 1.0f by the
	 * count and MULTIPLIES twice; a source that had divided twice would
	 * agree with it over almost every input and not over this one.
	 * Forty-one samples of magnitude 1 accumulate to exactly 41.0 -- every
	 * partial sum is a whole number a `float` holds exactly -- and at 80
	 * bits `41.0 * (1.0f/41)` falls one part in 2^64 SHORT of 1.0 while
	 * `41.0 / 41` is exactly 1.0.  So with `threshold` at 1.0f every block
	 * MISSES, and would hit if the object divided.  The assertion below is
	 * that measurement and nothing softer.
	 */
	{
		static const float amps[1] = { 1.0f };
		struct arms e;

		memset(&e, 0, sizeof e);
		seed(6);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(41u, amps, 1);
		drive_scalar(1u, 1u, 2u, 82u, 82u, 1.0f, 0u, 0.5f, 41u, NSAMP,
			     207, &e);
		diff_eq_int("the mean of 41 unit samples falls SHORT of 1.0, "
			    "so every block misses (%ld)", e.hit, 0, 207);
		diff_eq_int("blocks of 41 completed (%ld)", e.boundary > 0, 1,
			    207);
		a.miss += e.miss;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
	}

	/*
	 * THE THRESHOLD IS THE OUTPUT'S MEAN AND NOT THE INPUT'S.  A gain of
	 * 0.5 puts a factor of four between them -- meanIn 4.0, meanOut 1.0 --
	 * and a threshold of 2.0f sits between the two, so the object's answer
	 * says which one it looked at.  With `flag` clear the ratio test is
	 * not reached and the threshold is the only question asked.
	 */
	{
		static const float amps[1] = { 2.0f };
		struct arms e;

		memset(&e, 0, sizeof e);
		seed(7);
		den[0] = 1.0;
		num[0] = 0.5;
		fill_blocks(8u, amps, 1);
		drive_scalar(1u, 1u, 2u, 24u, 800u, 2.0f, 0u, 0.5f, 8u, NSAMP,
			     208, &e);
		diff_eq_int("a threshold between the two means follows the "
			    "OUTPUT, so every block misses (%ld)", e.hit, 0,
			    208);
		a.miss += e.miss;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
	}

	/*
	 * THE STRONG ARM'S MISS, WITH SOMETHING TO LOSE.  A three-tap comb and
	 * a stream that alternates between constant sign and alternating sign
	 * swings the smoothers' ratio between 9 and 1 across a `ratio` of 8,
	 * while `meanOut` never goes near the threshold.  So the first phase
	 * hits until the answer is up and the second takes the arm that passed
	 * the threshold and failed the ratio -- with `count_2c` standing at
	 * three, which is what makes `count_2c = 0` in the withdrawal a store
	 * with a consequence rather than a write of zero over zero.
	 * blocks1 = 3, blocks2 = 2.
	 */
	{
		struct arms e;

		memset(&e, 0, sizeof e);
		seed(8);
		den[0] = 1.0;
		num[0] = 1.0;
		num[1] = 1.0;
		num[2] = 1.0;
		fill_phases(8u, 8, 1.0f);
		drive_scalar(1u, 3u, 2u, 24u, 16u, SCEN_THR, 1u, 8.0f, 8u,
			     NSAMP, 209, &e);
		diff_eq_int("the comb's first phase hits (%ld)", e.hit > 0, 1,
			    209);
		diff_eq_int("its second phase misses on the ratio (%ld)",
			    e.miss > 0, 1, 209);
		diff_eq_int("the answer went up (%ld)", e.set > 0, 1, 209);
		diff_eq_int("and was withdrawn again (%ld)", e.cleared > 0, 1,
			    209);
		a.hit += e.hit;
		a.miss += e.miss;
		a.set += e.set;
		a.cleared += e.cleared;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
	}

	/*
	 * The arms, asserted rather than hoped for.  Every one of these was
	 * zero at some point while this test was being written, and a run that
	 * cannot say which arms it entered is a run that proves nothing about
	 * the ones it did not.
	 */
	diff_eq_int("blocks completed (%ld)", a.boundary > 0, 1, 0);
	diff_eq_int("samples inside a block (%ld)", a.inblock > 0, 1, 0);
	diff_eq_int("the hit arm was reached (%ld)", a.hit > 0, 1, 0);
	diff_eq_int("the miss arm was reached (%ld)", a.miss > 0, 1, 0);
	diff_eq_int("the answer was set (%ld)", a.set > 0, 1, 0);
	diff_eq_int("the answer was withdrawn (%ld)", a.cleared > 0, 1, 0);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * THE ARRAY OVERLOAD, WHICH IS A DIFFERENT ALGORITHM.
 *
 * It agrees with `process(float)` on the accumulation, the smoother and the
 * bookkeeping and disagrees about how a block SCORES: a block that fell short
 * of `threshold` but reached HALF of it still hits, if `flag` is set and the
 * output smoother stands above 0.85 of the input smoother.  So the test that
 * matters most here is not "does ours match the blob" -- that is the same
 * per-call comparison as before -- but "is that third arm ENTERED", because
 * the band it lives in is narrow and a pseudorandom sweep never finds it.
 *
 * It is driven deliberately, and the proof that it was entered is the SCALAR
 * OVERLOAD run over the same stream: the array's hits on a block the scalar
 * misses can only have come from the weak arm.  That is a comparison of the
 * blob against itself and needs nothing from our side to be true.
 */
static unsigned char alt[SLOT];

/* The blob's own `process(float)` over the whole stream; returns count_2c. */
static unsigned int
blob_scalar_hits(unsigned int nden, unsigned int nnum, unsigned int blockSize,
		 unsigned int s1, unsigned int s2, float thr,
		 unsigned int flag, float rat, unsigned int blockLen,
		 int nsamp)
{
	unsigned int hits;
	int i;

	memcpy(alt, before, SLOT);
	ref_gtd_c1(alt, nden, nnum, den, num, s1, s2, thr, flag, rat,
		   blockLen, blockSize);
	for (i = 0; i < nsamp; i++)
		ref_gtd_process1(alt, stream[i]);
	hits = ((GenericToneDetector *)alt)->count_2c;
	ref_gtd_d1(alt);

	return hits;
}

/*
 * One stream through `process(float *, unsigned)`.  `chunk` is how many
 * samples a call takes; zero means "a varying number, including none", which
 * is what drives the `n == 0` early return inside a live stream as well as at
 * the start of one.  Arms are only counted at one sample a call, because a
 * per-block reading of the object cannot be taken across a chunk that spans
 * several blocks.
 */
static void
drive_array(unsigned int nden, unsigned int nnum, unsigned int blockSize,
	    unsigned int s1, unsigned int s2, float thr, unsigned int flag,
	    float rat, unsigned int blockLen, int nsamp, int chunk, int tag,
	    struct arms *a)
{
	unsigned char pre[SLOT];
	int i = 0, step = 0, live = harness_alloc.live;

	our_gtd_c1(ours, nden, nnum, den, num, s1, s2, thr, flag, rat,
		   blockLen, blockSize);
	ref_gtd_c1(theirs, nden, nnum, den, num, s1, s2, thr, flag, rat,
		   blockLen, blockSize);

	while (i < nsamp) {
		int cnt = chunk > 0 ? chunk : step % 5;
		int ra, rb;

		if (cnt > nsamp - i)
			cnt = nsamp - i;

		memcpy(pre, theirs, SLOT);
		ra = our_gtd_processn(ours, stream + i, (unsigned int)cnt);
		rb = ref_gtd_processn(theirs, stream + i, (unsigned int)cnt);

		diff_eq_int("process(float *, n) returns the blob's answer "
			    "(%ld)", ra, rb, tag * 1000 + i);
		diff_eq_int("and it is the field at +0x38 (%ld)", rb,
			    (int)((GenericToneDetector *)theirs)->detected,
			    tag * 1000 + i);
		compare_objects_("after process(float *, n)", tag * 1000 + i);
		if (cnt == 1)
			note_arm(pre, theirs, a);
		i += cnt;
		step++;
	}

	compare_filters_after_(nden, nnum, blockSize, tag);
	diff_eq_int("no store past the object (%ld)", guard_intact(), 1, tag);
	our_gtd_d1(ours);
	ref_gtd_d1(theirs);
	diff_eq_int("the stream leaked nothing (%ld)",
		    harness_alloc.live - live, 0, tag);
}

/*
 * A designed block-scoring scenario, run twice: once through the array
 * overload counting arms, and once through the scalar overload on the blob
 * alone.  `hit` is what the array did with every block and `scalarHits` is
 * what the scalar did, so the two together say WHICH arm the array took.
 */
static void
weak_case(const char *what, double gain, float amp, float thr,
	  unsigned int flag, int expectHit, int tag, struct arms *a)
{
	struct arms e;
	unsigned int scalarHits;

	memset(&e, 0, sizeof e);
	seed(tag);
	den[0] = 1.0;
	num[0] = gain;
	fill_blocks(8u, &amp, 1);

	drive_array(1u, 1u, 2u, 24u, 800u, thr, flag, 0.5f, 8u, NSAMP, 1, tag,
		    &e);
	scalarHits = blob_scalar_hits(1u, 1u, 2u, 24u, 800u, thr, flag, 0.5f,
				      8u, NSAMP);

	diff_eq_int(what, expectHit ? (e.hit == e.boundary) : (e.hit == 0), 1,
		    tag);
	diff_eq_int("the block count is not zero (%ld)", e.boundary > 0, 1,
		    tag);
	/*
	 * The mean is under the threshold in every one of these, so the scalar
	 * overload -- which has no weak arm -- must score nothing at all.  Any
	 * hit the array scored above is therefore a WEAK hit and cannot be
	 * anything else.
	 */
	diff_eq_int("the scalar overload, which has no weak arm, scores "
		    "nothing on the same stream (%ld)", (int)scalarHits, 0,
		    tag);

	a->hit += e.hit;
	a->miss += e.miss;
	a->set += e.set;
	a->cleared += e.cleared;
	a->boundary += e.boundary;
	a->inblock += e.inblock;
}

static int
run_processn(void)
{
	struct arms a, w;
	int trial;

	diff_begin("GenericToneDetector::process(float *, unsigned)");

	memset(&a, 0, sizeof a);
	memset(&w, 0, sizeof w);

	/*
	 * `n == 0` FIRST, on a fresh object and on a used one.  The object
	 * tests it at 0x104a2 before it has touched anything, so the claim is
	 * that nothing moves and the answer still comes back.
	 */
	{
		static const float amps[1] = { AMP_STRONG };
		int ra, rb, i;
		unsigned char snap[SLOT];

		seed(9);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(8u, amps, 1);

		our_gtd_c1(ours, 1u, 1u, den, num, 24u, 16u, SCEN_THR, 1u,
			   0.5f, 8u, 2u);
		ref_gtd_c1(theirs, 1u, 1u, den, num, 24u, 16u, SCEN_THR, 1u,
			   0.5f, 8u, 2u);

		for (i = 0; i < 2; i++) {
			memcpy(snap, theirs, SLOT);
			ra = our_gtd_processn(ours, stream, 0u);
			rb = ref_gtd_processn(theirs, stream, 0u);

			diff_eq_int("n == 0 returns the answer (%ld)", ra, rb,
				    300 + i);
			diff_eq_int("and it is the field at +0x38 (%ld)", rb,
				    (int)((GenericToneDetector *)theirs)
					->detected, 300 + i);
			diff_eq_int("n == 0 moved nothing (%ld)",
				    memcmp(snap, theirs, SLOT) == 0, 1,
				    300 + i);
			compare_objects_("after n == 0", 300 + i);

			/* Run a while, then ask again on a used object. */
			our_gtd_processn(ours, stream, 40u);
			ref_gtd_processn(theirs, stream, 40u);
			compare_objects_("after 40 samples", 300 + i);
		}
		our_gtd_d1(ours);
		ref_gtd_d1(theirs);
	}

	/*
	 * The random sweep, at varying chunk sizes so that block boundaries
	 * fall inside a call as often as on one, and with the same deliberately
	 * unbounded coefficients as the scalar sweep -- the accumulators reach
	 * infinity and NaN, and the weak arm's two comparisons are spelled the
	 * way the object's branches are precisely because of what happens
	 * there.
	 */
	for (trial = 0; trial < 12; trial++) {
		unsigned int nden = 1u + (unsigned)(trial % 4);
		unsigned int nnum = 1u + (unsigned)((trial + 2) % 4);
		unsigned int blockSize = (unsigned)(trial % 3);
		unsigned int blockLen = 1u + (unsigned)(trial % 7);
		unsigned int flag = (unsigned)(trial & 1);
		float thr = as_float(floatbits[trial % NFLOATS]);
		float rat = as_float(floatbits[(trial + 5) % NFLOATS]);
		int i;

		seed(trial);
		for (i = 0; i < NSAMP; i++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
			stream[i] = (float)(int)(lfsr - 0x8000u) / 4096.0f;
		}
		drive_array(nden, nnum, blockSize, 3u * blockLen,
			    2u * blockLen, thr, flag, rat, blockLen, NSAMP,
			    trial & 1 ? 1 : 0, 100 + trial, &a);
	}

	/*
	 * THE WEAK ARM, ALL FOUR OF ITS DECISIONS.  A gain of `g` makes
	 * meanOut = g^2 * amp^2 and acc_18 = g^2 * acc_14, so each of the
	 * arm's three tests can be put on either side of its constant by
	 * choosing `g` and `amp`.  The threshold is 1.0f throughout and every
	 * mean below is UNDER it, which is what keeps the scalar overload
	 * scoring nothing and makes each hit unambiguously a weak one.
	 *
	 *   0.64  in the band, flag set, ratio 1.00 over 0.85  -> HIT
	 *   0.45  under half the threshold                     -> miss
	 *   0.64  in the band but flag clear                   -> miss
	 *   0.64  in the band, ratio 0.25 under 0.85           -> miss
	 *
	 * and then the constant itself, from both sides, because 0.8 and 0.9
	 * would both pass everything above:
	 *
	 *   0.83  in the band, ratio 0.8281 UNDER 0.85         -> miss
	 *   0.86  in the band, ratio 0.8649 OVER 0.85          -> HIT
	 */
	weak_case("a mean in the upper half of the threshold hits (%ld)",
		  1.0, 0.8f, 1.0f, 1u, 1, 400, &w);
	weak_case("a mean below half the threshold does not (%ld)",
		  1.0, 0.671f, 1.0f, 1u, 0, 401, &w);
	weak_case("nor does one in the band with flag clear (%ld)",
		  1.0, 0.8f, 1.0f, 0u, 0, 402, &w);
	weak_case("nor one whose smoothers stand at a quarter (%ld)",
		  0.5, 1.6f, 1.0f, 1u, 0, 403, &w);
	weak_case("nor one whose smoothers stand at 0.8281, just under 0.85 "
		  "(%ld)", 0.91, 1.0f, 1.0f, 1u, 0, 404, &w);
	weak_case("one at 0.8649, just over it, does (%ld)",
		  0.93, 1.0f, 1.0f, 1u, 1, 405, &w);

	diff_eq_int("the weak arm scored (%ld)", w.hit > 0, 1, 0);
	diff_eq_int("and declined to score (%ld)", w.miss > 0, 1, 0);

	/*
	 * THE WEAK ARM'S OWN BOOKKEEPING, which the six cases above cannot
	 * reach because each of them takes one arm for every block of its
	 * stream.  Four blocks in the band and four below half, repeating, so
	 * the weak hit's `count_30 = 0` has a counter to clear and the weak
	 * miss's withdrawal has a `count_2c` to drop.  `blocks1` is ZERO here,
	 * which makes the weak miss's silence about `blocks1` visible in the
	 * same way tag 206 makes the below-threshold arm's: the answer goes
	 * down at the withdrawal and stays down until a block actually hits.
	 * blocks2 is 2.
	 */
	{
		static const float amps[8] = {
			0.8f, 0.8f, 0.8f, 0.8f,
			0.671f, 0.671f, 0.671f, 0.671f
		};
		struct arms e;
		unsigned int scalarHits;

		memset(&e, 0, sizeof e);
		seed(11);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(8u, amps, 8);
		drive_array(1u, 1u, 2u, 0u, 16u, 1.0f, 1u, 0.5f, 8u, NSAMP, 1,
			    407, &e);
		scalarHits = blob_scalar_hits(1u, 1u, 2u, 0u, 16u, 1.0f, 1u,
					      0.5f, 8u, NSAMP);

		diff_eq_int("the weak arm hits on the loud blocks (%ld)",
			    e.hit > 0, 1, 407);
		diff_eq_int("and misses on the quiet ones (%ld)", e.miss > 0,
			    1, 407);
		diff_eq_int("the answer goes up (%ld)", e.set > 0, 1, 407);
		diff_eq_int("and is withdrawn (%ld)", e.cleared > 0, 1, 407);
		diff_eq_int("none of it is the scalar overload's doing (%ld)",
			    (int)scalarHits, 0, 407);
		a.hit += e.hit;
		a.miss += e.miss;
		a.set += e.set;
		a.cleared += e.cleared;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
	}

	/*
	 * The reciprocal, in this body too -- 41 unit samples, a mean that
	 * falls short of 1.0, and a threshold of 1.0f.  The weak arm is
	 * disarmed with `flag` clear so the block's fate rests on the
	 * threshold alone; see the same scenario in run_process1 for the
	 * arithmetic.
	 */
	{
		static const float amps[1] = { 1.0f };
		struct arms e;

		memset(&e, 0, sizeof e);
		seed(12);
		den[0] = 1.0;
		num[0] = 1.0;
		fill_blocks(41u, amps, 1);
		drive_array(1u, 1u, 2u, 82u, 82u, 1.0f, 0u, 0.5f, 41u, NSAMP,
			    1, 408, &e);
		diff_eq_int("the mean of 41 unit samples falls SHORT of 1.0 "
			    "here too (%ld)", e.hit, 0, 408);
		diff_eq_int("blocks of 41 completed (%ld)", e.boundary > 0, 1,
			    408);
		a.miss += e.miss;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
	}

	a.hit += w.hit;
	a.miss += w.miss;
	a.set += w.set;
	a.cleared += w.cleared;
	a.boundary += w.boundary;
	a.inblock += w.inblock;

	/*
	 * The strong arm and the withdrawal, reached through this overload
	 * too: the same comb and the same phases as the scalar run, so the
	 * shared bookkeeping is driven on both bodies and not only on one.
	 */
	{
		struct arms e;

		memset(&e, 0, sizeof e);
		seed(10);
		den[0] = 1.0;
		num[0] = 1.0;
		num[1] = 1.0;
		num[2] = 1.0;
		fill_phases(8u, 8, 1.0f);
		drive_array(1u, 3u, 2u, 24u, 16u, SCEN_THR, 1u, 8.0f, 8u,
			    NSAMP, 1, 406, &e);
		diff_eq_int("the comb hits through this overload (%ld)",
			    e.hit > 0, 1, 406);
		diff_eq_int("misses on the ratio (%ld)", e.miss > 0, 1, 406);
		diff_eq_int("sets the answer (%ld)", e.set > 0, 1, 406);
		diff_eq_int("and withdraws it (%ld)", e.cleared > 0, 1, 406);
		a.hit += e.hit;
		a.miss += e.miss;
		a.set += e.set;
		a.cleared += e.cleared;
		a.boundary += e.boundary;
		a.inblock += e.inblock;
	}

	diff_eq_int("blocks completed (%ld)", a.boundary > 0, 1, 0);
	diff_eq_int("samples inside a block (%ld)", a.inblock > 0, 1, 0);
	diff_eq_int("the hit arm was reached (%ld)", a.hit > 0, 1, 0);
	diff_eq_int("the miss arm was reached (%ld)", a.miss > 0, 1, 0);
	diff_eq_int("the answer was set (%ld)", a.set > 0, 1, 0);
	diff_eq_int("the answer was withdrawn (%ld)", a.cleared > 0, 1, 0);

	return diff_end();
}

static int
run_ctor(void)
{
	int trial, variant, moved = 0, distinct = 0;
	unsigned int firstBlocks = 0;

	diff_begin("GenericToneDetector::GenericToneDetector");

	for (variant = 0; variant < 2; variant++) {
		gtd_ctor_t ctor_a = variant ? (gtd_ctor_t)our_gtd_c2
					    : (gtd_ctor_t)our_gtd_c1;
		gtd_ctor_t ctor_b = variant ? (gtd_ctor_t)ref_gtd_c2
					    : (gtd_ctor_t)ref_gtd_c1;
		gtd_dtor_t dtor_a = variant ? (gtd_dtor_t)our_gtd_d2
					    : (gtd_dtor_t)our_gtd_d1;
		gtd_dtor_t dtor_b = variant ? (gtd_dtor_t)ref_gtd_d2
					    : (gtd_dtor_t)ref_gtd_d1;

		for (trial = 0; trial < NTRIAL; trial++) {
			unsigned int nden = 1u + (unsigned)(trial % 4);
			unsigned int nnum = 1u + (unsigned)((trial + 2) % 4);
			unsigned int blockSize = (unsigned)(trial % 3);
			unsigned int s1 = durations[trial % NDUR];
			unsigned int s2 = durations[(trial + 3) % NDUR];
			unsigned int blockLen = blocklens[trial % NBLOCKLEN];
			unsigned int flag = (unsigned)(trial * 7 + variant);
			float thr = as_float(floatbits[trial % NFLOATS]);
			float rat = as_float(floatbits[(trial + 5) % NFLOATS]);
			int tag = trial * 2 + variant;
			int live;

			seed(trial);
			live = harness_alloc.live;

			ctor_a(ours, nden, nnum, den, num, s1, s2, thr, flag,
			       rat, blockLen, blockSize);
			ctor_b(theirs, nden, nnum, den, num, s1, s2, thr, flag,
			       rat, blockLen, blockSize);

			/*
			 * Three allocations per side: the filter itself and its
			 * two history buffers.  Asserted because the whole
			 * `operator new` reading rests on the constructor
			 * reaching sysdep_malloc at all.
			 */
			diff_eq_int("six allocations, three a side (%ld)",
				    harness_alloc.live - live, 6, tag);

			compare_filters_(nden, nnum, blockSize, 0, tag);

			/*
			 * The two filter POINTERS are two different heap
			 * addresses and always will be, so they are neutralised
			 * before the whole-object comparison and checked
			 * separately above.  Everything else in the object --
			 * fourteen fields -- is compared as it stands.
			 */
			{
				GenericIIR<float, double> *pa =
					((GenericToneDetector *)ours)->filter;
				GenericIIR<float, double> *pb =
					((GenericToneDetector *)theirs)->filter;

				((GenericToneDetector *)ours)->filter = 0;
				((GenericToneDetector *)theirs)->filter = 0;
				diff_eq_obj("after constructor",
					    GenericToneDetector, ours, theirs,
					    tag);
				((GenericToneDetector *)ours)->filter = pa;
				((GenericToneDetector *)theirs)->filter = pb;
			}

			diff_eq_int("no store past the object (%ld)",
				    guard_intact(), 1, tag);

			/*
			 * The two floats, compared as BITS.  `==` would call
			 * +0.0 and -0.0 equal and would say nothing at all
			 * about a NaN, and the constructor's claim is that it
			 * moved thirty-two bits unaltered.
			 */
			diff_eq_int("threshold is bit-exact (%ld)",
				    memcmp(ours + 4, theirs + 4, 4) == 0 &&
				    memcmp(ours + 4, &thr, 4) == 0, 1, tag);
			diff_eq_int("ratio is bit-exact (%ld)",
				    memcmp(ours + 8, theirs + 8, 4) == 0 &&
				    memcmp(ours + 8, &rat, 4) == 0, 1, tag);

			if (memcmp(before, ours, SLOT) != 0)
				moved = 1;
			if (trial == 0 && variant == 0)
				firstBlocks =
					((GenericToneDetector *)ours)->blocks1;
			else if (((GenericToneDetector *)ours)->blocks1 !=
				 firstBlocks)
				distinct = 1;

			/* Hand both filters back before the next trial. */
			dtor_a(ours);
			dtor_b(theirs);
			diff_eq_int("the destructor freed all six (%ld)",
				    harness_alloc.live - live, 0, tag);
		}
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the rounded-up duration was not the same every trial",
		    distinct, 1, 0);
	diff_eq_int("the seed varied between trials",
		    memcmp(trial0, before, SLOT) != 0, 1, 0);

	return diff_end();
}

/*
 * The destructor on its own.  Two claims: that it writes NOTHING into the
 * object -- `filter` is left dangling rather than nulled, which is visible
 * only because the object is compared against its own snapshot -- and that a
 * null `filter` is a no-op, which is the arm the blob's `test %ebx,%ebx; jne`
 * guards and which the constructor path never reaches.
 */
static int
run_dtor(void)
{
	int trial, variant;

	diff_begin("GenericToneDetector::~GenericToneDetector");

	for (variant = 0; variant < 2; variant++) {
		gtd_ctor_t ctor_a = variant ? (gtd_ctor_t)our_gtd_c2
					    : (gtd_ctor_t)our_gtd_c1;
		gtd_ctor_t ctor_b = variant ? (gtd_ctor_t)ref_gtd_c2
					    : (gtd_ctor_t)ref_gtd_c1;
		gtd_dtor_t dtor_a = variant ? (gtd_dtor_t)our_gtd_d2
					    : (gtd_dtor_t)our_gtd_d1;
		gtd_dtor_t dtor_b = variant ? (gtd_dtor_t)ref_gtd_d2
					    : (gtd_dtor_t)ref_gtd_d1;

		for (trial = 0; trial < NTRIAL; trial++) {
			unsigned char after_ctor_a[SLOT], after_ctor_b[SLOT];
			int tag = trial * 2 + variant;
			int nullFrees, live;

			seed(trial);
			live = harness_alloc.live;

			ctor_a(ours, 2u, 3u, den, num, 100u, 200u, 1.0f, 1u,
			       0.5f, blocklens[trial % NBLOCKLEN], 1u);
			ctor_b(theirs, 2u, 3u, den, num, 100u, 200u, 1.0f, 1u,
			       0.5f, blocklens[trial % NBLOCKLEN], 1u);

			memcpy(after_ctor_a, ours, SLOT);
			memcpy(after_ctor_b, theirs, SLOT);

			dtor_a(ours);
			dtor_b(theirs);

			diff_eq_int("the destructor wrote nothing (%ld)",
				    memcmp(after_ctor_a, ours, SLOT) == 0 &&
				    memcmp(after_ctor_b, theirs, SLOT) == 0,
				    1, tag);
			diff_eq_int("it left `filter` dangling, not null (%ld)",
				    ((GenericToneDetector *)ours)->filter != 0,
				    1, tag);
			diff_eq_int("everything it allocated is back (%ld)",
				    harness_alloc.live - live, 0, tag);

			/*
			 * The null arm.  A delete-expression tests the pointer
			 * itself, so `operator delete` is never entered and the
			 * allocator sees no free at all -- which is what
			 * distinguishes it from an unconditional
			 * `sysdep_free(filter)`.
			 */
			seed(trial);
			nullFrees = harness_alloc.free_null;
			live = harness_alloc.live;
			((GenericToneDetector *)ours)->filter = 0;
			((GenericToneDetector *)theirs)->filter = 0;
			memcpy(after_ctor_a, ours, SLOT);
			memcpy(after_ctor_b, theirs, SLOT);

			dtor_a(ours);
			dtor_b(theirs);

			diff_eq_obj("after destroying a null filter",
				    GenericToneDetector, ours, theirs, tag);
			diff_eq_int("a null filter is a no-op (%ld)",
				    memcmp(after_ctor_a, ours, SLOT) == 0 &&
				    memcmp(after_ctor_b, theirs, SLOT) == 0 &&
				    harness_alloc.live == live &&
				    harness_alloc.free_null == nullFrees,
				    1, tag);
		}
	}

	return diff_end();
}

/* ------------------------------------------------- ANSamToneDetector ---- */

/*
 * THE DERIVED CLASS ADDS ONE THING AND THE TEST IS BUILT AROUND IT: a table.
 * Seven of its eight arguments go straight through to the base, whose
 * constructor is swept above; what is unproven until here is the selector --
 * that `sampleRate == 8000` picks a 13-tap pair and anything else picks an
 * 11-tap pair, and that the four transcribed arrays are the blob's.
 *
 * SO THE COEFFICIENTS ARE COMPARED, NOT THE POINTERS.  Ours point into this
 * reconstruction's .data and the blob's into the blob's; they can never
 * agree, and comparing them would fail on a correct reconstruction.  What is
 * compared instead is `nden`/`nnum` doubles at each, in full, on every trial
 * -- so any one of the 48 values being mistyped is a failure here.  The
 * exclusion is exactly two pointers wide and everything else in the filter,
 * the object and the guard is compared as it stands.
 *
 * BOTH ARMS ARE REQUIRED TO HAVE BEEN REACHED.  A selector sweep that only
 * ever saw 8000 would pass with the two tables swapped, so the rates below
 * bracket 8000 on both sides -- 7999 and 8001 -- as well as covering zero and
 * 0xffffffff, and the run asserts that the 13-tap and the 11-tap arm were
 * both taken and that the base saw a DIFFERENT tap count on the two.
 *
 * THE BASE'S OFFSET IS CHECKED, not assumed: the C2/D2 variants say the base
 * is a base rather than a member, and `(GenericToneDetector *)p == p` is what
 * says it starts at offset zero.  If it did not, every field comparison here
 * would still pass and the object map would still be wrong.
 *
 * The float sweep and the object fill are `run_ctor`'s -- same slot, same
 * `seed`, same guard -- because the derived object IS the base object and
 * nothing here would be served by a second fixture.
 */

typedef void (*ansam_ctor_t)(void *self, unsigned int, unsigned int, float,
			     unsigned int, float, unsigned int, unsigned int,
			     unsigned int);

extern "C" {
void our_ansam_c1(void *, unsigned int, unsigned int, float, unsigned int,
		  float, unsigned int, unsigned int, unsigned int)
	asm("_ZN17ANSamToneDetectorC1Ejjfjfjjj");
void our_ansam_c2(void *, unsigned int, unsigned int, float, unsigned int,
		  float, unsigned int, unsigned int, unsigned int)
	asm("_ZN17ANSamToneDetectorC2Ejjfjfjjj");
void ref_ansam_c1(void *, unsigned int, unsigned int, float, unsigned int,
		  float, unsigned int, unsigned int, unsigned int)
	asm("ref__ZN17ANSamToneDetectorC1Ejjfjfjjj");
void ref_ansam_c2(void *, unsigned int, unsigned int, float, unsigned int,
		  float, unsigned int, unsigned int, unsigned int)
	asm("ref__ZN17ANSamToneDetectorC2Ejjfjfjjj");
void our_ansam_d1(void *) asm("_ZN17ANSamToneDetectorD1Ev");
void our_ansam_d2(void *) asm("_ZN17ANSamToneDetectorD2Ev");
void ref_ansam_d1(void *) asm("ref__ZN17ANSamToneDetectorD1Ev");
void ref_ansam_d2(void *) asm("ref__ZN17ANSamToneDetectorD2Ev");
}

/* 8000 twice over, and both its neighbours. */
static const unsigned int ansam_rates[] = {
	8000u, 0u, 1u, 7999u, 8001u, 8000u, 9600u, 16000u, 0xffffffffu
};
#define NRATE	((int)(sizeof(ansam_rates) / sizeof(ansam_rates[0])))

#define ANSAM_TRIALS	(NRATE * 7)

static int
run_ansam(void)
{
	int trial, variant, moved = 0;
	int saw13 = 0, saw11 = 0;

	diff_begin("ANSamToneDetector::ANSamToneDetector");

	for (variant = 0; variant < 2; variant++) {
		ansam_ctor_t ctor_a = variant ? (ansam_ctor_t)our_ansam_c2
					      : (ansam_ctor_t)our_ansam_c1;
		ansam_ctor_t ctor_b = variant ? (ansam_ctor_t)ref_ansam_c2
					      : (ansam_ctor_t)ref_ansam_c1;
		gtd_dtor_t dtor_a = variant ? (gtd_dtor_t)our_ansam_d2
					    : (gtd_dtor_t)our_ansam_d1;
		gtd_dtor_t dtor_b = variant ? (gtd_dtor_t)ref_ansam_d2
					    : (gtd_dtor_t)ref_ansam_d1;

		for (trial = 0; trial < ANSAM_TRIALS; trial++) {
			unsigned int rate = ansam_rates[trial % NRATE];
			unsigned int s1 = durations[trial % NDUR];
			unsigned int s2 = durations[(trial + 3) % NDUR];
			unsigned int blockLen = blocklens[trial % NBLOCKLEN];
			unsigned int blockSize = (unsigned)(trial % 3);
			unsigned int flag = (unsigned)(trial * 5 + variant);
			float thr = as_float(floatbits[trial % NFLOATS]);
			float rat = as_float(floatbits[(trial + 4) % NFLOATS]);
			unsigned int taps = (rate == 8000u) ? 13u : 11u;
			int tag = trial * 2 + variant;
			int live;

			seed(trial);
			live = harness_alloc.live;

			ctor_a(ours, s1, s2, thr, flag, rat, rate, blockLen,
			       blockSize);
			ctor_b(theirs, s1, s2, thr, flag, rat, rate, blockLen,
			       blockSize);

			if (rate == 8000u)
				saw13 = 1;
			else
				saw11 = 1;

			/*
			 * The base's own three allocations and no fourth: the
			 * derived constructor allocates nothing of its own.
			 */
			diff_eq_int("six allocations, three a side (%ld)",
				    harness_alloc.live - live, 6, tag);

			/* The base subobject starts at offset zero. */
			diff_eq_int("the base is at offset zero (%ld)",
				    (unsigned char *)(GenericToneDetector *)
					(ANSamToneDetector *)ours ==
				    (unsigned char *)ours,
				    1, tag);

			/*
			 * The tap count the base was handed, predicted from
			 * the selector and read out of the BLOB's filter, so
			 * the claim does not go through our source.
			 */
			{
				unsigned char *fb = (unsigned char *)
				    ((GenericToneDetector *)theirs)->filter;
				unsigned int nden, nnum;

				memcpy(&nden, fb + 0x10, sizeof nden);
				memcpy(&nnum, fb + 0x14, sizeof nnum);
				diff_eq_int("the blob's nden (%ld)",
					    (long)nden, (long)taps, tag);
				diff_eq_int("the blob's nnum (%ld)",
					    (long)nnum, (long)taps, tag);
			}

			compare_filters_(taps, taps, blockSize, 1, tag);

			{
				GenericIIR<float, double> *pa =
					((GenericToneDetector *)ours)->filter;
				GenericIIR<float, double> *pb =
					((GenericToneDetector *)theirs)->filter;

				((GenericToneDetector *)ours)->filter = 0;
				((GenericToneDetector *)theirs)->filter = 0;
				diff_eq_obj_(__FILE__, __LINE__,
					     "after constructor",
					     "ANSamToneDetector", ours, theirs,
					     (int)sizeof(ANSamToneDetector),
					     (long)tag);
				((GenericToneDetector *)ours)->filter = pa;
				((GenericToneDetector *)theirs)->filter = pb;
			}

			diff_eq_int("no store past the object (%ld)",
				    guard_intact(), 1, tag);

			diff_eq_int("threshold is bit-exact (%ld)",
				    memcmp(ours + 4, theirs + 4, 4) == 0 &&
				    memcmp(ours + 4, &thr, 4) == 0, 1, tag);
			diff_eq_int("ratio is bit-exact (%ld)",
				    memcmp(ours + 8, theirs + 8, 4) == 0 &&
				    memcmp(ours + 8, &rat, 4) == 0, 1, tag);

			if (memcmp(before, ours, SLOT) != 0)
				moved = 1;

			/*
			 * The destructor, in the same trial: it writes
			 * nothing, and it hands back everything the base
			 * allocated.
			 */
			{
				unsigned char aa[SLOT], ab[SLOT];

				memcpy(aa, ours, SLOT);
				memcpy(ab, theirs, SLOT);

				dtor_a(ours);
				dtor_b(theirs);

				diff_eq_int("the destructor wrote nothing "
					    "(%ld)",
					    memcmp(aa, ours, SLOT) == 0 &&
					    memcmp(ab, theirs, SLOT) == 0,
					    1, tag);
				diff_eq_int("it left `filter` dangling, not "
					    "null (%ld)",
					    ((GenericToneDetector *)ours)
						->filter != 0, 1, tag);
				diff_eq_int("the destructor freed all six "
					    "(%ld)",
					    harness_alloc.live - live, 0, tag);
			}
		}
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the 13-tap arm was reached", saw13, 1, 0);
	diff_eq_int("the 11-tap arm was reached", saw11, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_ctor();
	rc |= run_dtor();
	rc |= run_ansam();
	rc |= run_reset();
	rc |= run_process1();
	rc |= run_processn();

	return rc;
}
