/*
 * ResamplerTiming.cpp -- the timing-recovery layer of the resampler chain.
 *
 * Reconstructed from dsplibs.o.  dsplib/ResamplerTiming.h carries the object
 * map, the corrected field groupings and why `SdHalfBaudDft` returns void;
 * dsplib/Resampler.h carries the chain and the four vtables.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215).
 *
 * WHAT THE LOOP IS.  `timingCorrection` runs a two-pole resonator
 *
 *     y[n] = normBPFhBaudB0coef * x[n] - 0.9604 * y[n-2]
 *
 * whose poles sit at z = +/-0.98j, i.e. at Fs/4.  `V90Equalizer::process`
 * feeds `SdHalfBaudDft` two consecutive equalizer outputs per symbol, so the
 * sample rate is two per baud and Fs/4 is baud/2 -- "half baud", which is
 * what every name in this group says.  0.9604 is 0.98 * 0.98 and 0.03981 is
 * the nominal b0; `adjustHalfBaudBpfGain` scales that b0 by 1..2 so the
 * resonator's output level tracks the measured baud/2 energy.
 *
 * THE `DE`/`DC` PRINTING TRAP WAS SETTLED BY EXECUTION, NOT BY READING.  This
 * function group is full of popping subtracts, and objdump prints them as
 * their own opposite (finding F245).  The bytes were run with known operands:
 *
 *     DE E0+i  objdump `fsubp  %st,%st(i)`  ->  ST(i) = ST(0) - ST(i)
 *     DC E0+i  objdump `fsub   %st,%st(i)`  ->  ST(i) = ST(0) - ST(i)
 *     DC E8+i  objdump `fsubr  %st,%st(i)`  ->  ST(i) = ST(i) - ST(0)
 *     DE F0+i  objdump `fdivp`              ->  ST(i) = ST(0) / ST(i)
 *
 * so the `DC` NON-popping register forms are swapped in the output too, not
 * only the `DE` pops.  The DSP sense corroborates it independently: reading
 * `de e2` at .text+0x3580b the other way gives `y = 0.9604*y[n-2] - b0*x[n]`,
 * an unstable sign, and this way gives a stable resonator at Fs/4.
 *
 * THE CONSTRUCTOR'S BODY IS `reset(...)` INLINED -- the stores after the base
 * constructor returns are `reset(unsigned)`'s, store for store and in the
 * same order, with the SINGLE call to `ResamplerTimingOffset::reset()` that
 * `reset(unsigned)` opens with, and then the same `setTimingOffset` call.
 * That is the same one-level-of-inlining pattern the other three constructors
 * in the chain show; behaviour is identical whether the compiler takes it or
 * not.  THE ARGUMENT WRITTEN AT THAT CALL IS UNOBSERVABLE -- `reset` does not
 * read it -- so the 0 below is a placeholder and no test can tell it from any
 * other value.  It is recorded as such rather than dressed up as measured.
 */

#include <stddef.h>
#include <math.h>

#include "dsplib/debug.h"
#include "dsplib/ResamplerTiming.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define RT_OFF(field, off, tag) \
	typedef char rt_off_##tag[ \
	    ((int)__builtin_offsetof(ResamplerTiming, field) == (off)) \
	    ? 1 : -1]

RT_OFF(bllK1,			0x4c, bllk1);
RT_OFF(bllK2,			0x50, bllk2);
RT_OFF(lastHalfBaudErr,		0x54, lasterr);
RT_OFF(unnamed_58,		0x58, unnamed58);
RT_OFF(lastPhaseAdj,		0x5c, lastadj);
RT_OFF(halfBaudStep,		0x60, halfbaudstep);
RT_OFF(bpfSq1,			0x68, bpfsq1);
RT_OFF(bpfSq2,			0x6c, bpfsq2);
RT_OFF(bpfZ1,			0x70, bpfz1);
RT_OFF(bpfZ2,			0x74, bpfz2);
RT_OFF(errZ1,			0x78, errz1);
RT_OFF(dftCount,		0x7c, dftcount);
RT_OFF(dftMag,			0x80, dftmag);
RT_OFF(dftRe,			0x84, dftre);
RT_OFF(dftIm,			0x88, dftim);
RT_OFF(dftDone,			0x8c, dftdone);
RT_OFF(normBPFhBaudB0coef,	0x90, normb0);
typedef char rt_size[(sizeof(ResamplerTiming) == 0x94) ? 1 : -1];
#endif

