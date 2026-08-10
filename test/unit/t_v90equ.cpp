/*
 * t_v90equ.cpp -- differential test of V90Equalizer::setLinearEquBeta,
 * ::setDfeBeta and ::enterPhase3.
 *
 * The fixture is t_v90jd.cpp's: the object lives in a union with a byte
 * array, both sides are seeded with the SAME varied pseudorandom bytes and
 * never with zeros, the whole object is compared with `diff_eq_obj`, and the
 * bytes from `sizeof` to the end of an over-large slot are compared
 * separately so a store past the object's end is a failure rather than
 * silence (findings 223, 224, 230).
 *
 * WHAT THIS TEST HAS TO SEE THAT AN ORDINARY ONE WOULD NOT
 *
 * Two thirds of each setter is arithmetic whose only outputs are an integer
 * shift and an integer step size, and the shift is a TRUNCATED logarithm.  So
 * the inputs are built to put that logarithm exactly on an integer:
 *
 *      shift = (int)(log10(fabs(refLevel / (beta * 2**k))) / log10(2.0f))
 *
 * is exactly `m` when `refLevel` is 1.0f and `beta` is 2**-(k+m), and one ulp
 * either way in the logarithm moves it to `m-1`.  That is the whole reason
 * the reconstruction computes the two logarithms with `fldlg2`/`fyl2x`
 * instead of calling libm, and a sweep over "reasonable" step sizes would
 * never have told the two apart.  `beta_pow2` below is that sweep, at both
 * k = 24 (the linear equaliser) and k = 20 (the DFE), and over negative `m`
 * as well, where the truncation goes the other way.
 *
 * The rest of each setter is a diagnostic, and a diagnostic is invisible at
 * the shipped debug level.  So the sweep runs again with capture on and both
 * sides' levels raised, and the two transcripts are compared (finding 134's
 * problem, and the harness's answer to it).  That is what tests the format
 * string, the sign character, the integer part and the five fractional digits
 * -- none of which the object state can show.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90Equalizer.h"

/*
 * Our `dsplibs_debug_level` comes from dsplib/debug.h; the object's own copy
 * is renamed like every other symbol in the reference object, so it has to be
 * declared here.  The two are always set together -- otherwise the sides gate
 * differently and the transcripts diverge for a reason belonging to the
 * harness rather than to the function.  Same shape as t_v90p2info.cpp.
 */
extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_setLinearEquBeta(void *self, float beta)
	asm("ref__ZN12V90Equalizer16setLinearEquBetaEf");
void ref_setDfeBeta(void *self, float beta)
	asm("ref__ZN12V90Equalizer10setDfeBetaEf");
void ref_enterPhase3(void *self)
	asm("ref__ZN12V90Equalizer11enterPhase3Ev");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 400

union equ_slot {
	V90Equalizer o;
	unsigned char raw[SLOT];
};

static union equ_slot ours, theirs;

static void
seed(long trial)
{
	unsigned lfsr = 0x2f6du + 0x9e37u * (unsigned)trial;
	int i;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)(lfsr >> 3);
		ours.raw[i] = v;
		theirs.raw[i] = v;
	}
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V90Equalizer),
		      theirs.raw + sizeof(V90Equalizer),
		      SLOT - sizeof(V90Equalizer)) == 0;
}

/* 2**e as a float, without libm and without a rounding step. */
static float
pow2f(int e)
{
	union { unsigned u; float f; } v;

	v.u = (unsigned)(e + 127) << 23;
	return v.f;
}

static float
bitsf(unsigned u)
{
	union { unsigned u; float f; } v;

	v.u = u;
	return v.f;
}

/*
 * The step sizes.  The first two families put the truncated logarithm exactly
 * on an integer for k = 24 and k = 20 respectively; the third puts it just
 * off one, in both directions, by perturbing the exponent's neighbour bits.
 */
#define NBETA 64
static float beta_v[NBETA];
static int nbeta;

