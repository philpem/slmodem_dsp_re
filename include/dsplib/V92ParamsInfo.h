/**
 * @file V92ParamsInfo.h
 * @brief The 180-byte V.92 upstream mapping-parameters block `V92Modem` hangs
 *        off +0xaa0, plus the four free functions that allocate, release and
 *        refill the arrays inside it.
 *
 * A plain C struct, not a class: `V92Modem`'s constructor allocates it with
 * a bare `sysdep_malloc(0xb4)` and calls no constructor on the result, and
 * the destructor frees it with a bare `sysdep_free`. Its identity is the
 * complement of three functions' store sets -- `V92setParamsInfoFromCPUnPck`
 * writes every offset except the ten bytes of the two arrays
 * `V92createFilterCoefficients` and `V92createConstellations` own -- which is
 * what a refill function looks like against a block allocated once
 * (finding F3600).
 *
 * **This is the same block as `V92MappingParams`**, the author's own name
 * for it (it appears in three methods' manglings, including
 * `V92Transmitter::reset(V92MappingParams *)`). `V92ParamsInfo` is this
 * tree's name, invented from the unpacker's; both survive because the C++
 * side needs a class of that exact spelling for the manglings and the C side
 * needs a struct it can dereference, so the class stays an opaque forward
 * declaration and every method that touches it casts. The identification
 * rests on five independent offset/name agreements between
 * `V92Transmitter::reset`'s diagnostics and the unpacker's source-field
 * names, not on size (findings F1321/F3601).
 *
 * Field names below are almost all the author's own words, taken from the
 * two functions' debug-level-2 format strings and resolved with
 * `tools/relocscan.py` (finding F604); which string proves which field is
 * recorded per field. Four fields (`K` and the three `*Present` flags) are
 * named at one remove: the string names the field of the *source* block the
 * unpacker copies from, verbatim and at the same width -- and for the
 * `*Present` fields, the copy then gates exactly the part of the unpack its
 * name describes.
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
	/* +0x00  Named one remove: `V92Transmitter::reset` copies this
	 * verbatim to its own field and prints THAT as "K = %d". The
	 * unpacker forms it as `2 * (CPObj->drn + 17)`; nothing here says
	 * what units K counts in (finding F604). */
	int K;

	/* +0x04  Copied verbatim from the source field the unpacker prints as
	 * "CPObj->modulosEncoderPresent = %d", and the copy is then tested to
	 * gate whether the twelve moduli below get filled at all
	 * (finding F604). */
	int modulosEncoderPresent;

	/* +0x08  "CPObj->prefilterPrecoderPresent = %d"; this copy gates the
	 * four filter lengths and the four coefficient arrays (F604). */
	int prefilterPrecoderPresent;

	/* +0x0c  "CPObj->constellationPresent = %d"; this copy gates the six
	 * constellation sizes, index words and constellation copies (F604). */
	int constellationPresent;

	/*
	 * +0x10  "trellisType = %d", printed by `V92Transmitter::reset` off
	 * this block (finding F604). `int` is forced: reset hands it to
	 * `V92ConvolutionEncoder::reset(int)` (mangling `Ei`). The unpacker
	 * fills it by sign-extending a one-byte source field.
	 */
	int trellisType;

	/* +0x14  "extendEu = %d", off this block (finding F604). */
	int extendEu;

	/* +0x18  "Gain = %c%d.%07d", printed by `V92Transmitter::reset` from
	 * a bare copy of this; the unpacker calls the same quantity
	 * "V92Modulator: constellation gain" while building it (F604). */
	float gain;

	/*
	 * +0x1c .. +0x48  The twelve moduli, named "m0".."m11" by
	 * `V92Transmitter::reset`'s own diagnostics (finding F604). Kept as
	 * an array because `V92Precoder::reset` reads them with a loop and
	 * the source block's twelve are an array too (`CPObj->M[%d]`),
	 * though nothing here indexes them at run time -- every access is a
	 * constant displacement.
	 */
	int m[12];

	/*
	 * +0x4c .. +0x58  The four filter lengths, named "lz1"/"lp1"/"lz2"/
	 * "lp2" by `V92Transmitter::reset`'s diagnostics (finding F604).
	 * Unsigned is forced twice: `reset` passes lz1/lp1 to
	 * `V92Precoder::setCoefficients(float*,float*,unsigned,unsigned)`
	 * and lz2/lp2 to `V92PreFilter::setCoefficients` with the same
	 * mangled signature, and the unpacker's own clamp against
	 * #V92_PARAMSINFO_MAX_FILTER_LEN is the unsigned `jbe`.
	 */
	unsigned int lz1;
	unsigned int lp1;
	unsigned int lz2;
	unsigned int lp2;

	/*
	 * +0x5c .. +0x68  The four coefficient arrays, 0x600 bytes each,
	 * named "z1"/"p1"/"z2"/"p2" by `V92Transmitter::reset`'s dump loops
	 * (finding F604). `float *` is forced by the same setCoefficients
	 * manglings as lz1..lp2 above, and by the unpacker filling all four
	 * with `fstps`. Four fields rather than an array: the four names are
	 * four names, and nothing indexes them.
	 */
	float *z1;
	float *p1;
	float *z2;
	float *p2;

	/*
	 * +0x6c .. +0x80  The six constellation sizes. "CPObj->LC[%d] = %d"
	 * names the source array the unpacker copies from (finding F604); an
	 * array here because the unpacker's own print loop indexes the
	 * source and `V92Precoder::reset` reads these six with a loop.
	 * Unsigned is forced: the clamp against #V92_PARAMSINFO_MAX_LC and
	 * every constellation-copy loop bound are both `jbe`/`ja`.
	 */
	unsigned int LC[V92_PARAMSINFO_CONSTELLATIONS];

	/*
	 * +0x84 .. +0x98  The six constellations, 0x200 bytes each. `int *`
	 * rather than `void *`: the unpacker dumps the source arrays it
	 * copies from as "const1[%d] = %d" .. "const6[%d]" with a `%d`
	 * (finding F604) -- a `float` would print as `%f`/promote to
	 * `double` -- and the copy loop itself uses integer registers.
	 * 0x200 bytes is exactly #V92_PARAMSINFO_MAX_LC entries. The six
	 * have no names of their own in this block; their identity comes
	 * from the six allocations, not from a string.
	 */
	int *constellations[V92_PARAMSINFO_CONSTELLATIONS];

	/*
	 * +0x9c .. +0xb0  "CPObj->indexConstel[%d] = %d" names the source
	 * array (finding F604); the unpacker copies all six here
	 * unconditionally through a real indexed loop, which is what forces
	 * an array rather than six fields. `V92Precoder::reset` takes a
	 * pointer to this array and keeps it (finding F1372: each of the six
	 * is a selector picking both a constellation and a modulus).
	 */
	int indexConstel[V92_PARAMSINFO_CONSTELLATIONS];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate and fill the six `constellations[]` arrays. Called once
 *        from `V92Modem`'s constructor; its return value is ignored.
 * @param p  The mapping-parameters block.
 */
