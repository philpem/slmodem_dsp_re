/*
 * t_v90equproc.cpp -- differential test of V90Equalizer::process.
 *
 * IT IS ITS OWN BINARY FOR `t_v90eqdata.cpp`'S REASON.  `t_v90equ` is in
 * `tools/gccdiverge.json` because six of its checks are the object's
 * one-ordered-compare equality, which GCC 13 cannot emit; `tools/mutate.py`
 * refuses to score a mutant set against an already-red binary, because caught
 * and already-red are indistinguishable.  This binary keeps NaN out of the
 * grid deliberately -- the only divergence class in `process` is the ordered
 * compare at the `long double` high-error test and the `== 0.0f` guard on the
 * before/after ratio, and both are ordinary on finite inputs -- so it stays
 * green on the modern tier and stays mutation-scorable.
 *
 * THE FIXTURE IS `t_v90eqdata.cpp`'S, extended: the object in a byte array
 * carried by a union for its alignment, both sides seeded with the SAME
 * varied pseudorandom bytes and never with zeros (finding 230), the whole
 * object compared with `diff_eq_obj`, and the bytes from `sizeof` to the end
 * of an over-large slot compared separately so a store past the object's end
 * is a failure rather than silence (findings 223, 224).
 *
 * WHAT ONLY THIS TEST CAN SEE, and every one of them has a grid axis and an
 * anti-vacuity counter of its own:
 *
 *   - THE TWO ERRORS.  The DFE adapts on `soft - decision` and the linear
 *     half on `y - decision`, and the two differ by exactly the DFE output.
 *     A grid with a zero-length DFE, or with zero DFE coefficients, cannot
 *     tell them apart -- so `dfeLength >= 2` and the coefficients are planted
 *     non-zero on every trial that drives the update.
 *   - THE SQUARED ERROR'S 64-BIT CONVERT.  `word_78 += (unsigned)(long
 *     long)(err * err)` and `word_78 += (unsigned)(err * err)` agree for
 *     every |err| below 65536.  RESET's slicer is `(short)soft`, so a `soft`
 *     outside a short's range wraps the decision and makes |err| enormous;
 *     that is what the large-amplitude rows are for.
 *   - THE SECOND `word_20` DECREMENT, which is unconditional.  Trials that
 *     skip the coefficient update still have to retreat the cursor twice,
 *     and the wrap has to fire from both paths.
 *   - THE CLEAN-SYMBOL COUNTER'S ASYMMETRY.  A burst that ends with
 *     `word_94 <= 2` leaves `updateCoefs` at zero for the rest of the call.
 *
 * WHAT THIS BINARY DELIBERATELY DOES NOT DRIVE, and why it is a property of
 * the object rather than a gap in the grid:
 *
 *   - `state` OUTSIDE 0..6.  The dispatch's `default` writes no decision at
 *     all, so `outSym[j]` is whatever the caller left in a register; the two
 *     sides are different code and cannot agree.  The tail's tests against
 *     10..16 are reachable only from there, so their FALSE side is not
 *     differentially drivable.  Finding 6001.
 *   - `mmxMode` WITH `state` 0, 1, 2 OR 6.  Those four arms consume `soft`,
 *     which the fixed-point arm never writes -- the fixed-point
 *     representation is only entered from the data phase, and states 3, 4
 *     and 5 are the ones that read `softInt`.  Same argument, same finding.
 *   - `dfeLength` 0 OR 1.  The float history shift is an unguarded
 *     `do`/`while` over `dfeLength - 1`, so the object walks off the array
 *     for four billion iterations at either value.  D850.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90Equalizer.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_equ_process(void *self, float *in, unsigned int n, short *outSym,
		     float *outFloat, unsigned int *nOut)
	asm("ref__ZN12V90Equalizer7processEPfjPsS0_Rj");
}

#include "dsplib/V90Resampler.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90ConnectionEvaluator.h"

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

/*
 * ARR_F has to hold `word_1c`, which is `linearEquLength + 8` at its widest
 * here, and the sample buffers have to hold `n` plus the two floats
 * RECONVERT-D steps past the end of what the loop consumed.
 */
#define ARR_F	80
#define ARR_S	80
#define NIN	64
#define RSLOT	(0xb4 + 32)

