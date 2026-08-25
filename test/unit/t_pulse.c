/*
 * t_pulse.c -- differential test of the pulse dialler.
 *
 * These five functions reach the call object through
 * `modem_get_param(MDMPRM_DP_ADDR)`, so the test supplies one by overriding
 * that parameter with a pointer to its own -- which is exactly how the
 * original works, and means no part of the path is faked.
 *
 * The interesting one is `IsPulseDialerReady`, which is a state machine
 * clocked once per call.  Comparing its return value alone would pass a
 * reconstruction whose phase was wrong, because the return only reports
 * "digit finished"; so every tick also compares all three timing fields and
 * the hook state, and the hook transitions are counted so the test can assert
 * that the right number of pulses actually went out.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/pulse.h"
#include "dsplib/modem_params.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_SetPulseMakeTime(void *modem, int ms);
extern void ref_SetPulseBreakTime(void *modem, int ms);
extern void ref_LastPulseDigitDialed(void *modem);
extern void ref_PulseDialDigit(void *modem, int digit);
extern int ref_IsPulseDialerReady(void *modem);

/* Two call objects, one per side, each pointed at by its own parameter store. */
static struct call obj_a, obj_b;

static void
reset_objects(int make, int brk)
{
	memset(&obj_a, 0, sizeof(obj_a));
	memset(&obj_b, 0, sizeof(obj_b));
	obj_a.self = &obj_a;
	obj_b.self = &obj_b;
	obj_a.modem = (void *)0xAAAAu;
	obj_b.modem = (void *)0xBBBBu;
	obj_a.pulse_make = obj_b.pulse_make = make;
	obj_a.pulse_break = obj_b.pulse_break = brk;
}

/*
 * The parameter store cannot hold two different values for one parameter, so
 * the two sides are driven one at a time with the store pointed at whichever
 * object is in play.
 */
static void
point_at(struct call *c)
{
	harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)c);
}

static void
compare(const char *what, long n)
{
	diff_eq_int("%s: pulse_make", obj_b.pulse_make, obj_a.pulse_make, n);
	diff_eq_int("%s: pulse_break", obj_b.pulse_break, obj_a.pulse_break, n);
	diff_eq_int("tick %ld: pulse_remaining", obj_b.pulse_remaining,
		    obj_a.pulse_remaining, n);
	diff_eq_int("tick %ld: pulse_elapsed", obj_b.pulse_elapsed,
		    obj_a.pulse_elapsed, n);
	diff_eq_int("tick %ld: pulse_off_hook", obj_b.pulse_off_hook,
		    obj_a.pulse_off_hook, n);
	(void)what;
}

/*
 * ---------------------------------------------------------------------------
 * The setters and the two one-liners.
 */
static int
run_setters(void)
{
	static const int values[] = { 0, 1, 5, 33, 67, 1000, -1, 0x7fffffff };
	unsigned k;

	diff_begin("pulse: SetPulseMakeTime / SetPulseBreakTime");

	for (k = 0; k < sizeof(values) / sizeof(values[0]); k++) {
		reset_objects(0, 0);

		harness_param_reset();
		point_at(&obj_a);
		ref_SetPulseMakeTime((void *)0xAAAAu, values[k]);
		ref_SetPulseBreakTime((void *)0xAAAAu, values[k] + 1);

		harness_param_reset();
		point_at(&obj_b);
		SetPulseMakeTime((void *)0xBBBBu, values[k]);
		SetPulseBreakTime((void *)0xBBBBu, values[k] + 1);

		compare("setters", (long)k);
	}

	/*
	 * No datapump: both must leave the object alone rather than
	 * dereferencing null.
	 */
	reset_objects(11, 22);
	harness_param_reset();
	harness_param_set(MDMPRM_DP_ADDR, 0);
	ref_SetPulseMakeTime((void *)0xAAAAu, 99);
	ref_SetPulseBreakTime((void *)0xAAAAu, 99);
	SetPulseMakeTime((void *)0xBBBBu, 99);
	SetPulseBreakTime((void *)0xBBBBu, 99);
	compare("no datapump", 0);
	diff_eq_int("untouched with no datapump", obj_b.pulse_make, 11, 0);

	return diff_end();
}

