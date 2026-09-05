/**
 * @file V90TRN2Designer.h
 * @brief The V.90 TRN2 constellation designer: lays out the six per-phase
 *        transmit constellations used during TRN2 training, sized to fit
 *        under a power ceiling derived from the negotiated line power.
 *
 * Reconstructed from dsplibs.o. Of the class's six members, one pair is
 * reconstructed here: the constructor (0x3c9b0, 18 bytes) and the destructor
 * (0x3c9f0, one byte).
 *
 * Not polymorphic: the destructor appears with the `D1` and `D2` variants and
 * no `D0`, and GCC emits a deleting destructor only for a virtual one, so
 * offset 0 is a real member and there is no vptr. Finding F228 is the four
 * classes where that is not true.
 *
 * The constructor is the whole object map this file claims. Eighteen bytes,
 * two stores, no call and no branch:
 *
 *     mov 0x4(%esp),%eax      ; this
 *     mov 0x8(%esp),%ecx      ; argument 1
 *     mov 0xc(%esp),%edx      ; argument 2
 *     mov %ecx,(%eax)
 *     mov %edx,0x4(%eax)
 *
 * and the mangling is what types the two slots:
 * `_ZN15V90TRN2DesignerC1EP13V90ParametersP21V90ConstellationPower` gives
 * argument 1 as `V90Parameters *` and argument 2 as `V90ConstellationPower *`.
 * Neither is read here, so the pointer types come from the name and not from
 * a dereference -- the strongest evidence available for a constructor that
 * only stores.
 *
 * +0x00 is corroborated by two of the members not written here, worth saying
 * because it is the one slot something else touches: `setNofUcodesInTrn2`
 * does `mov 0x4(%esp),%ecx; mov (%ecx),%edx` and then reads +0x80 and writes
 * +0x78 through it, and `setTrn2DummyConstel` does `mov 0x0(%ebp),%ecx; cmp
 * 0x78(%ecx),%eax` with %ebp holding `this`. Both dereference +0x00 as a
 * pointer, which is what makes it a pointer here rather than merely the
 * first four bytes of an unmodelled object.
 *
 * The size is a bound with its scope stated, not a measurement of the class.
 * Four of the six members were read for this file -- the constructor, the
 * destructor, `setNofUcodesInTrn2` (0x18 bytes) and `setTrn2DummyConstel`
 * (0x59) -- plus `maxK` (0xcc), which turns out to touch `this` not at all
 * and works entirely through its `V90MappingParams *` argument. Between them
 * the largest `this`-relative displacement is +0x04 and it is four bytes
 * wide, so those five reach no further than 0x08. `V90TRN2Design` is 3,767
 * bytes and was not read, so the real object may be larger and this header
 * does not claim otherwise. What makes that safe rather than sloppy is the
 * test: `test/unit/t_v90designers.cpp` seeds a slot far larger than 8 bytes
 * and compares all of it, so a constructor store past +0x07 would fail
 * against the blob rather than pass unnoticed.
 */

#ifndef DSPLIB_V90TRN2DESIGNER_H
#define DSPLIB_V90TRN2DESIGNER_H

/*
 * POINTERS ONLY, so forward declarations belong here.  Two different
 * definitions of `V90Parameters` exist in this tree -- the 0x504 word block in
 * `V90PreFilter.h` and the 0x558 named map in `V90Parameters.h` -- and no
 * translation unit may include both, so declaring rather than including keeps
 * this header compatible with either (finding F1112).
 */
class V90Parameters;
class V90ConstellationPower;
class V90MappingParams;

/*
 * `PcmType` and `V90SpecialSpectralConditions` are ENUMS, so they cannot be
 * forward-declared in C++98 (docs/method/compilers.md) and the two headers
 * that define them are included.  Neither defines `V90Parameters`, so this
 * header stays compatible with either of that type's two definitions --
 * which is the whole reason the pointers above are declarations.
 */
#include "dsplib/V90Phase3Modulator.h"
#include "dsplib/V90SpectralConditions.h"

