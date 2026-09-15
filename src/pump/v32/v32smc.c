/*
 * v32smc.c -- ITU-T V.32/V.32bis: absolute, differential and trellis encoders.
 *
 * Reconstructed from dsplibs.o:
 *   SMCv32_encoder_dif  .text   0x07f950   357
 *   SMCv32_encoder_abs  .text   0x07fac0   156
 *   SMCv32_PMAP16       .rodata 0x007daa     8
 *   SMCv32_PMAP_ABS16   .rodata 0x007da2     8
 *
 * `SMCv32_encoder_tcm` (0x7fb60, 374 bytes), below, shares this state object
 * and output ring and uses three additional fields (+0x0e, +0x10, +0x14).
 * See finding F1625 for the initial recovery and v32smc.h for their roles.
 *
 * ---------------------------------------------------------------------------
 * What both encoders do
 *
 * Each input word yields one constellation index in the low four bits, with
 * the mode in the high byte, written into a ring buffer:
 *
 *     quad  = (quad + 3) & 3                 one quadrant BACKWARDS per symbol
 *     out   = (point + quad * 4) & 0xf
 *     ring[widx] = out | (mode << 8)
 *     widx  = (widx + 1 < limit) ? widx + 1 : 0
 *
 * and they differ only in where `point` comes from.  The `+ 3` is what the
 * object writes -- `add $0x3` then `and $0x3` -- so the rotation is by minus
 * one quadrant per symbol and not by plus one.
 *
 * `abs` takes it straight from a table:      point = PMAP_ABS16[in & 3]
 * `dif` accumulates:                         state = (state + PMAP16[(in >> shift) & 3]) & 0xf
 *                                            point = state + (mode ? (in & 3) : 1)
 *
 * so `dif`'s two arms differ in one term: a literal 1 where the other takes
 * the input's low two bits. `state` is indexed by `mode`, so the modes carry
 * independent accumulators and switching mode does not disturb the others.
 * How MANY of them there are is a reading and not a measurement -- see the
 * note on the field in v32smc.h -- and the differential test establishes it
 * over modes 0, 1 and 2.
 *
 * WHAT THE ENCODING FORCES:
 *
 *  - `dif` reads its input SIGNED (`movswl`) and shifts it ARITHMETICALLY
 *    (`sar`); `abs` reads it UNSIGNED (`movzwl`). On a negative input word
 *    the two would disagree, so the widths are mirrored rather than
 *    normalised.
 *  - `mode` is loaded with `movsbl` in `abs` and `movswl` in `dif`. That one
 *    IS free: the value is only ever shifted left by 8 into a 16-bit store,
 *    so nothing above bit 7 of it can survive, and the two loads agree over
 *    every input. Finding F614's rule, and it is why this file does not
 *    contort the field's type to match one of them.
 *  - the wrap compares SIGNED (`setl` after `movswl`), so a `limit` of zero
 *    or a negative one leaves `widx` at 0 rather than running away.
 */

#include "dsplib/v32dec.h"	/* SMCv32_PMAP16, declared once, see D308 */
#include "dsplib/v32smc.h"

/*
 * The four absolute phases.  `SMCv32_PMAP16`, its differential counterpart,
 * is defined in v32dec_tables.c beside the other V.32 maps and declared in
 * v32dec.h; both are multiples of four in the low nibble, which is what makes
 * `point + quad * 4` land on a constellation index.
 *
 * UNSIGNED, and that IS settled: `SMCv32_encoder_abs` is its only consumer
 * and loads it with `movzwl` into a 32-bit result that is used.  That every
 * value is positive -- so no test can see the difference -- is finding F613's
 * point and the reason the codegen evidence is worth having, not a reason to
 * discount it.  Contrast `SMCv32_PMAP16`, where five load sites disagree three to two
 * and the majority reading wins -- see D308.
 */
const unsigned short SMCv32_PMAP_ABS16[4] = { 1, 5, 13, 9 };

