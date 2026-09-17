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
MODENC_OFF(constellationSize0, 0x00, field_00);
MODENC_OFF(constellationSize1, 0x04, field_04);
MODENC_OFF(constellationSize2, 0x08, field_08);
MODENC_OFF(constellationSize3, 0x0c, field_0c);
MODENC_OFF(constellationSize4, 0x10, field_10);
MODENC_OFF(constellationSize5, 0x14, field_14);
MODENC_OFF(bitCount, 0x18, field_18);
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
	constellationSize0 = constellationSize1 = constellationSize2 = constellationSize3 = constellationSize4 = constellationSize5 =
		bitCount = 0;
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
	: constellationSize0(a), constellationSize1(b), constellationSize2(c), constellationSize3(d), constellationSize4(e),
	  constellationSize5(f), bitCount(g)
{
}

/*
 * MODULUS CONVERSION over a SIGNED 64-BIT accumulator.  Five moduli produce
 * six digits; `constellationSize5` is read by neither progress member.  The signed
 * division helpers and arithmetic shifts are the object's measured behavior.
 */
void
ModulusEncoder::progress(unsigned char *bytes, unsigned int *out)
{
	long long acc = 0;
	int i;
	/* The loop reads the bit string most-significant first from the top. */
	for (i = (int)bitCount - 1; i >= 0; i--)
		acc = (acc + acc) | (bytes[i] & 1);
	out[0] = (unsigned int)(acc % (long long)constellationSize0);
	acc = (acc - out[0]) / (long long)constellationSize0;
	out[1] = (unsigned int)(acc % (long long)constellationSize1);
	acc = (acc - out[1]) / (long long)constellationSize1;
	out[2] = (unsigned int)(acc % (long long)constellationSize2);
	acc = (acc - out[2]) / (long long)constellationSize2;
	out[3] = (unsigned int)(acc % (long long)constellationSize3);
	acc = (acc - out[3]) / (long long)constellationSize3;
	out[4] = (unsigned int)(acc % (long long)constellationSize4);
	acc = (acc - out[4]) / (long long)constellationSize4;
	out[5] = (unsigned int)acc;
}
