/*
 * t_v90adid.cpp -- differential test of V90AutoDigitalImpDetector::reset,
 * V90AutoDigitalImpDetector::resetLinearMapping and calculateDilLength.
 *
 * The fixture is t_v90jd.cpp's and t_v90p3mod.cpp's, with the three
 * differences this class forces:
 *
 * THE OBJECT IS 43,440 BYTES AND IS SEEDED WITH VARIED BYTES, NEVER ZEROED.
 * Both sides get the same pseudorandom fill before every call, reseeded each
 * trial, so a clear loop that stops one element short is visible and a field
 * neither side writes cannot pass by accident (findings 223, 224, 230).  Most
 * of this object is exactly such a field -- 25,320 bytes of it are `pad_2818`
 * -- and a zero fill would have made the whole of it agree for free.
 *
 * BOTH SIDES SHARE ONE PARAMETER BLOCK.  `reset` reads `params->[+0x0c]` and
 * writes nothing through the pointer, so pointing both objects at the same
 * block makes the pointer field itself compare equal and removes the only
 * reason this would have needed the skip-and-compare-offsets form.  The block
 * is compared before and after as well, because "reads and never writes" is a
 * claim and not a licence.
 *
 * `pcmType` IS ALWAYS FORCED TO 0 OR 1, for t_v90p3mod's reason:
 * `calculateDilLength` indexes a sixteen-int table at `8 * pcmType`, so a
 * random 32-bit value would read out of bounds -- out of OUR frame on our
 * side and out of THEIRS on theirs, which are different frames.  That is not
 * a test of anything.  Both values are exercised against every seed mode.
 *
 * THE OBJECT IS COMPARED WHOLE, AND SO IS A GUARD PAST ITS END.
 * `diff_eq_obj` covers `sizeof(V90AutoDigitalImpDetector)` = 0xa9b0; the
 * bytes from there to the end of an over-large slot are compared separately,
 * so a store that overruns the object fails rather than passing in silence.
 * 0xa9b0 is the largest `this`-relative displacement any of the class's
 * thirty-two members uses, +0xa9ae, plus the width of the two-byte access
 * there -- finding 215's rule, and finding 251 for the measurement.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * the alias as an identifier, which sidesteps finding 225 entirely.  The
 * convention is plain cdecl with `this` as the first stack argument (finding
 * 215); all three symbols are `T` in the blob, so no regparm is involved.
 * The scalar parameters are declared `int` on the alias because an `extern
 * "C"` prototype only has to describe the ABI, and the object reads the
 * `unsigned char` as `mov %al` and the `short` as `movswl`, which is what a
 * promoted argument slot holds either way.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90Dil.h"

extern "C" {
void ref_reset(void *self, int ucode, int law, int altRbs)
	asm("ref__ZN25V90AutoDigitalImpDetector5resetEh7PcmTypes");
void ref_resetLinearMapping(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector18resetLinearMappingEv");

/*
 * The return type is not mangled; the header derives `unsigned int` from the
 * shape of the accumulation.  Declaring the alias the same way is what makes
 * the comparison cover all thirty-two bits.
 */
unsigned int ref_calculateDilLength(void *dil, int law)
	asm("ref__Z18calculateDilLengthP19tagV90DILdescriptor7PcmType");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define OBJ_BYTES	0xa9b0
#define SLOT		(OBJ_BYTES + 128)

union adid_slot {
	V90AutoDigitalImpDetector o;
	unsigned char raw[SLOT];
};

static union adid_slot ours, theirs;

/*
 * The parameter block.  Only +0x0c is read, but the whole of V90PreFilter.h's
 * bound is allocated and filled so that a read anywhere else in it would be
 * reading varied bytes rather than zeros.
 */
#define PARAMS_BYTES		0x504
#define PARAMS_CONNECTION_TYPE	0x0c

static unsigned char params_block[PARAMS_BYTES];
static unsigned char params_copy[PARAMS_BYTES];

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * `mode` picks how varied the fill is.  Mode 1 and 2 are the two constant
 * fills a clear loop cannot be distinguished from if it is the only thing
 * running: 0xa5 is the harness's own malloc fill, which is what an unwritten
 * field looks like when the object is not seeded at all, and 0x00 is what a
 * zeroed object would give -- included precisely so that the run does not
 * consist only of the case where zeroing hides a short clear loop.
 */
