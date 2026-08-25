/*
 * t_v90shapereset.cpp -- differential test of the three per-connection resets
 * this batch adds:
 *
 *     V90SpectralShaper::reset(unsigned, unsigned, float, float, float, float)
 *     V90SpectralShaper::resetSSFilter(float, float, float, float)
 *     V90Modulator::reset()
 *
 * 380 bytes at 0x327b0, 0x32880 and 0x1a510.  Each of the three writes an
 * object AND at least one thing outside it -- a heap buffer, an embedded
 * subobject, a differential encoder's state -- so no one of them is testable
 * by comparing the object alone, and every suite below compares the reachable
 * state as well.
 *
 * EVERY POINTER IS A DIFFERENT ADDRESS ON THE TWO SIDES and always will be:
 * three `sysdep_malloc` returns in the shaper and one in the scrambler.  The
 * snapshots replace each with that side's own answer to a question that is
 * the same on both -- "is it null" for a buffer, "how far into your own
 * buffer is it" for the scrambler's six running pointers -- and what the
 * pointers point AT is compared separately and in full.  An offset is the
 * right normalisation for the scrambler because `reset` and
 * `resetHistoryIndexes` move those pointers and a test that dropped them
 * would not see it.
 *
 * `V90Modulator::reset` NEVER DEREFERENCES ANY OF THE ELEVEN POINTERS the
 * constructor stores, so the fixture does not build a modulator: it poisons
 * 0x70 bytes, constructs a real `Scrambler` over +0x44 on each side, and
 * calls the member.  Handing both sides the same poison rather than zeros is
 * what turns "does not read them" from a claim in the header into something
 * this file would notice being wrong -- a member that followed one would
 * fault on both sides rather than quietly reading a zero (finding F230).
 *
 * THE ARGUMENT SWEEP IS BUILT AROUND `6 / shaperSR`.  Zero is in it because
 * the object guards the divide and stores 0 instead (0x3286c); 1, 2, 3 and 6
 * are in it because they divide six exactly and give widths 6, 3, 2 and 1;
 * 4, 5 and 7 are in it because they do not, and a reconstruction that wrote
 * `6 - shaperSR` -- which `V90MappingParams.h` records as a real idiom
 * somewhere else in the object -- agrees with `6 / shaperSR` at 3 and
 * nowhere else.  0xffffffff is in it because the divide is UNSIGNED and a
 * signed one gives 0 where this gives 0 as well but by a different route,
 * so it is carried for the encoder-capacity check rather than for the
 * quotient.
 *
 * THE FOUR FLOATS CROSS AS BIT PATTERNS, for t_v90spectral.cpp's reason: a
 * `float` parameter declared as such is loaded and stored by the CALLER,
 * which rounds a denormal on both sides at once and proves nothing.  All four
 * are DISTINCT in every trial, because `setFilterCoeff` stores them to four
 * consecutive slots and a pair of equal arguments makes a swapped store
 * invisible.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/DiffCoder.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90Modulator.h"
#include "dsplib/V90SpectralShaper.h"
#include "dsplib/V90SpectralShapingFilter.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/* Both sides by asm() label: cdecl, `this` first on the stack (finding F215). */
void our_ss_ctor(void *self) asm("_ZN17V90SpectralShaperC1Ev");
void ref_ss_ctor(void *self) asm("ref__ZN17V90SpectralShaperC1Ev");
void our_ss_dtor(void *self) asm("_ZN17V90SpectralShaperD1Ev");
void ref_ss_dtor(void *self) asm("ref__ZN17V90SpectralShaperD1Ev");

void ref_ss_reset(void *self, unsigned id, unsigned sr, unsigned a1,
		  unsigned a2, unsigned b1, unsigned b2)
	asm("ref__ZN17V90SpectralShaper5resetEjjffff");
void our_ss_reset(void *self, unsigned id, unsigned sr, unsigned a1,
		  unsigned a2, unsigned b1, unsigned b2)
	asm("_ZN17V90SpectralShaper5resetEjjffff");
void ref_ss_resetssf(void *self, unsigned a1, unsigned a2, unsigned b1,
		     unsigned b2)
	asm("ref__ZN17V90SpectralShaper13resetSSFilterEffff");