static void
build_betas(void)
{
	int m;

	nbeta = 0;
	for (m = -8; m <= 12; m++)
		beta_v[nbeta++] = pow2f(-24 - m);	/* q = 2**m at k=24 */
	for (m = -4; m <= 8; m++)
		beta_v[nbeta++] = pow2f(-20 - m);	/* q = 2**m at k=20 */

	/* One ulp either side of an exact power, so the truncation is seen
	 * to move and the exact cases are not the only ones. */
	beta_v[nbeta++] = bitsf(0x33800000u + 1u);
	beta_v[nbeta++] = bitsf(0x33800000u - 1u);

	beta_v[nbeta++] = 0.0f;
	beta_v[nbeta++] = -0.0f;
	beta_v[nbeta++] = 1.0f;
	beta_v[nbeta++] = -1.0f;
	beta_v[nbeta++] = 1.0e-10f;
	beta_v[nbeta++] = -3.7e-7f;
	beta_v[nbeta++] = 1.0e-30f;
	beta_v[nbeta++] = bitsf(0x00000007u);		/* denormal          */
	beta_v[nbeta++] = bitsf(0x7f800000u);		/* +inf              */
	beta_v[nbeta++] = bitsf(0xff800000u);		/* -inf              */
	beta_v[nbeta++] = bitsf(0x7fc00000u);		/* quiet NaN         */
	beta_v[nbeta++] = 12345.678f;
}

static const float ref_v[] = {
	1.0f, 2.0f, 0.0f, -1.0f, 3.0f, 1.0e-6f, 1.0e12f
};
static const float scale_v[] = { 1.0f, 32768.0f, -1.0f, 1.0e10f };

#define NREF ((int)(sizeof(ref_v) / sizeof(ref_v[0])))
#define NSCALE ((int)(sizeof(scale_v) / sizeof(scale_v[0])))

/*
 * What varies across a trial, and what is observed from it.  `prev` chooses
 * whether the field already holds the value being set, which is the guard on
 * the diagnostic.
 */
struct outcome {
	int	printed;
	int	silent;
	int	shift_seen[8];
	int	nshift;
	int	nonzero_beta;
	int	changed;
};

static void
note_shift(struct outcome *out, int shift)
{
	int i;

	for (i = 0; i < out->nshift; i++)
		if (out->shift_seen[i] == shift)
			return;
	if (out->nshift < (int)(sizeof(out->shift_seen)
				/ sizeof(out->shift_seen[0])))
		out->shift_seen[out->nshift++] = shift;
}

/*
 * One setter, swept.  `which` is 0 for the linear equaliser and 1 for the
 * DFE; the two differ only in the fields they read and write, so the sweep
 * is shared and the field addresses are picked here.
 */
static void
sweep_setter(int which, unsigned level, struct outcome *out, long *tagp)
{
	int bi, ri, si, mmx, prev;

	for (bi = 0; bi < nbeta; bi++)
	    for (ri = 0; ri < NREF; ri++)
		for (si = 0; si < NSCALE; si++)
		    for (mmx = 0; mmx < 2; mmx++)
			for (prev = 0; prev < 2; prev++) {
				long tag = (*tagp)++;
				unsigned char before[SLOT];
				float beta = beta_v[bi];
				const char *ta, *tb;
				int shift;

				seed(tag);

				ours.o.mmxMode = theirs.o.mmxMode = mmx;
				ours.o.linearEquMmxRefLevel =
				    theirs.o.linearEquMmxRefLevel = ref_v[ri];
				ours.o.dfeMmxRefLevel =
				    theirs.o.dfeMmxRefLevel = ref_v[ri];
				ours.o.linearEquMmxBetaScale =
				    theirs.o.linearEquMmxBetaScale =
					scale_v[si];
				ours.o.dfeMmxBetaScale =
				    theirs.o.dfeMmxBetaScale = scale_v[si];

				/*
				 * `prev` drives both arms of the diagnostic's
				 * guard: with it set the field already holds
				 * the value, so the object must not print.
				 */
				ours.o.linearEquBeta =
				    theirs.o.linearEquBeta =
					prev ? beta : -7.5f;
				ours.o.dfeBeta = theirs.o.dfeBeta =
					prev ? beta : -7.5f;

				memcpy(before, ours.raw, SLOT);
				if (level > 1)
					dsplib_debug_capture_reset();

				if (which == 0) {
					ours.o.setLinearEquBeta(beta);
					ref_setLinearEquBeta(&theirs.o, beta);
				} else {
					ours.o.setDfeBeta(beta);
					ref_setDfeBeta(&theirs.o, beta);
				}

				diff_eq_obj("after setBeta", V90Equalizer,
					    &ours.o, &theirs.o, tag);
				diff_eq_int("no store past the object "
					    "(trial %ld)", guard_equal(), 1,
					    tag);

				if (level > 1) {
					ta = dsplib_debug_capture_text(0);
					tb = dsplib_debug_capture_text(1);
					diff_eq_int("transcript (trial %ld)",
						    strcmp(ta, tb) == 0, 1,
						    tag);
					if (dsplib_debug_capture_lines(1) > 0)
						out->printed = 1;
					else
						out->silent = 1;
				}

				shift = which == 0 ? ours.o.linearEquMmxShift
						   : ours.o.dfeMmxShift;
				if (mmx) {
					note_shift(out, shift);
					if ((which == 0
					     ? ours.o.linearEquMmxBeta
					     : ours.o.dfeMmxBeta) != 0)
						out->nonzero_beta = 1;
				}
				if (memcmp(before, ours.raw, SLOT) != 0)
					out->changed = 1;
			}
}

