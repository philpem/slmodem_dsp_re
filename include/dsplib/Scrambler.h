/*
 * Scrambler.h -- the object's own self-synchronising scrambler template.
 *
 * Reconstructed from dsplibs.o.  `Scrambler<T, I>` and `Descrambler<T, I>` are
 * two SEPARATE CLASS TEMPLATES, and the blob carries five instantiations of
 * them between them, every member emitted as a weak symbol in its own
 * `.gnu.linkonce.t.*` section.  All thirty-four are now written and
 * differentially tested; the table is the symbol table's, taken with
 *
 *     readelf -sW dsplibs.o | grep -E '_ZN9ScramblerI|_ZN11DescramblerI'
 *
 * and it is the authority on WHICH members each instantiation has, because
 * they differ and the difference is load-bearing:
 *
 *   member                        <h,h>  <h,i>  <i,h>  D<h,i>  D<i,i>
 *   ---------------------------   -----  -----  -----  ------  ------
 *   C1(unsigned,unsigned,unsigned)  102    102    107     102     107
 *   D1()                             29     29     29      29      29
 *   resetHistoryIndexes()            23     23     23      23      23
 *   copyHistoryTail()                31     31     36      31      36
 *   reset(T)                         44     44     48      44      48
 *   process(T)                      107    118      -     118     110
 *   process(const T *, I *, j)      120      -    136     120       -
 *   processAllOnes(I *, j)          120      -      -       -       -
 *   processAllZeros(I *, j)         120      -      -       -       -
 *   ---------------------------   -----  -----  -----  ------  ------
 *   bytes                           696    347    379     467     353
 *   symbols                           9      6      6       7       6
 *
 * 2,242 bytes over 34 symbols, which is what `tools/closure.py` reports for
 * the group.  **`Scrambler<unsigned char,int>` HAS NO BULK `process`** and
 * `Scrambler<int,unsigned char>` has no single-value one; a batch brief that
 * says otherwise is wrong, and writing a member with no blob symbol would be a
 * body with no `ref_` alias to compare against.
 *
 * Six of the thirty-four -- `<h,i>`'s reset, process(T), resetHistoryIndexes
 * and copyHistoryTail, and `<i,i>`'s reset and resetHistoryIndexes, 287 bytes
 * -- were written and tested earlier, through `V90Phase3Modulator` and
 * `V90Phase3Demodulator`.  THEIR BODIES ARE UNCHANGED HERE.  The other 1,955
 * bytes over 28 symbols are findings 869-872.
 *
 * `tools/closure.py --missing` used to report all thirty-four missing whatever
 * was written, because a header-inlined template member leaves no reference in
 * any object under `build/src` for it to resolve.  src/dsp/Scrambler.cpp names
 * all thirty-four in explicit MEMBER-BY-MEMBER instantiations, exactly as
 * `src/dsp/Queue.cpp` does and for the same reason -- a whole-class
 * `template class Scrambler<...>;` would emit members the original does not
 * have, and the member sets above are not the same across instantiations.  So
 * the symbols are real now and the tool's answer is the true one.
 *
 * ---------------------------------------------------------------------------
 * THE HISTORY RUNS BACKWARDS THROUGH THE BUFFER.  `process()` writes at
 * `pOut` and reads two taps *above* it, then steps all three down by one; it
 * is the falling-address form of y[n] = x[n] ^ y[n-a] ^ y[n-b].  When `pOut`
 * would fall below `pLimit` the object restarts: the three pointers go back
 * to their initial values and the `tailLength` elements at `pLimit` are copied
 * up to just above the restart point, so the taps still see the history they
 * would have seen had the buffer been unbounded.
 *
 * `reset(value)` fills that same history -- the elements from `pInitOut + 1`
 * up to and including `pInitTap2` -- with `value & 1`, one bit per element,
 * which is the same one-element-per-bit convention V90Jd uses for its message.
 *
 * ---------------------------------------------------------------------------
 * THE INTERMEDIATE IS `I`, NOT `T`, AND THAT IS FORCED (finding 870)
 *
 * `Scrambler<int, unsigned char>::process(const int *, unsigned char *,
 * unsigned)` computes the whole XOR in EIGHT BITS -- `mov (%edx),%ecx` loads
 * the full `int` tap and every following operation is `xor r/m8,%cl` -- and
 * then stores `movzbl %cl` into the `int` history.  A `T` intermediate would
 * put the full 32-bit XOR there.  That is a difference a test can see, not a
 * codegen preference, and it fixes the type of the temporary at `I` for every
 * bulk member.  `t_scrambler`'s `<i,h>` block drives taps with bits above bit
 * 7 set specifically to hold it.
 *
 * The same reading explains why `Scrambler<unsigned char,unsigned char>::
 * process(h)` is 107 bytes and `Scrambler<unsigned char,int>::process(h)` is
 * 118: same `T`, same body, and the byte-wide temporary of the first against
 * the 32-bit one of the second is `I` again.  For `T = unsigned char` the two
 * are BEHAVIOURALLY IDENTICAL -- every operand is already a byte and both the
 * store and the return truncate -- so no test in this tree can tell them
 * apart, and the single-value `process` keeps the `T` temporary it was
 * verified with rather than being rewritten on evidence no gate can check.
 * Recorded so a later batch does not re-derive it.
 *
 * The return type is not mangled and nothing pins it: `<h,h>` returns a
 * zero-extended byte and `<h,i>` a full register, which is what either
 * spelling gives.  `T` is kept for both templates.
 *
 * ---------------------------------------------------------------------------
 * THE CONSTRUCTOR AND DESTRUCTOR ARE DECLARED, and the reason this file used
 * to give for not declaring them was half right (finding 871).
 *
 *   - The UNION half is real.  Declaring either makes the class non-trivial
 *     and leaves it with no default constructor, which DELETES the default
 *     constructor and the destructor of any union holding one -- and the test
 *     fixtures for `V90Phase3Modulator` and `V90Phase3Demodulator` are a union
 *     of the object with a byte array.  The fix is two lines per union, a
 *     user-provided `slot() {}` and `~slot() {}`, which construct and destroy
 *     no variant member; every existing `.o` and `.raw` access site is
 *     untouched.  A union that is also a FUNCTION-LOCAL STATIC then needs
 *     hoisting to file scope, because a non-trivial local static wants
 *     `__cxa_guard_acquire` and this tree links no libstdc++.
 *   - The `__builtin_offsetof` half is STALE.  `offsetof` requires STANDARD
 *     LAYOUT, which a user-provided constructor does not affect; it is
 *     triviality that it does affect, and the two were the same thing only in
 *     C++03's `POD`.  CFLAGS names no `-std=` and no `-Werror`, so this
 *     compiles at C++17 with no diagnostic.  Verified by compiling, not read.
 *
 * The constructor's argument order is `(a, b, c)`: `a` is the NEAR tap's
 * distance above `pInitOut`, `b` the FAR tap's and also `tailLength`, and `c`
 * the distance `pInitOut` sits above `pLimit`.  `V90Phase3Demodulator` builds
 * its `Descrambler<int,int>` with (0x12, 0x17, 0x63) -- V.90's taps 18 and 23.
 */

