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

#include "dsplib/debug.h"
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

/*
 * reset -- the constructor's forty bytes again, instruction for instruction.
 *
 * 0x1f3e0 and 0x1f410 differ in nothing but their address: the same six
 * stores in the same order, with the same two scratch registers zeroed ahead
 * of the pair of four-byte ones.  Finding 1237 is why the assignments are
 * repeated here rather than written as a call to `resetDetector` plus two
 * counters -- `resetDetector` is a separate GLOBAL symbol at 0x1f3c0 and GCC
 * 3.4 at -O2 does not inline one of those, so an original that called it
 * would have left a call behind.
 */
void
V90MP::reset()
{
	word_14 = 0;
	byte_19 = 0;
	byte_1a = 0;
	byte_1b = 18;

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;
}

/*
 * getBitVector -- hand back the vector and its length.
 *
 * Twenty-one bytes and no branch:
 *
 *     1f708:  0f b6 88 18 01 00 00   movzbl 0x118(%eax),%ecx
 *     1f70f:  83 c0 1c               add    $0x1c,%eax
 *     1f712:  89 0a                  mov    %ecx,(%edx)
 *
 * so the length is the ONE BYTE at +0x118 widened without sign, written
 * whole into the caller's `unsigned int`, and the pointer is `this + 0x1c`
 * -- which is what fixes the bit vector's start.  `movzbl` into a register
 * whose whole 32 bits are then stored is the forced-signedness case
 * CLAUDE.md names: it is why +0x118 is `unsigned char` and not `char`.
 */
unsigned char *
V90MP::getBitVector(unsigned int &length)
{
	length = byte_118;
	return bits;
}

/*
 * printNofRecievedMpMpNot -- the two counters, by the debug string's words.
 *
 * The gate is the object's own: `cmpl $0x1,dsplibs_debug_level; ja`, which
 * is `DSPLIB_DEBUG_ON()`.  This one is NOT an `edprintf` -- the call at
 * 0x20bef relocates against `dsplibs_debug_printf` directly -- so unlike
 * every diagnostic in `V90ConnectionEvaluator` it says nothing at all below
 * the gate, and there is no encoder key to move.
 *
 * The argument order is the object's: +0x11c is the first `%d` and +0x120
 * the second, which is what names the two fields.
 */
void
V90MP::printNofRecievedMpMpNot()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90MP: received %d MP, %d MPNot\r\n",
				     nofRecievedMp, nofRecievedMpNot);
}