static int
run_setters(void)
{
	struct outcome le, dfe;
	long tag = 0;
	unsigned level;

	diff_begin("V90Equalizer::setLinearEquBeta / setDfeBeta");

	memset(&le, 0, sizeof(le));
	memset(&dfe, 0, sizeof(dfe));

	dsplib_debug_capture_on = 1;
	for (level = 0; level <= 3; level++) {
		if (level == 1)
			continue;		/* the gate is `> 1`      */
		dsplibs_debug_level = level;
		ref_dsplibs_debug_level = level;
		sweep_setter(0, level, &le, &tag);
		sweep_setter(1, level, &dfe, &tag);
	}
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	/*
	 * Anti-vacuity.  A sweep that never printed, never stayed silent,
	 * never left the fixed-point block with more than one answer or
	 * never changed the object would agree with anything.
	 */
	diff_eq_int("setLinearEquBeta printed", le.printed, 1, 0);
	diff_eq_int("setLinearEquBeta also stayed silent", le.silent, 1, 0);
	diff_eq_int("setLinearEquBeta changed the object", le.changed, 1, 0);
	diff_eq_int("setLinearEquBeta produced a non-zero step",
		    le.nonzero_beta, 1, 0);
	diff_eq_int("setLinearEquBeta produced several shifts",
		    le.nshift >= 4, 1, le.nshift);
	diff_eq_int("setDfeBeta printed", dfe.printed, 1, 0);
	diff_eq_int("setDfeBeta also stayed silent", dfe.silent, 1, 0);
	diff_eq_int("setDfeBeta changed the object", dfe.changed, 1, 0);
	diff_eq_int("setDfeBeta produced a non-zero step",
		    dfe.nonzero_beta, 1, 0);
	diff_eq_int("setDfeBeta produced several shifts", dfe.nshift >= 4, 1,
		    dfe.nshift);

	return diff_end();
}

/*
 * The exact-logarithm cases on their own, checked against the arithmetic
 * rather than only against the blob: with refLevel 1.0f and beta 2**-(k+m)
 * the quotient is exactly 2**m, so the shift the object stores must be m.
 * This is the claim that separates the coprocessor's logarithm from libm's,
 * and it is asserted here so that a regression names itself rather than
 * arriving as one differing byte at +0xd0.
 */
