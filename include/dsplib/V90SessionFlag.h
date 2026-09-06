/**
 * @file V90SessionFlag.h
 * @brief The remaining classes of the V.90 `setSessionFlag` chain.
 *
 * `V90Modem::setSessionFlag` fans out to the modulator or the demodulator,
 * each of which fans out to its phase 3 and phase 4 halves; five classes,
 * 274 bytes of code, and every one of them is a store and a tail call. They
 * were declared together originally because they form a closed set only
 * together -- `tools/closure.py` on all five reports CLOSED, and on any
 * four of them does not.
 *
 * The tree's convention is one class per header, and this file broke it
 * because the five are mutually recursive at the declaration level, not
 * just the call level: `V90Modem` needs `V90Modulator` and `V90Demodulator`,
 * and `V90Demodulator` needs `V90Phase3Demodulator` and
 * `V90Phase4Demodulator`, which embed the two modulators. The rule adopted
 * here was that the first class to earn real weight (a full field map, not
 * just this chain's one method) should be split out rather than grown in
 * place, and later waves did that for two of the five: `V90Phase3Demodulator`
 * is now in V90Phase3Demodulator.h and `V90Demodulator` in
 * V90Demodulator.h, both included below with their sizes settled and their
 * field maps filled in. Nothing recorded here was dropped in the move --
 * see the historical notes further down for what carried over.
 *
 * The three classes still declared in this file deliberately assert no
 * size. Every other class in this project was sized from the largest
 * `this`-relative displacement across all its members (finding F215), but
 * that scan does not check the base register, and running it naively here
 * gives numbers that are not `this`-relative at all:
 *
 *     cmpw $0x0,0xa948(%ecx)              %ecx is a V90AutoDigitalImpDetector*,
 *     mov  0xa95c(%eax),%ecx              which is 0xa9b0 bytes (finding F251)
 *     lea  0x28230(%edx,%edx,4),%eax      a scaled index: a table address
 *     lea  0x1388(%esi,%ebp,1),%eax       likewise
 *
 * So the classes below model only the prefix these five methods actually
 * touch, every unmodelled span is `pad_`, and the tests compare the whole
 * seeded slot rather than `sizeof` -- a stronger check than a size assertion
 * would have been, since it catches a store anywhere in the slot rather
 * than only one inside a guessed bound.
 *
 * What does settle a size here is the allocation, not a displacement scan:
 * every one of these classes is built on the heap, and the `sysdep_malloc`
 * immediately before its constructor call gives the size (finding F291).
 * That is how `V90Phase3Demodulator`'s 0x42c and `V90Demodulator`'s 0x298
 * were obtained, and it is why those two now assert their sizes while the
 * three below do not -- nobody has yet found the allocation for
 * `V90Modulator`, `V90Modem` or `V90Phase4Demodulator`. `V90Modem`'s
 * constructor is the place to look; it allocates 0x42d8 for something just
 * before the 0x298 it hands the demodulator.
 *
 * Data member names are invented; the mangling never carries one (finding
 * F226). `sessionFlag` is named for the method that writes it, as in
 * V90Phase3Modulator.h and V90Phase4Modulator.h.
 */

#ifndef DSPLIB_V90SESSIONFLAG_H
#define DSPLIB_V90SESSIONFLAG_H

#include "dsplib/V90Demodulator.h"
#include "dsplib/V90Modem.h"
#include "dsplib/V90Modulator.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Phase3Demodulator.h"
#include "dsplib/V90Phase3Modulator.h"
#include "dsplib/V90Phase4Modulator.h"

/*
 * `V90Phase3Demodulator` used to be declared here, as
 *
 *     pad_00[8]; sessionFlag; pad_0c[0x28]; V90Phase3Modulator at +0x34
 *
 * from `mov %edx,0x8(%eax); add $0x34,%eax; jmp
 * V90Phase3Modulator::setSessionFlag`. The `add` before the tail call was
 * the finding: the phase 3 modulator is embedded at +0x34, not pointed at,
 * since a pointer would be a load. V90Phase3Demodulator.h keeps that
 * sentence and fills the three `pad_` spans in.
 *
 * `V90Demodulator` likewise moved to V90Demodulator.h. It was the one
 * diagnostic in the chain -- emitted before the flag is stored, so the
 * printed value is the argument and the field still holds the old one at
 * that instant.
 */

