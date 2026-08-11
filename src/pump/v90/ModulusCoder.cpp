/*
 * ModulusCoder.cpp -- ModulusEncoder and ModulusDecoder: the default
 * constructor of each.
 *
 * Reconstructed from dsplibs.o.  Two of the six members these two classes
 * have between them; `include/dsplib/ModulusCoder.h` carries the object map
 * and the two independent bounds that make it 0x1c bytes.
 *
 * NEITHER CLASS HAS A DESTRUCTOR IN THE BLOB, so neither declares one -- a
 * user-declared destructor would emit an out-of-line symbol the original does
 * not have, and `V90Demapper::~V90Demapper` running no destructor over its
 * embedded decoder is the corroboration.
 *
 * SEVEN MEMBERS AND NOT AN ARRAY.  The blob zeroes them with seven separate
 * `movl $0x0` in descending address order, which is a member-initialiser list
 * over seven scalars; a `for` loop or a `memset` over an array is neither
 * that shape nor that length.  The member-initialiser list below is written
 * ascending and GCC is free to emit it either way -- what it may not do is
 * turn it into a loop.
 */

#include <stddef.h>

#include "dsplib/ModulusCoder.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define MODENC_OFF(field, off, tag) \
	typedef char modenc_off_##tag[ \
	    ((int)__builtin_offsetof(ModulusEncoder, field) == (off)) ? 1 : -1]
#define MODDEC_OFF(field, off, tag) \
	typedef char moddec_off_##tag[ \
	    ((int)__builtin_offsetof(ModulusDecoder, field) == (off)) ? 1 : -1]

MODENC_OFF(field_00, 0x00, field_00);
MODENC_OFF(field_04, 0x04, field_04);
MODENC_OFF(field_08, 0x08, field_08);
MODENC_OFF(field_0c, 0x0c, field_0c);
MODENC_OFF(field_10, 0x10, field_10);
MODENC_OFF(field_14, 0x14, field_14);
MODENC_OFF(field_18, 0x18, field_18);
typedef char modenc_size[(sizeof(ModulusEncoder) == 0x1c) ? 1 : -1];

MODDEC_OFF(field_00, 0x00, field_00);
MODDEC_OFF(field_18, 0x18, field_18);
typedef char moddec_size[(sizeof(ModulusDecoder) == 0x1c) ? 1 : -1];
#endif

ModulusEncoder::ModulusEncoder()
	: field_00(0), field_04(0), field_08(0), field_0c(0), field_10(0),
	  field_14(0), field_18(0)
{
}

/*
 * Byte for byte the encoder's, which is the object's own claim: the two
 * functions are 53 bytes each and differ in nothing but their symbol names.
 */
ModulusDecoder::ModulusDecoder()
	: field_00(0), field_04(0), field_08(0), field_0c(0), field_10(0),
	  field_14(0), field_18(0)
{
}
