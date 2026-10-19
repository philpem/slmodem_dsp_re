/* Native component history: constructors -> EIA6 selection -> parameter
 * copies. Deliberately distinct adjacent tuning values expose shifted access.
 * A separate synthetic representation probe includes signalling NaNs: it
 * establishes byte-copy fidelity, not a reachable modem tuning history. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "dsplib/V90PreFilter.h"
#include "dsplib/modem_params.h"

extern "C" {
unsigned int dsplibs_debug_level;
void *sysdep_malloc(unsigned n) { return malloc(n); }
void sysdep_free(void *p) { free(p); }
void edprintf(const char *, ...) {}
int dsplibs_debug_printf(const char *, ...) { return 0; }
}
static unsigned checks, failed;
static void check(const char *name, bool yes) {
 ++checks;
 if (!yes) { ++failed; fprintf(stderr, "FAIL: %s\n", name); }
}
static unsigned bits(const float &v) { unsigned b; memcpy(&b,&v,4); return b; }
static void put(float &v, unsigned b) { memcpy(&v,&b,4); }
int main() {
 check("native pointers", sizeof(void *) == 8);
 _tagModemParameters mp = {};
 mp.minRate = 2400;
 mp.maxRate = 33600;
 V90Parameters p(&mp);
 V90Phase2Info info(&p);
 p.PROBING_MODE = 13;
 p.HW_CODEC_TYPE = 2;
 p.LINE_CONNECTION_TYPE = 17;
 V90PreFilter filter((__tHardwareCodecTypes__)0, &info, &p);
 check("constructor uses codec member", filter.codecType == 2);
 check("constructor retains owner", filter.params == &p);
 p.MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP = 23;
 p.LOOP_TYPE = 6;
 check("typed EIA6 selection", filter.isV90WithEia6() != 0);
 int oldLoop; memcpy(&oldLoop, (const char *)&p + 0x500, 4);
 check("known old offset selects distinct adjacent field", oldLoop == 23 && oldLoop != p.LOOP_TYPE);
 p.LOOP_TYPE = 3;
 check("typed EIA6 deselection", filter.isV90WithEia6() == 0);
 p.PRE_FILTER_COEF_TYPE = 2;
 p.PRE_FILTER_GAIN = 9;
 p.GERMAN_ISDN_NT1_BOX_FILTER_GAIN = 5;
 p.GERMAN_PBX_PRE_FILTER_GAIN = 7;
 p.LINE_CONNECTION_TYPE = 1;
 filter.selectFilter();
 check("ISDN gain comes from distinct named field", filter.gain == 5);
 p.LINE_CONNECTION_TYPE = 2;
 filter.selectFilter();
 check("PBX gain comes from distinct named field", filter.gain == 7);
 p.LINE_CONNECTION_TYPE = 0;
 filter.selectFilter();
 check("manual gain comes from distinct named field", filter.gain == 9);
 check("manual coefficient type selects bank two", filter.coefficients == &V90PreFilter::preFilterCoefType2[9][0]);
 mp.connectionType = 11;
 mp.clockDeviation = 125;
 // Finite tuning history first, then synthetic raw representations.
 for (unsigned probe=0; probe<2; ++probe) {
  p.ENABLE_EQUALIZER_MMX = -1; p.EIA6_ENABLE_EQUALIZER_MMX = 1000;
  p.SPECTRAL_VERIFIER_ENABLE = -2; p.EIA6_SPECTRAL_VERIFIER_ENABLE = 1001;
  p.USE_RESTRICED_DMIN = -3; p.EIA6_USE_RESTRICED_DMIN = 1002;
  put(p.SPECTRAL_SHAPER_A1, 0xbf000000u + 3); put(p.EIA6_SPECTRAL_SHAPER_A1, probe ? (0x7f800001u + 3) : (0x3f000000u + 3));
  put(p.SPECTRAL_SHAPER_A2, 0xbf000000u + 4); put(p.EIA6_SPECTRAL_SHAPER_A2, probe ? (0x7f800001u + 4) : (0x3f000000u + 4));
  put(p.SPECTRAL_SHAPER_B1, 0xbf000000u + 5); put(p.EIA6_SPECTRAL_SHAPER_B1, probe ? (0x7f800001u + 5) : (0x3f000000u + 5));
  put(p.SPECTRAL_SHAPER_B2, 0xbf000000u + 6); put(p.EIA6_SPECTRAL_SHAPER_B2, probe ? (0x7f800001u + 6) : (0x3f000000u + 6));
  p.SPECTRAL_SHAPER_SR = -8; p.EIA6_SPECTRAL_SHAPER_SR = 1007;
  p.SPECTRAL_SHAPER_ID = -9; p.EIA6_SPECTRAL_SHAPER_ID = 1008;
  put(p.LINEAR_EQU_TRN1D_BETA, 0xbf000000u + 9); put(p.EIA6_LINEAR_EQU_TRN1D_BETA, probe ? (0x7f800001u + 9) : (0x3f000000u + 9));
  put(p.GERMAN_PBX_LINEAR_EQU_DIL_BETA, 0xbf000000u + 10); put(p.EIA6_LINEAR_EQU_DIL_BETA, probe ? (0x7f800001u + 10) : (0x3f000000u + 10));
  put(p.LINEAR_EQU_TRN2D_INITIAL_BETA, 0xbf000000u + 11); put(p.EIA6_LINEAR_EQU_TRN2D_INITIAL_BETA, probe ? (0x7f800001u + 11) : (0x3f000000u + 11));
  put(p.LINEAR_EQU_TRN2D_BETA, 0xbf000000u + 12); put(p.EIA6_LINEAR_EQU_TRN2D_BETA, probe ? (0x7f800001u + 12) : (0x3f000000u + 12));
  put(p.LINEAR_EQU_DATA_BETA, 0xbf000000u + 13); put(p.EIA6_LINEAR_EQU_DATA_BETA, probe ? (0x7f800001u + 13) : (0x3f000000u + 13));
  put(p.GERMAN_PBX_LINEAR_EQU_DIL_MED_UCODE_BETA, 0xbf000000u + 14); put(p.EIA6_LINEAR_EQU_DIL_MED_UCODE_BETA, probe ? (0x7f800001u + 14) : (0x3f000000u + 14));
  put(p.GERMAN_PBX_LINEAR_EQU_DIL_HIGH_UCODE_BETA, 0xbf000000u + 15); put(p.EIA6_LINEAR_EQU_DIL_HIGH_UCODE_BETA, probe ? (0x7f800001u + 15) : (0x3f000000u + 15));
  put(p.DFE_DIL_MED_UCODE_BETA, 0xbf000000u + 16); put(p.EIA6_DFE_DIL_MED_UCODE_BETA, probe ? (0x7f800001u + 16) : (0x3f000000u + 16));
  put(p.DFE_DIL_HIGH_UCODE_BETA, 0xbf000000u + 17); put(p.EIA6_DFE_DIL_HIGH_UCODE_BETA, probe ? (0x7f800001u + 17) : (0x3f000000u + 17));
  p.nofUcodesInTrn2 = -19; p.unnamed_07c = 1018;
  put(p.LINEAR_EQU_DIL_ERROR_RELAX_BETA, 0xbf000000u + 19); put(p.EIA6_LINEAR_EQU_DIL_ERROR_RELAX_BETA, probe ? (0x7f800001u + 19) : (0x3f000000u + 19));
  put(p.DFE_DIL_ERROR_RELAX_BETA, 0xbf000000u + 20); put(p.EIA6_DFE_DIL_ERROR_RELAX_BETA, probe ? (0x7f800001u + 20) : (0x3f000000u + 20));
  p.LINEAR_EQU_FADE_EDGES_CYCLE = -22; p.EIA6_LINEAR_EQU_FADE_EDGES_CYCLE = 1021;
  put(p.LINEAR_EQU_FADE_LEFT_EDGE_RATIO, 0xbf000000u + 22); put(p.EIA6_LINEAR_EQU_FADE_LEFT_EDGE_RATIO, probe ? (0x7f800001u + 22) : (0x3f000000u + 22));
  put(p.LINEAR_EQU_FADE_RIGHT_EDGE_RATIO, 0xbf000000u + 23); put(p.EIA6_LINEAR_EQU_FADE_RIGHT_EDGE_RATIO, probe ? (0x7f800001u + 23) : (0x3f000000u + 23));
  put(p.DFE_DIL_BETA, 0xbf000000u + 24); put(p.EIA6_DFE_DIL_BETA, probe ? (0x7f800001u + 24) : (0x3f000000u + 24));
  put(p.DFE_DATA_BETA, 0xbf000000u + 25); put(p.EIA6_DFE_DATA_BETA, probe ? (0x7f800001u + 25) : (0x3f000000u + 25));
  put(p.BLL_INITIAL_K1, 0xbf000000u + 26); put(p.EIA6_BLL_INITIAL_K1, probe ? (0x7f800001u + 26) : (0x3f000000u + 26));
  put(p.BLL_INITIAL_K2, 0xbf000000u + 27); put(p.EIA6_BLL_INITIAL_K2, probe ? (0x7f800001u + 27) : (0x3f000000u + 27));
  put(p.BLL_FAST_K1, 0xbf000000u + 28); put(p.EIA6_BLL_FAST_K1, probe ? (0x7f800001u + 28) : (0x3f000000u + 28));
  put(p.BLL_FAST_K2, 0xbf000000u + 29); put(p.EIA6_BLL_FAST_K2, probe ? (0x7f800001u + 29) : (0x3f000000u + 29));
  put(p.BLL_MEDIUM_K1, 0xbf000000u + 30); put(p.EIA6_BLL_MEDIUM_K1, probe ? (0x7f800001u + 30) : (0x3f000000u + 30));
  put(p.BLL_MEDIUM_K2, 0xbf000000u + 31); put(p.EIA6_BLL_MEDIUM_K2, probe ? (0x7f800001u + 31) : (0x3f000000u + 31));
  put(p.BLL_SLOW_K1, 0xbf000000u + 32); put(p.EIA6_BLL_SLOW_K1, probe ? (0x7f800001u + 32) : (0x3f000000u + 32));
  put(p.BLL_SLOW_K2, 0xbf000000u + 33); put(p.EIA6_BLL_SLOW_K2, probe ? (0x7f800001u + 33) : (0x3f000000u + 33));
  put(p.BLL_SLOW2_K1, 0xbf000000u + 34); put(p.EIA6_BLL_SLOW2_K1, probe ? (0x7f800001u + 34) : (0x3f000000u + 34));
  put(p.BLL_SLOW2_K2, 0xbf000000u + 35); put(p.EIA6_BLL_SLOW2_K2, probe ? (0x7f800001u + 35) : (0x3f000000u + 35));
  put(p.BLL_DIL_K1, 0xbf000000u + 36); put(p.EIA6_BLL_DIL_K1, probe ? (0x7f800001u + 36) : (0x3f000000u + 36));
  put(p.BLL_DIL_K2, 0xbf000000u + 37); put(p.EIA6_BLL_DIL_K2, probe ? (0x7f800001u + 37) : (0x3f000000u + 37));
  put(p.BLL_TRN2_INITIAL_K1, 0xbf000000u + 38); put(p.EIA6_BLL_TRN2_INITIAL_K1, probe ? (0x7f800001u + 38) : (0x3f000000u + 38));
  put(p.BLL_TRN2_INITIAL_K2, 0xbf000000u + 39); put(p.EIA6_BLL_TRN2_INITIAL_K2, probe ? (0x7f800001u + 39) : (0x3f000000u + 39));
  put(p.BLL_TRN2_K1, 0xbf000000u + 40); put(p.EIA6_BLL_TRN2_K1, probe ? (0x7f800001u + 40) : (0x3f000000u + 40));
  put(p.BLL_TRN2_K2, 0xbf000000u + 41); put(p.EIA6_BLL_TRN2_K2, probe ? (0x7f800001u + 41) : (0x3f000000u + 41));
  put(p.BLL_STEADY_STATE_K1, 0xbf000000u + 42); put(p.EIA6_BLL_STEADY_STATE_K1, probe ? (0x7f800001u + 42) : (0x3f000000u + 42));
  put(p.BLL_STEADY_STATE_K2, 0xbf000000u + 43); put(p.EIA6_BLL_STEADY_STATE_K2, probe ? (0x7f800001u + 43) : (0x3f000000u + 43));
  p.MAX_NOF_V90_RETRAINS = -45; p.EIA6_MAX_NOF_V90_RETRAINS = 1044;
  put(p.PDSNR_THRESHOLD_IN_PHASE3, 0xbf000000u + 45); put(p.EIA6_PDSNR_THRESHOLD_IN_PHASE3, probe ? (0x7f800001u + 45) : (0x3f000000u + 45));
  put(p.PDSNR_THRESHOLD_IN_PHASE4, 0xbf000000u + 46); put(p.EIA6_PDSNR_THRESHOLD_IN_PHASE4, probe ? (0x7f800001u + 46) : (0x3f000000u + 46));
  put(p.TRN1D_ERROR_FOR_V34_FALLBACK, 0xbf000000u + 47); put(p.EIA6_TRN1D_ERROR_FOR_V34_FALLBACK, probe ? (0x7f800001u + 47) : (0x3f000000u + 47));
  filter.setParamEia6();
  check("copy ENABLE_EQUALIZER_MMX", p.ENABLE_EQUALIZER_MMX == p.EIA6_ENABLE_EQUALIZER_MMX);
  check("copy SPECTRAL_VERIFIER_ENABLE", p.SPECTRAL_VERIFIER_ENABLE == p.EIA6_SPECTRAL_VERIFIER_ENABLE);
  check("copy USE_RESTRICED_DMIN", p.USE_RESTRICED_DMIN == p.EIA6_USE_RESTRICED_DMIN);
  check("copy SPECTRAL_SHAPER_A1", bits(p.SPECTRAL_SHAPER_A1) == bits(p.EIA6_SPECTRAL_SHAPER_A1));
  check("copy SPECTRAL_SHAPER_A2", bits(p.SPECTRAL_SHAPER_A2) == bits(p.EIA6_SPECTRAL_SHAPER_A2));
  check("copy SPECTRAL_SHAPER_B1", bits(p.SPECTRAL_SHAPER_B1) == bits(p.EIA6_SPECTRAL_SHAPER_B1));
  check("copy SPECTRAL_SHAPER_B2", bits(p.SPECTRAL_SHAPER_B2) == bits(p.EIA6_SPECTRAL_SHAPER_B2));
  check("copy SPECTRAL_SHAPER_SR", p.SPECTRAL_SHAPER_SR == p.EIA6_SPECTRAL_SHAPER_SR);
  check("copy SPECTRAL_SHAPER_ID", p.SPECTRAL_SHAPER_ID == p.EIA6_SPECTRAL_SHAPER_ID);
  check("copy LINEAR_EQU_TRN1D_BETA", bits(p.LINEAR_EQU_TRN1D_BETA) == bits(p.EIA6_LINEAR_EQU_TRN1D_BETA));
  check("copy GERMAN_PBX_LINEAR_EQU_DIL_BETA", bits(p.GERMAN_PBX_LINEAR_EQU_DIL_BETA) == bits(p.EIA6_LINEAR_EQU_DIL_BETA));
  check("copy LINEAR_EQU_TRN2D_INITIAL_BETA", bits(p.LINEAR_EQU_TRN2D_INITIAL_BETA) == bits(p.EIA6_LINEAR_EQU_TRN2D_INITIAL_BETA));
  check("copy LINEAR_EQU_TRN2D_BETA", bits(p.LINEAR_EQU_TRN2D_BETA) == bits(p.EIA6_LINEAR_EQU_TRN2D_BETA));
  check("copy LINEAR_EQU_DATA_BETA", bits(p.LINEAR_EQU_DATA_BETA) == bits(p.EIA6_LINEAR_EQU_DATA_BETA));
  check("copy GERMAN_PBX_LINEAR_EQU_DIL_MED_UCODE_BETA", bits(p.GERMAN_PBX_LINEAR_EQU_DIL_MED_UCODE_BETA) == bits(p.EIA6_LINEAR_EQU_DIL_MED_UCODE_BETA));
  check("copy GERMAN_PBX_LINEAR_EQU_DIL_HIGH_UCODE_BETA", bits(p.GERMAN_PBX_LINEAR_EQU_DIL_HIGH_UCODE_BETA) == bits(p.EIA6_LINEAR_EQU_DIL_HIGH_UCODE_BETA));
  check("copy DFE_DIL_MED_UCODE_BETA", bits(p.DFE_DIL_MED_UCODE_BETA) == bits(p.EIA6_DFE_DIL_MED_UCODE_BETA));
  check("copy DFE_DIL_HIGH_UCODE_BETA", bits(p.DFE_DIL_HIGH_UCODE_BETA) == bits(p.EIA6_DFE_DIL_HIGH_UCODE_BETA));
  check("copy nofUcodesInTrn2", p.nofUcodesInTrn2 == p.unnamed_07c);
  check("copy LINEAR_EQU_DIL_ERROR_RELAX_BETA", bits(p.LINEAR_EQU_DIL_ERROR_RELAX_BETA) == bits(p.EIA6_LINEAR_EQU_DIL_ERROR_RELAX_BETA));
  check("copy DFE_DIL_ERROR_RELAX_BETA", bits(p.DFE_DIL_ERROR_RELAX_BETA) == bits(p.EIA6_DFE_DIL_ERROR_RELAX_BETA));
  check("copy LINEAR_EQU_FADE_EDGES_CYCLE", p.LINEAR_EQU_FADE_EDGES_CYCLE == p.EIA6_LINEAR_EQU_FADE_EDGES_CYCLE);
  check("copy LINEAR_EQU_FADE_LEFT_EDGE_RATIO", bits(p.LINEAR_EQU_FADE_LEFT_EDGE_RATIO) == bits(p.EIA6_LINEAR_EQU_FADE_LEFT_EDGE_RATIO));
  check("copy LINEAR_EQU_FADE_RIGHT_EDGE_RATIO", bits(p.LINEAR_EQU_FADE_RIGHT_EDGE_RATIO) == bits(p.EIA6_LINEAR_EQU_FADE_RIGHT_EDGE_RATIO));
  check("copy DFE_DIL_BETA", bits(p.DFE_DIL_BETA) == bits(p.EIA6_DFE_DIL_BETA));
  check("copy DFE_DATA_BETA", bits(p.DFE_DATA_BETA) == bits(p.EIA6_DFE_DATA_BETA));
  check("copy BLL_INITIAL_K1", bits(p.BLL_INITIAL_K1) == bits(p.EIA6_BLL_INITIAL_K1));
  check("copy BLL_INITIAL_K2", bits(p.BLL_INITIAL_K2) == bits(p.EIA6_BLL_INITIAL_K2));
  check("copy BLL_FAST_K1", bits(p.BLL_FAST_K1) == bits(p.EIA6_BLL_FAST_K1));
  check("copy BLL_FAST_K2", bits(p.BLL_FAST_K2) == bits(p.EIA6_BLL_FAST_K2));
  check("copy BLL_MEDIUM_K1", bits(p.BLL_MEDIUM_K1) == bits(p.EIA6_BLL_MEDIUM_K1));
  check("copy BLL_MEDIUM_K2", bits(p.BLL_MEDIUM_K2) == bits(p.EIA6_BLL_MEDIUM_K2));
  check("copy BLL_SLOW_K1", bits(p.BLL_SLOW_K1) == bits(p.EIA6_BLL_SLOW_K1));
  check("copy BLL_SLOW_K2", bits(p.BLL_SLOW_K2) == bits(p.EIA6_BLL_SLOW_K2));
  check("copy BLL_SLOW2_K1", bits(p.BLL_SLOW2_K1) == bits(p.EIA6_BLL_SLOW2_K1));
  check("copy BLL_SLOW2_K2", bits(p.BLL_SLOW2_K2) == bits(p.EIA6_BLL_SLOW2_K2));
  check("copy BLL_DIL_K1", bits(p.BLL_DIL_K1) == bits(p.EIA6_BLL_DIL_K1));
  check("copy BLL_DIL_K2", bits(p.BLL_DIL_K2) == bits(p.EIA6_BLL_DIL_K2));
  check("copy BLL_TRN2_INITIAL_K1", bits(p.BLL_TRN2_INITIAL_K1) == bits(p.EIA6_BLL_TRN2_INITIAL_K1));
  check("copy BLL_TRN2_INITIAL_K2", bits(p.BLL_TRN2_INITIAL_K2) == bits(p.EIA6_BLL_TRN2_INITIAL_K2));
  check("copy BLL_TRN2_K1", bits(p.BLL_TRN2_K1) == bits(p.EIA6_BLL_TRN2_K1));
  check("copy BLL_TRN2_K2", bits(p.BLL_TRN2_K2) == bits(p.EIA6_BLL_TRN2_K2));
  check("copy BLL_STEADY_STATE_K1", bits(p.BLL_STEADY_STATE_K1) == bits(p.EIA6_BLL_STEADY_STATE_K1));
  check("copy BLL_STEADY_STATE_K2", bits(p.BLL_STEADY_STATE_K2) == bits(p.EIA6_BLL_STEADY_STATE_K2));
  check("copy MAX_NOF_V90_RETRAINS", p.MAX_NOF_V90_RETRAINS == p.EIA6_MAX_NOF_V90_RETRAINS);
  check("copy PDSNR_THRESHOLD_IN_PHASE3", bits(p.PDSNR_THRESHOLD_IN_PHASE3) == bits(p.EIA6_PDSNR_THRESHOLD_IN_PHASE3));
  check("copy PDSNR_THRESHOLD_IN_PHASE4", bits(p.PDSNR_THRESHOLD_IN_PHASE4) == bits(p.EIA6_PDSNR_THRESHOLD_IN_PHASE4));
  check("copy TRN1D_ERROR_FOR_V34_FALLBACK", bits(p.TRN1D_ERROR_FOR_V34_FALLBACK) == bits(p.EIA6_TRN1D_ERROR_FOR_V34_FALLBACK));
  check("typed modem clock deviation", p.INITIAL_BAUD_OFFSET == 125 * 0.001f);
  check("modem connection neighbor untouched", mp.connectionType == 11);
 }
 printf("native V90 parameter components: %u checks, %u failed; 1 old-offset negative control, 2 copy histories\n", checks, failed);
 return failed ? 1 : 0;
}
