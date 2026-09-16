/*
 * ResamplerTiming.h -- the timing-recovery layer of the resampler chain.
 *
 * Reconstructed from dsplibs.o.  `ResamplerTimingOffset` is a DIRECT base
 * (`_ZN15ResamplerTimingD1Ev` calls `_ZN21ResamplerTimingOffsetD2Ev`, and the
 * constructor opens with `_ZN21ResamplerTimingOffsetC2Ejfjffj`).
 * dsplib/Resampler.h carries the chain and the vtables.
 *
 * TWO THINGS ABOUT THE VIRTUALS, BOTH READ OFF `_ZTV15ResamplerTiming`:
 *
 *   - slot +0x10 is `_ZN21ResamplerTimingOffset5resetEv`.  This class does
 *     NOT override `reset()`.
 *   - slot +0x18 is `_ZN15ResamplerTiming5resetEj`, a slot the base tables do
 *     not have.  `reset(unsigned)` is a NEW virtual, not an override, and
 *     being an overload of `reset` it hides the inherited nullary form for
 *     this class and everything below it.
 *
 * `reset(unsigned)` IGNORES ITS ARGUMENT.  106 bytes and not one reference to
 * `0x14(%esp)`.  Both call sites in the closure pass 1
 * (`V90ResamplerC1` and `V90Resampler::reset`).  It is kept in the signature
 * because the mangling has it: `_ZN15ResamplerTiming5resetEj`.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE CLASS DOES
 *
 * `timingCorrection` -- the base's per-output-sample hook -- runs a two-pole
 * resonator over the sample it is handed:
 *
 *     y[n] = normBPFhBaudB0coef * x[n] - 0.9604 * y[n-2]
 *
 * 0.9604 is 0.98 squared, so the poles sit at z = +/-0.98j, which is Fs/4.
 * `V90Equalizer::process` feeds this chain two samples per symbol, so Fs/4 is
 * BAUD/2 -- which is what every name in the group says.  The squared output
 * is second-differenced into a timing error, and every other call closes a
 * PI loop over it with the `bllK1`/`bllK2` gains `V90Resampler::setBllState`
 * chooses.  `SdHalfBaudDft` accumulates 256 samples into the same Fs/4 bin,
 * and `adjustHalfBaudBpfGain` renormalises `normBPFhBaudB0coef` from it.
 *
 * ---------------------------------------------------------------------------
 * THE OBJECT IS 0x94 BYTES.  This class's own fields are +0x4c..+0x93 and
 * they are exactly the stores `_ZN15ResamplerTimingC2Ejfjffj` makes after the
 * base constructor returns -- a list identical, store for store, to the body
 * of `reset(unsigned)`.
 *
 * THE FIELD GROUPINGS BELOW COME FROM CODE THAT *USES* THEM, not from the way
 * `reset` zeroes them.  An earlier version of this file read +0x54..+0x5c as
 * `bll[3]`, +0x68..+0x78 as `half[5]` and +0x80..+0x88 as `dftAcc[3]`, purely
 * from the constructor storing a zeroed register to each; every one of those
 * three groupings was wrong.  +0x68/+0x6c are a delay line of y-SQUARED and
 * +0x70/+0x74 one of y; +0x80 is a RESULT and +0x84/+0x88 the accumulators
 * that produce it.  A constant-index array store and a scalar store are
 * indistinguishable, so an array can never be disproved -- but there is no
 * positive evidence for one here and there is positive evidence against a
 * common meaning.
 *
 * +0x58 and +0x64 are written by NOTHING but `reset` (and +0x64 not even by
 * that).  They are recorded as unnamed; inventing a name for a field no code
 * reads would put a guess where every other line here is a measurement.
 *
 * `normBPFhBaudB0coef` at +0x90 is THE AUTHOR'S OWN NAME, out of the third of
 * `adjustHalfBaudBpfGain`'s three diagnostic strings.  So is "normFactor",
 * and so is the "baud/2 dft bin" that named `dftMag`.  Everything else here
 * is invented and descriptive (finding F226).
 */

#ifndef DSPLIB_RESAMPLERTIMING_H
#define DSPLIB_RESAMPLERTIMING_H

#include "dsplib/ResamplerTimingOffset.h"

