/*
 * Resampler.cpp -- the polyphase resampler's base class.
 *
 * Reconstructed from dsplibs.o.  dsplib/Resampler.h carries the object map,
 * the inheritance chain, the four vtables and the argument for the member
 * `operator delete`; this file is the code.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * ---------------------------------------------------------------------------
 * WHAT THE CLASS IS
 *
 * An interpolating polyphase FIR.  `phases` is the interpolation factor,
 * `taps` the number of taps in each polyphase branch, and `coeffs` holds the
 * branches consecutively -- branch p at `coeffs + p * taps`.  `history` is a
 * ring of input, `historyIndex` the write cursor, `phase` the fractional
 * position within the input stream expressed in units of 1/phases of a
 * sample.
 *
 * THE TWO CONSTRUCTORS DIFFER IN WHO OWNS `coeffs`, and that is the whole
 * difference:
 *
 *   the `float` one   designs a Blackman-windowed low-pass with taps*phases
 *                     taps, transposes it into the polyphase bank, and stores
 *                     0 in `coeffsBorrowed`
 *   the `float *` one adopts the caller's array and stores 1
 *
 * and `~Resampler` frees `coeffs` only when the flag is 0.  The closure of
 * `dp_vpcm_init` needs only the first; the second is written because without
 * it `coeffsBorrowed` is never 1, the destructor's skip-the-free arm is
 * unreachable, and the test that claims to cover the destructor would be
 * covering one of its two arms.
 *
 * `taps` IS ROUNDED DOWN TO A MULTIPLE OF FOUR by the designing constructor
 * (`shr $0x2` then `lea (,%edx,4)`) and NOT by the adopting one, which takes
 * the caller's count as given.  That asymmetry is in the object and is not a
 * transcription slip; `resample`'s inner product is unrolled four ways.
 *
 * `historyLen` IS `minHistory` OR `10 * taps`, whichever the comparison
 * picks: both constructors compute `taps < minHistory ? minHistory
 * : 10 * taps`, the second spelled `(taps + 4*taps) * 2` by the compiler.
 *
 * ---------------------------------------------------------------------------
 * THE CONSTRUCTORS' TAILS ARE `reset()` INLINED
 *
 * Every store the designing constructor makes between the history allocation
 * and the `LowPassFIR` -- and every store the adopting one makes after the
 * allocation -- is `Resampler::reset()`, statement for statement and in the
 * same order.  Written as the call it is.  The object has it inlined, which
 * is the compiler's choice; behaviour is identical either way, and the same
 * pattern repeats one level down in each of the three derived classes.
 */

#include <stddef.h>

#include "dsplib/DspMath.h"
#include "dsplib/LowPassFIR.h"
#include "dsplib/Resampler.h"
#include "dsplib/sysdep.h"

/*
 * See V90ConstellationDesigner.cpp for why these are here and why guarded.
 * `vptr` is not a field, so it cannot be asserted with `offsetof`; the size
 * assertion below is what pins it, since 0x48 only comes out right if the
 * four bytes at +0x00 are there.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define RS_OFF(field, off, tag) \
	typedef char rs_off_##tag[ \
	    ((int)__builtin_offsetof(Resampler, field) == (off)) ? 1 : -1]

RS_OFF(coeffs,		0x04, coeffs);
RS_OFF(history,		0x08, history);
RS_OFF(phase,		0x0c, phase);
RS_OFF(pending,		0x14, pending);
RS_OFF(pendingCount,	0x28, pendingcount);
RS_OFF(ppmScale,	0x2c, ppmscale);
RS_OFF(phases,		0x30, phases);
RS_OFF(taps,		0x34, taps);
RS_OFF(historyLen,	0x38, historylen);
RS_OFF(historyIndex,	0x3c, historyindex);
RS_OFF(inputCredit,	0x40, inputcredit);
RS_OFF(coeffsBorrowed,	0x44, coeffsborrowed);
typedef char rs_size[(sizeof(Resampler) == 0x48) ? 1 : -1];
#endif

/*
 * The designing constructor.
 *
 * `LowPassFIR<float>` is built on the stack with `taps * phases` taps, a
 * cutoff of `cutoff / phases` and a gain of `phases`, windowed
 * WINDOW_BLACKMAN -- the object passes a literal 3, and DspMath.h records
 * that 3 is Blackman, read from `designWindow`'s switch rather than assumed.
 *
 * THE TRANSPOSE IS THE POINT.  `LowPassFIR` designs one long filter; the
 * polyphase bank wants branch p to hold every phases'th tap, and in REVERSE
 * order, because `resample` walks the history forwards and the coefficients
 * forwards at the same time.  Hence `(taps - 1 - k) * phases + p`.
 */
