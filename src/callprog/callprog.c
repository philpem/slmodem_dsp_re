/*
 * callprog.c -- Call Progress: the supervisor.
 *
 * Reconstructed from dsplibs.o Callprog.c:
 *
 *   CALLPROG_Create   .text 0x079570
 *   CALLPROG_Delete   .text 0x079440
 *
 * CALLPROG_Dial and CALLPROG_Progress are not reconstructed yet.
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

extern int modem_get_param(void *modem, int name);

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

	cp->f80 = 0;

	setup.tone = CADENCE_TONE_BUSY;
	cp->busy = cadence_create(0, &setup, 0, cp->modem);
	setup.tone = CADENCE_TONE_DIAL;
	cp->dial = cadence_create(0, &setup, 0, cp->modem);

	cp->dtmf = Dual_TONE_create();

	for (i = 0; i < 7; i++)
		cp->timeout[i] = callprog_default_timeout[i];

	build_state_machine(cp);

	cp->state = CALLPROG_STATE_START;
	cp->f34 = cp->timeout[0] * 8000;

	if (next_state_due_line_clear_timeout[CALLPROG_STATE_START] != 0)
		cp->f4c = enable_line_clear_timeout[CALLPROG_STATE_START];

	if (cp->state == 8)
		cp->f54 = 0;

	cp->f48 = cp->timeout[4] * 8000;
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
