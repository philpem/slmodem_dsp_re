/*
 * callprog.h -- Call Progress: status codes and the supervisor's interface.
 *
 * Call progress is what the modem does between "go off hook" and "there is a
 * modem on the other end": listen to the line, decide whether it heard dial
 * tone, ringback, busy, congestion or a person, and drive the dialler through
 * the number.  Everything in this header is about reporting that decision
 * upward; the deciding itself lives in cadence.c and dualtone.c.
 *
 * The status names are not guesses.  The original carries the whole table as
 * string literals for its own debug output -- code and spelling both -- at
 * .rodata+0x5dc0, and CALLPROG_Status_string is the accessor.  See
 * docs/findings.md.
 */

#ifndef DSPLIB_CALLPROG_H
#define DSPLIB_CALLPROG_H

/*
 * Progress messages, as reported to the caller.
 *
 * Codes 16 and 17 have no name in the original's table even though
 * CALLPROG_MAX_MESSAGES is 18, so two messages were retired without the table
 * being renumbered.  CALLPROG_Status_string returns "" for them.
 */
#define CALLPROG_NO_MESSAGE		0
#define CALLPROG_NO_RING		1
#define CALLPROG_NO_DIAL_TONE		2
#define CALLPROG_DIALING		3
#define CALLPROG_END_DIALING		4
#define CALLPROG_RINGBACK		5
#define CALLPROG_NO_ANSWER		6
#define CALLPROG_ANSWER			7
#define CALLPROG_MODEM_ANSWER		8
#define CALLPROG_VOICE_ANSWER		9
#define CALLPROG_BUSY			10
#define CALLPROG_CONGESTION		11
#define CALLPROG_ERROR			12
#define CALLPROG_ANSWER_STATE_TIMEOUT	13
#define CALLPROG_END_DIALING_PARTIALLY	14
#define CALLPROG_V8BIS_MODEM_ANSWER	15
/*
 * Two the object does not name: they are what the automode dual-tone
 * detector reports, for its verdicts 3 and 5.  Named after where they come
 * from, since the original's names for them were not recovered.
 */
#define CALLPROG_DUALTONE_A		16
#define CALLPROG_DUALTONE_B		17
#define CALLPROG_MAX_MESSAGES		18

/*
 * The supervisor's own state machine.  These names also come from the
 * original's debug strings (.rodata.str1.1+0xc7693 onward in the object), and
 * are a different enum from the messages above -- CALLPROG_END_PARTIALLY_STATE
 * and CALLPROG_END_DIALING_PARTIALLY are unrelated.
 *
 * NOW PINNED.  `CALLPROG_Progress` logs every transition as
 * "STATE:  %s --> %s", and both arguments index a table of string pointers
 * at .rodata+0x5d40 with the state itself -- so the table index IS the enum
 * value.  The order guessed from emission order was right; this confirms it
 * rather than assuming it.  See finding F142.
 *
 * Finding F142 also said the table runs on past these ten into the sixteen
 * MESSAGE names, making it an independent check on that enum.  It does not.
 * Exactly ten relocations apply to it; what follows is unrelated data that
 * happens to disassemble as plausible pointers.  The message names are their
 * own table at .rodata+0x5dc0, in a different shape ({code, name} pairs,
 * searched rather than indexed).  See finding F151.
 */
#define CALLPROG_NO_LEGAL_STATE		0
#define CALLPROG_WAIT_DIAL		1
#define CALLPROG_DIALING_STATE		2	/* not the message of the
						 * same name, which is 3 */
#define CALLPROG_WAIT_RING		3
#define CALLPROG_WAIT_TO_ANSWER		4
#define CALLPROG_ANSWER_STATE		5
#define CALLPROG_END			6
#define CALLPROG_END_PARTIALLY_STATE	7
#define CALLPROG_WFS_STATE		8	/* wait for silence */
#define CALLPROG_BONGTONE_STATE		9

/*
 * Name of a progress message, for logging.  Unknown codes give "", not NULL,
 * so a caller can print the result unconditionally.
 */
const char *CALLPROG_Status_string(int status);

/*
 * Names of the ten states above, indexed by the state.  Indexed raw, exactly
 * as the object does it, so the caller is responsible for the range -- see
 * the comment on the table in callprog_status.c.
 */
#define CALLPROG_STATES_NAMED	10

extern const char *const callprog_state_names[CALLPROG_STATES_NAMED];

#endif /* DSPLIB_CALLPROG_H */
