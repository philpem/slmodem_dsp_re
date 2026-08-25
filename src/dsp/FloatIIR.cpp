/*
 * FloatIIR.cpp -- Floating-point IIR: the GenericIIR instantiation the blob uses.
 *
 * Reconstructed from dsplibs.o FloatIIR.cpp:
 *   .gnu.linkonce.t._ZN10GenericIIRIfdEC1EjjPdS1_j   constructor
 *   .gnu.linkonce.t._ZN10GenericIIRIfdE5resetEv      reset
 *   .gnu.linkonce.t._ZN10GenericIIRIfdE7processEf    process(float)
 *   .gnu.linkonce.t._ZN10GenericIIRIfdE7processEPKfPfj  block process
 *   .gnu.linkonce.t._ZN10GenericIIRIfdED1Ev          destructor
 *
 * THE FLOATIIR HALF'S DEFINITION ORDER IS THE OBJECT'S.  `process(const float
 * *, float *, unsigned)` comes FIRST, then the constructor, destructor,
 * `reset` and `setCoefficients` -- which emits as process(Pf), reset, C2, C1,
 * D2, D1, setCoefficients, with `reset` hoisted above the constructor that
 * calls it.  FloatARMA.cpp has the identical shape and the same reason.
 * All 5! = 120 orderings were compiled: three distinct emissions, and 20 of
 * them both close `GenericIIR<float,double>::reset` and keep the four symbols
 * that were already exact.  So what is decoded is the FACT that process(Pf)
 * precedes the constructor, not a unique order; the blob's own `nm -n` picks
 * this member of the class, and the file went 5 of 12 to 7 of 12 (only the
 * relative order is recoverable -- 1,008 of the 1,020 blob symbols spanning
 * this file belong to other translation units).
 *
 * Note WHICH symbol that gained: `GenericIIR<float,double>::reset` sits ABOVE
 * the `#include "dsplib/FloatIIR.h"` and was not moved.  It closed as a
 * bystander of a permutation aimed below it, which is lever 3's "the carrier
 * is upstream of the function, not its own index" seen from the other side.
 *
 * The original was built -fno-exceptions -fno-rtti with no use of new/delete;
 * allocation goes through sysdep_malloc/sysdep_free like the rest of the
 * library.  This file keeps that, so it drops into the same environment.
 *
 * A note on floating-point reproducibility, which took a wrong turn worth
 * recording.
 *
 * The original mirrors its accumulator to memory after every
 * multiply-accumulate (`fstl 0x2c(%ecx)`), which looks like it rounds each
 * intermediate to 64-bit double.  It does not: `fstl` stores *without popping*,
 * so the running sum stays in the x87 register at full 80-bit extended
 * precision for the next iteration.  The memory write only keeps the member
 * up to date.  Rounding to double happens exactly once, when the finished
 * accumulator is read back for the result.
 *
 * So the faithful reconstruction accumulates at extended precision and rounds
 * at the end -- which is what an x87 target does naturally.  Building this
 * file with -ffloat-store was the wrong fix: it also forces the *product*
 * `num[i] * hist[i]` back to double before the add, which the original never
 * does, and that showed up as 1-ULP differences on an 8-tap FIR (35 samples
 * in 3200).  Short filters hid it; longer ones did not.
 */

#include "dsplib/GenericIIR.h"

extern "C" {
void *sysdep_malloc(unsigned size);
void sysdep_free(void *ptr);
}

/*
 * THE REPLACEMENT `operator delete[]`, AND IT IS READ OFF THE OBJECT.  The
 * blob makes an ordinary `call sysdep_free` at a destructor's LAST free where
 * an explicit `if (p) sysdep_free(p)` makes a sibling `jmp` -- one instruction
 * fewer at the same byte count.  Eight spellings were compiled and only
 * `delete[]` over an inline replacement reproduces the object's shape; see the
 * finding for the enumeration.  Behaviourally it is exactly the guard it
 * replaces: `float` has no destructor, so `delete[] p` is `if (p)
 * operator delete[](p)` with no array cookie.
 */
