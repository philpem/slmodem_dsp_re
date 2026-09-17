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
 * Their meanings ARE established now.  `progress` is written -- the encoder
 * in `src/pump/v90/V90ModulusEncoder.cpp`, the decoder in
 * `V90ModulusDecoder.cpp` -- and the constructor's `@param` docs below tie
 * `field_00`..`field_10` to the five conversion moduli, `field_18` to the bit
 * count, and `field_14` to an argument neither `progress` reads.  The fields
 * keep offset names only because the object itself spells none; naming them
 * `modulus*`/`bitCount` is the issue #119 pass that also unblocks
 * `V90Mapper`/`V90Demapper::word_08`.
 */

#ifndef DSPLIB_MODULUSCODER_H
#define DSPLIB_MODULUSCODER_H

class ModulusEncoder {
public:
	/**
	 * @brief Default-construct with all seven members zeroed.
	 *
	 * Zeroes all seven words and nothing else -- 53 bytes of `movl $0x0`
	 * and a `ret`, with no call and no allocation.
	 */
	ModulusEncoder();

	/**
	 * @brief Construct with all seven members set explicitly.
	 *
	 * A member-initialiser list and nothing else: each argument is
	 * stored to its field in order, with no arithmetic. `progress()`
	 * reads @p g as a bit count (an index into its `bytes` argument),
	 * and @p a..@p e as the five conversion moduli; @p f is stored but
	 * read by neither `progress()` member.
	 *
	 * @param a  Modulus for output digit 0 (`field_00`).
	 * @param b  Modulus for output digit 1 (`field_04`).
	 * @param c  Modulus for output digit 2 (`field_08`).
	 * @param d  Modulus for output digit 3 (`field_0c`).
	 * @param e  Modulus for output digit 4 (`field_10`).
	 * @param f  Stored but unused by progress() (`field_14`).
	 * @param g  Bit count / byte position (`field_18`).
	 */
	ModulusEncoder(unsigned int a, unsigned int b, unsigned int c,
		       unsigned int d, unsigned int e, unsigned int f,
		       unsigned int g);

	/**
	 * @brief Convert a bit string into six mixed-radix digits.
	 *
	 * Packs `field_18` bits from @p bytes (one bit per byte, MSB-first
	 * from the top of the array) into a signed 64-bit accumulator, then
	 * divides out five digits by the moduli `field_00`.. `field_10` in
	 * order; the sixth output word is whatever remains. The accumulator
	 * is genuinely signed (the object uses the signed `__divdi3`/
	 * `__moddi3` helpers), so a bit count of 64 or more lets the top bit
	 * become the sign -- reproduced rather than tidied.
	 *
	 * @param bytes  Input bit string, `field_18` bytes, one bit (bit 0)
	 *               used per byte.
	 * @param out    Output: six mixed-radix digits.
	 */
	void progress(unsigned char *bytes, unsigned int *out);

	/*
	 * Public because the original's access specifiers are not recoverable
	 * and one access section keeps the class standard-layout.  The type is
	 * the seven-argument constructor's `unsigned int`, which is evidence
	 * and not a default.  The names stay the offsets because the object
	 * spells none, though `progress` now gives each a role -- see the
	 * constructor's `@param` docs and the file banner.
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
	/** @brief Default-construct with all seven members zeroed. */
	ModulusDecoder();

	/**
	 * @brief Construct with all seven members set explicitly.
	 *
	 * Byte for byte ModulusEncoder's seven-argument constructor, module
	 * for member. See ModulusEncoder::ModulusEncoder(unsigned int,
	 * unsigned int, unsigned int, unsigned int, unsigned int, unsigned
	 * int, unsigned int) for what each argument feeds into progress().
	 */
	ModulusDecoder(unsigned int a, unsigned int b, unsigned int c,
		       unsigned int d, unsigned int e, unsigned int f,
		       unsigned int g);

	/**
	 * @brief Convert six mixed-radix digits back into a bit string.
	 *
	 * The encoder run backwards: Horner's method from the top digit down
	 * through moduli `field_10`.. `field_00` builds a signed 64-bit
	 * accumulator, then `field_18` bits come back out from the bottom.
	 * The loop bound (`field_18`) is re-read from the object every
	 * iteration and the shift is arithmetic; both are the object's own
	 * behaviour. Nothing checks that a digit is below its modulus -- an
	 * out-of-range digit simply carries into the next.
	 *
	 * @param bytes  Output bit string, `field_18` bytes, one bit per byte.
	 * @param out    Input: six mixed-radix digits (named @p out to match
	 *               progress()'s counterpart, though this direction reads
	 *               it).
	 */
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
