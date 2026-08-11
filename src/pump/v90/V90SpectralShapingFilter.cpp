/*
 * V90SpectralShapingFilter.cpp -- constructing the shaping filter.
 *
 * Reconstructed from dsplibs.o.  One of the class's five members: the
 * constructor.  `include/dsplib/V90SpectralShapingFilter.h` carries the
 * object map and the evidence for it.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * IT DOES NOT CALL `reset()` OR `setFilterCoeff()`.  Both are ordinary global
 * members and the object contains no call at all here, so the nine stores are
 * written out rather than delegated -- which is what -O2 without
 * `-finline-functions` gives for a member the compiler may not inline.
 *
 * THE COEFFICIENTS ARE CLEARED HIGH WORD FIRST, +0x0c then +0x08, +0x04,
 * +0x00, and the state low word first.  Right-to-left is what a chained
 * assignment evaluates to, so the coefficients are written as one here; it is
 * a store order of nine zeroes into nine distinct words, so no observer can
 * tell, and the object's order is recorded rather than relied on.
 */

#include <stddef.h>

#include "dsplib/V90SpectralShapingFilter.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SSF_OFF(field, off, tag) \
	typedef char v90ssf_off_##tag[ \
	    ((int)__builtin_offsetof(V90SpectralShapingFilter, field) \
	     == (off)) ? 1 : -1]

V90SSF_OFF(coeff,   0x00, coeff);
V90SSF_OFF(state,   0x10, state);
V90SSF_OFF(word_20, 0x20, word20);
typedef char v90ssf_size[(sizeof(V90SpectralShapingFilter) == 0x24) ? 1 : -1];
#endif

V90SpectralShapingFilter::V90SpectralShapingFilter()
{
	coeff[0] = coeff[1] = coeff[2] = coeff[3] = 0.0f;

	word_20 = 2;

	state[0] = 0.0f;
	state[1] = 0.0f;
	state[2] = 0.0f;
	state[3] = 0.0f;
}
