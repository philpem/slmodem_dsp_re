/*
 * V90ModulusDecoder.cpp -- ModulusDecoder constructors and progress.
 * Reconstructed from dsplibs.o; the object map is in ModulusCoder.h.
 */
#include <stddef.h>
#include "dsplib/ModulusCoder.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define MODDEC_OFF(field, off, tag) \
	typedef char moddec_off_##tag[ \
	    ((int)__builtin_offsetof(ModulusDecoder, field) == (off)) ? 1 : -1]
MODDEC_OFF(constellationSize0, 0x00, field_00);
MODDEC_OFF(bitCount, 0x18, field_18);
typedef char moddec_size[(sizeof(ModulusDecoder) == 0x1c) ? 1 : -1];
#endif

/* The decoder default constructor is byte-for-byte the encoder's. */
ModulusDecoder::ModulusDecoder()
{
	constellationSize0 = constellationSize1 = constellationSize2 = constellationSize3 = constellationSize4 = constellationSize5 =
		bitCount = 0;
}

/*
 * Byte for byte the encoder's seven-argument form again -- 0x32070/0x320b0,
 * 53 bytes each, differing from 0x32390/0x323d0 in nothing but the names.
 */
ModulusDecoder::ModulusDecoder(unsigned int a, unsigned int b, unsigned int c,
			       unsigned int d, unsigned int e, unsigned int f,
			       unsigned int g)
	: constellationSize0(a), constellationSize1(b), constellationSize2(c), constellationSize3(d), constellationSize4(e),
	  constellationSize5(f), bitCount(g)
{
}

/*
 * The encoder run backwards: Horner from the top digit down, then the bits
 * back out from the bottom.  The loop bound is re-read from the object and
 * the shift is arithmetic, matching the object.
 */
void
ModulusDecoder::progress(unsigned char *bytes, unsigned int *in)
{
	long long acc;
	unsigned int i;
	acc = (long long)in[5] * constellationSize4 + in[4];
	acc = acc * constellationSize3 + in[3];
	acc = acc * constellationSize2 + in[2];
	acc = acc * constellationSize1 + in[1];
	acc = acc * constellationSize0 + in[0];
	for (i = 0; i < bitCount; i++) {
		bytes[i] = (unsigned char)(acc & 1);
		acc >>= 1;
	}
}
