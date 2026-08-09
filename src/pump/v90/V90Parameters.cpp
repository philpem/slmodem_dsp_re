/*
 * V90Parameters.cpp -- the V.90 parameter block's own members.
 *
 * The LAYOUT is in include/dsplib/V90Parameters.h and is frozen; this file is
 * the behaviour.  Six of the class's seven members are here.
 *
 * THE SEVENTH, `loadParams(char *)`, IS DELIBERATELY ABSENT, and the two call
 * sites that would reach it are marked below.  The argument is finding 879's
 * and it is not "it is only diagnostics": `Vparser_read_int` and
 * `Vparser_read_float` are three bytes each in the shipped object -- `xor
 * %eax,%eax; ret` -- and the two `loadParams` members are the only callers of
 * either, anywhere in `.text`.  So all 295 of its calls write nothing, both
 * arms of the object's own `if (paramFile)` leave identical state, and the
 * test drives BOTH arms against the blob rather than asserting that.
 *
 * WHERE EVERY DEFAULT COMES FROM.  `setToDefault` is 3,589 bytes of stores
 * and it was not read by eye.  `tools/dis.py`'s output was abstract-
 * interpreted -- integer registers and the whole x87 stack -- under finding
 * 860's rule that any unmodelled instruction clobbers what it writes, so a
 * form the interpreter did not know became an explicit unknown rather than a
 * stale value that reads as a success.  338 of the 340 stores resolved; the
 * two that did not are the two that are genuinely computed, and both are
 * written out by hand below.  The offsets agree with finding 861's separate
 * walk, store for store.
 *
 * WHY NINE FIELDS TAKE A HEXADECIMAL BIT PATTERN.  Nine of the fifty-one
 * `unnamed_*` slots hold FLOATS where the header declares `int` -- two proved
 * by `fsts`, seven by an exact round decimal float bit pattern sitting among
 * float neighbours.  The header is frozen and a field's type is part of its
 * layout, so this file writes the bit pattern through the declared type,
 * which leaves the identical four bytes and reports the discovery instead of
 * acting on it.  Finding 878.
 *
 * `setToDefault` IS NOT A PURE WRITER.  It dereferences `modemParams` for the
 * two rate limits and for one flag bit, and it reads its own
 * `SENSITIVE_ISP_DETECTED` and `MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP` -- the
 * two fields finding 861 records as read-but-never-defaulted, which is
 * exactly why the constructor sets them before calling it.
 *
 * THE RATE MASK IS BUILT WITH THE INDEX RUNNING DOWN.  `for (i = 14; i >= 2;
 * i--) mask = mask * 2 | in_range(i)` puts index 2's bit at the bottom, so
 * bit (i - 2) means "index i is allowed" and index i is 2400 * i bit/s.  The
 * object reloads the accumulator from the field at the top of every iteration
 * (`mov 0x48(%ebp),%edi` inside the body) and writes it back at the bottom,
 * so the field, not a register, carries the loop.
 */

#include "dsplib/V90Parameters.h"

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/modem_params.h"

/*
 * The object divides by 2400 with the unsigned magic-number sequence GCC
 * emits for a constant unsigned divide (0x1b4e81b5, product's high half
 * shifted right 8), so this is a plain unsigned `/ 2400` and not a shift.
 */
#define V90_RATE_STEP		2400u

/* The upstream rate indices the mask covers, inclusive at both ends. */
#define V90_MIN_RATE_INDEX	2u
#define V90_MAX_RATE_INDEX	14u

