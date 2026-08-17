/*
 * V90Phase2Info.h -- what the V.90 Phase 2 line probe concluded.
 *
 * Reconstructed from dsplibs.o.  The class has three members in the blob and
 * this batch writes one of them, `printInfo() const`, which is 508 bytes of
 * nothing but diagnostics.  That makes it an unusually good oracle for the
 * object's TYPES -- every field it touches is printed with a conversion that
 * names its width and signedness -- and a poor one for the object's EXTENT,
 * because a printer reaches nothing it does not print.  So the map below is
 * assembled from all three members and says, for each entry, which one it
 * came from.
 *
 * NOT POLYMORPHIC.  tools/cppstruct.py lists no destructor at all, let alone
 * the deleting `D0` variant GCC emits only for a virtual one, so offset 0 is
 * a real member and there is no vptr (finding 228 is the four classes where
 * that is not true).
 *
 * THIRTY-SIX BYTES.  The largest `this`-relative displacement across all
 * three members is +0x20, and both the constructor and `setToDefault` reach
 * it with a four-byte access, so the object is 0x24 -- not the 0x20 the
 * displacement alone suggests and not the 0x1c `printInfo` alone reaches
 * (finding 215, and the V90Jd 0x8c -> 144 worked example in docs/v90cpp.md).
 * +0x10..0x17 and +0x1c..0x1f are reached by nothing and stay `pad_*`.
 *
 * THE FIELD NAMES ARE THE AUTHOR'S OWN, out of the format strings
 * `printInfo` hands to the diagnostic channel:
 *
 *     V90Phase2Info: pcmType = %s
 *     V90Phase2Info: rtd = %d
 *     V90Phase2Info: maxTxPower [dBm0]  = %c%d.%01d
 *     V90Phase2Info: txPowerMeasurementPoint = %s
 *     V90Phase2Info: Uinfo = %d
 *     V90Phase2Info: L2[%d] = %c%d.%03d
 *
 * `params` is the exception -- the constructor's argument type is the
 * mangling's (`V90Phase2Info(V90Parameters *)`) but a data member's name
 * never is, so that one is invented.
 *
 * THE CONSTRUCTOR IS NOW DECLARED, and the paragraph that used to stand here
 * saying it deliberately was not is superseded rather than deleted, because
 * its reasoning was sound and someone will re-derive it.  It said: declaring
 * one makes the class non-trivial and deletes the default members of the
 * union the test fixture needs, and writing it would mean modelling
 * V90Parameters, which nothing here did.
 *
 * Both halves have expired.  V90Parameters IS modelled -- 342 slots with the
 * author's own names, findings 860-862 -- so the five copies below can be
 * written by name instead of by offset.  And the fixture cost is now a cost
 * worth paying: `vpcm_create` cannot link without this symbol, so a class
 * that cannot be constructed is a class that keeps the whole construction
 * path unbuildable.  The union in t_v90p2info.cpp became a byte array and a
 * cast; that is the whole of the damage.
 *
 * `setToDefault` is still not written.  It repeats the constructor's five
 * copies without storing `params`, nothing in the construction closure needs
 * it, and finding 255 records it.
 *
 * ------------------------------------------------------------------------
 * THE COLLISION IS RESOLVED.  include/dsplib/V90PreFilter.h used to carry its
 * own stub `class V90Phase2Info`, a 0x1c-byte union, flagged there as
 * something "a later batch that models either should replace".  It now
 * includes this file instead, and the `#error` that stood here to make the
 * first translation unit needing both say so in one sentence is gone with it.
 * Finding 264.
 * ------------------------------------------------------------------------
 */

#ifndef DSPLIB_V90PHASE2INFO_H
#define DSPLIB_V90PHASE2INFO_H

/*
 * Declared, not defined.  `V90PreFilter.h` defines this class and includes
 * this file before doing so; a declaration may precede a definition, and
 * `params` below is only ever a pointer, so nothing here needs it complete.
 */
class V90Parameters;

/*
 * How many of `L2` `printInfo` prints: `inc %ebx; cmp $0x14,%ebx; jbe` is
 * 0 through 20 inclusive.  It is a LOWER BOUND on the array, not its length.
 *
 * `V90PreFilter::autoSelection` is the second reader and it agrees: it takes
 * entry 14 as a reference level and entries 15 through 20 as the six-point
 * signature it matches against each reference loop.  Twenty is the highest
 * index either function touches, from two different translation units, which
 * is what makes 21 a bound rather than a guess -- and nothing establishes an
 * upper one.
 */
#define V90PHASE2INFO_L2	21

class V90Phase2Info {
public:
	/*
	 * Five copies out of the parameter block, and the block itself kept at
	 * +0x20.  Two of the five are `cmpl $0x0` + `setne`, so what lands in
	 * `pcmType` and `txPowerMeasurementPoint` is 0 or 1 and never the
	 * parameter's own value.
	 *
	 * NO DESTRUCTOR.  `tools/cppstruct.py` lists none for this class and
	 * `nm` has no `_ZN13V90Phase2InfoD1Ev`, so the original declared none
	 * and neither do we -- which is why this class, unlike V90Jd, is still
	 * trivially destructible.
	 */
	V90Phase2Info(V90Parameters *params);

	/*
	 * Print the whole record.  `const` is not decoration: the method is
	 * `_ZNK...`, and dropping it mangles to a different symbol that links
	 * against nothing.
	 *
	 * A return type is not mangled.  `void` is what the object says: no
	 * path through the function assigns %eax before its `ret`.
	 */
	void printInfo() const;

