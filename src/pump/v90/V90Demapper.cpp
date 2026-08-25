/*
 * V90Demapper.cpp -- all thirteen members of the V.90 demapper: the
 * constructor, both destructors, the histogram diagnostic, and the eight that
 * do the work -- `hardDecision`, `process`, `resetLinearMappStudy`,
 * `incrementRBSFramePosition`, `updateConstelation`, `resetNoSpectral`,
 * `linearMappingStudy` and `reset`.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Demapper.h` carries the
 * object map and the 0x1eb8 allocation the size comes from.
 *
 * `reset` (0x30870, 723 bytes) IS NOW WRITTEN, and what unblocked it was
 * `V90SignBitsExtractor::reset` -- the one callee nothing in this tree had, so
 * a batch carrying it would not have been CLOSED and would have failed every
 * differential binary at `t_encode` rather than only its own (finding F215).
 *
 * WHAT THE CLASS IS FOR, now that its middle is read.  `hardDecision` takes
 * one PCM sample, finds the nearest level of the constellation belonging to
 * the current RBS frame position, and appends the level's CODE and the
 * sample's SIGN to two parallel arrays.  `process` then drains those arrays
 * six samples at a time: the codes through `ModulusDecoder::progress` and the
 * signs through either `V90SignBitsExtractor` or the serial differential
 * decoder at +0x664, both writing into one bit buffer the caller supplies.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215):
 * `mov 0x10(%esp),%ebx` after one push and an eight-byte frame in the
 * destructor, `mov 0x30(%esp),%ebx` after four saves and 0x2c in
 * `printErrorHistogramAndReset`.
 *
 * ---------------------------------------------------------------------------
 * WHICH TIER DECIDES WHICH HALF OF `printErrorHistogramAndReset`
 *
 * The name says PRINT, and the differential tier is structurally blind to
 * printing -- the debug level ships at zero (docs/method/tiers.md, tier 1).
 * It is worth being exact about what that costs here, because the answer is
 * "less than it looks":
 *
 *   - THE RESET HALF IS TIER 1, AND IT IS MOST OF THE FUNCTION.  Both
 *     3,072-byte histogram arrays are zeroed in full, and +0x1e90 is
 *     incremented, and both of those are ordinary memory the arena
 *     comparison sees.  So the loop bounds, the two bases, the 128-row
 *     zeroing width, the six-way guard and the position of the increment
 *     relative to that guard are all decided by a byte comparison.
 *   - THE PRINT HALF IS TIER 4, AND IT IS NOT SKIPPED.  Five `edprintf`
 *     calls, four format strings, three arguments on the busiest one.
 *     `t_v90leaves.cpp` already sweeps `dsplibs_debug_level` 0..2 across both
 *     sides and compares the captured text, and `edprintf` ENCODES its
 *     output (src/core/encode.c), so a transcript that matches is a format
 *     string, an argument list and a character count that all match.
 *   - WHAT NEITHER SEES is the one thing worth writing down: with the debug
 *     level at zero the encoder still runs and still moves its shared key, so
 *     the number of calls is not observable through anything this tree
 *     compares.  Nothing in the reset half depends on it.
 *
 * THE GATE IS NOT `dsplibs_debug_level` AND NOT `DSPLIB_DEBUG_ON()`.  The five
 * `edprintf` sites at 0x30beb, 0x30c0a, 0x30c26, 0x30c7a and 0x30cc4 are
 * unconditional; the level gate lives inside `edprintf` itself.  The two real
 * gates are `params->DEBUG_DEMAPPER_ERROR_HISTOGRAM` in the destructor and the
 * all-six-non-zero test at the top of this function -- and the second one
 * suppresses the RESET as well as the print, which is the part a reader of the
 * name would not predict.
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/sysdep.h"
/*
 * The NAMED 0x558 `V90Parameters` map, not `V90PreFilter.h`'s 0x504 word
 * block.  The destructor branches on one slot of it and that slot has the
 * original author's own name; a numeric index would throw that away.  No
 * translation unit may include both definitions; finding F1112.
 */
#include "dsplib/V90Parameters.h"
/*
 * `hardDecision` reads the detector's per-phase flag at +0x2800 and
 * `resetLinearMappStudy` calls `clearCamulativeVal` over all 6 x 128 cells of
 * it, so the pointer at +0x1ea0 has to be a complete type here.
 */
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/ModulusCoder.h"
/*
 * `resetNoSpectral` takes one of these and reads three of its members, so the
 * argument has to be a complete type here.  It is a DIFFERENT class from the
 * `V90Parameters` above and the two are not interchangeable; finding F1112 is
 * about the two `V90Parameters` maps and does not reach this one.
 */
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Demapper.h"

/*
 * Hold the compiler to the map in the header, as `V90ConnectionEvaluator.cpp`
 * does.  Skipped on the 64-bit `check64` pass, where a 32-bit layout is not
 * what the compiler lays out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define DEM_OFF(field, off, tag) \
	typedef char v90dem_off_##tag[ \
	    ((int)__builtin_offsetof(V90Demapper, field) == (off)) ? 1 : -1]

DEM_OFF(params,			0x0000, params);
DEM_OFF(bitsPerFrame,		0x0004, bitsframe);
DEM_OFF(word_08,		0x0008, word08);
DEM_OFF(signBitsPerFrame,	0x000c, sbframe);
DEM_OFF(signBitGroups,		0x0010, sbgroups);
DEM_OFF(signBitGroupSize,	0x0014, sbgroupsz);
DEM_OFF(rbsFramePosition,	0x0018, rbspos);
DEM_OFF(codes,			0x001c, codes);
DEM_OFF(signs,			0x0020, signs);
DEM_OFF(sampleCapacity,		0x0024, samplecap);
DEM_OFF(frameStart,		0x0028, framestart);
DEM_OFF(sampleCount,		0x002c, samplecnt);
DEM_OFF(constellation,		0x0030, constel);
DEM_OFF(constellationSize,	0x0630, constelsz);
DEM_OFF(modulusDecoder,		0x0648, moddec);
DEM_OFF(signDecoder,		0x0664, signdec);
DEM_OFF(signBits,		0x0668, signbits);
DEM_OFF(errorSum,		0x0690, errsum);
DEM_OFF(errorCount,		0x1290, errcount);
DEM_OFF(errorHistogramCount,	0x1e90, errhist);
DEM_OFF(histogramDelay,		0x1e94, histdelay);
DEM_OFF(histogramIntegration,	0x1e98, histint);
DEM_OFF(short_1e9c,		0x1e9c, short1e9c);
DEM_OFF(adiDetector,		0x1ea0, adi);
DEM_OFF(short_1ea4,		0x1ea4, short1ea4);
DEM_OFF(short_1ea6,		0x1ea6, short1ea6);
DEM_OFF(uint_1ea8,		0x1ea8, uint1ea8);
DEM_OFF(decisionCode,		0x1eac, deccode);
DEM_OFF(decisionFramePosition,	0x1eae, decpos);
DEM_OFF(uint_1eb0,		0x1eb0, uint1eb0);
DEM_OFF(linearMappStudyEnabled,	0x1eb4, lmstudyenabled);

/*
 * The allocation, and finding F1107's whole point: this number comes from
 * `movl $0x1eb8,(%esp); call sysdep_malloc` at 0x1c4d9 rather than from the
 * highest displacement any V90Demapper symbol uses, which would have said
 * 0x1e94.
 */
