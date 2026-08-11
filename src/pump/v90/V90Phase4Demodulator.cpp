/*
 * V90Phase4Demodulator.cpp -- the object map, asserted, and nothing else.
 *
 * `include/dsplib/V90Phase4Demodulator.h` carries the derivation: where each
 * of the fourteen fields came from, the three exact meetings that fix the
 * embedded subobjects, the mapping-parameter swap the modulator sees, and why
 * the constructor and destructor are declared nowhere yet.
 *
 * THIS FILE DEFINES NO FUNCTION, ON PURPOSE.  `V90Phase4Demodulator`'s
 * constructor and destructor both call `V90Phase4Modulator`'s, and neither of
 * those exists in this tree -- so the two members that would go here cannot
 * be written, cannot link and cannot be differentially tested, and this tree
 * does not commit a function that has not passed a test.
 *
 * WHAT A FILE OF ASSERTIONS IS FOR is that it turns the header's arithmetic
 * into something the compiler checks.  The three meetings below are the whole
 * reason the map is believable, and each of them depends on a size this file
 * does not own:
 *
 *     0x0050 + sizeof(V90Phase4Modulator) == 0x2ffc     (0x2fac)
 *     0x2ffc + sizeof(V90RDetector)       == 0x3028     (0x002c)
 *     0x3028 + sizeof(V90RDetector)       == 0x3054     (0x002c)
 *
 * If either class's own size ever moves -- and both are bounds derived from
 * displacement scans, so both may -- the two `offsetof`s on the far side stop
 * agreeing and this file stops compiling.  Recording it in a comment instead
 * would have let the map rot silently.
 */

#include <stddef.h>

#include "dsplib/V90Phase4Demodulator.h"

/*
 * Guarded on a 32-bit pointer because `check64` compiles the same source for
 * a host whose pointers are eight bytes, where none of these offsets can
 * hold.  Same shape as V90Demapper.cpp and V90Phase3Demodulator.cpp.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define P4D_OFF(field, off, tag) \
	typedef char v90p4d_off_##tag[ \
	    ((int)__builtin_offsetof(V90Phase4Demodulator, field) == (off)) \
	    ? 1 : -1]

P4D_OFF(sessionFlag,		0x0000, sessionflag);
P4D_OFF(params,			0x0004, params);
P4D_OFF(mappingParams1,		0x000c, mp1);
P4D_OFF(mappingParams2,		0x0010, mp2);
P4D_OFF(cp,			0x0014, cp);
P4D_OFF(mp,			0x0018, mp);
P4D_OFF(phase3Demodulator,	0x001c, p3d);
P4D_OFF(phase4Modulator,	0x0050, p4mod);
P4D_OFF(rDetector1,		0x2ffc, rdet1);
P4D_OFF(rDetector2,		0x3028, rdet2);
P4D_OFF(demapper,		0x3054, demapper);
P4D_OFF(descrambler,		0x3058, descrambler);
P4D_OFF(connectionEvaluator,	0x34f8, conneval);
P4D_OFF(autoDigitalImpDetector,	0x3514, adid);

/*
 * The allocation, and finding 1107's point again: this number is
 * `movl $0x351c,(%esp); call sysdep_malloc` at 0x1c8fe inside
 * `V90Demodulator::V90Demodulator`, not the highest displacement any
 * V90Phase4Demodulator symbol uses -- which would have said 0x3518.
 */
typedef char v90p4d_size[(sizeof(V90Phase4Demodulator) == 0x351c) ? 1 : -1];

/*
 * And the two sizes the three meetings rest on, named so that a failure says
 * WHICH class moved rather than only that an offset is wrong.
 */
typedef char v90p4d_modsize[(sizeof(V90Phase4Modulator) == 0x2fac) ? 1 : -1];
typedef char v90p4d_rdetsize[(sizeof(V90RDetector) == 0x2c) ? 1 : -1];

#endif