	/* --- data members; see the file comment on the naming --- */

	/*
	 * +0x00  PcmType, the enum V90Phase3Modulator.h declares: 0 is
	 * PCM_TYPE_MU_LAW and 1 is PCM_TYPE_A_LAW, and `printInfo` prints
	 * "A_LAW" for 1 and "MU_LAW" for everything else.  Spelled `int`
	 * rather than `PcmType` so that this header does not have to pull in
	 * V90Phase3Modulator.h -- the width is the same, and the constructor
	 * stores a `setne` result, which is an int store of 0 or 1.
	 */
	int pcmType;

	/*
	 * +0x04  Round trip delay, whole-word copy out of V90Parameters+0x1c,
	 * printed with %d.
	 *
	 * THIS USED TO SAY "Signedness is not recoverable: nothing does
	 * arithmetic on it".  Something does now, and it says UNSIGNED.
	 * `V90Demodulator::getAT_UD` scales this field by 10/96 into the
	 * diagnostics record and the object divides with
	 *
	 *     mov $0xaaaaaaab,%eax ; mul %ebx ; shr $0x6,%edx
	 *
	 * -- `mul`, and with no sign correction anywhere in the range, where
	 * a signed divide by 96 has to adjust the quotient for a negative
	 * dividend.  Three other entries (`enterRRN`, `enterFPE`,
	 * `enterPhase4`) also do arithmetic on it, but only through `lea`,
	 * which is signedness-blind and settles nothing.
	 *
	 * THE TYPE IS LEFT AS `int` DELIBERATELY AND THE CAST IS AT THE USE
	 * SITE.  This header is included by four live branches, and finding
	 * 3511 is what a type change reaching one of them costs when the
	 * other half asserts against the old shape.  `getAT_UD` therefore
	 * spells the conversion itself and reproduces the object's
	 * instructions today; the retype belongs to a pass that owns this
	 * header and can move every user at once.  Finding 4903.
	 */
	int rtd;

	/*
	 * +0x08  `movzbl 0x8(%esi),%edx` and printed with %d, so an unsigned
	 * char widened, not a signed one.  VPcmFloModem::getUinfoValue(short)
	 * is where the name comes from in the object's own vocabulary.
	 */
	unsigned char Uinfo;

	/*
	 * +0x09  Also `movzbl`.  Not a dBm0 value: `printInfo` prints
	 * (maxTxPower + 1) * -0.5 under the label "maxTxPower [dBm0]", so the
	 * field is a code in half-decibel steps where 0 means -0.5 dBm0 and
	 * 11 means -6.0.  The unit is in the format string; the arithmetic is
	 * in the function.
	 */
	unsigned char maxTxPower;

	/* +0x0a  Alignment before the word at +0x0c.  Nothing reaches it. */
	unsigned char pad_0a[2];

	/*
	 * +0x0c  1 prints "CodecOutput" and anything else
	 * "DigitalModemTerminal"; the constructor stores a `setne` result, so
	 * in practice it is 0 or 1.  Left as `int` rather than invented into
	 * an enum -- the object names the two VALUES and not the type.
	 */
	int txPowerMeasurementPoint;

	/*
	 * +0x10, +0x14  Two more float arrays, living in the same object `L2`
	 * points into and installed in the same breath.  These used to be
	 * `pad_10[8]`, "reached by none of the three members" -- which was
	 * true of the three members and stopped being the whole story the
	 * moment a fourth function was read.  `VPcmFloModem::setPhaseIIinfo`
	 * stores four `VPcmFloModem` arrays here in one run of four `lea`s,
	 * at +0x10, +0x14, +0x18 and +0x1c, and
	 * `VPcmFloModem::getUinfoValue` clears all four to V90PHASE2INFO_L2
	 * entries in a single loop.
	 *
	 * ONLY THE THIRD HAS A NAME.  `printInfo` prints +0x18 as `L2[%d]`
	 * and prints none of the other three, so those stay offset-named:
	 * nothing establishes what they hold.  V92Phase2Info carries the same
	 * four in the same order at +0x18..+0x24.
	 */
	float *array_10;
	float *array_14;

	/*
	 * +0x18  The line measurement, at least V90PHASE2INFO_L2 floats.
	 * `mov 0x18(%esi),%edx; flds (%edx,%ebx,4)` -- a pointer that is
	 * loaded again on every iteration, not an array in the object.
	 * `V90PreFilter::autoSelection` reads the same offset as a `float *`
	 * (src/pump/v90/V90PreFilter.cpp), taking entry 14 as a reference level
	 * and entries 15 through 20 as the six-point signature it matches
	 * against each reference loop.  It reads through this field by name now
	 * that the stub is gone; it used to pun a pointer out of `b[0x18]`.
	 *
	 * The pointee's constness is not recoverable; nothing in the blob
	 * writes through it, and nothing here needs to.
	 */
	float *L2;

	/* +0x1c  The fourth of the group above.  Was `pad_1c[4]`. */
	float *array_1c;

	/*
	 * +0x20  The V90Parameters the constructor was handed.  Stored by
	 * `V90Phase2Info(V90Parameters *)` and read back by `setToDefault()`,
	 * which repeats the constructor's five copies without it.  This is
	 * the offset that sizes the object, and `printInfo` never touches it.
	 */
	V90Parameters *params;
};

#endif /* DSPLIB_V90PHASE2INFO_H */
