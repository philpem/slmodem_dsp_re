/*
 * t_v90equ.cpp -- differential test of V90Equalizer::setLinearEquBeta,
 * ::setDfeBeta, ::enterPhase3, ::reset, ::enterChannelVerification, and the
 * constructor and destructor.
 *
 * The fixture is t_v90jd.cpp's: the object lives in a byte array carried by a
 * union for its alignment, both sides are seeded with the SAME varied
 * pseudorandom bytes and never with zeros, the whole object is compared with
 * `diff_eq_obj`, and the bytes from `sizeof` to the end of an over-large slot
 * are compared separately so a store past the object's end is a failure
 * rather than silence (findings F223, F224, F230).  The lifecycle pair needs
 * more than that and finding F1234 is what it needs; see the comment above
 * `run_ctor`.
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
 * sides' levels raised, and the two transcripts are compared (finding F134's
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

/*
 * The object, plus room past its end to catch a store that overruns it.
 *
 * THE UNION NO LONGER HAS THE OBJECT IN IT.  `V90Equalizer` has a
 * user-declared constructor and destructor as of the lifecycle batch, so a
 * union holding one has both of its own implicitly deleted and the fixture
 * stops compiling.  The union stays for its ALIGNMENT and the object is
 * reached through a cast -- which is the same shape the constructor test
 * needs anyway, since C++ has no syntax for running a constructor over
 * storage that already exists and already holds a seed.
 */
#define SLOT 400

union equ_slot {
	unsigned char raw[SLOT];
	double align_;
};

static union equ_slot ours, theirs;

#define OURS	(*(V90Equalizer *)ours.raw)
#define THEIRS	(*(V90Equalizer *)theirs.raw)

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

				OURS.mmxMode = THEIRS.mmxMode = mmx;
				OURS.maxLeCoefValue =
				    THEIRS.maxLeCoefValue = ref_v[ri];
				OURS.maxDfeCoefValue =
				    THEIRS.maxDfeCoefValue = ref_v[ri];
				OURS.linearEquMmxConversionFactor =
				    THEIRS.linearEquMmxConversionFactor =
					scale_v[si];
				OURS.dfeMmxConversionFactor =
				    THEIRS.dfeMmxConversionFactor = scale_v[si];

				/*
				 * `prev` drives both arms of the diagnostic's
				 * guard: with it set the field already holds
				 * the value, so the object must not print.
				 */
				OURS.linearEquBeta =
				    THEIRS.linearEquBeta =
					prev ? beta : -7.5f;
				OURS.dfeBeta = THEIRS.dfeBeta =
					prev ? beta : -7.5f;

				memcpy(before, ours.raw, SLOT);
				if (level > 1)
					dsplib_debug_capture_reset();

				if (which == 0) {
					OURS.setLinearEquBeta(beta);
					ref_setLinearEquBeta(&THEIRS, beta);
				} else {
					OURS.setDfeBeta(beta);
					ref_setDfeBeta(&THEIRS, beta);
				}

				diff_eq_obj("after setBeta", V90Equalizer,
					    &OURS, &THEIRS, tag);
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

				shift = which == 0 ? OURS.linearEquMmxShift
						   : OURS.dfeMmxShift;
				if (mmx) {
					note_shift(out, shift);
					if ((which == 0
					     ? OURS.linearEquMmxBeta
					     : OURS.dfeMmxBeta) != 0)
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
		OURS.mmxMode = THEIRS.mmxMode = 1;
		OURS.maxLeCoefValue =
		    THEIRS.maxLeCoefValue = 1.0f;
		OURS.maxDfeCoefValue = THEIRS.maxDfeCoefValue = 1.0f;
		OURS.linearEquMmxConversionFactor =
		    THEIRS.linearEquMmxConversionFactor = 1.0f;
		OURS.dfeMmxConversionFactor = THEIRS.dfeMmxConversionFactor = 1.0f;
		OURS.linearEquBeta = THEIRS.linearEquBeta = -7.5f;
		OURS.dfeBeta = THEIRS.dfeBeta = -7.5f;

		OURS.setLinearEquBeta(pow2f(-24 - m));
		ref_setLinearEquBeta(&THEIRS, pow2f(-24 - m));
		diff_eq_obj("after setLinearEquBeta", V90Equalizer, &OURS,
			    &THEIRS, m);
		diff_eq_int("linear equaliser shift at 2**%ld",
			    OURS.linearEquMmxShift, m, m);

		OURS.setDfeBeta(pow2f(-20 - m));
		ref_setDfeBeta(&THEIRS, pow2f(-20 - m));
		diff_eq_obj("after setDfeBeta", V90Equalizer, &OURS,
			    &THEIRS, m);
		diff_eq_int("DFE shift at 2**%ld", OURS.dfeMmxShift, m, m);

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
				OURS.state = THEIRS.state = state;
				OURS.mmxMode = THEIRS.mmxMode = mmx;
				OURS.maxLeCoefValue =
				    THEIRS.maxLeCoefValue = ref_v[ri];
				OURS.maxDfeCoefValue =
				    THEIRS.maxDfeCoefValue = ref_v[ri];
				OURS.linearEquMmxConversionFactor =
				    THEIRS.linearEquMmxConversionFactor = 1.0f;
				OURS.dfeMmxConversionFactor =
				    THEIRS.dfeMmxConversionFactor = 1.0f;

				memcpy(before, ours.raw, SLOT);
				if (level > 1)
					dsplib_debug_capture_reset();

				OURS.enterPhase3();
				ref_enterPhase3(&THEIRS);

				diff_eq_obj("after enterPhase3", V90Equalizer,
					    &OURS, &THEIRS, tag);
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
						    OURS.state,
						    V90EQU_STATE_PHASE3, tag);
					diff_eq_int("stateCount (trial %ld)",
						    OURS.stateCount, 0, tag);
					diff_eq_int("linearEquBeta zeroed "
						    "(trial %ld)",
						    OURS.linearEquBeta
						    == 0.0f, 1, tag);
					diff_eq_int("dfeBeta zeroed "
						    "(trial %ld)",
						    OURS.dfeBeta == 0.0f, 1,
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
 * F1105's rule is that identical argument pointers give identical stored
 * pointers, and that is what makes `diff_eq_obj` usable on the object with no
 * field excluded.  But the arrays are OUTPUTS, and two writers into one buffer
 * would leave only the second one's work: so the arena is snapshotted, ours
 * runs, the result is copied away, the arena is restored, and the blob's runs
 * against the same starting bytes.  That is finding F805's shape applied to a
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
/*
 * Explicitly: this fixture takes `sizeof(V90Parameters)` for its arena, and
 * `V90Resampler.h` now only DECLARES the class -- it holds one as a pointer
 * and never dereferences it, so it no longer drags the 342-slot definition in
 * behind it.
 */
#include "dsplib/V90Parameters.h"

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

/* Varied bytes, never zeros (finding F230). */
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
				wire(&OURS);
				wire(&THEIRS);

				OURS.linearEquLength =
				    THEIRS.linearEquLength = len;
				OURS.word_1c = THEIRS.word_1c = m;
				OURS.dfeLength = THEIRS.dfeLength =
				    dfe_v[di];
				OURS.mmxArraysPresent =
				    THEIRS.mmxArraysPresent = mmx;

				ARENA_PARAMS->LINEAR_EQU_FADE_LEFT_EDGE_RATIO =
				    fade_v[fi];
				ARENA_PARAMS->LINEAR_EQU_FADE_RIGHT_EDGE_RATIO =
				    fade_v[(fi + 3) % NFADE];
				ARENA_PARAMS->ERROR_ENERGY_MEAN_BLOCK_LEN =
				    (int)(0x1234 + tag);
				ARENA_PARAMS->ERROR_ENERGY_MEAN_K = 0.375f;

				memcpy(&arena_save, &arena, sizeof(arena));
				dsplib_debug_capture_reset();

				OURS.reset(cursor);

				memcpy(&arena_ours, &arena, sizeof(arena));
				memcpy(&arena, &arena_save, sizeof(arena));

				ref_equ_reset(&THEIRS, cursor);

				diff_eq_obj("after reset", V90Equalizer,
					    &OURS, &THEIRS, tag);
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
				 * compare equal (finding F1105), so the values
				 * the object must hold are asserted and not
				 * only compared.
				 */
				diff_eq_int("state (%ld)", THEIRS.state,
					    V90EQU_STATE_RESET, tag);
				diff_eq_int("stateCount (%ld)",
					    THEIRS.stateCount, 0, tag);
				diff_eq_int("flag_144 (%ld)",
					    (long)THEIRS.flag_144, 1, tag);
				diff_eq_int("flag_146 (%ld)",
					    (long)THEIRS.flag_146, 1, tag);
				diff_eq_int("mmxMode (%ld)", THEIRS.mmxMode,
					    0, tag);
				diff_eq_int("word_20 = word_1c - len - 1 "
					    "(%ld)", (long)THEIRS.word_20,
					    (long)(unsigned int)(m - len - 1u),
					    tag);
				diff_eq_int("errorEnergyMeanBlockLen (%ld)",
					    THEIRS.errorEnergyMeanBlockLen,
					    (int)(0x1234 + tag), tag);
				diff_eq_int("errorEnergyMeanK copied (%ld)",
					    THEIRS.errorEnergyMeanK
					    == 0.375f, 1, tag);
				diff_eq_int("linearEquBeta zeroed (%ld)",
					    THEIRS.linearEquBeta == 0.0f, 1,
					    tag);
				diff_eq_int("dfeBeta zeroed (%ld)",
					    THEIRS.dfeBeta == 0.0f, 1, tag);

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
					    THEIRS.linearEquWindowHalf * 2u
					    <= len, 1, tag);
				if (THEIRS.linearEquWindowHalf > 0)
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
			wire(&OURS);
			wire(&THEIRS);
			OURS.linearEquLength = THEIRS.linearEquLength = 16;
			OURS.word_1c = THEIRS.word_1c = 24;
			OURS.dfeLength = THEIRS.dfeLength = 8;
			OURS.mmxArraysPresent =
			    THEIRS.mmxArraysPresent = 0;
			ARENA_PARAMS->LINEAR_EQU_FADE_LEFT_EDGE_RATIO =
			    lr[k][0];
			ARENA_PARAMS->LINEAR_EQU_FADE_RIGHT_EDGE_RATIO =
			    lr[k][1];
			ARENA_PARAMS->ERROR_ENERGY_MEAN_BLOCK_LEN = 7;
			ARENA_PARAMS->ERROR_ENERGY_MEAN_K = 0.5f;

			memcpy(&arena_save, &arena, sizeof(arena));
			OURS.reset(3);
			memcpy(&arena_ours, &arena, sizeof(arena));
			memcpy(&arena, &arena_save, sizeof(arena));
			ref_equ_reset(&THEIRS, 3);

			diff_eq_obj("after reset (asymmetric ratios)",
				    V90Equalizer, &OURS, &THEIRS, tk);
			diff_eq_obj("the arena (asymmetric ratios)",
				    struct equ_arena, &arena_ours, &arena, tk);
			diff_eq_int("linearEquWindowHalf (%ld)",
				    (long)THEIRS.linearEquWindowHalf,
				    (long)(unsigned int)(lr[k][0] * 16.0f),
				    tk);
			diff_eq_int("dfeWindowHalf (%ld)",
				    (long)THEIRS.dfeWindowHalf,
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
			wire(&OURS);
			wire(&THEIRS);
			OURS.state = THEIRS.state = state;

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

			OURS.enterChannelVerification();

			memcpy(&arena_ours, &arena, sizeof(arena));
			memcpy(&arena, &arena_save, sizeof(arena));

			ref_equ_enterChannelVerification(&THEIRS);

			diff_eq_obj("after enterChannelVerification",
				    V90Equalizer, &OURS, &THEIRS, tag);
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
				diff_eq_int("state (%ld)", THEIRS.state,
					    V90EQU_STATE_CHANNEL_VERIFY, tag);
				diff_eq_int("stateCount (%ld)",
					    THEIRS.stateCount, 0, tag);
				diff_eq_int("linearEquBeta zeroed (%ld)",
					    THEIRS.linearEquBeta == 0.0f, 1,
					    tag);
				diff_eq_int("dfeBeta zeroed (%ld)",
					    THEIRS.dfeBeta == 0.0f, 1, tag);
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

/* ================================================================ lifecycle */

/*
 * THE CONSTRUCTOR AND THE DESTRUCTOR.
 *
 * Both sides are driven through asm() labels, and both the C1 and the C2
 * variant of each: C++ has no syntax for running a constructor over storage
 * that already exists, and `OURS = V90Equalizer(...)` would build a temporary
 * over uninitialised stack and copy it in, throwing away the seed the whole
 * fixture rests on (findings F223, F224).  The blob holds C1 and C2 as two
 * identical copies at different addresses and our compiler emits one function
 * under both names, so both names are called or half the pair is untested.
 *
 * FIFTEEN OF THE FIELDS CANNOT BE COMPARED AND ARE NOT.  The constructor
 * takes fifteen `sysdep_malloc`s and the two sides allocate separately, so
 * fifteen pointer words -- and, when the fixed-point arrays are present, the
 * twelve derived words beside them -- hold different addresses for ever.
 * What is compared instead is everything those addresses stand for:
 *
 *   - the OBJECT with those words blanked, byte for byte (`cmp_equ`);
 *   - the CONTENTS of every block, paired by the field that points at it,
 *     over the exact length the object's own lengths imply -- so `reset`'s
 *     work inside them is compared even though the buffers are not the same
 *     buffers;
 *   - the SIZE of every block, from `malloc_usable_size`, per field;
 *   - the NUMBER of allocations and the exact number of BYTES ASKED FOR,
 *     which is the only thing that can see an allocation that is the wrong
 *     size in a way the contents do not reach;
 *   - the SKEW/ALIGNED relation, on each side separately, because
 *     `aligned = raw + 2 * skew` and `skew = (align8(raw) - raw) / 2` are
 *     statements about one side's own pointer that survive the addresses
 *     being different.
 *
 * With the fixed-point arrays ABSENT the twelve derived words are compared
 * rather than blanked, which is what says the constructor did not write them.
 */

#include <malloc.h>

#include "dsplib/modem_params.h"

extern "C" {
void equ_ctor1(void *self, unsigned int le, unsigned int dfe, void *p3d,
	       void *p4d, void *dem, void *ce, void *sv, void *parms,
	       void *rs, void *pf, int mode)
	asm("_ZN12V90EqualizerC1EjjP20V90Phase3DemodulatorP20V90Phase4Demodul"
	    "atorP11V90DemapperP22V90ConnectionEvaluatorP19V90SpectralVerifie"
	    "rP13V90ParametersP12V90ResamplerP12V90PreFilter20V90Computationa"
	    "lMode");
void equ_ctor2(void *self, unsigned int le, unsigned int dfe, void *p3d,
	       void *p4d, void *dem, void *ce, void *sv, void *parms,
	       void *rs, void *pf, int mode)
	asm("_ZN12V90EqualizerC2EjjP20V90Phase3DemodulatorP20V90Phase4Demodul"
	    "atorP11V90DemapperP22V90ConnectionEvaluatorP19V90SpectralVerifie"
	    "rP13V90ParametersP12V90ResamplerP12V90PreFilter20V90Computationa"
	    "lMode");
void ref_equ_ctor1(void *self, unsigned int le, unsigned int dfe, void *p3d,
		   void *p4d, void *dem, void *ce, void *sv, void *parms,
		   void *rs, void *pf, int mode)
	asm("ref__ZN12V90EqualizerC1EjjP20V90Phase3DemodulatorP20V90Phase4Dem"
	    "odulatorP11V90DemapperP22V90ConnectionEvaluatorP19V90SpectralVer"
	    "ifierP13V90ParametersP12V90ResamplerP12V90PreFilter20V90Computat"
	    "ionalMode");
void ref_equ_ctor2(void *self, unsigned int le, unsigned int dfe, void *p3d,
		   void *p4d, void *dem, void *ce, void *sv, void *parms,
		   void *rs, void *pf, int mode)
	asm("ref__ZN12V90EqualizerC2EjjP20V90Phase3DemodulatorP20V90Phase4Dem"
	    "odulatorP11V90DemapperP22V90ConnectionEvaluatorP19V90SpectralVer"
	    "ifierP13V90ParametersP12V90ResamplerP12V90PreFilter20V90Computat"
	    "ionalMode");

void equ_dtor1(void *self) asm("_ZN12V90EqualizerD1Ev");
void equ_dtor2(void *self) asm("_ZN12V90EqualizerD2Ev");
void ref_equ_dtor1(void *self) asm("ref__ZN12V90EqualizerD1Ev");
void ref_equ_dtor2(void *self) asm("ref__ZN12V90EqualizerD2Ev");

void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);
}

