/**
 * @file V90MappingParams.h
 * @brief `V90MappingParams`, the constellation table the V.90 CP packer and
 *        unpacker read and write.
 *
 * The class name is the original's, from the mangling of the one caller:
 * `_Z11V90CPPackerP16V90MappingParamsP22tagV90AdditionalCPinfoPsi` names its
 * first argument `V90MappingParams *`. `V90Demodulator.h` already carries
 * `class V90MappingParams;` as a forward declaration and nothing in the tree
 * defines it; this header is the first definition, and it is deliberately a
 * new header so that V90Demodulator.h -- which several batches are merging
 * against -- does not have to change. That is the same reasoning
 * DILdescriptorPacker.h gives for living apart from V90Phase3Modulator.h.
 *
 * Nothing else here is the original's: every member name is invented and
 * describes what `getConstellationsIndex`, `getConstellationMask` and
 * `getCodecConstellationMask` do with the bytes, since those three functions
 * are unmangled symbols and carry no type information at all. The layout is
 * measured from their displacements and from nothing else:
 *
 *   +0x004 + 0x80*k   getConstellationMask       `lea 0x4(%ebx,%ecx,1)`
 *                     with %ebx = k << 7, read one unsigned byte at a time
 *   +0x304 + 0x80*k   getCodecConstellationMask  `lea 0x304(%ebx,%ecx,1)`,
 *                     the same shape -- and getConstellationsIndex compares
 *                     both arrays, at `-0x300(%ecx)` and `(%ecx)` off one
 *                     cursor 0x300 apart
 *   +0x604 + 4*k      a dword length, `mov 0x604(%ecx,%edx,4)`, used as the
 *                     bound of the byte loops
 *   +0x638 + 4*k      a dword list of representative constellations,
 *                     `mov 0x638(%ecx,%esi,4)`, written by
 *                     getConstellationsIndex and read by both mask functions
 *
 * The largest index any of the three reaches is 5 (`cmpl $0x5,i; jbe`), so
 * every array is six entries. 0x80 * 6 = 0x300 exactly, which is why the two
 * byte tables abut.
 *
 * Twenty-four of the 28 bytes at +0x61c are now explained, and the sentence
 * that used to stand here -- that no other reconstructed function reaches
 * this struct -- is retracted. `V90ConstellationDesigner::spectralDesign`
 * writes six consecutive dwords at +0x620..+0x634, and `tools/vparse.py`
 * names every one of the six `V90Parameters` fields it copies them from, so
 * the six are the spectral shaper's description and are named for their
 * sources rather than for their offsets. See the members below and
 * `include/dsplib/V90SpectralConditions.h`. The four bytes at +0x61c are now
 * explained too -- `V90ConstellationDesigner::process` stores the constant 1
 * into them and nothing reads them -- so all 28 are, and the "still
 * untouched" sentence that used to end this paragraph is retracted.
 *
 * The total size is still not known: 0x650 is where the last member this
 * tree can see ends, not a measured `sizeof`.
 *
 * The four bytes at +0 used to be in the same position and now have one
 * reader, `V90Demodulator::getBitRate`:
 *
 *     1b8e8  8b 42 18              mov    0x18(%edx),%eax
 *     1b8ed  69 08 40 1f 00 00     imul   $0x1f40,(%eax),%ecx
 *     1b8f3  52 51 df 2c 24        push;push;fildll (%esp)
 *
 * with `%edx` the demodulator and +0x18 its `mappingParamsAlt`. The `fildll`
 * off a pushed pair whose high word was zeroed before the multiply is the
 * unsigned-to-float idiom -- a signed `int` converts with a 32-bit `fildl`
 * and no push at all -- so the value entering the arithmetic is unsigned,
 * and that is the whole of what is forced. It is typed here and still named
 * for its offset: 8000/6 is the V.90 downstream rate granularity, which
 * makes this a bit count per six-sample frame, but that is an interpretation
 * of the arithmetic rather than something the object states, and it belongs
 * in finding F1160 and not in a member name in another batch's header.
 *
 * The length's signedness is measured, the index's is not. Every use of the
 * length is an unsigned comparison -- `cmp %ebp,%esi; jb` in both mask
 * functions, `cmp %esi,%ebx; ja` and `cmp $0x0,%ebx; jbe` in
 * getConstellationsIndex -- so it is an unsigned type. The index is only
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
	 * `V90ConstellationDesigner::spectralDesign` -- straight copies from a
	 * `V90Parameters` field `vparse.py` names in each case (either the
	 * `SPECTRAL_SHAPER_*` or `GERMAN_PBX_SPECTRAL_SHAPER_*` arm), so the
	 * names below are the author's for the sources and ours only for the
	 * destinations. Also read by `V90Demapper::reset` (four ways, off
	 * +0x620) and by `spectralDesign`'s own caller -- a claim that nothing
	 * reads a field is a claim about every function in the object
	 * (findings F4342/F5004).
	 *
	 * `shaperId` is the one field that is not a plain copy: it is
	 * `min(SPECTRAL_SHAPER_ID, rate)` under an unsigned compare, forced by
	 * the `unsigned int` rate argument. `shaperSR` is also read by an
	 * unsigned divide elsewhere, but that only constrains the read side
	 * (`V90DEMAPPER_FRAME` is already `6u`) and does not outrank the `int`
	 * the parameter block itself uses, so it stays `int` (finding F5004).
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

/**
 * @brief Collapse the six constellations to their distinct values.
 *
 * Two constellations are the same when their lengths are equal and their
 * two byte tables agree over that length; zero-length constellations are
 * therefore all equal to each other, which is what the object does and is
 * reproduced deliberately -- see the .cpp.
 *
 * @param params  The mapping-parameters block to read.
 * @param group   Receives the group number of each of the six constellations
 *                (six entries). `params->distinctIndex[g]` is left holding
 *                the first constellation of group `g`.
 * @return How many distinct groups there are, between 1 and 6.
 */
