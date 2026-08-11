/*
 * V90SignBitsExtractor.cpp -- two of the five members of the V.90 sign-bit
 * extractor: the constructor and the destructor.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90SignBitsExtractor.h`
 * carries the object map and the evidence for the 0x28-byte size, which comes
 * from containment inside `V90Demapper` rather than from an allocation --
 * every instance of this class is embedded.
 *
 * THE OTHER THREE -- `reset`, `applyFrameAction` and `process`, 733 bytes --
 * are the class's processing half and are declared in the header for the
 * record, not defined here.  One class, one owner.
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
 * a mem-initialiser for +0x18 and a body assignment to +0x10 produce, in that
 * order, from the declaration order in the header.  It is a WEAK signal and
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

SBE_OFF(word_10,	0x10, word10);
SBE_OFF(byte_18,	0x18, byte18);
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
	: byte_18(0), decoder(V90SBE_DECODER_SIZE)
{
	word_10 = 0;
}

/*
 * Empty, and that is the whole function.  Its twenty-two bytes are the
 * implicit destruction of `decoder` at +0x1c and the frame around it; the
 * compiler emits the call and this file must not.
 */
V90SignBitsExtractor::~V90SignBitsExtractor()
{
}