struct equ_arena {
	float		lecoefs[ARR_F];
	float		a18[ARR_F];
	float		lewin[ARR_F];
	float		dfewin[ARR_F];
	float		dfecoefs[ARR_F];
	float		a44[ARR_F];
	float		meanerr[V90EQU_MEAN_ERROR_LEN];
	short		lemmx[ARR_S];
	short		ad8[ARR_S];
	short		aec[ARR_S];
	short		dfemmx[ARR_S];
	short		a118[ARR_S];
	short		a12c[ARR_S];
	short		b4[512];
	short		b8[256];
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

static void
wire(V90Equalizer *o)
{
	o->linearEquCoefs = arena.lecoefs;
	o->array_18 = arena.a18;
	o->linearEquWindow = arena.lewin;
	o->dfeWindow = arena.dfewin;
	o->dfeCoefs = arena.dfecoefs;
	o->array_44 = arena.a44;
	o->meanErrorEnergy = arena.meanerr;
	o->linearEquMmxCoefs = arena.lemmx;
	o->array_d8 = arena.ad8;
	o->array_ec = arena.aec;
	o->dfeMmxCoefs = arena.dfemmx;
	o->array_118 = arena.a118;
	o->array_12c = arena.a12c;
	o->block_b4 = arena.b4;
	o->block_b8 = arena.b8;
	o->params = ARENA_PARAMS;
	o->resampler = ARENA_RSAMP;
	o->linearEquMmxCoefsAligned = arena.lemmx + 1;
	o->array_d8Aligned = arena.ad8 + 1;
	o->array_ecAligned = arena.aec + 1;
	o->dfeMmxCoefsAligned = arena.dfemmx + 1;
	o->array_118Aligned = arena.a118 + 1;
	o->array_12cAligned = arena.a12c + 1;
	o->linearEquMmxCoefsSkew = o->array_d8Skew = o->array_ecSkew = 1;
	o->dfeMmxCoefsSkew = o->array_118Skew = o->array_12cSkew = 1;
}

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

/* ---------------------------------------------------- the peers we compare */

/*
 * The connection evaluator is WRITTEN -- `updateAvePdsnr` runs every time a
 * block closes -- so each side gets its own copy of the same bytes, exactly
 * as `t_v90eqdata` does for the phase 4 demodulator.  `params` is
 * re-installed on top of the fill so the field itself compares equal.
 */
#define CE_SLOT ((unsigned)sizeof(V90ConnectionEvaluator) + 64u)

static unsigned char ce_[2][CE_SLOT] __attribute__((aligned(8)));
static unsigned char ce_seed[CE_SLOT];

static void
seed_ce_pair(long trial)
{
	unsigned lfsr = 0x1a7fu + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < CE_SLOT; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)((lfsr >> 3) | 1u);
		ce_[0][i] = v;
		ce_[1][i] = v;
	}
	((V90ConnectionEvaluator *)ce_[0])->params =
	    ((V90ConnectionEvaluator *)ce_[1])->params = ARENA_PARAMS;
	memcpy(ce_seed, ce_[0], CE_SLOT);
}

/* ------------------------------------------------------- the I/O buffers */

static float in_[2][NIN];
static short outsym_[2][NIN];
static float outflt_[2][NIN];

/*
 * Amplitudes, and each row is here for something.  RESET's slicer is
 * `(short)soft`, so anything inside a short's range gives an error in
 * (-1, 1); the two large rows wrap the decision and make |err| big enough to
 * reach the high-error arm's threshold and, on the largest, to overflow a
 * 32-bit product.
 */
static const float amp_v[] = {
	0.75f, 12.5f, 1024.0f, -1024.0f, 45000.0f, -45000.0f, 99000.0f
};
#define NAMP ((int)(sizeof(amp_v) / sizeof(amp_v[0])))

static void
plant_floats(long tag, unsigned int le, unsigned int dfe, int amp, int zero_dfe)
{
	unsigned int k;
	float a = amp_v[amp];

	for (k = 0; k < ARR_F; k++) {
		/*
		 * The linear equaliser sees one dominant tap and a tail, so
		 * `y` tracks the amplitude; the history alternates sign so
		 * that consecutive symbols do not repeat.
		 */
		arena.lecoefs[k] = (k == 0) ? 1.0f
		    : 0.03125f * (float)((int)(k % 5) - 2);
		arena.a18[k] = a * (float)(((int)(k + (unsigned)tag) % 7) - 3)
		    * 0.25f;
		/*
		 * The DFE has to be non-zero for the two errors to differ:
		 * `err` and `y - decision` are exactly `d` apart.  One row of
		 * the grid zeroes it, and that row is the negative control --
		 * it must still pass, and it is the one a swapped error would
		 * survive.
		 */
		arena.dfecoefs[k] = zero_dfe ? 0.0f
		    : 0.0625f * (float)((int)(k % 3) - 1) + 0.015625f;
		arena.a44[k] = zero_dfe ? 0.0f
		    : a * 0.03125f * (float)(((int)(k * 3u + (unsigned)tag)
					      % 5) - 2);
		arena.lewin[k] = 0.5f + 0.015625f * (float)(k % 9);
		arena.dfewin[k] = 0.25f + 0.03125f * (float)(k % 7);
	}
	(void)le;
	(void)dfe;
	for (k = 0; k < V90EQU_MEAN_ERROR_LEN; k++)
		arena.meanerr[k] = 0.125f * (float)((int)(k % 11) - 5);
}