/*
 * The float-as-%c%d.%03d idiom, duplicated verbatim from
 * src/pump/v90/V90Phase2Info.cpp, where finding F256 measured that the ORDER
 * of the subtraction inside `frac_of` is unobservable through the abs() on
 * the result.  The three copies in `adjustHalfBaudBpfGain` do `(int)v - v`,
 * which is the order written there and the order the object uses here.
 */
static char
sign_of(float v)
{
	return (0.0f < v) ? '+' : '-';
}

static int
whole_of(float v)
{
	return (int)fabsf(v);
}

static int
frac_of(float v, float scale)
{
	long double x = v;
	long double d = (int)v - x;
	int n = (int)(d * scale);

	return (n < 0) ? -n : n;
}

ResamplerTiming::ResamplerTiming(unsigned int nPhases, float scale,
				 unsigned int nTaps, float cutoff, float ppm,
				 unsigned int minHistory)
	: ResamplerTimingOffset(nPhases, scale, nTaps, cutoff, ppm, minHistory)
{
	reset(0);
	setTimingOffset(ppm);
}

ResamplerTiming::ResamplerTiming(unsigned int nPhases, float scale,
				 unsigned int nTaps, float *bank, float ppm,
				 unsigned int minHistory)
	: ResamplerTimingOffset(nPhases, scale, nTaps, bank, ppm, minHistory)
{
	reset(0);
	setTimingOffset(ppm);
}

ResamplerTiming::~ResamplerTiming()
{
}

/*
 * `0x3d230fd0` is 0.03981f exactly -- the literal round-trips to those four
 * bytes -- and it is the resonator's NOMINAL b0, so a fresh object is already
 * at `adjustHalfBaudBpfGain`'s normFactor of 1.0.
 *
 * This is also the only thing that clears `dftDone`.
 */
void
ResamplerTiming::reset(unsigned int)
{
	ResamplerTimingOffset::reset();

	halfBaudStep = 0;

	bllK1 = 0;
	bllK2 = 0;
	lastHalfBaudErr = 0;
	unnamed_58 = 0;
	lastPhaseAdj = 0;

	bpfSq1 = 0;
	bpfSq2 = 0;
	bpfZ1 = 0;
	bpfZ2 = 0;
	errZ1 = 0;

	resetSdHalfBaudDft();

	dftDone = 0;
	normBPFhBaudB0coef = 0.03981f;
}

/*
 * The four fields this clears -- and it clears nothing else -- are what makes
 * +0x7c..+0x88 one group.  IT DOES NOT CLEAR `dftDone`, which is the
 * asymmetry worth knowing about: restarting the accumulation does not
 * un-complete a completed DFT.
 */
void
ResamplerTiming::resetSdHalfBaudDft()
{
	dftCount = 0;
	dftMag = 0;
	dftRe = 0;
	dftIm = 0;
}

/*
 * One sample into a 256-point DFT bin at Fs/4.
 *
 * The quadrature pattern is cos = +1, 0, -1, 0 on `dftRe` and
 * sin = 0, -1, 0, +1 on `dftIm`, taken straight off the four arms.  `and
 * $0x3` on the count with no sign fixup is what says `dftCount` is unsigned.
 *
 * `cmp $0x100` is on the INCREMENTED value, so the magnitude lands after the
 * 256th sample and `dftDone` latches there.  The object squares and adds on
 * the x87 stack and rounds once, at the `fstps`; no local and no
 * -ffloat-store.  `sqrt` compiles to a bare `fsqrt` here exactly as
 * it does in `V90Resampler::getTimingHistoryStd`.
 */
