/*
 * V92ParamsInfo.h -- the 180-byte block `V92Modem` hangs off +0xaa0: the V.92
 * upstream mapping parameters, plus the four free functions that allocate and
 * release the arrays inside it.
 *
 * WHAT THE BLOCK IS, AND WHY IT IS NAMED FROM A SYMBOL RATHER THAN GUESSED.
 * `V92Modem`'s constructor allocates it with a bare `sysdep_malloc(0xb4)` at
 * .text+0x13def and calls NO constructor on the result, so it is a C struct
 * and not a class; the destructor at .text+0x139d4 frees it with a bare
 * `sysdep_free`.  Its name comes from the only other function in the object
 * that fills it, `V92setParamsInfoFromCPUnPck` (.text+0x12f00), and the
 * identification is not adjacency -- it is the COMPLEMENT of the store sets:
 *
 *     V92setParamsInfoFromCPUnPck writes  +0x00..+0x58, +0x6c..+0x80, +0x9c..
 *     V92createFilterCoefficients writes  +0x5c..+0x68
 *     V92createConstellations     writes  +0x84..+0x98
 *
 * The unpacker writes every offset in the block except the ten that hold the
 * two arrays somebody else owns, which is what one would expect of a function
 * that refreshes the scalars of a block whose storage is allocated once, and
 * would be a coincidence otherwise.  With the unpacker written, everything
 * below +0xb4 is now accounted for; the earlier revision of this header said
 * +0x9c..+0xb3 was reached by nothing, and finding 3540 is what it turned out
 * to be.
 *
 * IT IS THE SAME BLOCK AS `V92MappingParams`, WHICH IS THE AUTHOR'S OWN NAME
 * FOR IT.  Three methods carry it in their mangling --
 * `V92Transmitter::reset(V92MappingParams *)`, `V92Precoder::reset` and
 * `V92ModulusEncoder::reset` -- and `V92Phase4Modulator::setMappingParams`
 * takes it too.  `V92ParamsInfo` is this tree's name, invented from the
 * unpacker's; `V92MappingParams` is the object's.  Both survive because the
 * C++ side needs a class of that exact spelling for the manglings to come out
 * right and the C side needs a struct it can dereference, so the class stays
 * an opaque forward declaration and every method that dereferences it casts.
 * `V92Precoder.cpp` and `V92ModulusEncoder.cpp` already did that before this
 * header had any field names; `V92Transmitter.cpp` now does it too.
 *
 * That the two are one block is FIVE independent offset/name agreements and
 * not an inference from size: `V92Transmitter::reset` prints +0x4c as "lz1",
 * +0x50 as "lp1", +0x54 as "lz2", +0x58 as "lp2" and +0x14 as "extendEu",
 * and the unpacker fills those five offsets from `CPObj->lz1`, `CPObj->lp1`,
 * `CPObj->lz2`, `CPObj->lp2` and `CPObj->extendEu`.
 *
 * WHERE THE NAMES BELOW COME FROM.  Almost all of them are the author's own
 * words, taken from the format strings the two functions print at
 * `dsplibs_debug_level > 1` and resolved with `tools/relocscan.py --at
 * .rodata.str1.1:0xNNNN` (finding 604).  Which string proves which field is
 * recorded field by field.  Four fields -- `K`, `modulosEncoderPresent`,
 * `prefilterPrecoderPresent` and `constellationPresent` -- are named at ONE
 * REMOVE: the string names the field of the *source* block the unpacker
 * copies them from, verbatim and at the same width, and in the three
 * `*Present` cases the destination copy is then what gates exactly the part
 * of the unpack its name describes.  That is said again at each of them.
 *
 * THE TRANSLATION UNIT IS AMBIGUOUS.  `tools/tuattrib.py` brackets
 * .text+0x12d10..+0x12f00 as `V92Jd.cpp|V92Modem.cpp` -- a contiguity fill
 * between two confident neighbours, so the object does not say which.  The
 * five names are unmangled, so whichever .cpp it was they were `extern "C"`,
 * and a C file is the simpler reconstruction of the same thing.
 */

#ifndef DSPLIB_V92PARAMSINFO_H
#define DSPLIB_V92PARAMSINFO_H

/*
 * The two array counts and the two element sizes, all four read straight off
 * the immediates: six `movl $0x200` and four `movl $0x600`.
 */
#define V92_PARAMSINFO_CONSTELLATIONS	6
#define V92_PARAMSINFO_CONSTELLATION_SZ	0x200
#define V92_PARAMSINFO_FILTERCOEFS	4
#define V92_PARAMSINFO_FILTERCOEF_SZ	0x600

/*
 * The two ceilings `V92setParamsInfoFromCPUnPck` clamps to, spelled once per
 * clamp site in the object: four `cmp $0x148` over the filter lengths and six
 * `cmp $0x80` over the constellation sizes.  Both tests are `jbe`, which is
 * why the fields they guard are unsigned -- see `lz1` and `LC`.
 *
 * Each ceiling is exactly the storage behind it.  0x148 = 328 coefficients is
 * within the 0x600 bytes = 384 floats `V92createFilterCoefficients` allocates,
 * and 0x80 = 128 entries is EXACTLY the 0x200 bytes = 128 ints
 * `V92createConstellations` allocates.  The second is what settles the
 * constellation element type below.
 */
