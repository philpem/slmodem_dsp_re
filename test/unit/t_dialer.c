/*
 * t_dialer.c -- differential test of the dial-string parser.
 *
 * `AnalyseDialString` CANNOT BE CALLED DIRECTLY.  It is a file static -- `t`
 * rather than `T` in the symbol table -- so `objcopy --redefine-syms` cannot
 * give it a `ref_` name, and there is nothing to link against.  That is also
 * why it uses a register calling convention (finding 51): GCC is free to pick
 * one for a function whose callers it can all see.
 *
 * So it is reached through `IsDialStringInvalid`, its only caller outside
 * DialerProgress.  That costs the exact grade -- the caller collapses four
 * grades into "dial it or do not" -- and costs the `store` path, which only
 * DialerProgress uses.  What survives is the classification itself, which is
 * the part with eight character classes and three configuration flags, and
 * that is swept exhaustively below.
 *
 * The grades the differential cannot see are asserted separately against our
 * own implementation, and clearly marked as such: they are claims about what
 * the disassembly says, not about what the blob does.  DialerProgress will
 * close that gap when it is reconstructed.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/dialer.h"
#include "dsplib/modem_params.h"

extern int ref_IsDialStringInvalid(struct dialer *d, const char *s);
extern int ref_DialerCreate(struct dialer *d, const char *s, void *modem);
extern void ref_DialerAbort(struct dialer *d);

/* The pulse dialler DialerCreate configures; see t_pulse.c. */
#include "dsplib/pulse.h"
static struct call call_a, call_b;

/* How often each grade came back, over the whole run. */
static int grade_seen[4];

static void
setup(struct dialer *d, int abcd, int mixed, int validation, int calling_tone)
{
	memset(d, 0, sizeof(*d));
	d->modem = (void *)0xD1A1u;
	d->cfg.abcd_permitted = abcd;
	d->cfg.mixed_permitted = mixed;
	d->cfg.modifier_validation = validation;
	d->cfg.calling_tone = calling_tone;
}

/*
 * Drive both sides through IsDialStringInvalid, with the flags supplied the
 * way the original gets them -- through the parameter store, since
 * IsDialStringInvalid refetches the configuration and would overwrite
 * anything written into the struct.
 *
 * Returns our own grade, for the separate assertions below.
 */
static int
one(const char *s, int store, int abcd, int mixed, int validation, int ct)
{
	struct dialer a, b, g;
	int ra, rb, grade;

	(void)store;

	harness_param_reset();
	harness_param_set(GetABCDDialingPermittedFlag, abcd);
	harness_param_set(GetPulseAndToneDialInSameDialStringPermittedFlag,
			  mixed);
	harness_param_set(GetDialModifierValidation, validation);
	harness_param_set(GetCallingToneFlag, ct);
	harness_param_set(GetDTMFHighToneLevel, 9);
	harness_param_set(GetDTMFHighAndLowToneLevelDifference, 2);

	setup(&a, 0, 0, 0, 0);
	setup(&b, 0, 0, 0, 0);

	ra = ref_IsDialStringInvalid(&a, s);
	rb = IsDialStringInvalid(&b, s);

	diff_eq_int("verdict", rb, ra, 0);
	/*
	 * The whole object, which includes the configuration both sides just
	 * fetched and the last_digit field neither should have touched.
	 */
	diff_eq_int("object matches", memcmp(&a, &b, sizeof(a)) == 0, 1, 0);
	diff_eq_int("same parameter count", harness_param_ours.calls,
		    harness_param_ref.calls, 0);

	/*
	 * And our grade, which the differential cannot see.  Recorded so the
	 * anti-vacuity guard can prove all four are reachable.
	 */
	setup(&g, abcd, mixed, validation, ct);
	grade = AnalyseDialString(&g, s, 1);
	if (grade >= 0 && grade < 4)
		grade_seen[grade]++;

	/* Whatever the grade, the caller's boolean must follow from it. */
	diff_eq_int("verdict follows the grade", rb,
		    grade <= DIALER_INVALID, 0);

	return grade;
}

/*
 * Every character, in the middle of an otherwise valid string, at every flag
 * combination.  Sixteen combinations times ninety-odd characters.
 */
static int
run_every_character(void)
{
	int c, abcd, mixed, val, ct;

	diff_begin("AnalyseDialString: every character, every flag");

	for (c = 1; c < 256; c++) {
		char s[8];

		s[0] = 'T';
		s[1] = '5';
		s[2] = (char)c;
		s[3] = '6';
		s[4] = '\0';

		for (abcd = 0; abcd <= 1; abcd++)
		 for (mixed = 0; mixed <= 1; mixed++)
		  for (val = 0; val <= 1; val++)
		   for (ct = 1; ct <= 2; ct++)
			one(s, 1, abcd, mixed, val, ct);
	}

	return diff_end();
}