/**
 * @brief Partial, duplicate model of the V.90 phase 4 demodulator; see the
 *        note above the class for why a second definition exists.
 *
 * `mov %edx,(%eax); add $0x50,%eax; jmp V90Phase4Modulator::setSessionFlag`
 * -- the same shape as the other four classes in this chain, and the
 * subobject is the 0x2fac-byte `V90Phase4Modulator`, so this class is at
 * least 0x2ffc bytes. That is a floor, not a size.
 */
class V90Phase4Demodulator {
public:
	/**
	 * @brief Store the session-negotiation flag and tail-call into the
	 *        embedded phase 4 modulator's own `setSessionFlag`.
	 * @param flag  The session flag value to propagate.
	 */
	void setSessionFlag(unsigned int flag);

	unsigned int sessionFlag;		/* +0x00                  */
	unsigned char pad_04[0x34];		/* +0x04 not modelled     */

	/*
	 * +0x38 and +0x3c: a pair carved out of `pad_04` because
	 * `VPcmFloModem::v90RunDemodulator`'s MPnot arm reads both, and this
	 * partial model is the definition that reaches that translation
	 * unit. Same offsets, same spelling and same derivation as
	 * include/dsplib/V90Phase4Demodulator.h's fuller model: always
	 * written together, `resetBeforRRN` and `detectRRN` set both to 1
	 * and `reset` sets +0x38 to 1 and +0x3c to 0.
	 *
	 * This is the duplicate `tools/onedef.py` carries, and carving a pad
	 * out of one half of it makes the two models agree on more, not
	 * less. The right repair is still to delete this class and include
	 * the fuller header; finding F7584 records what that costs and why
	 * this batch did not take it.
	 */
	int int_0038;
	int int_003c;

	unsigned char pad_40[0x10];		/* +0x40 not modelled     */
	V90Phase4Modulator phase4Modulator;	/* +0x50 embedded, 0x2fac */
};

/*
 * `V90Modulator` was declared here too, as
 *
 *     pad_00[0x28]; sessionFlag; pad_2c[0x0c]; two modulator pointers
 *
 * from `mov 0x38(%esi),%edx` being a load where the two demodulators embed:
 * what it loads is passed as `this`, so the phase blocks are pointed at and
 * not contained. V90Modulator.h keeps that sentence, fills the object in to
 * its full 0x70 bytes, and keeps all three of the names asserted below.
 */

/*
 * `V90Modem` moved to include/dsplib/V90Modem.h, whole, once its
 * construction path was written. It used to be declared here as
 *
 *     modulator; demodulator; phase2Info; pad_0c[0x49a8]; params;
 *     sessionFlag; side
 *
 * with `side` spelled `int` and `pad_0c` unmodelled. `V90Modem::V90Modem`
 * names every field in that span, so the class earned real weight and this
 * file's own splitting rule applied to it as it already had to
 * `V90Demodulator` and `V90Phase3Demodulator`. The one thing that was about
 * `setSessionFlag` rather than layout stays here:
 *
 * +0x49bc selects which half gets the flag. 0 takes the modulator at +0x00,
 * 1 the demodulator at +0x04, and any other value stores the flag and calls
 * nothing -- `test`/`je`, then `dec`/`je`, then fall through to `ret`. The
 * constructor's switch has exactly that shape at exactly that field, which is
 * why `side` is now a `V90ModemSide` rather than an `int`: the destructor
 * compares it unsigned (`cmpl $0x1,0x49bc(%esi); jbe`), and V90Modem.h
 * carries that argument. `int which = side;` below is unaffected.
 */

#endif /* DSPLIB_V90SESSIONFLAG_H */
