/*
 * V92BitsToSymbol.cpp -- construction and destruction of the V.92 upstream
 * bit-to-symbol stage.
 *
 * Reconstructed from dsplibs.o.  Ten symbols, 1,661 bytes:
 *
 *     V92BitsToSymbol::V92BitsToSymbol(unsigned, V92Parameters *)
 *                                        .text+0x4ded0 (C2), +0x4df40 (C1)
 *     V92BitsToSymbol::~V92BitsToSymbol() .text+0x4dfb0 (D2), +0x4e010 (D1)
 *     V92BitsToSymbol::reset(V92MappingParams *)        +0x4e070,  68 B
 *     V92BitsToSymbol::nofBitsForNextTime()             +0x4e0c0, 100 B
 *     V92BitsToSymbol::setSymbolsBlockSize(unsigned)    +0x4e130, 108 B
 *     V92BitsToSymbol::process(unsigned char *, unsigned int &, short *)
 *                                                       +0x4e1a0, 468 B
 *     V92BitsToSymbol::process(unsigned char *, unsigned int)
 *                                                       +0x4e380, 180 B
 *     V92BitsToSymbol::process(unsigned int &, short *)  +0x4e440, 359 B
 *
 * Each pair is byte-identical; GCC emits both from one definition.
 * `include/dsplib/V92BitsToSymbol.h` carries the object map and the 0x20 the
 * allocation gives.
 *
 * WHY THE TRANSMITTER IS BUILT THROUGH AN asm() LABEL: exactly the reason
 * src/pump/v90/V92Precoder.cpp gives -- `sysdep_malloc(n); ctor(p)` with no
 * null test between them is `new`, the build is -nostdinc++ with no <new>,
 * and a placement form would add a null test the blob does not have.
 *
 * Plain cdecl, `this` first on the stack (`mov 0x20(%esp),%ebx` after a
 * 0x1c-byte frame and three saves) -- finding 215.
 */

#include <stddef.h>

#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/debug.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

/* The complete-object constructor, which is what the relocation at
 * .text+0x4deff names and what a `new` expression uses. */
void v92btos_transmitter_ctor(void *self) asm("_ZN14V92TransmitterC1Ev");
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92BTOS_OFF(field, off, tag) \
	typedef char v92btos_off_##tag[ \
	    ((int)__builtin_offsetof(V92BitsToSymbol, field) == (off)) \
	    ? 1 : -1]

V92BTOS_OFF(transmitter,	0x00, transmitter);
V92BTOS_OFF(params,		0x04, params);
V92BTOS_OFF(symbols,		0x08, symbols);
V92BTOS_OFF(nSymbols,		0x0c, nsymbols);
V92BTOS_OFF(symbolsDone,	0x10, symbolsdone);
V92BTOS_OFF(bitsPerFrame,	0x14, bitsperframe);
V92BTOS_OFF(symbolsBlockSize,	0x18, blocksize);
V92BTOS_OFF(flag_1c,		0x1c, flag1c);
V92BTOS_OFF(pad_1d,		0x1d, pad1d);

