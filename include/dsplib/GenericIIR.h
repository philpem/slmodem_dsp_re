/*
 * GenericIIR.h -- Generic IIR filter, templated on sample and coefficient type.
 *
 * Reconstructed from dsplibs.o FloatIIR.cpp.  The original ships exactly one
 * instantiation, GenericIIR<float, double>: float samples with double
 * coefficients and a double accumulator, which is the usual arrangement for a
 * fixed-topology filter whose poles sit close to the unit circle.
 *
 * Difference equation, as implemented:
 *
 *     acc = sum(num[i] * x[n-i], i = 0 .. nnum-1)
 *         - sum(den[i] * y[n-i], i = 1 .. nden-1)
 *     y[n] = (den[0] != 0) ? acc / den[0] : acc
 *
 * Note the den[0] convention: the original divides only when den[0] is
 * *non-zero*, so passing den[0] == 0 is how a caller says "already
 * normalised, skip the divide".  That reads backwards -- one would expect the
 * test to be against 1.0 -- but it is what the object does, and a caller
 * passing a genuine den[0] of 0 would be describing a filter with no output
 * term at all, which is meaningless.  Reproduced exactly.
 *
 * History buffers grow *downward*: the newest sample sits at the lowest index
 * and the write position decrements.  When it would pass below zero the most
 * recent (n-1) entries are copied back to the top of the buffer and the
 * position resets.  Each buffer is over-allocated by `blockSize` entries so a
 * whole block can be processed between compactions.
 *
 * (FixedRC uses the same trick in the opposite direction -- see
 * src/core/FixedRC.c.  Both trade one memmove per block for an inner loop
 * with no index wrapping.)
 */

#ifndef DSPLIB_GENERICIIR_H
#define DSPLIB_GENERICIIR_H

/* For `sysdep_malloc`, `sysdep_free` and `size_t`; see the note on the
 * allocation operators below. */
#include "dsplib/sysdep.h"

template <typename Sample, typename Coeff>
class GenericIIR {
public:
	/*
	 * `new` AND `delete` ARE MEMBERS AND THEY CALL sysdep_malloc/free.
	 *
	 * This is not a convenience: it is what the one caller in the blob
	 * that heap-allocates a `GenericIIR<float, double>` compiles to.
	 * `GenericToneDetector`'s constructor at 0x10690 opens with
	 *
	 *     movl $0x34,(%esp)
	 *     call sysdep_malloc          <- R_386_PC32, a direct call
	 *     mov  %eax,%ebp
	 *     ...
	 *     call _ZN10GenericIIRIfdEC1EjjPdS1_j
	 *
	 * and its destructor at 0x102f0 does the mirror image: test the
	 * pointer, call `D1`, then `call sysdep_free`.  0x34 is 52, which is
	 * exactly `sizeof(GenericIIR<float, double>)` as laid out below.
	 *
	 * Three things in that sequence are load-bearing.  The allocator is
	 * called DIRECTLY and not through `_Znwj` -- there is not one `_Znw*`
	 * or `_Zdl*` symbol in the whole 1.2 MB object, and the tree links
	 * test binaries with $(CC) and no libstdc++, so a reference to the
	 * default `::operator delete` would leave an undefined `_ZdlPvj` and
	 * break EVERY test binary rather than just this class's.  There is NO
	 * NULL CHECK between the allocation and the constructor call, which is
	 * what a plain (non-`throw()`) `operator new` gives: the compiler is
	 * entitled to assume it never returns null.  And the DESTRUCTOR does
	 * test for null before calling `D1`, which is what a delete-expression
	 * gives and a hand-written free would not.
	 *
	 * `Resampler.h` reaches the same conclusion for its own hierarchy from
	 * the same evidence and records the `nm` measurement in both
	 * directions; this is that argument applied to the one template the
	 * object instantiates.
	 *
	 * THE BLOB HAS NO `nwEj`/`dlEPv` SYMBOL FOR THIS TEMPLATE, AND OURS
	 * DOES.  That is worth saying plainly rather than leaving to be
	 * discovered.  Adding the two operators below put
	 * `_ZN10GenericIIRIfdEnwEj` and `_ZN10GenericIIRIfdEdlEPv` into
	 * `build/src/dsp/FloatIIR.o` -- measured, twenty-one symbols before and
	 * twenty-three after, those two being the whole difference.  It is not
	 * evidence against the reading, because the same object ALREADY carries
	 * four symbols the blob does not, for the same reason: `FloatIIR.cpp`
	 * ends with `template class GenericIIR<float, double>;`, and an
	 * EXPLICIT instantiation emits every member whether or not anything
	 * calls it out of line.  The blob's five --
	 * `reset`, both `process`es, `C1` and `D1`, and no `C2`, no `D2`, no
	 * `compactIn`, no `compactOut` -- are the signature of an IMPLICIT
	 * instantiation, which emits only what is referenced.  A member
	 * `operator new` that is inlined at its one call site leaves no symbol
	 * under implicit instantiation and an unreferenced one under explicit.
	 * So the difference is a property of our instantiation style, which
	 * predates this note, and not of the reading of the constructor.
	 */
	static void *operator new(size_t n) { return sysdep_malloc(n); }
	static void operator delete(void *p) { sysdep_free(p); }

