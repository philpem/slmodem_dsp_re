/*
 * x87copy.h -- copy a float without letting it through an x87 register.
 *
 * ---------------------------------------------------------------------------
 * THE ORIGINAL'S SOURCE SAID `*dst = *src;`.  THIS FILE EXISTS SO THAT OURS CAN
 * TOO, AND STILL BE CORRECT UNDER A MODERN COMPILER.
 *
 * The object was built by GCC 3.4.2 (finding 606), which lowers a `float`
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
 * Why `__builtin_memcpy` alone is not enough
 *
 * GCC folds a memcpy between two same-typed pointers straight back into an
 * assignment and emits the x87 pair again.  The empty `asm` makes the value
 * opaque and pins it in a general register.  It generates no instruction of its
 * own; it only denies the compiler the knowledge that the bytes are a float.
 *
 * The `__GNUC__ >= 4` test is on the MAJOR version because that is the boundary
 * that matters here: 3.4 is the original's compiler and everything since
 * behaves the other way.  A compiler that is neither gets the plain assignment,
 * which is the honest default -- it is what the source said.
 */

#ifndef DSPLIB_X87COPY_H
#define DSPLIB_X87COPY_H

template <class T>
static inline void dsplib_assign(T *dst, const T *src)
{
#if defined(__GNUC__) && __GNUC__ >= 4
	if (sizeof(T) == sizeof(unsigned)) {
		unsigned tmp;

		__builtin_memcpy(&tmp, src, sizeof(unsigned));
		__asm__("" : "+r" (tmp));
		__builtin_memcpy(dst, &tmp, sizeof(unsigned));
		return;
	}
#endif
	*dst = *src;
}

#endif /* DSPLIB_X87COPY_H */