void our_ss_resetssf(void *self, unsigned a1, unsigned a2, unsigned b1,
		     unsigned b2)
	asm("_ZN17V90SpectralShaper13resetSSFilterEffff");

void our_scr_ctor(void *self, unsigned a, unsigned b, unsigned c)
	asm("_ZN9ScramblerIihEC1Ejjj");
void our_scr_dtor(void *self) asm("_ZN9ScramblerIihED1Ev");
void ref_mod_reset(void *self) asm("ref__ZN12V90Modulator5resetEv");
}

/* ------------------------------------------------------------- the fixture */

#define SS_SLOT		0x90u	/* 0x6c of object, the rest a guard */
#define MOD_SLOT	0x90u	/* 0x70 of object, the rest a guard */
#define SS_BUF		24u	/* entries in each of the two buffers */
#define SCR_TAPS_A	0x12u
#define SCR_TAPS_B	0x17u
#define SCR_TAPS_C	0x63u
#define SCR_ELEMS	(1u + SCR_TAPS_B + SCR_TAPS_C)

union big_slot {
	double align;
	unsigned char raw[SS_SLOT > MOD_SLOT ? SS_SLOT : MOD_SLOT];
};

static union big_slot ss_a, ss_b, mod_a, mod_b;

#define SS_A (*(V90SpectralShaper *)ss_a.raw)
#define SS_B (*(V90SpectralShaper *)ss_b.raw)
#define MOD_A (*(V90Modulator *)mod_a.raw)
#define MOD_B (*(V90Modulator *)mod_b.raw)

typedef Scrambler<int, unsigned char> V90Scrambler;

static unsigned lfsr;

static unsigned char
next_byte(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

/* The same varied bytes into both sides.  Never zeros -- finding F230. */
static void
fill_pair(void *a, void *b, unsigned n, int trial)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	unsigned i;

	lfsr = 0x6d1bu + 0x9e37u * (unsigned)trial;
	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte();
}


/* ============================================ V90SpectralShaper::reset ==== */

/*
 * The three pointers become "is it null" and everything else is compared as
 * it stands; the encoder's `capacity_` and `size_` are ordinary words and are
 * NOT normalised, because `size_` is the one thing `reset` is supposed to
 * move in that subobject.
 */
static void
ss_snapshot(void *dst, const unsigned char *src)
{
	const V90SpectralShaper *s = (const V90SpectralShaper *)src;
	V90SpectralShaper *d = (V90SpectralShaper *)dst;

	memcpy(dst, src, sizeof(V90SpectralShaper));
	d->delayLine = (short *)(long)(s->delayLine != 0);
	d->trialLine = (short *)(long)(s->trialLine != 0);
	d->pde.state_ = (unsigned char *)(long)(s->pde.state_ != 0);
}

struct ss_case {
	unsigned id;
	unsigned sr;
};

static const struct ss_case ss_cases[] = {
	{ 0u, 0u }, { 0u, 1u }, { 1u, 1u }, { 1u, 2u }, { 2u, 2u },
	{ 2u, 3u }, { 3u, 3u }, { 3u, 4u }, { 4u, 5u }, { 5u, 6u },
	{ 6u, 6u }, { 6u, 7u }, { 7u, 12u }, { 12u, 0u }, { 1u, 0xffffffffu },
	{ 0xffffffffu, 1u }, { 0u, 6u }, { 23u, 2u }
};

#define NSS (sizeof(ss_cases) / sizeof(ss_cases[0]))

/*
 * Four distinct patterns per trial, and one row that is entirely denormal and
 * negative-zero: `setFilterCoeff` is a straight copy, so a `float` parameter
 * that had been declared and passed as a number would round those in the
 * caller, on both sides, and the trial would be measuring nothing.
 */
static const unsigned ss_coef[][4] = {
	{ 0x3f800000u, 0x40000000u, 0x40400000u, 0x40800000u },
	{ 0xbf800000u, 0x3e800000u, 0xc0a00000u, 0x41200000u },
	{ 0x00000001u, 0x80000000u, 0x007fffffu, 0x80800000u },
	{ 0x7f7fffffu, 0xff7fffffu, 0x33d6bf95u, 0xb3d6bf95u }
};

#define NCOEF (sizeof(ss_coef) / sizeof(ss_coef[0]))

