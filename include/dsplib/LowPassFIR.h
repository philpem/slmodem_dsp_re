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

	/**
	 * @brief Construct and design a filter in one step.
	 *
	 * Sets `coefficients` to NULL, then calls the four-argument design()
	 * and discards its result -- so if design() rejects its arguments,
	 * `taps` is left uninitialised and the caller has no way to tell.
	 *
	 * @param nTaps   Number of taps.
	 * @param cutoff  Normalised cutoff, 0..1 (1.0 is Nyquist, fs/2).
	 * @param type    Window shape to build internally.
	 * @param gain    Passband gain to normalise to.
	 */
	LowPassFIR(unsigned int nTaps, T cutoff, WindowType type, T gain);

	/** @brief Free `coefficients` (as `delete[]`; the pointer is not cleared). */
	~LowPassFIR();

	/**
	 * @brief Build a window internally, then design the filter with it.
	 *
	 * Allocates an `nTaps`-entry window, fills it via designWindow(),
	 * and forwards to the five-argument design() with `adopt = 1` so
	 * ownership passes to the object. Leaks the window buffer if the
	 * five-argument form rejects its arguments (bad cutoff or
	 * `nTaps <= 1`) -- the object's own shape, reproduced here.
	 *
	 * @param nTaps   Number of taps.
	 * @param cutoff  Normalised cutoff, 0..1.
	 * @param type    Window shape to build.
	 * @param gain    Passband gain to normalise to.
	 * @return 0 on success, 1 if @p cutoff is out of range, NaN, or
	 *         @p nTaps <= 1 (the window is still allocated and leaked
	 *         in that case).
	 */
	int design(unsigned int nTaps, T cutoff, WindowType type, T gain);

	/**
	 * @brief Design a windowed-sinc low-pass filter from a given window.
	 *
	 * The primitive both other entry points funnel into. Replaces any
	 * existing `coefficients` (freed first), then fills the new array
	 * with a windowed sinc response and normalises it to @p gain.
	 * Neither `sysdep_malloc` call here is checked -- an allocation
	 * failure feeds a null buffer onward rather than returning early.
	 *
	 * @param nTaps   Number of taps.
	 * @param cutoff  Normalised cutoff, 0..1 (1.0 is Nyquist, fs/2); a
	 *                NaN or negative value is rejected without touching
	 *                the object's state.
	 * @param gain    Passband gain to normalise to. A cutoff of exactly
	 *                0 makes every tap's sum zero, so the normalisation
	 *                divides by zero and fills every tap with the x87
	 *                indefinite -- the object's own behaviour.
	 * @param window  Window to apply, `nTaps` entries, or NULL to build
	 *                a Hamming window internally.
	 * @param adopt   0: copy @p window into a freshly allocated array.
	 *                Non-zero: store @p window itself in `coefficients`
	 *                (the `const` is cast away) and take ownership --
	 *                the destructor will free it.
	 * @return 0 on success, 1 if @p cutoff is out of range/NaN or
	 *         @p nTaps <= 1 (nothing is touched in that case).
	 */
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
