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
 * The unpacker.  `int` is V90Jd.cpp's reading of `%eax` and not the
 * mangling's, so declaring it that way here is itself a check: a blob-side
 * function that left `%eax` alone would hand this fixture whatever the call
 * left there, and our side's genuine 0 would not match it.
 */
int ref_unPackData(void *self, int bit) asm("ref__ZN5V90Jd10unPackDataEi");

/*
 * The three accessors.  Their return types are the ones V90Jd.cpp derives
 * from the object -- `int`, void and `unsigned char` -- and the declarations
 * here are what would fail if that reading were wrong on the blob's side: a
 * function that left `%eax` alone would hand this fixture whatever the call
 * left there, which varies between the two calls and would not compare equal.
 */
int ref_getRatesMask(void *self) asm("ref__ZN5V90Jd12getRatesMaskEv");
void ref_getConstelationSize(void *self, unsigned char *first,
			     unsigned char *second)
	asm("ref__ZN5V90Jd19getConstelationSizeEPhS0_");
unsigned char ref_getMaxLookahead(void *self)
	asm("ref__ZN5V90Jd15getMaxLookaheadEv");

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

/*
 * ===========================================================================
 * The three accessors, swept over the fields they decode.
 *
 * THE SWEEP IS OVER THE MESSAGE, NOT OVER THE SEED.  A seeded vector alone
 * would drive `getRatesMask` with 28 bytes that are non-zero about 255 times
 * in 256, so the answer would be 0x0fffffff on nearly every trial and a
 * reconstruction that read the wrong 28 bytes would agree with the blob
 * anyway.  Every position is therefore driven on its own, then in blocks, then
 * pseudorandomly, with the REST of the object still holding seeded bytes so
 * that a read one byte outside the payload diverges.
 *
 * WHAT A SET BIT LOOKS LIKE IN THE STORAGE IS PART OF THE SWEEP.  The object
 * reads the 28 mask bytes with `cmpb $0x0` and the two lookahead bytes with
 * `and $0x1`, so a byte of 2 sets a rate bit and clears a lookahead one.  A
 * fixture that only ever stored 0 and 1 could not tell those two rules apart,
 * and `mask & 1` in place of `mask != 0` is a mutation in v90jd.json.
 *
 * The accessors are also asserted to write NOTHING -- neither to the object
 * nor past the two bytes `getConstelationSize` is given.
 * ===========================================================================
 */
static void
paint_payload(unsigned int mask, unsigned char c0, unsigned char c1,
	      unsigned char l0, unsigned char l1, int style)
{
	static const unsigned char set_byte[4] = { 0x01, 0x02, 0x80, 0xff };
	unsigned char one = set_byte[style & 3];
	int i;

	for (i = 0; i < 28; i++) {
		unsigned char v = ((mask >> i) & 1u) != 0 ? one : 0x00;

		OURS.bits[i] = v;
		THEIRS.bits[i] = v;
	}
	OURS.bits[28] = THEIRS.bits[28] = c0;
	OURS.bits[29] = THEIRS.bits[29] = c1;
	OURS.bits[30] = THEIRS.bits[30] = l0;
	OURS.bits[31] = THEIRS.bits[31] = l1;
}

#define NACC 64