#ifndef DSPLIB_SCRAMBLER_H
#define DSPLIB_SCRAMBLER_H

#include "dsplib/sysdep.h"

/*
 * THE REPLACEMENT `operator delete[]`, AND IT IS READ OFF THE OBJECT.  At a
 * destructor's LAST free the blob makes an ordinary `call sysdep_free` where
 * our explicit `if (p) sysdep_free(p)` makes a sibling `jmp` -- one
 * instruction fewer, and the sibcall drops the frame with it.  Eight spellings
 * were compiled and only `delete[]` reproduces the object's shape; finding
 * 7786 and `docs/method/refinement.md` lever 7 carry the enumeration.
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
 * IT IS IN THE HEADER BECAUSE THE DESTRUCTOR IS.  `~Scrambler` is defined
 * inline here, so a replacement seen only by `Scrambler.cpp` would leave any
 * other TU that instantiates it referencing the library's `_ZdaPv`, which
 * this tree does not link.  Every file that reaches this header therefore
 * must NOT define its own -- `V90Equalizer.cpp`, which gets here through
 * `V90Phase4Demodulator.h`, is the one that found that out.
 */
inline void operator delete[](void *p) { sysdep_free(p); }

template <class T, class I>
class Scrambler {
public:
	/*
	 * `sysdep_malloc` is NOT checked, exactly as in the blob: a null return
	 * makes every init pointer a small address and the first `reset` stores
	 * through it.  The count is `1 + b + c` ELEMENTS -- `<int,...>` emits
	 * `shl $0x2` on it, which is where `sizeof(T)` is measured -- and the
	 * body ends in a tail call to `reset(0)`.
	 */
	Scrambler(unsigned int a, unsigned int b, unsigned int c)
	{
		tailLength = b;
		pLimit = (T *)sysdep_malloc((1 + b + c) * sizeof(T));
		pInitOut = pLimit + c;
		pInitTap1 = pInitOut + a;
		pInitTap2 = pInitOut + b;
		reset(0);
	}

