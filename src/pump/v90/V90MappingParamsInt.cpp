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
 * FOUR MORE SYMBOLS ARE IN THE SAME BRACKET AND ARE NOT WRITTEN:
 * `setConstellationMask` (0x33570), `setCodecConstellationMask` (0x335f0),
 * `setDataBitRate` (0x33690) and `setParamsInfoFromV92CPUnPck`
 * (0x33c60).  `getDataBitRate` used to be a fifth and `setParamsInfoFromCPUnPck`
 * a sixth; both are now written, below.
 * `setParamsInfoFromCPUnPck` and `setParamsInfoFromV92CPUnPck` are NOT
 * `src/pump/v90/V92ParamsInfo.c`'s -- that
 * file's `V92setParamsInfoFromCPUnPck` is a different symbol at .text+0x12f00
 * -- and the name similarity is exactly the trap `tools/tumap.py` exists to
 * avoid.
 *
 * THE FIRST TWO OF THOSE FOUR ARE THIS FILE'S OWN INLINED HELPER.  The two
 * `static` functions below are `setConstellationMask` and
 * `setCodecConstellationMask` -- the same body, with the register and
 * stack-slot allocation each inline site forces rather than the standalone
 * function's -- because `setParamsInfoFromCPUnPck` calls them three times
 * between them and the compiler inlines all three.  The INNER LOOP is
 * byte-identical to the object's at every one of the three sites, from the
 * `test $0x1,%cl` to the `jns` that closes the word loop; the setup around it
 * is the same operations in different registers, which is the allocator's.
 * They are `static` and not `extern "C"`
 * because writing the two GLOBALS is a separate 256 bytes of reconstruction
 * with its own differential test, which this batch did not take on; whoever
 * takes it can delete the `static` and the two calls become the object's own.
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
 * caller's and the clamp is the callee's.  That is finding 5821's measurement,
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
 * ===========================================================================
 * setParamsInfoFromCPUnPck (.text+0x336b0, 622 bytes)
 * ===========================================================================
 *
 * Fill a `V90MappingParams` from an unpacked V.90 CP message.  It is the
 * inverse of `setV92CPpckFromParamsInfo` below, over the V.90 message instead
 * of the V.92 one, and `setParamsInfoFromV92CPUnPck` (.text+0x33c60, still
 * unwritten) is the third corner: same destination, `V92CP` as the source.
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
 * WHY IT MATTERS ANYWAY.  Finding 7520 established that the V.90 DIGITAL side
 * has no reachable writer for the `V90MappingParams` block
 * `V90Modulator::progress` reads, bounded over the thirty symbols whose
 * MANGLING names a `V90MappingParams *`.  This function writes essentially
 * that whole block -- `word_0`, both 6 x 128 byte tables, `constellationSize`,
 * `word_61c`, the six shaper words and `distinctIndex` -- and it was outside
 * 7520's population by construction, because it is `extern "C"` and has no
 * mangled name to be found by.  So the writer exists; what it does not have
 * is a caller.  See finding 7570.
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

	if (cp->info->word_04 != 0)
		params->word_0 = cp->dataBitRate + 0x14;
	else
		params->word_0 = cp->dataBitRate + 8;
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
 * is the callee's.  Finding 5821.
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
	 * values.  It is not; finding 5820.
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
 * `FSUBR st(1),st(0)` (finding 245); the `abs()` on the result makes the
 * order unobservable either way (finding 256); and `sign_of` selects on CF
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
 * Finding 5822.
 *
 * `sign_of` TAKES A `long double` AND THAT IS MEASURED, NOT COPIED.  The
 * object's comparison is `fldz; fcompp` -- the zero materialised in a
 * register and both operands popped -- and every `float` spelling of it
 * compiled on the period compiler gives `fcomps` against a four-byte
 * constant in memory instead, whichever way round the operands are written.
 * `0.0L < v` with a `long double` parameter is what emits the object's form.
 * The rule: at a comparison whose shape will not reproduce, vary the TYPE
 * before the operand order, and compile the candidates rather than reasoning
 * about them.  Finding 5823 records all seven spellings and what each emitted,
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
	return (int)__builtin_fabsf(v);
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
