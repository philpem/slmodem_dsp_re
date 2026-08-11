/*
 * V92BitsToSymbol.h -- the V.92 upstream bit-to-symbol stage.
 *
 * Reconstructed from dsplibs.o.  Two of the class's eight symbols are written
 * in src/pump/v90/V92BitsToSymbol.cpp -- the constructor (C2 at .text+0x4ded0
 * and C1 at +0x4df40, 105 bytes each) and the destructor (D2 at +0x4dfb0 and
 * D1 at +0x4e010, 84 bytes each).  The other six are declared here and left
 * undefined for the reason docs/v90cpp.md gives.
 *
 * THE OBJECT IS 0x20 BYTES, AND IT IS THE ALLOCATION.  `V92Modulator::
 * V92Modulator` builds it:
 *
 *     15226:  c7 04 24 20 00 00 00   movl $0x20,(%esp)
 *     1522d:  e8 ..                  call sysdep_malloc
 *     15247:  e8 ..                  call V92BitsToSymbol::V92BitsToSymbol
 *
 * so 0x20 is the original compiler's own `sizeof` (finding 1249's oracle).
 * The furthest field anything here touches is the byte at +0x1c, and three
 * bytes of alignment carry the object to 0x20.
 *
 * ---------------------------------------------------------------------------
 * FOUR OF THE FIVE SCALARS ARE NAMED BY MEMBERS THIS FILE DOES NOT DEFINE,
 * and the three that are small enough to read whole are what named them:
 *
 *   `reset(V92MappingParams *)`  (.text+0x4e070, 68 B)
 *        transmitter->reset(params); +0x10 = 0; +0x18 = 0;
 *        +0x14 = *(unsigned *)params; +0x1c = 1
 *
 *   `setSymbolsBlockSize(unsigned n)`  (+0x4e130, 108 B)
 *        +0x18 = n, then returns what `nofBitsForNextTime` returns
 *
 *   `nofBitsForNextTime()`  (+0x4e0c0, 100 B)
 *        d = +0x18 - +0x10; if (d == 0 or negative-by-unsigned) 0
 *        else ceil(d / 12) * +0x14
 *
 * So +0x18 is a block size in SYMBOLS, +0x10 is how many of them are already
 * accounted for, +0x14 is a bit count per twelve symbols, and the divisor
 * twelve is the V.90/V.92 data frame.  Every comparison in those two members
 * is `jbe`/`ja` and the division is the 0xaaaaaaab reciprocal with a logical
 * shift, so all three are UNSIGNED and that is forced rather than chosen.
 *
 * **+0x14 IS THE ONE THE CONSTRUCTOR LEAVES ALONE**, and the hole is the
 * claim: the constructor writes +0x10, +0x18 and +0x1c and not +0x14, so a
 * freshly constructed object's bit count is whatever the allocation held
 * until `reset` copies it out of the mapping parameters.  Finding 1248's
 * shape, in a second class.
 *
 * Data member names are invented and descriptive (finding 226).
 */

#ifndef DSPLIB_V92BITSTOSYMBOL_H
#define DSPLIB_V92BITSTOSYMBOL_H

class V92MappingParams;
class V92Parameters;
class V92Transmitter;

class V92BitsToSymbol {
public:
	V92BitsToSymbol(unsigned int nSymbols, V92Parameters *params);
	~V92BitsToSymbol();

	/*
	 * Declared, not defined.  Argument types are the mangling's and
	 * exact; return types are not mangled, and the two below that DO
	 * return something leave it in %eax as an unsigned count.
	 */
	void reset(V92MappingParams *params);
	unsigned int nofBitsForNextTime();
	unsigned int setSymbolsBlockSize(unsigned int nSymbols);
	void process(unsigned char *bits, unsigned int nbits);
	void process(unsigned char *bits, unsigned int &nbits, short *out);
	void process(unsigned int &nbits, short *out);

	/* Public for `offsetof`; one access section keeps the class standard
	 * layout, and the original's access specifiers are not recoverable. */

	/*
	 * +0x00  The transmit chain, `sysdep_malloc(0x60)` and constructed.
	 * Owned: the destructor destroys and frees it.
	 */
	V92Transmitter *transmitter;

	/*
	 * +0x04  The constructor's second argument, stored and NOT owned --
	 * the destructor does not touch it.  It is stored BEFORE the
	 * transmitter is allocated, which is the first thing the constructor
	 * does at all.
	 */
	V92Parameters *params;

	/*
	 * +0x08  `sysdep_malloc(2 * nSymbols)` -- `lea (%edi,%edi,1)`, so the
	 * element is two bytes and the count is the constructor's first
	 * argument.  The class's `process` overloads take `short *`, which is
	 * what fixes the element type at `short` rather than at "two bytes".
	 * Owned; the destructor frees it with no destructor call.
	 */
	short *symbols;

	/* +0x0c  The constructor's first argument, kept verbatim: the number
	 * of elements `symbols` was sized for. */
	unsigned int nSymbols;

	/* +0x10  Symbols already accounted for; cleared here and by `reset`,
	 * and the subtrahend in `nofBitsForNextTime`. */
	unsigned int symbolsDone;

	/*
	 * +0x14  Bits per twelve symbols.  NOT WRITTEN BY THE CONSTRUCTOR --
	 * `reset` copies it out of the mapping parameters' first word, and
	 * until then it holds whatever the allocation did.
	 */
	unsigned int bitsPerFrame;

	/* +0x18  The block size in symbols, cleared here and by `reset` and
	 * set by `setSymbolsBlockSize`. */
	unsigned int symbolsBlockSize;

	/* +0x1c  One byte, set to 1 here and by `reset`.  A flag; which one
	 * is not established, because no member that reads it is written. */
	unsigned char flag_1c;

	/* +0x1d  Three bytes of alignment inside the 0x20 the allocation
	 * gives.  Nothing writes them. */
	unsigned char pad_1d[3];
};

#endif /* DSPLIB_V92BITSTOSYMBOL_H */
