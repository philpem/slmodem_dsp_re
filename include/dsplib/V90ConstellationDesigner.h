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
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling, and because a single access
	 * section is what keeps the class POD and __builtin_offsetof well
	 * defined -- the .cpp asserts the offsets below against what the
	 * compiler lays out.
	 */

	/* +0x00  The parameter block.  Not owned; `reset` reads +0x39c. */
	V90Parameters *params;

	unsigned char pad_04[4];	/* +0x04                            */

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
	 * four `movw $0x0`.  Nothing reconstructed here reads any of them, so
	 * they are offset-named; what makes them two bytes rather than four
	 * is the store width and nothing else.
	 */
	short short_0a;			/* +0x0a */
	short short_0c;			/* +0x0c */
	short short_0e;			/* +0x0e */
	short short_10;			/* +0x10 */

	unsigned char pad_12[0x12];	/* +0x12                            */

	/*
	 * +0x24  Seeded by `reset` from the parameter block's +0x39c, which
	 * `tools/vparse.py` reports as `unnamed_39c`: `setToDefault` writes it
	 * and `loadParams` never reads it, so the original has no name for it
	 * either (finding 878).
	 */
	unsigned int word_24;		/* +0x24 = params->w[0x39c / 4]     */

	unsigned char pad_28[8];	/* +0x28                            */

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
