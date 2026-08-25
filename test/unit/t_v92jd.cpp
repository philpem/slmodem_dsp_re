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

/*
 * The two unpackers.  `int` is V92Jd.cpp's reading of `%eax`, not the
 * mangling's -- declaring it here is part of the check, because a blob-side
 * function that left `%eax` alone would return whatever the call left there.
 */
int ref_unPackJdData(void *self, int bit) asm("ref__ZN5V92Jd12unPackJdDataEi");
int ref_unPackJdPhaseData(void *self, int bit)
	asm("ref__ZN5V92Jd17unPackJdPhaseDataEi");
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

/*
 * ===========================================================================
 * The two unpackers, driven as round trips against the two packs.
 *
 * t_v90jd.cpp's header carries the argument for the shape -- the CRC check is
 * half of each function and only a real message reaches it, so the message is
 * built by the pack this file already tests and fed back one bit at a time,
 * with the return value, the whole object and the guard compared after EVERY
 * call.  What this file adds is the three differences between the three
 * unpackers, each of which gets a probe of its own:
 *
 * THE CROSS FEED IS THE TAG CHECK, ISOLATED.  Each unpacker refuses a
 * completed message unless its own constant byte is right: `bits[28] == 0`
 * for the data one and `phaseBits[28] == 1` for the phase one, which is
 * framed position 47 in each -- the byte `V92Jd::V92Jd` writes as 0 and 1
 * respectively and neither pack touches.  Everything else about the two
 * messages is interchangeable: the framing is the same, the CRC covers the
 * same two runs, so a data message fed to the PHASE unpacker walks all the
 * way through, passes the CRC, and is then thrown away on that one byte.
 * Both directions of that are driven below, and both must reset and return 0
 * at bit 72 rather than complete.
 *
 * THE 11 + 1 SPLIT IS VISIBLE IN `unpackWord`, not in `bits`.  The data
 * unpacker takes its second rate run as eleven bytes and then one more in a
 * state of its own, where V.90's and the phase one take twelve in a single
 * state; the BYTES written are identical, so only the state number tells them
 * apart, and `diff_eq_obj` covers +0xd4.  The seeded sweep drives state 5 and
 * state 9 directly, which exist in the data unpacker and not in the phase
 * one.
 *
 * THE TWO VECTORS AND THE SHARED CRC REGISTER.  The data unpacker writes
 * `bits`, the phase one `phaseBits`, and both scribble on the same sixteen
 * ints at +0x94 while checking -- so a run that unpacks a data message and
 * then a phase message compares the register the second one left, which is
 * the sharing V92Jd.h records for the two packs.
 * ===========================================================================
 */
static int uk_moved, uk_complete, uk_zero;

/* Payload index to its position in the framed message; the same in both. */
static int
framed_of(int p)
{
	if (p < 16)
		return V90JD_GROUP1 + 1 + p;
	if (p < 32)
		return V90JD_GROUP2 + 1 + (p - 16);
	return V90JD_GROUP3 + 1 + (p - 32);
}

#define JD_DATA		0
#define JD_PHASE	1

static int
feed_one(int which, int bit, long sample)
{
	unsigned char before[SLOT];
	int ra, rb;

	memcpy(before, ours.raw, SLOT);
	if (which == JD_PHASE) {
		ra = OURS.unPackJdPhaseData(bit);
		rb = ref_unPackJdPhaseData(&THEIRS, bit);
	} else {
		ra = OURS.unPackJdData(bit);
		rb = ref_unPackJdData(&THEIRS, bit);
	}

	diff_eq_int("unpacker return value (sample %ld)", ra, rb, sample);
	diff_eq_obj("after the unpacker", V92Jd, &OURS, &THEIRS, sample);
	diff_eq_int("no store past the object (sample %ld)", guard_equal(), 1,
		    sample);

	if (memcmp(before, ours.raw, SLOT) != 0)
		uk_moved = 1;
	if (ra)
		uk_complete++;
	else
		uk_zero++;
	return ra;
}

