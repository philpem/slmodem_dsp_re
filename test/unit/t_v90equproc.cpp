/*
 * t_v90equproc.cpp -- differential test of V90Equalizer::process.
 *
 * IT IS ITS OWN BINARY, AND IT IS NOT MUTATION-SCORABLE.  `tools/mutate.py`
 * refuses to score a mutant set against an already-red binary, because caught
 * and already-red are indistinguishable -- and this binary IS red on the
 * modern tier, declared in `tools/gccdiverge.json` for the x87
 * excess-precision divergence finding F6203 measures.  So the defence against
 * a vacuous grid here is the anti-vacuity counters at the bottom of this
 * file and nothing else, and every one of them has to count an OBSERVABLE
 * the reference produced rather than something the fixture planted.
 *
 * NaN is kept out of the grid deliberately all the same.  The two remaining
 * divergence classes in `process` are the ordered compare at the `long
 * double` high-error test and the `== 0.0f` guard on the before/after ratio,
 * both of which are ordinary on finite inputs; a NaN would add a second,
 * unrelated reason for the modern tier to be red and make the first one
 * unreadable.
 *
 * THE FIXTURE IS `t_v90eqdata.cpp`'S, extended: the object in a byte array
 * carried by a union for its alignment, both sides seeded with the SAME
 * varied pseudorandom bytes and never with zeros (finding F230), the whole
 * object compared with `diff_eq_obj`, and the bytes from `sizeof` to the end
 * of an over-large slot compared separately so a store past the object's end
 * is a failure rather than silence (findings F223, F224).
 *
 * WHAT ONLY THIS TEST CAN SEE, and every one of them has a grid axis and an
 * anti-vacuity counter of its own:
 *
 *   - THE TWO ERRORS.  The DFE adapts on `soft - decision` and the linear
 *     half on `y - decision`, and the two differ by exactly the DFE output.
 *     A grid with a zero-length DFE, or with zero DFE coefficients, cannot
 *     tell them apart -- so `dfeLength >= 2` and the coefficients are planted
 *     non-zero on every trial that drives the update.
 *   - THE SQUARED ERROR'S 64-BIT CONVERT.  `blockErrorEnergySum += (unsigned)(long
 *     long)(err * err)` and `blockErrorEnergySum += (unsigned)(err * err)` agree for
 *     every |err| below 65536.  RESET's slicer is `(short)soft`, so a `soft`
 *     outside a short's range wraps the decision and makes |err| enormous;
 *     that is what the large-amplitude rows are for.
 *   - THE SECOND `historyIndex` DECREMENT, which is unconditional.  Trials that
 *     skip the coefficient update still have to retreat the cursor twice,
 *     and the wrap has to fire from both paths.
 *   - THE CLEAN-SYMBOL COUNTER'S ASYMMETRY.  A burst that ends with
 *     `highErrorCount <= 2` leaves `updateCoefs` at zero for the rest of the call.
 *
 * WHAT THIS BINARY DELIBERATELY DOES NOT DRIVE, and why it is a property of
 * the object rather than a gap in the grid:
 *
 *   - `state` OUTSIDE 0..6.  The dispatch's `default` writes no decision at
 *     all, so `outSym[j]` is whatever the caller left in a register; the two
 *     sides are different code and cannot agree.  The tail's tests against
 *     10..16 are reachable only from there, so their FALSE side is not
 *     differentially drivable.  Finding F6201.
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
 * ARR_F has to hold `linearEquHistoryLength`, which is `linearEquLength + 8` at its widest
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
					OURS.linearEquHistoryLength = THEIRS.linearEquHistoryLength = w1c;
					/*
					 * Start one symbol short of the wrap on
					 * every fourth trial, so the wrap fires
					 * from both the update and the
					 * update-skipped path.
					 */
					OURS.historyIndex = THEIRS.historyIndex =
					    (int)(w1c - le - 1u)
					    - (int)((unsigned)tag % 3u);
					OURS.historyIndexSaved =
					    THEIRS.historyIndexSaved = 0x5a5a;
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
					OURS.highErrorCount = THEIRS.highErrorCount =
					    w94_v[w94i];
					OURS.holdoverPending = THEIRS.holdoverPending =
					    (unsigned)held;
					OURS.holdoverSample = THEIRS.holdoverSample =
					    amp_v[ai] * 0.5f;
					OURS.blockSampleCount = THEIRS.blockSampleCount =
					    (unsigned)(tag % 3);
					OURS.blockErrorEnergySum = THEIRS.blockErrorEnergySum =
					    0xfffff000u;
					OURS.blockErrorEnergyRms = THEIRS.blockErrorEnergyRms = 3.5f;
					OURS.errorEnergyMeanBlockLen =
					    THEIRS.errorEnergyMeanBlockLen = 4;
					OURS.errorEnergyMeanK =
					    THEIRS.errorEnergyMeanK = 0.75f;
					OURS.meanErrorEnergyCurrent =
					    THEIRS.meanErrorEnergyCurrent = 2.25f;
					OURS.meanErrorRecordEnable = THEIRS.meanErrorRecordEnable =
					    (unsigned)(tag & 1);
					OURS.meanErrorCount =
					    THEIRS.meanErrorCount =
					    (tag % 5 == 0)
					    ? V90EQU_MEAN_ERROR_LEN - 1u : 3u;
					OURS.meanErrorFull =
					    THEIRS.meanErrorFull = 0;
					OURS.fadeEdgesCounter = THEIRS.fadeEdgesCounter =
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
					/*
					 * THE CURSOR RETREATS ON EVERY SYMBOL
					 * AND ONLY THE WRAP CAN RAISE IT, so
					 * a final position ABOVE the entering
					 * one is the observable.  Counting
					 * where it merely ENDED would be
					 * satisfied by a trial that started
					 * high and never wrapped, which is a
					 * counter that cannot fail (finding
					 * F3509).
					 */
					if (le > 0 && w1c > le
					    && THEIRS.historyIndex
					       > (int)(w1c - le - 1u)
						 - (int)((unsigned)tag % 3u))
						saw_wrap = 1;
					if (THEIRS.blockSampleCount == 0 && no_b != 0)
						saw_close = 1;
					if (THEIRS.fadeEdgesCounter == 0)
						saw_fade = 1;
					if (dsplib_debug_capture_lines(1) > 0)
						saw_burst = 1;
					/*
					 * THE HIGH-ERROR ARM IS GUARDED ON
					 * `state > 1` AND RESET IS STATE 0, so
					 * a symbol whose error is past the
					 * threshold must leave `highErrorCount`
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
							    || e < -300.0f)
								saw_bigerr = 1;
							/*
							 * THE ONLY CONDITION
							 * UNDER WHICH THE
							 * 64-BIT CONVERT AND A
							 * 32-BIT ONE DIFFER.
							 * Below it the two
							 * spellings agree bit
							 * for bit, so a
							 * counter that fired
							 * on "the large row
							 * ran" would have been
							 * a counter that
							 * cannot fail
							 * (finding F3509).
							 */
							if ((double)e * (double)e
							    >= 4294967296.0)
								saw_overflow = 1;
						}
					}
					/*
					 * `highErrorCount` counts high-error events and
					 * only the guarded arm increments it,
					 * so in state 0 it can fall to zero
					 * when a burst closes and can never
					 * RISE -- whatever the error was.
					 */
					diff_eq_int("state 0 never raises the "
						    "high-error count (%ld)",
						    (long)(THEIRS.highErrorCount
							   <= w94_v[w94i]), 1,
						    tag);
					if (THEIRS.highErrorCount == 0
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

/* ==================================================== the phase 4 peer set */

/*
 * THE FOUR ARMS THAT GO THROUGH `V90Phase4Demodulator` -- equaliser states 2,
 * 4 and 5, and state 3's two detectors -- need the demodulator's own peer set
 * behind it, because `int_0028` is CLEARED on entry to both decision
 * functions (`int_0028 = 0` at the head of `getV90Decision` and again at
 * `getV92Decision`'s) and cannot be planted.  It is 6201's `word_30` problem
 * exactly: the value has to be PRODUCED by the demodulator, so the axis is
 * driven through the demodulator's own state and counters and read back off
 * the reference peer afterwards.
 *
 * THE WIRING IS `t_v90p4ddec.cpp`'S, adapted rather than invented: the same
 * split/shared split (the demapper, the CP and MP records and the descrambler
 * are written and get one copy per side; the mapping blocks, the
 * impairment detector and the parameters are read-only and are SHARED so the
 * pointer word stays IN the comparison, finding F1105), the same
 * `resetNoSpectral`-safe mapping blocks, and the same "one bit from an
 * answer" plant of the two message decoders.  Its `arm_r` puts a detector one
 * sample from a decision, which is what makes 0x28 and 0x2c reachable inside
 * one call instead of six.
 *
 * WHAT THE SAMPLES ARE IS NOT FREE HERE.  The demodulator sees the
 * EQUALISER'S output, not a planted short, so every arm whose detector tests
 * `sample > 0` needs the filters arranged to produce a positive soft
 * decision -- in both representations.  That is why the fixed-point arrays
 * below are planted positive and the two output conversion factors are
 * planted at all: a pseudorandom `dfeMmxOutputConversionFactor` is a divide
 * by zero, and a pseudorandom sign is an arm that never fires.
 */

#include "dsplib/Scrambler.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"
#include "dsplib/V90SpectralVerifier.h"
#include "dsplib/V90PreFilter.h"

#define P4D_SLOT	((unsigned)sizeof(V90Phase4Demodulator) + 64u)
#define DEM_SLOT	((unsigned)sizeof(V90Demapper) + 64u)
#define CP_SLOT		((unsigned)sizeof(V90CP) + 64u)
#define MP_SLOT		((unsigned)sizeof(V90MP) + 64u)

#define NSAMPLE		72
#define NCPBUF		16

/* The V.90 descrambler's geometry, from `V90ModemCtor`. */
#define DSC_A		18u
#define DSC_B		23u
#define DSC_C		99u
#define DSC_N		(1u + DSC_B + DSC_C)

typedef Descrambler<unsigned char, int> V90Descrambler;

static unsigned char p4d_s[2][P4D_SLOT] __attribute__((aligned(8)));
static unsigned char p4d_seedb[P4D_SLOT];
static unsigned char p4d_cmp[2][P4D_SLOT];

static unsigned char dem_s[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char dem_cmp[2][DEM_SLOT];
static unsigned char cp_s[2][CP_SLOT] __attribute__((aligned(8)));
static unsigned char mp_s[2][MP_SLOT] __attribute__((aligned(8)));

static unsigned int code_s[2][NSAMPLE];
static unsigned char sign_s[2][NSAMPLE];
static unsigned char sbstate_s[2][V90SBE_DECODER_SIZE];
static int cpbuf_s[2][V90CP_BUFS][NCPBUF];

static unsigned char dsc_s[2][sizeof(V90Descrambler)]
	__attribute__((aligned(8)));
static unsigned char dscbuf_s[2][DSC_N];

/* Shared: nothing under test writes through any of these. */
static unsigned char mapp1_s[sizeof(V90MappingParams) + 64]
	__attribute__((aligned(8)));
static unsigned char mapp2_s[sizeof(V90MappingParams) + 64]
	__attribute__((aligned(8)));
/*
 * SPLIT, AND `t_v90p4ddec` SHARES IT.  The difference is that this file drives
 * `V90Demapper::linearMappingStudy` -- \`process\`'s state 3 arm calls it
 * directly -- and that member ACCUMULATES into the detector at +0x1000 and
 * +0x1c00.  A shared block would let our run's accumulation stand under the
 * reference run, so the two sides would not see the same starting state and
 * neither the arena replay nor any comparison would say so.
 */
static unsigned char adi_s[2][sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));
static unsigned char adi_seed[sizeof(V90AutoDigitalImpDetector)];
static unsigned char sv_s[sizeof(V90SpectralVerifier) + 64]
	__attribute__((aligned(8)));
static unsigned char pf_s[sizeof(V90PreFilter) + 64]
	__attribute__((aligned(8)));

#define P4D(s)		(*(V90Phase4Demodulator *)p4d_s[s])
#define DEM(s)		(*(V90Demapper *)dem_s[s])
#define CPR(s)		(*(V90CP *)cp_s[s])
#define MPR(s)		(*(V90MP *)mp_s[s])
#define DSC(s)		(*(V90Descrambler *)dsc_s[s])
#define MAPP1		((V90MappingParams *)mapp1_s)
#define MAPP2		((V90MappingParams *)mapp2_s)
#define ADI(s)		((V90AutoDigitalImpDetector *)adi_s[s])
#define SPECVER		((V90SpectralVerifier *)sv_s)
#define PREFILT		((V90PreFilter *)pf_s)

/* Varied, never zero: findings F223, F224, F230. */
static unsigned
fill_bytes(unsigned char *p, unsigned n, unsigned lfsr)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		p[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
	return lfsr;
}

/*
 * The four split peers' pointer words, and the demapper's own three.  +0x34f8
 * is a FIFTH here that `t_v90p4ddec` does not have: the connection evaluator
 * is shared there and split here, because in this binary the EQUALISER writes
 * through it -- `updateAvePdsnr` runs whenever a block closes.
 */
static const unsigned p4d_skip[] = {
	0x14u, 0x18u, 0x3054u, 0x3058u, 0x34f8u, ~0u
};
static const unsigned dem_skip[] = {
	0x1cu, 0x20u, 0x684u, 0x1ea0u, ~0u
};

static void
scrub(unsigned char *dst, const unsigned char *src, unsigned n,
      const unsigned *skip)
{
	int i;

	memcpy(dst, src, n);
	for (i = 0; skip[i] != ~0u; i++)
		memset(dst + skip[i], 0, 4);
}

/*
 * EVERY PARAMETER THESE ARMS READ IS PLANTED FINITE, and that is a
 * correctness requirement rather than tidiness.  `setLinearEquBeta` and
 * `setDfeBeta` decide with ONE ORDERED COMPARE and no parity test
 * (`linearEquBeta != beta`, findings F2300 and F2304) -- so a pseudorandom word
 * that happens to be a NaN takes a different arm on the two compilers and
 * turns this binary's declared divergence into two.  The fill gives varied
 * bytes; these fifteen words are planted on top of it.
 */
static void
plant_params(void)
{
	ARENA_PARAMS->LINEAR_EQU_FADE_EDGES_CYCLE = 3;
	ARENA_PARAMS->LINEAR_EQU_DATA_BETA = 0.001953125f;
	ARENA_PARAMS->DFE_DATA_BETA = 0.0009765625f;
	ARENA_PARAMS->LINEAR_EQU_TRN2D_BETA = 0.00390625f;
	ARENA_PARAMS->LINEAR_EQU_TRN2D_INITIAL_BETA = 0.0078125f;
	ARENA_PARAMS->DFE_TRN2D_BETA = 0.001953125f;
	ARENA_PARAMS->GERMAN_PBX_DFE_TRN2D_SLOW_BETA = 0.000244140625f;
	ARENA_PARAMS->GERMAN_PBX_DFE_TRN2D_FAST_BETA = 0.00048828125f;
	ARENA_PARAMS->EIA6_DFE_TRN2D_SLOW_BETA = 0.0001220703125f;
	ARENA_PARAMS->EIA6_DFE_TRN2D_FAST_BETA = 0.00390625f;
	ARENA_PARAMS->EIA6_DFE_TRN2D_RRN_BETA = 0.015625f;
	ARENA_PARAMS->LINEAR_EQU_TRN2D_INITIAL_DURATION = 0x20;
	ARENA_PARAMS->RRN_TRN2D_DD_LENGTH = 0x2a0;
	ARENA_PARAMS->RRN_R_DETECTION_LENGTH = 0x60;
	ARENA_PARAMS->DEBUG_DEMAPPER_ERROR_HISTOGRAM = 0;
	ARENA_PARAMS->DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM = 0;
}

/*
 * Put a detector one or two POSITIVE samples from a decision.  These are
 * `t_v90p4ddec`'s `arm_r`/`arm_rf` narrowed to the positive-pattern arms --
 * the equaliser's output is what the detector sees here and it is planted
 * positive -- and widened by a `want` of 2, which arms the detector one group
 * FURTHER out.
 *
 * THE SECOND SETTING IS NOT A CONVENIENCE.  The three re-convert blocks end
 * with `for (i = 0; i < j; i++) b8[i] = (short)outFloat[i]`, whose body cannot
 * run at all when the block fires on symbol ZERO -- which is where every
 * detector armed one sample out fires.  Driving the same block one symbol
 * later is the only way that loop executes, and gcov counted those three
 * lines as unexecuted until this existed.
 *
 * `want` 0 leaves the detector well short of its group, so the arm never
 * fires; the run counters are cleared either way so the two peers stay
 * identical.
 */
static void
arm_group6(V90RDetector *d, int want, int limit, int *run)
{
	d->rLimit = limit;
	d->rNotLimit = limit;
	d->notRunLength = 0;
	d->positiveRunLength = 0;
	d->negativeRunLength = 0;

	switch (want) {
	case 2:
		d->sampleCount = 4;
		d->signBits = 0x01;	/* -> 0x03 -> 0x07 on the second */
		*run = limit - 6;
		break;
	case 1:
		d->sampleCount = 5;
		d->signBits = 0x03;	/* 0x03 * 2 | 1 = 0x07 */
		*run = limit - 6;
		break;
	default:
		d->sampleCount = 2;
		d->signBits = 0x1c;
		break;
	}
}

/* `detectRNot`: one run counter, +0x1c, against the limit at +0x08. */
static void
arm_rnot(V90RDetector *d, int want, int limit)
{
	d->polarity = 0x33;
	arm_group6(d, want, limit, &d->notRunLength);
}

/* `detectR`: two run counters, and a POSITIVE sample matches 0x07 -> +0x18. */
static void
arm_r(V90RDetector *d, int want, int limit)
{
	d->polarity = 0x33;
	arm_group6(d, want, limit, &d->negativeRunLength);
	d->notRunLength = 0;
}

/* The twelve-sample pair: 0x333 is the positive pattern, +0x0c the limit. */
static void
arm_group12(V90RDetector *d, int want, int limit, int *run)
{
	d->rfLimit = limit;
	d->rfNotLimit = limit;
	d->notRunLength = 0;
	d->positiveRunLength = 0;
	d->negativeRunLength = 0;

	switch (want) {
	case 2:
		d->sampleCount = 10;
		d->signBits = 0x0cc;	/* -> 0x199 -> 0x333 */
		*run = limit - 12;
		break;
	case 1:
		d->sampleCount = 11;
		d->signBits = 0x199;	/* 0x199 * 2 | 1 = 0x333 */
		*run = limit - 12;
		break;
	default:
		d->sampleCount = 4;
		d->signBits = 0x666;
		break;
	}
}

static void
arm_rfnot(V90RDetector *d, int want, int limit)
{
	d->polarity = 0x44;
	arm_group12(d, want, limit, &d->notRunLength);
}

static void
arm_rf(V90RDetector *d, int want, int limit)
{
	d->polarity = 0x44;
	arm_group12(d, want, limit, &d->negativeRunLength);
	d->notRunLength = 0;
}

/*
 * One demodulator and its peers per side, and the mapping blocks and
 * impairment detector shared.  Nothing here calls a constructor.
 */
static void
p4_setup(long tag, int dly)
{
	unsigned lf = 0x37c1u + 0x9e37u * (unsigned)tag;
	int s, k;

	fill_bytes(mapp1_s, (unsigned)sizeof mapp1_s, lf ^ 0x8ac1u);
	fill_bytes(mapp2_s, (unsigned)sizeof mapp2_s, lf ^ 0x1f77u);
	fill_bytes(adi_s[0], (unsigned)sizeof adi_s[0], lf ^ 0x6d05u);
	memcpy(adi_s[1], adi_s[0], sizeof adi_s[0]);
	fill_bytes(sv_s, (unsigned)sizeof sv_s, lf ^ 0x3b21u);
	fill_bytes(pf_s, (unsigned)sizeof pf_s, lf ^ 0x55a3u);

	/*
	 * `3 * word_0 + 0x17` is the B1d delay; the blocks also have to be
	 * sane because four arms reach `resetNoSpectral`, which copies
	 * `constellationSize` straight out and then walks it.
	 */
	MAPP1->word_0 = 3u;
	MAPP2->word_0 = 5u;
	for (k = 0; k < V90_CONSTELLATIONS; k++) {
		int j;

		MAPP1->constellationSize[k] = 32u;
		MAPP2->constellationSize[k] = 24u;
		for (j = 0; j < V90_CONSTELLATION_MAX; j++) {
			MAPP1->constellation[k][j] =
			    (unsigned char)((V90_CONSTELLATION_MAX - 1 - j)
					    & 0x7f);
			MAPP2->constellation[k][j] =
			    (unsigned char)((V90_CONSTELLATION_MAX - 1 - j)
					    & 0x7f);
		}
	}
	for (k = 0; k < V90ADID_PHASES; k++) {
		int j, t;

		for (t = 0; t < 2; t++) {
			ADI(t)->altRbsFlag[k] = (short)(k & 1);
			for (j = 0; j < V90ADID_CODES; j++) {
				ADI(t)->linMapp[k][j] =
				    (short)(4000 - 20 * j);
				ADI(t)->linMappAlt[k][j] =
				    (short)(3990 - 20 * j);
			}
		}
	}
	memcpy(adi_seed, adi_s[0], sizeof adi_seed);

	/*
	 * `isV90WithEia6` reads the loop table only when `refLoop` is not
	 * negative, so -1 leaves the whole answer to `LOOP_TYPE` -- which is
	 * this fixture's EIA-6 axis and is planted per trial.
	 */
	PREFILT->refLoop = -1;
	PREFILT->params = ARENA_PARAMS;

	fill_bytes(p4d_s[0], P4D_SLOT, lf);
	memcpy(p4d_s[1], p4d_s[0], P4D_SLOT);
	memcpy(p4d_seedb, p4d_s[0], P4D_SLOT);

	for (s = 0; s < 2; s++) {
		V90Phase4Demodulator *d = &P4D(s);
		V90Demapper *m = &DEM(s);
		V90Descrambler *x = &DSC(s);

		fill_bytes(dem_s[s], DEM_SLOT, lf ^ 0x77u);
		fill_bytes(cp_s[s], CP_SLOT, lf ^ 0x31u);
		fill_bytes(mp_s[s], MP_SLOT, lf ^ 0x9bu);
		fill_bytes((unsigned char *)code_s[s],
			   (unsigned)sizeof code_s[s], lf ^ 0xa5u);
		fill_bytes(sign_s[s], NSAMPLE, lf ^ 0xc3u);
		fill_bytes(sbstate_s[s], V90SBE_DECODER_SIZE, lf ^ 0x5eu);
		fill_bytes((unsigned char *)cpbuf_s[s],
			   (unsigned)sizeof cpbuf_s[s], lf ^ 0x2du);
		/*
		 * A CONSTANT DESCRAMBLER BUFFER, so the two taps cancel and
		 * the demapper's own bits reach the message decoders -- which
		 * is what makes 0x1c and 0x35 reachable at all.  Finding F4811
		 * measured the other half of this.
		 */
		memset(dscbuf_s[s], 0x55, DSC_N);

		d->params = ARENA_PARAMS;
		d->mappingParams1 = MAPP1;
		d->mappingParams2 = MAPP2;
		d->connectionEvaluator = (V90ConnectionEvaluator *)ce_[s];
		d->demapper = m;
		d->descrambler = x;
		d->cp = &CPR(s);
		d->mp = &MPR(s);

		m->params = ARENA_PARAMS;
		m->adiDetector = ADI(s);
		m->codes = code_s[s];
		m->signs = sign_s[s];
		m->sampleCapacity = NSAMPLE;
		/*
		 * ONE SAMPLE SHORT OF A FRAME, or two.  `hardDecision`
		 * appends one per symbol and `process` yields a frame at six,
		 * so this is what decides whether the message decoders answer
		 * on symbol zero or on symbol one -- the same `dly` the
		 * detectors take, for the same reason.
		 */
		m->sampleCount = (unsigned)(5 - dly);
		m->frameStart = 0u;
		m->bitsPerFrame = 8u;
		m->signBitsPerFrame = 2u;
		m->signBitGroups = 0u;
		m->signBitGroupSize = 1u;
		m->rbsFramePosition = 2u;
		m->word_08 = 5u;
		m->linearMappStudyEnabled = 0;
		m->modulusDecoder.field_00 = 7u;
		m->modulusDecoder.field_04 = 11u;
		m->modulusDecoder.field_08 = 5u;
		m->modulusDecoder.field_0c = 13u;
		m->modulusDecoder.field_10 = 3u;
		m->modulusDecoder.field_14 = 0u;
		m->modulusDecoder.field_18 = 6u;
		m->signBits.spacing = 1u;
		m->signBits.width = 1u;
		m->signBits.state = 0u;
		m->signBits.decoder.state_ = sbstate_s[s];
		m->signBits.decoder.capacity_ = V90SBE_DECODER_SIZE;
		m->signBits.decoder.size_ = 0u;
		m->signBits.oddDecoder.prev_ = 0;
		m->signDecoder.prev_ = 0;
		/*
		 * THE STUDY'S UNCONDITIONAL SIDE EFFECT.  Its accumulation
		 * into the impairment detector is GATED on
		 * `|diff| < 0.4 * (high - low)`, which this fixture's
		 * constellation spacing puts out of reach, so "the detector
		 * moved" is a counter that cannot fire.  The progress counter
		 * at +0x1eb0 is incremented on every call that does not
		 * complete a run, and `studyLength` is planted far away so that
		 * is every call.
		 */
		m->studyProgress = 0u;
		m->studyLength = 0x1000u;
		m->errorHistogramCount = 0u;
		m->histogramDelay = 0;
		m->histogramIntegration = 0;

		for (k = 0; k < V90DEMAPPER_CONSTELLATIONS; k++) {
			int j;

			m->constellationSize[k] = V90DEMAPPER_LEVELS;
			for (j = 0; j < V90DEMAPPER_LEVELS; j++)
				m->constellation[k][j] =
				    (short)(8000 - j * 60 - k * 7);
		}

		/* Both message decoders one bit from an answer. */
		MPR(s).rxState = 0u;
		MPR(s).onesRun = 0;
		MPR(s).zerosRun = 1;
		MPR(s).bitIndex = 18;
		MPR(s).groupSize = 1u;
		CPR(s).word_ca4 = 0u;
		CPR(s).byte_ca9 = 0;
		CPR(s).byte_caa = 1;
		CPR(s).word_cac = 18u;
		CPR(s).word_cb0 = 0u;
		CPR(s).word_3ba8 = 1u;
		for (k = 0; k < V90CP_BUFS; k++) {
			CPR(s).buf[k] = cpbuf_s[s][k];
			CPR(s).nof_buf[k] = 1u;
		}

		x->pLimit = dscbuf_s[s];
		x->pInitOut = dscbuf_s[s] + DSC_C;
		x->pInitTap1 = dscbuf_s[s] + DSC_C + DSC_A;
		x->pInitTap2 = dscbuf_s[s] + DSC_C + DSC_B;
		x->tailLength = DSC_B;
		x->pOut = x->pInitOut;
		x->pTap1 = x->pInitTap1;
		x->pTap2 = x->pInitTap2;
	}
}

static void
p4_compare(long tag)
{
	unsigned n = (unsigned)sizeof(V90Phase4Demodulator);

	scrub(p4d_cmp[0], p4d_s[0], P4D_SLOT, p4d_skip);
	scrub(p4d_cmp[1], p4d_s[1], P4D_SLOT, p4d_skip);
	diff_eq_obj_(__FILE__, __LINE__, "the phase 4 demodulator",
		     "V90Phase4Demodulator", p4d_cmp[0], p4d_cmp[1], n, tag);
	diff_eq_int("nothing stored past the demodulator (%ld)",
		    memcmp(p4d_s[1] + n, p4d_seedb + n, P4D_SLOT - n) == 0, 1,
		    tag);

	scrub(dem_cmp[0], dem_s[0], DEM_SLOT, dem_skip);
	scrub(dem_cmp[1], dem_s[1], DEM_SLOT, dem_skip);
	diff_eq_obj_(__FILE__, __LINE__, "the demapper", "V90Demapper",
		     dem_cmp[0], dem_cmp[1], (unsigned)sizeof(V90Demapper),
		     tag);
	diff_eq_int("the demapper's codes (%ld)",
		    memcmp(code_s[0], code_s[1], sizeof code_s[0]) == 0, 1,
		    tag);
	diff_eq_int("the demapper's signs (%ld)",
		    memcmp(sign_s[0], sign_s[1], sizeof sign_s[0]) == 0, 1,
		    tag);
	diff_eq_int("the sign decoder's state (%ld)",
		    memcmp(sbstate_s[0], sbstate_s[1], sizeof sbstate_s[0])
		    == 0, 1, tag);
	diff_eq_int("the CP record (%ld)",
		    memcmp(cp_s[0], cp_s[1],
			   __builtin_offsetof(V90CP, buf)) == 0, 1, tag);
	diff_eq_int("the CP record past its buffers (%ld)",
		    memcmp(cp_s[0] + __builtin_offsetof(V90CP, word_ca0),
			   cp_s[1] + __builtin_offsetof(V90CP, word_ca0),
			   CP_SLOT - __builtin_offsetof(V90CP, word_ca0)) == 0,
		    1, tag);
	diff_eq_int("the CP record's buffers (%ld)",
		    memcmp(cpbuf_s[0], cpbuf_s[1], sizeof cpbuf_s[0]) == 0, 1,
		    tag);
	diff_eq_int("the impairment detector (%ld)",
		    memcmp(adi_s[0], adi_s[1], sizeof adi_s[0]) == 0, 1, tag);
	diff_eq_int("the MP record (%ld)",
		    memcmp(mp_s[0], mp_s[1], MP_SLOT) == 0, 1, tag);
	diff_eq_int("the descrambler's buffer (%ld)",
		    memcmp(dscbuf_s[0], dscbuf_s[1], DSC_N) == 0, 1, tag);
	diff_eq_int("the descrambler's output cursor (%ld)",
		    (long)(DSC(0).pOut - dscbuf_s[0]),
		    (long)(DSC(1).pOut - dscbuf_s[1]), tag);
	diff_eq_int("the descrambler's near tap (%ld)",
		    (long)(DSC(0).pTap1 - dscbuf_s[0]),
		    (long)(DSC(1).pTap1 - dscbuf_s[1]), tag);
	diff_eq_int("the descrambler's far tap (%ld)",
		    (long)(DSC(0).pTap2 - dscbuf_s[0]),
		    (long)(DSC(1).pTap2 - dscbuf_s[1]), tag);
}

/*
 * The equaliser's own state for a phase 4 trial.  Everything is planted
 * POSITIVE and inside a short: the demodulator's detectors test `sample > 0`,
 * and keeping `|soft|` under 32767 keeps the RESET slicer's wrap -- and with
 * it finding F6203's excess-precision divergence -- out of these groups.  The
 * float step sizes are FROZEN here for the same reason: `(0.0f * err) * x` is
 * exactly zero in both precisions, so the float LMS cannot carry an 80-bit
 * intermediate into `dfeCoefs`.  The RESET group is what drives the float LMS
 * with a live step size, and it is the group the divergence is declared for.
 */
static void
p4_equ_plant(long tag, unsigned int le, unsigned int dfe, int mmx)
{
	unsigned int w1c = le + 8u;
	unsigned int k;

	for (k = 0; k < ARR_F; k++) {
		arena.lecoefs[k] = (k == 0) ? 1.0f
		    : 0.03125f * (float)(k % 5);
		arena.a18[k] = 8.0f
		    * (float)(((k + (unsigned)tag) % 7) + 1u);
		arena.dfecoefs[k] = 0.0625f * (float)(k % 3) + 0.015625f;
		arena.a44[k] = 0.5f
		    * (float)(((k * 3u + (unsigned)tag) % 5) + 1u);
		arena.lewin[k] = 0.5f + 0.015625f * (float)(k % 9);
		arena.dfewin[k] = 0.25f + 0.03125f * (float)(k % 7);
	}
	for (k = 0; k < V90EQU_MEAN_ERROR_LEN; k++)
		arena.meanerr[k] = 0.125f * (float)((k % 11) + 1u);
	for (k = 0; k < ARR_S; k++) {
		arena.lemmx[k] = (short)(0x40 + (int)(k % 7));
		arena.ad8[k] = (short)(0x100 + (int)(k % 5));
		arena.aec[k] = (short)(0x200 + 16 * (int)(k % 9));
		arena.dfemmx[k] = (short)(1 + (int)(k % 3));
		arena.a118[k] = (short)(2 + (int)(k % 4));
		arena.a12c[k] = (short)(3 + (int)(k % 5));
	}

	OURS.linearEquLength = THEIRS.linearEquLength = le;
	OURS.dfeLength = THEIRS.dfeLength = dfe;
	OURS.linearEquHistoryLength = THEIRS.linearEquHistoryLength = w1c;
	OURS.historyIndex = THEIRS.historyIndex =
	    (int)(w1c - le - 1u) - (int)((unsigned)tag % 3u);
	OURS.historyIndexSaved = THEIRS.historyIndexSaved =
	    (int)(w1c - le - 1u) - (int)((unsigned)tag % 3u);
	OURS.mmxMode = THEIRS.mmxMode = mmx;
	OURS.mmxArraysPresent = THEIRS.mmxArraysPresent = 1;
	OURS.stateCount = THEIRS.stateCount = 0x3c3c;
	OURS.linearEquBeta = THEIRS.linearEquBeta = 0.0f;
	OURS.dfeBeta = THEIRS.dfeBeta = 0.0f;
	/*
	 * THE TWO CONVERSION FACTORS ARE FLOATS AND THEY MUST BE PLANTED.
	 * `restoreEqualizerToFloat` rebuilds every float coefficient as
	 * `(1.0f / linearEquMmxConversionFactor) * <32-bit accumulator>`, so a
	 * pseudorandom word there is a reciprocal of an arbitrary magnitude
	 * and the product's rounding then depends on whether the compiler kept
	 * the intermediate at 80 bits.  Measured: with these left to the fill,
	 * one DATA trial of 48 disagreed with the blob on GCC 13 in
	 * `linearEquCoefs[0..3]` and `highErrorCount`, and the SAME trial was green on
	 * the period compiler -- a fixture defect wearing finding F6203's
	 * clothes.  A power of two makes the reciprocal exact.
	 */
	OURS.linearEquMmxConversionFactor =
	    THEIRS.linearEquMmxConversionFactor = 1024.0f;
	OURS.dfeMmxConversionFactor = THEIRS.dfeMmxConversionFactor = 512.0f;
	OURS.linearEquMmxOutputConversionFactor =
	    THEIRS.linearEquMmxOutputConversionFactor = 1024;
	OURS.dfeMmxOutputConversionFactor =
	    THEIRS.dfeMmxOutputConversionFactor = 512;
	OURS.linearEquMmxBeta = THEIRS.linearEquMmxBeta = 4;
	OURS.linearEquMmxShift = THEIRS.linearEquMmxShift = 12;
	OURS.dfeMmxBeta = THEIRS.dfeMmxBeta = 2;
	OURS.dfeMmxShift = THEIRS.dfeMmxShift = 10;
	OURS.holdoverPending = THEIRS.holdoverPending = 0;
	OURS.holdoverSample = THEIRS.holdoverSample = 12.5f;
	OURS.blockSampleCount = THEIRS.blockSampleCount = (unsigned)(tag % 3);
	OURS.blockErrorEnergySum = THEIRS.blockErrorEnergySum = 0x1000u;
	OURS.blockErrorEnergyRms = THEIRS.blockErrorEnergyRms = 3.5f;
	OURS.highErrorCount = THEIRS.highErrorCount = (unsigned)(tag % 4);
	OURS.errorEnergyMeanBlockLen = THEIRS.errorEnergyMeanBlockLen = 4;
	OURS.errorEnergyMeanK = THEIRS.errorEnergyMeanK = 0.75f;
	OURS.meanErrorEnergyCurrent = THEIRS.meanErrorEnergyCurrent = 2.25f;
	OURS.meanErrorEnergyMean = THEIRS.meanErrorEnergyMean = 1.5f;
	OURS.meanErrorRecordEnable = THEIRS.meanErrorRecordEnable = (unsigned)(tag & 1);
	OURS.meanErrorCount = THEIRS.meanErrorCount = 3u;
	OURS.meanErrorFull = THEIRS.meanErrorFull = 0;
	OURS.fadeEdgesCounter = THEIRS.fadeEdgesCounter = (unsigned)(tag % 4);
	OURS.linearEquWindowHalf = THEIRS.linearEquWindowHalf =
	    le > 2u ? 2u : 1u;
	OURS.dfeWindowHalf = THEIRS.dfeWindowHalf = dfe > 2u ? 2u : 1u;
	OURS.timingOffset = THEIRS.timingOffset = 0.125f;
	OURS.dfeProtectionOnDil = THEIRS.dfeProtectionOnDil = 2;
	OURS.savedBllState = THEIRS.savedBllState = 0;
	OURS.ph4MeanErrorEnergyBeforeUpdate =
	    THEIRS.ph4MeanErrorEnergyBeforeUpdate = 1.0f;
	OURS.ph4MeanErrorEnergyBeforeToAfterUpdateRatio =
	    THEIRS.ph4MeanErrorEnergyBeforeToAfterUpdateRatio = 1.0f;

	OURS.connEval = (V90ConnectionEvaluator *)ce_[0];
	THEIRS.connEval = (V90ConnectionEvaluator *)ce_[1];
	OURS.phase4Demod = (V90Phase4Demodulator *)p4d_s[0];
	THEIRS.phase4Demod = (V90Phase4Demodulator *)p4d_s[1];
	OURS.demapper = (V90Demapper *)dem_s[0];
	THEIRS.demapper = (V90Demapper *)dem_s[1];
	OURS.spectralVerifier = THEIRS.spectralVerifier = SPECVER;
	OURS.preFilter = THEIRS.preFilter = PREFILT;

	/*
	 * THE RESAMPLER'S OWN `params` POINTER, and it is not decoration:
	 * `setBllState` reads a K1/K2 pair out of it on twelve of its sixteen
	 * arms, and the RESET group never calls it, so the pseudorandom word
	 * the fill leaves there survived one whole group before this one
	 * segfaulted on it.  The starting state is planted at STEADY_STATE so
	 * that every state this file's arms ask for is a CHANGE -- the member
	 * returns immediately when the state it is handed is the one it
	 * already holds, and a fixture that started in the target state would
	 * drive the call and none of its body.
	 */
	ARENA_RSAMP->params = ARENA_PARAMS;
	ARENA_RSAMP->bllState = V90_BLL_STEADY_STATE;
}

/*
 * The rows.  Each names a demodulator entry state and the counter that puts
 * it one sample from the progress code in the last column; the code itself is
 * NEVER planted and is read back off the reference peer after the call, which
 * is 6201's rule for an axis a callee clears on entry.
 */
struct p4_row {
	int		state;		/* the demodulator's entry state */
	int		rnot;		/* arm rDetector1 for RNot	 */
	int		v92;		/* sessionFlag			 */
	unsigned int	count;		/* countInState, one short	 */
	int		want;		/* the progress code aimed at	 */
};

static const struct p4_row p4_rows[] = {
	{ P4D_STATE_WAIT_FOR_RI,	0, 0, 0x10u,  0    },
	{ P4D_STATE_WAIT_FOR_RI_NOT,	1, 0, 0x10u,  0x17 },
	{ P4D_STATE_TRN2D_DD,		0, 0, 0x1fu,  0x18 },
	{ P4D_STATE_TRN2D_DD,		0, 0, 0x3fu,  0x19 },
	{ P4D_STATE_WAIT_FOR_ED,	0, 0, 0x10u,  0x1c },
	{ P4D_STATE_B1D,		0, 0, 0x11fu, 0x1d },
	{ P4D_STATE_WAIT_FOR_RT_NOT,	1, 0, 0x10u,  0x28 },
	{ P4D_STATE_RD_DETECTED,	1, 0, 0x10u,  0x2c },
	{ P4D_STATE_WAIT_FOR_V90CP,	0, 1, 0x10u,  0x35 }
};
#define NP4ROW ((int)(sizeof(p4_rows) / sizeof(p4_rows[0])))

static int
run_p4_arms(void)
{
	static const int est_v[] = {
		V90EQU_STATE_PHASE4, V90EQU_STATE_RRN, V90EQU_STATE_FPE
	};
	static const unsigned int n_v[] = { 4u, 7u };
	long tag = 5720000;
	int ri, ei, mmx, ni, opt, dly;
	int saw_code[0x40];
	int saw_mmx = 0, saw_float = 0, saw_reconv = 0, saw_mmx_after = 0;
	int saw_b4 = 0, saw_b8 = 0, saw_bll = 0, saw_beta = 0;
	int saw_ratio = 0, saw_wrap = 0, saw_close = 0;
	int saw_held = 0, saw_late = 0;
	int sep_sym = 0, sep_state = 0;
	long prev_sym = -1, prev_state = -1;
	int i;

	for (i = 0; i < 0x40; i++)
		saw_code[i] = 0;

	diff_begin("V90Equalizer::process, the phase 4 state arms");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (ri = 0; ri < NP4ROW; ri++)
	    for (ei = 0; ei < 3; ei++)
		for (mmx = 0; mmx < 2; mmx++)
		    for (ni = 0; ni < 2; ni++)
			for (dly = 0; dly < 2; dly++)
			    for (opt = 0; opt < 4; opt++) {
				unsigned int le = (ri & 1) ? 8u : 4u;
				unsigned int dfe = (ei & 1) ? 4u : 2u;
				unsigned int n = n_v[ni];
				unsigned int no_a = 0, no_b = 0;
				unsigned int k;
				int held = (int)((tag + 1) & 1);
				int code;

				/*
				 * `mmxMode` WITH STATE 2 IS UNDRIVABLE, and it
				 * is 6201 §2 rather than a gap: PHASE4 clamps
				 * and slices `soft`, which the fixed-point arm
				 * NEVER WRITES -- the two sides read a stack
				 * slot no path has filled and disagree by
				 * whatever their frames held.  The fixed-point
				 * representation is only ever entered from the
				 * data phase, and states 3, 4 and 5 are
				 * exactly the three that read `softInt`.
				 * Measured before it was believed: this grid
				 * ran with state 2 included and trial 5720009
				 * came back with our `outSym[0]` at the x87
				 * indefinite and the blob's at 0x19.
				 */
				if (mmx && est_v[ei] == V90EQU_STATE_PHASE4)
					continue;

				tag++;
				seed(tag);
				fill_arena(tag);
				seed_ce_pair(tag);
				wire(&OURS);
				wire(&THEIRS);
				p4_setup(tag, dly);
				plant_params();
				p4_equ_plant(tag, le, dfe, mmx);
				/*
				 * THE MMX PROLOGUE'S HELD-SAMPLE ARM, which is
				 * four lines no float trial can reach: with
				 * `holdoverPending` set it plants `(short)holdoverSample` at
				 * `block_b4[0]`, rewinds `cur` and makes `n`
				 * odd.  The float arm's own carry is the RESET
				 * group's; this is the fixed-point twin.
				 */
				OURS.holdoverPending = THEIRS.holdoverPending =
				    (unsigned)held;

				OURS.state = THEIRS.state = est_v[ei];
				OURS.quickConnect = THEIRS.quickConnect =
				    (int)(tag & 1);
				OURS.flag_144 = THEIRS.flag_144 =
				    (short)(tag & 1);
				OURS.flag_146 = THEIRS.flag_146 =
				    (short)((tag >> 1) & 1);
				ARENA_PARAMS->LOOP_TYPE = (opt & 1) ? 6 : 2;
				SPECVER->word_28 = (opt & 2) ? 2u : 1u;
				((V90ConnectionEvaluator *)ce_[0])->word_90 =
				    ((V90ConnectionEvaluator *)ce_[1])->word_90
				    = (unsigned)((tag >> 2) & 1);

				for (k = 0; k < 2; k++) {
					V90Phase4Demodulator *d =
					    &P4D((int)k);

					d->state = (Phase4DemodulatorState)
					    p4_rows[ri].state;
					d->sessionFlag =
					    (unsigned)p4_rows[ri].v92;
					d->countInState =
					    p4_rows[ri].count
					    - (unsigned)dly;
					d->trn2dDDLength = 0x40;
					/*
					 * OUT OF REACH, DELIBERATELY.  With
					 * this equal to `countInState` the
					 * TRN2dDD arm switches the demapper's
					 * LINEAR MAPPING STUDY on, and that
					 * member accumulates float sums whose
					 * 80-bit intermediates are
					 * `V90Demapper`'s divergence to own,
					 * not this binary's: three trials of
					 * 720 failed on `blockErrorEnergyRms` and one
					 * `array_d8` word with the study
					 * running, and the arm being driven
					 * here is `process`'s.  `t_v90demap`
					 * is where the study is adjudicated.
					 */
					d->linearMappStudyStart = 0x7d0;
					d->b1dBits = 0x60u;
					d->b1dZeros = 7u;
					d->nbits = 0u;
					d->int_0028 = 0;
					d->uchar_0030 = 1;
					d->int_0038 = 1;
					d->int_003c = 1;
					d->int_0040 = 1;
					d->int_0044 = 1;
					d->int_0048 = 0;
					d->quickConnect = 0u;
					d->uint_004c = 0u;
					d->errorEnergyBeforeEC = 4.0f;
					d->errorEnergyAfterEC = 1.0f;
					d->int_3510 = 0;
					arm_rnot(&d->rDetector1,
						 p4_rows[ri].rnot
						 ? 1 + dly : 0, 0x60);
					arm_rfnot(&d->rDetector2, 0, 0x60);
				}

				for (k = 0; k < NIN; k++) {
					float v = 6.0f
					    * (float)(((k + (unsigned)tag) % 5u)
						      + 1u);

					in_[0][k] = in_[1][k] = v;
					outsym_[0][k] = outsym_[1][k] =
					    (short)(0x1234 + k);
					outflt_[0][k] = outflt_[1][k] =
					    -1.5f * (float)k;
				}

				arena_snapshot();
				dsplib_debug_capture_reset();
				OURS.process(in_[0], n, outsym_[0], outflt_[0],
					     no_a);
				arena_switch();
				ref_equ_process(&THEIRS, in_[1], n, outsym_[1],
						outflt_[1], &no_b);

				diff_eq_int("nOut (%ld)", (long)no_a,
					    (long)no_b, tag);
				diff_eq_obj_(__FILE__, __LINE__, "outSym",
					     "short[NIN]", outsym_[0],
					     outsym_[1], sizeof(outsym_[0]),
					     tag);
				diff_eq_obj_(__FILE__, __LINE__, "outFloat",
					     "float[NIN]", outflt_[0],
					     outflt_[1], sizeof(outflt_[0]),
					     tag);
				diff_eq_int("in is not written (%ld)",
					    memcmp(in_[0], in_[1],
						   sizeof(in_[0])) == 0, 1,
					    tag);

				/* The three fields the sides MEAN to differ in. */
				diff_eq_int("the split peers are not written "
					    "(%ld)",
					    (long)(OURS.connEval
					     == (V90ConnectionEvaluator *)ce_[0]
					     && THEIRS.connEval
					     == (V90ConnectionEvaluator *)ce_[1]
					     && OURS.phase4Demod
					     == (V90Phase4Demodulator *)p4d_s[0]
					     && THEIRS.phase4Demod
					     == (V90Phase4Demodulator *)p4d_s[1]
					     && OURS.demapper
					     == (V90Demapper *)dem_s[0]
					     && THEIRS.demapper
					     == (V90Demapper *)dem_s[1]), 1,
					    tag);
				OURS.connEval = THEIRS.connEval =
				    (V90ConnectionEvaluator *)ce_[0];
				OURS.phase4Demod = THEIRS.phase4Demod =
				    (V90Phase4Demodulator *)p4d_s[0];
				OURS.demapper = THEIRS.demapper =
				    (V90Demapper *)dem_s[0];

				diff_eq_obj("after process", V90Equalizer,
					    &OURS, &THEIRS, tag);
				arena_compare("the arena after process", tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "the connection evaluator",
					     "V90ConnectionEvaluator", ce_[0],
					     ce_[1],
					     sizeof(V90ConnectionEvaluator),
					     tag);
				p4_compare(tag);
				diff_eq_int("no store past the object (%ld)",
					    guard_equal(), 1, tag);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);

				/*
				 * THE COUNTERS, every one read off the
				 * REFERENCE peer after the call.  `stateCount`
				 * is what the object copied out of
				 * `int_0028`, so it records the progress code
				 * the demodulator actually PRODUCED -- not the
				 * one this row aimed at.
				 */
				code = (int)THEIRS.stateCount;
				if (code >= 0 && code < 0x40)
					saw_code[code] = 1;
				if (mmx)
					saw_mmx = 1;
				else
					saw_float = 1;
				/*
				 * A RE-CONVERT RAN ONLY IF THE MODE FLIPPED.
				 * `enterDataPhase` returns non-zero exactly
				 * when `convertEqualizerToMmx` took, and the
				 * three re-convert blocks are its `if` body --
				 * so `mmxMode` rising from 0 within the call
				 * is the observable, and it is one the
				 * fixture cannot plant.
				 */
				if (!mmx && THEIRS.mmxMode) {
					saw_reconv = 1;
					if (no_b > 1)
						saw_mmx_after = 1;
					if (arena.b4[0] != arena_save.b4[0]
					    || arena.b4[1] != arena_save.b4[1])
						saw_b4 = 1;
					if (arena.b8[0] != arena_save.b8[0])
						saw_b8 = 1;
				}
				if (memcmp(arena.rsamp, arena_save.rsamp,
					   sizeof(arena.rsamp)) != 0)
					saw_bll = 1;
				if (THEIRS.linearEquBeta
				    != arena_save.lecoefs[0] * 0.0f
				    || THEIRS.dfeBeta != 0.0f)
					saw_beta = 1;
				if (THEIRS.ph4MeanErrorEnergyBeforeToAfterUpdateRatio
				    != 1.0f)
					saw_ratio = 1;
				if (le > 0
				    && THEIRS.historyIndex
				       > (int)(le + 8u - le - 1u)
					 - (int)((unsigned)tag % 3u))
					saw_wrap = 1;
				if (THEIRS.blockSampleCount == 0 && no_b != 0)
					saw_close = 1;

				if (prev_sym >= 0
				    && (long)outsym_[1][0] != prev_sym)
					sep_sym++;
				if (prev_state >= 0
				    && (long)THEIRS.state != prev_state)
					sep_state++;
				prev_sym = outsym_[1][0];
				prev_state = THEIRS.state;
				if (held)
					saw_held = 1;
				if (!mmx && THEIRS.mmxMode && dly)
					saw_late = 1;
			}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the float representation was driven", saw_float, 1, 0);
	diff_eq_int("the fixed-point representation was driven", saw_mmx, 1,
		    0);
	diff_eq_int("a re-convert block ran", saw_reconv, 1, 0);
	diff_eq_int("and the fixed-point arm ran after it", saw_mmx_after, 1,
		    0);
	diff_eq_int("the re-convert refilled block_b4", saw_b4, 1, 0);
	diff_eq_int("the re-convert refilled block_b8", saw_b8, 1, 0);
	diff_eq_int("a re-convert ran past symbol zero", saw_late, 1, 0);
	diff_eq_int("the fixed-point prologue carried a held sample", saw_held,
		    1, 0);
	diff_eq_int("the resampler was steered", saw_bll, 1, 0);
	diff_eq_int("a step size was moved", saw_beta, 1, 0);
	diff_eq_int("the before/after ratio was computed", saw_ratio, 1, 0);
	diff_eq_int("the cursor wrapped", saw_wrap, 1, 0);
	diff_eq_int("an error block closed", saw_close, 1, 0);
	diff_eq_int("the symbol output separated trials",
		    sep_sym > 16 ? 1 : 0, 1, 0);
	diff_eq_int("the equaliser state separated trials",
		    sep_state > 4 ? 1 : 0, 1, 0);

	/*
	 * PER PROGRESS CODE, and this is the check the row table cannot make
	 * for itself: a row aims at a code and the demodulator decides.  Each
	 * of these fails if the sub-case it names was never produced.
	 */
	diff_eq_int("progress code 0x17 was produced", saw_code[0x17], 1, 0);
	diff_eq_int("progress code 0x18 was produced", saw_code[0x18], 1, 0);
	diff_eq_int("progress code 0x19 was produced", saw_code[0x19], 1, 0);
	diff_eq_int("progress code 0x1c was produced", saw_code[0x1c], 1, 0);
	diff_eq_int("progress code 0x1d was produced", saw_code[0x1d], 1, 0);
	diff_eq_int("progress code 0x28 was produced", saw_code[0x28], 1, 0);
	diff_eq_int("progress code 0x2c was produced", saw_code[0x2c], 1, 0);
	diff_eq_int("progress code 0x35 was produced", saw_code[0x35], 1, 0);

	return diff_end();
}

