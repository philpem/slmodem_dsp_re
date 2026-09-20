/*
 * Strict keep-rate sentinel. Negative energy is a SYNTHETIC ADVERSARIAL
 * input, not evidence of reachable modem state. Flag 1 is NOT a NaN witness:
 * finite dB above the threshold also sets it. The inline production log's raw
 * result is not exposed by the integer-formatted diagnostic. Migration remains
 * blocked until an apparatus-only, build-bound witness observes that result on
 * BOTH sides. Do not substitute a recomputed logarithm or infer NaN from flags.
 *
 * Raw modern execution remains red; period execution must pass exactly. This
 * binary therefore cannot score an ordinary exit-code mutation suite. The
 * optional fault probes below validate individual observers, not mutation scores.
 */
#include <string.h>
#include "harness.h"
#include "transcript_evidence.h"
#include "dsplib/debug.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;
short ref_p4d_getv90decision(void *, short)
	asm("ref__ZN20V90Phase4Demodulator14getV90DecisionEs");
short ref_p4d_getv92decision(void *, short)
	asm("ref__ZN20V90Phase4Demodulator14getV92DecisionEs");
short our_p4d_getv90decision(void *, short)
	asm("_ZN20V90Phase4Demodulator14getV90DecisionEs");
short our_p4d_getv92decision(void *, short)
	asm("_ZN20V90Phase4Demodulator14getV92DecisionEs");
}

