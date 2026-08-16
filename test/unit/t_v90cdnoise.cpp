/*
 * t_v90cdnoise.cpp -- differential test of
 * `V90ConstellationDesigner::setConstellationToNoise`, against the blob's own
 * copy.
 *
 * It is a sibling of `t_v90cdesign.cpp` rather than another group inside it
 * because that file is already nine groups long and because this member needs
 * a fixture nothing else there wants: six tables, a five-way switch and a
 * staging buffer that must not be allowed to overflow.  The wiring helpers
 * below are copied rather than shared, which is the same call `t_v90cdesign`
 * made about the ctor/dtor test's helpers.
 *
 * NOTHING IN THE OBJECT CALLS IT (finding 2141's sweep covers this member
 * too), so it is driven directly by symbol on both sides -- ours by the
 * mangled name, the blob's by the `ref_` alias -- through an `asm()` label.
 * Plain cdecl, `this` as the first STACK argument (finding 215), and the
 * `float` occupies one stack slot.
 *
 * WHAT THIS MEMBER WRITES, and therefore what is compared:
 *
 *   `V90MappingParams`   all six `constellationSize`, and `constellation` and
 *                        `codecConstellation` up to each size.  Compared as a
 *                        WHOLE 0x650-byte object, because a field-by-field
 *                        check would pass over anything else it touched.
 *   `this`               +0x0a, +0x10 and +0x48 always; +0x18, +0x1c and
 *                        +0x20 in three of the four switch arms.  Each side
 *                        is compared against ITS OWN pre-call snapshot for
 *                        the "nothing else changed" half -- the two hold
 *                        different collaborator addresses by construction --
 *                        and side against side for the values.
 *   nothing else         the three input tables, the two per-constellation
 *                        arrays and the parameter block are all asserted
 *                        unchanged.
 *
 * THE HAZARDS ARE FRAME SMASHES, NOT DIFFERENCES, so the fixture excludes
 * them by construction rather than by tolerance:
 *
 *   THE STAGING BUFFER.  Accepted indices go into a 128-byte local and the
 *   loop runs from `params->unnamed_360` to `lastUcode[k]`, which is a byte.
 *   A span of 256 can therefore stage 256 entries into 128 bytes and smash
 *   the frame -- ours and the blob's differently, since the two frames are
 *   not the same.  Every trial below keeps the span at or under 120.  D333.
 *
 *   THE ROWS ARE SEVEN DEEP AND NOT SIX.  `i` reaches `lastUcode[k]`, up to
 *   255, and the report reads `ucode[k][constellation[k][u]]` with the same
 *   byte, so the largest flat index the object can form is 5*128 + 255 = 895
 *   -- the last element of a seven-row array exactly.  Six rows would be a
 *   read past the end on both sides and the test would be measuring the
 *   allocator.  D325's argument, with this member's own arithmetic.
 *
 * WHAT THE TRANSCRIPT CAN AND CANNOT CLASSIFY.  Sixteen of the twenty-two
 * diagnostics are `dsplibs_debug_printf` and readable; the other six are
 * `edprintf`, whose output is ENCODED, so "dMin Forced to:" cannot be found
 * with `strstr`.  The arms behind those six are classified from the inputs
 * the fixture chose and asserted through their observable effect instead --
 * and the encoded text is still compared between the sides, which is what
 * catches a wrong argument in any of the six.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
extern "C" {
#include "dsplib/pcm.h"
}
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90ConstellationDesigner.h"

extern "C" {

void our_ctn(void *, float, void *, void *, short *, unsigned char *, void *)
	asm("_ZN24V90ConstellationDesigner23setConstellationToNoiseEfPA128_sS1_"
	    "PsPhPA128_h");
void ref_ctn(void *, float, void *, void *, short *, unsigned char *, void *)
	asm("ref__ZN24V90ConstellationDesigner23setConstellationToNoiseEfPA128_"
	    "sS1_PsPhPA128_h");

extern unsigned int ref_dsplibs_debug_level;

}

/* The same generator t_v90cdesign uses, so a value that happens to be a
 * constant the code stores is not mistaken for a store. */
static unsigned lfsr;

static void
reseed(unsigned s)
{
	lfsr = s | 1u;
}

static unsigned
nextrand(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	lfsr = lfsr * 1103515245u + 12345u;
	return lfsr;
}

static V90MappingParams mpA;
static V90MappingParams mpB;

