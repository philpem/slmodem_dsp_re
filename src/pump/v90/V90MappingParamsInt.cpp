/*
 * V90MappingParamsInt.cpp -- the free functions over a V90MappingParams.
 *
 * THE FILE NAME IS INFERRED, NOT MEASURED.  `tools/tumap.py` recovers the
 * original's STT_FILE order and reports `V90MappingParamsInt.cpp` between
 * `V90SpectralShapingFilter.cpp` and `V90Resampler.cpp`; the three functions
 * here sit at 0x33330..0x33562, immediately after V90SpectralShapingFilter's
 * last method (0x33280) and before V90Resampler's first (0x34130).  The
 * extent is a shared bracket rather than an exact one, so this is inference
 * from the ordering and not a fact the object states.
 *
 * Reconstructed from dsplibs.o:
 *
 *   0x33330  286  getConstellationsIndex
 *   0x33450  130  getConstellationMask
 *   0x334e0  130  getCodecConstellationMask
 *   0x33670   24  getDataBitRate
 *   0x336b0  622  setParamsInfoFromCPUnPck
 *   0x33920  822  setV92CPpckFromParamsInfo
 *   0x33c60  601  setParamsInfoFromV92CPUnPck
 *   0x33ec0  619  displaySpectralParams
 *
 * All six are UNMANGLED `T` symbols, so the original declared them
 * `extern "C"`; a plain C++ prototype would emit `_Z...` and be a different
 * function.  The first three hold NO relocations at all -- `tools/dis.py`
 * prints its "N relocation(s) in this range" banner only when there are some,
 * and for 0x33330..0x33570 it prints nothing -- so they reference no data
 * symbol and call nothing, not even a debug printf.  Every constant is
 * immediate and every table is reached off the first argument.
 *
 * `setV92CPpckFromParamsInfo` holds none either, which is why its placement
 * rests entirely on the bracket above; `displaySpectralParams` holds fifteen
 * and every one of them is `edprintf` or a string.  Their own blocks below
 * carry the evidence.
 *
 * THREE MORE SYMBOLS ARE IN THE SAME BRACKET AND ARE NOT WRITTEN:
 * `setConstellationMask` (0x33570), `setCodecConstellationMask` (0x335f0) and
 * `setDataBitRate` (0x33690).  `getDataBitRate` used to be a fourth,
 * `setParamsInfoFromCPUnPck` a fifth and `setParamsInfoFromV92CPUnPck`
 * (0x33c60) a sixth; all three are now written, below.
 * `setParamsInfoFromCPUnPck` and `setParamsInfoFromV92CPUnPck` are NOT
 * `src/pump/v90/V92ParamsInfo.c`'s -- that
 * file's `V92setParamsInfoFromCPUnPck` is a different symbol at .text+0x12f00
 * -- and the name similarity is exactly the trap `tools/tumap.py` exists to
 * avoid.
 *
 * ALL THREE ARE BODIES `setParamsInfoFromCPUnPck` INLINES, and are written
 * below as `static` helpers.  `setParamsInfoFromV92CPUnPck` inlines the same
 * three, so between them the two unpackers hold six mask bodies and two rate
 * bodies, and GCC 3.4.2 inlines all eight calls exactly as the original's
 * compiler did -- `tools/dis.py` finds no relocation naming any of the three
 * from inside either function, and our object has none either.
 *
 * The two mask helpers are the same body with the register and stack-slot
 * allocation each inline site forces rather than the standalone function's;
 * their INNER LOOP is byte-identical to the object's at all three sites, from
 * the `test $0x1,%cl` to the `jns` that closes the word loop.  The rate
 * setter's inlined body is byte-identical to the object's outright, registers
 * included.
 *
 * They are `static` and not `extern "C"` because writing the three GLOBALS is
 * a separate 284 bytes of reconstruction with differential tests of their
 * own, which this batch did not take on; whoever takes it can delete the
 * `static` and the four calls become the object's own.
 *
 * WHAT `getConstellationsIndex` DOES, and the one thing about it worth
 * pausing over.  It walks constellations 1 to 5 looking for an earlier one
 * that is the same, and the sameness test is:
 *
 *	xor  %esi,%esi			n = 0
 *	cmp  $0x0,%ebx			length
 *	jbe  .Ldone			  ... and if it is zero, skip the loop
 *	  ... compare `length` bytes of both tables, breaking on the first
 *	      difference with `n` left at the index that differed ...
 *   .Ldone:
 *	cmp  %esi,%ebx			n == length ?
 *	jne  .Lnext
 *
 * so a length of zero leaves n = 0, the test compares 0 against 0, and the
 * two constellations are declared identical having compared nothing.  TWO
 * EMPTY CONSTELLATIONS ARE THE SAME CONSTELLATION.  That is the object's
 * behaviour, not an accident of this transcription, and t_v90cmask drives it
 * on purpose rather than avoiding it.
 *
 * Names are invented -- an unmangled symbol carries none -- and describe what
 * the object does with the value.  See V90MappingParams.h for the layout and
 * for what is measured about it.
 */

#include <math.h>

#include "dsplib/V90MappingParams.h"

#include "dsplib/V90CPUnPck.h"
#include "dsplib/V92CP.h"
#include "dsplib/encode.h"
#include "dsplib/tagV90AdditionalCPinfo.h"

/*
 * The distinct constellations, in first-seen order.
 *
 * Constellation 0 is always group 0 and is always its own representative;
 * the object writes both before the loop starts.  The return leaves the
 * group count in %eax and nothing distinguishes signed from unsigned, but the
 * value is a count bounded by 6 and the counter it comes from is compared
 * with `jae` and `jbe` throughout, so it is unsigned here.
 */
