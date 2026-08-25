/*
 * t_v92modstate.cpp -- differential test of the ten V92Modulator members that
 * drive the transmit graph: `reset`, `enterPhase3`, `enterDataPhase`, the four
 * phase 3 `exit*` members, `exitCPt`, `getV92TxFilterDelay` and
 * `mkResampledSignal`.
 *
 * SEPARATE FROM t_v92mod.cpp, which drives the constructor and the destructor.
 * That file's subject is an ELEVEN-PIECE GRAPH being built and taken apart, and
 * its whole apparatus -- allocation counting, 2,048 null subsets, sub-object
 * arguments -- exists to see through pointers that can never agree.  These ten
 * take a graph that is already built and change its STATE, so what has to be
 * compared is different: the modulator's own bytes, and then the bytes of every
 * sub-object each member reaches through.
 *
 * ---------------------------------------------------------------------------
 * WHAT AGREES BECAUSE IT IS SHARED, AND WHAT HAS TO BE POISONED
 *
 * The seven constructor arguments are ONE instance each, pointed at by both
 * sides, so `phase2Info`, `ja`, `dil`, `cp`, `mappingParams` and `params`
 * compare as addresses and a member that read the wrong one fails.  The eleven
 * OWNED pieces are two allocations each and can never agree; those runs
 * (+0x44..+0x6f and +0x74..+0x8f) are poisoned in the object comparison and
 * every one of them is covered by comparing the piece itself instead.
 *
 * Two of the sub-objects carry pointers of their own and are poisoned the same
 * way t_v92mod.cpp poisons them: the phase 3 modulator's scrambler at its
 * +0x18, and the phase 4 modulator's scrambler at +0x4c plus its
 * `bitsToSymbol` and `mapper` at +0x6c.  The phase 3 modulator's `jaBits` at
 * +0x3c is `ja + 4` over the SHARED `ja`, so it compares.
 *
 * ---------------------------------------------------------------------------
 * `resamplerPhaseOffset` AT +0x24 IS NEVER WRITTEN BY THE CLASS
 *
 * Eighteen symbols, three reads, no store -- V92Modulator.h and D800.  It is
 * seeded identically on both sides and never zeroed, so it agrees BECAUSE
 * nobody wrote it, and a reconstruction that helpfully cleared it fails.  The
 * `OFFSET` arm of `mkResampledSignal` is the reader, and this file is what
 * drives it.
 *
 * ---------------------------------------------------------------------------
 * FIVE THINGS THE FIXTURE HAS TO CONSTRAIN, AND NONE OF THEM IS TIDINESS
 *
 *   1. `MODULATOR_QUEUE_LENGTH` IS SMALL AND POSITIVE.  `reset` shifts it
 *      right arithmetically and uses the result as the UNSIGNED bound of the
 *      priming loop, so a negative parameter asks for about two billion
 *      iterations (finding F1284).  t_v92mod.cpp's fixture says the same.
 *   2. THE PHASE 4 MODULATOR'S `word_1b0` IS NON-ZERO.  `exitCPt` reaches
 *      `V92Phase4Modulator::exitCPt`, whose guard is
 *      `(symbolCount - 24) % word_1b0`, and that division is unguarded --
 *      D700.  Zero raises #DE identically on both sides, which measures the
 *      CPU rather than the reading (D571's argument).
 *   3. `resamplerPhaseChangeAt` IS NEVER PAST `blockRemaining`.  The object
 *      computes `blockRemaining - resamplerPhaseChangeAt` as an UNSIGNED
 *      subtraction with no test, so a larger split point asks the resampler
 *      for about four billion samples -- D801.  Reproduced in the source and
 *      kept out of the grid for the same reason as 2.
 *   4. `blockRemaining` IS NEVER PAST `blockSize`.  `resampleIn` holds
 *      `blockSize + 10` floats and the resampler reads all of the count it is
 *      given; more would read off the end on both sides at once.
 *   5. EVERY FLOAT THE RESAMPLER SEES IS SMALL AND FINITE.  The comparison is
 *      bit-exact on both the output samples and the resampler's own `phase`,
 *      and a NaN compares unequal to itself on both sides.
 *
 * ---------------------------------------------------------------------------
 * THE DIAGNOSTICS ARE DRIVEN AT FOUR LEVELS, NOT JUST RAISED
 *
 * Finding F150's rule.  Nine of the ten members gate on `dsplibs_debug_level >
 * 1`, so 0 and 1 must print NOTHING from them and 2 and 3 must print
 * everything; a site whose gate was dropped, or written `> 2`, is identical to
 * the object at one level and differs at another.  Line counts are asserted
 * against literals as well as against the blob's, because two silent sides
 * agree about nothing (finding F149).
 *
 * THE STARTING PHASE IS SET THROUGH OUR OWN `setNormalizedPhase` ON BOTH
 * SIDES.  It is a fixture operation and not the subject: the two resamplers
 * have the same `phases`, so one call each writes the same double into the same
 * offset, and what is compared afterwards is what `mkResampledSignal` did to
 * it.  Poking `phase` directly would need this file to know the scaling, which
 * is Resampler's business and already tested in t_resampler.cpp.
 *
 * `enterDataPhase` IS THE EXCEPTION AND IT IS NOT AN EXCEPTION IN THE LINE
 * COUNT.  Its message goes through `edprintf`, and the CALLER has no gate --
 * there is no `dsplibs_debug_level` test anywhere in its 69 bytes -- so the
 * formatting and the encoding run at every level and move `edprintf`'s shared
 * key.  Only the final `dsplibs_debug_printf` inside `edprintf` is gated, at
 * the same `> 1`, so the TRANSCRIPT still shows nothing below level 2.  What
 * the ungated call costs is the key, not a line, and that is why the count
 * below is the same shape as the other nine.
 *
 * The two sides' encoded lines compare because `edprintf` sets
 * `iEncodeOffset = 0` at the head of every call (src/core/encode.c), so one
 * call each of one identical message gives one identical encoded line however
 * many calls preceded it.  t_v90p2info.cpp's argument for the same site.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase3Modulator.h"
#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/V92CP.h"
#include "dsplib/V92ModulusEncoder.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/V92Phase2Info.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/Queue.h"
#include "dsplib/FloatFIR.h"
#include "dsplib/Resampler.h"
#include "dsplib/ResamplerTimingOffset.h"

extern "C" {
void our_c1(void *, unsigned, void *, void *, void *, void *, void *, void *)
	asm("_ZN12V92ModulatorC1EjP13V92Phase2InfoP5V92JaP19tagV90DILdescriptor"
	    "P5V92CPP16V92MappingParamsP13V92Parameters");
void ref_c1(void *, unsigned, void *, void *, void *, void *, void *, void *)
	asm("ref__ZN12V92ModulatorC1EjP13V92Phase2InfoP5V92JaP19tagV90DILdescri"
	    "ptorP5V92CPP16V92MappingParamsP13V92Parameters");
void our_d1(void *) asm("_ZN12V92ModulatorD1Ev");
void ref_d1(void *) asm("ref__ZN12V92ModulatorD1Ev");

void our_reset(void *) asm("_ZN12V92Modulator5resetEv");
void ref_reset(void *) asm("ref__ZN12V92Modulator5resetEv");
void our_enterPhase3(void *) asm("_ZN12V92Modulator11enterPhase3Ev");
void ref_enterPhase3(void *) asm("ref__ZN12V92Modulator11enterPhase3Ev");
void our_enterPhase4(void *) asm("_ZN12V92Modulator11enterPhase4Ev");
void ref_enterPhase4(void *) asm("ref__ZN12V92Modulator11enterPhase4Ev");
void our_enterDataPhase(void *) asm("_ZN12V92Modulator14enterDataPhaseEv");
void ref_enterDataPhase(void *) asm("ref__ZN12V92Modulator14enterDataPhaseEv");
int our_initiateRRN(void *) asm("_ZN12V92Modulator11initiateRRNEv");
int ref_initiateRRN(void *) asm("ref__ZN12V92Modulator11initiateRRNEv");
int our_initiateFPE(void *) asm("_ZN12V92Modulator11initiateFPEEv");
int ref_initiateFPE(void *) asm("ref__ZN12V92Modulator11initiateFPEEv");
void our_progress(void *, int *, unsigned int *, float *, unsigned int)
	asm("_ZN12V92Modulator8progressEPiRjPfj");
void ref_progress(void *, int *, unsigned int *, float *, unsigned int)
	asm("ref__ZN12V92Modulator8progressEPiRjPfj");
void our_exitJa(void *) asm("_ZN12V92Modulator6exitJaEv");
void ref_exitJa(void *) asm("ref__ZN12V92Modulator6exitJaEv");
void our_exitSilence(void *) asm("_ZN12V92Modulator11exitSilenceEv");
void ref_exitSilence(void *) asm("ref__ZN12V92Modulator11exitSilenceEv");
void our_exitSuSecond(void *) asm("_ZN12V92Modulator12exitSuSecondEv");
void ref_exitSuSecond(void *) asm("ref__ZN12V92Modulator12exitSuSecondEv");
void our_exitTRN1uSecond(void *) asm("_ZN12V92Modulator15exitTRN1uSecondEv");
void ref_exitTRN1uSecond(void *)
	asm("ref__ZN12V92Modulator15exitTRN1uSecondEv");
void our_exitCPt(void *) asm("_ZN12V92Modulator7exitCPtEv");
void ref_exitCPt(void *) asm("ref__ZN12V92Modulator7exitCPtEv");
int our_delay(void *) asm("_ZNK12V92Modulator19getV92TxFilterDelayEv");
int ref_delay(void *) asm("ref__ZNK12V92Modulator19getV92TxFilterDelayEv");
void our_mkres(void *, unsigned int *)
	asm("_ZN12V92Modulator17mkResampledSignalERj");
void ref_mkres(void *, unsigned int *)
	asm("ref__ZN12V92Modulator17mkResampledSignalERj");

extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

typedef void (*member_fn)(void *);

#define GUARD	64u
#define OBJSZ	((unsigned)sizeof(V92Modulator))
#define SLOT	(OBJSZ + GUARD)
#define CPSZ	((unsigned)sizeof(V92CP))
#define PARAMSZ	((unsigned)sizeof(V92Parameters))
#define P3MSZ	((unsigned)sizeof(V92Phase3Modulator))
#define P4MSZ	((unsigned)sizeof(V92Phase4Modulator))
#define BTSSZ	((unsigned)sizeof(V92BitsToSymbol))
#define RESSZ	0x4cu

/* The constructor's first argument.  Big enough that a block is worth
 * resampling and small enough that a wrong multiplier asks for a wrong
 * allocation rather than an impossible one. */
#define NSAMPLES	120u

/* `word_1b0`, kept off zero for the reason in the file comment. */
#define P4M_PERIOD	12u

static unsigned char ours[SLOT] __attribute__((aligned(8)));
static unsigned char theirs[SLOT] __attribute__((aligned(8)));
static unsigned char sown[SLOT];
static unsigned char cmp_a[SLOT], cmp_b[SLOT];

static unsigned char arg_params[PARAMSZ] __attribute__((aligned(8)));
static unsigned char arg_p2[CPSZ] __attribute__((aligned(8)));
static unsigned char arg_ja[CPSZ] __attribute__((aligned(8)));
static unsigned char arg_dil[CPSZ] __attribute__((aligned(8)));
static unsigned char arg_mp[CPSZ] __attribute__((aligned(8)));
static unsigned char arg_cp[CPSZ] __attribute__((aligned(8)));

/* Scratch for the sub-object comparisons. */
static unsigned char sub_a[P4MSZ], sub_b[P4MSZ];

/*
 * The two runs of owned pointers in the modulator's own bytes.  `tailLength`
 * at +0x70 is a COUNT and is deliberately left in.
 */
struct region { unsigned off, len; };
static const struct region skip[] = {
	{ 0x44, 0x2c },
	{ 0x74, 0x1c },
};
#define NSKIP	((unsigned)(sizeof(skip) / sizeof(skip[0])))

/*
 * TWO PARAMETERS `run_progress` HAS TO CONTROL AND THE OTHER RUNNERS MUST NOT
 * SEE.  Zero and -1 mean "as before", so every existing trial is unchanged.
 *
 * The queue length is the sharp one.  `progress` writes a whole resampled block
 * into the queue and reads a whole one back, so the queue has to hold
 * `queuePrime + nOut` at its peak; the 6..30 the other runners use is a length
 * `reset`'s priming loop is happy with and `progress` overflows on its first
 * call, and an overflowed `write` returns -1 having stored nothing.  That is
 * not a bug in either side -- both fail identically -- but it makes every trial
 * measure the same refusal.
 */
