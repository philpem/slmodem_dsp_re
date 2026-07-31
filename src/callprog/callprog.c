/*
 * callprog.c -- Call Progress: the supervisor.
 *
 * Reconstructed from dsplibs.o Callprog.c:
 *
 *   CALLPROG_Create   .text 0x079570
 *   CALLPROG_Delete   .text 0x079440
 *   CALLPROG_Dial     .text 0x07a5a0
 *
 * CALLPROG_Progress is not reconstructed yet.
 *
 * Most of Create is not object initialisation at all -- it is the state
 * machine, assembled a byte at a time into eleven module-scope tables.  The
 * contents are in docs/callprog_states.md, extracted by script rather than
 * transcribed, and this file is the same data in the order the original
 * writes it.
 */

#include "dsplib/callprog_state.h"
#include "dsplib/callprog.h"
#include "dsplib/callprog_cfg.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"


/*
 * ---------------------------------------------------------------------------
 * The state machine.  Zero-initialised by the loader and rebuilt by every
 * CALLPROG_Create -- so two supervisors share one machine, which is fine only
 * because the tables are constant and every Create writes the same values.
 */
unsigned char next_state_due_cptd[CALLPROG_STATES][CALLPROG_CPTD_EVENTS];
unsigned char message_due_cptd[CALLPROG_STATES][CALLPROG_CPTD_EVENTS];
unsigned char next_state_due_timeout[CALLPROG_STATES];
unsigned char message_due_timeout[CALLPROG_STATES];
unsigned char next_state_due_line_clear_timeout[CALLPROG_STATES];
unsigned char message_due_line_clear_timeout[CALLPROG_STATES];
int timeout_table[CALLPROG_STATES];
int enable_line_clear_timeout[CALLPROG_STATES];
unsigned char automode_table[CALLPROG_STATES];
unsigned char toneiir_dialtone_table[CALLPROG_STATES];
unsigned char toneiir_busy_table[CALLPROG_STATES];

/*
 * Default timeouts, in seconds, .rodata+0x5d68.  Copied into the object and
 * from there into `timeout_table`.
 */
static const int callprog_default_timeout[7] = { 10, 20, 15, 10, 8, 60, 60 };

/* The state the machine ends in, and the one it starts in. */
#define CALLPROG_STATE_END	6
#define CALLPROG_STATE_START	1

/*
 * The timeout half of the machine.  Built by Create and again by Dial, which
 * refreshes the durations from the host before every call -- so this is one
 * function here where the original has it twice, inlined.
 */
static void
build_timeouts(struct callprog *cp)
{
	int s;

	for (s = 0; s < CALLPROG_STATES; s++)
		next_state_due_timeout[s] = 0;
	for (s = 0; s < CALLPROG_STATES; s++)
		message_due_timeout[s] = 0;

	/*
	 * Six states can time out and all six end the call.  Read the messages
	 * down the column and they are the list of ways a call fails to
	 * connect.
	 */
	next_state_due_timeout[1] = CALLPROG_STATE_END;
	message_due_timeout[1] = CALLPROG_NO_DIAL_TONE;
	next_state_due_timeout[3] = CALLPROG_STATE_END;
	message_due_timeout[3] = CALLPROG_NO_RING;
	next_state_due_timeout[4] = CALLPROG_STATE_END;
	message_due_timeout[4] = CALLPROG_NO_ANSWER;
	next_state_due_timeout[5] = CALLPROG_STATE_END;
	message_due_timeout[5] = CALLPROG_ANSWER_STATE_TIMEOUT;
	next_state_due_timeout[8] = CALLPROG_STATE_END;
	message_due_timeout[8] = CALLPROG_BUSY;
	next_state_due_timeout[9] = CALLPROG_STATE_END;

	for (s = 0; s < CALLPROG_STATES; s++)
		timeout_table[s] = 0;

	timeout_table[1] = cp->timeout[0];
	timeout_table[3] = cp->timeout[1];
	timeout_table[4] = cp->timeout[2];
	timeout_table[5] = cp->timeout[3];
	timeout_table[8] = cp->timeout[5];
	timeout_table[9] = cp->timeout[6];
}

/*
 * Arm the state the machine is about to enter: load its timeout, and its
 * line-clear timeout if it has one.  Create and Dial both end this way.
 */
