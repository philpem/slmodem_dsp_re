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

#include <math.h>
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
		 * The object computes the sine with `fsin`, the
		 * x87 instruction, and libm's is a different function -- they
		 * agree to well within a float almost everywhere and disagree
		 * exactly where it matters here, at integer `x`, where
		 * sin(pi*x) is near zero and the relative error is all there
		 * is.  49 of 4019 swept points differed, every one of them at
		 * or beside an integer.
		 *
		 * Ordinary sin/cos calls expand through the period compiler and
		 * math header under the C++ source fast-math flags.  This does not
		 * uniquely recover the original flags.  Modern portability failures
		 * are a separate, forthcoming issue; see docs/issue19-inline-asm.md.
		 */
		s = sin(y);

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
 * ---------------------------------------------------------------------------
 * The cosine windows.
 *
 * Both were decoded from their own `.gnu.linkonce.t.*` section and verified
 * bit-exact against the blob over every n from 0 to 256 -- and out to
 * 1,000,003 for `hamming` -- comparing the WHOLE buffer, so a version writing
 * one element too many fails.
 *
 * THE TWO DENOMINATORS ARE DIFFERENT AND NEITHER IS THE TEXTBOOK ONE for the
 * other:
 *
 *     hanning   over n + 1, and indexed from 1
 *     hamming   over n - 1, and indexed from 0
 *
 * so `hanning` is the strict interior of an (n+2)-point Hann -- both zero
 * endpoints excluded, which is why w[0] is never 0 -- while `hamming` is the
 * ordinary symmetric form.  They are not two spellings of one loop and must
 * not be merged into one.
 */

template <typename T>
DSPMATH_STEP void hanning(T *w, unsigned n)
{
	if (n == 0)
		return;

	{
		/*
		 * `(n + 1u)` in 32 bits and THEN widened, so n == 0xffffffff
		 * wraps to zero and divides by it, exactly as the object's
		 * `lea 0x1(%esi)` does.  `(unsigned long long)n + 1` would
		 * quietly not.
		 */
		long double inv = 1.0L / (long double)(unsigned long long)(n + 1u);
		unsigned i;

		for (i = 1; i <= n; i++) {
			long double x = (long double)(unsigned long long)i;
			long double c;

			/*
			 * A DOUBLE, and the pool says so: the object loads
			 * this with `fldl` out of `.rodata.cst8+0x38`, which
			 * the comment beside it has always recorded.  Spelt
			 * `6.283185307179586L` the constant becomes XFmode,
			 * moves to `.rodata.cst16` and is loaded with `fldt`
			 * -- same six bytes, different section, and the
			 * relocation target is what gives it away.  Lever 11.
			 */
			x = x * 6.283185307179586;	/* .rodata.cst8+0x38 */
			x = x * inv;
			c = cosl(x);

			/* .rodata.cst4+0x1dc is 0.5f, a FLOAT here. */
			w[i - 1] = (T)((1.0L - c) * (long double)0.5f);
		}
	}
}

/*
 * NO `if (n == 0) return;`, AND THE RECIPROCAL IS INSIDE THE LOOP.  Both are
 * read off the object rather than chosen, and together they take this from 44
 * differing bytes of 104 to one.
 *
 * The guard: the object's only entry test is `xor %ecx,%ecx; cmp %ebx,%ecx;
 * jae`, which is the `for` loop's own condition.  An explicit `n == 0` guard
 * compiles to a `test %ebx,%ebx; je` IN FRONT of that -- GCC 3.4.2 does not
 * fold the two -- so the guard's presence is directly observable and it is
 * absent.  With `i` unsigned and `i < n`, the loop is its own guard.
 *
 * The reciprocal's PLACEMENT: written before the loop, the divide is emitted
 * before the guard branch, because that is where the statement is.  The object
 * divides AFTER it, in the loop preheader, alongside the three constant loads
 * -- which is where loop-invariant motion puts a computation that was written
 * inside the body.  So `d` is a body-local that GCC hoists, not a preheader
 * local that GCC sinks.  Four cells (guard x placement) and exactly one maps
 * onto the object, so this decodes a fact and not a text: any spelling that
 * leaves the divide loop-invariant INSIDE the body will do, `d` declared here
 * or the reciprocal written into the multiply.  Finding F8040.
 *
 * n == 1 DIVIDES BY ZERO and the object really does it: 1/(n-1) is +inf,
 * 0 * inf is the x87 indefinite, and w[0] comes out 0xffc00000.  Reproduced
 * bit for bit.  A one-tap window from this routine is garbage, and that is the
 * original's behaviour rather than an artefact here.  Note this is exactly why
 * the n == 0 guard could be dropped without the n == 1 one appearing: they are
 * different questions, and only the first is answered by the loop.
 *
 * THE ONE BYTE THAT IS LEFT is `pop %eax` against our `pop %ecx` -- the dummy
 * pop that undoes `sub $0x4,%esp`.  That register comes from `peephole2`'s
 * `match_scratch` and `peep2_find_free_register`'s round-robin cursor, which
 * is threaded through a TRANSLATION UNIT in emission order (lever 3b).  The
 * object's unit is not this one: its `.gnu.linkonce.t` run interleaves these
 * templates with `LowPassFIR<float>` and `Resampler::timingCorrection`, so the
 * cursor arrived at `hamming` having been advanced by code that is not in
 * `DspMath.cpp` at all.  Finding F8042.
 */