static int queue_len_override;
static int filter_override = -1;
/*
 * WHEN THE MAPPING PARAMETERS HAVE TO BE REAL.  Everywhere else in this file
 * they are 2,328 random bytes standing in for a pointer nothing dereferences.
 * `progress` DOES dereference them -- its phase 4 arm reaches
 * `V92Precoder::process`, whose `paramsAt9c` is `params->indexConstel`, an
 * array INSIDE the block -- so that arm needs a `struct V92ParamsInfo` with
 * the two creators run over it.
 */
static int mp_real;

static unsigned int lfsr;

static unsigned int
lfsr_step(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return lfsr;
}

static V92Modulator *
M(int s)
{
	return (V92Modulator *)(s == 0 ? ours : theirs);
}

/*
 * The parameter block: every slot a small positive int, so nothing read out of
 * it is a wild count or a wild divisor.  `MODULATOR_QUEUE_LENGTH` is set
 * explicitly for the reason in the file comment.
 */
static void
seed_params(int trial)
{
	V92Parameters *pp;
	unsigned i;

	for (i = 0; i < PARAMSZ / 4; i++) {
		int v = (int)(4u + ((i * 7u + (unsigned)trial) & 0x3fu));

		memcpy(arg_params + i * 4, &v, 4);
	}
	pp = (V92Parameters *)arg_params;
	pp->MODULATOR_QUEUE_LENGTH = queue_len_override != 0
				   ? queue_len_override
				   : 6 + 2 * (trial % 13);
	pp->V92_APPLY_TX_SHAPING_FILTER = filter_override >= 0
					? filter_override
					: (trial & 1);
}

/*
 * Build both sides.  The seven arguments are shared instances; `phase2Info`
 * and the four opaque blocks are seeded once per trial and not touched again,
 * so a member that reads the wrong one reads the same bytes on both sides and
 * the ADDRESS in the object is what catches it.
 */
static void
build(int trial)
{
	unsigned i;

	lfsr = 0x1234u + 0x9e37u * (unsigned)trial;

	for (i = 0; i < SLOT; i++) {
		unsigned char v = (unsigned char)((lfsr_step() >> 3) | 1u);

		ours[i] = v;
		theirs[i] = v;
		sown[i] = v;
	}
	for (i = 0; i < CPSZ; i++) {
		unsigned char mp = (unsigned char)(lfsr_step() >> 3);

		arg_p2[i] = (unsigned char)(lfsr_step() >> 3);
		arg_ja[i] = (unsigned char)(lfsr_step() >> 3);
		arg_dil[i] = (unsigned char)(lfsr_step() >> 3);
		if (!mp_real)
			arg_mp[i] = mp;
		arg_cp[i] = (unsigned char)((lfsr_step() >> 3) | 1u);
	}

	/*
	 * `ja` is read by V92Phase3Modulator::reset as a count followed by a
	 * vector, so its first word has to be a length the vector can hold --
	 * otherwise `enterPhase3` walks off the end on both sides at once.
	 */
	{
		unsigned int n = 8;

		memcpy(arg_ja, &n, sizeof(n));
	}
	/* `phase2Info->rtd` is `enterPhase3`'s and `enterPhase4`'s last
	 * argument; keep it small so nothing downstream loops on a wild
	 * count. */
	{
		int rtd = 5 + trial % 7;

		memcpy(arg_p2 + 4, &rtd, sizeof(rtd));
	}

	seed_params(trial);
	harness_alloc_reset();
	our_c1(ours, NSAMPLES, arg_p2, arg_ja, arg_dil, arg_cp, arg_mp,
	       arg_params);
	ref_c1(theirs, NSAMPLES, arg_p2, arg_ja, arg_dil, arg_cp, arg_mp,
	       arg_params);

	/* D700's divisor, on both sides, before anything reaches exitCPt. */
	M(0)->phase4Modulator->word_1b0 = P4M_PERIOD;
	M(1)->phase4Modulator->word_1b0 = P4M_PERIOD;
}

static void
teardown(void)
{
	our_d1(ours);
	ref_d1(theirs);
}

/*
 * `V92Phase4Modulator::reset` runs `cp->infoToBits()` and `cp->getBitVector()`
 * over whatever the CP holds, and both are undefined outside the bounds D570,
 * D571 and t_v92info record.  t_v92p4sym's grid carries the same six lines for
 * the same reason: outside them the two sides fault together, which measures
 * the CPU rather than the reading (D571's argument).  Every member that
 * reaches `V92Phase4Modulator::reset` -- `enterPhase4`, `initiateRRN`,
 * `initiateFPE` and `progress` -- calls this first.
 */
static void
sane_cp(int trial)
{
	V92CP *c = (V92CP *)(void *)arg_cp;
	unsigned mix = (unsigned)trial;

	c->bitsPerSymbol = (unsigned char)(1 + (mix % 6));
	c->word_10c = (unsigned short)(mix % 7);
	c->char_01 = (signed char)((mix % 5) - 1);
	c->char_02 = (signed char)((mix % 9) - 4);
	c->byte_00 = (unsigned char)(mix % 3);
	c->byte_04 = (unsigned char)(1 + (mix & 1));
	c->byte_24 = (unsigned char)((mix >> 1) & 1);
	c->flt_10 = 0.25f;
	c->flt_14 = -0.5f;
	c->flt_18 = 0.75f;
	c->flt_1c = -0.125f;
	c->flt_20 = 0.5f;
}

static int
guard_intact(void)
{
	return memcmp(ours + OBJSZ, sown + OBJSZ, GUARD) == 0
	    && memcmp(theirs + OBJSZ, sown + OBJSZ, GUARD) == 0;
}

/* The modulator's own bytes, with the two owned-pointer runs poisoned. */
static void
compare_obj(const char *what, long trial)
{
	unsigned i;

	memcpy(cmp_a, ours, OBJSZ);
	memcpy(cmp_b, theirs, OBJSZ);
	for (i = 0; i < NSKIP; i++) {
		memset(cmp_a + skip[i].off, 0x77, skip[i].len);
		memset(cmp_b + skip[i].off, 0x77, skip[i].len);
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Modulator", cmp_a, cmp_b,
		     (size_t)OBJSZ, trial);
}

/*
 * The phase 3 modulator, 0x50 bytes.  Only its scrambler's seven pointers
 * cannot agree; `jaBits` at +0x3c is `ja + 4` over the shared `ja` and
 * `params` at +0x4c is the shared block, so both compare.
 */
static void
compare_p3m(const char *what, long trial)
{
	memcpy(sub_a, M(0)->phase3Modulator, P3MSZ);
	memcpy(sub_b, M(1)->phase3Modulator, P3MSZ);
	memset(sub_a + 0x18, 0x77, 7 * sizeof(void *));
	memset(sub_b + 0x18, 0x77, 7 * sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Phase3Modulator", sub_a,
		     sub_b, (size_t)P3MSZ, trial);
}

/*
 * The phase 4 modulator, 0x1cc bytes.  Its scrambler's seven pointers at
 * +0x4c and its `bitsToSymbol`/`mapper` pair at +0x6c are the two runs that
 * cannot agree; `pattern` at +0x1a8 points into the SHARED V92CP and so
 * compares, which is what makes it a witness rather than noise.
 */
static void
compare_p4m(const char *what, long trial)
{
	memcpy(sub_a, M(0)->phase4Modulator, P4MSZ);
	memcpy(sub_b, M(1)->phase4Modulator, P4MSZ);
	memset(sub_a + 0x4c, 0x77, 7 * sizeof(void *));
	memset(sub_b + 0x4c, 0x77, 7 * sizeof(void *));
	memset(sub_a + 0x6c, 0x77, 2 * sizeof(void *));
	memset(sub_b + 0x6c, 0x77, 2 * sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Phase4Modulator", sub_a,
		     sub_b, (size_t)P4MSZ, trial);
}

/*
 * The bit-to-symbol stage, 0x20 bytes.  Its `transmitter` at +0x00, `params`
 * at +0x04 and `symbols` at +0x08 are two heap addresses and one shared one;
 * everything from +0x0c on is counts, and `symbolsBlockSize` at +0x18 is what
 * `enterDataPhase` writes.
 */
static void
compare_bts(const char *what, long trial)
{
	memcpy(sub_a, M(0)->bitsToSymbol, BTSSZ);
	memcpy(sub_b, M(1)->bitsToSymbol, BTSSZ);
	memset(sub_a, 0x77, 3 * sizeof(void *));
	memset(sub_b, 0x77, 3 * sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, what, "V92BitsToSymbol", sub_a, sub_b,
		     (size_t)BTSSZ, trial);
}

/*
 * The resampler, 0x4c bytes: the vptr and its two buffers are the only things
 * that cannot agree.  `phase` at +0x0c is a DOUBLE and is what
 * `mkResampledSignal`'s phase change moves, so it is the one field this whole
 * file exists to compare.
 */
static void
compare_resampler(const char *what, long trial)
{
	memcpy(sub_a, M(0)->resampler, RESSZ);
	memcpy(sub_b, M(1)->resampler, RESSZ);
	memset(sub_a, 0x77, 12);
	memset(sub_b, 0x77, 12);
	diff_eq_obj_(__FILE__, __LINE__, what, "ResamplerTimingOffset", sub_a,
		     sub_b, (size_t)RESSZ, trial);
}

/* The queue's occupancy, which is invisible in the modulator's bytes. */
static void
compare_queue(long trial)
{
	const unsigned char *qa = (const unsigned char *)M(0)->queue;
	const unsigned char *qb = (const unsigned char *)M(1)->queue;
	const unsigned char *ba, *bb, *ra, *rb, *wa, *wb;

	memcpy(&ba, qa + 0x00, sizeof(ba));
	memcpy(&bb, qb + 0x00, sizeof(bb));
	memcpy(&ra, qa + 0x08, sizeof(ra));
	memcpy(&rb, qb + 0x08, sizeof(rb));
	memcpy(&wa, qa + 0x0c, sizeof(wa));
	memcpy(&wb, qb + 0x0c, sizeof(wb));

	diff_eq_int("the queue's write cursor agrees (trial %ld)",
		    (int)(wa - ba), (int)(wb - bb), trial);
	diff_eq_int("the queue's read cursor agrees (trial %ld)",
		    (int)(ra - ba), (int)(rb - bb), trial);
	diff_eq_int("the queue holds the same floats (trial %ld)",
		    memcmp(ba, bb, (size_t)(wa - ba)) == 0, 1, trial);
}

/*
 * The scrambler: its history, at two addresses and holding the same bytes, AND
 * its three running cursors.
 *
 * THE CURSORS NEED THEIR OWN CHECK, and finding that out cost a mutation.
 * `pOut`, `pTap1` and `pTap2` live at +0x64..+0x6f, which is inside the
 * +0x44..+0x6f run the object comparison has to poison because the eleven
 * owned pointers are in it -- so "reset does not put the scrambler back" ran
 * green until they were compared as DISTANCES from `pLimit`, which are not
 * addresses and do compare.  t_v92mod.cpp's `scram_geometry` is the same
 * device for the three constructor arguments.
 */
static void
scram_cursor(const char *what, unsigned off, long trial)
{
	const unsigned char *base_a, *base_b, *pa, *pb;

	memcpy(&base_a, ours + 0x54, sizeof(base_a));
	memcpy(&base_b, theirs + 0x54, sizeof(base_b));
	memcpy(&pa, ours + off, sizeof(pa));
	memcpy(&pb, theirs + off, sizeof(pb));
	diff_eq_int(what, (int)(pa - base_a), (int)(pb - base_b), trial);
}

static void
compare_scrambler(long trial)
{
	const unsigned char *a, *b;
	unsigned n = (1u + V92MOD_SCRAM_TAP2 + V92MOD_SCRAM_SLACK)
		     * (unsigned)sizeof(int);

	memcpy(&a, ours + 0x54, sizeof(a));
	memcpy(&b, theirs + 0x54, sizeof(b));
	diff_eq_int("the scrambler's history agrees (trial %ld)",
		    memcmp(a, b, n) == 0, 1, trial);
	scram_cursor("the scrambler's output cursor agrees (trial %ld)", 0x64,
		     trial);
	scram_cursor("the scrambler's near tap agrees (trial %ld)", 0x68,
		     trial);
	scram_cursor("the scrambler's far tap agrees (trial %ld)", 0x6c,
		     trial);
}

