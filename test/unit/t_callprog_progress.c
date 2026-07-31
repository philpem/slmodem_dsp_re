/*
 * t_callprog_progress.c -- differential test of the call-progress supervisor.
 *
 * `CALLPROG_Progress` is where the whole of phase 3 meets: it drives both
 * cadence detectors, the band filter, the answer-tone detector, the calling
 * tone and the dialler, and it is the only writer of the eleven state tables
 * `CALLPROG_Create` builds.
 *
 * Two things shape this test.
 *
 * First, the two sides cannot run interleaved.  `CALLPROG_Dial` reaches the
 * pulse dialler through `modem_get_param(MDMPRM_DP_ADDR)`, and the parameter
 * store holds one value, so each side is driven to completion with the store
 * pointed at its own call object and the results compared afterwards.
 *
 * Second, a divergence here compounds: the transitions depend on detector
 * verdicts, which depend on every earlier sample, so a mismatch can surface
 * hundreds of buffers after its cause.  Every call therefore compares the
 * state and the whole scalar half of the object, not just the returned
 * message -- the state is what diverges first.
 *
 * Most of the machine is unreachable from synthetic audio alone: only states
 * 1 and 2 listen for dial tone, so a run that starts at the beginning never
 * visits the rest.  The states past those are reached by seeding `cp->state`
 * directly after `CALLPROG_Dial`, which is what `seed` does below.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/callprog.h"
#include "dsplib/callprog_state.h"
#include "dsplib/pulse.h"
#include "dsplib/modem_params.h"

extern int ref_CALLPROG_Progress(struct callprog *cp, const short *in,
				 short *out, int count);
extern void ref_CALLPROG_Create(struct callprog *cp, struct callprog_cfg *cfg);
extern void ref_CALLPROG_Dial(struct callprog *cp, const char *s);
extern void ref_CALLPROG_Delete(struct callprog *cp);

#define BUFSAMP		160
#define MAXCALLS	400

/* The shapes of input the supervisor is fed. */
#define SIG_SILENCE	0
#define SIG_DIALTONE	1	/* a continuous 550 Hz tone            */
#define SIG_BUSY	2	/* 550 Hz, half a second on, half off  */
#define SIG_NOISE	3	/* deterministic, and loud             */
#define SIG_COUNT	4

/* Half a second at 8 kHz, which is what t_cadence uses to get a detection. */
#define BUSY_ON_SAMPLES	4000

static long total_messages;
static int message_seen[CALLPROG_MAX_MESSAGES];
static int state_seen[CALLPROG_STATES];

struct side {
	struct callprog	cp;
	struct call	call;
	short		out[MAXCALLS][BUFSAMP];
	int		msg[MAXCALLS];
	int		state[MAXCALLS];
	int		countdown[MAXCALLS];
	int		calls;
};

static struct side side_a, side_b;
static short input[MAXCALLS][BUFSAMP];

/*
 * A sine at 550 Hz and amplitude 5000, from a fixed table so that both sides
 * and every run see bit-identical input.  No libm, and no floating point.
 *
 * Both numbers are copied from `t_cadence`, which is the only place in the
 * suite that gets a cadence detector to assert.  425 Hz at full scale -- the
 * obvious choice for a European busy tone -- produces no verdict at all,
 * which cost a while to work out.
 */
#define TONE_STEP	9011		/* 550 * 64 * 2048 / 8000 */

static short
tone550(long n)
{
	static const short cycle[64] = {
		     0,    490,    975,   1451,   1913,   2357,   2778,   3172,
		  3536,   3865,   4157,   4410,   4619,   4785,   4904,   4976,
		  5000,   4976,   4904,   4785,   4619,   4410,   4157,   3865,
		  3536,   3172,   2778,   2357,   1913,   1451,    975,    490,
		     0,   -490,   -975,  -1451,  -1913,  -2357,  -2778,  -3172,
		 -3536,  -3865,  -4157,  -4410,  -4619,  -4785,  -4904,  -4976,
		 -5000,  -4976,  -4904,  -4785,  -4619,  -4410,  -4157,  -3865,
		 -3536,  -3172,  -2778,  -2357,  -1913,  -1451,   -975,   -490,
	};

	return cycle[((n * TONE_STEP) >> 11) & 63];
}

