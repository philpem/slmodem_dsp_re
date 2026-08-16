/*
 * v90demfix.h -- one V90Demodulator graph, stood up identically on both sides.
 *
 * `V90Demodulator::enterPhase3` resets six subobjects reached four different
 * ways, and standing one up is the most expensive fixture in this tree: a
 * demodulator, a phase 3 demodulator, a parameter block and the block that
 * block points at, a Phase 2 record and its measurement array, an equaliser,
 * a connection evaluator, an automatic digital impairment detector, an SD
 * detector and its history, a V90Jd, a V92Jd, a DIL descriptor, a descrambler
 * buffer, a scrambler buffer, and a FloatFIR constructed per side by each
 * side's own constructor.  Sixteen allocations, and every one of them has to
 * be seeded pairwise, wired per side, and neutralised per side before the
 * comparison.
 *
 * IT WAS WRITTEN TWICE BEFORE THIS FILE EXISTED -- in t_v90p3dreset.cpp for
 * the phase 3 demodulator alone and in t_v90demod.cpp for the whole graph --
 * and `VPcmFloModem::enterPhase3` needed a third copy, which is what this
 * file is instead.  t_v90demod.cpp uses it; t_v90p3dreset.cpp does NOT, and
 * deliberately: it drives `V90Phase3Demodulator::reset` directly with eleven
 * arguments, allocates its slot as a union so the class can be a member of
 * one, and needs no demodulator at all.  Folding it in would mean carrying
 * two shapes here to save one copy of the smaller half.  It is recorded as a
 * candidate rather than done.
 *
 * WHAT A USER MUST DO, in order:
 *
 *     setup(trial, &args);     seed everything, wire it, construct both FIRs
 *     ... drive the method under test on D(0) and the blob's on D(1) ...
 *     teardown();              destroy both FIRs -- NOT optional, they malloc
 *     compare_all(what, tag);  compare all sixteen blocks
 *
 * `setup` leaves `lfsr_state` running, so a caller with storage of its own
 * fills it with `fill_pair` immediately afterwards and gets bytes that are
 * pairwise identical and varied, which is finding 230's rule.
 *
 * THE FLOAT-BEARING FIELDS ARE SET, NOT SEEDED.  `setTimingOffset` and
 * `V90Equalizer::enterPhase3` do x87 arithmetic on fields this fixture owns;
 * a random 32-bit pattern is a signalling NaN about one time in 250, and no
 * test should be discovering what two different compilations do with one.
 * Everything else is seeded.
 */

#ifndef DSPLIB_TEST_V90DEMFIX_H
#define DSPLIB_TEST_V90DEMFIX_H

#include <string.h>

#include "harness.h"

extern "C" {
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

/*
 * THE FOUR CLASS-TYPED PAIRS ARE HELD AS BYTES, and it is the constructor
 * batch that forced it.
 *
 * `static V90Jd jdo[2];` needs a default constructor, and a class the blob
 * gives a real one -- `_ZN5V90JdC1EP13V90Parameters` -- has none.  It needs a
 * destructor too, and the blob has `_ZN5V90JdD1Ev` as a one-byte `ret`, which
 * GCC emits ONLY for a user-declared destructor: a trivial implicit one
 * produces no symbol at all, so leaving it out would leave the symbol
 * undefined for every caller that destroys one.  So the classes gain both,
 * and this fixture stops being able to declare them by value.
 *
 * The cast-through-a-macro keeps every call site below unchanged, including
 * `&adid[0]`, `sizeof(adid[0])` and `adid[side].params`: the rows are exactly
 * `sizeof(class)` wide, so indexing the cast pointer is the same arithmetic
 * the array notation did.  The alternative -- an accessor function -- would
 * have meant editing twenty-odd sites in a header six tests include.
 */
static unsigned char adid_[2][sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));
static unsigned char sdd_[2][sizeof(V90SdDetector)]
	__attribute__((aligned(8)));
static float sdhist[2][SDD_HIST];
static unsigned char jdo_[2][sizeof(V90Jd)] __attribute__((aligned(8)));
static unsigned char jd92o_[2][sizeof(V92Jd)] __attribute__((aligned(8)));

#define adid	((V90AutoDigitalImpDetector *)adid_)
#define sdd	((V90SdDetector *)sdd_)
#define jdo	((V90Jd *)jdo_)
#define jd92o	((V92Jd *)jd92o_)
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
		 * anything, and no test is where two compilations' `(int)NaN`
		 * should be discovered.  Everything else in the Phase 2
		 * record stays seeded.
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
		((V90Equalizer *)equ[side])->maxLeCoefValue = 1.0f;
		((V90Equalizer *)equ[side])->linearEquMmxConversionFactor = 1.0f;
		((V90Equalizer *)equ[side])->maxDfeCoefValue = 1.0f;
		((V90Equalizer *)equ[side])->dfeMmxConversionFactor = 1.0f;

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
	/*
	 * NOTHING ELSE IS NEUTRALISED, DELIBERATELY.  Only a field this
	 * fixture sets to a per-side address can differ for a reason that is
	 * not a defect; every other word of the slot came out of fill_pair and
	 * is byte-identical on the two sides, so nulling it would turn a
	 * compared word into an ignored one.  The pointers this object holds
	 * but never uses -- the two mapping parameter blocks, the TRN2
	 * designer, the CP and MP records, the phase 4 demodulator, the
	 * demapper, the constellation designer, the detector, the embedded
	 * descrambler's seven and the resampler's vptr -- stay in the
	 * comparison for exactly that reason: a store that lands on one of
	 * them should fail the test.
	 */
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
		/* Storage plus a cast, for the reason given at `adid_` above. */
		static unsigned char aa_[sizeof(V90AutoDigitalImpDetector)]
			__attribute__((aligned(8)));
		static unsigned char ab_[sizeof(V90AutoDigitalImpDetector)]
			__attribute__((aligned(8)));
		V90AutoDigitalImpDetector &aa =
			*(V90AutoDigitalImpDetector *)aa_;
		V90AutoDigitalImpDetector &ab =
			*(V90AutoDigitalImpDetector *)ab_;

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

#endif /* DSPLIB_TEST_V90DEMFIX_H */
