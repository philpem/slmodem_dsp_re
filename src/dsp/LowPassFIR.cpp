/*
 * LowPassFIR.cpp -- the windowed-sinc low-pass FIR designer.
 * See dsplib/LowPassFIR.h for the object map.
 *
 * ---------------------------------------------------------------------------
 * THE FIVE-ARGUMENT `design` DOES NOT CALL `designWindow`.  The FOUR-argument
 * one does.  The five-argument form is the primitive: it takes a window that
 * is already built, and on the one path where it has none -- `window == 0` --
 * it calls `hamming<float>` DIRECTLY, not `designWindow(WINDOW_HAMMING, ...)`.
 * The relocation at +0x167 names `_Z7hammingIfEvPT_j`; there is no
 * `designWindow` relocation in that section at all.
 *
 * ---------------------------------------------------------------------------
 * `adopt`, and why `const T *window` is a lie
 *
 * The fifth argument selects between two ways of taking the window:
 *
 *     adopt == 0   sysdep_malloc a new array and copy `window` into it
 *     adopt != 0   store `window` itself in `coefficients`
 *
 * and in the second case the object then *writes through it* and, later,
 * `sysdep_free`s it from the destructor.  So a non-zero `adopt` hands
 * ownership of the caller's buffer to the object.  The parameter really is
 * `const T *` in the mangling (`PKf`) and the object really does cast the
 * const away.  The four-argument form is the only in-tree caller and it
 * passes `adopt == 1` on a buffer it has just allocated for the purpose.
 *
 * ---------------------------------------------------------------------------
 * x87 EXTENDED PRECISION, and where it is and is not allowed to persist
 *
 * The running sinc argument `x` is NOT a `long double` chain: the object
 * stores it with `fsts`/`fstps` -- 4 bytes -- once per iteration, both as the
 * argument to `sinc` and to the slot it reloads for the recurrence.  So `x`
 * is rounded to `float` every step, and only the *initial* value is computed
 * at extended precision.  `xf` below is that float and must genuinely be one.
 *
 * AND IT IS ONE BECAUSE OF THE CALL, NOT BECAUSE OF THE `(T)`.  Under
 * -mfpmath=387 with the default -fexcess-precision=fast, GCC DISCARDS an
 * explicit narrowing cast of a value living in an x87 register: two mutation
 * runs that added `(T)` narrowings elsewhere in this file compiled to
 * byte-identical code.  What forces the rounding here is that `xf` is passed
 * by value to `sinc(float)`, which is a real out-of-line call, so it must be
 * materialised in a 4-byte argument slot, and the call clobbers the x87 stack
 * so the recurrence has to reload it from there.  Verified in the emitted
 * code: `fstps 0x10(%esp)` / `flds 0x10(%esp)` around the call.
 *
 * That is a codegen dependency.  If a future compiler spills `xf` as a 10-byte
 * extended value instead, this becomes a `long double` recurrence and about
 * one design in five diverges -- the failure signature to look for.  Only
 * `volatile` states the intent in a way the compiler may not undo.
 *
 * Everything else stays 80-bit: `sinc(x) * fc * w[i]` rounds once, at the
 * `fstps` into the array, and the normalising `gain / sum(...)` is never
 * narrowed -- there is no store between the `call sum` and the `fdivrp`.
 *
 * FDIVRP, NOT FDIVP.  objdump prints `de f1` as `fdivp %st,%st(1)`, but
 * `DE F0+i` is FDIVRP: ST(1) = ST(0)/ST(1).  With `gain` pushed on top of
 * `sum`'s result the quotient is `gain / sum`, which is also the only reading
 * that makes the routine a filter designer.  Read the bytes, not the mnemonic.
 */

#include "dsplib/LowPassFIR.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *ptr);
}

/*
 * THE REPLACEMENT `operator delete[]`, AND IT IS READ OFF THE OBJECT.  At a
 * destructor's LAST free the blob makes an ordinary `call sysdep_free` where
 * our explicit `if (p) sysdep_free(p)` makes a sibling `jmp` -- one
 * instruction fewer, and the sibcall drops the frame with it.  Eight spellings
 * were compiled and only `delete[]` reproduces the object's shape; finding
 * F7786 and `docs/method/refinement.md` lever 7 carry the enumeration.
 *
 * Behaviourally it is exactly the guard it replaces: the element type is a POD
 * with no destructor, so `delete[] p` is `if (p) operator delete[](p)` and
 * there is no array cookie to read.
 *
 * ONLY THE LAST FREE IN A DESTRUCTOR IS BYTE-EVIDENCE for this.  Away from
 * tail position the two spellings emit identically, so the others carry the
 * same spelling because a destructor written with `delete[]` uses it for every
 * member, not because the object distinguishes them.
 *
 * IT IS A LOCAL COPY AND NOT AN INCLUDE ON PURPOSE.  Hoisting this one
 * definition into `dsplib/sysdep.h` -- which every one of these files already
 * reaches transitively -- moved it earlier in the translation unit and cost
 * EIGHT destructors their byte identity, `FloatFIR` and `FloatARMA` among
 * them.  That is refinement.md lever 3 with an inline function as the carrier,
 * and finding F7815 is the measurement.
 */
inline void operator delete[](void *p) { sysdep_free(p); }

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

template class LowPassFIR<float>;
