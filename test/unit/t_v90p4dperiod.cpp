/*
 * Period-primary silence measurement, with real construction/reset and Ed.
 * Scope: component methods, NOT a negotiated/public modem connection.
 * Inputs supplied by the environment: an eight-level mapping/calibration,
 * the evaluator's silence request, and entry to WaitForEd/WaitForCP.
 * V.92 receives synthesized CRC-bearing short CP/CPnot and Ed as samples;
 * the caller's evaluator-to-P4D request copy remains an explicit boundary.
 * No energy, measurement count, or measurement state is assigned by this test.
 * A separate producer boundary replaces the mapping/calibration with built-in
 * DIL -> ADI -> TRN2 for two aligned finite channel cases. Earlier 24-bit
 * CP/matrix cases retain their supplied environment; see producer_boundary.
 * See docs/p4d-period-fixture-audit.md for the exact boundary and blob sites.
 */
#include <stddef.h>
#include "harness.h"
#include "transcript_evidence.h"
#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"
#include "dsplib/V90DilDescriptorSettings.h"
#include "dsplib/V90TRN2Designer.h"
#include "dsplib/V90ConstellationPower.h"
#include "dsplib/V90Phase3Demodulator.h"

typedef Descrambler<unsigned char, int> PeriodDescrambler;
static const char *fault;

extern "C" {
extern unsigned int ref_dsplibs_debug_level;
#define PAIR(name, args, symbol) \
 void name args asm(symbol); void ref_##name args asm("ref_" symbol)
PAIR(param_ctor, (void *, void *), "_ZN13V90ParametersC1EP19_tagModemParameters");
PAIR(ce_ctor, (void *, void *), "_ZN22V90ConnectionEvaluatorC1EP13V90Parameters");
PAIR(adi_ctor, (void *, void *), "_ZN25V90AutoDigitalImpDetectorC1EP13V90Parameters");
PAIR(adi_reset, (void *, unsigned char, PcmType, short), "_ZN25V90AutoDigitalImpDetector5resetEh7PcmTypes");
PAIR(adi_mapping_reset, (void *), "_ZN25V90AutoDigitalImpDetector18resetLinearMappingEv");
PAIR(adi_accumulate, (void *, short, short, unsigned), "_ZN25V90AutoDigitalImpDetector25calculateLinearMeanAndVarEssj");
PAIR(adi_mean, (void *, short, short), "_ZN25V90AutoDigitalImpDetector23updateLinMappMeanAndVarEss");
PAIR(adi_second, (void *), "_ZN25V90AutoDigitalImpDetector18porcessSecondStudyEv");
PAIR(adi_max, (void *, short), "_ZN25V90AutoDigitalImpDetector17determineMaxUcodeEs");
PAIR(dil_descriptor, (void *, DilType), "_Z16setDilDescriptorP19tagV90DILdescriptor7DilType");
PAIR(p3m_ctor, (void *, void *, unsigned), "_ZN18V90Phase3ModulatorC1EP13V90Parametersj");
PAIR(p3m_reset, (void *, PcmType, unsigned char, Phase3ModulatorState, unsigned, void *, void *, void *, unsigned),
 "_ZN18V90Phase3Modulator5resetE7PcmTypeh20Phase3ModulatorStatejP5V90JdP5V92JdPK19tagV90DILdescriptorj");
PAIR(p3m_dtor, (void *), "_ZN18V90Phase3ModulatorD1Ev");
PAIR(p3d_ctor, (void *, void *, void *, unsigned, void *), "_ZN20V90Phase3DemodulatorC1EP13V90ParametersP19V90SpectralVerifierjP25V90AutoDigitalImpDetector");
PAIR(p3d_reset, (void *, PcmType, unsigned char, Phase3DemodulatorState, unsigned, void *, void *, void *, short, short, float, unsigned), "_ZN20V90Phase3Demodulator5resetE7PcmTypeh22Phase3DemodulatorStatejP5V90JdP5V92JdP19tagV90DILdescriptorssfj");
PAIR(p3d_dtor, (void *), "_ZN20V90Phase3DemodulatorD1Ev");
PAIR(jd_ctor, (void *, void *), "_ZN5V90JdC1EP13V90Parameters");
PAIR(p3m_exitjd, (void *), "_ZN18V90Phase3Modulator6exitJdEv");
int p3_symbol(void *) asm("_ZN18V90Phase3Modulator14generateSymbolEv");
int ref_p3_symbol(void *) asm("ref__ZN18V90Phase3Modulator14generateSymbolEv");
int p3_decision(void *, float) asm("_ZN20V90Phase3Demodulator11getDecisionEf");
int ref_p3_decision(void *, float) asm("ref__ZN20V90Phase3Demodulator11getDecisionEf");
PAIR(trn_ctor, (void *, void *, void *), "_ZN15V90TRN2DesignerC1EP13V90ParametersP21V90ConstellationPower");
PAIR(power_ctor, (void *), "_ZN21V90ConstellationPowerC1Ev");
int ce_evaluate(void *) asm("_ZN22V90ConnectionEvaluator18evaluateConnectionEv");
int ref_ce_evaluate(void *) asm("ref__ZN22V90ConnectionEvaluator18evaluateConnectionEv");
int dil_generate(void *) asm("_ZN18V90Phase3Modulator11generateDILEv");
int ref_dil_generate(void *) asm("ref__ZN18V90Phase3Modulator11generateDILEv");
short trn_design(void *, void *, short (*)[128], short (*)[128], unsigned char (*)[128], short *, PcmType, PcmType, short, unsigned char *, unsigned, unsigned char, V90SpecialSpectralConditions)
 asm("_ZN15V90TRN2Designer13V90TRN2DesignEP16V90MappingParamsPA128_sS3_PA128_hPs7PcmTypeS7_sPhjh28V90SpecialSpectralConditions");
short ref_trn_design(void *, void *, short (*)[128], short (*)[128], unsigned char (*)[128], short *, PcmType, PcmType, short, unsigned char *, unsigned, unsigned char, V90SpecialSpectralConditions)
 asm("ref__ZN15V90TRN2Designer13V90TRN2DesignEP16V90MappingParamsPA128_sS3_PA128_hPs7PcmTypeS7_sPhjh28V90SpecialSpectralConditions");
PAIR(cp_ctor, (void *), "_ZN5V90CPC1Ev");
PAIR(cp_reset, (void *), "_ZN5V90CP5resetEv");
PAIR(cp_dtor, (void *), "_ZN5V90CPD1Ev");
PAIR(mp_ctor, (void *), "_ZN5V90MPC1Ev");
PAIR(mp_reset, (void *), "_ZN5V90MP5resetEv");
PAIR(dsc_ctor, (void *, unsigned, unsigned, unsigned), "_ZN11DescramblerIhiEC1Ejjj");
PAIR(dsc_reset, (void *, unsigned char), "_ZN11DescramblerIhiE5resetEh");
PAIR(dsc_dtor, (void *), "_ZN11DescramblerIhiED1Ev");
PAIR(dm_ctor, (void *, unsigned, void *, void *), "_ZN11V90DemapperC1EjP13V90ParametersP25V90AutoDigitalImpDetector");
PAIR(dm_reset, (void *, void *), "_ZN11V90Demapper5resetEP16V90MappingParams");
PAIR(dm_dtor, (void *), "_ZN11V90DemapperD1Ev");
PAIR(p4_ctor, (void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, unsigned),
 "_ZN20V90Phase4DemodulatorC1EP16V90MappingParamsS1_P11V90DemapperP5V90CPP5V90MPP11DescramblerIhiEP22V90ConnectionEvaluatorP13V90ParametersP20V90Phase3DemodulatorP25V90AutoDigitalImpDetectorj");
PAIR(p4_reset, (void *, unsigned char, Phase4DemodulatorState, unsigned, unsigned),
 "_ZN20V90Phase4Demodulator5resetEh22Phase4DemodulatorStatejj");
PAIR(p4_rrn, (void *), "_ZN20V90Phase4Demodulator13resetBeforRRNEv");
PAIR(p4_ed, (void *), "_ZN20V90Phase4Demodulator14enterWaitForEdEv");
PAIR(p4_cp, (void *), "_ZN20V90Phase4Demodulator14enterWaitForCPEv");
PAIR(p4_dtor, (void *), "_ZN20V90Phase4DemodulatorD1Ev");
int decision(void *, short) asm("_ZN20V90Phase4Demodulator11getDecisionEs");
int ref_decision(void *, short) asm("ref__ZN20V90Phase4Demodulator11getDecisionEs");
short dm_hard(void *, short) asm("_ZN11V90Demapper12hardDecisionEs");
short ref_dm_hard(void *, short) asm("ref__ZN11V90Demapper12hardDecisionEs");
#undef PAIR
}

/* Raw slots permit the actual C1 entry on each side; guards are not objects. */
#define SLOT(name, type) \
 static unsigned char name[2][sizeof(type) + 32] __attribute__((aligned(8)))
