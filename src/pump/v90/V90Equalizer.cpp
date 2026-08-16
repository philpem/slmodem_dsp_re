/*
 * V90Equalizer.cpp -- the step-size setters, the state entries, and the
 * object's lifecycle.
 *
 * Seven of the class's twenty-seven members: `setLinearEquBeta(float)`,
 * `setDfeBeta(float)` and `enterPhase3()` (which calls the other two);
 * `reset(unsigned)` and `enterChannelVerification()`; and the constructor and
 * destructor, which are one unit with `reset` because the constructor's last
 * act is a tail call to it (findings 1230-1232).
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
#include "dsplib/sysdep.h"

/*
 * The constructor reaches one word of the HOST's parameter block, through
 * `V90Parameters::modemParams`, to decide whether the fixed-point arrays are
 * wanted at all.  `V90Parameters.h` only forward-declares that structure.
 */
#include "dsplib/modem_params.h"

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
 * Explicitly, because this file READS the parameter block's named fields.  It
 * used to arrive through `V90Resampler.h`, which now only declares the class
 * -- it holds one as a pointer and never dereferences it.  A translation unit
 * that dereferences a type is the translation unit that must include it.
 */
#include "dsplib/V90Parameters.h"

/*
 * `enterRRN` and `enterFPE` dereference two of the six pointers the class
 * otherwise only stores: the spectral verifier's +0x28 and the pre-filter's
 * `isV90WithEia6()`.
 *
 * INCLUDING `V90PreFilter.h` HERE IS NO LONGER A CONFLICT.  It used to carry
 * a second, smaller `V90Parameters` -- the 0x504 word block this file's
 * comment above warns about -- and task #116 reconciled the two (finding
 * 1112), so the header now includes the same `V90Parameters.h` this file
 * does.  `tools/onedef.py` is the gate that keeps it that way.
 */
#include "dsplib/V90SpectralVerifier.h"
#include "dsplib/V90PreFilter.h"

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
V90EQU_OFF(savedBllState,		0x004, savedbll);
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
V90EQU_OFF(meanErrorEnergyCurrent,	0x080, meecurrent);
V90EQU_OFF(meanErrorEnergyMean,		0x084, meemean);
V90EQU_OFF(meanErrorEnergyMin,		0x088, meemin);
V90EQU_OFF(meanErrorEnergyMax,		0x08c, meemax);
V90EQU_OFF(errorEnergyMeanK,		0x090, eemk);
V90EQU_OFF(phase3Demod,			0x048, phase3demod);
V90EQU_OFF(phase4Demod,			0x04c, phase4demod);
V90EQU_OFF(demapper,			0x050, demapper);
V90EQU_OFF(connEval,			0x054, conneval);
V90EQU_OFF(spectralVerifier,		0x058, specverif);
V90EQU_OFF(preFilter,			0x05c, prefilter);
V90EQU_OFF(word_94,			0x094, word94);
V90EQU_OFF(meanErrorEnergy,		0x098, meebuf);
V90EQU_OFF(meanErrorCount,		0x09c, meecount);
V90EQU_OFF(meanErrorFull,		0x0a0, meefull);
V90EQU_OFF(word_a4,			0x0a4, worda4);
V90EQU_OFF(params,			0x0a8, params);
V90EQU_OFF(mmxArraysPresent,		0x0ac, mmxarrays);
V90EQU_OFF(mmxMode,			0x0b0, mmxmode);
V90EQU_OFF(block_b4,			0x0b4, blockb4);
V90EQU_OFF(block_b8,			0x0b8, blockb8);
V90EQU_OFF(maxLeCoefValue,		0x0bc, maxlecoef);
V90EQU_OFF(minLeCoefValue,		0x0c0, minlecoef);
V90EQU_OFF(linearEquMmxConversionFactor,	0x0c4, lescale);
V90EQU_OFF(linearEquMmxOutputConversionFactor,	0x0c8, leoutscale);
V90EQU_OFF(linearEquMmxBeta,		0x0cc, lebeta);
V90EQU_OFF(linearEquMmxShift,		0x0d0, leshift);
V90EQU_OFF(linearEquMmxCoefs,		0x0d4, lemmxcoefs);
V90EQU_OFF(array_d8,			0x0d8, arrayd8);
V90EQU_OFF(linearEquMmxCoefsAligned,	0x0dc, lemmxalign);
V90EQU_OFF(array_d8Aligned,		0x0e0, arrayd8align);
V90EQU_OFF(linearEquMmxCoefsSkew,	0x0e4, lemmxskew);
V90EQU_OFF(array_d8Skew,		0x0e8, arrayd8skew);
V90EQU_OFF(array_ec,			0x0ec, arrayec);
V90EQU_OFF(array_ecAligned,		0x0f0, arrayecalign);
V90EQU_OFF(array_ecSkew,		0x0f4, arrayecskew);
V90EQU_OFF(word_20Saved,		0x0f8, word20saved);
V90EQU_OFF(maxDfeCoefValue,		0x0fc, maxdfecoef);
V90EQU_OFF(minDfeCoefValue,		0x100, mindfecoef);
V90EQU_OFF(dfeMmxConversionFactor,		0x104, dfescale);
V90EQU_OFF(dfeMmxOutputConversionFactor,	0x108, dfeoutscale);
V90EQU_OFF(dfeMmxBeta,			0x10c, dfebetai);
V90EQU_OFF(dfeMmxShift,			0x110, dfeshift);
V90EQU_OFF(dfeMmxCoefs,			0x114, dfemmxcoefs);
V90EQU_OFF(array_118,			0x118, array118);
V90EQU_OFF(dfeMmxCoefsAligned,		0x11c, dfemmxalign);
V90EQU_OFF(array_118Aligned,		0x120, array118align);
V90EQU_OFF(dfeMmxCoefsSkew,		0x124, dfemmxskew);
V90EQU_OFF(array_118Skew,		0x128, array118skew);
V90EQU_OFF(array_12c,			0x12c, array12c);
V90EQU_OFF(array_12cAligned,		0x130, array12calign);
V90EQU_OFF(array_12cSkew,		0x134, array12cskew);
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
 * One statistic, printed as the same hand-built fixed-point decimal the two
 * step-size setters use -- three printf arguments and not one float.
 *
 *   %c    the sign.  `fldz; fcomps v; sbb; and $-2; add $0x2d` selects on CF
 *         alone, and FCOM sets C0 for less-than AND for unordered, so the
 *         object's predicate is `0 < v || unordered`.
 *   %d    the integer part, `(int)fabs(v)`: `fld; fabs; fistpl` with the
 *         control word set to truncate.
 *   %06d  six fractional digits.  `fistl` leaves `(int)v` in memory WITHOUT
 *         popping, `fildl` reads it back, and `de e2` -- FSUBRP, which
 *         objdump prints as its own opposite (finding 245) -- computes
 *         `v - (float)(int)v`, the ordinary fractional part, which is then
 *         scaled by 1e6 as a DOUBLE (`fldl`, not `flds`) and made positive
 *         with `cltd; xor; sub`.
 *
 * NOTE THE DIFFERENCE FROM THE SETTERS: they take `(int)x - x`, the negative
 * of the fractional part, and this takes `x - (int)x`.  Both then take the
 * absolute value, so the printed digits agree; the subtraction order is read
 * from the bytes either way.
 */