static void
start(int which, int trial)
{
	seed(trial, trial % 4);
	if (which == JD_PHASE) {
		OURS.unPackJdPhaseReset();
		ref_unPackJdPhaseReset(&THEIRS);
	} else {
		OURS.unPackJdReset();
		ref_unPackJdReset(&THEIRS);
	}
}

/* The state word each direction switches on, for the assertions below. */
static int
state_of(int which)
{
	return which == JD_PHASE ? OURS.unpackPhaseWord : OURS.unpackWord;
}

/* The vector each direction fills. */
static const unsigned char *
vector_of(int which)
{
	return which == JD_PHASE ? OURS.phaseBits : OURS.bits;
}

#define NMSG 12

static int
run_unpackers(void)
{
	int trial, which;
	int first_mask = 0, mask_distinct = 0, late_complete = 0;
	int crossed = 0;

	uk_moved = 0;
	uk_complete = 0;
	uk_zero = 0;

	diff_begin("V92Jd::unPackJdData / unPackJdPhaseData");

	for (trial = 0; trial < NMSG; trial++) {
		unsigned char msg[2][V90JD_BITS];
		unsigned char expect[48];
		int wire[V90JD_BITS];
		V90Parameters *p = (V90Parameters *)params.raw;
		int i, rc;

		/* One object, both messages, straight off the two packs. */
		seed(trial, trial % 4);
		seed_params(trial);
		our_ctor1(&OURS, p);
		OURS.getJdBitVector();
		memcpy(msg[JD_DATA], OURS.bits, V90JD_BITS);
		OURS.getJdPhaseBitVector();
		memcpy(msg[JD_PHASE], OURS.phaseBits, V90JD_BITS);

		for (which = 0; which < 2; which++) {
			long base = ((long)trial * 2 + which) * 10000L;
			const unsigned char *m = msg[which];

			for (i = 0; i < 48; i++)
				expect[i] = m[framed_of(i)];

			/* (1) The message, one bit at a time. */
			start(which, trial);
			for (i = 0; i < V90JD_BITS; i++) {
				rc = feed_one(which, m[i], base + i);
				diff_eq_int("completes on the last bit and no "
					    "other (sample %ld)", rc,
					    i == V90JD_BITS - 1, base + i);
			}

			diff_eq_int("the unpacker left the payload flat at "
				    "index 0 (sample %ld)",
				    memcmp(vector_of(which), expect, 48), 0,
				    base);
			diff_eq_int("the counter stopped at 0x34 (sample %ld)",
				    OURS.unpack[1], 0x34, base);
			diff_eq_int("the state stopped at its last (sample "
				    "%ld)", state_of(which),
				    which == JD_PHASE ? 8 : 9, base);
			diff_eq_int("and the tag byte was what its unpacker "
				    "demands (sample %ld)",
				    vector_of(which)[28],
				    which == JD_PHASE ? 1 : 0, base);

			/* (2) Past the end: the counter runs on as a byte. */
			for (i = 0; i < 300; i++)
				if (feed_one(which, i & 1, base + 100 + i))
					late_complete = 1;

			/* (3) One payload bit flipped: the CRC rejects it. */
			start(which, trial);
			for (i = 0; i < V90JD_BITS; i++)
				wire[i] = m[i];
			wire[framed_of(trial % 48)] ^= 1;
			for (i = 0; i < V90JD_BITS; i++)
				diff_eq_int("a corrupt message never completes"
					    " (sample %ld)",
					    feed_one(which, wire[i],
						     base + 400 + i), 0,
					    base + 400 + i);
			diff_eq_int("and the corrupt message reset it "
				    "(sample %ld)", state_of(which), 0, base);

			/* (4) A CRC byte of 2 or 3: a magnitude, not a bit. */
			start(which, trial);
			for (i = 0; i < V90JD_BITS; i++)
				wire[i] = m[i];
			wire[framed_of(32 + trial % 16)] |= 2;
			for (i = 0; i < V90JD_BITS; i++)
				diff_eq_int("a CRC byte of 2 is rejected "
					    "(sample %ld)",
					    feed_one(which, wire[i],
						     base + 800 + i), 0,
					    base + 800 + i);
			diff_eq_int("and the high CRC byte reset it (sample "
				    "%ld)", state_of(which), 0, base);

			/* (5) Every 1 sent as 0x100: truthy, but stores 0. */
			start(which, trial);
			for (i = 0; i < V90JD_BITS; i++)
				diff_eq_int("a payload of 0x100 never "
					    "completes (sample %ld)",
					    feed_one(which,
						     m[i] ? 0x100 : 0,
						     base + 1200 + i), 0,
					    base + 1200 + i);
			diff_eq_int("0x100 walked the framing and stored "
				    "zeros (sample %ld)", state_of(which), 0,
				    base);

			/* (6) A 1 where group 1's marker belongs. */
			start(which, trial);
			for (i = 0; i < V90JD_BITS; i++)
				wire[i] = m[i];
			wire[V90JD_GROUP1] = (trial & 1) ? 0x100 : 1;
			for (i = 0; i < V90JD_BITS; i++)
				feed_one(which, wire[i], base + 1600 + i);
			diff_eq_int("a 1 at group 1's marker restarts the "
				    "hunt (sample %ld)", state_of(which), 0,
				    base);

			/*
			 * (7) THE OTHER DIRECTION'S MESSAGE.  It walks the
			 * whole framing and passes the CRC -- the two messages
			 * differ only in what they carry -- and is refused on
			 * the tag byte alone.
			 */
			start(which, trial);
			for (i = 0; i < V90JD_BITS; i++)
				diff_eq_int("the other direction's message "
					    "never completes (sample %ld)",
					    feed_one(which,
						     msg[1 - which][i],
						     base + 2000 + i), 0,
					    base + 2000 + i);
			diff_eq_int("the wrong tag reset it at the last bit "
				    "(sample %ld)", state_of(which), 0, base);
			crossed++;
		}

		/*
		 * The accessors, on the object the two round trips left.  The
		 * data message carries the rate mask and the lookahead; the
		 * phase message carries the phase and the constellation size.
		 * Both are read out of the WIRE bytes, not out of our own
		 * object, so this is an oracle the blob does not supply.
		 */
		{
			unsigned char c0 = 0, c1 = 0;
			unsigned int q = 0;
			int mask = 0;

			start(JD_DATA, trial);
			for (i = 0; i < V90JD_BITS; i++)
				feed_one(JD_DATA, msg[JD_DATA][i],
					 (long)trial * 100000L + i);
			for (i = 0; i < 28; i++)
				if (msg[JD_DATA][framed_of(i)])
					mask |= 1 << i;
			diff_eq_int("getRatesMask reads the unpacked data "
				    "payload (trial %ld)", OURS.getRatesMask(),
				    mask, trial);
			diff_eq_int("getMaxLookahead reads bits[30..31] "
				    "(trial %ld)", OURS.getMaxLookahead(),
				    (msg[JD_DATA][framed_of(30)] & 1) +
				    ((msg[JD_DATA][framed_of(31)] & 1) << 1),
				    trial);
			if (trial == 0)
				first_mask = mask;
			else if (mask != first_mask)
				mask_distinct = 1;

			/*
			 * Then the phase message into the same object.  It
			 * fills the other vector and rewrites the shared CRC
			 * register, and `bits` still holds what the data
			 * message left, so both pairs of accessors answer.
			 */
			OURS.unPackJdPhaseReset();
			ref_unPackJdPhaseReset(&THEIRS);
			for (i = 0; i < V90JD_BITS; i++)
				feed_one(JD_PHASE, msg[JD_PHASE][i],
					 (long)trial * 100000L + 200 + i);
			for (i = 0; i < 16; i++)
				if (msg[JD_PHASE][framed_of(i)])
					q |= 1u << i;
			diff_eq_int("getJdPhase reads the unpacked phase "
				    "payload (trial %ld)",
				    (int)(OURS.getJdPhase() * 65536.0f),
				    (int)q, trial);
			OURS.getConstelationSize(&c0, &c1);
			diff_eq_int("getConstelationSize reads phaseBits[29] "
				    "(trial %ld)", c0,
				    msg[JD_PHASE][framed_of(29)], trial);
			diff_eq_int("getConstelationSize reads phaseBits[30] "
				    "(trial %ld)", c1,
				    msg[JD_PHASE][framed_of(30)], trial);
			diff_eq_int("the rate mask survived the phase unpack "
				    "(trial %ld)", OURS.getRatesMask(), mask,
				    trial);
		}
	}

	diff_eq_int("the unpackers changed the object", uk_moved, 1, 0);
	diff_eq_int("they completed a message", uk_complete >= 2 * NMSG, 1, 0);
	diff_eq_int("and again after the counter wrapped", late_complete, 1,
		    0);
	diff_eq_int("and returned 0 far more often", uk_zero > uk_complete, 1,
		    0);
	diff_eq_int("both cross feeds were driven", crossed, 2 * NMSG, 0);
	diff_eq_int("the recovered rate mask is not the same on every trial",
		    mask_distinct, 1, 0);

	return diff_end();
}