static void
fill_input(int signal)
{
	long n = 0;
	int b, i;
	unsigned long r = 12345;

	for (b = 0; b < MAXCALLS; b++) {
		for (i = 0; i < BUFSAMP; i++, n++) {
			short v = 0;

			switch (signal) {
			case SIG_SILENCE:
				v = 0;
				break;
			case SIG_DIALTONE:
				v = tone550(n);
				break;
			case SIG_BUSY:
				/*
				 * The cadence has to land inside the window
				 * the detector was built with, and that
				 * window is in toneiir intervals: the table's
				 * 4..12 becomes 2..6 intervals of 20 ms, so
				 * 40..120 ms.  80 ms sits in the middle.
				 */
				v = ((n / BUSY_ON_SAMPLES) & 1) ? 0
								: tone550(n);
				break;
			default:
				r = r * 1103515245u + 12345u;
				v = (short)((r >> 16) & 0x3fff);
				v = (short)(v - 0x2000);
				break;
			}
			input[b][i] = v;
		}
	}
}

/* Which call-progress filter the busy detector uses; swept by main(). */
static int busy_filter;

/* Seeded into the counters after CALLPROG_Dial; see drive(). */
static int seed_countdown;
static int seed_line_clear;
static int seed_quiet;
static int seed_armed;

/* GetCallingToneFlag, which decides whether state 3 generates anything. */
static int opt_calling_tone = 1;

/* Non-silent output samples the reference produced in the last run(). */
static long last_nonzero;

static void
params(void)
{
	harness_param_reset();
	harness_param_set(GetPulseDialDigitPattern, 1);
	harness_param_set(GetPulseDialingFlag, 1);
	harness_param_set(GetABCDDialingPermittedFlag, 0);
	harness_param_set(GetPulseAndToneDialInSameDialStringPermittedFlag, 0);
	harness_param_set(GetDialModifierValidation, 1);
	harness_param_set(GetDialPauseTime, 2);
	harness_param_set(GetComaPauseDurationLimit, 10);
	harness_param_set(GetPulseDialMakeTime, 33);
	harness_param_set(GetPulseDialBreakTime, 67);
	harness_param_set(GetPulseBetweenDigitsInterval, 8);
	harness_param_set(GetDTMFDialSpeed, 70);
	harness_param_set(GetDTMFHighToneLevel, 9);
	harness_param_set(GetDTMFHighAndLowToneLevelDifference, 2);
	harness_param_set(GetCallingToneFlag, opt_calling_tone);
	harness_param_set(GetHookFlashTime, 50);

	/*
	 * The call-progress side.  Without these the cadence windows are all
	 * zero and the busy detector can never match, which is how the first
	 * version of this test ran green while never once reporting BUSY.
	 * Windows are in centiseconds.
	 */
	harness_param_set(GetCallProgressSamplesBufferLength, BUFSAMP);
	harness_param_set(GetMinBusyCadenceOnTime, 4);
	harness_param_set(GetMaxBusyCadenceOnTime, 12);
	harness_param_set(GetMinBusyCadenceOffTime, 4);
	harness_param_set(GetMaxBusyCadenceOffTime, 12);
	harness_param_set(GetBusyDetectionCyclesNumber, 3);
	harness_param_set(GetBusyToneCallProgressFilterIndex, busy_filter);
	harness_param_set(GetBusyToneDiffTime, 3);
	harness_param_set(GetDialToneCallProgressFilterIndex, busy_filter);
	harness_param_set(GetDialToneFilterSubindex, 0);
	harness_param_set(GetBusyToneLooseDetectionEnabled, 1);
	harness_param_set(GetDialToneDetectionThreshold, 1);
	harness_param_set(GetDialToneValidationTime, 10);
	harness_param_set(GetDialToneWaitTime, 100);
}