	/* `pLimit` is NOT nulled, so a second destruction double-frees. */
	~Scrambler()
	{
		delete[] pLimit;
	}

	/*
	 * Put the three running pointers back to their initial values.  This
	 * is 23 bytes in the blob and does exactly three word copies.
	 *
	 * THE BODY IS OUT OF LINE BECAUSE THE OBJECT CALLS IT.  Written
	 * inside the class body it is implicitly `inline`, which moves it
	 * from `--param max-inline-insns-auto` to `max-inline-insns-single`
	 * and GCC 3.4.2 then expands it into every caller while still
	 * emitting the weak symbol -- so the symbol table looks right and
	 * `reset` is wrong.  The object refutes that directly: its
	 * `Descrambler<h,i>::reset` spends `mov %esi,(%esp)` and
	 * `call _ZN11DescramblerIhiE19resetHistoryIndexesEv`, where ours had
	 * the three stores expanded in place.  That is lever 10's tell --
	 * an EXCESS of instructions with a MISSING call -- read off the
	 * object rather than inferred.  Finding 7862.
	 */
	void resetHistoryIndexes();

	/*
	 * Carry the wrapped history back up to the restart point.  The count
	 * is `tailLength`; the blob spells the loop as a decrement that stops
	 * when the counter reaches -1, which is `tailLength` iterations for
	 * any value including zero.
	 *
	 * THE POINTERS WALK; THE INDEX FORM IS WRONG AND COSTS A REGISTER.
	 * An indexed `for (i = 0; i < n; i++) dst[i] = src[i]` keeps FOUR
	 * values live across the loop -- both bases, `i` and the bound `n` --
	 * so it needs a second callee-saved register and emits `push %esi`
	 * beside `push %ebx`.  The object pushes `%ebx` alone and holds three:
	 * `src`, `dst` and a counter it compares against the CONSTANT -1
	 * (`dec %edx; cmp $0xffffffff,%edx; jne`), which is `while (n--)`
	 * with the loop rotated so entry lands on the test.
	 *
	 * Six spellings compiled, five distinct emissions; the two that reach
	 * the object's instruction sequence are `unsigned int n` and
	 * `int n` with the same `while (n--)`, and those two emit the SAME
	 * bytes.  So the decoded fact is the loop's SHAPE and not the
	 * counter's signedness, which the object cannot distinguish.  All
	 * five instantiations go SIZE to REGALLOC on it.  Finding 7861.
	 */
	void copyHistoryTail();

	/*
	 * Seed the history with one bit, repeated.  `value & 1` is masked in
	 * the blob before the loop, so only the low bit of the argument can
	 * reach the buffer.
	 */
	void reset(T value);

	/*
	 * One symbol.  Both taps are read at their current positions and then
	 * stepped down; the output is written where `pOut` points and `pOut`
	 * steps down after.  Falling below `pLimit` restarts the buffer.
	 *
	 * The `T` temporary is the one this was verified with; see the note on
	 * `I` in the file comment for why it is not `I` here and why nothing
	 * can tell.
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
	 * `n` symbols.  The result goes to BOTH `out[i]` -- as an `I`, which
	 * for `<int,unsigned char>` is narrower than the history -- and to the
	 * history at `pOut`, in that order.  The blob stores `out[i]` first and
	 * `*pOut` second, which is only observable if the caller aims `out`
	 * into the history; it is reproduced rather than tidied.
	 *
	 * DECLARED HERE AND DEFINED BELOW, for `reset`'s reason and on the same
	 * evidence.  See the block at the foot of this file.
	 */
	void process(const T *in, I *out, unsigned int n);

