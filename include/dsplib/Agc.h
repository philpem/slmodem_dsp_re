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

#include <math.h>

template <class T>
class Agc {
public:
	/** @brief Construct at unity gain, frozen (@c alpha == 1). */
	Agc();

	/** @brief Reset to unity gain, frozen -- same state as a freshly constructed Agc. */
	void reset();

	/**
	 * @brief Freeze the gain at its current value.
	 *
	 * Saves the current @c alpha into @c savedAlpha (write-only -- nothing
	 * in the object ever reads @c savedAlpha back; there is no
	 * corresponding `unfreeze()`) and sets @c alpha to 1, which stops
	 * process() from adapting.
	 */
	void freeze();

	/**
	 * @brief Apply the current gain to a block of samples, re-measuring
	 * and adapting the gain once per @c blockLen samples.
	 * @param in        Input samples.
	 * @param out       Output samples, `out[i] = gain * in[i]`.
	 * @param nSamples  Number of samples to process.
	 */
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

/*
 * `blockLen` is set BEFORE the tail call to `reset`, so `count` comes out of
 * construction equal to 500 as well.  There are no arguments: the caller pokes
 * `blockLen` and `ref` afterwards.
 */
template <class T>
Agc<T>::Agc()
{
	blockLen = 500;
	ref = T(1);
	reset();
}

/*
 * Leaves `alpha` at 1.0 -- FROZEN AT UNITY GAIN, not adapting.  `count` is
 * reloaded from whatever `blockLen` currently holds, which is what makes the
 * caller defect in finding F608 possible.
 */
template <class T>
void Agc<T>::reset()
{
	/*
	 * `gain` BEFORE `savedAlpha`, which is the object's order and not the
	 * declaration's.  GCC 3.4 preserves the order of independent stores,
	 * so the sequence in the object is the sequence in the author's source:
	 * the two live values together, then the backup that `freeze` writes.
	 * Ours was sorted by offset, which is a tidiness we imposed.
	 * Finding F615.
	 */
	alpha = T(1);
	gain = T(1);
	savedAlpha = T(1);
	level = T(0);
	acc = T(0);
	count = blockLen;
}

/*
 * The read comes FIRST: reordering the two statements costs 563,258.
 */
template <class T>
void Agc<T>::freeze()
{
	savedAlpha = alpha;
	alpha = T(1);
}

/*
 * Apply the gain, and re-measure at every block boundary.
 *
 * Safe in place -- `V90Demodulator::progress` passes the same buffer for both.
 *
 * For the float instantiation, GCC 3.4.2 keeps intermediate values in x87
 * registers until a member/output store rounds them. A float-returning square
 * root preserves that path; narrowing a double-returning helper result adds
 * an extra rounding step absent from the reference (F12120).
 */
template <class T>
void Agc<T>::process(const T *in, T *out, unsigned nSamples)
{
	if (nSamples == 0)
		return;			/* returns without touching `count` */

	/* Keep the floor beside zero and unity across the loop. Matching its
	 * type to lvl avoids a separate widened comparison value (F12120). */
	T minLevel = T(1e-10f);

	for (;;) {
		/*
		 * `in[0]` and `gain` are RE-READ after the `acc` store rather
		 * than kept in registers, because the store may alias them --
		 * `out` is allowed to be the object itself.  Caching them costs
		 * 42,866.
		 */
		acc = in[0] * in[0] + acc;
		out[0] = in[0] * gain;
		++in;
		++out;
		--nSamples;

		if (--count == 0) {
			/* Update before testing the remaining sample count: a call ending
			 * on a block boundary must still adapt. Reload the count even
			 * when the alpha/level tests leave the gain unchanged. */

			{
				/*
				 * RECIPROCAL THEN MULTIPLY, twice, and it is the
				 * author's rather than the optimiser's: no combination
				 * of -ffast-math, -funsafe-math-optimizations or
				 * -freciprocal-math turns `a/b` into `a*(1/b)` here,
				 * because GCC's reciprocal pass needs several divides
				 * by the same divisor.  The object says `fld1; fdivp;
				 * fmuls`, so the source did too.
				 *
				 * Both `de` forms read as their own opposite: `de f9`
				 * is 1.0/blockLen and `de f2` is 1.0/lvl (finding F245).
				 * Dividing the other way costs 24 and 20 mismatches
				 * respectively -- tiny, and only reachable by the cases
				 * built for them, which is the point of finding F608.
				 */
				T lvl = (T(1) / blockLen) * acc;
				/* Capture the next countdown after consuming the conversion
				 * operands. GCSE promotes this member and sinks its store
				 * to loop exit; a manual cache changes that graph (F12121). */
				count = blockLen;

				acc = T(0);
				level = (T)lvl;	/* stored rounded; lvl stays 80-bit */

				/*
				 * PLAIN `!=`, and the object's one `fcom` is what says
				 * so.  At 0xa8 it is `fcom %st(3); fnstsw; sahf; jne`
				 * -- ONE compare, no parity check -- and under
				 * -mno-ieee-fp that is exactly what GCC emits for
				 * `alpha != T(1)`, NaN included: FCOM sets C3 for
				 * unordered as well as for equal, so ZF is set and the
				 * frozen path is taken.
				 *
				 * This used to read `alpha < T(1) || alpha > T(1)`,
				 * which is TWO compares, and it was a workaround for
				 * `make period` being built -mieee-fp: there `!=` gets
				 * a parity test and a NaN adapts.  With the flag the
				 * object was built with, the naive form is both the
				 * correct one and the object's.  Finding F2300.
				 */
				if (alpha != T(1)) {
					if (lvl > minLevel) {
						gain = (T)(alpha * gain
						    + (T(1) - alpha)
						      * sqrtf((T(1) / lvl) * ref));
					}
				}
			}

		}

		if (nSamples == 0)
			break;
	}
}

#endif /* DSPLIB_AGC_H */
