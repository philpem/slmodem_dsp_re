/*
 * V90ModulusEncoder.cpp -- ModulusEncoder constructors and progress.
 * Reconstructed from dsplibs.o; the object map is in ModulusCoder.h.
 */
#include <stddef.h>
#include "dsplib/ModulusCoder.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define MODENC_OFF(field, off, tag) \
	typedef char modenc_off_##tag[ \
	    ((int)__builtin_offsetof(ModulusEncoder, field) == (off)) ? 1 : -1]
MODENC_OFF(field_00, 0x00, field_00);
MODENC_OFF(field_04, 0x04, field_04);
MODENC_OFF(field_08, 0x08, field_08);
MODENC_OFF(field_0c, 0x0c, field_0c);
MODENC_OFF(field_10, 0x10, field_10);
MODENC_OFF(field_14, 0x14, field_14);
MODENC_OFF(field_18, 0x18, field_18);
typedef char modenc_size[(sizeof(ModulusEncoder) == 0x1c) ? 1 : -1];
#endif

/*
 * SEVEN SCALARS ZEROED IN DESCENDING ADDRESS ORDER.  A full member-initialiser
 * list emits ascending stores under GCC 3.4.2, regardless of its written
 * order.  The chained assignment below emits the blob's seven descending
 * stores and makes all four default-constructor clones byte-exact (F10219).
 */
ModulusEncoder::ModulusEncoder()
{
	field_00 = field_04 = field_08 = field_0c = field_10 = field_14 =
		field_18 = 0;
}

/*
 * The seven-argument form, 0x32390/0x323d0 (C2/C1), 53 bytes each: seven
 * `unsigned int` parameters stored to +0x00..+0x18 in argument order with no
 * arithmetic anywhere -- a constructor whose whole job is its initialiser
 * list, which is how it is spelled.
 */
ModulusEncoder::ModulusEncoder(unsigned int a, unsigned int b, unsigned int c,
			       unsigned int d, unsigned int e, unsigned int f,
			       unsigned int g)
	: field_00(a), field_04(b), field_08(c), field_0c(d), field_10(e),
	  field_14(f), field_18(g)
{
}

/*
 * MODULUS CONVERSION over a SIGNED 64-BIT accumulator.  Five moduli produce
 * six digits; `field_14` is read by neither progress member.  The signed
 * division helpers and arithmetic shifts are the object's measured behavior.
 */
void
ModulusEncoder::progress(unsigned char *bytes, unsigned int *out)
{
	long long acc = 0;
	int i;
	/* The loop reads the bit string most-significant first from the top. */
	for (i = (int)field_18 - 1; i >= 0; i--)
		acc = (acc + acc) | (bytes[i] & 1);
	out[0] = (unsigned int)(acc % (long long)field_00);
	acc = (acc - out[0]) / (long long)field_00;
	out[1] = (unsigned int)(acc % (long long)field_04);
	acc = (acc - out[1]) / (long long)field_04;
	out[2] = (unsigned int)(acc % (long long)field_08);
	acc = (acc - out[2]) / (long long)field_08;
	out[3] = (unsigned int)(acc % (long long)field_0c);
	acc = (acc - out[3]) / (long long)field_0c;
	out[4] = (unsigned int)(acc % (long long)field_10);
	acc = (acc - out[4]) / (long long)field_10;
	out[5] = (unsigned int)acc;
}
