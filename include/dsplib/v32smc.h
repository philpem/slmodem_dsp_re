/*
 * v32smc.h -- ITU-T V.32/V.32bis: the symbol-mapping coder's state.
 *
 * `SMC` is the author's prefix.  Three encoders share this object and an
 * output ring: `SMCv32_encoder_abs` (absolute phase), `SMCv32_encoder_dif`
 * (differential phase, V.32's 4.3 quadrant coding) and `SMCv32_encoder_tcm`
 * (the trellis coder).  Each turns a stream of input words into a stream of
 * constellation indices in the low four bits, with the mode in the high byte.
 *
 * The owner reconstruction also models initialization of these objects.
 * Field meanings below are bounded by their encoder uses; untouched gaps
 * remain padding rather than guessed fields.
 */

#ifndef DSPLIB_V32SMC_H
#define DSPLIB_V32SMC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The coder's TEMPLATE, and it is only four bytes long.
 *
 * `V32FP_recreate` loads `SMCv32_CFG` as one dword (7e9db), replaces its low
 * half with `fp + 0x28 != 0`, and stores the result to fp + 0x48 -- which is
 * `struct v32_smc` below, and which the same function then goes on to fill
 * field by field from +0x04 upwards.  So the template covers exactly the two
 * shorts named here and the object's four `.bss` bytes are its whole extent.
 *
 * It is a separate type rather than a `struct v32_smc` because the object
 * sizes it at four bytes; a 22-byte template read as a dword would be a claim
 * the symbol table contradicts.  Both members are zero -- it is `.bss` -- so
 * the low one is overwritten before it is ever read and only `pad02` survives
 * the copy.
 */
struct v32_smc_cfg {
	short mode;		/* +0x00 replaced on every path              */
	short pad02;		/* +0x02                                     */
};

extern struct v32_smc_cfg SMCv32_CFG;	/* .bss 0x000188, 4 bytes, GLOBAL   */

/*
 * The coder state.  `mode` is both the arm selector and the tag that ends up
 * in the high byte of every symbol; `state` is indexed BY it, so the three
 * modes carry independent differential accumulators.
 *
 * +0x0e through +0x15 are used by `SMCv32_encoder_tcm`. Their names are
 * usage-derived, not recovered original identifiers: +0x0e feeds and retains
 * TrellisEncodeDifTable's differential result; +0x10 feeds and retains the
 * TrellisTransitionTable state; +0x14 counts the low input bits that bypass
 * those encoders and are appended unchanged to the encoded symbol.
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
	short trellis_diff_state;	/* +0x0e differential table state          */
	short trellis_state;	/* +0x10 trellis transition state          */
	short pad12;		/* +0x12                                     */
	unsigned short uncoded_bits; /* +0x14 low bits bypassing trellis coding */
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
 * The absolute phase map, four entries: index by the two selected bits.
 * UNSIGNED, and that IS forced: `SMCv32_encoder_abs` is its only consumer and
 * loads it with `movzwl` into a 32-bit result that is used.  That every value
 * is positive -- so no test can see the difference -- is finding F613's point
 * and the reason the codegen evidence is worth having, not a reason to
 * discount it.
 *
 * Its differential counterpart `SMCv32_PMAP16` is NOT declared here.  The
 * object reads that one both ways from different translation units, so it
 * gets ONE declaration, `const short` in v32dec.h, and this file includes
 * that header.  Two `extern`s of one object with different types would be
 * undefined behaviour (C99 6.2.7p2, no diagnostic required), and onedef.py
 * would not catch it because it tracks types, not object declarations.
 * D308 records what we give up instead.
 */
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

/**
 * @brief V.32 symbol-mapping coder: absolute-phase encoding.
 * @param smc    The coder state.
 * @param out    The symbol ring to append constellation indices to.
 * @param in     The input words.
 * @param count  How many words.
 */
void SMCv32_encoder_abs(struct v32_smc *smc, struct v32_symout *out,
			const short *in, unsigned short count);

/**
 * @brief V.32 symbol-mapping coder: differential-phase encoding (V.32's 4.3 quadrant coding).
 * @param smc    The coder state; `quad` accumulates the running quadrant.
 * @param out    The symbol ring to append constellation indices to.
 * @param in     The input words.
 * @param count  How many words.
 */
void SMCv32_encoder_dif(struct v32_smc *smc, struct v32_symout *out,
			const short *in, unsigned short count);

/**
 * @brief V.32 symbol-mapping coder: trellis-coded modulation.
 *
 * @p in is not const: the trellis coder masks each word in place.
 *
 * @param smc    The coder state.
 * @param out    The symbol ring to append constellation indices to.
 * @param in     The input words, masked in place.
 * @param count  How many words.
 */
void SMCv32_encoder_tcm(struct v32_smc *smc, struct v32_symout *out,
			short *in, unsigned short count);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32SMC_H */