Resampler::Resampler(unsigned int nPhases, float scale, unsigned int nTaps,
		     float cutoff, unsigned int minHistory)
{
	unsigned int p, k;

	ppmScale = scale;
	phases = nPhases;
	taps = (nTaps >> 2) * 4;
	historyLen = (taps < minHistory) ? minHistory : 10 * taps;

	history = 0;
	if (historyLen)
		history = (float *)sysdep_malloc(historyLen * sizeof(float));

	reset();

	{
		LowPassFIR<float> fir(taps * phases, cutoff / phases,
				      WINDOW_BLACKMAN, phases);

		coeffs = (float *)sysdep_malloc(taps * phases * sizeof(float));
		coeffsBorrowed = 0;

		if (coeffs) {
			for (p = 0; p < phases; p++)
				for (k = 0; k < taps; k++)
					coeffs[p * taps + k] =
					    fir.coefficients[(taps - 1 - k)
							     * phases + p];
		}
	}
}

/*
 * The adopting constructor.  `coeffsBorrowed` is 1 and `taps` is taken as
 * given -- see the file comment.
 */
Resampler::Resampler(unsigned int nPhases, float scale, unsigned int nTaps,
		     float *bank, unsigned int minHistory)
{
	coeffs = bank;
	phases = nPhases;
	ppmScale = scale;
	taps = nTaps;
	coeffsBorrowed = 1;
	historyLen = (taps < minHistory) ? minHistory : 10 * taps;

	history = 0;
	if (historyLen)
		history = (float *)sysdep_malloc(historyLen * sizeof(float));

	reset();
}

/*
 * `history` is always ours; `coeffs` only when we designed it.  The object
 * tests `coeffsBorrowed` FIRST and skips the whole second free when it is
 * set, so a borrowed null pointer is never even loaded.
 */
Resampler::~Resampler()
{
	if (history)
		sysdep_free(history);
	if (!coeffsBorrowed && coeffs)
		sysdep_free(coeffs);
}

/*
 * `historyIndex` starts at `taps`, not at zero: the first `taps` floats of
 * the ring are the past the first inner product needs, and `copyHistoryTail`
 * is what refills them when the cursor wraps.
 */
void
Resampler::reset()
{
	unsigned int i;

	if (history) {
		for (i = 0; i < historyLen; i++)
			history[i] = 0;
	}

	inputCredit = 0;
	phase = 0;
	historyIndex = taps;

	for (i = 0; i < 5; i++)
		pending[i] = 0;

	pendingCount = 0;
}

/*
 * Normalized means "in [0, 1) of one input sample", so the stored value is
 * scaled by `phases`.  ANYTHING OUTSIDE [0, 1) GIVES ZERO, including a
 * negative, and the two comparisons are against the 0.0f at
 * .rodata.cst4+0x1d4 and the 1.0f at +0x1d8.
 *
 * `p * phases` is computed at extended precision: `fildll` puts the exact
 * integer on the x87 stack, `fmulp` multiplies there, and the only rounding
 * is the `fstpl` into the `double`.  That is what the tree's default
 * (-mfpmath=387, no -ffloat-store) gives for this expression.
 */
void
Resampler::setNormalizedPhase(float p)
{
	if (p >= 0.0f && p < 1.0f)
		phase = p * phases;
	else
		phase = 0;
}

float
Resampler::getNormalizedPhase() const
{
	return phase / phases;
}

/*
 * Bring the last `taps` samples of the ring to its front.  `resample` does
 * this every time the write cursor reaches the end, so that the inner product
 * for the next output can still reach `taps` samples back.
 */
void
Resampler::copyHistoryTail()
{
	unsigned int i;

	for (i = 0; i < taps; i++)
		history[i] = history[historyLen - taps + i];
}

void
Resampler::resetHistoryIndex()
{
	historyIndex = taps;
}

