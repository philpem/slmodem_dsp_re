/*
 * V92EchoCanceller.cpp -- moving the echo canceller's delay, clearing it, and
 * tearing it down.
 *
 * Reconstructed from dsplibs.o.  Three of the class's twelve members:
 * `setEchoDelay`, which is the one `v34handshak` reaches, `reset` and the
 * destructor.  `include/dsplib/V92EchoCanceller.h` carries the object map and
 * the argument that +0x2c is a tap count rather than a pointer.
 *
 * `reset` AND `~V92EchoCanceller` ARE BOTH 195 BYTES AND ARE NOT THE SAME
 * CODE.  Two functions of one class with one size is worth checking rather
 * than assuming, and the check says coincidence: `reset` opens `push %ebx;
 * xor %eax,%eax; sub $0x18,%esp` and ends in `jmp FloatARMA::reset`, while
 * the destructor opens `sub $0xc,%esp; cmpl $0x1,dsplibs_debug_level` and is
 * three conditional frees.  What IS duplicated is `D1` and `D2`, which are
 * byte-identical to each other 0xd0 apart -- and that pair is what a single
 * C++ destructor body produces for a class with no virtual bases, so one
 * definition here is right and no second spelling is needed.  Finding 1270.
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

extern "C" {
#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/sysdep.h"
}

#include "dsplib/FloatARMA.h"
#include "dsplib/V92EchoCanceller.h"
#include "dsplib/V92Parameters.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92EC_OFF(field, off, tag) \
	typedef char v92ec_off_##tag[ \
	    ((int)__builtin_offsetof(V92EchoCanceller, field) == (off)) \
	    ? 1 : -1]

V92EC_OFF(params,       0x00, params);
V92EC_OFF(arma,         0x04, arma);
V92EC_OFF(word_08,      0x08, word08);
V92EC_OFF(filterLength, 0x14, filterlength);
V92EC_OFF(echoCoeff,    0x20, echocoeff);
V92EC_OFF(echoHistory,  0x24, echohistory);
V92EC_OFF(historyIndex, 0x28, historyindex);
V92EC_OFF(echoLength,   0x2c, echolength);
V92EC_OFF(echoBeta,     0x30, echobeta);
V92EC_OFF(echoBetaDecay, 0x34, echobetadecay);
V92EC_OFF(echoDelay,    0x38, echodelay);
typedef char v92ec_size[(sizeof(V92EchoCanceller) == 0x3c) ? 1 : -1];

/* The one field of the parameter block this file reads. */
typedef char v92ec_off_delayoffset[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_DELAY_OFFSET) == 0x74)
    ? 1 : -1];
#endif

void
V92EchoCanceller::setEchoDelay(unsigned int delay)
{
	echoLength += delay - echoDelay;
	echoDelay = delay;

	edprintf("V92EchoCanceller: echoDelay updated to: %d\n", delay);
}

/*
 * Clear the canceller: both buffers, the write cursor, the tap count, the two
 * betas and the ARMA behind it.
 *
 * FOUR OF THIS CLASS'S OWN MEMBERS ARE INLINED HERE, AND THE FACTORING IS
 * RECORDED RATHER THAN REPRODUCED.  The first loop below is byte-for-byte the
 * body of `zeroEchoCoeff` (blob 0x10f60, 45 B); the block after it is
 * byte-for-byte the body of `resetEchoHistory` (0x10f90, 62 B), down to the
 * order of the `historyIndex` store and the `echoLength` recomputation; and
 * the two store-and-print pairs are `setEchoBeta(0.0f)` (0x10cc0) and
 * `setDecayFactor(0.0f)` (0x10d60) with their argument constant-folded --
 * `setEchoBeta` prints the sign of the FIELD it has just stored, so a zero
 * argument folds `fldz; fcomps 0x30(%ecx)` to false and the character to
 * `'-'`, and its `(int)fabs` and fractional terms to zero.  That is exactly
 * the `$0x2d, $0, $0` the object passes.
 *
 * None of the four is written in this tree, so spelling them as calls would
 * be spelling calls to nothing.  They are inlined instead: GCC inlined them
 * in the original too, so the emitted code is the same either way, and
 * CLAUDE.md's rule that a different factoring may differ for ever while
 * behaving identically is what makes that a choice rather than a compromise.
 * Finding 1271.
 *
 * THE SECOND LOOP IS D72's, AND THIS FILE DOES NOT CLAMP IT.  `echoLength` is
 * rebuilt from `filterLength`, `echoDelay` and the parameter block and used
 * as the bound on `echoHistory` with no reference to what that buffer was
 * allocated with.  D72 is the entry; it is CONFIRMED present and CANNOT FIRE
 * at any real `V92_ECHO_INITIAL_DELAY`, and the test that covers this
 * function puts a compared guard past the end of both buffers so that a
 * spelling which overran would fail rather than pass quietly.
 */
void
V92EchoCanceller::reset()
{
	unsigned int i;

	for (i = 0; i < filterLength; i++)
		echoCoeff[i] = 0.0f;

	historyIndex = 0;
	echoLength = (filterLength >> 1) + echoDelay
		     + (unsigned int)params->V92_ECHO_DELAY_OFFSET;
	for (i = 0; i < echoLength; i++)
		echoHistory[i] = 0.0f;

	word_08 = 0;

	echoBeta = 0.0f;
	edprintf("V92EchoCanceller: echoBeta = %c%d.%06d\r\n", '-', 0, 0);

	echoBetaDecay = 0.0f;
	edprintf("V92EchoCanceller: echoBetaDecay = %c%d.%06d\r\n", '-', 0, 0);

	arma->reset();
}

/*
 * Three owned blocks, each freed only if non-null and each nulled afterwards.
 *
 * THE GATE IS THE CALL SITE'S, NOT `edprintf`'s.  This is the one diagnostic
 * in the class that goes through `dsplibs_debug_printf` directly, so the
 * object tests `dsplibs_debug_level` itself -- `cmpl $0x1,...; ja` -- and the
 * message is plain text rather than encoded.  Every other diagnostic here is
 * an ungated `edprintf`, which self-gates one level down.
 *
 * THE ARMA IS DESTROYED AND THEN FREED SEPARATELY, which is what an explicit
 * destructor call followed by `sysdep_free` compiles to and is NOT what
 * `delete` compiles to: `delete` would call `operator delete`, and the object
 * calls `sysdep_free`.  Two calls, the same pointer in `%ebx` for both.
 */
V92EchoCanceller::~V92EchoCanceller()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92EchoCanceller Destruction\r\n");

	if (echoCoeff != NULL) {
		sysdep_free(echoCoeff);
		echoCoeff = NULL;
	}
	if (echoHistory != NULL) {
		sysdep_free(echoHistory);
		echoHistory = NULL;
	}
	if (arma != NULL) {
		arma->~FloatARMA();
		sysdep_free(arma);
		arma = NULL;
	}
}
