/*
 * V90SpectralConditions.h -- the spectral-condition selector.
 *
 * THE TYPE'S NAME IS THE OBJECT'S, from two manglings that name it as a
 * parameter and nothing else:
 *
 *   _ZN24V90ConstellationDesigner14spectralDesignEj28V90SpecialSpectralConditions
 *   _ZN15V90TRN2Designer13V90TRN2DesignEP16V90MappingParamsPA128_sS3_PA128_hPs\
 *       7PcmTypeS7_sPhjh28V90SpecialSpectralConditions
 *
 * It gets its own header for the same reason `__tHardwareCodecTypes__` does:
 * a C++98 enum definition may appear once, two classes in different headers
 * need it, and this is the only home small enough for both to include.
 *
 * ONE VALUE IS MEASURED AND IT IS NAMED FOR WHAT IT SELECTS, not guessed
 * from the type name.  `V90ConstellationDesigner::spectralDesign` compares
 * its second argument against the literal 2 (`cmpl $0x2,0x10(%esp)`) and
 * takes one of two arms; the two arms copy two disjoint runs of
 * `V90Parameters`, and `tools/vparse.py` gives the author's own names for
 * both runs:
 *
 *   the fall-through arm  +0x3a8..+0x3bc  SPECTRAL_SHAPER_{A1,A2,B1,B2,SR,ID}
 *   the `== 2` arm        +0x3c0..+0x3d4  GERMAN_PBX_SPECTRAL_SHAPER_{...}
 *
 * so 2 is the German PBX condition.  That is a measurement of the value's
 * MEANING; the enumerator's spelling is still ours.
 *
 * NO OTHER ENUMERATOR IS NAMED.  `spectralDesign` distinguishes 2 from
 * everything else and nothing more, so the fall-through covers 0 and any
 * other value alike and naming a "none" would be putting a guess in the
 * record.  A third shaper set exists in the parameters --
 * `EIA6_SPECTRAL_SHAPER_A1` at +0x3d8 -- which says the type has at least a
 * third value somewhere, and says nothing about which.
 *
 * THE PIN FIXES THE UNDERLYING TYPE and only that, exactly as
 * `V90CodecType.h`'s does: a single negative enumerator makes the base
 * signed and every `int` representable, which the type needs because
 * `V90ConstellationDesigner::process` receives one from a caller this tree
 * has not written and the test drives arbitrary values through it.
 */

#ifndef DSPLIB_V90SPECTRALCONDITIONS_H
#define DSPLIB_V90SPECTRALCONDITIONS_H

enum V90SpecialSpectralConditions {
	V90SpecialSpectralConditions_BASE_PIN = -0x7fffffff - 1,
	V90_SPECTRAL_GERMAN_PBX = 2
};

#endif /* DSPLIB_V90SPECTRALCONDITIONS_H */
