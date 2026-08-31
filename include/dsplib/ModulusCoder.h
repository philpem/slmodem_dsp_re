/*
 * ModulusCoder.h -- ModulusEncoder and ModulusDecoder, the modulus conversion
 * either end of the V.90 mapper.
 *
 * Reconstructed from dsplibs.o.  Three members each: two constructors and
 * `progress`.  This tree defines the default constructor of each and declares
 * the rest.  They are one header because they are one object layout, one
 * pair of constructors emitted from the same shape of source, and adjacent in
 * the blob (0x31ff0..0x32601).
 *
 * NEITHER HAS A DESTRUCTOR, and that is not an omission here: the blob has no
 * `D0`, `D1` or `D2` for either name.  So neither class may declare one -- a
 * user-declared destructor emits an out-of-line symbol the original does not
 * have.  `V90Demapper::~V90Demapper` corroborates it from the other side: it
 * destroys its `V90SignBitsExtractor` member and runs no destructor at all
 * over the ModulusDecoder embedded at +0x648.
 *
 * THE OBJECT IS 0x1c BYTES, from two independent bounds (finding F1247):
 *
 *   below   the seven-argument constructor stores its seven `unsigned int`
 *           parameters at +0x00, +0x04 ... +0x18 and the default constructor
 *           zeroes exactly those seven words, so there are seven members and
 *           the last ends at 0x1c; `progress` reaches no further.
 *   above   `V90Mapper::V90Mapper` constructs an embedded ModulusEncoder at
 *           `lea 0x670(%ebx)` and then, as its very next act, constructs a
 *           `V90SpectralShaper` at `lea 0x68c(%ebx)`.  Two CONSTRUCTED
 *           members 0x1c apart is a hard ceiling, not an inference from a
 *           displacement.  `V90Demapper` says the same for the decoder from
 *           +0x648 to its next field at +0x664, and
 *           include/dsplib/V90Demapper.h already records that 0x1c as "the
 *           distance to the next field and an upper bound, not a size".
 *
 * The two bounds meet, so the upper one is now a measurement.  That file's
 * note stands as written and this is its other half; nothing there needs
 * changing.
 *
 * SEVEN MEMBERS, IN THE CONSTRUCTOR'S PARAMETER ORDER.  The mangling is
 * `C1Ejjjjjjj` -- seven `unsigned int` -- and the body is seven stores to
 * ascending offsets in argument order with no arithmetic anywhere.  That is
 * what a constructor whose whole job is a member-initialiser list looks like,
 * so the seven parameters ARE the seven members and the order is settled.
 * Their meanings are not: nothing in this tree names them yet, and `progress`
 * is another batch's work.
 */

#ifndef DSPLIB_MODULUSCODER_H
#define DSPLIB_MODULUSCODER_H

class ModulusEncoder {
public:
	/*
	 * Written.  Zeroes all seven words and nothing else -- 53 bytes of
	 * `movl $0x0` and a `ret`, with no call and no allocation.
	 */
	ModulusEncoder();

	/*
	 * Both signatures are the mangling's; the seven-argument form is now
	 * defined too (seven stores in argument order, ModulusCoder.cpp).
	 * `progress` reads +0x18 as an index into its first
	 * argument (`mov 0x18(%eax),%ebp` then `movzbl (%edx,%ebp,1),%ebx`),
	 * so that word is a byte position in the caller's buffer and the run
	 * is resumable; the other six it uses as a group and this file does
	 * not claim what they are.
	 */
	ModulusEncoder(unsigned int a, unsigned int b, unsigned int c,
		       unsigned int d, unsigned int e, unsigned int f,
		       unsigned int g);
	void progress(unsigned char *bytes, unsigned int *out);

	/*
	 * Public because the original's access specifiers are not recoverable
	 * and one access section keeps the class standard-layout.  The names
	 * are the offsets because the object gives no meanings; the type is
	 * the seven-argument constructor's `unsigned int`, which is evidence
	 * and not a default.
	 */
	unsigned int field_00;
	unsigned int field_04;
	unsigned int field_08;
	unsigned int field_0c;
	unsigned int field_10;
	unsigned int field_14;
	unsigned int field_18;	/* byte position, per `progress` above */
};

/*
 * The decoder is the same object and the same pair of constructors, byte for
 * byte apart from the symbol names: 53 bytes each, the same seven offsets in
 * the same order.  Only `progress` differs (534 bytes against 497).
 */
class ModulusDecoder {
public:
	/* Written. */
	ModulusDecoder();

	ModulusDecoder(unsigned int a, unsigned int b, unsigned int c,
		       unsigned int d, unsigned int e, unsigned int f,
		       unsigned int g);
	void progress(unsigned char *bytes, unsigned int *out);

	unsigned int field_00;
	unsigned int field_04;
	unsigned int field_08;
	unsigned int field_0c;
	unsigned int field_10;
	unsigned int field_14;
	unsigned int field_18;
};

#endif /* DSPLIB_MODULUSCODER_H */
