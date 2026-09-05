/**
 * @file V92DILdescriptorPacker.h
 * @brief The V.92 DIL descriptor's wire form.
 *
 * The signature is forced by the mangling and is not the V.90 one.
 * `_Z22V92DILdescriptorPackerP19tagV90DILdescriptorPhPi` decodes as
 *
 *     V92DILdescriptorPacker(tagV90DILdescriptor *, unsigned char *, int *)
 *
 * which settles three things before a byte of the object is read: the
 * descriptor is not `const` (a `const` would mangle to `PK19...`, a
 * different symbol -- V90DilDescriptorSettings.h's `calculateDilLength`
 * makes the same point); the bit output is one `unsigned char` per bit, not
 * one `short`; and the count is written through an `int *` rather than a
 * `short *` (the blob's store is `mov %ebp,(%edx)`, a full 32-bit word,
 * agreeing).
 *
 * The neighbouring `DILdescriptorPacker` is an unmangled `T` symbol and is
 * declared `extern "C"`; this one is mangled, so it is an ordinary C++ free
 * function and must not be -- an `extern "C"` here would emit the plain name
 * and match nothing.
 *
 * The return type is not mangled, so it is measured rather than assumed:
 * `V92Modem::reset` is the only caller, nothing after either of the
 * function's two `ret`s is set deliberately (one leaves the last CRC stage
 * in %eax, the other leaves whatever the skipped loop left), and a caller
 * wanting the length already has it through the third argument. `void`.
 *
 * The argument names are the original's, not invented: six diagnostics in
 * the function print `pParamObj address`, `BitVector address`, `Cnt
 * address`, and `pParamObj->N`, `->Lsp` and `->Ltp` for the three length
 * fields. The reconstruction keeps this tree's own field names for the
 * struct, which is shared with the V.90 packer, and records the original's
 * spelling here.
 */

#ifndef DSPLIB_V92DILDESCRIPTORPACKER_H
#define DSPLIB_V92DILDESCRIPTORPACKER_H

#include "dsplib/V90Phase3Modulator.h"

/**
 * @brief Expand a V.92 DIL descriptor into its framed wire form, one bit
 *        per output byte.
 * @param desc   The descriptor to expand.
 * @param bits   Destination buffer, one `unsigned char` per bit. Needs
 *               2,688 entries for the largest descriptor the fields can
 *               describe (both sequences 128 long and `dilCount` 255: 17
 *               * (13 + 8 + 8 + 128) = 2,669 for the CRC frame's framing
 *               position, rounded up to 2,688) -- 34 more than the V.90
 *               packer's 2,654, because the V.92 stream carries two extra
 *               frames. The function does not bound-check; a shorter buffer
 *               is out of contract.
 * @param nbits  Receives the number of bits written.
 */
void V92DILdescriptorPacker(tagV90DILdescriptor *desc, unsigned char *bits,
			    int *nbits);

#endif /* DSPLIB_V92DILDESCRIPTORPACKER_H */
