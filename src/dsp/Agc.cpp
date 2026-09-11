/*
 * Agc.cpp -- see dsplib/Agc.h.
 *
 * Verified against the object over 6,466,255 comparison points and a
 * 24-mutation kill table; re-verified in this tree by t_agc.  Three of those
 * mutations were at zero kills until a case was built specifically to separate
 * them, which is recorded in finding F608 because "0 kills, presumably
 * equivalent" would have shipped three unproven readings looking verified.
 */

#include "dsplib/Agc.h"

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

/*
 * MEMBER BY MEMBER, not `template class Agc<float>;` -- the object contains
 * these four symbols and no destructor at all, and an explicit class
 * instantiation would emit one (finding F603).  Its absence is itself evidence:
 * an implicit destructor is only omitted when it is trivial, so nothing here is
 * owned and nothing is virtual.
 */
template Agc<float>::Agc();
template void Agc<float>::reset();
template void Agc<float>::freeze();
template void Agc<float>::process(const float *, float *, unsigned);