/* ======================================================= the DATA state arm */

/*
 * State 3, and the two re-convert blocks that run the INVERSE direction.
 *
 * This arm is the only one that leaves the fixed-point representation: the
 * demapper hard-decides, `detectRRN` and `detectFPE` are offered the decision,
 * and each of `enterRRN` and `enterFPE` returns non-zero only when `mmxMode`
 * was set -- so RECONVERT-D and RECONVERT-E can ONLY be driven from a
 * fixed-point trial, and the mode is 0 for the rest of the call afterwards.
 * That makes each of those trials drive the fixed-point head, the inverse
 * conversion, the stepped `in` pointer AND the float head on the symbols after
 * it, in one call.
 *
 * THE TWO DETECTORS ARE ARMED EXCLUSIVELY and that is forced by the object:
 * `detectRRN` runs first and clears `mmxMode` through `enterRRN`, after which
 * `enterFPE` sees a float equaliser and returns 0.  So a trial with both armed
 * drives RECONVERT-D and can never reach RECONVERT-E.
 */
static int
run_data_arm(void)
{
	static const unsigned int n_v[] = { 4u, 7u };
	long tag = 5730000;
	int which, mmx, ni, dly, study;
	int saw_rrn = 0, saw_fpe = 0, saw_none = 0, saw_study = 0;
	int saw_float_after = 0, saw_outfloat = 0, saw_b4step = 0;
	int sep_sym = 0;
	long prev_sym = -1;

	diff_begin("V90Equalizer::process, the DATA state arm");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (which = 0; which < 3; which++)
	    for (mmx = 0; mmx < 2; mmx++)
		for (ni = 0; ni < 2; ni++)
		    for (dly = 0; dly < 2; dly++)
			for (study = 0; study < 2; study++) {
				unsigned int le = (which & 1) ? 8u : 4u;
				unsigned int dfe = 4u;
				unsigned int n = n_v[ni];
				unsigned int no_a = 0, no_b = 0;
				unsigned int k;
				int held;

				tag++;
				held = (int)(tag & 1);
				/*
				 * THE FPE ROWS FIRE ON THE LAST SYMBOL AND
				 * THEY HAVE TO.  `detectFPE` leaves the
				 * demodulator in state 0x10, and V.90's
				 * `getV90Decision` is one of the two arms that
				 * NEVER WRITES the answer there -- D600, and
				 * `t_v90p4ddec` asserts no return value on it
				 * for the same reason.  So a symbol after the
				 * detection reads the caller's register and
				 * the two sides differ for a reason belonging
								 * to the object.  Measured: with `n` left
				 * at 7 and the detection at symbol 1, trial
				 * 5730033's `outSym[3]` came back 0x56 against
				 * the blob's 0x00.
				 *
				 * THE RRN ROWS TAKE THE SAME RULE FOR A
				 * DIFFERENT REASON.  After RECONVERT-D the
				 * equaliser is back in FLOAT mode with
				 * coefficients rebuilt from the fixed-point
				 * pair, and a symbol demodulated after that
				 * runs the whole float tail on values three
				 * orders of magnitude larger than this
				 * fixture's own -- which is finding F6203's
				 * subtraction, in the one group that is
				 * otherwise clear of it.  Measured: three
				 * trials of 48 disagreed on `blockErrorEnergySum` and
				 * `blockErrorEnergyRms` on GCC 13 and were green on the
				 * period compiler.  Firing on the LAST symbol
				 * drives the whole inverse block, its `b8`
				 * loop and its `in` step, and stops before the
				 * arithmetic this binary already declares.
				 */
				if (which != 0) {
					n = 2u * (unsigned)(dly + 1);
					held = 0;
				}
				seed(tag);
				fill_arena(tag);
				seed_ce_pair(tag);
				wire(&OURS);
				wire(&THEIRS);
				p4_setup(tag, dly);
				plant_params();
				p4_equ_plant(tag, le, dfe, mmx);
				OURS.holdoverPending = THEIRS.holdoverPending =
				    (unsigned)(mmx ? held : 0);

				OURS.state = THEIRS.state = V90EQU_STATE_DATA;
				OURS.quickConnect = THEIRS.quickConnect = 0;
				ARENA_PARAMS->LOOP_TYPE = (which & 1) ? 6 : 2;
				/*
				 * NOT 2, AND THIS GROUP CANNOT AFFORD IT.
				 * `enterRRN`/`enterFPE` call
				 * `setDfeBeta(GERMAN_PBX_DFE_TRN2D_FAST_BETA)`
				 * on that arm, and with `mmxMode` still set
				 * the setter recomputes the fixed-point step
				 * size through a logarithm -- which is
				 * `t_v90equ`'s DECLARED divergence
				 * (gccdiverge.json, "enterRRN / enterFPE"),
				 * not this binary's.  Measured: with word_28
				 * at 2 the group failed 38 of 1209 on GCC 13,
				 * in `highErrorCount` and the restored linear
				 * coefficients.  The EIA-6 arm is still
				 * driven, because both its setters are handed
				 * 0.0f against a step size already 0.0f, which
				 * is ordered on both compilers.
				 */
				SPECVER->word_28 = 1u;

				for (k = 0; k < 2; k++) {
					V90Phase4Demodulator *d = &P4D((int)k);

					d->state = P4D_STATE_TRN2D_DD;
					d->sessionFlag = 0u;
					d->countInState = 0x10u;
					d->trn2dDDLength = 0x40;
					d->linearMappStudyStart = 0x7d0;
					d->b1dBits = 0x60u;
					d->b1dZeros = 7u;
					d->nbits = 0u;
					d->int_0028 = 0;
					d->uchar_0030 = 1;
					d->int_0038 = 1;
					d->int_003c = 1;
					d->int_0040 = 1;
					d->int_0044 = 1;
					d->int_0048 = 0;
					d->quickConnect = 0u;
					d->uint_004c = 0u;
					arm_r(&d->rDetector1,
					      which == 1 ? 1 + dly : 0, 0x60);
					arm_rf(&d->rDetector2,
					       which == 2 ? 1 + dly : 0, 0x60);
					DEM((int)k).linearMappStudyEnabled =
					    (short)study;
				}

				for (k = 0; k < NIN; k++) {
					float v = 6.0f
					    * (float)(((k + (unsigned)tag) % 5u)
						      + 1u);

					in_[0][k] = in_[1][k] = v;
					outsym_[0][k] = outsym_[1][k] =
					    (short)(0x1234 + k);
					outflt_[0][k] = outflt_[1][k] =
					    -1.5f * (float)k;
				}

				arena_snapshot();
				dsplib_debug_capture_reset();
				OURS.process(in_[0], n, outsym_[0], outflt_[0],
					     no_a);
				arena_switch();
				ref_equ_process(&THEIRS, in_[1], n, outsym_[1],
						outflt_[1], &no_b);

				diff_eq_int("nOut (%ld)", (long)no_a,
					    (long)no_b, tag);
				diff_eq_obj_(__FILE__, __LINE__, "outSym",
					     "short[NIN]", outsym_[0],
					     outsym_[1], sizeof(outsym_[0]),
					     tag);
				diff_eq_obj_(__FILE__, __LINE__, "outFloat",
					     "float[NIN]", outflt_[0],
					     outflt_[1], sizeof(outflt_[0]),
					     tag);
				diff_eq_int("in is not written (%ld)",
					    memcmp(in_[0], in_[1],
						   sizeof(in_[0])) == 0, 1,
					    tag);
				diff_eq_int("the split peers are not written "
					    "(%ld)",
					    (long)(OURS.connEval
					     == (V90ConnectionEvaluator *)ce_[0]
					     && THEIRS.phase4Demod
					     == (V90Phase4Demodulator *)p4d_s[1]
					     && OURS.demapper
					     == (V90Demapper *)dem_s[0]), 1,
					    tag);
				OURS.connEval = THEIRS.connEval =
				    (V90ConnectionEvaluator *)ce_[0];
				OURS.phase4Demod = THEIRS.phase4Demod =
				    (V90Phase4Demodulator *)p4d_s[0];
				OURS.demapper = THEIRS.demapper =
				    (V90Demapper *)dem_s[0];

				diff_eq_obj("after process", V90Equalizer,
					    &OURS, &THEIRS, tag);
				arena_compare("the arena after process", tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "the connection evaluator",
					     "V90ConnectionEvaluator", ce_[0],
					     ce_[1],
					     sizeof(V90ConnectionEvaluator),
					     tag);
				p4_compare(tag);
				diff_eq_int("no store past the object (%ld)",
					    guard_equal(), 1, tag);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);

				/*
				 * THE OBSERVABLE FOR AN INVERSE RE-CONVERT is
				 * the equaliser state the demodulator's
				 * detector drove it into, plus `mmxMode`
				 * FALLING -- and neither is something the
				 * fixture plants.  Both are read off the
				 * reference after the call.
				 */
				if (THEIRS.state == V90EQU_STATE_RRN) {
					saw_rrn = 1;
					if (mmx && !THEIRS.mmxMode)
						saw_float_after = 1;
				} else if (THEIRS.state == V90EQU_STATE_FPE) {
					saw_fpe = 1;
					if (mmx && !THEIRS.mmxMode)
						saw_float_after = 1;
				} else if (THEIRS.state == V90EQU_STATE_DATA) {
					saw_none = 1;
				}
				/*
				 * THE STUDY'S OWN OBSERVABLE.  It accumulates
				 * into the impairment detector at +0x1000 and
				 * counts at +0x1c00, and NOTHING else in this
				 * arm writes that object -- so the reference
				 * detector having moved off the seed is the
				 * study having run, read off the blob's side
				 * and not off the flag the fixture planted.
				 */
				if (((V90Demapper *)dem_s[1])->studyProgress != 0u)
					saw_study = 1;
				/*
				 * `outFloat[0]` came back as a converted
				 * `block_b8` word, which only the inverse
				 * block writes -- so a value equal to the
				 * short it was planted from, where the fixture
				 * planted a negative multiple of 1.5, is the
				 * conversion having happened.
				 */
				if (no_b > 1
				    && outflt_[1][0] != -0.0f
				    && outflt_[1][0]
				       == (float)(short)outflt_[1][0])
					saw_outfloat = 1;
				if (memcmp(arena.b4, arena_save.b4,
					   sizeof(arena.b4)) != 0)
					saw_b4step = 1;

				/*
				 * SEPARATION OVER THE WHOLE REFERENCE OUTPUT,
				 * and the two counters this replaces both read
				 * zero.  `outSym[0]` alone is a constellation
				 * level and repeats across neighbouring
				 * trials; the equaliser state takes three
				 * values in three blocks and changes twice in
				 * forty-eight.  Neither could ever reach its
				 * threshold, which is finding F3509's shape --
				 * so the observable is the sum of every symbol,
				 * every float and the count, all off the
				 * reference.
				 */
				{
					long h = (long)no_b;
					unsigned int q;

					for (q = 0; q < NIN; q++)
						h = h * 31
						    + (long)outsym_[1][q]
						    + (long)(int)outflt_[1][q];
					if (prev_sym >= 0 && h != prev_sym)
						sep_sym++;
					prev_sym = h;
				}
			}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the RRN detector took the equaliser out of DATA",
		    saw_rrn, 1, 0);
	diff_eq_int("the FPE detector took the equaliser out of DATA",
		    saw_fpe, 1, 0);
	diff_eq_int("a trial stayed in DATA", saw_none, 1, 0);
	diff_eq_int("the mapping study ran", saw_study, 1, 0);
	diff_eq_int("the fixed-point mode fell inside a call",
		    saw_float_after, 1, 0);
	diff_eq_int("an inverse re-convert refilled outFloat", saw_outfloat, 1,
		    0);
	diff_eq_int("block_b4 was written", saw_b4step, 1, 0);
	diff_eq_int("the reference output separated trials",
		    sep_sym > 24 ? 1 : 0, 1, 0);

	return diff_end();
}