/*
 * The blocks, in the order the field map has them.  The first seven are taken
 * unconditionally; the last eight only when the fixed-point arrays are
 * wanted.  `len` is filled in per trial from the object's own lengths.
 */
struct equ_block {
	unsigned	off;		/* where the pointer lives   */
	const char	*name;
	unsigned	len;		/* bytes, this trial         */
	int		mmx;		/* only present when set     */
};

static struct equ_block block_v[] = {
	{ 0x014, "linearEquCoefs",		0, 0 },
	{ 0x018, "array_18",			0, 0 },
	{ 0x024, "linearEquWindow",		0, 0 },
	{ 0x028, "dfeWindow",			0, 0 },
	{ 0x040, "dfeCoefs",			0, 0 },
	{ 0x044, "array_44",			0, 0 },
	{ 0x098, "meanErrorEnergy",			0, 0 },
	{ 0x0b4, "block_b4",			0, 1 },
	{ 0x0b8, "block_b8",			0, 1 },
	{ 0x0d4, "linearEquMmxCoefs",		0, 1 },
	{ 0x0d8, "array_d8",			0, 1 },
	{ 0x0ec, "array_ec",			0, 1 },
	{ 0x114, "dfeMmxCoefs",			0, 1 },
	{ 0x118, "array_118",			0, 1 },
	{ 0x12c, "array_12c",			0, 1 }
};

#define NBLOCK ((int)(sizeof(block_v) / sizeof(block_v[0])))

/* The three raw/aligned/skew triples of each half, as (raw, aligned, skew). */
static const unsigned triple_v[6][3] = {
	{ 0x0d4, 0x0dc, 0x0e4 },
	{ 0x0d8, 0x0e0, 0x0e8 },
	{ 0x0ec, 0x0f0, 0x0f4 },
	{ 0x114, 0x11c, 0x124 },
	{ 0x118, 0x120, 0x128 },
	{ 0x12c, 0x130, 0x134 }
};

/* The words that can never agree: the fifteen pointers, and the twelve
 * derived from six of them.  Terminated by ~0u, t_resampler's idiom. */
static const unsigned skip_plain[] = {
	0x014, 0x018, 0x024, 0x028, 0x040, 0x044, 0x098, ~0u
};
static const unsigned skip_mmx[] = {
	0x014, 0x018, 0x024, 0x028, 0x040, 0x044, 0x098,
	0x0b4, 0x0b8, 0x0d4, 0x0d8, 0x0dc, 0x0e0, 0x0e4, 0x0e8,
	0x0ec, 0x0f0, 0x0f4, 0x114, 0x118, 0x11c, 0x120, 0x124, 0x128,
	0x12c, 0x130, 0x134, ~0u
};

static unsigned char scratch[2][SLOT];

static void *
ptr_at(int side, unsigned off)
{
	void *p;

	memcpy(&p, (side ? theirs.raw : ours.raw) + off, sizeof p);
	return p;
}

static unsigned
u32_at(int side, unsigned off)
{
	unsigned v;

	memcpy(&v, (side ? theirs.raw : ours.raw) + off, sizeof v);
	return v;
}

static void
cmp_equ(const char *what, const unsigned *skip, long trial)
{
	int i;

	memcpy(scratch[0], ours.raw, SLOT);
	memcpy(scratch[1], theirs.raw, SLOT);
	for (i = 0; skip[i] != ~0u; i++) {
		memset(scratch[0] + skip[i], 0, 4);
		memset(scratch[1] + skip[i], 0, 4);
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Equalizer", scratch[0],
		     scratch[1], sizeof(V90Equalizer), trial);
	diff_eq_int("no store past the object (%ld)",
		    memcmp(scratch[0] + sizeof(V90Equalizer),
			   scratch[1] + sizeof(V90Equalizer),
			   SLOT - sizeof(V90Equalizer)) == 0, 1, trial);
}

/* The parameter block and the host block the constructor reads. */
static unsigned char mp_block[sizeof(struct _tagModemParameters) + 32]
	__attribute__((aligned(8)));

#define ARENA_MP ((struct _tagModemParameters *)mp_block)

/* Six pointers the constructor stores and never dereferences. */
static unsigned char dummy_obj[6][8] __attribute__((aligned(8)));

static void
free_blocks(int side, int mmx)
{
	int i;

	for (i = 0; i < NBLOCK; i++) {
		void *p = ptr_at(side, block_v[i].off);

		if (block_v[i].mmx && !mmx)
			continue;
		if (p != 0)
			sysdep_free(p);
	}
}

