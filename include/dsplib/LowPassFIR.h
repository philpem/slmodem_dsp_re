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
#include "dsplib/sysdep.h"

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

/*
 * Hold the compiler to the map in the header.
 */
#if __SIZEOF_POINTER__ == 4
typedef char lowpassfir_off_coefficients[
    ((int)__builtin_offsetof(LowPassFIR<float>, coefficients) == 0x00) ? 1 : -1];
typedef char lowpassfir_off_taps[
    ((int)__builtin_offsetof(LowPassFIR<float>, taps) == 0x04) ? 1 : -1];
typedef char lowpassfir_size[(sizeof(LowPassFIR<float>) == 0x08) ? 1 : -1];
#endif

/*
 * The primitive.  `window` may be null, in which case a Hamming window is
 * built in place of one.
 *
 * Returns 1 without touching a thing if the cutoff is out of range or fewer
 * than two taps were asked for, and 0 otherwise.  There is no other failure
 * path: NEITHER `sysdep_malloc` HERE IS CHECKED, so an allocation failure
 * calls `hamming(0, n)` or copies into a null pointer.  The four-argument
 * form below does check its own.
 */
template <typename T>
int LowPassFIR<T>::design(unsigned int nTaps, T cutoff, T gain,
			  const T *window, int adopt)
{
	unsigned int i;

	/*
	 * `fcoms 0.0f` + `jb`, so an unordered compare fails too: a NaN
	 * cutoff is rejected here and never reaches the second test.  Written
	 * as `!(cutoff >= 0)` because `cutoff < 0` would let the NaN through.
	 *
	 * -0.0f compares equal and is ACCEPTED.
	 */
	if (!(cutoff >= (T)0.0f))
		return 1;

	/*
	 * `seta` after `fcomps 1.0f`, or'd with `setbe` on `cmp $1,%ecx`.  So
	 * the cutoff is normalised to Nyquist -- 1.0 is fs/2, not fs -- and
	 * one tap is not a filter.
	 */
	if (cutoff > (T)1.0f || nTaps <= 1)
		return 1;

	taps = nTaps;

	/* The old array goes before the new one is decided on. */
	if (coefficients != 0)
		sysdep_free(coefficients);

	if (window == 0) {
		coefficients = (T *)sysdep_malloc(nTaps * sizeof(T));
		hamming(coefficients, nTaps);
	} else if (adopt == 0) {
		coefficients = (T *)sysdep_malloc(nTaps * sizeof(T));
		for (i = 0; i < taps; i++)
			coefficients[i] = window[i];
	} else {
		/* THE CONST IS CAST AWAY AND THE BUFFER IS ADOPTED. */
		coefficients = (T *)window;
	}

	{
		/*
		 * x = -(nTaps - 1) * cutoff / 2, at extended precision, and
		 * the count comes in through `fildll` with the high word
		 * zeroed -- x87 has no unsigned integer load.
		 *
		 * The association is the object's: (nTaps-1) * (-cutoff)
		 * first, then * 0.5f.  0.5 is `.rodata.cst4+0x1f0`, a float.
		 */
		long double x = (long double)(unsigned long long)(nTaps - 1)
			      * (long double)(-cutoff)
			      * (long double)0.5f;

		for (i = 0; i < taps; i++) {
			/*
			 * ROUNDED TO float EVERY ITERATION.  `fsts (%esp)`
			 * puts it in sinc's argument slot and `fstps
			 * 0x10(%esp)` in the slot the recurrence reloads --
			 * two 4-byte stores of the same register.  A `long
			 * double` running total would be a different filter.
			 */
			T xf = (T)x;
			long double t;

			t = (long double)sinc(xf) * (long double)cutoff;

			/* flds 0x10(%esp) ; fadds cutoff -- from the FLOAT. */
			x = (long double)xf + (long double)cutoff;

			/* One rounding, here, on (sinc*cutoff)*w[i]. */
			coefficients[i] =
			    (T)(t * (long double)coefficients[i]);
		}
	}

	{
		/*
		 * Normalise to the requested gain.  `sum` is the DspMath
		 * template and it is a real call, so its result arrives in
		 * st(0) unrounded; nothing stores it before the divide, so
		 * the quotient is taken at extended precision and stays there
		 * for the whole scaling loop.
		 *
		 * A zero cutoff makes every tap zero, so `sum` is zero and
		 * this is gain/0 = inf; the loop then writes 0*inf, the x87
		 * indefinite, into every tap.  That is what the blob does.
		 */
		long double scale = (long double)gain / sum(coefficients, taps);

		for (i = 0; i < taps; i++)
			coefficients[i] =
			    (T)((long double)coefficients[i] * scale);
	}

	return 0;
}

/*
 * The convenient form: build the window here and hand it over.
 *
 * THIS is the caller of `designWindow`, with the arguments in the object's
 * order -- (type, buffer, count) -- and it passes `adopt == 1` so the
 * primitive takes the buffer rather than copying it.
 *
 * IT LEAKS THE WINDOW WHENEVER THE PRIMITIVE REJECTS ITS ARGUMENTS: a bad
 * cutoff or `nTaps <= 1` returns 1 before the adopt, and nothing frees the
 * buffer.  `nTaps == 0` still calls `sysdep_malloc(0)` first.  No guard here
 * either; that is the original's shape.
 */
template <typename T>
int LowPassFIR<T>::design(unsigned int nTaps, T cutoff, WindowType type, T gain)
{
	T *window = (T *)sysdep_malloc(nTaps * sizeof(T));

	if (window == 0)
		return 1;

	designWindow(type, window, nTaps);

	return design(nTaps, cutoff, gain, window, 1);
}

/*
 * The whole constructor is a null store and a tail call.  It initialises
 * `coefficients` ONLY -- so if `design` rejects its arguments, `taps` is left
 * uninitialised, and the return value is discarded, so a caller cannot tell.
 */
template <typename T>
LowPassFIR<T>::LowPassFIR(unsigned int nTaps, T cutoff, WindowType type, T gain)
{
	coefficients = 0;
	design(nTaps, cutoff, type, gain);
}

/*
 * `sysdep_free`, and the pointer is NOT nulled afterwards.
 */
template <typename T>
LowPassFIR<T>::~LowPassFIR()
{
	delete[] coefficients;
}

#endif /* DSPLIB_LOWPASSFIR_H */
