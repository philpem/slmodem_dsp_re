/*
 * V90Phase4Demodulator.cpp -- the lifecycle pair, and the object map it came
 * from, asserted.
 *
 * `include/dsplib/V90Phase4Demodulator.h` carries the derivation: where each
 * of the fourteen fields came from, the three exact meetings that fix the
 * embedded subobjects, and the mapping-parameter swap the modulator sees.
 *
 * PLAIN CDECL, `this` AS THE FIRST STACK ARGUMENT (finding 215).  After
 * `sub $0x3c,%esp` and four register saves into the frame, the constructor's
 * twelve incoming words are this 0x40, mappingParams1 0x44, mappingParams2
 * 0x48, demapper 0x4c, cp 0x50, mp 0x54, descrambler 0x58,
 * connectionEvaluator 0x5c, params 0x60, phase3Demodulator 0x64,
 * autoDigitalImpDetector 0x68 and sessionFlag 0x6c.
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

/*
 * The modulator's eighth argument, `mov $0xc,%eax` into outgoing slot 0x20.
 * `V90Modulator` passes the same 0xc from its own constructor, so the two
 * call sites agree and the constant belongs to the modulator's contract
 * rather than to either caller.
 */
#define V90P4D_MODULATOR_ARG8	0xcu

/*
 * `V90Phase4Demodulator::V90Phase4Demodulator` -- 225 bytes at 0x25a20 (C1)
 * and again at 0x25930 (C2).
 *
 * THE EMBEDDED MODULATOR RECEIVES ITS TWO MAPPING-PARAMETER POINTERS
 * SWAPPED, and this is the one claim in the function a careless fixture
 * cannot see.  `mov 0x48(%esp),%edx` -- the SECOND argument -- reaches
 * outgoing slot 0x14, which is the modulator's fifth parameter, and
 * `mov 0x44(%esp),%ebp` -- the FIRST -- reaches slot 0x18, its sixth.  This
 * object stores them the other way round at +0x0c and +0x10, so it really is
 * a crossing.  Finding 1301; the V.90 modulator batch derived the same swap
 * independently from the far side of the call.
 *
 * THREE OF THE MODULATOR'S EIGHT ARGUMENTS ARE NULL AND ONE IS THE LITERAL
 * 12.  `xor %ecx,%ecx` into slot 0x0c, `xor %eax,%eax` into 0x10, another
 * zero into 0x1c and `mov $0xc,%eax` into 0x20: the bits-to-symbol converter,
 * the MP record and the CP record are all null from here, and the trailing
 * count is 0xc.  A phase 4 modulator built by `V90Modulator` gets real
 * pointers in those three slots; this one does not, and its ownership flag
 * comes out the other way as a result.
 *
 * THE SIX MEMBER CALLS ARE THE COMPILER'S.  Three constructions in
 * declaration order here and three destructions in reverse below, with no
 * body statement between any pair -- so the mem-initializer list is what this
 * file writes and the calls are what it must not.
 *
 * `sessionFlag` IS NAMED FOR ITS VALUE, NOT FOR THIS CONSTRUCTOR.
 * `V90Demodulator` passes its own +0x30 -- the field its `setSessionFlag`
 * writes -- and hands the same value to `V90Phase3Demodulator`'s third
 * argument, which that class already calls `sessionFlag`.
 */
V90Phase4Demodulator::V90Phase4Demodulator(V90MappingParams *mp1,
					   V90MappingParams *mp2,
					   V90Demapper *dem, V90CP *cpArg,
					   V90MP *mpArg,
					   Descrambler<unsigned char, int> *dsc,
					   V90ConnectionEvaluator *ce,
					   V90Parameters *par,
					   V90Phase3Demodulator *p3d,
					   V90AutoDigitalImpDetector *adid,
					   unsigned int flag)
	: phase4Modulator(par, flag, 0, 0, mp2, mp1, 0, V90P4D_MODULATOR_ARG8),
	  rDetector1(par), rDetector2(par)
{
	sessionFlag = flag;
	params = par;
	mappingParams1 = mp1;
	mappingParams2 = mp2;
	cp = cpArg;
	mp = mpArg;
	phase3Demodulator = p3d;
	demapper = dem;
	descrambler = dsc;
	connectionEvaluator = ce;
	autoDigitalImpDetector = adid;
}

/*
 * `~V90Phase4Demodulator` -- 52 bytes at 0x25b50 (D1) and 0x25b10 (D2).
 *
 * AN EMPTY BODY, AND THE FIFTY-TWO BYTES ARE ALL THE COMPILER'S: three calls
 * at +0x3028, +0x2ffc and +0x50, in that order, and the frame around them.
 * Nothing is freed -- this object owns no heap -- and nothing is stored, so
 * the eleven pointers it holds are still there after it runs.  A body that
 * cleared any of them would be a real difference and `-fno-lifetime-dse`
 * keeps it visible, which is what the destructor's mutations are for.
 */
V90Phase4Demodulator::~V90Phase4Demodulator()
{
}
