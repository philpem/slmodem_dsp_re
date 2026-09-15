/*
 * Smc.c -- the fax pumps' SyMbol Coder.
 *
 * Reconstructed from dsplibs.o:
 *   SMC_encoder  .text 0x09fa10   609 bytes
 *   SMC_init     .text 0x09fc80    53 bytes
 *   SMC_CFG      .rodata 0x00bb40   44 bytes
 *
 * `SMC_init` is BYTE-FOR-BYTE `FPM_SMC_init` -- all 53 bytes equal -- so it is
 * the same source in a second translation unit.  `SMC_encoder` is
 * `FPM_SMC_encoder` with one more output form; what the form is and how the
 * config fields were named is in `include/dsplib/smc.h`, and what the mapping
 * and the two wraps mean is in `include/dsplib/fpm_smc.h`.
 *
 * Three things about the arithmetic are worth having in front of you, and all
 * three are inherited from the generic encoder rather than new here:
 *
 *   - the quadrant is masked as a 32-bit value and only THEN truncated to a
 *     short for the next symbol, so the index and the stored state can
 *     disagree above bit 15;
 *
 *   - both wraps are a single conditional subtract, so a value at or above
 *     2 * rot_mod comes out unreduced and a negative `rot_mod` makes the test
 *     always true;
 *
 *   - the write index wraps on `widx + 1 < len`, tested after the increment,
 *     so an index seeded at or past `len` writes out of range once.
 *
 * What IS new is the complex form's indices: `imap`/`qmap` are addressed by
 * the symbol index and `cosine`/`sine` by the carrier accumulator as it stood
 * BEFORE this symbol's step, and neither is bounds-checked against anything.
 */

#include "dsplib/smc.h"

#include <string.h>
#include "dsplib/v17fax.h"
#include "dsplib/v32smc.h"

#define FIELD(obj, off) ((unsigned char *)(obj) + (off))
#define AT_S(p, off) (*(short *)(void *)FIELD((p), (off)))
#define AT_US(p, off) (*(unsigned short *)(void *)FIELD((p), (off)))
#define AT_SB(p, off) (*(signed char *)(void *)FIELD((p), (off)))
#define SMC_MODE(smc) AT_SB((smc), 0x00 - 0x00)
#define SMC_QUAD(smc) AT_S((smc), V17FP_SMC_SHORT_06 - V17FP_SMC)
#define SMC_STATE(smc) AT_S((smc), V17FP_SMC_SHORT_08 - V17FP_SMC)
#define SMC_TRELLIS(smc) AT_S((smc), V17FP_SMC_SHORT_0C - V17FP_SMC)
#define SMC_PREV(smc) AT_S((smc), V17FP_SMC_SHORT_0E - V17FP_SMC)
#define SMC_NBITS(smc) AT_US((smc), V17FP_SMC_SHORT_12 - V17FP_SMC)

extern const unsigned short SMCv17_PMAP4[4];
extern const unsigned short SMCv17_ABS4[4];
extern const unsigned short SMCv17_MOD[8];
extern const short SMCv17_CFG[2];


/*
 * The object's table.  Nine of its eleven dwords are zero and every caller
 * copies it and fills the rest in: V29TX_create takes `direct`, `rot_step`
 * 0x11, `rot_mod` 0x18 and the five table pointers from its own V29TX_SMC_*
 * symbols, and clears `f00` to select the complex form.
 */
const struct fpm_smc_cfg SMC_CFG = {
	1,			/* f00      */
	1,			/* direct   */
	3,			/* rot_step */
	8,			/* rot_mod  */
	0,			/* qshift   */
	2,			/* qmask    */
	0,			/* amask    */
	0,			/* pmask    */
	0,			/* pmap     */
	0,			/* imap     */
	0,			/* qmap     */
	0,			/* cosine   */
	0,			/* sine     */
	0			/* f28      */
};

