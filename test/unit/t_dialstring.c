/*
 * t_dialstring.c -- AnalyseDialString, called directly at last.
 *
 * t_dialer opens by saying this function "CANNOT BE CALLED DIRECTLY", and
 * finding 51 says the same and draws a rule from it.  Half of that was true.
 * The symbol is file-local and had no `ref_` alias, so t_dialer reaches it
 * through `IsDialStringInvalid`, which pays twice: the caller collapses the
 * four grades into `grade <= DIALER_INVALID`, and it never passes `store`.
 * Finding 221 removed the first half -- --globalize-symbols promotes the
 * symbol and the rename map then applies -- so `ref_AnalyseDialString` links.
 *
 * THE SECOND HALF OF FINDING 51 IS STILL TRUE, AND IS THE POINT
 *
 * "A `t` symbol is not a testing inconvenience, it is a signal that the
 * calling convention may not be the C one.  Check before writing the
 * prototype, not after the comparison fails."  So, checked:
 *
 *     7a9f0:  sub    $0x1c,%esp
 *     7a9f7:  mov    %edx,%ebx          <- the string, in edx
 *     7aa04:  mov    %eax,%esi          <- the dialler, in eax
 *     7aa26:  mov    %eax,0xa0(%esi)         (and last_digit = -2)
 *     7aae9:  mov    0x20(%esp),%ebx    <- `store`, on the stack
 *     7aab5:  ret                            caller cleans up
 *
 * and from `IsDialStringInvalid`, which is the shape from the other side:
 *
 *     7af5b:  movl   $0x0,(%esp)        <- store
 *     7af62:  mov    %ebx,%eax          <- d
 *     7af64:  mov    %esi,%edx          <- s
 *     7af66:  call   7a9f0 <AnalyseDialString>
 *
 * Two arguments in eax and edx, the third on the stack, and the caller pops:
 * exactly GCC's `regparm(2)`, which is what it picks for a static function
 * whose callers it can all see.  The declaration below says so, and getting
 * that wrong would not have failed to link -- it would have passed a stack
 * slot as a pointer and crashed, or worse, not crashed.
 *
 * Our own copy has external linkage, so GCC gives it the ordinary convention
 * and it is declared the ordinary way.  Each side is called as its own
 * compiler built it; that difference is a property of the two builds and not
 * of the algorithm, which is what the comparison is about.
 *
 * WHAT THIS ADDS OVER t_dialer
 *
 * The exact grade rather than a boolean -- t_dialer asserts the four grades
 * against our own implementation only, and says so, "claims about what the
 * disassembly says, not about what the blob does".  They are claims about the
 * blob now.  And the `store` path, which no caller in the suite reaches.
 * The whole `struct dialer` is compared, not just the return.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/dialer.h"

extern unsigned int ref_dsplibs_debug_level;

/* See the header comment.  regparm(2), and the third argument on the stack. */
extern int ref_AnalyseDialString(struct dialer *d, const char *s, int store)
	__attribute__((regparm(2)));

static int grade_seen[4];
static long stored_something;

static void
setup(struct dialer *d, int abcd, int mixed, int validation, int calling_tone)
{
	/*
	 * 0xa5, not 0: a field the function never writes must hold the same
	 * thing on both sides for the object comparison to mean anything, and
	 * zero is the one filler that makes "never written" look deliberate.
	 * Same argument as HARNESS_MALLOC_FILL.
	 */
	memset(d, 0xa5, sizeof(*d));
	memset(&d->cfg, 0, sizeof(d->cfg));
	d->modem = (void *)0xD1A1u;
	d->last_digit = -2;
	d->grade = 0;
	d->cfg.abcd_permitted = abcd;
	d->cfg.mixed_permitted = mixed;
	d->cfg.modifier_validation = validation;
	d->cfg.calling_tone = calling_tone;
}

/*
 * One string, one flag combination, both values of `store`.  Returns the
 * reference's grade so the sweep can count what it has seen.
 */
static int
one(const char *s, int store, int abcd, int mixed, int validation, int ct,
    long tag)
{
	struct dialer a, b;
	int ra, rb;

	setup(&a, abcd, mixed, validation, ct);
	setup(&b, abcd, mixed, validation, ct);

	ra = ref_AnalyseDialString(&a, s, store);
	rb = AnalyseDialString(&b, s, store);

	diff_eq_int("grade [%ld]", rb, ra, tag);
	diff_eq_obj("the dialler object", struct dialer, &b, &a, tag);

	if (ra >= 0 && ra < 4)
		grade_seen[ra]++;
	if (a.last_digit != -2)
		stored_something++;
	return ra;
}

