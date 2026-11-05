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
		ref_SetPulseBreakTime((void *)0xAAAAu, (int)((unsigned int)values[k] + 1u));

		harness_param_reset();
		point_at(&obj_b);
		SetPulseMakeTime((void *)0xBBBBu, values[k]);
		SetPulseBreakTime((void *)0xBBBBu, (int)((unsigned int)values[k] + 1u));

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


/*
 * Public pulse-component histories: the zeroed object below models only the
 * pulse component's initialized storage, not a full call_create lifecycle.
 * Normal 33/67 durations come from u8 country-table fields. Negative/high-bit
 * setter inputs are explicitly exploratory API-domain probes, not normal
 * modem configuration. No elapsed/count/phase field is planted after load.
 *
 * The import wrappers observe real parameter calls. In explicitly synthetic
 * host-reentry groups they reenter the same side through PulseDialDigit once.
 * No production host contract establishing such reentry has been found; these
 * groups measure native value age across an external-call boundary only.
 */
#define PULSE_EVENTS 64
struct pulse_event {
	unsigned name;
	int value, modem_matches;
	struct call state;
};
static struct pulse_event pulse_events[2][PULSE_EVENTS];
static int pulse_nevents[2], pulse_overflow[2], pulse_callbacks[2];
static int pulse_hook_starts[2], pulse_probe_active, pulse_action;
static int pulse_armed[2];

static void
pulse_normalise(struct call *dst, const struct call *src)
{
	*dst = *src;
	dst->self = 0;
	dst->modem = 0;
}

static void
pulse_compare_object(long tick)
{
	struct call a, b;
	diff_eq_int("reference self retained", obj_a.self == &obj_a, 1, tick);
	diff_eq_int("source self retained", obj_b.self == &obj_b, 1, tick);
	diff_eq_int("reference modem retained", obj_a.modem == (void *)0xAAAAu,
		    1, tick);
	diff_eq_int("source modem retained", obj_b.modem == (void *)0xBBBBu,
		    1, tick);
	pulse_normalise(&a, &obj_a);
	pulse_normalise(&b, &obj_b);
	diff_eq_obj("complete pulse component", struct call, &b, &a, tick);
}

static void
pulse_observe(int side, void *modem, unsigned name, int value)
{
	struct call *c = side ? &obj_a : &obj_b;
	struct pulse_event *e;
	int fire;
	if (!pulse_probe_active)
		return;
	if (pulse_nevents[side] == PULSE_EVENTS) {
		pulse_overflow[side]++;
		return;
	}
	e = &pulse_events[side][pulse_nevents[side]++];
	memset(e, 0, sizeof *e);
	e->name = name;
	e->value = value;
	e->modem_matches = modem == c->modem;
	pulse_normalise(&e->state, c);
	if (name == MDMPRM_HOOK_ON && value == 1)
		pulse_hook_starts[side]++;
	fire = pulse_armed[side] && pulse_callbacks[side] == 0 &&
		((pulse_action == 1 && name == MDMPRM_HOOK_ON && value == 0) ||
		 (pulse_action == 2 && name == MDMPRM_HOOK_ON && value == 1 &&
		  pulse_hook_starts[side] == 2) ||
		 (pulse_action == 3 && name == MDMPRM_PULSE_DIAL));
	if (fire) {
		pulse_callbacks[side]++;
		if (side)
			ref_PulseDialDigit(modem, 4);
		else
			PulseDialDigit(modem, 4);
	}
}

extern long __real_modem_set_param(void *, unsigned, int);
extern long __real_ref_modem_set_param(void *, unsigned, int);
long
__wrap_modem_set_param(void *modem, unsigned name, int value)
{
	long result = __real_modem_set_param(modem, name, value);
	pulse_observe(0, modem, name, value);
	return result;
}
long
__wrap_ref_modem_set_param(void *modem, unsigned name, int value)
{
	long result = __real_ref_modem_set_param(modem, name, value);
	pulse_observe(1, modem, name, value);
	return result;
}

