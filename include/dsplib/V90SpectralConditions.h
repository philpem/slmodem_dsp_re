/**
 * @file V90SpectralConditions.h
 * @brief `enum V90SpecialSpectralConditions` -- the special line-spectrum
 *        condition `V90SpectralVerifier` detects, and which of two alternate
 *        spectral-shaper tables `V90ConstellationDesigner::spectralDesign`
 *        should therefore use.
 *
 * The type takes its name from the object's own C++ manglings, which name
 * it only as a parameter of `V90ConstellationDesigner::spectralDesign` and
 * `V90TRN2Designer::V90TRN2Design`. It has its own header for the same
 * reason `__tHardwareCodecTypes__` does: a C++98 enum definition may appear
 * only once, and both classes need it.
 *
 * All four enumerators are named from the object's own evidence.
 * `V90SpectralVerifier::checkSpecialSpectralConditions` is the detector, and
 * each of its four outcomes is a stored value paired with the author's own
 * diagnostic string ("German ISDN NT1 box conditions detected!", "German PBX
 * conditions detected!", "Severe Codec conditions detected!", or the field
 * left at 0 for "No special conditions"). `V90_SPECTRAL_GERMAN_PBX == 2` has
 * two independent derivations that agree: the detector's string, and
 * separately `spectralDesign` comparing this value against the literal 2 to
 * choose between two disjoint runs of `V90Parameters` that `tools/vparse.py`
 * names `SPECTRAL_SHAPER_*` and `GERMAN_PBX_SPECTRAL_SHAPER_*`. See findings
 * F2142 and F5801.
 *
 * A third, unused shaper table set exists in the parameters --
 * `EIA6_SPECTRAL_SHAPER_A1` -- but nothing in the object ever stores a fifth
 * value into the detected-condition field, so whatever selects that set is
 * not this enumeration as the object uses it, and no enumerator is invented
 * for it.
 *
 * The negative base enumerator (`_BASE_PIN`) fixes only the underlying type
 * to `int`, exactly as `V90CodecType.h`'s does: `spectralDesign` receives
 * this value from a caller this tree has not written, and the test harness
 * drives arbitrary values through it, so the enum's storage must be able to
 * represent any `int`.
 */

#ifndef DSPLIB_V90SPECTRALCONDITIONS_H
#define DSPLIB_V90SPECTRALCONDITIONS_H

enum V90SpecialSpectralConditions {
	V90SpecialSpectralConditions_BASE_PIN = -0x7fffffff - 1,
	V90_SPECTRAL_NONE = 0,
	V90_SPECTRAL_GERMAN_ISDN_NT1 = 1,
	V90_SPECTRAL_GERMAN_PBX = 2,
	V90_SPECTRAL_SEVERE_CODEC = 3
};

#endif /* DSPLIB_V90SPECTRALCONDITIONS_H */
