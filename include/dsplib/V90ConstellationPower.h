/*
 * V90ConstellationPower.h -- the V.90 downstream constellation power model.
 *
 * Reconstructed from dsplibs.o.  The class owns ten symbols in the object and
 * all ten are written: the constructor and destructor in both ABI variants,
 * five members, and the static data member `averagePowerLimits`.
 *
 * WHY A CLASS WHOSE CONSTRUCTOR DOES NOTHING IS STILL DECLARED HERE.  The
 * blob has four symbols for it -- `C1`, `C2`, `D1`, `D2` at 0x3dd40, 0x3dd50,
 * 0x3dd60 and 0x3dd70, one byte each -- and GCC emits an out-of-line
 * constructor or destructor symbol only for a USER-DECLARED one.  An implicit
 * trivial destructor produces no symbol at all.  So the four bytes are
 * themselves the evidence that the original declared both and left both
 * bodies empty, and reproducing that means declaring them and leaving them
 * empty here.  `test/unit/t_v90designers.cpp` runs all four against the blob
 * and asserts that they write nothing, so "does nothing" is measured rather
 * than assumed.
 *
 * NOT POLYMORPHIC: the destructor appears with the `D1` and `D2` variants and
 * no `D0`, and GCC emits a deleting destructor only for a virtual one, so
 * offset 0 is a real member and there is no vptr.
 *
 * THE OBJECT IS 144 BYTES (0x90) and every byte of it is now a field.  The
 * measurement that fixed the size is still the one below, and the interior is
 * no longer `pad_`: `calcModulusParameters` writes all of +0x08..+0x5f and the
 * six doubles from +0x60, and `getPower` indexes the same three regions at
 * `0x48(%esi,%edi,4)`, `0x18(%esi,%edi,8)` and `0x60(%esi,%edi,8)`.  The last
 * double is eight bytes wide at +0x88, so the object ends at 0x90 -- a
 * displacement is not a size (finding F215); the width of what sits at the
 * bound is what turns one into the other.
 *
 * WHAT THE OBJECT IS FOR.  `getPower` is the whole story and the field names
 * come from it.  `calcModulusParameters` sets up a MIXED-RADIX odometer over
 * the six V.90 constellations -- `codewordCount` codewords spread over six
 * symbols whose radices are `V90MappingParams::constellationSize[0..5]` --
 * and `getPower` then walks each constellation, works out what FRACTION of
 * the codewords put each of its points in that symbol position, and
 * accumulates the mean square of the companded level.  The tail multiplies by
 * 1/6, which is the six symbols of a V.90 frame.
 *
 * THE STATIC DATA MEMBER IS 35 UNSIGNED WORDS in `.data` at 0xb60, 0x8c bytes.
 * It is `.data` and not `.rodata`, so the original did not spell it `const`,
 * and it is read only by `getPowerIndexForPower`, whose `xor %edx,%edx; push
 * %edx; push %eax; fildll` is the unsigned-to-double idiom -- a signed `int`
 * converts with a bare `fildl` and no push pair.  See the .cpp for what the
 * values are.
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
 * The six constellations, and the highest legal power index.  `getPower`'s
 * loop runs `cmp $0x5,%edi; jbe`, `getConstellationInfo`'s switch bounds at
 * `cmp $0x5,%eax; ja`, and `isLegalPowerIndex` is `cmpl $0x22,...; setbe`.
 */
#define V90CP_CONSTELLATIONS	6
#define V90CP_POWER_INDICES	35

/*
 * WHICH OF `V90MappingParams`' TWO BYTE TABLES A POWER FIGURE IS TAKEN OVER.
 * The mangling names the type (`26V90TxPowerMeasurementPoint`); the
 * enumerator names are INVENTED and describe only what the object does with
 * the value, which is a single equality test:
 *
 *     3e00d  4a           dec  %edx          getConstellationInfo
 *     3e00e  0f 84 ...    je   <codec arm>
 *     3e1f3  83 7c 24 68 01  cmpl $0x1,0x68(%esp)   getPower, inlined
 *
 * So ONE is the only value the object distinguishes, and it selects
 * `codecConstellation`; every other value takes `constellation`.  Nothing in
 * the object names a second enumerator or fixes what the two points ARE, so
 * the names below say which table is read and no more.
 *
 * THE BASE PIN IS OURS, and it is the same argument `V90CodecType.h` makes.
 * A two-valued enum has the range 0..1, which entitles the compiler to narrow
 * `== 1` to `!= 0` and would make the object's `cmpl $0x1` unreproducible and
 * untestable; a single negative enumerator makes the base signed and every
 * `int` representable.  `test/unit/t_v90cpower.cpp` drives the value 2 for
 * exactly this reason.
 */
enum V90TxPowerMeasurementPoint {
	V90_TX_POWER_OVER_CONSTELLATION = 0,
	V90_TX_POWER_OVER_CODEC_CONSTELLATION = 1,
	V90_TX_POWER_MEASUREMENT_POINT_BASE_PIN = -0x7fffffff - 1
};

