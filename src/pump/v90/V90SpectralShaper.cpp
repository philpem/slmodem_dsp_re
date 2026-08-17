/*
 * V90SpectralShaper.cpp -- building, resetting and tearing down the shaper.
 *
 * Reconstructed from dsplibs.o.  Four of the class's eight members: the
 * constructor, the destructor and the two resets.  `include/dsplib/V90SpectralShaper.h`
 * carries the object map and the evidence for the 108-byte bound.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THE ORDER OF THE THREE SUBOBJECT ACTIONS IS THE ABI'S, NOT THIS FILE'S.
 * `movb $0x0,0x38(%ebx)` comes before the call to the encoder's constructor,
 * and a constructor BODY cannot run before a member subobject is built -- so
 * that store is a member-initialiser and is written as one.  The encoder at
 * +0x3c is built before the filter at +0x48 because that is declaration
 * order, and the destructor destroys only the encoder because the filter has
 * no destructor to call.
 *
 * THE ALLOCATION SIZE IS NOT COMPUTED FROM +0x34.  Both allocations are
 * `movl $0x30` and the store of 24 into +0x34 comes after them, so the source
 * cannot have read the field back; 24 * sizeof(unsigned short) is written
 * here because `advanceTrellis` indexes both buffers with a stride of two
 * (`movzwl (%reg,%edx,2)`), which makes 0x30 bytes 24 entries.
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90SpectralShaper.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SS_OFF(field, off, tag) \
	typedef char v90ss_off_##tag[ \
	    ((int)__builtin_offsetof(V90SpectralShaper, field) == (off)) \
	    ? 1 : -1]

V90SS_OFF(shaperId,   0x00, shaperid);
V90SS_OFF(shaperSR,   0x04, shapersr);
V90SS_OFF(blockLength, 0x08, blocklen);
V90SS_OFF(word_20,  0x20, word20);
V90SS_OFF(word_24,  0x24, word24);
V90SS_OFF(buf_28,   0x28, buf28);
V90SS_OFF(buf_2c,   0x2c, buf2c);
V90SS_OFF(word_30,  0x30, word30);
V90SS_OFF(word_34,  0x34, word34);
V90SS_OFF(byte_38,  0x38, byte38);
V90SS_OFF(pde,      0x3c, pde);
V90SS_OFF(ssf,      0x48, ssf);
typedef char v90ss_size[(sizeof(V90SpectralShaper) == 0x6c) ? 1 : -1];
#endif

V90SpectralShaper::V90SpectralShaper()
	: byte_38(0), pde(6)
{
	buf_28 = (unsigned short *)
	    sysdep_malloc(24 * sizeof(unsigned short));
	buf_2c = (unsigned short *)
	    sysdep_malloc(24 * sizeof(unsigned short));

	word_34 = 24;
	word_30 = 0;
	word_20 = 0;
}

/*
 * Both pointers are tested and neither is nulled; the encoder's destructor is
 * the implicit member call and is not written out.
 */
V90SpectralShaper::~V90SpectralShaper()
{
	if (buf_28 != 0)
		sysdep_free(buf_28);

	if (buf_2c != 0)
		sysdep_free(buf_2c);
}

