/*
 * LowPassFIR.h -- the windowed-sinc low-pass FIR designer.
 *
 * Reconstructed from dsplibs.o, four weak symbols each in its own
 * `.gnu.linkonce.t.*` section:
 *
 *   _ZN10LowPassFIRIfE6designEjffPKfi          373 bytes
 *   _ZN10LowPassFIRIfE6designEjf10WindowTypef  106 bytes
 *   _ZN10LowPassFIRIfED1Ev                      29 bytes  (== D2)
 *   _ZN10LowPassFIRIfEC1Ejf10WindowTypef        15 bytes  (== C2)
 *
 * THIS CLASS DESIGNS A FILTER, IT DOES NOT RUN ONE.  There is no `process`,
 * no history buffer, no `reset` -- the object owns a coefficient array and a
 * tap count and nothing else.  Whatever ran the filter took the array from
 * here; `FloatFIR` is the shape that does the convolution.
 *
 * NOT POLYMORPHIC.  `nm` lists D1/D2 and C1/C2 and no deleting destructor
 * `D0`, which GCC emits only for a virtual one, and the constructor's whole
 * body is `movl $0x0,(%edx)` -- offset 0 is a real member, there is no vptr.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument (`mov 0x30(%esp),%edi` after three pushes and a 32-byte frame),
 * not %ecx.
 */

#ifndef DSPLIB_LOWPASSFIR_H
#define DSPLIB_LOWPASSFIR_H

#include "dsplib/DspMath.h"

/*
 * The original instantiates this at `float` and at nothing else.
 */
template <typename T>
class LowPassFIR {
public:
	/*
	 * The signatures are the mangling's, so they are a specification and
	 * not a guess.  A return type is never mangled, but both `design`
	 * overloads leave 0 or 1 in %eax on every path, so both are `int`.
	 */
	LowPassFIR(unsigned int nTaps, T cutoff, WindowType type, T gain);
	~LowPassFIR();

	int design(unsigned int nTaps, T cutoff, WindowType type, T gain);
	int design(unsigned int nTaps, T cutoff, T gain, const T *window,
		   int adopt);

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling, and because one access section is
	 * what keeps the class standard-layout so `__builtin_offsetof` is well
	 * defined -- the .cpp asserts both offsets.  The names are invented;
	 * the mangling never carries a data member's name.
	 *
	 * These are the ONLY two displacements that appear anywhere in the
	 * four methods.  A member the four never touch would be invisible, so
	 * `sizeof` is a lower bound of 8 and not a measurement.
	 */
	T *coefficients;	/* +0x00 sysdep_malloc'd or adopted, `taps` long */
	unsigned int taps;	/* +0x04 compared with ja/setbe, so unsigned    */
};

#endif /* DSPLIB_LOWPASSFIR_H */
