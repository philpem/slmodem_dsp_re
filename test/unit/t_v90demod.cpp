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
 */

#include <string.h>

#include "harness.h"

extern "C" {
void ref_enterPhase3(void *self) asm("ref__ZN14V90Demodulator11enterPhase3Ev");

/* The embedded prefilter's FIR: each side's own constructor, each side's own
 * allocator. */
void fir_ctor(void *self, unsigned n, float *c, unsigned b)
	asm("_ZN8FloatFIRC1EjPfj");
void fir_dtor(void *self) asm("_ZN8FloatFIRD1Ev");
void ref_fir_ctor(void *self, unsigned n, float *c, unsigned b)
	asm("ref__ZN8FloatFIRC1EjPfj");
void ref_fir_dtor(void *self) asm("ref__ZN8FloatFIRD1Ev");

extern unsigned int ref_dsplibs_debug_level;
}

#include "dsplib/debug.h"
#include "dsplib/V90Demodulator.h"

#define DEM_SLOT	(0x298 + 64)
#define P3D_SLOT	(0x42c + 64)
#define PARM_SLOT	(V90PARAMETERS_BOUND + 64)
#define PH2_SLOT	(sizeof(V90Phase2Info) + 32)
#define MEAS_SLOT	0x80
#define BLK_SLOT	0x80
#define EQU_SLOT	0x150
#define CE_SLOT		0xbc

/* The FIR shape V90PreFilter's own constructor uses; see t_v90prefilter.cpp. */
#define FIR_TAPS	0x28
#define FIR_SLACK	0x63

/* The descrambler and scrambler inside the phase 3 demodulator. */
#define DSC_TAP1	0x12u
#define DSC_TAIL	0x17u
#define DSC_OUT		0x63u
#define DSC_WORDS	(1u + DSC_TAIL + DSC_OUT)
#define SCR_BUF		64u
#define SCR_OUT		40u
#define SCR_TAP1	45u
#define SCR_TAP2	63u
#define SCR_TAIL	23u
#define SDD_HIST	12u

/* Where V90PreFilter::isV90WithEia6 looks, and where the timing offset is. */
#define PARAMS_EIA6	0x500
#define PARAMS_TIMING	0x084

static unsigned char dem[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char p3d[2][P3D_SLOT] __attribute__((aligned(8)));
static unsigned char parm[2][PARM_SLOT] __attribute__((aligned(8)));
static unsigned char ph2[2][PH2_SLOT] __attribute__((aligned(8)));
static unsigned char meas[2][MEAS_SLOT] __attribute__((aligned(8)));
static unsigned char blk[2][BLK_SLOT] __attribute__((aligned(8)));
static unsigned char equ[2][EQU_SLOT] __attribute__((aligned(8)));
static unsigned char ce[2][CE_SLOT] __attribute__((aligned(8)));

static V90AutoDigitalImpDetector adid[2];
static V90SdDetector sdd[2];
static float sdhist[2][SDD_HIST];
static V90Jd jdo[2];
static V92Jd jd92o[2];
static tagV90DILdescriptor dilo[2];
static int dbuf[2][DSC_WORDS];
static unsigned char sbuf[2][SCR_BUF];

static V90Demodulator *
D(int side)
{
	return (V90Demodulator *)dem[side];
}

static V90Phase3Demodulator *
P3(int side)
{
	return (V90Phase3Demodulator *)p3d[side];
}

static V90Phase2Info *
P2(int side)
{
	return (V90Phase2Info *)ph2[side];
}

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

static void
fill_pair(void *a, void *b, size_t n)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	size_t i;

	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte();
}

static void
set_int(int side, int off, int v)
{
	memcpy(&parm[side][off], &v, sizeof v);
}

static void
set_float(int side, int off, float v)
{
	memcpy(&parm[side][off], &v, sizeof v);
}

/* Sane finite values for everything the two x87 callees touch. */
static const float ppm_v[] = { 0.0f, 1.0f, -1.0f, 250.0f, -37.5f, 1e4f };
#define NPPM ((int)(sizeof(ppm_v) / sizeof(ppm_v[0])))

