/*
 * t_dialerprog.c -- differential test of the dialler's audio generator.
 *
 * `DialerProgress` is the only way to reach
 * `GetNextDigitAndReturnNextState`, which is a file static with no symbol, so
 * this one test covers both halves of Dialer.c's loop: the part that walks the
 * string and the part that turns what it finds into pulses, tones and
 * silence.
 *
 * The two sides cannot run interleaved.  Both reach the pulse dialler through
 * `modem_get_param(MDMPRM_DP_ADDR)`, and the parameter store holds one value,
 * so each side is driven to completion with the store pointed at its own call
 * object and the results compared afterwards.
 *
 * What is compared: every sample of every buffer, the return code from every
 * call, the whole dialler object, and the call object the pulse dialler
 * writes into.  Comparing return codes alone would pass a generator that
 * emitted the wrong audio, which is most of what this function is for.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/dialer.h"
#include "dsplib/pulse.h"
#include "dsplib/modem_params.h"

extern int ref_DialerProgress(struct dialer *d, short *buf, int *pos,
			      int limit);
extern int ref_DialerCreate(struct dialer *d, const char *s, void *modem);

#define BUFSAMP		160
#define MAXCALLS	4000

/* How often each return code was seen, over the whole run. */
static int code_seen[8];

/* Non-silent samples the reference produced, over every case. */
static long total_nonzero;

struct side {
	struct dialer	d;
	struct call	call;
	short		buf[MAXCALLS][BUFSAMP];
	int		code[MAXCALLS];
	int		pos[MAXCALLS];
	int		calls;
};

static struct side side_a, side_b;

/*
 * Two country flags most cases do not care about.  Kept as state rather than
 * as a seventh and eighth argument on every line of main().
 */
static int opt_calling_tone = 1;
static int opt_mixed;

static void
params(int pattern, int tone, int abcd, int pause, int cap)
{
	harness_param_reset();
	harness_param_set(GetPulseDialDigitPattern, pattern);
	harness_param_set(GetPulseDialingFlag, tone);
	harness_param_set(GetABCDDialingPermittedFlag, abcd);
	harness_param_set(GetPulseAndToneDialInSameDialStringPermittedFlag,
			  opt_mixed);
	harness_param_set(GetDialModifierValidation, 1);
	harness_param_set(GetDialPauseTime, pause);
	harness_param_set(GetComaPauseDurationLimit, cap);
	harness_param_set(GetPulseDialMakeTime, 33);
	harness_param_set(GetPulseDialBreakTime, 67);
	harness_param_set(GetPulseBetweenDigitsInterval, 8);
	harness_param_set(GetDTMFDialSpeed, 70);
	harness_param_set(GetDTMFHighToneLevel, 9);
	harness_param_set(GetDTMFHighAndLowToneLevelDifference, 2);
	harness_param_set(GetCallingToneFlag, opt_calling_tone);
	harness_param_set(GetHookFlashTime, 50);
}

/*
 * Run one side to completion.  `ref` selects which implementation and which
 * call object; the store is pointed at that object for the whole run.
 */
static void
drive(struct side *s, int ref, const char *dialstr, int pattern, int tone,
      int abcd, int pause, int cap)
{
	int n;

	memset(s, 0, sizeof(*s));
	s->call.self = &s->call;
	s->call.modem = (void *)0xD1A1u;

	params(pattern, tone, abcd, pause, cap);
	harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&s->call);

	if (ref)
		ref_DialerCreate(&s->d, dialstr, (void *)0xD1A1u);
	else
		DialerCreate(&s->d, dialstr, (void *)0xD1A1u);

	for (n = 0; n < MAXCALLS; n++) {
		int pos = 0;
		int code;

		if (ref)
			code = ref_DialerProgress(&s->d, s->buf[n], &pos,
						  BUFSAMP - 1);
		else
			code = DialerProgress(&s->d, s->buf[n], &pos,
					      BUFSAMP - 1);

		s->code[n] = code;
		s->pos[n] = pos;
		s->calls = n + 1;

		if (code == DIALER_DONE || code == DIALER_BAD_STATE)
			break;
		/*
		 * The event codes need the caller to do something before the
		 * dialler can continue; here nothing does, so the run stops.
		 */
		if (code == DIALER_WAIT_DIALTONE || code == DIALER_WAIT_ANSWER
		    || code == DIALER_WAIT_BONG)
			break;
	}
}