static int
run_exact_powers(void)
{
	int m;
	long tag = 900000;

	diff_begin("V90Equalizer: the shift at an exact power of two");

	for (m = -8; m <= 12; m++) {
		seed(tag);
		ours.o.mmxMode = theirs.o.mmxMode = 1;
		ours.o.linearEquMmxRefLevel =
		    theirs.o.linearEquMmxRefLevel = 1.0f;
		ours.o.dfeMmxRefLevel = theirs.o.dfeMmxRefLevel = 1.0f;
		ours.o.linearEquMmxBetaScale =
		    theirs.o.linearEquMmxBetaScale = 1.0f;
		ours.o.dfeMmxBetaScale = theirs.o.dfeMmxBetaScale = 1.0f;
		ours.o.linearEquBeta = theirs.o.linearEquBeta = -7.5f;
		ours.o.dfeBeta = theirs.o.dfeBeta = -7.5f;

		ours.o.setLinearEquBeta(pow2f(-24 - m));
		ref_setLinearEquBeta(&theirs.o, pow2f(-24 - m));
		diff_eq_obj("after setLinearEquBeta", V90Equalizer, &ours.o,
			    &theirs.o, m);
		diff_eq_int("linear equaliser shift at 2**%ld",
			    ours.o.linearEquMmxShift, m, m);

		ours.o.setDfeBeta(pow2f(-20 - m));
		ref_setDfeBeta(&theirs.o, pow2f(-20 - m));
		diff_eq_obj("after setDfeBeta", V90Equalizer, &ours.o,
			    &theirs.o, m);
		diff_eq_int("DFE shift at 2**%ld", ours.o.dfeMmxShift, m, m);

		diff_eq_int("no store past the object (m = %ld)",
			    guard_equal(), 1, m);
		tag++;
	}

	return diff_end();
}

static int
run_enterphase3(void)
{
	long tag = 500000;
	int state, mmx, ri, entered = 0, skipped = 0;
	unsigned level;

	diff_begin("V90Equalizer::enterPhase3");

	dsplib_debug_capture_on = 1;
	for (level = 0; level <= 3; level++) {
		if (level == 1)
			continue;
		dsplibs_debug_level = level;
		ref_dsplibs_debug_level = level;

		for (state = -1; state <= 7; state++)
		    for (mmx = 0; mmx < 2; mmx++)
			for (ri = 0; ri < NREF; ri++) {
				unsigned char before[SLOT];

				seed(tag);
				ours.o.state = theirs.o.state = state;
				ours.o.mmxMode = theirs.o.mmxMode = mmx;
				ours.o.linearEquMmxRefLevel =
				    theirs.o.linearEquMmxRefLevel = ref_v[ri];
				ours.o.dfeMmxRefLevel =
				    theirs.o.dfeMmxRefLevel = ref_v[ri];
				ours.o.linearEquMmxBetaScale =
				    theirs.o.linearEquMmxBetaScale = 1.0f;
				ours.o.dfeMmxBetaScale =
				    theirs.o.dfeMmxBetaScale = 1.0f;

				memcpy(before, ours.raw, SLOT);
				if (level > 1)
					dsplib_debug_capture_reset();

				ours.o.enterPhase3();
				ref_enterPhase3(&theirs.o);

				diff_eq_obj("after enterPhase3", V90Equalizer,
					    &ours.o, &theirs.o, tag);
				diff_eq_int("no store past the object "
					    "(trial %ld)", guard_equal(), 1,
					    tag);

				if (level > 1) {
					diff_eq_int("transcript (trial %ld)",
						    strcmp(dsplib_debug_capture_text(0),
							   dsplib_debug_capture_text(1))
						    == 0, 1, tag);
				}

				if (state == V90EQU_STATE_PHASE3) {
					/*
					 * Idempotent: the whole body is under
					 * `cmpl $0x1,0x60(%esi); je`, so
					 * nothing at all may move.
					 */
					diff_eq_int("re-entry touched nothing "
						    "(trial %ld)",
						    memcmp(before, ours.raw,
							   SLOT) == 0, 1, tag);
					skipped = 1;
				} else {
					diff_eq_int("state (trial %ld)",
						    ours.o.state,
						    V90EQU_STATE_PHASE3, tag);
					diff_eq_int("stateCount (trial %ld)",
						    ours.o.stateCount, 0, tag);
					diff_eq_int("linearEquBeta zeroed "
						    "(trial %ld)",
						    ours.o.linearEquBeta
						    == 0.0f, 1, tag);
					diff_eq_int("dfeBeta zeroed "
						    "(trial %ld)",
						    ours.o.dfeBeta == 0.0f, 1,
						    tag);
					entered = 1;
				}
				tag++;
			}
	}
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	diff_eq_int("enterPhase3 ran its body", entered, 1, 0);
	diff_eq_int("and skipped it when already in Phase 3", skipped, 1, 0);

	return diff_end();
}