static const float beta_v[] = { 0.0f, 1.0f, -0.25f, 1e-6f };
#define NBETA ((int)(sizeof(beta_v) / sizeof(beta_v[0])))

struct trial_args {
	unsigned int latch;	/* +0x34 */
	unsigned int flag;	/* +0x30 */
	int eia6;		/* the parameter block's +0x500 */
	int blockByte;		/* the signed byte the block's +0x02 holds */
	int pcmType;		/* the Phase 2 record's, 0 or 1 */
	int idx;		/* picks the float and beta values */
};

static void
setup(int trial, const struct trial_args *t)
{
	int side;

	lfsr_state = 0x3b7fu + 0x9e37u * (unsigned)trial;

	fill_pair(dem[0], dem[1], DEM_SLOT);
	fill_pair(p3d[0], p3d[1], P3D_SLOT);
	fill_pair(parm[0], parm[1], PARM_SLOT);
	fill_pair(ph2[0], ph2[1], PH2_SLOT);
	fill_pair(meas[0], meas[1], MEAS_SLOT);
	fill_pair(blk[0], blk[1], BLK_SLOT);
	fill_pair(equ[0], equ[1], EQU_SLOT);
	fill_pair(ce[0], ce[1], CE_SLOT);
	fill_pair(&adid[0], &adid[1], sizeof(adid[0]));
	fill_pair(&sdd[0], &sdd[1], sizeof(sdd[0]));
	fill_pair(sdhist[0], sdhist[1], sizeof(sdhist[0]));
	fill_pair(&jdo[0], &jdo[1], sizeof(jdo[0]));
	fill_pair(&jd92o[0], &jd92o[1], sizeof(jd92o[0]));
	fill_pair(&dilo[0], &dilo[1], sizeof(dilo[0]));
	fill_pair(dbuf[0], dbuf[1], sizeof(dbuf[0]));
	fill_pair(sbuf[0], sbuf[1], sizeof(sbuf[0]));

	fir_ctor(&D(0)->preFilter.fir, FIR_TAPS, 0, FIR_SLACK);
	ref_fir_ctor(&D(1)->preFilter.fir, FIR_TAPS, 0, FIR_SLACK);

	for (side = 0; side < 2; side++) {
		V90Demodulator *d = D(side);

		d->phase2Info = P2(side);
		d->jd = &jdo[side];
		d->jdV92 = &jd92o[side];
		d->dil = &dilo[side];
		d->params = (V90Parameters *)parm[side];
		d->equalizer = (V90Equalizer *)equ[side];
		d->phase3Demodulator = P3(side);
		d->connectionEvaluator = (V90ConnectionEvaluator *)ce[side];
		d->inPhase3 = t->latch;
		d->sessionFlag = t->flag;

		d->preFilter.phase2 = P2(side);
		d->preFilter.params = (V90Parameters *)parm[side];
		d->preFilter.codecType = 0;
		d->preFilter.gain = 0;
		d->preFilter.refLoop = -1;

		d->resampler.ppmScale = ppm_v[t->idx % NPPM];
		d->resampler.timingOffset = 0.0f;

		/*
		 * FINITE MEASUREMENTS, NOT SEEDED BYTES.  `printInfo` prints
		 * all 21 entries of `L2` and `autoSelection` matches six of
		 * them against every reference loop, both in x87 arithmetic;
		 * a random 32-bit pattern is as likely to be a NaN as
		 * anything, and this file is not where two compilations'
		 * `(int)NaN` should be discovered.  Everything else in the
		 * Phase 2 record stays seeded.
		 */
		{
			unsigned int i;

			for (i = 0; i < MEAS_SLOT / sizeof(float); i++) {
				float v = -40.0f + 0.75f * (float)(int)
				    ((i * 7u + (unsigned)trial) % 61u);

				memcpy(&meas[side][i * sizeof(float)], &v,
				       sizeof v);
			}
		}
		P2(side)->L2 = (float *)meas[side];
		P2(side)->pcmType = t->pcmType;
		P2(side)->rtd = 0x1234 + trial;
		P2(side)->Uinfo = (unsigned char)(trial * 11u + 3u);

		*(void **)&parm[side][0] = blk[side];
		blk[side][2] = (unsigned char)t->blockByte;
		set_int(side, PARAMS_EIA6, t->eia6);
		set_float(side, PARAMS_TIMING, ppm_v[(t->idx + 1) % NPPM]);

		((V90Equalizer *)equ[side])->linearEquBeta =
		    beta_v[t->idx % NBETA];
		((V90Equalizer *)equ[side])->dfeBeta =
		    beta_v[(t->idx + 1) % NBETA];
		((V90Equalizer *)equ[side])->state = t->idx % 7;
		((V90Equalizer *)equ[side])->mmxMode = t->idx & 1;
		((V90Equalizer *)equ[side])->linearEquMmxRefLevel = 1.0f;
		((V90Equalizer *)equ[side])->linearEquMmxBetaScale = 1.0f;
		((V90Equalizer *)equ[side])->dfeMmxRefLevel = 1.0f;
		((V90Equalizer *)equ[side])->dfeMmxBetaScale = 1.0f;

		/* The phase 3 demodulator, wired as t_v90p3dreset.cpp wires it. */
		P3(side)->autoDigitalImpDetector = &adid[side];
		P3(side)->sdDetector = &sdd[side];
		adid[side].params = (V90Parameters *)parm[side];
		sdd[side].history = sdhist[side];
		sdd[side].historyLength = SDD_HIST;
		dilo[side].dilCount = (unsigned char)(trial * 37u + 1u);
		dilo[side].seq1Length =
		    (unsigned char)((trial * 13u + 1u) % 129u);
		dilo[side].seq2Length =
		    (unsigned char)((trial * 29u + 7u) % 129u);

		{
			Descrambler<int, int> *ds = &P3(side)->descrambler;
			unsigned int out = (unsigned)trial % (DSC_OUT + 1u);

			ds->pLimit = dbuf[side];
			ds->pInitOut = dbuf[side] + DSC_OUT;
			ds->pInitTap1 = dbuf[side] + DSC_OUT + DSC_TAP1;
			ds->pInitTap2 = dbuf[side] + DSC_OUT + DSC_TAIL;
			ds->pOut = dbuf[side] + out;
			ds->pTap1 = dbuf[side] + out + DSC_TAP1;
			ds->pTap2 = dbuf[side] + out + DSC_TAIL;
			ds->tailLength = DSC_TAIL;
		}
		{
			Scrambler<unsigned char, int> *s =
			    &P3(side)->phase3Modulator.scrambler;
			unsigned int out = (unsigned)trial % 41u;

			s->pLimit = sbuf[side];
			s->pInitOut = sbuf[side] + SCR_OUT;
			s->pInitTap1 = sbuf[side] + SCR_TAP1;
			s->pInitTap2 = sbuf[side] + SCR_TAP2;
			s->pOut = sbuf[side] + out;
			s->pTap1 = sbuf[side] + out + (SCR_TAP1 - SCR_OUT);
			s->pTap2 = sbuf[side] + out + (SCR_TAP2 - SCR_OUT);
			s->tailLength = SCR_TAIL;
		}
	}
}