#define V92_PARAMSINFO_MAX_FILTER_LEN	0x148
#define V92_PARAMSINFO_MAX_LC		0x80

struct V92ParamsInfo {
	/*
	 * +0x00  `V92Transmitter::reset` copies this to its own +0x04 and
	 * prints THAT as "K = %d" (.rodata.str1.1:0x26b6), so the name is the
	 * author's for the copy rather than for this slot; the copy is a bare
	 * `mov`, so they are the same quantity.  The unpacker forms it as
	 * `2 * (CPObj->drn + 17)` -- `lea 0x22(%edi,%edi,1)` -- and prints
	 * `(CPObj->drn + 17) * 1333.3333740234375` as "Upstream rate = %d" in
	 * the same breath.  Both are measured; nothing here says what units K
	 * counts in and this file does not guess.
	 */
	int K;

	/*
	 * +0x04  Copied verbatim from the field the unpacker prints as
	 * "CPObj->modulosEncoderPresent = %d" (.rodata.str1.4:0x32ac), and the
	 * copy is then tested to decide whether the twelve moduli below are
	 * filled at all -- `mov 0x4(%esi),%ebx; test %ebx,%ebx; je` skips
	 * exactly the m[] copy.  The name is the source field's; what
	 * corroborates it here is that this copy gates what it says it gates.
	 * The author's spelling of "modulus" is his own and is kept.
	 */
	int modulosEncoderPresent;

	/*
	 * +0x08  "CPObj->prefilterPrecoderPresent = %d"
	 * (.rodata.str1.4:0x32d0), same one-remove reading; this copy gates
	 * the four filter lengths and the four coefficient arrays.
	 */
	int prefilterPrecoderPresent;

	/*
	 * +0x0c  "CPObj->constellationPresent = %d" (.rodata.str1.4:0x32f8);
	 * this copy gates the six constellation sizes, the six index words and
	 * the six constellation copies.
	 */
	int constellationPresent;

	/*
	 * +0x10  "trellisType = %d" (.rodata.str1.1:0x26bf), printed by
	 * `V92Transmitter::reset` off THIS block.  `int` is forced rather than
	 * chosen: reset hands it to `V92ConvolutionEncoder::reset(int)`, whose
	 * mangling ends `Ei`.  The unpacker fills it by sign-extending a
	 * one-byte source field it prints as "CPObj->trellisState".
	 */
	int trellisType;

	/* +0x14  "extendEu = %d" (.rodata.str1.1:0x26d2), off this block. */
	int extendEu;

	/*
	 * +0x18  "Gain = %c%d.%07d" (.rodata.str1.1:0x26a3) -- printed by
	 * `V92Transmitter::reset` from its own +0x44, which is a bare copy of
	 * this.  The unpacker calls the same quantity "V92Modulator:
	 * constellation gain" while it builds it, twice over: once "(before Lu
	 * multiplication)" and once "(after)".
	 */
	float gain;

	/*
	 * +0x1c .. +0x48  The twelve moduli.  `V92Transmitter::reset` prints
	 * them one at a time as "m0 = %d" .. "m11 = %d"
	 * (.rodata.str1.1:0x26e2 .. 0x2751), so the twelve NAMES are the
	 * author's; the array is this tree's modelling and not a claim.
	 * Nothing in the object indexes these twelve at run time -- every
	 * access in the unpacker, in reset and in `V92Precoder::reset` is a
	 * constant displacement -- and twelve statements over an array give
	 * exactly that, so the object cannot tell an array from twelve fields
	 * here.  The array is kept because `V92Precoder::reset` reads them
	 * with a loop and did so before this header had names.
	 *
	 * The SOURCE block's twelve are an array and that IS forced: the
	 * unpacker prints them from a loop, "CPObj->M[%d] = %d".
	 */
	int m[12];

	/*
	 * +0x4c .. +0x58  The four filter lengths, in the order the author
	 * prints them: "lz1" (.rodata.str1.1:0x275c), "lp1" (0x2767), "lz2"
	 * (0x268d), "lp2" (0x2698), all four off this block in
	 * `V92Transmitter::reset`.
	 *
	 * UNSIGNED IS FORCED TWICE.  `V92Transmitter::reset` passes lz1 and
	 * lp1 to `V92Precoder::setCoefficients(float *, float *, unsigned,
	 * unsigned)` -- mangling `EPfS0_jj` -- and lz2 and lp2 to
	 * `V92PreFilter::setCoefficients` with the same signature; and the
	 * unpacker's clamp against V92_PARAMSINFO_MAX_FILTER_LEN is `jbe`,
	 * not `jle`.
	 */
	unsigned int lz1;
	unsigned int lp1;
	unsigned int lz2;
	unsigned int lp2;

