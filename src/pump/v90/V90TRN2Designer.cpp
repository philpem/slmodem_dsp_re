/*
 * V90TRN2Designer.cpp -- the constructor and destructor.
 *
 * `include/dsplib/V90TRN2Designer.h` carries the object map, the evidence for
 * the two pointer types and the scope of the size bound.
 *
 * PLAIN CDECL with `this` as the first STACK argument -- `mov 0x4(%esp),%eax`
 * with no frame at all -- so nothing here needs a calling-convention
 * attribute (finding 215).
 */

#include <stddef.h>

#include "dsplib/V90TRN2Designer.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own.
 *
 * GUARDED ON THE POINTER WIDTH, because both members are pointers and
 * `make check64` compiles this file for the native target, where they are
 * eight bytes and +0x04 moves.  The claim is about the 32-bit layout the blob
 * has, so it is asserted only where the compiler lays that layout out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define TRN2_OFF(field, off, tag) \
	typedef char trn2_off_##tag[ \
	    ((int)__builtin_offsetof(V90TRN2Designer, field) == (off)) ? 1 : -1]

TRN2_OFF(params, 0x00, params);
TRN2_OFF(power,  0x04, power);
typedef char trn2_size[(sizeof(V90TRN2Designer) == 0x08) ? 1 : -1];
#endif

/*
 * Two stores, in the object's order.  The argument names are the mangling's
 * types spelled out; nothing here reads either pointer.
 */
V90TRN2Designer::V90TRN2Designer(V90Parameters *p, V90ConstellationPower *cp)
{
	params = p;
	power = cp;
}

/*
 * One byte, `ret`.  See the header: the symbol exists only because the
 * original declared the destructor, so declaring and emptying it here is the
 * reconstruction and not a placeholder.
 */
V90TRN2Designer::~V90TRN2Designer()
{
}
