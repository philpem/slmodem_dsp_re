/*
 * Queue.h -- the object's single-producer ring buffer, as a class template.
 *
 * Six weak symbols in their own `.gnu.linkonce.t.*` sections, instantiated at
 * `float` and nothing else.  20 bytes, no virtuals -- every call site is a
 * direct relocation and there is no vtable slot at +0.
 *
 * ---------------------------------------------------------------------------
 * HEADER-ONLY, AND THE BLOB PROVES IT
 *
 * There is no `Queue.cpp` in the object: `.symtab` has no `STT_FILE` record
 * for one, and `ld -r` preserves a FILE record for an object with no symbols
 * at all (a partial link of our own empty stubs still carries them).  The six
 * Queue functions are weak `.gnu.linkonce.t` instantiations emitted by the ONE
 * TU that reaches the definitions -- `V92Modulator.cpp`, whose `reset`,
 * `progress`, constructor and destructor are the only callers.  Defining the
 * members out of line in a `Queue.cpp` and explicitly instantiating them
 * produces the same six bodies but ALSO C2/D2, the base-object ctor/dtor the
 * blob does not have; under implicit instantiation GCC emits only the
 * referenced C1/D1.  So the members are defined here, and WITHOUT the `inline`
 * keyword: with it GCC inlines five of the six away and their out-of-line
 * symbols disappear, while without it they are emitted, exactly as the object
 * has them.  `reset` keeps its noinline attribute so the constructor
 * tail-jumps to it.  Deleting `Queue.cpp` also removes the one FILE record the
 * object does not have.
 *
 * `sizeof` is pinned from two sides: the 4-byte `size` at +0x10 puts a floor
 * under it, and the caller at 0x152ee does `movl $0x14,(%esp); call
 * sysdep_malloc` and hands the result straight to the constructor.
 *
 * ---------------------------------------------------------------------------
 * ONE SLOT IS ALWAYS EMPTY, which is why the sizes look off by one
 *
 * `Queue(n)` allocates n+1 slots and can hold n items.  That is the ordinary
 * way to tell a full ring from an empty one without a separate count: the
 * write cursor is never allowed to catch the read cursor from behind.  So
 * `Queue(0)` is legal, allocates one slot, holds nothing, and every write
 * returns -1.
 *
 * `last` points at the LAST element, not one past it.  Every wrap test is
 * against that, and the block paths check `== last + 1` rather than `> last`
 * because the cursor can only ever land exactly there.
 *
 * ---------------------------------------------------------------------------
 * `count()` is the original's, not an idiom invented here
 *
 * `V92Modulator::progress` open-codes the identical
 * `lea (%eax,%ecx,4); sub; sar $2; xor %edx; div %ecx` against this object's
 * +0x08, +0x0c and +0x10 at 0x14c88 and again at 0x14d6f.  The occupancy is
 * computed as `(wr + size) - rd`, a POINTER add of `size` elements before the
 * subtraction, not `(wr - rd) + size`; algebraically the same and that is the
 * form the object uses.
 *
 * `isEmpty()` and `isFull()` ARE THE ORIGINAL'S TOO, and the second one is
 * where a `space()` would have shown itself.  `V92Modulator::progress` closes
 * with the pair, short-circuited, at 0x14d97:
 *
 *     14d97:  39 d3       cmp %edx,%ebx        rd == wr, and no division
 *     14d99:  74 16       je  <the message>
 *     ...     count() ...
 *     14dac:  29 d5       sub %edx,%ebp        size - count()
 *     14dae:  4d          dec %ebp             - 1
 *     14daf:  75 10       jne <return>         != 0 -> not full
 *
 * so BOTH SPELLINGS ARE FORCED rather than chosen.  `isEmpty()` is `rd == wr`
 * and not `count() == 0`, which would have emitted the `div`; `isFull()` is
 * `size - count() - 1 == 0` and not `count() == size - 1`, which would have
 * emitted `lea -1(%ebp); cmp` instead of `sub; dec`.  The message printed on
 * the joint arm is the author's own name for the pair --
 * "V92Modulator: Queue is Empty/Full !!!".
 *
 * A `space()` returning `size - count() - 1` is what `isFull()` tests against
 * zero; whether the author spelled that separate function is still not
 * recoverable, because nothing calls it on its own.
 */

#ifndef DSPLIB_QUEUE_H
#define DSPLIB_QUEUE_H

#include "dsplib/sysdep.h"

