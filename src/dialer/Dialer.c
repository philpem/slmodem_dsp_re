/*
 * Dialer.c -- Dialler: turning a dial string into tones and pulses.
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

#include "dsplib/debug.h"
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

/*
 * The grade names, .rodata+0x613c, spelled as the object's debug output
 * spells them.  The lookup is unsigned, so a negative grade would read
 * "ILLEGAL!" too -- nothing produces one, or a grade past VALID.
 */
static const char *const dialer_grade_names[4] = {
	"FATAL", "INVALID", "TOLERABLE", "VALID"
};

static const char *
dialer_grade_name(int grade)
{
	return (unsigned)grade > 3 ? "ILLEGAL!" : dialer_grade_names[grade];
}

/*
 * LOCAL (`t`) in the blob, but a `static` copy here is partially inlined by
 * -O3 and the plain name becomes `AnalyseDialString.part.0`, so the record
 * stays external and the differential tests reach it through the header.
 */
static int
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

	/*
	 * The author's own timestamp, printed on every analysis that gets
	 * this far.  17 May 1999, and "Analyze" with a z -- the symbol is
	 * spelled with an s.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "AnalyzeDialString: Updated 17 May 1999 00:50\n");

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
			 * everywhere.  See finding F52.
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

	if (store) {
		d->last_digit = last_digit;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "AnalyzeDialString: LAST_DIALABLE_SYMBOL is %d\n",
			    last_digit);
	}

	return grade;
}

int
IsDialStringInvalid(struct dialer *d, const char *s)
{
	int grade;

	GetDialerConfig(&d->cfg, d->modem);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DIALER.C: IsDialStringInvalid(char * "
				     "dialString )  got: %s\n", s);

	grade = AnalyseDialString(d, s, 0);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Dial String Syntax is %s\n",
				     dialer_grade_name(grade));

	return grade <= DIALER_INVALID;
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
	d->col = 0;
	d->pos = -1;
	d->row = 0;
	d->silence = 0;
	d->pulse_started = 0;

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

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Dial String Syntax is %s\n",
				     dialer_grade_name(d->grade));

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

/*
 * ---------------------------------------------------------------------------
 * GetNextDigitAndReturnNextState  .text 0x07abb0
 * DialerProgress                  .text 0x07b0e0
 *
 * Two halves of one loop.  The first walks the string and says what the next
 * thing to do is; the second does it, producing audio as it goes.
 *
 * The first is a file static in the original and takes its argument in a
 * register (finding F51), so it has no `ref_` symbol and can only be tested
 * through the second.  That is why they are written together.
 */

extern short TONE_read(short phase);
extern void PulseDialDigit(void *modem, int digit);
extern int IsPulseDialerReady(void *modem);

/*
 * The DTMF keypad, as frequencies.  `row` and `col` index these directly.
 * .rodata+0x5e72 and .rodata+0x5e68.
 */
static const short dtmf_row_hz[4] = { 697, 770, 852, 941 };
static const short dtmf_col_hz[4] = { 1209, 1336, 1477, 1633 };

/*
 * Hertz to phase increment for a 14-bit accumulator at 8000 Hz: 16384/8000 is
 * 2.048, and 16777 >> 13 is 2.04797.
 */
#define DTMF_HZ_TO_PHASE	16777

/* One cycle of the phase accumulator, and the shift onto TONE_read's 2048. */
#define DIALER_PHASE_MASK	0x3fff
#define DIALER_PHASE_SHIFT	3
#define DIALER_PHASE_ROUND	4

/*
 * What GetNextDigitAndReturnNextState reports.  These are also the generator's
 * state numbers -- the parser picks the state directly.
 *
 * NEXT_GAP has no character behind it: the generator has a case for it, but
 * every path through the parser's jump table returns something else, so
 * nothing can ask for a bare inter-digit gap.  Kept because the original's
 * switch has it.
 */
