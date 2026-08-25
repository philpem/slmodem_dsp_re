/*
 * V92Parameters.cpp -- the V.92 parameter block's own members.
 *
 * SEPARATE CLASS, SEPARATE ALLOCATION, and neither one's destructor calls the
 * other's; V92Parameters.h says why that is not an assumption.  The contrast
 * with the V.90 block is the interesting part of this file:
 *
 *   - `setToDefault` here is 477 bytes and 54 stores, ALL of them constants.
 *     It reads nothing, branches nowhere, calls nothing, and does not touch
 *     `modemParams`.  The V.90 one is 3,589 bytes, reads four things and
 *     builds a rate mask in a loop.
 *   - the constructor does NOT call `loadModemParamsData`, because there is
 *     no V.92 equivalent -- the modem block's contribution is made once, to
 *     the V.90 object.
 *   - `init()` tail-calls `loadParams` and stops; the V.90 `init()` has a
 *     third step after it.
 *
 * The 54 stores were extracted by the same abstract interpretation as the
 * V.90 block's (finding 860's clobber rule; see V90Parameters.cpp), which
 * resolved 54 of 54 with nothing computed and nothing unknown.  Every one of
 * the 54 offsets is also read by `loadParams` under a name, so unlike the
 * V.90 block there is not one `unnamed_*` field here and not one type to
 * argue about -- the two readings cover the identical set (finding 861).
 */

#include "dsplib/V92Parameters.h"

#include "dsplib/modem_params.h"
#include "dsplib/Vparser.h"

