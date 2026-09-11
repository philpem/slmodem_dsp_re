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
 * The element copy.
 */
template <class T>
static inline void copy1(T *dst, const T *src)
{
	*dst = *src;
}

/*
 * `reset` is called rather than inlined -- the constructor tail-jumps to it --
 * so the attribute keeps the shape.  It does no arithmetic, so nothing here
 * depends on that; it is reproduced because the object does it.
 */
template <class T>
__attribute__((noinline)) void Queue<T>::reset()
{
	/*
	 * `rd = wr = buf`, not `wr = rd = buf`: the object stores `wr` first,
	 * and written this way the function comes out full-text identical to
	 * it, operands and all.  The two spellings are equivalent -- both set
	 * both -- so this is the author's, recovered.  Finding F617.
	 */
	rd = wr = buf;
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
	delete[] buf;
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

/*
 * INSTANTIATED MEMBER BY MEMBER, not `template class Queue<float>;`.
 *
 * An explicit class instantiation emits EVERY member, and the object has no
 * `count` symbol -- both of its call sites open-code the expression, so a weak
 * out-of-line copy here would be a symbol we define and the original does not.
 * Naming the six members the object actually contains keeps the symbol sets
 * equal.  `always_inline` on `count` is not enough on its own; the explicit
 * instantiation overrides it.
 */
template Queue<float>::Queue(unsigned);
template Queue<float>::~Queue();
template void Queue<float>::reset();
template int Queue<float>::write(float);
template int Queue<float>::write(float *, unsigned);
template int Queue<float>::read(float *, unsigned);
