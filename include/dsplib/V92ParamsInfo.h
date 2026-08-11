/*
 * V92ParamsInfo.h -- the 180-byte block `V92Modem` hangs off +0xaa0, and the
 * four free functions that allocate and release the arrays inside it.
 *
 * WHAT THE BLOCK IS, AND WHY IT IS NAMED FROM A SYMBOL RATHER THAN GUESSED.
 * `V92Modem`'s constructor allocates it with a bare `sysdep_malloc(0xb4)` at
 * .text+0x13def and calls NO constructor on the result, so it is a C struct
 * and not a class; the destructor at .text+0x139d4 frees it with a bare
 * `sysdep_free`.  Its name comes from the only other function in the object
 * that fills it, `V92setParamsInfoFromCPUnPck` (.text+0x12f00), and the
 * identification is not adjacency -- it is the COMPLEMENT of the store sets:
 *
 *     V92setParamsInfoFromCPUnPck writes  +0x00..+0x58, +0x6c..+0x80
 *     V92createFilterCoefficients writes  +0x5c..+0x68
 *     V92createConstellations     writes  +0x84..+0x98
 *
 * The unpacker writes twenty-eight distinct offsets and skips EXACTLY the four
 * that hold filter-coefficient pointers, which is what one would expect of a
 * function that refreshes the scalars of a block whose arrays somebody else
 * owns, and would be a coincidence otherwise.  Everything below +0xb4 is
 * accounted for except +0x9c..+0xb3, which nothing in this batch reaches.
 *
 * WHAT THE ARRAYS ARE IS NOT KNOWN AND IS NOT GUESSED.  The four functions
 * that own them never dereference them: they store what `sysdep_malloc`
 * returned and nothing more.  So the element type is `void *` here, which is
 * the honest reading -- 0x200 bytes is 128 floats or 256 shorts and this
 * batch cannot tell them apart, and writing `float *` because "constellation"
 * sounds like floats would be putting a guess into the record.  Whoever
 * reconstructs the reader replaces the two array declarations.
 *
 * THE TRANSLATION UNIT IS AMBIGUOUS.  `tools/tuattrib.py` brackets
 * .text+0x12d10..+0x12f00 as `V92Jd.cpp|V92Modem.cpp` -- a contiguity fill
 * between two confident neighbours, so the object does not say which.  The
 * four names are unmangled, so whichever .cpp it was they were `extern "C"`,
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

struct V92ParamsInfo {
	/*
	 * +0x00..+0x5b.  Twenty-three slots the unpacker fills and this batch
	 * does not model; `V92setParamsInfoFromCPUnPck` is the reader.
	 */
	unsigned char pad_00[0x5c];

	/* +0x5c, +0x60, +0x64, +0x68 -- 0x600 bytes each. */
	void *filterCoefficients[V92_PARAMSINFO_FILTERCOEFS];

	/* +0x6c..+0x83.  Six more of the unpacker's slots. */
	unsigned char pad_6c[0x84 - 0x6c];

	/* +0x84, +0x88, +0x8c, +0x90, +0x94, +0x98 -- 0x200 bytes each. */
	void *constellations[V92_PARAMSINFO_CONSTELLATIONS];

	/* +0x9c..+0xb3.  Inside the 0xb4 allocation and reached by nothing. */
	unsigned char pad_9c[0xb4 - 0x9c];
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

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V92PARAMSINFO_H */
