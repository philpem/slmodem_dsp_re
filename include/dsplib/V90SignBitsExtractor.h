/*
 * V90SignBitsExtractor.h -- the V.90 sign-bit extractor.  Four of its five
 * members are written; `reset` is the one that is not.
 *
 * WHAT THE CLASS DOES, now that `process` is read.  One call is one V.90
 * frame of `width` sign bits.  It picks an `ACTIONS` from a two-state machine
 * and the frame's FIRST bit, applies that inversion pattern into its own
 * buffer at +0x08, runs the buffer through a per-position differential decoder
 * and then a serial one over the odd positions only, and hands the caller
 * positions 1..width-1 -- so the first bit is consumed as the state signal and
 * `width - 1` bits come out.  `V90Demapper::process` relies on that off-by-one
 * exactly, advancing its output pointer by `width - 1` per group.
 *
 * Reconstructed from dsplibs.o.  The class had no header and no .cpp in this
 * tree before this file; `.symtab` carries `V90SignBitsExtractor.cpp` as a
 * FILE entry, so the original had a translation unit of its own and this file
 * pair is named after it.
 *
 * THE OBJECT IS 0x28 = 40 BYTES, and the derivation is CONTAINMENT rather
 * than a displacement scan (finding F1107's rule, applied the other way up).
 * There is no `sysdep_malloc` to read: every instance is embedded.  The one
 * this tree can see is `V90Demapper`'s at +0x668, and the demapper's next
 * field is the error-sum array at +0x690 -- an offset that is not a bound
 * from a scan but the base of a 3,072-byte array indexed 0..767 by
 * `printErrorHistogramAndReset`.  So the extractor occupies exactly
 * 0x690 - 0x668 = 0x28 bytes.  The two agree from the other side too: the
 * last member is the parallel decoder at +0x1c, which is 12 bytes
 * (`state_`, `capacity_`, `size_` -- DiffCoder.h), and 0x1c + 0xc = 0x28
 * with no trailing padding.
 *
 * NOT POLYMORPHIC.  `nm` gives `D1` at 0x318e0 and `D2` at 0x318c0 and no
 * `D0`; GCC emits a deleting destructor only for a virtual class, so offset 0
 * is a real member and there is no vptr (finding F228).
 *
 * WHAT THE DESTRUCTOR IS.  Twenty-two bytes, and every one of them is the
 * compiler's:
 *
 *     318e0:  83 ec 0c        sub  $0xc,%esp
 *     318e3:  8b 44 24 10     mov  0x10(%esp),%eax
 *     318e7:  83 c0 1c        add  $0x1c,%eax
 *     318ea:  89 04 24        mov  %eax,(%esp)
 *     318ed:  e8 ..           call ParallelDifferentialDecoder<unsigned char>
 *                                   ::~ParallelDifferentialDecoder
 *     318f2:  83 c4 0c        add  $0xc,%esp
 *     318f5:  c3              ret
 *
 * -- the implicit destruction of the member at +0x1c and nothing else.  An
 * empty body is therefore the whole source, and the `+0x1c` in the object is
 * the only evidence that the member is at +0x1c; there is no store to read.
 * The value of writing it is that it is what makes `~V90Demapper` complete:
 * the demapper's destructor ends with `lea 0x668(%ebx),%ecx` and a call to
 * this symbol, which the compiler emits for us only if the member's type has
 * a destructor to call.
 */

#ifndef DSPLIB_V90SIGNBITSEXTRACTOR_H
#define DSPLIB_V90SIGNBITSEXTRACTOR_H

#include "dsplib/DiffCoder.h"

/*
 * The capacity the constructor fixes (`mov $0x6,%edx` at 0x31901) and, with
 * it, the most `width` can ever be -- so it is also the size of the frame
 * buffer at +0x08.  See both member comments.
 */
#define V90SBE_DECODER_SIZE	6u

class V90SignBitsExtractor {
public:
	/*
	 * The two lifecycle members this tree defines.  The signatures are
	 * the mangling's: `_ZN20V90SignBitsExtractorC1Ev` and
	 * `_ZN20V90SignBitsExtractorD1Ev`.
	 */
	V90SignBitsExtractor();
	~V90SignBitsExtractor();

	/*
	 * `ACTIONS` is the original author's own enum name, out of
	 * `_ZN20V90SignBitsExtractor16applyFrameActionENS_7ACTIONSEPhS1_`.
	 * ITS ENUMERATORS ARE NOT RECOVERABLE FROM THE MANGLING, so the four
	 * spellings below are DESCRIPTIVE and not the author's -- what is the
	 * object's is the four VALUES and what each one does, read off the
	 * switch at 0x319f9..0x31a0c and the `sete`/`movzbl` arms behind it:
	 *
	 *     0   out[i] = in[i]                       pass every position
	 *     1   out[i] = (in[i] == 0)                invert every position
	 *     2   out[i] = (i & 1) ? in[i] : !in[i]    invert the even ones
	 *     3   out[i] = (i & 1) ? !in[i] : in[i]    invert the odd ones
	 *
	 * The switch is over a SIGNED value -- `cmp $0x1,%ebx; jle` at
	 * 0x31a02 with the negative side falling into the default -- which is
	 * what an `enum` parameter compiles to and is why this is an enum and
	 * not an `unsigned`.  The default arm is not empty in the object: it
	 * still runs the loop, writing nothing, which is a `switch` inside a
	 * `for` and not the other way round.
	 */
	enum ACTIONS {
		V90SBE_PASS_ALL		= 0,
		V90SBE_INVERT_ALL	= 1,
		V90SBE_INVERT_EVEN	= 2,
		V90SBE_INVERT_ODD	= 3
	};

