/*
 * V90Demapper.cpp -- eight of the fourteen members of the V.90 demapper: the
 * constructor, both destructors, the histogram diagnostic, and three of the
 * nine that do the work -- `hardDecision`, `process` and
 * `resetLinearMappStudy`.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Demapper.h` carries the
 * object map and the 0x1eb8 allocation the size comes from.
 *
 * THE SIX STILL OUTSTANDING are `reset`, `resetNoSpectral`,
 * `incrementRBSFramePosition`, `linearMappingStudy` and `updateConstelation`
 * -- about 2,400 bytes, declared in the header for the record.  One class,
 * one owner.
 *
 * WHAT THE CLASS IS FOR, now that its middle is read.  `hardDecision` takes
 * one PCM sample, finds the nearest level of the constellation belonging to
 * the current RBS frame position, and appends the level's CODE and the
 * sample's SIGN to two parallel arrays.  `process` then drains those arrays
 * six samples at a time: the codes through `ModulusDecoder::progress` and the
 * signs through either `V90SignBitsExtractor` or the serial differential
 * decoder at +0x664, both writing into one bit buffer the caller supplies.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215):
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
 * translation unit may include both definitions; finding 1112.
 */
#include "dsplib/V90Parameters.h"
/*
 * `hardDecision` reads the detector's per-phase flag at +0x2800 and
 * `resetLinearMappStudy` calls `clearCamulativeVal` over all 6 x 128 cells of
 * it, so the pointer at +0x1ea0 has to be a complete type here.
 */
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/ModulusCoder.h"
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

/*
 * The allocation, and finding 1107's whole point: this number comes from
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
 * THE ORDER OF THE STORES IS NOT THE OBJECT'S ORDER, and only one part of it
 * was forced.  `adiDetector` is stored BEFORE the two allocations and the
 * compiler could not have moved it there: a store to `*this` cannot cross a
 * call to `sysdep_malloc`, which may alias anything.  The eight zeroed words
 * and the two trailing stores are plain stores with no call between them, so
 * their order in the object is the scheduler's and is not evidence.
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

	bitsPerFrame = 0;
	word_08 = 0;
	signBitsPerFrame = 0;
	signBitGroups = 0;
	signBitGroupSize = 0;
	rbsFramePosition = 0;
	frameStart = 0;
	sampleCount = 0;

	for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++)
		constellationSize[i] = 0;

	errorHistogramCount = 0;
	this->params = params;
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
 * THE SIX STORES ARE NOT IN THE OBJECT'S ORDER AND THAT IS NOT EVIDENCE.
 * They are six plain stores with no call between them, so the scheduler was
 * free to interleave them with the argument reload it needed anyway (finding
 * 617); what IS in the object is their widths -- 16-bit at +0x1e9c, +0x1ea4,
 * +0x1ea6 and +0x1eae, 32-bit at +0x1ea8 and +0x1eb0 -- and the header
 * declares each accordingly.
 */
void
V90Demapper::resetLinearMappStudy(unsigned int n)
{
	short phase, code;

	for (phase = 0; phase < V90DEMAPPER_CONSTELLATIONS; phase++)
		for (code = 0; code < V90DEMAPPER_LEVELS; code++)
			adiDetector->clearCamulativeVal(phase, code);

	short_1e9c = 0;
	short_1ea4 = 0;
	short_1ea6 = 0;
	uint_1ea8 = n;
	decisionFramePosition = 0;
	uint_1eb0 = 0;
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
 * `__builtin_abs` AND NOT `x < 0 ? -x : x`, three times.  Finding 2117
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
