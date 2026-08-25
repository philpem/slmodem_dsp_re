/*
 * t_v90packdata.cpp -- differential test of V90Jd::packData.
 *
 * WHY IT IS ITS OWN BINARY.  t_v90jd already drives the rest of the class and
 * its mutation suite is recorded against it; packData landed afterwards and
 * carries its own suite (`v90packdata`), so keeping the two apart keeps each
 * suite's binary answering for exactly the code its rows mutate.
 *
 * THE FUNCTION HAS NO CALLER IN THE OBJECT, so nothing constrains its inputs
 * and this fixture has to choose them.  What it actually READS is narrow and
 * measured, not assumed: packData overwrites bits[0..16], bits[17], bits[34],
 * bits[51], bits[52..67] and bits[68..71], and it overwrites all sixteen ints
 * of the CRC register before using them.  Its only INPUTS are the two payload
 * runs, bits[18..33] and bits[35..50] -- thirty-two bytes.  Everything else
 * seeded here is a write-only observable rather than a driver, and is seeded
 * anyway because a byte that must not move is as much a claim as one that must.
 *
 * THE OBJECT IS NEVER ZEROED (finding 230's pattern, and 7105's trap).  Both
 * sides get the SAME varied pseudorandom bytes before every trial, so a
 * clear-loop one byte short cannot hide behind a zero that was already there,
 * and the CRC's input varies from trial to trial.  Nothing in packData's path
 * clears the seed before packData reads it: the only thing here that writes
 * the object before the call is the constructor, and it writes bits[18..50] --
 * which IS the input -- and the three unpacker fields, which this fixture then
 * re-dirties on purpose.
 *
 * FOUR THINGS ARE CLAIMED, AND EACH HAS ITS OWN CHECK:
 *
 *   * the whole 144-byte object agrees with the blob's, `diff_eq_obj`;
 *   * nothing is stored past it, the guard past `sizeof(V90Jd)`;
 *   * the three UNPACKER fields survive -- packData's last store is crc[15] at
 *     +0x88 and it never writes +0x00, +0x01 or +0x8c, so they are seeded
 *     non-zero and asserted unchanged; and
 *   * the CRC register is RESET, which is why it is seeded non-zero: a
 *     packData that skipped the reset would carry the seed into the answer.
 *
 * AND THE COVERAGE IS MEASURED RATHER THAN ASSERTED.  `run_reach` flips each
 * of the thirty-two payload positions in turn and requires the BLOB's own CRC
 * output to move.  That is what tells a 16/16 CRC from the 16/12 one a
 * reconstruction would write if it carried the CONSTRUCTOR's rate-mask split
 * across: under 16/12, bits[47..50] would reach nothing and four of the
 * thirty-two checks would fail.  A detector must report its denominator.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90Parameters.h"

extern "C" {
/*
 * `void` is V90Jd.cpp's reading of the object and not the mangling's -- a
 * return type is not mangled.  The object never sets %eax deliberately on the
 * way out, where getBitVector ends `lea 0x2(%edi),%eax`; declaring it void
 * here is what keeps this fixture from depending on a register the function
 * does not define.
 */
void ref_packData(void *self) asm("ref__ZN5V90Jd8packDataEv");

void our_ctor1(void *self, V90Parameters *p)
	asm("_ZN5V90JdC1EP13V90Parameters");
void ref_ctor1(void *self, V90Parameters *p)
	asm("ref__ZN5V90JdC1EP13V90Parameters");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 192

union jd_slot {
	unsigned char raw[SLOT];
	int align;	/* the object holds ints; keep the slot 4-aligned */
};

static union jd_slot ours, theirs;

/*
 * The object lives in the byte array: V90Jd has a user-declared destructor, so
 * a union with it as a variant member would have its own destructor deleted.
 * t_v90jd.cpp carries the argument in full.
 */
#define OURS	(*(V90Jd *)ours.raw)
#define THEIRS	(*(V90Jd *)theirs.raw)

