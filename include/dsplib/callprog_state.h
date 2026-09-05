/*
 * callprog_state.h -- the call-progress supervisor's object and state tables.
 *
 * The tables are built at run time by `CALLPROG_Create` rather than declared
 * as static data, which is unusual and worth knowing: nothing in `.rodata`
 * describes this machine, and reading `CALLPROG_Progress` alone would leave
 * you staring at eleven anonymous arrays.
 *
 * They keep their names in the object's symbol table even though they are
 * file statics, so the vocabulary below is the author's, not invented here.
 * See docs/callprog_states.md for the extracted contents.
 *
 * ONE DELIBERATE DEVIATION: the original's tables are file statics.  Ours are
 * global, purely so the differential test can compare them -- the original's
 * can be reached only by pinning the `.bss` base off a neighbouring global
 * symbol, and having both sides visible by name is worth more than matching a
 * storage class that changes no behaviour.
 */

#ifndef DSPLIB_CALLPROG_STATE_H
#define DSPLIB_CALLPROG_STATE_H

#include "dsplib/cadence.h"
#include "dsplib/dualtone.h"
#include "dsplib/dialer.h"
#include "dsplib/toneiir.h"
#include "dsplib/callingtone.h"

/*
 * The states, with the author's own names: the object carries them as debug
 * strings, so these are recovered rather than invented.  They are prefixed
 * here because the original's two enums overlap -- `CALLPROG_DIALING` is
 * state 2 and, separately, message 3.
 */
#define CPSTATE_NO_LEGAL_STATE		0
#define CPSTATE_WAIT_DIAL		1
#define CPSTATE_DIALING			2
#define CPSTATE_WAIT_RING		3
#define CPSTATE_WAIT_TO_ANSWER		4
#define CPSTATE_ANSWER_STATE		5
#define CPSTATE_END			6
#define CPSTATE_END_PARTIALLY_STATE	7
#define CPSTATE_WFS_STATE		8	/* wait for silence */
#define CPSTATE_BONGTONE_STATE		9

/* Ten states, and eight possible verdicts from the tone detector. */
#define CALLPROG_STATES		10
#define CALLPROG_CPTD_EVENTS	8

/*
 * Three ways to leave a state, each with its own next-state and message:
 * the call-progress tone detector, a timeout, and a line-clear timeout.
 */
extern unsigned char next_state_due_cptd[CALLPROG_STATES][CALLPROG_CPTD_EVENTS];
extern unsigned char message_due_cptd[CALLPROG_STATES][CALLPROG_CPTD_EVENTS];
extern unsigned char next_state_due_timeout[CALLPROG_STATES];
extern unsigned char message_due_timeout[CALLPROG_STATES];
extern unsigned char next_state_due_line_clear_timeout[CALLPROG_STATES];
extern unsigned char message_due_line_clear_timeout[CALLPROG_STATES];

extern int timeout_table[CALLPROG_STATES];
extern int enable_line_clear_timeout[CALLPROG_STATES];

extern unsigned char automode_table[CALLPROG_STATES];

/*
 * Which cadence detector each state listens to.  Both tables depend on
 * `cfg.w0`, which `call_create` derives from S56, and it inverts them: with
 * w0 = 0 dial tone is heard in states 1 and 2 and busy everywhere, while with
 * w0 = 1 dial tone is heard nowhere and states 1 and 2 hear nothing at all --
 * blind dialling, where the wait in CALLPROG_WAIT_DIAL ends on its timeout
 * rather than on a tone.  See docs/callprog_states.md.
 *
 * Either way only these two detectors are ever named, which is why
 * CALLPROG_Create builds two and never a ringback or congestion one.
 */
extern unsigned char toneiir_dialtone_table[CALLPROG_STATES];
extern unsigned char toneiir_busy_table[CALLPROG_STATES];

/*
 * The supervisor's configuration: four words the caller builds on its stack.
 * `call_create` fills `get_sreg` with `call_GetSRegister`, which is how the
 * calling tone's level reaches S221 (finding F55).
 */
struct callprog_cfg {
	int	w0;					/* -> cp->f1c    */
	long	(*get_sreg)(void *modem, unsigned short n);	/* -> cp->get_sreg */
	void	*modem;					/* -> cp->modem  */
	int	w3;					/* -> cp->f28    */
};

/*
 * The supervisor object.  Embedded in the call object at +0x444, not
 * allocated, which is why CALLPROG_Delete frees the pieces and not the whole.
 */
struct callprog {
	/*
	 * Seven timeouts in seconds, copied from a table in .rodata:
	 * { 10, 20, 15, 10, 8, 60, 60 }.  The state machine's timeout_table
	 * is filled from six of them.
	 */
	int	timeout[7];				/* +0x00 */