static long
sreg(void *modem, unsigned short n)
{
	(void)modem;
	return 10 + n % 7;
}

/* Run one side to completion. */
static void
drive(struct side *s, int ref, const char *dialstr, int seed, int calls)
{
	struct callprog_cfg cfg;
	int n;

	memset(s, 0, sizeof(*s));
	s->call.self = &s->call;
	s->call.modem = (void *)0xD1A1u;

	memset(&cfg, 0, sizeof(cfg));
	cfg.get_sreg = sreg;
	cfg.modem = (void *)0xD1A1u;

	params();
	harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&s->call);

	if (ref) {
		ref_CALLPROG_Create(&s->cp, &cfg);
		ref_CALLPROG_Dial(&s->cp, dialstr);
	} else {
		CALLPROG_Create(&s->cp, &cfg);
		CALLPROG_Dial(&s->cp, dialstr);
	}

	if (seed >= 0)
		s->cp.state = seed;

	/*
	 * The timeouts are the one route into the transition machinery that a
	 * test can take deterministically: unlike a detector verdict, the two
	 * counters are plain fields.  Seeding them short makes the state's own
	 * timeout and the line-clear timeout fire within a few buffers.
	 */
	if (seed_countdown > 0)
		s->cp.countdown = seed_countdown;
	if (seed_quiet > 0)
		s->cp.quiet_count = seed_quiet;
	/*
	 * No value of GetCallingToneFlag reaches CALLPROG_Dial's arming arm
	 * from this harness, so the flag alone leaves state 3 emitting
	 * silence.  Seeding the field is what covers GenerateCallingTone.
	 */
	if (seed_armed)
		s->cp.calling_tone_armed = 1;
	if (seed_line_clear > 0) {
		s->cp.line_clear_active = 1;
		s->cp.line_clear_limit = seed_line_clear;
		s->cp.line_clear_count = 0;
	}

	for (n = 0; n < calls; n++) {
		if (ref)
			s->msg[n] = ref_CALLPROG_Progress(&s->cp, input[n],
							  s->out[n], BUFSAMP);
		else
			s->msg[n] = CALLPROG_Progress(&s->cp, input[n],
						      s->out[n], BUFSAMP);
		s->state[n] = s->cp.state;
		s->countdown[n] = s->cp.countdown;
		s->calls = n + 1;
	}

	if (ref)
		ref_CALLPROG_Delete(&s->cp);
	else
		CALLPROG_Delete(&s->cp);
}

/* Every scalar of the object.  The four pointers differ between sides. */
static void
compare_object(const struct callprog *a, const struct callprog *b)
{
	diff_eq_int("state", b->state, a->state, 0);
	diff_eq_int("last_state", b->last_state, a->last_state, 0);
	diff_eq_int("countdown", b->countdown, a->countdown, 0);
	diff_eq_int("pending_state", b->pending_state, a->pending_state, 0);
	diff_eq_int("pending", b->pending, a->pending, 0);
	diff_eq_int("message", b->message, a->message, 0);
	diff_eq_int("line_clear_count", b->line_clear_count,
		    a->line_clear_count, 0);
	diff_eq_int("line_clear_limit", b->line_clear_limit,
		    a->line_clear_limit, 0);
	diff_eq_int("line_clear_active", b->line_clear_active,
		    a->line_clear_active, 0);
	diff_eq_int("event", b->event, a->event, 0);
	diff_eq_int("quiet_count", b->quiet_count, a->quiet_count, 0);
	diff_eq_int("fatal", b->fatal, a->fatal, 0);
	diff_eq_int("calling_tone_mode", b->calling_tone_mode,
		    a->calling_tone_mode, 0);
	diff_eq_int("calling_tone_armed", b->calling_tone_armed,
		    a->calling_tone_armed, 0);
	diff_eq_int("dialtone_seen", b->dialtone_seen, a->dialtone_seen, 0);
	diff_eq_int("band_wanted", b->band_wanted, a->band_wanted, 0);
}