unsigned int getConstellationsIndex(V90MappingParams *params, int *group);

/**
 * @brief Read the occupancy bitmap of one constellation's ordinary table.
 *
 * With `n` a table byte's low nibble and `h` its high one, sets bit `n` of
 * `mask[h]`. The function zeroes `mask[0..7]` first and only ever sets
 * bits, so a byte of 0x80 or more sets a bit in an entry it never cleared
 * -- the object masks nothing.
 *
 * @param params  The mapping-parameters block to read.
 * @param which   Selects a constellation through `distinctIndex`; 6 or
 *                above selects entry 0, and the bound is a signed test, so
 *                a negative value indexes before the array unguarded.
 * @param mask    Eight-entry output bitmap.
 */
void getConstellationMask(V90MappingParams *params, int which, short *mask);

/** @brief The same as getConstellationMask(), over the codec constellation table. */
void getCodecConstellationMask(V90MappingParams *params, int which,
			       short *mask);

/**
 * @brief Recover the data bit rate `V90CPPacker` sends.
 *
 * Computes `params->word_0` less 0x14 when `islong` is non-zero and less 8
 * when it is not -- the same two constants `setV92CPpckFromParamsInfo` ends
 * with, out of line.
 *
 * @param params  The mapping-parameters block to read.
 * @param islong  Non-zero selects the long-form offset (0x14); zero the short one (8).
 * @return The data bit rate; `V90CPPacker` sends its low five bits.
 */
int getDataBitRate(V90MappingParams *params, int islong);

/*
 * The three callerless setters (0x33570, 0x335f0, 0x33690), the inverses of
 * the getters beside them; V90MappingParamsInt.cpp defines each as a
 * wrapper over the file-static body `setParamsInfoFromCPUnPck` inlines.
 * All three are unmangled in the blob, hence this C block.
 */

/** @brief The inverse of getConstellationMask(): write a bitmap into the ordinary constellation table. @param params The block to modify. @param which The constellation index. @param mask Eight-entry input bitmap. */
void setConstellationMask(V90MappingParams *params, int which,
			  const short *mask);
/** @brief The inverse of getCodecConstellationMask(): write a bitmap into the codec constellation table. @param params The block to modify. @param which The constellation index. @param mask Eight-entry input bitmap. */
void setCodecConstellationMask(V90MappingParams *params, int which,
			       const short *mask);
/** @brief The inverse of getDataBitRate(): store a data bit rate back as `word_0`. @param params The block to modify. @param islong Selects the long- or short-form offset, as in getDataBitRate(). @param rate The data bit rate to store. */
void setDataBitRate(V90MappingParams *params, int islong, int rate);

/**
 * @brief Fill this block in from an unpacked V.90 CP message.
 *
 * Writes `word_0`, both byte tables, `constellationSize`, `word_61c`, the
 * six shaper words and `distinctIndex` -- essentially the whole block.
 *
 * It has no caller in the object and must not be given one here: zero
 * relocations of any kind name the symbol in the whole of dsplibs.o, so a
 * call added by this reconstruction would be new behaviour with nothing to
 * compare it against. It is declared so that its differential test can
 * reach it and for no other reason. See finding F7570.
 *
 * @param params  The block to fill in.
 * @param cp      The unpacked V.90 CP message to read from.
 */
void setParamsInfoFromCPUnPck(V90MappingParams *params, V90CPUnPck *cp);

/**
 * @brief Fill this block in from a `V92CP`, the received V.92 CP message.
 *
 * The exact inverse of setV92CPpckFromParamsInfo() below, and writes the
 * same members setParamsInfoFromCPUnPck() does, from the other source.
 *
 * It has no caller in the object and must not be given one here, for the
 * reason setParamsInfoFromCPUnPck() gives above: zero relocations of any
 * kind name the symbol in the whole of dsplibs.o. The two relocations that
 * do name something spelled like it belong to
 * `V92setParamsInfoFromCPUnPck`, a different symbol at .text+0x12f00 with
 * two call sites in `runPcmModem` (finding F7571). Declared so that its
 * differential test can reach it and for no other reason.
 *
 * @param params  The block to fill in.
 * @param cp      The received V.92 CP message to read from.
 */
void setParamsInfoFromV92CPUnPck(V90MappingParams *params, V92CP *cp);

/**
 * @brief Fill a `V92CP` in from this block and the record beside it.
 *
 * The two source types are the object's, out of the mangling of the V.90
 * twin `V90CPPacker(V90MappingParams *, tagV90AdditionalCPinfo *, short *, int)`
 * and out of the call site's three offsets into `VPcmFloModem`.
 *
 * @param params  The mapping-parameters block to read.
 * @param info    The additional-CP-info record beside the message.
 * @param cp      The V.92 CP message to fill in.
 */
void setV92CPpckFromParamsInfo(V90MappingParams *params,
			       tagV90AdditionalCPinfo *info, V92CP *cp);

/**
 * @brief Print the six spectral-shaper fields, six `edprintf` lines.
 *
 * Not gated: the level test is inside `edprintf`, so at level 0 the calls
 * still happen and print nothing.
 *
 * @param params  The mapping-parameters block to print from.
 */
void displaySpectralParams(V90MappingParams *params);

}

#endif /* DSPLIB_V90MAPPINGPARAMS_H */
