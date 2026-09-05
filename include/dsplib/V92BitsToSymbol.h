/**
 * @file V92BitsToSymbol.h
 * @brief V.92 upstream bit-to-symbol stage: decouples "here are some bits"
 *        from "give me exactly `symbolsBlockSize` symbols".
 *
 * The class owns a V92Transmitter and a `short` staging buffer. Bits go in
 * through the two overloads that take a `bits` argument; the transmitter
 * turns each `K` of them into twelve symbols appended at `symbolsDone`. The
 * two overloads that take an `out` argument hand `symbolsBlockSize` of the
 * staged symbols back and shift whatever is left down to the front.
 * `nofBitsForNextTime` is the class telling its caller how many bits it
 * wants next.
 *
 * `sizeof(V92BitsToSymbol)` is 0x20 -- the exact allocation
 * `V92Modulator::V92Modulator` requests before constructing it (finding
 * F1249). The furthest field anything touches is the byte at +0x1c; three
 * bytes of alignment carry the object to 0x20.
 *
 * `nofBitsForNextTime` (and `setSymbolsBlockSize`, which inlines it) rounds
 * `(symbolsBlockSize - symbolsDone)` symbols up to a bit count in two
 * genuinely different ways depending on whether twelve divides the
 * remainder evenly -- multiply-then-divide when it does, divide-then-round
 * when it doesn't -- and the two forms only disagree once the intermediate
 * multiply overflows 32 bits. Do not simplify this to one expression;
 * finding F3052 has the object-level proof that the object itself does not.
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
	/**
	 * @brief Construct the stage: build the owned V92Transmitter and
	 *        allocate the `short` staging buffer.
	 * @param nSymbols  Capacity of the staging buffer, in symbols.
	 * @param params    Stored verbatim, not owned; not read by the
	 *                  constructor itself.
	 */
	V92BitsToSymbol(unsigned int nSymbols, V92Parameters *params);
	/**
	 * @brief Destroy the owned transmitter and free the staging buffer.
	 *        Neither pointer nor `params` is cleared afterwards, so a
	 *        second destruction double-frees (docs/deviations.md,
	 *        reproduced from the object).
	 */
	~V92BitsToSymbol();

	/**
	 * @brief Reset the transmitter and this stage's own counters for a
	 *        new connection.
	 * @param params  A V92ParamsInfo block; only its first word (`K`,
	 *                bits per twelve-symbol frame) is read.
	 */
	void reset(V92MappingParams *params);
	/**
	 * @brief How many more bits the caller must supply before the
	 *        currently configured block of `symbolsBlockSize` symbols can
	 *        be filled from what is already staged.
	 * @return 0 if enough symbols are already staged, else the bit count
	 *         (see the file header for the two-formula rounding rule).
	 */
	unsigned int nofBitsForNextTime();
	/**
	 * @brief Set the number of symbols a subsequent `process(nbits, out)`
	 *        call should deliver.
	 * @param nSymbols  The new block size, in symbols.
	 * @return The same value `nofBitsForNextTime()` would return.
	 */
	unsigned int setSymbolsBlockSize(unsigned int nSymbols);
	/**
	 * @brief Feed bits in without collecting any symbols out.
	 * @param bits   Input bits for the transmitter.
	 * @param nbits  Number of bits in @p bits (input only, on this
	 *               overload).
	 * @return A #V92BTOS_OK / #V92BTOS_SIZE_NOT_SET / #V92BTOS_BUFFER_OVERFLOW
	 *         status.
	 */
	int process(unsigned char *bits, unsigned int nbits);
	/**
	 * @brief Feed bits in and collect one block of symbols out in the
	 *        same call.
	 * @param bits   Input bits for the transmitter.
	 * @param nbits  In: number of bits in @p bits. Out: bits wanted for
	 *               the next call (`nofBitsForNextTime()`).
	 * @param out    Receives `symbolsBlockSize` symbols.
	 * @return A #V92BTOS_OK / #V92BTOS_SIZE_NOT_SET /
	 *         #V92BTOS_BUFFER_UNDERFLOW / #V92BTOS_BUFFER_OVERFLOW status.
	 */
	int process(unsigned char *bits, unsigned int &nbits, short *out);
	/**
	 * @brief Collect one block of already-staged symbols, feeding no new
	 *        bits in.
	 * @param nbits  Out: bits wanted for the next call
	 *               (`nofBitsForNextTime()`).
	 * @param out    Receives `symbolsBlockSize` symbols, or fewer (with
	 *               #V92BTOS_BUFFER_UNDERFLOW) if not enough are staged.
	 * @return A #V92BTOS_OK / #V92BTOS_SIZE_NOT_SET /
	 *         #V92BTOS_BUFFER_UNDERFLOW status.
	 */
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

	/* +0x08  The staging buffer, sized `2 * nSymbols` bytes; element type
	 * `short` is fixed by the `process` overloads' own signatures. Owned;
	 * the destructor frees it with no destructor call. */
	short *symbols;

	/* +0x0c  The constructor's first argument, kept verbatim: the number
	 * of elements `symbols` was sized for. */
	unsigned int nSymbols;

	/* +0x10  Symbols already accounted for; cleared here and by `reset`,
	 * and the subtrahend in `nofBitsForNextTime`. */
	unsigned int symbolsDone;

	/*
	 * +0x14  Bits per twelve-symbol frame -- the author's own "K", copied
	 * by `reset` from the mapping parameters' first word (finding F4503;
	 * see V92ParamsInfo.h). Not written by the constructor, so a freshly
	 * constructed object's value here is whatever the allocation held
	 * until `reset` runs.
	 */
	unsigned int bitsPerFrame;

	/* +0x18  The block size in symbols, cleared here and by `reset` and
	 * set by `setSymbolsBlockSize`. */
	unsigned int symbolsBlockSize;

	/*
	 * +0x1c  One byte, set to 1 by the constructor and by `reset`. All
	 * three `process` overloads read it, on every path including the two
	 * error ones, as
	 *
	 *     if (flag_1c != 0)
	 *             flag_1c = 0;
	 *
	 * -- a real branch in the object (`cmpb $0x0,0x1c(%ebx); je; movb
	 * $0x0,0x1c(%ebx)`), not something the compiler would add over a bare
	 * store. So it is a one-shot latch: set at construction and at every
	 * reset, cleared by the first `process` call after either. Nothing
	 * written here ever branches on its value, so there is no evidence
	 * for what it is a one-shot flag *of* -- the role is not established
	 * and the neutral name stays (finding F4505/F3120).
	 */
	unsigned char flag_1c;

	/*
	 * +0x1d was `pad_1d[3]`, the struct's LAST member -- REMOVED
	 * (finding F10151).  Already correctly described as alignment
	 * inside the 0x20 the allocation gives; `flag_1c` ends at +0x1d and
	 * the class's own 4-byte alignment (forced by its leading pointers/
	 * ints) rounds `sizeof` up to +0x20 on its own, already proved by
	 * the existing `v92btos_size[(sizeof(V92BitsToSymbol) == 0x20) ? 1 :
	 * -1]` hard compile assertion. `dis.py` over all ten
	 * `V92BitsToSymbol` methods finds no access to offset
	 * 0x1d/0x1e/0x1f.
	 */
};

#endif /* DSPLIB_V92BITSTOSYMBOL_H */
