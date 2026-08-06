/*
 * SineWave.h -- the object's sine generator, as a class template.
 *
 * Three weak symbols in their own `.gnu.linkonce.t.*` sections, instantiated
 * at `<float, float>` and nothing else, so this file's name is a description
 * rather than a translation unit's -- the same situation as DspMath.h, and for
 * the same reason (finding 243).
 *
 * The only caller is the V.90 line-verification tone: both `VPcmFloModem`
 * constructors build one with (4800, 980, 0, 9600) and `qcLineVerification`
 * drives it a block at a time.  980 Hz at a 9600 Hz sample rate.
 *
 * ---------------------------------------------------------------------------
 * WHICH TEMPLATE PARAMETER IS WHICH IS A GUESS, and is labelled as one
 *
 * There is exactly one instantiation in the object, `<float, float>`, so
 * nothing distinguishes the sample type from the parameter type -- both are
 * `float` and the mangling records only that.  The two-parameter split itself
 * is the original's (the sibling `GenericIIR<float, double>` shows the
 * codebase really does use two distinct type arguments on templates like
 * this), but the ROLES are not recoverable.  `Tout` for the output samples and
 * `Tparam` for the four members is the reading that makes `generate`'s
 * signature sensible; it is not proven and no test can prove it.
 *
 * ---------------------------------------------------------------------------
 * The constructor argument order is NOT the obvious one
 *
 *     SineWave(amplitude, frequency, phase, sampleRate)
 *
 * and the third and fourth are the ones to get wrong.  Both real call sites
 * pass (4800.0f, 980.0f, 0.0f, 9600.0f), which settles rate-against-phase by
 * magnitude alone; the dataflow settles it independently, since the per-sample
 * increment is `member[+4] * 2pi / member[+12]` and only frequency-over-rate
 * makes that a radian step.  Swapping the two is caught by the test.
 *
 * ---------------------------------------------------------------------------
 * `phase` is the running accumulator, and it drifts on purpose
 *
 * It is wrapped to (-2pi, 2pi) exactly once, at the END of every `generate`,
 * and never inside the loop.  Within one call the angle therefore grows to
 * `phase + n*step` while being re-rounded to `float` on every sample, so the
 * quantisation error random-walks and the accumulated angle loses precision
 * with length.  Measured, f = 1200 at 9600:
 *
 *     one call of 96000 samples     final phase error 1.864 rad -- useless
 *     200 calls of 480              0.159 rad
 *     2000 calls of 48              0.031 rad
 *
 * So the caller's block size is part of the accuracy of the output, and the
 * V.90 code's per-block calls are not incidental.  This is the object's
 * behaviour and is reproduced, not corrected.
 */

#ifndef DSPLIB_SINEWAVE_H
#define DSPLIB_SINEWAVE_H

template <class Tout, class Tparam>
class SineWave {
public:
	SineWave(Tparam a, Tparam f, Tparam p, Tparam sr);
	~SineWave();

	/*
	 * `n` is `unsigned long`, not `size_t`.  On i386 those are the same
	 * type to the compiler but NOT to the mangler: `size_t` mangles as `j`
	 * and the object says `m`, so `..._EPfm` only comes out of the spelling
	 * used here.
	 *
	 * Returns nothing.  A function template would have encoded the return
	 * type, but this is an ordinary member so the mangling is silent; the
	 * call site at 0xf889 discards `%eax`, and what `%eax` holds at the
	 * `ret` is leftover from the wrap's `fildl`.
	 */
	void generate(Tout *out, unsigned long n);

	Tparam	amplitude;	/* +0x00 output scale                       */
	Tparam	frequency;	/* +0x04 Hz, numerator of 2pi*f/rate        */
	Tparam	phase;		/* +0x08 radians, carried between calls     */
	Tparam	sampleRate;	/* +0x0c Hz, divisor                        */
};

#endif /* DSPLIB_SINEWAVE_H */
