/*
 * V90SpectralShaper.cpp -- the V.90 transmit sign-bit spectral shaper.
 *
 * Reconstructed from dsplibs.o.  All eight members and both data tables.
 * `include/dsplib/V90SpectralShaper.h` carries the object map, the 108-byte
 * bound, what the class is FOR and how `actionLookupTable` is built; this file
 * is the code and the per-function evidence.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215).
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
 * touches either.  Recorded rather than guessed at, per finding F3120's rule.
 * ===========================================================================
 */
unsigned int pow10Table[5] = { 1, 10, 100, 1000, 10000 };

/*
 * THE DESTRUCTOR IS DEFINED FIRST, AND THAT ORDER IS INERT -- MEASURED, NOT
 * INHERITED.  The blob emits this file D2, D1, C1, C2, reset, ...; we emitted
 * C2, C1, D2, D1, ...  Swapping the two definitions moves our emission to
 * D2, D1, C2, C1 -- the blob's first two, exactly -- and **not one byte
 * changes anywhere in the tree**: `reset` stays at 15 differing bytes,
 * `process` at 279, and the four head symbols keep the identity they already
 * had.  That is refinement.md 9a's null with the detector shown to fire (the
 * order demonstrably moved), and it is worth contrasting with
 * V92Transmitter.cpp, where the identical swap traded two symbols for two.
 * The blob's C1-before-C2 clone order is not reachable from source: GCC 3.4.2
 * emits this class's constructor clones C2-first whatever the file says.
 * Kept because it is reorder-only and costs nothing, as 7796's neutral files
 * were.  Finding F7846.
 */

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

	/*
	 * THE FIFTEEN DIFFERING BYTES ARE HERE, AND THEY ARE A BASE REGISTER --
	 * DECLINED, do not "fix" it (finding F7826).  They are one contiguous
	 * run, +0x7c..+0x8a:
	 *
	 *     blob   mov 0x8(%esi),%edx ;  mov %edx,0x20(%ebx)
	 *     ours   mov 0x8(%esi),%edx ;  ... ;  mov %edx,0x68(%esi)
	 *
	 * `ssf` is at +0x48 and 0x48 + 0x20 = 0x68, so BOTH STORE THE SAME
	 * ADDRESS.  The blob reaches it through the pointer still live from the
	 * `setFilterCoeff` call; we recompute it off `this`.  So `--why`'s
	 * "NON-REGISTER OPERAND 0x8(%esi),%edx | (%esi),%ecx" is not a
	 * different field, and it is not statement order either -- this store
	 * is already where the object emits it, and lever 1's 10-cell order
	 * domain reached no preimage.
	 *
	 * Six spellings were compiled and only two emissions exist.  The three
	 * that reach byte identity ALL introduce a local pointer or reference
	 * held live across the two calls; the three spelled through `ssf.` --
	 * including `resetSSFilter()` and including `(&ssf)->blockLength` --
	 * all stay at fifteen.  **The non-shim domain is exhausted with no
	 * preimage**, and nothing in the object proves the author wrote a
	 * local: a base-register choice is `tiers.md`'s FREE column, and the
	 * one mechanism that makes allocation steerable here is lever 3b's
	 * scratch-consuming peephole2, which is emission order and not source
	 * aliasing.  A local added only to lengthen a register's live range is
	 * fitting the compiler, so 7782 declines it and the measurement is the
	 * deliverable.
	 */
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
 * and would round, which is the distinction finding F1448 turns on.
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