/*
 * The things a character sweep cannot reach: the mode letter, the length
 * limit, the null string, and `;` in a position where it is legal.
 */
static int
run_shapes(void)
{
	static const char *strings[] = {
		"", "T", "P", "t", "p", "5551234", "X5551234",
		"T5551234", "P5551234", "t5551234;", "T;", "T5;",
		"T5;5", "T,,,555", "T555W123", "T@555", "T$555",
		"T(555)1234", "T555-1234", "TABCD", "TabcD", "T^555",
		"T555^", "TP555", "PT555", "TT555", "PP555",
		"T555P123", "P555T123", "T ", "T\t555", "T\xff""555",
		"T\x80", "T\x7f", "T5\x1f""5", "TW", "Tw", "T!*#",
		"T0123456789", "T;;", "T;5"
	};
	unsigned k;
	int abcd, mixed, val, ct, store;

	diff_begin("AnalyseDialString: string shapes");

	for (k = 0; k < sizeof(strings) / sizeof(strings[0]); k++)
		for (abcd = 0; abcd <= 1; abcd++)
		 for (mixed = 0; mixed <= 1; mixed++)
		  for (val = 0; val <= 1; val++)
		   for (ct = 0; ct <= 2; ct++)
		    for (store = 0; store <= 1; store++)
			one(strings[k], store, abcd, mixed, val, ct);

	/* NULL, which is a pointer rather than a string. */
	one(0, 0, 0, 0, 0, 1);
	one(0, 1, 1, 1, 1, 2);

	return diff_end();
}

/*
 * Length: 100 characters is the last acceptable one and 101 is FATAL, because
 * the buffer the string would be copied into is exactly 100 bytes and the
 * configuration begins immediately after it.
 */
static int
run_length(void)
{
	char s[160];
	int n;

	diff_begin("AnalyseDialString: length limit");

	for (n = 96; n <= 108; n++) {
		int i, g;
		char msg[80];

		s[0] = 'T';
		for (i = 1; i < n; i++)
			s[i] = '5';
		s[n] = '\0';

		g = one(s, 1, 0, 0, 0, 1);
		snprintf(msg, sizeof(msg), "length %d graded %d", n, g);
		diff_eq_int(msg, g, n <= DIALER_MAX_STRING ? DIALER_VALID
						           : DIALER_FATAL, n);
	}

	return diff_end();
}

/*
 * DialerCreate, which is also the only path that reaches AnalyseDialString's
 * `store` argument -- so `last_digit` becomes comparable here, where the
 * parser test could not see it.
 *
 * It also configures the pulse dialler through SetPulse*Time, so the call
 * object each side writes into is checked too.  Those go through
 * MDMPRM_DP_ADDR, which is why the two sides run one at a time.
 */