#define NEXT_DIGIT		1
#define NEXT_GAP		2	/* unreachable; see above */
#define NEXT_FLASH		3
#define NEXT_PAUSE		4
#define NEXT_WAIT_DIALTONE	5
#define NEXT_WAIT_ANSWER	6
#define NEXT_WAIT_BONG		7
#define NEXT_CALLING_TONE	8
#define NEXT_COMMAND		9
#define NEXT_END		10

/*
 * Walk to the next character that means something and say what it is.  A
 * character that means nothing -- a space, a bracket, anything the parser
 * merely tolerated -- is stepped over silently.
 */
static int
GetNextDigitAndReturnNextState(struct dialer *d)
{
	d->repeat = 1;

	if (d == 0)
		return NEXT_END;

	for (;;) {
		unsigned char c;

		d->pos++;
		c = (unsigned char)d->string[d->pos];
		if (c == '\0')
			return NEXT_END;

		/*
		 * Announced before the range check, so junk the parser is
		 * about to step over is announced too -- spaces included.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Digit is %c\n", c);

		if ((unsigned)((signed char)c - ' ') > 0x57)
			continue;

		switch (c) {
		/* The keypad, as a grid position. */
		case '1': d->row = 0; d->col = 0; return NEXT_DIGIT;
		case '2': d->row = 0; d->col = 1; return NEXT_DIGIT;
		case '3': d->row = 0; d->col = 2; return NEXT_DIGIT;
		case '4': d->row = 1; d->col = 0; return NEXT_DIGIT;
		case '5': d->row = 1; d->col = 1; return NEXT_DIGIT;
		case '6': d->row = 1; d->col = 2; return NEXT_DIGIT;
		case '7': d->row = 2; d->col = 0; return NEXT_DIGIT;
		case '8': d->row = 2; d->col = 1; return NEXT_DIGIT;
		case '9': d->row = 2; d->col = 2; return NEXT_DIGIT;
		case '0': d->row = 3; d->col = 1; return NEXT_DIGIT;

		/*
		 * `*` and `#` are the bottom row's outer keys, and A-D the
		 * fourth column.  All of them need tone mode -- there is no
		 * way to pulse them -- and A-D additionally need the country
		 * to allow it.
		 */
		case '*':
			if (d->cfg.tone_or_pulse != DIALER_TONE_DIALING)
				continue;
			d->row = 3; d->col = 0;
			return NEXT_DIGIT;
		case '#':
			if (d->cfg.tone_or_pulse != DIALER_TONE_DIALING)
				continue;
			d->row = 3; d->col = 2;
			return NEXT_DIGIT;
		case 'A': case 'a': case 'B': case 'b':
		case 'C': case 'c': case 'D': case 'd':
			if (d->cfg.abcd_permitted != 0)
				continue;
			if (d->cfg.tone_or_pulse != DIALER_TONE_DIALING)
				continue;
			d->row = (c | 0x20) - 'a';
			d->col = 3;
			return NEXT_DIGIT;

		case ',':
			/*
			 * Swallow a run of commas into one pause.  The count
			 * goes to the pause state, which multiplies it by the
			 * country's pause length and caps the result.
			 */
			if (d->string[d->pos + 1] == ',') {
				d->repeat++;
				continue;
			}
			return NEXT_PAUSE;

		case 'W': case 'w':	return NEXT_WAIT_DIALTONE;
		case '@':		return NEXT_WAIT_ANSWER;
		case '$':		return NEXT_WAIT_BONG;
		case '!':		return NEXT_FLASH;

		case '^':
			/* Only meaningful where a calling tone would be sent. */
			if (d->cfg.calling_tone == 1 || d->cfg.calling_tone == 3)
				return NEXT_CALLING_TONE;
			continue;

		case ';':
			if (d->string[d->pos + 1] == '\0')
				return NEXT_COMMAND;
			continue;

		/*
		 * Mode switches.  At the very start they just set the mode;
		 * later they are a switch, which the country may forbid.
		 */
		case 'T': case 't':
			if (d->pos == 0) {
				d->cfg.tone_or_pulse = DIALER_TONE_DIALING;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "Dialer.c: GetNextDigit... " "TONE_OR_PULSE_FLAG became "
					    "TONE_DIALING\n");
				continue;
			}
			if (d->cfg.tone_or_pulse != DIALER_PULSE_DIALING)
				continue;
			if (d->cfg.mixed_permitted != 0) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "Dialer.c: GetNextDigit... " "Not permitted to switch to " "Tone\n");
				continue;
			}
			d->cfg.tone_or_pulse = DIALER_TONE_DIALING;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Dialer.c: GetNextDigit..." " Switching to Tone\n");
			continue;

		case 'P': case 'p':
			if (d->pos == 0) {
				d->cfg.tone_or_pulse = DIALER_PULSE_DIALING;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "Dialer.c: GetNextDigit... " "TONE_OR_PULSE_FLAG became "
					    "PULSE_DIALING\n");
				continue;
			}
			if (d->cfg.tone_or_pulse != DIALER_TONE_DIALING)
				continue;
			if (d->cfg.mixed_permitted != 0) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "Dialer.c: GetNextDigit... " "Not permitted to switch to " "Pulse\n");
				continue;
			}
			d->cfg.tone_or_pulse = DIALER_PULSE_DIALING;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Dialer.c: GetNextDigit..." " Switching to Pulse\n");
			continue;

		default:
			continue;
		}
	}
}