/* The transmit filter's history and counts. */
static void
compare_filter(long trial)
{
	const unsigned char *fa = (const unsigned char *)M(0)->txFilter;
	const unsigned char *fb = (const unsigned char *)M(1)->txFilter;
	const float *ha, *hb;
	unsigned int na, nb;

	memcpy(&ha, fa + 0x04, sizeof(ha));
	memcpy(&hb, fb + 0x04, sizeof(hb));
	memcpy(&na, fa + 0x0c, sizeof(na));
	memcpy(&nb, fb + 0x0c, sizeof(nb));

	diff_eq_int("the filter's buffer length agrees (trial %ld)", (int)na,
		    (int)nb, trial);
	diff_eq_int("the filter's history agrees (trial %ld)",
		    memcmp(ha, hb, na * sizeof(float)) == 0, 1, trial);
}

/*
 * Everything, after any member.  Cheap enough to run on every trial and the
 * only way a member that wrote through the wrong pointer is caught.
 */
static void
compare_all(const char *what, long trial)
{
	compare_obj(what, trial);
	compare_p3m(what, trial);
	compare_p4m(what, trial);
	compare_bts(what, trial);
	compare_resampler(what, trial);
	compare_queue(trial);
	compare_scrambler(trial);
	compare_filter(trial);
	diff_eq_int("nothing was stored past the object (trial %ld)",
		    guard_intact(), 1, trial);
}

/* ------------------------------------------------------------------ */
/* getV92TxFilterDelay                                                  */
/* ------------------------------------------------------------------ */

/*
 * The sweep is not just {0, 1}.  The object's `cmp $0x1; sbb; not; and $0x12`
 * is an UNSIGNED test for non-zero over a field declared `int`, so a negative
 * value and a value above one are the two inputs a signed reading would get
 * wrong, and both are here.
 */
static const int delay_values[] = { 0, 1, 2, -1, 0x7fffffff,
				    (-0x7fffffff - 1), 18, -18 };
#define NDELAY	((int)(sizeof(delay_values) / sizeof(delay_values[0])))