static void
teardown(void)
{
	fir_dtor(&D(0)->preFilter.fir);
	ref_fir_dtor(&D(1)->preFilter.fir);
}

/*
 * A copy of one side's demodulator with every pointer replaced by something
 * both sides can agree about.  Same idiom as snap_dem in
 * t_v90sessionflag.cpp.  `preFilter.fir.coefficients` points into whichever
 * side's static coefficient table `selectFilter` chose, so it is replaced by
 * its offset from that side's own bank -- which is what t_v90prefilter.cpp
 * does, except that here `selectFilter` is already differentially tested and
 * all this needs to establish is that it ran on the right object.
 */
static void
snap_dem(unsigned char *dst, int side)
{
	V90Demodulator *s;
	V90Demodulator *l = D(side);

	memcpy(dst, dem[side], DEM_SLOT);
	s = (V90Demodulator *)dst;

	s->phase2Info = (V90Phase2Info *)(long)(l->phase2Info == P2(side));
	s->jd = (V90Jd *)(long)(l->jd == &jdo[side]);
	s->jdV92 = (V92Jd *)(long)(l->jdV92 == &jd92o[side]);
	s->dil = (tagV90DILdescriptor *)(long)(l->dil == &dilo[side]);
	s->params = (V90Parameters *)(long)
	    (l->params == (V90Parameters *)parm[side]);
	s->equalizer = (V90Equalizer *)(long)
	    (l->equalizer == (V90Equalizer *)equ[side]);
	s->phase3Demodulator = (V90Phase3Demodulator *)(long)
	    (l->phase3Demodulator == P3(side));
	s->connectionEvaluator = (V90ConnectionEvaluator *)(long)
	    (l->connectionEvaluator == (V90ConnectionEvaluator *)ce[side]);
	s->preFilter.phase2 = (V90Phase2Info *)(long)
	    (l->preFilter.phase2 == P2(side));
	s->preFilter.params = (V90Parameters *)(long)
	    (l->preFilter.params == (V90Parameters *)parm[side]);
	/*
	 * The FIR's coefficient pointer is into that side's OWN copy of the
	 * static bank and its history is that side's own allocation, so
	 * neither can ever compare equal.  Which bank `selectFilter` chose is
	 * t_v90prefilter.cpp's claim, not this file's; what this file needs is
	 * that `selectFilter` ran on the right object, and the prefilter's
	 * `gain` and `refLoop` beside them say that.
	 */
	s->preFilter.fir.coefficients = NULL;
	s->preFilter.fir.history = NULL;
	s->resampler.vptr = NULL;
	s->descrambler.pLimit = NULL;
	s->descrambler.pInitOut = NULL;
	s->descrambler.pInitTap1 = NULL;
	s->descrambler.pInitTap2 = NULL;
	s->descrambler.pOut = NULL;
	s->descrambler.pTap1 = NULL;
	s->descrambler.pTap2 = NULL;
	s->mappingParams = NULL;
	s->mappingParamsAlt = NULL;
	s->trn2Designer = NULL;
	s->cp = NULL;
	s->mp = NULL;
	s->phase4Demodulator = NULL;
	s->demapper = NULL;
	s->constellationDesigner = NULL;
	s->autoDigitalImpDetector = NULL;
}

