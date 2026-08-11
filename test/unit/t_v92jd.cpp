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
#include "dsplib/V90Parameters.h"

extern "C" {
/*
 * The constructor and destructor by symbol, on both sides -- C++ has no
 * syntax for running a constructor over storage that already exists, and this
 * fixture's whole point is that the storage is seeded and never zeroed.
 * t_v90jd.cpp carries the argument in full.
 */
void our_ctor1(void *self, V90Parameters *p)
	asm("_ZN5V92JdC1EP13V90Parameters");
void ref_ctor1(void *self, V90Parameters *p)
	asm("ref__ZN5V92JdC1EP13V90Parameters");
void our_ctor2(void *self, V90Parameters *p)
	asm("_ZN5V92JdC2EP13V90Parameters");
void ref_ctor2(void *self, V90Parameters *p)
	asm("ref__ZN5V92JdC2EP13V90Parameters");
void our_dtor(void *self) asm("_ZN5V92JdD1Ev");
void ref_dtor(void *self) asm("ref__ZN5V92JdD1Ev");

void ref_packJdData(void *self) asm("ref__ZN5V92Jd10packJdDataEv");
void ref_packJdPhaseData(void *self) asm("ref__ZN5V92Jd15packJdPhaseDataEv");
unsigned char *ref_getJdBitVector(void *self)
	asm("ref__ZN5V92Jd14getJdBitVectorEv");
unsigned char *ref_getJdPhaseBitVector(void *self)
	asm("ref__ZN5V92Jd19getJdPhaseBitVectorEv");
/*
 * The four accessors.  The return types are the ones V92Jd.cpp derives from
 * the object: `float`, `int`, void and `unsigned char`.  `getJdPhase` really
 * does return in `st(0)` -- it ends `fstps`/`flds` on a stack slot, which is
 * the x87 return convention with the float rounded once on the way out.
 */
float ref_getJdPhase(void *self) asm("ref__ZN5V92Jd10getJdPhaseEv");
int ref_getRatesMask(void *self) asm("ref__ZN5V92Jd12getRatesMaskEv");
void ref_getConstelationSize(void *self, unsigned char *first,
			     unsigned char *second)
	asm("ref__ZN5V92Jd19getConstelationSizeEPhS0_");
unsigned char ref_getMaxLookahead(void *self)
	asm("ref__ZN5V92Jd15getMaxLookaheadEv");

void ref_unPackJdReset(void *self) asm("ref__ZN5V92Jd13unPackJdResetEv");
void ref_unPackJdPhaseReset(void *self)
	asm("ref__ZN5V92Jd18unPackJdPhaseResetEv");
}

#define SLOT 288

union jd_slot {
	unsigned char raw[SLOT];
	int align;	/* the object holds ints; keep the slot 4-aligned */
};

static union jd_slot ours, theirs;

/*
 * The object lives IN the byte array: V92Jd has a user-declared destructor,
 * because the blob has `_ZN5V92JdD1Ev` and GCC emits no symbol for a trivial
 * implicit one, and a union with a non-trivially-destructible variant member
 * has its own destructor deleted.  t_v90jd.cpp carries the full argument.
 */