void
V90Parameters::setToDefault()
{
	unsigned int minIndex = modemParams->minRate / V90_RATE_STEP;
	unsigned int maxIndex = modemParams->maxRate / V90_RATE_STEP;
	int i;

	PROBING_MODE = 0;
	HW_CODEC_TYPE = -1;
	LINE_CONNECTION_TYPE = -1;
	ENABLE_EQUALIZER_MMX = 1;
	EIA6_ENABLE_EQUALIZER_MMX = 0;
	PHASE2_INFO_A_OR_MU = 0;
	PHASE2_INFO_RTD = 0;
	PHASE2_INFO_UINFO = 78;
	PHASE2_INFO_MAX_TX_POWER = 23;
	PHASE2_INFO_TX_POWER_MEASURE_POINT = 1;
	DIGITAL_RATE_MASK = 0xfffffff;
	MAX_SPECTRAL_SHAPER_LOOKAHEAD = 3;
	V34_PHASE4_CONSTELLATION = 0;
	V34_RRN_CONSTELLATION = 0;
	V92_DIGITAL_RATE_MASK = 0xfffffff;
	V92_MAX_SPECTRAL_SHAPER_LOOKAHEAD = 3;
	V92_JD_PHASE = 0.85f;
	ANALOG_RATE_MASK = 0;
	/* +0x048 ANALOG_RATE_MASK -- computed, see below */
	PRE_FILTER_GAIN = -1;
	PRE_FILTER_COEF_TYPE = -1;
	GERMAN_ISDN_NT1_BOX_FILTER_GAIN = 12;
	GERMAN_PBX_PRE_FILTER_GAIN = 12;
	AGC_NOMINAL_ENERGY = 14000000.0f;
	AGC_K = 0.6f;
	AGC_BLOCK_LEN = 150;
	AGC_ADAPTATION_DURATION = 1000;
	unnamed_06c = 0x3f800000;	/* the object stores 1.0f here -- finding 878 */
	unnamed_070 = 0x3f19999a;	/* the object stores 0.6f here -- finding 878 */
	unnamed_074 = 92;
	unnamed_078 = 8;
	unnamed_07c = 4;
	unnamed_080 = 4;
	INITIAL_BAUD_OFFSET = 0.0f;
	BLL_INITIAL_K1 = 0.002f;
	BLL_INITIAL_K2 = 0.0f;
	BLL_FAST_K1 = 0.00113f;
	BLL_FAST_K2 = 0.0000003f;
	BLL_MEDIUM_K1 = 0.0002f;
	BLL_MEDIUM_K2 = 0.000000025f;
	BLL_SLOW_K1 = 0.00015f;
	BLL_SLOW_K2 = 1.875e-08f;
	BLL_SLOW2_K1 = 0.00006f;
	BLL_SLOW2_K2 = 0.000000004f;
	BLL_DIL_K1 = 0.000003f;
	BLL_DIL_K2 = 0.0f;
	BLL_TRN2_INITIAL_K1 = 0.00003f;
	BLL_TRN2_INITIAL_K2 = 7e-12f;
	BLL_TRN2_K1 = 0.00002f;
	BLL_TRN2_K2 = 5e-12f;
	BLL_STEADY_STATE_K1 = 0.000003f;
	BLL_STEADY_STATE_K2 = 1e-13f;
	BLL_PRE_ANSPCM_K1 = 0.0005f;
	BLL_PRE_ANSPCM_K2 = 0.0f;
	BLL_TRN1_QC_INITIAL_K1 = 0.0002f;
	BLL_TRN1_QC_INITIAL_K2 = 0.0f;
	BLL_TRN1_QC_FAST_K1 = 0.0005f;
	BLL_TRN1_QC_FAST_K2 = 7e-12f;
	BLL_TRN1_QC_MEDIUM_K1 = 0.0003f;
	BLL_TRN1_QC_MEDIUM_K2 = 5e-12f;
	BLL_TRN1_QC_SLOW_K2 = 0.0001f;
	unnamed_0f4 = 0x2c0cbccc;	/* the object stores 2e-12f here -- finding 878 */
	BLL_TRN1D_INITIAL_TO_FAST_DURATION = 2000;
	BLL_TRN1D_FAST_TO_SLOW_DURATION = 7200;
	unnamed_100 = 6000;
	unnamed_104 = 1000;
	unnamed_108 = 4000;
	unnamed_10c = 4000;
	EIA6_BLL_INITIAL_K1 = 0.00005f;
	EIA6_BLL_INITIAL_K2 = 0.0f;
	EIA6_BLL_FAST_K1 = 0.00003f;
	EIA6_BLL_FAST_K2 = 7e-12f;
	EIA6_BLL_MEDIUM_K1 = 0.000015f;
	EIA6_BLL_MEDIUM_K2 = 3e-12f;
	EIA6_BLL_SLOW_K1 = 0.000005f;
	EIA6_BLL_SLOW_K2 = 2e-12f;
	EIA6_BLL_SLOW2_K1 = 0.000003f;
	EIA6_BLL_SLOW2_K2 = 1e-12f;
	EIA6_BLL_DIL_K1 = 0.0000008f;
	EIA6_BLL_DIL_K2 = 0.0f;
	EIA6_BLL_TRN2_INITIAL_K1 = 0.000003f;
	EIA6_BLL_TRN2_INITIAL_K2 = 1e-12f;
	EIA6_BLL_TRN2_K1 = 0.000001f;
	EIA6_BLL_TRN2_K2 = 3e-13f;
	EIA6_BLL_STEADY_STATE_K1 = 0.0000008f;
	EIA6_BLL_STEADY_STATE_K2 = 5e-14f;
	EIA6_BLL_TRN1D_INITIAL_TO_FAST_DURATION = 1000;
	EIA6_BLL_TRN1D_FAST_TO_SLOW_DURATION = 8000;
	TIMING_HISTORY_EVALUATION_ENABLED = 1;
	TIMING_HISTORY_EVALUATION_BUFFER_LENGTH = 100;
	TIMING_HISTORY_EVALUATION_PERIOD = 1500;
	TIMING_OFFESET_MIN_STD_FOR_SAVE = 0.1f;
	LINEAR_EQU_LENGTH = 300;
	LINEAR_EQU_HISTORY_LENGTH = 500;
	LINEAR_EQU_FADE_EDGES_CYCLE = 20;
	LINEAR_EQU_FADE_LEFT_EDGE_RATIO = 0.15f;
	LINEAR_EQU_FADE_RIGHT_EDGE_RATIO = 0.15f;
	LINEAR_EQU_CURSOR_PLACE = -1;
	LINEAR_EQU_TRN1D_BETA = 1e-10f;
	GERMAN_PBX_LINEAR_EQU_DIL_BETA = 5e-11f;
	GERMAN_PBX_LINEAR_EQU_DIL_MED_UCODE_BETA = 3.3e-11f;
	GERMAN_PBX_LINEAR_EQU_DIL_HIGH_UCODE_BETA = 1.5e-11f;
	EIA6_LINEAR_EQU_DIL_MED_UCODE_BETA = 5e-12f;
	EIA6_LINEAR_EQU_DIL_HIGH_UCODE_BETA = 2e-12f;
	EIA6_LINEAR_EQU_DIL_ERROR_RELAX_BETA = 8e-11f;
	LINEAR_EQU_ALT_DIL_BETA = 1.25e-11f;
	LINEAR_EQU_ALT_DIL_MED_UCODE_BETA = 8.25e-12f;
	LINEAR_EQU_ALT_DIL_HIGH_UCODE_BETA = 3.75e-12f;
	unnamed_1b0 = 0x2ebaeabf;	/* the object stores 8.5e-11f here -- finding 878 */
	unnamed_1b4 = 0x2e83f0ff;	/* the object stores 6e-11f here -- finding 878 */
	unnamed_1b8 = 0x2d83f0ff;	/* the object stores 1.5e-11f here -- finding 878 */
	LINEAR_EQU_DIL_ERROR_RELAX_BETA = 7.5e-11f;
	LINEAR_EQU_TRN2D_INITIAL_BETA = 1e-10f;
	LINEAR_EQU_TRN2D_BETA = 7e-11f;
	LINEAR_EQU_DATA_BETA = 2e-11f;
	LINEAR_EQU_TRN1D_FREEZE_DURATION = 1000;
	LINEAR_EQU_TRN2D_INITIAL_DURATION = 3000;
	EIA6_LINEAR_EQU_TRN1D_BETA = 8e-11f;
	EIA6_LINEAR_EQU_DIL_BETA = 1.5e-11f;
	EIA6_LINEAR_EQU_TRN2D_BETA = 3.5e-11f;
	EIA6_LINEAR_EQU_DATA_BETA = 3e-11f;
	EIA6_LINEAR_EQU_TRN2D_INITIAL_BETA = 8.5e-11f;
	EIA6_LINEAR_EQU_FADE_EDGES_CYCLE = 30;
	EIA6_LINEAR_EQU_FADE_LEFT_EDGE_RATIO = 0.12f;
	EIA6_LINEAR_EQU_FADE_RIGHT_EDGE_RATIO = 0.12f;
	GERMAN_PBX_LINEAR_EQU_DATA_BETA = 7e-11f;
	GERMAN_ISDN_NT1_LINEAR_EQU_DATA_BETA = 6e-11f;
	DFE_LENGTH = 12;
	DFE_TRN1D_BETA = 0.000000007f;
	DFE_DIL_BETA = 0.00000007f;
	DFE_TRN2D_BETA = 0.00000001f;
	DFE_DATA_BETA = 0.000000005f;
	DFE_DIL_MED_UCODE_BETA = 0.00000006f;
	DFE_DIL_HIGH_UCODE_BETA = 0.00000005f;
	EIA6_DFE_DIL_MED_UCODE_BETA = 0.00000001f;
	EIA6_DFE_DIL_HIGH_UCODE_BETA = 4.52e-09f;
	EIA6_DFE_DIL_ERROR_RELAX_BETA = 0.00000002f;
	DFE_DIL_ALT_BETA = 1.5e-09f;
	DFE_DIL_ALT_MED_UCODE_BETA = 0.000000001f;
	DFE_DIL_ALT_HIGH_UCODE_BETA = 4.52e-10f;
	DFE_DIL_ERROR_RELAX_BETA = 0.00000007f;
	DFE_TRN1D_FREEZE_DURATION = 2000;
	GERMAN_PBX_DFE_TRN2D_FAST_BETA = 0.00000007f;
	GERMAN_PBX_DFE_TRN2D_SLOW_BETA = 0.00000004f;
	GERMAN_PBX_DFE_DATA_BETA = 0.000000015f;
	EIA6_DFE_DIL_BETA = 0.000000015f;
	EIA6_DFE_TRN1D_BETA = 0.000000015f;
	EIA6_DFE_DATA_BETA = 0.000000008f;
	EIA6_DFE_TRN2D_FAST_BETA = 0.00000003f;
	EIA6_DFE_TRN2D_SLOW_BETA = 0.000000015f;
	EIA6_DFE_TRN2D_RRN_BETA = 0.00000005f;
	ERROR_ENERGY_MEAN_BLOCK_LEN = 160;
	ERROR_ENERGY_MEAN_K = 0.7f;
	ERROR_ENERGY_PRINT_PERIOD_PHASE3 = 768;
	ERROR_ENERGY_PRINT_PERIOD_PHASE4 = 768;
	ERROR_ENERGY_PRINT_PERIOD_DATA = 1920;
	NOF_DD_SYMBOLS_BEFORE_MEAN_ERROR_DIAG_PHASE3 = 5000;
	NOF_DD_SYMBOLS_BEFORE_MEAN_ERROR_DIAG_PHASE4 = 2000;
	TIMING_OFFSET_PRINT_PERIOD_PHASE3 = 9600;
	TIMING_OFFSET_PRINT_PERIOD_PHASE4 = 9600;
	TIMING_OFFSET_PRINT_PERIOD_DATA = 19200;
	SD_DETECTOR_ENERGY_THRESHOLD = 250000.0f;
	SD_DETECTOR_POSITIVE_CORR_THRESHOLD = 0.8f;
	SD_DETECTOR_NEGATIVE_CORR_THRESHOLD = 0.4f;
	SD_DETECTOR_DETECTION_COUNTER_THRESHOLD = 150;
	PHASE4_R_DETECTION_LENGTH = 180;
	RRN_R_DETECTION_LENGTH = 240;
	ENERGY_DROP_DETECTOR_THRESHOLD = 20000.0f;
	NO_ENERGY_DURATION_FOR_REMOTE_RETRAIN = 384;
	SPECTRAL_VERIFIER_ENABLE = 1;
	EIA6_SPECTRAL_VERIFIER_ENABLE = 0;
	SPECTRAL_VERIFIER_SAMPLE_FREQ = 9600.0f;
	SPECTRAL_VERIFIER_FFT_LEN = 1024;
	SPECTRAL_VERIFIER_FFT_WINDOW = 1;
	SPECTRAL_VERIFIER_PSD_LEN = 4096;
	SPECTRAL_VERIFIER_PSD_OVERLAP_LEN = 512;
	SPECTRAL_VERIFIER_PRINT_SPECTRUM = 0;
	SPECTRAL_VERIFIER_ISDN_NULL_FREQ = 4220.0f;
	SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_FREQ = 4120.0f;
	SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_FREQ = 4350.0f;
	SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_DELTA = 11.0f;
	SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_DELTA = 1.0f;
	SPECTRAL_VERIFIER_GERMAN_PBX_NULL_FREQ = 4000.0f;
	SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_FREQ = 3950.0f;
	SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_FREQ = 4100.0f;
	SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_DELTA = 10.0f;
	SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_DELTA = 10.0f;
	SPECTRAL_VERIFIER_SEVERE_CODEC_REF_FREQ = 3600.0f;
	SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ1 = 4000.0f;
	SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ2 = 4200.0f;
	SPECTRAL_VERIFIER_SEVERE_CODEC_DELTA = 28.0f;
	TRN1D_DD_LENGTH = 12000;
	unnamed_300 = 21841;
	unnamed_304 = 1200;
	unnamed_308 = 24720;
	unnamed_30c = 3840;
	unnamed_310 = 600;
	unnamed_314 = 17041;
	unnamed_318 = 3120;
	unnamed_31c = 240;
	unnamed_320 = 720;
	unnamed_324 = 40000;
	unnamed_328 = 0x3f75c28f;	/* the object stores 0.96f here -- finding 878 */
	unnamed_32c = 1920;
	unnamed_330 = 6120;
	unnamed_334 = 3901;
	unnamed_338 = 5161;
	unnamed_33c = 4201;
	unnamed_340 = 4921;
	unnamed_344 = 12000;
	unnamed_348 = 600;
	unnamed_34c = 1200;
	unnamed_350 = 600;
	unnamed_354 = 1800;
	unnamed_358 = 1200;
	unnamed_35c = 3000;
	unnamed_360 = 2;
	/* +0x364 SILENCE_SCR -- conditional, see below */
	MINIMUM_RTD_FOR_NON_SILENCE_SCR = 1000;
	TRN2D_DD_LENGTH = 24000;
	RRN_TRN2D_DD_LENGTH = 12000;
	USE_RESTRICED_DMIN = 0;
	ENABLE_REDUNDANCY_OPTIMIZATION = 1;
	ENABLE_DIGITAL_POWER_REDUCTION = 1;
	DIGITAL_POWER_REDUCTION = -1.0f;
	UP_ROUND_K = 0.745f;
	EIA6_USE_RESTRICED_DMIN = 1;
	DMIN_CALC_FACTOR1 = 1.25f;
	DMIN_CALC_FACTOR2 = 12;
	DMIN_EIA6_FACTOR = 1.08f;
	FORCED_DMIN = -1;
	unnamed_39c = 0;
	FORCE_RATE_ENABLE = 0;
	RATE_FORCE = 45333;
	SPECTRAL_SHAPER_A1 = 1.0f;
	SPECTRAL_SHAPER_A2 = 0.0f;
	SPECTRAL_SHAPER_B1 = 0.0f;
	SPECTRAL_SHAPER_B2 = 0.0f;
	SPECTRAL_SHAPER_SR = 1;
	SPECTRAL_SHAPER_ID = 3;
	GERMAN_PBX_SPECTRAL_SHAPER_A1 = 1.0f;
	GERMAN_PBX_SPECTRAL_SHAPER_A2 = -1.0f;
	GERMAN_PBX_SPECTRAL_SHAPER_B1 = 0.0f;
	GERMAN_PBX_SPECTRAL_SHAPER_B2 = 0.0f;
	GERMAN_PBX_SPECTRAL_SHAPER_SR = 3;
	GERMAN_PBX_SPECTRAL_SHAPER_ID = 1;
	EIA6_SPECTRAL_SHAPER_A1 = 1.0f;
	EIA6_SPECTRAL_SHAPER_A2 = -1.0f;
	EIA6_SPECTRAL_SHAPER_B1 = 0.0f;
	EIA6_SPECTRAL_SHAPER_B2 = 0.0f;
	EIA6_SPECTRAL_SHAPER_SR = 2;
	EIA6_SPECTRAL_SHAPER_ID = 1;
	RRN_SILENCE_REQUESTED = 1;
	MASK_RRN_SILENCE_ON_PROBLEMATIC_ISP = 0;
	RRN_SILENCE_SCR_LENGTH = 2500;
	RRN_SILENCE_WAIT_BEFORE_ECHO_CALC = 132;
	RRN_SILENCE_ECHO_CALC_PERIOD = 240;
	RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE = 2.0f;
	MIN_RATE_FOR_SILENCE_RRN_KEEP_RATE = 50666;
	PDSNR_THRESHOLD_IN_PHASE3 = 120.0f;
	PDSNR_THRESHOLD_IN_PHASE4 = 100.0f;
	TRN1D_ERROR_FOR_V34_FALLBACK = 180.0f;
	TRN1D_MEAN_ERROR_STD_EVALUATION_ENABLE = 0;
	TRN1D_MAX_MEAN_ERROR_STD_IN_PHASE3 = 17.0f;
	TRN2D_MEAN_ERROR_STD_EVALUATION_ENABLE = 0;
	TRN2D_MAX_MEAN_ERROR_STD_IN_PHASE4 = 1.414f;
	TRN2D_MAX_MEAN_ERROR_ENERGY_IN_PHASE4 = 35.0f;
	PHASE3_ERROR_FOR_V34_FALLBACK = 200.0f;
	PHASE4_ERROR_FOR_V34_FALLBACK = 5000.0f;
	unnamed_434 = 0x437a0000;	/* the object stores 250.0f here -- finding 878 */
	PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH = 1.5f;
	QC_PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH = 1.75f;
	unnamed_440 = 0x41200000;	/* the object stores 10.0f here -- finding 878 */
	ENABLE_RRN_UP = 1;
	ENABLE_RRN_DOWN = 1;
	RATE_UP_DETECT_DURATION = 2400;
	RATE_DOWN_DETECT_DURATION = 2400;
	RETRAIN_DETECT_DURATION = 2400;
	NOF_REMOTE_RATE_RENEG_BEFORE_RETRAIN = 10;
	unnamed_45c = 2;
	MAX_NOF_V90_RETRAINS = 2;
	MAX_NOF_REMOTE_RETRAINS = 3;
	RETRAIN_COUNTER_FADE_COUNT = 0xea600;
	REMOTE_RRN_COUNTER_FADE_COUNT = 0xafc80;
	MINIMUM_DURATION_IN_DATA_BEFORE_RRN_UP = 0x249f00;
	MINIMUM_DURATION_IN_DATA_BEFORE_RRN_DOWN = 0x27100;
	MINIMUM_DURATION_IN_DATA_BEFORE_EC_RRN = 0x3a980;
	MAX_NOF_RATES_DIFF_BEFORE_RETRAIN = 4;
	ENABLE_ERROR_CORRECTION_RRN = 1;
	EIA6_PDSNR_THRESHOLD_IN_PHASE4 = 150.0f;
	EIA6_PDSNR_THRESHOLD_IN_PHASE3 = 160.0f;
	EIA6_TRN1D_ERROR_FOR_V34_FALLBACK = 180.0f;
	EIA6_MAX_NOF_V90_RETRAINS = 1;
	ENABLE_DROP_2_V34_ON_SEVERE_CODEC = 1;
	DEBUG_DIGITAL_MODEM_INITIATE_RRN = 0;
	DEBUG_DIGITAL_MODEM_INITIATE_RRN_TIME = 5000;
	TRN1_QC_DD_LENGTH = 3600;
	unnamed_4a4 = 2400;
	unnamed_4a8 = 300;
	unnamed_4ac = 600;
	unnamed_4b0 = 600;
	unnamed_4b4 = 600;
	unnamed_4b8 = 300;
	unnamed_4bc = 600;
	TRN2D_QC_DD_LENGTH = 8400;
	LINEAR_EQU_QC_TRN1D_FREEZE_DURATION = 1000;
	DFE_QC_TRN1D_FREEZE_DURATION = 1500;
	/* +0x4cc ANSPCM_DEMODULATION_LENGTH -- computed, see below */
	QC_LOGGING_PERIOD_INITIAL = 48000;
	QC_LOGGING_PERIOD_STEADY_STATE = 0x8ca00;
	ANSPCM_CORRELATION_THRESH_FOR_VALIDATION = 0.975f;
	DEBUG_CONNECTION_EVALUATOR_ALTERNATE_DEBUG = 0;
	DEBUG_CONNECTION_EVALUATOR_FALL_BACK = 0;
	DEBUG_CONNECTION_EVALUATOR_RETRAIN = 0;
	DEBUG_CONNECTION_EVALUATOR_RATE_UP = 0;
	DEBUG_CONNECTION_EVALUATOR_RATE_DOWN = 0;
	DEBUG_CONNECTION_EVALUATOR_PERIOD = 200;
	HIGH_LEVEL_TX_ACTIVE = 0;
	LOOP_TYPE = -1;
	SEPARATE_PHASE3_CONSTELLATIONS = 0;
	SEPARATE_PHASE4_CONSTELLATIONS = 1;
	SEPARATE_DATA_PHASE_CONSTELLATIONS = 1;
	WRITE_TIMING_PHASE_AND_OFFSET_TO_FILE = 0;
	WRITE_ERROR_TO_FILE = 0;
	WRITE_DEMOD_IN_SAMPLES_TO_FILE = 0;
	WRITE_EQU_COEFS_TO_FILE = 0;
	LOAD_EQU_COEFS_FROM_FILE = 0;
	DEBUG_PRINT_MAPPER_CONSTELLATIONS = 0;
	DEBUG_PRINT_DEMAPPER_CONSTELLATIONS = 0;
	DEBUG_DEMAPPER_ERROR_HISTOGRAM = 0;
	DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM = 8000;
	DEMAPPER_ERROR_HISTOGRAM_INTEGRATION_TIME = 0x3a980;
	TEMP_INT_PARAMETER1 = 0;
	TEMP_INT_PARAMETER2 = 0;
	TEMP_INT_PARAMETER3 = 0;
	TEMP_INT_PARAMETER4 = 0;
	TEMP_FLOAT_PARAMETER1 = 0.0f;
	TEMP_FLOAT_PARAMETER2 = 0.0f;
	TEMP_FLOAT_PARAMETER3 = 0.0f;
	TEMP_FLOAT_PARAMETER4 = 0.0f;

	/*
	 * The upstream rate window, and the mask it turns into.
	 *
	 * `SENSITIVE_ISP_DETECTED` and `MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP`
	 * are read here and written nowhere in this function -- the
	 * constructor sets them and `initSession` resets them.  The clamp is
	 * unsigned throughout, which is what the object's `ja`/`jae`/`jbe`
	 * say and not an assumption about the values.
	 */
	if (SENSITIVE_ISP_DETECTED != 0
	    && maxIndex > (unsigned int)MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP)
		maxIndex = (unsigned int)MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP;

	if (minIndex < V90_MIN_RATE_INDEX)
		minIndex = V90_MIN_RATE_INDEX;
	else if (minIndex > V90_MAX_RATE_INDEX)
		minIndex = V90_MAX_RATE_INDEX;

	if (maxIndex < V90_MIN_RATE_INDEX)
		maxIndex = V90_MIN_RATE_INDEX;
	else if (maxIndex > V90_MAX_RATE_INDEX)
		maxIndex = V90_MAX_RATE_INDEX;

	if (minIndex > maxIndex) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Parameters: bad upstream "
					     "rate, using default values\n");
		minIndex = V90_MIN_RATE_INDEX;
		maxIndex = V90_MAX_RATE_INDEX;
	}

	for (i = (int)V90_MAX_RATE_INDEX; i >= (int)V90_MIN_RATE_INDEX; i--)
		ANALOG_RATE_MASK = ANALOG_RATE_MASK * 2
		    | (((unsigned int)i <= maxIndex
			&& (unsigned int)i >= minIndex) ? 1 : 0);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Parameters: upStream min rate : %d "
				     "upStream max rate : %d  Rate mask :%x\n",
				     minIndex * V90_RATE_STEP,
				     maxIndex * V90_RATE_STEP,
				     ANALOG_RATE_MASK);

	/*
	 * Bit 0 of the modem block's first byte picks between one second and
	 * a little over three at 2400 baud.  The object computes it branch-
	 * lessly (`cmp $0x1,%dl; sbb %ecx,%ecx; and $0x14dc,%ecx; add
	 * $0x960,%ecx`), which is what GCC does with this conditional; the
	 * two constants are 0x960 and 0x960 + 0x14dc.
	 */
	ANSPCM_DEMODULATION_LENGTH =
	    (modemParams->sessionFlags & 1) ? 2400 : 7740;

	/*
	 * The one field this function leaves ALONE on the other arm.  There
	 * is no default store to `SILENCE_SCR` anywhere in `setToDefault`, so
	 * when an insensitive ISP has been detected the field keeps whatever
	 * was in the allocation.  A test that zeroed both objects could not
	 * tell that from "wrote 0".
	 */
	if (SENSITIVE_ISP_DETECTED == 0)
		SILENCE_SCR = 1;
}

