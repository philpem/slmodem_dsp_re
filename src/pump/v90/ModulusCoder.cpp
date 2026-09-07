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
 * SEVEN SCALARS ZEROED IN DESCENDING ADDRESS ORDER.  A full member-initialiser
 * list emits ascending stores under GCC 3.4.2, regardless of its written
 * order.  The chained assignment below emits the blob's seven descending
 * stores and makes all four default-constructor clones byte-exact (F10219).
 * Descending separate assignments also match, so the object establishes the
 * store order without uniquely identifying the author's source spelling.
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
{
	field_00 = field_04 = field_08 = field_0c = field_10 = field_14 =
		field_18 = 0;
}

/*
 * The seven-argument form, 0x32390/0x323d0 (C2/C1), 53 bytes each: seven
 * `unsigned int` parameters stored to +0x00..+0x18 in argument order with no
 * arithmetic anywhere -- a constructor whose whole job is its initialiser
 * list, which is how it is spelled.  The parameter names are the offsets
 * because the object gives no meanings (ModulusCoder.h).
 */
ModulusEncoder::ModulusEncoder(unsigned int a, unsigned int b, unsigned int c,
			       unsigned int d, unsigned int e, unsigned int f,
			       unsigned int g)
	: field_00(a), field_04(b), field_08(c), field_0c(d), field_10(e),
	  field_14(f), field_18(g)
{
}

/*
 * Byte for byte the encoder's, which is the object's own claim: the two
 * functions are 53 bytes each and differ in nothing but their symbol names.
 */
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
 * MODULUS CONVERSION, and the two `progress` members are each other's inverse
 * over a SIGNED 64-BIT accumulator.  What the coder does is read a bit string
 * as one integer and write it out in a MIXED-RADIX representation -- digit k
 * to base `field_00`, `field_04`, `field_08`, `field_0c`, `field_10` -- with
 * the sixth output word carrying whatever is left.  Five moduli, six digits,
 * `field_18` bits.
 *
 * THE ACCUMULATOR IS SIGNED, and that is a measurement rather than a
 * convention.  The encoder's divisions go through `__divdi3` and `__moddi3`,
 * which are the SIGNED helpers -- the unsigned ones are `__udivdi3` and
 * `__umoddi3` and appear nowhere in either function -- and the decoder's shift
 * is `shrd`/`sar`, an arithmetic right shift.  With `field_18` at or above 64
 * the top bit becomes the sign and the two spellings part company; nothing in
 * the object stops that happening, so the type is reproduced rather than
 * tidied.
 *
 * `field_14` IS READ BY NEITHER.  Five moduli are used and six words are
 * written, so the seventh member is the constructor's alone.
 *
 * THE DIVISORS ARE ZERO-EXTENDED, not sign-extended: each is pushed as
 * `{field, 0}`.  So a modulus above 2^31 is a large positive divisor and not
 * a negative one, which `(long long)(unsigned int)` says in C and a plain
 * `(long long)field` would also say, the members being `unsigned int`.
 */
void
ModulusEncoder::progress(unsigned char *bytes, unsigned int *out)
{
	long long acc = 0;
	int i;

	/*
	 * The bit string, MOST SIGNIFICANT FIRST from the top of the array:
	 * the loop counts down from `field_18 - 1` and shifts each byte's low
	 * bit in at the bottom, so `bytes[k]` ends up at bit k.  The counter
	 * is signed and the test is `jns`, so a length of zero leaves the
	 * accumulator at nothing rather than running 2^32 times.
	 */
	for (i = (int)field_18 - 1; i >= 0; i--)
		acc = (acc + acc) | (bytes[i] & 1);

	/*
	 * Five digits, each a remainder, and the quotient carried forward.
	 * The object SUBTRACTS the remainder before dividing -- which changes
	 * nothing, the division being exact after it -- and it re-reads the
	 * remainder from the caller's array rather than keeping it in a
	 * register.  Both are reproduced: the second is observable if `out`
	 * ever aliases something the divisor lives in.
	 */
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

	/* The sixth word is the rest of it, and only its low half is stored. */
	out[5] = (unsigned int)acc;
}

/*
 * The encoder run backwards: Horner from the top digit down, then the bits
 * back out from the bottom.
 *
 * THE MULTIPLIES ARE 64 BY 32.  Each modulus is loaded as a single word and
 * the object forms `mull` for the low product and one `imul` for the cross
 * term, with no fourth partial product -- the shape of a 64-bit multiply
 * whose right operand has a zero high word.  The overflow that shape allows
 * is the object's: nothing checks that the digits are below their moduli, and
 * a digit that is not simply carries into the next.
 *
 * THE LOOP BOUND IS RE-READ FROM THE OBJECT on every iteration, and the shift
 * is arithmetic.  Both are kept.
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