void
DialerAbort(struct dialer *d)
{
	/*
	 * The one path that is an error, and the only one that does not reach
	 * the message below.
	 */
	if (d->progress_state > 10) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Dialer was aborted - error. \n");

		return;
	}

	/*
	 * Written as one condition rather than two early returns, which is
	 * what it was until the call sites were restored.  Behaviour is
	 * identical -- both returns did nothing but return -- but the object
	 * does not return there: 0x7be2c and 0x7bdf9 both fall into the same
	 * gate at 0x7bdfb, so an abort with nothing to release still says so.
	 * Three guards that look alike, and one of them is not like the others.
	 */
	if (d->pulse_released == 0 && d->pulse_active != 0) {
		LastPulseDigitDialed(d->modem);

		/* After the call, and before the flag is set (0x7be3c). */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(" **** Dialer.C: " "LastPulseDigitDialed was "
					     "called\n");

		d->pulse_released = 1;
	}

	/* A tail call in the object -- `jmp` at 0x7be11, not `call`. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Dialer was aborted.\n");
}

/* The country's rule for turning a keypad position into pulses. */
static int
pulse_count(const struct dialer *d)
{
	int n = d->row * 3 + d->col;

	switch (d->cfg.pulse_pattern) {
	case 1:					/* most of the world */
		return n + 1 == 11 ? 10 : n + 1;
	case 2:					/* Sweden */
		return n == 10 ? 1 : n + 2;
	case 3:					/* New Zealand */
		/*
		 * The only pattern that announces itself -- and with no
		 * newline, so whatever is printed next continues the line.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Original digitToPulseDial %d",
					     n + 1);
		if (n + 1 == 11)
			return 10;
		return n + 1 > 9 ? n + 1 : 10 - (n + 1);
	default:
		/*
		 * No pattern.  The original leaves the count at -1 and dials
		 * that, which never terminates.  No shipped country table
		 * reaches here -- see finding F56.
		 */
		return -1;
	}
}

/* Fill from *pos to `end`, inclusive, with silence. */
static void
emit_silence(short *buf, int *pos, int end)
{
	int i = *pos;

	while (i <= end)
		buf[i++] = 0;
	*pos = i;
}

/*
 * The gap that follows a digit, in samples.  Tone and pulse dialling have
 * separate settings and the country sets both.
 */
static int
dial_gap(const struct dialer *d)
{
	return (d->cfg.tone_or_pulse == DIALER_TONE_DIALING ? d->cfg.dtmf_gap : d->cfg.pulse_gap)
	       * 8;
}

