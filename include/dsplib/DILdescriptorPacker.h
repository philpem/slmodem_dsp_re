/*
 * DILdescriptorPacker.h -- the V.90 DIL descriptor's wire form.
 *
 * `DILdescriptorPacker` is an UNMANGLED `T` symbol in the blob while its
 * neighbours `calculateDilLength` and `V92DILdescriptorPacker` are mangled,
 * so the original declared this one `extern "C"` (or compiled it as C).  The
 * declaration below reproduces that: an ordinary C++ prototype would emit
 * `_Z19DILdescriptorPackerPK19tagV90DILdescriptorPsS2_` and be a different
 * function.
 *
 * The name is the original's, from the symbol.  NOTHING ELSE HERE IS: the
 * symbol is unmangled, so it carries no argument types at all and no
 * parameter names, and every name in this file and in the .cpp is invented
 * and derived from the object's own use of the argument.  The signature is
 * measured, not assumed -- see the .cpp.
 *
 * It is declared in its own header rather than in V90Phase3Modulator.h so
 * that the struct's header, which several batches are merging against, does
 * not have to change to gain a free function.
 */

#ifndef DSPLIB_DILDESCRIPTORPACKER_H
#define DSPLIB_DILDESCRIPTORPACKER_H

#include "dsplib/V90Phase3Modulator.h"

extern "C" {

/**
 * @brief Expand a V.90 DIL descriptor into its framed bit stream.
 *
 * Packs `*desc` one bit per `short`, in the wire form the V.90 downstream
 * sends, and stores the number of bits produced through @p nbits.
 *
 * @param desc   The descriptor to pack.
 * @param bits   Output buffer. Needs 2,654 entries for the largest
 *               descriptor the fields can describe (both sequences 128
 *               long, `dilCount` 255) -- the object bounds nothing, so a
 *               caller supplying less is out of contract.
 * @param nbits  Set to the number of valid entries written to @p bits.
 */
void DILdescriptorPacker(const tagV90DILdescriptor *desc, short *bits,
			 short *nbits);

}

#endif /* DSPLIB_DILDESCRIPTORPACKER_H */
