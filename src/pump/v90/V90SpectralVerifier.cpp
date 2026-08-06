/*
 * V90SpectralVerifier.cpp -- clearing the spectrum accumulator.
 *
 * Reconstructed from dsplibs.o.  One of the class's twelve members:
 * `reset()`, which is the one `v34handshak` reaches.
 * `include/dsplib/V90SpectralVerifier.h` carries the object map.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THE DIAGNOSTIC IS NOT GATED.  Unlike `V90ConstellationDesigner`'s two, this
 * one is a bare `call edprintf` with no `dsplibs_debug_level` test in front
 * of it -- because `edprintf` does its own gating, and does it AFTER
 * formatting and encoding, so the call has an effect at every level (see
 * src/core/encode.c).  It comes first in the object and it comes first here.
 *
 * THE THREE STORES ARE INDEPENDENT.  The object emits them +0x24, +0x20,
 * +0x28; they are three zeroes into three distinct words of the same object,
 * so the order is the scheduler's and not the source's, and no order of the
 * three is distinguishable by any observer.  Written low-to-high here.
 */

#include <stddef.h>

#include "dsplib/encode.h"
#include "dsplib/V90SpectralVerifier.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SV_OFF(field, off, tag) \
	typedef char v90sv_off_##tag[ \
	    ((int)__builtin_offsetof(V90SpectralVerifier, field) == (off)) \
	    ? 1 : -1]

V90SV_OFF(accumCount,   0x20, accumcount);
V90SV_OFF(accumulating, 0x24, accumulating);
V90SV_OFF(word_28,      0x28, word28);
typedef char v90sv_size[(sizeof(V90SpectralVerifier) == 0x2c) ? 1 : -1];
#endif

void
V90SpectralVerifier::reset()
{
	edprintf("V90SpectralVerifier: Reset\r\n");

	accumCount = 0;
	accumulating = 0;
	word_28 = 0;
}
