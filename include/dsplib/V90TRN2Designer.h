/*
 * V90TRN2Designer.h -- the V.90 TRN2 constellation designer.
 *
 * Reconstructed from dsplibs.o.  Six members; ONE pair is written here, the
 * constructor (0x3c9b0, 18 bytes) and the destructor (0x3c9f0, one byte).
 *
 * NOT POLYMORPHIC: the destructor appears with the `D1` and `D2` variants and
 * no `D0`, and GCC emits a deleting destructor only for a virtual one, so
 * offset 0 is a real member and there is no vptr.  Finding 228 is the four
 * classes where that is not true.
 *
 * THE CONSTRUCTOR IS THE WHOLE OBJECT MAP THIS FILE CLAIMS.  Eighteen bytes,
 * two stores, no call and no branch:
 *
 *     mov 0x4(%esp),%eax      ; this
 *     mov 0x8(%esp),%ecx      ; argument 1
 *     mov 0xc(%esp),%edx      ; argument 2
 *     mov %ecx,(%eax)
 *     mov %edx,0x4(%eax)
 *
 * and the MANGLING is what types the two slots:
 * `_ZN15V90TRN2DesignerC1EP13V90ParametersP21V90ConstellationPower` gives
 * argument 1 as `V90Parameters *` and argument 2 as `V90ConstellationPower *`.
 * Neither is read here, so the pointer types come from the name and not from
 * a dereference -- which is the strongest evidence available for a
 * constructor that only stores.
 *
 * +0x00 IS CORROBORATED by two of the members not written here, and this is
 * worth saying because it is the one slot something else touches:
 * `setNofUcodesInTrn2` does `mov 0x4(%esp),%ecx; mov (%ecx),%edx` and then
 * reads +0x80 and writes +0x78 through it, and `setTrn2DummyConstel` does
 * `mov 0x0(%ebp),%ecx; cmp 0x78(%ecx),%eax` with %ebp holding `this`.  Both
 * dereference +0x00 as a pointer, which is what makes it a pointer here
 * rather than merely the first four bytes of an unmodelled object.
 *
 * THE SIZE IS A BOUND WITH ITS SCOPE STATED, NOT A MEASUREMENT OF THE CLASS.
 * Four of the six members were read for this file -- the constructor, the
 * destructor, `setNofUcodesInTrn2` (0x18 bytes) and `setTrn2DummyConstel`
 * (0x59) -- plus `maxK` (0xcc), which turns out to touch `this` not at all
 * and works entirely through its `V90MappingParams *` argument.  Between them
 * the largest `this`-relative displacement is +0x04 and it is four bytes
 * wide, so those five reach no further than 0x08.  `V90TRN2Design` is 3,767
 * bytes and was NOT read, so the real object may be larger and this header
 * does not claim otherwise.  What makes that safe rather than sloppy is the
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
 * this header compatible with either (finding 1112).
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
	/*
	 * THE DESTRUCTOR IS ONE BYTE, a bare `ret`.  It is declared because
	 * the blob has the symbol: GCC emits an out-of-line destructor only
	 * for a user-declared one, so a class with an implicit destructor
	 * contributes no `D1`/`D2` at all.  The test drives it and asserts it
	 * writes nothing.
	 */
	V90TRN2Designer(V90Parameters *params, V90ConstellationPower *power);
	~V90TRN2Designer();

	/*
	 * Adopt `V90Parameters` +0x080 as `nofUcodesInTrn2` -- the ARGUMENT IS
	 * A FLAG, not a length; a zero does nothing.  See the .cpp.
	 */
	void setNofUcodesInTrn2(short on);

	/*
	 * Fill all six of `mappingParams`' constellations with a descending run
	 * from 78, `params->nofUcodesInTrn2` entries long.  Unbounded against
	 * the 128-byte row length.
	 */
	void setTrn2DummyConstel(V90MappingParams *mappingParams);

	/*
	 * log2 of the product of the six constellation lengths, truncated,
	 * with the object's own 1e-6 guard.  Touches nothing through `this`.
	 */
	int maxK(V90MappingParams *mappingParams);

	/*
	 * Lay out all six TRN2 constellations into `mappingParams`.  1 when
	 * every phase was designed, 0 when one could not be -- and on the
	 * failure path the mapping block is left holding
	 * `setTrn2DummyConstel`'s descending run rather than a half-design.
	 *
	 * SIXTEEN BITS OF RETURN, because the one caller tests `%ax`.
	 * `unused` is never read; see the .cpp.
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
