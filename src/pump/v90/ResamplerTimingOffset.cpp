/*
 * ResamplerTimingOffset.cpp -- the resampler's fixed timing offset.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/ResamplerTimingOffset.h`
 * carries the object map and the correction to where `ppmScale` and the
 * `double` really live; `dsplib/Resampler.h` carries the chain and the
 * vtables.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THE CLASS ADDS ONE FIELD AND ONE BEHAVIOUR.  The field is `timingOffset`;
 * the behaviour is the `timingCorrection` override, which is what turns the
 * base's empty per-output-sample hook into "advance the phase by a fixed
 * amount every sample".  Everything else here is plumbing around those two.
 *
 * ---------------------------------------------------------------------------
 * `setTimingOffset`, WHICH IS THE ONE `v34handshak` REACHES
 *
 * Twenty-one bytes, no frame, no call.  THE WHOLE FUNCTION IS FIVE
 * INSTRUCTIONS:
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
 *
 * ---------------------------------------------------------------------------
 * THE CONSTRUCTOR CALLS THE BASE'S `reset`, QUALIFIED
 *
 * `_ZN21ResamplerTimingOffsetC2Ejfjffj` forwards six of its seven arguments
 * to `Resampler` (all but the `ppm`), stores its own vptr, and then calls
 * `_ZN9Resampler5resetEv` -- the BASE's reset, by name, not this class's.
 * Two source spellings produce that and they behave identically: an explicit
 * `Resampler::reset()`, or an unqualified `reset()` that the compiler
 * devirtualises to `ResamplerTimingOffset::reset()` and inlines, whereupon
 * its `timingOffset = 0` is dead under the assignment that follows.  The
 * qualified form is written because it is what the object literally contains
 * and because the two classes below spell it that way of NECESSITY:
 * `ResamplerTiming` declares `reset(unsigned)`, which hides the nullary
 * `reset` for itself and everything under it, so `ResamplerTiming`'s
 * constructor and `V90Resampler`'s have no unqualified spelling available.
 */

#include <stddef.h>

#include "dsplib/ResamplerTimingOffset.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define RTO_OFF(field, off, tag) \
	typedef char rto_off_##tag[ \
	    ((int)__builtin_offsetof(ResamplerTimingOffset, field) == (off)) \
	    ? 1 : -1]

RTO_OFF(timingOffset, 0x48, timingoffset);
typedef char rto_size[(sizeof(ResamplerTimingOffset) == 0x4c) ? 1 : -1];
#endif

ResamplerTimingOffset::ResamplerTimingOffset(unsigned int nPhases, float scale,
					     unsigned int nTaps, float cutoff,
					     float ppm,
					     unsigned int minHistory)
	: Resampler(nPhases, scale, nTaps, cutoff, minHistory)
{
	Resampler::reset();
	setTimingOffset(ppm);
}

ResamplerTimingOffset::ResamplerTimingOffset(unsigned int nPhases, float scale,
					     unsigned int nTaps, float *bank,
					     float ppm,
					     unsigned int minHistory)
	: Resampler(nPhases, scale, nTaps, bank, minHistory)
{
	Resampler::reset();
	setTimingOffset(ppm);
}

/*
 * Nothing of this class's own to release -- the whole body is the vptr store
 * the compiler puts there and the base call.
 */
ResamplerTimingOffset::~ResamplerTimingOffset()
{
}

void
ResamplerTimingOffset::reset()
{
	Resampler::reset();
	timingOffset = 0;
}

/*
 * THE ARGUMENT IS IGNORED, and that is the object's own doing: the entire
 * body is `fldl 0xc(%eax) / fadds 0x48(%eax) / fstpl 0xc(%eax)` with no
 * reference to `0x8(%esp)`.  The base declares the parameter because the
 * class one level down uses it; here the correction is a constant rate.
 *
 * The addition is `double += float`, so the float widens and the sum is
 * kept at the accumulator's precision.
 */
void
ResamplerTimingOffset::timingCorrection(float)
{
	phase = phase + timingOffset;
}

void
ResamplerTimingOffset::setTimingOffset(float ppm)
{
	timingOffset = ppmScale * ppm * 1e-6f;
}

/*
 * The inverse of `setTimingOffset`, and it names the units: 1e6f comes from
 * .rodata.cst4+0x208 and is loaded with `flds`, so it is single precision
 * exactly as the 1e-6f above is.
 */
float
ResamplerTimingOffset::getTimingOffsetPPM() const
{
	return 1e6f * timingOffset / ppmScale;
}