static unsigned char parAbuf[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char parBbuf[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char cdAbuf[sizeof(V90ConstellationDesigner)]
	__attribute__((aligned(8)));
static unsigned char cdBbuf[sizeof(V90ConstellationDesigner)]
	__attribute__((aligned(8)));
static V90Parameters *parA;
static V90Parameters *parB;
static V90ConstellationDesigner *cdA;
static V90ConstellationDesigner *cdB;

/* Seven rows; see the file header for why six is not enough. */
static short ucA[7][128];
static short ucB[7][128];
static short alA[7][128];
static short alB[7][128];
static unsigned char okA[7][128];
static unsigned char okB[7][128];
static short dminA[6];
static short dminB[6];
static unsigned char lastA[6];
static unsigned char lastB[6];

/*
 * `constelTable` is at +0x14 and this member never reads it, which is a
 * negative claim -- so it is pointed at a buffer that would fault nothing and
 * the whole-object comparison would show any write to it.
 */
static unsigned char tblA[12288] __attribute__((aligned(8)));
static unsigned char tblB[12288] __attribute__((aligned(8)));

static void
wire(void)
{
	parA = (V90Parameters *)parAbuf;
	parB = (V90Parameters *)parBbuf;
	cdA = (V90ConstellationDesigner *)cdAbuf;
	cdB = (V90ConstellationDesigner *)cdBbuf;
	memset((void *)cdA, 0, sizeof(V90ConstellationDesigner));
	memset((void *)cdB, 0, sizeof(V90ConstellationDesigner));
	memset(parAbuf, 0, sizeof(parAbuf));
	memset(parBbuf, 0, sizeof(parBbuf));
	memset(tblA, 0x5a, sizeof(tblA));
	memset(tblB, 0x5a, sizeof(tblB));
	cdA->params = parA;
	cdB->params = parB;
	cdA->mappingParams = &mpA;
	cdB->mappingParams = &mpB;
	cdA->constelTable = (short (*)[128])tblA;
	cdB->constelTable = (short (*)[128])tblB;
}

static unsigned char snapA[sizeof(V90ConstellationDesigner)];
static unsigned char snapB[sizeof(V90ConstellationDesigner)];

static void
snap_this(void)
{
	memcpy(snapA, cdAbuf, sizeof(snapA));
	memcpy(snapB, cdBbuf, sizeof(snapB));
}

/*
 * Put the six written fields back to what they were and require the rest of
 * the object to be untouched.  That is the negative half of the claim and it
 * is asserted rather than assumed, the way `run_dmin_quiet` does it.
 */
static void
restore_and_check_this(long input)
{
	const V90ConstellationDesigner *sa =
	    (const V90ConstellationDesigner *)snapA;
	const V90ConstellationDesigner *sb =
	    (const V90ConstellationDesigner *)snapB;

	cdA->short_0a = sa->short_0a;
	cdA->short_10 = sa->short_10;
	cdA->word_48 = sa->word_48;
	cdA->float_18 = sa->float_18;
	cdA->float_1c = sa->float_1c;
	cdA->float_20 = sa->float_20;
	cdB->short_0a = sb->short_0a;
	cdB->short_10 = sb->short_10;
	cdB->word_48 = sb->word_48;
	cdB->float_18 = sb->float_18;
	cdB->float_1c = sb->float_1c;
	cdB->float_20 = sb->float_20;

	diff_eq_obj("ours writes only the six", V90ConstellationDesigner,
		    cdAbuf, snapA, input);
	diff_eq_obj("the blob writes only the six", V90ConstellationDesigner,
		    cdBbuf, snapB, input);
}

/* Fill both mapping blocks identically with varied bytes. */
static void
fill_mp(unsigned s)
{
	unsigned char *a = (unsigned char *)&mpA;
	unsigned char *b = (unsigned char *)&mpB;
	unsigned i;

	reseed(s);
	for (i = 0; i < sizeof(mpA); i++) {
		unsigned char v = (unsigned char)(nextrand() >> 13);

		a[i] = v;
		b[i] = v;
	}
}

/*
 * ===========================================================================
 * The fixture
 * ===========================================================================
 *
 * THE 16-BIT TABLES ARE RAMPS AND NOT NOISE, for finding 2164's reason.  Both
 * inner loops count an entry only when it clears a threshold that jumps to
 * `seed + value` after every hit, so over uniformly random 16-bit values the
 * count is a RECORD count -- about ln(span), and essentially independent of
 * the seed.  A count that does not move with the seed leaves every claim
 * about `short_10` and `short_0a` untested.  Against a ramp of slope `ramp`
 * the count is about `span * ramp / (seed + ramp)`, which is smooth,
 * monotone and tunable, so the sweep can put it anywhere between zero and the
 * whole span.
 */
struct ctn_case {
	float noise;
	int w48;
	int restricted;
	int w24;
	int w2c;
	int w28;
	int forced;
	short dmin0a;
	short dmin0c;
	short dmin0e;
	int start;
	int allZeroAllow;
};

static struct ctn_case cur;

static const int w48tab[8] = { 0, 1, 2, 3, 0, -1, 4, 100 };
static const int fdtab[7] = { -2, -1, 0, 1, 63, 0x8123, 32767 };
static const int w2ctab[5] = { 0, 0, 1, 1, 3 };
static const int w28tab[5] = { 0, 1, 0, 1, 3 };
static const float noisetab[13] = {
	0.0f, 1.0f, -1.0f, 4.5f, -4.5f, 7.9f, -7.9f, 12.25f, -12.25f,
	40.0f, -40.0f, 0.125f, -0.125f
};

/*
 * Plant one trial's inputs on both sides.  Nothing here is random except the
 * jitter on the tables and the row lengths; every discriminating value is
 * indexed off the trial with a period coprime to the others, so the sweep
 * crosses them rather than walking one at a time.
 */
static void
ctn_fixture(int trial)
{
	unsigned ramp = 1u + (unsigned)(trial % 23);
	int k;
	int j;
	int span;

	reseed(0x3c3cu + 137u * (unsigned)trial);

	cur.w48 = w48tab[trial % 8];
	cur.forced = fdtab[trial % 7];
	cur.w2c = w2ctab[trial % 5];
	cur.w28 = w28tab[trial % 5];
	cur.noise = noisetab[trial % 13];
	cur.restricted = (trial % 11) < 4;
	cur.w24 = (trial % 17) < 8;
	cur.allZeroAllow = (trial % 31) == 0;
	cur.start = (trial % 3 == 0) ? 0
		  : (trial % 3 == 1) ? (int)(nextrand() % 24u)
				     : 100 + (int)(nextrand() % 36u);

	for (k = 0; k < 7; k++)
		for (j = 0; j < 128; j++) {
			short u = (short)((unsigned)j * ramp
					  + nextrand() % (2u * ramp))
				- (short)ramp;
			short a = (short)(u + (int)(nextrand() % 64u) - 32);
			unsigned char ok = (unsigned char)
			    (cur.allZeroAllow ? 0 : ((nextrand() % 8u) != 0));

			ucA[k][j] = u;
			ucB[k][j] = u;
			alA[k][j] = a;
			alB[k][j] = a;
			okA[k][j] = ok;
			okB[k][j] = ok;
		}

	for (k = 0; k < 6; k++) {
		/*
		 * THE SPAN IS CAPPED AT 120 AND THAT IS THE FRAME GUARD, not
		 * a convenience: see the file header and D333.
		 */
		span = 1 + (int)(nextrand() % 120u);
		if (cur.start + span - 1 > 255)
			span = 256 - cur.start;
		if (span < 1)
			span = 1;
		lastA[k] = (unsigned char)(cur.start + span - 1);
		lastB[k] = lastA[k];
		dminA[k] = (short)(((trial >> k) & 1) ? 1 : 0);
		dminB[k] = dminA[k];
	}
	/* Both inner loops in every trial, whatever the bit pattern says. */
	dminA[trial % 6] = 0;
	dminB[trial % 6] = 0;
	dminA[(trial + 3) % 6] = (short)(1 + (trial % 7));
	dminB[(trial + 3) % 6] = dminA[(trial + 3) % 6];

	fill_mp(0x9a1cu + 61u * (unsigned)trial);

	parA->unnamed_360 = cur.start;
	parA->USE_RESTRICED_DMIN = cur.restricted;
	parA->FORCED_DMIN = cur.forced;
	parB->unnamed_360 = parA->unnamed_360;
	parB->USE_RESTRICED_DMIN = parA->USE_RESTRICED_DMIN;
	parB->FORCED_DMIN = parA->FORCED_DMIN;

	cur.dmin0a = (short)((int)(nextrand() % 200u) - 40);
	cur.dmin0c = (short)((trial % 5 == 0) ? 0
			     : (int)(nextrand() % 200u) - 60);
	cur.dmin0e = (short)((trial % 7 == 0) ? 0
			     : (int)(nextrand() % 200u) - 60);

	cdA->word_48 = (unsigned int)cur.w48;
	cdA->word_24 = (unsigned int)cur.w24;
	cdA->word_2c = cur.w2c;
	cdA->word_28 = cur.w28;
	cdA->short_0a = cur.dmin0a;
	cdA->short_0c = cur.dmin0c;
	cdA->short_0e = cur.dmin0e;
	cdA->short_10 = 0x0bad;
	cdA->float_18 = (trial & 1) ? 1.5f : -1.5f;
	cdA->float_1c = (trial & 2) ? 2.25f : -2.25f;
	cdA->float_20 = (trial & 4) ? 3.75f : -3.75f;

	cdB->word_48 = cdA->word_48;
	cdB->word_24 = cdA->word_24;
	cdB->word_2c = cdA->word_2c;
	cdB->word_28 = cdA->word_28;
	cdB->short_0a = cdA->short_0a;
	cdB->short_0c = cdA->short_0c;
	cdB->short_0e = cdA->short_0e;
	cdB->short_10 = cdA->short_10;
	cdB->float_18 = cdA->float_18;
	cdB->float_1c = cdA->float_1c;
	cdB->float_20 = cdA->float_20;
}

/* Every outcome the sweep has to reach. */
static int seenArm[8];			/* keep/up/down/none/high/neg      */
static int seenRestricted[2];
static int seenSign[2];
static int seenForced[2];
static int seenForcedTrunc;
static int seenDminArm[2];
static int seenLaw[2];
static int seenEq[2];
static int seenMinTakes[2];
static int seenClamp[2];
static int seenMaxM[2];
static int seenUpArm[3];
static int seenDownArm[3];
static int seenPicked[2];
static int seenIndexHigh;
static int seenNegOddSeed;
static int seenPrinted;

enum {
	ARM_KEEP = 0, ARM_UP, ARM_DOWN, ARM_NONE, ARM_HIGH, ARM_NEG
};

/*
 * The codec byte the object would compute for one entry, so the `word_2c ==
 * word_28` arm's `min` can be classified exactly rather than by the proxy
 * "the two bytes came out equal".  It is deliberately spelled the way the
 * reconstruction spells it; what it classifies is the BLOB's output, so a
 * shared mistake here shows up as a count that never moves, which the
 * outcome group then fails on.
 */
static unsigned char
ctn_codec(int law, int k, unsigned char c)
{
	int s = __builtin_abs((int)ucB[k][c]);

	if (law != 0)
		return (unsigned char)(linear2alaw(s) ^ 0xd5);
	return (unsigned char)~linear2ulaw(s);
}

static void
ctn_classify(int trial)
{
	int k;
	unsigned int i;
	unsigned int maxM = 0;

	switch (cur.w48) {
	case 1:  seenArm[ARM_KEEP]++; break;
	case 2:  seenArm[ARM_UP]++; break;
	case 3:  seenArm[ARM_DOWN]++; break;
	case 0:  seenArm[ARM_NONE]++; break;
	default: seenArm[cur.w48 < 0 ? ARM_NEG : ARM_HIGH]++; break;
	}
	seenRestricted[cur.restricted ? 1 : 0]++;
	seenSign[cur.noise > 0.0f ? 1 : 0]++;
	seenForced[cur.forced > -1 ? 1 : 0]++;
	if (cur.forced > 32767)
		seenForcedTrunc++;
	seenLaw[cur.w2c != 0 ? 1 : 0]++;
	seenEq[cur.w2c == cur.w28 ? 1 : 0]++;

	if (cur.w48 == 2) {
		if (cur.dmin0e == 0)
			seenUpArm[0]++;
		else if (cur.dmin0e > cdB->short_0a)
			seenUpArm[1]++;
		else
			seenUpArm[2]++;
	}
	if (cur.w48 == 3) {
		if (cur.dmin0c == 0)
			seenDownArm[0]++;
		else if (cur.dmin0c < cdB->short_0a)
			seenDownArm[1]++;
		else
			seenDownArm[2]++;
	}

	for (k = 0; k < 6; k++) {
		seenDminArm[dminB[k] != 0 ? 1 : 0]++;
		seenPicked[mpB.constellationSize[k] != 0 ? 1 : 0]++;
		if (mpB.constellationSize[k] > maxM)
			maxM = mpB.constellationSize[k];
		for (i = 0; i < mpB.constellationSize[k]; i++) {
			unsigned char c = mpB.constellation[k][i];

			if (c > 127)
				seenIndexHigh++;
			if (cur.w2c == cur.w28) {
				unsigned char e = ctn_codec(cur.w2c, k, c);

				seenMinTakes[e < c ? 1 : 0]++;
			}
		}
	}
	seenMaxM[maxM != 0 ? 1 : 0]++;
	(void)trial;
}

/*
 * ===========================================================================
 * The quiet pass
 * ===========================================================================
 */
#define CTN_TRIALS	720

static int
run_ctn_quiet(void)
{
	int trial;

	diff_begin("V90ConstellationDesigner::setConstellationToNoise, quiet");
	wire();

	for (trial = 0; trial < CTN_TRIALS; trial++) {
		ctn_fixture(trial);
		snap_this();

		our_ctn(cdA, cur.noise, ucA, alA, dminA, lastA, okA);
		ref_ctn(cdB, cur.noise, ucB, alB, dminB, lastB, okB);

		diff_eq_obj("the mapping parameters", V90MappingParams,
			    &mpA, &mpB, trial);
		diff_eq_int("dMin (trial %ld)", cdA->short_0a, cdB->short_0a,
			    trial);
		diff_eq_int("short_10 (trial %ld)", cdA->short_10,
			    cdB->short_10, trial);
		diff_eq_int("word_48 (trial %ld)", (long)cdA->word_48,
			    (long)cdB->word_48, trial);
		diff_eq_obj("the three pdsnr thresholds", float[3],
			    &cdA->float_18, &cdB->float_18, trial);

		diff_eq_obj("the ucode table is read only", short[7][128],
			    ucA, ucB, trial);
		diff_eq_obj("the second table is read only", short[7][128],
			    alA, alB, trial);
		diff_eq_obj("the flag table is read only", unsigned char[7][128],
			    okA, okB, trial);
		diff_eq_obj("the per-constellation dmin is read only",
			    short[6], dminA, dminB, trial);
		diff_eq_obj("the per-constellation bound is read only",
			    unsigned char[6], lastA, lastB, trial);
		diff_eq_obj("the parameters are read only", V90Parameters,
			    parA, parB, trial);
		diff_eq_obj("+0x14's table is not touched",
			    unsigned char[12288], tblA, tblB, trial);

		ctn_classify(trial);

		/*
		 * THE FORCED dMin IS ASSERTED AND NOT ONLY COMPARED: both
		 * sides agreeing on a wrong value would pass the comparison
		 * above, and this arm's whole content is one store.
		 */
		if (cur.forced > -1)
			diff_eq_int("a forced dMin arrives truncated (%ld)",
				    cdB->short_0a, (short)cur.forced,
				    cur.forced);

		restore_and_check_this(trial);
	}

	return diff_end();
}

/*
 * ===========================================================================
 * The loud pass -- and the levels are swept, not fixed
 * ===========================================================================
 *
 * Every gate in this member is `cmpl $0x1`, so level 2 is where the eighteen
 * `dsplibs_debug_printf` sites and the six `edprintf` ones all speak.  Levels
 * 1 and 3 are swept anyway, because debug.h's note about `cadence_progress`
 * is that a site at the wrong threshold produces a byte-identical transcript
 * at one level and not at another, and a test that fixes the level cannot see
 * it (finding 150).
 */
static const char *const ctn_sites[12] = {
	": noiseEnergy = ",
	": dMinHighRates = ",
	"KeepRate => keep dMin",
	"dMin calc OneRateUp",
	": rrnUpDmin = ",
	"dMin calc OneRateDown",
	": rrnDownDmin = ",
	"dMin calc NoRestriction",
	"adjusting dMin for rate>=53k",
	"V90 Constellation Designer report:",
	"constelation size phase[0..5]",
	"ucode["
};

static int ctn_seen[12];

static long
ctn_num(const char *t, const char *key, long dflt)
{
	const char *p = strstr(t, key);
	long v;

	if (p == NULL || sscanf(p + strlen(key), "%ld", &v) != 1)
		return dflt;
	return v;
}

static int
ctn_count(const char *t, const char *key)
{
	const char *p = t;
	size_t k = strlen(key);
	int n = 0;

	while ((p = strstr(p, key)) != NULL) {
		n++;
		p += k;
	}
	return n;
}

static int
run_ctn_loud(void)
{
	int trial;
	int i;

	diff_begin("setConstellationToNoise's diagnostics, both sides talking");
	wire();

	for (trial = 0; trial < CTN_TRIALS; trial++) {
		unsigned int level = 1u + (unsigned)(trial % 3);
		const char *t;

		ctn_fixture(trial);
		snap_this();

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = level;
		ref_dsplibs_debug_level = level;

		our_ctn(cdA, cur.noise, ucA, alA, dminA, lastA, okA);
		ref_ctn(cdB, cur.noise, ucB, alB, dminB, lastB, okB);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("the transcripts agree (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0 ? 1 : 0,
			    1, trial);
		diff_eq_obj("the mapping parameters, loud", V90MappingParams,
			    &mpA, &mpB, trial);
		diff_eq_int("dMin, loud (trial %ld)", cdA->short_0a,
			    cdB->short_0a, trial);
		diff_eq_obj("the three pdsnr thresholds, loud", float[3],
			    &cdA->float_18, &cdB->float_18, trial);

		if (dsplib_debug_capture_lines(1) > 0)
			seenPrinted = 1;

		/* Everything below reads the BLOB's transcript, never ours. */
		t = dsplib_debug_capture_text(1);
		if (level > 1) {
			for (i = 0; i < 12; i++)
				if (ctn_count(t, ctn_sites[i]) > 0)
					ctn_seen[i]++;
			if (strstr(t, ": noiseEnergy = +") != NULL)
				seenSign[1]++;
			if (strstr(t, ": noiseEnergy = -") != NULL)
				seenSign[0]++;
			seenClamp[strstr(t, "adjusting dMin for rate>=53k")
				  != NULL ? 1 : 0]++;
			/*
			 * The two `edprintf` runs of encoded text bracket the
			 * report, so a transcript that has the banner has the
			 * `$!$ ` frames too -- which is the only visible sign
			 * that the six ungated sites ran at all.
			 */
			diff_eq_int("the encoded channel spoke (trial %ld)",
				    ctn_count(t, "$!$ ") >= 5 ? 1 : 0, 1,
				    trial);
		}

		restore_and_check_this(trial);
	}

	return diff_end();
}

/*
 * ===========================================================================
 * The 53k clamp, one below, at, and one above each of its two bounds
 * ===========================================================================
 *
 * The window is `0x3e < dMin <= 0x43` and `word_24 == 0`, and dMin at that
 * point is `(short)(noiseEnergy * 6.7762098f + 9.9f)`.  So the sweep drives
 * the NOISE and reads the dMin the blob computed back out of its own
 * transcript, rather than assuming the arithmetic -- which is what makes this
 * a test of the boundary and not of our own copy of the formula.
 */
static int
run_ctn_clamp(void)
{
	int trial;
	int seenBelow = 0;
	int seenAt3e = 0;
	int seenInside = 0;
	int seenInsideOpen = 0;
	int seenAt43 = 0;
	int seenAbove = 0;
	int seenQuiet = 0;

	diff_begin("setConstellationToNoise's 53k clamp at its boundaries");
	wire();

	for (trial = 0; trial < 260; trial++) {
		int step = trial % 130;
		const char *t;
		long got;

		ctn_fixture(trial + 3000);
		cur.w48 = 0;
		cur.restricted = 0;
		cur.w24 = (trial >= 130) ? 1 : 0;
		cur.forced = -1;
		cur.noise = 7.40f + 0.01f * (float)step;
		parA->unnamed_360 = cur.start;
		parA->USE_RESTRICED_DMIN = 0;
		parA->FORCED_DMIN = -1;
		parB->unnamed_360 = parA->unnamed_360;
		parB->USE_RESTRICED_DMIN = 0;
		parB->FORCED_DMIN = -1;
		cdA->word_48 = 0;
		cdB->word_48 = 0;
		cdA->word_24 = (unsigned int)cur.w24;
		cdB->word_24 = (unsigned int)cur.w24;
		snap_this();

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;

		our_ctn(cdA, cur.noise, ucA, alA, dminA, lastA, okA);
		ref_ctn(cdB, cur.noise, ucB, alB, dminB, lastB, okB);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("the transcripts agree at the clamp (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0 ? 1 : 0,
			    1, trial);
		diff_eq_int("clamped dMin (trial %ld)", cdA->short_0a,
			    cdB->short_0a, trial);
		diff_eq_obj("the mapping parameters at the clamp",
			    V90MappingParams, &mpA, &mpB, trial);

		/*
		 * THE ORIGINAL dMin COMES OUT OF THE BLOB'S OWN TRANSCRIPT,
		 * not out of our arithmetic: "current dMin = %d" prints
		 * `short_0a` before the switch runs, so it is what the
		 * boundary has to be classified against.  Reading the field
		 * afterwards would classify the CLAMPED value, and 0x43 would
		 * then never be seen at all -- which is exactly what this
		 * block reported before it was written this way.
		 */
		t = dsplib_debug_capture_text(1);
		got = ctn_num(t, "V90ConstellationDesigner: current dMin = ",
			      -30000);
		diff_eq_int("the blob announced its dMin (trial %ld)",
			    got != -30000, 1, trial);
		if (got < 0x3e)
			seenBelow++;
		else if (got == 0x3e)
			seenAt3e++;
		else if (got < 0x43)
			seenInsideOpen++;
		else if (got == 0x43)
			seenAt43++;
		else
			seenAbove++;

		if (cur.w24 == 0 && got > 0x3e && got <= 0x43) {
			seenInside++;
			diff_eq_int("the clamp announced itself (%ld)",
				    strstr(t, "adjusting dMin for rate>=53k")
				    != NULL, 1, got);
			diff_eq_int("the clamp answers 0x3e (%ld)",
				    (long)cdB->short_0a, 0x3e, got);
		} else {
			diff_eq_int("the clamp stayed quiet (%ld)",
				    strstr(t, "adjusting dMin for rate>=53k")
				    == NULL, 1, got);
			diff_eq_int("dMin is what the switch left (%ld)",
				    (long)cdB->short_0a, got, trial);
			seenQuiet++;
		}

		restore_and_check_this(trial);
	}

	diff_eq_int("dMin landed below the window %ld times", seenBelow > 0, 1,
		    seenBelow);
	diff_eq_int("dMin landed on 0x3e %ld times", seenAt3e > 0, 1, seenAt3e);
	diff_eq_int("dMin landed strictly inside %ld times",
		    seenInsideOpen > 0, 1, seenInsideOpen);
	diff_eq_int("the clamp fired %ld times", seenInside > 0, 1, seenInside);
	diff_eq_int("the clamp did not fire %ld times", seenQuiet > 0, 1,
		    seenQuiet);
	diff_eq_int("dMin landed on 0x43 %ld times", seenAt43 > 0, 1, seenAt43);
	diff_eq_int("dMin landed above the window %ld times", seenAbove > 0, 1,
		    seenAbove);
	seenClamp[1] += seenInside;
	seenClamp[0] += seenQuiet;

	return diff_end();
}

/*
 * ===========================================================================
 * The negative odd threshold seed
 * ===========================================================================
 *
 * `constelBuild` opens with `sar $1` and this member opens with
 * `shr $0x1f; lea; sar $1`, so one is `>> 1` and the other is `/ 2`.  The two
 * agree everywhere except on a NEGATIVE ODD seed, where `-3 >> 1` is -2 and
 * `-3 / 2` is -1.  Nothing in the broad sweep above reaches that: the seed is
 * `short_10`, which the function itself sets to `(short)(dMin * 1.25f)`, and
 * dMin is positive over most of the noise range.
 *
 * The KeepRate arm is the way in.  With `word_48 == 1` the function restores
 * the dMin it was handed, so the seed is whatever this fixture puts at +0x0a
 * -- and `(short)(-3 * 1.25f)` truncates toward zero to -3, which is negative
 * and odd in both loops at once.  The tables are then filled with small
 * values straddling -2 and -1 so the two readings admit different entries.
 */
static int
run_ctn_negodd(void)
{
	int trial;
	int k;
	int j;
	int seenCount = 0;

	diff_begin("setConstellationToNoise's threshold seed is divided");
	wire();

	for (trial = 0; trial < 240; trial++) {
		short seed = (short)(-(1 + (trial % 24)));

		ctn_fixture(trial + 6000);
		cur.w48 = 1;
		cur.forced = -1;
		cur.dmin0a = seed;
		parA->FORCED_DMIN = -1;
		parB->FORCED_DMIN = -1;
		parA->unnamed_360 = 0;
		parB->unnamed_360 = 0;
		cdA->word_48 = 1;
		cdB->word_48 = 1;
		cdA->short_0a = seed;
		cdB->short_0a = seed;

		/*
		 * Small values around the two readings of the seed, so an
		 * entry can clear -1 and not -2 (or the other way).  The
		 * flags are all set: nothing else must be allowed to decide
		 * which entries are counted.
		 */
		reseed(0x77a1u + 29u * (unsigned)trial);
		for (k = 0; k < 7; k++)
			for (j = 0; j < 128; j++) {
				short u = (short)((int)(nextrand() % 13u) - 6);
				short a = (short)((int)(nextrand() % 13u) - 6);

				ucA[k][j] = u;
				ucB[k][j] = u;
				alA[k][j] = a;
				alB[k][j] = a;
				okA[k][j] = 1;
				okB[k][j] = 1;
			}
		for (k = 0; k < 6; k++) {
			lastA[k] = (unsigned char)(20 + (trial % 100));
			lastB[k] = lastA[k];
			dminA[k] = (short)((k + trial) & 1);
			dminB[k] = dminA[k];
		}
		snap_this();

		our_ctn(cdA, cur.noise, ucA, alA, dminA, lastA, okA);
		ref_ctn(cdB, cur.noise, ucB, alB, dminB, lastB, okB);

		diff_eq_obj("the mapping parameters, negative seed",
			    V90MappingParams, &mpA, &mpB, trial);
		diff_eq_int("short_10 off a negative dMin (trial %ld)",
			    cdA->short_10, cdB->short_10, trial);

		if (cdB->short_10 < 0 && (cdB->short_10 & 1) != 0)
			seenNegOddSeed++;
		for (k = 0; k < 6; k++)
			if (mpB.constellationSize[k] != 0)
				seenCount++;

		restore_and_check_this(trial);
	}

	diff_eq_int("a negative ODD seed was reached %ld times",
		    seenNegOddSeed > 0, 1, seenNegOddSeed);
	diff_eq_int("something was counted under it %ld times", seenCount > 0,
		    1, seenCount);

	return diff_end();
}

/*
 * ===========================================================================
 * The edges: an empty build, a span that runs past 127, a start past the end
 * ===========================================================================
 */
static int
run_ctn_edges(void)
{
	int trial;
	int k;
	int j;
	int seenEmpty = 0;
	int seenStartPastEnd = 0;

	diff_begin("setConstellationToNoise at its edges");
	wire();

	for (trial = 0; trial < 96; trial++) {
		int mode = trial % 4;

		ctn_fixture(trial + 9000);
		reseed(0x5151u + 71u * (unsigned)trial);
		for (k = 0; k < 7; k++)
			for (j = 0; j < 128; j++) {
				short u = (short)(j * 8 + (int)(nextrand() % 8u));

				ucA[k][j] = u;
				ucB[k][j] = u;
				alA[k][j] = (short)(u + 3);
				alB[k][j] = alA[k][j];
				okA[k][j] = (unsigned char)(mode == 1 ? 0 : 1);
				okB[k][j] = okA[k][j];
			}

		switch (mode) {
		case 0:
			/* i runs from 130 to 249, entirely past row 0's end. */
			parA->unnamed_360 = 130;
			for (k = 0; k < 6; k++)
				lastA[k] = 249;
			break;
		case 1:
			/* Every flag zero: six empty constellations. */
			parA->unnamed_360 = 0;
			for (k = 0; k < 6; k++)
				lastA[k] = 100;
			break;
		case 2:
			/* The bound below the start: the loop never runs. */
			parA->unnamed_360 = 200;
			for (k = 0; k < 6; k++)
				lastA[k] = (unsigned char)(k * 7);
			seenStartPastEnd++;
			break;
		default:
			/* A negative start, read as a huge unsigned. */
			parA->unnamed_360 = -4;
			for (k = 0; k < 6; k++)
				lastA[k] = 60;
			break;
		}
		parB->unnamed_360 = parA->unnamed_360;
		for (k = 0; k < 6; k++) {
			lastB[k] = lastA[k];
			dminA[k] = (short)((k + trial) & 1);
			dminB[k] = dminA[k];
		}
		snap_this();

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;

		our_ctn(cdA, cur.noise, ucA, alA, dminA, lastA, okA);
		ref_ctn(cdB, cur.noise, ucB, alB, dminB, lastB, okB);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("the transcripts agree at the edges (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0 ? 1 : 0,
			    1, trial);
		diff_eq_obj("the mapping parameters at the edges",
			    V90MappingParams, &mpA, &mpB, trial);
		diff_eq_obj("the ucode table at the edges", short[7][128],
			    ucA, ucB, trial);

		{
			unsigned int m = 0;

			for (k = 0; k < 6; k++)
				if (mpB.constellationSize[k] > m)
					m = mpB.constellationSize[k];
			if (m == 0) {
				seenEmpty++;
				/*
				 * AN EMPTY BUILD SKIPS THE PER-UCODE LINE
				 * ENTIRELY, and that is the arm at 0x49304.
				 */
				diff_eq_int("an empty build prints no ucode"
					    " line (%ld)",
					    strstr(dsplib_debug_capture_text(1),
						   "ucode[") == NULL ? 1 : 0,
					    1, trial);
			}
			seenMaxM[m != 0 ? 1 : 0]++;
		}

		restore_and_check_this(trial);
	}

	diff_eq_int("an empty build happened %ld times", seenEmpty > 0, 1,
		    seenEmpty);
	diff_eq_int("the bound below the start happened %ld times",
		    seenStartPastEnd > 0, 1, seenStartPastEnd);

	return diff_end();
}

/*
 * ===========================================================================
 * Every outcome, not every comparison (findings 149, 223, 224)
 * ===========================================================================
 */
static int
run_ctn_outcomes(void)
{
	static const char *const armname[6] = {
		"KeepRate", "OneRateUp", "OneRateDown", "NoRestriction",
		"a word_48 of 4 or more", "a negative word_48"
	};
	int i;

	diff_begin("setConstellationToNoise reached every arm");

	diff_eq_int("the blob printed something (%ld)", seenPrinted, 1, 0);
	for (i = 0; i < 12; i++)
		diff_eq_int("diagnostic %ld fired", ctn_seen[i] > 0, 1, i);
	for (i = 0; i < 6; i++) {
		(void)armname[i];
		diff_eq_int("switch arm %ld was taken", seenArm[i] > 0, 1, i);
	}

	diff_eq_int("USE_RESTRICED_DMIN was zero %ld times",
		    seenRestricted[0] > 0, 1, seenRestricted[0]);
	diff_eq_int("USE_RESTRICED_DMIN was non-zero %ld times",
		    seenRestricted[1] > 0, 1, seenRestricted[1]);
	diff_eq_int("the sign printer chose '-' %ld times", seenSign[0] > 0, 1,
		    seenSign[0]);
	diff_eq_int("the sign printer chose '+' %ld times", seenSign[1] > 0, 1,
		    seenSign[1]);
	diff_eq_int("dMin was not forced %ld times", seenForced[0] > 0, 1,
		    seenForced[0]);
	diff_eq_int("dMin was forced %ld times", seenForced[1] > 0, 1,
		    seenForced[1]);
	diff_eq_int("a forced dMin needed truncating %ld times",
		    seenForcedTrunc > 0, 1, seenForcedTrunc);
	diff_eq_int("the zero-dmin inner loop ran %ld times",
		    seenDminArm[0] > 0, 1, seenDminArm[0]);
	diff_eq_int("the non-zero-dmin inner loop ran %ld times",
		    seenDminArm[1] > 0, 1, seenDminArm[1]);
	diff_eq_int("the u-law arm ran %ld times", seenLaw[0] > 0, 1,
		    seenLaw[0]);
	diff_eq_int("the A-law arm ran %ld times", seenLaw[1] > 0, 1,
		    seenLaw[1]);
	diff_eq_int("word_2c differed from word_28 %ld times", seenEq[0] > 0, 1,
		    seenEq[0]);
	diff_eq_int("word_2c equalled word_28 %ld times", seenEq[1] > 0, 1,
		    seenEq[1]);
	diff_eq_int("the min took the constellation byte %ld times",
		    seenMinTakes[0] > 0, 1, seenMinTakes[0]);
	diff_eq_int("the min took the codec byte %ld times",
		    seenMinTakes[1] > 0, 1, seenMinTakes[1]);
	diff_eq_int("the 53k clamp did not fire %ld times", seenClamp[0] > 0, 1,
		    seenClamp[0]);
	diff_eq_int("the 53k clamp fired %ld times", seenClamp[1] > 0, 1,
		    seenClamp[1]);
	diff_eq_int("the report had nothing to print %ld times",
		    seenMaxM[0] > 0, 1, seenMaxM[0]);
	diff_eq_int("the report printed ucode lines %ld times",
		    seenMaxM[1] > 0, 1, seenMaxM[1]);
	diff_eq_int("rrnUpDmin was zero %ld times", seenUpArm[0] > 0, 1,
		    seenUpArm[0]);
	diff_eq_int("rrnUpDmin was above dMin %ld times", seenUpArm[1] > 0, 1,
		    seenUpArm[1]);
	diff_eq_int("rrnUpDmin was at or below dMin %ld times",
		    seenUpArm[2] > 0, 1, seenUpArm[2]);
	diff_eq_int("rrnDownDmin was zero %ld times", seenDownArm[0] > 0, 1,
		    seenDownArm[0]);
	diff_eq_int("rrnDownDmin was below dMin %ld times", seenDownArm[1] > 0,
		    1, seenDownArm[1]);
	diff_eq_int("rrnDownDmin was at or above dMin %ld times",
		    seenDownArm[2] > 0, 1, seenDownArm[2]);
	diff_eq_int("a constellation came out empty %ld times",
		    seenPicked[0] > 0, 1, seenPicked[0]);
	diff_eq_int("a constellation came out non-empty %ld times",
		    seenPicked[1] > 0, 1, seenPicked[1]);
	diff_eq_int("an accepted index passed 127 %ld times", seenIndexHigh > 0,
		    1, seenIndexHigh);
	diff_eq_int("a negative odd threshold seed was driven %ld times",
		    seenNegOddSeed > 0, 1, seenNegOddSeed);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_ctn_quiet();
	rc |= run_ctn_loud();
	rc |= run_ctn_clamp();
	rc |= run_ctn_negodd();
	rc |= run_ctn_edges();
	rc |= run_ctn_outcomes();

	return rc;
}
