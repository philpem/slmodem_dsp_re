/*
 * x87copy.h -- copy a float without letting it through an x87 register.
 *
 * ---------------------------------------------------------------------------
 * THE ORIGINAL'S SOURCE SAID `*dst = *src;`.  THIS FILE EXISTS SO THAT OURS CAN
 * TOO, AND STILL BE CORRECT UNDER A MODERN COMPILER.
 *
 * The object was built by GCC 3.4.2 (finding F606), which lowers a `float`
 * assignment to a pair of integer `mov`s.  Modern GCC under `-mfpmath=387`
 * lowers the same statement to `flds`/`fstps`, and an x87 load-store QUIETENS A
 * SIGNALLING NaN: 0x7f800001 goes in and 0x7fc00001 comes out.  Every ordinary
 * value survives either way, so nothing but a signalling NaN can tell the two
 * apart -- and three reconstructions have been caught by one:
 *
 *     Queue<float>::write         the ring's element copy
 *     SineWave<float,float>       all four constructor stores
 *     Agc<float>::freeze          `savedAlpha = alpha`
 *
 * Under GCC 3.4 the plain assignment is measurably correct: compiled in
 * tools/toolchain's container and linked against the blob, t_queue, t_sinewave
 * and t_agc all pass with the natural form and no barrier at all.  So the
 * `#if` is not hedging -- the two branches were each run against the object.
 *
 * ---------------------------------------------------------------------------
 * Why an out-of-line byte copy is needed on the modern compiler
 *
 * GCC folds a memcpy between two same-typed pointers straight back into an
 * assignment and emits the x87 pair again.  The modern build uses ordinary
 * `memcpy` with `-fno-builtin-memcpy`, so the library copies the representation
 * without evaluating it as a floating-point value.  Copying through an unsigned
 * temporary also permits source and destination to overlap.  Issue #19's
 * crossed full-TU checks reject the builtin form on signalling NaNs and pass
 * the library-call form; see docs/issue19-inline-asm.md.
 *
 * The `__GNUC__ >= 4` test is on the MAJOR version because that is the boundary
 * that matters here: 3.4 is the original's compiler and everything since
 * behaves the other way.  A compiler that is neither gets the plain assignment,
 * which is the honest default -- it is what the source said.
 */

#ifndef DSPLIB_X87COPY_H
#define DSPLIB_X87COPY_H

#if defined(__GNUC__) && __GNUC__ >= 4
#include <string.h>
#endif

/**
 * @brief Copy `*src` to `*dst` without letting the value pass through an
 * x87 register.
 *
 * Under GCC >= 4 with `T` the same size as `unsigned` (the `float` case this
 * exists for), the bytes are moved through an unsigned
 * temporary instead of a plain assignment, which is what stops a modern
 * compiler's `flds`/`fstps` lowering from quietening a signalling NaN -- see
 * the file comment for the object's own GCC 3.4.2 behavior and the three
 * reconstructions this was found from. Under GCC 3 or any other compiler,
 * and for any other size, it is a plain `*dst = *src`, which is what the
 * object's own source said and is measurably correct there.
 *
 * @param dst Destination.
 * @param src Source.
 */
template <class T>
static inline void dsplib_assign(T *dst, const T *src)
{
#if defined(__GNUC__) && __GNUC__ >= 4
	if (sizeof(T) == sizeof(unsigned)) {
		unsigned tmp;

		memcpy(&tmp, src, sizeof(unsigned));
		memcpy(dst, &tmp, sizeof(unsigned));
		return;
	}
#endif
	*dst = *src;
}

#endif /* DSPLIB_X87COPY_H */
