/**
 * @file V90SdDetector.h
 * @brief `V90SdDetector` -- a signal-drop-style detector that scores a
 *        rolling autocorrelation of the recent signal against two thresholds
 *        and reports one of three verdicts per sample.
 *
 * Not polymorphic (`tools/cppstruct.py` lists the destructor with `D1`/`D2`
 * and no `D0`, so there is no vptr and offset 0 is a real member); the
 * object is 28 bytes (0x1c), and every field is named.
 *
 * The constructor's four arguments are stored to four words with no
 * conversion (see V90SdDetector.cpp) and, unusually, out of declaration
 * order: argument 4 (the limit) lands at +0x04 and argument 3 at +0x10.
 * `process()` reads a run-length counter (+0x00) against a limit (+0x04),
 * two float thresholds (+0x08, +0x0c) against an energy accumulator and a
 * correlation/energy ratio, and a third threshold (+0x10) against that same
 * ratio on the branch where +0x0c did not clear it -- so +0x10 is the lower
 * of two thresholds on one quotient, and the band between them is the only
 * path that leaves the counter untouched.
 *
 * The history buffer is always twelve floats: the constructor stores the
 * constant 12 into `historyLength` and allocates `0x30` bytes regardless of
 * what the caller passes, because `process()`'s correlation loop always
 * looks six lags ahead (`history[i]` against `history[i + 6]` for
 * i = 0..5) and twelve is exactly enough room for that. `historyLength`
 * is still a real field, read back by both the constructor's and
 * `reset()`'s clearing loops, not inlined as a constant.
 */

#ifndef DSPLIB_V90SDDETECTOR_H
#define DSPLIB_V90SDDETECTOR_H

class V90SdDetector {
public:
	/**
	 * @brief Construct the detector: stores the three thresholds and the
	 *        limit verbatim, and allocates a fixed twelve-element float
	 *        history buffer (independent of any argument), zeroed.
	 * @param thresh08  Energy floor below which `process()` resets the
	 *                  run-length counter (stored at +0x08).
	 * @param thresh0c  Correlation/energy ratio above which `process()`
	 *                  advances the run-length counter (stored at +0x0c).
	 * @param value10   The lower of the two ratio thresholds; the band
	 *                  between it and `thresh0c` leaves the counter
	 *                  untouched (stored at +0x10).
	 * @param limit     Run length at which `process()` starts reporting a
	 *                  positive result (stored at +0x04).
	 */
	V90SdDetector(float thresh08, float thresh0c, float value10,
		      unsigned int limit);

	/** @brief Frees the history buffer. */
	~V90SdDetector();

	/** @brief Zero the run-length counter and the history buffer. */
	void reset();

	/**
	 * @brief Shift one new sample into the history, recompute the
	 *        rolling energy and lag-6 autocorrelation over the twelve-
	 *        sample window, and score the result against the three
	 *        thresholds.
	 * @param sample  The newest signal sample.
	 * @return 1 once the run-length counter reaches `limit` (energy above
	 *         `thresh08` and ratio above `thresh0c`); -1 while the ratio
	 *         falls in the band between `value10` and `thresh0c`
	 *         (counter left unchanged); 0 otherwise (counter reset to 0,
	 *         or still counting up towards `limit`).
	 */
	int process(float sample);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int count;		/* +0x00 the run-length counter     */
	unsigned int limit;		/* +0x04 what +0x00 is compared to  */
	float thresh_08;		/* +0x08 argument 1                 */
	float thresh_0c;		/* +0x0c argument 2                 */
	float value_10;			/* +0x10 argument 3; the lower of two
					   ratio thresholds `process()` reads */
	float *history;			/* +0x14 the correlator's history   */
	unsigned int historyLength;	/* +0x18 elements in it; ctor sets 12 */
};

#endif /* DSPLIB_V90SDDETECTOR_H */
