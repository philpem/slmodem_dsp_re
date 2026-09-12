/*
 * SineWave.h -- the object's sine generator, as a class template.
 *
 * Three weak symbols in their own `.gnu.linkonce.t.*` sections, instantiated
 * at `<float, float>` and nothing else, so this file's name is a description
 * rather than a translation unit's -- the same situation as DspMath.h, and for
 * the same reason (finding F243).
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

#include <math.h>

template <class Tout, class Tparam>
class SineWave {
public:
	/**
	 * @brief Construct an oscillator, storing all four parameters as-is.
	 *
	 * Nine plain `mov`s in the object and nothing else -- a signalling
	 * NaN handed to any parameter lands in the member intact.
	 *
	 * @param a   Output amplitude (`amplitude`).
	 * @param f   Frequency in Hz (`frequency`), numerator of `2*pi*f/sr`.
	 * @param p   Initial phase in radians (`phase`), carried between
	 *            generate() calls.
	 * @param sr  Sample rate in Hz (`sampleRate`), divisor of the step.
	 */
	SineWave(Tparam a, Tparam f, Tparam p, Tparam sr);

	/**
	 * @brief Destroy the oscillator. Empty body, but declared rather than
	 *        left implicit -- the object's own destructor is a bare `ret`
	 *        that exists only because the original source declared one.
	 */
	~SineWave();

	/**
	 * @brief Generate @p n samples of the sine wave into @p out.
	 *
	 * Advances the phase accumulator by `frequency * 2*pi / sampleRate`
	 * per sample, at extended (`long double`) precision, and writes
	 * `amplitude * sin(phase)` for each. Wraps `phase` to `(-2*pi, 2*pi)`
	 * exactly once, at the end of the call -- never inside the loop -- so
	 * quantisation error random-walks within a call and accuracy over a
	 * long run depends on the caller's block size (see the file comment
	 * above for measured error at several block sizes). Even for
	 * `n == 0`, the wrap still runs and rewrites `phase`, and the step is
	 * still computed, so a zero `sampleRate` still raises the masked
	 * divide-by-zero.
	 *
	 * `n` is spelled `unsigned long` and not `size_t` because the two
	 * mangle differently even though they are the same type on this
	 * target; the object's mangled name says `m`, not `j`.
	 *
	 * @param out  Output buffer, @p n samples.
	 * @param n    Number of samples to generate.
	 */
	void generate(Tout *out, unsigned long n);

	Tparam	amplitude;	/* +0x00 output scale                       */
	Tparam	frequency;	/* +0x04 Hz, numerator of 2pi*f/rate        */
	Tparam	phase;		/* +0x08 radians, carried between calls     */
	Tparam	sampleRate;	/* +0x0c Hz, divisor                        */
};

/*
 * The two constants, from `.rodata.cst4` at +0x4c and +0x50.  They are the
 * correctly rounded FLOATS, not the doubles: the object loads them with `flds`
 * and `fmuls`, and (float)2pi is high by 2.8e-8 relative, which is where the
 * wrap's residual error comes from.  Writing `2*M_PI` here instead would be a
 * different number and the test says so.
 */
#define SW_TWO_PI	0x1.921fb6p+2f	/* 0x40c90fdb = (float)(2*M_PI)    */
#define SW_INV_2PI	0x1.45f306p-3f	/* 0x3e22f983 = (float)(1/(2*M_PI))*/

/*
 * The four constructor stores.  The object's constructor is nine `mov`s and
 * nothing else, so a signalling NaN handed to it lands in the member intact.
 * The const qualifiers on all four
 * by-value parameters recover the period compiler's interleaved load/store
 * schedule: the other fifteen qualifier masks do not match (finding F10220).
 */
template <class Tout, class Tparam>
SineWave<Tout, Tparam>::SineWave(const Tparam a, const Tparam f,
			      const Tparam p, const Tparam sr)
{
	amplitude = a;
	frequency = f;
	phase = p;
	sampleRate = sr;
}