#define P4D_SLOT ((unsigned)sizeof(V90Phase4Demodulator) + 64u)
#define DEM_SLOT ((unsigned)sizeof(V90Demapper) + 64u)
#define PARM_SLOT ((unsigned)sizeof(V90Parameters) + 64u)
static unsigned char p4d_s[2][P4D_SLOT] __attribute__((aligned(8)));
static unsigned char dem_s[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char parm_s[2][PARM_SLOT] __attribute__((aligned(8)));
static unsigned char parm_before[2][PARM_SLOT];
static unsigned char guard_before[2][64];
static unsigned char dem_before[2][64];
static unsigned char canonical[2][P4D_SLOT] __attribute__((aligned(8)));
#define P4D(s) (*(V90Phase4Demodulator *)p4d_s[s])
#define DEM(s) ((V90Demapper *)dem_s[s])
#define PARAMS(s) ((V90Parameters *)parm_s[s])

static void fill(unsigned char *p, unsigned n, unsigned lfsr)
{
	for (unsigned i = 0; i < n; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		p[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
}

/* Fixed arithmetic controls, chosen without harvesting observed output.
 * sample=0, after=42, count=42 gives normalized energy 1 and ratio=before.
 * ratio=1 gives exactly 0 dB: equality and thresholds on either side.
 * before=0 gives log(0), NOT a NaN witness. */
static const struct {
	float before, after, threshold;
	short sample;
	int flag;                 /* -1: differential only, no NaN claim */
} cases[] = {
	{ -4.0f, 1.0f, -80.0f, 300, -1 },
	{ 4.0f, 42.0f, 0.0f, 0, 1 },
	{ 0.0f, 42.0f, -80.0f, 0, 0 },
	{ 1.0f, 42.0f, 0.0f, 0, 0 },
	{ 1.0f, 42.0f, -1.0f, 0, 1 },
	{ 1.0f, 42.0f, 1.0f, 0, 0 }
};

int main(void)
{
	const char *fault = getenv("DSPLIB_P4DNAN_FAULT");
	diff_begin("V90Phase4Demodulator: strict keep-rate sentinel");
	dsplibs_debug_level = ref_dsplibs_debug_level = 3;
	for (int v92 = 0; v92 < 2; v92++) {
		for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; c++) {
			long trial = 800000L + v92 * 100 + c;
			short result[2];
			fill(parm_s[0], PARM_SLOT, 0x51edu + v92);
			fill(dem_s[0], DEM_SLOT, 0x9e37u + v92);
			fill(p4d_s[0], P4D_SLOT, 0x37c1u + v92);
			memcpy(parm_s[1], parm_s[0], PARM_SLOT);
			memcpy(dem_s[1], dem_s[0], DEM_SLOT);
			memcpy(p4d_s[1], p4d_s[0], P4D_SLOT);
			for (int s = 0; s < 2; s++) {
				PARAMS(s)->RRN_SILENCE_ECHO_CALC_PERIOD = 36;
				PARAMS(s)->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE = cases[c].threshold;
				DEM(s)->rbsFramePosition = 2u;
				P4D(s).params = PARAMS(s);
				P4D(s).demapper = DEM(s);
				P4D(s).state = P4D_STATE_CALC_ENERGY_AFTER_EC;
				P4D(s).sessionFlag = v92;
				P4D(s).countInState = 41u;
				P4D(s).int_0028 = 0;
				P4D(s).errorEnergyBeforeEC = cases[c].before;
				P4D(s).errorEnergyAfterEC = cases[c].after;
				P4D(s).int_3510 = 0;
				memcpy(parm_before[s], parm_s[s], PARM_SLOT);
				memcpy(guard_before[s], p4d_s[s] + sizeof(V90Phase4Demodulator), 64);
				memcpy(dem_before[s], dem_s[s] + sizeof(V90Demapper), 64);
			}
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			result[0] = v92 ? our_p4d_getv92decision(p4d_s[0], cases[c].sample)
				: our_p4d_getv90decision(p4d_s[0], cases[c].sample);
			result[1] = v92 ? ref_p4d_getv92decision(p4d_s[1], cases[c].sample)
				: ref_p4d_getv90decision(p4d_s[1], cases[c].sample);
			if (fault && !strcmp(fault, "always0")) P4D(0).int_3510 = 0;
			if (fault && !strcmp(fault, "always1")) P4D(0).int_3510 = 1;
			/* Apparatus-only probes at the SAME already-failing synthetic trial. */
			if (fault && strcmp(fault, "always0") && strcmp(fault, "always1") && v92 == 0 && c == 0) {
				if (!strcmp(fault, "return")) result[0] ^= 1;
				else if (!strcmp(fault, "progress")) P4D(0).int_0028 ^= 1;
				else if (!strcmp(fault, "state")) P4D(0).state = P4D_STATE_CALC_ENERGY_AFTER_EC;
				else if (!strcmp(fault, "unrelated")) p4d_s[0][0x100] ^= 1;
				else if (!strcmp(fault, "guard")) p4d_s[0][sizeof(V90Phase4Demodulator)] ^= 1;
				else if (!strcmp(fault, "peer")) DEM(0)->rbsFramePosition ^= 1;
				else if (!strcmp(fault, "parameter")) parm_s[0][0x404] ^= 1;
				else if (!strcmp(fault, "transcript")) dsplibs_debug_printf("injected diagnostic\r\n");
				else diff_eq_int("unknown fault probe", 0, 1, trial);
			}
			dsplib_debug_capture_on = 0;
			diff_eq_int("keep-rate", P4D(0).int_3510, P4D(1).int_3510, trial);
			diff_eq_int("return", result[0], result[1], trial);
			diff_eq_int("p4d.silence", transcript_exact("p4d.silence", trial), 1, trial);
			for (int s = 0; s < 2; s++) {
				long tag = trial * 2 + s;
				diff_eq_int("diagnostic present", dsplib_debug_capture_size(s) > 0, 1, tag);
				diff_eq_int("return expected", result[s], cases[c].sample, tag);
				diff_eq_int("progress expected", P4D(s).int_0028, 0x2a, tag);
				diff_eq_int("state expected", P4D(s).state, P4D_STATE_WAIT_FOR_RT, tag);
				diff_eq_int("count expected", P4D(s).countInState, 0, tag);
				diff_eq_int("cursor expected", DEM(s)->rbsFramePosition, 3, tag);
				diff_eq_int("params pointer", P4D(s).params == PARAMS(s), 1, tag);
				diff_eq_int("demapper pointer", P4D(s).demapper == DEM(s), 1, tag);
				if (cases[c].flag >= 0)
					diff_eq_int("finite/zero flag expected", P4D(s).int_3510, cases[c].flag, tag);
				diff_eq_obj("params immutable", unsigned char[PARM_SLOT], parm_s[s], parm_before[s], tag);
				diff_eq_obj("p4d guard", unsigned char[64], p4d_s[s] + sizeof(V90Phase4Demodulator), guard_before[s], tag);
				diff_eq_obj("demapper guard", unsigned char[64], dem_s[s] + sizeof(V90Demapper), dem_before[s], tag);
				/* Normalize only independently checked pointers and keep-rate
				 * in COPIES. Every other byte, including padding, stays exact. */
				memcpy(canonical[s], p4d_s[s], P4D_SLOT);
				V90Phase4Demodulator *p = (V90Phase4Demodulator *)canonical[s];
				p->params = 0;
				p->demapper = 0;
				p->int_3510 = 0;
			}
			diff_eq_obj("remaining state", unsigned char[P4D_SLOT], canonical[0], canonical[1], trial);
			diff_eq_obj("demapper state", unsigned char[DEM_SLOT], dem_s[0], dem_s[1], trial);
		}
	}
	return diff_end();
}
