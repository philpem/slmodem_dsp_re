/*
 * dialer.c -- Dialler: turning a dial string into tones and pulses.
 *
 * Reconstructed from dsplibs.o Dialer.c:
 *
 *   AnalyseDialString     .text 0x07a9f0
 *   IsDialStringInvalid   .text 0x07af30
 *
 * WHAT A DIAL STRING LOOKS LIKE HERE
 *
 * Not what an AT command contains.  By the time it reaches this function the
 * mode letter is mandatory and leading: `T` or `P` decides tone or pulse, and
 * a string starting with anything else is not examined at all.  So "5551234"
 * grades INVALID and "T5551234" grades VALID, which is only sensible if
 * whatever parsed the AT line put the letter there.
 *
 * After the mode letter the characters fall into eight classes, dispatched
 * through an 88-entry jump table over `c - ' '`.  Three of them consult the
 * country's rules, and all three test with the opposite polarity to the name
 * of the flag they read -- see below.
 */

#include "dsplib/dialer.h"
#include "dsplib/sysdep.h"

/*
 * Grades, worst to best, and the rule for lowering one.  The scan starts at
 * VALID and only ever comes down.
 */
static int
lower(int grade, int to)
{
	return grade > to ? to : grade;
}

int
AnalyseDialString(struct dialer *d, const char *s, int store)
{
	int grade = DIALER_VALID;
	int last_digit = -2;
	int tone;			/* 1 after T, 0 after P */
	int i;
	unsigned char c;

	d->last_digit = -2;

	/*
	 * A null string grades VALID, so `IsDialStringInvalid(d, NULL)` says
	 * the string is fine.  Reproduced; see D16.
	 */
	if (s == 0)
		return DIALER_VALID;

	/*
	 * Longer than the buffer it would be copied into, so nothing can be
	 * done with it at all -- the one condition graded FATAL rather than
	 * merely INVALID.
	 */
	if (sysdep_strlen(s) > DIALER_MAX_STRING)
		return DIALER_FATAL;

	if (s[0] == 't' || s[0] == 'T')
		tone = 1;
	else if (s[0] == 'p' || s[0] == 'P')
		tone = 0;
	else
		/*
		 * No mode letter, so nothing is examined.  An empty string
		 * grades VALID and anything else INVALID.
		 */
		return s[0] == '\0' ? DIALER_VALID : DIALER_INVALID;

	for (i = 1; (c = (unsigned char)s[i]) != '\0'; i++) {
		int illegal = 0;

		/*
		 * The table covers ' ' to 'w'.  The subtraction is done on a
		 * SIGNED char, so anything with the top bit set goes negative
		 * and fails the unsigned range test -- which is how the
		 * original keeps eight-bit input out of the table.
		 */
		if ((unsigned)((signed char)c - ' ') > 0x57) {
			illegal = 1;
		} else switch (c) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case '*': case '#': case '!':
			/* Something will actually be sent for this one. */
			last_digit = i;
			break;

		case ' ': case '$': case '(': case ')':
		case ',': case '-': case '@': case 'W': case 'w':
			/* Pauses and waits: legal, and send nothing. */
			break;

		case 'A': case 'B': case 'C': case 'D':
		case 'a': case 'b': case 'c': case 'd':
			/*
			 * The extended DTMF digits.  Note the sense: the flag
			 * REJECTS when set, which is the opposite of its name.
			 * The shipped data agrees with the behaviour rather
			 * than the name -- 49 of slmodemd's 50 countries leave
			 * it at zero, so ABCD dialling works almost
			 * everywhere.  See finding 52.
			 */
			if (d->cfg.abcd_permitted != 0)
				illegal = 1;
			else
				last_digit = i;
			break;

		case 'T': case 't':
			/* Already in tone: nothing to do. */
			if (tone != 0)
				break;
			/* Otherwise this is a mode switch mid-string. */
			if (d->cfg.mixed_permitted != 0)
				illegal = 1;
			break;

		case 'P': case 'p':
			if (tone != 1)
				break;
			if (d->cfg.mixed_permitted != 0)
				illegal = 1;
			break;

		case '^':
			/*
			 * Suppresses the calling tone, and is refused when the
			 * country's calling-tone setting is 2 -- the one value
			 * for which CALLPROG_Dial would have emitted one.
			 */
			if (d->cfg.calling_tone == 2)
				illegal = 1;
			break;

		case ';':
			/* Return to command mode: legal only as the last one. */
			if (s[i + 1] != '\0')
				illegal = 1;
			break;

		default:
			illegal = 1;
			break;
		}

		if (illegal) {
			/*
			 * And here the polarity is genuinely surprising:
			 * validation SET makes an unknown character merely
			 * TOLERABLE, validation CLEAR makes it INVALID.  Twenty
			 * of the fifty countries set it.
			 */
			if (d->cfg.modifier_validation == 0)
				grade = lower(grade, DIALER_INVALID);
			else
				grade = lower(grade, DIALER_TOLERABLE);
		}
	}

	if (store)
		d->last_digit = last_digit;

	return grade;
}

int
IsDialStringInvalid(struct dialer *d, const char *s)
{
	GetDialerConfig(&d->cfg, d->modem);

	return AnalyseDialString(d, s, 0) <= DIALER_INVALID;
}

/*
 * ---------------------------------------------------------------------------
 * DialerCreate  .text 0x07afd0
 * DialerAbort   .text 0x07bde0
 */

extern void SetPulseMakeTime(void *modem, int ms);
extern void SetPulseBreakTime(void *modem, int ms);
extern void LastPulseDigitDialed(void *modem);

int
DialerCreate(struct dialer *d, const char *s, void *modem)
{
	d->modem = modem;
	GetDialerConfig(&d->cfg, modem);

	d->progress_state = 0;
	d->f_b4 = 0;
	d->pos = -1;
	d->f_b0 = 0;
	d->f_bc = 0;
	d->f_c8 = 0;

	/*
	 * The country's pulse timings go straight through to the call object,
	 * so the pulse dialler is configured before the string is even looked
	 * at -- and stays configured if the string is then rejected.
	 */
	SetPulseMakeTime(modem, d->cfg.pulse_make);
	SetPulseBreakTime(modem, d->cfg.pulse_break);

	d->pulse_active = 0;
	d->pulse_released = 0;

	/*
	 * A null string leaves an empty one and succeeds, which is the same
	 * judgement AnalyseDialString makes about NULL (D15) arrived at by a
	 * different route.
	 */
	if (s == 0) {
		d->string[0] = '\0';
		return 0;
	}

	/*
	 * The only call anywhere that asks AnalyseDialString to store the
	 * position of the last dialable character.
	 */
	d->grade = AnalyseDialString(d, s, 1);
	if (d->grade <= DIALER_INVALID)
		return DIALER_CREATE_REJECTED;

	/*
	 * Unbounded, into a 100-byte buffer.  What keeps it in bounds is the
	 * length check inside AnalyseDialString, which grades anything longer
	 * FATAL and so returns above.  The two are a hundred lines and one
	 * function call apart.
	 */
	sysdep_strcpy(d->string, s);

	return 0;
}

void
DialerAbort(struct dialer *d)
{
	if (d->progress_state > 10)
		return;
	if (d->pulse_released != 0)
		return;
	if (d->pulse_active == 0)
		return;

	LastPulseDigitDialed(d->modem);
	d->pulse_released = 1;
}
