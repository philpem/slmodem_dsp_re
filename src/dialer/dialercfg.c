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

#include "dsplib/debug.h"
#include "dsplib/dialercfg.h"
#include "dsplib/modem_params.h"


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
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->tone_DigitLength %d\n",
				     cfg->dtmf_duration);
	cfg->pulse_make = modem_get_param(modem, GetPulseDialMakeTime);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->pulse_OffHookTime %d\n",
				     cfg->pulse_make);
	cfg->pulse_break = modem_get_param(modem, GetPulseDialBreakTime);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->pulse_OnHookTime %d\n",
				     cfg->pulse_break);
	cfg->pause = modem_get_param(modem, GetDialPauseTime);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->dialPauseTime %d\n",
				     cfg->pause);
	cfg->hook_flash = modem_get_param(modem, GetHookFlashTime);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->flashTime %d\n",
				     cfg->hook_flash);

	/*
	 * The one flag normalised to 0 or 1.  Every other flag here is stored
	 * as whatever the host returned, so a caller testing them for equality
	 * with 1 rather than for non-zero would behave differently on this one.
	 */
	cfg->pulse_dialing = modem_get_param(modem, GetPulseDialingFlag) != 0;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->toneOrPulseFlag %d\n",
				     cfg->pulse_dialing);

	cfg->modifier_validation = modem_get_param(modem,
						   GetDialModifierValidation);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->dialModifierValidationFlag %d\n",
				     cfg->modifier_validation);
	cfg->abcd_permitted = modem_get_param(modem,
					      GetABCDDialingPermittedFlag);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->ABCD_PermittedFlag %d\n",
				     cfg->abcd_permitted);
	cfg->mixed_permitted = modem_get_param(modem,
			GetPulseAndToneDialInSameDialStringPermittedFlag);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->pulseAndToneInSameStringPermittedFlag %d\n",
				     cfg->mixed_permitted);
	cfg->calling_tone = modem_get_param(modem, GetCallingToneFlag);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->callingToneFlag %d\n",
				     cfg->calling_tone);
	cfg->coma_pause_limit = modem_get_param(modem,
						GetComaPauseDurationLimit);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->commaPauseDurLimit %d\n",
				     cfg->coma_pause_limit);
	cfg->pulse_pattern = modem_get_param(modem, GetPulseDialDigitPattern);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->digitPattern %d\n",
				     cfg->pulse_pattern);

	/* GetDTMFDialSpeed a second time, for the inter-digit gap. */
	cfg->dtmf_gap = modem_get_param(modem, GetDTMFDialSpeed);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->tone_BetweenDigitsInterval %d\n",
				     cfg->dtmf_gap);

	/*
	 * The one field that is not a straight copy: ten times what the host
	 * says.  So this parameter is in units of 10 ms like the cadence
	 * times, and the dialler wants milliseconds -- consistent with
	 * docs/parameters.md, and the first direct confirmation of that
	 * convention outside cadence.
	 */
	cfg->pulse_gap = modem_get_param(modem, GetPulseBetweenDigitsInterval)
			 * 10;

	/* The MULTIPLIED value is what is reported, not the raw parameter. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Configuration->pulse_BetweenDigitsInterval %d\n",
				     cfg->pulse_gap);

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

	/*
	 * The two gains are reported LAST, after everything else, and read
	 * back from the struct rather than from the register that computed
	 * them -- so they are sign-extended shorts here where every other
	 * value above is an int.  The second is a tail call in the object.
	 *
	 * Gain1 is the field at +0x08 and Gain2 the one at +0x0a, which is
	 * dtmf_low then dtmf_high.  Worth stating because the names invite
	 * the opposite reading, and this was written the wrong way round
	 * first -- caught by re-reading the offsets, not by the test, which
	 * was written afterwards.  Swapping them deliberately does fail it,
	 * so the check has teeth; it just was not what found the error.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DTMF_Gain1 = %d\n", cfg->dtmf_low);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DTMF_Gain2 = %d\n", cfg->dtmf_high);
}
