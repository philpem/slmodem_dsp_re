/*
 * V90SpectralVerifier.h -- the V.90 received-spectrum accumulator's state.
 *
 * Reconstructed from dsplibs.o.  Twelve members, 2,764 bytes; `reset()` and
 * `checkSpecialSpectralConditions()` are the ones written here, and `reset()`
 * is the one `v34handshak` reaches.
 *
 * NOT POLYMORPHIC: `~V90SpectralVerifier` is listed with `D1` and `D2` and no
 * `D0`, so there is no vptr.
 *
 * THE OBJECT IS 44 BYTES.  The largest `this`-relative displacement across
 * all fourteen defined members is +0x28, four bytes wide, so the object ends
 * at 0x2c.  `process(float *, unsigned int)` reaches +0x2c0 and +0x2a4, but
 * not through `this`: both are inside the block whose address `this + 0x00`
 * holds, which is the trap finding 234 recorded for `V90PreFilter`.
 *
 * WHAT `reset()` WRITES.  Three consecutive words, all set to zero, and
 * nothing else -- the whole body is one `edprintf` and three stores.  Two of
 * the three are named from what other members do with them, which was
 * measured while bounding the object:
 *
 *     +0x20  `process` loads it and compares it against +0x10 before doing
 *            any work; `startAccumulation` also sets it to 0.  A count of
 *            what has been accumulated so far, against a target at +0x10.
 *     +0x24  `startAccumulation` returns immediately unless it is 1, then
 *            sets it to 1; `process` returns immediately unless it is 1.
 *            The gate that says an accumulation is running.
 *     +0x28  stored zero HERE AND NOWHERE ELSE.  No member of the class
 *            reads it and no other writes it, so it gets an offset-derived
 *            name: naming it for a purpose would be inventing one.
 *
 * BOTH OF THOSE LAST TWO PARAGRAPHS ARE NOW RETRACTED, and `checkSpecial-
 * SpectralConditions` is what retracts them.  Neither NAME below moves --
 * see the warning at the end of this comment -- but what the two slots hold
 * is measured rather than bounded now:
 *
 *     +0x24 IS A THREE-STATE, NOT A FLAG.  `reset` stores 0 (0x45ca4),
 *           `startAccumulation` stores 1 (0x45cec) and `process` stores 2
 *           (0x46659) on the sample that completes the accumulation, and
 *           `checkSpecialSpectralConditions` opens `cmpl $0x2,0x24(%edi)`
 *           and returns at once unless it holds 2.  So 0 is idle, 1 is
 *           running, and 2 is complete-and-not-yet-restarted.  The old
 *           sentence bounded it at "the gate that says an accumulation is
 *           running", which is true of 1 and says nothing about 2.
 *
 *     +0x28 IS THE DETECTED `V90SpecialSpectralConditions`, and the
 *           original's own diagnostics name all four values.
 *           `checkSpecialSpectralConditions` clears it on entry and then
 *           stores 1, 2 or 3 beside an `edprintf` that says what each is:
 *
 *               0  "V90SpectralVerifier: No special conditions"
 *               1  "... German ISDN NT1 box conditions detected!"
 *               2  "... German PBX conditions detected!"
 *               3  "... Severe Codec conditions detected!"
 *
 *           TWO INDEPENDENT DERIVATIONS AGREE ON THE 2.  `include/dsplib/
 *           V90SpectralConditions.h` reached `V90_SPECTRAL_GERMAN_PBX = 2`
 *           from a different function entirely -- `V90ConstellationDesigner
 *           ::spectralDesign` compares its argument against the literal 2
 *           and copies the `GERMAN_PBX_SPECTRAL_SHAPER_*` run -- and this
 *           function's `movl $0x2,0x28(%edi)` at 0x46588 sits beside the
 *           string that says German PBX.  Neither reading knew about the
 *           other.
 *
 *     THE NAMES BELOW ARE DELIBERATELY UNCHANGED, and this is not
 *     timidity.  `word_28` would become `specialConditions` and
 *     `accumulating` would become a state, but `src/pump/v90/V90Equalizer
 *     .cpp` reads `spectralVerifier->word_28 == 2` at three sites,
 *     `test/unit/t_v90equ.cpp` and `t_v90leaves.cpp` name both fields, and
 *     `test/mutations/v90specver.json` carries `word_28 = 0;` and
 *     `accumCount = 0;\n\taccumulating = 0;\n\tword_28 = 0;` inside `find`
 *     strings that `make phase` does not execute -- so a rename would rot
 *     the mutation register silently, which is finding 3511's failure mode
 *     with the register in place of a header.  The rename belongs in one
 *     commit of its own that moves all five files together.
 *
 * EVERYTHING BELOW +0x20 IS NAMED BY THE CONSTRUCTOR, which writes all eight
 * words and takes six of them from the parameter block:
 *
 *     +0x00  the `V90Parameters *` argument, stored first and re-read out of
 *            the object twice afterwards.
 *     +0x04  a `Psd`, `sysdep_malloc(0x10)` and then `Psd::Psd` on the
 *            result: length +0x0c, window PARAMS+0x2b4, overlap PARAMS+0x2bc.
 *     +0x08  `SPECTRAL_VERIFIER_SAMPLE_FREQ` (PARAMS+0x2ac), a float.
 *     +0x0c  `SPECTRAL_VERIFIER_FFT_LEN` (PARAMS+0x2b0).  UNSIGNED, twice
 *            over: it is converted with `push 0` / `push` / `fildll`, the
 *            zero-extending idiom, and halved with `shr $1`.
 *     +0x10  `SPECTRAL_VERIFIER_PSD_LEN` (PARAMS+0x2b8).
 *     +0x14  +0x08 divided by +0x0c.  The bytes are `de f9`, which objdump
 *            prints as `fdivrp` and which IS `FDIVP` -- finding 245 -- so the
 *            quotient is sampleFreq/fftLength and not its reciprocal.  A
 *            sample rate over a transform length is a bin width, which is
 *            what `freqToNearestBin` divides by.
 *     +0x18  `sysdep_malloc(4 * psdLength)`, left as the allocator returned
 *            it.  Nothing here says what goes in it, so it is named for its
 *            offset.
 *     +0x1c  `sysdep_malloc(4 * (fftLength / 2))`, the spectrum
 *            `getSpectrumOfBin` indexes with `flds (%edx,%eax,4)`.
 *
 * THE CONSTRUCTOR DOES NOT WRITE +0x20 OR +0x24.  Only +0x28 of the three
 * words `reset()` clears is initialised, so a verifier that is constructed
 * and never reset carries whatever the allocator left in its accumulation
 * counter and its running flag.  Recorded in docs/deviations.md, unmeasured.
 */