/* ================================================================ task #88 */

/*
 * `reset` and `enterChannelVerification` need what the three setters did not:
 * twelve arrays, a parameter block and a resampler.
 *
 * ONE ARENA, SHARED BY BOTH SIDES, AND A SNAPSHOT ROUND EACH CALL.  Finding
 * 1105's rule is that identical argument pointers give identical stored
 * pointers, and that is what makes `diff_eq_obj` usable on the object with no
 * field excluded.  But the arrays are OUTPUTS, and two writers into one buffer
 * would leave only the second one's work: so the arena is snapshotted, ours
 * runs, the result is copied away, the arena is restored, and the blob's runs
 * against the same starting bytes.  That is finding 805's shape applied to a
 * single member instead of to a whole datapump block.
 *
 * WHAT THE ARENA HAS TO BE BIG ENOUGH FOR.  `reset` clears
 * `linearEquLength`, `word_1c`, `dfeLength` and each of those plus eight
 * entries; it plants 1.0f at `linearEquCoefs[cursor]` with the cursor clamped
 * in UNSIGNED arithmetic -- so a zero-length equaliser does not clamp at all
 * and the cursor lands wherever it was asked to -- and it hammings
 * `2 * (unsigned)(ratio * length)` entries of two windows with the ratio
 * clamped to [0, 0.5].  Every bound below is at least twice what the sweep
 * can produce.
 */

#include "dsplib/V90Resampler.h"

extern "C" {
void ref_equ_reset(void *self, unsigned int cursor)
	asm("ref__ZN12V90Equalizer5resetEj");
void ref_equ_enterChannelVerification(void *self)
	asm("ref__ZN12V90Equalizer24enterChannelVerificationEv");
}

#define ARR_F	64		/* entries in each float array  */
#define ARR_S	64		/* entries in each short array  */
#define RSLOT	(0xb4 + 32)	/* the V90Resampler, plus slack */

struct equ_arena {
	float		lecoefs[ARR_F];
	float		a18[ARR_F];
	float		lewin[ARR_F];
	float		dfewin[ARR_F];
	float		dfecoefs[ARR_F];
	float		a44[ARR_F];
	short		lemmx[ARR_S];
	short		ad8[ARR_S];
	short		aec[ARR_S];
	short		dfemmx[ARR_S];
	short		a118[ARR_S];
	short		a12c[ARR_S];
	unsigned char	rsamp[RSLOT];
	unsigned char	parm[sizeof(V90Parameters) + 32];
};

static struct equ_arena arena, arena_save, arena_ours;

#define ARENA_PARAMS ((V90Parameters *)arena.parm)
#define ARENA_RSAMP  ((V90Resampler *)arena.rsamp)

/* Varied bytes, never zeros (finding 230). */
static void
fill_arena(long trial)
{
	unsigned char *p = (unsigned char *)&arena;
	unsigned s = 0x4d2fu + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < sizeof(arena); i++) {
		s = (s >> 1) ^ (-(int)(s & 1u) & 0xb400u);
		p[i] = (unsigned char)((s >> 3) | 1u);
	}
}

/* Point both objects at the one arena, so every stored pointer agrees. */
static void
wire(V90Equalizer *o)
{
	o->linearEquCoefs = arena.lecoefs;
	o->array_18 = arena.a18;
	o->linearEquWindow = arena.lewin;
	o->dfeWindow = arena.dfewin;
	o->dfeCoefs = arena.dfecoefs;
	o->array_44 = arena.a44;
	o->linearEquMmxCoefs = arena.lemmx;
	o->array_d8 = arena.ad8;
	o->array_ec = arena.aec;
	o->dfeMmxCoefs = arena.dfemmx;
	o->array_118 = arena.a118;
	o->array_12c = arena.a12c;
	o->params = ARENA_PARAMS;
	o->resampler = ARENA_RSAMP;
}

