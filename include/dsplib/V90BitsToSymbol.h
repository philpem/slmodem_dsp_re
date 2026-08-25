/*
 * V90BitsToSymbol.h -- the V.90 downstream bit-to-symbol converter.
 *
 * Reconstructed from dsplibs.o.  NINE members and 1,672 bytes of code,
 * counting each of the duplicated constructor and destructor symbols once,
 * and ALL NINE ARE NOW WRITTEN.  This sentence has been wrong twice: it once
 * said "eight members and 1,532 bytes", where both halves were wrong, and it
 * then said eight of nine were written with `process(unsigned char *,
 * unsigned int &, short *)` -- 484 bytes at 0x2faa0 -- outstanding.  That one
 * is finding F7520's; `nm -S -C` is where the figures come from.
 *
 * NOT POLYMORPHIC: `~V90BitsToSymbol` is listed with `D1` and `D2` and no
 * `D0`, so offset 0 is a real member and there is no vptr.
 *
 * THE SIZE IS 0x24, AND IT IS THE ORIGINAL COMPILER'S OWN `sizeof`.  Two
 * independent call sites allocate this class and both spell it the same way:
 * `V90Phase4Modulator`'s constructor does `movl $0x24,(%esp) ; call
 * sysdep_malloc ; ... ; call V90BitsToSymbol::C1`, and `V90Modulator`'s does
 * the same (finding F1246).  The last field the constructor writes is the byte
 * at +0x20, so the class ends at 0x21 and pads to 0x24.
 *
 * THE MAPPER IS OWNED, THE PARAMETERS ARE BORROWED.  `mapper` at +0x00 is
 * `sysdep_malloc(sizeof(V90Mapper))` followed by `V90Mapper::V90Mapper`, and
 * the destructor destroys and frees it; `params` at +0x04 is stored straight
 * from the argument and the destructor does not touch it.  The second
 * argument the mapper is built with is RELOADED FROM `params` at +0x04 rather
 * than kept in the register the argument arrived in -- `mov 0x4(%ebx),%edx`
 * after the allocator call -- so the original wrote the member and not the
 * parameter there.  Nothing can distinguish the two by behaviour; it is
 * written the blob's way because the blob is what is being reconstructed.
 *
 * THE FIELD NAMES BEYOND +0x0c COME FROM `reset` AND `nofBitsForNextTime`,
 * which are not written here but were read for them:
 *
 *   - `reset` sets `bitsPerFrame` from the mapping parameters' first word,
 *     computes `extraSymbols` as `(6 * mp[+0x624]) / mp[+0x620]` when the
 *     divisor is nonzero and zero otherwise, then clears `symbolsDone` and
 *     `symbolsBlockSize` and sets `extraSymbolsPending` to 1 -- the same
 *     three stores the constructor ends with.
 *   - `nofBitsForNextTime` compares `symbolsBlockSize` against `symbolsDone`,
 *     adds `extraSymbols` to the difference when `extraSymbolsPending` is
 *     nonzero, and scales by `bitsPerFrame`.  `extraSymbolsPending` is read
 *     with `cmpb`, which is where its width comes from.
 *
 * `bitsPerFrame` and `extraSymbols` are the two fields the CONSTRUCTOR LEAVES
 * ALONE: nothing is stored at +0x14 or +0x18 until the first `reset`.  That
 * is a property of the object worth stating, because it is exactly what a
 * differential test over never-zeroed storage checks.
 *
 * Data member names are invented; the mangling never carries one (finding
 * F226).
 */

#ifndef DSPLIB_V90BITSTOSYMBOL_H
#define DSPLIB_V90BITSTOSYMBOL_H

#include "dsplib/V90Phase3Modulator.h"	/* for `PcmType`; see the resets */

class V90Mapper;
class V90MappingParams;
class V90Parameters;

class V90BitsToSymbol {
public:
	/* Defined in src/pump/v90/V90BitsToSymbol.cpp. */
	V90BitsToSymbol(unsigned int nofSymbols, V90Parameters *params);
	~V90BitsToSymbol();

