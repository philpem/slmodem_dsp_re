/*
 * V90SpectralShapingFilter.cpp -- constructing the shaping filter.
 *
 * Reconstructed from dsplibs.o.  All five members of the class.
 * `include/dsplib/V90SpectralShapingFilter.h` carries the object map, the
 * evidence for it and the recurrence the last two share.
 *
 * THE RETURN IS NOT ROUNDED, AND THAT WAS A DEFECT HERE UNTIL `advanceTrellis`
 * WAS WRITTEN.  This function used to return `float` and end `return (float)
 * sum;`, which GCC narrows through memory -- `fstps`/`flds` -- where the object
 * ends with three bare `fstp %st(1)` and a `ret` (0x33270) and hands the caller
 * the full 64-bit significand.  The question was left open in this comment on
 * the ground that no caller existed yet.  A caller exists now, it compares
 * before it stores, and the rounding changed which trellis candidate it chose;
 * see the header, and finding F5854 for how the tie arises and why the suite
 * that passed over this could not have caught it.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215).
 *
 * IT DOES NOT CALL `reset()` OR `setFilterCoeff()`.  Both are ordinary global
 * members and the object contains no call at all here, so the nine stores are
 * written out rather than delegated -- which is what -O2 without
 * `-finline-functions` gives for a member the compiler may not inline.
 *
 * THE COEFFICIENTS ARE CLEARED HIGH WORD FIRST, +0x0c then +0x08, +0x04,
 * +0x00, and the state low word first.  Right-to-left is what a chained
 * assignment evaluates to, so the coefficients are written as one here; it is
 * a store order of nine zeroes into nine distinct words, so no observer can
 * tell, and the object's order is recorded rather than relied on.
 */

#include <stddef.h>

#include "dsplib/V90SpectralShapingFilter.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SSF_OFF(field, off, tag) \
	typedef char v90ssf_off_##tag[ \
	    ((int)__builtin_offsetof(V90SpectralShapingFilter, field) \
	     == (off)) ? 1 : -1]

V90SSF_OFF(coeff,       0x00, coeff);
V90SSF_OFF(state,       0x10, state);
V90SSF_OFF(blockLength, 0x20, blocklength);
typedef char v90ssf_size[(sizeof(V90SpectralShapingFilter) == 0x24) ? 1 : -1];
#endif

V90SpectralShapingFilter::V90SpectralShapingFilter()
{
	coeff[0] = coeff[1] = coeff[2] = coeff[3] = 0.0f;

	blockLength = 2;

	state[0] = 0.0f;
	state[1] = 0.0f;
	state[2] = 0.0f;
	state[3] = 0.0f;
}

/*
 * The four arguments into the four coefficient words IN ORDER.  The object
 * copies them as integers, four `mov`s and no x87, because a stack float
 * argument copied to a float member needs no arithmetic; ours converts
 * through the coprocessor, and the two differ for exactly one input class --
 * a signalling NaN, which x87 quietens and `mov` does not.  That is the same
 * boundary V90SdDetector's constructor sits on (finding F1242), so it is
 * measured the same way: the test sweeps bit patterns and leaves signalling
 * NaNs out.
 */
void
V90SpectralShapingFilter::setFilterCoeff(float c0, float c1, float c2,
					 float c3)
{
	coeff[0] = c0;
	coeff[1] = c1;
	coeff[2] = c2;
	coeff[3] = c3;
}

/*
 * Four zeroes into the four state words and nothing else -- not the
 * coefficients, not the block length.  It is the only member that clears the
 * energy accumulator at state[3], which `progress` otherwise adds to for ever.
 *
 * WRITTEN AS ONE CHAINED ASSIGNMENT rather than as the constructor's four
 * statements, and deliberately: the two would otherwise be the same four
 * lines in one file, every `find` string over either would match twice, and
 * `mutate.py` would report the suite UNUSABLE -- which does not fail a run
 * (finding F1264).  The chain evaluates right to left where the object stores
 * +0x10 first, and that is a distinction no observer can make: four zeroes
 * into four distinct words with no call and no aliasing between them.
 */