extern "C" unsigned int
getConstellationsIndex(V90MappingParams *params, int *group)
{
	unsigned int count = 1;
	unsigned int i;

	params->distinctIndex[0] = 0;
	group[0] = 0;

	for (i = 1; i <= 5; i++) {
		unsigned int j;
		unsigned int length = params->constellationSize[i];

		for (j = 0; j < count; j++) {
			int k = params->distinctIndex[j];
			unsigned int n;

			if (params->constellationSize[k] != length)
				continue;

			/*
			 * `n` survives the loop and is the acceptance test,
			 * which is why the zero-length case reads as a match.
			 */
			for (n = 0; n < length; n++) {
				if (params->constellation[k][n] !=
				    params->constellation[i][n])
					break;
				if (params->codecConstellation[k][n] !=
				    params->codecConstellation[i][n])
					break;
			}
			if (n == length) {
				group[i] = group[k];
				break;
			}
		}

		if (j == count) {
			params->distinctIndex[count] = (int)i;
			group[i] = (int)count;
			count++;
		}
	}

	return count;
}

/*
 * `mask[byte >> 4] |= 1 << (byte & 15)` over one constellation.
 *
 * Only eight entries are cleared, and the object shifts the byte right by
 * four in an 8-bit register (`mov %dl,%al; shr $0x4,%al`) with no further
 * masking, so a table byte of 0x80 or more addresses entry 8 to 15 -- which
 * the function never cleared and the caller may not have sized for.  Both
 * halves of that are the object's and are reproduced.
 *
 * The two functions differ in one expression, the table they read, and the
 * object writes them out twice rather than sharing a helper: the two bodies
 * are byte-identical apart from the `lea` displacement (0x4 against 0x304)
 * and are 130 bytes each.  They are written out twice here for the same
 * reason.
 */
extern "C" void
getConstellationMask(V90MappingParams *params, int which, short *mask)
{
	const unsigned char *table;
	unsigned int length;
	unsigned int n;
	int k, i;

	k = params->distinctIndex[which < 6 ? which : 0];
	length = params->constellationSize[k];
	table = params->constellation[k];

	for (i = 0; i <= 7; i++)
		mask[i] = 0;

	for (n = 0; n < length; n++) {
		unsigned char v = table[n];

		mask[v >> 4] = (short)(mask[v >> 4] | (1 << (v & 15)));
	}
}

extern "C" void
getCodecConstellationMask(V90MappingParams *params, int which, short *mask)
{
	const unsigned char *table;
	unsigned int length;
	unsigned int n;
	int k, i;

	k = params->distinctIndex[which < 6 ? which : 0];
	length = params->constellationSize[k];
	table = params->codecConstellation[k];

	for (i = 0; i <= 7; i++)
		mask[i] = 0;

	for (n = 0; n < length; n++) {
		unsigned char v = table[n];

		mask[v >> 4] = (short)(mask[v >> 4] | (1 << (v & 15)));
	}
}

/*
 * ===========================================================================
 * setConstellationMask (.text+0x33570, 128 bytes) and
 * setCodecConstellationMask (.text+0x335f0, 128 bytes), as the STATIC helper
 * `setParamsInfoFromCPUnPck` inlines rather than as the two globals.
 * ===========================================================================
 *
 * The exact inverse of `getConstellationMask` above: that one turns a table
 * of bytes into an occupancy bitmap, and this one turns a bitmap back into
 * the table, rewriting the length as it goes.
 *
 * WHY THEY ARE `static` AND NOT THE OBJECT'S TWO GLOBALS.  Both globals
 * exist, at the addresses in the heading, and both are `T`.  This batch was
 * scoped to `setParamsInfoFromCPUnPck`, which INLINES all three of its calls
 * to them -- the object holds no relocation naming either symbol from inside
 * it, and `tools/closure.py --missing setParamsInfoFromCPUnPck` is one symbol,
 * itself.  So the two bodies are needed here and the two globals are not, and
 * writing the globals is 256 further bytes with a differential test of their
 * own.  `static` is the honest spelling of that: the code is the object's,
 * the symbol is not claimed.  Delete the `static` and add a prototype to
 * V90MappingParams.h to claim them.
 *
 * THE `int which` IS THE FORCED PART AND IS WHY THIS IS A FUNCTION AT ALL.
 * `setParamsInfoFromCPUnPck`'s three loops count with an UNSIGNED variable --
 * `cmpl $0x5,(%esp) ; jbe` at .text+0x338de -- and each inlined body then
 * clamps that same variable with a SIGNED test, `cmp $0x6,%ebp ; setl` at
 * .text+0x3373c, +0x337e0 and +0x33872.  One variable cannot be compared both
 * ways; a CALL whose argument is `(int)i` can, because the bound is the
 * caller's and the clamp is the callee's.  That is finding F5821's measurement,
 * made on `setV92CPpckFromParamsInfo` in this same file, and it is what says
 * the author wrote a call here rather than three open-coded loops.
 *
 * WHAT THE BITMAP MEANS.  Eight `short`, scanned from `mask[7]` down to
 * `mask[0]`, and within each word bit 0 first with the emitted byte counting
 * DOWN from `j * 16 + 15`.  So the byte written for bit `b` of word `j` is
 * `j * 16 + (15 - b)`, the most significant bit of word 7 is byte 127, and
 * the table comes out in strictly DESCENDING byte order.  The whole sequence
 * is 128 possible entries into a 128-byte table, so an all-ones bitmap fills
 * one constellation exactly and cannot overrun it.
 *
 * `movswl` on the word and `sar` on the shift, so the working copy is a
 * SIGNED 32-bit value; the top bit therefore replicates, which is invisible
 * because the loop stops after sixteen tests either way.  The sum is added in
 * an 8-bit register (`mov %edi,%eax ; add %bl,%al`), which is the cast to
 * `unsigned char`; 112 + 15 is 127 so it never wraps, and that arm of the
 * cast is unobservable rather than tested.
 *
 * THE LENGTH IS A MEMORY LVALUE THROUGHOUT, and that is forced rather than
 * stylistic: the object re-reads it before the store (`mov (%esi),%edx`) and
 * increments it in place afterwards (`incl (%esi)`), because the byte just
 * written could have BEEN it -- the table and the length are in the same
 * object and a long enough table reaches the length array.  Caching it in a
 * register would be a different function on exactly the inputs that overrun.
 */