/* ================================================================= spine */

/*
 * Stage one: the float arm with the RESET slicer, which calls no peer at all
 * inside the loop.  Everything the function does OUTSIDE the state arms is
 * here -- the prologue's held sample, both dot products, the soft output, the
 * clean-symbol counter, both LMS loops, the history shifts, the cursor wrap,
 * the squared-error accumulation, the block close and its call out to the
 * connection evaluator, the odd-sample carry and the fade-edges cycle.
 */
static int
run_reset_arm(void)
{
	static const unsigned int le_v[] = { 1u, 3u, 8u, 16u };
	static const unsigned int dfe_v[] = { 2u, 5u, 12u };
	static const unsigned int n_v[] = { 0u, 1u, 2u, 5u, 8u, 17u };
	long tag = 5710000;
	int li, di, ni, ai, held, w94i, zd;
	int saw_wrap = 0, saw_close = 0, saw_fade = 0, saw_held = 0;
	int saw_odd = 0, saw_bigerr = 0, saw_overflow = 0, saw_burst = 0;
	int saw_clean_close = 0, saw_zero_dfe = 0, saw_dfe = 0, saw_zero_n = 0;
	int sep_sym = 0, sep_nout = 0;
	long prev_sym = -1;
	long prev_nout = -1;
	static const unsigned int w94_v[] = { 0u, 1u, 2u, 3u, 7u };

	diff_begin("V90Equalizer::process, the RESET arm");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (li = 0; li < 4; li++)
	    for (di = 0; di < 3; di++)
		for (ni = 0; ni < 6; ni++)
		    for (ai = 0; ai < NAMP; ai++)
			for (held = 0; held < 2; held++)
			    for (w94i = 0; w94i < 5; w94i++)
				for (zd = 0; zd < 2; zd++) {
					unsigned int le = le_v[li];
					unsigned int dfe = dfe_v[di];
					unsigned int n = n_v[ni];
					unsigned int w1c = le + 8u;
					unsigned int no_a = 0, no_b = 0;
					unsigned int k;

					tag++;
					seed(tag);
					fill_arena(tag);
					plant_floats(tag, le, dfe, ai, zd);
					wire(&OURS);
					wire(&THEIRS);
					seed_ce_pair(tag);

					OURS.linearEquLength =
					    THEIRS.linearEquLength = le;
					OURS.dfeLength = THEIRS.dfeLength = dfe;
					OURS.word_1c = THEIRS.word_1c = w1c;
					/*
					 * Start one symbol short of the wrap on
					 * every fourth trial, so the wrap fires
					 * from both the update and the
					 * update-skipped path.
					 */
					OURS.word_20 = THEIRS.word_20 =
					    (int)(w1c - le - 1u)
					    - (int)((unsigned)tag % 3u);
					OURS.word_20Saved =
					    THEIRS.word_20Saved = 0x5a5a;
					OURS.state = THEIRS.state =
					    V90EQU_STATE_RESET;
					OURS.stateCount = THEIRS.stateCount =
					    0x3c3c;
					OURS.mmxMode = THEIRS.mmxMode = 0;
					OURS.mmxArraysPresent =
					    THEIRS.mmxArraysPresent = 1;
					OURS.quickConnect =
					    THEIRS.quickConnect = 0;

					/*
					 * THE LARGE-AMPLITUDE ROWS ADAPT WITH
					 * A ZERO STEP SIZE, and that is not a
					 * weakened axis.  They exist to wrap
					 * the RESET slicer -- `(short)soft`
					 * outside a short's range is the only
					 * way this arm can produce an error
					 * bigger than one -- and with a live
					 * step size the LMS then feeds an
					 * error of 10**5 back through a
					 * history of 45,000 and the
					 * coefficients reach infinity within
					 * nine symbols.  Past that point the
					 * two sides are comparing an
					 * overflowed filter, not this
					 * function, and the object's own
					 * `fistpll` is outside `long long`
					 * as well.  A frozen step size is
					 * also a real state -- five of the
					 * arms call `setLinearEquBeta(0.0f)`
					 * -- so the rows still drive the
					 * update loops, just with nothing to
					 * add.  The adapting rows are the
					 * four below 1,025.
					 */
					OURS.linearEquBeta =
					    THEIRS.linearEquBeta =
					    (ai >= 4) ? 0.0f : 0.0009765625f;
					OURS.dfeBeta = THEIRS.dfeBeta =
					    (ai >= 4) ? 0.0f : 0.00048828125f;
					OURS.word_94 = THEIRS.word_94 =
					    w94_v[w94i];
					OURS.word_68 = THEIRS.word_68 =
					    (unsigned)held;
					OURS.word_6c = THEIRS.word_6c =
					    amp_v[ai] * 0.5f;
					OURS.word_70 = THEIRS.word_70 =
					    (unsigned)(tag % 3);
					OURS.word_78 = THEIRS.word_78 =
					    0xfffff000u;
					OURS.word_7c = THEIRS.word_7c = 3.5f;
					OURS.errorEnergyMeanBlockLen =
					    THEIRS.errorEnergyMeanBlockLen = 4;
					OURS.errorEnergyMeanK =
					    THEIRS.errorEnergyMeanK = 0.75f;
					OURS.meanErrorEnergyCurrent =
					    THEIRS.meanErrorEnergyCurrent = 2.25f;
					OURS.word_a4 = THEIRS.word_a4 =
					    (unsigned)(tag & 1);
					OURS.meanErrorCount =
					    THEIRS.meanErrorCount =
					    (tag % 5 == 0)
					    ? V90EQU_MEAN_ERROR_LEN - 1u : 3u;
					OURS.meanErrorFull =
					    THEIRS.meanErrorFull = 0;
					OURS.word_34 = THEIRS.word_34 =
					    (unsigned)(tag % 4);
					ARENA_PARAMS->LINEAR_EQU_FADE_EDGES_CYCLE
					    = 3;
					OURS.linearEquWindowHalf =
					    THEIRS.linearEquWindowHalf =
					    le > 2u ? 2u : 1u;
					OURS.dfeWindowHalf =
					    THEIRS.dfeWindowHalf =
					    dfe > 2u ? 2u : 1u;

					OURS.connEval = (V90ConnectionEvaluator *)
					    ce_[0];
					THEIRS.connEval =
					    (V90ConnectionEvaluator *)ce_[1];

					for (k = 0; k < NIN; k++) {
						float v = amp_v[ai]
						    * (float)(((int)(k
							+ (unsigned)tag) % 9)
							- 4) * 0.25f;

						in_[0][k] = in_[1][k] = v;
						outsym_[0][k] = outsym_[1][k] =
						    (short)(0x1234 + k);
						outflt_[0][k] = outflt_[1][k] =
						    -1.5f * (float)k;
					}

					arena_snapshot();
					dsplib_debug_capture_reset();
					OURS.process(in_[0], n, outsym_[0],
						     outflt_[0], no_a);
					arena_switch();
					ref_equ_process(&THEIRS, in_[1], n,
							outsym_[1], outflt_[1],
							&no_b);

					/* The five channels. */
					diff_eq_int("nOut (%ld)", (long)no_a,
						    (long)no_b, tag);
					diff_eq_obj_(__FILE__, __LINE__,
						     "outSym", "short[NIN]",
						     outsym_[0], outsym_[1],
						     sizeof(outsym_[0]), tag);
					diff_eq_obj_(__FILE__, __LINE__,
						     "outFloat", "float[NIN]",
						     outflt_[0], outflt_[1],
						     sizeof(outflt_[0]), tag);
					diff_eq_int("in is not written (%ld)",
						    memcmp(in_[0], in_[1],
							   sizeof(in_[0])) == 0,
						    1, tag);

					/*
					 * The one field the two sides are MEANT
					 * to differ in.  Checked, then
					 * normalised so the object comparison
					 * covers everything else exactly.
					 */
					diff_eq_int("connEval is not written "
						    "(%ld)",
						    (long)(OURS.connEval
							   == (V90ConnectionEvaluator *)
							      ce_[0]
							   && THEIRS.connEval
							   == (V90ConnectionEvaluator *)
							      ce_[1]), 1, tag);
					OURS.connEval = THEIRS.connEval =
					    (V90ConnectionEvaluator *)ce_[0];

					diff_eq_obj("after process", V90Equalizer,
						    &OURS, &THEIRS, tag);
					arena_compare("the arena after process",
						      tag);
					diff_eq_obj_(__FILE__, __LINE__,
						     "the connection evaluator",
						     "V90ConnectionEvaluator",
						     ce_[0], ce_[1],
						     sizeof(V90ConnectionEvaluator),
						     tag);
					diff_eq_int("no store past the peer "
						    "(%ld)",
						    memcmp(ce_[1]
							   + sizeof(V90ConnectionEvaluator),
							   ce_seed
							   + sizeof(V90ConnectionEvaluator),
							   CE_SLOT
							   - sizeof(V90ConnectionEvaluator))
						    == 0, 1, tag);
					diff_eq_int("no store past the object "
						    "(%ld)", guard_equal(), 1,
						    tag);
					diff_eq_int("transcript (%ld)",
						    strcmp(dsplib_debug_capture_text(0),
							   dsplib_debug_capture_text(1))
						    == 0, 1, tag);

					/*
					 * The anti-vacuity counters, every one
					 * read off the REFERENCE side after the
					 * call and not off what the fixture
					 * planted.
					 */
					if (no_b == 0)
						saw_zero_n = 1;
					if (n & 1u)
						saw_odd = 1;
					if (held)
						saw_held = 1;
					if (zd)
						saw_zero_dfe = 1;
					else
						saw_dfe = 1;
					if (THEIRS.word_20 > (int)(w1c - le - 1u)
					    || THEIRS.word_20
					       == (int)(w1c - le - 1u))
						saw_wrap = 1;
					if (THEIRS.word_70 == 0 && no_b != 0)
						saw_close = 1;
					if (THEIRS.word_34 == 0)
						saw_fade = 1;
					if (dsplib_debug_capture_lines(1) > 0)
						saw_burst = 1;
					/*
					 * THE HIGH-ERROR ARM IS GUARDED ON
					 * `state > 1` AND RESET IS STATE 0, so
					 * a symbol whose error is past the
					 * threshold must leave `word_94`
					 * ALONE.  The error is recomputed from
					 * the reference's own two outputs --
					 * `outFloat[j] - (float)outSym[j]` is
					 * exactly what the function subtracts
					 * -- so this counts an observable and
					 * the check beside it is what the
					 * guard is worth.
					 */
					{
						unsigned int q;

						for (q = 0; q < no_b; q++) {
							float e = outflt_[1][q]
							    - (float)outsym_[1][q];

							if (e > 300.0f
							    || e < -300.0f) {
								saw_bigerr = 1;
								break;
							}
						}
					}
					/*
					 * `word_94` counts high-error events and
					 * only the guarded arm increments it,
					 * so in state 0 it can fall to zero
					 * when a burst closes and can never
					 * RISE -- whatever the error was.
					 */
					diff_eq_int("state 0 never raises the "
						    "high-error count (%ld)",
						    (long)(THEIRS.word_94
							   <= w94_v[w94i]), 1,
						    tag);
					if (THEIRS.word_94 == 0
					    && w94_v[w94i] != 0)
						saw_clean_close = 1;
					if (ai == NAMP - 1 && no_b > 0)
						saw_overflow = 1;

					if (prev_sym >= 0
					    && (long)outsym_[1][0] != prev_sym)
						sep_sym++;
					if (prev_nout >= 0
					    && (long)no_b != prev_nout)
						sep_nout++;
					prev_sym = outsym_[1][0];
					prev_nout = no_b;
				}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("nOut came back zero somewhere", saw_zero_n, 1, 0);
	diff_eq_int("an odd sample was carried out", saw_odd, 1, 0);
	diff_eq_int("a carried sample was consumed", saw_held, 1, 0);
	diff_eq_int("a zero DFE was driven", saw_zero_dfe, 1, 0);
	diff_eq_int("a NON-zero DFE was driven", saw_dfe, 1, 0);
	diff_eq_int("the cursor wrapped", saw_wrap, 1, 0);
	diff_eq_int("an error block closed", saw_close, 1, 0);
	diff_eq_int("the fade-edges cycle fired", saw_fade, 1, 0);
	diff_eq_int("a symbol past the error threshold was driven", saw_bigerr,
		    1, 0);
	diff_eq_int("a burst-length line was printed", saw_burst, 1, 0);
	diff_eq_int("a burst was closed by four clean symbols",
		    saw_clean_close, 1, 0);
	diff_eq_int("the squared error overflowed 32 bits", saw_overflow, 1, 0);
	diff_eq_int("the symbol output separated trials",
		    sep_sym > 32 ? 1 : 0, 1, 0);
	diff_eq_int("nOut separated trials", sep_nout > 32 ? 1 : 0, 1, 0);

	return diff_end();
}

int
main(void)
{
	return run_reset_arm();
}