static void
seed(int trial, int mode)
{
	int i;

	lfsr_state = 0x1234u + 0x9e37u * (unsigned)trial + (unsigned)mode;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		switch (mode) {
		case 1:
			v = 0xa5;
			break;
		case 2:
			v = 0x00;
			break;
		case 3:
			/* High bits varied, low bit alternating. */
			v = (unsigned char)((next_byte() & 0xfe) |
					    (unsigned)(i & 1));
			break;
		default:
			v = next_byte();
			break;
		}
		ours.raw[i] = v;
		theirs.raw[i] = v;
	}

	for (i = 0; i < PARAMS_BYTES; i++)
		params_block[i] = next_byte();

	ours.o.params = (V90Parameters *)params_block;
	theirs.o.params = (V90Parameters *)params_block;
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + OBJ_BYTES, theirs.raw + OBJ_BYTES,
		      SLOT - OBJ_BYTES) == 0;
}

/*
 * THE INPUT SWEEPS ARE ROTATED AGAINST EACH OTHER, NOT INDEXED TOGETHER.
 * Eight trials of `input[trial % 8]` for every input pairs each value of one
 * with exactly one value of every other, forever -- and the first version of
 * this file did that, which left the reference code 0xd5 only ever companded
 * as A-law.  The one mutation the run did not catch was dropping the `& 0x7f`
 * mask, which only shows on a mu-law code of 0x80 or more; the one such code
 * the schedule offered was 0x80 itself, and `ulaw2linear(0xff)` and
 * `ulaw2linear(0x7f)` are both 0, so the masked and unmasked forms agreed.
 * Rotating each sweep by a different multiple of the block number fixes it.
 * Finding 253.
 */
#define NTRIAL 64
#define NBLOCK 8

#define IDX(trial, rot) \
	(((trial) + (rot) * ((trial) / NBLOCK)) % NBLOCK)

/*
 * resetLinearMapping.
 *
 * The two fields it reads -- `ucode` and `ucodeLevel` -- are forced to the
 * same value on both sides before each call rather than left to the seed, so
 * that the trial sweeps them deliberately: every one of the six phases, the
 * bottom and the top of the 128-wide row, a code of 128 or more (which the
 * object does not mask and which therefore writes into the row above), and
 * levels of both signs.
 */
static int
run_resetlinearmapping(void)
{
	static const unsigned char code[] = {
		0, 1, 0x2a, 0x7e, 0x7f, 0x80, 0xd5, 0xff
	};
	static const short level[] = {
		0, 1, -1, 0x7fff, (short)0x8000, 0x1234, (short)0xabcd, 0x55
	};
	int trial, distinct = 0, moved = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::resetLinearMapping");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		ours.o.ucode = theirs.o.ucode = code[IDX(trial, 0)];
		ours.o.ucodeLevel = theirs.o.ucodeLevel =
		    level[IDX(trial, 1)];

		ours.o.resetLinearMapping();
		ref_resetLinearMapping(&theirs.o);

		diff_eq_obj("after resetLinearMapping",
			    V90AutoDigitalImpDetector, &ours.o, &theirs.o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours.o.linMapp[0][0x2a];
		else if (ours.o.linMapp[0][0x2a] != first)
			distinct = 1;
	}

	diff_eq_int("resetLinearMapping changed the object", moved, 1, 0);
	diff_eq_int("the seeded entry is not the same on every trial",
		    distinct, 1, 0);

	return diff_end();
}

/*
 * reset.
 *
 * Four inputs are swept independently rather than together, because each
 * selects a different branch: the companding law picks `alaw2linear` against
 * `ulaw2linear` and a different xor mask, the third argument alone decides
 * between 5.0f and 1.5f at +0xa970, and the parameter block's +0x0c decides
 * the four values at +0xa978..+0xa980.  The reference code sweeps the whole
 * byte, including the values whose masked form is 0 and 0x7f.
 */
