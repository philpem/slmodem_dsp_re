/**
 * @file V92Ja.h
 * @brief The V.92 Ja message -- PARTIAL, not a reconstruction of the class.
 *
 * The blob carries no `_ZN5V92Ja*` symbol at all: `V92Ja` appears only
 * inside two other symbols' mangled parameter lists,
 * `V92Modulator::V92Modulator(unsigned, V92Phase2Info*, V92Ja*,
 * tagV90DILdescriptor*, ...)` and `V92Phase3Modulator::reset`. So nothing
 * here owns its own methods, and nothing here can derive its size.
 *
 * Exactly two fields are established, both from `V92Phase3Modulator::reset`
 * (.text+0x16c41):
 *
 *     16c41:  8b 03           mov    (%ebx),%eax     <- the count, 32 bits
 *     16c43:  8d 53 04        lea    0x4(%ebx),%edx  <- the vector, at +0x04
 *     16c46:  89 56 3c        mov    %edx,0x3c(%esi)
 *     16c49:  89 46 40        mov    %eax,0x40(%esi)
 *
 * The count is unsigned: `V92Phase3Modulator::generateSymbol` uses it as the
 * divisor of a `divl` (.text+0x168c9 and +0x16939) with no sign fixup nearby.
 * The vector's element type is one byte, since the modulator reads it with
 * `movzbl (%ecx,%edx,1)`.
 *
 * The vector's length is not established, and neither is the size of the
 * object as a whole. `bits[1]` below marks where the array starts, not how
 * long it runs; every reader indexes it through the `unsigned char *` the
 * modulator stored, never through this member. Whoever reconstructs `V92Ja`
 * proper should replace this file outright rather than extend it.
 */

#ifndef DSPLIB_V92JA_H
#define DSPLIB_V92JA_H

class V92Ja {
public:
	unsigned int bitCount;		/* +0x000 entries in `bits` (unsigned: used as a divl divisor) */
	unsigned char bits[1];		/* +0x004 PARTIAL: array start only, true length unknown */
};

#endif /* DSPLIB_V92JA_H */