static int
run_delay(void)
{
	int trial, saw_zero = 0, saw_delay = 0;

	diff_begin("V92Modulator::getV92TxFilterDelay");

	build(0);
	for (trial = 0; trial < NDELAY; trial++) {
		V92Parameters *pp = (V92Parameters *)arg_params;
		int a, b;

		pp->V92_APPLY_TX_SHAPING_FILTER = delay_values[trial];
		a = our_delay(ours);
		b = ref_delay(theirs);

		diff_eq_int("the delay agrees (trial %ld)", a, b, trial);
		diff_eq_int("and is 18 or 0 (trial %ld)",
			    a == V92MOD_TX_FILTER_DELAY || a == 0, 1, trial);
		compare_all("after getV92TxFilterDelay", trial);

		if (a == 0)
			saw_zero = 1;
		if (a == V92MOD_TX_FILTER_DELAY)
			saw_delay = 1;
	}
	teardown();

	diff_eq_int("the filter-off answer was reached", saw_zero, 1, 0);
	diff_eq_int("the filter-on answer was reached", saw_delay, 1, 0);
	diff_eq_int("delay trials run", NDELAY, NDELAY, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* reset                                                                */
/* ------------------------------------------------------------------ */

/*
 * The constructor already ran `reset` once, so calling it again on a fresh
 * object would compare two identical no-ops.  Every field `reset` writes is
 * therefore DIRTIED first, identically on both sides, and the queue is drained
 * and the scrambler run so that the two calls it makes have work to do.
 */
static void
dirty(int trial)
{
	int s;

	for (s = 0; s < 2; s++) {
		V92Modulator *m = M(s);
		unsigned i;

		m->phase = (unsigned int)(1 + trial % 4);
		m->word_30 = 0xdeadbeefu;
		m->word_34 = 0x12345678u;
		m->resamplerPhaseChange = (unsigned int)(trial % 3);
		m->resamplerPhaseChangeAt = (unsigned int)(trial * 3);
		m->blockRemaining = 0xffffu;
		m->queuePrime = 0xffffu;
		m->byte_0c = (unsigned char)(trial + 1);
		m->byte_0d = (unsigned char)(trial + 2);
		m->float_28 = 1.5f;
		m->scrambler.process((int)(trial & 1));

		/*
		 * THE FILTER'S HISTORY IS ZERO OUT OF THE CONSTRUCTOR, so
		 * `txFilter->reset()` -- which zeroes it -- is the identity
		 * until something has been through it.  Dirtying it is what
		 * makes "reset does not put the transmit filter back" a
		 * catchable mutation; it ran green without this.
		 */
		{
			float *hist;
			unsigned int len;
			const unsigned char *f =
			    (const unsigned char *)m->txFilter;

			memcpy(&hist, f + 0x04, sizeof(hist));
			memcpy(&len, f + 0x0c, sizeof(len));
			for (i = 0; i < len; i++)
				hist[i] = 0.5f + (float)(i % 7);
		}
		for (i = 0; i < 3; i++) {
			float drained;

			m->queue->read(&drained, 1);
		}
	}
}

#define NRESET	13

static int
run_reset(void)
{
	int trial;

	diff_begin("V92Modulator::reset");

	for (trial = 0; trial < NRESET; trial++) {
		build(trial);
		dirty(trial);

		our_reset(ours);
		ref_reset(theirs);

		compare_all("after reset", trial);
		diff_eq_int("reset took the phase back to zero (trial %ld)",
			    (int)M(0)->phase, 0, trial);
		diff_eq_int("reset cleared the pending phase change "
			    "(trial %ld)", (int)M(0)->resamplerPhaseChange,
			    V92MOD_PHASECHG_NONE, trial);
		diff_eq_int("reset restored the block remaining (trial %ld)",
			    (int)M(0)->blockRemaining, (int)M(0)->blockSize,
			    trial);
		diff_eq_int("reset primed half the queue length (trial %ld)",
			    (int)M(0)->queuePrime,
			    ((V92Parameters *)arg_params)
				->MODULATOR_QUEUE_LENGTH >> 1, trial);
		diff_eq_int("reset left the split point alone (trial %ld)",
			    (int)M(0)->resamplerPhaseChangeAt,
			    (int)(unsigned int)(trial * 3), trial);
		teardown();
	}

	diff_eq_int("reset trials run", NRESET, NRESET, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* enterPhase3, enterDataPhase                                          */
/* ------------------------------------------------------------------ */

/*
 * Both are guarded on `phase` and both are driven from EVERY value the guard
 * can see, so the early return and the body are each reached.  0 is the value
 * `reset` leaves and is not one of the three named ones.
 */
static const unsigned int phase_values[] = { 0, 1, 2, 3, 4, 0xffffffffu };
#define NPHASE	((int)(sizeof(phase_values) / sizeof(phase_values[0])))

static int
run_enter(void)
{
	int trial, acted3 = 0, skipped3 = 0, actedD = 0, skippedD = 0;
	int acted4 = 0, skipped4 = 0;

	diff_begin("V92Modulator::enterPhase3, ::enterPhase4, ::enterDataPhase");

	for (trial = 0; trial < NPHASE * 3; trial++) {
		int which = trial % 3;
		int pi = trial / 3;

		build(trial);
		M(0)->phase = phase_values[pi];
		M(1)->phase = phase_values[pi];
		M(0)->word_30 = 0xa5a5a5a5u;
		M(1)->word_30 = 0xa5a5a5a5u;
		M(0)->word_34 = 0x5a5a5a5au;
		M(1)->word_34 = 0x5a5a5a5au;
		M(0)->resamplerPhaseChange = V92MOD_PHASECHG_OFFSET;
		M(1)->resamplerPhaseChange = V92MOD_PHASECHG_OFFSET;
		/*
		 * `blockRemaining` IS MOVED OFF `blockSize`, and that is what
		 * makes `enterDataPhase`'s argument testable at all: `reset`
		 * leaves the two equal, so a member that handed on what is
		 * LEFT of the block rather than the block itself agreed on
		 * every trial until this line existed.
		 */
		M(0)->blockRemaining = M(0)->blockSize - 7u;
		M(1)->blockRemaining = M(1)->blockSize - 7u;
		/*
		 * THE TWO FLAG BYTES ARE GIVEN DIFFERENT VALUES, which is what
		 * makes "enterPhase4 handed on byte_0c" a check that can fail.
		 * `reset` clears both, so out of the constructor they are equal
		 * and the two readings agree on every trial.  Finding F7105.
		 */
		M(0)->byte_0c = M(1)->byte_0c = (unsigned char)(0x11 + trial);
		M(0)->byte_0d = M(1)->byte_0d = (unsigned char)(0x40 + trial);
		/* `V92Phase4Modulator::reset` runs `cp->infoToBits()`; see
		 * `sane_cp` for why the seeded CP has to be bounded first. */
		sane_cp(trial);

		if (which == 0) {
			our_enterPhase3(ours);
			ref_enterPhase3(theirs);
			compare_all("after enterPhase3", trial);
			if (phase_values[pi] == V92MOD_PHASE_3) {
				diff_eq_int("enterPhase3 did nothing at 1 "
					    "(trial %ld)",
					    (int)M(0)->word_34, 0x5a5a5a5a,
					    trial);
				skipped3 = 1;
			} else {
				diff_eq_int("enterPhase3 took the phase to 1 "
					    "(trial %ld)", (int)M(0)->phase,
					    V92MOD_PHASE_3, trial);
				diff_eq_int("and dropped the pending phase "
					    "change (trial %ld)",
					    (int)M(0)->resamplerPhaseChange,
					    V92MOD_PHASECHG_NONE, trial);
				acted3 = 1;
			}
		} else if (which == 1) {
			/*
			 * `enterPhase4` resets the PHASE 4 modulator, so the
			 * witness is that sub-object rather than a word of the
			 * modulator: 460 bytes of it move, and `compare_p4m`
			 * inside `compare_all` is what compares them.  The
			 * three checks here are the ones the phase 4 fixture
			 * cannot make, because they are about which of the
			 * modulator's two flag bytes reached `reset`.
			 */
			our_enterPhase4(ours);
			ref_enterPhase4(theirs);
			compare_all("after enterPhase4", trial);
			if (phase_values[pi] == V92MOD_PHASE_4) {
				diff_eq_int("enterPhase4 did nothing at 2 "
					    "(trial %ld)", (int)M(0)->word_34,
					    0x5a5a5a5a, trial);
				skipped4 = 1;
			} else {
				diff_eq_int("enterPhase4 took the phase to 2 "
					    "(trial %ld)", (int)M(0)->phase,
					    V92MOD_PHASE_4, trial);
				diff_eq_int("and cleared the running count "
					    "(trial %ld)", (int)M(0)->word_30, 0,
					    trial);
				diff_eq_int("and the status (trial %ld)",
					    (int)M(0)->word_34, 0, trial);
				/*
				 * IT PASSES `byte_0c` AND NOT `byte_0d`, which
				 * is the ONLY thing separating this call from
				 * the two `initiate` members'.  `reset`'s
				 * second argument becomes `byte_42` raw, so
				 * reading it back is reading which byte went
				 * in -- and `dirty` gives the two different
				 * values for exactly this check.
				 */
				diff_eq_int("and handed on byte_0c, not byte_0d "
					    "(trial %ld)",
					    (int)M(0)->phase4Modulator->byte_42,
					    (int)M(0)->byte_0c, trial);
				/*
				 * THE SYMBOL COUNT IS ZERO, so `reset`'s
				 * generation loop never ran: `nSymbols` is a
				 * trip count and is not stored, and this is
				 * the only place it shows.  The state is the
				 * literal zero the enum's one enumerator is.
				 */
				diff_eq_int("and asked for no symbols at all "
					    "(trial %ld)",
					    (int)M(0)->phase4Modulator
						->symbolCount, 0, trial);
				diff_eq_int("and reset it into state zero "
					    "(trial %ld)",
					    M(0)->phase4Modulator->state,
					    V92P4M_RESET_STATE_ZERO, trial);
				acted4 = 1;
			}
		} else {
			our_enterDataPhase(ours);
			ref_enterDataPhase(theirs);
			compare_all("after enterDataPhase", trial);
			if (phase_values[pi] == V92MOD_PHASE_DATA) {
				diff_eq_int("enterDataPhase did nothing at 3 "
					    "(trial %ld)",
					    (int)M(0)->word_34, 0x5a5a5a5a,
					    trial);
				skippedD = 1;
			} else {
				diff_eq_int("enterDataPhase took the phase to "
					    "3 (trial %ld)", (int)M(0)->phase,
					    V92MOD_PHASE_DATA, trial);
				diff_eq_int("and handed the block size on "
					    "(trial %ld)",
					    (int)M(0)->bitsToSymbol
						->symbolsBlockSize,
					    (int)M(0)->blockSize, trial);
				diff_eq_int("and left the status at ten "
					    "(trial %ld)", (int)M(0)->word_34,
					    10, trial);
				actedD = 1;
			}
		}
		teardown();
	}

	diff_eq_int("enterPhase3 acted", acted3, 1, 0);
	diff_eq_int("enterPhase3 returned early", skipped3, 1, 0);
	diff_eq_int("enterPhase4 acted", acted4, 1, 0);
	diff_eq_int("enterPhase4 returned early", skipped4, 1, 0);
	diff_eq_int("enterDataPhase acted", actedD, 1, 0);
	diff_eq_int("enterDataPhase returned early", skippedD, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* the five exits                                                       */
/* ------------------------------------------------------------------ */

struct exitcase {
	const char *name;
	member_fn ours, theirs;
	int phase4;		/* 1 == guarded on the phase 4 modulator */
	int acts_on;		/* the one state code it acts on         */
};

static const struct exitcase exits[] = {
	{ "exitJa",	     our_exitJa,	  ref_exitJa,	       0,
	  V92P3M_STATE_JA },
	{ "exitSilence",     our_exitSilence,	  ref_exitSilence,     0,
	  V92P3M_STATE_SILENCE },
	{ "exitSuSecond",    our_exitSuSecond,	  ref_exitSuSecond,    0,
	  V92P3M_STATE_SU_SECOND },
	{ "exitTRN1uSecond", our_exitTRN1uSecond, ref_exitTRN1uSecond, 0,
	  V92P3M_STATE_TRN1U_SECOND },
	{ "exitCPt",	     our_exitCPt,	  ref_exitCPt,	       1, 0 }
};
#define NEXIT	((int)(sizeof(exits) / sizeof(exits[0])))

/*
 * Every exit is driven over EVERY state its guard can see, not just its own:
 * a guard written for the wrong code, or dropped, differs on the states it
 * should have ignored and nowhere else.  16 covers the phase 3 alphabet and
 * 30 covers the phase 4 one.
 */
#define NP3STATE	16
#define NP4STATE	30

static int
run_exits(void)
{
	int e, st, trial = 0;
	int acted[NEXIT], ignored[NEXIT];

	diff_begin("V92Modulator's five exits");

	for (e = 0; e < NEXIT; e++) {
		acted[e] = 0;
		ignored[e] = 0;
	}

	for (e = 0; e < NEXIT; e++) {
		int nstate = exits[e].phase4 ? NP4STATE : NP3STATE;

		for (st = 0; st < nstate; st++, trial++) {
			int before, after;

			build(trial);
			M(0)->word_34 = 0x5a5a5a5au;
			M(1)->word_34 = 0x5a5a5a5au;

			if (exits[e].phase4) {
				M(0)->phase4Modulator->state = st;
				M(1)->phase4Modulator->state = st;
				M(0)->phase4Modulator->symbolCount = 24u;
				M(1)->phase4Modulator->symbolCount = 24u;
				before = st;
			} else {
				M(0)->phase3Modulator->state =
				    (V92Phase3ModulatorState)st;
				M(1)->phase3Modulator->state =
				    (V92Phase3ModulatorState)st;
				M(0)->phase3Modulator->symbolCount = 12u;
				M(1)->phase3Modulator->symbolCount = 12u;
				before = st;
			}

			exits[e].ours(ours);
			exits[e].theirs(theirs);

			compare_all(exits[e].name, trial);

			after = (int)M(0)->word_34;
			if (before == exits[e].acts_on) {
				diff_eq_int("the exit cleared the status "
					    "(trial %ld)", after, 0, trial);
				acted[e] = 1;
			} else {
				diff_eq_int("the exit left the status alone "
					    "(trial %ld)", after, 0x5a5a5a5a,
					    trial);
				ignored[e] = 1;
			}
			teardown();
		}
	}

	for (e = 0; e < NEXIT; e++) {
		diff_eq_int("this exit acted at least once (case %ld)",
			    acted[e], 1, e);
		diff_eq_int("and was ignored at least once (case %ld)",
			    ignored[e], 1, e);
	}
	diff_eq_int("exit trials run", trial,
		    4 * NP3STATE + NP4STATE, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* mkResampledSignal                                                    */
/* ------------------------------------------------------------------ */

/*
 * The grid, and every axis is one the object branches on:
 *
 *   phase                  1 (the split path) and three others
 *   resamplerPhaseChange   0, 1, 2, and 3 -- which is the value the object's
 *                          `cmp $0x2; dec; je` falls THROUGH on, so it splits
 *                          the block and changes no phase.  An `if/else` on
 *                          the code would apply the half-sample step to it.
 *   resamplerPhaseOffset   negative, zero, small and large enough to wrap
 *   resamplerPhaseChangeAt 0, the middle, and the whole block
 *   blockRemaining         the whole block and less
 *
 * The wrap is what selects between the two join loops, so the offsets are
 * chosen to reach both: 0.9 plus a phase already past 0.1 wraps, 0.2 does not.
 */
static const float mkres_offsets[] = { -0.25f, 0.0f, 0.2f, 0.5f, 0.9f,
				       1.75f };
#define NOFFSET	((int)(sizeof(mkres_offsets) / sizeof(mkres_offsets[0])))

/*
 * THE STARTING PHASES, and 0.5 IS THERE FOR ONE REASON.  The wrap test is
 * `>= 1.0f` and a `> 1.0f` differs from it at exactly one input, so the grid
 * has to produce exactly 1.0f or the two readings are indistinguishable.
 * `setNormalizedPhase(0.5f)` stores `0.5 * 120` and `getNormalizedPhase()`
 * gives 0.5f back with no rounding, so 0.5 plus the half-sample step -- and
 * 0.5 plus the 0.5f offset -- is 1.0f exactly.  It only holds when the FIRST
 * segment consumed nothing, which is the `split == 0` column: `resample` with
 * no input and nothing pending leaves the phase where it was.
 */
static const float mkres_starts[] = { 0.05f, 0.35f, 0.5f, 0.65f };
#define NSTART	((int)(sizeof(mkres_starts) / sizeof(mkres_starts[0])))

static const unsigned int mkres_codes[] = { 0, 1, 2, 3 };
#define NCODE	((int)(sizeof(mkres_codes) / sizeof(mkres_codes[0])))

/* Phase 3 is the only one that splits; the other three all take the simple
 * path however the code is set, and 0 is the value `reset` leaves. */
static const unsigned int mkres_phases[] = { 1, 0, 2, 3 };
#define NMKPHASE ((int)(sizeof(mkres_phases) / sizeof(mkres_phases[0])))

#define NSPLIT	4

/*
 * The two output buffers are wiped with values the resampler CANNOT produce:
 * every input is in [-1, 1] and the polyphase kernel is a normalised low-pass,
 * so nothing it writes is anywhere near -1000.  That is what makes "the second
 * segment produced at least one sample" a real check rather than a hope.
 */
#define MKRES_WIPE_OUT		-2000.0f
#define MKRES_WIPE_TAIL		-1000.0f
#define MKRES_UNTOUCHED		-100.0f

static void
fill_input(int trial)
{
	unsigned int n = M(0)->blockSize + V92MOD_BUF_SLACK;
	unsigned int i;

	for (i = 0; i < n; i++) {
		float v = (float)((int)((lfsr_step() >> 3) & 0x7f) - 64)
			  / 64.0f;

		M(0)->resampleIn[i] = v;
		M(1)->resampleIn[i] = v;
	}
	/* The two outputs are wiped identically, so a member that wrote
	 * nothing is caught by the wipe still being there. */
	n = NSAMPLES + V92MOD_BUF_SLACK;
	for (i = 0; i < n; i++) {
		M(0)->resampleOut[i] = MKRES_WIPE_OUT;
		M(1)->resampleOut[i] = MKRES_WIPE_OUT;
		M(0)->resampleTail[i] = MKRES_WIPE_TAIL;
		M(1)->resampleTail[i] = MKRES_WIPE_TAIL;
	}
	(void)trial;
}

static int
run_mkres(void)
{
	int pi, ci, si, oi, ti, trial = 0;
	int saw_simple = 0, saw_split = 0, saw_wrap = 0, saw_nowrap = 0;
	int saw_fallthrough = 0, saw_half = 0, saw_offset = 0, saw_exact = 0;

	diff_begin("V92Modulator::mkResampledSignal");

	for (pi = 0; pi < NMKPHASE; pi++) {
	 for (ci = 0; ci < NCODE; ci++) {
	  for (si = 0; si < NSPLIT; si++) {
	   for (oi = 0; oi < NOFFSET; oi++) {
	    for (ti = 0; ti < NSTART; ti++, trial++) {
		unsigned int na = 0xa5a5a5a5u, nb = 0xa5a5a5a5u;
		unsigned int rem, at;
		float p0;
		int s;

		build(trial);
		/*
		 * THE SECOND SEGMENT ALWAYS GETS AT LEAST EIGHT INPUT
		 * SAMPLES, and that is not tidiness.  At `blockRemaining -
		 * resamplerPhaseChangeAt == 0` the second `resample` can
		 * return `n2 == 0`, and the WRAPPED join's trip count is
		 * `n2 - 1` computed unsigned -- the object's own `dec %edi;
		 * cmp $0x0,%edi; jbe` does not take the branch at
		 * 0xffffffff -- so both sides would copy four billion floats
		 * off the end of the same buffers at once.  That measures the
		 * allocator rather than the reading (D571's argument) and is
		 * recorded as D802 rather than driven.
		 */
		rem = (si == 3) ? M(0)->blockSize * 3u / 4u : M(0)->blockSize;
		at = (si == 0) ? 0u
		   : (si == 1) ? rem / 4u
		   : (si == 2) ? rem / 2u
		   : rem - 8u;

		/* The starting phase, so both the wrapping and the
		 * non-wrapping join are reached. */
		p0 = mkres_starts[ti];

		for (s = 0; s < 2; s++) {
			V92Modulator *m = M(s);

			m->phase = mkres_phases[pi];
			m->resamplerPhaseChange = mkres_codes[ci];
			m->resamplerPhaseChangeAt = at;
			m->resamplerPhaseOffset = mkres_offsets[oi];
			m->blockRemaining = rem;
			m->resampler->setNormalizedPhase(p0);
		}
		fill_input(trial);

		our_mkres(ours, &na);
		ref_mkres(theirs, &nb);

		diff_eq_int("the output count agrees (trial %ld)", (int)na,
			    (int)nb, trial);
		diff_eq_int("the output count is inside the buffer "
			    "(trial %ld)", na <= NSAMPLES + V92MOD_BUF_SLACK,
			    1, trial);
		diff_eq_int("and something came out at all (trial %ld)",
			    na > 0u, 1, trial);
		diff_eq_int("and the first sample was written (trial %ld)",
			    M(0)->resampleOut[0] > MKRES_UNTOUCHED, 1, trial);
		diff_eq_int("the resampled block agrees (trial %ld)",
			    memcmp(M(0)->resampleOut, M(1)->resampleOut,
				   (NSAMPLES + V92MOD_BUF_SLACK)
				   * sizeof(float)) == 0, 1, trial);
		diff_eq_int("the split scratch agrees (trial %ld)",
			    memcmp(M(0)->resampleTail, M(1)->resampleTail,
				   (NSAMPLES + V92MOD_BUF_SLACK)
				   * sizeof(float)) == 0, 1, trial);
		compare_all("after mkResampledSignal", trial);

		if (M(0)->phase == V92MOD_PHASE_3
		    && mkres_codes[ci] != V92MOD_PHASECHG_NONE) {
			diff_eq_int("the split path cleared the code "
				    "(trial %ld)",
				    (int)M(0)->resamplerPhaseChange,
				    V92MOD_PHASECHG_NONE, trial);
			diff_eq_int("and cleared the split point (trial %ld)",
				    (int)M(0)->resamplerPhaseChangeAt, 0,
				    trial);
			diff_eq_int("the second segment produced a sample "
				    "(trial %ld)",
				    M(0)->resampleTail[0] > MKRES_UNTOUCHED, 1,
				    trial);
			saw_split = 1;
			if (mkres_codes[ci] == V92MOD_PHASECHG_HALF)
				saw_half = 1;
			else if (mkres_codes[ci] == V92MOD_PHASECHG_OFFSET)
				saw_offset = 1;
			else
				saw_fallthrough = 1;
		} else {
			diff_eq_int("the simple path left the code alone "
				    "(trial %ld)",
				    (int)M(0)->resamplerPhaseChange,
				    (int)mkres_codes[ci], trial);
			diff_eq_int("and left the split point alone "
				    "(trial %ld)",
				    (int)M(0)->resamplerPhaseChangeAt,
				    (int)at, trial);
			saw_simple = 1;
		}

		/*
		 * The wrap: the resampler's phase after the call is below
		 * where it would have been had nothing wrapped.  Counted from
		 * the SIDE UNDER TEST and asserted equal on both, so this is a
		 * separating trial and not a path counter (findings F3403,
		 * F3509).
		 */
		if (M(0)->phase == V92MOD_PHASE_3
		    && (mkres_codes[ci] == V92MOD_PHASECHG_HALF
			|| mkres_codes[ci] == V92MOD_PHASECHG_OFFSET)) {
			float step = (mkres_codes[ci] == V92MOD_PHASECHG_HALF)
				   ? V92MOD_PHASECHG_HALF_STEP
				   : mkres_offsets[oi];

			if (p0 + step >= 1.0f)
				saw_wrap = 1;
			else
				saw_nowrap = 1;
			/* The one input `>` and `>=` disagree on, and it is
			 * only exact when the first segment took nothing. */
			if (at == 0u && p0 + step == 1.0f)
				saw_exact = 1;
		}
		teardown();
	    }
	   }
	  }
	 }
	}

	diff_eq_int("the simple path was reached", saw_simple, 1, 0);
	diff_eq_int("the split path was reached", saw_split, 1, 0);
	diff_eq_int("the half-sample arm was reached", saw_half, 1, 0);
	diff_eq_int("the offset arm was reached", saw_offset, 1, 0);
	diff_eq_int("a code that is neither was reached", saw_fallthrough, 1,
		    0);
	diff_eq_int("a wrapping phase change was reached", saw_wrap, 1, 0);
	diff_eq_int("a non-wrapping one was reached", saw_nowrap, 1, 0);
	diff_eq_int("and one landing on exactly 1.0f", saw_exact, 1, 0);
	diff_eq_int("mkResampledSignal trials run", trial,
		    NMKPHASE * NCODE * NSPLIT * NOFFSET * NSTART, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* the diagnostics                                                      */
/* ------------------------------------------------------------------ */

/*
 * How many lines of a transcript start with `prefix`.  A line is the start of
 * the buffer or whatever follows a newline; `dsplibs_debug_printf` appends
 * "\r\n" and `edprintf` appends "\n", so both end one.
 */
static int
count_prefix(const char *text, const char *prefix)
{
	size_t n = strlen(prefix);
	int count = 0;
	int at_line_start = 1;
	const char *p;

	for (p = text; *p != '\0'; p++) {
		if (at_line_start && strncmp(p, prefix, n) == 0)
			count++;
		at_line_start = (*p == '\n');
	}
	return count;
}

/*
 * The five phase steps the diagnostic is printed over.  Zero separates
 * `0.0f < v` from `v < 0.0f`; -1.75 and 1.75 separate `(int)fabsf(v)` from
 * `(int)v`; -0.375 and 0.5 keep a fractional part that is not a round number.
 */
static const float diag_offsets[] = { -0.375f, 0.0f, 0.5f, -1.75f, 1.75f };
#define NDIAGOFF ((int)(sizeof(diag_offsets) / sizeof(diag_offsets[0])))

/*
 * Nine of the ten gate on `> 1`; `enterDataPhase` gates one level down, inside
 * `edprintf`, and the file comment says what that does and does not change.  The
 * level is swept 0..3 rather than raised, which is finding F150's rule, and the
 * line counts are asserted against literals as well as against the blob's
 * (finding F149).
 */
static int
run_debug(void)
{
	int lvl, saw = 0;

	diff_begin("V92Modulator's fifteen diagnostics");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl < 4; lvl++) {
		unsigned lines;
		int want, own, enc;

		build(lvl);
		set_level((unsigned)lvl);
		dsplib_debug_capture_reset();

		/*
		 * One call of each of the nine gated sites plus the one
		 * ungated site, each set up so its body runs.
		 */
		M(0)->phase = 0;
		M(1)->phase = 0;
		our_enterPhase3(ours);
		ref_enterPhase3(theirs);

		M(0)->phase3Modulator->state = V92P3M_STATE_JA;
		M(1)->phase3Modulator->state = V92P3M_STATE_JA;
		our_exitJa(ours);
		ref_exitJa(theirs);

		M(0)->phase3Modulator->state = V92P3M_STATE_SILENCE;
		M(1)->phase3Modulator->state = V92P3M_STATE_SILENCE;
		our_exitSilence(ours);
		ref_exitSilence(theirs);

		M(0)->phase3Modulator->state = V92P3M_STATE_SU_SECOND;
		M(1)->phase3Modulator->state = V92P3M_STATE_SU_SECOND;
		our_exitSuSecond(ours);
		ref_exitSuSecond(theirs);

		M(0)->phase3Modulator->state = V92P3M_STATE_TRN1U_SECOND;
		M(1)->phase3Modulator->state = V92P3M_STATE_TRN1U_SECOND;
		our_exitTRN1uSecond(ours);
		ref_exitTRN1uSecond(theirs);

		M(0)->phase4Modulator->state = 0;
		M(1)->phase4Modulator->state = 0;
		M(0)->phase4Modulator->symbolCount = 24u;
		M(1)->phase4Modulator->symbolCount = 24u;
		our_exitCPt(ours);
		ref_exitCPt(theirs);

		our_reset(ours);
		ref_reset(theirs);

		/*
		 * `enterPhase4`'s message, and BOTH arms of each `initiate`
		 * member -- five gated sites that no other trial in this file
		 * reaches, because every other runner drives them at level 0.
		 * `debugcov.py` counts a site that never executes, so a
		 * message with no trial behind it is a claim about the object's
		 * text with nothing testing it.
		 *
		 * The two `initiate` members reach
		 * `V92Phase4Modulator::reset` and through it `V92CP::
		 * infoToBits`, which prints ADDRESSES at level 2 -- the CP is
		 * the SHARED argument block here, so both sides print the same
		 * one and the transcript still compares.  `sane_cp` is what
		 * keeps that call inside D570 and D571.
		 */
		sane_cp(lvl);
		M(0)->phase = 0;
		M(1)->phase = 0;
		our_enterPhase4(ours);
		ref_enterPhase4(theirs);

		M(0)->phase = 0;
		M(1)->phase = 0;
		diff_eq_int("a refused RRN agrees (level %ld)",
			    our_initiateRRN(ours), ref_initiateRRN(theirs), lvl);
		diff_eq_int("a refused FPE agrees (level %ld)",
			    our_initiateFPE(ours), ref_initiateFPE(theirs), lvl);

		M(0)->phase = V92MOD_PHASE_DATA;
		M(1)->phase = V92MOD_PHASE_DATA;
		M(0)->bitsToSymbol->symbolsDone = 0u;
		M(1)->bitsToSymbol->symbolsDone = 0u;
		M(0)->bitsToSymbol->bitsPerFrame = 24u;
		M(1)->bitsToSymbol->bitsPerFrame = 24u;
		diff_eq_int("an approved RRN agrees (level %ld)",
			    our_initiateRRN(ours), ref_initiateRRN(theirs), lvl);

		M(0)->phase = V92MOD_PHASE_DATA;
		M(1)->phase = V92MOD_PHASE_DATA;
		M(0)->bitsToSymbol->symbolsDone = 3u;
		M(1)->bitsToSymbol->symbolsDone = 3u;
		diff_eq_int("an approved FPE agrees (level %ld)",
			    our_initiateFPE(ours), ref_initiateFPE(theirs), lvl);

		/* Both `mkResampledSignal` messages, one call each. */
		{
			unsigned int na = 0, nb = 0;
			int s, di;

			for (s = 0; s < 2; s++) {
				V92Modulator *m = M(s);

				m->phase = V92MOD_PHASE_3;
				m->resamplerPhaseChange =
				    V92MOD_PHASECHG_HALF;
				m->resamplerPhaseChangeAt =
				    m->blockSize / 2u;
				m->blockRemaining = m->blockSize;
				m->resamplerPhaseOffset = -0.375f;
				m->resampler->setNormalizedPhase(0.25f);
			}
			fill_input(lvl);
			our_mkres(ours, &na);
			ref_mkres(theirs, &nb);

			/*
			 * THE OFFSET ARM IS DRIVEN OVER FIVE VALUES AND THE
			 * CHOICE IS FORCED BY WHAT THE THREE CONVERSIONS DO.
			 * `sign_of` is `0.0f < v`, which differs from
			 * `v < 0.0f ? '-' : '+'` at EXACTLY ZERO and nowhere
			 * else; `whole_of` is `(int)fabsf(v)`, which differs
			 * from `(int)v` only where |v| >= 1 and v < 0.  A
			 * sweep of small negative offsets sees neither, and
			 * both of those mutations ran green until this array
			 * existed.
			 */
			for (di = 0; di < NDIAGOFF; di++) {
				for (s = 0; s < 2; s++) {
					V92Modulator *m = M(s);

					m->phase = V92MOD_PHASE_3;
					m->resamplerPhaseChange =
					    V92MOD_PHASECHG_OFFSET;
					m->resamplerPhaseChangeAt =
					    m->blockSize / 2u;
					m->blockRemaining = m->blockSize;
					m->resamplerPhaseOffset =
					    diag_offsets[di];
					m->resampler->setNormalizedPhase(
					    0.25f);
				}
				fill_input(lvl);
				our_mkres(ours, &na);
				ref_mkres(theirs, &nb);
				diff_eq_int("the two counts still agree "
					    "(level %ld)", (int)na, (int)nb,
					    lvl);
			}
		}

		/* Last, because it is the ungated one and its encoded line
		 * has to be attributable. */
		M(0)->phase = 0;
		M(1)->phase = 0;
		our_enterDataPhase(ours);
		ref_enterDataPhase(theirs);

		lines = dsplib_debug_capture_lines(0);
		diff_eq_int("both sides printed the same text (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, lvl);
		diff_eq_int("both sides printed the same number of lines "
			    "(level %ld)", (int)lines,
			    (int)dsplib_debug_capture_lines(1), lvl);

		/*
		 * EVERY LINE IS ATTRIBUTED, and the two classes are counted
		 * SEPARATELY so that a dropped message here cannot be masked
		 * by a sub-modulator's appearing.  At level 2 and 3 the
		 * transcript is, in this order:
		 *
		 *   V92Modulator enter Phase 3            enterPhase3
		 *   $!$ ...                               V92Phase3Modulator
		 *                                         ::reset, "TRN1u
		 *                                         state length set to"
		 *   V92Modulator: exit Ja                 exitJa
		 *   V92Modulator: exit Silence            exitSilence
		 *   V92Modulator: exit SuSecond           exitSuSecond
		 *   V92Modulator: exit TRN1uSecond        exitTRN1uSecond
		 *   V92Modulator: exit CPt                exitCPt
		 *   $!$ ...                               V92Phase4Modulator
		 *                                         ::exitCPt, "enter
		 *                                         E1u @ %d"
		 *   V92Modulator reset                    reset
		 *   V92Modulator: setPhase = 0.5          mkResampledSignal,
		 *                                         the HALF arm
		 *   V92Modulator: setPhase = ...  x5      mkResampledSignal,
		 *                                         the OFFSET arm, once
		 *                                         per `diag_offsets`
		 *   $!$ ...                               enterDataPhase
		 *
		 *   V92Modulator: enter Phase 4           enterPhase4
		 *   ... RRN requested but NOT approved     initiateRRN
		 *   ... FPE requested but NOT approved     initiateFPE
		 *   ... RRN requested, enter Phase 4       initiateRRN
		 *   ... FPE requested, enter Phase 4       initiateFPE
		 *
		 * THIRTEEN of this class's own plus one per `diag_offsets`, and
		 * FIVE encoded.  The encoded five are not readable by
		 * construction, so they are counted by `edprintf`'s own
		 * `"$!$ "` framing; two of them belong to sub-modulators, one
		 * to `enterDataPhase` and two to the `initiate` members' state
		 * messages, and only their TOTAL is asserted because nothing
		 * here can tell them apart.
		 *
		 * At 0 and 1 everything is silent, `enterDataPhase` included:
		 * `edprintf` gates its own `dsplibs_debug_printf` at the same
		 * `> 1` the other nine test for themselves.
		 */
		own = count_prefix(dsplib_debug_capture_text(0),
				   "V92Modulator");
		enc = count_prefix(dsplib_debug_capture_text(0), "$!$ ");
		want = (lvl > 1) ? 1 : 0;
		diff_eq_int("this class printed its own lines (level %ld)",
			    own, (13 + NDIAGOFF) * want, lvl);
		diff_eq_int("and five went through edprintf (level %ld)",
			    enc, 5 * want, lvl);
		diff_eq_int("and there was nothing else (level %ld)",
			    (int)lines, own + enc, lvl);
		if (lines > 0)
			saw = 1;
		teardown();
	}

	diff_eq_int("something was printed at all", saw, 1, 0);

	dsplib_debug_capture_on = 0;
	set_level(0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* initiateRRN, initiateFPE                                             */
/* ------------------------------------------------------------------ */

/*
 * THE TWO REQUESTS ARE THE SAME FUNCTION FOUR TIMES OVER, and the fixture is
 * built around the ONE difference a shared V92CP cannot see.
 *
 *   1. the two state codes they hand `V92Phase4Modulator::reset` -- read back
 *      out of the sub-object's `state`
 *   2. `resetBeforRRN` against `resetBeforFPE` -- different fields of the
 *      sub-object, and `compare_p4m` is what compares them
 *   3. FPE's store into `bitsToSymbol->transmitter->modulusEncoder->field_50`
 *      -- FOUR LEVELS DOWN, and checked absolutely as well as differentially
 *   4. `cp->byte_04 = 0` and `cp->infoToBits()`
 *
 * THE FOURTH NEEDS A V92CP PER SIDE AND THE REST OF THIS FILE DELIBERATELY
 * SHARES ONE.  Both sides writing the same 2,328 bytes means an omission on
 * ours is repaired by the blob's call before anything looks, so this runner --
 * and only this runner -- repoints the reference side at its own copy and
 * poisons the two pointers that then differ.  Everything else stays shared, so
 * the address is still the witness for the other five arguments.
 */
static unsigned char arg_cp2[CPSZ] __attribute__((aligned(8)));

/* Give the reference side its own CP, seeded identically. */
static void
split_cp(void)
{
	memcpy(arg_cp2, arg_cp, CPSZ);
	M(1)->cp = (V92CP *)(void *)arg_cp2;
	M(1)->phase4Modulator->cp = (V92CP *)(void *)arg_cp2;
}

/* The modulator's own bytes with `cp` at +0x20 poisoned as well. */
static void
compare_obj_splitcp(const char *what, long trial)
{
	unsigned i;

	memcpy(cmp_a, ours, OBJSZ);
	memcpy(cmp_b, theirs, OBJSZ);
	for (i = 0; i < NSKIP; i++) {
		memset(cmp_a + skip[i].off, 0x77, skip[i].len);
		memset(cmp_b + skip[i].off, 0x77, skip[i].len);
	}
	memset(cmp_a + 0x20, 0x77, 4);
	memset(cmp_b + 0x20, 0x77, 4);
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Modulator", cmp_a, cmp_b,
		     (size_t)OBJSZ, trial);
}

/* `compare_p4m` with the sub-object's own `cp` at +0x74 poisoned too. */
static void
compare_p4m_splitcp(const char *what, long trial)
{
	memcpy(sub_a, M(0)->phase4Modulator, P4MSZ);
	memcpy(sub_b, M(1)->phase4Modulator, P4MSZ);
	memset(sub_a + 0x4c, 0x77, 7 * sizeof(void *));
	memset(sub_b + 0x4c, 0x77, 7 * sizeof(void *));
	memset(sub_a + 0x6c, 0x77, 2 * sizeof(void *));
	memset(sub_b + 0x6c, 0x77, 2 * sizeof(void *));
	memset(sub_a + 0x74, 0x77, sizeof(void *));
	memset(sub_b + 0x74, 0x77, sizeof(void *));
	/*
	 * `pattern` at +0x1a8 points INTO the CP, so with one CP per side the
	 * two addresses differ; the DISTANCE from each side's own CP is what
	 * still compares, and it is checked separately below.
	 */
	memset(sub_a + 0x1a8, 0x77, sizeof(void *));
	memset(sub_b + 0x1a8, 0x77, sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Phase4Modulator", sub_a,
		     sub_b, (size_t)P4MSZ, trial);
}

static unsigned int
modenc_selector(int s)
{
	return (unsigned int)M(s)->bitsToSymbol->transmitter->modulusEncoder
			       ->field_50;
}

struct initcase {
	const char *name;
	int (*ours)(void *);
	int (*theirs)(void *);
	int fpe;
	int signalState;	/* nofBitsForNextTime() != 0 */
	int dataState;		/* nofBitsForNextTime() == 0 */
};

static const struct initcase inits[] = {
	{ "initiateRRN", our_initiateRRN, ref_initiateRRN, 0,
	  V92P4M_STATE_RU, V92P4M_STATE_DATA_TO_RU },
	{ "initiateFPE", our_initiateFPE, ref_initiateFPE, 1,
	  V92P4M_STATE_RM, V92P4M_STATE_DATA_TO_RM }
};
#define NINIT	2

static int
run_initiate(void)
{
	int e, pi, bi, trial = 0;
	int approved = 0, refused = 0, sawSignal = 0, sawData = 0;
	int sawSelector = 0, sawNoSelector = 0;

	diff_begin("V92Modulator::initiateRRN, ::initiateFPE");

	for (e = 0; e < NINIT; e++) {
	 for (pi = 0; pi < NPHASE; pi++) {
	  /*
	   * THE BIT-COUNT AXIS, and it is keyed on something the FUNCTION
	   * changes rather than on a constant (finding F7458).  Both members
	   * call `setSymbolsBlockSize(1)` first, so what `nofBitsForNextTime`
	   * returns is decided by `symbolsDone`: at three the one-symbol block
	   * is already banked, the count is ZERO and the modulator enters the
	   * "finish the data first" state; at zero the count is a frame's worth
	   * of bits and the renegotiation signal starts at once.
	   */
	  for (bi = 0; bi < 2; bi++, trial++) {
		int want, rc_a, rc_b;
		int approvedTrial = (phase_values[pi] == V92MOD_PHASE_DATA);
		int s;

		build(trial);
		sane_cp(trial);
		split_cp();

		for (s = 0; s < 2; s++) {
			V92Modulator *m = M(s);

			m->phase = (int)phase_values[pi];
			m->word_30 = 0xa5a5a5a5u;
			m->word_34 = 0x5a5a5a5au;
			m->byte_0c = (unsigned char)(0x11 + trial);
			m->byte_0d = (unsigned char)(0x40 + trial);
			m->bitsToSymbol->symbolsDone = bi ? 3u : 0u;
			m->bitsToSymbol->bitsPerFrame = 24u;
			m->bitsToSymbol->transmitter->modulusEncoder
			    ->field_50 = 0x77;
		}

		rc_a = inits[e].ours(ours);
		rc_b = inits[e].theirs(theirs);

		diff_eq_int("the status agrees (trial %ld)", rc_a, rc_b, trial);

		compare_obj_splitcp(inits[e].name, trial);
		compare_p3m(inits[e].name, trial);
		compare_p4m_splitcp(inits[e].name, trial);
		compare_bts(inits[e].name, trial);
		compare_resampler(inits[e].name, trial);
		compare_queue(trial);
		compare_scrambler(trial);
		compare_filter(trial);
		diff_eq_int("nothing was stored past the object (trial %ld)",
			    guard_intact(), 1, trial);

		/* The two V92CPs, which is what the split exists for. */
		diff_eq_int("the two V92CPs agree (trial %ld)",
			    memcmp(arg_cp, arg_cp2, CPSZ) == 0, 1, trial);

		/* The modulus encoder, four levels down. */
		diff_eq_int("the modulus encoder's selector agrees (trial %ld)",
			    (int)modenc_selector(0), (int)modenc_selector(1),
			    trial);

		if (!approvedTrial) {
			refused = 1;
			diff_eq_int("a refusal returns -1 (trial %ld)", rc_a, -1,
				    trial);
			diff_eq_int("and changes the phase not at all "
				    "(trial %ld)", (int)M(0)->phase,
				    (int)phase_values[pi], trial);
			diff_eq_int("and leaves the status alone (trial %ld)",
				    (int)M(0)->word_34, 0x5a5a5a5a, trial);
			diff_eq_int("and the running count (trial %ld)",
				    (int)M(0)->word_30, (int)0xa5a5a5a5u,
				    trial);
			diff_eq_int("and the encoder's selector (trial %ld)",
				    (int)modenc_selector(0), 0x77, trial);
			sawNoSelector = 1;
			teardown();
			continue;
		}

		approved = 1;
		want = bi ? inits[e].dataState : inits[e].signalState;
		if (bi)
			sawData = 1;
		else
			sawSignal = 1;

		diff_eq_int("an approved request returns 0 (trial %ld)", rc_a, 0,
			    trial);
		diff_eq_int("and takes the phase to 4 (trial %ld)",
			    (int)M(0)->phase, V92MOD_PHASE_4, trial);
		diff_eq_int("and clears the running count (trial %ld)",
			    (int)M(0)->word_30, 0, trial);
		diff_eq_int("and the status (trial %ld)", (int)M(0)->word_34, 0,
			    trial);
		diff_eq_int("and asks for one symbol a block (trial %ld)",
			    (int)M(0)->bitsToSymbol->symbolsBlockSize, 1, trial);
		diff_eq_int("and chose the state the bit count names "
			    "(trial %ld)", M(0)->phase4Modulator->state, want,
			    trial);
		/*
		 * `byte_0d`, NOT `byte_0c`: the one argument that separates
		 * these two members from `enterPhase4`, and the two bytes are
		 * seeded differently above so the check can fail.
		 */
		diff_eq_int("and handed on byte_0d (trial %ld)",
			    (int)M(0)->phase4Modulator->byte_42,
			    (int)M(0)->byte_0d, trial);
		diff_eq_int("and cleared the CP's bit 33 (trial %ld)",
			    (int)((V92CP *)(void *)arg_cp)->byte_04, 0, trial);
		/*
		 * `pattern` points INTO the CP, and only after `reset` has
		 * re-derived it -- on a refusal it still points into the
		 * SHARED block the constructor gave it, which is why this
		 * check is here and not beside the object comparison.
		 */
		diff_eq_int("the bit vectors are the same distance in "
			    "(trial %ld)",
			    (int)((const unsigned char *)M(0)->phase4Modulator
				  ->pattern - (const unsigned char *)arg_cp),
			    (int)((const unsigned char *)M(1)->phase4Modulator
				  ->pattern - (const unsigned char *)arg_cp2),
			    trial);

		/*
		 * THE ONE STORE THAT IS NOT IN BOTH.  Swapping the two bodies
		 * moves nothing else the fixture can see.
		 */
		if (inits[e].fpe) {
			diff_eq_int("FPE armed the modulus encoder (trial %ld)",
				    (int)modenc_selector(0), 1, trial);
			sawSelector = 1;
		} else {
			diff_eq_int("RRN left the modulus encoder alone "
				    "(trial %ld)", (int)modenc_selector(0),
				    0x77, trial);
			sawNoSelector = 1;
		}

		teardown();
	  }
	 }
	}

	diff_eq_int("a request was approved", approved, 1, 0);
	diff_eq_int("a request was refused", refused, 1, 0);
	diff_eq_int("the signal state was chosen", sawSignal, 1, 0);
	diff_eq_int("the data-first state was chosen", sawData, 1, 0);
	diff_eq_int("the encoder was armed", sawSelector, 1, 0);
	diff_eq_int("and left alone", sawNoSelector, 1, 0);
	diff_eq_int("initiate trials run", trial, NINIT * NPHASE * 2, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* progress                                                             */
/* ------------------------------------------------------------------ */

/*
 * ONE BLOCK OF UPSTREAM, and the grid is built from what the object branches
 * on rather than from what the function looks like it does.
 *
 * THE FIVE ARMS OF THE SWITCH, and the two that are one value each.  `phase`
 * is a SIGNED int here -- .text+0x14c9e lowers the dispatch with `jle` -- so
 * the set has to include a NEGATIVE value as well as a large positive one, and
 * they take different paths through the object's comparison tree even though
 * both land in the default arm.
 *
 * THE OCCUPANCY AXIS, WHICH IS THE ONE THAT WOULD HAVE BEEN VACUOUS.  Four of
 * the five arms set `blockRemaining = queuePrime - queue->count() + n` and the
 * fifth sets it to `n` flat -- and straight out of `reset` the queue holds
 * exactly `queuePrime`, so the subtraction is zero and the two expressions are
 * the SAME NUMBER.  A fixture that called `progress` once on a fresh modulator
 * could not fail on the difference.  Every case is therefore driven twice, and
 * the second call runs against a queue the first one moved.  Findings F7105 and
 * F7458.
 *
 * THE THREE PHASE 3 EVENT CODES ARE REACHED BY SEEDING THE SUB-MODULATOR, not
 * by running it for two thousand symbols.  `V92Phase3Modulator::generateSymbol`
 * raises 5 leaving SuNot at symbol 24, 7 leaving SuSecondNot at 24 and 8
 * leaving TRN1uSecondEnd past 2039 on a 12-symbol boundary, so the state and
 * the count one short of each are what this grid sets.  Code 8 is the sharpest
 * row: it enters phase 4 MID-BLOCK, and every remaining symbol of that block
 * has to come from the other modulator -- which is what the per-symbol reload
 * of `phase` at .text+0x14e3b buys and what a hoisted test would lose.
 *
 * CODE 9 IS THE PHASE 4 SIDE OF THE SAME THING: `V92Phase4Modulator` writes 9
 * into its `word_0c` leaving B1u at symbol 576, `progress` latches it and the
 * data phase is entered at the end of the block.
 *
 * THE QUEUE'S OWN TWO LIMITS get a row each, and both are asked for with
 * `nSamples == 0` so that the block is empty and the occupancy is exactly what
 * the fixture set it to.  Anything else leaves the count where the resampler's
 * rate conversion happens to put it.
 */
#define PROG_QUEUE_LEN	240
#define PROG_SAMPLES	96u
#define PROG_NBITS	96u

static int prog_bits_a[256], prog_bits_b[256];
static float prog_out_a[PROG_SAMPLES + 16u], prog_out_b[PROG_SAMPLES + 16u];

#define PROG_WIPE	-4000.0f

enum prog_queue { PQ_PRIMED = 0, PQ_EMPTY, PQ_FULL };

struct progcase {
	const char *name;
	int phase;
	int p3state;		/* -1: leave the phase 3 modulator alone */
	unsigned p3count;
	int p4state;		/* -1: leave the phase 4 modulator alone */
	unsigned p4count;
	unsigned nSamples;
	int queue;
};

static const struct progcase progs[] = {
	{ "phase 0, silence",	V92MOD_PHASE_RESET, -1, 0, -1, 0,
	  PROG_SAMPLES, PQ_PRIMED },
	{ "phase 3, Ru",	V92MOD_PHASE_3, V92P3M_STATE_RU, 0, -1, 0,
	  PROG_SAMPLES, PQ_PRIMED },
	{ "phase 3, code 5",	V92MOD_PHASE_3, V92P3M_STATE_SU_NOT, 18u,
	  -1, 0, PROG_SAMPLES, PQ_PRIMED },
	{ "phase 3, code 7",	V92MOD_PHASE_3, V92P3M_STATE_SU_SECOND_NOT, 18u,
	  -1, 0, PROG_SAMPLES, PQ_PRIMED },
	{ "phase 3, code 8",	V92MOD_PHASE_3, V92P3M_STATE_TRN1U_SECOND_END,
	  2046u, -1, 0, PROG_SAMPLES, PQ_PRIMED },
	{ "phase 4, B1u",	V92MOD_PHASE_4, -1, 0, V92P4M_STATE_B1U, 0u,
	  PROG_SAMPLES, PQ_PRIMED },
	{ "phase 4, code 9",	V92MOD_PHASE_4, -1, 0, V92P4M_STATE_B1U, 570u,
	  PROG_SAMPLES, PQ_PRIMED },
	{ "data phase",		V92MOD_PHASE_DATA, -1, 0, -1, 0,
	  PROG_SAMPLES, PQ_PRIMED },
	{ "illegal, positive",	9, -1, 0, -1, 0, PROG_SAMPLES, PQ_PRIMED },
	{ "illegal, negative",	-1, -1, 0, -1, 0, PROG_SAMPLES, PQ_PRIMED },
	{ "queue empty",	V92MOD_PHASE_3, V92P3M_STATE_RU, 0, -1, 0,
	  0u, PQ_EMPTY },
	{ "queue full",		V92MOD_PHASE_3, V92P3M_STATE_RU, 0, -1, 0,
	  0u, PQ_FULL }
};
#define NPROG	((int)(sizeof(progs) / sizeof(progs[0])))

/* The buffers `progress` writes and `compare_obj` cannot see: they are five
 * heap pointers in the poisoned run, so the CONTENTS need their own check. */
static void
compare_buffers(const char *what, long trial)
{
	unsigned bs = M(0)->blockSize + V92MOD_BUF_SLACK;
	unsigned ns = NSAMPLES + V92MOD_BUF_SLACK;

	diff_eq_int("the symbol block agrees (trial %ld)",
		    memcmp(M(0)->buf_7c, M(1)->buf_7c, bs * sizeof(short)) == 0,
		    1, trial);
	diff_eq_int("the resampler input agrees (trial %ld)",
		    memcmp(M(0)->resampleIn, M(1)->resampleIn,
			   bs * sizeof(float)) == 0, 1, trial);
	diff_eq_int("the resampled block agrees (trial %ld)",
		    memcmp(M(0)->resampleOut, M(1)->resampleOut,
			   ns * sizeof(float)) == 0, 1, trial);
	diff_eq_int("the split scratch agrees (trial %ld)",
		    memcmp(M(0)->resampleTail, M(1)->resampleTail,
			   ns * sizeof(float)) == 0, 1, trial);
	diff_eq_int("the scrambled bits agree (trial %ld)",
		    memcmp(M(0)->buf_88, M(1)->buf_88,
			   M(0)->blockSize * 8u) == 0, 1, trial);
	(void)what;
}

static unsigned
queue_count(int s)
{
	return M(s)->queue->count();
}

/* Move the queue to the level the case asks for, and put `queuePrime` with it
 * so that `queuePrime - count() + n` stays inside `blockSize` whatever the
 * level is -- constraint 4 in the file comment, which the object does not
 * bound for itself. */
/*
 * `queuePrime` IS PUT A FIXED DISTANCE ABOVE THE OCCUPANCY AND NOT ON IT, and
 * that distance is what makes two of `progress`'s stores testable at all.
 * `reset` primes the queue to exactly `queuePrime`, so on a fresh modulator
 * `queuePrime - count()` is zero and
 *
 *     blockRemaining = queuePrime - count() + n        (four arms)
 *     blockRemaining = n                               (the silence arm)
 *     blockRemaining = queuePrime - count() + blockSize (the tail)
 *     blockRemaining = blockSize                       (a plausible mutant)
 *
 * are the same two numbers.  Ten apart they are four.  The bias is small
 * enough that `blockRemaining` stays inside `blockSize`, which the object does
 * not bound for itself -- constraint 4 in the file comment.
 */
#define PROG_PRIME_BIAS	10u

static void
set_queue(int s, int how)
{
	V92Modulator *m = M(s);
	float v;

	if (how == PQ_EMPTY) {
		while (m->queue->count() != 0u)
			m->queue->read(&v, 1);
	} else if (how == PQ_FULL) {
		while (m->queue->write(0.0f) == 0)
			;
	}
	m->queuePrime = m->queue->count()
		      + (how == PQ_PRIMED ? PROG_PRIME_BIAS : 0u);
}

static int
run_progress(void)
{
	int pi, ci, trial = 0;
	int sawCode5 = 0, sawCode7 = 0, sawCode8 = 0, sawCode9 = 0;
	int sawSilence = 0, sawData = 0, sawIllegal = 0, sawLimit = 0;
	int sawFilter = 0, sawNoFilter = 0, sawOffPrime = 0, sawLevel2 = 0;

	diff_begin("V92Modulator::progress");

	queue_len_override = PROG_QUEUE_LEN;

	/*
	 * ONE REAL PARAMETER BLOCK, built once and shared.  Zeroed first, so
	 * `K` is zero, both "present" flags are clear and every index in
	 * `indexConstel` selects constellation 0 -- which the creator filled.
	 * `V92BitsToSymbol::reset` is what carries it down to the precoder and
	 * the transmitter, and nothing in `V92Modulator` calls that, so the
	 * fixture does.
	 */
	mp_real = 1;
	memset(arg_mp, 0, CPSZ);
	V92createConstellations((struct V92ParamsInfo *)(void *)arg_mp);
	V92createFilterCoefficients((struct V92ParamsInfo *)(void *)arg_mp);
	{
		struct V92ParamsInfo *mp =
		    (struct V92ParamsInfo *)(void *)arg_mp;
		int k;

		mp->K = 8;
		/*
		 * THE GAIN MUST NOT BE ZERO.  `V92Transmitter::process` ends
		 * `out[n + j] = (short)(shaped[j] * gain)`, so a zeroed
		 * parameter block makes every symbol the chain produces zero
		 * WHATEVER the bits were -- and the data arm then compares
		 * equal however it is mutated.  That is the vacuity of finding
		 * F7105 arriving four objects down.
		 */
		mp->gain = 4096.0f;
		/*
		 * THE TWELVE MODULI MUST NOT BE ZERO.  `V92ModulusEncoder::
		 * reset` copies them straight out of here and `::progress`
		 * divides by the first without a guard -- D700's shape again,
		 * and a zero raises #DE identically on both sides, which
		 * measures the CPU rather than the reading (D571's argument).
		 * `modulosEncoderPresent` being clear does NOT keep the
		 * encoder out of the transmitter's path; it only keeps the
		 * unpacker from filling these.
		 */
		for (k = 0; k < 12; k++)
			mp->m[k] = 8 + k;
		/*
		 * AND THE SIX CONSTELLATION INDICES MUST NOT ALL BE ZERO.
		 * `V92Precoder::process` selects a constellation with
		 * `paramsAt9c[n % 6]`, and index 0 is the smallest of the six
		 * -- small enough that every codeword the modulus encoder
		 * produces lands on the same point, which makes the symbols
		 * the chain emits independent of the bits that went in.  A
		 * data arm compared over constant symbols cannot fail however
		 * it is mutated.
		 */
		for (k = 0; k < V92_PARAMSINFO_CONSTELLATIONS; k++)
			mp->indexConstel[k] = (int)k;

		/*
		 * AND THE SIX CONSTELLATIONS THEMSELVES HAVE TO CARRY POINTS.
		 * `V92createConstellations` ALLOCATES the six 512-byte arrays
		 * and fills none of them -- the unpacker does that -- so out of
		 * the creator each holds the allocator's one repeated byte, and
		 * a constellation whose every point is the same value maps
		 * every codeword to the same symbol.  That is what made the
		 * whole transmit chain insensitive to its own input, and the
		 * check in the data row is what found it.
		 */
		for (k = 0; k < V92_PARAMSINFO_CONSTELLATIONS; k++) {
			unsigned j;

			mp->LC[k] = V92_PARAMSINFO_CONSTELLATION_SZ
				  / sizeof(int) / 2u;
			for (j = 0; j < V92_PARAMSINFO_CONSTELLATION_SZ
					 / sizeof(int); j++)
				mp->constellations[k][j] =
				    (int)((j * 7u + k) % 61u) - 30;
		}
		mp->constellationPresent = 1;
	}

	for (pi = 0; pi < NPROG; pi++) {
	 for (ci = 0; ci < 2; ci++, trial++) {	/* filter off, filter on */
		unsigned int na = 0xa5a5a5a5u, nb = 0xa5a5a5a5u;
		unsigned int i;
		int call, s;
		int lvl2 = (pi == 4 || pi == 8 || pi == 9 || pi == 10
			    || pi == 11);

		filter_override = ci;
		build(trial);
		sane_cp(trial);

		for (s = 0; s < 2; s++) {
			V92Modulator *m = M(s);

			/*
			 * A well-formed phase 3 modulator first -- `enterPhase3`
			 * is the only thing that fills its `jaBits` and
			 * `jaBitCount` -- and then the state the case wants.
			 */
			if (progs[pi].p3state >= 0) {
				if (s == 0)
					our_enterPhase3(ours);
				else
					ref_enterPhase3(theirs);
				m->phase3Modulator->state =
				    (V92Phase3ModulatorState)progs[pi].p3state;
				m->phase3Modulator->symbolCount =
				    progs[pi].p3count;
			}
			if (progs[pi].p4state >= 0) {
				if (s == 0)
					our_enterPhase4(ours);
				else
					ref_enterPhase4(theirs);
				m->phase4Modulator->state = progs[pi].p4state;
				m->phase4Modulator->symbolCount =
				    progs[pi].p4count;
			}

			/*
			 * The bit-to-symbol stage's `bitsPerFrame` is the ONE
			 * field its constructor leaves alone, so out of the
			 * graph it is the allocator's fill -- the same on both
			 * sides, and large enough to make `nofBitsForNextTime`
			 * return a wild count.  Both the data arm and code 9's
			 * tail read it, so it is pinned here.
			 */
			/*
			 * THE SYMBOL BLOCK IS ONE EVERYWHERE BUT THE DATA
			 * PHASE, and that is the object's own arrangement
			 * rather than a fixture convenience.  `initiateRRN`
			 * and `initiateFPE` both call `setSymbolsBlockSize(1)`
			 * on the way into phase 4 and `enterDataPhase` sets it
			 * to `blockSize` on the way into the data phase --
			 * because phase 4 asks the bit-to-symbol stage for ONE
			 * symbol per `generateSymbol` and the data phase asks
			 * for a whole block at once.  With the block size left
			 * at `blockSize`, `V92Phase4Modulator::generateB1u`
			 * hands `bitsToSymbol->process` a `short *` pointing at
			 * one stack slot and is written a hundred symbols into
			 * it.
			 */
			m->bitsToSymbol->reset(m->mappingParams);
			m->bitsToSymbol->setSymbolsBlockSize(
			    progs[pi].phase == V92MOD_PHASE_DATA
			    ? m->blockSize : 1u);

			m->phase = progs[pi].phase;
			m->word_30 = 0xa5a5a5a5u;
			m->word_34 = 0x5a5a5a5au;
			m->resamplerPhaseChange = V92MOD_PHASECHG_NONE;
			m->resamplerPhaseChangeAt = 0u;
			m->resamplerPhaseOffset = 0.125f;
			m->float_28 = 0.0f;

			for (i = 0; i < m->blockSize + V92MOD_BUF_SLACK; i++) {
				m->buf_7c[i] = (short)(0x100 + (int)i);
				m->resampleIn[i] = 0.0f;
			}
			for (i = 0; i < NSAMPLES + V92MOD_BUF_SLACK; i++) {
				m->resampleOut[i] = 0.0f;
				m->resampleTail[i] = 0.0f;
			}
			memset(m->buf_88, 0x5a, m->blockSize * 8u);

			set_queue(s, progs[pi].queue);
		}

		for (i = 0; i < 256u; i++) {
			int v = (int)((lfsr_step() >> 3) & 1u);

			prog_bits_a[i] = v;
			prog_bits_b[i] = v;
		}

		/*
		 * FIVE ROWS RUN AT LEVEL 2, and they are the five that reach a
		 * diagnostic `progress` owns: "Enter phase4" on the code 8
		 * row, "Illegal state" on the two illegal ones and
		 * "Queue is Empty/Full !!!" on the two queue ones.  The level
		 * changes nothing but the messages -- every state comparison
		 * below still runs -- and without it `debugcov.py` counts three
		 * sites in this file that never execute, which is three claims
		 * about the object's text with no trial behind them.
		 *
		 * The TEXT is compared only where the row does not descend into
		 * the transmit chain: the code 8 row reaches `V92CP::
		 * infoToBits`, which prints heap addresses, and the two sides'
		 * modulators are two allocations.  The LINE COUNT compares
		 * everywhere.
		 */
		if (lvl2) {
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			set_level(2);
		}

		for (call = 0; call < 2; call++) {
			for (i = 0; i < PROG_SAMPLES + 16u; i++) {
				prog_out_a[i] = PROG_WIPE;
				prog_out_b[i] = PROG_WIPE;
			}
			na = PROG_NBITS;
			nb = PROG_NBITS;

			our_progress(ours, prog_bits_a, &na, prog_out_a,
				     progs[pi].nSamples);
			ref_progress(theirs, prog_bits_b, &nb, prog_out_b,
				     progs[pi].nSamples);

			diff_eq_int("the bit count agrees (trial %ld)", (int)na,
				    (int)nb, trial);
			diff_eq_int("the samples agree (trial %ld)",
				    memcmp(prog_out_a, prog_out_b,
					   sizeof(prog_out_a)) == 0, 1, trial);
			diff_eq_int("the input words are untouched (trial %ld)",
				    memcmp(prog_bits_a, prog_bits_b,
					   sizeof(prog_bits_a)) == 0, 1, trial);
			compare_all(progs[pi].name, trial);
			compare_buffers(progs[pi].name, trial);
			diff_eq_int("the queue holds the same count (trial %ld)",
				    (int)queue_count(0), (int)queue_count(1),
				    trial);

			if (call == 0 && progs[pi].queue == PQ_PRIMED
			    && progs[pi].nSamples != 0u
			    && queue_count(0) != M(0)->queuePrime)
				sawOffPrime = 1;
		}

		if (lvl2) {
			set_level(0);
			dsplib_debug_capture_on = 0;
			diff_eq_int("the transcripts are the same length "
				    "(trial %ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), trial);
			diff_eq_int("and something was printed (trial %ld)",
				    dsplib_debug_capture_lines(0) > 0u, 1,
				    trial);
			if (pi != 4)
				diff_eq_int("and they say the same thing "
					    "(trial %ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, trial);
			sawLevel2 = 1;
		}

		/* What each row was for, checked absolutely. */
		switch (pi) {
		case 0: {
			/*
			 * The silence arm zeroes `n` symbols, not
			 * `blockRemaining` -- the tail RECOMPUTES that from
			 * `blockSize` before the call returns, so reading it
			 * back afterwards asks about the wrong count.  `n` is
			 * the constructor's own expression over the argument.
			 */
			unsigned int n = (unsigned int)(progs[pi].nSamples
			    * (V92MOD_RATE_NUM / V92MOD_RATE_DEN) + 0.5f);

			sawSilence = 1;
			for (i = 0; i < n; i++)
				if (M(0)->buf_7c[i] != 0)
					break;
			diff_eq_int("phase 0 filled the block with silence "
				    "(trial %ld)", i >= n, 1, trial);
			diff_eq_int("and the seed is still past it (trial %ld)",
				    M(0)->buf_7c[n] != 0, 1, trial);
			diff_eq_int("and reported no bits (trial %ld)", (int)na,
				    0, trial);
			break;
		}
		case 2:
			sawCode5 = 1;
			diff_eq_int("code 5 staged the half-sample change "
				    "(trial %ld)", (int)M(0)->phase3Modulator
							->state,
				    V92P3M_STATE_SU_SECOND, trial);
			break;
		case 3:
			sawCode7 = 1;
			diff_eq_int("code 7 left TRN1uSecond running "
				    "(trial %ld)", (int)M(0)->phase3Modulator
							->state,
				    V92P3M_STATE_TRN1U_SECOND, trial);
			break;
		case 4:
			sawCode8 = 1;
			diff_eq_int("code 8 entered phase 4 (trial %ld)",
				    M(0)->phase, V92MOD_PHASE_4, trial);
			diff_eq_int("and the blob's did too (trial %ld)",
				    M(1)->phase, V92MOD_PHASE_4, trial);
			break;
		case 6:
			sawCode9 = 1;
			diff_eq_int("code 9 entered the data phase (trial %ld)",
				    M(0)->phase, V92MOD_PHASE_DATA, trial);
			diff_eq_int("and the blob's did too (trial %ld)",
				    M(1)->phase, V92MOD_PHASE_DATA, trial);
			break;
		case 7: {
			int varied = 0;

			sawData = 1;
			diff_eq_int("the data arm scrambled something "
				    "(trial %ld)",
				    memcmp(M(0)->buf_88, M(1)->buf_88,
					   PROG_NBITS) == 0, 1, trial);
			/*
			 * AND THE SYMBOLS IT PRODUCED ARE NOT ALL THE SAME.
			 * The chain is four objects deep and every one of them
			 * has a parameter that collapses it -- a zero gain, a
			 * one-point constellation -- so the block coming out
			 * constant is the failure mode this whole row is
			 * exposed to.  Constant symbols compare equal whatever
			 * was fed in.
			 */
			for (i = 1; i < M(0)->blockSize; i++)
				if (M(0)->buf_7c[i] != M(0)->buf_7c[0])
					varied = 1;
			diff_eq_int("and the symbols it made are not all one "
				    "value (trial %ld)", varied, 1, trial);
			break;
		}
		case 8:
		case 9:
			sawIllegal = 1;
			diff_eq_int("an illegal phase is left alone (trial %ld)",
				    M(0)->phase, progs[pi].phase, trial);
			break;
		case 10:
			diff_eq_int("the empty queue raised the limit status "
				    "(trial %ld)", (int)M(0)->word_34,
				    V92MOD_STATUS_QUEUE_LIMIT, trial);
			sawLimit = 1;
			break;
		case 11:
			diff_eq_int("the full queue raised it too (trial %ld)",
				    (int)M(0)->word_34,
				    V92MOD_STATUS_QUEUE_LIMIT, trial);
			sawLimit = 1;
			break;
		default:
			break;
		}

		if (ci)
			sawFilter = 1;
		else
			sawNoFilter = 1;

		teardown();
	 }
	}

	V92deleteConstellations((struct V92ParamsInfo *)(void *)arg_mp);
	V92deleteFilterCoefficients((struct V92ParamsInfo *)(void *)arg_mp);
	queue_len_override = 0;
	filter_override = -1;
	mp_real = 0;

	diff_eq_int("the silence arm was driven", sawSilence, 1, 0);
	diff_eq_int("event code 5 was raised", sawCode5, 1, 0);
	diff_eq_int("event code 7 was raised", sawCode7, 1, 0);
	diff_eq_int("event code 8 was raised", sawCode8, 1, 0);
	diff_eq_int("event code 9 was raised", sawCode9, 1, 0);
	diff_eq_int("the data arm was driven", sawData, 1, 0);
	diff_eq_int("the illegal arm was driven", sawIllegal, 1, 0);
	diff_eq_int("the queue limit was reached", sawLimit, 1, 0);
	diff_eq_int("the shaping filter was applied", sawFilter, 1, 0);
	diff_eq_int("and skipped", sawNoFilter, 1, 0);
	/*
	 * THE OCCUPANCY REALLY MOVED OFF `queuePrime`.  Without this the whole
	 * grid could run with `queuePrime - count()` identically zero, which is
	 * the one number that makes case 0's `blockRemaining = n` and the other
	 * four arms' expression the same store.
	 */
	diff_eq_int("the queue left its primed level", sawOffPrime, 1, 0);
	diff_eq_int("five rows ran at level 2", sawLevel2, 1, 0);
	diff_eq_int("progress trials run", trial, NPROG * 2, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	set_level(0);
	rc |= run_delay();
	rc |= run_reset();
	rc |= run_enter();
	rc |= run_exits();
	rc |= run_mkres();
	rc |= run_initiate();
	rc |= run_progress();
	rc |= run_debug();

	return rc;
}
