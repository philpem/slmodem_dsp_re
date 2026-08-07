/*
 * V90Equalizer.cpp -- the two step-size setters and the Phase 3 entry.
 *
 * Three of the class's twenty-seven members: `setLinearEquBeta(float)`,
 * `setDfeBeta(float)` and `enterPhase3()`.  They are one unit because
 * `enterPhase3` calls the other two, and they are a closed batch because
 * between them they reach nothing outside the class except `edprintf`.
 * `include/dsplib/V90Equalizer.h` carries the object map.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x40(%esp),%ebx` with 0x40 the frame's argument slot --
 * not %ecx, so nothing here needs an attribute (finding 215).
 *
 *
 * A 350-BYTE FLOAT SETTER IS NOT STORING A FLOAT
 *
 * Each setter is three things, and the store is the smallest of them:
 *
 *   1. a diagnostic, printing the new value in a fixed-point decimal the
 *      object builds by hand out of three printf arguments, taken only when
 *      the value has changed;
 *   2. the store itself;
 *   3. when the equaliser is in its fixed-point mode, a renormalisation that
 *      turns the float step size into an integer and a shift.
 *
 * The two functions are the same 350 bytes with different fields, different
 * scale constants and a different format string.  They are written out twice
 * here rather than shared, because that is how the object has them.
 *
 * (3) is the interesting half.  It is
 *
 *      shift = (int)( log10(fabs(refLevel / (beta * 2**k))) / log10(2.0f) )
 *      scaledBeta = (int)(beta * betaScale * (float)(1 << shift))
 *
 * with k = 24 for the linear equaliser and k = 20 for the DFE, and it is the
 * same arithmetic `convertEqualizerToMmx` runs over a magnitude it has just
 * measured -- so the two agree on the format the coefficients are held in.
 *
 *
 * THE X87 IS THE SPECIFICATION, AND GCC 13 WILL NOT EMIT IT
 *
 * The object computes both logarithms on the coprocessor:
 *
 *      d9 ec       fldlg2                  ; log10(2), 64-bit mantissa
 *      d9 c9       fxch   %st(1)
 *      d9 f1       fyl2x                   ; -> log10(2) * log2(x)
 *
 * GCC gates that expansion on -funsafe-math-optimizations, which this tree
 * does not build with, so a literal `log10()` here compiles to a call into
 * libm returning a double.  That is not a style difference: the quotient is
 * truncated to an int, so one ulp between libm's answer and the
 * coprocessor's is one step in `shift` and a factor of two in `scaledBeta` --
 * and it is worst exactly where the input is a power of two, which is where
 * a renormalisation lands most often.  So the three instructions are
 * transcribed.  Finding 233's conclusion, one class further on: where the
 * object uses the coprocessor, transcribing the coprocessor is both
 * necessary and sufficient.
 *
 * Everything else stays ordinary C++.  The intermediates are `long double`
 * because the object keeps them in x87 registers and never rounds them to
 * 32 bits: it stores the argument to a 4-byte stack slot only to survive the
 * `edprintf` call, and reloads the same bits.  Finding 256 measured that a
 * `float` spelling would have agreed here anyway under -mfpmath=387; the
 * explicit `long double` does not depend on that measurement holding.
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here.
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/encode.h"

#include "dsplib/V90Equalizer.h"

/*
 * Hold the compiler to the map in the header.  tools/offcheck.py only parses
 * `struct name {` out of include/dsplib, so a C++ class has to assert its own
 * -- and this is exactly the check that catches an object right in size and
 * wrong by four in every offset, which is what a missed vptr produces
 * (finding 228).  Skipped on the 64-bit syntax pass, where nothing about a
 * 32-bit layout is being claimed.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90EQU_OFF(field, off, tag) \
	typedef char v90equ_off_##tag[ \
	    ((int)__builtin_offsetof(V90Equalizer, field) == (off)) ? 1 : -1]

V90EQU_OFF(linearEquBeta,		0x010, linearequbeta);
V90EQU_OFF(dfeBeta,			0x03c, dfebeta);
V90EQU_OFF(state,			0x060, state);
V90EQU_OFF(stateCount,			0x064, statecount);
V90EQU_OFF(mmxMode,			0x0b0, mmxmode);
V90EQU_OFF(linearEquMmxRefLevel,	0x0bc, leref);
V90EQU_OFF(linearEquMmxBetaScale,	0x0c4, lescale);
V90EQU_OFF(linearEquMmxBeta,		0x0cc, lebeta);
V90EQU_OFF(linearEquMmxShift,		0x0d0, leshift);
V90EQU_OFF(dfeMmxRefLevel,		0x0fc, dferef);
V90EQU_OFF(dfeMmxBetaScale,		0x104, dfescale);
V90EQU_OFF(dfeMmxBeta,			0x10c, dfebetai);
V90EQU_OFF(dfeMmxShift,			0x110, dfeshift);
typedef char v90equ_size[(sizeof(V90Equalizer) == 0x148) ? 1 : -1];
#endif


/*
 * log10() on the coprocessor, as the object computes it.
 *
 * `fldlg2` pushes log10(2) at the register's full 64-bit mantissa and
 * `fyl2x` computes st(1) * log2(st(0)) and pops, so the sequence takes one
 * value and leaves one -- net stack effect zero, which is what makes the
 * "=t"/"0" tie below legal.  See the file comment for why this is not
 * written as a call to log10().
 */
static inline long double
x87_log10(long double x)
{
	long double r;

	__asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x));
	return r;
}

