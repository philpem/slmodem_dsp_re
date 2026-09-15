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
 * bytes over 28 symbols are findings F869-872.
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
 * THE INTERMEDIATE IS `I`, NOT `T`, AND THAT IS FORCED (finding F870)
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
 * are BEHAVIOURALLY IDENTICAL -- every operand is already a byte, so the XOR
 * cannot exceed one byte.  The original's 32-bit XOR and register-held result
 * nevertheless identify the single-value temporary as `I`, too.
 *
 * The return type is not mangled, but the callers now pin its width:
 * V90Phase3Modulator::generateTRN1d and V92Phase3Modulator::generateTRN1u
 * both test EAX after calling `<h,i>::process(h)`.  A byte return tests AL.
 * The scalar Scrambler therefore returns `I`; `<h,h>` is unchanged and
 * `<i,h>` has no scalar instantiation.  Descrambler's independent declaration
 * is unchanged.  Differential tests check all byte-valued results, while the
 * caller disassembly is what distinguishes the ABI declarations.
 *
 * ---------------------------------------------------------------------------
 * THE CONSTRUCTOR AND DESTRUCTOR ARE DECLARED, and the reason this file used
 * to give for not declaring them was half right (finding F871).
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
	/**
	 * @brief Allocate the history buffer and seed it to all zero bits.
	 *
	 * `sysdep_malloc` is NOT checked, exactly as in the blob: a null
	 * return makes every init pointer a small address and the first
	 * reset() stores through it. Tail-calls `reset(0)`.
	 *
	 * @param a  Near tap's distance above `pInitOut`.
	 * @param b  Far tap's distance above `pInitOut`, and also `tailLength`.
	 * @param c  Distance `pInitOut` sits above `pLimit`.
	 */
	Scrambler(unsigned int a, unsigned int b, unsigned int c);

	/**
	 * @brief Free the history buffer.
	 *
	 * `pLimit` is NOT nulled, so a second destruction double-frees --
	 * the object's own behaviour.
	 */
	~Scrambler();

	/**
	 * @brief Put the three running pointers back to their initial values.
	 *
	 * Defined out of line (see the block below the class) because the
	 * object calls it rather than inlining it (finding F7862).
	 */
	void resetHistoryIndexes();

	/**
	 * @brief Carry the wrapped history back up to the restart point.
	 *
	 * Copies `tailLength` elements from `pLimit` to `pInitOut + 1`; see
	 * the out-of-line definition below for the exact loop shape the
	 * object requires (finding F7861).
	 */
	void copyHistoryTail();

	/**
	 * @brief Seed the history with one repeated bit.
	 *
	 * Fills the elements from `pInitOut + 1` up to and including
	 * `pInitTap2` with `value & 1` -- only the low bit of @p value can
	 * reach the buffer, masked in the blob before the loop.
	 *
	 * @param value  Bit to seed with (only bit 0 is used).
	 */
	void reset(T value);

	/**
	 * @brief Scramble one symbol.
	 *
	 * Both taps are read at their current positions and then stepped
	 * down; the output is written where `pOut` points and `pOut` steps
	 * down after. Falling below `pLimit` restarts the buffer
	 * (resetHistoryIndexes() then copyHistoryTail()).
	 *
	 * @param in  Input symbol.
	 * @return `in XOR *pTap1 XOR *pTap2`, the scrambled symbol.
	 */
	I process(T in);

	/**
	 * @brief Scramble @p n symbols in bulk.
	 *
	 * The result goes to BOTH `out[i]` -- as an `I`, which for
	 * `<int,unsigned char>` is narrower than the history -- and to the
	 * history at `pOut`, in that order; the blob stores `out[i]` first
	 * and `*pOut` second, reproduced even though it is observable only
	 * if the caller aims @p out into the history.
	 *
	 * Declared here and defined below the class, for the same
	 * out-of-line reason as resetHistoryIndexes().
	 *
	 * @param in   Input symbols, @p n of them.
	 * @param out  Output buffer, @p n entries.
	 * @param n    Number of symbols to process.
	 */
	void process(const T *in, I *out, unsigned int n);

	/**
	 * @brief Scramble @p n symbols of an all-ones input, in bulk.
	 *
	 * Open-coded rather than calling process() with a constant input --
	 * the blob's body is the bulk loop with `in[i]` replaced by 1. Only
	 * `<unsigned char, unsigned char>` instantiates this.
	 *
	 * @param out  Output buffer, @p n entries.
	 * @param n    Number of symbols to process.
	 */
	void processAllOnes(I *out, unsigned int n);

	/**
	 * @brief Scramble @p n symbols of an all-zeros input, in bulk.
	 *
	 * Open-coded rather than calling process() with a constant input --
	 * the blob's body is the bulk loop with `in[i]` replaced by 0. Only
	 * `<unsigned char, unsigned char>` instantiates this.
	 *
	 * @param out  Output buffer, @p n entries.
	 * @param n    Number of symbols to process.
	 */
	void processAllZeros(I *out, unsigned int n);

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable (finding F226) and because one access section is what
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
	/**
	 * @brief Allocate the history buffer and seed it to all zero bits.
	 *
	 * Byte for byte the shape of Scrambler::Scrambler(); see its @brief
	 * for what each argument means.
	 */
	Descrambler(unsigned int a, unsigned int b, unsigned int c);

	/** @brief Free the history buffer. Same as Scrambler::~Scrambler(). */
	~Descrambler();

	/**
	 * @brief Put the three running pointers back to their initial values.
	 * Three word copies, as in Scrambler; out of line for the same reason.
	 */
	void resetHistoryIndexes();

	/**
	 * @brief Carry the wrapped history back up to the restart point.
	 * The count is `tailLength`, as in Scrambler.
	 */
	void copyHistoryTail();

	/**
	 * @brief Seed the history with one repeated bit.
	 *
	 * Fills the elements from `pInitOut + 1` up to and including
	 * `pInitTap2` with `value & 1` -- only the low bit of @p value can
	 * reach the buffer, masked in the blob before the loop.
	 *
	 * @param value  Bit to seed with (only bit 0 is used).
	 */
	void reset(T value);

	/**
	 * @brief Descramble one symbol.
	 *
	 * Stores @p in at `pOut`, reads it back, XORs the two taps, then
	 * steps all three pointers down. Falling below `pLimit` restarts the
	 * buffer.
	 *
	 * @param in  Input symbol.
	 * @return The descrambled symbol.
	 */
	T process(T in);

	/**
	 * @brief Descramble @p n symbols in bulk.
	 *
	 * The result goes to `out[i]` as an `I`, and nowhere else (unlike
	 * Scrambler's bulk process(), which also updates the history from
	 * the same value).
	 *
	 * @param in   Input symbols, @p n of them.
	 * @param out  Output buffer, @p n entries.
	 * @param n    Number of symbols to process.
	 */
	void process(const T *in, I *out, unsigned int n);

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
 * called `reset` too.  Finding F5805.
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
 * against 19 -- which is finding F7480's rule reading exactly right: an excess
 * of instructions with a missing call is an inlining difference and not a
 * missing statement.
 *
 * `Descrambler`'s bulk `process` MOVES OUT TOO, and the sentence that used to
 * stand here -- "the blob carries no `Descrambler<...>::process(const ...)`
 * symbol at all" -- was false when it was written.  See the block at the foot
 * of this file.
 * ---------------------------------------------------------------------------
 */