typedef char v90dem_size[(sizeof(V90Demapper) == 0x1eb8) ? 1 : -1];
#endif

/*
 * `V90Demapper::V90Demapper` -- 193 bytes at 0x30640 (C1) and again at
 * 0x30710 (C2).
 *
 * `this` is the first STACK argument, so after `push esi; push ebx; sub $4`
 * the frame is this 0x10, levels 0x14, params 0x18, adi 0x1c -- and every
 * store below was read off that.
 *
 * THE ELEMENT COUNT IS THE FIRST ARGUMENT AND THE TWO WIDTHS ARE FOUR AND
 * ONE.  `lea 0x0(,%ebx,4),%eax` before the first `sysdep_malloc` and a bare
 * `mov %ebx,(%esp)` before the second, with `%ebx` the first argument
 * throughout; `sampleCapacity` then receives `%ebx` itself.  The element
 * TYPES used to be unknown and are not any more -- `process` hands both
 * blocks to callees whose manglings type them -- so the casts below are what
 * `sysdep_malloc` returning `void *` costs and nothing more.
 *
 * THE ORDER OF THE STORES IS THE OBJECT'S ORDER, and the paragraph that used
 * to stand here said the opposite.  It read: "the eight zeroed words and the
 * two trailing stores are plain stores with no call between them, so their
 * order in the object is the scheduler's and is not evidence."  **That was
 * never measured and it is false**, which is 6100's defect and 7779's -- a
 * comment asserting a question is closed when nobody had opened it.
 *
 * `adiDetector` is stored BEFORE the two allocations and the compiler could
 * not have moved it there: a store to `*this` cannot cross a call to
 * `sysdep_malloc`, which may alias anything.  That much was right.
 *
 * The eight zeroed words are 7766's case, and the licence is available here
 * because it was checked in this function first: written low-to-high, GCC
 * 3.4.2 emits them low-to-high, so on this run the map from source order to
 * emitted order is the IDENTITY and the object's emission is therefore its
 * own preimage.  The object emits them HIGH TO LOW -- +0x2c, +0x28, +0x18,
 * +0x14, +0x10, +0x0c, +0x08, +0x04 -- and writing that order is what the
 * source now does.  8! is far too large to enumerate and no enumeration is
 * claimed; what is claimed is 7766's inversion of a measured identity map,
 * and 617's acceptance test decides it: differing bytes of 193 went
 *
 *     ascending, errorHistogramCount then params (as written)   13
 *     DESCENDING, errorHistogramCount then params                5
 *     DESCENDING, params then errorHistogramCount                1
 *
 * and the two trailing stores were enumerated properly, over all eight
 * positions they can take relative to the constellation loop: the six that
 * move either of them ABOVE the loop change the function's SIZE and are
 * excluded outright, and of the two that do not, `params` first is the one
 * that reaches 1.
 *
 * ONE BYTE IS LEFT AND IT IS THE FREE COLUMN, named rather than shrugged at
 * (2900).  The epilogue discards the `sub $0x4` slot with a `pop` into a dead
 * register: the blob picks `%eax` and we pick `%ebx`, one byte of modrm, both
 * values dead.  No source text chooses that, `byteident.py` grades both
 * constructors **grade 1 ACCEPT** where they were REJECT before, and the
 * register difference that USED to sit at 0xaf -- `mov 0x18(%esp),%ecx`
 * against `%edx` -- disappeared when the store order was fixed.  That is
 * 7779's rule confirmed in a second function: a register difference
 * downstream of a store-order difference is not independent evidence.
 *
 * THE SIX COUNTS ARE A ROLLED LOOP, not six stores.  `mov %ebx,0x630(%esi,
 * %eax,4); inc %eax; cmp $0x5,%eax; jbe` -- one store and a back edge, where
 * the eight words above really are eight separate `movl $0x0`.
 */
V90Demapper::V90Demapper(unsigned int levels, V90Parameters *params,
			 V90AutoDigitalImpDetector *adi)
	: signDecoder()
{
	unsigned int i;

	adiDetector = adi;
	codes = (unsigned int *)sysdep_malloc(levels * 4);
	signs = (unsigned char *)sysdep_malloc(levels);
	sampleCapacity = levels;

	sampleCount = 0;
	frameStart = 0;
	rbsFramePosition = 0;
	signBitGroupSize = 0;
	signBitGroups = 0;
	signBitsPerFrame = 0;
	word_08 = 0;
	bitsPerFrame = 0;

	for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++)
		constellationSize[i] = 0;

	this->params = params;
	errorHistogramCount = 0;
}

/*
 * `printErrorHistogramAndReset` -- 362 bytes at 0x30b80.
 *
 * THE SIX-WAY GUARD IS A CHAIN OF `&&` AND NOT A LOOP.  Six loads into six
 * different registers, each with its own `test`/`je` to the same join point:
 * %eax at 0x30b97, %edx at 0x30ba5, %ecx at 0x30bb3, %esi at 0x30bc1,
 * %edi at 0x30bcf, %ebp at 0x30bdd.  A rolled test would be one load, one
 * compare and a back edge.
 *
 * THE INCREMENT AT THE END IS OUTSIDE THE GUARD.  0x30cd0 is where the
 * guard's six `je`s land AND where the printing path falls through, so the
 * counter moves on every call.  A reconstruction that put it inside would
 * be byte-identical on the printing path and silently wrong on the other,
 * which is what the "one size zero" trial in `t_v90leaves.cpp` exists for.
 */
