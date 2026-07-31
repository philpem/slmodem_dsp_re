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

struct callprog_status_name {
	int		status;
	const char	*name;
};

/*
 * .rodata+0x5dc0.  Seventeen entries, {code, name}, in code order -- so a
 * direct index would have worked for all but the last, and the original
 * searches anyway.
 */
static const struct callprog_status_name callprog_status_names[] = {
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

#define CALLPROG_STATUS_NAMES \
	((int)(sizeof(callprog_status_names) / sizeof(callprog_status_names[0])))

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

	for (i = 0; i < CALLPROG_STATUS_NAMES; i++)
		if (callprog_status_names[i].status == status)
			name = callprog_status_names[i].name;

	return name;
}