class V90TRN2Designer {
public:
	/**
	 * @brief Construct the designer, recording the parameter block and
	 * the power table it will design against.
	 * @param params  The V.90 parameter block (source of `nofUcodesInTrn2`
	 *                and the spectral-shaper coefficients).
	 * @param power   The constellation-power helper used to convert a
	 *                designed constellation into a transmit power figure.
	 */
	V90TRN2Designer(V90Parameters *params, V90ConstellationPower *power);

	/**
	 * @brief Destructor. One byte (`ret`) in the object -- declared only
	 * because the original class declared it; it touches no state.
	 */
	~V90TRN2Designer();

	/**
	 * @brief Adopt the configured TRN2 length as the working one.
	 *
	 * @p on is a flag, not a length: when non-zero, copies
	 * `params->unnamed_080` (the configured value) into
	 * `params->nofUcodesInTrn2` (the working value); when zero, does
	 * nothing.
	 * @param on  Non-zero to adopt the configured length.
	 */
	void setNofUcodesInTrn2(short on);

	/**
	 * @brief Fill all six constellations with a placeholder ramp.
	 *
	 * Used as the fallback when V90TRN2Design() cannot design a real
	 * constellation for some phase: fills `mappingParams->constellation`
	 * and `->codecConstellation` with a descending run starting at 78,
	 * `params->nofUcodesInTrn2` entries long. Not bounded against the
	 * 128-entry row length.
	 * @param mappingParams  The mapping-parameter block to fill.
	 */
	void setTrn2DummyConstel(V90MappingParams *mappingParams);

	/**
	 * @brief Compute how many bits one frame of the six constellations
	 * carries.
	 *
	 * log2 of the product of the six constellation lengths, truncated
	 * towards zero with a small epsilon guard against an exact power of
	 * two rounding low. Touches nothing through `this` -- works entirely
	 * off @p mappingParams.
	 * @param mappingParams  Supplies the six `constellationSize` entries.
	 * @return The bit count.
	 */
	int maxK(V90MappingParams *mappingParams);

	/**
	 * @brief Design all six TRN2 constellations.
	 *
	 * Derives a maximum transmit level from the power ladder (indexed by
	 * @p maxTxIndex) and @p params's configured `maxUcode` floor, then
	 * for each of the six phases walks the phase's codeword table down to
	 * fit under that level and builds `mappingParams->constellation`/
	 * `->codecConstellation` from it. On success also computes the
	 * resulting transmit power via @p power. If any phase cannot be
	 * designed, the whole mapping block is left holding
	 * setTrn2DummyConstel()'s placeholder ramp rather than a half-design.
	 * @param mappingParams   Mapping-parameter block to fill.
	 * @param ucode           Per-phase codeword table.
	 * @param alt             Per-phase alternate codeword table.
	 * @param allow           Per-phase per-codeword allow mask.
	 * @param dmin            Per-phase minimum-distance flag/seed.
	 * @param codecPcmType    Companding law used by the far-end codec.
	 * @param pcmType         Companding law used by this end.
	 * @param unused          Never read by the object; present only for
	 *                        calling-convention/ABI compatibility.
	 * @param topUcode        Per-phase starting codeword index.
	 * @param maxLookahead    Upper bound applied to the spectral-shaper id.
	 * @param maxTxIndex      Index into the average-power-limit ladder.
	 * @param cond            Selects the German-PBX spectral-shaper
	 *                        coefficients when applicable.
	 * @return 1 if every phase was designed, 0 if one could not be (the
	 *         caller tests the low 16 bits of the result).
	 */
	short V90TRN2Design(V90MappingParams *mappingParams,
			    short (*ucode)[128], short (*alt)[128],
			    unsigned char (*allow)[128], short *dmin,
			    PcmType codecPcmType, PcmType pcmType,
			    short unused, unsigned char *topUcode,
			    unsigned int maxLookahead,
			    unsigned char maxTxIndex,
			    V90SpecialSpectralConditions cond);

	/*
	 * Public for `offsetof`; the original's access specifiers are not
	 * recoverable, and one access section is what keeps `offsetof`
	 * meaningful.  See V90ConstellationDesigner.h.
	 */

	V90Parameters *params;		/* +0x00 = constructor argument 1 */
	V90ConstellationPower *power;	/* +0x04 = constructor argument 2 */
};

#endif /* DSPLIB_V90TRN2DESIGNER_H */