/*
 * ===========================================================================
 * V90SpectralShaper::advanceTrellis -- .text+0x32ba0, 1054 bytes
 *
 * The search.  Every one of the 2^(shaperId+1) candidate polarity patterns for
 * the frames still in the delay line is applied to a scratch copy of the whole
 * line, scored by the embedded shaping filter, and the best-scoring one kept;
 * then the winner's LEADING action -- the one belonging to the frame about to
 * leave -- is committed to the line itself and its low bit becomes the next
 * call's trellis state.
 *
 * THE INITIAL SCORE IS `1e38f`, not FLT_MAX and not infinity: `flds` of
 * `.rodata.cst4+0x1c8`, which is 0x7e967699 and the float nearest 1e38.  A
 * four-byte load, so a `float` and not a `double` literal.
 *
 * THE COMPARISON IS `metric < best` WITH `metric` IN `%st(0)`.  `fcoms
 * 0x30(%esp)` at 0x32cdd, then `fnstsw`/`sahf`/`jae` to skip -- an ORDERED
 * compare with no parity test, which is `-mno-ieee-fp` (finding F1990) and not
 * a source choice.  The operand order is the natural one here and needed no
 * correction: the value being tested is a call's return in `%st(0)` and the
 * threshold is the memory operand, which is what `x < local` compiles to when
 * `x` is a temporary.  Finding F3529's swap does not apply, because
 * `tree_swap_operands_p` only swaps when operand 0 is a bare DECL and a
 * function result is not one.
 *
 * THE TWO OPERANDS ARE AT DIFFERENT PRECISIONS ON PURPOSE, and this is the
 * whole of finding F5854.  `best` is a `float` and lives in memory, so storing
 * it ROUNDS; `metric` is a `long double` and never leaves `%st(0)`, so the
 * comparison sees all 64 significand bits of the value `getMetric` just
 * returned.  Declaring `metric` a `float` puts an `fstps`/`flds` pair between
 * the call and the `fcoms` that the object does not have, and that pair
 * changes which candidate wins a tie.
 *
 * STRICT `<`, SO AN EXACT TIE KEEPS THE EARLIER CANDIDATE -- except that with
 * `best` rounded and `metric` not, a tie at 64 bits is not a tie at 32: where
 * the stored `float` rounded UP, the tied later candidate compares strictly
 * smaller and takes it.  That is reachable and common rather than exotic,
 * because the candidates come in exactly-negating pairs (5854).  With `best`
 * starting at 1e38f every reachable metric wins the first comparison, so the
 * only way `bestAction` stays unset is a line whose every candidate scores
 * 1e38f or worse -- unreachable for a sum of squares of 24 shorts.  The object
 * has no initialiser for it and neither has this; see the note on the switch
 * below.
 *
 * THE TABLE IS READ TWICE PER CANDIDATE, at 0x32c36 and again at 0x32d0b, and
 * that is the source and not a rematerialisation.  `applyAction` consumes its
 * argument -- it divides it down to zero -- so after the call the value is
 * gone; a source that had kept it in a local would have kept it in a register
 * across the call, and the object instead recomputes
 * `(state + 2 * shaperId) * 16 + n` from three memory reloads.  Written as two
 * reads of the same expression, which is what produces that.
 *
 * `shaperId` IS RE-READ FROM THE OBJECT AFTER `getMetric` on both arms of the
 * branch (0x32cfa taken, 0x32f56 not), which is what a member access across a
 * call compiles to and is why it is spelled as the member here rather than
 * hoisted into a local.
 *
 * THE LEADING DIGIT IS AN UNSIGNED DIVIDE: `xor %edx,%edx; divl 0x0(,%esi,4)`
 * at 0x32d32.  `divl` and not `idivl`, so one of the two operands is unsigned;
 * `pow10Table` carries it, and the alternative -- an `unsigned` local divided
 * by an `int` table -- emits the identical instruction.  See the note on the
 * table.
 *
 * THE SWITCH ON THE LEADING DIGIT HAS NO DEFAULT, AND THE LOCAL IT WRITES IS
 * LEFT UNINITIALISED WHEN NOTHING MATCHES.  The object stores 0, 1, 2 and 3
 * for digits 1, 2, 3 and 4 from four separate constant loads (0x32f80,
 * 0x32ee1, 0x32f8b, 0x32ed3) and falls through to 0x32d4f with the slot
 * untouched otherwise -- and `act = leading - 1` would have been one `dec`, so
 * the four stores are the source's own switch.  Reproduced with no default arm
 * and no initialiser, because adding either moves code generation.  It is
 * unreachable while `actionLookupTable` holds only digits 1..4, which 5853
 * establishes for all 128 entries.  D930.
 *
 * THE COMMITTED ACTION IS APPLIED WITH `start` ZERO, which is the frame at the
 * head of the line -- the one `process` is about to hand back.  The state
 * update that follows is a second switch over the same value, storing 0, 1, 0,
 * 1 for actions 0..3; `state = act & 1` would have been an `and` and the
 * object has four constant stores at two addresses, so it is a switch as well.
 * ===========================================================================
 */
void
V90SpectralShaper::advanceTrellis()
{
	unsigned int candidates;
	unsigned int n;
	unsigned int i;
	float best;
	int bestAction;
	ACTIONS action;

	candidates = 1u << (shaperId + 1);
	best = 1e38f;

	for (n = 0; n < candidates; n++) {
		long double metric;

		for (i = 0; i < windowLength; i++)
			trialLine[i] = delayLine[i];

		applyAction(actionLookupTable[state + 2 * shaperId][n],
			    trialLine);

		metric = ssf.getMetric(trialLine, shaperId + 1);
		if (metric < best) {
			best = metric;
			bestAction =
			    actionLookupTable[state + 2 * shaperId][n];
		}
	}

	switch (bestAction / pow10Table[shaperId]) {
	case 1:
		action = V90SS_KEEP_ALL;
		break;
	case 2:
		action = V90SS_NEGATE_ALL;
		break;
	case 3:
		action = V90SS_NEGATE_EVEN;
		break;
	case 4:
		action = V90SS_NEGATE_ODD;
		break;
	}

	applyFrameAction(action, delayLine, 0);

	switch (action) {
	case V90SS_KEEP_ALL:
	case V90SS_NEGATE_EVEN:
		state = 0;
		break;
	case V90SS_NEGATE_ALL:
	case V90SS_NEGATE_ODD:
		state = 1;
		break;
	}
}

