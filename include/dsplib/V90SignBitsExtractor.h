/*
 * V90SignBitsExtractor.h -- the V.90 sign-bit extractor, so far as its
 * constructor and destructor need it.
 *
 * Reconstructed from dsplibs.o.  The class had no header and no .cpp in this
 * tree before this file; `.symtab` carries `V90SignBitsExtractor.cpp` as a
 * FILE entry, so the original had a translation unit of its own and this file
 * pair is named after it.
 *
 * THE OBJECT IS 0x28 = 40 BYTES, and the derivation is CONTAINMENT rather
 * than a displacement scan (finding 1107's rule, applied the other way up).
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
 * is a real member and there is no vptr (finding 228).
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
	 * Declared for the record and deliberately left undefined.  Their
	 * signatures come from the mangling and their return types are not
	 * mangled, so they are unknown.  They belong to whichever batch
	 * writes this class's processing half.  `ACTIONS` is the original
	 * author's own enum name, out of
	 * `_ZN20V90SignBitsExtractor16applyFrameActionENS_7ACTIONSEPhS1_`;
	 * its enumerators are not recoverable from the mangling.  The one
	 * below is a PLACEHOLDER so that the enum can be named at all -- C++
	 * has no opaque enum declaration before C++11 -- and its spelling
	 * says so.  Nothing may read it until `applyFrameAction` is written.
	 */
	enum ACTIONS { V90SBE_ACTION_NOT_YET_DERIVED = 0 };

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
	 * +0x00 .. +0x0f  NOT MODELLED.  `process` reads a byte at +0x00 and
	 * addresses +0x03, +0x04 and +0x08; `reset` writes +0x04 and +0x08.
	 * Nothing written here touches any of them, so per this tree's
	 * convention they stay a pad and an offset landing in one is itself
	 * the answer.
	 */
	unsigned char pad_00[0x10];

	/*
	 * +0x10  Zeroed by the constructor, `movl $0x0,0x10(%ebx)` -- a full
	 * 32-bit store, which is what makes it a word and not the first byte
	 * of one.  `reset`, `applyFrameAction` and `process` all read it.
	 */
	unsigned int word_10;

	unsigned char pad_14[4];	/* +0x14  read by applyFrameAction   */

	/*
	 * +0x18  Zeroed by the constructor, `movb $0x0,0x18(%ebx)` -- one
	 * byte, so a `char`-width flag and not a word.
	 */
	unsigned char byte_18;
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

/* The capacity the constructor fixes; see the member comment. */
#define V90SBE_DECODER_SIZE	6u

#endif /* DSPLIB_V90SIGNBITSEXTRACTOR_H */