/* One DTMF sample: two cosines, each at its own amplitude, summed. */
static short
dtmf_sample(struct dialer *d)
{
	int lo, hi;

	d->phase_low = (short)((d->phase_low + d->inc_low)
			       & DIALER_PHASE_MASK);
	d->phase_high = (short)((d->phase_high + d->inc_high)
				& DIALER_PHASE_MASK);

	lo = d->cfg.dtmf_low
	     * TONE_read((short)(((unsigned)(d->phase_low + DIALER_PHASE_ROUND))
				 >> DIALER_PHASE_SHIFT));
	hi = d->cfg.dtmf_high
	     * TONE_read((short)((d->phase_high + DIALER_PHASE_ROUND)
				 >> DIALER_PHASE_SHIFT));

	return (short)((lo >> 14) + (hi >> 14));
}


/*
 * Ask the parser what comes next and set up the state that will carry it out.
 *
 * The state number IS the parser's answer -- states 5 to 10 exist only to
 * return one code each and reset -- so everything below is preparation for a
 * state the parser has already chosen.
 */
static void
begin_next(struct dialer *d)
{
	int next = GetNextDigitAndReturnNextState(d);

	switch (next) {
	case NEXT_DIGIT:
		if (d->cfg.tone_or_pulse == DIALER_TONE_DIALING) {
			/*
			 * Arm the two oscillators.  The high tone starts a
			 * quarter cycle in (0x2000 of 0x4000), so the pair
			 * begins in quadrature rather than adding to a single
			 * large first sample.
			 */
			d->silence = d->cfg.dtmf_duration * 8;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Samples left = %d\n",
						     d->silence);
			d->phase_high = 0x2000;
			d->phase_low = 0;
			d->inc_low = (short)((dtmf_row_hz[d->row]
					      * DTMF_HZ_TO_PHASE) >> 13);
			d->inc_high = (short)((dtmf_col_hz[d->col]
					       * DTMF_HZ_TO_PHASE) >> 13);
		} else {
			d->pulse_started = 0;
		}
		break;

	case NEXT_GAP:
		d->silence = dial_gap(d);
		break;

	case NEXT_FLASH:
		d->pulse_started = 0;
		break;

	case NEXT_PAUSE: {
		/*
		 * A run of commas asked for `repeat` pauses; the country says
		 * how long one is and how long a run may grow to.  Seconds
		 * here, unlike everywhere else in this file.
		 */
		int total = d->repeat * d->cfg.pause;

		if (total > d->cfg.coma_pause_limit)
			total = d->cfg.coma_pause_limit;
		d->silence = total * 8000;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Consequitive-Commas Dialing; "
					     "Number of samples is %d.\n",
					     d->silence);
		break;
	}

	default:
		/* States 0 and 5-10 need nothing set up. */
		break;
	}

	d->progress_state = next;
}

/*
 * A digit has finished sounding.  Anything but the last one is followed by an
 * inter-digit gap; after the last there is nothing left to separate, so the
 * parser is asked for the next thing straight away.  That is what lets a
 * trailing `;` or `W` take effect the instant the final digit stops.
 */
static void
digit_finished(struct dialer *d)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Done Generating digit\n");

	if (d->pos == d->last_digit) {
		begin_next(d);
		return;
	}
	d->silence = dial_gap(d);
	d->progress_state = 2;
}

/*
 * Emit silence until `d->silence` runs out.  Returns 1 once it has, 0 when the
 * buffer filled first.  Both the inter-digit gap (state 2) and the comma pause
 * (state 4) are this -- but each announces its completion differently, which
 * is why the cases share this helper and not a body.
 */
static int
emit_gap(struct dialer *d, short *buf, int *pos, int limit)
{
	int room = limit - *pos + 1;
	int end;

	if (d->silence > room) {
		d->silence -= room;
		end = limit;
		emit_silence(buf, pos, end);
		return 0;
	}

	end = *pos + d->silence - 1;
	d->silence = 0;
	emit_silence(buf, pos, end);
	return 1;
}

