/*
 * V90SpectralShaper.cpp -- building and tearing down the spectral shaper.
 *
 * Reconstructed from dsplibs.o.  Two of the class's eight members: the
 * constructor and the destructor.  `include/dsplib/V90SpectralShaper.h`
 * carries the object map and the evidence for the 108-byte bound.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THE ORDER OF THE THREE SUBOBJECT ACTIONS IS THE ABI'S, NOT THIS FILE'S.
 * `movb $0x0,0x38(%ebx)` comes before the call to the encoder's constructor,
 * and a constructor BODY cannot run before a member subobject is built -- so
 * that store is a member-initialiser and is written as one.  The encoder at
 * +0x3c is built before the filter at +0x48 because that is declaration
 * order, and the destructor destroys only the encoder because the filter has
 * no destructor to call.
 *
 * THE ALLOCATION SIZE IS NOT COMPUTED FROM +0x34.  Both allocations are
 * `movl $0x30` and the store of 24 into +0x34 comes after them, so the source
 * cannot have read the field back; 24 * sizeof(unsigned short) is written
 * here because `advanceTrellis` indexes both buffers with a stride of two
 * (`movzwl (%reg,%edx,2)`), which makes 0x30 bytes 24 entries.
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90SpectralShaper.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SS_OFF(field, off, tag) \
	typedef char v90ss_off_##tag[ \
	    ((int)__builtin_offsetof(V90SpectralShaper, field) == (off)) \
	    ? 1 : -1]

V90SS_OFF(word_20,  0x20, word20);
V90SS_OFF(buf_28,   0x28, buf28);
V90SS_OFF(buf_2c,   0x2c, buf2c);
V90SS_OFF(word_30,  0x30, word30);
V90SS_OFF(word_34,  0x34, word34);
V90SS_OFF(byte_38,  0x38, byte38);
V90SS_OFF(pde,      0x3c, pde);
V90SS_OFF(ssf,      0x48, ssf);
typedef char v90ss_size[(sizeof(V90SpectralShaper) == 0x6c) ? 1 : -1];
#endif

V90SpectralShaper::V90SpectralShaper()
	: byte_38(0), pde(6)
{
	buf_28 = (unsigned short *)
	    sysdep_malloc(24 * sizeof(unsigned short));
	buf_2c = (unsigned short *)
	    sysdep_malloc(24 * sizeof(unsigned short));

	word_34 = 24;
	word_30 = 0;
	word_20 = 0;
}

/*
 * Both pointers are tested and neither is nulled; the encoder's destructor is
 * the implicit member call and is not written out.
 */
V90SpectralShaper::~V90SpectralShaper()
{
	if (buf_28 != 0)
		sysdep_free(buf_28);

	if (buf_2c != 0)
		sysdep_free(buf_2c);
}
