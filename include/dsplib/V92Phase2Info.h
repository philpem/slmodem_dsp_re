/**
 * @file V92Phase2Info.h
 * @brief The V.92 twin of V90Phase2Info: what V.92 Phase 2 concluded,
 *        installed into `VPcmFloModem` by `setPhaseIIinfo`.
 *
 * `sizeof(V92Phase2Info)` is 0x2c. Identified as the type behind
 * `VPcmFloModem`'s +0x612c pointer by five independent agreements with the
 * sibling V90Phase2Info that `setPhaseIIinfo` fills at the same time --
 * matching field values at +0x00/+0x04/+0x09/+0x0c, and the same float array
 * stored into both classes' `L2` field (finding F1222).
 *
 * Field names are almost all the author's own, out of `printInfo`'s format
 * strings ("pcmType", "rtd", "Uinfo", "maxTxPower", "ShortPhase2"/
 * "v92Capabilities" local and remote, "v90UseHighCarrier", `L2[%d]`) and
 * `setPhaseIIinfo`'s matching diagnostics; `txPowerMeasurementPoint` is the
 * one exception, named by its whole-word copy from V90Phase2Info rather than
 * by any V.92-side string, since `printInfo` never prints it (finding
 * F1222).
 */

#ifndef DSPLIB_V92PHASE2INFO_H
#define DSPLIB_V92PHASE2INFO_H

/*
 * How many of `L2` `V92Phase2Info::printInfo` prints: `cmp $0x14,%ebx` with a
 * `jbe` is 0 through 20 inclusive, the same bound V90PHASE2INFO_L2 records
 * for the V.90 class, and `VPcmFloModem::getUinfoValue` clears exactly 21
 * entries of the array it hands to both.  A lower bound, not a length.
 */
#define V92PHASE2INFO_L2	21

/*
 * Declared, not defined: the constructor takes one only to read eight fields
 * out of it, and `params` below is only ever a pointer.  `V92Phase2Info.cpp`
 * includes the 55-slot header.
 */
class V92Parameters;

class V92Phase2Info {
public:
	/**
	 * @brief Construct the record: store @p params and fill the three
	 *        echo-canceller sizing bytes (+0x14..+0x16) from it. No
	 *        destructor exists in the object, so none is declared here.
	 * @param params  The V.92 parameter block; only eight of its fields
	 *                are read.
	 */
	V92Phase2Info(V92Parameters *params);

	/**
	 * @brief Refill the record from the `V92_PHASE2_INFO_*` defaults and
	 *        the echo-canceller sizing parameters, resetting the
	 *        local/remote capability bytes to the local-V.92 defaults
	 *        (`v92CapabilitiesLocal` = 1, the rest 0).
	 */
	void setToDefault();
	/**
	 * @brief Log the whole record through the object's own diagnostic
	 *        calls, in its own field order.
	 */
	void printInfo() const;

	/*
	 * +0x00  `printInfo` compares it against 1 and prints "A_LAW" for 1
	 * and "MU_LAW" for anything else -- the same two spellings and the
	 * same comparison V90Phase2Info::printInfo uses on its own +0x00.
	 */
	int pcmType;

	/* +0x04  Printed with %d, and `setPhaseIIinfo` stores its `int` argument
	 * here and at V90Phase2Info +0x04 in the same breath. */
	int rtd;

	/* +0x08  `movzbl` and printed with %d, so an unsigned char widened. */
	unsigned char Uinfo;

	/*
	 * +0x09  Also `movzbl`.  `setPhaseIIinfo` builds it as a five-bit
	 * weighted sum of INFO0 bits 33..37 and copies it here from
	 * V90Phase2Info +0x09.  As there, the printed value is not the field:
	 * `printInfo` prints it through the same half-decibel arithmetic.
	 */
	unsigned char maxTxPower;