static int
run_ctor(void)
{
	static const unsigned int le_v[]  = { 0u, 1u, 4u, 7u, 16u, 32u };
	static const unsigned int dfe_v[] = { 0u, 5u, 12u };
	static const int hist_v[]         = { 0, 33, 64 };
	static const int hw_v[]           = { 0, 1, 2, 3 };
	static const int mode_v[]         = { 1, 2 };
	long trial = 800000;
	int li, di, hi, wi, mi, mmxen, lvl, i;
	int saw_mmx = 0, saw_plain = 0, saw_mode1 = 0, saw_other = 0;

	diff_begin("V90Equalizer::V90Equalizer");

	dsplib_debug_capture_on = 1;

	for (li = 0; li < 6; li++)
	    for (di = 0; di < 3; di++)
		for (hi = 0; hi < 3; hi++)
		    for (wi = 0; wi < 4; wi++)
			for (mi = 0; mi < 2; mi++)
			    for (mmxen = 0; mmxen < 2; mmxen++)
				for (lvl = 0; lvl < 2; lvl++) {
					struct alloc_log a0, a1, a2;
					unsigned int lelen = le_v[li] & ~3u;
					unsigned int dfelen = dfe_v[di] & ~3u;
					unsigned int hist =
					    2u * (unsigned int)(hist_v[hi] / 2);
					int mmx;

					/*
					 * `reset` clears `array_18` from
					 * `word_1c` downwards over
					 * `linearEquLength` entries, so a
					 * history shorter than the equaliser
					 * would run off the front of it.  The
					 * blob does that too; it is not what
					 * this test is for.
					 */
					if (hist < lelen)
						continue;

					trial++;

					dsplibs_debug_level =
					    ref_dsplibs_debug_level =
						lvl ? 2u : 0u;

					seed(trial);
					fill_arena(trial);
					memset(mp_block, 0x5a,
					       sizeof(mp_block));

					ARENA_PARAMS->modemParams = ARENA_MP;
					ARENA_PARAMS->ENABLE_EQUALIZER_MMX =
					    mmxen;
					ARENA_PARAMS
					    ->LINEAR_EQU_HISTORY_LENGTH =
						hist_v[hi];
					ARENA_MP->unnamed_005c = hw_v[wi];
					ARENA_PARAMS
					    ->LINEAR_EQU_FADE_LEFT_EDGE_RATIO =
						0.25f;
					ARENA_PARAMS
					    ->LINEAR_EQU_FADE_RIGHT_EDGE_RATIO =
						0.5f;
					ARENA_PARAMS
					    ->ERROR_ENERGY_MEAN_BLOCK_LEN =
						(int)(0x1234 + trial);
					ARENA_PARAMS->ERROR_ENERGY_MEAN_K =
					    0.375f;

					mmx = mmxen &&
					    (mode_v[mi] == V90EQU_COMP_MODE_1
					     ? hw_v[wi] != 2 : hw_v[wi] == 1);

					dsplib_debug_capture_reset();

					a0 = harness_alloc;
					if (trial & 1)
						equ_ctor1(ours.raw, le_v[li],
							  dfe_v[di],
							  dummy_obj[0],
							  dummy_obj[1],
							  dummy_obj[2],
							  dummy_obj[3],
							  dummy_obj[4],
							  ARENA_PARAMS,
							  ARENA_RSAMP,
							  dummy_obj[5],
							  mode_v[mi]);
					else
						equ_ctor2(ours.raw, le_v[li],
							  dfe_v[di],
							  dummy_obj[0],
							  dummy_obj[1],
							  dummy_obj[2],
							  dummy_obj[3],
							  dummy_obj[4],
							  ARENA_PARAMS,
							  ARENA_RSAMP,
							  dummy_obj[5],
							  mode_v[mi]);
					a1 = harness_alloc;
					if (trial & 1)
						ref_equ_ctor1(theirs.raw,
							      le_v[li],
							      dfe_v[di],
							      dummy_obj[0],
							      dummy_obj[1],
							      dummy_obj[2],
							      dummy_obj[3],
							      dummy_obj[4],
							      ARENA_PARAMS,
							      ARENA_RSAMP,
							      dummy_obj[5],
							      mode_v[mi]);
					else
						ref_equ_ctor2(theirs.raw,
							      le_v[li],
							      dfe_v[di],
							      dummy_obj[0],
							      dummy_obj[1],
							      dummy_obj[2],
							      dummy_obj[3],
							      dummy_obj[4],
							      ARENA_PARAMS,
							      ARENA_RSAMP,
							      dummy_obj[5],
							      mode_v[mi]);
					a2 = harness_alloc;

					cmp_equ("after the constructor",
						mmx ? skip_mmx : skip_plain,
						trial);

					/* The lengths, asserted not compared. */
					diff_eq_int("linearEquLength (%ld)",
						    (long)THEIRS.linearEquLength,
						    (long)lelen, trial);
					diff_eq_int("dfeLength (%ld)",
						    (long)THEIRS.dfeLength,
						    (long)dfelen, trial);
					diff_eq_int("word_1c (%ld)",
						    (long)THEIRS.word_1c,
						    (long)hist, trial);
					diff_eq_int("mmxArraysPresent (%ld)",
						    THEIRS.mmxArraysPresent,
						    mmx, trial);
					diff_eq_int("params (%ld)",
						    (void *)THEIRS.params ==
						    (void *)ARENA_PARAMS, 1,
						    trial);
					diff_eq_int("resampler (%ld)",
						    (void *)THEIRS.resampler ==
						    (void *)ARENA_RSAMP, 1,
						    trial);
					diff_eq_int("phase3Demod (%ld)",
						    (void *)THEIRS.phase3Demod
						    == (void *)dummy_obj[0], 1,
						    trial);
					diff_eq_int("phase4Demod (%ld)",
						    (void *)THEIRS.phase4Demod
						    == (void *)dummy_obj[1], 1,
						    trial);
					diff_eq_int("demapper (%ld)",
						    (void *)THEIRS.demapper ==
						    (void *)dummy_obj[2], 1,
						    trial);
					diff_eq_int("connEval (%ld)",
						    (void *)THEIRS.connEval ==
						    (void *)dummy_obj[3], 1,
						    trial);
					diff_eq_int("spectralVerifier (%ld)",
						    (void *)THEIRS
						    .spectralVerifier ==
						    (void *)dummy_obj[4], 1,
						    trial);
					diff_eq_int("preFilter (%ld)",
						    (void *)THEIRS.preFilter ==
						    (void *)dummy_obj[5], 1,
						    trial);
					diff_eq_int("reset ran (%ld)",
						    THEIRS.state,
						    V90EQU_STATE_RESET, trial);

					/* The allocator's view. */
					diff_eq_int("allocations (%ld)",
						    a1.allocs - a0.allocs,
						    a2.allocs - a1.allocs,
						    trial);
					diff_eq_int("bytes asked for (%ld)",
						    (long)(a1.bytes - a0.bytes),
						    (long)(a2.bytes - a1.bytes),
						    trial);
					diff_eq_int("how many blocks (%ld)",
						    a1.allocs - a0.allocs,
						    mmx ? 15 : 7, trial);

					/* Every block, paired by its field. */
					block_v[0].len = lelen * 4;
					block_v[1].len = hist * 4;
					block_v[2].len = lelen * 4;
					block_v[3].len = lelen * 4;
					block_v[4].len = dfelen * 4;
					block_v[5].len = dfelen * 4;
					block_v[6].len = 0x4b0;
					block_v[7].len = 0x400;
					block_v[8].len = 0x200;
					block_v[9].len = (lelen + 8) * 2;
					block_v[10].len = (lelen + 8) * 2;
					block_v[11].len = (hist + 8) * 2;
					block_v[12].len = (dfelen + 8) * 2;
					block_v[13].len = (dfelen + 8) * 2;
					block_v[14].len = (dfelen + 8) * 2;

					for (i = 0; i < NBLOCK; i++) {
						void *pa, *pb;

						if (block_v[i].mmx && !mmx)
							continue;
						pa = ptr_at(0, block_v[i].off);
						pb = ptr_at(1, block_v[i].off);
						diff_eq_int("a block was "
							    "allocated (%ld)",
							    pa != 0 && pb != 0,
							    1, trial);
						if (pa == 0 || pb == 0)
							continue;
						diff_eq_int("block contents "
							    "(%ld)",
							    memcmp(pa, pb,
								   block_v[i]
								   .len) == 0,
							    1, trial);
						/*
						 * The size ASKED for, not
						 * malloc_usable_size: that
						 * reports the chunk the
						 * request was served from,
						 * and two identical requests
						 * differ whenever one was
						 * carved from the top and the
						 * other recycled something
						 * larger.  It read 132
						 * against 140 for two equal
						 * allocations.  Finding F1353.
						 */
						diff_eq_int("block size (%ld)",
							    (long)
							    harness_alloc_reqsize(
								pa),
							    (long)
							    harness_alloc_reqsize(
								pb), trial);
					}

					/*
					 * The alignment triples, each checked
					 * against its OWN side's pointer --
					 * the one statement about them that
					 * two different addresses can both
					 * satisfy.
					 */
					if (mmx) {
						int side;

						for (side = 0; side < 2; side++)
						    for (i = 0; i < 6; i++) {
							unsigned long raw =
							    (unsigned long)
							    ptr_at(side,
								   triple_v[i][0]);
							unsigned long al =
							    (unsigned long)
							    ptr_at(side,
								   triple_v[i][1]);
							unsigned skew =
							    u32_at(side,
								   triple_v[i][2]);

							diff_eq_int("skew "
								    "(%ld)",
								    (long)skew,
								    (long)
								    ((((raw + 7)
								       & ~7ul) -
								      raw) / 2),
								    trial);
							diff_eq_int("aligned "
								    "(%ld)",
								    al == raw +
								    2 * skew, 1,
								    trial);
						    }
					}

					diff_eq_int("transcript (%ld)",
						    strcmp(dsplib_debug_capture_text(0),
							   dsplib_debug_capture_text(1))
						    == 0, 1, trial);

					if (mmx)
						saw_mmx = 1;
					else
						saw_plain = 1;
					if (mode_v[mi] == V90EQU_COMP_MODE_1)
						saw_mode1 = 1;
					else
						saw_other = 1;

					/* Give it all back, both sides. */
					free_blocks(0, mmx);
					free_blocks(1, mmx);
					diff_eq_int("nothing wild was freed "
						    "(%ld)",
						    harness_alloc.bad_free -
						    a0.bad_free, 0, trial);
					diff_eq_int("live is back (%ld)",
						    harness_alloc.live,
						    a0.live, trial);
				}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the fixed-point arrays were taken", saw_mmx, 1, 0);
	diff_eq_int("and skipped", saw_plain, 1, 0);
	diff_eq_int("mode 1 was seen", saw_mode1, 1, 0);
	diff_eq_int("and another mode", saw_other, 1, 0);

	return diff_end();
}

/*
 * The destructor, one slot at a time.
 *
 * Every one of the fifteen pointers is set to NULL except the one under test,
 * which gets a real block on each side, and the number of frees says whether
 * that slot was freed.  A count is enough to name the slot because only one
 * slot is ever non-null, and it is the only shape that can tell "freed the
 * right seven" from "freed seven things".  The mmx flag runs both ways, so
 * the eight gated slots are seen both freed and skipped.
 */
static int
run_dtor(void)
{
	long trial = 850000;
	int k, mmx, which, i, side;
	int saw_freed = 0, saw_skipped = 0;

	diff_begin("V90Equalizer::~V90Equalizer");

	for (k = 0; k <= NBLOCK; k++)
	    for (mmx = 0; mmx < 2; mmx++)
		for (which = 0; which < 2; which++) {
			struct alloc_log a0, a1, a2;
			unsigned char before[SLOT], before_ours[SLOT];
			int want;

			trial++;
			seed(trial);

			for (i = 0; i < NBLOCK; i++)
				for (side = 0; side < 2; side++) {
					void *p = 0;

					if (k == NBLOCK || i == k)
						p = sysdep_malloc(32);
					memcpy((side ? theirs.raw : ours.raw) +
					       block_v[i].off, &p, sizeof p);
				}

			OURS.mmxArraysPresent = THEIRS.mmxArraysPresent = mmx;
			memcpy(before, theirs.raw, SLOT);
			memcpy(before_ours, ours.raw, SLOT);

			want = 0;
			for (i = 0; i < NBLOCK; i++) {
				if (k != NBLOCK && i != k)
					continue;
				if (block_v[i].mmx && !mmx)
					continue;
				want++;
			}

			a0 = harness_alloc;
			if (which)
				equ_dtor1(ours.raw);
			else
				equ_dtor2(ours.raw);
			a1 = harness_alloc;
			if (which)
				ref_equ_dtor1(theirs.raw);
			else
				ref_equ_dtor2(theirs.raw);
			a2 = harness_alloc;

			diff_eq_int("frees (%ld)", a1.frees - a0.frees,
				    a2.frees - a1.frees, trial);
			diff_eq_int("free(NULL) (%ld)",
				    a1.free_null - a0.free_null,
				    a2.free_null - a1.free_null, trial);
			diff_eq_int("wild frees (%ld)",
				    a1.bad_free - a0.bad_free,
				    a2.bad_free - a1.bad_free, trial);
			diff_eq_int("freed exactly the right slots (%ld)",
				    a1.frees - a0.frees, want, trial);
			diff_eq_int("and never freed a null (%ld)",
				    a2.free_null - a0.free_null, 0, trial);
			diff_eq_int("the object is untouched (%ld)",
				    memcmp(before, theirs.raw, SLOT) == 0, 1,
				    trial);
			/*
			 * NEITHER object is written -- the destructor does
			 * not clear the pointers it frees, which is why
			 * calling it twice frees every one of them again.
			 * Each side is compared against its OWN state
			 * before the call: the fifteen slots hold different
			 * addresses on the two sides, so a comparison
			 * ACROSS the sides would fail here for a reason
			 * that has nothing to do with the destructor.
			 */
			diff_eq_int("and ours is untouched too (%ld)",
				    memcmp(before_ours, ours.raw, SLOT) == 0,
				    1, trial);

			if (want != 0)
				saw_freed = 1;
			if (k != NBLOCK && block_v[k].mmx && !mmx)
				saw_skipped = 1;

			/* Whatever it did not free, free here. */
			for (i = 0; i < NBLOCK; i++) {
				if (k != NBLOCK && i != k)
					continue;
				if (!(block_v[i].mmx && !mmx))
					continue;
				for (side = 0; side < 2; side++) {
					void *p = ptr_at(side,
							 block_v[i].off);

					if (p != 0)
						sysdep_free(p);
				}
			}
		}

	diff_eq_int("a block was freed", saw_freed, 1, 0);
	diff_eq_int("a gated block was skipped", saw_skipped, 1, 0);

	return diff_end();
}

/* ======================================================= the coefficient set */

/*
 * HALF OF THIS BATCH WRITES NOTHING INSIDE THE OBJECT.  `setLinearEquCoeff`
 * stores through `this->linearEquCoefs`; `zeroDfeCoefs` through +0x40, +0x114
 * and +0x118; `linearEquFadeEdges` and `restoreEqualizerToFloat` read one set
 * of arrays and write another.  An object-only `diff_eq_obj` reports "equal"
 * for every one of them even if the body were empty, so each test below
 * compares the ARENA as well -- the same snapshot-and-restore `run_reset` uses,
 * because two writers into one buffer would otherwise leave only the second's
 * work.
 *
 * AND THE ALIGNED POINTERS ARE SKEWED BY ONE ENTRY.  `wire_mmx` points each
 * of the six `*Aligned` slots one SHORT past its raw array, so a version that
 * reached for the raw pointer where the object reaches for the aligned one
 * reads and writes different entries and the arena comparison sees it.  The
 * skew is real: the constructor's `(align8(p) - p) / 2` is 0, 1, 2 or 3.
 */

