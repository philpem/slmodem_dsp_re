/*
 * Queue.h -- the object's single-producer ring buffer, as a class template.
 *
 * Six weak symbols in their own `.gnu.linkonce.t.*` sections, instantiated at
 * `float` and nothing else.  20 bytes, no virtuals -- every call site is a
 * direct relocation and there is no vtable slot at +0.
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

template <class T>
class Queue {
public:
	Queue(unsigned n);
	~Queue();

	void reset();

	/*
	 * 0 on success, -1 if there is not room for the whole request.  A
	 * partial write never happens and `num == 0` succeeds trivially.
	 *
	 * BOTH CALL SITES DISCARD THE RETURN, at 0x14d44 and 0x14d5f, as does
	 * the single-value form's at 0x153c0.  The type is `int` here because
	 * the object returns 0 and -1; nothing in the blob distinguishes that
	 * from `unsigned`, and no caller looks.
	 */
	int write(T v);
	int write(T *p, unsigned num);
	int read(T *p, unsigned num);

	/*
	 * `always_inline` because the object has NO `count` symbol: both call
	 * sites open-code it, and an out-of-line weak copy here would be a
	 * symbol we define and the object does not.  GCC emits one for an
	 * ordinary in-class definition even when every call is inlined.
	 */
	__attribute__((always_inline)) unsigned count() const
	{
		return (unsigned)((wr + size) - rd) % size;
	}

	/*
	 * `always_inline` for `count()`'s reason, and the only call site of
	 * either is `V92Modulator::progress`'s closing test.  See the file
	 * comment for why each is spelled the way it is; both spellings are
	 * the object's and neither is a tidier equivalent.
	 */
	__attribute__((always_inline)) int isEmpty() const
	{
		return rd == wr;
	}

	__attribute__((always_inline)) int isFull() const
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

#endif /* DSPLIB_QUEUE_H */
