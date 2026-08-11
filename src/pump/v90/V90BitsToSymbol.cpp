/*
 * V90BitsToSymbol.cpp -- V90BitsToSymbol's constructor and destructor.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90BitsToSymbol.h` carries
 * the object map and the argument for the 0x24 size; this file is the two
 * functions and the assertions that hold the compiler to that map.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x20(%esp),%ebx` after a 0x1c-byte frame -- not %ecx, so
 * these are not thiscall and nothing here needs an attribute (finding 215).
 *
 * WHY THE MAPPER IS BUILT THROUGH AN asm() LABEL RATHER THAN `new`.  The
 * blob's constructor is
 *
 *     movl $0x704,(%esp) ; call sysdep_malloc ; call V90Mapper::V90Mapper
 *
 * with no null check between the two, and its destructor is
 *
 *     if (p) { V90Mapper::~V90Mapper(p); sysdep_free(p); }
 *
 * -- which is exactly what GCC emits for `new V90Mapper(...)` and `delete p`
 * when `operator new` and `operator delete` are inline wrappers over
 * sysdep_malloc and sysdep_free.  That is almost certainly the original's
 * source.  It is not what this file can write: the build is `-nostdinc++`,
 * there is no <new>, and C++ has no other syntax for running a constructor
 * over storage that already exists.  Declaring a replacement global
 * `operator new` inline is ill-formed, and a user-declared PLACEMENT form
 * makes GCC emit the null test the blob does not have.  So the constructor
 * calls V90Mapper's by its mangled name and the destructor uses the explicit
 * destructor call, which needs no trick at all.  The instruction sequence is
 * the blob's either way; only the spelling differs.  src/pump/v90/
 * V92Precoder.cpp reaches the same conclusion for FloatFIR.
 *
 * THE LOCAL `m` MATTERS.  The blob keeps the fresh pointer in a register
 * across the constructor call and stores it to +0x00 AFTERWARDS; assigning
 * the member first and passing the member would make GCC store, then reload
 * across the call, because it cannot prove the allocation does not alias
 * `this`.  Same reason the mapper's second argument reads `params` back out
 * of +0x04 rather than reusing the incoming register: the blob does, so this
 * does.
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90Mapper.h"

extern "C" {
/*
 * V90Mapper's constructor, by the name the blob calls.  C1 is the
 * complete-object variant, which is what a `new` expression uses and what the
 * relocation at 0x2f766 names.  `sizeof(V90Mapper)` and not the literal 0x704
 * is what the allocation is spelled with, so the two cannot drift apart.
 */
void v90bts_mapper_ctor(void *self, V90Parameters *params)
	asm("_ZN9V90MapperC1EP13V90Parameters");
}

#if __SIZEOF_POINTER__ == 4
#define V90BTS_OFF(field, off, tag) \
	typedef char v90bts_off_##tag[ \
	    ((int)__builtin_offsetof(V90BitsToSymbol, field) == (off)) ? 1 : -1]

V90BTS_OFF(mapper,		0x00, mapper);
V90BTS_OFF(params,		0x04, params);
V90BTS_OFF(symbols,		0x08, symbols);
V90BTS_OFF(nofSymbols,		0x0c, nofsym);
V90BTS_OFF(symbolsDone,		0x10, done);
V90BTS_OFF(bitsPerFrame,	0x14, bpf);
V90BTS_OFF(extraSymbols,	0x18, extra);
V90BTS_OFF(symbolsBlockSize,	0x1c, block);
V90BTS_OFF(extraSymbolsPending,	0x20, pending);
typedef char v90bts_size[(sizeof(V90BitsToSymbol) == 0x24) ? 1 : -1];
#endif

/*
 * ===========================================================================
 * V90BitsToSymbol::V90BitsToSymbol -- .text+0x2f730 (C2) and +0x2f7a0 (C1),
 * 112 bytes each.
 *
 * `bitsPerFrame` at +0x14 and `extraSymbols` at +0x18 are DELIBERATELY not
 * written: the blob leaves both untouched and the first `reset` fills them
 * in.  Neither allocation is null-checked.
 * ===========================================================================
 */
V90BitsToSymbol::V90BitsToSymbol(unsigned int n, V90Parameters *p)
{
	V90Mapper *m;

	params = p;
	m = (V90Mapper *)sysdep_malloc(sizeof(V90Mapper));
	v90bts_mapper_ctor(m, params);
	mapper = m;
	symbols = (short *)sysdep_malloc(2 * n);
	nofSymbols = n;
	symbolsDone = 0;
	symbolsBlockSize = 0;
	extraSymbolsPending = 1;
}

/*
 * ===========================================================================
 * V90BitsToSymbol::~V90BitsToSymbol -- .text+0x2f810 (D2) and +0x2f870 (D1),
 * 84 bytes each.
 *
 * Neither pointer is nulled after being released, so a second destruction
 * double-frees; that is the blob's behaviour and is left alone.
 * ===========================================================================
 */
V90BitsToSymbol::~V90BitsToSymbol()
{
	if (mapper) {
		mapper->~V90Mapper();
		sysdep_free(mapper);
	}
	if (symbols)
		sysdep_free(symbols);
}