void
ResamplerTiming::SdHalfBaudDft(float v)
{
	if (dftDone)
		return;

	switch (dftCount % 4) {
	case 0:
		dftRe += v;
		break;
	case 1:
		dftIm -= v;
		break;
	case 2:
		dftRe -= v;
		break;
	case 3:
		dftIm += v;
		break;
	}

	dftCount++;

	if (dftCount == 256) {
		dftDone = 1;
		dftMag = sqrt(dftRe * dftRe + dftIm * dftIm);
	}
}

/*
 * The base's per-output-sample hook, vtable slot +0x14.
 *
 * PRECISION.  There is no `fstps`/`flds` to a stack slot anywhere in this
 * function: every store is to a member, and each of `y`, `sq` and `e` is used
 * again from the 80-bit register AFTER its member store has rounded it.  The
 * `float` locals below are therefore the right spelling -- with -mfpmath=387
 * and no -ffloat-store GCC keeps them in x87 registers, which is what the
 * object does.  -ffloat-store would break this function.
 */
void
ResamplerTiming::timingCorrection(float v)
{
	float y, sq, e;

	/*
	 * The resonator.  `bpfZ2` is READ -- as the `fmuls 0x74` memory
	 * operand at .text+0x357f5 -- before it is overwritten by the plain
	 * float copy at 0x357fa, so this statement order is forced.
	 */
	y = normBPFhBaudB0coef * v - 0.9604f * bpfZ2;
	bpfZ2 = bpfZ1;
	bpfZ1 = y;

	/*
	 * A second difference of the squared resonator output.  The grouping
	 * is the object's -- `(K1*sq - K2*bpfSq1) + K1*bpfSq2`, which is what
	 * left-to-right association of the line below produces -- and the
	 * single `flds 0.00025f` reused by both of its outer terms is why the
	 * literal appears twice rather than being factored out.  0.0005f is
	 * bit-exactly twice 0.00025f, and the object loads both anyway.
	 */
	sq = y * y;
	e = 0.00025f * sq - 0.0005f * bpfSq1 + 0.00025f * bpfSq2;
	bpfSq2 = bpfSq1;
	bpfSq1 = sq;

	lastHalfBaudErr = e;

	/*
	 * `halfBaudStep` is tested with `test $0x1,%al`, so only its low bit
	 * matters.  It runs 0 -> 1 -> 2 -> 1 -> 2 ..., which makes this an
	 * every-other-sample loop: the even call remembers -e and coasts, the
	 * odd call differences the two and closes the loop.  That is the
	 * half-baud rate again.
	 */
	if (halfBaudStep & 1) {
		float d, adj;

		halfBaudStep++;

		errZ1 += e;
		d = errZ1 * 0.5f;

		/* The integrator, then the proportional term on top of it. */
		timingOffset = timingOffset + bllK2 * d;
		adj = bllK1 * d + timingOffset;

		lastPhaseAdj = adj;
		phase += adj;
	} else {
		/*
		 * The object stores 0 at +0x60 (0x3584d) and then 1 (0x35860,
		 * via `xor %eax,%eax; inc %eax`), two stores to the same word
		 * with nothing between them that could alias.  `= 0; ...; ++`
		 * and `= 0; ...; = 1;` are indistinguishable here; the
		 * increment is written because `xor`/`inc` is how GCC
		 * materialises a value it tracked through one.
		 */
		halfBaudStep = 0;

		lastPhaseAdj = timingOffset;
		phase += timingOffset;
		errZ1 = -e;

		halfBaudStep++;
	}
}

/*
 * Renormalise the resonator's b0 from the measured baud/2 DFT bin.
 *
 * DOES NOTHING AT ALL unless `dftDone` is set, which needs 256 prior calls to
 * `SdHalfBaudDft`.  That early return is what makes a comparison over two
 * freshly-constructed objects vacuous, and test/unit/t_resampler.cpp drives
 * the DFT to completion before it calls this.
 *
 * The `1.0f` of the reciprocal and the `1.0f` of the floor are ONE `fld1` in
 * the object, shared -- which is the evidence both are spelled `1.0f` and
 * that the division is written this way round rather than as
 * `350000.0f / dftMag`.  Over 4 is not a gain the loop will accept -- it
 * means the bin is implausibly small -- so it asks for nominal rather than
 * clamping to 4.
 */