	/*
	 * `process` with an all-ones input and with an all-zeros one, each
	 * open-coded rather than calling `process`: the blob's bodies are the
	 * bulk loop with `in[i]` replaced by the constant, so the all-ones one
	 * carries an `xor $0x1` and the all-zeros one carries nothing at all.
	 *
	 * Only `<unsigned char, unsigned char>` instantiates these, where `T`
	 * and `I` are the same type and the parameter's spelling is therefore
	 * not recoverable from the mangling; `I *` is chosen to agree with
	 * `process`'s output parameter.
	 */
	void processAllOnes(I *out, unsigned int n)
	{
		unsigned int i;

		for (i = 0; i < n; i++) {
			T *p = pOut;
			I r = (I)(1 ^ *pTap1 ^ *pTap2);

			pTap1--;
			pTap2--;
			out[i] = r;
			*p = (T)r;
			if (--pOut < pLimit) {
				resetHistoryIndexes();
				copyHistoryTail();
			}
		}
	}

	void processAllZeros(I *out, unsigned int n)
	{
		unsigned int i;

		for (i = 0; i < n; i++) {
			T *p = pOut;
			I r = (I)(*pTap1 ^ *pTap2);

			pTap1--;
			pTap2--;
			out[i] = r;
			*p = (T)r;
			if (--pOut < pLimit) {
				resetHistoryIndexes();
				copyHistoryTail();
			}
		}
	}

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
	unsigned int tailLength;	/* +0x1c elements carried on restart */
};

/*
 * `Descrambler<T, I>` -- a SEPARATE TEMPLATE, not a typedef of the above and
 * not a base or derived class of it.  The blob carries `Descrambler<h,i>` and
 * `Descrambler<i,i>` under their own mangled names in their own
 * `.gnu.linkonce.t.*` sections, so spelling it any other way emits symbols
 * that link against nothing.
 *
 * THE LAYOUT IS THE SAME EIGHT FIELDS AS `Scrambler`, IN THE SAME ORDER, and
 * that is measured rather than assumed by analogy.  `resetHistoryIndexes`
 * gives +0x04 -> +0x10, +0x08 -> +0x14 and +0x0c -> +0x18; `reset` gives
 * +0x04 and +0x0c as the bounds of the fill; `copyHistoryTail` reads +0x00 as
 * the source and +0x1c as the count; `process` compares the stepped-down
 * +0x10 against +0x00; and the constructor writes all five of the first
 * fields plus +0x1c.  All of it disassembled with
 * `objdump -dr --section=.gnu.linkonce.t.<symbol>`, which is the only way to
 * read a weak member here (tools/dis.py takes its zero `st_value` for a
 * `.text` offset).
 *
 * WHERE IT DIFFERS FROM `Scrambler` IS `process`.  The scrambler writes its
 * OUTPUT at `pOut`; the descrambler writes its INPUT there FIRST and then
 * READS IT BACK -- `Descrambler<h,i>::process(h)` stores `in`, reloads
 * `this->pOut` because a store through `unsigned char *` may alias it, and
 * XORs `*pOut` rather than `in`.  Written as `r = in ^ ...` the reload would
 * not be there, so the store-then-read is the source's and not the compiler's.
 * That the input is stored BEFORE the taps are read is a behavioural claim and
 * is tested; that the first XOR operand is spelled `*pOut` rather than `in` is
 * NOT, and cannot be -- a plain `T` store followed by a read of the same
 * object yields the value stored for every input and every aliasing, so the
 * two spellings are the same number in every case.  Both mutations are in
 * `test/mutations/scrambler.json`, recorded `equivalent` with that argument.
 */
template <class T, class I>
class Descrambler {
public:
	/* Byte for byte the shape of `Scrambler`'s; see its comment. */
	Descrambler(unsigned int a, unsigned int b, unsigned int c)
	{
		tailLength = b;
		pLimit = (T *)sysdep_malloc((1 + b + c) * sizeof(T));
		pInitOut = pLimit + c;
		pInitTap1 = pInitOut + a;
		pInitTap2 = pInitOut + b;
		reset(0);
	}

	~Descrambler()
	{
		delete[] pLimit;
	}

	/* Three word copies, as in `Scrambler`; out of line for its reason. */
	void resetHistoryIndexes();

	/* The count is `tailLength`, as in `Scrambler`. */
	void copyHistoryTail();

	/*
	 * Seed the history with one bit, repeated.  `value & 1` is masked
	 * before the loop, so only the low bit of the argument reaches the
	 * buffer, and the loop runs from `pInitOut + 1` up to and including
	 * `pInitTap2`.
	 */
	void reset(T value);

	/* One symbol.  Store, read back, XOR the two taps, step all three. */
	T process(T in)
	{
		I r;

		*pOut = in;
		r = (I)(*pOut ^ *pTap1 ^ *pTap2);
		pTap1--;
		pTap2--;
		if (--pOut < pLimit) {
			resetHistoryIndexes();
			copyHistoryTail();
		}
		return (T)r;
	}

