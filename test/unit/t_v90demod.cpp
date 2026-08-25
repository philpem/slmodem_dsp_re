/*
 * t_v90demod.cpp -- V90Demodulator::enterPhase3 against the blob.
 *
 * THIS IS A GRAPH TEST WITH A LATCH IN FRONT OF IT.  The method resets six
 * subobjects reached four different ways -- two embedded (`V90PreFilter` at
 * +0x6c, `V90SpectralVerifier` at +0x210), one embedded and called on its base
 * subobject's address (the resampler at +0x94), three through pointers, and
 * two words read out of the parameter block plus one pointer read out of it --
 * so every block is allocated per side, seeded identically with varied bytes,
 * and compared whole.  The object is 0x298 and the slot is bigger.
 *
 * THE THREE VACUITY TRAPS HERE, AND WHAT IS DONE ABOUT EACH:
 *
 *   1. `if (inPhase3 == 1) return;` is the first instruction.  A seeded slot
 *      that happened to hold 1 there makes the whole method a no-op and
 *      everything compares equal for the worst possible reason.  So +0x34 is
 *      set explicitly every trial, the sweep includes 1 (which must return)
 *      and 0, 2 and 0xffffffff (which must not), and `run_latch` requires
 *      both outcomes to have been seen AND requires them to leave DIFFERENT
 *      objects behind.
 *
 *   2. `V90PreFilter::isV90WithEia6()` is called twice with
 *      `setParamEia6()` between the two calls, and `setParamEia6` writes
 *      eighteen words of the block `isV90WithEia6` reads.  The test drives
 *      +0x500 of the parameter block both ways and requires both arms.
 *
 *   3. Three exits: the early one on a negative byte in the block the
 *      parameter block points at, the retrain one on a non-zero
 *      `sessionFlag`, and the plain one.  All three are driven, and the
 *      retrain one is the only thing in wave 2 that writes a value that is
 *      neither 0 nor 1 -- +0x3c becomes 0x20 -- so it is checked by name as
 *      well as by the object comparison.
 *
 * THE FLOAT-BEARING FIELDS ARE SET, NOT SEEDED.  `setTimingOffset` and
 * `V90Equalizer::enterPhase3` do x87 arithmetic on fields of objects this
 * test owns; a random 32-bit pattern is a signalling NaN about one time in
 * 250, and this test is not the place to discover what two different
 * compilations do with one.  Everything else is seeded.
 *
 * THE SIXTEEN BLOCKS NOW LIVE IN test/harness/v90demfix.h, which this file
 * and t_vpcmep3.cpp share.  `setup`, `teardown`, `compare_all`, `set_level`,
 * `D`, `P2`, `P3`, `struct trial_args` and every slot constant come from
 * there; the paragraphs above still describe what they do, because moving
 * them changed none of it.  Everything below is what is specific to
 * `V90Demodulator::enterPhase3`.
 */

#include "v90demfix.h"
/*
 * The designer is a POINTER in V90Demodulator.h, so the fixture needs only the
 * forward declaration; the lifecycle block below sets its `params` and reads
 * its +0x48, so it needs the definition.  That header forward-declares
 * `V90Parameters` and includes nothing, so it cannot collide with the
 * definition V90PreFilter.h has already supplied (finding F1112).
 */
#include "dsplib/V90ConstellationDesigner.h"
/*
 * `getBitRate` dereferences `mappingParamsAlt`, which the shared fixture
 * leaves as a seeded pattern because nothing else in this file follows it.
 * The block below gives it a real one; the definition is needed for that.
 */
#include "dsplib/V90MappingParams.h"

extern "C" {
void ref_enterPhase3(void *self) asm("ref__ZN14V90Demodulator11enterPhase3Ev");
unsigned int ref_dem_getBitRate(const void *self)
    asm("ref__ZNK14V90Demodulator10getBitRateEv");
}


static void
run_pair(int trial, const struct trial_args *t)
{
	setup(trial, t);
	dsplib_debug_capture_reset();
	D(0)->enterPhase3();
	ref_enterPhase3(D(1));
	teardown();
}