typedef char v92btos_size[(sizeof(V92BitsToSymbol) == 0x20) ? 1 : -1];
typedef char v92btos_tx_size[(sizeof(V92Transmitter) == 0x60) ? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * V92BitsToSymbol::V92BitsToSymbol (.text+0x4ded0 / +0x4df40, 105 bytes)
 *
 * The whole body, with nothing elided:
 *
 *     +0x04 = params                       <- BEFORE the first allocation
 *     malloc(0x60) -> V92Transmitter() -> +0x00
 *     +0x08 = malloc(2 * nSymbols)
 *     +0x0c = nSymbols
 *     +0x10 = 0 ; +0x18 = 0 ; +0x1c = 1    (the last a BYTE store)
 *
 * `2 * nSymbols` is `lea (%edi,%edi,1)` and not a shift, which is what GCC
 * emits for `n * sizeof(short)` with `n` already in a register.  There is NO
 * multiply by anything else and no rounding: the buffer is exactly the
 * argument's worth of shorts.
 *
 * +0x14 is not written; see the header.  No allocation is checked.
 * ===========================================================================
 */
V92BitsToSymbol::V92BitsToSymbol(unsigned int n, V92Parameters *p)
{
	void *tx;

	params = p;

	tx = sysdep_malloc(sizeof(V92Transmitter));
	v92btos_transmitter_ctor(tx);
	transmitter = (V92Transmitter *)tx;

	symbols = (short *)sysdep_malloc(n * sizeof(short));
	nSymbols = n;
	symbolsDone = 0;
	symbolsBlockSize = 0;
	flag_1c = 1;
}

/*
 * ===========================================================================
 * V92BitsToSymbol::~V92BitsToSymbol (.text+0x4dfb0 / +0x4e010, 84 bytes)
 *
 * Two null-guarded releases and nothing else.  The transmitter gets its
 * destructor -- `_ZN14V92TransmitterD1Ev`, the complete-object variant --
 * and the symbol buffer does not, because it is raw storage.
 *
 * NEITHER POINTER IS NULLED after its free and neither is `params` touched,
 * so a second destruction double-frees both; the blob leaves it that way and
 * so does this.  See docs/deviations.md.
 * ===========================================================================
 */
V92BitsToSymbol::~V92BitsToSymbol()
{
	if (transmitter != 0) {
		transmitter->~V92Transmitter();
		sysdep_free(transmitter);
	}

	if (symbols != 0)
		sysdep_free(symbols);
}

/*
 * ===========================================================================
 * V92BitsToSymbol::reset (.text+0x4e070, 68 bytes)
 *
 * The transmitter first, then four stores.  The parameter block is read
 * through its FIRST WORD ONLY -- `mov (%esi),%eax` -- and that word is
 * `V92ParamsInfo::K`, which is the same field `V92Transmitter::reset` copies
 * and prints as "K = %d".  The cast is the one V92Precoder.cpp and
 * V92Transmitter.cpp already make: `V92MappingParams` is the mangling's name
 * for the block this tree models as `struct V92ParamsInfo`.
 *
 * The load is hoisted above the three stores in the object, which is
 * scheduling; the store ORDER, +0x10 before +0x18 before +0x14 before the
 * byte at +0x1c, is what is written here.
 * ===========================================================================
 */
void
V92BitsToSymbol::reset(V92MappingParams *p)
{
	transmitter->reset(p);

	symbolsDone = 0;
	symbolsBlockSize = 0;
	bitsPerFrame = (unsigned int)((struct V92ParamsInfo *)p)->K;
	flag_1c = 1;
}

/*
 * ===========================================================================
 * V92BitsToSymbol::nofBitsForNextTime (.text+0x4e0c0, 100 bytes)
 *
 * How many bits the class wants before it can fill the block that has been
 * asked of it.  The header carries the whole derivation, including why the
 * two arms are written as two different expressions and must not be folded
 * into one -- they disagree when `left * bitsPerFrame` wraps 32 bits, and
 * agree everywhere else.
 *
 * The zero is a result and not an early return: the object clears the result
 * register before the comparison (`xor %esi,%esi` at +0x4e0ce) and falls into
 * the shared epilogue.
 * ===========================================================================
 */
unsigned int
V92BitsToSymbol::nofBitsForNextTime()
{
	unsigned int bits = 0;

	if (symbolsBlockSize > symbolsDone) {
		unsigned int left = symbolsBlockSize - symbolsDone;

		if (left % V92BTOS_SYMBOLS_PER_FRAME != 0)
			bits = (left / V92BTOS_SYMBOLS_PER_FRAME + 1)
			       * bitsPerFrame;
		else
			bits = left * bitsPerFrame
			       / V92BTOS_SYMBOLS_PER_FRAME;
	}

	return bits;
}

/*
 * ===========================================================================
 * V92BitsToSymbol::setSymbolsBlockSize (.text+0x4e130, 108 bytes)
 *
 * Two statements.  The object holds `nofBitsForNextTime` INLINED here rather
 * than called -- there is no relocation on any call in the range and the 108
 * bytes are the 100 above plus the store and one extra `mov` -- so the two
 * functions are the same code twice and `make similarity` sees them that way.
 * Nothing here re-reads +0x18 after storing it, which is what an inline of a
 * member the compiler can see through gives.
 * ===========================================================================
 */
unsigned int
V92BitsToSymbol::setSymbolsBlockSize(unsigned int n)
{
	symbolsBlockSize = n;

	return nofBitsForNextTime();
}

/*
 * ===========================================================================
 * V92BitsToSymbol::process(unsigned char *, unsigned int)
 *                                              (.text+0x4e380, 180 bytes)
 *
 * BITS IN, NOTHING OUT.  The transmitter appends its symbols at
 * `symbols + symbolsDone` and reports how many through a local the address of
 * which is passed; this adds them on and checks the total against the
 * BUFFER's size, `nSymbols`, not against the block size.
 *
 * THE OVERFLOW ARM IS DIAGNOSED AFTER THE FACT AND THE CLAMP DOES NOT REPAIR
 * IT.  By the time `symbolsDone > nSymbols` is true the transmitter has
 * already written past the end of the buffer; setting `symbolsDone` back to
 * `symbolsBlockSize` only tidies the count.  Reproduced -- docs/deviations.md
 * D500.
 * ===========================================================================
 */
int
V92BitsToSymbol::process(unsigned char *bits, unsigned int nbits)
{
	int ret = V92BTOS_OK;

	if (symbolsBlockSize == 0) {
		ret = V92BTOS_SIZE_NOT_SET;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V92BitsToSymbol - error: "
					     "process called, "
					     "SIZE_NOT_SET\r\n");
	} else {
		unsigned int nout;

		transmitter->process(bits, nbits, symbols + symbolsDone, nout);
		symbolsDone += nout;

		if (symbolsDone > nSymbols) {
			ret = V92BTOS_BUFFER_OVERFLOW;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V92BitsToSymbol - "
						     "error: process called, "
						     "BUFFER_OVERFLOW\r\n");
			symbolsDone = symbolsBlockSize;
		}
	}

	if (flag_1c != 0)
		flag_1c = 0;

	return ret;
}

