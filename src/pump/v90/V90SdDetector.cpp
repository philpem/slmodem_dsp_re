/*
 * V90SdDetector.cpp -- clearing the SD detector.
 *
 * Reconstructed from dsplibs.o.  One of the class's four members: `reset()`,
 * which is the one `v34handshak` reaches.
 * `include/dsplib/V90SdDetector.h` carries the object map and the evidence.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THE LOOP BOUND IS UNSIGNED and the guard is separate from it.  The object
 * tests `cmp $0x0,%ecx` / `jbe` before loading the buffer pointer at all, and
 * closes the loop with `cmp %eax,%ecx` / `ja` -- both unsigned, which is what
 * `historyLength` being `unsigned int` compiles to.  A signed bound would
 * have produced `jle` and `jg`.  The pointer load sitting INSIDE the guard is
 * ordinary code motion and not a null check: nothing here tests the pointer.
 *
 * THE COUNTER IS CLEARED LAST.  `movl $0x0,(%ebx)` is after the loop, not
 * before it.  The order is visible rather than assumed because a store
 * through `history` could alias `count` as far as the compiler knows, so it
 * could not have moved the store across the loop even if the source had put
 * it first.
 */

#include <stddef.h>

#include "dsplib/V90SdDetector.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SD_OFF(field, off, tag) \
	typedef char v90sd_off_##tag[ \
	    ((int)__builtin_offsetof(V90SdDetector, field) == (off)) ? 1 : -1]

V90SD_OFF(count,         0x00, count);
V90SD_OFF(history,       0x14, history);
V90SD_OFF(historyLength, 0x18, historylength);
typedef char v90sd_size[(sizeof(V90SdDetector) == 0x1c) ? 1 : -1];
#endif

void
V90SdDetector::reset()
{
	unsigned int i;

	for (i = 0; i < historyLength; i++)
		history[i] = 0.0f;

	count = 0;
}
