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

#include "dsplib/sysdep.h"
#include "dsplib/V90SdDetector.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SD_OFF(field, off, tag) \
	typedef char v90sd_off_##tag[ \
	    ((int)__builtin_offsetof(V90SdDetector, field) == (off)) ? 1 : -1]

V90SD_OFF(count,         0x00, count);
V90SD_OFF(limit,         0x04, limit);
V90SD_OFF(thresh_08,     0x08, thresh08);
V90SD_OFF(thresh_0c,     0x0c, thresh0c);
V90SD_OFF(value_10,      0x10, value10);
V90SD_OFF(history,       0x14, history);
V90SD_OFF(historyLength, 0x18, historylength);
typedef char v90sd_size[(sizeof(V90SdDetector) == 0x1c) ? 1 : -1];
#endif

/*
 * THE FOUR ARGUMENTS GO TO FOUR WORDS AND NOTHING IS COMPUTED FROM THEM.
 * Each is a 32-bit `mov` from the incoming stack slot into the object -- no
 * `flds`/`fstps` pair anywhere -- so the original COPIED the three floats
 * rather than converting them.
 *
 * OUR BUILD DOES NOT, AND THAT IS A TOOLCHAIN DIFFERENCE, NOT A DEFECT HERE.
 * The modern compiler renders the same assignment as `flds`/`fstps` under
 * `-mfpmath=387`, which is bit-exact for every float value including
 * denormals and quiet NaNs and quietens a SIGNALLING NaN.  Finding 1242
 * measures it and bounds it; the alternative is a bit-copy spelling chosen to
 * make the instruction match, which is fitting the compiler and is what the
 * codegen rule in CLAUDE.md forbids.
 *
 * THE LENGTH IS THE CONSTANT 12 AND SO IS THE ALLOCATION.  `movl $0xc` into
 * `historyLength` and `movl $0x30` as the allocator's argument: no argument
 * reaches either, so a fourth argument of 11 or 13 changes neither.  The
 * clearing loop then re-reads `historyLength` from the object, which is what
 * the reload after the call is, and is why it is written that way here.
 */
V90SdDetector::V90SdDetector(float thresh08, float thresh0c, float value10,
			     unsigned int limitArg)
{
	unsigned int i;

	thresh_08 = thresh08;
	thresh_0c = thresh0c;
	value_10 = value10;
	limit = limitArg;

	historyLength = 12;
	history = (float *)sysdep_malloc(historyLength * sizeof(float));

	for (i = 0; i < historyLength; i++)
		history[i] = 0.0f;

	count = 0;
}

/*
 * THE POINTER IS TESTED AND NOT NULLED.  `test %eax,%eax` / `jne` around the
 * one call, and nothing is written back, so a destroyed object still holds
 * the address it freed -- which is why the test checks the destructor through
 * the allocator rather than through the object.
 */
V90SdDetector::~V90SdDetector()
{
	if (history != 0)
		sysdep_free(history);
}

void
V90SdDetector::reset()
{
	unsigned int i;

	for (i = 0; i < historyLength; i++)
		history[i] = 0.0f;

	count = 0;
}