static int
run_reset(void)
{
	static const unsigned char code[] = {
		0, 1, 0x2a, 0x7e, 0x7f, 0x80, 0xd5, 0xff
	};
	static const short alt[] = { 0, 1, -1, 0x100, 0, 2, 0, (short)0x8000 };
	static const short conn[] = { 2, 0, 1, 3, 2, -2, 2, 0x102 };
	int trial, distinct = 0, moved = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::reset");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		unsigned char c = code[IDX(trial, 0)];
		int law = (trial / NBLOCK) & 1;
		short a = alt[IDX(trial, 1)];
		short t = conn[IDX(trial, 3)];

		seed(trial, trial % 4);
		memcpy(&params_block[PARAMS_CONNECTION_TYPE], &t, sizeof t);
		memcpy(params_copy, params_block, PARAMS_BYTES);
		memcpy(before, ours.raw, SLOT);

		ours.o.reset(c, (PcmType)law, a);
		ref_reset(&theirs.o, c, law, a);

		diff_eq_obj("after reset", V90AutoDigitalImpDetector,
			    &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("reset wrote nothing through params (trial %ld)",
			    memcmp(params_copy, params_block, PARAMS_BYTES),
			    0, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours.o.ucodeLevel;
		else if (ours.o.ucodeLevel != first)
			distinct = 1;
	}

	diff_eq_int("reset changed the object", moved, 1, 0);
	diff_eq_int("the companded level is not the same on every trial",
		    distinct, 1, 0);

	return diff_end();
}

/*
 * calculateDilLength.
 *
 * The descriptor is filled with varied bytes and then its three length fields
 * are forced, because `dilCount` is what the outer loop runs on and a random
 * one would make most trials the same length.  Codes of 0x80 and more are
 * exercised deliberately: under mu-law they match none of the eight
 * boundaries, so the object's search leaves its segment index at 8 and the
 * length code it takes is one past `segmentSize`.  A test that only ever
 * offered seven-bit codes would never see that and the reconstruction could
 * have clamped it.
 */
static int
run_calculatedillength(void)
{
	static tagV90DILdescriptor desc;
	static tagV90DILdescriptor copy;
	static const unsigned char counts[] = { 0, 1, 2, 7, 16, 64, 255, 3 };
	int trial, law, distinct = 0, nonzero = 0;
	unsigned int firstlen = 0;

	diff_begin("calculateDilLength");

	/* The null descriptor, which is the whole of the first branch. */
	for (law = 0; law <= 1; law++)
		diff_eq_int("calculateDilLength(0, %ld)",
			    calculateDilLength(0, (PcmType)law),
			    ref_calculateDilLength(0, law), law);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int i;

		lfsr_state = 0x2f6du + 0x9e37u * (unsigned)trial;
		for (i = 0; i < sizeof desc; i++)
			((unsigned char *)&desc)[i] = next_byte();

		desc.dilCount = counts[trial % (int)(sizeof counts)];

		/*
		 * Every eighth trial makes the codes sweep 0..255 in order, so
		 * that each of the eight segments and the off-the-end case are
		 * all reached rather than left to the fill.
		 */
		if ((trial & 7) == 0)
			for (i = 0; i < sizeof desc.dilCode; i++)
				desc.dilCode[i] = (unsigned char)i;
		else if ((trial & 7) == 1)
			for (i = 0; i < sizeof desc.dilCode; i++)
				desc.dilCode[i] =
				    (unsigned char)(0x80 + (i & 0x7f));
		else if ((trial & 7) == 2)
			for (i = 0; i < sizeof desc.dilCode; i++)
				desc.dilCode[i] = (unsigned char)(i & 0x7f);

		memcpy(&copy, &desc, sizeof desc);

		for (law = 0; law <= 1; law++) {
			unsigned int got = calculateDilLength(&desc,
							      (PcmType)law);
			unsigned int want = ref_calculateDilLength(&desc, law);

			diff_eq_int("calculateDilLength(trial %ld)",
				    got, want, trial * 2 + law);
			diff_eq_int("the descriptor is unchanged (%ld)",
				    memcmp(&copy, &desc, sizeof desc), 0,
				    trial * 2 + law);

			if (got != 0)
				nonzero = 1;
			if (trial == 0 && law == 0)
				firstlen = got;
			else if (got != firstlen)
				distinct = 1;
		}
	}

	diff_eq_int("some length is nonzero", nonzero, 1, 0);
	diff_eq_int("the length is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_resetlinearmapping();
	rc |= run_reset();
	rc |= run_calculatedillength();

	return rc;
}