static int
run_ss_reset(void)
{
	unsigned c, k;
	long tag = 0;

	diff_begin("V90SpectralShaper::reset");

	for (c = 0; c < NSS; c++) {
		for (k = 0; k < NCOEF; k++) {
			unsigned char sa[sizeof(V90SpectralShaper)];
			unsigned char sb[sizeof(V90SpectralShaper)];
			unsigned width;

			tag++;
			harness_alloc_reset();
			fill_pair(ss_a.raw, ss_b.raw, SS_SLOT, (int)tag);

			our_ss_ctor(ss_a.raw);
			ref_ss_ctor(ss_b.raw);

			/*
			 * The constructor leaves both buffers exactly as the
			 * allocator returned them -- HARNESS_MALLOC_FILL --
			 * so a `reset` that cleared the wrong one, or fewer
			 * than 24 entries of the right one, shows up below
			 * against 0xa5 rather than against a zero both sides
			 * happened to have.
			 */
			our_ss_reset(ss_a.raw, ss_cases[c].id, ss_cases[c].sr,
				     ss_coef[k][0], ss_coef[k][1],
				     ss_coef[k][2], ss_coef[k][3]);
			ref_ss_reset(ss_b.raw, ss_cases[c].id, ss_cases[c].sr,
				     ss_coef[k][0], ss_coef[k][1],
				     ss_coef[k][2], ss_coef[k][3]);

			ss_snapshot(sa, ss_a.raw);
			ss_snapshot(sb, ss_b.raw);
			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "V90SpectralShaper", sa, sb,
				     sizeof(V90SpectralShaper), tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(ss_a.raw + sizeof(SS_A),
					   ss_b.raw + sizeof(SS_B),
					   SS_SLOT - sizeof(SS_A)) == 0, 1,
				    tag);

			diff_eq_int("trellis buffer +0x28 (%ld)",
				    memcmp(SS_A.delayLine, SS_B.delayLine,
					   SS_BUF * sizeof(short))
				    == 0, 1, tag);
			diff_eq_int("trellis buffer +0x2c (%ld)",
				    memcmp(SS_A.trialLine, SS_B.trialLine,
					   SS_BUF * sizeof(short))
				    == 0, 1, tag);
			diff_eq_int("encoder state (%ld)",
				    memcmp(SS_A.pde.state_, SS_B.pde.state_,
					   SS_A.pde.capacity_) == 0, 1, tag);

			/*
			 * THE ABSOLUTE CHECKS, because side-against-side
			 * cannot see two sides that both failed to store
			 * (findings F223, F224).  The width is the object's own
			 * arithmetic and the buffer must be all zero and
			 * +0x2c must be untouched -- still the allocator's
			 * fill.
			 */
			width = (ss_cases[c].sr != 0u)
			    ? 6u / ss_cases[c].sr : 0u;
			diff_eq_int("the blob's blockLength (%ld)",
				    (long)SS_B.blockLength, (long)width, tag);
			diff_eq_int("the blob's shaperId (%ld)",
				    (long)SS_B.shaperId,
				    (long)ss_cases[c].id, tag);
			diff_eq_int("the blob's shaperSR (%ld)",
				    (long)SS_B.shaperSR,
				    (long)ss_cases[c].sr, tag);
			diff_eq_int("the blob's writeIndex (%ld)",
				    (long)SS_B.writeIndex,
				    (long)(unsigned)(ss_cases[c].id * width),
				    tag);
			diff_eq_int("the blob's windowLength (%ld)",
				    (long)SS_B.windowLength,
				    (long)(unsigned)((ss_cases[c].id + 1u)
						     * width), tag);
			diff_eq_int("the filter took the width (%ld)",
				    (long)SS_B.ssf.blockLength, (long)width,
				    tag);
			diff_eq_int("the four coefficients reached the "
				    "filter (%ld)",
				    memcmp(SS_B.ssf.coeff, ss_coef[k],
					   4 * sizeof(float)) == 0, 1, tag);

			{
				unsigned i;
				int allzero = 1, untouched = 1;

				for (i = 0; i < SS_BUF; i++)
					if (SS_B.delayLine[i] != 0)
						allzero = 0;
				for (i = 0;
				     i < SS_BUF * sizeof(short); i++)
					if (((unsigned char *)SS_B.trialLine)[i]
					    != HARNESS_MALLOC_FILL)
						untouched = 0;

				diff_eq_int("all 24 of +0x28 cleared (%ld)",
					    allzero, 1, tag);
				diff_eq_int("+0x2c left as allocated (%ld)",
					    untouched, 1, tag);
			}

			our_ss_dtor(ss_a.raw);
			ref_ss_dtor(ss_b.raw);
			diff_eq_int("no leak, no bad free (%ld)",
				    harness_alloc.live == 0
				    && harness_alloc.bad_free == 0, 1, tag);
		}
	}

	/*
	 * THE SEPARATING TRIALS, and each is a pair whose OBSERVABLE state
	 * differs (findings F3509, F3403).  `6 / sr` against `6 - sr` is the
	 * one that matters: they agree at sr == 3 and at nowhere else in the
	 * sweep, so a reconstruction that took the subtraction passes every
	 * trial at 3 and fails the rest -- which is only a separating trial
	 * if some trial in the set actually distinguishes them, and this
	 * counts the ones that do.
	 */
	{
		int sep_div = 0, sep_id = 0;
		unsigned prev = 0u;
		unsigned i;

		harness_alloc_reset();
		for (i = 1; i <= 7; i++) {
			fill_pair(ss_a.raw, ss_b.raw, SS_SLOT, 900 + (int)i);
			our_ss_ctor(ss_a.raw);
			our_ss_reset(ss_a.raw, 2u, i, ss_coef[0][0],
				     ss_coef[0][1], ss_coef[0][2],
				     ss_coef[0][3]);
			if (SS_A.blockLength != 6u - i)
				sep_div++;
			our_ss_dtor(ss_a.raw);
		}

		for (i = 0; i < 4; i++) {
			fill_pair(ss_a.raw, ss_b.raw, SS_SLOT, 950 + (int)i);
			our_ss_ctor(ss_a.raw);
			our_ss_reset(ss_a.raw, i, 2u, ss_coef[0][0],
				     ss_coef[0][1], ss_coef[0][2],
				     ss_coef[0][3]);
			if (i > 0 && SS_A.writeIndex != prev)
				sep_id++;
			prev = SS_A.writeIndex;
			our_ss_dtor(ss_a.raw);
		}

		diff_eq_int("the divide is not a subtraction, over 1..7",
			    sep_div, 6, 0);
		diff_eq_int("shaperId moves writeIndex", sep_id, 3, 0);
	}

	return diff_end();
}

