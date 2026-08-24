/*
 * V90Modulator.h -- the V.90 downstream modulator, the top of the chain.
 *
 * Reconstructed from dsplibs.o.  Seventeen members and 3,354 bytes of code, of
 * which the constructor, the destructor, `setSessionFlag`, `reset`,
 * `progress` and `initiateRRN` are written; the eleven remaining phase
 * transitions -- `enterPhase3`, `enterPhase4`, `enterDataPhase`, `exitJd`,
 * `exitJdPhase`, `exitDIL`, `exitRi`, `acknowledgeCPReception`,
 * `acknowledgeCPNotReception`, `acknowledgeEReception` and `initiateFPE` --
 * are not.
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
 * +0x2c..+0x37 ARE THREE WORDS AND `reset` IS WHAT ESTABLISHES IT.  They were
 * `pad_2c[0x0c]` while only the constructor was written; `reset` stores a
 * separate `movl $0x0` into each of +0x2c, +0x30 and +0x34 (0x1a532, 0x1a539,
 * 0x1a540), which fixes the width at four bytes and the count at three.
 *
 * THEY NOW HAVE NAMES, AND `progress` IS WHAT EARNED THEM.  This paragraph
 * used to say the names stayed offset-derived because "the roles below are
 * read out of `progress`, which is not written yet, and a role that has not
 * been reproduced is not a name".  `progress` and `initiateRRN` are written
 * (finding 7520), so the condition that clause set is discharged:
 *
 *   - +0x2c `state`.  The object's own word: the `default` arm of `progress`'s
 *     switch prints "V90Modulator progress: Illegal state".  1 is phase 3, 2
 *     is phase 4, 3 is the data phase and 0 emits silence.  It is `int` and
 *     not `unsigned int`, which is MEASURED: the switch at 0x1a848 is
 *     `cmp $0x1 ; je ; jle`, and a `jle` is the signed decision tree -- an
 *     unsigned selector gives `jbe` there.  Nothing else in the class compares
 *     it with anything but `==`, so this one site is the whole evidence and it
 *     is enough.
 *   - +0x30 `symbolCount`.  `progress` adds its symbol count to it on entry
 *     and every state transition clears it, so it counts symbols since the
 *     state was entered -- exactly `V90Phase3Modulator::symbolCount` (+0x018)
 *     and `V90Phase4Modulator::symbolCount` (+0x0008), which sit in the same
 *     relative position in their own triples.  It stays UNSIGNED: 0x1aab2
 *     compares it against `V90Parameters::DEBUG_DIGITAL_MODEM_INITIATE_RRN_
 *     TIME`, which is declared `int`, and the object's `jbe` is unsigned --
 *     so the unsignedness has to come from this side of the comparison.
 *   - +0x34 `eventCode`.  Every value in it is COPIED OUT OF a field already
 *     named that: `V90Phase3Modulator::eventCode` (+0x01c), whose header says
 *     "the caller's per-symbol notification and nothing reads it here", and
 *     `V90Phase4Modulator::word_000c` (+0x000c), whose header says "what reads
 *     it is outside this class".  `progress` is that caller and that reader.
 *     It acts on 6 (phase 3 terminated -> enter phase 4) and 7 (phase 4
 *     terminated -> enter the data phase) and sets 8 and 9 of its own.
 *
 * The constructor still does not touch any of the three; `reset` is what
 * clears them, and `progress` is undefined before the first `reset`.
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

	/*
	 * Per-connection reset: the embedded scrambler back to 0 and the
	 * three words at +0x2c..+0x37 cleared.  Does NOT rebuild anything --
	 * the scrambler keeps the taps its constructor set.
	 */
	void reset();

	/*
	 * `progress` -- .text+0x1a820, 780 bytes.  The transmit chain's whole
	 * per-block entry point and the only thing `V90Modem::progress` calls
	 * on the digital side.  `void` because no path arranges %eax and the
	 * caller tail-JUMPS to it, so its return type is this one; the twin
	 * `V90Demodulator::progress` reads the same way.
	 *
	 * One switch over `state` and a shared tail that widens the finished
	 * `symbolBuf` into the caller's `float *`.  `bits` is only touched in
	 * the data phase, where it goes straight to the scrambler.
	 */
	void progress(int *bits, unsigned int &nofBits, float *out,
		      unsigned int nofSymbols);

	/*
	 * `initiateRRN` -- .text+0x1a2c0, 308 bytes.  Rate renegotiation:
	 * leave the data phase for phase 4 and rebuild the phase 4 modulator
	 * around a one-symbol block.  Returns 0 when it acted and -1 when
	 * `state` was not 3 -- both are `mov $imm,%eax` before the shared
	 * epilogue, so the return type is real and not a leftover.
	 */
	int initiateRRN();

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
	int state;				/* +0x2c reset clears     */
	unsigned int symbolCount;		/* +0x30 reset clears     */
	unsigned int eventCode;			/* +0x34 reset clears     */
	V90Phase3Modulator *phase3Modulator;	/* +0x38 owned, 0x398     */
	V90Phase4Modulator *phase4Modulator;	/* +0x3c owned, 0x2fac    */
	V90BitsToSymbol *bitsToSymbol;		/* +0x40 owned, 0x24      */
	Scrambler<int, unsigned char> scrambler; /* +0x44 32 bytes        */
	unsigned int nofSymbols;		/* +0x64 argument 1       */
	short *symbolBuf;			/* +0x68 2 per symbol     */

	/*
	 * +0x6c, 8 bytes per symbol.  WAS `void *`, and the type is forced
	 * rather than chosen: `progress` hands it to
	 * `Scrambler<int,unsigned char>::process` as its `unsigned char *`
	 * output and to `V90BitsToSymbol::process` as its `unsigned char *`
	 * input, and C++ converts neither from `void *` without a cast.  One
	 * scrambled BIT PER BYTE, which is what makes 8 per symbol the right
	 * size -- `V90Mapper::process` reads the same buffer a byte to a bit.
	 */
	unsigned char *frameBuf;		/* +0x6c 8 per symbol     */
};

#endif /* DSPLIB_V90MODULATOR_H */
