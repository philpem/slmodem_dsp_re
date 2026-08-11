/*
 * V92Precoder.cpp -- V.92 transmit precoder: construction and destruction.
 *
 * Reconstructed from dsplibs.o.  Two of the class's six members;
 * `include/dsplib/V92Precoder.h` carries the object map and the evidence for
 * it.  The other four are declared there and deliberately left undefined.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL, with `this` as the first stack
 * argument (`mov 0x20(%esp),%edi` after a 0x1c-byte frame), so nothing here
 * needs an attribute -- finding 215.
 *
 * WHY THE SUB-OBJECTS ARE BUILT THROUGH AN asm() LABEL RATHER THAN `new`.
 * The blob's constructor is
 *
 *     movl $0x14,(%esp) ; call sysdep_malloc ; call FloatFIR::FloatFIR
 *
 * with no null check between the two, and its destructor is
 *
 *     if (p) { FloatFIR::~FloatFIR(p); sysdep_free(p); }
 *
 * -- which is exactly what GCC emits for `new FloatFIR(...)` and `delete p`
 * when `operator new` and `operator delete` are inline wrappers over
 * sysdep_malloc and sysdep_free.  That is almost certainly the original's
 * source.  It is not what this file can write: the build is `-nostdinc++`,
 * there is no <new>, and C++ has no other syntax for running a constructor
 * over storage that already exists.  Declaring a replacement global
 * `operator new` inline is ill-formed, and a user-declared PLACEMENT form
 * makes GCC emit the null test the blob does not have.  So the constructor
 * calls FloatFIR's by its mangled name and the destructor uses the explicit
 * destructor call, which needs no header.  The instruction sequence is the
 * blob's either way; only the spelling differs.
 */

#include <stddef.h>

#include "dsplib/V92Precoder.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

/*
 * FloatFIR's constructor, by the name the blob calls.  C1 is the
 * complete-object variant, which is what a `new` expression uses and what the
 * relocation at 0x56bdb names.  `sizeof(FloatFIR)` and not the literal 20
 * that appears in the object: this file is also compiled 64-bit for the
 * interop binaries, where 20 is wrong.
 */
void v92precoder_floatfir_ctor(void *self, unsigned int nTaps, float *coef,
			       unsigned int blockSize)
	asm("_ZN8FloatFIRC1EjPfj");
}

/*
 * Hold the compiler to the map in the header.  Guarded because the object is
 * a 32-bit one and every pointer field moves in the 64-bit build.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92PRE_OFF(field, off, tag) \
	typedef char v92pre_off_##tag[ \
	    ((int)__builtin_offsetof(V92Precoder, field) == (off)) ? 1 : -1]

V92PRE_OFF(word_00,     0x00, word_00);
V92PRE_OFF(paramsAt9c,  0x04, paramsat9c);
V92PRE_OFF(head,        0x08, head);
V92PRE_OFF(tableA,      0x20, tablea);
V92PRE_OFF(tableB,      0x50, tableb);
V92PRE_OFF(fir1,        0x68, fir1);
V92PRE_OFF(fir2,        0x6c, fir2);
V92PRE_OFF(state0,      0x70, state0);
V92PRE_OFF(state1,      0x74, state1);
V92PRE_OFF(taps1,       0x78, taps1);
V92PRE_OFF(taps2,       0x7c, taps2);
typedef char v92pre_size[(sizeof(V92Precoder) == 0x80) ? 1 : -1];
typedef char v92pre_fir_size[(sizeof(FloatFIR) == 0x14) ? 1 : -1];
#endif

/*
 * Two identical filters, both `FloatFIR(nTaps, 0, 99)`, and nothing else --
 * the other thirty fields of the object are left as they were found and
 * `reset(V92MappingParams *)` is what fills them.
 *
 * The allocation is not checked.  Neither is the blob's; see
 * docs/deviations.md.
 */
V92Precoder::V92Precoder(unsigned int nTaps)
{
	FloatFIR *p;

	p = (FloatFIR *)sysdep_malloc(sizeof(FloatFIR));
	v92precoder_floatfir_ctor(p, nTaps, 0, V92PRECODER_BLOCK);
	fir1 = p;

	p = (FloatFIR *)sysdep_malloc(sizeof(FloatFIR));
	v92precoder_floatfir_ctor(p, nTaps, 0, V92PRECODER_BLOCK);
	fir2 = p;
}

/*
 * Both filters, in the order they were allocated.  The null tests are not
 * decoration and they are not free to omit: this tree's `sysdep_free`
 * tolerates NULL, so dropping them leaves every byte of every object
 * unchanged and moves only `harness_alloc.free_null` -- which is why
 * t_v92precoder.cpp asserts that counter on the trials that null a pointer by
 * hand.
 *
 * Nothing is nulled after the free.  The object is left holding two dangling
 * addresses, exactly as the blob leaves it; see docs/deviations.md.
 */
V92Precoder::~V92Precoder()
{
	if (fir1 != 0) {
		fir1->~FloatFIR();
		sysdep_free(fir1);
	}
	if (fir2 != 0) {
		fir2->~FloatFIR();
		sysdep_free(fir2);
	}
}