#define OURS	(*(V92Jd *)ours.raw)
#define THEIRS	(*(V92Jd *)theirs.raw)

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
			OURS.packJdPhaseData();
			ref_packJdPhaseData(&THEIRS);
			diff_eq_obj("after packJdPhaseData", V92Jd, &OURS,
				    &THEIRS, trial);
			OURS.packJdData();
			ref_packJdData(&THEIRS);
			diff_eq_obj("after packJdData", V92Jd, &OURS,
				    &THEIRS, trial);
		} else {
			OURS.packJdData();
			ref_packJdData(&THEIRS);
			diff_eq_obj("after packJdData", V92Jd, &OURS,
				    &THEIRS, trial);
			OURS.packJdPhaseData();
			ref_packJdPhaseData(&THEIRS);
			diff_eq_obj("after packJdPhaseData", V92Jd, &OURS,
				    &THEIRS, trial);
		}

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, &OURS.bits[V90JD_GROUP3 + 1], 16);
		else if (memcmp(first, &OURS.bits[V90JD_GROUP3 + 1], 16))
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

		pa = OURS.getJdBitVector();
		pb = ref_getJdBitVector(&THEIRS);
		diff_eq_int("getJdBitVector() return offset (trial %ld)",
			    pa - ours.raw, pb - theirs.raw, trial);
		diff_eq_obj("after getJdBitVector", V92Jd, &OURS, &THEIRS,
			    trial);

		pa = OURS.getJdPhaseBitVector();
		pb = ref_getJdPhaseBitVector(&THEIRS);
		diff_eq_int("getJdPhaseBitVector() return offset (trial %ld)",
			    pa - ours.raw, pb - theirs.raw, trial);
		diff_eq_obj("after getJdPhaseBitVector", V92Jd, &OURS,
			    &THEIRS, trial);

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (trial == 0)
			memcpy(first, &OURS.phaseBits[V90JD_GROUP3 + 1], 16);
		else if (memcmp(first, &OURS.phaseBits[V90JD_GROUP3 + 1], 16))
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
		OURS.unpack[0] = THEIRS.unpack[0] =
			(unsigned char)(trial | 1);
		OURS.unpack[1] = THEIRS.unpack[1] =
			(unsigned char)(trial | 2);
		OURS.unpackWord = THEIRS.unpackWord = 0x5a5a0000 + trial;
		OURS.unpackPhaseWord = THEIRS.unpackPhaseWord =
			0x33330000 + trial;

		if (trial & 1) {
			OURS.unPackJdPhaseReset();
			ref_unPackJdPhaseReset(&THEIRS);
			diff_eq_obj("after unPackJdPhaseReset", V92Jd, &OURS,
				    &THEIRS, trial);
			/*
			 * The phase reset must leave the DATA word alone --
			 * the two share their bytes and not their words.
			 */
			diff_eq_int("unPackJdPhaseReset spared +0xd4 (%ld)",
				    OURS.unpackWord, 0x5a5a0000 + trial,
				    trial);
			OURS.unPackJdReset();
			ref_unPackJdReset(&THEIRS);
			diff_eq_obj("after unPackJdReset", V92Jd, &OURS,
				    &THEIRS, trial);
		} else {
			OURS.unPackJdReset();
			ref_unPackJdReset(&THEIRS);
			diff_eq_obj("after unPackJdReset", V92Jd, &OURS,
				    &THEIRS, trial);
			diff_eq_int("unPackJdReset spared +0xd8 (%ld)",
				    OURS.unpackPhaseWord,
				    0x33330000 + trial, trial);
			OURS.unPackJdPhaseReset();
			ref_unPackJdPhaseReset(&THEIRS);
			diff_eq_obj("after unPackJdPhaseReset", V92Jd, &OURS,
				    &THEIRS, trial);
		}

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
	}

	return diff_end();
}

/*
 * ===========================================================================
 * The constructor.
 *
 * ONE PARAMETER BLOCK, SHARED, reseeded every trial -- the constructor only
 * reads it, and two separately seeded blocks would agree whatever was read.
 *
 * `V92_JD_PHASE` IS NOT LEFT TO THE SEED.  It goes through
 * `(long long)(65536.0f * phase)` with the x87 set to truncate, so the
 * interesting cases are the ones near a boundary of that conversion, and
 * pseudorandom bytes are a NaN or a 10^30 nine times in ten -- which tests one
 * path and then tests it again.  The table below sweeps the fixed-point
 * conversion instead: zero, both signs, the exact half, a value whose product
 * lands one ulp under an integer, and two that overflow sixteen bits so the
 * masking of the low word is exercised.  The random bytes still cover the
 * other two fields.
 * ===========================================================================
 */
