/*
 * V90SignBitsExtractor.cpp -- four of the five members of the V.90 sign-bit
 * extractor: the constructor, the destructor, `applyFrameAction` and
 * `process`.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90SignBitsExtractor.h`
 * carries the object map and the evidence for the 0x28-byte size, which comes
 * from containment inside `V90Demapper` rather than from an allocation --
 * every instance of this class is embedded.
 *
 * THE ONE STILL OUTSTANDING is `reset`, 119 bytes, declared in the header for
 * the record and not defined here.  It is what sets `spacing`, `width` and
 * `state`, so every field this file's `process` reads is seeded there and by
 * nothing else.  One class, one owner.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215):
 * `mov 0x10(%esp),%ebx` after one push and an eight-byte frame.
 *
 * WHY THE CONSTRUCTOR IS HERE AT ALL, since the batch that wrote this file was
 * asked for the destructor path.  It is what allocates the block the
 * destructor frees, so construct-then-destruct is the only shape in which the
 * destructor's free is provably freeing what it should rather than freeing a
 * pointer the test planted; and it is the only evidence in the object for
 * +0x10 and +0x18, on a class that had no header at all.  Forty-four bytes,
 * and its one call -- `ParallelDifferentialDecoder<unsigned char>` -- is
 * already written and already tested (`t_diffcoder`).
 *
 * THE ORDER OF THE THREE STORES IS THE OBJECT'S:
 *
 *     31910:  c6 43 18 00           movb $0x0,0x18(%ebx)
 *     3191b:  e8 ..                 call ParallelDifferentialDecoder<
 *                                          unsigned char>::PDD(unsigned)
 *     31920:  c7 43 10 00 00 00 00  movl $0x0,0x10(%ebx)
 *
 * -- +0x18 before the member's constructor and +0x10 after it, which is what
 * a mem-initialiser for `oddDecoder` and a body assignment to `state`
 * produce, in that order, from the declaration order in the header.  It is a WEAK signal and
 * recorded as one: GCC does not preserve statement order (finding 617), so a
 * matching order confirms this spelling and does not prove it is the only one.
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90SignBitsExtractor.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own.  Skipped on the 64-bit `check64` pass, where a 32-bit layout is
 * not what the compiler lays out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define SBE_OFF(field, off, tag) \
	typedef char v90sbe_off_##tag[ \
	    ((int)__builtin_offsetof(V90SignBitsExtractor, field) == (off)) \
	    ? 1 : -1]

SBE_OFF(spacing,	0x00, spacing);
SBE_OFF(width,		0x04, width);
SBE_OFF(bits,		0x08, bits);
SBE_OFF(state,		0x10, state);
SBE_OFF(oddDecoder,	0x18, odddec);
SBE_OFF(decoder,	0x1c, decoder);

/*
 * The size is the claim finding 1107 says to make loudly, because here it is
 * the containment argument and not an allocation: 0x690 - 0x668 inside
 * `V90Demapper`, and 0x1c + sizeof(ParallelDifferentialDecoder<unsigned
 * char>) from this side.
 */
typedef char v90sbe_size[(sizeof(V90SignBitsExtractor) == 0x28) ? 1 : -1];
#endif

V90SignBitsExtractor::V90SignBitsExtractor()
	: oddDecoder(), decoder(V90SBE_DECODER_SIZE)
{
	state = 0;
}

/*
 * Empty, and that is the whole function.  Its twenty-two bytes are the
 * implicit destruction of `decoder` at +0x1c and the frame around it; the
 * compiler emits the call and this file must not.
 */
V90SignBitsExtractor::~V90SignBitsExtractor()
{
}

/*
 * ===========================================================================
 * THE PROCESSING HALF -- two of the three, 564 bytes of the object.  `reset`
 * is the one still outstanding.
 * ===========================================================================
 */

/*
 * `applyFrameAction` -- 147 bytes at 0x319e0.  One inversion pattern applied
 * across `width` positions.
 *
 * THE SWITCH IS INSIDE THE LOOP AND NOT OUTSIDE IT, and the object's shape is
 * the compiler's, not the source's.  What is written here is one `for` with a
 * four-way `switch` in it; `-O3` unswitches that into four copies of the loop
 * plus a fifth for the default, which is why the object has five loops and
 * why the default one runs `width` times writing nothing.  A source with the
 * switch outermost would give the same five loops and would NOT explain the
 * empty one -- an outermost switch with no `default:` would skip the loop
 * entirely.
 *
 * THE SELECTOR IS SIGNED.  `cmp $0x1,%ebx; je; jle` sends everything below 1
 * to a test for zero and everything else to the default, so a value of -1
 * lands in the default arm rather than wrapping into the table.  That is an
 * `enum` (or an `int`), never an `unsigned`.
 *
 * ITS THREE ARGUMENTS ARE THE MANGLING'S: `NS_7ACTIONSEPhS1_`.
 */
