/*
 * V90ConstellationDesigner.cpp -- the V.90 constellation designer's rate
 * limits.
 *
 * Reconstructed from dsplibs.o.  One of the class's twenty-two members:
 * `setMinMaxRates`, which is the only one `v34handshak` reaches.
 * `include/dsplib/V90ConstellationDesigner.h` carries the object map and the
 * evidence for it.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215):
 * `mov 0x10(%esp),%ebx` after one push and an eight-byte frame.  Nothing here
 * needs a calling-convention attribute.
 *
 * THE GATE IS TESTED TWICE.  The object compares `dsplibs_debug_level`
 * against 1 before the first diagnostic and AGAIN before the second, which is
 * what two separate `DSPLIB_DEBUG_ON()` sites compile to and what a single
 * `if` around both would not: `dsplibs_debug_printf` is an external call, so
 * the compiler must assume it can change the level.  For the same reason the
 * second diagnostic RELOADS `maxRate` from the object -- the argument
 * register did not survive the first call -- while the first uses the
 * argument still live in `%edx`.  Both spellings of the first are the same
 * code; `minRate` is written here because it is what the second must be.
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/V90ConstellationDesigner.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own -- and this is the check that catches an object right in size and
 * wrong in its offsets.  Skipped on the 64-bit `check64` pass, where a
 * 32-bit layout is not what the compiler lays out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90CD_OFF(field, off, tag) \
	typedef char v90cd_off_##tag[ \
	    ((int)__builtin_offsetof(V90ConstellationDesigner, field) \
	     == (off)) ? 1 : -1]

V90CD_OFF(maxRate, 0x4c, maxrate);
V90CD_OFF(minRate, 0x50, minrate);
typedef char v90cd_size[(sizeof(V90ConstellationDesigner) == 0x54) ? 1 : -1];
#endif

void
V90ConstellationDesigner::setMinMaxRates(unsigned int min, unsigned int max)
{
	minRate = min;
	maxRate = max;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner: set min rate to %d\r\n",
		    minRate);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner: set max rate to %d\r\n",
		    maxRate);
}
