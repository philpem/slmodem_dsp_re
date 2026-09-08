/*
 * t_v90demprog.cpp -- V90Demodulator::progress, differentially.
 *
 * WHY THIS IS ITS OWN BINARY AND NOT AN EXTENSION OF t_v90p4ddec.  Finding
 * F7483 chose `t_v90p4ddec` for `exitPhase3` by asking what its LAST statement
 * needed constructed, and that is the right question here too -- but
 * `progress` opens with FOUR calls into objects `exitPhase3` never touches:
 * `FloatFIR::process` on the prefilter, `Agc<float>::process`,
 * `V90Resampler::resample` and `V90Equalizer::process`, over five heap arrays
 * hanging off +0x244..+0x25c.  `t_v90p4ddec` plants its `V90Demodulator` as a
 * seeded byte slot, so not one of those subobjects is CONSTRUCTED there;
 * `test/harness/v90demfix.h` builds the pair that is, and already carries the
 * sixteen blocks, the `fir_ctor` pair and the phase 3 wiring.  So the front
 * end comes from there and this file adds what `progress` needs beyond it.
 *
 * THE FIRST THING THIS FILE MEASURED WAS WHETHER IT COULD EXIST AT ALL.
 * `V90Equalizer::process` is called unconditionally in the prologue, and
 * `t_v90equproc` is declared in `tools/gccdiverge.json` for finding F6203's
 * x87 excess precision -- one subtraction the object narrows to 32 bits and
 * GCC 13 keeps at 80.  A `progress` test that drove that site with arbitrary
 * floats would have to be declared too, and CLAUDE.md's rule is that a
 * declared binary can score NO mutations, for the suite and not the row
 * (findings F6000, F6001, F6002).  The measurement is in the trial data below.
 */

#include "v90demfix.h"

#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Phase4Demodulator.h"
#include "dsplib/tagV90AdditionalCPinfo.h"
#include "dsplib/modem_params.h"

extern "C" {
void ref_dem_progress(void *self, int *out, unsigned int *nofOut, float *in,
		      unsigned int n)
	asm("ref__ZN14V90Demodulator8progressEPiRjPfj");

void vr_ctor(void *s, unsigned int phases, float ppmScale, unsigned int taps,
	     float cutoff, void *params, float ppm, unsigned int minHistory)
	asm("_ZN12V90ResamplerC1EjfjfP13V90Parametersfj");
void ref_vr_ctor(void *s, unsigned int phases, float ppmScale,
		 unsigned int taps, float cutoff, void *params, float ppm,
		 unsigned int minHistory)
	asm("ref__ZN12V90ResamplerC1EjfjfP13V90Parametersfj");
void vr_dtor(void *s) asm("_ZN12V90ResamplerD1Ev");
void ref_vr_dtor(void *s) asm("ref__ZN12V90ResamplerD1Ev");

void equ_reset(void *s, unsigned int cursor) asm("_ZN12V90Equalizer5resetEj");
void ref_equ_reset(void *s, unsigned int cursor)
	asm("ref__ZN12V90Equalizer5resetEj");
void equ_enter_p3(void *s) asm("_ZN12V90Equalizer11enterPhase3Ev");
void ref_equ_enter_p3(void *s)
	asm("ref__ZN12V90Equalizer11enterPhase3Ev");

#define PARAM_CTOR "_ZN13V90ParametersC1EP19_tagModemParameters"
void param_ctor(void *s, void *mp) asm(PARAM_CTOR);
void ref_param_ctor(void *s, void *mp) asm("ref_" PARAM_CTOR);
void param_dtor(void *s) asm("_ZN13V90ParametersD1Ev");
void ref_param_dtor(void *s) asm("ref__ZN13V90ParametersD1Ev");

#define ADID_CTOR "_ZN25V90AutoDigitalImpDetectorC1EP13V90Parameters"
void adid_ctor(void *s, void *p) asm(ADID_CTOR);
void ref_adid_ctor(void *s, void *p) asm("ref_" ADID_CTOR);
void adid_dtor(void *s) asm("_ZN25V90AutoDigitalImpDetectorD1Ev");
void ref_adid_dtor(void *s)
	asm("ref__ZN25V90AutoDigitalImpDetectorD1Ev");

#define CE_CTOR "_ZN22V90ConnectionEvaluatorC1EP13V90Parameters"
void ce_ctor(void *s, void *p) asm(CE_CTOR);
void ref_ce_ctor(void *s, void *p) asm("ref_" CE_CTOR);
void ce_dtor(void *s) asm("_ZN22V90ConnectionEvaluatorD1Ev");
void ref_ce_dtor(void *s) asm("ref__ZN22V90ConnectionEvaluatorD1Ev");

#define P3D_CTOR "_ZN20V90Phase3DemodulatorC1EP13V90ParametersP19V90SpectralVerifierjP25V90AutoDigitalImpDetector"
void p3d_ctor(void *s, void *p, void *sv, unsigned int flag, void *ad)
	asm(P3D_CTOR);
void ref_p3d_ctor(void *s, void *p, void *sv, unsigned int flag, void *ad)
	asm("ref_" P3D_CTOR);
void p3d_dtor(void *s) asm("_ZN20V90Phase3DemodulatorD1Ev");
void ref_p3d_dtor(void *s) asm("ref__ZN20V90Phase3DemodulatorD1Ev");

#define DM_CTOR	"_ZN11V90DemapperC1EjP13V90ParametersP25V90AutoDigitalImpDetector"
void dm_ctor(void *s, unsigned int n, void *p, void *adi) asm(DM_CTOR);
void ref_dm_ctor(void *s, unsigned int n, void *p, void *adi)
	asm("ref_" DM_CTOR);
void dm_reset(void *s, void *mp) asm("_ZN11V90Demapper5resetEP16V90MappingParams");
void ref_dm_reset(void *s, void *mp)
	asm("ref__ZN11V90Demapper5resetEP16V90MappingParams");

#define CD_CTOR	"_ZN24V90ConstellationDesignerC1EP13V90ParametersP12V90PreFilterP21V90ConstellationPower"
void cd_ctor(void *s, void *p, void *pf, void *pw) asm(CD_CTOR);
void ref_cd_ctor(void *s, void *p, void *pf, void *pw) asm("ref_" CD_CTOR);

#define TD_CTOR	"_ZN15V90TRN2DesignerC1EP13V90ParametersP21V90ConstellationPower"
void td_ctor(void *s, void *p, void *pw) asm(TD_CTOR);
void ref_td_ctor(void *s, void *p, void *pw) asm("ref_" TD_CTOR);
}

