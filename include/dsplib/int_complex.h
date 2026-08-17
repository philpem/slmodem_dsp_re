/*
 * int_complex.h -- the point the visual diagnostics API hands back.
 *
 * THE NAME IS THE OBJECT'S OWN, out of the mangling of the nine members that
 * take one:
 *
 *     _ZN12VPcmFloModem16getConstellationEP11int_complexm
 *      -> VPcmFloModem::getConstellation(int_complex *, unsigned long)
 *
 * `struct` rather than `class` is want of evidence -- the mangling carries no
 * class-key -- and it changes nothing: both members are public data either
 * way.  The `11` in `P11int_complex` is the identifier's length, so the
 * spelling is exact.
 *
 * ---------------------------------------------------------------------------
 * THE LAYOUT IS MEASURED.  THE MEMBER NAMES ARE THE TYPE'S OWN NAME.
 *
 * Every writer in the object indexes an array of these by 8 and writes two
 * 32-bit slots, at +0 and +4:
 *
 *     7324:  mov %eax,(%edi,%edx,8)          the V.34 constellation arm
 *     7327:  mov %esi,0x4(%edi,%edx,8)
 *     f4b2:  mov %eax,(%ecx,%esi,8)          VPcmFloModem::getConstellation
 *     f4bc:  fistpl 0x4(%ecx,%esi,8)
 *
 * so the record is two ints and eight bytes, with no padding to argue about.
 *
 * WHICH SLOT IS REAL AND WHICH IS IMAGINARY IS NOT SEPARATELY ESTABLISHED,
 * and cannot be by any instruction: both members are ints and no code in the
 * object reads one back.  What IS established is the ORDER the halves arrive
 * in.  `VPcmV34GetVisualDiagnostics`'s V.34 constellation arm copies
 * `v34_object::hist_2aa8[i]`, which v34fsk.h has as `short [2]` and describes
 * as "a complex buffer being written with a real value" -- element [0] to the
 * slot at +0, element [1] to the slot at +4:
 *
 *     7310:  movswl 0x2aa8(%ecx,%edx,4),%eax     hist_2aa8[i][0]
 *     7318:  movswl 0x2aaa(%ecx,%edx,4),%esi     hist_2aa8[i][1]
 *
 * So the names below are the conventional reading of the type's own name,
 * applied in the object's own order.  They are recorded as that rather than
 * as a measurement, and nothing in this tree depends on which is which:
 * `re` and `im` name the same two words `+0` and `+4` name, and every
 * differential test compares the bytes.
 *
 * WHAT THE TWO SLOTS ACTUALLY CARRY VARIES BY DIAGNOSTIC, which is worth
 * knowing before reading either as a complex sample:
 *
 *   constellation      both, and both meaningful
 *   linear equaliser   `re` is 0 and the coefficient is in `im`
 *   DFE                the same
 *   resampler phase    the value is in `re` and `im` is 0
 *   resampler offset   the same
 *
 * The two equaliser getters putting a REAL coefficient in the second slot is
 * recorded as found; V.90's downstream signal is real, so there is no
 * imaginary part for the taps to occupy and the author picked a slot.
 */

#ifndef DSPLIB_INT_COMPLEX_H
#define DSPLIB_INT_COMPLEX_H

struct int_complex {
	int	re;			/* +0x00 */
	int	im;			/* +0x04 */
};

#endif /* DSPLIB_INT_COMPLEX_H */
