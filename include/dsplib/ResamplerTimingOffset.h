/*
 * ResamplerTimingOffset.h -- the resampler's timing-offset control.
 *
 * Reconstructed from dsplibs.o.  `Resampler` is a DIRECT base
 * (`_ZN21ResamplerTimingOffsetD1Ev` calls `_ZN9ResamplerD2Ev`, and the
 * constructor opens with `_ZN9ResamplerC2Ejfjfj`); dsplib/Resampler.h carries
 * the whole chain, the virtual set and the four vtables' contents.
 *
 * THE OBJECT IS 0x4c BYTES AND THIS CLASS CONTRIBUTES EXACTLY FOUR OF THEM.
 * That is a correction to what this file used to say, and the correction is
 * measured rather than argued.  The old text put `ppmScale` at +0x2c and the
 * `double` at +0x0c here, on the evidence of `setTimingOffset` and
 * `getTimingOffsetPPM` touching them; both are real, and both are INHERITED.
 * `_ZN9ResamplerC2Ejfjfj` -- which runs before this class's constructor
 * stores anything -- writes +0x04, +0x08, +0x0c, +0x14..+0x24, +0x28, +0x2c,
 * +0x30, +0x34, +0x38, +0x3c, +0x40 and +0x44, so all of those belong to
 * `Resampler` and `Resampler` is 0x48 bytes.  The only store this class's
 * constructor makes after the base call is
 *
 *     flds  0x34(%esp)   ; the argument the base ctor did NOT forward
 *     fmuls 0x2c(%ebx)   ; ppmScale, inherited
 *     fmuls .rodata.cst4 ; 1e-6f
 *     fstps 0x48(%ebx)   ; <-- the one new field
 *
 * so `timingOffset` at +0x48 is all there is, and 0x4c is the size.  The
 * "76 bytes" in the old text was right; the reason it gave was wrong.
 *
 * The class had also been modelled with a plain `void *vptr` field instead of
 * a real `virtual`, deliberately, to avoid emitting a vtable for a 21-byte
 * setter.  That is no longer the right trade: the four vtables are now
 * written, `Resampler::resample` really does dispatch through slot +0x14, and
 * `dsplib/Resampler.h` explains how the member `operator delete` keeps the
 * link free of libstdc++.  The layout is unchanged either way -- a vptr and a
 * leading pointer-sized member occupy the same four bytes.
 *
 * ---------------------------------------------------------------------------
 * THE ARGUMENT IS IN PARTS PER MILLION, and both directions are in the object:
 *
 *     setTimingOffset(f)      timingOffset = ppmScale * f * 1e-6f
 *     getTimingOffsetPPM()    1e6f * timingOffset / ppmScale
 *
 * 1e-6f is `.rodata.cst4+0x1fc` (0x358637bd) and 1e6f is `+0x208`; both are
 * loaded with `fmuls`/`flds`, so both are SINGLE precision and the `f`
 * suffixes below are load-bearing.  `+0x2c` is whatever factor converts one
 * to the other, which is what `ppmScale` says and all it says.
 *
 * Data member names are invented and descriptive (finding 226).
 */

#ifndef DSPLIB_RESAMPLERTIMINGOFFSET_H
#define DSPLIB_RESAMPLERTIMINGOFFSET_H

#include "dsplib/Resampler.h"

class ResamplerTimingOffset : public Resampler {
public:
	ResamplerTimingOffset(unsigned int phases, float ppmScale,
			      unsigned int taps, float cutoff, float ppm,
			      unsigned int minHistory);
	ResamplerTimingOffset(unsigned int phases, float ppmScale,
			      unsigned int taps, float *coeffs, float ppm,
			      unsigned int minHistory);

	virtual ~ResamplerTimingOffset();

	virtual void reset();

	/*
	 * The float argument is IGNORED -- the whole body is
	 * `fldl 0xc; fadds 0x48; fstpl 0xc`.  This is the override that makes
	 * the base's per-sample hook do something: it advances the phase
	 * accumulator by a fixed offset instead of by a measurement.
	 */
	virtual void timingCorrection(float);

	void setTimingOffset(float ppm);
	float getTimingOffsetPPM() const;

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	float	timingOffset;		/* +0x48 phase units per sample */
};

#endif /* DSPLIB_RESAMPLERTIMINGOFFSET_H */