static void
seed(int trial, int mode)
{
	unsigned lfsr = 0x6c1du + 0x9e37u * (unsigned)trial;
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

/*
 * The state packData must NOT touch, and the state it must throw away.  Both
 * sides get the same values and both are non-zero, so "unchanged" and "reset"
 * are each distinguishable from "the seed happened to agree at zero"
 * (findings 223, 224).
 */
static void
dirty(int trial)
{
	int i;

	OURS.unpack[0] = THEIRS.unpack[0] = (unsigned char)(trial | 1);
	OURS.unpack[1] = THEIRS.unpack[1] = (unsigned char)(trial | 0x82);
	OURS.unpackWord = THEIRS.unpackWord = 0x5a5a0000 + trial;

	/*
	 * THE LOW BIT OF EVERY CRC WORD HAS TO VARY WITH THE TRIAL, and the
	 * first version of this line got it wrong in a way only the mutation
	 * suite could see (the same shape as finding 7458).  It seeded
	 * `0x51ed0000 + trial * 16 + i`, whose low bit is `i & 1` and is
	 * therefore CONSTANT for a given element across every trial -- so
	 * crc[15] was odd in all of them, which is exactly what the correct
	 * reset leaves there.  A reset that stopped at crc[14] then agreed
	 * with the object on every input, and `the CRC register reset is one
	 * int short` read NOT CAUGHT.  `trial * 61 + i * 7` makes the low bit
	 * (trial + i) & 1, so each element is seeded against its reset value
	 * on half the trials.  The high bits stay large so that a word copied
	 * whole rather than as a bit is still visible.
	 */
	for (i = 0; i < 16; i++)
		OURS.crc[i] = THEIRS.crc[i] = 0x51ed0000 + trial * 61 + i * 7;
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V90Jd), theirs.raw + sizeof(V90Jd),
		      SLOT - sizeof(V90Jd)) == 0;
}

/*
 * The claims that do not depend on how the trial was set up: the two objects
 * agree, nothing ran past the end, and the unpacker's three fields are exactly
 * what `dirty` left in them.
 */
static void
compare(long sample, int trial)
{
	diff_eq_obj("after packData", V90Jd, &OURS, &THEIRS, sample);
	diff_eq_int("no store past the object (sample %ld)",
		    guard_equal(), 1, sample);
	diff_eq_int("packData left unpack[0] alone (sample %ld)",
		    OURS.unpack[0], (unsigned char)(trial | 1), sample);
	diff_eq_int("packData left unpack[1] alone (sample %ld)",
		    OURS.unpack[1], (unsigned char)(trial | 0x82), sample);
	diff_eq_int("packData left unpackWord alone (sample %ld)",
		    OURS.unpackWord, 0x5a5a0000 + trial, sample);
}

/*
 * ===========================================================================
 * Driven from a parameter block, which is the only way a real digital V.90
 * modem reaches this code: the Jd message carries V90Parameters' own
 * DIGITAL_RATE_MASK (+0x2c), MAX_SPECTRAL_SHAPER_LOOKAHEAD (+0x30) and the two
 * constellation parameters (+0x34, +0x38), and phase 3 is where it is sent.
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
	unsigned lfsr = 0x2b7fu + 0x4f1bu * (unsigned)trial;
	unsigned i;

	for (i = 0; i < sizeof(params.raw); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		params.raw[i] = (unsigned char)(lfsr >> 5);
	}
}

/*
 * THE MASKS EXERCISE BOTH SIDES OF THE GROUP BOUNDARY, because the 28-bit mask
 * splits 16/12 and a fixture that only drove small values could not tell that
 * split from 14/14.  Six are plausible traffic and six are boundary probes,
 * and the comment on each says which.
 */