static void
setConstellationMaskInline(V90MappingParams *params, int which,
			   const short *mask)
{
	unsigned int c = (unsigned int)(which < 6 ? which : 0);
	unsigned char *table = params->constellation[c];
	unsigned int *size = &params->constellationSize[c];
	int j, k;

	*size = 0;

	for (j = 7; j >= 0; j--) {
		int v = mask[j];

		for (k = 15; k >= 0; k--) {
			if ((v & 1) != 0) {
				table[*size] = (unsigned char)(j * 16 + k);
				(*size)++;
			}
			v >>= 1;
		}
	}
}

/*
 * The same over the second table, and the ONE difference is the destination
 * base -- 0x4 against 0x304.  The length it writes is the SAME word, so a
 * call to this one after a call to the other leaves
 * `constellationSize[which]` describing the CODEC table and not the first
 * one.  See `setParamsInfoFromCPUnPck`, which does exactly that.
 *
 * Written out twice for the reason `getConstellationMask` and
 * `getCodecConstellationMask` are: the object holds two 128-byte functions
 * that are byte-identical apart from that displacement, and does not share a
 * helper between them.
 */
static void
setCodecConstellationMaskInline(V90MappingParams *params, int which,
				const short *mask)
{
	unsigned int c = (unsigned int)(which < 6 ? which : 0);
	unsigned char *table = params->codecConstellation[c];
	unsigned int *size = &params->constellationSize[c];
	int j, k;

	*size = 0;

	for (j = 7; j >= 0; j--) {
		int v = mask[j];

		for (k = 15; k >= 0; k--) {
			if ((v & 1) != 0) {
				table[*size] = (unsigned char)(j * 16 + k);
				(*size)++;
			}
			v >>= 1;
		}
	}
}

/*
 * THE TWO GLOBALS THEMSELVES, claimed by the VPcmV34Main leaf pass.  The
 * paragraph above priced the move ("delete the `static` and add a
 * prototype"); it is made as a WRAPPER instead, because fifty-five mutation
 * anchors in v90unpck/v92mpunpck name the `*Inline` spellings and renaming
 * the helpers would orphan them all.  GCC inlines a same-TU callee at -O3,
 * so each wrapper's body IS the helper's -- one symbol, the blob's 128
 * bytes -- and the call sites above keep the factoring F5821 measured.
 * `extern "C"` because both blob symbols are unmangled; neither has a
 * caller anywhere in the object.
 */
extern "C" void
setConstellationMask(V90MappingParams *params, int which, const short *mask)
{
	setConstellationMaskInline(params, which, mask);
}

extern "C" void
setCodecConstellationMask(V90MappingParams *params, int which,
			  const short *mask)
{
	setCodecConstellationMaskInline(params, which, mask);
}

/*
 * ===========================================================================
 * getDataBitRate (.text+0x33670, 24 bytes)
 * ===========================================================================
 *
 * Twenty-four bytes and two arms, and it is the out-of-line twin of the four
 * lines `setV92CPpckFromParamsInfo` ends with:
 *
 *	mov  0x8(%esp),%edx		the flag
 *	mov  0x4(%esp),%eax		the block
 *	test %edx,%edx
 *	je   .Lshort
 *	mov  (%eax),%eax  ;  sub $0x14,%eax  ;  ret
 *   .Lshort:
 *	mov  (%eax),%eax  ;  sub $0x8,%eax   ;  ret
 *
 * TWO CONSTANTS AND NOT ONE EXPRESSION, exactly as in the twin: each arm has
 * its own subtract, its own move and its own `ret`.
 *
 * THE FIRST PARAMETER'S TYPE IS INFERENCE FROM ONE CALL SITE, and it is the
 * only call site: `readelf -r` finds exactly ONE relocation naming this
 * symbol in the whole object, at .text+0x3c996 inside `V90CPPacker`, which
 * passes its own first argument.  The function itself only does `mov (%eax)`,
 * so anything with an `int` at offset 0 would satisfy it.  The second
 * parameter is `info->word_04` at the same site, tested and not otherwise
 * used, so its signedness is not forced either.
 *
 * THE RETURN IS SIGNED and its five low bits are what reach the message:
 * `V90CPPacker` shifts it with `sar`.  Nothing bounds `params->word_0`, so a
 * value below 0x14 returns a negative rate and the five bits sent are its
 * two's complement -- reproduced, not guarded.
 */
extern "C" int
getDataBitRate(V90MappingParams *params, int islong)
{
	if (islong != 0)
		return (int)params->word_0 - 0x14;

	return (int)params->word_0 - 8;
}

/*
 * `setDataBitRate` (.text+0x33690, 28 bytes), as the STATIC helper
 * `setParamsInfoFromCPUnPck` inlines rather than as the global.
 *
 * The exact inverse of `getDataBitRate` above and the same two constants:
 *
 *	mov  0x8(%esp),%ecx	  islong
 *	mov  0x4(%esp),%edx	  the block
 *	mov  0xc(%esp),%eax	  the rate
 *	test %ecx,%ecx
 *	je   .Lshort
 *	add  $0x14,%eax  ;  mov %eax,(%edx)  ;  ret
 *   .Lshort:
 *	add  $0x8,%eax   ;  mov %eax,(%edx)  ;  ret
 *
 * TWO CONSTANTS AND NOT ONE EXPRESSION, exactly as in the getter.
 *
 * THAT IT IS A CALL AND NOT AN OPEN-CODED `if` IS MEASURED, and it is the
 * same class of measurement as the mask helper's `int which`.  The object
 * loads the rate ONCE, before the test (`mov 0x14(%ecx),%edx` at
 * .text+0x338f2, then `test %esi,%esi`), and adds with `lea` in each arm.
 * Written as `if (flag) p->word_0 = rate + 0x14; else p->word_0 = rate + 8;`
 * GCC 3.4.2 SINKS the load into both arms and uses `add` -- measured, not
 * assumed: that is what this file emitted before the call form was tried.  A
 * function argument has to be evaluated before the call, so the call form is
 * what produces one load, and with it our tail is the object's instruction
 * for instruction.
 *
 * `static` for the same reason as the two mask helpers: the global is 28
 * further bytes with a differential test of its own and is not claimed here.
 */
