/*
 * V90SessionFlag.h -- the five classes of the V.90 setSessionFlag chain.
 *
 * Reconstructed from dsplibs.o.  `V90Modem::setSessionFlag` fans out to the
 * modulator or the demodulator, each of which fans out to its phase 3 and
 * phase 4 halves; five classes, 274 bytes of code, and every one of them is
 * a store and a call.  They are declared together because they are a closed
 * set only together -- `tools/closure.py` on all five reports CLOSED, and on
 * any four of them does not.
 *
 * ONE HEADER, FIVE CLASSES, DELIBERATELY.  The tree's convention is one class
 * per header, and this batch breaks it because these five are mutually
 * recursive at the DECLARATION level, not just the call level: V90Modem needs
 * V90Modulator and V90Demodulator, and V90Demodulator needs
 * V90Phase3Demodulator and V90Phase4Demodulator, which embed the two
 * modulators.  Splitting them into five headers is five files each including
 * the next, and the first batch that gives any of these classes real weight
 * should do that split rather than grow this file.
 *
 * NO SIZE IS ASSERTED FOR ANY OF THEM, AND THAT IS THE POINT.
 *
 * Every other class in this task was sized from the largest `this`-relative
 * displacement across all its members (finding 215).  Run that scan here and
 * the answers are 0xa948 for V90Phase3Demodulator, 0xa95c for
 * V90Phase4Demodulator, 0x3ba8 for V90Modulator and 0x28230 for
 * V90Demodulator -- and NOT ONE of them is off `this`:
 *
 *     cmpw $0x0,0xa948(%ecx)              %ecx is a V90AutoDigitalImpDetector*,
 *     mov  0xa95c(%eax),%ecx              which is 0xa9b0 bytes (finding 251)
 *     lea  0x28230(%edx,%edx,4),%eax      a scaled index: a table address
 *     lea  0x1388(%esi,%ebp,1),%eax       likewise
 *
 * A displacement scan that does not check the base register produces a number
 * that looks exactly like a measurement.  So the classes below model only the
 * prefix these five methods touch, every unmodelled span is `pad_`, and the
 * test compares the whole seeded slot rather than `sizeof` -- which is a
 * stronger check than a size assertion would have been, because it catches a
 * store anywhere in the slot and not merely one inside a guessed bound.
 *
 * Data member names are invented; the mangling never carries one (finding
 * 226).  `sessionFlag` is named for the method that writes it, as in
 * V90Phase3Modulator.h and V90Phase4Modulator.h.
 */

#ifndef DSPLIB_V90SESSIONFLAG_H
#define DSPLIB_V90SESSIONFLAG_H

#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Phase3Modulator.h"
#include "dsplib/V90Phase4Modulator.h"

/*
 * `mov %edx,0x8(%eax); add $0x34,%eax; jmp V90Phase3Modulator::setSessionFlag`
 *
 * The `add` before the tail call is the whole finding: the phase 3 modulator
 * is EMBEDDED at +0x34, not pointed at.  A pointer would be a load.
 */
class V90Phase3Demodulator {
public:
	void setSessionFlag(unsigned int flag);

	unsigned char pad_00[8];		/* +0x00 not modelled     */
	unsigned int sessionFlag;		/* +0x08                  */
	unsigned char pad_0c[0x28];		/* +0x0c not modelled     */
	V90Phase3Modulator phase3Modulator;	/* +0x34 embedded, 0x398  */
};

/*
 * `mov %edx,(%eax); add $0x50,%eax; jmp V90Phase4Modulator::setSessionFlag`
 *
 * Same shape, and the subobject is the 0x2fac-byte V90Phase4Modulator, so
 * this class is at least 0x2ffc.  That is a floor, not a size.
 */
class V90Phase4Demodulator {
public:
	void setSessionFlag(unsigned int flag);

	unsigned int sessionFlag;		/* +0x00                  */
	unsigned char pad_04[0x4c];		/* +0x04 not modelled     */
	V90Phase4Modulator phase4Modulator;	/* +0x50 embedded, 0x2fac */
};

/*
 * The modulator holds POINTERS where the two demodulators embed:
 * `mov 0x38(%esi),%edx` is a load, and what it loads is passed as `this`.
 */
