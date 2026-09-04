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

/*
 * The two values of `tone_or_pulse`, in the author's own names -- the parser
 * prints "TONE_OR_PULSE_FLAG became TONE_DIALING" as it stores 1.
 */
#define DIALER_PULSE_DIALING	0
#define DIALER_TONE_DIALING	1

/*
 * The author's own field names are given alongside ours, recovered from
 * GetDialerConfig's dropped debug output (findings F134, F143, F146).  Ours
 * were inferred from the modem_get_param name that fills each field.
 *
 * That inference was right for fourteen of the sixteen and WRONG for the
 * make/break pair, which is why every one of these is now tied to a
 * parameter ID read out of the disassembly rather than to a plausible
 * reading of a getter's name.  See finding F146.
 */
struct dialer_cfg {
	/*
	 * How long a DTMF digit lasts and how long the gap after it is.  Both
	 * come from GetDTMFDialSpeed; the original fetches it twice rather
	 * than once, which is visible to a harness that counts parameter
	 * calls and so is reproduced.
	 */
	int	dtmf_duration;		/* +0x00  GetDTMFDialSpeed          */
	/*   author: tone_DigitLength */
	int	dtmf_gap;		/* +0x04  the same, fetched again   */
	/*   author: tone_BetweenDigitsInterval */

	/* Q14 amplitudes; see above. */
	short	dtmf_low;		/* +0x08  author: DTMF_Gain1 */
	short	dtmf_high;		/* +0x0a  author: DTMF_Gain2 */

	int	pulse_break;		/* +0x0c  author: pulse_OnHookTime  */
	int	pulse_make;		/* +0x10  author: pulse_OffHookTime */
	int	pulse_gap;		/* +0x14  author: pulse_BetweenDigitsInterval */
	int	pause;			/* +0x18  author: dialPauseTime     */
	int	hook_flash;		/* +0x1c  author: flashTime         */

	/*
	 * Normalised to 0 or 1 by the original, unlike every other flag.
	 *
	 * The author's VALUES for it are recovered too: the parser announces
	 * "TONE_OR_PULSE_FLAG became TONE_DIALING" when it stores 1 and
	 * "... became PULSE_DIALING" when it stores 0.  An earlier draft
	 * called this field `pulse_dialing`, under which 1 meant tone --
	 * see finding F161.
	 */
	int	tone_or_pulse;		/* +0x20  author: toneOrPulseFlag   */

	int	modifier_validation;	/* +0x24  author: dialModifierValidationFlag */
	int	abcd_permitted;		/* +0x28  author: ABCD_PermittedFlag */
	int	mixed_permitted;	/* +0x2c  author: pulseAndToneInSameStringPermittedFlag */
	int	calling_tone;		/* +0x30  author: callingToneFlag   */
	int	coma_pause_limit;	/* +0x34  author: commaPauseDurLimit -- TWO m's;
				 * the parameter name has the typo,
				 * the struct field does not */
	int	pulse_pattern;		/* +0x38  author: digitPattern      */
};

/**
 * @brief Fill a dialler configuration from the host's per-country parameters.
 *
 * Fourteen fields are copied straight from `modem_get_param`; the two DTMF
 * levels are looked up in decibel tables instead. Fifteen distinct
 * parameters are read (`GetDTMFDialSpeed` is fetched twice, once for
 * `dtmf_duration` and once for `dtmf_gap`, matching the object).
 *
 * @param cfg    Filled in; no field is left unset.
 * @param modem  The host's modem object, queried once per dial attempt.
 */
void GetDialerConfig(struct dialer_cfg *cfg, void *modem);

#endif /* DSPLIB_DIALERCFG_H */
