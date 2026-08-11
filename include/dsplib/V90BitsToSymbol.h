/*
 * V90BitsToSymbol.h -- the V.90 downstream bit-to-symbol converter.
 *
 * Reconstructed from dsplibs.o.  Eight members and 1,532 bytes of code, of
 * which the constructor and the destructor are written here.
 *
 * NOT POLYMORPHIC: `~V90BitsToSymbol` is listed with `D1` and `D2` and no
 * `D0`, so offset 0 is a real member and there is no vptr.
 *
 * THE SIZE IS 0x24, AND IT IS THE ORIGINAL COMPILER'S OWN `sizeof`.  Two
 * independent call sites allocate this class and both spell it the same way:
 * `V90Phase4Modulator`'s constructor does `movl $0x24,(%esp) ; call
 * sysdep_malloc ; ... ; call V90BitsToSymbol::C1`, and `V90Modulator`'s does
 * the same (finding 1246).  The last field the constructor writes is the byte
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
 * 226).
 */

#ifndef DSPLIB_V90BITSTOSYMBOL_H
#define DSPLIB_V90BITSTOSYMBOL_H

class V90Mapper;
class V90Parameters;

class V90BitsToSymbol {
public:
	/* Defined in src/pump/v90/V90BitsToSymbol.cpp. */
	V90BitsToSymbol(unsigned int nofSymbols, V90Parameters *params);
	~V90BitsToSymbol();

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