/* ==================================== V90SpectralShaper::resetSSFilter ==== */

/*
 * The narrow one: it must touch the embedded filter and NOTHING else.  The
 * shaper is reset first so that every field outside the filter holds a value
 * a wrong store would move visibly, and the whole 0x6c is compared after.
 */
static int
run_ss_resetssf(void)
{
	unsigned c, k;
	long tag = 0;

	diff_begin("V90SpectralShaper::resetSSFilter");

	for (c = 0; c < NSS; c++) {
		for (k = 0; k < NCOEF; k++) {
			unsigned char sa[sizeof(V90SpectralShaper)];
			unsigned char sb[sizeof(V90SpectralShaper)];
			unsigned other = (k + 1u) % NCOEF;

			tag++;
			harness_alloc_reset();
			fill_pair(ss_a.raw, ss_b.raw, SS_SLOT, (int)tag + 400);

			our_ss_ctor(ss_a.raw);
			ref_ss_ctor(ss_b.raw);
			our_ss_reset(ss_a.raw, ss_cases[c].id, ss_cases[c].sr,
				     ss_coef[k][0], ss_coef[k][1],
				     ss_coef[k][2], ss_coef[k][3]);
			ref_ss_reset(ss_b.raw, ss_cases[c].id, ss_cases[c].sr,
				     ss_coef[k][0], ss_coef[k][1],
				     ss_coef[k][2], ss_coef[k][3]);

			/* A DIFFERENT row, so the overwrite is observable. */
			our_ss_resetssf(ss_a.raw, ss_coef[other][0],
					ss_coef[other][1], ss_coef[other][2],
					ss_coef[other][3]);
			ref_ss_resetssf(ss_b.raw, ss_coef[other][0],
					ss_coef[other][1], ss_coef[other][2],
					ss_coef[other][3]);

			ss_snapshot(sa, ss_a.raw);
			ss_snapshot(sb, ss_b.raw);
			diff_eq_obj_(__FILE__, __LINE__, "after resetSSFilter",
				     "V90SpectralShaper", sa, sb,
				     sizeof(V90SpectralShaper), tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(ss_a.raw + sizeof(SS_A),
					   ss_b.raw + sizeof(SS_B),
					   SS_SLOT - sizeof(SS_A)) == 0, 1,
				    tag);

			diff_eq_int("the new coefficients landed (%ld)",
				    memcmp(SS_B.ssf.coeff, ss_coef[other],
					   4 * sizeof(float)) == 0, 1, tag);
			diff_eq_int("the filter state was cleared (%ld)",
				    SS_B.ssf.state[0] == 0.0f
				    && SS_B.ssf.state[1] == 0.0f
				    && SS_B.ssf.state[2] == 0.0f
				    && SS_B.ssf.state[3] == 0.0f, 1, tag);

			/*
			 * AND THE WIDTH SURVIVED.  `reset` writes
			 * `ssf.blockLength` and `resetSSFilter` does not; a
			 * reconstruction that called the shaper's `reset`
			 * instead of the filter's would pass every check
			 * above and fail this one.
			 */
			diff_eq_int("blockLength untouched by resetSSFilter "
				    "(%ld)", (long)SS_B.ssf.blockLength,
				    (long)((ss_cases[c].sr != 0u)
					   ? 6u / ss_cases[c].sr : 0u), tag);
			diff_eq_int("the shaper's own words untouched (%ld)",
				    SS_B.shaperId == ss_cases[c].id
				    && SS_B.shaperSR == ss_cases[c].sr, 1,
				    tag);

			our_ss_dtor(ss_a.raw);
			ref_ss_dtor(ss_b.raw);
			diff_eq_int("no leak, no bad free (%ld)",
				    harness_alloc.live == 0
				    && harness_alloc.bad_free == 0, 1, tag);
		}
	}

	return diff_end();
}

