/*
 * V90SpectralShapingFilter.h -- the shaping filter's state.
 *
 * Reconstructed from dsplibs.o.  Five members, 262 bytes; the constructor is
 * the one written here.
 *
 * NOT POLYMORPHIC: the class has no destructor at all -- `nm` lists no `D0`,
 * `D1` or `D2` for it, and `V90SpectralShaper`'s destructor, which does call
 * its other member subobject's, does not call one for this -- so there is no
 * vptr and offset 0 is a real member.
 *
 * THE OBJECT IS 36 BYTES, and that is pinned from OUTSIDE rather than from
 * the largest displacement inside.  `V90SpectralShaper` builds one at
 * `%ebx+0x48` and is itself 0x6c bytes long (its own bound is in
 * V90SpectralShaper.h), so this object occupies [0x48, 0x6c) of it: 0x24.
 * The largest displacement any of the five members uses, +0x20 four bytes
 * wide, agrees with that.
 *
 * WHAT THE FIVE MEMBERS SAY ABOUT THE NINE WORDS:
 *
 *     +0x00..+0x0c  `setFilterCoeff(float, float, float, float)` stores its
 *                   four arguments into these four words IN ORDER, so they
 *                   are four floats and they are the filter's coefficients.
 *     +0x10..+0x1c  `reset()` stores zero into exactly these four and nothing
 *                   else, so they are the filter's state.
 *     +0x20         the constructor's only non-zero store: 2.  It is the
 *                   number of SAMPLES `progress` consumes per call and the
 *                   number `getMetric` consumes per block, which is what
 *                   renamed it from `word_20` once those two were read.
 *
 * WHAT THE FOUR STATE WORDS ARE was decided by `progress`, which loads all
 * four before its loop and stores all four after it:
 *
 *     state[0]  the previous input sample
 *     state[1]  the previous output of the first section
 *     state[2]  the previous output of the second section
 *     state[3]  a running sum of the SQUARES of the second section's output,
 *               which neither member ever clears -- only `reset()` does
 *
 * THE RECURRENCE IS THE SAME IN BOTH `progress` AND `getMetric`, and that is
 * how the popping-form reading was checked rather than assumed (finding
 * 1400).  Per input sample x:
 *
 *     a = (x - state[0] * coeff[2]) + state[1] * coeff[0]
 *     y = (a - state[1] * coeff[3]) + state[2] * coeff[1]
 *     state[3] += y * y
 *
 * Two cascaded first-order sections -- zero at coeff[2] and pole at coeff[0],
 * then zero at coeff[3] and pole at coeff[1] -- with an energy accumulator on
 * the output.  The bracketing is the object's and is not free to move:
 * addition is commutative, association is not.
 *
 * FOUR SCALARS OR AN ARRAY OF FOUR IS NOT DECIDABLE from the object: every
 * access to both groups is at a constant displacement and compiles the same
 * way either way.  They are written as arrays here because the four in each
 * group are handled uniformly by the member that touches them; nothing in the
 * object contradicts the other reading, and nothing depends on it.
 */

#ifndef DSPLIB_V90SPECTRALSHAPINGFILTER_H
#define DSPLIB_V90SPECTRALSHAPINGFILTER_H

class V90SpectralShapingFilter {
public:
	/* All five are defined in src/pump/v90/V90SpectralShapingFilter.cpp. */
	V90SpectralShapingFilter();

	void	setFilterCoeff(float c0, float c1, float c2, float c3);
	void	reset();
	void	progress(const short *in);

	/*
	 * Returns the accumulator, and is `const`: it runs the same recurrence
	 * over `blocks * blockLength` samples in x87 registers and writes not
	 * one word of the object back.
	 */
	float	getMetric(const short *in, unsigned int blocks) const;

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	float		coeff[4];	/* +0x00 setFilterCoeff's arguments  */
	float		state[4];	/* +0x10 what reset() clears         */
	unsigned int	blockLength;	/* +0x20 samples per call; ctor's 2  */
};

#endif /* DSPLIB_V90SPECTRALSHAPINGFILTER_H */
