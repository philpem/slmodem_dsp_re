/**
 * @file V90ConstellationPower.h
 * @brief `V90ConstellationPower`, the V.90 downstream constellation power
 *        model: given a frame's mapping parameters, works out the mean
 *        square companded level a whole six-symbol frame carries.
 *
 * The class owns ten symbols in the object and all ten are written: the
 * constructor and destructor in both ABI variants, five members, and the
 * static data member `averagePowerLimits`.
 *
 * The constructor and destructor are declared but empty (four one-byte
 * symbols in the object, `C1`/`C2`/`D1`/`D2`), because GCC emits an
 * out-of-line constructor or destructor symbol only for a user-declared
 * one -- an implicit trivial destructor produces no symbol at all, so the
 * four bytes are themselves the evidence that the original declared both
 * and left both bodies empty. `test/unit/t_v90designers.cpp` runs all four
 * against the blob and asserts that they write nothing, so "does nothing"
 * is measured rather than assumed. Not polymorphic: the destructor appears
 * with the `D1`/`D2` variants and no `D0`, and GCC emits a deleting
 * destructor only for a virtual one, so offset 0 is a real member and there
 * is no vptr.
 *
 * The object is 144 bytes (0x90) and every byte of it is now a field:
 * `calcModulusParameters` writes all of +0x08..+0x5f and the six doubles
 * from +0x60, and `getPower` indexes the same three regions. The last
 * double is eight bytes wide at +0x88, so the object ends at 0x90 -- a
 * displacement is not a size (finding F215); the width of what sits at the
 * bound is what turns one into the other.
 *
 * `getPower()` is the whole story and the field names come from it.
 * `calcModulusParameters()` sets up a mixed-radix odometer over the six
 * V.90 constellations -- `codewordCount` codewords spread over six symbols
 * whose radices are `V90MappingParams::constellationSize[0..5]` -- and
 * `getPower()` then walks each constellation, works out what fraction of
 * the codewords put each of its points in that symbol position, and
 * accumulates the mean square of the companded level. The tail multiplies
 * by 1/6, the six symbols of a V.90 frame.
 *
 * `averagePowerLimits` is 35 unsigned words in `.data` at 0xb60. It is
 * `.data` and not `.rodata`, so the original did not spell it `const`, and
 * it is read only by `getPowerIndexForPower()`, whose
 * `xor %edx,%edx; push %edx; push %eax; fildll` is the unsigned-to-double
 * idiom -- a signed `int` converts with a bare `fildl` and no push pair.
 * See the .cpp for what the values are.
 */

#ifndef DSPLIB_V90CONSTELLATIONPOWER_H
#define DSPLIB_V90CONSTELLATIONPOWER_H

/*
 * For `PcmType`, which `getPower`'s mangling names (`7PcmType`) and which
 * therefore cannot be spelled any other way here.  Included, never edited --
 * the same arrangement `V90DilDescriptorSettings.h` makes for the same enum.
 */
#include "dsplib/V90Phase3Modulator.h"

class V90MappingParams;

/*
 * The six constellations, and the highest legal power index -- both bounds
 * are the object's own loop and switch limits (finding F3050).
 */
#define V90CP_CONSTELLATIONS	6
#define V90CP_POWER_INDICES	35

/*
 * Which of `V90MappingParams`' two byte tables a power figure is taken
 * over.  The object only ever tests this value for equality against 1
 * (finding F3053), selecting `codecConstellation`; every other value takes
 * `constellation`.  Nothing in the object names a second enumerator or
 * fixes what the two points ARE, so the names below say which table is read
 * and no more.
 *
 * The base pin is ours: a two-valued enum has the range 0..1, which entitles
 * the compiler to narrow `== 1` to `!= 0` and would make the object's
 * `cmpl $0x1` unreproducible and untestable, so a wide negative enumerator
 * makes the base signed and every `int` representable (same argument as
 * `V90CodecType.h`).  `test/unit/t_v90cpower.cpp` drives the value 2 for
 * exactly this reason.
 */
enum V90TxPowerMeasurementPoint {
	V90_TX_POWER_OVER_CONSTELLATION = 0,
	V90_TX_POWER_OVER_CODEC_CONSTELLATION = 1,
	V90_TX_POWER_MEASUREMENT_POINT_BASE_PIN = -0x7fffffff - 1
};

class V90ConstellationPower {
public:
	/**
	 * @brief Construct a power model. Initialises nothing.
	 *
	 * The object has four blob symbols for this class -- `C1`, `C2`, `D1`,
	 * `D2`, one byte each, `ret` and nothing else -- which is what a
	 * user-declared but empty constructor/destructor pair compiles to; an
	 * implicit trivial one emits no symbol at all. So the empty bodies here
	 * are not a placeholder, they are what the original wrote.
	 * `test/unit/t_v90designers.cpp` asserts against the blob that all four
	 * write nothing.
	 */
	V90ConstellationPower();

	/** @brief Destroy a power model. Does nothing (see the constructor). */
	~V90ConstellationPower();

