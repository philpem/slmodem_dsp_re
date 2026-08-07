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
 * Everything below +0x4c is `pad_`.  Twenty-one other members write into that
 * region and none of them is reconstructed here, so naming any of it would be
 * a guess rather than a measurement.
 */

#ifndef DSPLIB_V90CONSTELLATIONDESIGNER_H
#define DSPLIB_V90CONSTELLATIONDESIGNER_H

class V90ConstellationDesigner {
public:
	/* Defined in src/pump/v90/V90ConstellationDesigner.cpp. */
	void setMinMaxRates(unsigned int, unsigned int);

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling, and because a single access
	 * section is what keeps the class POD and __builtin_offsetof well
	 * defined -- the .cpp asserts the offsets below against what the
	 * compiler lays out.
	 */
	unsigned char pad_00[0x4c];	/* +0x00 twenty-one members' state  */
	unsigned int maxRate;		/* +0x4c defaults to 56000          */
	unsigned int minRate;		/* +0x50 defaults to 28000          */
};

#endif /* DSPLIB_V90CONSTELLATIONDESIGNER_H */