/*
 * `shl %cl,%edx` with %edx holding 1.  The count is masked to five bits by
 * the hardware; C leaves `1 << n` undefined outside 0..31, and the shift
 * here is a truncated logarithm of two fields the caller controls, so the
 * mask is written out rather than left to chance.  This computes what the
 * object computes for every count, including the negative ones.
 */
static inline int
one_shifted_by(int n)
{
	return (int)(1u << ((unsigned int)n & 31u));
}


/*
 * V90Equalizer::setLinearEquBeta(float)
 *
 * _ZN12V90Equalizer16setLinearEquBetaEf, 350 bytes at 0x36490.
 */
void
V90Equalizer::setLinearEquBeta(float beta)
{
	/*
	 * `flds 0x10(%ebx); fcomp %st(1); fnstsw; sahf; je` -- the jump over
	 * the diagnostic is taken on ZF, and FCOM sets C3 for equal AND for
	 * unordered, so a NaN on either side skips the print where C's `!=`
	 * would take it.  Finding 236 is the same shape in `setParamEia6`.
	 */
	if (linearEquBeta < beta || linearEquBeta > beta) {
		long double scaled = (long double)beta * 1.0e10f;

		/*
		 * Three arguments, and none of them is the float: the object
		 * formats a fixed-point decimal itself.
		 *
		 *   %c  the sign.  `fldz; fcompp; sahf; sbb; and $-2; add
		 *       $0x2d` selects on CF alone, and FCOM sets C0 for
		 *       less-than AND for unordered, so the object's predicate
		 *       is `beta > 0 || unordered` -- written out below.  The
		 *       unordered arm is unreachable through the guard above.
		 *   %d  the integer part, `(int)fabs(scaled)`: `fld; fabs;
		 *       fistp` with the control word set to truncate.
		 *   %05d  five fractional digits, from the truncation error
		 *       times 1e5.  The object takes `(int)scaled - scaled`,
		 *       which is the NEGATIVE of the usual fractional part,
		 *       and then takes the absolute value of the integer --
		 *       `cltd; xor %edx,%eax; sub %edx,%eax`.
		 */
		edprintf("V90Equalizer: LE Beta = %c%d.%05de-10\r\n",
			 !(beta <= 0.0f) ? '+' : '-',
			 (int)__builtin_fabsl(scaled),
			 __builtin_abs((int)(((long double)(int)scaled - scaled)
					     * 1.0e5f)));
	}

	/* `fsts 0x10(%ebx)` -- a store that does not pop; beta is still live. */
	linearEquBeta = beta;

	if (mmxMode == 0)
		return;

	/*
	 * `fcoms <0.0f>; fnstsw; sahf; je` -- ZF again, so this arm is taken
	 * for zero and for unordered both, and C's `beta == 0.0f` is not the
	 * object's test.
	 */
	if (beta < 0.0f || beta > 0.0f) {
		int shift = (int)(x87_log10(__builtin_fabsl(
					(long double)linearEquMmxRefLevel
					/ ((long double)beta * 16777216.0f)))
				  / x87_log10((long double)2.0f));

		linearEquMmxShift = shift;
		linearEquMmxBeta = (int)((long double)beta
					 * linearEquMmxBetaScale
					 * (long double)one_shifted_by(shift));
	} else {
		linearEquMmxShift = 0;
		linearEquMmxBeta = 0;
	}
}


/*
 * V90Equalizer::setDfeBeta(float)
 *
 * _ZN12V90Equalizer10setDfeBetaEf, 350 bytes at 0x365f0.  Instruction for
 * instruction `setLinearEquBeta` with four fields moved, 1e10 down to 1e7,
 * 2**24 down to 2**20, and its own message.
 */
void
V90Equalizer::setDfeBeta(float beta)
{
	if (dfeBeta < beta || dfeBeta > beta) {
		long double scaled = (long double)beta * 1.0e7f;

		edprintf("V90Equalizer: DFE Beta = %c%d.%05de-7\r\n",
			 !(beta <= 0.0f) ? '+' : '-',
			 (int)__builtin_fabsl(scaled),
			 __builtin_abs((int)(((long double)(int)scaled - scaled)
					     * 1.0e5f)));
	}

	dfeBeta = beta;

	if (mmxMode == 0)
		return;

	if (beta < 0.0f || beta > 0.0f) {
		int shift = (int)(x87_log10(__builtin_fabsl(
					(long double)dfeMmxRefLevel
					/ ((long double)beta * 1048576.0f)))
				  / x87_log10((long double)2.0f));

		dfeMmxShift = shift;
		dfeMmxBeta = (int)((long double)beta
				   * dfeMmxBetaScale
				   * (long double)one_shifted_by(shift));
	} else {
		dfeMmxShift = 0;
		dfeMmxBeta = 0;
	}
}


/*
 * V90Equalizer::enterPhase3()
 *
 * _ZN12V90Equalizer11enterPhase3Ev, 88 bytes at 0x36790.  Idempotent: the
 * whole body is under `cmpl $0x1,0x60(%esi); je <return>`, which is the shape
 * `enterRRN`, `enterFPE` and `enterChannelVerification` share with it.
 *
 * The two zeros are passed as `mov $0x0,%ebx; mov %ebx,0x4(%esp)` -- an
 * integer register holding the float argument's bit pattern, which for 0.0f
 * is the same word.
 */
void
V90Equalizer::enterPhase3()
{
	if (state == V90EQU_STATE_PHASE3)
		return;

	edprintf("V90Equalizer: enter Phase 3\r\n");
	setLinearEquBeta(0.0f);
	setDfeBeta(0.0f);
	state = V90EQU_STATE_PHASE3;
	stateCount = 0;
}