	/*
	 * +0x5c .. +0x68  The four coefficient arrays, 0x600 bytes each.  The
	 * names are the author's: `V92Transmitter::reset` dumps them as
	 * "z1[%d] = %c%d.%07d" (.rodata.str1.1:0x261b), "p1[%d]" (0x2630),
	 * "z2[%d]" (0x2645) and "p2[%d]" (0x265a), each loop bounded by the
	 * length of the same name.
	 *
	 * `float *` IS FORCED, and by two independent readings: reset passes
	 * z1 and p1 to `V92Precoder::setCoefficients(float *, float *, ...)`
	 * and z2 and p2 to `V92PreFilter::setCoefficients`, both `EPfS0_jj`;
	 * and the unpacker fills all four with `fstps (%ecx,%edx,4)`.  The
	 * earlier revision of this header declared them `void *` and said so;
	 * this is the reader it was waiting for.
	 *
	 * They were `filterCoefficients[4]` until the names were found.  Four
	 * fields rather than an array because the four names are four names --
	 * and because nothing in the object indexes them, here or in the two
	 * creators.
	 */
	float *z1;
	float *p1;
	float *z2;
	float *p2;

	/*
	 * +0x6c .. +0x80  The six constellation sizes.  "CPObj->LC[%d] = %d"
	 * (.rodata.str1.1:0x8d0) names the source array the unpacker copies
	 * them from, and the six banners it prints over the six constellation
	 * dumps -- "======== Constellation LC 1 ============="
	 * (.rodata.str1.4:0x331c and the five after it) -- are what tie LC[n]
	 * to constellation n.  An array here because the unpacker's own print
	 * loop indexes the source, and because `V92Precoder::reset` reads
	 * these six with a loop.
	 *
	 * UNSIGNED IS FORCED: the clamp against V92_PARAMSINFO_MAX_LC is
	 * `cmpl $0x80,0x70(%esi); jbe`, and every constellation copy loop is
	 * bounded `cmp %edx,0x6c(%esi); ja`.
	 */
	unsigned int LC[V92_PARAMSINFO_CONSTELLATIONS];

	/*
	 * +0x84 .. +0x98  The six constellations, 0x200 bytes each.
	 *
	 * `int *` rather than the `void *` this header carried before, on the
	 * strength of the format string: the unpacker dumps the SOURCE arrays
	 * it copies from as "\tconst1[%d] = %d" (.rodata.str1.1:0xa1b) through
	 * "const6" (0x9bc), pushing four bytes per element for a `%d`.  A
	 * float would have been promoted to double and pushed as eight.  The
	 * copy itself is `mov (%ebx,%edx,4); mov ..,(%ecx,%edx,4)` -- four
	 * bytes, and integer registers -- and 0x200 bytes is exactly the
	 * V92_PARAMSINFO_MAX_LC = 128 entries the sizes above are clamped to.
	 *
	 * The six have no names of their own in this block: const1..const6 are
	 * the SOURCE arrays' names, and this array's identity comes from the
	 * six allocations rather than from any string.
	 */
	int *constellations[V92_PARAMSINFO_CONSTELLATIONS];

	/*
	 * +0x9c .. +0xb0  "CPObj->indexConstel[%d] = %d"
	 * (.rodata.str1.4:0x328c) names the source array; the unpacker copies
	 * all six here unconditionally with a real indexed loop --
	 * `mov 0xc70(%ebp,%edx,4),%eax; mov %eax,0x9c(%esi,%edx,4)` -- which
	 * is what forces an array rather than six fields at this one site.
	 *
	 * `V92Precoder::reset` takes a pointer to this array and keeps it
	 * (`paramsAt9c`), and the header there already read the region as six
	 * words; it called them "the eighteen bytes", which is 0x18 written as
	 * decimal.  Twenty-four bytes, six words.
	 */
	int indexConstel[V92_PARAMSINFO_CONSTELLATIONS];
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All four take the block and return nothing.  `V92Modem`'s constructor calls
 * the two creators back to back at .text+0x13e04 and +0x13e12 and ignores
 * what comes back in %eax; its destructor calls the two deleters at
 * .text+0x139c1 and +0x139cf and then frees the block itself.
 */
void V92createConstellations(struct V92ParamsInfo *p);
void V92createFilterCoefficients(struct V92ParamsInfo *p);

/*
 * BOTH DELETERS LEAVE THE POINTERS DANGLING.  Each slot is tested, freed if
 * non-null, and NOT written back -- there is no store to any of the ten
 * offsets anywhere in the two functions.  That is the object's behaviour and
 * it is reproduced rather than repaired; docs/deviations.md D170.
 */
void V92deleteConstellations(struct V92ParamsInfo *p);
void V92deleteFilterCoefficients(struct V92ParamsInfo *p);

/*
 * .text+0x12f00, 2,695 bytes.  Refill the block from an unpacked CP message.
 * `VPcmFloModem::runPcmModem` is the only caller and reaches it twice.
 */
struct V92CPUnPck;
void V92setParamsInfoFromCPUnPck(struct V92ParamsInfo *p,
				 struct V92CPUnPck *cp);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V92PARAMSINFO_H */