static int
run_create(void)
{
	static const struct {
		const char *s;
		int make, brk;
	} cases[] = {
		{ "T5551234", 33, 67 },
		{ "P5551234", 40, 60 },
		{ "5551234",  33, 67 },
		{ "",         33, 67 },
		{ "T",        33, 67 },
		{ "T555%%%1", 33, 67 },
		{ "T555,,,1", 10, 90 },
		{ "T5;",      33, 67 },
		{ "TABCD",    33, 67 },
		{ "T555^",    33, 67 },
		{ "t555W123", 1, 1 }
	};
	unsigned k;
	int val;

	diff_begin("DialerCreate");

	for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++)
	 for (val = 0; val <= 1; val++) {
		struct dialer a, b;
		int ra, rb;
		char msg[128];

		memset(&call_a, 0, sizeof(call_a));
		memset(&call_b, 0, sizeof(call_b));
		call_a.self = &call_a;
		call_b.self = &call_b;
		memset(&a, 0xA5, sizeof(a));
		memset(&b, 0xA5, sizeof(b));

		harness_param_reset();
		harness_param_set(GetDialModifierValidation, val);
		harness_param_set(GetPulseDialMakeTime, cases[k].make);
		harness_param_set(GetPulseDialBreakTime, cases[k].brk);
		harness_param_set(GetDTMFHighToneLevel, 9);
		harness_param_set(GetDTMFHighAndLowToneLevelDifference, 2);
		harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&call_a);
		ra = ref_DialerCreate(&a, cases[k].s, (void *)0xD1A1u);

		harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&call_b);
		rb = DialerCreate(&b, cases[k].s, (void *)0xD1A1u);

		snprintf(msg, sizeof(msg), "\"%s\" val=%d: return", cases[k].s,
			 val);
		diff_eq_int(msg, rb, ra, (long)k);
		diff_eq_int("whole object", memcmp(&a, &b, sizeof(a)) == 0, 1,
			    (long)k);
		/*
		 * Named separately so a failure says which field, since a
		 * memcmp only says "somewhere".
		 */
		diff_eq_int("grade", b.grade, a.grade, (long)k);
		diff_eq_int("last_digit", b.last_digit, a.last_digit, (long)k);
		diff_eq_int("pos", b.pos, a.pos, (long)k);
		diff_eq_int("string", strcmp(a.string, b.string), 0, (long)k);

		/* And the pulse timings it pushed into the call object. */
		diff_eq_int("pulse_make", call_b.pulse_make, call_a.pulse_make,
			    (long)k);
		diff_eq_int("pulse_break", call_b.pulse_break,
			    call_a.pulse_break, (long)k);
		diff_eq_int("make reached the call object", call_a.pulse_make,
			    cases[k].make, (long)k);
	}

	/* NULL, which succeeds and leaves an empty string. */
	{
		struct dialer a, b;

		memset(&call_a, 0, sizeof(call_a));
		memset(&call_b, 0, sizeof(call_b));
		call_a.self = &call_a;
		call_b.self = &call_b;
		memset(&a, 0x5A, sizeof(a));
		memset(&b, 0x5A, sizeof(b));

		harness_param_reset();
		harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&call_a);
		diff_eq_int("NULL: ref returns", ref_DialerCreate(&a, 0,
			    (void *)0xD1A1u), 0, 0);
		harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&call_b);
		diff_eq_int("NULL: we return", DialerCreate(&b, 0,
			    (void *)0xD1A1u), 0, 0);
		diff_eq_int("NULL: whole object", memcmp(&a, &b, sizeof(a)) == 0,
			    1, 0);
		diff_eq_int("NULL: string is empty", b.string[0], 0, 0);
	}

	return diff_end();
}

/*
 * DialerAbort, over every combination of the three fields it looks at.
 * Sixteen states, of which exactly one does anything.
 */
static int
run_abort(void)
{
	int state, active, released;

	diff_begin("DialerAbort");

	for (state = 0; state <= 12; state++)
	 for (active = 0; active <= 1; active++)
	  for (released = 0; released <= 1; released++) {
		struct dialer a, b;
		char msg[128];

		memset(&a, 0, sizeof(a));
		memset(&b, 0, sizeof(b));
		a.modem = b.modem = (void *)0xD1A1u;
		a.progress_state = b.progress_state = state;
		a.pulse_active = b.pulse_active = active;
		a.pulse_released = b.pulse_released = released;

		harness_param_reset();
		harness_modem_reset(0, 0);
		ref_DialerAbort(&a);
		DialerAbort(&b);

		snprintf(msg, sizeof(msg),
			 "state=%d active=%d released=%d", state, active,
			 released);
		diff_eq_int(msg, memcmp(&a, &b, sizeof(a)) == 0, 1, state);
		diff_eq_int("same host notifications",
			    harness_modem_ours.nparams,
			    harness_modem_ref.nparams, state);

		/*
		 * The one state that acts: not yet released, a digit active,
		 * and the progress state within bounds.
		 */
		if (state <= 10 && active && !released) {
			diff_eq_int("released the line", a.pulse_released, 1,
				    state);
			diff_eq_int("told the host", harness_modem_ref.nparams,
				    1, state);
		} else {
			diff_eq_int("did nothing", a.pulse_released, released,
				    state);
			diff_eq_int("told the host nothing",
				    harness_modem_ref.nparams, 0, state);
		}
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int g;

	rc |= run_every_character();
	rc |= run_shapes();
	rc |= run_length();
	rc |= run_create();
	rc |= run_abort();

	/*
	 * Anti-vacuity.  All four grades must have been produced; a parser
	 * that returned VALID for everything would agree with itself
	 * perfectly and pass everything above.
	 */
	diff_begin("guards");
	for (g = 0; g < 4; g++) {
		static const char *const names[4] = {
			"FATAL", "INVALID", "TOLERABLE", "VALID"
		};
		char msg[80];

		snprintf(msg, sizeof(msg), "%s was produced (%d times)",
			 names[g], grade_seen[g]);
		diff_eq_int(msg, grade_seen[g] > 0, 1, g);
	}
	rc |= diff_end();

	return rc;
}