static int
run_last_digit(void)
{
	diff_begin("pulse: LastPulseDigitDialed");

	harness_param_reset();
	harness_modem_reset(0, 0);

	ref_LastPulseDigitDialed((void *)0xAAAAu);
	LastPulseDigitDialed((void *)0xBBBBu);

	diff_eq_int("one parameter set per side", harness_modem_ours.nparams,
		    harness_modem_ref.nparams, 0);
	diff_eq_int("exactly one", harness_modem_ref.nparams, 1, 0);
	diff_eq_int("it is MDMPRM_PULSE_DIAL",
		    (int)harness_modem_ref.param_name[0], MDMPRM_PULSE_DIAL, 0);
	diff_eq_int("set to zero", harness_modem_ref.param_value[0], 0, 0);
	diff_eq_int("same name", (int)harness_modem_ours.param_name[0],
		    (int)harness_modem_ref.param_name[0], 0);
	diff_eq_int("same value", harness_modem_ours.param_value[0],
		    harness_modem_ref.param_value[0], 0);

	/* It never reads the datapump, so it works with none. */
	harness_param_reset();
	harness_modem_reset(0, 0);
	harness_param_set(MDMPRM_DP_ADDR, 0);
	ref_LastPulseDigitDialed((void *)0xAAAAu);
	LastPulseDigitDialed((void *)0xBBBBu);
	diff_eq_int("works without a datapump", harness_modem_ours.nparams,
		    harness_modem_ref.nparams, 0);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * A whole digit, tick by tick.
 */
static int
hook_changes_a, hook_changes_b;

static int
run_digit(const char *label, int digit, int make, int brk, int ticks)
{
	int last_a = 0, last_b = 0;
	int i;

	diff_begin(label);

	reset_objects(make, brk);
	hook_changes_a = hook_changes_b = 0;

	harness_param_reset();
	point_at(&obj_a);
	ref_PulseDialDigit((void *)0xAAAAu, digit);
	harness_param_reset();
	point_at(&obj_b);
	PulseDialDigit((void *)0xBBBBu, digit);
	compare("after load", -1);
	diff_eq_int("loaded the count", obj_b.pulse_remaining,
		    obj_a.pulse_remaining, -1);

	for (i = 0; i < ticks; i++) {
		int ra, rb;

		harness_param_reset();
		point_at(&obj_a);
		ra = ref_IsPulseDialerReady((void *)0xAAAAu);

		harness_param_reset();
		point_at(&obj_b);
		rb = IsPulseDialerReady((void *)0xBBBBu);

		diff_eq_int("tick %ld: ready", rb, ra, i);
		compare("tick", i);

		if (obj_a.pulse_off_hook != last_a) {
			hook_changes_a++;
			last_a = obj_a.pulse_off_hook;
		}
		if (obj_b.pulse_off_hook != last_b) {
			hook_changes_b++;
			last_b = obj_b.pulse_off_hook;
		}
	}

	diff_eq_int("hook transitions match", hook_changes_b, hook_changes_a,
		    0);
	/*
	 * Anti-vacuity: the line must actually have been interrupted.  A
	 * reconstruction that never touched the hook would agree with itself
	 * on every field above and send no pulses at all.
	 *
	 * Except with a break time of zero, where `elapsed < break` is never
	 * true and the line is correctly never interrupted -- a configuration
	 * that dials nothing.  Asserted in that direction instead, since it is
	 * as much a property of the original as the pulsing is.
	 */
	if (brk > 0)
		diff_eq_int("the line was interrupted (%ld transitions)",
			    hook_changes_a > 0, 1, hook_changes_a);
	else
		diff_eq_int("zero break sends no pulses (%ld transitions)",
			    hook_changes_a, 0, hook_changes_a);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * The diagnostic paths.
 *
 * All eight call sites here are gated on `dsplibs_debug_level > 1`, which
 * ships at zero, so everything above passes whether they are present, absent,
 * or present with the wrong text and the wrong arguments.  Raising the level
 * on both sides at once and comparing the two transcripts is the only thing
 * that reads them (findings F134, F147).
 *
 * A digit is used rather than a single tick because the two hook messages sit
 * on opposite sides of `modem_set_param` -- "hook on" before it, "hook off"
 * after -- and only a run that actually pulses reaches either.  Ticks are kept
 * modest: the capture buffer is 16 KB and the entry message alone is ~42 bytes
 * a tick.
 */
static int
run_transcript(const char *label, int digit, int make, int brk, int ticks,
	       unsigned level)
{
	char name[96];
	int i;

	snprintf(name, sizeof(name), "%s, level %u", label, level);
	diff_begin(name);

	reset_objects(make, brk);
	dsplibs_debug_level = level;
	ref_dsplibs_debug_level = level;
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	harness_param_reset();
	point_at(&obj_a);
	ref_SetPulseMakeTime((void *)0xAAAAu, make);
	ref_SetPulseBreakTime((void *)0xAAAAu, brk);
	ref_PulseDialDigit((void *)0xAAAAu, digit);

	harness_param_reset();
	point_at(&obj_b);
	SetPulseMakeTime((void *)0xBBBBu, make);
	SetPulseBreakTime((void *)0xBBBBu, brk);
	PulseDialDigit((void *)0xBBBBu, digit);

	for (i = 0; i < ticks; i++) {
		harness_param_reset();
		point_at(&obj_a);
		(void)ref_IsPulseDialerReady((void *)0xAAAAu);

		harness_param_reset();
		point_at(&obj_b);
		(void)IsPulseDialerReady((void *)0xBBBBu);
	}

	harness_param_reset();
	ref_LastPulseDigitDialed((void *)0xAAAAu);
	LastPulseDigitDialed((void *)0xBBBBu);

	diff_eq_int("transcript matches",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, (long)level);
	/* The two sides must also have printed the same NUMBER of lines. */
	diff_eq_int("line counts match", (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), (long)level);

	if (level > 1) {
		/*
		 * Anti-vacuity.  Two empty transcripts compare equal, and now
		 * that the harness marks its own callbacks the buffer is never
		 * empty anyway -- so this counts printed lines, not bytes
		 * (finding F149).  The reference side is the one measured, so
		 * it asserts what the original says, not what we say it says.
		 */
		diff_eq_int("reference printed something",
			    dsplib_debug_capture_lines(1) > 0, 1, (long)level);
		if (brk > 0) {
			diff_eq_int("reference said 'hook on'",
				    strstr(dsplib_debug_capture_text(1),
					   ": hook on...") != 0, 1, (long)level);
			diff_eq_int("reference said 'hook off'",
				    strstr(dsplib_debug_capture_text(1),
					   ": hook off...") != 0, 1,
				    (long)level);
		}
	} else {
		/*
		 * Every gate in the object here is `cmpl $0x1` + `ja`, so it
		 * fires at 2 and above and NOTHING should print at 1.  Worth
		 * asserting because a site spelled `> 0` rather than `> 1`
		 * produces a byte-identical transcript at level 2; level 1 is
		 * the only place the threshold itself is visible, and one
		 * shared DSPLIB_DEBUG_ON() macro cannot express a site that
		 * disagrees.  If this ever fails, the object has a gate we
		 * have flattened.
		 */
		diff_eq_int("reference silent below the threshold",
			    (int)dsplib_debug_capture_lines(1), 0, (long)level);
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_setters();
	rc |= run_last_digit();

	/*
	 * 67 ms break and 33 ms make is the usual 10 pulses per second at a
	 * 2:1 ratio; the tick is 5 ms, so a pulse is twenty ticks and a digit
	 * of 7 takes 140.
	 */
	rc |= run_digit("pulse: digit 7, 67/33", 7, 33, 67, 200);
	rc |= run_digit("pulse: digit 1, 67/33", 1, 33, 67, 60);
	rc |= run_digit("pulse: digit 0 means ten", 0, 33, 67, 300);
	rc |= run_digit("pulse: digit 9, 40/60", 9, 60, 40, 300);
	rc |= run_digit("pulse: make and break equal", 5, 50, 50, 200);
	/*
	 * Degenerate timings.  A break of zero means the transition test can
	 * never fire on the first tick, which is the sort of edge where two
	 * implementations part company.
	 */
	rc |= run_digit("pulse: zero break", 3, 33, 0, 60);
	rc |= run_digit("pulse: zero make", 3, 0, 67, 120);
	rc |= run_digit("pulse: both zero", 3, 0, 0, 40);
	rc |= run_digit("pulse: one-tick times", 2, 5, 5, 40);

	/*
	 * Level 1 is below every gate, 2 is the first that fires, 3 is above
	 * them all -- and all three must agree side for side.
	 */
	{
		unsigned lvl;

		for (lvl = 1; lvl <= 3; lvl++) {
			rc |= run_transcript("pulse: transcript",
					     3, 33, 67, 100, lvl);
			rc |= run_transcript("pulse: transcript, zero break",
					     2, 33, 0, 20, lvl);
		}
	}

	/* Polling with nothing loaded, and with no datapump at all. */
	diff_begin("pulse: idle and detached");
	{
		int i;

		reset_objects(33, 67);
		for (i = 0; i < 4; i++) {
			int ra, rb;

			harness_param_reset();
			point_at(&obj_a);
			ra = ref_IsPulseDialerReady((void *)0xAAAAu);
			harness_param_reset();
			point_at(&obj_b);
			rb = IsPulseDialerReady((void *)0xBBBBu);
			diff_eq_int("idle: ready", rb, ra, i);
			diff_eq_int("idle: reports ready", ra, 1, i);
			compare("idle", i);
		}

		harness_param_reset();
		harness_param_set(MDMPRM_DP_ADDR, 0);
		diff_eq_int("detached: ref reports ready",
			    ref_IsPulseDialerReady((void *)0xAAAAu), 1, 0);
		diff_eq_int("detached: we report ready",
			    IsPulseDialerReady((void *)0xBBBBu), 1, 0);
		harness_param_reset();
		harness_param_set(MDMPRM_DP_ADDR, 0);
		ref_PulseDialDigit((void *)0xAAAAu, 5);
		PulseDialDigit((void *)0xBBBBu, 5);
		compare("detached load", 0);
	}
	rc |= diff_end();

	return rc;
}
