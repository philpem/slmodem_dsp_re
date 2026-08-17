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
 * EVERYTHING BELOW +0x20 WAS `pad_`, AND `reset` NAMES THREE WORDS OF IT.
 * The old note here said the region stays padded until something traces it
 * back to `this`; `reset` does, at +0x00, +0x04, +0x08 and +0x24, and the
 * caller types the first two for us:
 *
 *     +0x00  `shaperId`, `V90MappingParams+0x624`.  `V90Mapper::reset`
 *            passes that field as argument 1 (0x3024e / 0x30257), and
 *            `tools/vparse.py` gives the name from the parameter block, so
 *            it is the author's and not ours.
 *     +0x04  `shaperSR`, `V90MappingParams+0x620`, argument 2.
 *     +0x08  `6 / shaperSR`, with 0 substituted when `shaperSR` is 0 --
 *            `mov $0x6,%eax; xor %edx,%edx; div %ecx` at 0x327d0, an
 *            UNSIGNED divide, which is the second reason the arguments are
 *            unsigned and the mangling is the first.  It is also what is
 *            stored into the embedded filter's `blockLength` at 0x32830 and
 *            what the encoder is reset with, which is `DiffCoder.h`'s
 *            "6 / spacing as the active width" seen from the other side:
 *            six samples to a V.90 frame, one independent memory per
 *            position that is still in play.  Named `blockLength` for the
 *            member it is copied into.
 *     +0x24  `shaperId` AGAIN, stored at 0x3285c from a re-read of +0x00.
 *            Which of the two is the working copy is not decidable from
 *            this function, so it keeps an offset name; `advanceTrellis`
 *            and `process` are what will settle it.
 *
 * +0x0c..+0x1f IS STILL `pad_`, and +0x12 and +0x18 are still only known to
 * be reached by the other six members through registers this file has not
 * traced.
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

	/*
	 * Set up for a connection: `V90MappingParams`' six shaper words, as
	 * `V90Mapper::reset` passes them (0x3025c).  The `unsigned` pair is
	 * the mangling's (`Ejjffff`) and the four floats go straight through
	 * to the embedded filter's coefficients.
	 */
	void reset(unsigned int shaperId, unsigned int shaperSR,
		   float a1, float a2, float b1, float b2);

	/* Re-run the filter's own two-step setup and nothing else. */
	void resetSSFilter(float a1, float a2, float b1, float b2);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int	 shaperId;	/* +0x00 V90MappingParams::shaperId */
	unsigned int	 shaperSR;	/* +0x04 V90MappingParams::shaperSR */
	unsigned int	 blockLength;	/* +0x08 6 / shaperSR, or 0         */
	unsigned char	 pad_0c[0x14];	/* +0x0c not touched here           */
	unsigned int	 word_20;	/* +0x20 constructed 0              */
	unsigned int	 word_24;	/* +0x24 reset stores shaperId again */
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
