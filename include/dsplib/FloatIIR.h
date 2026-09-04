/*
 * FloatIIR.h -- All-pole IIR filter over float samples.
 *
 * NOT the same thing as the `GenericIIR<float, double>` instantiation that
 * shares this translation unit and, confusingly, this file's name.  The blob
 * holds both:
 *
 *     GenericIIR<float,double>   5 members, 1,269 bytes, weak/linkonce
 *     FloatIIR                   5 members,   575 bytes, ordinary globals
 *
 * `GenericIIR` is the biquad-style filter with separate numerator and
 * denominator histories.  This one is all-pole and much simpler: a single
 * coefficient array, a single history buffer, and the output fed back.
 *
 *     y[n] = x[n] + SUM(k = 0 .. ncoeff-1) coeff[k] * y[n-1-k]
 *
 * THE HISTORY BUFFER RUNS BACKWARDS, which is worth having in front of you
 * before reading `process`.  `m_pos` starts at `m_len - m_ncoeff` and walks
 * DOWN, so the newest sample is at the lowest index and the taps are read
 * upwards from it.  When `m_pos` would go below zero the newest `m_ncoeff-1`
 * samples are copied to the top of the buffer and `m_pos` resets -- the
 * usual double-buffer trick, which is why `m_len` is `m_ncoeff` plus a block
 * size rather than just `m_ncoeff`.
 *
 * `m_ncoeff` is always rounded DOWN to a multiple of four, by both the
 * constructor and `setCoefficients`, because the dot product is unrolled four
 * ways.  Asking for 6 taps gets 4.
 */

#ifndef DSPLIB_FLOATIIR_H
#define DSPLIB_FLOATIIR_H

class FloatIIR {
public:
	/**
	 * @brief Construct an all-pole IIR filter.
	 * @param ncoeff     Tap count, rounded down to a multiple of four
	 *                   (the dot product is unrolled four ways).
	 * @param coeff      Feedback coefficients, @p ncoeff entries; not
	 *                   copied, not owned.
	 * @param blockSize  Slack above @p ncoeff in the history buffer,
	 *                   deciding how often it is compacted -- not part
	 *                   of the response. The history is allocated here
	 *                   and zeroed; a failed allocation is not reported
	 *                   and process() would fault on it (docs/deviations.md).
	 */
	FloatIIR(unsigned ncoeff, float *coeff, unsigned blockSize);
	~FloatIIR();

	/** @brief Zero the history buffer and rewind the write position. */
	void reset();

	/**
	 * @brief Point at a new coefficient array, and optionally change the
	 * tap count.
	 * @param coeff   New feedback coefficients; not copied, not owned.
	 * @param ncoeff  New tap count, rounded down to a multiple of four.
	 * @return 0 on success, or -1 if the rounded tap count would not
	 *         leave room in the existing buffer -- in which case nothing
	 *         is changed, not even the pointer.
	 */
	int setCoefficients(float *coeff, unsigned ncoeff);

	/**
	 * @brief Filter a block of samples: y[n] = x[n] + sum(coeff[k] * y[n-1-k]).
	 * @param in     Input samples, @p count entries.
	 * @param out    Output samples, @p count entries. In-place safe only
	 *               if @p in and @p out do not alias.
	 * @param count  Number of samples to process.
	 */
	void process(const float *in, float *out, unsigned count);

private:
	float *m_coeff;		/* +0x00 not owned                          */
	float *m_hist;		/* +0x04 owned, m_len entries               */
	unsigned m_ncoeff;	/* +0x08 taps, always a multiple of four    */
	unsigned m_len;		/* +0x0c m_ncoeff + blockSize               */
	int m_pos;		/* +0x10 write index, counts DOWN, signed   */
};				/* 20 bytes                                 */

#endif /* DSPLIB_FLOATIIR_H */
