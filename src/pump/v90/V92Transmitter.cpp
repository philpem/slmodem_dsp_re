/*
 * V92Transmitter.cpp -- the V.92 transmit chain's construction and
 * destruction.
 *
 * Reconstructed from dsplibs.o.  Four symbols, 706 bytes:
 *
 *     V92Transmitter::V92Transmitter()   .text+0x53b90 (C1), +0x53c50 (C2)
 *     V92Transmitter::~V92Transmitter()  .text+0x53a30 (D2), +0x53ae0 (D1)
 *
 * C1 and C2 are byte-identical bar the register allocation of two argument
 * setups, and so are D1 and D2; GCC emits both from one definition.
 *
 * `include/dsplib/V92Transmitter.h` carries the object map, the 0x60 the
 * allocation gives and the evidence for every field.
 *
 * WHY THE SUB-OBJECTS ARE BUILT THROUGH asm() LABELS.  The same reason
 * src/pump/v90/V92Precoder.cpp gives in full: the blob's
 * `sysdep_malloc(n); ctor(p)` with no null test between is what GCC emits for
 * `new T(...)` over an inline `operator new`, the build is `-nostdinc++` with
 * no <new>, and a user-declared placement form makes GCC emit the null test
 * the blob does not have.  So each constructor is called by its mangled name
 * and each destructor through the explicit destructor-call syntax, which
 * needs no header.  The instruction sequence is the blob's either way.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL, `this` as the first stack argument
 * (`mov 0x20(%esp),%esi` after two pushes and a 0x14-byte frame), so nothing
 * here needs an attribute -- finding 215.
 */

#include <stddef.h>

#include "dsplib/V92Transmitter.h"
#include "dsplib/V92ModulusEncoder.h"
#include "dsplib/V92ConvolutionEncoder.h"
#include "dsplib/V92Precoder.h"
#include "dsplib/V92PreFilter.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

/*
 * The four constructors this one calls, by the names the blob's relocations
 * carry.  Every one is the C1 -- the complete-object variant a `new`
 * expression uses.
 */
void v92tx_moduluscoder_ctor(void *self) asm("_ZN17V92ModulusEncoderC1Ev");
void v92tx_convcoder_ctor(void *self) asm("_ZN21V92ConvolutionEncoderC1Ev");
void v92tx_precoder_ctor(void *self, unsigned int nTaps)
	asm("_ZN11V92PrecoderC1Ej");
