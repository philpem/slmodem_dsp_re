/*
 * V90ConstellationDesigner.h -- the V.90 downstream constellation designer.
 *
 * Reconstructed from dsplibs.o.  Twenty-two members and 22,672 bytes of code,
 * of which ONE is written here: `setMinMaxRates`, the only member of the
 * class `v34handshak` reaches (docs/v90cpp.md's table of the fifty).
 *
 * NOT POLYMORPHIC.  `tools/cppstruct.py` lists the destructor with the `D1`
 * and `D2` variants and no `D0`, and GCC emits a deleting destructor only for
 * a virtual one, so offset 0 is a real member and there is no vptr.  Finding
 * 228 is the four classes where that is not true, and
 * `ResamplerTimingOffset` -- also in this batch -- is one of them.
 *
 * THE OBJECT IS 84 BYTES.  The largest `this`-relative displacement any of
 * the twenty-four defined members uses is +0x50 and the access there is four
 * bytes wide, so the object ends at 0x54.  A displacement is not a size
 * (finding 215); the width of what sits at the bound is what turns one into
 * the other.  The .cpp asserts both the size and the two offsets below.
 *
 * WHAT +0x4c AND +0x50 HOLD, measured and not inferred from the method name
 * alone.  The constructor ends with
 *
 *     movl $0x6d60,0x50(%eax)      28000
 *     movl $0xdac0,0x4c(%eax)      56000
 *
 * -- neither immediate carries a relocation, so both are integers and not
 * addresses -- and 28000 and 56000 are exactly the bottom and top of the V.90
 * downstream rate ladder.  `setMinMaxRates`' own two diagnostics then name
 * them the other way round from the argument order a reader would guess:
 * the FIRST argument is printed as "set min rate" and stored at +0x50, the
 * SECOND is printed as "set max rate" and stored at +0x4c.  So the offsets
 * descend as the arguments ascend, and the defaults confirm which is which.
 *
 * Everything the written members do not touch is `pad_`.  The other members
 * write into that region and none of them is reconstructed here, so naming
 * any of it would be a guess rather than a measurement.
 *
 * THE CONSTRUCTOR NAMED FOUR MORE SLOTS, and they were `pad_` until it was
 * read (task: the ctor/dtor batch).  `+0x30` and `+0x44` are its third and
 * second arguments -- a `V90ConstellationPower *` and a `V90PreFilter *`, and
 * the mangling is what types them.  `+0x08` and `+0x38` are bytes by their
 * store encodings (`c6 40 08 00` and `c6 40 38 16`, neither with an
 * operand-size prefix), seeded 0 and 22.  The constructor also writes the
 * two rate defaults documented above and zeroes `word_48`, so `reset()` is
 * not the only thing that clears it.
 *
 * `reset()` (task #88, the lifecycle batch) added the seven fields it writes.
 * It is 47 bytes of straight-line stores with no branch and no call, and the
 * only thing it reads is the parameter block at +0x00 -- which is what makes
 * +0x00 a `V90Parameters *` rather than merely the first four bytes of the
 * padding: `mov (%eax),%ecx` and then `mov 0x39c(%ecx),%edx`.
 */

#ifndef DSPLIB_V90CONSTELLATIONDESIGNER_H
#define DSPLIB_V90CONSTELLATIONDESIGNER_H

/*
 * `spectralDesign`'s second parameter, and the type is part of its mangled
 * name, so this include is what makes the symbol come out right.  See that
 * header for why the type lives on its own.
 */
#include "dsplib/V90SpectralConditions.h"

/*
 * A POINTER ONLY, so a forward declaration is what belongs here.  Two
 * different definitions of `V90Parameters` exist in this tree -- the 0x504
 * word block in `V90PreFilter.h` and the 0x558 named map in
 * `V90Parameters.h` -- and no translation unit may include both.  Declaring
 * the class here keeps this header compatible with either; the .cpp picks
 * one.  Finding 1112.
 */
class V90Parameters;

/*
 * The constructor's other two arguments, and pointers only, so forward
 * declarations are what belongs here for the same reason `V90Parameters` is
 * one: `V90PreFilter.h` is one of the two headers that DEFINE
 * `V90Parameters`, so including it here would decide for every translation
 * unit which of the two definitions it gets.
 */
class V90PreFilter;
class V90ConstellationPower;

/*
 * The constellation table five of the members below take by pointer, and the
 * type of the member at +0x04.  `V90MappingParams.h` defines it; a pointer is
 * all that is needed here.
 */
class V90MappingParams;