/* The fade ratios, one per clamp arm plus the two edges and a NaN. */
static const float fade_v[] = {
	-1.0f, -0.0f, 0.0f, 0.05f, 0.25f, 0.5f, 0.6f, 1.0f, 37.0f
};
#define NFADE ((int)(sizeof(fade_v) / sizeof(fade_v[0])))

static int
run_reset(void)
{
	static const unsigned int len_v[] = { 0u, 1u, 4u, 8u, 16u };
	static const unsigned int m_v[]   = { 1u, 8u, 24u };
	static const unsigned int dfe_v[] = { 0u, 3u, 12u };
	static const unsigned int cur_v[] = { 0u, 1u, 7u, 40u };
	long tag = 700000;
	int li, mi, di, ci, fi, mmx;
	int saw_clamped = 0, saw_kept = 0, saw_window = 0, saw_mmx = 0;

	diff_begin("V90Equalizer::reset");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (li = 0; li < 5; li++)
	    for (mi = 0; mi < 3; mi++)
		for (di = 0; di < 3; di++)
		    for (ci = 0; ci < 4; ci++)
			for (fi = 0; fi < NFADE; fi++)
			    for (mmx = 0; mmx < 2; mmx++) {
				unsigned int len = len_v[li];
				unsigned int m = m_v[mi];
				unsigned int cursor = cur_v[ci];
				unsigned int want;

				/* array_18 runs down from word_1c. */
				if (m < len)
					continue;

				tag++;
				seed(tag);
				fill_arena(tag);
				wire(&ours.o);
				wire(&theirs.o);

				ours.o.linearEquLength =
				    theirs.o.linearEquLength = len;
				ours.o.word_1c = theirs.o.word_1c = m;
				ours.o.dfeLength = theirs.o.dfeLength =
				    dfe_v[di];
				ours.o.mmxArraysPresent =
				    theirs.o.mmxArraysPresent = mmx;

				ARENA_PARAMS->LINEAR_EQU_FADE_LEFT_EDGE_RATIO =
				    fade_v[fi];
				ARENA_PARAMS->LINEAR_EQU_FADE_RIGHT_EDGE_RATIO =
				    fade_v[(fi + 3) % NFADE];
				ARENA_PARAMS->ERROR_ENERGY_MEAN_BLOCK_LEN =
				    (int)(0x1234 + tag);
				ARENA_PARAMS->ERROR_ENERGY_MEAN_K = 0.375f;

				memcpy(&arena_save, &arena, sizeof(arena));
				dsplib_debug_capture_reset();

				ours.o.reset(cursor);

				memcpy(&arena_ours, &arena, sizeof(arena));
				memcpy(&arena, &arena_save, sizeof(arena));

				ref_equ_reset(&theirs.o, cursor);

				diff_eq_obj("after reset", V90Equalizer,
					    &ours.o, &theirs.o, tag);
				diff_eq_obj("the arena after reset",
					    struct equ_arena, &arena_ours,
					    &arena, tag);
				diff_eq_int("no store past the object (%ld)",
					    guard_equal(), 1, tag);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);

				/*
				 * The fill is GONE.  Two never-reset objects
				 * compare equal (finding 1105), so the values
				 * the object must hold are asserted and not
				 * only compared.
				 */
				diff_eq_int("state (%ld)", theirs.o.state,
					    V90EQU_STATE_RESET, tag);
				diff_eq_int("stateCount (%ld)",
					    theirs.o.stateCount, 0, tag);
				diff_eq_int("flag_144 (%ld)",
					    (long)theirs.o.flag_144, 1, tag);
				diff_eq_int("flag_146 (%ld)",
					    (long)theirs.o.flag_146, 1, tag);
				diff_eq_int("mmxMode (%ld)", theirs.o.mmxMode,
					    0, tag);
				diff_eq_int("word_20 = word_1c - len - 1 "
					    "(%ld)", (long)theirs.o.word_20,
					    (long)(unsigned int)(m - len - 1u),
					    tag);
				diff_eq_int("errorEnergyMeanBlockLen (%ld)",
					    theirs.o.errorEnergyMeanBlockLen,
					    (int)(0x1234 + tag), tag);
				diff_eq_int("errorEnergyMeanK copied (%ld)",
					    theirs.o.errorEnergyMeanK
					    == 0.375f, 1, tag);
				diff_eq_int("linearEquBeta zeroed (%ld)",
					    theirs.o.linearEquBeta == 0.0f, 1,
					    tag);
				diff_eq_int("dfeBeta zeroed (%ld)",
					    theirs.o.dfeBeta == 0.0f, 1, tag);

				/*
				 * The cursor, and the clamp that is unsigned:
				 * a zero-length equaliser does not clamp.
				 */
				want = (len - 1u < cursor) ? len - 1u : cursor;
				if (want != cursor)
					saw_clamped = 1;
				else
					saw_kept = 1;
				diff_eq_int("the 1.0f landed at the clamped "
					    "cursor (%ld)",
					    arena.lecoefs[want] == 1.0f, 1,
					    tag);

				/* And the window half really is a fraction. */
				diff_eq_int("linearEquWindowHalf <= len/2 "
					    "(%ld)",
					    theirs.o.linearEquWindowHalf * 2u
					    <= len, 1, tag);
				if (theirs.o.linearEquWindowHalf > 0)
					saw_window = 1;
				if (mmx)
					saw_mmx = 1;
			}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the cursor was clamped somewhere", saw_clamped, 1, 0);
	diff_eq_int("and left alone somewhere", saw_kept, 1, 0);
	diff_eq_int("a non-empty window was built", saw_window, 1, 0);
	diff_eq_int("the fixed-point arrays were cleared", saw_mmx, 1, 0);

	/*
	 * WHICH RATIO SCALES WHICH WINDOW, asked with two ratios that cannot
	 * give the same answer.  The sweep above draws both from one table and
	 * requires only that SOME window came out non-empty, so it would agree
	 * with a version that scaled both halves from the left ratio -- the
	 * x87 sequence is `fmul %st,%st(1)` then `fmulp %st,%st(2)`, two
	 * different destinations one instruction apart, and getting the second
	 * wrong squares the first product instead of scaling the second ratio.
	 * 0.5 * 16 truncates to 8 and 0.05 * 16 truncates to 0, so the pair is
	 * (8, 0) one way round and (0, 8) the other.
	 */
	{
		static const float lr[4][2] = {
			{ 0.5f, 0.05f }, { 0.05f, 0.5f },
			{ 0.25f, 0.5f }, { 0.5f, 0.25f }
		};
		int k;

		for (k = 0; k < 4; k++) {
			long tk = 780000 + k;

			seed(tk);
			fill_arena(tk);
			wire(&ours.o);
			wire(&theirs.o);
			ours.o.linearEquLength = theirs.o.linearEquLength = 16;
			ours.o.word_1c = theirs.o.word_1c = 24;
			ours.o.dfeLength = theirs.o.dfeLength = 8;
			ours.o.mmxArraysPresent =
			    theirs.o.mmxArraysPresent = 0;
			ARENA_PARAMS->LINEAR_EQU_FADE_LEFT_EDGE_RATIO =
			    lr[k][0];
			ARENA_PARAMS->LINEAR_EQU_FADE_RIGHT_EDGE_RATIO =
			    lr[k][1];
			ARENA_PARAMS->ERROR_ENERGY_MEAN_BLOCK_LEN = 7;
			ARENA_PARAMS->ERROR_ENERGY_MEAN_K = 0.5f;

			memcpy(&arena_save, &arena, sizeof(arena));
			ours.o.reset(3);
			memcpy(&arena_ours, &arena, sizeof(arena));
			memcpy(&arena, &arena_save, sizeof(arena));
			ref_equ_reset(&theirs.o, 3);

			diff_eq_obj("after reset (asymmetric ratios)",
				    V90Equalizer, &ours.o, &theirs.o, tk);
			diff_eq_obj("the arena (asymmetric ratios)",
				    struct equ_arena, &arena_ours, &arena, tk);
			diff_eq_int("linearEquWindowHalf (%ld)",
				    (long)theirs.o.linearEquWindowHalf,
				    (long)(unsigned int)(lr[k][0] * 16.0f),
				    tk);
			diff_eq_int("dfeWindowHalf (%ld)",
				    (long)theirs.o.dfeWindowHalf,
				    (long)(unsigned int)(lr[k][1] * 16.0f),
				    tk);
		}
	}

	return diff_end();
}