#ifndef DSPLIB_V90SPECTRALVERIFIER_H
#define DSPLIB_V90SPECTRALVERIFIER_H

class Psd;
class V90Parameters;

class V90SpectralVerifier {
public:
	/* Defined in src/pump/v90/V90SpectralVerifier.cpp. */
	V90SpectralVerifier(V90Parameters *params);
	~V90SpectralVerifier();

	void reset();

	/*
	 * Look for the three special line conditions in the accumulated
	 * spectrum and leave the answer in `word_28`.  Does nothing but
	 * clear `word_28` unless `accumulating` holds 2; see the file
	 * comment for the four values and where their names come from.
	 */
	void checkSpecialSpectralConditions();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V90Parameters *params;		/* +0x00 the constructor's argument */
	Psd *psd;			/* +0x04 owned, 16 bytes            */
	float sampleFreq;		/* +0x08 PARAMS+0x2ac               */
	unsigned int fftLength;		/* +0x0c PARAMS+0x2b0               */
	unsigned int psdLength;		/* +0x10 PARAMS+0x2b8               */
	float binWidth;			/* +0x14 sampleFreq / fftLength     */
	float *buf_18;			/* +0x18 owned, psdLength floats    */
	float *spectrum;		/* +0x1c owned, fftLength/2 floats  */
	unsigned int accumCount;	/* +0x20 progress toward +0x10      */
	unsigned int accumulating;	/* +0x24 1 while an accumulation runs */
	unsigned int word_28;		/* +0x28 written only by reset()    */
};

#endif /* DSPLIB_V90SPECTRALVERIFIER_H */
