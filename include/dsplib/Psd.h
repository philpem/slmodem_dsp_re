/*
 * Psd.h -- Welch periodogram: overlapped, windowed, magnitude-squared.
 *
 * Reconstructed from dsplibs.o.  Six members, 900 bytes by `nm` because the
 * constructor and destructor each appear twice, byte-identical -- C1/C2 and
 * D1/D2 -- which GCC emits from one definition.  NOT POLYMORPHIC, by the same
 * argument as FloatFIR: no deleting destructor, so offset 0 is a real member.
 *
 * ALL SIX ARE NOW HERE.  `Psd::process` was blocked twice and both blockers
 * are gone; the record of what they were is worth keeping, because the second
 * of them is a shape this tree meets repeatedly (finding F876):
 *
 *   1. it calls `realfft`, which calls `four1`.  Neither was written, and the
 *      Makefile renames every symbol the blob defines to `ref_*`, so a
 *      reference to `realfft` from our side resolved to nothing.  Both are
 *      written now, in src/dsp/fft.cpp.
 *   2. two of its three output arms compute log10 with `fldlg2`/`fyl2x`,
 *      which GCC emits only under `-funsafe-math-optimizations` -- not in
 *      this tree's derived flag set, and not a flag to reach for, because it
 *      changes every other expression in the translation unit too.  A libm
 *      `log10` is a different function in the last place.  psd.cpp uses an
 *      inline-asm helper that is the object's own two instructions, as
 *      V90Equalizer.cpp and VpcmFloModem.cpp already do.
 *
 * THE WINDOW TYPE IS NOT STORED.  `setWindowType` redesigns the window in
 * place and keeps nothing, so the object cannot be asked which one it holds.
 *
 * THE SPECTRUM BUFFER IS ONE-BASED.  It is allocated with `m_length + 1`
 * floats and the windowed segment is written from index 1, which is the
 * Numerical Recipes convention `realfft` expects; `m_fft[0]` is never
 * written.  Neither buffer is cleared by the constructor -- only `m_window`
 * is filled, by `designWindow`, so `m_fft` holds allocator garbage until the
 * first `process`.
 */

#ifndef DSPLIB_PSD_H
#define DSPLIB_PSD_H

#include "dsplib/DspMath.h"	/* WindowType, designWindow<T> */

class Psd {
public:
	/*
	 * What `process` leaves in the caller's output array.  The type name
	 * is the mangling's (`Psd::OutputOption`); an enum's ENUMERATORS are
	 * not mangled, so the three VALUES below are read out of the compare
	 * chain at the end of `process` -- `cmp $1` / `jle` / `cmp $2`, a
	 * SIGNED chain, with anything outside 0..2 falling through and
	 * leaving the accumulated sum untouched.  The names are invented.
	 *
	 *   OUTPUT_DB          10 * log10(sum / segments + 1e-25)
	 *   OUTPUT_DB_PEAK     10 * log10(sum / max(sum) + 1e-25)
	 *   OUTPUT_LINEAR      sum / segments
	 */
	enum OutputOption {
		OUTPUT_DB = 0,
		OUTPUT_DB_PEAK = 1,
		OUTPUT_LINEAR = 2
	};

	/**
	 * @brief Construct a periodogram over segments of @p length samples.
	 *
	 * Allocates the window and spectrum buffers (window first; neither
	 * allocation is checked -- designWindow() writes through the first
	 * immediately) and designs the window. The spectrum buffer is left
	 * uninitialised (allocator garbage) until the first process().
	 *
	 * @param length   Segment and transform length.
	 * @param window   Window shape to design.
	 * @param overlap  Samples each segment shares with the one before it;
	 *                 used only by process().
	 */
	Psd(unsigned int length, WindowType window, unsigned int overlap);

	/** @brief Free the window and spectrum buffers. */
	~Psd();

	/** @brief Change the overlap between segments. @param overlap New value. */
	void setOverlapLength(unsigned int overlap);

	/**
	 * @brief Redesign the window in place.
	 *
	 * The window type itself is not stored -- this rebuilds the window
	 * coefficients and keeps nothing else, so the object cannot later be
	 * asked which type it holds.
	 *
	 * @param window  New window shape.
	 */
	void setWindowType(WindowType window);

	/**
	 * @brief Fill in the frequency (Hz) of each output bin.
	 * @param freq        Output buffer, `length / 2` entries.
	 * @param sampleRate  Sample rate in Hz.
	 */
	void getFrequencies(float *freq, float sampleRate) const;

	/**
	 * @brief Compute the Welch periodogram of @p in.
	 *
	 * Splits @p in into overlapped, windowed segments, transforms each
	 * with realfft() and accumulates squared magnitude per bin, then
	 * scales the accumulated bins per @p option (see #OutputOption).
	 *
	 * @param in      Input samples, enough for at least one segment.
	 * @param count   Number of samples in @p in.
	 * @param out     Output spectrum, `length / 2` bins.
	 * @param option  How to scale/report each output bin.
	 */
	void process(float *in, unsigned int count, float *out,
		     OutputOption option);

	/*
	 * Public for the reason given in include/dsplib/FloatFIR.h; the names
	 * are invented, the offsets are the object's and the .cpp asserts
	 * them.
	 */
	unsigned int m_length;	/* +0x00 segment / transform length     */
	unsigned int m_overlap;	/* +0x04 samples shared between segments */
	float *m_window;	/* +0x08 owned, m_length entries         */
	float *m_fft;		/* +0x0c owned, m_length + 1 entries     */
};				/* 0x10 bytes                            */

#endif /* DSPLIB_PSD_H */