/* ================================================= V90Modulator::reset ==== */

/*
 * The scrambler's seven pointers become offsets from its own buffer, which is
 * the same number on both sides for the same history and a DIFFERENT one the
 * moment `reset` or `resetHistoryIndexes` fails to move one.  `pLimit` itself
 * becomes "is it null"; everything else is relative to it.
 */
static void
mod_snapshot(void *dst, const unsigned char *src)
{
	const V90Modulator *s = (const V90Modulator *)src;
	V90Modulator *d = (V90Modulator *)dst;
	const V90Scrambler *ss = &s->scrambler;
	V90Scrambler *ds = &d->scrambler;
	const int *base = ss->pLimit;

	memcpy(dst, src, sizeof(V90Modulator));

	/* The eleven constructor pointers are never read; neutralise them. */
	d->phase2Info = 0;
	d->jd = 0;
	d->v92Jd = 0;
	d->dil = 0;
	d->mappingParams = 0;
	d->mappingParams2 = 0;
	d->additionalCPinfo = 0;
	d->mp = 0;
	d->cp = 0;
	d->params = 0;
	d->phase3Modulator = 0;
	d->phase4Modulator = 0;
	d->bitsToSymbol = 0;
	d->symbolBuf = 0;
	d->frameBuf = 0;

	ds->pLimit = (int *)(long)(base != 0);
	ds->pInitOut = (int *)(long)(ss->pInitOut - base);
	ds->pInitTap1 = (int *)(long)(ss->pInitTap1 - base);
	ds->pInitTap2 = (int *)(long)(ss->pInitTap2 - base);
	ds->pOut = (int *)(long)(ss->pOut - base);
	ds->pTap1 = (int *)(long)(ss->pTap1 - base);
	ds->pTap2 = (int *)(long)(ss->pTap2 - base);
}