extern "C" {
float ref_equ_getDfeBeta(void *self)
	asm("ref__ZN12V90Equalizer10getDfeBetaEv");
void ref_equ_resetMeanErrorEnergyDiagnostics(void *self)
	asm("ref__ZN12V90Equalizer31resetMeanErrorEnergyDiagnosticsEv");
void ref_equ_setLinearEquCoeff(void *self, float *src, unsigned int n)
	asm("ref__ZN12V90Equalizer17setLinearEquCoeffEPfj");
void ref_equ_setDfeCoeff(void *self, float *src, unsigned int n)
	asm("ref__ZN12V90Equalizer11setDfeCoeffEPfj");
void ref_equ_zeroLinearEquCoefs(void *self)
	asm("ref__ZN12V90Equalizer18zeroLinearEquCoefsEv");
void ref_equ_zeroDfeCoefs(void *self)
	asm("ref__ZN12V90Equalizer12zeroDfeCoefsEv");
void ref_equ_setLinearEquEdgesFadingParams(void *self, float l, float r)
	asm("ref__ZN12V90Equalizer29setLinearEquEdgesFadingParamsEff");
void ref_equ_freeze(void *self)
	asm("ref__ZN12V90Equalizer6freezeEv");
void ref_equ_restoreEqualizerToFloat(void *self)
	asm("ref__ZN12V90Equalizer23restoreEqualizerToFloatEv");
void ref_equ_linearEquFadeEdges(void *self)
	asm("ref__ZN12V90Equalizer18linearEquFadeEdgesEv");
void ref_equ_printCoefsToFile(const void *self)
	asm("ref__ZNK12V90Equalizer16printCoefsToFileEv");
void ref_equ_loadCoefsFromFile(void *self)
	asm("ref__ZN12V90Equalizer17loadCoefsFromFileEv");
void ref_equ_printEquStuff(void *self)
	asm("ref__ZN12V90Equalizer13printEquStuffEv");
}

/* One SHORT of skew on every aligned view, so raw and aligned cannot pass
 * for one another. */
static void
wire_mmx(V90Equalizer *o)
{
	o->linearEquMmxCoefsAligned = arena.lemmx + 1;
	o->array_d8Aligned = arena.ad8 + 1;
	o->array_ecAligned = arena.aec + 1;
	o->dfeMmxCoefsAligned = arena.dfemmx + 1;
	o->array_118Aligned = arena.a118 + 1;
	o->array_12cAligned = arena.a12c + 1;
	o->linearEquMmxCoefsSkew = o->array_d8Skew = o->array_ecSkew = 1;
	o->dfeMmxCoefsSkew = o->array_118Skew = o->array_12cSkew = 1;
}

/* The two sides run against the same starting arena, one after the other. */
static void
arena_snapshot(void)
{
	memcpy(&arena_save, &arena, sizeof(arena));
}

static void
arena_switch(void)
{
	memcpy(&arena_ours, &arena, sizeof(arena));
	memcpy(&arena, &arena_save, sizeof(arena));
}

static void
arena_compare(const char *what, long tag)
{
	diff_eq_obj_(__FILE__, __LINE__, what, "struct equ_arena",
		     &arena_ours, &arena, sizeof(arena), tag);
}

/*
 * `getDfeBeta` is the one member whose only output is the return value, so
 * comparing the object would pass a body that returned the wrong field.  The
 * float is compared bit for bit, and the object is compared too, because
 * nothing may move.
 */
static int
run_getdfebeta(void)
{
	static const unsigned pat[] = {
		0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u,
		0x283424dcu, 0x7f800000u, 0x7fc00000u, 0x00000001u,
		0x12345678u
	};
	long tag = 900000;
	int i;

	diff_begin("V90Equalizer::getDfeBeta");

	for (i = 0; i < (int)(sizeof(pat) / sizeof(pat[0])); i++) {
		union { unsigned u; float f; } v;
		float got, want;

		tag++;
		seed(tag);
		v.u = pat[i];
		OURS.dfeBeta = THEIRS.dfeBeta = v.f;

		got = OURS.getDfeBeta();
		want = ref_equ_getDfeBeta(&THEIRS);

		diff_eq_int("the bits came back (%ld)",
			    (long)(*(unsigned *)&got == *(unsigned *)&want),
			    1, tag);
		diff_eq_obj("after getDfeBeta", V90Equalizer, &OURS, &THEIRS,
			    tag);
		diff_eq_int("no store past the object (%ld)", guard_equal(),
			    1, tag);
	}

	return diff_end();
}

/*
 * `resetMeanErrorEnergyDiagnostics` and the three one-byte members.  The
 * empty three are tested exactly as the others are: the object and the arena
 * must both come back untouched, which is what an empty body means and what a
 * body that did anything would fail.
 */
static int
run_smallmembers(void)
{
	long tag = 910000;
	int i;

	diff_begin("V90Equalizer::resetMeanErrorEnergyDiagnostics and the "
		   "empty three");

	for (i = 0; i < 4; i++) {
		tag++;
		seed(tag);
		fill_arena(tag);
		wire(&OURS);
		wire(&THEIRS);
		wire_mmx(&OURS);
		wire_mmx(&THEIRS);

		arena_snapshot();
		switch (i) {
		case 0:
			OURS.resetMeanErrorEnergyDiagnostics();
			break;
		case 1:
			OURS.printCoefsToFile();
			break;
		case 2:
			OURS.loadCoefsFromFile();
			break;
		default:
			OURS.printEquStuff();
			break;
		}
		arena_switch();
		switch (i) {
		case 0:
			ref_equ_resetMeanErrorEnergyDiagnostics(&THEIRS);
			break;
		case 1:
			ref_equ_printCoefsToFile(&THEIRS);
			break;
		case 2:
			ref_equ_loadCoefsFromFile(&THEIRS);
			break;
		default:
			ref_equ_printEquStuff(&THEIRS);
			break;
		}

		diff_eq_obj("after the call", V90Equalizer, &OURS, &THEIRS,
			    tag);
		arena_compare("the arena after the call", tag);
		diff_eq_int("no store past the object (%ld)", guard_equal(),
			    1, tag);

		if (i == 0) {
			diff_eq_int("meanErrorCount zeroed (%ld)",
				    (long)THEIRS.meanErrorCount, 0, tag);
			diff_eq_int("meanErrorFull zeroed (%ld)",
				    (long)THEIRS.meanErrorFull, 0, tag);
		} else {
			/* Nothing moved at all: the fill is still there. */
			diff_eq_int("the fill survived (%ld)",
				    (long)(THEIRS.meanErrorCount != 0), 1, tag);
		}
	}

	return diff_end();
}

/* The two coefficient loaders, swept over a count that runs past nothing. */
static int
run_setcoeff(void)
{
	static const unsigned int n_v[] = { 0u, 1u, 3u, 16u, 32u };
	static float src[32];
	long tag = 920000;
	int ni, which, k;
	int saw_copy = 0;

	diff_begin("V90Equalizer::setLinearEquCoeff / setDfeCoeff");

	for (which = 0; which < 2; which++)
	    for (ni = 0; ni < 5; ni++) {
		unsigned int n = n_v[ni];

		tag++;
		seed(tag);
		fill_arena(tag);
		wire(&OURS);
		wire(&THEIRS);
		wire_mmx(&OURS);
		wire_mmx(&THEIRS);

		for (k = 0; k < 32; k++)
			src[k] = (float)(k + 1) * 0.25f - (float)which;

		arena_snapshot();
		if (which == 0)
			OURS.setLinearEquCoeff(src, n);
		else
			OURS.setDfeCoeff(src, n);
		arena_switch();
		if (which == 0)
			ref_equ_setLinearEquCoeff(&THEIRS, src, n);
		else
			ref_equ_setDfeCoeff(&THEIRS, src, n);

		diff_eq_obj("after the loader", V90Equalizer, &OURS, &THEIRS,
			    tag);
		arena_compare("the arena after the loader", tag);
		diff_eq_int("no store past the object (%ld)", guard_equal(),
			    1, tag);

		if (n > 0) {
			const float *dst = which == 0 ? arena.lecoefs
						      : arena.dfecoefs;

			saw_copy = 1;
			diff_eq_float("the first entry copied", dst[0],
				      src[0], tag);
			diff_eq_float("the last entry copied", dst[n - 1],
				      src[n - 1], tag);
		}
	    }

	diff_eq_int("something was copied", saw_copy, 1, 0);

	return diff_end();
}

/* The two clearers, over both modes and both lengths. */
static int
run_zerocoefs(void)
{
	static const unsigned int len_v[] = { 0u, 1u, 4u, 24u };
	long tag = 930000;
	int li, which, mmx;
	int saw_mmx = 0, saw_plain = 0;

	diff_begin("V90Equalizer::zeroLinearEquCoefs / zeroDfeCoefs");

	for (which = 0; which < 2; which++)
	    for (li = 0; li < 4; li++)
		for (mmx = 0; mmx < 2; mmx++) {
			unsigned int len = len_v[li];

			tag++;
			seed(tag);
			fill_arena(tag);
			wire(&OURS);
			wire(&THEIRS);
			wire_mmx(&OURS);
			wire_mmx(&THEIRS);

			OURS.linearEquLength = THEIRS.linearEquLength = len;
			OURS.dfeLength = THEIRS.dfeLength = len;
			OURS.mmxMode = THEIRS.mmxMode = mmx;

			arena_snapshot();
			if (which == 0)
				OURS.zeroLinearEquCoefs();
			else
				OURS.zeroDfeCoefs();
			arena_switch();
			if (which == 0)
				ref_equ_zeroLinearEquCoefs(&THEIRS);
			else
				ref_equ_zeroDfeCoefs(&THEIRS);

			diff_eq_obj("after the clear", V90Equalizer, &OURS,
				    &THEIRS, tag);
			arena_compare("the arena after the clear", tag);
			diff_eq_int("no store past the object (%ld)",
				    guard_equal(), 1, tag);

			if (len > 0) {
				const float *f = which == 0 ? arena.lecoefs
							    : arena.dfecoefs;

				diff_eq_int("the float array was cleared "
					    "(%ld)", (long)(f[len - 1] == 0.0f),
					    1, tag);
				if (mmx) {
					const short *s = which == 0
					    ? arena.lemmx : arena.dfemmx;

					saw_mmx = 1;
					diff_eq_int("the RAW fixed-point "
						    "array was cleared (%ld)",
						    (long)(s[0] == 0 &&
							   s[len + 7] == 0),
						    1, tag);
				} else {
					saw_plain = 1;
					diff_eq_int("and left alone without "
						    "the mode (%ld)",
						    (long)(arena.lemmx[0] != 0
							   || arena.dfemmx[0]
							      != 0),
						    1, tag);
				}
			}
		}

	diff_eq_int("the fixed-point arrays were cleared somewhere", saw_mmx,
		    1, 0);
	diff_eq_int("and skipped somewhere", saw_plain, 1, 0);

	return diff_end();
}

/*
 * setLinearEquEdgesFadingParams.  The same asymmetric pair `run_reset` uses,
 * because the sweep alone would agree with a version that scaled both halves
 * from the left ratio.
 */
/* The object's clamp, restated here so the expected value is not computed by
 * the code under test.  NaN and negative zero both come out as 0.0f. */
static float
clampf(float x)
{
	if (!(x >= 0.0f))
		return 0.0f;
	if (!(x <= 0.5f))
		return 0.5f;
	return x;
}

