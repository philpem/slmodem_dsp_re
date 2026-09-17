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
 * neither side writes cannot pass by accident (findings F223, F224, F230).  Most
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
 * there -- finding F215's rule, and finding F251 for the measurement.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * the alias as an identifier, which sidesteps finding F225 entirely.  The
 * convention is plain cdecl with `this` as the first stack argument (finding
 * F215); all three symbols are `T` in the blob, so no regparm is involved.
 * The scalar parameters are declared `int` on the alias because an `extern
 * "C"` prototype only has to describe the ABI, and the object reads the
 * `unsigned char` as `mov %al` and the `short` as `movswl`, which is what a
 * promoted argument slot holds either way.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90DilDescriptorSettings.h"

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
 * `void`, because the two exits do not agree on %eax -- the ADI arm tail-jumps
 * to `edprintf` and the ADI_QC arm calls it and returns.  The `DilType` is
 * declared `int` for the reason the block above gives: a promoted argument
 * slot is four bytes whatever the enum's base.
 */
void ref_setDilDescriptor(void *d, int type)
	asm("ref__Z16setDilDescriptorP19tagV90DILdescriptor7DilType");

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

/*
 * The four study methods.
 *
 * `getAltVarThresh` RETURNS A FLOAT and the mangling does not say so; the
 * object leaves the value in %st(0) with `flds 0x34(%esp)` at 0x4088c and
 * nothing in %eax.  Declaring the alias `float` is what makes the return
 * value compared at all -- and it is compared as a BIT PATTERN, because one
 * arm of the method returns a NaN and `==` is false for a NaN on both sides.
 *
 * The `unsigned char` and `unsigned int` parameters are `int` and
 * `unsigned int` here for the reason above: a promoted slot is four bytes.
 */
float ref_getAltVarThresh(void *self, float *var, float factor)
	asm("ref__ZN25V90AutoDigitalImpDetector15getAltVarThreshEPff");
void ref_uniteLinMappInfoOfUnsuspectedPhases(void *self, int at)
	asm("ref__ZN25V90AutoDigitalImpDetector35uniteLinMappInfoOfUnsuspectedPhasesEh");
