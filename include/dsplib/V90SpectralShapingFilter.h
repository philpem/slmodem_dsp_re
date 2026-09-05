/**
 * @file V90SpectralShapingFilter.h
 * @brief The two-section shaping filter embedded in `V90SpectralShaper`.
 *
 * Two cascaded first-order sections -- a zero at `coeff[2]` and a pole at
 * `coeff[0]`, then a zero at `coeff[3]` and a pole at `coeff[1]` -- with an
 * energy accumulator on the output. progress() runs the recurrence over one
 * block and updates the filter's state; getMetric() runs the same
 * recurrence over one or more blocks without touching the state, to score a
 * trellis candidate. Per input sample x:
 *
 *     a = (x - state[0] * coeff[2]) + state[1] * coeff[0]
 *     y = (a - state[1] * coeff[3]) + state[2] * coeff[1]
 *     state[3] += y * y
 *
 * The bracketing is the object's and is not free to move: addition is
 * commutative, association is not (finding F1400 is how the popping x87
 * forms behind this recurrence were checked rather than assumed).
 *
 * `state` holds, in order: the previous input sample, the previous output
 * of the first section, the previous output of the second section, and a
 * running sum of the squares of the second section's output -- the energy
 * accumulator, which only reset() clears.
 *
 * The class is not polymorphic (no destructor at all: `nm` lists no `D0`,
 * `D1` or `D2`, and `V90SpectralShaper`'s destructor does not call one for
 * this member), so offset 0 is a real member. The object is 36 bytes,
 * pinned from outside: `V90SpectralShaper` builds one at `%ebx+0x48` and is
 * itself 0x6c bytes, so this object occupies [0x48, 0x6c) of it.
 *
 * `coeff`/`state` are written as four-element arrays rather than four
 * scalars each; the object cannot distinguish the two, since every access
 * is at a constant displacement either way.
 */

#ifndef DSPLIB_V90SPECTRALSHAPINGFILTER_H
#define DSPLIB_V90SPECTRALSHAPINGFILTER_H

class V90SpectralShapingFilter {
public:
	/** @brief Construct a filter with zeroed coefficients and state, `blockLength` 2. */
	V90SpectralShapingFilter();

	/**
	 * @brief Set the two sections' four coefficients.
	 * @param c0,c1,c2,c3  Stored into `coeff[0..3]` in order.
	 */
	void	setFilterCoeff(float c0, float c1, float c2, float c3);

	/** @brief Zero the filter's state, including the energy accumulator. */
	void	reset();

	/**
	 * @brief Run `blockLength` samples through the filter, updating its
	 * state, including the running energy accumulator.
	 * @param in  `blockLength` input samples.
	 */
	void	progress(const short *in);

	/**
	 * @brief Score `blocks * blockLength` samples against the filter's
	 * current state, without modifying it.
	 *
	 * Runs the same recurrence as progress() in x87 registers, starting
	 * from the current `state`, and returns what the energy accumulator
	 * would reach -- writing nothing back.
	 *
	 * The return type is `long double`, not `float`, and that is a
	 * behavioural fact rather than a style choice: the object's return
	 * path does no narrowing, so the caller receives the x87 stack's full
	 * 64-bit significand. This matters because `V90SpectralShaper::
	 * advanceTrellis` compares the returned value, unrounded, against a
	 * `float` running best -- deliberately at different precisions, since
	 * rounding this return would change which trellis candidate wins an
	 * exact tie (candidates come in exactly-negating pairs with
	 * bit-identical metrics; finding F5854).
	 *
	 * @param in      `blocks * blockLength` input samples.
	 * @param blocks  Number of `blockLength`-sample blocks to score.
	 * @return The energy accumulator's value after all `blocks` blocks.
	 */
	long double getMetric(const short *in, unsigned int blocks) const;

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	float		coeff[4];	/* +0x00 setFilterCoeff's arguments  */
	float		state[4];	/* +0x10 what reset() clears         */
	unsigned int	blockLength;	/* +0x20 samples per call; ctor's 2  */
};

#endif /* DSPLIB_V90SPECTRALSHAPINGFILTER_H */
