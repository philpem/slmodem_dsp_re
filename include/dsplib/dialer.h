/*
 * dialer.h -- Dialler: turning a dial string into tones and pulses.
 *
 * The dialler holds the string it is working through, a copy of the country's
 * dialling rules, and where it has got to.  It is not allocated: it lives
 * inside the call-progress supervisor, which is why `DialerCreate` takes an
 * object rather than returning one.
 */

#ifndef DSPLIB_DIALER_H
#define DSPLIB_DIALER_H

#include "dsplib/dialercfg.h"

/*
 * The dial string lives at the very start of the object and the configuration
 * begins immediately after it, at +0x64.  That is not a coincidence: it is
 * why `AnalyseDialString` rejects anything longer than 100 characters.
 */
#define DIALER_MAX_STRING	100

/*
 * How `AnalyseDialString` grades a string.  The names are the object's own --
 * it carries them at .rodata+0x613c for its debug output -- and
 * `IsDialStringInvalid` is exactly `grade <= DIALER_INVALID`.
 */
#define DIALER_FATAL		0	/* too long to store             */
#define DIALER_INVALID		1
#define DIALER_TOLERABLE	2	/* oddities, but dial it anyway  */
#define DIALER_VALID		3

struct dialer {
	char	string[DIALER_MAX_STRING];	/* +0x00 */
	struct dialer_cfg cfg;			/* +0x64 */

	/*
	 * Index of the last character that would actually send something --
	 * a digit rather than a pause or a modifier.  -2 when there is none,
	 * and only written when `AnalyseDialString` is asked to store it.
	 */
	int	last_digit;			/* +0xa0 */

	/* What AnalyseDialString made of the string, kept for the caller. */
	int	grade;				/* +0xa4 */

	/*
	 * How many consecutive commas the last pause stood for.  Reset to 1 by
	 * every call to GetNextDigitAndReturnNextState and incremented as it
	 * walks a run, so ",,," is one pause of three units rather than three
	 * pauses.
	 */
	int	repeat;				/* +0xa8 */

	/* How far through the string the dialler has got.  -1 before it starts. */
	int	pos;				/* +0xac */

	/*
	 * The keypad position of the digit being sent, 0-based.  Row and
	 * column index the DTMF frequency tables directly, and together they
	 * give the pulse count through the country's pattern.
	 */
	int	row;				/* +0xb0 */
	int	col;				/* +0xb4 */

	/*
	 * +0xb8.  The dialler's own state machine, named by the AUTHOR --
	 * recovered from DialerProgress's debug strings, which this
	 * reconstruction had dropped (findings 134, 138).  Four are pinned to
	 * their case exactly; the rest are named but not yet placed.
	 *
	 * Note DIALER_WAIT_FOR_SILENCE_STATE: it returns DIALER_WAIT_ANSWER,
	 * which this header describes as "'@'".  The author's name is the
	 * better one -- '@' waits for quiet, not for an answer as such.
	 */
	int	progress_state;			/* +0xb8 */

	/* Samples of silence still owed to the caller. */
	int	silence;			/* +0xbc */

	/*
	 * The DTMF generator: two 14-bit phase accumulators and their
	 * increments.  Same idiom as the calling tone and, unlike it, correct
	 * -- see D11.
	 */
	short	phase_low;			/* +0xc0 */
	short	phase_high;			/* +0xc2 */
	short	inc_low;			/* +0xc4 */
	short	inc_high;			/* +0xc6 */

	/* Whether a pulse train has been started for the current digit. */
	int	pulse_started;			/* +0xc8 */

	/*
	 * The pulse dialler's handshake.  `pulse_active` says a digit is being
	 * pulsed and `pulse_released` says the host has been told it is over;
	 * DialerAbort is the only place both are visible at once.
	 */
	int	pulse_active;			/* +0xcc */
	int	pulse_released;			/* +0xd0 */

	void	*modem;				/* +0xd4 */
};

