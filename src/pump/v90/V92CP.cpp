/*
 * V92CP.cpp -- V.92 CP message: construction and destruction.
 *
 * Reconstructed from dsplibs.o V92CP.cpp.  Two of the class's twelve
 * symbols: the constructor (0x4e8a0, 61 bytes) and the destructor (0x4e5b0,
 * one byte -- a bare `ret`).  `include/dsplib/V92CP.h` carries the object map
 * and says which member proved which offset.
 *
 * THE CONSTRUCTOR IS `reset` PLUS ONE STORE.  `V92CP::reset` (0x4e860, 57
 * bytes) writes +0x11c = 18, +0x120 = 0, +0x114 = 0, +0x119 = 0, +0x11a = 0
 * and +0x914 = -1; the constructor writes those six and `movb $0x0,0x4(%eax)`
 * as well.  Both are ordinary GLOBAL symbols in `.text` rather than linkonce,
 * so `reset` is not an in-class inline the compiler folded in (GCC 3.4 at -O2
 * does not inline an ordinary global function): the original repeated the
 * assignments.  Finding 1237.
 *
 * The calling convention is plain cdecl -- `mov 0x4(%esp),%eax` -- not
 * thiscall (finding 215).
 */

#include <stddef.h>

#include "dsplib/V92CP.h"

/* Hold the compiler to the map in the header; see V90CP.cpp for why.  This
 * class has no pointer members, so the layout is the same at both widths --
 * the guard is kept for consistency with the sibling classes. */
#if __SIZEOF_POINTER__ == 4
#define V92CP_OFF(field, off, tag) \
	typedef char v92cp_off_##tag[ \
	    ((int)__builtin_offsetof(V92CP, field) == (off)) ? 1 : -1]

V92CP_OFF(byte_04,	0x004, byte04);
V92CP_OFF(word_104,	0x104, word104);
V92CP_OFF(suv,		0x108, suv);
V92CP_OFF(word_114,	0x114, word114);
V92CP_OFF(byte_119,	0x119, byte119);
V92CP_OFF(byte_11a,	0x11a, byte11a);
V92CP_OFF(word_11c,	0x11c, word11c);
V92CP_OFF(word_120,	0x120, word120);
V92CP_OFF(bits,		0x129, bits);
V92CP_OFF(crc,		0x8f9, crc);
V92CP_OFF(word_90c,	0x90c, word90c);
V92CP_OFF(word_914,	0x914, word914);
typedef char v92cp_size[(sizeof(V92CP) == 0x918) ? 1 : -1];
#endif

V92CP::V92CP()
{
	byte_04 = 0;

	word_114 = 0;
	byte_119 = 0;
	byte_11a = 0;
	word_11c = 18;
	word_120 = 0;

	word_914 = -1;
}

/*
 * One byte in the object: `ret`.  The class allocates nothing -- unlike
 * V90CP, whose 173-byte destructor releases six buffers -- so there is
 * nothing for this to do, and the size is the evidence that it does none.
 */
V92CP::~V92CP()
{
}
