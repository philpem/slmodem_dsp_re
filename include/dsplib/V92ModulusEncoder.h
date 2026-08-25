/*
 * V92ModulusEncoder.h -- the V.92 modulus encoder.
 *
 * Reconstructed from dsplibs.o.  Three members, 6,910 bytes: the constructor,
 * `reset(V92MappingParams *)` and `progress`.  This tree defines the
 * constructor and declares the other two.
 *
 * NO DESTRUCTOR.  The blob has no D0/D1/D2 for this class, so none is
 * declared here -- declaring one would emit a symbol the original does not
 * have.  The constructor allocates nothing, which is consistent: it is 96
 * bytes of `movl $0x0` and a `ret`, with no call in it.
 *
 * THE OBJECT IS 0x54 BYTES, and it is measured rather than bounded:
 *
 *     53bb6:  c7 04 24 54 00 00 00   movl $0x54,(%esp)
 *     53bbd:  e8 ..                  call sysdep_malloc
 *     53bc7:  e8 ..                  call V92ModulusEncoder::V92ModulusEncoder()
 *
 * in `V92Transmitter::V92Transmitter`, which stores the result at its own
 * +0x48.  The furthest any of the three members reaches is +0x50, a four-byte
 * access, so the two agree (finding F1248).
 *
 * THE CONSTRUCTOR STARTS AT +0x18, WHICH IS THE INTERESTING PART.  It zeroes
 * thirteen consecutive words from +0x18 to +0x48 and leaves +0x00..+0x14 and
 * +0x4c, +0x50 exactly as it found them.  Thirteen separate `movl $0x0` in
 * descending address order is what GCC emits for a member-initialiser list
 * over thirteen scalars -- an array would be a loop or a `memset` -- so they
 * are thirteen members and not a block.  `reset` is the one that fills the
 * first six words and the last two: it writes +0x4c and +0x50 at two of its
 * exits and copies the parameter block into +0x18 upward.
 *
 * WHAT `reset` AND `progress` ADD TO THAT MAP.  The six words at the bottom
 * are THREE 64-BIT MEMBERS, not six 32-bit ones: `reset` writes +0x00/+0x04,
 * +0x08/+0x0c and +0x10/+0x14 as three add-with-carry pairs, and `progress`
 * loads +0x08 and +0x10 as 64-bit values and divides and shifts them
 * ARITHMETICALLY (`sar`, `shrd`, the sign-correction before a `>>1`), so the
 * type is signed.  What they hold is the product of the twelve moduli:
 * +0x00 is that product truncated to 64 bits, and +0x08/+0x10 are the same
 * product carried exactly, split into two 63-bit limbs -- +0x10 is masked
 * with 0x7fffffffffffffff at both of `reset`'s exits and +0x08 takes the
 * bits above it (`u >> (62 - n)` against `(u << (n + 1)) & MASK`).
 *
 * +0x50 IS SIGNED and that is forced, not chosen: `progress` opens with
 * `cmp $1 / je / jle / cmp $2 / je`, and GCC emits `jle`/`jg` for a switch
 * over a signed index and `jbe`/`ja` over an unsigned one (finding F1378).
 * +0x48 goes the other way -- `cmp $0x3f / jbe` -- so it is unsigned.
 */

#ifndef DSPLIB_V92MODULUSENCODER_H
#define DSPLIB_V92MODULUSENCODER_H

class V92MappingParams;

class V92ModulusEncoder {
public:
	/* Written. */
	V92ModulusEncoder();

	/*
	 * The signatures are the mangling's.  `progress` switches on +0x50
	 * over 0, 1 and 2 and returns at once for anything else, so that word
	 * selects the conversion in force; `out` is twelve words long, which
	 * is what cases 1 and 2 write unconditionally and what case 0's digit
	 * chain fills.
	 */
	void reset(V92MappingParams *params);
	void progress(unsigned char *bytes, unsigned int *out);

	/*
	 * Public because the original's access specifiers are not recoverable
	 * and one access section keeps the class standard-layout.  A 32-bit
	 * store establishes width and not type; the spellings below are
	 * `unsigned int` throughout because nothing in these three members
	 * says otherwise.
	 */

	/*
	 * +0x00 .. +0x14  NOT WRITTEN BY THE CONSTRUCTOR.  Three 64-bit
	 * members that `reset` fills and the object leaves indeterminate
	 * until it runs.
	 *
	 * `product` is the twelve moduli multiplied together and allowed to
	 * wrap; nothing in these three members ever reads it back.
	 * `productHi` and `productLo` are the same product carried exactly,
	 * as two limbs in base 2^63, and they are what `progress` divides by
	 * to turn a bit string into twelve digits.
	 */
	long long product;
	long long productHi;
	long long productLo;

	/*
	 * +0x18 .. +0x48  The thirteen the constructor zeroes, one member
	 * each for the reason above.
	 *
	 * +0x18 .. +0x44 are the TWELVE MODULI, copied by `reset` from the
	 * parameter block's +0x1c .. +0x48 and used as divisors by
	 * `progress`; each is loaded zero-extended into a 64-bit divide, so
	 * unsigned is forced.  +0x48 is the BIT COUNT: case 0 reads
	 * `bytes[+0x48 - 1]` down to `bytes[0]`, one bit per byte.  The names
	 * are left as they were rather than renumbered, because six mutation
	 * entries and the constructor's initialiser list quote them.
	 */
	unsigned int field_18;
	unsigned int field_1c;
	unsigned int field_20;
	unsigned int field_24;
	unsigned int field_28;
	unsigned int field_2c;
	unsigned int field_30;
	unsigned int field_34;
	unsigned int field_38;
	unsigned int field_3c;
	unsigned int field_40;
	unsigned int field_44;
	unsigned int field_48;	/* `reset` copies the parameters' +0x00 here */

	/*
	 * +0x4c, +0x50  Written by `reset` (`movl $0x0` to both, at two of
	 * its exits) and not by the constructor.  +0x50 is `progress`'s
	 * selector; +0x4c it both reads and writes -- case 0 reads it, uses
	 * it to choose between the value and its complement, and stores it
	 * back exclusive-ORed with a comparison, so it is a one-bit state
	 * carried from call to call.
	 */
	unsigned int field_4c;
	int field_50;
};

#endif /* DSPLIB_V92MODULUSENCODER_H */