/*
 * Grade a dial string.
 *
 * NOT the C calling convention: the original takes `d` in `eax` and `s` in
 * `edx`, with `store` on the stack -- GCC's regparm(2), used for calls that
 * never leave Dialer.c.  Anything declaring this for the differential test
 * must say `__attribute__((regparm(2)))` or it will pass arguments the callee
 * never reads.  See finding 51.
 *
 * `store` asks for `d->last_digit` to be updated; the grade is returned
 * either way.
 */
int AnalyseDialString(struct dialer *d, const char *s, int store);

/* True when the string is too poor to dial: `AnalyseDialString(...) <= 1`. */
int IsDialStringInvalid(struct dialer *d, const char *s);

/*
 * Prepare a dialler.  Does NOT allocate -- the object belongs to the
 * call-progress supervisor, which is why this takes one rather than returning
 * one.
 *
 * Returns 0 when the string was accepted and DIALER_CREATE_REJECTED when it
 * was not.  A null string is accepted, and leaves an empty one behind.
 */
#define DIALER_CREATE_REJECTED	7

int DialerCreate(struct dialer *d, const char *s, void *modem);

/*
 * Give up on the current digit, telling the host the pulse dialler has
 * finished with the line -- but only once, and only if a digit was actually
 * being pulsed.
 */
void DialerAbort(struct dialer *d);

/*
 * What DialerProgress reports.  The first six mirror the modifiers
 * `GetNextDigitAndReturnNextState` finds in the string; the last two are the
 * dialler's own.
 */
/*
 * Every code above zero corresponds to one dial-string modifier, and each is
 * reached through a state of its own -- the parser returns the state number
 * and the state returns the code.  A hook flash and a comma pause have no
 * code: both are carried out inside the generator without the caller hearing
 * about it, which is why the list jumps straight from `busy` to `W`.
 */
#define DIALER_BUSY		0	/* buffer full, nothing to report   */
#define DIALER_WAIT_DIALTONE	1	/* 'W'                              */
#define DIALER_WAIT_ANSWER	2	/* '@'                              */
#define DIALER_WAIT_BONG	3	/* '$' -- the credit-card tone      */
#define DIALER_CALLING_TONE	4	/* '^'                              */
#define DIALER_COMMAND		5	/* ';' -- back to command mode      */
#define DIALER_DONE		6	/* end of string                    */
#define DIALER_BAD_STATE	7	/* progress_state out of range      */

/*
 * `progress_state` values, in the author's own names.  Confirmed by which
 * debug string each switch case prints (finding 138), and the last two --
 * INITIAL and END -- by the jump table at .rodata+0x614c once their
 * announcements were found inside cases 0 and 10 after all (finding 162).
 * States 1-4 (digit, gap, flash, pause) never announce and have no
 * recovered names.
 */
#define DIALER_INITIAL_STATE		0
#define DIALER_WAIT_FOR_DIALTONE_STATE	5	/* -> DIALER_WAIT_DIALTONE */
#define DIALER_WAIT_FOR_SILENCE_STATE	6	/* -> DIALER_WAIT_ANSWER   */
#define DIALER_WAIT_FOR_BONGTONE_STATE	7	/* -> DIALER_WAIT_BONG     */
#define DIALER_CALLING_TONE_STATE	8	/* -> DIALER_CALLING_TONE  */
#define DIALER_END_PARTIALLY_STATE	9	/* -> DIALER_COMMAND       */
#define DIALER_END_STATE		10	/* -> DIALER_DONE          */

/*
 * Produce the next stretch of dialling audio.
 *
 * Fills `buf` from `*pos` up to and including `limit`, advancing `*pos`, and
 * returns one of the codes above.  It is a generator, not a poll: pauses and
 * inter-digit gaps are silence it writes itself, not silence it asks for.
 */
int DialerProgress(struct dialer *d, short *buf, int *pos, int limit);

#endif /* DSPLIB_DIALER_H */
