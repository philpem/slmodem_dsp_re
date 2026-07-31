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
#include <string.h>

#include "harness.h"
#include "dsplib/dialer.h"
#include "dsplib/modem_params.h"

extern int ref_IsDialStringInvalid(struct dialer *d, const char *s);

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

int
main(void)
{
	int rc = 0;
	int g;

	rc |= run_every_character();
	rc |= run_shapes();
	rc |= run_length();

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
