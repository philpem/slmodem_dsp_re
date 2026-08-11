/*
 * V90Demapper.cpp -- two of the fourteen members of the V.90 demapper: the
 * destructor and the diagnostic it exists to reach.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Demapper.h` carries the
 * object map, the 0x1eb8 allocation the size comes from, and why the
 * constructor is declared there and not defined here.
 *
 * THE OTHER TWELVE ARE THE CLASS'S PROCESSING HALF -- `process`,
 * `hardDecision`, `linearMappingStudy`, `updateConstelation`, the three
 * resets -- 4,400-odd bytes, declared in the header for the record.  One
 * class, one owner.
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
DEM_OFF(word_04,		0x0004, word04);
DEM_OFF(word_18,		0x0018, word18);
DEM_OFF(array_1c,		0x001c, array1c);
DEM_OFF(array_20,		0x0020, array20);
DEM_OFF(count_24,		0x0024, count24);
DEM_OFF(word_28,		0x0028, word28);
DEM_OFF(word_2c,		0x002c, word2c);
DEM_OFF(constellation,		0x0030, constel);
DEM_OFF(constellationSize,	0x0630, constelsz);
DEM_OFF(modulusDecoder,		0x0648, moddec);
DEM_OFF(byte_664,		0x0664, byte664);
DEM_OFF(signBits,		0x0668, signbits);
DEM_OFF(errorSum,		0x0690, errsum);
DEM_OFF(errorCount,		0x1290, errcount);
DEM_OFF(errorHistogramCount,	0x1e90, errhist);
DEM_OFF(adiDetector,		0x1ea0, adi);

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
 * throughout; `count_24` then receives `%ebx` itself.  The element TYPES are
 * still unknown -- see the header -- so the two stay `void *`.
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
	: byte_664(0)
{
	unsigned int i;

	adiDetector = adi;
	array_1c = sysdep_malloc(levels * 4);
	array_20 = sysdep_malloc(levels);
	count_24 = levels;

	word_04 = 0;
	word_08 = 0;
	word_0c = 0;
	word_10 = 0;
	word_14 = 0;
	word_18 = 0;
	word_28 = 0;
	word_2c = 0;

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
	if (array_1c)
		sysdep_free(array_1c);
	if (array_20)
		sysdep_free(array_20);
}
