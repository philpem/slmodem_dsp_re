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
 * rather than silence (findings 223, 224, 230).  The lifecycle pair needs
 * more than that and finding 1234 is what it needs; see the comment above
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
				OURS.linearEquMmxRefLevel =
				    THEIRS.linearEquMmxRefLevel = ref_v[ri];
				OURS.dfeMmxRefLevel =
				    THEIRS.dfeMmxRefLevel = ref_v[ri];
				OURS.linearEquMmxBetaScale =
				    THEIRS.linearEquMmxBetaScale =
					scale_v[si];
				OURS.dfeMmxBetaScale =
				    THEIRS.dfeMmxBetaScale = scale_v[si];

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
		OURS.linearEquMmxRefLevel =
		    THEIRS.linearEquMmxRefLevel = 1.0f;
		OURS.dfeMmxRefLevel = THEIRS.dfeMmxRefLevel = 1.0f;
		OURS.linearEquMmxBetaScale =
		    THEIRS.linearEquMmxBetaScale = 1.0f;
		OURS.dfeMmxBetaScale = THEIRS.dfeMmxBetaScale = 1.0f;
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
				OURS.linearEquMmxRefLevel =
				    THEIRS.linearEquMmxRefLevel = ref_v[ri];
				OURS.dfeMmxRefLevel =
				    THEIRS.dfeMmxRefLevel = ref_v[ri];
				OURS.linearEquMmxBetaScale =
				    THEIRS.linearEquMmxBetaScale = 1.0f;
				OURS.dfeMmxBetaScale =
				    THEIRS.dfeMmxBetaScale = 1.0f;

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
				 * compare equal (finding 1105), so the values
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
 * fixture rests on (findings 223, 224).  The blob holds C1 and C2 as two
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
	{ 0x098, "block_98",			0, 0 },
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
						diff_eq_int("block size (%ld)",
							    (long)
							    malloc_usable_size(
								pa),
							    (long)
							    malloc_usable_size(
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

	return rc;
}