void
SMC_encoder(struct fpm_smc *smc, struct fpm_smc_ring *ring,
	    const unsigned short *data, unsigned short count)
{
	const unsigned short *pmap = smc->cfg.pmap;
	const short *imap = smc->cfg.imap;
	const short *qmap = smc->cfg.qmap;
	const short *cosine = smc->cfg.cosine;
	const short *sine = smc->cfg.sine;
	const int qshift = smc->cfg.qshift;
	const int qmask = smc->cfg.qmask;
	const int amask = smc->cfg.amask;
	const int pmask = smc->cfg.pmask;
	const int rot_step = smc->cfg.rot_step;
	const int rot_mod = smc->cfg.rot_mod;
	short *rail_i = ring->i;
	short *rail_q = ring->q;
	short *sym = ring->sym;
	const int len = ring->len;
	short quad = smc->quad;
	short acc = smc->acc;
	int widx = ring->widx;
	/*
	 * Both selectors are loaded ONCE and outside the loop, so a config
	 * edited under a running encoder is not seen until the next call.
	 * `f00` picks the output form; `direct` picks the quadrant rule.
	 */
	const int mapped = smc->cfg.f00;
	const int direct = smc->cfg.direct;

	while (count--) {
		unsigned short word = *data++;
		int q;
		int index;

		if (direct) {
			/* Absolute: the quadrant is in the word as it stands. */
			q = word & qmask;
		} else {
			/*
			 * Differential: the selected bits index a table of
			 * quadrant CHANGES, which accumulate.
			 */
			q = (quad + pmap[(word >> qshift) & qmask]) & pmask;
		}
		quad = (short)q;
		index = q | (word & amask);

		if (mapped) {
			/* Carrier rotation folded into the same index. */
			int out = (unsigned short)(acc + index);

			if (out >= rot_mod)
				out = (unsigned short)(out - rot_mod);
			sym[widx] = (short)out;
		} else {
			/*
			 * Carrier rotation done as a Q15 complex multiply of
			 * the constellation point by the phasor at `acc`.
			 * `acc` here is the value BEFORE this symbol's step.
			 */
			int c = cosine[acc];
			int s = sine[acc];

			rail_i[widx] = (short)((imap[index] * c >> 15)
					       - (qmap[index] * s >> 15));
			rail_q[widx] = (short)((qmap[index] * c >> 15)
					       + (imap[index] * s >> 15));
		}

		acc = (short)(acc + rot_step);
		if (acc >= rot_mod)
			acc = (short)(acc - rot_mod);

		widx = (short)(widx + 1);
		if (widx >= len)
			widx = 0;
	}

	smc->acc = acc;
	smc->quad = quad;
	ring->widx = (short)widx;
}

void
SMC_init(struct fpm_smc *smc, const struct fpm_smc_cfg *cfg)
{
	smc->cfg = *cfg;
	smc->quad = 0;
	smc->acc = 0;
}

/* SMCv17 definitions moved from v17.c; reference order. */
/* (widx + 1) mod len, exactly as the object spells it -- see v32smc.c. */
static short
smc_ring_advance(short widx, short len)
{
	short next = (short)(widx + 1);

	return (short)((next < len) ? next : 0);
}

/*
 * SMCv17_encoder_dif -- .text 0x09fcc0, 164 bytes.
 *
 * `state = (state + PMAP4[in[i] & 3]) & 3; point = (state + quad + 1) & 3`.
 * `in[i]` is read whole and unsigned (`movzwl`) and only its low two bits are
 * ever used, so nothing separates `short` from `unsigned short` here -- the
 * type follows `v17_encoder_fn`, not a forced reading.
 */
void
SMCv17_encoder_dif(void *smc, struct fpm_smc_ring *ring,
		   const unsigned short *data, unsigned short count)
{
	short *const sym = ring->sym;
	const short len = ring->len;
	short widx = ring->widx;
	int quad = SMC_QUAD(smc);
	int state = SMC_STATE(smc);
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int sel = (unsigned int)data[i] & 3u;
		int point;

		quad = (quad + 3) & 3;
		state = (int)(state + SMCv17_PMAP4[sel]) & 3;
		point = (state + quad + 1) & 3;
		sym[widx] = (short)point;
		widx = smc_ring_advance(widx, len);
	}

	SMC_STATE(smc) = (short)state;
	SMC_QUAD(smc) = (short)quad;
	ring->widx = widx;
}

/*
 * SMCv17_encoder_abs -- .text 0x09fd70, 135 bytes.
 *
 * `point = (quad + ABS4[in[i] & 3]) & 3` -- no accumulator, the point comes
 * straight out of the table every symbol.
 */