void
V90Demapper::printErrorHistogramAndReset()
{
	if (constellationSize[0] && constellationSize[1] &&
	    constellationSize[2] && constellationSize[3] &&
	    constellationSize[4] && constellationSize[5]) {
		unsigned int i, j;

		edprintf("======================================================"
			 "=====\r\n");
		edprintf("Error histogram #%d:\r\n", errorHistogramCount);
		for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++) {
			edprintf("constellation %d :\r\n", i);
			/*
			 * The row's own length, so a constellation that has
			 * fewer than 128 levels prints fewer than 128 lines.
			 * `cmp %ebp,0x630(%ebx,%eax,4)` with `ja` -- an
			 * UNSIGNED bound, which is why `count` is unsigned.
			 */
			for (j = 0; j < constellationSize[i]; j++) {
				unsigned int mean = 0;

				/*
				 * `div %ecx` and not `idiv`, guarded by a
				 * `test`/`je` on the divisor: an average of
				 * zero is printed for a level nothing landed
				 * on, rather than the machine trapping.
				 */
				if (errorCount[i][j] != 0)
					mean = errorSum[i][j] / errorCount[i][j];
				edprintf("  %d\t-\t%d\t\t%d\r\n",
					 constellation[i][j], errorCount[i][j],
					 mean);
			}
			/*
			 * AND THE RESET IS THE FULL 128, not `constellationSize
			 * [i]`.  `cmp $0x7f,%edx; jbe` at 0x30c9f -- a
			 * constant, with the row length nowhere in the loop.
			 * The two bounds differ whenever a constellation is
			 * short, so this is a real distinction and not a
			 * restatement.
			 */
			for (j = 0; j < V90DEMAPPER_LEVELS; j++) {
				errorSum[i][j] = 0;
				errorCount[i][j] = 0;
			}
		}
		edprintf("======================================================"
			 "=====\r\n");
	}
	errorHistogramCount++;
}

/*
 * `~V90Demapper` -- 123 bytes at 0x30f50, and the same 123 bytes again at
 * 0x30fd0 as D2.
 *
 * The three tests are all explicit in the object:
 *
 *     30f58:  8b 13                 mov  (%ebx),%edx
 *     30f5a:  8b 82 2c 05 00 00     mov  0x52c(%edx),%eax
 *     30f60:  85 c0                 test %eax,%eax
 *     30f62:  75 21                 jne  30f85           -> the histogram
 *     30f64:  8b 43 1c              mov  0x1c(%ebx),%eax
 *     30f67:  85 c0                 test %eax,%eax
 *     30f69:  75 35                 jne  30fa0           -> sysdep_free
 *     30f6b:  8b 43 20              mov  0x20(%ebx),%eax
 *     30f6e:  85 c0                 test %eax,%eax
 *     30f70:  75 3e                 jne  30fb0           -> sysdep_free
 *     30f72:  8d 8b 68 06 00 00     lea  0x668(%ebx),%ecx
 *     30f7b:  e8 ..                 call V90SignBitsExtractor::~...
 *
 * THE NULL TESTS ARE NOT DECORATION AND THEY ARE NOT FREE TO OMIT.  This
 * tree's `sysdep_free` tolerates NULL, so dropping the two `if`s leaves every
 * byte comparison unchanged; what changes is `harness_alloc.free_null`, which
 * is why `t_v90leaves.cpp` asserts that counter is untouched on the
 * all-NULL trial.
 *
 * THE FOURTH CALL IS THE COMPILER'S.  `signBits` is the only member with a
 * non-trivial destructor, so GCC emits its destruction after the body and
 * this file must not write it.  The embedded `ModulusDecoder` at +0x648 gets
 * no such call in the object, which is the evidence that ITS destructor is
 * trivial -- and it stayed true when the member stopped being a pad and
 * became the real class, because `ModulusCoder.h` declares no destructor for
 * it either.  If it ever gains one this destructor grows a call it must not
 * have, and nothing but this comment says so.
 */
V90Demapper::~V90Demapper()
{
	if (params->DEBUG_DEMAPPER_ERROR_HISTOGRAM)
		printErrorHistogramAndReset();
	if (codes)
		sysdep_free(codes);
	if (signs)
		sysdep_free(signs);
}

/*
 * ===========================================================================
 * THE PROCESSING HALF -- three of the nine, 1,207 bytes of the object.
 * ===========================================================================
 */

/*
 * `resetLinearMappStudy` -- 139 bytes at 0x307e0.
 *
 * THE TWO COUNTERS ARE `short` AND SIGNED, and that is forced rather than
 * chosen.  Both are incremented through `lea 0x1(%r),%eax; movswl %ax,%r` --
 * a 16-bit truncation on every step, which a 32-bit counter would not have --
 * and both loop tests are `cmp $imm,%r16` followed by `jle`, the SIGNED
 * branch.  An `unsigned short` would give `jbe`, and an `int` neither the
 * truncation nor the 16-bit compare.  The width also matches the callee:
 * `clearCamulativeVal(short, short)` says so in its mangling.
 *
 * THE SIX STORES ARE IN THE OBJECT'S ORDER, AND THIS PARAGRAPH USED TO SAY
 * THE OPPOSITE.  It read "not in the object's order and that is not
 * evidence -- six plain stores with no call between them, so the scheduler
 * was free to interleave them".  That was reasoning about the compiler rather
 * than measuring it, and lever 1 is explicit that nothing before the compile
 * separates a constant map from a bijection.
 *
 * ENUMERATED, ALL 6! = 720 ORDERINGS (finding F7820).  The map is a perfect
 * BIJECTION -- 720 orderings, 720 DISTINCT emissions, so the scheduler
 * reorders nothing here at all -- and exactly ONE cell reaches positional byte
 * identity.  The preimage is unique, so this is a decoded ORDER and not a
 * decoded fact, and 7782's ruling takes the bytes.
 *
 * The recovered order is the object's own emission order read straight off the
 * disassembly, +0x1eb0, +0x1eae, +0x1ea4, +0x1ea6, +0x1e9c, +0x1ea8.  What had
 * been in this file was ASCENDING FIELD ORDER, which is the transcriber's
 * tidying and not the author's -- the trap lever 1 names, where our source
 * order looks like an answer and is only ever a restatement of the header.
 *
 * The widths are unchanged and were never in doubt: 16-bit at +0x1e9c, +0x1ea4,
 * +0x1ea6 and +0x1eae, 32-bit at +0x1ea8 and +0x1eb0, and the header declares
 * each accordingly.
 */
void
V90Demapper::resetLinearMappStudy(unsigned int n)
{
	short phase, code;

	for (phase = 0; phase < V90DEMAPPER_CONSTELLATIONS; phase++)
		for (code = 0; code < V90DEMAPPER_LEVELS; code++)
			adiDetector->clearCamulativeVal(phase, code);

	uint_1eb0 = 0;
	decisionFramePosition = 0;
	short_1ea4 = 0;
	short_1ea6 = 0;
	short_1e9c = 0;
	uint_1ea8 = n;
}

