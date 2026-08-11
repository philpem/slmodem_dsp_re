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
 * access, so the two agree (finding 1248).
 *
 * THE CONSTRUCTOR STARTS AT +0x18, WHICH IS THE INTERESTING PART.  It zeroes
 * thirteen consecutive words from +0x18 to +0x48 and leaves +0x00..+0x14 and
 * +0x4c, +0x50 exactly as it found them.  Thirteen separate `movl $0x0` in
 * descending address order is what GCC emits for a member-initialiser list
 * over thirteen scalars -- an array would be a loop or a `memset` -- so they
 * are thirteen members and not a block.  `reset` is the one that fills the
 * first six words and the last two: it writes +0x4c and +0x50 at two of its
 * exits and copies the parameter block into +0x18 upward.
 */

#ifndef DSPLIB_V92MODULUSENCODER_H
#define DSPLIB_V92MODULUSENCODER_H

class V92MappingParams;

class V92ModulusEncoder {
public:
	/* Written. */
	V92ModulusEncoder();

	/*
	 * Declared and deliberately not defined; the signatures are the
	 * mangling's.  `progress` opens by switching on +0x50 against 1 and 2
	 * and returning at once for anything else, so that word selects the
	 * conversion in force.
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
	 * +0x00 .. +0x14  NOT WRITTEN BY THE CONSTRUCTOR.  Six words that
	 * `reset` fills and the object leaves indeterminate until it runs.
	 */
	unsigned int head[6];

	/*
	 * +0x18 .. +0x48  The thirteen the constructor zeroes, one member
	 * each for the reason above.
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
	 * selector; +0x4c it both reads and writes.
	 */
	unsigned int field_4c;
	unsigned int field_50;
};

#endif /* DSPLIB_V92MODULUSENCODER_H */