inline void operator delete[](void *p) { sysdep_free(p); }

template <typename Sample, typename Coeff>
GenericIIR<Sample, Coeff>::GenericIIR(unsigned nden, unsigned nnum,
				      Coeff *den, Coeff *num,
				      unsigned blockSize)
{
	m_den = den;
	m_num = num;
	m_nden = nden;
	m_nnum = nnum;

	/*
	 * Over-allocate by blockSize so a whole block fits between
	 * compactions.  The history is `n` deep; the slack is what lets the
	 * write position walk downward without a wrap test per sample.
	 */
	m_inLen = nnum + blockSize;
	m_outLen = nden + blockSize;

	m_inHist = 0;
	m_outHist = 0;
	m_inHist = (Coeff *)sysdep_malloc(m_inLen * sizeof(Coeff));
	m_outHist = (Coeff *)sysdep_malloc(m_outLen * sizeof(Coeff));

	/*
	 * NEITHER SCRATCH MEMBER IS INITIALISED HERE, and that is the object's
	 * and not an omission.  `_ZN10GenericIIRIfdEC1EjjPdS1_j` writes exactly
	 * +0x00, +0x04, +0x10, +0x14, +0x18, +0x1c and the two history pointers
	 * and then TAIL-CALLS `reset` -- `jmp` at +0x66 -- so +0x28 and +0x2c
	 * are never stored, and `m_acc` comes out of the constructor holding
	 * whatever the allocator left.  `m_i` gets its value from `reset`.
	 *
	 * This is the second half of finding F1250, which recorded both stores
	 * as a known divergence because repairing them belonged to this class
	 * rather than to `GenericToneDetector`.
	 */
	reset();
}

template <typename Sample, typename Coeff>
GenericIIR<Sample, Coeff>::~GenericIIR()
{
	delete[] m_inHist;
	delete[] m_outHist;
}

/*
 * BOTH LOOPS COUNT IN `m_i`, WHICH IS A MEMBER AND NOT A LOCAL.  The object
 * is explicit about it -- `movl $0x0,0x28(%ecx)` at +0x0d and +0x38 and
 * `mov %eax,0x28(%ecx)` at +0x20 and +0x64 of
 * `_ZN10GenericIIRIfdE5resetEv` -- so a reset filter is left with `m_i`
 * holding `m_outLen`, or zero when there is no output history, rather than
 * with whatever the last `process` left there.  GenericIIR.h already said
 * +0x28 was the original's loop counter; this is the site that shows it.
 *
 * The object's first loop stores the counter back only when it is about to
 * go round again, so it leaves `m_inLen - 1` there -- and then overwrites it
 * with zero unconditionally before the second loop, which is why writing the
 * two loops the obvious way reproduces every observable value, including the
 * two cases where a length is zero.
 *
 * Found by `test/unit/t_v34pcmapi.cpp`: `VPcmV34InitMOH` resets the session's
 * `GenericToneDetector`, whose own `reset` calls this one, and the whole-block
 * comparison against the blob differed at +0x28 and nowhere else.
 */
template <typename Sample, typename Coeff>
void
GenericIIR<Sample, Coeff>::reset()
{
	for (m_i = 0; m_i < m_inLen; m_i++)
		m_inHist[m_i] = 0;
	for (m_i = 0; m_i < m_outLen; m_i++)
		m_outHist[m_i] = 0;

	/* Start at the top of the slack, leaving `n` entries of history above. */
	m_inPos = m_inLen - m_nnum;
	m_outPos = m_outLen - m_nden;
}

/*
 * Move the most recent (n-1) entries back to the top of the buffer and reset
 * the write position beneath them.  Order is preserved: the newest stays
 * newest.  Copied high-to-low because source and destination overlap when the
 * slack is smaller than the history.
 */