static const int masks[] = {
	0x0fffffff,	/* plausible: all 28 digital rates offered       */
	0x000fffff,	/* plausible: 28000 up to the middle of the range*/
	0x0ffff000,	/* plausible: only the upper rates               */
	0x0007fff8,	/* plausible: a contiguous run astride the split */
	0x00003ffc,	/* plausible: a run wholly inside group 1        */
	0x0f000000,	/* plausible: the top four rates only            */
	0x00000000,	/* probe: nothing enabled                        */
	0x0000ffff,	/* probe: group 1 only, every bit                */
	0x0fff0000,	/* probe: group 2 only, every bit                */
	0x08000000,	/* probe: bit 27 alone, the highest rate         */
	0x00000001,	/* probe: bit 0 alone                            */
	0x00018000	/* probe: bits 15 and 16, astride the split      */
};

/*
 * Both constellation-size values, and four probes past them.  The constructor
 * stores the parameter's LOW BYTE, so 0x100 and 0x101 must behave as 0 and 1;
 * 2 has its low bit clear while being non-zero, which is what tells the
 * object's `& 1` from a `!= 0` reading once the byte reaches the CRC.
 */
static const int constel[] = { 0, 1, 2, 0xff, 0x100, 0x101 };

/* All four lookahead values, plus two probes above the two bits it keeps. */
static const int looks[] = { 0, 1, 2, 3, 6, 0xff };

#define NELEM(a)	((int)(sizeof(a) / sizeof((a)[0])))

