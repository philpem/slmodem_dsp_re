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
#include "dsplib/DspMath.h"
#include "dsplib/encode.h"

/*
 * `V90Resampler.h` brings in the NAMED `V90Parameters` map, which is the one
 * this file wants -- `reset` copies four slots out of the parameter block and
 * all four have the original author's own names (finding 861).  The other
 * definition, `V90PreFilter.h`'s 0x504 word block, must not be included in
 * the same translation unit; finding 1112.
 */
#include "dsplib/V90Resampler.h"

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

V90EQU_OFF(resampler,			0x000, resampler);
V90EQU_OFF(short_08,			0x008, short08);
V90EQU_OFF(linearEquLength,		0x00c, linearequlength);
V90EQU_OFF(linearEquBeta,		0x010, linearequbeta);
V90EQU_OFF(linearEquCoefs,		0x014, linearequcoefs);
V90EQU_OFF(array_18,			0x018, array18);
V90EQU_OFF(word_1c,			0x01c, word1c);
V90EQU_OFF(word_20,			0x020, word20);
V90EQU_OFF(linearEquWindow,		0x024, lewindow);
V90EQU_OFF(dfeWindow,			0x028, dfewindow);
V90EQU_OFF(linearEquWindowHalf,		0x02c, lewindowhalf);
V90EQU_OFF(dfeWindowHalf,		0x030, dfewindowhalf);
V90EQU_OFF(word_34,			0x034, word34);
V90EQU_OFF(dfeLength,			0x038, dfelength);
V90EQU_OFF(dfeBeta,			0x03c, dfebeta);
V90EQU_OFF(dfeCoefs,			0x040, dfecoefs);
V90EQU_OFF(array_44,			0x044, array44);
V90EQU_OFF(state,			0x060, state);
V90EQU_OFF(stateCount,			0x064, statecount);
V90EQU_OFF(word_68,			0x068, word68);
V90EQU_OFF(word_6c,			0x06c, word6c);
V90EQU_OFF(word_70,			0x070, word70);
V90EQU_OFF(errorEnergyMeanBlockLen,	0x074, eemblocklen);
V90EQU_OFF(word_78,			0x078, word78);
V90EQU_OFF(word_7c,			0x07c, word7c);
V90EQU_OFF(word_80,			0x080, word80);
V90EQU_OFF(word_84,			0x084, word84);
V90EQU_OFF(word_88,			0x088, word88);
V90EQU_OFF(word_8c,			0x08c, word8c);
V90EQU_OFF(errorEnergyMeanK,		0x090, eemk);
V90EQU_OFF(word_94,			0x094, word94);
V90EQU_OFF(word_9c,			0x09c, word9c);
V90EQU_OFF(word_a0,			0x0a0, worda0);
V90EQU_OFF(word_a4,			0x0a4, worda4);
V90EQU_OFF(params,			0x0a8, params);
V90EQU_OFF(mmxArraysPresent,		0x0ac, mmxarrays);
V90EQU_OFF(mmxMode,			0x0b0, mmxmode);
V90EQU_OFF(linearEquMmxRefLevel,	0x0bc, leref);
V90EQU_OFF(word_c0,			0x0c0, wordc0);
V90EQU_OFF(linearEquMmxBetaScale,	0x0c4, lescale);
V90EQU_OFF(linearEquMmxBeta,		0x0cc, lebeta);
V90EQU_OFF(linearEquMmxShift,		0x0d0, leshift);
V90EQU_OFF(linearEquMmxCoefs,		0x0d4, lemmxcoefs);
V90EQU_OFF(array_d8,			0x0d8, arrayd8);
V90EQU_OFF(array_ec,			0x0ec, arrayec);
V90EQU_OFF(dfeMmxRefLevel,		0x0fc, dferef);
V90EQU_OFF(word_100,			0x100, word100);
V90EQU_OFF(dfeMmxBetaScale,		0x104, dfescale);
V90EQU_OFF(dfeMmxBeta,			0x10c, dfebetai);
V90EQU_OFF(dfeMmxShift,			0x110, dfeshift);
V90EQU_OFF(dfeMmxCoefs,			0x114, dfemmxcoefs);
V90EQU_OFF(array_118,			0x118, array118);
V90EQU_OFF(array_12c,			0x12c, array12c);
V90EQU_OFF(word_13c,			0x13c, word13c);
V90EQU_OFF(word_140,			0x140, word140);
V90EQU_OFF(flag_144,			0x144, flag144);
V90EQU_OFF(flag_146,			0x146, flag146);
V90EQU_OFF(quickConnect,		0x148, quickconnect);
typedef char v90equ_size[(sizeof(V90Equalizer) == 0x150) ? 1 : -1];
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

/*
 * enterChannelVerification -- `enterPhase3`'s shape with a resampler call on
 * the end.
 *
 * The same early-out on the same field, the same two setters with zero, the
 * same state-and-count pair; then the one thing this one does that the other
 * does not, which is to put the resampler's band-limited loop into
 * V90_BLL_PRE_ANSPCM with a one-sample count.  `V90Demodulator::
 * enterChannelVerification` is the only caller.
 */
void
V90Equalizer::enterChannelVerification()
{
	if (state == V90EQU_STATE_CHANNEL_VERIFY)
		return;

	edprintf("V90Equalizer: enterChannelVerification\r\n");
	setLinearEquBeta(0.0f);
	setDfeBeta(0.0f);
	state = V90EQU_STATE_CHANNEL_VERIFY;
	stateCount = 0;

	resampler->setBllState(V90_BLL_PRE_ANSPCM, 1);
}