	/* `n` symbols, the result to `out[i]` as an `I` and nowhere else. */
	void process(const T *in, I *out, unsigned int n)
	{
		unsigned int i;

		for (i = 0; i < n; i++) {
			I r;

			*pOut = in[i];
			r = (I)(*pOut ^ *pTap1 ^ *pTap2);
			pTap1--;
			pTap2--;
			out[i] = r;
			if (--pOut < pLimit) {
				resetHistoryIndexes();
				copyHistoryTail();
			}
		}
	}

	T *pLimit;		/* +0x00 lowest address `pOut` may reach   */
	T *pInitOut;		/* +0x04 restart value for pOut            */
	T *pInitTap1;		/* +0x08 restart value for pTap1           */
	T *pInitTap2;		/* +0x0c restart value for pTap2           */
	T *pOut;		/* +0x10 where the next input is stored    */
	T *pTap1;		/* +0x14 the near tap                      */
	T *pTap2;		/* +0x18 the far tap                       */
	unsigned int tailLength;	/* +0x1c elements carried on restart */
};

/*
 * ===========================================================================
 * `reset` IS DEFINED OUT OF CLASS, AND THE OBJECT IS WHAT ASKS FOR IT
 * ===========================================================================
 *
 * A member defined inside its class body is implicitly `inline`, which puts
 * it under `max-inline-insns-single` rather than `max-inline-insns-auto` and
 * makes GCC 3.4.2 inline it almost everywhere.  Defined out here it is an
 * ordinary template member, and the compiler emits the call.
 *
 * THE BLOB EMITS THE CALL.  `_ZN9ScramblerIihE5resetEi` is a real weak symbol
 * with TWENTY `R_386_PC32` call sites, one of them the whole middle of
 * `V90Modulator::reset` (0x1a52d).  With the body in the class body ours
 * inlined it at every one; `V90Modulator::reset` came out 101 bytes against
 * the object's 78, and the 23 bytes were the inlined loop.
 *
 * MEASURED, NOT ASSUMED, AND WITH THE SETS DIFFED RATHER THAN THE COUNTS
 * (docs/cleanup.md): one tree, two builds on the exact 3.4.2 compiler,
 * identical-mnemonic set 452 -> 455 with **nothing lost** and three gained --
 * `V90Modulator::reset`, `Scrambler<unsigned char,int>::Scrambler` and
 * `Descrambler<unsigned char,int>::Scrambler`, the last two because they
 * called `reset` too.  Finding 5805.
 *
 * `Descrambler::reset` moves with it because the two bodies are the same text
 * and the same argument applies; the sibling template in `DiffCoder.h` has
 * always been this shape -- declared in the class, defined in
 * `src/dsp/DiffCoder.cpp` -- and `ParallelDifferentialEncoder<h>::reset` is
 * the call the object makes from `V90SpectralShaper::reset` and the call we
 * make.  This brings the two templates into line with each other and with the
 * object.
 */
/*
 * ---------------------------------------------------------------------------
 * AND THE BULK `process` GOES WITH IT, ON THE SAME EVIDENCE
 *
 * `_ZN9ScramblerIihE7processEPKiPhj` and `_ZN9ScramblerIhhE7processEPKhPhj`
 * are real weak symbols in the blob with THREE and SIXTEEN `R_386_PC32` call
 * sites respectively -- nineteen calls the original compiler chose to make.
 * With the body in the class body ours had **zero**: implicitly `inline`, so
 * GCC 3.4.2 inlined it at every one of the nineteen and still emitted the weak
 * symbol because it could not prove nobody needed the address.
 *
 * `V92Modulator::progress` is where that was found.  Its data-phase arm is one
 * `scrambler.process(bits, buf_88, nbits)`, and the function came out 355
 * instructions against the object's 298 with the CALL COUNT one SHORT -- 18
 * against 19 -- which is finding 7480's rule reading exactly right: an excess
 * of instructions with a missing call is an inlining difference and not a
 * missing statement.
 *
 * `Descrambler`'s bulk `process` STAYS IN THE CLASS BODY, and that is the same
 * evidence read the other way: the blob carries no
 * `Descrambler<...>::process(const ...)` symbol at all, so moving it out would
 * make us emit a weak symbol the original does not have.  The two templates
 * differ here because the object says they differ.
 * ---------------------------------------------------------------------------
 */