static inline void
edprint_stat(const char *fmt, float v, long double scale)
{
	edprintf(fmt, !(v <= 0.0f) ? '+' : '-',
		 (int)__builtin_fabsl((long double)v),
		 __builtin_abs((int)(((long double)v
				      - (long double)(int)v) * scale)));
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
					(long double)maxLeCoefValue
					/ ((long double)beta * 16777216.0f)))
				  / x87_log10((long double)2.0f));

		linearEquMmxShift = shift;
		linearEquMmxBeta = (int)((long double)beta
					 * linearEquMmxConversionFactor
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
					(long double)maxDfeCoefValue
					/ ((long double)beta * 1048576.0f)))
				  / x87_log10((long double)2.0f));

		dfeMmxShift = shift;
		dfeMmxBeta = (int)((long double)beta
				   * dfeMmxConversionFactor
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
	maxLeCoefValue = 0;
	minLeCoefValue = 0;
	maxDfeCoefValue = 0;
	minDfeCoefValue = 0;

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
	meanErrorEnergyCurrent = 0;
	meanErrorEnergyMean = 0;
	meanErrorEnergyMin = 0;
	meanErrorEnergyMax = 0;
	word_13c = 0;
	word_140 = 0;
	errorEnergyMeanK = params->ERROR_ENERGY_MEAN_K;
	word_68 = 0;
	word_70 = 0;
	errorEnergyMeanBlockLen = params->ERROR_ENERGY_MEAN_BLOCK_LEN;
	meanErrorCount = 0;
	word_78 = 0;
	meanErrorFull = 0;
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

/* ================================================================ lifecycle */

/*
 * How many SHORTS have to be stepped over to reach an eight-byte boundary.
 *
 *      8d 51 07    lea 0x7(%ecx),%edx
 *      83 e2 f8    and $0xfffffff8,%edx
 *      29 ca       sub %ecx,%edx
 *      d1 ea       shr $1,%edx
 *
 * -- transcribed rather than written as `(-(unsigned)p) & 7`, which is the
 * same number by a different route.  The shift is LOGICAL and the difference
 * is at most seven, so the divide is exact and the count is 0, 1, 2 or 3.
 */
static inline unsigned int
mmx_skew(const short *p)
{
	unsigned long a = (unsigned long)p;

	return (unsigned int)((((a + 7) & ~7ul) - a) >> 1);
}

/*
 * THE CONSTRUCTOR IS AN ALLOCATOR AND NOTHING ELSE.  Eleven arguments in,
 * seven of them stored untouched, three lengths derived, fifteen
 * `sysdep_malloc`s, six alignment triples, one diagnostic, and a tail call to
 * `reset`.  It initialises no other field: everything the object holds when
 * it is handed back that is not a pointer or a length was written by `reset`.
 *
 * THE THREE LENGTHS ARE ROUNDED DOWN, TWO OF THEM TO A MULTIPLE OF FOUR.
 * `shr $2` then a scale by four is `& ~3u` written so the quotient can be
 * reused as the malloc's scale -- `shl $4` on the same register is
 * `length * sizeof(float)` -- and it is a LOGICAL shift, which is what makes
 * both arguments unsigned as the mangling already said.  `word_1c` is rounded
 * to a multiple of two instead, and from a SIGNED divide:
 *
 *      c1 ea 1f    shr $0x1f,%edx      ; sign bit
 *      01 d1       add %edx,%ecx       ; round toward zero
 *      d1 f9       sar $1,%ecx
 *
 * so `LINEAR_EQU_HISTORY_LENGTH` is an int and a negative one would make the
 * length negative and the allocation enormous.  See docs/deviations.md D175.
 *
 * WHAT DECIDES `mmxArraysPresent`.  Three things: the parameter block's
 * `ENABLE_EQUALIZER_MMX`, the computational mode, and one word of the host's
 * modem parameter block reached through `params->modemParams`.  The two arms
 * do not test that word the same way -- mode 1 accepts everything except 2,
 * every other mode accepts only 1 -- and the object writes it as two arms
 * that GCC then cross-jumps, the `je` at 3b774 landing in the middle of the
 * other arm's comparison.  Written here as the two arms it came from.
 *
 * THE FIXED-POINT ARRAYS ARE ALLOCATED RAW AND USED ALIGNED.  Each of the six
 * is followed by `skew = (align8(p) - p) / 2` and `aligned = p + 2 * skew`;
 * see the note in the header.  `reset` clears the RAW array, not the aligned
 * one, so the last `skew` shorts of each allocation are cleared and the eight
 * extra entries the length carries are what keeps that inside the block.
 */
V90Equalizer::V90Equalizer(unsigned int linearEquLen, unsigned int dfeLen,
			   V90Phase3Demodulator *p3d, V90Phase4Demodulator *p4d,
			   V90Demapper *dem, V90ConnectionEvaluator *ce,
			   V90SpectralVerifier *sv, V90Parameters *parms,
			   V90Resampler *rs, V90PreFilter *pf,
			   V90ComputationalMode mode)
{
	params = parms;
	resampler = rs;

	phase3Demod = p3d;
	phase4Demod = p4d;
	demapper = dem;
	connEval = ce;
	spectralVerifier = sv;
	preFilter = pf;

	linearEquLength = linearEquLen & ~3u;
	linearEquCoefs = (float *)sysdep_malloc(linearEquLength *
						sizeof(float));

	word_1c = 2 * (unsigned int)(params->LINEAR_EQU_HISTORY_LENGTH / 2);
	array_18 = (float *)sysdep_malloc(word_1c * sizeof(float));

	linearEquWindow = (float *)sysdep_malloc(linearEquLength *
						 sizeof(float));
	dfeWindow = (float *)sysdep_malloc(linearEquLength * sizeof(float));

	dfeLength = dfeLen & ~3u;
	dfeCoefs = (float *)sysdep_malloc(dfeLength * sizeof(float));
	array_44 = (float *)sysdep_malloc(dfeLength * sizeof(float));

	mmxArraysPresent = 0;
	if (params->ENABLE_EQUALIZER_MMX != 0) {
		if ((int)mode == V90EQU_COMP_MODE_1) {
			if (params->modemParams->unnamed_005c != 2)
				mmxArraysPresent = 1;
		} else {
			if (params->modemParams->unnamed_005c == 1)
				mmxArraysPresent = 1;
		}
	}

	if (mmxArraysPresent != 0) {
		block_b8 = sysdep_malloc(0x200);
		block_b4 = sysdep_malloc(0x400);

		linearEquMmxCoefs = (short *)sysdep_malloc(
		    (linearEquLength + 8) * sizeof(short));
		array_d8 = (short *)sysdep_malloc(
		    (linearEquLength + 8) * sizeof(short));
		array_ec = (short *)sysdep_malloc(
		    (word_1c + 8) * sizeof(short));
		dfeMmxCoefs = (short *)sysdep_malloc(
		    (dfeLength + 8) * sizeof(short));
		array_118 = (short *)sysdep_malloc(
		    (dfeLength + 8) * sizeof(short));
		array_12c = (short *)sysdep_malloc(
		    (dfeLength + 8) * sizeof(short));

		linearEquMmxCoefsSkew = mmx_skew(linearEquMmxCoefs);
		linearEquMmxCoefsAligned = linearEquMmxCoefs +
		    linearEquMmxCoefsSkew;

		array_d8Skew = mmx_skew(array_d8);
		array_d8Aligned = array_d8 + array_d8Skew;

		array_ecSkew = mmx_skew(array_ec);
		array_ecAligned = array_ec + array_ecSkew;

		dfeMmxCoefsSkew = mmx_skew(dfeMmxCoefs);
		dfeMmxCoefsAligned = dfeMmxCoefs + dfeMmxCoefsSkew;

		array_118Skew = mmx_skew(array_118);
		array_118Aligned = array_118 + array_118Skew;

		array_12cSkew = mmx_skew(array_12c);
		array_12cAligned = array_12c + array_12cSkew;

		edprintf("V90Equalizer: Created - MMX mode is enabled.\r\n");
	} else {
		edprintf("V90Equalizer: Created - MMX mode is disabled.\r\n");
	}

	meanErrorEnergy = (float *)sysdep_malloc(V90EQU_MEAN_ERROR_LEN *
						 sizeof(float));

	reset(linearEquLength / 2);
}

/*
 * The destructor frees the fifteen blocks the constructor took, each under
 * its own null test, and does nothing else: it does not clear the pointers,
 * so a second call frees every one of them again.  The eight fixed-point
 * blocks are gated on `mmxArraysPresent` exactly as their allocation was --
 * the one field that has to survive from the constructor for the object to be
 * destroyed correctly.
 *
 * THE ORDER IS NOT THE ALLOCATION ORDER: +0x40 and +0x44 come before +0x24
 * and +0x28, and +0x98 -- allocated last -- is freed seventh, before the
 * fixed-point group.  It is transcribed rather than tidied.
 *
 * The six aligned pointers are NOT freed, and must not be: each points into
 * the middle of a block whose raw pointer is freed beside it.
 */
V90Equalizer::~V90Equalizer()
{
	if (linearEquCoefs != 0)
		sysdep_free(linearEquCoefs);
	if (array_18 != 0)
		sysdep_free(array_18);
	if (dfeCoefs != 0)
		sysdep_free(dfeCoefs);
	if (array_44 != 0)
		sysdep_free(array_44);
	if (linearEquWindow != 0)
		sysdep_free(linearEquWindow);
	if (dfeWindow != 0)
		sysdep_free(dfeWindow);
	if (meanErrorEnergy != 0)
		sysdep_free(meanErrorEnergy);

	if (mmxArraysPresent != 0) {
		if (block_b8 != 0)
			sysdep_free(block_b8);
		if (block_b4 != 0)
			sysdep_free(block_b4);
		if (linearEquMmxCoefs != 0)
			sysdep_free(linearEquMmxCoefs);
		if (array_d8 != 0)
			sysdep_free(array_d8);
		if (array_ec != 0)
			sysdep_free(array_ec);
		if (dfeMmxCoefs != 0)
			sysdep_free(dfeMmxCoefs);
		if (array_118 != 0)
			sysdep_free(array_118);
		if (array_12c != 0)
			sysdep_free(array_12c);
	}
}

/* ============================================================= coefficients */

/*
 * THE FIXED-POINT COEFFICIENT IS THIRTY-TWO BITS SPLIT ACROSS TWO ARRAYS.
 * Every conversion in this class reads it as
 *
 *      0f bf 04 4a     movswl (%edx,%ecx,2),%eax    ; the high half, SIGNED
 *      0f b7 14 4f     movzwl (%edi,%ecx,2),%edx    ; the low half, UNSIGNED
 *      c1 e0 10        shl    $0x10,%eax
 *      09 d0           or     %edx,%eax
 *
 * -- the high sixteen bits in `*MmxCoefsAligned` and the low sixteen in the
 * array beside it -- and writes it back with the halves in the same places
 * (`mov %ax,(%edx); sar $0x10,%eax; mov %ax,(%ecx)`).  The two extensions
 * differ and both are FORCED: the high half's sign is the value's sign and
 * the low half must not sign-extend into it, so `movswl`/`movzwl` is the one
 * pairing that reconstructs the word.  That is why the low half is read
 * through an `unsigned short` cast below and the high half is not.
 *
 * The pair is always the ALIGNED pointers, never the raw ones -- which is the
 * opposite of what `reset`, `zeroLinearEquCoefs` and `zeroDfeCoefs` clear.
 */
static inline int
mmx_coef_get(const short *hi, const short *lo, unsigned int i)
{
	return ((int)hi[i] << 16) | (unsigned short)lo[i];
}

/*
 * `getDfeBeta` -- `mov 0x4(%esp),%eax; flds 0x3c(%eax); ret`, and that is the
 * whole function.  The value comes back in st(0), so the return type is float.
 */
float
V90Equalizer::getDfeBeta()
{
	return dfeBeta;
}

/*
 * The two diagnostic accumulators, and nothing else: `xor %edx,%edx; xor
 * %ecx,%ecx; mov %edx,0x9c(%eax); mov %ecx,0xa0(%eax); ret`.  Two zeroed
 * registers for two stores, so two assignments and not one wider one.
 */
void
V90Equalizer::resetMeanErrorEnergyDiagnostics()
{
	meanErrorCount = 0;
	meanErrorFull = 0;
}

/*
 * The two coefficient loaders.  `mov (%esi,%edx,4),%eax; mov %eax,(%ecx,%edx,4)`
 * is a plain copy, and the count is UNSIGNED -- the guard is `cmp %ebx,%edx;
 * jae` and the latch is `jb`, so a count of zero copies nothing and there is
 * no bound against the equaliser's own length.  The destination pointer is
 * loaded once, inside the guard.
 */
void
V90Equalizer::setLinearEquCoeff(float *src, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		linearEquCoefs[i] = src[i];
}

void
V90Equalizer::setDfeCoeff(float *src, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		dfeCoefs[i] = src[i];
}

/*
 * `zeroLinearEquCoefs` and `zeroDfeCoefs` -- the float array, then the two
 * fixed-point arrays if the equaliser is in fixed-point mode.
 *
 * THE GATE IS `mmxMode` AND NOT `mmxArraysPresent`.  `reset` clears the same
 * arrays under `mmxArraysPresent`; these two read +0xb0, which is the mode.
 * The `+ 8` on the length is the same slack `reset` and the constructor use,
 * and the arrays cleared are the RAW pointers, so the clear starts before the
 * aligned view and the eight extra entries are what keeps it in the block.
 */
void
V90Equalizer::zeroLinearEquCoefs()
{
	unsigned int i, n;

	for (i = 0; i < linearEquLength; i++)
		linearEquCoefs[i] = 0;

	if (mmxMode != 0) {
		n = linearEquLength + 8;
		for (i = 0; i < n; i++) {
			linearEquMmxCoefs[i] = 0;
			array_d8[i] = 0;
		}
	}
}

void
V90Equalizer::zeroDfeCoefs()
{
	unsigned int i, n;

	for (i = 0; i < dfeLength; i++)
		dfeCoefs[i] = 0;

	if (mmxMode != 0) {
		n = dfeLength + 8;
		for (i = 0; i < n; i++) {
			dfeMmxCoefs[i] = 0;
			array_118[i] = 0;
		}
	}
}

/*
 * setLinearEquEdgesFadingParams -- `reset`'s last five statements, exposed.
 *
 * The same two clamps against the same 0.5f (`.rodata.cst4+0x294`), the same
 * `fildll` of `linearEquLength` scaling BOTH ratios -- one `fmul %st,%st(1)`
 * and one `fmulp %st,%st(2)`, so the DFE half is a fraction of the LINEAR
 * length here too -- the same pair of truncating `fistpll`s, and the same two
 * `hamming` calls with the second a tail call.  Written against the same
 * `clamp_fade_ratio` as `reset` so the two cannot drift apart.
 */
void
V90Equalizer::setLinearEquEdgesFadingParams(float left, float right)
{
	float l, r, scale;

	l = clamp_fade_ratio(left);
	r = clamp_fade_ratio(right);

	scale = (float)linearEquLength;
	linearEquWindowHalf = (unsigned int)(l * scale);
	dfeWindowHalf = (unsigned int)(r * scale);

	hamming(linearEquWindow, 2 * linearEquWindowHalf);
	hamming(dfeWindow, 2 * dfeWindowHalf);
}

/*
 * freeze -- both step sizes to zero and nothing else.  The two zeros go out
 * the same way `enterPhase3` sends them, as an integer register holding the
 * float's bit pattern.
 */
void
V90Equalizer::freeze()
{
	setLinearEquBeta(0.0f);
	setDfeBeta(0.0f);
}

/*
 * restoreEqualizerToFloat -- the fixed-point coefficients back into the float
 * arrays, and the mode off.
 *
 * Three loops and a restore.  The first and third undo the scaling
 * `convertEqualizerToMmx` applied: the object divides ONE by the scale and
 * multiplies, rather than dividing by it, and the reciprocal is recomputed
 * inside the loop -- `fld1` outside, `fld %st(0)` then `fdivs` within.  It is
 * written as the one expression it is; hoisting the reciprocal is the
 * compiler's business and changes no value.
 *
 * The second loop is a plain 16-bit widening, `filds` into `fstps`, with no
 * scale at all -- so whatever `array_ec` holds is a count and not a
 * coefficient.
 *
 * BETWEEN THEM, +0x20 IS RESTORED FROM +0xf8, which is where
 * `convertEqualizerToMmx` parked it.  See the header.
 *
 * The whole body is under `mmxMode`, and the mode is cleared last -- so a
 * second call does nothing, which is what makes this callable from `enterRRN`
 * and `enterFPE` without either of them checking first.
 */
void
V90Equalizer::restoreEqualizerToFloat()
{
	unsigned int i;

	if (mmxMode == 0)
		return;

	for (i = 0; i < linearEquLength; i++)
		linearEquCoefs[i] = (1.0f / linearEquMmxConversionFactor)
		    * (float)mmx_coef_get(linearEquMmxCoefsAligned,
					  array_d8Aligned, i);

	for (i = 0; i < word_1c; i++)
		array_18[i] = (float)array_ecAligned[i];

	word_20 = word_20Saved;

	for (i = 0; i < dfeLength; i++) {
		dfeCoefs[i] = (1.0f / dfeMmxConversionFactor)
		    * (float)mmx_coef_get(dfeMmxCoefsAligned,
					  array_118Aligned, i);
		array_44[i] = (float)array_12cAligned[i];
	}

	edprintf("V90Equalizer: Equalizer restored to FLOAT mode.\r\n");
	mmxMode = 0;
}

/*
 * linearEquFadeEdges -- taper both ends of the linear equaliser with the two
 * Hamming windows `reset` built.
 *
 * FOUR STEPS, AND THE FIRST AND LAST ONLY HAPPEN IN FIXED-POINT MODE.  The
 * windows are float, so a fixed-point equaliser is converted out, tapered, and
 * converted back: step 1 is `restoreEqualizerToFloat`'s first and third loops
 * with `array_18`, `array_44` and +0x20 left alone, and step 4 is
 * `convertEqualizerToMmx`'s coefficient loop with nothing else.  The mode is
 * NOT changed, and no diagnostic is printed.
 *
 * THE TWO WINDOWS ARE APPLIED TO ONE ARRAY, FROM ITS TWO ENDS.
 * `linearEquWindow[i]` multiplies `linearEquCoefs[i]` going up, and
 * `dfeWindow[j]` multiplies `linearEquCoefs[linearEquLength - 1 - j]` coming
 * down: `mov %esi,%eax; sub %edx,%eax; fmuls -0x4(%ecx,%eax,4)` is
 * `linearEquLength - j`, indexed one word back.  `dfeCoefs` is not tapered by
 * either -- which is what says `dfeWindow` and `dfeWindowHalf` are named for
 * the parameter that sets them and not for the filter they touch.
 *
 * Nothing bounds `linearEquWindowHalf + dfeWindowHalf` against
 * `linearEquLength`, so an overlapping pair tapers the middle twice; `reset`
 * and `setLinearEquEdgesFadingParams` both clamp each ratio to 0.5
 * independently, which permits exactly that.  D-entry in docs/deviations.md.
 */
void
V90Equalizer::linearEquFadeEdges()
{
	unsigned int i;

	if (mmxMode != 0) {
		for (i = 0; i < linearEquLength; i++)
			linearEquCoefs[i] = (1.0f / linearEquMmxConversionFactor)
			    * (float)mmx_coef_get(linearEquMmxCoefsAligned,
						  array_d8Aligned, i);

		for (i = 0; i < dfeLength; i++)
			dfeCoefs[i] = (1.0f / dfeMmxConversionFactor)
			    * (float)mmx_coef_get(dfeMmxCoefsAligned,
						  array_118Aligned, i);
	}

	for (i = 0; i < linearEquWindowHalf; i++)
		linearEquCoefs[i] = linearEquWindow[i] * linearEquCoefs[i];

	for (i = 0; i < dfeWindowHalf; i++)
		linearEquCoefs[linearEquLength - i - 1] =
		    dfeWindow[i] * linearEquCoefs[linearEquLength - i - 1];

	if (mmxMode != 0) {
		for (i = 0; i < linearEquLength; i++) {
			int v = (int)(linearEquCoefs[i]
				      * linearEquMmxConversionFactor);

			array_d8Aligned[i] = (short)v;
			linearEquMmxCoefsAligned[i] = (short)(v >> 16);
		}
	}
}

/*
 * The conversion factor's diagnostic: `edprint_stat`'s hand-built decimal with
 * the sign and the digits taken from DIFFERENT expressions.  The object
 * reloads the member for the comparison --
 *
 *      3757f:  d9 ee           fldz
 *      3758e:  d8 9d c4 ..     fcomps 0xc4(%ebp)
 *
 * -- while the three printed digits come from that member scaled by 1e-8,
 * which is what the "e8" on the end of the format string means.  The scale is
 * a DOUBLE (`fldl` out of .rodata.cst8) and the fractional multiplier is a
 * float (`flds` out of .rodata.cst4); 1e-8 is not exactly representable in
 * either, so the two are different numbers and the printed digits show it.
 */
static inline void
edprint_scaled_stat(const char *fmt, float v, long double scaled,
		    long double scale)
{
	edprintf(fmt, !(v <= 0.0f) ? '+' : '-',
		 (int)__builtin_fabsl(scaled),
		 __builtin_abs((int)((scaled - (long double)(int)scaled)
				     * scale)));
}

/*
 * The same decimal applied to an INTEGER, which is why its fractional digits
 * are a compile-time zero: `v - (int)v` is integer arithmetic and folds before
 * it ever reaches the coprocessor, so the whole third argument becomes `xor
 * %esi,%esi` and the object carries no 1e3 constant at all.  THE MISSING
 * CONSTANT IS THE EVIDENCE -- .rodata.cst4 holds every other scale this
 * function uses (1e5, 1e4, 2**30, 2**24, 2**20, 2**-16, 2.0) and nothing near
 * a thousand -- so the argument was an int and not a float.  Finding 2146.
 *
 * The sign still costs a comparison on the coprocessor, because it is against
 * a float zero and the usual arithmetic conversions promote:
 *
 *      375dc:  d9 ee           fldz
 *      375de:  db 85 c8 ..     fildl 0xc8(%ebp)
 *      375ed:  de d9           fcompp
 */
static inline void
edprint_int_stat(const char *fmt, int v)
{
	edprintf(fmt, !(v <= 0.0f) ? '+' : '-',
		 (int)__builtin_fabsl((long double)v),
		 __builtin_abs((int)((v - (int)v) * 1.0e3f)));
}

/*
 * convertEqualizerToMmx -- the float coefficients into the fixed-point pair,
 * and the mode on.  `restoreEqualizerToFloat` is the other half.
 *
 * 2,573 bytes, and about two thirds of them are the twenty-seven diagnostic
 * lines.  Each half is: the scaling factors, the step-size renormalisation,
 * the coefficient conversion, a summary of what came out, and the history
 * array; then the DFE half again with its own fields, its own constants and
 * its own strings, the way `setLinearEquBeta` and `setDfeBeta` are the same
 * 350 bytes twice.  Written out twice here for the same reason.
 *
 * THE SCALING IS COMPUTED ONCE AND KEPT IN A REGISTER.  `fsts 0xc4(%ebp)` is a
 * store that does not pop, and every later use is the x87 copy -- `fmul
 * %st(2),%st` in the coefficient loop, `fmul %st,%st(2)` for the output factor
 * and `fmul %st(1),%st` for the step size -- so all three see the UNROUNDED
 * value and not the 24-bit float left in the object.  `setLinearEquBeta` and
 * `linearEquFadeEdges` read the member, and copying them here would be wrong
 * in the last place a coefficient can differ.  The one thing that DOES read
 * the member back is the diagnostic, `fmuls 0xc4(%ebp)`.
 *
 * THE RENORMALISATION IS THE SETTER'S WITH THE DIVISION TURNED INSIDE OUT.
 * `setLinearEquBeta` computes `maxLeCoefValue / (beta * 2**24)`; this computes
 * the RECIPROCAL of the denominator and multiplies:
 *
 *      37383:  d9 e8           fld1                    ; and 1.0/maxLeCoef
 *      373d8:  d8 0d ..        fmuls  <2**24>          ;   comes off the same
 *      373de:  de fc           FDIVP  st(4)=st(4)/st(0); 1.0/(beta * 2**24)
 *      373e2:  de cb           FMULP  st(3)=st(3)*st(0); times maxLeCoefValue
 *
 * A reciprocal rounds, so the two routes can differ in the last bit and the
 * TRUNCATED logarithm downstream can differ by a whole step; docs/deviations.md
 * D327.  (objdump prints both `DE` forms as their own opposite -- finding 245.)
 *
 * THE FOUR SUMS ARE `float` AND NOT `long double`.  Each accumulator is stored
 * back to a four-byte stack slot every iteration (`fstps 0x4c(%esp)` and
 * `fstps 0x48(%esp)`, and 0x28/0x24 in the DFE half), so the object rounds to
 * single precision at every step and the printed digits depend on it.  Finding
 * 2139's rule, applied where it fires.
 *
 * THE HISTORY LOOP IS NOT A COEFFICIENT LOOP.  It converts `array_18` to 16
 * bits with no scale at all -- `fistps`, a two-byte store -- which is the same
 * thing `restoreEqualizerToFloat`'s middle loop says from the other side, and
 * it tracks the largest and smallest MAGNITUDE as a `short`, so the magnitude
 * of -32768 comes back negative.  docs/deviations.md D328.
 */
void
V90Equalizer::convertEqualizerToMmx()
{
	long double conv;
	float fsum, fabsSum;
	int sumHi, absSumHi, maxHi, minHi;
	int maxHist, minHist;
	unsigned int i;

	/*
	 * Three ways not to convert: already converted, no fixed-point arrays
	 * to convert into, or the parameter block says the equaliser may not
	 * use them.  `mmxMode` is written on the way out even though two of the
	 * three arms have just proved it is already zero.
	 */
	if (mmxMode != 0 || mmxArraysPresent == 0
	    || params->ENABLE_EQUALIZER_MMX == 0) {
		edprintf("V90Equalizer: LE & DFE running in FLOAT mode.\r\n");
		mmxMode = 0;
		return;
	}

	/* ------------------------------------------- the linear equaliser */

	conv = (1.0f / (long double)maxLeCoefValue) * 1073741824.0f;
	linearEquMmxConversionFactor = (float)conv;
	linearEquMmxOutputConversionFactor = (int)((1.0f / 65536.0f) * conv);

	/*
	 * `fcom %st(1)` against `fldz` and `je`, so this arm is taken for zero
	 * and for unordered both -- the setters' test, and not C's `!= 0.0f`.
	 */
	if (linearEquBeta < 0.0f || linearEquBeta > 0.0f) {
		int shift = (int)(x87_log10(__builtin_fabsl(
					(1.0f / ((long double)linearEquBeta
						 * 16777216.0f))
					* (long double)maxLeCoefValue))
				  / x87_log10((long double)2.0f));

		linearEquMmxShift = shift;
		linearEquMmxBeta = (int)((long double)linearEquBeta * conv
					 * (long double)one_shifted_by(shift));
	} else {
		linearEquMmxShift = 0;
		linearEquMmxBeta = 0;
	}

	/*
	 * The split write, `linearEquFadeEdges`'s exactly: the low half into
	 * the array beside the coefficients, the high half into the
	 * coefficients' own, and both through the ALIGNED pointers.
	 */
	for (i = 0; i < linearEquLength; i++) {
		int v = (int)(linearEquCoefs[i] * conv);

		array_d8Aligned[i] = (short)v;
		linearEquMmxCoefsAligned[i] = (short)(v >> 16);
	}

	/*
	 * What came out, read back through the same aligned pointer.  The
	 * maximum and the minimum are of the MAGNITUDE -- `cltd; xor; sub`
	 * runs before both comparisons -- and the signed sum is not; the
	 * minimum starts at 0x10000, one past anything a `short` can hold, so
	 * an empty filter prints 65536.  D326.
	 */
	sumHi = 0;
	absSumHi = 0;
	maxHi = 0;
	minHi = 0x10000;
	fsum = 0.0f;
	fabsSum = 0.0f;

	for (i = 0; i < linearEquLength; i++) {
		int hi = linearEquMmxCoefsAligned[i];
		int a = __builtin_abs(hi);

		sumHi += hi;
		if (a > maxHi)
			maxHi = a;
		if (a < minHi)
			minHi = a;
		absSumHi += a;
		fsum += linearEquCoefs[i];
		fabsSum += __builtin_fabsf(linearEquCoefs[i]);
	}

	edprintf("========================================"
		 "======================\r\n");
	edprint_scaled_stat("V90Equalizer: linearEquMmxConversionFactor = "
			    "%c%d.%05de8\r\n", linearEquMmxConversionFactor,
			    1.0e-8 * linearEquMmxConversionFactor, 1.0e5f);
	edprint_int_stat("V90Equalizer: linearEquMmxOutputConversionFactor = "
			 "%c%d.%03d\r\n", linearEquMmxOutputConversionFactor);
	edprintf("V90Equalizer: linearEquBetaMmx = %d, beta exponent = %d\r\n",
		 linearEquMmxBeta, linearEquMmxShift);
	edprintf("========================================"
		 "======================\r\n");
	edprint_stat("V90Equalizer: float LE coeffs sum = %c%d.%04d\r\n", fsum,
		     1.0e4f);
	edprint_stat("V90Equalizer: float LE coeffs abs sum = %c%d.%04d\r\n",
		     fabsSum, 1.0e4f);
	edprintf("V90Equalizer: short high LE coeffs sum = %d\r\n", sumHi);
	edprintf("V90Equalizer: short high LE coeffs abs sum = %d\r\n",
		 absSumHi);
	edprintf("V90Equalizer: short high LE coeffs min value = %d\r\n", minHi);
	edprintf("V90Equalizer: short high LE coeffs max value = %d\r\n", maxHi);

	/*
	 * The history, `word_1c` entries of it, with no scaling and a
	 * two-byte conversion.  `(short)__builtin_abs(s)` is the object's
	 * `cltd; xor; sub; cwtl`, and the truncation is what makes the
	 * magnitude of -32768 negative again.
	 */
	maxHist = 0;
	minHist = 0x8000;

	for (i = 0; i < word_1c; i++) {
		short s = (short)array_18[i];
		short a;

		array_ecAligned[i] = s;
		a = (short)__builtin_abs(s);
		if (a > maxHist)
			maxHist = a;
		if (a < minHist)
			minHist = a;
	}

	edprintf("V90Equalizer: Max LE History = %d\r\n", maxHist);
	edprintf("V90Equalizer: Min LE History = %d\r\n", minHist);

	/* -------------------------------------- the decision-feedback half */

	conv = (1.0f / (long double)maxDfeCoefValue) * 1073741824.0f;

	/*
	 * +0x20 IS PARKED IN +0xf8 FOR THE DURATION OF THE FIXED-POINT MODE,
	 * and `restoreEqualizerToFloat` copies it straight back.  The object
	 * does it here, in the middle of the DFE half's scaling, because that
	 * is where GCC scheduled a copy with no floating-point dependency.
	 */
	word_20Saved = word_20;

	dfeMmxConversionFactor = (float)conv;
	dfeMmxOutputConversionFactor = (int)((1.0f / 65536.0f) * conv);

	if (dfeBeta < 0.0f || dfeBeta > 0.0f) {
		int shift = (int)(x87_log10(__builtin_fabsl(
					(1.0f / ((long double)dfeBeta
						 * 1048576.0f))
					* (long double)maxDfeCoefValue))
				  / x87_log10((long double)2.0f));

		dfeMmxShift = shift;
		dfeMmxBeta = (int)((long double)dfeBeta * conv
				   * (long double)one_shifted_by(shift));
	} else {
		dfeMmxShift = 0;
		dfeMmxBeta = 0;
	}

	for (i = 0; i < dfeLength; i++) {
		int v = (int)(dfeCoefs[i] * conv);

		array_118Aligned[i] = (short)v;
		dfeMmxCoefsAligned[i] = (short)(v >> 16);
	}

	sumHi = 0;
	absSumHi = 0;
	maxHi = 0;
	minHi = 0x10000;
	fsum = 0.0f;
	fabsSum = 0.0f;

	for (i = 0; i < dfeLength; i++) {
		int hi = dfeMmxCoefsAligned[i];
		int a = __builtin_abs(hi);

		sumHi += hi;
		if (a > maxHi)
			maxHi = a;
		if (a < minHi)
			minHi = a;
		absSumHi += a;
		fsum += dfeCoefs[i];
		fabsSum += __builtin_fabsf(dfeCoefs[i]);
	}

	edprintf("========================================"
		 "======================\r\n");
	edprint_scaled_stat("V90Equalizer: dfeMmxConversionFactor = "
			    "%c%d.%05de8\r\n", dfeMmxConversionFactor,
			    1.0e-8 * dfeMmxConversionFactor, 1.0e5f);
	edprint_int_stat("V90Equalizer: dfeMmxOutputConversionFactor = "
			 "%c%d.%03d\r\n", dfeMmxOutputConversionFactor);
	edprintf("V90Equalizer: dfeBetaMmx = %d, beta exponent = %d\r\n",
		 dfeMmxBeta, dfeMmxShift);
	edprintf("========================================"
		 "======================\r\n");
	edprint_stat("V90Equalizer: float dfe coeffs sum = %c%d.%04d\r\n", fsum,
		     1.0e4f);
	edprint_stat("V90Equalizer: float dfe coeffs abs sum = %c%d.%04d\r\n",
		     fabsSum, 1.0e4f);
	edprintf("V90Equalizer: short high dfe coeffs sum = %d\r\n", sumHi);
	edprintf("V90Equalizer: short high dfe coeffs abs sum = %d\r\n",
		 absSumHi);
	edprintf("V90Equalizer: short high dfe coeffs min value = %d\r\n",
		 minHi);
	edprintf("V90Equalizer: short high dfe coeffs max value = %d\r\n",
		 maxHi);

	/* The DFE's history is `dfeLength` long and lives in +0x12c. */
	maxHist = 0;
	minHist = 0x8000;

	for (i = 0; i < dfeLength; i++) {
		short s = (short)array_44[i];
		short a;

		array_12cAligned[i] = s;
		a = (short)__builtin_abs(s);
		if (a > maxHist)
			maxHist = a;
		if (a < minHist)
			minHist = a;
	}

	edprintf("V90Equalizer: Max dfe History = %d\r\n", maxHist);
	edprintf("V90Equalizer: Min dfe History = %d\r\n", minHist);

	/* The mode goes on BEFORE the line that announces it. */
	mmxMode = 1;
	edprintf("V90Equalizer: LE & DFE running in MMX mode.\r\n");
}

/* ============================================================ state entries */

/*
 * enterRRN and enterFPE -- the same 387 bytes with the state constant and the
 * entry string swapped, the way `setLinearEquBeta` and `setDfeBeta` are the
 * same 350.  Written out twice for the same reason.
 *
 * THEY RETURN AN int AND THE OTHER `enter*` MEMBERS DO NOT.  `%edi` is zeroed
 * at entry, made 1 on exactly one path, and moved to `%eax` at both returns;
 * the 1 means "the equaliser was taken out of fixed-point mode", which a
 * caller cannot see from the state word.  Finding 2134.
 *
 * NEITHER TOUCHES `stateCount`.  `enterPhase3` and `enterChannelVerification`
 * zero +0x64 in the instruction after they write +0x60; these two write +0x60
 * and leave +0x64 alone.
 *
 * THREE INDEPENDENT CONDITIONS, NOT A CHAIN.  The German-PBX arm, the EIA-6
 * freeze and the fixed-point restore each test their own thing and any
 * combination can run.  GCC cross-jumps the second and third out of both arms
 * of the first, which is why `isV90WithEia6` is called from two sites and the
 * `mmxMode` test from three.
 *
 * `mmxMode` IS READ ONCE FOR THE ARRAY CLEAR AND AGAIN FOR THE RESTORE.  The
 * first read is cached across the two clearing loops (`mov 0xb0(%esi),%ebx`
 * before the German-PBX test, used after it); the second is reloaded because
 * `setDfeBeta` and `setLinearEquBeta` intervene.  Neither of them writes
 * +0xb0, so the two agree -- it is one field read twice in the source.
 */
int
V90Equalizer::enterRRN()
{
	unsigned int i, n;

	if (state == V90EQU_STATE_RRN)
		return 0;

	edprintf("V90Equalizer: enter RRN state\r\n");
	state = V90EQU_STATE_RRN;

	if (spectralVerifier->word_28 == 2) {
		for (i = 0; i < dfeLength; i++)
			dfeCoefs[i] = 0;

		if (mmxMode != 0) {
			n = dfeLength + 8;
			for (i = 0; i < n; i++) {
				dfeMmxCoefs[i] = 0;
				array_118[i] = 0;
			}
		}

		edprintf("V90Equalizer: Dfe coefs zeroed (GERMAN_PBX) !!\r\n");
		setDfeBeta(params->GERMAN_PBX_DFE_TRN2D_FAST_BETA);
	}

	if (preFilter->isV90WithEia6()) {
		setLinearEquBeta(0.0f);
		setDfeBeta(0.0f);
		edprintf("V90Equalizer: Dfe & Linear EQU coefs freezed "
			 "(RRN : EIA6 )!!\r\n");
	}

	if (mmxMode != 0) {
		edprintf("V90Equalizer: restoring Equalizer to float...\r\n");
		restoreEqualizerToFloat();
		mmxMode = 0;
		return 1;
	}

	return 0;
}

/*
 * One filter's coefficients summarised: the largest and smallest magnitude,
 * the signed sum and the sum of magnitudes.
 *
 * `enterPhase4` runs this twice, once over `linearEquCoefs` and once over
 * `dfeCoefs`, and the two are instruction for instruction the same with the
 * length, the array and the two destination slots moved -- the relationship
 * the two step-size setters have.  Written once here; the object has it
 * twice, inline.
 *
 * THE FIRST COEFFICIENT IS READ UNCONDITIONALLY AND IS NOT IN EITHER SUM.
 * `flds (%edx)` happens before the `cmp $1,%eax; jbe` that guards the loop,
 * so a zero-length filter reads `coefs[0]` anyway; and the loop runs from 1,
 * so `coefs[0]` seeds the maximum and the minimum and contributes to neither
 * total.  Both are the object's; docs/deviations.md D325.
 *
 * THE MAXIMUM AND MINIMUM ARE OF THE MAGNITUDE and the sums are not: `fabs`
 * is applied before both comparisons and before the second accumulator, and
 * the first accumulator takes the coefficient as it stands.
 */
static void
summarise_coefs(const float *coefs, unsigned int len, float *maxOut,
		float *minOut, float *sumOut, float *absSumOut)
{
	float mx, mn, sum, absSum;
	unsigned int i;

	sum = 0;
	absSum = 0;

	mx = mn = __builtin_fabsf(coefs[0]);
	*maxOut = mx;
	*minOut = mn;

	for (i = 1; i < len; i++) {
		float a = __builtin_fabsf(coefs[i]);

		if (a > mx) {
			mx = a;
			*maxOut = mx;
		}
		if (a < mn) {
			mn = a;
			*minOut = mn;
		}
		sum += coefs[i];
		absSum += __builtin_fabsf(coefs[i]);
	}

	*sumOut = sum;
	*absSumOut = absSum;
}

/*
 * enterPhase4 -- freeze the equaliser, freeze the resampler's band-limited
 * loop, and dump both filters.
 *
 * Four things happen and three of them are diagnostics:
 *
 *  1. the state, both step sizes to zero, and the resampler's BLL state saved
 *     into +0x04 and set to `V90_BLL_FROZEN` with a one-sample count;
 *  2. the timing offset printed;
 *  3. when the spectral verifier's +0x28 is 2 -- the German-PBX arm
 *     `enterRRN` and `enterFPE` also have -- the DFE coefficients zeroed;
 *  4. the mean-error diagnostics reset, and both filters summarised.
 *
 * THE TIMING OFFSET IS FETCHED FOUR TIMES.  `getTimingOffsetPPM` is called
 * once for the fractional digits' minuend, once for their subtrahend, once
 * for the integer part and once for the sign -- four `call`s, in the order
 * GCC evaluates the three arguments right to left.  It is written out four
 * times below because that is what the object does; a temporary would be one
 * call and would read the same value, but it is not what was compiled.
 *
 * `V90Resampler` derives from `ResamplerTiming`, which derives from
 * `ResamplerTimingOffset`, so `resampler->getTimingOffsetPPM()` is the
 * `_ZNK21ResamplerTimingOffset18getTimingOffsetPPMEv` the object calls with
 * the resampler pointer unadjusted -- offset zero of a single-inheritance
 * chain.
 *
 * THE UNCONDITIONAL EARLY-OUT IS THE `enter*` FAMILY'S.  `cmpl $0x2,0x60;
 * je <return>`, and `stateCount` is not touched -- the same as `enterRRN` and
 * `enterFPE` and unlike `enterPhase3`.
 */
void
V90Equalizer::enterPhase4()
{
	float sum, absSum, ppm, ppmFrac;
	unsigned int i, n;

	if (state == V90EQU_STATE_PHASE4)
		return;

	edprintf("V90Equalizer: enter Phase 4\r\n");
	state = V90EQU_STATE_PHASE4;

	setLinearEquBeta(0.0f);
	setDfeBeta(0.0f);

	savedBllState = resampler->bllState;
	resampler->setBllState(V90_BLL_FROZEN, 1);

	/*
	 * THE FRACTIONAL PART IS ROUNDED TO SINGLE PRECISION TWICE, AND IT
	 * MATTERS.  The object stores the first call's result to a 4-byte
	 * slot, calls again, subtracts, and stores the DIFFERENCE to the same
	 * 4-byte slot before scaling it:
	 *
	 *     36b96:  d9 5c 24 38     fstps 0x38(%esp)      ; t
	 *     36bcc:  d8 6c 24 38     fsubrs 0x38(%esp)     ; t - (int)t
	 *     36bd0:  d9 5c 24 38     fstps 0x38(%esp)      ; rounded again
	 *     36bd4:  d9 05 ..        flds  1e4
	 *     36bda:  d8 4c 24 38     fmuls 0x38(%esp)
	 *
	 * `getTimingOffsetPPM` returns its result in st(0) at EXTENDED
	 * precision -- it is `1e6f * timingOffset / ppmScale` and nothing
	 * rounds it on the way out -- so keeping the difference in a register
	 * changes the fourth decimal digit at the truncation, which is
	 * exactly what the printed `%04d` carries.  Two `float` locals is what
	 * the object has and what makes the transcript agree; measured, not
	 * assumed (the first spelling here used `long double` throughout and
	 * disagreed on every non-zero offset).
	 *
	 * Hoisting them out of the argument list changes no order: GCC
	 * evaluates the three arguments right to left, so the two calls the
	 * fractional digits need come first either way.
	 */
	ppm = resampler->getTimingOffsetPPM();
	ppmFrac = ppm - (float)(int)resampler->getTimingOffsetPPM();

	edprintf("V90Equalizer: timing offset on freeze (phase4) = "
		 "%c%d.%04d\r\n",
		 !(resampler->getTimingOffsetPPM() <= 0.0f) ? '+' : '-',
		 (int)__builtin_fabsl((long double)
				      resampler->getTimingOffsetPPM()),
		 __builtin_abs((int)(1.0e4f * ppmFrac)));

	if (spectralVerifier->word_28 == 2) {
		for (i = 0; i < dfeLength; i++)
			dfeCoefs[i] = 0;

		if (mmxMode != 0) {
			n = dfeLength + 8;
			for (i = 0; i < n; i++) {
				dfeMmxCoefs[i] = 0;
				array_118[i] = 0;
			}
		}

		edprintf("V90Equalizer: Dfe coefs zeroed!!\r\n");
	}

	meanErrorFull = 0;
	meanErrorCount = 0;

	summarise_coefs(linearEquCoefs, linearEquLength, &maxLeCoefValue,
			&minLeCoefValue, &sum, &absSum);

	edprintf("=======================================================\r\n");
	edprintf("Linear Equalizer:\r\n");
	edprint_stat("minLeCoefValue  = %c%d.%010d\r\n", minLeCoefValue,
		     1.0e10f);
	edprint_stat("maxLeCoefValue  = %c%d.%06d\r\n", maxLeCoefValue, 1.0e6);
	edprint_stat("coefs sum  = %c%d.%06d\r\n", sum, 1.0e6);
	edprint_stat("abs coefs sum  = %c%d.%06d\r\n", absSum, 1.0e6);
	edprintf("=======================================================\r\n");

	summarise_coefs(dfeCoefs, dfeLength, &maxDfeCoefValue,
			&minDfeCoefValue, &sum, &absSum);

	edprintf("DFE:\r\n");
	edprint_stat("minDfeCoefValue  = %c%d.%010d\r\n", minDfeCoefValue,
		     1.0e10f);
	edprint_stat("maxDfeCoefValue  = %c%d.%06d\r\n", maxDfeCoefValue,
		     1.0e6);
	edprint_stat("coefs sum  = %c%d.%06d\r\n", sum, 1.0e6);
	edprint_stat("abs coefs sum  = %c%d.%06d\r\n", absSum, 1.0e6);
	edprintf("=======================================================\r\n");
}

/*
 * enterFPE -- `enterRRN` with state 5 and its own entry string.  Everything
 * after the first `edprintf` is instruction for instruction the same, down to
 * the redundant `mmxMode = 0` after `restoreEqualizerToFloat` has already
 * cleared it.
 */
int
V90Equalizer::enterFPE()
{
	unsigned int i, n;

	if (state == V90EQU_STATE_FPE)
		return 0;

	edprintf("V90Equalizer: enter FPE state\r\n");
	state = V90EQU_STATE_FPE;

	if (spectralVerifier->word_28 == 2) {
		for (i = 0; i < dfeLength; i++)
			dfeCoefs[i] = 0;

		if (mmxMode != 0) {
			n = dfeLength + 8;
			for (i = 0; i < n; i++) {
				dfeMmxCoefs[i] = 0;
				array_118[i] = 0;
			}
		}

		edprintf("V90Equalizer: Dfe coefs zeroed (GERMAN_PBX) !!\r\n");
		setDfeBeta(params->GERMAN_PBX_DFE_TRN2D_FAST_BETA);
	}

	if (preFilter->isV90WithEia6()) {
		setLinearEquBeta(0.0f);
		setDfeBeta(0.0f);
		edprintf("V90Equalizer: Dfe & Linear EQU coefs freezed "
			 "(RRN : EIA6 )!!\r\n");
	}

	if (mmxMode != 0) {
		edprintf("V90Equalizer: restoring Equalizer to float...\r\n");
		restoreEqualizerToFloat();
		mmxMode = 0;
		return 1;
	}

	return 0;
}

/* =============================================== the mean-error diagnostic */

/*
 * calcMeanErrorStatistics -- mean, standard deviation, variance, minimum and
 * maximum of the 300-float buffer at +0x98, printed, with the standard
 * deviation returned.
 *
 * THE EARLY EXIT RETURNS AN UNINITIALISED STACK SLOT.  `flds 0x20(%esp); ret`
 * at 0x38d34 is the ONE return point, and 0x20(%esp) is written only by the
 * `Std<float>` call the early exit jumps over.  Transcribed as the
 * uninitialised local it is; docs/deviations.md D324, and the test compares
 * everything about that path except the value.
 *
 * THE LENGTH IS ONE EXPRESSION AND THE OBJECT TESTS `meanErrorFull` TWICE.
 * `if (count == 0 && full == 0) return std;` then `len = full ? 300 : count`
 * is exactly the object's four-way graph: the second test is threaded away on
 * the path where the first already proved `full` non-zero, which is why
 * 0x38d40 reloads +0xa0 and 0x388e5 does not.
 *
 * THE MINIMUM AND MAXIMUM ARE TRACKED IN THE MEMBERS THEMSELVES.  `fsts` is a
 * store that does not pop, so the object keeps the running pair on the x87
 * stack and writes it out at every update; that is what a plain assignment to
 * a member compiles to.  Both start at `meanErrorEnergy[0]` and the loop runs
 * from 1.  A NaN in the buffer moves the MINIMUM and not the maximum: `jbe`
 * skips the maximum on unordered and `jae` does not skip the minimum.
 */
float
V90Equalizer::calcMeanErrorStatistics()
{
	/*
	 * Deliberately uninitialised, because 0x20(%esp) is: on the early
	 * exit the object returns whatever the frame happened to hold.  See
	 * the comment above and D324.
	 */
	float std;
	float var;
	unsigned int len, i;

	if (meanErrorCount == 0 && meanErrorFull == 0)
		return std;

	len = (meanErrorFull != 0) ? V90EQU_MEAN_ERROR_LEN : meanErrorCount;

	meanErrorEnergyMean = mean(meanErrorEnergy, len);
	std = Std(meanErrorEnergy, len);
	var = Var(meanErrorEnergy, len);

	meanErrorEnergyMax = meanErrorEnergy[0];
	meanErrorEnergyMin = meanErrorEnergy[0];
	for (i = 1; i < len; i++) {
		if (meanErrorEnergy[i] > meanErrorEnergyMax)
			meanErrorEnergyMax = meanErrorEnergy[i];
		if (meanErrorEnergy[i] < meanErrorEnergyMin)
			meanErrorEnergyMin = meanErrorEnergy[i];
	}

	edprintf("##########################################"
		 "##########\r\n");
	edprintf("V90Equalizer: meanErrorEnergy Debug:\r\n");
	edprintf("V90Equalizer: calculated over %d mean errors\r\n", len);
	edprintf("--------------------------------------------\r\n");
	edprint_stat("V90Equalizer: meanErrorEnergy mean  = %c%d.%06d\r\n",
		     meanErrorEnergyMean, 1.0e6);
	edprint_stat("V90Equalizer: current meanErrorEnergy  = %c%d.%06d\r\n",
		     meanErrorEnergyCurrent, 1.0e6);
	edprintf("--------------------------------------------\r\n");
	edprint_stat("V90Equalizer: meanErrorEnergy Std  = %c%d.%06d\r\n",
		     std, 1.0e6);
	edprint_stat("V90Equalizer: meanErrorEnergy Variance  = %c%d.%06d\r\n",
		     var, 1.0e6);
	edprintf("--------------------------------------------\r\n");
	edprint_stat("V90Equalizer: meanErrorEnergy min value  = "
		     "%c%d.%06d\r\n", meanErrorEnergyMin, 1.0e6);
	edprint_stat("V90Equalizer: meanErrorEnergy max value  = "
		     "%c%d.%06d\r\n", meanErrorEnergyMax, 1.0e6);
	edprintf("##########################################"
		 "##########\r\n");

	return std;
}

/*
 * The three that are one `ret`.  See the header.
 */
void
V90Equalizer::printCoefsToFile() const
{
}

void
V90Equalizer::loadCoefsFromFile()
{
}

void
V90Equalizer::printEquStuff()
{
}
