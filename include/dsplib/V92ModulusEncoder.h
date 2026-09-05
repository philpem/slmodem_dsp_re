/**
 * @file V92ModulusEncoder.h
 * @brief The V.92 modulus encoder.
 *
 * Three members, 6,910 bytes: the constructor, `reset(V92MappingParams *)`
 * and `progress`. This tree defines the constructor and declares the other
 * two.
 *
 * No destructor: the blob has no D0/D1/D2 for this class, so none is
 * declared here -- declaring one would emit a symbol the original does not
 * have. Consistent with the constructor allocating nothing (96 bytes of
 * `movl $0x0` and a `ret`, no call in it).
 *
 * The object is 0x54 bytes, measured from `V92Transmitter::V92Transmitter`'s
 * `sysdep_malloc(0x54)` immediately before it calls this constructor, and
 * agreeing with the furthest member access at +0x50 (finding F1248).
 *
 * The constructor's own zeroing starts at +0x18: thirteen separate
 * `movl $0x0` in descending address order, which is what GCC emits for a
 * member-initialiser list over thirteen scalars (an array would be a loop
 * or a `memset`), leaving +0x00..+0x14 and +0x4c/+0x50 exactly as found.
 * `reset` is what fills those: the first six words and the last two, at two
 * of its exits, and it copies the parameter block into +0x18 upward.
 *
 * `reset` and `progress` between them settle the rest of the map, including
 * two facts forced by the encoding rather than chosen: the six words at the
 * bottom are three 64-bit members (not six 32-bit ones), signed, holding the
 * product of the twelve moduli as a truncated 64-bit value plus the same
 * product carried exactly as two 63-bit limbs; and the +0x50 selector is
 * signed while +0x48 is unsigned, per the `jle`/`jbe` branches each is
 * switched on (finding F1376).
 */

#ifndef DSPLIB_V92MODULUSENCODER_H
#define DSPLIB_V92MODULUSENCODER_H

class V92MappingParams;

class V92ModulusEncoder {
public:
	/** @brief Construct with every member left indeterminate; reset()
	 *         must be called before use. Empty body -- 96 bytes of
	 *         `movl $0x0`/`ret`, no call. */
	V92ModulusEncoder();

	/**
	 * @brief Load the twelve moduli and bit count from a mapping
	 *        parameter block and (re)compute the running product state.
	 * @param params  Supplies the twelve moduli (its +0x1c..+0x48) and
	 *                the bit count (its +0x00).
	 */
	void reset(V92MappingParams *params);

	/**
	 * @brief Convert one block of input bits into the twelve mixed-radix
	 *        digits the moduli define. `field_50` selects which of three
	 *        conversions runs (0, 1 or 2); `out` is always the twelve
	 *        output digits, filled unconditionally by cases 1 and 2 and
	 *        by case 0's digit chain.
	 * @param bytes  One input bit per byte. Case 0 reads
	 *               `bytes[field_48 - 1]` down to `bytes[0]`.
	 * @param out    Receives the twelve output digits.
	 */
	void progress(unsigned char *bytes, unsigned int *out);

	/*
	 * Public because the original's access specifiers are not recoverable
	 * and one access section keeps the class standard-layout.  A 32-bit
	 * store establishes width and not type; the spellings below are
	 * `unsigned int` throughout because nothing in these three members
	 * says otherwise.
	 */

	/* +0x00 .. +0x14  Three 64-bit members, left indeterminate by the
	 * constructor and filled by reset() (finding F1376). `product` is
	 * the twelve moduli multiplied together and allowed to wrap; nothing
	 * here reads it back. `productHi`/`productLo` carry the same product
	 * exactly, as two limbs in base 2^63, which `progress` divides by to
	 * turn a bit string into twelve digits. `productHi` (+0x08) is
	 * written twice by `reset`, the first time for nothing -- a dead
	 * store through a pointer that GCC 3.4.2 does not eliminate,
	 * reproduced rather than dropped (docs/deviations.md D263). */
	long long product;
	long long productHi;
	long long productLo;

	/*
	 * +0x18 .. +0x48  The thirteen words the constructor zeroes, one
	 * member each (see the file comment for why they're thirteen scalars
	 * and not a block).
	 *
	 * +0x18 .. +0x44 are the twelve moduli, copied by `reset` from the
	 * parameter block's +0x1c..+0x48 and used as divisors by `progress`;
	 * each is loaded zero-extended into a 64-bit divide, so unsigned is
	 * forced. +0x48 is the bit count: case 0 reads `bytes[+0x48 - 1]`
	 * down to `bytes[0]`, one bit per byte (finding F1376, and F1377 for
	 * where `reset` gets these thirteen values from). The names are left
	 * as `field_NNNN` rather than renumbered, because six mutation
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
	 * its exits) and not by the constructor. +0x50 is `progress`'s
	 * selector, and is `int` rather than `unsigned int`: forced by the
	 * `jle`/`jbe` branch each is switched on (finding F1376). +0x4c it
	 * both reads and writes -- case 0 reads it, uses it to choose between
	 * the value and its complement, and stores it back exclusive-ORed
	 * with a comparison, so it is a one-bit state carried from call to
	 * call.
	 */
	unsigned int field_4c;
	int field_50;
};

#endif /* DSPLIB_V92MODULUSENCODER_H */