/*
 * The destructor is empty but is DECLARED, not left implicit.  An implicit one
 * is never emitted, so the object's `D1` -- a bare `ret`, one byte -- only
 * exists because the original's source declared it.
 */
template <class Tout, class Tparam>
SineWave<Tout, Tparam>::~SineWave()
{
}

template <class Tout, class Tparam>
void SineWave<Tout, Tparam>::generate(Tout *out, unsigned long n)
{
	/*
	 * `long double` THROUGHOUT, and not for extra accuracy.  Under
	 * `-mfpmath=387` the intermediates live in x87 registers at 64-bit
	 * mantissa and are rounded to `float` only where the object stores
	 * them.  `double` gives the same answers here ONLY because the same
	 * thing happens to it; on any target where doubles round at each step
	 * it would diverge.  The type is written to say what is meant.
	 */
	const long double k = SW_TWO_PI;
	const long double ik = SW_INV_2PI;
	const long double step = (long double)frequency * k / sampleRate;
	long double x;

	if (n == 0) {
		/*
		 * NOT A NO-OP.  The loop is skipped but the wrap below still
		 * runs, so a zero-length call still rewrites `phase` -- 100.0
		 * comes back as 5.75221777.  The step is computed before the
		 * test too, so a zero `sampleRate` still raises the masked
		 * divide-by-zero.
		 */
		x = phase;
	} else {
		unsigned long i = 0;

		x = phase;
		for (;;) {
			long double s;
			bool more;

			/*
			 * The period compiler/header expands sinl under the C++ source
			 * fast-math flags to `fsin`.  In the object its argument is
			 * the UNROUNDED 80-bit register
			 * value -- the `fsts` below rounds a copy for memory
			 * and the sine is taken of what is still in st(0).
			 * Feeding `fsin` the float-rounded phase instead is a
			 * real difference: 17,650 mismatches.
			 *
			 * Above |x| = 2^63 the instruction sets C2 and leaves
			 * st(0) alone, so the "sine" comes back as the angle.
			 * The object does not test C2 and neither does this.
			 */
			s = sinl(x);
			out[i] = (Tout)(s * (long double)amplitude);
			++i;
			more = (i < n);
			/*
			 * THE ADVANCE IS UNCONDITIONAL and runs one more time
			 * than there are samples.  In the object it sits
			 * between the `cmp` at +0x3b and the `jb` at +0x42, so
			 * after the last sample the phase steps once more and
			 * THAT is the value the wrap stores.  `generate(buf,1)`
			 * on a 1200 Hz tone at 9600 leaves phase = 0.785398185,
			 * a full delta, not 0.  Moving the advance inside the
			 * `if` is the natural tidy-up and costs 50,686
			 * mismatches.
			 *
			 * It also re-reads `phase` from memory rather than
			 * carrying the accumulator in the register, which is
			 * what makes the running value a float at every step.
			 */
			x = (long double)phase + step;
			if (!more)
				break;
			phase = (Tparam)x;
		}
	}

	/*
	 * The wrap: x - 2pi*trunc(x/2pi), leaving (-2pi, 2pi).  TRUNCATION, not
	 * floor -- the sign is preserved, and `floor` costs 19,105 mismatches.
	 *
	 * `int`, not `long`.  They are the same width here, but the width is
	 * load-bearing: it puts the `fistpl` overflow at 2^31*2pi, and above
	 * |phase| = 1.349e10 the conversion returns the integer indefinite
	 * 0x80000000, so the "wrap" ADDS 1.349e10 instead of reducing.  Silently
	 * -- no NaN, no flag the object looks at.  On LP64 a `long` would move
	 * that threshold and change the behaviour being reproduced.
	 *
	 * The last operation is FSUBRP: `de e1` prints as `fsubp` and computes
	 * ST(1) = ST(0) - ST(1), so it is `x - 2pi*t` and not the negation of
	 * it.  Reading objdump literally here costs 75,297 mismatches -- the
	 * same trap as the `de f1` divide, finding F245.
	 */
	{
		int t = (int)(x * ik);

		phase = (Tparam)(x - (long double)t * k);
	}
}

#endif /* DSPLIB_SINEWAVE_H */