static void
snap_p3d(unsigned char *dst, int side)
{
	V90Phase3Demodulator *s;
	V90Phase3Demodulator *l = P3(side);

	memcpy(dst, p3d[side], P3D_SLOT);
	s = (V90Phase3Demodulator *)dst;

	s->autoDigitalImpDetector = (V90AutoDigitalImpDetector *)(long)
	    (l->autoDigitalImpDetector == &adid[side]);
	s->sdDetector = (V90SdDetector *)(long)(l->sdDetector == &sdd[side]);
	s->dil = (tagV90DILdescriptor *)(long)(l->dil == &dilo[side]);
	s->jd = (V90Jd *)(long)(l->jd == &jdo[side]);
	s->jdV92 = (V92Jd *)(long)(l->jdV92 == &jd92o[side]);
	s->params = NULL;
	s->ansamToneDetector = NULL;

	s->descrambler.pLimit = (int *)(l->descrambler.pLimit - dbuf[side]);
	s->descrambler.pInitOut = (int *)(l->descrambler.pInitOut - dbuf[side]);
	s->descrambler.pInitTap1 =
	    (int *)(l->descrambler.pInitTap1 - dbuf[side]);
	s->descrambler.pInitTap2 =
	    (int *)(l->descrambler.pInitTap2 - dbuf[side]);
	s->descrambler.pOut = (int *)(l->descrambler.pOut - dbuf[side]);
	s->descrambler.pTap1 = (int *)(l->descrambler.pTap1 - dbuf[side]);
	s->descrambler.pTap2 = (int *)(l->descrambler.pTap2 - dbuf[side]);

	s->phase3Modulator.scrambler.pLimit = (unsigned char *)
	    (l->phase3Modulator.scrambler.pLimit - sbuf[side]);
	s->phase3Modulator.scrambler.pInitOut = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitOut - sbuf[side]);
	s->phase3Modulator.scrambler.pInitTap1 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitTap1 - sbuf[side]);
	s->phase3Modulator.scrambler.pInitTap2 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitTap2 - sbuf[side]);
	s->phase3Modulator.scrambler.pOut = (unsigned char *)
	    (l->phase3Modulator.scrambler.pOut - sbuf[side]);
	s->phase3Modulator.scrambler.pTap1 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pTap1 - sbuf[side]);
	s->phase3Modulator.scrambler.pTap2 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pTap2 - sbuf[side]);
}