/*
 * `hardDecision` -- 660 bytes at 0x31050, and the largest of the three.
 *
 * ONE PCM SAMPLE IN, THE NEAREST CONSTELLATION LEVEL OUT, and one entry
 * appended to `codes` and `signs` on the way.  The return is the level with
 * the input's sign put back on it.
 *
 * THE RETURN TYPE IS `short` AND THE OBJECT SAYS SO.  It ends
 * `imul %ebx,%esi; mov %esi,0x20(%esp); movswl 0x20(%esp),%eax` -- the
 * product is formed 32 bits wide and then TRUNCATED to 16 and sign-extended
 * back.  An `int` return would stop at the `imul`.
 *
 * FOUR THINGS ARE READ FROM THE OBJECT RATHER THAN CACHED, and the reloads
 * are what says so.  `rbsFramePosition` is re-read at 0x3109a after the byte
 * store into `signs` and again at 0x312d8 after
 * `printErrorHistogramAndReset`; `sampleCount` at 0x31173 and 0x31254; the
 * `signs` and `codes` pointers on every use.  A local copy would have
 * survived all of those, because nothing in the source could alias `this`
 * -- but a store through an `unsigned char *` and a call to another member
 * both can, as far as the compiler knows, so the member accesses are written
 * out and not hoisted into locals.
 *
 * `sign` IS ASSIGNED IN BOTH ARMS OF THE SIGN TEST AND NOWHERE ELSE, so the
 * over-capacity arm below reaches the multiply with it unset.  THE OBJECT
 * DOES EXACTLY THAT -- it loads 0x20(%esp) at 0x31229 having never written it
 * on that path -- and it is unobservable, because `level` is zero there and
 * anything times zero is zero.  This reconstruction initialises it instead,
 * which is a deviation of one instruction and no behaviour: docs/deviations.md
 * D385.
 *
 * THE SEARCH IS A LINEAR SCAN DOWN A DESCENDING CONSTELLATION, unswitched by
 * the compiler into two copies that differ only in their bound -- `size - 1`
 * at 0x310d0 and `2 * size - 1` at 0x311d4.  The doubling is the automatic
 * digital-impairment detector's flag at its +0x2800: with that set the row
 * holds two levels per code and the scan runs over twice as many of them,
 * which is also why the code that goes into `codes` is halved.
 *
 * AND THE SCAN HAS THE OBJECT'S OWN HAZARD, kept rather than fixed: `n - 1`
 * with `n` zero is 0xffffffff, so a constellation whose count is still zero
 * and whose first level compares `>=` sends the scan off the end of the row.
 * The object computes the same `size - 1` and takes the same branch
 * (`cmp $0x0,%ecx; jbe` -- unsigned, so only zero exits).  Nothing calls this
 * before `updateConstelation` has filled the counts.
 *
 * `__builtin_abs` AND NOT `x < 0 ? -x : x`, three times.  Finding F2117
 * measured both spellings on the period compiler over a composed translation
 * unit: GCC 3.4.2 renders the conditional as a conditional NEGATION and only
 * the builtin gives the `cltd; xor; sub` triple the object has at 0x3109d,
 * 0x3110f and 0x3111d.
 *
 * THE ABSOLUTE VALUE IS TAKEN TWICE ON THE NEGATIVE ARM and once on the
 * other, which is in the object and is not an accident of scheduling: the
 * negative arm's `cltd; xor; sub` at 0x312af leaves `%esi` already
 * non-negative and the shared code at 0x3109d does it again.  So the sign
 * test's own arm assigns `in`, and `mag` is a second, separate abs.
 */
short
V90Demapper::hardDecision(short in)
{
	unsigned int code = 0;
	int sign = 1;
	short level = 0;

	decisionFramePosition = (short)rbsFramePosition;

	if (sampleCount >= sampleCapacity) {
		/*
		 * THE ONLY DIAGNOSTIC IN THE FUNCTION, and the sample is
		 * dropped: nothing is appended, no position is advanced, and
		 * the return is zero because `level` never leaves its
		 * initialiser.
		 */
		edprintf("V90Demapper: Hard decision input buffers are full "
			 "!!!!!!!!!!!!!\n");
	} else {
		unsigned int n;
		short mag, err, alt;

		if (in < 0) {
			signs[sampleCount] = 0;
			sign = -1;
			in = (short)__builtin_abs(in);
		} else {
			signs[sampleCount] = 1;
			sign = 1;
		}
		mag = (short)__builtin_abs(in);

		n = constellationSize[rbsFramePosition];
		if (adiDetector->short_2800[rbsFramePosition] != 0)
			n = 2 * n;

		while (constellation[rbsFramePosition][code] >= mag &&
		       code < n - 1)
			code++;
		if (code)
			code--;

		/*
		 * The two neighbours the scan left `code` between, compared on
		 * 16-bit distance -- `cmp 0x24(%esp),%dx` with `jge`, so the
		 * lower index wins a tie.
		 */
		err = (short)__builtin_abs(constellation[rbsFramePosition]
					   [code] - mag);
		alt = (short)__builtin_abs(constellation[rbsFramePosition]
					   [code + 1] - mag);
		if (alt < err) {
			level = constellation[rbsFramePosition][code + 1];
			err = alt;
			code++;
		} else {
			level = constellation[rbsFramePosition][code];
		}

		/*
		 * THE HISTOGRAM, and its two counters run in sequence rather
		 * than together: the delay is spent first, one decision at a
		 * time, and only then does the integration begin.  The
		 * comparison against the parameter is SIGNED (`jge` at
		 * 0x3128b), which is what makes both fields `int`.
		 */
		if (params->DEBUG_DEMAPPER_ERROR_HISTOGRAM) {
			if (histogramDelay != 0) {
				histogramDelay--;
			} else {
				errorSum[rbsFramePosition][code] += err;
				errorCount[rbsFramePosition][code]++;
				if (histogramIntegration >= params->
				    DEMAPPER_ERROR_HISTOGRAM_INTEGRATION_TIME) {
					printErrorHistogramAndReset();
					histogramIntegration = 0;
				} else {
					histogramIntegration++;
				}
			}
		}

		/*
		 * THE STORED CODE IS HALVED WHEN THE DETECTOR'S FLAG IS SET,
		 * and the object's shift is LOGICAL with a 16-bit truncation
		 * after it -- `shr $1,%eax; cwtl` at 0x3125a.  An arithmetic
		 * shift would say the value was signed and there would be no
		 * `cwtl` if the result went straight into the word.
		 */
		if (adiDetector->short_2800[rbsFramePosition] != 0)
			codes[sampleCount] = (short)(code >> 1);
		else
			codes[sampleCount] = code;
		sampleCount++;

		/*
		 * `mul $0xaaaaaaab; shr $0x2` and a multiply back -- the
		 * UNSIGNED division by six, which is what makes
		 * `rbsFramePosition` unsigned.
		 */
		rbsFramePosition = (rbsFramePosition + 1) %
				   V90DEMAPPER_CONSTELLATIONS;
	}

	decisionCode = (short)code;
	return (short)(sign * level);
}

