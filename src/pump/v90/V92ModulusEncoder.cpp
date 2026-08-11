/*
 * V92ModulusEncoder.cpp -- the V.92 modulus encoder's constructor.
 *
 * Reconstructed from dsplibs.o.  One of the class's three members;
 * `include/dsplib/V92ModulusEncoder.h` carries the object map, the 0x54 the
 * caller's `sysdep_malloc` measures, and why the initialised range starts at
 * +0x18 rather than at zero.
 *
 * NO DESTRUCTOR is declared, because the blob has none.  The constructor
 * initialises thirteen members and no others; the six words below them and
 * the two above are `reset`'s to fill, and the test asserts they keep
 * whatever the storage held.
 */

#include <stddef.h>

#include "dsplib/V92ModulusEncoder.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92ME_OFF(field, off, tag) \
	typedef char v92me_off_##tag[ \
	    ((int)__builtin_offsetof(V92ModulusEncoder, field) == (off)) \
	    ? 1 : -1]

V92ME_OFF(head,     0x00, head);
V92ME_OFF(field_18, 0x18, field_18);
V92ME_OFF(field_1c, 0x1c, field_1c);
V92ME_OFF(field_20, 0x20, field_20);
V92ME_OFF(field_24, 0x24, field_24);
V92ME_OFF(field_28, 0x28, field_28);
V92ME_OFF(field_2c, 0x2c, field_2c);
V92ME_OFF(field_30, 0x30, field_30);
V92ME_OFF(field_34, 0x34, field_34);
V92ME_OFF(field_38, 0x38, field_38);
V92ME_OFF(field_3c, 0x3c, field_3c);
V92ME_OFF(field_40, 0x40, field_40);
V92ME_OFF(field_44, 0x44, field_44);
V92ME_OFF(field_48, 0x48, field_48);
V92ME_OFF(field_4c, 0x4c, field_4c);
V92ME_OFF(field_50, 0x50, field_50);
typedef char v92me_size[(sizeof(V92ModulusEncoder) == 0x54) ? 1 : -1];
#endif

/*
 * Thirteen scalars, one `movl $0x0` each in the object -- so a
 * member-initialiser list and not a loop, and not `memset(this, 0, ...)`,
 * which would also have cleared +0x00..+0x14 and +0x4c, +0x50.
 */
V92ModulusEncoder::V92ModulusEncoder()
	: field_18(0), field_1c(0), field_20(0), field_24(0), field_28(0),
	  field_2c(0), field_30(0), field_34(0), field_38(0), field_3c(0),
	  field_40(0), field_44(0), field_48(0)
{
}
