/*
 * V90MP.cpp -- V.90 MP message: construction and destruction.
 *
 * Reconstructed from dsplibs.o V90MP.cpp.  Two of the class's sixteen
 * symbols: the constructor (0x1f410, 40 bytes) and the destructor (0x1f130,
 * one byte -- a bare `ret`).  `include/dsplib/V90MP.h` carries the object map
 * and says which member proved which offset.
 *
 * THE CONSTRUCTOR AND `reset` ARE THE SAME FORTY BYTES, instruction for
 * instruction: the two symbols at 0x1f410 and 0x1f3e0 disassemble alike down
 * to the register allocation.  Both are ordinary GLOBAL symbols in `.text`
 * rather than in a linkonce section, so `reset` is not an in-class inline the
 * compiler folded into the constructor (GCC 3.4 at -O2 does not inline an
 * ordinary global function).  The original repeated the assignments, which is
 * why they are repeated here rather than written as `reset()`.  Finding 1237.
 *
 * The calling convention is plain cdecl -- `mov 0x4(%esp),%eax` -- not
 * thiscall (finding 215).
 */

#include <stddef.h>

#include "dsplib/V90MP.h"

/* Hold the compiler to the map in the header; see V90CP.cpp for why. */
#if __SIZEOF_POINTER__ == 4
#define V90MP_OFF(field, off, tag) \
	typedef char v90mp_off_##tag[ \
	    ((int)__builtin_offsetof(V90MP, field) == (off)) ? 1 : -1]

V90MP_OFF(word_14,		0x014, word14);
V90MP_OFF(byte_19,		0x019, byte19);
V90MP_OFF(byte_1a,		0x01a, byte1a);
V90MP_OFF(byte_1b,		0x01b, byte1b);
V90MP_OFF(bits,			0x01c, bits);
V90MP_OFF(crc,			0x102, crc);
V90MP_OFF(word_114,		0x114, word114);
V90MP_OFF(byte_118,		0x118, byte118);
V90MP_OFF(byte_119,		0x119, byte119);
V90MP_OFF(nofRecievedMp,	0x11c, nofmp);
V90MP_OFF(nofRecievedMpNot,	0x120, nofmpnot);
typedef char v90mp_size[(sizeof(V90MP) == 0x124) ? 1 : -1];
#endif

V90MP::V90MP()
{
	word_14 = 0;
	byte_19 = 0;
	byte_1a = 0;
	byte_1b = 18;

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;
}

/*
 * One byte in the object: `ret`.  The class allocates nothing -- unlike
 * V90CP, whose 173-byte destructor releases six buffers -- so there is
 * nothing for this to do, and the size is the evidence that it does none.
 */
V90MP::~V90MP()
{
}
