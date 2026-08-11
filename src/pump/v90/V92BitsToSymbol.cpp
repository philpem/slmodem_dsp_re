/*
 * V92BitsToSymbol.cpp -- construction and destruction of the V.92 upstream
 * bit-to-symbol stage.
 *
 * Reconstructed from dsplibs.o.  Four symbols, 378 bytes:
 *
 *     V92BitsToSymbol::V92BitsToSymbol(unsigned, V92Parameters *)
 *                                        .text+0x4ded0 (C2), +0x4df40 (C1)
 *     V92BitsToSymbol::~V92BitsToSymbol() .text+0x4dfb0 (D2), +0x4e010 (D1)
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
