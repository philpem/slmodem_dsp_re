/*
 * V90SdDetector.cpp -- clearing the SD detector.
 *
 * Reconstructed from dsplibs.o.  One of the class's four members: `reset()`,
 * which is the one `v34handshak` reaches.
 * `include/dsplib/V90SdDetector.h` carries the object map and the evidence.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THE LOOP BOUND IS UNSIGNED and the guard is separate from it.  The object
 * tests `cmp $0x0,%ecx` / `jbe` before loading the buffer pointer at all, and
 * closes the loop with `cmp %eax,%ecx` / `ja` -- both unsigned, which is what
 * `historyLength` being `unsigned int` compiles to.  A signed bound would
 * have produced `jle` and `jg`.  The pointer load sitting INSIDE the guard is
 * ordinary code motion and not a null check: nothing here tests the pointer.
 *
 * THE COUNTER IS CLEARED LAST.  `movl $0x0,(%ebx)` is after the loop, not
 * before it.  The order is visible rather than assumed because a store
 * through `history` could alias `count` as far as the compiler knows, so it
 * could not have moved the store across the loop even if the source had put
 * it first.
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90SdDetector.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SD_OFF(field, off, tag) \
	typedef char v90sd_off_##tag[ \
	    ((int)__builtin_offsetof(V90SdDetector, field) == (off)) ? 1 : -1]

V90SD_OFF(count,         0x00, count);
V90SD_OFF(limit,         0x04, limit);
V90SD_OFF(thresh_08,     0x08, thresh08);
V90SD_OFF(thresh_0c,     0x0c, thresh0c);
V90SD_OFF(value_10,      0x10, value10);
V90SD_OFF(history,       0x14, history);
V90SD_OFF(historyLength, 0x18, historylength);
typedef char v90sd_size[(sizeof(V90SdDetector) == 0x1c) ? 1 : -1];
#endif

/*
 * THE FOUR ARGUMENTS GO TO FOUR WORDS AND NOTHING IS COMPUTED FROM THEM.
 * Each is a 32-bit `mov` from the incoming stack slot into the object -- no
 * `flds`/`fstps` pair anywhere -- so the original COPIED the three floats
 * rather than converting them.
 *
 * OUR BUILD DOES NOT, AND THAT IS A TOOLCHAIN DIFFERENCE, NOT A DEFECT HERE.
 * The modern compiler renders the same assignment as `flds`/`fstps` under
 * `-mfpmath=387`, which is bit-exact for every float value including
 * denormals and quiet NaNs and quietens a SIGNALLING NaN.  Finding 1242
 * measures it and bounds it; the alternative is a bit-copy spelling chosen to
 * make the instruction match, which is fitting the compiler and is what the
 * codegen rule in CLAUDE.md forbids.
 *
 * THE LENGTH IS THE CONSTANT 12 AND SO IS THE ALLOCATION.  `movl $0xc` into
 * `historyLength` and `movl $0x30` as the allocator's argument: no argument
 * reaches either, so a fourth argument of 11 or 13 changes neither.  The
 * clearing loop then re-reads `historyLength` from the object, which is what
 * the reload after the call is, and is why it is written that way here.
 */
V90SdDetector::V90SdDetector(float thresh08, float thresh0c, float value10,
			     unsigned int limitArg)
{
	unsigned int i;

	thresh_08 = thresh08;
	thresh_0c = thresh0c;
	value_10 = value10;
	limit = limitArg;

	historyLength = 12;
	history = (float *)sysdep_malloc(historyLength * sizeof(float));

	for (i = 0; i < historyLength; i++)
		history[i] = 0.0f;

	count = 0;
}

/*
 * THE POINTER IS TESTED AND NOT NULLED.  `test %eax,%eax` / `jne` around the
 * one call, and nothing is written back, so a destroyed object still holds
 * the address it freed -- which is why the test checks the destructor through
 * the allocator rather than through the object.
 */
V90SdDetector::~V90SdDetector()
{
	if (history != 0)
		sysdep_free(history);
}

void
V90SdDetector::reset()
{
	unsigned int i;

	for (i = 0; i < historyLength; i++)
		history[i] = 0.0f;

	count = 0;
}

