/*
 * V90Resampler.h -- the V.90 receiver's resampler, bottom of the chain.
 *
 * Reconstructed from dsplibs.o.  `ResamplerTiming` is a DIRECT base
 * (`_ZN12V90ResamplerD1Ev` calls `_ZN15ResamplerTimingD2Ev`, and the
 * constructor opens with `_ZN15ResamplerTimingC2Ejfjffj`).
 * dsplib/Resampler.h carries the chain and the four vtables.
 *
 * `_ZTV12V90Resampler` overrides `reset()` and NOTHING else: slot +0x14 is
 * still `ResamplerTiming::timingCorrection` and slot +0x18 still
 * `ResamplerTiming::reset(unsigned)`.  `resample` is not in any vtable, so
 * `V90Resampler::resample` HIDES `Resampler::resample` rather than overriding
 * it -- and calls it, as its first instruction after the prologue.
 *
 * ---------------------------------------------------------------------------
 * THE OBJECT IS 0xb4 BYTES, and that number has a second, independent
 * witness: `include/dsplib/V90Demodulator.h` embeds one at +0x094 and records
 * from that class's own constructor that it "ends at +0x148".  0x148 - 0x094
 * == 0xb4.  Neither measurement was made from the other.
 *
 * This class's own fields are +0x94..+0xb3.  `V90ResamplerC1` writes +0xa0,
 * +0xa4, +0xa8, +0x94, +0x98, +0xac and +0xb0; `setBllState` writes +0x94,
 * +0x98 and +0x9c and reads +0xa0.  Nothing writes anything above +0xb0.
 *
 * ---------------------------------------------------------------------------
 * THE SIXTEEN BLL STATES ARE THE OBJECT'S OWN WORDS
 *
 * `setBllState` is a sixteen-way jump table at `.rodata:0xbc0`, and every arm
 * ends by handing `edprintf` a string that names the state.  The enum below
 * pairs each table index with the string its arm prints, and with the
 * `V90Parameters` field pair the same arm loads -- three things agreeing, so
 * the values are read and not assigned:
 *
 *     0  FROZEN               K1 = K2 = 0
 *     1  SECOND_ORDER_FROZEN  K2 = 0 only, K1 left alone
 *     2  INITIAL              BLL_INITIAL_K1/K2            +0x088
 *     3  FAST                 BLL_FAST_K1/K2               +0x090
 *     4  MEDIUM               BLL_MEDIUM_K1/K2             +0x098
 *     5  SLOW                 BLL_SLOW_K1/K2               +0x0a0
 *     6  SLOW2                BLL_SLOW2_K1/K2              +0x0a8
 *     7  DIL                  BLL_DIL_K1/K2                +0x0b0
 *     8  TRN2_INITIAL         BLL_TRN2_INITIAL_K1/K2       +0x0b8
 *     9  TRN2                 BLL_TRN2_K1/K2               +0x0c0
 *    10  STEADY_STATE         BLL_STEADY_STATE_K1/K2       +0x0c8
 *    11  PRE_ANSPCM           BLL_PRE_ANSPCM_K1/K2         +0x0d0
 *    12  TRN1_QC_INITIAL      BLL_TRN1_QC_INITIAL_K1/K2    +0x0d8
 *    13  TRN1_QC_FAST         BLL_TRN1_QC_FAST_K1/K2       +0x0e0
 *    14  TRN1_QC_MEDIUM       BLL_TRN1_QC_MEDIUM_K1/K2     +0x0e8
 *    15  TRN1_QC_SLOW         BLL_TRN1_QC_SLOW_K1/K2       +0x0f0
 *
 * The names come from the diagnostic strings ("V90Resampler: state = SLOW2"),
 * so unlike the data members they are NOT inventions; the underscores in
 * `SECOND_ORDER_FROZEN`, `TRN2_INITIAL` and `TRN2` are, because those three
 * print with spaces.  The type name `V90BllState` is the mangling's, out of
 * `_ZN12V90Resampler11setBllStateE11V90BllStatej`.
 *
 * Data member names are invented and descriptive (finding F226).
 */