/*
 * `process` -- 408 bytes at 0x31720.  Drain whole six-sample frames into the
 * caller's bit buffer, then slide what is left down to the front.
 *
 * SIX SAMPLES IS THE FRAME AND THE CONSTANT IS IN THE OBJECT TWICE -- `lea
 * 0x6(%edx),%eax` for the entry test and `lea 0x6(%edi),%edx` with
 * `add $0xc,%edi` for the advance-and-retest.  It is the same six as
 * `V90DEMAPPER_CONSTELLATIONS`, the RBS frame, and the sign-bit extractor's
 * capacity; they are one number and the macro says which.
 *
 * FEWER THAN SIX SAMPLES IS NOT AN EMPTY DRAIN, IT IS A DIFFERENT RETURN.
 * `cmp $0x5,%ebx; jbe` short-circuits to `*nbits = 0; return 0` without
 * touching `frameStart` or compacting anything -- so a caller that keeps
 * feeding gets its partial frame back untouched, and the compaction below
 * only ever runs on the path that also returns 1.
 *
 * THE TWO SIGN-BIT PATHS ARE EXCLUSIVE AND `signBitGroups` IS THE SWITCH.
 * Non-zero cuts the frame into that many groups through `signBits`, each
 * consuming `signBitGroupSize` samples and yielding one bit fewer; zero sends
 * every sign bit through the serial decoder at +0x664 instead.  Both write
 * the same `signBitsPerFrame` bytes at the front of the frame's output, and
 * `ModulusDecoder::progress` writes after them -- which is what
 * `signBitsPerFrame` IS.
 *
 * THE COMPACTION USES A LOCAL SOURCE CURSOR, not `frameStart`.  The object
 * keeps it in `%ecx` and writes `frameStart` exactly once, at the end and
 * with zero (`movl $0x0,0x28(%esi)`); a version that walked the member would
 * have to store it every iteration.
 */
int
V90Demapper::process(unsigned char *out, unsigned int &nbits)
{
	unsigned int total = 0;
	unsigned int i, j;

	if (sampleCount <= V90DEMAPPER_FRAME - 1) {
		nbits = 0;
		return 0;
	}

	while (frameStart + V90DEMAPPER_FRAME <= sampleCount) {
		modulusDecoder.progress(out + total + signBitsPerFrame,
					&codes[frameStart]);

		if (signBitGroups != 0) {
			unsigned int g;

			for (g = 0; g < signBitGroups; g++)
				signBits.process(&signs[frameStart +
							signBitGroupSize * g],
						 out + total +
						 (signBitGroupSize - 1) * g);
		} else {
			unsigned int b;

			for (b = 0; b < signBitsPerFrame; b++)
				out[total + b] = signDecoder.process(
				    signs[frameStart + b]);
		}

		total += bitsPerFrame;
		frameStart += V90DEMAPPER_FRAME;
	}

	for (i = 0, j = frameStart; j < sampleCount; i++, j++) {
		codes[i] = codes[j];
		signs[i] = signs[j];
	}
	sampleCount = i;
	frameStart = 0;

	nbits = total;
	return 1;
}

/*
 * `incrementRBSFramePosition` -- 33 bytes at 0x30b50, and the whole function
 * is the one statement below.
 *
 * THE DIVISION IS UNSIGNED AND THAT IS THE ONLY THING TO READ HERE.
 * `mov $0xaaaaaaab,%edx; mul %edx; shr $0x2,%edx` is the reciprocal form of
 * an UNSIGNED divide by six, `lea (%edx,%edx,2),%edx; add %edx,%edx`
 * multiplies the quotient back by six and `sub %edx,%ebx` takes the
 * remainder.  A signed `% 6` needs either an `idiv` or a sign-correction on
 * the quotient and the object has neither, which is the same evidence
 * `hardDecision`'s own copy of this advance carries at 0x312bc.
 *
 * That the member exists at all is worth a line: `hardDecision` does not call
 * it, it repeats it.  So the class advances the position two ways and only
 * this one is reachable from outside.
 */
void
V90Demapper::incrementRBSFramePosition()
{
	rbsFramePosition = (rbsFramePosition + 1) % V90DEMAPPER_CONSTELLATIONS;
}

/*
 * `updateConstelation` -- 286 bytes at 0x312f0, and the author's spelling of
 * the name, with one `l`, is the blob's and is kept.
 *
 * WHAT IT IS: the automatic digital-impairment detector has been accumulating
 * a magnitude sum and a sample count per (phase, code) cell -- `float_1000`
 * and `uint_1c00`, which `linearMappingStudy` below is what fills.  This turns
 * each cell's MEAN into the demapper's own constellation table, rounded to
 * nearest, and leaves a cell nothing landed on alone.
 *
 * BOTH LOOP COUNTERS ARE `unsigned short` AND BOTH BOUNDS ARE 16-BIT.
 * `cmp $0x5,%di; jbe` for the outer and `cmp %di,%bx; jb` for the inner, with
 * `movzwl %bx,%edi` / `movzwl %cx,%ebx` truncating each increment back to 16
 * bits.  An `unsigned int` counter has neither the truncation nor the 16-bit
 * compare, and a `short` one would compare with `jle`.
 *
 * THE ROW LENGTH DOUBLES WITH THE DETECTOR'S PER-PHASE FLAG, the same rule
 * `hardDecision` scans under: with `short_2800[i]` set the row holds two
 * levels per code.  The zero arm loads `constellationSize[i]` with `movzwl`
 * off a 32-bit field (0x313ec) and the non-zero arm loads it 32-bit, doubles
 * it and truncates (0x31336 .. 0x3133f) -- so BOTH arms discard the upper
 * half, which is what makes the narrow load finding F614's free case rather
 * than evidence about the field.  `constellationSize` stays `unsigned int`;
 * `hardDecision` and `printErrorHistogramAndReset` both depend on that.
 *
 * `linearMappingStudy` READS THE SAME QUANTITY AS A `short` AND THAT ONE IS
 * FORCED -- see its comment below.  The two functions genuinely differ in the
 * type of the local, and finding F4341 is why the difference is not a defect in
 * either.
 *
 * THE CELL INDEX IS `i * 128 + j` AND j IS NOT BOUNDED BY 128.  The object
 * forms `(i << 7) + j` once (`shl $0x7,%ebp`, `lea 0x0(%ebp,%ebx,1),%ecx`)
 * and scales it by 4 for the detector's two arrays and by 2 for ours, so with
 * the flag set and a count above 64 the row runs into the next one.  Writing
 * it `[i][j]` gives the identical arithmetic and keeps the object's overrun
 * rather than hiding it behind a bound the object does not have.
 *
 * THE RECIPROCAL IS IN THE SOURCE, not an optimisation of `sum / count`.
 * The object computes `1.0 / count` with `fdivr %st(2),%st` and then
 * `fmuls` -- three x87 operations where a plain divide is two, and the extra
 * one only exists because the constant 1.0 is there to divide.  `d8 fa` is a
 * D8 register form, which finding F245's swap does not touch, and `dis.py`
 * prints no Intel note against it.
 *
 * BOTH CONSTANTS ARE HOISTED OUT OF BOTH LOOPS.  `fld1` and
 * `flds .rodata.cst4+0x1b8` (which holds exactly 0.5) are in the prologue at
 * 0x312f1 and 0x312fd, and each outer iteration pushes a working copy of the
 * pair with two `fld %st(1)` and drops them with two `fstp %st(0)`.
 *
 * THE `+ 0.5f` IS A ROUNDING TERM AND `fistps` IS WHY.  The store runs under a
 * control word OR'd with 0xc00 -- round toward zero -- so the half added
 * before it is what makes the result round to nearest.  The `(short)` cast is
 * what emits the whole `fnstcw`/`or`/`fldcw`/`fistps`/`fldcw` sequence.
 *
 * THE TAIL IS `dsplibs_debug_printf` AND NOT `edprintf`, which is the one
 * place this file's habit is the wrong reflex: the object ends
 * `jmp dsplibs_debug_printf` -- a tail call, no encoding, one argument -- under
 * an explicit `cmpl $0x1,dsplibs_debug_level; ja`, which is `DSPLIB_DEBUG_ON()`
 * written out.  Every `edprintf` site in this file is unconditional because
 * the level gate lives inside `edprintf` instead.
 */