union param_slot {
	unsigned char raw[sizeof(V90Parameters)];
	int align;
};

static union param_slot params;

static const float jd_phase[] = {
	0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 0.25f, 0.1f, 1.0f / 3.0f,
	0.49999997f,		/* * 65536 is just under 32768	     */
	0.500000060f,		/* and just over		     */
	32767.5f, -32768.0f,	/* products that overflow 16 bits    */
	1.0f / 65536.0f,	/* exactly one			     */
	0.9999847f,		/* 65535.0 / 65536.0		     */
	1e-8f, -1e-8f
};

#define NPHASE ((int)(sizeof(jd_phase) / sizeof(jd_phase[0])))

static void
seed_params(int trial)
{
	unsigned lfsr = 0x51edu + 0x4f1bu * (unsigned)trial;
	unsigned i;

	for (i = 0; i < sizeof(params.raw); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		params.raw[i] = (unsigned char)(lfsr >> 5);
	}
	((V90Parameters *)params.raw)->V92_JD_PHASE = jd_phase[trial % NPHASE];
}

#define NCTOR 32

static int
run_ctor(void)
{
	unsigned char first_mask[16], first_phase[16];
	int trial, mask_distinct = 0, phase_distinct = 0, moved = 0;

	diff_begin("V92Jd::V92Jd(V90Parameters *)");

	for (trial = 0; trial < NCTOR; trial++) {
		unsigned char before[SLOT];
		V90Parameters *p = (V90Parameters *)params.raw;

		seed(trial, trial % 4);
		seed_params(trial);
		memcpy(before, ours.raw, SLOT);

		our_ctor1(&OURS, p);
		ref_ctor1(&THEIRS, p);

		diff_eq_obj("after V92Jd(params) [C1]", V92Jd, &OURS, &THEIRS,
			    trial);
		diff_eq_int("no store past the object, C1 (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0) {
			memcpy(first_mask, &OURS.bits[V90JD_GROUP1 + 1], 16);
			memcpy(first_phase, &OURS.phaseBits[V90JD_GROUP1 + 1],
			       16);
		} else {
			if (memcmp(first_mask, &OURS.bits[V90JD_GROUP1 + 1],
				   16))
				mask_distinct = 1;
			if (memcmp(first_phase,
				   &OURS.phaseBits[V90JD_GROUP1 + 1], 16))
				phase_distinct = 1;
		}

		seed(trial, trial % 4);
		our_ctor2(&OURS, p);
		ref_ctor2(&THEIRS, p);

		diff_eq_obj("after V92Jd(params) [C2]", V92Jd, &OURS, &THEIRS,
			    trial);
		diff_eq_int("no store past the object, C2 (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("C1 and C2 agree (trial %ld)",
			    memcmp(ours.raw, theirs.raw, SLOT), 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the rate mask is not the same on every trial",
		    mask_distinct, 1, 0);
	diff_eq_int("the Jd phase is not the same on every trial",
		    phase_distinct, 1, 0);

	return diff_end();
}

/* One byte of `ret`; the check is that it stays that way.  See t_v90jd.cpp. */
static int
run_dtor(void)
{
	int trial;

	diff_begin("V92Jd::~V92Jd");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		our_dtor(&OURS);
		ref_dtor(&THEIRS);

		diff_eq_obj("after ~V92Jd", V92Jd, &OURS, &THEIRS, trial);
		diff_eq_int("~V92Jd wrote nothing (trial %ld)",
			    memcmp(before, ours.raw, SLOT), 0, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
	}

	return diff_end();
}

/*
 * ===========================================================================
 * The four accessors, swept over the fields they decode.
 *
 * THEY DO NOT ALL READ THE SAME VECTOR, which is the property this fixture
 * exists to hold: `getRatesMask` and `getMaxLookahead` read `bits`,
 * `getJdPhase` reads `phaseBits[0..15]`, and `getConstelationSize` reads
 * `phaseBits[29..30]` -- one vector further along than V90Jd's, and one index
 * further along than that (D270, D271).  Both vectors are painted
 * INDEPENDENTLY here, with different values, so a reconstruction that read the
 * right index in the wrong vector diverges instead of agreeing.
 *
 * THE PHASE IS COMPARED AS BITS, not as a float, so that a sign or a rounding
 * difference cannot hide inside a tolerance -- the contract is bit-exactness
 * and 2**-16 scaling is exact for every one of the 65,536 inputs.  Every one
 * of the sixteen positions is driven alone, so a phase byte read from the
 * wrong place fails on its own trial.
 * ===========================================================================
 */
static void
paint_v92_payload(unsigned int mask, unsigned int phase, unsigned char c0,
		  unsigned char c1, unsigned char l0, unsigned char l1,
		  int style)
{
	static const unsigned char set_byte[4] = { 0x01, 0x02, 0x80, 0xff };
	unsigned char one = set_byte[style & 3];
	int i;

	for (i = 0; i < 28; i++) {
		unsigned char v = ((mask >> i) & 1u) != 0 ? one : 0x00;

		OURS.bits[i] = v;
		THEIRS.bits[i] = v;
	}
	OURS.bits[30] = THEIRS.bits[30] = l0;
	OURS.bits[31] = THEIRS.bits[31] = l1;

	for (i = 0; i < 16; i++) {
		unsigned char v = ((phase >> i) & 1u) != 0 ? one : 0x00;

		OURS.phaseBits[i] = v;
		THEIRS.phaseBits[i] = v;
	}
	OURS.phaseBits[29] = THEIRS.phaseBits[29] = c0;
	OURS.phaseBits[30] = THEIRS.phaseBits[30] = c1;
}

#define NACC 64

static int
run_accessors(void)
{
	int trial;
	int first_mask = 0, first_look = -1;
	unsigned first_phase = 0;
	int mask_distinct = 0, mask_nonzero = 0, look_distinct = 0;
	int phase_distinct = 0, phase_nonzero = 0;
	int look_same_bits = 0, look_diff_bits = 0;

	diff_begin("V92Jd::getJdPhase / getRatesMask / getConstelationSize / "
		   "getMaxLookahead");

	for (trial = 0; trial < NACC; trial++) {
		unsigned char before[SLOT];
		unsigned char pa[8], pb[8];
		unsigned int mask, phase, abits, bbits;
		unsigned char c0, c1, l0, l1;
		float af, bf;
		int am, bm;
		unsigned int i;

		seed(trial, trial % 4);

		if (trial < 28)
			mask = 1u << trial;
		else if (trial < 32)
			mask = 0x0ffffff0u >> (trial - 28);
		else
			mask = 0x9e3779b9u * (unsigned)(trial + 1);

		if (trial < 16)
			phase = 1u << trial;		/* one position   */
		else if (trial < 20)
			phase = 0xffffu >> (trial - 16);
		else
			phase = 0x85ebca6bu * (unsigned)(trial + 3);
		phase &= 0xffffu;

		c0 = (unsigned char)(trial * 37u + 1u);
		c1 = (unsigned char)(trial * 53u + 7u);
		/*
		 * Bit 1 of the trial number is folded into the second
		 * lookahead byte so that the two bytes' LOW BITS take all four
		 * combinations; two odd multiples of `trial` would share bit 0
		 * on every trial and a transposition of the pair would be
		 * invisible.  t_v90jd.cpp carries the argument -- it was the
		 * mutation tier that found it, in that file.
		 */
		l0 = (unsigned char)(trial * 11u);
		l1 = (unsigned char)(trial * 29u + (((unsigned)trial >> 1) & 1u));
		paint_v92_payload(mask, phase, c0, c1, l0, l1, trial);
		memcpy(before, ours.raw, SLOT);

		for (i = 0; i < sizeof(pa); i++) {
			pa[i] = (unsigned char)(0x60u + i + trial);
			pb[i] = pa[i];
		}

		af = OURS.getJdPhase();
		bf = ref_getJdPhase(&THEIRS);
		memcpy(&abits, &af, sizeof(abits));
		memcpy(&bbits, &bf, sizeof(bbits));
		diff_eq_int("getJdPhase bits (trial %ld)", (long)abits,
			    (long)bbits, trial);
		{
			/* Q16: the painted word over 65536, exactly. */
			float want = (float)phase / 65536.0f;
			unsigned wantbits;

			memcpy(&wantbits, &want, sizeof(wantbits));
			diff_eq_int("getJdPhase is the painted word / 65536 "
				    "(trial %ld)", (long)abits, (long)wantbits,
				    trial);
		}

		am = OURS.getRatesMask();
		bm = ref_getRatesMask(&THEIRS);
		diff_eq_int("getRatesMask (trial %ld)", am, bm, trial);
		diff_eq_int("getRatesMask decodes the painted mask (trial %ld)",
			    am, (int)(mask & 0x0fffffffu), trial);

		OURS.getConstelationSize(&pa[2], &pa[5]);
		ref_getConstelationSize(&THEIRS, &pb[2], &pb[5]);
		diff_eq_int("getConstelationSize wrote the same bytes "
			    "(trial %ld)", memcmp(pa, pb, sizeof(pa)), 0,
			    trial);
		diff_eq_int("getConstelationSize reads phaseBits[29] "
			    "(trial %ld)", pa[2], c0, trial);
		diff_eq_int("getConstelationSize reads phaseBits[30] "
			    "(trial %ld)", pa[5], c1, trial);

		{
			unsigned char al = OURS.getMaxLookahead();
			unsigned char bl = ref_getMaxLookahead(&THEIRS);

			diff_eq_int("getMaxLookahead (trial %ld)", al, bl,
				    trial);
			diff_eq_int("getMaxLookahead is the two low bits "
				    "(trial %ld)", al,
				    (l0 & 1) + ((l1 & 1) << 1), trial);
			if (trial == 0)
				first_look = al;
			else if (al != first_look)
				look_distinct = 1;
			if ((l0 & 1) == (l1 & 1))
				look_same_bits = 1;
			else
				look_diff_bits = 1;
		}

		diff_eq_obj("after the accessors", V92Jd, &OURS, &THEIRS,
			    trial);
		diff_eq_int("the accessors wrote nothing (trial %ld)",
			    memcmp(before, ours.raw, SLOT), 0, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (trial == 0) {
			first_mask = am;
			first_phase = abits;
		} else {
			if (am != first_mask)
				mask_distinct = 1;
			if (abits != first_phase)
				phase_distinct = 1;
		}
		if (am != 0)
			mask_nonzero = 1;
		if (af != 0.0f)
			phase_nonzero = 1;
	}

	diff_eq_int("the rate mask is not the same on every trial",
		    mask_distinct, 1, 0);
	diff_eq_int("the rate mask is not always zero", mask_nonzero, 1, 0);
	diff_eq_int("the phase is not the same on every trial", phase_distinct,
		    1, 0);
	diff_eq_int("the phase is not always zero", phase_nonzero, 1, 0);
	diff_eq_int("the lookahead is not the same on every trial",
		    look_distinct, 1, 0);
	diff_eq_int("the two lookahead bits were driven equal", look_same_bits,
		    1, 0);
	diff_eq_int("and unequal, so a transposition is visible",
		    look_diff_bits, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_packs();
	rc |= run_getters();
	rc |= run_resets();
	rc |= run_ctor();
	rc |= run_dtor();
	rc |= run_accessors();

	return rc;
}