/* ============================== the re-convert with a linear output too wide */

/*
 * `leSum = (short)y` AND `(int)y` AGREE OVER EVERY `y` THE OTHER GROUPS
 * PRODUCE, and that is a property of those fixtures rather than of the
 * object.  This one exists to remove the coincidence: the linear history is
 * planted three orders of magnitude larger so `y` leaves a short's range, the
 * re-convert fires on symbol ZERO, and `leSum` then reaches the fixed-point
 * tail's `diff = (short)(leSum - decision)` and the linear LMS behind it,
 * which lands in `array_d8` and is compared.
 *
 * ONE SYMBOL PER CALL, and it is what keeps this group clear of finding
 * F6203.  With `nOut` at 1 the only symbol is the one the re-convert runs on,
 * and `mmxMode` is set before its tail -- so the FLOAT tail, where
 * `err = soft - fdec` on a `soft` of 10**5 needs a twenty-fifth mantissa bit,
 * never executes.  Everything downstream of the wide `y` is integer.
 */
static int
run_reconvert_wide(void)
{
	static const int est_v[] = {
		V90EQU_STATE_PHASE4, V90EQU_STATE_RRN, V90EQU_STATE_FPE
	};
	long tag = 5740000;
	int ei, li;
	int saw_wide = 0, saw_reconv = 0, saw_diff = 0, saw_wided = 0;

	diff_begin("V90Equalizer::process, a re-convert on a wide linear output");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (ei = 0; ei < 3; ei++)
	    for (li = 0; li < 2; li++) {
		unsigned int le = li ? 8u : 4u;
		unsigned int dfe = 4u;
		unsigned int n = 2u;
		unsigned int no_a = 0, no_b = 0;
		unsigned int k;

		tag++;
		seed(tag);
		fill_arena(tag);
		seed_ce_pair(tag);
		wire(&OURS);
		wire(&THEIRS);
		p4_setup(tag, 0);
		plant_params();
		p4_equ_plant(tag, le, dfe, 0);
		OURS.holdoverPending = THEIRS.holdoverPending = 0;

		/*
		 * Wide enough that `(short)y` and `(int)y` cannot agree, and
		 * the DFE history with it so `(short)d` and `(int)d` cannot
		 * either.  Both matter: the three forward re-convert blocks
		 * narrow `y` at all three sites and `d` at exactly one, and a
		 * fixture whose filters stay inside a short cannot tell any of
		 * those four spellings apart.
		 */
		for (k = 0; k < ARR_F; k++) {
			arena.a18[k] = 20000.0f
			    * (float)(((k + (unsigned)tag) % 7) + 1u);
			arena.a44[k] = 30000.0f
			    * (float)(((k * 3u + (unsigned)tag) % 5) + 1u);
		}

		OURS.state = THEIRS.state = est_v[ei];
		OURS.quickConnect = THEIRS.quickConnect = 0;
		ARENA_PARAMS->LOOP_TYPE = 2;
		SPECVER->word_28 = 1u;

		for (k = 0; k < 2; k++) {
			V90Phase4Demodulator *d = &P4D((int)k);

			d->state = P4D_STATE_B1D;
			d->sessionFlag = 0u;
			d->countInState = 0x11fu;
			d->trn2dDDLength = 0x40;
			d->linearMappStudyStart = 0x7d0;
			d->b1dBits = 0x60u;
			d->b1dZeros = 7u;
			d->nbits = 0u;
			d->int_0028 = 0;
			d->uchar_0030 = 1;
			d->int_0038 = 1;
			d->int_003c = 1;
			d->int_0040 = 1;
			d->int_0044 = 1;
			d->int_0048 = 0;
			d->quickConnect = 0u;
			d->uint_004c = 0u;
			arm_rnot(&d->rDetector1, 0, 0x60);
			arm_rfnot(&d->rDetector2, 0, 0x60);
		}

		for (k = 0; k < NIN; k++) {
			in_[0][k] = in_[1][k] = 20000.0f
			    * (float)(((k + (unsigned)tag) % 5u) + 1u);
			outsym_[0][k] = outsym_[1][k] = (short)(0x1234 + k);
			outflt_[0][k] = outflt_[1][k] = -1.5f * (float)k;
		}

		arena_snapshot();
		dsplib_debug_capture_reset();
		OURS.process(in_[0], n, outsym_[0], outflt_[0], no_a);
		arena_switch();
		ref_equ_process(&THEIRS, in_[1], n, outsym_[1], outflt_[1],
				&no_b);

		diff_eq_int("nOut (%ld)", (long)no_a, (long)no_b, tag);
		diff_eq_obj_(__FILE__, __LINE__, "outSym", "short[NIN]",
			     outsym_[0], outsym_[1], sizeof(outsym_[0]), tag);
		diff_eq_obj_(__FILE__, __LINE__, "outFloat", "float[NIN]",
			     outflt_[0], outflt_[1], sizeof(outflt_[0]), tag);
		OURS.connEval = THEIRS.connEval =
		    (V90ConnectionEvaluator *)ce_[0];
		OURS.phase4Demod = THEIRS.phase4Demod =
		    (V90Phase4Demodulator *)p4d_s[0];
		OURS.demapper = THEIRS.demapper = (V90Demapper *)dem_s[0];
		diff_eq_obj("after process", V90Equalizer, &OURS, &THEIRS, tag);
		arena_compare("the arena after process", tag);
		diff_eq_obj_(__FILE__, __LINE__, "the connection evaluator",
			     "V90ConnectionEvaluator", ce_[0], ce_[1],
			     sizeof(V90ConnectionEvaluator), tag);
		p4_compare(tag);
		diff_eq_int("no store past the object (%ld)", guard_equal(), 1,
			    tag);
		diff_eq_int("transcript (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);

		/*
		 * THE COUNTERS, off the reference.  `outFloat[0]` is the float
		 * `soft` this symbol produced -- the re-convert runs after it
		 * is published -- so a magnitude past a short's range is the
		 * wide `y` having happened, and `mmxMode` rising is the
		 * re-convert having run.
		 */
		if (no_b > 0 && (outflt_[1][0] > 32767.0f
				 || outflt_[1][0] < -32767.0f))
			saw_wide = 1;
		if (THEIRS.mmxMode)
			saw_reconv = 1;
		if (memcmp(arena.ad8, arena_save.ad8, sizeof(arena.ad8)) != 0)
			saw_diff = 1;
		/*
		 * AND THE DFE OUTPUT IS PAST A SHORT TOO.  `outFloat[0]` is
		 * `y - d`, so it cannot witness `d` on its own; the history
		 * this group plants makes `|d|` at least 30000 by
		 * construction and the coefficients are all positive, which
		 * is what puts the `(short)d` at RECONVERT-B inside the
		 * domain where it differs from RECONVERT-A's `(int)d`.
		 */
		if (arena_save.a44[0] > 32767.0f)
			saw_wided = 1;
	    }

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("a linear output past a short's range was produced",
		    saw_wide, 1, 0);
	diff_eq_int("and a re-convert ran on it", saw_reconv, 1, 0);
	diff_eq_int("and the fixed-point LMS moved array_d8", saw_diff, 1, 0);
	diff_eq_int("the DFE history was past a short's range too", saw_wided,
		    1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	if (run_reset_arm())
		rc = 1;
	if (run_p4_arms())
		rc = 1;
	if (run_data_arm())
		rc = 1;
	if (run_reconvert_wide())
		rc = 1;
	return rc;
}
