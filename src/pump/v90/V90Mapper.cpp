/*
 * V90Mapper.cpp -- V90Mapper's constructor and destructor.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Mapper.h` carries the
 * object map and the argument for the 0x704 size; this file is the two
 * functions and the assertions that hold the compiler to that map.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x10(%esp),%ebx` after one push and a 8-byte frame -- not
 * %ecx, so these are not thiscall and nothing here needs an attribute
 * (finding 215).
 *
 * THE TWO SUBOBJECTS ARE BUILT BY THE MEM-INITIALIZER LIST AND NOT BY HAND.
 * `ModulusEncoder` and `V90SpectralShaper` both have a user-declared default
 * constructor, so declaring them as members is what emits
 *
 *     lea 0x670(%ebx),%edx ; call ModulusEncoder::C1
 *     lea 0x68c(%ebx),%eax ; call V90SpectralShaper::C1
 *
 * in declaration order, before the body, which is the blob's order.  The
 * destructor's single `V90SpectralShaper::~V90SpectralShaper` call is emitted
 * the same way, after the body, because `ModulusEncoder` has no destructor.
 *
 * THE SIX-WORD CLEAR AT +0x658 IS A LOOP IN THE BLOB, NOT SIX STORES.  It is
 * `cmp $0x5,%eax ; jbe` over an unsigned counter incremented after the store,
 * so it runs for 0..5 inclusive and covers 0x658..0x66f, which is exactly the
 * gap up to the modulus encoder.  Written as six unrolled stores the
 * behaviour is identical and the code is not, so it is written as the loop.
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90Mapper.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class has to assert its own -- and it is exactly the check that
 * catches an object right in size and wrong by four in every offset.
 */
#if __SIZEOF_POINTER__ == 4
#define V90MAPPER_OFF(field, off, tag) \
	typedef char v90mapper_off_##tag[ \
	    ((int)__builtin_offsetof(V90Mapper, field) == (off)) ? 1 : -1]

V90MAPPER_OFF(params,		0x000, params);
V90MAPPER_OFF(cleared_004,	0x004, c004);
V90MAPPER_OFF(cleared_014,	0x014, c014);
V90MAPPER_OFF(buf,		0x018, buf);
V90MAPPER_OFF(cleared_01c,	0x01c, c01c);
V90MAPPER_OFF(cleared_658,	0x658, c658);
V90MAPPER_OFF(modulusEncoder,	0x670, modenc);
V90MAPPER_OFF(spectralShaper,	0x68c, shaper);
V90MAPPER_OFF(cleared_6f8,	0x6f8, c6f8);
V90MAPPER_OFF(cleared_6fc,	0x6fc, c6fc);
typedef char v90mapper_size[(sizeof(V90Mapper) == 0x704) ? 1 : -1];
#endif

/*
 * ===========================================================================
 * V90Mapper::V90Mapper -- .text+0x2ff50 (C1) and +0x2ffe0 (C2), 138 bytes.
 *
 * The 0x50-byte allocation is NOT null-checked, exactly as in the blob: the
 * pointer is stored and the first user of it stores through whatever came
 * back.  Every other store is a constant.
 * ===========================================================================
 */
V90Mapper::V90Mapper(V90Parameters *p)
{
	unsigned int i;

	cleared_6fc = 0;
	buf = sysdep_malloc(0x50);
	cleared_01c = 0;
	cleared_6f8 = 0;
	cleared_014 = 0;
	cleared_010 = 0;
	cleared_00c = 0;
	cleared_008 = 0;
	cleared_004 = 0;
	for (i = 0; i <= 5; i++)
		cleared_658[i] = 0;
	params = p;
}

/*
 * ===========================================================================
 * V90Mapper::~V90Mapper -- .text+0x2fed0 (D2) and +0x2ff10 (D1), 61 bytes.
 *
 * `buf` is NOT nulled after the free, so a second destruction double-frees;
 * that is the blob's behaviour and is left alone.  The spectral shaper's
 * destructor runs after this body whichever way the branch went, which is
 * why the blob has two copies of the call.
 * ===========================================================================
 */
V90Mapper::~V90Mapper()
{
	if (buf)
		sysdep_free(buf);
}
