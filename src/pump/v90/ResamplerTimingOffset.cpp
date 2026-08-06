/*
 * ResamplerTimingOffset.cpp -- setting the resampler's timing offset.
 *
 * Reconstructed from dsplibs.o.  One of the class's seven members:
 * `setTimingOffset`, which is the one `v34handshak` reaches.
 * `include/dsplib/ResamplerTimingOffset.h` carries the object map, the
 * evidence that offset 0 is a vptr, and why it is modelled as a field rather
 * than declared `virtual`.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).  Twenty-one
 * bytes, no frame, no call: this member does not dispatch through the vptr
 * and does not reach the `Resampler` base, even though `reset()` and the
 * destructor next to it do.
 *
 * THE WHOLE FUNCTION IS FIVE INSTRUCTIONS:
 *
 *     flds  0x2c(%eax)             ppmScale
 *     fmuls 0x8(%esp)              * the argument
 *     fmuls .rodata.cst4+0x1f4     * 0x358637bd
 *     fstps 0x48(%eax)             -> timingOffset
 *
 * 0x358637bd IS 1e-6f AND THE `f` IS LOAD-BEARING.  `fmuls` is a
 * SINGLE-precision load, so the constant is the float nearest 1e-6 and not
 * the double; writing `1e-6` here would emit `fmull` against a different
 * value and differ in the last bit of most results.  tools/tabdump.py reads
 * the four bytes back as 9.99999997e-07f, which is that float printed.
 *
 * THE INTERMEDIATE IS NOT ROUNDED, and the Makefile is where that is
 * arranged.  The object is `-mfpmath=387` and multiplies twice on the x87
 * stack before the single `fstps`, so `ppmScale * arg` keeps 80-bit extended
 * precision and only the final result is rounded to `float`.  The tree
 * compiles the same way and deliberately does NOT pass `-ffloat-store`, which
 * would round the intermediate product as well -- the Makefile says so where
 * it sets CXXFLAGS, and src/dsp/FloatIIR.cpp is the precedent.  Left to
 * right is the order the object multiplies in and the order C++ evaluates
 * `a * b * c`, so the expression below needs no parentheses to match.
 */

#include <stddef.h>

#include "dsplib/ResamplerTimingOffset.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define RTO_OFF(field, off, tag) \
	typedef char rto_off_##tag[ \
	    ((int)__builtin_offsetof(ResamplerTimingOffset, field) == (off)) \
	    ? 1 : -1]

RTO_OFF(vptr,         0x00, vptr);
RTO_OFF(ppmScale,     0x2c, ppmscale);
RTO_OFF(timingOffset, 0x48, timingoffset);
typedef char rto_size[(sizeof(ResamplerTimingOffset) == 0x4c) ? 1 : -1];
#endif

void
ResamplerTimingOffset::setTimingOffset(float ppm)
{
	timingOffset = ppmScale * ppm * 1e-6f;
}