/*
 * Generate DTMF.  Returns 1 when the digit's tone time has run out, 0 when the
 * buffer filled first.
 */
static int
tone_burst(struct dialer *d, short *buf, int *pos, int limit)
{
	int end = limit;
	int done = 0;
	int i;

	/*
	 * Tone dialling never needs the pulse dialler, so if a `P` prefix left
	 * it holding the line this is where it is let go.  The guard means it
	 * happens once, on the first buffer of the first tone.
	 */
	if (d->pulse_released == 0 && d->pulse_active != 0) {
		LastPulseDigitDialed(d->modem);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(" **** Dialer.C: " "LastPulseDigitDialed was "
					     "called\n");
		d->pulse_active = 0;
		d->pulse_released = 1;
	}

	/*
	 * A zero tone time never elapses: the burst fills every buffer it is
	 * handed and the dialler never moves on.  Reproduced rather than
	 * fixed -- GetDTMFDialSpeed returning zero is a country-data fault,
	 * and no shipped table does it.
	 */
	if (d->cfg.dtmf_duration != 0) {
		int room = limit - *pos + 1;

		if (d->silence > room) {
			d->silence -= room;
		} else {
			end = *pos + d->silence - 1;
			d->silence = 0;
			done = 1;
		}
	}

	for (i = *pos; i <= end; i++)
		buf[i] = dtmf_sample(d);
	*pos = i;

	return done;
}

/*
 * Hand the pulse dialler one digit and wait for it to finish.  Returns 1 when
 * it has, 0 when it is busy -- in which case the rest of the buffer is filled
 * with silence, since the line is being made and broken, not driven.
 *
 * The pulse count is computed here rather than by the caller because the
 * original computes it before every readiness poll while the digit has not
 * started, and NOT after it has -- visible only through pattern 3's debug
 * output, which announces the digit on exactly those polls.
 */
static int
pulse_digit(struct dialer *d, short *buf, int *pos, int limit)
{
	if (d->pulse_started == 0) {
		int count = pulse_count(d);

		if (!IsPulseDialerReady(d->modem)) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "=========> Waiting to " "PULSE_IS_DIALER_READY_PROC "
				    "to become true.\n");
			emit_silence(buf, pos, limit);
			return 0;
		}
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("=========> Calling " "PULSE_DIAL_DIGIT_PROC "
					     "with %d.\n", count);
		PulseDialDigit(d->modem, count);
		d->pulse_started = 1;
	}

	/* Still making and breaking the line: silence, and no message. */
	if (!IsPulseDialerReady(d->modem)) {
		emit_silence(buf, pos, limit);
		return 0;
	}
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("=========>END OF DIAL DIGIT " "DETECTED.\n");
	return 1;
}