/*
 * The parameter slots the resampler's constructor and `setBllState` read.
 * v90demfix.h's `setup` seeds the whole block, and a seeded 32-bit pattern is
 * a plausible NaN and a wildly implausible buffer length, so every one of
 * these is SET rather than left as it was found -- the same rule the shared
 * fixture states for the Phase 2 measurements.
 */
#define PARAMS_TIMING_HIST_LEN		(0x160 / 4)	/* int   */
#define PARAMS_TIMING_HIST_PERIOD	(0x164 / 4)	/* int   */
#define PARAMS_BLL_FIRST_K		(0x0b8 / 4)	/* float */
#define PARAMS_BLL_LAST_K		(0x0f4 / 4)	/* float */
#define PARAMS_ENERGY_DROP_THRESHOLD	(0x29c / 4)	/* float */
#define PARAMS_NO_ENERGY_DURATION	(0x2a0 / 4)	/* int   */
#define PARAMS_MAX_NOF_REMOTE_RETRAINS	(0x464 / 4)	/* int   */

#define PROG_IN		48u		/* input samples a call             */
#define PROG_TAPS	4u		/* the resampler's polyphase taps   */
#define PROG_PHASES	2u
#define PROG_ARR	1024u		/* every per-sample buffer          */
#define PROG_EQARR	80u		/* t_v90equproc's ARR_F/ARR_S       */

/*
 * `word_1c` must EXCEED `linearEquLength`: `V90Equalizer::reset` sets
 * `word_20 = word_1c - linearEquLength - 1` and `process` indexes `array_18`
 * with it, so the sixteen-against-eight shape is the fixture's own
 * requirement and not a taste.  t_v90demod's `wire_life` uses the same pair.
 */
/*
 * TWO DIFFERENT DENOMINATORS.  The reset equaliser produces one event value;
 * `progress` may then replace that value with a connection-evaluator outcome,
 * so counting its final `word_3c` used to misreport three reached event arms.
 * Keep the producer event and final outcome separate.
 */
#define PROG_EVENTS	1
#define PROG_OUTCOMES	3

#define PROG_LELEN	8u
#define PROG_M		16u
#define PROG_DFELEN	6u

static float fir_bank[FIR_TAPS];
static float prog_in[PROG_IN];

static float a244[2][PROG_ARR];
static float a248[2][PROG_ARR];
static short a250[2][PROG_ARR];
static float a254[2][PROG_ARR];
static unsigned char a25c[2][PROG_ARR];
static int prog_out[2][PROG_ARR];

static struct {
	float lecoefs[PROG_EQARR];
	float a18[PROG_EQARR];
	float lewin[PROG_EQARR];
	float dfewin[PROG_EQARR];
	float dfecoefs[PROG_EQARR];
	float a44[PROG_EQARR];
	float meanerr[V90EQU_MEAN_ERROR_LEN];
	short lemmx[PROG_EQARR];
	short ad8[PROG_EQARR];
	short aec[PROG_EQARR];
	short dfemmx[PROG_EQARR];
	short a118[PROG_EQARR];
	short a12c[PROG_EQARR];
	short b4[512];
	short b8[256];
} eqa[2];

/*
 * THE HALF OF THE RECEIVER v90demfix.h DOES NOT BUILD.  `progress` reaches
 * six more objects than `enterPhase3` does, and each is here in the shape the
 * arms that touch it need: the demapper and the two designers are CONSTRUCTED,
 * because `V90Demapper::process` and `V90ConstellationDesigner::process` walk
 * them; the phase 4 demodulator is a seeded block, because the four members
 * `progress` calls on it (`enterWaitForCP`, `enterWaitForMP`,
 * `enterWaitForEd`, `resetBeforRRN`) are between five and three stores each
 * and read nothing an allocation would have to have made.
 */
#define DMP_SLOT	(0x1eb8u + 64u)
#define P4D_SLOT	(0x351cu + 64u)
#define CDZ_SLOT	((unsigned)sizeof(V90ConstellationDesigner) + 64u)
#define TDZ_SLOT	(8u + 64u)
#define ACP_SLOT	((unsigned)sizeof(tagV90AdditionalCPinfo) + 64u)
#define CPZ_SLOT	0x3bb0u
#define MAP_SLOT	((unsigned)sizeof(V90MappingParams) + 64u)

static unsigned char dmp_[2][DMP_SLOT] __attribute__((aligned(8)));
static unsigned char p4d_[2][P4D_SLOT] __attribute__((aligned(8)));
static unsigned char cdz_[2][CDZ_SLOT] __attribute__((aligned(8)));
static unsigned char tdz_[2][TDZ_SLOT] __attribute__((aligned(8)));
static unsigned char acp_[2][ACP_SLOT] __attribute__((aligned(8)));
static unsigned char cpz_[2][CPZ_SLOT] __attribute__((aligned(8)));
static unsigned char mpz_[2][sizeof(V90MP) + 64] __attribute__((aligned(8)));
static unsigned char map1_[2][MAP_SLOT] __attribute__((aligned(8)));
static unsigned char map2_[2][MAP_SLOT] __attribute__((aligned(8)));
static unsigned char dscbuf[2][256];

#define DMP(s)	((V90Demapper *)dmp_[s])
#define P4D(s)	((V90Phase4Demodulator *)p4d_[s])
#define CDZ(s)	((V90ConstellationDesigner *)cdz_[s])
#define ACP(s)	((tagV90AdditionalCPinfo *)acp_[s])
#define MAP1(s)	((V90MappingParams *)map1_[s])
#define MAP2(s)	((V90MappingParams *)map2_[s])
#define MPZ(s)	((V90MP *)mpz_[s])


/*
 * PER-SIDE ADDRESSES, MASKED BY MEASUREMENT RATHER THAN BY A LIST.  Six of the
 * objects below hold pointers into their own side's storage -- the demapper's
 * two allocations, the designer's prefilter and power, every `params` word --
 * and those can never compare equal however right the code is.  Naming them
 * one by one is a list that goes stale the moment a constructor gains a
 * pointer; taking the BEFORE image of both sides and masking exactly the bytes
 * that already differed is the same claim with no list to maintain, and it
 * cannot hide a store, because a byte the two sides agreed on before the call
 * is still compared after it.
 */