/*
 * THE BULK LOOPS ARE `while (n--)` WITH BOTH POINTERS WALKING, and that is
 * 7861's decoded fact applied to the loop nobody applied it to.
 *
 * 7861 read `copyHistoryTail`'s `dec %edx; cmp $0xffffffff,%edx; jne` as
 * `while (n--)` with the loop rotated so entry lands on the test, and closed
 * five symbols on it.  Every bulk member has the SAME entry sequence --
 * `dec %edi; cmp $0xffffffff,%edi; je` -- and ours had `cmp %ebp,%edi; jb`
 * off an index.  The object also walks both pointers rather than indexing
 * them: `xor (%eax),%dl` beside `incl 0x24(%esp)` for the input, and
 * `mov %dl,0x0(%ebp); inc %ebp` for the output.
 *
 * `in++` IS ITS OWN STATEMENT, AFTER BOTH TAP STORES, and that position is a
 * unique preimage.  The blob spends it at
 *
 *     dec %eax / mov %eax,0x18(%esi) / mov %ecx,0x14(%esi) /
 *     incl 0x24(%esp) / mov %dl,0x0(%ebp)
 *
 * Twenty-two cells over three rounds, six distinct emissions: the loop shape
 * (`while (n--)`, `for (i...)`, `for (; n; n--)`) crossed with the input
 * spelling (`*in++` in the XOR, `*in` with `in++` at each of five positions,
 * `in[0]`) and the output spelling (`*out++ = r` against `*out = r; out++;`).
 * Only position 2 emits the object's order; `*out++ = r` and the split form
 * emit the SAME BYTES, so what is decoded is the position and not that
 * spelling.  `Scrambler<int,unsigned char>::process` is EXACT on it.
 *
 * `Descrambler`'s bulk loop is a DIFFERENT body -- it stores the input and
 * reads it back -- and it wants the input walked INSIDE the store,
 * `*pOut = *in++`, which its own five positions all lose to (25 differing
 * bytes against 66 to 72).  The two templates differ here because the object
 * says they differ, exactly as they do over `process`'s in-class status.
 */