class ResamplerTiming : public ResamplerTimingOffset {
public:
	/**
	 * @brief Construct with a self-designed filter and timing-recovery
	 *        state reset.
	 *
	 * Forwards the first five arguments to ResamplerTimingOffset's
	 * designing constructor, then calls `reset(0)` (the argument is
	 * unobservable -- reset(unsigned int) never reads it) and
	 * setTimingOffset(@p ppm) again, mirroring the base's own
	 * construction sequence one level down.
	 *
	 * @param phases      See Resampler's constructor.
	 * @param ppmScale    See Resampler's constructor.
	 * @param taps        See Resampler's constructor.
	 * @param cutoff      See Resampler's constructor.
	 * @param ppm         Fixed timing offset, in parts per million.
	 * @param minHistory  See Resampler's constructor.
	 */
	ResamplerTiming(unsigned int phases, float ppmScale, unsigned int taps,
			float cutoff, float ppm, unsigned int minHistory);

	/**
	 * @brief Construct over caller-supplied coefficients with
	 *        timing-recovery state reset.
	 *
	 * See the other overload; forwards to ResamplerTimingOffset's
	 * adopting constructor instead.
	 *
	 * @param phases      See Resampler's constructor.
	 * @param ppmScale    See Resampler's constructor.
	 * @param taps        See Resampler's constructor.
	 * @param coeffs      See Resampler's constructor.
	 * @param ppm         Fixed timing offset, in parts per million.
	 * @param minHistory  See Resampler's constructor.
	 */
	ResamplerTiming(unsigned int phases, float ppmScale, unsigned int taps,
			float *coeffs, float ppm, unsigned int minHistory);

	/** @brief Destroy the instance. Nothing of this class's own to release. */
	virtual ~ResamplerTiming();

	/**
	 * @brief Run the half-baud band-pass resonator and close the timing
	 *        loop.
	 *
	 * Filters @p v through the two-pole resonator at Fs/4 (baud/2),
	 * second-differences its squared output into a phase error, and --
	 * every other call, per `halfBaudStep` -- closes a PI loop over that
	 * error onto `phase` using `bllK1`/`bllK2`.
	 *
	 * @param v  The output sample to filter (the resampler's per-output
	 *           value).
	 */
	virtual void timingCorrection(float v);

	/*
	 * The NEW virtual at vtable slot +0x18.  The argument is unused.
	 *
	 * -Woverloaded-virtual fires on this declaration, from
	 * V90Resampler.h, and it is RIGHT: `V90Resampler::reset()` hides it.
	 * That is the object's own shape -- `_ZTV12V90Resampler` overrides
	 * slot +0x10 and leaves slot +0x18 as `_ZN15ResamplerTiming5resetEj`
	 * -- and it is exactly why `V90ResamplerC1` reaches this one as the
	 * qualified, non-virtual `ResamplerTiming::reset(1)`.  A `using`
	 * declaration would silence the warning and would be an invention:
	 * nothing in the object says the name was re-exported.  The pragma is
	 * around the declaration because that is where GCC reports it.
	 *
	 * `tools/toolchain`'s gcc-3.4.3 does not know `#pragma GCC
	 * diagnostic` and says `warning: ignoring #pragma GCC diagnostic`
	 * three times per translation unit that includes this header.  It
	 * still compiles and exits 0, so `make similarity` is unaffected
	 * beyond the noise; measured rather than assumed.
	 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
	/**
	 * @brief Reset the base and this class's own timing-recovery state.
	 *
	 * Calls `ResamplerTimingOffset::reset()`, then zeroes
	 * `halfBaudStep`, `bllK1`/`bllK2`, `lastHalfBaudErr`, `unnamed_58`,
	 * `lastPhaseAdj`, `bpfSq1`/`bpfSq2`, `bpfZ1`/`bpfZ2` and `errZ1`,
	 * calls resetSdHalfBaudDft(), clears `dftDone`, and sets
	 * `normBPFhBaudB0coef` to its nominal 0.03981f. The argument is
	 * unobservable -- nothing in the object reads it.
	 * @param unused Ignored; retained to preserve the original signature.
	 */
	virtual void reset(unsigned int unused);
#pragma GCC diagnostic pop

	/**
	 * @brief Restart the half-baud DFT accumulation.
	 *
	 * Clears `dftCount`, `dftMag`, `dftRe` and `dftIm`. Does NOT clear
	 * `dftDone` -- restarting the accumulation does not un-complete an
	 * already-completed DFT.
	 */
	void resetSdHalfBaudDft();