class V90Modulator {
public:
	void setSessionFlag(unsigned int flag);

	unsigned char pad_00[0x28];		/* +0x00 not modelled     */
	unsigned int sessionFlag;		/* +0x28                  */
	unsigned char pad_2c[0x0c];		/* +0x2c not modelled     */
	V90Phase3Modulator *phase3Modulator;	/* +0x38                  */
	V90Phase4Modulator *phase4Modulator;	/* +0x3c                  */
};

/*
 * Pointers again, and the ONE diagnostic in the chain -- which is emitted
 * BEFORE the flag is stored, so the printed value is the argument and the
 * field still holds the old one at that instant.  `edprintf` gates itself.
 */
class V90Demodulator {
public:
	void setSessionFlag(unsigned int flag);

	unsigned char pad_00[0x30];		/* +0x00 not modelled     */
	unsigned int sessionFlag;		/* +0x30                  */
	unsigned char pad_34[0x1a8];		/* +0x34 not modelled     */
	V90Phase3Demodulator *phase3Demodulator;	/* +0x1dc         */
	V90Phase4Demodulator *phase4Demodulator;	/* +0x1e0         */

	/*
	 * +0x1e4..+0x20b is still nothing, and +0x20c IS A POINTER, added by
	 * the VPcmFloModem batch.  `VPcmFloModem::getV90CpBits` loads the
	 * demodulator out of the modem, loads this, and copies one word
	 * inside what it points at (+0x78 to +0x7c) each time a CP sequence
	 * finishes.  What it points at is not modelled anywhere; see
	 * src/pump/v90/VPcmFloModem.cpp.
	 *
	 * The prefix grew from 0x1e4 to 0x210 and no offset moved.
	 */
	unsigned char pad_1e4[0x28];		/* +0x1e4 not modelled    */
	void *ptr_20c;				/* +0x20c                 */
};

/*
 * The fan-out, and the only branch in the batch: +0x49bc selects which half
 * gets the flag.  0 takes the modulator at +0x00, 1 the demodulator at +0x04,
 * and ANY OTHER VALUE stores the flag and calls nothing -- `test`/`je`, then
 * `dec`/`je`, then fall through to `ret`.
 *
 * The mangling of the constructor is `V90Modem(V90ModemSide, ...)`, so the
 * field's TYPE has a name in the original.  Which of its enumerators is 0 and
 * which is 1 does not, so it is spelled `int` here and the two values are
 * named by what they do, rather than an enum being invented (finding 226).
 */
class V90Modem {
public:
	void setSessionFlag(unsigned int flag);

	V90Modulator *modulator;		/* +0x0000 side == 0      */
	V90Demodulator *demodulator;		/* +0x0004 side == 1      */

	/*
	 * +0x0008 and +0x49b4 were carved out of `pad_08` by the
	 * VPcmFloModem batch, which reaches both through the V90Modem
	 * EMBEDDED in a VPcmFloModem at +0x1758 -- so what that batch reads
	 * as `this + 0x1760` and `this + 0x610c` is this object's +0x08 and
	 * +0x49b4.  Neither is reached by anything in the setSessionFlag
	 * chain; splitting them out moved no offset.
	 *
	 * `phase2Info` is a V90Phase2Info because
	 * `VPcmFloModem::setPhaseIIinfo` fills the pointee's `pcmType`,
	 * `rtd`, `maxTxPower`, `txPowerMeasurementPoint` and `L2` at
	 * V90Phase2Info's own offsets for those five -- see
	 * include/dsplib/V92Phase2Info.h for the rest of that argument.
	 *
	 * `ptr_49b4` is not typed: `VPcmFloModem::getUinfoValue` loads it and
	 * reads a signed short at its +0x20, and nothing else in the blob
	 * that this tree has read touches it.
	 */
	V90Phase2Info *phase2Info;		/* +0x0008                */
	unsigned char pad_0c[0x49a8];		/* +0x000c not modelled   */
	void *ptr_49b4;				/* +0x49b4                */

	unsigned int sessionFlag;		/* +0x49b8                */
	int side;				/* +0x49bc V90ModemSide   */
};

#endif /* DSPLIB_V90SESSIONFLAG_H */