static void
setDataBitRateInline(V90MappingParams *params, int islong, int rate)
{
	if (islong != 0)
		params->word_0 = (unsigned int)(rate + 0x14);
	else
		params->word_0 = (unsigned int)(rate + 8);
}

/* The global, on the same wrapper terms as the two mask setters above. */
extern "C" void
setDataBitRate(V90MappingParams *params, int islong, int rate)
{
	setDataBitRateInline(params, islong, rate);
}

/*
 * ===========================================================================
 * setParamsInfoFromCPUnPck (.text+0x336b0, 622 bytes)
 * ===========================================================================
 *
 * Fill a `V90MappingParams` from an unpacked V.90 CP message.  It is the
 * inverse of `setV92CPpckFromParamsInfo` below, over the V.90 message instead
 * of the V.92 one, and `setParamsInfoFromV92CPUnPck` (.text+0x33c60, written
 * below) is the third corner: same destination, `V92CP` as the source.
 *
 * ---------------------------------------------------------------------------
 * IT HAS NO CALLER, AND THAT IS THE POINT RATHER THAN AN OVERSIGHT.
 * `readelf -r` finds ZERO relocations naming this symbol anywhere in the
 * 1.2 MB object -- not a call, not a data reference, nothing.  It is
 * reconstructed here and left exactly that way: **nothing in this tree calls
 * it and nothing should be made to**.  Supplying a call site would be new
 * code with no blob behaviour to compare against, which is not
 * reconstruction.  The differential test drives it directly.
 *
 * WHY IT MATTERS ANYWAY.  Finding F7520 established that the V.90 DIGITAL side
 * has no reachable writer for the `V90MappingParams` block
 * `V90Modulator::progress` reads, bounded over the thirty symbols whose
 * MANGLING names a `V90MappingParams *`.  This function writes essentially
 * that whole block -- `word_0`, both 6 x 128 byte tables, `constellationSize`,
 * `word_61c`, the six shaper words and `distinctIndex` -- and it was outside
 * 7520's population by construction, because it is `extern "C"` and has no
 * mangled name to be found by.  So the writer exists; what it does not have
 * is a caller.  See finding F7570.
 * ---------------------------------------------------------------------------
 *
 * FOUR STEPS, AND ONLY THE THIRD IS CONDITIONAL.
 *
 *   1. Seven scalars, copied with no arithmetic.  The gate byte becomes an
 *      exact 0 or 1 in `word_61c` (`cmpb $0x0 ; setne`), and the six spectral
 *      words go across whole.
 *   2. `distinctIndex[0..5]`, widened from bytes.  A separate loop from 3:
 *      the object closes it at .text+0x3371e and opens the next at +0x33720.
 *   3. The six constellations, then the six codec constellations.  ONE flag
 *      selects the codec source and it is re-read on every iteration
 *      (.text+0x337c5, inside the loop the back edge at +0x338e2 closes), so
 *      the `if` is inside the loop and not around it.  With the flag CLEAR
 *      the codec tables are unpacked from the ORDINARY bitmaps -- the same
 *      source, the other destination -- rather than being skipped.
 *   4. `word_0`, the data bit rate plus 0x14 or plus 8, the exact inverse of
 *      `getDataBitRate` above.
 *
 * THE FLAG IS READ FROM THE SOURCE AND NOT FROM WHAT STEP 1 WROTE, which is
 * the opposite of `V92setParamsInfoFromCPUnPck` (src/pump/v90/V92ParamsInfo.c,
 * whose head records that its three gates read back the DESTINATION's copies).
 * Here .text+0x337c5 is `cmpb $0x0,0x30(%ecx)` with %ecx reloaded from the
 * second argument, so a caller that scribbled on `params->word_61c` between
 * two calls would change nothing.
 *
 * `constellationSize` ENDS UP DESCRIBING THE CODEC TABLE.  Step 3's second
 * half zeroes and refills the SAME length word the first half just wrote
 * (.text+0x337ff and +0x3388e both store to `0x604(%ebx)`), so when the two
 * bitmaps differ in population the first table keeps entries past the length
 * that nothing will read.  With the flag clear the two bitmaps are the same
 * bitmap and the counts agree, which is why this needs a fixture that sets
 * the flag and gives the two bitmaps different weights.
 *
 * THE SIX `distinctIndex` BYTES ARE READ TWICE, once into the destination and
 * once per loop as the bitmap selector, and the second read is from the
 * SOURCE (`movzbl 0x31(%ebp,%ecx,1)` with %ecx the CP block).  Nothing masks
 * or bounds them; see include/dsplib/V90CPUnPck.h.
 */
extern "C" void
setParamsInfoFromCPUnPck(V90MappingParams *params, V90CPUnPck *cp)
{
	unsigned int i;

	params->shaperSR = cp->shaperSR;
	params->shaperId = cp->shaperId;
	params->shaperA1 = cp->shaperA1;
	params->shaperA2 = cp->shaperA2;
	params->shaperB1 = cp->shaperB1;
	params->shaperB2 = cp->shaperB2;
	params->word_61c = (cp->codecConstellationPresent != 0);

	for (i = 0; i < V90_CPUNPCK_CONSTELS; i++)
		params->distinctIndex[i] = cp->distinctIndex[i];

	for (i = 0; i < V90_CPUNPCK_CONSTELS; i++)
		setConstellationMaskInline(params, (int)i,
			cp->constellationMask[cp->distinctIndex[i]]);

	for (i = 0; i < V90_CPUNPCK_CONSTELS; i++) {
		if (cp->codecConstellationPresent != 0)
			setCodecConstellationMaskInline(params, (int)i,
			    cp->codecConstellationMask[cp->distinctIndex[i]]);
		else
			setCodecConstellationMaskInline(params, (int)i,
			    cp->constellationMask[cp->distinctIndex[i]]);
	}

	setDataBitRateInline(params, (int)cp->info->word_04,
			     (int)cp->dataBitRate);
}

