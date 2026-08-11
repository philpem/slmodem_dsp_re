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

/*
 * The sixteen processing methods.  Scalar parameters are `int` for the same
 * reason as above -- a promoted argument slot is four bytes whatever the
 * declared type -- but a `float` parameter is NOT promoted in a prototyped
 * call, so those are declared `float` and occupy one slot each: the object
 * reads them with a four-byte `flds`.
 *
 * Three have a return type here where the header gives them one.  Nothing in
 * the mangling says so; it is what the object leaves in %eax, and declaring
 * it on the alias is what makes the comparison cover the value at all.
 */
void ref_clearCamulativeVal(void *self, int phase, int code)
	asm("ref__ZN25V90AutoDigitalImpDetector18clearCamulativeValEss");
void ref_clearCamulativeAltVal(void *self, int phase, int code)
	asm("ref__ZN25V90AutoDigitalImpDetector21clearCamulativeAltValEss");
void ref_setMaxUcodeArray(void *self, unsigned char *from)
	asm("ref__ZN25V90AutoDigitalImpDetector16setMaxUcodeArrayEPh");
void ref_setPrevSessionLinearMapping(void *self, short *from)
	asm("ref__ZN25V90AutoDigitalImpDetector27setPrevSessionLinearMappingEPs");
void ref_calculateLinearMeanAndVarAlt(void *self, int v, unsigned int phase)
	asm("ref__ZN25V90AutoDigitalImpDetector28calculateLinearMeanAndVarAltEsj");
void ref_setConnectionType(void *self, int type)
	asm("ref__ZN25V90AutoDigitalImpDetector17setConnectionTypeEs");