SLOT(p4, V90Phase4Demodulator);
SLOT(dm, V90Demapper);
SLOT(cp, V90CP);
SLOT(mp, V90MP);
SLOT(ds, PeriodDescrambler);
SLOT(pa, V90Parameters);
SLOT(ce, V90ConnectionEvaluator);
SLOT(ad, V90AutoDigitalImpDetector);
SLOT(ma, V90MappingParams);
SLOT(mb, V90MappingParams);
SLOT(host, _tagModemParameters);
SLOT(gen, V90Phase3Modulator);
SLOT(desc, tagV90DILdescriptor);
SLOT(trn, V90TRN2Designer);
SLOT(power, V90ConstellationPower);
SLOT(p3, V90Phase3Demodulator);
SLOT(jdt, V90Jd);
SLOT(jdr, V90Jd);
#undef SLOT
#define P(s) (*(V90Phase4Demodulator *)p4[s])
#define D(s) (*(V90Demapper *)dm[s])
#define A(s) (*(V90AutoDigitalImpDetector *)ad[s])
#define M(s) (*(V90MappingParams *)ma[s])
#define B(s) (*(V90MappingParams *)mb[s])
#define C(s) (*(V90ConnectionEvaluator *)ce[s])
#define PARAM(s) (*(V90Parameters *)pa[s])
#define CALL(name, s, args) do { \
 if (getenv("DSPLIB_P4D_PERIOD_TRACE")) { printf("side %d: %s\n", s, #name); fflush(stdout); } \
 if (s) ref_##name args; else name args; } while (0)

static void raw_eq(const char *what, const void *a, const void *b, unsigned n, long tag)
{
 diff_eq_obj_(__FILE__, __LINE__, what, "raw bytes", a, b, n, tag);
}

/* Validate the fixture's entire active mapping before using any code as an
 * ADI/seen index, or handing it to a production decoder. Scan a fixed eight
 * entries: an invalid output size must not control this observer's bounds. */
static int mapping_in_range(int s, long tag)
{
 int valid = 1;
 for (unsigned p = 0; p < 6; ++p) {
  diff_eq_int("mapping size is eight before use", M(s).constellationSize[p], 8, tag);
  if (M(s).constellationSize[p] != 8) valid = 0;
  for (unsigned i = 0; i < 8; ++i) {
   unsigned c = M(s).constellation[p][i];
   unsigned codec = M(s).codecConstellation[p][i];
   diff_eq_int("mapping code below 128 before use", c < 128, 1, tag);
   diff_eq_int("mapping codec code below 128 before use", codec < 128, 1, tag);
   if (c >= 128 || codec >= 128) {
    printf("mapping range rejection: side=%d phase=%u index=%u code=%u codec=%u\n", s, p, i, c, codec);
    valid = 0;
   }
  }
 }
 return valid;
}

static short mapping_level(int s, unsigned phase, unsigned i, long tag)
{
 unsigned c = M(s).constellation[phase][i];
 diff_eq_int("mapping level code below 128 before dereference", c < 128, 1, tag);
 if (c >= 128) exit(diff_end());
 return A(s).linMapp[phase][c];
}

static void guards(long tag)
{
 unsigned char want[32];
 memset(want, 0x69, sizeof want);
 for (int s = 0; s < 2; ++s) {
#define GUARD(name, type) raw_eq(#name " guard", name[s] + sizeof(type), want, 32, tag * 2 + s)
  GUARD(p4, V90Phase4Demodulator); GUARD(dm, V90Demapper);
  GUARD(cp, V90CP); GUARD(mp, V90MP); GUARD(ds, PeriodDescrambler);
  GUARD(pa, V90Parameters); GUARD(ce, V90ConnectionEvaluator);
  GUARD(ad, V90AutoDigitalImpDetector);
  GUARD(ma, V90MappingParams); GUARD(mb, V90MappingParams);
  GUARD(host, _tagModemParameters);
#undef GUARD
 }
}

/* New boundary: built-in DIL, aligned noiseless short-sample channel, ADI
 * accumulation/finalization, TRN2 design. This does not execute P3D's timed
 * study machine or prove preceding synchronization, codec negotiation, or
 * evaluator history. mode 1 = unity gain, 2 = half gain, 3 = missing samples
 * (synthetic fault, not a legal completed calibration).
 */
static unsigned producer_samples[2];
static short producer_result[2];
static unsigned char calibrated[2][sizeof(V90AutoDigitalImpDetector)] __attribute__((aligned(8)));
static short generated[2][40000];
static unsigned char manual_calibration[3][2][sizeof(V90AutoDigitalImpDetector)] __attribute__((aligned(8)));
static unsigned char manual_mapping[3][2][sizeof(V90MappingParams)] __attribute__((aligned(8)));
static void produce_mapping(int s, unsigned mode)
{
 unsigned char original_params[sizeof pa[0]];
 memcpy(original_params, pa[s], sizeof original_params);
#define INIT_PRODUCER(name, type) memset(name[s], 0, sizeof(type)); memset(name[s] + sizeof(type), 0x69, 32)
 INIT_PRODUCER(gen, V90Phase3Modulator);
 INIT_PRODUCER(desc, tagV90DILdescriptor);
 INIT_PRODUCER(trn, V90TRN2Designer);
 INIT_PRODUCER(power, V90ConstellationPower);
#undef INIT_PRODUCER
 CALL(dil_descriptor, s, (desc[s], DIL_TYPE_ADI));
 CALL(p3m_ctor, s, (gen[s], pa[s], 0));
 CALL(p3m_reset, s, (gen[s], PCM_TYPE_MU_LAW, 0, P3M_STATE_DIL, 0, 0, 0, desc[s], 0));
 V90Phase3Modulator *g = (V90Phase3Modulator *)gen[s];
 int omit = mode == 3 || (mode == 1 && fault && !strcmp(fault, "calibration-missing"));
 unsigned n = 0;
 do {
  short symbol = (short)(s ? ref_dil_generate(gen[s]) : dil_generate(gen[s]));
  generated[s][n] = symbol;
  /* Code association comes from the generated signed symbol, not dilPcmCode:
   * segment-reference symbols can have a DIFFERENT code (2b092/2b180). */
  if (!omit)
   CALL(adi_accumulate, s, (ad[s], (short)(mode == 2 ? symbol / 2 : symbol), symbol, n % 6));
  ++n;
 } while ((g->dilIndex != 0 || g->segmentPos != 0) && n < 40000);
 producer_samples[s] = n;
 diff_eq_int("built-in DIL sample denominator", n, 32280, s);
 diff_eq_int("DIL complete cycle inside bound", n > 0 && n < 40000, 1, s);
 diff_eq_int("DIL cycle ends at phase boundary", n % 6, 0, s);
 for (short phase = 0; phase < 6; ++phase)
  for (short code = 0; code < 128; ++code)
   CALL(adi_mean, s, (ad[s], phase, code));
 CALL(adi_second, s, (ad[s]));
 /* 116 is the highest code in the built-in ADI descriptor, checked below;
  * no arbitrary out-of-range designer limit or planted usable mask. */
 unsigned top = 0;
 tagV90DILdescriptor *d = (tagV90DILdescriptor *)desc[s];
 for (unsigned i = 0; i < d->dilCount; ++i) if (d->dilCode[i] > top) top = d->dilCode[i];
 diff_eq_int("built-in DIL highest code", top, 116, s);
 CALL(adi_max, s, (ad[s], (short)top));
 memcpy(calibrated[s], ad[s], sizeof(V90AutoDigitalImpDetector));
 CALL(power_ctor, s, (power[s]));
 CALL(trn_ctor, s, (trn[s], pa[s], power[s]));
 diff_eq_int("designer parameter identity", ((V90TRN2Designer *)trn[s])->params == (V90Parameters *)pa[s], 1, s);
 diff_eq_int("designer power identity", ((V90TRN2Designer *)trn[s])->power == (V90ConstellationPower *)power[s], 1, s);
 /* Caller-interface choices: both laws mu, Jd lookahead zero, normal
  * spectrum, phase2 maxTxPower zero -> argument 1 (exitPhase3). */
 if (s) producer_result[s] = ref_trn_design(trn[s], ma[s], A(s).linMapp, A(s).linMappAlt,
   A(s).usableMask, A(s).altRbsFlag, PCM_TYPE_MU_LAW, PCM_TYPE_MU_LAW,
   A(s).unSuspectedPhase, A(s).maxUcode, 0, 1, (V90SpecialSpectralConditions)0);
 else producer_result[s] = trn_design(trn[s], ma[s], A(s).linMapp, A(s).linMappAlt,
   A(s).usableMask, A(s).altRbsFlag, PCM_TYPE_MU_LAW, PCM_TYPE_MU_LAW,
   A(s).unSuspectedPhase, A(s).maxUcode, 0, 1, (V90SpecialSpectralConditions)0);
 raw_eq("designer leaves ADI inputs intact", calibrated[s], ad[s], sizeof(V90AutoDigitalImpDetector), s);
 CALL(p3m_dtor, s, (gen[s]));
 raw_eq("producer preserves default parameters and guard", pa[s], original_params, sizeof original_params, s);
}

static void construct(unsigned session, unsigned producer = 0)
{
 harness_alloc_reset();
 if (producer) dsplib_debug_capture_reset();
 for (int s = 0; s < 2; ++s) {
#define INIT(name, type) memset(name[s], 0, sizeof(type)); memset(name[s] + sizeof(type), 0x69, 32)
  INIT(p4, V90Phase4Demodulator); INIT(dm, V90Demapper);
  INIT(cp, V90CP); INIT(mp, V90MP); INIT(ds, PeriodDescrambler);
  INIT(pa, V90Parameters); INIT(ce, V90ConnectionEvaluator);
  INIT(ad, V90AutoDigitalImpDetector);
  INIT(ma, V90MappingParams); INIT(mb, V90MappingParams);
  INIT(host, _tagModemParameters);
#undef INIT
  /* Whole P4D storage poison: do not silently inherit zeroed energies. */
  memset(p4[s], 0xa5, sizeof(V90Phase4Demodulator));
  ((_tagModemParameters *)host[s])->minRate = 28000;
  ((_tagModemParameters *)host[s])->maxRate = 56000;
  CALL(param_ctor, s, (pa[s], host[s]));
  CALL(ce_ctor, s, (ce[s], pa[s]));
  if (producer) {
   /* A real evaluator call with only ctor history is insufficient. Do not
    * plant a sample count or designer thresholds to make it request RRN. */
   unsigned char expected[sizeof ce[0]] __attribute__((aligned(8)));
   memcpy(expected, ce[s], sizeof expected);
   ((V90ConnectionEvaluator *)expected)->initDmin = 0;
   int verdict = s ? ref_ce_evaluate(ce[s]) : ce_evaluate(ce[s]);
   diff_eq_int("empty evaluator rejects request production", verdict, 0, s);
   diff_eq_int("empty evaluator silence request stays zero", C(s).silenceRrnRequest, 0, s);
   raw_eq("empty evaluator only latches initial distance", ce[s], expected, sizeof expected, s);
  }
  CALL(adi_ctor, s, (ad[s], pa[s]));
  CALL(adi_reset, s, (ad[s], 0, PCM_TYPE_MU_LAW, 0));
  CALL(adi_mapping_reset, s, (ad[s]));
  /* Supplied component inputs, NOT negotiated/calibrated modem outputs.
   * The reset methods establish PCM law and no-alt-RBS flags; the eight
   * levels below still assume a calibration not produced by DIL training.
   * Calling an accumulator with invented code/sample associations would
   * merely move that assumption, so keep it visible here. */
  C(s).silenceRrnRequest = 1;
  if (producer) {
   dsplib_debug_capture_on = 1;
   produce_mapping(s, producer);
   dsplib_debug_capture_on = 0;
  } else {
   M(s).word_0 = 24;
   for (int phase = 0; phase < 6; ++phase) {
    diff_eq_int("ADI reset disables alternate RBS", A(s).altRbsFlag[phase], 0, phase);
    M(s).constellationSize[phase] = 8;
    for (int code = 0; code < 8; ++code) {
     M(s).constellation[phase][code] = (unsigned char)code;
     M(s).codecConstellation[phase][code] = (unsigned char)code;
     A(s).linMapp[phase][code] = (short)(300 - 20 * code);
     A(s).linMappAlt[phase][code] = (short)(300 - 20 * code);
    }
   }
  }
  memcpy(mb[s], ma[s], sizeof(V90MappingParams));
  if (!mapping_in_range(s, producer)) exit(diff_end());
  diff_eq_int("ADI reset selects mu law", A(s).pcmType, PCM_TYPE_MU_LAW, s);
  CALL(cp_ctor, s, (cp[s]));
  CALL(mp_ctor, s, (mp[s]));
  CALL(dsc_ctor, s, (ds[s], 18, 23, 99));
  CALL(dsc_reset, s, (ds[s], 0));
  CALL(dm_ctor, s, (dm[s], 72, pa[s], ad[s]));
  CALL(p4_ctor, s, (p4[s], ma[s], mb[s], dm[s], cp[s], mp[s], ds[s],
                    ce[s], pa[s], (void *)0, ad[s], session));
  CALL(p4_reset, s, (p4[s], 0, P4D_STATE_WAIT_FOR_RI, 0, 0));
  diff_eq_int("P4D parameter identity", P(s).params == (V90Parameters *)pa[s], 1, s);
  diff_eq_int("P4D demapper identity", P(s).demapper == (V90Demapper *)dm[s], 1, s);
  diff_eq_int("P4D MP identity", P(s).mp == (V90MP *)mp[s], 1, s);
  diff_eq_int("P4D CP identity", P(s).cp == (V90CP *)cp[s], 1, s);
  diff_eq_int("P4D descrambler identity", P(s).descrambler == (PeriodDescrambler *)ds[s], 1, s);
  diff_eq_int("P4D evaluator identity", P(s).connectionEvaluator == (V90ConnectionEvaluator *)ce[s], 1, s);
  diff_eq_int("reset state", P(s).state, P4D_STATE_WAIT_FOR_RI, s);
  diff_eq_int("reset count", P(s).countInState, 0, s);
  diff_eq_int("default wait", PARAM(s).RRN_SILENCE_WAIT_BEFORE_ECHO_CALC, 132, s);
  diff_eq_int("default period", PARAM(s).RRN_SILENCE_ECHO_CALC_PERIOD, 240, s);
  diff_eq_int("default SCR", PARAM(s).RRN_SILENCE_SCR_LENGTH, 2500, s);
  float threshold = 2.0f;
  raw_eq("default threshold", &PARAM(s).RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE,
         &threshold, sizeof threshold, s);
 }
 if (producer) {
  diff_eq_int("producer transcript exact and complete", transcript_exact("p4d.producer", producer), 1, producer);
  raw_eq("producer descriptor", desc[0], desc[1], sizeof desc[0], producer);
  raw_eq("producer generated samples", generated[0], generated[1], 32280 * sizeof(short), producer);
  raw_eq("producer generator DIL state and guard", gen[0] + 0x54, gen[1] + 0x54,
         sizeof gen[0] - 0x54, producer);
  raw_eq("producer ADI before parameter pointer", calibrated[0], calibrated[1], 0x2814, producer);
  raw_eq("producer ADI after parameter pointer", calibrated[0] + 0x2818, calibrated[1] + 0x2818,
         sizeof(V90AutoDigitalImpDetector) - 0x2818, producer);
  raw_eq("producer mapping", ma[0], ma[1], sizeof ma[0], producer);
  diff_eq_int("producer sample counts", producer_samples[0], producer_samples[1], producer);
  diff_eq_int("producer design results", producer_result[0], producer_result[1], producer);
  raw_eq("producer power state after pointer", power[0] + 4, power[1] + 4, sizeof power[0] - 4, producer);
  for (int s = 0; s < 2; ++s)
   diff_eq_int("power retains own codec constellation", ((V90ConstellationPower *)power[s])->constellation == M(s).codecConstellation[5], 1, s);
 }
 guards(0);
}

/* Exactly the peers/heaps the measurement must leave alone, snapshots per side.
 * No address masking: comparisons are against each object's own preimage.
 */
static void *heap[128];
static unsigned char *heap_before[128];
static unsigned heap_size[128];
static int nheap;
static unsigned char pa_before[sizeof pa], ce_before[sizeof ce], ad_before[sizeof ad];
static unsigned char cp_before[sizeof cp], mp_before[sizeof mp], ds_before[sizeof ds];
static unsigned char ma_before[sizeof ma], mb_before[sizeof mb];
static unsigned char host_before[sizeof host];
static unsigned char gen_before[sizeof gen], desc_before[sizeof desc];
static unsigned char trn_before[sizeof trn], power_before[sizeof power];
static unsigned char p3_before[sizeof p3], jdt_before[sizeof jdt], jdr_before[sizeof jdr];

static void freeze_peers(void)
{
#define SAVE(name) memcpy(name##_before, name, sizeof name)
 SAVE(pa); SAVE(ce); SAVE(ad); SAVE(cp); SAVE(mp); SAVE(ds); SAVE(ma); SAVE(mb); SAVE(host);
 SAVE(gen); SAVE(desc); SAVE(trn); SAVE(power);
 SAVE(p3); SAVE(jdt); SAVE(jdr);
#undef SAVE
 nheap = harness_alloc_live_set(heap, 128);
 if (nheap <= 0 || nheap > 128) { fprintf(stderr, "invalid heap denominator %d\n", nheap); exit(1); }
 for (int i = 0; i < nheap; ++i) {
  heap_size[i] = harness_alloc_reqsize(heap[i]);
  if (!heap_size[i]) exit(1);
  heap_before[i] = (unsigned char *)malloc(heap_size[i]);
  if (!heap_before[i]) exit(1);
  memcpy(heap_before[i], heap[i], heap_size[i]);
 }
}

static void thaw_peers(long tag)
{
#define CHECK(name) raw_eq(#name " immutable during measurement", name, name##_before, sizeof name, tag)
 CHECK(pa); CHECK(ce); CHECK(ad); CHECK(cp); CHECK(mp); CHECK(ds); CHECK(ma); CHECK(mb); CHECK(host);
 CHECK(gen); CHECK(desc); CHECK(trn); CHECK(power);
 CHECK(p3); CHECK(jdt); CHECK(jdr);
#undef CHECK
 void *live[128];
 diff_eq_int("measurement allocation count unchanged", harness_alloc_live_set(live, 128), nheap, tag);
 for (int i = 0; i < nheap; ++i) {
  diff_eq_int("measurement allocation retained", harness_alloc_reqsize(heap[i]), heap_size[i], tag);
  raw_eq("measurement heap immutable", heap[i], heap_before[i], heap_size[i], tag);
  free(heap_before[i]);
 }
}

/* Message-level stimulus, independent of the receiver's state/expected flags.
 * Short CP: 17 ones, framing zero, type=1, reserved zeros, supplied peer SUV,
 * acknowledgement bit, framing zero, CCITT CRC, zero padding to 24 bits.
 * Blob: infoToBits 52240..522bd; bitsToInfo validates CRC before reporting.
 * This is NOT negotiated mapping data or a simulated remote modem.
 */
/* Optional matrix arguments: slot -1 is absent (72 zero bits), 0/1 is
 * CP/CPnot, 2/3 is that message with its first CRC bit corrupted. */
static void cp_samples(short *samples, unsigned char *wire, int control,
                       unsigned peer = 0, int first = 0, int second = 1)
{
 unsigned char message[768];
 memset(message, 0, sizeof message);
 if (control != 2) {
  for (unsigned slot = 0; slot < 2; ++slot) {
   int kind = slot ? second : first;
   if (kind < 0) continue;
   unsigned char *b = message + 72 * slot;
   memset(b, 1, 17);
   b[18] = 1;
   b[32] = (unsigned char)peer;
   b[33] = (unsigned char)(kind & 1);
   unsigned crc = 0xffff;
   for (unsigned i = 18; i < 34; ++i) {
    unsigned feedback = ((crc >> 15) ^ b[i]) & 1;
    crc = ((crc << 1) & 0xffff) ^ (feedback ? 0x1021 : 0);
   }
   for (unsigned i = 0; i < 16; ++i) b[35 + i] = (crc >> (15 - i)) & 1;
   if (control == 1 || kind >= 2) b[35] ^= 1; /* message only */
  }
 }
 /* Invert the initialized x^18+x^23 descrambler, then the unshaped
  * six-sign/18-modulus-bit frame. Eight levels give six radix-eight digits.
  * No receiver field is consulted when constructing this sample stream.
  */
 for (unsigned i = 0; i < sizeof message; ++i)
  wire[i] = message[i] ^ (i >= 18 ? wire[i - 18] : 0) ^
                        (i >= 23 ? wire[i - 23] : 0);
 unsigned sign = 0;
 for (unsigned frame = 0; frame < sizeof message / 24; ++frame) {
  const unsigned char *w = wire + 24 * frame;
  for (unsigned i = 0; i < 6; ++i) {
   unsigned code = w[6 + 3*i] | (w[7 + 3*i] << 1) | (w[8 + 3*i] << 2);
   int magnitude = 300 - 20 * (int)code;
   sign ^= w[i];
   samples[6*frame + i] = (short)(sign ? magnitude : -magnitude);
  }
 }
}

static int enter_silence(unsigned session, long tag, int control = 0, unsigned producer = 0)
{
 unsigned char old_after[2][4], old_before[2][4];
 short samples[192];
 unsigned char wire[768];
 int injected_crc = session && !control && fault && !strcmp(fault, "cp-crc");
 if (session) cp_samples(samples, wire, injected_crc ? 1 : control);
 for (int s = 0; s < 2; ++s) {
  memcpy(old_after[s], &P(s).errorEnergyAfterEC, 4);
  memcpy(old_before[s], &P(s).errorEnergyBeforeEC, 4);
  if (!mapping_in_range(s, tag)) exit(diff_end());
  CALL(dm_reset, s, (dm[s], ma[s]));
  CALL(dsc_reset, s, (ds[s], 0));
  CALL(p4_rrn, s, (p4[s]));
  if (!session) {
   CALL(mp_reset, s, (mp[s]));
   ((V90MP *)mp[s])->groupSize = M(s).word_0;
   CALL(p4_ed, s, (p4[s]));
  } else {
   CALL(cp_reset, s, (cp[s]));
   ((V90CP *)cp[s])->groupSize = M(s).word_0;
   CALL(p4_cp, s, (p4[s]));
   /* Caller boundary: V90Demodulator::progress at .text+1cc01 copies
    * evaluator +90 to P4D +40 after resetBeforRRN. We supply that exact
    * interface input, not a CP acceptance result. Evaluator provenance
    * remains bounded separately in the audit. */
   P(s).int_0040 = C(s).silenceRrnRequest;
   diff_eq_int("CP acceptance initially clear", P(s).uchar_0030, 0, tag);
   diff_eq_int("CP handshake initially clear", P(s).int_0044, 0, tag);
  }
 }
  dsplib_debug_capture_reset();
 dsplib_debug_capture_on = 1;
 unsigned calls;
 unsigned accepted[2] = {0, 0};
 for (calls = 0; calls < 192; ++calls) {
  short sample = session ? samples[calls] : -300;
  short sample_ref = sample;
  if (producer) {
   sample = (short)-mapping_level(0, calls % 6, 0, tag + calls);
   sample_ref = (short)-mapping_level(1, calls % 6, 0, tag + calls);
   diff_eq_int("producer Ed input samples agree", sample, sample_ref, tag + calls);
  }
  int a = decision(p4[0], sample), b = ref_decision(p4[1], sample_ref);
  diff_eq_int("Ed input decision", a, b, tag + calls);
  diff_eq_int("Ed state agrees", P(0).state, P(1).state, tag + calls);
  diff_eq_int("Ed count agrees", P(0).countInState, P(1).countInState, tag + calls);
  diff_eq_int("Ed progress agrees", P(0).int_0028, P(1).int_0028, tag + calls);
  diff_eq_int("Ed cursor agrees", D(0).rbsFramePosition, D(1).rbsFramePosition, tag + calls);
  if (producer && calls % 6 == 5) for (int s = 0; s < 2; ++s) {
   unsigned char zeros[23] = {0};
   diff_eq_int("producer default shaped frame size", P(s).nbits, 23, tag + calls);
   raw_eq("producer Ed wire zeros", P(s).bits, zeros, sizeof zeros, tag + calls);
  }
  if (session) for (int s = 0; s < 2; ++s) {
   if (calls % 6 == 5) {
    diff_eq_int("CP frame bit count", P(s).nbits, 24, tag + calls);
    raw_eq("CP synthesized wire recovered", P(s).bits, wire + (calls / 6) * 24, 24, tag + calls);
   }
   if (P(s).int_0028 == 0x31 || P(s).int_0028 == 0x33) {
    ++accepted[s];
    diff_eq_int("CP acceptance result from decoder", P(s).uchar_0030, 1, tag + calls);
    diff_eq_int("CP enables handshake", P(s).int_0044, 1, tag + calls);
    diff_eq_int("CP peer SUV decoded zero", P(s).uint_004c, 0, tag + calls);
    diff_eq_int("CP then CPnot progress", P(s).int_0028, accepted[s] == 1 ? 0x31 : 0x33, tag + calls);
   }
  }
  if (P(0).state == P4D_STATE_SILENCE && P(1).state == P4D_STATE_SILENCE) break;
 }
 dsplib_debug_capture_on = 0;
 if (session) for (int s = 0; s < 2; ++s)
  diff_eq_int("two CRC-validated CP messages", accepted[s], control ? 0 : 2, tag);
 if (control) {
  diff_eq_int("CP rejection observation bound", calls, 192, tag);
  diff_eq_int("CP rejection transcript exact", transcript_exact("p4d.period.CP.reject", tag), 1, tag);
  for (int s = 0; s < 2; ++s) {
   diff_eq_int("missing/corrupt CP withholds Ed entry", P(s).state, P4D_STATE_WAIT_FOR_V90CP, tag);
   diff_eq_int("rejected CP acceptance stays clear", P(s).uchar_0030, 0, tag);
   diff_eq_int("rejected CP handshake stays clear", P(s).int_0044, 0, tag);
   diff_eq_int("bad CRC diagnostic", strstr(dsplib_debug_capture_text(s), "bad CRC") != 0, control == 1, tag);
   raw_eq("rejected CP leaves before energy untouched", &P(s).errorEnergyBeforeEC, old_before[s], 4, tag);
   raw_eq("rejected CP leaves after energy untouched", &P(s).errorEnergyAfterEC, old_after[s], 4, tag);
  }
  guards(tag);
  printf("period fixture: CP control=%s calls=%u/192 accepted=%u/2 no Ed entry\n",
         control == 1 ? "corrupt" : "missing", calls, accepted[1]);
  return 0;
 }
 diff_eq_int("Ed reached silence within bound", calls < 192, 1, tag);
 diff_eq_int("Ed transcript exact", transcript_exact("p4d.period.Ed", tag), 1, tag);
 for (int s = 0; s < 2; ++s) {
  diff_eq_int("Ed diagnostic nonempty", dsplib_debug_capture_size(s) > 0, 1, tag);
  diff_eq_int("Ed initialized state", P(s).state, P4D_STATE_SILENCE, tag);
  diff_eq_int("Ed initialized count", P(s).countInState, 0, tag);
  diff_eq_int("Ed progress", P(s).int_0028, session ? 0x35 : 0x1c, tag);
  float zero = 0.0f;
  raw_eq("Ed initialized before energy", &P(s).errorEnergyBeforeEC, &zero, 4, tag);
  raw_eq("Ed preserves previous after energy until cancellation wait", &P(s).errorEnergyAfterEC, old_after[s], 4, tag);
 }
 guards(tag);
 printf("%s fixture: session=%u Ed calls=%u/192 scope=%s\n", producer ? "producer" : "period", session, calls < 192 ? calls + 1 : calls,
        session ? "sample-driven-short-CP-component" : "component-method");
 return calls < 192;
}

static const short inputs[][2] = {
 {2, 1}, {0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 1}
};

/* Independent-review follow-up: acceptance is not the same observation as
 * handshake enable or Ed entry. All 40 cells start with fresh real objects.
 * +3c=1/+48=0 come from resetBeforRRN; only the local request remains a
 * supplied evaluator/caller input. No CP result or energy is planted.
 */
static void cp_entry_matrix(void)
{
 static const struct {
  const char *name;
  int first, second;
  unsigned end_call;
 } cases[] = {
  {"pair", 0, 1, 48}, {"CP-only", 0, -1, 30}, {"CPnot-only", 1, -1, 30},
  {"reverse", 1, 0, 48}, {"bad-CP", 2, 1, 48}, {"bad-CPnot", 0, 3, 48},
  {"omit-CP", -1, 1, 48}, {"omit-CPnot", 0, -1, 30},
  {"both-bad", 2, 3, 192}, {"neither", -1, -1, 192}
 };
 unsigned cells = 0, total_calls = 0, peer_writes = 0;
 for (unsigned local = 0; local < 2; ++local)
  for (unsigned peer = 0; peer < 2; ++peer)
   for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
    long tag = 2000000L + 1000L * cells++;
    short samples[192];
    unsigned char wire[768], old_before[2][4], old_after[2][4];
    unsigned old_peer[2];
    unsigned accepted[2] = {0, 0}, at[2][2] = {{0, 0}, {0, 0}};
    unsigned ack[2][2] = {{0, 0}, {0, 0}};
    unsigned expected_ack[2], expected_at[2], expected_n = 0;
    int kinds[2] = {cases[c].first, cases[c].second};
    for (unsigned slot = 0; slot < 2; ++slot)
     if (kinds[slot] == 0 || kinds[slot] == 1) {
      expected_ack[expected_n] = kinds[slot];
      expected_at[expected_n++] = 18 + 18 * slot;
     }
    construct(1);
    cp_samples(samples, wire, 0, peer, kinds[0], kinds[1]);
    for (int s = 0; s < 2; ++s) {
     memcpy(old_before[s], &P(s).errorEnergyBeforeEC, 4);
      memcpy(old_after[s], &P(s).errorEnergyAfterEC, 4);
      if (!mapping_in_range(s, tag)) exit(diff_end());
      CALL(dm_reset, s, (dm[s], ma[s]));
     CALL(dsc_reset, s, (ds[s], 0));
     CALL(p4_rrn, s, (p4[s]));
     CALL(cp_reset, s, (cp[s]));
     ((V90CP *)cp[s])->groupSize = M(s).word_0;
     CALL(p4_cp, s, (p4[s]));
     C(s).silenceRrnRequest = local;
     P(s).int_0040 = C(s).silenceRrnRequest;
     /* Neither C1 nor reset writes +4c. Existing whole-object poison is
      * NOT a negotiated peer bit; acceptance must overwrite it. */
     old_peer[s] = P(s).uint_004c;
     diff_eq_int("matrix peer preimage retains storage poison", old_peer[s] == 0xa5a5a5a5u, 1, tag);
     diff_eq_int("matrix acceptance preimage", P(s).uchar_0030, 0, tag);
     diff_eq_int("matrix handshake preimage", P(s).int_0044, 0, tag);
    }
    unsigned calls = 0;
    dsplib_debug_capture_reset();
    dsplib_debug_capture_on = 1;
    for (unsigned i = 0; i < 192; ++i) {
     ++calls;
     int a = decision(p4[0], samples[i]), b = ref_decision(p4[1], samples[i]);
     diff_eq_int("matrix decision", a, b, tag + i);
     /* This contains only state/counters/flags, no pointers. */
     raw_eq("matrix state and flags", p4[0] + 0x20, p4[1] + 0x20, 0x30, tag + i);
     for (int s = 0; s < 2; ++s) {
      if (i % 6 == 5) {
       diff_eq_int("matrix frame size", P(s).nbits, 24, tag + i);
       raw_eq("matrix recovered wire", P(s).bits, wire + (i / 6) * 24, 24, tag + i);
      }
      int event = P(s).int_0028;
      if (event == 0x2f || event == 0x30 || event == 0x31 || event == 0x33) {
       unsigned n = accepted[s]++;
       diff_eq_int("matrix at most two acceptances", n < 2, 1, tag + i);
       if (n < 2) {
        at[s][n] = calls;
        ack[s][n] = event == 0x30 || event == 0x33;
       }
       diff_eq_int("matrix accepted before Ed", P(s).uchar_0030, 1, tag + i);
       diff_eq_int("matrix actual peer bit copy", P(s).uint_004c, peer, tag + i);
       diff_eq_int("matrix peer copy differs from preimage", P(s).uint_004c != old_peer[s], 1, tag + i);
       diff_eq_int("matrix handshake OR gate", P(s).int_0044, local || peer, tag + i);
       if (s == 1 && peer) ++peer_writes;
      }
     }
     if (P(0).state != P4D_STATE_WAIT_FOR_V90CP ||
         P(1).state != P4D_STATE_WAIT_FOR_V90CP) break;
    }
    dsplib_debug_capture_on = 0;
    total_calls += calls;
    diff_eq_int("matrix observed exit or bound sample", calls, cases[c].end_call, tag);
    diff_eq_int("matrix transcript", transcript_exact("p4d.period.CP.matrix", tag), 1, tag);
    unsigned end_state = !expected_n ? P4D_STATE_WAIT_FOR_V90CP :
                         !local && !peer ? P4D_STATE_B1D :
                         peer ? P4D_STATE_WAIT_FOR_RT : P4D_STATE_SILENCE;
    for (int s = 0; s < 2; ++s) {
     diff_eq_int("matrix accepted message count", accepted[s], expected_n, tag);
     for (unsigned n = 0; n < expected_n; ++n) {
      diff_eq_int("matrix accepted acknowledgement", ack[s][n], expected_ack[n], tag);
      diff_eq_int("matrix acceptance sample", at[s][n], expected_at[n], tag);
     }
     diff_eq_int("matrix destination", P(s).state, end_state, tag);
     diff_eq_int("matrix handshake at exit", P(s).int_0044, expected_n && (local || peer), tag);
     unsigned want_peer = expected_n ? peer : old_peer[s];
     raw_eq("matrix peer at exit", &P(s).uint_004c, &want_peer, sizeof want_peer, tag);
     diff_eq_int("matrix count on exit or bound", P(s).countInState, expected_n ? 0 : 192, tag);
     diff_eq_int("matrix Ed progress", P(s).int_0028, !expected_n ? 0 : local || peer ? 0x35 : 0x1c, tag);
     unsigned bad_crc = 0;
     const char *text = dsplib_debug_capture_text(s);
     while ((text = strstr(text, "bad CRC")) != 0) { ++bad_crc; text += 7; }
     diff_eq_int("matrix independently corrupted messages rejected", bad_crc,
                 (kinds[0] >= 2) + (kinds[1] >= 2), tag);
     raw_eq("matrix after energy untouched", &P(s).errorEnergyAfterEC, old_after[s], 4, tag);
     float zero = 0.0f;
     raw_eq("matrix before initialization only on silence handshake", &P(s).errorEnergyBeforeEC,
            expected_n && (local || peer) ? (const void *)&zero : (const void *)old_before[s], 4, tag);
    }
    printf("CP matrix: local=%u peer=%u case=%s accepted=%u acks=%u,%u at=%u,%u calls=%u/192 state=%u handshake=%d peer-in=%08x peer-out=%08x\n",
           local, peer, cases[c].name, accepted[1], ack[1][0], ack[1][1], at[1][0], at[1][1],
           calls, (unsigned)P(1).state, P(1).int_0044, old_peer[1], P(1).uint_004c);
    for (int s = 0; s < 2; ++s) {
     CALL(p4_dtor, s, (p4[s])); CALL(dm_dtor, s, (dm[s]));
     CALL(dsc_dtor, s, (ds[s])); CALL(cp_dtor, s, (cp[s]));
    }
    guards(tag);
    diff_eq_int("matrix allocations freed", harness_alloc.live, 0, tag);
    diff_eq_int("matrix no bad free", harness_alloc.bad_free, 0, tag);
   }
 diff_eq_int("matrix cell denominator", cells, 40, 0);
 diff_eq_int("matrix nonzero peer copies observed", peer_writes, 20, 0);
 diff_eq_int("matrix sample denominator", total_calls, 2856, 0);
 printf("CP matrix summary: %u/40 cells, %u sample calls per side, %u nonzero peer-copy observations\n",
        cells, total_calls, peer_writes);
}

static void measure(unsigned session, unsigned which, long tag)
{
 static const Phase4DemodulatorState states[] = {
  P4D_STATE_SILENCE, P4D_STATE_CALC_ENERGY_BEFORE_EC,
  P4D_STATE_WAIT_FOR_ECHO_CANCEL, P4D_STATE_CALC_ENERGY_AFTER_EC,
  P4D_STATE_WAIT_FOR_RT
 };
 static const unsigned lengths[] = {132, 240, 1782, 240};
 static unsigned char expect_p[2][sizeof p4[0]] __attribute__((aligned(8)));
 static unsigned char expect_d[2][sizeof dm[0]] __attribute__((aligned(8)));
 freeze_peers();
 unsigned total = 0, diagnostics = 0;
 for (unsigned phase = 0; phase < 4; ++phase) {
  short sample = phase == 1 ? inputs[which][0] : phase == 3 ? inputs[which][1] : 0;
  for (unsigned n = 1; n <= lengths[phase]; ++n) {
   long id = tag + total++;
   for (int s = 0; s < 2; ++s) {
    memcpy(expect_p[s], p4[s], sizeof p4[s]);
    memcpy(expect_d[s], dm[s], sizeof dm[s]);
    V90Phase4Demodulator *e = (V90Phase4Demodulator *)expect_p[s];
    V90Demapper *d = (V90Demapper *)expect_d[s];
    e->state = n == lengths[phase] ? states[phase + 1] : states[phase];
    e->countInState = n == lengths[phase] ? 0 : n;
    e->int_0028 = phase == 3 && n == lengths[phase] ? 0x2a : 0;
    d->rbsFramePosition = (d->rbsFramePosition + 1) % 6;
    if (phase == 1) e->errorEnergyBeforeEC = (float)(sample * sample * (n == 240 ? 1 : n));
    if (phase == 2 && n == lengths[phase]) e->errorEnergyAfterEC = 0.0f;
    if (phase == 3) e->errorEnergyAfterEC = (float)(sample * sample * (n == 240 ? 1 : n));
    if (phase == 3 && n == 240) {
     /* Expected x87 decision, NOT a runtime NaN witness. Zero/zero is
      * inferred unordered; positive/zero is +Inf. Threshold stays 2. */
     e->int_3510 = which == 2 || which == 4 ? 0 : 1;
    }
   }
   dsplib_debug_capture_reset();
   dsplib_debug_capture_on = 1;
   int result[2] = {decision(p4[0], sample), ref_decision(p4[1], sample)};
   dsplib_debug_capture_on = 0;
   /* Observer controls on the same zero/zero final call, without modifying
    * production code. An ordinary green period baseline makes these real
    * additional failures. Opt-in only; never an exemption mechanism. */
   if (fault && session == 0 && which == 1 && phase == 3 && n == 240) {
    if (!strcmp(fault, "return")) result[0] ^= 1;
    else if (!strcmp(fault, "energy")) P(0).errorEnergyAfterEC = 1.0f;
    else if (!strcmp(fault, "state")) P(0).state = P4D_STATE_CALC_ENERGY_AFTER_EC;
    else if (!strcmp(fault, "guard")) p4[0][sizeof(V90Phase4Demodulator)] ^= 1;
    else if (!strcmp(fault, "peer")) ad[0][0x100] ^= 1;
    else if (!strcmp(fault, "transcript")) {
     dsplib_debug_capture_on = 1;
     dsplibs_debug_printf("injected period observer diagnostic\r\n");
     dsplib_debug_capture_on = 0;
    }
   }
   diff_eq_int("measurement return agrees", result[0], result[1], id);
   for (int s = 0; s < 2; ++s) {
    diff_eq_int("measurement returns input", result[s], sample, id);
    raw_eq("complete P4D step including pointers and guard", p4[s], expect_p[s], sizeof p4[s], id);
    raw_eq("complete demapper step including pointers and guard", dm[s], expect_d[s], sizeof dm[s], id);
    diff_eq_int("diagnostic at each transition only", dsplib_debug_capture_size(s) > 0, n == lengths[phase], id);
   }
   raw_eq("before energy exact", &P(0).errorEnergyBeforeEC, &P(1).errorEnergyBeforeEC, 4, id);
   raw_eq("after energy exact", &P(0).errorEnergyAfterEC, &P(1).errorEnergyAfterEC, 4, id);
   diff_eq_int("measurement transcript exact", transcript_exact("p4d.period.step", id), 1, id);
   if (dsplib_debug_capture_size(1)) ++diagnostics;
  }
 }
 diff_eq_int("default timing calls", total, 2394, tag);
 diff_eq_int("four diagnostic transitions", diagnostics, 4, tag);
 thaw_peers(tag);
 guards(tag);
 printf("period fixture: cycle=%u inputs=%d/%d measurement calls=%u/2394 transitions=%u/4 heaps=%d unchanged; no raw dB witness\n",
        which, inputs[which][0], inputs[which][1], total, diagnostics, nheap);
}

static void producer_boundary(void)
{
 short decision_at_1000[3][2];
 unsigned char first_mapping[sizeof(V90MappingParams)];
 unsigned char first_calibration[0xc00];
 for (unsigned mode = 1; mode <= 3; ++mode) {
  construct(0, mode);
  memcpy(manual_calibration[mode-1], calibrated, sizeof calibrated);
  for (int s = 0; s < 2; ++s)
   memcpy(manual_mapping[mode-1][s], ma[s], sizeof(V90MappingParams));
  unsigned usable[2] = {0, 0};
  /* Missing samples are a fault control: do not credit the designer's
   * success flag alone as evidence of a usable constellation. */
  for (int s = 0; s < 2; ++s) {
   diff_eq_int("TRN2 default frame bits", M(s).word_0, 23, mode);
   diff_eq_int("TRN2 default shaper rate", M(s).shaperSR, 1, mode);
   diff_eq_int("TRN2 design returned success (not validity)", producer_result[s], 1, mode);
   if (!mapping_in_range(s, mode)) exit(diff_end());
   CALL(dm_reset, s, (dm[s], ma[s]));
   unsigned distinct = 0, measured = 0;
   if (!mapping_in_range(s, mode)) exit(diff_end());
   for (unsigned phase = 0; phase < 6; ++phase) {
    diff_eq_int("TRN2 default eight levels", M(s).constellationSize[phase], 8, mode);
    for (unsigned i = 0; i < 8; ++i) {
     short level = A(s).linMapp[phase][M(s).constellation[phase][i]];
     const V90AutoDigitalImpDetector *a = (const V90AutoDigitalImpDetector *)calibrated[s];
     if (a->magnitudeCount[phase][M(s).constellation[phase][i]] > 0) ++measured;
     diff_eq_int("demapper consumes produced calibration", D(s).constellation[phase][i], level, mode);
     if (level > 0 && (!i || level < D(s).constellation[phase][i-1])) ++distinct;
    }
   }
   diff_eq_int("producer usable descending levels", distinct, mode == 3 ? 0 : 48, mode);
   diff_eq_int("selected constellation codes measured", measured, mode == 3 ? 0 : 48, mode);
   usable[s] = distinct;
   decision_at_1000[mode-1][s] = s ? ref_dm_hard(dm[s], 1000) : dm_hard(dm[s], 1000);
   diff_eq_int("missing calibration changes downstream decision",
               decision_at_1000[mode-1][s] != 0, mode != 3, mode);
   diff_eq_int("producer ADI parameter identity", A(s).params == (V90Parameters *)pa[s], 1, mode);
  }
  diff_eq_int("produced hard decisions agree", decision_at_1000[mode-1][0], decision_at_1000[mode-1][1], mode);
  if (mode == 1) {
   memcpy(first_mapping, ma[1], sizeof first_mapping);
   memcpy(first_calibration, calibrated[1], sizeof first_calibration);
  } else if (mode == 2) {
   diff_eq_int("finite channel gain changes calibration", memcmp(first_calibration, calibrated[1], sizeof first_calibration) != 0, 1, mode);
   diff_eq_int("finite channel gain changes design", memcmp(first_mapping, ma[1], sizeof first_mapping) != 0, 1, mode);
   for (int s = 0; s < 2; ++s)
    diff_eq_int("finite channel gain changes downstream decision", decision_at_1000[0][s] != decision_at_1000[1][s], 1, s);
  }
  printf("producer boundary: mode=%u samples=%u design=%d bits=%u decision1000=%d\n",
         mode, producer_samples[1], producer_result[1], M(1).word_0, decision_at_1000[mode-1][1]);
  if (mode != 3 && usable[0] == 48 && usable[1] == 48) {
   for (unsigned cycle = 0; cycle < sizeof inputs / sizeof inputs[0]; ++cycle) {
    long tag = 3000000L + mode * 100000L + cycle * 5000L;
    if (!enter_silence(0, tag, 0, mode)) break;
    measure(0, cycle, tag + 200);
   }
  }
  for (int s = 0; s < 2; ++s) {
   CALL(p4_dtor, s, (p4[s])); CALL(dm_dtor, s, (dm[s]));
   CALL(dsc_dtor, s, (ds[s])); CALL(cp_dtor, s, (cp[s]));
   unsigned char canary[32]; memset(canary, 0x69, sizeof canary);
#define PRODUCER_GUARD(name, type) raw_eq(#name " producer guard", name[s] + sizeof(type), canary, 32, mode)
   PRODUCER_GUARD(gen, V90Phase3Modulator); PRODUCER_GUARD(desc, tagV90DILdescriptor);
   PRODUCER_GUARD(trn, V90TRN2Designer); PRODUCER_GUARD(power, V90ConstellationPower);
#undef PRODUCER_GUARD
  }
  guards(mode);
  diff_eq_int("producer allocations freed", harness_alloc.live, 0, mode);
  diff_eq_int("producer no bad free", harness_alloc.bad_free, 0, mode);
 }
 printf("producer summary: 2 finite channel cases, 1 missing-calibration fault; evaluator request supplied\n");
}

/* DIL alone is not an Sd/TRN1/Jd history. Stop at the first terminal event;
 * never continue through a default/uninitialized-return state. */
static void ctor_dil_replay(void)
{
 for (unsigned mode = 1; mode <= 3; ++mode) {
  harness_alloc_reset();
  for (int s = 0; s < 2; ++s) {
#define REPLAY_INIT(name, type) memset(name[s], 0, sizeof(type)); memset(name[s] + sizeof(type), 0x69, 32)
   REPLAY_INIT(pa, V90Parameters); REPLAY_INIT(host, _tagModemParameters);
   REPLAY_INIT(ad, V90AutoDigitalImpDetector); REPLAY_INIT(p3, V90Phase3Demodulator);
   REPLAY_INIT(gen, V90Phase3Modulator); REPLAY_INIT(desc, tagV90DILdescriptor);
#undef REPLAY_INIT
   ((_tagModemParameters *)host[s])->minRate = 28000;
   ((_tagModemParameters *)host[s])->maxRate = 56000;
   CALL(param_ctor, s, (pa[s], host[s])); CALL(adi_ctor, s, (ad[s], pa[s]));
   CALL(p3d_ctor, s, (p3[s], pa[s], 0, 0, ad[s]));
   CALL(dil_descriptor, s, (desc[s], DIL_TYPE_ADI));
   CALL(p3m_ctor, s, (gen[s], pa[s], 0));
   CALL(p3m_reset, s, (gen[s], PCM_TYPE_MU_LAW, 0, P3M_STATE_DIL, 0, 0, 0, desc[s], 0));
   memcpy(calibrated[s], ad[s], sizeof(V90AutoDigitalImpDetector));
  }
  unsigned calls = 0;
  for (; calls < 32280; ++calls) {
   int result[2], symbol[2];
   dsplib_debug_capture_reset(); dsplib_debug_capture_on = 1;
   for (int s = 0; s < 2; ++s) {
    V90Phase3Demodulator &r = *(V90Phase3Demodulator *)p3[s];
    unsigned old = r.state;
    symbol[s] = s ? ref_dil_generate(gen[s]) : dil_generate(gen[s]);
    float sample = mode == 3 ? 0.0f : mode == 2 ? symbol[s] / 2.0f : (float)symbol[s];
    result[s] = s ? ref_p3_decision(p3[s], sample) : p3_decision(p3[s], sample);
    if ((unsigned)r.state != old)
     printf("P3 ctor path: mode=%u side=%d call=%u state=%u->%u event=%u\n",
            mode, s, calls + 1, old, (unsigned)r.state, r.eventCode);
   }
   dsplib_debug_capture_on = 0;
   diff_eq_int("P3 ctor replay independent symbols", symbol[0], symbol[1], calls);
   diff_eq_int("P3 ctor replay decision", result[0], result[1], calls);
   raw_eq("P3 ctor replay state", p3[0] + 0x28, p3[1] + 0x28, 12, calls);
   diff_eq_int("P3 ctor replay transcript", transcript_exact("p3.ctor.replay", calls), 1, calls);
   if (((V90Phase3Demodulator *)p3[0])->state >= 19 ||
       ((V90Phase3Demodulator *)p3[1])->state >= 19) { ++calls; break; }
  }
  for (int s = 0; s < 2; ++s) {
   V90Phase3Demodulator &r = *(V90Phase3Demodulator *)p3[s];
   printf("P3 ctor replay: mode=%u side=%d calls=%u/32280 state=%u count=%u event=%u\n",
          mode, s, calls, (unsigned)r.state, r.samplesInState, r.eventCode);
   diff_eq_int("P3 ctor replay timeout duration", calls, 12000, mode);
   diff_eq_int("P3 ctor replay timeout state", r.state, 20, mode);
   diff_eq_int("P3 ctor replay timeout event", r.eventCode, 21, mode);
   diff_eq_int("P3 ctor replay timeout count", r.samplesInState, 0, mode);
   raw_eq("P3 ctor DIL does not calibrate", calibrated[s], ad[s], sizeof(V90AutoDigitalImpDetector), mode);
   CALL(p3d_dtor, s, (p3[s])); CALL(p3m_dtor, s, (gen[s]));
   unsigned char canary[32]; memset(canary, 0x69, 32);
   raw_eq("P3 ctor replay guard", p3[s] + sizeof(V90Phase3Demodulator), canary, 32, mode);
  }
  diff_eq_int("P3 ctor replay allocations freed", harness_alloc.live, 0, mode);
  diff_eq_int("P3 ctor replay no bad free", harness_alloc.bad_free, 0, mode);
 }
}

/* Defined reset boundary, not ctor-to-study reachability. Earlier Sd/SdNot
 * synchronization is assumed. The fixture responds to decoded Jd with the
 * transmitter's actual exitJd method; no enclosing modem handshake is claimed.
 * Missing-input control replaces only the received DIL, never its code labels.
 */
static void study_boundary(void)
{
 static const unsigned at[] = {0, 2040, 14040, 21240, 21324, 21336, 25176, 49896, 50496};
 static const unsigned state[] = {3, 4, 5, 6, 9, 10, 11, 12, 16};
 static unsigned char params_before[2][sizeof pa[0]], descriptor_before[2][sizeof desc[0]], tx_jd_before[2][sizeof jdt[0]];
 for (unsigned mode = 1; mode <= 3; ++mode) {
  harness_alloc_reset();
  for (int s = 0; s < 2; ++s) {
#define STUDY_INIT(name, type) memset(name[s], 0, sizeof(type)); memset(name[s] + sizeof(type), 0x69, 32)
   STUDY_INIT(pa, V90Parameters); STUDY_INIT(host, _tagModemParameters);
   STUDY_INIT(ad, V90AutoDigitalImpDetector); STUDY_INIT(p3, V90Phase3Demodulator);
   STUDY_INIT(gen, V90Phase3Modulator); STUDY_INIT(desc, tagV90DILdescriptor);
   STUDY_INIT(jdt, V90Jd); STUDY_INIT(jdr, V90Jd);
#undef STUDY_INIT
   ((_tagModemParameters *)host[s])->minRate = 28000;
   ((_tagModemParameters *)host[s])->maxRate = 56000;
   CALL(param_ctor, s, (pa[s], host[s]));
   CALL(adi_ctor, s, (ad[s], pa[s]));
   CALL(p3d_ctor, s, (p3[s], pa[s], 0, 0, ad[s]));
   CALL(jd_ctor, s, (jdt[s], pa[s]));
   CALL(jd_ctor, s, (jdr[s], pa[s]));
   CALL(dil_descriptor, s, (desc[s], DIL_TYPE_ADI));
   CALL(p3d_reset, s, (p3[s], PCM_TYPE_MU_LAW, 64, P3D_STATE_TRN1D_KNOWN_DATA,
        0, jdr[s], 0, desc[s], 0, 1, 0.0f, 0));
   CALL(p3m_ctor, s, (gen[s], pa[s], 0));
   CALL(p3m_reset, s, (gen[s], PCM_TYPE_MU_LAW, 64, P3M_STATE_TRN1D,
        0, jdt[s], 0, desc[s], 0));
   memcpy(params_before[s], pa[s], sizeof pa[0]);
   memcpy(descriptor_before[s], desc[s], sizeof desc[0]);
   memcpy(tx_jd_before[s], jdt[s], sizeof jdt[0]);
  }
  unsigned calls = 0, dil_calls[2] = {0, 0}, studies[2] = {0, 0};
  unsigned exits[2] = {0, 0};
  unsigned valid[2] = {0, 0}, transitions[2] = {0, 0};
  unsigned char seen[2][6][128] = {{{0}}};
  for (; calls < 70000; ++calls) {
   int result[2], symbol[2];
   dsplib_debug_capture_reset(); dsplib_debug_capture_on = 1;
   for (int s = 0; s < 2; ++s) {
    V90Phase3Demodulator &r = *(V90Phase3Demodulator *)p3[s];
    V90Phase3Modulator &g = *(V90Phase3Modulator *)gen[s];
    unsigned old = r.state;
    int dil_input = g.state == P3M_STATE_DIL;
    symbol[s] = s ? ref_p3_symbol(gen[s]) : p3_symbol(gen[s]);
    float sample = mode == 2 ? symbol[s] / 2.0f : (float)symbol[s];
    if (dil_input) {
     ++dil_calls[s];
     if (mode == 3 || (mode == 1 && fault && !strcmp(fault, "study-missing"))) sample = 0.0f;
    }
    result[s] = s ? ref_p3_decision(p3[s], sample) : p3_decision(p3[s], sample);
    for (unsigned p = 0; p < 6; ++p) for (unsigned c = 0; c < 128; ++c)
     if (A(s).magnitudeCount[p][c]) seen[s][p][c] = 1;
    if (old >= 10 && old <= 12) ++studies[s];
    if (r.eventCode == 6) { CALL(p3m_exitjd, s, (gen[s])); ++exits[s]; }
    if ((unsigned)r.state != old) {
     ++transitions[s];
     printf("P3 study path: mode=%u side=%d call=%u state=%u->%u event=%u dil=%u\n",
            mode, s, calls + 1, old, (unsigned)r.state, r.eventCode, dil_calls[s]);
    }
    unsigned step = 0;
    while (step < 8 && calls + 1 >= at[step + 1]) ++step;
    diff_eq_int("P3 expected state path", r.state, state[step], calls);
    diff_eq_int("P3 expected elapsed samples", r.samplesInState, calls + 1 - at[step], calls);
    if (dil_input)
     raw_eq("P3 internal DIL aligned with independent transmitter", (unsigned char *)&r.phase3Modulator + 0x54,
            gen[s] + 0x54, sizeof(V90Phase3Modulator) - 0x54, calls);
   }
   dsplib_debug_capture_on = 0;
   diff_eq_int("P3 independently generated symbol", symbol[0], symbol[1], calls);
   diff_eq_int("P3 decision", result[0], result[1], calls);
   raw_eq("P3 state count event", p3[0] + 0x28, p3[1] + 0x28, 12, calls);
   diff_eq_int("P3 frame", ((V90Phase3Demodulator *)p3[0])->framePosition,
               ((V90Phase3Demodulator *)p3[1])->framePosition, calls);
   diff_eq_int("P3 transcript", transcript_exact("p3.study", calls), 1, calls);
   raw_eq("P3 ADI step before pointer", ad[0], ad[1], 0x2814, calls);
   raw_eq("P3 ADI step after pointer", ad[0] + 0x2818, ad[1] + 0x2818,
          sizeof ad[0] - 0x2818, calls);
   unsigned a = ((V90Phase3Demodulator *)p3[0])->state;
   unsigned b = ((V90Phase3Demodulator *)p3[1])->state;
   if (a == 16 || b == 16 || a >= 19 || b >= 19) { ++calls; break; }
  }
  dsplib_debug_capture_reset(); dsplib_debug_capture_on = 1;
  for (int s = 0; s < 2; ++s) {
   unsigned measured = 0, ever = 0;
   for (unsigned p = 0; p < 6; ++p) for (unsigned c = 0; c < 128; ++c)
    { if (A(s).magnitudeCount[p][c]) ++measured; if (seen[s][p][c]) ++ever; }
   V90Phase3Demodulator &r = *(V90Phase3Demodulator *)p3[s];
   diff_eq_int("P3 completed inside bound", calls, 50496, mode);
   diff_eq_int("P3 study calls", studies[s], 29160, mode);
   diff_eq_int("P3 independent DIL calls", dil_calls[s], 29160, mode);
   diff_eq_int("P3 decoded Jd event", exits[s], 1, mode);
   diff_eq_int("P3 transition denominator", transitions[s], 8, mode);
   diff_eq_int("P3 completed state", r.state, 16, mode);
   diff_eq_int("P3 completion event", r.eventCode, 17, mode);
   diff_eq_int("P3 live cell observation denominator", ever, 690, mode);
   diff_eq_int("P3 own ADI", r.autoDigitalImpDetector == (V90AutoDigitalImpDetector *)ad[s], 1, mode);
   diff_eq_int("P3 own parameters", r.params == (V90Parameters *)pa[s], 1, mode);
   diff_eq_int("P3 own receiver Jd", r.jd == (V90Jd *)jdr[s], 1, mode);
   diff_eq_int("P3 own descriptor", r.dil == (tagV90DILdescriptor *)desc[s], 1, mode);
   diff_eq_int("P3 ADI own parameters", A(s).params == (V90Parameters *)pa[s], 1, mode);
   raw_eq("P3 preserves parameters and guard", pa[s], params_before[s], sizeof pa[0], mode);
   raw_eq("P3 preserves descriptor and guard", desc[s], descriptor_before[s], sizeof desc[0], mode);
   raw_eq("P3 preserves transmit Jd and guard", jdt[s], tx_jd_before[s], sizeof jdt[0], mode);
   printf("P3 study boundary: mode=%u side=%d calls=%u/70000 state=%u count=%u event=%u DIL=%u study=%u cells=%u/768 seen=%u/768 Jd=%u\n",
          mode, s, calls, (unsigned)r.state, r.samplesInState, r.eventCode,
          dil_calls[s], studies[s], measured, ever, exits[s]);
   CALL(adi_max, s, (ad[s], 116));
   memcpy(calibrated[s], ad[s], sizeof(V90AutoDigitalImpDetector));
   const V90AutoDigitalImpDetector *manual = (const V90AutoDigitalImpDetector *)manual_calibration[mode-1][s];
   unsigned leveldiff = 0, altdiff = 0, maskdiff = 0, manualcells = 0;
   for (unsigned p = 0; p < 6; ++p) for (unsigned c = 0; c < 128; ++c) {
    leveldiff += A(s).linMapp[p][c] != manual->linMapp[p][c];
     altdiff += A(s).linMappAlt[p][c] != manual->linMappAlt[p][c];
     if (A(s).linMappAlt[p][c] != manual->linMappAlt[p][c])
      printf("P3 alternate difference: mode=%u side=%d phase=%u code=%u timed=%d manual=%d\n",
             mode, s, p, c, A(s).linMappAlt[p][c], manual->linMappAlt[p][c]);
    maskdiff += A(s).usableMask[p][c] != manual->usableMask[p][c];
    manualcells += manual->magnitudeCount[p][c] != 0;
   }
   printf("P3 manual comparison: mode=%u side=%d primary-diff=%u/768 alternate-diff=%u/768 mask-diff=%u/768 manual-live-cells=%u/768\n",
          mode, s, leveldiff, altdiff, maskdiff, manualcells);
   memset(ma[s], 0, sizeof(V90MappingParams));
   memset(trn[s], 0, sizeof(V90TRN2Designer));
   memset(power[s], 0, sizeof(V90ConstellationPower));
   memset(ma[s] + sizeof(V90MappingParams), 0x69, 32);
   memset(trn[s] + sizeof(V90TRN2Designer), 0x69, 32);
   memset(power[s] + sizeof(V90ConstellationPower), 0x69, 32);
   CALL(power_ctor, s, (power[s]));
   CALL(trn_ctor, s, (trn[s], pa[s], power[s]));
   short design = s ? ref_trn_design(trn[s], ma[s], A(s).linMapp, A(s).linMappAlt,
     A(s).usableMask, A(s).altRbsFlag, PCM_TYPE_MU_LAW, PCM_TYPE_MU_LAW,
     A(s).unSuspectedPhase, A(s).maxUcode, 0, 1, (V90SpecialSpectralConditions)0) :
    trn_design(trn[s], ma[s], A(s).linMapp, A(s).linMappAlt,
     A(s).usableMask, A(s).altRbsFlag, PCM_TYPE_MU_LAW, PCM_TYPE_MU_LAW,
     A(s).unSuspectedPhase, A(s).maxUcode, 0, 1, (V90SpecialSpectralConditions)0);
   raw_eq("P3 design preserves learned ADI", calibrated[s], ad[s], sizeof(V90AutoDigitalImpDetector), mode);
   if (mode == 1 && fault && (!strcmp(fault, "mapping-128") || !strcmp(fault, "mapping-255")))
    M(s).constellation[5][0] = !strcmp(fault, "mapping-128") ? 128 : 255;
   int mapping_ok = mapping_in_range(s, mode);
   unsigned selected = 0, positive = 0, masks = 0;
   for (unsigned p = 0; p < 6; ++p) {
    diff_eq_int("P3 no alternate RBS", A(s).altRbsFlag[p], 0, mode);
    for (unsigned c = 0; c < 128; ++c) if (A(s).usableMask[p][c]) ++masks;
    short previous = 0;
    if (mapping_ok) for (unsigned i = 0; i < 8; ++i) {
     unsigned c = M(s).constellation[p][i];
     if (seen[s][p][c]) ++selected;
     short v = A(s).linMapp[p][c];
     if (v > 0 && (!i || v < previous)) ++positive;
     previous = v; /* Only a previously range-checked level, never another index. */
    }
   }
   CALL(dm_ctor, s, (dm[s], 72, pa[s], ad[s]));
   short hard = 0;
   if (mapping_ok) {
    CALL(dm_reset, s, (dm[s], ma[s]));
    hard = s ? ref_dm_hard(dm[s], 1000) : dm_hard(dm[s], 1000);
   } else {
    printf("mapping downstream blocked: mode=%u side=%d dm_reset=0 hardDecision=0 P4D=0\n", mode, s);
   }
   valid[s] = positive;
   diff_eq_int("P3 usable descending levels required", positive, mode == 3 ? 0 : 48, mode);
   diff_eq_int("P3 downstream decision requires study input", hard, mode == 1 ? 988 : mode == 2 ? 622 : 0, mode);
   diff_eq_int("P3 selected cells observed", selected, 48, mode);
   diff_eq_int("P3 designer success is not validity", design, 1, mode);
   diff_eq_int("P3 default frame bits", M(s).word_0, 23, mode);
   diff_eq_int("P3 default shaper", M(s).shaperSR, 1, mode);
   diff_eq_int("P3 usable mask denominator", masks, 702, mode);
   diff_eq_int("P3 primary calibration comparison", leveldiff, mode == 3 ? 6 : 0, mode);
   diff_eq_int("P3 alternate calibration comparison", altdiff, 6, mode);
   diff_eq_int("P3 mask comparison", maskdiff, 0, mode);
   diff_eq_int("P3 live cells after timed finalization", measured, mode == 3 ? 690 : 12, mode);
   for (unsigned p = 0; p < 6; ++p)
    diff_eq_int("P3 measured maximum", A(s).maxUcode[p], mode == 3 ? 80 : 116, mode);
   raw_eq("P3 mapping matches manual-boundary mapping", ma[s], manual_mapping[mode-1][s], sizeof(V90MappingParams), mode);
   printf("P3 study design: mode=%u side=%d result=%d bits=%u selected=%u/48 positive=%u/48 masks=%u/768 max=%u,%u,%u,%u,%u,%u decision1000=%d manual-map-equal=%d manual-levels-equal=%d\n",
    mode, s, design, M(s).word_0, selected, positive, masks,
    A(s).maxUcode[0], A(s).maxUcode[1], A(s).maxUcode[2], A(s).maxUcode[3], A(s).maxUcode[4], A(s).maxUcode[5], hard,
    !memcmp(ma[s], manual_mapping[mode-1][s], sizeof(V90MappingParams)),
    !memcmp(ad[s], manual_calibration[mode-1][s], 0xc00));
   CALL(dm_dtor, s, (dm[s]));
   CALL(p3d_dtor, s, (p3[s])); CALL(p3m_dtor, s, (gen[s]));
   unsigned char canary[32]; memset(canary, 0x69, sizeof canary);
#define STUDY_GUARD(name, type) raw_eq(#name " study guard", name[s] + sizeof(type), canary, 32, mode)
   STUDY_GUARD(p3, V90Phase3Demodulator); STUDY_GUARD(gen, V90Phase3Modulator);
   STUDY_GUARD(ad, V90AutoDigitalImpDetector); STUDY_GUARD(desc, tagV90DILdescriptor);
   STUDY_GUARD(jdt, V90Jd); STUDY_GUARD(jdr, V90Jd);
   STUDY_GUARD(pa, V90Parameters); STUDY_GUARD(host, _tagModemParameters);
   STUDY_GUARD(ma, V90MappingParams); STUDY_GUARD(trn, V90TRN2Designer);
   STUDY_GUARD(power, V90ConstellationPower); STUDY_GUARD(dm, V90Demapper);
#undef STUDY_GUARD
  }
  dsplib_debug_capture_on = 0;
  diff_eq_int("P3 finalization and design transcript", transcript_exact("p3.study.design", mode), 1, mode);
  for (int s = 0; s < 2; ++s)
   diff_eq_int("P3 design diagnostic nonempty", dsplib_debug_capture_size(s) > 0, 1, mode);
  raw_eq("P3 decoded Jd and guard", jdr[0], jdr[1], sizeof jdr[0], mode);
  raw_eq("P3 learned snapshot before pointer", calibrated[0], calibrated[1], 0x2814, mode);
  raw_eq("P3 learned snapshot after pointer", calibrated[0] + 0x2818, calibrated[1] + 0x2818,
         sizeof(V90AutoDigitalImpDetector) - 0x2818, mode);
  raw_eq("P3 ADI before parameter pointer", ad[0], ad[1], 0x2814, mode);
  raw_eq("P3 mapping", ma[0], ma[1], sizeof ma[0], mode);
  raw_eq("P3 ADI after parameter pointer", ad[0] + 0x2818, ad[1] + 0x2818,
         sizeof ad[0] - 0x2818, mode);
  diff_eq_int("P3 study allocations freed", harness_alloc.live, 0, mode);
  diff_eq_int("P3 study no bad free", harness_alloc.bad_free, 0, mode);
  if (mode != 3 && valid[0] == 48 && valid[1] == 48) {
   /* Same P4D component entry as the manual case, now with P3-produced ADI.
    * The positive evaluator outcome is STILL supplied, not relabelled. */
   for (int s = 0; s < 2; ++s) {
#define STUDY_RX_INIT(name, type) memset(name[s], 0, sizeof(type)); memset(name[s] + sizeof(type), 0x69, 32)
    STUDY_RX_INIT(ce, V90ConnectionEvaluator); STUDY_RX_INIT(cp, V90CP);
    STUDY_RX_INIT(mp, V90MP); STUDY_RX_INIT(ds, PeriodDescrambler);
    STUDY_RX_INIT(dm, V90Demapper); STUDY_RX_INIT(mb, V90MappingParams);
    STUDY_RX_INIT(p4, V90Phase4Demodulator);
#undef STUDY_RX_INIT
    memset(p4[s], 0xa5, sizeof(V90Phase4Demodulator));
    CALL(ce_ctor, s, (ce[s], pa[s])); C(s).silenceRrnRequest = 1;
    memcpy(mb[s], ma[s], sizeof(V90MappingParams));
    CALL(cp_ctor, s, (cp[s])); CALL(mp_ctor, s, (mp[s]));
    CALL(dsc_ctor, s, (ds[s], 18, 23, 99)); CALL(dsc_reset, s, (ds[s], 0));
    CALL(dm_ctor, s, (dm[s], 72, pa[s], ad[s]));
    CALL(p4_ctor, s, (p4[s], ma[s], mb[s], dm[s], cp[s], mp[s], ds[s], ce[s], pa[s], (void *)0, ad[s], 0));
    CALL(p4_reset, s, (p4[s], 0, P4D_STATE_WAIT_FOR_RI, 0, 0));
   }
   for (unsigned cycle = 0; cycle < 6; ++cycle) {
    long tag = 4000000L + mode * 100000L + cycle * 5000L;
    if (!enter_silence(0, tag, 0, mode)) break;
    measure(0, cycle, tag + 200);
   }
   for (int s = 0; s < 2; ++s) {
    CALL(p4_dtor, s, (p4[s])); CALL(dm_dtor, s, (dm[s]));
    CALL(dsc_dtor, s, (ds[s])); CALL(cp_dtor, s, (cp[s]));
   }
   guards(mode);
   diff_eq_int("P3 downstream allocations freed", harness_alloc.live, 0, mode);
   diff_eq_int("P3 downstream no bad free", harness_alloc.bad_free, 0, mode);
  }
 }
 printf("P3 study summary: 3/3 cases; 50496 calls/side/case; timed study, evaluator request supplied\n");
}

int main(void)
{
 diff_begin("V90Phase4Demodulator: default-timing component silence lifecycle");
 fault = getenv("DSPLIB_P4D_PERIOD_FAULT");
 if (fault && strcmp(fault, "return") && strcmp(fault, "energy") &&
     strcmp(fault, "state") && strcmp(fault, "guard") &&
     strcmp(fault, "peer") && strcmp(fault, "transcript") && strcmp(fault, "cp-crc") &&
      strcmp(fault, "calibration-missing") && strcmp(fault, "study-missing") &&
      strcmp(fault, "mapping-128") && strcmp(fault, "mapping-255")) {
  diff_eq_int("unknown observer probe", 0, 1, 0);
  return diff_end();
 }
 dsplibs_debug_level = ref_dsplibs_debug_level = 3;
 for (unsigned session = 0; session < 2; ++session) {
  construct(session);
  if (session) {
   enter_silence(session, 800000L, 1);
   enter_silence(session, 810000L, 2);
  }
  for (unsigned cycle = 0; cycle < sizeof inputs / sizeof inputs[0]; ++cycle) {
   long tag = 100000L * session + 5000L * cycle;
   if (!enter_silence(session, tag)) return diff_end();
   measure(session, cycle, tag + 200);
  }
  for (int s = 0; s < 2; ++s) {
   CALL(p4_dtor, s, (p4[s])); CALL(dm_dtor, s, (dm[s]));
   CALL(dsc_dtor, s, (ds[s])); CALL(cp_dtor, s, (cp[s]));
  }
  guards(999999);
  diff_eq_int("all owned allocations freed", harness_alloc.live, 0, session);
  diff_eq_int("no bad free", harness_alloc.bad_free, 0, session);
 }
 cp_entry_matrix();
 producer_boundary();
 ctor_dil_replay();
 study_boundary();
 return diff_end();
}
