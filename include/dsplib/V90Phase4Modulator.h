/*
 * V90Phase4Modulator.h -- the V.90 / V.92 phase 4 downstream symbol source.
 *
 * Reconstructed from dsplibs.o.  Forty-three members and 12,078 bytes of
 * code, of which THREE are written here: the constructor, the destructor and
 * `setSessionFlag`, the only member of the class `v34handshak` reaches.
 *
 * NOT POLYMORPHIC: `~V90Phase4Modulator` is listed with `D1` and `D2` and no
 * `D0`, so offset 0 is a real member and there is no vptr.
 *
 * THE OBJECT IS 12,204 BYTES, and the shape of the evidence matters because
 * the number is so much larger than its siblings'.  The largest
 * `this`-relative displacement across all forty-five defined members is
 * +0x2fa8, four bytes wide, so the object ends at 0x2fac.  The displacements
 * are not scattered: they fall in two dense runs, +0x00..+0x58 in fours and
 * +0x2f64..+0x2fa8 in twos and then fours, with nothing between.  A run of
 * consecutive two-byte fields at +0x2f68..+0x2f8c, written by the constructor
 * and read by `recivedCPtag`, `recivedSUV`, `resetBeforRRN`, `reset` and the
 * destructor, is a field block and not an artefact of losing track of which
 * register held `this`.  The ~12 KB between the two runs is the constellation
 * the modulator maps into.
 *
 * That was a BOUND on the object, arrived at the same way as every other size
 * in that task (finding 215): the maximum displacement plus the width of what
 * sits at it.  It was not a claim that +0x5c..+0x2f63 contains no larger
 * member -- nothing reaches past +0x2fab, which is all a displacement scan
 * can say.
 *
 * THE BOUND IS NOW THE SIZE.  `V90Modulator`'s constructor does `movl
 * $0x2fac,(%esp) ; call sysdep_malloc` and then calls this class's `C1` on
 * what came back: finding 1246's oracle, which is the original compiler's own
 * `sizeof` and not a scan of anything.  It agrees with the displacement bound
 * to the byte, which is the first independent confirmation the number has had.
 *
 * THE CONSTRUCTOR'S THIRTEEN STORES fill in most of the first run and all of
 * the last.  Five of the eight arguments are borrowed pointers stored straight
 * through at +0x44..+0x54; the `Scrambler<unsigned char,unsigned char>` at
 * +0x58 is EMBEDDED (`lea 0x58(%esi),%edx` before the call, never a load) and
 * is built with (0x12, 0x17, 0x63), V.90's taps 18 and 23; the eighth argument
 * lands at +0x2f98 and the second at +0x00; +0x2f9c and +0x2fa0 are cleared;
 * and +0x2fa8 takes the V90Parameters.
 *
 * THE BITS-TO-SYMBOL CONVERTER IS EITHER SUPPLIED OR OWNED, AND +0x2fa4
 * RECORDS WHICH.  A non-null third argument is stored at +0x44 and +0x2fa4 is
 * set to 1; a null one makes the constructor allocate 0x24, build a
 * `V90BitsToSymbol(0x140, params)` in it, and set +0x2fa4 to 0.  The
 * destructor reads +0x2fa4 FIRST and skips the release entirely when it is
 * nonzero, so the flag is an ownership bit and 1 means "not ours".  Its two
 * arms are otherwise identical: both end in the scrambler's destructor, which
 * is why the blob has two copies of that call.
 *
 * The allocating arm passes the INCOMING PARAMETER as the converter's
 * V90Parameters and not the `params` member it has just stored -- `%edi` is
 * still live and is not reloaded from +0x2fa8, which it would have to be if
 * the original had named the member.  Nothing can tell the two apart by
 * behaviour; it is written the blob's way because the blob is the
 * specification.
 *
 * `sessionFlag` at +0x00 is named for the method that writes it, exactly as
 * `V90Phase3Modulator::setSessionFlag` and `V90Modulator::setSessionFlag`
 * write their own classes' +0x000; in `V90Phase3Modulator` the field's
 * meaning is settled -- nonzero selects V.92 -- and this class's setter has
 * the same signature and the same one-line body.  Everything after it is
 * `pad_`, because forty-two unwritten members' state is not something a
 * scan of displacements can name.
 */

#ifndef DSPLIB_V90PHASE4MODULATOR_H
#define DSPLIB_V90PHASE4MODULATOR_H

#include "dsplib/Scrambler.h"		/* embedded at +0x58, 0x20 bytes */

class V90BitsToSymbol;
class V90CP;
class V90MP;
class V90MappingParams;
class V90Parameters;

class V90Phase4Modulator {
public:
	/* Defined in src/pump/v90/V90Phase4Modulator.cpp. */
	V90Phase4Modulator(V90Parameters *params, unsigned int sessionFlag,
			   V90BitsToSymbol *bitsToSymbol, V90MP *mp,
			   V90MappingParams *mappingParams,
			   V90MappingParams *mappingParams2, V90CP *cp,
			   unsigned int ctorArg8);
	~V90Phase4Modulator();
	void setSessionFlag(unsigned int);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int sessionFlag;		/* +0x0000 argument 2      */
	unsigned char pad_0004[0x40];		/* +0x0004 not modelled    */
	V90BitsToSymbol *bitsToSymbol;		/* +0x0044 argument 3      */
	V90MP *mp;				/* +0x0048 argument 4      */
	V90MappingParams *mappingParams;	/* +0x004c argument 5      */
	V90MappingParams *mappingParams2;	/* +0x0050 argument 6      */
	V90CP *cp;				/* +0x0054 argument 7      */
	Scrambler<unsigned char, unsigned char> scrambler;
						/* +0x0058 32 bytes        */
	unsigned char pad_0078[0x2f20];		/* +0x0078 the constellation
						 * and the 2-byte field
						 * block at +0x2f68..0x2f8c */
	unsigned int ctorArg8;			/* +0x2f98 argument 8      */
	unsigned int cleared_2f9c;		/* +0x2f9c zeroed by ctor  */
	unsigned int cleared_2fa0;		/* +0x2fa0 zeroed by ctor  */
	unsigned int externalBitsToSymbol;	/* +0x2fa4 1 = not ours    */
	V90Parameters *params;			/* +0x2fa8 argument 1      */
};

#endif /* DSPLIB_V90PHASE4MODULATOR_H */