	/*
	 * Public for `offsetof`, and because the original's access specifiers
	 * are not recoverable from the mangling.  See V90ConstellationDesigner.h.
	 */

	/**
	 * @brief Lay out the mixed-radix codeword odometer for one mapping.
	 *
	 * Writes every field below except `constellation` and
	 * `constellationSize`, and reads nothing of its own. See finding F3050
	 * for the full derivation of the odometer this sets up.
	 *
	 * @param mappingParams  The V.90 mapping whose constellation sizes and
	 *                       shaper/word bit counts drive the layout.
	 */
	void calcModulusParameters(V90MappingParams *mappingParams);

	/**
	 * @brief Point `constellation` and `constellationSize` at one of
	 *        `mappingParams`' twelve byte tables.
	 *
	 * A `void` return: no path assigns `%eax` before its `ret`. An `index`
	 * above 5 writes nothing -- the switch has no default arm and the
	 * function falls straight to its epilogue.
	 *
	 * @param mappingParams  The mapping whose tables are selected from.
	 * @param point          Which of the two measurement points to read;
	 *                       only `V90_TX_POWER_OVER_CODEC_CONSTELLATION`
	 *                       is distinguished (finding F3053).
	 * @param index          Which of the six constellations, 0..5.
	 */
	void getConstellationInfo(V90MappingParams *mappingParams,
				  V90TxPowerMeasurementPoint point,
				  unsigned int index);

	/**
	 * @brief Mean square companded level of a whole six-symbol frame.
	 *
	 * Calls calcModulusParameters() first and then getConstellationInfo()
	 * once per constellation, so it leaves the object fully written. See
	 * finding F3051 for the digit-frequency argument behind the per-point
	 * weighting.
	 *
	 * @param mappingParams  The mapping to measure.
	 * @param point          Which of the two measurement points to use.
	 * @param pcmType        mu-law or A-law, selecting the companding used
	 *                       to convert each constellation byte to a level.
	 * @return The frame's mean square power.
	 */
	float getPower(V90MappingParams *mappingParams,
		       V90TxPowerMeasurementPoint point, PcmType pcmType);

	/**
	 * @brief The largest ladder index whose limit still reaches `power`.
	 *
	 * @param power  The power to look up.
	 * @return An index into `averagePowerLimits`, saturating at 0; never a
	 *         failure code.
	 */
	unsigned int getPowerIndexForPower(float power);

	/**
	 * @brief Whether `index` is a valid `averagePowerLimits` subscript.
	 * @param index  The index to check.
	 * @return `index <= V90CP_POWER_INDICES - 1`.
	 */
	bool isLegalPowerIndex(unsigned int index);

	/* --- data members --- */

	/*
	 * +0x00 and +0x04  The constellation `getConstellationInfo` last
	 * selected, and its length.  The pointer is into the CALLER'S
	 * `V90MappingParams`, not owned here; both are unsigned (finding F3050).
	 */
	unsigned char *constellation;			/* +0x00 */
	unsigned int constellationSize;			/* +0x04 */

	/*
	 * +0x08  `1LL << (mappingParams->shaperSR + mappingParams->word_0 - 6)`,
	 * the number of distinct codewords a frame can carry.  Signed 64 bits,
	 * forced by the object's division and shift instructions (F3050).
	 */
	long long codewordCount;			/* +0x08 */

	/*
	 * +0x10  The odometer, seven entries ending at +0x47.  `remaining[0]`
	 * is `codewordCount - 1` and `remaining[i + 1]` is what is left after
	 * constellation `i` has taken its digit -- one array of seven, not a
	 * scalar and a separate array of six (finding F3050).
	 */
	long long remaining[V90CP_CONSTELLATIONS + 1];	/* +0x10 */

	/*
	 * +0x48  Six 32-bit unsigned digits (finding F3050).  `getPower`
	 * compares them against the symbol index, unsigned.
	 *
	 * The sixth is not a remainder: five of them are
	 * `remaining[i] % constellationSize[i]`, but `modulus[5]` is the plain
	 * truncation of `remaining[5]` to 32 bits, with no division at all.
	 * The two readings agree whenever `remaining[5] < constellationSize[5]`,
	 * the ordinary case, so the test drives shift counts large enough to
	 * separate them (finding F3052).
	 */
	unsigned int modulus[V90CP_CONSTELLATIONS];	/* +0x48 */

	/*
	 * +0x60  The odometer's place values as doubles: `placeValue[0]` is 1.0
	 * and `placeValue[i]` is the product of the first `i` constellation
	 * sizes.  The running product stays in a register across all six
	 * stores and is never reloaded, which is why the .cpp keeps a local
	 * rather than reading the member back (finding F3050).
	 */
	double placeValue[V90CP_CONSTELLATIONS];	/* +0x60 */

	/*
	 * The power ladder, 35 entries at `.data` 0xb60.  `getPowerIndexForPower`
	 * walks it downwards from 34 and `isLegalPowerIndex` bounds an index
	 * against the same 34.
	 */
	static unsigned int averagePowerLimits[V90CP_POWER_INDICES];
};

#endif /* DSPLIB_V90CONSTELLATIONPOWER_H */