void
SMCv17_encoder_abs(void *smc, struct fpm_smc_ring *ring,
		   const unsigned short *data, unsigned short count)
{
	short *const sym = ring->sym;
	const short len = ring->len;
	short widx = ring->widx;
	int quad = SMC_QUAD(smc);
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int sel = (unsigned int)data[i] & 3u;
		int point;

		quad = (quad + 3) & 3;
		point = (int)(quad + SMCv17_ABS4[sel]) & 3;
		sym[widx] = (short)point;
		widx = smc_ring_advance(widx, len);
	}

	SMC_QUAD(smc) = (short)quad;
	ring->widx = widx;
}

/*
 * SMCv17_encoder_tcm -- .text 0x09fe00, 374 bytes.
 *
 * The trellis coder.  Three tables and four pieces of state (`quad`,
 * `trellis`, `prev`, `nbits`), and unlike its two siblings it WRITES BACK to
 * its input buffer -- see v17data.h for why `data` cannot be `const` here.
 *
 *     in[i] &= mask_all                        -- in place, before anything
 *     trellis = TrellisEncodeDifTable[trellis + (in[i] >> nbits) * 4]
 *     word    = (trellis << nbits) + (in[i] & mask_low)
 *     if (prev > 3) word = (word + (1 << (nbits + 2))) & 0xffff
 *     quad    = (quad + 3) & 3
 *     prev    = TrellisTransitionTable[trellis + prev * 4]
 *     rot     = (SMCv17_MOD[word >> nbits] >> (quad * 4)) & 7
 *     out     = ((rot << nbits) + (word & mask_low)) | (mode << 8)
 *
 * Exactly `SMCv32_encoder_tcm`'s algorithm (`v32smc.c`), reusing that file's
 * tables; only the state's offsets differ.  `mask_low`, `mask_all` and the
 * `1 << (nbits + 2)` constant are built once, before the loop, and all three
 * are truncated to 16 bits where the object builds them.
 *
 * `prev > 3` is a SIGNED 16-bit comparison (`cmpw $0x3` / `jle`) against the
 * value from the PREVIOUS iteration -- the update below it happens later in
 * the same body, exactly as in V.32's coder.
 */
void
SMCv17_encoder_tcm(void *smc, struct fpm_smc_ring *ring,
		   unsigned short *data, unsigned short count)
{
	short *const sym = ring->sym;
	const short len = ring->len;
	const int nbits = SMC_NBITS(smc);
	const unsigned short mask_low = (unsigned short)((1 << nbits) - 1);
	const unsigned short bit_hi = (unsigned short)(1 << (nbits + 2));
	const unsigned short mask_all = (unsigned short)(bit_hi - 1);
	const int tag = SMC_MODE(smc) << 8;
	short widx = ring->widx;
	int quad = SMC_QUAD(smc);
	int trellis = SMC_TRELLIS(smc);
	int prev = SMC_PREV(smc);
	unsigned int i;

	for (i = 0; i < count; i++) {
		int word;
		int rot;
		int masked;

		/* In place, and the caller sees it. */
		data[i] = (unsigned short)(data[i] & mask_all);
		masked = data[i];

		trellis = TrellisEncodeDifTable[trellis
						+ ((masked >> nbits) * 4)];
		word = (trellis << nbits) + (masked & mask_low);
		if (prev > 3)
			word = (int)(unsigned short)(word + bit_hi);

		quad = (quad + 3) & 3;
		prev = TrellisTransitionTable[trellis + prev * 4];

		rot = (SMCv17_MOD[(unsigned short)(word >> nbits)]
		       >> (quad * 4)) & 7;
		sym[widx] = (short)(((rot << nbits) + (word & mask_low))
				    | tag);
		widx = smc_ring_advance(widx, len);
	}

	SMC_QUAD(smc) = (short)quad;
	SMC_TRELLIS(smc) = (short)trellis;
	SMC_PREV(smc) = (short)prev;
	ring->widx = widx;
}

/*
 * SMCv17_init -- .text 0x0a0a60, 87 bytes.  See v17data.h for what this
 * confirms about the five neutral fields it clears.
 */
void
SMCv17_init(void *smc, const short *cfg)
{
	if (cfg == NULL)
		cfg = SMCv17_CFG;

	memcpy(FIELD(smc, 0x00), cfg, sizeof(short[2]));
	SMC_QUAD(smc) = 0;
	SMC_STATE(smc) = 0;
	SMC_TRELLIS(smc) = 0;
	SMC_PREV(smc) = 0;
	AT_S(smc, V17FP_SMC_SHORT_10 - V17FP_SMC) = 0;
}