static int
run_pulse_component(const char *label, int make, int brk, int ticks,
		    int action, unsigned level)
{
	int tick, side, i, expected_callbacks = action == 1 || action == 2;
	diff_begin(label);
	reset_objects(0, 0);
	memset(pulse_events, 0, sizeof pulse_events);
	memset(pulse_nevents, 0, sizeof pulse_nevents);
	memset(pulse_overflow, 0, sizeof pulse_overflow);
	memset(pulse_callbacks, 0, sizeof pulse_callbacks);
	memset(pulse_hook_starts, 0, sizeof pulse_hook_starts);
	memset(pulse_armed, 0, sizeof pulse_armed);
	pulse_action = action;
	pulse_probe_active = 1;
	harness_modem_reset(0, 0);
	dsplibs_debug_level = ref_dsplibs_debug_level = level;
	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;
	for (side = 0; side < 2; side++) {
		struct call *c = side ? &obj_a : &obj_b;
		harness_param_reset();
		point_at(c);
		if (side) {
			ref_SetPulseMakeTime(c->modem, make);
			ref_SetPulseBreakTime(c->modem, brk);
			ref_PulseDialDigit(c->modem, 2);
		} else {
			SetPulseMakeTime(c->modem, make);
			SetPulseBreakTime(c->modem, brk);
			PulseDialDigit(c->modem, 2);
		}
		pulse_armed[side] = 1;
	}
	pulse_compare_object(-1);
	for (tick = 0; tick < ticks; tick++) {
		int ready[2];
		for (side = 0; side < 2; side++) {
			struct call *c = side ? &obj_a : &obj_b;
			harness_param_reset();
			point_at(c);
			ready[side] = side ? ref_IsPulseDialerReady(c->modem) :
					    IsPulseDialerReady(c->modem);
		}
		diff_eq_int("ready at public poll", ready[0], ready[1], tick);
		pulse_compare_object(tick);
		/* Compare the cumulative side-effect prefix at every API boundary. */
		diff_eq_int("parameter event count", pulse_nevents[0],
			    pulse_nevents[1], tick);
		for (i = 0; i < pulse_nevents[0] && i < pulse_nevents[1]; i++)
			diff_eq_obj("parameter event and boundary state",
				    struct pulse_event, &pulse_events[0][i],
				    &pulse_events[1][i], tick);
	}
	for (side = 0; side < 2; side++) {
		diff_eq_int("bounded trace has no overflow", pulse_overflow[side],
			    0, side);
		diff_eq_int("host callback execution count", pulse_callbacks[side],
			    expected_callbacks, side);
		diff_eq_int("digit load reached host", pulse_nevents[side] > 0,
			    1, side);
		diff_eq_int("initial host parameter", pulse_events[side][0].name,
			    MDMPRM_PULSE_DIAL, side);
		diff_eq_int("initial host pulse count", pulse_events[side][0].value,
			    2, side);
	}
	diff_eq_int("complete ordered debug and host transcript",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, level);
	diff_eq_int("same debug line count", dsplib_debug_capture_lines(0),
		    dsplib_debug_capture_lines(1), level);
	diff_eq_int("debug threshold observed",
		    dsplib_debug_capture_lines(1) > 0, level > 1, level);
	pulse_probe_active = 0;
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
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


	/* Ordinary and disjoint callback controls, then synthetic host reentry.
	 * Action 3 is armed only after load and matches PULSE_DIAL, which an
	 * ordinary poll never sends; this checks selection without reentry. */
	rc |= run_pulse_component("pulse component: ordinary setter/load/poll",
				  33, 67, 50, 0, 2);
	rc |= run_pulse_component("pulse component: disjoint host control",
				  33, 67, 50, 3, 2);
	rc |= run_pulse_component("pulse exploratory: reload on restore, quiet",
				  33, 67, 50, 1, 0);
	rc |= run_pulse_component("pulse exploratory: reload on restore, debug",
				  33, 67, 50, 1, 2);
	rc |= run_pulse_component("pulse exploratory: reload on second break, quiet",
				  33, 67, 50, 2, 0);
	rc |= run_pulse_component("pulse exploratory: reload on second break, debug",
				  33, 67, 50, 2, 2);
	/* All values enter through setters. These are non-country API probes. */
	rc |= run_pulse_component("pulse exploratory: unsigned break duration",
				  33, -1, 8, 0, 0);
	rc |= run_pulse_component("pulse exploratory: unsigned make duration",
				  (int)0x80000000u, 0, 8, 0, 0);
	rc |= run_pulse_component("pulse exploratory: unsigned sum boundary",
				  0x7fffffff, 1, 8, 0, 0);
	rc |= run_pulse_component("pulse exploratory: modular sum wraps",
				  -1, 1, 8, 0, 0);

	return rc;
}