/*
 * The inner product, UNROLLED BY FOUR INTO TWO PARTIAL SUMS -- which is the
 * object's shape and not a performance idea of ours.
 *
 * `Resampler::resample` at .text+0x34da0 opens each inner product by pushing
 * three zeros and the phase:
 *
 *      34ece:  d9 ee           fldz
 *      34ed4:  d9 c0           fld    %st(0)
 *      34ed6:  d9 c1           fld    %st(1)
 *      34eda:  dd 47 0c        fldl   0xc(%edi)      <- phase
 *
 * and then alternates its accumulations between two of them, four products
 * to an iteration, walking both pointers by 0x10:
 *
 *      34f25:  flds  (%eax) ; fmuls  (%edx) ; faddp %st,%st(2)   <- a
 *      34f2e:  flds 4(%eax) ; fmuls 4(%edx) ; faddp %st,%st(3)   <- b
 *      34f36:  flds 8(%eax) ; fmuls 8(%edx) ; faddp %st,%st(2)   <- a
 *      34f3e:  flds c(%eax) ; fmuls c(%edx) ; faddp %st,%st(3)   <- b
 *      34f4a:  cmp   $0x3,%ecx ; ja 34f25
 *
 * with a one-at-a-time residue loop at 34f55 accumulating into `a` alone,
 * the two sums joined at 34f6a, and the result narrowed ONCE at 34f76 by
 * `fstps 0x34(%esp)`.
 *
 * IT IS IN THE SOURCE, NOT IN THE COMPILER.  GCC 3.4's `-O2` does not imply
 * `-funroll-loops`; that is `-O3`.  So the author wrote the unrolling, and a
 * plain `for (i = 0; i < taps; i++)` here -- which is what this file had --
 * was never what was compiled.  Two accumulators rather than one is the
 * standard reason: an x87 add has a latency the next add cannot hide, and
 * alternating halves the dependent chain.
 *
 * THE UNROLLING DOES NOT BRING THE NARROWING WITH IT, which was the thing
 * worth finding out.  The object narrows once, `fstps 0x34(%esp)` at
 * .text+0x34f76, because it has spilled a `float` accumulator; the guess was
 * that our un-unrolled loop simply kept too few values live and that matching
 * the shape would restore the pressure.  It does not.  With this exact shape
 * GCC 3.4.2 emits the same alternating two-accumulator sequence and STILL
 * keeps both sums in x87 registers -- 100 of 15491 and 592 of 4356 checks
 * disagree with the blob, the same counts as no narrowing at all.
 *
 * The pressure is not in the loop.  The object holds `phase` AND three zeros
 * on the x87 stack across it (34ece-34eda above), which is four slots gone
 * before the first product; ours keeps phase in memory.  So the narrowing
 * stays stated -- accumulate wide, convert once -- which is what the object
 * does and now depends on no compiler's allocator.  Findings 1352, 1354, 1356.
 *
 * A macro and not an inline function, deliberately: GCC 3.4 at -O2 does not
 * inline a function not declared `inline`, and an out-of-line call here would
 * put a four-byte argument slot in the middle of the inner product -- exactly
 * the accident this file spent two findings getting out of.
 */
#define DOT(dst, hp, cp)						\
	do {								\
		const float *h_ = (hp), *c_ = (cp);			\
		double a_ = 0, b_ = 0;					\
		unsigned int n_ = taps;					\
									\
		while (n_ > 3) {					\
			a_ += h_[0] * c_[0];				\
			b_ += h_[1] * c_[1];				\
			a_ += h_[2] * c_[2];				\
			b_ += h_[3] * c_[3];				\
			h_ += 4;					\
			c_ += 4;					\
			n_ -= 4;					\
		}							\
		while (n_-- != 0)					\
			a_ += *h_++ * *c_++;				\
									\
		(dst) = (float)(a_ + b_);				\
	} while (0)

/*
 * Turn `n` new input samples into however many output samples the phase
 * accumulator asks for.  This is the only member that reads `pending`,
 * `historyIndex` and `inputCredit`, and the only one that dispatches through
 * the `timingCorrection` vtable slot.
 *
 * `inputCredit` IS AN INPUT CREDIT and the obvious reading of it is backwards.
 * At the top of every pass it holds the number of INPUT samples that must be
 * shifted into `history` before the next output can be computed, and the
 * bottom of the pass recomputes it as the number of whole `phases` the
 * accumulator has just stepped over.  `reset()` leaves it 0, which is why the
 * very first call emits an output immediately, out of a zeroed ring.
 *
 * ONE SAMPLE IS ALWAYS HELD BACK.  The loop runs while `avail > 1` and the
 * function ends by pushing the sample it did not consume into `pending`,
 * because the interpolation has to look one input sample AHEAD whenever the
 * upper of its two branches would be branch `phases` -- see the `else` arm,
 * which writes `*in` into `history[historyIndex]` WITHOUT advancing the
 * cursor.  `pending` is that carry across calls: a FIFO, drained with a
 * shift-down, which with `n >= 1` never holds more than one sample.
 *
 * THE `- taps` IN EVERY WINDOW BASE is why `historyIndex` starts at `taps`
 * and why `copyHistoryTail` exists: the inner product always reaches `taps`
 * samples back from the cursor.
 *
 * IT READS ONE SAMPLE PAST `in[n - 1]`, and that is the ORIGINAL'S BUG, not a
 * transcription slip: the tail store runs on every return path, including the
 * one where the loop has just consumed the last sample, and the look-ahead
 * arm can reach it too.  It is reproduced deliberately -- a caller that
 * cannot spare the sample is a caller the blob would have broken as well.
 *
 * The signed comparison in the drain loop is the object's: `cmp %edx,%ecx`
 * with `jg` at .text+0x34e63, not `ja`.  The branch above it guarantees
 * `pendingCount >= 1`, so the two readings never differ dynamically, but the
 * signed one is what is there.
 */