void
ResamplerTiming::adjustHalfBaudBpfGain(float v)
{
	float normFactor;

	if (!dftDone)
		return;

	dftMag = v * dftMag;

	if (dftMag == 0.0f) {
		normFactor = 1.0f;
	} else {
		normFactor = 1.0f / dftMag * 350000.0f;

		if (normFactor > 4.0f)
			normFactor = 1.0f;
		else if (normFactor > 2.0f)
			normFactor = 2.0f;
	}
	if (normFactor < 1.0f)
		normFactor = 1.0f;

	normBPFhBaudB0coef = normFactor * 0.03981f;

	/*
	 * Three separately gated prints, each re-reading `dsplibs_debug_level`
	 * (0x35977, 0x35a19, 0x35aad).  These three strings are the author's
	 * own words and they are what named `normBPFhBaudB0coef`,
	 * `normFactor` and the "baud/2 dft bin" -- see ResamplerTiming.h.
	 *
	 * ONE DEVIATION, RECORDED RATHER THAN CONTORTED AROUND: in the first
	 * block the object takes the digits from the 80-bit register holding
	 * `v * dftMag` and the SIGN from a reload of the member
	 * (`fcomps 0x80(%ebx)`, the rounded float).  Both readings are the
	 * same source expression; which one the compiler used is register
	 * allocation, and only a value whose 80-bit and float forms straddle
	 * zero could tell them apart.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "ResamplerTiming::adjustHalfBaudBpfGain  baud/2 dft bin "
		    "= %c%d.%03d\r\n",
		    sign_of(dftMag), whole_of(dftMag),
		    frac_of(dftMag, 1000.0f));

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "ResamplerTiming::adjustHalfBaudBpfGain  normFactor = " "%c%d.%03d\r\n",
		    sign_of(normFactor), whole_of(normFactor),
		    frac_of(normFactor, 1000.0f));

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "ResamplerTiming::adjustHalfBaudBpfGain  "
		    "normBPFhBaudB0coef = %c%d.%03d\r\n",
		    sign_of(normBPFhBaudB0coef), whole_of(normBPFhBaudB0coef),
		    frac_of(normBPFhBaudB0coef, 1000.0f));
}

/*
 * Advance `phase` by one nominal step and bank whole outputs.
 *
 * `push 0; push phases; fildll` is the unsigned-to-double idiom, so `phases`
 * is unsigned, and the conversion happens ONCE outside the loop.
 * `inputCredit` is loaded and stored only INSIDE the rotated loop (0x35f93,
 * 0x35fae), so on the no-wrap path it is not written at all -- which a test
 * that only checks its VALUE cannot see.
 *
 * Neither this nor `addPhase` has a caller anywhere in dsplibs.o; both are
 * global, so they are reached from outside the object or they are dead.  The
 * name is unexplained: it advances the phase and inverts nothing.
 */
void
ResamplerTiming::invertPhase()
{
	phase += ppmScale;

	while (phase >= (double)phases) {
		phase -= (double)phases;
		inputCredit++;
	}
}

/*
 * Nudge `phase` by a fraction of a step.
 *
 * `0.00555555569f` is 0x3bb60b61, which is also exactly what `1.0f / 180.0f`
 * folds to -- so the argument is plausibly in degrees against a half cycle,
 * but the object only proves the constant and the decimal spelling is the one
 * that cannot be wrong.  The guard is `fcoms 0.0f; jbe`, so zero, a negative
 * and an unordered compare all return without touching anything.
 */
void
ResamplerTiming::addPhase(float v)
{
	if (v > 0.0f) {
		phase += v * ppmScale * 0.00555555569f;

		while (phase >= (double)phases) {
			phase -= (double)phases;
			inputCredit++;
		}
	}
}
