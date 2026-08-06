/*
 * DspMath.cpp -- the float statistics templates.  See dsplib/DspMath.h.
 *
 * Reconstructed from dsplibs.o, one weak symbol per `.gnu.linkonce.t.*`
 * section:
 *
 *   _Z3sumIfET_PS0_j       28 bytes    _Z6sqrSumIfET_PS0_j   46 bytes
 *   _Z4meanIfET_PS0_j      38 bytes    _Z3VarIfET_PS0_j      69 bytes
 *   _Z3StdIfET_PS0_j       29 bytes    _Z4sincIfET_S0_       46 bytes
 *   _Z6boxcarIfEvPT_j      29 bytes
 *
 * The four window shapes and `designWindow` are NOT here: `designWindow`
 * tail-calls all four, so it cannot land before them, and the three cosine
 * windows need their x87 stack read out carefully.  This batch is closed
 * without them -- the five statistics call only each other, and `sinc` and
 * `boxcar` are leaves.
 */

#include "dsplib/DspMath.h"

/*
 * NOINLINE, AND IT IS NOT A STYLE CHOICE.
 *
 * Each of these is a separate weak symbol in the object and they call one
 * another for real -- `Var` has call relocations to `sqrSum` and to `mean`.
 * A call forces the callee's result out of st(0) and back through memory as a
 * float, and that rounding is part of the answer: allowed to inline, GCC
 * folds all three into one loop that never leaves extended precision, and
 * `Var` comes out one ulp different on about one input in seven.
 *
 * So the attribute reproduces the original's call structure, which is what
 * produces the original's rounding.
 */
#define DSPMATH_STEP __attribute__((noinline))

/*
 * x87 THROUGHOUT, and that is the point of writing these as the original did
 * rather than more directly.  Every accumulator below stays in a register at
 * 80-bit extended precision and rounds to `float` exactly once, where the
 * value is returned or stored.  Adding a `float` temporary anywhere in the
 * chain would round early and change the answer.
 */

template <typename T>
DSPMATH_STEP T sum(T *x, unsigned n)
{
	T acc = 0;

	/*
	 * `while (n--)` and not `for (i = 0; i < n; i++)`: the object counts
	 * down and leaves the counter at -1, and with `n == 0` the unsigned
	 * wrap means zero iterations in both.
	 */
	while (n--)
		acc += *x++;

	return acc;
}

template <typename T>
DSPMATH_STEP T mean(T *x, unsigned n)
{
	/*
	 * The count is converted through a 64-bit signed load -- `fildll` with
	 * the high word zeroed -- because x87 has no unsigned integer load.
	 * Writing `(T)n` produces exactly that.
	 */
	return sum(x, n) / (T)n;
}

/*
 * NOT a sum of squares.  It divides by `n` before returning, so it is the
 * mean of the squares, and `Var` below is the textbook E[x^2] - E[x]^2.  The
 * name is the original's.
 */
template <typename T>
DSPMATH_STEP T sqrSum(T *x, unsigned n)
{
	T acc = 0;
	unsigned k = n;

	while (k--) {
		T v = *x++;

		acc += v * v;
	}

	return acc / (T)n;
}

template <typename T>
DSPMATH_STEP T Var(T *x, unsigned n)
{
	/*
	 * Two passes over the array, because that is what the object does: it
	 * calls `sqrSum` and then `mean` rather than accumulating both at
	 * once.  A single-pass version is the same algebra and a different
	 * sequence of roundings.
	 *
	 * The intermediate IS rounded to float -- the object stores the
	 * sqrSum result with `fstps` before calling `mean`, because the call
	 * would otherwise clobber the register.  So this one narrowing is the
	 * original's and not an accident of writing it in C++.
	 */
	T s = sqrSum(x, n);
	T m = mean(x, n);

	/*
	 * THE ASYMMETRY IS THE OBJECT'S.  `sqrSum`'s result IS rounded to
	 * float -- `fstps` before the second call, because the call would
	 * clobber st(0) -- and the mean is NOT, because nothing follows it.
	 * So the square is taken at extended precision on an unrounded mean
	 * and subtracted from a rounded sum.
	 *
	 * Both are written as plain `T` here: the noinline above is what keeps
	 * the calls, and the calls are what produce the rounding.
	 */
	return s - m * m;
}

template <typename T>
DSPMATH_STEP T Std(T *x, unsigned n)
{
	return __builtin_sqrt(Var(x, n));
}

/*
 * The normalised sinc, sin(pi*x) / (pi*x), with the removable singularity
 * answered as 1.
 *
 * The comparison is against 0.0f exactly and is the object's own: a value one
 * ulp from zero takes the general path and divides by it.  Reproduced.
 */
template <typename T>
T sinc(T x)
{
	if (x == (T)0)
		return (T)1;

	{
		/*
		 * pi is loaded as a DOUBLE and the multiply happens at
		 * extended precision, so the argument to the sine is not the
		 * float product.
		 */
		double y = 3.141592653589793 * x;
		double s;

		/*
		 * `fsin`, not `sin()`.  The object computes the sine with the
		 * x87 instruction, and libm's is a different function -- they
		 * agree to well within a float almost everywhere and disagree
		 * exactly where it matters here, at integer `x`, where
		 * sin(pi*x) is near zero and the relative error is all there
		 * is.  49 of 4019 swept points differed, every one of them at
		 * or beside an integer.
		 *
		 * GCC emits `fsin` for `__builtin_sin` only under
		 * -funsafe-math-optimizations, which would change every other
		 * float in this file.  One instruction of asm is the smaller
		 * and more honest change.
		 */
		__asm__ ("fsin" : "=t" (s) : "0" (y));

		return (T)(s / y);
	}
}

template <typename T>
void boxcar(T *w, unsigned n)
{
	unsigned i;

	for (i = 0; i < n; i++)
		w[i] = (T)1;
}

/*
 * The original instantiates every one of these at `float` and at nothing
 * else, and emits them weak.  Naming them here is what makes the compiler
 * emit ours the same way, under the same mangled names.
 */
template float sum<float>(float *, unsigned);
template float mean<float>(float *, unsigned);
template float sqrSum<float>(float *, unsigned);
template float Var<float>(float *, unsigned);
template float Std<float>(float *, unsigned);
template float sinc<float>(float);
template void boxcar<float>(float *, unsigned);
