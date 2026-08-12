/*
 * v32smc.h -- ITU-T V.32/V.32bis: the symbol-mapping coder's state.
 *
 * `SMC` is the author's prefix.  Three encoders share this object and an
 * output ring: `SMCv32_encoder_abs` (absolute phase), `SMCv32_encoder_dif`
 * (differential phase, V.32's 4.3 quadrant coding) and `SMCv32_encoder_tcm`
 * (the trellis coder).  Each turns a stream of input words into a stream of
 * constellation indices in the low four bits, with the mode in the high byte.
 *
 * NEITHER STRUCT IS COMPLETE, and both say where they stop.  Nothing that
 * constructs either is reconstructed yet, so the fields below are the ones
 * the encoders READ, at the offsets they read them from, and the gaps are
 * named padding rather than guesses.
 */

#ifndef DSPLIB_V32SMC_H
#define DSPLIB_V32SMC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The coder state.  `mode` is both the arm selector and the tag that ends up
 * in the high byte of every symbol; `state` is indexed BY it, so the three
 * modes carry independent differential accumulators.
 *
 * +0x0e through +0x15 are read by `SMCv32_encoder_tcm`, which is not
 * reconstructed here; they are named from its loads and nothing more is
 * claimed about them.
 */
struct v32_smc {
	short mode;		/* +0x00 arm selector, and the high byte tag */
	short pad02;		/* +0x02 not read by any encoder             */
	short shift;		/* +0x04 right shift applied to each input   */
	short quad;		/* +0x06 quadrant accumulator, mod 4         */
	/*
	 * +0x08 differential state, indexed BY `mode`.  THREE ELEMENTS IS A
	 * READING, not a measurement: `SMCv32_encoder_dif` indexes it with an
	 * unbounded `mode`, and what fixes the upper end is `SMCv32_encoder_tcm`
	 * reading +0x0e as a scalar -- so the array cannot reach past +0x0d.
	 * The differential test drives modes 0, 1 and 2 only, and that is the
	 * range over which the layout is established.
	 */
	short state[3];
	short f0e;		/* +0x0e tcm only                            */
	short f10;		/* +0x10 tcm only                            */
	short pad12;		/* +0x12                                     */
	unsigned short f14;	/* +0x14 tcm only; a shift count             */
};

/*
 * The symbol ring the encoders write into.  `widx` wraps at `limit`, which is
 * a count of SHORTS and not of bytes.
 */
struct v32_symout {
	unsigned char pad00[8];	/* +0x00 not read by any encoder             */
	short *buf;		/* +0x08                                     */
	short widx;		/* +0x0c write index, wraps at limit         */
	short pad0e;		/* +0x0e                                     */
	short limit;		/* +0x10 ring length, in shorts              */
};

/*
 * Phase maps, four entries each: index by the two selected bits.
 * UNSIGNED, and that is forced: both encoders load an element with `movzwl`
 * and use the 32-bit result in the addition that follows.
 */
extern const unsigned short SMCv32_PMAP16[4];
extern const unsigned short SMCv32_PMAP_ABS16[4];

/*
 * The trellis coder's tables.  `short` and `unsigned short` are taken from
 * the `movswl` / `movzwl` the encoder loads them with, and the element COUNT
 * from the symbol size; the 2-D shapes in the comments are readings of how
 * they are indexed, not something the object states.
 */
extern const short TrellisEncodeDifTable[16];		/* [4][4] */
extern const short TrellisTransitionTable[32];		/* [8][4] */
extern const unsigned short SMCv32_MOD[8];

void SMCv32_encoder_abs(struct v32_smc *smc, struct v32_symout *out,
			const short *in, unsigned short count);
void SMCv32_encoder_dif(struct v32_smc *smc, struct v32_symout *out,
			const short *in, unsigned short count);
/* `in` is NOT const: the trellis coder masks each word in place. */
void SMCv32_encoder_tcm(struct v32_smc *smc, struct v32_symout *out,
			short *in, unsigned short count);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32SMC_H */
