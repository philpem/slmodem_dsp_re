/*
 * V92DILdescriptorPacker.h -- the V.92 DIL descriptor's wire form.
 *
 * THE SIGNATURE IS FORCED BY THE MANGLING, and it is not the V.90 one.
 * `_Z22V92DILdescriptorPackerP19tagV90DILdescriptorPhPi` decodes as
 *
 *     V92DILdescriptorPacker(tagV90DILdescriptor *, unsigned char *, int *)
 *
 * so three things are settled before a byte of the object is read: the
 * descriptor is NOT const (`P19...`, where a `const` would give `PK19...`,
 * emit a different symbol, and link against nothing -- V90DilDescriptorSettings.h makes the
 * same point about `calculateDilLength`); the bit output is one `unsigned
 * char` per bit, not one `short`; and the count is written through an `int *`
 * rather than a `short *`.  The blob's store is `mov %ebp,(%edx)`, a full
 * 32-bit word, which agrees.
 *
 * The neighbouring `DILdescriptorPacker` is an UNMANGLED `T` symbol and is
 * therefore declared `extern "C"`.  This one is mangled, so it is an ordinary
 * C++ free function and must NOT be -- an `extern "C"` here emits the plain
 * name and matches nothing.
 *
 * The return type is not mangled and so is measured: `V92Modem::reset` is the
 * only caller, nothing after the two `ret`s is set deliberately (one leaves
 * the last CRC stage in %eax, the other leaves whatever the skipped loop
 * left), and a caller wanting the length has it through the third argument.
 * `void`.
 *
 * THE ARGUMENT NAMES ARE THE ORIGINAL'S, not invented.  Six diagnostics in
 * the function print them: `pParamObj address`, `BitVector address`, `Cnt
 * address`, and then `pParamObj->N`, `->Lsp` and `->Ltp` for the three length
 * fields.  The reconstruction keeps this tree's own field names for the
 * struct, which is shared with the V.90 packer, and records the original's
 * spelling here.
 */

#ifndef DSPLIB_V92DILDESCRIPTORPACKER_H
#define DSPLIB_V92DILDESCRIPTORPACKER_H

#include "dsplib/V90Phase3Modulator.h"

/*
 * Expand `desc` into the framed bit stream the V.92 downstream sends it as,
 * one bit per byte, and store the number of bits through `nbits`.
 *
 * `bits` needs 2,688 entries for the largest descriptor the fields can
 * describe -- both sequences 128 long and `dilCount` 255, which give
 * 17 * (13 + 8 + 8 + 128) = 2,669 for the CRC frame's framing position and a
 * count of 2,688.  That is 34 more than the V.90 packer's 2,654, because the
 * V.92 stream carries two extra frames.  The object bounds nothing, so a
 * caller supplying less is out of contract.
 */
void V92DILdescriptorPacker(tagV90DILdescriptor *desc, unsigned char *bits,
			    int *nbits);

#endif /* DSPLIB_V92DILDESCRIPTORPACKER_H */
