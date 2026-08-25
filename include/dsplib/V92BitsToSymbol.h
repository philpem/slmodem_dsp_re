/*
 * V92BitsToSymbol.h -- the V.92 upstream bit-to-symbol stage.
 *
 * Reconstructed from dsplibs.o.  ALL EIGHT of the class's symbols are written
 * in src/pump/v90/V92BitsToSymbol.cpp -- the constructor (C2 at .text+0x4ded0
 * and C1 at +0x4df40, 105 bytes each), the destructor (D2 at +0x4dfb0 and
 * D1 at +0x4e010, 84 bytes each), `reset`, `nofBitsForNextTime`,
 * `setSymbolsBlockSize` and the three `process` overloads.
 *
 * WHAT THE CLASS IS.  It owns a V92Transmitter and a `short` staging buffer,
 * and it exists to decouple "here are some bits" from "give me exactly
 * `symbolsBlockSize` symbols".  Bits go in through the two overloads that
 * take a `bits` argument, the transmitter turns each `K` of them into twelve
 * symbols appended at `symbolsDone`, and the two overloads that take an
 * `out` argument hand `symbolsBlockSize` of them back and shift whatever is
 * left down to the front.  `nofBitsForNextTime` is the class telling its
 * caller how many bits it wants next.
 *
 * THE OBJECT IS 0x20 BYTES, AND IT IS THE ALLOCATION.  `V92Modulator::
 * V92Modulator` builds it:
 *
 *     15226:  c7 04 24 20 00 00 00   movl $0x20,(%esp)
 *     1522d:  e8 ..                  call sysdep_malloc
 *     15247:  e8 ..                  call V92BitsToSymbol::V92BitsToSymbol
 *
 * so 0x20 is the original compiler's own `sizeof` (finding F1249's oracle).
 * The furthest field anything here touches is the byte at +0x1c, and three
 * bytes of alignment carry the object to 0x20.
 *
 * ---------------------------------------------------------------------------
 * FOUR OF THE FIVE SCALARS ARE NAMED BY THE THREE SMALL MEMBERS:
 *
 *   `reset(V92MappingParams *)`  (.text+0x4e070, 68 B)
 *        transmitter->reset(params); +0x10 = 0; +0x18 = 0;
 *        +0x14 = *(unsigned *)params; +0x1c = 1
 *
 *   `setSymbolsBlockSize(unsigned n)`  (+0x4e130, 108 B)
 *        +0x18 = n, then returns what `nofBitsForNextTime` returns --
 *        inlined, so the two bodies are the same code twice
 *
 *   `nofBitsForNextTime()`  (+0x4e0c0, 100 B)
 *        d = +0x18 - +0x10; zero if +0x18 <= +0x10, and otherwise the
 *        TWO EXPRESSIONS BELOW, which are not the same expression
 *
 * So +0x18 is a block size in SYMBOLS, +0x10 is how many of them are already
 * accounted for, +0x14 is a bit count per twelve symbols, and the divisor
 * twelve is the V.90/V.92 data frame.  Every comparison in those two members
 * is `jbe`/`ja` and the division is the 0xaaaaaaab reciprocal with a logical
 * shift, so all three are UNSIGNED and that is forced rather than chosen.
 *
 * **THE ROUNDING-UP IS NOT ONE EXPRESSION AND THE DIFFERENCE IS REAL.**  The
 * obvious reading -- `ceil(d / 12) * bitsPerFrame` -- is what the object
 * computes only when twelve does NOT divide `d`.  When it does, the object
 * multiplies FIRST and divides after:
 *
 *     d % 12 != 0   ->   (d / 12 + 1) * bitsPerFrame     .text+0x4e0f2
 *     d % 12 == 0   ->   d * bitsPerFrame / 12           .text+0x4e108
 *
 * and there is no doubt about which is which: the `je` at +0x4e0f0 is taken
 * on `d == (d / 12) * 12`, and the block it lands in reloads +0x14 into the
 * register that held the quotient, so the quotient is dead there and cannot
 * be what the multiply uses.  Over 32-bit arithmetic the two agree on every
 * input where `d * bitsPerFrame` fits, and separate the moment it wraps --
 * finding F3052's shape exactly, and the reason this is spelt out here rather
 * than tidied into the shorter form that "obviously" means the same thing.
 *
 * **+0x14 IS THE ONE THE CONSTRUCTOR LEAVES ALONE**, and the hole is the
 * claim: the constructor writes +0x10, +0x18 and +0x1c and not +0x14, so a
 * freshly constructed object's bit count is whatever the allocation held
 * until `reset` copies it out of the mapping parameters.  Finding F1248's
 * shape, in a second class.
 *
 * Data member names are invented and descriptive (finding F226).
 */

#ifndef DSPLIB_V92BITSTOSYMBOL_H
#define DSPLIB_V92BITSTOSYMBOL_H