class V90ConstellationPower {
public:
	/*
	 * Both one byte, both `ret`.  See the file comment for why they are
	 * declared at all.
	 */
	V90ConstellationPower();
	~V90ConstellationPower();

	/*
	 * Public for `offsetof`, and because the original's access specifiers
	 * are not recoverable from the mangling.  See V90ConstellationDesigner.h.
	 */

	/*
	 * Lay out the odometer for one `V90MappingParams`.  Writes every field
	 * below except `constellation` and `constellationSize`, and reads
	 * nothing of its own.
	 */
	void calcModulusParameters(V90MappingParams *mappingParams);

	/*
	 * Point `constellation` and `constellationSize` at one of the twelve
	 * byte tables in `mappingParams`.  A `void` return: no path assigns
	 * %eax before its `ret`.  An index above 5 writes NOTHING -- the switch
	 * has no default arm and the function falls straight to its epilogue.
	 */
	void getConstellationInfo(V90MappingParams *mappingParams,
				  V90TxPowerMeasurementPoint point,
				  unsigned int index);

	/*
	 * The mean square companded level of a whole six-symbol frame.  Calls
	 * `calcModulusParameters` first and then `getConstellationInfo` once
	 * per constellation, so it leaves the object fully written.
	 */
	float getPower(V90MappingParams *mappingParams,
		       V90TxPowerMeasurementPoint point, PcmType pcmType);

	/*
	 * The largest index whose limit is at or above `power`, saturating at
	 * 0.  Returns an index, never a failure code.
	 */
	unsigned int getPowerIndexForPower(float power);

	/* `index <= 34`, unsigned -- eleven bytes, `xor`/`cmpl`/`setbe`/`ret`. */
	bool isLegalPowerIndex(unsigned int index);

	/* --- data members --- */

	/*
	 * +0x00 and +0x04  The constellation `getConstellationInfo` last
	 * selected and its length, `mov %eax,(%ebx)` and `mov %eax,0x4(%ebx)`.
	 * The pointer is into the CALLER'S `V90MappingParams`, not owned here,
	 * and the length is the `unsigned int` that header measures -- it is
	 * used as `cmp $0x0,%eax; jbe` and `cmp %ebx,0x4(%esi); ja`, both
	 * unsigned.
	 */
	unsigned char *constellation;			/* +0x00 */
	unsigned int constellationSize;			/* +0x04 */

	/*
	 * +0x08  `1LL << (mappingParams->shaperSR + mappingParams->word_0 - 6)`,
	 * the number of distinct codewords a frame can carry.  SIGNED 64 bits:
	 * every division of it below goes to `__divdi3`/`__moddi3` and not to
	 * the `__u*` pair.  The shift is the i386 `shld`/`shl`/`test $0x20`
	 * sequence, so the width is forced too.
	 */
	long long codewordCount;			/* +0x08 */

	/*
	 * +0x10  The odometer, seven entries of eight bytes ending at +0x47.
	 * `remaining[0]` is `codewordCount - 1` and `remaining[i + 1]` is what
	 * is left after constellation `i` has taken its digit.  `getPower`
	 * reads `remaining[i]` at `0x10(%esi,%edi,8)` and `remaining[i + 1]` at
	 * `0x18(%esi,%edi,8)` off the same %edi, which is what makes this ONE
	 * array of seven rather than a scalar and an array of six.
	 */
	long long remaining[V90CP_CONSTELLATIONS + 1];	/* +0x10 */

	/*
	 * +0x48  Six 32-bit unsigned digits, `mov %eax,0x48(%edi)` and friends,
	 * read back with a `xor %ecx,%ecx` ahead of the 64-bit subtract -- a
	 * zero extension, so the field is unsigned and not `int`.  `getPower`
	 * compares them against the symbol index with `ja`/`je`, unsigned again.
	 *
	 * THE SIXTH IS NOT A REMAINDER.  Five of them are
	 * `remaining[i] % constellationSize[i]`; `modulus[5]` is the plain
	 * truncation of `remaining[5]` to 32 bits, `mov 0x38(%edi),%esi ; mov
	 * %esi,0x5c(%edi)`, with no division at all.  The two readings agree
	 * whenever `remaining[5] < constellationSize[5]`, which is the ordinary
	 * case, so the test drives shift counts large enough to separate them.
	 * Finding F3052.
	 */
	unsigned int modulus[V90CP_CONSTELLATIONS];	/* +0x48 */

	/*
	 * +0x60  The odometer's place values as doubles: `placeValue[0]` is 1.0
	 * and `placeValue[i]` is the product of the first `i` constellation
	 * sizes.  Six `fstl`/`fstpl` at +0x60 through +0x88, and the running
	 * product stays in %st across all six -- the object never reloads a
	 * stored one -- which is why the .cpp keeps a local rather than reading
	 * the member back.
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
