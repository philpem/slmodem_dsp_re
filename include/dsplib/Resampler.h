/*
 * Resampler.h -- the polyphase resampler, the base of a four-deep chain.
 *
 * Reconstructed from dsplibs.o.  This is the root of
 *
 *     Resampler  <-  ResamplerTimingOffset  <-  ResamplerTiming
 *                                            <-  V90Resampler
 *
 * (each arrow a DIRECT, single, non-virtual base), and it is the file that
 * defines the one thing every class in the chain needs: the virtual set.
 *
 * ---------------------------------------------------------------------------
 * THE CHAIN IS READ OFF THE DESTRUCTORS AND THE FOUR VTABLES, NOT GUESSED
 *
 *   _ZN21ResamplerTimingOffsetD1Ev  calls  _ZN9ResamplerD2Ev
 *   _ZN15ResamplerTimingD1Ev        calls  _ZN21ResamplerTimingOffsetD2Ev
 *   _ZN12V90ResamplerD1Ev           calls  _ZN15ResamplerTimingD2Ev
 *
 * A D1 that calls exactly one D2 is a single non-virtual direct base.  The
 * constructors agree: `ResamplerTimingOffsetC2` opens with a call to
 * `_ZN9ResamplerC2Ejfjfj`, `ResamplerTimingC2` with one to
 * `_ZN21ResamplerTimingOffsetC2Ejfjffj`, `V90ResamplerC1` with one to
 * `_ZN15ResamplerTimingC2Ejfjffj`.  And the C1/C2 emission pattern matches:
 * a class used only as a base gets C2, a most-derived one gets C1.
 *
 * THE VIRTUAL SET, from the four vtables' relocations (`objdump -r
 * --section=.gnu.linkonce.r._ZTV<name>`).  Slots 0 and 1 are the two Itanium
 * ABI words, so the first function is at byte 8:
 *
 *     slot   _ZTV9Resampler   _ZTV21Resampler-   _ZTV15Resampler-  _ZTV12V90-
 *                             TimingOffset       Timing            Resampler
 *     +0x08  ResamplerD1      RTOD1              RTD1              V90RD1
 *     +0x0c  ResamplerD0      RTOD0              RTD0              V90RD0
 *     +0x10  Resampler::      RTO::reset()       RTO::reset()      V90R::reset()
 *              reset()
 *     +0x14  Resampler::      RTO::timing-       RT::timing-       RT::timing-
 *              timingCorrection  Correction        Correction        Correction
 *     +0x18  --               --                 RT::reset(unsigned)
 *                                                                  RT::reset(unsigned)
 *
 * Three things fall straight out of that table and each is easy to get wrong:
 *
 *   - `ResamplerTiming` does NOT override `reset()`.  Its slot +0x10 is
 *     `_ZN21ResamplerTimingOffset5resetEv`, the base's.  A naive
 *     reconstruction gives `ResamplerTiming` a `reset()` override and still
 *     passes every byte comparison of a constructed object;
 *     test/unit/t_resampler.cpp dispatches through a `Resampler *` to
 *     separate the two.
 *   - `ResamplerTiming` introduces a NEW virtual, `reset(unsigned)`, at slot
 *     +0x18.  It is an overload of `reset()`, so inside `ResamplerTiming` and
 *     below, the name `reset` hides the inherited nullary one -- which is
 *     exactly why `V90ResamplerC1` reaches it as the qualified, non-virtual
 *     `ResamplerTiming::reset(1)`.
 *   - `V90Resampler` overrides `reset()` and nothing else.  `resample` is NOT
 *     virtual anywhere: it appears in no vtable, and `V90Resampler::resample`
 *     hides rather than overrides `Resampler::resample`.
 *
 * The vtable is reached for real, so none of this is vestigial:
 * `Resampler::resample` ends its per-output-sample block with
 *
 *     mov  (%ecx),%ebx        ; ebx = *(void **)this, the vptr
 *     ...
 *     call *0xc(%ebx)         ; slot +0x14 == timingCorrection(float)
 *
 * -- a virtual call to `timingCorrection`, whose base implementation is the
 * one-byte `ret` at `_ZN9Resampler16timingCorrectionEf` (a WEAK symbol in
 * `.gnu.linkonce.t.*`, which is what an in-class empty body compiles to).
 *
 * ---------------------------------------------------------------------------
 * `operator delete` IS A MEMBER AND IT CALLS `sysdep_free`
 *
 * The three deleting destructors end `jmp sysdep_free`, and there is not one
 * `_Zdl*` or `_Znw*` symbol in the whole 1.2 MB object.  The tree links test
 * binaries with $(CC) and no libstdc++, so a `D0` that referenced the default
 * `::operator delete` would leave an undefined `_ZdlPvj` and break EVERY test
 * binary, not just this class's.  Measured, in both directions, on the host
 * compiler:
 *
 *     with the member below      nm -C: sysdep_free undefined, no operator delete
 *     with the member removed    nm -C: `U operator delete(void*, unsigned int)`
 *
 * The member is declared here on the base so all four classes inherit it.
 *
 * ---------------------------------------------------------------------------
 * THE OBJECT IS 0x48 BYTES and every offset below was read from a store or a
 * load, not inferred.  The vptr is at +0x00: every constructor and destructor
 * in the chain does `movl $0x8,(%this)` with an `R_386_32` against its own
 * `_ZTV`, and eight is the two leading ABI words.
 *
 * The data member NAMES are invented and descriptive -- the mangling
 * preserves method and type names but never a data member's (finding F226).
 */

