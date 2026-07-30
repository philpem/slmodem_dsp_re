/*
 * FloatIIR.cpp -- GenericIIR implementation and the one instantiation the
 * original ships.
 *
 * Reconstructed from dsplibs.o FloatIIR.cpp:
 *   .gnu.linkonce.t._ZN10GenericIIRIfdEC1EjjPdS1_j   constructor
 *   .gnu.linkonce.t._ZN10GenericIIRIfdE5resetEv      reset
 *   .gnu.linkonce.t._ZN10GenericIIRIfdE7processEf    process(float)
 *   .gnu.linkonce.t._ZN10GenericIIRIfdE7processEPKfPfj  block process
 *   .gnu.linkonce.t._ZN10GenericIIRIfdED1Ev          destructor
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

	m_i = 0;
	m_acc = 0;

	reset();
}

template <typename Sample, typename Coeff>
GenericIIR<Sample, Coeff>::~GenericIIR()
{
	if (m_inHist)
		sysdep_free(m_inHist);
	if (m_outHist)
		sysdep_free(m_outHist);
}

template <typename Sample, typename Coeff>
void
GenericIIR<Sample, Coeff>::reset()
{
	unsigned k;

	for (k = 0; k < m_inLen; k++)
		m_inHist[k] = 0;
	for (k = 0; k < m_outLen; k++)
		m_outHist[k] = 0;

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