static int
run_mod_reset(void)
{
	unsigned lvl;
	int trial;
	long tag = 0;

	diff_begin("V90Modulator::reset");

	for (lvl = 0; lvl <= 2; lvl++) {
		for (trial = 0; trial < 6; trial++) {
			unsigned char sa[sizeof(V90Modulator)];
			unsigned char sb[sizeof(V90Modulator)];
			unsigned i;

			tag++;
			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;

			harness_alloc_reset();
			fill_pair(mod_a.raw, mod_b.raw, MOD_SLOT, (int)tag);

			our_scr_ctor(&MOD_A.scrambler, SCR_TAPS_A,
				     SCR_TAPS_B, SCR_TAPS_C);
			our_scr_ctor(&MOD_B.scrambler, SCR_TAPS_A,
				     SCR_TAPS_B, SCR_TAPS_C);

			/*
			 * Walk the scrambler off its initial pointers before
			 * the reset, so "put the three running pointers back"
			 * has something to undo.  Half the trials skip it, so
			 * the already-at-rest case is driven too.
			 */
			if (trial & 1) {
				unsigned char out[16];

				MOD_A.scrambler.processAllZeros(out, 16u);
				MOD_B.scrambler.processAllZeros(out, 16u);
			}

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			MOD_A.reset();
			ref_mod_reset(mod_b.raw);

			dsplib_debug_capture_on = 0;

			mod_snapshot(sa, mod_a.raw);
			mod_snapshot(sb, mod_b.raw);
			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "V90Modulator", sa, sb,
				     sizeof(V90Modulator), tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(mod_a.raw + sizeof(MOD_A),
					   mod_b.raw + sizeof(MOD_B),
					   MOD_SLOT - sizeof(MOD_A)) == 0, 1,
				    tag);
			diff_eq_int("scrambler history (%ld)",
				    memcmp(MOD_A.scrambler.pLimit,
					   MOD_B.scrambler.pLimit,
					   SCR_ELEMS * sizeof(int)) == 0, 1,
				    tag);
			diff_eq_int("transcript matches (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("line counts match (%ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);

			/*
			 * THE DIAGNOSTIC IS GATED AT LEVEL 1, unlike the
			 * spectral group's: `cmpl $0x1` with `ja` at
			 * 0x1a518, so levels 0 and 1 are silent and 2 is not.
			 * Asserted absolutely, because two silent sides agree
			 * whatever either of them does.
			 */
			diff_eq_int("printed exactly when it should (%ld)",
				    dsplib_debug_capture_lines(1) > 0,
				    lvl > 1, tag);

			/* And the three words, at their absolute offsets. */
			diff_eq_int("the blob cleared +0x2c (%ld)",
				    (long)MOD_B.state, 0, tag);
			diff_eq_int("the blob cleared +0x30 (%ld)",
				    (long)MOD_B.symbolCount, 0, tag);
			diff_eq_int("the blob cleared +0x34 (%ld)",
				    (long)MOD_B.eventCode, 0, tag);

			/*
			 * AND THE SEED WAS NOT ZERO, so "cleared" is a
			 * statement about the function and not about the
			 * fixture.
			 */
			{
				int seeded = 0;

				lfsr = 0x6d1bu + 0x9e37u * (unsigned)tag;
				for (i = 0; i < 0x34u + 4u; i++) {
					unsigned char v = next_byte();

					if (i >= 0x2cu && v != 0)
						seeded = 1;
				}
				diff_eq_int("the three words were not already"
					    " zero (%ld)", seeded, 1, tag);
			}

			/*
			 * The scrambler is BACK AT REST: all three running
			 * pointers equal to their init values, whether or not
			 * this trial walked them.
			 */
			diff_eq_int("scrambler at rest (%ld)",
				    MOD_B.scrambler.pOut
				    == MOD_B.scrambler.pInitOut
				    && MOD_B.scrambler.pTap1
				    == MOD_B.scrambler.pInitTap1
				    && MOD_B.scrambler.pTap2
				    == MOD_B.scrambler.pInitTap2, 1, tag);

			our_scr_dtor(&MOD_A.scrambler);
			our_scr_dtor(&MOD_B.scrambler);
			diff_eq_int("no leak, no bad free (%ld)",
				    harness_alloc.live == 0
				    && harness_alloc.bad_free == 0, 1, tag);
		}
	}

	dsplibs_debug_level = 0u;
	ref_dsplibs_debug_level = 0u;
	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_ss_reset();
	bad |= run_ss_resetssf();
	bad |= run_mod_reset();

	return bad;
}