void
V90SignBitsExtractor::applyFrameAction(ACTIONS action, unsigned char *in,
				       unsigned char *out)
{
	unsigned int i;

	for (i = 0; i < width; i++) {
		switch (action) {
		case V90SBE_PASS_ALL:
			out[i] = in[i];
			break;
		case V90SBE_INVERT_ALL:
			out[i] = (unsigned char)(in[i] == 0);
			break;
		case V90SBE_INVERT_EVEN:
			out[i] = (i & 1) ? in[i]
					 : (unsigned char)(in[i] == 0);
			break;
		case V90SBE_INVERT_ODD:
			out[i] = (i & 1) ? (unsigned char)(in[i] == 0)
					 : in[i];
			break;
		}
	}
}

/*
 * `process` -- 417 bytes at 0x31ab0.  One V.90 frame of sign bits in,
 * `width - 1` decoded bits out.
 *
 * `applyFrameAction` IS CALLED, NOT COPIED.  The object contains its four
 * arms twice: once as the symbol above, and once here, writing into +0x08
 * instead of the caller's buffer.  The two copies are the same instructions
 * in the same order down to the `sete`/`movzbl` split on `i & 1`, which is
 * what an `-O3` inline of a same-translation-unit member looks like -- and
 * the out-of-line copy still exists because the member is external and could
 * be called from anywhere.  Writing the switch out a second time here would
 * reproduce the object and lose that.
 *
 * THE FIRST BIT IS THE STATE SIGNAL AND IS NOT AN OUTPUT.  `in[0]` chooses
 * the action and the next state, and the final copy runs `j` from ONE
 * (`mov $0x1,%edx` at 0x31bb5, storing to `-0x1(%ecx,%edx,1)`), so `width`
 * bits go in and `width - 1` come out.
 *
 * THE TWO DIFFERENTIAL DECODERS ARE NOT ALTERNATIVES.  The parallel one at
 * +0x1c runs over every position, in place -- the object passes +0x08 as both
 * its arguments -- and the serial one at +0x18 then runs over the ODD
 * positions only, which is the `test $0x1,%bl; je` at 0x31b32.  The even
 * positions still go through the store (`movzbl 0x8(%esi,%ebx,1),%eax; mov
 * %al,0x8(%esi,%ebx,1)` -- a byte read back to where it came from), which is
 * what a `x = cond ? f(x) : x` compiles to and is why it is written that way.
 *
 * A THIRD STATE IS REACHABLE AND THE OBJECT DOES NOT HANDLE IT.  `reset`
 * stores its second argument into `state` unfiltered, and with anything but 0
 * or 1 there the object falls past both tests and switches on whatever its
 * `%edi` happened to hold on entry.  This reconstruction gives `action` a
 * defined value instead -- one instruction, and no behaviour that is defined
 * on both sides.  docs/deviations.md D386.
 */
void
V90SignBitsExtractor::process(unsigned char *in, unsigned char *out)
{
	ACTIONS action = V90SBE_PASS_ALL;
	unsigned int i;

	if (state == 0) {
		/*
		 * `setne %al; movzbl %al,%edi; mov %edi,0x10(%esi)` -- one
		 * value, used as both the action and the next state, so the
		 * two are the same expression and not two.
		 */
		action = (ACTIONS)(in[0] != 0);
		state = (unsigned int)action;
	} else if (state == 1) {
		/*
		 * Two independent computations of the same predicate in the
		 * object -- `cmp $0x1,%dl; sbb; not; lea 0x3(%ebx)` for the
		 * action and `test %dl,%dl; sete %cl` for the state.  A source
		 * that wrote `action = state + 2` would have given one `lea`
		 * off the other.
		 */
		if (in[0] == 0) {
			action = V90SBE_INVERT_ODD;
			state = 1;
		} else {
			action = V90SBE_INVERT_EVEN;
			state = 0;
		}
	}

	applyFrameAction(action, in, bits);

	decoder.process(bits, bits);

	for (i = 0; i < width; i++)
		bits[i] = (i & 1) ? oddDecoder.process(bits[i]) : bits[i];

	for (i = 1; i < width; i++)
		out[i - 1] = bits[i];
}