#ifndef DSPLIB_RESAMPLER_H
#define DSPLIB_RESAMPLER_H

#include "dsplib/sysdep.h"

class Resampler {
public:
	/*
	 * TWO CONSTRUCTORS, and the difference between them is who owns the
	 * coefficients.  The `float` overload designs a `LowPassFIR<float>`
	 * and transposes it into a freshly allocated polyphase bank, storing
	 * 0 at `coeffsBorrowed`; the `float *` overload adopts the caller's
	 * array and stores 1.  `~Resampler` frees `coeffs` only when the flag
	 * is 0, which is the only reason the second one exists here: the
	 * closure of `dp_vpcm_init` needs only the first, and without the
	 * second nothing would ever set the flag and the destructor's
	 * skip-the-free arm would be unreachable.
	 */
	Resampler(unsigned int phases, float ppmScale, unsigned int taps,
		  float cutoff, unsigned int minHistory);
	Resampler(unsigned int phases, float ppmScale, unsigned int taps,
		  float *coeffs, unsigned int minHistory);

	virtual ~Resampler();

	virtual void reset();

	/*
	 * The hook `resample` dispatches through once per output sample.  The
	 * base body is EMPTY -- `_ZN9Resampler16timingCorrectionEf` is one
	 * byte, a bare `ret` -- and it is defined in-class here because that
	 * is what puts it in a `.gnu.linkonce.t.` section, which is where the
	 * object has it.
	 */
	virtual void timingCorrection(float) { }

	void resample(const float *in, unsigned int n, float *out,
		      unsigned int &nOut);

	void setNormalizedPhase(float p);
	float getNormalizedPhase() const;

	void copyHistoryTail();
	void resetHistoryIndex();

	/*
	 * See the file comment.  Inline, so it is folded into every deleting
	 * destructor and nothing references `::operator delete`.
	 */
	static void operator delete(void *p) { sysdep_free(p); }

	/*
	 * Public for `offsetof`; the original's access specifiers are not
	 * recoverable, and one access section is what keeps `offsetof`
	 * meaningful.  See V90ConstellationDesigner.h.
	 */

	/*
	 * +0x00 is the vptr.  It is NOT declared here -- `virtual ~Resampler`
	 * above is what puts it there, and declaring both would move
	 * everything by four.
	 */

	float		*coeffs;	/* +0x04 phases*taps, polyphase       */
	float		*history;	/* +0x08 historyLen floats            */
	double		 phase;		/* +0x0c fractional phase, in samples
					 *       scaled by `phases`           */
	float		 pending[5];	/* +0x14 input not yet consumed       */
	unsigned int	 pendingCount;	/* +0x28 how many of `pending` valid  */
	float		 ppmScale;	/* +0x2c PPM <-> internal units       */
	unsigned int	 phases;	/* +0x30 polyphase branches           */
	unsigned int	 taps;		/* +0x34 taps per branch              */
	unsigned int	 historyLen;	/* +0x38 floats in `history`          */
	unsigned int	 historyIndex;	/* +0x3c write cursor, starts at taps */
	unsigned int	 inputCredit;	/* +0x40 INPUT samples still to be
					 *       shifted in before the next
					 *       output; see resample()       */
	int		 coeffsBorrowed;/* +0x44 1 == do not free `coeffs`    */
};

#endif /* DSPLIB_RESAMPLER_H */