static unsigned char msk_a[0x3bb0], msk_b[0x3bb0];

static void
mask_cmp(const char *file, int line, const char *what, const char *type,
	 const unsigned char *a, const unsigned char *b,
	 const unsigned char *pa, const unsigned char *pb, unsigned int n,
	 long tag)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		int same = (pa[i] == pb[i]);

		msk_a[i] = same ? a[i] : 0;
		msk_b[i] = same ? b[i] : 0;
	}
	diff_eq_obj_(file, line, what, type, msk_a, msk_b, n, tag);
}

static unsigned char cmp_a[DEM_SLOT];
static unsigned char cmp_b[DEM_SLOT];
static unsigned char pre_dmp[2][DMP_SLOT];
static unsigned char pre_p4d[2][P4D_SLOT];
static unsigned char pre_ce[2][CE_SLOT];
static unsigned char pre_cdz[2][CDZ_SLOT];
static unsigned char pre_parm[2][PARM_SLOT];
static unsigned char pre_acp[2][ACP_SLOT];
static unsigned char pre_equ[2][EQU_SLOT];
static unsigned char pre_p3d[2][P3D_SLOT];
static unsigned char pre_flow_parm[2][sizeof(V90Parameters) + 64u];
static unsigned char pre_vr[2][sizeof(V90Resampler)];

/* Full parameter objects for the constructed Phase-3 composition.  The
 * shared historical fixture intentionally carries only the old 0x504-byte
 * view and therefore cannot legally host V90Parameters' 0x558-byte ctor. */
static unsigned char flow_parm_[2][sizeof(V90Parameters) + 64u]
	__attribute__((aligned(8)));
static struct _tagModemParameters flow_mparm[2];

#define FLOW_PARAMS(s) ((V90Parameters *)flow_parm_[s])

/*
 * The per-side words this file plants with a per-side address.  Every other
 * word of the slot is byte-identical after `fill_pair`, so neutralising one
 * would turn a compared word into an ignored one -- v90demfix.h's own rule.
 */
static void
prog_snap(unsigned char *dst, int side)
{
	V90Demodulator *s;

	snap_dem(dst, side);
	s = (V90Demodulator *)dst;

	s->array_244 = 0;
	s->array_248 = 0;
	s->array_250 = 0;
	s->array_254 = 0;
	s->array_25c = 0;
	s->spectralVerifier.params = 0;
	s->mappingParams = 0;
	s->mappingParamsAlt = 0;
	s->trn2Designer = 0;
	s->additionalCPinfo = 0;
	s->cp = 0;
	s->mp = 0;
	s->phase4Demodulator = 0;
	s->demapper = 0;
	s->constellationDesigner = 0;
	s->autoDigitalImpDetector = 0;
	memset(&s->descrambler, 0, sizeof s->descrambler);
	/* The resampler owns three heap blocks and holds a vptr. */
	memset((unsigned char *)dst + 0x94, 0, 0xb4);
}

static void
prog_wire(int side, int trial)
{
	V90Demodulator *d = D(side);
	V90Equalizer *e = (V90Equalizer *)equ[side];
	unsigned int i;

	for (i = 0; i < PARAMS_BLL_LAST_K - PARAMS_BLL_FIRST_K + 1; i++)
		set_float(side, (int)(PARAMS_BLL_FIRST_K + i) * 4,
			  0.0625f * (float)(int)(1 + (i & 3)));
	set_int(side, PARAMS_TIMING_HIST_LEN * 4, 8);
	set_int(side, PARAMS_TIMING_HIST_PERIOD * 4, 16);

	/* What the common tail reads: the energy-drop detector and the two
	 * print periods.  All four are seeded 32-bit patterns otherwise, and
	 * a seeded threshold is a plausible NaN. */
	set_float(side, PARAMS_ENERGY_DROP_THRESHOLD * 4, 0.25f);
	set_int(side, PARAMS_NO_ENERGY_DURATION * 4, 100);
	set_int(side, PARAMS_MAX_NOF_REMOTE_RETRAINS * 4, 3);
	((V90ConnectionEvaluator *)ce[side])->params = (V90Parameters *)parm[side];
	((V90ConnectionEvaluator *)ce[side])->nofRemoteRetrains =
	    (unsigned int)(trial % 5);
	((V90ConnectionEvaluator *)ce[side])->nofV90Retrains = 0;

	d->energyDropDetectorArmed = (unsigned int)(trial & 1);
	d->noEnergyDuration = (unsigned int)(trial * 13u) % 90u;
	d->errorEnergyPrintCounter = (unsigned int)(trial * 7u) % 40u;
	d->errorEnergyPrintPeriod = 48u;
	d->timingOffsetPrintCounter = (unsigned int)(trial * 11u) % 40u;
	d->timingOffsetPrintPeriod = 64u;

	/* The prefilter's FIR: a real bank, so `process` has coefficients. */
	d->preFilter.coefficients = fir_bank;

	/* The AGC, configured the way `V90Demodulator::reset` configures it. */
	d->agc.alpha = 0.5f;
	d->agc.savedAlpha = 0.25f;
	d->agc.gain = 1.0f;
	d->agc.ref = 1.0f;
	d->agc.level = 0.5f;
	d->agc.acc = 0.0f;
	d->agc.blockLen = 16;
	d->agc.count = 16;

	/* The five heap arrays the whole chain writes through. */
	d->array_244 = a244[side];
	d->array_248 = a248[side];
	d->array_250 = a250[side];
	d->array_254 = a254[side];
	d->array_25c = a25c[side];
	d->nofResampled = 0;
	d->nofSymbols = 0;
	d->word_260 = (unsigned int)(trial % 5) + 1u;

	/* The equaliser's twelve arrays, t_v90equproc's arena shape. */
	e->linearEquCoefs = eqa[side].lecoefs;
	e->array_18 = eqa[side].a18;
	e->linearEquWindow = eqa[side].lewin;
	e->dfeWindow = eqa[side].dfewin;
	e->dfeCoefs = eqa[side].dfecoefs;
	e->array_44 = eqa[side].a44;
	e->meanErrorEnergy = eqa[side].meanerr;
	e->linearEquMmxCoefs = eqa[side].lemmx;
	e->array_d8 = eqa[side].ad8;
	e->array_ec = eqa[side].aec;
	e->dfeMmxCoefs = eqa[side].dfemmx;
	e->array_118 = eqa[side].a118;
	e->array_12c = eqa[side].a12c;
	e->block_b4 = eqa[side].b4;
	e->block_b8 = eqa[side].b8;
	e->linearEquMmxCoefsAligned = eqa[side].lemmx + 1;
	e->array_d8Aligned = eqa[side].ad8 + 1;
	e->array_ecAligned = eqa[side].aec + 1;
	e->dfeMmxCoefsAligned = eqa[side].dfemmx + 1;
	e->array_118Aligned = eqa[side].a118 + 1;
	e->array_12cAligned = eqa[side].a12c + 1;
	e->linearEquMmxCoefsSkew = e->array_d8Skew = e->array_ecSkew = 1;
	e->dfeMmxCoefsSkew = e->array_118Skew = e->array_12cSkew = 1;
	e->linearEquLength = PROG_LELEN;
	e->linearEquHistoryLength = PROG_M;
	e->dfeLength = PROG_DFELEN;
	e->mmxMode = 0;
	e->mmxArraysPresent = 0;
	e->params = (V90Parameters *)parm[side];
	e->resampler = &d->resampler;
	e->spectralVerifier = &d->spectralVerifier;
	/*
	 * THE EQUALISER'S SIX PEERS ARE LEFT AS v90demfix.h SEEDED THEM, AND
	 * THAT IS MEASURED RATHER THAN LAZY.  Wiring them real -- and forcing
	 * `state` to PHASE3 so `process` would take the arm that reads
	 * `phase3Demod->eventCode` -- was tried, and it makes the equaliser call
	 * `V90Phase3Demodulator::getDecision` once per symbol.  The old attempt
	 * failed 155 of 1,868 PERIOD checks, but F10241 establishes that every
	 * one of its seeded Phase-3 states lay above the implemented 0..0x21
	 * range, whose return is deliberately indeterminate.  That was a fixture
	 * defect, not evidence against the callee.  The separate constructed
	 * composition below supplies a real Phase-3 peer; this broad sweep keeps
	 * the cheap equaliser arm and measures its event and final outcome
	 * separately.  Finding F7513 records the original result.
	 */

	memset(&eqa[side], 0, sizeof eqa[side]);
	for (i = 0; i < PROG_EQARR; i++)
		eqa[side].lecoefs[i] = (i == 0) ? 1.0f : 0.0f;

	/* The spectral verifier: not accumulating, so `process` is cheap. */
	d->spectralVerifier.params = (V90Parameters *)parm[side];
	d->spectralVerifier.accumCount = 0;
	d->spectralVerifier.accumulating = 0;
	d->spectralVerifier.word_28 = 0;

	memset(a244[side], 0, sizeof a244[side]);
	memset(a248[side], 0, sizeof a248[side]);
	memset(a250[side], 0, sizeof a250[side]);
	memset(a254[side], 0, sizeof a254[side]);
	memset(a25c[side], 0, sizeof a25c[side]);
	memset(prog_out[side], 0, sizeof prog_out[side]);
}