void
V92Parameters::setToDefault()
{
	VPCM_SESSION_TYPE = 1;
	V92_PHASE2_INFO_A_OR_MU = 0;
	V92_PHASE2_INFO_RTD = 0;
	V92_PHASE2_INFO_UINFO = 78;
	V92_PHASE2_INFO_MAX_TX_POWER = 23;
	V92_PHASE2_INFO_TX_POWER_MEASURE_POINT = 1;
	V92_EXTEND_EU = 0;
	V92_DELAY_BEFOR_STEADY_STATE = 5000;
	/*
	 * SIMULATION_SWITCH (+0x28) BEFORE START_DELAY (+0x24), which is not
	 * this struct's offset order and is not an accident.  Two independent
	 * observables agree on it: the blob's `setToDefault` emits
	 * `movl $0x0,0x28(%eax)` ahead of `movl $0xfa0,0x24(%eax)`, and
	 * `loadParams` below -- 1,384 bytes and byte-exact against the object,
	 * 54 calls to an external function GCC may not reorder, so its emitted
	 * call order IS its source order -- reads the two in this same order.
	 * The author wrote one field list and used it twice.
	 */
	V92_RRN_SIMULATION_SWITCH = 0;
	V92_RRN_START_DELAY = 4000;
	V92_SILENCE_RRN_REQUESTE = 0;
	V92_RRN_TRN2U_DD_LENGTH = 12000;
	V92_MAX_SILENCE_LENGTH_FLAG = 0;
	V92_SILENCE_LENGTH = 2400;
	V92_FPE_SIMULATION_SWITCH = 0;
	V92_FPE_START_DELAY = 20000;
	V92A_DIGITAL_RATE_MASK = 0xfffffff;
	V92A_MAX_SPECTRAL_SHAPER_LOOKAHEAD = 3;
	V92A_PHASE4_CONSTELLATION = 0;
	V92A_RRN_CONSTELLATION = 0;
	V92_PHASE4_CONSTELLATION = 0;
	V92_RRN_CONSTELLATION = 0;
	V92_NOF_FILTER_SECTIONS = 3;
	V92_MAX_TOTAL_NOF_COEFFS = 3;
	V92_MAX_NOF_COEFFS_IN_EACH_SECTION = 3;
	V92_APPLY_TX_SHAPING_FILTER = 1;
	V92_ECHO_FILTER_LENGTH = 180;
	V92_ECHO_INITIAL_DELAY = 840;
	V92_ECHO_DELAY_OFFSET = -14;
	/*
	 * THE TWO DECAYS, THEN THE TWO DURATIONS, THEN THE TWO BETAS -- not the
	 * struct's offset order (+0x78 beta, +0x7c decay, +0x80 beta, +0x84
	 * decay, +0x88 dur, +0x8c dur) and not `loadParams`' order either, so
	 * this one is NOT the shared field list the swap above is.  All 6! = 720
	 * orderings of these six statements were compiled on the period
	 * compiler; they give 720 DISTINCT emissions -- the map is a bijection,
	 * so the harness demonstrably fires -- and exactly ONE reaches
	 * positional byte identity.  Nearest near-miss is 6 differing bytes.
	 * A unique preimage, so the order is decoded rather than fitted
	 * (7782's ruling).  Finding 7840.
	 */
	V92_ECHO_FAST_DECAY_FACTOR = 1.0f;
	V92_ECHO_SLOW_DECAY_FACTOR = 0.9987f;
	V92_ECHO_FAST_UPDATE_DURATION = 9000;
	V92_ECHO_SLOW_UPDATE_DURATION = 11000;
	V92_ECHO_FAST_BETA_FACTOR = 1.953125e-10f;
	V92_ECHO_SLOW_BETA_FACTOR = 1.8554687e-10f;
	V92_RESAMPLER_RESULOTION = 1600;
	V92_LINEAR_EQU_LENGTH = 128;
	V92_LE_PHASE_3_BETA = 3e-10f;
	V92_LE_BETA_I_DURATION = 5000;
	V92_LE_PHASE_3_BETA_II = 1e-10f;
	V92_DFE_LENGTH = 8;
	V92_DFE_PHASE_3_BETA = 6e-10f;
	V92_DFE_TRN1U_FREEZE_DURATION = 1700;
	ERROR_ENERGY_PRINT_PERIOD_PHASE3 = 768;
	ERROR_ENERGY_PRINT_PERIOD_PHASE4 = 768;
	V92_AGC_NOMINAL_ENERGY = 24000000.0f;
	V92_AGC_K = 0.6f;
	V92_AGC_BLOCK_LEN = 150;
	V92_AGC_ADAPTATION_DURATION = 1000;
	SU_DETECTOR_ENERGY_THRESHOLD = 300.0f;
	SU_DETECTOR_POSITIVE_CORR_THRESHOLD = 0.8f;
	SU_DETECTOR_NEGATIVE_CORR_THRESHOLD = 0.4f;
	SU_DETECTOR_DETECTION_COUNTER_THRESHOLD = 80;
	MODULATOR_QUEUE_LENGTH = 1000;
}

/*
 * `loadParams(char *)` -- 1,384 bytes, 54 calls, 54 distinct offsets, no
 * alias and no hole.  Same shape and same provenance as the V.90 member; see
 * the long comment on that one in V90Parameters.cpp.  `this` arrives at
 * `0x20(%esp)` here against `0x4(%esp)` in `setToDefault`, which is the trap
 * finding 861 records: reading the second as the first shifts every offset
 * down by four and yields a map that is wrong in all 54 lines and looks fine.
 */