void
V90Demapper::updateConstelation()
{
	unsigned short i, j;

	for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++) {
		unsigned short n;

		if (adiDetector->short_2800[i] != 0)
			n = 2 * constellationSize[i];
		else
			n = constellationSize[i];

		for (j = 0; j < n; j++)
			if (adiDetector->uint_1c00[i][j] != 0)
				constellation[i][j] = (short)
				    (1.0F / adiDetector->uint_1c00[i][j] *
				     adiDetector->float_1000[i][j] + 0.5F);
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demapper: constelation update !!!\n");
}

/*
 * `resetNoSpectral` -- 605 bytes at 0x30cf0.  Rebuild the whole demapper from
 * a `V90MappingParams` and the detector's two measured mapping tables, with
 * no spectral shaping involved; `reset` is the other half of the pair and is
 * not written here.
 *
 * WHAT THE ARGUMENT SUPPLIES: the frame's bit count at its +0x00, the six row
 * lengths at its +0x604, and the six 128-byte tables of CODES at its +0x004.
 * `V90MappingParams.h` names all three from the three unmangled functions that
 * read them, and this is a second reader agreeing with that map.
 *
 * A CODE IS AN `unsigned char` AND IS NOT BOUNDED BY 128.  The object loads it
 * with `movzbl 0x4(%ecx,%ebp,1)` and adds it to `i * 128` before scaling by
 * two, so a byte of 128 or more indexes past its own row of `linMapp` and into
 * the next one.  That is the object's arithmetic and is reproduced rather than
 * clamped; `t_v90demap.cpp` counts the trials that reach it.
 *
 * THE DOUBLED ARM LAYS THE PAIR DOWN LARGER FIRST, and the comparison is
 * SIGNED and 16-BIT: `cmp %cx,%bx` with `jle` at 0x30dab, the two values
 * loaded `movzwl` from `linMapp` and `linMappAlt` and never widened.  On the
 * `jle` arm -- so on EQUAL as well as smaller -- `linMappAlt` is written
 * first, which is what a `>` and not a `>=` puts there.
 *
 * `k` IS A `short` AND IT LIVES ONLY IN THE DOUBLED ARM.  Its two increments
 * per iteration are truncated to 16 bits every time (`inc %edx; movswl %dx,%edx`
 * at 0x30dc0 and `lea 0x1(%edx),%eax; cwtl` at 0x30dc9), while the plain arm
 * indexes with `j` itself -- `inc %ecx` with no truncation and a 32-bit
 * unsigned `ja`.  Hoisting one counter out of the `if` would give the plain
 * arm truncations the object does not have, and no differential test could
 * see the difference, so this one is settled by the encoding alone.
 *
 * THE SEVEN WORDS AT +0x648 ARE THE EMBEDDED `ModulusDecoder`, and they are
 * written as seven field assignments rather than as the seven-argument
 * constructor the mangling advertises.  That constructor is DECLARED AND NOT
 * DEFINED in this tree -- the blob has it out of line at 0x320b0 and 0x32070
 * and nobody has reconstructed it -- so spelling it here would add a symbol
 * outside this batch's closure.  The object inlines whatever the original
 * wrote: seven plain `mov`s in the scheduler's order, 0, 3, 4, 1, 2, 5, 6,
 * which is not a source order and is not chased (finding F617).
 *
 * `histogramIntegration` BEFORE `histogramDelay` is the object's order and
 * costs nothing to adopt; both are plain stores after the call, so it is a
 * hint and not evidence.
 */
void
V90Demapper::resetNoSpectral(V90MappingParams *mapp)
{
	unsigned int i, j;

	bitsPerFrame = mapp->word_0;
	word_08 = bitsPerFrame - signBitsPerFrame;

	if (params->DEBUG_DEMAPPER_ERROR_HISTOGRAM) {
		printErrorHistogramAndReset();
		histogramIntegration = 0;
		histogramDelay = params->DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM;
	}

	for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++) {
		constellationSize[i] = mapp->constellationSize[i];

		if (adiDetector->short_2800[i] != 0) {
			short k = 0;

			for (j = 0; j < constellationSize[i]; j++) {
				unsigned char c = mapp->constellation[i][j];

				if (adiDetector->linMapp[i][c] >
				    adiDetector->linMappAlt[i][c]) {
					constellation[i][k] =
					    adiDetector->linMapp[i][c];
					k++;
					constellation[i][k] =
					    adiDetector->linMappAlt[i][c];
					k++;
				} else {
					constellation[i][k] =
					    adiDetector->linMappAlt[i][c];
					k++;
					constellation[i][k] =
					    adiDetector->linMapp[i][c];
					k++;
				}
			}
		} else {
			for (j = 0; j < constellationSize[i]; j++)
				constellation[i][j] = adiDetector->linMapp[i]
				    [mapp->constellation[i][j]];
		}
	}

	modulusDecoder.field_00 = constellationSize[0];
	modulusDecoder.field_04 = constellationSize[1];
	modulusDecoder.field_08 = constellationSize[2];
	modulusDecoder.field_0c = constellationSize[3];
	modulusDecoder.field_10 = constellationSize[4];
	modulusDecoder.field_14 = constellationSize[5];
	modulusDecoder.field_18 = word_08;
	signDecoder.prev_ = 0;
}