static int
run_params(void)
{
	unsigned char first[16];
	int mi, li, ci, distinct = 0, moved = 0, trial = 0;
	long sample = 0;

	diff_begin("V90Jd::packData from a parameter block");

	for (mi = 0; mi < NELEM(masks); mi++)
	for (li = 0; li < NELEM(looks); li++)
	for (ci = 0; ci < NELEM(constel); ci++) {
		unsigned char before[SLOT];
		V90Parameters *p = (V90Parameters *)params.raw;

		seed(trial, trial % 4);
		seed_params(trial);
		p->DIGITAL_RATE_MASK = masks[mi];
		p->MAX_SPECTRAL_SHAPER_LOOKAHEAD = looks[li];
		p->V34_PHASE4_CONSTELLATION = constel[ci];
		p->V34_RRN_CONSTELLATION = constel[NELEM(constel) - 1 - ci];

		our_ctor1(&OURS, p);
		ref_ctor1(&THEIRS, p);
		dirty(trial);
		memcpy(before, ours.raw, SLOT);

		OURS.packData();
		ref_packData(&THEIRS);

		compare(sample, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (sample == 0)
			memcpy(first, &OURS.bits[V90JD_GROUP3 + 1], 16);
		else if (memcmp(first, &OURS.bits[V90JD_GROUP3 + 1], 16))
			distinct = 1;

		trial++;
		sample++;
	}

	diff_eq_int("packData changed the object", moved, 1, 0);
	diff_eq_int("the CRC is not the same on every trial", distinct, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * Driven straight into the payload, which reaches values the constructor
 * cannot produce.  The constructor writes 0 and 1; the object stores whatever
 * byte is there and the CRC adds it unmasked, so the encodings below drive a
 * "0" that is non-zero (0x02, 0x80, 0xfe) and a "1" that is not 1 (0x03, 0xff,
 * 0x7f).  Under a `!= 0` reading of the payload those two columns swap
 * answers; under the object's `& 1` they do not.
 * ===========================================================================
 */
static const unsigned char ones[]  = { 0x01, 0x03, 0xff, 0x01, 0x7f };
static const unsigned char zeros[] = { 0x00, 0x00, 0x02, 0x80, 0xfe };

static void
drive_payload(int mask, int a, int b, int look, int enc)
{
	unsigned char one = ones[enc], zero = zeros[enc];
	int i;

	for (i = 0; i <= 15; i++)
		OURS.bits[V90JD_GROUP1 + 1 + i] =
		    THEIRS.bits[V90JD_GROUP1 + 1 + i] =
		    ((mask >> i) & 1) ? one : zero;
	for (i = 0; i <= 11; i++)
		OURS.bits[V90JD_GROUP2 + 1 + i] =
		    THEIRS.bits[V90JD_GROUP2 + 1 + i] =
		    ((mask >> (i + 16)) & 1) ? one : zero;

	OURS.bits[47] = THEIRS.bits[47] = (unsigned char)a;
	OURS.bits[48] = THEIRS.bits[48] = (unsigned char)b;
	OURS.bits[49] = THEIRS.bits[49] = ((look >> 0) & 1) ? one : zero;
	OURS.bits[50] = THEIRS.bits[50] = ((look >> 1) & 1) ? one : zero;
}

static int
run_direct(void)
{
	int mi, ei, ci, moved = 0, trial = 0;
	long sample = 0;

	diff_begin("V90Jd::packData over a driven payload");

	for (mi = 0; mi < NELEM(masks); mi++)
	for (ei = 0; ei < NELEM(ones); ei++)
	for (ci = 0; ci < NELEM(constel); ci++) {
		unsigned char before[SLOT];

		seed(trial + 500, trial % 4);
		dirty(trial);
		drive_payload(masks[mi], constel[ci],
			      constel[NELEM(constel) - 1 - ci],
			      (mi + ci) & 3, ei);
		memcpy(before, ours.raw, SLOT);

		OURS.packData();
		ref_packData(&THEIRS);

		compare(sample, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;

		trial++;
		sample++;
	}

	diff_eq_int("the driven sweep changed the object", moved, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * What the two runs above cannot say on their own: that every one of the
 * thirty-two payload positions is actually READ.  Each is flipped in turn and
 * the BLOB's sixteen CRC bytes are required to move.  Positions 47..50 are the
 * ones that separate this function's 16/16 CRC from the constructor's 16/12
 * rate-mask split.
 * ===========================================================================
 */
static const int payload_pos[32] = {
	18, 19, 20, 21, 22, 23, 24, 25,
	26, 27, 28, 29, 30, 31, 32, 33,
	35, 36, 37, 38, 39, 40, 41, 42,
	43, 44, 45, 46, 47, 48, 49, 50
};

static const int framing_pos[7] = { 17, 34, 51, 68, 69, 70, 71 };

static int
run_reach(void)
{
	unsigned char base[16];
	int k, d;

	diff_begin("every payload bit reaches packData's CRC");

	for (k = 0; k < 32; k++) {
		unsigned char flipped[16];

		seed(k, 0);
		dirty(k);
		drive_payload(masks[0], 1, 1, 3, 0);
		ref_packData(&THEIRS);
		memcpy(base, &THEIRS.bits[V90JD_GROUP3 + 1], 16);

		seed(k, 0);
		dirty(k);
		drive_payload(masks[0], 1, 1, 3, 0);
		THEIRS.bits[payload_pos[k]] ^= 1;
		ref_packData(&THEIRS);
		memcpy(flipped, &THEIRS.bits[V90JD_GROUP3 + 1], 16);

		diff_eq_int("payload byte %ld changes the blob's CRC",
			    memcmp(base, flipped, 16) != 0, 1,
			    (long)payload_pos[k]);
	}

	/*
	 * And the converse.  The group markers and the four trailing bits are
	 * overwritten before the CRC runs, so changing them must change
	 * nothing -- which is what stops the sweep above being read as "any
	 * byte of the object moves the answer".
	 */
	seed(99, 0);
	dirty(99);
	drive_payload(masks[0], 1, 1, 3, 0);
	ref_packData(&THEIRS);
	memcpy(base, &THEIRS.bits[V90JD_GROUP3 + 1], 16);

	for (d = 0; d < 7; d++) {
		seed(99, 0);
		dirty(99);
		drive_payload(masks[0], 1, 1, 3, 0);
		THEIRS.bits[framing_pos[d]] ^= 0xff;
		ref_packData(&THEIRS);
		diff_eq_int("framing byte %ld reaches nothing",
			    memcmp(base, &THEIRS.bits[V90JD_GROUP3 + 1], 16),
			    0, (long)framing_pos[d]);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_params();
	rc |= run_direct();
	rc |= run_reach();

	return rc;
}