class V90ConstellationDesigner {
public:
	/*
	 * THE CONSTRUCTOR IS 54 BYTES OF STORES -- no call, no branch, and it
	 * reads nothing it is handed.  It keeps the three collaborators and
	 * seeds four constants: the rate ladder's two ends, a zero byte at
	 * +0x08 and 22 at +0x38.
	 *
	 * THE DESTRUCTOR IS ONE BYTE, a bare `ret` at 0x47900.  It is declared
	 * because the blob HAS the symbol: GCC emits an out-of-line destructor
	 * only for a user-declared one, so a class whose destructor were
	 * implicit would contribute no `D1`/`D2` at all.  The blob has both,
	 * one byte each, so the original declared it and left the body empty.
	 * `test/unit/t_v90designers.cpp` drives it and asserts that it writes
	 * nothing, rather than assuming it.
	 */
	V90ConstellationDesigner(V90Parameters *params, V90PreFilter *preFilter,
				 V90ConstellationPower *power);
	~V90ConstellationDesigner();

	/* Defined in src/pump/v90/V90ConstellationDesigner.cpp. */
	void setMinMaxRates(unsigned int, unsigned int);
	void reset();

	/*
	 * THE ELEVEN SMALL MEMBERS, and NOTHING IN THE OBJECT CALLS ANY OF
	 * THEM.  A sweep of every `R_386_PC32` in `.text` finds no caller for
	 * any of the eleven -- nor for `constelBuild` or `pow6` in particular
	 * -- so they survive only because a non-static member function has
	 * external linkage.  That is why the differential test drives each one
	 * directly rather than through a caller, and why nothing here can be
	 * cross-checked against a call site's argument types.
	 *
	 * RETURN TYPES ARE NOT MANGLED and therefore not measured.  What the
	 * object fixes is the register the value comes back in -- `%eax` for
	 * the six below that return an integer, `%st(0)` for `pow6`, `calcK`
	 * and `realK` -- and the widths chosen here are the narrowest that
	 * carries every value the body can produce.
	 */
	float pow6(short);
	float calcK(unsigned int, float *);
	float realK(V90MappingParams *);
	int maxK(V90MappingParams *);
	int calcMtoMatchKtarget(float, float);
	int findMinValueIndex(V90MappingParams *);
	int findConstelMaxValueIndex(V90MappingParams *);
	unsigned char constelBuild(short, short);
	void spectralDesign(unsigned int, V90SpecialSpectralConditions);
	void reconstructInitialConditions(V90MappingParams *, unsigned char *);
	int findNextUcodeToAdd(unsigned char *, unsigned char,
			      short (*)[128], short (*)[128], short *,
			      unsigned char (*)[128]);

	/*
	 * `setConstellationToNoise`, `setConstellationToNoise_forceRate` and
	 * `determineDminForRrn` are deliberately NOT declared yet.  Their
	 * argument lists are settled by the manglings, but a return type is
	 * not mangled and each of the three is large enough that reading it is
	 * what decides; a placeholder `void` committed ahead of that reading
	 * would be a guess in the record.  They arrive with their definitions.
	 */

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling, and because a single access
	 * section is what keeps the class POD and __builtin_offsetof well
	 * defined -- the .cpp asserts the offsets below against what the
	 * compiler lays out.
	 */

	/* +0x00  The parameter block.  Not owned; `reset` reads +0x39c. */
	V90Parameters *params;

	/*
	 * +0x04  The constellation table.  THREE INDEPENDENT MEMBERS FORCE THE
	 * TYPE, and each recovers the shape `V90MappingParams.h` documents
	 * from its own displacements:
	 *
	 *   spectralDesign            writes six dwords at +0x620..+0x634
	 *   constelBuild              reads `movzbl 0x4(%edx,%esi,1)` with
	 *                             %edx = k << 7 -- the +0x004 + 0x80*k
	 *                             constellation byte
	 *   findNextUcodeToAdd        the same read, same scaling
	 *
	 * It used to be `pad_04`.  Nothing reconstructed here WRITES it, so
	 * where it comes from is still unknown.
	 */
	V90MappingParams *mappingParams;	/* +0x04                    */

	/*
	 * +0x08  `movb $0x0,0x8(%eax)` in the constructor, and a BYTE: the
	 * encoding is `c6 40 08 00`, which has no operand-size prefix and no
	 * 32-bit immediate.  Nothing reconstructed here reads it, so it is
	 * offset-named; what the constructor proves is the width and the
	 * initial value, not the meaning.  It used to be inside `pad_04`.
	 */
	unsigned char byte_08;		/* +0x08                            */

	unsigned char pad_09;		/* +0x09                            */