/*
 * ===========================================================================
 * V90SpectralShaper::process -- .text+0x32fc0, 362 bytes
 *
 * One V.90 frame in, one frame out, `shaperId` frames later.  `in` is
 * `blockLength` samples, `bits` their `blockLength - 1` payload sign bits, and
 * `out` receives the frame leaving the far end of the delay line.
 *
 * POSITION 0 IS NOT A PAYLOAD BIT.  `movb $0x0,0xc(%edi)` at 0x32fd4 forces it
 * to zero and the copy loop shifts the caller's bits up by one, so the caller
 * supplies `blockLength - 1` of them.  `V90SignBitsExtractor::process` spends
 * the same position on the receive side, handing its caller positions
 * 1..width-1 -- the two halves agree, and that is where the trellis's own
 * signalling lives.
 *
 * THE COPY BOUND IS `blockLength - 1` AND THE OBJECT COMPUTES IT AS SUCH:
 * `lea -0x1(%ecx),%ebx; cmp $0x0,%ebx; ja` at 0x32fd8.  A loop written
 * `for (i = 1; i < blockLength; i++)` would have compared `blockLength`
 * against 1 directly.  The subtraction is UNSIGNED and unguarded, which is
 * D930's second half.
 *
 * THE SERIAL ENCODER RUNS ON THE ODD POSITIONS ONLY -- `test $0x1,%bl; je` at
 * 0x33002 -- and the even ones are copied through.  `blockLength` is re-read
 * after the call (0x3301b) because the callee could have changed it, which is
 * what a member as a loop bound compiles to.
 *
 * A ZERO SIGN BIT NEGATES.  `cmpb $0x0,0x18(%ecx,%edi,1)` then `jne` past the
 * `neg` at 0x3306a, so 1 is positive.  The 32-bit `movzwl`/`neg` has its upper
 * half discarded by the 16-bit store, which is finding F614's free case and not
 * evidence about `in`'s signedness.
 *
 * `writeIndex` IS INVARIANT ACROSS EVERY PATH.  The write loop advances a
 * local from it and stores the result back (0x3307d), and the tail subtracts
 * `blockLength` again (0x330cc, 0x3310f) after the line has been shifted down
 * by the same amount.  When `blockLength` is zero the loop is skipped and so
 * is the store, and the tail subtracts zero.  So a test comparing this field
 * before and after is passing trivially, and t_v90specproc.cpp says so rather
 * than claiming it as coverage.
 *
 * THE TRELLIS IS HELD OFF FOR `shaperId` CALLS.  `if (primeFrames != 0)
 * primeFrames--; else advanceTrellis();` -- see 5851 for why that is a
 * countdown and not a copy of `shaperId`.
 *
 * THE OUTPUT IS TAKEN BEFORE THE FILTER RUNS AND BEFORE THE SHIFT, and the
 * filter is fed the delay line's head rather than the outgoing frame -- one
 * pointer, `delayLine`, in both cases (0x330bb).  The shift-down that follows
 * moves `windowLength - blockLength` entries and is written as the object
 * indexes it, source index from `blockLength` and destination from zero.
 *
 * THE +3 IS NOT THE DELAY-LINE LOOP, AND THAT IS AN EXHAUSTED FAMILY RATHER
 * THAN AN OPINION.  This function is the one ABSENCE in its cluster -- 122
 * instructions against the blob's 125 with padding stripped, so lever 2 says
 * a statement of ours is missing -- and the `pos` cursor was the obvious
 * candidate, because the blob's frame is 0x1c against our 0xc and it defers
 * loading `in` until after the first loop, both of which read as more spilling
 * and therefore more locals.  Seven spellings of the loop were compiled on the
 * period compiler (`pos` as written; `writeIndex` incremented in place;
 * `writeIndex + i` with and without the cursor; the cursor assigned inside the
 * body; and the ternary written as an if/else in two of those), giving SIX
 * distinct emissions.  Two keep the blob's 362 bytes and both still differ in
 * 279; the other five change the SIZE.  None is closer.  So the missing
 * statement is elsewhere, and the residual here is block layout -- the blob
 * puts the odd-index encoder call inline after the loop and jumps back, where
 * we send it out of line.  Finding F7846.
 * ===========================================================================
 */
void
V90SpectralShaper::process(short *in, unsigned char *bits, short *out)
{
	unsigned int i;
	unsigned int pos;

	frameBits[0] = 0;
	for (i = 0; i < blockLength - 1; i++)
		frameBits[i + 1] = bits[i];

	for (i = 0; i < blockLength; i++) {
		if (i & 1)
			codedBits[i] = oddEncoder.process(frameBits[i]);
		else
			codedBits[i] = frameBits[i];
	}

	pde.process(codedBits, signBits);

	pos = writeIndex;
	for (i = 0; i < blockLength; i++) {
		delayLine[pos] = signBits[i] ? in[i] : -in[i];
		pos++;
	}
	writeIndex = pos;

	if (primeFrames != 0)
		primeFrames--;
	else
		advanceTrellis();

	for (i = 0; i < blockLength; i++)
		out[i] = delayLine[i];

	ssf.progress(delayLine);

	for (i = blockLength; i < windowLength; i++)
		delayLine[i - blockLength] = delayLine[i];

	writeIndex -= blockLength;
}
