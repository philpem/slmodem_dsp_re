/**
 * @file V90DilDescriptorSettings.h
 * @brief The free functions over a V.90 DIL (Digital Impairment Learning)
 *        descriptor: filling one from the built-in ADI/ADI_QC tables and
 *        computing how long its sequence runs.
 *
 * The blob's own name for this translation unit is
 * `V90DilDescriptorSettings.cpp` (`STT_FILE` #244 of 283), corroborated by
 * `setDilDescriptor`'s two `edprintf` messages, both of which begin
 * `"V90DilDescriptorSettings: "` where every other diagnostic in this area
 * names a class and a member instead ("V90Modem Reset: ...") -- these name
 * the file because there is no class to name (finding F7600).
 *
 * The translation unit is four free functions and eight tables, contiguous
 * in `.text`:
 *
 *     0x31c60  setDilDescriptor(tagV90DILdescriptor *, DilType)   0x131
 *     0x31da0  getSegmentPointer(PcmType, int)                    0x073
 *     0x31e20  calculateDilLength(DilType, PcmType)               0x0f1
 *     0x31f20  calculateDilLength(tagV90DILdescriptor *, PcmType) 0x0c4
 *
 * Membership is proved, not inferred, for two of the four: the eight tables
 * are LOCAL `.data` symbols, referenceable only from their own translation
 * unit, so `setDilDescriptor` (which reads all eight) and
 * `calculateDilLength(DilType, PcmType)` (which reads `N`, `TO` and `H`) are
 * necessarily in one file together. The other two join by adjacency and the
 * overload relationship. `getSegmentPointer` and the `DilType` overload of
 * `calculateDilLength` are still unwritten; both are unreachable from any
 * entry point (`tools/service.py --list none`), so neither blocks a link.
 *
 * `calculateDilLength`'s descriptor argument is deliberately not `const`:
 * its mangling is `P19tagV90DILdescriptor`, where
 * `V90Phase3Modulator::resetDILGenerator` takes the same struct as
 * `PK19tagV90DILdescriptor`. The function only reads it, so the missing
 * `const` is the author's, and reproducing it is not optional -- adding one
 * here emits a different mangled symbol that links against nothing
 * (docs/v90cpp.md).
 */

#ifndef DSPLIB_V90DILDESCRIPTORSETTINGS_H
#define DSPLIB_V90DILDESCRIPTORSETTINGS_H

/* For `tagV90DILdescriptor` and `PcmType`.  Included, never edited. */
#include "dsplib/V90Phase3Modulator.h"

/*
 * Named `DilType` from its mangling (`7DilType`, a plain enum at namespace
 * scope); the enumerator values are the object's own, read off the two
 * `edprintf` messages at the end of `setDilDescriptor` ("... option ADI." /
 * "... option ADI_QC."), with only the `DIL_TYPE_` prefix ours, following
 * `PcmType`/`PCM_TYPE_*` in V90Phase3Modulator.h (finding F226).
 *
 * ADI is the Automatic Digital Impairment learning descriptor and ADI_QC the
 * quick-connect one; `V90Modem::reset` chooses between them on its `qcFlag`
 * argument and nothing else in the object constructs a `DilType`.  The two
 * differ in the lengths only -- see the table comment in
 * src/pump/v90/V90DilDescriptorSettings.cpp.
 */
enum DilType {
	DIL_TYPE_ADI	= 0,
	DIL_TYPE_ADI_QC	= 1
};

/**
 * @brief Fill a DIL descriptor from the built-in ADI / ADI_QC tables.
 *
 * Fills `d` from the row of eight file-scope tables selected by `type`: the
 * three counts, then `seq1`, `seq2`, `segmentSize`, `segmentCode` and
 * `dilCode`.
 *
 * It writes only as far as each count says: `seq1` past `seq1Length`, `seq2`
 * past `seq2Length` and `dilCode` past `dilCount` are left alone, so a
 * descriptor handed to this function twice with different `DilType`s keeps
 * the longer one's tail (finding F7602). `segmentSize` and `segmentCode` are
 * the only two filled unconditionally, all eight entries every time.
 *
 * The function is `void` because the two exits do not agree on `%eax`: the
 * `DIL_TYPE_ADI` arm tail-jumps to `edprintf` and the `DIL_TYPE_ADI_QC` arm
 * calls it and then returns whatever the epilogue leaves -- the same
 * argument that made `V90Phase4Modulator::setMappingParams` `void` (finding
 * F7431).
 *
 * @param d     The descriptor to fill. Not `const`, because the function
 *              writes through it -- the caller's declaration must match.
 * @param type  Which built-in table row to copy in.
 */
void setDilDescriptor(tagV90DILdescriptor *d, DilType type);

/*
 * The other two free functions of the TU, written by the VPcmV34Main leaf
 * pass: the boundary search standalone (0x31da0) and the table-driven
 * length (0x31e20).  Both return `unsigned int` -- the search's own counter
 * and the accumulator, each moved to %eax whole.  `PcmType` comes from
 * V90Phase3Modulator.h, which this header already relies on for the
 * descriptor overload below.
 */

/**
 * @brief Find which G.711 segment a PCM level falls in.
 *
 * @param pcmType  mu-law or A-law, selecting the boundary table and its
 *                 length (eight boundaries under mu-law, seven under A-law).
 * @param level    The PCM code point to place.
 * @return The index of the first boundary `>= level`, or the row length if
 *         `level` is above every boundary.
 */
unsigned int getSegmentPointer(PcmType pcmType, int level);

/**
 * @brief Length, in phase 3 symbols, of the built-in ADI/ADI_QC DIL sequence.
 *
 * Runs the descriptor overload's own algorithm over this file's built-in
 * tables instead of a caller-supplied descriptor.
 *
 * @param type     Which built-in table row to sum.
 * @param pcmType  mu-law or A-law, selecting the segment-boundary table used
 *                 to classify each ucode.
 * @return The DIL sequence length in phase 3 symbols.
 */
unsigned int calculateDilLength(DilType type, PcmType pcmType);

/**
 * @brief Length, in phase 3 symbols, of the DIL sequence in a descriptor.
 *
 * The number of phase 3 symbols the DIL sequence in `dil` will occupy, as
 * the sum over its `dilCount` entries of `6 * segmentSize[segment] + 6`,
 * where `segment` is the G.711 segment the entry's code falls in. Zero for
 * a null descriptor and for an empty one.
 *
 * That sum is Recommendation V.90's own `Lc = (Hc + 1) * 6`, §8.4.1, over
 * the `N` DIL-segments of §8.3.1 -- `segmentSize` is the spec's `Hc` and
 * `dilCount` its `N` (finding F7601 has the whole correspondence).
 *
 * The return type is not mangled: the object leaves the sum in `%eax` and
 * nothing in the function distinguishes signed from unsigned. `unsigned int`
 * is chosen because every term is a non-negative product of an
 * `unsigned char`, so the sum cannot be negative and cannot overflow 32
 * bits -- the worst case is 255 entries of `6 * 255 + 6`, 391,680.
 *
 * @param dil      The descriptor to measure.
 * @param pcmType  mu-law or A-law, selecting the segment-boundary table used
 *                 to classify each ucode.
 * @return The DIL sequence length in phase 3 symbols.
 */
unsigned int calculateDilLength(tagV90DILdescriptor *dil, PcmType pcmType);

#endif /* DSPLIB_V90DILDESCRIPTORSETTINGS_H */