static void
transcripts_agree(long tag)
{
	diff_eq_int("transcript line count (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);
	diff_eq_int("transcript text (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
}

/*
 * The latch.  1 returns immediately; every other value, including other
 * non-zero ones, runs the whole method.  Both outcomes are required, and they
 * are required to be DIFFERENT -- otherwise "it returned early" and "it did
 * the work" would be the same observation.
 */
static const unsigned int latch_v[] = {
	0u, 1u, 2u, 0xffffffffu, 0x80000000u
};
#define NLATCH ((int)(sizeof(latch_v) / sizeof(latch_v[0])))

static int
run_latch(void)
{
	static unsigned char early[DEM_SLOT], late[DEM_SLOT];
	int i, sawEarly = 0, sawLate = 0;
	struct trial_args t;

	diff_begin("V90Demodulator::enterPhase3 -- the phase 3 latch");

	dsplib_debug_capture_on = 1;
	set_level(2);

	for (i = 0; i < NLATCH; i++) {
		long tag = i;

		t.latch = latch_v[i];
		t.flag = 0;
		t.eia6 = 6;
		t.blockByte = 0;
		t.pcmType = 0;
		t.idx = i;

		run_pair(11, &t);
		compare_all("after enterPhase3", tag);
		transcripts_agree(tag);

		diff_eq_int("the latch is set (%ld)", (long)D(1)->inPhase3,
			    1, tag);

		if (latch_v[i] == 1u) {
			memcpy(early, dem[1], DEM_SLOT);
			sawEarly = 1;
		} else {
			memcpy(late, dem[1], DEM_SLOT);
			sawLate = 1;
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the early return was taken", sawEarly, 1, 0);
	diff_eq_int("the method ran to the end", sawLate, 1, 0);
	diff_eq_int("returning early is observably different",
		    memcmp(early, late, DEM_SLOT) != 0, 1, 0);

	return diff_end();
}

/*
 * The EIA-6 arm and the two late exits.  `eia6` drives the parameter block's
 * +0x500 both ways, `blockByte` drives the sign that ends the method early,
 * and `flag` drives the V.92 retrain.
 */
static int
run_branches(void)
{
	int e, b, f, lvl, p;
	int sawEia = 0, sawNoEia = 0, sawNeg = 0, sawPos = 0, sawRetrain = 0;
	int printed = 0;
	struct trial_args t;

	diff_begin("V90Demodulator::enterPhase3 -- the arms and the exits");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (e = 0; e < 2; e++) {
			for (b = 0; b < 2; b++) {
				for (f = 0; f < 2; f++) {
					for (p = 0; p < 2; p++) {
						long tag = (long)lvl * 10000 +
						    e * 1000 + b * 100 +
						    f * 10 + p;

						t.latch = 0;
						t.flag = f ? 0x5a5a5a5au : 0u;
						t.eia6 = e ? 6 : 0;
						t.blockByte = b ? -1 : 0x7f;
						t.pcmType = p;
						t.idx = lvl + e + b + f + p;

						run_pair(20 + tag % 7, &t);
						compare_all("after enterPhase3",
							    tag);
						transcripts_agree(tag);
						printed += (int)
						    dsplib_debug_capture_lines(0);

						if (t.eia6 == 6)
							sawEia = 1;
						else
							sawNoEia = 1;
						if (b)
							sawNeg = 1;
						else
							sawPos = 1;
						if (!b && f &&
						    D(1)->word_3c == 0x20)
							sawRetrain = 1;

						/*
						 * The retrain exit is the only
						 * write in wave 2 that is
						 * neither 0 nor 1.
						 */
						diff_eq_int(
						    "+0x3c after the exits (%ld)",
						    (long)D(1)->word_3c,
						    (long)((!b && f) ? 0x20 : 0),
						    tag);
					}
				}
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the EIA-6 arm was taken", sawEia, 1, 0);
	diff_eq_int("the EIA-6 arm was skipped", sawNoEia, 1, 0);
	diff_eq_int("the negative-byte exit was taken", sawNeg, 1, 0);
	diff_eq_int("the negative-byte exit was not taken", sawPos, 1, 0);
	diff_eq_int("the V.92 retrain exit was taken", sawRetrain, 1, 0);
	diff_eq_int("the diagnostics were emitted", printed > 0, 1, 0);

	return diff_end();
}

/*
 * ANTI-VACUITY: the claims the object comparisons above would agree about for
 * the wrong reason if they were not separately shown to be observable.
 */
static int
run_observable(void)
{
	static unsigned char withEia[PARM_SLOT], withoutEia[PARM_SLOT];
	static unsigned char negExit[DEM_SLOT], plainExit[DEM_SLOT];
	struct trial_args t;

	diff_begin("the arms and the exits leave different state behind");

	set_level(0);

	/*
	 * 1.  The EIA-6 arm writes the parameter block; without it the block
	 *     comes back as it went in.  If those two agreed, `setParamEia6`
	 *     could be dropped and nothing above would notice.
	 */
	t.latch = 0;
	t.flag = 0;
	t.blockByte = 0;
	t.pcmType = 0;
	t.idx = 3;

	t.eia6 = 6;
	run_pair(31, &t);
	memcpy(withEia, parm[0], PARM_SLOT);

	t.eia6 = 0;
	run_pair(31, &t);
	memcpy(withoutEia, parm[0], PARM_SLOT);

	diff_eq_int("the EIA-6 arm changes the parameter block",
		    memcmp(withEia, withoutEia, PARM_SLOT) != 0, 1, 0);

	/*
	 * 2.  The timing offset is written only on the EIA-6 arm, and from the
	 *     parameter block rather than from the object.
	 */
	t.eia6 = 6;
	t.idx = 3;
	run_pair(31, &t);
	diff_eq_int("the EIA-6 arm set the timing offset (%ld)",
		    D(0)->resampler.timingOffset != 0.0f, 1, 0);

	/*
	 * 3.  The negative-byte exit really does cut the method short: the
	 *     retrain would otherwise have fired with the same flag.
	 */
	t.eia6 = 6;
	t.flag = 1;
	t.blockByte = -1;
	run_pair(33, &t);
	memcpy(negExit, dem[0], DEM_SLOT);

	t.blockByte = 0;
	run_pair(33, &t);
	memcpy(plainExit, dem[0], DEM_SLOT);

	diff_eq_int("the negative-byte exit skips the retrain",
		    memcmp(negExit, plainExit, DEM_SLOT) != 0, 1, 0);

	/*
	 * 4.  The phase 3 demodulator's +0x410 takes the demodulator's +0x294
	 *     AFTER its reset zeroed it -- so a non-zero +0x294 must survive.
	 */
	t.latch = 0;
	t.flag = 0;
	t.blockByte = 0;
	t.eia6 = 0;
	setup(35, &t);
	D(0)->quickConnect = 0x1234abcdu;
	D(1)->quickConnect = 0x1234abcdu;
	D(0)->enterPhase3();
	ref_enterPhase3(D(1));
	teardown();
	diff_eq_int("+0x294 reached the phase 3 demodulator's +0x410 (%ld)",
		    (long)P3(0)->word_410, (long)0x1234abcdu, 0);
	diff_eq_int("...on the blob's side too (%ld)", (long)P3(1)->word_410,
		    (long)0x1234abcdu, 0);

	return diff_end();
}

/* =============================================== task #88, the lifecycle */

/*
 * `reset`, `reInit` and `enterChannelVerification` reach five sub-objects the
 * `enterPhase3` fixture does not have to wire, and every one of them is
 * reached through a POINTER the two sides would otherwise hold different
 * values of.  The answer is finding F1105's: give both sides THE SAME arena,
 * so every stored pointer agrees and `compare_all`'s raw comparison of
 * `equ[0]` against `equ[1]` and of the two demodulator slots keeps working
 * with nothing excluded -- and then snapshot the arena, run ours, copy the
 * result away, restore, and run the blob's, so that two writers into one
 * buffer do not hide each other (finding F805's shape).
 *
 * THE EMBEDDED RESAMPLER IS WIRED BY OFFSET, and that is not laziness.
 * `V90Demodulator::reset` calls `V90Resampler::reset` on the object at +0x94,
 * whose V90Resampler-only fields start at ITS +0x94; but this file cannot
 * include `V90Resampler.h`, because that header carries the OTHER definition
 * of `V90Parameters` and `V90Demodulator.h` has already supplied the union
 * one (finding F1112).  So the four fields are poked by displacement, with the
 * offsets named against `include/dsplib/V90Resampler.h`'s map.
 */

extern "C" {
void ref_dem_reset(void *self, unsigned int q)
	asm("ref__ZN14V90Demodulator5resetEj");
void ref_dem_reInit(void *self) asm("ref__ZN14V90Demodulator6reInitEv");
void ref_dem_enterChannelVerification(void *self, short a, short b)
	asm("ref__ZN14V90Demodulator24enterChannelVerificationEss");
}

/* V90Resampler's own fields, as displacements inside that sub-object. */
#define VR_BLLSTATE	0x94
#define VR_STATESAMPLES	0x98
#define VR_COUNTSAMPLES	0x9c
#define VR_PARAMS	0xa0
#define VR_TIMINGHIST	0xa4
#define VR_TIMINGLEN	0xa8
#define VR_HISTINDEX	0xac
#define VR_PERIOD	0xb0

#define LIFE_F	64
#define LIFE_S	64
#define LIFE_T	16

struct life_arena {
	float		lecoefs[LIFE_F];
	float		a18[LIFE_F];
	float		lewin[LIFE_F];
	float		dfewin[LIFE_F];
	float		dfecoefs[LIFE_F];
	float		a44[LIFE_F];
	short		lemmx[LIFE_S];
	short		ad8[LIFE_S];
	short		aec[LIFE_S];
	short		dfemmx[LIFE_S];
	short		a118[LIFE_S];
	short		a12c[LIFE_S];
	float		rhist[LIFE_F];		/* Resampler::history      */
	float		thist[LIFE_T];		/* V90Resampler timing     */
	unsigned char	cd[0x54 + 16];		/* the designer            */
	unsigned char	rs[0xb4 + 16];		/* the equaliser's own     */
	unsigned char	dsc[0x60];		/* the demodulator's       */
};

static struct life_arena la, la_save, la_ours;

static void
fill_life(long trial)
{
	unsigned char *p = (unsigned char *)&la;
	unsigned s = 0x71ffu + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < sizeof(la); i++) {
		s = (s >> 1) ^ (-(int)(s & 1u) & 0xb400u);
		p[i] = (unsigned char)((s >> 3) | 1u);
	}
}

static void
poke_ptr(unsigned char *base, int off, void *v)
{
	memcpy(base + off, &v, sizeof v);
}

static void
poke_u32(unsigned char *base, int off, unsigned int v)
{
	memcpy(base + off, &v, sizeof v);
}

static unsigned int
peek_u32(const unsigned char *base, int off)
{
	unsigned int v;

	memcpy(&v, base + off, sizeof v);
	return v;
}

/*
 * Everything the base fixture leaves seeded that a lifecycle member walks
 * into.  Both sides get the same arena; the parameter block stays per-side,
 * because it holds identical bytes and `snap_dem` already accounts for the
 * one pointer inside it.
 */
static void
wire_life(int side, unsigned int lelen, unsigned int m, unsigned int dfelen,
	  int mmx)
{
	V90Demodulator *d = D(side);
	V90Equalizer *e = (V90Equalizer *)equ[side];
	unsigned char *rs = dem[side] + 0x94;	/* the embedded resampler */
	Descrambler<unsigned char, int> *ds = &d->descrambler;

	e->linearEquCoefs = la.lecoefs;
	e->array_18 = la.a18;
	e->linearEquWindow = la.lewin;
	e->dfeWindow = la.dfewin;
	e->dfeCoefs = la.dfecoefs;
	e->array_44 = la.a44;
	e->linearEquMmxCoefs = la.lemmx;
	e->array_d8 = la.ad8;
	e->array_ec = la.aec;
	e->dfeMmxCoefs = la.dfemmx;
	e->array_118 = la.a118;
	e->array_12c = la.a12c;
	e->linearEquLength = lelen;
	e->word_1c = m;
	e->dfeLength = dfelen;
	e->mmxArraysPresent = mmx;
	e->params = (V90Parameters *)parm[0];	/* shared: read only */
	e->resampler = (V90Resampler *)la.rs;

	/* The equaliser's own resampler, for enterChannelVerification. */
	poke_ptr(la.rs, VR_PARAMS, parm[0]);
	poke_u32(la.rs, VR_BLLSTATE, 3u);	/* not PRE_ANSPCM */
	poke_u32(la.rs, VR_STATESAMPLES, 0x11223344u);
	poke_u32(la.rs, VR_COUNTSAMPLES, 0x55667788u);

	/* The designer, shared, so its pointer agrees on both sides. */
	d->constellationDesigner = (V90ConstellationDesigner *)la.cd;
	((V90ConstellationDesigner *)la.cd)->params = (V90Parameters *)parm[0];

	/* The connection evaluator the fixture already allocates per side. */
	((V90ConnectionEvaluator *)ce[side])->params = (V90Parameters *)parm[0];

	/*
	 * The demodulator's OWN embedded resampler.  `Resampler::reset` walks
	 * `history` when it is not null and `V90Resampler::reset` walks
	 * `timingHistory`; both buffers are shared, and both are inside the
	 * snapshot.
	 */
	d->resampler.history = la.rhist;
	d->resampler.historyLen = LIFE_F;
	d->resampler.taps = 4;
	d->resampler.phases = 8;
	d->resampler.coeffs = NULL;
	d->resampler.coeffsBorrowed = 1;
	poke_ptr(rs, VR_PARAMS, parm[0]);
	poke_ptr(rs, VR_TIMINGHIST, la.thist);
	poke_u32(rs, VR_TIMINGLEN, LIFE_T);
	poke_u32(rs, VR_BLLSTATE, 3u);
	poke_u32(rs, VR_HISTINDEX, 5u);
	poke_u32(rs, VR_PERIOD, 7u);

	/* The demodulator's embedded descrambler, shared like the rest. */
	ds->pLimit = la.dsc;
	ds->pInitOut = la.dsc + 0x20;
	ds->pInitTap1 = la.dsc + 0x30;
	ds->pInitTap2 = la.dsc + 0x38;
	ds->pOut = la.dsc + 4;
	ds->pTap1 = la.dsc + 0x14;
	ds->pTap2 = la.dsc + 0x1c;
	ds->tailLength = 8;
}

static void
life_setup(int trial, const struct trial_args *t, unsigned int lelen,
	   unsigned int m, unsigned int dfelen, int mmx)
{
	setup(trial, t);
	fill_life(trial + 4242);
	wire_life(0, lelen, m, dfelen, mmx);
	wire_life(1, lelen, m, dfelen, mmx);
}

static void
life_compare(const char *what, long tag)
{
	compare_all(what, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the shared arena",
		     &la_ours, &la, sizeof(la), tag);
	transcripts_agree(tag);
}

static int
run_reinit(void)
{
	struct trial_args t;
	int trial, printed = 0;
	unsigned lvl;

	diff_begin("V90Demodulator::reInit");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level(lvl);
		for (trial = 0; trial < 6; trial++) {
			long tag = (long)lvl * 100 + trial;
			unsigned int was;

			t.latch = 0;
			t.flag = 0;
			t.eia6 = 6;
			t.blockByte = 0;
			t.pcmType = 0;
			t.idx = trial;

			life_setup(trial + 300, &t, 8, 16, 6, trial & 1);
			P3(0)->verificationStatus =
			    P3(1)->verificationStatus = 0xdeadbe00u + trial;
			was = P3(1)->verificationStatus;

			memcpy(&la_save, &la, sizeof(la));
			dsplib_debug_capture_reset();

			D(0)->reInit();

			memcpy(&la_ours, &la, sizeof(la));
			memcpy(&la, &la_save, sizeof(la));

			ref_dem_reInit(D(1));

			life_compare("after reInit", tag);

			/*
			 * The two calls really happened: the evaluator was
			 * configured out of the parameter block and the
			 * verification status went to zero from something
			 * else.
			 */
			diff_eq_int("the evaluator was reset (%ld)",
				    ((V90ConnectionEvaluator *)ce[1])->word_64,
				    1600, tag);
			diff_eq_int("verificationStatus cleared (%ld)",
				    (long)P3(1)->verificationStatus, 0, tag);
			diff_eq_int("and it had held something else (%ld)",
				    was != 0, 1, tag);

			if (lvl > 1) {
				diff_eq_int("the diagnostic was reached (%ld)",
					    dsplib_debug_capture_lines(1) > 0,
					    1, tag);
				printed = 1;
			}

			teardown();
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);
	diff_eq_int("reInit printed at level 2", printed, 1, 0);

	return diff_end();
}

static int
run_reset(void)
{
	static const unsigned int len_v[] = { 0u, 4u, 8u };
	static const unsigned int cur_v[] = { 0u, 3u };
	static const int cursor_param[] = { -1, 0, 2, 0x7fffffff };
	/*
	 * THE BAUD OFFSET, AND THE THIRD ONE IS THE POINT.  `reset` prints
	 * `PARAMS_TIMING_OFFSET` UNGATED as `%c%d.%03d`, and the object builds
	 * the sign character branchlessly -- `fldz; fcomps 0x84(%ebx); sahf;
	 * sbb %eax,%eax; and $0xfffffffe,%eax; add $0x2d,%eax` at 0x1c0dc,
	 * which is `0x2d - 2*CF` with the ZERO in %st(0).  FCOM sets CF for
	 * less-than AND for unordered, so an unordered offset prints '+' where
	 * `(0.0f < offset)` prints '-'; on every ordered value the two agree,
	 * so a finite-only sweep cannot tell them apart.  The shared fixture's
	 * `ppm_v` is deliberately all finite (t_vpcmep3.cpp shares it), so the
	 * NaN is planted HERE, per trial, after `life_setup` -- the same way
	 * the cursor parameter is.  Findings F2300 and F2410.
	 */
	static const unsigned int off_bits[] = {
		0x3f800000u,	/*   1.0f          */
		0xc1480000u,	/* -12.5f          */
		0x7fc00000u	/* a quiet NaN     */
	};
	struct trial_args t;
	long tag = 90000;
	int li, ci, cp, mmx, q, ob;
	int saw_derived = 0, saw_configured = 0, saw_nan_offset = 0;
	unsigned lvl;

	diff_begin("V90Demodulator::reset");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level(lvl);
		for (li = 0; li < 3; li++)
		    for (ci = 0; ci < 2; ci++)
			for (cp = 0; cp < 4; cp++)
			    for (mmx = 0; mmx < 2; mmx++)
				for (ob = 0; ob < 3; ob++)
				for (q = 0; q < 2; q++) {
					unsigned int quick =
					    q ? 0x5a5a1234u : 0u;
					float offset;

					tag++;
					t.latch = 0;
					t.flag = 0;
					t.eia6 = 6;
					t.blockByte = 0;
					t.pcmType = 0;
					t.idx = li + ci + cp;

					life_setup((int)tag, &t, len_v[li], 16,
						   4u + cur_v[ci], mmx);
					set_int(0, 0x184, cursor_param[cp]);
					set_int(1, 0x184, cursor_param[cp]);
					memcpy(&offset, &off_bits[ob],
					       sizeof offset);
					set_float(0, 0x84, offset);
					set_float(1, 0x84, offset);
					if (diff_isnan_f(offset))
						saw_nan_offset = 1;

					memcpy(&la_save, &la, sizeof(la));
					dsplib_debug_capture_reset();

					D(0)->reset(quick);

					memcpy(&la_ours, &la, sizeof(la));
					memcpy(&la, &la_save, sizeof(la));

					ref_dem_reset(D(1), quick);

					life_compare("after reset", tag);

					/* The fill is gone, everywhere. */
					diff_eq_int("inPhase3 (%ld)",
						    (long)D(1)->inPhase3, 0,
						    tag);
					diff_eq_int("word_288 (%ld)",
						    (long)D(1)->word_288,
						    19200, tag);
					diff_eq_int("word_290 (%ld)",
						    (long)D(1)->word_290,
						    19200, tag);
					diff_eq_int("byte_280 (%ld)",
						    (long)D(1)->byte_280, 0,
						    tag);
					diff_eq_int("+0x294 took the quick "
						    "connect argument (%ld)",
						    (long)D(1)->quickConnect,
						    (long)quick, tag);
					/*
					 * The store into ANOTHER object, and
					 * the reason V90Equalizer is 0x150
					 * bytes (finding F1107).
					 */
					diff_eq_int("the equaliser's +0x148 "
						    "(%ld)",
						    (long)((V90Equalizer *)
							   equ[1])
						    ->quickConnect,
						    (long)quick, tag);
					/* The AGC was reconfigured after it
					 * was reset. */
					diff_eq_int("agc.blockLen (%ld)",
						    (long)D(1)->agc.blockLen,
						    (long)(unsigned int)
						    *(int *)&parm[1][0x64],
						    tag);
					/* The designer ran. */
					diff_eq_int("the designer's +0x48 "
						    "(%ld)",
						    (long)((V90ConstellationDesigner *)
							   la.cd)->word_48, 0,
						    tag);
					/* And the resampler did. */
					diff_eq_int("the resampler's period "
						    "(%ld)",
						    (long)peek_u32(dem[1]
								   + 0x94,
								   VR_PERIOD),
						    0, tag);

					/*
					 * The equaliser's cursor: derived from
					 * its own length when the parameter is
					 * negative, configured otherwise.
					 */
					if (cursor_param[cp] < 0)
						saw_derived = 1;
					else
						saw_configured = 1;

					teardown();
				}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);
	diff_eq_int("the cursor was derived somewhere", saw_derived, 1, 0);
	diff_eq_int("and configured somewhere", saw_configured, 1, 0);
	diff_eq_int("an unordered baud offset was printed", saw_nan_offset,
		    1, 0);

	return diff_end();
}

static int
run_enterchannelverification(void)
{
	static const short arg_v[] = { 0, 1, -1, 0x1234, -0x4000 };
	struct trial_args t;
	long tag = 95000;
	int a, b;
	unsigned lvl;

	diff_begin("V90Demodulator::enterChannelVerification");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level(lvl);
		for (a = 0; a < 5; a++)
			for (b = 0; b < 5; b++) {
				tag++;
				t.latch = 0;
				t.flag = 0;
				t.eia6 = 6;
				t.blockByte = 0;
				t.pcmType = (a + b) & 1;
				t.idx = a + b;

				life_setup((int)tag, &t, 8, 16, 8, (a + b) & 1);
				set_int(0, 0x184, 2);
				set_int(1, 0x184, 2);

				memcpy(&la_save, &la, sizeof(la));
				dsplib_debug_capture_reset();

				D(0)->enterChannelVerification(arg_v[a],
							       arg_v[b]);

				memcpy(&la_ours, &la, sizeof(la));
				memcpy(&la, &la_save, sizeof(la));

				ref_dem_enterChannelVerification(D(1),
								 arg_v[a],
								 arg_v[b]);

				life_compare("after enterChannelVerification",
					     tag);

				diff_eq_int("inPhase3 is 5 (%ld)",
					    (long)D(1)->inPhase3, 5, tag);
				diff_eq_int("word_38 (%ld)",
					    (long)D(1)->word_38, 0, tag);
				diff_eq_int("word_40 (%ld)",
					    (long)D(1)->word_40, 0, tag);
				/*
				 * The SECOND argument is the one that gets
				 * through, as the phase 3 demodulator's
				 * +0x414; the first is never loaded.
				 */
				diff_eq_int("short414 is the second argument "
					    "(%ld)",
					    (long)P3(1)->short_414,
					    (long)arg_v[b], tag);
				diff_eq_int("the phase 3 state is WaitForQTS "
					    "(%ld)",
					    (long)P3(1)->state,
					    (long)P3D_STATE_WAIT_FOR_QTS, tag);
				diff_eq_int("verificationStatus cleared (%ld)",
					    (long)P3(1)->verificationStatus, 0,
					    tag);
				/* The equaliser really entered verification. */
				diff_eq_int("the equaliser's state (%ld)",
					    (long)((V90Equalizer *)equ[1])
					    ->state,
					    V90EQU_STATE_CHANNEL_VERIFY, tag);
				diff_eq_int("the equaliser's resampler (%ld)",
					    (long)peek_u32(la.rs,
							   VR_BLLSTATE), 11,
					    tag);

				teardown();
			}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * getBitRate, and this block DELIBERATELY DOES NOT USE THE SHARED FIXTURE.
 *
 * The method reads two things -- a byte at +0x280 and one word through
 * `mappingParamsAlt` -- calls nothing, and writes nothing.  `setup()` would
 * bring sixteen blocks, a `fir_ctor` allocation and a `teardown` with it for
 * two loads, and it would need `snap_dem` taught to neutralise a pointer it
 * neutralises for nobody else; both t_v90demod.cpp and t_vpcmep3.cpp share
 * that header, so the cheaper change is the one that stays local.  Two slots
 * of the right size, `fill_pair` for identical seeds, and one hole.
 *
 * THE THREE THINGS THIS HAS TO SEPARATE, none of which a sampled input does:
 *
 *   1. `cmpb $0x0` is ANY-non-zero.  0x80 and 0xff take the computing arm,
 *      and a `signed char > 0` reading would return zero for both.
 *   2. THE MULTIPLICAND IS UNSIGNED.  `word_0 * 8000` crosses 2^31 at
 *      word_0 = 268435, and only above that do `fildll`-with-zero-high and
 *      `fildl` disagree -- by 2^32/6, which is not subtle.  268434, 268435
 *      and 268436 are in the sweep for that, plus 0x80000000 and 0xffffffff.
 *      This is finding F613's case: below the crossing the two readings agree
 *      over every value, so a sweep that stops at plausible rates tests
 *      nothing about the type.
 *   3. THE `+ 0.5f` IS A ROUNDING AND NOT DECORATION.  8000 mod 6 is 2, so
 *      the exact quotient's fraction is 0, 1/3 or 2/3 as word_0 is 0, 1 or 2
 *      mod 3 -- and only the 2-mod-3 case rounds UP.  Dropping the constant
 *      changes nothing at all for a third of the inputs and one count for
 *      another third, so both residues are present at several magnitudes.
 *
 * The object is compared with the four bytes of `mappingParamsAlt` held out,
 * because they are the one field this block gives two different values.
 */
static unsigned char gbr_dem[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char gbr_mpa[2][32] __attribute__((aligned(8)));
/*
 * AND A SECOND MAPPING BLOCK, ON THE FIELD NEXT DOOR.  `mappingParams` at
 * +0x14 and `mappingParamsAlt` at +0x18 are adjacent pointers of the same
 * type; the object loads +0x18.  Left as the seeded pattern the two would be
 * separated only by a segmentation fault, which is a crash and not a
 * diagnosis, so +0x14 gets a real block holding a DIFFERENT count and reading
 * the wrong one is a wrong number instead.
 */
static unsigned char gbr_mp0[2][32] __attribute__((aligned(8)));

/*
 * Where the two held-out pointers are.  They are adjacent, so one hole of two
 * words covers both; the assert is what says they still are.
 */
#define GBR_HOLE ((unsigned)__builtin_offsetof(V90Demodulator, mappingParams))
typedef char gbr_holes_adjacent[
    ((int)__builtin_offsetof(V90Demodulator, mappingParamsAlt)
     == (int)__builtin_offsetof(V90Demodulator, mappingParams)
	+ (int)sizeof(void *)) ? 1 : -1];

static int
run_getbitrate(void)
{
	/*
	 * 21, 22 and 23 are 28000, 29333 and 30667 -- the bottom three V.90
	 * downstream rates -- and they are also 0, 1 and 2 mod 3, so the
	 * three roundings appear at a real rate as well as at the extremes.
	 */
	static const unsigned int nbits[] = {
		0u, 1u, 2u, 3u, 4u, 5u,
		21u, 22u, 23u, 41u, 42u, 43u,
		268434u, 268435u, 268436u,
		0x7ffffffeu, 0x7fffffffu, 0x80000000u,
		0xfffffffdu, 0xfffffffeu, 0xffffffffu
	};
	/*
	 * AND THE ANSWER, COMPUTED BY HAND, for the twelve inputs a V.90
	 * session could plausibly hold.  Agreement with the blob is the
	 * oracle; this is the second, independent one, and it is what makes
	 * the `+ 0.5f` a checked claim rather than a shared assumption -- the
	 * four values that are 1 mod 3 round DOWN and the four that are 2 mod
	 * 3 round UP, and a version without the constant gets the second four
	 * wrong by one and everything else right.  -1 means "swept for
	 * agreement only": above 268435 the product wraps and the arithmetic
	 * stops being a rate.
	 */
	static const long want_bps[] = {
		0, 1333, 2667, 4000, 5333, 6667,
		28000, 29333, 30667, 54667, 56000, 57333,
		-1, -1, -1, -1, -1, -1, -1, -1, -1
	};
	static const unsigned char flag[] = { 0, 1, 2, 0x7f, 0x80, 0xff };
	unsigned i, f;
	long tag = 96000;
	int saw_zero = 0, saw_rate = 0;

	diff_begin("V90Demodulator::getBitRate");

	for (f = 0; f < sizeof(flag) / sizeof(flag[0]); f++)
	for (i = 0; i < sizeof(nbits) / sizeof(nbits[0]); i++) {
		static unsigned char a[DEM_SLOT], b[DEM_SLOT];
		unsigned int got, want;
		int side;

		tag++;
		lfsr_state = 0x51edu + 0x9e37u * (unsigned)tag;
		fill_pair(gbr_dem[0], gbr_dem[1], DEM_SLOT);
		fill_pair(gbr_mpa[0], gbr_mpa[1], sizeof(gbr_mpa[0]));
		fill_pair(gbr_mp0[0], gbr_mp0[1], sizeof(gbr_mp0[0]));

		for (side = 0; side < 2; side++) {
			V90Demodulator *d = (V90Demodulator *)gbr_dem[side];
			V90MappingParams *mp =
			    (V90MappingParams *)gbr_mpa[side];
			V90MappingParams *mp0 =
			    (V90MappingParams *)gbr_mp0[side];

			d->mappingParams = mp0;
			d->mappingParamsAlt = mp;
			d->byte_280 = flag[f];
			mp->word_0 = nbits[i];
			/*
			 * Never equal to the one next door, and never zero:
			 * either would let a read of +0x14 pass.
			 */
			mp0->word_0 = nbits[i] + 6u + (i & 1u);
		}

		got = ((const V90Demodulator *)gbr_dem[0])->getBitRate();
		want = ref_dem_getBitRate(gbr_dem[1]);

		diff_eq_int("getBitRate (%ld)", (long)got, (long)want, tag);

		/*
		 * AND IT IS THE ARITHMETIC, not merely agreement.  Two
		 * versions that both returned zero would pass the line above
		 * for every case; this records that the computing arm was
		 * reached at all and that the gate really gates.
		 */
		if (flag[f] == 0) {
			diff_eq_int("...the gate returns zero (%ld)",
				    (long)want, 0, tag);
			saw_zero = 1;
		} else if (want_bps[i] >= 0) {
			diff_eq_int("...and an open gate gives the rate "
				    "computed by hand (%ld)",
				    (long)want, want_bps[i], tag);
			if (want_bps[i] != 0)
				saw_rate = 1;
		} else if (nbits[i] == 0x80000000u) {
			/*
			 * 2^31 * 8000 is 2^37 * 125, so the low 32 bits are
			 * ZERO and an open gate answers zero.  The multiply
			 * really is a wrapping 32-bit one on both sides, and
			 * this is the case that says so by name -- a version
			 * that widened it before multiplying would answer
			 * 2863311530 here and agree everywhere else in this
			 * sweep except the last three entries.
			 */
			diff_eq_int("...2^31 bits wraps the product to zero "
				    "(%ld)", (long)want, 0, tag);
		}

		/* Neither side may write anything, including the pointer. */
		memcpy(a, gbr_dem[0], DEM_SLOT);
		memcpy(b, gbr_dem[1], DEM_SLOT);
		memset(a + GBR_HOLE, 0, 2 * sizeof(void *));
		memset(b + GBR_HOLE, 0, 2 * sizeof(void *));
		diff_eq_obj_(__FILE__, __LINE__, "getBitRate writes nothing",
			     "V90Demodulator slot", a, b, DEM_SLOT, tag);
		diff_eq_obj_(__FILE__, __LINE__,
			     "getBitRate leaves the mapping block alone",
			     "V90MappingParams head",
			     gbr_mpa[0], gbr_mpa[1], sizeof(gbr_mpa[0]), tag);
		diff_eq_obj_(__FILE__, __LINE__,
			     "...and the one next door", "V90MappingParams +14",
			     gbr_mp0[0], gbr_mp0[1], sizeof(gbr_mp0[0]), tag);
	}

	/*
	 * The anti-vacuity pair for the sweep itself: a fixture that never
	 * reached one of the two arms would report a clean run.
	 */
	diff_eq_int("the zero arm was reached (%ld)", (long)saw_zero, 1, tag);
	diff_eq_int("the computing arm was reached (%ld)", (long)saw_rate, 1,
		    tag);

	return diff_end();
}

/* ------------------------------------ V90Demodulator::sessionTermination */

/*
 * WHAT THIS METHOD DOES THAT NOTHING ELSE IN THIS FILE DOES: it reads two x87
 * summaries of the resampler's timing history, prints three numbers built out
 * of them, and on one arm writes a fourth into a block reached through TWO
 * pointers.  So the fixture has to supply a timing history, and the checks
 * have to reach the block `V90PW(params)[0]` points at -- which `compare_all`
 * already compares, because `enterPhase3` reads a byte of it.
 *
 * THE HISTORY IS THE FIXTURE'S ONE ADDITION, and it is made here rather than
 * in v90demfix.h so that t_vpcmep3.cpp is not perturbed.  `V90Resampler`'s
 * `timingHistory` and `timingHistoryLen` are its +0xa4 and +0xa8
 * (V90Resampler.h); the resampler is embedded at V90Demodulator+0x94, and
 * neither field is modelled -- both fall in `pad_e0`.  They are written by
 * absolute offset, asserted UNCHANGED after the call, and only then made
 * equal on the two sides, so normalising them cannot hide a store.
 *
 * THE HISTORY VALUES ARE CHOSEN, NOT SEEDED, and the choosing is the test.
 * `%c%d.%04d` degenerates on any value with an exact binary fraction: the
 * sign character, the `abs()` and the four decimals all read the same for a
 * right and a wrong spelling.  So the patterns below cover a positive and a
 * negative mean, a mean of exactly zero (which prints '-', because the object
 * asks `0.0f < x` and not `<=`), fractions that are not exact in binary, a
 * CONSTANT history so that `std` is exactly zero, and magnitudes either side
 * of 1.  `timingHistoryLen` is never zero: `mean` divides by it.
 *
 * AND A NaN HISTORY, WHICH IS THE ONLY THING THAT SEPARATES THE TWO SIGN
 * SPELLINGS.  The object builds the character branchlessly -- `fldz; fcomps
 * mean; sahf; sbb %eax,%eax; and $0xfffffffe,%eax; add $0x2d,%eax` at 0x1ac3a
 * -- which is `0x2d - 2*CF` with the ZERO in %st(0), and FCOM sets CF for
 * less-than AND for unordered, so a NaN prints '+'.  `(0.0f < mean)` prints
 * '-' for it and agrees on every other value there is, so without this
 * pattern the two spellings are indistinguishable and the sweep proves
 * nothing about the site.  `sawNan` below requires it to have been printed.
 * Findings F2300 and F2410.
 *
 * NOTHING ELSE IN THE FUNCTION FORKS ON IT.  The saving arm is
 * `V90PF(params)[MIN_STD_FOR_SAVE] >= std`, the object's `flds thresh;
 * fcomps std; jb` with the threshold on the left, so an unordered compare
 * sets CF, `jb` is taken and the arm is REFUSED -- which is what C says too,
 * so the predicted arm stays right.  The magnitude and the fraction both go
 * through `cvttss`-style truncation of a NaN and land on the indefinite
 * integer on both sides alike.
 *
 * D72's NEIGHBOUR IS NOT HERE.  This method only reads the history; the
 * echo canceller's unclamped clear is t_v90leaves.cpp's.
 */

#define RS_HIST		(0x094 + 0x0a4)	/* V90Resampler::timingHistory    */
#define RS_HLEN		(0x094 + 0x0a8)	/* V90Resampler::timingHistoryLen */
#define PARAMS_EVAL	0x160		/* TIMING_HISTORY_EVALUATION_ENABLED */
#define PARAMS_MINSTD	0x16c		/* TIMING_OFFESET_MIN_STD_FOR_SAVE  */
#define BLK_DEVIATION	0x4c		/* _tagModemParameters, thousandths */

#define ST_HIST		24

static float st_hist[2][ST_HIST];

extern "C" {
int ref_sessionTermination(void *self)
	asm("ref__ZN14V90Demodulator18sessionTerminationEv");
/*
 * The two summaries, called by this test on the BLOB's resampler and BEFORE
 * the method runs, so that the value stored in the registry word and the arm
 * that stores it can both be predicted without reference to the source under
 * test.  Neither reads anything `sessionTermination` writes.
 */
float ref_timingHistoryMean(void *self)
	asm("ref__ZN12V90Resampler20getTimingHistoryMeanEv");
float ref_timingHistoryStd(void *self)
	asm("ref__ZN12V90Resampler19getTimingHistoryStdEv");
}

/* Sixth pattern is constant, so `std` is exactly zero and prints '-'. */
static float
st_value(int pattern, int i)
{
	switch (pattern) {
	case 0:
		return 1.0f / 3.0f + 0.01f * (float)i;	/* +, inexact   */
	case 1:
		return -1.0f / 7.0f - 0.013f * (float)i;/* -, inexact   */
	case 2:
		return (i & 1) ? 2.5f : -2.5f;		/* mean 0        */
	case 3:
		return 123.4567f + 0.9f * (float)i;	/* magnitude > 1 */
	case 4:
		return 0.00009f * (float)(i + 1);	/* tiny          */
	case 5:
		return 7.25f;				/* std == 0      */
	case 6:
		return -0.99995f;			/* rounds at 1e-4 */
	case 7:
		return (float)(i - ST_HIST / 2) * 0.3125f;
	case 8:
		return 0.0f;		/* mean AND std exactly zero    */
	default: {
		/*
		 * A quiet NaN, so `mean` and `std` are both unordered and the
		 * sign character is the only thing that can disagree.  Built
		 * from the bits rather than written as a literal: 2303 folded
		 * a self-comparison away here, and a computed 0.0f/0.0f would
		 * be folded too.
		 */
		float q;
		unsigned int b = 0x7fc00000u;

		memcpy(&q, &b, sizeof q);
		return q;
	}
	}
}

#define ST_PATTERNS	10

static int
run_sessterm(void)
{
	static const unsigned int st_len[] = { 1u, 2u, 7u, ST_HIST };
	static const float st_minstd[] = { -1.0f, 0.0f, 0.5f, 1.0e9f };
	static const unsigned int st_state[] = { 3u, 0u, 1u, 5u };
	struct trial_args t;
	int lvl, pat, li, mi, si, ei, gi, ii;
	int sawSaved = 0, sawRefused = 0, sawDisabled = 0, sawElse = 0;
	int sawPlus = 0, sawMinus = 0, sawFrac = 0, sawNan = 0;

	diff_begin("V90Demodulator::sessionTermination");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level((unsigned int)lvl);

		/*
		 * EIA-6 IS ITS OWN DIMENSION, and it has to be.  It was
		 * derived from a bit of the trial index that the history
		 * length also uses, which correlated the two: every trial
		 * with an even number of samples was also an EIA-6 trial and
		 * therefore took the refusing arm, so the alternating pattern
		 * -- the only one whose mean is exactly zero -- never reached
		 * the printer.  `(0.0f < mean)` and `(0.0f <= mean)` then
		 * agreed on every value the sweep produced and the mutation
		 * that swaps them survived.  Two correlated knobs are one
		 * knob; findings F223 and F224 are about exactly this.
		 */
		for (ii = 0; ii < ST_PATTERNS * 4 * 4 * 4 * 2 * 2; ii++) {
			static unsigned char ba[BLK_SLOT], bb[BLK_SLOT];
			void *hp[2];
			unsigned int hl[2];
			long tag = (long)lvl * 100000 + ii;
			int side, ours, theirs, k;
			unsigned int n;
			float minstd, rmean, rstd;
			unsigned int state;
			int eval, mainArm, saveArm;

			pat = ii % ST_PATTERNS;
			li = (ii / ST_PATTERNS) % 4;
			mi = (ii / (ST_PATTERNS * 4)) % 4;
			si = (ii / (ST_PATTERNS * 4 * 4)) % 4;
			ei = (ii / (ST_PATTERNS * 4 * 4 * 4)) % 2;
			gi = (ii / (ST_PATTERNS * 4 * 4 * 4 * 2)) % 2;

			n = st_len[li];
			minstd = st_minstd[mi];
			state = st_state[si];
			eval = ei;

			t.latch = state;
			t.flag = 0;
			t.eia6 = gi ? 6 : 0;
			t.blockByte = 0;
			t.pcmType = 0;
			t.idx = ii;

			setup(ii, &t);

			for (side = 0; side < 2; side++) {
				unsigned int i;

				for (i = 0; i < ST_HIST; i++)
					st_hist[side][i] =
					    st_value(pat, (int)i);

				hp[side] = st_hist[side];
				hl[side] = n;
				memcpy(&dem[side][RS_HIST], &hp[side],
				       sizeof(void *));
				memcpy(&dem[side][RS_HLEN], &n, sizeof n);
				set_int(side, PARAMS_EVAL, eval);
				set_float(side, PARAMS_MINSTD, minstd);
			}

			memcpy(ba, blk[0], BLK_SLOT);
			memcpy(bb, blk[1], BLK_SLOT);

			/*
			 * Predicted before the call, off the blob's own
			 * summaries of the blob's own history.
			 */
			rmean = ref_timingHistoryMean(&dem[1][0x94]);
			rstd = ref_timingHistoryStd(&dem[1][0x94]);
			mainArm = (state == 3u && t.eia6 != 6);
			saveArm = mainArm && eval && (minstd >= rstd);

			dsplib_debug_capture_reset();

			ours = D(0)->sessionTermination();
			theirs = ref_sessionTermination(D(1));

			teardown();

			diff_eq_int("the return value (%ld)", (long)ours,
				    (long)theirs, tag);
			diff_eq_int("the blob returned zero (%ld)",
				    (long)theirs, 0, tag);

			/*
			 * Neither the history pointer nor its length is
			 * written; asserted before they are made equal, so
			 * the normalisation below cannot mask a store.
			 */
			for (side = 0; side < 2; side++) {
				diff_eq_int("timingHistory is untouched "
					    "(%ld)",
					    memcmp(&dem[side][RS_HIST],
						   &hp[side],
						   sizeof(void *)) == 0,
					    1, tag * 10 + side);
				diff_eq_int("timingHistoryLen is untouched "
					    "(%ld)",
					    memcmp(&dem[side][RS_HLEN], &hl[side],
						   sizeof hl[0]) == 0,
					    1, tag * 10 + side);
				memset(&dem[side][RS_HIST], 0,
				       sizeof(void *));
			}

			compare_all("after sessionTermination", tag);
			transcripts_agree(tag);

			diff_eq_obj_(__FILE__, __LINE__,
				     "after sessionTermination",
				     "the timing history itself",
				     st_hist[0], st_hist[1],
				     sizeof(st_hist[0]), tag);

			/*
			 * The ONE store the method makes, predicted rather
			 * than read back: the deviation in thousandths, and
			 * nothing else in the block, on exactly the arm the
			 * three gates and the threshold select.
			 */
			{
				int outside = 0, want, got;

				for (k = 0; k < BLK_SLOT; k++) {
					if (k >= BLK_DEVIATION
					    && k < BLK_DEVIATION + 4)
						continue;
					if (bb[k] != blk[1][k])
						outside = 1;
				}
				diff_eq_int("the blob changed nothing but "
					    "+0x4c of the block (%ld)",
					    outside, 0, tag);

				memcpy(&got, &blk[1][BLK_DEVIATION],
				       sizeof got);
				if (saveArm) {
					want = (int)(1000.0f * rmean);
					sawSaved = 1;
				} else {
					memcpy(&want, &bb[BLK_DEVIATION],
					       sizeof want);
					if (mainArm && eval)
						sawRefused = 1;
				}
				diff_eq_int("the saved ClockDeviation (%ld)",
					    (long)got, (long)want, tag);

				if (mainArm && !eval)
					sawDisabled = 1;
				if (!mainArm)
					sawElse = 1;
			}

			/*
			 * The sign character and the fraction are what the
			 * `%c%d.%04d` idiom degenerates on, so the sweep is
			 * required to have driven both signs and a fraction
			 * that is not zero.  Claimed off the INPUT -- the
			 * blob's own mean, taken before the call -- because
			 * the transcript is encoded and cannot be read.
			 */
			if (mainArm && eval) {
				float fr = rmean - (float)(int)rmean;

				if (diff_isnan_f(rmean)) {
					sawNan = 1;
				} else if (0.0f < rmean) {
					sawPlus = 1;
				} else {
					sawMinus = 1;
				}
				if (!diff_isnan_f(fr)
				    && (int)(fr * 10000.0f) != 0)
					sawFrac = 1;
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the saving arm was reached", sawSaved, 1, 0);
	diff_eq_int("the too-noisy arm was reached", sawRefused, 1, 0);
	diff_eq_int("the evaluation-disabled arm was reached", sawDisabled,
		    1, 0);
	diff_eq_int("the not-data-state arm was reached", sawElse, 1, 0);
	diff_eq_int("a '+' sign was printed", sawPlus, 1, 0);
	diff_eq_int("a '-' sign was printed", sawMinus, 1, 0);
	diff_eq_int("a non-zero fraction was printed", sawFrac, 1, 0);
	/*
	 * The NaN pattern reached the printer.  Without this the whole point
	 * of pattern 9 is unverified -- the arm is gated on `inPhase3` and
	 * EIA-6 and a schedule that correlated either with the pattern index
	 * would silently never print one.  Finding F2410.
	 */
	diff_eq_int("an unordered mean reached the sign printer", sawNan, 1, 0);

	return diff_end();
}

/* --------------------------------- VPCMXF_SessionTermination (19 bytes) */

/*
 * NINETEEN BYTES, AND ONLY ONE OF THE THREE THINGS THEY DO IS NEW.  The
 * function loads `+0x175c` of its argument and tail-jumps to
 * `V90Demodulator::sessionTermination`, which `run_sessterm` above already
 * sweeps against the blob nine patterns wide.  So what is left to prove is
 * the forwarding itself and the OFFSET -- and the offset is the part a naive
 * test cannot see, because a wrapper reading +0x1758 or +0x1760 instead would
 * pass every check that only looks at the demodulator it was handed.
 *
 * SO THE NEIGHBOURS ARE OCCUPIED.  `modem.modulator` (+0x1758) and
 * `modem.phase2Info` (+0x1760) are pointed at a SECOND, differently seeded
 * V90Demodulator-sized slot, and that slot is required to be byte-identical
 * to its pre-call image on both sides.  A wrapper off by one word would run
 * the method on it and the check would fail; nothing else here would.
 *
 * THE HANDLE ITSELF IS COMPARED AGAINST ITS PRE-IMAGE rather than side to
 * side, because the two sides' handles hold different pointers on purpose --
 * each points at its own demodulator.  The method writes nothing through the
 * handle, so "unchanged" is the whole claim and it is made per side.
 *
 * THE FOUR ARMS ARE STILL DRIVEN.  A forwarder that reached the method only
 * on the arm that prints, or only on the arm that stores, would be a
 * forwarder that is wrong; the sweep requires the saving arm, the
 * evaluation-disabled arm and the not-data-state arm all to have been reached
 * THROUGH THE WRAPPER, and the ClockDeviation word is predicted from the
 * blob's own summary of the blob's own history, exactly as `run_sessterm`
 * predicts it.
 *
 * The return value is not compared: this function is written `void` because
 * the object cannot say (see src/pump/v90/VPcmXfTerm.cpp), so there is
 * nothing here that a comparison would be comparing.
 */

#define XF_SLOT		0x1800		/* the handle's prefix, generously  */
#define XF_MODULATOR	0x1758		/* VPcmFloModem::modem.modulator    */
#define XF_DEMODULATOR	0x175c		/* VPcmFloModem::modem.demodulator  */
#define XF_PHASE2INFO	0x1760		/* VPcmFloModem::modem.phase2Info   */

static unsigned char xf[2][XF_SLOT] __attribute__((aligned(8)));
static unsigned char xfd[2][DEM_SLOT] __attribute__((aligned(8)));

extern "C" {
/*
 * Both sides by symbol, ours as well as the blob's: the entry point is
 * `extern "C"`, so its name is its own, and naming it here keeps this file
 * free of `VPcmFloModem.h` -- which it cannot have beside the V90Parameters
 * definition the fixture already carries (finding F1112).
 */
void our_xf_sessterm(void *self) asm("VPCMXF_SessionTermination");
void ref_xf_sessterm(void *self) asm("ref_VPCMXF_SessionTermination");
}

static int
run_vpcmxf_sessterm(void)
{
	static const unsigned int xf_len[] = { 1u, ST_HIST };
	static const float xf_minstd[] = { -1.0f, 1.0e9f };
	static const unsigned int xf_state[] = { 3u, 1u };
	struct trial_args t;
	int lvl, ii;
	int sawSaved = 0, sawRefused = 0, sawDisabled = 0, sawElse = 0;
	int sawPrinted = 0;

	diff_begin("VPCMXF_SessionTermination");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level((unsigned int)lvl);

		for (ii = 0; ii < ST_PATTERNS * 2 * 2 * 2 * 2; ii++) {
			static unsigned char xa[XF_SLOT], xb[XF_SLOT];
			static unsigned char da[DEM_SLOT], db[DEM_SLOT];
			static unsigned char ba[BLK_SLOT], bb[BLK_SLOT];
			void *hp[2], *dp[2], *cp[2];
			long tag = (long)lvl * 100000 + ii;
			int pat, li, mi, si, gi, side, k;
			unsigned int n, state;
			float minstd, rmean, rstd;
			int eval, mainArm, saveArm;

			pat = ii % ST_PATTERNS;
			li  = (ii / ST_PATTERNS) % 2;
			mi  = (ii / (ST_PATTERNS * 2)) % 2;
			si  = (ii / (ST_PATTERNS * 4)) % 2;
			gi  = (ii / (ST_PATTERNS * 8)) % 2;

			n = xf_len[li];
			minstd = xf_minstd[mi];
			state = xf_state[si];
			/*
			 * The evaluation switch rides on the pattern rather
			 * than on a dimension of its own: it only has to be
			 * seen both ways, and the two arms it selects are
			 * asserted reached below.
			 */
			eval = (pat & 1) ^ gi;

			t.latch = state;
			t.flag = 0;
			t.eia6 = gi ? 6 : 0;
			t.blockByte = 0;
			t.pcmType = 0;
			t.idx = ii;

			setup(ii, &t);

			/* The handle and the decoy, on the fixture's LFSR. */
			fill_pair(xf[0], xf[1], XF_SLOT);
			fill_pair(xfd[0], xfd[1], DEM_SLOT);

			for (side = 0; side < 2; side++) {
				unsigned int i;

				for (i = 0; i < ST_HIST; i++)
					st_hist[side][i] =
					    st_value(pat, (int)i);

				hp[side] = st_hist[side];
				memcpy(&dem[side][RS_HIST], &hp[side],
				       sizeof(void *));
				memcpy(&dem[side][RS_HLEN], &n, sizeof n);
				set_int(side, PARAMS_EVAL, eval);
				set_float(side, PARAMS_MINSTD, minstd);

				dp[side] = D(side);
				cp[side] = xfd[side];
				memcpy(&xf[side][XF_DEMODULATOR], &dp[side],
				       sizeof(void *));
				memcpy(&xf[side][XF_MODULATOR], &cp[side],
				       sizeof(void *));
				memcpy(&xf[side][XF_PHASE2INFO], &cp[side],
				       sizeof(void *));
			}

			memcpy(xa, xf[0], XF_SLOT);
			memcpy(xb, xf[1], XF_SLOT);
			memcpy(da, xfd[0], DEM_SLOT);
			memcpy(db, xfd[1], DEM_SLOT);
			memcpy(ba, blk[0], BLK_SLOT);
			memcpy(bb, blk[1], BLK_SLOT);

			rmean = ref_timingHistoryMean(&dem[1][0x94]);
			rstd = ref_timingHistoryStd(&dem[1][0x94]);
			mainArm = (state == 3u && t.eia6 != 6);
			saveArm = mainArm && eval && (minstd >= rstd);

			dsplib_debug_capture_reset();

			our_xf_sessterm(xf[0]);
			ref_xf_sessterm(xf[1]);

			teardown();

			/* Nothing is written through the handle, either side. */
			diff_eq_int("ours left the handle alone (%ld)",
				    memcmp(xa, xf[0], XF_SLOT) == 0, 1, tag);
			diff_eq_int("the blob left the handle alone (%ld)",
				    memcmp(xb, xf[1], XF_SLOT) == 0, 1, tag);

			/*
			 * THE OFFSET.  +0x1758 and +0x1760 point at this
			 * object; a wrapper reading either would have run the
			 * method on it.
			 */
			diff_eq_int("ours left +0x1758/+0x1760's object "
				    "alone (%ld)",
				    memcmp(da, xfd[0], DEM_SLOT) == 0, 1, tag);
			diff_eq_int("the blob left +0x1758/+0x1760's object "
				    "alone (%ld)",
				    memcmp(db, xfd[1], DEM_SLOT) == 0, 1, tag);

			for (side = 0; side < 2; side++) {
				diff_eq_int("timingHistory is untouched "
					    "(%ld)",
					    memcmp(&dem[side][RS_HIST],
						   &hp[side],
						   sizeof(void *)) == 0,
					    1, tag * 10 + side);
				memset(&dem[side][RS_HIST], 0,
				       sizeof(void *));
			}

			compare_all("after VPCMXF_SessionTermination", tag);
			transcripts_agree(tag);

			diff_eq_obj_(__FILE__, __LINE__,
				     "after VPCMXF_SessionTermination",
				     "the timing history itself",
				     st_hist[0], st_hist[1],
				     sizeof(st_hist[0]), tag);

			/*
			 * The forwarding, made visible: the one store the
			 * method makes has to have been made THROUGH the
			 * wrapper, on exactly the arm the gates select.
			 */
			{
				int outside = 0, want, got;

				for (k = 0; k < BLK_SLOT; k++) {
					if (k >= BLK_DEVIATION
					    && k < BLK_DEVIATION + 4)
						continue;
					if (bb[k] != blk[1][k])
						outside = 1;
				}
				diff_eq_int("the blob changed nothing but "
					    "+0x4c of the block (%ld)",
					    outside, 0, tag);

				memcpy(&got, &blk[1][BLK_DEVIATION],
				       sizeof got);
				if (saveArm) {
					want = (int)(1000.0f * rmean);
					sawSaved = 1;
				} else {
					memcpy(&want, &bb[BLK_DEVIATION],
					       sizeof want);
					if (mainArm && eval)
						sawRefused = 1;
				}
				diff_eq_int("the saved ClockDeviation (%ld)",
					    (long)got, (long)want, tag);

				if (mainArm && !eval)
					sawDisabled = 1;
				if (!mainArm)
					sawElse = 1;
			}

			if (lvl > 1 && dsplib_debug_capture_lines(1) != 0)
				sawPrinted = 1;
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the saving arm was reached through the wrapper",
		    sawSaved, 1, 0);
	diff_eq_int("the too-noisy arm was reached through the wrapper",
		    sawRefused, 1, 0);
	diff_eq_int("the evaluation-disabled arm was reached through the "
		    "wrapper", sawDisabled, 1, 0);
	diff_eq_int("the not-data-state arm was reached through the wrapper",
		    sawElse, 1, 0);
	diff_eq_int("the blob printed through the wrapper", sawPrinted, 1, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_latch();
	bad |= run_branches();
	bad |= run_observable();
	bad |= run_reinit();
	bad |= run_reset();
	bad |= run_enterchannelverification();
	bad |= run_getbitrate();
	bad |= run_sessterm();
	bad |= run_vpcmxf_sessterm();

	return bad;
}
