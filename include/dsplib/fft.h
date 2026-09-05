/*
 * fft.h -- the object's complex and real FFT pair.
 *
 * Reconstructed from dsplibs.o.  The original translation unit is called
 * `fft.cpp`: STT_FILE entry #290, bracketed between `V90CP.cpp` (whose last
 * symbol ends at 0x536c0) and `V92Transmitter.cpp` (which starts at
 * 0x53a30), and the only two symbols in that gap are these.  So the file
 * holds exactly this pair and nothing else.
 *
 * BOTH ARRAYS ARE ONE-BASED.  `data[0]` is never read and never written by
 * either function; the transform occupies `data[1 .. 2*nn]` for `four1` and
 * `data[1 .. n]` for `realfft`.  That is the Numerical Recipes convention,
 * and `Psd` already allocates `m_length + 1` floats for it
 * (include/dsplib/Psd.h).
 *
 * NEITHER FUNCTION VALIDATES ANYTHING.  There is no power-of-two check, no
 * null check and no length check in either body -- the disassembly enters
 * its first loop on the raw argument.  A length that is not a power of two
 * does not fault; it computes a wrong answer, and it does so identically on
 * both sides only up to the reciprocal-math argument in src/dsp/fft.cpp.
 * Do not test one.
 *
 * `unsigned long` and not `size_t`: the mangled names are `_Z5four1Pfmi` and
 * `_Z7realfftPfmi`, and `m` is `unsigned long`.  `size_t` mangles as `j`
 * (`unsigned int`) on this target and would not link against the blob.
 */

#ifndef DSPLIB_FFT_H
#define DSPLIB_FFT_H

/**
 * @brief In-place complex DFT, Numerical Recipes' `four1`.
 *
 * @param data   One-based: `data[1..2*nn]` holds @p nn complex numbers as
 *               consecutive (real, imaginary) pairs. `data[0]` is never
 *               touched. Not validated -- @p nn must be a power of two or
 *               the result is silently wrong, not faulted.
 * @param nn     Number of complex points, a power of two.
 * @param isign  +1 for the forward transform, -1 for the inverse. The
 *               inverse is NOT scaled by 1/nn; the caller owns normalisation.
 */
void four1(float *data, unsigned long nn, int isign);

/**
 * @brief In-place real DFT, Numerical Recipes' `realfft`.
 *
 * Calls four1() and nothing else.
 *
 * @param data   One-based, `data[1..n]`. With @p isign == 1 the @p n real
 *               input samples are replaced by `n/2` complex values:
 *               `data[1]`/`data[2]` hold the purely real zero and Nyquist
 *               terms, and `data[2k+1]`/`data[2k+2]` the k'th complex
 *               coefficient. With @p isign == -1 that packing is transformed
 *               back, scaled by 2/n short of the original. Not validated --
 *               @p n must be a power of two or the result is silently wrong.
 * @param n      Number of real points, a power of two.
 * @param isign  +1 to pack real to complex, -1 to unpack complex to real.
 */
void realfft(float *data, unsigned long n, int isign);

#endif /* DSPLIB_FFT_H */