template <typename Sample, typename Coeff>
void
GenericIIR<Sample, Coeff>::compactIn()
{
	unsigned k;

	m_inPos = m_inLen - m_nnum;
	for (k = 0; k + 1 < m_nnum; k++)
		m_inHist[m_inLen - 1 - k] = m_inHist[m_nnum - 2 - k];
}

template <typename Sample, typename Coeff>
void
GenericIIR<Sample, Coeff>::compactOut()
{
	unsigned k;

	m_outPos = m_outLen - m_nden;
	for (k = 0; k + 1 < m_nden; k++)
		m_outHist[m_outLen - 1 - k] = m_outHist[m_nden - 2 - k];
}

template <typename Sample, typename Coeff>
Sample
GenericIIR<Sample, Coeff>::process(Sample x)
{
	/*
	 * Extended precision throughout, rounded to Coeff only on the way out
	 * -- see the note at the top of this file.  On x87 this is the natural
	 * evaluation width, so it costs nothing and matches the original bit
	 * for bit.
	 */
	long double acc = 0;

	m_inHist[m_inPos] = x;

	/* Feedforward: the numerator over the input history. */
	for (m_i = 0; m_i < m_nnum; m_i++)
		acc = acc + m_num[m_i] * m_inHist[m_inPos + m_i];

	/* Feedback: the denominator over the output history, from index 1. */
	for (m_i = 1; m_i < m_nden; m_i++)
		acc = acc - m_den[m_i] * m_outHist[m_outPos + m_i];

	/* See the header: a zero den[0] means "already normalised". */
	if (m_den[0] != 0)
		acc = acc / m_den[0];

	m_acc = (Coeff)acc;
	m_outHist[m_outPos] = m_acc;

	if (m_outPos == 0)
		compactOut();
	else
		m_outPos--;

	if (m_inPos == 0)
		compactIn();
	else
		m_inPos--;

	return (Sample)m_acc;
}

template <typename Sample, typename Coeff>
void
GenericIIR<Sample, Coeff>::process(const Sample *in, Sample *out,
				   unsigned count)
{
	for (unsigned k = 0; k < count; k++)
		out[k] = process(in[k]);
}

/*
 * The only instantiation the original contains.  Emitted explicitly rather
 * than relying on implicit instantiation, so the symbols exist for the
 * differential test to link against.
 */
template class GenericIIR<float, double>;

/*
 * ---------------------------------------------------------------------------
 * FloatIIR -- the concrete all-pole class that shares this translation unit.
 *
 * Nothing to do with the GenericIIR instantiation above beyond the file they
 * were compiled into.  See include/dsplib/FloatIIR.h for the shape.
 */

#include "dsplib/FloatIIR.h"

