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
MODDEC_OFF(field_00, 0x00, field_00);
MODDEC_OFF(field_18, 0x18, field_18);
typedef char moddec_size[(sizeof(ModulusDecoder) == 0x1c) ? 1 : -1];
#endif

/* The decoder default constructor is byte-for-byte the encoder's. */
ModulusDecoder::ModulusDecoder()
{
	field_00 = field_04 = field_08 = field_0c = field_10 = field_14 =
		field_18 = 0;
}

/*
 * Byte for byte the encoder's seven-argument form again -- 0x32070/0x320b0,
 * 53 bytes each, differing from 0x32390/0x323d0 in nothing but the names.
 */
ModulusDecoder::ModulusDecoder(unsigned int a, unsigned int b, unsigned int c,
			       unsigned int d, unsigned int e, unsigned int f,
			       unsigned int g)
	: field_00(a), field_04(b), field_08(c), field_0c(d), field_10(e),
	  field_14(f), field_18(g)
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
	acc = (long long)in[5] * field_10 + in[4];
	acc = acc * field_0c + in[3];
	acc = acc * field_08 + in[2];
	acc = acc * field_04 + in[1];
	acc = acc * field_00 + in[0];
	for (i = 0; i < field_18; i++) {
		bytes[i] = (unsigned char)(acc & 1);
		acc >>= 1;
	}
}
