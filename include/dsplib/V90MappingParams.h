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
 * TWENTY-FOUR OF THE 28 BYTES AT +0x61C ARE NOW EXPLAINED, and the sentence
 * that used to stand here -- that no other reconstructed function reaches
 * this struct -- is RETRACTED.  `V90ConstellationDesigner::spectralDesign`
 * writes six consecutive dwords at +0x620..+0x634, and `tools/vparse.py`
 * names every one of the six `V90Parameters` fields it copies them from, so
 * the six are the spectral shaper's description and are named for their
 * sources rather than for their offsets.  See the members below and
 * `include/dsplib/V90SpectralConditions.h`.  THE FOUR BYTES AT +0x61C ARE
 * NOW EXPLAINED TOO -- `V90ConstellationDesigner::process` stores the
 * constant 1 into them and nothing reads them -- so all 28 are, and the
 * "still untouched" sentence that used to end this paragraph is retracted.
 *
 * The total size is still not known: 0x650 is where the last member this
 * tree can see ends, not a measured `sizeof`.
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
 * finding F1160 and not in a member name in another batch's header.
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

class V92CP;
struct tagV90AdditionalCPinfo;
struct V90CPUnPck;

class V90MappingParams {
public:
	unsigned int word_0;					/* +0x000 */
	unsigned char constellation[V90_CONSTELLATIONS]
				   [V90_CONSTELLATION_MAX];	/* +0x004 */
	unsigned char codecConstellation[V90_CONSTELLATIONS]
					[V90_CONSTELLATION_MAX];/* +0x304 */
	unsigned int constellationSize[V90_CONSTELLATIONS];	/* +0x604 */

	/*
	 * +0x61c  `V90ConstellationDesigner::process` writes the constant 1
	 * here and nothing anywhere in the object reads it: `mov $0x1,%ecx`
	 * then `mov %ecx,0x61c(%eax)` with %eax the mapping block, a `movl`
	 * with no operand-size prefix, which is the whole of what is forced.
	 * It used to be `pad_61c[4]` and the header said the four bytes were
	 * untouched by anything reconstructed; that sentence is retracted.
	 * The name is the offset's because a write-only slot has no meaning to
	 * take a name from.
	 */
	unsigned int word_61c;					/* +0x61c */

