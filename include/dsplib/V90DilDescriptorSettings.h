/*
 * V90DilDescriptorSettings.h -- the free functions over a V.90 DIL
 * descriptor.
 *
 * THIS FILE WAS `V90Dil.h` AND THE CLASS-LESS NAME WAS A GUESS.  It said so:
 * "`calculateDilLength` is a free function, not a member -- its mangling has
 * no class component -- so it needs a home of its own", and a home of its own
 * had to be invented because nothing then said what the original's was.
 * Something does now.  The blob carries an `STT_FILE` entry
 * `V90DilDescriptorSettings.cpp` (#244 of 283), and `setDilDescriptor`'s two
 * `edprintf` messages both begin `"V90DilDescriptorSettings: "` -- the
 * author's own words, which is CLAUDE.md's first tier of evidence.  Where
 * every other diagnostic in this area names a class and a member
 * ("V90Modem Reset: ..."), these name the FILE, because there is no class to
 * name.  Finding F7600.
 *
 * ===========================================================================
 * THE TRANSLATION UNIT IS FOUR FREE FUNCTIONS AND EIGHT TABLES
 * ===========================================================================
 *
 *     0x31c60  setDilDescriptor(tagV90DILdescriptor *, DilType)   0x131
 *     0x31da0  getSegmentPointer(PcmType, int)                    0x073
 *     0x31e20  calculateDilLength(DilType, PcmType)               0x0f1
 *     0x31f20  calculateDilLength(tagV90DILdescriptor *, PcmType) 0x0c4
 *
 * contiguous in `.text`, bounded below by `V90SignBitsExtractor::process`
 * (ends 0x31c51) and above by `ModulusDecoder::ModulusDecoder` (0x31ff0), and
 * every one of them a mangled free function with no class component.  The
 * membership is PROVED and not inferred for two of the four: the eight tables
 * are LOCAL `.data` symbols, and a local symbol can only be referenced from
 * its own translation unit, so `setDilDescriptor` (which reads all eight) and
 * `calculateDilLength(DilType, PcmType)` (which reads `N`, `TO` and `H`) are
 * necessarily in one file together.  The other two are adjacency and the
 * overload relationship.
 *
 * TWO OF THE FOUR ARE STILL UNWRITTEN and both are unreachable from any entry
 * point -- `tools/service.py --list none` -- so neither blocks a link.
 *
 * ===========================================================================
 * THE DESCRIPTOR ARGUMENT IS NOT `const`, AND THAT IS THE AUTHOR'S
 * ===========================================================================
 *
 * `calculateDilLength`'s mangling is `P19tagV90DILdescriptor`, where
 * `V90Phase3Modulator::resetDILGenerator` says `PK19tagV90DILdescriptor`.
 * The function reads and never writes, so the missing `const` is the
 * author's and reproducing it is not optional -- a `const` here emits a
 * different symbol that links against nothing (docs/v90cpp.md).
 */

#ifndef DSPLIB_V90DILDESCRIPTORSETTINGS_H
#define DSPLIB_V90DILDESCRIPTORSETTINGS_H

/* For `tagV90DILdescriptor` and `PcmType`.  Included, never edited. */
#include "dsplib/V90Phase3Modulator.h"

/*
 * `DilType`, spelled `7DilType` in both manglings that carry it, so it is a
 * plain enum at namespace scope with that exact tag.  The object names no
 * enumerator -- a mangling never does (finding F226) -- but the two
 * `edprintf` messages at the end of `setDilDescriptor` do:
 *
 *     0 -> "DIL descriptor set to option ADI."
 *     1 -> "DIL descriptor set to option ADI_QC."
 *
 * so the VALUES are the object's and the words are the author's; only the
 * `DIL_TYPE_` prefix is ours, following `PcmType`/`PCM_TYPE_*` in
 * V90Phase3Modulator.h.  An enumerator is a compile-time substitution and
 * cannot move code generation, so this costs nothing at the codegen tier.
 *
 * ADI is the Automatic Digital Impairment learning descriptor and ADI_QC the
 * quick-connect one; `V90Modem::reset` chooses between them on its `qcFlag`
 * argument and nothing else in the object constructs a `DilType`.  The two
 * differ in the LENGTHS only -- see the table comment in
 * src/pump/v90/V90DilDescriptorSettings.cpp.
 */
enum DilType {
	DIL_TYPE_ADI	= 0,
	DIL_TYPE_ADI_QC	= 1
};

/*
 * .text+0x31c60, 0x131 = 305 bytes.  Fills `d` from the row of eight
 * file-scope tables selected by `type`: the three counts, then `seq1`,
 * `seq2`, `segmentSize`, `segmentCode` and `dilCode`.
 *
 * IT WRITES ONLY AS FAR AS EACH COUNT SAYS.  `seq1` past `seq1Length`,
 * `seq2` past `seq2Length` and `dilCode` past `dilCount` are LEFT ALONE, so
 * a descriptor handed to this function twice with different `DilType`s keeps
 * the longer one's tail.  That is the object's behaviour and a fixture that
 * zeroes the descriptor first cannot see it (finding F7602).
 *
 * `void`, because the two exits do not agree on `%eax`: the `DIL_TYPE_ADI`
 * arm tail-JUMPS to `edprintf` and the `DIL_TYPE_ADI_QC` arm CALLS it and
 * then returns whatever the epilogue leaves.  The same argument that made
 * `V90Phase4Modulator::setMappingParams` `void` (finding F7431).
 *
 * The descriptor pointer is `P19tagV90DILdescriptor`, non-`const`, and here
 * that is not a curiosity: the function writes through it.
 */
void setDilDescriptor(tagV90DILdescriptor *d, DilType type);

/*
 * The number of phase 3 symbols the DIL sequence in `dil` will occupy, as the
 * sum over its `dilCount` entries of `6 * segmentSize[segment] + 6`, where
 * `segment` is the G.711 segment the entry's code falls in.  Zero for a null
 * descriptor and for an empty one.
 *
 * THAT SUM IS RECOMMENDATION V.90's OWN `Lc = (Hc + 1) * 6`, §8.4.1, over the
 * `N` DIL-segments of §8.3.1 -- `segmentSize` IS the spec's `Hc` and
 * `dilCount` its `N`.  Finding F7601 has the whole correspondence.
 *
 * THE RETURN TYPE IS NOT MANGLED.  The object leaves the sum in %eax and
 * nothing in the function distinguishes signed from unsigned -- the
 * accumulation is `lea 0x6(%esi,%edx,2),%ecx` either way.  `unsigned int` is
 * chosen because every term is a non-negative product of an `unsigned char`,
 * so the sum cannot be negative and cannot overflow 32 bits: the worst case
 * is 255 entries of 6 * 255 + 6, which is 391,680.
 */
unsigned int calculateDilLength(tagV90DILdescriptor *dil, PcmType pcmType);

#endif /* DSPLIB_V90DILDESCRIPTORSETTINGS_H */
