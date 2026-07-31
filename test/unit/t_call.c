/*
 * t_call.c -- differential test of the call-setup datapump.
 *
 * All four functions in call.c are file statics, so `objcopy` cannot give
 * them `ref_` aliases and there is no way to call them by name.  There is
 * one way in: `dp_call_init` is global and registers the operations table,
 * so calling it and taking the three pointers out of the harness's
 * registration log reaches every one of them.  That is what `ops_of` does.
 *
 * S56 matters more here than anywhere else: through `CALLPROG_Create`'s `w0`
 * it decides which states listen to which detector, and with 0, 1 or 3 *no*
 * state listens for dial tone -- the machine blind-dials after a fixed wait.
 * Both halves of that split are covered below, because only the dial-tone
 * half ever puts samples on the line.
 *
 * As with the dialler and the supervisor, the two sides cannot run
 * interleaved: `CALLPROG_Dial` reaches the pulse dialler through the single
 * `MDMPRM_DP_ADDR` slot.  Each side is driven to completion with the store
 * pointed at its own call object.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/call.h"
#include "dsplib/dp.h"
#include "dsplib/callprog.h"
#include "dsplib/modem_params.h"
#include "dsplib/pulse.h"

extern void ref_dp_call_init(void);

#define MAXCALLS	120
#define MAXSAMP		200

static int status_seen[16];
static long total_nonzero;

/*
 * S56 decides whether the supervisor waits for dial tone, and through
 * CALLPROG_Create's `w0` it decides which states listen to which detector.
 * 0, 1 and 3 mean one thing; everything else means the other.
 */
static int sreg56;

struct side {
	struct dp_operations	*op;
	struct dp		*dp;
	struct call	call;
	short		out[MAXCALLS][MAXSAMP];
	int		rc[MAXCALLS];
	int		message[MAXCALLS];
	int		calls;
};

static struct side side_a, side_b;
static short input[MAXCALLS][MAXSAMP];

/* 550 Hz at amplitude 5000, the level and frequency the detectors respond to. */
static const short cycle[64] = {
	     0,    490,    975,   1451,   1913,   2357,   2778,   3172,
	  3536,   3865,   4157,   4410,   4619,   4785,   4904,   4976,
	  5000,   4976,   4904,   4785,   4619,   4410,   4157,   3865,
	  3536,   3172,   2778,   2357,   1913,   1451,    975,    490,
	     0,   -490,   -975,  -1451,  -1913,  -2357,  -2778,  -3172,
	 -3536,  -3865,  -4157,  -4410,  -4619,  -4785,  -4904,  -4976,
	 -5000,  -4976,  -4904,  -4785,  -4619,  -4410,  -4157,  -3865,
	 -3536,  -3172,  -2778,  -2357,  -1913,  -1451,   -975,   -490
};

#define TONE_STEP	9011		/* 550 * 64 * 2048 / 8000 */

#define SIG_SILENCE	0
#define SIG_TONE	1
#define SIG_NOISE	2

static void
fill_input(int signal, int samples)
{
	unsigned long r = 7717;
	long n = 0;
	int b, i;

	for (b = 0; b < MAXCALLS; b++) {
		for (i = 0; i < samples; i++, n++) {
			switch (signal) {
			case SIG_TONE:
				input[b][i] = cycle[(int)(((n * TONE_STEP)
							  >> 11) & 63)];
				break;
			case SIG_NOISE:
				r = r * 1103515245u + 12345u;
				input[b][i] = (short)(((r >> 16) & 0x1fff)
						      - 0x1000);
				break;
			default:
				input[b][i] = 0;
				break;
			}
		}
	}
}

static void
params(const char *dialstr)
{
	harness_param_reset();
	harness_sreg_reset();

	harness_param_set(MDMPRM_DIALSTR, (long)(intptr_t)dialstr);
	harness_param_set(GetPulseDialDigitPattern, 1);
	harness_param_set(GetPulseDialingFlag, 1);
	harness_param_set(GetDialModifierValidation, 1);
	harness_param_set(GetDialPauseTime, 2);
	harness_param_set(GetComaPauseDurationLimit, 10);
	harness_param_set(GetPulseDialMakeTime, 33);
	harness_param_set(GetPulseDialBreakTime, 67);
	harness_param_set(GetPulseBetweenDigitsInterval, 8);
	harness_param_set(GetDTMFDialSpeed, 70);
	harness_param_set(GetDTMFHighToneLevel, 9);
	harness_param_set(GetDTMFHighAndLowToneLevelDifference, 2);
	harness_param_set(GetCallingToneFlag, 1);
	harness_param_set(GetHookFlashTime, 50);

	harness_param_set(GetCallProgressSamplesBufferLength, 160);
	harness_param_set(GetMinBusyCadenceOnTime, 4);
	harness_param_set(GetMaxBusyCadenceOnTime, 12);
	harness_param_set(GetMinBusyCadenceOffTime, 4);
	harness_param_set(GetMaxBusyCadenceOffTime, 12);
	harness_param_set(GetBusyDetectionCyclesNumber, 3);
	harness_param_set(GetBusyToneCallProgressFilterIndex, 0);
	harness_param_set(GetBusyToneDiffTime, 3);
	harness_param_set(GetDialToneCallProgressFilterIndex, 0);
	/* In dB; below about 30 the threshold wraps out of reach. */
	harness_param_set(GetDialToneDetectionThreshold, 40);
	harness_param_set(GetDialToneValidationTime, 10);
	harness_param_set(GetDialToneWaitTime, 100);

	harness_sreg_set(16, 1);	/* tone dialling when no prefix */
	harness_sreg_set(56, sreg56);
}