template <typename T>
DSPMATH_STEP void hamming(T *w, unsigned n)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		long double x = (long double)(unsigned long long)i;
		long double c;
		long double d = 1.0 / (long double)(unsigned long long)(n - 1);

		x = x * 6.283185307179586;	/* .rodata.cst8+0x40 */
		x = x * d;			/* a reciprocal MULTIPLY */
		c = cosl(x);

		/* 0.54 and 0.46, .rodata.cst8+0x50 and +0x48. */
		w[i] = (T)(0.54 - c * 0.46);
	}
}


template <typename T>
DSPMATH_STEP void blackman(T *w, unsigned n)
{
	if (n == 0)
		return;

	{
		/*
		 * TWO cosine terms, at 2pi and 4pi, and the weights are not
		 * all the same type: 0.42 and 0.08 are doubles in
		 * .rodata.cst8 (+0x60, +0x70) and 0.5 is the object's ONLY
		 * single-precision constant, .rodata.cst4+0x1e0, loaded with
		 * `flds`.  Writing 0.5 rather than 0.5f changes the answer.
		 *
		 * n == 1 divides by zero and writes the x87 indefinite,
		 * 0xffc00000, exactly as `hamming` does.  No guard: adding one
		 * breaks the match.
		 *
		 * THE ENDPOINTS ARE NOT ZERO.  w[0] and w[n-1] come out
		 * 0xa3800000, about -1.39e-17, because 0.42 + 0.08 is not
		 * exactly 0.5 in binary.  An implementation that tidies them
		 * to 0.0f fails.
		 *
		 * THIS ONE IS STILL FOUR BYTES OFF AND THE LOOP BODY IS NOT
		 * WHY -- 40 spellings say so, and the count is the point.
		 * The object holds nine `fxch` where we hold three and spend
		 * `fldt`/`fstpt` twice over a 0x14 frame against its 0x4; the
		 * 0x4 is ALIGNMENT (push, push, sub 4 lands esp at 0 mod 16)
		 * and not a spill slot, so there is no narrow spill to read a
		 * type off -- lever 12's premise fails here rather than its
		 * conclusion.  Crossing {`x` as `(float)i` or via `unsigned
		 * long long`} x {the reciprocal likewise} x {before the loop,
		 * first in the body, or between the angles} x {four ways of
		 * pairing the two angle multiplies}, plus the guard on and
		 * off, gives 40 cells: SIX distinct emissions, ALL of them
		 * 156 bytes, none of them 152.  No preimage, so by lever 0
		 * the difference is not the loop body's statement order, its
		 * angle factoring, or those two conversions -- it is upstream
		 * of the text, and dropping the guard alone would move this
		 * from 60 instructions to 58 and further away.  Do not spend
		 * another pass inside these braces.  Finding F8041.
		 */
		long double d = 1.0 / (float)(n - 1);
		unsigned i;

		for (i = 0; i < n; i++) {
			long double x = (float)i;
			long double a1 = x * 6.283185307179586 * d;
			long double a2 = x * 12.566370614359172 * d;
			long double c1, c2;

			c1 = cosl(a1);
			c2 = cosl(a2);

			w[i] = (T)(0.42 - c1 * 0.5f + c2 * 0.08);
		}
	}
}

/*
 * The selector, and its four arms are READ FROM THE SWITCH rather than
 * assumed: case 1 is `hanning` and case 2 is `hamming`, which is the opposite
 * way round from the alphabetical guess.
 *
 * The object reaches `boxcar` two ways -- case 0 tail-jumps to it and the
 * default CALLs it and returns -- which is a layout artefact of the compiler's
 * switch and not a behavioural difference.  One `default` covers both.
 */
template <typename T>
void designWindow(WindowType type, T *w, unsigned n)
{
	switch (type) {
	case WINDOW_HANNING:
		hanning(w, n);
		break;
	case WINDOW_HAMMING:
		hamming(w, n);
		break;
	case WINDOW_BLACKMAN:
		blackman(w, n);
		break;
	default:
		boxcar(w, n);
		break;
	}
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
template void hanning<float>(float *, unsigned);
template void hamming<float>(float *, unsigned);
template void blackman<float>(float *, unsigned);
template void designWindow<float>(WindowType, float *, unsigned);