	/*
	 * +0x0a was `pad_0a[2]` -- REMOVED (finding F10151).  Already
	 * correctly described as alignment; proved mechanically by the
	 * existing `V92P2I_OFF(maxTxPower, 0x09, ...)`/`V92P2I_OFF
	 * (txPowerMeasurementPoint, 0x0c, ...)` and by `dis.py` over every
	 * `V92Phase2Info` method plus `V92Modulator`'s two constructors,
	 * which take a `V92Phase2Info *` -- the only hit near this offset is
	 * an unrelated `add $0xa,%eax` immediate in an allocation-size
	 * computation, not a memory access.
	 */

	/*
	 * +0x0c  Whole-word copy out of V90Phase2Info +0x0c, which
	 * `V90Phase2Info::printInfo` labels `txPowerMeasurementPoint`.
	 * `V92Phase2Info::printInfo` does not print it, so the name is by
	 * that copy and not by this class's own words.
	 */
	int txPowerMeasurementPoint;

	/* +0x10  "ShortPhase2: local".  Not written by anything here. */
	unsigned char shortPhase2Local;

	/*
	 * +0x11  "v92Capabilities: local".  `VPcmFloModem::setPcmSessionType`
	 * stores the low byte of its argument here.
	 */
	unsigned char v92CapabilitiesLocal;

	/*
	 * +0x12  "ShortPhase2: remote", and +0x13 "v92Capabilities: remote".
	 * `setPhaseIIinfo` fills both out of INFO0 bits 26 and 27 -- and
	 * which bit goes to which is a runtime choice; see VPcmFloModem.h.
	 */
	unsigned char shortPhase2Remote;
	unsigned char v92CapabilitiesRemote;

	/*
	 * +0x14, +0x15, +0x16  Three fields, not the padding this used to
	 * call them.  The constructor copies the low byte of
	 * `V92_NOF_FILTER_SECTIONS`, `V92_MAX_TOTAL_NOF_COEFFS` and
	 * `V92_MAX_NOF_COEFFS_IN_EACH_SECTION` here, in that order and in
	 * three separate whole-word loads with byte stores -- so each is a
	 * count small enough to fit in a byte and the parameter block's own
	 * `int` width is not carried.
	 *
	 * The names are the parameters', not the class's: neither
	 * `printInfo` nor `setPhaseIIinfo` touches these three, so no format
	 * string names them; what is recoverable is which parameter fills
	 * each, and that is what they are named after.  Finding F1222.
	 */
	unsigned char nofFilterSections;
	unsigned char maxTotalNofCoeffs;
	unsigned char maxNofCoeffsInEachSection;

	/* +0x17  "v90UseHighCarrier = %d", `movzbl`. */
	unsigned char v90UseHighCarrier;

	/*
	 * +0x18, +0x1c, +0x20, +0x24  Four float arrays in the enclosing
	 * VPcmFloModem, installed by `setPhaseIIinfo`, of which only the
	 * third has a name: `printInfo` prints +0x20 as `L2[%d]`.  The other
	 * three are cleared alongside it by `getUinfoValue` and read by
	 * nothing this tree has written, so they are offset-named.
	 *
	 * The same four arrays are installed into V90Phase2Info at +0x10,
	 * +0x14, +0x18 and +0x1c, in the same order -- so `L2` is third in
	 * both classes and this one simply carries eight more bytes ahead of
	 * the group.
	 */
	float *array_18;
	float *array_1c;
	float *L2;
	float *array_24;

	/*
	 * +0x28  The V92Parameters the constructor was handed, stored first
	 * and read back by nothing this tree has written.  It is what SIZES
	 * the object: a four-byte store at +0x28 makes the class 0x2c, where
	 * the four array pointers alone would have stopped at 0x28.  The same
	 * shape as V90Phase2Info's `params` at its own +0x20, and the name is
	 * borrowed from there -- a data member's name is not mangled and so
	 * is not recoverable.
	 */
	V92Parameters *params;
};

#endif /* DSPLIB_V92PHASE2INFO_H */
