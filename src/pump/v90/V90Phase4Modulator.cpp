/*
 * V90Phase4Modulator.cpp -- the phase 4 modulator's session flag.
 *
 * Reconstructed from dsplibs.o.  One of the class's forty-three members:
 * `setSessionFlag`, which is the one `v34handshak` reaches.
 * `include/dsplib/V90Phase4Modulator.h` carries the object map and how its
 * 12,204 bytes were bounded.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).  Eleven
 * bytes: two loads, a store and a return, with no frame.
 *
 * `V90Phase3Modulator::setSessionFlag` is the same eleven bytes against the
 * same offset in a different class, and src/pump/v90/V90Phase3Modulator.cpp
 * is where that one lives.
 */

#include <stddef.h>

#include "dsplib/V90Phase4Modulator.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v90p4_off_sessionflag[
    ((int)__builtin_offsetof(V90Phase4Modulator, sessionFlag) == 0x0000)
    ? 1 : -1];
typedef char v90p4_size[(sizeof(V90Phase4Modulator) == 0x2fac) ? 1 : -1];
#endif

void
V90Phase4Modulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
}