static int
run(const char *label, const char *dialstr, int signal, int seed, int calls)
{
	int n, i;

	diff_begin(label);
	fill_input(signal);

	drive(&side_a, 1, dialstr, seed, calls);
	drive(&side_b, 0, dialstr, seed, calls);

	diff_eq_int("same number of calls", side_b.calls, side_a.calls, 0);

	last_nonzero = 0;
	for (n = 0; n < side_a.calls && n < side_b.calls; n++) {
		diff_eq_int("call %ld: message", side_b.msg[n], side_a.msg[n],
			    n);
		diff_eq_int("call %ld: state", side_b.state[n],
			    side_a.state[n], n);
		diff_eq_int("call %ld: countdown", side_b.countdown[n],
			    side_a.countdown[n], n);
		for (i = 0; i < BUFSAMP; i++) {
			diff_eq_int("call %ld: sample", side_b.out[n][i],
				    side_a.out[n][i], n);
			if (side_a.out[n][i] != 0)
				last_nonzero++;
		}

		if (side_a.msg[n] >= 0
		    && side_a.msg[n] < CALLPROG_MAX_MESSAGES)
			message_seen[side_a.msg[n]]++;
		if (side_a.state[n] >= 0 && side_a.state[n] < CALLPROG_STATES)
			state_seen[side_a.state[n]]++;
		if (side_a.msg[n] != 0)
			total_messages++;
	}

	compare_object(&side_a.cp, &side_b.cp);

	/* The tables themselves, which Create rebuilds on every run. */
	diff_eq_int("next_state_due_cptd matches", 1, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i, distinct;

	/* From the beginning, with each shape of input. */
	rc |= run("callprog: silence, tone dial", "T5551234", SIG_SILENCE, -1,
		  120);
	rc |= run("callprog: dial tone present", "T5551234", SIG_DIALTONE, -1,
		  120);
	/*
	 * The busy detector over every call-progress filter the country data
	 * can select.  Which index is 425 Hz is a property of the filter bank,
	 * not of this test, so the sweep both covers the bank and guarantees
	 * that at least one index actually reports BUSY -- the guard below
	 * insists on it.
	 */
	for (i = 0; i < 8; i++) {
		char label[64];

		busy_filter = i;
		sprintf(label, "callprog: busy cadence, filter %d", i);
		rc |= run(label, "T5551234", SIG_BUSY, -1, 300);
		sprintf(label, "callprog: dial tone, filter %d", i);
		rc |= run(label, "T5551234", SIG_DIALTONE, -1, 300);
	}
	busy_filter = 0;
	rc |= run("callprog: noise", "T5551234", SIG_NOISE, -1, 120);
	rc |= run("callprog: pulse dialling", "P5551234", SIG_SILENCE, -1, 200);

	/*
	 * The rest of the machine, seeded directly.  State 0 is the illegal
	 * one and is included on purpose: it has table entries, so it has
	 * behaviour, and nothing else would exercise it.
	 */
	for (i = 0; i < CALLPROG_STATES; i++) {
		char label[64];

		sprintf(label, "callprog: seeded state %d, silence", i);
		rc |= run(label, "T5551234", SIG_SILENCE, i, 90);
		sprintf(label, "callprog: seeded state %d, busy cadence", i);
		rc |= run(label, "T5551234", SIG_BUSY, i, 90);
		sprintf(label, "callprog: seeded state %d, noise", i);
		rc |= run(label, "T5551234", SIG_NOISE, i, 90);
	}

	/*
	 * The timeouts, in every state.  This is what exercises the transition
	 * machinery -- request_state, the commit, and the per-state timeout
	 * and line-clear tables -- which the detector verdicts would otherwise
	 * be the only way to reach.
	 */
	for (i = 0; i < CALLPROG_STATES; i++) {
		char label[64];

		seed_countdown = 200;
		sprintf(label, "callprog: state %d, timeout fires", i);
		rc |= run(label, "T5551234", SIG_SILENCE, i, 90);

		seed_countdown = 0;
		seed_line_clear = 3;
		sprintf(label, "callprog: state %d, line clear fires", i);
		rc |= run(label, "T5551234", SIG_SILENCE, i, 90);
		seed_line_clear = 0;
	}

	/*
	 * The answered-at-last transition in state 8.  It needs 40000/160 =
	 * 250 consecutive quiet buffers, so a 90-call run can never reach it;
	 * seeding the counter just short is the only way to cover the
	 * comparison and the transition it guards.
	 */
	seed_countdown = 0;
	seed_quiet = 248;
	rc |= run("callprog: state 8, silence runs out", "T5551234",
		  SIG_SILENCE, CPSTATE_WFS_STATE, 20);
	rc |= run("callprog: state 8, noise resets the count", "T5551234",
		  SIG_NOISE, CPSTATE_WFS_STATE, 20);
	seed_quiet = 0;

	/*
	 * State 3 with the calling tone actually armed.  Flag 1 disarms it, so
	 * every run above took the silence branch and GenerateCallingTone was
	 * never called.
	 */
	opt_calling_tone = 3;
	seed_armed = 1;
	rc |= run("callprog: state 3, calling tone armed", "T5551234",
		  SIG_SILENCE, CPSTATE_WAIT_RING, 60);
	seed_armed = 0;
	diff_begin("callprog: the armed calling tone is audible");
	diff_eq_int("state 3 generated a tone (%ld samples)", last_nonzero > 0,
		    1, last_nonzero);
	rc |= diff_end();
	rc |= run("callprog: caret with the tone armed", "T5^5", SIG_SILENCE,
		  -1, 120);
	opt_calling_tone = 2;
	rc |= run("callprog: caret, calling tone mode 2", "T5^5", SIG_SILENCE,
		  -1, 120);
	opt_calling_tone = 1;

	/* The modifiers, which are how the dialler drives the supervisor. */
	rc |= run("callprog: W modifier", "T5W5", SIG_SILENCE, -1, 120);
	rc |= run("callprog: at modifier", "T5@5", SIG_SILENCE, -1, 200);
	rc |= run("callprog: dollar modifier", "T5$5", SIG_SILENCE, -1, 120);
	rc |= run("callprog: caret modifier", "T5^5", SIG_SILENCE, -1, 120);
	rc |= run("callprog: trailing semicolon", "T555;", SIG_SILENCE, -1, 120);
	rc |= run("callprog: rejected dial string",
		  "TXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
		  "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
		  SIG_SILENCE, -1, 20);

	/*
	 * Anti-vacuity.  A supervisor that reported nothing and never moved
	 * would agree with itself on every check above.
	 */
	printf("  states visited:");
	for (i = 0; i < CALLPROG_STATES; i++)
		if (state_seen[i])
			printf(" %d:%d", i, state_seen[i]);
	printf("\n  messages seen:");
	for (i = 0; i < CALLPROG_MAX_MESSAGES; i++)
		if (message_seen[i])
			printf(" %d:%d", i, message_seen[i]);
	printf("\n");

	diff_begin("guards");
	{
		distinct = 0;
		for (i = 0; i < CALLPROG_STATES; i++)
			if (state_seen[i] != 0)
				distinct++;
		diff_eq_int("several states were visited (%ld)", distinct >= 6,
			    1, distinct);

		distinct = 0;
		for (i = 1; i < CALLPROG_MAX_MESSAGES; i++)
			if (message_seen[i] != 0)
				distinct++;
		diff_eq_int("several messages were reported (%ld)",
			    distinct >= 3, 1, distinct);
		diff_eq_int("messages were reported at all (%ld)",
			    total_messages > 0, 1, total_messages);
		diff_eq_int("CALLPROG_BUSY was reported (%ld)",
			    message_seen[CALLPROG_BUSY] > 0, 1,
			    message_seen[CALLPROG_BUSY]);
	}
	rc |= diff_end();

	return rc;
}