/* A mapping block a demapper reset can survive: six six-level phases. */
static void
plant_mapping(unsigned char *m, int trial)
{
	unsigned int *w = (unsigned int *)m;
	int k, i;

	memset(m, 0, MAP_SLOT);
	w[0] = 12u + (unsigned int)(trial % 3);
	for (k = 0; k < 6; k++) {
		w[0x604 / 4 + k] = 8u;
		for (i = 0; i < 128; i++)
			m[128 * k + i + 4] =
			    (unsigned char)((i * 3 + k * 7 + trial) & 0x7f);
	}
	w[0x620 / 4] = 6u;			/* shaperSR: divides six    */
	w[0x624 / 4] = 0u;			/* shaperId: no shaper      */
	((float *)m)[0x628 / 4] = 0.5f;
	((float *)m)[0x62c / 4] = -0.25f;
	((float *)m)[0x630 / 4] = 0.125f;
	((float *)m)[0x634 / 4] = 0.0625f;
}

static void
prog_deep(int side, int trial)
{
	V90Demodulator *d = D(side);
	Descrambler<unsigned char, int> *ds = &d->descrambler;
	unsigned int i;

	for (i = 0; i < DMP_SLOT; i++)
		dmp_[side][i] = (unsigned char)((i * 7u + 0x11u) | 1u);
	for (i = 0; i < P4D_SLOT; i++)
		p4d_[side][i] = (unsigned char)((i * 5u + 0x23u) | 1u);
	for (i = 0; i < CPZ_SLOT; i++)
		cpz_[side][i] = (unsigned char)((i * 3u + 0x37u) | 1u);
	for (i = 0; i < ACP_SLOT; i++)
		acp_[side][i] = (unsigned char)((i * 11u + 0x41u) | 1u);
	for (i = 0; i < TDZ_SLOT; i++)
		tdz_[side][i] = (unsigned char)((i * 13u + 0x53u) | 1u);
	for (i = 0; i < sizeof mpz_[side]; i++)
		mpz_[side][i] = (unsigned char)((i * 17u + 0x61u) | 1u);
	for (i = 0; i < sizeof dscbuf[side]; i++)
		dscbuf[side][i] = (unsigned char)((i * 7u + 5u) | 1u);

	plant_mapping(map1_[side], trial);
	plant_mapping(map2_[side], trial + 1);

	d->mappingParams = MAP1(side);
	d->mappingParamsAlt = MAP2(side);
	d->trn2Designer = (V90TRN2Designer *)tdz_[side];
	d->additionalCPinfo = ACP(side);
	d->cp = (V90CP *)cpz_[side];
	d->mp = MPZ(side);
	d->phase4Demodulator = P4D(side);
	d->demapper = DMP(side);
	d->constellationDesigner = CDZ(side);
	d->autoDigitalImpDetector = &adid[side];

	/* The embedded descrambler, wired the way t_v90demod wires it. */
	ds->pLimit = dscbuf[side];
	ds->pInitOut = dscbuf[side] + 0x20;
	ds->pInitTap1 = dscbuf[side] + 0x30;
	ds->pInitTap2 = dscbuf[side] + 0x38;
	ds->pOut = dscbuf[side] + 4;
	ds->pTap1 = dscbuf[side] + 0x14;
	ds->pTap2 = dscbuf[side] + 0x1c;
	ds->tailLength = 8;

	/* The four shallow members `progress` calls on the phase 4 half. */
	P4D(side)->state = (Phase4DemodulatorState)(trial % 7);
	P4D(side)->countInState = (unsigned int)(trial * 13u);
	P4D(side)->int_0038 = trial & 1;
	P4D(side)->int_003c = (trial >> 1) & 1;
	P4D(side)->int_0040 = 0;
	P4D(side)->int_3510 = (trial >> 2) & 1;

	/* The signature the "'Problematic' ISP Modem" arm matches against. */
	MPZ(side)->Type = (char)((trial % 3) == 0 ? 1 : 2);
	MPZ(side)->Trellis = 0;
	MPZ(side)->NonLin = 0;
	MPZ(side)->Shaping = 0;
	MPZ(side)->h1Real = MPZ(side)->h1Imag = 0;
	MPZ(side)->h2Real = MPZ(side)->h2Imag = 0;
	MPZ(side)->h3Real = MPZ(side)->h3Imag = (short)((trial % 5) == 0 ? 0
									: 7);
}