/*
 * `reset` -- 723 bytes at 0x30870, and `resetNoSpectral`'s other half: the
 * same rebuild of the six constellations, with the SIGN-BIT geometry taken
 * from the mapping block, the sign-bit extractor re-armed, the histogram
 * emptied and the detector's cumulative cells cleared.
 *
 * WHAT `resetNoSpectral` DOES NOT DO, in the order the object does it:
 *
 *   - the four sign-bit words at +0x0c .. +0x14 from `mapp->shaperSR`;
 *   - `V90SignBitsExtractor::reset(shaperSR, 0)` on the embedded extractor;
 *   - the cursor, the frame start and the RBS position back to zero;
 *   - both 3,072-byte histogram arrays cleared IN FULL, 6 x 128 and not
 *     `constellationSize[i]` (`cmp $0x7f,%edx; jbe` at 0x30a8f -- a constant);
 *   - the histogram delay reseeded, and `clearCamulativeVal` over all 768
 *     cells of the detector;
 *   - the linear-mapping study's six words zeroed.
 *
 * And what it does NOT have that `resetNoSpectral` does: there is no
 * `DEBUG_DEMAPPER_ERROR_HISTOGRAM` arm and no call to
 * `printErrorHistogramAndReset`.  The histogram is emptied here rather than
 * printed.
 *
 * THE SIGN-BIT GEOMETRY IS ALL ONE FIELD.  `mapp->shaperSR` (+0x620) becomes
 * `signBitGroups` unchanged, `6 - it` becomes `signBitsPerFrame` and `6 / it`
 * becomes `signBitGroupSize`; `groups * groupSize == 6` and `groups *
 * (groupSize - 1) == 6 - groups` are the two identities the header derives
 * those names from, and both hold.  The same word is the extractor's
 * `spacing`.
 *
 * THE DIVIDE IS UNSIGNED AND IT IS GUARDED, and both halves are forced.
 * `f7 74 24 20  divl 0x20(%esp)` at 0x308b6 is `div` and not `idiv`, which is
 * what makes `V90DEMAPPER_FRAME`'s `6u` the right spelling of the numerator
 * -- `6 / (int)` would be a signed division.  And `test %esi,%esi; je` at
 * 0x308a7 skips it, so a zero `shaperSR` LEAVES `signBitGroupSize` UNWRITTEN:
 * that is a real early-out and not a fold, the field keeps whatever it held,
 * and `t_v90demap.cpp` plants a recognisable value in it to see that happen.
 * The callee guards its own divide the same way, so a zero spacing is a
 * runnable input on both sides and not a #DE.
 *
 * THE LOOP BOUND IS A COPY OF THE MAPPING BLOCK'S LENGTH AND NOT THE MEMBER
 * JUST WRITTEN FROM IT.  The object loads `mapp->constellationSize[i]` into
 * `%eax`, spills it to `0x18(%esp)`, stores it to `constellationSize[i]`, and
 * every loop compare reads the SPILL.  The two readings are the same number
 * except when the doubled arm's overrun reaches `constellationSize` itself --
 * which it can, for the sixth row, exactly as the header documents -- so the
 * local below is what the object encodes and re-reading the member would not
 * be.  `resetNoSpectral` is spelled the other way and its object does not
 * decide between them.
 *
 * EVERYTHING ELSE IN THE TWO CONSTELLATION LOOPS IS `resetNoSpectral`'S, down
 * to the `short k` that lives only in the doubled arm and is truncated to 16
 * bits on every step (`inc %edx; movswl %dx,%edx` at 0x3091d, `lea 0x1(%edx),
 * %eax; cwtl` at 0x30929) while the plain arm indexes with `j` itself.  The
 * larger of the two mapping tables is laid down first and the comparison is
 * SIGNED and 16-BIT -- `cmp %cx,%bx` with `jg` at 0x30977, so EQUAL puts
 * `linMappAlt` first, which is what a `>` and not a `>=` gives.
 *
 * THE HISTOGRAM DELAY IS BOUNDED BY A LENGTH FROM THE OTHER END OF THE
 * PARAMETER BLOCK, and this is what the object says rather than something
 * that reads naturally:
 *
 *     30aa4:  8b 83 30 05 00 00  mov  0x530(%ebx),%eax   ; DEMAPPER_DELAY_...
 *     30aaa:  3b 83 6c 03 00 00  cmp  0x36c(%ebx),%eax   ; TRN2D_DD_LENGTH
 *     30ab0:  7c 02              jl   30ab4
 *     30ab2:  31 c0              xor  %eax,%eax
 *     30ab4:  89 85 94 1e 00 00  mov  %eax,0x1e94(%ebp)
 *
 * so a delay that is not SHORTER than `TRN2D_DD_LENGTH` is taken as zero,
 * which starts the histogram immediately.  `jl` is the signed branch and both
 * parameters are `int`.  No rationale is offered here for why those two
 * quantities are compared; the instructions are.
 *
 * `decisionCode` (+0x1eac) IS NOT WRITTEN, and its neighbour +0x1eae is.  The
 * store block at 0x30b04..0x30b34 covers +0x1eb0, +0x1eae, +0x1ea4, +0x1ea6,
 * +0x1e9c, +0x1ea8 and +0x1eb4 and skips the one between the first two;
 * `resetLinearMappStudy` leaves it alone in the same way.  Their order is the
 * scheduler's -- seven plain stores with no call between them, finding F617 --
 * and what is in the object is their WIDTHS, which the header declares.
 */
void
V90Demapper::reset(V90MappingParams *mapp)
{
	unsigned int i, j;
	short phase, code;

	bitsPerFrame = mapp->word_0;
	signBitGroups = mapp->shaperSR;
	signBitsPerFrame = V90DEMAPPER_FRAME - mapp->shaperSR;
	word_08 = bitsPerFrame - signBitsPerFrame;

	if (mapp->shaperSR != 0)
		signBitGroupSize = V90DEMAPPER_FRAME / mapp->shaperSR;

	for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++) {
		unsigned int n = mapp->constellationSize[i];

		constellationSize[i] = n;

		if (adiDetector->short_2800[i] != 0) {
			short k = 0;

			for (j = 0; j < n; j++) {
				unsigned char c = mapp->constellation[i][j];

				if (adiDetector->linMapp[i][c] >
				    adiDetector->linMappAlt[i][c]) {
					constellation[i][k] =
					    adiDetector->linMapp[i][c];
					k++;
					constellation[i][k] =
					    adiDetector->linMappAlt[i][c];
					k++;
				} else {
					constellation[i][k] =
					    adiDetector->linMappAlt[i][c];
					k++;
					constellation[i][k] =
					    adiDetector->linMapp[i][c];
					k++;
				}
			}
		} else {
			for (j = 0; j < n; j++)
				constellation[i][j] = adiDetector->linMapp[i]
				    [mapp->constellation[i][j]];
		}
	}

	modulusDecoder.field_00 = constellationSize[0];
	modulusDecoder.field_04 = constellationSize[1];
	modulusDecoder.field_08 = constellationSize[2];
	modulusDecoder.field_0c = constellationSize[3];
	modulusDecoder.field_10 = constellationSize[4];
	modulusDecoder.field_14 = constellationSize[5];
	modulusDecoder.field_18 = word_08;
	signDecoder.prev_ = 0;

	signBits.reset(mapp->shaperSR, 0);

	sampleCount = 0;
	frameStart = 0;
	rbsFramePosition = 0;

	for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++)
		for (j = 0; j < V90DEMAPPER_LEVELS; j++) {
			errorSum[i][j] = 0;
			errorCount[i][j] = 0;
		}

	if (params->DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM <
	    params->TRN2D_DD_LENGTH)
		histogramDelay = params->DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM;
	else
		histogramDelay = 0;
	histogramIntegration = 0;

	for (phase = 0; phase < V90DEMAPPER_CONSTELLATIONS; phase++)
		for (code = 0; code < V90DEMAPPER_LEVELS; code++)
			adiDetector->clearCamulativeVal(phase, code);

	uint_1eb0 = 0;
	decisionFramePosition = 0;
	short_1ea4 = 0;
	short_1ea6 = 0;
	short_1e9c = 0;
	uint_1ea8 = 0;
	linearMappStudyEnabled = 0;
}

