/*
 * SineWave.cpp -- see dsplib/SineWave.h.
 *
 * Reconstructed from three weak sections and verified over 70,520 comparison
 * points, then re-verified in this tree by t_sinewave: the full cross product
 * of amplitudes, frequencies, phases and sample rates including zero, negative,
 * denormal and the two overflow thresholds, plus random float bit patterns in
 * all four members, plus NaN and infinity.  Every point compares all sixteen
 * object bytes and the whole output buffer as bit patterns.
 */

#include "dsplib/SineWave.h"

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
 * THE CONSTRUCTOR'S FOUR STORES GO THROUGH INTEGERS, and that is not a style
 * choice.  The object's constructor is nine instructions, all `mov`:
 *
 *     mov 0x8(%esp),%ecx ; mov %ecx,(%edx)      ... and so on for all four
 *
 * so a signalling NaN handed to it lands in the member with its payload
 * intact.  The natural `: amplitude(a), frequency(f), ...` compiles under
 * `-mfpmath=387` to `flds`/`fstps`, and an x87 load-store QUIETENS a
 * signalling NaN -- 0x7f800001 goes in, 0x7fc00001 comes out.  Measured: it
 * cost 59 mismatches in t_sinewave's random-bit-pattern block before this was
 * put in, and nothing else in the test could see it.
 *
 * Same reason and same shape as `Queue`'s `copy1`; finding 247 has the general
 * statement.  A future reader who restores the member-init list will pass every
 * case except the one that matters.
 */
template <class T>
static inline void store(T *dst, T v)
{
	if (sizeof(T) == sizeof(unsigned)) {
		unsigned tmp;

		__builtin_memcpy(&tmp, &v, sizeof(unsigned));
		__asm__("" : "+r" (tmp));
		__builtin_memcpy(dst, &tmp, sizeof(unsigned));
	} else {
		*dst = v;
	}
}

template <class Tout, class Tparam>
SineWave<Tout, Tparam>::SineWave(Tparam a, Tparam f, Tparam p, Tparam sr)
{
	store(&amplitude, a);
	store(&frequency, f);
	store(&phase, p);
	store(&sampleRate, sr);
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
			 * `fsin` by inline asm, not `sin()`.  GCC only emits
			 * the instruction under -funsafe-math-optimizations,
			 * and the argument is the UNROUNDED 80-bit register
			 * value -- the `fsts` below rounds a copy for memory
			 * and the sine is taken of what is still in st(0).
			 * Feeding `fsin` the float-rounded phase instead is a
			 * real difference: 17,650 mismatches.
			 *
			 * Above |x| = 2^63 the instruction sets C2 and leaves
			 * st(0) alone, so the "sine" comes back as the angle.
			 * The object does not test C2 and neither does this.
			 */
			__asm__ ("fsin" : "=t" (s) : "0" (x));
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
	 * same trap as the `de f1` divide, finding 245.
	 */
	{
		int t = (int)(x * ik);

		phase = (Tparam)(x - (long double)t * k);
	}
}

template class SineWave<float, float>;
