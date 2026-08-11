/*
 * V92Ja.h -- the V.92 Ja message, PARTIAL.
 *
 * THIS IS NOT A RECONSTRUCTION OF THE CLASS.  The blob carries no
 * `_ZN5V92Ja*` symbol at all -- `readelf -sW` matches `V92Ja` only inside
 * three OTHER symbols' mangled parameter lists,
 * `_ZN12V92ModulatorC1EjP13V92Phase2InfoP5V92JaP19tagV90DILdescriptor...`
 * and `V92Phase3Modulator::reset` -- so nothing here owns its methods and
 * nothing here can derive its size.
 *
 * Exactly two facts are established, and both come from
 * `V92Phase3Modulator::reset` at .text+0x16c41:
 *
 *     16c41:  8b 03           mov    (%ebx),%eax     <- the count, 32 bits
 *     16c43:  8d 53 04        lea    0x4(%ebx),%edx  <- the vector, at +0x04
 *     16c46:  89 56 3c        mov    %edx,0x3c(%esi)
 *     16c49:  89 46 40        mov    %eax,0x40(%esi)
 *
 * The count is UNSIGNED because `V92Phase3Modulator::generateSymbol` uses it
 * as the divisor of a `divl` (.text+0x168c9 and +0x16939) with no sign fixup
 * anywhere near it.  The vector's ELEMENT type is one byte, because the
 * modulator reads it with `movzbl (%ecx,%edx,1)`.
 *
 * ITS LENGTH IS NOT ESTABLISHED and neither is the size of the object.
 * `bits[1]` below is a declaration of where the array starts, not of how long
 * it is; every reader indexes it through the `unsigned char *` the modulator
 * stored, never through this member.  Whoever reconstructs `V92Ja` proper
 * should replace this file outright rather than extend it.
 */

#ifndef DSPLIB_V92JA_H
#define DSPLIB_V92JA_H

class V92Ja {
public:
	unsigned int bitCount;		/* +0x000 entries in `bits`        */
	unsigned char bits[1];		/* +0x004 PARTIAL: length unknown  */
};

#endif /* DSPLIB_V92JA_H */