static int
run_fadingparams(void)
{
	static const unsigned int len_v[] = { 0u, 4u, 16u, 32u };
	static const float lr[6][2] = {
		{ 0.5f, 0.05f }, { 0.05f, 0.5f }, { 0.25f, 0.5f },
		{ -1.0f, 0.7f }, { 0.0f, 0.0f }, { 37.0f, -0.0f }
	};
	long tag = 940000;
	int li, k;
	int saw_window = 0;

	diff_begin("V90Equalizer::setLinearEquEdgesFadingParams");

	for (li = 0; li < 4; li++)
	    for (k = 0; k < 6; k++) {
		unsigned int len = len_v[li];

		tag++;
		seed(tag);
		fill_arena(tag);
		wire(&OURS);
		wire(&THEIRS);
		OURS.linearEquLength = THEIRS.linearEquLength = len;

		arena_snapshot();
		OURS.setLinearEquEdgesFadingParams(lr[k][0], lr[k][1]);
		arena_switch();
		ref_equ_setLinearEquEdgesFadingParams(&THEIRS, lr[k][0],
						      lr[k][1]);

		diff_eq_obj("after the fading params", V90Equalizer, &OURS,
			    &THEIRS, tag);
		arena_compare("the arena after the fading params", tag);
		diff_eq_int("no store past the object (%ld)", guard_equal(),
			    1, tag);

		diff_eq_int("linearEquWindowHalf (%ld)",
			    (long)THEIRS.linearEquWindowHalf,
			    (long)(unsigned int)(clampf(lr[k][0])
						 * (float)len), tag);
		diff_eq_int("dfeWindowHalf (%ld)",
			    (long)THEIRS.dfeWindowHalf,
			    (long)(unsigned int)(clampf(lr[k][1])
						 * (float)len), tag);
		if (THEIRS.linearEquWindowHalf > 0)
			saw_window = 1;
	    }

	diff_eq_int("a non-empty window was built", saw_window, 1, 0);

	return diff_end();
}