/*
 * ===========================================================================
 * setV92CPpckFromParamsInfo (.text+0x33920, 822 bytes)
 * ===========================================================================
 *
 * Fill a `V92CP` from a `V90MappingParams` and the record beside it.  Three
 * arguments, and the two source types are the object's own: the call sites
 * are `VPcmFloModem::runPcmModem` at .text+0xe7d8 and .text+0xe895, which
 * pass `this + 0x1770` or `this + 0x1dc0` and `this + 0x2410`, and
 * `VPcmFloModem` embeds a `V90Modem` at +0x1758 whose `mappingParams`,
 * `mappingParamsAlt` and `additionalCPinfo` are at +0x18, +0x668 and +0xcb8.
 * The V.90 twin of this function names both types in its mangling --
 * `V90CPPacker(V90MappingParams *, tagV90AdditionalCPinfo *, short *, int)`.
 * Argument 3 is `*(V92CP **)(this + 0x6bc8)`, which is `V92Modem::cp`.
 *
 * WHY IT IS IN THIS FILE.  It is unmangled `T`, so `extern "C"`; it holds NO
 * relocations at all, so no call and no data reference says anything about
 * its translation unit; and its ADDRESS is the whole of the evidence.
 * `tools/tumap.py` recovers the original's `STT_FILE` order and reports it as
 * `V90SpectralShapingFilter.cpp`, `V90MappingParamsInt.cpp`,
 * `V90Resampler.cpp`, with `anchor order matches FILE order: True`;
 * `V90SpectralShapingFilter`'s last method ends at 0x33280 and
 * `V90Resampler`'s first begins at 0x34130, so everything between belongs to
 * the file in the middle.  That is a SHARED BRACKET and not an exact extent
 * -- `setConstellationMask`, `setCodecConstellationMask`, `getDataBitRate`,
 * `setDataBitRate`, `setParamsInfoFromCPUnPck` and `setParamsInfoFromV92CPUnPck`
 * are in it too and are unwritten -- so this is inference from the ordering,
 * exactly as the head of this file says about its own three.
 *
 * IT IS NOT `V92ParamsInfo.c`'s, and that was worth checking rather than
 * assuming: that file holds `V92setParamsInfoFromCPUnPck` at .text+0x12f00,
 * which is a DIFFERENT symbol 0x20000 away, and the two `setParamsInfo*`
 * functions whose names look like this one's siblings are the unwritten
 * neighbours in the bracket above.
 *
 * ---------------------------------------------------------------------------
 * WHAT IT DOES.  Four steps, and the first three are this file's own three
 * functions inlined by the compiler rather than open-coded by the author:
 *
 *   1. `getConstellationsIndex` over the six constellations, writing the
 *      group numbers into `cp->word_28` and the count into `cp->word_10c`.
 *   2. eight scalars out of the two sources into the message block.
 *   3. `getConstellationMask` for each group, into `cp->short_42[i]`, and
 *      `getCodecConstellationMask` the same way into `cp->short_a2[i]` when
 *      `cp->byte_24` is non-zero -- which is the gate `V92CP::infoToBits`
 *      applies to the same block on the way out.
 *   4. one derived byte: `params->word_0` less 0x14 or 8 according to
 *      `cp->char_01`.
 *
 * THAT THE THREE ARE CALLS AND NOT COPIES IS MEASURABLE, and the measurement
 * is the loop counter's SIGNEDNESS.  The mask loops bound their counter
 * against `cp->word_10c` with `ja` -- unsigned -- and then clamp it with
 * `cmp $0x5,%eax; jle` -- SIGNED.  One variable cannot be both; a call whose
 * argument is `(int)i` can, because the bound is the caller's and the clamp
 * is the callee's.  Finding F5821.
 *
 * NOTHING BOUNDS `cp->word_10c` HERE EITHER.  It is whatever
 * `getConstellationsIndex` returned, which is 1..6, so in practice the two
 * blocks are never overrun by this writer -- but the loops trust the field
 * rather than the return value, and a caller that raised it between the two
 * would write past both.  Same shape as D570, which measured it on
 * `infoToBits`; the differential trials keep it in range, because a trial
 * that reaches undefined behaviour in the reconstruction is not a trial
 * (D561).
 */
extern "C" void
setV92CPpckFromParamsInfo(V90MappingParams *params,
			  tagV90AdditionalCPinfo *info, V92CP *cp)
{
	unsigned int i;

	cp->word_10c = (unsigned short)getConstellationsIndex(params,
							      cp->word_28);

	/*
	 * The eight scalars.  Five come from the record and three of those
	 * are narrowed to a byte by the destination; four are the spectral
	 * shaper's, copied whole, and one is the write-only word at +0x61c.
	 *
	 * THE FLOATS ARE PLAIN ASSIGNMENTS AND THE OBJECT'S `movl` IS WHAT
	 * GCC EMITS FOR ONE, which was probed on the period compiler rather
	 * than assumed -- a float copy under `-mfpmath=387` could as easily
	 * have been `flds`/`fstps`, and if it had been, the object's `movl`
	 * would have meant the author copied the words rather than the
	 * values.  It is not; finding F5820.
	 */
	cp->byte_04 = (unsigned char)info->word_00;
	cp->char_01 = (signed char)info->word_04;
	cp->byte_03 = (unsigned char)info->word_0c;
	cp->flt_10 = info->float_08;
	cp->suv = info->word_10;

	cp->word_08 = (unsigned int)params->shaperSR;
	cp->word_0c = params->shaperId;
	cp->flt_14 = params->shaperA1;
	cp->flt_18 = params->shaperA2;
	cp->flt_1c = params->shaperB1;
	cp->flt_20 = params->shaperB2;
	cp->byte_24 = (unsigned char)params->word_61c;

	for (i = 0; i < cp->word_10c; i++)
		getConstellationMask(params, (int)i, cp->short_42[i]);

	if (cp->byte_24 != 0)
		for (i = 0; i < cp->word_10c; i++)
			getCodecConstellationMask(params, (int)i,
						  cp->short_a2[i]);

	/*
	 * The closing byte, and the two constants are 0x14 and 8 rather than
	 * one expression: the object branches on `char_01` and each arm has
	 * its own subtract, its own store and its own epilogue.
	 */
	if (cp->char_01 != 0)
		cp->char_02 = (signed char)(params->word_0 - 0x14);
	else
		cp->char_02 = (signed char)(params->word_0 - 8);
}

