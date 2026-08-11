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
 *     +0x20         the constructor's only non-zero store: 2.  `progress`
 *                   and `getMetric` both read it; neither is reconstructed,
 *                   so the word gets an offset-derived name rather than an
 *                   invented purpose.
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
	/* Defined in src/pump/v90/V90SpectralShapingFilter.cpp. */
	V90SpectralShapingFilter();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	float		coeff[4];	/* +0x00 setFilterCoeff's arguments  */
	float		state[4];	/* +0x10 what reset() clears         */
	unsigned int	word_20;	/* +0x20 constructed as 2            */
};

#endif /* DSPLIB_V90SPECTRALSHAPINGFILTER_H */