/*
 * The trellis coder's three tables.  Widths are taken from the loads, not
 * from the symbol sizes: the two Trellis ones are read with `movswl` and a
 * scale of 2, so they are `short`; `SMCv32_MOD` is read with a scale of 2
 * as well, so it is EIGHT SHORTS and not the `unsigned char[16]` its bytes
 * would also fit.
 *
 * SIGNEDNESS IS A WEAKER CLAIM THAN WIDTH, and the phase maps are the worked
 * example.  `SMCv32_encoder_dif` loads `SMCv32_PMAP16` with `movzwl` and
 * `FSE_decision_AB` and `FSE_decision_4pt` load the SAME object with
 * `movswl` -- two translation units, two readings, which is what independent
 * `extern` declarations look like.  Every value in both maps is positive, so
 * the readings agree over every entry and neither is forced.  Width is
 * different: a scale of 2 against a scale of 1 changes which bytes are read,
 * and no value can hide that.
 *
 * `TrellisEncodeDifTable` is the differential quadrant encoder V.32 Figure 6
 * specifies -- a 4x4 Latin square, each row a permutation of 0..3.
 * `TrellisTransitionTable` is 8 states by 4 inputs, and every entry is even
 * in its first half and odd in its second, which is the parity the
 * convolutional encoder maintains.
 */
const short TrellisEncodeDifTable[16] = {
	0, 1, 2, 3,
	1, 0, 3, 2,
	2, 3, 1, 0,
	3, 2, 0, 1
};

const short TrellisTransitionTable[32] = {
	0, 6, 2, 4,
	2, 4, 0, 6,
	4, 2, 6, 0,
	6, 0, 4, 2,
	1, 5, 7, 3,
	3, 7, 5, 1,
	7, 3, 1, 5,
	5, 1, 3, 7
};

/*
 * Four 3-bit rotations per entry, selected by a shift of `quad * 4`.  Dumped
 * as bytes this reads "paapBSSB4%%4" and is not a string.
 */
const unsigned short SMCv32_MOD[8] = {
	0x6170, 0x7061, 0x5342, 0x4253,
	0x2534, 0x3425, 0x1706, 0x0617
};

/* (widx + 1) mod limit, exactly as the object spells it. */
static short
ring_advance(short widx, short limit)
{
	short next = (short)(widx + 1);

	return (short)((next < limit) ? next : 0);
}

void
SMCv32_encoder_dif(struct v32_smc *smc, struct v32_symout *out,
		   const short *in, unsigned short count)
{
	short *const buf = out->buf;
	const short limit = out->limit;
	const int shift = smc->shift;
	const int mode = smc->mode;
	short widx = out->widx;
	int quad = smc->quad;
	int state = smc->state[mode];
	const int tag = mode << 8;
	unsigned int i;

	for (i = 0; i < count; i++) {
		int word = in[i];		/* signed: the shift is `sar` */
		int sel = (word >> shift) & 3;
		int point;

		quad = (quad + 3) & 3;
		state = (int)(state + SMCv32_PMAP16[sel]) & 0xf;
		/*
		 * The one difference between the two arms.  Mode 0 adds a
		 * literal 1; every other mode adds the input's low two bits.
		 */
		point = state + (mode != 0 ? (word & 3) : 1);
		point = (point + quad * 4) & 0xf;
		buf[widx] = (short)(point | tag);
		widx = ring_advance(widx, limit);
	}

	smc->quad = (short)quad;
	smc->state[mode] = (short)state;
	out->widx = widx;
}

void
SMCv32_encoder_abs(struct v32_smc *smc, struct v32_symout *out,
		   const short *in, unsigned short count)
{
	short *const buf = out->buf;
	const short limit = out->limit;
	short widx = out->widx;
	int quad = smc->quad;
	/*
	 * The high-byte tag.  `smc->mode` is loaded as a signed char here in
	 * the object; only its low eight bits reach the 16-bit store, so the
	 * width of the load is not observable.
	 */
	const int tag = smc->mode << 8;
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int sel = (unsigned int)(unsigned short)in[i] & 3u;
		int point;

		quad = (quad + 3) & 3;
		point = (int)(SMCv32_PMAP_ABS16[sel] + quad * 4) & 0xf;
		buf[widx] = (short)(point | tag);
		widx = ring_advance(widx, limit);
	}

	smc->quad = (short)quad;
	out->widx = widx;
}