/*
 * enterChannelVerification.  The one thing it does that `enterPhase3` does
 * not is call `V90Resampler::setBllState(V90_BLL_PRE_ANSPCM, 1)`, so the
 * resampler is a real object in the arena and the check that the call
 * happened is that the RESAMPLER moved: its state, its sample counter and its
 * two loop gains.  The early-out state is swept as well, and in that arm
 * nothing anywhere may move.
 */
static int
run_enterchannelverification(void)
{
	long tag = 800000;
	int state, entered = 0, skipped = 0;

	diff_begin("V90Equalizer::enterChannelVerification");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (state = -1; state <= 7; state++) {
		int bll;

		for (bll = 0; bll < 3; bll++) {
			unsigned char before[SLOT];

			tag++;
			seed(tag);
			fill_arena(tag);
			wire(&ours.o);
			wire(&theirs.o);
			ours.o.state = theirs.o.state = state;

			ARENA_RSAMP->params = ARENA_PARAMS;
			ARENA_RSAMP->bllState = (V90BllState)
			    (bll == 0 ? V90_BLL_FROZEN
				      : bll == 1 ? V90_BLL_STEADY_STATE
						 : V90_BLL_PRE_ANSPCM);
			ARENA_RSAMP->stateSamples = 0x11223344u;
			ARENA_RSAMP->countStateSamples = 0x55667788u;

			memcpy(before, ours.raw, SLOT);
			memcpy(&arena_save, &arena, sizeof(arena));
			dsplib_debug_capture_reset();

			ours.o.enterChannelVerification();

			memcpy(&arena_ours, &arena, sizeof(arena));
			memcpy(&arena, &arena_save, sizeof(arena));

			ref_equ_enterChannelVerification(&theirs.o);

			diff_eq_obj("after enterChannelVerification",
				    V90Equalizer, &ours.o, &theirs.o, tag);
			diff_eq_obj("the arena after "
				    "enterChannelVerification",
				    struct equ_arena, &arena_ours, &arena,
				    tag);
			diff_eq_int("no store past the object (%ld)",
				    guard_equal(), 1, tag);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			if (state == V90EQU_STATE_CHANNEL_VERIFY) {
				diff_eq_int("re-entry touched the object "
					    "(%ld)",
					    memcmp(before, ours.raw, SLOT)
					    == 0, 1, tag);
				diff_eq_int("re-entry touched the arena "
					    "(%ld)",
					    memcmp(&arena_save, &arena,
						   sizeof(arena)) == 0, 1,
					    tag);
				skipped = 1;
			} else {
				diff_eq_int("state (%ld)", theirs.o.state,
					    V90EQU_STATE_CHANNEL_VERIFY, tag);
				diff_eq_int("stateCount (%ld)",
					    theirs.o.stateCount, 0, tag);
				diff_eq_int("linearEquBeta zeroed (%ld)",
					    theirs.o.linearEquBeta == 0.0f, 1,
					    tag);
				diff_eq_int("dfeBeta zeroed (%ld)",
					    theirs.o.dfeBeta == 0.0f, 1, tag);
				/*
				 * The resampler was told, and told the RIGHT
				 * thing: 11 is V90_BLL_PRE_ANSPCM and the
				 * count is 1.  Without this the call could
				 * be missing entirely and every check above
				 * would still pass.
				 */
				diff_eq_int("the resampler's state (%ld)",
					    (long)ARENA_RSAMP->bllState,
					    (long)V90_BLL_PRE_ANSPCM, tag);
				if (bll != 2)
					diff_eq_int("the resampler's sample "
						    "count (%ld)",
						    (long)ARENA_RSAMP
						    ->countStateSamples, 1,
						    tag);
				entered = 1;
			}
		}
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("enterChannelVerification ran its body", entered, 1, 0);
	diff_eq_int("and skipped it when already verifying", skipped, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	build_betas();

	rc |= run_setters();
	rc |= run_exact_powers();
	rc |= run_enterphase3();
	rc |= run_reset();
	rc |= run_enterchannelverification();

	return rc;
}