	/*
	 * +0x0a .. +0x10  Four consecutive 16-bit slots `reset` zeroes with
	 * four `movw $0x0`.  They are offset-named; what makes them two bytes
	 * rather than four is the store width and nothing else.
	 *
	 * TWO OF THE FOUR NOW HAVE A READER and both readings are `movswl`,
	 * so they stay SIGNED: `findNextUcodeToAdd` loads +0x0a and +0x10 and
	 * adds each to a sign-extended `short` from a caller's table before a
	 * signed comparison.  That is the "forced" kind of extension --
	 * the 32-bit result is what the comparison uses -- and not the free
	 * kind of finding 614.
	 */
	short short_0a;			/* +0x0a */
	short short_0c;			/* +0x0c */
	short short_0e;			/* +0x0e */
	short short_10;			/* +0x10 */

	unsigned char pad_12[2];	/* +0x12                            */

	/*
	 * +0x14  `constelBuild`'s table, and its ONLY reader anywhere in the
	 * object.  `mov 0x14(%ebx),%edi` loads it once and the body then
	 * addresses BOTH a 16-bit table at `(%edi,%eax,2)` with %eax =
	 * (k << 7) + i and an 8-bit one at `0xd00(%eax,%ecx,1)` off the same
	 * register.  One base register with a fixed 0xd00 displacement is the
	 * measurement: two pointer members would have been two loads.  So the
	 * pointer is to the first table and the second lives 0xd00 bytes on,
	 * which is how the .cpp reaches it.
	 *
	 * WHAT THE POINTED-AT OBJECT IS is NOT known.  0xd00 is 13 rows of
	 * 128 shorts, and reading a row count out of that would be inference;
	 * nothing writes this field anywhere in the object, so there is no
	 * assignment to type it from either.  It used to be inside `pad_12`.
	 */
	short (*constelTable)[128];	/* +0x14                            */

	unsigned char pad_18[12];	/* +0x18                            */

	/*
	 * +0x24  Seeded by `reset` from the parameter block's +0x39c, which
	 * `tools/vparse.py` reports as `unnamed_39c`: `setToDefault` writes it
	 * and `loadParams` never reads it, so the original has no name for it
	 * either (finding 878).
	 */
	unsigned int word_24;		/* +0x24 = params->w[0x39c / 4]     */

	unsigned char pad_28[4];	/* +0x28                            */

	/*
	 * +0x2c  The companding law, and a four-byte load: `mov 0x2c(%edx),%esi
	 * ; test %esi,%esi` in `findNextUcodeToAdd`, whose zero arm calls
	 * `linear2ulaw` and complements the result and whose non-zero arm
	 * calls `linear2alaw` and XORs it with 0xd5.  So non-zero is A-law,
	 * which is the same convention `PcmType` records for
	 * `V90Phase3Modulator` (that header's finding).  It is typed `int`
	 * rather than `PcmType` because the only thing the object forces is
	 * the width and the `!= 0`, and naming the type would make this header
	 * depend on the one that defines the enum for no measured gain.  It
	 * used to be inside `pad_28`.
	 */
	int word_2c;			/* +0x2c non-zero selects A-law     */

	/*
	 * +0x30 and +0x44  The constructor's third and second arguments,
	 * stored and never read by anything reconstructed here.  What makes
	 * them pointers rather than four-byte integers is that they are copies
	 * of arguments the MANGLING types: the constructor is
	 * `_ZN24V90ConstellationDesignerC1EP13V90ParametersP12V90PreFilterP21V90ConstellationPower`,
	 * so argument 2 is a `V90PreFilter *` and argument 3 a
	 * `V90ConstellationPower *`, and the object puts argument 3 at +0x30
	 * and argument 2 at +0x44.  Both used to be inside `pad_28`.
	 */
	V90ConstellationPower *power;	/* +0x30 = constructor argument 3   */

	unsigned char pad_34[4];	/* +0x34                            */

	/*
	 * +0x38  `movb $0x16,0x38(%eax)`, again a byte by its encoding
	 * (`c6 40 38 16`).  22 is not one of the rate-ladder constants and
	 * nothing here reads it, so it is offset-named too.
	 */
	unsigned char byte_38;		/* +0x38 defaults to 22             */

	unsigned char pad_39[11];	/* +0x39                            */

	V90PreFilter *preFilter;	/* +0x44 = constructor argument 2   */

	unsigned int word_48;		/* +0x48 zeroed by reset            */

	unsigned int maxRate;		/* +0x4c defaults to 56000          */
	unsigned int minRate;		/* +0x50 defaults to 28000          */
};

#endif /* DSPLIB_V90CONSTELLATIONDESIGNER_H */