/*
 * ===========================================================================
 * V90SpectralShaper::reset -- .text+0x327b0, 200 bytes
 *
 * Everything a connection needs: the two shaper words out of
 * `V90MappingParams`, the derived width, the differential encoder, the
 * embedded filter and its four coefficients, the two products, and the
 * trellis buffer cleared.
 *
 * WHAT THE ARGUMENTS ARE COMES FROM THE CALLER AND FROM `vparse.py`, not from
 * here.  `V90Mapper::reset` is the object's only caller (0x3025c) and it
 * passes `V90MappingParams+0x624`, `+0x620` and the four floats at
 * `+0x628..+0x634` -- which that header already names `shaperId`, `shaperSR`
 * and `shaperA1`/`A2`/`B1`/`B2` from the parameter block itself.  So the
 * names here are the author's, one level removed.
 *
 * `6 / shaperSR` IS AN UNSIGNED DIVIDE AND THE ZERO IS GUARDED.
 * `mov $0x6,%eax; xor %edx,%edx; div %ecx` at 0x327d0 -- `div`, not `idiv`,
 * which agrees with the mangling's `Ejjffff`.  A zero `shaperSR` skips the
 * divide entirely and stores 0 (0x3286c), so the guard is the object's and
 * not defensive programming added here.
 *
 * THE STORE ORDER IS THE SCHEDULER'S at the top: +0x04 goes down at 0x327c3
 * and +0x00 at 0x327c6, from two registers loaded before either.  Written
 * low-to-high, which is 617's position -- a store-order difference is a hint
 * and the acceptance test is full-text identity.
 *
 * THE FILTER'S `blockLength` IS SET BY A DIRECT STORE, not by a setter:
 * `mov %edx,0x20(%ebx)` at 0x32830 with `%ebx` holding `this + 0x48`, and
 * there is no call between it and `setFilterCoeff`.  The class has no setter
 * for that field and the object did not invent one.
 *
 * THE TWO PRODUCTS ARE `shaperId * blockLength` AND `(shaperId + 1) *
 * blockLength`, both from a re-read of +0x00 (0x32833) and the width already
 * in `%edx`.  The `lea 0x1(%ecx),%ebx` before the multiply is the `+ 1`
 * happening on the COUNT and not on the product, so it is not
 * `word_30 + blockLength`.
 *
 * THE BUFFER LOOP IS 24 ENTRIES, WRITTEN AS 24.  `cmp $0x17,%eax; jbe` is a
 * literal bound and not a read of `word_34`, which by then holds a product;
 * the constructor allocates 24 and stores 24 into +0x34, and this function
 * overwrites +0x34 while still clearing all 24.  Only `buf_28` is cleared --
 * `buf_2c` is not touched.
 * ===========================================================================
 */
void
V90SpectralShaper::reset(unsigned int id, unsigned int sr,
			 float a1, float a2, float b1, float b2)
{
	unsigned int i;

	shaperId = id;
	shaperSR = sr;

	if (sr != 0)
		blockLength = 6 / sr;
	else
		blockLength = 0;

	byte_38 = 0;

	pde.reset(blockLength, 0);

	ssf.reset();
	ssf.setFilterCoeff(a1, a2, b1, b2);
	ssf.blockLength = blockLength;

	word_34 = (shaperId + 1) * blockLength;
	word_30 = shaperId * blockLength;

	for (i = 0; i < 24; i++)
		buf_28[i] = 0;

	word_24 = shaperId;
	word_20 = 0;
}

/*
 * ===========================================================================
 * V90SpectralShaper::resetSSFilter -- .text+0x32880, 102 bytes
 *
 * The embedded filter's own two-step setup, and nothing else: no shaper state
 * is touched, no width is recomputed, and `blockLength` is left where `reset`
 * put it.  102 bytes of which almost all is the frame shuffle for a SIBLING
 * CALL -- 0x328d2 writes `this + 0x48` over the incoming `this` slot and
 * 0x328e1 `jmp`s to `setFilterCoeff`, so `setFilterCoeff` must stay the last
 * statement here.
 *
 * THE FIRST COEFFICIENT MAKES A ROUND TRIP THROUGH THE STACK -- `flds
 * 0x34(%esp)` / `fstps 0x18(%esp)` before the call and back afterwards -- and
 * it is a `float` spill, four bytes each way, so it is the identity.  It is
 * the register allocator keeping the value live across `reset()`, not an
 * arithmetic step: a `double` or `long double` spill would be `fstpl`/`fstpt`
 * and would round, which is the distinction finding 1448 turns on.
 * ===========================================================================
 */
void
V90SpectralShaper::resetSSFilter(float a1, float a2, float b1, float b2)
{
	ssf.reset();
	ssf.setFilterCoeff(a1, a2, b1, b2);
}
