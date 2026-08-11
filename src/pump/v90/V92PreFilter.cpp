/*
 * V92PreFilter.cpp -- V.92 transmit pre-filter: construction and destruction.
 *
 * Reconstructed from dsplibs.o.  Two of the class's five members;
 * `include/dsplib/V92PreFilter.h` carries the object map, the evidence for
 * it, and the reason the second sub-object is a FloatIIR and not a second
 * FloatFIR -- which is a relocation and not anything a test can see.
 *
 * The sub-objects are built through an asm() label for the reason
 * src/pump/v90/V92Precoder.cpp gives at length: the original almost certainly
 * wrote `new`/`delete` over an allocator hooked to sysdep_malloc, and this
 * build has no <new> to spell that with.  The instruction sequence is the
 * blob's either way.
 */

#include <stddef.h>

#include "dsplib/V92PreFilter.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

void v92prefilter_floatfir_ctor(void *self, unsigned int nTaps, float *coef,
				unsigned int blockSize)
	asm("_ZN8FloatFIRC1EjPfj");
void v92prefilter_floatiir_ctor(void *self, unsigned int ncoeff, float *coeff,
				unsigned int blockSize)
	asm("_ZN8FloatIIRC1EjPfj");
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92PF_OFF(field, off, tag) \
	typedef char v92pf_off_##tag[ \
	    ((int)__builtin_offsetof(V92PreFilter, field) == (off)) ? 1 : -1]

V92PF_OFF(word_00,  0x00, word_00);
V92PF_OFF(fir,      0x04, fir);
V92PF_OFF(iir,      0x08, iir);
V92PF_OFF(tapsFir,  0x0c, tapsfir);
V92PF_OFF(tapsIir,  0x10, tapsiir);
typedef char v92pf_size[(sizeof(V92PreFilter) == 0x14) ? 1 : -1];
typedef char v92pf_fir_size[(sizeof(FloatFIR) == 0x14) ? 1 : -1];
typedef char v92pf_iir_size[(sizeof(FloatIIR) == 0x14) ? 1 : -1];
#endif

/*
 * A FIR then an IIR, both with the constructor's one tap count and 99 samples
 * of slack, and nothing else: the two counts at +0x0c and +0x10 that decide
 * whether either filter runs are `setCoefficients`' business and are left as
 * they were found.
 *
 * The allocations are not checked, exactly as in the blob; see
 * docs/deviations.md.
 */
V92PreFilter::V92PreFilter(unsigned int nTaps)
{
	FloatFIR *f;
	FloatIIR *i;

	f = (FloatFIR *)sysdep_malloc(sizeof(FloatFIR));
	v92prefilter_floatfir_ctor(f, nTaps, 0, V92PREFILTER_BLOCK);
	fir = f;

	i = (FloatIIR *)sysdep_malloc(sizeof(FloatIIR));
	v92prefilter_floatiir_ctor(i, nTaps, 0, V92PREFILTER_BLOCK);
	iir = i;
}

/*
 * Both filters, in the order they were allocated, each guarded by a null test
 * that changes no bytes and only `harness_alloc.free_null` -- see the same
 * note in V92Precoder.cpp.  Neither pointer is nulled afterwards.
 */
V92PreFilter::~V92PreFilter()
{
	if (fir != 0) {
		fir->~FloatFIR();
		sysdep_free(fir);
	}
	if (iir != 0) {
		iir->~FloatIIR();
		sysdep_free(iir);
	}
}