void V92createConstellations(struct V92ParamsInfo *p);
/**
 * @brief Allocate and fill the four `z1`/`p1`/`z2`/`p2` coefficient arrays.
 *        Called once from `V92Modem`'s constructor; its return value is
 *        ignored.
 * @param p  The mapping-parameters block.
 */
void V92createFilterCoefficients(struct V92ParamsInfo *p);

/**
 * @brief Free the six `constellations[]` arrays. Each slot is null-guarded
 *        but left dangling afterwards -- no offset is written back
 *        (reproduced from the object, docs/deviations.md D170).
 * @param p  The mapping-parameters block.
 */
void V92deleteConstellations(struct V92ParamsInfo *p);
/**
 * @brief Free the four coefficient arrays, with the same dangling-pointer
 *        behavior as V92deleteConstellations() (docs/deviations.md D170).
 * @param p  The mapping-parameters block.
 */
void V92deleteFilterCoefficients(struct V92ParamsInfo *p);

struct V92CPUnPck;
/**
 * @brief Refill the block's scalars and arrays from an unpacked CP message.
 *        `VPcmFloModem::runPcmModem` is the only caller, reaching it twice.
 * @param p   The mapping-parameters block to refill.
 * @param cp  The unpacked CP message to refill it from.
 */
void V92setParamsInfoFromCPUnPck(struct V92ParamsInfo *p,
				 struct V92CPUnPck *cp);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V92PARAMSINFO_H */