	int	f1c;					/* +0x1c */
	long	(*get_sreg)(void *modem, unsigned short n);	/* +0x20 */
	void	*modem;					/* +0x24 */
	int	f28;					/* +0x28 */

	int	state;					/* +0x2c */

	/*
	 * The state as of the end of the last call.  A transition is only
	 * taken when it would land somewhere other than here, which is what
	 * stops a detector that keeps firing from re-entering its own state.
	 */
	int	last_state;				/* +0x30 */

	/* Counts down in samples; -10 once fired, so it fires once. */
	int	countdown;				/* +0x34 */

	/* Where to go at the end of this call, and whether to go anywhere. */
	int	pending_state;				/* +0x38 */
	int	pending;				/* +0x3c */

	int	message;	/* the last one reported  +0x40 */

	/* The line-clear timeout: its own counter, limit and enable. */
	int	line_clear_count;			/* +0x44 */
	int	line_clear_limit;			/* +0x48 */
	int	line_clear_active;			/* +0x4c */

	int	event;		/* last detector verdict  +0x50 */

	/*
	 * Consecutive quiet buffers in state 8, which is how waiting for the
	 * far end to answer is decided.  Reset whenever the envelope rises.
	 */
	int	quiet_count;				/* +0x54 */

	int	fatal;		/* 7 means give up        +0x58 */

	/* The calling tone: what the country asked for, and whether it is on. */
	int	calling_tone_mode;			/* +0x5c */
	int	calling_tone_armed;			/* +0x60 */

	struct cadence	*dial;				/* +0x64 */
	int		f68;				/* +0x68 */
	struct cadence	*busy;				/* +0x6c */
	int		f70, f74;			/* +0x70 */

	/*
	 * The band filter, and `MustNoiseFilterBeApplied` -- the country's say
	 * in whether one is built at all.  CALLPROG_Delete uses the flag
	 * rather than the pointer to decide whether to free it.
	 */
	struct iir_filter *band;			/* +0x78 */
	int		band_wanted;			/* +0x7c */

	/*
	 * Latched the moment dial tone is heard.  It gates the band filter,
	 * which is therefore applied only until then -- see findings.
	 */
	unsigned char	dialtone_seen;			/* +0x80 */
	unsigned char	pad81[3];

	struct dual_tone *dtmf;				/* +0x84 */

	/* The calling-tone generator, armed by CALLPROG_Dial. */
	struct calling_tone calling_tone;		/* +0x88 */

	struct dialer	dialer;				/* +0x98 */
};

/**
 * @brief Build the call-progress supervisor.
 *
 * Takes an object rather than returning one, since the supervisor is
 * embedded in the call datapump, not allocated. Also builds the two
 * cadence detectors, the answer-tone detector, optionally the band
 * filter, and the whole state machine.
 *
 * @param cp   Caller-owned supervisor object to initialise.
 * @param cfg  Configuration (S-register accessor, modem pointer, two
 *             country-derived flags).
 */
void CALLPROG_Create(struct callprog *cp, struct callprog_cfg *cfg);

/**
 * @brief Tear down what CALLPROG_Create() built.
 *
 * Does NOT free the supervisor itself, and clears only two of the five
 * pointers it releases -- see finding F55.
 *
 * @param cp  The supervisor to tear down.
 */
void CALLPROG_Delete(struct callprog *cp);

/**
 * @brief Start a call.
 *
 * Refreshes everything that could have changed since CALLPROG_Create(),
 * arms the calling tone, hands the string to the dialler, and decides
 * whether to wait for dial tone. Does nothing at all if no S-register
 * accessor was configured.
 *
 * @param cp  The supervisor.
 * @param s   The dial string.
 */
void CALLPROG_Dial(struct callprog *cp, const char *s);

/**
 * @brief Grade a dial string, asked of the dialler the supervisor owns.
 *
 * The object defines this as a two-instruction thunk onto
 * IsDialStringInvalid().
 *
 * @param cp  The supervisor, whose dialler is queried.
 * @param s   The string to grade.
 * @return Nonzero if the string is too poor to dial.
 */
int Dialer_IsDialStringInvalid(struct callprog *cp, const char *s);

/**
 * @brief Run one buffer through the call-progress supervisor.
 *
 * At most one state transition happens per call, at the very end.
 *
 * @param cp     The supervisor, updated in place.
 * @param in     @p count input samples.
 * @param out    @p count output samples.
 * @param count  Number of samples.
 * @return The `CALLPROG_*` message for this buffer.
 */
int CALLPROG_Progress(struct callprog *cp, const short *in, short *out,
		      int count);

#endif /* DSPLIB_CALLPROG_STATE_H */
