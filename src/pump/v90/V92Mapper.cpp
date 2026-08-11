/*
 * V92Mapper.cpp -- the V.92 symbol mapper's constructor and destructor, both
 * of which are empty.
 *
 * Reconstructed from dsplibs.o.  Two of the class's four members;
 * `include/dsplib/V92Mapper.h` carries the object map and the 0x2c that
 * `V92Phase4Modulator::V92Phase4Modulator` measures.
 *
 * As with V92ConvolutionEncoder, the claim is that the original DECLARED
 * both: GCC emits an out-of-line constructor or destructor only for a
 * user-declared one, and the blob has C1, C2, D1 and D2 at one byte each.
 * `reset(short, unsigned char)` is what puts the object into a usable state,
 * so there was nothing for the constructor to do.
 */

#include <stddef.h>

#include "dsplib/V92Mapper.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92MAP_OFF(field, off, tag) \
	typedef char v92map_off_##tag[ \
	    ((int)__builtin_offsetof(V92Mapper, field) == (off)) ? 1 : -1]

V92MAP_OFF(scale,  0x00, scale);
V92MAP_OFF(mode,   0x02, mode);
V92MAP_OFF(pad_03, 0x03, pad_03);
V92MAP_OFF(bits,   0x26, bits);
V92MAP_OFF(power,  0x28, power);
typedef char v92map_size[(sizeof(V92Mapper) == 0x2c) ? 1 : -1];
#endif

V92Mapper::V92Mapper()
{
}

V92Mapper::~V92Mapper()
{
}