/*
 * Reach the file statics: dp_call_init registers the operations table, so
 * calling it hands over pointers to all four.
 */
static struct dp_operations *
ops_of(int ref)
{
	struct reg_log *log = ref ? &harness_reg_ref : &harness_reg_ours;
	int i;

	harness_reg_reset();
	if (ref)
		ref_dp_call_init();
	else
		dp_call_init();

	for (i = 0; i < log->count; i++)
		if (log->id[i] == DP_CALL)
			return log->ops[i];
	return 0;
}

static void
drive(struct side *s, int ref, const char *dialstr, int srate, int samples,
      int calls)
{
	int n;

	memset(s->out, 0, sizeof(s->out));
	memset(s->rc, 0, sizeof(s->rc));
	memset(&s->call, 0, sizeof(s->call));
	s->call.self = &s->call;
	s->call.modem = (void *)0xD1A1u;
	s->calls = 0;

	s->op = ops_of(ref);
	if (s->op == 0)
		return;

	params(dialstr);
	harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&s->call);

	s->dp = s->op->create((void *)0xD1A1u, DP_CALL, 1, srate, 160, s->op);
	if (s->dp == 0)
		return;

	for (n = 0; n < calls; n++) {
		s->rc[n] = s->op->process(s->dp, input[n], s->out[n], samples);
		s->message[n] = ((struct call_dp *)s->dp)->message;
		s->calls = n + 1;
	}
}

