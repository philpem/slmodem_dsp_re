/*
 * V92Parameters.h -- the V.92 half of the parameter block.
 *
 * Reconstructed from dsplibs.o, by exactly the method described at the top of
 * V90Parameters.h and with the same standing: SHARED TYPE, FROZEN LAYOUT.
 * `V92Modulator`, `V92EchoCanceller`, `V92Phase4Modulator`, `V92Phase3
 * Modulator`, `V92BitsToSymbol`, `V92Jd` and `V92Phase2Info` all take a
 * `V92Parameters *`.
 *
 * IT IS A SEPARATE CLASS AND NOT A BASE OR A MEMBER OF `V90Parameters`.  Both
 * are constructed independently -- `V90Modem`'s constructor allocates 0x558
 * and calls `V90Parameters::V90Parameters`, `V92Modem`'s allocates 0xdc and
 * calls `V92Parameters::V92Parameters` -- and neither destructor calls the
 * other's.
 *
 * The evidence is tighter here than for the V.90 block, because the two
 * readings cover the SAME set of offsets rather than one containing the other:
 *
 *   - `loadParams`   54 calls, 54 distinct offsets, +0x004..+0x0d8.
 *   - `setToDefault` 54 stores, 54 distinct offsets, +0x004..+0x0d8.
 *
 * Identical sets, no aliases, no hole, every slot four bytes, and no offset
 * whose declared reader disagrees with the shape of its default.  `V92Modem`
 * then does `sysdep_malloc(0xdc)` at .text+0x13d90 and +0x13f20, and
 * 0xd8 + 4 == 0xdc.
 *
 * `this` arrives at `0x20(%esp)` in `loadParams` and `0x4(%esp)` in
 * `setToDefault`, which is worth writing down only because reading the second
 * as the first shifts every offset by four and produces a map that looks
 * entirely reasonable -- +0x000..+0x0d4 -- and is wrong in every line.
 */

#ifndef DSPLIB_V92PARAMETERS_H
#define DSPLIB_V92PARAMETERS_H

struct _tagModemParameters;

class V92Parameters {
public:
	/*
	 * Declared from the mangling, defined nowhere yet:
	 *
	 *     loadParams(char *)          1384 B
	 *     setToDefault()               477 B
	 *     V92Parameters(_tagModemParameters *)   53 B
	 *     init()                        49 B
	 *     ~V92Parameters()               1 B   (a bare `ret`)
	 */
	_tagModemParameters *modemParams;	/* +0x000 */
	int  	VPCM_SESSION_TYPE;	/* +0x004 */
	int  	V92_PHASE2_INFO_A_OR_MU;	/* +0x008 */
	int  	V92_PHASE2_INFO_RTD;	/* +0x00c */
	int  	V92_PHASE2_INFO_UINFO;	/* +0x010 */
	int  	V92_PHASE2_INFO_MAX_TX_POWER;	/* +0x014 */
	int  	V92_PHASE2_INFO_TX_POWER_MEASURE_POINT;	/* +0x018 */
	int  	V92_EXTEND_EU;	/* +0x01c */
	int  	V92_DELAY_BEFOR_STEADY_STATE;	/* +0x020 */
	int  	V92_RRN_START_DELAY;	/* +0x024 */
	int  	V92_RRN_SIMULATION_SWITCH;	/* +0x028 */
	int  	V92_SILENCE_RRN_REQUESTE;	/* +0x02c */
	int  	V92_RRN_TRN2U_DD_LENGTH;	/* +0x030 */
	int  	V92_MAX_SILENCE_LENGTH_FLAG;	/* +0x034 */
	int  	V92_SILENCE_LENGTH;	/* +0x038 */
	int  	V92_FPE_SIMULATION_SWITCH;	/* +0x03c */
	int  	V92_FPE_START_DELAY;	/* +0x040 */
	int  	V92A_DIGITAL_RATE_MASK;	/* +0x044 */
	int  	V92A_MAX_SPECTRAL_SHAPER_LOOKAHEAD;	/* +0x048 */
	int  	V92A_PHASE4_CONSTELLATION;	/* +0x04c */
	int  	V92A_RRN_CONSTELLATION;	/* +0x050 */
	int  	V92_PHASE4_CONSTELLATION;	/* +0x054 */
	int  	V92_RRN_CONSTELLATION;	/* +0x058 */
	int  	V92_NOF_FILTER_SECTIONS;	/* +0x05c */
	int  	V92_MAX_TOTAL_NOF_COEFFS;	/* +0x060 */
	int  	V92_MAX_NOF_COEFFS_IN_EACH_SECTION;	/* +0x064 */
	int  	V92_APPLY_TX_SHAPING_FILTER;	/* +0x068 */
	int  	V92_ECHO_FILTER_LENGTH;	/* +0x06c */
	int  	V92_ECHO_INITIAL_DELAY;	/* +0x070 */
	int  	V92_ECHO_DELAY_OFFSET;	/* +0x074 */
	float	V92_ECHO_FAST_BETA_FACTOR;	/* +0x078 */
	float	V92_ECHO_FAST_DECAY_FACTOR;	/* +0x07c */
	float	V92_ECHO_SLOW_BETA_FACTOR;	/* +0x080 */
	float	V92_ECHO_SLOW_DECAY_FACTOR;	/* +0x084 */
	int  	V92_ECHO_FAST_UPDATE_DURATION;	/* +0x088 */
	int  	V92_ECHO_SLOW_UPDATE_DURATION;	/* +0x08c */
	int  	V92_RESAMPLER_RESULOTION;	/* +0x090 */
	int  	V92_LINEAR_EQU_LENGTH;	/* +0x094 */
	float	V92_LE_PHASE_3_BETA;	/* +0x098 */
	int  	V92_LE_BETA_I_DURATION;	/* +0x09c */
	float	V92_LE_PHASE_3_BETA_II;	/* +0x0a0 */
	int  	V92_DFE_LENGTH;	/* +0x0a4 */
	float	V92_DFE_PHASE_3_BETA;	/* +0x0a8 */
	int  	V92_DFE_TRN1U_FREEZE_DURATION;	/* +0x0ac */
	int  	ERROR_ENERGY_PRINT_PERIOD_PHASE3;	/* +0x0b0 */
	int  	ERROR_ENERGY_PRINT_PERIOD_PHASE4;	/* +0x0b4 */
	float	V92_AGC_NOMINAL_ENERGY;	/* +0x0b8 */
	float	V92_AGC_K;	/* +0x0bc */
	int  	V92_AGC_BLOCK_LEN;	/* +0x0c0 */
	int  	V92_AGC_ADAPTATION_DURATION;	/* +0x0c4 */
	float	SU_DETECTOR_ENERGY_THRESHOLD;	/* +0x0c8 */
	float	SU_DETECTOR_POSITIVE_CORR_THRESHOLD;	/* +0x0cc */
	float	SU_DETECTOR_NEGATIVE_CORR_THRESHOLD;	/* +0x0d0 */
	int  	SU_DETECTOR_DETECTION_COUNTER_THRESHOLD;	/* +0x0d4 */
	int  	MODULATOR_QUEUE_LENGTH;	/* +0x0d8 */
};

#endif /* DSPLIB_V92PARAMETERS_H */
