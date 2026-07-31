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
 * Which cadence detector each state listens to.  `toneiir_busy_table` is all
 * ones -- busy tone is listened for everywhere -- and dial tone only in states
 * 1 and 2, which is where a modem is waiting to dial.  That is why
 * CALLPROG_Create builds two detectors and never a ringback or congestion
 * one: the machine never asks.
 */
extern unsigned char toneiir_dialtone_table[CALLPROG_STATES];
extern unsigned char toneiir_busy_table[CALLPROG_STATES];

/*
 * The supervisor's configuration: four words the caller builds on its stack.
 * `call_create` fills `get_sreg` with `call_GetSRegister`, which is how the
 * calling tone's level reaches S221 (finding 55).
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
	int	f30;					/* +0x30 */
	int	f34;		/* timeout[0] in samples   +0x34 */
	int	f38, f3c, f40, f44;			/* +0x38 */
	int	f48;		/* timeout[4] in samples   +0x48 */
	int	f4c;					/* +0x4c */
	int	f50, f54, f58, f5c, f60;		/* +0x50 */

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

	unsigned char	f80;				/* +0x80 */
	unsigned char	pad81[3];

	struct dual_tone *dtmf;				/* +0x84 */
	unsigned char	pad88[0x10];			/* +0x88 */

	struct dialer	dialer;				/* +0x98 */
};

/*
 * Build the supervisor.  Takes an object rather than returning one; also
 * builds the two cadence detectors, the answer-tone detector, optionally the
 * band filter, and the whole state machine.
 */
void CALLPROG_Create(struct callprog *cp, struct callprog_cfg *cfg);

/*
 * Tear down what Create built.  Does NOT free the supervisor itself, and
 * clears only two of the five pointers it releases -- see finding 55.
 */
void CALLPROG_Delete(struct callprog *cp);

#endif /* DSPLIB_CALLPROG_STATE_H */
