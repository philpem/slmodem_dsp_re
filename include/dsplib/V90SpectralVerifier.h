/**
 * @file V90SpectralVerifier.h
 * @brief The V.90 received-spectrum accumulator: periodically captures a
 *        power spectral density of the line and checks it for three known
 *        special conditions (German ISDN NT1, German PBX, severe codec).
 *
 * Reconstructed from dsplibs.o. Twelve members, 2,764 bytes of code;
 * `reset()` and `checkSpecialSpectralConditions()` are reconstructed here,
 * and `reset()` is the one `v34handshak` reaches. Not polymorphic (`D1`/`D2`
 * with no `D0`, so no vptr). The object is 44 bytes -- the largest
 * `this`-relative displacement across all fourteen members is +0x28, four
 * bytes wide.
 *
 * `accumCount` (+0x20) and `accumulating` (+0x24) are a progress counter
 * against `psdLength` and the accumulation's own state, and `word_28`
 * (+0x28) is the detected condition. `accumulating` is a three-state, not a
 * flag: `reset()` sets it to 0 (idle), `startAccumulation()` to 1 (running),
 * and `process()` to 2, on the sample that completes an accumulation
 * (complete-and-not-yet-restarted); `checkSpecialSpectralConditions()` acts
 * only when it holds 2. Finding F3528. See `word_28`'s own comment for what
 * it holds and why it keeps its offset-derived name.
 *
 * The constructor writes every word from +0x00 to +0x1c (six of eight taken
 * from the parameter block, one computed, one allocated) but not +0x20 or
 * +0x24 -- of the three words `reset()` clears, only `word_28` (+0x28) is
 * also initialised by the constructor, so a verifier that is constructed and
 * never reset carries the allocator's bytes in its accumulation counter and
 * running state (docs/deviations.md, unmeasured). Findings F1240 and F245
 * (the quotient at +0x14 is sampleFreq/fftLength, not its reciprocal --
 * `de f9` is `FDIVP`, which objdump's `fdivrp` mnemonic reads backwards).
 */

#ifndef DSPLIB_V90SPECTRALVERIFIER_H
#define DSPLIB_V90SPECTRALVERIFIER_H

class Psd;
class V90Parameters;

class V90SpectralVerifier {
public:
	/**
	 * @brief Construct the verifier from a V.90 parameter block.
	 *
	 * Takes `fftLength`, `sampleFreq` and `psdLength` from @p params,
	 * derives `binWidth`, allocates `buf_18` and `spectrum`, and
	 * constructs `psd`. Does not initialise `accumCount` or
	 * `accumulating` -- only reset() clears those (docs/deviations.md,
	 * unmeasured).
	 * @param params  The V.90 parameter block this verifier's thresholds
	 *                and sizes are taken from.
	 */
	V90SpectralVerifier(V90Parameters *params);

	/**
	 * @brief Destructor. Frees `buf_18`, `spectrum` and `psd`, in that
	 * order.
	 */
	~V90SpectralVerifier();

	/**
	 * @brief Idle the accumulator: clear `accumCount`, `accumulating`
	 * and `word_28`.
	 */
	void reset();

	/**
	 * @brief Classify the just-completed accumulated spectrum.
	 *
	 * Does nothing but clear `word_28` unless `accumulating` holds 2
	 * (an accumulation has just completed); otherwise checks the
	 * spectrum for the three known special line conditions and leaves
	 * the result in `word_28`. See `word_28`'s own comment for the four
	 * values and their diagnostics.
	 */
	void checkSpecialSpectralConditions();

	/**
	 * @brief Arm a new spectrum accumulation.
	 *
	 * Clears `accumCount` and moves `accumulating` from idle (0) to
	 * running (1). A no-op if an accumulation is already running (state
	 * 1); a completed one (state 2) re-arms. Also a no-op, including the
	 * state change, unless `SPECTRAL_VERIFIER_ENABLE` is set in the
	 * parameter block -- so with the verifier disabled the state never
	 * leaves 0 and process() never accumulates.
	 */
	void startAccumulation();

	/**
	 * @brief Feed samples into the running accumulation.
	 *
	 * Copies up to @p count samples into the accumulation buffer and,
	 * on the sample that fills it, runs the periodogram and classifies
	 * the line via checkSpecialSpectralConditions(). A no-op while
	 * `accumulating` is not 1 (running).
	 * @param in     Input samples.
	 * @param count  Number of samples available in @p in.
	 * @return 1 exactly on the sample that completes an accumulation,
	 *         0 otherwise.
	 */
	int process(float *in, unsigned int count);

	/**
	 * @brief Print the accumulated spectrum, one `%c%d.%02d` line per
	 * bin between two rules.
	 */
	void printSpectrum() const;

