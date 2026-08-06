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

#include "dsplib/callprog.h"
#include "dsplib/callprog_state.h"
#include "dsplib/debug.h"
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
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CallProgFP_Create >>\n");

	cp->band_wanted = modem_get_param(cfg->modem,
					  MustNoiseFilterBeApplied);

	/*
	 * After the parameter read, which the argument pins: it IS the read's
	 * result.  The gate sits late in the body (0x79982), later than the
	 * statement below would suggest, so the exact line is not certain --
	 * the data dependency is, and so is the order against the get_param,
	 * which the harness now marks.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("APPLY_FILTER = %d\n", cp->band_wanted);

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

	/*
	 * "<<" MARKS THE EXIT, not the entry, and the arrows mean what they
	 * say after all: ">>" going in to the nested create at the top, "<<"
	 * coming back out of this one at the bottom.  An earlier reading put
	 * this first, on the ground that its gate is at 0x79588 and the other
	 * at 0x795e6 -- but GCC moves these blocks out of line, so the order
	 * of the GATES is not the order they run in, and the transcript says
	 * the object prints this immediately before `CALLPROG Dialing`.
	 * Finding 194.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CALLPROG Create <<\n");
}

void
CALLPROG_Delete(struct callprog *cp)
{
	DialerAbort(&cp->dialer);

	/*
	 * AFTER the abort, not before it, which is the opposite of what
	 * "is entered" suggests -- the transcript shows the object printing
	 * "Dialer was aborted." first.  A store cannot cross a call, but a
	 * gated print can sit either side of one and nothing but the trace can
	 * say which.  Finding 194.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CALLPROG_Delete is entered\n");

	/*
	 * Which name goes with which object is read from the return targets,
	 * not from the order they are written here: DIAL_OBJ's block returns
	 * to 0x79493 and CADENCE_OBJ's to 0x7949a, so the dial detector is
	 * announced first -- as it is deleted first.
	 */
	if (cp->dial != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("cadence_delete with "
					     "CADENCE_DIAL_OBJ is invoked\n");

		cadence_delete(cp->dial);
	}
	if (cp->busy != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("cadence_delete with "
					     "CADENCE_OBJ is invoked\n");

		cadence_delete(cp->busy);
	}
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

	/* A tail call in the object -- `jmp`, not `call`, at 0x794cd. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CALLPROG_Delete is exited\n");
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
	 * ANNOUNCED BEFORE THE REFUSAL, not after.
	 *
	 * The object checks the debug level at function entry (0x7a5ab) and
	 * jumps to an out-of-line block at 0x7a86d that prints this and
	 * returns to 0x7a5bc -- which is the load of `get_sreg`.  So the
	 * dial string is announced first and the refusal below second, and
	 * a caller with no accessor sees both lines.
	 *
	 * This was the other way round here until the refusal path was
	 * driven with the level raised, and the transcripts disagreed by
	 * exactly this line.  Finding 240.  It is finding 194's warning
	 * again: the cold block sits 0x2b0 bytes past the gate, so gate
	 * ADDRESS order is not execution order and reading the two in
	 * address order puts this print second.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CALLPROG Dialing %s\n", s);

	/*
	 * No S-register accessor means no way to find the calling tone's
	 * level, and the function gives up rather than calling through null.
	 */
	if (cp->get_sreg == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"sreg function is not defined!\n");

		return;
	}

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

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("GetNoAnswerTimeOut. %d\n",
				     cp->timeout[5]);

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

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"BlindCall: GetBlindDialPause = %d .\n",
				timeout_table[1]);

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

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("WAIT DIAL TIMEOUT = %d\n",
					     timeout_table[1]);

		next_state_due_timeout[1] = CALLPROG_STATE_END;
		message_due_timeout[1] = CALLPROG_NO_DIAL_TONE;
		toneiir_dialtone_table[1] = 1;
		toneiir_busy_table[1] = 1;
		toneiir_dialtone_table[2] = 1;
		toneiir_busy_table[2] = 1;
	}

	/* State 7 dials again without going back to waiting. */
	if (cp->state == 7) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Set state to CALLPROG_DIALING_STATE\n");

		cp->state = 2;
	} else
		cp->state = CALLPROG_STATE_START;

	enter_state(cp);

	/* A tail call in the object -- `jmp`, not `call`, at 0x7a868. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CALLPROG_Dial was exited.\n");
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

	/*
	 * Eleven call sites in the object, all inlined here, all after both
	 * guards and before the store -- so a refused transition is silent.
	 * Eight of them had `next` folded to a constant and the table load
	 * folded with it; three did not.  Same shape either way (finding 152).
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("STATE:  %s --> %s\n",
				     callprog_state_names[cp->state],
				     callprog_state_names[next]);

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
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"tone detected by cadence dial\n");

			cp->dialtone_seen = 1;
			event = CPTD_DIAL_TONE;
		}
	}

	if (toneiir_busy_table[cp->state] == 1) {
		int r = cadence_progress(cp->busy, sample);

		if (r == 1) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"busy detected by cadence\n");

			event = CPTD_BUSY;
		} else if (r == 7) {
			/*
			 * The busy detector has given up.  The timeout is
			 * zeroed so that the state's own timeout fires on this
			 * same sample rather than a moment later.
			 */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"no answer detected by cadence\n");

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
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("CALLPROG: Time out\n");

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

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CALLPROG: LINE CLEAR TIMEOUT\n");

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

	if (count > CALLPROG_MAX_SAMPLES) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Invalid buffer length\n");

		return CALLPROG_ERROR;
	}

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
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("DIALER_ERROR_MSG encountered in "
					     "Callprog_progress.\n");

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

		/*
		 * The two messages callprog.h could not name.  These strings
		 * name them: verdict 3 is 2100 Hz and verdict 5 is 2250 Hz.
		 * Finding 153.
		 */
		if (r == 3) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Found 2100\n");

			message = CALLPROG_DUALTONE_A;
		} else if (r == 5) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Found 2250\n");

			message = CALLPROG_DUALTONE_B;
		}
	}

	if (cp->state == CPSTATE_WFS_STATE && count != 0) {
		/*
		 * Waiting for an answer, which is decided by silence rather
		 * than by any tone: five seconds below the threshold.
		 */
		if (answer_envelope(work, count) > ANSWER_THRESHOLD)
			cp->quiet_count = 0;
		else if (++cp->quiet_count == CALLPROG_ANSWER_SAMPLES / count) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"CALLPROG: 5 sec. silence was "
					"detected.\n");

			request_state(cp, CPSTATE_DIALING);
		}
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
			/*
			 * FOUR ARMS AND FOUR MESSAGES, one each.  The bare
			 * "^ encountered." is not a preamble to the other
			 * three -- it is what modes 0 and 2 say, and modes 1
			 * and 3 do not say it at all.  A mode outside 0..3
			 * says nothing and does not touch `calling_tone_armed`
			 * either.
			 *
			 *   mode 0   armed = 0   "^ encountered."
			 *   mode 1   armed = 0   "... Disabling Calling-Tone."
			 *   mode 2   armed = 1   "^ encountered."
			 *   mode 3   armed = 1   "... Enabling Calling-Tone."
			 *
			 * Mode 2 is what makes the shape hard to guess from
			 * the source side: it stores `armed = 1` at 0x7a439
			 * and then jumps BACK to 0x7a020, the debug gate that
			 * belongs to mode 0, so it reaches the bare message by
			 * sharing mode 0's tail rather than by having one of
			 * its own.  Read as a switch, that looks like the
			 * message is common to every arm.  It is not.
			 *
			 * The store and the message stay on opposite sides for
			 * modes 1 and 3, as before: "Disabling" after the store
			 * (0x7a31d precedes the branch at 0x7a324), "Enabling"
			 * before it (0x7a283 follows the block at 0x7a52f).
			 * Same asymmetry as the pulse dialler's hook messages,
			 * finding 148.
			 */
			switch (cp->calling_tone_mode) {
			case 0:
				cp->calling_tone_armed = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"CALLPROG: ^ encountered.\n");
				break;
			case 1:
				cp->calling_tone_armed = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"CALLPROG: ^ encountered. "
						"Disabling Calling-Tone.\n");
				break;
			case 2:
				cp->calling_tone_armed = 1;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"CALLPROG: ^ encountered.\n");
				break;
			case 3:
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"CALLPROG: ^ encountered. "
						"Enabling Calling-Tone.\n");
				cp->calling_tone_armed = 1;
				break;
			default:
				break;
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
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("CALLPROG: Wait dial "
						     "tone, reset the "
						     "cadence\n");
			cadence_reset(cp->dial);
			break;
		case DIALER_WAIT_ANSWER:
			request_state(cp, CPSTATE_WFS_STATE);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"CALLPROG: @ encountered.\n");
			break;
		case DIALER_WAIT_BONG:
			request_state(cp, CPSTATE_BONGTONE_STATE);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"CALLPROG: $ encountered.\n");
			break;
		case DIALER_COMMAND:
			request_state(cp, CPSTATE_END_PARTIALLY_STATE);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("CALLPROG: Dialing "
						     "string partially "
						     "ended.\n");
			code = CALLPROG_END_DIALING_PARTIALLY;
			break;
		case DIALER_DONE:
			request_state(cp, CPSTATE_WAIT_RING);
			code = CALLPROG_END_DIALING;
			break;
		default:
			request_state(cp, CPSTATE_END);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"CALLPROG: Dialing string error.\n");
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