int
DialerProgress(struct dialer *d, short *buf, int *pos, int limit)
{
	for (;;) {
		if (*pos > limit)
			return DIALER_BUSY;

		switch (d->progress_state) {
		case DIALER_INITIAL_STATE:
			/* Nothing in flight: find the next thing to do. */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("DIALER_INITIAL_STATE\n");
			begin_next(d);
			continue;

		case 1:
			if (d->cfg.tone_or_pulse == DIALER_TONE_DIALING) {
				if (tone_burst(d, buf, pos, limit))
					digit_finished(d);
				continue;
			}
			if (d->cfg.tone_or_pulse != DIALER_PULSE_DIALING) {
				/*
				 * Neither tone nor pulse.  The original spins
				 * here forever; DialerCreate normalises the
				 * flag to 0 or 1, so nothing reaches it.
				 */
				continue;
			}
			if (!pulse_digit(d, buf, pos, limit))
				continue;
			d->pulse_started = 0;
			d->pulse_released = 0;
			d->pulse_active = 1;
			digit_finished(d);
			continue;

		case 2:
			if (emit_gap(d, buf, pos, limit)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "Done Generating silence " "between digits\n");
				begin_next(d);
			}
			continue;

		case 4:
			if (emit_gap(d, buf, pos, limit)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "Done Generating Dial-Pause\n");
				begin_next(d);
			}
			continue;

		case 3:
			/*
			 * A hook flash: one break of `hook_flash` with no make
			 * either side, dialled as a single "pulse" through the
			 * ordinary pulse dialler with its timings rewritten.
			 * They are put back before the state ends, so the next
			 * digit is dialled normally.
			 */
			if (d->pulse_started == 0) {
				SetPulseMakeTime(d->modem, 0);
				SetPulseBreakTime(d->modem,
						  d->cfg.hook_flash * 10);
				if (!IsPulseDialerReady(d->modem)) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "=========> Waiting to " "PULSE_IS_DIALER_READY_"
						    "PROC to become true.\n");
					emit_silence(buf, pos, limit);
					continue;
				}
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "=========> Begin Dialing " "FLASH.\n");
				PulseDialDigit(d->modem, 1);
				d->pulse_started = 1;
			}
			if (!IsPulseDialerReady(d->modem)) {
				emit_silence(buf, pos, limit);
				continue;
			}
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "=========> End dialing Flash.\n");
			d->pulse_started = 0;
			SetPulseMakeTime(d->modem, d->cfg.pulse_make);
			SetPulseBreakTime(d->modem, d->cfg.pulse_break);
			d->pulse_released = 0;
			d->pulse_active = 1;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Done Generating flash\n");
			d->progress_state = 2;
			d->silence = d->cfg.pulse_gap * 8;
			continue;

		/*
		 * One state per modifier, each of which announces itself,
		 * reports its code once, and hands control back to state 0
		 * for whatever follows.
		 */
		case DIALER_WAIT_FOR_DIALTONE_STATE:
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "DIALER_WAIT_FOR_DIALTONE_STATE\n");
			d->progress_state = 0;
			return DIALER_WAIT_DIALTONE;

		case DIALER_WAIT_FOR_SILENCE_STATE:
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "DIALER_WAIT_FOR_SILENCE_STATE\n");
			d->progress_state = 0;
			return DIALER_WAIT_ANSWER;

		case DIALER_WAIT_FOR_BONGTONE_STATE:
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "DIALER_WAIT_FOR_BONGTONE_STATE\n");
			d->progress_state = 0;
			return DIALER_WAIT_BONG;

		case DIALER_CALLING_TONE_STATE:
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "DIALER_CALLING_TONE_STATE\n");
			d->progress_state = 0;
			return DIALER_CALLING_TONE;

		/*
		 * The two ways of stopping: `;` and the end of the string.
		 * Neither resets the state, so every further call announces
		 * and returns the same thing again.  The line is released
		 * first if pulse dialling still holds it -- but `pulse_active`
		 * is left set, unlike the release in tone_burst.  The two
		 * bodies are genuinely separate in the object, each with its
		 * own release sequence and announcement -- see finding F162,
		 * which corrects 158 on this point.
		 */
		case DIALER_END_PARTIALLY_STATE:
			if (d->pulse_released == 0 && d->pulse_active != 0) {
				LastPulseDigitDialed(d->modem);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    " **** Dialer.C: " "LastPulseDigitDialed was " "called\n");
				d->pulse_released = 1;
			}
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "DIALER_END_PARTIALLY_STATE\n");
			return DIALER_COMMAND;

		case DIALER_END_STATE:
			if (d->pulse_released == 0 && d->pulse_active != 0) {
				LastPulseDigitDialed(d->modem);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    " **** Dialer.C: " "LastPulseDigitDialed was " "called\n");
				d->pulse_released = 1;
			}
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("DIALER_END_STATE\n");
			return DIALER_DONE;

		default:
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "Dialer switch default case\n");
			return DIALER_BAD_STATE;
		}
	}
}