template <class T, class I>
void Scrambler<T, I>::process(const T *in, I *out, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		T *p = pOut;
		I r = (I)(in[i] ^ *pTap1 ^ *pTap2);

		pTap1--;
		pTap2--;
		out[i] = r;
		*p = (T)r;
		if (--pOut < pLimit) {
			resetHistoryIndexes();
			copyHistoryTail();
		}
	}
}

/*
 * THE MASK IS COMPUTED AFTER THE CALL, AND THAT IS THE WHOLE OF THE
 * DIFFERENCE -- five symbols closed on it.  With `bit` initialised in its
 * declaration it is live ACROSS `resetHistoryIndexes()`, so GCC 3.4.2 must
 * park it in a callee-saved register and pays a `push`/`pop` pair for it;
 * ours spent `%esi` (and for `<h,i>` spilled the byte to `0x7(%esp)`).  The
 * object reads `value` back off its incoming stack slot AFTER the call and
 * masks into a caller-saved register -- `mov 0x14(%esp),%ecx ... and
 * $0x1,%ecx` for `<i,i>` -- so nothing of it crosses the call.
 *
 * Eight spellings compiled, THREE distinct emissions.  Four reach the
 * object: `bit` declared mid-block after the call, `bit` declared then
 * assigned after it, the cast dropped, and masking `value` in place.  They
 * emit the SAME BYTES, so what is decoded is the POSITION of the mask
 * relative to the call and not which of those four the author typed; the
 * two that mask inside the loop body do NOT match, which is what makes the
 * hoist part of the decoded fact rather than an assumption.  The form below
 * is chosen for this file's declarations-at-the-top style only.
 *
 * Finding 7863.
 */
template <class T, class I>
void Scrambler<T, I>::reset(T value)
{
	T bit;
	T *p;

	resetHistoryIndexes();
	bit = (T)(value & 1);
	for (p = pInitOut + 1; p <= pInitTap2; p++)
		*p = bit;
}

template <class T, class I>
void Descrambler<T, I>::reset(T value)
{
	T bit;
	T *p;

	resetHistoryIndexes();
	bit = (T)(value & 1);
	for (p = pInitOut + 1; p <= pInitTap2; p++)
		*p = bit;
}

/*
 * THE POSITION OF THESE TWO IS PART OF THE MEASUREMENT, NOT A TIDYING
 * CHOICE.  Finding 7815 measured that moving ONE inline function within the
 * headers this group reaches cost eight destructors their byte identity
 * while touching no destructor and no free, because an inline definition's
 * place in the translation unit is itself a lever-3 carrier.  The cell that
 * was scored put both definitions here, after `reset`, at the end of the
 * header; anything that moves them has to re-run the tree-wide SET diff.
 */
template <class T, class I>
void Scrambler<T, I>::resetHistoryIndexes()
{
	pOut = pInitOut;
	pTap1 = pInitTap1;
	pTap2 = pInitTap2;
}

template <class T, class I>
void Descrambler<T, I>::resetHistoryIndexes()
{
	pOut = pInitOut;
	pTap1 = pInitTap1;
	pTap2 = pInitTap2;
}

/*
 * OUT OF LINE FOR THE SAME REASON, AND THE OBJECT SAYS SO NINE TIMES.
 * `objdump -dr` over the blob finds NINE `R_386_PC32` call sites against
 * `..._15copyHistoryTailEv` and our object had ZERO -- every one of them
 * expanded in place, because an in-class body is implicitly `inline`.  It is
 * the same defect 7862 fixed for `resetHistoryIndexes` and it survived that
 * fix, which is why it is worth naming separately: moving one member out
 * makes the other's inlining VISIBLE in the `process` bodies' byte counts
 * and invisible to the bucket diff, since SIZE to SIZE moves no bucket.
 * Finding 7866.
 */
template <class T, class I>
void Scrambler<T, I>::copyHistoryTail()
{
	T *dst = pInitOut + 1;
	const T *src = pLimit;
	unsigned int n = tailLength;

	while (n--)
		*dst++ = *src++;
}

template <class T, class I>
void Descrambler<T, I>::copyHistoryTail()
{
	T *dst = pInitOut + 1;
	const T *src = pLimit;
	unsigned int n = tailLength;

	while (n--)
		*dst++ = *src++;
}

#endif /* DSPLIB_SCRAMBLER_H */
