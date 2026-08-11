/*
 * V90MappingParams.h -- the constellation table the V.90 CP packer reads.
 *
 * THE CLASS NAME IS THE ORIGINAL'S, from the mangling of the one caller:
 * `_Z11V90CPPackerP16V90MappingParamsP22tagV90AdditionalCPinfoPsi` names its
 * first argument `V90MappingParams *`.  `V90Demodulator.h` already carries
 * `class V90MappingParams;` as a forward declaration and nothing in the tree
 * defines it; this header is the first definition, and it is deliberately a
 * NEW header so that V90Demodulator.h -- which several batches are merging
 * against -- does not have to change.  That is the same reasoning
 * DILdescriptorPacker.h gives for living apart from V90Phase3Modulator.h.
 *
 * NOTHING ELSE HERE IS THE ORIGINAL'S.  Every member name is invented and
 * describes what `getConstellationsIndex`, `getConstellationMask` and
 * `getCodecConstellationMask` do with the bytes; those three functions are
 * unmangled symbols and carry no type information at all.  The layout is
 * measured from their displacements and from nothing else:
 *
 *   +0x004 + 0x80*k   getConstellationMask       `lea 0x4(%ebx,%ecx,1)`
 *                     with %ebx = k << 7, read one unsigned byte at a time
 *   +0x304 + 0x80*k   getCodecConstellationMask  `lea 0x304(%ebx,%ecx,1)`,
 *                     the same shape -- and getConstellationsIndex compares
 *                     BOTH arrays, at `-0x300(%ecx)` and `(%ecx)` off one
 *                     cursor 0x300 apart
 *   +0x604 + 4*k      a dword length, `mov 0x604(%ecx,%edx,4)`, used as the
 *                     bound of the byte loops
 *   +0x638 + 4*k      a dword list of representative constellations,
 *                     `mov 0x638(%ecx,%esi,4)`, written by
 *                     getConstellationsIndex and read by both mask functions
 *
 * The largest index any of the three reaches is 5 (`cmpl $0x5,i; jbe`), so
 * every array is six entries.  0x80 * 6 = 0x300 exactly, which is why the two
 * byte tables abut.
 *
 * THE 28 BYTES AT +0x61C ARE NOT EXPLAINED.  No function in this file touches
 * them and no other reconstructed function reaches this struct, so they are a
 * pad and are named as one rather than guessed into fields.  Neither is the
 * total size known: 0x650 is where the last member this tree can see ends,
 * not a measured `sizeof`.
 *
 * THE FOUR BYTES AT +0 USED TO BE IN THE SAME POSITION AND NOW HAVE ONE
 * READER, `V90Demodulator::getBitRate`:
 *
 *     1b8e8  8b 42 18              mov    0x18(%edx),%eax
 *     1b8ed  69 08 40 1f 00 00     imul   $0x1f40,(%eax),%ecx
 *     1b8f3  52 51 df 2c 24        push;push;fildll (%esp)
 *
 * with `%edx` the demodulator and +0x18 its `mappingParamsAlt`.  The `fildll`
 * off a pushed pair whose high word was zeroed BEFORE the multiply is the
 * unsigned-to-float idiom -- a signed `int` converts with a 32-bit `fildl`
 * and no push at all -- so the value entering the arithmetic is UNSIGNED, and
 * that is the whole of what is forced.  It is typed here and still named for
 * its offset: 8000/6 is the V.90 downstream rate granularity, which makes
 * this a bit count per six-sample frame, but that is an interpretation of the
 * arithmetic rather than something the object states, and it belongs in
 * finding 1160 and not in a member name in another batch's header.
 *
 * THE LENGTH'S SIGNEDNESS IS MEASURED, the index's is not.  Every use of the
 * length is an unsigned comparison -- `cmp %ebp,%esi; jb` in both mask
 * functions, `cmp %esi,%ebx; ja` and `cmp $0x0,%ebx; jbe` in
 * getConstellationsIndex -- so it is an unsigned type.  The index is only
 * ever loaded and used to scale, which says nothing, and `int` is the choice.
 */

#ifndef DSPLIB_V90MAPPINGPARAMS_H
#define DSPLIB_V90MAPPINGPARAMS_H

/* One constellation's table, and the number of them. */
#define V90_CONSTELLATION_MAX	128
#define V90_CONSTELLATIONS	6

class V90MappingParams {
public:
	unsigned int word_0;					/* +0x000 */
	unsigned char constellation[V90_CONSTELLATIONS]
				   [V90_CONSTELLATION_MAX];	/* +0x004 */
	unsigned char codecConstellation[V90_CONSTELLATIONS]
					[V90_CONSTELLATION_MAX];/* +0x304 */
	unsigned int constellationSize[V90_CONSTELLATIONS];	/* +0x604 */
	unsigned char pad_61c[28];				/* +0x61c */
	int distinctIndex[V90_CONSTELLATIONS];			/* +0x638 */
};

extern "C" {

/*
 * Collapse the six constellations to their distinct values.
 *
 * Two constellations are the same when their lengths are equal and their two
 * byte tables agree over that length; ZERO-LENGTH CONSTELLATIONS ARE
 * THEREFORE ALL EQUAL TO EACH OTHER, which is what the object does and is
 * reproduced deliberately -- see the .cpp.
 *
 * Writes through `group` the group number of each of the six, leaves in
 * `distinctIndex[g]` the first constellation of group `g`, and returns how
 * many groups there are, which is between 1 and 6.  `group` needs six
 * entries.
 */
unsigned int getConstellationsIndex(V90MappingParams *params, int *group);

/*
 * The occupancy bitmap of constellation `which`'s first table: with `n` the
 * byte's low nibble and `h` its high one, bit `n` of `mask[h]` is set.  The
 * function zeroes `mask[0..7]` and then sets bits, so a byte of 0x80 or more
 * sets a bit in an entry it never cleared -- the object masks nothing.
 *
 * `which` selects through `distinctIndex`, and `6` or above selects entry 0.
 * The bound is a SIGNED test (`cmp $0x6,%esi; setl`), so a negative `which`
 * is passed straight through and indexes before the array; nothing in the
 * object guards it.
 */
void getConstellationMask(V90MappingParams *params, int which, short *mask);

/* The same over the second table.  See getConstellationMask. */
void getCodecConstellationMask(V90MappingParams *params, int which,
			       short *mask);

}

#endif /* DSPLIB_V90MAPPINGPARAMS_H */
