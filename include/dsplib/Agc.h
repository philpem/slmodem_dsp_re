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

/*
 * The period compiler keeps the double expression in its x87 register.
 * -fno-math-errno supplies the bare square root, including the instruction's
 * negative-input NaN.  The caller must not first convert a long-double
 * expression to double: that introduces a narrowing absent from the object.
 * See docs/issue19-inline-asm.md for the crossed full-TU controls.
 */
static inline double agc_fsqrt(double x)
{
	return sqrt(x);
}

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
 * EVERY `long double` HERE IS AN x87 REGISTER VALUE the original never rounds,
 * and every assignment to a `T` member is a store the original does round.
 * Rounding the gain terms costs 1,058,034 mismatches, rounding the square root
 * 1,054,601, and keeping `acc` at 80 bits across a block 159,062.
 */
template <class T>
void Agc<T>::process(const T *in, T *out, unsigned nSamples)
{
	unsigned c;

	if (nSamples == 0)
		return;			/* returns without touching `count` */

	c = count;			/* cached in a register for the call */

	/*
	 * THE FLOOR IS A NAMED LOCAL AND THE OBJECT SAYS SO.  `flds 0x128` at
	 * 0x1c loads it BEFORE the loop is entered, beside `fld1` and `fldz`,
	 * and it stays in `%st(4)` for the whole function -- the block update
	 * compares against it with `fcom %st(4)`.  Written as a literal inside
	 * the loop, GCC 3.4.2 does not hoist it: it reloads `flds` into
	 * `%st(0)` at the comparison and then has the CONSTANT as the left
	 * operand, so it emits the reversed predicate `jb`, and under
	 * -mno-ieee-fp `jb` is taken for an unordered compare -- a NaN level
	 * then adapts the gain where the object's `jbe` freezes it.  A local
	 * initialised here puts `lvl` back in `%st(0)`, `fcom %st(4)`, and the
	 * object's NaN routing.  Finding F2302.
	 */
	long double minLevel = 1e-10f;

	for (;;) {
		/*
		 * `in[0]` and `gain` are RE-READ after the `acc` store rather
		 * than kept in registers, because the store may alias them --
		 * `out` is allowed to be the object itself.  Caching them costs
		 * 42,866.
		 */
		acc = in[0] * in[0] + acc;
		out[0] = gain * in[0];
		++in;
		++out;
		--nSamples;

		if (--c != 0) {
			if (nSamples == 0)
				break;
			continue;
		}

		/*
		 * The block update runs BEFORE `nSamples` is retested, so a
		 * call that ends exactly on a boundary still performs it.
		 * Writing the loop the natural way round -- `while (nSamples)`
		 * with the update at the bottom -- costs 2,670,947.
		 *
		 * `count` is reloaded unconditionally, not only when the gain
		 * is updated: 663,433.
		 */
		c = blockLen;

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
			double lvl = (1.0 / blockLen) * acc;

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
					double t =
					    agc_fsqrt((1.0 / lvl) * ref);

					gain = (T)(alpha * gain
						   + (1.0 - alpha) * t);
				}
			}
		}

		if (nSamples == 0)
			break;
	}

	count = c;
}

#endif /* DSPLIB_AGC_H */