#ifndef DSPLIB_V90RESAMPLER_H
#define DSPLIB_V90RESAMPLER_H

#include "dsplib/ResamplerTiming.h"

/*
 * DECLARED, NOT INCLUDED, and that is load-bearing rather than tidy.
 *
 * This header uses `V90Parameters` only as a pointer -- one member at +0xa0
 * and two constructor parameters -- so a declaration satisfies all three.  The
 * two `params->` in the comments below are prose about what the blob reads,
 * not code.
 *
 * It used to `#include` the 342-slot header, and that forced
 * `V90Demodulator.h` into claiming `DSPLIB_V90PARAMETERS_H` for itself --
 * defining another header's include guard so its own block form of the class
 * would survive.  That works, and it makes a collision fail as "no member
 * named ..." at the field instead of as a redefinition naming the class, which
 * points an hour of debugging at the wrong file.  Removing the include removes
 * the reason for the hijack.
 */
class V90Parameters;

enum V90BllState {
	V90_BLL_FROZEN			= 0,
	V90_BLL_SECOND_ORDER_FROZEN	= 1,
	V90_BLL_INITIAL			= 2,
	V90_BLL_FAST			= 3,
	V90_BLL_MEDIUM			= 4,
	V90_BLL_SLOW			= 5,
	V90_BLL_SLOW2			= 6,
	V90_BLL_DIL			= 7,
	V90_BLL_TRN2_INITIAL		= 8,
	V90_BLL_TRN2			= 9,
	V90_BLL_STEADY_STATE		= 10,
	V90_BLL_PRE_ANSPCM		= 11,
	V90_BLL_TRN1_QC_INITIAL		= 12,
	V90_BLL_TRN1_QC_FAST		= 13,
	V90_BLL_TRN1_QC_MEDIUM		= 14,
	V90_BLL_TRN1_QC_SLOW		= 15
};

class V90Resampler : public ResamplerTiming {
public:
	V90Resampler(unsigned int phases, float ppmScale, unsigned int taps,
		     float cutoff, V90Parameters *params, float ppm,
		     unsigned int minHistory);
	V90Resampler(unsigned int phases, float ppmScale, unsigned int taps,
		     float *coeffs, V90Parameters *params, float ppm,
		     unsigned int minHistory);

	virtual ~V90Resampler();

	virtual void reset();

	void resample(const float *in, unsigned int n, float *out,
		      unsigned int &nOut);

	void setBllState(V90BllState state, unsigned int countSamples);

	float getTimingHistoryMean();
	float getTimingHistoryStd();

	/* Public for offsetof; see V90ConstellationDesigner.h. */

	/*
	 * +0x94 holds a `V90BllState` but is compared and stored as a plain
	 * 32-bit word (`cmp %esi,0x94(%ebx)`), which is what an `enum` of
	 * sixteen values compiles to.  The constructor seeds it with 1, not 0,
	 * so that its own `setBllState(V90_BLL_FROZEN, 1)` is not the
	 * early-out at the top of that function.
	 */
	V90BllState	bllState;	/* +0x94 */
	unsigned int	stateSamples;	/* +0x98 samples since the state changed */
	unsigned int	countStateSamples; /* +0x9c setBllState's 2nd argument;
					    *       `resample` adds to
					    *       stateSamples only when set */
	V90Parameters	*params;	/* +0xa0 not owned                  */
	float		*timingHistory;	/* +0xa4 owned; freed by ~V90Resampler */
	unsigned int	timingHistoryLen; /* +0xa8 == params->
					   *  TIMING_HISTORY_EVALUATION_BUFFER_LENGTH */
	unsigned int	timingHistoryIndex; /* +0xac wraps at timingHistoryLen */
	unsigned int	periodSamples;	/* +0xb0 counts up to params->
					 *       TIMING_HISTORY_EVALUATION_PERIOD */
};

#endif /* DSPLIB_V90RESAMPLER_H */