static void
enter_state(struct callprog *cp)
{
	cp->countdown = timeout_table[cp->state] * 8000;

	if (next_state_due_line_clear_timeout[cp->state] != 0)
		cp->line_clear_active = enable_line_clear_timeout[cp->state];

	if (cp->state == 8)
		cp->quiet_count = 0;
}

static void
build_state_machine(struct callprog *cp)
{
	int s;

	for (s = 0; s < CALLPROG_STATES; s++) {
		int e;

		for (e = 0; e < CALLPROG_CPTD_EVENTS; e++) {
			next_state_due_cptd[s][e] = 0;
			message_due_cptd[s][e] = 0;
		}
	}

	for (s = 0; s < CALLPROG_STATES; s++)
		next_state_due_line_clear_timeout[s] = 0;
	for (s = 0; s < CALLPROG_STATES; s++)
		message_due_line_clear_timeout[s] = 0;

	/*
	 * Busy, congestion and error end the call from wherever it is.  These
	 * three are the reason the machine is tractable: every state has the
	 * same exit for the three things that mean "this is not going to
	 * work".
	 */
	for (s = 0; s < CALLPROG_STATES; s++) {
		next_state_due_cptd[s][2] = CALLPROG_STATE_END;
		message_due_cptd[s][2] = CALLPROG_BUSY;
		next_state_due_cptd[s][3] = CALLPROG_STATE_END;
		message_due_cptd[s][3] = CALLPROG_CONGESTION;
		next_state_due_cptd[s][6] = CALLPROG_STATE_END;
		message_due_cptd[s][6] = CALLPROG_ERROR;
	}

	/* Waiting to dial: dial tone heard, so start dialling. */
	next_state_due_cptd[1][1] = 2;
	message_due_cptd[1][1] = CALLPROG_DIALING;
	next_state_due_cptd[1][4] = CALLPROG_STATE_END;
	message_due_cptd[1][4] = CALLPROG_ERROR;
	next_state_due_cptd[1][5] = CALLPROG_STATE_END;
	message_due_cptd[1][5] = CALLPROG_ERROR;

	/* Waiting for ring: ringback heard, so wait to be answered. */
	next_state_due_cptd[3][1] = CALLPROG_STATE_END;
	message_due_cptd[3][1] = CALLPROG_ERROR;
	next_state_due_cptd[3][4] = 4;
	message_due_cptd[3][4] = CALLPROG_RINGBACK;
	next_state_due_cptd[3][5] = 4;
	message_due_cptd[3][5] = CALLPROG_RINGBACK;

	/* Waiting to answer, and answering: stay put, say nothing. */
	next_state_due_cptd[4][1] = CALLPROG_STATE_END;
	message_due_cptd[4][1] = CALLPROG_ERROR;
	next_state_due_cptd[4][4] = 4;
	message_due_cptd[4][4] = CALLPROG_NO_MESSAGE;
	next_state_due_cptd[4][5] = 4;
	message_due_cptd[4][5] = CALLPROG_NO_MESSAGE;

	next_state_due_cptd[5][1] = CALLPROG_STATE_END;
	message_due_cptd[5][1] = CALLPROG_ERROR;
	next_state_due_cptd[5][4] = 4;
	message_due_cptd[5][4] = CALLPROG_NO_MESSAGE;
	next_state_due_cptd[5][5] = 4;
	message_due_cptd[5][5] = CALLPROG_NO_MESSAGE;

	/* Exactly one state has a line-clear timeout. */
	next_state_due_line_clear_timeout[4] = 5;
	message_due_line_clear_timeout[4] = CALLPROG_ANSWER;

	build_timeouts(cp);

	for (s = 0; s < CALLPROG_STATES; s++)
		automode_table[s] = 0;
	automode_table[3] = 1;
	automode_table[4] = 1;
	automode_table[5] = 1;

	/* Dial tone is listened for only where a modem waits to dial. */
	for (s = 0; s < CALLPROG_STATES; s++)
		toneiir_dialtone_table[s] = 0;
	toneiir_dialtone_table[1] = 1;
	toneiir_dialtone_table[2] = 1;

	/* Busy tone, everywhere. */
	for (s = 0; s < CALLPROG_STATES; s++)
		toneiir_busy_table[s] = 1;

	for (s = 0; s < CALLPROG_STATES; s++)
		enable_line_clear_timeout[s] = 0;
}