static int
run_accessors(void)
{
	int trial;
	int first_mask = 0, first_look = -1;
	int mask_distinct = 0, mask_nonzero = 0, look_distinct = 0;
	int look_same_bits = 0, look_diff_bits = 0;

	diff_begin("V90Jd::getRatesMask / getConstelationSize / getMaxLookahead");

	for (trial = 0; trial < NACC; trial++) {
		unsigned char before[SLOT];
		unsigned char pa[8], pb[8];
		unsigned int mask;
		unsigned char c0, c1, l0, l1;
		int am, bm;
		unsigned int i;

		seed(trial, trial % 4);

		if (trial < 28)
			mask = 1u << trial;		/* one position   */
		else if (trial < 32)
			mask = 0x0ffffff0u >> (trial - 28);
		else
			mask = 0x9e3779b9u * (unsigned)(trial + 1);

		c0 = (unsigned char)(trial * 37u + 1u);
		c1 = (unsigned char)(trial * 53u + 7u);
		/*
		 * THE TWO LOOKAHEAD BYTES MUST NOT SHARE A LOW BIT, and the
		 * first spelling of this fixture did: `trial * 11` and
		 * `trial * 29 + 2` are both odd multiples, so bit 0 was
		 * `trial & 1` in both and TRANSPOSING THE PAIR CHANGED
		 * NOTHING.  The mutation tier caught the fixture rather than
		 * the code -- v90jd.json's "getMaxLookahead transposes the two
		 * bits" read NOT CAUGHT.  Bit 1 of the trial number is folded
		 * into the second byte so that all four combinations occur,
		 * and the two coverage counters below refuse to let the blind
		 * spot come back quietly.
		 */
		l0 = (unsigned char)(trial * 11u);
		l1 = (unsigned char)(trial * 29u + (((unsigned)trial >> 1) & 1u));
		paint_payload(mask, c0, c1, l0, l1, trial);
		memcpy(before, ours.raw, SLOT);

		for (i = 0; i < sizeof(pa); i++) {
			pa[i] = (unsigned char)(0x60u + i + trial);
			pb[i] = pa[i];
		}

		am = OURS.getRatesMask();
		bm = ref_getRatesMask(&THEIRS);
		diff_eq_int("getRatesMask (trial %ld)", am, bm, trial);
		/*
		 * And an oracle the blob does not supply: the 28 bits that were
		 * painted, and nothing above them.  This is what says the
		 * fixture drove what it meant to drive.
		 */
		diff_eq_int("getRatesMask decodes the painted mask (trial %ld)",
			    am, (int)(mask & 0x0fffffffu), trial);

		OURS.getConstelationSize(&pa[2], &pa[5]);
		ref_getConstelationSize(&THEIRS, &pb[2], &pb[5]);
		diff_eq_int("getConstelationSize wrote the same bytes "
			    "(trial %ld)", memcmp(pa, pb, sizeof(pa)), 0,
			    trial);
		diff_eq_int("getConstelationSize first out (trial %ld)",
			    pa[2], c0, trial);
		diff_eq_int("getConstelationSize second out (trial %ld)",
			    pa[5], c1, trial);

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

		diff_eq_obj("after the accessors", V90Jd, &OURS, &THEIRS,
			    trial);
		diff_eq_int("the accessors wrote nothing (trial %ld)",
			    memcmp(before, ours.raw, SLOT), 0, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (trial == 0)
			first_mask = am;
		else if (am != first_mask)
			mask_distinct = 1;
		if (am != 0)
			mask_nonzero = 1;
	}

	diff_eq_int("the rate mask is not the same on every trial",
		    mask_distinct, 1, 0);
	diff_eq_int("the rate mask is not always zero", mask_nonzero, 1, 0);
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
 * The unpacker, driven as a state machine and as a round trip.
 *
 * THE ROUND TRIP IS THE ONLY THING THAT REACHES THE CRC.  Roughly half of
 * `unPackData`'s 879 bytes is the check it runs when the sixteenth CRC bit
 * arrives, and that code is unreachable unless the preceding fifty-two bits
 * walked the framing exactly: seventeen 1 bits, a 0, sixteen, a 0, sixteen, a
 * 0.  A sequence of random bits would never get there.  So the message is
 * built by the packer this file already tests -- construct from parameters,
 * `getBitVector()`, take the 72 bytes -- and fed back in one bit at a time.
 * It closes: the packer's two sixteen-byte CRC runs at framed 18 and 35 are
 * the unpacker's single thirty-two-byte run over payload 0..31, so the
 * register agrees and the message is accepted on its 72nd bit.
 *
 * AND THE ROUND TRIP IS THE ANSWER TO D270.  `expect[]` below is the payload
 * lifted out of the WIRE message by the framing rule -- not by reading our
 * own object -- and the run asserts that `bits[0..47]` holds exactly it, then
 * that the three accessors decode it.  That is an oracle the blob does not
 * supply, and it is what says the payload-contiguous layout has a producer.
 *
 * EVERY CALL IS COMPARED, not just the last: the return value, the whole
 * object and the guard past its end.  A divergence that corrects itself
 * later is still a divergence.
 *
 * THE FOUR CORRUPTIONS each aim at one instruction:
 *
 *   - a payload bit flipped        the CRC must not be ignored
 *   - a CRC byte or'd with 2       `abs(crc[i] - bits[32+i])`, not `& 1`
 *   - every 1 sent as 0x100        the store is `mov %cl` -- the LOW BYTE --
 *                                  while the marker tests are `test %ecx`
 *                                  on the whole word, so the message walks
 *                                  the framing and stores zeros
 *   - a 1 at a group marker        the restart, and after it no run of
 *                                  seventeen exists in the rest of the
 *                                  message (the longest is sixteen), so the
 *                                  state stays 0 to the end
 * ===========================================================================
 */
static int uk_moved, uk_complete, uk_zero;

/* Payload index to its position in the framed message. */
static int
framed_of(int p)
{
	if (p < 16)
		return V90JD_GROUP1 + 1 + p;
	if (p < 32)
		return V90JD_GROUP2 + 1 + (p - 16);
	return V90JD_GROUP3 + 1 + (p - 32);
}

static int
feed_one(int bit, long sample)
{
	unsigned char before[SLOT];
	int ra, rb;

	memcpy(before, ours.raw, SLOT);
	ra = OURS.unPackData(bit);
	rb = ref_unPackData(&THEIRS, bit);

	diff_eq_int("unPackData return value (sample %ld)", ra, rb, sample);
	diff_eq_obj("after unPackData", V90Jd, &OURS, &THEIRS, sample);
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

/* Seeded storage on both sides, then the unpacker's own reset. */
static void
start(int trial)
{
	seed(trial, trial % 4);
	OURS.unPackReset();
	ref_unPackReset(&THEIRS);
}

#define NMSG 16

static int
run_unpackdata(void)
{
	int trial;
	int first_mask = 0, mask_distinct = 0, late_complete = 0;

	uk_moved = 0;
	uk_complete = 0;
	uk_zero = 0;

	diff_begin("V90Jd::unPackData");

	for (trial = 0; trial < NMSG; trial++) {
		unsigned char msg[V90JD_BITS];
		unsigned char expect[48];
		int wire[V90JD_BITS];
		V90Parameters *p = (V90Parameters *)params.raw;
		long base = (long)trial * 1000L;
		int i, rc, mask;
		unsigned char c0 = 0, c1 = 0;

		/* A real message, straight off the packer. */
		seed(trial, trial % 4);
		seed_params(trial);
		our_ctor1(&OURS, p);
		OURS.getBitVector();
		memcpy(msg, OURS.bits, V90JD_BITS);
		for (i = 0; i < 48; i++)
			expect[i] = msg[framed_of(i)];

		/* (1) The message, one bit at a time. */
		start(trial);
		for (i = 0; i < V90JD_BITS; i++) {
			rc = feed_one(msg[i], base + i);
			diff_eq_int("completes on the last bit and no other "
				    "(sample %ld)", rc,
				    i == V90JD_BITS - 1, base + i);
		}

		diff_eq_int("the unpacker left the payload flat at bits[0] "
			    "(trial %ld)", memcmp(OURS.bits, expect, 48), 0,
			    trial);
		diff_eq_int("the counter stopped at 0x34 (trial %ld)",
			    OURS.unpack[1], 0x34, trial);
		diff_eq_int("the state stopped at 8 (trial %ld)",
			    OURS.unpackWord, 8, trial);

		/* And the accessors decode what it left. */
		mask = 0;
		for (i = 0; i < 28; i++)
			if (expect[i])
				mask |= 1 << i;
		diff_eq_int("getRatesMask reads the unpacked payload "
			    "(trial %ld)", OURS.getRatesMask(), mask, trial);
		OURS.getConstelationSize(&c0, &c1);
		diff_eq_int("getConstelationSize reads bits[28] (trial %ld)",
			    c0, expect[28], trial);
		diff_eq_int("getConstelationSize reads bits[29] (trial %ld)",
			    c1, expect[29], trial);
		diff_eq_int("getMaxLookahead reads bits[30..31] (trial %ld)",
			    OURS.getMaxLookahead(),
			    (expect[30] & 1) + ((expect[31] & 1) << 1), trial);

		if (trial == 0)
			first_mask = mask;
		else if (mask != first_mask)
			mask_distinct = 1;

		/*
		 * (2) Keep going past the end.  The state stays at 8 and the
		 * counter runs on as a BYTE, so it wraps through 255 and fires
		 * again 256 bits later.
		 */
		for (i = 0; i < 300; i++)
			if (feed_one(i & 1, base + 100 + i))
				late_complete = 1;

		/* (3) One payload bit flipped: the CRC must reject it. */
		start(trial);
		for (i = 0; i < V90JD_BITS; i++)
			wire[i] = msg[i];
		wire[framed_of(trial % 48)] ^= 1;
		for (i = 0; i < V90JD_BITS; i++)
			diff_eq_int("a corrupt message never completes "
				    "(sample %ld)", feed_one(wire[i],
				    base + 400 + i), 0, base + 400 + i);
		diff_eq_int("and the corrupt message reset it (trial %ld)",
			    OURS.unpackWord, 0, trial);

		/* (4) A CRC byte of 2 or 3: the compare is a magnitude. */
		start(trial);
		for (i = 0; i < V90JD_BITS; i++)
			wire[i] = msg[i];
		wire[framed_of(32 + trial % 16)] |= 2;
		for (i = 0; i < V90JD_BITS; i++)
			diff_eq_int("a CRC byte of 2 is rejected (sample %ld)",
				    feed_one(wire[i], base + 800 + i), 0,
				    base + 800 + i);
		diff_eq_int("and the high CRC byte reset it (trial %ld)",
			    OURS.unpackWord, 0, trial);

		/* (5) Every 1 sent as 0x100: truthy, but it stores 0. */
		start(trial);
		for (i = 0; i < V90JD_BITS; i++)
			diff_eq_int("a payload of 0x100 never completes "
				    "(sample %ld)",
				    feed_one(msg[i] ? 0x100 : 0,
					     base + 1200 + i), 0,
				    base + 1200 + i);
		diff_eq_int("0x100 walked the framing and stored zeros "
			    "(trial %ld)", OURS.unpackWord, 0, trial);

		/* (6) A 1 -- or a 0x100 -- where a group marker belongs. */
		start(trial);
		for (i = 0; i < V90JD_BITS; i++)
			wire[i] = msg[i];
		wire[V90JD_GROUP1] = (trial & 1) ? 0x100 : 1;
		for (i = 0; i < V90JD_BITS; i++)
			feed_one(wire[i], base + 1600 + i);
		diff_eq_int("a 1 at group 1's marker restarts the hunt "
			    "(trial %ld)", OURS.unpackWord, 0, trial);
	}

	diff_eq_int("unPackData changed the object", uk_moved, 1, 0);
	diff_eq_int("it completed a message", uk_complete >= NMSG, 1, 0);
	diff_eq_int("it completed again after the counter wrapped",
		    late_complete, 1, 0);
	diff_eq_int("and it returned 0 far more often", uk_zero > uk_complete,
		    1, 0);
	diff_eq_int("the recovered rate mask is not the same on every trial",
		    mask_distinct, 1, 0);

	return diff_end();
}

/*
 * The same function driven from seeded state rather than from a reset, over
 * every state the jump table has and the two either side of it, every counter
 * value that is a boundary in some state, four run lengths including the two
 * around the byte wrap, and an argument domain wide enough to tell
 * `test %ecx,%ecx` from a byte test.  `unpack[1]` is held below 72 so that
 * `bits[unpack[1]] = bit` stays inside the vector: the object bounds it
 * nowhere, and a seeded 200 would write 200 bytes past `this` on both sides
 * and prove nothing about either.
 */
static int
run_unpackdata_states(void)
{
	static const int states[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 100, -1,
				      0x7fffffff };
	static const int counters[] = { 0, 15, 16, 27, 31, 47, 51, 71 };
	static const int runs[] = { 0, 16, 254, 255 };
	static const int args[] = { 0, 1, 2, 0x80, 0xff, 0x100, 0x101, -1,
				    0x7fffffff, (int)0x80000000u };
	int si, ci, ri, ai;
	long sample = 0;

	uk_moved = 0;
	uk_complete = 0;
	uk_zero = 0;

	diff_begin("V90Jd::unPackData over seeded state");

	for (si = 0; si < (int)(sizeof(states) / sizeof(states[0])); si++)
		for (ci = 0; ci < (int)(sizeof(counters) / sizeof(counters[0]));
		     ci++)
			for (ri = 0;
			     ri < (int)(sizeof(runs) / sizeof(runs[0])); ri++)
				for (ai = 0;
				     ai < (int)(sizeof(args) / sizeof(args[0]));
				     ai++) {
					seed((int)sample, (int)(sample % 4));
					OURS.unpackWord = THEIRS.unpackWord =
					    states[si];
					OURS.unpack[1] = THEIRS.unpack[1] =
					    (unsigned char)counters[ci];
					OURS.unpack[0] = THEIRS.unpack[0] =
					    (unsigned char)runs[ri];
					feed_one(args[ai], sample);
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

	rc |= run_getbitvector();
	rc |= run_unpackreset();
	rc |= run_ctor();
	rc |= run_dtor();
	rc |= run_accessors();
	rc |= run_unpackdata();
	rc |= run_unpackdata_states();

	return rc;
}