	void reset(unsigned int, unsigned int);
	void applyFrameAction(ACTIONS, unsigned char *, unsigned char *);
	void process(unsigned char *, unsigned char *);

	/*
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable from the mangling, and a
	 * single access section is what lets the .cpp assert every offset
	 * below with __builtin_offsetof.
	 */

	/*
	 * +0x00  `reset`'s first argument, stored before anything branches on
	 * it (`mov %edx,(%ebx)` at 0x3196e) and read by NOTHING else in the
	 * object.  It is the sign-bit SPACING: `width` below is 6 / it.
	 *
	 * THE OLD COMMENT HERE WAS WRONG AND IS WORTH RECORDING AS WRONG.  It
	 * read "`process` reads a byte at +0x00 and addresses +0x03, +0x04 and
	 * +0x08", which came from reading `0x0(%ebp)` -- the caller's `in`
	 * pointer -- and `lea 0x3(%ebx),%edi` -- an action number -- as object
	 * displacements.  Finding F3531.
	 */
	unsigned int spacing;

	/*
	 * +0x04  THE ACTIVE WIDTH, and every loop in this class runs
	 * `i < width` with an UNSIGNED comparison (`cmp %ecx,0x4(%esi); ja`).
	 * `reset` computes it as `6 / spacing` with a `div` and hands the same
	 * word to `ParallelDifferentialDecoder<unsigned char>::reset`, so it is
	 * both this class's loop bound and the decoder's active size.
	 */
	unsigned int width;

	/*
	 * +0x08  The frame under construction, `width` bytes of it.  Its base
	 * is `lea 0x8(%esi),%ebx` in `process`, which then indexes
	 * `(%ebx,%ecx,1)` for `i` in 0..width-1, hands the same base to the
	 * parallel decoder as BOTH its input and its output, and finally
	 * copies bits 1..width-1 out to the caller.
	 *
	 * SIX ELEMENTS AND NOT EIGHT.  Nothing in the object bounds the array
	 * itself -- +0x10 is the next field and that would allow eight -- but
	 * `width` is `6 / spacing` and `spacing` is at least 1, so six is the
	 * most the code can reach and the two trailing bytes are the alignment
	 * of the word at +0x10.
	 */
	unsigned char bits[V90SBE_DECODER_SIZE];
	unsigned char pad_0e[2];

	/*
	 * +0x10  THE TWO-STATE MACHINE `process` runs, and `reset`'s second
	 * argument is its seed.  Zeroed by the constructor with a full 32-bit
	 * `movl $0x0,0x10(%ebx)`, which is what makes it a word.  `process`
	 * reads it, chooses an `ACTIONS` from it and the frame's first byte,
	 * and writes back 0 or 1:
	 *
	 *     state 0:  action = state = (in[0] != 0)
	 *     state 1:  in[0] == 0 -> action INVERT_ODD,  state 1
	 *               in[0] != 0 -> action INVERT_EVEN, state 0
	 *
	 * A THIRD VALUE IS REACHABLE AND THE OBJECT DOES NOT HANDLE IT --
	 * `reset` will store any word here, and `process` falls through both
	 * tests with its action register never written.  docs/deviations.md
	 * D386.
	 */
	unsigned int state;

	/*
	 * +0x14  NOT MODELLED.  Nothing in the five members touches it.  The
	 * old comment claimed `applyFrameAction` read it; that was
	 * `0x14(%esp)`, the action argument on the stack.  Finding F3531.
	 */
	unsigned char pad_14[4];

	/*
	 * +0x18  A `SerialDifferentialDecoder<unsigned char>`, and the type is
	 * FORCED rather than inferred: `process` does
	 * `lea 0x18(%esi),%ebp` and hands that as the `this` of
	 * `_ZN25SerialDifferentialDecoderIhE7processEh` at 0x31b46.  It was
	 * `unsigned char byte_18` while only the constructor's
	 * `movb $0x0,0x18(%ebx)` was known -- and DiffCoder.h's file comment
	 * had already predicted exactly this, that the serial classes emit no
	 * constructor and an enclosing class value-initialising one gets the
	 * store inlined.  One byte, so the three that follow are padding.
	 *
	 * IT RUNS ON THE ODD POSITIONS ONLY, after the parallel decoder has
	 * run on all of them: `test $0x1,%bl; je` skips it for even `i`.
	 */
	SerialDifferentialDecoder<unsigned char> oddDecoder;
	unsigned char pad_19[3];

	/*
	 * +0x1c  The per-position sign-bit memory, capacity SIX -- one per
	 * sample of the V.90 frame -- which is the constructor's only
	 * argument to it (`mov $0x6,%edx` at 0x31901).  DiffCoder.h's file
	 * comment already recorded this class as one of the two callers that
	 * fix the capacity at 6.  Last member; the object ends where it does.
	 */
	ParallelDifferentialDecoder<unsigned char> decoder;
};

#endif /* DSPLIB_V90SIGNBITSEXTRACTOR_H */
