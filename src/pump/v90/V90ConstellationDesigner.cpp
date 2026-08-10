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
/*
 * For `V90Parameters` -- the NAMED 0x558 map, not `V90PreFilter.h`'s 0x504
 * word block.  Two definitions of that class exist in this tree and no
 * translation unit may include both (finding 1112); this one takes the named
 * map, because the slot `reset` copies has a name in it and a numeric index
 * would throw that away.
 */
#include "dsplib/V90Parameters.h"
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

V90CD_OFF(params,    0x00, params);
V90CD_OFF(short_0a,  0x0a, short0a);
V90CD_OFF(short_0c,  0x0c, short0c);
V90CD_OFF(short_0e,  0x0e, short0e);
V90CD_OFF(short_10,  0x10, short10);
V90CD_OFF(word_24,   0x24, word24);
V90CD_OFF(word_48,   0x48, word48);
V90CD_OFF(maxRate, 0x4c, maxrate);
V90CD_OFF(minRate, 0x50, minrate);
typedef char v90cd_size[(sizeof(V90ConstellationDesigner) == 0x54) ? 1 : -1];
#endif

/*
 * reset -- seven stores, no branch, no call, no diagnostic.
 *
 * The whole body is `movl $0x0,0x48(%eax)`, four `movw $0x0` and two copies,
 * so the only thing that is not obvious from the disassembly is the widths,
 * and those are the store encodings: 0x48 and 0x24 are `movl`, the four at
 * +0x0a..+0x10 are `movw` with a `66` prefix.
 *
 * The parameter read is the last thing the object does and the FIRST thing
 * the compiler scheduled -- `mov (%eax),%ecx` is the second instruction --
 * which is register pressure and not statement order (CLAUDE.md's "free, so
 * ignore it").  The order below is the store order.
 */
void
V90ConstellationDesigner::reset()
{
	word_48 = 0;
	short_0a = 0;
	short_0c = 0;
	short_0e = 0;
	short_10 = 0;
	word_24 = params->unnamed_39c;
}

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