static void
compare_all(const char *what, long tag)
{
	static unsigned char a[DEM_SLOT], b[DEM_SLOT];
	static unsigned char c[P3D_SLOT], d[P3D_SLOT];
	static unsigned char pa[PARM_SLOT], pb[PARM_SLOT];

	snap_dem(a, 0);
	snap_dem(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Demodulator slot",
		     a, b, DEM_SLOT, tag);

	snap_p3d(c, 0);
	snap_p3d(d, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase3Demodulator slot",
		     c, d, P3D_SLOT, tag);

	/* The block's first word is each side's own pointer; the rest is not. */
	memcpy(pa, parm[0], PARM_SLOT);
	memcpy(pb, parm[1], PARM_SLOT);
	memset(pa, 0, sizeof(void *));
	memset(pb, 0, sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Parameters block",
		     pa, pb, PARM_SLOT, tag);

	diff_eq_obj_(__FILE__, __LINE__, what, "V90Equalizer",
		     equ[0], equ[1], EQU_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90ConnectionEvaluator",
		     ce[0], ce[1], CE_SLOT, tag);
	{
		static unsigned char ha[PH2_SLOT], hb[PH2_SLOT];

		/* `L2` is each side's own measurement array. */
		memcpy(ha, ph2[0], PH2_SLOT);
		memcpy(hb, ph2[1], PH2_SLOT);
		((V90Phase2Info *)ha)->L2 = NULL;
		((V90Phase2Info *)hb)->L2 = NULL;
		diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase2Info block",
			     ha, hb, PH2_SLOT, tag);
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "L2 measurements",
		     meas[0], meas[1], MEAS_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the block params points at",
		     blk[0], blk[1], BLK_SLOT, tag);
	{
		static V90AutoDigitalImpDetector aa, ab;

		/* Its `params` is each side's own block; everything else is
		 * memory the detector's own reset writes. */
		memcpy(&aa, &adid[0], sizeof(aa));
		memcpy(&ab, &adid[1], sizeof(ab));
		aa.params = NULL;
		ab.params = NULL;
		diff_eq_obj_(__FILE__, __LINE__, what,
			     "V90AutoDigitalImpDetector", &aa, &ab,
			     sizeof(aa), tag);
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SdDetector history",
		     sdhist[0], sdhist[1], sizeof(sdhist[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Jd",
		     &jdo[0], &jdo[1], sizeof(jdo[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Jd",
		     &jd92o[0], &jd92o[1], sizeof(jd92o[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "descrambler buffer",
		     dbuf[0], dbuf[1], sizeof(dbuf[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "scrambler buffer",
		     sbuf[0], sbuf[1], sizeof(sbuf[0]), tag);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
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

int
main(void)
{
	int bad = 0;

	bad |= run_latch();
	bad |= run_branches();
	bad |= run_observable();

	return bad;
}
