/*
 * dialercfg.c -- Dialler Configuration: the country's dialling rules.
 *
 * Reconstructed from dsplibs.o DialerConfig.c:
 *
 *   GetDialerConfig   .text 0x07be80
 *
 * Sixteen `modem_get_param` calls in a fixed order, two of them for the same
 * parameter.  The order matters to nothing except a harness that counts the
 * calls -- which this tree's does, precisely so that a reconstruction cannot
 * quietly read a different parameter and land on the same value.
 */

#include "dsplib/dialercfg.h"
#include "dsplib/modem_params.h"

extern int modem_get_param(void *modem, int name);

/*
 * DTMF send levels, .rodata+0x6178, indexed directly by
 * `GetDTMFHighToneLevel` over 6..12 -- so this table is offset by six and its
 * first entry is level 6.  One decibel per step from 0 dBFS.
 */
static const short dtmf_level[DIALER_DTMF_LEVEL_MAX
			      - DIALER_DTMF_LEVEL_MIN + 1] = {
	16384,		/*  0 dB */
	14602,		/* -1 dB */
	13014,		/* -2 dB */
	11599,		/* -3 dB */
	10338,		/* -4 dB */
	 9213,		/* -5 dB */
	 8211		/* -6 dB */
};

/*
 * Twist: how far below the high group the low group is sent, as a Q15 ratio.
 * .rodata+0x6186, indexed by `GetDTMFHighAndLowToneLevelDifference` over
 * 1..5, so this table is offset by one.
 */
static const short dtmf_twist[DIALER_DTMF_TWIST_MAX
			      - DIALER_DTMF_TWIST_MIN + 1] = {
	29205,		/* -1 dB */
	26029,		/* -2 dB */
	23198,		/* -3 dB */
	20675,		/* -4 dB */
	18427		/* -5 dB */
};

void
GetDialerConfig(struct dialer_cfg *cfg, void *modem)
{
	int level, twist;

	cfg->dtmf_duration = modem_get_param(modem, GetDTMFDialSpeed);
	cfg->pulse_make = modem_get_param(modem, GetPulseDialMakeTime);
	cfg->pulse_break = modem_get_param(modem, GetPulseDialBreakTime);
	cfg->pause = modem_get_param(modem, GetDialPauseTime);
	cfg->hook_flash = modem_get_param(modem, GetHookFlashTime);

	/*
	 * The one flag normalised to 0 or 1.  Every other flag here is stored
	 * as whatever the host returned, so a caller testing them for equality
	 * with 1 rather than for non-zero would behave differently on this one.
	 */
	cfg->pulse_dialing = modem_get_param(modem, GetPulseDialingFlag) != 0;

	cfg->modifier_validation = modem_get_param(modem,
						   GetDialModifierValidation);
	cfg->abcd_permitted = modem_get_param(modem,
					      GetABCDDialingPermittedFlag);
	cfg->mixed_permitted = modem_get_param(modem,
			GetPulseAndToneDialInSameDialStringPermittedFlag);
	cfg->calling_tone = modem_get_param(modem, GetCallingToneFlag);
	cfg->coma_pause_limit = modem_get_param(modem,
						GetComaPauseDurationLimit);
	cfg->pulse_pattern = modem_get_param(modem, GetPulseDialDigitPattern);

	/* GetDTMFDialSpeed a second time, for the inter-digit gap. */
	cfg->dtmf_gap = modem_get_param(modem, GetDTMFDialSpeed);

	/*
	 * The one field that is not a straight copy: ten times what the host
	 * says.  So this parameter is in units of 10 ms like the cadence
	 * times, and the dialler wants milliseconds -- consistent with
	 * docs/parameters.md, and the first direct confirmation of that
	 * convention outside cadence.
	 */
	cfg->pulse_gap = modem_get_param(modem, GetPulseBetweenDigitsInterval)
			 * 10;

	/*
	 * The levels.  Both tables are indexed by the raw parameter with the
	 * range checked as unsigned after subtracting the low bound, so a
	 * negative value fails the check rather than reading behind the table.
	 */
	level = modem_get_param(modem, GetDTMFHighToneLevel);
	twist = modem_get_param(modem, GetDTMFHighAndLowToneLevelDifference);

	if ((unsigned)(level - DIALER_DTMF_LEVEL_MIN)
	    <= DIALER_DTMF_LEVEL_MAX - DIALER_DTMF_LEVEL_MIN)
		cfg->dtmf_high = dtmf_level[level - DIALER_DTMF_LEVEL_MIN];
	else
		cfg->dtmf_high = DIALER_DTMF_LEVEL_DEFAULT;

	if ((unsigned)(twist - DIALER_DTMF_TWIST_MIN)
	    <= DIALER_DTMF_TWIST_MAX - DIALER_DTMF_TWIST_MIN)
		cfg->dtmf_low = (short)(((int)cfg->dtmf_high
					 * dtmf_twist[twist
						      - DIALER_DTMF_TWIST_MIN])
					>> 15);
	else
		cfg->dtmf_low = (short)(((int)cfg->dtmf_high
					 * DIALER_DTMF_TWIST_DEFAULT) >> 15);
}