/*
 * The object's clamp, written as the object's predicates and not as the two
 * comparisons a reader would reach for.
 *
 * `fcom / fnstsw / sahf / jae` takes the NOT-LESS branch, and an unordered
 * compare sets C0, C2 and C3 -- so a NaN is "less" for this jump and comes
 * out of here as 0.0f, where `x < 0.0f ? 0.0f : ...` would have kept it.
 * The parameter is read out of a configuration block, so a NaN is not
 * obviously unreachable, and V90PreFilter.cpp's `setParamEia6` has the same
 * note for the same reason.
 */
static inline float
clamp_fade_ratio(float x)
{
	if (!(x >= 0.0f))
		return 0.0f;
	if (!(x <= 0.5f))
		return 0.5f;
	return x;
}

/*
 * reset -- the whole equaliser back to its constructed state, and the only
 * member of the class that reads the parameter block.
 *
 * FIVE THINGS IN IT ARE NOT OBVIOUS FROM THE STORES.
 *
 * 1. THE TWO BETAS ARE SEEDED WITH A SENTINEL BEFORE THE SETTERS RUN.
 *    `1e-14f` is exactly 0x283424dc, the immediate the object plants in
 *    +0x10 and +0x3c; `setLinearEquBeta` and `setDfeBeta` compare their
 *    argument against the field and return early when it matches, so seeding
 *    a value nothing can legitimately hold is what forces both to do their
 *    work for an argument of zero.  The header says the same thing from the
 *    setter's side.
 *
 * 2. THE CURSOR IS CLAMPED WITH UNSIGNED ARITHMETIC AND THE EDGE CASE IS
 *    REACHABLE.  `linearEquLength - 1` is 0xffffffff when the equaliser has
 *    no taps, and `jae` is unsigned, so the clamp does nothing and the 1.0f
 *    lands at `linearEquCoefs[cursor]` for whatever cursor was passed.  That
 *    is what the object does; it is not defended against here.
 *
 * 3. THE TWO FLOAT ARRAYS ARE CLEARED IN ONE LOOP, IN OPPOSITE DIRECTIONS.
 *    One counter, `linearEquCoefs[i]` ascending and `array_18[word_1c - 1 -
 *    i]` descending.
 *
 * 4. THE FIXED-POINT ARRAYS ARE ALL `+ 8` LONGER than the filter they belong
 *    to, and the whole group is skipped when `mmxArraysPresent` is clear --
 *    which is a different field from `mmxMode`, zeroed a hundred
 *    instructions earlier in the same function.
 *
 * 5. THE TWO WINDOW LENGTHS ARE BOTH SCALED BY THE LINEAR EQUALISER'S
 *    LENGTH, not one each by its own.  `fildll` converts `linearEquLength`
 *    once and `fmul` / `fmulp` apply it to both ratios, so `dfeWindowHalf`
 *    is a fraction of the LINEAR length and `dfeLength` is not in it at all.
 *    Read twice; it is what the object does.
 */
void
V90Equalizer::reset(unsigned int cursor)
{
	unsigned int i, n;
	float left, right, scale;

	edprintf("V90Equalizer: reset\r\n");

	short_08 = 0;
	linearEquBeta = 1e-14f;
	dfeBeta = 1e-14f;
	mmxMode = 0;
	linearEquMmxRefLevel = 0;
	word_c0 = 0;
	dfeMmxRefLevel = 0;
	word_100 = 0;

	setLinearEquBeta(0.0f);
	setDfeBeta(0.0f);

	state = V90EQU_STATE_RESET;
	stateCount = 0;

	word_20 = word_1c - linearEquLength - 1;

	for (i = 0; i < linearEquLength; i++) {
		linearEquCoefs[i] = 0;
		array_18[word_1c - 1 - i] = 0;
	}

	if (linearEquLength - 1 < cursor)
		cursor = linearEquLength - 1;
	linearEquCoefs[cursor] = 1.0f;

	for (i = 0; i < dfeLength; i++) {
		array_44[i] = 0;
		dfeCoefs[i] = 0;
	}

	if (mmxArraysPresent) {
		n = linearEquLength + 8;
		for (i = 0; i < n; i++) {
			linearEquMmxCoefs[i] = 0;
			array_d8[i] = 0;
		}

		n = word_1c + 8;
		for (i = 0; i < n; i++)
			array_ec[i] = 0;

		n = dfeLength + 8;
		for (i = 0; i < n; i++) {
			dfeMmxCoefs[i] = 0;
			array_118[i] = 0;
			array_12c[i] = 0;
		}
	}

	word_6c = 0;
	word_7c = 0;
	word_80 = 0;
	word_84 = 0;
	word_88 = 0;
	word_8c = 0;
	word_13c = 0;
	word_140 = 0;
	errorEnergyMeanK = params->ERROR_ENERGY_MEAN_K;
	word_68 = 0;
	word_70 = 0;
	errorEnergyMeanBlockLen = params->ERROR_ENERGY_MEAN_BLOCK_LEN;
	word_9c = 0;
	word_78 = 0;
	word_a0 = 0;
	word_a4 = 0;
	word_94 = 0;
	flag_144 = 1;
	flag_146 = 1;
	word_34 = 0;

	left = clamp_fade_ratio(params->LINEAR_EQU_FADE_LEFT_EDGE_RATIO);
	right = clamp_fade_ratio(params->LINEAR_EQU_FADE_RIGHT_EDGE_RATIO);

	scale = (float)linearEquLength;
	linearEquWindowHalf = (unsigned int)(left * scale);
	dfeWindowHalf = (unsigned int)(right * scale);

	hamming(linearEquWindow, 2 * linearEquWindowHalf);
	hamming(dfeWindow, 2 * dfeWindowHalf);
}
