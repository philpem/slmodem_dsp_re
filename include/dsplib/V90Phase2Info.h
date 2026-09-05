/**
 * @file V90Phase2Info.h
 * @brief What the V.90 Phase 2 line probe concluded: PCM law, round-trip
 *        delay, transmit power limit and the multi-point line-level
 *        measurement (`L2`) it took.
 *
 * Reconstructed from dsplibs.o. The object is 36 bytes (0x24) -- the largest
 * `this`-relative displacement across all three members is +0x20, reached by
 * a four-byte access in both the constructor and setToDefault() (finding
 * F215); +0x10..0x17 and +0x1c..0x1f are reached by nothing reconstructed
 * here and stay `pad_*`. Not polymorphic: no destructor at all, so offset 0
 * is a real member with no vptr (finding F228 is the four classes where that
 * is not true).
 *
 * Most field names are the author's own, out of `printInfo()`'s format
 * strings (`pcmType`, `rtd`, `maxTxPower`, `txPowerMeasurementPoint`,
 * `Uinfo`, `L2`); `params` is invented, since a data member's name is never
 * in the mangling the way the constructor argument's type is.
 *
 * The constructor and setToDefault() were declined for a while: declaring
 * either makes the class non-trivial (breaking a test fixture's union) and
 * needs `V90Parameters` modelled, which nothing had done yet. Both
 * conditions have since cleared -- `V90Parameters` has 342 named slots
 * (findings F860-862), and `vpcm_create` cannot link without a constructor,
 * so a class that cannot be constructed keeps the whole construction path
 * unbuildable. setToDefault() followed once the constructor did (finding
 * F255 is the earlier decline).
 *
 * `include/dsplib/V90PreFilter.h` used to carry its own stub of this class,
 * a 0x1c-byte union; it now includes this file instead (finding F264).
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
 * A lower bound on `L2`'s length, not its length: `printInfo()` prints
 * entries 0-20 and `V90PreFilter::autoSelection` (a different translation
 * unit) reads entry 14 as a reference level and 15-20 as a six-point
 * signature, so 20 is the highest index either function touches and nothing
 * establishes an upper bound.
 */
#define V90PHASE2INFO_L2	21

class V90Phase2Info {
public:
	/**
	 * @brief Construct from a V.90 parameter block: copy five fields
	 * out of it and keep the block itself.
	 *
	 * `pcmType` and `txPowerMeasurementPoint` are each normalised to 0
	 * or 1 (`!= 0`) rather than storing the parameter's own value. No
	 * destructor is declared -- the original had none, so this class is
	 * still trivially destructible.
	 * @param params  The V.90 parameter block to copy from and keep.
	 */
	V90Phase2Info(V90Parameters *params);

	/**
	 * @brief Print the whole record to the diagnostic channel.
	 */
	void printInfo() const;

	/**
	 * @brief Reapply the constructor's five parameter copies from the
	 * stored `params`, without re-storing it. Leaves the four `L2`-group
	 * arrays and `params` itself untouched.
	 */
	void setToDefault();

	/* --- data members; see the file comment on the naming --- */

	/**
	 * @brief The line's PCM law: 0 (`PCM_TYPE_MU_LAW`) or 1
	 * (`PCM_TYPE_A_LAW`, printed "A_LAW"; anything else prints "MU_LAW").
	 * Spelled `int` rather than `PcmType` so this header need not include
	 * V90Phase3Modulator.h for the enum.
	 */
	int pcmType;

	/**
	 * @brief Round-trip delay, copied whole from the parameter block.
	 *
	 * Treated as unsigned by its one arithmetic user:
	 * `V90Demodulator::getAT_UD` scales it by 10/96 with an unsigned
	 * `mul`/`shr` and no sign correction. Left typed `int` regardless --
	 * this header has four live include sites and a type change reaching
	 * one without the others is finding F3511's failure mode, so the
	 * unsigned conversion is spelled at the one use site instead (finding
	 * F4903). A future pass that owns every caller at once can retype it.
	 */
	int rtd;

	/**
	 * @brief Line level in Uinfo units, an unsigned byte widened before
	 * printing. Named from `VPcmFloModem::getUinfoValue(short)`, the
	 * object's own vocabulary for it.
	 */
	unsigned char Uinfo;

	/**
	 * @brief Maximum transmit power, as a code in half-decibel steps
	 * rather than a direct dBm0 value: `printInfo()` prints
	 * `(maxTxPower + 1) * -0.5` under the label "maxTxPower [dBm0]", so 0
	 * means -0.5 dBm0 and 11 means -6.0.
	 */
	unsigned char maxTxPower;

	/*
	 * +0x0a was `pad_0a[2]` -- REMOVED (finding F10151).  It was already
	 * correctly described as alignment; now proved mechanically rather
	 * than by description alone, both ends already asserted in the .cpp
	 * (`V90P2I_OFF(maxTxPower, 0x09, ...)`, `V90P2I_OFF
	 * (txPowerMeasurementPoint, 0x0c, ...)`), and `dis.py` over every
	 * `V90Phase2Info` method plus `VPcmFloModem::getUinfoValue` finds no
	 * access to offset 0x0a.
	 */

	/**
	 * @brief Where transmit power was measured: 1 prints "CodecOutput",
	 * anything else "DigitalModemTerminal". Left as `int` rather than an
	 * invented enum -- the object names the two values, not the type.
	 */
	int txPowerMeasurementPoint;

	/*
	 * +0x10, +0x14  Two more float array pointers, part of the same group
	 * of four as `L2` (+0x18) and `array_1c` (+0x1c): `VPcmFloModem::
	 * setPhaseIIinfo` installs all four in one run of four `lea`s, and
	 * `VPcmFloModem::getUinfoValue` clears all four to V90PHASE2INFO_L2
	 * entries in a single loop. Only `L2` has a name -- `printInfo()`
	 * prints only +0x18 as `L2[%d]` -- so these two stay offset-named;
	 * nothing establishes what they hold. `V92Phase2Info` carries the
	 * same four, in the same order, at +0x18..+0x24.
	 */
	float *array_10;
	float *array_14;

	/**
	 * @brief The Phase 2 line-level measurement: a pointer to at least
	 * #V90PHASE2INFO_L2 floats, reloaded on every access rather than
	 * cached (not a fixed-size array in the object). `printInfo()`
	 * prints it as `L2[%d]`; `V90PreFilter::autoSelection` reads entry 14
	 * as a reference level and entries 15-20 as a six-point signature
	 * matched against each reference loop. Pointee constness is not
	 * recoverable -- nothing in the blob writes through it.
	 */
	float *L2;

	/* +0x1c  The fourth of the +0x10 group above. */
	float *array_1c;

	/**
	 * @brief The parameter block passed to the constructor, kept for
	 * setToDefault() to re-read. Not touched by printInfo().
	 */
	V90Parameters *params;
};

#endif /* DSPLIB_V90PHASE2INFO_H */
