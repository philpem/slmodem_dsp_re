/*
 * V90SessionFlag.cpp -- the five members of the V.90 setSessionFlag chain.
 *
 * `include/dsplib/V90SessionFlag.h` carries the object maps and the argument
 * for why none of the five classes has a size asserted.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215).  Three of
 * the five end in a tail call -- `jmp`, not `call`, with the arguments
 * rewritten into the caller's own outgoing slots -- which is what a call in
 * final position of a `void` function compiles to and needs nothing said in
 * the source to reproduce.
 *
 * THE CHAIN, top down:
 *
 *     V90Modem          +0x49b8, then side 0 -> Modulator, 1 -> Demodulator
 *     V90Modulator      +0x28,   then Phase3Modulator*, Phase4Modulator*
 *     V90Demodulator    edprintf, +0x30, then Phase3Demod*, Phase4Demod*
 *     V90Phase3Demod    +0x08,   then the EMBEDDED Phase3Modulator at +0x34
 *     V90Phase4Demod    +0x00,   then the EMBEDDED Phase4Modulator at +0x50
 */

#include <stddef.h>

#include "dsplib/encode.h"
#include "dsplib/V90SessionFlag.h"

/*
 * Hold the compiler to the maps in the header.  tools/offcheck.py parses only
 * `struct name {` out of include/dsplib, so a C++ class asserts its own.
 * Guarded on a 32-bit pointer because six of the offsets below are pointers
 * and `make check64` lays them out differently -- the layout the blob has is
 * a 32-bit layout, and asserting it on a host that cannot have it is
 * asserting the wrong thing.
 *
 * OFFSETS ONLY, NO SIZES.  See the header: every candidate for a size here
 * turned out to be a displacement off some other object.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define SF_OFF(cls, field, off, tag) \
	typedef char sf_off_##tag[ \
	    ((int)__builtin_offsetof(cls, field) == (off)) ? 1 : -1]

SF_OFF(V90Phase3Demodulator, sessionFlag,		0x0008, p3d_flag);
SF_OFF(V90Phase3Demodulator, phase3Modulator,		0x0034, p3d_mod);
SF_OFF(V90Phase4Demodulator, sessionFlag,		0x0000, p4d_flag);
SF_OFF(V90Phase4Demodulator, phase4Modulator,		0x0050, p4d_mod);
SF_OFF(V90Modulator,	     sessionFlag,		0x0028, mod_flag);
SF_OFF(V90Modulator,	     phase3Modulator,		0x0038, mod_p3);
SF_OFF(V90Modulator,	     phase4Modulator,		0x003c, mod_p4);
SF_OFF(V90Demodulator,	     sessionFlag,		0x0030, dem_flag);
SF_OFF(V90Demodulator,	     phase3Demodulator,		0x01dc, dem_p3);
SF_OFF(V90Demodulator,	     phase4Demodulator,		0x01e0, dem_p4);
SF_OFF(V90Demodulator,	     connectionEvaluator,	0x020c, dem_20c);
SF_OFF(V90Modem,	     modulator,			0x0000, mdm_mod);
SF_OFF(V90Modem,	     demodulator,		0x0004, mdm_dem);
SF_OFF(V90Modem,	     phase2Info,		0x0008, mdm_p2i);
SF_OFF(V90Modem,	     ptr_49b4,			0x49b4, mdm_49b4);
SF_OFF(V90Modem,	     sessionFlag,		0x49b8, mdm_flag);
SF_OFF(V90Modem,	     side,			0x49bc, mdm_side);
#endif /* 32-bit host */

void
V90Phase3Demodulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
	phase3Modulator.setSessionFlag(flag);
}

void
V90Phase4Demodulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
	phase4Modulator.setSessionFlag(flag);
}

void
V90Modulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
	phase3Modulator->setSessionFlag(flag);
	phase4Modulator->setSessionFlag(flag);
}

/*
 * THE DIAGNOSTIC COMES FIRST.  `call edprintf` is at +0x17 and the store to
 * +0x30 at +0x23, so the value printed is the argument and the field still
 * holds the previous flag while it is printed.  Not gated here: `edprintf`
 * gates itself, the same as V90Phase2Info::printInfo's Uinfo line.
 */
void
V90Demodulator::setSessionFlag(unsigned int flag)
{
	edprintf("V90Demodulator: setSessionFlag, flag = %d\r\n", flag);

	sessionFlag = flag;
	phase3Demodulator->setSessionFlag(flag);
	phase4Demodulator->setSessionFlag(flag);
}

/*
 * `mov 0x49bc(%eax),%edx` is read BEFORE the store to +0x49b8, which matters
 * only if the two could alias and they cannot -- they are distinct members of
 * one object.  Written in the order the object reads them anyway.
 *
 * The flag is stored on every path, including the one that calls nothing.
 */
void
V90Modem::setSessionFlag(unsigned int flag)
{
	int which = side;

	sessionFlag = flag;

	if (which == 0)
		modulator->setSessionFlag(flag);
	else if (which == 1)
		demodulator->setSessionFlag(flag);
}
