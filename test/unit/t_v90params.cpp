/*
 * t_v90params.cpp -- differential test of V90Parameters and V92Parameters.
 *
 * TWO ZEROED BUFFERS AGREE ABOUT EVERYTHING NEITHER SIDE WROTE, and this
 * tree has fallen into that three times.  `V90Parameters::setToDefault`
 * writes 339 of the class's 342 words and fifty-one of them are fields
 * `loadParams` never reads, so a zero fill would make "wrote the default 0"
 * and "did not write" the same observation.  Both objects are therefore
 * filled with a POISON that is a function of the offset -- `0xa5000000 |
 * offset` -- before every call.  It is per-word distinct, so a store that
 * lands at the wrong offset shows up rather than cancelling out, and the high
 * byte 0xa5 appears in none of the 394 default values in either class (a
 * float 0xa5xxxxxx is about -2.4e-16, an int about -1.5e9; neither is
 * anywhere near a parameter).
 *
 * SO THE TEST COUNTS WHAT IT LOOKED AT AND WHAT CHANGED.  Comparing the two
 * objects is necessary and, on its own, weak: it cannot distinguish a
 * `setToDefault` that writes every field from one that writes none, if both
 * sides do the same thing -- and both sides are our own reading of the same
 * disassembly.  So after each call the test also counts the words that no
 * longer hold their poison and requires that number to be exactly 339, or 338
 * on the arm where `SILENCE_SCR` is not written.  That number is the store
 * map's own claim (finding F861: 340 stores over 339 distinct offsets), and it
 * is checked against a literal, not against the blob.  gates.md rule 1.
 *
 * THE MATRIX EXISTS BECAUSE `setToDefault` READS FOUR THINGS.  It is not a
 * table of constants: it dereferences `modemParams` for the two rate limits
 * and for one flag bit, and it reads `SENSITIVE_ISP_DETECTED` and
 * `MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP`.  One call with one fixture proves
 * about half the function.  The sweep drives, deliberately:
 *
 *   - rate indices below 2, above 14, and in range, on each side
 *     independently, so both clamps are exercised in both directions;
 *   - min > max, which is the only path to the `bad upstream rate` site and
 *     the only place `imul $0x960` is applied to the forced defaults;
 *   - the sensitive-ISP cap both engaged and not, which gates BOTH the
 *     `+0x4fc` clamp and the conditional `SILENCE_SCR` store;
 *   - `sessionFlags` bit 0 both ways for the 2400/7740 select, and with
 *     other bits set, so "reads the byte" and "reads bit 0" are distinguished.
 *
 * THE DEBUG LEVEL IS SWEPT 0 TO 3, finding F150's rule.  `setToDefault`'s two
 * sites are gated `> 1` and `loadModemParamsData`'s four `edprintf` calls are
 * not gated at all -- `edprintf` formats and encodes unconditionally and only
 * its handoff to `dsplibs_debug_printf` is behind the level (src/core/
 * encode.c).  A site moved to the wrong side of that distinction is invisible
 * at a fixed level.  Comparing the two sides' `edprintf` output is safe for
 * t_v90p2info's reason: `iEncodeOffset` is reset at the top of every
 * successful call, so the key does not carry between calls and each side's
 * own copy of the encoder produces the same line.
 *
 * `loadModemParamsData` IS WHERE THE ARITHMETIC HIDES.  It stores exactly
 * four fields, and the `%c%d.%02d` power-reduction line -- the sign from
 * `fldz; fcomps`, the whole part, the two fraction digits -- lands in none of
 * them.  Get that wrong and all four store assertions still pass.  The
 * transcript is the only oracle for it and that is why it is compared
 * character for character.
 *
 * BOTH ARMS OF THE `loadParams` BRANCH ARE DRIVEN, which is the measured form
 * of the reason `loadParams` is not reconstructed.  The object's `init()` and
 * constructor call it when `modemParams->paramFile` is non-null; this tree
 * does not, on the argument that its 295 reads all go to `xor %eax,%eax;
 * ret`.  Running both sides with the pointer null AND with it pointing at a
 * real string turns that argument into a comparison: if `loadParams` changed
 * anything the blob would move and we would not.  Finding F879.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/modem_params.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V92Parameters.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_v90_setToDefault(void *self)
	asm("ref__ZN13V90Parameters12setToDefaultEv");
void ref_v90_loadModemParamsData(void *self)
	asm("ref__ZN13V90Parameters19loadModemParamsDataEv");
void ref_v90_init(void *self)
	asm("ref__ZN13V90Parameters4initEv");
void ref_v90_initSession(void *self)
	asm("ref__ZN13V90Parameters11initSessionEv");
void ref_v90_ctor1(void *self, void *mp)
	asm("ref__ZN13V90ParametersC1EP19_tagModemParameters");
void ref_v90_ctor2(void *self, void *mp)
	asm("ref__ZN13V90ParametersC2EP19_tagModemParameters");
void ref_v90_dtor1(void *self) asm("ref__ZN13V90ParametersD1Ev");
void ref_v90_dtor2(void *self) asm("ref__ZN13V90ParametersD2Ev");

void our_v90_ctor1(void *self, void *mp)
	asm("_ZN13V90ParametersC1EP19_tagModemParameters");
void our_v90_ctor2(void *self, void *mp)
	asm("_ZN13V90ParametersC2EP19_tagModemParameters");
void our_v90_dtor1(void *self) asm("_ZN13V90ParametersD1Ev");
void our_v90_dtor2(void *self) asm("_ZN13V90ParametersD2Ev");

void ref_v92_setToDefault(void *self)
	asm("ref__ZN13V92Parameters12setToDefaultEv");
void ref_v92_init(void *self) asm("ref__ZN13V92Parameters4initEv");
void ref_v92_ctor1(void *self, void *mp)
	asm("ref__ZN13V92ParametersC1EP19_tagModemParameters");
void ref_v92_ctor2(void *self, void *mp)
	asm("ref__ZN13V92ParametersC2EP19_tagModemParameters");
void ref_v92_dtor1(void *self) asm("ref__ZN13V92ParametersD1Ev");
void ref_v92_dtor2(void *self) asm("ref__ZN13V92ParametersD2Ev");

void our_v92_ctor1(void *self, void *mp)
	asm("_ZN13V92ParametersC1EP19_tagModemParameters");
void our_v92_ctor2(void *self, void *mp)
	asm("_ZN13V92ParametersC2EP19_tagModemParameters");
void our_v92_dtor1(void *self) asm("_ZN13V92ParametersD1Ev");
void our_v92_dtor2(void *self) asm("_ZN13V92ParametersD2Ev");
}

/*
 * The sizes, asserted rather than assumed.  Finding F861 measures both three
 * ways -- the field span, `setToDefault`'s largest displacement and the
 * `sysdep_malloc` immediately before each constructor -- and this is where
 * that stops being prose.  32-bit only: the class holds a pointer, so a
 * 64-bit build lays it out differently and `make check64` does not compile
 * this file anyway.
 */