void ref_porcessFirstStudy(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector17porcessFirstStudyEv");
void ref_resetStudyUrefHandler(void *self, unsigned int qc)
	asm("ref__ZN25V90AutoDigitalImpDetector21resetStudyUrefHandlerEj");

/*
 * The DIL batch.  None of the three returns anything: the first ends in a
 * plain `ret` with %eax holding the last thing it happened to load, and the
 * other two end in a tail `jmp` to `dsplibs_debug_printf` -- so `void` is what
 * they leave, and the whole of what each does is in the object and in the two
 * transcripts.
 */
void ref_updateAltRbsPhaseInDil(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector22updateAltRbsPhaseInDilEv");
void ref_porcessSecondStudy(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector18porcessSecondStudyEv");
void ref_setQcLinearMapping(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector18setQcLinearMappingEv");

/*
 * The pad-gain batch.  Neither returns anything -- `determineMaxUcode` ends
 * in a plain `ret` with %eax holding the last thing it loaded and
 * `findPadGain` ends either in a plain `ret` or in a tail `jmp` to
 * `dsplibs_debug_printf` -- so `void` is what they leave, and the whole of
 * what each does is in the object and in the transcript.  The `short`
 * parameter is `int` here for the usual reason: the object reads it with
 * `movswl 0x54(%esp)`, which is what a promoted slot holds either way.
 */
void ref_determineMaxUcode(void *self, int maxCode)
	asm("ref__ZN25V90AutoDigitalImpDetector17determineMaxUcodeEs");
void ref_findPadGain(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector11findPadGainEv");

/*
 * The last member of the class, and it RETURNS AN int the mangling does not
 * mention: every arm leaves through `mov 0x3c(%esp),%eax` at 0x421a3 off a slot
 * the prologue seeds with 1, and 0, 1 and 2 are all reachable.  Declaring the
 * alias `int` is what makes the value compared at all -- the whole
 * accept/reject/finished protocol between this method and its caller lives
 * there and nowhere in the object.
 *
 * The `float` parameter is declared `float` for the reason above: it is not
 * promoted in a prototyped call and the object reads it with `flds 0xa4(%esp)`.
 */
int ref_studyUrefHandler(void *self, float v, unsigned int phase)
	asm("ref__ZN25V90AutoDigitalImpDetector16studyUrefHandlerEfj");

/*
 * The reference side's copy of the debug level.  Raising ours alone would put
 * the two sides on different branches of `porcessFirstStudy`'s only gate.
 */
extern unsigned int ref_dsplibs_debug_level;

/*
 * THE ONE THING AN `edprintf` LEAVES BEHIND WHEN NOBODY IS LISTENING.  Its
 * final `dsplibs_debug_printf` is gated on the level, but the reset and
 * advance of `iEncodeOffset` are not -- so at level 0 the number and the
 * length of the calls a method makes is still observable, through the rotating
 * key `cEncodeChar` reads.  Neither side exports the counter, and this is how
 * `t_encode` reads it: two probes name the position.  It is what makes "this
 * report is NOT behind the gate" a testable claim rather than an assertion.
 */
char cEncodeChar(unsigned char c);
char ref_cEncodeChar(unsigned char c);
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
/*
 * A GUARD IN FRONT OF THE OBJECT AS WELL AS BEHIND IT.
 * `porcessSecondStudy` reads `linMapp[unSuspectedPhase][code - 2]` at codes 0
 * and 1, which for an unsuspected phase of 0 is four bytes IN FRONT of `this`
 * (D287).  Two static objects have two different sets of bytes in front of
 * them, so without this the two sides would read different memory and the
 * comparison would be measuring the linker's layout.  Sixteen bytes is four
 * times the deepest reach, it is seeded alike on both sides, and
 * `guard_equal()` compares it -- a store in front of the object fails here
 * exactly as a store past its end does.
 */
#define PRE	16

struct adid_slot {
	unsigned char pre[PRE];
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

	/*
	 * The front guard, filled from the trial and NOT from the LFSR: taking
	 * bytes off the stream here would have moved every seeded byte in
	 * every suite above by sixteen places.
	 */
	for (i = 0; i < PRE; i++) {
		unsigned char v = (unsigned char)(0x5au ^ (unsigned)(i * 31)
						  ^ (unsigned)(trial * 13)
						  ^ (unsigned)(mode * 7));

		ours.pre[i] = theirs.pre[i] = v;
	}

	ours_o.params = (V90Parameters *)params_block;
	theirs_o.params = (V90Parameters *)params_block;
}

/*
 * A float compared as an INTEGER.  `getAltVarThresh` returns a NaN whenever
 * nothing is below the average of its six inputs, and `==` is false for a NaN
 * on both sides -- so an equality comparison of the return value would pass
 * for ever on exactly the arm that is hardest to get right.
 */
static unsigned int
fbits(float f)
{
	unsigned int u;

	memcpy(&u, &f, sizeof u);
	return u;
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + OBJ_BYTES, theirs.raw + OBJ_BYTES,
		      SLOT - OBJ_BYTES) == 0
	    && memcmp(ours.pre, theirs.pre, PRE) == 0;
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
 * Finding F253.
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
 * `setDilDescriptor` -- .text+0x31c60, 0x131 = 305 bytes
 * ==========================================================================
 *
 * Five copies and three scalars out of eight two-row tables, and the whole
 * difficulty of testing it is that MOST OF WHAT IT DOES IS NOT WRITE.
 *
 * THE DESCRIPTOR IS SEEDED WITH NON-ZERO BYTES AND NEVER ZEROED, and here
 * that is not the usual finding-230 hygiene but the only way the central
 * claim is observable at all.  `seq1` is filled to `seq1Length`, `seq2` to
 * `seq2Length` and `dilCode` to `dilCount`; the rest of all three arrays
 * keeps whatever it held (finding F7602).  Under `DIL_TYPE_ADI_QC` that is 68
 * bytes of each pattern array and 112 ucode slots -- and every table entry at
 * those indices is a ZERO, so over a zeroed descriptor "not written" and
 * "written zero" are the same bytes and the claim cannot fail.  The seed
 * therefore maps a zero byte to 0xa5 rather than emitting it.
 *
 * AND THE TAIL IS CHECKED TWICE, THE SECOND TIME WITHOUT THE SEED.  A tail
 * that survives a seed still only says the loop stopped somewhere at or below
 * the count.  The second half below calls ADI and then ADI_QC on the SAME
 * descriptor and requires `seq1[60..119]` to be exactly what the ADI arm put
 * there -- 60 bytes of a DIFFERENT table row, which no bound except 60 leaves
 * standing -- and then calls them the other way round and requires the same
 * 60 to have been overwritten.  That is the count driving the extent, in both
 * directions, against a witness the fixture did not choose.
 *
 * A `DilType` OUTSIDE {0, 1} IS NOT DRIVEN, and the .cpp says why: every
 * index is `type` scaled by the row width with no bound check, so a type of 2
 * reads past all eight tables into whatever `.data` holds next -- the blob's
 * layout for the blob's copy and GCC's for ours.  Such a fixture would be
 * measuring section placement.
 *
 * THE DEBUG SWEEP IS {0, 1, 2}.  Both messages go through `edprintf`, which
 * gates itself on `dsplibs_debug_level > 1`, so a sweep over {0, 2} could not
 * tell that gate from `> 0`.
 *
 * WHAT NO FIXTURE HERE CAN CATCH, named rather than left as a silent hole:
 * `TO`'s two rows are byte-for-byte identical and so are `REF`'s, `N` is
 * {144, 144}, and `Lsp` and `Ltp` are the SAME pair {120, 60}.  So a
 * row-selection defect on `TO`, `REF` or `N`, and a swap of `Lsp` for `Ltp`,
 * are equivalent mutants by construction; test/mutations/v90dil.json carries
 * them as such rather than as uncaught rows.
 */

/*
 * The two rows of `H` and `REF` and the three counts, spelled out here so
 * that the assertions below are claims about the OBJECT rather than about the
 * reconstruction agreeing with itself.
 */
static const unsigned char dil_H[2][8] = {
	{  19,  39,  39,  39,  39,  39,  39,  19 },
	{   9,  19,  19,  19,  19,  19,  19,   9 }
};
static const unsigned char dil_REF[2][8] = {
	{  78,  78,  78,  78,  78,  78,  78,  25 },
	{  78,  78,  78,  78,  78,  78,  78,  25 }
};
static const unsigned char dil_N[2] = { 144, 144 };
static const unsigned char dil_L[2] = { 120, 60 };

#define DIL_GUARD	64
#define DIL_SLOT	((int)sizeof(tagV90DILdescriptor) + DIL_GUARD)

static unsigned char dilA[DIL_SLOT] __attribute__((aligned(8)));
static unsigned char dilB[DIL_SLOT] __attribute__((aligned(8)));
static unsigned char dilS[DIL_SLOT];

#define DA	((tagV90DILdescriptor *)dilA)
#define DB	((tagV90DILdescriptor *)dilB)
#define DS	((const tagV90DILdescriptor *)dilS)

static void
dil_seed_pair(int trial)
{
	int i;

	lfsr_state = 0x77a1u + 0x9e37u * (unsigned)trial;
	for (i = 0; i < DIL_SLOT; i++) {
		unsigned char v = next_byte();

		/* Never zero; see the group comment. */
		dilS[i] = v != 0 ? v : (unsigned char)0xa5;
	}
	memcpy(dilA, dilS, DIL_SLOT);
	memcpy(dilB, dilS, DIL_SLOT);
}

static void
dil_fire(int type)
{
	dsplib_debug_capture_reset();
	setDilDescriptor(DA, (DilType)type);
	ref_setDilDescriptor(dilB, type);
}

static void
dil_compare(long tag)
{
	diff_eq_obj_(__FILE__, __LINE__, "after setDilDescriptor",
		     "the descriptor and the guard past it", dilA, dilB,
		     (size_t)DIL_SLOT, tag);
	diff_eq_int("transcript line count (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);
	diff_eq_int("transcript text (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
}

static int
run_setdildescriptor(void)
{
	static unsigned char midB[DIL_SLOT];
	static char adiText[8192], qcText[8192];
	int trial, type, lvl, i;
	int seen[8];
	int quiet = 0, loud = 0;
	int tailWitness = 0;

	diff_begin("setDilDescriptor");

	for (i = 0; i < 8; i++)
		seen[i] = 0;

	dsplib_debug_capture_on = 1;
	adiText[0] = qcText[0] = '\0';

	for (lvl = 0; lvl <= 2; lvl++) {
		dsplibs_debug_level = ref_dsplibs_debug_level = (unsigned)lvl;

		for (trial = 0; trial < 12; trial++)
			for (type = 0; type <= 1; type++) {
				long tag = (long)lvl * 1000 + trial * 2 + type;
				unsigned int n = dil_N[type];
				unsigned int l = dil_L[type];

				dil_seed_pair(trial * 2 + type);
				dil_fire(type);
				dil_compare(tag);

				/*
				 * THE GUARD, against the SEED and not against
				 * the other side: both were seeded alike, so a
				 * store past the end that both made would
				 * compare equal.
				 */
				diff_eq_obj_(__FILE__, __LINE__,
					     "nothing is stored past the "
					     "descriptor", "the guard",
					     dilB + sizeof(tagV90DILdescriptor),
					     dilS + sizeof(tagV90DILdescriptor),
					     DIL_GUARD, tag);

				/* The three counts, by value, on the blob's side. */
				diff_eq_int("dilCount (%ld)", (long)DB->dilCount,
					    (long)dil_N[type], tag);
				diff_eq_int("seq1Length (%ld)",
					    (long)DB->seq1Length,
					    (long)dil_L[type], tag);
				diff_eq_int("seq2Length (%ld)",
					    (long)DB->seq2Length,
					    (long)dil_L[type], tag);

				/*
				 * `H` and `REF`, the only two arrays written
				 * unconditionally -- `cmp $0x7,%edx; jbe` and
				 * no reloaded bound.  `H`'s two rows DIFFER,
				 * so this carries the row-selection claim;
				 * `REF`'s do not, and the group comment says
				 * so.
				 */
				for (i = 0; i < 8; i++) {
					diff_eq_int("segmentSize[%ld]",
						    (long)DB->segmentSize[i],
						    (long)dil_H[type][i],
						    (long)i);
					diff_eq_int("segmentCode[%ld]",
						    (long)DB->segmentCode[i],
						    (long)dil_REF[type][i],
						    (long)i);
				}

				/* THE THREE TAILS.  Finding F7602. */
				diff_eq_obj_(__FILE__, __LINE__,
					     "seq1 past seq1Length is left "
					     "alone", "seq1 tail",
					     DB->seq1 + l, DS->seq1 + l,
					     128u - l, tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "seq2 past seq2Length is left "
					     "alone", "seq2 tail",
					     DB->seq2 + l, DS->seq2 + l,
					     128u - l, tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "dilCode past dilCount is left "
					     "alone", "dilCode tail",
					     DB->dilCode + n, DS->dilCode + n,
					     256u - n, tag);

				/*
				 * ANTI-VACUITY, per region: a store agrees for
				 * the right reason only where what it replaced
				 * was different.  Eight regions, each of which
				 * has to have been seen to change.
				 */
				if (DS->dilCount != DB->dilCount)
					seen[0] = 1;
				if (DS->seq1Length != DB->seq1Length)
					seen[1] = 1;
				if (DS->seq2Length != DB->seq2Length)
					seen[2] = 1;
				if (memcmp(DS->seq1, DB->seq1, l) != 0)
					seen[3] = 1;
				if (memcmp(DS->seq2, DB->seq2, l) != 0)
					seen[4] = 1;
				if (memcmp(DS->dilCode, DB->dilCode, n) != 0)
					seen[5] = 1;
				if (memcmp(DS->segmentSize, DB->segmentSize, 8)
				    != 0)
					seen[6] = 1;
				if (memcmp(DS->segmentCode, DB->segmentCode, 8)
				    != 0)
					seen[7] = 1;

				/*
				 * AND THE TAILS ARE NOT VACUOUS EITHER: what
				 * stands there must differ from the zero every
				 * table holds at those indices, or "left
				 * alone" and "written from the table" are the
				 * same bytes.
				 */
				for (i = (int)l; i < 128; i++)
					if (DB->seq1[i] != 0
					    && DB->seq2[i] != 0)
						tailWitness = 1;

				if (dsplib_debug_capture_lines(1) == 0)
					quiet++;
				else
					loud++;

				if (lvl == 2) {
					if (type == 0)
						strcpy(adiText,
						       dsplib_debug_capture_text(1));
					else
						strcpy(qcText,
						       dsplib_debug_capture_text(1));
				}
			}
	}

	for (i = 0; i < 8; i++)
		diff_eq_int("region %ld was observably written", seen[i], 1,
			    (long)i);
	diff_eq_int("the tails hold something no table would have put there",
		    tailWitness, 1, 0);
	diff_eq_int("both arms of the edprintf gate were taken",
		    quiet > 0 && loud > 0, 1, 0);
	diff_eq_int("the two DilTypes print DIFFERENT messages",
		    adiText[0] != '\0' && qcText[0] != '\0'
		    && strcmp(adiText, qcText) != 0, 1, 0);

	/*
	 * ==================================================================
	 * THE SAME DESCRIPTOR TWICE, WHICH IS THE TAIL CLAIM WITHOUT THE SEED
	 * ==================================================================
	 */
	dsplibs_debug_level = ref_dsplibs_debug_level = 0u;

	dil_seed_pair(101);
	dil_fire(0);
	dil_compare(9000);
	memcpy(midB, dilB, DIL_SLOT);

	dil_fire(1);
	dil_compare(9001);

	diff_eq_int("ADI_QC leaves ADI's seq1[60..119] standing",
		    memcmp(DB->seq1 + 60,
			   ((tagV90DILdescriptor *)midB)->seq1 + 60, 60), 0,
		    0);
	diff_eq_int("...and ADI's seq2[60..119] too",
		    memcmp(DB->seq2 + 60,
			   ((tagV90DILdescriptor *)midB)->seq2 + 60, 60), 0,
		    0);
	/*
	 * AND THAT WITNESS IS NOT THE SEED.  Had ADI left those 60 bytes alone
	 * as well, the two checks above would hold vacuously.
	 */
	diff_eq_int("the surviving span is ADI's and not the seed's",
		    memcmp(((tagV90DILdescriptor *)midB)->seq1 + 60,
			   DS->seq1 + 60, 60) != 0, 1, 0);

	dil_seed_pair(102);
	dil_fire(1);
	dil_compare(9002);
	memcpy(midB, dilB, DIL_SLOT);

	dil_fire(0);
	dil_compare(9003);

	diff_eq_int("ADI overwrites ADI_QC's seq1[60..119]",
		    memcmp(DB->seq1 + 60,
			   ((tagV90DILdescriptor *)midB)->seq1 + 60, 60) != 0,
		    1, 0);
	diff_eq_int("...and dilCode is written to 144 either way",
		    memcmp(DB->dilCode,
			   ((tagV90DILdescriptor *)midB)->dilCode, 144), 0, 0);

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0u;

	return diff_end();
}

/*
 * ==========================================================================
 * The processing methods.
 *
 * WHAT IS CLAMPED, AND WHY THAT IS NOT A WEAKENING.  Three of these methods
 * index the object with a value the object does not bound: the phase, the
 * code, and `sampleCount[phase]`, which is `addReceivedSampleToStorage`'s store
 * index.  A seeded 32-bit `sampleCount` is about four thousand million, and both
 * sides would then write four thousand million shorts past their own object
 * -- into two different pieces of the test's own memory.  That is not a
 * comparison of anything, and the crash it produces is the test's fault and
 * not the reconstruction's.
 *
 * So the phase is swept over 0..5, the store index over the row, and the code
 * over 0..127 -- EXCEPT where a larger value still lands inside the object,
 * which is where the interesting behaviour is.  `calculateLinearMeanAndVar`
 * builds its own index by companding, and that index reaches 255: at phase 5
 * it addresses +0x9F14 in `magnitudeSqSum`'s row, which is 2,716 bytes short of
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

		ours_o.sampleCount[p] = theirs_o.sampleCount[p] = n;
	}
}

/* Force one field to the same value on both sides. */
#define BOTH(field, value) \
	do { ours_o.field = theirs_o.field = (value); } while (0)

/*
 * The four helpers the pad-gain batch needs.  They live up here rather than
 * beside their own sweeps because `run_signal` uses three of them; the long
 * argument for what each bound is for is at the head of `run_maxucode`.
 */

/* The last `linearMappingVar` entry whose four bytes are still inside the object. */
#define ADID_VAR_LAST	793

/*
 * `determineMaxUcode` indexes that array as `phase * 128 + code` with a code
 * that reaches the argument, so phase 5 plus anything over 153 leaves the
 * object.  The phase is therefore chosen FROM the argument.
 */
static short
safe_usp(int arg, int want)
{
	if (want == 5 && (arg & 0xff) > ADID_VAR_LAST - 5 * V90ADID_CODES)
		return 4;
	return (short)want;
}

/* One flag per phase from a bit pattern, the same on both sides. */
static void
adid_set_2800(int pattern)
{
	int p;

	for (p = 0; p < NPHASE; p++)
		ours_o.altRbsFlag[p] = theirs_o.altRbsFlag[p] =
		    (short)((pattern >> p) & 1);
}

/*
 * Plant the five variances `findPadGain`'s scan reads -- the entries at
 * `originalMaxUcode - 3` and the four below it -- with the smallest at a chosen
 * offset.  That does two things: it fixes `projectionBaseUcode`, which is
 * otherwise a function of seeded bytes and unobservable behind the
 * [0x50, 0x5f] clamp, and it guarantees the scan's take happens at all, which
 * is what keeps the run off the uninitialised index of D290.
 */
static void
plant_window(short usp, unsigned char a954, int minoff, float minval,
	     float other)
{
	unsigned char s = (unsigned char)(a954 - 3);
	int d;

	for (d = 0; d <= 4; d++) {
		unsigned char at = (unsigned char)(s - d);
		float v = (d == minoff) ? minval : other;

		ours_o.linearMappingVar[usp][at] = v;
		theirs_o.linearMappingVar[usp][at] = v;
	}
}

/* Where that planting puts the base code, clamp included. */
static unsigned char
planted_base(unsigned char a954, int minoff)
{
	unsigned char b = (unsigned char)((unsigned char)(a954 - 3) - minoff);

	if (b > 0x5f)
		b = 0x5f;
	else if (b < 0x50)
		b = 0x50;
	return b;
}

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
			first = ours_o.minMaxUcode;
		else if (ours_o.minMaxUcode != first)
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
			first = ours_o.magnitudeCount[0][0];
		else if (ours_o.magnitudeCount[0][0] != first)
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
			first = ours_o.altMagnitudeSum[0];
		else if (ours_o.altMagnitudeSum[0] != first)
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
 * Finding F149: a method that always takes the same branch passes a whole
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
			BOTH(magnitudeCount[phase][code], 0u);
			BOTH(altMagnitudeCount[phase], 0u);
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
			 * here; 41 is the first that does not.  Finding F1366.
			 *
			 * `updateLinMappMeanAndVar` and `updateUrefAlt` take
			 * the reciprocal and `updateLinMappMeanAndVarAlt`
			 * divides, so this arm has to seed BOTH the per-code
			 * and the per-phase accumulators to pin all three.
			 */
			int q;

			BOTH(magnitudeCount[phase][code], 41u);
			BOTH(magnitudeSum[phase][code], 143.5f);
			BOTH(magnitudeSqSum[phase][code], 600.0f);
			for (q = 0; q < NPHASE; q++) {
				BOTH(altMagnitudeCount[q], 41u);
				BOTH(altMagnitudeSum[q], 143.5f);
			}
			nonzerocount = 1;
		} else {
			BOTH(magnitudeCount[phase][code],
			     (unsigned)(trial * 7 + 1));
			BOTH(altMagnitudeCount[phase], (unsigned)(trial + 1));
			nonzerocount = 1;
		}

		/*
		 * The float accumulators are left as the seed made them for
		 * three trials in four, and given tame values on the fourth,
		 * so that both a wild bit pattern and an ordinary mean are
		 * exercised through the same `fistp`.
		 */
		if ((trial & 3) == 1) {
			BOTH(magnitudeSum[phase][code], 1234.5f);
			BOTH(magnitudeSqSum[phase][code], 4000000.0f);
			BOTH(altMagnitudeSum[phase], -987.25f);
		}

		for (p = 0; p < NPHASE; p++) {
			short flag = (short)(((trial >> p) & 1) ? p + 1 : 0);

			BOTH(altRbsFlag[p], flag);
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
 * `isAltRbs` is a detector and this is where finding F149 applies.  It has
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

			BOTH(altRbsFlag[p], flag);
		}
		BOTH(altRbsDistanceThresh, t);
		BOTH(linMapp[phase][code], e);
		memcpy(before, ours.raw, SLOT);

		got = ours_o.isAltRbs(phase, code, x);
		want = ref_isAltRbs(&theirs_o, phase, code, x);
		diff_eq_int("isAltRbs (trial %ld)", got, want, trial);

		if (ours_o.altRbsFlag[phase] == 0)
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
				BOTH(altRbsFlag[p], 1);
			BOTH(altRbsDistanceThresh, dt[i]);
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

	/*
	 * The study's own reset, once, with a gain that misses every clamp --
	 * so the thresholds the rest of the run uses are the ones the object
	 * computes rather than seeded bytes, and `linMapp[p][ucode]` starts at
	 * `ucodeLevel / gain` rather than at `ucodeLevel`.
	 */
	BOTH(float_a950, 2.0f);
	ours_o.resetStudyUrefHandler(1u);
	ref_resetStudyUrefHandler(&theirs_o, 1u);
	diff_eq_obj("after the run's resetStudyUrefHandler",
		    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);

	/*
	 * The study's six durations, forced short.  `resetStudyUrefHandler` has
	 * just copied them out of the parameter block, which is seeded, so
	 * without this the state machine below would sit in state 0 for two
	 * billion samples.  At these lengths forty blocks walk the whole chain
	 * 0 -> 1 -> 2 -> 4 -> 3 -> 5 -> 6 several times over.
	 */
	BOTH(int_a98c, 5);
	BOTH(int_a990, 7);
	BOTH(int_a994, 3);
	BOTH(int_a998, 11);
	BOTH(int_a99c, 4);

	for (block = 0; block < 40; block++) {
		int p, k;

		/*
		 * The phase flags change from block to block, so
		 * `updateUrefAlt`'s two arms both run many times over the
		 * course of the sequence rather than once at the start.
		 */
		for (p = 0; p < NPHASE; p++)
			BOTH(altRbsFlag[p],
			     (short)(((block + p) % 3 == 0) ? 0 : 1));

		for (k = 0; k < 6; k++) {
			for (p = 0; p < NPHASE; p++) {
				int jitter = (int)(next_byte()) - 128;
				short level = (short)(base[p] + jitter * 3);
				float x = (float)level + 0.25f;
				/*
				 * SEVEN BITS HERE, DELIBERATELY, and the
				 * eighth is what D259 is about:
				 * `codeHistogram[5][128]` is `sampleCount[0]`, so
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

				/*
				 * AND THE STUDY ITSELF, ONE CALL PER SAMPLE,
				 * which is what the class is for and what this
				 * method is the entry point of.  Its tails call
				 * `updateUref`, and `unitePhasesInfoOfUref`
				 * reads an uninitialised local when all five of
				 * phases 0..4 are flagged (D281) -- so one of
				 * them is cleared before each call, rotating.
				 * The state machine's own tails can only ADD
				 * flags, and they add them after the call's
				 * `updateUref` has already run, so clearing at
				 * entry is enough.
				 */
				BOTH(altRbsFlag[(block + k) % 5], 0);
				{
					int g = ours_o.studyUrefHandler(x,
						    (unsigned int)p);
					int r = ref_studyUrefHandler(&theirs_o,
						    x, (unsigned int)p);

					diff_eq_int("block: studyUrefHandler "
						    "(%ld)", g, r,
						    block * 100 + k * 10 + p);
					diff_eq_obj("block: studyUrefHandler",
						    V90AutoDigitalImpDetector,
						    &ours_o, &theirs_o,
						    block * 100 + k * 10 + p);
					calls++;
				}
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
		BOTH(uniteUrefDistanceThresh, (short)(1 << (block % 12)));
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
			/*
			 * THE HONEST VERSION OF THE PAD GAIN: compute it from
			 * the variances the block just produced instead of
			 * planting one.  `determineMaxUcode` leaves +0xa954
			 * behind and `findPadGain` reads it, so the pair runs
			 * in that order and `applyPadGainToLinMapp` then
			 * divides by what they decided rather than by a table
			 * value -- which is the sequence the class is for.
			 *
			 * THE THREE FIELDS FORCED HERE ARE THE THREE THAT
			 * DECIDE WHETHER EITHER METHOD TERMINATES OR STAYS
			 * INSIDE THE OBJECT, and they are the same bounds the
			 * two dedicated sweeps above use: the argument is
			 * under 255, +0xa954 is in [8, 0x9c], and the phase
			 * comes from `safe_usp`.  `determineMaxUcode` runs
			 * first and rewrites +0xa954 itself, so the value is
			 * put back before `findPadGain` is called.
			 */
			short mc = (short)(0x40 + block % 0x40);

			BOTH(unSuspectedPhase, safe_usp(mc, block % NPHASE));
			BOTH(minMaxUcode, (short)(0x30 + block % 8));
			BOTH(float_a980, 1.0f + (float)(block % 5));
			ours_o.determineMaxUcode(mc);
			ref_determineMaxUcode(&theirs_o, mc);
			diff_eq_obj("block: determineMaxUcode",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);

			BOTH(originalMaxUcode, (unsigned char)(0x50 + block % 0x28));
			plant_window(ours_o.unSuspectedPhase,
				     ours_o.originalMaxUcode, block % 5, 1.0f,
				     1.0e9f);
			ours_o.findPadGain();
			ref_findPadGain(&theirs_o);
			diff_eq_obj("block: findPadGain",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);

			ours_o.applyPadGainToLinMapp();
			ref_applyPadGainToLinMapp(&theirs_o);
			diff_eq_obj("block: applyPadGainToLinMapp (computed)",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);

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
			calls += 6;
		}

		/*
		 * THE STUDY METHODS, over the state the block just built.
		 *
		 * `getAltVarThresh` is offered the six variances the block's
		 * `updateLinMappMeanAndVar` calls actually produced, so its
		 * input evolves with the run instead of being a table -- and
		 * its answer is compared as a bit pattern, because a run that
		 * flattens the variances makes the count zero and the answer a
		 * NaN.
		 *
		 * `uniteLinMappInfoOfUnsuspectedPhases` needs its flag forced
		 * EVERY TIME.  It groups on +0x280c, which `porcessFirstStudy`
		 * writes below, and that method sets all six when the mapping
		 * is smooth -- which is exactly the state where the object
		 * forms no group and reads `bestGroup` uninitialised (D284).
		 * Two different stack frames; leaving the flag to the previous
		 * call would make this test nondeterministic.  This guard is a
		 * different one from the `altRbsFlag` guard above, which is
		 * D281's.
		 */
		{
			float var[NPHASE];
			unsigned int got, want;
			int p;

			for (p = 0; p < NPHASE; p++)
				var[p] = ours_o.linearMappingVar[p][0x2a];

			got = fbits(ours_o.getAltVarThresh(var,
						1.5f + (float)block * 0.25f));
			want = fbits(ref_getAltVarThresh(&theirs_o, var,
						1.5f + (float)block * 0.25f));
			diff_eq_int("block: getAltVarThresh (%ld)", got, want,
				    block);
			diff_eq_obj("block: getAltVarThresh stored nothing",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);
			calls++;
		}

		if (block % 5 == 4) {
			unsigned char at =
			    (unsigned char)((block * 11 + 3) & 0x7f);
			int p;

			for (p = 0; p < NPHASE; p++)
				BOTH(byte_280c[p],
				     (unsigned char)((block + p) % 4 == 0 ? 0
						     : 1));

			ours_o.uniteLinMappInfoOfUnsuspectedPhases(at);
			ref_uniteLinMappInfoOfUnsuspectedPhases(&theirs_o, at);
			diff_eq_obj("block: uniteLinMappInfoOfUnsuspectedPhases",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);
			calls++;
		}

		/*
		 * The first study, every tenth block.  It clears every
		 * accumulator and every mapping entry but the reference code's,
		 * so running it per block would flatten the state the rest of
		 * the sequence is building; every tenth lets the run rebuild in
		 * between and still exercise the study over evolved input.
		 */
		if (block % 10 == 9) {
			BOTH(trn1Sigma, 20.0f + (float)block);
			BOTH(neighborUcodeMinDistance, (short)(100 + block));
			BOTH(neighborUcodeMaxDistance, (short)(4000 + block));

			ours_o.porcessFirstStudy();
			ref_porcessFirstStudy(&theirs_o);
			diff_eq_obj("block: porcessFirstStudy",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);
			calls++;

			/*
			 * The second study, on the verdict the first study
			 * just wrote.  It scans `byte_280c` for the reference
			 * phase itself, so nothing has to be forced here -- and
			 * it calls `updateAltRbsPhaseInDil`, which walks the
			 * sample store with a cursor that is the histogram's
			 * running sum.  Forty blocks of six samples a phase is
			 * 240 against the row's 2,110, so the cursor stays
			 * inside the row for the whole run without a clamp.
			 *
			 * Codes 0 and 1 reach four bytes in front of the
			 * object (D287); the fixture's front guard is what
			 * makes that comparable.
			 */
			ours_o.porcessSecondStudy();
			ref_porcessSecondStudy(&theirs_o);
			diff_eq_obj("block: porcessSecondStudy",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);
			calls++;
		}

		/*
		 * The QC mapping, off the phase of the first study: it rebuilds
		 * every unflagged phase's mapping from the accumulators the
		 * block just filled, or from `prevLinMapp` where the phase has
		 * no verdict -- which is seeded once, here, so that the copy
		 * arm has something recognisable to copy.
		 */
		if (block % 10 == 4) {
			int c;

			if (block == 4)
				for (c = 0; c < V90ADID_CODES; c++)
					BOTH(prevLinMapp[c],
					     (short)(c * 61 - 3000));

			ours_o.setQcLinearMapping();
			ref_setQcLinearMapping(&theirs_o);
			diff_eq_obj("block: setQcLinearMapping",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);
			calls++;
		}

		/*
		 * And the repair on its own, with the reference phase forced:
		 * `reset` does not clear +0xa968 and the two callers are what
		 * normally leave a value there, so a direct call has to supply
		 * one -- a seeded 16-bit row index would walk the mapping
		 * table straight out of the object.
		 */
		if (block % 5 == 2) {
			BOTH(unSuspectedPhase, (short)(block % NPHASE));
			ours_o.updateAltRbsPhaseInDil();
			ref_updateAltRbsPhaseInDil(&theirs_o);
			diff_eq_obj("block: updateAltRbsPhaseInDil",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, block);
			calls++;
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
	diff_eq_int("the sample store filled", ours_o.sampleCount[0], 240, 0);
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
 * test is either always true or always false, and with a random `altRbsFlag`
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
		BOTH(uniteUrefDistanceThresh, t);

		for (p = 0; p < NPHASE; p++) {
			/*
			 * Phase (trial % 5) is always clear, so a group can
			 * always be formed and D281's uninitialised read is
			 * never taken.
			 */
			short flag = (short)((p == trial % 5) ? 0
					     : ((trial >> p) & 1));

			BOTH(altRbsFlag[p], flag);
			BOTH(linMapp[p][at], mp[p]);
			BOTH(magnitudeCount[p][at], cp[p]);
			BOTH(magnitudeSum[p][at], (float)((int)cp[p] * 3));
			BOTH(magnitudeSqSum[p][at], (float)((int)cp[p] * 41));
			BOTH(linearMappingVar[p][at], 0.25f * (float)p);

			if (flag != 0)
				sawflag = 1;
			if (cp[p] != 0)
				nonzerototal = 1;
			else
				zerototal = 1;
		}

		for (p = 0; p < NPHASE - 1; p++)
			for (q = p + 1; q < NPHASE; q++)
				if (ours_o.altRbsFlag[p] == 0
				    && ours_o.altRbsFlag[q] == 0
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
		BOTH(uniteUrefDistanceThresh, 20);
		for (p = 0; p < NPHASE; p++) {
			BOTH(altRbsFlag[p], (short)(p == NPHASE - 1 ? 1 : 0));
			BOTH(linMapp[p][at], e[p]);
			BOTH(magnitudeCount[p][at], 1u);
			BOTH(magnitudeSum[p][at], fs[p]);
			BOTH(magnitudeSqSum[p][at], fs[p] * 4.0f);
			BOTH(linearMappingVar[p][at], 0.125f * (float)p);
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
		BOTH(uniteUrefDistanceThresh, (short)(1 << (trial % 12)));

		for (p = 0; p < NPHASE; p++) {
			unsigned n = counts[(trial + p) % 8];

			BOTH(altRbsFlag[p], (short)((p == trial % 5) ? 0
						    : ((trial >> p) & 1)));
			BOTH(linMapp[p][at], (short)(100 * p + trial));
			BOTH(magnitudeCount[p][at], n);
			BOTH(magnitudeSum[p][at], (float)((int)n * 143));
			BOTH(magnitudeSqSum[p][at], (float)((int)n * 4001));
			BOTH(linearMappingVar[p][at], 0.5f * (float)p);

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
			    (long)ours_o.magnitudeCount[0][at], 0, trial);

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
		BOTH(uniteUrefDistanceThresh, 2);
		for (p = 0; p < NPHASE; p++) {
			int last = (p == NPHASE - 1);

			BOTH(altRbsFlag[p], (short)(p < 4 ? 1 : 0));
			BOTH(linMapp[p][at],
			     (short)(last ? 30000 : 100 * p));
			BOTH(magnitudeCount[p][at], (unsigned)(last ? 2 : 4));
			BOTH(magnitudeSum[p][at], last ? 287.0f : 400.0f);
			BOTH(magnitudeSqSum[p][at], last ? 8000.0f : 100.0f);
			BOTH(linearMappingVar[p][at], 0.0f);
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

/*
 * ==========================================================================
 * THE FOUR STUDY METHODS, and the one thing that is new about testing them:
 * THEY TALK.
 *
 * Between them they make twelve `edprintf` calls and one
 * `dsplibs_debug_printf`, and a format string or an argument list is exactly
 * the kind of claim a whole-object comparison is blind to -- finding F126 is
 * `updateAlpha`, which had all three wrong and passed everything.  So these
 * sweeps raise BOTH debug levels, turn the harness's capture on, and compare
 * the two transcripts as well as the two objects.
 *
 * WHAT IS COMPARED IS THE ENCODED TEXT, not the readable one.  `edprintf`
 * runs its formatted output through the rotating key before handing it to
 * `dsplibs_debug_printf`, and `dsplib_encode_plain` -- which would show the
 * readable form -- is ours and not the object's (D40), so turning it on would
 * make our transcript differ from the blob's for a reason that is not a
 * defect.  The encoding is a deterministic function of the formatted text and
 * the key is reset at the head of every successful call, so comparing the
 * encoded form compares the formatted form exactly.
 *
 * The capture buffer is 16 KB and `resetStudyUrefHandler` alone writes about
 * 810 bytes per side per call, so it is reset every trial rather than once.
 * ==========================================================================
 */
static void
study_debug_on(void)
{
	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2u;
}

static void
study_debug_off(void)
{
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0u;
}

/*
 * resetStudyUrefHandler.
 *
 * The argument is a flag and the two branches share nothing: zero copies six
 * words out of the parameter block at +0x348, installs five constants and
 * returns without printing, and nonzero copies a different six from +0x4a8
 * and derives every threshold from `1.0f / float_a950`.  Both are swept, and
 * the run asserts each was taken.
 *
 * THE GAIN IS WHAT MAKES THE THREE CLAMPS TESTABLE.  Each of +0xa9a8, +0xa9ac
 * and +0xa9ae is `constant / gain` limited to the value the zero branch would
 * have installed, and the three limits are reached together at a gain of 1
 * and missed together at a gain of 2 -- so the sweep has to carry both, and
 * it asserts that it did.  A gain of zero is in the table as well: the
 * division is NOT guarded, so it produces infinities that every `fistp` turns
 * into 0x8000, and only the seeding of the mapping tables is skipped.
 *
 * `ucode` is swept over the whole byte without clamping.  The seeding writes
 * `linMapp[5][ucode]` and `linMappAlt[5][ucode]`, and at 0xff those are
 * +0x6fe and +0xcfe -- inside `linMappAlt` and inside `prevLinMapp`, both
 * well within the object.  The out-of-row index is the object's behaviour and
 * is exercised rather than avoided.
 */
static int
run_studyreset(void)
{
	static const float gain[] = {
		1.0f, 2.0f, 0.5f, 0.0f, 0.9f, 100.0f, -4.0f, 1.0e-30f
	};
	static const unsigned int qcv[] = {
		0u, 1u, 0u, 2u, 0xffffffffu, 0u, 0x80000000u, 7u
	};
	static const short level[] = {
		0, 1, -1, 8031, (short)0x8000, 4096, -4096, 32767
	};
	int trial, moved = 0, distinct = 0;
	int sawqc = 0, sawplain = 0, sawzerogain = 0, sawgain = 0;
	int clamped = 0, unclamped = 0, printed = 0, silent = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::resetStudyUrefHandler");
	study_debug_on();

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		unsigned int qc = qcv[IDX(trial, 1)];
		float g = gain[IDX(trial, 3)];

		seed(trial, trial % 4);
		BOTH(float_a950, g);
		BOTH(ucode, (unsigned char)((trial * 41) & 0xff));
		BOTH(ucodeLevel, level[IDX(trial, 5)]);
		memcpy(params_copy, params_block, PARAMS_BYTES);
		memcpy(before, ours.raw, SLOT);
		dsplib_debug_capture_reset();

		ours_o.resetStudyUrefHandler(qc);
		ref_resetStudyUrefHandler(&theirs_o, qc);

		diff_eq_obj("after resetStudyUrefHandler",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("resetStudyUrefHandler wrote nothing through "
			    "params (trial %ld)",
			    memcmp(params_copy, params_block, PARAMS_BYTES), 0,
			    trial);
		diff_eq_int("the QC report matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);

		if (qc == 0) {
			sawplain = 1;
			if (dsplib_debug_capture_lines(0) == 0)
				silent = 1;
		} else {
			sawqc = 1;
			if (dsplib_debug_capture_lines(0) == 8)
				printed = 1;
			if (g == 0.0f)
				sawzerogain = 1;
			else
				sawgain = 1;
			if (ours_o.neighborUcodeMaxDistance == 6000
			    && ours_o.neighborUcodeMinDistance == 2500)
				clamped = 1;
			else if (ours_o.neighborUcodeMaxDistance < 6000
				 && ours_o.neighborUcodeMaxDistance > 0)
				unclamped = 1;
		}

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.uniteUrefDistanceThresh;
		else if (ours_o.uniteUrefDistanceThresh != first)
			distinct = 1;
	}

	/*
	 * A DIRECTED GAIN GRID.
	 *
	 * The five thresholds are `constant / gain` rounded to an integer and
	 * then clamped, and the eight gains above are all round numbers -- so a
	 * constant moved by less than one in a thousand lands on the same
	 * integer at every one of them.  Measured: the mutations that moved
	 * 2777.7778 to 2777 and 6666.667 to 6666 both survived the sweep.
	 *
	 * Ninety-six gains from 1.0 up in steps of 1/80 put the reciprocal
	 * between 0.45 and 1, which is below the clamps for most of the range
	 * and moves each scaled constant by about 35 per step -- so the
	 * fractional part sweeps the whole unit interval several times over and
	 * a shift of 0.7 crosses an integer many times.
	 */
	{
		int k;

		study_debug_on();
		for (k = 0; k < 96; k++) {
			seed(920 + k, 0);
			BOTH(float_a950, 1.0f + (float)k * 0.0125f);
			BOTH(ucode, (unsigned char)(0x2a + (k & 7)));
			BOTH(ucodeLevel, (short)(1000 + k * 13));
			dsplib_debug_capture_reset();

			ours_o.resetStudyUrefHandler(1u);
			ref_resetStudyUrefHandler(&theirs_o, 1u);
			diff_eq_obj("studyreset: the gain grid",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, k);
			diff_eq_int("the gain grid's report matched (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, k);
		}
		diff_eq_int("no store past the object (gain grid)",
			    guard_equal(), 1, 0);
	}

	study_debug_off();

	diff_eq_int("resetStudyUrefHandler changed the object", moved, 1, 0);
	diff_eq_int("the unite threshold is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("the QC branch was exercised", sawqc, 1, 0);
	diff_eq_int("the default branch was exercised", sawplain, 1, 0);
	diff_eq_int("a zero gain was exercised", sawzerogain, 1, 0);
	diff_eq_int("a nonzero gain was exercised", sawgain, 1, 0);
	diff_eq_int("the neighbour clamps were reached", clamped, 1, 0);
	diff_eq_int("the neighbour clamps were missed", unclamped, 1, 0);
	diff_eq_int("the QC branch printed eight lines", printed, 1, 0);
	diff_eq_int("the default branch printed nothing", silent, 1, 0);

	return diff_end();
}

/*
 * getAltVarThresh.
 *
 * The six variances live in the CALLER's array, so both sides are handed the
 * same pointer and the array is checked afterwards for a store the method has
 * no business making.
 *
 * THE COUNT CAN BE ZERO AND THE RESULT IS THEN A NaN.  Nothing is below the
 * average when all six entries are equal -- which a freshly cleared object
 * gives -- so the divisor is zero, the mean is 0.0f/0 and the return value is
 * the x87 real indefinite.  That is why the return is compared as a bit
 * pattern: `==` is false for a NaN on both sides and would pass for ever.
 * D282, and the table below carries the constant row that reaches it.
 *
 * THE NaN ROW IS NOT DECORATION EITHER.  The object accumulates an entry when
 * `jae` is NOT taken, and an unordered compare sets CF -- so a NaN variance
 * JOINS the average where the readable spelling `var[i] < lim` would exclude
 * it.  One 32-bit pattern in 128 is a NaN, so a seeded object reaches this and
 * the reconstruction has to have the branch the right way round.
 */
static int
run_altvarthresh(void)
{
	static const float vars[8][NPHASE] = {
		{ 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f },
		{ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f },
		{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f },
		{ -1.0f, -2.0f, 3.0f, 4.0f, -5.0f, 600.0f },
		{ 1.0e-40f, 2048.0f, 0.1f, -0.1f, -32768.0f, 1.0e9f },
		{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
		{ 25.0f, 25.0f, 25.0f, 25.0f, 25.0f, 1.0f },
		{ 4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f }
	};
	static const float minv[] = {
		0.0f, 1.0f, 1.0e6f, -1.0f, 100.0f, 1.0e-30f, 4000.0f, 0.5f
	};
	int trial, floored = 0, unfloored = 0, nanned = 0, finite = 0;
	int distinct = 0;
	unsigned int first = 0;

	diff_begin("V90AutoDigitalImpDetector::getAltVarThresh");
	study_debug_on();

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		float var[NPHASE], varcopy[NPHASE];
		float factor = fsweep[IDX(trial, 1)];
		unsigned int got, want;
		int i;

		seed(trial, trial % 4);
		BOTH(altMinVarThresh, minv[IDX(trial, 5)]);

		for (i = 0; i < NPHASE; i++)
			var[i] = vars[IDX(trial, 3)][i];

		/*
		 * A NaN variance on every eighth trial, which is the row the
		 * merge-sense claim rests on.
		 */
		if ((trial & 7) == 4)
			var[trial % NPHASE] = __builtin_nanf("");

		memcpy(varcopy, var, sizeof var);
		memcpy(before, ours.raw, SLOT);
		dsplib_debug_capture_reset();

		got = fbits(ours_o.getAltVarThresh(var, factor));
		want = fbits(ref_getAltVarThresh(&theirs_o, var, factor));

		diff_eq_int("getAltVarThresh (trial %ld)", got, want, trial);
		diff_eq_int("the variance array is unchanged (trial %ld)",
			    memcmp(varcopy, var, sizeof var), 0, trial);
		diff_eq_int("getAltVarThresh stored nothing (trial %ld)",
			    memcmp(before, ours.raw, SLOT), 0, trial);
		diff_eq_obj("getAltVarThresh left both objects alike",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("the threshold report matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed three lines (trial %ld)",
			    (long)dsplib_debug_capture_lines(0), 3, trial);

		if ((got & 0x7f800000u) == 0x7f800000u
		    && (got & 0x007fffffu) != 0)
			nanned = 1;
		else
			finite = 1;
		if (got == fbits(ours_o.altMinVarThresh))
			floored = 1;
		else
			unfloored = 1;
		if (trial == 0)
			first = got;
		else if (got != first)
			distinct = 1;
	}

	study_debug_off();

	diff_eq_int("the threshold is not the same on every trial", distinct, 1,
		    0);
	diff_eq_int("an empty below-average set was exercised", nanned, 1, 0);
	diff_eq_int("a non-empty below-average set was exercised", finite, 1,
		    0);
	diff_eq_int("the floor at altMinVarThresh was applied", floored, 1, 0);
	diff_eq_int("the floor at altMinVarThresh was not applied", unfloored,
		    1, 0);

	return diff_end();
}

/*
 * uniteLinMappInfoOfUnsuspectedPhases.
 *
 * The same shape as `run_unite`, and driven from the same kind of tables, but
 * every decision is a different one: the flag is the BYTE at +0x280c and not
 * the short at +0x2800, the merge test is `d*d < variance/4` and not a
 * distance against `uniteUrefDistanceThresh`, and there is no convergence loop.  So the
 * per-phase variance at +0x9d48 has to be forced as well, and it is what the
 * threshold sweep runs on.
 *
 * ONE ARM IS DELIBERATELY NOT REACHED, for D284: with all five of phases 0..4
 * flagged the object forms no group and reads `bestGroup` uninitialised.  The
 * flag pattern below always leaves phase `trial % 5` clear.
 *
 * THE ZERO POOLED TOTAL IS A DISTINCT ARM AND NOT JUST A ZERO MEAN: it
 * returns before the clearing loop, so the three accumulators keep whatever
 * they held.  Every other path empties them.  The seeded fill is what turns
 * that into a comparison -- a zeroed object could not tell "left alone" from
 * "cleared".
 */
static int
run_uniteunsuspected(void)
{
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
	static const float vars[8] = {
		0.0f, 4.0f, 400.0f, 40000.0f, 4000000.0f, 1.0f, 1.0e12f,
		-1.0f
	};
	static const unsigned counts[8][NPHASE] = {
		{ 1, 2, 3, 4, 5, 6 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 0, 41, 0, 41, 0, 41 },
		{ 10, 0, 0, 0, 0, 0 },
		{ 25, 25, 25, 25, 25, 25 },
		{ 0, 0, 7, 0, 0, 0 },
		{ 100, 200, 300, 400, 500, 600 },
		/* Pools to -25,536 in the object's `short` -- see run_unite. */
		{ 40000, 0, 41, 0, 41, 0 }
	};
	int trial, moved = 0, distinct = 0;
	int merged = 0, unmerged = 0, zerototal = 0, nonzerototal = 0;
	int anyflagged = 0, noneflagged = 0, nanvar = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::uniteLinMappInfoOfUnsuspected"
		   "Phases");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		unsigned char at = (unsigned char)((trial * 23) & 0x7f);
		const short *mp = maps[IDX(trial, 1)];
		const unsigned *cp = counts[IDX(trial, 3)];
		float v = vars[IDX(trial, 5)];
		int p, q, sawpair = 0, sawflag = 0;

		seed(trial, trial % 4);

		for (p = 0; p < NPHASE; p++) {
			/* Phase (trial % 5) is always clear -- D284. */
			unsigned char flag =
			    (unsigned char)((p == trial % 5) ? 0
					    : ((trial >> p) & 1));

			BOTH(byte_280c[p], flag);
			BOTH(linMapp[p][at], mp[p]);
			BOTH(magnitudeCount[p][at], cp[p]);
			BOTH(magnitudeSum[p][at], (float)((int)cp[p] * 3));
			BOTH(magnitudeSqSum[p][at], (float)((int)cp[p] * 41));
			/*
			 * THE VARIANCE VARIES WITH THE PHASE, which is what
			 * makes "a quarter of the LEADER's variance" a claim:
			 * a table that gave all six the same value agrees with
			 * one that reads the candidate's, and the mutation
			 * that swaps them was NOT CAUGHT until this multiplier
			 * existed.
			 */
			BOTH(linearMappingVar[p][at], v * (float)(p + 1));

			if (flag != 0)
				sawflag = 1;
			if (cp[p] != 0)
				nonzerototal = 1;
			else
				zerototal = 1;
		}

		/*
		 * A NaN variance on the leader every eighth trial: the object
		 * merges on an unordered compare, so this row is what pins the
		 * sense of the test rather than only its value.
		 */
		if ((trial & 7) == 6) {
			BOTH(linearMappingVar[trial % 5][at], __builtin_nanf(""));
			nanvar = 1;
		}

		for (p = 0; p < NPHASE - 1; p++)
			for (q = p + 1; q < NPHASE; q++)
				if (ours_o.byte_280c[p] == 0
				    && ours_o.byte_280c[q] == 0) {
					float d = (float)(mp[p] - mp[q]);
					float lim = v * (float)(p + 1) * 0.25f;

					if (!(d * d >= lim))
						sawpair = 1;
				}

		if (sawpair)
			merged = 1;
		else
			unmerged = 1;
		if (sawflag)
			anyflagged = 1;
		else
			noneflagged = 1;

		memcpy(before, ours.raw, SLOT);

		ours_o.uniteLinMappInfoOfUnsuspectedPhases(at);
		ref_uniteLinMappInfoOfUnsuspectedPhases(&theirs_o, at);

		diff_eq_obj("after uniteLinMappInfoOfUnsuspectedPhases",
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
	 * A DIRECTED BLOCK FOR THE RUNNING GROUP SIZE (D283).
	 *
	 * The object sets the size to 1 ONCE, outside the scan, so the second
	 * group starts counting from where the first stopped and the "biggest"
	 * group is whichever merged last.  Two groups of two, phases 0-1 and
	 * 2-3, with different means: reset-per-group picks the first (size 2
	 * against a running best of 2, which is not greater), and the object's
	 * running counter makes the second group size 3 and picks IT.  The two
	 * spellings write different means into every unsuspected phase.
	 *
	 * Phases 4 and 5 are suspected so that they neither join a group nor
	 * receive the answer, which leaves the pooled mean visible.
	 */
	{
		static const short e[NPHASE] = { 100, 101, 900, 901, 0, 0 };
		static const float fs[NPHASE] = {
			100.0f, 100.0f, 900.0f, 900.0f, 0.0f, 0.0f
		};
		unsigned char at = 0x2d;
		int p;

		seed(903, 0);
		for (p = 0; p < NPHASE; p++) {
			BOTH(byte_280c[p], (unsigned char)(p >= 4 ? 1 : 0));
			BOTH(linMapp[p][at], e[p]);
			BOTH(magnitudeCount[p][at], 1u);
			BOTH(magnitudeSum[p][at], fs[p]);
			BOTH(magnitudeSqSum[p][at], fs[p] * fs[p]);
			BOTH(linearMappingVar[p][at], 400.0f);
		}

		ours_o.uniteLinMappInfoOfUnsuspectedPhases(at);
		ref_uniteLinMappInfoOfUnsuspectedPhases(&theirs_o, at);
		diff_eq_obj("unite: the running group size",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("no store past the object (group-size block)",
			    guard_equal(), 1, 0);
	}

	/*
	 * A DIRECTED BLOCK FOR THE RECIPROCAL, which is finding F1366's witness
	 * again: this method forms `1.0f / total` and multiplies, and a
	 * straight `fsum / total` agrees over every table above.  Count 41
	 * against a sum of 143.5 divides to exactly 3.5, so the `+ 0.5f` lands
	 * on 4.0 and the truncating `fistp` stores 4; the reciprocal is a hair
	 * under and stores 3.  One phase only in the group, so the pooled sum
	 * is the phase's own and the witness is not diluted.
	 */
	{
		unsigned char at = 0x37;
		int p;

		seed(904, 0);
		for (p = 0; p < NPHASE; p++) {
			BOTH(byte_280c[p], (unsigned char)(p == 0 ? 0 : 1));
			BOTH(linMapp[p][at], (short)(p * 1000));
			BOTH(magnitudeCount[p][at], (unsigned)(p == 0 ? 41 : 0));
			BOTH(magnitudeSum[p][at], p == 0 ? 143.5f : 0.0f);
			BOTH(magnitudeSqSum[p][at], p == 0 ? 600.0f : 0.0f);
			BOTH(linearMappingVar[p][at], 4.0f);
		}

		ours_o.uniteLinMappInfoOfUnsuspectedPhases(at);
		ref_uniteLinMappInfoOfUnsuspectedPhases(&theirs_o, at);
		diff_eq_obj("unite: the reciprocal witness",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("no store past the object (reciprocal block)",
			    guard_equal(), 1, 0);
	}

	/*
	 * A DIRECTED BLOCK FOR THE "ALREADY GROUPED" TEST.
	 *
	 * The inner loop skips a phase that already has a group number, and
	 * nothing in a sweep separates that from skipping only the suspected
	 * ones: it needs a phase that a LATER leader would also have taken.
	 * Entries 0, 250 and 100 with a tolerance of 200 do it -- phase 0
	 * takes phase 2 (100 away) and cannot reach phase 1 (250 away), and
	 * phase 1 then leads and IS within 200 of phase 2, which the object
	 * refuses and the mutation allows.  The variance is 160,000, whose
	 * quarter is 40,000 = 200 squared.
	 *
	 * It doubles as the witness for the quarter: at half the variance the
	 * tolerance is 283 and phase 0 takes phase 1 as well, so the first
	 * group is different from the first instruction on.
	 */
	{
		static const short e[NPHASE] = { 0, 250, 100, 0, 0, 0 };
		static const float fs[NPHASE] = {
			10.0f, 700.0f, 400.0f, 0.0f, 0.0f, 0.0f
		};
		unsigned char at = 0x41;
		int p;

		seed(905, 0);
		for (p = 0; p < NPHASE; p++) {
			BOTH(byte_280c[p], (unsigned char)(p >= 3 ? 1 : 0));
			BOTH(linMapp[p][at], e[p]);
			BOTH(magnitudeCount[p][at], (unsigned)(p + 1));
			BOTH(magnitudeSum[p][at], fs[p]);
			BOTH(magnitudeSqSum[p][at], fs[p] * 8.0f);
			BOTH(linearMappingVar[p][at], 160000.0f);
		}

		ours_o.uniteLinMappInfoOfUnsuspectedPhases(at);
		ref_uniteLinMappInfoOfUnsuspectedPhases(&theirs_o, at);
		diff_eq_obj("unite: a phase a later leader could also take",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("no store past the object (regroup block)",
			    guard_equal(), 1, 0);
	}

	/*
	 * A DIRECTED BLOCK FOR WHOSE VARIANCE THE TOLERANCE COMES FROM.
	 *
	 * The object reads `linearMappingVar[LEADER][at]`, and a table that gives the
	 * two phases the same variance cannot tell that from reading the
	 * candidate's -- measured: the mutation that swaps the index survived
	 * even the per-phase multiplier above.  So: a distance of 150, a
	 * leader variance of 40,000 whose quarter is 10,000, and a candidate
	 * variance of 400,000 whose quarter is 100,000.  22,500 is above the
	 * first and below the second, so the object refuses the merge and the
	 * mutation makes it.
	 */
	{
		unsigned char at = 0x53;
		int p;

		seed(906, 0);
		for (p = 0; p < NPHASE; p++) {
			BOTH(byte_280c[p], (unsigned char)(p >= 2 ? 1 : 0));
			BOTH(linMapp[p][at], (short)(p == 1 ? 150 : 0));
			BOTH(magnitudeCount[p][at], (unsigned)(p + 3));
			BOTH(magnitudeSum[p][at], (float)(p * 700 + 90));
			BOTH(magnitudeSqSum[p][at], (float)(p * 9000 + 500));
			BOTH(linearMappingVar[p][at], p == 0 ? 40000.0f : 400000.0f);
		}

		ours_o.uniteLinMappInfoOfUnsuspectedPhases(at);
		ref_uniteLinMappInfoOfUnsuspectedPhases(&theirs_o, at);
		diff_eq_obj("unite: the tolerance is the leader's",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("no store past the object (tolerance block)",
			    guard_equal(), 1, 0);
	}

	diff_eq_int("uniting the unsuspected changed the object", moved, 1, 0);
	diff_eq_int("the united entry is not the same on every trial", distinct,
		    1, 0);
	diff_eq_int("a mergeable pair was offered", merged, 1, 0);
	diff_eq_int("a trial with nothing to merge was offered", unmerged, 1,
		    0);
	diff_eq_int("a zero pooled count was exercised", zerototal, 1, 0);
	diff_eq_int("a nonzero pooled count was exercised", nonzerototal, 1, 0);
	diff_eq_int("a suspected phase was exercised", anyflagged, 1, 0);
	diff_eq_int("a trial with no suspected phase was exercised", noneflagged,
		    1, 0);
	diff_eq_int("a NaN leader variance was exercised", nanvar, 1, 0);

	return diff_end();
}

/*
 * porcessFirstStudy.
 *
 * THE STUDY IS OVER SIXTEEN CODES, 0x40..0x4f, and the sweep has to put the
 * counts and the sums there rather than anywhere in the row: everything
 * outside that window is only ever cleared.  `ucode` is swept over the whole
 * byte, and the values inside the window are what reach the `ucode == code`
 * skip.
 *
 * NOTHING IS CLAMPED HERE.  The clearing loops index `[phase][k]` with a byte
 * `k`, and the worst case the method can reach is phase 5, k = 254 -- which
 * lands at +0x9f14 in `magnitudeSqSum`'s row, 2,716 bytes short of the end of the
 * object.  The unbounded index is exercised for real rather than avoided.
 *
 * THE DEBUG LEVEL IS PART OF THE SWEEP.  This is the class's only member with
 * a gate on it, and the gated branch reloads `ucode` from the object after the
 * call -- so running only at level 0 would leave both the branch and the
 * reload untested.  Both levels are exercised and both are asserted.
 */
static int
run_firststudy(void)
{
	static const float sigma[] = {
		1000.0f, 0.0f, 40.0f, 4000.0f, -100.0f, 1.0e9f, 0.5f, 100.0f
	};
	static const short nmin[] = { 2500, 0, 100, 1, 25, 2500, -50, 1000 };
	static const short nmax[] = { 6000, 6000, 200, 30000, 25, 4, 0, 2000 };
	static const unsigned char uc[] = {
		0x2a, 0x00, 0x40, 0x4f, 0x7f, 0xff, 0x45, 0x80
	};
	int trial, moved = 0, distinct = 0;
	int susp = 0, unsusp = 0, zerocount = 0, nonzerocount = 0;
	int skipped = 0, level0 = 0, level2 = 0;
	unsigned char firsttally = 0;

	diff_begin("V90AutoDigitalImpDetector::porcessFirstStudy");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		unsigned char at = uc[IDX(trial, 1)];
		int p, c;

		seed(trial, trial % 4);
		BOTH(trn1Sigma, sigma[IDX(trial, 3)]);
		BOTH(neighborUcodeMinDistance, nmin[IDX(trial, 5)]);
		BOTH(neighborUcodeMaxDistance, nmax[IDX(trial, 7)]);
		BOTH(ucode, at);

		for (p = 0; p < NPHASE; p++) {
			BOTH(altRbsFlag[p],
			     (short)(((trial >> p) & 1) ? p + 1 : 0));

			for (c = 0x40; c <= 0x4f; c++) {
				unsigned n = ((trial + c) & 3) == 0
					     ? 0u : (unsigned)(c - 0x3f);

				BOTH(magnitudeCount[p][c], n);
				/*
				 * A HALF IN THE MEAN, deliberately: a sum that
				 * divides exactly makes the `+ 0.5f` invisible
				 * and the mutation that drops it survived until
				 * this was 100.5 rather than 100.
				 */
				BOTH(magnitudeSum[p][c],
				     (float)n * (100.5f + 37.0f * (float)p));
				BOTH(magnitudeSqSum[p][c],
				     (float)((int)n * (20000 + 11 * c)));

				if (n == 0)
					zerocount = 1;
				else
					nonzerocount = 1;
			}

			/*
			 * A saw-tooth mapping outside the window, so the
			 * clearing pass has something varied to zero and the
			 * one spared entry is visibly spared.
			 */
			for (c = 0; c < V90ADID_CODES; c++)
				BOTH(linMapp[p][c],
				     (short)((c * 251 + p * 37) & 0x7fff));
		}

		if (at >= 0x40 && at <= 0x4f)
			skipped = 1;

		if ((trial & 1) == 0) {
			study_debug_on();
			level2 = 1;
		} else {
			level0 = 1;
		}
		dsplib_debug_capture_reset();
		memcpy(before, ours.raw, SLOT);

		ours_o.porcessFirstStudy();
		ref_porcessFirstStudy(&theirs_o);

		diff_eq_obj("after porcessFirstStudy",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("the study report matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);
		diff_eq_int("the gated line is there exactly when the level is "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (trial & 1) == 0 ? 2 : 0, trial);
		study_debug_off();

		for (p = 0; p < NPHASE; p++) {
			if (ours_o.byte_280c[p] != 0)
				susp = 1;
			else
				unsusp = 1;
		}

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			firsttally = ours_o.suspectedPhaseCount;
		else if (ours_o.suspectedPhaseCount != firsttally)
			distinct = 1;
	}

	/*
	 * A DIRECTED BLOCK FOR THE ROUGHNESS COUNT.
	 *
	 * The counter starts at 1 and the phase is trusted only when it ends
	 * above 9, so the boundary is at exactly nine large differences: eight
	 * leaves it at 9 and suspected, nine takes it to 10 and unsuspected.
	 * A sweep never lands there -- the mappings above are either flat or
	 * saw-toothed all the way -- and without the boundary neither the
	 * initial 1 nor the `> 9` is a tested claim.
	 *
	 * All sixteen counts are zero so the mean loop leaves the mapping
	 * exactly as it is written here, and `ucode` is outside the window so
	 * nothing is skipped.  The threshold is 2500 and the two levels are
	 * 0 and 5000, so a step is 25,000,000 and a flat run is 0.
	 */
	{
		int k;

		for (k = 7; k <= 10; k++) {
			int p, c;

			seed(910 + k, 0);
			BOTH(trn1Sigma, 1000.0f);
			BOTH(neighborUcodeMinDistance, 2500);
			BOTH(neighborUcodeMaxDistance, 6000);
			BOTH(ucode, 0x2a);

			for (p = 0; p < NPHASE; p++) {
				BOTH(altRbsFlag[p], 0);
				for (c = 0x40; c <= 0x4f; c++) {
					int i = c - 0x40;
					int j = i <= k ? i : k;

					BOTH(magnitudeCount[p][c], 0u);
					BOTH(linMapp[p][c],
					     (short)((j & 1) ? 5000 : 0));
				}
			}

			ours_o.porcessFirstStudy();
			ref_porcessFirstStudy(&theirs_o);
			diff_eq_obj("porcessFirstStudy: the roughness boundary",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, k);
			diff_eq_int("no store past the object (boundary %ld)",
				    guard_equal(), 1, k);
			diff_eq_int("nine differences trust the phase (%ld)",
				    ours_o.byte_280c[0] == 0, k >= 9 ? 1 : 0,
				    k);

			if (ours_o.byte_280c[0] == 0)
				unsusp = 1;
			else
				susp = 1;
		}
	}

	/*
	 * A DIRECTED BLOCK FOR THE FIRST STUDY'S OWN ARITHMETIC AND ITS REPORT.
	 *
	 * Two claims that the sweep above cannot separate, in one state:
	 *
	 * Count 41 against a sum of 143.5 is finding F1366's witness again --
	 * the division is exactly 3.5 and rounds to 4, the reciprocal is a hair
	 * under and truncates to 3.  It pins the `1.0f / count` spelling AND
	 * the `+ 0.5f`, both of which survived the sweep.
	 *
	 * A sigma of 100.3 makes 2.5 * it 250.75, whose reported value is 251
	 * with the rounding term and 250 without.  Every sigma in the table
	 * above is a whole number after scaling, so the mutation that drops the
	 * rounding from the report was invisible; the clamps are set wide so
	 * that neither of them hides it.
	 */
	{
		int p, c;

		seed(930, 0);
		BOTH(trn1Sigma, 100.3f);
		BOTH(neighborUcodeMinDistance, 10);
		BOTH(neighborUcodeMaxDistance, 30000);
		BOTH(ucode, 0x2a);

		for (p = 0; p < NPHASE; p++) {
			BOTH(altRbsFlag[p], 0);
			for (c = 0x40; c <= 0x4f; c++) {
				BOTH(magnitudeCount[p][c], 41u);
				BOTH(magnitudeSum[p][c], 143.5f);
				BOTH(magnitudeSqSum[p][c], 600.0f);
			}
		}

		study_debug_on();
		dsplib_debug_capture_reset();
		ours_o.porcessFirstStudy();
		ref_porcessFirstStudy(&theirs_o);
		diff_eq_obj("porcessFirstStudy: the reciprocal witness",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("the fractional report matched",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, 0);
		diff_eq_int("no store past the object (witness block)",
			    guard_equal(), 1, 0);
		study_debug_off();
	}

	/*
	 * A DIRECTED BLOCK FOR THE FIRST STUDY'S MEAN, WHICH IS OTHERWISE DEAD.
	 *
	 * EVERY MAPPING ENTRY THE MEAN LOOP WRITES IS CLEARED BY THE SAME CALL:
	 * the clearing pass at the end covers 0..127 except `ucode`, and the
	 * mean loop writes 0x40..0x4f except `ucode`.  So the rounded mean
	 * leaves the object only through the roughness count between them --
	 * and that is a comparison against a threshold, which a mean that is
	 * one code out crosses only if the differences are arranged to straddle
	 * it.  Both the mutation that drops the `+ 0.5f` and the one that
	 * divides instead of multiplying by the reciprocal survived every other
	 * state in this file for exactly that reason.
	 *
	 * Both witnesses put the mean on alternate codes and leave the codes
	 * between them empty with a mapping entry of zero, so the fifteen
	 * neighbouring differences are all the mean itself and one threshold
	 * decides the whole phase:
	 *
	 *   count 41, sum 143.5, threshold 12.  Dividing gives exactly 3.5,
	 *   which the `+ 0.5f` lifts to 4.0 and the truncation keeps as 4 --
	 *   16 against the threshold, fifteen rough neighbours, trusted.  The
	 *   reciprocal is a hair under, stores 3, and 9 is below the threshold
	 *   -- suspected.  The object takes the reciprocal, so the ASSERTION
	 *   below is that the phase comes out SUSPECTED.
	 *
	 *   count 2, sum 21, threshold 110.  A half is exact either way, so
	 *   this one says nothing about the division -- it separates the
	 *   rounding: 10.5 rounds to 11 and 121 is above the threshold, and
	 *   truncating to 10 puts 100 below it.
	 */
	{
		static const unsigned cnt[2] = { 41u, 2u };
		static const float sum[2] = { 143.5f, 21.0f };
		static const float sig[2] = { 4.8f, 44.0f };
		static const int want[2] = { 1, 0 };
		int w;

		for (w = 0; w < 2; w++) {
			int p, c;

			seed(931 + w, 0);
			BOTH(trn1Sigma, sig[w]);
			BOTH(neighborUcodeMinDistance, 0);
			BOTH(neighborUcodeMaxDistance, 30000);
			BOTH(ucode, 0x2a);

			for (p = 0; p < NPHASE; p++) {
				BOTH(altRbsFlag[p], 0);
				for (c = 0x40; c <= 0x4f; c++) {
					int odd = c & 1;

					BOTH(magnitudeCount[p][c],
					     odd ? cnt[w] : 0u);
					BOTH(magnitudeSum[p][c],
					     odd ? sum[w] : 0.0f);
					BOTH(magnitudeSqSum[p][c],
					     odd ? 600.0f : 0.0f);
					BOTH(linMapp[p][c], 0);
				}
			}

			ours_o.porcessFirstStudy();
			ref_porcessFirstStudy(&theirs_o);
			diff_eq_obj("porcessFirstStudy: the mean inside the "
				    "roughness count",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, w);
			diff_eq_int("the witness lands where it was aimed (%ld)",
				    ours_o.byte_280c[0] != 0, want[w], w);
			diff_eq_int("no store past the object (roughness %ld)",
				    guard_equal(), 1, w);
		}
	}

	diff_eq_int("porcessFirstStudy changed the object", moved, 1, 0);
	diff_eq_int("the suspect tally is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("a suspected phase came out", susp, 1, 0);
	diff_eq_int("an unsuspected phase came out", unsusp, 1, 0);
	diff_eq_int("a zero count was exercised", zerocount, 1, 0);
	diff_eq_int("a nonzero count was exercised", nonzerocount, 1, 0);
	diff_eq_int("a reference code inside the studied window was exercised",
		    skipped, 1, 0);
	diff_eq_int("the gate was exercised open", level2, 1, 0);
	diff_eq_int("the gate was exercised shut", level0, 1, 0);

	return diff_end();
}

/*
 * ==========================================================================
 * THE DIL BATCH: `updateAltRbsPhaseInDil` and the two members that call it.
 *
 * THE SAMPLE-STORE CURSOR IS THE THING TO BE CAREFUL WITH.
 * `updateAltRbsPhaseInDil` walks a cursor forward by `codeHistogram[phase][code]`
 * for each of the 115 codes in its scan order and writes
 * `sampleStore[phase][cursor + j]`, and nothing anywhere compares the cursor
 * against the row's 0x83e entries -- D287.  A seeded histogram holds random
 * shorts whose sum over 115 codes is around 1.9 million, so the writes would
 * leave the object inside the first few codes and the two sides would be
 * scribbling on two different pieces of unrelated memory.  That is not a test
 * of anything, so the histogram is clamped, and the clamp is the reason the
 * per-code counts below are small.
 *
 * THE REFERENCE ROW IS DELIBERATELY COARSE ON HALF THE TRIALS.  Every sample
 * is quantised onto `linMapp[unSuspectedPhase]` before the popularity count
 * runs, so a row of 112 distinct random entries makes runs of equal samples
 * vanishingly unlikely -- and then the count, the -1 marking and the
 * `maxCount` comparison never do anything at all.  Three distinct levels make
 * runs the common case.
 * ==========================================================================
 */

/*
 * Sixteen samples per code at most: 115 codes of that is 1,840 against the
 * row's 2,110, and the zeros in the sequence are what exercise the empty-code
 * arm.
 */
static void
sane_histogram(int trial)
{
	int p, c;

	for (p = 0; p < NPHASE; p++)
		for (c = 0; c < V90ADID_CODES; c++) {
			short n = (short)((unsigned)(trial * 7 + p * 13
						     + c * 5) % 17u);

			ours_o.codeHistogram[p][c] = theirs_o.codeHistogram[p][c] = n;
		}
}

/*
 * updateAltRbsPhaseInDil.
 *
 * ONLY THE PHASES FLAGGED AT +0x2800 ARE TOUCHED, and they are also the only
 * ones that print -- the flag test jumps past the report as well as past the
 * work.  Both arms are swept and both are asserted.
 *
 * THE TRANSCRIPT IS 128 LINES PER FLAGGED PHASE and the capture buffer is
 * 16 KB a side, so a level-2 trial flags exactly one phase and the wider flag
 * patterns are carried by the level-0 trials.  Six flagged phases would print
 * about 23 KB and the comparison would be of two truncations.
 */
static int
run_dilrepair(void)
{
	static const short lev[3] = { 96, 1024, -4000 };
	int trial, moved = 0, distinct = 0;
	int flagged = 0, unflagged = 0, empty = 0, filled = 0;
	int popular = 0, unpopular = 0, level0 = 0, level2 = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::updateAltRbsPhaseInDil");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short u = (short)(trial % NPHASE);
		int p, c;

		seed(trial, trial % 4);

		/*
		 * The reference phase is a ROW INDEX into `linMapp` and the
		 * object does not bound it: a seeded 16-bit value would read
		 * and write 8 KB either side of the table.  It is what the two
		 * callers leave behind, so the sweep leaves what they can
		 * leave -- 0 to 5.
		 */
		BOTH(unSuspectedPhase, u);
		sane_histogram(trial);

		if ((trial & 1) == 0) {
			for (p = 0; p < NPHASE; p++)
				BOTH(altRbsFlag[p],
				     (short)(p == (trial / 2) % NPHASE ? 3 : 0));
			study_debug_on();
			level2 = 1;
		} else {
			for (p = 0; p < NPHASE; p++)
				BOTH(altRbsFlag[p],
				     (short)(((trial >> p) & 1) ? p + 1 : 0));
			level0 = 1;
		}

		if ((trial & 2) == 0)
			for (c = 0; c < V90ADID_CODES; c++)
				BOTH(linMapp[u][c], lev[(c + trial) % 3]);

		for (p = 0; p < NPHASE; p++) {
			if (ours_o.altRbsFlag[p] != 0)
				flagged = 1;
			else
				unflagged = 1;
		}

		dsplib_debug_capture_reset();
		memcpy(before, ours.raw, SLOT);

		ours_o.updateAltRbsPhaseInDil();
		ref_updateAltRbsPhaseInDil(&theirs_o);

		diff_eq_obj("after updateAltRbsPhaseInDil",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store outside the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("the AltUcode report matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);
		diff_eq_int("the report is 131 lines for one flagged phase "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (trial & 1) == 0 ? 131 : 0, trial);
		study_debug_off();

		/*
		 * The scan order is exactly the codes 2..116, so those are the
		 * ones a verdict can have been written for.
		 */
		for (p = 0; p < NPHASE; p++) {
			if (ours_o.altRbsFlag[p] == 0)
				continue;

			for (c = 2; c <= 116; c++) {
				if (ours_o.codeHistogram[p][c] == 0) {
					empty = 1;
					continue;
				}
				filled = 1;
				if (ours_o.linMappAlt[p][c]
				    != ours_o.linMapp[p][c])
					popular = 1;
				else
					unpopular = 1;
			}
		}

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMappAlt[0][63];
		else if (ours_o.linMappAlt[0][63] != first)
			distinct = 1;
	}

	/*
	 * A DIRECTED BLOCK FOR THE POPULARITY COUNT, which the sweep can only
	 * reach by accident.
	 *
	 * Phase 4 is given five samples that quantise onto two levels, two of
	 * one and three of the other, and a reference entry equal to neither.
	 * The three win: `linMappAlt` gets 5000 where `linMapp` gets the
	 * reference's 777, and the sample store is left with the duplicates
	 * marked -1 -- which is the whole of how the count works and none of
	 * which a random row reaches.
	 *
	 * Phase 5 is the other arm: two samples that quantise onto the
	 * reference entry itself, so both are skipped, `maxCount` stays 0 and
	 * `linMappAlt` gets the reference rather than the unset `maxValue`.
	 *
	 * 63 IS THE FIRST CODE IN THE SCAN ORDER, which is what puts both
	 * phases' samples at cursor 0.
	 */
	{
		static const short samp[5] = { 100, 100, 5000, 5000, 5000 };
		int p, c, k;

		seed(940, 0);
		BOTH(unSuspectedPhase, 1);

		for (p = 0; p < NPHASE; p++) {
			BOTH(altRbsFlag[p], (short)(p >= 4 ? 1 : 0));
			for (c = 0; c < V90ADID_CODES; c++)
				BOTH(codeHistogram[p][c], 0);
		}
		BOTH(codeHistogram[4][63], 5);
		BOTH(codeHistogram[5][63], 2);

		for (c = 0; c < V90ADID_CODES; c++)
			BOTH(linMapp[1][c], (short)(c <= 60 ? 100 : 5000));
		BOTH(linMapp[1][63], 777);

		for (k = 0; k < 5; k++)
			BOTH(sampleStore[4][k], samp[k]);
		BOTH(sampleStore[5][0], 777);
		BOTH(sampleStore[5][1], 777);
		BOTH(linMappAlt[4][63], 12345);
		BOTH(linMappAlt[5][63], 12345);

		ours_o.updateAltRbsPhaseInDil();
		ref_updateAltRbsPhaseInDil(&theirs_o);

		diff_eq_obj("updateAltRbsPhaseInDil: the popularity count",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("no store outside the object (popularity)",
			    guard_equal(), 1, 0);
		diff_eq_int("the reference entry is copied into the mapping",
			    ours_o.linMapp[4][63], 777, 0);
		diff_eq_int("the most popular sample wins the alternate",
			    ours_o.linMappAlt[4][63], 5000, 0);
		diff_eq_int("the counted duplicates are marked",
			    ours_o.sampleStore[4][1] == -1
			    && ours_o.sampleStore[4][3] == -1
			    && ours_o.sampleStore[4][4] == -1, 1, 0);
		diff_eq_int("the run's first sample is left alone",
			    ours_o.sampleStore[4][2], 5000, 0);
		diff_eq_int("no popular sample gives the reference instead",
			    ours_o.linMappAlt[5][63], 777, 0);

		if (ours_o.linMappAlt[4][63] != ours_o.linMapp[4][63])
			popular = 1;
		if (ours_o.linMappAlt[5][63] == ours_o.linMapp[5][63])
			unpopular = 1;
	}

	diff_eq_int("updateAltRbsPhaseInDil changed the object", moved, 1, 0);
	diff_eq_int("the alternate mapping is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("a flagged phase was exercised", flagged, 1, 0);
	diff_eq_int("an unflagged phase was exercised", unflagged, 1, 0);
	diff_eq_int("a code with no samples was exercised", empty, 1, 0);
	diff_eq_int("a code with samples was exercised", filled, 1, 0);
	diff_eq_int("a popular sample was found", popular, 1, 0);
	diff_eq_int("a code with no popular sample was exercised", unpopular, 1,
		    0);
	diff_eq_int("the report was exercised", level2, 1, 0);
	diff_eq_int("the gate was exercised shut", level0, 1, 0);

	return diff_end();
}

/*
 * porcessSecondStudy.
 *
 * THE TWO SENTINELS ARE THE WHOLE OF THIS METHOD'S READING AND A SWEEP CANNOT
 * REACH EITHER.  The nearest distance starts at 32,256 per (code, phase) and
 * the second-nearest starts at a NaN ONCE for the whole call (D286), and both
 * only show when all three candidate distances are 32,256 or more -- which
 * three random shorts do about once in a hundred million.  The two directed
 * blocks below are constructed for exactly that state, one for each claim.
 *
 * THE FRONT GUARD IS WHAT MAKES CODES 0 AND 1 TESTABLE.  Their window reaches
 * `linMapp[unSuspectedPhase][-2]`, which at an unsuspected phase of 0 is four
 * bytes in front of the object; the fixture seeds sixteen bytes there alike on
 * both sides, so the two calls read the same memory and the comparison means
 * something.  Forcing the unsuspected phase away from 0 instead would have
 * left the commonest state untested.
 */
static int
run_secondstudy(void)
{
	int trial, moved = 0, distinct = 0;
	int scanstop = 0, scanfull = 0, repaired = 0, kept = 0;
	int above = 0, below = 0, level0 = 0, level2 = 0;
	int atzero = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::porcessSecondStudy");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short linbefore[NPHASE][V90ADID_CODES];
		int stop = trial % 7;
		int p, c;

		seed(trial, trial % 4);
		sane_histogram(trial);

		/*
		 * `stop` is where the scan for the unsuspected phase should
		 * stop, and 6 is the row that has nowhere to stop: all six
		 * bytes nonzero, which leaves the scan at 5 rather than at 6.
		 */
		for (p = 0; p < NPHASE; p++)
			BOTH(byte_280c[p],
			     (unsigned char)(p == stop ? 0 : 1));

		if ((trial & 1) == 0) {
			for (p = 0; p < NPHASE; p++)
				BOTH(altRbsFlag[p],
				     (short)(p == (trial / 2) % NPHASE ? 7 : 0));
			study_debug_on();
			level2 = 1;
		} else {
			for (p = 0; p < NPHASE; p++)
				BOTH(altRbsFlag[p],
				     (short)(((trial >> p) & 3) == 3 ? 1 : 0));
			level0 = 1;
		}

		memcpy(linbefore, ours_o.linMapp, sizeof linbefore);
		dsplib_debug_capture_reset();
		memcpy(before, ours.raw, SLOT);

		ours_o.porcessSecondStudy();
		ref_porcessSecondStudy(&theirs_o);

		diff_eq_obj("after porcessSecondStudy",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store outside the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("the mapping report matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);
		study_debug_off();

		diff_eq_int("the scan stopped where the flags said (trial %ld)",
			    ours_o.unSuspectedPhase, stop < 6 ? stop : 5,
			    trial);
		if (stop < 6)
			scanstop = 1;
		else
			scanfull = 1;
		if (ours_o.unSuspectedPhase == 0)
			atzero = 1;

		for (p = 0; p < NPHASE; p++) {
			if (ours_o.byte_280c[p] == 0
			    || ours_o.altRbsFlag[p] != 0)
				continue;

			for (c = 0; c <= 0x74; c++) {
				short u = ours_o.unSuspectedPhase;

				if (linbefore[p][c] > linbefore[u][c])
					above = 1;
				else
					below = 1;
				if (ours_o.linMapp[p][c] != linbefore[p][c])
					repaired = 1;
				else
					kept = 1;
			}
		}

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[1][7];
		else if (ours_o.linMapp[1][7] != first)
			distinct = 1;
	}

	/*
	 * TWO CONSTRUCTED WITNESSES, both in the state where all three
	 * candidate distances are 65,535 -- above the 32,256 the nearest
	 * distance starts at, so the nearest never improves and the decision
	 * is made entirely by what the SECOND-nearest holds.
	 *
	 * Witness 0: the second-nearest is still its sentinel, so the first
	 * distance takes it -- because the update is guarded by a `jae` that
	 * an unordered compare does not take, and the sentinel is a NaN.  The
	 * ratio is then 65535/32256 = 2.032, over the 2.0 the object compares
	 * against, and the entry is replaced with `linMapp[0][bestAt]` --
	 * where `bestAt` is the ZERO it was initialised with at the head of
	 * the method, because nothing has improved the nearest distance yet.
	 * Any ordered sentinel, and any sentinel below 65,535, leaves the
	 * entry alone.
	 *
	 * Witness 1: the same state, but preceded at code 0 by an iteration
	 * that leaves the second-nearest at 499.  The object initialises the
	 * second-nearest ONCE for the whole call, so 499 is what code 3 sees;
	 * the ratio is 499/32256 and the entry is KEPT.  A second-nearest
	 * reinitialised per (code, phase) would see the sentinel again and
	 * replace it, which is the difference the assertion measures.
	 */
	{
		int w;

		for (w = 0; w < 2; w++) {
			int p, c;

			seed(950 + w, 0);
			for (p = 0; p < NPHASE; p++) {
				BOTH(altRbsFlag[p], 0);
				BOTH(byte_280c[p], (unsigned char)(p == 0 ? 0
								   : 1));
				for (c = 0; c < V90ADID_CODES; c++)
					BOTH(codeHistogram[p][c], 0);
			}

			for (c = 0; c < V90ADID_CODES; c++) {
				int q;

				BOTH(linMapp[0][c], (short)-32768);
				for (q = 1; q < NPHASE; q++)
					BOTH(linMapp[q][c], 32767);
			}

			if (w == 1) {
				/*
				 * The primer at code 0: distances of 1, 499
				 * and 501, so the nearest ends at 1 and the
				 * second-nearest at 499.  Both windows below
				 * stay clear of code 3.
				 */
				BOTH(linMapp[0][0], 0);
				BOTH(linMapp[0][1], 500);
				BOTH(linMapp[0][2], -500);
				BOTH(linMapp[1][0], 1);
			}

			ours_o.porcessSecondStudy();
			ref_porcessSecondStudy(&theirs_o);

			diff_eq_obj("porcessSecondStudy: the sentinel witness",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, w);
			diff_eq_int("no store outside the object (witness %ld)",
				    guard_equal(), 1, w);

			if (w == 0) {
				diff_eq_int("an unordered second-nearest takes "
					    "the first distance",
					    ours_o.linMapp[1][0], -32768, 0);
				repaired = 1;
			} else {
				diff_eq_int("the primer took its own nearest",
					    ours_o.linMapp[1][0], 0, 0);
				diff_eq_int("the second-nearest carries across "
					    "the codes",
					    ours_o.linMapp[1][3], 32767, 0);
				kept = 1;
			}
		}
	}

	diff_eq_int("porcessSecondStudy changed the object", moved, 1, 0);
	diff_eq_int("the mapping is not the same on every trial", distinct, 1,
		    0);
	diff_eq_int("the scan stopped at a clear phase", scanstop, 1, 0);
	diff_eq_int("the scan ran off the end", scanfull, 1, 0);
	diff_eq_int("an unsuspected phase of zero was exercised", atzero, 1, 0);
	diff_eq_int("an entry above the reference was exercised", above, 1, 0);
	diff_eq_int("an entry below the reference was exercised", below, 1, 0);
	diff_eq_int("an entry was repaired", repaired, 1, 0);
	diff_eq_int("an entry was kept", kept, 1, 0);
	diff_eq_int("the report was exercised", level2, 1, 0);
	diff_eq_int("the gate was exercised shut", level0, 1, 0);

	return diff_end();
}

/*
 * setQcLinearMapping.
 *
 * THREE ARMS PER PHASE and the sweep drives all three: a phase flagged at
 * +0x2800 is left entirely alone, one with a study verdict at +0x280c has its
 * mapping rebuilt from its accumulators, and one WITHOUT a verdict is given
 * the previous session's mapping -- the same 128 entries for every such phase,
 * because `prevLinMapp` has no phase dimension.
 *
 * The rebuild is `updateLinMappMeanAndVar`, which the object inlines and this
 * calls; its own arithmetic is pinned by `run_means` and by finding F1366's
 * witness there, so what this suite has to establish is which cells it is
 * applied to.  The zero-count arm is swept here too, because a cell with no
 * samples keeps whatever it had and that is only visible against a seeded
 * mapping.
 */
static int
run_qcmapping(void)
{
	int trial, moved = 0, distinct = 0;
	int meaned = 0, copied = 0, skipped = 0;
	int zerocount = 0, nonzerocount = 0, level0 = 0, level2 = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::setQcLinearMapping");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		int p, c;

		seed(trial, trial % 4);
		sane_histogram(trial);

		for (c = 0; c < V90ADID_CODES; c++)
			BOTH(prevLinMapp[c],
			     (short)((c * 517 + trial * 29) & 0x3fff));

		for (p = 0; p < NPHASE; p++) {
			BOTH(byte_280c[p],
			     (unsigned char)(((trial >> p) & 1) ? 1 : 0));

			for (c = 0; c < V90ADID_CODES; c++) {
				unsigned n = ((trial + c + p) & 3) == 0
					     ? 0u : (unsigned)(1 + ((c + p) & 7));

				BOTH(magnitudeCount[p][c], n);
				BOTH(magnitudeSum[p][c],
				     (float)n * (60.5f + 11.0f * (float)p));
				BOTH(magnitudeSqSum[p][c],
				     (float)((int)n * (3000 + 7 * c)));

				if (n == 0)
					zerocount = 1;
				else
					nonzerocount = 1;
			}
		}

		if ((trial & 1) == 0) {
			for (p = 0; p < NPHASE; p++)
				BOTH(altRbsFlag[p],
				     (short)(p == (trial / 2) % NPHASE ? 9 : 0));
			study_debug_on();
			level2 = 1;
		} else {
			for (p = 0; p < NPHASE; p++)
				BOTH(altRbsFlag[p],
				     (short)(((trial >> p) & 3) == 3 ? 1 : 0));
			level0 = 1;
		}

		for (p = 0; p < NPHASE; p++) {
			if (ours_o.altRbsFlag[p] != 0)
				skipped = 1;
			else if (ours_o.byte_280c[p] != 0)
				meaned = 1;
			else
				copied = 1;
		}

		dsplib_debug_capture_reset();
		memcpy(before, ours.raw, SLOT);

		ours_o.setQcLinearMapping();
		ref_setQcLinearMapping(&theirs_o);

		diff_eq_obj("after setQcLinearMapping",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store outside the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("the mapping report matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);
		study_debug_off();

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[2][11];
		else if (ours_o.linMapp[2][11] != first)
			distinct = 1;
	}

	/*
	 * A DIRECTED BLOCK FOR WHICH ROW GOES WHERE.
	 *
	 * Phase 0 is flagged and must keep its seeded mapping exactly; phase 1
	 * has a verdict and one nonzero count, so exactly one of its entries
	 * moves and the rest keep theirs; phase 2 has no verdict and must come
	 * out as `prevLinMapp` from end to end -- all 128 of them, which is
	 * what makes the copy's bound a tested claim.
	 *
	 * Count 41 against a sum of 143.5 is finding F1366's witness: the
	 * division is exactly 3.5 and rounds to 4, the reciprocal is a hair
	 * under and truncates to 3.  It is here as well as in `run_means`
	 * because this is where the call site is.
	 */
	{
		int p, c, ok = 1;

		seed(960, 0);
		for (p = 0; p < NPHASE; p++) {
			BOTH(altRbsFlag[p], (short)(p == 0 ? 1 : 0));
			BOTH(byte_280c[p], (unsigned char)(p == 1 ? 1 : 0));
			for (c = 0; c < V90ADID_CODES; c++) {
				BOTH(codeHistogram[p][c], 0);
				BOTH(magnitudeCount[p][c], 0u);
				BOTH(linMapp[p][c], (short)(1000 + c + p * 7));
			}
		}
		for (c = 0; c < V90ADID_CODES; c++)
			BOTH(prevLinMapp[c], (short)(c * 3 - 100));

		BOTH(magnitudeCount[1][40], 41u);
		BOTH(magnitudeSum[1][40], 143.5f);
		BOTH(magnitudeSqSum[1][40], 600.0f);

		ours_o.setQcLinearMapping();
		ref_setQcLinearMapping(&theirs_o);

		diff_eq_obj("setQcLinearMapping: the three arms",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("no store outside the object (three arms)",
			    guard_equal(), 1, 0);
		diff_eq_int("a flagged phase keeps its mapping",
			    ours_o.linMapp[0][40], 1040, 0);
		diff_eq_int("the reciprocal witness truncates to three",
			    ours_o.linMapp[1][40], 3, 0);
		diff_eq_int("a cell with no samples keeps its mapping",
			    ours_o.linMapp[1][41], 1048, 0);

		for (c = 0; c < V90ADID_CODES; c++)
			if (ours_o.linMapp[2][c] != (short)(c * 3 - 100))
				ok = 0;
		diff_eq_int("a phase with no verdict gets all 128 of the "
			    "previous session's entries", ok, 1, 0);
	}

	diff_eq_int("setQcLinearMapping changed the object", moved, 1, 0);
	diff_eq_int("the mapping is not the same on every trial", distinct, 1,
		    0);
	diff_eq_int("a flagged phase was skipped", skipped, 1, 0);
	diff_eq_int("a phase with a verdict was rebuilt", meaned, 1, 0);
	diff_eq_int("a phase without one took the previous session", copied, 1,
		    0);
	diff_eq_int("a zero count was exercised", zerocount, 1, 0);
	diff_eq_int("a nonzero count was exercised", nonzerocount, 1, 0);
	diff_eq_int("the report was exercised", level2, 1, 0);
	diff_eq_int("the gate was exercised shut", level0, 1, 0);

	return diff_end();
}

/*
 * ==========================================================================
 * THE PAD-GAIN BATCH: `determineMaxUcode` and `findPadGain`.
 *
 * WHAT HAS TO BE BOUNDED BEFORE EITHER CAN BE CALLED AT ALL, and none of it
 * is a weakening -- each bound is a place the object walks out of its own
 * 43,440 bytes, which is two different pieces of the test's memory and
 * therefore a measurement of nothing.  docs/deviations.md D288 and D290 are
 * where the four are written down.
 *
 *   `linearMappingVar` RUNS OUT AT ENTRY 793.  It is 768 entries long but the
 *   object indexes it as `phase * 128 + code` with a code that reaches 255,
 *   and the object is 0xa9b0 bytes, so the last entry still inside is
 *   (0xa9b0 - 0x9d48) / 4 - 1 = 793.  Phase 5 plus a code of 154 is the first
 *   one out, so an argument above 153 is only ever offered to phases 0..4 --
 *   which is what `safe_usp` below does, and why the phase is chosen from the
 *   argument rather than swept against it.
 *
 *   `determineMaxUcode`'s REPORT LOOP COUNTS AN `unsigned char` UP TO THE
 *   ARGUMENT, so an argument of 255 or more never ends it -- the counter
 *   wraps from 255 to 0 and 0 is still under the bound.  Every argument here
 *   is 254 or less.
 *
 *   `findPadGain`'s SCAN BOUND IS `(int)(originalMaxUcode - 3) - 5` COMPARED AGAINST
 *   A ZERO-EXTENDED BYTE, so a `originalMaxUcode` of 3..7 makes the bound negative
 *   and the loop never ends.  Every trial forces the field into [8, 0x9c],
 *   whose top keeps the window's own index inside entry 793 at phase 5.
 *
 *   `findPadGain` READS ITS BEST INDEX UNINITIALISED if none of the five
 *   window entries beats the 1e8 sentinel (D290, D281's situation).  Every
 *   trial plants the window with `plant_window`, so exactly one entry is
 *   small and the take always happens -- two static objects have two
 *   different stack frames and a trial that read the slot would be comparing
 *   the linker's layout.
 *
 * `determineMaxUcode`'s OTHER TWO UNBOUNDED INDICES NEED NO GUARD and are
 * exercised for real.  The five-entry scan window reads below the start of
 * the reference phase's row and the backwards walk in the last loop wraps a
 * byte through 256 values -- but the deepest either reaches is +0x107f, which
 * is inside the object, so both are left alone.  The walk cannot spin
 * forever either: the fill immediately before it sets
 * `usableMask[phase][ucode]` to 1 for every phase, and that byte is one of the
 * 256 the walk visits.
 * ==========================================================================
 */

/*
 * THE SWEEP'S VARIANCES ARE FINITE, AND THIS IS THE ONE PLACE THAT IS FORCED.
 *
 * `determineMaxUcode`'s scan skips an entry with `if (v == 0.0f) continue;`,
 * which the object compiles to ONE ordered `fcom` and a `je` with no parity
 * test -- so an UNORDERED entry sets C3 and is skipped along with a zero one.
 * GCC 13 emits the parity test whatever it is told (finding F2304), keeps the
 * entry, and the count comes out different.
 *
 * `linearMappingVar` holds VARIANCES, and the object's own writer --
 * `updateLinMappMeanAndVar`, behind a guard on a zero sample count -- cannot
 * put a NaN there.  The sweep seeds the whole object from an LFSR, so one
 * trial in sixty-four lands a non-finite word in the window the scan reads,
 * and that trial alone made this group RED on the modern build and the whole
 * BINARY with it: `t_v90adid` carries the `v90adid` and `v90dil` mutation
 * suites, 497 mutations, and `tools/mutate.py` refuses a red baseline
 * (findings F2157 and F3002).
 *
 * So the accidental case is removed and the DELIBERATE one is kept, in its own
 * binary: `t_v90adidnan` drives a window of planted NaNs and is declared in
 * `tools/gccdiverge.json`.  Nothing else about the sweep moves -- only words
 * whose exponent field is all ones are touched, and they are turned into the
 * largest finite exponent rather than into a constant, so the entry keeps its
 * sign and its significand and stays as varied as the seed made it.
 * Findings F6001 and F1436.
 */
static long mu_finite_words;		/* rewritten */
static long mu_finite_seen;		/* examined  */

static void
mu_finite_variances(void)
{
	float *a = &ours_o.linearMappingVar[0][0];
	float *b = &theirs_o.linearMappingVar[0][0];
	int i;

	/*
	 * Through the BITS and not through `v != v`: the period build sets
	 * `-mno-ieee-fp`, which folds a self-comparison to zero and deleted a
	 * NaN detector in the harness once already (finding F2303).
	 */
	for (i = 0; i <= ADID_VAR_LAST; i++) {
		unsigned int u;

		mu_finite_seen++;
		memcpy(&u, &a[i], sizeof u);
		if ((u & 0x7f800000u) != 0x7f800000u)
			continue;
		u &= ~0x00800000u;
		memcpy(&a[i], &u, sizeof u);
		memcpy(&b[i], &u, sizeof u);
		mu_finite_words++;
	}
}

/*
 * determineMaxUcode.
 *
 * THE METHOD IS FIVE DECISIONS DEEP AND A SEEDED OBJECT REACHES ONE SIDE OF
 * MOST OF THEM, so the sweep forces the six fields the answers turn on --
 * `unSuspectedPhase`, `minMaxUcode`, `float_a980`, `ucode`, the six flags at
 * +0x2800, and the argument -- and then four directed grids drive the arms a
 * sweep cannot reach.
 *
 * THE THRESHOLD'S TWO CLAMPS NEED A CONSTRUCTED INPUT.  It is the mean of
 * twenty variances times `float_a980`, and a seeded `linearMappingVar` puts that
 * mean somewhere astronomical almost every time -- so the grid sets those
 * twenty entries directly: all zero clamps it up to 500, all 1e9 clamps it
 * down to 100000, and a middling set leaves it alone.
 *
 * THE ZERO SKIP IN THE SCAN.  An entry joins the count when it is smaller than
 * the threshold AND is not zero, so a grid plants exactly three zeros in a
 * five-entry window with two small entries beside them: the count is 2 and the
 * window does NOT qualify, where a reading that counted them would make it 5,
 * qualify, and move the answer.  Finding F1436.
 *
 * THE OTHER HALF OF THAT SKIP IS `t_v90adidnan`'s.  The object's zero test is
 * one `fcomp`/`je` with no parity test, so an UNORDERED entry is skipped too --
 * which GCC 13 cannot reproduce and which therefore cannot be observed in a
 * binary that has to exit zero on the modern build.  The NaN window row moved
 * out with it; `mu_finite_variances` above is why the SWEEP no longer trips
 * over the same arm by accident.
 */
static int
run_maxucode(void)
{
	static const short mcv[] = {
		0x5a, 0x40, 0x2a, 0x60, 0x50, 0x33, 0x4c, 0x55
	};
	static const short a97av[] = {
		80, 88, 0x28, 0x3c, 300, 0x30, 0x35, 0x48
	};
	static const float a980v[] = {
		1.5f, 1.75f, 0.0f, 100.0f, -2.0f, 1.0f, 1.0e6f, 1.0e-3f
	};
	static const unsigned char ucv[] = {
		0, 1, 0x2a, 0x40, 0x7f, 0x80, 0xd5, 0xff
	};
	int trial;
	int moved = 0, distinct = 0;
	int forced = 0, unforced = 0;
	int mask0 = 0, mask1 = 0;
	int walked = 0, direct = 0;
	int level2 = 0, level0 = 0;
	unsigned char first = 0;

	diff_begin("V90AutoDigitalImpDetector::determineMaxUcode");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short mc = mcv[IDX(trial, 1)];
		short a97a = a97av[IDX(trial, 3)];
		short usp = safe_usp(mc, trial % NPHASE);
		int p;

		seed(trial, trial % 4);
		mu_finite_variances();
		BOTH(unSuspectedPhase, usp);
		BOTH(minMaxUcode, a97a);
		BOTH(float_a980, a980v[IDX(trial, 5)]);
		BOTH(ucode, ucv[IDX(trial, 7)]);

		/*
		 * A pattern in 1..62 is never all clear and never all set, so
		 * both arms of every per-phase test are taken inside every
		 * single call rather than only across the sweep.
		 */
		adid_set_2800(1 + trial % 62);

		/* Half the sweep at level 2 and half at level 0. */
		if ((trial & 1) != 0) {
			study_debug_on();
			level2 = 1;
		} else {
			study_debug_off();
			level0 = 1;
		}
		dsplib_debug_capture_reset();
		memcpy(params_copy, params_block, PARAMS_BYTES);
		memcpy(before, ours.raw, SLOT);

		ours_o.determineMaxUcode(mc);
		ref_determineMaxUcode(&theirs_o, mc);

		diff_eq_obj("after determineMaxUcode",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("determineMaxUcode wrote nothing through params "
			    "(trial %ld)",
			    memcmp(params_copy, params_block, PARAMS_BYTES), 0,
			    trial);
		diff_eq_int("the maxUcode report matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.originalMaxUcode;
		else if (ours_o.originalMaxUcode != first)
			distinct = 1;

		if (a97a == 300)
			forced = 1;
		else if (ours_o.originalMaxUcode > (unsigned char)a97a)
			unforced = 1;

		for (p = 0; p < NPHASE; p++) {
			int c;

			if (ours_o.altRbsFlag[p] != 0)
				continue;
			if (ours_o.maxUcode[p] == ours_o.originalMaxUcode)
				direct = 1;
			else
				walked = 1;
			for (c = 0; c < V90ADID_CODES; c++) {
				if (ours_o.usableMask[p][c] == 0)
					mask0 = 1;
				else
					mask1 = 1;
			}
		}
	}

	/*
	 * THE THRESHOLD GRID.  Twenty entries decide it, and each row here
	 * makes the answer land on one side of one of the two clamps: zeros
	 * give a threshold of 0 and the 500 floor, 1e9 gives 1e9 and the
	 * 100000 ceiling, and 2000 with a gain of 1 gives 2000 and neither.
	 * The mask the last loop reads is built from twice that threshold, so
	 * the same grid moves what `maxUcode` comes out as.
	 */
	{
		/*
		 * The last three rows are just inside a boundary rather than
		 * far from it: twenty entries of `f` and a gain of one give a
		 * threshold of `f` almost exactly, so 500.5 and 100000.5 land
		 * between each clamp's test and the value one above it, and
		 * 6666.6 is the only row with a fractional part for the two
		 * reports to print.  A table of round numbers cannot see a
		 * constant moved by one -- finding F1423's lesson, met again.
		 */
		static const float fill[] = {
			0.0f, 1.0e9f, 2000.0f, 40000.0f, 6666.6f, 500.5f,
			100000.5f
		};
		int g;

		study_debug_on();
		for (g = 0; g < 7; g++) {
			int i;

			seed(700 + g, 0);
			mu_finite_variances();
			BOTH(unSuspectedPhase, (short)(g % NPHASE));
			BOTH(minMaxUcode, 0x30);
			BOTH(float_a980, 1.0f);
			BOTH(ucode, (unsigned char)(0x41 + g));
			adid_set_2800(0x15);

			for (i = 0; i < V90ADID_CODES; i++) {
				ours_o.linearMappingVar[g % NPHASE][i] =
				    theirs_o.linearMappingVar[g % NPHASE][i] = fill[g];
			}

			dsplib_debug_capture_reset();
			ours_o.determineMaxUcode(0x5a);
			ref_determineMaxUcode(&theirs_o, 0x5a);
			diff_eq_obj("maxucode: the threshold grid",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, g);
			diff_eq_int("the threshold grid's report matched (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, g);
		}
	}

	/*
	 * THE WINDOW GRID.  Five entries decide whether a window qualifies and
	 * the count has to exceed two, so a window of exactly three small
	 * entries qualifies and one of exactly two does not -- and the two
	 * skips are separated by planting NaNs and zeros where the small
	 * entries would otherwise be.  Row `w` of the table is the five
	 * variances at codes 0x5a down to 0x56, top first.
	 */
	{
		static const unsigned int win[5][5] = {
			/* three small: qualifies, and the first is at the top */
			{ 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x7f7fffffu,
			  0x7f7fffffu },
			/* three small, the first one entry down */
			{ 0x7f7fffffu, 0x3f800000u, 0x3f800000u, 0x3f800000u,
			  0x7f7fffffu },
			/* three small, the first at the BOTTOM of the window */
			{ 0x7f7fffffu, 0x7f7fffffu, 0x3f800000u, 0x3f800000u,
			  0x3f800000u },
			/* two small and three zeros: must NOT qualify */
			{ 0x00000000u, 0x3f800000u, 0x00000000u, 0x3f800000u,
			  0x00000000u },
			/* nothing small at all */
			{ 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu,
			  0x7f7fffffu }
			/*
			 * THE SIXTH ROW WAS `two small and three NaN`, and it
			 * is `t_v90adidnan`'s now.  It is the same claim as the
			 * zero row one instruction further on -- the object's
			 * `fcomp`/`je` skips an unordered entry exactly as it
			 * skips a zero -- and it is the half GCC 13 cannot
			 * reproduce.  Findings F6001, F2304 and F1436.
			 */
		};
		int w;
		unsigned char seen[5];

		study_debug_on();
		for (w = 0; w < 5; w++) {
			int i;

			seed(760 + w, 0);
			mu_finite_variances();
			BOTH(unSuspectedPhase, 2);
			BOTH(minMaxUcode, 0x30);
			BOTH(float_a980, 1.0f);
			BOTH(ucode, 0x41);
			adid_set_2800(0x15);

			/*
			 * Everything outside the window is huge, so only the
			 * planted window can ever qualify and the answer is a
			 * function of the five entries alone.  0x4f000000 is
			 * 2^31, which is over the threshold the twenty entries
			 * at 40..59 produce and is not a NaN.
			 */
			for (i = 0; i < V90ADID_CODES; i++)
				ours_o.linearMappingVar[2][i] =
				    theirs_o.linearMappingVar[2][i] = 1.0e9f;
			for (i = 0; i < 5; i++) {
				float v;

				memcpy(&v, &win[w][i], sizeof v);
				ours_o.linearMappingVar[2][0x5a - i] =
				    theirs_o.linearMappingVar[2][0x5a - i] = v;
			}

			dsplib_debug_capture_reset();
			ours_o.determineMaxUcode(0x5a);
			ref_determineMaxUcode(&theirs_o, 0x5a);
			diff_eq_obj("maxucode: the window grid",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, w);
			diff_eq_int("the window grid's report matched (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, w);
			seen[w] = ours_o.originalMaxUcode;
		}

		/*
		 * The three qualifying rows put the highest small entry at
		 * three different places, so the answer has to move with it;
		 * the two that do not qualify both fall through to the floor.
		 */
		diff_eq_int("a window qualifying at its top gives that code",
			    seen[0], 0x5a, 0);
		diff_eq_int("a window qualifying one down gives that code",
			    seen[1], 0x59, 0);
		diff_eq_int("a window qualifying at its foot gives that code",
			    seen[2], 0x58, 0);
		diff_eq_int("three zeros do not make a window qualify",
			    seen[3] != 0x5a && seen[3] != 0x59, 1, 0);
		diff_eq_int("no small entry at all falls through to the floor",
			    seen[4], 0x30, 0);
	}

	/*
	 * THREE CORNERS THAT ONLY A CONSTRUCTED INPUT REACHES, and each of the
	 * three is invisible in the OBJECT and visible only in the transcript
	 * -- which is the whole argument for comparing the transcript at all.
	 *
	 * ROW 0, THE WINDOW AT THE VERY BOTTOM OF THE SCAN.  Three small
	 * entries at codes 0x2c..0x2e and a floor at 0x30: the window ending at
	 * 0x31 holds two of them and does not qualify, and the window ending at
	 * 0x30 holds all three and does -- but the scan's test is `ci > minU`
	 * and never evaluates it.  A test of `ci >= minU` would qualify there,
	 * answer 0x2e, and then have that answer forced back up to 0x30 by the
	 * floor -- so `originalMaxUcode` is 0x30 either way and the only difference is
	 * the "original maxUcode ... forced minimum maxUcode" line.
	 *
	 * ROW 1, A NEGATIVE FLOOR.  `minMaxUcode` of -1 makes the byte 0xff, so
	 * the scan never runs and the answer is 0xff -- and the floor test is a
	 * SIGNED compare against the whole `short`, so 255 < -1 is false and
	 * nothing is forced.  Read unsigned it would be 255 < 65535, which
	 * forces, prints, and stores (unsigned char)(-1) -- the same 0xff.
	 * That row is also the deepest the backwards walk in the last loop
	 * ever gets here, and it terminates by construction and not by luck:
	 * all three unflagged phases start the walk at 0xff, `ucode` is 0x41,
	 * and the fill immediately before sets `usableMask[phase][0x41]` to 1
	 * for every phase -- so each walk stops after at most 190 steps
	 * without wrapping, whatever the seed put in the rest of the row.
	 *
	 * ROW 2, A VARIANCE EXACTLY ON THE MASK'S THRESHOLD.  Twenty entries of
	 * 1000 give a threshold of 1000 exactly and a mask limit of 2000
	 * exactly, and sixteen codes below the argument are set to 2000 -- so
	 * `>=` marks them unusable where `>` would mark them usable, and
	 * `maxUcode` moves with them.
	 */
	{
		int r;

		study_debug_on();
		for (r = 0; r < 3; r++) {
			int i;

			seed(830 + r, 0);
			mu_finite_variances();
			BOTH(unSuspectedPhase, 1);
			BOTH(float_a980, 1.0f);
			BOTH(ucode, 0x41);
			adid_set_2800(0x15);

			for (i = 0; i < V90ADID_CODES; i++)
				ours_o.linearMappingVar[1][i] =
				    theirs_o.linearMappingVar[1][i] =
				    (r == 2) ? 1000.0f : 1.0e9f;

			if (r == 0) {
				BOTH(minMaxUcode, 0x30);
				for (i = 0x2c; i <= 0x2e; i++)
					ours_o.linearMappingVar[1][i] =
					    theirs_o.linearMappingVar[1][i] = 1.0f;
			} else if (r == 1) {
				BOTH(minMaxUcode, -1);
			} else {
				BOTH(minMaxUcode, 0x30);
				for (i = 0x10; i <= 0x1f; i++)
					ours_o.linearMappingVar[1][i] =
					    theirs_o.linearMappingVar[1][i] = 2000.0f;
			}

			dsplib_debug_capture_reset();
			ours_o.determineMaxUcode(0x5a);
			ref_determineMaxUcode(&theirs_o, 0x5a);
			diff_eq_obj("maxucode: the corner grid",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, r);
			diff_eq_int("the corner grid's report matched (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, r);
		}
	}

	/*
	 * THE ARGUMENT'S OWN ARMS.  A negative argument skips the report loop
	 * and makes every code fail `code <= arg`, so the mask comes out all
	 * zero except the reference code the fill forces back to 1 -- which is
	 * the only thing that stops the backwards walk in the last loop.  An
	 * argument at or below `minMaxUcode` skips the scan entirely.  Both are
	 * only offered to phases 0..4, because a byte-wide argument of 0xff at
	 * phase 5 would index past the object.
	 */
	{
		static const short arg[] = { -1, -32768, 0x20, 0, 0xfe, 0x99 };
		int a;

		study_debug_off();
		for (a = 0; a < 6; a++) {
			seed(800 + a, a % 4);
			mu_finite_variances();
			BOTH(unSuspectedPhase, (short)(a % 5));
			BOTH(minMaxUcode, 0x30);
			BOTH(float_a980, 1.0f);
			BOTH(ucode, (unsigned char)(0x20 + a));
			adid_set_2800(1 + a * 7 % 62);

			ours_o.determineMaxUcode(arg[a]);
			ref_determineMaxUcode(&theirs_o, arg[a]);
			diff_eq_obj("maxucode: the argument grid",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, a);
			diff_eq_int("no store past the object (argument %ld)",
				    guard_equal(), 1, a);
		}
	}

	study_debug_off();

	diff_eq_int("determineMaxUcode changed the object", moved, 1, 0);
	diff_eq_int("the maxUcode is not the same on every trial", distinct, 1,
		    0);
	diff_eq_int("the floor was forced", forced, 1, 0);
	diff_eq_int("the floor was not forced", unforced, 1, 0);
	diff_eq_int("a code was marked unusable", mask0, 1, 0);
	diff_eq_int("a code was marked usable", mask1, 1, 0);
	diff_eq_int("a phase took the scan's answer directly", direct, 1, 0);
	diff_eq_int("a phase had to walk down for a usable code", walked, 1, 0);
	diff_eq_int("the report was exercised", level2, 1, 0);
	diff_eq_int("the gate was exercised shut", level0, 1, 0);

	/*
	 * THE SANITISER REPORTS ITS DENOMINATOR, because a fixture guard that
	 * silently did nothing and one that silently rewrote half the array
	 * both leave this group green -- 2400's argument, and 3100's.  Both
	 * numbers are PINNED rather than bounded: they are a function of the
	 * seed alone and of nothing in `src/`, so either one moving means the
	 * sweep's inputs moved and every verdict this group carries describes
	 * a different grid.  459 of 67,490 is 0.68% of the words examined,
	 * and what it removed was ONE trial's worth of failure: trial 27,
	 * whose non-finite word is at flat index 424 --
	 * `linearMappingVar[3][40]`, the first of the twenty entries the threshold
	 * averages, which is why the divergence showed in the REPORT and not
	 * in the object.  Finding F6001.
	 */
	diff_eq_int("the variance sanitiser examined %ld words",
		    mu_finite_seen, 67490L, 0);
	diff_eq_int("and rewrote %ld of them", mu_finite_words, 459L, 0);

	return diff_end();
}

/*
 * findPadGain.
 *
 * FIVE THINGS ARE FORCED AND ONE IS PLANTED.  `originalMaxUcode` sets both scans'
 * extent and has to stay in [8, 0x9c] for the method to terminate at all;
 * `unSuspectedPhase` picks the row everything is read from; `padGainSearchScale`
 * decides where the candidate range starts; `linMapp[phase][base]` is the
 * numerator of every candidate gain and therefore the only lever on which of
 * the three error buckets a candidate lands in; and the five-entry variance
 * window is planted outright, because `projectionBaseUcode` is otherwise
 * invisible behind the [0x50, 0x5f] clamp.
 *
 * THAT LAST POINT IS WHY THE GRID EXISTS.  A seeded window puts the smallest
 * variance somewhere arbitrary, the clamp swallows the difference, and a
 * mutation on the window's width, on the `-3`, or on the sense of the
 * comparison survives a perfect sweep -- finding F1366's shape.  The grid
 * plants the minimum at each of the five offsets in turn with the base code
 * inside the clamp window, and asserts the gain that comes out is not the
 * same for all five.
 *
 * THE GAIN IS ROUTINELY A NUMBER NOTHING SENSIBLE COMES OF.  `ref` is swept
 * through zero and both signs, so `1.0f / gain` is an infinity on some passes
 * and the projected level saturates to 0x8000 through the object's sixteen-bit
 * `fistps`; that is the object's arithmetic and it is exercised rather than
 * avoided.
 */
static int
run_padgain(void)
{
	static const unsigned char a954v[] = {
		0x5a, 0x62, 0x70, 0x08, 0x3f, 0x53, 0x9c, 0x40
	};
	static const float a97cv[] = {
		0.25f, 0.35f, 1.0f, 2.0f, 8.0f, 0.0f, -1.0f, 0.03f
	};
	static const short refv[] = {
		1000, 8031, 32000, 0, -8000, 100, 4000, 20000
	};
	int trial;
	int moved = 0, distinct = 0;
	int mulaw = 0, alaw = 0;
	int gainhigh = 0, gainmid = 0, gainlow = 0, gainone = 0;
	int clamplo = 0, clamphi = 0, noclamp = 0;
	int shortscan = 0, longscan = 0;
	int level2 = 0, level0 = 0;
	float first = 0.0f;

	diff_begin("V90AutoDigitalImpDetector::findPadGain");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		unsigned char a954 = a954v[IDX(trial, 1)];
		short usp = (short)(trial % NPHASE);
		int minoff = trial % 5;
		unsigned char base = planted_base(a954, minoff);
		float g;

		seed(trial, trial % 4);
		BOTH(unSuspectedPhase, usp);
		BOTH(originalMaxUcode, a954);
		BOTH(padGainSearchScale, a97cv[IDX(trial, 3)]);
		plant_window(usp, a954, minoff, 1.0f, 1.0e9f);
		ours_o.linMapp[usp][base] = theirs_o.linMapp[usp][base] =
		    refv[IDX(trial, 5)];

		if ((trial & 1) != 0) {
			study_debug_on();
			level2 = 1;
		} else {
			study_debug_off();
			level0 = 1;
		}
		dsplib_debug_capture_reset();
		memcpy(params_copy, params_block, PARAMS_BYTES);
		memcpy(before, ours.raw, SLOT);

		ours_o.findPadGain();
		ref_findPadGain(&theirs_o);

		diff_eq_obj("after findPadGain", V90AutoDigitalImpDetector,
			    &ours_o, &theirs_o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("findPadGain wrote nothing through params "
			    "(trial %ld)",
			    memcmp(params_copy, params_block, PARAMS_BYTES), 0,
			    trial);
		diff_eq_int("the pad-gain report matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.padGain;
		else if (fbits(ours_o.padGain) != fbits(first))
			distinct = 1;

		if (ours_o.detectedPcmType == 0)
			mulaw = 1;
		else if (ours_o.detectedPcmType == 1)
			alaw = 1;

		g = ours_o.padGain;
		if (fbits(g) == fbits(1.0f))
			gainone = 1;
		else if (g >= 2.7f)
			gainhigh = 1;
		else if (g >= 1.2f)
			gainmid = 1;
		else
			gainlow = 1;

		if (base == 0x50)
			clamplo = 1;
		else if (base == 0x5f)
			clamphi = 1;
		else
			noclamp = 1;

		/*
		 * The candidate scan prints one line per pass, so the line
		 * count is what says whether it ran at all: a call whose
		 * starting code is already above the base code prints only its
		 * fixed lines, and every other call prints tens more.
		 */
		if ((trial & 1) != 0) {
			if (dsplib_debug_capture_lines(0) < 20)
				shortscan = 1;
			else
				longscan = 1;
		}
	}

	/*
	 * THE WINDOW GRID.  Five offsets, the same everything else, and the
	 * base code inside the clamp window at all five -- so the only thing
	 * that moves is which entry of the window the scan picked, and the pad
	 * gain has to move with it.  Without this the width of the window, the
	 * `- 3`, and the sense of the comparison are all unobservable.
	 */
	{
		float got[5];
		int w;

		study_debug_on();
		for (w = 0; w < 5; w++) {
			int i;
			unsigned char base = planted_base(0x62, w);

			seed(860 + w, 0);
			BOTH(unSuspectedPhase, 1);
			BOTH(originalMaxUcode, 0x62);
			BOTH(padGainSearchScale, 0.25f);
			plant_window(1, 0x62, w, 1.0f, 1.0e9f);

			/*
			 * A mapping that rises with the code, so the candidate
			 * gain falls as the scan proceeds and the three error
			 * buckets are all reached inside one call.
			 */
			for (i = 0; i < V90ADID_CODES; i++)
				ours_o.linMapp[1][i] = theirs_o.linMapp[1][i] =
				    (short)(i * 137 + 200);

			dsplib_debug_capture_reset();
			ours_o.findPadGain();
			ref_findPadGain(&theirs_o);
			diff_eq_obj("padgain: the window grid",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, w);
			diff_eq_int("the window grid's report matched (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, w);
			got[w] = ours_o.padGain;
			(void)base;
		}

		diff_eq_int("the window offset moves the pad gain",
			    fbits(got[0]) != fbits(got[4])
			    || fbits(got[1]) != fbits(got[3]), 1, 0);
	}

	/*
	 * THE TIE AND THE NaN, which are the two readings of the scan's
	 * comparison that no ordinary window separates.
	 *
	 * The scan walks DOWNWARDS and keeps the entry that is strictly
	 * smaller than the best so far, so two equal minima leave the HIGHER
	 * code standing; a `<=` would leave the lower one.  And the object's
	 * `jb` is taken by an unordered compare, so a NaN in the window
	 * becomes the running best and then loses to nothing -- every entry
	 * after it wins -- which puts the answer at the BOTTOM of the window
	 * where a C `<` would have left it above.  Finding F1436.
	 */
	{
		static const unsigned int probe[2][5] = {
			{ 0x3f800000u, 0x4f000000u, 0x3f800000u, 0x4f000000u,
			  0x4f000000u },
			{ 0x4f000000u, 0x7fc00000u, 0x4f000000u, 0x4f000000u,
			  0x4f000000u }
		};
		int t;

		study_debug_on();
		for (t = 0; t < 2; t++) {
			int i;

			seed(880 + t, 0);
			BOTH(unSuspectedPhase, 0);
			BOTH(originalMaxUcode, 0x62);
			BOTH(padGainSearchScale, 0.25f);

			for (i = 0; i < V90ADID_CODES; i++)
				ours_o.linMapp[0][i] = theirs_o.linMapp[0][i] =
				    (short)(i * 137 + 200);
			for (i = 0; i < 5; i++) {
				float v;

				memcpy(&v, &probe[t][i], sizeof v);
				ours_o.linearMappingVar[0][0x5f - i] =
				    theirs_o.linearMappingVar[0][0x5f - i] = v;
			}

			dsplib_debug_capture_reset();
			ours_o.findPadGain();
			ref_findPadGain(&theirs_o);
			diff_eq_obj("padgain: the tie and the NaN",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, t);
			diff_eq_int("the probe's report matched (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, t);
		}
	}

	/*
	 * A FINE GRID OVER THE CANDIDATE GAIN AND OVER THE RANGE'S FLOOR.
	 *
	 * Everything above is a table, and a table of round numbers never puts
	 * a value just inside a boundary: the buckets split at 2.7 and 1.2, the
	 * two best-of-three margins are 0.8 and 0.85, and the range's floor is
	 * an exact `minU <= 0x27` -- so a constant moved by a tenth, or a
	 * comparison made strict, has to be STRADDLED to be seen.  Measured:
	 * five such mutations survived the tables and are caught here.
	 *
	 * The gain is `linMapp[phase][base] / level(cur)` with `level` fixed
	 * per candidate, so stepping the mapping by 37 a call steps every
	 * candidate gain in that call by a few hundredths and walks it across
	 * each boundary many times.  `padGainSearchScale` is swept in thousandths over
	 * the range that puts the companded floor in the low forties, which is
	 * where `minU` can be exactly 0x27.
	 */
	{
		int k;

		study_debug_off();
		for (k = 0; k < 200; k++) {
			unsigned char a954 = (unsigned char)(0x53 + k % 16);
			short usp = (short)(k % NPHASE);
			int i;

			seed(900 + k, 0);
			BOTH(unSuspectedPhase, usp);
			BOTH(originalMaxUcode, a954);
			BOTH(padGainSearchScale, 0.001f + 0.002f * (float)(k % 80));
			plant_window(usp, a954, k % 5, 1.0f, 1.0e9f);
			for (i = 0; i < V90ADID_CODES; i++)
				ours_o.linMapp[usp][i] =
				    theirs_o.linMapp[usp][i] =
				    (short)((i * 61 + 40 + k * 37) % 20000);

			ours_o.findPadGain();
			ref_findPadGain(&theirs_o);
			diff_eq_obj("padgain: the gain grid",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, k);
		}
		diff_eq_int("no store past the object (gain grid)",
			    guard_equal(), 1, 0);
	}

	/*
	 * AND THE SAME GRID WITH THE PROJECTION LOOP EMPTY.  A `originalMaxUcode`
	 * under 0x40 skips the round trip entirely, so every candidate's error
	 * is exactly zero -- and then the best-of-three's `errMid * 0.8f >
	 * errHigh` is `0 > 0`, which is the one input that separates it from
	 * `>=`.  Both buckets have to be filled for that to be visible, which
	 * is what the gain sweep is for.
	 */
	{
		int k;

		study_debug_off();
		for (k = 0; k < 64; k++) {
			short usp = (short)(k % NPHASE);
			int i;

			seed(1100 + k, 0);
			BOTH(unSuspectedPhase, usp);
			BOTH(originalMaxUcode, 0x20);
			BOTH(padGainSearchScale, 0.25f);
			plant_window(usp, 0x20, k % 5, 1.0f, 1.0e9f);
			for (i = 0; i < V90ADID_CODES; i++)
				ours_o.linMapp[usp][i] =
				    theirs_o.linMapp[usp][i] =
				    (short)((i * 7 + 100 + k * 211) % 24000);

			ours_o.findPadGain();
			ref_findPadGain(&theirs_o);
			diff_eq_obj("padgain: the empty-projection grid",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, k);
		}
		diff_eq_int("no store past the object (empty-projection grid)",
			    guard_equal(), 1, 0);
	}

	/*
	 * THE RANGE'S FLOOR, WHICH IS AN EXACT EQUALITY AND NEEDED SOLVING FOR.
	 *
	 * `if (minU <= 0x27) minU = 0x28;` differs from `< 0x27` at ONE value
	 * of `minU`, and `minU` is a companded code -- so no sweep of a scaling
	 * factor lands on it by luck.  It was solved for instead: at a base
	 * code of 0x50 and mu-law, `~linear2ulaw(|(int)(620 * a97c)|)` is 0x27
	 * for `padGainSearchScale` anywhere in [0.14775, 0.15525], which a throwaway
	 * program built -m32 -mfpmath=387 against this tree's own `pcm.c`
	 * enumerated.  Eight values inside that band are swept here.
	 *
	 * AND THE DIFFERENCE HAS TO BE MADE TO REACH `padGain`.  With the
	 * projection loop empty every candidate's error is zero, so the first
	 * candidate wins its bucket and no later one displaces it -- which
	 * makes the bottom of the range the whole answer.  A reference level of
	 * 20000 against companded levels of 620..4092 keeps every candidate
	 * gain above 2.7, so the middle bucket is never filled, its error stays
	 * at 1e6, `errMid * 0.8f > errHigh` fires, and the pad gain is the
	 * high bucket's -- which is `20000 / 620` under one reading of the
	 * floor and `20000 / 652` under the other.
	 */
	{
		int j;

		study_debug_off();
		for (j = 0; j < 8; j++) {
			short usp = (short)(j % NPHASE);
			int i;

			seed(1200 + j, 0);
			BOTH(unSuspectedPhase, usp);
			BOTH(originalMaxUcode, 0x20);
			BOTH(padGainSearchScale, 0.148f + 0.001f * (float)j);
			plant_window(usp, 0x20, j % 5, 1.0f, 1.0e9f);
			for (i = 0; i < V90ADID_CODES; i++)
				ours_o.linMapp[usp][i] =
				    theirs_o.linMapp[usp][i] = 20000;

			ours_o.findPadGain();
			ref_findPadGain(&theirs_o);
			diff_eq_obj("padgain: the range's floor",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, j);
			diff_eq_int("the floor grid put the gain in the high "
				    "bucket (%ld)",
				    ours_o.padGain > 2.7f, 1, j);
		}
		diff_eq_int("no store past the object (floor grid)",
			    guard_equal(), 1, 0);
	}

	study_debug_off();

	diff_eq_int("findPadGain changed the object", moved, 1, 0);
	diff_eq_int("the pad gain is not the same on every trial", distinct, 1,
		    0);
	diff_eq_int("mu-law was identified", mulaw, 1, 0);
	diff_eq_int("A-law was identified", alaw, 1, 0);
	diff_eq_int("a gain above 2.7 was chosen", gainhigh, 1, 0);
	diff_eq_int("a gain between 1.2 and 2.7 was chosen", gainmid, 1, 0);
	diff_eq_int("a gain below 1.2 was chosen", gainlow, 1, 0);
	diff_eq_int("a gain of exactly one was chosen", gainone, 1, 0);
	diff_eq_int("the base code was clamped up", clamplo, 1, 0);
	diff_eq_int("the base code was clamped down", clamphi, 1, 0);
	diff_eq_int("the base code needed no clamp", noclamp, 1, 0);
	diff_eq_int("a call whose candidate range was empty", shortscan, 1, 0);
	diff_eq_int("a call whose candidate range was not", longscan, 1, 0);
	diff_eq_int("the report was exercised", level2, 1, 0);
	diff_eq_int("the gate was exercised shut", level0, 1, 0);

	return diff_end();
}

/*
 * ==========================================================================
 * studyUrefHandler -- the per-sample entry point, the largest member of the
 * class and the only one that is a state machine.
 *
 * THE STATE WORD IS A PLAIN FIELD, SO EVERY ARM IS ONE CALL AWAY.  Walking the
 * chain 0 -> 1 -> 2 -> 4 -> 3 -> 5 -> 6 by running the machine costs the sum of
 * six durations with the right accumulator state at each boundary; forcing
 * +0xa984 and +0xa988 costs nothing and reaches the same code.  Both are done
 * here: a sweep that forces each arm and each of its two outcomes, and a
 * directed chain that walks the whole machine end to end and compares after
 * every call.
 *
 * THE RETURN VALUE IS COMPARED, and it is a third of what this method does.
 * 2 means the study is over, 0 means "do not use this sample", 1 is everything
 * else, and the sweep asserts all three were seen.
 *
 * ONE OF PHASES 0..4 IS ALWAYS LEFT UNFLAGGED, on every path that reaches a
 * tail.  Four of the seven arms call `updateUref`, which calls
 * `unitePhasesInfoOfUref`, which reads an uninitialised local when no group is
 * ever formed -- D281, and the two sides have two different stack frames there.
 * All five of phases 0..4 flagged at +0x2800 on entry is exactly that
 * condition, so the sweep clears one and the chain clears one before each call.
 *
 * WHAT THAT COSTS IS THE SIGMA BLOCK'S EMPTY CASE.  `trn1Sigma` is
 * `sum / count` over the phases that are NOT flagged, so a count of zero needs
 * all six flagged -- which needs all of 0..4 flagged, which is D281.  The
 * divide-by-zero is therefore unreachable without an uncomparable frame and is
 * deliberately not exercised; the missing guard in state 0's variance loop
 * (D292) is the same arithmetic and IS exercised, through the zeros in
 * `counts[]`.
 *
 * `ucode` STOPS AT 0x7f.  The sigma block reads `linearMappingVar[phase][ucode]` as
 * `phase * 128 + ucode` and the last entry of that array inside the object is
 * ADID_VAR_LAST = 793, so phase 5 with a code over 153 would compare memory
 * past the rear guard.  Other sweeps in this file drive the unmasked code where
 * it stays inside the object; here it would not.
 * ==========================================================================
 */

/* The five durations five of the arms fire on.  +0xa9a0 is left seeded. */
static void
study_durations(int a98c, int a990, int a994, int a998, int a99c)
{
	BOTH(int_a98c, a98c);
	BOTH(int_a990, a990);
	BOTH(int_a994, a994);
	BOTH(int_a998, a998);
	BOTH(int_a99c, a99c);
}

/* Which of them the given state counts against; 0 for the two that do not. */
static int
study_duration(int st)
{
	switch (st) {
	case 0:
		return 11;
	case 1:
		return 13;
	case 2:
		return 7;
	case 3:
		return 17;
	case 4:
		return 5;
	case 5:
		return 17;
	default:
		return 0;
	}
}

static int
run_studyuref(void)
{
	static const unsigned char codes[] = {
		0, 1, 0x2a, 0x40, 0x5a, 0x6f, 0x7e, 0x7f
	};
	static const short levels[] = {
		0, 1, -1, 8031, 4096, -4096, 32767, (short)0x8000
	};
	static const unsigned counts[] = { 0, 1, 25, 41, 3, 0, 100, 7 };
	static const short dists[] = { -1, 0, 1, 25, 50, 300, 4000, 30000 };
	static const float factors[] = {
		1.5f, 5.0f, 0.5f, 2.0f, 0.0f, -1.0f, 100.0f, 1.0f
	};
	int trial, i;
	int moved = 0, distinct = 0;
	int seen[8], fired[6], held[6];
	int ret0 = 0, ret1 = 0, ret2 = 0;
	int altyes = 0, altno = 0;
	int flagged = 0, unflagged = 0, cleared = 0, kept = 0;
	int emptycell = 0, fullcell = 0, talked = 0;
	long gated = 0;
	int keydrift = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::studyUrefHandler");
	study_debug_on();

	for (i = 0; i < 8; i++)
		seen[i] = 0;
	for (i = 0; i < 6; i++)
		fired[i] = held[i] = 0;

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short was[NPHASE];
		int st = trial % 8;		/* 7 lands in the default arm */
		int fire = (trial / 8) & 1;
		int dur = study_duration(st);
		unsigned char at = codes[IDX(trial, 1)];
		float x = fsweep[IDX(trial, 5)];
		unsigned int ph = (unsigned int)(trial % NPHASE);
		int law = (trial >> 3) & 1;
		int pat = (trial * 13 + 5) & 0x3f;
		int alt, got, refgot, p, any;

		/* D281 again: one of phases 0..4 has to stay clear. */
		pat &= ~(1 << (trial % 5));

		seed(trial, trial % 4);
		BOTH(ucode, at);
		BOTH(ucodeLevel, levels[IDX(trial, 3)]);
		BOTH(pcmType, law ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW);
		BOTH(studyState, st);
		BOTH(stateSampleCount, fire ? dur - 1 : dur - 4);
		study_durations(11, 13, 7, 17, 5);
		BOTH(uniteUrefDistanceThresh, (short)(1 << (trial % 12)));
		BOTH(altRbsDistanceThresh, dists[IDX(trial, 7)]);
		BOTH(altRbsVarianceThresholdFactor, factors[IDX(trial, 3)]);
		BOTH(altMinVarThresh, (float)(trial % 5) * 1000.0f);
		BOTH(altRbsInUse, (short)-1);
		BOTH(trn1Sigma, -1.0f);

		for (p = 0; p < NPHASE; p++) {
			unsigned n = counts[(trial + p) % 8];

			BOTH(altRbsFlag[p], (short)((pat >> p) & 1));
			BOTH(linMapp[p][at], (short)(100 * p + trial * 7));
			BOTH(linMappAlt[p][at], (short)(50 * p - trial * 3));
			BOTH(magnitudeCount[p][at], n);
			BOTH(magnitudeSum[p][at], (float)((int)n * 143));
			BOTH(magnitudeSqSum[p][at], (float)((int)n * 4001));
			BOTH(linearMappingVar[p][at], 0.5f * (float)p + (float)trial);
			BOTH(altMagnitudeCount[p], counts[(trial + p + 3) % 8]);
			BOTH(altMagnitudeSum[p], (float)(trial * 11 + p));

			was[p] = ours_o.altRbsFlag[p];
			if (n == 0)
				emptycell = 1;
			else
				fullcell = 1;
		}

		/*
		 * `isAltRbs` reads and writes nothing, so asking it here is how
		 * the sweep knows which arm of states 2, 3 and 5 the call is
		 * about to take.
		 */
		alt = ours_o.isAltRbs((short)ph, (short)at, x);

		memcpy(before, ours.raw, SLOT);
		dsplib_debug_capture_reset();

		got = ours_o.studyUrefHandler(x, ph);
		refgot = ref_studyUrefHandler(&theirs_o, x, ph);

		diff_eq_int("studyUrefHandler returned the same (trial %ld)",
			    got, refgot, trial);
		diff_eq_obj("after studyUrefHandler", V90AutoDigitalImpDetector,
			    &ours_o, &theirs_o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("the study transcript matched (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);

		seen[st] = 1;
		if (dsplib_debug_capture_lines(0) != 0)
			talked = 1;
		if (got == 0)
			ret0 = 1;
		else if (got == 1)
			ret1 = 1;
		else if (got == 2)
			ret2 = 1;

		if (st < 6) {
			if (ours_o.studyState != st)
				fired[st] = 1;
			else
				held[st] = 1;
		}

		if (st == 2 || st == 3 || st == 5) {
			if (alt)
				altyes = 1;
			else
				altno = 1;
		}

		if (st == 0 && ours_o.studyState != 0) {
			any = 0;
			for (p = 0; p < NPHASE; p++)
				if (was[p] == 0 && ours_o.altRbsFlag[p] != 0)
					any = 1;
			if (any)
				flagged = 1;
			else
				unflagged = 1;
		}

		if ((st == 2 || st == 3) && ours_o.studyState != st)
			for (p = 0; p < NPHASE; p++) {
				if (was[p] == 0)
					continue;
				if (ours_o.altRbsFlag[p] == 0)
					cleared = 1;
				else
					kept = 1;
			}

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[0][at];
		else if (ours_o.linMapp[0][at] != first)
			distinct = 1;
	}

	/*
	 * THE TERMINAL ARM AND THE DEFAULT ONE.  State 6 answers 2 and touches
	 * nothing; anything above 6 falls through the jump table's bound.  The
	 * dispatch is `cmp $0x6; ja`, an UNSIGNED compare, so a NEGATIVE state
	 * word lands in the default arm and not below the table -- which is the
	 * one reading of that instruction a test can separate from the other.
	 */
	{
		static const int sts[] = {
			6, 7, 8, 100, -1, -2147483647 - 1
		};
		int k;

		for (k = 0; k < 6; k++) {
			int got, refgot;

			seed(700 + k, 0);
			BOTH(ucode, 0x2a);
			BOTH(studyState, sts[k]);
			BOTH(stateSampleCount, 3);
			dsplib_debug_capture_reset();

			got = ours_o.studyUrefHandler(1.25f, 2u);
			refgot = ref_studyUrefHandler(&theirs_o, 1.25f, 2u);

			diff_eq_int("terminal/default: same answer (%ld)", got,
				    refgot, sts[k]);
			diff_eq_obj("terminal/default: after studyUrefHandler",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, k);
			diff_eq_int("terminal/default: no store past the object "
				    "(%ld)", guard_equal(), 1, k);
			diff_eq_int("terminal/default: printed nothing (%ld)",
				    (long)dsplib_debug_capture_lines(0), 0, k);
			diff_eq_int("terminal/default: the state is untouched "
				    "(%ld)", (long)ours_o.studyState,
				    (long)sts[k], k);
			diff_eq_int("terminal/default: the count is untouched "
				    "(%ld)", (long)ours_o.stateSampleCount, 3, k);
			diff_eq_int("terminal/default: the answer (%ld)", got,
				    sts[k] == 6 ? 2 : 1, k);
			if (got == 2)
				ret2 = 1;
			else
				ret1 = 1;
		}
	}

	/*
	 * STATE 0's TAIL, BOTH WAYS.  Six equal variances put every entry at
	 * the average, `getAltVarThresh` averages the five below a threshold
	 * none of them reach and the answer is well above them all, so nothing
	 * is flagged and the accumulators are NOT cleared.  One wild phase
	 * drags the average up, is the only entry above the answer, and is
	 * flagged -- and then the clear runs.  The two differ in one float.
	 */
	{
		int k;

		for (k = 0; k < 2; k++) {
			unsigned char at = 0x33;
			int p, got, refgot, any;
			short was[NPHASE];

			seed(710 + k, 0);
			BOTH(ucode, at);
			BOTH(ucodeLevel, 0);
			BOTH(pcmType, PCM_TYPE_MU_LAW);
			BOTH(studyState, 0);
			BOTH(stateSampleCount, 4);
			study_durations(5, 13, 7, 17, 3);
			BOTH(altRbsVarianceThresholdFactor, 1.5f);
			BOTH(altMinVarThresh, 0.0f);

			for (p = 0; p < NPHASE; p++) {
				BOTH(altRbsFlag[p], 0);
				BOTH(magnitudeCount[p][at], 4u);
				BOTH(magnitudeSum[p][at], 400.0f);
				BOTH(magnitudeSqSum[p][at],
				     (k == 1 && p == 3) ? 4000000.0f
							: 44000.0f);
				was[p] = 0;
			}

			dsplib_debug_capture_reset();
			got = ours_o.studyUrefHandler(0.0f, 1u);
			refgot = ref_studyUrefHandler(&theirs_o, 0.0f, 1u);

			diff_eq_int("state 0: same answer (%ld)", got, refgot, k);
			diff_eq_obj("state 0: after studyUrefHandler",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, k);
			diff_eq_int("state 0: transcript matched (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, k);
			diff_eq_int("state 0: the tail fired (%ld)",
				    (long)ours_o.studyState, 1, k);

			any = 0;
			for (p = 0; p < NPHASE; p++)
				if (was[p] == 0 && ours_o.altRbsFlag[p] != 0)
					any = 1;
			if (any)
				flagged = 1;
			else
				unflagged = 1;
			diff_eq_int("state 0: the wild phase is the flagged one "
				    "(%ld)", any, k, k);
		}
	}

	/*
	 * THE WHOLE MACHINE, END TO END, COMPARED AFTER EVERY CALL.  Six short
	 * durations walk 0 -> 1 -> 2 -> 4 -> 3 -> 5 -> 6 in about twenty-two
	 * calls, and sixty are made: a divergence that appears at one state
	 * boundary and is washed out by the next is a defect, and a final-state
	 * comparison would miss it.
	 *
	 * One phase is cleared before every call, rotating, for D281's reason.
	 */
	{
		int step, p;
		int chain[8];

		for (p = 0; p < 8; p++)
			chain[p] = 0;

		seed(760, 0);
		BOTH(ucode, 0x2a);
		BOTH(ucodeLevel, 8031);
		BOTH(pcmType, PCM_TYPE_A_LAW);
		BOTH(studyState, 0);
		BOTH(stateSampleCount, 0);
		study_durations(3, 4, 3, 5, 2);
		BOTH(uniteUrefDistanceThresh, 40);
		BOTH(altRbsDistanceThresh, 60);
		BOTH(altRbsVarianceThresholdFactor, 1.5f);
		BOTH(altMinVarThresh, 10.0f);
		BOTH(altRbsInUse, 0);
		BOTH(trn1Sigma, 0.0f);

		for (p = 0; p < NPHASE; p++) {
			int c;

			BOTH(altRbsFlag[p], 0);
			BOTH(altMagnitudeCount[p], 0u);
			BOTH(altMagnitudeSum[p], 0.0f);
			for (c = 0; c < V90ADID_CODES; c++) {
				BOTH(magnitudeCount[p][c], 0u);
				BOTH(magnitudeSum[p][c], 0.0f);
				BOTH(magnitudeSqSum[p][c], 0.0f);
				BOTH(linearMappingVar[p][c], (float)(c + p));
				BOTH(linMapp[p][c], (short)(c * 64 + p * 5));
				BOTH(linMappAlt[p][c], (short)(c * 64 - p * 9));
			}
		}

		for (step = 0; step < 60; step++) {
			float x = (float)(7800 + ((step * 371) % 1200))
				  + 0.25f;
			unsigned int ph = (unsigned int)(step % NPHASE);
			int got, refgot;

			BOTH(altRbsFlag[step % 5], 0);

			dsplib_debug_capture_reset();
			got = ours_o.studyUrefHandler(x, ph);
			refgot = ref_studyUrefHandler(&theirs_o, x, ph);

			diff_eq_int("chain: same answer (step %ld)", got,
				    refgot, step);
			diff_eq_obj("chain: after studyUrefHandler",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, step);
			diff_eq_int("chain: no store past the object (step %ld)",
				    guard_equal(), 1, step);
			diff_eq_int("chain: transcript matched (step %ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, step);

			if (ours_o.studyState >= 0 && ours_o.studyState < 8)
				chain[ours_o.studyState] = 1;
			if (got == 0)
				ret0 = 1;
			else if (got == 2)
				ret2 = 1;
		}

		for (p = 0; p <= 6; p++)
			diff_eq_int("chain: state %ld was reached", chain[p], 1,
				    p);
		diff_eq_int("chain: the machine ended in the terminal state",
			    (long)ours_o.studyState, 6, 0);
		diff_eq_int("chain: trn1Sigma was written",
			    ours_o.trn1Sigma != 0.0f, 1, 0);
	}

	/*
	 * A VARIANCE EXACTLY ON THE THRESHOLD, AND A THRESHOLD EXACTLY ON A
	 * QUARTER.  Two claims share one witness.
	 *
	 * Six counts of 1 with six sums of 0 make each phase's variance its own
	 * sum of squares, so the six are planted directly: 1234.75 for phase 0
	 * and 0 for the rest.  `getAltVarThresh` then averages the five that are
	 * below a sixth of the total -- all five zeros -- and returns 0, which
	 * the floor at `altMinVarThresh` raises to exactly 1234.75.  So
	 * `var[0] == thresh` to the bit, which is the only input that separates
	 * the object's `>` from a `>=`; and the report prints `(int)thresh`,
	 * which is 1234 truncated and 1235 rounded.  Neither difference is
	 * reachable by sweeping: a random variance never lands on the answer a
	 * function of the other five produced, and a random threshold is under a
	 * half as often as not.  Finding F1366's method.
	 */
	{
		unsigned char at = 0x2a;
		int p, got, refgot;

		seed(730, 0);
		BOTH(ucode, at);
		BOTH(ucodeLevel, 0);
		BOTH(pcmType, PCM_TYPE_MU_LAW);
		BOTH(studyState, 0);
		BOTH(stateSampleCount, 4);
		study_durations(5, 13, 7, 17, 3);
		BOTH(altRbsVarianceThresholdFactor, 1.0f);
		BOTH(altMinVarThresh, 1234.75f);

		for (p = 0; p < NPHASE; p++) {
			BOTH(altRbsFlag[p], 0);
			BOTH(magnitudeCount[p][at], 1u);
			BOTH(magnitudeSum[p][at], 0.0f);
			BOTH(magnitudeSqSum[p][at], (p == 0) ? 1234.75f : 0.0f);
		}

		dsplib_debug_capture_reset();
		got = ours_o.studyUrefHandler(0.0f, 1u);
		refgot = ref_studyUrefHandler(&theirs_o, 0.0f, 1u);

		diff_eq_int("on the threshold: same answer (%ld)", got, refgot, 0);
		diff_eq_obj("on the threshold: after studyUrefHandler",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("on the threshold: transcript matched",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, 0);
		diff_eq_int("on the threshold: the tail fired",
			    (long)ours_o.studyState, 1, 0);
		diff_eq_int("on the threshold: an equal variance does not flag",
			    (long)ours_o.altRbsFlag[0], 0, 0);
		diff_eq_int("on the threshold: the report was made",
			    dsplib_debug_capture_lines(0) > 0, 1, 0);
	}

	/*
	 * A SIGMA THAT IS NOT A FLOAT.  `trn1Sigma` reaches its field through
	 * `fsts` and its report through `fstpl`, two roundings of one extended
	 * register (finding F1443, D291) -- and the two agree for every quotient
	 * that happens to BE a float, which most seeded ones are.  Three
	 * unflagged phases with variances 1, 1 and 0 make the quotient 2/3,
	 * which is not, so the reported mantissa and the stored one differ.
	 *
	 * State 5 rather than state 3 because state 3 re-tests the flags first
	 * and would change which phases the average is over.  Every count is
	 * zero, so `updateUref` and `updateUrefAlt` write nothing, and the
	 * mapping entries are a thousand apart against a merge threshold of 1,
	 * so the unite forms no group and leaves the three variances alone.
	 * Phases 0..2 are unflagged, which is also what keeps D281 out.
	 */
	{
		unsigned char at = 0x2a;
		int p, got, refgot;

		seed(740, 0);
		BOTH(ucode, at);
		BOTH(ucodeLevel, 0);
		BOTH(pcmType, PCM_TYPE_MU_LAW);
		BOTH(studyState, 5);
		BOTH(stateSampleCount, 16);
		study_durations(11, 13, 7, 17, 5);
		BOTH(uniteUrefDistanceThresh, 1);
		BOTH(altRbsDistanceThresh, 30000);

		for (p = 0; p < NPHASE; p++) {
			BOTH(altRbsFlag[p], (short)(p >= 3 ? 1 : 0));
			BOTH(magnitudeCount[p][at], 0u);
			BOTH(altMagnitudeCount[p], 0u);
			BOTH(altMagnitudeSum[p], 0.0f);
			BOTH(linMapp[p][at], (short)(p * 1000));
			BOTH(linMappAlt[p][at], (short)(p * 1000));
			BOTH(linearMappingVar[p][at], (p < 2) ? 1.0f : 0.0f);
		}

		dsplib_debug_capture_reset();
		got = ours_o.studyUrefHandler(0.0f, 0u);
		refgot = ref_studyUrefHandler(&theirs_o, 0.0f, 0u);

		diff_eq_int("two thirds: same answer (%ld)", got, refgot, 0);
		diff_eq_obj("two thirds: after studyUrefHandler",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, 0);
		diff_eq_int("two thirds: transcript matched",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, 0);
		diff_eq_int("two thirds: the study finished",
			    (long)ours_o.studyState, 6, 0);
		diff_eq_int("two thirds: and said so", got, 2, 0);
	}

	study_debug_off();

	/*
	 * AND ONCE WITH THE GATE SHUT, WITH THE CAPTURE STILL ON.  Half of what
	 * this method prints is behind `dsplibs_debug_level > 1` and half -- the
	 * two `edprintf` sites in states 0 and 1, and the three `getAltVarThresh`
	 * makes on their behalf -- is not.  At level 2 both halves print and a
	 * gate written the wrong way round is invisible; at level 0 only the
	 * ungated half does, and the transcript separates them.  Finding F1428,
	 * which is why the level has to be driven at 0 AND at 2 rather than
	 * merely turned off at the end.
	 */
	{
		int k;

		dsplib_debug_capture_on = 1;

		for (k = 0; k < 8; k++) {
			int st = k % 7;
			int dur = study_duration(st);
			int p, got, refgot;

			seed(780 + k, 0);
			BOTH(ucode, (unsigned char)(0x21 + k));
			BOTH(ucodeLevel, (short)(2000 + k * 91));
			BOTH(pcmType, (k & 1) ? PCM_TYPE_A_LAW
					      : PCM_TYPE_MU_LAW);
			BOTH(studyState, st);
			BOTH(stateSampleCount, dur - 1);
			study_durations(11, 13, 7, 17, 5);
			BOTH(altRbsDistanceThresh, (short)(k * 40));
			BOTH(altRbsVarianceThresholdFactor, 1.5f);
			BOTH(altMinVarThresh, 100.0f);

			for (p = 0; p < NPHASE; p++) {
				BOTH(altRbsFlag[p], (short)(p == 0 ? 0
							    : (k >> p) & 1));
				BOTH(magnitudeCount[p][0x21 + k], (unsigned)(p + 1));
				BOTH(magnitudeSum[p][0x21 + k],
				     (float)(300 * (p + 1)));
				BOTH(magnitudeSqSum[p][0x21 + k],
				     (float)(91000 * (p + 1)));
				BOTH(altMagnitudeCount[p], (unsigned)(k + p));
				BOTH(altMagnitudeSum[p], (float)(k * 100 + p));
			}

			dsplib_debug_capture_reset();
			got = ours_o.studyUrefHandler(1234.5f, (unsigned)(k % 6));
			refgot = ref_studyUrefHandler(&theirs_o, 1234.5f,
						      (unsigned)(k % 6));

			diff_eq_int("gate shut: same answer (%ld)", got, refgot,
				    k);
			diff_eq_obj("gate shut: after studyUrefHandler",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, k);
			diff_eq_int("gate shut: no store past the object (%ld)",
				    guard_equal(), 1, k);
			diff_eq_int("gate shut: transcript matched (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, k);
			diff_eq_int("gate shut: nothing reached the transcript "
				    "(%ld)",
				    (long)dsplib_debug_capture_lines(0), 0, k);
			gated += (long)dsplib_debug_capture_lines(0);

			/*
			 * AND THE UNGATED HALF STILL RAN.  `edprintf` gates
			 * only its final call and advances the rotating key
			 * whatever the level is, so two probes of the key say
			 * how many characters the arm's `edprintf` sites
			 * formatted -- which is the only way to see that
			 * states 0 and 1 print unconditionally where every
			 * other report is behind `DSPLIB_DEBUG_ON()`.
			 */
			{
				int a1 = (unsigned char)cEncodeChar(0);
				int a2 = (unsigned char)cEncodeChar(0);
				int b1 = (unsigned char)ref_cEncodeChar(0);
				int b2 = (unsigned char)ref_cEncodeChar(0);

				diff_eq_int("gate shut: the encode key is in "
					    "step (%ld)", a1 * 256 + a2,
					    b1 * 256 + b2, k);
				if (a1 != b1 || a2 != b2)
					keydrift = 1;
			}
		}

		/*
		 * AND THE KEY PROBE NEEDS THE LENGTHS TO MOVE.  `iEncodeOffset`
		 * runs modulo TEN and every `edprintf` leaves it at twice its
		 * formatted length, so only five positions are reachable and two
		 * different reports land on the same one half the time.  Sweeping
		 * the width of what the report prints is what makes the probe
		 * decide: five runs whose report is one character longer each
		 * cover all five positions, so no alternative length matches them
		 * all.  Without it a report moved behind the gate is invisible at
		 * every level -- open, it prints either way; shut, the key happens
		 * to agree.
		 *
		 * THIS IS STATE 1'S REPORT AND NOT STATE 0's, and the difference is
		 * that state 1's is the only `edprintf` its arm makes.  State 0's
		 * arm calls `getAltVarThresh`, which makes three of its own, and
		 * measured at level 0 the key after that arm sits at the THIRD of
		 * those and not at the pattern report -- for a reason this batch
		 * did not run to ground, since the same probe reads the pattern
		 * report correctly at level 2 and both transcripts and both objects
		 * agree at both levels.  An assertion nobody can explain is worse
		 * than none, so state 0's report is left with its wording and its
		 * arguments tested and its ungatedness not.  Finding F1445.
		 */
		/*
		 * Then state 1, whose report is the arm's only `edprintf` and
		 * whose widths come out of `linMapp`.  Every count is zero and
		 * no phase is flagged, so `updateUref` writes nothing and the
		 * unite -- with the mapping entries a thousand apart against a
		 * merge threshold of 1 -- forms no group and leaves them.
		 */
		for (k = 0; k < 5; k++) {
			static const short widths[] = {
				1, 12, 123, 1234, 12345
			};
			unsigned char at = 0x2a;
			int p, got, refgot;
			int a1, a2, b1, b2;

			seed(830 + k, 0);
			BOTH(ucode, at);
			BOTH(ucodeLevel, 0);
			BOTH(pcmType, PCM_TYPE_MU_LAW);
			BOTH(studyState, 1);
			BOTH(stateSampleCount, 6);
			study_durations(5, 7, 3, 17, 5);
			BOTH(uniteUrefDistanceThresh, 1);

			for (p = 0; p < NPHASE; p++) {
				BOTH(altRbsFlag[p], 0);
				BOTH(magnitudeCount[p][at], 0u);
				BOTH(linMapp[p][at],
				     (short)((p == 0) ? widths[k]
					     : (short)(p * 1000 + 7)));
			}

			dsplib_debug_capture_reset();
			got = ours_o.studyUrefHandler(0.0f, 2u);
			refgot = ref_studyUrefHandler(&theirs_o, 0.0f, 2u);

			a1 = (unsigned char)cEncodeChar(0);
			a2 = (unsigned char)cEncodeChar(0);
			b1 = (unsigned char)ref_cEncodeChar(0);
			b2 = (unsigned char)ref_cEncodeChar(0);

			diff_eq_int("width sweep: same answer (%ld)", got,
				    refgot, k);
			diff_eq_obj("width sweep: after studyUrefHandler",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, k);
			diff_eq_int("width sweep: the key is in step (%ld)",
				    a1 * 256 + a2, b1 * 256 + b2, k);
			diff_eq_int("width sweep: still silent (%ld)",
				    (long)dsplib_debug_capture_lines(0), 0, k);
			diff_eq_int("width sweep: the tail fired (%ld)",
				    (long)ours_o.studyState, 2, k);
			if (a1 != b1 || a2 != b2)
				keydrift = 1;
		}

		/*
		 * AND THE ONE GATED REPORT THAT NEEDS A WITNESS OF ITS OWN.
		 * The false-detection line only fires when the re-test WITHDRAWS
		 * a flag, which a seeded object reaches almost never: it needs
		 * the phase's alternate entry within `altRbsDistanceThresh` of its plain
		 * one.  Both are planted equal here, so both flagged phases are
		 * withdrawn and both would print -- which at level 0 is exactly
		 * the difference between the object's gate and no gate.  Every
		 * count is zero so the two updates before it write nothing, and
		 * phase 0 is left clear for D281.
		 */
		{
			unsigned char at = 0x2a;
			int p, got, refgot;

			seed(840, 0);
			BOTH(ucode, at);
			BOTH(ucodeLevel, 0);
			BOTH(pcmType, PCM_TYPE_MU_LAW);
			BOTH(studyState, 2);
			BOTH(stateSampleCount, 2);
			study_durations(5, 7, 3, 17, 5);
			BOTH(uniteUrefDistanceThresh, 1);
			BOTH(altRbsDistanceThresh, 100);

			for (p = 0; p < NPHASE; p++) {
				BOTH(altRbsFlag[p],
				     (short)((p == 1 || p == 2) ? 1 : 0));
				BOTH(magnitudeCount[p][at], 0u);
				BOTH(altMagnitudeCount[p], 0u);
				BOTH(altMagnitudeSum[p], 0.0f);
				BOTH(linMapp[p][at], 1234);
				BOTH(linMappAlt[p][at], 1234);
			}

			dsplib_debug_capture_reset();
			got = ours_o.studyUrefHandler(0.0f, 0u);
			refgot = ref_studyUrefHandler(&theirs_o, 0.0f, 0u);

			diff_eq_int("withdrawal: same answer (%ld)", got,
				    refgot, 0);
			diff_eq_obj("withdrawal: after studyUrefHandler",
				    V90AutoDigitalImpDetector, &ours_o,
				    &theirs_o, 0);
			diff_eq_int("withdrawal: the gated report stayed shut",
				    (long)dsplib_debug_capture_lines(0), 0, 0);
			diff_eq_int("withdrawal: the first flag was withdrawn",
				    (long)ours_o.altRbsFlag[1], 0, 0);
			diff_eq_int("withdrawal: the second flag was withdrawn",
				    (long)ours_o.altRbsFlag[2], 0, 0);
			cleared = 1;
		}

		dsplib_debug_capture_on = 0;
	}

	/* At level 0 the gated half of the reports reaches nothing at all. */
	diff_eq_int("the gated reports were silent with the gate shut", gated, 0,
		    0);
	diff_eq_int("the encode key never drifted", keydrift, 0, 0);

	for (i = 0; i <= 7; i++)
		diff_eq_int("the sweep entered arm %ld", seen[i], 1, i);
	for (i = 0; i < 6; i++) {
		diff_eq_int("arm %ld fired its tail", fired[i], 1, i);
		diff_eq_int("arm %ld held its tail", held[i], 1, i);
	}

	diff_eq_int("studyUrefHandler changed the object", moved, 1, 0);
	diff_eq_int("the reference entry is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("an answer of 0 was seen", ret0, 1, 0);
	diff_eq_int("an answer of 1 was seen", ret1, 1, 0);
	diff_eq_int("an answer of 2 was seen", ret2, 1, 0);
	diff_eq_int("the alternate-RBS test said yes", altyes, 1, 0);
	diff_eq_int("the alternate-RBS test said no", altno, 1, 0);
	diff_eq_int("state 0 flagged a phase", flagged, 1, 0);
	diff_eq_int("state 0 flagged nothing", unflagged, 1, 0);
	diff_eq_int("the re-test cleared a flag", cleared, 1, 0);
	diff_eq_int("the re-test kept a flag", kept, 1, 0);
	diff_eq_int("a cell with no samples was exercised", emptycell, 1, 0);
	diff_eq_int("a cell with samples was exercised", fullcell, 1, 0);
	diff_eq_int("the method printed", talked, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_resetlinearmapping();
	rc |= run_reset();
	rc |= run_calculatedillength();
	rc |= run_setdildescriptor();
	rc |= run_setters();
	rc |= run_clears();
	rc |= run_accumulate();
	rc |= run_means();
	rc |= run_maptransforms();
	rc |= run_queries();
	rc |= run_unite();
	rc |= run_updateuref();
	rc |= run_studyreset();
	rc |= run_altvarthresh();
	rc |= run_uniteunsuspected();
	rc |= run_firststudy();
	rc |= run_dilrepair();
	rc |= run_secondstudy();
	rc |= run_qcmapping();
	rc |= run_maxucode();
	rc |= run_padgain();
	rc |= run_studyuref();
	rc |= run_signal();

	return rc;
}
