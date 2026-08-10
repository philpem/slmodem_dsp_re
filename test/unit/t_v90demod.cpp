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
 * definition V90PreFilter.h has already supplied (finding 1112).
 */
#include "dsplib/V90ConstellationDesigner.h"

extern "C" {
void ref_enterPhase3(void *self) asm("ref__ZN14V90Demodulator11enterPhase3Ev");
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
	D(0)->word_294 = 0x1234abcdu;
	D(1)->word_294 = 0x1234abcdu;
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
 * values of.  The answer is finding 1105's: give both sides THE SAME arena,
 * so every stored pointer agrees and `compare_all`'s raw comparison of
 * `equ[0]` against `equ[1]` and of the two demodulator slots keeps working
 * with nothing excluded -- and then snapshot the arena, run ours, copy the
 * result away, restore, and run the blob's, so that two writers into one
 * buffer do not hide each other (finding 805's shape).
 *
 * THE EMBEDDED RESAMPLER IS WIRED BY OFFSET, and that is not laziness.
 * `V90Demodulator::reset` calls `V90Resampler::reset` on the object at +0x94,
 * whose V90Resampler-only fields start at ITS +0x94; but this file cannot
 * include `V90Resampler.h`, because that header carries the OTHER definition
 * of `V90Parameters` and `V90Demodulator.h` has already supplied the union
 * one (finding 1112).  So the four fields are poked by displacement, with the
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
	struct trial_args t;
	long tag = 90000;
	int li, ci, cp, mmx, q;
	int saw_derived = 0, saw_configured = 0;
	unsigned lvl;

	diff_begin("V90Demodulator::reset");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level(lvl);
		for (li = 0; li < 3; li++)
		    for (ci = 0; ci < 2; ci++)
			for (cp = 0; cp < 4; cp++)
			    for (mmx = 0; mmx < 2; mmx++)
				for (q = 0; q < 2; q++) {
					unsigned int quick =
					    q ? 0x5a5a1234u : 0u;

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
					diff_eq_int("word_294 = quickConnect "
						    "(%ld)",
						    (long)D(1)->word_294,
						    (long)quick, tag);
					/*
					 * The store into ANOTHER object, and
					 * the reason V90Equalizer is 0x150
					 * bytes (finding 1107).
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

	return bad;
}
