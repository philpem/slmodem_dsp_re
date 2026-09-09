/*
 * VPcmXfTerm.cpp -- `VPCMXF_SessionTermination`, the V.PCM interface's
 * session-termination hook.
 *
 * Reconstructed from dsplibs.o, 0xf730, NINETEEN BYTES: one load through the
 * handle and a tail jump.
 *
 *     mov 0x4(%esp),%edx
 *     mov 0x175c(%edx),%eax
 *     mov %eax,0x4(%esp)
 *     jmp V90Demodulator::sessionTermination
 *
 * +0x175c IS `modem.demodulator`, and that is not a new measurement: a
 * V90Modem is embedded in a VPcmFloModem at +0x1758 and `demodulator` is its
 * +0x04 (include/dsplib/VPcmFloModem.h's three-way argument, and
 * `VpcmFloModem.cpp`'s `VPCM_OFF(modem.demodulator, 0x175c, ...)`).  So the
 * handle the V.PCM interface passes around is a `VPcmFloModem *`; the ONE
 * thing this function adds to that map is that the offset is reached from
 * outside the class, by a C entry point, and the test pins it by putting a
 * different object at +0x1758 and +0x1760 and requiring both to be untouched.
 *
 * THE RETURN TYPE IS NOT RECOVERABLE AND IS WRITTEN `void`.
 * `V90Demodulator::sessionTermination` returns `int`, and GCC compiles both
 * `void f(T *p) { p->d->g(); }` and `int f(T *p) { return p->d->g(); }` to
 * the same `jmp` -- the sibling call is taken either way, so the object's
 * bytes do not choose.  The one caller, `vpcm_delete` (0x3dd0), does
 * `call VPCMXF_SessionTermination` and then immediately reloads its own
 * pointer for `VPCMXF_Delete` without looking at `%eax`, so the call site
 * does not choose either.  `void` is written because that is what the caller
 * uses it as, and the ambiguity is recorded here rather than hidden.
 *
 * WHY THIS IS ITS OWN TRANSLATION UNIT.  In the object it is not: 0xf730 sits
 * between `VPCMXF_Delete` (0xf6c0) and `VPcmFloModem::qcLineVerification`
 * (0xf750), so the original compiled it beside the class.  The split here is
 * finding F1264's rule -- one source file is one mutation suite's namespace --
 * applied ahead of the collision rather than after it: `VpcmFloModem.cpp`
 * already carries two suites (`vpcmflomodem`, `vpcmep3`), and the rest of the
 * `VPCMXF_` family is unwritten and will want its own anchors.  The reason is
 * about the tier and not about the object, which is why it is stated.
 *
 * `sessionTermination` IS NAMED BY ITS MANGLED SYMBOL rather than called as a
 * member, for `V90Demodulator.cpp`'s reason at its own `v90resampler_reset`:
 * `V90Demodulator.h` drags in `V90PreFilter.h` and with it the OTHER
 * definition of `V90Parameters` (finding F1112), which this file cannot have
 * beside `VPcmFloModem.h`.  A forward-declared pointer type and the symbol
 * are enough, and the call is direct in the object too -- there is no vptr in
 * either class.
 */

#include <stddef.h>

#include "dsplib/VPcmFloModem.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char vpcmxfterm_off_dem[
    ((int)__builtin_offsetof(VPcmFloModem, modem.demodulator) == 0x175c)
    ? 1 : -1];
#endif

/*
 * `V90Demodulator::sessionTermination`, by the name the ABI gives it.  It
 * takes `this` as its first stack argument like every other member here
 * (finding F215), so no attribute is involved.
 */
extern int v90dem_sessionTermination(V90Demodulator *self)
	asm("_ZN14V90Demodulator18sessionTerminationEv");

/*
 * No header carries this prototype yet: the only caller in the object is
 * `vpcm_delete`, which is not written here, and a declaration nobody needs is
 * a claim nobody checked (VPcmFloModem.h's own rule).  It is declared here so
 * that the definition below is not the first declaration.
 */
extern "C" void VPCMXF_SessionTermination(VPcmFloModem *self);

extern "C" void
VPCMXF_SessionTermination(VPcmFloModem *self)
{
	v90dem_sessionTermination(self->modem.demodulator);
}