template <class T>
class Queue {
public:
	/**
	 * @brief Construct a ring buffer that holds @p n items.
	 *
	 * Allocates `n + 1` slots (one is always kept empty, so a full ring
	 * can be told from an empty one without a separate count) and resets
	 * the read/write cursors. The allocation is not checked: a NULL
	 * return makes `last` the address -4 and the first write stores
	 * through it -- the object's own behaviour.
	 *
	 * @param n  Capacity in items. Zero is legal: it allocates one slot,
	 *           holds nothing, and every write() then returns -1.
	 */
	Queue(unsigned n);

	/** @brief Free the backing store. `buf` is not nulled afterwards. */
	~Queue();

	/** @brief Empty the queue: move both cursors to the start of the buffer. */
	void reset();

	/**
	 * @brief Write one item.
	 * @param v  Value to write.
	 * @return 0 on success, -1 if the queue is full.
	 */
	int write(T v);

	/**
	 * @brief Write @p num items.
	 *
	 * A partial write never happens: either all @p num items are written
	 * or none are. `num == 0` succeeds trivially.
	 *
	 * @param p    Items to write, @p num of them.
	 * @param num  Number of items to write.
	 * @return 0 on success, -1 if there is not room for the whole request.
	 */
	int write(T *p, unsigned num);

	/**
	 * @brief Read @p num items.
	 *
	 * A partial read never happens: either all @p num items are read or
	 * none are.
	 *
	 * @param p    Destination for the read items, @p num of them.
	 * @param num  Number of items to read.
	 * @return 0 on success, -1 if fewer than @p num items are available.
	 */
	int read(T *p, unsigned num);

	/**
	 * @brief Number of items currently queued.
	 *
	 * The object has no standalone `count` symbol.  The member functions
	 * are header-inline here, and this in-class definition needs no
	 * forced-inline attribute: every call site open-codes it.  See #22
	 * and docs/cid-dcr-audit.md for the crossed source/profile controls.
	 *
	 * @return Number of items currently in the queue.
	 */
	unsigned count() const
	{
		return (unsigned)((wr + size) - rd) % size;
	}

	/**
	 * @brief Whether the queue holds nothing.
	 *
	 * The only call site of either this or isFull() is
	 * `V92Modulator::progress`'s closing test; both
	 * spellings here are the object's own forced codegen, not a tidier
	 * equivalent (`rd == wr` rather than `count() == 0`, which would
	 * emit a division).
	 *
	 * @return Non-zero if the queue is empty.
	 */
	int isEmpty() const
	{
		return rd == wr;
	}

	/**
	 * @brief Whether the queue is at capacity.
	 * @return Non-zero if the queue is full.
	 */
	int isFull() const
	{
		return size - count() - 1 == 0;
	}

private:
	T	*buf;		/* +0x00 owned                              */
	T	*last;		/* +0x04 &buf[size - 1], NOT one past       */
	T	*rd;		/* +0x08                                    */
	T	*wr;		/* +0x0c                                    */
	unsigned size;		/* +0x10 slots, which is the ctor's n + 1   */
};

template <class T>
__attribute__((noinline)) void Queue<T>::reset()
{
	rd = wr = buf;
}

template <class T>
Queue<T>::Queue(unsigned n)
{
	size = n + 1;
	buf = (T *)sysdep_malloc(size * sizeof(T));
	last = buf + size - 1;
	reset();
}

template <class T>
Queue<T>::~Queue()
{
	delete[] buf;
}

template <class T>
int Queue<T>::write(T v)
{
	if (size - count() - 1 == 0)
		return -1;

	*wr = v;
	wr = (wr == last) ? buf : wr + 1;
	return 0;
}

template <class T>
int Queue<T>::write(T *p, unsigned num)
{
	int room = (int)((last - wr) + 1);
	int i;

	if (size - count() - 1 < num)
		return -1;

	if (room < (int)num) {
		T *q = wr;

		for (i = 0; i < room; i++)
			*q++ = *p++;
		q = buf;
		for (; i < (int)num; i++)
			*q++ = *p++;
		wr = q;
	} else {
		for (i = 0; i < (int)num; i++)
			*wr++ = *p++;
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
			*p++ = *q++;
		q = buf;
		for (; i < (int)num; i++)
			*p++ = *q++;
		rd = q;
	} else {
		for (i = 0; i < (int)num; i++)
			*p++ = *rd++;
		if (rd == last + 1)
			rd = buf;
	}
	return 0;
}

#endif /* DSPLIB_QUEUE_H */