int ref_isThereAnyAltRbsPhase(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector21isThereAnyAltRbsPhaseEv");
void ref_updateLinMappMeanAndVar(void *self, int phase, int code)
	asm("ref__ZN25V90AutoDigitalImpDetector23updateLinMappMeanAndVarEss");
void ref_updateLinMappMeanAndVarAlt(void *self, int phase, int code)
	asm("ref__ZN25V90AutoDigitalImpDetector26updateLinMappMeanAndVarAltEss");
int ref_isAltRbs(void *self, int phase, int code, float v)
	asm("ref__ZN25V90AutoDigitalImpDetector8isAltRbsEssf");
void ref_addReceivedSampleToStorage(void *self, int phase, int code, float v)
	asm("ref__ZN25V90AutoDigitalImpDetector26addReceivedSampleToStorageEshf");
void ref_applyPadGainToLinMapp(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector21applyPadGainToLinMappEv");
short ref_unSuspectedPhaseNearestLinMapp(void *self, int v, int phase)
	asm("ref__ZN25V90AutoDigitalImpDetector30unSuspectedPhaseNearestLinMappEss");
void ref_adjustUinfoToPhaseOffset(void *self, int offset)
	asm("ref__ZN25V90AutoDigitalImpDetector24adjustUinfoToPhaseOffsetEs");
void ref_updateUrefAlt(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector13updateUrefAltEv");
void ref_calculateLinearMeanAndVar(void *self, int v, int level,
				   unsigned int phase)
	asm("ref__ZN25V90AutoDigitalImpDetector25calculateLinearMeanAndVarEssj");
void ref_unitePhasesInfoOfUref(void *self, int at)
	asm("ref__ZN25V90AutoDigitalImpDetector21unitePhasesInfoOfUrefEs");
void ref_updateUref(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector10updateUrefEv");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define OBJ_BYTES	0xa9b0
#define SLOT		(OBJ_BYTES + 128)

/*
 * STORAGE PLUS A CAST, WHERE THIS WAS A UNION OF THE CLASS AND A BYTE ARRAY.
 * `V90AutoDigitalImpDetector` gained a user-declared constructor and
 * destructor when they were reconstructed, and a union may not hold a member
 * with a non-trivial one -- so the union stopped compiling.  The alias below
 * is the same reinterpretation the union performed, and it is what this
 * fixture always wanted: raw seeded storage that no constructor has run over.
 */
struct adid_slot {
	unsigned char raw[SLOT];
} __attribute__((aligned(8)));

static struct adid_slot ours, theirs;

#define ours_o		(*(V90AutoDigitalImpDetector *)ours.raw)
#define theirs_o	(*(V90AutoDigitalImpDetector *)theirs.raw)

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

	ours_o.params = (V90Parameters *)params_block;
	theirs_o.params = (V90Parameters *)params_block;
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

		ours_o.ucode = theirs_o.ucode = code[IDX(trial, 0)];
		ours_o.ucodeLevel = theirs_o.ucodeLevel =
		    level[IDX(trial, 1)];

		ours_o.resetLinearMapping();
		ref_resetLinearMapping(&theirs_o);

		diff_eq_obj("after resetLinearMapping",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[0][0x2a];
		else if (ours_o.linMapp[0][0x2a] != first)
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

		ours_o.reset(c, (PcmType)law, a);
		ref_reset(&theirs_o, c, law, a);

		diff_eq_obj("after reset", V90AutoDigitalImpDetector,
			    &ours_o, &theirs_o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("reset wrote nothing through params (trial %ld)",
			    memcmp(params_copy, params_block, PARAMS_BYTES),
			    0, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.ucodeLevel;
		else if (ours_o.ucodeLevel != first)
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

/*
 * ==========================================================================
 * The processing methods.
 *
 * WHAT IS CLAMPED, AND WHY THAT IS NOT A WEAKENING.  Three of these methods
 * index the object with a value the object does not bound: the phase, the
 * code, and `int_9100[phase]`, which is `addReceivedSampleToStorage`'s store
 * index.  A seeded 32-bit `int_9100` is about four thousand million, and both
 * sides would then write four thousand million shorts past their own object
 * -- into two different pieces of the test's own memory.  That is not a
 * comparison of anything, and the crash it produces is the test's fault and
 * not the reconstruction's.
 *
 * So the phase is swept over 0..5, the store index over the row, and the code
 * over 0..127 -- EXCEPT where a larger value still lands inside the object,
 * which is where the interesting behaviour is.  `calculateLinearMeanAndVar`
 * builds its own index by companding, and that index reaches 255: at phase 5
 * it addresses +0x9F14 in `float_9118`'s row, which is 2,716 bytes short of
 * the end of the object, so the unmasked index is exercised for real rather
 * than avoided.  `addReceivedSampleToStorage`'s code byte is swept over the
 * whole of 0..255 for the same reason.  Every deviation that is reproducible
 * inside the object is reproduced; only the ones that leave it are bounded,
 * and docs/deviations.md D256 is where the unbounded one is written down.
 *
 * THE FLOAT SWEEP is zero, negative zero, a denormal, an exactly
 * representable value, one that is not, both signs, and a magnitude past the
 * range of the `short` every one of these methods eventually rounds into --
 * because the object is `-mfpmath=387` and its `fistp` stores 0x8000 for an
 * out-of-range conversion rather than saturating.
 * ==========================================================================
 */

#define NPHASE	V90ADID_PHASES

static const float fsweep[] = {
	0.0f, -0.0f, 1.0e-40f, 2048.0f, 0.1f, -0.1f, -32768.0f, 1.0e9f
};

/*
 * Put the six sample counters somewhere inside their rows, on both sides
 * alike.  The values vary with the trial so that the store index is not the
 * same one twice, and the largest is well under 0x83e even after a run of
 * blocks has added to it.
 */
static void
sane_sample_counts(int trial)
{
	int p;

	for (p = 0; p < NPHASE; p++) {
		int n = (int)((unsigned)(trial * 37 + p * 131) % 0x600);

		ours_o.int_9100[p] = theirs_o.int_9100[p] = n;
	}
}

/* Force one field to the same value on both sides. */
#define BOTH(field, value) \
	do { ours_o.field = theirs_o.field = (value); } while (0)

/*
 * The three that only move bytes about: two array copies and the connection
 * type's four constants.
 *
 * `setConnectionType`'s else arm writes three of the four fields and leaves
 * +0xa978 alone (D255), which is exactly the kind of claim a zeroed object
 * cannot check -- so the seed is what proves it: the field holds varied bytes
 * going in, and both sides have to leave the same varied bytes there.
 */
static int
run_setters(void)
{
	static const short type[] = { 2, 0, 1, 3, -2, 2, 0x102, (short)0x8002 };
	unsigned char maxin[NPHASE];
	short mapin[V90ADID_CODES];
	int trial, moved = 0, distinct = 0, sawtwo = 0, sawother = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::set*");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short t = type[IDX(trial, 1)];
		int i;

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		for (i = 0; i < NPHASE; i++)
			maxin[i] = next_byte();
		for (i = 0; i < V90ADID_CODES; i++)
			mapin[i] = (short)((next_byte() << 8) | next_byte());

		ours_o.setMaxUcodeArray(maxin);
		ref_setMaxUcodeArray(&theirs_o, maxin);
		diff_eq_obj("after setMaxUcodeArray",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.setPrevSessionLinearMapping(mapin);
		ref_setPrevSessionLinearMapping(&theirs_o, mapin);
		diff_eq_obj("after setPrevSessionLinearMapping",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.setConnectionType(t);
		ref_setConnectionType(&theirs_o, t);
		diff_eq_obj("after setConnectionType",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (t == 2)
			sawtwo = 1;
		else
			sawother = 1;
		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.short_a97a;
		else if (ours_o.short_a97a != first)
			distinct = 1;
	}

	diff_eq_int("the setters changed the object", moved, 1, 0);
	diff_eq_int("the connection threshold is not always the same",
		    distinct, 1, 0);
	diff_eq_int("a connection type of 2 was exercised", sawtwo, 1, 0);
	diff_eq_int("a connection type other than 2 was exercised", sawother,
		    1, 0);

	return diff_end();
}

/*
 * The two clears.
 *
 * `clearCamulativeAltVal` reads only its first argument (D257), so the second
 * is swept independently of everything else: if the reconstruction used it,
 * the trials where it differs from the phase would diverge.
 */
static int
run_clears(void)
{
	static const short second[] = { 0, 1, 5, 127, -1, 3, 64, 2 };
	int trial, moved = 0, distinct = 0;
	unsigned int first = 0;

	diff_begin("V90AutoDigitalImpDetector::clearCamulative*");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short phase = (short)(trial % NPHASE);
		short code = (short)(IDX(trial, 1) * 17 + (trial % 7));

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		ours_o.clearCamulativeVal(phase, code);
		ref_clearCamulativeVal(&theirs_o, phase, code);
		diff_eq_obj("after clearCamulativeVal",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.clearCamulativeAltVal(phase, second[IDX(trial, 3)]);
		ref_clearCamulativeAltVal(&theirs_o, phase,
					  second[IDX(trial, 3)]);
		diff_eq_obj("after clearCamulativeAltVal",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.uint_1c00[0][0];
		else if (ours_o.uint_1c00[0][0] != first)
			distinct = 1;
	}

	diff_eq_int("the clears changed the object", moved, 1, 0);
	diff_eq_int("the untouched neighbour is not always the same",
		    distinct, 1, 0);

	return diff_end();
}

/*
 * The three that accumulate.
 *
 * `calculateLinearMeanAndVar` is driven under both companding laws, because
 * the law picks the conversion AND the mask -- and the mask is what turns the
 * companded byte back into the index.  `pcmType` is forced to 0 or 1 for
 * t_v90p3mod's reason, restated at the top of this file.
 */
static int
run_accumulate(void)
{
	static const short level[] = {
		0, 1, -1, 0x7fff, (short)0x8000, 1234, -5678, 0x55
	};
	static const short mag[] = {
		0, 1, -1, 100, -100, 0x7fff, (short)0x8000, 4096
	};
	int trial, moved = 0, distinct = 0, law0 = 0, law1 = 0;
	float first = 0.0f;

	diff_begin("V90AutoDigitalImpDetector::calculate*/addReceivedSample");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		unsigned int phase = (unsigned int)(trial % NPHASE);
		int law = (trial / NBLOCK) & 1;
		short v = mag[IDX(trial, 1)];
		short l = level[IDX(trial, 3)];
		float x = fsweep[IDX(trial, 5)];
		int code = (trial * 37) & 0xff;

		seed(trial, trial % 4);
		sane_sample_counts(trial);
		BOTH(pcmType, (PcmType)law);
		memcpy(before, ours.raw, SLOT);

		if (law)
			law1 = 1;
		else
			law0 = 1;

		ours_o.calculateLinearMeanAndVar(v, l, phase);
		ref_calculateLinearMeanAndVar(&theirs_o, v, l, phase);
		diff_eq_obj("after calculateLinearMeanAndVar",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.calculateLinearMeanAndVarAlt(v, phase);
		ref_calculateLinearMeanAndVarAlt(&theirs_o, v, phase);
		diff_eq_obj("after calculateLinearMeanAndVarAlt",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.addReceivedSampleToStorage((short)phase,
						  (unsigned char)code, x);
		ref_addReceivedSampleToStorage(&theirs_o, (int)phase, code, x);
		diff_eq_obj("after addReceivedSampleToStorage",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.float_9d18[0];
		else if (ours_o.float_9d18[0] != first)
			distinct = 1;
	}

	diff_eq_int("the accumulators changed the object", moved, 1, 0);
	diff_eq_int("the alternate sum is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("mu-law was exercised", law0, 1, 0);
	diff_eq_int("A-law was exercised", law1, 1, 0);

	return diff_end();
}

/*
 * The three that turn accumulators into a mapping.
 *
 * Each has an arm that does nothing -- a zero count for the two
 * `*MeanAndVar*` methods, an unflagged or empty phase for `updateUrefAlt` --
 * and a seeded count is nonzero with probability one, so the zero is forced
 * on every fourth trial and the run asserts that both arms were reached.
 * Finding 149: a method that always takes the same branch passes a whole
 * sweep of that branch perfectly.
 */
static int
run_means(void)
{
	int trial, moved = 0, distinct = 0;
	int zerocount = 0, nonzerocount = 0, altflag = 0, altnoflag = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::update*MeanAndVar*");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short phase = (short)(trial % NPHASE);
		short code = (short)((trial * 13) & 0x7f);
		int p;

		seed(trial, trial % 4);

		if ((trial & 3) == 0) {
			BOTH(uint_1c00[phase][code], 0u);
			BOTH(uint_9d30[phase], 0u);
			zerocount = 1;
		} else if ((trial & 3) == 2) {
			/*
			 * THE ONE CASE THAT SEPARATES A DIVISION FROM A
			 * MULTIPLICATION BY A RECIPROCAL, and without it the
			 * two spellings agree over everything else this file
			 * offers -- measured, not assumed: the mutation that
			 * swaps them was NOT CAUGHT until this arm existed.
			 *
			 * 143.5 / 41 is exactly 3.5, so the object's `+ 0.5f`
			 * lands exactly on 4.0 and its truncating `fistp`
			 * stores 4.  1/41 is not representable and rounds the
			 * wrong way, so 143.5 * (1/41) is a hair under 3.5,
			 * the sum is a hair under 4.0, and the same
			 * truncation stores 3.  One code apart, from one bit.
			 *
			 * THE PAIR WAS FOUND, NOT GUESSED, and the search had
			 * to mirror the method's WHOLE body to find it: a
			 * probe with only the mean in it says n = 25 and
			 * sum = 12.5 disagree, and in the real method they do
			 * not, because the variance line either side changes
			 * which x87 register the mean lives in and therefore
			 * whether it is rounded.  n = 3 and n = 25 both agree
			 * here; 41 is the first that does not.  Finding 1366.
			 *
			 * `updateLinMappMeanAndVar` and `updateUrefAlt` take
			 * the reciprocal and `updateLinMappMeanAndVarAlt`
			 * divides, so this arm has to seed BOTH the per-code
			 * and the per-phase accumulators to pin all three.
			 */
			int q;

			BOTH(uint_1c00[phase][code], 41u);
			BOTH(float_1000[phase][code], 143.5f);
			BOTH(float_9118[phase][code], 600.0f);
			for (q = 0; q < NPHASE; q++) {
				BOTH(uint_9d30[q], 41u);
				BOTH(float_9d18[q], 143.5f);
			}
			nonzerocount = 1;
		} else {
			BOTH(uint_1c00[phase][code],
			     (unsigned)(trial * 7 + 1));
			BOTH(uint_9d30[phase], (unsigned)(trial + 1));
			nonzerocount = 1;
		}

		/*
		 * The float accumulators are left as the seed made them for
		 * three trials in four, and given tame values on the fourth,
		 * so that both a wild bit pattern and an ordinary mean are
		 * exercised through the same `fistp`.
		 */
		if ((trial & 3) == 1) {
			BOTH(float_1000[phase][code], 1234.5f);
			BOTH(float_9118[phase][code], 4000000.0f);
			BOTH(float_9d18[phase], -987.25f);
		}

		for (p = 0; p < NPHASE; p++) {
			short flag = (short)(((trial >> p) & 1) ? p + 1 : 0);

			BOTH(short_2800[p], flag);
			if (flag != 0)
				altflag = 1;
			else
				altnoflag = 1;
		}

		memcpy(before, ours.raw, SLOT);

		ours_o.updateLinMappMeanAndVar(phase, code);
		ref_updateLinMappMeanAndVar(&theirs_o, phase, code);
		diff_eq_obj("after updateLinMappMeanAndVar",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.updateLinMappMeanAndVarAlt(phase, code);
		ref_updateLinMappMeanAndVarAlt(&theirs_o, phase, code);
		diff_eq_obj("after updateLinMappMeanAndVarAlt",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.updateUrefAlt();
		ref_updateUrefAlt(&theirs_o);
		diff_eq_obj("after updateUrefAlt", V90AutoDigitalImpDetector,
			    &ours_o, &theirs_o, trial);

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[phase][code];
		else if (ours_o.linMapp[phase][code] != first)
			distinct = 1;
	}

	diff_eq_int("the mean methods changed the object", moved, 1, 0);
	diff_eq_int("the mapping entry is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("a zero count was exercised", zerocount, 1, 0);
	diff_eq_int("a nonzero count was exercised", nonzerocount, 1, 0);
	diff_eq_int("a flagged phase was exercised", altflag, 1, 0);
	diff_eq_int("an unflagged phase was exercised", altnoflag, 1, 0);

	return diff_end();
}

/*
 * The two that rewrite the mapping tables wholesale.
 *
 * The pad gain is swept including 1.0f, which makes `applyPadGainToLinMapp`
 * a rounding pass and nothing else, and 0.0f, whose reciprocal is an infinity
 * that every entry then multiplies by -- the object divides once outside the
 * loop, so that is the behaviour and not a division by zero per entry.
 *
 * `adjustUinfoToPhaseOffset`'s offset is swept over 0..6 rather than over the
 * whole `short`: the wrap is `phase + 1 == 6 ? 0` and not a modulus (D258),
 * so an offset of 6 walks phases 6..11 -- still inside the object, and the
 * arm worth exercising -- while a negative one addresses in front of it.
 */
static int
run_maptransforms(void)
{
	static const float gain[] = {
		1.0f, 2.0f, 0.5f, 0.0f, -1.0f, 3.7f, 1.0e-30f, 100.0f
	};
	int trial, moved = 0, distinct = 0, sawwrap = 0, sawplain = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::applyPadGain/adjustUinfo");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short offset = (short)(trial % 7);

		seed(trial, trial % 4);
		BOTH(padGain, gain[IDX(trial, 1)]);
		BOTH(ucode, (unsigned char)((trial * 29) & 0xff));
		memcpy(before, ours.raw, SLOT);

		if (offset == 6)
			sawwrap = 1;
		else
			sawplain = 1;

		ours_o.applyPadGainToLinMapp();
		ref_applyPadGainToLinMapp(&theirs_o);
		diff_eq_obj("after applyPadGainToLinMapp",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.adjustUinfoToPhaseOffset(offset);
		ref_adjustUinfoToPhaseOffset(&theirs_o, offset);
		diff_eq_obj("after adjustUinfoToPhaseOffset",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[3][17];
		else if (ours_o.linMapp[3][17] != first)
			distinct = 1;
	}

	diff_eq_int("the transforms changed the object", moved, 1, 0);
	diff_eq_int("the mapping is not the same on every trial", distinct, 1,
		    0);
	diff_eq_int("an offset of 6 was exercised", sawwrap, 1, 0);
	diff_eq_int("an offset inside 0..5 was exercised", sawplain, 1, 0);

	return diff_end();
}

/*
 * THE THREE THAT ANSWER RATHER THAN STORE, and the whole-object comparison is
 * worth nothing for them: a method that does nothing at all passes it.  What
 * is compared is the RETURN VALUE, every trial; the object comparison is kept
 * as the other half of the claim, which is that they store nothing.
 *
 * `isAltRbs` is a detector and this is where finding 149 applies.  It has
 * three outcomes -- the early return on an unflagged phase, a distance inside
 * the threshold, and a distance outside it -- and a seeded object reaches the
 * first almost never and the third almost always.  So the flag, the
 * threshold and the mapping entry are all forced, arranged so that the
 * distance straddles the threshold, and the run asserts each of the three was
 * actually observed rather than assuming a sweep found them.
 */
static int
run_queries(void)
{
	static const short thresh[] = {
		0, 1, 100, 1000, 20000, 5, -1, 32767
	};
	static const short entry[] = {
		0, 100, -100, 1000, 32767, (short)0x8000, 7, -7
	};
	int trial, unflagged = 0, inside = 0, outside = 0;
	int anyyes = 0, anyno = 0, nearvaried = 0;
	short firstnear = 0;

	diff_begin("V90AutoDigitalImpDetector::isAltRbs/isThereAny/nearest");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short phase = (short)(trial % NPHASE);
		short code = (short)((trial * 11) & 0x7f);
		float x = fsweep[IDX(trial, 1)];
		short t = thresh[IDX(trial, 3)];
		short e = entry[IDX(trial, 5)];
		int got, want, p;
		short gots, wants;

		seed(trial, trial % 4);

		/*
		 * Four flag patterns, one per trial in four.
		 *
		 * All-zero is `isAltRbs`'s early return and
		 * `isThereAnyAltRbsPhase`'s "no"; negatives are the other way
		 * a signed total stays at or below zero; a bit pattern gives
		 * an ordinary mixed answer.
		 *
		 * THE FOURTH IS WHAT MAKES THE ACCUMULATOR'S WIDTH TESTABLE.
		 * Four phases of 10,000 total 40,000, which is positive in an
		 * `int` and NEGATIVE once truncated to the `short` the object
		 * keeps the running total in -- so the answer is "yes" if the
		 * accumulator is 32 bits wide and "no" if it is 16.  Without
		 * this case the two spellings agree over every input the test
		 * offers and the `movswl` after each add is an untested claim.
		 */
		for (p = 0; p < NPHASE; p++) {
			short flag;

			switch (trial & 3) {
			case 0:
				flag = 0;
				break;
			case 1:
				flag = (short)-(p + 1);
				break;
			case 2:
				flag = (short)(((trial >> p) & 1) ? 1 : 0);
				break;
			default:
				flag = (short)(p < 4 ? 10000 : 0);
				break;
			}

			BOTH(short_2800[p], flag);
		}
		BOTH(short_a9a6, t);
		BOTH(linMapp[phase][code], e);
		memcpy(before, ours.raw, SLOT);

		got = ours_o.isAltRbs(phase, code, x);
		want = ref_isAltRbs(&theirs_o, phase, code, x);
		diff_eq_int("isAltRbs (trial %ld)", got, want, trial);

		if (ours_o.short_2800[phase] == 0)
			unflagged = 1;
		else if (got)
			outside = 1;
		else
			inside = 1;

		got = ours_o.isThereAnyAltRbsPhase();
		want = ref_isThereAnyAltRbsPhase(&theirs_o);
		diff_eq_int("isThereAnyAltRbsPhase (trial %ld)", got, want,
			    trial);
		if (got)
			anyyes = 1;
		else
			anyno = 1;

		gots = ours_o.unSuspectedPhaseNearestLinMapp(e, phase);
		wants = ref_unSuspectedPhaseNearestLinMapp(&theirs_o, e,
							   phase);
		diff_eq_int("unSuspectedPhaseNearestLinMapp (trial %ld)",
			    gots, wants, trial);
		if (trial == 0)
			firstnear = gots;
		else if (gots != firstnear)
			nearvaried = 1;

		diff_eq_int("the queries stored nothing (trial %ld)",
			    memcmp(before, ours.raw, SLOT), 0, trial);
		diff_eq_obj("the queries left both objects alike",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
	}

	/*
	 * A DIRECTED BLOCK FOR THE INNER MAGNITUDE.
	 *
	 * `isAltRbs` takes |sample| and THEN subtracts the mapping entry;
	 * taking the magnitude of the difference instead agrees whenever the
	 * sample and the entry have the same sign, which every trial above
	 * happens to arrange.  Measured: the mutation that moves the
	 * magnitude was NOT CAUGHT by the sweep alone.  Opposite signs are
	 * what separate them -- |(|-100| - 100)| is 0 and |-100 - 100| is 200
	 * -- so each row below is a sample and an entry of opposite sign with
	 * a threshold between the two answers.
	 */
	{
		static const float dv[] = {
			-100.0f, -100.0f, -1000.0f, 1000.0f, -7.0f
		};
		static const short de[] = { 100, 100, 900, -900, 7 };
		static const short dt[] = { 50, 150, 1000, 1000, 3 };
		unsigned int i;

		for (i = 0; i < sizeof dv / sizeof dv[0]; i++) {
			int got, want, p;

			seed((int)i + 200, 0);
			for (p = 0; p < NPHASE; p++)
				BOTH(short_2800[p], 1);
			BOTH(short_a9a6, dt[i]);
			BOTH(linMapp[2][9], de[i]);

			got = ours_o.isAltRbs(2, 9, dv[i]);
			want = ref_isAltRbs(&theirs_o, 2, 9, dv[i]);
			diff_eq_int("isAltRbs, opposite signs (%ld)", got,
				    want, (long)i);
			if (got)
				outside = 1;
			else
				inside = 1;
		}
	}

	diff_eq_int("isAltRbs returned early on an unflagged phase",
		    unflagged, 1, 0);
	diff_eq_int("isAltRbs said no on a flagged phase", inside, 1, 0);
	diff_eq_int("isAltRbs said yes on a flagged phase", outside, 1, 0);
	diff_eq_int("isThereAnyAltRbsPhase said yes", anyyes, 1, 0);
	diff_eq_int("isThereAnyAltRbsPhase said no", anyno, 1, 0);
	diff_eq_int("the nearest entry is not the same on every trial",
		    nearvaried, 1, 0);

	return diff_end();
}

/*
 * ==========================================================================
 * THE SIGNAL TEST, which is the one the per-method sweeps above cannot
 * replace.
 *
 * Every sweep above reseeds the object before each call, so no accumulator is
 * ever exercised AS an accumulator: a wrong index advance, or the sum and the
 * sum of squares transposed, agrees on the first sample and diverges on the
 * second, and a test that never takes a second sample cannot see it.  This is
 * how `t_vpcmrun.c` and `t_v34call.c` are built and it is the shape that
 * matters here.
 *
 * So: seed once, `reset` and `resetLinearMapping` once, then drive both sides
 * with the same sample sequence over forty blocks and compare the whole
 * object AFTER EVERY CALL -- not after every block and not at the end.  A
 * divergence that appears at block 12 and is washed out by block 30 is still
 * a defect, and only a per-call comparison sees it.
 *
 * The samples are not noise.  Each phase gets its own level plus a varying
 * perturbation, so the per-phase means separate, the per-code histogram fills
 * unevenly, and the variance is neither zero nor dominated by one outlier --
 * which is what makes `E[x^2] - E[x]^2` a claim rather than an identity.
 * ==========================================================================
 */
static int
run_signal(void)
{
	static const short base[NPHASE] = {
		1024, -2048, 96, 8192, -300, 4
	};
	int block, moved = 0, changed = 0, calls = 0;
	short firstmap = 0;
	unsigned char before[SLOT];

	diff_begin("V90AutoDigitalImpDetector: forty blocks of samples");

	seed(1, 0);
	memcpy(before, ours.raw, SLOT);

	/* One companding law for the run; the sweeps above cover both. */
	{
		short conn = 2;

		memcpy(&params_block[PARAMS_CONNECTION_TYPE], &conn,
		       sizeof conn);
	}
	ours_o.reset(0x2a, PCM_TYPE_A_LAW, 1);
	ref_reset(&theirs_o, 0x2a, PCM_TYPE_A_LAW, 1);
	ours_o.resetLinearMapping();
	ref_resetLinearMapping(&theirs_o);
	diff_eq_obj("after the run's reset", V90AutoDigitalImpDetector,
		    &ours_o, &theirs_o, 0);

	for (block = 0; block < 40; block++) {
		int p, k;

		/*
		 * The phase flags change from block to block, so
		 * `updateUrefAlt`'s two arms both run many times over the
		 * course of the sequence rather than once at the start.
		 */
		for (p = 0; p < NPHASE; p++)
			BOTH(short_2800[p],
			     (short)(((block + p) % 3 == 0) ? 0 : 1));

		for (k = 0; k < 6; k++) {
			for (p = 0; p < NPHASE; p++) {
				int jitter = (int)(next_byte()) - 128;
				short level = (short)(base[p] + jitter * 3);
				float x = (float)level + 0.25f;
				/*
				 * SEVEN BITS HERE, DELIBERATELY, and the
				 * eighth is what D259 is about:
				 * `short_8b00[5][128]` is `int_9100[0]`, so
				 * a phase of 5 with a code of 128 or more
				 * increments the sample-store index of phase
				 * 0 instead of a histogram bin.  Both sides
				 * do it alike -- `run_accumulate` sweeps the
				 * whole byte and compares it -- but here the
				 * damage would compound: the corrupted index
				 * is then used as a store offset by the next
				 * block, and at 65,777 that leaves the object
				 * entirely.  A test that scribbles over its
				 * own memory measures nothing.
				 */
				unsigned char code =
				    (unsigned char)((block * 7 + k * 5 + p)
						    & 0x7f);

				ours_o.addReceivedSampleToStorage((short)p,
								  code, x);
				ref_addReceivedSampleToStorage(&theirs_o, p,
							       code, x);
				diff_eq_obj("block: addReceivedSample",
					    V90AutoDigitalImpDetector,
					    &ours_o, &theirs_o,
					    block * 100 + k * 10 + p);

				ours_o.calculateLinearMeanAndVar(level, level,
							(unsigned int)p);
				ref_calculateLinearMeanAndVar(&theirs_o, level,
							      level,
							(unsigned int)p);
				diff_eq_obj("block: calculateLinearMeanAndVar",
					    V90AutoDigitalImpDetector,
					    &ours_o, &theirs_o,
					    block * 100 + k * 10 + p);

				ours_o.calculateLinearMeanAndVarAlt(level,
							(unsigned int)p);
				ref_calculateLinearMeanAndVarAlt(&theirs_o,
							level, (unsigned int)p);
				diff_eq_obj("block: calculateLinearMeanAndVarAlt",
					    V90AutoDigitalImpDetector,
					    &ours_o, &theirs_o,
					    block * 100 + k * 10 + p);

				diff_eq_int("block: isAltRbs (%ld)",
					    ours_o.isAltRbs((short)p,
							    (short)code, x),
					    ref_isAltRbs(&theirs_o, p, code,
							 x),
					    block * 100 + k * 10 + p);
				calls += 4;
			}
		}

		/*
		 * The end of a block: turn the accumulators into a mapping,
		 * fold the alternate hypothesis in, and -- every eighth block
		 * -- apply a pad gain, which is the one step that rewrites
		 * every entry of both tables at once.
		 */
		for (p = 0; p < NPHASE; p++) {
			int c;

			for (c = 0; c < V90ADID_CODES; c += 13) {
				ours_o.updateLinMappMeanAndVar((short)p,
							       (short)c);
				ref_updateLinMappMeanAndVar(&theirs_o, p, c);
				diff_eq_obj("block: updateLinMappMeanAndVar",
					    V90AutoDigitalImpDetector,
					    &ours_o, &theirs_o,
					    block * 1000 + p * 128 + c);

				ours_o.updateLinMappMeanAndVarAlt((short)p,
								  (short)c);
				ref_updateLinMappMeanAndVarAlt(&theirs_o, p,
							       c);
				diff_eq_obj("block: updateLinMappMeanAndVarAlt",
					    V90AutoDigitalImpDetector,
					    &ours_o, &theirs_o,
					    block * 1000 + p * 128 + c);
				calls += 2;
			}
		}

		ours_o.updateUrefAlt();
		ref_updateUrefAlt(&theirs_o);
		diff_eq_obj("block: updateUrefAlt", V90AutoDigitalImpDetector,
			    &ours_o, &theirs_o, block);

		/*
		 * `updateUref` folds the block's accumulators for the
		 * reference code into the mapping, unites the phases and
		 * clears the accumulators -- so from here the sequence
		 * exercises the grouping over evolving state rather than over
		 * a table.  The threshold is swept across the block number so
		 * that the merge test goes both ways as the run proceeds; the
		 * flag pattern above always leaves one of phases 0..4 clear,
		 * which is what keeps D281's uninitialised read out of it.
		 */
		BOTH(short_a9a4, (short)(1 << (block % 12)));
		ours_o.updateUref();
		ref_updateUref(&theirs_o);
		diff_eq_obj("block: updateUref", V90AutoDigitalImpDetector,
			    &ours_o, &theirs_o, block);

		ours_o.adjustUinfoToPhaseOffset((short)(block % NPHASE));
		ref_adjustUinfoToPhaseOffset(&theirs_o, block % NPHASE);
		diff_eq_obj("block: adjustUinfoToPhaseOffset",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    block);

		if ((block & 7) == 7) {
			BOTH(padGain, 1.0f + (float)block * 0.125f);
			ours_o.applyPadGainToLinMapp();
			ref_applyPadGainToLinMapp(&theirs_o);
			diff_eq_obj("block: applyPadGainToLinMapp",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);

			ours_o.clearCamulativeVal((short)(block % NPHASE),
						  (short)(block % 128));
			ref_clearCamulativeVal(&theirs_o, block % NPHASE,
					       block % 128);
			diff_eq_obj("block: clearCamulativeVal",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);

			ours_o.clearCamulativeAltVal((short)(block % NPHASE),
						     0);
			ref_clearCamulativeAltVal(&theirs_o, block % NPHASE,
						  0);
			diff_eq_obj("block: clearCamulativeAltVal",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);
			calls += 3;
		}

		diff_eq_int("no store past the object (block %ld)",
			    guard_equal(), 1, block);

		if (block == 0)
			firstmap = ours_o.linMapp[0][0x2a];
		else if (ours_o.linMapp[0][0x2a] != firstmap)
			changed = 1;
		calls += 3;
	}

	if (memcmp(before, ours.raw, SLOT) != 0)
		moved = 1;

	diff_eq_int("the sequence changed the object", moved, 1, 0);
	diff_eq_int("the mapping moved while the sequence ran", changed, 1, 0);
	diff_eq_int("the sample store filled", ours_o.int_9100[0], 240, 0);
	diff_eq_int("the run made the calls it says it did",
		    calls > 1000, 1, calls);

	return diff_end();
}

/*
 * ==========================================================================
 * `unitePhasesInfoOfUref` and `updateUref`.
 *
 * The method groups the phases that are NOT flagged at +0x2800 by how close
 * their `linMapp` entries are, pools each group's accumulators into one mean
 * and variance, and then hands the largest group's answer to every phase that
 * IS flagged.  Nothing about that is reachable from a seeded object by
 * accident: with random `linMapp` entries and a random threshold the merge
 * test is either always true or always false, and with a random `short_2800`
 * every phase is flagged.
 *
 * So the six mapping entries, the six flags, the threshold and the six pooled
 * counts are all forced from tables chosen to straddle each decision, and the
 * run asserts that each outcome was seen:
 *
 *   - at least one merge happened, and at least one trial merged nothing;
 *   - at least one group had a zero pooled count, which is the arm that
 *     writes NEITHER table, and at least one had a nonzero one;
 *   - at least one flagged phase existed to receive the best group's answer,
 *     and at least one trial had none.
 *
 * ONE ARM IS DELIBERATELY NOT REACHED.  If all five of phases 0..4 are
 * flagged the object never forms a group, and it then reads an uninitialised
 * local for the value it writes to every flagged phase -- see D281.  That is
 * not a comparison of anything: the two sides read two different stack
 * frames.  The flag pattern below always leaves at least one of 0..4 clear,
 * and this comment is where that restriction is written down rather than
 * being an accident of the tables.
 * ==========================================================================
 */
static int
run_unite(void)
{
	/* Six mapping entries per row: some within a threshold, some not. */
	static const short maps[8][NPHASE] = {
		{  100,  102,  400,  402, 1000,  100 },
		{    0,    0,    0,    0,    0,    0 },
		{ -100, -102,  100,  102,    0, 3000 },
		{ 32767, -32768, 0, 1, -1, 2 },
		{  500,  600,  700,  800,  900, 1000 },
		{    7,    7,    7,    7,    7,    7 },
		{ 1234, 1235, 1236, 1237, 1238, 1239 },
		{ -5000, 5000, -5000, 5000, 0, 0 }
	};
	static const short thresh[] = { 0, 1, 3, 10, 200, 1000, 32767, -1 };
	static const unsigned counts[8][NPHASE] = {
		{ 1, 2, 3, 4, 5, 6 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 0, 41, 0, 41, 0, 41 },
		{ 10, 0, 0, 0, 0, 0 },
		{ 25, 25, 25, 25, 25, 25 },
		{ 0, 0, 7, 0, 0, 0 },
		{ 100, 200, 300, 400, 500, 600 },
		/*
		 * 40,000 IS NOT AN ARBITRARY LARGE NUMBER.  The object pools
		 * the counts into a `short` -- it loads the `unsigned int` at
		 * +0x1c00 with `movswl`, which is the low half sign-extended
		 * -- so a group of this size pools to -25,536, which is
		 * nonzero and negative.  `total != 0` and `total > 0` are
		 * different tests for it and identical for everything else in
		 * this table.
		 */
		{ 40000, 0, 41, 0, 41, 0 }
	};
	int trial;
	int merged = 0, unmerged = 0, zerototal = 0, nonzerototal = 0;
	int anyflagged = 0, noneflagged = 0, moved = 0, distinct = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::unitePhasesInfoOfUref");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short at = (short)((trial * 23) & 0x7f);
		short t = thresh[IDX(trial, 1)];
		const short *mp = maps[IDX(trial, 3)];
		const unsigned *cp = counts[IDX(trial, 5)];
		int p, q, sawpair = 0, sawflag = 0;

		seed(trial, trial % 4);
		BOTH(short_a9a4, t);

		for (p = 0; p < NPHASE; p++) {
			/*
			 * Phase (trial % 5) is always clear, so a group can
			 * always be formed and D281's uninitialised read is
			 * never taken.
			 */
			short flag = (short)((p == trial % 5) ? 0
					     : ((trial >> p) & 1));

			BOTH(short_2800[p], flag);
			BOTH(linMapp[p][at], mp[p]);
			BOTH(uint_1c00[p][at], cp[p]);
			BOTH(float_1000[p][at], (float)((int)cp[p] * 3));
			BOTH(float_9118[p][at], (float)((int)cp[p] * 41));
			BOTH(float_9d48[p][at], 0.25f * (float)p);

			if (flag != 0)
				sawflag = 1;
			if (cp[p] != 0)
				nonzerototal = 1;
			else
				zerototal = 1;
		}

		for (p = 0; p < NPHASE - 1; p++)
			for (q = p + 1; q < NPHASE; q++)
				if (ours_o.short_2800[p] == 0
				    && ours_o.short_2800[q] == 0
				    && (mp[p] - mp[q] < t)
				    && (mp[q] - mp[p] < t))
					sawpair = 1;

		if (sawpair)
			merged = 1;
		else
			unmerged = 1;
		if (sawflag)
			anyflagged = 1;
		else
			noneflagged = 1;

		memcpy(before, ours.raw, SLOT);

		ours_o.unitePhasesInfoOfUref(at);
		ref_unitePhasesInfoOfUref(&theirs_o, at);

		diff_eq_obj("after unitePhasesInfoOfUref",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[0][at];
		else if (ours_o.linMapp[0][at] != first)
			distinct = 1;
	}

	/*
	 * A DIRECTED BLOCK FOR THE CONVERGENCE CHECKSUM (D280).
	 *
	 * The object's checksum is six copies of `linMapp[5][at]`, so it goes
	 * blind exactly when that entry is zero: the sum equals the initial
	 * `prev` of zero and the loop stops after ONE round.  The obvious
	 * spelling -- summing the six phases -- does not stop there, runs a
	 * second round, and regroups by the MEANS the first round installed
	 * rather than by the entries it started from.
	 *
	 * So: phase 5 flagged with a zero entry, and phases 0..4 arranged so
	 * that the second round would merge two groups the first round kept
	 * apart.  Entries 0 and 10 pool to 200; 100 and 110 pool to 210; 200
	 * and 210 are 10 apart and the threshold is 20, so a second round
	 * merges all four into 205.  One round leaves 200,200,210,210 and two
	 * rounds leave 205,205,205,205 -- and the mutation that sums the six
	 * phases is caught by that difference and by nothing else in this
	 * file.
	 */
	{
		static const short e[NPHASE] = { 0, 10, 100, 110, 500, 0 };
		static const float fs[NPHASE] = {
			150.0f, 250.0f, 200.0f, 220.0f, 500.0f, 0.0f
		};
		short at = 0x2b;
		int p;

		seed(901, 0);
		BOTH(short_a9a4, 20);
		for (p = 0; p < NPHASE; p++) {
			BOTH(short_2800[p], (short)(p == NPHASE - 1 ? 1 : 0));
			BOTH(linMapp[p][at], e[p]);
			BOTH(uint_1c00[p][at], 1u);
			BOTH(float_1000[p][at], fs[p]);
			BOTH(float_9118[p][at], fs[p] * 4.0f);
			BOTH(float_9d48[p][at], 0.125f * (float)p);
		}

		ours_o.unitePhasesInfoOfUref(at);
		ref_unitePhasesInfoOfUref(&theirs_o, at);
		diff_eq_obj("unite: the one-round checksum",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("no store past the object (checksum block)",
			    guard_equal(), 1, 0);
	}

	diff_eq_int("uniting changed the object", moved, 1, 0);
	diff_eq_int("the united entry is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("a mergeable pair was offered", merged, 1, 0);
	diff_eq_int("a trial with nothing to merge was offered", unmerged, 1,
		    0);
	diff_eq_int("a zero pooled count was exercised", zerototal, 1, 0);
	diff_eq_int("a nonzero pooled count was exercised", nonzerototal, 1,
		    0);
	diff_eq_int("a flagged phase was there to receive the answer",
		    anyflagged, 1, 0);
	diff_eq_int("a trial with no flagged phase was exercised", noneflagged,
		    1, 0);

	return diff_end();
}

/*
 * `updateUref`, which is the same three steps in sequence: form each phase's
 * mean and variance for the reference code, unite the phases, then clear the
 * accumulators.  Driven with the SAME table shapes as above so that the unite
 * inside it reaches both arms, and with `ucode` swept -- it is the index for
 * all three steps and the method takes no argument, so the field is the only
 * way in.
 */
static int
run_updateuref(void)
{
	static const unsigned counts[] = { 0, 1, 25, 41, 3, 0, 100, 7 };
	int trial, moved = 0, distinct = 0, zero = 0, nonzero = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::updateUref");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		unsigned char at = (unsigned char)((trial * 19) & 0x7f);
		int p;

		seed(trial, trial % 4);
		BOTH(ucode, at);
		BOTH(short_a9a4, (short)(1 << (trial % 12)));

		for (p = 0; p < NPHASE; p++) {
			unsigned n = counts[(trial + p) % 8];

			BOTH(short_2800[p], (short)((p == trial % 5) ? 0
						    : ((trial >> p) & 1)));
			BOTH(linMapp[p][at], (short)(100 * p + trial));
			BOTH(uint_1c00[p][at], n);
			BOTH(float_1000[p][at], (float)((int)n * 143));
			BOTH(float_9118[p][at], (float)((int)n * 4001));
			BOTH(float_9d48[p][at], 0.5f * (float)p);

			if (n == 0)
				zero = 1;
			else
				nonzero = 1;
		}

		memcpy(before, ours.raw, SLOT);

		ours_o.updateUref();
		ref_updateUref(&theirs_o);

		diff_eq_obj("after updateUref", V90AutoDigitalImpDetector,
			    &ours_o, &theirs_o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("updateUref cleared the count (trial %ld)",
			    (long)ours_o.uint_1c00[0][at], 0, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[0][at];
		else if (ours_o.linMapp[0][at] != first)
			distinct = 1;
	}

	/*
	 * A DIRECTED BLOCK FOR updateUref's OWN ARITHMETIC.
	 *
	 * Everything the first loop writes is normally overwritten by the
	 * unite that follows it -- a grouped phase gets its group's pooled
	 * mean, and a flagged phase gets the best group's -- so the mean and
	 * the variance this method computes are invisible in almost every
	 * state.  Measured: the mutations that drop its rounding term and
	 * reverse its variance both survived the sweep above.
	 *
	 * ONE PHASE ESCAPES, and it is phase 5.  The unite's outer loop runs
	 * `i` over 0..4, so phase 5 can only ever be a group MEMBER; leave it
	 * unflagged and further than the threshold from every other entry and
	 * it is neither leader nor member, and `updateUref`'s own answer for
	 * it survives to be compared.  The count of 2 against a sum of 287
	 * makes the mean exactly 143.5, which rounds to 144 and truncates to
	 * 143 -- one code apart.
	 */
	{
		unsigned char at = 0x33;
		int p;

		seed(902, 0);
		BOTH(ucode, at);
		BOTH(short_a9a4, 2);
		for (p = 0; p < NPHASE; p++) {
			int last = (p == NPHASE - 1);

			BOTH(short_2800[p], (short)(p < 4 ? 1 : 0));
			BOTH(linMapp[p][at],
			     (short)(last ? 30000 : 100 * p));
			BOTH(uint_1c00[p][at], (unsigned)(last ? 2 : 4));
			BOTH(float_1000[p][at], last ? 287.0f : 400.0f);
			BOTH(float_9118[p][at], last ? 8000.0f : 100.0f);
			BOTH(float_9d48[p][at], 0.0f);
		}

		ours_o.updateUref();
		ref_updateUref(&theirs_o);
		diff_eq_obj("updateUref: the phase the unite cannot reach",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("no store past the object (isolated block)",
			    guard_equal(), 1, 0);
	}

	diff_eq_int("updateUref changed the object", moved, 1, 0);
	diff_eq_int("the reference entry is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("an empty cell was exercised", zero, 1, 0);
	diff_eq_int("a filled cell was exercised", nonzero, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_resetlinearmapping();
	rc |= run_reset();
	rc |= run_calculatedillength();
	rc |= run_setters();
	rc |= run_clears();
	rc |= run_accumulate();
	rc |= run_means();
	rc |= run_maptransforms();
	rc |= run_queries();
	rc |= run_unite();
	rc |= run_updateuref();
	rc |= run_signal();

	return rc;
}