int
main(void)
{
	/*
	 * Strings chosen for the eight character classes and the three flags,
	 * plus the two length boundaries: DIALER_MAX_STRING is 100 and
	 * anything over that grades FATAL.
	 */
	static const char *const strings[] = {
		"", "T", "P", "t", "p", "5551234",
		"T5551234", "P5551234", "T555-1234", "T(555)1234",
		"T555,1234", "T,,,5551234", "T555,,,", "T5W551234",
		"T555@1234", "T555!1234", "T555;1234", "T555:1234",
		"TABCD", "TABCD1234", "PABCD", "T*#5551234", "P*#5551234",
		"T555P1234", "P555T1234", "TP5551234", "PT5551234",
		"T ", "T\t5551234", "T\n", "T555 1234",
		"T5555555555555555555555555555555555555555555555555"
		"5555555555555555555555555555555555555555555555555",
		"T55555555555555555555555555555555555555555555555555"
		"55555555555555555555555555555555555555555555555555",
		"T555555555555555555555555555555555555555555555555555"
		"555555555555555555555555555555555555555555555555555"
	};
	static char probe[8];
	int rc = 0;
	int abcd, mixed, val, ct, store;
	unsigned k, c, lvl;
	long tag = 0, lines = 0;

	/*
	 * The named strings, at every flag combination and both `store`
	 * values.  Sixteen combinations because the three flags that reach
	 * the parser test with the opposite polarity to their names
	 * (finding 51's "three flags test inverted"), so both sides of each
	 * have to be walked rather than assumed inert.
	 */
	diff_begin("AnalyseDialString: strings and flags");
	for (k = 0; k < sizeof(strings) / sizeof(strings[0]); k++)
		for (abcd = 0; abcd <= 1; abcd++)
			for (mixed = 0; mixed <= 1; mixed++)
				for (val = 0; val <= 1; val++)
					for (ct = 0; ct <= 1; ct++)
						for (store = 0; store <= 1;
						     store++)
							one(strings[k], store,
							    abcd, mixed, val,
							    ct, tag++);
	rc |= diff_end();

	/*
	 * Every byte value, in the position where the eight classes are
	 * dispatched -- an 88-entry jump table over `c - ' '`, so everything
	 * outside 0x20..0x77 falls to the same arm and everything inside does
	 * not.  Driven directly, the exact grade for each is compared, which
	 * through IsDialStringInvalid it never was.
	 */
	diff_begin("AnalyseDialString: every byte, both modes");
	for (c = 1; c < 256; c++) {
		probe[0] = 'T';
		probe[1] = (char)c;
		probe[2] = '5';
		probe[3] = '\0';
		one(probe, 1, 0, 0, 0, 0, (long)c);
		one(probe, 1, 1, 1, 1, 1, (long)(c + 256));
		probe[0] = 'P';
		one(probe, 1, 0, 0, 0, 0, (long)(c + 512));
		one(probe, 1, 1, 1, 1, 1, (long)(c + 768));
	}
	rc |= diff_end();

	/*
	 * And with the diagnostics on.  Not a claim on the dead-site count --
	 * debugcov reports six sites in dialer.c that never execute and all
	 * six are in `pulse_digit` and `DialerProgress`, which nothing here
	 * touches.  What this adds is the transcript at grades and on the
	 * `store` path that the IsDialStringInvalid route cannot produce: the
	 * function announces what it decided, and it decides more things when
	 * it is called directly.
	 *
	 * BOTH levels have to move together or the two sides take different
	 * branches for reasons unrelated to the dialler.
	 */
	diff_begin("AnalyseDialString: the diagnostics");
	for (lvl = 1; lvl <= 3; lvl++) {
		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_on = 1;

		for (k = 0; k < sizeof(strings) / sizeof(strings[0]); k++)
			one(strings[k], (int)(k & 1), 1, 0, 1, 0,
			    (long)(lvl * 1000 + k));

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		diff_eq_int("transcript matches (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		diff_eq_int("line counts match (level %ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), (long)lvl);
		lines += dsplib_debug_capture_lines(1);
	}
	diff_eq_int("diagnostics were captured (%ld lines)", lines > 0, 1,
		    lines);
	rc |= diff_end();

	/*
	 * Anti-vacuity.  Comparing two grades proves nothing if only one grade
	 * ever came back, and comparing two objects proves nothing if `store`
	 * never stored: `last_digit` is the only field the flag controls, so
	 * a sweep in which it stayed at its -2 never walked that path at all.
	 */
	diff_begin("guards");
	{
		int distinct = 0;
		int g;

		for (g = 0; g < 4; g++)
			if (grade_seen[g] != 0)
				distinct++;
		diff_eq_int("every grade was returned (%ld of 4)", distinct, 4,
			    distinct);
		diff_eq_int("the store path stored (%ld)", stored_something > 0,
			    1, stored_something);
	}
	rc |= diff_end();

	return rc;
}