template <class T, class I>
void Scrambler<T, I>::process(const T *in, I *out, unsigned int n)
{
	while (n--) {
		T *p = pOut;
		I r = (I)(*in ^ *pTap1 ^ *pTap2);

		pTap1--;
		pTap2--;
		in++;
		*out++ = r;
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
 * Finding F7863.
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
 * CHOICE.  Finding F7815 measured that moving ONE inline function within the
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
 * Finding F7866.
 */
/*
 * `src` IS DECLARED BEFORE `dst`, AND THAT ONE FACT CLOSED ALL FIVE.  Both
 * bodies emitted the object's sixteen instructions in the object's order with
 * the two pointers in each other's registers -- the blob puts `src` (`pLimit`,
 * +0x00) in `%ebx` and `dst` (`pInitOut + 1`, +0x04) in `%ecx`, and ours had
 * them the other way round in every instantiation.  `--why` called it
 * REGALLOC, which is the bucket this file's brief says not to chase; the
 * register choice here is not free, it follows the DECLARATION ORDER of the
 * two locals, and nothing else in the body reaches it.
 *
 * Eighteen cells, enumerated to completion before any was read: 3! orders of
 * the three declarations crossed with three loop-body spellings (`*dst++ =
 * *src++`, and the split form with each increment order).  The `while (n--)`
 * entry shape is held fixed because F7861 decoded it.
 *
 *     src BEFORE dst  (SDN, SND, NSD)  x  all three loops   ->  EXACT, 9 cells
 *     dst BEFORE src  (DSN, DNS, NDS)  x  all three loops   ->  7 differing
 *                                                               bytes, 9 cells
 *
 * TWO distinct emissions over the whole domain, and the split is exactly on
 * the pair's relative order: `n`'s position is free and so is the loop
 * spelling, all three of which emit the SAME BYTES.  So what is decoded is
 * the ORDER OF THE PAIR and not the whole declaration list -- F0's
 * several-preimages case, and the finding says which fact.  Finding F8140.
 */
template <class T, class I>
void Scrambler<T, I>::copyHistoryTail()
{
	const T *src = pLimit;
	T *dst = pInitOut + 1;
	unsigned int n = tailLength;

	while (n--)
		*dst++ = *src++;
}

template <class T, class I>
void Descrambler<T, I>::copyHistoryTail()
{
	const T *src = pLimit;
	T *dst = pInitOut + 1;
	unsigned int n = tailLength;

	while (n--)
		*dst++ = *src++;
}

/*
 * ===========================================================================
 * AND THE REMAINING `process` MEMBERS, ON THE RELOCATION COUNT AND NOTHING
 * ELSE
 * ===========================================================================
 *
 * `objdump -dr` over the blob against ours, `R_386_PC32` sites per mangled
 * name -- 7867's screening test, which is a COUNT and not a judgement about
 * which members "look inlineable":
 *
 *     _ZN9ScramblerIhiE7processEh            blob 22   ours 0
 *     _ZN9ScramblerIhhE14processAllOnesEPhj  blob 20   ours 0
 *     _ZN11DescramblerIiiE7processEi         blob 10   ours 0
 *     _ZN9ScramblerIhhE7processEh            blob  9   ours 0
 *     _ZN9ScramblerIhhE15processAllZerosEPhj blob  7   ours 0
 *     _ZN11DescramblerIhiE7processEh         blob  5   ours 0
 *     _ZN11DescramblerIhiE7processEPKhPij    blob  2   ours 0
 *
 * 7867 warns that a raw 0-against-N list cannot tell "we expand it" from "we
 * have not written anybody who would call it", so every site was traced to
 * the blob function containing it and intersected with what this tree
 * defines: 65 of the 75 are in functions we have written, and the TEN that
 * are not are all still-unwritten generators -- seven
 * `V9xPhase3Modulator::generate*` for `process(h)`, and
 * `V90Phase4Modulator`'s `generateB1d`, `generateTRN2d` and `generateEd` for
 * the two constant-input forms.  Not one of these names is 7867's
 * unwritten-caller noise.
 *
 * `Descrambler`'s BULK `process` moves too, and the paragraph above
 * `Scrambler`'s own bulk form used to say it must not, on the ground that the
 * blob carries no `Descrambler<...>::process(const ...)` symbol at all.  That
 * was WRONG and this file's own member table always contradicted it:
 * `_ZN11DescramblerIhiE7processEPKhPij` is a real 120-byte weak symbol with
 * two call sites in `V90Demodulator::progress`.  The claim is deleted rather
 * than corrected in place, because it was never true.
 *
 * The definitions are APPENDED here rather than placed beside their siblings:
 * 7815 measured that moving one inline definition within this family of
 * headers cost eight destructors their byte identity, so nothing already in
 * this file changes position.
 */
/*
 * THE CONSTRUCTOR AND DESTRUCTOR ARE OUT OF LINE ON THE SAME COUNT, and both
 * are the loudest rows the screen has:
 *
 *     ~Scrambler / ~Descrambler   blob 26 sites over five instantiations, ours 0
 *     Scrambler / Descrambler     blob 16 sites over five instantiations, ours 0
 *
 * Every one of the 42 is inside a `V90Modulator`, `V92Modulator`,
 * `V90Demodulator`, `V9xPhase3Modulator`, `V9xPhase4Modulator` or
 * `V90Phase3Demodulator` constructor or destructor that this tree defines, so
 * none of them is 7867's unwritten-caller noise.
 */
template <class T, class I>
Scrambler<T, I>::Scrambler(unsigned int a, unsigned int b, unsigned int c)
{
	tailLength = b;
	/* Keep the element count separate from the byte-size conversion.
	 * The period object adds the guard element before scaling; see
	 * docs/scrambler-allocation-retained-result.md for the full-TU controls. */
	unsigned int count = b + c + 1;
	pLimit = (T *)sysdep_malloc(count * sizeof(T));
	pInitOut = pLimit + c;
	pInitTap1 = pInitOut + a;
	pInitTap2 = pInitOut + b;
	reset(0);
}

template <class T, class I>
Scrambler<T, I>::~Scrambler()
{
	delete[] pLimit;
}

template <class T, class I>
Descrambler<T, I>::Descrambler(unsigned int a, unsigned int b, unsigned int c)
{
	tailLength = b;
	pLimit = (T *)sysdep_malloc((1 + b + c) * sizeof(T));
	pInitOut = pLimit + c;
	pInitTap1 = pInitOut + a;
	pInitTap2 = pInitOut + b;
	reset(0);
}

template <class T, class I>
Descrambler<T, I>::~Descrambler()
{
	delete[] pLimit;
}

template <class T, class I>
I Scrambler<T, I>::process(T in)
{
	T *out = pOut;
	I r;

	r = (I)(in ^ *pTap1 ^ *pTap2);
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
 * `pTap2` IS DECREMENTED BEFORE `pTap1` IN BOTH OF THESE, WHICH IS THE ORDER
 * `Descrambler`'s BULK LOOP ALREADY USES AND THE OPPOSITE OF `Scrambler`'s OWN
 * -- and it is the object that says so.  The tell is the WRITE-BACK: GCC
 * hoists both taps into registers for the whole loop body and stores them
 * back in the order the source decrements them, so the blob's
 *
 *     mov %eax,0x18(%esi) / mov %ecx,0x14(%esi)      pTap2 then pTap1
 *
 * against our `0x14` then `0x18` is the source order read straight off the
 * object.  The two taps swapping registers (blob `pTap2`->%eax,
 * `pTap1`->%ecx) is the same fact and not a second one.
 *
 * Thirty-two cells, enumerated before any was read: 2 decrement orders x 4 xor
 * spellings (which tap is named first, crossed with whether the `1` leads or
 * trails) x 2 local declaration orders x 2 orders of the two stores.  Eight
 * distinct emissions, FOUR of them the object:
 *
 *     pTap2-- first, taps named pTap1 then pTap2, `*out++ = r` before
 *     `*p = (T)r`                                          EXACT, 4 cells
 *     pTap1-- first, everything else held                  8 differing bytes
 *     taps named pTap2 then pTap1, pTap2-- first           8 differing bytes
 *     `*p = (T)r` before `*out++ = r`                      8 to 42
 *
 * The four that reach zero are the two declaration orders of `p` and `r`
 * crossed with the two parenthesisations of the xor -- both FREE, they emit
 * the same bytes.  So three facts are decoded (the decrement order, which tap
 * the xor names first, and that the caller's store comes before the history
 * store) and two spellings are not.  F0's several-preimages case.
 * Finding F8142.
 *
 * `Scrambler<h,h>::process(const T *, I *, j)` does NOT move with them: it
 * stays REGALLOC at 8 differing bytes in all thirty-two cells, and its own
 * `<i,h>` twin is EXACT from the same template text, so its residual is not a
 * spelling this file can reach.
 */
template <class T, class I>
void Scrambler<T, I>::processAllOnes(I *out, unsigned int n)
{
	while (n--) {
		T *p = pOut;
		I r = (I)(1 ^ *pTap1 ^ *pTap2);

		pTap2--;
		pTap1--;
		*out++ = r;
		*p = (T)r;
		if (--pOut < pLimit) {
			resetHistoryIndexes();
			copyHistoryTail();
		}
	}
}

template <class T, class I>
void Scrambler<T, I>::processAllZeros(I *out, unsigned int n)
{
	while (n--) {
		T *p = pOut;
		I r = (I)(*pTap1 ^ *pTap2);

		pTap2--;
		pTap1--;
		*out++ = r;
		*p = (T)r;
		if (--pOut < pLimit) {
			resetHistoryIndexes();
			copyHistoryTail();
		}
	}
}

template <class T, class I>
T Descrambler<T, I>::process(T in)
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

/*
 * `pTap2` IS DECREMENTED BEFORE `pTap1` HERE, WHICH IS THE OTHER WAY ROUND
 * FROM `Scrambler`'s BULK LOOP ABOVE -- and it is the object that says so,
 * not a preference.  Nine cells over this body, enumerated before any was
 * read: the two decrement orders, the two xor associations, the four
 * placements of a post-decrement inside the expression, and `*out++ = r`
 * moved past the decrements.
 *
 *     pTap1 first (the tree's own order)          25 differing bytes of 120
 *     xor operands swapped, pTap1 first           10
 *     `*out++ = r` before the decrements          32
 *     the xor re-associated                       SIZE, 136 against 120
 *     pTap2 first                                  6   <-- four cells
 *
 * The four that reach 6 are `pTap2--; pTap1--;`, the same with explicit
 * parentheses, `*pTap2--` written into the expression, and both taps
 * post-decremented in it; they emit the SAME BYTES.  So what is decoded is
 * the ORDER and not which of the four the author typed -- F0's
 * several-preimages case.
 *
 * F8046 left six bytes: the updated output-pointer spill and the `pOut` member
 * store were adjacent but scheduled in the opposite order.  Phase 4 then
 * exhausted seven natural spellings of that residual.  Keeping `--pOut`
 * inside the condition leaves six bytes with either `*out++ = r` or its split
 * form; moving the output increment after the restart block or introducing
 * `nextOut`/destination locals changes the instruction stream.  The two exact
 * preimages both spell `--pOut` as the preceding statement, followed by the
 * output store, with post-increment and split output forms emitting the same
 * bytes.  The compact one is retained below.  Finding F10210.
 */
template <class T, class I>
void Descrambler<T, I>::process(const T *in, I *out, unsigned int n)
{
	while (n--) {
		I r;

		*pOut = *in++;
		r = (I)(*pOut ^ *pTap1 ^ *pTap2);
		pTap2--;
		pTap1--;
		--pOut;
		*out++ = r;
		if (pOut < pLimit) {
			resetHistoryIndexes();
			copyHistoryTail();
		}
	}
}

#endif /* DSPLIB_SCRAMBLER_H */