/*
 * `linearMappingStudy` -- 779 bytes at 0x31410, and the largest member of the
 * class after `hardDecision`.  One decided sample in, one cell of the
 * detector's running mean out, and every `uint_1ea8` calls a whole pass that
 * turns those means into the constellation and starts again.
 *
 * ITS CALLER IS `V90Equalizer::process`, which calls it at 0x3a3dd -- so the
 * two arguments are the equaliser's, and the pair it hands over is a SAMPLE
 * and the LEVEL that was decided for it.  The names below are that reading
 * and are the weakest of CLAUDE.md's three ranks; what is NOT inference is
 * that the first argument is the one whose magnitude is accumulated into
 * `float_1000`, which `V90AutoDigitalImpDetector.h` already documents as a sum
 * of magnitudes with `uint_1c00` as its count.
 *
 * WHICH NEIGHBOURING PAIR IS MEASURED AGAINST, and the two tests that choose
 * it are the whole first half:
 *
 *   `|sample| - |level| > 0` -- STRICTLY greater: `fcoms` against
 *   `.rodata.cst4+0x1bc`, which holds exactly 0.0f, with `jbe` to the other
 *   arm.  A sample bigger than its level sits ABOVE the level, and the row
 *   descends, so the pair to bracket it with is the one an index LOWER.
 *
 *   otherwise the pair an index HIGHER, unless there is no such pair:
 *   `decisionCode + 1 >= n` falls back to the lower one, with `n` the row
 *   length doubled by the detector's per-phase flag exactly as `hardDecision`
 *   doubles its scan.
 *
 * `n` IS A `short` HERE AND AN `unsigned short` IN `updateConstelation`, and
 * both are forced by different encodings: 0x3152a loads `constellationSize[p]`
 * with `movswl` and 0x31573 truncates the doubled value with `movswl %ax,%ecx`,
 * and in both cases the 32-bit result feeds the SIGNED `jge` at 0x31545 -- the
 * forced column of CLAUDE.md's rule.  `updateConstelation`'s `movzwl` feeds a
 * 16-bit `jb` and discards its upper half, which is the free column.  Finding
 * F4341.
 *
 * AND THE PAIR IS READ WITHOUT A LOWER BOUND.  With `decisionCode` zero and
 * the row length one, `decisionCode + 1 >= n` holds and the object reads
 * `constellation[p][-1]` -- two bytes before the row, which for phase 0 is the
 * tail of `sampleCount`.  The object computes exactly that displacement
 * (`0x2e(%ebp,%eax,2)`) and the reconstruction keeps it.
 *
 * THE GATE IS `|diff| < 0.4f * gap` AND THE MULTIPLIER IS SINGLE PRECISION.
 * `fmuls .rodata.cst4+0x1c0` = 0.4f; a `double` 0.4 would be `fmull` against
 * `.rodata.cst8`.  The comparison is `fcompp` with the product in ST(0) and
 * `jbe` to skip, so the product must be strictly greater -- and `de d9` is
 * FCOMPP, which has no reversed twin and is not finding F245's trap.  Nothing
 * in the differential tier can separate `0.4f` from `0.4`: the two straddle no
 * integer for any gap a `short` row can hold, so that half is a codegen claim.
 *
 * THE ABSOLUTE VALUE OF THE SAMPLE IS TAKEN TWICE, once into `diff` at 0x31434
 * and again at 0x314bd, and both are the `cltd; xor; sub` triple finding F2117
 * pins to `__builtin_abs` rather than to `x < 0 ? -x : x`.
 *
 * THE END-OF-RUN PASS INLINES `updateConstelation` -- the same 0.5f rounding
 * loop, the same doubled bound and the same "constelation update" line, at
 * 0x315bd..0x316f9 -- and then clears all 6 x 128 cells through
 * `clearCamulativeVal`, exactly as `resetLinearMappStudy` does.  So the means
 * are read into the constellation BEFORE they are wiped, and the order of
 * those two is observable.
 *
 * `short_1e9c` IS NOT CLEARED HERE.  It counts completed runs and is only ever
 * zeroed by `resetLinearMappStudy`, so the `== 2` test fires exactly once per
 * study and `short_1ea6` is "a second run has finished" where `short_1ea4`,
 * set unconditionally below, is "a run has finished".
 */
void
V90Demapper::linearMappingStudy(short sample, short level)
{
	float diff = __builtin_abs(sample) - __builtin_abs(level);
	short high, low;

	if (diff > 0.0F) {
		if (decisionCode - 1 >= 0) {
			high = constellation[decisionFramePosition]
					    [decisionCode - 1];
			low = constellation[decisionFramePosition]
					   [decisionCode];
		} else {
			high = constellation[decisionFramePosition]
					    [decisionCode];
			low = constellation[decisionFramePosition]
					   [decisionCode + 1];
		}
	} else {
		short n;

		if (adiDetector->short_2800[decisionFramePosition] != 0)
			n = 2 * constellationSize[decisionFramePosition];
		else
			n = constellationSize[decisionFramePosition];

		if (decisionCode + 1 >= n) {
			high = constellation[decisionFramePosition]
					    [decisionCode - 1];
			low = constellation[decisionFramePosition]
					   [decisionCode];
		} else {
			high = constellation[decisionFramePosition]
					    [decisionCode];
			low = constellation[decisionFramePosition]
					   [decisionCode + 1];
		}
	}

	if (__builtin_fabsf(diff) < 0.4F * (high - low)) {
		adiDetector->float_1000[decisionFramePosition][decisionCode] +=
		    __builtin_abs(sample);
		adiDetector->uint_1c00[decisionFramePosition][decisionCode]++;
	}

	if (uint_1eb0 + 1 == uint_1ea8) {
		short phase, code;

		short_1e9c++;
		if (short_1e9c == 2)
			short_1ea6 = 1;

		uint_1eb0 = 0;
		short_1ea4 = 1;

		updateConstelation();

		for (phase = 0; phase < V90DEMAPPER_CONSTELLATIONS; phase++)
			for (code = 0; code < V90DEMAPPER_LEVELS; code++)
				adiDetector->clearCamulativeVal(phase, code);
	} else {
		uint_1eb0++;
	}
}