/*
 * ===========================================================================
 * V92BitsToSymbol::process(unsigned int &, short *)
 *                                              (.text+0x4e440, 359 bytes)
 *
 * SYMBOLS OUT, NO BITS IN, and the transmitter is not called at all.
 *
 * THE UNDERFLOW ARM HANDS BACK WHAT IT HAS RATHER THAN REFUSING.  If fewer
 * than `symbolsBlockSize` symbols are staged the whole staging buffer is
 * copied out -- `symbolsDone` of them, not the block -- the count is cleared,
 * and the status says UNDERFLOW.  The caller is told how much it got only
 * through `nbits`, which comes back as the count for NEXT time and not as the
 * count just delivered, so nothing in this overload tells the caller how many
 * of the `symbolsBlockSize` shorts it asked for were actually written.  That
 * is the object's design and not a reading of it -- docs/deviations.md D502.
 *
 * THE SHIFT-DOWN IS SHARED BETWEEN THE TWO ARMS and the compiler proves it:
 * the underflow arm reaches it with `symbolsDone` provably zero, so GCC
 * materialises the bound as `xor %esi,%esi` (+0x4e4e4) and enters the common
 * tail two bytes later at +0x4e4e6 from the other arm.  One loop in the
 * source, two entries in the object.
 * ===========================================================================
 */