/*
 * THE STATES, and every one of them is a `case` of one of the two `word_3c`
 * tables.  `progress` opens with `word_3c = equalizer->stateCount`, so the
 * arm is selected by planting the EQUALISER and not the demodulator -- which
 * is also the only spelling that can tell "it read the equaliser" from "it
 * kept what was there".
 *
 * THREE ARE OUT AND THE REASON IS THE CALLEE, NOT THE ARM.  0x12 is
 * `exitPhase3()`, whose last statement is `phase4Demodulator->reset(...)` and
 * needs the whole 0x351c receiver constructed (finding F7483); 0x19 and 0x2a
 * end in the fourteen-argument `V90ConstellationDesigner::process` over a
 * detector whose per-code tables have to be walkable.  Both are driven by
 * their own binaries -- `t_v90p4ddec` and `t_v90trn2design` -- and what is
 * NOT claimed here is that `progress` reaches them correctly.  Said in the
 * finding rather than left to be inferred from a coverage number.
 */
static const int p3_state_v[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x0b,
	0x11, 0x13, 0x14, 0x15, 0x16
};
#define N_P3 ((int)(sizeof(p3_state_v) / sizeof(p3_state_v[0])))

static const int p4_state_v[] = {
	0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x28, 0x29, 0x31, 0x33, 0x35, 0x36
};
#define N_P4 ((int)(sizeof(p4_state_v) / sizeof(p4_state_v[0])))

static const int data_state_v[] = { 0x22, 0x23, 0x25, 0x26, 0x1f };
#define N_DATA ((int)(sizeof(data_state_v) / sizeof(data_state_v[0])))

