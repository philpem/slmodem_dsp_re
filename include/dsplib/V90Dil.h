/*
 * V90Dil.h -- the free functions over a V.90 DIL descriptor.
 *
 * One so far.  `calculateDilLength` is a free function, not a member -- its
 * mangling is `_Z18calculateDilLengthP19tagV90DILdescriptor7PcmType`, with no
 * class component -- so it needs a home of its own rather than a place in
 * V90Phase3Modulator, whose header already declares the descriptor and the
 * companding enum this takes.
 *
 * The descriptor argument is NOT const in the original: the mangling says
 * `P19tagV90DILdescriptor`, where `V90Phase3Modulator::resetDILGenerator`
 * says `PK19tagV90DILdescriptor`.  The function reads and never writes, so
 * the missing `const` is the author's and reproducing it is not optional --
 * a `const` here emits a different symbol that links against nothing
 * (docs/v90cpp.md).
 */

#ifndef DSPLIB_V90DIL_H
#define DSPLIB_V90DIL_H

/* For `tagV90DILdescriptor` and `PcmType`.  Included, never edited. */
#include "dsplib/V90Phase3Modulator.h"

/*
 * The number of phase 3 symbols the DIL sequence in `dil` will occupy, as the
 * sum over its `dilCount` entries of `6 * segmentSize[segment] + 6`, where
 * `segment` is the G.711 segment the entry's code falls in.  Zero for a null
 * descriptor and for an empty one.
 *
 * THE RETURN TYPE IS NOT MANGLED.  The object leaves the sum in %eax and
 * nothing in the function distinguishes signed from unsigned -- the
 * accumulation is `lea 0x6(%esi,%edx,2),%ecx` either way.  `unsigned int` is
 * chosen because every term is a non-negative product of an `unsigned char`,
 * so the sum cannot be negative and cannot overflow 32 bits: the worst case
 * is 255 entries of 6 * 255 + 6, which is 391,680.
 */
unsigned int calculateDilLength(tagV90DILdescriptor *dil, PcmType pcmType);

#endif /* DSPLIB_V90DIL_H */
