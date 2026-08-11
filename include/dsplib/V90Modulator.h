/*
 * V90Modulator.h -- the V.90 downstream modulator, the top of the chain.
 *
 * Reconstructed from dsplibs.o.  Seventeen members and 3,354 bytes of code, of
 * which the constructor, the destructor and `setSessionFlag` are written;
 * `progress`, `reset` and the twelve phase transitions are not.
 *
 * WHERE THIS CLASS USED TO LIVE.  A partial map -- `pad_00[0x28]`,
 * `sessionFlag`, `pad_2c[0x0c]`, then the two modulator pointers -- was
 * declared in `V90SessionFlag.h`, derived from `mov 0x38(%esi),%edx` being a
 * LOAD where the two demodulators embed their phase blocks.  It has moved here
 * and been filled in, exactly as `V90Phase3Demodulator` and `V90Demodulator`
 * moved out of that file before it; `src/pump/v90/V90SessionFlag.cpp` asserts
 * the three offsets it named and they are unchanged.
 *
 * NOT POLYMORPHIC: `~V90Modulator` is listed with `D1` and `D2` and no `D0`,
 * so offset 0 is a real member and there is no vptr.
 *
 * THE SIZE IS 0x70, AND IT IS THE ORIGINAL COMPILER'S OWN `sizeof`.
 * `V90Modem`'s constructor does
 *
 *     movl $0x70,(%esp) ; call sysdep_malloc ; ... ; call V90Modulator::C1
 *
 * at .text+0x19604, which is finding 1246's oracle: the allocation is
 * `sizeof(V90Modulator)` written by the compiler that laid the class out.  The
 * highest field the constructor writes is the pointer at +0x6c, which ends at
 * 0x70 exactly, so nothing is unaccounted for.
 *
 * TWELVE ARGUMENTS, AND THE ORDER THEY LAND IN IS NOT THE ORDER THEY ARRIVE
 * IN.  The constructor stores each straight through, but the offsets
 * interleave: arguments 2..5 go to +0x00..+0x0c in order, argument 6 to +0x10
 * and 7 to +0x14, and then 8, 9, 10, 11 go to +0x18, +0x1c, +0x20, +0x24 with
 * the V90CP (argument 9) landing ABOVE the V90MP (argument 10).  Argument 1
 * goes to +0x64, not +0x00, and argument 12 to +0x28, which is the session
 * flag.  Nothing about this is inferable from the signature.
 *
 * THE SCRAMBLER IS EMBEDDED, NOT POINTED AT: `lea 0x44(%ebx),%eax` before both
 * `Scrambler<int,unsigned char>::Scrambler` in the constructor and
 * `::~Scrambler` in the destructor.  Its taps are (0x12, 0x17, 0x63) -- V.90's
 * 18 and 23, the same three constants `V90Phase3Demodulator` builds its
 * descrambler with.
 *
 * WHAT THE CONSTRUCTOR DOES NOT TOUCH is +0x2c..+0x37, which `progress` uses
 * as three separate words: a state at +0x2c that it compares against 2 and 3,
 * a running count at +0x30, and a step at +0x34 that it sets to 0, 6, 7, 8 and
 * 9.  They are left `pad_` because three words a constructor never writes are
 * `reset`'s business and `reset` is not written here.
 *
 * Data member names are invented; the mangling never carries one (finding
 * 226).  `sessionFlag`, `phase3Modulator` and `phase4Modulator` keep the names
 * V90SessionFlag.h gave them.
 */

#ifndef DSPLIB_V90MODULATOR_H
#define DSPLIB_V90MODULATOR_H

#include "dsplib/Scrambler.h"		/* embedded at +0x44, 0x20 bytes */

class V90BitsToSymbol;
class V90CP;
class V90Jd;
class V90MP;
class V90MappingParams;
class V90Parameters;
class V90Phase2Info;
class V90Phase3Modulator;
class V90Phase4Modulator;
class V92Jd;
struct tagV90AdditionalCPinfo;
struct tagV90DILdescriptor;

class V90Modulator {
public:
	/* Defined in src/pump/v90/V90Modulator.cpp. */
	V90Modulator(unsigned int nofSymbols, V90Phase2Info *phase2Info,
		     V90Jd *jd, V92Jd *v92Jd, tagV90DILdescriptor *dil,
		     V90MappingParams *mappingParams,
		     V90MappingParams *mappingParams2,
		     tagV90AdditionalCPinfo *additionalCPinfo, V90CP *cp,
		     V90MP *mp, V90Parameters *params, unsigned int sessionFlag);
	~V90Modulator();

	/* Defined in src/pump/v90/V90SessionFlag.cpp. */
	void setSessionFlag(unsigned int flag);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V90Phase2Info *phase2Info;		/* +0x00 argument 2       */
	V90Jd *jd;				/* +0x04 argument 3       */
	V92Jd *v92Jd;				/* +0x08 argument 4       */
	tagV90DILdescriptor *dil;		/* +0x0c argument 5       */
	V90MappingParams *mappingParams;	/* +0x10 argument 6       */
	V90MappingParams *mappingParams2;	/* +0x14 argument 7       */
	tagV90AdditionalCPinfo *additionalCPinfo; /* +0x18 argument 8     */
	V90MP *mp;				/* +0x1c argument 10      */
	V90CP *cp;				/* +0x20 argument 9       */
	V90Parameters *params;			/* +0x24 argument 11      */
	unsigned int sessionFlag;		/* +0x28 argument 12      */
	unsigned char pad_2c[0x0c];		/* +0x2c progress's three */
	V90Phase3Modulator *phase3Modulator;	/* +0x38 owned, 0x398     */
	V90Phase4Modulator *phase4Modulator;	/* +0x3c owned, 0x2fac    */
	V90BitsToSymbol *bitsToSymbol;		/* +0x40 owned, 0x24      */
	Scrambler<int, unsigned char> scrambler; /* +0x44 32 bytes        */
	unsigned int nofSymbols;		/* +0x64 argument 1       */
	short *symbolBuf;			/* +0x68 2 per symbol     */
	void *frameBuf;				/* +0x6c 8 per symbol     */
};

#endif /* DSPLIB_V90MODULATOR_H */