void v92tx_prefilter_ctor(void *self, unsigned int nTaps)
	asm("_ZN12V92PreFilterC1Ej");
}

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` parses
 * `struct name {` out of include/dsplib and cannot see a C++ class, so the
 * class asserts its own (finding 230).  Guarded on a 32-bit pointer because
 * every field from +0x08 on is a pointer or comes after one; `make check64`
 * compiles this file for the host to prove the CODE does not depend on 32-bit.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92TX_OFF(field, off, tag) \
	typedef char v92tx_off_##tag[ \
	    ((int)__builtin_offsetof(V92Transmitter, field) == (off)) \
	    ? 1 : -1]

V92TX_OFF(pad_00,		0x00, pad00);
V92TX_OFF(word_04,		0x04, word04);
V92TX_OFF(buf_08,		0x08, buf08);
V92TX_OFF(word_0c,		0x0c, word0c);
V92TX_OFF(pad_10,		0x10, pad10);
V92TX_OFF(modulusEncoder,	0x48, modenc);
V92TX_OFF(precoder,		0x4c, precoder);
V92TX_OFF(preFilter,		0x50, prefilter);
V92TX_OFF(convolutionEncoder,	0x54, convenc);
V92TX_OFF(byte_58,		0x58, byte58);
V92TX_OFF(pad_5c,		0x5c, pad5c);

typedef char v92tx_size[(sizeof(V92Transmitter) == 0x60) ? 1 : -1];

/*
 * The five sizes the constructor allocates are the five classes' own, and
 * every one of them is already pinned by its own header from its own
 * allocation site.  Asserting them here is what makes `sizeof(X)` below a
 * faithful transcription of the blob's literal rather than a hope.
 */
typedef char v92tx_modenc_size[(sizeof(V92ModulusEncoder) == 0x54) ? 1 : -1];
typedef char v92tx_convenc_size[
	(sizeof(V92ConvolutionEncoder) == 0x2008) ? 1 : -1];
typedef char v92tx_precoder_size[(sizeof(V92Precoder) == 0x80) ? 1 : -1];
typedef char v92tx_prefilter_size[(sizeof(V92PreFilter) == 0x14) ? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * V92Transmitter::V92Transmitter (.text+0x53b90 / +0x53c50, 180 bytes)
 *
 * Six allocations and three stores of zero, in the object's own order:
 *
 *     malloc(0x50)  -> +0x08 ; +0x0c = 0 ; +0x04 = 0
 *     malloc(0x54)  -> V92ModulusEncoder()      -> +0x48
 *     malloc(1)     -> *p = 0                   -> +0x58
 *     malloc(0x2008)-> V92ConvolutionEncoder()  -> +0x54
 *     malloc(0x80)  -> V92Precoder(0x140)       -> +0x4c
 *     malloc(0x14)  -> V92PreFilter(0x140)      -> +0x50
 *
 * The two zero stores sit BETWEEN the first allocation and the second, which
 * is why they are written where they are: any other placement is a mutation
 * the store-order comparison cannot see, and the ordering is recorded here
 * because it is the object's, not because a test decides it.
 *
 * NOT ONE ALLOCATION IS CHECKED, the blob's included -- a null return walks
 * straight into the constructor call.  See docs/deviations.md.
 * ===========================================================================
 */
V92Transmitter::V92Transmitter()
{
	void *p;

	buf_08 = sysdep_malloc(V92TX_BUF08_BYTES);
	word_0c = 0;
	word_04 = 0;

	p = sysdep_malloc(sizeof(V92ModulusEncoder));
	v92tx_moduluscoder_ctor(p);
	modulusEncoder = (V92ModulusEncoder *)p;

	byte_58 = (unsigned char *)sysdep_malloc(1);
	*byte_58 = 0;

	p = sysdep_malloc(sizeof(V92ConvolutionEncoder));
	v92tx_convcoder_ctor(p);
	convolutionEncoder = (V92ConvolutionEncoder *)p;

	p = sysdep_malloc(sizeof(V92Precoder));
	v92tx_precoder_ctor(p, V92TX_FILTER_TAPS);
	precoder = (V92Precoder *)p;

	p = sysdep_malloc(sizeof(V92PreFilter));
	v92tx_prefilter_ctor(p, V92TX_FILTER_TAPS);
	preFilter = (V92PreFilter *)p;
}

/*
 * ===========================================================================
 * V92Transmitter::~V92Transmitter (.text+0x53a30 / +0x53ae0, 173 bytes)
 *
 * Six null-guarded releases, in an order that is NOT the constructor's: the
 * precoder, the pre-filter and the convolution encoder come back in a
 * different sequence from the one they were built in.  Three of the six get a
 * destructor call and three do not, and which three is the header's subject.
 *
 * `precoder = 0` after its free is the object's own and the only one of its
 * kind here; leaving the other five dangling is D210.  The null tests
 * themselves are not decoration and not free to omit -- this tree's
 * `sysdep_free` tolerates NULL, so dropping one leaves every byte of the
 * object unchanged and moves only `harness_alloc.free_null`, which is why
 * t_v92tx.cpp asserts that counter over all sixty-four null combinations.
 * ===========================================================================
 */
V92Transmitter::~V92Transmitter()
{
	if (buf_08 != 0)
		sysdep_free(buf_08);

	if (modulusEncoder != 0)
		sysdep_free(modulusEncoder);

	if (byte_58 != 0)
		sysdep_free(byte_58);

	if (precoder != 0) {
		precoder->~V92Precoder();
		sysdep_free(precoder);
		precoder = 0;
	}

	if (preFilter != 0) {
		preFilter->~V92PreFilter();
		sysdep_free(preFilter);
	}

	if (convolutionEncoder != 0) {
		convolutionEncoder->~V92ConvolutionEncoder();
		sysdep_free(convolutionEncoder);
	}
}