#if __SIZEOF_POINTER__ == 4
typedef char v90_size_check[(sizeof(V90Parameters) == 0x558) ? 1 : -1];
typedef char v92_size_check[(sizeof(V92Parameters) == 0xdc) ? 1 : -1];
#endif

#define V90_WORDS	(0x558 / 4)		/* 342 */
#define V92_WORDS	(0xdc / 4)		/* 55 */

/* Room past each object, to catch a store that overruns it. */
#define GUARD		64

/*
 * `setToDefault`'s own claim about itself: 340 stores over 339 distinct
 * offsets, of which `SILENCE_SCR` is the one that is conditional.
 */
#define V90_STORES		339
#define V90_STORES_NO_SCR	(V90_STORES - 1)
#define V92_STORES		54

static unsigned char ours_raw[0x558 + GUARD] __attribute__((aligned(16)));
static unsigned char theirs_raw[0x558 + GUARD] __attribute__((aligned(16)));

#define OURS	((V90Parameters *)(void *)ours_raw)
#define THEIRS	((V90Parameters *)(void *)theirs_raw)
#define OURS92	((V92Parameters *)(void *)ours_raw)
#define THEIRS92 ((V92Parameters *)(void *)theirs_raw)

/*
 * ONE shared modem block, not two.  Both sides only read it, so pointing both
 * at the same one keeps the pointer word at +0x000 comparable -- two
 * identical copies at two addresses would differ there for ever and the
 * comparison would have to skip the field that proves the constructor stored
 * its argument.
 */