static int
run(const char *label, const char *dialstr, int signal, int srate, int samples,
    int calls)
{
	int n, i;

	diff_begin(label);
	fill_input(signal, samples);

	drive(&side_a, 1, dialstr, srate, samples, calls);
	drive(&side_b, 0, dialstr, srate, samples, calls);

	diff_eq_int("both registered", side_b.op != 0 && side_a.op != 0, 1, 0);
	diff_eq_int("both created", side_b.dp != 0 && side_a.dp != 0, 1, 0);
	diff_eq_int("same number of calls", side_b.calls, side_a.calls, 0);

	for (n = 0; n < side_a.calls && n < side_b.calls; n++) {
		diff_eq_int("call %ld: status", side_b.rc[n], side_a.rc[n], n);
		diff_eq_int("call %ld: message", side_b.message[n],
			    side_a.message[n], n);
		for (i = 0; i < samples; i++) {
			diff_eq_int("call %ld: sample", side_b.out[n][i],
				    side_a.out[n][i], n);
			if (side_a.out[n][i] != 0)
				total_nonzero++;
		}
		if (side_a.rc[n] >= 0 && side_a.rc[n] < 16)
			status_seen[side_a.rc[n]]++;
	}

	/* The object itself, minus the pointers, which differ by construction. */
	if (side_a.dp != 0 && side_b.dp != 0) {
		struct call_dp *a = (struct call_dp *)side_a.dp;
		struct call_dp *b = (struct call_dp *)side_b.dp;

		diff_eq_int("id", b->id, a->id, 0);
		diff_eq_int("answered", b->answered, a->answered, 0);
		diff_eq_int("message", b->message, a->message, 0);
		diff_eq_int("pulse_make", b->pulse_make, a->pulse_make, 0);
		diff_eq_int("pulse_break", b->pulse_break, a->pulse_break, 0);
		diff_eq_int("in_q.count", b->in_q.count, a->in_q.count, 0);
		diff_eq_int("in_q.head", b->in_q.head, a->in_q.head, 0);
		diff_eq_int("in_q.active", b->in_q.active, a->in_q.active, 0);
		diff_eq_int("out_q.count", b->out_q.count, a->out_q.count, 0);
		diff_eq_int("out_q.tail", b->out_q.tail, a->out_q.tail, 0);
		diff_eq_int("out_q.active", b->out_q.active, a->out_q.active,
			    0);
		diff_eq_int("callprog state", b->callprog.state,
			    a->callprog.state, 0);
		diff_eq_int("callprog countdown", b->callprog.countdown,
			    a->callprog.countdown, 0);
		diff_eq_int("rc_in present", b->rc_in != 0, a->rc_in != 0, 0);
		diff_eq_int("rc_out present", b->rc_out != 0, a->rc_out != 0,
			    0);
		diff_eq_int("ring data", memcmp(a->in_q.data, b->in_q.data,
						sizeof(a->in_q.data)) == 0, 1,
			    0);
		diff_eq_int("out ring data", memcmp(a->out_q.data,
						    b->out_q.data,
						    sizeof(a->out_q.data)) == 0,
			    1, 0);
	}

	if (side_a.dp != 0)
		side_a.op->destroy(side_a.dp);
	if (side_b.dp != 0)
		side_b.op->destroy(side_b.dp);
	side_a.dp = side_b.dp = 0;

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i, distinct;

	/*
	 * The native rate, in every fragment size that matters: one that
	 * divides the 48-sample block, one that does not, one smaller than a
	 * block and one larger than the ring.
	 */
	rc |= run("call: 8k, 160 samples, silence", "5551234", SIG_SILENCE,
		  8000, 160, 60);
	rc |= run("call: 8k, 48 samples", "5551234", SIG_SILENCE, 8000, 48, 90);
	rc |= run("call: 8k, 47 samples", "5551234", SIG_SILENCE, 8000, 47, 90);
	rc |= run("call: 8k, 1 sample", "5551234", SIG_SILENCE, 8000, 1, 100);
	rc |= run("call: 8k, 200 samples", "5551234", SIG_SILENCE, 8000, 200,
		  40);

	/* With something on the line, so the supervisor has work to do. */
	rc |= run("call: 8k, tone", "5551234", SIG_TONE, 8000, 160, 80);

	/*
	 * Both halves of the S56 split.  With 0, 1 or 3 the machine starts in
	 * WAIT_DIAL and *no* state listens for dial tone -- it is blind
	 * dialling, and only the state timeout moves it on.  With 2 or 4 it
	 * starts in DIALING and dial tone is listened for in states 1 and 2.
	 * The long run is there to outlast the timeout, which is seconds.
	 */
	for (i = 0; i <= 4; i++) {
		char lb[64];

		sreg56 = i;
		sprintf(lb, "call: S56=%d, tone", i);
		rc |= run(lb, "5551234", SIG_TONE, 8000, 160, 80);
	}
	sreg56 = 0;
	rc |= run("call: blind dial outlasts its timeout", "5551234",
		  SIG_SILENCE, 8000, 160, 120);
	sreg56 = 2;
	rc |= run("call: dial-tone detection, long run", "5551234", SIG_TONE,
		  8000, 160, 120);
	sreg56 = 0;
	rc |= run("call: 8k, noise", "5551234", SIG_NOISE, 8000, 160, 60);

	/* The two rates that get resamplers, and one that does not. */
	rc |= run("call: 9600, resampled", "5551234", SIG_TONE, 9600, 160, 60);
	rc |= run("call: 48000, resampled", "5551234", SIG_TONE, 48000, 160,
		  60);
	rc |= run("call: 11025, no converter", "5551234", SIG_TONE, 11025, 160,
		  40);

	/* Dial strings: prefixed, unprefixed, modifiers, and one too long. */
	rc |= run("call: explicit tone prefix", "T5551234", SIG_SILENCE, 8000,
		  160, 60);
	rc |= run("call: explicit pulse prefix", "P5551234", SIG_SILENCE, 8000,
		  160, 90);
	rc |= run("call: modifiers", "5W55@5", SIG_SILENCE, 8000, 160, 60);
	rc |= run("call: too long to prefix",
		  "5555555555555555555555555555555555555555555555555555555555"
		  "5555555", SIG_SILENCE, 8000, 160, 40);
	rc |= run("call: empty dial string", "", SIG_SILENCE, 8000, 160, 20);

	diff_begin("guards");
	{
		distinct = 0;
		for (i = 0; i < 16; i++)
			if (status_seen[i] != 0)
				distinct++;
		diff_eq_int("more than one status was returned (%ld)",
			    distinct >= 2, 1, distinct);
		diff_eq_int("DPSTAT_OK was returned (%ld)",
			    status_seen[DPSTAT_OK] > 0, 1,
			    status_seen[DPSTAT_OK]);
		/*
		 * The outgoing side only carries samples once the supervisor
		 * reaches DIALING, which needs S56 to be 2 or 4 -- with 0, 1
		 * or 3 no state listens for dial tone at all and the machine
		 * waits out its timeout in silence.  So this guard is really
		 * a guard on the S56 cases above being present.
		 */
		diff_eq_int("the dialler reached the line (%ld samples)",
			    total_nonzero > 1000, 1, total_nonzero);
		diff_eq_int("several messages were seen (%ld)",
			    side_a.message[side_a.calls - 1]
			    != CALLPROG_MAX_MESSAGES, 1,
			    side_a.message[side_a.calls - 1]);
	}
	rc |= diff_end();

	return rc;
}
