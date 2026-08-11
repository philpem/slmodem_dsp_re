/*
 * V92ConvolutionEncoder.cpp -- the V.92 trellis encoder's constructor and
 * destructor, both of which are empty.
 *
 * Reconstructed from dsplibs.o.  Two of the class's six members;
 * `include/dsplib/V92ConvolutionEncoder.h` carries the object map and the
 * 0x2008 that `V92Transmitter::V92Transmitter` measures.
 *
 * THE CLAIM IS THAT THE ORIGINAL DECLARED THEM, not that they do anything.
 * GCC emits an out-of-line constructor or destructor symbol only for a
 * user-declared one, and the blob has C1, C2, D1 and D2, one byte each.  So
 * the source declared both and left the bodies empty, and the state the class
 * needs is set up by `reset(int)` instead.  t_v92convmapper.cpp carries the
 * only thing there is to check: that neither writes a byte of a seeded
 * object, over the whole 0x2008 and a guard past it.
 */

#include <stddef.h>

#include "dsplib/V92ConvolutionEncoder.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92CE_OFF(field, off, tag) \
	typedef char v92ce_off_##tag[ \
	    ((int)__builtin_offsetof(V92ConvolutionEncoder, field) == (off)) \
	    ? 1 : -1]

V92CE_OFF(mode,       0x0000, mode);
V92CE_OFF(state,      0x0004, state);
V92CE_OFF(transition, 0x0008, transition);
V92CE_OFF(output,     0x1008, output);
typedef char v92ce_size[(sizeof(V92ConvolutionEncoder) == 0x2008) ? 1 : -1];
#endif

V92ConvolutionEncoder::V92ConvolutionEncoder()
{
}

V92ConvolutionEncoder::~V92ConvolutionEncoder()
{
}