static struct _tagModemParameters mp;

/* A file name for the arm the object would hand to `loadParams`. */
static char param_file[] = "v90.par";

static void
poison(unsigned bytes)
{
	unsigned i;

	memset(ours_raw, 0, sizeof ours_raw);
	memset(theirs_raw, 0, sizeof theirs_raw);
	for (i = 0; i + 4 <= bytes; i += 4) {
		unsigned w = 0xa5000000u | i;

		memcpy(ours_raw + i, &w, 4);
		memcpy(theirs_raw + i, &w, 4);
	}
	for (i = bytes; i < bytes + GUARD; i++)
		ours_raw[i] = theirs_raw[i] = (unsigned char)(0x5a + (i & 7));
}

/*
 * How many words no longer hold their poison, and how many were examined.
 * `skip_from`/`skip_to` names the half-open span the fixture wrote itself.
 */
static unsigned examined;

static unsigned
changed_words(const unsigned char *p, unsigned bytes,
	      unsigned skip_from, unsigned skip_to)
{
	unsigned i, n = 0;

	examined = 0;
	for (i = 0; i + 4 <= bytes; i += 4) {
		unsigned w, want = 0xa5000000u | i;

		if (i >= skip_from && i < skip_to)
			continue;
		examined++;
		memcpy(&w, p + i, 4);
		if (w != want)
			n++;
	}
	return n;
}

static void
check_guard(unsigned bytes, long tag)
{
	unsigned i;
	int ok = 1;

	for (i = bytes; i < bytes + GUARD; i++)
		if (ours_raw[i] != (unsigned char)(0x5a + (i & 7))
		    || theirs_raw[i] != (unsigned char)(0x5a + (i & 7)))
			ok = 0;
	diff_eq_int("nothing stored past the object (case %ld)", ok, 1, tag);
}

/* ------------------------------------------------------- V90 setToDefault */

struct v90_case {
	unsigned int	minRate;
	unsigned int	maxRate;
	unsigned char	sessionFlags;
	int		sensitiveIsp;
	int		maxTxRateIndex;
};

/*
 * Rates chosen for what they do to the index, not for being realistic: 0 and
 * 2399 land below 2, 33600 lands exactly on 14, 36000 and 200000 land above
 * it, and the pairs that cross put min above max.
 */
static const struct v90_case v90_cases[] = {
	{  4800,  33600, 0, 0,  14 },	/* the ordinary window */
	{  4800,  33600, 1, 0,  14 },	/* the other ANSPCM length */
	{  4800,  33600, 0xfe, 0, 14 },	/* bit 0 clear, others set */
	{  4800,  33600, 0xff, 0, 14 },	/* bit 0 set, others set */
	{     0,  33600, 0, 0,  14 },	/* min index 0, clamps up to 2 */
	{  2399,  33600, 0, 0,  14 },	/* min index 0 again, not a multiple */
	{  2400,  33600, 0, 0,  14 },	/* min index 1, still clamps to 2 */
	{  4800,      0, 0, 0,  14 },	/* max index 0 -> 2, and min > max */
	{  4800,   2400, 0, 0,  14 },	/* max index 1 -> 2, and min > max */
	{ 36000,  33600, 0, 0,  14 },	/* min index 15 -> 14 */
	{ 200000, 33600, 0, 0,  14 },	/* min index 83 -> 14, min > max */
	{  4800, 200000, 0, 0,  14 },	/* max index 83 -> 14 */
	{  4800,  36000, 0, 0,  14 },	/* max index 15 -> 14 */
	{ 33600,  33600, 0, 0,  14 },	/* one rate only */
	{ 33600,   4800, 0, 0,  14 },	/* min > max, both in range */
	{  4800,  33600, 0, 1,   2 },	/* cap bites, down to the bottom */
	{  4800,  33600, 0, 1,   7 },	/* cap bites, mid */
	{  4800,  33600, 0, 1,  14 },	/* cap does not bite */
	{  4800,  33600, 0, 1,   0 },	/* cap below the clamp floor */
	{  4800,  33600, 0, 1,  99 },	/* cap above everything */
	{  4800,  33600, 0, -1, 3 },	/* non-zero and negative */
	{ 33600,   4800, 0, 1,   2 },	/* the cap and the crossed window */
	{  9600,  28800, 1, 0,  14 },
	{  2400,   2400, 1, 1,   1 },
};

