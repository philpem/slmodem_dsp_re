/*
 * Period-primary silence measurement, with real construction/reset and Ed.
 * Scope: component methods, NOT a negotiated/public modem connection.
 * Inputs supplied by the environment: an eight-level mapping/calibration,
 * the evaluator's silence request, and entry to WaitForEd/WaitForCP.
 * V.92 receives synthesized CRC-bearing short CP/CPnot and Ed as samples;
 * the caller's evaluator-to-P4D request copy remains an explicit boundary.
 * No energy, measurement count, or measurement state is assigned by this test.
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

static void construct(unsigned session)
{
 harness_alloc_reset();
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
  CALL(adi_ctor, s, (ad[s], pa[s]));
  CALL(adi_reset, s, (ad[s], 0, PCM_TYPE_MU_LAW, 0));
  CALL(adi_mapping_reset, s, (ad[s]));
  /* Supplied component inputs, NOT negotiated/calibrated modem outputs.
   * The reset methods establish PCM law and no-alt-RBS flags; the eight
   * levels below still assume a calibration not produced by DIL training.
   * Calling an accumulator with invented code/sample associations would
   * merely move that assumption, so keep it visible here. */
  C(s).silenceRrnRequest = 1;
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
  memcpy(mb[s], ma[s], sizeof(V90MappingParams));
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

static void freeze_peers(void)
{
#define SAVE(name) memcpy(name##_before, name, sizeof name)
 SAVE(pa); SAVE(ce); SAVE(ad); SAVE(cp); SAVE(mp); SAVE(ds); SAVE(ma); SAVE(mb); SAVE(host);
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

static int enter_silence(unsigned session, long tag, int control = 0)
{
 unsigned char old_after[2][4], old_before[2][4];
 short samples[192];
 unsigned char wire[768];
 int injected_crc = session && !control && fault && !strcmp(fault, "cp-crc");
 if (session) cp_samples(samples, wire, injected_crc ? 1 : control);
 for (int s = 0; s < 2; ++s) {
  memcpy(old_after[s], &P(s).errorEnergyAfterEC, 4);
  memcpy(old_before[s], &P(s).errorEnergyBeforeEC, 4);
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
  int a = decision(p4[0], sample), b = ref_decision(p4[1], sample);
  diff_eq_int("Ed input decision", a, b, tag + calls);
  diff_eq_int("Ed state agrees", P(0).state, P(1).state, tag + calls);
  diff_eq_int("Ed count agrees", P(0).countInState, P(1).countInState, tag + calls);
  diff_eq_int("Ed progress agrees", P(0).int_0028, P(1).int_0028, tag + calls);
  diff_eq_int("Ed cursor agrees", D(0).rbsFramePosition, D(1).rbsFramePosition, tag + calls);
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
 printf("period fixture: session=%u Ed calls=%u/192 scope=%s\n", session, calls < 192 ? calls + 1 : calls,
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

int main(void)
{
 diff_begin("V90Phase4Demodulator: default-timing component silence lifecycle");
 fault = getenv("DSPLIB_P4D_PERIOD_FAULT");
 if (fault && strcmp(fault, "return") && strcmp(fault, "energy") &&
     strcmp(fault, "state") && strcmp(fault, "guard") &&
     strcmp(fault, "peer") && strcmp(fault, "transcript") && strcmp(fault, "cp-crc")) {
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
 return diff_end();
}
