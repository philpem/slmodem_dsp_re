/*
 * V90SpectralShaper.cpp -- the V.90 transmit sign-bit spectral shaper.
 *
 * Reconstructed from dsplibs.o.  All eight members and both data tables.
 * `include/dsplib/V90SpectralShaper.h` carries the object map, the 108-byte
 * bound, what the class is FOR and how `actionLookupTable` is built; this file
 * is the code and the per-function evidence.
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
 * cannot have read the field back; 24 * sizeof(short) is written here because
 * every access to either buffer has a stride of two, which makes 0x30 bytes 24
 * entries.  The ELEMENT TYPE is the header's argument and not this one's: two
 * callees take these pointers as `const short *` with no conversion.
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
V90SS_OFF(frameBits,   0x0c, framebits);
V90SS_OFF(codedBits,   0x12, codedbits);
V90SS_OFF(signBits,    0x18, signbits);
V90SS_OFF(state,       0x20, state);
V90SS_OFF(primeFrames, 0x24, primefr);
V90SS_OFF(delayLine,   0x28, delay);
V90SS_OFF(trialLine,   0x2c, trial);
V90SS_OFF(writeIndex,  0x30, wrindex);
V90SS_OFF(windowLength, 0x34, winlen);
V90SS_OFF(oddEncoder,  0x38, oddenc);
V90SS_OFF(pde,      0x3c, pde);
V90SS_OFF(ssf,      0x48, ssf);
typedef char v90ss_size[(sizeof(V90SpectralShaper) == 0x6c) ? 1 : -1];
#endif

/*
 * ===========================================================================
 * V90SpectralShaper::actionLookupTable -- .data+0x8e0, 512 bytes
 *
 * Eight rows of sixteen signed ints, indexed `[state + 2 * shaperId]
 * [candidate]`.  Row `2 * shaperId + state` holds the 2^(shaperId+1)
 * candidates that row can reach and zeros after them, so more than half of
 * the table is padding and the padding is in the object.
 *
 * WHAT A NUMBER MEANS is in the header: `shaperId + 1` decimal digits, one
 * per frame in the delay line, most significant first, each digit being
 * `1 + 2 * previous_bit + this_bit` and therefore an `ACTIONS` plus one.
 *
 * WRITTEN OUT RATHER THAN GENERATED, deliberately.  The rule above reproduces
 * all 128 values, but the object carries an initialised `.data` object and
 * not a builder -- there is no code anywhere in the blob that writes into
 * this table -- so a generator would be a different program that happens to
 * agree.  The differential suite compares these bytes against the blob's.
 * ===========================================================================
 */
int V90SpectralShaper::actionLookupTable[8][16] = {
	{    1,    2,    0,    0,    0,    0,    0,    0,
	     0,    0,    0,    0,    0,    0,    0,    0 },
	{    3,    4,    0,    0,    0,    0,    0,    0,
	     0,    0,    0,    0,    0,    0,    0,    0 },
	{   11,   12,   23,   24,    0,    0,    0,    0,
	     0,    0,    0,    0,    0,    0,    0,    0 },
	{   31,   32,   43,   44,    0,    0,    0,    0,
	     0,    0,    0,    0,    0,    0,    0,    0 },
	{  111,  112,  123,  124,  231,  232,  243,  244,
	     0,    0,    0,    0,    0,    0,    0,    0 },
	{  311,  312,  323,  324,  431,  432,  443,  444,
	     0,    0,    0,    0,    0,    0,    0,    0 },
	{ 1111, 1112, 1123, 1124, 1231, 1232, 1243, 1244,
	  2311, 2312, 2323, 2324, 2431, 2432, 2443, 2444 },
	{ 3111, 3112, 3123, 3124, 3231, 3232, 3243, 3244,
	  4311, 4312, 4323, 4324, 4431, 4432, 4443, 4444 }
};

/*
 * ===========================================================================
 * pow10Table -- .data+0xae0, 20 bytes
 *
 * A plain global, not a member and not `static`: `nm` gives it as an
 * unmangled GLOBAL `D`, which a namespace-scope C++ variable keeps and a
 * `static` one would not.
 *
 * FIVE ELEMENTS OF FOUR BYTES, and the width is the USE SITE's rather than
 * the size's: `divl 0x0(,%esi,4)` at 0x32d32 scales the index by four.  Only
 * indices 0..3 are reachable -- `shaperId` above 3 would run off the end of
 * `actionLookupTable` first -- so the fifth entry is dead in this object.
 *
 * `unsigned` BECAUSE THE DIVIDE IS `divl` AND NOT `idivl`, which forces
 * exactly ONE of the two operands to be unsigned and does not say which.  The
 * other reading is an `unsigned` local in `advanceTrellis` dividing by an
 * `int` table; it emits the same instruction and nothing else in the object
 * touches either.  Recorded rather than guessed at, per finding 3120's rule.
 * ===========================================================================
 */
unsigned int pow10Table[5] = { 1, 10, 100, 1000, 10000 };

V90SpectralShaper::V90SpectralShaper()
	: oddEncoder(), pde(6)
{
	delayLine = (short *)sysdep_malloc(24 * sizeof(short));
	trialLine = (short *)sysdep_malloc(24 * sizeof(short));

	windowLength = 24;
	writeIndex = 0;
	state = 0;
}

/*
 * Both pointers are tested and neither is nulled; the encoder's destructor is
 * the implicit member call and is not written out.
 */