void
V90SpectralShapingFilter::reset()
{
	state[0] = state[1] = state[2] = state[3] = 0.0f;
}

/*
 * One block of `blockLength` samples through the two sections, accumulating
 * output energy.  The header carries the recurrence; three things about the
 * SHAPE are this function's own and are what a natural translation loses:
 *
 * THE ZERO-LENGTH CASE WRITES NOTHING.  The count is read and tested before
 * the four state words are even loaded (`cmp $0x0,%eax; jbe` to the epilogue),
 * so a zero-length block leaves the object byte-for-byte alone -- including
 * whatever non-float bits happen to be in it.  A `for` loop would load and
 * store four identical values and be invisible except on an object that was
 * never reset, which is why the test never zeroes one.
 *
 * THE STATE ROUNDS TO FLOAT EXACTLY FOUR TIMES, at the four `fstps` after
 * the loop.  Everything between is x87 registers at 64-bit significand: the
 * accumulator is never rounded per sample, and neither is either section's
 * output before it is fed back.  `long double` locals say that in C; leaving
 * them `float` would be a promise the compiler is free to keep or break.
 *
 * THE INPUT IS READ WITH `filds`, a 16-bit integer load straight to the
 * coprocessor, so the samples are `short` and are never rounded either.
 */
void
V90SpectralShapingFilter::progress(const short *in)
{
	unsigned int left = blockLength;
	long double prevIn, prevMid, prevOut, energy;
	long double b0, b1, b2, b3;

	if (left == 0)
		return;

	prevIn  = state[0];
	prevMid = state[1];
	prevOut = state[2];
	energy  = state[3];

	b0 = coeff[0];
	b1 = coeff[1];
	b2 = coeff[2];
	b3 = coeff[3];

	do {
		long double sample = *in++;
		long double mid = (sample - prevIn * b2) + prevMid * b0;
		long double out = (mid - prevMid * b3) + prevOut * b1;

		energy = energy + out * out;

		prevIn = sample;
		prevMid = mid;
		prevOut = out;
	} while (--left != 0);

	/* The object's store order: +0x10, +0x14, +0x1c, then +0x18. */
	state[0] = (float)prevIn;
	state[1] = (float)prevMid;
	state[3] = (float)energy;
	state[2] = (float)prevOut;
}

/*
 * `blocks` blocks of `blockLength` samples, consecutively, and the value the
 * accumulator would have reached -- WITHOUT storing any of it.  The method is
 * `const` and the object really is untouched: the four state words live in
 * x87 registers for the whole run and only the accumulator leaves, as the
 * return value.
 *
 * SO THE ARITHMETIC IS `progress`'s AND THE ENCODING IS NOT, which is what
 * makes the pair worth reading together.  `progress` forms `x - prevIn * b2`
 * with a POPPING `de e2`, whose printed mnemonic is its own opposite (finding
 * F245); `getMetric` forms the same difference with `d8 ef`, a register form
 * objdump prints correctly.  The two agree, so the popping forms in
 * `progress` are read right -- finding F1400.
 *
 * `blocks == 0` returns state[3] unchanged, having done nothing; a zero
 * `blockLength` makes each of the `blocks` iterations a no-op, which the
 * object spells as a four-register rotation that comes back to the identity
 * after two passes and is not modelled here because it computes nothing.
 */
long double
V90SpectralShapingFilter::getMetric(const short *in, unsigned int blocks) const
{
	long double lastIn, lastMid, lastOut, sum;
	long double a0, a1, a2, a3;

	lastIn  = state[0];
	lastMid = state[1];
	lastOut = state[2];
	sum     = state[3];

	if (blocks == 0)
		return sum;

	a0 = coeff[0];
	a1 = coeff[1];
	a2 = coeff[2];
	a3 = coeff[3];

	do {
		unsigned int n = blockLength;

		while (n != 0) {
			long double v = *in++;
			long double m = (v - lastIn * a2) + lastMid * a0;
			long double w = (m - lastMid * a3) + lastOut * a1;

			sum = sum + w * w;

			lastIn = v;
			lastMid = m;
			lastOut = w;
			n--;
		}
	} while (--blocks != 0);

	return sum;
}