/*
 * THE THREE `process` OVERLOADS RETURN A STATUS, AND THE AUTHOR NAMED ALL
 * THREE FAILURES HIMSELF.  Each writes a small constant into the register it
 * returns and, at level 2, prints a string carrying the same word:
 *
 *   1  "V92BitsToSymbol - error: process called, SIZE_NOT_SET\r\n"
 *                                          .rodata.str1.4:0xd484
 *   2  "V92BitsToSymbol - error: process called, BUFFER_OVERFLOW\r\n"
 *                                          .rodata.str1.4:0xd4bc
 *   3  "V92BitsToSymbol - error: process called, BUFFER_UNDERFLOW\r\n"
 *                                          .rodata.str1.4:0xd4f8
 *
 * so the constant-to-name pairing is the object's own and not a reading of
 * what each path does.  Zero is the value the three functions start from and
 * has no string; "OK" is this tree's word for it.
 *
 * These are `#define` rather than an `enum` deliberately.  The return type is
 * not mangled, so nothing in the object says whether the author wrote an
 * enumeration or plain `int`s, and inventing a type name would be a claim
 * where a constant is a fact.
 */
#define V92BTOS_OK			0
#define V92BTOS_SIZE_NOT_SET		1
#define V92BTOS_BUFFER_OVERFLOW		2
#define V92BTOS_BUFFER_UNDERFLOW	3

/*
 * Twelve symbols to a frame.  Every division in this class is by twelve, by
 * the 0xaaaaaaab reciprocal with a LOGICAL shift of three -- so unsigned --
 * and V92Transmitter::process produces exactly twelve samples per `K` bits,
 * which is the other half of the same statement.
 */
#define V92BTOS_SYMBOLS_PER_FRAME	12

class V92MappingParams;
class V92Parameters;
class V92Transmitter;

class V92BitsToSymbol {
public:
	V92BitsToSymbol(unsigned int nSymbols, V92Parameters *params);
	~V92BitsToSymbol();

	/*
	 * Argument types are the mangling's and exact.  Return types are not
	 * mangled: `nofBitsForNextTime` and `setSymbolsBlockSize` leave an
	 * unsigned count in %eax, the three `process` overloads leave one of
	 * the four V92BTOS_* constants above, and `reset` sets %eax on no
	 * path of its own.
	 *
	 * `nbits` IS IN-OUT IN ONE OVERLOAD AND OUT IN THE OTHER, which the
	 * object states rather than the signature: the three-argument form
	 * reads it (`mov (%ecx),%eax` at .text+0x4e22f) before handing it to
	 * the transmitter and overwrites it on the way out, and the
	 * two-argument form only ever stores.
	 */
	void reset(V92MappingParams *params);
	unsigned int nofBitsForNextTime();
	unsigned int setSymbolsBlockSize(unsigned int nSymbols);
	int process(unsigned char *bits, unsigned int nbits);
	int process(unsigned char *bits, unsigned int &nbits, short *out);
	int process(unsigned int &nbits, short *out);

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
	 * +0x14  Bits per twelve symbols, and it is now MEASURED rather than
	 * inferred from the arithmetic that reads it.  `reset` copies it out
	 * of the mapping parameters' first word, which is the same word
	 * `V92Transmitter::reset` copies into its own +0x04 and prints as
	 * "K = %d" -- so this field and that one hold one quantity, and
	 * `V92Transmitter::process` is what says what the quantity counts: it
	 * consumes exactly `K` input bits per twelve output samples.  The
	 * author's word for it is "K"; the name here is kept descriptive
	 * because "K" alone says nothing, and the identity is recorded rather
	 * than the letter copied.  See include/dsplib/V92ParamsInfo.h.
	 *
	 * NOT WRITTEN BY THE CONSTRUCTOR -- until `reset` runs it holds
	 * whatever the allocation did.
	 */
	unsigned int bitsPerFrame;

	/* +0x18  The block size in symbols, cleared here and by `reset` and
	 * set by `setSymbolsBlockSize`. */
	unsigned int symbolsBlockSize;

	/*
	 * +0x1c  One byte, set to 1 by the constructor and by `reset`.
	 *
	 * ALL THREE `process` OVERLOADS NOW READ IT AND THE ROLE IS STILL NOT
	 * ESTABLISHED, so the neutral name stays.  What each of them does
	 * with it, on every path including the two error ones, is
	 *
	 *     if (flag_1c != 0)
	 *             flag_1c = 0;
	 *
	 * -- `cmpb $0x0,0x1c(%ebx); je; movb $0x0,0x1c(%ebx)`, and the test
	 * is the object's rather than the compiler's, because a bare store
	 * would have been one instruction and GCC does not add a branch to
	 * avoid one.  So it is a one-shot: set at construction and at every
	 * reset, cleared by the first `process` after either.  NOTHING
	 * WRITTEN HERE EVER BRANCHES ON IT, which is why naming it "first
	 * call" or anything else would be a guess about a reader that has not
	 * been found.  Finding F3120's ruling, in a second class.
	 */
	unsigned char flag_1c;

	/* +0x1d  Three bytes of alignment inside the 0x20 the allocation
	 * gives.  Nothing writes them. */
	unsigned char pad_1d[3];
};

#endif /* DSPLIB_V92BITSTOSYMBOL_H */