static int
run_progress(void)
{
	struct trial_args t;
	int latch, si, trial = 0;
	int sawLevel[3];
	int seen[5];
	int sawDrop = 0, sawNoDrop = 0, sawPrint = 0;
	int seenEvent[64], seenOutcome[64];
	int distinctEvent = 0, distinctOutcome = 0;
	int invalidSeededP3 = 0;

	sawLevel[0] = sawLevel[1] = sawLevel[2] = 0;
	memset(seenEvent, 0, sizeof seenEvent);
	memset(seenOutcome, 0, sizeof seenOutcome);
	seen[0] = seen[1] = seen[2] = seen[3] = seen[4] = 0;

	diff_begin("V90Demodulator::progress");

	for (latch = 0; latch < 5; latch++)
	 for (si = 0; si < N_P3 + N_P4 + N_DATA; si++) {
		unsigned int nofOut[2];
		unsigned int i;
		int side;
		int state;
		/*
		 * THE ANTI-VACUITY AXIS, and 7458 is why it is `si + latch * 3`
		 * and not either variable alone: each arm reaches its own
		 * transition at ONE state and ONE latch, so a level derived
		 * from either drives that arm at one level only.
		 */
		unsigned int lvl = (unsigned int)((si + latch * 3 + trial) % 3);

		if (si < N_P3)
			state = p3_state_v[si];
		else if (si < N_P3 + N_P4)
			state = p4_state_v[si - N_P3];
		else
			state = data_state_v[si - N_P3 - N_P4];

		t.latch = (unsigned int)latch;
		t.flag = (unsigned int)((trial >> 1) & 1);
		t.eia6 = (trial % 3) == 0 ? 1 : 6;
		t.blockByte = 0;
		t.pcmType = trial & 1;
		t.idx = trial;

		setup(trial + 900, &t);
		/* Reproduce F7513's prerequisite exactly.  The old composed
		 * experiment entered the equaliser's Phase-3 arm while leaving
		 * this seeded word as though it were a constructed state.  Every
		 * trial lands outside the implemented 0..0x21 state range, where
		 * getDecision's return is deliberately indeterminate. */
		if ((unsigned int)P3(1)->state > 0x21u)
			invalidSeededP3++;

		for (i = 0; i < FIR_TAPS; i++)
			fir_bank[i] = (i == 0) ? 1.0f : 0.0f;
		for (i = 0; i < PROG_IN; i++)
			prog_in[i] = (float)(int)((i + (unsigned)trial) % 9u)
				     * 0.25f;

		for (side = 0; side < 2; side++) {
			prog_wire(side, trial);
			prog_deep(side, trial);
		}

		/*
		 * EACH SIDE BUILDS ITS OWN SUBOBJECTS WITH ITS OWN CODE, which
		 * is what the `ref_` half of every pair below is for: one
		 * shared resampler would have the blob's `resample` read what
		 * ours left in its history, and one shared equaliser the same.
		 */
		vr_ctor(&D(0)->resampler, PROG_PHASES, 1.0f, PROG_TAPS, 0.25f,
			parm[0], 0.0f, 0);
		ref_vr_ctor(&D(1)->resampler, PROG_PHASES, 1.0f, PROG_TAPS,
			    0.25f, parm[1], 0.0f, 0);
		equ_reset(equ[0], 2);
		ref_equ_reset(equ[1], 2);
		dm_ctor(dmp_[0], 6u, parm[0], &adid[0]);
		ref_dm_ctor(dmp_[1], 6u, parm[1], &adid[1]);
		dm_reset(dmp_[0], map1_[0]);
		ref_dm_reset(dmp_[1], map1_[1]);
		cd_ctor(cdz_[0], parm[0], &D(0)->preFilter,
			&D(0)->constellationPower);
		ref_cd_ctor(cdz_[1], parm[1], &D(1)->preFilter,
			    &D(1)->constellationPower);
		td_ctor(tdz_[0], parm[0], &D(0)->constellationPower);
		ref_td_ctor(tdz_[1], parm[1], &D(1)->constellationPower);

		/*
		 * THE ARM IS PLANTED IN THE EQUALISER, NOT IN `word_3c`.
		 * `progress` copies `equalizer->stateCount` over `word_3c` on
		 * entry, so a value planted in the demodulator would be
		 * overwritten before the switch ever saw it -- and planting it
		 * here is what makes "the copy was made" observable.
		 */
		/*
		 * THE ARM IS PLANTED THROUGH THE EQUALISER'S OWN INPUT, and
		 * that is forced rather than chosen.  `V90Equalizer::process`
		 * opens with `stateCount = 0` and then sets it from
		 * `phase3Demod->eventCode` in its PHASE3 arm and from
		 * `phase4Demod->int_0028` in its RRN one, so a value planted
		 * in `stateCount` -- or in `word_3c` -- is gone before
		 * `progress` reads it.  Planting the SOURCE is also the only
		 * spelling that can tell "progress copied the equaliser's
		 * answer" from "progress kept what was there", which is the
		 * first mutation in the suite.
		 */
		for (side = 0; side < 2; side++) {
			V90Equalizer *e = (V90Equalizer *)equ[side];

			/*
			 * `state` is left where `V90Equalizer::reset` put it
			 * -- RESET -- because every other arm of `process`
			 * dereferences a peer this fixture seeds rather than
			 * builds.  See prog_wire.
			 */
			P3(side)->eventCode = (unsigned int)state;
			P4D(side)->int_0028 = state;
			e->stateCount = 0x7f;
		}
		D(0)->word_3c = D(1)->word_3c = 0x7fu;

		for (side = 0; side < 2; side++) {
			memcpy(pre_dmp[side], dmp_[side], DMP_SLOT);
			memcpy(pre_p4d[side], p4d_[side], P4D_SLOT);
			memcpy(pre_ce[side], ce[side], CE_SLOT);
			memcpy(pre_cdz[side], cdz_[side], CDZ_SLOT);
			memcpy(pre_parm[side], parm[side], PARM_SLOT);
			memcpy(pre_acp[side], acp_[side], ACP_SLOT);
		}

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;

		nofOut[0] = nofOut[1] = 0xa5a5a5a5u;

		D(0)->progress(prog_out[0], nofOut[0], prog_in, PROG_IN);
		ref_dem_progress(D(1), prog_out[1], &nofOut[1], prog_in,
				 PROG_IN);

		dsplib_debug_capture_on = 0;
		sawLevel[lvl]++;
		seen[latch]++;
		if (dsplib_debug_capture_lines(1) > 0)
			sawPrint++;

		prog_snap(cmp_a, 0);
		prog_snap(cmp_b, 1);
		diff_eq_obj_(__FILE__, __LINE__, "after progress",
			     "V90Demodulator", cmp_a, cmp_b, DEM_SLOT, trial);
		diff_eq_int("nofOut (%ld)", (long)nofOut[0], (long)nofOut[1],
			    trial);
		diff_eq_int("the resampled block (%ld)",
			    memcmp(a248[0], a248[1], sizeof a248[0]) == 0, 1,
			    trial);
		diff_eq_int("the equalised symbols (%ld)",
			    memcmp(a250[0], a250[1], sizeof a250[0]) == 0, 1,
			    trial);
		diff_eq_int("the descrambled output (%ld)",
			    memcmp(prog_out[0], prog_out[1],
				   sizeof prog_out[0]) == 0, 1, trial);
		mask_cmp(__FILE__, __LINE__, "the demapper after progress",
			 "V90Demapper", dmp_[0], dmp_[1], pre_dmp[0], pre_dmp[1],
			 DMP_SLOT, trial);
		mask_cmp(__FILE__, __LINE__, "the phase 4 demodulator after progress",
			 "V90Phase4Demodulator", p4d_[0], p4d_[1], pre_p4d[0], pre_p4d[1],
			 P4D_SLOT, trial);
		mask_cmp(__FILE__, __LINE__, "the evaluator after progress",
			 "V90ConnectionEvaluator", ce[0], ce[1], pre_ce[0],
			 pre_ce[1], CE_SLOT, trial);
		mask_cmp(__FILE__, __LINE__, "the designer after progress",
			 "V90ConstellationDesigner", cdz_[0], cdz_[1], pre_cdz[0], pre_cdz[1],
			 CDZ_SLOT, trial);
		mask_cmp(__FILE__, __LINE__,
			 "the parameter block after progress",
			 "V90Parameters", parm[0], parm[1], pre_parm[0],
			 pre_parm[1], PARM_SLOT, trial);
		mask_cmp(__FILE__, __LINE__, "the additional CP record after progress",
			 "tagV90AdditionalCPinfo", acp_[0], acp_[1], pre_acp[0], pre_acp[1],
			 ACP_SLOT, trial);
		diff_eq_int("transcript (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);

		seenEvent[(unsigned)((V90Equalizer *)equ[1])->stateCount &
			  0x3fu]++;
		seenOutcome[(unsigned)D(1)->word_3c & 0x3fu]++;
		if (D(1)->noEnergyDuration == 0)
			sawDrop++;
		else
			sawNoDrop++;

		vr_dtor(&D(0)->resampler);
		ref_vr_dtor(&D(1)->resampler);
		teardown();
		trial++;
	}

	set_level(0);

	for (si = 0; si < 64; si++) {
		if (seenEvent[si])
			distinctEvent++;
		if (seenOutcome[si])
			distinctOutcome++;
	}
	/*
	 * TWO DENOMINATORS, not one mislabeled count.  `stateCount` is the
	 * event `progress` consumed.  `word_3c` is the final outcome after the
	 * phase switch and evaluator have had permission to replace that event.
	 */
	diff_eq_int("the broad sweep produced the measured number of equalizer "
		    "events", distinctEvent, PROG_EVENTS, 0);
	diff_eq_int("the broad sweep produced the measured number of final "
		    "outcomes", distinctOutcome, PROG_OUTCOMES, 0);
	diff_eq_int("all historical Phase-3 peers were invalid seeded states",
		    invalidSeededP3, 5 * (N_P3 + N_P4 + N_DATA), 0);
	diff_eq_int("the demodulator was driven in every phase",
		    seen[0] && seen[1] && seen[2] && seen[3] && seen[4], 1, 0);
	diff_eq_int("level 0 was driven", sawLevel[0] > 0, 1, 0);
	diff_eq_int("level 1 was driven", sawLevel[1] > 0, 1, 0);
	diff_eq_int("level 2 was driven", sawLevel[2] > 0, 1, 0);
	diff_eq_int("something was printed", sawPrint > 0, 1, 0);
	diff_eq_int("the energy-drop counter was cleared somewhere",
		    sawDrop > 0, 1, 0);
	diff_eq_int("and carried somewhere", sawNoDrop > 0, 1, 0);

	return diff_end();
}

/*
 * A LEGAL PHASE-3 COMPOSITION, kept separate from the broad seeded sweep.
 * The old experiment changed the equaliser state to PHASE3 but left its
 * Phase-3 peer as random bytes.  Here each side constructs its own complete
 * parameter block, impairment detector, evaluator and Phase-3 demodulator;
 * the latter's constructor performs the real reset and owns real SD/ANSam
 * allocations.  Only then is the equaliser reset, wired and entered.
 *
 * The deliberately permissive but finite SD thresholds make one transition
 * arise from detector input.  No event is planted in either the equaliser or
 * demodulator: event 1 is therefore evidence that progress composed the
 * prefilter, AGC, resampler, equaliser and P3 decision machine successfully.
 */
static int
run_constructed_phase3(void)
{
	struct trial_args t;
	int flag, call;
	int calls = 0, sawEvent = 0, sawQuiet = 0;
	int sawV90 = 0, sawV92 = 0;

	diff_begin("V90Demodulator::progress, constructed Phase-3 chain");

	for (flag = 0; flag < 2; flag++) {
		int side;

		t.latch = 1;
		t.flag = (unsigned int)flag;
		t.eia6 = 6;
		t.blockByte = 0;
		t.pcmType = flag;
		t.idx = 0;
		setup(4000 + flag, &t);

		for (side = 0; side < 2; side++) {
			unsigned int i;

			prog_wire(side, 500 + flag);
			prog_deep(side, 500 + flag);
			memset(&flow_mparm[side], 0, sizeof flow_mparm[side]);
			flow_mparm[side].minRate = 4800;
			flow_mparm[side].maxRate = 33600;
			flow_mparm[side].connectionType = -1;
			for (i = 0; i < FIR_TAPS; i++)
				fir_bank[i] = (i == 0) ? 1.0f : 0.0f;
			for (i = 0; i < PROG_IN; i++)
				prog_in[i] = (float)(int)((i + (unsigned)flag) % 7u)
					     * 0.125f;
		}

		param_ctor(flow_parm_[0], &flow_mparm[0]);
		ref_param_ctor(flow_parm_[1], &flow_mparm[1]);

		for (side = 0; side < 2; side++) {
			V90Demodulator *d = D(side);
			V90Equalizer *e = (V90Equalizer *)equ[side];

			d->params = FLOW_PARAMS(side);
			d->preFilter.params = FLOW_PARAMS(side);
			d->preFilter.refLoop = 0;
			d->spectralVerifier.params = FLOW_PARAMS(side);
			d->spectralVerifier.accumCount = 0;
			d->spectralVerifier.accumulating = 0;
			d->spectralVerifier.word_28 = 0;
			d->sessionFlag = (unsigned int)flag;
			d->inPhase3 = 1;
			d->quickConnect = 0;
			e->params = FLOW_PARAMS(side);
		}

		vr_ctor(&D(0)->resampler, PROG_PHASES, 1.0f, PROG_TAPS, 0.25f,
			FLOW_PARAMS(0), 0.0f, 0);
		ref_vr_ctor(&D(1)->resampler, PROG_PHASES, 1.0f, PROG_TAPS,
			    0.25f, FLOW_PARAMS(1), 0.0f, 0);
		adid_ctor(&adid[0], FLOW_PARAMS(0));
		ref_adid_ctor(&adid[1], FLOW_PARAMS(1));
		ce_ctor(ce[0], FLOW_PARAMS(0));
		ref_ce_ctor(ce[1], FLOW_PARAMS(1));
		p3d_ctor(p3d[0], FLOW_PARAMS(0), &D(0)->spectralVerifier,
			  (unsigned int)flag, &adid[0]);
		ref_p3d_ctor(p3d[1], FLOW_PARAMS(1), &D(1)->spectralVerifier,
			      (unsigned int)flag, &adid[1]);

		for (side = 0; side < 2; side++) {
			V90Demodulator *d = D(side);
			V90Equalizer *e = (V90Equalizer *)equ[side];
			V90SdDetector *sd = P3(side)->sdDetector;

			d->phase3Demodulator = P3(side);
			d->connectionEvaluator =
			    (V90ConnectionEvaluator *)ce[side];
			d->autoDigitalImpDetector = &adid[side];
			e->phase3Demod = P3(side);
			/* Inactive peers are null, not seeded pseudo-objects. */
			e->phase4Demod = 0;
			e->demapper = 0;
			e->connEval = (V90ConnectionEvaluator *)ce[side];
			e->spectralVerifier = &d->spectralVerifier;
			e->preFilter = &d->preFilter;

			/* Finite, valid detector configuration: energy always clears
			 * the floor and a single positive correlation verdict is
			 * sufficient. */
			sd->thresh_08 = -1.0f;
			sd->thresh_0c = -1.0f;
			sd->value_10 = -2.0f;
			sd->limit = 1;
		}

		equ_reset(equ[0], 2);
		ref_equ_reset(equ[1], 2);
		equ_enter_p3(equ[0]);
		ref_equ_enter_p3(equ[1]);

		diff_eq_int("constructed P3 starts in WaitForSd (session %ld)",
			    (long)P3(1)->state, 0, flag);
		diff_eq_int("equalizer entered Phase3 (session %ld)",
			    (long)((V90Equalizer *)equ[1])->state, 1, flag);
		diff_eq_int("constructed receive graph is wired (session %ld)",
			    ((V90Equalizer *)equ[1])->phase3Demod == P3(1) &&
			    ((V90Equalizer *)equ[1])->connEval ==
				(V90ConnectionEvaluator *)ce[1] &&
			    P3(1)->autoDigitalImpDetector == &adid[1] &&
			    D(1)->phase3Demodulator == P3(1), 1, flag);
		diff_eq_int("constructed SD storage is valid (session %ld)",
			    P3(1)->sdDetector != 0 &&
			    P3(1)->sdDetector->history != 0 &&
			    P3(1)->sdDetector->historyLength == 12, 1, flag);

		for (call = 0; call < 4; call++) {
			unsigned int nofOut[2];
			unsigned char pre_sd[2][sizeof(V90SdDetector)];
			V90SdDetector *sd0 = P3(0)->sdDetector;
			V90SdDetector *sd1 = P3(1)->sdDetector;
			long tag = flag * 100 + call;

			memcpy(pre_equ[0], equ[0], EQU_SLOT);
			memcpy(pre_equ[1], equ[1], EQU_SLOT);
			memcpy(pre_p3d[0], p3d[0], P3D_SLOT);
			memcpy(pre_p3d[1], p3d[1], P3D_SLOT);
			memcpy(pre_vr[0], &D(0)->resampler, sizeof pre_vr[0]);
			memcpy(pre_vr[1], &D(1)->resampler, sizeof pre_vr[1]);
			memcpy(pre_ce[0], ce[0], CE_SLOT);
			memcpy(pre_ce[1], ce[1], CE_SLOT);
			memcpy(pre_flow_parm[0], flow_parm_[0],
			       sizeof flow_parm_[0]);
			memcpy(pre_flow_parm[1], flow_parm_[1],
			       sizeof flow_parm_[1]);
			memcpy(pre_sd[0], sd0, sizeof *sd0);
			memcpy(pre_sd[1], sd1, sizeof *sd1);

			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			nofOut[0] = nofOut[1] = 0xa5a5a5a5u;
			D(0)->progress(prog_out[0], nofOut[0], prog_in, PROG_IN);
			ref_dem_progress(D(1), prog_out[1], &nofOut[1], prog_in,
					 PROG_IN);
			dsplib_debug_capture_on = 0;

			prog_snap(cmp_a, 0);
			prog_snap(cmp_b, 1);
			diff_eq_obj_(__FILE__, __LINE__, "after composed progress",
				     "V90Demodulator", cmp_a, cmp_b, DEM_SLOT,
				     tag);
			diff_eq_int("composed nofOut (%ld)", (long)nofOut[0],
				    (long)nofOut[1], tag);
			diff_eq_int("composed resampled block (%ld)",
				    memcmp(a248[0], a248[1], sizeof a248[0]) == 0,
				    1, tag);
			diff_eq_int("composed prefilter/AGC block (%ld)",
				    memcmp(a244[0], a244[1], sizeof a244[0]) == 0,
				    1, tag);
			diff_eq_int("composed equalized symbols (%ld)",
				    memcmp(a250[0], a250[1], sizeof a250[0]) == 0,
				    1, tag);
			diff_eq_int("composed equalizer float output (%ld)",
				    memcmp(a254[0], a254[1], sizeof a254[0]) == 0,
				    1, tag);
			diff_eq_int("composed equalizer arrays (%ld)",
				    memcmp(&eqa[0], &eqa[1], sizeof eqa[0]) == 0,
				    1, tag);
			diff_eq_int("composed output (%ld)",
				    memcmp(prog_out[0], prog_out[1],
					   sizeof prog_out[0]) == 0, 1, tag);
			mask_cmp(__FILE__, __LINE__, "composed equalizer",
				 "V90Equalizer", equ[0], equ[1], pre_equ[0],
				 pre_equ[1], EQU_SLOT, tag);
			mask_cmp(__FILE__, __LINE__, "composed resampler",
				 "V90Resampler", (unsigned char *)&D(0)->resampler,
				 (unsigned char *)&D(1)->resampler, pre_vr[0],
				 pre_vr[1], sizeof(V90Resampler), tag);
			mask_cmp(__FILE__, __LINE__, "composed P3 demodulator",
				 "V90Phase3Demodulator", p3d[0], p3d[1],
				 pre_p3d[0], pre_p3d[1], P3D_SLOT, tag);
			mask_cmp(__FILE__, __LINE__, "composed SD detector",
				 "V90SdDetector", (unsigned char *)sd0,
				 (unsigned char *)sd1, pre_sd[0], pre_sd[1],
				 sizeof *sd0, tag);
			diff_eq_int("composed SD history (%ld)",
				    memcmp(sd0->history, sd1->history,
					   12 * sizeof(float)) == 0, 1, tag);
			mask_cmp(__FILE__, __LINE__, "composed evaluator",
				 "V90ConnectionEvaluator", ce[0], ce[1],
				 pre_ce[0], pre_ce[1], CE_SLOT, tag);
			mask_cmp(__FILE__, __LINE__, "composed parameter block",
				 "V90Parameters", flow_parm_[0], flow_parm_[1],
				 pre_flow_parm[0], pre_flow_parm[1],
				 sizeof(V90Parameters), tag);
			diff_eq_int("composed transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			if (((V90Equalizer *)equ[1])->stateCount == 1)
				sawEvent++;
			if (((V90Equalizer *)equ[1])->stateCount == 0)
				sawQuiet++;
			calls++;
		}

		if (flag)
			sawV92++;
		else
			sawV90++;

		p3d_dtor(p3d[0]);
		ref_p3d_dtor(p3d[1]);
		ce_dtor(ce[0]);
		ref_ce_dtor(ce[1]);
		adid_dtor(&adid[0]);
		ref_adid_dtor(&adid[1]);
		vr_dtor(&D(0)->resampler);
		ref_vr_dtor(&D(1)->resampler);
		param_dtor(flow_parm_[0]);
		ref_param_dtor(flow_parm_[1]);
		teardown();
	}

	diff_eq_int("both session flavours were composed", sawV90 && sawV92,
		    1, 0);
	diff_eq_int("all composed progress calls ran", calls, 8, 0);
	diff_eq_int("detector-produced event 1 was observed", sawEvent > 0, 1,
		    0);
	diff_eq_int("post-transition event 0 was observed", sawQuiet > 0, 1,
		    0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_progress();
	rc |= run_constructed_phase3();

	return rc;
}