void
CALLPROG_Create(struct callprog *cp, struct callprog_cfg *cfg)
{
	struct cadence_setup setup;
	int i;

	/*
	 * The cadence detectors' descriptor, built once and reused for both.
	 * cadence_create writes the tone back into it, so it must be reset
	 * between the two calls -- which the original does by setting the
	 * field explicitly each time rather than relying on what is left.
	 */
	setup.w0 = 50;
	setup.w1 = 50;
	setup.w2 = 3;
	setup.w3 = 0;
	setup.tone = 0;
	setup.w5 = 0;
	setup.w6 = 1;

	cp->f1c = cfg->w0;
	cp->get_sreg = cfg->get_sreg;
	cp->modem = cfg->modem;
	cp->f28 = cfg->w3;
	cp->band = 0;

	/*
	 * Whether to band-limit at all.  `MustNoiseFilterBeApplied` is exactly
	 * what this filter is for -- the elliptic bandpass in callprog_cfg.h
	 * keeps everything but the call-progress band out of the detectors --
	 * so the country can turn it off on a line clean enough not to need
	 * it.
	 */
	cp->band_wanted = modem_get_param(cfg->modem,
					  MustNoiseFilterBeApplied);
	if (cp->band_wanted != 0)
		cp->band = _iir_filter_create(cp->band,
					      IIR_FILTER_COEFF, IIR_FILTER_COEFF,
					      CALLPROG_BandFilter_a,
					      CALLPROG_BandFilter_b,
					      CALLPROG_BandFilter_shift);

	cp->dialtone_seen = 0;

	setup.tone = CADENCE_TONE_BUSY;
	cp->busy = cadence_create(0, &setup, 0, cp->modem);
	setup.tone = CADENCE_TONE_DIAL;
	cp->dial = cadence_create(0, &setup, 0, cp->modem);

	cp->dtmf = Dual_TONE_create();

	for (i = 0; i < 7; i++)
		cp->timeout[i] = callprog_default_timeout[i];

	build_state_machine(cp);

	cp->state = CALLPROG_STATE_START;
	enter_state(cp);

	cp->line_clear_limit = cp->timeout[4] * 8000;
}

void
CALLPROG_Delete(struct callprog *cp)
{
	DialerAbort(&cp->dialer);

	if (cp->dial != 0)
		cadence_delete(cp->dial);
	if (cp->busy != 0)
		cadence_delete(cp->busy);
	if (cp->band_wanted != 0)
		_iir_filter_delete(cp->band);

	/*
	 * Two of the five pointers are cleared and three are not, so a second
	 * Delete on the same object would free `busy`, `band` and `dtmf`
	 * again.  call_delete calls this once.  See finding 55.
	 */
	cp->f70 = 0;
	cp->dial = 0;

	if (cp->dtmf != 0)
		Dual_TONE_delete(cp->dtmf);
}

/*
 * ---------------------------------------------------------------------------
 * CALLPROG_Dial  .text 0x07a5a0
 *
 * Everything the supervisor needs that could have changed since it was built:
 * the calling-tone setting, the dial string, and the timeouts.  Then it either
 * waits for dial tone or does not, which is the one real decision here.
 */
