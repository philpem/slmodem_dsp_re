/*
 * dialercfg.h -- Dialler Configuration: the country's dialling rules.
 *
 * `GetDialerConfig` fills this from `modem_get_param`, once per dial attempt.
 * Nothing here is computed; it is fourteen fields copied straight across,
 * except the two DTMF levels, which are looked up in decibel tables.
 *
 * The dialler reads it to answer questions like "may this string contain
 * letters", "how long is a comma", and "how hard should the tones be sent".
 * Every field is a per-country homologation setting -- see
 * docs/parameters.md -- so the same dial string is legal in one country and
 * rejected in another.
 */

#ifndef DSPLIB_DIALERCFG_H
#define DSPLIB_DIALERCFG_H

/*
 * DTMF amplitudes, Q14 against 16384 as 0 dBFS.
 *
 * `GetDTMFHighToneLevel` indexes the first table directly and is valid from 6
 * to 12, giving 0 dB down to -6 dB in 1 dB steps.  Out of range it is -2 dB.
 *
 * `GetDTMFHighAndLowToneLevelDifference` is the twist -- how far the low group
 * sits below the high one -- valid from 1 to 5 for -1 dB to -5 dB, and -2 dB
 * out of range.  Two decibels is what the ITU asks for, so the fallbacks are
 * the specified values rather than arbitrary ones.
 */
#define DIALER_DTMF_LEVEL_MIN	6
#define DIALER_DTMF_LEVEL_MAX	12
#define DIALER_DTMF_TWIST_MIN	1
#define DIALER_DTMF_TWIST_MAX	5

/* -2 dB in Q14, and -2 dB as a Q15 ratio. */
#define DIALER_DTMF_LEVEL_DEFAULT	13014
#define DIALER_DTMF_TWIST_DEFAULT	26029

struct dialer_cfg {
	/*
	 * How long a DTMF digit lasts and how long the gap after it is.  Both
	 * come from GetDTMFDialSpeed; the original fetches it twice rather
	 * than once, which is visible to a harness that counts parameter
	 * calls and so is reproduced.
	 */
	int	dtmf_duration;		/* +0x00  GetDTMFDialSpeed          */
	int	dtmf_gap;		/* +0x04  the same, fetched again   */

	/* Q14 amplitudes; see above. */
	short	dtmf_low;		/* +0x08 */
	short	dtmf_high;		/* +0x0a */

	int	pulse_break;		/* +0x0c  GetPulseDialBreakTime     */
	int	pulse_make;		/* +0x10  GetPulseDialMakeTime      */
	int	pulse_gap;		/* +0x14  GetPulseBetweenDigitsInterval */
	int	pause;			/* +0x18  GetDialPauseTime          */
	int	hook_flash;		/* +0x1c  GetHookFlashTime          */

	/* Normalised to 0 or 1 by the original, unlike every other flag. */
	int	pulse_dialing;		/* +0x20  GetPulseDialingFlag       */

	int	modifier_validation;	/* +0x24  GetDialModifierValidation */
	int	abcd_permitted;		/* +0x28  GetABCDDialingPermittedFlag */
	int	mixed_permitted;	/* +0x2c  GetPulseAndToneDialInSameDialStringPermittedFlag */
	int	calling_tone;		/* +0x30  GetCallingToneFlag        */
	int	coma_pause_limit;	/* +0x34  GetComaPauseDurationLimit */
	int	pulse_pattern;		/* +0x38  GetPulseDialDigitPattern  */
};

/* Fill `cfg` from the host.  Reads fifteen parameters and returns nothing. */
void GetDialerConfig(struct dialer_cfg *cfg, void *modem);

#endif /* DSPLIB_DIALERCFG_H */