V90SpectralShaper::~V90SpectralShaper()
{
	if (delayLine != 0)
		sysdep_free(delayLine);

	if (trialLine != 0)
		sysdep_free(trialLine);
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
 * `writeIndex + blockLength`.
 *
 * THE BUFFER LOOP IS 24 ENTRIES, WRITTEN AS 24.  `cmp $0x17,%eax; jbe` is a
 * literal bound and not a read of `windowLength`, which by then holds a product;
 * the constructor allocates 24 and stores 24 into +0x34, and this function
 * overwrites +0x34 while still clearing all 24.  Only `delayLine` is cleared --
 * `trialLine` is not touched.
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

	oddEncoder.prev_ = 0;

	pde.reset(blockLength, 0);

	ssf.reset();
	ssf.setFilterCoeff(a1, a2, b1, b2);
	ssf.blockLength = blockLength;

	windowLength = (shaperId + 1) * blockLength;
	writeIndex = shaperId * blockLength;

	for (i = 0; i < 24; i++)
		delayLine[i] = 0;

	primeFrames = shaperId;
	state = 0;
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

/*
 * ===========================================================================
 * V90SpectralShaper::applyFrameAction -- .text+0x328f0, 287 bytes
 *
 * One frame of `blockLength` samples, read from the delay line at `start` and
 * written to `dst` at the same index, with one of four polarity patterns.
 *
 * THE SWITCH IS INSIDE THE LOOP AND THAT IS READABLE FROM THE OBJECT.  The
 * bound test `cmp %ebx,%eax; jbe` at 0x3291a comes BEFORE the four-way
 * comparison tree, and the default arm at 0x32937 is an EMPTY LOOP that still
 * counts `i` up to the bound.  Both are what `-O3` loop unswitching leaves
 * behind when a loop-invariant switch is hoisted out of a loop; a switch
 * written outside the loop would have no default arm at all.  `applyAction`
 * below is the other shape and the two are worth comparing.
 *
 * `(i - start) & 1` IS COUNTED FROM THE FRAME AND NOT FROM THE BUFFER --
 * `sub %ebx,%eax; test $0x1,%al` at 0x32997 -- so a frame's pattern does not
 * depend on where in the line it currently sits.  `advanceTrellis` calls this
 * with `start` 0, where the subtraction folds away and only the `test`
 * survives (0x32f2c), which is how that call site is identifiable.
 *
 * THE SOURCE IS RE-READ EVERY ITERATION: `mov 0x28(%edi),%esi` sits inside
 * each arm's loop rather than above it.  Written as a plain member access,
 * which is what produces that.
 * ===========================================================================
 */
void
V90SpectralShaper::applyFrameAction(ACTIONS action, short *dst, int start)
{
	unsigned int i;

	for (i = start; i < start + blockLength; i++) {
		switch (action) {
		case V90SS_KEEP_ALL:
			dst[i] = delayLine[i];
			break;
		case V90SS_NEGATE_ALL:
			dst[i] = -delayLine[i];
			break;
		case V90SS_NEGATE_EVEN:
			dst[i] = ((i - start) & 1) ? delayLine[i]
						  : -delayLine[i];
			break;
		case V90SS_NEGATE_ODD:
			dst[i] = ((i - start) & 1) ? -delayLine[i]
						  : delayLine[i];
			break;
		}
	}
}

/*
 * ===========================================================================
 * V90SpectralShaper::applyAction -- .text+0x32a10, 387 bytes
 *
 * One whole candidate: `action`'s decimal digits, least significant first,
 * one digit per frame in the delay line.  Digit `m` drives the frame at
 * `(shaperId - m) * blockLength`, so the LEADING digit lands on the frame at
 * offset zero -- the oldest, the one about to leave.
 *
 * THE DIVISION BY TEN IS SIGNED.  `imul $0x66666667` with the `sar $0x1f`
 * correction at 0x32a4d is what GCC emits for `int / 10`; an unsigned divide
 * by ten needs no correction step.  That is what types `actionLookupTable`
 * as `int`, and it agrees with this function's own mangled `int` parameter.
 *
 * THE FOUR ARMS ARE `applyFrameAction` CALLS WITH CONSTANT ACTIONS, and the
 * object says so twice over.  The switch is OUTSIDE any loop -- its default
 * at 0x32a75 jumps straight to the outer increment with no empty loop of the
 * kind `applyFrameAction` itself leaves behind -- and each arm carries a
 * single specialised loop with the bound test in front of it, which is one
 * inlined copy per constant with the inner switch folded away.  A single call
 * passing `(ACTIONS)(digit - 1)` would have inlined to the OTHER shape.
 * ===========================================================================
 */
void
V90SpectralShaper::applyAction(int action, short *dst)
{
	unsigned int m;
	int digit;

	for (m = 0; m <= shaperId; m++) {
		digit = action % 10;
		action /= 10;

		switch (digit) {
		case 1:
			applyFrameAction(V90SS_KEEP_ALL, dst,
					 (shaperId - m) * blockLength);
			break;
		case 2:
			applyFrameAction(V90SS_NEGATE_ALL, dst,
					 (shaperId - m) * blockLength);
			break;
		case 3:
			applyFrameAction(V90SS_NEGATE_EVEN, dst,
					 (shaperId - m) * blockLength);
			break;
		case 4:
			applyFrameAction(V90SS_NEGATE_ODD, dst,
					 (shaperId - m) * blockLength);
			break;
		}
	}
}