void
CALLPROG_Dial(struct callprog *cp, const char *s)
{
	long level;
	int flag;

	/*
	 * No S-register accessor means no way to find the calling tone's
	 * level, and the function gives up rather than calling through null.
	 */
	if (cp->get_sreg == 0)
		return;

	flag = modem_get_param(cp->modem, GetCallingToneFlag);
	cp->calling_tone_mode = flag;
	if (flag == 0)
		cp->calling_tone_armed = 0;
	else if (flag == 1)
		cp->calling_tone_armed = 1;
	else
		cp->calling_tone_armed = (flag == 2);

	/*
	 * S221, through the accessor call.c installed.  Narrowed to a signed
	 * char, so an S-register of 128 or more arrives negative -- see D13,
	 * where it makes almost no difference because the level control barely
	 * works.
	 */
	level = cp->get_sreg(cp->modem, 221);
	ResetCallingTone(&cp->calling_tone, (char)level);

	cp->fatal = DialerCreate(&cp->dialer, s, cp->modem);

	/*
	 * Five timeouts, all from the same parameter.  Whatever they were
	 * meant to be individually, the host is asked the same question five
	 * times and gives the same answer.
	 */
	cp->timeout[1] = modem_get_param(cp->modem, GetNoAnswerTimeOut);
	cp->timeout[2] = modem_get_param(cp->modem, GetNoAnswerTimeOut);
	cp->timeout[3] = modem_get_param(cp->modem, GetNoAnswerTimeOut);
	cp->timeout[4] = modem_get_param(cp->modem, GetNoAnswerTimeOut);
	cp->timeout[5] = modem_get_param(cp->modem, GetNoAnswerTimeOut);

	build_timeouts(cp);

	if (cp->f1c != 0) {
		/*
		 * Blind dialling: do not wait for dial tone.  State 1 times
		 * out straight into dialling rather than into an error, and
		 * both detectors are switched off for the two states that
		 * would otherwise be listening.
		 */
		timeout_table[1] = modem_get_param(cp->modem,
						   GetBlindDialPause);
		next_state_due_timeout[1] = 2;
		message_due_timeout[1] = CALLPROG_DIALING;
		toneiir_dialtone_table[1] = 0;
		toneiir_busy_table[1] = 0;
		toneiir_dialtone_table[2] = 0;
		toneiir_busy_table[2] = 0;
	} else {
		int wait = modem_get_param(cp->modem, GetDialToneWaitTime);
		int validate = modem_get_param(cp->modem,
					       GetDialToneValidationTime);
		int extra = (validate + 9) / 10;

		/*
		 * Long enough to hear dial tone, plus long enough to be sure
		 * of it -- the validation time in whole seconds, rounded up,
		 * and never less than two.
		 */
		if (extra <= 2)
			extra = 2;
		timeout_table[1] = wait + extra;

		next_state_due_timeout[1] = CALLPROG_STATE_END;
		message_due_timeout[1] = CALLPROG_NO_DIAL_TONE;
		toneiir_dialtone_table[1] = 1;
		toneiir_busy_table[1] = 1;
		toneiir_dialtone_table[2] = 1;
		toneiir_busy_table[2] = 1;
	}

	/* State 7 dials again without going back to waiting. */
	if (cp->state == 7)
		cp->state = 2;
	else
		cp->state = CALLPROG_STATE_START;

	enter_state(cp);
}

/*
 * The supervisor's per-buffer step.  Everything above builds the machine;
 * this runs it.
 *
 * Three things happen in order, and keeping them apart is what makes the
 * function readable:
 *
 *   1. every sample is offered to whichever cadence detector this state
 *      listens to, and the verdict, the timeout and the line-clear timeout
 *      each get a chance to request a transition;
 *   2. the state decides what to do with the buffer -- dial into it, put a
 *      calling tone in it, or judge whether the far end has gone quiet;
 *   3. the requested transition, if any, is taken.
 *
 * Only step 3 moves `cp->state`, so every sample in one call is judged
 * against the same state, and the caller sees at most one transition per
 * buffer.
 */

/* One buffer, and the largest this will accept. */
#define CALLPROG_MAX_SAMPLES	160

/* Detector verdicts, which index the eight-wide dimension of the tables. */
#define CPTD_NONE		0
#define CPTD_DIAL_TONE		1
#define CPTD_BUSY		2
#define CPTD_BUSY_GIVE_UP	7

/*
 * Waiting for the far end to answer: the envelope is a rectifier with a
 * single-pole smoother, and anything below the threshold counts as quiet.
 * `CALLPROG_ANSWER_SAMPLES / count` buffers of it means answered.
 */
#define ANSWER_GAIN		164	/* Q14: 0.01 */
#define ANSWER_DECAY		0x3f5c	/* Q14: 0.99 */
#define ANSWER_THRESHOLD	99
#define CALLPROG_ANSWER_SAMPLES	40000	/* five seconds at 8 kHz */

/*
 * Ask for a transition to `next`, unless one has already been asked for this
 * call, or `next` is nowhere, or it is where we already are.  The last test
 * is against `last_state` rather than `state`, so a detector that fires on
 * every sample cannot keep re-entering the state it is already in.
 */
static void
request_state(struct callprog *cp, int next)
{
	if (cp->pending == 1)
		return;
	if (next == 0 || cp->last_state == next)
		return;
	cp->pending_state = next;
	cp->pending = 1;
}

/* The leaky-integrator envelope used to decide the line has gone quiet. */
static int
answer_envelope(const short *buf, int count)
{
	int env = 0;
	int i;

	for (i = 0; i < count; i++) {
		int x = (buf[i] * ANSWER_GAIN) >> 14;
		int decayed = (env * ANSWER_DECAY) >> 14;

		if (x < 0)
			x = -x;
		if (decayed < 0)
			decayed = -decayed;
		env = (short)(x + decayed);
	}
	return (short)env;
}