	/**
	 * @brief Accumulate one sample into the 256-point half-baud DFT bin.
	 *
	 * No-op once `dftDone` is set. Otherwise adds @p v into `dftRe` or
	 * `dftIm` per a four-phase quadrature pattern (cos = +1,0,-1,0; sin =
	 * 0,-1,0,+1) selected by `dftCount % 4`, and on the 256th sample sets
	 * `dftDone` and computes `dftMag = sqrt(dftRe^2 + dftIm^2)`.
	 *
	 * @param v  The next sample to accumulate.
	 */
	void SdHalfBaudDft(float);

	/**
	 * @brief Renormalise the resonator's gain from the measured baud/2
	 *        energy.
	 *
	 * Does nothing unless `dftDone` is set (256 prior SdHalfBaudDft()
	 * calls). Otherwise scales `dftMag` by @p v, derives a normalising
	 * factor clamped to [1, 2] (a factor above 4 is treated as an
	 * implausibly small bin and reset to nominal), and sets
	 * `normBPFhBaudB0coef` accordingly. Prints three debug lines when
	 * debug output is enabled.
	 *
	 * @param v  Scale applied to `dftMag` before deriving the
	 *           normalising factor.
	 */
	void adjustHalfBaudBpfGain(float);

	/**
	 * @brief Nudge `phase` by a fraction of one nominal step.
	 *
	 * No-op for @p v <= 0, NaN, or an unordered comparison. Otherwise
	 * advances `phase` by `v * ppmScale * (1/180)` and banks whole
	 * `phases`-sized steps into `inputCredit`. Has no caller anywhere in
	 * dsplibs.o.
	 *
	 * @param v  Nudge amount; plausibly degrees against a half cycle
	 *           (the constant matches `1.0f / 180.0f`), though only the
	 *           constant itself is established.
	 */
	void addPhase(float);

	/**
	 * @brief Advance `phase` by one nominal step and bank whole outputs.
	 *
	 * Adds `ppmScale` to `phase` and increments `inputCredit` once per
	 * whole `phases` the result wraps past. Has no caller anywhere in
	 * dsplibs.o; the name is unexplained -- it advances the phase and
	 * inverts nothing.
	 */
	void invertPhase();

	/* Public for offsetof; see V90ConstellationDesigner.h. */

	/*
	 * +0x4c and +0x50 are the band-limited-loop gains.  `setBllState`
	 * picks a consecutive (K1, K2) pair out of V90Parameters' `BLL_*`
	 * block at +0x088..+0x0f4 and stores it here; `timingCorrection` uses
	 * K2 on the integrator and K1 on the proportional term.
	 */
	float		bllK1;			/* +0x4c */
	float		bllK2;			/* +0x50 */

	float		lastHalfBaudErr;	/* +0x54 written every call,
						 *       read by nothing    */
	float		unnamed_58;		/* +0x58 written only by
						 *       reset             */
	float		lastPhaseAdj;		/* +0x5c what was added to
						 *       `phase`; read by
						 *       nothing           */
	unsigned int	halfBaudStep;		/* +0x60 only bit 0 is read;
						 *       0,1,2,1,2,...     */
	unsigned char	gap_64[4];		/* +0x64 written by nothing */

	float		bpfSq1;			/* +0x68 y*y, one back      */
	float		bpfSq2;			/* +0x6c y*y, two back      */
	float		bpfZ1;			/* +0x70 y,   one back      */
	float		bpfZ2;			/* +0x74 y,   two back      */
	float		errZ1;			/* +0x78 the error, one back */

	/*
	 * +0x7c..+0x88 are the half-baud DFT, and they are one group because
	 * they are exactly what `resetSdHalfBaudDft` clears.  Note that
	 * `dftDone` at +0x8c is NOT in that group: only `reset(unsigned)`
	 * clears it, so a `resetSdHalfBaudDft` on a completed DFT leaves the
	 * latch set.
	 */
	unsigned int	dftCount;		/* +0x7c 0..256             */
	float		dftMag;			/* +0x80 the "baud/2 dft bin" */
	float		dftRe;			/* +0x84 cos = +1,0,-1,0    */
	float		dftIm;			/* +0x88 sin = 0,-1,0,+1    */

	unsigned char	dftDone;		/* +0x8c latched at 256     */
	unsigned char	gap_8d[3];
	float		normBPFhBaudB0coef;	/* +0x90 starts at 0.03981f */
};

#endif /* DSPLIB_RESAMPLERTIMING_H */