#define NV90CASES ((int)(sizeof v90_cases / sizeof v90_cases[0]))

static int
run_v90_setToDefault(void)
{
	int c, lvl;
	int printed = 0, ref_printed = 0, crossed = 0;

	diff_begin("V90Parameters::setToDefault, levels 0..3");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl < 4; lvl++)
	for (c = 0; c < NV90CASES; c++) {
		const struct v90_case *k = &v90_cases[c];
		long tag = lvl * 1000 + c;
		unsigned n;

		dsplibs_debug_level = ref_dsplibs_debug_level =
		    (unsigned int)lvl;

		mp.minRate = k->minRate;
		mp.maxRate = k->maxRate;
		mp.sessionFlags = k->sessionFlags;

		poison(0x558);
		OURS->modemParams = THEIRS->modemParams = &mp;
		OURS->SENSITIVE_ISP_DETECTED =
		    THEIRS->SENSITIVE_ISP_DETECTED = k->sensitiveIsp;
		OURS->MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP =
		    THEIRS->MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP =
		    k->maxTxRateIndex;

		dsplib_debug_capture_reset();
		OURS->setToDefault();
		ref_v90_setToDefault(theirs_raw);

		diff_eq_obj("after setToDefault", V90Parameters,
			    OURS, THEIRS, tag);
		check_guard(0x558, tag);

		/*
		 * The fixture wrote +0x000 and the two words at +0x4f8 and
		 * +0x4fc itself, so those three are not the function's.
		 */
		n = changed_words(ours_raw, 0x558, 0x4f8, 0x500);
		n -= 1;					/* +0x000 */
		diff_eq_int("words examined (case %ld)", (long)examined,
			    (long)(V90_WORDS - 2), tag);
		diff_eq_int("fields setToDefault wrote (case %ld)", (long)n,
			    (long)(k->sensitiveIsp == 0 ? V90_STORES
						        : V90_STORES_NO_SCR),
			    tag);

		/*
		 * SILENCE_SCR is the whole reason the fill is not zero: on
		 * one arm it is written and on the other it keeps what the
		 * allocation left.
		 */
		diff_eq_int("SILENCE_SCR on the untouched arm (case %ld)",
			    (long)(k->sensitiveIsp != 0
				   ? (unsigned)OURS->SILENCE_SCR
					== (0xa5000000u | 0x364)
				   : OURS->SILENCE_SCR == 1),
			    1, tag);

		diff_eq_int("transcript line count (case %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), tag);
		diff_eq_int("transcript text (case %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);

		printed += (int)dsplib_debug_capture_lines(0);
		ref_printed += (int)dsplib_debug_capture_lines(1);
		if (lvl > 1 && dsplib_debug_capture_lines(0) == 2)
			crossed++;
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	/* Finding F149: two silent sides agree about nothing. */
	diff_eq_int("our gated sites printed something", printed > 0, 1, 0);
	diff_eq_int("the blob's gated sites printed something",
		    ref_printed > 0, 1, 0);
	/*
	 * And the second site is only reached when min > max, so a sweep that
	 * never crossed the two rates would leave `bad upstream rate` dead
	 * while still printing plenty.
	 */
	diff_eq_int("the bad-upstream-rate site was reached", crossed > 0, 1,
		    0);

	return diff_end();
}

/* ------------------------------------------------ V90 loadModemParamsData */

struct lmp_case {
	unsigned int	powerReductionTenths;
	unsigned char	modeFlags;
	int		connectionType;
	int		lineConnectionType;
};

/*
 * `powerReductionTenths / 10 * 0.5` is the printed number, so the values are
 * chosen around the division: below 10 gives zero and prints `-0.00`, and 10,
 * 15, 25 and 35 straddle the half that makes the two fraction digits 50.
 */
static const struct lmp_case lmp_cases[] = {
	{      0, 0, 0,  -1 },		/* the whole block is skipped */
	{      1, 0, 0,  -1 },
	{      9, 1, 1,  -1 },
	{     10, 2, 2,  -1 },
	{     15, 3, 3,  -1 },
	{     20, 0, -1, -1 },
	{     25, 1, 7,  -1 },
	{     35, 2, 7,   0 },		/* connection type already set */
	{    100, 3, 9,   5 },
	{    123, 0, -2, -1 },
	{   1234, 1, 100, -1 },
	{  40000, 2, 0,  -1 },
	{ 100000, 3, 0,  -1 },
	{ 0x7fffffffu, 0, 0, -1 },
	{ 0x80000000u, 1, 0, -1 },	/* the unsigned divide's top half */
	{ 0xffffffffu, 2, 0, -1 },
	{      5, 0xfc, 0, -1 },	/* neither low bit set */
	{      5, 0xfd, 0, -1 },	/* bit 0 only */
	{      5, 0xfe, 0, -1 },	/* bit 1 only */
	{      5, 0xff, 0, -1 },	/* both */
};

#define NLMPCASES ((int)(sizeof lmp_cases / sizeof lmp_cases[0]))

static int
run_v90_loadModemParamsData(void)
{
	int c, lvl, printed = 0, ref_printed = 0;

	diff_begin("V90Parameters::loadModemParamsData, levels 0..3");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl < 4; lvl++)
	for (c = 0; c < NLMPCASES; c++) {
		const struct lmp_case *k = &lmp_cases[c];
		long tag = lvl * 1000 + c;
		unsigned n;

		dsplibs_debug_level = ref_dsplibs_debug_level =
		    (unsigned int)lvl;

		mp.powerReductionTenths = k->powerReductionTenths;
		mp.modeFlags = k->modeFlags;
		mp.connectionType = k->connectionType;

		poison(0x558);
		OURS->modemParams = THEIRS->modemParams = &mp;
		OURS->LINE_CONNECTION_TYPE = THEIRS->LINE_CONNECTION_TYPE =
		    k->lineConnectionType;

		dsplib_debug_capture_reset();
		OURS->loadModemParamsData();
		ref_v90_loadModemParamsData(theirs_raw);

		diff_eq_obj("after loadModemParamsData", V90Parameters,
			    OURS, THEIRS, tag);
		check_guard(0x558, tag);

		/*
		 * At most four fields: +0x004, +0x00c, +0x380 and +0x420.
		 * The fixture wrote +0x000 and +0x00c, so the count runs
		 * over everything else and the ceiling is three.
		 */
		n = changed_words(ours_raw, 0x558, 0x00c, 0x010);
		n -= 1;					/* +0x000 */
		diff_eq_int("words examined (case %ld)", (long)examined,
			    (long)(V90_WORDS - 1), tag);
		diff_eq_int("loadModemParamsData wrote at most three "
			    "words (case %ld)", (long)(n <= 3), 1, tag);
		diff_eq_int("it wrote DIGITAL_POWER_REDUCTION iff tempPR "
			    "(case %ld)",
			    (long)((*(unsigned *)(void *)(ours_raw + 0x380)
				    != (0xa5000000u | 0x380))
				   == (k->powerReductionTenths != 0)),
			    1, tag);

		/*
		 * FIVE `edprintf` calls, none of them gated: they run at
		 * every level and only their handoff to
		 * `dsplibs_debug_printf` is behind it.  Four are
		 * unconditional -- tempPR, tempProbe, tempConnectionType and
		 * the trn2d flag -- and the fifth, the power-reduction line,
		 * is inside `if (tempPR)`.  So the count is 4 or 5 at levels
		 * 2 and 3 and 0 below, and it is asserted against those
		 * literals as well as against the blob's, because two silent
		 * sides agree about nothing (finding F149).
		 */
		diff_eq_int("transcript line count (case %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), tag);
		diff_eq_int("five ungated sites, four unconditional "
			    "(case %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)(lvl > 1
				   ? (k->powerReductionTenths != 0 ? 5 : 4)
				   : 0), tag);
		diff_eq_int("transcript text (case %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);

		printed += (int)dsplib_debug_capture_lines(0);
		ref_printed += (int)dsplib_debug_capture_lines(1);
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("our sites printed something", printed > 0, 1, 0);
	diff_eq_int("the blob's sites printed something", ref_printed > 0, 1,
		    0);

	return diff_end();
}

/* ------------------------------------------ the constructors and the rest */

/*
 * `init()` and the constructor, with the parameter-file pointer null and
 * non-null.  The second arm is the one the object would spend 7,894 bytes in.
 */
static int
run_v90_construct(void)
{
	int c, arm, lvl;

	diff_begin("V90Parameters: init, initSession, ctor, dtor");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl < 4; lvl += 2)
	for (arm = 0; arm < 2; arm++)
	for (c = 0; c < NV90CASES; c++) {
		const struct v90_case *k = &v90_cases[c];
		long tag = lvl * 10000 + arm * 1000 + c;

		dsplibs_debug_level = ref_dsplibs_debug_level =
		    (unsigned int)lvl;

		mp.minRate = k->minRate;
		mp.maxRate = k->maxRate;
		mp.sessionFlags = k->sessionFlags;
		mp.powerReductionTenths = 250u + 7u * (unsigned)c;
		mp.modeFlags = (unsigned char)c;
		mp.connectionType = (c & 1) ? -1 : c;
		mp.paramFile = arm ? param_file : (char *)0;

		/* initSession, on its own. */
		poison(0x558);
		OURS->initSession();
		ref_v90_initSession(theirs_raw);
		diff_eq_obj("after initSession", V90Parameters, OURS, THEIRS,
			    tag);
		diff_eq_int("initSession wrote two words (case %ld)",
			    (long)changed_words(ours_raw, 0x558, 0, 0), 2,
			    tag);
		check_guard(0x558, tag);

		/* init(), both arms of the parameter-file branch. */
		poison(0x558);
		OURS->modemParams = THEIRS->modemParams = &mp;
		OURS->SENSITIVE_ISP_DETECTED =
		    THEIRS->SENSITIVE_ISP_DETECTED = k->sensitiveIsp;
		OURS->MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP =
		    THEIRS->MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP =
		    k->maxTxRateIndex;
		dsplib_debug_capture_reset();
		OURS->init();
		ref_v90_init(theirs_raw);
		diff_eq_obj("after init", V90Parameters, OURS, THEIRS, tag);
		diff_eq_int("init transcript (case %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		check_guard(0x558, tag);

		/* The two constructor bodies, C1 and C2. */
		poison(0x558);
		dsplib_debug_capture_reset();
		our_v90_ctor1(ours_raw, &mp);
		ref_v90_ctor1(theirs_raw, &mp);
		diff_eq_obj("after C1", V90Parameters, OURS, THEIRS, tag);
		diff_eq_int("C1 transcript (case %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("C1 set modemParams (case %ld)",
			    OURS->modemParams == &mp, 1, tag);
		check_guard(0x558, tag);

		poison(0x558);
		dsplib_debug_capture_reset();
		our_v90_ctor2(ours_raw, &mp);
		ref_v90_ctor2(theirs_raw, &mp);
		diff_eq_obj("after C2", V90Parameters, OURS, THEIRS, tag);
		check_guard(0x558, tag);

		/* Both destructors: one byte each, and they must do it. */
		our_v90_dtor1(ours_raw);
		ref_v90_dtor1(theirs_raw);
		our_v90_dtor2(ours_raw);
		ref_v90_dtor2(theirs_raw);
		diff_eq_obj("after both destructors", V90Parameters,
			    OURS, THEIRS, tag);
		check_guard(0x558, tag);
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}

/* ------------------------------------------------------------------- V.92 */

static int
run_v92(void)
{
	int c, arm, lvl;

	diff_begin("V92Parameters: setToDefault, init, ctor, dtor");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl < 4; lvl += 2)
	for (arm = 0; arm < 2; arm++)
	for (c = 0; c < NV90CASES; c++) {
		long tag = lvl * 10000 + arm * 1000 + c;

		dsplibs_debug_level = ref_dsplibs_debug_level =
		    (unsigned int)lvl;

		/*
		 * The V.92 block reads NOTHING, so the fixture varies the
		 * modem block anyway: a case that changed the answer would be
		 * a field the disassembly says is not read.
		 */
		mp.minRate = v90_cases[c].minRate;
		mp.maxRate = v90_cases[c].maxRate;
		mp.sessionFlags = (unsigned char)c;
		mp.modeFlags = (unsigned char)(c * 3);
		mp.powerReductionTenths = 17u * (unsigned)c;
		mp.connectionType = c - 4;
		mp.paramFile = arm ? param_file : (char *)0;

		poison(0xdc);
		OURS92->modemParams = THEIRS92->modemParams = &mp;
		dsplib_debug_capture_reset();
		OURS92->setToDefault();
		ref_v92_setToDefault(theirs_raw);
		diff_eq_obj("after setToDefault", V92Parameters,
			    OURS92, THEIRS92, tag);
		diff_eq_int("words examined (case %ld)",
			    (long)(changed_words(ours_raw, 0xdc, 0, 0),
				   examined), (long)V92_WORDS, tag);
		diff_eq_int("fields V92 setToDefault wrote (case %ld)",
			    (long)changed_words(ours_raw, 0xdc, 0, 0) - 1,
			    (long)V92_STORES, tag);
		diff_eq_int("V92 setToDefault said nothing (case %ld)",
			    (long)dsplib_debug_capture_lines(0), 0, tag);
		check_guard(0xdc, tag);

		poison(0xdc);
		OURS92->modemParams = THEIRS92->modemParams = &mp;
		OURS92->init();
		ref_v92_init(theirs_raw);
		diff_eq_obj("after init", V92Parameters, OURS92, THEIRS92,
			    tag);
		check_guard(0xdc, tag);

		poison(0xdc);
		our_v92_ctor1(ours_raw, &mp);
		ref_v92_ctor1(theirs_raw, &mp);
		diff_eq_obj("after C1", V92Parameters, OURS92, THEIRS92, tag);
		diff_eq_int("C1 set modemParams (case %ld)",
			    OURS92->modemParams == &mp, 1, tag);
		check_guard(0xdc, tag);

		poison(0xdc);
		our_v92_ctor2(ours_raw, &mp);
		ref_v92_ctor2(theirs_raw, &mp);
		diff_eq_obj("after C2", V92Parameters, OURS92, THEIRS92, tag);
		check_guard(0xdc, tag);

		our_v92_dtor1(ours_raw);
		ref_v92_dtor1(theirs_raw);
		our_v92_dtor2(ours_raw);
		ref_v92_dtor2(theirs_raw);
		diff_eq_obj("after both destructors", V92Parameters,
			    OURS92, THEIRS92, tag);
		check_guard(0xdc, tag);
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_v90_setToDefault();
	rc |= run_v90_loadModemParamsData();
	rc |= run_v90_construct();
	rc |= run_v92();
	return rc;
}
