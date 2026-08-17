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
 * THE OTHER THREE ARE NOW NAMED, AND THE EVIDENCE IS THE STRONGEST KIND
 * THERE IS: the author's own sentences, each one printed beside the store
 * that sets the value.  `V90SpectralVerifier::checkSpecialSpectralConditions`
 * is the detector, `V90SpectralVerifier::word_28` is where it leaves the
 * answer, and the four arms are
 *
 *   0x46559  movl $0x1,0x28(%edi)  "German ISDN NT1 box conditions detected!"
 *   0x46588  movl $0x2,0x28(%edi)  "German PBX conditions detected!"
 *   0x46573  movl $0x3,0x28(%edi)  "Severe Codec conditions detected!"
 *   0x46517  the field still 0     "No special conditions"
 *
 * -- the store and its `edprintf` adjacent in every case, and the fourth
 * reached by testing the field for zero at 0x4651a.  The earlier note here
 * declined 0, 1 and 3 because `spectralDesign` cannot distinguish them; that
 * was a statement about ONE function's reach and not about the object, and
 * the detector settles all four.  Finding 5801.
 *
 * TWO INDEPENDENT DERIVATIONS AGREE ON THE 2, which is why it was the one
 * that could be named first: the paragraph above reaches it from the shaper
 * runs `spectralDesign` copies, and 0x46588 reaches it from the string.
 * Neither reading knew about the other.
 *
 * A THIRD SHAPER SET EXISTS IN THE PARAMETERS -- `EIA6_SPECTRAL_SHAPER_A1`
 * at +0x3d8, with `EIA6_SPECTRAL_VERIFIER_ENABLE` at +0x2a8 beside the
 * verifier's own enable.  Nothing in the object stores a fifth value into
 * `word_28`, so whatever selects that set is not this enumeration as this
 * object uses it, and no enumerator is invented for it.
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
	V90_SPECTRAL_NONE = 0,
	V90_SPECTRAL_GERMAN_ISDN_NT1 = 1,
	V90_SPECTRAL_GERMAN_PBX = 2,
	V90_SPECTRAL_SEVERE_CODEC = 3
};

#endif /* DSPLIB_V90SPECTRALCONDITIONS_H */