/*
 * ===========================================================================
 * setParamsInfoFromV92CPUnPck (.text+0x33c60, 601 bytes)
 * ===========================================================================
 *
 * The third corner of the same triangle: `setParamsInfoFromCPUnPck` fills a
 * `V90MappingParams` from the V.90 message, `setV92CPpckFromParamsInfo` fills
 * a `V92CP` from the block, and this one fills the block from a `V92CP`.  It
 * writes exactly the members the V.90 unpacker writes -- +0x000, +0x604,
 * +0x61c, +0x620..+0x634, +0x638 and the two 6 x 128 byte tables -- and is the
 * inverse of `setV92CPpckFromParamsInfo` above field for field.
 *
 * ---------------------------------------------------------------------------
 * IT HAS NO CALLER, EXACTLY AS THE V.90 TWIN HAS NONE.  `readelf -r` finds
 * ZERO relocations of any type naming this symbol anywhere in the 1.2 MB
 * object, and `tools/dis.py` over 0x33c60..0x33eb9 prints no relocation banner,
 * so the range holds no call and no data reference either.  The two
 * relocations in the object whose name looks like this one's are
 * `V92setParamsInfoFromCPUnPck`'s -- a DIFFERENT symbol at .text+0x12f00,
 * 2,695 bytes, called twice from `VPcmFloModem::runPcmModem` (finding F7571).
 * **No call is added here and none should be.**  The differential test reaches
 * it directly.
 * ---------------------------------------------------------------------------
 *
 * THE SOURCE TYPE IS A `V92CP`, AND IT WAS CHECKED RATHER THAN ASSUMED.
 * Twelve distinct displacements are formed off the second argument and every
 * one of them lands on a member `include/dsplib/V92CP.h` already declares, at
 * the width the object reads it:
 *
 *	+0x01	`char_01`	`cmpb $0x0,0x1(%esi)`, the rate gate
 *	+0x02	`char_02`	`movsbl 0x2(%esi),%eax`, SIGNED
 *	+0x08	`word_08`	`mov 0x8(%edx),%ebx`   -> `shaperSR`
 *	+0x0c	`word_0c`	                       -> `shaperId`
 *	+0x14	`flt_14`	                       -> `shaperA1`
 *	+0x18	`flt_18`	                       -> `shaperA2`
 *	+0x1c	`flt_1c`	                       -> `shaperB1`
 *	+0x20	`flt_20`	                       -> `shaperB2`
 *	+0x24	`byte_24`	`cmpb $0x0,0x24(%edx)`, the codec gate
 *	+0x28	`word_28[6]`	`mov 0x28(%edi,%edx,4)`, a FOUR-byte stride
 *	+0x42	`short_42[6][8]` `lea 0x42(%ebx,%ecx,1)` with %ebx = k << 4
 *	+0xa2	`short_a2[6][8]` `lea 0xa2(%edi,%ecx,1)`, the same shape
 *
 * so NO NEW TYPE IS DECLARED and none is needed.  The corroboration is that
 * this is `setV92CPpckFromParamsInfo` run backwards through the same twelve
 * fields -- that function's source is a `V92CP` because its two call sites in
 * `runPcmModem` pass `*(V92CP **)(this + 0x6bc8)`, which is `V92Modem::cp` --
 * and the two agree on every field, every width and both constants.
 *
 * **AND THAT IS NOT FINDING F7572's TRAP IN A NEW COSTUME**, which was checked
 * rather than waved away.  7572 records that `V90CP` and `V92CPUnPck` are two
 * NAMES FOR ONE STRUCTURE -- the 0xca0-byte message block inline at
 * `VPcmFloModem+0x254c`, which is `V90Modem::cp`.  `V92CP` is a different
 * object: 0x918 bytes, `sysdep_malloc`'d and constructed by `V92Modem::V92Modem`,
 * reached as `V92Modem::cp`, and its layout has nothing in common with that
 * one at any offset used here (+0x28 is six four-byte entries where the
 * message block has `M[12]`; +0x42 and +0xa2 are two mask blocks where the
 * message block has four 384-short coefficient arrays).  This function's
 * displacements fit `V92CP` and do not fit the other, so naming `V92CP` here
 * adds no third spelling of anything.
 *
 * ---------------------------------------------------------------------------
 * FIVE STEPS, AND ONLY THE FOURTH IS CONDITIONAL.
 *
 *   1. Six spectral words copied whole, plus the gate byte turned into an
 *      exact 0 or 1 in `word_61c` (`cmpb $0x0,0x24(%edx) ; setne`).  The six
 *      are `setV92CPpckFromParamsInfo`'s six read the other way round, and the
 *      four `float` copies are `movl` at both ends, which is what GCC emits
 *      for a float assignment (finding F5820) rather than evidence that the
 *      author copied words.
 *   2. `distinctIndex[0..5]`, and it is a FOUR-BYTE COPY here where the V.90
 *      twin widens a byte: `mov 0x28(%edi,%edx,4),%esi ; mov
 *      %esi,0x638(%ecx,%edx,4)`.  A separate loop from 3 -- the object closes
 *      it at .text+0x33ccd and opens the next at +0x33ccf.
 *   3. The six constellations, each from `cp->short_42[cp->word_28[i]]`.
 *   4. The six codec constellations.  ONE gate selects the source and it is
 *      re-read from the SOURCE on every iteration: .text+0x33d71 reloads the
 *      CP pointer and +0x33d75 is `cmpb $0x0,0x24(%ecx)`, both INSIDE the loop
 *      the back edge at +0x33e82 closes.  So the `if` is inside the loop and
 *      not around it, and a caller that scribbled on `params->word_61c`
 *      between two calls would change nothing -- which is the V.90 twin's
 *      behaviour and the OPPOSITE of `V92setParamsInfoFromCPUnPck`, whose
 *      three gates read back the DESTINATION's copies.
 *      With the gate CLEAR the codec tables are NOT skipped: they are unpacked
 *      from the ORDINARY bitmaps at +0x42 (`lea 0x42(%ebx,%edi,1)` at
 *      .text+0x33e14) into the CODEC destination at +0x304 (`lea
 *      0x304(%edx,%esi,1)` at +0x33e3c).  Same source, other destination.
 *   5. `word_0`, `char_02` plus 0x14 or plus 8.
 *
 * THE SELECTOR IS `word_28` AND NOT `i`, and the two are the same only by
 * accident.  `setV92CPpckFromParamsInfo` fills `cp->word_28` with
 * `getConstellationsIndex`'s GROUP NUMBERS and writes group g's bitmap into
 * `short_42[g]`; so constellation i's bitmap is at `short_42[word_28[i]]`, and
 * that is what the object reads.  The address is built
 * `mov 0x28(%ecx,%ebp,4),%ebx ; shl $0x4,%ebx ; lea 0x42(%ebx,%ecx,1)` -- a
 * sixteen-byte stride, which is `short[8]`, the row length `V92CP.h` derives
 * from the packer's `add $0x10,%edi`.
 *
 * `constellationSize` ENDS UP DESCRIBING THE CODEC TABLE, for the same reason
 * it does in the V.90 twin: step 4's helper zeroes and refills the SAME length
 * word step 3 wrote (`mov %ecx,0x604(%ebx)` at .text+0x33d03 and again at
 * +0x33dae and +0x33e36).  When the two bitmaps differ in population the first
 * table keeps entries past the length that nothing will read.  With equal
 * populations that is invisible, which is why the fixture carries cases whose
 * two arrays differ in WEIGHT and asserts that at least one trial did.
 *
 * THE TWO BITMAP ARRAYS ABUT, WHERE THE V.90 TWIN'S DO NOT.  0xa2 - 0x42 =
 * 0x60 = 6 * 16 exactly, so `V92CP`'s two blocks are adjacent and
 * `V92CP.h` declares them so; the V.90 message has 0x9c - 0x3a = 0x62 and two
 * bytes between them that nothing reads (`pad_9a[2]`, finding F7570).
 *
 * THE BITMAP ORDERING IS THE V.90 TWIN'S, INSTRUCTION FOR INSTRUCTION.  Eight
 * `short` scanned from `mask[7]` down to `mask[0]` (`mov $0x7` then `decl ;
 * jns`), bit 0 first within each word (`test $0x1,%cl ; sar $1,%ecx`), and the
 * emitted byte counting DOWN from `j * 16 + 15` (`mov $0xf,%ebx ; dec %ebx ;
 * jns`, the sum formed `mov %edi,%eax ; add %bl,%al`).  So bit `b` of word `j`
 * becomes byte `j * 16 + (15 - b)` and the table comes out in strictly
 * DESCENDING byte order, 127 down to 0.  All three inlined sites are the same
 * body as the V.90 twin's three, so the two helpers below are reused unchanged.
 *
 * THAT THE THREE MASK BODIES ARE CALLS AND NOT OPEN-CODED LOOPS IS THE SAME
 * MEASUREMENT 7570 MADE, and it reproduces here exactly.  The three loops count
 * with an UNSIGNED variable -- `cmpl $0x5,0x4(%esp) ; jbe` at .text+0x33d5f and
 * `cmpl $0x5,(%esp) ; jbe` at +0x33e7e -- and each inlined body then clamps
 * that same variable with a SIGNED test, `cmp $0x6,%ebp ; setl` at
 * .text+0x33cea, +0x33d8f and +0x33e11.  One variable cannot be compared both
 * ways; a call whose argument is `(int)i` can, because the bound is the
 * caller's and the clamp is the callee's.  And the clamp is DEAD in this caller
 * too: `which` is the loop counter and the loop runs 0..5.
 *
 * THE TAIL IS `setDataBitRate` INLINED TOO, AND THAT IS MEASURED HERE RATHER
 * THAN CARRIED OVER FROM 7570.  The discriminator is WHERE THE RATE IS LOADED,
 * and it had to be re-measured because the V.90 twin's rate is a 32-bit `mov`
 * and this one is a `movsbl` of a `signed char`, which the compiler might well
 * hoist differently.  The object loads it ONCE, at .text+0x33e90, between the
 * `cmpb` and the `je`, because a function argument is evaluated before the
 * call.  Both spellings were compiled on the period compiler:
 *
 *	call form      cmpb $0x0,0x1(%ebp) ; movsbl 0x2(%ebp),%eax ; je
 *	               ... add $0x14,%eax ... / ... add $0x8,%eax ...
 *	               ONE load, and the whole tail is the object's
 *	               instruction for instruction, operands included
 *	open-coded if  cmpb $0x0,0x1(%ecx) ; je ; movsbl 0x2(%ecx),%eax ...
 *	               / movsbl 0x2(%ebp),%eax ...
 *	               TWO loads, one SUNK into each arm
 *
 * so the object cannot have been written as an `if` here.  The byte compare is
 * NOT the discriminator -- both spellings emit `cmpb $0x0` against memory, and
 * the `(int)` on the argument does not force a `movsbl`+`test`.  Nothing
 * observable rides on this: the two spellings agree on every input, which is
 * why it is settled from the object's instructions and from nowhere else.
 */
