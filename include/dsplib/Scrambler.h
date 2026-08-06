/*
 * Scrambler.h -- the object's own self-synchronising scrambler template.
 *
 * Reconstructed from dsplibs.o.  `Scrambler<T, I>` is a CLASS TEMPLATE, and
 * the blob carries four instantiations of it -- `<h,h>`, `<h,i>`, `<i,h>` and
 * the parallel `Descrambler<h,i>` / `Descrambler<i,i>` -- each emitted as
 * weak symbols in their own `.gnu.linkonce.t.*` sections.  Only
 * `Scrambler<unsigned char, int>` is reconstructed here, and only the four
 * members `V90Phase3Modulator` reaches:
 *
 *     _ZN9ScramblerIhiE5resetEh                   44 B
 *     _ZN9ScramblerIhiE7processEh                118 B
 *     _ZN9ScramblerIhiE19resetHistoryIndexesEv    23 B
 *     _ZN9ScramblerIhiE15copyHistoryTailEv        31 B
 *
 * THOSE FOUR ARE PART OF TASK #60's BATCH 2 AND `callgraph.py` CANNOT SEE
 * THEM: it enumerates `T` symbols, and a weak template instantiation is `W`.
 * They are nevertheless renamed `ref_*` in the reference object like
 * everything else, so leaving them unwritten breaks the link for the whole
 * suite exactly as an unwritten ordinary callee does (docs/v90cpp.md).
 *
 * THE HISTORY RUNS BACKWARDS THROUGH THE BUFFER.  `process()` writes at
 * `pOut` and reads two taps *above* it, then steps all three down by one; it
 * is the falling-address form of y[n] = x[n] ^ y[n-a] ^ y[n-b].  When `pOut`
 * would fall below `pLimit` the object restarts: the three pointers go back
 * to their initial values and the `tailLength` bytes at `pLimit` are copied
 * up to just above the restart point, so the taps still see the history they
 * would have seen had the buffer been unbounded.
 *
 * ALL FOUR ARE NOW DIFFERENTIALLY TESTED, both through
 * `V90Phase3Modulator::generate*Symbol`, which own the subobject at +0x20,
 * and directly against the `ref__ZN9ScramblerIhiE*` aliases -- which exist:
 * symmap.py renames weak symbols like everything else.  test/unit/t_v90p3mod
 * drives them with a buffer small enough that `pOut` falls below `pLimit`,
 * because the restart is a third of the class and is invisible otherwise.
 * The reconstruction below needed no change to pass.
 *
 * `reset(value)` fills that same history -- the bytes from `pInitOut + 1` up
 * to and including `pInitTap2` -- with `value & 1`, one bit per byte, which is
 * the same one-byte-per-bit convention V90Jd uses for its message.
 *
 * The remaining members are declared for the record and deliberately left
 * undefined: nothing in batch 2 calls them, and defining one would re-open
 * the link closure.  There is deliberately no
 * `template class Scrambler<unsigned char, int>;` here for the same reason --
 * an explicit instantiation would demand a definition for every member.
 */

#ifndef DSPLIB_SCRAMBLER_H
#define DSPLIB_SCRAMBLER_H

template <class T, class I>
class Scrambler {
public:
	/*
	 * Put the three running pointers back to their initial values.  This
	 * is 23 bytes in the blob and does exactly three word copies.
	 */
	void resetHistoryIndexes()
	{
		pOut = pInitOut;
		pTap1 = pInitTap1;
		pTap2 = pInitTap2;
	}

	/*
	 * Carry the wrapped history back up to the restart point.  The count
	 * is `tailLength`; the blob spells the loop as a decrement that stops
	 * when the counter reaches -1, which is `tailLength` iterations for
	 * any value including zero.
	 */
	void copyHistoryTail()
	{
		T *dst = pInitOut + 1;
		const T *src = pLimit;
		unsigned int n = tailLength;
		unsigned int i;

		for (i = 0; i < n; i++)
			dst[i] = src[i];
	}

	/*
	 * Seed the history with one bit, repeated.  `value & 1` is masked in
	 * the blob before the loop, so only the low bit of the argument can
	 * reach the buffer.
	 */
	void reset(T value)
	{
		T bit = (T)(value & 1);
		T *p;

		resetHistoryIndexes();
		for (p = pInitOut + 1; p <= pInitTap2; p++)
			*p = bit;
	}

	/*
	 * One symbol.  Both taps are read at their current positions and then
	 * stepped down; the output is written where `pOut` points and `pOut`
	 * steps down after.  Falling below `pLimit` restarts the buffer.
	 */
	T process(T in)
	{
		T *out = pOut;
		T r;

		r = (T)(in ^ *pTap1 ^ *pTap2);
		pTap1--;
		pTap2--;
		*out = r;
		if (--pOut < pLimit) {
			resetHistoryIndexes();
			copyHistoryTail();
		}
		return r;
	}