void
FloatIIR::process(const float *in, float *out, unsigned count)
{
	if (count == 0)
		return;

	while (count-- != 0) {
		const float *h = m_hist + m_pos;
		const float *c = m_coeff;
		unsigned n = m_ncoeff;
		int pos = m_pos - 1;

		/*
		 * TWO ACCUMULATORS, because the original has two -- and NOT
		 * because anything here can tell the difference.
		 *
		 * The original interleaves its four-way unrolled body between
		 * them, taps 0 and 2 into one and 1 and 3 into the other, then
		 * adds them with the odd sum on the left.  That is a different
		 * association order from a single accumulator, and floating
		 * point addition is not associative, so it ought to be
		 * observable.
		 *
		 * IT IS NOT, on anything tried.  A single-accumulator build
		 * passes the whole of t_floatiir: six configurations of 512
		 * samples, and a 262,144-sample stress run with poles near the
		 * unit circle and inputs spanning two decades.  The reason is
		 * that sixteen products of similar magnitude sum inside the
		 * x87's 64-bit significand without rounding at all, so both
		 * orders are exact and the single rounding to float at the end
		 * sees the same number.
		 *
		 * A standalone experiment DID separate them -- 3 samples in
		 * 200,000 -- but only by letting two independent filters run
		 * until their histories diverged, with the input magnitude
		 * growing without bound.  That is not this filter.
		 *
		 * So this is an equivalent mutant as far as the suite goes,
		 * and what was held fixed is: bounded input, sixteen or fewer
		 * taps, coefficients of similar magnitude.  Written the
		 * original's way regardless, because matching the object is
		 * the point and a reader should not have to rediscover that
		 * the split was deliberate.
		 *
		 * Everything here stays in x87 registers at 80-bit extended
		 * precision and rounds to float exactly once, where `y` is
		 * assigned.  That is what the hardware does unaided; do not
		 * build this file with -ffloat-store, which is the wrong fix
		 * and is recorded as such in the GenericIIR note above.
		 */
		float even = 0.0f;
		float odd = 0.0f;

		for (; n > 3; n -= 4) {
			even += h[0] * c[0];
			odd  += h[1] * c[1];
			even += h[2] * c[2];
			odd  += h[3] * c[3];
			h += 4;
			c += 4;
		}
		/* The tail cannot happen while m_ncoeff is a multiple of
		 * four, and the original still emits it. */
		for (; n != 0; n--)
			even += *h++ * *c++;

		float y = (odd + even) + *in++;
		*out = y;

		if (pos < 0) {
			/*
			 * Out of room below.  Copy the newest m_ncoeff-1
			 * samples to the top of the buffer and rewind.
			 *
			 * A do-while, exactly as the original: with m_ncoeff
			 * zero the counter starts at -1 and this runs 2^32
			 * times.  m_ncoeff is a multiple of four so that means
			 * a zero-tap filter, which is degenerate anyway, but
			 * the loop is reproduced as written.  D57.
			 */
			float *dst = m_hist + m_len - 1;
			const float *src = m_hist + m_ncoeff - 2;
			unsigned i = m_ncoeff - 1;

			do {
				*dst-- = *src--;
			} while (--i != 0);

			pos = (int)(m_len - m_ncoeff);
		}

		m_hist[pos] = y;
		m_pos = pos;
		out++;
	}
}

FloatIIR::FloatIIR(unsigned ncoeff, float *coeff, unsigned blockSize)
{
	m_ncoeff = ncoeff & ~3u;
	m_coeff = coeff;
	m_len = m_ncoeff + blockSize;
	m_hist = (float *)sysdep_malloc(m_len * sizeof(float));

	/*
	 * A failed allocation leaves m_hist null and is NOT reported: the
	 * constructor completes, and the first `process` faults.  Reproduced
	 * rather than fixed -- D55.
	 */
	if (m_hist != 0) {
		for (unsigned i = 0; i < m_len; i++)
			m_hist[i] = 0.0f;
	}
	m_pos = (int)(m_len - m_ncoeff);
}

FloatIIR::~FloatIIR()
{
	/* m_hist is not nulled, so a second delete double-frees.  D56. */
	delete[] m_hist;
}

void
FloatIIR::reset()
{
	if (m_hist != 0) {
		for (unsigned i = 0; i < m_len; i++)
			m_hist[i] = 0.0f;
	}
	m_pos = (int)(m_len - m_ncoeff);
}

int
FloatIIR::setCoefficients(float *coeff, unsigned ncoeff)
{
	unsigned n = ncoeff & ~3u;

	/*
	 * The check is against the WHOLE buffer, not the slack, so a tap
	 * count equal to m_len is refused and one below it is accepted --
	 * leaving a single sample of block room.
	 */
	if (m_len <= n)
		return -1;

	m_coeff = coeff;
	if (m_ncoeff == n)
		return 0;		/* pointer swapped, geometry unchanged */

	m_ncoeff = n;

	/*
	 * The write position is clamped, not rewound: growing the tap count
	 * shrinks the room above it, and a position already inside that room
	 * is pulled down to the new limit.  A position below it is left alone,
	 * so the history in flight survives the change.
	 */
	if (m_pos > (int)(m_len - n))
		m_pos = (int)(m_len - n);
	return 0;
}