/* freeze -- both setters with zero, and the transcript that proves it. */
static int
run_freeze(void)
{
	long tag = 950000;
	int bi, mmx;

	diff_begin("V90Equalizer::freeze");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (bi = 0; bi < nbeta; bi++)
		for (mmx = 0; mmx < 2; mmx++) {
			tag++;
			seed(tag);
			fill_arena(tag);
			wire(&OURS);
			wire(&THEIRS);

			OURS.linearEquBeta = THEIRS.linearEquBeta =
			    beta_v[bi];
			OURS.dfeBeta = THEIRS.dfeBeta = beta_v[bi];
			OURS.mmxMode = THEIRS.mmxMode = mmx;
			OURS.maxLeCoefValue =
			    THEIRS.maxLeCoefValue = 1.0f;
			OURS.maxDfeCoefValue = THEIRS.maxDfeCoefValue = 1.0f;
			OURS.linearEquMmxConversionFactor =
			    THEIRS.linearEquMmxConversionFactor = 32768.0f;
			OURS.dfeMmxConversionFactor = THEIRS.dfeMmxConversionFactor =
			    32768.0f;

			dsplib_debug_capture_reset();
			OURS.freeze();
			ref_equ_freeze(&THEIRS);

			diff_eq_obj("after freeze", V90Equalizer, &OURS,
				    &THEIRS, tag);
			diff_eq_int("no store past the object (%ld)",
				    guard_equal(), 1, tag);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("linearEquBeta is zero (%ld)",
				    (long)(THEIRS.linearEquBeta == 0.0f), 1,
				    tag);
			diff_eq_int("dfeBeta is zero (%ld)",
				    (long)(THEIRS.dfeBeta == 0.0f), 1, tag);
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}

/*
 * The fixed-point halves get planted by hand rather than left to the fill:
 * the high half must go negative and the low half must go above 0x7fff, which
 * is the pair `movswl`/`movzwl` exists for, and a fill that never produced
 * 0x8000 in a low half would let a sign-extending reading pass.
 */
static void
plant_mmx_words(unsigned int n, long tag)
{
	static const unsigned short hi_v[] = {
		0x0000u, 0xffffu, 0x8000u, 0x7fffu, 0x0001u, 0xfffeu
	};
	static const unsigned short lo_v[] = {
		0x0000u, 0x8000u, 0xffffu, 0x7fffu, 0x0001u, 0x1234u
	};
	unsigned int i;

	for (i = 0; i < n + 8 && i + 1 < 64; i++) {
		int a = (int)((i + (unsigned)tag) % 6);
		int b = (int)((i * 5u + (unsigned)tag) % 6);

		arena.lemmx[i + 1]  = (short)hi_v[a];
		arena.ad8[i + 1]    = (short)lo_v[b];
		arena.dfemmx[i + 1] = (short)hi_v[b];
		arena.a118[i + 1]   = (short)lo_v[a];
		arena.aec[i + 1]    = (short)hi_v[(a + b) % 6];
		arena.a12c[i + 1]   = (short)lo_v[(a + 2 * b) % 6];
	}
}

static int
run_restoretofloat(void)
{
	static const unsigned int len_v[] = { 0u, 1u, 5u, 20u };
	static const float scale_p[] = { 1.0f, 32768.0f, 0.5f, 1024.0f };
	long tag = 960000;
	int li, si, mmx;
	int saw_work = 0, saw_skip = 0;

	diff_begin("V90Equalizer::restoreEqualizerToFloat");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (li = 0; li < 4; li++)
	    for (si = 0; si < 4; si++)
		for (mmx = 0; mmx < 2; mmx++) {
			unsigned int len = len_v[li];

			tag++;
			seed(tag);
			fill_arena(tag);
			plant_mmx_words(len, tag);
			wire(&OURS);
			wire(&THEIRS);
			wire_mmx(&OURS);
			wire_mmx(&THEIRS);

			OURS.linearEquLength = THEIRS.linearEquLength = len;
			OURS.dfeLength = THEIRS.dfeLength = len;
			OURS.word_1c = THEIRS.word_1c = len;
			OURS.word_20Saved = THEIRS.word_20Saved =
			    0xa5a50000u + (unsigned)li;
			OURS.word_20 = THEIRS.word_20 = 0x5a5a1111u;
			OURS.mmxMode = THEIRS.mmxMode = mmx;
			OURS.linearEquMmxConversionFactor =
			    THEIRS.linearEquMmxConversionFactor = scale_p[si];
			OURS.dfeMmxConversionFactor = THEIRS.dfeMmxConversionFactor =
			    scale_p[(si + 2) % 4];

			arena_snapshot();
			dsplib_debug_capture_reset();
			OURS.restoreEqualizerToFloat();
			arena_switch();
			ref_equ_restoreEqualizerToFloat(&THEIRS);

			diff_eq_obj("after the restore", V90Equalizer, &OURS,
				    &THEIRS, tag);
			arena_compare("the arena after the restore", tag);
			diff_eq_int("no store past the object (%ld)",
				    guard_equal(), 1, tag);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			if (mmx) {
				saw_work = 1;
				diff_eq_int("the mode was cleared (%ld)",
					    (long)THEIRS.mmxMode, 0, tag);
				diff_eq_int("word_20 came back from +0xf8 "
					    "(%ld)", (long)THEIRS.word_20,
					    (long)(0xa5a50000u
						   + (unsigned)li), tag);
			} else {
				saw_skip = 1;
				diff_eq_int("nothing happened without the "
					    "mode (%ld)",
					    (long)(THEIRS.word_20
						   == 0x5a5a1111u), 1, tag);
			}
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the restore ran somewhere", saw_work, 1, 0);
	diff_eq_int("and was skipped somewhere", saw_skip, 1, 0);

	return diff_end();
}

/*
 * linearEquFadeEdges.
 *
 * `dfeWindowHalf` indexes DOWN from `linearEquLength`, in unsigned arithmetic
 * and with no bound of its own, so a zero-length equaliser with a non-empty
 * right window walks off the front of the array.  The sweep keeps
 * `dfeWindowHalf <= linearEquLength` for that reason; docs/deviations.md
 * carries the entry.
 */
static int
run_fadeedges(void)
{
	static const unsigned int len_v[] = { 1u, 4u, 20u, 40u };
	long tag = 970000;
	int li, hi_i, mmx;
	int saw_mmx = 0, saw_plain = 0, saw_overlap = 0;

	diff_begin("V90Equalizer::linearEquFadeEdges");

	for (li = 0; li < 4; li++)
	    for (hi_i = 0; hi_i < 5; hi_i++)
		for (mmx = 0; mmx < 2; mmx++) {
			unsigned int len = len_v[li];
			unsigned int lh, dh;
			unsigned int k;

			/* The last pair is both windows over the WHOLE
			 * array, which is what the two independent clamps
			 * permit and what tapers the middle twice. */
			lh = (hi_i == 0) ? 0u
			   : (hi_i == 1) ? 1u
			   : (hi_i == 2) ? len / 2u : len;
			dh = (hi_i == 0) ? len
			   : (hi_i == 1) ? len / 2u
			   : (hi_i == 2) ? 1u
			   : (hi_i == 3) ? 0u : len;
			if (lh + dh > len)
				saw_overlap = 1;

			tag++;
			seed(tag);
			fill_arena(tag);
			plant_mmx_words(len, tag);
			wire(&OURS);
			wire(&THEIRS);
			wire_mmx(&OURS);
			wire_mmx(&THEIRS);

			/* Coefficients and windows with a settled range, so
			 * the truncation back to 32 bits stays in an int. */
			for (k = 0; k < 64; k++) {
				arena.lecoefs[k] = (float)((int)k - 32)
				    * 0.03125f;
				arena.dfecoefs[k] = (float)((int)k - 16)
				    * 0.0625f;
				arena.lewin[k] = 0.5f
				    + (float)(k % 5) * 0.125f;
				arena.dfewin[k] = 0.25f
				    + (float)(k % 7) * 0.0625f;
			}

			OURS.linearEquLength = THEIRS.linearEquLength = len;
			OURS.dfeLength = THEIRS.dfeLength = len;
			OURS.linearEquWindowHalf =
			    THEIRS.linearEquWindowHalf = lh;
			OURS.dfeWindowHalf = THEIRS.dfeWindowHalf = dh;
			OURS.mmxMode = THEIRS.mmxMode = mmx;
			OURS.linearEquMmxConversionFactor =
			    THEIRS.linearEquMmxConversionFactor = 1024.0f;
			OURS.dfeMmxConversionFactor = THEIRS.dfeMmxConversionFactor =
			    256.0f;

			arena_snapshot();
			OURS.linearEquFadeEdges();
			arena_switch();
			ref_equ_linearEquFadeEdges(&THEIRS);

			diff_eq_obj("after the fade", V90Equalizer, &OURS,
				    &THEIRS, tag);
			arena_compare("the arena after the fade", tag);
			diff_eq_int("no store past the object (%ld)",
				    guard_equal(), 1, tag);

			if (mmx)
				saw_mmx = 1;
			else
				saw_plain = 1;

			/*
			 * Without the mode the first tap is the window
			 * times what was there; with it, it came out of the
			 * fixed-point pair first.  The right window reaches
			 * index 0 exactly when `dh >= len`, and then the tap
			 * is tapered TWICE -- which is the overlap the two
			 * independent clamps permit, asserted rather than
			 * only compared.
			 */
			if (!mmx && lh > 0 && dh < len)
				diff_eq_float("the first tap was tapered",
					      arena.lecoefs[0],
					      arena_save.lewin[0]
					      * arena_save.lecoefs[0], tag);
			if (!mmx && lh > 0 && dh >= len && len > 0)
				diff_eq_float("the first tap was tapered "
					      "twice", arena.lecoefs[0],
					      arena_save.dfewin[len - 1]
					      * (arena_save.lewin[0]
						 * arena_save.lecoefs[0]),
					      tag);
		}

	diff_eq_int("the fade ran in fixed-point mode", saw_mmx, 1, 0);
	diff_eq_int("and in float mode", saw_plain, 1, 0);
	diff_eq_int("the two windows overlapped somewhere", saw_overlap, 1, 0);

	return diff_end();
}

/* ================================================== enterRRN and enterFPE */

/*
 * The two 387-byte state entries need two more objects than anything before
 * them: the spectral verifier, for the one word at its +0x28 that selects the
 * German-PBX arm, and the pre-filter, for `isV90WithEia6()`.
 *
 * NEITHER IS IN THE ARENA, because neither is written -- `isV90WithEia6` is
 * `const` and +0x28 is only read -- so one static of each, shared by both
 * sides, is all that is needed and the stored pointers agree by construction.
 *
 * `isV90WithEia6` reads `refLoop` first and only touches `dataBase[codecType]`
 * when it is not negative, so `refLoop = -1` keeps the call inside the
 * fixture; the answer then comes from the parameter block's +0x500 word, which
 * the sweep drives to 6 and to something else.
 */

#include "dsplib/V90SpectralVerifier.h"
#include "dsplib/V90PreFilter.h"

extern "C" {
int ref_equ_enterRRN(void *self) asm("ref__ZN12V90Equalizer8enterRRNEv");
int ref_equ_enterFPE(void *self) asm("ref__ZN12V90Equalizer8enterFPEEv");
}

static unsigned char sv_block[sizeof(V90SpectralVerifier) + 32]
	__attribute__((aligned(8)));
static unsigned char pf_block[sizeof(V90PreFilter) + 32]
	__attribute__((aligned(8)));

#define ARENA_SV ((V90SpectralVerifier *)sv_block)
#define ARENA_PF ((V90PreFilter *)pf_block)

static int
run_enterrrnfpe(void)
{
	static const unsigned int sv_v[] = { 0u, 1u, 2u, 3u };
	long tag = 980000;
	int which, si, eia, mmx, state;
	int saw_pbx = 0, saw_eia = 0, saw_restore = 0, saw_early = 0;

	diff_begin("V90Equalizer::enterRRN / enterFPE");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (which = 0; which < 2; which++)
	    for (state = 3; state <= 6; state++)
		for (si = 0; si < 4; si++)
		    for (eia = 0; eia < 2; eia++)
			for (mmx = 0; mmx < 2; mmx++) {
				unsigned int len = 12;
				int got, want;
				int early = (state == (which == 0
						       ? V90EQU_STATE_RRN
						       : V90EQU_STATE_FPE));

				tag++;
				seed(tag);
				fill_arena(tag);
				plant_mmx_words(len, tag);
				wire(&OURS);
				wire(&THEIRS);
				wire_mmx(&OURS);
				wire_mmx(&THEIRS);

				memset(sv_block, 0, sizeof sv_block);
				memset(pf_block, 0, sizeof pf_block);
				ARENA_SV->word_28 = sv_v[si];
				ARENA_PF->refLoop = -1;
				ARENA_PF->params = ARENA_PARAMS;
				V90PW(ARENA_PARAMS)[0x500 / 4] = eia ? 6u : 3u;
				ARENA_PARAMS->GERMAN_PBX_DFE_TRN2D_FAST_BETA =
				    0.0009765625f;

				OURS.spectralVerifier =
				    THEIRS.spectralVerifier = ARENA_SV;
				OURS.preFilter = THEIRS.preFilter = ARENA_PF;
				OURS.state = THEIRS.state = state;
				OURS.stateCount = THEIRS.stateCount =
				    0x1a2b3c00 + state;
				OURS.linearEquLength =
				    THEIRS.linearEquLength = len;
				OURS.dfeLength = THEIRS.dfeLength = len;
				OURS.word_1c = THEIRS.word_1c = len;
				OURS.mmxMode = THEIRS.mmxMode = mmx;
				OURS.maxLeCoefValue =
				    THEIRS.maxLeCoefValue = 1.0f;
				OURS.maxDfeCoefValue =
				    THEIRS.maxDfeCoefValue = 1.0f;
				OURS.linearEquMmxConversionFactor =
				    THEIRS.linearEquMmxConversionFactor = 1024.0f;
				OURS.dfeMmxConversionFactor =
				    THEIRS.dfeMmxConversionFactor = 256.0f;

				arena_snapshot();
				dsplib_debug_capture_reset();
				got = which == 0 ? OURS.enterRRN()
						 : OURS.enterFPE();
				arena_switch();
				want = which == 0 ? ref_equ_enterRRN(&THEIRS)
						  : ref_equ_enterFPE(&THEIRS);

				diff_eq_int("the return value (%ld)",
					    (long)got, (long)want, tag);
				diff_eq_obj("after the entry", V90Equalizer,
					    &OURS, &THEIRS, tag);
				arena_compare("the arena after the entry",
					      tag);
				diff_eq_int("no store past the object (%ld)",
					    guard_equal(), 1, tag);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);
				diff_eq_int("the verifier is untouched (%ld)",
					    (long)ARENA_SV->word_28,
					    (long)sv_v[si], tag);

				if (early) {
					saw_early = 1;
					diff_eq_int("the early out returns 0 "
						    "(%ld)", (long)want, 0,
						    tag);
					diff_eq_int("and does nothing (%ld)",
						    (long)(THEIRS.mmxMode
							   == (int)mmx), 1,
						    tag);
					continue;
				}

				diff_eq_int("the state was entered (%ld)",
					    (long)THEIRS.state,
					    (long)(which == 0
						   ? V90EQU_STATE_RRN
						   : V90EQU_STATE_FPE), tag);
				/*
				 * `enterPhase3` and
				 * `enterChannelVerification` zero +0x64 in
				 * the instruction after they write +0x60.
				 * These two leave it alone, so the planted
				 * value has to still be there -- which
				 * `diff_eq_obj` cannot say, since a version
				 * that zeroed it on both sides would agree.
				 */
				diff_eq_int("stateCount is NOT reset (%ld)",
					    (long)THEIRS.stateCount,
					    (long)(0x1a2b3c00 + state), tag);

				if (mmx) {
					saw_restore = 1;
					diff_eq_int("out of fixed-point mode "
						    "(%ld)", (long)want, 1,
						    tag);
					diff_eq_int("and the mode is off "
						    "(%ld)",
						    (long)THEIRS.mmxMode, 0,
						    tag);
				} else {
					diff_eq_int("nothing to restore "
						    "(%ld)", (long)want, 0,
						    tag);
				}

				if (sv_v[si] == 2) {
					saw_pbx = 1;
					diff_eq_int("the DFE coefs were "
						    "zeroed (%ld)",
						    (long)(arena.dfecoefs[0]
							   == 0.0f
							   || mmx), 1, tag);
				}
				if (eia) {
					saw_eia = 1;
					diff_eq_int("both betas are zero "
						    "(%ld)",
						    (long)(THEIRS.linearEquBeta
							   == 0.0f
							   && THEIRS.dfeBeta
							      == 0.0f), 1,
						    tag);
				}
			}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the German-PBX arm ran", saw_pbx, 1, 0);
	diff_eq_int("the EIA-6 freeze ran", saw_eia, 1, 0);
	diff_eq_int("the fixed-point restore ran", saw_restore, 1, 0);
	diff_eq_int("the early out was taken", saw_early, 1, 0);

	return diff_end();
}

/* ============================================ calcMeanErrorStatistics */

/*
 * The 300-float buffer is a static rather than an arena member: the function
 * only READS it, so one copy shared by both sides is enough and no
 * snapshot-and-restore is needed.  What it WRITES is three floats inside the
 * object, which `diff_eq_obj` sees, and eleven lines of transcript, which is
 * most of the function -- the mean, the standard deviation and the variance
 * are each printed and only the standard deviation comes back.
 *
 * THE EARLY EXIT'S RETURN VALUE IS NOT COMPARED, and cannot be: the object
 * returns an uninitialised stack slot (D324), so the two sides read two
 * different frames.  Everything else about that path is compared, including
 * that nothing moved and nothing was printed.
 */

extern "C" {
float ref_equ_calcMeanErrorStatistics(void *self)
	asm("ref__ZN12V90Equalizer23calcMeanErrorStatisticsEv");
}

static float mee_buf[V90EQU_MEAN_ERROR_LEN];

static int
run_calcmeanerror(void)
{
	static const unsigned int cnt_v[] = { 0u, 1u, 2u, 7u, 64u, 300u };
	long tag = 990000;
	int ci, full, pat;
	int saw_early = 0, saw_full = 0, saw_count = 0;

	diff_begin("V90Equalizer::calcMeanErrorStatistics");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (ci = 0; ci < 6; ci++)
	    for (full = 0; full < 2; full++)
		for (pat = 0; pat < 3; pat++) {
			unsigned int cnt = cnt_v[ci];
			int i;
			float got, want;
			int early = (cnt == 0 && full == 0);

			tag++;
			seed(tag);
			fill_arena(tag);
			wire(&OURS);
			wire(&THEIRS);

			/*
			 * Three shapes: a ramp through zero so the sign
			 * character goes both ways and the minimum is
			 * negative; a tight cluster so the variance is tiny
			 * and the six fractional digits carry all of it; and
			 * a large one so the integer part is not zero.
			 */
			for (i = 0; i < V90EQU_MEAN_ERROR_LEN; i++) {
				if (pat == 0)
					mee_buf[i] = (float)(i - 150) * 0.125f;
				else if (pat == 1)
					mee_buf[i] = 0.0078125f
					    + (float)(i % 7) * 0.00048828125f;
				else
					mee_buf[i] = (float)((i * 37) % 211)
					    * 4.0f - 300.0f;
			}

			OURS.meanErrorEnergy = THEIRS.meanErrorEnergy =
			    mee_buf;
			OURS.meanErrorCount = THEIRS.meanErrorCount = cnt;
			OURS.meanErrorFull = THEIRS.meanErrorFull =
			    (unsigned int)full;
			OURS.meanErrorEnergyCurrent =
			    THEIRS.meanErrorEnergyCurrent =
			    pat == 0 ? -12.5f : 3.0e-5f;

			dsplib_debug_capture_reset();
			got = OURS.calcMeanErrorStatistics();
			want = ref_equ_calcMeanErrorStatistics(&THEIRS);

			diff_eq_obj("after the statistics", V90Equalizer,
				    &OURS, &THEIRS, tag);
			diff_eq_int("no store past the object (%ld)",
				    guard_equal(), 1, tag);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			if (early) {
				saw_early = 1;
				diff_eq_int("nothing was printed (%ld)",
					    (long)(dsplib_debug_capture_text(0)
						   [0] == '\0'), 1, tag);
				continue;
			}

			diff_eq_int("the returned bits (%ld)",
				    (long)(*(unsigned *)&got
					   == *(unsigned *)&want), 1, tag);

			{
				unsigned int len = full
				    ? (unsigned)V90EQU_MEAN_ERROR_LEN : cnt;
				float lo = mee_buf[0], hi = mee_buf[0];
				unsigned int k;

				for (k = 1; k < len; k++) {
					if (mee_buf[k] > hi)
						hi = mee_buf[k];
					if (mee_buf[k] < lo)
						lo = mee_buf[k];
				}
				diff_eq_float("the maximum over len",
					      THEIRS.meanErrorEnergyMax, hi,
					      tag);
				diff_eq_float("the minimum over len",
					      THEIRS.meanErrorEnergyMin, lo,
					      tag);
				/*
				 * THE TRANSCRIPT IS ENCODED, not plain text
				 * -- `edprintf` runs its format through
				 * `encode.c` -- so an anti-vacuity check
				 * cannot look for a substring.  The LINE
				 * COUNT can: thirteen `edprintf` calls when
				 * the early exit is not taken, which is what
				 * a version that dropped one of the six
				 * statistics would fail.
				 */
				diff_eq_int("thirteen lines were printed "
					    "(%ld)",
					    (long)dsplib_debug_capture_lines(1),
					    13, tag);
				if (full)
					saw_full = 1;
				else
					saw_count = 1;
			}
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the early exit was taken", saw_early, 1, 0);
	diff_eq_int("the wrapped length was used", saw_full, 1, 0);
	diff_eq_int("and the partial one", saw_count, 1, 0);

	return diff_end();
}

/* ==================================================== enterPhase4 */

/*
 * enterPhase4 needs everything the batch has needed so far at once: the arena
 * for both coefficient arrays, a real resampler for `setBllState` and
 * `getTimingOffsetPPM`, the spectral verifier for the German-PBX arm, and the
 * fixed-point arrays for the clear inside it.
 *
 * WHAT THE OBJECT COMPARISON CANNOT SEE, and what is asserted instead: the two
 * `*CoefValue` pairs are compared as object fields, but a version that summed
 * from index 0 instead of 1 would write the same maximum and minimum and only
 * differ in a printed total.  So the two sums are recomputed here -- skipping
 * the first tap, as the object does (D325) -- and checked against the
 * transcript by way of the transcript comparison, which is exact.
 */

extern "C" {
void ref_equ_enterPhase4(void *self)
	asm("ref__ZN12V90Equalizer11enterPhase4Ev");
}

static int
run_enterphase4(void)
{
	static const unsigned int len_v[] = { 0u, 1u, 3u, 16u };
	static const float ppm_v[] = { 0.0f, 12.5f, -3.75f, 1234.5f };
	long tag = 1000000;
	int li, pi, sv2, mmx, state;
	int saw_pbx = 0, saw_early = 0, saw_zero_len = 0, saw_neg = 0;

	diff_begin("V90Equalizer::enterPhase4");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (state = 1; state <= 2; state++)
	    for (li = 0; li < 4; li++)
		for (pi = 0; pi < 4; pi++)
		    for (sv2 = 0; sv2 < 2; sv2++)
			for (mmx = 0; mmx < 2; mmx++) {
				unsigned int len = len_v[li];
				unsigned int k;
				int early = (state == V90EQU_STATE_PHASE4);

				tag++;
				seed(tag);
				fill_arena(tag);
				plant_mmx_words(len, tag);
				wire(&OURS);
				wire(&THEIRS);
				wire_mmx(&OURS);
				wire_mmx(&THEIRS);

				memset(sv_block, 0, sizeof sv_block);
				ARENA_SV->word_28 = sv2 ? 2u : 1u;
				OURS.spectralVerifier =
				    THEIRS.spectralVerifier = ARENA_SV;

				for (k = 0; k < 64; k++) {
					arena.lecoefs[k] = (float)((int)k - 20)
					    * 0.0625f;
					arena.dfecoefs[k] =
					    (float)((int)((k * 13) % 41) - 25)
					    * 0.03125f;
				}

				OURS.state = THEIRS.state = state;
				OURS.stateCount = THEIRS.stateCount =
				    0x5c5c0000 + (int)len;
				OURS.linearEquLength =
				    THEIRS.linearEquLength = len;
				OURS.dfeLength = THEIRS.dfeLength = len;
				OURS.word_1c = THEIRS.word_1c = len;
				OURS.mmxMode = THEIRS.mmxMode = mmx;
				OURS.meanErrorCount = THEIRS.meanErrorCount =
				    17u;
				OURS.meanErrorFull = THEIRS.meanErrorFull = 1u;
				OURS.linearEquMmxConversionFactor =
				    THEIRS.linearEquMmxConversionFactor = 1024.0f;
				OURS.dfeMmxConversionFactor =
				    THEIRS.dfeMmxConversionFactor = 256.0f;

				ARENA_RSAMP->params = ARENA_PARAMS;
				ARENA_RSAMP->bllState = (V90BllState)
				    (pi == 0 ? V90_BLL_FROZEN
					     : V90_BLL_STEADY_STATE);
				ARENA_RSAMP->stateSamples = 0x11223344u;
				ARENA_RSAMP->countStateSamples = 0x55667788u;
				ARENA_RSAMP->ppmScale = 4.0f;
				ARENA_RSAMP->timingOffset =
				    ppm_v[pi] * 4.0f * 1.0e-6f;

				arena_snapshot();
				dsplib_debug_capture_reset();
				OURS.enterPhase4();
				arena_switch();
				ref_equ_enterPhase4(&THEIRS);

				diff_eq_obj("after enterPhase4", V90Equalizer,
					    &OURS, &THEIRS, tag);
				arena_compare("the arena after enterPhase4",
					      tag);
				diff_eq_int("no store past the object (%ld)",
					    guard_equal(), 1, tag);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);

				if (early) {
					saw_early = 1;
					diff_eq_int("the early out printed "
						    "nothing (%ld)",
						    (long)(dsplib_debug_capture_lines(1)
							   == 0), 1, tag);
					diff_eq_int("and left the diagnostics "
						    "alone (%ld)",
						    (long)THEIRS.meanErrorCount,
						    17, tag);
					continue;
				}

				diff_eq_int("the state was entered (%ld)",
					    (long)THEIRS.state,
					    V90EQU_STATE_PHASE4, tag);
				diff_eq_int("stateCount is NOT reset (%ld)",
					    (long)THEIRS.stateCount,
					    (long)(0x5c5c0000 + (int)len), tag);
				diff_eq_int("the BLL state was saved (%ld)",
					    (long)THEIRS.savedBllState,
					    (long)(pi == 0 ? V90_BLL_FROZEN
						   : V90_BLL_STEADY_STATE),
					    tag);
				diff_eq_int("and the loop frozen (%ld)",
					    (long)ARENA_RSAMP->bllState,
					    (long)V90_BLL_FROZEN, tag);
				diff_eq_int("the mean-error diagnostics were "
					    "reset (%ld)",
					    (long)(THEIRS.meanErrorCount == 0
						   && THEIRS.meanErrorFull
						      == 0), 1, tag);

				if (ppm_v[pi] < 0.0f)
					saw_neg = 1;
				if (len == 0)
					saw_zero_len = 1;
				if (sv2)
					saw_pbx = 1;

				/*
				 * The maximum and minimum are over the
				 * MAGNITUDE and include the first tap; the
				 * two sums start at index 1.  The sums are
				 * not in the object, so they are checked
				 * through the transcript -- what is asserted
				 * here is the pair that IS.
				 */
				{
					float mx = arena_save.lecoefs[0] < 0.0f
					    ? -arena_save.lecoefs[0]
					    : arena_save.lecoefs[0];
					float mn = mx;

					for (k = 1; k < len; k++) {
						float a =
						    arena_save.lecoefs[k];

						if (a < 0.0f)
							a = -a;
						if (a > mx)
							mx = a;
						if (a < mn)
							mn = a;
					}
					diff_eq_float("maxLeCoefValue",
						      THEIRS.maxLeCoefValue,
						      mx, tag);
					diff_eq_float("minLeCoefValue",
						      THEIRS.minLeCoefValue,
						      mn, tag);
				}
			}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the German-PBX arm ran", saw_pbx, 1, 0);
	diff_eq_int("the early out was taken", saw_early, 1, 0);
	diff_eq_int("a zero-length filter was summarised", saw_zero_len, 1, 0);
	diff_eq_int("a negative timing offset was printed", saw_neg, 1, 0);

	return diff_end();
}

/* =========================================== convertEqualizerToMmx */

/*
 * convertEqualizerToMmx -- twenty-seven printed lines, six arrays written and
 * twelve fields set, so it needs everything the batch has built at once: the
 * arena for both coefficient pairs and both histories, the skewed aligned
 * views, the planted fixed-point words and the transcript.
 *
 * WHAT ONLY THE TRANSCRIPT CAN SEE.  Six of the printed numbers are locals the
 * object never stores -- the signed and absolute sums over the high halves,
 * their two extremes, and the two history extremes -- along with every
 * fractional digit of the four float sums.  A body that accumulated any of
 * them differently leaves the object and the arena identical, so the
 * transcript comparison is the test and the object comparison is the backstop.
 *
 * WHAT THE TRANSCRIPT CANNOT SAY OUT LOUD.  The capture is ciphertext (finding
 * F2136), so `strstr` finds nothing in it.  The empty-filter case -- where the
 * minimum is printed as the 0x10000 it was initialised to -- is checked in one
 * extra trial with `dsplib_encode_plain` set, which turns OUR side's output
 * readable and leaves the reference's encoded.  That trial compares everything
 * except the transcript; what carries its claim back to the object is that
 * every other trial compared the two and they agreed.
 *
 * THE THREE LENGTHS ARE SET APART FROM ONE ANOTHER IN EVERY TRIAL.
 * `linearEquLength` bounds the linear coefficients, `dfeLength` the DFE's and
 * its history, and `word_1c` the linear history -- three different fields, and
 * `fill_arena` leaves garbage in all three, so a trial that left one alone
 * would be a wild write and not a wrong answer.
 */

#include "dsplib/encode.h"

extern "C" {
void ref_equ_convertEqualizerToMmx(void *self)
	asm("ref__ZN12V90Equalizer21convertEqualizerToMmxEv");
}

/*
 * The reference levels.  1024 and 0.5 make the reciprocal exact; 3.0 and 1e-6
 * do not, and those are the ones that tell `(1.0/x) * m` -- which is what the
 * object computes -- from `m / x`, which is what `setLinearEquBeta` computes.
 * 0.0f divides by zero on purpose: the factor comes out infinite and the
 * conversion of it is the x87's integer indefinite on both sides.
 */
static const float mmxref_v[] = {
	1.0f, 2.0f, 3.0f, 1024.0f, 0.5f, -1.0f, 1.0e-6f, 1048576.0f, 0.0f
};
#define NMMXREF ((int)(sizeof(mmxref_v) / sizeof(mmxref_v[0])))

/* Floats for the two history arrays: both ends of a short, two values outside
 * it, and -32768 itself, whose magnitude comes back negative. */
static const float mmxhist_v[] = {
	-32768.0f, 32767.0f, 40000.0f, -40000.0f, 0.0f,
	-0.5f, 100.75f, -100.75f, 1.0f
};

/* Exactly float-representable 32-bit words, planted through a factor of 2**30:
 * the high half goes negative and the low half above 0x7fff. */
static const int mmxwit_v[] = {
	32768, -32768, 2147450880, -2147450880, 98304, -98304
};

static void
mmx_setup(long tag, unsigned int le, unsigned int dfe, unsigned int w1c,
	  int pat, float ml, float md, float beta, float dbeta)
{
	unsigned int k;

	seed(tag);
	fill_arena(tag);
	plant_mmx_words(le > dfe ? le : dfe, tag);
	wire(&OURS);
	wire(&THEIRS);
	wire_mmx(&OURS);
	wire_mmx(&THEIRS);

	for (k = 0; k < 64; k++) {
		switch (pat) {
		case 0:
			arena.lecoefs[k] = (float)((int)k - 20) * 0.0625f;
			arena.dfecoefs[k] = (float)((int)k - 9) * 0.125f;
			break;
		case 1:
			arena.lecoefs[k] = (float)mmxwit_v[k % 6]
			    * (1.0f / 1073741824.0f);
			arena.dfecoefs[k] = (float)mmxwit_v[(k + 3) % 6]
			    * (1.0f / 1073741824.0f);
			break;
		default:
			/*
			 * One large tap and a tail of small ones, so a sum
			 * accumulated in extended precision differs from one
			 * accumulated in `float` in the digits that are
			 * printed.
			 */
			arena.lecoefs[k] = (k == 0) ? 1024.0f
			    : 0.0001f * (float)(k % 7 + 1);
			arena.dfecoefs[k] = (k == 0) ? 512.0f
			    : 0.00013f * (float)(k % 5 + 1);
			break;
		}
		arena.a18[k] = mmxhist_v[(k + (unsigned)tag) % 9];
		arena.a44[k] = mmxhist_v[(k * 3u + (unsigned)tag) % 9];
	}

	OURS.linearEquLength = THEIRS.linearEquLength = le;
	OURS.dfeLength = THEIRS.dfeLength = dfe;
	OURS.word_1c = THEIRS.word_1c = w1c;
	OURS.word_20 = THEIRS.word_20 = 0x5a5a0000u + (unsigned)pat;
	OURS.word_20Saved = THEIRS.word_20Saved = 0xdeadbeefu;
	OURS.maxLeCoefValue = THEIRS.maxLeCoefValue = ml;
	OURS.maxDfeCoefValue = THEIRS.maxDfeCoefValue = md;
	OURS.linearEquBeta = THEIRS.linearEquBeta = beta;
	OURS.dfeBeta = THEIRS.dfeBeta = dbeta;

	OURS.mmxMode = THEIRS.mmxMode = 0;
	OURS.mmxArraysPresent = THEIRS.mmxArraysPresent = 1;
	ARENA_PARAMS->ENABLE_EQUALIZER_MMX = 1;
}

/*
 * The conversion run on both sides against the same starting arena, and
 * everything that is asserted for every trial.  The line count is the
 * call-site check the ciphertext comparison cannot name: one line on the bail
 * and thirteen for each half plus the closing line on the success path.
 */
static void
mmx_run(long tag, int bail)
{
	arena_snapshot();
	dsplib_debug_capture_reset();
	OURS.convertEqualizerToMmx();
	arena_switch();
	ref_equ_convertEqualizerToMmx(&THEIRS);

	diff_eq_obj("after the conversion", V90Equalizer, &OURS, &THEIRS, tag);
	arena_compare("the arena after the conversion", tag);
	diff_eq_int("no store past the object (%ld)", guard_equal(), 1, tag);
	diff_eq_int("transcript (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("lines printed (%ld)", (long)dsplib_debug_capture_lines(1),
		    bail ? 1 : 27, tag);
	diff_eq_int("the mode (%ld)", (long)THEIRS.mmxMode, bail ? 0 : 1, tag);
}

/*
 * The two factors, restated here from the disassembly instead of taken from
 * the code under test: the RECIPROCAL first and the scale after it, which is
 * the order 3738f and 37395 have, and then that times 2**-16 for the output
 * factor.  A body that computed `2**30 / m` in one step would agree with this
 * for every power of two and differ in the last bit for 3.0 and 1e-6.
 */
static void
mmx_check_factors(const char *what, float m, float gotConv, int gotOut,
		  long tag)
{
	long double c = (1.0L / (long double)m) * 1073741824.0L;
	long double o = c * 0.0000152587890625L;

	diff_eq_float(what, gotConv, (float)c, tag);
	if (o > -2.0e9L && o < 2.0e9L)
		diff_eq_int("the output conversion factor (%ld)", (long)gotOut,
			    (long)(int)o, tag);
}

static int
run_converttommx(void)
{
	static const unsigned int len_v[] = { 0u, 1u, 5u, 20u };
	long tag = 1010000;
	int bi, ri, li, pat, gate;
	int saw_mode = 0, saw_noarrays = 0, saw_noparam = 0, saw_ok = 0;
	int saw_beta = 0, saw_zerobeta = 0;

	diff_begin("V90Equalizer::convertEqualizerToMmx");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	/*
	 * The three ways out.  Only the first arm can make the `mmxMode = 0`
	 * store observable -- the other two enter with the field already
	 * clear -- so it is set to something else entirely there.
	 */
	for (gate = 0; gate < 3; gate++)
		for (li = 0; li < 4; li++) {
			tag++;
			mmx_setup(tag, len_v[li], len_v[(li + 1) % 4],
				  len_v[(li + 2) % 4], 0, 1.0f, 2.0f, 0.0f,
				  0.0f);

			switch (gate) {
			case 0:
				OURS.mmxMode = THEIRS.mmxMode = 0x1234u;
				saw_mode = 1;
				break;
			case 1:
				OURS.mmxArraysPresent =
				    THEIRS.mmxArraysPresent = 0;
				saw_noarrays = 1;
				break;
			default:
				ARENA_PARAMS->ENABLE_EQUALIZER_MMX = 0;
				saw_noparam = 1;
				break;
			}

			mmx_run(tag, 1);
			diff_eq_int("nothing was converted (%ld)",
				    (long)(THEIRS.word_20Saved == 0xdeadbeefu),
				    1, tag);
		}

	/*
	 * The arithmetic: every step size the setters are swept over, against
	 * every reference level, with the lengths and the coefficient pattern
	 * rotating underneath so no combination of the three is the only one
	 * seen.
	 */
	for (bi = 0; bi < nbeta; bi++)
		for (ri = 0; ri < NMMXREF; ri++) {
			float ml = mmxref_v[ri];
			float md = mmxref_v[(ri + 3) % NMMXREF];

			li = (bi + ri) % 4;
			pat = (bi + 2 * ri) % 3;

			tag++;
			mmx_setup(tag, len_v[li], len_v[(li + 1) % 4],
				  len_v[(li + 2) % 4], pat, ml, md,
				  beta_v[bi], beta_v[(bi + 7) % nbeta]);
			mmx_run(tag, 0);

			saw_ok = 1;
			if (beta_v[bi] != 0.0f)
				saw_beta = 1;
			else
				saw_zerobeta = 1;

			mmx_check_factors("linearEquMmxConversionFactor", ml,
					  THEIRS.linearEquMmxConversionFactor,
					  THEIRS.linearEquMmxOutputConversionFactor,
					  tag);
			mmx_check_factors("dfeMmxConversionFactor", md,
					  THEIRS.dfeMmxConversionFactor,
					  THEIRS.dfeMmxOutputConversionFactor,
					  tag);
			diff_eq_int("word_20 was parked in +0xf8 (%ld)",
				    (long)THEIRS.word_20Saved,
				    (long)THEIRS.word_20, tag);
			if (beta_v[bi] == 0.0f) {
				diff_eq_int("a zero step size is zero (%ld)",
					    (long)THEIRS.linearEquMmxBeta, 0,
					    tag);
				diff_eq_int("and its exponent (%ld)",
					    (long)THEIRS.linearEquMmxShift, 0,
					    tag);
			}
		}

	/*
	 * Every length against every coefficient pattern, at a reference level
	 * of 1.0 so the factor is exactly 2**30 and the planted witnesses land
	 * on the 32-bit words they were chosen for.
	 */
	for (li = 0; li < 4; li++)
		for (pat = 0; pat < 3; pat++) {
			unsigned int le = len_v[li];

			tag++;
			mmx_setup(tag, le, len_v[(li + 2) % 4],
				  len_v[(li + 3) % 4], pat, 1.0f, 1.0f,
				  1.0e-8f, 3.0e-7f);
			mmx_run(tag, 0);

			if (pat == 1 && le > 0) {
				/* The witness at index 0 is 32768: high half
				 * 0x0000, low half 0x8000 -- the pairing the
				 * two extensions exist for. */
				diff_eq_int("the low half was written (%ld)",
					    (long)(unsigned short)
					    arena.ad8[1], 0x8000, tag);
				diff_eq_int("and the high half (%ld)",
					    (long)arena.lemmx[1], 0, tag);
			}
		}

	/*
	 * THE RECIPROCAL IS TAKEN BEFORE THE MULTIPLY, and the sweep above
	 * cannot see it.  `1/(beta*2**24)` then times the reference level,
	 * not the level divided by the product -- the two differ by at most
	 * one bit, and the only inputs where one bit matters are those where
	 * the exact quotient is a power of two and the divisor is not,
	 * because the truncated logarithm downstream then lands on either
	 * side of an integer.  Searched for rather than guessed: beta =
	 * 41 * 2**-24 makes the divisor exactly 41, and a reference level of
	 * 82 makes the exact quotient exactly 2, so the reciprocal route
	 * comes out just under and truncates to 0 where a division truncates
	 * to 1.  D327's difference, made visible; the mutation that survived
	 * until this trial existed is in test/mutations/v90equ.json.
	 */
	tag++;
	mmx_setup(tag, 4u, 4u, 4u, 0, 82.0f, 82.0f, 41.0f / 16777216.0f,
		  41.0f / 1048576.0f);
	mmx_run(tag, 0);
	diff_eq_int("the reciprocal route truncates the exponent down (%ld)",
		    (long)THEIRS.linearEquMmxShift, 0, tag);

	/*
	 * The empty filter, in plain text.  Both extremes come out as what
	 * they were initialised to and neither is a field, so this is the one
	 * claim the ciphertext cannot make; see the note above.
	 */
	tag++;
	mmx_setup(tag, 0u, 0u, 0u, 0, 1.0f, 1.0f, 0.0f, 0.0f);
	arena_snapshot();
	dsplib_debug_capture_reset();
	dsplib_encode_plain = 1;
	OURS.convertEqualizerToMmx();
	dsplib_encode_plain = 0;
	arena_switch();
	ref_equ_convertEqualizerToMmx(&THEIRS);

	diff_eq_obj("after the empty conversion", V90Equalizer, &OURS, &THEIRS,
		    tag);
	arena_compare("the arena after the empty conversion", tag);
	diff_eq_int("the empty filter's minimum is 0x10000 (%ld)",
		    (long)(strstr(dsplib_debug_capture_text(0),
				  "short high LE coeffs min value = 65536")
			   != 0), 1, tag);
	diff_eq_int("and its maximum is zero (%ld)",
		    (long)(strstr(dsplib_debug_capture_text(0),
				  "short high LE coeffs max value = 0\r")
			   != 0), 1, tag);
	diff_eq_int("the DFE's minimum too (%ld)",
		    (long)(strstr(dsplib_debug_capture_text(0),
				  "short high dfe coeffs min value = 65536")
			   != 0), 1, tag);
	diff_eq_int("the empty history's minimum is 0x8000 (%ld)",
		    (long)(strstr(dsplib_debug_capture_text(0),
				  "Min LE History = 32768") != 0), 1, tag);
	diff_eq_int("and its maximum is zero (%ld)",
		    (long)(strstr(dsplib_debug_capture_text(0),
				  "Max LE History = 0\r") != 0), 1, tag);

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the mode-already-set arm was taken", saw_mode, 1, 0);
	diff_eq_int("the no-arrays arm was taken", saw_noarrays, 1, 0);
	diff_eq_int("the parameter arm was taken", saw_noparam, 1, 0);
	diff_eq_int("the conversion ran", saw_ok, 1, 0);
	diff_eq_int("a non-zero step size was renormalised", saw_beta, 1, 0);
	diff_eq_int("and a zero one was not", saw_zerobeta, 1, 0);

	return diff_end();
}

/*
 * THE FOUR COEFFICIENT SUMS ARE `float`, AND ONLY THE PERIOD COMPILER AGREES.
 *
 * The object stores each accumulator back to a four-byte slot every iteration
 * (`fstps 0x4c(%esp)` and `fstps 0x48(%esp)`, 0x28 and 0x24 in the DFE half),
 * so a tap smaller than half the accumulator's ulp is lost entirely.  A large
 * first tap and a tail below that ulp is what separates a `float` accumulator
 * from an extended-precision one: 65536.0f has an ulp of 0.0078125, so
 * nineteen taps of 0.0005f vanish one at a time in `float` and add up to
 * 0.0095 in `long double` -- four printed digits apart.  The pattern in
 * `mmx_setup` has a tail that is merely SMALL, which agrees to the fourth
 * digit either way, and that is why the mutation survived it.
 *
 * IT RUNS ONLY UNDER THE PERIOD COMPILER, and that is not a convenience.
 * GCC 13 holds the accumulator in an x87 register across the whole loop and
 * rounds once at the end, so under the modern build BOTH a `float` and a
 * `long double` accumulator produce the extended-precision answer and the
 * check fails for our source and passes for a wrong one -- it would be a
 * check that lies.  GCC 3.4.2 spills exactly as the object does.  Two
 * spellings that force the narrowing elsewhere were tried and neither moved
 * GCC 13; `tools/gccdiverge.json` was the other candidate and is worse here,
 * because `tools/mutate.py` does not consult it and a binary with a failing
 * check takes the whole mutation suite down with it.  Finding F2150.
 */
#if defined(__GNUC__) && __GNUC__ < 4
#define V90EQU_SUM_PRECISION_TESTABLE 1
#endif

#ifdef V90EQU_SUM_PRECISION_TESTABLE
static int
run_mmx_sum_precision(void)
{
	long tag = 1011000;
	unsigned int k;

	diff_begin("V90Equalizer: the coefficient sums are float");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	mmx_setup(tag, 20u, 20u, 8u, 0, 65536.0f, 32768.0f, 0.0f, 0.0f);
	arena.lecoefs[0] = 65536.0f;
	arena.dfecoefs[0] = 32768.0f;
	for (k = 1; k < 64; k++) {
		arena.lecoefs[k] = 0.0005f;
		arena.dfecoefs[k] = 0.0007f;
	}
	mmx_run(tag, 0);

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}
#endif /* V90EQU_SUM_PRECISION_TESTABLE */

int
main(void)
{
	int rc = 0;

	build_betas();

	rc |= run_ctor();
	rc |= run_dtor();
	rc |= run_setters();
	rc |= run_exact_powers();
	rc |= run_enterphase3();
	rc |= run_reset();
	rc |= run_enterchannelverification();

	rc |= run_getdfebeta();
	rc |= run_smallmembers();
	rc |= run_setcoeff();
	rc |= run_zerocoefs();
	rc |= run_fadingparams();
	rc |= run_freeze();
	rc |= run_restoretofloat();
	rc |= run_fadeedges();
	rc |= run_enterrrnfpe();
	rc |= run_calcmeanerror();
	rc |= run_enterphase4();
	rc |= run_converttommx();
#ifdef V90EQU_SUM_PRECISION_TESTABLE
	rc |= run_mmx_sum_precision();
#endif

	return rc;
}
