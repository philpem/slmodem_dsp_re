/*
 * V90RDetector.cpp -- the constructor and destructor.
 *
 * `include/dsplib/V90RDetector.h` carries the object map, the 44-byte
 * measurement and the evidence for every field width.
 *
 * PLAIN CDECL with `this` as the first STACK argument -- `mov 0x4(%esp),%eax`
 * with no frame -- so nothing here needs a calling-convention attribute
 * (finding 215).
 */

#include <stddef.h>

#include "dsplib/V90RDetector.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own -- and this is the check that catches an object right in size and
 * wrong in its offsets.
 *
 * GUARDED ON THE POINTER WIDTH, because the class holds one at +0x28 and
 * `make check64` compiles this file for the native target, where it is eight
 * bytes and the size moves with it.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define RD_OFF(field, off, tag) \
	typedef char rd_off_##tag[ \
	    ((int)__builtin_offsetof(V90RDetector, field) == (off)) ? 1 : -1]

RD_OFF(int_00,    0x00, int00);
RD_OFF(int_04,    0x04, int04);
RD_OFF(int_08,    0x08, int08);
RD_OFF(int_0c,    0x0c, int0c);
RD_OFF(int_10,    0x10, int10);
RD_OFF(int_14,    0x14, int14);
RD_OFF(int_18,    0x18, int18);
RD_OFF(int_1c,    0x1c, int1c);
RD_OFF(ushort_20, 0x20, ushort20);
RD_OFF(int_24,    0x24, int24);
RD_OFF(params,    0x28, params);
typedef char rd_size[(sizeof(V90RDetector) == 0x2c) ? 1 : -1];
#endif

/*
 * One store.  Everything else in the object keeps whatever was there before,
 * which is why the test seeds both sides and compares the WHOLE object: the
 * claim is not only that +0x28 is written but that the other forty bytes are
 * not.
 */
V90RDetector::V90RDetector(V90Parameters *p)
{
	params = p;
}

/*
 * One byte, `ret`.  See the header: the symbol exists only because the
 * original declared the destructor.
 */
V90RDetector::~V90RDetector()
{
}