extern "C" void
setParamsInfoFromV92CPUnPck(V90MappingParams *params, V92CP *cp)
{
	unsigned int i;

	params->shaperSR = (int)cp->word_08;
	params->shaperId = cp->word_0c;
	params->shaperA1 = cp->flt_14;
	params->shaperA2 = cp->flt_18;
	params->shaperB1 = cp->flt_1c;
	params->shaperB2 = cp->flt_20;
	params->word_61c = (cp->byte_24 != 0);

	for (i = 0; i < V90_CONSTELLATIONS; i++)
		params->distinctIndex[i] = cp->word_28[i];

	for (i = 0; i < V90_CONSTELLATIONS; i++)
		setConstellationMaskInline(params, (int)i,
					   cp->short_42[cp->word_28[i]]);

	for (i = 0; i < V90_CONSTELLATIONS; i++) {
		if (cp->byte_24 != 0)
			setCodecConstellationMaskInline(params, (int)i,
			    cp->short_a2[cp->word_28[i]]);
		else
			setCodecConstellationMaskInline(params, (int)i,
			    cp->short_42[cp->word_28[i]]);
	}

	setDataBitRateInline(params, (int)cp->char_01, (int)cp->char_02);
}

/*
 * ===========================================================================
 * displaySpectralParams (.text+0x33ec0, 619 bytes)
 * ===========================================================================
 *
 * Print the spectral shaper's six fields.  Six `edprintf` calls and nothing
 * else -- no store, no return value, and `edprintf` is the only relocation in
 * the whole range -- so the transcript is the only observable thing it has,
 * and `test/unit/t_v90mpdisp.cpp` compares it against the blob's line for
 * line through the harness's debug-capture tier.
 *
 * ONE ARGUMENT, and it is a `V90MappingParams *`: the six fields are +0x620
 * to +0x634 read at the widths `V90ConstellationDesigner::spectralDesign`
 * writes them at, and the seven callers -- `V90Modulator::enterDataPhase`,
 * `::exitRi`, `::progress`, `V90Demodulator::exitPhase3`, `::progress`,
 * `V90Phase4Modulator::generateV90Symbol` and `::generateV92Symbol` -- all
 * hand it one.  The file is this one for the reason
 * `setV92CPpckFromParamsInfo`'s block above gives: the address is inside
 * `V90MappingParamsInt.cpp`'s bracket and there is no other evidence.
 *
 * IT IS NOT GATED.  There is no `cmpl $0x1,dsplibs_debug_level` anywhere in
 * the range; the level test is inside `edprintf` itself.  So at level 0 the
 * six calls all happen and print nothing, which is a different claim from a
 * gated site not being called, and the test says which it is checking.
 *
 * ---------------------------------------------------------------------------
 * THE FLOAT-AS-%c%d.%06d IDIOM, AND THE ONE THING THAT DIFFERS FROM
 * `src/pump/v90/V92ParamsInfo.c`'s COPY OF IT.
 *
 * The three helpers below are that file's, and the derivations there stand:
 * the subtraction inside `frac_of` is `v - (int)v` because the object is
 * `dc e1`, which objdump prints as `fsub %st,%st(1)` and which IS
 * `FSUBR st(1),st(0)` (finding F245); the `abs()` on the result makes the
 * order unobservable either way (finding F256); and `sign_of` selects on CF
 * alone, so zero prints as '-'.
 *
 * WHAT DIFFERS IS THE SCALE'S TYPE, and it is forced.  `V92ParamsInfo.c`'s
 * copy multiplies by a constant in `.rodata.cst4` -- four bytes, a `float`.
 * This one multiplies by `.rodata.cst8 + 0x30`, EIGHT bytes, and it holds
 * 1000000.0 exactly.  GCC never widens a `float` constant into a `double`
 * pool entry, so the source constant here is a `double` where the sibling's
 * is a `float`.  Nothing observable turns on it -- both are exactly 1e6 and
 * the multiply happens in the x87's extended registers either way -- and it
 * is written as the object holds it because that is what the object holds.
 * Finding F5822.
 *
 * `sign_of` TAKES A `long double` AND THAT IS MEASURED, NOT COPIED.  The
 * object's comparison is `fldz; fcompp` -- the zero materialised in a
 * register and both operands popped -- and every `float` spelling of it
 * compiled on the period compiler gives `fcomps` against a four-byte
 * constant in memory instead, whichever way round the operands are written.
 * `0.0L < v` with a `long double` parameter is what emits the object's form.
 * The rule: at a comparison whose shape will not reproduce, vary the TYPE
 * before the operand order, and compile the candidates rather than reasoning
 * about them.  Finding F5823 records all seven spellings and what each emitted,
 * and names the second site that reached the same conclusion.
 */