	/**
	 * @brief Construct a filter over borrowed coefficient arrays.
	 *
	 * Coefficient arrays are borrowed, not copied -- the caller keeps
	 * ownership and must outlive the filter. Allocates the input/output
	 * history buffers (sized with `blockSize` headroom) and tail-calls
	 * reset(); the scratch members `m_i`/`m_acc` are left uninitialised
	 * by everything before that tail call, matching the object.
	 *
	 * @param nden       Number of denominator (feedback) coefficients.
	 * @param nnum       Number of numerator (feedforward) coefficients.
	 * @param den        Denominator coefficients, `nden` of them;
	 *                   `den[0]` doubles as the output divisor (see
	 *                   process()).
	 * @param num        Numerator coefficients, `nnum` of them.
	 * @param blockSize  Largest sample count the block process() will be
	 *                   called with; only sets the headroom between
	 *                   buffer compactions.
	 */
	GenericIIR(unsigned nden, unsigned nnum, Coeff *den, Coeff *num,
		   unsigned blockSize);

	/** @brief Free the input and output history buffers. */
	~GenericIIR();

	/**
	 * @brief Clear both history buffers and rewind the write positions.
	 *
	 * Also leaves `m_i` holding `m_outLen` (or zero if there is no
	 * output history) rather than whatever the last process() left
	 * there -- the object's own observable behaviour, since both loops
	 * here count in the `m_i` member rather than a local.
	 */
	void reset();

	/**
	 * @brief Filter one input sample and produce one output sample.
	 *
	 * Computes `acc = sum(num[i]*x[n-i]) - sum(den[i]*y[n-i], i>=1)` at
	 * extended precision, dividing by `den[0]` only when it is non-zero
	 * (zero means "already normalised" -- see the file comment above).
	 * Rounds to `Coeff` once, at the end. Advances (and compacts, via
	 * compactIn()/compactOut(), when a history buffer's slack runs out)
	 * both history buffers' write positions.
	 *
	 * @param x  The new input sample.
	 * @return   The filtered output sample.
	 */
	Sample process(Sample x);

	/**
	 * @brief Filter @p count samples; equivalent to that many calls to
	 *        the single-sample process().
	 * @param in     Input samples, @p count of them.
	 * @param out    Output buffer, @p count entries.
	 * @param count  Number of samples to filter.
	 */
	void process(const Sample *in, Sample *out, unsigned count);

private:
	/**
	 * @brief Compact the input history: move the most recent `nnum - 1`
	 *        entries back to the top of the buffer and rewind `m_inPos`.
	 *
	 * Copied high-to-low since source and destination overlap when the
	 * slack is smaller than the history; order is preserved (newest
	 * stays newest).
	 */
	void compactIn();

	/** @brief Compact the output history; see compactIn() for the shape. */
	void compactOut();

	/*
	 * Field order matches the original's 52-byte object layout, so the two
	 * can be compared field by field during differential testing.
	 */
	Coeff *m_den;		/* +0x00 denominator, m_den[0] is the divisor  */
	Coeff *m_num;		/* +0x04 numerator                             */
	Coeff *m_inHist;	/* +0x08 input history,  m_inLen entries       */
	Coeff *m_outHist;	/* +0x0c output history, m_outLen entries      */
	unsigned m_nden;	/* +0x10                                       */
	unsigned m_nnum;	/* +0x14                                       */
	unsigned m_inLen;	/* +0x18 m_nnum + blockSize                    */
	unsigned m_outLen;	/* +0x1c m_nden + blockSize                    */
	unsigned m_inPos;	/* +0x20 next input write index                */
	unsigned m_outPos;	/* +0x24 next output write index               */
	unsigned m_i;		/* +0x28 loop counter, a member in the original */
	Coeff m_acc;		/* +0x2c accumulator, likewise                 */
};

#endif /* DSPLIB_GENERICIIR_H */
