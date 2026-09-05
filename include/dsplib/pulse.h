/*
 * pulse.h -- Pulse dialling: dialling by interrupting the line.
 *
 * Before tones there was loop disconnect: to dial a 7, break the line seven
 * times.  These five functions are what does it, and they are the only part
 * of the library that operates a relay rather than a filter.
 *
 * They live in call.c rather than Dialer.c, and the dialler calls across to
 * them.  All state is in the call object, so a caller needs nothing but the
 * modem handle -- the call object is reached through
 * `modem_get_param(MDMPRM_DP_ADDR)`, which returns it, and which the object
 * also stores a pointer to itself at +0x10 for reasons that only become clear
 * when call.c is reconstructed.
 *
 * HOW A DIGIT IS SENT
 *
 * `PulseDialDigit` loads the pulse count, then `IsPulseDialerReady` is called
 * repeatedly -- once per 5 ms tick -- and each call advances the cycle:
 *
 *     elapsed <  break         hook on, line interrupted
 *     elapsed >= break         hook off, line restored
 *     elapsed >= break + make  one pulse done; count down, start again
 *
 * and it returns true once the count reaches zero.  The tick is a literal 5
 * in the code, so every duration here is in milliseconds.
 */

#ifndef DSPLIB_PULSE_H
#define DSPLIB_PULSE_H

/* Milliseconds added to the phase timer per call. */
#define PULSE_TICK_MS	5

/*
 * The call object, as far as pulse dialling is concerned.  The rest is filled
 * in when call.c is reconstructed; these offsets are what the pulse code
 * touches.
 */
struct call {
	void		*dp_runtime;		/* +0x000 */
	void		*modem;			/* +0x004 */
	unsigned char	pad008[8];		/* +0x008 */
	struct call	*self;			/* +0x010 */
	unsigned char	pad014[0x5a0];		/* +0x014 */

	int	pulse_make;	/* line restored, ms       +0x5b4 */
	int	pulse_break;	/* line interrupted, ms    +0x5b8 */
	int	pad5bc;					/* +0x5bc */

	/*
	 * Pulses still to send for this digit, and how far into the current
	 * pulse we are.  A digit of zero is loaded as ten, which is how loop
	 * disconnect has always spelled it.
	 */
	int	pulse_remaining;			/* +0x5c0 */
	int	pulse_elapsed;				/* +0x5c4 */

	/* Whether the line is currently interrupted. */
	int	pulse_off_hook;				/* +0x5c8 */
};

/**
 * @brief Set the "make" (line-restored) duration of a pulse.
 *
 * Takes the value from the country table and writes it into the call
 * object; does nothing if the modem has no datapump attached.
 *
 * @param modem  Modem handle.
 * @param ms     Duration in milliseconds.
 */
void SetPulseMakeTime(void *modem, int ms);

/**
 * @brief Set the "break" (line-interrupted) duration of a pulse.
 *
 * Takes the value from the country table and writes it into the call
 * object; does nothing if the modem has no datapump attached.
 *
 * @param modem  Modem handle.
 * @param ms     Duration in milliseconds.
 */
void SetPulseBreakTime(void *modem, int ms);

/**
 * @brief Tell the host the pulse dialler has finished with the line.
 *
 * Does not look at the call object at all -- just clears the parameter
 * PulseDialDigit() set. So the host, not the library, owns the "a digit is
 * being pulsed" state; the library only owns the timing.
 *
 * @param modem  Modem handle.
 */
void LastPulseDigitDialed(void *modem);

/**
 * @brief Begin sending @p digit pulses.
 * @param modem  Modem handle.
 * @param digit  Digit to dial, 0-9. A value of 0 dials ten pulses -- how
 *               loop disconnect has always spelled it.
 */
void PulseDialDigit(void *modem, int digit);

/**
 * @brief Advance the pulse dialler by one 5 ms tick.
 *
 * @param modem  Modem handle.
 * @return Non-zero when the current digit is complete (including when
 *         there is nothing to send at all, or the modem has no datapump --
 *         so a caller polling this on a torn-down modem terminates rather
 *         than hangs).
 */
int IsPulseDialerReady(void *modem);

#endif /* DSPLIB_PULSE_H */
