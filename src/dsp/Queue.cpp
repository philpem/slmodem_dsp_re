/*
 * Queue.cpp -- see dsplib/Queue.h.
 *
 * Reconstructed from six weak sections, 587 bytes, and verified over 699,510
 * comparison points: 3 seeds x 13 sizes x 400 random 40-operation sequences,
 * an exhaustive wrap sweep over size x phase x fill x count, and a
 * huge-`num` sweep up to 0xffffffff.  Every point compares the return value,
 * all five object words with the pointers normalised against each side's own
 * arena, the ENTIRE backing store byte for byte, and the whole destination
 * buffer as bits.
 */

#include "dsplib/Queue.h"

extern "C" void *sysdep_malloc(unsigned size);
extern "C" void sysdep_free(void *p);

/*
 * THE COPY GOES THROUGH AN INTEGER, and that is not tidiness -- it is the
 * only way to match.
 *
 * The object copies elements with `movl`.  Modern GCC compiles `*q++ = *p++`
 * on a `float` under `-mfpmath=387` into `flds`/`fstps`, and an x87 load-store
 * QUIETENS A SIGNALLING NaN: 0x7f800001 goes in and 0x7fc00001 comes out.
 * Measured here, not assumed.  The 2003 compiler emitted an integer move for
 * the same source, so the blob preserves the payload and a plain assignment
 * does not.
 *
 * The original's source was almost certainly the plain assignment; this is a
 * difference in what the compiler makes of it, twenty years apart.  Since the
 * goal is a replacement that behaves identically, the behaviour wins over the
 * likely source form -- and a future reader who "simplifies" this back to
 * `*q++ = *p++` will pass every test that does not feed a signalling NaN.
 * t_queue does feed them.
 *
 * THE `asm` BARRIER IS LOAD-BEARING and `__builtin_memcpy` alone is not enough.
 * GCC folds a memcpy between two same-typed pointers straight back into an
 * assignment, and in `write(T v)` -- where the source is a `float` PARAMETER
 * and its type is therefore in front of the compiler -- it did exactly that
 * and emitted `flds 0x10(%esp)` / `fstps (%esi)`.  The block paths happened to
 * come out as integer moves anyway, so the single-value path was the only one
 * that diverged, which is a good illustration of why this cannot be left to
 * luck.  The empty `"+r"` constraint makes the value opaque and pins it in a
 * general register, matching the object's `mov 0x14(%esp),%esi; mov %esi,(%ecx)`
 * at +0x36.  It generates no instructions of its own.
 */
template <class T>
static inline void copy1(T *dst, const T *src)
{
	if (sizeof(T) == sizeof(unsigned)) {
		unsigned tmp;

		__builtin_memcpy(&tmp, src, sizeof(unsigned));
		__asm__("" : "+r" (tmp));
		__builtin_memcpy(dst, &tmp, sizeof(unsigned));
	} else {
		__builtin_memcpy(dst, src, sizeof(T));
	}
}

/*
 * `reset` is called rather than inlined -- the constructor tail-jumps to it --
 * so the attribute keeps the shape.  It does no arithmetic, so nothing here
 * depends on that; it is reproduced because the object does it.
 */
template <class T>
__attribute__((noinline)) void Queue<T>::reset()
{
	wr = rd = buf;
}

template <class T>
Queue<T>::Queue(unsigned n)
{
	/*
	 * n + 1 SLOTS.  No check on the allocation: a NULL return makes `last`
	 * the address -4, `reset` points both cursors at NULL, and the first
	 * write stores through it.  The object does not check either.
	 */
	size = n + 1;
	buf = (T *)sysdep_malloc(size * sizeof(T));
	last = buf + size - 1;
	reset();
}

template <class T>
Queue<T>::~Queue()
{
	/* `buf` is NOT nulled, so a second destruction double-frees. */
	if (buf)
		sysdep_free(buf);
}

template <class T>
int Queue<T>::write(T v)
{
	if (size - count() - 1 == 0)
		return -1;

	copy1(wr, &v);
	wr = (wr == last) ? buf : wr + 1;
	return 0;
}

template <class T>
int Queue<T>::write(T *p, unsigned num)
{
	int room = (int)((last - wr) + 1);	/* to the end of the store */
	int i;

	if (size - count() - 1 < num)
		return -1;

	if (room < (int)num) {
		T *q = wr;

		for (i = 0; i < room; i++)
			copy1(q++, p++);
		q = buf;
		for (; i < (int)num; i++)
			copy1(q++, p++);
		wr = q;
	} else {
		for (i = 0; i < (int)num; i++)
			copy1(wr++, p++);
		if (wr == last + 1)
			wr = buf;
	}
	return 0;
}

template <class T>
int Queue<T>::read(T *p, unsigned num)
{
	int avail = (int)((last - rd) + 1);
	int i;

	if (count() < num)
		return -1;

	if (avail < (int)num) {
		T *q = rd;

		for (i = 0; i < avail; i++)
			copy1(p++, q++);
		q = buf;
		for (; i < (int)num; i++)
			copy1(p++, q++);
		rd = q;
	} else {
		for (i = 0; i < (int)num; i++)
			copy1(p++, rd++);
		if (rd == last + 1)
			rd = buf;
	}
	return 0;
}

template class Queue<float>;