void
Resampler::resample(const float *in, unsigned int n, float *out,
		    unsigned int &nOut)
{
	unsigned int avail;
	unsigned int i, k;
	int j;

	nOut = 0;
	avail = n + pendingCount;

	while (avail > 1) {
		unsigned int credit = inputCredit;
		unsigned int fill;
		const float *h;
		const float *c;
		int ph;
		float y0, y1, frac, y;

		/*
		 * Shift in what the credit asks for, or all there is,
		 * whichever is less.  `pending` drains before `in`.
		 */
		fill = (credit < avail) ? credit : avail;

		for (k = 0; k < fill; k++) {
			if (pendingCount) {
				history[historyIndex++] = pending[0];
				pendingCount--;
				for (j = 0; j < (int)pendingCount; j++)
					pending[j] = pending[j + 1];
			} else {
				history[historyIndex++] = *in++;
			}

			if (historyIndex == historyLen) {
				copyHistoryTail();
				resetHistoryIndex();
			}
		}

		/* Not enough input to pay the credit off: carry the rest. */
		if (credit > avail) {
			inputCredit = credit - avail;
			break;
		}
		avail -= credit;

		/*
		 * Two adjacent polyphase branches, linearly interpolated.
		 * `ph` is the truncating cast the object spells out as
		 * fnstcw / or $0xc00 / fldcw / fistl.
		 *
		 * THE ACCUMULATOR IS `double` AND THE RESULT IS NARROWED
		 * EXPLICITLY, because that is what the object does and it is
		 * the only spelling that says so portably.
		 *
		 * The blob accumulates at 80 bits -- `faddp %st,%st(2)`,
		 * `taps` times, with NO store in the loop -- and then narrows
		 * exactly once, `fstps 0x34(%esp)` at .text+0x34f76.  That
		 * store is a spill of a variable the author declared `float`;
		 * the narrowing was a side effect of GCC running out of x87
		 * registers, not of anything written down (finding 1352).
		 *
		 * WRITING `float y0` AND HOPING FOR THE SAME SPILL DOES NOT
		 * WORK, and it is worth knowing why before anyone tidies this
		 * back.  Under -mfpmath=387 with the default
		 * -fexcess-precision=fast, neither a plain assignment to a
		 * `float` nor an explicit `(float)` cast emits a store --
		 * measured on both compilers, and it is why this file once
		 * carried a `volatile` helper.  A double-to-float conversion
		 * is different: the value really is a `double`, so the
		 * conversion is one the compiler must perform.
		 *
		 * So the accumulation stays wide and the narrowing is stated,
		 * which is exactly the object's behaviour and depends on no
		 * compiler's register pressure.  Finding 1354.
		 */
		ph = (int)phase;

		h = history + historyIndex - taps;
		c = coeffs + ph * taps;
		DOT(y0, h, c);

		if ((unsigned int)(ph + 1) < phases) {
			h = history + historyIndex - taps;
			c = coeffs + (ph + 1) * taps;
			DOT(y1, h, c);
		} else {
			/*
			 * Branch `phases` IS branch 0 one input sample later,
			 * so peek the next sample into the ring without
			 * advancing the cursor and slide the window up one.
			 */
			history[historyIndex] = *in;
			h = history + historyIndex - taps + 1;
			c = coeffs;
			DOT(y1, h, c);
		}

		frac = (float)(phase - ph);
		y = y0 + (y1 - y0) * frac;
		*out++ = y;
		nOut++;

		/*
		 * The new phase has to be IN MEMORY before the hook runs:
		 * ResamplerTimingOffset::timingCorrection adds its offset to
		 * it, and the credit loop below reads it back.  `y` reaches
		 * the hook and the output stream from the same unrounded
		 * value: the object stores it with a non-popping `fsts` and
		 * then spills the same register with `fstps` for the argument.
		 */
		phase = phase + ppmScale;
		timingCorrection(y);

		inputCredit = 0;
		while (phase >= phases) {
			phase -= phases;
			inputCredit++;
		}
		if (phase < 0)
			phase = 0;
	}

	pending[pendingCount++] = *in;
}
