/*
 * V90CP.cpp -- V.90 CP message: construction and destruction.
 *
 * Reconstructed from dsplibs.o V90CP.cpp.  Two of the class's fifteen
 * symbols: the constructor (0x51590, 193 bytes) and the destructor (0x51200,
 * 173 bytes).  `include/dsplib/V90CP.h` carries the object map and says which
 * member proved which offset.
 *
 * THE DESTRUCTOR IS THE INTERESTING ONE.  173 bytes against a 193-byte
 * constructor is not an empty class with a fat header: the constructor makes
 * six `sysdep_malloc(0x200)` calls and stores the results at +0xc88..+0xc9c,
 * and the destructor is six null-guarded `sysdep_free`s of exactly those six,
 * in the same order.  It does not clear them afterwards -- the object is left
 * holding six dangling pointers, which is visible in the differential test
 * because both sides leave them dangling identically.
 *
 * The guard is a real branch and not the compiler being careful: each pointer
 * is loaded, tested, and jumped over when null, so a reconstruction that
 * freed unconditionally would differ only in the null case.  The harness's
 * allocator counts `sysdep_free(NULL)` separately from a wild free, so
 * t_v90cp can see that difference without crashing.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x10(%esp),%ebx` after a push and an 8-byte frame -- not
 * %ecx, so nothing here needs an attribute (finding 215).
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here.
 * No virtuals, no static data members, no new/delete: the allocation goes
 * through the object's own `sysdep_malloc`, so the test binaries still link
 * with $(CC).
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90CP.h"

/*
 * Hold the compiler to the map in the header.  tools/offcheck.py does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class has to assert its own -- and it is exactly the check that
 * catches an object right in size and wrong by four in every offset.
 *
 * Guarded on the pointer width because `buf[6]` is 24 bytes in the 32-bit
 * build the blob is and 48 in the 64-bit syntax check `make check64` runs;
 * every offset past +0xc88 moves with it and the class is 24 bytes longer.
 */
#if __SIZEOF_POINTER__ == 4
#define V90CP_OFF(field, off, tag) \
	typedef char v90cp_off_##tag[ \
	    ((int)__builtin_offsetof(V90CP, field) == (off)) ? 1 : -1]

V90CP_OFF(byte_13,		0x0013, byte13);
V90CP_OFF(buf,			0x0c88, buf);
V90CP_OFF(word_ca4,		0x0ca4, wordca4);
V90CP_OFF(byte_ca9,		0x0ca9, byteca9);
V90CP_OFF(byte_caa,		0x0caa, bytecaa);
V90CP_OFF(word_cac,		0x0cac, wordcac);
V90CP_OFF(word_cb0,		0x0cb0, wordcb0);
V90CP_OFF(bits,			0x0cb8, bits);
V90CP_OFF(crc,			0x3b98, crc);
V90CP_OFF(word_3ba8,		0x3ba8, word3ba8);
V90CP_OFF(word_3bac,		0x3bac, word3bac);
V90CP_OFF(word_3bb0,		0x3bb0, word3bb0);
V90CP_OFF(nofRecievedMp,	0x3bb4, nofmp);
V90CP_OFF(nofRecievedMpNot,	0x3bb8, nofmpnot);
V90CP_OFF(word_3bbc,		0x3bbc, word3bbc);
typedef char v90cp_size[(sizeof(V90CP) == 0x3bc0) ? 1 : -1];
#endif

/*
 * Six buffers, then the detector's five fields, the two frame counters, the
 * byte at +0x13 that only this member ever clears, and -1 at +0x3bbc.
 *
 * The five detector stores are `resetDetector`'s whole body and the eight
 * after the allocations are `reset`'s, byte for byte -- but `reset` and
 * `resetDetector` are ordinary GLOBAL functions in `.text`, not linkonce, so
 * neither was an in-class inline the compiler folded in here (GCC 3.4 at -O2
 * does not inline an ordinary global function).  The original repeated the
 * assignments; finding 1237.
 */
V90CP::V90CP()
{
	buf[0] = sysdep_malloc(V90CP_BUFSIZE);
	buf[1] = sysdep_malloc(V90CP_BUFSIZE);
	buf[2] = sysdep_malloc(V90CP_BUFSIZE);
	buf[3] = sysdep_malloc(V90CP_BUFSIZE);
	buf[4] = sysdep_malloc(V90CP_BUFSIZE);
	buf[5] = sysdep_malloc(V90CP_BUFSIZE);

	word_ca4 = 0;
	byte_ca9 = 0;
	byte_caa = 0;
	word_cac = 18;
	word_cb0 = 0;

	nofRecievedMp = 0;
	nofRecievedMpNot = 0;

	byte_13 = 0;
	word_3bbc = -1;
}

/*
 * And back, in the same order.  Nothing else: the destructor touches no field
 * but the six pointers, and only reads them.
 */
V90CP::~V90CP()
{
	if (buf[0] != 0)
		sysdep_free(buf[0]);
	if (buf[1] != 0)
		sysdep_free(buf[1]);
	if (buf[2] != 0)
		sysdep_free(buf[2]);
	if (buf[3] != 0)
		sysdep_free(buf[3]);
	if (buf[4] != 0)
		sysdep_free(buf[4]);
	if (buf[5] != 0)
		sysdep_free(buf[5]);
}