/*
 * The scale, and the `06` in the format strings is its number of digits.
 * Spelled `double` on the evidence above.
 */
#define SPECTRAL_FRAC_SCALE	1.0e6

static char
sign_of(long double v)
{
	return (0.0L < v) ? '+' : '-';
}

static int
whole_of(float v)
{
	return (int)fabsf(v);
}

static int
frac_of(float v)
{
	return __builtin_abs((int)(((long double)v - (long double)(int)v)
				   * SPECTRAL_FRAC_SCALE));
}

extern "C" void
displaySpectralParams(V90MappingParams *params)
{
	char s;
	int w, f;

	edprintf("V90MappingParams: Sr = %d\r\n", params->shaperSR);
	edprintf("V90MappingParams: Id = %d\r\n", params->shaperId);

	s = sign_of(params->shaperA1);
	w = whole_of(params->shaperA1);
	f = frac_of(params->shaperA1);
	edprintf("V90MappingParams: A1  = %c%d.%06d\r\n", s, w, f);

	s = sign_of(params->shaperA2);
	w = whole_of(params->shaperA2);
	f = frac_of(params->shaperA2);
	edprintf("V90MappingParams: A2  = %c%d.%06d\r\n", s, w, f);

	s = sign_of(params->shaperB1);
	w = whole_of(params->shaperB1);
	f = frac_of(params->shaperB1);
	edprintf("V90MappingParams: B1  = %c%d.%06d\r\n", s, w, f);

	s = sign_of(params->shaperB2);
	w = whole_of(params->shaperB2);
	f = frac_of(params->shaperB2);
	edprintf("V90MappingParams: B2  = %c%d.%06d\r\n", s, w, f);
}
