/*
 * V92EchoCanceller.cpp -- moving the echo canceller's delay.
 *
 * Reconstructed from dsplibs.o.  One of the class's twelve members:
 * `setEchoDelay`, which is the one `v34handshak` reaches.
 * `include/dsplib/V92EchoCanceller.h` carries the object map and the argument
 * that +0x2c is a tap count rather than a pointer.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).  This one
 * has no frame at all: `mov 0x4(%esp),%eax` is the whole prologue.
 *
 * THE OLD DELAY IS READ BEFORE IT IS OVERWRITTEN, which is the only ordering
 * constraint in the function.  The object computes `newDelay - oldDelay` into
 * `%ecx`, stores the new delay, and only then folds `%ecx` into the tap
 * count; the two spellings that produce that -- taking a copy of the old
 * value first, or writing the `+=` before the assignment -- are the same
 * code, because +0x2c and +0x38 are distinct members of one object and GCC
 * knows they cannot alias.  Written in the order that needs no temporary.
 *
 * THE DIAGNOSTIC IS A TAIL CALL.  `jmp edprintf`, not `call`, with the
 * arguments written into the caller's own outgoing slots -- which is what
 * `edprintf` being the last statement of a `void` function compiles to.  It
 * prints the argument, not the field, but the two are equal by then.
 */

#include <stddef.h>

#include "dsplib/encode.h"
#include "dsplib/V92EchoCanceller.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92EC_OFF(field, off, tag) \
	typedef char v92ec_off_##tag[ \
	    ((int)__builtin_offsetof(V92EchoCanceller, field) == (off)) \
	    ? 1 : -1]

V92EC_OFF(echoLength, 0x2c, echolength);
V92EC_OFF(echoDelay,  0x38, echodelay);
typedef char v92ec_size[(sizeof(V92EchoCanceller) == 0x3c) ? 1 : -1];
#endif

void
V92EchoCanceller::setEchoDelay(unsigned int delay)
{
	echoLength += delay - echoDelay;
	echoDelay = delay;

	edprintf("V92EchoCanceller: echoDelay updated to: %d\n", delay);
}