/*
 * The trellis coder.  Three tables and four pieces of state, and unlike the
 * other two it WRITES BACK to its input buffer.
 *
 *     in[i] &= mask_all              -- in place, before anything else
 *     trellis = TrellisEncodeDifTable[trellis + (in[i] >> nbits) * 4]
 *     word    = (trellis << nbits) + (in[i] & mask_low)
 *     if (prev > 3) word = (word + (1 << (nbits + 2))) & 0xffff
 *     quad    = (quad + 3) & 3
 *     prev    = TrellisTransitionTable[trellis + prev * 4]
 *     rot     = (SMCv32_MOD[word >> nbits] >> (quad * 4)) & 7
 *     out     = ((rot << nbits) + (word & mask_low)) | (mode << 8)
 *
 * `nbits` is `smc->uncoded_bits` and the three masks are derived from it once, before
 * the loop: `mask_low` is `(1 << nbits) - 1`, `mask_all` is
 * `(1 << (nbits + 2)) - 1`, and the constant added on the `prev > 3` arm is
 * `1 << (nbits + 2)`.  All three are truncated to 16 bits where they are
 * built, which is why they are `unsigned short` here.
 *
 * `SMCv32_MOD` IS `unsigned short[8]`, NOT `unsigned char[16]`, and the
 * consumer is what settles it: `movzwl SMCv32_MOD(,%eax,2)`.  Each entry
 * carries four 3-bit rotations, one per quadrant, selected by a shift of
 * `quad * 4`.  Dumped as bytes it reads as ASCII -- "paapBSSB4%%4" -- which
 * is a coincidence of the values and not a string.
 *
 * `prev > 3` is a SIGNED 16-bit comparison (`cmpw $0x3` then `jle`) against
 * the value from the PREVIOUS iteration: the update below it happens later in
 * the same body.
 */
void
SMCv32_encoder_tcm(struct v32_smc *smc, struct v32_symout *out, short *in,
		   unsigned short count)
{
	short *const buf = out->buf;
	const short limit = out->limit;
	const int nbits = smc->uncoded_bits;
	const unsigned short mask_low = (unsigned short)((1 << nbits) - 1);
	const unsigned short bit_hi = (unsigned short)(1 << (nbits + 2));
	const unsigned short mask_all = (unsigned short)(bit_hi - 1);
	const int tag = smc->mode << 8;
	short widx = out->widx;
	int quad = smc->quad;
	int trellis = smc->trellis_diff_state;
	int prev = smc->trellis_state;
	unsigned int i;

	for (i = 0; i < count; i++) {
		int word;
		int rot;
		int masked;

		/* In place, and the caller sees it. */
		in[i] = (short)((unsigned int)(unsigned short)in[i]
				& mask_all);
		masked = (unsigned short)in[i];

		trellis = TrellisEncodeDifTable[trellis
						+ ((masked >> nbits) * 4)];
		word = (trellis << nbits) + (masked & mask_low);
		if (prev > 3)
			word = (int)(unsigned short)(word + bit_hi);

		quad = (quad + 3) & 3;
		prev = TrellisTransitionTable[trellis + prev * 4];

		rot = (SMCv32_MOD[(unsigned short)(word >> nbits)]
		       >> (quad * 4)) & 7;
		buf[widx] = (short)(((rot << nbits) + (word & mask_low))
				    | tag);
		widx = ring_advance(widx, limit);
	}

	smc->quad = (short)quad;
	smc->trellis_diff_state = (short)trellis;
	smc->trellis_state = (short)prev;
	out->widx = widx;
}