/*
 * Offer one sample to the detectors this state listens to and return the
 * verdict.  Dial tone is asked first and busy second, and both can be asked
 * for the same sample -- the tables allow it, even though no state currently
 * sets both.
 */
static int
detect(struct callprog *cp, short sample, int event)
{
	if (toneiir_dialtone_table[cp->state] == 1) {
		if (cadence_progress(cp->dial, sample) == 1
		    && event == CPTD_NONE) {
			/*
			 * Dial tone heard.  This also latches off the band
			 * filter for the rest of the call: it is there to help
			 * find dial tone, and once found it only colours what
			 * follows.
			 */
			cp->dialtone_seen = 1;
			event = CPTD_DIAL_TONE;
		}
	}

	if (toneiir_busy_table[cp->state] == 1) {
		int r = cadence_progress(cp->busy, sample);

		if (r == 1) {
			event = CPTD_BUSY;
		} else if (r == 7) {
			/*
			 * The busy detector has given up.  The timeout is
			 * zeroed so that the state's own timeout fires on this
			 * same sample rather than a moment later.
			 */
			cp->countdown = 0;
			event = CPTD_BUSY_GIVE_UP;
		}
	}

	return event;
}

/* Take the verdict's transition and report its message. */
static void
apply_event(struct callprog *cp, int event, int *message)
{
	cp->event = event;
	cp->line_clear_count = 0;
	*message = message_due_cptd[cp->state][event];
	request_state(cp, next_state_due_cptd[cp->state][event]);
}

/*
 * Run the timeouts for one sample.  The state's own timeout counts down in
 * samples and then sits at -10 so it fires once; the line-clear timeout
 * counts buffers of state 6 and is the way a cleared line is noticed.
 */
static void
run_timeouts(struct callprog *cp, int *message)
{
	if (cp->countdown > 0) {
		cp->countdown--;
	} else if (cp->countdown == 0) {
		/*
		 * States 2 and 7 are exempt: dialling and the command-mode
		 * stop both last as long as they last.
		 */
		if (cp->state != CPSTATE_DIALING
		    && cp->state != CPSTATE_END_PARTIALLY_STATE) {
			cp->countdown = -10;
			*message = message_due_timeout[cp->state];
			request_state(cp, next_state_due_timeout[cp->state]);
		}
	}

	/*
	 * In the terminal state the previous message is repeated for as long
	 * as nothing else has anything to say.
	 */
	if (cp->state == CPSTATE_END)
		*message = cp->message;

	if (cp->line_clear_active != 1)
		return;
	if (++cp->line_clear_count <= cp->line_clear_limit)
		return;

	*message = message_due_line_clear_timeout[cp->state];
	request_state(cp, next_state_due_line_clear_timeout[cp->state]);
	cp->line_clear_active = 0;
}

