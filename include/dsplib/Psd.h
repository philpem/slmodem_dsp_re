/*
 * Psd.h -- Welch periodogram: overlapped, windowed, magnitude-squared.
 *
 * Reconstructed from dsplibs.o.  Six members, 900 bytes by `nm` because the
 * constructor and destructor each appear twice, byte-identical -- C1/C2 and
 * D1/D2 -- which GCC emits from one definition.  NOT POLYMORPHIC, by the same
 * argument as FloatFIR: no deleting destructor, so offset 0 is a real member.
 *
 * FIVE OF THE SIX ARE HERE.  `Psd::process` is NOT reconstructed and its
 * declaration below has no definition anywhere in `src/`.  Two independent
 * blockers, and clearing either alone does not unblock it -- see finding 876:
 *
 *   1. it calls `realfft`, which calls `four1`; neither is written, and the
 *      Makefile renames every symbol the blob defines to `ref_*`, so a
 *      reference to `realfft` from our side resolves to nothing.  Confirmed
 *      against `build/dsplibs_ref.o`, not inferred from the Makefile.
 *   2. two of its four output arms compute log10 with `fldlg2`/`fyl2x`, which
 *      GCC emits only under `-funsafe-math-optimizations` -- not in this
 *      tree's derived flag set -- so our build would call libm and the two
 *      would differ in the last bit.
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

	/*
	 * `length` is the segment and transform length; `overlap` is how many
	 * samples each segment shares with the one before it, and is used
	 * only by `process`.  Two allocations, window first, and neither is
	 * checked -- `designWindow` writes through the first immediately.
	 */
	Psd(unsigned int length, WindowType window, unsigned int overlap);
	~Psd();

	void setOverlapLength(unsigned int overlap);
	void setWindowType(WindowType window);

	/* freq[i] = i * sampleRate / length, for i < length / 2. */
	void getFrequencies(float *freq, float sampleRate) const;

	/* DECLARED, NOT DEFINED.  See the header comment above. */
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
