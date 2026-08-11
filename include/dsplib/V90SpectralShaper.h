/*
 * V90SpectralShaper.h -- the V.90 spectral shaper's state.
 *
 * Reconstructed from dsplibs.o.  Eight members, 2,562 bytes of text plus a
 * 512-byte `actionLookupTable`; the constructor and the destructor are the
 * two written here.
 *
 * NOT POLYMORPHIC: `nm` lists `D1` and `D2` and no `D0`, and the constructor
 * stores no vptr, so offset 0 is a real member.
 *
 * THE OBJECT IS 108 BYTES, and the bound comes from OUTSIDE it as well as
 * from inside.  `V90Mapper` embeds one at +0x68c -- its constructor does
 * `lea 0x68c(%ebx),%eax` and calls `V90SpectralShaperC1` on it -- and the
 * next thing `V90Mapper::reset` and `V90Mapper::process` touch above that is
 * +0x6f8, which is 0x68c + 0x6c exactly.  Inside, the largest displacement
 * any of the eight members uses is the +0x48 subobject, which is 0x24 long.
 * The two agree on 0x6c.
 *
 * WHAT THE CONSTRUCTOR AND DESTRUCTOR PIN:
 *
 *     +0x20  constructed 0.  `advanceTrellis` stores 1 into it.
 *     +0x28  `sysdep_malloc(0x30)`, freed by the destructor.  UNSIGNED SHORT
 *     +0x2c  `sysdep_malloc(0x30)`, freed by the destructor.  ELEMENTS: both
 *            are indexed `movzwl (%reg,%edx,2)` by `advanceTrellis`, so the
 *            stride is two and the load zero-extends; 0x30 bytes is 24 of
 *            them, which is what +0x34 holds.
 *     +0x30  constructed 0.
 *     +0x34  constructed 24.  Stored AFTER both allocations, so the
 *            allocation size is not computed from it.
 *     +0x38  constructed 0, one byte.  It is written BEFORE the +0x3c
 *            subobject's constructor runs, which is what a member-initialiser
 *            list does and what a constructor body cannot do, so it is
 *            initialised in the list here.
 *     +0x3c  a `ParallelDifferentialEncoder<unsigned char>`, constructed with
 *            6 and destroyed by the destructor's last call.  12 bytes, which
 *            is what DiffCoder.h already pinned from this very constructor.
 *     +0x48  a `V90SpectralShapingFilter`, default-constructed.  The
 *            destructor does NOT call one for it, which is half the evidence
 *            that that class has no destructor at all.
 *
 * EVERYTHING BELOW +0x20 IS STILL `pad_`.  Neither function written here
 * touches it and the rule is that an unmodelled region stays `pad_` rather
 * than being guessed into fields; the other six members reach +0x00, +0x04,
 * +0x08, +0x0c, +0x12, +0x18 and +0x24 through registers this file did not
 * trace back to `this`, so even their existence is not asserted here.
 */

#ifndef DSPLIB_V90SPECTRALSHAPER_H
#define DSPLIB_V90SPECTRALSHAPER_H

#include "dsplib/DiffCoder.h"
#include "dsplib/V90SpectralShapingFilter.h"

class V90SpectralShaper {
public:
	/* Defined in src/pump/v90/V90SpectralShaper.cpp. */
	V90SpectralShaper();
	~V90SpectralShaper();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned char	 pad_00[0x20];	/* +0x00 not touched here           */
	unsigned int	 word_20;	/* +0x20 constructed 0              */
	unsigned char	 pad_24[4];	/* +0x24 not touched here           */
	unsigned short	*buf_28;	/* +0x28 owned, 24 entries          */
	unsigned short	*buf_2c;	/* +0x2c owned, 24 entries          */
	unsigned int	 word_30;	/* +0x30 constructed 0              */
	unsigned int	 word_34;	/* +0x34 constructed 24             */
	unsigned char	 byte_38;	/* +0x38 constructed 0, in the list */
	unsigned char	 pad_39[3];	/* +0x39 alignment                  */
	ParallelDifferentialEncoder<unsigned char> pde;	/* +0x3c, built with 6 */
	V90SpectralShapingFilter ssf;			/* +0x48              */
};

#endif /* DSPLIB_V90SPECTRALSHAPER_H */