/*
 * Both unpackers from seeded state: every state either table has and the
 * three outside it, every counter value that is a boundary in some state --
 * 26 and 27 are the data unpacker's split and belong to neither of the other
 * two -- four run lengths including the pair around the byte wrap, and an
 * argument domain wide enough to tell `test %ecx,%ecx` from a byte test.
 * `unpack[1]` is held below 72 so that the unbounded `vec[unpack[1]] = bit`
 * stays inside the vector it is meant to fill.
 */
static int
run_unpackers_states(void)
{
	static const int states[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 100,
				      -1, 0x7fffffff };
	static const int counters[] = { 0, 15, 16, 26, 27, 31, 47, 51, 71 };
	static const int runs[] = { 0, 16, 254, 255 };
	static const int args[] = { 0, 1, 2, 0x80, 0xff, 0x100, 0x101, -1,
				    0x7fffffff, (int)0x80000000u };
	int which, si, ci, ri, ai;
	long sample = 0;

	uk_moved = 0;
	uk_complete = 0;
	uk_zero = 0;

	diff_begin("V92Jd's two unpackers over seeded state");

	for (which = 0; which < 2; which++)
	    for (si = 0; si < (int)(sizeof(states) / sizeof(states[0])); si++)
		for (ci = 0; ci < (int)(sizeof(counters) / sizeof(counters[0]));
		     ci++)
		    for (ri = 0; ri < (int)(sizeof(runs) / sizeof(runs[0]));
			 ri++)
			for (ai = 0;
			     ai < (int)(sizeof(args) / sizeof(args[0])); ai++) {
				seed((int)sample, (int)(sample % 4));
				/*
				 * BOTH state words are forced on both sides,
				 * not just the one this direction reads: the
				 * other is part of the object and a
				 * reconstruction that switched on the wrong
				 * one has to be told them apart.
				 */
				OURS.unpackWord = THEIRS.unpackWord =
				    which == JD_PHASE ? 3 : states[si];
				OURS.unpackPhaseWord = THEIRS.unpackPhaseWord =
				    which == JD_PHASE ? states[si] : 3;
				OURS.unpack[1] = THEIRS.unpack[1] =
				    (unsigned char)counters[ci];
				OURS.unpack[0] = THEIRS.unpack[0] =
				    (unsigned char)runs[ri];
				/*
				 * HALF THE SWEEP GETS A VALID TAG BYTE.
				 * Seeded storage almost never satisfies
				 * `bits[28] == 0` and `phaseBits[28] == 1` at
				 * once, so without this the completion path
				 * is never reached from seeded state at all
				 * -- which is how the first spelling of this
				 * run failed its own anti-vacuity check
				 * (findings F223, F224).  Alternating leaves
				 * the other half refusing on the tag.
				 */
				if ((sample & 1) == 0) {
					OURS.bits[28] = THEIRS.bits[28] = 0;
					OURS.phaseBits[28] =
					    THEIRS.phaseBits[28] = 1;
				}
				feed_one(which, args[ai], sample);
				sample++;
			}

	diff_eq_int("the seeded sweep changed the object", uk_moved, 1, 0);
	diff_eq_int("the seeded sweep completed a message at least once",
		    uk_complete > 0, 1, 0);
	diff_eq_int("and did not complete on every call", uk_zero > 0, 1, 0);

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
	rc |= run_unpackers();
	rc |= run_unpackers_states();

	return rc;
}
