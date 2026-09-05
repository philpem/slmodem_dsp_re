/*
 * V92PreFilter.cpp -- V.92 transmit pre-filter: construction and destruction.
 *
 * Reconstructed from dsplibs.o.  Two of the class's five members;
 * `include/dsplib/V92PreFilter.h` carries the object map, the evidence for
 * it, and the reason the second sub-object is a FloatIIR and not a second
 * FloatFIR -- which is a relocation and not anything a test can see.
 *
 * The sub-objects are built with ordinary placement `new` over an allocator
 * hooked to sysdep_malloc (finding F10155/F10157): this build is
 * `-nostdinc++` with no <new>, so `include/dsplib/sysdep.h` declares the
 * shared non-throw placement `operator new`/`operator delete` pair every
 * such site in this tree uses.  This file used to reach both constructors
 * through hand-mangled `asm("_ZN...")` labels on the belief (finding F1340)
 * that a user-declared placement `operator new` would force a null test the
 * blob does not have; F10155 retracts that empirically.
 */

#include <stddef.h>

#include "dsplib/V92PreFilter.h"
#include "dsplib/sysdep.h"

/*
 * THE REPLACEMENT `operator delete`, AND IT IS READ OFF THE OBJECT.  The blob
 * frees each owned sub-object with `if (p) { T::~T(p); sysdep_free(p); }`,
 * which is what GCC emits for `delete p` when `operator delete` is an inline
 * wrapper over `sysdep_free` -- and the blob defines and references no
 * `_ZdlPv` at all, so the codebase replaced the global operator.
 *
 * WRITING IT OUT BY HAND IS NOT EQUIVALENT, and that is the whole of finding
 * F7816's correction to this file's own older comment.  At the destructor's
 * LAST free the delete-expression emits an ordinary `call sysdep_free`; the
 * open-coded `p->~T(); sysdep_free(p);` emits a sibling `jmp` and drops the
 * frame with it.  refinement.md lever 7.
 */
extern "C" void sysdep_free(void *p);
inline void operator delete(void *p) { sysdep_free(p); }

/*
 * NO SIZED `operator delete` HERE, AND NOWHERE ELSE EITHER.  C++14 made GCC
 * 13 prefer `operator delete(void *, size_t)` for the delete-expressions
 * below -- an undefined `_ZdlPvj` in a tree that links no libstdc++ -- and
 * this file used to carry a `#if __cplusplus >= 201402L` block answering it.
 * The Makefile passes `-fno-sized-deallocation` instead, so the modern build
 * resolves `delete p` the way GCC 3.4.2 resolves it: to the UNSIZED operator
 * just above.  C++14 postdates the object by sixteen years and cannot be the
 * author's, so the shim was apparatus inside the reconstruction; the flag is
 * apparatus where apparatus belongs.  Finding F7900.
 *
 * THE UNSIZED OPERATORS ARE A DIFFERENT CASE AND STAY HERE, one copy per
 * file -- the one above, and any `operator delete[]` this file defines.  The
 * period compiler DOES see those, and an inline definition's POSITION in the
 * translation unit is a lever-3 carrier: consolidating the unsized array form
 * into `sysdep.h` cost eight destructors their byte identity, four of them a
 * previous wave's.  So a local copy is what PRESERVES those symbols, not what
 * costs them, and it is deliberate duplication rather than a tidy-up waiting
 * to happen.  Finding F7815, which says not to consolidate them without
 * re-running the SET diff.
 */

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
	new (f) FloatFIR(nTaps, 0, V92PREFILTER_BLOCK);
	fir = f;

	i = (FloatIIR *)sysdep_malloc(sizeof(FloatIIR));
	new (i) FloatIIR(nTaps, 0, V92PREFILTER_BLOCK);
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