int
CALLPROG_Progress(struct callprog *cp, const short *in, short *out, int count)
{
	short work[CALLPROG_MAX_SAMPLES];
	int message = 0;
	int pos = 0;
	int code = 0;
	int i;

	/*
	 * The verdict is NOT cleared between samples.  Once a detector fires,
	 * every remaining sample of the buffer re-applies its transition --
	 * harmless, because `request_state` refuses the second one, but it is
	 * why `message` and `event` end up holding the last sample's view.
	 */
	int event = CPTD_NONE;

	if (count > CALLPROG_MAX_SAMPLES)
		return CALLPROG_ERROR;

	sysdep_memcpy(work, in, count * 2);
	cp->pending = 0;
	cp->event = 0;

	/* The band filter, until dial tone has been heard. */
	if (cp->band_wanted != 0 && cp->dialtone_seen == 0)
		_iir_filter_progress(cp->band, count, work);

	if (cp->fatal == 7) {
		/*
		 * The dial string was rejected.  Note that the transition is
		 * recorded and then thrown away -- this returns without
		 * reaching the commit at the end, and the next call clears
		 * `pending` on entry.  Reproduced; it changes nothing, since
		 * every later call takes this same branch.
		 */
		request_state(cp, CPSTATE_END);
		return CALLPROG_ERROR;
	}

	/* 1. The detectors, sample by sample. */
	for (i = 0; i < count; i++) {
		event = detect(cp, work[i], event);

		if (event != CPTD_NONE)
			apply_event(cp, event, &message);
		run_timeouts(cp, &message);
	}

	/* 2. What this state does with the buffer. */
	if (automode_table[cp->state] == 1) {
		/*
		 * The answer-tone detector reads the caller's samples, not
		 * the filtered copy.
		 */
		int r = Dual_TONE_detect(cp->dtmf, in, count);

		if (r == 3)
			message = CALLPROG_DUALTONE_A;
		else if (r == 5)
			message = CALLPROG_DUALTONE_B;
	}

	if (cp->state == CPSTATE_WFS_STATE && count != 0) {
		/*
		 * Waiting for an answer, which is decided by silence rather
		 * than by any tone: five seconds below the threshold.
		 */
		if (answer_envelope(work, count) > ANSWER_THRESHOLD)
			cp->quiet_count = 0;
		else if (++cp->quiet_count == CALLPROG_ANSWER_SAMPLES / count)
			request_state(cp, CPSTATE_DIALING);
	} else if (cp->state == CPSTATE_WAIT_RING) {
		if (cp->calling_tone_armed != 0)
			GenerateCallingTone(&cp->calling_tone, out, count);
		else
			sysdep_memset(out, 0, count * 2);
	}

	/*
	 * Dialling is the only state that writes the outgoing buffer itself.
	 * The loop is for `^`, which arms or disarms the calling tone and then
	 * hands straight back to the dialler rather than ending the buffer.
	 */
	while (cp->state == CPSTATE_DIALING) {
		int rc = DialerProgress(&cp->dialer, out, &pos, count - 1);

		if (rc == DIALER_CALLING_TONE) {
			switch (cp->calling_tone_mode) {
			case 0: case 1:	cp->calling_tone_armed = 0; break;
			case 2: case 3:	cp->calling_tone_armed = 1; break;
			default:	break;
			}
			continue;
		}

		switch (rc) {
		case DIALER_BUSY:
			/*
			 * Still dialling.  This sets a message that the tail
			 * below then declines to use -- see findings; the
			 * effect is that CALLPROG_DIALING is never reported
			 * from here.
			 */
			code = CALLPROG_DIALING;
			break;
		case DIALER_WAIT_DIALTONE:
			request_state(cp, CPSTATE_WAIT_DIAL);
			cadence_reset(cp->dial);
			break;
		case DIALER_WAIT_ANSWER:
			request_state(cp, CPSTATE_WFS_STATE);
			break;
		case DIALER_WAIT_BONG:
			request_state(cp, CPSTATE_BONGTONE_STATE);
			break;
		case DIALER_COMMAND:
			request_state(cp, CPSTATE_END_PARTIALLY_STATE);
			code = CALLPROG_END_DIALING_PARTIALLY;
			break;
		case DIALER_DONE:
			request_state(cp, CPSTATE_WAIT_RING);
			code = CALLPROG_END_DIALING;
			break;
		default:
			request_state(cp, CPSTATE_END);
			code = CALLPROG_ERROR;
			break;
		}
		break;
	}

	/* Silence for whatever the dialler did not fill. */
	if (cp->state != CPSTATE_WAIT_RING) {
		while (pos < count)
			out[pos++] = 0;
	}

	/*
	 * Only three of the codes the block above can set are actually
	 * reported.  `CALLPROG_DIALING` is set and dropped.
	 */
	if (code == CALLPROG_END_DIALING || code == CALLPROG_ERROR
	    || code == CALLPROG_END_DIALING_PARTIALLY)
		message = code;

	/* 3. Take the transition. */
	if (cp->pending != 0) {
		int next = cp->pending_state;

		cp->state = next;
		cp->countdown = timeout_table[next] * 8000;
		if (next_state_due_line_clear_timeout[next] != 0)
			cp->line_clear_active = enable_line_clear_timeout[next];
		if (next == CPSTATE_WFS_STATE)
			cp->quiet_count = 0;
	}

	cp->last_state = cp->state;
	cp->message = message;
	return message;
}

/*
 * Grade a dial string on behalf of the supervisor.
 *
 * A thunk, and nothing but: the object's version adds 0x98 to its first
 * argument and tail-jumps.  0x98 is where the dialler sits inside the
 * supervisor, so this is the same question asked of the one the supervisor
 * owns rather than of a dialler the caller has to reach into.
 */
int
Dialer_IsDialStringInvalid(struct callprog *cp, const char *s)
{
	return IsDialStringInvalid(&cp->dialer, s);
}