static int
run(const char *label, const char *dialstr, int pattern, int tone, int abcd,
    int pause, int cap)
{
	int n, i;
	int nonzero = 0;

	diff_begin(label);

	drive(&side_a, 1, dialstr, pattern, tone, abcd, pause, cap);
	drive(&side_b, 0, dialstr, pattern, tone, abcd, pause, cap);

	diff_eq_int("same number of calls", side_b.calls, side_a.calls, 0);

	for (n = 0; n < side_a.calls && n < side_b.calls; n++) {
		diff_eq_int("call %ld: code", side_b.code[n], side_a.code[n],
			    n);
		diff_eq_int("call %ld: pos", side_b.pos[n], side_a.pos[n], n);
		for (i = 0; i < BUFSAMP; i++) {
			diff_eq_int("call %ld: sample", side_b.buf[n][i],
				    side_a.buf[n][i], n);
			if (side_a.buf[n][i] != 0)
				nonzero++;
		}
		if (side_a.code[n] >= 0 && side_a.code[n] < 8)
			code_seen[side_a.code[n]]++;
	}

	/* The dialler's own state, and the call object it drove. */
	diff_eq_int("dialler object", memcmp(&side_a.d, &side_b.d,
					     sizeof(side_a.d)) == 0, 1, 0);
	diff_eq_int("progress_state", side_b.d.progress_state,
		    side_a.d.progress_state, 0);
	diff_eq_int("pos in string", side_b.d.pos, side_a.d.pos, 0);
	diff_eq_int("row", side_b.d.row, side_a.d.row, 0);
	diff_eq_int("col", side_b.d.col, side_a.d.col, 0);
	diff_eq_int("silence", side_b.d.silence, side_a.d.silence, 0);
	diff_eq_int("pulse_remaining", side_b.call.pulse_remaining,
		    side_a.call.pulse_remaining, 0);
	diff_eq_int("pulse_elapsed", side_b.call.pulse_elapsed,
		    side_a.call.pulse_elapsed, 0);
	diff_eq_int("pulse_off_hook", side_b.call.pulse_off_hook,
		    side_a.call.pulse_off_hook, 0);

	/*
	 * Anti-vacuity, counted across the whole run rather than per case:
	 * two of the cases below are meant to generate nothing at all, and
	 * a generator that emitted silence throughout would otherwise agree
	 * with itself on everything above.
	 */
	total_nonzero += nonzero;

	return diff_end();
}

/* run() with the two uncommon flags set, then put them back. */
static int
run_flags(int calling_tone, int mixed, const char *label, const char *dialstr,
	  int pattern, int tone, int abcd, int pause, int cap)
{
	int rc;

	opt_calling_tone = calling_tone;
	opt_mixed = mixed;
	rc = run(label, dialstr, pattern, tone, abcd, pause, cap);
	opt_calling_tone = 1;
	opt_mixed = 0;
	return rc;
}

int
main(void)
{
	int rc = 0;
	int c;

	/* Tone dialling, which is where the DTMF generator lives. */
	rc |= run("dial: tone, T5551234", "T5551234", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, every digit", "T1234567890", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, star and hash", "T*123#", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, ABCD allowed", "TABCD", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, ABCD refused", "TABCD", 1, 1, 1, 2, 10);
	rc |= run("dial: tone, one comma", "T5,5", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, comma run", "T5,,,,,,5", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, comma capped", "T5,,,,,,,,,,5", 1, 1, 0, 3, 4);
	rc |= run("dial: tone, trailing semicolon", "T555;", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, W modifier", "T5W5", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, at modifier", "T5@5", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, dollar modifier", "T5$5", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, bang modifier", "T5!5", 1, 1, 0, 2, 10);
	rc |= run("dial: tone, junk between digits", "T5()-. 5", 1, 1, 0, 2,
		  10);
	rc |= run("dial: empty", "T", 1, 1, 0, 2, 10);

	/*
	 * The calling tone, which is the only way to reach state 8, and both
	 * of the flag values that enable it -- plus one that does not, where
	 * the character has to be stepped over instead.
	 */
	rc |= run_flags(1, 0, "dial: caret, calling tone on", "T5^5", 1, 1, 0,
			2, 10);
	rc |= run_flags(3, 0, "dial: caret, calling tone mode 3", "T5^5", 1, 1,
			0, 2, 10);
	rc |= run_flags(0, 0, "dial: caret, calling tone off", "T5^5", 1, 1, 0,
			2, 10);

	/*
	 * Mode switches away from the first character, which is where the
	 * country's say in mixed dialling applies.  Both values of the flag,
	 * both directions of switch.
	 */
	rc |= run_flags(1, 0, "dial: switch to tone mid-string", "P5T5", 1, 0,
			0, 2, 10);
	rc |= run_flags(1, 1, "dial: switch to tone, mixed refused", "P5T5", 1,
			0, 0, 2, 10);
	rc |= run_flags(1, 0, "dial: switch to pulse mid-string", "T5P5", 1, 1,
			0, 2, 10);
	rc |= run_flags(1, 1, "dial: switch to pulse, mixed refused", "T5P5", 1,
			1, 0, 2, 10);

	/* A semicolon anywhere but the end means nothing and is stepped over. */
	rc |= run("dial: semicolon mid-string", "T5;5", 1, 1, 0, 2, 10);

	/* Pulse dialling, all three national patterns. */
	rc |= run("dial: pulse pattern 1", "P1234567890", 1, 0, 0, 2, 10);
	rc |= run("dial: pulse pattern 2 (Sweden)", "P1234567890", 2, 0, 0, 2,
		  10);
	rc |= run("dial: pulse pattern 3 (New Zealand)", "P1234567890", 3, 0,
		  0, 2, 10);
	rc |= run("dial: pulse, short", "P07", 1, 0, 0, 2, 10);
	rc |= run("dial: pulse with a pause", "P5,5", 1, 0, 0, 2, 10);

	/*
	 * Anti-vacuity.  A generator stuck on one code would agree with itself
	 * forever; the run must have produced several distinct ones.
	 */
	diff_begin("guards");
	{
		/*
		 * Every code the generator can return must have been returned
		 * by something above.  A count of distinct codes would have
		 * let DIALER_CALLING_TONE go untested, which is how this
		 * check came to be written this way.
		 */
		for (c = DIALER_BUSY; c <= DIALER_DONE; c++)
			diff_eq_int("code %ld was returned at least once",
				    code_seen[c] > 0, 1, c);
		diff_eq_int("audio was actually generated (%ld samples)",
			    total_nonzero > 10000, 1, total_nonzero);
	}
	rc |= diff_end();

	return rc;
}