	/*
	 * +0x620..+0x634  The spectral shaper, six dwords written together by
	 * `V90ConstellationDesigner::spectralDesign`.  Each is a straight `mov`
	 * from a `V90Parameters` field `vparse.py` names, so the names below
	 * are the AUTHOR'S for the sources and ours only for the destinations;
	 * the two arms copy `SPECTRAL_SHAPER_*` or
	 * `GERMAN_PBX_SPECTRAL_SHAPER_*` into the same six slots.
	 *
	 * "READ BY NOTHING THIS TREE HAS WRITTEN" USED TO END THAT SENTENCE AND
	 * IS RETRACTED.  `V90Demapper::reset` reads +0x620 four times over --
	 * into `signBitGroups` unchanged, as `6 - it`, as `6 / it`, and as
	 * `V90SignBitsExtractor::reset`'s spacing -- and `spectralDesign`'s own
	 * caller forms `6 - shaperSR` too (`mov $0x6,%cl; sub 0x620(%ebx),%cl`,
	 * quoted in V90ConstellationDesigner.cpp).  Finding F4342's rule is why
	 * this matters: a claim that nothing reads a field is a claim about
	 * every function in the object.  Finding F5004.
	 *
	 * The widths are the store encodings -- six `movl` -- and the types
	 * below are the SOURCES' types, which is what a four-byte copy carries
	 * and not something the copy itself forces.  `shaperId` is the one
	 * exception and it is measured: it is not a copy but
	 * `min(SPECTRAL_SHAPER_ID, rate)` computed with an UNSIGNED compare
	 * (`ja` in one arm, `jbe` in the other), which the `unsigned int` rate
	 * argument forces whatever the parameter's own `int` says.
	 *
	 * AND THERE IS NOW A SECOND MEASUREMENT, ON `shaperSR`, WHICH IS
	 * DELIBERATELY NOT ACTED ON HERE.  `V90Demapper::reset` divides six by
	 * it with `divl` and not `idiv` (0x308b6), which is unsigned
	 * arithmetic; but what that forces is the NUMERATOR's type, and
	 * `V90DEMAPPER_FRAME` is already `6u`, so the reading is satisfied with
	 * the field left `int`.  The two spellings agree over every spacing a
	 * caller can produce -- `6 - x` has the same bits either way and the
	 * quotient differs only for a negative divisor -- so retyping would be
	 * choosing between two readings the object does not separate.  The name
	 * is not touched either: `shaperSR` is the author's for the PARAMETER
	 * `spectralDesign` copies, and the demapper's use of the same word as a
	 * sign-bit spacing is usage inference, which is the weakest of
	 * CLAUDE.md's three ranks and does not outrank a name from the
	 * parameter block.
	 */
	int shaperSR;						/* +0x620 */
	unsigned int shaperId;					/* +0x624 */
	float shaperA1;						/* +0x628 */
	float shaperA2;						/* +0x62c */
	float shaperB1;						/* +0x630 */
	float shaperB2;						/* +0x634 */

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

/*
 * `params->word_0` less 0x14 when `islong` is non-zero and less 8 when it is
 * not -- the same two constants `setV92CPpckFromParamsInfo` ends with, out of
 * line.  `V90CPPacker` sends the low five bits of the result.
 *
 * BOTH PARAMETER TYPES ARE INFERENCE FROM ONE CALL SITE and the object forces
 * neither: the body reads four bytes at offset 0 and tests the second
 * argument against zero, and there is exactly one relocation naming the
 * symbol in the whole object.  See the .cpp.
 */
int getDataBitRate(V90MappingParams *params, int islong);

/*
 * Fill this block from an unpacked V.90 CP message.  It writes `word_0`, both
 * byte tables, `constellationSize`, `word_61c`, the six shaper words and
 * `distinctIndex` -- essentially the whole block.
 *
 * **IT HAS NO CALLER IN THE OBJECT AND MUST NOT BE GIVEN ONE HERE.**  Zero
 * relocations of any kind name the symbol in the whole of dsplibs.o, so a
 * call added by this reconstruction would be new behaviour with nothing to
 * compare it against.  It is declared so that its differential test can reach
 * it and for no other reason.  See the .cpp and finding F7570.
 */
void setParamsInfoFromCPUnPck(V90MappingParams *params, V90CPUnPck *cp);

/*
 * Fill this block from a `V92CP`, the received V.92 CP message.  It is the
 * exact inverse of `setV92CPpckFromParamsInfo` below and writes the same
 * members `setParamsInfoFromCPUnPck` does, from the other source.
 *
 * **IT HAS NO CALLER IN THE OBJECT AND MUST NOT BE GIVEN ONE HERE**, for the
 * reason `setParamsInfoFromCPUnPck` gives above: zero relocations of any kind
 * name the symbol in the whole of dsplibs.o.  The two relocations that DO name
 * something spelled like it belong to `V92setParamsInfoFromCPUnPck`, a
 * different symbol at .text+0x12f00 with two call sites in `runPcmModem`
 * (finding F7571).  Declared so that its differential test can reach it and for
 * no other reason.  See the .cpp.
 */
void setParamsInfoFromV92CPUnPck(V90MappingParams *params, V92CP *cp);

/*
 * Fill a `V92CP` from this block and the record beside it.  The two source
 * types are the object's, out of the mangling of the V.90 twin
 * `V90CPPacker(V90MappingParams *, tagV90AdditionalCPinfo *, short *, int)`
 * and out of the call site's three offsets into `VPcmFloModem`; see the .cpp.
 */
void setV92CPpckFromParamsInfo(V90MappingParams *params,
			       tagV90AdditionalCPinfo *info, V92CP *cp);

/*
 * Print the six spectral-shaper fields, six `edprintf` lines and nothing
 * else.  NOT GATED: the level test is inside `edprintf`, so at level 0 the
 * calls still happen and print nothing.
 */
void displaySpectralParams(V90MappingParams *params);

}

#endif /* DSPLIB_V90MAPPINGPARAMS_H */