	/*
	 * Declared, not defined.  The signatures are the mangling's, so this
	 * is a specification rather than a guess; `I` is the wider type the
	 * bulk overloads use, which is what makes `<unsigned char, int>` and
	 * `<unsigned char, unsigned char>` two different instantiations.
	 *
	 * The constructor `Scrambler(unsigned, unsigned, unsigned)` and the
	 * destructor are NOT declared, deliberately.  Declaring either makes
	 * the class non-trivial, which deletes the default members of any
	 * union holding one and makes `__builtin_offsetof` conditionally
	 * supported -- and the test fixture is a union of the object with a
	 * byte array, so both matter.  Their signatures are on the record in
	 * docs/v90cpp.md instead.
	 */
	void process(const T *, I *, unsigned int);
	void processAllOnes(T *, unsigned int);
	void processAllZeros(T *, unsigned int);

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable (finding 226) and because one access section is what
	 * keeps `__builtin_offsetof` well defined -- the .cpp asserts every
	 * offset below.
	 */
	T *pLimit;		/* +0x00 lowest address `pOut` may reach   */
	T *pInitOut;		/* +0x04 restart value for pOut            */
	T *pInitTap1;		/* +0x08 restart value for pTap1           */
	T *pInitTap2;		/* +0x0c restart value for pTap2           */
	T *pOut;		/* +0x10 where the next output goes        */
	T *pTap1;		/* +0x14 the near tap                      */
	T *pTap2;		/* +0x18 the far tap                       */
	unsigned int tailLength;	/* +0x1c bytes carried on restart  */
};

/*
 * `Descrambler<T, I>` -- a SEPARATE TEMPLATE, not a typedef of the above and
 * not a base or derived class of it.  The blob carries `Descrambler<h,i>` and
 * `Descrambler<i,i>` under their own mangled names in their own
 * `.gnu.linkonce.t.*` sections, so spelling it any other way emits symbols
 * that link against nothing.
 *
 * `V90Phase3Demodulator::reset` reaches two of its members and they are the
 * only two DEFINED here:
 *
 *     _ZN11DescramblerIiiE5resetEi                48 B
 *     _ZN11DescramblerIiiE19resetHistoryIndexesEv 23 B
 *
 * Both are differentially tested by test/unit/t_v90p3dreset.cpp -- through
 * `V90Phase3Demodulator::reset`, which owns the `<int,int>` subobject at
 * +0x3d0, and directly against the `ref__ZN11DescramblerIii*` aliases, which
 * exist because symmap.py renames weak symbols like everything else.
 *
 * THE LAYOUT IS THE SAME EIGHT FIELDS AS `Scrambler`, IN THE SAME ORDER, and
 * that is measured rather than assumed by analogy.  `resetHistoryIndexes`
 * gives +0x04 -> +0x10, +0x08 -> +0x14 and +0x0c -> +0x18; `reset` gives
 * +0x04 and +0x0c as the bounds of the fill; and the two members outside this
 * batch pin the other two -- `copyHistoryTail` reads +0x00 as the source and
 * +0x1c as the count, and `process` compares the stepped-down +0x10 against
 * +0x00.  All four disassembled with
 * `objdump -dr --section=.gnu.linkonce.t.<symbol>`, which is the only way to
 * read a weak member here (tools/dis.py takes its zero `st_value` for a
 * `.text` offset).
 *
 * WHERE IT DIFFERS FROM `Scrambler` IS `process`, WHICH IS NOT DEFINED HERE.
 * The scrambler writes its OUTPUT at `pOut`; the descrambler writes its INPUT
 * there, and returns `in ^ *pTap1 ^ *pTap2` -- which is what makes it the
 * inverse.  Nothing in this batch calls it, so it stays declared, exactly as
 * `Scrambler`'s bulk overloads do: defining a member no test drives would put
 * an unverified body in the tree.
 *
 * The constructor is on the record and not declared, for the reason given
 * above for `Scrambler`'s.  `V90Phase3Demodulator`'s constructor builds this
 * one with (0x12, 0x17, 0x63), and the blob's body is:
 *
 *     tailLength = b;
 *     pLimit     = (T *)sysdep_malloc((1 + b + c) * sizeof(T));
 *     pInitOut   = pLimit + c;
 *     pInitTap1  = pInitOut + a;
 *     pInitTap2  = pInitOut + b;
 *     reset(0);
 *
 * which is where t_v90p3dreset.cpp's hand-built subobject comes from.
 */
template <class T, class I>
class Descrambler {
public:
	/* Three word copies, as in `Scrambler`. */
	void resetHistoryIndexes()
	{
		pOut = pInitOut;
		pTap1 = pInitTap1;
		pTap2 = pInitTap2;
	}

	/*
	 * Seed the history with one bit, repeated.  `value & 1` is masked
	 * before the loop, so only the low bit of the argument reaches the
	 * buffer, and the loop runs from `pInitOut + 1` up to and including
	 * `pInitTap2`.
	 */
	void reset(T value)
	{
		T bit = (T)(value & 1);
		T *p;

		resetHistoryIndexes();
		for (p = pInitOut + 1; p <= pInitTap2; p++)
			*p = bit;
	}

	/* Declared, not defined -- see the file comment above. */
	T process(T);
	void process(const T *, I *, unsigned int);
	void copyHistoryTail();

	T *pLimit;		/* +0x00 lowest address `pOut` may reach   */
	T *pInitOut;		/* +0x04 restart value for pOut            */
	T *pInitTap1;		/* +0x08 restart value for pTap1           */
	T *pInitTap2;		/* +0x0c restart value for pTap2           */
	T *pOut;		/* +0x10 where the next input is stored    */
	T *pTap1;		/* +0x14 the near tap                      */
	T *pTap2;		/* +0x18 the far tap                       */
	unsigned int tailLength;	/* +0x1c words carried on restart  */
};

#endif /* DSPLIB_SCRAMBLER_H */