void
V92Parameters::loadParams(char *paramFile)
{
	Vparser_read_int(paramFile, "VPCM_SESSION_TYPE", &VPCM_SESSION_TYPE);
	Vparser_read_int(paramFile, "V92_PHASE2_INFO_A_OR_MU", &V92_PHASE2_INFO_A_OR_MU);
	Vparser_read_int(paramFile, "V92_PHASE2_INFO_RTD", &V92_PHASE2_INFO_RTD);
	Vparser_read_int(paramFile, "V92_PHASE2_INFO_UINFO", &V92_PHASE2_INFO_UINFO);
	Vparser_read_int(paramFile, "V92_PHASE2_INFO_MAX_TX_POWER", &V92_PHASE2_INFO_MAX_TX_POWER);
	Vparser_read_int(paramFile, "V92_PHASE2_INFO_TX_POWER_MEASURE_POINT", &V92_PHASE2_INFO_TX_POWER_MEASURE_POINT);
	Vparser_read_int(paramFile, "V92_EXTEND_EU", &V92_EXTEND_EU);
	Vparser_read_int(paramFile, "V92_DELAY_BEFOR_STEADY_STATE", &V92_DELAY_BEFOR_STEADY_STATE);
	Vparser_read_int(paramFile, "V92_RRN_SIMULATION_SWITCH", &V92_RRN_SIMULATION_SWITCH);
	Vparser_read_int(paramFile, "V92_RRN_START_DELAY", &V92_RRN_START_DELAY);
	Vparser_read_int(paramFile, "V92_SILENCE_RRN_REQUESTE", &V92_SILENCE_RRN_REQUESTE);
	Vparser_read_int(paramFile, "V92_RRN_TRN2U_DD_LENGTH", &V92_RRN_TRN2U_DD_LENGTH);
	Vparser_read_int(paramFile, "V92_MAX_SILENCE_LENGTH_FLAG", &V92_MAX_SILENCE_LENGTH_FLAG);
	Vparser_read_int(paramFile, "V92_SILENCE_LENGTH", &V92_SILENCE_LENGTH);
	Vparser_read_int(paramFile, "V92_FPE_SIMULATION_SWITCH", &V92_FPE_SIMULATION_SWITCH);
	Vparser_read_int(paramFile, "V92_FPE_START_DELAY", &V92_FPE_START_DELAY);
	Vparser_read_int(paramFile, "V92A_DIGITAL_RATE_MASK", &V92A_DIGITAL_RATE_MASK);
	Vparser_read_int(paramFile, "V92A_MAX_SPECTRAL_SHAPER_LOOKAHEAD", &V92A_MAX_SPECTRAL_SHAPER_LOOKAHEAD);
	Vparser_read_int(paramFile, "V92A_PHASE4_CONSTELLATION", &V92A_PHASE4_CONSTELLATION);
	Vparser_read_int(paramFile, "V92A_RRN_CONSTELLATION", &V92A_RRN_CONSTELLATION);
	Vparser_read_int(paramFile, "V92_PHASE4_CONSTELLATION", &V92_PHASE4_CONSTELLATION);
	Vparser_read_int(paramFile, "V92_RRN_CONSTELLATION", &V92_RRN_CONSTELLATION);
	Vparser_read_int(paramFile, "V92_NOF_FILTER_SECTIONS", &V92_NOF_FILTER_SECTIONS);
	Vparser_read_int(paramFile, "V92_MAX_TOTAL_NOF_COEFFS", &V92_MAX_TOTAL_NOF_COEFFS);
	Vparser_read_int(paramFile, "V92_MAX_NOF_COEFFS_IN_EACH_SECTION", &V92_MAX_NOF_COEFFS_IN_EACH_SECTION);
	Vparser_read_int(paramFile, "V92_APPLY_TX_SHAPING_FILTER", &V92_APPLY_TX_SHAPING_FILTER);
	Vparser_read_int(paramFile, "V92_ECHO_FILTER_LENGTH", &V92_ECHO_FILTER_LENGTH);
	Vparser_read_int(paramFile, "V92_ECHO_INITIAL_DELAY", &V92_ECHO_INITIAL_DELAY);
	Vparser_read_int(paramFile, "V92_ECHO_DELAY_OFFSET", &V92_ECHO_DELAY_OFFSET);
	Vparser_read_float(paramFile, "V92_ECHO_FAST_BETA_FACTOR", &V92_ECHO_FAST_BETA_FACTOR);
	Vparser_read_float(paramFile, "V92_ECHO_FAST_DECAY_FACTOR", &V92_ECHO_FAST_DECAY_FACTOR);
	Vparser_read_float(paramFile, "V92_ECHO_SLOW_BETA_FACTOR", &V92_ECHO_SLOW_BETA_FACTOR);
	Vparser_read_float(paramFile, "V92_ECHO_SLOW_DECAY_FACTOR", &V92_ECHO_SLOW_DECAY_FACTOR);
	Vparser_read_int(paramFile, "V92_ECHO_FAST_UPDATE_DURATION", &V92_ECHO_FAST_UPDATE_DURATION);
	Vparser_read_int(paramFile, "V92_ECHO_SLOW_UPDATE_DURATION", &V92_ECHO_SLOW_UPDATE_DURATION);
	Vparser_read_int(paramFile, "V92_RESAMPLER_RESULOTION", &V92_RESAMPLER_RESULOTION);
	Vparser_read_int(paramFile, "V92_LINEAR_EQU_LENGTH", &V92_LINEAR_EQU_LENGTH);
	Vparser_read_float(paramFile, "V92_LE_PHASE_3_BETA", &V92_LE_PHASE_3_BETA);
	Vparser_read_int(paramFile, "V92_LE_BETA_I_DURATION", &V92_LE_BETA_I_DURATION);
	Vparser_read_float(paramFile, "V92_LE_PHASE_3_BETA_II", &V92_LE_PHASE_3_BETA_II);
	Vparser_read_int(paramFile, "V92_DFE_LENGTH", &V92_DFE_LENGTH);
	Vparser_read_float(paramFile, "V92_DFE_PHASE_3_BETA", &V92_DFE_PHASE_3_BETA);
	Vparser_read_int(paramFile, "V92_DFE_TRN1U_FREEZE_DURATION", &V92_DFE_TRN1U_FREEZE_DURATION);
	Vparser_read_int(paramFile, "ERROR_ENERGY_PRINT_PERIOD_PHASE3", &ERROR_ENERGY_PRINT_PERIOD_PHASE3);
	Vparser_read_int(paramFile, "ERROR_ENERGY_PRINT_PERIOD_PHASE4", &ERROR_ENERGY_PRINT_PERIOD_PHASE4);
	Vparser_read_float(paramFile, "V92_AGC_NOMINAL_ENERGY", &V92_AGC_NOMINAL_ENERGY);
	Vparser_read_float(paramFile, "V92_AGC_K", &V92_AGC_K);
	Vparser_read_int(paramFile, "V92_AGC_BLOCK_LEN", &V92_AGC_BLOCK_LEN);
	Vparser_read_int(paramFile, "V92_AGC_ADAPTATION_DURATION", &V92_AGC_ADAPTATION_DURATION);
	Vparser_read_float(paramFile, "SU_DETECTOR_ENERGY_THRESHOLD", &SU_DETECTOR_ENERGY_THRESHOLD);
	Vparser_read_float(paramFile, "SU_DETECTOR_POSITIVE_CORR_THRESHOLD", &SU_DETECTOR_POSITIVE_CORR_THRESHOLD);
	Vparser_read_float(paramFile, "SU_DETECTOR_NEGATIVE_CORR_THRESHOLD", &SU_DETECTOR_NEGATIVE_CORR_THRESHOLD);
	Vparser_read_int(paramFile, "SU_DETECTOR_DETECTION_COUNTER_THRESHOLD", &SU_DETECTOR_DETECTION_COUNTER_THRESHOLD);
	Vparser_read_int(paramFile, "MODULATOR_QUEUE_LENGTH", &MODULATOR_QUEUE_LENGTH);
}

void
V92Parameters::init()
{
	setToDefault();

	/*
	 * `mov (%ebx),%eax; mov 0x78(%eax),%eax; test %eax,%eax` at 0x15e60,
	 * then a call to `loadParams` and `ret` -- so unlike the V.90 `init()`
	 * there is nothing after it and this member ends here.  Finding 879 for
	 * why the callee used to be left out, 6400 for the oracle that tested
	 * it.  The test runs this with the pointer null and non-null.
	 */
	if (modemParams->paramFile)
		loadParams(modemParams->paramFile);
}

/*
 * `modemParams = mp; init();` -- three stores' worth less than the V.90 one,
 * because nothing here reads a field that `setToDefault` does not write.
 */
V92Parameters::V92Parameters(_tagModemParameters *mp)
{
	modemParams = mp;
	init();
}

/* One byte: `ret`. */
V92Parameters::~V92Parameters()
{
}