/*
 * The four values the modem block contributes, and the transcript that is the
 * only place three of them can be seen.
 *
 * NONE OF THESE FOUR `edprintf` CALLS IS GATED.  Unlike `setToDefault`'s two
 * `dsplibs_debug_printf` sites, they run at every debug level -- `edprintf`
 * itself formats and encodes unconditionally and only its final handoff is
 * behind the level test (src/core/encode.c).  So the arithmetic behind
 * `%c%d.%02d` has no other observer: `DIGITAL_POWER_REDUCTION` is stored, and
 * the sign, the whole part and the two fraction digits are not.
 *
 * THE FLOAT IS PRINTED AS THREE INTEGERS because the channel has no `%f`.
 * The object's own test for the sign is `0 < v` with `v` RELOADED FROM THE
 * FIELD -- `fldz; fcomps 0x380(%esi)` -- so zero prints as '-', and the whole
 * part is taken from |v| while the fraction is taken from the signed v and
 * then made positive.  Truncation, not rounding: the object sets the x87
 * rounding mode to zero around each `fistl`, which is what a C cast to `int`
 * compiles to.
 */
void
V90Parameters::loadModemParamsData()
{
	unsigned int tempPR = modemParams->powerReductionTenths;
	int tempProbe;
	int tempConnectionType;

	edprintf("V90Parameters: Debug - tempPR = %d\r\n", tempPR);

	if (tempPR != 0) {
		/*
		 * DIVISION BY 5, NOT BY 10, and the two are one instruction
		 * apart.  `mul $0xcccccccd` puts the high half in %edx, which
		 * is already a shift of 32, and the object then does `shr
		 * $0x2` -- so the total is 34 and 2^34 / 0xcccccccd is 5.0
		 * exactly.  A shift of 3 would have been 10.  Reading it as
		 * 10 halves every power reduction the modem applies and the
		 * only thing that objected was the differential test.
		 */
		float pr = (float)(int)(tempPR / 5) * 0.5f;
		int whole = (int)pr;
		int frac = (int)((pr - (float)(int)pr) * 100.0f);

		DIGITAL_POWER_REDUCTION = pr;

		/*
		 * The object takes the whole part from |v| and the fraction
		 * from the signed v, then makes the fraction positive.
		 * `trunc` is odd, so `(int)|v|` and `|(int)v|` are the same
		 * number and this is the second spelling.
		 */
		if (whole < 0)
			whole = -whole;
		if (frac < 0)
			frac = -frac;

		edprintf("V90Parameters: setting power reduction to  = "
			 "%c%d.%02d\r\n",
			 (0 < DIGITAL_POWER_REDUCTION) ? '+' : '-',
			 whole, frac);
	}

	tempProbe = (modemParams->modeFlags >> 1) & 1;
	edprintf("V90Parameters: Debug - tempProbe = %d\r\n", tempProbe);
	if (tempProbe != 0)
		PROBING_MODE = tempProbe;

	tempConnectionType = modemParams->connectionType;
	edprintf("V90Parameters: Debug - tempConnectionType = %d\r\n",
		 tempConnectionType);
	if (LINE_CONNECTION_TYPE == -1)
		LINE_CONNECTION_TYPE = tempConnectionType;

	TRN2D_MEAN_ERROR_STD_EVALUATION_ENABLE = modemParams->modeFlags & 1;
	edprintf("V90Parameters: Debug - "
		 "trn2d_mean_error_std_evaluation_enable = %d\r\n",
		 TRN2D_MEAN_ERROR_STD_EVALUATION_ENABLE);
}

