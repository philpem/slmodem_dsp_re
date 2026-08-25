/*
 * V92Phase2Info.h -- the V.92 twin of V90Phase2Info.
 *
 * Reconstructed from dsplibs.o.  The class has three members in the blob --
 * `V92Phase2Info(V92Parameters *)`, `setToDefault()` and `printInfo() const`
 * -- and this tree writes NONE of them.  It is declared here because
 * `VPcmFloModem::setPhaseIIinfo` fills one in through a pointer at
 * VPcmFloModem +0x612c, and writing that method means naming the fields it
 * writes.
 *
 * WHY THE POINTER AT +0x612c IS A V92Phase2Info AND NOT A GUESS.  The same
 * method fills a V90Phase2Info at the same time, and copies four values from
 * it into this one -- +0x00, +0x04, +0x09 and +0x0c, which are exactly
 * V90Phase2Info's `pcmType`, `rtd`, `maxTxPower` and
 * `txPowerMeasurementPoint`.  It then stores the SAME float array into
 * V90Phase2Info +0x18 and into this class's +0x20, and
 * `V92Phase2Info::printInfo` prints +0x20 as `L2[%d]` over indices 0..20 --
 * the identical loop bound V90Phase2Info::printInfo uses on its own +0x18.
 * Five agreements, two of them from a translation unit nothing here wrote.
 *
 * THE FIELD NAMES ARE THE AUTHOR'S OWN, out of `V92Phase2Info::printInfo`'s
 * format strings:
 *
 *     V92Phase2Info: pcmType = %s
 *     V92Phase2Info: rtd = %d
 *     V92Phase2Info: Uinfo = %d
 *     V92Phase2Info: maxTxPower [dBm0]  = %c%d.%01d
 *     V92Phase2Info: ShortPhase2: local=%d , remote=%d
 *     V92Phase2Info: v92Capabilities: local=%d , remote=%d
 *     V92Phase2Info: v90UseHighCarrier = %d
 *     V92Phase2Info: L2[%d] = %c%d.%03d
 *
 * and `VPcmFloModem::setPhaseIIinfo`'s own diagnostics agree, calling +0x10
 * and +0x12 "short phase2: local / remote" and +0x11 and +0x13 "V92
 * capabilities: local / remote".  `txPowerMeasurementPoint` is the one
 * exception: `printInfo` does not print +0x0c, and the name is carried over
 * from the whole-word copy out of V90Phase2Info +0x0c.
 *
 * NO SIZE IS ASSERTED.  The largest displacement the two readers reach is
 * +0x24, a four-byte store, so the object is at least 0x28 -- but only two of
 * its three members have been read and neither is the constructor, which is
 * where V90Phase2Info's own size came from.  0x28 is a floor.
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
	/*
	 * THE CONSTRUCTOR IS NOW WRITTEN, and the note that used to stand here
	 * -- "DATA ONLY ... declaring one and defining it would re-open the
	 * link closure for V92Parameters" -- is superseded.  V92Parameters is
	 * modelled (55 slots, the author's own names for 54 of them, findings
	 * F860-862), so there is no closure to re-open; and `vpcm_create`
	 * cannot link without this symbol.
	 *
	 * IT ALSO SETTLED TWO THINGS THE HEADER HAD WRONG, both recorded in
	 * finding F1222: the object is 0x2c bytes and not 0x28, because the
	 * constructor stores its argument at +0x28; and +0x14..+0x16, called
	 * `pad_14[3]` here on the grounds that neither reader reached them,
	 * are three real fields the constructor fills from the V.92 filter
	 * parameters.  A padding run is only padding until a third function is
	 * read.
	 *
	 * NO DESTRUCTOR.  `nm` has no `_ZN13V92Phase2InfoD1Ev`, so the
	 * original declared none and neither do we.
	 */
	V92Phase2Info(V92Parameters *params);

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

	/* +0x0a  Alignment before the word at +0x0c.  Nothing reaches it. */
	unsigned char pad_0a[2];

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
	 * WHICH BIT GOES TO WHICH IS A RUNTIME CHOICE, see VPcmFloModem.h.
	 */
	unsigned char shortPhase2Remote;
	unsigned char v92CapabilitiesRemote;

	/*
	 * +0x14, +0x15, +0x16  THREE FIELDS, not the padding this used to
	 * call them.  The constructor copies the low byte of
	 * `V92_NOF_FILTER_SECTIONS`, `V92_MAX_TOTAL_NOF_COEFFS` and
	 * `V92_MAX_NOF_COEFFS_IN_EACH_SECTION` here, in that order and in
	 * three separate whole-word loads with byte stores -- so each is a
	 * count small enough to fit in a byte and the parameter block's own
	 * `int` width is not carried.
	 *
	 * THE NAMES ARE THE PARAMETERS', NOT THE CLASS'S.  Neither
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
