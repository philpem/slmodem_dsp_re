/**
 * @file V92Parameters.h
 * @brief The V.92 half of the parameter block.
 *
 * Reconstructed by the same method as `V90Parameters.h`'s top comment
 * describes, and with the same standing: a shared type with a frozen
 * layout. `V92Modulator`, `V92EchoCanceller`, `V92Phase4Modulator`,
 * `V92Phase3Modulator`, `V92BitsToSymbol`, `V92Jd` and `V92Phase2Info` all
 * take a `V92Parameters *`.
 *
 * It is a separate class, not a base or a member of `V90Parameters`: the
 * two are constructed independently (`V90Modem`'s constructor allocates
 * 0x558 and calls `V90Parameters::V90Parameters`; `V92Modem`'s allocates
 * 0xdc and calls `V92Parameters::V92Parameters`), and neither destructor
 * calls the other's.
 *
 * The evidence here is tighter than for the V.90 block, because the two
 * readings cover the same set of offsets rather than one containing the
 * other: `loadParams` makes 54 calls at 54 distinct offsets, +0x004..+0x0d8,
 * and `setToDefault` makes 54 stores at the identical 54 offsets -- no
 * aliases, no hole, every slot four bytes, and no offset whose declared
 * reader disagrees with the shape of its default. `V92Modem` then allocates
 * exactly `sysdep_malloc(0xdc)`, and 0xd8 + 4 == 0xdc.
 *
 * One easy mistake to repeat: `this` arrives at `0x20(%esp)` in
 * `loadParams` but at `0x4(%esp)` in `setToDefault`. Reading the second
 * function's offsets as if `this` were at the first's stack slot shifts
 * every field by four bytes and produces a map (+0x000..+0x0d4) that looks
 * entirely reasonable and is wrong in every line.
 */

#ifndef DSPLIB_V92PARAMETERS_H
#define DSPLIB_V92PARAMETERS_H

struct _tagModemParameters;

class V92Parameters {
public:
	/**
	 * @brief Construct, storing the owning modem parameters pointer.
	 *        Does not itself populate the V.92 fields below -- see
	 *        setToDefault()/loadParams()/init().
	 * @param mp  The owning `_tagModemParameters` block.
	 */
	V92Parameters(_tagModemParameters *mp);
	/** @brief Destroy. Bare `ret` in the object -- nothing to release. */
	~V92Parameters();

	/** @brief Fill every V.92 field with its compiled-in default. 54
	 *  stores at 54 distinct offsets, +0x004..+0x0d8 -- see the file
	 *  comment. */
	void	setToDefault();
	/**
	 * @brief Load V.92 parameter overrides from a text config file, one
	 *        `NAME = value` line per field. 54 calls at the same 54
	 *        offsets `setToDefault` fills (finding F6400, which
	 *        supersedes the earlier decision in finding F879 to leave
	 *        this member out).
	 * @param paramFile  Path to the parameter file.
	 */
	void	loadParams(char *paramFile);
	/** @brief Post-load fixup; 49 bytes in the object. */
	void	init();
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
