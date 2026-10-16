/**
 * @file V90SessionFlag.h
 * @brief Compatibility includes for the V.90 session-flag propagation chain.
 *
 * `V90Modem::setSessionFlag` selects the modulator or demodulator, which
 * forwards the flag to its phase 3 and phase 4 halves. These classes were
 * first modelled together from that closed call chain (finding F226).
 * Their complete definitions now live in their respective headers.
 *
 * In particular, V90Phase4Demodulator.h is the single layout authority for
 * the phase 4 demodulator, including the embedded modulator at +0x50.
 * The prefix model formerly here ended at +0x2ffc; the constructor's
 * allocation establishes the complete object's size as 0x351c. See
 * docs/phase4-type-consolidation.md for the inclusion and code-generation
 * checks that retired the duplicate recorded in finding F7584.
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
#include "dsplib/V90Phase4Demodulator.h"

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
 * carries that argument.
 */

#endif /* DSPLIB_V90SESSIONFLAG_H */