	/*
	 * THREE MORE, and the return types are inference and not mangling:
	 * none of the three reaches a mangled return type, and each builds
	 * its answer in a full 32-bit %eax with no sign extension anywhere,
	 * so `unsigned int` is what the arithmetic says.
	 *
	 * `nofBitsForNextTime` is how many bits the caller must supply to
	 * fill what is left of the block; `setSymbolsBlockSize` stores its
	 * argument into +0x1c and answers the same number -- the blob inlines
	 * the first into the second and into `process`, and emits no `call`
	 * in either.
	 *
	 * `process(unsigned int &, short *)` hands out one block of symbols,
	 * shifts whatever is left over down to the front, writes the next bit
	 * demand through its reference parameter and answers a STATUS: 0
	 * silently, 1 when `symbolsBlockSize` is zero and 3 when there were
	 * not enough symbols ready.  The two non-zero values are the object's
	 * own words -- "SIZE_NOT_SET" and "BUFFER_UNDERFLOW" in the two
	 * messages -- and 0 is the one with no message.
	 *
	 * `process(unsigned char *, unsigned int)` IS THE OTHER DIRECTION AND
	 * SHARES THAT ALPHABET.  It is the FILL: the bits go to the mapper,
	 * the symbols the mapper makes are appended to `symbols` at
	 * `symbolsDone`, and the answer is 0, 1 for the very same
	 * "SIZE_NOT_SET" message, or **2** for "BUFFER_OVERFLOW" -- the third
	 * of the three strings at .rodata.str1.4+0x85b4, +0x85ec and +0x8628,
	 * and the one the other overload never raises.  So the class has one
	 * status alphabet, 1 SIZE_NOT_SET / 2 BUFFER_OVERFLOW / 3
	 * BUFFER_UNDERFLOW, and each overload can reach the two its own
	 * direction can hit.  (That third address read +0x8624 here and in the
	 * .cpp until finding F7520 checked it: 0x8624 is the "\r\n" INSIDE the
	 * BUFFER_OVERFLOW string, which ends at 0x8626 and pads to 0x8628.)
	 *
	 * THE THIRD OVERLOAD, `(unsigned char *, unsigned int &, short *)` at
	 * 0x2faa0, IS THE FILL AND THE DRAIN IN ONE CALL, and it is the only
	 * one of the three the transmit chain reaches:
	 * `V90Modulator::progress`'s data phase calls exactly this mangling,
	 * `_ZN15V90BitsToSymbol7processEPhRjPs`.  It is not a composition of
	 * the other two -- it can raise ALL THREE statuses, where each sibling
	 * reaches only two, and its underflow arm hands out what it has and
	 * then empties the buffer.
	 */
	unsigned int nofBitsForNextTime();
	unsigned int setSymbolsBlockSize(unsigned int blockSize);
	unsigned int process(unsigned int &nofBits, short *outSymbols);
	unsigned int process(unsigned char *bits, unsigned int nofBits);
	unsigned int process(unsigned char *bits, unsigned int &nofBits,
			     short *outSymbols);

	/*
	 * BOTH RESETS ARE THE MAPPER'S OWN, PLUS WHAT THIS CLASS ADDS.  The
	 * mangled names are `_ZN15V90BitsToSymbol5resetEP16V90MappingParams
	 * 7PcmType` and `..15resetNoSpectral..`, and each begins by handing
	 * both of its arguments straight to the same-named member of
	 * `mapper` -- the pointer at +0x00 is RELOADED from the member
	 * (`mov (%esi),%edx`) rather than kept, as in the constructor.
	 *
	 * `resetNoSpectral` then sets `bitsPerFrame` and stops, 58 bytes
	 * altogether.  `reset` sets `bitsPerFrame` too, computes
	 * `extraSymbols`, and ends with the same three stores the constructor
	 * ends with.  So the ONE field neither this class's constructor nor
	 * `resetNoSpectral` ever writes is `extraSymbols`, which is what a
	 * differential test over never-zeroed storage is for.
	 */
	void reset(V90MappingParams *mp, PcmType pcm);
	void resetNoSpectral(V90MappingParams *mp, PcmType pcm);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V90Mapper *mapper;		/* +0x00 owned, 0x704 bytes         */
	V90Parameters *params;		/* +0x04 borrowed                   */
	short *symbols;			/* +0x08 owned, 2 * nofSymbols      */
	unsigned int nofSymbols;	/* +0x0c the first argument         */
	unsigned int symbolsDone;	/* +0x10 zeroed by ctor and reset   */
	unsigned int bitsPerFrame;	/* +0x14 NOT set by the constructor */
	unsigned int extraSymbols;	/* +0x18 NOT set by the constructor */
	unsigned int symbolsBlockSize;	/* +0x1c zeroed by ctor and reset   */
	unsigned char extraSymbolsPending; /* +0x20 set to 1 by both         */
	unsigned char pad_21[3];	/* +0x21 tail padding               */
};

#endif /* DSPLIB_V90BITSTOSYMBOL_H */