/*
 * One sample in, one of three verdicts out.  The header names the fields;
 * what this function adds is what the numbers ARE and where the decisions sit.
 *
 * THE SHIFT IS A DO-WHILE AND IS WRITTEN AS ONE.  `historyLength - 1` is
 * computed, then the body runs and decrements: a length of 1 does not skip the
 * loop, it runs it 2^32 times through history[-1].  The object is written that
 * way and the constructor's 12 means nothing ever reaches it; a `for` would be
 * a different function on an input the object cannot receive, so the object's
 * shape is kept and the test drives the length the constructor sets.
 *
 * SIX LAGS AND SIX PRODUCTS, not `historyLength` of them.  The correlation
 * loop's bound is the immediate 5 (`cmp $0x5,%eax; jbe`), so it runs for
 * i = 0..5 whatever the length field says, reading history[i] and
 * history[i + 6] -- which is what makes twelve the right allocation and not a
 * coincidence.  Both sums stay in x87 registers for all six terms and are
 * never rounded to float; only the two threshold loads are float-wide.
 *
 * THE QUOTIENT IS NaN WHENEVER THE ENERGY IS ZERO, and that is why the
 * middle comparison is spelt `!(a >= b)` and not `a < b` (finding 1401).
 * A silent history divides zero by zero; `fcom` then reports UNORDERED, which
 * sets CF as well as ZF, and the object's `jae` is not taken -- so the
 * unordered case goes down the COUNTING arm.  `thresh_0c < ratio` in C is
 * false on a NaN and would go down the other one.  The other two comparisons
 * need no such care and are written the obvious way: `ja` on
 * thresh_08 : energy is not taken when unordered, which is what `>` does,
 * and `jbe` on value_10 : ratio IS taken when unordered, which is what the
 * `else` of `>` does.  All three senses are the object's, measured against it
 * over a history that really does go silent.
 *
 * FIVE EXITS, THREE RESULTS, AND ONE PATH THAT LEAVES THE COUNTER ALONE:
 *
 *     energy below thresh_08          count = 0,   returns 0
 *     quotient above thresh_0c        count += 1,  returns 0 while
 *                                     count < limit and 1 once it is not
 *     quotient below value_10 too     count = 0,   returns 0
 *     quotient between the two        count UNTOUCHED, returns -1
 *
 * The limit compare is `jb` -- UNSIGNED -- and it is made on the incremented
 * value before it is stored, so a limit of 0 latches 1 on the first sample.
 */
int
V90SdDetector::process(float sample)
{
	/*
	 * `correlation` IS DECLARED FIRST, and that is not a tidiness -- it is
	 * what puts the two accumulators in the object's x87 stack slots.  GCC
	 * 3.4's reg-stack pass follows the declaration order here, and with
	 * `energy` first the whole floating-point skeleton comes out shifted:
	 * `fxch %st(3)` where the object has `fxch %st(2)`, `faddp %st,%st(2)`
	 * where it has `faddp %st,%st(1)`, and -- the part that is not
	 * cosmetic -- the middle compare with its operands the other way round
	 * and `jbe` for the object's `jae`.  Swapping these two lines makes
	 * every x87 instruction and every branch in the function the object's.
	 * Finding 2301.
	 */
	unsigned int i = historyLength - 1;
	long double correlation = 0.0L;
	long double energy = 0.0L;
	long double ratio;
	unsigned int run;

	do {
		history[i] = history[i - 1];
	} while (--i != 0);

	history[0] = sample;

	for (i = 0; i <= 5; i++) {
		long double h = history[i];

		energy = energy + h * h;
		correlation = correlation + h * history[i + 6];
	}

	if (thresh_08 > energy) {
		count = 0;
		return 0;
	}

	ratio = correlation / energy;

	/*
	 * `flds 0xc(%edi); fcomp %st(1); fnstsw; sahf; jae` -- THRESH_0C IS
	 * THE LEFT OPERAND and the jump over this arm is `jae`, so the arm is
	 * entered on CF, which FCOM sets for unordered as well as for less: a
	 * NaN quotient counts.  Under -mno-ieee-fp that is what the plain `<`
	 * emits, given the declaration order above.
	 *
	 * THE SPELLING IS NOT THE LEVER HERE.  `thresh_0c < ratio` emits the
	 * identical instruction under -mno-ieee-fp -- the operand order came
	 * from the stack slots, which came from the declarations above.  The
	 * negated form is kept because it is ALSO right for the modern build,
	 * where the compiler will not drop the parity test and `<` is false on
	 * a NaN: one text, both tiers.  Finding 2301, and 2300 for the sites
	 * where the spelling IS the lever and no single text serves both.
	 */
	if (!(thresh_0c >= ratio)) {
		run = count + 1;
		count = run;
		return run < limit ? 0 : 1;
	}

	if (value_10 > ratio)
		return -1;

	count = 0;
	return 0;
}
