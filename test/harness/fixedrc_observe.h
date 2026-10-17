#ifndef TEST_FIXEDRC_OBSERVE_H
#define TEST_FIXEDRC_OBSERVE_H

#include <string.h>
#include "dsplib/fixedrc.h"

/* Fixture view of the existing handle contract: the state pointer follows
 * the kind word at +4 on i386, +8 on the native x86_64 portability tier.
 * Copying that pointer observes the actual allocation without defining a
 * second struct rc or exposing a measurement API in production. */
static struct rc_state *
RcFixed_State(const void *handle)
{
	struct rc_state *state;

	if (handle == NULL)
		return NULL;
	memcpy(&state, (const unsigned char *)handle + sizeof(void *),
	       sizeof(state));
	return state;
}

#endif
