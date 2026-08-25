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

/*
 * THE REPLACEMENT `operator delete`, AND IT IS READ OFF THE OBJECT.  The blob
 * frees each owned sub-object with `if (p) { T::~T(p); sysdep_free(p); }`,
 * which is what GCC emits for `delete p` when `operator delete` is an inline
 * wrapper over `sysdep_free` -- and the blob defines and references no
 * `_ZdlPv` at all, so the codebase replaced the global operator.
 *
 * WRITING IT OUT BY HAND IS NOT EQUIVALENT, and that is the whole of finding
 * 7816's correction to this file's own older comment.  At the destructor's
 * LAST free the delete-expression emits an ordinary `call sysdep_free`; the
 * open-coded `p->~T(); sysdep_free(p);` emits a sibling `jmp` and drops the
 * frame with it.  refinement.md lever 7.
 */
extern "C" void sysdep_free(void *p);
inline void operator delete(void *p) { sysdep_free(p); }

/*
 * AND THE SIZED FORM, FOR THE MODERN BUILD ONLY.  C++14 added
 * `operator delete(void *, size_t)`, and GCC 13 calls it for `delete p` on a
 * class with a destructor -- an undefined `_ZdlPvj` in a tree that links no
 * libstdc++, which is the link failure `dsplib/Resampler.h` documents.
 *
 * `__cplusplus >= 201402L` is FALSE under GCC 3.4.2 (199711L), so the compiler
 * that decides byte identity never sees this.  It is portability plumbing and
 * carries no claim about the object.
 */
#if defined(__cplusplus) && __cplusplus >= 201402L
inline void operator delete(void *p, __SIZE_TYPE__) { sysdep_free(p); }
#endif

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
	delete fir;
	delete iir;
}

/*
 * Both filters, unconditionally -- neither pointer is tested here, although
 * the destructor tests both.  The second call is a tail jump in the object,
 * which is the compiler's business and not the source's.
 */
void
V92PreFilter::reset()
{
	fir->reset();
	iir->reset();
}

/*
 * The FIR takes the first pair and the IIR the second, and the two counts are
 * stored AFTER both calls: `mov %esi,0xc(%ebx)` and `mov %edi,0x10(%ebx)` sit
 * between the second call and the epilogue.  Neither filter's return value is
 * looked at.
 *
 * The counts stored are the caller's, not the ones the filters ended up with
 * -- FloatFIR and FloatIIR both round a tap count down to a multiple of four
 * and keep the result in their own fields.  So a caller asking for three taps
 * leaves +0x0c non-zero and the filter with none, and `process` still runs it.
 */
void
V92PreFilter::setCoefficients(float *coefFir, float *coefIir,
			      unsigned int tapsFirArg, unsigned int tapsIirArg)
{
	fir->setCoefficients(coefFir, tapsFirArg);
	iir->setCoefficients(coefIir, tapsIirArg);

	tapsFir = tapsFirArg;
	tapsIir = tapsIirArg;
}

/*
 * Twelve samples, by whichever of the four paths the two tap counts select.
 *
 * WHEN BOTH ARE ON THE FIR'S OUTPUT GOES TO A STACK BUFFER and the IIR reads
 * it: `lea 0x10(%esp),%ebx` inside a 0x40-byte frame, passed as the FIR's
 * output and then as the IIR's input.  Twelve floats is 0x30, which is
 * exactly what the frame has above +0x10.
 *
 * When neither is on the input is copied straight through, one word at a
 * time, with the loop's own `cmp $0xb,%edx; jle` for the count -- twelve
 * again, and not V92PREFILTER_SAMPLES read out of anywhere.
 */
void
V92PreFilter::process(float *in, float *out)
{
	float tmp[V92PREFILTER_SAMPLES];
	int i;

	if (tapsFir != 0) {
		if (tapsIir != 0) {
			fir->process(in, tmp, V92PREFILTER_SAMPLES);
			iir->process(tmp, out, V92PREFILTER_SAMPLES);
		} else {
			fir->process(in, out, V92PREFILTER_SAMPLES);
		}
	} else if (tapsIir != 0) {
		iir->process(in, out, V92PREFILTER_SAMPLES);
	} else {
		for (i = 0; i < V92PREFILTER_SAMPLES; i++)
			out[i] = in[i];
	}
}