/*
 * Twenty-four bytes: the two fields `setToDefault` reads and never writes,
 * put back to what the constructor gave them.  14 is the top V.90 upstream
 * rate index, so the default is "no cap".
 */
void
V90Parameters::initSession()
{
	SENSITIVE_ISP_DETECTED = 0;
	MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP = 14;
}

void
V90Parameters::init()
{
	setToDefault();

	/*
	 * The object's branch is `if (modemParams->paramFile)
	 * loadParams(modemParams->paramFile);` and both of its arms leave the
	 * object identical, because every read `loadParams` makes goes to a
	 * three-byte stub.  Finding 879.  The test runs this with the pointer
	 * null and non-null and compares against the blob both ways, so the
	 * claim is measured here rather than asserted.
	 */

	loadModemParamsData();
}

/*
 * `initSession(); modemParams = mp; init();`
 *
 * The object interleaves the three stores at +0x4f8, +0x000 and +0x4fc and
 * inlines the whole of `init`, which is scheduling and an inlining decision
 * respectively -- neither is observable and neither is worth fitting.  What
 * IS forced is that +0x4f8 and +0x4fc are set BEFORE `setToDefault` runs,
 * since it reads both.
 */
V90Parameters::V90Parameters(_tagModemParameters *mp)
{
	initSession();
	modemParams = mp;
	init();
}

/* One byte: `ret`. */
V90Parameters::~V90Parameters()
{
}