	/**
	 * @brief Round a frequency down to its bin index.
	 * @param freq  Frequency in Hz.
	 * @return `freq / binWidth`, truncated.
	 */
	unsigned int freqToLeftBin(float freq) const;

	/**
	 * @brief Round a frequency up to its bin index.
	 * @param freq  Frequency in Hz.
	 * @return `freq / binWidth`, truncated, plus one.
	 */
	unsigned int freqToRightBin(float freq) const;

	/**
	 * @brief Round a frequency to its nearest bin index.
	 * @param freq  Frequency in Hz.
	 * @return `freq / binWidth`, rounded to nearest.
	 */
	unsigned int freqToNearestBin(float freq) const;

	/**
	 * @brief Look up one bin's accumulated spectrum value.
	 *
	 * Unchecked, in the object and here: `spectrum` is `fftLength / 2`
	 * floats long and this indexes it with whatever it is handed, with
	 * no compare or clamp. Reproduced as-is (docs/deviations.md D780);
	 * callers are the bound (D561).
	 * @param bin  Bin index.
	 * @return The spectrum value at that bin.
	 */
	float getSpectrumOfBin(unsigned long bin) const;

	/**
	 * @brief Look up the spectrum value of the bin nearest a frequency.
	 * @param freq  Frequency in Hz.
	 * @return getSpectrumOfBin(freqToNearestBin(freq)).
	 */
	float getSpectrumOfNearestBin(float freq) const;

	/* Public for offsetof; see V90ConstellationDesigner.h. */

	/** @brief The V.90 parameter block passed to the constructor. */
	V90Parameters *params;		/* +0x00 */

	/** @brief The owned periodogram estimator (finding F1240). */
	Psd *psd;			/* +0x04 owned, 16 bytes            */

	/** @brief `SPECTRAL_VERIFIER_SAMPLE_FREQ` (finding F1240). */
	float sampleFreq;		/* +0x08 PARAMS+0x2ac               */

	/** @brief `SPECTRAL_VERIFIER_FFT_LEN` (finding F1240). */
	unsigned int fftLength;		/* +0x0c PARAMS+0x2b0               */

	/** @brief `SPECTRAL_VERIFIER_PSD_LEN` (finding F1240). */
	unsigned int psdLength;		/* +0x10 PARAMS+0x2b8               */

	/**
	 * @brief The FFT bin width, `sampleFreq / fftLength`. Findings F1240
	 * and F245 (the division's direction is measured, not read off the
	 * mnemonic).
	 */
	float binWidth;			/* +0x14 */

	/*
	 * +0x18  `psdLength` owned floats, allocated by the constructor and
	 * otherwise untouched by any reconstructed member -- nothing here
	 * says what goes in it, so it keeps its offset-derived name.
	 */
	float *buf_18;			/* +0x18 */

	/** @brief The owned accumulated spectrum, `fftLength / 2` floats. */
	float *spectrum;		/* +0x1c */

	/** @brief Samples accumulated so far, checked against `psdLength`. */
	unsigned int accumCount;	/* +0x20 */

	/**
	 * @brief The accumulation's own state, not a flag: 0 idle (reset()),
	 * 1 running (startAccumulation()), 2 just completed
	 * (process(), on the sample that fills the buffer). Finding F3528.
	 */
	unsigned int accumulating;	/* +0x24 */

	/*
	 * +0x28  The detected `V90SpecialSpectralConditions`, kept under its
	 * offset-derived name rather than the header's own enum's spelling.
	 * checkSpecialSpectralConditions() clears it on entry and then
	 * stores 1, 2 or 3 beside an `edprintf` that names each: 1 "German
	 * ISDN NT1 box conditions detected!", 2 "German PBX conditions
	 * detected!", 3 "Severe Codec conditions detected!" (0 stays "No
	 * special conditions"). The 2 is derived twice, independently: this
	 * function's store sits beside the "German PBX" string, and
	 * `include/dsplib/V90SpectralConditions.h` reaches the same value
	 * from `V90ConstellationDesigner::spectralDesign` comparing its
	 * argument against the literal 2 -- two functions, two kinds of
	 * evidence, one answer (finding F3528).
	 *
	 * Not renamed to match the enum: `V90Equalizer.cpp` reads
	 * `spectralVerifier->word_28 == 2` at three sites, two unit test
	 * files name it directly, and `test/mutations/v90specver.json`
	 * matches this exact spelling inside `find` strings that
	 * `make phase` never executes -- so a silent rename would leave
	 * those entries matching nothing (finding F3511's mechanism). The
	 * rename belongs in one commit that moves all of those together.
	 */
	unsigned int word_28;		/* +0x28 */
};

#endif /* DSPLIB_V90SPECTRALVERIFIER_H */