int
V92BitsToSymbol::process(unsigned int &nbits, short *out)
{
	int ret = V92BTOS_OK;

	if (symbolsBlockSize == 0) {
		ret = V92BTOS_SIZE_NOT_SET;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V92BitsToSymbol - error: "
					     "process called, "
					     "SIZE_NOT_SET\r\n");
	} else {
		unsigned int i;
		unsigned int j;

		if (symbolsDone < symbolsBlockSize) {
			ret = V92BTOS_BUFFER_UNDERFLOW;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V92BitsToSymbol - "
						     "error: process called, "
						     "BUFFER_UNDERFLOW\r\n");

			for (i = 0; i < symbolsDone; i++)
				out[i] = symbols[i];

			symbolsDone = 0;
		} else {
			for (i = 0; i < symbolsBlockSize; i++)
				out[i] = symbols[i];
		}

		for (i = 0, j = symbolsBlockSize; j < symbolsDone; i++, j++)
			symbols[i] = symbols[j];

		symbolsDone = i;

		nbits = nofBitsForNextTime();
	}

	if (flag_1c != 0)
		flag_1c = 0;

	return ret;
}

/*
 * ===========================================================================
 * V92BitsToSymbol::process(unsigned char *, unsigned int &, short *)
 *                                              (.text+0x4e1a0, 468 bytes)
 *
 * BOTH HALVES IN ONE CALL, and it is not the two above run back to back --
 * the middle differs.  Bits go to the transmitter exactly as in the
 * two-argument form, and then the staged symbols come out exactly as in the
 * other, but the THREE outcomes are decided by one chain: too few staged is
 * UNDERFLOW, enough is the normal path, and more than the buffer holds is
 * OVERFLOW -- which still copies `symbolsBlockSize` out afterwards, where the
 * two-argument form copies nothing at all.
 *
 * `nbits` IS READ BEFORE IT IS WRITTEN.  It arrives holding the number of
 * bits in `bits` (`mov (%ecx),%eax` at +0x4e22f, straight into the
 * transmitter's second argument) and leaves holding what
 * `nofBitsForNextTime` wants next.
 *
 * The reload of `symbolsDone` at +0x4e294, where the two-argument form
 * folded it to a constant, is the overflow arm's doing: that arm stores to it
 * as well, so GCC cannot know its value on the join.
 * ===========================================================================
 */
int
V92BitsToSymbol::process(unsigned char *bits, unsigned int &nbits, short *out)
{
	int ret = V92BTOS_OK;

	if (symbolsBlockSize == 0) {
		ret = V92BTOS_SIZE_NOT_SET;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V92BitsToSymbol - error: "
					     "process called, "
					     "SIZE_NOT_SET\r\n");
	} else {
		unsigned int nout;
		unsigned int i;
		unsigned int j;

		transmitter->process(bits, nbits, symbols + symbolsDone, nout);
		symbolsDone += nout;

		if (symbolsDone < symbolsBlockSize) {
			ret = V92BTOS_BUFFER_UNDERFLOW;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V92BitsToSymbol - "
						     "error: process called, "
						     "BUFFER_UNDERFLOW\r\n");

			for (i = 0; i < symbolsDone; i++)
				out[i] = symbols[i];

			symbolsDone = 0;
		} else {
			if (symbolsDone > nSymbols) {
				ret = V92BTOS_BUFFER_OVERFLOW;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V92BitsToSymbol - error: "
						"process called, "
						"BUFFER_OVERFLOW\r\n");
				symbolsDone = symbolsBlockSize;
			}

			for (i = 0; i < symbolsBlockSize; i++)
				out[i] = symbols[i];
		}

		for (i = 0, j = symbolsBlockSize; j < symbolsDone; i++, j++)
			symbols[i] = symbols[j];

		symbolsDone = i;

		nbits = nofBitsForNextTime();
	}

	if (flag_1c != 0)
		flag_1c = 0;

	return ret;
}
