/*
 * callprog_status.c -- Call Progress: message names.
 *
 * Reconstructed from dsplibs.o Callprog.c, .text 0x079540.
 *
 * Split out of callprog.c because it is pure data and reconstructing it
 * needed nothing but the object's own string table; the rest of Callprog.c
 * is a 5.2 kB state machine.
 */

#include "dsplib/callprog.h"

/*
 * .rodata+0x5d40.  The STATE names -- a different table from the message
 * names below, and a different shape: a plain array of ten pointers, indexed
 * by the state itself, with no code field and no search.
 *
 * `CALLPROG_Progress` indexes it raw, `mov 0x5d40(,%esi,4),%edx`, eleven times
 * -- once per transition it reports.  There is no bounds check in the object
 * and there is none here; the states are 0..9 and every write to the field is
 * a constant in that range, so the check would be unreachable.
 *
 * Two things worth noticing.  State 2 is spelled `CALLPROG_DIALING`, exactly
 * the same string as MESSAGE 3 in the table below, which is why callprog.h
 * calls the state `CALLPROG_DIALING_STATE` -- the collision is the original's,
 * not ours.  And the table stops at ten: finding F142 recorded it as running on
 * into the sixteen message names, and that was wrong.  Only ten relocations
 * apply to it, and what follows is unrelated data that merely disassembles as
 * plausible pointers.
 */
const char *const callprog_state_names[CALLPROG_STATES_NAMED] = {
	"CALLPROG_NO_LEGAL_STATE",		/* 0 */
	"CALLPROG_WAIT_DIAL",			/* 1 */
	"CALLPROG_DIALING",			/* 2 */
	"CALLPROG_WAIT_RING",			/* 3 */
	"CALLPROG_WAIT_TO_ANSWER",		/* 4 */
	"CALLPROG_ANSWER_STATE",		/* 5 */
	"CALLPROG_END",				/* 6 */
	"CALLPROG_END_PARTIALLY_STATE",		/* 7 */
	"CALLPROG_WFS_STATE",			/* 8 */
	"CALLPROG_BONGTONE_STATE"		/* 9 */
};

struct callprog_status_name {
	int		status;
	const char	*name;
};

/*
 * .rodata+0x5dc0.  Seventeen entries, {code, name}, in code order -- so a
 * direct index would have worked for all but the last, and the original
 * searches anyway.
 */
static const struct callprog_status_name message_names[] = {
	{ CALLPROG_NO_MESSAGE,		"CALLPROG_NO_MESSAGE"		},
	{ CALLPROG_NO_RING,		"CALLPROG_NO_RING"		},
	{ CALLPROG_NO_DIAL_TONE,	"CALLPROG_NO_DIAL_TONE"		},
	{ CALLPROG_DIALING,		"CALLPROG_DIALING"		},
	{ CALLPROG_END_DIALING,		"CALLPROG_END_DIALING"		},
	{ CALLPROG_RINGBACK,		"CALLPROG_RINGBACK"		},
	{ CALLPROG_NO_ANSWER,		"CALLPROG_NO_ANSWER"		},
	{ CALLPROG_ANSWER,		"CALLPROG_ANSWER"		},
	{ CALLPROG_MODEM_ANSWER,	"CALLPROG_MODEM_ANSWER"		},
	{ CALLPROG_VOICE_ANSWER,	"CALLPROG_VOICE_ANSWER"		},
	{ CALLPROG_BUSY,		"CALLPROG_BUSY"			},
	{ CALLPROG_CONGESTION,		"CALLPROG_CONGESTION"		},
	{ CALLPROG_ERROR,		"CALLPROG_ERROR"		},
	{ CALLPROG_ANSWER_STATE_TIMEOUT, "CALLPROG_ANSWER_STATE_TIMEOUT" },
	{ CALLPROG_END_DIALING_PARTIALLY, "CALLPROG_END_DIALING_PARTIALLY" },
	{ CALLPROG_V8BIS_MODEM_ANSWER,	"CALLPROG_V8BIS_MODEM_ANSWER"	},
	{ CALLPROG_MAX_MESSAGES,	"CALLPROG_MAX_MESSAGES"		}
};

#define MESSAGE_NAMES \
	((int)(sizeof(message_names) / sizeof(message_names[0])))

/*
 * The scan does not stop at the first hit -- it runs the whole table and
 * keeps the last match.  With this table that is indistinguishable from
 * stopping early, since no code appears twice, and it is reproduced only
 * because a duplicate added later would change the answer.
 */
const char *
CALLPROG_Status_string(int status)
{
	const char *name = "";
	int i;

	for (i = 0; i < MESSAGE_NAMES; i++)
		if (message_names[i].status == status)
			name = message_names[i].name;

	return name;
}
