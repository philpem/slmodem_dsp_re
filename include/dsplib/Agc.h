/*
 * Agc.h -- block automatic gain control, as a class template.
 *
 * Four weak symbols in their own `.gnu.linkonce.t.*` sections, so as with
 * DspMath.h, SineWave.h and DiffCoder.h the file name is a description rather
 * than a translation unit's (finding F243).
 *
 * GLOBAL NAMESPACE, deliberately.  The object's symbols are `_ZN3AgcIfE...`;
 * putting the class in a namespace would mangle them as `_ZN6dsplib3AgcIfE...`
 * and nothing would line up.
 *
 * ---------------------------------------------------------------------------
 * What it does
 *
 * It applies one gain to every sample, and once per `blockLen` samples it
 * re-measures the mean square of the block and nudges the gain towards the
 * value that would bring that mean square to `ref`.  The nudge is a one-pole
 * smoother with `alpha` as the pole:
 *
 *     gain <- alpha*gain + (1 - alpha)*sqrt(ref/level)
 *
 * so `alpha` near 1 adapts slowly and `alpha == 1` does not adapt at all.
 *
 * ---------------------------------------------------------------------------
 * `alpha == 1` IS the frozen state, and it is also the state after `reset`
 *
 * `reset` leaves `alpha` at 1.0, so a freshly reset AGC is frozen at unity
 * gain and stays there until someone writes a real pole into the field.  Its
 * one caller does exactly that: `V90Demodulator` pokes `alpha` from its
 * parameter block to switch the AGC on, and calls `freeze()` to switch it off.
 *
 * A NaN `alpha` also takes the frozen path -- the object compares with
 * `fcom`/`fnstsw`/`sahf`/`jne` and no parity check, so unordered means "not
 * equal" means frozen.
 *
 * ---------------------------------------------------------------------------
 * `savedAlpha` is written and never read
 *
 * `freeze()` saves the old pole there and nothing in the whole object ever
 * reads it back.  The obvious counterpart -- an `unfreeze()` restoring
 * `alpha = savedAlpha` -- is not in the blob at all, which for an implicitly
 * instantiated template member means it was declared but never called.
 * `V90Demodulator` reactivates by poking `alpha` directly instead.
 */

#ifndef DSPLIB_AGC_H
#define DSPLIB_AGC_H

/*
 * FSQRT on the 80-bit value already in st(0).  Inline asm rather than
 * `sqrtl()` because GCC only inlines that to the instruction under
 * `-fno-math-errno`, and a libm fallback would return a different NaN for a
 * negative argument -- reachable here, since `ref` is caller-supplied.
 */
static inline long double agc_fsqrt(long double x)
{
	long double r;

	__asm__("fsqrt" : "=t" (r) : "0" (x));
	return r;
}

template <class T>
class Agc {
public:
	Agc();

	void reset();
	void freeze();
	void process(const T *in, T *out, unsigned nSamples);

	T		alpha;		/* +0x00 pole; exactly 1.0 == frozen  */
	T		savedAlpha;	/* +0x04 written by freeze, never read*/
	T		gain;		/* +0x08 applied to every sample      */
	T		ref;		/* +0x0c target mean square           */
	T		level;		/* +0x10 mean square of the last block*/
	T		acc;		/* +0x14 sum of squares, this block   */
	unsigned	blockLen;	/* +0x18 samples per block            */
	unsigned	count;		/* +0x1c samples left in this block   */
};					/* sizeof == 32                       */

#endif /* DSPLIB_AGC_H */
